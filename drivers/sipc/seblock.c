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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/log2.h>

#include <linux/sipc.h>
#include "sblock.h"
#include "seblock.h"

/* debugging macros */
#define SEBLK_INFO(x...)		pr_info("SEBLOCK: " x)
#define SEBLK_DEBUG(x...)	pr_debug("SEBLOCK: " x)
#define SEBLK_ERR(x...)		pr_err("SEBLOCK: [E] " x)


static struct seblock_main_mgr *seblocks[SIPC_ID_NR];


int destroy_seblock_main_ctrl(struct seblock_main_mgr *seblock)
{
        if(seblock->dl_pub_pool) {
                kfree(seblock->dl_pub_pool);
        }
        if(seblock->ul_pub_pool) {
                kfree(seblock->ul_pub_pool);
        }
        if(seblock->smem_virt) {
                iounmap(seblock->smem_virt);
        }
        if(seblock->smem_addr) {
                smem_free(seblock->smem_addr, seblock->smem_size);
        }

        kfree(seblock);

        return 0;
}

int destroy_seblock_channel_ctrl(struct seblock_channel_mgr *se_chanl)
{
        if(se_chanl->dl_ring) {
                kfree(se_chanl->dl_ring);
        }
        if(se_chanl->dl_pool) {
                kfree(se_chanl->dl_pool);
        }
        if(se_chanl->ul_ring) {
                kfree(se_chanl->ul_ring);
        }
        if(se_chanl->ul_pool) {
                kfree(se_chanl->ul_pool);
        }
        if(se_chanl->smem_virt) {
                iounmap(se_chanl->smem_virt);
        }
        if(se_chanl->smem_addr) {
                smem_free(se_chanl->smem_addr, se_chanl->smem_size);
        }

        kfree(se_chanl);

        return 0;
}


int create_seblock_ring_ctrl(struct seblock_ring **out,
                             volatile struct seblock_fifo_info *fifo_info,
                             void *fifo_virt, struct seblock_main_mgr *seblock)
{
        struct seblock_ring *ring = NULL;

        ring = kzalloc(sizeof(struct seblock_ring), GFP_KERNEL);
        if (!ring) {
                SEBLK_ERR("Failed to allocate seblock_ring for seblock\n");
                return -ENOMEM;
        }

        ring->fifo_info = fifo_info;
        ring->fifo_buf = fifo_virt;
        ring->fifo_size = fifo_info->fifo_size;
        ring->seblock = seblock;

        spin_lock_init(&ring->lock);
        init_waitqueue_head(&ring->wait);

        *out = ring;

        return 0;
}


int create_seblock_pool_ctrl(struct seblock_pool **out,
                             volatile struct seblock_fifo_info *fifo_info,
                             void *fifo_virt, void * blk_virt,
                             struct seblock_main_mgr *seblock)
{
        struct seblock_pool *pool = NULL;

        pool = kzalloc(sizeof(struct seblock_pool), GFP_KERNEL);
        if (!pool) {
                SEBLK_ERR("Failed to allocate seblock_fifo for seblock\n");
                return -ENOMEM;
        }

        pool->fifo_info = fifo_info;
        pool->fifo_buf = fifo_virt;
        pool->fifo_size = fifo_info->fifo_size;
        pool->blks_buf = blk_virt;
        pool->blk_size = fifo_info->blk_size;
        pool->seblock = seblock;

        if(blk_virt) {
                int i;
                for (i = 0; i < pool->fifo_size; i++) {
                        pool->fifo_buf[i].addr = pool->blks_buf + i * pool->blk_size
                                                 - seblock->smem_virt + seblock->smem_addr;
                        pool->fifo_buf[i].length = pool->blk_size;
                        pool->fifo_info->fifo_wrptr++;
                }
        }
        spin_lock_init(&pool->lock);
        init_waitqueue_head(&pool->wait);

        *out = pool;
        return 0;
}

int get_item_from_pool(struct seblock_pool *pool, struct sblock_blks *blk)
{
        unsigned long flags;
        int pos;
        int ret = 0;

        /* multi-gotter may cause got failure */
        spin_lock_irqsave(&pool->lock, flags);
        if (pool->fifo_info->fifo_rdptr != pool->fifo_info->fifo_wrptr) {
                pos = sblock_get_ringpos(pool->fifo_info->fifo_rdptr, pool->fifo_size);
                blk->addr = pool->fifo_buf[pos].addr;
                blk->length = pool->blk_size;
                pool->fifo_info->fifo_rdptr++;
        } else {
                ret = -EAGAIN;
        }
        spin_unlock_irqrestore(&pool->lock, flags);

        return ret;
}

int get_pool_free_count(struct seblock_pool *pool)
{
        unsigned long flags;
        int ret = 0;

        spin_lock_irqsave(&pool->lock, flags);
        ret = pool->fifo_info->fifo_wrptr - pool->fifo_info->fifo_rdptr;
        spin_unlock_irqrestore(&pool->lock, flags);

        return ret;
}

int put_item_into_pool(struct seblock_pool *pool, struct sblock_blks *blk)
{
        unsigned long flags;
        int pos;
        int ret = 0;

        spin_lock_irqsave(&pool->lock, flags);
        if ((pool->fifo_info->fifo_wrptr - pool->fifo_info->fifo_rdptr) < pool->fifo_size) {
                pos = sblock_get_ringpos(pool->fifo_info->fifo_wrptr, pool->fifo_size);
                pool->fifo_buf[pos].addr = blk->addr;
                pool->fifo_buf[pos].length = pool->blk_size;
                pool->fifo_info->fifo_wrptr++;
        } else {
                ret = -1;
        }
        spin_unlock_irqrestore(&pool->lock, flags);

        return ret;
}

int put_item_back_to_pool(struct seblock_pool *pool, struct sblock_blks *blk)
{
        unsigned long flags;
        int pos;
        int ret = 0;
        uint32_t last_rdptr;

        spin_lock_irqsave(&pool->lock, flags);
        if ((pool->fifo_info->fifo_wrptr - pool->fifo_info->fifo_rdptr) < pool->fifo_size) {
                last_rdptr = pool->fifo_info->fifo_rdptr - 1;
                pos = sblock_get_ringpos(last_rdptr, pool->fifo_size);
                pool->fifo_buf[pos].addr = blk->addr;
                pool->fifo_buf[pos].length = pool->blk_size;
                pool->fifo_info->fifo_rdptr = last_rdptr;
        } else {
                ret = -1;
        }
        spin_unlock_irqrestore(&pool->lock, flags);

        return ret;
}

int fill_blks_into_priv_pool(struct seblock_pool *pub_pool,struct seblock_pool *priv_pool)
{
        struct sblock_blks blk;
        int i;

        for(i = 0; i < priv_pool->fifo_size; i++) {
                if(!get_item_from_pool(pub_pool, &blk)) {
                        put_item_into_pool(priv_pool, &blk);
                } else {
                        break;
                }
        }
        return 0;
}

