/*
* Samsung debugging features
*
* Copyright (c) 2016 Samsung Electronics Co., Ltd.
*      http://www.samsung.com
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*/

#ifndef SEC_DEBUG_H
#define SEC_DEBUG_H

/* +++ MediaTek Feature +++ */
#include <mach/upmu_hw.h>
#include <linux/sched.h> /* TASK_COMM_LEN */

#define MT6737T_SEC_SRAM_ADDR	(0x0010FFF0)
#define MT6737T_SEC_SRAM_SIZE	(0x10)
#define PANIC_STRBUF_LEN	(256)

struct ram_console_buffer {
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
	uint32_t off_console;

	/* console buffer */
	uint32_t log_start;
	uint32_t log_size;
	uint32_t sz_console;
};

/*
   This group of API call by sub-driver module to report reboot reasons
   aee_rr_* stand for previous reboot reason
 */
struct last_reboot_reason {
	/* + SEC Feature + */
	uint32_t is_upload;
	uint32_t upload_reason;

	uint32_t is_power_reset;
	uint32_t power_reset_reason;
	char panic_str[PANIC_STRBUF_LEN];
	/* - SEC Feature - */
	uint32_t fiq_step;
	uint32_t exp_type;	/* 0xaeedeadX: X=1 (HWT), X=2 (KE), X=3 (nested panic) */
	uint32_t reboot_mode;

	uint32_t last_irq_enter[NR_CPUS];
	uint64_t jiffies_last_irq_enter[NR_CPUS];

	uint32_t last_irq_exit[NR_CPUS];
	uint64_t jiffies_last_irq_exit[NR_CPUS];

	uint64_t jiffies_last_sched[NR_CPUS];
	char last_sched_comm[NR_CPUS][TASK_COMM_LEN];

	uint8_t hotplug_data1[NR_CPUS];
	uint8_t hotplug_data2;
	uint64_t hotplug_data3;

	uint32_t mcdi_wfi;
	uint32_t mcdi_r15;
	uint32_t deepidle_data;
	uint32_t sodi3_data;
	uint32_t sodi_data;
	uint32_t spm_suspend_data;
	uint64_t cpu_dormant[NR_CPUS];
	uint32_t clk_data[8];
	uint32_t suspend_debug_flag;

	uint32_t vcore_dvfs_opp;
	uint32_t vcore_dvfs_status;

	uint8_t cpu_dvfs_vproc_big;
	uint8_t cpu_dvfs_vproc_little;
	uint8_t cpu_dvfs_oppidx;
	uint8_t cpu_dvfs_status;

	uint8_t gpu_dvfs_vgpu;
	uint8_t gpu_dvfs_oppidx;
	uint8_t gpu_dvfs_status;

	uint64_t ptp_cpu_big_volt;
	uint64_t ptp_cpu_little_volt;
	uint64_t ptp_gpu_volt;
	uint64_t ptp_temp;
	uint8_t ptp_status;


	uint8_t thermal_temp1;
	uint8_t thermal_temp2;
	uint8_t thermal_temp3;
	uint8_t thermal_temp4;
	uint8_t thermal_temp5;
	uint8_t thermal_status;

	uint8_t isr_el1;

	void *kparams;
};

struct s_last_reboot_reason {
	/* + SEC Feature + */
	uint32_t is_upload;
	uint32_t upload_reason;

	uint32_t is_power_reset;
	uint32_t power_reset_reason;
	/* - SEC Feature - */
};

/* aee sram flags save */
#define RR_BASE(stage) ((void *)ram_console_buffer + ram_console_buffer->off_##stage)
#define RR_LINUX ((struct last_reboot_reason *)RR_BASE(linux))
#define RR_BASE_PA(stage) ((void *)ram_console_buffer_pa + ram_console_buffer->off_##stage)
#define RR_LINUX_PA ((struct last_reboot_reason *)RR_BASE_PA(linux))

/*NOTICE: You should check if ram_console is null before call these macros*/
#define LAST_RR_SET(rr_item, value) (RR_LINUX->rr_item = value)
#define LAST_RR_SET_WITH_ID(rr_item, id, value) (RR_LINUX->rr_item[id] = value)
#define LAST_RR_VAL(rr_item)				\
	(ram_console_buffer ? RR_LINUX->rr_item : 0)
