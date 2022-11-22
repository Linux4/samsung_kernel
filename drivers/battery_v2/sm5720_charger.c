/*
 *  sm5720_charger.c
 *  Samsung SM5720 Charger Driver
 *
 *  Copyright (C) 2016 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/power_supply.h>
#include "include/charger/sm5720_charger.h"

enum {
	SS_LIM_VBUS_10mA_per_ms = 0x0,
	SS_LIM_VBUS_40mA_per_ms = 0x1,
};

enum {
	SS_LIM_WPC_5mA_per_ms   = 0x0,
	SS_LIM_WPC_10mA_per_ms  = 0x1,
};

enum {
	PRECHG_CURRENT_450mA    = 0x0,
	PRECHG_CURRENT_650mA    = 0x1,
};

enum {
	VSYS_MIN_REG_V_3000mV   = 0x0,
	VSYS_MIN_REG_V_3100mV   = 0x1,
	VSYS_MIN_REG_V_3200mV   = 0x2,
	VSYS_MIN_REG_V_3300mV   = 0x3,
	VSYS_MIN_REG_V_3400mV   = 0x4,
	VSYS_MIN_REG_V_3500mV   = 0x5,
	VSYS_MIN_REG_V_3600mV   = 0x7,
};

enum {
	RECHARGE_THRES_D_50mV   = 0x0,
	RECHARGE_THRES_D_100mV  = 0x1,
	RECHARGE_THRES_D_150mV  = 0x2,
	RECHARGE_THRES_D_200mV  = 0x3,
};

enum {
	AICL_THRES_VOLTAGE_4_3V = 0x0,
	AICL_THRES_VOLTAGE_4_4V = 0x1,
	AICL_THRES_VOLTAGE_4_5V = 0x2,
	AICL_THRES_VOLTAGE_4_6V = 0x3,
};

enum {
	TOPOFF_TIMER_10m        = 0x0,
	TOPOFF_TIMER_20m        = 0x1,
	TOPOFF_TIMER_30m        = 0x2,
	TOPOFF_TIMER_45m        = 0x3,
};

enum {
	OTGCURRENT_500mA	= 0x0,
	OTGCURRENT_9000mA	= 0x1,
	OTGCURRENT_12000mA	= 0x2,
	OTGCURRENT_1500mA	= 0x3,
};

enum {
	BST_IQ3LIMIT_2000mA     = 0x0,
	BST_IQ3LIMIT_2800mA     = 0x1,
	BST_IQ3LIMIT_3500mA     = 0x2,
	BST_IQ3LIMIT_4000mA     = 0x3,
};

enum {
	SBPS_THEM_HOT_45C       = 0x0,
	SBPS_THEM_HOT_50C       = 0x1,
	SBPS_THEM_HOT_55C       = 0x2,
	SBPS_THEM_HOT_60C       = 0x3,
};

enum {
	SBPS_THEM_COLD_5C       = 0x0,
	SBPS_THEM_COLD_0C       = 0x1,
};

enum {
	SBPS_DISCHARGE_I_50mA   = 0x0,
	SBPS_DISCHARGE_I_100mA  = 0x1,
	SBPS_DISCHARGE_I_150mA  = 0x2,
	SBPS_DISCHARGE_I_200mA  = 0x3,
};

enum {
	SBPS_EN_THRES_V_4_2V    = 0x0,
	SBPS_EN_THRES_V_4_25V   = 0x1,
};

enum {
	WATCHDOG_TIMER_30s      = 0x0,
	WATCHDOG_TIMER_60s      = 0x1,
	WATCHDOG_TIMER_90s      = 0x2,
	WATCHDOG_TIMER_120s     = 0x3,
};

static inline int __is_cable_type_used_wpc_in_path(int cable_type)
{
	if (cable_type == POWER_SUPPLY_TYPE_WIRELESS        ||  \
			cable_type == POWER_SUPPLY_TYPE_HV_WIRELESS     ||  \
			cable_type == POWER_SUPPLY_TYPE_PMA_WIRELESS    ||  \
			cable_type == POWER_SUPPLY_TYPE_WIRELESS_PACK   ||  \
			cable_type == POWER_SUPPLY_TYPE_WIRELESS_PACK_TA || \
			cable_type == POWER_SUPPLY_TYPE_WIRELESS_STAND || \
			cable_type == POWER_SUPPLY_TYPE_WIRELESS_HV_STAND || \
			cable_type == POWER_SUPPLY_TYPE_HV_WIRELESS_ETX) {
		return true;
	} else {
		return false;
	}
}

/**
 * SM5720 Charger Register control functions.
 */

static inline u8 _calc_limit_current_offset_to_mA(unsigned short mA)
{
	unsigned char offset;

	if (mA < 100) {
		offset = 0x00;
	} else {
		offset = ((mA - 100) / 25) & 0x7F;
	}

	return offset;
}

static inline unsigned short _calc_limit_current_mA_to_offset(u8 offset)
{
	return (offset * 25) + 100;
}

static inline u8 _calc_chg_current_offset_to_mA(unsigned short mA)
{
	unsigned char offset;

	if (mA < 100) {
		offset = 0x00;
	} else {
		offset = ((mA - 100) / 50) & 0x3F;
	}

	return offset;
}

static inline unsigned short _calc_chg_current_mA_to_offset(u8 offset)
{
	return (offset * 50) + 100;
}

static inline u8 _calc_bat_float_voltage_offset_to_mV(unsigned short mV)
{
	unsigned char offset;

	if (mV < 40000) {
		offset = 0x0;     /* BATREG = 3.8V */
	} else if (mV < 40100) {
		offset = 0x1;     /* BATREG = 4.0V */
	} else if (mV < 46300) {
		offset = (((mV - 40100) / 100) + 2);    /* BATREG = 4.01 ~ 4.62 in 0.01V steps */
	} else {
		pr_err("sm5720-charger: %s: can't support BATREG at over voltage 4.62V (mV=%d)\n", __func__, mV);
		offset = 0x15;    /* default Offset : 4.2V */
	}

	return offset;
}

static inline unsigned char _calc_topoff_current_offset_to_mA(unsigned short mA)
{
	unsigned char offset;

	if (mA < 100) {
		offset = 0x0;               /* Topoff = 100mA */
	} else if (mA < 600) {
		offset = (mA - 100) / 25;   /* Topoff = 125mA ~ 575mA in 25mA steps */
	} else {
		offset = 0x1F;              /* Topoff = 600mA */
	}

	return offset;
}