int notify_cp_blk_released(uint8_t dst, uint8_t channel)
{
        struct smsg mevt;

        /* send smsg to notify the peer side */
        smsg_set(&mevt, channel, SMSG_TYPE_EVENT, SMSG_EVENT_SBLOCK_RELEASE, 0);
        smsg_send(dst, &mevt, -1);

        return 0;
}
int create_seblock_main_ctrl(struct seblock_main_mgr **out,uint8_t dst,
                             uint32_t txblocknum, uint32_t txblocksize,
                             uint32_t rxblocknum, uint32_t rxblocksize)
{
        struct seblock_main_mgr *seblock = NULL;
        volatile struct seblock_fifo_info *dl_fifo_info;
        volatile struct seblock_fifo_info *ul_fifo_info;
        uint32_t offset_dl_blk;
        uint32_t offset_dl_fifo;
        uint32_t offset_ul_blk;
        uint32_t offset_ul_fifo;
        volatile uint32_t *chanl_ptr;
        uint32_t total_size = 0;
        int ret = 0;
        int i;

        seblock = kzalloc(sizeof(struct seblock_main_mgr) , GFP_KERNEL);
        if (!seblock) {
                SEBLK_ERR("Failed to kzalloc for seblock\n");
                return -ENOMEM;
        }

        memset(seblock, 0, sizeof(struct seblock_main_mgr));
        seblock->dst = dst;
        seblock->ul_blk_size = SBLOCKSZ_ALIGN(txblocksize,SBLOCK_ALIGN_BYTES);
        seblock->dl_blk_size = SBLOCKSZ_ALIGN(rxblocksize,SBLOCK_ALIGN_BYTES);
        seblock->ul_fifo_size = txblocknum;
        seblock->dl_fifo_size = rxblocknum;

        /* allocated smem based on seblock_main_mgr structure , dl & ul mixed */
        total_size = sizeof(struct seblock_fifo_info);                          /* dl pub pool info */
        total_size += sizeof(struct seblock_fifo_info);                         /* ul pub pool info */
        total_size += sizeof(uint32_t) * SMSG_CH_NR;                            /* channels smem ptr */
        total_size += seblock->dl_blk_size * seblock->dl_fifo_size;             /* dl blk mem */
        total_size += seblock->ul_blk_size * seblock->ul_fifo_size;             /* ul blk mem */
        total_size += sizeof(struct sblock_blks) * seblock->dl_fifo_size;       /* dl pool fifo mem */
        total_size += sizeof(struct sblock_blks) * seblock->ul_fifo_size;       /* ul pool fifo mem */

        seblock->smem_size = SBLOCKSZ_ALIGN(total_size, SBLOCK_ALIGN_BYTES);
        seblock->smem_addr = smem_alloc(seblock->smem_size);
        if (!seblock->smem_addr) {
                SEBLK_ERR("Failed to allocate smem for seblock\n");
                ret = -ENOMEM;
                goto fail;
        }

        seblock->smem_virt = ioremap_nocache(seblock->smem_addr, seblock->smem_size);
        if (!seblock->smem_virt) {
                SEBLK_ERR("Failed to map smem for seblock\n");
                ret = -EFAULT;
                goto fail;
        }

        memset(seblock->smem_virt, 0, seblock->smem_size);

        /* dl pub pool */
        offset_dl_blk = sizeof(struct seblock_fifo_info) +              /* dl pub pool info */
                        sizeof(struct seblock_fifo_info) +                      /* ul pub pool info */
                        (sizeof(uint32_t) * SMSG_CH_NR);                        /* channels smem ptr */
        offset_dl_fifo = sizeof(struct seblock_fifo_info) +             /* dl pub pool info */
                         sizeof(struct seblock_fifo_info) +                      /* ul pub pool info */
                         (sizeof(uint32_t) * SMSG_CH_NR) +                       /* channels smem ptr */
                         (seblock->dl_blk_size * seblock->dl_fifo_size) +        /* dl blk mem */
                         (seblock->ul_blk_size * seblock->ul_fifo_size);         /* ul blk mem */

        dl_fifo_info = (volatile struct seblock_fifo_info *)(seblock->smem_virt);
        dl_fifo_info->blks_addr = seblock->smem_addr + offset_dl_blk; /* offset of smem_addr */
        dl_fifo_info->fifo_size = seblock->dl_fifo_size;
        dl_fifo_info->blk_size = seblock->dl_blk_size;
        dl_fifo_info->fifo_rdptr = 0;
        dl_fifo_info->fifo_wrptr = 0;
        dl_fifo_info->fifo_addr = seblock->smem_addr + offset_dl_fifo;  /* offset of smem_addr */

        ret = create_seblock_pool_ctrl(&seblock->dl_pub_pool, dl_fifo_info,
                                       seblock->smem_virt + offset_dl_fifo,
                                       seblock->smem_virt + offset_dl_blk,
                                       seblock);
        if(ret) {
                goto fail;
        }
        /* ul pub pool */
        offset_ul_blk = sizeof(struct seblock_fifo_info) +              /* dl pub pool info */
                        sizeof(struct seblock_fifo_info) +                      /* ul pub pool info */
                        (sizeof(uint32_t) * SMSG_CH_NR) +                       /* channels smem ptr */
                        (seblock->dl_fifo_size * seblock->dl_blk_size);         /* dl blk mem */

        offset_ul_fifo = sizeof(struct seblock_fifo_info) +             /* dl pub pool info */
                         sizeof(struct seblock_fifo_info) +                      /* ul pub pool info */
                         (sizeof(uint32_t) * SMSG_CH_NR) +                       /* channels smem ptr */
                         (seblock->dl_blk_size * seblock->dl_fifo_size) +        /* dl blk mem */
                         (seblock->ul_blk_size * seblock->ul_fifo_size) +        /* ul blk mem */
                         (sizeof(struct sblock_blks) * seblock->dl_fifo_size);   /* dl blk mem */

        ul_fifo_info = (volatile struct seblock_fifo_info *)((seblock->smem_virt) +
                        sizeof(struct seblock_fifo_info));                       /* dl pub pool info */
        ul_fifo_info->blks_addr = seblock->smem_addr + offset_ul_blk;           /* offset of smem_addr */
        ul_fifo_info->fifo_size = seblock->ul_fifo_size;
        ul_fifo_info->blk_size = seblock->ul_blk_size;
        ul_fifo_info->fifo_rdptr = 0;
        ul_fifo_info->fifo_wrptr = 0;
        ul_fifo_info->fifo_addr = seblock->smem_addr + offset_ul_fifo;  /* offset of smem_addr */

        ret = create_seblock_pool_ctrl(&seblock->ul_pub_pool, ul_fifo_info,
                                       seblock->smem_virt + offset_ul_fifo,
                                       seblock->smem_virt + offset_ul_blk,
                                       seblock);
        if(ret) {
                goto fail;
        }
        /* init seblock channel ptrs */
        chanl_ptr = (volatile uint32_t *)((seblock->smem_virt) +
                                          (sizeof(struct seblock_fifo_info) * 2));                /* dl & ul pub pool info */
        for(i = 0; i < SMSG_CH_NR; i++) {
                *chanl_ptr = 0;
                chanl_ptr++;
        }
        seblock->state = SEBLOCK_STATE_READY;
        *out = seblock;

        return ret;
fail:
        if(seblock) {
                destroy_seblock_main_ctrl(seblock);
        }

        return ret;
}

