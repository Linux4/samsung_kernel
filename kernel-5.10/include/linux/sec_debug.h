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

#if IS_ENABLED(CONFIG_SEC_DUMP_SINK)	
#define SEC_DUMPSINK_MASK 0x0000FFFF
#endif

extern int wd_dram_reserved_mode(bool enabled);
extern void __inner_flush_dcache_all(void);

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

#if IS_ENABLED(CONFIG_SEC_DUMP_SINK)	
enum sec_reboot_magic_flags {
	MAGIC_SDR_FOR_MINFORM = 0x3,
	MAGIC_STR_FOR_MINFORM = 0xC,
};
#endif

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
	#if IS_ENABLED(CONFIG_SEC_ABC)
	SEC_RESET_REASON_USER_DRAM_TEST   = (SEC_RESET_REASON_PREFIX | 0xB), /* nad user dram test */
	#endif
	SEC_RESET_REASON_BOOTLOADER  = (SEC_RESET_REASON_PREFIX | 0xd),
	SEC_RESET_REASON_EMERGENCY = 0x0,
	SEC_RESET_REASON_INIT 	   = 0xCAFEBABE,

	#if IS_ENABLED(CONFIG_SEC_DEBUG_MDM_SEPERATE_CRASH)
	SEC_RESET_CP_DEBUG         = (SEC_RESET_SET_PREFIX | 0xc0000)
	#endif
	SEC_RESET_SET_DEBUG        = (SEC_RESET_SET_PREFIX | 0xd0000),
	SEC_RESET_SET_SWSEL        = (SEC_RESET_SET_PREFIX | 0xe0000),
	SEC_RESET_SET_SUD          = (SEC_RESET_SET_PREFIX | 0xf0000),
	SEC_RESET_CP_DBGMEM        = (SEC_RESET_SET_PREFIX | 0x50000), /* cpmem_on: CP RAM logging */

	#if IS_ENABLED(CONFIG_DIAG_MODE)
	SEC_RESET_SET_DIAG         = (SEC_RESET_SET_PREFIX | 0xe)	/* Diag enable for CP */
	#endif

#if IS_ENABLED(CONFIG_SEC_DUMP_SINK)	
	SEC_RESET_SET_DUMPSINK     = (SEC_RESET_SET_PREFIX | 0x80000),	/* dumpsink */
#endif
};

extern void secdbg_set_upload_magic(uint32_t upload_magic);
extern void secdbg_set_power_flag(uint32_t power_flag);
extern void secdbg_set_upload_reason(uint32_t upload_reason);
extern void secdbg_set_power_reset_reason(uint32_t reset_reason);
extern void secdbg_set_panic_string(void* buf);

#define	_THIS_CPU	(-1)
#define LOG_MAGIC 	0x4d474f4c	/* "LOGM" */

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

extern unsigned int reset_reason;

#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)

#define WDT_FRAME	16
#define MAX_EXTRA_INFO_HDR_LEN	6
#define MAX_EXTRA_INFO_KEY_LEN	16
#define MAX_EXTRA_INFO_VAL_LEN	1002
#define SEC_DEBUG_BADMODE_MAGIC	0x6261646d

enum sec_debug_extra_buf_type {
	INFO_AID,
	INFO_KTIME,
	INFO_QBI,
	INFO_LEVEL,
	INFO_BIN,
	INFO_RSTCNT,
	INFO_REASON,
	INFO_PC,
	INFO_LR,
	INFO_FAULT,
	INFO_BUG,
	INFO_PANIC,
	INFO_STACK,
	INFO_SMPL,
	INFO_ETC,
	INFO_KLG,
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

#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
struct sec_debug_shared_info {
	/* initial magic code */	
	unsigned int magic[4];
	
	/* reset reason extra info for bigdata */
	struct sec_debug_panic_extra_info sec_debug_extra_info;

	/* reset reason extra info for bigdata */
	struct sec_debug_panic_extra_info sec_debug_extra_info_backup;
};

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
extern void sec_debug_set_extra_info_smpl(unsigned int count);
extern void sec_debug_set_extra_info_zswap(char *str);
extern void sec_debug_set_extra_info_upload(char *str);
#define secdbg_exin_set_batt(a, b, c, d)	do { } while (0)
#endif /* CONFIG_SEC_DEBUG_EXTRA_INFO */

int sec_save_context(int cpu, void *v_regs);
void sec_debug_save_core_reg(void *v_regs);
void sec_debug_save_mmu_reg(sec_debug_mmu_reg_t *mmu_reg);

#if IS_ENABLED(CONFIG_SEC_DEBUG)
extern bool is_debug_level_low(void);
extern void sec_debug_dump_info(struct pt_regs *regs);
extern void sec_upload_cause(void *buf);
#endif

#endif /* SEC_DEBUG_H */