static inline unsigned short _calc_topoff_mA_to_offset(unsigned char offset)
{
    unsigned short mA;

    if (offset < 0x15) {
        mA = (offset * 25) + 100;
    } else {
        mA = 600;
    }

    return mA;
}

static inline unsigned short _calc_bat_float_voltage_mV_to_offset(u8 offset)
{
	return ((offset - 2) * 10) + 4010;
}

static int sm5720_CHG_set_ENQ4FET(struct sm5720_charger_data *charger, bool enable)
{
	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_CNTL1, ((enable & 0x1) << 3), (0x1 << 3));

	dev_dbg(charger->dev, "%s: Q4FET = %s\n", __func__, enable ? "enable" : "disable");

	return 0;
}

static int sm5720_CHG_set_AICLEN_WPC(struct sm5720_charger_data *charger, bool enable)
{
	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_CNTL1, ((enable & 0x1) << 5), (0x1 << 5));

	return 0;
}

static int sm5720_CHG_set_AICLEN_VBUS(struct sm5720_charger_data *charger, bool enable)
{
	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_CNTL1, ((enable & 0x1) << 6), (0x1 << 6));

	return 0;
}

#if 0
static int sm5720_CHG_set_SS_LIM_VBUS(struct sm5720_charger_data *charger, bool val)
{
	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_VBUSCNTL, ((val & 0x1) << 7), (0x1 << 7));

	return 0;
}
#endif

static int sm5720_CHG_set_VBUSLIMIT(struct sm5720_charger_data *charger, unsigned short limit_mA)
{
	u8 offset = _calc_limit_current_offset_to_mA(limit_mA);

	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_VBUSCNTL, ((offset & 0x7F) << 0), (0x7F << 0));

	return 0;
}

static unsigned short sm5720_CHG_get_VBUSLIMIT(struct sm5720_charger_data *charger)
{
	u8 offset = 0x0;

	sm5720_read_reg(charger->i2c, SM5720_CHG_REG_VBUSCNTL, &offset);

	return _calc_limit_current_mA_to_offset(offset & 0x7F);
}

#if 0
static int sm5720_CHG_set_SS_LIM_WPC(struct sm5720_charger_data *charger, bool val)
{
	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_WPCINCNTL, ((val & 0x1) << 7), (0x1 << 7));

	return 0;
}
#endif

static int sm5720_CHG_set_WPCINLIMIT(struct sm5720_charger_data *charger, unsigned short limit_mA)
{
	u8 offset = _calc_limit_current_offset_to_mA(limit_mA);

	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_WPCINCNTL, ((offset & 0x7F) << 0), (0x7F << 0));

	return 0;
}

static unsigned short sm5720_CHG_get_WPCINLIMIT(struct sm5720_charger_data *charger)
{
	u8 offset = 0x0;

	sm5720_read_reg(charger->i2c, SM5720_CHG_REG_WPCINCNTL, &offset);

	return _calc_limit_current_mA_to_offset(offset & 0x7F);
}

#if 0
static int sm5720_CHG_set_PRECHG(struct sm5720_charger_data *charger, bool enable, bool val)
{
	u8 offset = (enable << 1) | val;

	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_CHGCNTL1, ((offset & 0x3) << 0), (0x3 << 0));

	return 0;
}

static int sm5720_CHG_set_VSYS_MIN(struct sm5720_charger_data *charger, u8 val)
{
	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_CHGCNTL1, ((val & 0x7) << 2), (0x7 << 2));

	return 0;
}
#endif

static int sm5720_CHG_set_VBUS_FASTCHG(struct sm5720_charger_data *charger, unsigned short chg_mA)
{
	u8 offset = _calc_chg_current_offset_to_mA(chg_mA);

	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_CHGCNTL2, ((offset & 0x3F) << 0), (0x3F << 0));

	return 0;
}

#if 0
static unsigned short sm5720_CHG_get_VBUS_FASTCHG(struct sm5720_charger_data *charger)
{
	u8 offset = 0x0;

	sm5720_read_reg(charger->i2c, SM5720_CHG_REG_CHGCNTL2, &offset);

	return _calc_chg_current_mA_to_offset(offset & 0x3F);
}
#endif

static int sm5720_CHG_set_WPC_FASTCHG(struct sm5720_charger_data *charger, unsigned short chg_mA)
{
	u8 offset = _calc_chg_current_offset_to_mA(chg_mA);

	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_CHGCNTL3, ((offset & 0x3F) << 0), (0x3F << 0));

	return 0;
}

#if 0
static unsigned short sm5720_CHG_get_WPC_FASTCHG(struct sm5720_charger_data *charger)
{
	u8 offset = 0x0;

	sm5720_read_reg(charger->i2c, SM5720_CHG_REG_CHGCNTL3, &offset);

	return _calc_chg_current_mA_to_offset(offset & 0x3F);
}
#endif

static int sm5720_CHG_set_AUTOSTOP(struct sm5720_charger_data *charger, bool enable)
{
	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_CHGCNTL4, ((enable & 0x1) << 6), (0x1 << 6));

	return 0;
}

static int sm5720_CHG_set_BATREG(struct sm5720_charger_data *charger, unsigned short float_mV)
{
	u8 offset = _calc_bat_float_voltage_offset_to_mV(float_mV);

	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_CHGCNTL4, ((offset & 0x3F) << 0), (0x3F << 0));

	return 0;
}

static unsigned short sm5720_CHG_get_BATREG(struct sm5720_charger_data *charger)
{
	u8 offset = 0x0;

	sm5720_read_reg(charger->i2c, SM5720_CHG_REG_CHGCNTL4, &offset);

	return _calc_bat_float_voltage_mV_to_offset(offset & 0x3F);
}

#if 0
static int sm5720_CHG_set_RECHARGE(struct sm5720_charger_data *charger, u8 val)
{
	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_CHGCNTL5, ((val & 0x3) << 5), (0x3 << 5));

	return 0;
}
#endif

static int sm5720_CHG_set_TOPOFF(struct sm5720_charger_data *charger, unsigned short topoff_mA)
{
	u8 offset = _calc_topoff_current_offset_to_mA(topoff_mA);

	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_CHGCNTL5, ((offset & 0x1F) << 0), (0x1F << 0));

	return 0;
}