int init_seblock_channel_fifos(struct seblock_main_mgr *seblock,
                               struct seblock_channel_mgr *se_chanl, uint32_t dl_fifo_size,
                               uint32_t ul_fifo_size)
{
        volatile struct seblock_fifo_info *fifo_info;
        uint32_t offset;
        int ret = 0;

        /* dl ring fifo info */
        fifo_info = (volatile struct seblock_fifo_info *)se_chanl->smem_virt;
        fifo_info->blks_addr = 0;
        fifo_info->blk_size = seblock->dl_blk_size;
        fifo_info->fifo_size = seblock->dl_fifo_size;
        fifo_info->fifo_rdptr = 0;
        fifo_info->fifo_wrptr = 0;
        offset = sizeof(struct seblock_fifo_info) * 4;                  /* dl ul ring pool info */
        fifo_info->fifo_addr = se_chanl->smem_addr + offset;
        ret = create_seblock_ring_ctrl(&se_chanl->dl_ring, fifo_info,
                                       se_chanl->smem_virt + offset, seblock);
        if(ret) {
                return ret;
        }
        /* ul ring fifo info */
        fifo_info = (volatile struct seblock_fifo_info *)(se_chanl->smem_virt +
                        sizeof(struct seblock_fifo_info));                       /* dl ring info */
        fifo_info->blks_addr = 0;
        fifo_info->blk_size = seblock->ul_blk_size;
        fifo_info->fifo_size = seblock->ul_fifo_size;
        fifo_info->fifo_rdptr = 0;
        fifo_info->fifo_wrptr = 0;
        offset = (sizeof(struct seblock_fifo_info) * 4) +               /* dl ul ring pool info */
                 (sizeof(struct sblock_blks) * seblock->dl_fifo_size);   /* dl ring fifo mem */
        fifo_info->fifo_addr = se_chanl->smem_addr + offset;
        ret = create_seblock_ring_ctrl(&se_chanl->ul_ring, fifo_info,
                                       se_chanl->smem_virt + offset, seblock);
        if(ret) {
                return ret;
        }
        /* dl pool fifo info */
        fifo_info = (volatile struct seblock_fifo_info *)(se_chanl->smem_virt +
                        sizeof(struct seblock_fifo_info) +                      /* dl ring info */
                        sizeof(struct seblock_fifo_info));                      /* ul ring info */
        fifo_info->blks_addr = 0;
        fifo_info->blk_size = seblock->dl_blk_size;
        fifo_info->fifo_size = dl_fifo_size;
        fifo_info->fifo_rdptr = 0;
        fifo_info->fifo_wrptr = 0;
        offset = (sizeof(struct seblock_fifo_info) * 4) +               /* dl ul ring pool info */
                 (sizeof(struct sblock_blks) * seblock->dl_fifo_size) +   /* dl ring fifo mem */
                 (sizeof(struct sblock_blks) * seblock->ul_fifo_size);   /* ul ring fifo mem */
        fifo_info->fifo_addr = se_chanl->smem_addr + offset;
        ret = create_seblock_pool_ctrl(&se_chanl->dl_pool, fifo_info,
                                       se_chanl->smem_virt + offset, NULL, seblock);
        if(ret) {
                return ret;
        }
        fill_blks_into_priv_pool(seblock->dl_pub_pool, se_chanl->dl_pool);
        /* ul pool fifo info */
        fifo_info = (volatile struct seblock_fifo_info *)(se_chanl->smem_virt +
                        sizeof(struct seblock_fifo_info) +                      /* dl ring info */
                        sizeof(struct seblock_fifo_info) +                      /* ul ring info */
                        sizeof(struct seblock_fifo_info));                      /* dl pool info */
        fifo_info->blks_addr = 0;
        fifo_info->blk_size = seblock->ul_blk_size;
        fifo_info->fifo_size = ul_fifo_size;
        fifo_info->fifo_rdptr = 0;
        fifo_info->fifo_wrptr = 0;
        offset = (sizeof(struct seblock_fifo_info) * 4) +                       /* dl ul ring pool info */
                 (sizeof(struct sblock_blks) * seblock->dl_fifo_size) +          /* dl ring fifo mem */
                 (sizeof(struct sblock_blks) * seblock->ul_fifo_size) +          /* ul ring fifo mem */
                 (sizeof(struct sblock_blks) * se_chanl->dl_pool->fifo_size);     /* dl pool fifo mem */
        fifo_info->fifo_addr = se_chanl->smem_addr + offset;
        ret = create_seblock_pool_ctrl(&se_chanl->ul_pool, fifo_info,
                                       se_chanl->smem_virt + offset, NULL, seblock);
        if(ret) {
                return ret;
        }
        fill_blks_into_priv_pool(seblock->ul_pub_pool, se_chanl->ul_pool);

        return ret;
}

int update_channel_ptr_in_shmem(struct seblock_main_mgr *seblock,
                                struct seblock_channel_mgr *se_chanl)
{
        uint32_t offset;
        volatile uint32_t *chan_ptr_mem_ptr;

        offset = sizeof(struct seblock_fifo_info);                          /* dl pub pool info */
        offset += sizeof(struct seblock_fifo_info);                         /* ul pub pool info */
        offset += sizeof(uint32_t) * se_chanl->channel;

        chan_ptr_mem_ptr = (volatile uint32_t *)(seblock->smem_virt + offset);
        *chan_ptr_mem_ptr = se_chanl->smem_addr;

        return 0;
}

