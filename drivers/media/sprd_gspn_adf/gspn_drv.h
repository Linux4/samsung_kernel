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

#ifndef _GSPN_DRV_H_
#define _GSPN_DRV_H_

#include <linux/init.h>// included <linux/types.h>
#include <linux/module.h>// included <linux/list.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>// kzalloc
#include <linux/delay.h>
//#include <linux/math64.h>
//#include <linux/types.h>
#include <linux/interrupt.h>//request irq
#include <linux/errno.h>
//#include <linux/irq.h>
#include <linux/kthread.h>//create_kthread
#include <linux/io.h>//for ioremap
#include <linux/pid.h>
#include <linux/clk.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/device.h>
#endif

#include <soc/sprd/hardware.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/globalregs.h> //define IRQ_GSP_INT
#include <soc/sprd/arch_misc.h>// get chip id
#include <video/ion_sprd.h>

#include <video/gspn_user_cfg.h>
#include "gspn_register_cfg.h"


#ifdef CONFIG_OF
#define GSPN_CLOCK_PARENT3      ("clk_gsp_parent")
#else // TODO:
#define GSPN_CLOCK_PARENT3      ("clk_256m")
#define GSPN_CLOCK_PARENT2      ("clk_192m")
#define GSPN_CLOCK_PARENT1      ("clk_153m6")
#define GSPN_CLOCK_PARENT0      ("clk_96m")
#endif
#define GSPN0_CLOCK_NAME         ("clk_gsp0")
#define GSPN1_CLOCK_NAME         ("clk_gsp1")
#define GSPN0_MMU_CLOCK_NAME         ("clk_gsp0_mmu_eb")
#define GSPN1_MMU_CLOCK_NAME         ("clk_gsp1_mmu_eb")
#define GSPN_MTX_CLOCK_NAME             ("clk_gsp_mtx_eb")
#define GSPN_NIU_CLOCK_NAME      ("clk_gsp_niu")
#define GSPN_DISP_CLOCK_NAME      ("clk_disp_eb")
#define GSPN_MMU_CLOCK_PARENT_NAME         ("clk_gsp_mmu_parent")
#define GSPN_MTX_CLOCK_PARENT_NAME      ("clk_gsp_mtx_parent")
#define GSPN_NIU_CLOCK_PARENT_NAME      ("clk_niu_parent")
#define GSPN_EMC_CLOCK_NAME      ("clk_aon_apb")

#define MEM_OPS_ADDR_ALIGN_MASK (0x7UL)

#define GSPN_CORE_MAX               2
#define GSPN_SPLIT_PARTS_MAX        GSPN_CORE_MAX

#define GSPN_KCMD_MAX               4 
#define GSPN_FENCE_MAX_PER_CMD      6
#define GSPN_CMDQ_ARRAY_MAX         GSPN_CMD_ARRAY_MAX

#define MIN_POOL_SIZE               (6 * 1024)
#define SCALER_COEF_TAB_LEN_HOR     48
#define SCALER_COEF_TAB_LEN_VER     68
#define GSPN_COEF_CACHE_MAX         32
#define GSC_COUNT                   64

#define GSPN_CMD_EXE_TIMEOUT        500// ms
#define GSPN_WAIT_FENCE_TIMEOUT     3000// ms

#define MS_TO_NS(ms)            ((ms)*1000000)
#define NS_TO_US(ns)            (((ns) + 500L) /1000L)
#define US_TO_JIFFIES(us)       msecs_to_jiffies(((us) + 999L) /1000L)
#define CORE_ID_VALID(id)   (0<=id && id < GSPN_CORE_MAX)

