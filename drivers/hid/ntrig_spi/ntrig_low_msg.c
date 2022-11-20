/*
 * drivers/hid/ntrig_low_msg.c
 * Utilities for building and parsing of low level messages received and sent
 * to the sensor over a low-level transport (SPI or i2c)
 * Sending messages:
 * Contains utilities to build a complete data link message for sensor command,
 * given short command codes (similar to USB report wrapping)
 * Received messages:
 * Maintains a state machine in order to idenfity data link messages, and when
 * complete message is identified pass the message to an external function for
 * further processing
 *
 * Copyright (c) 2011, N-Trig
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include "ntrig_low_msg.h"
#include "ntrig-common.h"

/** states during message processing */

/* idle, looking for preamble */
#define STATE_IDLE 0
/* checking signature */
#define STATE_VERIFY_SIG 1
/* reading type */
#define STATE_TYPE 2
/* reading length */
#define STATE_LENGTH 3
/* reading flags */
#define STATE_FLAGS 4
/* reading rest of the message */
#define STATE_MSG 5

/** filter pattern, for	identifying a valid message after the preamble
 * (0xFFFFFFFF) */
static char FILTER_PATTERN[] = {0xA5, 0x5A, 0xE7, 0x7E};

/** maximum size of a packet that can be tracked by the state
 *  machine. When the state machine encountered a packet
 *  larger then this size, it assumes a corruption and goes
 *  back to idle state, to minimize packet loss  */
#define MAX_TRACKED_PACKET_SIZE 264

/** number of bytes from tracer/insight that should be dropped from packet */
#define BULK_DATA 5

/**
 * initialize the state machine structure. Called once in the
 * beginning, before starting to feed data to the state machine
 */
void init_ntrig_low_msg_sm(struct _ntrig_low_sm_info *info)
{
	info->state = STATE_IDLE;
	info->substate = 0;
	info->dataLen = info->amountProcessed = 0;
}

/**
 * set a new data packet for processing by the state machine
 */
void set_low_msg_sm_data_packet(struct _ntrig_low_sm_info *info, u8 *data,
	u16 length)
{
	info->data = data;
	info->dataLen = length;
	info->amountProcessed = 0;
	info->gotDataLinkMsg = 0;
}

/**
 * process the data in the state machine.
 * Call set_low_msg_sm_data_packet before.
 * Returns 0 if all data from packet was processed, 1 if there
 * is more data to process.
 * After each call, you need to call has_complete_low_message to
 * check if there is a ready message to process
 */
int process_low_sm_data_packet(struct _ntrig_low_sm_info *info)
{
	info->gotDataLinkMsg = 0;
	while (info->amountProcessed < info->dataLen) {
		u8 b = *info->data;
		info->data++;
		info->amountProcessed++;
		ntrig_dbg_lvl(NTRIG_DEBUG_LEVEL_ALL,
			"%s: got %d state %d substate %d\n", __func__, b,
			info->state, info->substate);
		switch (info->state) {
		case STATE_IDLE:
			/* in idle state, just count the number of successive
			 * 0xFF bytes in order to detect a valid preamble.
			 * a valid preamble has 4 or more 0xFF bytes */
			if (b == 0xFF) {
				if (info->substate < 4)
					info->substate++;
			} else if (b == FILTER_PATTERN[0]) {
				if (info->substate >= 4) {
					/* start verifying signature (filter
					 * pattern) */
					info->state = STATE_VERIFY_SIG;
					info->substate = 1;
				} else {
					/* start of filter pattern but not
					 * enough preamble bytes */
					info->substate = 0;
				}
			} else
				info->substate = 0;
			break;
		case STATE_VERIFY_SIG:
			/* verify the filter pattern, in order to know if we
			 * have a valid message following */
			if (b == FILTER_PATTERN[info->substate]) {
				info->substate++;
				if (info->substate == 4) {
					/* we have a valid filter pattern, next
					 * we can read the type field */
					info->state = STATE_TYPE;
				}
			} else {
				/* invalid filter pattern, back to idle */
				info->state = STATE_IDLE;
				info->substate = (b == 0xFF) ? 1 : 0;
			}
			break;
		case STATE_TYPE:
			/* read the type */
			info->lmsg.type = b;
			info->state = STATE_LENGTH;
			info->substate = 0;
			break;
		case STATE_LENGTH:
			/* read the length (2 bytes) */
			if (info->substate == 0) {
				/* read first byte */
				info->lmsg.length = b;
				info->substate++;
			} else {
				/* read second byte */
				info->lmsg.length = info->lmsg.length |
					(b << 8);
				if (info->lmsg.length >
					MAX_TRACKED_PACKET_SIZE) {
					/* the packet is too large, we assume it
					 * is corrupted. Go back to idle */
					ntrig_err(
						"%s: Message too large - "
						"length=%d (> max SPI packet "
						"size)\n",  __func__,
						info->lmsg.length);
					info->state = STATE_IDLE;
				} else
					info->state = STATE_FLAGS;
			}
			break;
		case STATE_FLAGS:
			/* read the flags */
			info->lmsg.flags = b;
			if (info->lmsg.length < 2) {
				/* invalid message - it must contain at least
				 * channel and function, ignore it */
				ntrig_err(
					"%s: invalid message, length (%d) is "
					"too short\n", __func__,
					info->lmsg.length);
				info->state = STATE_IDLE;
				info->substate = 0;
			}
			info->state = STATE_MSG;
			info->substate = 4; /* already read type+length+flags */
			break;
		case STATE_MSG:
			/* reading the rest of the message */
			if (info->substate >= 6)
				info->lmsg.data[info->substate - 6] = b;
			else if (info->substate == 4)
				info->lmsg.channel = b;
			else if (info->substate == 5)
				info->lmsg.function = b;
			info->substate++;
			if (info->substate >= info->lmsg.length) {
				/* we have a complete data link message */
				info->gotDataLinkMsg = 1;
				/* reset state for next iteration */
				info->state = STATE_IDLE;
				info->substate = 0;
				goto after_loop;
			}
			break;
		}
	}
after_loop:
	/* return if we have more data to process or not */
	return (info->amountProcessed < info->dataLen);
}

