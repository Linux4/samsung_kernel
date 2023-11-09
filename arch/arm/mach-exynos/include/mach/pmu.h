/* linux/arch/arm/mach-exynos/include/mach/pmu.h
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - PMU(Power Management Unit) support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_PMU_H
#define __ASM_ARCH_PMU_H __FILE__

#define PMU_TABLE_END	NULL
#define CLUSTER_NUM	2
#define CPUS_PER_CLUSTER	4

#if defined(CONFIG_SOC_EXYNOS3475)
#define SMC_ID		0x81000700
#define SMC_ID_CLK	0x81000040 /* SSS clk control */
#else
#define SMC_ID		0x82000700
#define SMC_ID_CLK	0xC2001011 /* SSS clk control */
#endif

#define READ_CTRL	0x3
#define WRITE_CTRL	0x4

#define SSS_CLK_ENABLE	0
#define SSS_CLK_DISABLE	1

enum cp_mode {
	CP_POWER_ON,
	CP_RESET,
	CP_POWER_OFF,
	NUM_CP_MODE,
};

enum reset_mode {
	CP_HW_RESET,
	CP_SW_RESET,
};

enum cp_control {
	CP_CTRL_S,
	CP_CTRL_NS,
};

/* PMU(Power Management Unit) support */
enum sys_powerdown {
#if defined(CONFIG_SOC_EXYNOS5430)
	SYS_AFTR,
	SYS_LPD,
	SYS_LPA,
	SYS_ALPA,
	SYS_DSTOP,
	SYS_DSTOP_PSR,
	SYS_SLEEP,
#elif defined(CONFIG_SOC_EXYNOS5433)
	SYS_AFTR,
	SYS_STOP,
	SYS_DSTOP,
	SYS_DSTOP_PSR,
	SYS_LPD,
	SYS_LPA,
	SYS_ALPA,
	SYS_SLEEP,
#elif defined (CONFIG_SOC_EXYNOS3475)
	SYS_SICD,
	SYS_AFTR,
	SYS_LPD,
	SYS_DSTOP,
	SYS_CP_CALL,	/* actually means LPD for CP CALL */
#else
	SYS_AFTR,
	SYS_LPA,
	SYS_DSTOP,
	SYS_SLEEP,
#endif
	NUM_SYS_POWERDOWN,
};
#define PMU_TABLE_END	NULL

enum cpu_type {
	ARM,
	KFC,
	CPU_TYPE_MAX,
};

enum type_pmu_wdt_reset {
	/* if pmu_wdt_reset is EXYNOS_SYS_WDTRESET */
	PMU_WDT_RESET_TYPE0 = 0,
	/* if pmu_wdt_reset is EXYNOS5410_SYS_WDTRESET, EXYNOS5422_SYS_WDTRESET/ */
	PMU_WDT_RESET_TYPE1,
	/* if pmu_wdt_reset is EXYNOS5430_SYS_WDTRESET_EGL */
	PMU_WDT_RESET_TYPE2,
	/* if pmu_wdt_reset is EXYNOS5430_SYS_WDTRESET_KFC */
	PMU_WDT_RESET_TYPE3,
	/* if pmu_wdt_reset for exynos3475 */
	PMU_WDT_RESET_TYPE4,
};

extern unsigned long l2x0_regs_phys;
struct exynos_pmu_conf {
	void __iomem *reg;
	unsigned int val[NUM_SYS_POWERDOWN];
};

/* cpu boot mode flag */
#define RESET			(1 << 0)
#define SECONDARY_RESET		(1 << 1)
#define HOTPLUG			(1 << 2)
#define C2_STATE		(1 << 3)
#define CORE_SWITCH		(1 << 4)
#define WAIT_FOR_OB_L2FLUSH	(1 << 5)

#define BOOT_MODE_MASK  0x1f

extern void set_boot_flag(unsigned int cpu, unsigned int mode);
extern void clear_boot_flag(unsigned int cpu, unsigned int mode);
#ifdef CONFIG_SOC_EXYNOS3475
extern void exynos_set_wakeupmask(enum sys_powerdown mode);
void exynos_show_wakeup_reason(void);
#endif

#ifdef CONFIG_PINCTRL_EXYNOS
extern u64 exynos_get_eint_wake_mask(void);
#else
static inline u64 exynos_get_eint_wake_mask(void) { return 0xffffffffL; }
#endif

#ifndef CONFIG_CAL_SYS_PWRDOWN
extern void exynos_sys_powerdown_conf(enum sys_powerdown mode);
#endif
extern void exynos_xxti_sys_powerdown(bool enable);
extern void s3c_cpu_resume(void);
extern void exynos_set_core_flag(void);
extern void exynos_l2_common_pwr_ctrl(void);
extern void exynos_enable_idle_clock_down(unsigned int cluster);
extern void exynos_disable_idle_clock_down(unsigned int cluster);
extern void exynos_lpi_mask_ctrl(bool on);
extern void exynos_set_dummy_state(bool on);
extern void exynos_pmu_wdt_control(bool on, unsigned int pmu_wdt_reset_type);
extern void exynos_cpu_sequencer_ctrl(bool enable);
#ifndef CONFIG_CAL_SYS_PWRDOWN
extern void exynos_central_sequencer_ctrl(bool enable);
#endif
extern void exynos_eagle_asyncbridge_ignore_lpi(void);

struct exynos_cpu_power_ops {
	void (*power_up)(unsigned int cpu_id);
	void (*power_up_noinit)(unsigned int cpu_id);
	unsigned int (*power_state)(unsigned int cpu_id);
	void (*power_down)(unsigned int cpu_id);
	void (*power_down_noinit)(unsigned int cpu_id);
	void (*cluster_down)(unsigned int cluster);
	unsigned int (*cluster_state)(unsigned int cluster);
	bool (*is_last_core)(unsigned int cpu);
};

extern struct exynos_cpu_power_ops exynos_cpu;

#if defined(CONFIG_SOC_EXYNOS7580) || defined(CONFIG_SOC_EXYNOS7890) || defined(CONFIG_SOC_EXYNOS3475)
extern int exynos_cp_reset(void);
extern int exynos_cp_release(void);
extern int exynos_cp_init(void);
extern int exynos_cp_active_clear(void);
extern int exynos_clear_cp_reset(void);
extern int exynos_get_cp_power_status(void);
extern int exynos_set_cp_power_onoff(enum cp_mode mode);
extern void exynos_sys_powerdown_conf_cp(enum cp_mode mode);
extern int exynos_pmu_cp_init(void);
extern void exynos_cp_dump_pmu_reg(void);
#endif

#endif /* __ASM_ARCH_PMU_H */
