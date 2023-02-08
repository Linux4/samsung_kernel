/* linux/arch/arm/mach-exynos/pmu-cp.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
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
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <mach/regs-bts.h>
#include <mach/regs-clock.h>
#include <mach/pmu.h>
#include <mach/gpio.h>
#include <asm/cputype.h>
#include <plat/gpio-cfg.h>

/* #define DEBUG_SLEEP_WITHOUT_CP */

struct exynos_pmu_cp_conf {
	void __iomem *reg;
	unsigned int val[NUM_CP_MODE];
};

static struct exynos_pmu_cp_conf exynos3470_pmu_cp_config[] = {
	/* { .reg = address, .val = { CP_POWER_ON, CP_RESET, CP_POWER_OFF } */
	{ EXYNOS3470_EXT_REGULATOR_CPBLK_LOWPWR,	{ 0x1, 0x1, 0x0 } },
	{ EXYNOS3470_TOP_ASB_RESET_CPBLK_LOWPWR,	{ 0x1, 0x0, 0x0 } },
	{ EXYNOS3470_TOP_ASB_ISOLATION_CPBLK_LOWPWR,	{ 0x1, 0x1, 0x0 } },
	{ EXYNOS3470_LOGIC_RESET_CPBLK_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS3470_CENTRAL_SEQ_CONFIGURATION_CPBLK,	{ 0x10000, 0x0, 0x0 } },
	{ PMU_TABLE_END,},
};

