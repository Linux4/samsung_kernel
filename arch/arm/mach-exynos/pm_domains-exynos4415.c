/*
 * Exynos4415 power domain support.
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <mach/pm_domains_v2.h>
#include <mach/regs-clock-exynos4415.h>

#define SET_REG_BITS(_val, _reg)			\
{							\
	unsigned int _tmp = __raw_readl(_reg);		\
	__raw_writel(_tmp | (_val), (_reg));		\
}

#define CLEAR_REG_BITS(_val, _reg)			\
{							\
	unsigned int _tmp = __raw_readl(_reg);		\
	__raw_writel(_tmp & ~(_val), (_reg));		\
}

/* exynos_pd_g3d_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable clks.
 */
static int exynos_pd_g3d_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power off\n", pd->name);
	SET_REG_BITS(0x3, EXYNOS4415_CLKGATE_IP_G3D);

	return 0;
}

/* pre[on/off] sequence is same for g3d */
static struct exynos_pd_callback cb_pd_g3d = {
	.name = "pd-g3d",
	.off_pre = exynos_pd_g3d_power_off_pre,
	.on_pre = exynos_pd_g3d_power_off_pre,
};

/* exynos_pd_mfc_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable clks.
 */
static int exynos_pd_mfc_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power off\n", pd->name);
	SET_REG_BITS(0x1F, EXYNOS4415_CLKGATE_IP_MFC);

	return 0;
}

/* pre[on/off] sequence is same for mfc */
static struct exynos_pd_callback cb_pd_mfc = {
	.name = "pd-mfc",
	.off_pre = exynos_pd_mfc_power_off_pre,
	.on_pre = exynos_pd_mfc_power_off_pre,
};

/* exynos_pd_lcd_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable clks.
 */
static int exynos_pd_lcd_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power off\n", pd->name);
	SET_REG_BITS(0x3F, EXYNOS4415_CLKGATE_IP_LCD);

	return 0;
}

/* pre[on/off] sequence is same for lcd */
static struct exynos_pd_callback cb_pd_lcd = {
	.name = "pd-disp",
	.off_pre = exynos_pd_lcd_power_off_pre,
	.on_pre = exynos_pd_lcd_power_off_pre,
};

/* exynos_pd_cam_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable clks.
 * check LPI options
 */
static int exynos_pd_cam_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power off\n", pd->name);
	SET_REG_BITS(0x570FFF, EXYNOS4415_CLKGATE_IP_CAM);

	return 0;
}

/* pre[on/off] sequence is same for cam */
static struct exynos_pd_callback cb_pd_cam = {
	.name = "pd-cam",
	.off_pre = exynos_pd_cam_power_off_pre,
	.on_pre = exynos_pd_cam_power_off_pre,
};

/* exynos_pd_tv_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable clks.
 */
static int exynos_pd_tv_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power off\n", pd->name);
	SET_REG_BITS(0x3B, EXYNOS4415_CLKGATE_IP_TV);

	return 0;
}

/* pre[on/off] sequence is same for tv */
static struct exynos_pd_callback cb_pd_tv = {
	.name = "pd-tv",
	.off_pre = exynos_pd_tv_power_off_pre,
	.on_pre = exynos_pd_tv_power_off_pre,
};

/* exynos_pd_isp1_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable clks.
 */
static int exynos_pd_isp1_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power off\n", pd->name);
	SET_REG_BITS(0x3FFFFFFF, EXYNOS4415_CLKGATE_IP_ISP1_SUB0);
	/* Change Main CLK to OSCCLK */
	CLEAR_REG_BITS(1, EXYNOS4415_CLKSRC_ISP1);

	return 0;
}

/* exynos_pd_isp1_power_on_post - setup before power on.
 * @pd: power domain.
 *
 * Clock Divider Reconfiguration
 */
static int exynos_pd_isp1_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s post power on\n", pd->name);

	/* Clock Divider Reconfiguration (if exists) */
	/* Restore ACLK (USER_MUX) if necessary */

	return 0;
}

static struct exynos_pd_callback cb_pd_isp1 = {
	.name = "pd-isp1",
	.off_pre = exynos_pd_isp1_power_off_pre,
	.on_post = exynos_pd_isp1_power_on_post,
};

/* exynos_pd_isp0_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable clks.
 */
static int exynos_pd_isp0_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power off\n", pd->name);

	SET_REG_BITS(0xFFFFFFFF, EXYNOS4415_CLKGATE_IP_ISP0_SUB0);
	SET_REG_BITS(0x7FFFFFFF, EXYNOS4415_CLKGATE_IP_ISP0_SUB1);
	SET_REG_BITS(0xFFFFFFFF, EXYNOS4415_CLKGATE_IP_ISP0_SUB2);
	SET_REG_BITS(0x7FF, EXYNOS4415_CLKGATE_IP_ISP0_SUB3);
	SET_REG_BITS(0x3, EXYNOS4415_CLKGATE_IP_ISP0_SUB4);
	/* Change Main CLK to OSCCLK */
	CLEAR_REG_BITS(1 | (1 << 4), EXYNOS4415_CLKSRC_ISP0);

	return 0;
}

