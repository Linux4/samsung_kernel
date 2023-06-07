/*
 * s2mcs01-charger.h - Header of S2MCS01 sub charger IC
 *
 * Copyright (C) 2016 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __S2MCS01_CHARGER_H__
#define __S2MCS01_CHARGER_H__

#include "../sec_charging_common.h"

#define MASK(width, shift)      (((0x1 << (width)) - 1) << shift) 

/*---------------------------------------- */
/* Registers */
/*---------------------------------------- */
#define S2MCS01_CHG_INT1			0x1
#define S2MCS01_CHG_INT2			0x2
#define S2MCS01_CHG_INT1_MASK		0x3
#define S2MCS01_CHG_INT2_MASK		0x4
#define S2MCS01_CHG_STATUS0			0x5
#define S2MCS01_CHG_STATUS1			0x6
#define S2MCS01_CHG_SC_CTRL0			0x0A
#define S2MCS01_CHG_SC_CTRL1			0x0B
#define S2MCS01_CHG_SC_CTRL2			0x4A
#define S2MCS01_CHG_SC_CTRL3			0x52
#define S2MCS01_CHG_SC_CTRL4			0x54
#define S2MCS01_CHG_SC_CTRL5			0x55
#define S2MCS01_CHG_SC_CTRL6			0x56
#define S2MCS01_CHG_SC_CTRL7			0x57
#define S2MCS01_CHG_SC_CTRL8			0x58
#define S2MCS01_CHG_SC_CTRL9			0x59
#define S2MCS01_CHG_SC_CTRL10			0x5A
#define S2MCS01_CHG_SC_CTRL11			0x5B
#define S2MCS01_CHG_SC_CTRL12			0x5D
#define S2MCS01_PMIC_ID				0x5E
#define S2MCS01_PMIC_REV			0x5F

/* S2MCS01_CHG_INT1_MASK */
#define BATCVD_INT_M_SHIFT	4
#define BATCVD_INT_M_MASK	BIT(BATCVD_INT_M_SHIFT)

/* S2MCS01_CHG_INT2_MASK */
#define BUCK_ILIM_INT_M_SHIFT	5
#define BUCK_ILIM_INT_M_MASK	BIT(BUCK_ILIM_INT_M_SHIFT)

/* S2MCS01_CHG_STATUS0			0x5 */
#define CHGIN_OK_SHIFT 7
#define CHGIN_OK_STATUS_MASK	BIT(CHGIN_OK_SHIFT)
#define CHGINOV_OK_SHIFT 6
#define CHGINOV_OK_STATUS_MASK	BIT(CHGINOV_OK_SHIFT)
#define CHGINUV_OK_SHIFT 5
#define CHGINUV_OK_STATUS_MASK	BIT(CHGINUV_OK_SHIFT)
#define BATCVD_SHIFT 4
#define BATCVD_STATUS_MASK BIT(BATCVD_SHIFT)
#define BATUV_SHIFT 3
#define BATUV_STATUS_MASK BIT(BATUV_SHIFT)
#define TSD_SHIFT 2
#define TSD_STATUS_MASK BIT(TSD_SHIFT)
#define CIN2BAT_SHIFT 1
#define CIN2BAT_STATUS_MASK BIT(CIN2BAT_SHIFT)

/* S2MCS01_CHG_STATUS1			0x6 */
#define ENSW_SHIFT 6
#define ENSW_STATUS_MASK	BIT(ENSW_SHIFT)
#define BUCK_ILIM_SHIFT 5
#define BUCK_ILIM_STATUS_MASK	BIT(BUCK_ILIM_SHIFT)
#define BATOVP_SHIFT 4
#define BATOVP_STATUS_MASK	BIT(BATOVP_SHIFT)
#define BATCV_SHIFT 3
#define BATCV_STATUS_MASK	BIT(BATCV_SHIFT)
#define ENI2C_EN_SHIFT 1
#define ENI2C_EN_STATUS_MASK	BIT(ENI2C_EN_SHIFT)
#define SS_DOWN_SHIFT 0
#define SS_DOWN_STATUS_MASK	BIT(SS_DOWN_SHIFT)

/* S2MCS01_CHG_SC_CTRL0			0x0A */
#define EN_TMR_SHIFT			3
#define EN_TMR_MASK				BIT(EN_TMR_SHIFT)
#define SET_FCHG_TIME_SHIFT		0
#define SET_FCHG_TIME_WIDTH		3
#define SET_FCHG_TIME_MASK		MASK(SET_FCHG_TIME_WIDTH, SET_FCHG_TIME_SHIFT)

