/*
 * Exynos Generic power domain support.
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm. Support for both device-tree and non-device-tree
 * based power domain support is included.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/suspend.h>

#include <plat/pm.h>

#include <mach/pm_domains_v2.h>
#include <mach/bts.h>
#include <mach/regs-clock-exynos5260.h>
#include <mach/devfreq.h>
#include <mach/rcg.h>

static struct sleep_save exynos_pd_g3d_clk_save[] = {
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_G3D),
};

static int exynos_pd_g3d_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s post power on\n", pd->name);

	s3c_pm_do_restore_core(exynos_pd_g3d_clk_save,
			ARRAY_SIZE(exynos_pd_g3d_clk_save));

	exynos_rcg_enable(RCG_G3D);

	return 0;
}

/* exynos_pd_g3d_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable clks.
 */
static int exynos_pd_g3d_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power off\n", pd->name);

	s3c_pm_do_save(exynos_pd_g3d_clk_save,
			ARRAY_SIZE(exynos_pd_g3d_clk_save));

	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_ACLK_G3D);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_PCLK_G3D);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_SCLK_G3D);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_IP_G3D);

	return 0;
}

static struct exynos_pd_callback cb_pd_g3d = {
	.name = "pd-g3d",
	.on_post = exynos_pd_g3d_power_on_post,
	.off_pre = exynos_pd_g3d_power_off_pre,
};

static struct sleep_save exynos_pd_mfc_clk_save[] = {
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_MFC),
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_MFC_SECURE_SMMU2_MFC),
};

static int exynos_pd_mfc_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s post power on\n", pd->name);

	s3c_pm_do_restore_core(exynos_pd_mfc_clk_save,
			ARRAY_SIZE(exynos_pd_mfc_clk_save));

	exynos_rcg_enable(RCG_MFC);

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

	s3c_pm_do_save(exynos_pd_mfc_clk_save,
			ARRAY_SIZE(exynos_pd_mfc_clk_save));

	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_ACLK_MFC);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_ACLK_MFC_SECURE_SMMU2_MFC);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_PCLK_MFC);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_PCLK_MFC_SECURE_SMMU2_MFC);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_IP_MFC);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_IP_MFC_SECURE_SMMU2_MFC);

	return 0;
}

static struct exynos_pd_callback cb_pd_mfc = {
	.name = "pd-mfc",
	.on_post = exynos_pd_mfc_power_on_post,
	.off_pre = exynos_pd_mfc_power_off_pre,
};

static struct sleep_save exynos_pd_disp_clk_save[] = {
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_DISP),
};

/* exynos_pd_disp_power_on_post - clean up after power on.
 * @pd: power domain.
 *
 * enable clks.
 */
static int exynos_pd_disp_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s post power on\n", pd->name);

	s3c_pm_do_restore_core(exynos_pd_disp_clk_save,
			ARRAY_SIZE(exynos_pd_disp_clk_save));

#ifdef CONFIG_ARM_EXYNOS5260_BUS_DEVFREQ
	exynos5_devfreq_disp_enable(true);
#endif

	exynos_rcg_enable(RCG_DISP);

	return 0;
}

/* exynos_pd_disp_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable clks.
 */
static int exynos_pd_disp_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power off\n", pd->name);

	s3c_pm_do_save(exynos_pd_disp_clk_save,
			ARRAY_SIZE(exynos_pd_disp_clk_save));

	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_ACLK_DISP);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_PCLK_DISP);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_SCLK_DISP0);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_SCLK_DISP1);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_IP_DISP);

#ifdef CONFIG_ARM_EXYNOS5260_BUS_DEVFREQ
	exynos5_devfreq_disp_enable(false);
#endif

	return 0;
}

static struct exynos_pd_callback cb_pd_disp = {
	.name = "pd-disp",
	.on_post = exynos_pd_disp_power_on_post,
	.off_pre = exynos_pd_disp_power_off_pre,
};

static struct sleep_save exynos_pd_gscl_clk_save[] = {
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_GSCL),
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_GSCL_FIMC),
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_GSCL_SECURE_SMMU_GSCL0),
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_GSCL_SECURE_SMMU_GSCL1),
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_GSCL_SECURE_SMMU_MSCL0),
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_GSCL_SECURE_SMMU_MSCL1),
};