#define LAST_RR_MEMCPY(rr_item, str, len)				\
	(strlcpy(RR_LINUX->rr_item, str, len))
#define LAST_RR_MEMCPY_WITH_ID(rr_item, id, str, len)			\
	(strlcpy(RR_LINUX->rr_item[id], str, len))
#define SEC_LAST_RR_SET(rr_item, value)			\
	do {						\
		LAST_RR_SET(rr_item, value);		\
		s_reboot_reason_buffer->rr_item = value;	\
	} while (0)

extern struct ram_console_buffer *ram_console_buffer;
extern struct s_last_reboot_reason *s_reboot_reason_buffer;
extern unsigned short pmic_get_register_value(PMU_FLAGS_LIST_ENUM flagname);
extern unsigned short pmic_get_register_value_nolock(PMU_FLAGS_LIST_ENUM flagname);
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
	UPLOAD_CAUSE_HSIC_DISCONNECTED	= 0x000000DD,
	UPLOAD_CAUSE_POWERKEY_LONG_PRESS = 0x00000085,
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
	SEC_RESET_REASON_EMERGENCY = 0x0,
	SEC_RESET_REASON_INIT 	   = 0xCAFEBABE,

	#ifdef CONFIG_SEC_DEBUG_MDM_SEPERATE_CRASH
	SEC_RESET_CP_DEBUG         = (SEC_RESET_SET_PREFIX | 0xc0000)
	#endif
	SEC_RESET_SET_DEBUG        = (SEC_RESET_SET_PREFIX | 0xd0000),
	SEC_RESET_SET_SWSEL        = (SEC_RESET_SET_PREFIX | 0xe0000),
	SEC_RESET_SET_SUD          = (SEC_RESET_SET_PREFIX | 0xf0000),

	#ifdef CONFIG_DIAG_MODE
	SEC_RESET_SET_DIAG         = (SEC_RESET_SET_PREFIX | 0xe)	/* Diag enable for CP */
	#endif
};

#define LOG_MAGIC 		0x4d474f4c	/* "LOGM" */

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

struct sec_debug_mmu_reg_t {
	int SCTLR;
	int TTBR0;
	int TTBR1;
	int TTBCR;
	int DACR;
	int DFSR;
	int DFAR;
	int IFSR;
	int IFAR;
	int DAFSR;
	int IAFSR;
	int PMRRR;
	int NMRRR;
	int FCSEPID;
	int CONTEXT;
	int URWTPID;
	int UROTPID;
	int POTPIDR;
};

struct sec_debug_core_t {
	/* COMMON */
	unsigned int r0;
	unsigned int r1;
	unsigned int r2;
	unsigned int r3;
	unsigned int r4;
	unsigned int r5;
	unsigned int r6;
	unsigned int r7;
	unsigned int r8;
	unsigned int r9;
	unsigned int r10;
	unsigned int r11;
	unsigned int r12;
	/* SVC */
	unsigned int r13_svc;
	unsigned int r14_svc;
	unsigned int spsr_svc;
	/* PC & CPSR */
	unsigned int pc;
	unsigned int cpsr;
	/* USR/SYS */
	unsigned int r13_usr;
	unsigned int r14_usr;
	/* FIQ */
	unsigned int r8_fiq;
	unsigned int r9_fiq;
	unsigned int r10_fiq;
	unsigned int r11_fiq;
	unsigned int r12_fiq;
	unsigned int r13_fiq;
	unsigned int r14_fiq;
	unsigned int spsr_fiq;
	/* IRQ */
	unsigned int r13_irq;
	unsigned int r14_irq;
	unsigned int spsr_irq;
	/* MON */
	unsigned int r13_mon;
	unsigned int r14_mon;
	unsigned int spsr_mon;
	/* ABT */
	unsigned int r13_abt;
	unsigned int r14_abt;
	unsigned int spsr_abt;
	/* UNDEF */
	unsigned int r13_und;
	unsigned int r14_und;
	unsigned int spsr_und;
};

