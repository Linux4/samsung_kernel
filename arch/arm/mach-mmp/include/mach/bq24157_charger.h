/*
 * Copyright (C) 2011, SAMSUNG Corporation.
 * Author: YongTaek Lee  <ytk.lee@samsung.com> 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __BQ24157_CHARGER_H
#define __BQ24157_CHARGER_H

#include <linux/workqueue.h>
#include <linux/wakelock.h>

#define	BQ24157_STAT_IRQ_DELAY	(HZ*1) // 1 sec
#define	BQ24157_SET_CURRENT_LIMIT_DELAY	(HZ*0.5) // 1 sec

struct bq24157_platform_data{
	unsigned int cd;
	struct work_struct	stat_irq_work;
	struct delayed_work	stat_irq_delayed_work;
	struct wake_lock	stat_irq_wakelock;
	struct delayed_work	set_current_limit_work;
	struct wake_lock	set_current_limit_wakelock;
	unsigned int max_charger_current;
	unsigned int max_charger_voltage;
	unsigned int regulation_voltage;
	unsigned int cin_limit_current;
	unsigned int weak_bat_voltage;
	unsigned int charge_ta_current;
	unsigned int charge_usb_current;
	bool low_charge_mode;
	unsigned int dpm_voltage;
	unsigned int temination_current;
	unsigned int lpm_temination_current;
	bool termination_en;
};


/* BQ24157 register map */
#define BQ24157_STATUS_CONTROL		0x0
#define BQ24157_CONTROL			0x1
#define	BQ24157_BATTERY_VOLTAGE		0x2
#define	BQ24157_VENDER_PART_REVISION	0x3
#define	BQ24157_BATTERY_TERMINATION	0x4
#define	BQ24157_CHARGER_VOLTAGE		0x5
#define	BQ24157_SAFETY_LIMIT		0x6


/* BQ24157_STATUS_CONTROL REGISTER */
#define TMR_RST_OTG	(1 << 7)
#define	EN_STAT		(1 << 6)
#define STAT2		(1 << 5)
#define	STAT1		(1 << 4)
#define	BOOST		(1 << 3)
#define	FAULT_3		(1 << 2)
#define FAULT_2		(1 << 1)
#define FAULT_1		(1 << 0)


#define STAT			(STAT1 | STAT2)
#define STAT_READY		0
#define STAT_INPROGRESS		(STAT1)
#define STAT_CHARGE_DONE	(STAT2)
#define STAT_FAULT		(STAT1 | STAT2)

#define FAULT			(FAULT_1 | FAULT_2 | FAULT_3)
#define FAULT_NORMAL		0
#define FAULT_VBUS_OVP		(FAULT_1)
#define FAULT_SLEEP_MODE	(FAULT_2)
#define	FAULT_BAD_ADAPTOR	(FAULT_1 | FAULT_2)
#define FAULT_OUTPUT_OVP	(FAULT_3)
#define FAULT_THEMAL_SHUTDOWN	(FAULT_1 | FAULT_3)
#define FAULT_TIMER_FAULT	(FAULT_2 | FAULT_3)
#define FAULT_NO_BATTERY	(FAULT_1 | FAULT_2 | FAULT_3)	

/* BQ24157_CONTROL REGISTER */
#define	LIN_LIMIT2	(1 << 7)
#define LIN_LIMIT1	(1 << 6)
#define	V_LOWV2		(1 << 5)
#define V_LOWV1		(1 << 4)
#define TE		(1 << 3)
#define CE		(1 << 2)
#define HZ_MODE		(1 << 1)
#define OPA_MODE	(1 << 0)

#define LIN_LIMIT		(LIN_LIMIT1 | LIN_LIMIT2)
#define USB_100MA		0
#define USB_500MA		(LIN_LIMIT1)
#define USB_CHARGER_800MA	(LIN_LIMIT2)
#define NO_LIMIT		(LIN_LIMIT1 | LIN_LIMIT2)

#define V_LOWV			(V_LOWV1 | V_LOWV2)
#define WEAK_BATTERY_3700MV	(V_LOWV1 | V_LOWV2)
#define WEAK_BATTERY_3600MV	(V_LOWV2)
#define WEAK_BATTERY_3500MV	(V_LOWV1)
#define WEAK_BATTERY_3400MV	0

#define TE_ENABLE 	(TE)
#define TE_DISABLE	0
#define	INPUT_CURRENT_LIMIT_SHIFT	6
#define	ENABLE_ITERM_SHIFT		3
#define	WEAK_BATTERY_VOLTAGE_SHIFT	4

