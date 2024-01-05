/* linux/arch/arm/mach-exynos/cpuidle-exynos3250.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/io.h>
#include <linux/export.h>
#include <linux/time.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/suspend.h>
#include <linux/moduleparam.h>
#include <linux/smp.h>

#include <asm/proc-fns.h>
#include <asm/smp_scu.h>
#include <asm/suspend.h>
#include <asm/unified.h>
#include <asm/cputype.h>
#include <asm/cacheflush.h>
#include <asm/system_misc.h>
#include <asm/tlbflush.h>

#include <mach/regs-pmu.h>
#include <mach/regs-clock.h>
#include <mach/pmu.h>
#include <mach/smc.h>
#include <mach/asv.h>

#include <plat/pm.h>
#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/regs-serial.h>
#include <plat/gpio-cfg.h>
#include <plat/gpio-core.h>
#include <plat/usb-phy.h>

#ifdef CONFIG_ARM_TRUSTZONE
#define REG_DIRECTGO_ADDR	(S5P_VA_SYSRAM_NS + 0x24)
#define REG_DIRECTGO_FLAG	(S5P_VA_SYSRAM_NS + 0x20)
#else
#define REG_DIRECTGO_ADDR	EXYNOS_INFORM0
#define REG_DIRECTGO_FLAG	EXYNOS_INFORM1
#endif

#define EXYNOS_CHECK_DIRECTGO	0xFCBA0D10

#define CPUIDLE_ENABLE_MASK (ENABLE_C2 | ENABLE_C3)

static enum {
	ENABLE_IDLE = 0x0,
	ENABLE_C2   = 0x1,
	ENABLE_C3   = 0x2,
} enable_mask = CPUIDLE_ENABLE_MASK;
module_param_named(enable_mask, enable_mask, uint, 0644);

static int exynos_enter_idle(struct cpuidle_device *dev,
			struct cpuidle_driver *drv,
			      int index);
#if defined(CONFIG_EXYNOS_CPUIDLE_C2)
static int exynos_enter_c2(struct cpuidle_device *dev,
				 struct cpuidle_driver *drv,
				 int index);
#endif
static int exynos_enter_lowpower(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index);

struct check_reg_lpa {
	void __iomem	*check_reg;
	unsigned int	check_bit;
};

/*
 * List of check power domain list for LPA mode
 * These register are have to power off to enter LPA mode
 */
static struct check_reg_lpa exynos_power_domain[] = {
	{.check_reg = EXYNOS3_LCD0_STATUS,	.check_bit = 0x7},
	{.check_reg = EXYNOS3_CAM_STATUS,       .check_bit = 0x7},
	{.check_reg = EXYNOS3_G3D_STATUS,	.check_bit = 0x7},
	{.check_reg = EXYNOS3_MFC_STATUS,	.check_bit = 0x7},
	{.check_reg = EXYNOS3_ISP_STATUS,       .check_bit = 0x7},
};

/*
 * List of check clock gating list for LPA mode
 * If clock of list is not gated, system can not enter LPA mode.
 */
static struct check_reg_lpa exynos_clock_gating[] = {
	{.check_reg = EXYNOS3_CLKGATE_IP_MFC,		.check_bit = 0x00000001},
	{.check_reg = EXYNOS3_CLKGATE_IP_FSYS,		.check_bit = 0x00000001},
	{.check_reg = EXYNOS3_CLKGATE_IP_PERIL,		.check_bit = 0x00033FC0},
};

#if defined(CONFIG_EXYNOS_DEV_DWMCI)
enum hc_type {
	HC_SDHC,
	HC_MSHC,
};

struct check_device_op {
	void __iomem		*base;
	struct platform_device	*pdev;
	enum hc_type		type;
};

static struct check_device_op chk_sdhc_op[] = {
	{.base = 0, .pdev = &exynos4_device_dwmci0, .type = HC_MSHC},
	{.base = 0, .pdev = &exynos4_device_dwmci1, .type = HC_MSHC},
	{.base = 0, .pdev = &exynos4_device_dwmci2, .type = HC_MSHC},
};

#define MSHCI_STATUS	(0x48)  /* Status */
#define MSHCI_DATA_BUSY	(0x1<<9)
#define MSHCI_DATA_STAT_BUSY	(0x1<<10)

