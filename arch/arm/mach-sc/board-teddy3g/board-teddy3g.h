/* arch/arm/mach-sc/board-teddy3g/board-teddy3g.h
 *
 * Copyright (C) 2013 Samsung Electronics Co, Ltd.
 *
 * Based on mach-sc/board-teddy3g/board-teddy3g.h
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

#ifndef __BOARD_TEDDY3G_H__
#define __BOARD_TEDDY3G_H__

extern struct smp_operations sprd_smp_ops;

extern void __init sci_reserve(void);
extern void __init sci_map_io(void);
extern void __init sci_init_irq(void);
extern void __init sci_timer_init(void);
extern void __init sci_enable_timer_early(void);

/** @category Serial Interface - UART, SPI & I2C */
void teddy3g_serial_init(void);

/** @category Display - LDC and Backlight */
void teddy3g_display_init(void);

/** @category Audio */
void teddy3g_audio_init(void);

/** @category Audio late initialization */
void teddy3g_audio_init_late(void);

/** @category Audio early initialization */
void teddy3g_audio_init_early(void);

/** @category MMC - SDIO, NAND and eMMC */
void teddy3g_mmc_init(void);

/** @category Connectors - mUSB-IC, USB-OTG */
void teddy3g_connector_init(void);

/** @category Regulator - PMIC */
void teddy3g_pmic_init(void);

/** @category Battery & Charger */
void teddy3g_battery_init(void);

/** @category Sesnors */
void teddy3g_sensors_init(void);

/** @category Camera */
void teddy3g_camera_init(void);

/** @category Input - Keypad, Touch Core and TSP */
void teddy3g_input_init(void);

/** @category RF - BT, WiFi */
void teddy3g_bt_wifi_init(void);

/** @category MISC Devices */
void teddy3g_misc_devices(void);

#endif
