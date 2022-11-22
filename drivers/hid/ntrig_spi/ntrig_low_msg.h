#ifndef _ntrig_low_msg_h
#define _ntrig_low_msg_h
/*
 * drivers/hid/ntrig_low_msg.h
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

/** the maximum length of a data link packet, not including
 * the data link header and 2 bytes from message (channel
 * and function) */
#define MAX_DATA_SIZE 256

/** the 4 byte preamble before filter pattern of each bus
 * message (sent or received)
 **/
#define LOWMSG_PREAMBLE 0xFFFFFFFF
#define LOWMSG_PREAMBLE_BYTE 0xFF

/** incoming message types */
#define LOWMSG_TYPE_SPONTANEOUS_REPORT 0x01
#define LOWMSG_TYPE_RESPONSE 0x02
#define LOWMSG_TYPE_DEBUG 0x10
#define LOWMSG_TYPE_MISC 0x20
#define LOWMSG_TYPE_PROTOCOL 0xF0

/** outgoing message types */
#define LOWMSG_TYPE_COMMAND 0x02

/** message channels, device to host */
#define LOWMSG_CHANNEL_MULTITOUCH 0x01
#define LOWMSG_CHANNEL_PEN 0x03
#define LOWMSG_CHANNEL_MULTITOUCH_TRACKED 0x04
#define LOWMSG_CHANNEL_MAINT_REPLY 0x20
#define LOWMSG_CHANNEL_DEBUG_REPLY 0x30

/** message channels, host to device */
#define LOWMSG_CHANNEL_MAINT 0x02
#define LOWMSG_CHANNEL_DEBUG 0x30

/** message functions TODO complete list */
#define LOWMSG_FUNCTION_PEN_REPORT 0x01
#define LOWMSG_FUNCTION_ST_REPORT 0x02
#define LOWMSG_FUNCTION_MT_REPORT 0x03
#define LOWMSG_FUNCTION_GET_FW_VERSION 0x0c
#define LOWMSG_FUNCTION_GET_MODE 0x0d
#define LOWMSG_FUNCTION_GET_FEATURE_GET_PEN_BITS 0x15
#define LOWMSG_FUNCTION_GO_TO_BOOTLOADER 0x51
#define LOWMSG_FUNCTION_NCP 0x61
#define LOWMSG_FUNCTION_DEBUG_TYPE_A 0xe0
#define LOWMSG_FUNCTION_DEBUG_TYPE_B 0xe1
#define LOWMSG_FUNCTION_DEBUG_TYPE_C 0xe2
#define LOWMSG_FUNCTION_DEBUG_TYPE_D 0xe3
#define LOWMSG_FUNCTION_DEBUG_TYPE_E 0xe4
#define LOWMSG_FUNCTION_DEBUG_TYPE_F 0xe5
#define LOWMSG_FUNCTION_DEBUG_TYPE_G 0xe6
#define LOWMSG_FUNCTION_DEBUG_TYPE_H 0xe7
#define LOWMSG_FUNCTION_DEBUG_TYPE_I 0xe8
#define LOWMSG_FUNCTION_DEBUG_TYPE_J 0xe9

/** request code for generic NCP/DFU command */
#define LOWMSG_REQUEST_NCP_DFU 0x7e
/** Tracer request codes */
#define LOWMSG_REQUEST_DEBUG_AGENT 0x0f

/** structure for a	data link message */
struct _ntrig_low_msg {
	u8 type;
	u16 length;
	u8 flags;
	u8 channel;
	u8 function;
	u8 data[MAX_DATA_SIZE];
} __packed;

/**
 * raw message received over the SPI bus (or i2c in the future?)
 * includes a preamble, signature, followed by the message
 * itself
 */
struct _ntrig_low_bus_msg {
	u32 preamble;
	u8 pattern[4];
	struct _ntrig_low_msg msg;
} __packed;


