/*
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *
 * EXYNOS5 - Helper functions for MIPI-CSIS control
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <mach/regs-clock.h>
#include <mach/exynos5-mipiphy.h>

#define MIPI_PHY_BIT0					(1 << 0)
#define MIPI_PHY_BIT1					(1 << 1)

#if defined(CONFIG_SOC_EXYNOS5422)
static int __exynos5_mipi_phy_control(int id, bool on, u32 reset)
{
	void __iomem *addr_phy;
	u32 cfg;

	addr_phy = S5P_MIPI_DPHY_CONTROL(id);

	cfg = __raw_readl(addr_phy);
	cfg = (cfg | S5P_MIPI_DPHY_SRESETN);
	__raw_writel(cfg, addr_phy);

	if (1) {
		cfg |= S5P_MIPI_DPHY_ENABLE;
	} else if (!(cfg & (S5P_MIPI_DPHY_SRESETN | S5P_MIPI_DPHY_MRESETN)
			& (~S5P_MIPI_DPHY_SRESETN))) {
		cfg &= ~S5P_MIPI_DPHY_ENABLE;
	}

	__raw_writel(cfg, addr_phy);

	return 0;
}

#elif defined(CONFIG_SOC_EXYNOS3475)

/* reference counter for m4s4 */
static int dphy_m4s4_status;
static DEFINE_SPINLOCK(lock);

static int sresetn_onoff(int id, bool on)
{
	void __iomem *phy_con_sysreg;
	u32 cfg;
	int ret = 0;

	if (on) {
		switch (id) {
		case 0:
			dphy_m4s4_status++;
			/* If dphy_m4s4 was turned on before referenced by the driver,
			 * dphy_m4s4 does not get enabled but resetted
			 */
			if (dphy_m4s4_status == 1) {
				if ((__raw_readl(EXYNOS_PMU_MIPI_PHY_M4S4_CONTROL) & 0x1)) {
					pr_warn("%s: dphy_m4s4_status is one but already enabled\n",
							__func__);
					break;
				}
				/* enable DPHY */
				__raw_writel(MIPI_PHY_BIT0, EXYNOS_PMU_MIPI_PHY_M4S4_CONTROL);
			} else {
				if (!(__raw_readl(EXYNOS_PMU_MIPI_PHY_M4S4_CONTROL) & 0x1)) {
					pr_err("%s: Invalid disable status %d\n",
							__func__, dphy_m4s4_status);
					break;
					BUG();
				}
			}

			/* PHY reset */
			phy_con_sysreg = S5P_VA_SYSREG_ISP + 0x1030;
			cfg = __raw_readl(phy_con_sysreg);

			cfg &= ~(1 << 0);
			__raw_writel(cfg, phy_con_sysreg);
			cfg |= (1 << 0);
			__raw_writel(cfg, phy_con_sysreg);
			break;
		case 1:
			/* enable DPHY */
			__raw_writel(MIPI_PHY_BIT0, EXYNOS_PMU_MIPI_PHY_M0S2_CONTROL);

			/* PHY reset */
			phy_con_sysreg = S5P_VA_SYSREG_ISP + 0x1030; /* ISP_MIPI_PHY_CON */
			cfg = __raw_readl(phy_con_sysreg);

			cfg &= ~(1 << 1);
			__raw_writel(cfg, phy_con_sysreg);
			cfg |= (1 << 1);
			__raw_writel(cfg, phy_con_sysreg);
			break;
		default:
			pr_err("id(%d) is invalid", id);
			ret =  -EINVAL;
			goto p_err;
			break;
		}
	} else {
		switch (id) {
		case 0:
			dphy_m4s4_status--;
			if (dphy_m4s4_status < 0) {
				pr_info("%s: reset %s: dphy_m4s4_ref_cnt %d is wrong",
							__func__, "SRESETN", dphy_m4s4_status);
				BUG();
			}

			if (dphy_m4s4_status == 0)
				__raw_writel(0, EXYNOS_PMU_MIPI_PHY_M4S4_CONTROL);
			break;
		case 1:
			__raw_writel(0, EXYNOS_PMU_MIPI_PHY_M0S2_CONTROL);
			break;
		default:
			pr_err("id(%d) is invalid", id);
			ret =  -EINVAL;
			goto p_err;
			break;
		}
	}

p_err:	
	return ret;
}

