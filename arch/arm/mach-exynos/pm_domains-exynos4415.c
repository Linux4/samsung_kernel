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

#include <plat/pm.h>

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

#define CLEAR_AND_SET_REG_BITS(_clr, _set, _reg)	\
{							\
	unsigned int _tmp = __raw_readl(_reg);		\
	__raw_writel((_tmp & ~(_clr)) | (_set), _reg);	\
}

enum spd_disp_index {
	ID_SPD_TVMIXER,
	ID_SPD_HDMI,
};

static struct exynos_pm_domain pm_spd_disp_exynos4415[] = {
	[ID_SPD_TVMIXER] = EXYNOS_SUB_GPD(true, "spd-tvmixer", NULL, false),
	[ID_SPD_HDMI] = EXYNOS_SUB_GPD(true, "spd-hdmi", NULL, false),
	EXYNOS_SUB_GPD(true, NULL, NULL, false),
};

static int exynos_pd_power_off_custom(struct exynos_pm_domain *pd, int power_flags);

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

static struct sleep_save exynos_mfc_clock_save[] = {
	SAVE_ITEM(EXYNOS4415_CLKGATE_IP_MFC),
};

static int exynos_pd_mfc_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s post power on\n", pd->name);
	s3c_pm_do_restore_core(exynos_mfc_clock_save,
			ARRAY_SIZE(exynos_mfc_clock_save));

	return 0;
}

static int exynos_pd_mfc_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power on\n", pd->name);
	SET_REG_BITS(0x1F, EXYNOS4415_CLKGATE_IP_MFC);

	return 0;
}

static int exynos_pd_mfc_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s post power off\n", pd->name);
	s3c_pm_do_restore_core(exynos_mfc_clock_save,
			ARRAY_SIZE(exynos_mfc_clock_save));

	return 0;
}

/* exynos_pd_mfc_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable clks.
 */
static int exynos_pd_mfc_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power off\n", pd->name);
	s3c_pm_do_save(exynos_mfc_clock_save,
			ARRAY_SIZE(exynos_mfc_clock_save));
	SET_REG_BITS(0x1F, EXYNOS4415_CLKGATE_IP_MFC);

	return 0;
}

/* pre[on/off] sequence is same for mfc */
static struct exynos_pd_callback cb_pd_mfc = {
	.name = "pd-mfc",
	.off_pre = exynos_pd_mfc_power_off_pre,
	.off_post = exynos_pd_mfc_power_off_post,
	.on_pre = exynos_pd_mfc_power_on_pre,
	.on_post = exynos_pd_mfc_power_on_post,
};

static struct sleep_save exynos_lcd_clock_save[] = {
	SAVE_ITEM(EXYNOS4415_CLKGATE_IP_LCD),
};

static int exynos_pd_lcd_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s post power on\n", pd->name);
	s3c_pm_do_restore_core(exynos_lcd_clock_save,
			ARRAY_SIZE(exynos_lcd_clock_save));

	return 0;
}

static int exynos_pd_lcd_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power on\n", pd->name);
	SET_REG_BITS(0x3F, EXYNOS4415_CLKGATE_IP_LCD);

	CLEAR_REG_BITS(0x3 << 3, EXYNOS4415_CLKGATE_SCLK_LCD);
	return 0;
}

static int exynos_pd_lcd_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s post power off\n", pd->name);
	s3c_pm_do_restore_core(exynos_lcd_clock_save,
			ARRAY_SIZE(exynos_lcd_clock_save));

	return 0;
}

/* exynos_pd_lcd_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable clks.
 */
static int exynos_pd_lcd_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power off\n", pd->name);
	s3c_pm_do_save(exynos_lcd_clock_save,
			ARRAY_SIZE(exynos_lcd_clock_save));
	SET_REG_BITS(0x3F, EXYNOS4415_CLKGATE_IP_LCD);

	return 0;
}