/* BQ24157_BATTERY_VOLTAGE REGISTER */
#define VO_REG5		(1 << 7)
#define VO_REG4		(1 << 6)	
#define VO_REG3		(1 << 5)
#define VO_REG2		(1 << 4)
#define VO_REG1		(1 << 3)
#define VO_REG0		(1 << 2)
#define OTG_PL		(1 << 1)
#define OTG_EN		(1 << 0)

#define	VO_REG		(VO_REG0 | VO_REG1 | VO_REG2 | \
			 VO_REG3 | VO_REG4 | VO_REG5 )
 
#define VOLTAGE_4200MV	(VO_REG5 | VO_REG1 | VO_REG0)
#define VOLTAGE_4350MV	(VO_REG5 | VO_REG3 | VO_REG1)
#define VOLTAGE_4360MV	(VO_REG5 | VO_REG3 | VO_REG1 | VO_REG0)
#define VOLTAGE_SHIFT	2

/* BQ24157_VENDER_PART_REVISION REGISTER */
#define VENDER2		(1 << 7)
#define VENDER1		(1 << 6)
#define VENDER0		(1 << 5)
#define	PN1		(1 << 4)
#define PN0		(1 << 3)
#define	REVISION2	(1 << 2)
#define REVISION1	(1 << 1)
#define REVISION0	(1 << 0)

/* BQ24157_BATTERY_TERMINATION REGISTER */
#define	RESET		(1 << 7)
#define	VI_CHRG3	(1 << 6)
#define	VI_CHRG2	(1 << 5)
#define	VI_CHRG1	(1 << 4)
#define	VI_CHRG0	(1 << 3)
#define VI_TERM2	(1 << 2)
#define VI_TERM1	(1 << 1)
#define VI_TERM0	(1 << 0)

#define VI_CHRG		(VI_CHRG1 | VI_CHRG2 | VI_CHRG3) //VI_CHRG0 is NA for bq24157
#define VI_CHRG_550MA	0
#define VI_CHRG_650MA	(VI_CHRG1)
#define VI_CHRG_750MA	(VI_CHRG2)
#define VI_CHRG_850MA	(VI_CHRG1 | VI_CHRG2)
#define VI_CHRG_950MA	(VI_CHRG3)
#define VI_CHRG_1050MA	(VI_CHRG1 | VI_CHRG3)
#define VI_CHRG_1150MA	(VI_CHRG2 | VI_CHRG3)
#define VI_CHRG_1250MA	(VI_CHRG1 | VI_CHRG2 | VI_CHRG3)

#define VI_TERM		(VI_TERM0 | VI_TERM1 | VI_TERM2)
#define VI_TERM_50MA	0
#define VI_TERM_100MA	(VI_TERM0)
#define VI_TERM_150MA	(VI_TERM1)
#define VI_TERM_200MA	(VI_TERM0 | VI_TERM1)
#define VI_TERM_250MA	(VI_TERM2)
#define VI_TERM_300MA	(VI_TERM0 | VI_TERM2)
#define VI_TERM_350MA	(VI_TERM1 | VI_TERM2)
#define VI_TERM_400MA	(VI_TERM0 | VI_TERM1 | VI_TERM2)
#define CHARGE_CURRENT_SHIFT	4

/* BQ24157_CHARGER_VOLTAGE REGISTER */
#define LOW_CHG		(1 << 5)
#define DPM_STATUS	(1 << 4)
#define CD_STATUS	(1 << 3)
#define VSREG2		(1 << 2)
#define VSREG1		(1 << 1)
#define VSREG0		(1 << 0)

#define DPM_MASK (VSREG0 | VSREG1 | VSREG2)
#define DPM_4600 (VSREG2 | VSREG0)

#define LOW_CHG_CURRENT		LOW_CHG
#define NORMAL_CHG_CURRENT	0

/* BQ24157_SAFETY_LIMIT REGISTER */
#define VMCHRG3		(1 << 7)
#define VMCHRG2		(1 << 6)
#define VMCHRG1		(1 << 5)
#define VMCHRG0		(1 << 4)
#define VMREG3		(1 << 3)
#define VMREG2		(1 << 2)
#define VMREG1		(1 << 1)
#define VMREG0		(1 << 0)

#define LIMIT_VOLTAGE_MASK (VMREG0 | VMREG1 | VMREG2 | VMREG3)
#define LIMIT_VOLTAGE_4340MV	(VMREG0 | VMREG1 | VMREG2)
#define VMREG (VMREG0 | VMREG1 | VMREG2 | VMREG3 | VMCHRG0 | VMCHRG1 | VMCHRG2 | VMCHRG3)
#define LIMIT_4360MV_1150MA (VMREG3 | VMCHRG1 | VMCHRG2)
#define LIMIT_4200MV_1150MA (VMCHRG1 | VMCHRG2)
#define	MAX_CURRENT_SHIFT		4

#endif
