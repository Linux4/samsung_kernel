/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
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

#ifndef __SIPX_H
#define __SIPX_H

#include "sblock.h"
#include <linux/hrtimer.h>

#ifdef CONFIG_SIPCMEM_CACHE_EN

extern void __inval_cache_range(const void *, const void *);
extern void __dma_flush_range(const void *, const void *);

#define SIPC_DATA_TO_SKB_CACHE_INV(start, end) \
	__inval_cache_range(start, end)
#define SKB_DATA_TO_SIPC_CACHE_FLUSH(start, end) \
	__dma_flush_range(start, end)
#endif

#define SIPX_STATE_IDLE		0
#define SIPX_STATE_READY	0x7c7d7e7f

struct sipx_mgr;

struct sipx_init_data {
	char *name;
	uint8_t dst;
	uint32_t dl_pool_size;
	uint32_t dl_ack_pool_size;
	uint32_t ul_pool_size;
	uint32_t ul_ack_pool_size;
};

struct sipx_blk {
	uint32_t		addr; /*phy address*/
	uint32_t		length;
	uint16_t                index;
	uint16_t                offset;
};

struct sipx_blk_item {
	uint16_t		index; /* index in pools */

	/* bit0-bit10: valid lenth, bit11-bit15 valid offset */
	uint16_t		desc;
} __packed;



struct sipx_fifo_info {
	uint32_t		blks_addr;
	uint32_t		blk_size;
	uint32_t		fifo_size;
	uint32_t		fifo_rdptr;
	uint32_t		fifo_wrptr;
	uint32_t		fifo_addr;
};

struct sipx_pool {
	volatile struct sipx_fifo_info *fifo_info;
	struct sipx_blk_item *fifo_buf;/* virt of info->fifo_addr */
	uint32_t fifo_size;
	void *blks_buf; /* virt of info->blks_addr */
	uint32_t blk_size;

	spinlock_t lock;

	struct sipx_mgr *sipx;
};

struct sipx_ring {
	volatile struct sipx_fifo_info *fifo_info;
	struct sipx_blk_item *fifo_buf;/* virt of info->fifo_addr */
	void *blks_buf; /* virt of info->blks_addr */
	uint32_t fifo_size;
	uint32_t blk_size;
	spinlock_t lock;

	struct sipx_mgr *sipx;
};

struct sipx_channel {
	uint32_t		dst;
	uint32_t		channel;
	uint32_t		state;

	void			*smem_virt;
	uint32_t		smem_addr;
	uint32_t		smem_size;

	struct sipx_ring        *dl_ring;
	struct sipx_ring        *dl_ack_ring;
	struct sipx_ring        *ul_ring;
	struct sipx_ring        *ul_ack_ring;

	uint32_t                dl_record_flag;
	struct sblock           dl_record_blk;
	uint32_t                ul_record_flag;
	struct sblock           ul_record_blk;

	spinlock_t              lock;
	struct hrtimer          ul_timer;
	int                     ul_timer_active;
	ktime_t                 ul_timer_val;

	struct task_struct	*thread;
	void			(*handler)(int event, void *data);
	void			*data;

	struct sipx_mgr *sipx;
};



struct sipx_mgr {
	uint32_t dst;
	uint32_t state;
	int recovery;

	struct sipx_init_data *pdata;	/* platform data */

	void *smem_virt;
	void *smem_cached_virt;
	uint32_t smem_addr;
	uint32_t smem_size;
	void *dl_ack_start;
	void *ul_ack_start;

	uint32_t dl_pool_size;
	uint32_t dl_ack_pool_size;
	uint32_t ul_pool_size;
	uint32_t ul_ack_pool_size;
	uint32_t blk_size;
	uint32_t ack_blk_size;

	struct sipx_pool *dl_pool;
	struct sipx_pool *dl_ack_pool;
	struct sipx_pool *ul_pool;
	struct sipx_pool *ul_ack_pool;
	struct sipx_channel *channels[SMSG_VALID_CH_NR];
};





#endif /* !__SIPX_H */
