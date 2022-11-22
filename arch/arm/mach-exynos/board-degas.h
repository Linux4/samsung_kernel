/*
 * Copyright (c) 2012-2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * board-degas.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MACH_EXYNOS_BOARD_DEGAS_H
#define __MACH_EXYNOS_BOARD_DEGAS_H

int exynos3_get_revision(void);
void board_sec_thermistor_init(void);
void exynos4_smdk4270_led_init(void);

#if defined(CONFIG_BATTERY_SAMSUNG)
#include <linux/battery/sec_charging_common.h>
extern sec_battery_platform_data_t sec_battery_pdata;
#endif

#endif /* __MACH_EXYNOS_BOARD_DEGAS_H */
