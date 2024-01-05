/* linux/arch/arm/mach-exynos/pmu-exynos3250.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - CPU PMU(Power Management Unit) support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/cpumask.h>
#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/gpio.h>

#include <plat/gpio-cfg.h>

#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>
#include <mach/pmu.h>
#include <asm/cputype.h>

static struct exynos_pmu_conf *exynos_pmu_config;

static struct exynos_pmu_conf exynos3250_pmu_config[] = {
	/* { .reg = address, .val = { AFTR, W-AFTR, SLEEP } */
	{ EXYNOS3_ARM_CORE0_SYS_PWR_REG,			{ 0x0, 0x0, 0x2} },
	{ EXYNOS3_DIS_IRQ_ARM_CORE0_LOCAL_SYS_PWR_REG,		{ 0x0, 0x0, 0x0} },
	{ EXYNOS3_DIS_IRQ_ARM_CORE0_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0} },
	{ EXYNOS3_ARM_CORE1_SYS_PWR_REG,			{ 0x0, 0x0, 0x2} },
	{ EXYNOS3_DIS_IRQ_ARM_CORE1_LOCAL_SYS_PWR_REG,		{ 0x0, 0x0, 0x0} },
	{ EXYNOS3_DIS_IRQ_ARM_CORE1_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0} },
	{ EXYNOS3_ARM_CORE2_SYS_PWR_REG,			{ 0x0, 0x0, 0x2} },
	{ EXYNOS3_DIS_IRQ_ARM_CORE2_LOCAL_SYS_PWR_REG,		{ 0x0, 0x0, 0x0} },
	{ EXYNOS3_DIS_IRQ_ARM_CORE2_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0} },
	{ EXYNOS3_ARM_CORE3_SYS_PWR_REG,			{ 0x0, 0x0, 0x2} },
	{ EXYNOS3_DIS_IRQ_ARM_CORE3_LOCAL_SYS_PWR_REG,		{ 0x0, 0x0, 0x0} },
	{ EXYNOS3_DIS_IRQ_ARM_CORE3_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0} },
	{ EXYNOS3_ISP_ARM_SYS_PWR_REG,				{ 0x1, 0x0, 0x0} },
	{ EXYNOS3_DIS_IRQ_ISP_ARM_LOCAL_SYS_PWR_REG,		{ 0x0, 0x0, 0x0} },
	{ EXYNOS3_DIS_IRQ_ISP_ARM_CENTRAL_SYS_PWR_REG,		{ 0x0, 0x0, 0x0} },
	{ EXYNOS3_ARM_COMMON_SYS_PWR_REG,			{ 0x0, 0x0, 0x2} },
	{ EXYNOS3_ARM_L2_SYS_PWR_REG,				{ 0x0, 0x0, 0x3} },
	{ EXYNOS3_CMU_ACLKSTOP_SYS_PWR_REG,			{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_CMU_SCLKSTOP_SYS_PWR_REG,			{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_CMU_RESET_SYS_PWR_REG,			{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_DRAM_FREQ_DOWN_SYS_PWR_REG,			{ 0x1, 0x1, 0x1} },
	{ EXYNOS3_DDRPHY_DLLOFF_SYS_PWR_REG,			{ 0x1, 0x1, 0x1} },
	{ EXYNOS3_LPDDR_PHY_DLL_LOCK_SYS_PWR_REG,		{ 0x1, 0x1, 0x1} },
	{ EXYNOS3_CMU_ACLKSTOP_COREBLK_SYS_PWR_REG,		{ 0x1, 0x0, 0x0} },
	{ EXYNOS3_CMU_SCLKSTOP_COREBLK_SYS_PWR_REG,		{ 0x1, 0x0, 0x0} },
	{ EXYNOS3_CMU_RESET_COREBLK_SYS_PWR_REG,		{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_APLL_SYSCLK_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS3_MPLL_SYSCLK_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS3_BPLL_SYSCLK_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS3_VPLL_SYSCLK_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS3_EPLL_SYSCLK_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS3_UPLL_SYSCLK_SYS_PWR_REG,			{ 0x1, 0x1, 0x1} },
	{ EXYNOS3_EPLLUSER_SYSCLK_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS3_MPLLUSER_SYSCLK_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS3_BPLLUSER_SYSCLK_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS3_CMU_CLKSTOP_CAM_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS3_CMU_CLKSTOP_MFC_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS3_CMU_CLKSTOP_G3D_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS3_CMU_CLKSTOP_LCD0_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS3_CMU_CLKSTOP_ISP_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS3_CMU_CLKSTOP_MAUDIO_SYS_PWR_REG,		{ 0x1, 0x0, 0x0} },
	{ EXYNOS3_CMU_RESET_CAM_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS3_CMU_RESET_MFC_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS3_CMU_RESET_G3D_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS3_CMU_RESET_LCD0_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS3_CMU_RESET_ISP_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS3_CMU_RESET_MAUDIO_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS3_TOP_BUS_SYS_PWR_REG,			        { 0x3, 0x0, 0x0} },
	{ EXYNOS3_TOP_RETENTION_SYS_PWR_REG,			{ 0x1, 0x1, 0x1} },
	{ EXYNOS3_TOP_PWR_SYS_PWR_REG,				{ 0x3, 0x3, 0x3} },
	{ EXYNOS3_TOP_BUS_COREBLK_SYS_PWR_REG,			{ 0x3, 0x0, 0x0} },
	{ EXYNOS3_TOP_RETENTION_COREBLK_SYS_PWR_REG,		{ 0x1, 0x1, 0x1} },
	{ EXYNOS3_TOP_PWR_COREBLK_SYS_PWR_REG,			{ 0x3, 0x3, 0x3} },
	{ EXYNOS3_LOGIC_RESET_SYS_PWR_REG,			{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_OSCCLK_GATE_SYS_PWR_REG,			{ 0x1, 0x0, 0x1} },
	{ EXYNOS3_LOGIC_RESET_COREBLK_SYS_PWR_REG,		{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_OSCCLK_GATE_COREBLK_SYS_PWR_REG,		{ 0x1, 0x0, 0x1} },
	{ EXYNOS3_PAD_RETENTION_DRAM_SYS_PWR_REG,		{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_PAD_RETENTION_MAUDIO_SYS_PWR_REG,		{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_PAD_RETENTION_GPIO_SYS_PWR_REG,		{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_PAD_RETENTION_UART_SYS_PWR_REG,		{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_PAD_RETENTION_MMC0_SYS_PWR_REG,		{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_PAD_RETENTION_MMC1_SYS_PWR_REG,		{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_PAD_RETENTION_MMC2_SYS_PWR_REG,		{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_PAD_RETENTION_SPI_SYS_PWR_REG,		{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_PAD_RETENTION_EBIA_SYS_PWR_REG,		{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_PAD_RETENTION_EBIB_SYS_PWR_REG,		{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_PAD_RETENTION_JTAG_SYS_PWR_REG,		{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_PAD_ISOLATION_SYS_PWR_REG,			{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_PAD_ALV_SEL_SYS_PWR_REG,			{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_XUSBXTI_SYS_PWR_REG,				{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_XXTI_SYS_PWR_REG,				{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_EXT_REGULATOR_SYS_PWR_REG,			{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_EXT_REGULATOR_COREBLK_SYS_PWR_REG,		{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_GPIO_MODE_SYS_PWR_REG,			{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_GPIO_MODE_MAUDIO_SYS_PWR_REG,			{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_TOP_ASB_RESET_SYS_PWR_REG,			{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_TOP_ASB_ISOLATION_SYS_PWR_REG,		{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_TOP_ASB_RESET_COREBLK_SYS_PWR_REG,		{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_TOP_ASB_ISOLATION_COREBLK_SYS_PWR_REG,	{ 0x1, 0x1, 0x0} },
	{ EXYNOS3_CAM_SYS_PWR_REG,				{ 0x7, 0x0, 0x0} },
	{ EXYNOS3_MFC_SYS_PWR_REG,				{ 0x7, 0x0, 0x0} },
	{ EXYNOS3_G3D_SYS_PWR_REG,				{ 0x7, 0x0, 0x0} },
	{ EXYNOS3_LCD0_SYS_PWR_REG,				{ 0x7, 0x0, 0x0} },
	{ EXYNOS3_ISP_SYS_PWR_REG,				{ 0x7, 0x0, 0x0} },
	{ EXYNOS3_MAUDIO_SYS_PWR_REG,				{ 0x7, 0x0, 0x0} },
	{ EXYNOS3_CMU_SYSCLK_ISP_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ PMU_TABLE_END,},
};

void __iomem *exynos3250_list_feed[] = {
	EXYNOS3_ARM_CORE_OPTION(0),
	EXYNOS3_ARM_CORE_OPTION(1),
	EXYNOS3_ARM_CORE_OPTION(2),
	EXYNOS3_ARM_CORE_OPTION(3),
	EXYNOS3_ARM_COMMON_OPTION,
	EXYNOS3_TOP_PWR_OPTION,
	EXYNOS3_CORE_TOP_PWR_OPTION,
	EXYNOS3_CAM_OPTION,
	EXYNOS3_MFC_OPTION,
	EXYNOS3_G3D_OPTION,
	EXYNOS3_LCD0_OPTION,
	EXYNOS3_ISP_OPTION,
};

static void exynos3250_init_pmu(void)
{
	unsigned int i;
	unsigned int tmp;

	/* Enable only SC_FEEDBACK */
	for (i = 0 ; i < ARRAY_SIZE(exynos3250_list_feed) ; i++) {
		tmp = __raw_readl(exynos3250_list_feed[i]);
		tmp &= ~(EXYNOS3_OPTION_USE_SC_COUNTER);
		tmp |= EXYNOS3_OPTION_USE_SC_FEEDBACK;
		__raw_writel(tmp, exynos3250_list_feed[i]);
	}
}

void exynos_sys_powerdown_conf(enum sys_powerdown mode)
{
	unsigned int i;

	for (i = 0; (exynos_pmu_config[i].reg != PMU_TABLE_END) ; i++)
		__raw_writel(exynos_pmu_config[i].val[mode],
				exynos_pmu_config[i].reg);

	exynos3250_init_pmu();

	if (mode == SYS_SLEEP) {
		__raw_writel(0x00000BB8, EXYNOS3_XUSBXTI_DURATION);
		__raw_writel(0x00000BB8, EXYNOS3_XXTI_DURATION);
		__raw_writel(0x00001D4C, EXYNOS3_EXT_REGULATOR_DURATION);
		__raw_writel(0x00001D4C, EXYNOS3_EXT_REGULATOR_COREBLK_DURATION);
	}
}

void exynos_xxti_sys_powerdown(bool enable)
{
	unsigned int value;
	void __iomem *base;

	base = EXYNOS3_XXTI_SYS_PWR_REG;

	value = __raw_readl(base);

	if (enable)
		value |= EXYNOS_SYS_PWR_CFG;
	else
		value &= ~EXYNOS_SYS_PWR_CFG;

	__raw_writel(value, base);
}

void exynos_reset_assert_ctrl(bool on) {}

void exynos_l2_common_pwr_ctrl(void)
{
	unsigned int i;
	unsigned int option;

	for (i = 0; i < CLUSTER_NUM; i++) {
		if ((__raw_readl(EXYNOS_COMMON_STATUS(i)) & 0x3) == 0) {
			option = __raw_readl(EXYNOS_COMMON_CONFIGURATION(i));
			option |= EXYNOS_L2_COMMON_PWR_EN;
			__raw_writel(option, EXYNOS_COMMON_CONFIGURATION(i));
		}
	}
}

void exynos_set_core_flag(void)
{
	int cluster_id = (read_cpuid_mpidr() >> 8) & 0xf;

	if (cluster_id)
		__raw_writel(ARM, EXYNOS_IROM_DATA2);
	else
		__raw_writel(KFC, EXYNOS_IROM_DATA2);
}


void exynos_pmu_wdt_control(bool on, unsigned int pmu_wdt_reset_type)
{
	unsigned int value;

	/*
	 * When SYS_WDTRESET is set, watchdog timer reset request is ignored
	 * by power management unit.
	 */
	if (pmu_wdt_reset_type == PMU_WDT_RESET_TYPE0) {
		pmu_wdt_reset_type = EXYNOS_SYS_WDTRESET;
	} else {
		pr_err("Failed to %s pmu wdt reset\n",
				on ? "enable" : "disable");
		return;
	}
	value = __raw_readl(EXYNOS_AUTOMATIC_WDT_RESET_DISABLE);
	if (on)
		value &= ~pmu_wdt_reset_type;
	else
		value |= pmu_wdt_reset_type;
	__raw_writel(value, EXYNOS_AUTOMATIC_WDT_RESET_DISABLE);
	value = __raw_readl(EXYNOS_MASK_WDT_RESET_REQUEST);
	if (on)
		value &= ~pmu_wdt_reset_type;
	else
		value |= pmu_wdt_reset_type;
	__raw_writel(value, EXYNOS_MASK_WDT_RESET_REQUEST);
}

static int __init exynos_pmu_init(void)
{
	unsigned int value;

	if (soc_is_exynos3250() || soc_is_exynos3472()) {
		/*
		* To prevent form issuing new bus request form L2 memory system
		* If core status is power down, should be set '1' to L2  power down
		*/
		value = __raw_readl(EXYNOS3_ARM_COMMON_OPTION);
		value |= EXYNOS3_OPTION_SKIP_DEACTIVATE_ACEACP_IN_PWDN;
		__raw_writel(value, EXYNOS3_ARM_COMMON_OPTION);

		/* Enable USE_STANDBY_WFI for all CORE */
		__raw_writel(EXYNOS3_USE_STANDBY_WFI_ALL,
				EXYNOS_CENTRAL_SEQ_OPTION);

		/*
		* Set PSHOLD port for ouput high
		*/
		value = __raw_readl(EXYNOS_PS_HOLD_CONTROL);
		value |= EXYNOS_PS_HOLD_OUTPUT_HIGH;
		__raw_writel(value, EXYNOS_PS_HOLD_CONTROL);
		/*
		 * Enable signal for PSHOLD port
		 */
		value = __raw_readl(EXYNOS_PS_HOLD_CONTROL);
		value |= EXYNOS_PS_HOLD_EN;
		__raw_writel(value, EXYNOS_PS_HOLD_CONTROL);

		exynos_pmu_config = exynos3250_pmu_config;
		pr_info("EXYNOS3250 PMU Initialize\n");
	} else {
		pr_info("EXYNOS: PMU not supported\n");
	}

	return 0;
}
arch_initcall(exynos_pmu_init);