static unsigned short sm5720_CHG_get_TOPOFF(struct sm5720_charger_data *charger)
{
    u8 offset = 0x0;

    sm5720_read_reg(charger->i2c, SM5720_CHG_REG_CHGCNTL5, &offset);

    return _calc_topoff_mA_to_offset(offset & 0x1F);
}

static int sm5720_CHG_set_AICLTH(struct sm5720_charger_data *charger, u8 val)
{
	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_CHGCNTL6, ((val & 0x3) << 6), (0x3 << 6));

	return 0;
}

static int sm5720_CHG_set_TOPOFFTIMER(struct sm5720_charger_data *charger, u8 val)
{
	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_CHGCNTL7, ((val & 0x3) << 3), (0x3 << 3));

	return 0;
}

#if 0
static int sm5720_CHG_set_ENWATCHDOG(struct sm5720_charger_data *charger, bool enable)
{
	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_WDTCNTL, ((enable & 0x1) << 0), (0x1 << 0));

	return 0;
}

static int sm5720_CHG_set_WATCHDOG_TMR(struct sm5720_charger_data *charger, u8 val)
{
	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_WDTCNTL, ((val & 0x3) << 1), (0x3 << 1));

	return 0;
}
#endif

static int sm5720_CHG_set_WDTMR_RST(struct sm5720_charger_data *charger)
{
	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_WDTCNTL, (0x1 << 3), (0x1 << 3));

	return 0;
}

#if 0
static int sm5720_CHG_set_ENCHGREON_WDT(struct sm5720_charger_data *charger, bool enable)
{
	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_WDTCNTL, ((enable & 0x1) << 4), (0x1 << 4));

	return 0;
}
#endif
static int sm5720_CHG_set_OTGCURRENT(struct sm5720_charger_data *charger, u8 val)
{
	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_BSTCNTL1, ((val & 0x3) << 6), (0x3 << 6));

	return 0;
}

static int sm5720_CHG_set_BST_IQ3LIMIT(struct sm5720_charger_data *charger, u8 val)
{
	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_BSTCNTL1, ((val & 0x3) << 4), (0x3 << 4));

	return 0;
}

#if 0
static int sm5720_CHG_set_ENSBPS(struct sm5720_charger_data *charger, bool enable)
{
	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_SBPSCNTL, ((enable & 0x1) << 0), (0x1 << 0));

	return 0;
}

static int sm5720_CHG_set_THEM_H(struct sm5720_charger_data *charger, u8 val)
{
	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_BSTCNTL1, ((val & 0x3) << 1), (0x3 << 1));

	return 0;
}

static int sm5720_CHG_set_THEM_C(struct sm5720_charger_data *charger, u8 val)
{
	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_BSTCNTL1, ((val & 0x1) << 3), (0x1 << 3));

	return 0;
}

static int sm5720_CHG_set_I_SBPS(struct sm5720_charger_data *charger, u8 val)
{
	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_BSTCNTL1, ((val & 0x3) << 4), (0x3 << 4));

	return 0;
}

static int sm5720_CHG_set_VBAT_SBPS(struct sm5720_charger_data *charger, u8 val)
{
	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_BSTCNTL1, ((val & 0x1) << 6), (0x1 << 6));

	return 0;
}

static int sm5720_CHG_set_ENSBPS_FORCE(struct sm5720_charger_data *charger, bool enable)
{
	sm5720_update_reg(charger->i2c, SM5720_CHG_REG_SBPSCNTL, ((enable & 0x1) << 7), (0x1 << 7));

	return 0;
}
#endif

static void sm5720_CHG_print_reg(struct sm5720_charger_data *charger)
{
	u8 regs[SM5720_CHG_REG_END] = {0x0, };
	int i;

	sm5720_bulk_read(charger->i2c, SM5720_CHG_REG_STATUS1, (SM5720_CHG_REG_RGBWCNTL1 - SM5720_CHG_REG_STATUS1), regs);

	pr_info("sm5720-charger: ");

	for (i = 0; i < (SM5720_CHG_REG_RGBWCNTL1 - SM5720_CHG_REG_STATUS1); ++i) {
		pr_info("0x%x:0x%x ", SM5720_CHG_REG_STATUS1 + i, regs[i]);
	}

	pr_info("\n");
}

/**
 * Power Supply - SM5720 Charger operation functions.
 */

static enum power_supply_property sm5720_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_FULL,
#if defined(CONFIG_BATTERY_SWELLING)
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
#endif
	POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
	POWER_SUPPLY_PROP_USB_HC,
	POWER_SUPPLY_PROP_ENERGY_NOW,
#if defined(CONFIG_AFC_CHARGER_MODE)
	POWER_SUPPLY_PROP_AFC_CHARGER_MODE,
#endif
};

static void sm5720_chg_set_input_limit(struct sm5720_charger_data *charger, int input_limit)
{
	if (__is_cable_type_used_wpc_in_path(charger->cable_type)) {
		sm5720_CHG_set_WPCINLIMIT(charger, input_limit);
	} else {
		sm5720_CHG_set_VBUSLIMIT(charger, input_limit);
	}

	dev_info(charger->dev, "%s: input_limit=%d\n", __func__, input_limit);
}

static void sm5720_chg_set_charging_current(struct sm5720_charger_data *charger, int charging_current)
{
	if (__is_cable_type_used_wpc_in_path(charger->cable_type)) {
		sm5720_CHG_set_WPC_FASTCHG(charger, charging_current);
	} else {
		sm5720_CHG_set_VBUS_FASTCHG(charger, charging_current);
	}

	dev_info(charger->dev, "%s: charging_current=%d\n", __func__, charging_current);
}

