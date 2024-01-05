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

#ifndef __MACH_EXYNOS_BOARD_UNIVERSAL4415_H
#define __MACH_EXYNOS_BOARD_UNIVERSAL4415_H
void exynos4415_universal4415_clock_init(void);
void exynos4_universal4415_power_init(void);
void exynos4415_universal4415_mmc_init(void);
void exynos4_universal4415_input_init(void);
void exynos4_universal4415_display_init(void);
void exynos4_universal4415_media_init(void);
void exynos4_universal4415_audio_init(void);
void exynos4_universal4415_usb_init(void);
void exynos4_universal4415_pmic_init(void);
void exynos4_universal4415_muic_init(void);
void exynos5_universal4415_battery_init(void);
void exynos5_universal4415_mfd_init(void);
void exynos4_universal4415_led_init(void);
void exynos4_universal4415_w1_init(void);
void exynos4_universal4415_sensor_init(void);
void exynos4_universal4415_nfc_init(void);
void exynos4_universal4415_camera_init(void);
void board_universal4415_thermistor_init(void);
void board_universal4415_gpio_init(void);
#ifdef CONFIG_SEC_DEV_JACK
void exynos4_universal4415_jack_init(void);
#endif
#endif
