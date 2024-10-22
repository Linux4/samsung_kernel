/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#ifndef __MBOOT_PARAMS_H__
#define __MBOOT_PARAMS_H__

#include <linux/console.h>
#include <linux/pstore.h>

enum AEE_FIQ_STEP_NUM {
	AEE_FIQ_STEP_COMMON_DIE_START = 64,
	AEE_FIQ_STEP_COMMON_DIE_LOCK,
	AEE_FIQ_STEP_COMMON_DIE_KASLR,
	AEE_FIQ_STEP_COMMON_DIE_SCP,
	AEE_FIQ_STEP_COMMON_DIE_TRACE,
	AEE_FIQ_STEP_COMMON_DIE_EMISC,
	AEE_FIQ_STEP_COMMON_DIE_CS,
	AEE_FIQ_STEP_COMMON_DIE_DONE
};

enum AEE_EXP_TYPE_NUM {
	AEE_EXP_TYPE_HWT = 1,
	AEE_EXP_TYPE_KE = 2,
	AEE_EXP_TYPE_NESTED_PANIC = 3,
	AEE_EXP_TYPE_SMART_RESET = 4,
	AEE_EXP_TYPE_HANG_DETECT = 5,
	AEE_EXP_TYPE_BL33_CRASH = 6,
	AEE_EXP_TYPE_BL2_EXT_CRASH = 7,
	AEE_EXP_TYPE_AEE_LK_CRASH = 8,
	AEE_EXP_TYPE_TFA_CRASH = 9,
	AEE_EXP_TYPE_PL_CRASH = 10,
	AEE_EXP_TYPE_GZ_CRASH = 11,
	AEE_EXP_TYPE_MAX_NUM = 16,
};

#if IS_ENABLED(CONFIG_MTK_AEE_UT)
extern void init_register_callback(void (* fn)(const char *str));
extern void shutdown_register_callback(void (* fn)(const char *str));
#endif

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
extern void aee_rr_rec_clk(int id, u32 val);
extern void aee_rr_rec_exp_type(unsigned int type);
extern unsigned int aee_rr_curr_exp_type(void);
extern void aee_rr_rec_kick(uint32_t kick_bit);
extern void aee_rr_rec_check(uint32_t check_bit);
extern void aee_rr_rec_kaslr_offset(u64 value64);
extern void aee_rr_rec_ptp_vboot(u64 val);
extern void aee_rr_rec_ptp_gpu_volt(u64 val);
extern void aee_rr_rec_ptp_gpu_volt_1(u64 val);
extern void aee_rr_rec_ptp_gpu_volt_2(u64 val);
extern void aee_rr_rec_ptp_gpu_volt_3(u64 val);
extern void aee_rr_rec_ptp_temp(u64 val);
extern void aee_rr_rec_ptp_status(u8 val);
extern u64 aee_rr_curr_ptp_gpu_volt(void);
extern u64 aee_rr_curr_ptp_gpu_volt_1(void);
extern u64 aee_rr_curr_ptp_gpu_volt_2(void);
extern u64 aee_rr_curr_ptp_gpu_volt_3(void);
extern u64 aee_rr_curr_ptp_temp(void);
extern u8 aee_rr_curr_ptp_status(void);
extern unsigned long *aee_rr_rec_mtk_cpuidle_footprint_va(void);
extern unsigned long *aee_rr_rec_mtk_cpuidle_footprint_pa(void);
extern void aee_rr_rec_mcdi_val(int id, u32 val);
extern void aee_rr_rec_gpu_dvfs_vgpu(u8 val);
extern u8 aee_rr_curr_gpu_dvfs_vgpu(void);
extern void aee_rr_rec_gpu_dvfs_oppidx(u8 val);
extern void aee_rr_rec_gpu_dvfs_status(u8 val);
extern u8 aee_rr_curr_gpu_dvfs_status(void);
extern void aee_rr_rec_gpu_dvfs_power_count(int val);
extern void aee_rr_rec_hang_detect_timeout_count(unsigned int val);
extern int aee_rr_curr_fiq_step(void);
extern void aee_rr_rec_fiq_step(u8 i);
extern void aee_rr_rec_last_init_func(unsigned long val);
extern void aee_rr_rec_last_init_func_name(const char *str);
extern void aee_rr_rec_last_shutdown_device(const char *str);
extern void aee_sram_fiq_log(const char *msg);
extern void aee_rr_rec_wdk_ktime(u64 val);
extern void aee_rr_rec_wdk_systimer_cnt(u64 val);

#else
static inline void aee_rr_rec_clk(int id, u32 val)
{
}

static inline void aee_rr_rec_exp_type(unsigned int type)
{
}

static inline unsigned int aee_rr_curr_exp_type(void)
{
	return 0;
}

static inline void aee_rr_rec_kick(uint32_t kick_bit)
{
}

static inline void aee_rr_rec_check(uint32_t check_bit)
{
}

static inline void aee_rr_rec_kaslr_offset(u64 value64)
{
}

static inline void aee_rr_rec_ptp_vboot(u64 val)
{
}

static inline void aee_rr_rec_ptp_gpu_volt(u64 val)
{
}

static inline void aee_rr_rec_ptp_gpu_volt_1(u64 val)
{
}

static inline void aee_rr_rec_ptp_gpu_volt_2(u64 val)
{
}

static inline void aee_rr_rec_ptp_gpu_volt_3(u64 val)
{
}

static inline void aee_rr_rec_ptp_temp(u64 val)
{
}

static inline void aee_rr_rec_ptp_status(u8 val)
{
}

static inline u64 aee_rr_curr_ptp_gpu_volt(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_gpu_volt_1(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_gpu_volt_2(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_gpu_volt_3(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_temp(void)
{
	return 0;
}

static inline u8 aee_rr_curr_ptp_status(void)
{
	return 0;
}

static inline unsigned long *aee_rr_rec_mtk_cpuidle_footprint_va(void)
{
	return NULL;
}

static inline unsigned long *aee_rr_rec_mtk_cpuidle_footprint_pa(void)
{
	return NULL;
}

static inline void aee_rr_rec_mcdi_val(int id, u32 val)
{
}

static inline void aee_rr_rec_gpu_dvfs_vgpu(u8 val)
{
}

static inline u8 aee_rr_curr_gpu_dvfs_vgpu(void)
{
	return 0;
}

static inline void aee_rr_rec_gpu_dvfs_oppidx(u8 val)
{
}

static inline void aee_rr_rec_gpu_dvfs_status(u8 val)
{
}

static inline u8 aee_rr_curr_gpu_dvfs_status(void)
{
	return 0;
}

static inline void aee_rr_rec_gpu_dvfs_power_count(int val)
{
}

static inline void aee_rr_rec_hang_detect_timeout_count(unsigned int val)
{
}

static inline int aee_rr_curr_fiq_step(void)
{
	return 0;
}

static inline void aee_rr_rec_fiq_step(u8 i)
{
}

static inline void aee_rr_rec_last_init_func(unsigned long val)
{
}

static inline void aee_rr_rec_last_init_func_name(const char *str)
{
}

static inline void aee_rr_rec_last_shutdown_device(const char *str)
{
}

static inline void aee_sram_fiq_log(const char *msg)
{
}

static inline void aee_rr_rec_wdk_ktime(u64 val)
{
}

static inline void aee_rr_rec_wdk_systimer_cnt(u64 val)
{
}

#endif /* CONFIG_MTK_AEE_IPANIC */

#endif