static int psy_chg_set_cable_online(struct sm5720_charger_data *charger, int cable_type)
{
	int prev_cable_type = charger->cable_type;

	charger->slow_late_chg_mode = false;
	charger->cable_type = cable_type;

	charger->input_current =
		charger->charging_current_t[charger->cable_type].input_current_limit;
	charger->charging_current =
		charger->charging_current_t[charger->cable_type].fast_charging_current;
	dev_info(charger->dev, "[start] prev_cable_type(%d), cable_type(%d), op_mode(%d), op_status(0x%x)",
			prev_cable_type, cable_type, sm5720_charger_oper_get_current_op_mode(), sm5720_charger_oper_get_current_status());

	if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY) {
		/* set default value */
		sm5720_charger_oper_push_event(SM5720_CHARGER_OP_EVENT_VBUSIN, 0);
		sm5720_charger_oper_push_event(SM5720_CHARGER_OP_EVENT_WPCIN, 0);
		sm5720_charger_oper_push_event(SM5720_CHARGER_OP_EVENT_PWR_SHAR, 0);

		/* set default input current */
		sm5720_CHG_set_VBUSLIMIT(charger, 500);
		sm5720_CHG_set_WPCINLIMIT(charger, 400);
	} else {
		if (__is_cable_type_used_wpc_in_path(charger->cable_type)) {
			sm5720_charger_oper_push_event(SM5720_CHARGER_OP_EVENT_VBUSIN, 0);
			sm5720_charger_oper_push_event(SM5720_CHARGER_OP_EVENT_WPCIN, 1);
		} else {
			sm5720_charger_oper_push_event(SM5720_CHARGER_OP_EVENT_WPCIN, 0);
			sm5720_charger_oper_push_event(SM5720_CHARGER_OP_EVENT_VBUSIN, 1);
		}
	}
	dev_info(charger->dev, "[end] is_charging=%d, fc = %d, il = %d, t1 = %d, t2 = %d, cable = %d,"
			"op_mode(%d), op_status(0x%x)\n",
			charger->is_charging,
			charger->charging_current,
			charger->input_current,
			charger->charging_current_t[charger->cable_type].full_check_current_1st,
			charger->charging_current_t[charger->cable_type].full_check_current_2nd,
			charger->cable_type,
			sm5720_charger_oper_get_current_op_mode(),
			sm5720_charger_oper_get_current_status());

	return 0;
}

static void psy_chg_set_charging_enabled(struct sm5720_charger_data *charger, int charge_mode)
{
	int buck_off = false;

	dev_info(charger->dev, "charger_mode changed [%d] -> [%d]\n", charger->charge_mode, charge_mode);

	charger->charge_mode = charge_mode;

	switch (charger->charge_mode) {
		case SEC_BAT_CHG_MODE_BUCK_OFF:
			buck_off = true;
			break;
		case SEC_BAT_CHG_MODE_CHARGING_OFF:
			charger->is_charging = false;
			break;
		case SEC_BAT_CHG_MODE_CHARGING:
			charger->is_charging = true;
			break;
	}

	sm5720_CHG_set_ENQ4FET(charger, charger->is_charging);
	sm5720_charger_oper_push_event(SM5720_CHARGER_OP_EVENT_SUSPEND, buck_off);
}

static int psy_chg_get_charger_status(struct sm5720_charger_data *charger)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	u8 reg;

	sm5720_read_reg(charger->i2c, SM5720_CHG_REG_STATUS2, &reg);
	dev_info(charger->dev, "%s: INT_STATUS2=0x%x\n", __func__, reg);

	if (reg & (0x1 << 5)) { /* check: Top-off */
		status = POWER_SUPPLY_STATUS_FULL;
	} else if (reg & (0x1 << 3)) {  /* check: Charging ON */
		status = POWER_SUPPLY_STATUS_CHARGING;
	} else {
		sm5720_read_reg(charger->i2c, SM5720_CHG_REG_STATUS1, &reg);
		dev_info(charger->dev, "%s: INT_STATUS1=0x%x\n", __func__, reg);

		if ((reg & (0x1 << 0)) || (reg & (0x1 << 4))) { /* check: VBUS_POK, WPC_POK */
			status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		} else {
			status = POWER_SUPPLY_STATUS_DISCHARGING;
		}
	}

	return status;
}

static int psy_chg_get_online(struct sm5720_charger_data *charger)
{
	int type = POWER_SUPPLY_TYPE_BATTERY;
	u8 reg;

	sm5720_read_reg(charger->i2c, SM5720_CHG_REG_STATUS1, &reg);
	dev_info(charger->dev, "%s: INT_STATUS1=0x%x\n", __func__, reg);

	if (reg & (0x1 << 0)) { /* check: VBUS_POK */
		type = POWER_SUPPLY_TYPE_MAINS;
	} else if (reg & (0x1 << 4)) {  /* check: WPC_POK */
		type = POWER_SUPPLY_TYPE_WIRELESS;
	}

	return type;
}

static int psy_chg_get_battery_present(struct sm5720_charger_data *charger)
{
	u8 reg;

	sm5720_read_reg(charger->i2c, SM5720_CHG_REG_STATUS2, &reg);
	dev_info(charger->dev, "%s: INT_STATUS2=0x%x\n", __func__, reg);

	if (reg & (0x1 << 2)) { /* check: NOBAT */
		return 0;
	}

	return 1;
}

static int psy_chg_get_charge_type(struct sm5720_charger_data *charger)
{
	int charge_type;

	if (charger->is_charging) {
		if (charger->slow_late_chg_mode) {
			dev_info(charger->dev, "%s: slow late charge mode\n", __func__);
			charge_type = POWER_SUPPLY_CHARGE_TYPE_SLOW;
		} else {
			charge_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
		}
	} else {
		charge_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
	}

	return charge_type;
}

static int psy_chg_get_charging_health(struct sm5720_charger_data *charger)
{
	u8 reg;
	int offset, health;

	sm5720_read_reg(charger->i2c, SM5720_CHG_REG_STATUS1, &reg);
	dev_info(charger->dev, "is_charging=%d, cable_type=%d, status1_reg=0x%x\n", 
			charger->is_charging, charger->cable_type, reg);

	if (__is_cable_type_used_wpc_in_path(charger->cable_type)) {
		offset = 4;
	} else {
		offset = 0;
	}

	if (reg & (0x1 << (0 + offset))) {
		health = POWER_SUPPLY_HEALTH_GOOD;
	} else if (reg & (0x1 << (1 + offset))) {
		health = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
	} else if (reg & (0x1 << (2 + offset))) {
		health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	} else {
		health = POWER_SUPPLY_HEALTH_UNKNOWN;
	}

	/* Watchdog-Timer Kick */
	sm5720_CHG_set_WDTMR_RST(charger);

	return health;
}

