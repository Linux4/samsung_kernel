/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __ADSP_CORE_H__
#define __ADSP_CORE_H__

#include "adsp_helper.h"
#include "adsp_clk.h"
#include "adsp_reserved_mem.h"
#include "adsp_feature_define.h"

#define RELEASE_BUS_PROTECT 0
#define SET_BUS_PROTECT 1

enum ADSP_CORE_STATE {
	ADSP_RESET       = 0,
	ADSP_SUSPEND     = 1,
	ADSP_SLEEP       = 2,
	ADSP_RUNNING     = 3,
	ADSP_SUSPENDING  = 4,
};

enum adsp_smc_ops {
	MTK_ADSP_KERNEL_OP_INIT = 0,
	MTK_ADSP_KERNEL_OP_ENTER_LP,
	MTK_ADSP_KERNEL_OP_LEAVE_LP,
	MTK_ADSP_KERNEL_OP_SYS_CLEAR,
	MTK_ADSP_KERNEL_OP_CORE_START,
	MTK_ADSP_KERNEL_OP_CORE_STOP,
	MTK_ADSP_KERNEL_OP_CFG_LATCH,
	MTK_ADSP_KERNEL_OP_RELOAD,
	MTK_ADSP_KERNEL_OP_QUERY_STATE,
	MTK_ADSP_KERNEL_OP_DUMP_PWR_CLK,
	MTK_ADSP_KERNEL_OP_SET_SLP_PROT,
	MTK_ADSP_KERNEL_OP_NUM
};

struct adsp_priv;
struct adspsys_priv;

/* core api */
#define get_adsp_core_by_ptr(ptr)  _get_adsp_core(ptr, 0)
#define get_adsp_core_by_id(id)    _get_adsp_core(NULL, id)

struct adsp_priv *_get_adsp_core(void *ptr, int id);

void set_adsp_state(struct adsp_priv *pdata, int state);
int get_adsp_state(struct adsp_priv *pdata);
bool is_adsp_system_running(void);

void switch_adsp_power(bool on);
int adsp_reset(void);
void adsp_core_start(u32 cid);
void adsp_core_stop(u32 cid);
void adsp_latch_dump_region(bool en);

void register_adspsys(struct adspsys_priv *mt_adspsys);
void register_adsp_core(struct adsp_priv *pdata);

enum adsp_ipi_status adsp_push_message(enum adsp_ipi_id id, void *buf,
				       unsigned int len, unsigned int wait_ms,
				       unsigned int core_id);

int adsp_copy_to_sharedmem(struct adsp_priv *pdata, int id, const void *src, int count);
int adsp_copy_from_sharedmem(struct adsp_priv *pdata, int id, void *dst, int count);

void adsp_extern_notify_chain(enum ADSP_NOTIFY_EVENT event);
void adsp_pow_clk_dump(void);

/* wakelock */
int adsp_awake_lock(u32 cid);
int adsp_awake_unlock(u32 cid);

int adsp_pre_wake_lock(u32 cid);
int adsp_pre_wake_unlock(u32 cid);

extern void adsp_update_c2c_memory_info(struct adsp_priv *pdata);
#endif