/* pre[on/off] sequence is same for lcd */
static struct exynos_pd_callback cb_pd_lcd = {
	.name = "pd-lcd",
	.off_pre = exynos_pd_lcd_power_off_pre,
	.off_post = exynos_pd_lcd_power_off_post,
	.on_pre = exynos_pd_lcd_power_on_pre,
	.on_post = exynos_pd_lcd_power_on_post,
};

static struct sleep_save exynos_cam_clock_save[] = {
	SAVE_ITEM(EXYNOS4415_CLKGATE_IP_CAM),
};

static int exynos_pd_cam_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s post power on\n", pd->name);
	s3c_pm_do_restore_core(exynos_cam_clock_save,
			ARRAY_SIZE(exynos_cam_clock_save));

	return 0;
}

static int exynos_pd_cam_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power off\n", pd->name);
	SET_REG_BITS(0x570FFF, EXYNOS4415_CLKGATE_IP_CAM);

	return 0;
}

static int exynos_pd_cam_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s post power off\n", pd->name);
	s3c_pm_do_restore_core(exynos_cam_clock_save,
			ARRAY_SIZE(exynos_cam_clock_save));

	return 0;
}

/* exynos_pd_cam_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable clks.
 * check LPI options
 */
static int exynos_pd_cam_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power off\n", pd->name);
	s3c_pm_do_save(exynos_cam_clock_save,
			ARRAY_SIZE(exynos_cam_clock_save));
	SET_REG_BITS(0x570FFF, EXYNOS4415_CLKGATE_IP_CAM);

	return 0;
}

/* pre[on/off] sequence is same for cam */
static struct exynos_pd_callback cb_pd_cam = {
	.name = "pd-cam",
	.off_post = exynos_pd_cam_power_off_post,
	.off_pre = exynos_pd_cam_power_off_pre,
	.on_post = exynos_pd_cam_power_on_post,
	.on_pre = exynos_pd_cam_power_on_pre,
};

static struct sleep_save exynos_tv_clock_save[] = {
	SAVE_ITEM(EXYNOS4415_CLKGATE_IP_TV),
};

static int exynos_pd_tv_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s post power on\n", pd->name);
	s3c_pm_do_restore_core(exynos_tv_clock_save,
			ARRAY_SIZE(exynos_tv_clock_save));

	return 0;
}

static int exynos_pd_tv_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power on\n", pd->name);
	SET_REG_BITS(0x3B, EXYNOS4415_CLKGATE_IP_TV);

	return 0;
}

static int exynos_pd_tv_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s post power off\n", pd->name);
	s3c_pm_do_restore_core(exynos_tv_clock_save,
			ARRAY_SIZE(exynos_tv_clock_save));

	return 0;
}

/* exynos_pd_tv_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable clks.
 */
static int exynos_pd_tv_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power off\n", pd->name);
	s3c_pm_do_save(exynos_tv_clock_save,
			ARRAY_SIZE(exynos_tv_clock_save));
	SET_REG_BITS(0x3B, EXYNOS4415_CLKGATE_IP_TV);

	return 0;
}

/* pre[on/off] sequence is same for tv */
static struct exynos_pd_callback cb_pd_tv = {
	.name = "pd-tv",
	.off_pre = exynos_pd_tv_power_off_pre,
	.off_post = exynos_pd_tv_power_off_post,
	.on_pre = exynos_pd_tv_power_on_pre,
	.on_post = exynos_pd_tv_power_on_post,
};

/* exynos_pd_isp1_power_off_pre - setup before power off.
 * @pd: power domain.
 */
static int exynos_pd_isp1_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power off\n", pd->name);
	CLEAR_REG_BITS(0x1, EXYNOS4415_CMU_RESET_ISP1_SYS_PWR_REG);
	CLEAR_REG_BITS(0x1, EXYNOS4415_CLKSRC_CMU_ISP1);
	CLEAR_AND_SET_REG_BITS(0x3, 0x2, EXYNOS4415_ISP1_OPTION);
	return 0;
}

/* exynos_pd_isp1_power_on_pre - setup before power on.
 * @pd: power domain.
 */
static int exynos_pd_isp1_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power on\n", pd->name);
	CLEAR_AND_SET_REG_BITS(0x3, 0x2, EXYNOS4415_ISP1_OPTION);
	return 0;
}