static int sm5720_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct sm5720_charger_data *charger =
		container_of(psy, struct sm5720_charger_data, psy_chg);
	int status;

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			val->intval = psy_chg_get_charger_status(charger);
			break;
		case POWER_SUPPLY_PROP_CHARGING_ENABLED:
			break;
		case POWER_SUPPLY_PROP_PRESENT:
			val->intval = psy_chg_get_battery_present(charger);
			break;
		case POWER_SUPPLY_PROP_CHARGE_TYPE:
			val->intval = psy_chg_get_charge_type(charger);
			break;
		case POWER_SUPPLY_PROP_HEALTH:
			val->intval = psy_chg_get_charging_health(charger);
			sm5720_CHG_print_reg(charger);
			break;
		case POWER_SUPPLY_PROP_ONLINE:
			val->intval = psy_chg_get_online(charger);
			break;
		case POWER_SUPPLY_PROP_CURRENT_MAX:
			val->intval = charger->input_current;
			break;
		case POWER_SUPPLY_PROP_CURRENT_AVG:
			if (__is_cable_type_used_wpc_in_path(charger->cable_type)) {
				val->intval = sm5720_CHG_get_WPCINLIMIT(charger);
			} else {
				val->intval = sm5720_CHG_get_VBUSLIMIT(charger);
			}        
			break;
		case POWER_SUPPLY_PROP_CURRENT_NOW:
			val->intval = charger->charging_current;
			break;
		case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
			status = sm5720_charger_oper_get_current_status();
			val->intval = status & (0x1 << SM5720_CHARGER_OP_EVENT_USB_OTG) ? 1 : 0;
			break;
		case POWER_SUPPLY_PROP_USB_HC:
			return -ENODATA;
		case POWER_SUPPLY_PROP_VOLTAGE_MAX:
			val->intval = sm5720_CHG_get_BATREG(charger);
			break;
    case POWER_SUPPLY_PROP_CURRENT_FULL:
        val ->intval = sm5720_CHG_get_TOPOFF(charger);
        break;
		default:
			return -EINVAL;
	}

	return 0;
}

#if defined(CONFIG_AFC_CHARGER_MODE)
extern void sm5720_muic_charger_init(void);
#endif

static int sm5720_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct sm5720_charger_data *charger =
		container_of(psy, struct sm5720_charger_data, psy_chg);
	int current_max;


	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			charger->status = val->intval;
			break;
		case POWER_SUPPLY_PROP_CHARGING_ENABLED:
			dev_info(charger->dev, "CHG:POWER_SUPPLY_PROP_CHARGING_ENABLED (val=%d)\n", val->intval);
			psy_chg_set_charging_enabled(charger, val->intval);
			sm5720_CHG_print_reg(charger);
			break;
		case POWER_SUPPLY_PROP_ONLINE:
			psy_chg_set_cable_online(charger, val->intval);
			break;
		case POWER_SUPPLY_PROP_CURRENT_MAX:
			dev_info(charger->dev, "CHG:POWER_SUPPLY_PROP_CURRENT_MAX (val=%d)\n", val->intval);

			if (val->intval > charger->input_current) {
				dev_dbg(charger->dev, "pervent current_max from charger input current\n");
				current_max = charger->input_current;
			} else {
				current_max = val->intval;
			}
			sm5720_chg_set_input_limit(charger, current_max);
			break;
		case POWER_SUPPLY_PROP_CURRENT_AVG:
			dev_info(charger->dev, "CHG:POWER_SUPPLY_PROP_CURRENT_AVG (val=%d)\n", val->intval);

			charger->charging_current = val->intval;
			sm5720_chg_set_charging_current(charger, charger->charging_current);
			break;
		case POWER_SUPPLY_PROP_CURRENT_NOW:
			if (!__is_cable_type_used_wpc_in_path(charger->cable_type)) {
				charger->charging_current = val->intval;
				sm5720_chg_set_charging_current(charger, charger->charging_current);
			}
			break;
		case POWER_SUPPLY_PROP_CURRENT_FULL:
			dev_info(charger->dev, "CHG:POWER_SUPPLY_PROP_CURRENT_FULL (val=%d)\n", val->intval);
			sm5720_CHG_set_TOPOFF(charger, val->intval);
			break;
#if defined(CONFIG_BATTERY_SWELLING)
		case POWER_SUPPLY_PROP_VOLTAGE_MAX:
			dev_info(charger->dev, "CHG:POWER_SUPPLY_PROP_VOLTAGE_MAX (val=%d)\n", val->intval);

			charger->float_voltage = val->intval;
			sm5720_CHG_set_BATREG(charger, charger->float_voltage);
			break;
#endif
		case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
			dev_info(charger->dev, "CHG:POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL - %s\n", val->intval > 0 ? "ON" : "OFF");

			if (val->intval) {
				sm5720_charger_oper_push_event(SM5720_CHARGER_OP_EVENT_USB_OTG, 1);
			} else {
				sm5720_charger_oper_push_event(SM5720_CHARGER_OP_EVENT_USB_OTG, 0);
			}
			break;
		case POWER_SUPPLY_PROP_USB_HC:
			return -ENODATA;
		case POWER_SUPPLY_PROP_ENERGY_NOW:
			/* if jig attached, */
			break;
#if defined(CONFIG_AFC_CHARGER_MODE)
		case POWER_SUPPLY_PROP_AFC_CHARGER_MODE:
			sm5720_muic_charger_init();
			break;
#endif
		default:
			return -EINVAL;
	}

	return 0;
}

/**
 * Power Supply - OTG operation functions.
 */

static enum power_supply_property sm5720_otg_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static bool sm5720_charger_check_oper_otg_mode_on(void)
{
	unsigned char current_status = sm5720_charger_oper_get_current_status();
	bool ret;

	if (current_status & (1 << SM5720_CHARGER_OP_EVENT_USB_OTG)) {
		ret = true;
	} else {
		ret = false;
	}

	return ret;
}

