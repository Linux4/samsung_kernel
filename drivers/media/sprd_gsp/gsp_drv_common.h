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

#include <linux/semaphore.h>
#include <linux/types.h>
#include <linux/miscdevice.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
//#include "gsp_drv.h"

#define GSP_MAX_USER                        8
#define GSP_ERR_RECORD_CNT  8

#define MEM_OPS_ADDR_ALIGN_MASK (0x7UL)

//#define GSP_DEBUG
#ifdef GSP_DEBUG
#define GSP_TRACE   printk
#else
#define GSP_TRACE   pr_debug
#endif


#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif


typedef struct _gsp_perf_data
{
    unsigned long gsp_perf_cnt ;
    unsigned long long gsp_perf_s ;
    unsigned long long gsp_perf_ns;
    unsigned long gsp_perf;
} gsp_perf_data_t;

#define INVALID_USER_ID 0xFFFFFFFF

typedef struct _gsp_user_data
{
    pid_t pid;
    uint32_t is_exit_force;// if the client process hungup on IOCTL-wait-finish, this variable mark it should wakeup and return anyway
    struct semaphore sem_open;// one thread can open device once per time
    void *priv;
} gsp_user;


typedef struct _gsp_context
{
    struct miscdevice           dev;

    uint32_t                    gsp_irq_num;
    struct clk                  *gsp_emc_clk;
    struct clk                  *gsp_clk;
    struct semaphore            gsp_hw_resource_sem;//cnt == 1,only one thread can access critical section at the same time
    struct semaphore            gsp_wait_interrupt_sem;// init to 0, gsp done/timeout/client discard release sem

    volatile uint32_t           suspend_resume_flag;
#ifdef CONFIG_HAS_EARLYSUSPEND
    struct early_suspend            earlysuspend;
#endif

    uint32_t                    cache_coef_init_flag;
    ulong                       gsp_coef_force_calc;

    gsp_user                    gsp_user_array[GSP_MAX_USER];
    volatile pid_t              gsp_cur_client_pid;
    GSP_CONFIG_INFO_T       gsp_cfg;//protect by gsp_hw_resource_sem

    GSP_REG_T gsp_reg_err_record[GSP_ERR_RECORD_CNT];
    uint32_t gsp_reg_err_record_rp;
    uint32_t gsp_reg_err_record_wp;

} gsp_context_t;


//extern gsp_context_t *gsp_ctx;
#ifdef CONFIG_OF
extern ulong g_gsp_base_addr ;
extern ulong g_gsp_mmu_ctrl_addr ;
#endif

/*
uint32_t get_gsp_base_addr(gsp_drv_data_t *gspDrvdata)
{
        return gspDrvdata->gsp_base_addr;
}
*/
#endif
