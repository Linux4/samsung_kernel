/*
 * drivers/battery/s2mpw01_charger.h
 *
 * Header of S2MPW01 Charger Driver
 *
 * Copyright (C) 2015 Samsung Electronics
 * Develope by Nguyen Tien Dat <tiendat.nt@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef S2MPW01_CHARGER_H
#define S2MPW01_CHARGER_H
#include <linux/mfd/samsung/s2mpw01.h>
#include <linux/mfd/samsung/s2mpw01-private.h>

#define MASK(width, shift)      (((0x1 << (width)) - 1) << shift)

#define S2MPW01_CHG_REG_INT1		0x00
#define S2MPW01_CHG_REG_INT2		0x01
#define S2MPW01_CHG_REG_INT3		0x02
#define S2MPW01_CHG_REG_INT1M		0x03
#define S2MPW01_CHG_REG_INT2M		0x04
#define S2MPW01_CHG_REG_INT3M		0x05
#define S2MPW01_CHG_REG_STATUS1		0x06
#define S2MPW01_CHG_REG_STATUS2		0x07
#define S2MPW01_CHG_REG_STATUS3		0x08
#define S2MPW01_CHG_REG_CTRL1       0x09
#define S2MPW01_CHG_REG_CTRL2       0x0A
#define S2MPW01_CHG_REG_CTRL3       0x0B
#define S2MPW01_CHG_REG_CTRL4       0x0C
#define S2MPW01_CHG_REG_CTRL5       0x0D
#define S2MPW01_CHG_REG_CTRL6       0x0E
#define S2MPW01_CHG_REG_CTRL7       0x0F
#define S2MPW01_CHG_REG_CTRL8       0x10
#define S2MPW01_CHG_REG_CTRL9		0x11


/* S2MPW01_CHG_STATUS1 */
#define CHG_STATUS1_RE_CHG	0
#define CHG_STATUS1_CHG_DONE 1
#define CHG_STATUS1_TOP_OFF	2
#define	CHG_STATUS1_PRE_CHG	3
#define CHG_STATUS1_CHG_STS	4
#define CHG_STATUS1_CIN2BAT	5
#define CHG_STATUS1_CHGVINOVP 6
#define CHG_STATUS1_CHGVIN	7

/* S2MPW01_CHG_STATUS2 */
#define DET_BAT_STATUS_SHIFT	7
#define DET_BAT_STATUS_MASK	BIT(DET_BAT_STATUS_SHIFT)

/* S2MPW01_CHG_CTRL1 */
#define EN_CHG_SHIFT		7
#define EN_CHG_MASK		BIT(EN_CHG_SHIFT)

/* S2MPW01_CHG_CTRL2 */
#define FAST_CHARGING_CURRENT_SHIFT	1
#define FAST_CHARGING_CURRENT_WIDTH	3
#define FAST_CHARGING_CURRENT_MASK	MASK(FAST_CHARGING_CURRENT_WIDTH,\
					FAST_CHARGING_CURRENT_SHIFT)

/* S2MPW01_CHG_CTRL4 */
#define FIRST_TOPOFF_CURRENT_SHIFT	4
#define FIRST_TOPOFF_CURRENT_WIDTH	4
#define FIRST_TOPOFF_CURRENT_MASK	MASK(FIRST_TOPOFF_CURRENT_WIDTH,\
					FIRST_TOPOFF_CURRENT_SHIFT)
#define SET_VIN_DROP_SHIFT	1
#define SET_VIN_DROP_WIDTH	3
#define SET_VIN_DROP_MASK	MASK(SET_VIN_DROP_WIDTH, SET_VIN_DROP_SHIFT)


/* S2MPW01_CHG_CTRL5 */
#define SET_VF_VBAT_SHIFT	1
#define SET_VF_VBAT_WIDTH	3
#define SET_VF_VBAT_MASK	MASK(SET_VF_VBAT_WIDTH, SET_VF_VBAT_SHIFT)

#define FAKE_BAT_LEVEL                  50

enum {
	CHG_REG = 0,
	CHG_DATA,
	CHG_REGS,
};

struct charger_info {
	int dummy;
};

#endif /*S2MPW01_CHARGER_H*/