/*
power_on/resume -> GSPN_UNINITIALIZED --clock init--> GSPN_FREE
CMD come:GSPN_FREE --down sema--> GSPN_OCCUPIED --param set and trigger--> GSPN_BUSY --isr called--> GSPN_INT_COME --up sema--> GSPN_FREE
                                                                                                                        --hw param check failed, then up sema--> GSPN_FREE
                                                                                                                                           --timeout--> GSPN_EXCEPTION --recover and up sema--> GSPN_FREE
                                                                                                                                                           --if busy down and local int status is ok, up sema--> GSPN_FREE
when in GSPN_EXCEPTION, try to module reset and recover from exception
GSPN_FREE --clock disable--> GSPN_UNINITIALIZED -> suspend/power_off
*/
typedef enum {
    GSPN_CORE_UNINITIALIZED = 0,// power, clock is not config, local register is not set to default
    //the below status is initialized
    GSPN_CORE_EXCEPTION,// hw exception , busy lung up
    GSPN_CORE_FREE,
    GSPN_CORE_OCCUPIED,
    GSPN_CORE_CONFIGURED,
    GSPN_CORE_BUSY,// after trigger
    GSPN_CORE_INT_COME,
    GSPN_CORE_STATUS_MAX,
} GSPN_CORE_STATUS_E;

struct GSPN_CORE_T;

typedef struct { //GSPN_KCMD_INFO_T
    GSPN_CMD_INFO_T src_cmd;// copy from user
    GSPN_CMD_INFO_T __user *ucmd;
    struct GSPN_CORE_T *occupied_core;// if cmd not be split apart, which core it occupied?
    uint32_t done_flag;// if this kcmd really done be hw, set it to 1

    struct list_head list;// empty list, work list, not release list
    pid_t pid;// user process id, from which the cmd come

    //if the cmd can be split apart
    GSPN_CMD_INFO_T sub_cmd[GSPN_SPLIT_PARTS_MAX];// split src_cmd into a few part, each part exe on a single core
    struct GSPN_CORE_T* sub_cmd_occupied_core[GSPN_SPLIT_PARTS_MAX];// sub_cmd[n] binded core
    uint32_t sub_cmd_total;
    uint32_t sub_cmd_done_cnt;

    /*async/sync relative*/
	/*defined in ioctl,  init it to 0,
	 * pass addr of it to every KCMD,
	 * when all parts process over, add it 1*/
    uint32_t *done_cnt;
	/*if cmd[n] is sync, cmd list belong same frame,
	 * user thread return when all these cmds process over*/
    struct list_head list_frame;
    struct sync_fence *acq_fence[GSPN_FENCE_MAX_PER_CMD];/*wait fence before process*/
    uint32_t acq_fence_cnt;
    struct sync_fence *rls_fence;/*wait fence before process*/
	int tag;
	int status;/*0:idle, 1:filled, 2:ISR handled, 3:post*/
	int mmu_id;
    struct timespec start_time;/*start from trigger, used to timeout judgment*/
} GSPN_KCMD_INFO_T;


typedef struct GSPN_CORE_T {
    GSPN_CTL_REG_T *ctl_reg_base;// module ctl reg base, virtual
    uint32_t* clk_select_reg_base;// gspn clock source select ctl reg base
    uint32_t* auto_gate_reg_base;// gspn clock auto-gate/force-gate ctl reg base
    uint32_t* emc_reg_base;// gspn emc clock ctl reg base
    uint32_t* iommu_ctl_reg_base;// iommu ctl reg base
    uint32_t gspn_en_rst_bit;
    uint32_t mmu_en_rst_bit;
    uint32_t auto_gate_bit;
    uint32_t force_gate_bit;
    uint32_t emc_en_bit;
    uint32_t noc_auto_bit;
    uint32_t noc_force_bit;
    uint32_t mtx_auto_bit;
    uint32_t mtx_force_bit;
    uint32_t mtx_en_bit;
    uint32_t core_id;// core id, 0 1
    uint32_t interrupt_id;// global interrupt id
    GSPN_CORE_STATUS_E status;
    GSPN_KCMD_INFO_T *current_kcmd;// current processing cmd, for signal fence
    int32_t current_kcmd_sub_cmd_index;// if the kcmd have sub cmd, sub cmd index; if have no sub cmd, set to -1

    GSPN_CMD_REG_T *cmdq;//[GSPN_CMDQ_ARRAY_MAX];
    struct clk *gspn_clk;
    struct clk *mmu_clk;
    struct clk *mtx_clk;
    struct clk *niu_clk;
    struct clk *disp_clk;
    struct clk *gspn_clk_parent;
    struct clk *mmu_clk_parent;
    struct clk *mtx_clk_parent;
    struct clk *niu_clk_parent;
    struct clk *emc_clk;

	struct gspn_sync_timeline *timeline;
	struct semaphore fence_create_sema;
	/*protect kcmd_fifo relative access*/
    struct semaphore fill_list_sema;
	struct list_head fill_list;
    uint32_t fill_list_cnt;
} GSPN_CORE_T;



