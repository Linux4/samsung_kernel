/* linux/arch/arm/mach-exynos/cpuidle-exynos4415.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/io.h>
#include <linux/export.h>
#include <linux/time.h>
#include <linux/gpio.h>
#include <linux/suspend.h>

#include <asm/proc-fns.h>
#include <asm/smp_scu.h>
#include <asm/suspend.h>
#include <asm/cpuidle.h>
#include <asm/cacheflush.h>
#include <asm/system_misc.h>
#include <asm/tlbflush.h>

#include <mach/sec_debug.h>

#include <plat/pm.h>
#include <plat/gpio-cfg.h>
#include <plat/devs.h>

#include <mach/regs-gpio.h>
#include <mach/regs-pmu.h>
#include <mach/regs-clock-exynos4415.h>
#include <mach/regs-clock.h>
#include <mach/pmu.h>
#include <mach/smc.h>
#include <plat/cpu.h>
#include <plat/usb-phy.h>

#define MSHCI_STATUS            0x48
#define MSHCI_DATA_BUSY         (0x1 << 9)
#define MSHCI_DATA_STAT_BUSY    (0x1 << 10)

#ifdef CONFIG_ARM_TRUSTZONE
#define REG_DIRECTGO_ADDR       (S5P_VA_SYSRAM_NS + 0x24)
#define REG_DIRECTGO_FLAG       (S5P_VA_SYSRAM_NS + 0x20)
#else
#define REG_DIRECTGO_ADDR	(S5P_VA_SYSRAM + 0x24)
#define REG_DIRECTGO_FLAG	(S5P_VA_SYSRAM + 0x20)
#endif

#define EXYNOS_CHECK_DIRECTGO		0xFCBA0D10

#if defined(CONFIG_SEC_PM)
#include <linux/moduleparam.h>

#define CPUIDLE_ENABLE_MASK ( ENABLE_LPA )

static enum {
        ENABLE_IDLE = 0x0,
        ENABLE_LPA  = 0x4,
} enable_mask = CPUIDLE_ENABLE_MASK;
module_param_named(enable_mask, enable_mask, uint, 0644);

#ifdef CONFIG_SEC_PM_DEBUG
unsigned int log_en = ENABLE_LPA;
module_param_named(log_en, log_en, uint, 0644);
#endif /* CONFIG_SEC_PM_DEBUG */
#endif /*CONFIG_SEC_PM*/

#if defined(CONFIG_BT_BCM4339) || defined(CONFIG_BT_BCM4334)
extern int bt_is_running;
extern void bt_uart_rts_ctrl(int flag);
#endif

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

static int sdmmc_dev_num;

