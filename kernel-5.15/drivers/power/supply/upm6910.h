/*
 * BQ2560x battery charging driver
 *
 * Copyright (C) 2013 Texas Instruments
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef _LINUX_BQ2560X_I2C_H
#define _LINUX_BQ2560X_I2C_H

#include <linux/power_supply.h>
/* +S96818AA1-6688, zhouxiaopeng2.wt, MODIFY, 20230629, UI_soc reaches 100% after charging stops */
//enum {
//	EVENT_EOC,
//	EVENT_RECHARGE,
//	CHARGER_DEV_NOTIFY_EOC,
//	CHARGER_DEV_NOTIFY_RECHG,
//};
/* -S96818AA1-6688, zhouxiaopeng2.wt, MODIFY, 20230629, UI_soc reaches 100% after charging stops */

/* Register 00h */
#define UPM6910_REG_00      		0x00
#define REG00_ENHIZ_MASK		    0x80
#define REG00_ENHIZ_SHIFT		    7
#define	REG00_HIZ_ENABLE			1
#define	REG00_HIZ_DISABLE			0

#define	REG00_STAT_CTRL_MASK		0x60
#define REG00_STAT_CTRL_SHIFT		5
#define	REG00_STAT_CTRL_STAT		0
#define	REG00_STAT_CTRL_ICHG		1
#define	REG00_STAT_CTRL_IINDPM		2
#define	REG00_STAT_CTRL_DISABLE		3

#define REG00_IINLIM_MASK		    0x1F
#define REG00_IINLIM_SHIFT			0
#define	REG00_IINLIM_LSB			100
#define	REG00_IINLIM_BASE			100

/* Register 01h */
#define UPM6910_REG_01		    	0x01
#define REG01_PFM_DIS_MASK	      	0x80
#define	REG01_PFM_DIS_SHIFT			7
#define	REG01_PFM_ENABLE			0
#define	REG01_PFM_DISABLE			1

#define REG01_WDT_RESET_MASK		0x40
#define REG01_WDT_RESET_SHIFT		6
#define REG01_WDT_RESET				1

#define	REG01_OTG_CONFIG_MASK		0x20
#define	REG01_OTG_CONFIG_SHIFT		5
#define	REG01_OTG_ENABLE			1
#define	REG01_OTG_DISABLE			0

#define REG01_CHG_CONFIG_MASK     	0x10
#define REG01_CHG_CONFIG_SHIFT    	4
#define REG01_CHG_DISABLE        	0
#define REG01_CHG_ENABLE         	1

#define REG01_SYS_MINV_MASK       	0x0E
#define REG01_SYS_MINV_SHIFT      	1

#define	REG01_MIN_VBAT_SEL_MASK		0x01
#define	REG01_MIN_VBAT_SEL_SHIFT	0
#define	REG01_MIN_VBAT_2P8V			0
#define	REG01_MIN_VBAT_2P5V			1

/* Register 0x02*/
#define UPM6910_REG_02              0x02
#define	REG02_BOOST_LIM_MASK		0x80
#define	REG02_BOOST_LIM_SHIFT		7
#define	REG02_BOOST_LIM_0P5A		0
#define	REG02_BOOST_LIM_1P2A		1

#define	REG02_Q1_FULLON_MASK		0x40
#define	REG02_Q1_FULLON_SHIFT		6
#define	REG02_Q1_FULLON_ENABLE		1
#define	REG02_Q1_FULLON_DISABLE		0

#define REG02_ICHG_MASK           	0x3F
#define REG02_ICHG_SHIFT          	0
#define REG02_ICHG_BASE           	0
#define REG02_ICHG_LSB            	60
#define REG02_ICHG_MIN				840

/* Register 0x03*/
#define UPM6910_REG_03              	0x03
#define REG03_IPRECHG_MASK        	0xF0
#define REG03_IPRECHG_SHIFT       	4
#define REG03_IPRECHG_BASE        	60
#define REG03_IPRECHG_LSB         	60

#define REG03_ITERM_MASK          	0x0F
#define REG03_ITERM_SHIFT         	0
#define REG03_ITERM_BASE          	60
#define REG03_ITERM_LSB           	60

