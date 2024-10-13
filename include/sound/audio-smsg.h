/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __AUDIO_SMSG_H
#define  __AUDIO_SMSG_H
#ifdef CONFIG_SPRD_MAILBOX
#include <soc/sprd/mailbox.h>
#endif

#define CHAN_STATE_UNUSED	0
#define CHAN_STATE_WAITING 	1
#define CHAN_STATE_OPENED	2
#define CHAN_STATE_FREE 	3

#define SMSG_CACHE_NR		256

/* flag for CONNECT/DISCONNECT msg type */
#define	SMSG_CONNECT_MAGIC		0xBEEE
#define	SMSG_DISCONNECT_MAGIC	0xEDDD

/* sipc processor ID definition */
enum {
	SIPC_ID_AP = 0,		/* Application Processor */
    SIPC_ID_AGDSP,		/* AGDSP Processor */
	SIPC_ID_NR,			/* total processor number */
};

/* smsg channel definition */
enum {
	SMSG_CH_VBC_CTL = 0,	/* vbc conrol channel */
	SMSG_CH_MP3_OFFLOAD,	/* MP3 offload control channel */
	SMSG_CH_DSP_ASSERT_CTL, /*audo dsp assert control*/
	SMSG_CH_NR,		/* total channel number */
};

/* smsg command definition for shaking hands during opening a channel */
enum {
	SMSG_CMD_NONE = 0,
	SMSG_CMD_CONNECT,		/* first msg to open a channel */
	SMSG_CMD_DISCONNECT,	/* last msg to close a channel */
	SMSG_CMD_NR,			/* total msg commands to shake hands */
};

/* share-mem ring buffer short message */
struct smsg {
	uint16_t		command;	/* command */
	uint16_t		channel;	/* channel index */
	uint32_t		parameter0;	/* msg parameter0 */
	uint32_t		parameter1;	/* msg parameter1 */
	uint32_t		parameter2;	/* msg parameter2 */
	uint32_t		parameter3;	/* msg parameter3 */
};

struct saudio_channel {
	/* wait queue for recv-buffer */
	wait_queue_head_t	rxwait;
	struct mutex		rxlock;

	/* cached msgs for recv */
	uint32_t		wrptr[1];
	uint32_t		rdptr[1];
	struct smsg		caches[SMSG_CACHE_NR];
};

/* saudio ring-buffer between AP/AGDSP ipc */
struct saudio_ipc {
	char			*name;
	uint8_t 		dst;
	uint8_t 		padding[3];

	/* send-buffer info */
	size_t		txbuf_addr;
	uint32_t		txbuf_size; /* must be 2^n */
	size_t		txbuf_rdptr;
	size_t		txbuf_wrptr;

	/* recv-buffer info */
	size_t		rxbuf_addr;
	uint32_t		rxbuf_size; /* must be 2^n */
	size_t		rxbuf_rdptr;
	size_t		rxbuf_wrptr;

//#ifdef 	CONFIG_SPRD_MAILBOX
	/* target core_id over mailbox */
	int 			target_id;
//#endif
/* sipc irq related */
#ifdef CONFIG_SPRD_MAILBOX_FIFO
	MBOX_FUNCALL		irq_handler;
#else
	irq_handler_t		irq_handler;
#endif
	/* sipc ctrl thread */
	struct task_struct	*thread;

	/* lock for send-buffer */
	spinlock_t		txpinlock;

	/* all fixed channels receivers */
	struct saudio_channel *channels[SMSG_CH_NR];

	/* record the runtime status of smsg channel */
	atomic_t		busy[SMSG_CH_NR];

	/* all channel states: 0 unused, 1 opened */
	uint8_t 		states[SMSG_CH_NR];

};

/**
 * saudio_ch_open -- open a channel for saudio
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @timeout: milliseconds, 0 means no wait, -1 means unlimited
 * @return: 0 on success, <0 on failue
 */
int saudio_ch_open(uint8_t dst, uint16_t channel, int timeout);

/**
 * saudio_ch_close -- close a channel for saudio
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @timeout: milliseconds, 0 means no wait, -1 means unlimited
 * @return: 0 on success, <0 on failue
 */
int saudio_ch_close(uint8_t dst, uint16_t channel, int timeout);

/**
 * saudio_send -- send smsg for saudio
 *
 * @dst: dest processor ID
 * @msg: smsg body to be sent
 * @timeout: milliseconds, 0 means no wait, -1 means unlimited
 * @return: 0 on success, <0 on failue
 */
int saudio_send(uint8_t dst, struct smsg *msg, int timeout);

/**
 * saudio_recv -- poll and recv smsg for saudio
 *
 * @dst: dest processor ID
 * @msg: smsg body to be received, channel should be filled as input
 * @timeout: milliseconds, 0 means no wait, -1 means unlimited
 * @return: 0 on success, <0 on failue
 */
int saudio_recv(uint8_t dst, struct smsg *msg, int timeout);

/* create/destroy smsg ipc between AP/CP */
int saudio_ipc_create(uint8_t dst, struct saudio_ipc *ipc);
int saudio_ipc_destroy(uint8_t dst);

/* quickly fill a smsg body */
static inline void smsg_set(struct smsg *msg, uint16_t channel, uint16_t cmd,
		uint32_t value0, uint32_t value1, uint32_t value2, int value3)
{
	msg->channel = channel;
	msg->command = cmd;
	msg->parameter0 = value0;
	msg->parameter1 = value1;
	msg->parameter2 = value2;
	msg->parameter3 = value3;
}

#endif