static struct exynos_pmu_conf exynos3470_pmu_mif_config[] = {
	/* LPM3 */
	{ EXYNOS4X12_LPDDR_PHY_DLL_LOCK_LOWPWR,		{ 0x1, 0x1, 0x1 } },
	{ EXYNOS4X12_CMU_RESET_COREBLK_LOWPWR,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4X12_MPLLUSER_SYSCLK_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4X12_TOP_RETENTION_COREBLK_LOWPWR,	{ 0x1, 0x0, 0x1 } },
	{ EXYNOS4X12_TOP_PWR_COREBLK_LOWPWR,		{ 0x3, 0x0, 0x3 } },
	{ EXYNOS4X12_LOGIC_RESET_COREBLK_LOWPWR,	{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4X12_OSCCLK_GATE_COREBLK_LOWPWR,	{ 0x1, 0x0, 0x1 } },
	{ EXYNOS3470_EXT_REGULATOR_COREBLK_LOWPWR,	{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4X12_TOP_ASB_RESET_LOWPWR,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4X12_TOP_ASB_ISOLATION_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS3470_TOP_ASB_RESET_COREBLK_LOWPWR,	{ 0x1, 0x1, 0x0 } },
	{ EXYNOS3470_TOP_ASB_ISOLATION_COREBLK_LOWPWR,  { 0x1, 0x0, 0x0 } },
	{ PMU_TABLE_END,},
};

/* reset cp */
int exynos4_cp_reset(void)
{
	u32 cp_ctrl;
	int ret = 0;
	pr_err("mif: %s: +++\n", __func__);

	/* set sys_pwr_cfg registers */
	exynos_sys_powerdown_conf_cp(CP_RESET);

	/* assert cp_reset */
	cp_ctrl = __raw_readl(EXYNOS3470_CP_CTRL);
	cp_ctrl |= CP_RESET_SET;
	__raw_writel(cp_ctrl, EXYNOS3470_CP_CTRL);

	/* some delay*/
	cpu_relax();
	usleep_range(80, 100);

	/* need to check reset is ok or not at message box */
	pr_err("mif: %s: ---\n", __func__);
	return ret;
}

/* release cp */
int exynos4_cp_release(void)
{
	u32 cp_ctrl;
	pr_err("mif: %s: +++\n", __func__);

	cp_ctrl = __raw_readl(EXYNOS3470_CP_CTRL);
	cp_ctrl |= CP_START;

	__raw_writel(cp_ctrl, EXYNOS3470_CP_CTRL);
	pr_err("mif: %s: cp_ctrl [0x%08x] -> [0x%08x]\n",
		__func__, cp_ctrl, __raw_readl(EXYNOS3470_CP_CTRL));

	pr_err("mif: %s: ---\n", __func__);
	return 0;
}

/* Clear cp active */
int exynos4_cp_active_clear(void)
{
	u32 cp_ctrl;
	pr_err("mif: %s: +++\n", __func__);

	cp_ctrl = __raw_readl(EXYNOS3470_CP_CTRL);
	cp_ctrl |= CP_ACTIVE_CLR;

	__raw_writel(cp_ctrl, EXYNOS3470_CP_CTRL);
	pr_err("mif: %s: cp_ctrl [0x%08x] -> [0x%08x]\n",
		__func__, cp_ctrl, __raw_readl(EXYNOS3470_CP_CTRL));

	pr_err("mif: %s: ---\n", __func__);
	return 0;
}

/* clear cpu_reset_req from cp */
int exynos4_clear_cp_reset_req(void)
{
	u32 cp_ctrl;
	pr_err("mif: %s: +++\n", __func__);

	cp_ctrl = __raw_readl(EXYNOS3470_CP_CTRL);
	cp_ctrl |= CP_RESET_REQ_CLR;

	__raw_writel(cp_ctrl, EXYNOS3470_CP_CTRL);
	pr_err("mif: %s: cp_ctrl [0x%08x] -> [0x%08x]\n",
		__func__, cp_ctrl, __raw_readl(EXYNOS3470_CP_CTRL));

	pr_err("mif: %s: ---\n", __func__);
	return 0;
}

int exynos4_get_cp_power_status(void)
{
	u32 cp_state;

	cp_state = __raw_readl(EXYNOS3470_CP_CTRL);
	cp_state &= CP_PWRON;

	if (cp_state)
		return 1;
	else
		return 0;
}

static void set_bts_cp(enum cp_mode mode)
{
	struct clk *cp_clk = clk_get(NULL, "c2c");

	if (mode == CP_POWER_ON) {
		clk_enable(cp_clk);

		/* set CP read data highest priority */
		__raw_writel(0x0, EXYNOS4_READ_QOS_CONTROL);
		__raw_writel(0xffff, EXYNOS4_READ_CHANNEL_PRIORITY);
		__raw_writel(0x20, EXYNOS4_READ_TOKEN_MAX_VALUE);
		__raw_writel(0x8, EXYNOS4_READ_BW_UPPER_BOUNDARY);
		__raw_writel(0x1, EXYNOS4_READ_BW_LOWER_BOUNDARY);
		__raw_writel(0x8, EXYNOS4_READ_INITIAL_TOKEN_VALUE);
		__raw_writel(0x1, EXYNOS4_READ_QOS_CONTROL);

		/* set CP write data highest priority */
		__raw_writel(0x0, EXYNOS4_WRITE_QOS_CONTROL);
		__raw_writel(0xffff, EXYNOS4_WRITE_CHANNEL_PRIORITY);
		__raw_writel(0x20, EXYNOS4_WRITE_TOKEN_MAX_VALUE);
		__raw_writel(0x8, EXYNOS4_WRITE_BW_UPPER_BOUNDARY);
		__raw_writel(0x1, EXYNOS4_WRITE_BW_LOWER_BOUNDARY);
		__raw_writel(0x8, EXYNOS4_WRITE_INITIAL_TOKEN_VALUE);
		__raw_writel(0x1, EXYNOS4_WRITE_QOS_CONTROL);
	} else {
		clk_disable(cp_clk);
	}
}

int exynos4_set_cp_power_onoff(enum cp_mode mode)
{
	u32 cp_ctrl;
	int ret = 0;
	pr_err("mif: %s: mode[%d]\n", __func__, mode);

	/* set sys_pwr_cfg registers */
	exynos_sys_powerdown_conf_cp(mode);

	/* set power on/off */
	cp_ctrl = __raw_readl(EXYNOS3470_CP_CTRL);

	if (mode == CP_POWER_ON) {
		if (cp_ctrl & CP_PWRON) {
			pr_err("mif: %s: CP already ON!!!\n", __func__);
			return -1;
		}
		cp_ctrl |= (CP_PWRON | CP_START);
		__raw_writel(cp_ctrl, EXYNOS3470_CP_CTRL);
		set_bts_cp(mode);
	} else {
		set_bts_cp(mode);
		cp_ctrl &= ~CP_PWRON;
		__raw_writel(cp_ctrl, EXYNOS3470_CP_CTRL);
	}

	return ret;
}

void exynos_sys_powerdown_conf_mif(enum sys_powerdown mode)
{
	u32 i;
#ifdef DEBUG_SLEEP_WITHOUT_CP
	u32 cp_ctrl;
#endif

	for (i = 0; (exynos3470_pmu_mif_config[i].reg != PMU_TABLE_END) ; i++)
		__raw_writel(exynos3470_pmu_mif_config[i].val[mode],
				exynos3470_pmu_mif_config[i].reg);

#ifdef DEBUG_SLEEP_WITHOUT_CP
	/* cp disable for test purpose */
	cp_ctrl = __raw_readl(EXYNOS3470_CP_CTRL);
	cp_ctrl &= ~ENABLE_CP;
	exynos4_set_cp_power_onoff(CP_POWER_OFF);
#endif
}

void exynos_sys_powerdown_conf_cp(enum cp_mode mode)
{
	u32 i;

	pr_err("mif: %s: mode [%d]\n", __func__, mode);

	for (i = 0; (exynos3470_pmu_cp_config[i].reg != PMU_TABLE_END) ; i++)
		__raw_writel(exynos3470_pmu_cp_config[i].val[mode],
				exynos3470_pmu_cp_config[i].reg);
}

int exynos4_pmu_cp_init(void)
{
	u32 cp_ctrl;
	int ret = 0;
	unsigned int gpio;

        pr_info("%s\n", __func__);
	if (samsung_rev() >= EXYNOS3470_REV_2_0) {
		gpio = EXYNOS4_GPM2(3);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(1));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_pd_cfg(gpio, S5P_GPIO_PD_OUTPUT1);
		s5p_gpio_set_data(gpio, 1);
		s5p_gpio_set_pd_pull(gpio, S3C_GPIO_PULL_NONE);
	}

	cp_ctrl = __raw_readl(EXYNOS3470_CP_CTRL);
	cp_ctrl |= MASK_CP_PWRDN_DONE;
	__raw_writel(cp_ctrl, EXYNOS3470_CP_CTRL);

#ifdef DEBUG_SLEEP_WITHOUT_CP
	/* test purpose */
	exynos4_set_cp_power_onoff(CP_POWER_ON);

	exynos4_cp_reset();

	exynos4_clear_cp_reset_req();

	exynos4_set_cp_power_onoff(CP_POWER_OFF);

	exynos4_set_cp_power_onoff(CP_POWER_ON);
#endif

	pr_err("mif: %s: ---\n", __func__);
	return ret;
}