static int sdmmc_dev_num;
/* If SD/MMC interface is working: return = 1 or not 0 */
static int check_sdmmc_op(unsigned int ch)
{
	unsigned int reg1;
	void __iomem *base_addr;

	if (unlikely(ch >= sdmmc_dev_num)) {
		pr_err("Invalid ch[%d] for SD/MMC\n", ch);
		return 0;
	}

	if (chk_sdhc_op[ch].type == HC_MSHC) {
		base_addr = chk_sdhc_op[ch].base;
		/* Check STATUS [9] for data busy */
		reg1 = readl(base_addr + MSHCI_STATUS);
		return (reg1 & (MSHCI_DATA_BUSY)) ||
			(reg1 & (MSHCI_DATA_STAT_BUSY));
	}
	/* should not be here */
	return 0;
}

/* Check all sdmmc controller */
static int loop_sdmmc_check(void)
{
	unsigned int iter;

	for (iter = 0; iter < sdmmc_dev_num; iter++) {
		if (check_sdmmc_op(iter)) {
			pr_debug("SDMMC [%d] working\n", iter);
			return 1;
		}
	}
	return 0;
}
#endif

static int exynos_check_reg_status(struct check_reg_lpa *reg_list,
				    unsigned int list_cnt)
{
	unsigned int i;
	unsigned int tmp;

	for (i = 0; i < list_cnt; i++) {
		tmp = __raw_readl(reg_list[i].check_reg);
		if (tmp & reg_list[i].check_bit)
			return -EBUSY;
	}

	return 0;
}

static int __maybe_unused exynos_check_enter_mode(void)
{
	/* Check power domain */
	if (exynos_check_reg_status(exynos_power_domain,
			    ARRAY_SIZE(exynos_power_domain)))
		return EXYNOS_CHECK_DIDLE;
	/* Check clock gating */
	if (exynos_check_reg_status(exynos_clock_gating,
			    ARRAY_SIZE(exynos_clock_gating)))
		return EXYNOS_CHECK_DIDLE;
#if defined(CONFIG_EXYNOS_DEV_DWMCI)
	if (loop_sdmmc_check())
		return EXYNOS_CHECK_DIDLE;
#endif
	if (exynos_check_usb_op())
		return EXYNOS_CHECK_DIDLE;

	return EXYNOS_CHECK_LPA;
}

static struct cpuidle_state exynos_cpuidle_set[] __initdata = {
	[0] = {
		.enter			= exynos_enter_idle,
		.exit_latency		= 1,
		.target_residency	= 1000,
		.flags			= CPUIDLE_FLAG_TIME_VALID,
		.name			= "C1",
		.desc			= "ARM clock gating(WFI)",
	},
	[1] = {
/* Fix_Me: enable this code for C2, C3 */
#if defined(CONFIG_EXYNOS_CPUIDLE_C2)
		.enter			= exynos_enter_c2,
		.exit_latency		= 30,
		.target_residency	= 1000,
		.flags			= CPUIDLE_FLAG_TIME_VALID,
		.name			= "C2",
		.desc			= "ARM power down",
	},
	[2] = {
#endif
		.enter                  = exynos_enter_lowpower,
		.exit_latency           = 300,
		.target_residency       = 5000,
		.flags                  = CPUIDLE_FLAG_TIME_VALID,
		.name                   = "C3",
		.desc                   = "ARM power down",
	},
};

static DEFINE_PER_CPU(struct cpuidle_device, exynos_cpuidle_device);

static struct cpuidle_driver exynos_idle_driver = {
	.name		= "exynos_idle",
	.owner		= THIS_MODULE,
};

#ifdef CONFIG_EXYNOS_IDLE_CLK_DOWN
void exynos_enable_idle_clock_down(void)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS3_PWR_CTRL);
	tmp &= ~((0x7 << 28) | (0x7 << 16) | (1 << 9) | (1 << 8));
	tmp |= (0x7 << 28) | (0x7 << 16) | 0x3ff;
	__raw_writel(tmp, EXYNOS3_PWR_CTRL);

	tmp = __raw_readl(EXYNOS3_PWR_CTRL2);
	tmp &= ~((0x3 << 24) | (0xffff << 8) | (0x77));
	tmp |= (100 << 16) | (10 << 8) | (1 << 4) | (0 << 0);
	tmp |= (1 << 25) | (1 << 24);
	__raw_writel(tmp, EXYNOS3_PWR_CTRL2);
}

