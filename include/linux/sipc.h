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
#ifdef CONFIG_SPRD_MAILBOX
#include <soc/sprd/mailbox.h>
#endif

/* ****************************************************************** */
/* SMSG interfaces */

/* sipc processor ID definition */
enum {
	SIPC_ID_AP = 0,		/* Application Processor */
	SIPC_ID_CPT,		/* TD processor */
	SIPC_ID_CPW,		/* WCDMA processor */
	SIPC_ID_WCN,		/* Wireless Connectivity */
	SIPC_ID_GGE, 		/* Gsm Gprs Edge processor */
	SIPC_ID_LTE, 		/* LTE processor */
	SIPC_ID_PM_SYS,		/* Power management processor */
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
	SMSG_CH_DATA3,		/* 2G/3G wirleless data */
	SMSG_CH_DATA4,		/* 2G/3G wirleless data */
	SMSG_CH_DATA5,		/* 2G/3G wirleless data */
	SMSG_CH_DIAG,		/* pipe for debug log/dump */
	SMSG_CH_PM_CTRL, 	/* power management control */
	SMSG_CH_DUAL_SIM_PLUG,	/* dual sim plug channel */
#ifdef CONFIG_ARCH_WHALE
	SMSG_CH_PMSYS_DBG,	/* PM sys debug channel */
#endif
	SMSG_CH_DATA6,		/* 2G/3G wirleless data */
	SMSG_CH_DATA7,		/* 2G/3G wirleless data */
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
	SMSG_TYPE_DFS,
	SMSG_TYPE_DFS_RSP,
	SMSG_TYPE_ASS_TRG,
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
#ifdef CONFIG_ZERO_COPY_SIPX
        uint16_t        index;
        uint16_t        offset;
#endif
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
#define	SBLOCK_NOTIFY_STATUS	0x04
#define	SBLOCK_NOTIFY_OPEN	0x08
#define	SBLOCK_NOTIFY_CLOSE	0x10
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
 * sblock_get_arrived_count  -- get the count of sblock(s) arrived at AP (sblock_send on CP)
 *                              but not received (sblock_receive on AP).
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @return: >=0  the count of blocks
 */
int sblock_get_arrived_count(uint8_t dst, uint8_t channel);



/**
 * sblock_get_free_count  -- get the count of available sblock(s) resident in sblock pool on AP.
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @return: >=0  the count of blocks
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

/**
 * seblock_create -- create seblock manager on a channel
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @txblocknum: tx block number
 * @txblocksize: tx block size
 * @txpoolsize: tx private pool size
 * @rxblocknum: rx block number
 * @rxblocksize: rx block size
 * @txpoolsize: rx private pool size
 * @return: 0 on success, <0 on failue
 */
int seblock_create(uint8_t dst, uint8_t channel,
		uint32_t txblocknum, uint32_t txblocksize,uint32_t txpoolsize,
		uint32_t rxblocknum, uint32_t rxblocksize,uint32_t rxpoolsize);

/**
 * seblock_destroy -- destroy seblock manager on a channel
 *
 * @dst: dest processor ID
 * @channel: channel ID
 */
int seblock_destroy(uint8_t dst, uint8_t channel);

/**
 * seblock_get  -- get a free sblock for sender
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @blk: return a gotten sblock pointer
 * @return: 0 on success, <0 on failue
 */
int seblock_get(uint8_t dst, uint8_t channel, struct sblock *blk);

/**
 * seblock_register_notifier -- register a callback that's called
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
int seblock_register_notifier(uint8_t dst, uint8_t channel,
		void (*handler)(int event, void *data), void *data);

/**
 * seblock_send  -- send a sblock with smsg, it should be from seblock_get
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @blk: the sblock to be sent
 * @return: 0 on success, <0 on failue
 */
int seblock_send(uint8_t dst, uint8_t channel, struct sblock *blk);

/**
 * seblock_flush  -- trigger an smsg to notify that sblock has been sent
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @return: 0 on success, <0 on failue
 */
int seblock_flush(uint8_t dst, uint8_t channel);

/**
 * seblock_receive  -- receive a sblock, it should be released after it's handled
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @blk: return a received sblock pointer
 * @return: 0 on success, <0 on failue
 */
int seblock_receive(uint8_t dst, uint8_t channel, struct sblock *blk);

/**
 * sblock_release  -- release a sblock from reveiver
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @return: 0 on success, <0 on failue
 */
int seblock_release(uint8_t dst, uint8_t channel, struct sblock *blk);

/**
 * sblock_get_arrived_count  -- get the count of sblock(s) arrived at AP (sblock_send on CP)
 *                              but not received (sblock_receive on AP).
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @return: >=0  the count of blocks
 */
int seblock_get_arrived_count(uint8_t dst, uint8_t channel);

/**
 * seblock_get_free_count  -- get the count of available sblock(s) resident in both
 * public pool and private pool on AP.
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @return: >=0  the count of blocks
 */
int seblock_get_free_count(uint8_t dst, uint8_t channel);

/**
 * seblock_put  -- put a free sblock for sender
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @blk: sblock pointer
 * @return: void
 */
int seblock_put(uint8_t dst, uint8_t channel, struct sblock *blk);

/* ****************************************************************** */

/* ****************************************************************** */

#define SIPX_ACK_BLK_LEN                (100)

/**
 * sipx_chan_create -- create a sipx channel
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @return: 0 on success, <0 on failue
 */
int sipx_chan_create(uint8_t dst, uint8_t channel);

/**
 * sipx_chan_destroy -- destroy seblock manager on a channel
 *
 * @dst: dest processor ID
 * @channel: channel ID
 */
int sipx_chan_destroy(uint8_t dst, uint8_t channel);

/**
 * sipx_get_ack_blk_len  -- get sipx ack block max length
 *
 * @dst: dest processor ID
 * @return: length
 */
uint32_t sipx_get_ack_blk_len(uint8_t dst);

/**
 * sipx_get  -- get a free sblock for sender
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @blk: return a gotten sblock pointer
 * @is_ack: if want to get block for ack packet 
 * @return: 0 on success, <0 on failue
 */
int sipx_get(uint8_t dst, uint8_t channel, struct sblock *blk, int is_ack);

/**
 * sipx_chan_register_notifier -- register a callback that's called
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
int sipx_chan_register_notifier(uint8_t dst, uint8_t channel,
		void (*handler)(int event, void *data), void *data);

/**
 * sipx_send  -- send a sblock with smsg, it should be from seblock_get
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @blk: the sblock to be sent
 * @return: 0 on success, <0 on failue
 */
int sipx_send(uint8_t dst, uint8_t channel, struct sblock *blk);

/**
 * sipx_flush  -- trigger an smsg to notify that sblock has been sent
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @return: 0 on success, <0 on failue
 */
int sipx_flush(uint8_t dst, uint8_t channel);

/**
 * sipx_receive  -- receive a sblock, it should be released after it's handled
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @blk: return a received sblock pointer
 * @return: 0 on success, <0 on failue
 */
int sipx_receive(uint8_t dst, uint8_t channel, struct sblock *blk);

/**
 * sipx_release  -- release a sblock from reveiver
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @return: 0 on success, <0 on failue
 */
int sipx_release(uint8_t dst, uint8_t channel, struct sblock *blk);

/**
 * sipx_get_arrived_count  -- get the count of sblock(s) arrived at AP (sblock_send on CP)
 *                              but not received (sblock_receive on AP).
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @return: >=0  the count of blocks
 */
int sipx_get_arrived_count(uint8_t dst, uint8_t channel);

/**
 * sipx_get_free_count  -- get the count of available sblock(s) resident in 
 * normal pool on AP.
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @return: >=0  the count of blocks
 */
int sipx_get_free_count(uint8_t dst, uint8_t channel);

/**
 * sipx_put  -- put a free sblock for sender
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @blk: sblock pointer
 * @return: void
 */
int sipx_put(uint8_t dst, uint8_t channel, struct sblock *blk);

/* ****************************************************************** */

#ifdef CONFIG_ZERO_COPY_SIPX

#define SBLOCK_CREATE(dst, channel,\
                txblocknum, txblocksize,txpoolsize, \
                rxblocknum, rxblocksize,rxpoolsize)  \
        sipx_chan_create(dst, channel)