static struct exynos_pd_callback cb_pd_isp1 = {
	.name = "pd-isp1",
	.off_pre = exynos_pd_isp1_power_off_pre,
	.off = exynos_pd_power_off_custom,
	.on_pre = exynos_pd_isp1_power_on_pre,
};

/* exynos_pd_isp0_power_off_pre - setup before power off.
 * @pd: power domain.
 */
static int exynos_pd_isp0_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power off\n", pd->name);
	CLEAR_REG_BITS(0x1, EXYNOS4415_CMU_RESET_ISP0_SYS_PWR_REG);
	SET_REG_BITS((1 << 18), EXYNOS4415_CENTRAL_SEQ_OPTION);
	__raw_writel(0x10000, EXYNOS4415_ISP_ARM_OPTION);
	SET_REG_BITS(0x1, EXYNOS4415_ISP_ARM_SYS_PWR);
	CLEAR_REG_BITS(0x11, EXYNOS4415_CLKSRC_CMU_ISP0);
	CLEAR_AND_SET_REG_BITS(0x3, 0x2, EXYNOS4415_ISP0_OPTION);
	return 0;
}

/* exynos_pd_isp0_power_off_post - setup after power off.
 * @pd: power domain.
 */
static int exynos_pd_isp0_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s post power off\n", pd->name);
	CLEAR_REG_BITS(0x1, EXYNOS4415_ISP_ARM_CONFIGURATION);
	return 0;
}

/* exynos_pd_isp0_power_on_pre - setup before power on.
 * @pd: power domain.
 */
static int exynos_pd_isp0_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power on\n", pd->name);
	CLEAR_AND_SET_REG_BITS(0x3, 0x2, EXYNOS4415_ISP0_OPTION);
	return 0;
}

/* exynos_pd_isp0_power_on_post - setup after power on.
 * @pd: power domain.
 */
static int exynos_pd_isp0_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s post power on\n", pd->name);
	SET_REG_BITS(0x1, EXYNOS4415_ISP_ARM_CONFIGURATION);
	return 0;
}

static struct exynos_pd_callback cb_pd_isp0 = {
	.name = "pd-isp0",
	.off_pre = exynos_pd_isp0_power_off_pre,
	.off = exynos_pd_power_off_custom,
	.off_post = exynos_pd_isp0_power_off_post,
	.on_pre = exynos_pd_isp0_power_on_pre,
	.on_post = exynos_pd_isp0_power_on_post,
};

static struct exynos_pm_domain pm_spd_isp0_exynos4415[] = {
	EXYNOS_MASTER_GPD(true, EXYNOS4415_ISP1_CONFIGURATION, "pd-isp1",
		&cb_pd_isp1, NULL, false),
	EXYNOS_MASTER_GPD(true, NULL, NULL, NULL, NULL, false),
};

static int force_power_down(struct exynos_pm_domain *pd, int power_flags)
{
	unsigned long timeout;

	if (strncmp(pd->name, "pd-isp1", 7) == 0) {
		SET_REG_BITS(0x1C80, EXYNOS4X12_LPI_MASK0);
		CLEAR_REG_BITS(0x1, EXYNOS4415_CMU_RESET_ISP1_SYS_PWR_REG);
		CLEAR_REG_BITS(0x1, EXYNOS4415_CLKSRC_CMU_ISP1);
	} else if (strncmp(pd->name, "pd-isp0", 7) == 0) {
		SET_REG_BITS(0x66340, EXYNOS4X12_LPI_MASK0);
		CLEAR_REG_BITS(0x1, EXYNOS4415_CMU_RESET_ISP0_SYS_PWR_REG);
		CLEAR_REG_BITS((1 << 18), EXYNOS4415_CENTRAL_SEQ_OPTION);
		__raw_writel(0x0, EXYNOS4415_ISP_ARM_OPTION);
		SET_REG_BITS(0x1, EXYNOS4415_ISP_ARM_SYS_PWR);
		CLEAR_REG_BITS(0x11, EXYNOS4415_CLKSRC_CMU_ISP0);
	} else {
		pr_err("PM_DOMAIN : %s invalid pd name for %s\n", pd->name, __func__);
		return -EINVAL;
	}

	__raw_writel(0x0102, pd->base+0x8);
	__raw_writel(power_flags, pd->base);

	timeout = 5000;
	while ((__raw_readl(pd->base+0x4) & EXYNOS_INT_LOCAL_PWR_EN) != power_flags) {
		if (timeout == 0) {
			pr_err("%s@%p: %08x, %08x, %08x\n",
					pd->genpd.name,
					pd->base,
					__raw_readl(pd->base),
					__raw_readl(pd->base+4),
					__raw_readl(pd->base+8));
			pr_err(PM_DOMAIN_PREFIX "%s cannot control power, timeout\n", pd->name);
			return -ETIMEDOUT;
		}
		--timeout;
		cpu_relax();
		usleep_range(8, 10);
	}

	return 0;
}