void exynos_disable_idle_clock_down(void)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS3_PWR_CTRL);
	tmp &= ~(0x3 << 8);
	__raw_writel(tmp, EXYNOS3_PWR_CTRL);

	tmp = __raw_readl(EXYNOS3_PWR_CTRL2);
	tmp &= ~(0x3 << 24);
	__raw_writel(tmp, EXYNOS3_PWR_CTRL2);
}
#else
void exynos_enable_idle_clock_down(void) {
	pr_info("%s is not implement\n", __func__);
}

void exynos_disable_idle_clock_down(void) {
	pr_info("%s is not implement\n", __func__);
}
#endif

/* Ext-GIC nIRQ/nFIQ is the only wakeup source in AFTR */
static void exynos_set_wakeupmask(void)
{
	unsigned int origin = __raw_readl(EXYNOS_WAKEUP_MASK);

	origin = (origin & ~((0x1<<14)|(0x3<<9)|(0x1<<5)|(0x3<<1))) | (0x1 << 30);
	__raw_writel(origin, EXYNOS_WAKEUP_MASK);
	__raw_writel(0x0, EXYNOS_WAKEUP_MASK2);
}

#if !defined(CONFIG_ARM_TRUSTZONE)
static unsigned int g_pwr_ctrl, g_diag_reg;

static void save_cpu_arch_register(void)
{
	/*read power control register*/
	asm("mrc p15, 0, %0, c15, c0, 0" : "=r"(g_pwr_ctrl) : : "cc");
	/*read diagnostic register*/
	asm("mrc p15, 0, %0, c15, c0, 1" : "=r"(g_diag_reg) : : "cc");
	return;
}

static void restore_cpu_arch_register(void)
{
	/*write power control register*/
	asm("mcr p15, 0, %0, c15, c0, 0" : : "r"(g_pwr_ctrl) : "cc");
	/*write diagnostic register*/
	asm("mcr p15, 0, %0, c15, c0, 1" : : "r"(g_diag_reg) : "cc");
	return;
}
#else
static void save_cpu_arch_register(void)
{
}

static void restore_cpu_arch_register(void)
{
}
#endif

static int idle_finisher(unsigned long flags)
{
#if defined(CONFIG_ARM_TRUSTZONE)
	exynos_smc(SMC_CMD_SAVE, OP_TYPE_CORE, SMC_POWERSTATE_IDLE, 0);
	exynos_smc(SMC_CMD_SHUTDOWN, OP_TYPE_CLUSTER, SMC_POWERSTATE_IDLE, 0);
#else
	cpu_do_idle();
#endif
	return 1;
}

#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
static int c2_finisher(unsigned long flags)
{
#if defined(CONFIG_ARM_TRUSTZONE)
	exynos_smc(SMC_CMD_SAVE, OP_TYPE_CORE, SMC_POWERSTATE_IDLE, 0);
	exynos_smc(SMC_CMD_SHUTDOWN, OP_TYPE_CORE, SMC_POWERSTATE_IDLE, 0);
	/*
	 * Secure monitor disables the SMP bit and takes the CPU out of the
	 * coherency domain.
	 */
	local_flush_tlb_all();
#else
	cpu_do_idle();
#endif
	return 1;
}
#endif

static int exynos_enter_core0_aftr(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	struct timeval before, after;
	int idle_time;
	unsigned long tmp;
	unsigned int ret = 0;
	unsigned int cpuid = smp_processor_id();

	local_irq_disable();
	do_gettimeofday(&before);

	exynos_set_wakeupmask();

	exynos_disable_idle_clock_down();

	/* Set value of power down register for aftr mode */
	exynos_sys_powerdown_conf(SYS_AFTR);

	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_DIRECTGO_ADDR);
	__raw_writel(EXYNOS_CHECK_DIRECTGO, REG_DIRECTGO_FLAG);

	save_cpu_arch_register();

	/* Setting Central Sequence Register for power down mode */
	tmp = __raw_readl(EXYNOS3_CENTRAL_SEQ_CONFIGURATION_COREBLK);
	tmp &= ~EXYNOS_CENTRAL_LOWPWR_CFG;
	__raw_writel(tmp, EXYNOS3_CENTRAL_SEQ_CONFIGURATION_COREBLK);
	tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	tmp &= ~EXYNOS_CENTRAL_LOWPWR_CFG;
	__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);

	set_boot_flag(cpuid, C2_STATE);

	cpu_pm_enter();

	ret = cpu_suspend(0, idle_finisher);
	if (ret) {
		tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
		tmp |= EXYNOS_CENTRAL_LOWPWR_CFG;
		__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	}
	tmp = __raw_readl(EXYNOS3_CENTRAL_SEQ_CONFIGURATION_COREBLK);
	tmp |= EXYNOS_CENTRAL_LOWPWR_CFG;
	__raw_writel(tmp, EXYNOS3_CENTRAL_SEQ_CONFIGURATION_COREBLK);

	clear_boot_flag(cpuid, C2_STATE);

	cpu_pm_exit();

	restore_cpu_arch_register();

	exynos_enable_idle_clock_down();

	/* Clear wakeup state register */
	__raw_writel(0x0, EXYNOS_WAKEUP_STAT);

	do_gettimeofday(&after);

	local_irq_enable();
	idle_time = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
		    (after.tv_usec - before.tv_usec);

	dev->last_residency = idle_time;
	return index;
}

