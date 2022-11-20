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

#ifndef __SIPC_H
#define __SIPC_H

#include <linux/poll.h>

/* ****************************************************************** */
/* SMSG interfaces */

/* sipc processor ID definition */
enum {
	SIPC_ID_AP = 0,		/* Application Processor */
	SIPC_ID_CPT,		/* TD processor */
	SIPC_ID_CPW,		/* WCDMA processor */
	SIPC_ID_WCN,		/* Wireless Connectivity */
	SIPC_ID_NR,		/* total processor number */
};

/* share-mem ring buffer short message */
struct smsg {
	uint8_t			channel;	/* channel index */
	uint8_t			type;		/* msg type */
	uint16_t		flag;		/* msg flag */
	uint32_t		value;		/* msg value */
};

/* smsg channel definition */
enum {
	SMSG_CH_CTRL = 0,	/* some emergency control */
	SMSG_CH_COMM,		/* general communication channel */
	SMSG_CH_RPC_AP,		/* RPC server channel in AP side */
	SMSG_CH_RPC_CP,		/* RPC server channel in CP side */
	SMSG_CH_PIPE,		/* general pipe channel */
	SMSG_CH_PLOG,		/* pipe for debug log/dump */
	SMSG_CH_TTY,		/* virtual serial for telephony */
	SMSG_CH_DATA0,		/* 2G/3G wirleless data */
	SMSG_CH_DATA1,		/* 2G/3G wirleless data */
	SMSG_CH_DATA2,		/* 2G/3G wirleless data */
	SMSG_CH_VBC,		/* audio conrol channel */
	SMSG_CH_PLAYBACK, 	/* audio playback channel */
	SMSG_CH_CAPTURE,	/* audio capture channel */
	SMSG_CH_MONITOR_AUDIO,	/* audio monitor channel */
	SMSG_CH_CTRL_VOIP,	/* audio voip conrol channel */
	SMSG_CH_PLAYBACK_VOIP, 	/* audio voip playback channel */
	SMSG_CH_CAPTURE_VOIP,	/* audio voip capture channel */
	SMSG_CH_MONITOR_VOIP,	/* audio voip monitor channel */
	SMSG_CH_NR,		/* total channel number */
};

/* smsg type definition */
enum {
	SMSG_TYPE_NONE = 0,
	SMSG_TYPE_OPEN,		/* first msg to open a channel */
	SMSG_TYPE_CLOSE,	/* last msg to close a channel */
	SMSG_TYPE_DATA,		/* data, value=addr, no ack */
	SMSG_TYPE_EVENT,	/* event with value, no ack */
	SMSG_TYPE_CMD,		/* command, value=cmd */
	SMSG_TYPE_DONE,		/* return of command */
	SMSG_TYPE_SMEM_ALLOC,	/* allocate smem, flag=order */
	SMSG_TYPE_SMEM_FREE,	/* free smem, flag=order, value=addr */
	SMSG_TYPE_SMEM_DONE,	/* return of alloc/free smem */
	SMSG_TYPE_FUNC_CALL,	/* RPC func, value=addr */
	SMSG_TYPE_FUNC_RETURN,	/* return of RPC func */
	SMSG_TYPE_DIE,
	SMSG_TYPE_NR,		/* total type number */
};

/* flag for OPEN/CLOSE msg type */
#define	SMSG_OPEN_MAGIC		0xBEEE
#define	SMSG_CLOSE_MAGIC	0xEDDD

/**
 * smsg_ch_open -- open a channel for smsg
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @timeout: milliseconds, 0 means no wait, -1 means unlimited
 * @return: 0 on success, <0 on failue
 */
int smsg_ch_open(uint8_t dst, uint8_t channel, int timeout);

/**
 * smsg_ch_close -- close a channel for smsg
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @timeout: milliseconds, 0 means no wait, -1 means unlimited
 * @return: 0 on success, <0 on failue
 */
int smsg_ch_close(uint8_t dst, uint8_t channel, int timeout);

/**
 * smsg_send -- send smsg
 *
 * @dst: dest processor ID
 * @msg: smsg body to be sent
 * @timeout: milliseconds, 0 means no wait, -1 means unlimited
 * @return: 0 on success, <0 on failue
 */
int smsg_send(uint8_t dst, struct smsg *msg, int timeout);