/* If SD/MMC interface is working: return = 1 or not 0 */
static int check_sdmmc_op(unsigned int ch)
{
	void __iomem *base_addr;
	unsigned int status;

	if (unlikely(ch >= sdmmc_dev_num)) {
		pr_err("Invalid ch[%d] for SD/MMC\n", ch);
		return 0;
	}

	base_addr = chk_sdhc_op[ch].base;
	status = __raw_readl(base_addr + MSHCI_STATUS);
	return (status & MSHCI_DATA_BUSY) || (status & MSHCI_DATA_STAT_BUSY);
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

static struct sleep_save exynos4_lpa_save[] = {
	/* CMU side */
	SAVE_ITEM(EXYNOS4415_CLKSRC_DMC),
	SAVE_ITEM(EXYNOS4415_CLKSRC_MASK_TOP),
	SAVE_ITEM(EXYNOS4415_CLKSRC_MASK_CAM),
	SAVE_ITEM(EXYNOS4415_CLKSRC_MASK_TV),
	SAVE_ITEM(EXYNOS4415_CLKSRC_MASK_LCD),
	SAVE_ITEM(EXYNOS4415_CLKSRC_MASK_ISP),
	SAVE_ITEM(EXYNOS4415_CLKSRC_MASK_MAUDIO),
	SAVE_ITEM(EXYNOS4415_CLKSRC_MASK_FSYS),
	SAVE_ITEM(EXYNOS4415_CLKSRC_MASK_PERIL0),
	SAVE_ITEM(EXYNOS4415_CLKSRC_MASK_PERIL1),
	SAVE_ITEM(EXYNOS4415_CLKSRC_MASK_CMU_ISP0),
	SAVE_ITEM(EXYNOS4415_CLKSRC_MASK_ACP),
};

static struct sleep_save exynos4_set_clksrc[] = {
	{ .reg = EXYNOS4415_CLKSRC_MASK_ACP		, .val = 0x00010000, },
	{ .reg = EXYNOS4_CLKSRC_MASK_TOP		, .val = 0x00000001, },
	{ .reg = EXYNOS4_CLKSRC_MASK_CAM		, .val = 0xD1101111, },
	{ .reg = EXYNOS4_CLKSRC_MASK_TV			, .val = 0x00000001, },
	{ .reg = EXYNOS4_CLKSRC_MASK_LCD0		, .val = 0x00001001, },
	{ .reg = EXYNOS4_CLKSRC_MASK_ISP		, .val = 0x00011110, },
	{ .reg = EXYNOS4_CLKSRC_MASK_MAUDIO		, .val = 0x00000001, },
	{ .reg = EXYNOS4_CLKSRC_MASK_FSYS		, .val = 0x10000111, },
	{ .reg = EXYNOS4_CLKSRC_MASK_PERIL0		, .val = 0x00001111, },
	{ .reg = EXYNOS4_CLKSRC_MASK_PERIL1		, .val = 0x01110111, },
	{ .reg = EXYNOS4415_CLKSRC_MASK_CMU_ISP0	, .val = 0x01111111, },
};

static int exynos4_enter_idle(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index);
#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
static int exynos4_enter_c2(struct cpuidle_device *dev,
				 struct cpuidle_driver *drv,
				 int index);
#endif
static int exynos4_enter_lowpower(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index);

static struct cpuidle_state exynos4_cpuidle_set[] __initdata = {
	[0] = {
		.enter                  = exynos4_enter_idle,
		.exit_latency           = 1,
		.target_residency       = 1000,
		.flags                  = CPUIDLE_FLAG_TIME_VALID,
		.name                   = "C1",
		.desc                   = "ARM clock gating(WFI)",
	},
	[1] = {
#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
		.enter			= exynos4_enter_c2,
		.exit_latency		= 200,
		.target_residency	= 3000,
		.flags			= CPUIDLE_FLAG_TIME_VALID,
		.name			= "C2",
		.desc			= "ARM local power down",
	},
	[2] = {
#endif
		.enter			= exynos4_enter_lowpower,
		.exit_latency		= 300,
		.target_residency	= 10000,
		.flags			= CPUIDLE_FLAG_TIME_VALID,
		.name			= "C3",
		.desc			= "ARM system level power down",
	},
};

static DEFINE_PER_CPU(struct cpuidle_device, exynos4_cpuidle_device);

static struct cpuidle_driver exynos4_idle_driver = {
	.name			= "exynos4_idle",
	.owner			= THIS_MODULE,
};

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

/* Use only EXT_GIC IRQn/FIQn as wake up source for AFTR
   Mask all other interrupts and dont touch reserved bits */
static void exynos_set_wakeupmask(void)
{
	unsigned int wakeup_mask, wakeup_mask2;

	wakeup_mask = __raw_readl(EXYNOS_WAKEUP_MASK);
	wakeup_mask2 = __raw_readl(EXYNOS_WAKEUP_MASK2);

	__raw_writel(wakeup_mask | 0x4000EE3E, EXYNOS_WAKEUP_MASK);
	__raw_writel(wakeup_mask2 & ~(0x3333), EXYNOS_WAKEUP_MASK2);

}

static int idle_finisher(unsigned long flags)
{
#ifdef CONFIG_ARM_TRUSTZONE
	exynos_smc(SMC_CMD_CPU0AFTR, 0, 0, 0);
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

static int check_power_domain(void)
{

	unsigned long tmp;

	tmp = __raw_readl(EXYNOS4415_LCD0_CONFIGURATION + 4);
	if ((tmp & EXYNOS_INT_LOCAL_PWR_EN) == EXYNOS_INT_LOCAL_PWR_EN)
		return 1;

	tmp = __raw_readl(EXYNOS4415_MFC_CONFIGURATION + 4);
	if ((tmp & EXYNOS_INT_LOCAL_PWR_EN) == EXYNOS_INT_LOCAL_PWR_EN)
		return 2;

	tmp = __raw_readl(EXYNOS4415_G3D_CONFIGURATION + 4);
	if ((tmp & EXYNOS_INT_LOCAL_PWR_EN) == EXYNOS_INT_LOCAL_PWR_EN)
		return 3;

	tmp = __raw_readl(EXYNOS4415_TV_CONFIGURATION + 4);
	if ((tmp & EXYNOS_INT_LOCAL_PWR_EN) == EXYNOS_INT_LOCAL_PWR_EN)
		return 4;

	tmp = __raw_readl(EXYNOS4415_CAM_CONFIGURATION + 4);
	if ((tmp & EXYNOS_INT_LOCAL_PWR_EN) == EXYNOS_INT_LOCAL_PWR_EN)
		return 5;

	tmp = __raw_readl(EXYNOS4415_ISP0_CONFIGURATION + 4);
	if ((tmp & EXYNOS_INT_LOCAL_PWR_EN) == EXYNOS_INT_LOCAL_PWR_EN)
		return 6;

	tmp = __raw_readl(EXYNOS4415_ISP1_CONFIGURATION + 4);
	if ((tmp & EXYNOS_INT_LOCAL_PWR_EN) == EXYNOS_INT_LOCAL_PWR_EN)
		return 7;

	tmp = __raw_readl(EXYNOS4415_CLKGATE_IP_FSYS);
	if (tmp & (0x3<<0))
		return 8;
	return 0;
}

static int check_usb_op(void)
{
	return exynos_check_usb_op();
}

#if defined(CONFIG_GPS_BCMxxxxx)
static int check_gps_op(void)
{
	return  gpio_get_value(GPIO_GPS_PWR_EN);
}
#endif

#if defined(CONFIG_BT)
static int check_bt_op(void)
{
	#if defined(CONFIG_BT_BCM4339) || defined(CONFIG_BT_BCM4334)
	return bt_is_running;
	#else
	return 0;
	#endif
}
#endif

static int exynos_check_operation(void)
{
	int ret = 0;

	ret = check_power_domain();
	if (ret)
		goto out;

	ret = check_usb_op();
	if (ret)
		goto out;

#if defined(CONFIG_GPS_BCMxxxxx)
	ret = check_gps_op();
	if (ret)
		goto out;
#endif

#if defined(CONFIG_BT)
	ret = check_bt_op();
	if (ret)
		goto out;
#endif

#if defined(CONFIG_EXYNOS_DEV_DWMCI)
	ret = loop_sdmmc_check();
	if (ret)
		goto out;
#endif

#if defined(CONFIG_SEC_PM)
        if (!(enable_mask & ENABLE_LPA))
                return 1;
#endif
out:
	return ret;
}

static int exynos4_enter_core0_aftr(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	struct timeval before, after;
	unsigned long tmp;
	unsigned int cpuid = smp_processor_id();

	local_irq_disable();
	sec_debug_task_log_msg(cpuid, "aftr+");
	do_gettimeofday(&before);

	exynos_set_wakeupmask();

	/* Set value of power down register for aftr mode */
	exynos_sys_powerdown_conf(SYS_AFTR);

	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_DIRECTGO_ADDR);
	__raw_writel(EXYNOS_CHECK_DIRECTGO, REG_DIRECTGO_FLAG);

	save_cpu_arch_register();

	/* Setting Central Sequence Register for power down mode */
	tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	tmp &= ~EXYNOS_CENTRAL_LOWPWR_CFG;
	__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);

	set_boot_flag(0, C2_STATE);
	cpu_pm_enter();

	tmp = cpu_suspend(0, idle_finisher);
	if (tmp) {
		tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
		tmp |= EXYNOS_CENTRAL_LOWPWR_CFG;
		__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	}

	scu_enable(S5P_VA_SCU);

	cpu_pm_exit();
	clear_boot_flag(0, C2_STATE);

	restore_cpu_arch_register();

	/* Clear jump address and AFTR flag*/
	__raw_writel(0x0, REG_DIRECTGO_ADDR);
	__raw_writel(0x0, REG_DIRECTGO_FLAG);

	/* Clear wakeup state register */
	__raw_writel(0x0, EXYNOS_WAKEUP_STAT);
	__raw_writel(0x0, EXYNOS_WAKEUP_STAT2);

	do_gettimeofday(&after);
	sec_debug_task_log_msg(cpuid, "aftr-");
	dev->last_residency = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
					(after.tv_usec - before.tv_usec);

	local_irq_enable();
	return index;
}