/* S2MCS01_CHG_SC_CTRL1			0x0B */
#define WDT_CLR_SHIFT			4
#define WDT_CLR_WIDTH			2
#define WDT_CLR_MASK			MASK(WDT_CLR_WIDTH, WDT_CLR_SHIFT)
#define EN_WDT_SHIFT			3
#define EN_WDT_MASK				BIT(EN_WDT_SHIFT)
#define SET_WDT_SHIFT			0
#define SET_WDT_WIDTH			3
#define SET_WDT_MASK			MASK(SET_WDT_WIDTH, SET_WDT_SHIFT)

/* S2MCS01_CHG_CHG_SC_CTRL2			0x4A */
#define EN_TSD_SHIFT			3
#define EN_TSD_MASK				BIT(EN_TSD_SHIFT)

/* S2MCS01_CHG_SC_CTRL3			0x53 */
#define CHG_CURRENT_SHIFT		7
#define CHG_CURRENT_MASK		BIT(CHG_CURRENT_SHIFT)

/* S2MCS01_CHG_SC_CTRL4			0x54 */
#define SS_FOLLOW_TIME_SHIFT		4
#define SS_FOLLOW_TIME_WIDTH		2
#define SS_FOLLOW_TIME_MASK			MASK(SS_FOLLOW_TIME_WIDTH, SS_FOLLOW_TIME_SHIFT)
#define SS_DOWN_TIME_SHIFT		1
#define SS_DOWN_TIME_WIDTH		3
#define SS_DOWN_TIME_MASK			MASK(SS_DOWN_TIME_WIDTH, SS_DOWN_TIME_SHIFT)

/* S2MCS01_CHG_SC_CTRL5			0x55 */
#define ENI2C_SHIFT			7
#define ENI2C_MASK				BIT(ENI2C_SHIFT)
#define SS_DOWN_STEP_SHIFT		1
#define SS_DOWN_STEP_WIDTH		3
#define SS_DOWN_STEP_MASK		MASK(SS_DOWN_STEP_WIDTH, SS_DOWN_STEP_SHIFT)

/* S2MCS01_CHG_SC_CTRL6			0x56 */
#define SET_BAT_UVLO_SHIFT		7
#define SET_BAT_UVLO_WIDTH		4
#define SET_BAT_UVLO_MASK		MASK(SET_BAT_UVLO_WIDTH, SET_BAT_UVLO_SHIFT)

/* S2MCS01_CHG_SC_CTRL7		0x57 */
#define OVP_SHIFT		7
#define OVP_WIDTH		4
#define OVP_MASK		MASK(OVP_WIDTH, OVP_SHIFT)

/* S2MCS01_CHG_SC_CTRL8			0x58 */
#define SOFT_DN_DISABLE_SHIFT			0
#define SOFT_DN_DISABLE_MASK			BIT(SOFT_DN_DISABLE_SHIFT)

/* S2MCS01_CHG_SC_CTRL9			0x59 */
#define CV_FLG_SHIFT		0
#define CV_FLG_WIDTH		6
#define CV_FLG_MASK		MASK(CV_FLG_WIDTH, CV_FLG_SHIFT)

/* S2MCS01_CHG_SC_CTRL10			0x5A */
#define CIN2BAT_OFF_SHIFT		7
#define CIN2BAT_OFF_MASK		BIT(CIN2BAT_OFF_SHIFT)
#define BYP_CP_EN_SHIFT			5
#define BYP_CP_EN_MASK			BIT(BYP_CP_EN_SHIFT)
#define PN_OFF_SHIFT			1
#define PN_OFF_MASK				BIT(PN_OFF_SHIFT)

/* S2MCS01_CHG_SC_CTRL11			0x5B */
#define BYP_SW_OFF_SHIFT		2
#define BYP_SW_OFF_MASK			BIT(BYP_SW_OFF_SHIFT)
#define BYP_STBY_SHIFT			1
#define BYP_STBY_MASK			BIT(BYP_STBY_SHIFT)
#define BYP_EN_SHIFT			0
#define BYP_EN_MASK				BIT(BYP_EN_SHIFT)

/*---------------------------------------- */
/* Bit Fields */
/*---------------------------------------- */

struct s2mcs01_charger_data {
	struct device           *dev;
	struct i2c_client       *i2c;
	struct mutex            io_lock;

	sec_charger_platform_data_t *pdata;

	struct power_supply	psy_chg;
	struct workqueue_struct *wqueue;
	struct delayed_work	isr_work;

	struct pinctrl *i2c_pinctrl;
	struct pinctrl_state *i2c_gpio_state_active;
	struct pinctrl_state *i2c_gpio_state_suspend;

	unsigned int siop_level;
	unsigned int chg_irq;
	unsigned int is_charging;
	unsigned int charging_type;
	unsigned int cable_type;
	unsigned int charging_current_max;
	unsigned int charging_current;

	u8 addr;
	int size;
};
#endif	/* __S2MCS01_CHARGER_H__ */
