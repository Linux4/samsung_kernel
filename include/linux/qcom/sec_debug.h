/*
 * sec_debug.h
 *
 * header file supporting debug functions for Samsung device
 *
 * COPYRIGHT(C) Samsung Electronics Co., Ltd. 2006-2011 All Right Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#ifndef SEC_DEBUG_H
#define SEC_DEBUG_H

#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/reboot.h>
#include <asm/cacheflush.h>

// Enable CONFIG_RESTART_REASON_DDR to use DDR address for saving restart reason
#ifdef CONFIG_RESTART_REASON_DDR
extern void *restart_reason_ddr_address;
#endif
extern struct class *sec_class;

#ifdef CONFIG_SEC_DEBUG
struct sec_debug_mmu_reg_t {
	unsigned long TTBR0_EL1;
	unsigned long TTBR1_EL1;
	unsigned long TCR_EL1;
	unsigned long MAIR_EL1;
	unsigned long ATCR_EL1;
	unsigned long AMAIR_EL1;	

	unsigned long HSTR_EL2;
	unsigned long HACR_EL2;
	unsigned long TTBR0_EL2;
	unsigned long VTTBR_EL2;
	unsigned long TCR_EL2;
	unsigned long VTCR_EL2;
	unsigned long MAIR_EL2;
	unsigned long ATCR_EL2;
	
	unsigned long TTBR0_EL3;
	unsigned long MAIR_EL3;
	unsigned long ATCR_EL3;
};


/* ARM CORE regs mapping structure */
struct sec_debug_core_t {
	/* COMMON */
	unsigned long x0;
	unsigned long x1;
	unsigned long x2;
	unsigned long x3;
	unsigned long x4;
	unsigned long x5;
	unsigned long x6;
	unsigned long x7;
	unsigned long x8;
	unsigned long x9;
	unsigned long x10;
	unsigned long x11;
	unsigned long x12;
	unsigned long x13;
	unsigned long x14;
	unsigned long x15;
	unsigned long x16;
	unsigned long x17;
	unsigned long x18;
	unsigned long x19;
	unsigned long x20;
	unsigned long x21;
	unsigned long x22;
	unsigned long x23;
	unsigned long x24;
	unsigned long x25;
	unsigned long x26;
	unsigned long x27;
	unsigned long x28;
	unsigned long x29;//sp
	unsigned long x30;//lr

	/* PC & CPSR */	
	unsigned long pc;
	unsigned long cpsr;

	/* EL0 */
	unsigned long sp_el0;

	/* EL1 */
	unsigned long sp_el1;
	unsigned long elr_el1;
	unsigned long spsr_el1;

	/* EL2 */
	unsigned long sp_el2;
	unsigned long elr_el2;
	unsigned long spsr_el2;

	/* EL3 */
//	unsigned long sp_el3;
//	unsigned long elr_el3;
//	unsigned long spsr_el3;

};
#define READ_SPECIAL_REG(x) ({ \
	u64 val; \
	asm volatile ("mrs %0, " # x : "=r"(val)); \
	val; \
})

DECLARE_PER_CPU(struct sec_debug_core_t, sec_debug_core_reg);
DECLARE_PER_CPU(struct sec_debug_mmu_reg_t, sec_debug_mmu_reg);
DECLARE_PER_CPU(enum sec_debug_upload_cause_t, sec_debug_upload_cause);

