/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
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

#ifndef __MACH_EXYNOS_BOARD_UNIVERSAL5260_H
#define __MACH_EXYNOS_BOARD_UNIVERSAL5260_H
#include <linux/i2c.h>

#if defined(CONFIG_BATTERY_SAMSUNG)
#include <linux/battery/sec_charging_common.h>
extern sec_battery_platform_data_t sec_battery_pdata;
extern int current_cable_type;
#endif

void exynos5_universal5260_pmic_init(void);
void exynos5_universal5260_power_init(void);
void exynos5_universal5260_battery_init(void);
void exynos5_universal5260_display_init(void);
void exynos5_universal5260_clock_init(void);
void exynos5_universal5260_mmc_init(void);
void exynos5_universal5260_usb_init(void);
void exynos5_universal5260_led_init(void);
void exynos5_universal5260_input_init(void);
void exynos5_universal5260_mfd_init(void);
void exynos5_universal5260_media_init(void);
void exynos5_universal5260_camera_init(void);
void exynos5_universal5260_vibrator_init(void);
void exynos5_universal5260_sensor_init(void);
void exynos5_universal5260_nfc_init(void);
void exynos5_universal5260_audio_init(void);
void exynos5_universal5420_gpio_init(void);
#if defined(CONFIG_ICE4_FPGA)
void exynos5_universal5260_fpga_init(void);
#endif
void exynos5_universal5260_thermistor_init(void);
extern unsigned int lpcharge;
#endif