/* Register 0x04*/
#define UPM6910_REG_04              0x04
#define REG04_VREG_MASK             0xF8
#define REG04_VREG_SHIFT            3
#define REG04_VREG_BASE             3856
#define REG04_VREG_LSB              32
#define REG04_UPM_VREG_BASE         3848
#define REG04_UPM_VREG_MIN          3848
#define REG04_UPM_VREG_MAX          4808

#define	REG04_TOPOFF_TIMER_MASK		0x06
#define	REG04_TOPOFF_TIMER_SHIFT	1
#define	REG04_TOPOFF_TIMER_DISABLE	0
#define	REG04_TOPOFF_TIMER_15M		1
#define	REG04_TOPOFF_TIMER_30M		2
#define	REG04_TOPOFF_TIMER_45M		3

#define REG04_VRECHG_MASK         	0x01
#define REG04_VRECHG_SHIFT        	0
#define REG04_VRECHG_100MV        	0
#define REG04_VRECHG_200MV        	1

/* Register 0x05*/
#define UPM6910_REG_05             	0x05
#define REG05_EN_TERM_MASK        	0x80
#define REG05_EN_TERM_SHIFT       	7
#define REG05_TERM_ENABLE         	1
#define REG05_TERM_DISABLE        	0

#define REG05_WDT_MASK            	0x30
#define REG05_WDT_SHIFT           	4
#define REG05_WDT_DISABLE         	0
#define REG05_WDT_40S             	1
#define REG05_WDT_80S             	2
#define REG05_WDT_160S            	3
#define REG05_WDT_BASE            	0
#define REG05_WDT_LSB             	40

#define REG05_EN_TIMER_MASK       	0x08
#define REG05_EN_TIMER_SHIFT      	3
#define REG05_CHG_TIMER_ENABLE    	1
#define REG05_CHG_TIMER_DISABLE   	0

#define REG05_CHG_TIMER_MASK      	0x04
#define REG05_CHG_TIMER_SHIFT     	2
#define REG05_CHG_TIMER_5HOURS    	0
#define REG05_CHG_TIMER_10HOURS   	1

#define	REG05_TREG_MASK				0x02
#define	REG05_TREG_SHIFT			1
#define	REG05_TREG_90C				0
#define	REG05_TREG_110C				1

#define REG05_JEITA_ISET_MASK     	0x01
#define REG05_JEITA_ISET_SHIFT    	0
#define REG05_JEITA_ISET_50PCT    	0
#define REG05_JEITA_ISET_20PCT    	1

/* Register 0x06*/
#define UPM6910_REG_06              0x06
#define	REG06_OVP_MASK				0xC0
#define	REG06_OVP_SHIFT				0x6
#define	REG06_OVP_5P5V				0
#define	REG06_OVP_6P5V				1
#define	REG06_OVP_10P5V				2
#define	REG06_OVP_14P0V				3

#define	REG06_BOOSTV_MASK			0x30
#define	REG06_BOOSTV_SHIFT			4
#define	REG06_BOOSTV_4P85V			0
#define	REG06_BOOSTV_5V				1
#define	REG06_BOOSTV_5P15V			2
#define	REG06_BOOSTV_5P3V			3

#define	REG06_VINDPM_MASK			0x0F
#define	REG06_VINDPM_SHIFT			0
#define	REG06_VINDPM_BASE			3900
#define	REG06_VINDPM_LSB			100

/* Register 0x07*/
#define UPM6910_REG_07              0x07
#define REG07_FORCE_DPDM_MASK     	0x80
#define REG07_FORCE_DPDM_SHIFT    	7
#define REG07_FORCE_DPDM          	1

#define REG07_TMR2X_EN_MASK       	0x40
#define REG07_TMR2X_EN_SHIFT      	6
#define REG07_TMR2X_ENABLE        	1
#define REG07_TMR2X_DISABLE       	0

#define REG07_BATFET_DIS_MASK     	0x20
#define REG07_BATFET_DIS_SHIFT    	5
#define REG07_BATFET_OFF          	1
#define REG07_BATFET_ON          	0

#define REG07_JEITA_VSET_MASK     	0x10
#define REG07_JEITA_VSET_SHIFT    	4
#define REG07_JEITA_VSET_4100     	0
#define REG07_JEITA_VSET_VREG     	1

