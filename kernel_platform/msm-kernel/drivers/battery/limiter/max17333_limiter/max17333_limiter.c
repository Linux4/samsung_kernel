/*
 * Copyright (c) 2023 Maxim Integrated Products, Inc.
 * Author: Maxim Integrated <opensource@maximintegrated.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/version.h>
#include <asm/unaligned.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/mfd/max17333.h>
#include "max17333_limiter.h"

/* for Regmap */
#include <linux/regmap.h>

/* for Device Tree */
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>

#ifdef USE_MAX17333_FUELGAUGE
#define USE_STATUS_ALERT
#endif

#define MAX17333_CURRENT_STANDARD_SCALAR 15625
#define MAX17333_CURRENT_SCALAR(rsense) (10/rsense)

#define MAX17333_QSCALE_CURRENT_STEP(_x, _bit) (((_x) & 0x7) == _bit)
#define MAX17333_QSCALE_CURRENT(_x) (MAX17333_QSCALE_CURRENT_STEP(_x, 0x0) ? 25 :\
				MAX17333_QSCALE_CURRENT_STEP(_x, 0x1) ? 50 :\
				MAX17333_QSCALE_CURRENT_STEP(_x, 0x2) ? 100 :\
				MAX17333_QSCALE_CURRENT_STEP(_x, 0x3) ? 200 :\
				MAX17333_QSCALE_CURRENT_STEP(_x, 0x4) ? 250 :\
				MAX17333_QSCALE_CURRENT_STEP(_x, 0x5) ? 400 :\
				MAX17333_QSCALE_CURRENT_STEP(_x, 0x6) ? 500 :\
				MAX17333_QSCALE_CURRENT_STEP(_x, 0x7) ? 1000 : 25)

static char *chg_supplied_to[] = {
	"max17333-charger",
};

static char *chg_sub_supplied_to[] = {
	"max17333-charger-sub",
};

static char *max17333_battery_type[] = {
	"SUB",
	"MAIN",
};

//#define __lock(_me)    mutex_lock(&(_me)->lock)
//#define __unlock(_me)  mutex_unlock(&(_me)->lock)

static enum power_supply_property max17333_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
};

static struct device_attribute max17333_chg_attrs[] = {
	MAX17333_CHG_ATTR(max17333_chg_current),
	MAX17333_CHG_ATTR(max17333_chg_en),
	MAX17333_CHG_ATTR(max17333_chg_voltage),
	MAX17333_CHG_ATTR(max17333_prechg_current),
	MAX17333_CHG_ATTR(max17333_prequal_voltage),
	MAX17333_CHG_ATTR(max17333_ship_en),
	MAX17333_CHG_ATTR(max17333_commsh_en),
};

static int max17333_set_chg_current(struct max17333_charger_data *charger, unsigned int curr);

static inline int max17333_raw_voltage_to_uvolts(u16 lsb)
{
	return lsb * 625 / 8; /* 78.125uV per bit */
}

static inline int max17333_raw_current_to_uamps(struct max17333_charger_data *charger, int curr)
{
	return curr * 15625 / ((int)charger->rsense * 10);
}

static void max17333_periodic_dump(struct max17333_charger_data *charger)
{
	int i, ret;
	char *str = NULL;
	u16 read_val = 0;
	u16 max17333_debug_shdw_reg[72];
	u16 max17333_debug_reg[52] = {
		REG_STATUS,
		0x04, // AtRate
		REG_AGE,
		REG_MAXMINVOLT,
		0x09, // MaxMinTemp
		0x0A, // MaxMinCurrent
		REG_CONFIG,
		0x0F, // MiscCfg
		REG_AVGVCELL,
		REG_VCELL,
		REG_TEMP,
		REG_CURRENT,
		REG_AVGCURRENT,
		0x1E,// IChgTerm
		REG_VERSION,
		REG_CHARGING_CURRENT,
		REG_CHARGING_VOLTAGE,
		0x2C, // Reserved2C
		0x2D, // Reserved2D
		0x2E, // Reserved2E
		0x2F, // Reserved2F
		0x37, // Reserved37
		0x38, // Reserved38
		REG_FSTAT2,
		0x3C, // Reserved3C
		REG_FSTAT,
		0x3E, // Timer
		0x3F, // Reserved3F
		0x49, // ProtTmrStat
		REG_QH,
		0x4E, // QL
		REG_CHGSTAT,
		REG_CONFIG2,
		REG_PROTALRTS,
		REG_STATUS2,
		REG_FOTPSTAT,
		0xBE, // TimerH
		0xD1, // ReservedD1
		0xD2, // ReservedD2
		0xD3, // ReservedD3
		REG_AVGCELL1,
		0xD5, // ReservedD5
		0xD6, // ReservedD6
		0xD7, // Batt
		0xD8, // Cell1
		REG_PROTSTATUS,
		REG_FPROTSTAT,
		REG_PCKP,
		0x79, // hChgTarg
		0x7A, // hChgPwm
		0x7B, // hChgEst
		0x7C // hChgCurr
	};

	u16 max17333_debug_hw_reg[11] = {
		REG_VG_TRIM,
		REG_CG_TRIM,
		REG_TG_TRIM,
		REG_HCONFIG2,
		REG_TEMP_RAW,
		REG_VOLT_RAW,
		REG_CURR_RAW,
		REG_HCONFIG,
		REG_VFOCV,
		REG_HPROTCFG,
		REG_HPROTCFG2
	};

	str = kzalloc(sizeof(char) * 4096, GFP_KERNEL);
	if (!str)
		return;

	/* NV registers */
	max17333_debug_shdw_reg[0] = REG_I2CCMD_SHDW;
	max17333_debug_shdw_reg[1] = REG_NRSENSE_SHDW;
	for (i = 2; i < ARRAY_SIZE(max17333_debug_shdw_reg); i++)
		max17333_debug_shdw_reg[i] = REG_NLEARNCFG_SHDW + i - 2;
	for (i = 0; i < ARRAY_SIZE(max17333_debug_shdw_reg); i++) {
		ret = max17333_read(charger->regmap_shdw, max17333_debug_shdw_reg[i], &read_val);
		if (ret < 0) {
			kfree(str);
			return;
		}
		sprintf(str + strlen(str), "%04xh,", read_val);
	}
	pr_info("[MAX17333][%s] NV : %s\n", max17333_battery_type[charger->battery_type], str);
	kfree(str);

	/* FG registers */
	str = kzalloc(sizeof(char) * 4096, GFP_KERNEL);
	for (i = 0; i < ARRAY_SIZE(max17333_debug_reg); i++) {
		ret = max17333_read(charger->regmap, max17333_debug_reg[i], &read_val);
		if (ret < 0) {
			kfree(str);
			return;
		}
		sprintf(str + strlen(str), "%04xh,", read_val);
	}
	pr_info("[MAX17333][%s] FG: %s\n", max17333_battery_type[charger->battery_type], str);
	kfree(str);

	/* HW registers */
	str = kzalloc(sizeof(char) * 4096, GFP_KERNEL);
	for (i = 0; i < ARRAY_SIZE(max17333_debug_hw_reg); i++) {
		ret = max17333_read(charger->regmap, max17333_debug_hw_reg[i], &read_val);
		if (ret < 0) {
			kfree(str);
			return;
		}
		sprintf(str + strlen(str), "%04xh,", read_val);
	}
	pr_info("[MAX17333][%s] HW: %s\n", max17333_battery_type[charger->battery_type], str);

	kfree(str);
}