static int exynos_enter_core0_lpa(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	struct timeval before, after;
	int idle_time;
	unsigned long tmp;
	unsigned int ret;
	unsigned int cpuid = smp_processor_id();

	local_irq_disable();
	do_gettimeofday(&before);

	exynos_set_wakeupmask();

	exynos_disable_idle_clock_down();

	/* Set value of power down register for aftr mode */
	exynos_sys_powerdown_conf(SYS_LPA);

	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_DIRECTGO_ADDR);
	__raw_writel(EXYNOS_CHECK_DIRECTGO, REG_DIRECTGO_FLAG);

	save_cpu_arch_register();

	/* Setting Central Sequence Register for power down mode */
	tmp = __raw_readl(EXYNOS3_CENTRAL_SEQ_CONFIGURATION_COREBLK);
	tmp &= ~EXYNOS_CENTRAL_LOWPWR_CFG;
	__raw_writel(tmp, EXYNOS3_CENTRAL_SEQ_CONFIGURATION_COREBLK);
	tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	tmp &= ~EXYNOS_CENTRAL_LOWPWR_CFG;
	__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);

	set_boot_flag(cpuid, C2_STATE);

	cpu_pm_enter();

	ret = cpu_suspend(0, idle_finisher);
	if (ret) {
		tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
		tmp |= EXYNOS_CENTRAL_LOWPWR_CFG;
		__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	}
	tmp = __raw_readl(EXYNOS3_CENTRAL_SEQ_CONFIGURATION_COREBLK);
	tmp |= EXYNOS_CENTRAL_LOWPWR_CFG;
	__raw_writel(tmp, EXYNOS3_CENTRAL_SEQ_CONFIGURATION_COREBLK);

	clear_boot_flag(cpuid, C2_STATE);

	cpu_pm_exit();

	restore_cpu_arch_register();

	exynos_enable_idle_clock_down();

	/* Clear wakeup state register */
	__raw_writel(0x0, EXYNOS_WAKEUP_STAT);

	do_gettimeofday(&after);

	local_irq_enable();
	idle_time = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
		    (after.tv_usec - before.tv_usec);

	dev->last_residency = idle_time;
	return index;
}

static int exynos_enter_idle(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	struct timeval before, after;
	int idle_time;

	local_irq_disable();
	do_gettimeofday(&before);

	cpu_do_idle();

	do_gettimeofday(&after);
	local_irq_enable();
	idle_time = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
		    (after.tv_usec - before.tv_usec);

	dev->last_residency = idle_time;
	return index;
}

static int exynos_enter_lowpower(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	int new_index = index;

	/* This mode only can be entered when other core's are offline */
	if (!(enable_mask & ENABLE_C3) || (num_online_cpus() > 1))
#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
		return exynos_enter_c2(dev, drv, (new_index - 1));
#else
		return exynos_enter_idle(dev, drv, 0);
#endif
	if (exynos_check_enter_mode() == EXYNOS_CHECK_DIDLE)
		return exynos_enter_core0_aftr(dev, drv, new_index);
	else
		return exynos_enter_core0_lpa(dev, drv, new_index);
}

#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
static bool ping_sent = false;

