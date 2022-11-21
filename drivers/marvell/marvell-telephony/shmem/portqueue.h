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

#ifndef PORTQUEUE_H_
#define PORTQUEUE_H_

#include <linux/list.h>
#include <linux/skbuff.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/poll.h>

/* some queue size constant define, you may need fine-tune these carefully */
#define PORTQ_TX_MSG_NUM		16
#define PORTQ_TX_MSG_HUGE_NUM		256
#define PORTQ_RX_MSG_LO_WM 		16
#define PORTQ_RX_MSG_HI_WM  		64
#define PORTQ_RX_MSG_HUGE_LO_WM 	64
#define PORTQ_RX_MSG_HUGE_HI_WM		256

/* port status bit */
#define PORTQ_STATUS_XMIT_FULL		(1 << 0)	/* tx_q full */
#define PORTQ_STATUS_XMIT_QUEUED	(1 << 1)	/* tx_q queued */
#define PORTQ_STATUS_RECV_EMPTY		(1 << 2)	/* rx_q empty */

#define PORTQ_NUM_MAX			16

#define DUMP_PORT(x)	(1<<((x)+3))
#define DUMP_TX		(1<<0)
#define DUMP_RX		(1<<1)
#define DUMP_TX_SP (1<<2)
#define DUMP_RX_SP (1<<3)

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
};

extern bool portq_is_rx_work_running;
extern spinlock_t portq_rx_work_lock;
extern unsigned int portq_cp_port_fc;
extern spinlock_t portq_list_lock;
extern struct portq *portq_array[];
extern struct shm_rbctl *portq_rbctl;

extern int portq_init(void);
extern void portq_exit(void);

extern struct portq *portq_open(int port);
extern void portq_close(struct portq *portq);

extern int portq_xmit(struct portq *portq, struct sk_buff *skb, bool block);
extern struct sk_buff *portq_recv(struct portq *portq, bool block);
extern void portq_broadcase_msg(int proc);
extern void portq_flush_init(void);

extern void portq_schedule_tx(void);
extern void portq_schedule_rx(void);
extern void portq_schedule_sync(struct work_struct *work);
extern void portq_flush_workqueue(void);
extern void portq_set_dumpflag(int flag);
extern int portq_get_dumpflag(void);
extern unsigned int portq_poll(struct portq *portq, struct file *filp, poll_table *wait);
extern void portq_send_msg(int port, int proc);
#endif /* PORTQUEUE_H_ */