static int exynos_pd_power_off_custom(struct exynos_pm_domain *pd, int power_flags)
{
	unsigned long timeout;
	int ret;

	if (unlikely(!pd))
		return -EINVAL;

	mutex_lock(&pd->access_lock);
	if (likely(pd->base)) {
		/* sc_feedback to OPTION register */
		__raw_writel(0x0102, pd->base+0x8);

		/* on/off value to CONFIGURATION register */
		__raw_writel(power_flags, pd->base);

		/* Wait max 50ms */
		timeout = 5000;
		/* check STATUS register */
		while ((__raw_readl(pd->base+0x4) & EXYNOS_INT_LOCAL_PWR_EN) != power_flags) {
			if (timeout == 0) {
				ret = force_power_down(pd, power_flags);
				mutex_unlock(&pd->access_lock);
				return ret;
			}
			--timeout;
			cpu_relax();
			usleep_range(8, 10);
		}
		if (unlikely(timeout < 50)) {
			pr_warn(PM_DOMAIN_PREFIX "long delay found during %s is %s\n", pd->name, power_flags ? "on":"off");
			pr_warn("%s@%p: %08x, %08x, %08x\n",
					pd->name,
					pd->base,
					__raw_readl(pd->base),
					__raw_readl(pd->base+4),
					__raw_readl(pd->base+8));
		}
	}
	pd->status = power_flags;
	mutex_unlock(&pd->access_lock);

	DEBUG_PRINT_INFO("%s@%p: %08x, %08x, %08x\n",
				pd->genpd.name, pd->base,
				__raw_readl(pd->base),
				__raw_readl(pd->base+4),
				__raw_readl(pd->base+8));

	return 0;
}

/*
 * EXYNOS_MASTER_GPD(ENABLE, _BASE, NAME, PDEV, CB, SUB, BTS)
 */
static struct exynos_pm_domain pm_domain_exynos4415[] = {
	[ID_PD_CAM] = EXYNOS_MASTER_GPD(true, EXYNOS4415_CAM_CONFIGURATION,
			"pd-cam", &cb_pd_cam, NULL, false),
	[ID_PD_TV] = EXYNOS_MASTER_GPD(true, EXYNOS4415_TV_CONFIGURATION,
			"pd-tv", &cb_pd_tv, pm_spd_disp_exynos4415, false),
	[ID_PD_MFC] = EXYNOS_MASTER_GPD(true, EXYNOS4415_MFC_CONFIGURATION,
			"pd-mfc", &cb_pd_mfc, NULL, false),
	[ID_PD_G3D] = EXYNOS_MASTER_GPD(true, EXYNOS4415_G3D_CONFIGURATION,
			"pd-g3d", &cb_pd_g3d, NULL, true),
	[ID_PD_LCD0] = EXYNOS_MASTER_GPD(true, EXYNOS4415_LCD0_CONFIGURATION,
			"pd-lcd", &cb_pd_lcd, NULL, true),
	[ID_PD_MAU] = EXYNOS_MASTER_GPD(false, EXYNOS4415_MAUDIO_CONFIGURATION,
			"pd-maudio", NULL, NULL, false),
	[ID_PD_ISP0] = EXYNOS_MASTER_GPD(true, EXYNOS4415_ISP0_CONFIGURATION,
			"pd-isp0", &cb_pd_isp0, pm_spd_isp0_exynos4415, false),
	EXYNOS_MASTER_GPD(true, NULL, NULL, NULL, NULL, false),
};

