/* Copyright (C) 2013 Samsung Electronics Co, Ltd.
 *
 * Based on mach-sc/board-huskytd/board-huskytd.h
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __BOARD_HUSKYTD_H__
#define __BOARD_HUSKYTD_H__

extern struct smp_operations sprd_smp_ops;

extern void __init sci_reserve(void);
extern void __init sci_map_io(void);
extern void __init sci_init_irq(void);
extern void __init sci_timer_init(void);
extern void __init sci_enable_timer_early(void);

/** @category Serial Interface - UART, SPI & I2C */
void huskytd_serial_init(void);

/** @category Display - LDC and Backlight */
void huskytd_display_init(void);

/** @category Audio */
void huskytd_audio_init(void);

/** @category Audio -Late Init */
void huskytd_audio_late_init(void);

/** @category MMC - SDIO, NAND and eMMC */
void huskytd_mmc_init(void);

/** @category Connectors - mUSB-IC, USB-OTG */
void huskytd_connector_init(void);

/** @category Regulator - PMIC */
void huskytd_pmic_init(void);

/** @category Battery & Charger */
void huskytd_battery_init(void);

/** @category Sesnors */
void huskytd_sensors_init(void);

/** @category Camera */
void huskytd_camera_init(void);

/** @category Input - Keypad, Touch Core and TSP */
void huskytd_input_init(void);

/** @category RF - BT, WiFi */
void huskytd_bt_wifi_init(void);

/** @category MISC Devices */
void huskytd_misc_devices(void);

#endif