static inline void sec_debug_save_mmu_reg(struct sec_debug_mmu_reg_t *mmu_reg)
{
	u32 pstate,which_el; 

	pstate = READ_SPECIAL_REG(CurrentEl);
	which_el = pstate & PSR_MODE_MASK;

	//pr_emerg("%s: sec_debug EL mode=%d\n", __func__,which_el);

	mmu_reg->TTBR0_EL1 = READ_SPECIAL_REG(TTBR0_EL1);
	mmu_reg->TTBR1_EL1 = READ_SPECIAL_REG(TTBR1_EL1);	
	mmu_reg->TCR_EL1 = READ_SPECIAL_REG(TCR_EL1);
	mmu_reg->MAIR_EL1 = READ_SPECIAL_REG(MAIR_EL1);	
	mmu_reg->AMAIR_EL1 = READ_SPECIAL_REG(AMAIR_EL1);
#if 0
	if(which_el >= PSR_MODE_EL2t){
		mmu_reg->HSTR_EL2 = READ_SPECIAL_REG(HSTR_EL2);	
		mmu_reg->HACR_EL2 = READ_SPECIAL_REG(HACR_EL2);
		mmu_reg->TTBR0_EL2 = READ_SPECIAL_REG(TTBR0_EL2);	
		mmu_reg->VTTBR_EL2 = READ_SPECIAL_REG(VTTBR_EL2);
		mmu_reg->TCR_EL2 = READ_SPECIAL_REG(TCR_EL2);	
		mmu_reg->VTCR_EL2 = READ_SPECIAL_REG(VTCR_EL2);
		mmu_reg->MAIR_EL2 = READ_SPECIAL_REG(MAIR_EL2);	
		mmu_reg->AMAIR_EL2 = READ_SPECIAL_REG(AMAIR_EL2);
	}
#endif	
}

static inline void sec_debug_save_core_reg(struct sec_debug_core_t *core_reg)
{
	u32 pstate,which_el; 

	pstate = READ_SPECIAL_REG(CurrentEl);
	which_el = pstate & PSR_MODE_MASK;
	
	//pr_emerg("%s: sec_debug EL mode=%d\n", __func__,which_el);

	asm("str x0, [%0,#0]\n\t"	/* x0 is pushed first to core_reg */
		"mov x0, %0\n\t"		/* x0 will be alias for core_reg */
		"str x1, [x0,#0x8]\n\t"	/* x1 */
		"str x2, [x0,#0x10]\n\t"	/* x2 */
		"str x3, [x0,#0x18]\n\t"	/* x3 */
		"str x4, [x0,#0x20]\n\t"	/* x4 */
		"str x5, [x0,#0x28]\n\t"	/* x5 */
		"str x6, [x0,#0x30]\n\t"	/* x6 */
		"str x7, [x0,#0x38]\n\t"	/* x7 */
		"str x8, [x0,#0x40]\n\t"	/* x8 */
		"str x9, [x0,#0x48]\n\t"	/* x9 */
		"str x10, [x0,#0x50]\n\t" /* x10 */
		"str x11, [x0,#0x58]\n\t" /* x11 */
		"str x12, [x0,#0x60]\n\t" /* x12 */
		"str x13, [x0,#0x68]\n\t"/* x13 */
		"str x14, [x0,#0x70]\n\t"/* x14 */
		"str x15, [x0,#0x78]\n\t"/* x15 */
		"str x16, [x0,#0x80]\n\t"/* x16 */
#ifdef CONFIG_RKP_CFP_ROPP
		"str xzr, [x0,#0x88]\n\t"/* x17 */
#else
		"str x17, [x0,#0x88]\n\t"/* x17 */
#endif
		"str x18, [x0,#0x90]\n\t"/* x18 */
		"str x19, [x0,#0x98]\n\t"/* x19 */
		"str x20, [x0,#0xA0]\n\t"/* x20 */
		"str x21, [x0,#0xA8]\n\t"/* x21 */
		"str x22, [x0,#0xB0]\n\t"/* x22 */
		"str x23, [x0,#0xB8]\n\t"/* x23 */
		"str x24, [x0,#0xC0]\n\t"/* x24 */
		"str x25, [x0,#0xC8]\n\t"/* x25 */
		"str x26, [x0,#0xD0]\n\t"/* x26 */
		"str x27, [x0,#0xD8]\n\t"/* x27 */
		"str x28, [x0,#0xE0]\n\t"/* x28 */
		"str x29, [x0,#0xE8]\n\t"/* x29 */
		"str x30, [x0,#0xF0]\n\t"/* x30 */
		"adr x1, .\n\t"
		"str x1, [x0,#0xF8]\n\t"/* pc *///Need to check how to get the pc value.
		: 	/* output */
		: "r"(core_reg)		/* input */
		: "%x0", "%x1"/* clobbered registers */
		);	
		
	core_reg->sp_el0 = READ_SPECIAL_REG(sp_el0);

	if(which_el >= PSR_MODE_EL2t){
		core_reg->sp_el0 = READ_SPECIAL_REG(sp_el1);
		core_reg->elr_el1 = READ_SPECIAL_REG(elr_el1);
		core_reg->spsr_el1 = READ_SPECIAL_REG(spsr_el1);
		core_reg->sp_el2 = READ_SPECIAL_REG(sp_el2);
		core_reg->elr_el2 = READ_SPECIAL_REG(elr_el2);
		core_reg->spsr_el2 = READ_SPECIAL_REG(spsr_el2);
	}

	return;
}

