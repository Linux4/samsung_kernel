/*
 * SAMSUNG NFC N2 Controller
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Woonki Lee <woonki84.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the License, or (at your
 * option) any later version.
 *
 */

#include <linux/platform_device.h>

#define SEC_NFC_DRIVER_NAME		"sec-nfc"

#define SEC_NFC_MAX_BUFFER_SIZE	512

/* ioctl */
#define SEC_NFC_MAGIC	'S'
#define SEC_NFC_GET_MODE	_IOW(SEC_NFC_MAGIC, 0, unsigned int)
#define SEC_NFC_SET_MODE	_IOW(SEC_NFC_MAGIC, 1, unsigned int)
#define SEC_NFC_SET_READTIMES	_IOW(SEC_NFC_MAGIC, 3, unsigned int)

/* size */
#define SEC_NFC_MSG_MIN_SIZE	1
#define SEC_NFC_MSG_MAX_SIZE	(256+4)

/* wait for device stable */
#define SEC_NFC_VEN_WAIT_TIME	(50)

/* gpio pin configuration */
struct sec_nfc_platform_data {
	unsigned int irq;
	unsigned int ven;
	unsigned int firm;
//	unsigned int tvdd;
	void	(*cfg_gpio)(void);
	u32 ven_gpio_flags;
	u32 firm_gpio_flags;
//	u32 tvdd_gpio_flags;
	u32 irq_gpio_flags;
};

enum sec_nfc_state {
	SEC_NFC_ST_OFF = 0,
	SEC_NFC_ST_NORM,
	SEC_NFC_ST_FIRM,
	SEC_NFC_ST_COUNT,
};