int create_seblock_channel_ctrl(uint8_t dst, uint8_t channel,
                                struct seblock_channel_mgr **out, struct seblock_main_mgr *seblock,
                                uint32_t txpoolsize, uint32_t rxpoolsize)
{
        struct seblock_channel_mgr *se_chanl;
        int ret;

        /* create channel ctrl */
        se_chanl = kzalloc(sizeof(struct seblock_channel_mgr) , GFP_KERNEL);
        if (!se_chanl) {
                return -ENOMEM;
        }
        memset(se_chanl, 0, sizeof(struct seblock_channel_mgr));

        se_chanl->state = SBLOCK_STATE_IDLE;
        se_chanl->dst = dst;
        se_chanl->channel = channel;
        se_chanl->seblock = seblock;
        se_chanl->smem_size = sizeof(struct seblock_fifo_info) * 4;      	/* dl ul ring pool info */
        se_chanl->smem_size += sizeof(struct sblock_blks) * seblock->dl_fifo_size;    /* dl ring fifo mem */
        se_chanl->smem_size += sizeof(struct sblock_blks) * seblock->ul_fifo_size;    /* ul ring fifo mem */
        se_chanl->smem_size += sizeof(struct sblock_blks) * rxpoolsize ;              /* dl priv pool fifo mem */
        se_chanl->smem_size += sizeof(struct sblock_blks) * txpoolsize;                /* ul priv pool fifo mem */

        SEBLK_DEBUG("se_chanl->smem_size = %d\n", se_chanl->smem_size);

        se_chanl->smem_addr = smem_alloc(se_chanl->smem_size);		/*single channel in smem*/
        if (!se_chanl->smem_addr) {
                SEBLK_ERR("Failed to allocate smem for se_chanl\n");
                return -ENOMEM;
        }
        se_chanl->smem_virt = ioremap_nocache(se_chanl->smem_addr, se_chanl->smem_size);
        if (!se_chanl->smem_virt) {
                SEBLK_ERR("Failed to map smem for se_chanl\n");
                return -EFAULT;
        }
        /* init ring/pool fifos in channel ctrl */
        ret = init_seblock_channel_fifos(seblock, se_chanl, rxpoolsize, txpoolsize);
        if(ret) {
                return ret;
        }
        /* update channel ptr in main share mem */
        update_channel_ptr_in_shmem(seblock, se_chanl);		/*physical address*/
        seblock->channels[channel] = se_chanl;

        *out = se_chanl;

        return 0;
}
static int seblock_recover_channel(struct seblock_main_mgr *seblock,
                                   struct seblock_channel_mgr *se_chanl)
{
        unsigned long flags;
        volatile struct seblock_fifo_info *fifo_info;
        struct sblock_blks blk_phy;
        int pos;

        se_chanl->state = SBLOCK_STATE_IDLE;

        /* clean dl ring blks */
        fifo_info = se_chanl->dl_ring->fifo_info;

        spin_lock_irqsave(&se_chanl->dl_ring->lock, flags);
        if(se_chanl->dl_record) {
                blk_phy.addr = se_chanl->dl_record;
                blk_phy.length = se_chanl->seblock->dl_blk_size;
                /*sblock has not been released, 1st try to put into private pool */
                if(put_item_into_pool(se_chanl->dl_pool, &blk_phy)) {
                        /* 2nd, try public pool */
                        if(put_item_into_pool(seblock->dl_pub_pool, &blk_phy)) {
                                BUG_ON(1);
                        }
                }
                se_chanl->dl_record = 0;
        }
        while(fifo_info->fifo_wrptr != fifo_info->fifo_rdptr) {
                pos = sblock_get_ringpos(fifo_info->fifo_rdptr, se_chanl->dl_ring->fifo_size);
                blk_phy.addr = se_chanl->dl_ring->fifo_buf[pos].addr;
                blk_phy.length = se_chanl->dl_ring->fifo_buf[pos].length;
                fifo_info->fifo_rdptr++;
                /*release sblocks from CP via ring fifo, 1st try to put into private pool */
                if(put_item_into_pool(se_chanl->dl_pool, &blk_phy)) {
                        /* 2nd, try public pool */
                        if(put_item_into_pool(seblock->dl_pub_pool, &blk_phy)) {
                                BUG_ON(1);
                        }
                }
        }
        spin_unlock_irqrestore(&se_chanl->dl_ring->lock, flags);

        /* clean ul ring blks */
        fifo_info = se_chanl->ul_ring->fifo_info;

        spin_lock_irqsave(&se_chanl->ul_ring->lock, flags);
        while(fifo_info->fifo_wrptr != fifo_info->fifo_rdptr) {
                pos = sblock_get_ringpos(fifo_info->fifo_rdptr, se_chanl->ul_ring->fifo_size);
                blk_phy.addr = se_chanl->ul_ring->fifo_buf[pos].addr;
                blk_phy.length = se_chanl->ul_ring->fifo_buf[pos].length;
                fifo_info->fifo_wrptr--;
                /*sblocks have been sent, 1st try put back to private pool */
                if(put_item_back_to_pool(se_chanl->ul_pool, &blk_phy)) {
                        /* 2nd, try public pool */
                        if(put_item_back_to_pool(seblock->ul_pub_pool, &blk_phy)) {
                                BUG_ON(1);
                        }
                }
        }

        if(se_chanl->ul_record) {
                blk_phy.addr = se_chanl->ul_record;
                blk_phy.length = se_chanl->seblock->ul_blk_size;
                /*sblock has not been sent, 1st try put back to private pool */
                if(put_item_back_to_pool(se_chanl->ul_pool, &blk_phy)) {
                        /* 2nd, try public pool */
                        if(put_item_back_to_pool(seblock->ul_pub_pool, &blk_phy)) {
                                BUG_ON(1);
                        }
                }
                se_chanl->ul_record = 0;
        }
        spin_unlock_irqrestore(&se_chanl->ul_ring->lock, flags);

        return 0;
}


/* recover all channels here */
static int seblock_recover(uint8_t dst)
{
        struct seblock_main_mgr *seblock = (struct seblock_main_mgr *)seblocks[dst];
        struct seblock_channel_mgr *se_chanl;
        int i;

        if (!seblock) {
                return -ENODEV;
        }

        for(i = 0; i < SMSG_CH_NR; i++) {
                se_chanl = seblock->channels[i];
                if (se_chanl) {
                        seblock_recover_channel(seblock, se_chanl);
                }
        }

        return 0;
}