static int sm5720_otg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	switch (psp) {
		case POWER_SUPPLY_PROP_ONLINE:
			val->intval = sm5720_charger_check_oper_otg_mode_on();
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int sm5720_otg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct sm5720_charger_data *charger =
		container_of(psy, struct sm5720_charger_data, psy_otg);

	switch (psp) {
		case POWER_SUPPLY_PROP_ONLINE:
			dev_info(charger->dev, "OTG:POWER_SUPPLY_PROP_ONLINE - %s\n", val->intval > 0 ? "ON" : "OFF");

			if (val->intval) {
				sm5720_charger_oper_push_event(SM5720_CHARGER_OP_EVENT_USB_OTG, 1);
			} else {
				sm5720_charger_oper_push_event(SM5720_CHARGER_OP_EVENT_USB_OTG, 0);
			}
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

/**
 * SM5720 Charger Platform driver operation functions.
 */

static struct device_attribute sm5720_charger_attrs[] = {
	SM5720_CHARGER_ATTR(chip_id),
	SM5720_CHARGER_ATTR(data),
};

static int sm5720_chg_create_attrs(struct device *dev)
{
	unsigned long i;
	int rc;

	for (i = 0; i < ARRAY_SIZE(sm5720_charger_attrs); i++) {
		rc = device_create_file(dev, &sm5720_charger_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	return rc;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &sm5720_charger_attrs[i]);
	return rc;
}

ssize_t sm5720_chg_show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sm5720_charger_data *charger =
		container_of(psy, struct sm5720_charger_data, psy_chg);
	const ptrdiff_t offset = attr - sm5720_charger_attrs;
	int i = 0;
	u8 addr, data;

	switch(offset) {
		case CHIP_ID:
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", "SM5720");
			break;
		case DATA:
			for (addr = SM5720_CHG_REG_STATUS1; addr <= SM5720_CHG_REG_BSTCNTL2; addr++) {
				sm5720_read_reg(charger->i2c, addr, &data);
				i += scnprintf(buf + i, PAGE_SIZE - i, "0x%02x : 0x%02x\n", addr, data);
			}
			break;
		default:
			return -EINVAL;
	}
	return i;
}

ssize_t sm5720_chg_store_attrs(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sm5720_charger_data *charger =
		container_of(psy, struct sm5720_charger_data, psy_chg);
	const ptrdiff_t offset = attr - sm5720_charger_attrs;
	int ret = 0;
	int x,y;

	switch(offset) {
		case CHIP_ID:
			ret = count;
			break;
		case DATA:
			if (sscanf(buf, "0x%x 0x%x", &x, &y) == 2) {
				if (x >= SM5720_CHG_REG_STATUS1 && x <= SM5720_CHG_REG_BSTCNTL2) {
					u8 addr = x;
					u8 data = y;
					if (sm5720_write_reg(charger->i2c, addr, data) < 0)
					{
						dev_info(charger->dev,
								"%s: addr: 0x%x write fail\n", __func__, addr);
					}
				} else {
					dev_info(charger->dev,
							"%s: addr: 0x%x is wrong\n", __func__, x);
				}
			}
			ret = count;
			break;
		default:
			ret = -EINVAL;
	}
	return ret;
}

static irqreturn_t chg_vbus_in_isr(int irq, void *data)
{
	struct sm5720_charger_data *charger = data;

	dev_dbg(charger->dev, "%s - irq=%d\n", __func__, irq);

	return IRQ_HANDLED;
}

static irqreturn_t chg_wpcin_pok_isr(int irq, void *data)
{
	struct sm5720_charger_data *charger = data;

	dev_dbg(charger->dev, "%s - irq=%d\n", __func__, irq);

	return IRQ_HANDLED;
}

static irqreturn_t chg_aicl_isr(int irq, void *data)
{
	struct sm5720_charger_data *charger = data;

	dev_dbg(charger->dev, "%s - irq=%d\n", __func__, irq);

	wake_lock(&charger->aicl_wake_lock);
	queue_delayed_work(charger->wqueue, &charger->aicl_work, 0);

	return IRQ_HANDLED;
}

static irqreturn_t chg_topoff_isr(int irq, void *data)
{
	struct sm5720_charger_data *charger = data;

	dev_dbg(charger->dev, "%s - irq=%d\n", __func__, irq);

	return IRQ_HANDLED;
}

static irqreturn_t chg_done_isr(int irq, void *data)
{
	struct sm5720_charger_data *charger = (struct sm5720_charger_data *)data;

	/* Toggle Q4FAT for Reset Topoff Timer condition */

	dev_dbg(charger->dev, "%s - irq=%d\n", __func__, irq);

	sm5720_CHG_set_ENQ4FET(charger, 0);
	msleep(10);
	sm5720_CHG_set_ENQ4FET(charger, 1);

	return IRQ_HANDLED;
}

static inline int _reduce_input_limit_current(struct sm5720_charger_data *charger)
{
	int input_limit;

	if (__is_cable_type_used_wpc_in_path(charger->cable_type)) {
		input_limit = sm5720_CHG_get_WPCINLIMIT(charger);
		sm5720_CHG_set_WPCINLIMIT(charger, input_limit - REDUCE_CURRENT_STEP);
		charger->input_current = sm5720_CHG_get_WPCINLIMIT(charger);
	} else {
		input_limit = sm5720_CHG_get_VBUSLIMIT(charger);
		sm5720_CHG_set_VBUSLIMIT(charger, input_limit - REDUCE_CURRENT_STEP);
		charger->input_current = sm5720_CHG_get_VBUSLIMIT(charger);
	}


	dev_info(charger->dev, "reduce input-limit: [%dmA] to [%dmA]\n", 
			input_limit, charger->input_current);

	return charger->input_current;
}

static inline void _check_slow_rate_charging(struct sm5720_charger_data *charger)
{
	union power_supply_propval value;
	/* under 400mA considered as slow charging concept for VZW */
	if (charger->input_current <= SLOW_CHARGING_CURRENT_STANDARD &&
			charger->cable_type != POWER_SUPPLY_TYPE_BATTERY) {

		dev_info(charger->dev, "slow-rate charging on : input current(%dmA), cable-type(%d)\n",
				charger->input_current, charger->cable_type);

		charger->slow_late_chg_mode = 1;
		value.intval = POWER_SUPPLY_CHARGE_TYPE_SLOW;
		psy_do_property("battery", set, POWER_SUPPLY_PROP_CHARGE_TYPE, value);
	}
}

static void aicl_work(struct work_struct *work)
{
	struct sm5720_charger_data *charger =
		container_of(work, struct sm5720_charger_data, aicl_work.work);
	int aicl_cnt = 0, input_limit;
	u8 reg;

	dev_dbg(charger->dev, "%s - start\n", __func__);

	mutex_lock(&charger->charger_mutex);
	sm5720_read_reg(charger->i2c, SM5720_CHG_REG_STATUS2, &reg);
	while ((reg & (0x1 << 0)) && charger->cable_type != POWER_SUPPLY_TYPE_BATTERY) {
		if (++aicl_cnt >= 2) {
			input_limit = _reduce_input_limit_current(charger);
			if (input_limit <= MINIMUM_INPUT_CURRENT) {
				break;
			}
			aicl_cnt = 0;
		}
		mdelay(50);
		sm5720_read_reg(charger->i2c, SM5720_CHG_REG_STATUS2, &reg);
	}

	if (!__is_cable_type_used_wpc_in_path(charger->cable_type)) {
		_check_slow_rate_charging(charger);
	}

	mutex_unlock(&charger->charger_mutex);
	wake_unlock(&charger->aicl_wake_lock);

	dev_dbg(charger->dev, "%s - done\n", __func__);
}

#ifdef CONFIG_OF
static int sm5720_charger_parse_dt(struct sm5720_charger_data *charger)
{
	struct device_node *np;
	int ret = 0;

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_err("%s np NULL\n", __func__);
	} else {
		int len;
		unsigned int i;
		const u32 *p;

		ret = of_property_read_u32(np, "battery,chg_float_voltage", &charger->float_voltage);
		if (ret) {
			pr_info("%s: battery,chg_float_voltage is Empty\n", __func__);
			charger->float_voltage = 4200;
		}
		pr_info("%s: battery,chg_float_voltage is %d\n", __func__, charger->float_voltage);

		p = of_get_property(np, "battery,input_current_limit", &len);
		if (!p)
			return 1;

		len = len / sizeof(u32);

		charger->charging_current_t = kzalloc(sizeof(sec_charging_current_t) * len, GFP_KERNEL);

		for(i = 0; i < len; i++) {
			ret = of_property_read_u32_index(np, "battery,input_current_limit", i, 
					&charger->charging_current_t[i].input_current_limit);
			if (ret)
				pr_info("%s : Input_current_limit is Empty\n", __func__);
		}
	}
	return ret;
}
#endif

static void sm5720_charger_initialize(struct sm5720_charger_data *charger)
{
	dev_info(charger->dev, "charger initial hardware condition process start. (float_voltage=%d)\n", 
			charger->float_voltage);

	sm5720_CHG_set_AICLTH(charger, AICL_THRES_VOLTAGE_4_5V);
	sm5720_CHG_set_AICLEN_VBUS(charger, 1);
	sm5720_CHG_set_AICLEN_WPC(charger, 1);
	sm5720_CHG_set_BATREG(charger, charger->float_voltage);

	sm5720_CHG_set_BST_IQ3LIMIT(charger, BST_IQ3LIMIT_4000mA);
	sm5720_CHG_set_OTGCURRENT(charger, OTGCURRENT_1500mA);

	/* Topoff-Timer configuration for Battery Swelling protection */
	sm5720_CHG_set_TOPOFFTIMER(charger, TOPOFF_TIMER_45m);
	sm5720_CHG_set_AUTOSTOP(charger, 1);


#if defined(SM5720_WATCHDOG_RESET_ACTIVATE)
	/* Watchdog-Timer configuration for Battery Swelling protection */
	sm5720_CHG_set_WATCHDOG_TMR(charger, WATCHDOG_TIMER_120s);
	sm5720_CHG_set_ENCHGREON_WDT(charger, 1);
	sm5720_CHG_set_ENWATCHDOG(charger, 1);
#endif

#if defined(SM5720_SBPS_ACTIVATE)
	/* SBPS configuration */
	sm5720_CHG_set_THEM_H(charger, SBPS_THEM_HOT_60C);
	sm5720_CHG_set_THEM_C(charger, SBPS_THEM_COLD_0C);
	sm5720_CHG_set_I_SBPS(charger, SBPS_DISCHARGE_I_50mA);
	sm5720_CHG_set_VBAT_SBPS(charger, SBPS_EN_THRES_V_4_2V);
	sm5720_CHG_set_ENSBPS(charger, 1);
#endif

	sm5720_CHG_print_reg(charger);

	dev_info(charger->dev, "charger initial hardware condition process done.\n");
}

static inline int _init_sm5720_charger_drv_info(struct sm5720_charger_data *charger)
{
	struct sm5720_platform_data *pdata = charger->sm5720_pdata;

	if (pdata == NULL) {
		dev_err(charger->dev, "%s: can't get sm5720_platform_data info\n", __func__);
		return -EINVAL;
	}

	mutex_init(&charger->charger_mutex);

	charger->wqueue = create_singlethread_workqueue(dev_name(charger->dev));
	if (!charger->wqueue) {
		dev_err(charger->dev, "%s: fail to create workqueue\n", __func__);
		return -ENOMEM;
	}

	charger->slow_late_chg_mode = false;

	/* setup Work-queue control handlers */
	INIT_DELAYED_WORK(&charger->aicl_work, aicl_work);

	wake_lock_init(&charger->aicl_wake_lock, WAKE_LOCK_SUSPEND, "charger-aicl");

	/* Get IRQ-service numbers */
	charger->irq_wpcin_pok = pdata->irq_base + SM5720_CHG_IRQ_INT1_WPCINPOK;
	charger->irq_vbus_pok = pdata->irq_base + SM5720_CHG_IRQ_INT1_VBUSPOK;
	charger->irq_aicl = pdata->irq_base + SM5720_CHG_IRQ_INT2_AICL;
	charger->irq_topoff = pdata->irq_base + SM5720_CHG_IRQ_INT2_TOPOFF;
	charger->irq_done = pdata->irq_base + SM5720_CHG_IRQ_INT2_DONE;

	return 0;
}

static void sm5720_check_reset_ic(struct i2c_client *i2c, void *data)
{

}

static int sm5720_charger_probe(struct platform_device *pdev)
{
	struct sm5720_dev *sm5720 = dev_get_drvdata(pdev->dev.parent);
	struct sm5720_platform_data *pdata = dev_get_platdata(sm5720->dev);
	struct sm5720_charger_data *charger;
	int ret = 0;

	dev_info(&pdev->dev, "%s: SM5720 Charger Driver Probe Start\n", __func__);

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	charger->dev = &pdev->dev;
	charger->i2c = sm5720->charger;
	charger->sm5720_pdata = pdata;

#if defined(CONFIG_OF)
	ret = sm5720_charger_parse_dt(charger);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s not found charger dt! ret[%d]\n", __func__, ret);
	}
#endif
	platform_set_drvdata(pdev, charger);

	sm5720->chg_data = (void *)charger;
	sm5720->check_chg_reset = sm5720_check_reset_ic;

	sm5720_charger_initialize(charger);
	sm5720_charger_oper_table_init(charger->i2c);

	ret = _init_sm5720_charger_drv_info(charger);
	if (ret) {
		goto err_pdata_free;
	}

	charger->psy_chg.name		    = "sm5720-charger";
	charger->psy_chg.type		    = POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property	= sm5720_chg_get_property;
	charger->psy_chg.set_property	= sm5720_chg_set_property;
	charger->psy_chg.properties	    = sm5720_charger_props;
	charger->psy_chg.num_properties	= ARRAY_SIZE(sm5720_charger_props);
	charger->psy_otg.name		    = "otg";
	charger->psy_otg.type		    = POWER_SUPPLY_TYPE_OTG;
	charger->psy_otg.get_property	= sm5720_otg_get_property;
	charger->psy_otg.set_property	= sm5720_otg_set_property;
	charger->psy_otg.properties	    = sm5720_otg_props;
	charger->psy_otg.num_properties	= ARRAY_SIZE(sm5720_otg_props);

	ret = power_supply_register(&pdev->dev, &charger->psy_chg);
	if (ret) {
		dev_err(&pdev->dev, "%s: fail to Register psy_chg\n", __func__);
		goto err_power_supply_register_chg;
	}

	ret = power_supply_register(&pdev->dev, &charger->psy_otg);
	if (ret) {
		dev_err(&pdev->dev, "%s: fail to Register psy_otg\n", __func__);
		goto err_power_supply_register_otg;
	}

	ret = sm5720_chg_create_attrs(charger->psy_chg.dev);
	if (ret) {
		dev_err(&pdev->dev, "%s : fail to create_attrs\n", __func__);
		goto err_irq;
	}

	/* Request IRQ */
	ret = request_threaded_irq(charger->irq_vbus_pok, NULL,
			chg_vbus_in_isr, 0, "chgin-irq", charger);
	if (ret < 0) {
		pr_err("fail to request chgin IRQ: %d: %d\n", charger->irq_vbus_pok, ret);
		goto err_irq;
	}
	ret = request_threaded_irq(charger->irq_wpcin_pok, NULL,
			chg_wpcin_pok_isr, 0, "wpc-int", charger);
	if (ret) {
		pr_err("fail to request wpcin IRQ: %d: %d\n", charger->irq_wpcin_pok, ret);
		goto err_irq;
	}
	ret = request_threaded_irq(charger->irq_topoff, NULL,
			chg_topoff_isr, 0, "topoff-irq", charger);
	if (ret < 0) {
		pr_err("fail to request topoff IRQ: %d: %d\n", charger->irq_topoff, ret);
		goto err_power_supply_register_otg;
	}
	ret = request_threaded_irq(charger->irq_aicl, NULL,
			chg_aicl_isr, 0, "aicl-irq", charger);
	if (ret < 0) {
		pr_err("fail to request aicl IRQ: %d: %d\n", charger->irq_aicl, ret);
		goto err_power_supply_register_otg;
	}
	ret = request_threaded_irq(charger->irq_done, NULL,
			chg_done_isr, 0, "done-irq", charger);
	if (ret < 0) {
		pr_err("fail to request chgin IRQ: %d: %d\n", charger->irq_done, ret);
		goto err_power_supply_register_otg;
	}

	/* Watchdog-Timer Kick */
	sm5720_CHG_set_WDTMR_RST(charger);

	dev_info(&pdev->dev, "SM5720 Charger Driver Loaded\n");

	return 0;

err_irq:
	power_supply_unregister(&charger->psy_otg);
err_power_supply_register_otg:
	power_supply_unregister(&charger->psy_chg);
err_power_supply_register_chg:
	destroy_workqueue(charger->wqueue);
err_pdata_free:
	mutex_destroy(&charger->charger_mutex);
	kfree(charger);

	return ret;
}