static int exynos_pd_gscl_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s post power on\n", pd->name);

	s3c_pm_do_restore_core(exynos_pd_gscl_clk_save,
			ARRAY_SIZE(exynos_pd_gscl_clk_save));

	exynos_rcg_enable(RCG_GSCL);

	return 0;
}

/* exynos_pd_gscl_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable clks.
 * check LPI options
 */
static int exynos_pd_gscl_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power off\n", pd->name);

	s3c_pm_do_save(exynos_pd_gscl_clk_save,
			ARRAY_SIZE(exynos_pd_gscl_clk_save));

	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_ACLK_GSCL);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_ACLK_GSCL_FIMC);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_ACLK_GSCL_SECURE_SMMU_GSCL0);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_ACLK_GSCL_SECURE_SMMU_GSCL1);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_ACLK_GSCL_SECURE_SMMU_MSCL0);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_ACLK_GSCL_SECURE_SMMU_MSCL1);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_PCLK_GSCL);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_PCLK_GSCL_FIMC);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_PCLK_GSCL_SECURE_SMMU_GSCL0);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_PCLK_GSCL_SECURE_SMMU_GSCL1);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_PCLK_GSCL_SECURE_SMMU_MSCL0);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_PCLK_GSCL_SECURE_SMMU_MSCL1);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_SCLK_GSCL);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_SCLK_GSCL_FIMC);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_IP_GSCL_SECURE_SMMU_GSCL0);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_IP_GSCL_SECURE_SMMU_GSCL1);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_IP_GSCL_SECURE_SMMU_MSCL0);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_IP_GSCL_SECURE_SMMU_MSCL1);

	/* guide tells that LPI should be disabled when ISP is off.
	 * Because ISP is always off code does not check ISP's status.
	 * always disable LPI.
	 * This is treated as single code block.
	 */
	if (samsung_rev() < EXYNOS5260_REV_1_0) {
		unsigned int reg;

		reg = __raw_readl(EXYNOS5260_VA_PMU_GSCL+0x20);
		DEBUG_PRINT_INFO("LPI_MASK: %08x\n", reg);
		reg |= (1<<0);
		__raw_writel(reg, EXYNOS5260_VA_PMU_GSCL+0x20);

		reg = __raw_readl(EXYNOS5260_VA_PMU_GSCL+0x20);
		DEBUG_PRINT_INFO("LPI_MASK: %08x\n", reg);
	}

	return 0;
}

/* exynos_pd_gscl_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable clks.
 * check LPI options
 */
static int exynos_pd_gscl_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s post power off\n", pd->name);

	return 0;
}

static struct exynos_pd_callback cb_pd_gscl = {
	.name = "pd-gscl",
	.on_post = exynos_pd_gscl_power_on_post,
	.off_pre = exynos_pd_gscl_power_off_pre,
	.off_post = exynos_pd_gscl_power_off_post,
};

static int exynos_pd_maudio_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s post power on\n", pd->name);

	exynos_rcg_enable(RCG_AUD);

	return 0;
}

/* exynos_pd_maudio_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable clks.
 */
static int exynos_pd_maudio_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power off\n", pd->name);

	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_ACLK_AUD);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_PCLK_AUD);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_SCLK_AUD);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_IP_AUD);

	return 0;
}

static struct exynos_pd_callback cb_pd_maudio = {
	.name = "pd-maudio",
	.on_post = exynos_pd_maudio_power_on_post,
	.off_pre = exynos_pd_maudio_power_off_pre,
};

static struct sleep_save exynos_pd_g2d_clk_save[] = {
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_G2D),
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_G2D_SECURE_SSS_G2D),
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_G2D_SECURE_SLIM_SSS_G2D),
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_G2D_SECURE_SMMU_SLIM_SSS_G2D),
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_G2D_SECURE_SMMU_SSS_G2D),
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_G2D_SECURE_SMMU_G2D_G2D),
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_G2D_SECURE_SMMU_MDMA_G2D),
};

static int exynos_pd_g2d_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s post power on\n", pd->name);

	s3c_pm_do_restore_core(exynos_pd_g2d_clk_save,
			ARRAY_SIZE(exynos_pd_g2d_clk_save));

	exynos_rcg_enable(RCG_G2D);

	return 0;
}