static int seblock_thread(void *data)
{
        struct seblock_channel_mgr *se_chanl = data;
        struct smsg mcmd, mrecv;
        struct sched_param param = {.sched_priority = 90};
        int rval;

        /*set the thread as a real time thread, and its priority is 90*/
        sched_setscheduler(current, SCHED_RR, &param);

        /* since the channel open may hang, we call it in the seblock thread */
        rval = smsg_ch_open(se_chanl->dst, se_chanl->channel, -1);
        if (rval != 0) {
                SEBLK_ERR("Failed to open channel %d\n", se_chanl->channel);
                /* assign NULL to thread poniter as failed to open channel */
                se_chanl->thread = NULL;
                return rval;
        }
        //msleep(30 * 1000);
        /* handle the seblock events */
        while (!kthread_should_stop()) {
                /* monitor seblock recv smsg */
                smsg_set(&mrecv, se_chanl->channel, 0, 0, 0);
                rval = smsg_recv(se_chanl->dst, &mrecv, -1);
                if (rval == -EIO || rval == -ENODEV) {
                        /* channel state is FREE */
                        msleep(5);
                        continue;
                }

                SEBLK_DEBUG("seblock thread recv msg: dst=%d, channel=%d, "
                            "type=%d, flag=0x%04x, value=0x%08x\n",
                            se_chanl->dst, se_chanl->channel,
                            mrecv.type, mrecv.flag, mrecv.value);

                switch (mrecv.type) {
                case SMSG_TYPE_OPEN:
                        /* just ack open */
                        smsg_open_ack(se_chanl->dst, se_chanl->channel);
                        break;
                case SMSG_TYPE_CLOSE:
                        /* handle channel close */
                        smsg_close_ack(se_chanl->dst, se_chanl->channel);
                        if (se_chanl->handler) {
                                se_chanl->handler(SBLOCK_NOTIFY_CLOSE, se_chanl->data);
                        }
                        se_chanl->state = SBLOCK_STATE_IDLE;
                        break;
                case SMSG_TYPE_CMD:
                        /* respond cmd done for seblock init */
                        WARN_ON(mrecv.flag != SMSG_CMD_SBLOCK_INIT);

                        /* handle channel recovery */
                        if (se_chanl->seblock->recovery && (mrecv.value == 0)) {
                                SEBLK_INFO("seblock start recover! recv msg: dst=%d, channel=%d, "
                                           "type=%d, flag=0x%04x, value=0x%08x\n",
                                           se_chanl->dst, se_chanl->channel,
                                           mrecv.type, mrecv.flag, mrecv.value);
                                if (se_chanl->handler) {
                                        se_chanl->handler(SBLOCK_NOTIFY_CLOSE, se_chanl->data);
                                }
                                /*no channel of CP is opened yet*/
                                seblock_recover(se_chanl->dst);
                        }

                        /* give smem address to cp side */
                        smsg_set(&mcmd, se_chanl->channel, SMSG_TYPE_DONE,
                                 SMSG_DONE_SBLOCK_INIT, se_chanl->seblock->smem_addr);
                        smsg_send(se_chanl->dst, &mcmd, -1);
                        if (se_chanl->handler) {
                                se_chanl->handler(SBLOCK_NOTIFY_OPEN, se_chanl->data);
                        }
                        se_chanl->state = SBLOCK_STATE_READY;
                        se_chanl->seblock->recovery = 1;
                        break;
                case SMSG_TYPE_EVENT:
                        /* handle seblock send/release events */
                        switch (mrecv.flag) {
                        case SMSG_EVENT_SBLOCK_SEND:
                                wake_up_interruptible_all(&se_chanl->dl_ring->wait);
                                if (se_chanl->handler) {
                                        se_chanl->handler(SBLOCK_NOTIFY_RECV, se_chanl->data);
                                }
                                break;
                        case SMSG_EVENT_SBLOCK_RELEASE:
                                wake_up_interruptible_all(&(se_chanl->ul_ring->wait));
                                if (se_chanl->handler) {
                                        se_chanl->handler(SBLOCK_NOTIFY_GET, se_chanl->data);
                                }
                                break;
                        default:
                                rval = 1;
                                break;
                        }
                        break;
                default:
                        rval = 1;
                        break;
                };
                if (rval) {
                        SEBLK_INFO("non-handled seblock msg: %d-%d, %d, %d, %d\n",
                                   se_chanl->dst, se_chanl->channel,
                                   mrecv.type, mrecv.flag, mrecv.value);
                        rval = 0;
                }
        }

        SEBLK_ERR("se_chanl %d-%d thread stop", se_chanl->dst, se_chanl->channel);

        return rval;
}

int seblock_create(uint8_t dst, uint8_t channel,
                   uint32_t txblocknum, uint32_t txblocksize,uint32_t txpoolsize,
                   uint32_t rxblocknum, uint32_t rxblocksize,uint32_t rxpoolsize)
{
        struct seblock_main_mgr *seblock = NULL;
        struct seblock_channel_mgr *se_chanl = NULL;
        int ret = 0;

        BUG_ON((dst >= SIPC_ID_NR) || (channel >= SMSG_CH_NR));

        SEBLK_DEBUG("txblocknum: %d, txblocksize: %d, txpoolsize: %d\n",
                    txblocknum, txblocksize, txpoolsize);
        SEBLK_DEBUG("rxblocknum: %d, rxblocksize: %d, rxpoolsize: %d\n",
                    rxblocknum, rxblocksize, rxpoolsize);
        /* check and create main ctrl */
        seblock = seblocks[dst];

        if(!seblock) {
                ret = create_seblock_main_ctrl(&seblock, dst,
                                               txblocknum,txblocksize,
                                               rxblocknum, rxblocksize);
                if(ret)
                        return ret;
                seblocks[dst] = seblock;
        }
        ret = create_seblock_channel_ctrl(dst, channel,
                                          &se_chanl, seblock, txpoolsize, rxpoolsize);
        if(ret) {
                goto fail;
        }
        /* create channel thread for this seblock channel */
        se_chanl->thread = kthread_create(seblock_thread, se_chanl,
                                          "seblock-%d-%d", dst, channel);
        if (IS_ERR(se_chanl->thread)) {
                SEBLK_ERR("Failed to create kthread: seblock-%d-%d\n", dst, channel);
                ret = PTR_ERR(se_chanl->thread);
                goto fail;
        }

        wake_up_process(se_chanl->thread);

        return 0;
fail:
        if(se_chanl) {
                destroy_seblock_channel_ctrl(se_chanl);
        }
        return ret;
}

int seblock_destroy(uint8_t dst, uint8_t channel)
{
        struct seblock_main_mgr *seblock = (struct seblock_main_mgr *)seblocks[dst];
        struct seblock_channel_mgr *se_chanl;

        if (!seblock) {
                return -ENODEV;
        }

        se_chanl = seblock->channels[channel];

        if (!se_chanl) {
                return -ENODEV;
        }

        seblock_recover_channel(seblock, se_chanl);
        smsg_ch_close(dst, channel, -1);

        /* stop seblock thread if it's created successfully and still alive */
        if (!IS_ERR_OR_NULL(se_chanl->thread)) {
                kthread_stop(se_chanl->thread);
        }

        if (se_chanl->dl_ring) {	/*dl_ring fifo*/
                wake_up_interruptible_all(&se_chanl->dl_ring->wait);
                kfree(se_chanl->dl_ring);
        }
        if (se_chanl->ul_ring) {	/*ul_ring fifo*/
                wake_up_interruptible_all(&se_chanl->ul_ring->wait);
                kfree(se_chanl->ul_ring);
        }
        if (se_chanl->dl_pool) {	/*dl_pool fifo*/
                kfree(se_chanl->dl_pool);
        }
        if (se_chanl->ul_pool) {	/*ul_pool fifo*/
                kfree(se_chanl->ul_pool);
        }
        if (se_chanl->smem_virt) {
                iounmap(se_chanl->smem_virt);
        }
        smem_free(se_chanl->smem_addr, se_chanl->smem_size);
        kfree(se_chanl);

        seblocks[dst]=NULL;

        return 0;
}

