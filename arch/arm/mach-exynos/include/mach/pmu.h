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

#ifdef CONFIG_SOC_EXYNOS5260
#define CPUS_PER_CLUSTER	4
#endif

enum sys_powerdown {
	SYS_AFTR,
	SYS_LPA,
	SYS_SLEEP,
	NUM_SYS_POWERDOWN,
};

enum cp_mode {
	CP_POWER_ON,
	CP_RESET,
	CP_POWER_OFF,
	NUM_CP_MODE,
};

enum running_cpu {
	KFC,
	ARM,
};

enum type_pmu_wdt_reset {
	/* if pmu_wdt_reset is EXYNOS_SYS_WDTRESET */
	PMU_WDT_RESET_TYPE0 = 0,
	/* if pmu_wdt_reset is EXYNOS5410_SYS_WDTRESET */
	PMU_WDT_RESET_TYPE1,
	/* if pmu_wdt_reset is EXYNOS5260_SYS_WDTRESET */
	PMU_WDT_RESET_TYPE2,
};

enum reg_op {
	REG_INIT,	/* write new value */
	REG_RESET,	/* clear with zero */
	REG_SET,	/* bit set */
	REG_CLEAR,	/* bit clear */
};

/* reg/value set */
#define EXYNOS_PMU_REG(REG, VAL, OP)		\
{						\
	.reg	=	(void __iomem *)REG,	\
	.val	=	VAL,			\
	.op	=	OP,			\
}

struct exynos_pmu_init_reg {
	void __iomem *reg;
	unsigned int val;
	enum reg_op op;
};

extern unsigned long l2x0_regs_phys;
struct exynos_pmu_conf {
	void __iomem *reg;
	unsigned int val[NUM_SYS_POWERDOWN];
};

extern void exynos_sys_powerdown_conf(enum sys_powerdown mode);
extern void exynos_xxti_sys_powerdown(bool enable);
extern void s3c_cpu_resume(void);
extern void exynos_reset_assert_ctrl(bool on);
extern void exynos_set_core_flag(void);
extern void exynos_l2_common_pwr_ctrl(void);
#if defined(CONFIG_SOC_EXYNOS3250)
extern void exynos_enable_idle_clock_down(void);
extern void exynos_disable_idle_clock_down(void);
#else
extern void exynos_enable_idle_clock_down(unsigned int cluster);
extern void exynos_disable_idle_clock_down(unsigned int cluster);
#endif
extern void exynos_lpi_mask_ctrl(bool on);
extern void exynos_pmu_wdt_control(bool on, unsigned int pmu_wdt_reset_type);

extern bool exynos5260_is_last_core(unsigned int cpu);

#if defined(CONFIG_SOC_EXYNOS3470)
extern int exynos4_cp_reset(void);
extern int exynos4_cp_release(void);
extern int exynos4_cp_active_clear(void);
extern int exynos4_clear_cp_reset_req(void);
extern int exynos4_get_cp_power_status(void);
extern int exynos4_set_cp_power_onoff(enum cp_mode mode);
extern int exynos4_pmu_cp_init(void);
extern void exynos_sys_powerdown_conf_mif(enum sys_powerdown mode);
extern void exynos_sys_powerdown_conf_cp(enum cp_mode mode);
extern void exynos_init_idle_clock_down(void);
extern void exynos_idle_clock_down_disable(void);
#else
static inline int exynos4_pmu_cp_init(void) { return 0; }
static inline void exynos_sys_powerdown_conf_mif(enum sys_powerdown mode) { }
#endif

#endif /* __ASM_ARCH_PMU_H */