/**
 * smsg_recv -- poll and recv smsg
 *
 * @dst: dest processor ID
 * @msg: smsg body to be received, channel should be filled as input
 * @timeout: milliseconds, 0 means no wait, -1 means unlimited
 * @return: 0 on success, <0 on failue
 */
int smsg_recv(uint8_t dst, struct smsg *msg, int timeout);

/* quickly fill a smsg body */
static inline void smsg_set(struct smsg *msg, uint8_t channel,
		uint8_t type, uint16_t flag, uint32_t value)
{
	msg->channel = channel;
	msg->type = type;
	msg->flag = flag;
	msg->value = value;
}

/* ack an open msg for modem recovery */
static inline void smsg_open_ack(uint8_t dst, uint16_t channel)
{
	struct smsg mopen;
	smsg_set(&mopen, channel, SMSG_TYPE_OPEN, SMSG_OPEN_MAGIC, 0);
	smsg_send(dst, &mopen, -1);
}

/* ack an close msg for modem recovery */
static inline void smsg_close_ack(uint8_t dst, uint16_t channel)
{
	struct smsg mclose;
	smsg_set(&mclose, channel, SMSG_TYPE_CLOSE, SMSG_CLOSE_MAGIC, 0);
	smsg_send(dst, &mclose, -1);
}
/* ****************************************************************** */
/* SMEM interfaces */

/**
 * smem_alloc -- allocate shared memory block
 *
 * @size: size to be allocated, page-aligned
 * @return: phys addr or 0 if failed
 */
uint32_t smem_alloc(uint32_t size);

/**
 * smem_free -- free shared memory block
 *
 * @addr: smem phys addr to be freed
 * @order: size to be freed
 */
void smem_free(uint32_t addr, uint32_t size);

/* ****************************************************************** */
/* SBUF Interfaces */

/**
 * sbuf_create -- create pipe ring buffers on a channel
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @txbufsize: tx buffer size
 * @rxbufsize: rx buffer size
 * @bufnum: how many buffers to be created
 * @return: 0 on success, <0 on failue
 */
int sbuf_create(uint8_t dst, uint8_t channel, uint32_t bufnum,
		uint32_t txbufsize, uint32_t rxbufsize);

/**
 * sbuf_destroy -- destroy the pipe ring buffers on a channel
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @return: 0 on success, <0 on failue
 */
void sbuf_destroy(uint8_t dst, uint8_t channel);

/**
 * sbuf_write -- write data to a sbuf
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @bufid: buffer ID
 * @buf: data to be written
 * @len: data length
 * @timeout: milliseconds, 0 means no wait, -1 means unlimited
 * @return: written bytes on success, <0 on failue
 */
int sbuf_write(uint8_t dst, uint8_t channel, uint32_t bufid,
		void *buf, uint32_t len, int timeout);

/**
 * sbuf_read -- write data to a sbuf
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @bufid: buffer ID
 * @buf: data to be written
 * @len: data length
 * @timeout: milliseconds, 0 means no wait, -1 means unlimited
 * @return: read bytes on success, <0 on failue
 */
int sbuf_read(uint8_t dst, uint8_t channel, uint32_t bufid,
		void *buf, uint32_t len, int timeout);

/**
 * sbuf_poll_wait -- poll sbuf read/write, used in spipe driver
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @bufid: buffer ID
 * @file: struct file handler
 * @wait: poll table
 * @return: POLLIN or POLLOUT
 */
int sbuf_poll_wait(uint8_t dst, uint8_t channel, uint32_t bufid,
		struct file *file, poll_table *wait);

/**
 * sbuf_status -- get sbuf status
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @return: 0 when ready, <0 when broken
 */
int sbuf_status(uint8_t dst, uint8_t channel);

#define	SBUF_NOTIFY_READ	0x01
#define	SBUF_NOTIFY_WRITE	0x02
/**
 * sbuf_register_notifier -- register a callback that's called
 * 		when a tx sbuf is available or a rx sbuf is received.
 * 		non-blocked sbuf_read can be called.
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @bufid: buf ID
 * @handler: a callback handler
 * @event: NOTIFY_READ, NOTIFY_WRITE, or both
 * @data: opaque data passed to the receiver
 * @return: 0 on success, <0 on failue
 */