int seblock_get(uint8_t dst, uint8_t channel, struct sblock *blk)
{
        struct seblock_main_mgr *seblock = (struct seblock_main_mgr *)seblocks[dst];
        struct seblock_channel_mgr *se_chanl;
        struct sblock_blks blk_phy;

        if (!seblock) {
                SEBLK_ERR("seblock-%d-%d seblock_get seblock not ready!\n", dst, channel);
                return -ENODEV;
        }

        se_chanl = seblock->channels[channel];

        if (!se_chanl) {
                SEBLK_ERR("seblock-%d-%d seblock_get se_chanl not ready!\n", dst, channel);
                return -ENODEV;
        }

        if (se_chanl->state != SBLOCK_STATE_READY) {
                SEBLK_ERR("seblock-%d-%d seblock_get not ready!\n", dst, channel);
                return -EIO;
        }

        /* 1st, get from private pool */
        if(get_item_from_pool(se_chanl->ul_pool, &blk_phy)) {
                /* 2nd, try public pool */
                if(get_item_from_pool(seblock->ul_pub_pool, &blk_phy)) {
                        SEBLK_INFO("seblock_get %d-%d is empty!\n",
                                   dst, channel);
                        return -EAGAIN;
                }
        }

        /* save to records */
        se_chanl->ul_record = blk_phy.addr;

        /* smem addr to virt addr */
        blk->addr = seblock->smem_virt + (blk_phy.addr - seblock->smem_addr);
        blk->length = blk_phy.length;

        return 0;
}

/*put back*/
int seblock_put(uint8_t dst, uint8_t channel, struct sblock *blk)
{
        struct seblock_main_mgr *seblock = (struct seblock_main_mgr *)seblocks[dst];
        struct seblock_channel_mgr *se_chanl;
        struct sblock_blks blk_phy;

        if (!seblock) {
                SEBLK_ERR("seblock-%d-%d seblock_put seblock is not exist!\n", dst, channel);
                return -ENODEV;
        }

        se_chanl = seblock->channels[channel];

        if (!se_chanl) {
                SEBLK_ERR("seblock-%d-%d seblock_put se_chanl is not exist!\n", dst, channel);
                return -ENODEV;
        }

        if (se_chanl->state != SBLOCK_STATE_READY) {
                SEBLK_ERR("seblock-%d-%d seblock_put not ready!\n", dst, channel);
                return -EIO;
        }

        /* virt  addr to smem addr */
        blk_phy.addr = blk->addr - seblock->smem_virt + seblock->smem_addr;
        blk_phy.length = seblock->ul_blk_size;

        /* 1st, try put to private pool */
        if(put_item_back_to_pool(se_chanl->ul_pool, &blk_phy)) {
                /* 2nd, try public pool */
                if(put_item_back_to_pool(seblock->ul_pub_pool, &blk_phy)) {
                        BUG_ON(1);
                }
        }
        /* pop from records */
        se_chanl->ul_record = 0;

        return 0;
}

int seblock_flush(uint8_t dst, uint8_t channel)
{
        struct seblock_main_mgr *seblock = (struct seblock_main_mgr *)seblocks[dst];
        struct seblock_channel_mgr *se_chanl;
        struct smsg mevt;
        int rval = 0;

        if (!seblock) {
                SEBLK_ERR("seblock-%d-%d seblock_flush seblock is not exist!\n", dst, channel);
                return -ENODEV;
        }

        se_chanl = seblock->channels[channel];

        if (!se_chanl) {
                SEBLK_ERR("seblock-%d-%d seblock_flush se_chanl is not exist!\n", dst, channel);
                return -ENODEV;
        }

        if (se_chanl->state != SBLOCK_STATE_READY) {
                SEBLK_ERR("seblock-%d-%d seblock_flush not ready!\n", dst, channel);
                return -EIO;
        }

        if (se_chanl->yell) {
                se_chanl->yell = 0;
                smsg_set(&mevt, channel, SMSG_TYPE_EVENT, SMSG_EVENT_SBLOCK_SEND, 0);
                rval = smsg_send(dst, &mevt, 0);
        }

        return rval;
}

int seblock_send(uint8_t dst, uint8_t channel, struct sblock *blk)
{
        struct seblock_main_mgr *seblock = (struct seblock_main_mgr *)seblocks[dst];
        struct seblock_channel_mgr *se_chanl;
        struct sblock_blks blk_phy;
        unsigned long flags;
        int pos;

        if (!seblock) {
                SEBLK_ERR("seblock-%d-%d seblock_send seblock is not exist!\n", dst, channel);
                return -ENODEV;
        }

        se_chanl = seblock->channels[channel];

        if (!se_chanl) {
                SEBLK_ERR("seblock-%d-%d seblock_send se_chanl is not exist!\n", dst, channel);
                return -ENODEV;
        }

        if (se_chanl->state != SBLOCK_STATE_READY) {
                SEBLK_ERR("seblock-%d-%d seblock_send not ready!\n", dst, channel);
                return -EIO;
        }

        SEBLK_DEBUG("seblock_send: dst=%d, channel=%d, addr=%p, len=%d\n",
                    dst, channel, blk->addr, blk->length);

        /* virt  addr to smem addr */
        blk_phy.addr = blk->addr - seblock->smem_virt + seblock->smem_addr;
	blk_phy.length = blk->length;

        /* multi-gotter may cause got failure */
        spin_lock_irqsave(&se_chanl->ul_ring->lock, flags);
        pos = sblock_get_ringpos(se_chanl->ul_ring->fifo_info->fifo_wrptr, se_chanl->ul_ring->fifo_size);
        se_chanl->ul_ring->fifo_buf[pos].addr = blk_phy.addr;
        se_chanl->ul_ring->fifo_buf[pos].length = blk_phy.length;
        se_chanl->ul_ring->fifo_info->fifo_wrptr++;
        if(((int)(se_chanl->ul_ring->fifo_info->fifo_wrptr - se_chanl->ul_ring->fifo_info->fifo_rdptr) == 1)) {
                se_chanl->yell = 1;
        }
        spin_unlock_irqrestore(&se_chanl->ul_ring->lock, flags);

        /* pop from records */
        se_chanl->ul_record = 0;

        if ((se_chanl->ul_ring->fifo_info->fifo_wrptr - se_chanl->ul_ring->fifo_info->fifo_rdptr)
            >= se_chanl->ul_ring->fifo_info->fifo_size) {
                seblock_flush(dst, channel);
        }

        return 0;
}