/* exynos_pd_g2d_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable clks.
 */
static int exynos_pd_g2d_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power off\n", pd->name);

	s3c_pm_do_save(exynos_pd_g2d_clk_save,
			ARRAY_SIZE(exynos_pd_g2d_clk_save));

	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_ACLK_G2D);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_ACLK_G2D_SECURE_SSS_G2D);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_ACLK_G2D_SECURE_SLIM_SSS_G2D);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_ACLK_G2D_SECURE_SMMU_SLIM_SSS_G2D);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_ACLK_G2D_SECURE_SMMU_SSS_G2D);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_ACLK_G2D_SECURE_SMMU_MDMA_G2D);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_ACLK_G2D_SECURE_SMMU_G2D_G2D);

	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_PCLK_G2D);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_PCLK_G2D_SECURE_SMMU_SLIM_SSS_G2D);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_PCLK_G2D_SECURE_SMMU_SSS_G2D);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_PCLK_G2D_SECURE_SMMU_MDMA_G2D);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_PCLK_G2D_SECURE_SMMU_G2D_G2D);

	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_IP_G2D);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_IP_G2D_SECURE_SSS_G2D);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_IP_G2D_SECURE_SLIM_SSS_G2D);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_IP_G2D_SECURE_SMMU_SLIM_SSS_G2D);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_IP_G2D_SECURE_SMMU_SSS_G2D);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_IP_G2D_SECURE_SMMU_MDMA_G2D);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_IP_G2D_SECURE_SMMU_G2D_G2D);

	return 0;
}

static struct exynos_pd_callback cb_pd_g2d = {
	.name = "pd-g2d",
	.on_post = exynos_pd_g2d_power_on_post,
	.off_pre = exynos_pd_g2d_power_off_pre,
};

static struct sleep_save exynos_pd_isp_clk_save[] = {
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_ISP0),
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_ISP1),
};

static int exynos_pd_isp_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s post power on\n", pd->name);

	s3c_pm_do_restore_core(exynos_pd_isp_clk_save,
			ARRAY_SIZE(exynos_pd_isp_clk_save));

	exynos_rcg_enable(RCG_ISP);

	return 0;
}

/* exynos_pd_isp_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable clks.
 */
static int exynos_pd_isp_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s pre power off\n", pd->name);

	s3c_pm_do_save(exynos_pd_isp_clk_save,
			ARRAY_SIZE(exynos_pd_isp_clk_save));

	__raw_writel(0x1, EXYNOS5260_A5IS_SYS_PWR_REG);

	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_ACLK_ISP0);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_ACLK_ISP1);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_PCLK_ISP0);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_PCLK_ISP1);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_SCLK_ISP);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_IP_ISP0);
	__raw_writel(0xffffffff, EXYNOS5260_CLKGATE_IP_ISP1);

	return 0;
}

static int exynos_pd_isp_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s post power off\n", pd->name);

	__raw_writel(0x0, EXYNOS5260_A5IS_CONFIGURATION);

	return 0;
}

/* helpers for special power-off sequence */
#define __set_mask(name) __raw_writel(name##_ALL, name)
#define __clr_mask(name) __raw_writel(~(name##_ALL), name)

static int force_down_pre(const char *name)
{
	unsigned int reg;

	if (strncmp(name, "pd-isp", 6) == 0) {
		reg = __raw_readl(EXYNOS5260_A5IS_OPTION);
		reg &= ~(1 << 24 | 1 << 16);
		__raw_writel(reg, EXYNOS5260_A5IS_OPTION);

		__set_mask(EXYNOS5260_LPI_MASK_ISP_BUSMASTER);

		reg = 0;
		reg |= EXYNOS5260_LPI_MASK_ISP_ASYNCBRIDGE_AXIM_CORTEX;
		reg |= EXYNOS5260_LPI_MASK_ISP_ASYNCBRIDGE_AXIS_CORTEX;
		__raw_writel(reg, EXYNOS5260_LPI_MASK_ISP_ASYNCBRIDGE);
	} else {
		return -EINVAL;
	}

	return 0;
}