/* Linking device to power domain */
static struct exynos_device_pd_link pm_device_to_pd_exynos4415[] = {
#ifdef CONFIG_FB_MIPI_DSIM
	{&pm_domain_exynos4415[ID_PD_LCD0], &s5p_device_mipi_dsim0, true},
#endif
	{&pm_domain_exynos4415[ID_PD_LCD0], &s5p_device_fimd1, true},
	{&pm_domain_exynos4415[ID_PD_LCD0], &SYSMMU_PLATDEV(fimd0), true},
	{&pm_domain_exynos4415[ID_PD_LCD0], &SYSMMU_PLATDEV(fimd1), true},
#ifdef CONFIG_S5P_DP
	{&pm_domain_exynos4415[ID_PD_LCD0], &s5p_device_dp, true},
#endif
	{&pm_domain_exynos4415[ID_PD_G3D], &exynos4_device_g3d, true},
#ifdef CONFIG_S5P_DEV_MFC
	{&pm_domain_exynos4415[ID_PD_MFC], &s5p_device_mfc, true},
	{&pm_domain_exynos4415[ID_PD_MFC], &SYSMMU_PLATDEV(mfc_lr), true},
#endif
#ifdef CONFIG_S5P_DEV_FIMC0
	{&pm_domain_exynos4415[ID_PD_CAM], &s5p_device_fimc0, true},
	{&pm_domain_exynos4415[ID_PD_CAM], &SYSMMU_PLATDEV(fimc0), true},
#endif
#ifdef CONFIG_S5P_DEV_FIMC1
	{&pm_domain_exynos4415[ID_PD_CAM], &s5p_device_fimc1, true},
	{&pm_domain_exynos4415[ID_PD_CAM], &SYSMMU_PLATDEV(fimc1), true},
#endif
#ifdef CONFIG_S5P_DEV_FIMC2
	{&pm_domain_exynos4415[ID_PD_CAM], &s5p_device_fimc2, true},
	{&pm_domain_exynos4415[ID_PD_CAM], &SYSMMU_PLATDEV(fimc2), true},
#endif
#ifdef CONFIG_S5P_DEV_FIMC3
	{&pm_domain_exynos4415[ID_PD_CAM], &s5p_device_fimc3, true},
	{&pm_domain_exynos4415[ID_PD_CAM], &SYSMMU_PLATDEV(fimc3), true},
#endif
#ifdef CONFIG_S5P_DEV_TV
	{&pm_spd_disp_exynos4415[ID_SPD_HDMI], &s5p_device_hdmi, true},
	{&pm_spd_disp_exynos4415[ID_SPD_TVMIXER], &s5p_device_mixer, true},
	{&pm_spd_disp_exynos4415[ID_SPD_TVMIXER], &SYSMMU_PLATDEV(tv), true},
#endif
#ifdef CONFIG_EXYNOS4_DEV_JPEG
	{&pm_domain_exynos4415[ID_PD_CAM], &s5p_device_jpeg, true},
	{&pm_domain_exynos4415[ID_PD_CAM], &SYSMMU_PLATDEV(jpeg), true},
#endif
#if defined(CONFIG_EXYNOS4_DEV_FIMC_IS) && defined(CONFIG_VIDEO_EXYNOS_FIMC_IS)
	{&pm_domain_exynos4415[ID_PD_ISP0], &exynos_device_fimc_is_sensor0, true},
	{&pm_domain_exynos4415[ID_PD_ISP0], &exynos_device_fimc_is_sensor1, true},
	{&pm_spd_isp0_exynos4415[0], &exynos4_device_fimc_is, true},
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
