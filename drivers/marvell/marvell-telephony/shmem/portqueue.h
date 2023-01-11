/*
    portqueue.h Created on: Jul 29, 2010, Jinhua Huang <jhhuang@marvell.com>

    Marvell PXA9XX ACIPC-MSOCKET driver for Linux
    Copyright (C) 2010 Marvell International Ltd.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _PORTQUEUE_H_
#define _PORTQUEUE_H_

#include <linux/list.h>
#include <linux/skbuff.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/poll.h>
#include "msocket.h"

/* some queue size constant define, you may need fine-tune these carefully */
#define PORTQ_TX_MSG_NUM		16
#define PORTQ_TX_MSG_HUGE_NUM		256
#define PORTQ_RX_MSG_LO_WM		16
#define PORTQ_RX_MSG_HI_WM		64
#define PORTQ_RX_MSG_HUGE_LO_WM	64
#define PORTQ_RX_MSG_HUGE_HI_WM		256

/* port status bit */
#define PORTQ_STATUS_XMIT_FULL		(1 << 0)	/* tx_q full */
#define PORTQ_STATUS_XMIT_QUEUED	(1 << 1)	/* tx_q queued */
#define PORTQ_STATUS_RECV_EMPTY		(1 << 2)	/* rx_q empty */

#ifdef CONFIG_SSIPC_SUPPORT
#define PORTQ_CP_NUM_MAX		32
#else
#define PORTQ_CP_NUM_MAX		16
#endif
#define PORTQ_M3_NUM_MAX		5
#define PORTQ_M3_PORT_OFFSET	64
#define PORTQ_NUM_MAX			(PORTQ_M3_PORT_OFFSET + PORTQ_M3_NUM_MAX)

struct portq {
	struct list_head tx_list;	/* portq tx list */
	struct sk_buff_head tx_q, rx_q;	/* xmit & recv packet queue */
	wait_queue_head_t tx_wq, rx_wq;	/* xmit & recv wait queue */
	int port;		/* port number */
	int priority;		/* port priority */
	int tx_q_limit;		/* the maximum packets in tx queue */
	int rx_q_low_wm, rx_q_high_wm;	/* the watermark of rx queue */
	unsigned long status;	/* port status */
	int refcounts;
	spinlock_t lock;	/* exclusive lock */

	int grp_type;	/* portq group */

	/* statistics data for debug */
	unsigned long stat_tx_request;
	unsigned long stat_tx_sent;
	unsigned long stat_tx_drop;
	unsigned long stat_tx_queue_max;
	unsigned long stat_rx_indicate;
	unsigned long stat_rx_got;
	unsigned long stat_rx_queue_max;
	unsigned long stat_fc_ap_throttle_cp;
	unsigned long stat_fc_ap_unthrottle_cp;
	unsigned long stat_fc_cp_throttle_ap;
	unsigned long stat_fc_cp_unthrottle_ap;
	void (*recv_cb)(struct sk_buff *, void *);
	void *recv_arg;
};

enum portq_grp_type {
	portq_grp_cp_main,
	portq_grp_m3,

	portq_grp_cnt
};

struct portq_group {
	const char *name;
	enum portq_grp_type grp_type;

	bool is_inited;

	/* portq work queue and work */
	struct workqueue_struct *wq;
	struct work_struct tx_work;
	struct delayed_work rx_work;

	/* rb structure */
	struct shm_rbctl *rbctl;

	/* port queue list and lock */
	int port_cnt;
	int port_offset;
	struct portq **port_list;
	struct list_head tx_head;	/* tx port list header */
	spinlock_t list_lock;	/* queue lock */

	const int *priority;

	/* rx work queue state and lock */
	bool is_rx_work_running;
	spinlock_t rx_work_lock;

	/* tx work queue state and lock */
	bool is_tx_work_running;
	spinlock_t tx_work_lock;

	struct wakeup_source *ipc_ws;

	/* stat */
	int rx_workq_sched_num;
};

extern struct portq_group portq_grp[];
extern unsigned int portq_cp_port_fc;

extern int portq_init(void);
extern void portq_exit(void);

extern int portq_grp_init(struct portq_group *pgrp);
extern void portq_grp_exit(struct portq_group *pgrp);

extern struct portq *portq_open(int port);
extern struct portq *portq_open_with_cb(int port,
		void (*clbk)(struct sk_buff *, void *), void *arg);
extern void portq_close(struct portq *portq);

extern struct portq *portq_get(int port);

extern int portq_xmit(struct portq *portq, struct sk_buff *skb, bool block);
extern struct sk_buff *portq_recv(struct portq *portq, bool block);
extern void portq_broadcast_msg(enum portq_grp_type grp_type, int proc);
extern void portq_flush_init(enum portq_grp_type grp_type);

extern void portq_schedule_tx(struct portq_group *pgrp);
extern void portq_schedule_rx(struct portq_group *pgrp);
extern void portq_schedule_sync(struct portq_group *pgrp,
	struct work_struct *work);
extern void portq_flush_workqueue(enum portq_grp_type grp_type);
extern void portq_set_dumpflag(int flag);
extern int portq_get_dumpflag(void);
extern unsigned int portq_poll(struct portq *portq, struct file *filp,
			       poll_table *wait);

static inline bool __portq_is_synced(enum portq_grp_type grp_type)
{
	if (grp_type == portq_grp_cp_main)
		return cp_is_synced;
	else if (grp_type == portq_grp_m3)
		return m3_is_synced;

	return false;
}

static inline bool portq_is_synced(struct portq *portq)
{
	return __portq_is_synced(portq->grp_type);
}

static inline struct portq_group *portq_get_group(struct portq *portq)
{
	return &portq_grp[portq->grp_type];
}

extern void portq_recv_throttled(struct portq *portq);
extern void portq_recv_unthrottled(struct portq *portq);

#endif /* _PORTQUEUE_H_ */
