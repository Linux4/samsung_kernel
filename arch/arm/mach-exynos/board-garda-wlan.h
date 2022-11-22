/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MACH_EXYNOS_BOARD_UNIVERSAL5410_WLAN_H
#define __MACH_EXYNOS_BOARD_UNIVERSAL5410_WLAN_H

#define GPIO_WLAN_HOST_WAKE EXYNOS4_GPX0(0)

#define GPIO_WLAN_EN 	EXYNOS4_GPC0(3)
#define WLAN_SDIO_CLK   EXYNOS4_GPK1(0)
#define WLAN_SDIO_CMD   EXYNOS4_GPK1(1)
#define WLAN_SDIO_D0 EXYNOS4_GPK1(3)
#define WLAN_SDIO_D1 EXYNOS4_GPK1(4)
#define WLAN_SDIO_D2 EXYNOS4_GPK1(5)
#define WLAN_SDIO_D3 EXYNOS4_GPK1(6)

#define GPIO_WLAN_SDIO_CLK_AF	2
#define GPIO_WLAN_SDIO_CMD_AF 	2
#define GPIO_WLAN_SDIO_D0_AF 	2
#define GPIO_WLAN_SDIO_D1_AF 	2
#define GPIO_WLAN_SDIO_D2_AF 	2
#define GPIO_WLAN_SDIO_D3_AF 	2

extern int brcm_wlan_init(void);
extern void mmc_force_presence_change(struct platform_device *pdev, int val);

#endif /* __MACH_EXYNOS_BOARD_UNIVERSAL5410_WLAN_H */
