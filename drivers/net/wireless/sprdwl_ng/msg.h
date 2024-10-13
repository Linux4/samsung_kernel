/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * Authors	:
 * Keguang Zhang <keguang.zhang@spreadtrum.com>
 * Jingxiang Li <Jingxiang.li@spreadtrum.com>
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

#ifndef __SPRDWL_MSG_H__
#define __SPRDWL_MSG_H__

#include <linux/types.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <asm/byteorder.h>

/* 0 for cmd, 1 for event, 2 for data */
enum sprdwl_head_type {
	SPRDWL_TYPE_CMD,
	SPRDWL_TYPE_EVENT,
	SPRDWL_TYPE_DATA,
};

enum sprdwl_head_rsp {
	/* cmd no rsp */
	SPRDWL_HEAD_NORSP,
	/* cmd need rsp */
	SPRDWL_HEAD_RSP,
};

/* bit[7][6][5] mode: sprdwl_mode
 * bit[4] rsp: sprdwl_head_rsp
 * bit[3] reserv
 * bit[2][1][0] type: sprdwl_head_type
 */
struct sprdwl_common_hdr {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8 type:3;
	__u8 reserv:1;
	__u8 rsp:1;
	__u8 mode:3;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8 mode:3;
	__u8 rsp:1;
	__u8 reserv:1;
	__u8 type:3;
#else
#error  "check <asm/byteorder.h> defines"
#endif
};

#define SPRDWL_HEAD_GET_TYPE(common) \
	(((struct sprdwl_common_hdr *)(common))->type)

struct sprdwl_cmd_hdr {
	struct sprdwl_common_hdr common;
	u8 cmd_id;
	/* the payload len include the size of this struct */
	__le16 plen;
	__le32 mstime;
	u8 paydata[0];
} __packed;

#define SPRDWL_GET_CMD_PAYDATA(msg) \
	    (((struct sprdwl_cmd_hdr *)((msg)->skb->data))->paydata)

struct sprdwl_data_hdr {
	struct sprdwl_common_hdr common;
	/* bit[7][6][5] type: 0 for normal data, 1 for wapi data
	 * bit[4][3][2][1][0] offset: the ETH data after this struct address
	 */
#define SPRDWL_DATA_TYPE_NORMAL		0
#define SPRDWL_DATA_TYPE_WAPI		(0x1 << 5)
#define SPRDWL_DATA_TYPE_MAX		SPRDWL_DATA_TYPE_WAPI
#define SPRDWL_GET_DATA_TYPE(info)	((info) & 0xe0)
#define SPRDWL_DATA_OFFSET_MASK		0x1f
	u8 info1;
	/* the payload len include the size of this struct */
	__le16 plen;
	/* the flow contrl shared by sta and p2p */
	u8 flow0;
	/* the sta flow contrl */
	u8 flow1;
	/* the p2p flow contrl */
	u8 flow2;
	/* flow3 0: share, 1: self */
	u8 flow3;
} __packed;

struct sprdwl_msg_list {
	struct list_head freelist;
	struct list_head busylist;
	int maxnum;
	/* freelist lock */
	spinlock_t freelock;
	/* busylist lock */
	spinlock_t busylock;
	atomic_t ref;
	/* data flow contrl */
	atomic_t flow;
};

struct sprdwl_msg_buf {
	struct list_head list;
	struct sk_buff *skb;
	/* data just tx cmd use,not include the head */
	void *data;
	void *tran_data;
	u8 type;
	u8 mode;
	u16 len;
	unsigned long timeout;
	/* marlin 2 */
	unsigned int fifo_id;
	struct sprdwl_msg_list *msglist;
};

static inline void sprdwl_fill_msg(struct sprdwl_msg_buf *msg,
				   struct sk_buff *skb, void *data, u16 len)
{
	msg->skb = skb;
	msg->tran_data = data;
	msg->len = len;
}

static inline int sprdwl_msg_ref(struct sprdwl_msg_list *msglist)
{
	return atomic_read(&msglist->ref);
}

static inline int sprdwl_msg_tx_pended(struct sprdwl_msg_list *msglist)
{
	return !list_empty(&msglist->busylist);
}

int sprdwl_msg_init(int num, struct sprdwl_msg_list *list);
void sprdwl_msg_deinit(struct sprdwl_msg_list *list);
struct sprdwl_msg_buf *sprdwl_alloc_msg_buf(struct sprdwl_msg_list *list);
void sprdwl_free_msg_buf(struct sprdwl_msg_buf *msg_buf,
			 struct sprdwl_msg_list *list);
void sprdwl_queue_msg_buf(struct sprdwl_msg_buf *msg_buf,
			  struct sprdwl_msg_list *list);
struct sprdwl_msg_buf *sprdwl_peek_msg_buf(struct sprdwl_msg_list *list);
void sprdwl_dequeue_msg_buf(struct sprdwl_msg_buf *msg_buf,
			    struct sprdwl_msg_list *list);
struct sprdwl_msg_buf *sprdwl_get_msgbuf_by_data(void *data,
						 struct sprdwl_msg_list *list);

#endif