static inline int max17333_raw_capacity_to_uamph(struct max17333_charger_data *charger,
												 int cap)
{
	return cap * 5000 / (int)charger->rsense;
}

static int max17333_get_charging_status(struct max17333_charger_data *charger)
{
	return 0;
}

static unsigned int max17333_raw_current_to_chgdata(struct max17333_charger_data *charger,
												 unsigned int curr)
{
	unsigned int current_val;

	current_val = (curr * 1000) / MAX17333_CURRENT_SCALAR(charger->rsense);
	current_val = (current_val * 100) / MAX17333_CURRENT_STANDARD_SCALAR;

	return current_val;
}

static unsigned int max17333_raw_voltage_to_voltdata(unsigned int volt)
{
	return ((volt * 8 * 100) / 625) * 10;
}

static int max17333_chgdata_to_raw_current(struct max17333_charger_data *charger,
u16 chgdata)
{
	int current_val;

	current_val = max17333_raw_current_to_uamps(charger, sign_extend32(chgdata, 15));
	/* Convert to milliamps */
	current_val /= 1000;

	return current_val;
}

static int max17333_set_allowchg(struct max17333_charger_data *charger)
{
	u16 config_val;
	int ret;

	ret = max17333_read(charger->regmap_shdw, REG_I2CCMD_SHDW, &config_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_I2CCMD_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	pr_info("%s[%s] : Charging is allowed\n", __func__, max17333_battery_type[charger->battery_type]);
	config_val &= ~(MAX17333_I2CCMD_BLOCKCHG | MAX17333_I2CCMD_CHGDONE);

	ret = max17333_write(charger->regmap_shdw, REG_I2CCMD_SHDW, config_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to write REG_I2CCMD_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	return ret;
}

static int max17333_set_blockchg(struct max17333_charger_data *charger)
{
	u16 config_val;
	int ret;

	ret = max17333_read(charger->regmap_shdw, REG_I2CCMD_SHDW, &config_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_I2CCMD_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	pr_info("%s[%s] : Charging is blocked\n", __func__, max17333_battery_type[charger->battery_type]);
	config_val |= (MAX17333_I2CCMD_BLOCKCHG | MAX17333_I2CCMD_CHGDONE);

	ret = max17333_write(charger->regmap_shdw, REG_I2CCMD_SHDW, config_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to write REG_I2CCMD_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	return ret;
}

static int max17333_set_chg_current(struct max17333_charger_data *charger, unsigned int curr)
{
	u16 current_val;
	int ret;

	pr_info("%s[%s] : curr(%d)mA\n", __func__, max17333_battery_type[charger->battery_type], curr);

	current_val = (u16)max17333_raw_current_to_chgdata(charger, curr);

	pr_info("%s[%s] : charging Current (0x%x)\n", __func__, max17333_battery_type[charger->battery_type], current_val);

	ret = max17333_write(charger->regmap, REG_CHARGING_CURRENT, current_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to write REG_CHARGING_CURRENT\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	return ret;
}

static ssize_t max17333_get_chg_current(struct max17333_charger_data *charger, char *buf)
{
	u16 val;
	int ret, rc = 0;
	int curr;

	ret = max17333_read(charger->regmap, REG_CHARGING_CURRENT, &val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_CHARGING_CURRENT\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	curr = max17333_chgdata_to_raw_current(charger, val);

	rc += snprintf(buf + rc, PAGE_SIZE - rc, "CURRENT : %d mA\n", curr);

	return rc;
}

static int max17333_set_prechg_current(struct max17333_charger_data *charger, unsigned int curr)
{
	u16 current_val, val;
	int ret;
	int qscale;

	pr_info("%s[%s] : curr(%d)mA\n", __func__, max17333_battery_type[charger->battery_type], curr);

	ret = max17333_read(charger->regmap_shdw, REG_NDESIGNCAP_SHDW, &val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_NCHGCFG0_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	qscale = MAX17333_QSCALE_CURRENT(BITS_VALUE(val, MAX17333_DESIGNCAP_QSCALE)) / (charger->rsense);

	if ((curr/qscale) >= 1)
		current_val = (curr / qscale) - 1;
	else
		current_val = 0;

	if (current_val < 0 || current_val > 0x7) {
		pr_err("%s[%s]: value is out of range\n", __func__, max17333_battery_type[charger->battery_type]);
		return 0;
	}

	val = BITS_SET(val, MAX17333_CHGCFG0_PRECHGCURR, current_val);

	pr_info("%s[%s] : Prequal charging Voltage (0x%x)(0x%x)\n", __func__, max17333_battery_type[charger->battery_type], val, current_val);

	ret = max17333_write(charger->regmap_shdw, REG_NCHGCFG0_SHDW, val);
	if (ret < 0)
		pr_err("%s[%s]: fail to write REG_NCHGCFG0_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);

	return ret;
}


static ssize_t max17333_get_prechg_current(struct max17333_charger_data *charger, char *buf)
{
	u16 val;
	int ret, rc = 0;
	int curr;
	int qscale;

	ret = max17333_read(charger->regmap_shdw, REG_NDESIGNCAP_SHDW, &val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_NCHGCFG0_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	qscale = MAX17333_QSCALE_CURRENT(BITS_VALUE(val, MAX17333_DESIGNCAP_QSCALE)) / (charger->rsense);

	ret = max17333_read(charger->regmap_shdw, REG_NCHGCFG0_SHDW, &val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_NCHGCFG0_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	curr = (BITS_VALUE(val, MAX17333_CHGCFG0_PRECHGCURR) + 1) * qscale;

	rc += snprintf(buf + rc, PAGE_SIZE - rc, "PRECHGCURR : %d mA\n", curr);

	return rc;
}


static int max17333_set_chg_voltage(struct max17333_charger_data *charger, unsigned int voltage)
{
	u16 voltage_val;
	int ret;

	pr_info("%s[%s] : volt(%d)mV\n", __func__, max17333_battery_type[charger->battery_type], voltage);

	voltage_val = (u16)max17333_raw_voltage_to_voltdata(voltage);

	pr_info("%s[%s] : charging Voltage (0x%x)\n", __func__, max17333_battery_type[charger->battery_type], voltage_val);

	ret = max17333_write(charger->regmap, REG_CHARGING_VOLTAGE, voltage_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to write REG_CHARGING_VOLTAGE\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	return ret;
}

static ssize_t max17333_get_chg_voltage(struct max17333_charger_data *charger, char *buf)
{
	u16 val;
	int ret, rc = 0;
	int volt;

	ret = max17333_read(charger->regmap, REG_CHARGING_VOLTAGE, &val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_CHARGING_VOLTAGE\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	volt = max17333_raw_voltage_to_uvolts(val);
	volt /= 1000;

	rc += snprintf(buf + rc, PAGE_SIZE - rc, "VOLTAGE : %d mV\n", volt);

	return rc;
}

static int max17333_set_prequal_voltage(struct max17333_charger_data *charger, unsigned int voltage)
{
	u16 voltage_val, val;
	int ret;

	pr_info("%s[%s] : volt(%d)mV\n", __func__, max17333_battery_type[charger->battery_type], voltage);

	ret = max17333_read(charger->regmap_shdw, REG_NCHGCFG0_SHDW, &val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_NCHGCFG0_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	voltage -= 2200;
	voltage_val = voltage / 40;

	if (voltage_val < 0 || voltage_val > 0x1F) {
		pr_err("%s[%s]: value is out of range\n", __func__, max17333_battery_type[charger->battery_type]);
		return 0;
	}

	val = BITS_SET(val, MAX17333_CHGCFG0_PREQUALVOLT, voltage_val);

	pr_info("%s[%s] : Prequal charging Voltage (0x%x)(0x%x)\n", __func__, max17333_battery_type[charger->battery_type], val, voltage_val);

	ret = max17333_write(charger->regmap_shdw, REG_NCHGCFG0_SHDW, val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to write REG_NCHGCFG0_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	return ret;
}

static ssize_t max17333_get_prequal_voltage(struct max17333_charger_data *charger, char *buf)
{
	u16 val;
	int ret, rc = 0;
	int volt;

	ret = max17333_read(charger->regmap_shdw, REG_NCHGCFG0_SHDW, &val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_CHARGING_VOLTAGE\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	volt = 2200 + (BITS_VALUE(val, MAX17333_CHGCFG0_PREQUALVOLT) * 40);

	rc += snprintf(buf + rc, PAGE_SIZE - rc, "PREQUALVOLT : %d mV\n", volt);

	return rc;
}

static ssize_t max17333_get_chg_enable(struct max17333_charger_data *charger, char *buf)
{
	int rc = 0, ret;
	u16 val = 0;

	// Read Charging Status
	ret = max17333_read(charger->regmap_shdw, REG_I2CCMD_SHDW, &val);
	if (ret < 0) {
		dev_err(charger->dev, "[%s] REG_I2CCMD_SHDW Register reading failed (%d)\n", max17333_battery_type[charger->battery_type], ret);
		return ret;
	}

	if ((!(val & MAX17333_I2CCMD_BLOCKCHG)) && (!(val & MAX17333_I2CCMD_CHGDONE)))
		rc += snprintf(buf + rc, PAGE_SIZE - rc, "I2CCmd : Charging Enabled!\n");
	else
		rc += snprintf(buf + rc, PAGE_SIZE - rc, "I2CCmd : Charging Disabled!\n");

	return rc;
}

static void max17333_set_chg_enable(struct max17333_charger_data *charger, bool on)
{
	int ret;

	if (on) {
		ret = max17333_set_allowchg(charger);
		if (ret < 0) {
			dev_err(charger->dev, "[%s] Charging control failed (%d)\n", max17333_battery_type[charger->battery_type], ret);
			return;
		}
	} else {
		ret = max17333_set_blockchg(charger);
		if (ret < 0) {
			dev_err(charger->dev, "[%s] Charging control failed (%d)\n", max17333_battery_type[charger->battery_type], ret);
			return;
		}
	}
}

static bool max17333_get_ship_enable(struct max17333_charger_data *charger)
{
	bool shipen = false;
	int ret;
	u16 val = 0;

	// Read Status Register
	ret = max17333_read(charger->regmap, REG_STATUS, &val);
	if (ret < 0) {
		dev_err(charger->dev, "[%s] REG_STATUS Register reading failed (%d)\n", max17333_battery_type[charger->battery_type], ret);
		return false;
	}

	if (val & BIT_STATUS_SHIPRDY)
		shipen = true;

	return shipen;
}

static void max17333_set_ship_enable(struct max17333_charger_data *charger, bool on)
{
	u16 config;
	int ret;

	ret = max17333_read(charger->regmap, REG_CONFIG, &config);
	if (ret < 0)
		return;

	if (on) {
		config |= MAX17333_CONFIG_SHIP;
		pr_info("%s: Enable shipmode enter\n", __func__);
	} else {
		config &= ~MAX17333_CONFIG_SHIP;
		pr_info("%s: Disable shipmode enter\n", __func__);
	}

	ret = max17333_write(charger->regmap, REG_CONFIG, config);
	if (ret < 0)
		return;
}

static bool max17333_get_commsh_enable(struct max17333_charger_data *charger)
{
	bool commshen = false;
	int ret;
	u16 val = 0;

	// Read Config Register
	ret = max17333_read(charger->regmap, REG_CONFIG, &val);
	if (ret < 0) {
		dev_err(charger->dev, "[%s] REG_CONFIG Register reading failed (%d)\n", max17333_battery_type[charger->battery_type], ret);
		return false;
	}

	if (val & MAX17333_CONFIG_COMMSH)
		commshen = true;

	return commshen;
}


static int max17333_get_inow(struct max17333_charger_data *charger, u8 mode)
{
	int ret, inow;
	u16 reg;

	ret = max17333_read(charger->regmap, REG_AVGCURRENT, &reg);
	if (ret < 0)
		return 0;

	inow = max17333_raw_current_to_uamps(charger, sign_extend32(reg, 15));
	if (mode == SEC_BATTERY_CURRENT_MA)
		inow = inow / 1000;

	return inow;
}

static int max17333_get_vcell(struct max17333_charger_data *charger, u8 mode)
{
	int ret, vcell;
	u16 reg;

	ret = max17333_read(charger->regmap, REG_AVGVCELL, &reg);
	if (ret < 0)
		return ret;
	vcell = max17333_raw_voltage_to_uvolts(reg);
	if (mode == SEC_BATTERY_VOLTAGE_MV)
		vcell = vcell / 1000;

	return vcell;
}

static int max17333_get_bat_vcell(struct max17333_charger_data *charger, u8 mode)
{
	int ret, vcell;
	u16 reg;

	ret = max17333_read(charger->regmap, REG_VCELL, &reg);
	if (ret < 0)
		return ret;

	vcell = max17333_raw_voltage_to_uvolts(reg);
	if (mode == SEC_BATTERY_VOLTAGE_MV)
		vcell = vcell / 1000;

	return vcell;
}

static int max17333_set_commsh_enable(struct max17333_charger_data *charger, bool on)
{
	u16 config;
	int ret;

	ret = max17333_read(charger->regmap, REG_CONFIG, &config);
	if (ret < 0)
		return ret;

	if (on) {
		config |= MAX17333_CONFIG_COMMSH;
		pr_info("%s: Commsh bit is Enabled\n", __func__);
	} else {
		config &= ~MAX17333_CONFIG_COMMSH;
		pr_info("%s: Commsh bit is Disabled\n", __func__);
	}

	ret = max17333_write(charger->regmap, REG_CONFIG, config);
	if (ret < 0)
		return ret;

	return ret;
}

static void max17333_permfail_check(struct max17333_charger_data *charger)
{
	u16 protstatus, battstatus;
	int ret;

	ret = max17333_read(charger->regmap, REG_PROTSTATUS, &protstatus);
	if (ret < 0)
		return;

	ret = max17333_read(charger->regmap_shdw, REG_NBATTSTATUS_SHDW, &battstatus);
	if (ret < 0)
		return;

	if ((protstatus & BIT_PROTSTATUS_PERMFAIL) || (battstatus & BIT_NBATTSTATUS_PERMFAIL)) {
		pr_info("%s: PROTSTATUS PERMFAIL bit clear\n", __func__);
		ret = max17333_write(charger->regmap, REG_PROTSTATUS, (protstatus & BIT_PROTSTATUS_PERMFAIL_MASK));
		if (ret < 0)
			return;

		usleep_range(1000, 2000);

		ret = max17333_read(charger->regmap, REG_PROTSTATUS, &protstatus);
		if (ret < 0)
			return;

		if ((protstatus & BIT_PROTSTATUS_PERMFAIL))
			max17333_write(charger->regmap, REG_PROTSTATUS, (protstatus & BIT_PROTSTATUS_PERMFAIL_MASK));

		pr_info("%s: NBATTSTATUS PERMFAIL bit clear\n", __func__);
		ret = max17333_write(charger->regmap_shdw, REG_NBATTSTATUS_SHDW, (battstatus & BIT_NBATTSTATUS_PERMFAIL_MASK));
		if (ret < 0)
			return;

		usleep_range(1000, 2000);

		ret = max17333_read(charger->regmap_shdw, REG_NBATTSTATUS_SHDW, &battstatus);
		if (ret < 0)
			return;

		if ((battstatus & BIT_NBATTSTATUS_PERMFAIL))
			max17333_write(charger->regmap_shdw, REG_NBATTSTATUS_SHDW, (battstatus & BIT_NBATTSTATUS_PERMFAIL_MASK));
	}

	return;
}

static int max17333_chg_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < (int)ARRAY_SIZE(max17333_chg_attrs); i++) {
		rc = device_create_file(dev, &max17333_chg_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	return rc;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &max17333_chg_attrs[i]);
	return rc;
}

ssize_t max17333_chg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct max17333_charger_data *charger = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - max17333_chg_attrs;
	int i = 0;

	dev_info(charger->dev, "[%s]\n", max17333_battery_type[charger->battery_type]);

	switch (offset) {
	case MAX17333_CHG_CURRENT:
		i = max17333_get_chg_current(charger, buf);
		break;
	case MAX17333_CHG_EN:
		i = max17333_get_chg_enable(charger, buf);
		break;
	case MAX17333_CHG_VOLTAGE:
		i = max17333_get_chg_voltage(charger, buf);
		break;
	case MAX17333_PRECHG_CURRENT:
		i = max17333_get_prechg_current(charger, buf);
		break;
	case MAX17333_PREQUAL_VOLTAGE:
		i = max17333_get_prequal_voltage(charger, buf);
		break;
	case MAX17333_SHIP_EN:
		if (max17333_get_ship_enable(charger))
			i = snprintf(buf, PAGE_SIZE, "Ship is Ready!\n");
		else
			i = snprintf(buf, PAGE_SIZE, "Ship is Not Ready!\n");
		break;
	case MAX17333_COMMSH_EN:
		if (max17333_get_commsh_enable(charger))
			i = snprintf(buf, PAGE_SIZE, "Commsh was Enabled!\n");
		else
			i = snprintf(buf, PAGE_SIZE, "Commsh was Disabled!\n");
		break;
	default:
		return -EINVAL;
	}
	return i;
}

ssize_t max17333_chg_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct max17333_charger_data *charger = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - max17333_chg_attrs;
	int x, ret;

	dev_info(charger->dev, "[%s]\n", max17333_battery_type[charger->battery_type]);

	switch (offset) {
	case MAX17333_CHG_CURRENT:
		if (sscanf(buf, "%5d\n", &x) == 1) {
			dev_info(charger->dev, "[%s] current = %d\n", max17333_battery_type[charger->battery_type], x);
			max17333_set_chg_current(charger, x);
		}
		ret = count;
		break;
	case MAX17333_CHG_EN:
		if (sscanf(buf, "%5d\n", &x) == 1) {
			dev_info(charger->dev, "[%s] chg en = %d\n", max17333_battery_type[charger->battery_type], x);
			max17333_set_chg_enable(charger, x);
		}
		ret = count;
		break;
	case MAX17333_CHG_VOLTAGE:
		if (sscanf(buf, "%5d\n", &x) == 1) {
			dev_info(charger->dev, "[%s] voltage = %d\n", max17333_battery_type[charger->battery_type], x);
			max17333_set_chg_voltage(charger, x);
		}
		ret = count;
		break;
	case MAX17333_PRECHG_CURRENT:
		if (sscanf(buf, "%5d\n", &x) == 1) {
			dev_info(charger->dev, "[%s] prechg current = %d\n", max17333_battery_type[charger->battery_type], x);
			max17333_set_prechg_current(charger, x);
		}
		ret = count;
		break;
	case MAX17333_PREQUAL_VOLTAGE:
		if (sscanf(buf, "%5d\n", &x) == 1) {
			dev_info(charger->dev, "[%s] prequal voltage = %d\n", max17333_battery_type[charger->battery_type], x);
			max17333_set_prequal_voltage(charger, x);
		}
		ret = count;
		break;
	case MAX17333_SHIP_EN:
		if (sscanf(buf, "%5d\n", &x) == 1) {
			dev_info(charger->dev, "[%s] ship en = %d\n", max17333_battery_type[charger->battery_type], x);
			max17333_set_ship_enable(charger, x);
		}
		ret = count;
		break;
	case MAX17333_COMMSH_EN:
		if (sscanf(buf, "%5d\n", &x) == 1) {
			dev_info(charger->dev, "[%s] commsh en = %d\n", max17333_battery_type[charger->battery_type], x);
			max17333_set_commsh_enable(charger, x);
		}
		ret = count;
		break;
	default:
		ret = -EINVAL;
	}

	max17333_periodic_dump(charger);

	return ret;
}

static int max17333_charger_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	struct max17333_charger_data *charger =
		power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property)psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = max17333_get_charging_status(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = max17333_get_inow(charger, val->intval);
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_BAT_VOLTAGE:
			val->intval = max17333_get_vcell(charger, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_BAT_VCELL:
			val->intval = max17333_get_bat_vcell(charger, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_CHG_CURRENT:
			val->intval = max17333_get_inow(charger, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_LIMITER_SHIPMODE:
			val->intval = max17333_get_ship_enable(charger);
			break;
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			max17333_periodic_dump(charger);
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	max17333_permfail_check(charger);

	return 0;
}

static int max17333_charger_set_property(struct power_supply *psy,
								 enum power_supply_property psp,
								 const union power_supply_propval *val)
{
	struct max17333_charger_data *charger =
		power_supply_get_drvdata(psy);
	int ret = 0;
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property)psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		pr_info("%s[%s]: is it full? %d\n", __func__, max17333_battery_type[charger->battery_type], val->intval);
		max17333_set_chg_enable(charger, !val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		max17333_set_chg_voltage(charger, val->intval);
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_SUPLLEMENT_MODE:
			//max17333_set_chg_enable(charger, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_CHG_MODE:
			//max17333_set_chg_enable(charger, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_FASTCHG_LIMIT_CURRENT:
			max17333_set_chg_current(charger, val->intval);
			max17333_periodic_dump(charger);
			break;
		case POWER_SUPPLY_EXT_PROP_LIMITER_SHIPMODE:
			max17333_set_ship_enable(charger, val->intval);
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int max17333_chg_property_is_writeable(struct power_supply *psy,
										  enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = 1;
		break;
	default:
		ret = 0;
	}

	return ret;
}

#ifdef USE_STATUS_ALERT
static irqreturn_t max17333_protalrt_irq_isr(int irq, void *data)
{
	struct max17333_charger_data *charger = data;
	int ret;
	u16 val;

	dev_info(charger->dev, "[%s] Protection Interrupt Handler!\n", max17333_battery_type[charger->battery_type]);

	/* Check Protection Alert type */

	ret = max17333_read(charger->regmap, REG_PROTALRTS, &val);
	if (ret < 0) {
		dev_err(charger->dev, "[%s]: fail to read REG_PROTALRTS\n", max17333_battery_type[charger->battery_type]);
		return IRQ_HANDLED;
	}

	if (val & BIT_CHGWDT_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Charge Watch Dog Timer!\n", max17333_battery_type[charger->battery_type]);
	if (val & BIT_TOOHOTC_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Overtemperature for Charging!\n", max17333_battery_type[charger->battery_type]);
	if (val & BIT_FULL_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Full Detection!\n", max17333_battery_type[charger->battery_type]);
	if (val & BIT_TOOCOLDC_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Undertemperature!\n", max17333_battery_type[charger->battery_type]);
	if (val & BIT_OVP_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Overvoltage!\n", max17333_battery_type[charger->battery_type]);
	if (val & BIT_OCCP_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Overcharge Current!\n", max17333_battery_type[charger->battery_type]);
	if (val & BIT_QOVFLW_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Q Overflow!\n", max17333_battery_type[charger->battery_type]);
	if (val & BIT_PERMFAIL_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Permanent Failure!\n", max17333_battery_type[charger->battery_type]);
	if (val & BIT_DIEHOT_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Overtemperature for die temperature!\n", max17333_battery_type[charger->battery_type]);
	if (val & BIT_TOOHOTD_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Overtemperature for Discharging!\n", max17333_battery_type[charger->battery_type]);
	if (val & BIT_UVP_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Undervoltage Protection!\n", max17333_battery_type[charger->battery_type]);
	if (val & BIT_ODCP_INT)
		dev_info(charger->dev, "[%s] Protection Alert: Overdischarge current!\n", max17333_battery_type[charger->battery_type]);

	/* Clear alerts */
	ret = max17333_write(charger->regmap, REG_PROTALRTS, val & REG_PROTALRTS_MASK);
	if (ret < 0) {
		dev_err(charger->dev, "[%s]: fail to write REG_PROTALRTS\n", max17333_battery_type[charger->battery_type]);
		return IRQ_HANDLED;
	}

	return IRQ_HANDLED;
}

static irqreturn_t max17333_alrt_irq_isr(int irq, void *data)
{
	struct max17333_charger_data *charger = data;
	int ret;
	u16 staus, chgstat;

    u16 vg_trim_val;
    u16 cg_trim_val;
    u16 tg_trim_val;
    u16 hconfig2;
    u16 temp_raw;
    u16 volt_raw;
    u16 curr_raw;
    u16 hconfig;
    u16 vfocv;

	dev_info(charger->dev, "[%s]STATUS : Interrupt Handler!\n", max17333_battery_type[charger->battery_type]);

	/* Check alert type */
	ret = max17333_read(charger->regmap, REG_STATUS, &staus);
	if (ret < 0) {
		dev_err(charger->dev, "[%s]: fail to read REG_STATUS\n", max17333_battery_type[charger->battery_type]);
		return IRQ_HANDLED;
	}

	if (staus & BIT_STATUS_POR)
		dev_info(charger->dev, "[%s] Alert: POR Detect!\n", max17333_battery_type[charger->battery_type]);
	if (staus & BIT_STATUS_CA) {
		dev_info(charger->dev, "[%s] Alert: CHARGING!\n", max17333_battery_type[charger->battery_type]);

		/* Check Charging type */
		ret = max17333_read(charger->regmap, REG_CHGSTAT, &chgstat);

		if (chgstat & BIT_STATUS_CP)
			dev_info(charger->dev, "[%s] Charging Alert: Heat limit!\n", max17333_battery_type[charger->battery_type]);
		if (chgstat & BIT_STATUS_CT)
			dev_info(charger->dev, "[%s] Charging Alert: FET Temperature limit!\n", max17333_battery_type[charger->battery_type]);
		if (chgstat & BIT_STATUS_DROPOUT)
			dev_info(charger->dev, "[%s] Charging Alert: Dropout!\n", max17333_battery_type[charger->battery_type]);
	}

    max17333_read(charger->regmap, REG_VG_TRIM, &vg_trim_val);
    max17333_read(charger->regmap, REG_CG_TRIM, &cg_trim_val);
    max17333_read(charger->regmap, REG_TG_TRIM, &tg_trim_val);
    max17333_read(charger->regmap, REG_HCONFIG2, &hconfig2);
    max17333_read(charger->regmap, REG_TEMP_RAW, &temp_raw);
    max17333_read(charger->regmap, REG_VOLT_RAW, &volt_raw);
    max17333_read(charger->regmap, REG_CURR_RAW, &curr_raw);
	max17333_read(charger->regmap, REG_HCONFIG, &hconfig);
	max17333_read(charger->regmap, REG_VFOCV, &vfocv);

    pr_info("[MAX17333][%s] HW: %04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh\n", max17333_battery_type[charger->battery_type],
			vg_trim_val, cg_trim_val, tg_trim_val, hconfig2, temp_raw, volt_raw, curr_raw, hconfig, vfocv);

	/* Clear alerts */
	ret = max17333_write(charger->regmap, REG_STATUS, staus & REG_STATUS_MASK);
	if (ret < 0) {
		dev_err(charger->dev, "[%s]: fail to write REG_STATUS\n", max17333_battery_type[charger->battery_type]);
		return IRQ_HANDLED;
	}

	return IRQ_HANDLED;
}
#endif

static int max17333_charger_initialize(struct max17333_charger_data *charger)
{
	int ret = 0;

	ret = max17333_set_chg_voltage(charger, 4450);
	ret |= max17333_set_chg_current(charger, 1000);
	ret |= max17333_set_allowchg(charger);
#if defined(CONFIG_SEC_FACTORY)
	ret |= max17333_set_commsh_enable(charger, 1);
#else
	ret |= max17333_set_commsh_enable(charger, 0);
#endif
	return ret;
}

static int max17333_charger_probe(struct platform_device *pdev)
{
	struct max17333_dev *max17333 = dev_get_drvdata(pdev->dev.parent);
	struct max17333_charger_data *charger;
	int ret = 0;
	struct power_supply_config charger_cfg = {};

	pr_info("%s: MAX17333 Charger Driver Loading\n", __func__);

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	mutex_init(&charger->lock);

	charger->dev = &pdev->dev;
	charger->max17333 = max17333;
	charger->regmap = max17333->regmap_pmic;
	charger->regmap_shdw = max17333->regmap_shdw;
	charger->rsense = max17333->pdata->rsense;
	charger->cycles_reg_lsb_percent = 25;
	charger->battery_type = max17333->pdata->battery_type;

	platform_set_drvdata(pdev, charger);
	if (charger->battery_type) {
		pr_info("%s: max17333 charger main power supply register\n", __func__);
		charger->psy_chg_d.name		= "max17333-charger";
		charger->psy_chg_d.type		= POWER_SUPPLY_TYPE_UNKNOWN;
		charger->psy_chg_d.get_property	= max17333_charger_get_property;
		charger->psy_chg_d.set_property	= max17333_charger_set_property;
		charger->psy_chg_d.property_is_writeable   = max17333_chg_property_is_writeable;
		charger->psy_chg_d.properties	= max17333_charger_props;
		charger->psy_chg_d.num_properties	=
			ARRAY_SIZE(max17333_charger_props);
		charger_cfg.drv_data = charger;
		charger_cfg.supplied_to = chg_supplied_to;
		charger_cfg.of_node = max17333->dev->of_node;
		charger_cfg.num_supplicants = ARRAY_SIZE(chg_supplied_to);
	} else {
		pr_info("%s: max17333 charger sub power supply register\n", __func__);
		charger->psy_chg_d.name		= "max17333-charger-sub";
		charger->psy_chg_d.type		= POWER_SUPPLY_TYPE_UNKNOWN;
		charger->psy_chg_d.get_property	= max17333_charger_get_property;
		charger->psy_chg_d.set_property	= max17333_charger_set_property;
		charger->psy_chg_d.property_is_writeable   = max17333_chg_property_is_writeable;
		charger->psy_chg_d.properties	= max17333_charger_props;
		charger->psy_chg_d.num_properties	=
			ARRAY_SIZE(max17333_charger_props);
		charger_cfg.drv_data = charger;
		charger_cfg.supplied_to = chg_sub_supplied_to;
		charger_cfg.of_node = max17333->dev->of_node;
		charger_cfg.num_supplicants = ARRAY_SIZE(chg_sub_supplied_to);
	}

	charger->psy_chg =
		devm_power_supply_register(max17333->dev,
				&charger->psy_chg_d,
				&charger_cfg);
	if (IS_ERR(charger->psy_chg)) {
		pr_err("Couldn't register psy_chg rc=%ld\n",
				PTR_ERR(charger->psy_chg));
		goto error;
	}

#ifdef USE_STATUS_ALERT
	if (max17333->irq > 0) {
		ret = devm_request_threaded_irq(max17333->dev, max17333->irq, NULL,
			max17333_alrt_irq_isr,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_SHARED,
			"alrt-irq", charger);
		if (ret) {
			pr_err("%s: Failed to Request IRQ\n", __func__);
			goto error_alrt_irq;
		}
		dev_info(&pdev->dev, "MAX17333 irq requested %d\n", max17333->irq);
	}

	if (max17333->irqc_intsrc) {
		charger->prot_irq = regmap_irq_get_virq(max17333->irqc_intsrc,
			MAX17333_FG_PROT_INT);
		dev_info(&pdev->dev, "MAX17333 prot irq %d\n", charger->prot_irq);
	}

	if (charger->prot_irq > 0) {
		ret = request_threaded_irq(charger->prot_irq, NULL,
			max17333_protalrt_irq_isr,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_SHARED,
			"protalrt-irq", charger);
		if (ret) {
			pr_err("%s: Failed to Request IRQ\n", __func__);
			goto error_protalrt_irq;
		}
	}

	/* Enable alert on fuel-gauge outputs */
	max17333_update_bits(charger->regmap, REG_CONFIG, BIT_CONFIG_ALRT_EN, BIT_CONFIG_ALRT_EN);

	/* Clear alerts */
	max17333_write(charger->regmap, REG_PROTALRTS, REG_PROTALRTS_MASK);
	max17333_write(charger->regmap, REG_STATUS, REG_STATUS_MASK);
#endif

	ret = max17333_chg_create_attrs(&charger->psy_chg->dev);
	if (ret) {
		dev_err(charger->dev,
			"%s : Failed to create_attrs\n", __func__);
	}

	pr_info("%s: Charger Initialize\n", __func__);
	ret = max17333_charger_initialize(charger);
	if (ret < 0) {
		dev_err(charger->dev, "Error: Initializing Charger\n");
		goto error_init;
	}

	if (charger->battery_type)
		sec_chg_set_dev_init(SC_DEV_MAIN_LIM);
	else
		sec_chg_set_dev_init(SC_DEV_SUB_LIM);

	pr_info("%s: MAX17333 Charger Driver Loaded\n", __func__);
	return 0;

error_init:
#ifdef USE_STATUS_ALERT
	if (charger->prot_irq)
		free_irq(charger->prot_irq, charger);

error_protalrt_irq:
	if (max17333->irq)
		free_irq(max17333->irq, charger);

error_alrt_irq:
#endif
	power_supply_unregister(charger->psy_chg);

error:
	kfree(charger);
	return ret;
}

static int max17333_charger_remove(struct platform_device *pdev)
{
	struct max17333_charger_data *charger =
		platform_get_drvdata(pdev);

	power_supply_unregister(charger->psy_chg);

	kfree(charger);

	return 0;
}

static const struct platform_device_id max17333_charger_id[] = {
	{ "max17333-charger", 0, },
	{ }
};
MODULE_DEVICE_TABLE(platform, max17333_charger_id);

static struct platform_driver max17333_charger_driver = {
	.driver = {
		.name = "max17333-charger",
	},
	.probe = max17333_charger_probe,
	.remove = max17333_charger_remove,
	.id_table = max17333_charger_id,
};

static const struct platform_device_id max17333_charger_sub_id[] = {
	{ "max17333-charger-sub", 0, },
	{ }
};
MODULE_DEVICE_TABLE(platform, max17333_charger_sub_id);

static struct platform_driver max17333_charger_sub_driver = {
	.driver = {
		.name = "max17333-charger-sub",
	},
	.probe = max17333_charger_probe,
	.remove = max17333_charger_remove,
	.id_table = max17333_charger_sub_id,
};

static int __init max17333_limiter_init(void)
{
	pr_info("%s:\n", __func__);
	platform_driver_register(&max17333_charger_driver);
	platform_driver_register(&max17333_charger_sub_driver);
	return 0;
}

static void __exit max17333_limiter_exit(void)
{
	platform_driver_unregister(&max17333_charger_driver);
	platform_driver_unregister(&max17333_charger_sub_driver);
}

module_init(max17333_limiter_init);
module_exit(max17333_limiter_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MAX17333 Charger");
MODULE_VERSION(MAX17333_RELEASE_VERSION);
