/*
 *  include/linux/regulator/rt5746-regulator.h
 *  Include header file to Richtek RT5746 Regulator driver
 *
 *  Copyright (C) 2014 Richtek Technology Corp.
 *  cy_huang <cy_huang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef __LINUX_RT5746_REGULATOR_H
#define __LINUX_RT5746_REGULATOR_H

#define RT5746_DRV_VER	"1.0.0_G"
#define RT5746_DEV_NAME	"rt5746"
#define RT5746_VEN_ID	0x88

struct rt5746_platform_data {
	struct regulator_init_data *regulator;
	int irq_gpio;
	int standby_uV;
	unsigned int rampup_sel:3;
	unsigned int rampdn_sel:2;
	unsigned int ipeak_sel:2;
	unsigned int tpwth_sel:2;
	unsigned int rearm_en:1;
	unsigned int discharge_en:1;
};

enum {
	RT5746_REG_INTACK,
	RT5746_REG_RANGE1START = RT5746_REG_INTACK,
	RT5746_REG_INTSEN,
	RT5746_REG_INTMSK,
	RT5746_REG_PID,
	RT5746_REG_REVID,
	RT5746_REG_FID,
	RT5746_REG_VID,
	RT5746_REG_RANGE1END = RT5746_REG_VID,
	RT5746_REG_PROGVSEL1 = 0x10,
	RT5746_REG_RANGE2START = RT5746_REG_PROGVSEL1,
	RT5746_REG_PROGVSEL0,
	RT5746_REG_PGOOD,
	RT5746_REG_TIME,
	RT5746_REG_COMMAND,
	RT5746_REG_RANGE2END = RT5746_REG_COMMAND,
	RT5746_REG_LIMCONF = 0x16,
	RT5746_REG_RANGE3START = RT5746_REG_LIMCONF,
	RT5746_REG_RANGE3END = RT5746_REG_LIMCONF,
	RT5746_REG_MAX,
};

enum {
	RT5746_IRQ_PGOOD,
	RT5746_IRQ_IDCDC,
	RT5746_IRQ_UVLO,
	RT5746_IRQ_TPREW = 5,
	RT5746_IRQ_TWARN,
	RT5746_IRQ_TSD,
	RT5746_IRQ_MAX,
};

enum {
	RT5746_RAMPUP_64mV,
	RT5746_RAMPUP_16mV,
	RT5746_RAMPUP_32mV_1,
	RT5746_RAMPUP_8mV_1,
	RT5746_RAMPUP_4mV_1,
	RT5746_RAMPUP_4mV,
	RT5746_RAMPUP_32mV,
	RT5746_RAMPUP_8mV,
	RT5746_RAMPUP_MAX = RT5746_RAMPUP_8mV,
};

enum {
	RT5746_RAMPDN_32mV,
	RT5746_RAMPDN_4mV,
	RT5746_RAMPDN_8mV,
	RT5746_RAMPDN_16mV,
	RT5746_RAMPDN_MAX = RT5746_RAMPDN_16mV,
};

enum {
	RT5746_IPEAK_4P5A,
	RT5746_IPEAK_5P0A,
	RT5746_IPEAK_5P5A,
	RT5746_IPEAK_6P0A,
	RT5746_IPEAK_MAX = RT5746_IPEAK_6P0A,
};

enum {
	RT5746_TPWTH_83C,
	RT5746_TPWTH_94C,
	RT5746_TPWTH_105C,
	RT5746_TPWTH_116C,
	RT5746_TPWTH_MAX = RT5746_TPWTH_116C,
};

#ifdef CONFIG_RT5746_VSEL_ACTIVEHIGH
#define RT5746_REG_BUCKVN RT5746_REG_PROGVSEL1
#define RT5746_REG_BUCKVS RT5746_REG_PROGVSEL0
#else
#define RT5746_REG_BUCKVN RT5746_REG_PROGVSEL0
#define RT5746_REG_BUCKVS RT5746_REG_PROGVSEL1
#endif /* #ifdef CONFIG_RT5746_VSEL_ACTIVE_HIGH */

#define RT5746_BUCKVOUT_SHIFT	0
#define RT5746_BUCKVOUT_MASK	0x7F

#define RT5746_RAMPUP_MASK	0x1C
#define RT5746_RAMPUP_SHIFT	2
#define RT5746_RAMPDN_MASK	0x06
#define RT5746_RAMPDN_SHIFT	1
#define RT5746_IPEAK_MASK	0xC0
#define RT5746_IPEAK_SHIFT	6
#define RT5746_TPWTH_MASK	0x30
#define RT5746_TPWTH_SHIFT	4
#define RT5746_REARM_MASK	0x01
#define RT5746_DISCHG_MASK	0x10

#ifdef CONFIG_REGULATOR_RT5746_DBGINFO
#define RTINFO(format, args...) \
	pr_info("%s:%s() line-%d: " format, RT5746_DEV_NAME,__FUNCTION__, \
		__LINE__, ##args)
#else
#define RTINFO(format, args...)
#endif /* #ifdef CONFIG_REGULATOR_RT5746_DBGINFO */

#endif  /* __LINUX_RT5746_REGULATOR_H */