static inline void sec_debug_save_context(void)
{
	unsigned long flags;
	local_irq_save(flags);
	sec_debug_save_mmu_reg(&per_cpu(sec_debug_mmu_reg,
				smp_processor_id()));
	sec_debug_save_core_reg(&per_cpu(sec_debug_core_reg,
				smp_processor_id()));
	pr_emerg("(%s) context saved(CPU:%d)\n", __func__,
			smp_processor_id());
	local_irq_restore(flags);
	flush_cache_all();
}

void simulate_msm_thermal_bite(void);
extern void sec_debug_prepare_for_wdog_bark_reset(void);
extern int sec_debug_init(void);
extern int sec_debug_dump_stack(void);
extern void sec_debug_hw_reset(void);
#ifdef CONFIG_SEC_PERIPHERAL_SECURE_CHK
extern void sec_peripheral_secure_check_fail(void);
#endif
extern void sec_debug_check_crash_key(unsigned int code, int value);
extern void sec_getlog_supply_fbinfo(void *p_fb, u32 res_x, u32 res_y, u32 bpp,
		u32 frames);
extern void sec_getlog_supply_meminfo(u32 size0, u32 addr0, u32 size1,
		u32 addr1);
extern void sec_getlog_supply_loggerinfo(void *p_main, void *p_radio,
		void *p_events, void *p_system);
extern void sec_getlog_supply_kloginfo(void *klog_buf);

extern void sec_gaf_supply_rqinfo(unsigned short curr_offset,
				  unsigned short rq_offset);
extern int sec_debug_is_secure_dump(void);
extern int sec_debug_is_rp_enabled(void);
extern int sec_debug_is_enabled(void);
extern int sec_debug_is_enabled_for_ssr(void);
extern int sec_debug_is_modem_seperate_debug_ssr(void);
extern int silent_log_panic_handler(void);
extern void sec_debug_secure_app_addr_size(uint32_t addr, uint32_t size);
#else
static inline void sec_debug_save_context(void) {}
static inline void sec_debug_prepare_for_wdog_bark_reset(void){}
static inline int sec_debug_init(void)
{
	return 0;
}

static inline int sec_debug_dump_stack(void)  
{
	return 0;
}
static inline void sec_debug_check_crash_key(unsigned int code, int value) {}

static inline void sec_getlog_supply_fbinfo(void *p_fb, u32 res_x, u32 res_y,
					    u32 bpp, u32 frames)
{
}

static inline void sec_getlog_supply_meminfo(u32 size0, u32 addr0, u32 size1,
					     u32 addr1)
{
}

static inline void sec_getlog_supply_loggerinfo(void *p_main,
						void *p_radio, void *p_events,
						void *p_system)
{
}

static inline void sec_getlog_supply_kloginfo(void *klog_buf)
{
}

