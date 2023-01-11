/*
 * Copyright (C) 2014 Marvell Technology Group Ltd.
 *
 * Authors: Guoqing Li <ligq@marvell.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef __LEDS_88PM828X_H__
#define __LEDS_88PM828X_H__

#include <linux/ioctl.h>

#define PM828X_NAME                 "88pm828x"
#define PM828X_LED_NAME             "lcd-backlight-88pm828x"
#define PM828X_I2C_ADDR             0x10

/* Register map */
#define PM828X_CHIP_ID              0x00
#define PM828X_REV_ID               0x01
#define PM828X_STATUS               0x02
#define PM828X_STATUS_FLAG          0x03
#define PM828X_IDAC_CTRL0           0x04
#define PM828X_IDAC_CTRL1           0x05
#define PM828X_IDAC_RAMP_CLK_CTRL0  0x06
#define PM828X_MISC_CTRL            0x07
#define PM828X_IDAC_OUT_CURRENT0    0x08
#define PM828X_IDAC_OUT_CURRENT1    0x09

#define PM828X_ID                   0x28
#define PM828X_RAMP_MODE_DIRECT     0x1
#define PM828X_RAMP_MODE_NON_LINEAR 0x2
#define PM828X_RAMP_MODE_LINEAR     0x4
#define PM828X_RAMP_CLK_8K          0x0
#define PM828X_RAMP_CLK_4K          0x1
#define PM828X_RAMP_CLK_1K          0x2
#define PM828X_RAMP_CLK_500         0x3
#define PM828X_RAMP_CLK_250         0x4
#define PM828X_RAMP_CLK_125         0x5
#define PM828X_RAMP_CLK_62          0x6
#define PM828X_RAMP_CLK_31          0x7
#define PM828X_IDAC_CURRENT_20MA    0xA00
#define PM828X_EN_SHUTDN            (0x0 << 7)
#define PM828X_DIS_SHUTDN           (0x1 << 7)
#define PM828X_EN_PWM               (0x1 << 6)
#define PM828X_EN_SLEEP             (0x1 << 5)
#define PM828X_SINGLE_STR_CONFIG    0x1
#define PM828X_DUAL_STR_CONFIG      0x0

#ifdef __KERNEL__
struct pm828x_platform_data {
	int (*init)(void);
	void (*exit)(void);
	int (*power_on)(void);
	int (*power_off)(void);

	u32 ramp_mode;     /* IDAC current ramp up/down mode */
	u32 idac_current; /* IDAC current setting */
	u32 ramp_clk;      /* set for linear or non-linear mode */
	u32 str_config;    /* single or dual string selection */
};
#endif

#endif /* __LEDS_88PM828X_H__ */