static void s3c_pm_do_update_clksrc(struct sleep_save *ptr, int count)
{
	unsigned int tmp;

	for (; count > 0; count--, ptr++) {
		tmp = __raw_readl(ptr->reg);
		tmp |= ptr->val;
		__raw_writel(tmp, ptr->reg);
	}
	return;
}

static int exynos4_enter_core0_lpa(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	unsigned long tmp;
	struct timeval before, after;
	unsigned int wakeup_mask, wakeup_mask2;
	unsigned int temp, restore;
	unsigned int cpuid = smp_processor_id();
	unsigned int before_dll0, before_dll1, after_dll0, after_dll1;

	local_irq_disable();
#ifdef CONFIG_SEC_PM_DEBUG
        if (log_en & ENABLE_LPA)
                pr_info("+++lpa\n");
#endif
	sec_debug_task_log_msg(cpuid, "lpa+");
	do_gettimeofday(&before);

	/* Store the wakeup mask regs */
	wakeup_mask = __raw_readl(EXYNOS_WAKEUP_MASK);
	wakeup_mask2 = __raw_readl(EXYNOS_WAKEUP_MASK2);

	/* Unmask all possible interrupts and dont touch reserved bits */
	tmp = wakeup_mask | (1 << 30);
	tmp &= 0xFFFF11C1;
	__raw_writel(tmp, EXYNOS_WAKEUP_MASK);
	__raw_writel(wakeup_mask2 | 0x3333, EXYNOS_WAKEUP_MASK2);
	__raw_writel(0x0, EXYNOS_EINT_WAKEUP_MASK);

	#if defined(CONFIG_BT_BCM4339) || defined(CONFIG_BT_BCM4334)
	bt_uart_rts_ctrl(1);
	#endif

	/* Configure GPIO Power down control register */
	exynos_set_lpa_pdn(S3C_GPIO_END);

	s3c_pm_do_save(exynos4_lpa_save, ARRAY_SIZE(exynos4_lpa_save));
	s3c_pm_do_update_clksrc(exynos4_set_clksrc,
					ARRAY_SIZE(exynos4_set_clksrc));

	/* Switch DMC block to MPLL */
	//tmp = __raw_readl(EXYNOS4415_CLKSRC_DMC);
	//tmp &= ~((1 << 4) | (1 << 8));
	//__raw_writel(tmp, EXYNOS4415_CLKSRC_DMC);

	/* Set value of power down register for aftr mode */
	exynos_sys_powerdown_conf(SYS_LPA);

	save_cpu_arch_register();

	set_boot_flag(0, C2_STATE);

	/* Store resume address and LPA flag */
	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_DIRECTGO_ADDR);
	__raw_writel(EXYNOS_CHECK_DIRECTGO, REG_DIRECTGO_FLAG);

	/* Setting Central Sequence Register for power down mode */
	tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	tmp &= ~EXYNOS_CENTRAL_LOWPWR_CFG;
	__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);

	temp = __raw_readl(EXYNOS4415_CLKGATE_SCLK_DMC);
	restore = temp;
	temp &= ~(0x1 << 2 | 0x1 << 3);
	__raw_writel(temp, EXYNOS4415_CLKGATE_SCLK_DMC);

        //pr_err("============ %s ============ \n", __func__);
        before_dll0 = (__raw_readl(S5P_VA_DMC0 + 0x34)&0x1FF00)>>8;
        before_dll1 = (__raw_readl(S5P_VA_DMC1 + 0x34)&0x1FF00)>>8;
        sec_debug_aux_log(SEC_DEBUG_AUXLOG_DLL_CHANGE,"[LPA+]reg_PHY0_CON13 = 0x%08x", before_dll0);
        sec_debug_aux_log(SEC_DEBUG_AUXLOG_DLL_CHANGE,"[LPA+]reg_PHY1_CON13 = 0x%08x", before_dll1);

	cpu_pm_enter();
	tmp = cpu_suspend(0, idle_finisher);
	if (tmp) {
		tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
		tmp |= EXYNOS_CENTRAL_LOWPWR_CFG;
		__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	}

	after_dll0 = (__raw_readl(S5P_VA_DMC0 + 0x34)&0x1FF00)>>8;
	after_dll1 = (__raw_readl(S5P_VA_DMC1 + 0x34)&0x1FF00)>>8;

	__raw_writel(restore, EXYNOS4415_CLKGATE_SCLK_DMC);

	clear_boot_flag(0, C2_STATE);

	scu_enable(S5P_VA_SCU);
	cpu_pm_exit();

	restore_cpu_arch_register();

	s3c_pm_do_restore_core(exynos4_lpa_save, ARRAY_SIZE(exynos4_lpa_save));

	__raw_writel((1 << 28), EXYNOS_PAD_RET_GPIO_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_UART_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_MMCA_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_MMCB_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_EBIA_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_EBIB_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_JTAG_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_DRAM_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_SPI_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_MMC2_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_ISOL_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_ISOL_COREBLK_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_GPIO_COREBLK_OPTION);

	/* Clear jump address and AFTR flag*/
	__raw_writel(0x0, REG_DIRECTGO_ADDR);
	__raw_writel(0x0, REG_DIRECTGO_FLAG);

	/* Clear wakeup state register */
	__raw_writel(0x0, EXYNOS_WAKEUP_STAT);
	__raw_writel(0x0, EXYNOS_WAKEUP_STAT2);

	/* Restore the saved wakeup mask regs */
	__raw_writel(wakeup_mask, EXYNOS_WAKEUP_MASK);
	__raw_writel(wakeup_mask2, EXYNOS_WAKEUP_MASK2);

	#if defined(CONFIG_BT_BCM4339) || defined(CONFIG_BT_BCM4334)
	bt_uart_rts_ctrl(0);
	#endif

	do_gettimeofday(&after);
