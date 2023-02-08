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

#ifndef __MACH_EXYNOS_BOARD_UNIVERSAL222AP_H
#define __MACH_EXYNOS_BOARD_UNIVERSAL222AP_H

void exynos4_smdk4270_power_init(void);
void exynos4_smdk4270_mmc0_init(void);
void exynos4_smdk4270_mmc1_init(void);
void exynos4_smdk4270_mmc2_init(void);
void exynos4_smdk4270_usb_init(void);
void exynos4_smdk4270_display_init(void);
void exynos4_smdk4270_input_init(void);
void exynos4_smdk4270_audio_init(void);
void exynos4_smdk4270_media_init(void);
void exynos4_smdk4270_charger_init(void);
void exynos4_smdk4270_muic_init(void);
void exynos4_universal222ap_clock_init(void);
void exynos4_universal222ap_camera_init(void);
void board_universal_ss222ap_init_fpga(void);
void board_universal_ss222ap_init_gpio(void);
void board_universal_ss222ap_init_gpio_i2c(void);
int board_universal_ss222ap_add_platdata_gpio_i2c(int index,
		                void *platform_data);
void exynos4_smdk4270_mfd_init(void);
void board_universal_ss222ap_init_sensor(void);
#ifdef CONFIG_W1
void exynos4_universal222ap_cover_id_init(void);
#endif

#endif
