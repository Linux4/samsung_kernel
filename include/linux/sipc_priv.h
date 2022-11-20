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

#ifndef __SIPC_PRIV_H
#define __SIPC_PRIV_H

#define SMSG_CACHE_NR		256

struct smsg_channel {
	/* wait queue for recv-buffer */
	wait_queue_head_t	rxwait;
	struct mutex		rxlock;

	/* cached msgs for recv */
	uint32_t		wrptr[1];
	uint32_t		rdptr[1];
	struct smsg		caches[SMSG_CACHE_NR];
};

/* smsg ring-buffer between AP/CP ipc */
struct smsg_ipc {
	char			*name;
	uint8_t			dst;
	uint8_t			padding[3];

	/* send-buffer info */
	uint32_t		txbuf_addr;
	uint32_t		txbuf_size;	/* must be 2^n */
	uint32_t		txbuf_rdptr;
	uint32_t		txbuf_wrptr;

	/* recv-buffer info */
	uint32_t		rxbuf_addr;
	uint32_t		rxbuf_size;	/* must be 2^n */
	uint32_t		rxbuf_rdptr;
	uint32_t		rxbuf_wrptr;

	/* sipc irq related */
	int			irq;
	irq_handler_t		irq_handler;
	irq_handler_t		irq_threadfn;

	uint32_t 		(*rxirq_status)(void);
	void			(*rxirq_clear)(void);
	void			(*txirq_trigger)(void);

	/* sipc ctrl thread */
	struct task_struct	*thread;

	/* lock for send-buffer */
	spinlock_t		txpinlock;

	/* all fixed channels receivers */
	struct smsg_channel	*channels[SMSG_CH_NR];

	/* record the runtime status of smsg channel */
	atomic_t 		busy[SMSG_CH_NR];

	/* all channel states: 0 unused, 1 opened */
	uint8_t			states[SMSG_CH_NR];
};

#define CHAN_STATE_UNUSED	0
#define CHAN_STATE_WAITING 	1
#define CHAN_STATE_OPENED	2
#define CHAN_STATE_FREE 	3

/* create/destroy smsg ipc between AP/CP */
int smsg_ipc_create(uint8_t dst, struct smsg_ipc *ipc);
int smsg_ipc_destroy(uint8_t dst);
int  smsg_suspend_init(void);

/* initialize smem pool for AP/CP */
int smem_init(uint32_t addr, uint32_t size);

#endif