int sbuf_register_notifier(uint8_t dst, uint8_t channel, uint32_t bufid,
		void (*handler)(int event, void *data), void *data);

/* ****************************************************************** */
/* SBLOCK interfaces */

/* sblock structure: addr is the uncached virtual address */
struct sblock {
	void		*addr;
	uint32_t	length;
};

/**
 * sblock_create -- create sblock manager on a channel
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @txblocknum: tx block number
 * @txblocksize: tx block size
 * @rxblocknum: rx block number
 * @rxblocksize: rx block size
 * @return: 0 on success, <0 on failue
 */
int sblock_create(uint8_t dst, uint8_t channel,
		uint32_t txblocknum, uint32_t txblocksize,
		uint32_t rxblocknum, uint32_t rxblocksize);

/**
 * sblock_destroy -- destroy sblock manager on a channel
 *
 * @dst: dest processor ID
 * @channel: channel ID
 */
void sblock_destroy(uint8_t dst, uint8_t channel);

#define	SBLOCK_NOTIFY_GET	0x01
#define	SBLOCK_NOTIFY_RECV	0x02
#define	SBLOCK_NOTIFY_STATUS	0x03
#define	SBLOCK_NOTIFY_OPEN	0x04
#define	SBLOCK_NOTIFY_CLOSE	0x05
/**
 * sblock_register_notifier -- register a callback that's called
 * 		when a tx sblock is available or a rx block is received.
 * 		non-blocked sblock_get or sblock_receive can be called.
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @handler: a callback handler
 * @event: SBLOCK_NOTIFY_GET, SBLOCK_NOTIFY_RECV, or both
 * @data: opaque data passed to the receiver
 * @return: 0 on success, <0 on failue
 */
int sblock_register_notifier(uint8_t dst, uint8_t channel,
		void (*handler)(int event, void *data), void *data);

/**
 * sblock_get  -- get a free sblock for sender
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @blk: return a gotten sblock pointer
 * @timeout: milliseconds, 0 means no wait, -1 means unlimited
 * @return: 0 on success, <0 on failue
 */
int sblock_get(uint8_t dst, uint8_t channel, struct sblock *blk, int timeout);

/**
 * sblock_send  -- send a sblock with smsg, it should be from sblock_get
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @blk: the sblock to be sent
 * @return: 0 on success, <0 on failue
 */
int sblock_send(uint8_t dst, uint8_t channel, struct sblock *blk);

/**
 * sblock_send_prepare  -- send a sblock without smsg, it should be from sblock_get
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @blk: the sblock to be sent
 * @return: 0 on success, <0 on failue
 */
int sblock_send_prepare(uint8_t dst, uint8_t channel, struct sblock *blk);

/**
 * sblock_send_finish  -- trigger an smsg to notify that sblock has been sent
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @return: 0 on success, <0 on failue
 */
int sblock_send_finish(uint8_t dst, uint8_t channel);

/**
 * sblock_receive  -- receive a sblock, it should be released after it's handled
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @blk: return a received sblock pointer
 * @timeout: milliseconds, 0 means no wait, -1 means unlimited
 * @return: 0 on success, <0 on failue
 */
int sblock_receive(uint8_t dst, uint8_t channel, struct sblock *blk, int timeout);

/**
 * sblock_release  -- release a sblock from reveiver
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @return: 0 on success, <0 on failue
 */
int sblock_release(uint8_t dst, uint8_t channel, struct sblock *blk);


/**
 * sblock_get_free_count  -- the count of free(empty) block
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @return: >=0  the count of free(empty) block
 */
int sblock_get_free_count(uint8_t dst, uint8_t channel);


/**
 * sblock_put  -- put a free sblock for sender
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @blk: sblock pointer
 * @return: void
 */
void sblock_put(uint8_t dst, uint8_t channel, struct sblock *blk);

/* ****************************************************************** */
/* TODO: SRPC interfaces */

enum {
	SPRC_ID_NONE = 0,
	SPRC_ID_XXX,
	SPRC_ID_NR,
};

struct srpc_server {
	uint32_t		id;
	/* TODO */
};

struct srpc_client {
	uint32_t		id;
	/* TODO */
};

int srpc_init(uint8_t dst);
int srpc_server_register(uint8_t dst, struct srpc_server *server);
int srpc_client_call(uint8_t dst, struct srpc_client *client);

#endif
