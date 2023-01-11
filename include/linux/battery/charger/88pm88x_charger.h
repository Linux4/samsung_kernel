/*
 * 88pm88x_charger.h
 * 88PM88X PMIC Charger Header
 *
 * Copyright (C) 2015 Marvell Technology Ltd.
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

#ifndef __88PM88X_CHARGER_H
#define __88PM88X_CHARGER_H __FILE__
#include <linux/mfd/88pm88x.h>

#define CHG_RESTART_DELAY		(5) /* in minutes */

#define PM88X_ID_REG_CHG		(0x00)

#define PM88X_CC_CONFIG1		(0x01)
#define PM88X_CC_CONFIG2		(0x02)

#define PM88X_IBAT_OFFSET_VAL	(0x0C)
#define PM88X_SD_CONFIG1		(0x0D)
#define PM88X_SD_CONFIG2		(0x0E)

#define PM88X_IBAT_EOC_CONFIG1	(0x0F)

#define PM88X_CHG_CONFIG1		(0x28)
#define PM88X_USB_OTG			(1 << 7)
#define PM88X_USB_SUSP			(1 << 6)
#define PM88X_BATTEMP_MON2_DIS	(1 << 5)
#define PM88X_BATTEMP_MON_EN	(1 << 4)
#define PM88X_CHG_TERM_DIS		(1 << 3)
#define PM88X_CHG_WDT_EN		(1 << 1)
#define PM88X_CHG_ENABLE		(1 << 0)

#define PM88X_CHG_CONFIG3		(0x2A)
#define PM88X_OV_VBAT_EN		(1 << 0)

#define PM88X_CHG_CONFIG4		(0x2B)
#define PM88X_VBUS_SW_EN		(1 << 0)

#define PM88X_PRE_CONFIG1		(0x2D)
#define PM88X_ICHG_PRE_SET_OFFSET	(0)
#define PM88X_ICHG_PRE_SET_MASK		(0X7 << PM88X_ICHG_PRE_SET_OFFSET)
#define PM88X_VBAT_PRE_TERM_SET_OFFSET	(4)
#define PM88X_VBAT_PRE_TERM_SET_MASK	(0x7 << PM88X_VBAT_PRE_TERM_SET_OFFSET)

#define PM88X_FAST_CONFIG1		(0x2E)
#define PM88X_VBAT_FAST_SET_MASK	(0x7F << 0)

#define PM88X_FAST_CONFIG2		(0x2F)
#define PM88X_ICHG_FAST_SET_MASK	(0x3F << 0)

#define PM88X_FAST_CONFIG3		(0x30)
#define PM88X_IBAT_EOC_TH		(0x3F << 0)

#define PM88X_TIMER_CONFIG		(0x31)
#define PM88X_FASTCHG_TOUT_OFFSET	(0)
#define PM88X_RECHG_THR_SET_50MV	(0x0 << 4)
#define PM88X_CHG_ILIM_EXTD_1_3		(0x40)
#define PM88X_CHG_ILIM_EXTD_1_235	(0x80)
#define PM88X_CHG_ILIM_EXTD_1_5		(0xC0)
#define PM88X_CHG_ILIM_EXTD_CLR_MASK (0x3F)

/* IR drop compensation for PM880 and it's subsequent versions */
#define PM88X_CHG_CONFIG5			(0x32)
#define PM88X_IR_COMP_EN			(1 << 5)
#define PM88X_IR_COMP_UPD_OFFSET	(0)
#define PM88X_IR_COMP_UPD_MSK		(0x1f << PM88X_IR_COMP_UPD_OFFSET)
#define PM88X_IR_COMP_UPD_SET(x)	((x) << PM88X_IR_COMP_UPD_OFFSET)
#define PM88X_IR_COMP_RES_SET		(0x33)

#define PM88X_EXT_ILIM_CONFIG		(0x34)
#define PM88X_CHG_ILIM_RAW_MASK		(0xF << 0)
#define PM88X_CHG_ILIM_FINE_OFFSET	(4)
#define PM88X_CHG_ILIM_FINE_10		(0x40)
#define PM88X_CHG_ILIM_FINE_11_25	(0x30)
#define PM88X_CHG_ILIM_FINE_12_5	(0x20)
#define PM88X_CHG_ILIM_FINE_13_75	(0x10)
#define PM88X_CHG_ILIM_FINE_15		(0x00)