typedef struct {
    uint32_t* int_reg_base;// gspn interrupt reg base
    uint32_t* enable_reg_base;// gspn clock enable ctl reg base
    uint32_t* reset_reg_base;// gspn clock reset ctl reg base
    uint32_t* module_sel_reg_base;// sharkLT8 gsp/gspn select ctl reg base, bit[0] 0:gsp; 1:gspn
    uint32_t* chip_id_addr;// ap chip id addr
    GSPN_CORE_T *cores;// alloc in probe
    uint32_t core_cnt;// total core count
    uint32_t free_cnt;

    /*dissociation_list: not in empty-list or fill-list*/
    struct list_head dissociation_list;
} GSPN_CORE_MNG_T;


typedef struct {
    struct list_head list;

    uint16_t in_w;
    uint16_t in_h;
    uint16_t out_w;
    uint16_t out_h;
    uint16_t hor_tap;
    uint16_t ver_tap;
    uint32_t coef[32+16];
} COEF_ENTRY_T;


typedef struct { //GSPN_CONTEXT_T
    struct miscdevice dev;
    volatile uint32_t suspend_flag;// 0:resume; 1:suspend;
    volatile uint32_t log_level;// set from misc-write interface, GSPN_LOG_LEVEL_E define the bit mask.

    GSPN_CAPABILITY_T capability;

#ifdef CONFIG_HAS_EARLYSUSPEND
    struct early_suspend earlysuspend;
#endif
    struct semaphore suspend_clean_done_sema;// when suspend, drain all filled_list , wait cores done, then set this sema

    struct device    *gspn_of_dev;

    ulong gsp_coef_force_calc;
    uint32_t cache_coef_init_flag;
    int32_t coef_calc_buf[SCALER_COEF_TAB_LEN_HOR+SCALER_COEF_TAB_LEN_VER];
    char coef_buf_pool[MIN_POOL_SIZE];
    COEF_ENTRY_T coef_cache[GSPN_COEF_CACHE_MAX];

    struct list_head coef_list;

    GSPN_KCMD_INFO_T kcmds[GSPN_KCMD_MAX];// cmds memory
    struct list_head empty_list;// put to tail, get from head
    uint32_t empty_list_cnt;
    struct semaphore empty_list_sema;// protect kcmd_fifo relative access
    wait_queue_head_t empty_list_wq;//when empty_list_cnt increase, wake_up this wait queue.
    wait_queue_head_t sync_done_wq;//when sync KCMD done, wake_up this wait queue.



    GSPN_CORE_MNG_T core_mng;
    long long total_cmd;// for debug, record how many cmd have we process from boot

    struct task_struct *work_thread;
    struct semaphore wake_work_thread_sema;// wake up work_thread sema
    uint32_t timeout;// work thread wait event timeout, in us unit


    struct semaphore fence_create_sema;// signal fence will change timeline,
    volatile uint32_t remain_async_cmd_cnt;// async cmd count, used as sprd_fence_create() life_value

} GSPN_CONTEXT_T;

extern GSPN_CONTEXT_T *g_gspn_ctx;

#define GSPN_TAG    "GSPN"