#ifdef CONFIG_SEC_PM_DEBUG
        if (log_en & ENABLE_LPA)
                pr_info("---lpa\n");
#endif
	sec_debug_task_log_msg(cpuid, "lpa-");
	//after_dll0 = (__raw_readl(S5P_VA_DMC0 + 0x34)&0x1FF00)>>8;
	//after_dll1 = (__raw_readl(S5P_VA_DMC1 + 0x34)&0x1FF00)>>8;
	//pr_err("============ %s after ============ \n", __func__);
	sec_debug_aux_log(SEC_DEBUG_AUXLOG_DLL_CHANGE,"[LPA-]reg_PHY0_CON13 = 0x%08x", after_dll0);
	sec_debug_aux_log(SEC_DEBUG_AUXLOG_DLL_CHANGE,"[LPA-]reg_PHY1_CON13 = 0x%08x", after_dll1);

	if(after_dll0 <= 0x80 || after_dll1 <= 0x80)
		panic("dll lock corrupted");

	dev->last_residency = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
					(after.tv_usec - before.tv_usec);

	local_irq_enable();
	return index;
}

static int exynos4_enter_idle(struct cpuidle_device *dev,
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

#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
static int exynos4_enter_c2(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	struct timeval before, after;
	int idle_time, ret = 0;
	unsigned int tmp;
	unsigned int cpuid = smp_processor_id();

	local_irq_disable();
	do_gettimeofday(&before);

	/* Store resume address and DIRECTGO flag */
	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_DIRECTGO_ADDR);
	__raw_writel(EXYNOS_CHECK_DIRECTGO, REG_DIRECTGO_FLAG);

	tmp = __raw_readl(EXYNOS_ARM_CORE_CONFIGURATION(cpuid));
	tmp &= ~EXYNOS_CORE_LOCAL_PWR_EN;
	__raw_writel(tmp, EXYNOS_ARM_CORE_CONFIGURATION(cpuid));

	set_boot_flag(cpuid, C2_STATE);
	cpu_pm_enter();

	ret = cpu_suspend(0, c2_finisher);

	if (ret) {
		tmp = __raw_readl(EXYNOS_ARM_CORE_CONFIGURATION(cpuid));
		tmp |= EXYNOS_CORE_LOCAL_PWR_EN;
		__raw_writel(tmp, EXYNOS_ARM_CORE_CONFIGURATION(cpuid));
	}

	cpu_pm_exit();
	clear_boot_flag(cpuid, C2_STATE);

	do_gettimeofday(&after);
	local_irq_enable();
	idle_time = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
		    (after.tv_usec - before.tv_usec);

	dev->last_residency = idle_time;
	return index;
}
#endif