#define PM88X_CHG_ILIM_RAW_100		(0x00)
#define PM88X_CHG_ILIM_RAW_200		(0x01)
#define PM88X_CHG_ILIM_RAW_300		(0x02)
#define PM88X_CHG_ILIM_RAW_400		(0x03)
#define PM88X_CHG_ILIM_RAW_500		(0x04)
#define PM88X_CHG_ILIM_RAW_600		(0x05)
#define PM88X_CHG_ILIM_RAW_700		(0x06)
#define PM88X_CHG_ILIM_RAW_800		(0x07)
#define PM88X_CHG_ILIM_RAW_900		(0x08)
#define PM88X_CHG_ILIM_RAW_1000		(0x09)
#define PM88X_CHG_ILIM_RAW_1100		(0x0a)
#define PM88X_CHG_ILIM_RAW_1200		(0x0b)
#define PM88X_CHG_ILIM_RAW_1300		(0x0c)
#define PM88X_CHG_ILIM_RAW_1400		(0x0d)
#define PM88X_CHG_ILIM_RAW_1500		(0x0e)
#define PM88X_CHG_ILIM_RAW_1600		(0x0f)

#define PM88X_EXT_THRM1			(0x35)
#define PM88X_EXT_THRM2			(0x36)

#define PM88X_MPPT_CONFIG1		(0x3E)

#define PM88X_MPPT_CONFIG2		(0x3F)
#define MPPT_EN					(1 << 7)

#define PM88X_MPPT_CONFIG3		(0x40)
#define PM88X_WA_TH_SET_MASK		(0x3 << 0)

#define PM88X_CHG_STATUS1		(0x42)

#define PM88X_CHG_STATUS2		(0x43)
#define PM88X_VBUS_SW_OV		(1 << 0)
#define PM88X_VBUS_SW_ON		(1 << 1)
#define PM88X_USB_LIM_OK		(1 << 3)
#define PM88X_CHG_HIZ			(0x00)
#define PM88X_CHG_TRICLE		(0x40)
#define PM88X_CHG_PRECHG		(0x50)
#define PM88X_CHG_FASTCHG		(0x60)

#define PM88X_CHG_STATUS3		(0x44)
#define PM88X_CHG_LIM			(0xF << 0)

#define PM88X_CHG_LOG1			(0x45)
#define PM88X_BATT_REMOVAL		(1 << 0)
#define PM88X_CHG_REMOVAL		(1 << 1)
#define PM88X_BATT_TEMP_NOK		(1 << 2)
#define PM88X_CHG_WDT_EXPIRED	(1 << 3)
#define PM88X_OV_VBAT			(1 << 5)
#define PM88X_CHG_TIMEOUT		(1 << 6)
#define PM88X_OV_ITEMP			(1 << 7)

#define PM88X_OTG_LOG1			(0x47)
#define PM88X_OTG_UVVBAT		(1 << 0)
#define PM88X_OTG_SHORT_DET		(1 << 1)

#define PM88X_BOOST_CONFIG1		(0x6B)
#define PM88X_BOOST_CONFIG5		(0x6F)

/* Registers from GPADC page (0x32) */
#define PM88X_VBUS_MIN			(0x1A)
#define PM88X_VBUS_MAX			(0x2A)

/* Registers from BASE page (0x30) */
#define PM88X_IRQ_2			(0x06)

struct pm88x_charger_info {
	struct device *dev;
	struct power_supply	psy_chg;
	struct power_supply	psy_otg;
	struct delayed_work isr_work;

	struct mutex lock;

	unsigned int ir_comp_res;	/* IR compensation resistor */
	unsigned int ir_comp_update;	/* IR compensation update time */

	bool		 is_charging;
	unsigned int battery_present;
	unsigned int cable_type;
	unsigned int charging_current;
	unsigned int input_current_limit;
	bool		 is_fullcharged;
	int			 status;
	int			 siop_level;

	unsigned int prechg_cur;	/* precharge current limit */
	unsigned int prechg_vol;	/* precharge voltage limit */

	unsigned int fastchg_cur;	/* fastcharge current */

	int irq_nums;
	int irq[7];

	struct pm88x_chip *chip;

	sec_battery_platform_data_t	*pdata;
};
#define pm88x_charger_info_t \
	struct pm88x_charger_info

int sec_chg_get_property(struct power_supply *,
			      enum power_supply_property,
			      union power_supply_propval *);

int sec_chg_set_property(struct power_supply *,
			      enum power_supply_property,
			      const union power_supply_propval *);

#endif /* __88PM88X_CHARGER_H */