/**
 * return 1 if there is a ready link layer data packet for
 * analysis. You must call this function after each call to
 * process_low_sm_data_packet above
 */
int has_complete_low_message(struct _ntrig_low_sm_info *info)
{
	return info->gotDataLinkMsg;
}

/**
 * return 1 if the state machine is idle.
 */
int is_state_machine_idle(struct _ntrig_low_sm_info *info)
{
	return (info->state == STATE_IDLE);
}

/**
 * build an NCP/DFU command. The command code will determine how to build
 * the SPI header. The command data will be copied after the header
 * Buffer must be large enough to hold maximum possible raw
 * command (560 bytes for DFU)
 */
void build_ncp_dfu_cmd(u8 cmd, const char *buf, short msg_len, char *out_buf)
{
	int i;
	struct _ntrig_low_bus_msg *msg = (struct _ntrig_low_bus_msg *)out_buf;
	char *data;
	u32 sum;

	/* set the common fields: preamble, filter pattern */
	msg->preamble = LOWMSG_PREAMBLE;
	for (i = 0; i < 4; i++)
		msg->pattern[i] = FILTER_PATTERN[i];
	msg->msg.type = LOWMSG_TYPE_COMMAND;
	msg->msg.flags = 0;
	/* fill specific requests */
	switch (cmd) {
	case LOWMSG_REQUEST_DEBUG_AGENT:
		data = &msg->msg.data[0];
		/* ignore BULK_DATA prefix - should not be sent */
		msg_len -= BULK_DATA;
		buf += BULK_DATA;
		msg->msg.length = msg_len + offsetof(struct _ntrig_low_msg,
			data);
		msg->msg.channel = LOWMSG_CHANNEL_DEBUG;
		msg->msg.function = LOWMSG_FUNCTION_DEBUG_TYPE_A;
		memcpy(data, buf, msg_len);
		data += msg_len;
		/* checksum */
		for (i = 0, sum = 0; i < msg->msg.length; i++)
			sum += (u32)((unsigned char *)&msg->msg)[i];
		memcpy(data, &sum, sizeof(sum));
		break;
	case SPI_ENABLED_COMMAND:
		/* arbitrary SPI command, see h file defining this constant */
		data = &msg->msg.data[0];
		msg->msg.length = msg_len - SPI_ENABLED_COMMAND_HEADER_LENGTH +
			offsetof(struct _ntrig_low_msg, data);
		msg->msg.channel = buf[1];
		msg->msg.function = buf[2];
		memcpy(data, &buf[SPI_ENABLED_COMMAND_HEADER_LENGTH], msg_len);
		break;
	case LOWMSG_REQUEST_NCP_DFU:
		/* a standard NCP/DFU command. Copy buffer as is */
		data = &msg->msg.data[0];
		msg->msg.length = msg_len + offsetof(struct _ntrig_low_msg,
			data);
		msg->msg.channel = LOWMSG_CHANNEL_MAINT;
		msg->msg.function = LOWMSG_FUNCTION_NCP;
		memcpy(data, buf, msg_len);
		break;
	default:
		ntrig_err("%s: unknown request %d\n", __func__, cmd);
	}
}


