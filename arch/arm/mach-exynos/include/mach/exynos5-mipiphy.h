/*
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *
 * EXYNOS5 - Helper functions for MIPI-CSIS control
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PLAT_MIPI_PHY_H
#define __PLAT_MIPI_PHY_H __FILE__

#define EXYNOS_PMU_MIPI_PHY_CONTROL(_nr)	(EXYNOS_PMU_MIPI_PHY_M4S4_CONTROL \
								+ ((_nr) * 0x04))
#define S3P_MIPI_DPHY_CONTROL(n)	(EXYNOS_PMUREG(0x0710 + (n) * 0x4))
#define S3P_MIPI_DPHY_SYSREG		(S5P_VA_SYSREG_DISP + 0x100C)

extern int exynos5_csis_phy_enable(int csi_id, int sensor_instance, bool on);
extern int exynos5_dism_phy_enable(int id, bool on);

#endif /* __PLAT_MIPI_PHY_H */