static unsigned int check_power_status(struct exynos_pm_domain *pd, int power_flags,
					unsigned int timeout)
{
	/* check STATUS register */
	while ((__raw_readl(pd->base+0x4) & EXYNOS_INT_LOCAL_PWR_EN) != power_flags) {
		if (timeout == 0) {
			pr_err("%s@%p: %08x, %08x, %08x\n",
					pd->genpd.name,
					pd->base,
					__raw_readl(pd->base),
					__raw_readl(pd->base+4),
					__raw_readl(pd->base+8));
			return 0;
		}
		--timeout;
		cpu_relax();
		usleep_range(8, 10);
	}

	return timeout;
}

#define TIMEOUT_COUNT	500 /* about 5ms, based on 10us */
static int exynos_pd_isp_power_off(struct exynos_pm_domain *pd, int power_flags)
{
	unsigned long timeout;

	if (unlikely(!pd))
		return -EINVAL;

	mutex_lock(&pd->access_lock);
	if (likely(pd->base)) {
		if (force_down_pre(pd->name))
			pr_warn("%s: failed to make force down state\n", pd->name);

		/* sc_feedback to OPTION register */
		__raw_writel(0x0102, pd->base+0x8);

		/* on/off value to CONFIGURATION register */
		__raw_writel(power_flags, pd->base);

		timeout = check_power_status(pd, power_flags, TIMEOUT_COUNT);

		if (unlikely(!timeout)) {
			pr_err(PM_DOMAIN_PREFIX "%s can't control power, timeout\n",
					pd->name);
			mutex_unlock(&pd->access_lock);
			return -ETIMEDOUT;
		} else {
			pr_info(PM_DOMAIN_PREFIX "%s power down success\n", pd->name);
		}

		if (unlikely(timeout < (TIMEOUT_COUNT >> 1))) {
			pr_warn("%s@%p: %08x, %08x, %08x\n",
					pd->name,
					pd->base,
					__raw_readl(pd->base),
					__raw_readl(pd->base+4),
					__raw_readl(pd->base+8));
			pr_warn(PM_DOMAIN_PREFIX "long delay found during %s is %s\n",
					pd->name, power_flags ? "on":"off");
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

static struct exynos_pd_callback cb_pd_isp = {
	.name = "pd-isp",
	.on_post = exynos_pd_isp_power_on_post,
	.off_pre = exynos_pd_isp_power_off_pre,
	.off_post = exynos_pd_isp_power_off_post,
	.off = exynos_pd_isp_power_off,
};

/* sub domain should have followings:
 * . callback function
 * . bts features
 *
 * #define EXYNOS_SUB_GPD(ENABLE, NAME, PDEV, CB, BTS)
 */
enum spd_disp_index {
	ID_SPD_FIMD,
	ID_SPD_TVMIXER,
	ID_SPD_MIPI_DSI,
	ID_SPD_DP,
	ID_SPD_HDMI,
};

static struct exynos_pm_domain pm_spd_disp_exynos5260[] = {
	EXYNOS_SUB_GPD(true, "spd-fimd", NULL, true),
	EXYNOS_SUB_GPD(true, "spd-tvmixer", NULL, true),
	EXYNOS_SUB_GPD(true, "spd-mipi-dsi", NULL, false),
	EXYNOS_SUB_GPD(true, "spd-dp", NULL, false),
	EXYNOS_SUB_GPD(true, "spd-hdmi", NULL, false),
	EXYNOS_SUB_GPD(true, NULL, NULL, false),
};

/* PD_ISP belongs to PD_GSCL
 *
 */
enum spd_gscl_index {
	ID_SPD_GSCL0,
	ID_SPD_GSCL1,
	ID_SPD_MSCL0,
	ID_SPD_MSCL1,
	ID_SPD_FLITE_A,
	ID_SPD_FLITE_B,
	ID_SPD_FLITE_C,
	ID_SPD_ISP,
};

static struct exynos_pm_domain pm_spd_gscl_exynos5260[] = {
	EXYNOS_SUB_GPD(true, "spd-gscl0", NULL, true),
	EXYNOS_SUB_GPD(true, "spd-gscl1", NULL, true),
	EXYNOS_SUB_GPD(true, "spd-mscl0", NULL, true),
	EXYNOS_SUB_GPD(true, "spd-mscl1", NULL, true),
	EXYNOS_SUB_GPD(true, "spd-flite-a", NULL, false),
	EXYNOS_SUB_GPD(true, "spd-flite-b", NULL, false),
	EXYNOS_SUB_GPD(true, "spd-flite-d", NULL, false),
	EXYNOS_MASTER_GPD(true, EXYNOS5260_ISP_CONFIGURATION,  "pd-isp",    &cb_pd_isp, NULL, true),
	EXYNOS_SUB_GPD(true, NULL, NULL, false),
};

enum spd_g2d_index {
	ID_SPD_MDMA,
	ID_SPD_G2D,
	ID_SPD_SSS,
	ID_SPD_JPEG,
	ID_SPD_SLIMSSS,
};

static struct exynos_pm_domain pm_spd_g2d_exynos5260[] = {
	EXYNOS_SUB_GPD(true, "spd-mdma", NULL, true),
	EXYNOS_SUB_GPD(true, "spd-g2d", NULL, true),
	EXYNOS_SUB_GPD(true, "spd-sss", NULL, false),
	EXYNOS_SUB_GPD(true, "spd-jpeg", NULL, true),
	EXYNOS_SUB_GPD(true, "spd-slimsss", NULL, false),
	EXYNOS_SUB_GPD(true, NULL, NULL, false),
};

/* master domain should have followings:
 * . registers to set
 * . callback function
 * . sub domain
 * . bts features
 *
 * when master_pd has sub_pd, master_pd should be enabled
 * not to skip sub_pd init.
 *
 * EXYNOS_MASTER_GPD(ENABLE, _BASE, NAME, PDEV, CB, SUB, BTS)
 */
static struct exynos_pm_domain pm_domain_exynos5260[] = {
	EXYNOS_MASTER_GPD(true, EXYNOS5260_G3D_CONFIGURATION,  "pd-g3d",    &cb_pd_g3d, NULL, true),
	EXYNOS_MASTER_GPD(true, EXYNOS5260_MFC_CONFIGURATION,  "pd-mfc",    &cb_pd_mfc, NULL, true),
	EXYNOS_MASTER_GPD(true, EXYNOS5260_DISP_CONFIGURATION, "pd-disp",   &cb_pd_disp, pm_spd_disp_exynos5260, false),
	EXYNOS_MASTER_GPD(true, EXYNOS5260_GSCL_CONFIGURATION, "pd-gscl",   &cb_pd_gscl, pm_spd_gscl_exynos5260, false),
	EXYNOS_MASTER_GPD(true, EXYNOS5260_MAU_CONFIGURATION,  "pd-maudio", &cb_pd_maudio, NULL, false),
	EXYNOS_MASTER_GPD(false, EXYNOS5260_G2D_CONFIGURATION,  "pd-g2d",    &cb_pd_g2d, pm_spd_g2d_exynos5260, false),
	EXYNOS_MASTER_GPD(true, NULL, NULL, NULL, NULL, false),
};

int exynos_get_runtime_pd_status(enum master_pd_index index)
{
	if (index == ID_PD_ISP)
		return pm_spd_gscl_exynos5260[ID_SPD_ISP].check_status(&pm_spd_gscl_exynos5260[ID_SPD_ISP]);

	return pm_domain_exynos5260[index].check_status(&pm_domain_exynos5260[index]);
}

/* Linking device to power domain */
static struct exynos_device_pd_link pm_device_to_pd_exynos5260[] = {
#ifdef CONFIG_MALI_T6XX
	{&pm_domain_exynos5260[ID_PD_G3D], &exynos5_device_g3d, true},
#endif
#ifdef CONFIG_S5P_DEV_MFC
	{&pm_domain_exynos5260[ID_PD_MFC], &s5p_device_mfc, true},
	{&pm_domain_exynos5260[ID_PD_MFC], &SYSMMU_PLATDEV(mfc_lr), true},
#endif
#ifdef CONFIG_FB_S3C
	{&pm_spd_disp_exynos5260[ID_SPD_FIMD], &s5p_device_fimd1, true},
	{&pm_spd_disp_exynos5260[ID_SPD_FIMD], &SYSMMU_PLATDEV(fimd1), true},
	{&pm_spd_disp_exynos5260[ID_SPD_FIMD], &SYSMMU_PLATDEV(fimd1a), true},
#endif
#ifdef CONFIG_FB_MIPI_DSIM
	{&pm_spd_disp_exynos5260[ID_SPD_MIPI_DSI], &s5p_device_mipi_dsim1, true},
#endif
#ifdef CONFIG_S5P_DP
	{&pm_spd_disp_exynos5260[ID_SPD_DP], &s5p_device_dp, true},
#endif
#ifdef CONFIG_S5P_DEV_TV
	{&pm_spd_disp_exynos5260[ID_SPD_HDMI], &s5p_device_hdmi, true},
	{&pm_spd_disp_exynos5260[ID_SPD_TVMIXER], &s5p_device_mixer, true},
	{&pm_spd_disp_exynos5260[ID_SPD_TVMIXER], &SYSMMU_PLATDEV(tv), true},
#endif
#ifdef CONFIG_EXYNOS5_DEV_GSC
	{&pm_spd_gscl_exynos5260[ID_SPD_GSCL0], &exynos5_device_gsc0, true},
	{&pm_spd_gscl_exynos5260[ID_SPD_GSCL1], &exynos5_device_gsc1, true},
	{&pm_spd_gscl_exynos5260[ID_SPD_GSCL0], &SYSMMU_PLATDEV(gsc0), true},
	{&pm_spd_gscl_exynos5260[ID_SPD_GSCL1], &SYSMMU_PLATDEV(gsc1), true},
#endif
#ifdef CONFIG_EXYNOS5_DEV_SCALER
	{&pm_spd_gscl_exynos5260[ID_SPD_MSCL0], &exynos5_device_scaler0, true},
	{&pm_spd_gscl_exynos5260[ID_SPD_MSCL1], &exynos5_device_scaler1, true},
	{&pm_spd_gscl_exynos5260[ID_SPD_MSCL0], &SYSMMU_PLATDEV(scaler), true},
	{&pm_spd_gscl_exynos5260[ID_SPD_MSCL1], &SYSMMU_PLATDEV(scaler1), true},
#endif
#ifdef CONFIG_EXYNOS_DEV_FIMC_IS
	{&pm_domain_exynos5260[ID_PD_GSCL], &SYSMMU_PLATDEV(camif0), true},
	{&pm_domain_exynos5260[ID_PD_GSCL], &SYSMMU_PLATDEV(camif1), true},
	{&pm_domain_exynos5260[ID_PD_GSCL], &SYSMMU_PLATDEV(camif2), true},
	{&pm_domain_exynos5260[ID_PD_GSCL], &exynos_device_fimc_is_sensor0, true},
	{&pm_domain_exynos5260[ID_PD_GSCL], &exynos_device_fimc_is_sensor1, true},
	{&pm_spd_gscl_exynos5260[ID_SPD_ISP], &SYSMMU_PLATDEV(isp2), true},
	{&pm_spd_gscl_exynos5260[ID_SPD_ISP], &exynos5_device_fimc_is, true},
	{&pm_spd_gscl_exynos5260[ID_SPD_ISP], &s3c64xx_device_spi3, true},
#endif
	{&pm_domain_exynos5260[ID_PD_MAU], &exynos5_device_lpass, true},
#ifdef CONFIG_SND_SAMSUNG_PCM
	{&pm_domain_exynos5260[ID_PD_MAU], &exynos5_device_pcm0, true},
#endif
#ifdef CONFIG_SND_SAMSUNG_ALP
	{&pm_domain_exynos5260[ID_PD_MAU], &exynos5_device_srp, true},
#endif
#ifdef CONFIG_S5P_DEV_FIMG2D
	{&pm_spd_g2d_exynos5260[ID_SPD_G2D], &s5p_device_fimg2d, true},
	{&pm_spd_g2d_exynos5260[ID_SPD_G2D], &SYSMMU_PLATDEV(2d), true},
#endif
	{&pm_spd_g2d_exynos5260[ID_SPD_JPEG], &exynos5_device_jpeg_hx, true},
	{&pm_spd_g2d_exynos5260[ID_SPD_JPEG], &SYSMMU_PLATDEV(mjpeg), true},
	{NULL, NULL, 0}
};

struct exynos_pm_domain * exynos5260_pm_domain(void)
{
	return pm_domain_exynos5260;
}

struct exynos_device_pd_link * exynos5260_device_pd_link(void)
{
	return pm_device_to_pd_exynos5260;
}
