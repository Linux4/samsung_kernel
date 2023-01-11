/*
    Marvell PXA9XX ACIPC-MSOCKET driver for Linux
    Copyright (C) 2012 Marvell International Ltd.

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

#ifndef _DIRECT_RB_H_
#define _DIRECT_RB_H_

#include <linux/workqueue.h>

enum direct_rb_type {
	direct_rb_type_diag,
	direct_rb_type_total_cnt
};

struct direct_rbctl {
	int refcount;
	int svc_id;
	enum direct_rb_type direct_type;

	struct shm_rbctl *rbctl;
	bool is_ap_recv_empty;

	struct sk_buff_head rx_q;	/* rx packet queue */
	unsigned long rx_q_size;
	int max_rx_q_size;

	struct workqueue_struct *rx_wq; /* rx workqueue */
	struct work_struct rx_work; /* rx work */

	/*
	 * wait queue and lock
	 */
	wait_queue_head_t rb_rx_wq;
	spinlock_t rb_rx_lock;

	unsigned long stat_tx_sent;
	unsigned long stat_tx_drop;
	unsigned long stat_rx_fail;
	unsigned long stat_rx_drop;
	unsigned long stat_rx_got;
	unsigned long stat_interrupt;
	unsigned long stat_broadcast_msg;
};

extern struct direct_rbctl direct_rbctl[];

extern int direct_rb_init(void);
extern void direct_rb_exit(void);
extern struct direct_rbctl *direct_rb_open(enum direct_rb_type direct_type,
					   int svc_id);
extern void direct_rb_close(struct direct_rbctl *rbctl);
extern int direct_rb_xmit(enum direct_rb_type direct_type,
			  const char __user *buf, int len);
extern ssize_t direct_rb_recv(enum direct_rb_type direct_type, char *buf,
			      int len);
extern void direct_rb_broadcast_msg(int proc);
extern void msocket_dump_direct_rb(void);
#endif /* _DIRECT_RB_H_ */
