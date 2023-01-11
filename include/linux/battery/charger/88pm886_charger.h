/*
 * 88pm886_charger.h
 * 88PM886 PMIC Charger Header
 *
 * Copyright (C) 2014 Marvell Technology Ltd.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __88PM886_CHARGER_H
#define __88PM886_CHARGER_H __FILE__
#include <linux/mfd/88pm886.h>

#define CHG_RESTART_DELAY		(5) /* in minutes */

#define PM886_ID_REG_CHG		(0x00)

#define PM886_CC_CONFIG1		(0x01)
#define PM886_CC_CONFIG2		(0x02)

#define PM886_IBAT_OFFSET_VAL	(0x0C)
#define PM886_SD_CONFIG1		(0x0D)
#define PM886_SD_CONFIG2		(0x0E)

#define PM886_IBAT_EOC_CONFIG1	(0x0F)

#define PM886_CHG_CONFIG4		(0x2B)

#define PM886_PRE_CONFIG1		(0x2D)

#define PM886_EXT_THRM1			(0x35)
#define PM886_EXT_THRM2			(0x36)

#define PM886_MPPT_CONFIG1		(0x3E)

#define PM886_OTG_LOG1			(0x47)

#define PM886_CHG_CONFIG1		(0x28)
#define PM886_CHG_ENABLE		(1 << 0)
#define PM886_CHG_WDT_EN		(1 << 1)
#define PM886_CHG_TERM_DIS		(1 << 3)
#define PM886_BATTEMP_MON_EN	(1 << 4)
#define PM886_BATTEMP_MON2_DIS	(1 << 5)
#define PM886_USB_SUSP			(1 << 6)
#define PM886_USB_OTG			(1 << 7)

#define PM886_CHG_CONFIG3		(0x2A)
#define PM886_OV_VBAT_EN		(1 << 0)

#define PM886_PRE_CONFIG1		(0x2D)
#define PM886_ICHG_PRE_SET_OFFSET	(0)
#define PM886_ICHG_PRE_SET_MASK		(0X7 << PM886_ICHG_PRE_SET_OFFSET)
#define PM886_VBAT_PRE_TERM_SET_OFFSET	(4)
#define PM886_VBAT_PRE_TERM_SET_MASK	(0x7 << PM886_VBAT_PRE_TERM_SET_OFFSET)

#define PM886_FAST_CONFIG1		(0x2E)
#define PM886_VBAT_FAST_SET_MASK	(0x7F << 0)

#define PM886_FAST_CONFIG2		(0x2F)
#define PM886_ICHG_FAST_SET_MASK	(0x1F << 0)

#define PM886_MPPT_CONFIG2		(0x3F)
#define MPPT_EN					(1 << 7)

#define PM886_FAST_CONFIG3		(0x30)
#define PM886_IBAT_EOC_TH		(0x3F << 0)

#define PM886_TIMER_CONFIG		(0x31)
#define PM886_FASTCHG_TOUT_OFFSET	(0)
#define PM886_RECHG_THR_SET_50MV	(0x0 << 4)
#define PM886_CHG_ILIM_EXTD_1X5		(0x3 << 6)

#define PM886_EXT_ILIM_CONFIG		(0x34)
#define PM886_CHG_ILIM_RAW_MASK		(0xF << 0)
#define PM886_CHG_ILIM_FINE_OFFSET	(4)
#define PM886_CHG_ILIM_FINE_10		(0x40)
#define PM886_CHG_ILIM_FINE_11_25	(0x30)
#define PM886_CHG_ILIM_FINE_12_5	(0x20)
#define PM886_CHG_ILIM_FINE_13_75	(0x10)
#define PM886_CHG_ILIM_FINE_15		(0x00)

