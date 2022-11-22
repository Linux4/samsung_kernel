/*
* Samsung debugging features
*
* Copyright (c) 2017 Samsung Electronics Co., Ltd.
*      http://www.samsung.com
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*/

#ifndef SEC_DEBUG_H
#define SEC_DEBUG_H

#include <linux/sizes.h>

#include "mt-plat/aee.h"

/* RESERVED MEMORY BASE ADDRESS */
//#define LK_RSV_ADDR		0x46C08000
//#define LK_RSV_SIZE		0x600000
#define SEC_LOG_BASE				0x4B000000    /* SZ_2M */
#define SEC_LASTKMSG_BASE			0x4B200000    /* SZ_2M */
#define SEC_LOGGER_BASE				0x4B400000    /* SZ_4M */
#define SEC_AUTO_COMMENT_BASE		0x4B800000    /* SZ_4K */
#define SEC_EXTRA_INFO_BASE			0x4B810000    /* SZ_4M - SZ_64K */

#ifdef CONFIG_SEC_DEBUG_INIT_LOG
#define SEC_INIT_LOG_BASE				0x4BA00000    /* SZ_2M */
#endif

/* +++ MediaTek Feature +++ */

#define PANIC_STRBUF_LEN	(256)
#define THERMAL_RESERVED_TZS (10)

struct mboot_params_buffer {
	uint32_t sig;
	/* for size comptible */
	uint32_t off_pl;
	uint32_t off_lpl;	/* last preloader: struct reboot_reason_pl */
	uint32_t sz_pl;
	uint32_t off_lk;
	uint32_t off_llk;	/* last lk: struct reboot_reason_lk */
	uint32_t sz_lk;
	uint32_t padding[3];
	uint32_t sz_buffer;
	uint32_t off_linux;	/* struct last_reboot_reason */
	uint32_t filling[4];
};

/*
 *  This group of API call by sub-driver module to report reboot reasons
 *  aee_rr_* stand for previous reboot reason
 */
struct last_reboot_reason {
	uint32_t fiq_step;
	/* 0xaeedeadX: X=1 (HWT), X=2 (KE), X=3 (nested panic) */
	uint32_t exp_type;
	uint64_t kaslr_offset;
	uint64_t oops_in_progress_addr;

	uint32_t last_irq_enter[AEE_MTK_CPU_NUMS];
	uint64_t jiffies_last_irq_enter[AEE_MTK_CPU_NUMS];

	uint32_t last_irq_exit[AEE_MTK_CPU_NUMS];
	uint64_t jiffies_last_irq_exit[AEE_MTK_CPU_NUMS];

	uint8_t hotplug_footprint[AEE_MTK_CPU_NUMS];
	uint8_t hotplug_cpu_event;
	uint8_t hotplug_cb_index;
	uint64_t hotplug_cb_fp;
	uint64_t hotplug_cb_times;
	uint64_t hps_cb_enter_times;
	uint32_t hps_cb_cpu_bitmask;
	uint32_t hps_cb_footprint;
	uint64_t hps_cb_fp_times;
	uint32_t cpu_caller;
	uint32_t cpu_callee;
	uint64_t cpu_up_prepare_ktime;
	uint64_t cpu_starting_ktime;
	uint64_t cpu_online_ktime;
	uint64_t cpu_down_prepare_ktime;
	uint64_t cpu_dying_ktime;
	uint64_t cpu_dead_ktime;
	uint64_t cpu_post_dead_ktime;

	uint32_t mcdi_wfi;
	uint32_t mcdi_r15;
	uint32_t deepidle_data;
	uint32_t sodi3_data;
	uint32_t sodi_data;
	uint32_t mcsodi_data;
	uint32_t spm_suspend_data;
	uint32_t spm_common_scenario_data;
	uint32_t mtk_cpuidle_footprint[AEE_MTK_CPU_NUMS];
	uint32_t mcdi_footprint[AEE_MTK_CPU_NUMS];
	uint32_t clk_data[8];
	uint32_t suspend_debug_flag;
	uint32_t fiq_cache_step;