#define	REG07_BATFET_DLY_MASK		0x08
#define	REG07_BATFET_DLY_SHIFT		3
#define	REG07_BATFET_DLY_0S			0
#define	REG07_BATFET_DLY_10S		1

#define	REG07_BATFET_RST_EN_MASK	0x04
#define	REG07_BATFET_RST_EN_SHIFT	2
#define	REG07_BATFET_RST_DISABLE	0
#define	REG07_BATFET_RST_ENABLE		1

#define	REG07_VDPM_BAT_TRACK_MASK	0x03
#define	REG07_VDPM_BAT_TRACK_SHIFT 	0
#define	REG07_VDPM_BAT_TRACK_DISABLE	0
#define	REG07_VDPM_BAT_TRACK_200MV	1
#define	REG07_VDPM_BAT_TRACK_250MV	2
#define	REG07_VDPM_BAT_TRACK_300MV	3

/* Register 0x08*/
#define UPM6910_REG_08              0x08
#define REG08_VBUS_STAT_MASK      0xE0
#define REG08_VBUS_STAT_SHIFT     	5
#define REG08_VBUS_TYPE_NONE	  	0
#define REG08_VBUS_TYPE_SDP       	1
#define REG08_VBUS_TYPE_CDP       	2
#define REG08_VBUS_TYPE_DCP			3
#define REG08_VBUS_TYPE_UNKNOWN		5
#define REG08_VBUS_TYPE_NON_STD		6
#define REG08_VBUS_TYPE_OTG       	7

#define REG08_CHRG_STAT_MASK      0x18
#define REG08_CHRG_STAT_SHIFT     3
#define REG08_CHRG_STAT_IDLE      0
#define REG08_CHRG_STAT_PRECHG    1
#define REG08_CHRG_STAT_FASTCHG   2
#define REG08_CHRG_STAT_CHGDONE   3

#define REG08_PG_STAT_MASK        0x04
#define REG08_PG_STAT_SHIFT       2
#define REG08_POWER_GOOD          1

#define REG08_THERM_STAT_MASK     0x02
#define REG08_THERM_STAT_SHIFT    1

#define REG08_VSYS_STAT_MASK      0x01
#define REG08_VSYS_STAT_SHIFT     0
#define REG08_IN_VSYS_STAT        1

/* Register 0x09*/
#define UPM6910_REG_09              0x09
#define REG09_FAULT_WDT_MASK      0x80
#define REG09_FAULT_WDT_SHIFT     7
#define REG09_FAULT_WDT           1

#define REG09_FAULT_BOOST_MASK    0x40
#define REG09_FAULT_BOOST_SHIFT   6

#define REG09_FAULT_CHRG_MASK     0x30
#define REG09_FAULT_CHRG_SHIFT    4
#define REG09_FAULT_CHRG_NORMAL   0
#define REG09_FAULT_CHRG_INPUT    1
#define REG09_FAULT_CHRG_THERMAL  2
#define REG09_FAULT_CHRG_TIMER    3

#define REG09_FAULT_BAT_MASK      0x08
#define REG09_FAULT_BAT_SHIFT     3
#define	REG09_FAULT_BAT_OVP		1

#define REG09_FAULT_NTC_MASK      0x07
#define REG09_FAULT_NTC_SHIFT     0
#define	REG09_FAULT_NTC_NORMAL		0
#define REG09_FAULT_NTC_WARM      2
#define REG09_FAULT_NTC_COOL      3
#define REG09_FAULT_NTC_COLD      5
#define REG09_FAULT_NTC_HOT       6

/* Register 0x0A */
#define UPM6910_REG_0A              0x0A
#define	REG0A_VBUS_GD_MASK			0x80
#define	REG0A_VBUS_GD_SHIFT			7
#define	REG0A_VBUS_GD				1

#define	REG0A_VINDPM_STAT_MASK		0x40
#define	REG0A_VINDPM_STAT_SHIFT		6
#define	REG0A_VINDPM_ACTIVE			1

#define	REG0A_IINDPM_STAT_MASK		0x20
#define	REG0A_IINDPM_STAT_SHIFT		5
#define	REG0A_IINDPM_ACTIVE			1

#define	REG0A_TOPOFF_ACTIVE_MASK	0x08
#define	REG0A_TOPOFF_ACTIVE_SHIFT	3
#define	REG0A_TOPOFF_ACTIVE			1