static inline void sec_debug_save_core_reg(struct sec_debug_core_t *core_reg)
{
	asm (
		"str r0, [%0,#0]\n\t"		/* R0 is pushed first to core_reg */
		"mov r0, %0\n\t"		/* R0 will be alias for core_reg */
		"str r1, [r0,#4]\n\t"		/* R1 */
		"str r2, [r0,#8]\n\t"		/* R2 */
		"str r3, [r0,#12]\n\t"		/* R3 */
		"str r4, [r0,#16]\n\t"		/* R4 */
		"str r5, [r0,#20]\n\t"		/* R5 */
		"str r6, [r0,#24]\n\t"		/* R6 */
		"str r7, [r0,#28]\n\t"		/* R7 */
		"str r8, [r0,#32]\n\t"		/* R8 */
		"str r9, [r0,#36]\n\t"		/* R9 */
		"str r10, [r0,#40]\n\t"		/* R10 */
		"str r11, [r0,#44]\n\t"		/* R11 */
		"str r12, [r0,#48]\n\t"		/* R12 */
		/* SVC */
		"str r13, [r0,#52]\n\t"		/* R13_SVC */
		"str r14, [r0,#56]\n\t"		/* R14_SVC */
		"mrs r1, spsr\n\t"		/* SPSR_SVC */
		"str r1, [r0,#60]\n\t"
		/* PC and CPSR */
		"sub r1, r15, #0x4\n\t"		/* PC */
		"str r1, [r0,#64]\n\t"
		"mrs r1, cpsr\n\t"		/* CPSR */
		"str r1, [r0,#68]\n\t"
		/* SYS/USR */
		"mrs r1, cpsr\n\t"		/* switch to SYS mode */
		"and r1, r1, #0xFFFFFFE0\n\t"
		"orr r1, r1, #0x1f\n\t"
		"msr cpsr,r1\n\t"
		"str r13, [r0,#72]\n\t"		/* R13_USR */
		"str r14, [r0,#76]\n\t"		/* R14_USR */
		/* FIQ */
		"mrs r1, cpsr\n\t"		/* switch to FIQ mode */
		"and r1,r1,#0xFFFFFFE0\n\t"
		"orr r1,r1,#0x11\n\t"
		"msr cpsr,r1\n\t"
		"str r8, [r0,#80]\n\t"		/* R8_FIQ */
		"str r9, [r0,#84]\n\t"		/* R9_FIQ */
		"str r10, [r0,#88]\n\t"		/* R10_FIQ */
		"str r11, [r0,#92]\n\t"		/* R11_FIQ */
		"str r12, [r0,#96]\n\t"		/* R12_FIQ */
		"str r13, [r0,#100]\n\t"	/* R13_FIQ */
		"str r14, [r0,#104]\n\t"	/* R14_FIQ */
		"mrs r1, spsr\n\t"		/* SPSR_FIQ */
		"str r1, [r0,#108]\n\t"
		/* IRQ */
		"mrs r1, cpsr\n\t"		/* switch to IRQ mode */
		"and r1, r1, #0xFFFFFFE0\n\t"
		"orr r1, r1, #0x12\n\t"
		"msr cpsr,r1\n\t"
		"str r13, [r0,#112]\n\t"	/* R13_IRQ */
		"str r14, [r0,#116]\n\t"	/* R14_IRQ */
		"mrs r1, spsr\n\t"		/* SPSR_IRQ */
		"str r1, [r0,#120]\n\t"
		/* ABT */
		"mrs r1, cpsr\n\t"		/* switch to Abort mode */
		"and r1, r1, #0xFFFFFFE0\n\t"
		"orr r1, r1, #0x17\n\t"
		"msr cpsr,r1\n\t"
		"str r13, [r0,#136]\n\t"	/* R13_ABT */
		"str r14, [r0,#140]\n\t"	/* R14_ABT */
		"mrs r1, spsr\n\t"		/* SPSR_ABT */
		"str r1, [r0,#144]\n\t"
		/* UND */
		"mrs r1, cpsr\n\t"		/* switch to undef mode */
		"and r1, r1, #0xFFFFFFE0\n\t"
		"orr r1, r1, #0x1B\n\t"
		"msr cpsr,r1\n\t"
		"str r13, [r0,#148]\n\t"	/* R13_UND */
		"str r14, [r0,#152]\n\t"	/* R14_UND */
		"mrs r1, spsr\n\t"		/* SPSR_UND */
		"str r1, [r0,#156]\n\t"
		/* restore to SVC mode */
		"mrs r1, cpsr\n\t"		/* switch to SVC mode */
		"and r1, r1, #0xFFFFFFE0\n\t"
		"orr r1, r1, #0x13\n\t"
		"msr cpsr,r1\n\t" :		/* output */
		: "r"(core_reg)			/* input */
		: "%r0", "%r1"			/* clobbered registers */
	);
}

