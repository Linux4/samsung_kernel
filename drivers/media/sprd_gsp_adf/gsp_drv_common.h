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
#ifndef __GSP_DRV_COMMON_H__
#define __GSP_DRV_COMMON_H__

#include <asm/div64.h>
#include <linux/semaphore.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <video/gsp_types_shark_adf.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/math64.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include "scaler_coef_cal.h"
#define GSP_ERR_RECORD_CNT  8
#define GSP_COEF_CACHE_MAX         32
#define GSP_CFG_EXE_TIMEOUT        500/*ms*/

#define MEM_OPS_ADDR_ALIGN_MASK (0x7UL)

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#define MS_TO_NS(ms)            ((ms)*1000000)
#define NS_TO_US(ns)			(div_u64(ns+500L, 1000))
#define US_TO_JIFFIES(us)       msecs_to_jiffies(((us) + 999)/1000)
#define GSP_TAG "[GSP] "
#define GSP_ERR(fmt, ...) \
		pr_err(GSP_TAG " <err> " pr_fmt(fmt), ##__VA_ARGS__)
#define GSP_DEBUG(fmt, ...) \
        pr_debug(GSP_TAG " <debug> " pr_fmt(fmt), ##__VA_ARGS__)

#define GSP_GAP 0
#define GSP_CLOCK 3
#define GSP_CONTEXT_NAME "gsp-ctx"

enum {
	GSP_STATUS_IDLE = 0,
	GSP_STATUS_BUSY,
	GSP_STATUS_ISR_DONE,
	GSP_STATUS_ERR
};

struct gsp_context
{
    struct miscdevice           dev;

    uint32_t                    gsp_irq_num;
    struct clk                  *gsp_emc_clk;
    struct clk                  *gsp_clk;
    struct semaphore            wake_sema;
    struct task_struct         *work_thread;
    char name[32];
    int suspend_flag;
#ifdef CONFIG_HAS_EARLYSUSPEND
    struct early_suspend            earlysuspend;
#endif
    struct gsp_sync_timeline    *timeline;
    wait_queue_head_t sync_wait;/*used at sync case*/
    /*work queue which manages empty list,fill list,sep_list*/
    struct gsp_work_queue       *wq;

    int                    cache_coef_init_flag;
    unsigned long                       gsp_coef_force_calc;
    struct coef_entry coef_cache[GSP_COEF_CACHE_MAX];
    struct list_head coef_list;

	struct gsp_capability     cap;
	unsigned long gsp_reg_addr;
	unsigned long mmu_reg_addr;
	struct gsp_kcfg_info *current_kcfg;
	struct semaphore suspend_clean_done_sema;

	int status;/*0:idle, 1:busy, 2:err*/
	uint32_t frame_id;
};

int gsp_work_thread(void *data);

int gsp_status_get(struct gsp_context *ctx);

void gsp_status_set(struct gsp_context *ctx, int stat);
#endif