/** describes a	single finger in a multi-touch report */
struct _ntrig_low_mt_finger {
	u8 flags; /* tip switch, in range, touch valid */
	u16 fingerIndex;
	u16 x;
	u16 y;
	u16 dx;
	u16 dy;
	u32 vendorDefined;
} __packed;

/** the maximum number of fingers sent in a multi-touch
 * report from G3.x sensor */
#define MAX_MT_FINGERS_G3 6

/** the maximum number of fingers sent in multi-touch report
 * from G4 sensor */
#define MAX_MT_FINGERS_G4 10

/** sensor multi-touch report received over SPI: fingers and
 *  contact count from G3 sensor (6 fingers) */
struct _ntrig_mt_report_fingers_g3 {
	struct _ntrig_low_mt_finger fingers[MAX_MT_FINGERS_G3];
	u8 contactCount; /* number of fingers */
} __packed;

/** sensor multi-touch report received over SPI: fingers and
 *  contact count from G4 sensor (10 fingers) */
struct _ntrig_mt_report_fingers_g4 {
	struct _ntrig_low_mt_finger fingers[MAX_MT_FINGERS_G4];
	u8 contactCount; /* number of fingers */
} __packed;

/** describes a	multi-touch	report received	over spi */
struct _ntrig_low_mt_report {
	u8 reportId;
	u16 reportCount;
	union {
		struct _ntrig_mt_report_fingers_g3 g3fingers;
		struct _ntrig_mt_report_fingers_g4 g4fingers;
	};
} __packed;

/** describes a	pen	report received	over spi */
struct _ntrig_low_pen_report {
	u8 reportId;
	/* bit 0: in range 1: tip switch 2: barrel (right click) 3: invert
	 * 4: eraser 5-7: unused */
	u8 flags;
	u16 x;
	u16 y;
	u16 pressure;
	u8 battery_status;
} __packed;

/** structure describing the data link message over	spi,
 *  starting from the "channel"	field */
struct _ntrig_low_msg_data {
	u8 channel;
	u8 function;
	union {
		struct _ntrig_low_mt_report mt;
		struct _ntrig_low_pen_report pen;
	};
} __packed;

/** structure with internal	state machine information */
struct _ntrig_low_sm_info {
	/** current state */
	u8 state;
	/** internal state-specific information */
	u16 substate;
	/** message - filled during message processing */
	struct _ntrig_low_msg lmsg;
	/** pointer to data being processed */
	u8 *data;
	/** length of the data being processed */
	u16 dataLen;
	/** amount of data already processed */
	u16 amountProcessed;
	/** true if we have a ready data link message */
	u8 gotDataLinkMsg;
};

/**
 * initialize the state machine structure. Called once in the
 * beginning, before starting to feed data to the state machine
 */
void init_ntrig_low_msg_sm(struct _ntrig_low_sm_info *info);

/**
 * set a new data packet for processing by the state machine
 */
void set_low_msg_sm_data_packet(struct _ntrig_low_sm_info *info, u8 *data,
	u16 length);

/**
 * process the data in the state machine.
 * Call set_low_msg_sm_data_packet before.
 * Returns 0 if all data from packet was processed, 1 if there
 * is more data to process.
 * After each call, you need to call has_complete_low_message to
 * check if there is a ready message to process
 */
int process_low_sm_data_packet(struct _ntrig_low_sm_info *info);

/**
 * return 1 if there is a ready link layer data packet for
 * analysis. You must call this function after each call to
 * process_low_sm_data_packet above
 */
int has_complete_low_message(struct _ntrig_low_sm_info *info);

/**
 * return 1 if the state machine is idle.
 */
int is_state_machine_idle(struct _ntrig_low_sm_info *info);

/**
 * build an NCP/DFU command. The command code will determine how to build
 * the SPI header. The command data will be copied after the header
 * Buffer must be large enough to hold maximum possible raw
 * command (560 bytes for DFU)
 */
void build_ncp_dfu_cmd(u8 cmd, const char *buf, short msg_len, char *out_buf);

#endif /* _ntrig_low_msg_h */