static inline void sec_gaf_supply_rqinfo(unsigned short curr_offset,
					 unsigned short rq_offset)
{
}

static inline int sec_debug_is_enabled(void) {return 0; }
#endif

#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
extern void sec_debug_task_sched_log_short_msg(char *msg);
extern void sec_debug_secure_log(u32 svc_id,u32 cmd_id);
extern void sec_debug_task_sched_log(int cpu, struct task_struct *task);
extern void sec_debug_irq_sched_log(unsigned int irq, void *fn, int en);
extern void sec_debug_irq_sched_log_end(void);
extern void sec_debug_timer_log(unsigned int type, int int_lock, void *fn);
extern void sec_debug_sched_log_init(void);
#define secdbg_sched_msg(fmt, ...) \
	do { \
		char ___buf[16]; \
		snprintf(___buf, sizeof(___buf), fmt, ##__VA_ARGS__); \
		sec_debug_task_sched_log_short_msg(___buf); \
	} while (0)
#else
static inline void sec_debug_task_sched_log(int cpu, struct task_struct *task)
{
}
static inline void sec_debug_secure_log(u32 svc_id,u32 cmd_id)
{
}
static inline void sec_debug_irq_sched_log(unsigned int irq, void *fn, int en)
{
}
static inline void sec_debug_irq_sched_log_end(void)
{
}
static inline void sec_debug_timer_log(unsigned int type,
						int int_lock, void *fn)
{
}
static inline void sec_debug_sched_log_init(void)
{
}
#define secdbg_sched_msg(fmt, ...)
#endif
extern void sec_debug_irq_enterexit_log(unsigned int irq,
						unsigned long long start_time);

/* klaatu - schedule log */
#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
#define SCHED_LOG_MAX 512
#define TZ_LOG_MAX 64

struct irq_log {
	unsigned long long time;
	int irq;
	void *fn;
	int en;
	int preempt_count;
	void *context;
};

struct secure_log {
	unsigned long long time;
	u32 svc_id, cmd_id;
};

struct irq_exit_log {
	unsigned int irq;
	unsigned long long time;
	unsigned long long end_time;
	unsigned long long elapsed_time;
};

struct sched_log {
	unsigned long long time;
	char comm[TASK_COMM_LEN];
	pid_t pid;
	struct task_struct *pTask;
};


struct timer_log {
	unsigned long long time;
	unsigned int type;
	int int_lock;
	void *fn;
};

#endif	/* CONFIG_SEC_DEBUG_SCHED_LOG */

#ifdef CONFIG_SEC_DEBUG_SEMAPHORE_LOG
#define SEMAPHORE_LOG_MAX 100
struct sem_debug {
	struct list_head list;
	struct semaphore *sem;
	struct task_struct *task;
	pid_t pid;
	int cpu;
	/* char comm[TASK_COMM_LEN]; */
};

enum {
	READ_SEM,
	WRITE_SEM
};

#define RWSEMAPHORE_LOG_MAX 100
struct rwsem_debug {
	struct list_head list;
	struct rw_semaphore *sem;
	struct task_struct *task;
	pid_t pid;
	int cpu;
	int direction;
	/* char comm[TASK_COMM_LEN]; */
};

#endif	/* CONFIG_SEC_DEBUG_SEMAPHORE_LOG */

#ifdef CONFIG_SEC_DEBUG_MSG_LOG
extern asmlinkage int sec_debug_msg_log(void *caller, const char *fmt, ...);
#define MSG_LOG_MAX 1024
struct secmsg_log {
	unsigned long long time;
	char msg[64];
	void *caller0;
	void *caller1;
	char *task;
};
#define secdbg_msg(fmt, ...) \
	sec_debug_msg_log(__builtin_return_address(0), fmt, ##__VA_ARGS__)
#else
#define secdbg_msg(fmt, ...)
#endif

//KNOX_SEANDROID_START
#ifdef CONFIG_SEC_DEBUG_AVC_LOG
extern asmlinkage int sec_debug_avc_log(const char *fmt, ...);
#define AVC_LOG_MAX 256
struct secavc_log {
	char msg[256];
};
#define secdbg_avc(fmt, ...) \
	sec_debug_avc_log(fmt, ##__VA_ARGS__)
#else
#define secdbg_avc(fmt, ...)
#endif
//KNOX_SEANDROID_END

#ifdef CONFIG_SEC_DEBUG_DCVS_LOG
#define DCVS_LOG_MAX 256

struct dcvs_debug {
	unsigned long long time;
	int cpu_no;
	unsigned int prev_freq;
	unsigned int new_freq;
};
extern void sec_debug_dcvs_log(int cpu_no, unsigned int prev_freq,
			unsigned int new_freq);
#else
static inline void sec_debug_dcvs_log(int cpu_no, unsigned int prev_freq,
					unsigned int new_freq)
{
}

#endif

#ifdef CONFIG_SEC_DEBUG_FUELGAUGE_LOG
#define FG_LOG_MAX 128

struct fuelgauge_debug {
	unsigned long long time;
	unsigned int voltage;
	unsigned short soc;
	unsigned short charging_status;
};
extern void sec_debug_fuelgauge_log(unsigned int voltage, unsigned short soc,
			unsigned short charging_status);
#else
static inline void sec_debug_fuelgauge_log(unsigned int voltage,
			unsigned short soc,	unsigned short charging_status)
{
}

#endif

#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
struct sec_debug_log {
	atomic_t idx_sched[CONFIG_NR_CPUS];
	struct sched_log sched[CONFIG_NR_CPUS][SCHED_LOG_MAX];

	atomic_t idx_irq[CONFIG_NR_CPUS];
	struct irq_log irq[CONFIG_NR_CPUS][SCHED_LOG_MAX];

	atomic_t idx_secure[CONFIG_NR_CPUS];
	struct secure_log secure[CONFIG_NR_CPUS][TZ_LOG_MAX];

	atomic_t idx_irq_exit[CONFIG_NR_CPUS];
	struct irq_exit_log irq_exit[CONFIG_NR_CPUS][SCHED_LOG_MAX];

	atomic_t idx_timer[CONFIG_NR_CPUS];
	struct timer_log timer_log[CONFIG_NR_CPUS][SCHED_LOG_MAX];

#ifdef CONFIG_SEC_DEBUG_MSG_LOG
	atomic_t idx_secmsg[CONFIG_NR_CPUS];
	struct secmsg_log secmsg[CONFIG_NR_CPUS][MSG_LOG_MAX];
#endif
#ifdef CONFIG_SEC_DEBUG_AVC_LOG
	atomic_t idx_secavc[CONFIG_NR_CPUS];
	struct secavc_log secavc[CONFIG_NR_CPUS][AVC_LOG_MAX];
#endif
#ifdef CONFIG_SEC_DEBUG_DCVS_LOG
	atomic_t dcvs_log_idx[CONFIG_NR_CPUS] ;
	struct dcvs_debug dcvs_log[CONFIG_NR_CPUS][DCVS_LOG_MAX] ;
#endif
#ifdef CONFIG_SEC_DEBUG_FUELGAUGE_LOG
	atomic_t fg_log_idx;
	struct fuelgauge_debug fg_log[FG_LOG_MAX] ;
#endif
	/* zwei variables -- last_pet und last_ns */
	unsigned long long last_pet;
	atomic64_t last_ns;
};

#endif	/* CONFIG_SEC_DEBUG_SCHED_LOG */
/* for sec debug level */
#define KERNEL_SEC_DEBUG_LEVEL_LOW	(0x574F4C44)
#define KERNEL_SEC_DEBUG_LEVEL_MID	(0x44494D44)
#define KERNEL_SEC_DEBUG_LEVEL_HIGH	(0x47494844)
#define ANDROID_DEBUG_LEVEL_LOW		0x4f4c
#define ANDROID_DEBUG_LEVEL_MID		0x494d
#define ANDROID_DEBUG_LEVEL_HIGH	0x4948


extern int ssr_panic_handler_for_sec_dbg(void);
__weak void dump_all_task_info(void);
__weak void dump_cpu_stat(void);
extern void emerg_pet_watchdog(void);
extern void do_msm_restart(enum reboot_mode reboot_mode, const char *cmd);
#define LOCAL_CONFIG_PRINT_EXTRA_INFO

/* #define CONFIG_SEC_DEBUG_SUMMARY */
#ifdef CONFIG_SEC_DEBUG_SUMMARY
#endif

#ifdef CONFIG_SEC_DEBUG_DOUBLE_FREE
extern void *kfree_hook(void *p, void *caller);
#endif

typedef struct {
	char dev_name[24];
	u32 fsr;
	u32 fsynr;	
	unsigned long iova;
	unsigned long far;
	char mas_name[24];
	u8 cbndx;
	phys_addr_t phys_soft;
	phys_addr_t phys_atos;
	u32 sid;
} ex_info_smmu_t;

#ifdef CONFIG_USER_RESET_DEBUG

typedef enum {
	USER_UPLOAD_CAUSE_MIN = 1,
	USER_UPLOAD_CAUSE_SMPL = USER_UPLOAD_CAUSE_MIN,	/* RESET_REASON_SMPL */
	USER_UPLOAD_CAUSE_WTSR,			/* RESET_REASON_WTSR */
	USER_UPLOAD_CAUSE_WATCHDOG,		/* RESET_REASON_WATCHDOG */
	USER_UPLOAD_CAUSE_PANIC,		/* RESET_REASON_PANIC */
	USER_UPLOAD_CAUSE_MANUAL_RESET,	/* RESET_REASON_MANUAL_RESET */
	USER_UPLOAD_CAUSE_POWER_RESET,	/* RESET_REASON_POWER_RESET */
	USER_UPLOAD_CAUSE_REBOOT,		/* RESET_REASON_REBOOT */
	USER_UPLOAD_CAUSE_BOOTLOADER_REBOOT,/* RESET_REASON_BOOTLOADER_REBOOT */
	USER_UPLOAD_CAUSE_POWER_ON,		/* RESET_REASON_POWER_ON */
	USER_UPLOAD_CAUSE_THERMAL,		/* RESET_REASON_THERMAL_RESET */
	USER_UPLOAD_CAUSE_UNKNOWN,		/* RESET_REASON_UNKNOWN */
	USER_UPLOAD_CAUSE_MAX = USER_UPLOAD_CAUSE_UNKNOWN,
} user_upload_cause_t;

#ifdef CONFIG_USER_RESET_DEBUG_TEST
extern void force_thermal_reset(void);
extern void force_watchdog_bark(void);
#endif

enum extra_info_dbg_type {
	DBG_0_GLAD_ERR = 0,
	DBG_1_UFS_ERR,
	DBG_2_DISPLAY_ERR,
	DBG_3_RESERVED,
	DBG_4_RESERVED,
	DBG_5_RESERVED,
	DBG_MAX,
};

#define EXINFO_KERNEL_DEFAULT_SIZE              2048
#define EXINFO_KERNEL_SPARE_SIZE                1024
#define EXINFO_RPM_DEFAULT_SIZE			448
#define EXINFO_TZ_DEFAULT_SIZE                  40
#define EXINFO_PIMEM_DEFAULT_SIZE               8
#define EXINFO_HYP_DEFAULT_SIZE			460
#define EXINFO_SUBSYS_DEFAULT_SIZE              1024// 456 + 40 + 8 + 460 + spare

typedef struct {
	unsigned int esr;
	char str[52];
	u64 var1;
	u64 var2;
	u64 pte[6];
} ex_info_fault_t;

extern ex_info_fault_t ex_info_fault[NR_CPUS];

typedef struct {
	int reason;
	char handler_str[24];
	unsigned int esr;
	char esr_str[32];
} ex_info_badmode_t;

typedef struct {
	u64 ktime;
	u32 extc_idx;
	int cpu;
	char task_name[TASK_COMM_LEN];
	char bug_buf[64];
	char panic_buf[64];
	ex_info_smmu_t smmu;
	ex_info_fault_t fault[NR_CPUS];
	ex_info_badmode_t badmode;
	char pc[64];
	char lr[64];
	char dbg0[97];
	char ufs_err[17];
	char display_err[257];
	u32 lpm_state[1+2+NR_CPUS];
	u64 lr_val[NR_CPUS];
	u64 pc_val[NR_CPUS];
	u32 pko;
	char backtrace[0];
} _kern_ex_info_t;

typedef union {
	_kern_ex_info_t info;
	char ksize[EXINFO_KERNEL_DEFAULT_SIZE + EXINFO_KERNEL_SPARE_SIZE];
} kern_exinfo_t ;

typedef struct {
	u64 nsec;
	u32 arg[4];
	char msg[50];
} __rpm_log_t;

typedef struct {
		u32 magic;
		u32 ver;
		u32 nlog;
		__rpm_log_t log[5];
} _rpm_ex_info_t;

typedef union {
	_rpm_ex_info_t info;
	char rsize[EXINFO_RPM_DEFAULT_SIZE];
} rpm_exinfo_t;

typedef struct {
	char msg[EXINFO_TZ_DEFAULT_SIZE];
} tz_exinfo_t;

typedef struct {
	u32 esr;
	u32 ear0;
} pimem_exinfo_t;

typedef struct {
	char msg[EXINFO_HYP_DEFAULT_SIZE];
} hyp_exinfo_t;

#define RPM_EX_INFO_MAGIC 0x584D5052

typedef struct {
	kern_exinfo_t kern_ex_info;
	rpm_exinfo_t rpm_ex_info;
	tz_exinfo_t tz_ex_info;
	pimem_exinfo_t pimem_info;
	hyp_exinfo_t hyp_ex_info;
} rst_exinfo_t;

//rst_exinfo_t sec_debug_reset_ex_info;

#define SEC_DEBUG_EX_INFO_SIZE	(sizeof(rst_exinfo_t))		// 3KB

enum debug_reset_header_state {
	DRH_STATE_INIT=0,
	DRH_STATE_VALID,
	DRH_STATE_INVALID,
	DRH_STATE_MAX,
};

struct debug_reset_header {
	uint32_t magic;
	uint32_t write_times;
	uint32_t read_times;
	uint32_t ap_klog_idx;
	uint32_t summary_size;
	uint32_t stored_tzlog;
};

extern rst_exinfo_t *sec_debug_reset_ex_info;
extern int sec_debug_get_cp_crash_log(char *str);
extern void _sec_debug_store_backtrace(unsigned long where);
extern void sec_debug_store_extc_idx(bool prefix);
extern void sec_debug_store_bug_string(const char *fmt, ...);
extern void sec_debug_store_additional_dbg(enum extra_info_dbg_type type, unsigned int value, const char *fmt, ...);
extern struct debug_reset_header *get_debug_reset_header(void);
extern void sec_debug_summary_modem_print(void);

static inline void sec_debug_store_pte(unsigned long addr, int idx)
{
	int cpu = smp_processor_id();
	_kern_ex_info_t *p_ex_info;

	if (sec_debug_reset_ex_info) {
		p_ex_info = &sec_debug_reset_ex_info->kern_ex_info.info;
		if (p_ex_info->cpu == -1) {
			if(idx == 0)
				memset(&p_ex_info->fault[cpu].pte, 0,
						sizeof(p_ex_info->fault[cpu].pte));

			p_ex_info->fault[cpu].pte[idx] = addr;
		}
	}
}

static inline void sec_debug_save_fault_info(unsigned int esr, const char *str,
			unsigned long var1, unsigned long var2)
{
	int cpu = smp_processor_id();
	_kern_ex_info_t *p_ex_info;

	if (sec_debug_reset_ex_info) {
		p_ex_info = &sec_debug_reset_ex_info->kern_ex_info.info;
		if (p_ex_info->cpu == -1) {
			p_ex_info->fault[cpu].esr = esr;
			snprintf(p_ex_info->fault[cpu].str,
				sizeof(p_ex_info->fault[cpu].str),
				"%s", str);
			p_ex_info->fault[cpu].var1 = var1;
			p_ex_info->fault[cpu].var2 = var2;
		}
	}
}

static inline void sec_debug_save_badmode_info(int reason, const char *handler_str,
			unsigned int esr, const char *esr_str)
{
	_kern_ex_info_t *p_ex_info;

	if (sec_debug_reset_ex_info) {
		p_ex_info = &sec_debug_reset_ex_info->kern_ex_info.info;
		if (p_ex_info->cpu == -1) {
			p_ex_info->badmode.reason = reason;
			snprintf(p_ex_info->badmode.handler_str,
				sizeof(p_ex_info->badmode.handler_str),
				"%s", handler_str);
			p_ex_info->badmode.esr = esr;
			snprintf(p_ex_info->badmode.esr_str,
				sizeof(p_ex_info->badmode.esr_str),
				"%s", esr_str);
		}
	}
}

static inline void sec_debug_save_smmu_info(ex_info_smmu_t *smmu_info)
{
	_kern_ex_info_t *p_ex_info;

	if (sec_debug_reset_ex_info) {
		p_ex_info = &sec_debug_reset_ex_info->kern_ex_info.info;
		if (p_ex_info->cpu == -1) {
			snprintf(p_ex_info->smmu.dev_name,
				sizeof(p_ex_info->smmu.dev_name),
				"%s", smmu_info->dev_name);
			p_ex_info->smmu.fsr = smmu_info->fsr;
			p_ex_info->smmu.fsynr = smmu_info->fsynr;
			p_ex_info->smmu.iova = smmu_info->iova;
			p_ex_info->smmu.far = smmu_info->far;
			snprintf(p_ex_info->smmu.mas_name,
				sizeof(p_ex_info->smmu.mas_name),
				"%s", smmu_info->mas_name);
			p_ex_info->smmu.cbndx = smmu_info->cbndx;
			p_ex_info->smmu.phys_soft = smmu_info->phys_soft;
			p_ex_info->smmu.phys_atos = smmu_info->phys_atos;
			p_ex_info->smmu.sid = smmu_info->sid;
		}
	}
}
#else // CONFIG_USER_RESET_DEBUG
static inline void sec_debug_store_pte(unsigned long addr, int idx)
{
}

static inline void sec_dbg_save_fault_info(unsigned int esr, const char *str,
			unsigned long var1, unsigned long var2)
{
}

static inline void sec_debug_save_badmode_info(int reason, const char *handler_str,
			unsigned int esr, const char *esr_str)
{
}
static inline void sec_debug_save_smmu_info(ex_info_smmu_t *smmu_info)
{
}
static inline void sec_debug_summary_modem_print(void) { }
#endif // CONFIG_USER_RESET_DEBUG

#ifdef CONFIG_TOUCHSCREEN_DUMP_MODE
struct tsp_dump_callbacks {
	void (*inform_dump)(void);
};
#endif
#ifdef CONFIG_SEC_ISDBT_FORCE_OFF
extern void (*isdbt_force_off_callback)(void);
#endif

extern unsigned int lpcharge;
#endif	/* SEC_DEBUG_H */