static int sm5720_charger_remove(struct platform_device *pdev)
{
	struct sm5720_charger_data *charger = platform_get_drvdata(pdev);
	struct sm5720_platform_data *pdata = charger->sm5720_pdata;

	power_supply_unregister(&charger->psy_otg);
	power_supply_unregister(&charger->psy_chg);

	destroy_workqueue(charger->wqueue);

	kfree(pdata->charger_data);

	mutex_destroy(&charger->charger_mutex);

	kfree(charger);

	return 0;
}

#if defined CONFIG_PM
static int sm5720_charger_suspend(struct device *dev)
{
	return 0;
}

static int sm5720_charger_resume(struct device *dev)
{
	return 0;
}
#else
#define sm5720_charger_suspend NULL
#define sm5720_charger_resume NULL
#endif

static void sm5720_charger_shutdown(struct device *dev)
{
	struct sm5720_charger_data *charger = dev_get_drvdata(dev);

	pr_info("%s: SM5720 Charger driver shutdown\n", __func__);
	if (!charger->i2c) {
		pr_err("%s: no sm5720 i2c client\n", __func__);
		return;
	}
}

static SIMPLE_DEV_PM_OPS(sm5720_charger_pm_ops, sm5720_charger_suspend,
		sm5720_charger_resume);

static struct platform_driver sm5720_charger_driver = {
	.driver = {
		.name = "sm5720-charger",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &sm5720_charger_pm_ops,
#endif
		.shutdown = sm5720_charger_shutdown,
	},
	.probe = sm5720_charger_probe,
	.remove = sm5720_charger_remove,
};

static int __init sm5720_charger_init(void)
{
	pr_info("%s : \n", __func__);
	return platform_driver_register(&sm5720_charger_driver);
}

static void __exit sm5720_charger_exit(void)
{
	platform_driver_unregister(&sm5720_charger_driver);
}

module_init(sm5720_charger_init);
module_exit(sm5720_charger_exit);

MODULE_DESCRIPTION("Samsung SM5720 Charger Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
