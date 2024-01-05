/*
 * cyttsp5_core.h
 * Cypress TrueTouch(TM) Standard Product V5 Core driver module.
 * For use with Cypress Txx5xx parts.
 * Supported parts include:
 * TMA5XX
 *
 * Copyright (C) 2012-2013 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#ifndef _LINUX_CYTTSP5_CORE_H
#define _LINUX_CYTTSP5_CORE_H

#include <linux/stringify.h>

#define CYTTSP5_CORE_NAME "cyttsp5_core"

#define CY_DRIVER_NAME TTDA
#define CY_DRIVER_MAJOR 03
#define CY_DRIVER_MINOR 00
#define CY_DRIVER_SUBRELEASE 01

#define CY_DRIVER_REVCTRL 000004

#define CY_DRIVER_VERSION		    \
__stringify(CY_DRIVER_NAME)		    \
"." __stringify(CY_DRIVER_MAJOR)	    \
"." __stringify(CY_DRIVER_MINOR)	    \
"." __stringify(CY_DRIVER_SUBRELEASE)	    \
"." __stringify(CY_DRIVER_REVCTRL)

#define CY_DRIVER_DATE "20130621"	/* YYYYMMDD */

/* x-axis resolution of panel in pixels */
#define CY_PCFG_RESOLUTION_X_MASK 0x7F

/* y-axis resolution of panel in pixels */
#define CY_PCFG_RESOLUTION_Y_MASK 0x7F

/* x-axis, 0:origin is on left side of panel, 1: right */
#define CY_PCFG_ORIGIN_X_MASK 0x80

/* y-axis, 0:origin is on top side of panel, 1: bottom */
#define CY_PCFG_ORIGIN_Y_MASK 0x80

#define CY_TOUCH_SETTINGS_MAX 32

enum cyttsp5_core_platform_flags {
	CY_FLAG_CORE_NONE,
	CY_FLAG_CORE_WAKE_ON_GESTURE,
	CY_FLAG_CORE_POWEROFF_ON_SLEEP,
};

enum cyttsp5_loader_platform_flags {
	CY_FLAG_LOAD_NONE,
	CY_LOADER_FLAG_CALIBRATE_AFTER_FW_UPGRADE,
};

struct touch_settings {
	const uint8_t   *data;
	uint32_t         size;
	const uint8_t   *ver;
	uint32_t         vsize;
	uint8_t         tag;
};

struct cyttsp5_touch_firmware {
	const uint8_t *img;
	uint32_t size;
	const uint8_t *ver;
	uint8_t vsize;
};

struct cyttsp5_touch_config {
	struct touch_settings *param_regs;
	struct touch_settings *param_size;
	const uint8_t *fw_ver;
	uint8_t fw_vsize;
};

struct cyttsp5_loader_platform_data {
	struct cyttsp5_touch_firmware *fw;
	struct cyttsp5_touch_config *ttconfig;
	u32 flags;
};

struct cyttsp5_core_platform_data {
	int irq_gpio;
	int rst_gpio;
	int level_irq_udelay;
	u16 hid_desc_register;
	u16 vendor_id;
	u16 product_id;
	int (*xres)(struct cyttsp5_core_platform_data *pdata,
		struct device *dev);
	int (*init)(struct cyttsp5_core_platform_data *pdata,
		int on, struct device *dev);
	int (*power)(struct cyttsp5_core_platform_data *pdata,
		int on, struct device *dev, atomic_t *ignore_irq);
	int (*irq_stat)(struct cyttsp5_core_platform_data *pdata,
		struct device *dev);
	struct touch_settings *sett[CY_TOUCH_SETTINGS_MAX];
	struct cyttsp5_loader_platform_data *loader_pdata;
	u32 flags;
};

#ifdef VERBOSE_DEBUG
extern void cyttsp5_pr_buf(struct device *dev, u8 *pr_buf, u8 *dptr, int size,
			   const char *data_name);
#else
#define cyttsp5_pr_buf(a, b, c, d, e) do { } while (0)
#endif

#endif /* _LINUX_CYTTSP5_CORE_H */