	uint32_t vcore_dvfs_opp;
	uint32_t vcore_dvfs_status;

	uint32_t ppm_cluster_limit[8];
	uint8_t ppm_step;
	uint8_t ppm_cur_state;
	uint32_t ppm_min_pwr_bgt;
	uint32_t ppm_policy_mask;
	uint8_t ppm_waiting_for_pbm;

	uint8_t cpu_dvfs_vproc_big;
	uint8_t cpu_dvfs_vproc_little;
	uint8_t cpu_dvfs_oppidx;
	uint8_t cpu_dvfs_cci_oppidx;
	uint8_t cpu_dvfs_status;
	uint8_t cpu_dvfs_step;
	uint8_t cpu_dvfs_pbm_step;
	uint8_t cpu_dvfs_cb;
	uint8_t cpufreq_cb;

	uint8_t gpu_dvfs_vgpu;
	uint8_t gpu_dvfs_oppidx;
	uint8_t gpu_dvfs_status;

	uint32_t drcc_0;
	uint32_t drcc_1;
	uint32_t drcc_2;
	uint32_t drcc_3;
	uint32_t drcc_dbg_ret;
	uint32_t drcc_dbg_off;
	uint64_t drcc_dbg_ts;
	uint32_t ptp_devinfo_0;
	uint32_t ptp_devinfo_1;
	uint32_t ptp_devinfo_2;
	uint32_t ptp_devinfo_3;
	uint32_t ptp_devinfo_4;
	uint32_t ptp_devinfo_5;
	uint32_t ptp_devinfo_6;
	uint32_t ptp_devinfo_7;
	uint32_t ptp_e0;
	uint32_t ptp_e1;
	uint32_t ptp_e2;
	uint32_t ptp_e3;
	uint32_t ptp_e4;
	uint32_t ptp_e5;
	uint32_t ptp_e6;
	uint32_t ptp_e7;
	uint32_t ptp_e8;
	uint32_t ptp_e9;
	uint32_t ptp_e10;
	uint32_t ptp_e11;
	uint64_t ptp_vboot;
	uint64_t ptp_cpu_big_volt;
	uint64_t ptp_cpu_big_volt_1;
	uint64_t ptp_cpu_big_volt_2;
	uint64_t ptp_cpu_big_volt_3;
	uint64_t ptp_cpu_2_little_volt;
	uint64_t ptp_cpu_2_little_volt_1;
	uint64_t ptp_cpu_2_little_volt_2;
	uint64_t ptp_cpu_2_little_volt_3;
	uint64_t ptp_cpu_little_volt;
	uint64_t ptp_cpu_little_volt_1;
	uint64_t ptp_cpu_little_volt_2;
	uint64_t ptp_cpu_little_volt_3;
	uint64_t ptp_cpu_cci_volt;
	uint64_t ptp_cpu_cci_volt_1;
	uint64_t ptp_cpu_cci_volt_2;
	uint64_t ptp_cpu_cci_volt_3;
	uint64_t ptp_gpu_volt;
	uint64_t ptp_gpu_volt_1;
	uint64_t ptp_gpu_volt_2;
	uint64_t ptp_gpu_volt_3;
	uint64_t ptp_temp;
	uint8_t ptp_status;
	uint8_t eem_pi_offset;
	uint8_t etc_status;
	uint8_t etc_mode;


	int8_t thermal_temp[THERMAL_RESERVED_TZS];
	uint8_t thermal_status;
	uint8_t thermal_ATM_status;
	uint64_t thermal_ktime;

