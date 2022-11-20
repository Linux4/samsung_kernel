/*
 * OmniVision OV882x sensor driver
 *
 * Copyright (C) 2009-2010 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Created:  2010 Jiaquan Su <jqsu@marvell.com>
 * Modified: 2010 Jiaquan Su <jqsu@marvell.com>
 * Modified: 2011 Henry Zhao <xzhao10@marvell.com>
 */


#ifndef _OV882X_H_
#define _OV882X_H_

/*
 * Basic window sizes.  These probably belong somewhere more globally
 * useful.
 */
#define REG_PIDH        0x300A  /* value should be 0x88 */
#define REG_PIDM        0x300B  /* value should be 0x20 or 0x25*/
#define REG_PIDL        0x300C  /* value should be 0x00 */

#define REG_PIDH_VALUE			0x88
#define REG_PIDM_VALUE_8820		0x20
#define REG_PIDM_VALUE_8825		0x25
#define REG_PIDL_VALUE			0x00

#define REG_RESET       0x0103
#define RESET_ACT       0x01    /* Commit reset */
#define RESET_DIS       0x00    /* Reset disable */


struct ov882x_datafmt {
	enum v4l2_mbus_pixelcode	code;
	enum v4l2_colorspace			colorspace;
};

struct regval_list {
	u16 reg_num;
	unsigned char value;
};

struct sensor_reg {
	unsigned int addr;
	unsigned char val;
};

#endif
