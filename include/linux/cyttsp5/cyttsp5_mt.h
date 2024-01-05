/*
 * cyttsp5_mt.h
 * Cypress TrueTouch(TM) Standard Product V5 Multi-touch module.
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

#ifndef _LINUX_CYTTSP5_MT_H
#define _LINUX_CYTTSP5_MT_H

#define CYTTSP5_MT_NAME "cyttsp5_mt"

/* abs settings */
#define CY_IGNORE_VALUE             0xFFFF
/* abs signal capabilities offsets in the frameworks array */
enum cyttsp5_sig_caps {
	CY_SIGNAL_OST,
	CY_MIN_OST,
	CY_MAX_OST,
	CY_FUZZ_OST,
	CY_FLAT_OST,
	CY_NUM_ABS_SET	/* number of signal capability fields */
};

/* abs axis signal offsets in the framworks array  */
enum cyttsp5_sig_ost {
	CY_ABS_X_OST,
	CY_ABS_Y_OST,
	CY_ABS_P_OST,
	CY_ABS_W_OST,
	CY_ABS_ID_OST,
	CY_ABS_MAJ_OST,
	CY_ABS_MIN_OST,
	CY_ABS_OR_OST,
	CY_ABS_TOOL_OST,
	CY_NUM_ABS_OST	/* number of abs signals */
};

enum cyttsp5_mt_platform_flags {
	CY_MT_FLAG_NONE,
	CY_MT_FLAG_HOVER = 0x04,
	CY_MT_FLAG_FLIP = 0x08,
	CY_MT_FLAG_INV_X = 0x10,
	CY_MT_FLAG_INV_Y = 0x20,
	CY_MT_FLAG_VKEYS = 0x40,
	CY_MT_FLAG_NO_TOUCH_ON_LO = 0x80,
};

struct touch_framework {
	const uint16_t  *abs;
	uint8_t         size;
	uint8_t         enable_vkeys;
} __packed;

struct cyttsp5_mt_platform_data {
	struct touch_framework *frmwrk;
	unsigned short flags;
	char const *inp_dev_name;
	int vkeys_x;
	int vkeys_y;
};

#endif /* _LINUX_CYTTSP5_MT_H */