	uint32_t idvfs_ctrl_reg;
	uint32_t idvfs_enable_cnt;
	uint32_t idvfs_swreq_cnt;
	uint16_t idvfs_curr_volt;
	uint16_t idvfs_sram_ldo;
	uint16_t idvfs_swavg_curr_pct_x100;
	uint16_t idvfs_swreq_curr_pct_x100;
	uint16_t idvfs_swreq_next_pct_x100;
	uint8_t idvfs_state_manchine;

	uint32_t ocp_target_limit[4];
	uint8_t ocp_enable;
	uint32_t scp_pc;
	uint32_t scp_lr;
	unsigned long last_init_func;
	uint8_t pmic_ext_buck;
	uint32_t hang_detect_timeout_count;
	unsigned long last_async_func;
	unsigned long last_sync_func;
	uint32_t gz_irq;

	/* + SEC Feature + */
	uint32_t is_upload;
	uint32_t upload_reason;

	uint32_t is_power_reset;
	uint32_t power_reset_reason;
	char panic_str[PANIC_STRBUF_LEN];
	/* - SEC Feature - */
};

/* aee sram flags save */
#define RR_BASE(stage)	\
	((void *)mboot_params_buffer + mboot_params_buffer->off_##stage)
#define RR_LINUX ((struct last_reboot_reason *)RR_BASE(linux))
#define RR_BASE_PA(stage)	\
	((void *)mboot_params_buffer_pa + mboot_params_buffer->off_##stage)
#define RR_LINUX_PA ((struct last_reboot_reason *)RR_BASE_PA(linux))

/*NOTICE: You should check if mboot_params is null before call these macros*/
#define LAST_RR_SET(rr_item, value) (RR_LINUX->rr_item = value)

#define LAST_RR_SET_WITH_ID(rr_item, id, value) (RR_LINUX->rr_item[id] = value)

#define LAST_RR_VAL(rr_item)				\
	(mboot_params_buffer ? RR_LINUX->rr_item : 0)

#define LAST_RR_MEMCPY(rr_item, str, len)				\
	(strlcpy(RR_LINUX->rr_item, str, len))

#define LAST_RR_MEMCPY_WITH_ID(rr_item, id, str, len)			\
	(strlcpy(RR_LINUX->rr_item[id], str, len))

extern struct mboot_params_buffer *mboot_params_buffer;

extern void mrdump_reboot(void);
extern void wdt_arch_reset(char mode);
extern void mt_power_off(void);
extern int wd_dram_reserved_mode(bool enabled);
extern void __inner_flush_dcache_all(void);
/* --- MediaTek Feature --- */

enum sec_debug_upload_magic_t {
	UPLOAD_MAGIC_NORMAL	= 0x0,
	UPLOAD_MAGIC_UPLOAD	= 0x66262564,
};
enum sec_debug_upload_cause_t {
	UPLOAD_CAUSE_INIT		= 0xCAFEBABE,
	UPLOAD_CAUSE_KERNEL_PANIC	= 0x000000C8,
	UPLOAD_CAUSE_FORCED_UPLOAD	= 0x00000022,
	UPLOAD_CAUSE_USER_FORCED_UPLOAD	= 0x00000074,
	UPLOAD_CAUSE_CP_ERROR_FATAL	= 0x000000CC,
	UPLOAD_CAUSE_USER_FAULT		= 0x0000002F,
	UPLOAD_CAUSE_POWERKEY_LONG_PRESS = 0x00000085,
	UPLOAD_CAUSE_WATCHDOG = 0x00000066,
};

enum sec_power_flags {
	SEC_POWER_OFF = 0x0,
	SEC_POWER_RESET = 0x12345678,
};
#define SEC_RESET_REASON_PREFIX 0x12345670
#define SEC_RESET_SET_PREFIX    0xabc00000
enum sec_reset_reason {
	SEC_RESET_REASON_UNKNOWN   = (SEC_RESET_REASON_PREFIX | 0x0),
	SEC_RESET_REASON_DOWNLOAD  = (SEC_RESET_REASON_PREFIX | 0x1),
	SEC_RESET_REASON_UPLOAD    = (SEC_RESET_REASON_PREFIX | 0x2),
	SEC_RESET_REASON_CHARGING  = (SEC_RESET_REASON_PREFIX | 0x3),
	SEC_RESET_REASON_RECOVERY  = (SEC_RESET_REASON_PREFIX | 0x4),
	SEC_RESET_REASON_FOTA      = (SEC_RESET_REASON_PREFIX | 0x5),
	SEC_RESET_REASON_FOTA_BL   = (SEC_RESET_REASON_PREFIX | 0x6), /* update bootloader */
	SEC_RESET_REASON_SECURE    = (SEC_RESET_REASON_PREFIX | 0x7), /* image secure check fail */
	SEC_RESET_REASON_FWUP      = (SEC_RESET_REASON_PREFIX | 0x9), /* emergency firmware update */
	SEC_RESET_REASON_IN_OFFSEQ = (SEC_RESET_REASON_PREFIX | 0xA),
	#if defined(CONFIG_SEC_ABC)
	SEC_RESET_REASON_USER_DRAM_TEST   = (SEC_RESET_REASON_PREFIX | 0xB), /* nad user dram test */
	#endif
	SEC_RESET_REASON_EMERGENCY = 0x0,
	SEC_RESET_REASON_INIT 	   = 0xCAFEBABE,

	#ifdef CONFIG_SEC_DEBUG_MDM_SEPERATE_CRASH
	SEC_RESET_CP_DEBUG         = (SEC_RESET_SET_PREFIX | 0xc0000)
	#endif
	SEC_RESET_SET_DEBUG        = (SEC_RESET_SET_PREFIX | 0xd0000),
	SEC_RESET_SET_SWSEL        = (SEC_RESET_SET_PREFIX | 0xe0000),
	SEC_RESET_SET_SUD          = (SEC_RESET_SET_PREFIX | 0xf0000),
	SEC_RESET_CP_DBGMEM        = (SEC_RESET_SET_PREFIX | 0x50000), /* cpmem_on: CP RAM logging */

	#ifdef CONFIG_DIAG_MODE
	SEC_RESET_SET_DIAG         = (SEC_RESET_SET_PREFIX | 0xe)	/* Diag enable for CP */
	#endif
};

#define	_THIS_CPU	(-1)
#define LOG_MAGIC 	0x4d474f4c	/* "LOGM" */

/* enable/disable sec_debug feature
 * level = 0 when enable = 0 && enable_user = 0
 * level = 1 when enable = 1 && enable_user = 0
 * level = 0x10001 when enable = 1 && enable_user = 1
 * The other cases are not considered
 */
union sec_debug_level_t {
	struct {
		u16 kernel_fault;
		u16 user_fault;
	} en;
	u32 uint_val;
};

/* This defines are for PSTORE */
#define SEC_LOGGER_LEVEL_HEADER 	(1)
#define SEC_LOGGER_LEVEL_PREFIX 	(2)
#define SEC_LOGGER_LEVEL_TEXT		(3)
#define SEC_LOGGER_LEVEL_MAX		(4)
#define SEC_LOGGER_SKIP_COUNT		(4)
#define SEC_LOGGER_STRING_PAD		(1)
#define SEC_LOGGER_HEADER_SIZE		(68)

#define SEC_LOG_ID_MAIN 		(0)
#define SEC_LOG_ID_RADIO		(1)
#define SEC_LOG_ID_EVENTS		(2)
#define SEC_LOG_ID_SYSTEM		(3)
#define SEC_LOG_ID_CRASH		(4)
#define SEC_LOG_ID_KERNEL		(5)

typedef struct __attribute__((__packed__)) {
	uint8_t magic;
	uint16_t len;
	uint16_t uid;
	uint16_t pid;
} sec_pmsg_log_header_t;

typedef struct __attribute__((__packed__)) {
	unsigned char id;
	uint16_t tid;
	int32_t tv_sec;
	int32_t tv_nsec;
} sec_android_log_header_t;

typedef struct sec_logger {
	uint16_t	len;
	uint16_t	id;
	uint16_t	pid;
	uint16_t	tid;
	uint16_t	uid;
	uint16_t	level;
	int32_t		tv_sec;
	int32_t		tv_nsec;
	char		msg[0];
	char*		buffer;
	void		(*func_hook_logger)(const char*, size_t);
} __attribute__((__packed__)) sec_logger;

typedef struct {
	u64 regs[31];
	u64 sp_el1;
	u64 pc;
	u64 spsr_el1;
	u64 elr_el1;
	u64 esr_el1;
	u64 sp_el0;
	u64 pstate;
} sec_debug_core_reg_t;

typedef struct {
	long SCTLR_EL1;
	long TTBR0_EL1;
	long TTBR1_EL1;
	long TCR_EL1;
	long ESR_EL1;
	long FAR_EL1;
	long CONTEXTIDR_EL1;
	long TPIDR_EL0;
	long TPIDRRO_EL0;
	long TPIDR_EL1;
	long MAIR_EL1;
} sec_debug_mmu_reg_t;

enum sec_debug_reset_reason_t {
	RR_S = 1,
	RR_W = 2,		// not support in MTK, WTSR
	RR_D = 3,
	RR_K = 4,
	RR_M = 5,
	RR_P = 6,
	RR_R = 7,
	RR_B = 8,		// not support in MTK, Bootloader reset
	RR_N = 9,
	RR_T = 10,		// not support in MTK, Thermal reset
	RR_C = 11,
};

extern unsigned reset_reason;

#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO

#define WDT_FRAME	16
#define MAX_EXTRA_INFO_HDR_LEN	6
#define MAX_EXTRA_INFO_KEY_LEN	16
#define MAX_EXTRA_INFO_VAL_LEN	1024
#define SEC_DEBUG_BADMODE_MAGIC	0x6261646d

enum sec_debug_extra_buf_type {
	INFO_AID,
	INFO_KTIME,
	INFO_BIN,
	INFO_FAULT,
	INFO_BUG,
	INFO_PANIC,
	INFO_PC,
	INFO_LR,
	INFO_STACK,
	INFO_REASON,
	INFO_RSTCNT,
	INFO_QBI,
	INFO_DPM,
	INFO_SMPL,
	INFO_ETC,
	INFO_ESR,
	INFO_PCB,
	INFO_SMD,
	INFO_CHI,
	INFO_KLG,
	INFO_LEVEL,
	INFO_MAX_A,

	INFO_BID = INFO_MAX_A,
	INFO_MAX_B,

	INFO_CID = INFO_MAX_B,
	INFO_MAX_C,

	INFO_MID = INFO_MAX_C,
	INFO_MREASON,
	INFO_MFC,
	INFO_MAX_M,

	INFO_MAX = INFO_MAX_M,
};

struct sec_debug_extra_info_item {
	char key[MAX_EXTRA_INFO_KEY_LEN];
	char val[MAX_EXTRA_INFO_VAL_LEN];
	unsigned int max;
};

struct sec_debug_panic_extra_info {
	struct sec_debug_extra_info_item item[INFO_MAX];
};

#endif

/*
 * SEC DEBUG AUTO COMMENT
 */
#ifdef CONFIG_SEC_DEBUG_AUTO_COMMENT
#define SEC_DEBUG_AUTO_COMM_BUF_SIZE 10

struct sec_debug_auto_comm_buf {
	int reserved_0;
	int reserved_1;
	int reserved_2;
	unsigned int offset;
	char buf[SZ_4K];
};

struct sec_debug_auto_comment {
	int header_magic;
	int fault_flag;
	int lv5_log_cnt;
	u64 lv5_log_order;
	int order_map_cnt;
	int order_map[SEC_DEBUG_AUTO_COMM_BUF_SIZE];
	struct sec_debug_auto_comm_buf auto_comm_buf[SEC_DEBUG_AUTO_COMM_BUF_SIZE];

	int tail_magic;
};

#define AC_SIZE 0xf3c
#define AC_MAGIC 0xcafecafe
#define AC_TAIL_MAGIC 0x00c0ffee
#define AC_EDATA_MAGIC 0x43218765

enum {
	PRIO_LV0 = 0,
	PRIO_LV1,
	PRIO_LV2,
	PRIO_LV3,
	PRIO_LV4,
	PRIO_LV5,
	PRIO_LV6,
	PRIO_LV7,
	PRIO_LV8,
	PRIO_LV9
};

enum sec_debug_FREQ_INFO {
	FREQ_INFO_CLD0 = 0,
	FREQ_INFO_CLD1,
	FREQ_INFO_INT,
	FREQ_INFO_MIF,
	FREQ_INFO_MAX,
};

struct sec_debug_auto_comm_log_idx {
	atomic_t logging_entry;
	atomic_t logging_disable;
	atomic_t logging_count;
};

struct auto_comment_log_map {
	char prio_level;
	char max_count;
};

static const struct auto_comment_log_map init_data[SEC_DEBUG_AUTO_COMM_BUF_SIZE] = {
	{PRIO_LV0, 0},
	{PRIO_LV5, 8},
	{PRIO_LV9, 0},
	{PRIO_LV5, 0},
	{PRIO_LV5, 0},
	{PRIO_LV1, 7},
	{PRIO_LV2, 8},
	{PRIO_LV5, 0},
	{PRIO_LV5, 8},
	{PRIO_LV0, 0}
};

extern void sec_debug_auto_comment_log_disable(int type);
extern void sec_debug_auto_comment_log_once(int type);
extern void register_set_auto_comm_buf(void (*func)(int type, const char *buf, size_t size));
extern void register_set_auto_comm_lastfreq(void (*func)(int type,
						int old_freq, int new_freq, u64 time, int en));
#endif

#define SEC_DEBUG_SHARED_MAGIC0 0xFFFFFFFF
#define SEC_DEBUG_SHARED_MAGIC1 0x95308180
#define SEC_DEBUG_SHARED_MAGIC2 0x14F014F0
#define SEC_DEBUG_SHARED_MAGIC3 0x00010001

struct sec_debug_ksyms {
	uint32_t magic;
	uint32_t kallsyms_all;
	uint64_t addresses_pa;
	uint64_t names_pa;
	uint64_t num_syms;
	uint64_t token_table_pa;
	uint64_t token_index_pa;
	uint64_t markers_pa;
	uint64_t sinittext;
	uint64_t einittext;
	uint64_t stext;
	uint64_t etext;
	uint64_t end;
};

struct sec_debug_shared_info {
	/* initial magic code */	
	unsigned int magic[4];
	
#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
	/* reset reason extra info for bigdata */
	struct sec_debug_panic_extra_info sec_debug_extra_info;

	/* reset reason extra info for bigdata */
	struct sec_debug_panic_extra_info sec_debug_extra_info_backup;
#endif

	/* last 1KB of kernel log */
	char last_klog[SZ_1K];
};

#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
extern void sec_debug_init_extra_info(struct sec_debug_shared_info *, phys_addr_t sec_debug_extra_info_size, int magic_status);
extern void sec_debug_finish_extra_info(void);
extern void sec_debug_store_extra_info(int start, int end);
extern void sec_debug_store_extra_info_A(void);
extern void sec_debug_store_extra_info_B(void);
extern void sec_debug_store_extra_info_C(void);
extern void sec_debug_store_extra_info_M(void);
extern void sec_debug_set_extra_info_fault(unsigned long addr, struct pt_regs *regs);
extern void sec_debug_set_extra_info_bug(const char *file, unsigned int line);
extern void sec_debug_set_extra_info_bug_verbose(unsigned long addr);
extern void sec_debug_set_extra_info_panic(char *str);
extern void sec_debug_set_extra_info_backtrace(struct pt_regs *regs);
extern void sec_debug_set_extra_info_wdt_lastpc(unsigned long stackframe[][WDT_FRAME], unsigned int kick, unsigned int check);
extern void sec_debug_set_extra_info_dpm_timeout(char *devname);
extern void sec_debug_set_extra_info_smpl(unsigned int count);
extern void sec_debug_set_extra_info_esr(unsigned int esr);
extern void sec_debug_set_extra_info_zswap(char *str);
#else
#define sec_debug_init_extra_info(a, b, c)	do { } while (0)
#define sec_debug_finish_extra_info()	do { } while (0)
#define sec_debug_store_extra_info(a, b)	do { } while (0)
#define sec_debug_store_extra_info_A()		do { } while (0)
#define sec_debug_store_extra_info_B()		do { } while (0)
#define sec_debug_store_extra_info_C()		do { } while (0)
#define sec_debug_store_extra_info_M()		do { } while (0)
#define sec_debug_set_extra_info_wdt_lastpc(a, b, c)	do { } while(0)
#endif /* CONFIG_SEC_DEBUG_EXTRA_INFO */

int sec_save_context(int cpu, void *v_regs);
void sec_debug_save_core_reg(void *v_regs);
void sec_debug_save_mmu_reg(sec_debug_mmu_reg_t *mmu_reg);

#ifdef CONFIG_SEC_DEBUG
extern union sec_debug_level_t sec_debug_level;
#define SEC_DEBUG_LEVEL(x)	(sec_debug_level.en.x##_fault)
#else
#define SEC_DEBUG_LEVEL(x)	0
#endif
extern void sec_dump_task_info(void);
extern void sec_upload_cause(void *buf);
extern void sec_debug_check_crash_key(unsigned int code, int value);
extern void register_log_text_hook(void (*f)(char *text, size_t size));
extern void *persistent_ram_vmap(phys_addr_t start, size_t size, unsigned int memtype);
#ifdef CONFIG_SEC_LOG_HOOK_PMSG
extern int sec_log_hook_pmsg(char *buffer, size_t count);
#else
#define sec_log_hook_pmsg(a, b)			do { } while(0)
#endif

/* getlog support */
#ifdef CONFIG_SEC_DEBUG
extern void sec_getlog_supply_kernel(void *klog_buf);
extern void sec_getlog_supply_platform(unsigned char *buffer, const char *name);
extern void sec_gaf_supply_rqinfo(unsigned short curr_offset, unsigned short rq_offset);
#else
#define sec_getlog_supply_kernel(a)		do { } while(0)
#define sec_getlog_supply_platform(a, b)	do { } while(0)
#define sec_gaf_supply_rqinfo(a, b)		do { } while(0)
#endif

#ifdef CONFIG_SEC_DEBUG_MDM_SEPERATE_CRASH
extern int  sec_debug_is_enabled_for_ssr(void);
#endif

#ifdef CONFIG_MTK_DRAMC
//extern unsigned int get_mr5_dram_mode_reg(void);
#else
#define get_mr5_dram_mode_reg()		(0)
#endif

/* sec logging */
#ifdef CONFIG_SEC_AVC_LOG
extern void sec_debug_avc_log(char *fmt, ...);
#else
#define sec_debug_avc_log(a, ...)		do { } while(0)
#endif

#ifdef CONFIG_TOUCHSCREEN_DUMP_MODE
struct tsp_dump_callbacks {
	void (*inform_dump)(void);
};
#endif

#ifdef CONFIG_SEC_DEBUG_LAST_KMSG
extern void sec_debug_save_last_kmsg(unsigned char* head_ptr, unsigned char* curr_ptr);
#else
#define sec_debug_save_last_kmsg(a, b)		do { } while(0)
#endif

#endif /* SEC_DEBUG_H */