#define SBLOCK_DESTROY(dst, channel) \
        sipx_chan_destroy(dst, channel)

#define SBLOCK_GET(dst, channel, blk, ack, timeout) \
        sipx_get(dst, channel, blk, ack)

#define SBLOCK_REGISTER_NOTIFIER(dst, channel, handler, data) \
        sipx_chan_register_notifier(dst, channel, handler, data)

#define SBLOCK_SEND(dst, channel, blk) \
        sipx_send(dst, channel, blk)

#define SBLOCK_SEND_PREPARE(dst, channel, blk) \
        sipx_send(dst, channel, blk)

#define SBLOCK_SEND_FINISH(dst, channel)\
        sipx_flush(dst, channel)

#define SBLOCK_RECEIVE(dst, channel, blk, timeout) \
        sipx_receive(dst, channel, blk)

#define SBLOCK_RELEASE(dst, channel, blk) \
        sipx_release(dst, channel, blk)

#define SBLOCK_GET_ARRIVED_COUNT(dst, channel) \
        sipx_get_arrived_count(dst, channel)

#define SBLOCK_GET_FREE_COUNT(dst, channel) \
        sipx_get_free_count(dst, channel)

#define SBLOCK_PUT(dst, channel, blk) \
        sipx_put(dst, channel, blk)


#else /* CONFIG_ZERO_COPY_SIPX */

#ifdef CONFIG_SBLOCK_SHARE_BLOCKS

#define SBLOCK_CREATE(dst, channel,\
                txblocknum, txblocksize,txpoolsize, \
                rxblocknum, rxblocksize,rxpoolsize)  \
        seblock_create(dst, channel,\
		txblocknum, txblocksize,txpoolsize,\
		rxblocknum, rxblocksize,rxpoolsize)


#define SBLOCK_DESTROY(dst, channel) \
        seblock_destroy(dst, channel)

