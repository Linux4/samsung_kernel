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

#ifdef CONFIG_SPRD_MAILBOX
#include <soc/sprd/mailbox.h>
#endif

#ifdef CONFIG_SIPC_V2
struct sipc_child_node_info {
	uint8_t dst;
	uint8_t is_new;
	char * name;

	uint32_t cp_base;
	uint32_t cp_size;

	uint32_t ring_base;
	uint32_t ring_size;

	uint32_t smem_base;
	uint32_t smem_size;

#ifdef CONFIG_SPRD_MAILBOX
	uint32_t core_id;
#else
	uint32_t ap2cp_int_ctrl;
	uint32_t cp2ap_int_ctrl;
	uint32_t ap2cp_bit_trig;
	uint32_t ap2cp_bit_clr;

	uint32_t irq;
#endif
};

struct sipc_init_data {
	int is_alloc;
	uint32_t chd_nr;
	uint32_t newchd_nr;
	uint32_t smem_base;
	uint32_t smem_size;
	char *name;
	struct sipc_child_node_info info_table[0];
};

struct sipc_device {
	int status;
	uint32_t inst_nr;
	struct sipc_init_data *pdata;
	struct smsg_ipc *smsg_inst;
};

/*1-10 supported*/
#define SIPC_DEV_NR			4

struct sipc_core {
	uint32_t sipc_tag_ids;
	struct sipc_child_node_info *sipc_tags[SIPC_ID_NR];
	struct sipc_device *sipc_dev[SIPC_DEV_NR];
};

extern struct sipc_core sipc_ap;
extern struct smsg_ipc *smsg_ipcs[];
#endif

#define SMSG_CACHE_NR		256

struct smsg_channel {
	/* wait queue for recv-buffer */
	wait_queue_head_t	rxwait;
	struct mutex		rxlock;

	/* cached msgs for recv */
	uintptr_t 		wrptr[1];
	uintptr_t 		rdptr[1];
	struct smsg		caches[SMSG_CACHE_NR];
};

/* smsg ring-buffer between AP/CP ipc */
struct smsg_ipc {
	char			*name;
	uint8_t			dst;
	uint8_t			id; // add id

	uint8_t			padding[2];

	/* send-buffer info */
	uintptr_t			txbuf_addr;
	uint32_t			txbuf_size;	/* must be 2^n */
	uintptr_t			txbuf_rdptr;
	uintptr_t			txbuf_wrptr;

	/* recv-buffer info */
	uintptr_t			rxbuf_addr;
	uint32_t			rxbuf_size;	/* must be 2^n */
	uintptr_t			rxbuf_rdptr;
	uintptr_t			rxbuf_wrptr;

#ifdef 	CONFIG_SPRD_MAILBOX
	/* target core_id over mailbox */
	int 			core_id;
#endif

	/* sipc irq related */
	int			irq;
#ifdef CONFIG_SPRD_MAILBOX_FIFO
	MBOX_FUNCALL		irq_handler;
#else
	irq_handler_t		irq_handler;
#endif
	irq_handler_t		irq_threadfn;

	uint32_t 		(*rxirq_status)(uint8_t id);
	void			(*rxirq_clear)(uint8_t id);

#ifdef CONFIG_SPRD_MAILBOX_FIFO
	void			(*txirq_trigger)(uint8_t id, u64 msg);
#else
	void			(*txirq_trigger)(uint8_t id);
#endif

	/* sipc ctrl thread */
	struct task_struct	*thread;

	/* lock for send-buffer */
	spinlock_t		txpinlock;

#ifdef CONFIG_SIPC_V2
	/* all fixed channels receivers */
	struct smsg_channel	*channels[SMSG_VALID_CH_NR];

	/* record the runtime status of smsg channel */
	atomic_t		busy[SMSG_VALID_CH_NR];

	/* all channel states: 0 unused, 1 be opened by other core, 2 opend */
	uint8_t			states[SMSG_VALID_CH_NR];
#else
	/* all fixed channels receivers */
	struct smsg_channel	*channels[SMSG_CH_NR];

	/* record the runtime status of smsg channel */
	atomic_t 		busy[SMSG_CH_NR];

	/* all channel states: 0 unused, 1 opened */
	uint8_t			states[SMSG_CH_NR];
#endif
};

#define CHAN_STATE_UNUSED	0
#define CHAN_STATE_WAITING 	1
#define CHAN_STATE_OPENED	2
#define CHAN_STATE_FREE 	3

/* create/destroy smsg ipc between AP/CP */
int smsg_ipc_create(uint8_t dst, struct smsg_ipc *ipc);
int smsg_ipc_destroy(uint8_t dst);
int  smsg_suspend_init(void);

#ifdef CONFIG_SIPC_V2
/*smem alloc size align*/
#define SMEM_ALIGN_POOLSZ (0x40000)		/*256KB*/
#define SMEM_ALCSZ_ALIGN(memsz,size) (((memsz)+((size)-1))&(~((size)-1)))
#ifdef CONFIG_64BIT
#define SMEM_ALIGN_BYTES (8)
#define SMEM_MIN_ORDER (3)
#else
#define SMEM_ALIGN_BYTES (4)
#define SMEM_MIN_ORDER (2)
#endif
#endif

/* initialize smem pool for AP/CP */
#ifdef CONFIG_SIPC_V2
int smem_init_phead(uint32_t addr);
#endif

int smem_init(uint32_t addr, uint32_t size);

#endif