#define PM886_CHG_ILIM_RAW_100		(0x00)
#define PM886_CHG_ILIM_RAW_200		(0x01)
#define PM886_CHG_ILIM_RAW_300		(0x02)
#define PM886_CHG_ILIM_RAW_400		(0x03)
#define PM886_CHG_ILIM_RAW_500		(0x04)
#define PM886_CHG_ILIM_RAW_600		(0x05)
#define PM886_CHG_ILIM_RAW_700		(0x06)
#define PM886_CHG_ILIM_RAW_800		(0x07)
#define PM886_CHG_ILIM_RAW_900		(0x08)
#define PM886_CHG_ILIM_RAW_1000		(0x09)
#define PM886_CHG_ILIM_RAW_1100		(0x0a)
#define PM886_CHG_ILIM_RAW_1200		(0x0b)
#define PM886_CHG_ILIM_RAW_1300		(0x0c)
#define PM886_CHG_ILIM_RAW_1400		(0x0d)
#define PM886_CHG_ILIM_RAW_1500		(0x0e)
#define PM886_CHG_ILIM_RAW_1600		(0x0f)

#define PM886_CHG_STATUS1		(0x42)

#define PM886_CHG_STATUS2		(0x43)
#define PM886_VBUS_SW_OV		(1 << 0)
#define PM886_VBUS_SW_ON		(1 << 1)
#define PM886_USB_LIM_OK		(1 << 3)
#define PM886_CHG_HIZ			(0x00)
#define PM886_CHG_TRICLE		(0x40)
#define PM886_CHG_PRECHG		(0x50)
#define PM886_CHG_FASTCHG		(0x60)

#define PM886_CHG_STATUS3		(0x44)
#define PM886_CHG_LIM			(0xF << 0)

#define PM886_MPPT_CONFIG3		(0x40)
#define PM886_WA_TH_SET_3V9		(0x3 << 0)
#define PM886_WA_TH_SET_4V3		(0x1 << 0)

#define PM886_CHG_LOG1			(0x45)
#define PM886_BATT_REMOVAL		(1 << 0)
#define PM886_CHG_REMOVAL		(1 << 1)
#define PM886_BATT_TEMP_NOK		(1 << 2)
#define PM886_CHG_WDT_EXPIRED		(1 << 3)
#define PM886_OV_VBAT			(1 << 5)
#define PM886_CHG_TIMEOUT		(1 << 6)
#define PM886_OV_ITEMP			(1 << 7)

struct pm886_charger_info {
	struct device *dev;
	struct power_supply ac_chg;
	struct power_supply usb_chg;
	struct power_supply	psy_chg;
	struct delayed_work restart_chg_work;
	struct notifier_block nb;
	struct delayed_work isr_work;
	int ac_chg_online;
	int usb_chg_online;

	struct mutex lock;

	bool		 is_charging;
	unsigned int battery_present;
	unsigned int cable_type;
	unsigned int charging_current;
	unsigned int input_current_limit;
	bool		 is_fullcharged;
	int			 aicl_on;
	int			 status;
	int			 siop_level;

	unsigned int prechg_cur;	/* precharge current limit */
	unsigned int prechg_vol;	/* precharge voltage limit */

	unsigned int fastchg_vol;	/* fastcharge voltage */
	unsigned int fastchg_cur;	/* fastcharge current */
	unsigned int fastchg_eoc;	/* fastcharge end current */
	unsigned int fastchg_tout;	/* fastcharge voltage */

	unsigned int recharge_thr;	/* gap between VBAT_FAST_SET and VBAT */

	unsigned int limit_cur;

	unsigned int allow_basic_charge;
	unsigned int allow_recharge;
	unsigned int allow_chg_after_tout;
	unsigned int allow_chg_after_overvoltage;

	unsigned int charging;
	unsigned int full;

	int irq_nums;
	int irq[7];

	struct pm886_chip *chip;
	int pm886_charger_status;

	sec_battery_platform_data_t	*pdata;
};
#define pm886_charger_info_t \
	struct pm886_charger_info

int sec_chg_get_property(struct power_supply *,
			      enum power_supply_property,
			      union power_supply_propval *);

int sec_chg_set_property(struct power_supply *,
			      enum power_supply_property,
			      const union power_supply_propval *);

#endif /* __SM5701_CHARGER_H */