/* exynos_pd_isp0_power_off_post - setup after power off.
 * @pd: power domain.
 *
 * enable clks.
 */
static int exynos_pd_isp0_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power off\n", pd->name);

	/* Reset Assertion CPU */
	CLEAR_REG_BITS(0x1, EXYNOS4415_ISP_ARM_CONFIGURATION);

	return 0;
}

/* exynos_pd_isp0_power_on_post - setup before power on.
 * @pd: power domain.
 *
 * Clock Divider Reconfiguration
 */
static int exynos_pd_isp0_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s post power on\n", pd->name);

	/* Reset Release CPU */
	SET_REG_BITS(0x1, EXYNOS4415_ISP_ARM_CONFIGURATION);

	/* Clock Divider Reconfiguration (if exists) */
	/* Restore ACLK (USER_MUX) if necessary */

	return 0;
}

static struct exynos_pd_callback cb_pd_isp0 = {
	.name = "pd-isp0",
	.off_pre = exynos_pd_isp0_power_off_pre,
	.off_post = exynos_pd_isp0_power_off_post,
	.on_post = exynos_pd_isp0_power_on_post,
};

static struct exynos_pm_domain pm_spd_isp0_exynos4415[] = {
	EXYNOS_MASTER_GPD(true, EXYNOS4415_ISP1_CONFIGURATION, "pd-isp1",
		&cb_pd_isp1, NULL, false),
};

/*
 * EXYNOS_MASTER_GPD(ENABLE, _BASE, NAME, PDEV, CB, SUB, BTS)
 */
static struct exynos_pm_domain pm_domain_exynos4415[] = {
	[ID_PD_CAM] = EXYNOS_MASTER_GPD(true, EXYNOS4415_CAM_CONFIGURATION,
			"pd-cam", &cb_pd_cam, NULL, false),
	[ID_PD_TV] = EXYNOS_MASTER_GPD(true, EXYNOS4415_TV_CONFIGURATION,
			"pd-tv", &cb_pd_tv, NULL, false),
	[ID_PD_MFC] = EXYNOS_MASTER_GPD(true, EXYNOS4415_MFC_CONFIGURATION,
			"pd-mfc", &cb_pd_mfc, NULL, false),
	[ID_PD_G3D] = EXYNOS_MASTER_GPD(true, EXYNOS4415_G3D_CONFIGURATION,
			"pd-g3d", &cb_pd_g3d, NULL, false),
	[ID_PD_LCD0] = EXYNOS_MASTER_GPD(true, EXYNOS4415_LCD0_CONFIGURATION,
			"pd-lcd", &cb_pd_lcd, NULL, false),
	[ID_PD_MAU] = EXYNOS_MASTER_GPD(false, EXYNOS4415_MAUDIO_CONFIGURATION,
			"pd-maudio", NULL, NULL, false),
	[ID_PD_ISP0] = EXYNOS_MASTER_GPD(true, EXYNOS4415_ISP0_CONFIGURATION,
			"pd-isp0", &cb_pd_isp0, pm_spd_isp0_exynos4415, false),
	EXYNOS_MASTER_GPD(true, NULL, NULL, NULL, NULL, false),
};

/* Linking device to power domain */
static struct exynos_device_pd_link pm_device_to_pd_exynos4415[] = {
#ifdef CONFIG_FB_MIPI_DSIM
	{&pm_domain_exynos4415[ID_PD_LCD0], &s5p_device_mipi_dsim1, true},
#endif
	{&pm_domain_exynos4415[ID_PD_LCD0], &s5p_device_fimd1, true},
#ifdef CONFIG_S5P_DP
	{&pm_domain_exynos4415[ID_PD_LCD0], &s5p_device_dp, true},
#endif
	{&pm_domain_exynos4415[ID_PD_G3D], &exynos4_device_g3d, true},
#ifdef CONFIG_S5P_DEV_MFC
	{&pm_domain_exynos4415[ID_PD_MFC], &s5p_device_mfc, true},
#endif
#ifdef CONFIG_S5P_DEV_FIMC0
	{&pm_domain_exynos4415[ID_PD_CAM], &s5p_device_fimc0, true},
#endif
#ifdef CONFIG_S5P_DEV_FIMC1
	{&pm_domain_exynos4415[ID_PD_CAM], &s5p_device_fimc1, true},
#endif
#ifdef CONFIG_S5P_DEV_FIMC2
	{&pm_domain_exynos4415[ID_PD_CAM], &s5p_device_fimc2, true},
#endif
#ifdef CONFIG_S5P_DEV_FIMC3
	{&pm_domain_exynos4415[ID_PD_CAM], &s5p_device_fimc3, true},
#endif
#ifdef CONFIG_S5P_DEV_TV
	{&pm_domain_exynos4415[ID_PD_TV], &s5p_device_mixer, true},
	{&pm_domain_exynos4415[ID_PD_TV], &s5p_device_hdmi, true},
#endif
	{NULL, NULL, 0}
};

struct exynos_pm_domain *exynos4415_pm_domain(void)
{
	return pm_domain_exynos4415;
}

struct exynos_device_pd_link *exynos4415_device_pd_link(void)
{
	return pm_device_to_pd_exynos4415;
}