static int exynos4_enter_lowpower(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	/* This mode only can be entered when other cores are offline */
	if (num_online_cpus() > 1)
#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
		return exynos4_enter_c2(dev, drv, 1);
#else
		return exynos4_enter_idle(dev, drv,
						drv->safe_state_index);
#endif

	if (exynos_check_operation())
		return exynos4_enter_core0_aftr(dev, drv, index);
	else
		return exynos4_enter_core0_lpa(dev, drv, index);

}

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

static int __init exynos4_init_cpuidle(void)
{
	int i, cpu_id;
	struct cpuidle_device *device;
	struct cpuidle_driver *drv = &exynos4_idle_driver;

#if defined(CONFIG_EXYNOS_DEV_DWMCI)
	struct platform_device *pdev;
	struct resource *res;
#endif
	/* Setup cpuidle driver */
	drv->state_count = ARRAY_SIZE(exynos4_cpuidle_set);

	for (i = 0; i < drv->state_count; i++) {
		memcpy(&drv->states[i], &exynos4_cpuidle_set[i],
				sizeof(struct cpuidle_state));
	}

	drv->safe_state_index = 0;
	if (cpuidle_register_driver(&exynos4_idle_driver))
		return -EIO;

	for_each_cpu(cpu_id, cpu_online_mask) {
		device = &per_cpu(exynos4_cpuidle_device, cpu_id);
		device->cpu = cpu_id;

		device->state_count = drv->state_count;

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
			goto err_dwmci_resource;
		}

		chk_sdhc_op[i].base = ioremap(res->start, resource_size(res));

		if (!chk_sdhc_op[i].base) {
			pr_err("failed to map io region\n");
			goto err_dwmci_ioremap;
		}
	}
#endif
	register_pm_notifier(&exynos_cpuidle_notifier);

	return 0;

err_dwmci_ioremap:
err_dwmci_resource:
	for_each_cpu(cpu_id, cpu_online_mask)
		cpuidle_unregister_device(&per_cpu(exynos4_cpuidle_device,
							cpu_id));
	cpuidle_unregister_driver(&exynos4_idle_driver);

	return -EIO;

}
device_initcall(exynos4_init_cpuidle);