#define	REG0A_ACOV_STAT_MASK		0x04
#define	REG0A_ACOV_STAT_SHIFT		2
#define	REG0A_ACOV_ACTIVE			1

#define	REG0A_VINDPM_INT_MASK		0x02
#define	REG0A_VINDPM_INT_SHIFT		1
#define	REG0A_VINDPM_INT_ENABLE		0
#define	REG0A_VINDPM_INT_DISABLE	1

#define	REG0A_IINDPM_INT_MASK		0x01
#define	REG0A_IINDPM_INT_SHIFT		0
#define	REG0A_IINDPM_INT_ENABLE		0
#define	REG0A_IINDPM_INT_DISABLE	1

#define	REG0A_INT_MASK_MASK			0x03
#define	REG0A_INT_MASK_SHIFT		0

/* Register 0x0B */
#define	UPM6910_REG_0B				0x0B
#define	REG0B_REG_RESET_MASK		0x80
#define	REG0B_REG_RESET_SHIFT		7
#define	REG0B_REG_RESET				1

#define REG0B_PN_MASK             	0x78
#define REG0B_PN_SHIFT            	3

#define REG0B_DEV_REV_MASK        	0x03
#define REG0B_DEV_REV_SHIFT       	0

/* Register 0x0C */
#define	UPM6910_REG_0C				0x0C
#define	REG0C_EN_HVDCP_MASK			0x80
#define	REG0C_EN_HVDCP_SHIFT		7
#define	REG0C_EN_HVDCP_ENABLE		1
#define	REG0C_EN_HVDCP_DISABLE		0

#define	REG0C_EN_BC12_MASK			0x40
#define	REG0C_EN_BC12_SHIFT			6
#define	REG0C_EN_BC12_ENABLE		1

#define	REG0C_DP_MUX_MASK			0x18
#define	REG0C_DP_MUX_SHIFT			3

#define	REG0C_DM_MUX_MASK			0x06
#define	REG0C_DM_MUX_SHIFT			1

#define	REG0C_DPDM_OUT_HIZ			0
#define	REG0C_DPDM_OUT_0V			1
#define	REG0C_DPDM_OUT_0P6V			2
#define	REG0C_DPDM_OUT_3P3V			3

/* Register 0x0D */
#define UPM6922P_REG_0D                 0x0D
#define REG0D_VREG_FT_MASK              0xC0
#define REG0D_VREG_FT_SHIFT             6
#define REG0D_VREG_FT_LSB               8

#define REG0D_V_RRE2FAST_MASK       0x0C
#define REG0D_V_RRE2FAST_SHIFT      2
#define REG0D_PRE2FAST_3V           0
#define REG0D_PRE2FAST_2P9V         1
#define REG0D_PRE2FAST_2P8V         2
#define REG0D_PRE2FAST_2P7V         3

#define REG0D_VINDPM_OS_MASK        0x03
#define REG0D_VINDPM_OS_SHIFT       0
#define REG0D_VINDPM_OS_3P9V        0
#define REG0D_VINDPM_OS_5P9V        1
#define REG0D_VINDPM_OS_7P5V        2
#define REG0D_VINDPM_OS_10P5V       3

#define	REG06_VINDPM_3P9V			3900
#define	REG06_VINDPM_5P4V			5400
#define	REG06_VINDPM_5P9V			5900
#define	REG06_VINDPM_7P5V			7500
#define	REG06_VINDPM_9P0V			9000
#define	REG06_VINDPM_10P5V			10500
#define	REG06_VINDPM_12P0V			12000
/* +S98901AA1 liangjianfeng wt, modify, 20240819, modify for add sgm41513 adapter */
/* Register 0x0F */
#define SGM41513D_REG_0F                0x0F
#define REG0F_VREG_FT_MASK              0xC0
#define REG0F_VREG_FT_SHIFT             6
/* -S98901AA1 liangjianfeng wt, modify, 20240819, modify for add sgm41513 adapter */
/* Register 0x10 */
#define UPM6922P_REG_10                 0x10
#define REG10_VSYS_MASK                 0x7F
#define REG10_VSYS_SHIFT                0
#define REG10_VSYS_LSB                  20
#define REG10_VSYS_BASE                 2304
#define REG10_VSYS_INVALID              4844