static int exynos_enter_c2(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	struct timeval before, after;
	int idle_time, ret = 0;
	unsigned int tmp;
	unsigned int cpuid = smp_processor_id();
	unsigned int i;

	if (!(enable_mask & ENABLE_C2)) {
		if (!ping_sent) {
			 ping_sent = true;
			for_each_online_cpu(i) {
				if (i == cpuid)
					continue;
#if defined (CONFIG_SMP)
					arm_send_ping_ipi(i);
#endif
			}
		}
		return exynos_enter_idle(dev, drv, (index - 1));
	} else {
		if (ping_sent)
			ping_sent = false;
	}

	local_irq_disable();
	do_gettimeofday(&before);

	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_DIRECTGO_ADDR);
	__raw_writel(EXYNOS_CHECK_DIRECTGO, REG_DIRECTGO_FLAG);

	set_boot_flag(cpuid, C2_STATE);
	cpu_pm_enter();

	tmp = __raw_readl(EXYNOS_ARM_CORE_CONFIGURATION(cpuid));
	tmp |= EXYNOS_CORE_AUTOWAKEUP_EN;
	tmp &= ~EXYNOS_CORE_LOCAL_PWR_EN;
	__raw_writel(tmp, EXYNOS_ARM_CORE_CONFIGURATION(cpuid));

	ret = cpu_suspend(0, c2_finisher);

	if (ret) {
		tmp = __raw_readl(EXYNOS_ARM_CORE_CONFIGURATION(cpuid));
		tmp |= EXYNOS_CORE_AUTOWAKEUP_EN;
		tmp |= EXYNOS_CORE_LOCAL_PWR_EN;
		__raw_writel(tmp, EXYNOS_ARM_CORE_CONFIGURATION(cpuid));
	}

	clear_boot_flag(cpuid, C2_STATE);
	cpu_pm_exit();

	do_gettimeofday(&after);
	local_irq_enable();
	idle_time = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
		    (after.tv_usec - before.tv_usec);

	dev->last_residency = idle_time;
	return index;
}
#endif

static int exynos_cpuidle_notifier_event(struct notifier_block *this,
					  unsigned long event,
					  void *ptr)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		disable_hlt();
		pr_debug("PM_SUSPEND_PREPARE for CPUIDLE\n");
		return NOTIFY_OK;
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		enable_hlt();
		pr_debug("PM_POST_SUSPEND for CPUIDLE\n");
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static struct notifier_block exynos_cpuidle_notifier = {
	.notifier_call = exynos_cpuidle_notifier_event,
};

static int __init exynos_init_cpuidle(void)
{
	int i, max_cpuidle_state, cpu_id;
	struct cpuidle_device *device;
	struct cpuidle_driver *drv = &exynos_idle_driver;
	struct cpuidle_state *idle_set;
	struct platform_device *pdev;
	struct resource *res;

	exynos_enable_idle_clock_down();

	/* Setup cpuidle driver */
	idle_set = exynos_cpuidle_set;
	drv->state_count = ARRAY_SIZE(exynos_cpuidle_set);

	max_cpuidle_state = drv->state_count;
	for (i = 0; i < max_cpuidle_state; i++) {
		memcpy(&drv->states[i], &idle_set[i],
				sizeof(struct cpuidle_state));
	}
	drv->safe_state_index = 0;
	cpuidle_register_driver(&exynos_idle_driver);

	for_each_cpu(cpu_id, cpu_online_mask) {
		device = &per_cpu(exynos_cpuidle_device, cpu_id);
		device->cpu = cpu_id;

	device->state_count = max_cpuidle_state;

	if (cpuidle_register_device(device)) {
		printk(KERN_ERR "CPUidle register device failed\n,");
		return -EIO;
		}
	}

#if defined(CONFIG_EXYNOS_DEV_DWMCI)
	sdmmc_dev_num = ARRAY_SIZE(chk_sdhc_op);

	for (i = 0; i < sdmmc_dev_num; i++) {

		pdev = chk_sdhc_op[i].pdev;

		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!res) {
			pr_err("failed to get iomem region\n");
			return -EINVAL;
		}

		chk_sdhc_op[i].base = ioremap(res->start, resource_size(res));

		if (!chk_sdhc_op[i].base) {
			pr_err("failed to map io region\n");
			return -EINVAL;
		}
	}
#endif
	register_pm_notifier(&exynos_cpuidle_notifier);

	return 0;
}
device_initcall(exynos_init_cpuidle);