/*tag printk*/
#define GSPN_PRINTK(fmt, ...)\
        printk(GSPN_TAG" %s[%d] " pr_fmt(fmt),__func__,__LINE__,##__VA_ARGS__)

/*normal info*/
#define GSPN_LOGI(fmt, ...)\
	do {\
		if(unlikely(g_gspn_ctx->log_level&(1<<(GSPN_LOG_INFO)))) {\
			pr_err(GSPN_TAG" %s[%d] " pr_fmt(fmt),__func__,__LINE__,##__VA_ARGS__);\
		} else {\
			pr_debug(GSPN_TAG" %s[%d] " pr_fmt(fmt),__func__,__LINE__,##__VA_ARGS__);\
		}\
	} while(0)

/*warning info*/
#define GSPN_LOGW(fmt, ...)\
    do {\
        if(unlikely(g_gspn_ctx->log_level&(1<<(GSPN_LOG_WARN)))) {\
            pr_err(GSPN_TAG" WARN %s[%d] " pr_fmt(fmt),__func__,__LINE__,##__VA_ARGS__);\
        } else {\
            pr_warn(GSPN_TAG" WARN %s[%d] " pr_fmt(fmt),__func__,__LINE__,##__VA_ARGS__);\
        }\
    } while(0)

/*err info*/
#define GSPN_LOGE(fmt, ...)\
	do {\
		if(unlikely(g_gspn_ctx->log_level&(1<<(GSPN_LOG_ERROR)))) {\
			pr_err(GSPN_TAG" ERR %s[%d] " pr_fmt(fmt),__func__,__LINE__,##__VA_ARGS__);\
		} else {\
			pr_err(GSPN_TAG" ERR %s[%d] " pr_fmt(fmt),__func__,__LINE__,##__VA_ARGS__);\
		}\
	} while(0)


/*performance info*/
#define GSPN_LOGP(fmt, ...)\
	do {\
		if(unlikely(g_gspn_ctx->log_level&(1<<(GSPN_LOG_PERF)))) {\
			pr_err(GSPN_TAG" %s[%d] " pr_fmt(fmt),__func__,__LINE__,##__VA_ARGS__);\
		} else {\
			pr_debug(GSPN_TAG" %s[%d] " pr_fmt(fmt),__func__,__LINE__,##__VA_ARGS__);\
		}\
	} while(0)

#define GSPN_PUT_FENCE_BY_FD(fd)    if((fd)>=0) {sync_fence_put(sync_fence_fdget(fd)); (fd) = -1;}
#define GSPN_PUT_CMD_ACQ_FENCE_BY_FD(cmd)\
                GSPN_PUT_FENCE_BY_FD((cmd)->l0_info.acq_fen_fd);\
                GSPN_PUT_FENCE_BY_FD((cmd)->l1_info.acq_fen_fd);\
                GSPN_PUT_FENCE_BY_FD((cmd)->l2_info.acq_fen_fd);\
                GSPN_PUT_FENCE_BY_FD((cmd)->l3_info.acq_fen_fd);\
                GSPN_PUT_FENCE_BY_FD((cmd)->des1_info.acq_fen_fd);\
                GSPN_PUT_FENCE_BY_FD((cmd)->des2_info.acq_fen_fd)

#define GSPN_ALL_CORE_FREE(gspnCtx)\
	((gspnCtx)->core_mng.core_cnt == (gspnCtx)->core_mng.free_cnt)

int gspn_work_thread(void *data);
GSPN_ERR_CODE_E gspn_put_list_to_empty_list(GSPN_CONTEXT_T *gspnCtx, struct list_head *KCMD_list);
uint32_t* gspn_gen_block_scaler_coef(GSPN_CONTEXT_T *gspnCtx,
                                     uint32_t i_w,
                                     uint32_t i_h,
                                     uint32_t o_w,
                                     uint32_t o_h,
                                     uint32_t hor_tap,
                                     uint32_t ver_tap);


int GSPN_InfoCfg( GSPN_KCMD_INFO_T *kcmd, int sub_idx);
int GSPN_TryToRecover(GSPN_CONTEXT_T *gspnCtx, uint32_t core_id);
int GSPN_Trigger(GSPN_CORE_MNG_T *core_mng,int32_t core_id);
int GSPN_CoreEnable(GSPN_CONTEXT_T *gspnCtx,int32_t core_id);
void GSPN_CoreDisable(GSPN_CORE_T *core);
void GSPN_DumpReg(GSPN_CORE_MNG_T *core_mng, uint32_t core_id);
irqreturn_t gspn_irq_handler_gsp0(int32_t irq, void *dev_id);
irqreturn_t gspn_irq_handler_gsp1(int32_t irq, void *dev_id);
void *gsp_memcpy(void *k_dst, void *k_src, size_t n);

#endif