/* Register 0xA9 */
#define UPM6922_REG_A9                  0xA9
#define UPM6922_DEVICE_ENTER            0x6E
#define UPM6922_DEVICE_EXIT             0x00

/* Register 0xAB */
#define UPM6922_REG_AB                  0XAB
#define REGAB_SCAN_CUR_MASK             0X0C
#define REGAB_SCAN_CUR_SHIFT            2
#define REGAB_SCAN_CUR_SET              2

/* Register 0xD2 */
#define UPM6922_REG_D2                  0xD2
#define UPM6922_DEVICE_DETECT_MASK      0x40
#define UPM6922_DEVICE_DETECT_SHIFT     6

#define	SGM41513D_REG_0D			0x0D

struct upm6910_charge_param {
	int vlim;
	int ilim;
	int ichg;
	int vreg;
};

enum stat_ctrl {
	STAT_CTRL_STAT,
	STAT_CTRL_ICHG,
	STAT_CTRL_INDPM,
	STAT_CTRL_DISABLE,
};

enum vboost {
	BOOSTV_4850 = 4850,
	BOOSTV_5000 = 5000,
	BOOSTV_5150 = 5150,
	BOOSTV_5300 = 5300,
};

enum iboost {
	BOOSTI_500 = 500,
	BOOSTI_1200 = 1200,
};

enum vac_ovp {
	VAC_OVP_5500 = 5500,
	VAC_OVP_6500 = 6500,
	VAC_OVP_10500 = 10500,
	VAC_OVP_14000 = 14000,
};

enum charg_stat {
	CHRG_STAT_NOT_CHARGING,
	CHRG_STAT_PRE_CHARGINT,
	CHRG_STAT_FAST_CHARGING,
	CHRG_STAT_CHARGING_TERM,
};

#if 0
enum wt_power_supply_type {
	POWER_SUPPLY_TYPE_UNKNOWN = 0,
	POWER_SUPPLY_TYPE_BATTERY,
	POWER_SUPPLY_TYPE_UPS,
	POWER_SUPPLY_TYPE_MAINS,
	POWER_SUPPLY_TYPE_USB,			/* Standard Downstream Port */
	POWER_SUPPLY_TYPE_USB_DCP,		/* Dedicated Charging Port */
	POWER_SUPPLY_TYPE_USB_CDP,		/* Charging Downstream Port */
	POWER_SUPPLY_TYPE_USB_ACA,		/* Accessory Charger Adapters */
	POWER_SUPPLY_TYPE_USB_TYPE_C,		/* Type C Port */
	POWER_SUPPLY_TYPE_USB_PD,		/* Power Delivery Port */
	POWER_SUPPLY_TYPE_USB_PD_DRP,		/* PD Dual Role Port */
	POWER_SUPPLY_TYPE_APPLE_BRICK_ID,	/* Apple Charging Method */
	POWER_SUPPLY_TYPE_WIRELESS,		/* Wireless */
	POWER_SUPPLY_TYPE_USB_FLOAT,    /* Floating charger*/
};

enum wt_charger_type {
        CHARGER_UNKNOWN = 0,
        STANDARD_HOST,          /* USB : 450mA */
        CHARGING_HOST,
        NONSTANDARD_CHARGER,    /* AC : 450mA~1A */
        STANDARD_CHARGER,       /* AC : ~1A */
        APPLE_2_4A_CHARGER, /* 2.4A apple charger */
        APPLE_2_1A_CHARGER, /* 2.1A apple charger */
        APPLE_1_0A_CHARGER, /* 1A apple charger */
        APPLE_0_5A_CHARGER, /* 0.5A apple charger */
        SAMSUNG_CHARGER,
        WIRELESS_CHARGER,
};
#endif
enum pmic_iio_channels {
	VBUS_VOLTAGE,
};

static const char * const pmic_iio_chan_name[] = {
	[VBUS_VOLTAGE] = "vbus_dect",
};


struct upm6910_platform_data {
	struct upm6910_charge_param usb;
	int iprechg;
	int iterm;

	enum stat_ctrl statctrl;
	enum vboost boostv;	// options are 4850,
	enum iboost boosti; // options are 500mA, 1200mA
	enum vac_ovp vac_ovp;

};

#endif