static int mresetn_onoff(int id, bool on)
{
	u32 cfg;
	volatile void __iomem *phys_con;

	if (on) {
		switch (id) {
		case 0:
			dphy_m4s4_status++;
			/* If dphy_m4s4 was turned on before referenced by the driver,
			 * dphy_m4s4 does not get resetted and enabled.
			 */
			if ((dphy_m4s4_status == 1) &&
				(__raw_readl(EXYNOS_PMU_MIPI_PHY_M4S4_CONTROL) & 0x1))
				break;

			/* Only when dphy_m4s4 is referenced once
			 * does dphy_m4s4 get resetted and enabled
			 */
			if ((dphy_m4s4_status == 1) ||
				(!(__raw_readl(EXYNOS_PMU_MIPI_PHY_M4S4_CONTROL) & 0x1))) {
				if (dphy_m4s4_status != 1) {
					pr_info("%s: reset %s: dphy_m4s4_ref_cnt %d is wrong",
							__func__, "MRESETN", dphy_m4s4_status);
					BUG();
				}
				/* DPHY PMU enable */
				__raw_writel(MIPI_PHY_BIT0, EXYNOS_PMU_MIPI_PHY_M4S4_CONTROL);
			}

			/* DPHY reset */
			phys_con = S3P_MIPI_DPHY_SYSREG;
			cfg = __raw_readl(phys_con);
			cfg &= ~(MIPI_PHY_BIT0);
			__raw_writel(cfg, phys_con);
			cfg |= (MIPI_PHY_BIT0);
			__raw_writel(cfg, phys_con);
			break;
		default:
			pr_err("id(%d) is invalid", id);
			return -EINVAL;
			break;
		}
	} else {
		switch (id) {
		case 0:
			dphy_m4s4_status--;
			if (dphy_m4s4_status < 0) {
				pr_info("%s: reset %s: dphy_m4s4_ref_cnt %d is wrong",
							__func__, "MRESETN", dphy_m4s4_status);
				BUG();
			}

			if (dphy_m4s4_status == 0) {
				/* DPHY reset */
				phys_con = S3P_MIPI_DPHY_SYSREG;
				cfg = __raw_readl(phys_con);
				cfg &= ~(MIPI_PHY_BIT0);
				__raw_writel(cfg, phys_con);
				cfg |= (MIPI_PHY_BIT0);
				__raw_writel(cfg, phys_con);
				/* DPHY PMU disable */
				__raw_writel(0, EXYNOS_PMU_MIPI_PHY_M4S4_CONTROL);
			}
			break;
		default:
			pr_err("id(%d) is invalid", id);
			return -EINVAL;
			break;
		}
	}
	return 0;
}

static int __exynos5_mipi_phy_control(int id, bool on, u32 reset)
{
	unsigned long flags;
	int valid = 0;

	spin_lock_irqsave(&lock, flags);

	switch (reset) {
	case S5P_MIPI_DPHY_SRESETN:
		valid = sresetn_onoff(id, on);
		break;
	case S5P_MIPI_DPHY_MRESETN:
		valid = mresetn_onoff(id, on);
		break;
	}

	spin_unlock_irqrestore(&lock, flags);

	return valid;
}

#else
static __inline__ u32 exynos5_phy0_is_running(u32 reset)
{
	u32 ret = 0;

	/* When you try to disable DSI, CHECK CAM0 PD STATUS */
	if (reset == S5P_MIPI_DPHY_MRESETN) {
		if (readl(EXYNOS5430_CAM0_STATUS) & 0x1)
			ret = __raw_readl(S5P_VA_SYSREG_CAM0 + 0x1014) & MIPI_PHY_BIT0;
	/* When you try to disable CSI, CHECK DISP PD STATUS */
	} else if (reset == S5P_MIPI_DPHY_SRESETN) {
		if (readl(EXYNOS5430_DISP_STATUS) & 0x1)
			ret = __raw_readl(S5P_VA_SYSREG_DISP + 0x000C) & MIPI_PHY_BIT0;
	}

	return ret;
}

