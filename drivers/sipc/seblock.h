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

#ifndef __SEBLOCK_H
#define __SEBLOCK_H

#include "sblock.h"

#define SEBLOCK_STATE_IDLE		0
#define SEBLOCK_STATE_READY		0x7c7d7e7f

struct seblock_main_mgr;

struct seblock_fifo_info {
        uint32_t		blks_addr;
        uint32_t		blk_size;
        uint32_t		fifo_size;
        uint32_t		fifo_rdptr;
        uint32_t		fifo_wrptr;
        uint32_t		fifo_addr;
};

struct seblock_ring {
        volatile struct seblock_fifo_info	*fifo_info;
        struct sblock_blks			*fifo_buf;/* virt of info->fifo_addr */
        uint32_t				fifo_size;
        int					*record; /* record the state of every blk */

        spinlock_t		lock;
        wait_queue_head_t	wait;

        struct seblock_main_mgr *seblock;
};

struct seblock_pool {
        volatile struct seblock_fifo_info 	*fifo_info;
        struct sblock_blks			*fifo_buf;/* virt of info->fifo_addr */
        uint32_t               			fifo_size;
        void					*blks_buf; /* virt of info->blks_addr */
        uint32_t               			blk_size;

        spinlock_t		lock;
        wait_queue_head_t	wait;

        struct seblock_main_mgr			*seblock;
};

struct seblock_channel_mgr {
        uint32_t		dst;
        uint32_t		channel;
        uint32_t		state;
        int			yell; 	/* need to notify cp ,trigger irq*/

        void			*smem_virt;
        uint32_t		smem_addr;
        uint32_t		smem_size;

        struct seblock_ring     *dl_ring;
        struct seblock_pool     *dl_pool;
        struct seblock_ring     *ul_ring;
        struct seblock_pool     *ul_pool;

        uint32_t                dl_record;
        uint32_t                ul_record;

        struct task_struct	*thread;
        void			(*handler)(int event, void *data);
        void			*data;

        struct seblock_main_mgr *seblock;
};



struct seblock_main_mgr {
        uint32_t		dst;
        uint32_t		state;
        int			recovery;

        void			*smem_virt;
        uint32_t		smem_addr;
        uint32_t		smem_size;

        uint32_t		dl_fifo_size;
        uint32_t		ul_fifo_size;
        uint32_t		dl_blk_size;
        uint32_t		ul_blk_size;

        struct seblock_pool        *dl_pub_pool;
        struct seblock_pool        *ul_pub_pool;
        struct seblock_channel_mgr *channels[SMSG_CH_NR];
};





#endif /* !__SEBLOCK_H */