int seblock_receive(uint8_t dst, uint8_t channel, struct sblock *blk)
{
        struct seblock_main_mgr *seblock = (struct seblock_main_mgr *)seblocks[dst];
        struct seblock_channel_mgr *se_chanl;
        volatile struct seblock_fifo_info *fifo_info;
        struct sblock_blks blk_phy;
        uint32_t pos;
        unsigned long flags;

        if (!seblock) {
                SEBLK_ERR("seblock-%d-%d seblock_receive seblock is not exist!\n", dst, channel);
                return -ENODEV;
        }

        se_chanl = seblock->channels[channel];

        if (!se_chanl) {
                SEBLK_ERR("seblock-%d-%d seblock_receive se_chanl is not exist!\n", dst, channel);
                return -ENODEV;
        }

        if (se_chanl->state != SBLOCK_STATE_READY) {
                SEBLK_ERR("seblock-%d-%d seblock_receive not ready!\n", dst, channel);
                WARN_ON(0);
                return -EIO;
        }

        fifo_info = se_chanl->dl_ring->fifo_info;
        SEBLK_DEBUG("seblock_receive: channel=%d, wrptr=%d, rdptr=%d",
                    channel, fifo_info->fifo_rdptr,
                    fifo_info->fifo_wrptr);

        if (fifo_info->fifo_wrptr == fifo_info->fifo_rdptr) {
                /* no wait */
                SEBLK_DEBUG("seblock_receive %d-%d is empty!\n",
                            dst, channel);
                return -ENODATA;
        }

        /* multi-receiver may cause recv failure */
        spin_lock_irqsave(&se_chanl->dl_ring->lock, flags);
        pos = sblock_get_ringpos(fifo_info->fifo_rdptr, se_chanl->dl_ring->fifo_size);
        blk_phy.addr = se_chanl->dl_ring->fifo_buf[pos].addr;
        blk_phy.length = se_chanl->dl_ring->fifo_buf[pos].length;
        fifo_info->fifo_rdptr++;
        spin_unlock_irqrestore(&se_chanl->dl_ring->lock, flags);

        SEBLK_DEBUG("seblock_receive: channel=%d, rxpos=%d, addr=0x%0x, len=%d\n",
                    channel, pos, blk_phy.addr, blk_phy.length);
        /* save record */
        se_chanl->dl_record = blk_phy.addr;

        /* smem addr to virt addr */
        blk->addr = seblock->smem_virt + (blk_phy.addr - seblock->smem_addr);
        blk->length = blk_phy.length;
        return 0;
}

int seblock_release(uint8_t dst, uint8_t channel, struct sblock *blk)
{
        struct seblock_main_mgr *seblock = (struct seblock_main_mgr *)seblocks[dst];
        struct seblock_channel_mgr *se_chanl;
        volatile struct seblock_fifo_info *fifo_info;
        struct sblock_blks blk_phy;

        if (!seblock) {
                SEBLK_ERR("seblock-%d-%d seblock_release seblock is not exist!\n", dst, channel);
                return -ENODEV;
        }

        se_chanl = seblock->channels[channel];

        if (!se_chanl) {
                SEBLK_ERR("seblock-%d-%d seblock_release se_chanl is not exist!\n", dst, channel);
                return -ENODEV;
        }

        if (se_chanl->state != SBLOCK_STATE_READY) {
                SEBLK_ERR("seblock-%d-%d seblock_release not ready!\n", dst, channel);
                return -EIO;
        }

        SEBLK_DEBUG("seblock_release: dst=%d, channel=%d, addr=%p, len=%d\n",
                    dst, channel, blk->addr, blk->length);

        fifo_info = se_chanl->dl_pool->fifo_info;

        /* virt  addr to smem addr */
        blk_phy.addr = blk->addr - seblock->smem_virt + seblock->smem_addr;
        blk_phy.length = seblock->dl_blk_size;

        /* 1st, try put to private pool */
        if(put_item_into_pool(se_chanl->dl_pool, &blk_phy)) {
                /* 2nd, try public pool */
                if(put_item_into_pool(seblock->dl_pub_pool, &blk_phy)) {
                        BUG_ON(1);
                }
        } else {
                if((int)(fifo_info->fifo_wrptr - fifo_info->fifo_rdptr) == 1) {
                        notify_cp_blk_released(dst, channel);
                }
        }
        /* pop record */
        se_chanl->dl_record = 0;
        return 0;
}

int seblock_get_arrived_count(uint8_t dst, uint8_t channel)
{
        struct seblock_main_mgr *seblock = (struct seblock_main_mgr *)seblocks[dst];
        struct seblock_channel_mgr *se_chanl;
        volatile struct seblock_fifo_info *fifo_info;
        unsigned long flags;
        int blk_count = 0;

        if (!seblock) {
                SEBLK_ERR("seblock-%d-%d get_arrived_count seblock is not exist!\n", dst, channel);
                return -ENODEV;
        }

        se_chanl = seblock->channels[channel];

        if (!se_chanl) {
                SEBLK_ERR("seblock-%d-%d get_arrived_count se_chanl is not exist!\n", dst, channel);
                return -ENODEV;
        }

        if (se_chanl->state != SBLOCK_STATE_READY) {
                SEBLK_ERR("seblock-%d-%d get_arrived_count not ready!\n", dst, channel);
                return -EIO;
        }

        fifo_info = se_chanl->dl_ring->fifo_info;

        spin_lock_irqsave(&se_chanl->dl_ring->lock, flags);
        blk_count = (int)(fifo_info->fifo_wrptr - fifo_info->fifo_rdptr);	/*sblocks sent by CP have not been received by AP*/
        spin_unlock_irqrestore(&se_chanl->dl_ring->lock, flags);

        return blk_count;

}

int seblock_get_free_count(uint8_t dst, uint8_t channel)
{
        struct seblock_main_mgr *seblock = (struct seblock_main_mgr *)seblocks[dst];
        struct seblock_channel_mgr *se_chanl;
        int blk_count = 0;

        if (!seblock) {
                SEBLK_ERR("seblock-%d-%d get_free_count seblock is not exist!\n", dst, channel);
                return -ENODEV;
        }

        se_chanl = seblock->channels[channel];

        if (!se_chanl) {
                SEBLK_ERR("seblock-%d-%d get_free_count se_chanl is not exist!\n", dst, channel);
                return -ENODEV;
        }

        if (se_chanl->state != SBLOCK_STATE_READY) {
                SEBLK_ERR("seblock-%d-%d get_free_count not ready!\n", dst, channel);
                return -EIO;
        }

        blk_count = get_pool_free_count(se_chanl->ul_pool);
        blk_count += get_pool_free_count(seblock->ul_pub_pool);		/*total free sblocks in private pool and public pool*/

        return blk_count;
}

int seblock_register_notifier(uint8_t dst, uint8_t channel,
                              void (*handler)(int event, void *data), void *data)
{
        struct seblock_main_mgr *seblock = (struct seblock_main_mgr *)seblocks[dst];
        struct seblock_channel_mgr *se_chanl;

        if (!seblock) {
                SEBLK_ERR("seblock-%d-%d get_free_count seblock is not exist!\n", dst, channel);
                return -ENODEV;
        }

        se_chanl = seblock->channels[channel];

        if (!se_chanl) {
                SEBLK_ERR("seblock-%d-%d get_free_count se_chanl is not exist!\n", dst, channel);
                return -ENODEV;
        }


        se_chanl->handler = handler;
        se_chanl->data = data;

        return 0;
}


