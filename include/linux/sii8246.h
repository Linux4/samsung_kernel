/*
 * Copyright (C) 2012 Samsung Electronics
 *
 * Author: Rajucm <rajkumar.m@samsung.com>
 *	   kmini.park <kmini.park@samsung.com>
 *	   Daniel(Philju) Lee <daniel.lee@siliconimage.com>
 *
 * Date: 00:00 AM, 6th September, 2013
 *
 * Based on  Silicon Image MHL SII8246 Transmitter Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef _SII8246_PLATFORM_H
#define _SII8246_PLATFORM_H

#define SFEATURE_SII8246_PLATFORM

#ifdef SFEATURE_SII8246_PLATFORM

struct mhl_platform_data {
	bool hpd_status;
	int charging_type;

	struct i2c_client *simg72_tx_client;
	struct i2c_client *simg7A_tx_client;
	struct i2c_client *simg92_tx_client;
	struct i2c_client *simgC8_tx_client;
	int (*muic_mhl_cb)(struct mhl_platform_data *pdata, bool on,
						int mhl_charger);
	int (*muic_otg_set)(int on);
	void (*power)(bool on);
	void (*hw_reset)(void);
	void (*gpio_cfg)(void);
	bool (*vbus_present)(void);
	u32 swing_level;
	bool drm_workaround;
#if defined(CONFIG_OF)
	int gpio_mhl_scl;
	int gpio_mhl_sda;
	int gpio_mhl_irq;
	int gpio_mhl_en;
	int gpio_mhl_reset;
	int gpio_mhl_wakeup;
	int gpio_ta_int;
	bool gpio_barcode_emul;
	struct regulator *vcc_1p2v;
	struct regulator *vcc_1p8v;
	struct regulator *vcc_3p3v;
#endif
};

enum mhl_attached_type {
	MHL_DETACHED = 0,
	MHL_ATTACHED,
};
int acc_register_notifier(struct notifier_block *nb);
int acc_unregister_notifier(struct notifier_block *nb);
void acc_notify(int event);

#endif/*SFEATURE_SII8246_PLATFORM*/
#endif/*_SII8240_PLATFORM_H*/