static inline void sec_debug_save_mmu_reg(struct sec_debug_mmu_reg_t *mmu_reg)
{
	asm (
		"mrc    p15, 0, r1, c1, c0, 0\n\t"	/* SCTLR */
		"str r1, [%0]\n\t"
		"mrc    p15, 0, r1, c2, c0, 0\n\t"	/* TTBR0 */
		"str r1, [%0,#4]\n\t"
		"mrc    p15, 0, r1, c2, c0,1\n\t"	/* TTBR1 */
		"str r1, [%0,#8]\n\t"
		"mrc    p15, 0, r1, c2, c0,2\n\t"	/* TTBCR */
		"str r1, [%0,#12]\n\t"
		"mrc    p15, 0, r1, c3, c0,0\n\t"	/* DACR */
		"str r1, [%0,#16]\n\t"
		"mrc    p15, 0, r1, c5, c0,0\n\t"	/* DFSR */
		"str r1, [%0,#20]\n\t"
		"mrc    p15, 0, r1, c6, c0,0\n\t"	/* DFAR */
		"str r1, [%0,#24]\n\t"
		"mrc    p15, 0, r1, c5, c0,1\n\t"	/* IFSR */
		"str r1, [%0,#28]\n\t"
		"mrc    p15, 0, r1, c6, c0,2\n\t"	/* IFAR */
		"str r1, [%0,#32]\n\t"
		/* Don't populate DAFSR and RAFSR */
		"mrc    p15, 0, r1, c10, c2,0\n\t"	/* PMRRR */
		"str r1, [%0,#44]\n\t"
		"mrc    p15, 0, r1, c10, c2,1\n\t"	/* NMRRR */
		"str r1, [%0,#48]\n\t"
		"mrc    p15, 0, r1, c13, c0,0\n\t"	/* FCSEPID */
		"str r1, [%0,#52]\n\t"
		"mrc    p15, 0, r1, c13, c0,1\n\t"	/* CONTEXT */
		"str r1, [%0,#56]\n\t"
		"mrc    p15, 0, r1, c13, c0,2\n\t"	/* URWTPID */
		"str r1, [%0,#60]\n\t"
		"mrc    p15, 0, r1, c13, c0,3\n\t"	/* UROTPID */
		"str r1, [%0,#64]\n\t"
		"mrc    p15, 0, r1, c13, c0,4\n\t"	/* POTPIDR */
		"str r1, [%0,#68]\n\t" :		/* output */
		: "r"(mmu_reg)			/* input */
		: "%r1", "memory"			/* clobbered register */
	);
}

extern union sec_debug_level_t sec_debug_level;
void sec_dump_task_info(void);
void sec_upload_cause(void *buf);
void sec_debug_check_crash_key(unsigned int code, int value);
void register_log_text_hook(void (*f)(char *text, unsigned int size), char *buf,
	unsigned int *position, unsigned int bufsize);
void *persistent_ram_vmap(phys_addr_t start, size_t size, unsigned int memtype);
/* getlog support */
void sec_getlog_supply_kernel(void *klog_buf);
void sec_getlog_supply_platform(unsigned char *buffer, const char *name);
void sec_gaf_supply_rqinfo(unsigned short curr_offset, unsigned short rq_offset);
/* reset extra info */
void sec_debug_store_bug_string(const char *fmt, ...);
void sec_debug_store_fault_addr(unsigned long addr, struct pt_regs *regs);
void sec_debug_store_backtrace(struct pt_regs *regs);
void sec_debug_set_upload_magic(unsigned magic, char *str);
/* sec logging */
#ifdef CONFIG_SEC_AVC_LOG
extern void sec_debug_avc_log(char *fmt, ...);
#else
#define sec_debug_avc_log(a, ...)		do { } while(0)
#endif

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
extern void sec_debug_tsp_log(char *fmt, ...);
#else
#define sec_debug_tsp_log(a, ...)		do { } while(0)
#endif

#endif /* SEC_DEBUG_H */