#if defined(CONFIG_DEBUG_FS)
static int seblock_debug_show(struct seq_file *m, void *private)
{
        struct seblock_main_mgr *seblock = NULL;
        struct seblock_channel_mgr *se_chanl = NULL;
        struct seblock_ring  *dl_ring = NULL;
        struct seblock_pool  *dl_pool = NULL;
        struct seblock_ring  *ul_ring = NULL;
        struct seblock_pool  *ul_pool = NULL;
        volatile struct seblock_fifo_info *fifo_info = NULL;
        int i, j;

        for (i = 0; i < SIPC_ID_NR; i++) {
                seblock = seblocks[i];
                if (!seblock) {
                        continue;
                }
                /*seblock_main_mgr*/
                seq_printf(m, "************************************************************************************************\n");
                seq_printf(m, "seblock dst 0x%0x, state: 0x%0x, recovery: %d, smem_virt: 0x%0x, smem_addr: 0x%0x, smem_size: 0x%0x, dl_fifo_size: %d, ul_fifo_size: %d, dl_blk_size: %d, ul_blk_size: %d\n",
                           seblock->dst, seblock->state, seblock->recovery,
                           (size_t)seblock->smem_virt, seblock->smem_addr,seblock->smem_size,
                           seblock->dl_fifo_size, seblock->ul_fifo_size,
                           seblock->dl_blk_size, seblock->ul_blk_size);
                seq_printf(m, "************************************************************************************************\n");

                for (j=0;  j < SMSG_CH_NR; j++) {
                        se_chanl = seblock->channels[j];
                        if(!se_chanl) {
                                continue;
                        }
                        dl_ring = se_chanl->dl_ring;
                        dl_pool = se_chanl->dl_pool;
                        ul_ring = se_chanl->ul_ring;
                        ul_pool = se_chanl->ul_pool;

                        /*seblock_channel_mgr*/
                        seq_printf(m, "se_channel dst 0x%0x, channel: 0x%0x, state: %d, smem_virt: 0x%0x, smem_addr: 0x%0x, smem_size: 0x%0x, dl_record: 0x%0x, ul_record: 0x%0x \n",
                                   se_chanl->dst, se_chanl->channel, se_chanl->state,
                                   (size_t)se_chanl->smem_virt, se_chanl->smem_addr,
                                   se_chanl->smem_size, se_chanl->dl_record, se_chanl->ul_record);


                        seq_printf(m, "se_channel dl_ring: fifo_buf_virt: 0x%0x, fifo_size :%d\n",
                                   (size_t)dl_ring->fifo_buf, (uint32_t)dl_ring->fifo_size);
                        fifo_info = dl_ring->fifo_info;
                        seq_printf(m, "dl_ring fifo: blks_addr :0x%0x, blk_size :%d, fifo_addr :0x%0x, fifo_size :%d, fifo_rdptr :0x%0x, fifo_wrptr: 0x%0x \n",
                                   (uint32_t)fifo_info->blks_addr, (uint32_t)fifo_info->blk_size,
                                   (uint32_t)fifo_info->fifo_addr, (uint32_t)fifo_info->fifo_size,
                                   (uint32_t)fifo_info->fifo_rdptr, (uint32_t)fifo_info->fifo_wrptr);

                        seq_printf(m, "se_channel ul_ring: fifo_buf_virt: 0x%0x, fifo_size :%d\n",
                                   (size_t)ul_ring->fifo_buf, (uint32_t)ul_ring->fifo_size);
                        fifo_info = ul_ring->fifo_info;
                        seq_printf(m, "ul_ring fifo: blks_addr :0x%0x, blk_size :%d, fifo_addr :0x%0x, fifo_size :%d, fifo_rdptr :0x%0x, fifo_wrptr: 0x%0x \n",
                                   (uint32_t)fifo_info->blks_addr, (uint32_t)fifo_info->blk_size,
                                   (uint32_t)fifo_info->fifo_addr, (uint32_t)fifo_info->fifo_size,
                                   (uint32_t)fifo_info->fifo_rdptr, (uint32_t)fifo_info->fifo_wrptr);

                        seq_printf(m, "se_channel dl_pool: fifo_buf_virt: 0x%0x, fifo_size :%d, blks_buf_virt: 0x%0x, blk_size :%d\n",
                                   (size_t)dl_pool->fifo_buf, (uint32_t)dl_pool->fifo_size, (size_t)dl_pool->blks_buf, (uint32_t)dl_pool->blk_size);
                        fifo_info = dl_pool->fifo_info;
                        seq_printf(m, "dl_pool fifo: blks_addr :0x%0x, blk_size :%d, fifo_addr :0x%0x, fifo_size :%d, fifo_rdptr :0x%0x, fifo_wrptr: 0x%0x \n",
                                   (uint32_t)fifo_info->blks_addr, (uint32_t)fifo_info->blk_size,
                                   (uint32_t)fifo_info->fifo_addr, (uint32_t)fifo_info->fifo_size,
                                   (uint32_t)fifo_info->fifo_rdptr, (uint32_t)fifo_info->fifo_wrptr);

                        seq_printf(m, "se_channel ul_pool: fifo_buf_virt: 0x%0x, fifo_size :%d, blks_buf_virt: 0x%0x, blk_size :%d\n",
                                   (size_t)ul_pool->fifo_buf, (uint32_t)ul_pool->fifo_size, (size_t)ul_pool->blks_buf, (uint32_t)ul_pool->blk_size);
                        fifo_info = ul_pool->fifo_info;
                        seq_printf(m, "ul_pool fifo: blks_addr :0x%0x, blk_size :%d, fifo_addr :0x%0x, fifo_size :%d, fifo_rdptr :0x%0x, fifo_wrptr: 0x%0x \n",
                                   (uint32_t)fifo_info->blks_addr, (uint32_t)fifo_info->blk_size,
                                   (uint32_t)fifo_info->fifo_addr, (uint32_t)fifo_info->fifo_size,
                                   (uint32_t)fifo_info->fifo_rdptr, (uint32_t)fifo_info->fifo_wrptr);

                        seq_printf(m, "************************************************************************************************\n\n");
                }
        }
        return 0;

}

static int seblock_debug_open(struct inode *inode, struct file *file)
{
        return single_open(file, seblock_debug_show, inode->i_private);
}

static const struct file_operations seblock_debug_fops = {
        .open = seblock_debug_open,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = single_release,
};

int  seblock_init_debugfs(void *root )
{
        if (!root)
                return -ENXIO;
        debugfs_create_file("seblock", S_IRUGO, (struct dentry *)root, NULL, &seblock_debug_fops);
        return 0;
}

#endif /* CONFIG_DEBUG_FS */

EXPORT_SYMBOL(seblock_put);
EXPORT_SYMBOL(seblock_create);
EXPORT_SYMBOL(seblock_destroy);
EXPORT_SYMBOL(seblock_register_notifier);
EXPORT_SYMBOL(seblock_get);
EXPORT_SYMBOL(seblock_send);
EXPORT_SYMBOL(seblock_receive);
EXPORT_SYMBOL(seblock_release);
EXPORT_SYMBOL(seblock_get_arrived_count);
EXPORT_SYMBOL(seblock_get_free_count);

MODULE_AUTHOR("Zeng Cheng, Wu Zhiwei");
MODULE_DESCRIPTION("SIPC/SEBLOCK driver");
MODULE_LICENSE("GPL");