static int __exynos5_mipi_phy_control(int id, bool on, u32 reset)
{
	static DEFINE_SPINLOCK(lock);
	void __iomem *addr_phy;
	void __iomem *addr_reset;
	unsigned long flags;
	u32 cfg;

	addr_phy = S5P_MIPI_DPHY_CONTROL(id);

	spin_lock_irqsave(&lock, flags);

	/* PHY PMU enable */
	if (on) {
		cfg = __raw_readl(addr_phy);
		cfg |= S5P_MIPI_DPHY_ENABLE;
		__raw_writel(cfg, addr_phy);
	}

	/* PHY reset */
	switch (id) {
	case 0:
		if (reset == S5P_MIPI_DPHY_SRESETN) {
			if (readl(EXYNOS5430_CAM0_STATUS) & 0x1) {
				addr_reset = S5P_VA_SYSREG_CAM0 + 0x1014;
				cfg = __raw_readl(addr_reset);
				cfg = on ? (cfg | MIPI_PHY_BIT0) : (cfg & ~MIPI_PHY_BIT0);
				__raw_writel(cfg, addr_reset);
			}
		} else {
			if (readl(EXYNOS5430_DISP_STATUS) & 0x1) {
				addr_reset = S5P_VA_SYSREG_DISP + 0x000c;
				cfg = __raw_readl(addr_reset);

				/* 0: enable reset, 1: release reset */
				cfg = (cfg & ~MIPI_PHY_BIT0);
				__raw_writel(cfg, addr_reset);
				cfg = (cfg | MIPI_PHY_BIT0);
				__raw_writel(cfg, addr_reset);
			}
		}
		break;
	case 1:
		if (readl(EXYNOS5430_CAM0_STATUS) & 0x1) {
			addr_reset = S5P_VA_SYSREG_CAM0 + 0x1014;
			cfg = __raw_readl(addr_reset);
			cfg = on ? (cfg | MIPI_PHY_BIT1) : (cfg & ~MIPI_PHY_BIT1);
			__raw_writel(cfg, addr_reset);
		}
		break;
	case 2:
		if (readl(EXYNOS5430_CAM1_STATUS) & 0x1) {
			addr_reset = S5P_VA_SYSREG_CAM1 + 0x1020;
			cfg = __raw_readl(addr_reset);
			cfg = on ? (cfg | MIPI_PHY_BIT0) : (cfg & ~MIPI_PHY_BIT0);
			__raw_writel(cfg, addr_reset);
		}
		break;
	default:
		pr_err("id(%d) is invalid", id);
		spin_unlock_irqrestore(&lock, flags);
		return -EINVAL;
	}

	/* PHY PMU disable */
	if (!on) {
		cfg = __raw_readl(addr_phy);
		if (id == 0) {
			if (!exynos5_phy0_is_running(reset))
				cfg &= ~S5P_MIPI_DPHY_ENABLE;
		} else {
			cfg &= ~S5P_MIPI_DPHY_ENABLE;
		}
		__raw_writel(cfg, addr_phy);
	}

	spin_unlock_irqrestore(&lock, flags);

	return 0;
}
#endif

int exynos5_csis_phy_enable(int csi_id, int sensor_instance, bool on)
{
/* dphy slave selection for sensor position switch */
#if defined(CONFIG_SOC_EXYNOS3475)
	void __iomem *phy_con_sysreg;
	u32 cfg;

	if (on) {
		phy_con_sysreg = S5P_VA_SYSREG_ISP + 0x1030;

		cfg = __raw_readl(phy_con_sysreg);
		switch (sensor_instance) {
		case 0:
			cfg &= ~(1 << 2);
			break;
		case 1:
			cfg |= (1 << 2);
			break;
		default:
			pr_err("sensor instance(%d) is invalid", sensor_instance);
			return -EINVAL;
		}
		__raw_writel(cfg, phy_con_sysreg);
	}

	return __exynos5_mipi_phy_control(sensor_instance, on, S5P_MIPI_DPHY_SRESETN);
#else
	return __exynos5_mipi_phy_control(csi_id, on, S5P_MIPI_DPHY_SRESETN);
#endif
}
EXPORT_SYMBOL(exynos5_csis_phy_enable);

int exynos5_dism_phy_enable(int id, bool on)
{
	return __exynos5_mipi_phy_control(id, on, S5P_MIPI_DPHY_MRESETN);
}
EXPORT_SYMBOL(exynos5_dism_phy_enable);
