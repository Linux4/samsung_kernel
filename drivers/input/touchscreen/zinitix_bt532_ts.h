/*
 *
 * Zinitix bt532 touch driver
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */


#ifndef _LINUX_BT532_TS_H
#define _LINUX_BT532_TS_H

#define TS_DRVIER_VERSION	"1.0.18_1"

#define BT532_TS_DEVICE		"bt532_ts_device"

#define SUPPORTED_TOUCH_KEY_LED	0

/* Model define */
#if defined(CONFIG_SEC_VASTA_PROJECT) || defined(CONFIG_SEC_VASTA3G_PROJECT)
#define CHECK_HWID			1
#define ESD_TIMER_INTERVAL		1	/* ESD Protection */

/*Test Mode (Monitoring Raw Data) */
#define SEC_DND_SHIFT_VALUE		2
#define SEC_DND_N_COUNT			10
#define SEC_DND_U_COUNT			2
#define SEC_DND_FREQUENCY		116

#define SEC_PDND_N_COUNT		16
#define SEC_PDND_U_COUNT		20
#define SEC_PDND_FREQUENCY		119

#else
#define CHECK_HWID			0
#define ESD_TIMER_INTERVAL		0

/*Test Mode (Monitoring Raw Data) */
#define SEC_DND_SHIFT_VALUE		3
#define SEC_DND_N_COUNT			10
#define SEC_DND_U_COUNT			2
#define SEC_DND_FREQUENCY		99 /* 200khz */

#define SEC_PDND_N_COUNT		16
#define SEC_PDND_U_COUNT		14
#define SEC_PDND_FREQUENCY		66

#endif

#define zinitix_debug_msg(fmt, args...) \
	do { \
		if (m_ts_debug_mode) \
			printk(KERN_INFO "bt532_ts[%-18s:%5d] " fmt, \
					__func__, __LINE__, ## args); \
	} while (0);

#define zinitix_printk(fmt, args...) \
	do { \
		printk(KERN_INFO "bt532_ts[%-18s:%5d] " fmt, \
				__func__, __LINE__, ## args); \
	} while (0);

#define bt532_err(fmt) \
	do { \
		pr_err("bt532_ts : %s " fmt, __func__); \
	} while (0);

struct bt532_ts_platform_data {
	int		gpio_int;
	//u32		gpio_scl;
	//u32		gpio_sda;
	//u32		gpio_ldo_en;
	u32		tsp_irq;
#if SUPPORTED_TOUCH_KEY_LED
	int		gpio_keyled;
#endif
	int		tsp_vendor1;
	int		tsp_vendor2;
	int		tsp_en_gpio;
	u32		tsp_supply_type;
	//int (*tsp_power)(int on);
	u16		x_resolution;
	u16		y_resolution;
	u16		page_size;
	u8		orientation;
	const char      *pname;
};

extern struct class *sec_class;

#endif /* LINUX_BT532_TS_H */