#define SBLOCK_GET(dst, channel, blk, ack, timeout) \
        seblock_get(dst, channel, blk)

#define SBLOCK_REGISTER_NOTIFIER(dst, channel, handler, data) \
        seblock_register_notifier(dst, channel, handler, data)

#define SBLOCK_SEND(dst, channel, blk) \
        seblock_send(dst, channel, blk)

#define SBLOCK_SEND_PREPARE(dst, channel, blk) \
        seblock_send(dst, channel, blk)

#define SBLOCK_SEND_FINISH(dst, channel)\
        seblock_flush(dst, channel)

#define SBLOCK_RECEIVE(dst, channel, blk, timeout) \
        seblock_receive(dst, channel, blk)

#define SBLOCK_RELEASE(dst, channel, blk) \
        seblock_release(dst, channel, blk)

#define SBLOCK_GET_ARRIVED_COUNT(dst, channel) \
        seblock_get_arrived_count(dst, channel)

#define SBLOCK_GET_FREE_COUNT(dst, channel) \
        seblock_get_free_count(dst, channel)

#define SBLOCK_PUT(dst, channel, blk) \
        seblock_put(dst, channel, blk)

#else /* CONFIG_SBLOCK_SHARE_BLOCKS */

#define SBLOCK_CREATE(dst, channel,\
                txblocknum, txblocksize,txpoolsize, \
                rxblocknum, rxblocksize,rxpoolsize)  \
        sblock_create(dst, channel,\
		txblocknum, txblocksize,\
		rxblocknum, rxblocksize)

#define SBLOCK_DESTROY(dst, channel) \
        sblock_destroy(dst, channel)

#define SBLOCK_GET(dst, channel, blk, ack, timeout) \
        sblock_get(dst, channel, blk, timeout)

#define SBLOCK_REGISTER_NOTIFIER(dst, channel, handler, data) \
                sblock_register_notifier(dst, channel, handler, data)

#define SBLOCK_SEND(dst, channel, blk) \
        sblock_send(dst, channel, blk)

#define SBLOCK_SEND_PREPARE(dst, channel, blk) \
        sblock_send_prepare(dst, channel, blk)

#define SBLOCK_SEND_FINISH(dst, channel)\
        sblock_send_finish(dst, channel)

#define SBLOCK_RECEIVE(dst, channel, blk, timeout) \
        sblock_receive(dst, channel, blk, timeout)

#define SBLOCK_RELEASE(dst, channel, blk) \
        sblock_release(dst, channel, blk)

#define SBLOCK_GET_ARRIVED_COUNT(dst, channel) \
        sblock_get_arrived_count(dst, channel)

#define SBLOCK_GET_FREE_COUNT(dst, channel) \
        sblock_get_free_count(dst, channel)

#define SBLOCK_PUT(dst, channel, blk) \
        sblock_put(dst, channel, blk)

#endif /* CONFIG_SBLOCK_SHARE_BLOCKS */

#endif /* CONFIG_ZERO_COPY_SIPX */

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
int sctrl_send_sync(uint32_t type, uint32_t target_id, uint32_t value);
#ifdef CONFIG_64BIT
static inline unsigned long unalign_copy_to_user(void __user *to, const void *from, unsigned long n)
{
	unsigned long rval = 0;

	while (((unsigned long)from & 7) && n) {
		rval |= copy_to_user(to++, from++, 1);
		n--;
	}
	rval = copy_to_user(to, from, n);

	return rval;
}
static inline unsigned long unalign_copy_from_user(void *to, const void __user *from, unsigned long n)
{
	unsigned long rval = 0;

	while (((unsigned long)to & 7) && n) {
		rval |= copy_from_user(to++, from++, 1);
		n--;
	}
	rval = copy_from_user(to, from, n);

	return rval;
}
static inline void *unalign_memcpy(void *to, const void *from, size_t n)
{
	if (((unsigned long)to & 7) == ((unsigned long)from & 7)) {
		while (((unsigned long)from & 7) && n) {
			*(char *)(to++) = *(char*)(from++);
			n--;
		}
		memcpy(to, from, n);
	} else if (((unsigned long)to & 3) == ((unsigned long)from & 3)) {
                while (((unsigned long)from & 3) && n) {
			*(char *)(to++) = *(char*)(from++);
			n--;
		}
                while (n >= 4) {
			*(uint32_t *)(to) = *(uint32_t *)(from);
                        to += 4;
                        from += 4;
			n -= 4;
		}
		while (n) {
			*(char *)(to++) = *(char*)(from++);
			n--;
		}
	} else {
		while (n) {
			*(char *)(to++) = *(char*)(from++);
			n--;
		}
	}
}
#else
static inline unsigned long unalign_copy_to_user(void __user *to, const void *from, unsigned long n)
{
	return copy_to_user(to, from, n);
}
static inline unsigned long unalign_copy_from_user(void *to, const void __user *from, unsigned long n)
{
	return copy_from_user(to, from, n);
}
static inline void *unalign_memcpy(void *to, const void *from, size_t n)
{
	return memcpy(to, from, n);
}
#endif


#endif
