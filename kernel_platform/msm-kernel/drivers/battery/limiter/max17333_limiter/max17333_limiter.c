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

#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

static int factory_mode;
module_param(factory_mode, int, 0444);

#if IS_ENABLED(CONFIG_FUELGAUGE_MAX17333)
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
	MAX17333_CHG_ATTR(max17333_chg_pckp),
	MAX17333_CHG_ATTR(max17333_ovp_voltage),	
};

static int max17333_set_chg_current(struct max17333_charger_data *charger, unsigned int curr);
static int max17333_get_chg_current(struct max17333_charger_data *charger);
static int max17333_set_chg_voltage(struct max17333_charger_data *charger, unsigned int voltage);
static bool max17333_get_dnr_status(struct max17333_charger_data *charger);
static bool max17333_get_ovp_status(struct max17333_charger_data *charger);

static inline int max17333_raw_voltage_to_uvolts(u16 lsb)
{
	return lsb * 625 / 8; /* 78.125uV per bit */
}

static inline u32 max17333_raw_pckp_to_uvolts(u16 lsb)
{
	return lsb * 2500 / 8; /* 312.5uV per bit */
}

static inline int max17333_raw_current_to_uamps(struct max17333_charger_data *charger, int curr)
{
	return curr * 15625 / ((int)charger->rsense * 10);
}

#if IS_ENABLED(CONFIG_FUELGAUGE_MAX17333)
static u32 oci_cnt = 0; // debug
static void max17333_error_check(struct max17333_charger_data *charger)
{
	u16 config_val;
	static u32 err_cnt = 0;

	max17333_read(charger->regmap_shdw, REG_I2CCMD_SHDW, &config_val);
	if (config_val == 0x2000)
		err_cnt++;
	else
		err_cnt = 0;

	if (oci_cnt || err_cnt)
		pr_info("[%s] %s oci_cnt = %d, err_cnt = %d\n", __func__, max17333_battery_type[charger->battery_type], oci_cnt, err_cnt);

	if (err_cnt > 5) {
		err_cnt = 0;
		pr_info("[%s] %s REG_I2CCMD_SHDW has wrong value which never set\n", __func__, max17333_battery_type[charger->battery_type]);
#if IS_ENABLED(CONFIG_SEC_ABC)
		sec_abc_send_event("MODULE=battery@WARN=lim_stuck");
#endif
	}
}

static int max17333_manual_chg_enable(struct max17333_charger_data *charger)
{
	u16 read_val = 0;
	int ret = 0;

	pr_info("[%s] %s \n", __func__, max17333_battery_type[charger->battery_type]);
	ret = max17333_read(charger->regmap, REG_CONFIG, &read_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	read_val |= MAX17333_CONFIG_MANCHG;
	ret = max17333_write(charger->regmap, REG_CONFIG, read_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to write\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}
	usleep_range(1000, 1100);

	return 0;
}

static bool max17333_get_manual_chg_status(struct max17333_charger_data *charger)
{
	u16 read_val = 0;
	int ret = 0;

	ret = max17333_read(charger->regmap, REG_CONFIG, &read_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read\n", __func__, max17333_battery_type[charger->battery_type]);
		return true;
	}

	if ((read_val & MAX17333_CONFIG_MANCHG) == 0x00) {
		pr_info("%s read_val = 0x%x, manual chg get back to 0\n", __func__, read_val);
		return false;
	}

	return true;
}

static void max17333_periodic_dump(struct max17333_charger_data *charger)
{
	int i, ret;
	char *str = NULL;
	u16 read_val = 0;
	int index = 0;
	u16 max17333_debug_shdw_reg[0x82];
	u16 max17333_debug_reg[0x80 + 6 + 15];

    u16 vg_trim_val;
    u16 cg_trim_val;
    u16 tg_trim_val;
    u16 hconfig2;
    u16 temp_raw;
    u16 volt_raw;
    u16 curr_raw;
    u16 hconfig;
    u16 vfocv;
	u16 hprotcfg;
	u16 hprotcfg2;

	max17333_debug_shdw_reg[0] = REG_I2CCMD_SHDW;
	max17333_debug_shdw_reg[1] = REG_PATCH_ALLOW;
	for (i = 2; i < 0x82; i++) {
		max17333_debug_shdw_reg[i] = REG_NXTABLE0_SHDW + i - 2;
	}

	max17333_debug_reg[0x80] = 0x79;
	max17333_debug_reg[0x81] = 0x7A;
	max17333_debug_reg[0x82] = 0x7B;
	max17333_debug_reg[0x83] = 0x7C;
	max17333_debug_reg[0x84] = 0xF0;
	max17333_debug_reg[0x85] = 0xF1;
	max17333_debug_reg[0x86] = 0xF2;
	max17333_debug_reg[0x87] = 0xF3;
	max17333_debug_reg[0x88] = 0xF4;
	max17333_debug_reg[0x89] = 0xF5;
	max17333_debug_reg[0x8A] = 0xF6;
	max17333_debug_reg[0x8B] = 0xF7;
	max17333_debug_reg[0x8C] = 0xF8;
	max17333_debug_reg[0x8D] = 0xF9;
	max17333_debug_reg[0x8E] = 0xFA;
	max17333_debug_reg[0x8F] = 0xFB;
	max17333_debug_reg[0x90] = 0xFC;
	max17333_debug_reg[0x91] = 0xFD;
	max17333_debug_reg[0x92] = 0xFE;
	max17333_debug_reg[0x93] = 0xFF;
	max17333_debug_reg[0x94] = 0xEE;

	index = 0;
	for (i = 0; i < 0x50; i++) {
		max17333_debug_reg[index] = i;
		index++;
	}
	for (i = 0xa0; i < 0xc0; i++) {
		max17333_debug_reg[index] = i;
		index++;
	}
	for (i = 0xd0; i < 0xe0; i++) {
		max17333_debug_reg[index] = i;
		index++;
	}

	str = kzalloc(sizeof(char) * 4096, GFP_KERNEL);
	if (!str)
		return;

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

    max17333_read(charger->regmap, REG_VG_TRIM, &vg_trim_val);
    max17333_read(charger->regmap, REG_CG_TRIM, &cg_trim_val);
    max17333_read(charger->regmap, REG_TG_TRIM, &tg_trim_val);
    max17333_read(charger->regmap, REG_HCONFIG2, &hconfig2);
    max17333_read(charger->regmap, REG_TEMP_RAW, &temp_raw);
    max17333_read(charger->regmap, REG_VOLT_RAW, &volt_raw);
    max17333_read(charger->regmap, REG_CURR_RAW, &curr_raw);
    max17333_read(charger->regmap, REG_HCONFIG, &hconfig);
    max17333_read(charger->regmap, REG_VFOCV, &vfocv);
    max17333_read(charger->regmap, REG_HPROTCFG, &hprotcfg);
    max17333_read(charger->regmap, REG_HPROTCFG2, &hprotcfg2);

    pr_info("[MAX17333][%s] HW: %04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh\n", max17333_battery_type[charger->battery_type],
			vg_trim_val, cg_trim_val, tg_trim_val, hconfig2, temp_raw, volt_raw, curr_raw, hconfig, vfocv, hprotcfg, hprotcfg2);

	max17333_error_check(charger);
	if (!max17333_get_manual_chg_status(charger)) {
		max17333_manual_chg_enable(charger);
		max17333_get_manual_chg_status(charger);
		max17333_set_chg_voltage(charger, charger->chg_vol);
		max17333_set_chg_current(charger, charger->chg_curr);
		pr_info("%s curr=%dmA\n", __func__, max17333_get_chg_current(charger));
//#if !defined(CONFIG_SEC_FACTORY)
		//panic("manual chg get back to 0");
//#endif
	}
}
#else
static void max17333_periodic_dump(struct max17333_charger_data *charger)
{
	int i, ret;
	char *str = NULL;
	u16 read_val = 0;
	u16 max17333_debug_shdw_reg[13] = {
		REG_I2CCMD_SHDW,
		REG_NCONFIG_SHDW,
		REG_NCHGCTRL1_SHDW,
		REG_NICHGTERM_SHDW,
		REG_NCHGCFG0_SHDW,
		REG_NCHGCTRL0_SHDW,
		REG_NUVPRTTH_SHDW,
		REG_NTPRTTH1_SHDW,
		REG_NTPRTTH3_SHDW,
		REG_NPROTCFG_SHDW,
		REG_NDELAYCFG_SHDW,
		REG_NODSCTH_SHDW,
		REG_NCHGCFG2_SHDW,
	};
	u16 max17333_debug_reg[16] = {
		REG_STATUS,
		REG_ATRATE,
		REG_CONFIG,
		REG_VCELL,
		REG_CURRENT,
		REG_AVGCURRENT,
		REG_CHARGING_CURRENT,
		REG_CHARGING_VOLTAGE,
		REG_CHGSTAT,
		REG_CONFIG2,
		REG_PROTALRTS,
		REG_STATUS2,
		REG_PROTSTATUS,
		REG_PCKP,
		REG_HCHGTARG,
		REG_HCHGPWM,
	};

	u16 max17333_debug_hw_reg[2] = {
		REG_HPROTCFG,
		REG_HPROTCFG2
	};

	str = kzalloc(sizeof(char) * 4096, GFP_KERNEL);
	if (!str)
		return;

	/* NV registers */
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
#endif

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

#define MAX17333_RECOVERY_COUNT	3
static int max17333_recovery_allowchg(struct max17333_charger_data *charger)
{
	u16 config_val;
	int i, ret = 0;

	for (i = 0; i < MAX17333_RECOVERY_COUNT; ++i) {
		ret = max17333_read(charger->regmap_shdw, REG_I2CCMD_SHDW, &config_val);
		if (ret < 0) {
			pr_err("%s[%s]: fail to read REG_I2CCMD_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
			return ret;
		}

		pr_info("%s[%s] : Charging is allowed (Enter:%04xh)\n", __func__, max17333_battery_type[charger->battery_type], config_val);
		config_val &= ~(MAX17333_I2CCMD_BLOCKCHG | MAX17333_I2CCMD_CHGDONE);

		ret = max17333_write(charger->regmap_shdw, REG_I2CCMD_SHDW, config_val);
		if (ret < 0) {
			pr_err("%s[%s]: fail to write REG_I2CCMD_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
			return ret;
		}

		ret = max17333_read(charger->regmap_shdw, REG_I2CCMD_SHDW, &config_val);
		if (ret < 0) {
			pr_err("%s[%s]: fail to read REG_I2CCMD_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
			return ret;
		}

		pr_info("%s[%s] : Charging is allowed (Exit:%04xh)\n", __func__, max17333_battery_type[charger->battery_type], config_val);

		if ((config_val & MAX17333_I2CCMD_CHGDONE) ||
			(config_val & MAX17333_I2CCMD_BLOCKCHG)) {
			pr_info("%s[%s] : REG_I2CCMD_SHDW has wrong value\n", __func__, max17333_battery_type[charger->battery_type]);
			msleep(50);
			ret = -EAGAIN;
		} else {
			pr_info("%s[%s] : REG_I2CCMD_SHDW is recovered(%d)\n", __func__, max17333_battery_type[charger->battery_type], i + 1);
			ret = 0;
			break;
		}
	}

	return ret;
}

static int max17333_recovery_blockchg(struct max17333_charger_data *charger)
{
	u16 config_val;
	int i, ret = 0;

	for (i = 0; i < MAX17333_RECOVERY_COUNT; ++i) {
		ret = max17333_read(charger->regmap_shdw, REG_I2CCMD_SHDW, &config_val);
		if (ret < 0) {
			pr_err("%s[%s]: fail to read REG_I2CCMD_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
			return ret;
		}

		pr_info("%s[%s] : Charging is blocked (Enter:%04xh)\n", __func__, max17333_battery_type[charger->battery_type], config_val);
		config_val |= (MAX17333_I2CCMD_BLOCKCHG | MAX17333_I2CCMD_CHGDONE);

		ret = max17333_write(charger->regmap_shdw, REG_I2CCMD_SHDW, config_val);
		if (ret < 0) {
			pr_err("%s[%s]: fail to write REG_I2CCMD_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
			return ret;
		}

		ret = max17333_read(charger->regmap_shdw, REG_I2CCMD_SHDW, &config_val);
		if (ret < 0) {
			pr_err("%s[%s]: fail to read REG_I2CCMD_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
			return ret;
		}

		pr_info("%s[%s] : Charging is blocked (Exit:%04xh)\n", __func__, max17333_battery_type[charger->battery_type], config_val);

		if (!(config_val & MAX17333_I2CCMD_CHGDONE) ||
			!(config_val & MAX17333_I2CCMD_BLOCKCHG)) {
			pr_info("%s[%s] : REG_I2CCMD_SHDW has wrong value\n", __func__, max17333_battery_type[charger->battery_type]);
			msleep(50);
			ret = -EAGAIN;
		} else {
			pr_info("%s[%s] : REG_I2CCMD_SHDW is recovered(%d)\n", __func__, max17333_battery_type[charger->battery_type], i + 1);
			ret = 0;
			break;
		}
	}

	return ret;
}

static int max17333_set_allowchg(struct max17333_charger_data *charger)
{
	u16 i2ccmd_val, nchgctrl2_val;
	int ret;

	mutex_lock(&charger->lock);
	charger->chgen = CHG_ENALBE;
	mutex_unlock(&charger->lock);

	ret = max17333_read(charger->regmap_shdw, REG_I2CCMD_SHDW, &i2ccmd_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_I2CCMD_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	pr_info("%s[%s] : Charging is allowed (Enter:%04xh)\n", __func__, max17333_battery_type[charger->battery_type], i2ccmd_val);
	i2ccmd_val &= ~(MAX17333_I2CCMD_BLOCKCHG | MAX17333_I2CCMD_CHGDONE);

	ret = max17333_write(charger->regmap_shdw, REG_I2CCMD_SHDW, i2ccmd_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to write REG_I2CCMD_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	ret = max17333_read(charger->regmap_shdw, REG_I2CCMD_SHDW, &i2ccmd_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_I2CCMD_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	pr_info("%s[%s] : Charging is allowed (Exit:%04xh)\n", __func__, max17333_battery_type[charger->battery_type], i2ccmd_val);

	if ((i2ccmd_val & MAX17333_I2CCMD_CHGDONE) ||
		(i2ccmd_val & MAX17333_I2CCMD_BLOCKCHG)) {
		pr_info("%s[%s] : REG_I2CCMD_SHDW has wrong value\n", __func__, max17333_battery_type[charger->battery_type]);
		ret = max17333_recovery_allowchg(charger);
#if IS_ENABLED(CONFIG_SEC_ABC)
		if (ret)
			sec_abc_send_event("MODULE=battery@WARN=lim_i2c_fail");
#endif
	}

	ret = max17333_read(charger->regmap_shdw, REG_NCHGCTRL2_SHDW, &nchgctrl2_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_NCHGCTRL2_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	nchgctrl2_val &= ~(MAX17333_NCHGCTRL2_DISAUTOCHG);

	ret = max17333_write(charger->regmap_shdw, REG_NCHGCTRL2_SHDW, nchgctrl2_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to write REG_NCHGCTRL2_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	ret = max17333_read(charger->regmap_shdw, REG_NCHGCTRL2_SHDW, &nchgctrl2_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_NCHGCTRL2_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	pr_info("%s[%s] : Disable Auto Charging (%04xh)\n", __func__, max17333_battery_type[charger->battery_type], nchgctrl2_val);

	return ret;
}

static int max17333_set_blockchg(struct max17333_charger_data *charger)
{
	u16 i2ccmd_val, nchgctrl2_val;
	int ret;

	mutex_lock(&charger->lock);
	charger->chgen = CHG_DISABLE;
	mutex_unlock(&charger->lock);

	ret = max17333_read(charger->regmap_shdw, REG_NCHGCTRL2_SHDW, &nchgctrl2_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_NCHGCTRL2_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	nchgctrl2_val |= (MAX17333_NCHGCTRL2_DISAUTOCHG);

	ret = max17333_write(charger->regmap_shdw, REG_NCHGCTRL2_SHDW, nchgctrl2_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to write REG_NCHGCTRL2_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	ret = max17333_read(charger->regmap_shdw, REG_NCHGCTRL2_SHDW, &nchgctrl2_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_NCHGCTRL2_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	pr_info("%s[%s] : Disable Auto Charging (%04xh)\n", __func__, max17333_battery_type[charger->battery_type], nchgctrl2_val);

	ret = max17333_read(charger->regmap_shdw, REG_I2CCMD_SHDW, &i2ccmd_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_I2CCMD_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	pr_info("%s[%s] : Charging is blocked (Enter:%04xh)\n", __func__, max17333_battery_type[charger->battery_type], i2ccmd_val);
	i2ccmd_val |= (MAX17333_I2CCMD_BLOCKCHG | MAX17333_I2CCMD_CHGDONE);

	ret = max17333_write(charger->regmap_shdw, REG_I2CCMD_SHDW, i2ccmd_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to write REG_I2CCMD_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	ret = max17333_read(charger->regmap_shdw, REG_I2CCMD_SHDW, &i2ccmd_val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_I2CCMD_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	pr_info("%s[%s] : Charging is blocked (Exit:%04xh)\n", __func__, max17333_battery_type[charger->battery_type], i2ccmd_val);

	if (!(i2ccmd_val & MAX17333_I2CCMD_CHGDONE) ||
		!(i2ccmd_val & MAX17333_I2CCMD_BLOCKCHG)) {
		pr_info("%s[%s] : REG_I2CCMD_SHDW has wrong value\n", __func__, max17333_battery_type[charger->battery_type]);
		ret = max17333_recovery_blockchg(charger);
#if IS_ENABLED(CONFIG_SEC_ABC)
		if (ret)
			sec_abc_send_event("MODULE=battery@WARN=lim_i2c_fail");
#endif
	}

	return ret;
}

#ifdef CONFIG_IFPMIC_LIMITER
#define OCMARGIN0_THRES_CUR 280
#endif
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

#ifdef CONFIG_IFPMIC_LIMITER
	if (curr <= OCMARGIN0_THRES_CUR)
		max17333_update_bits(charger->regmap_shdw, REG_NODSCTH_SHDW, MAX17333_NODSCTH_OCMARGIN, (0<<MAX17333_NODSCTH_OCMARGIN_SHIFT));
	else
		max17333_update_bits(charger->regmap_shdw, REG_NODSCTH_SHDW, MAX17333_NODSCTH_OCMARGIN, (2<<MAX17333_NODSCTH_OCMARGIN_SHIFT));
	max17333_read(charger->regmap_shdw, REG_NODSCTH_SHDW, &current_val);
	pr_info("%s[%s] : ocmargin : %04xh\n", __func__, max17333_battery_type[charger->battery_type], current_val);
#endif

	return ret;
}

static int max17333_get_chg_current(struct max17333_charger_data *charger)
{
	u16 val = 0;
	int ret = 0, curr = 0;

	ret = max17333_read(charger->regmap, REG_CHARGING_CURRENT, &val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_CHARGING_CURRENT\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	curr = max17333_chgdata_to_raw_current(charger, val);

	return curr;
}

static bool max17333_get_dnr_status(struct max17333_charger_data *charger)
{
	u16 read_data = 0, read_data2 = 0;
	int ret = 0;
	bool dnr_status = false;

	ret = max17333_read(charger->regmap, REG_FSTAT, &read_data);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_FSTAT\n", __func__, max17333_battery_type[charger->battery_type]);
		return dnr_status;
	}
	read_data = (read_data & 0x0001);

	ret = max17333_read(charger->regmap, REG_HCONFIG, &read_data2);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_HCONFIG\n", __func__, max17333_battery_type[charger->battery_type]);
		return dnr_status;
	}
	read_data2 = (read_data2 & 0x4000);

	if (read_data | read_data2) {
		/* Data is Ready, this value is only avaliable when BIT0 of REG_FSTAT is 0 and BIT14 of REG_HCONFIG is 0. */
		/* Otherwise Data is not Ready */
		pr_err("%s[%s]: Data is not Ready\n", __func__, max17333_battery_type[charger->battery_type]);
		dnr_status = true;
//#if !defined(CONFIG_SEC_FACTORY)
		//panic("ma17333 data is not ready");
//#endif
	}

	return dnr_status;
}


static ssize_t max17333_get_chg_current_sysfs(struct max17333_charger_data *charger, char *buf)
{
	int rc = 0, curr = 0;

	curr = max17333_get_chg_current(charger);

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

#if 0
static int max17333_set_ovp_voltage_delta(struct max17333_charger_data *charger, unsigned int vdelta)
{
	int ret;
	u16 novprtth, val;

	if (vdelta == 0) {
		val = 0;
	} else {
		val = vdelta / 5;
	}

	ret = max17333_read(charger->regmap_shdw, REG_NOVPRTTH_SHDW, &novprtth);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_NOVPRTTH_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	novprtth &= ~MAX17333_NOVPRTTH_DOVP;
	novprtth = (novprtth | ((val << 6) & MAX17333_NOVPRTTH_DOVP));

	pr_err("%s[%s]: Overvoltage protection voltage delta(%d)mV\n", __func__, max17333_battery_type[charger->battery_type], vdelta);
	
	ret = max17333_write(charger->regmap_shdw, REG_NOVPRTTH_SHDW, novprtth);
	if (ret < 0) {
		pr_err("%s[%s]: fail to write REG_NOVPRTTH_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}
	
	return ret;
}
#endif

static int max17333_get_ovp_voltage_delta(struct max17333_charger_data *charger)
{
	int ret;
	int val = 0;
	u16 novprtth = 0;
	
	ret = max17333_read(charger->regmap_shdw, REG_NOVPRTTH_SHDW, &novprtth);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_NOVPRTTH_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	val = (novprtth >> 6) & 0x001f;
	val = val * 5;

	return val;
}

static int max17333_set_room_chg_voltage(struct max17333_charger_data *charger, unsigned int voltage)
{
	u16 ndesigncap, nvchgcfg1;
	int voltage_val = 0, center_voltage = 0;
	int vstep = 0;
	int ret;
	
	ret = max17333_read(charger->regmap_shdw, REG_NDESIGNCAP_SHDW, &ndesigncap);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_NCHGCFG0_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	ret = max17333_read(charger->regmap_shdw, REG_NVCHGCFG1_SHDW, &nvchgcfg1);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_NVCHGCFG1_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	if ((ndesigncap & MAX17333_NDESIGNCAP_VSCALE) == MAX17333_NDESIGNCAP_VSCALE) {
		/* nDesignCap.VScale = 1 --> CenterVoltage = 3.7V, StepSize = 10mV */
		center_voltage = 3700; 
		vstep = 10;
	} else {
		/* nDesignCap.VScale = 1 --> CenterVoltage = 3.7V, StepSize = 10mV */
		center_voltage = 4200; 
		vstep = 5;
	}

	if (voltage < center_voltage) {
		pr_err("%s[%s]: input is smaller than the center voltage(%d,%d)mV\n",
				__func__, max17333_battery_type[charger->battery_type], voltage, center_voltage);
		return -1;
	} else if (voltage == center_voltage) {
		voltage_val = 0;
	} else {
		voltage_val = (voltage - center_voltage) / vstep;
	}
	
	nvchgcfg1 &= ~MAX17333_NVCHGCFG1_ROOMCHGVOL;
	nvchgcfg1 = (nvchgcfg1 | ((voltage_val << 4) & MAX17333_NVCHGCFG1_ROOMCHGVOL));

	pr_err("%s[%s]: room charge voltage(%d)mV\n", __func__, max17333_battery_type[charger->battery_type], voltage);

	
	ret = max17333_write(charger->regmap_shdw, REG_NVCHGCFG1_SHDW, nvchgcfg1);
	if (ret < 0) {
		pr_err("%s[%s]: fail to write REG_NVCHGCFG1_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}
	
	return ret;
}

static int max17333_set_ovp_voltage(struct max17333_charger_data *charger, unsigned int voltage)
{
	int voltage_delta, voltage_room;
	int ret;

	pr_info("%s[%s] : set ovp protection volt(%d)mV\n", __func__, max17333_battery_type[charger->battery_type], voltage);
	
	voltage_delta = max17333_get_ovp_voltage_delta(charger);

	voltage_room = voltage - voltage_delta;

	pr_info("%s[%s] : set ovp protection voltage_room voltage %d(%d+%d)mV\n", __func__, max17333_battery_type[charger->battery_type], voltage, voltage_room, voltage_delta);

	ret = max17333_set_room_chg_voltage(charger, voltage_room);
	if (ret < 0) {
		pr_err("%s[%s]: fail to set room charging voltage\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	return ret;
}

static ssize_t max17333_get_ovp_voltage(struct max17333_charger_data *charger, char *buf)
{
	int ret, rc = 0;

	u16 ndesigncap, nvchgcfg1;
	int voltage_delta = 0, center_voltage = 0, room_voltage = 0;
	int vstep = 0;

	ret = max17333_read(charger->regmap_shdw, REG_NDESIGNCAP_SHDW, &ndesigncap);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_NCHGCFG0_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	ret = max17333_read(charger->regmap_shdw, REG_NVCHGCFG1_SHDW, &nvchgcfg1);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_NVCHGCFG1_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
		return ret;
	}

	if ((ndesigncap & MAX17333_NDESIGNCAP_VSCALE) == MAX17333_NDESIGNCAP_VSCALE) {
		/* nDesignCap.VScale = 1 --> CenterVoltage = 3.7V, StepSize = 10mV */
		center_voltage = 3700; 
		vstep = 10;
	} else {
		/* nDesignCap.VScale = 1 --> CenterVoltage = 3.7V, StepSize = 10mV */
		center_voltage = 4200; 
		vstep = 5;
	}

	nvchgcfg1 = (nvchgcfg1 >> 4) & 0x00ff;
	room_voltage = center_voltage + (nvchgcfg1 * vstep);
	
	voltage_delta = max17333_get_ovp_voltage_delta(charger);

	rc += snprintf(buf + rc, PAGE_SIZE - rc, "VOLTAGE : %d(%d+%d) mV\n", room_voltage + voltage_delta, room_voltage, voltage_delta);

	return rc;
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

	ret = max17333_read(charger->regmap, REG_CURRENT, &reg);
	if (ret < 0)
		return 0;

	inow = max17333_raw_current_to_uamps(charger, sign_extend32(reg, 15));
	if (mode == SEC_BATTERY_CURRENT_MA)
		inow = inow / 1000;

	return inow;
}

static int max17333_get_iavg(struct max17333_charger_data *charger, u8 mode)
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

	ret = max17333_read(charger->regmap, REG_VCELL, &reg);
	if (ret < 0)
		return ret;
	vcell = max17333_raw_voltage_to_uvolts(reg);
	if (mode == SEC_BATTERY_VOLTAGE_MV)
		vcell = vcell / 1000;

	return vcell;
}

static int max17333_get_bat_avg_vcell(struct max17333_charger_data *charger, u8 mode)
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

static int max17333_get_pckp(struct max17333_charger_data *charger)
{
	int ret = 0, cnvt_val = 0;
	u16 val = 0;

	ret = max17333_read(charger->regmap, REG_PCKP, &val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_PCKP (%d)\n", __func__, max17333_battery_type[charger->battery_type], ret);
		return ret;
	}
	cnvt_val = max17333_raw_pckp_to_uvolts(val) / 1000;

	return cnvt_val;
}

static int max17333_get_float_voltage(struct max17333_charger_data *charger)
{
	u16 val;
	int ret, volt = 0;

	ret = max17333_read(charger->regmap, REG_CHARGING_VOLTAGE, &val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to read REG_CHARGING_VOLTAGE\n", __func__, max17333_battery_type[charger->battery_type]);
		return volt;
	}

	volt = max17333_raw_voltage_to_uvolts(val);
	volt /= 1000;

	return volt;
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

static u8 max17333_get_autoship_thr(struct max17333_charger_data *charger, bool en)
{
	u8 reg_data[] = {VSHUTDOWNTHR_3V, VSHUTDOWNTHR_3_4V, VSHUTDOWNTHR_3_6V, VSHUTDOWNTHR_4V}; /* 3V, 3.4V, 3.6V, 4V */
	int vnow = 0, inow = 0, reg_data_idx = 0, offset_by_inow = 0;
	int ari_cond = charger->spcom ? 91 : 0;

	/* Not delivered ari cnt or under 90, set 3v auto ship mode
	 * no dts, but if ari cnt write,
	 * it is judged to be abnormal and set 3v auto ship mode
	 */
	if (charger->ari_cnt < ari_cond || !en)
		return VSHUTDOWNTHR_3V;

	vnow = max17333_get_vcell(charger, SEC_BATTERY_VOLTAGE_MV);
	inow = max17333_get_inow(charger, SEC_BATTERY_CURRENT_MA);

	if (inow < 100) // discharging case
		offset_by_inow = 0;
	else if (inow >= 100 && inow < 3000)
		offset_by_inow = 1;
	else
		offset_by_inow = 2;

	if (vnow >= 4200)
		reg_data_idx = 3;
	else if (vnow >= 3900)
		reg_data_idx = 2;
	else if (vnow >= 3700)
		reg_data_idx = 1;
	else
		reg_data_idx = 0;

	reg_data_idx =
		reg_data_idx - offset_by_inow < 0 ? 0 : reg_data_idx - offset_by_inow;

	pr_info("%s: reg_data_idx : %d, reg_data : (0x%x)\n", __func__,
		reg_data_idx, reg_data[reg_data_idx]);

	return reg_data[reg_data_idx];
}

static void max17333_autoship_mode_enable(struct max17333_charger_data *charger, bool en)
{
	u16 rval = 0;
	u8 autoship_thr_data = 0;
	int ret = 0;

	/* set autoship threshold */
	autoship_thr_data = max17333_get_autoship_thr(charger, en);

	ret = max17333_read(charger->regmap_shdw, REG_NTPRTTH3_SHDW, &rval);
	if (ret < 0) {
		dev_err(charger->dev, "[%s]: fail to read REG_NTPRTTH3_SHDW\n", max17333_battery_type[charger->battery_type]);
		return;
	}

	rval &= 0xF1FF;
	rval |= ((autoship_thr_data << 9) | 0x3); // maximum ShipEntryTimer
	ret = max17333_write(charger->regmap_shdw, REG_NTPRTTH3_SHDW, rval);
	if (ret < 0) {
		dev_err(charger->dev, "[%s]: fail to write REG_NTPRTTH3_SHDW\n", max17333_battery_type[charger->battery_type]);
		return;
	}
	pr_info("%s: NTPRTTH3 : (0x%x)\n", __func__, rval);

	/* set autoship */
	ret = max17333_read(charger->regmap_shdw, REG_I2CCMD_SHDW, &rval);
	if (ret < 0) {
		dev_err(charger->dev, "[%s]: fail to read REG_I2CCMD_SHDW\n", max17333_battery_type[charger->battery_type]);
		return;
	}

	if (en)
		rval |= MAX17333_I2CCMD_AUTOSHIP;
	else
		rval &= ~MAX17333_I2CCMD_AUTOSHIP;

	ret = max17333_write(charger->regmap_shdw, REG_I2CCMD_SHDW, rval);
	if (ret < 0) {
		dev_err(charger->dev, "[%s]: fail to write REG_I2CCMD_SHDW\n", max17333_battery_type[charger->battery_type]);
		return;
	}
	pr_info("%s: I2CCMD : (0x%x)\n", __func__, rval);

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

	pr_info("%s[%s]\n", __func__, max17333_battery_type[charger->battery_type]);

	switch (offset) {
	case MAX17333_CHG_CURRENT:
		i = max17333_get_chg_current_sysfs(charger, buf);
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
	case MAX17333_CHG_PCKP:
		i = snprintf(buf, PAGE_SIZE, "%d\n", max17333_get_pckp(charger));
		break;
	case MAX17333_OVP_VOLTAGE:
		i = max17333_get_ovp_voltage(charger, buf);
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

	pr_info("%s[%s]\n", __func__, max17333_battery_type[charger->battery_type]);

	switch (offset) {
	case MAX17333_CHG_CURRENT:
		if (sscanf(buf, "%5d\n", &x) == 1) {
			pr_info("%s[%s] current = %d\n", __func__, max17333_battery_type[charger->battery_type], x);
			max17333_set_chg_current(charger, x);
		}
		ret = count;
		break;
	case MAX17333_CHG_EN:
		if (sscanf(buf, "%5d\n", &x) == 1) {
			pr_info("%s[%s] chg en = %d\n", __func__, max17333_battery_type[charger->battery_type], x);
			max17333_set_chg_enable(charger, x);
		}
		ret = count;
		break;
	case MAX17333_CHG_VOLTAGE:
		if (sscanf(buf, "%5d\n", &x) == 1) {
			pr_info("%s[%s] voltage = %d\n", __func__, max17333_battery_type[charger->battery_type], x);
			max17333_set_chg_voltage(charger, x);
		}
		ret = count;
		break;
	case MAX17333_PRECHG_CURRENT:
		if (sscanf(buf, "%5d\n", &x) == 1) {
			pr_info("%s[%s] prechg current = %d\n", __func__, max17333_battery_type[charger->battery_type], x);
			max17333_set_prechg_current(charger, x);
		}
		ret = count;
		break;
	case MAX17333_PREQUAL_VOLTAGE:
		if (sscanf(buf, "%5d\n", &x) == 1) {
			pr_info("%s[%s] prequal voltage = %d\n", __func__, max17333_battery_type[charger->battery_type], x);
			max17333_set_prequal_voltage(charger, x);
		}
		ret = count;
		break;
	case MAX17333_SHIP_EN:
		if (sscanf(buf, "%5d\n", &x) == 1) {
			pr_info("%s[%s] ship en = %d\n", __func__, max17333_battery_type[charger->battery_type], x);
			max17333_set_ship_enable(charger, x);
		}
		ret = count;
		break;
	case MAX17333_COMMSH_EN:
		if (sscanf(buf, "%5d\n", &x) == 1) {
			pr_info("%s : [%s] commsh en = %d\n", __func__, max17333_battery_type[charger->battery_type], x);
			max17333_set_commsh_enable(charger, x);
		}
		ret = count;
		break;
	case MAX17333_OVP_VOLTAGE:
		if (sscanf(buf, "%5d\n", &x) == 1) {
			pr_info("%s : [%s] voltage = %d\n", __func__, max17333_battery_type[charger->battery_type], x);
			max17333_set_ovp_voltage(charger, x);
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
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = max17333_get_iavg(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = max17333_get_inow(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		val->intval = max17333_get_bat_avg_vcell(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = max17333_get_vcell(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		val->intval = max17333_get_float_voltage(charger);
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_LIMITER_SHIPMODE:
			val->intval = max17333_get_commsh_enable(charger);
			break;
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			max17333_periodic_dump(charger);
			max17333_get_dnr_status(charger);
			max17333_get_ovp_status(charger);
			break;
		case POWER_SUPPLY_EXT_PROP_ARI_CNT:
			val->intval = charger->ari_cnt;
			break;
		case POWER_SUPPLY_EXT_PROP_FGSRC_SWITCHING:
			break;
		case POWER_SUPPLY_EXT_PROP_CHG_VOLTAGE:
			val->intval = max17333_get_pckp(charger);
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
		charger->chg_vol = val->intval;
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
			charger->chg_curr = val->intval;
			max17333_set_chg_current(charger, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_LIMITER_SHIPMODE:
			max17333_set_commsh_enable(charger, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_ARI_CNT:
			if (charger->spcom) {
				charger->ari_cnt = val->intval;
				pr_info("%s%s: ari cnt:%d\n",
						__func__, max17333_battery_type[charger->battery_type], charger->ari_cnt);
			} else {
				charger->ari_cnt = -1;
				pr_info("%s%s: not support ari cnt: %d\n",
					__func__, max17333_battery_type[charger->battery_type], val->intval);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_FGSRC_SWITCHING:
			{
				u8 reg_fgsrc = 0;
				u16 nchgctrl_pckp, i2ccmd_pckp, commstat;
				/* Unlock */
				max17333_read(charger->regmap, REG_COMMSTAT, &commstat);
				max17333_write(charger->regmap, REG_COMMSTAT, commstat & 0xFF06);
				max17333_write(charger->regmap, REG_COMMSTAT, commstat & 0xFF06);

				max17333_read(charger->regmap_shdw, REG_NCHGCTRL2_SHDW, &nchgctrl_pckp);
				max17333_read(charger->regmap_shdw, REG_I2CCMD_SHDW, &i2ccmd_pckp);

				if ((val->intval == SEC_BAT_INBAT_FGSRC_SWITCHING_VSYS) ||
					(val->intval == SEC_BAT_FGSRC_SWITCHING_VSYS)) {
					nchgctrl_pckp |= MAX17333_NCHGCTRL2_MG_PCKP;
					i2ccmd_pckp |= MAX17333_I2CCMD_MG_PCKP;
					reg_fgsrc = 1;
				} else {
					nchgctrl_pckp &= ~MAX17333_NCHGCTRL2_MG_PCKP;
					i2ccmd_pckp &= ~MAX17333_I2CCMD_MG_PCKP;
					reg_fgsrc = 0;
				}
				max17333_write(charger->regmap_shdw, REG_NCHGCTRL2_SHDW, nchgctrl_pckp);
				max17333_write(charger->regmap_shdw, REG_I2CCMD_SHDW, i2ccmd_pckp);

				pr_info("%s: POWER_SUPPLY_EXT_PROP_FGSRC_SWITCHING(%d): reg(0x%x) val(0x%x)\n",
					__func__, reg_fgsrc, REG_I2CCMD_SHDW, i2ccmd_pckp);
			}		
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

static bool max17333_get_ovp_status(struct max17333_charger_data *charger)
{
	int ret;
	u16 val;

	ret = max17333_read(charger->regmap, REG_PROTSTATUS, &val);
	if (ret < 0) {
		dev_err(charger->dev, "[%s]: fail to read REG_PROTSTATUS\n", max17333_battery_type[charger->battery_type]);
		return false;
	}

	if (val & BIT_OVP_INT) {
		/* Overvoltage event occurs */
		pr_info("%s[%s] Overvoltage!\n", __func__, max17333_battery_type[charger->battery_type]);
		return true;
	}

	/* No Overvoltage event occurs */
	return false;
}

#ifdef USE_STATUS_ALERT
static void max17333_patchallow_check(struct max17333_charger_data *charger)
{
	int ret;
	u16 patchallow;

	ret = max17333_read(charger->regmap_shdw, REG_PATCH_ALLOW, &patchallow);
	if (ret < 0) {
		dev_err(charger->dev, "[%s]: fail to read REG_PATCH_ALLOW\n", max17333_battery_type[charger->battery_type]);
		return;
	}

	pr_info("%s [%s] : 0x%x\n", __func__, max17333_battery_type[charger->battery_type], patchallow);

	patchallow = (patchallow & 0x000C);

	if (patchallow == 0x000C) {
		msleep(375);
	}
}

static void max17333_oci_clear(struct max17333_charger_data *charger)
{
	int ret;
	u16 commstat, odsccfg, odscth2;

	ret = max17333_read(charger->regmap, REG_ODSCCFG, &odsccfg);
	if (ret < 0)
		return;

	/* Unlock */
	max17333_read(charger->regmap, REG_COMMSTAT, &commstat);
	if (ret < 0)
		return;

	max17333_write(charger->regmap, REG_COMMSTAT, commstat & 0xFF06);
	max17333_write(charger->regmap, REG_COMMSTAT, commstat & 0xFF06);

	/* Enter Test Mode */
	ret = max17333_write(charger->regmap, REG_TESTLOCK1, 0x8700);
	if (ret < 0)
		return;

	ret = max17333_write(charger->regmap, REG_TESTLOCK2, 0xD200);
	if (ret < 0)
		return;

	ret = max17333_write(charger->regmap, REG_TESTLOCK3, 0xC27E);
	if (ret < 0)
		return;

	/* Check Patch Allow */
	max17333_patchallow_check(charger);

	/* POK = 0 & Halt Firmware */
	ret = max17333_write(charger->regmap, REG_TESTCTRL, 0x0400);
	if (ret < 0)
		return;

	/* Update CCTH to -3 */
	ret = max17333_read(charger->regmap, REG_ODSCTH2, &odscth2);
	if (ret < 0)
		return;

	odscth2 &= ~MAX17333_ODSCTH2_CCTH;

	ret = max17333_write(charger->regmap,  REG_ODSCTH2, odscth2 | (0x7C << 9));
	if (ret < 0)
		return;

	/* Clear ODSCCfg.OCi bit */
	odsccfg &= ~MAX17333_ODSCCFG_OCI;

	ret = max17333_write(charger->regmap, REG_ODSCCFG, odsccfg);
	if (ret < 0)
		return;

	/* Clear Sleep */
	ret = max17333_write(charger->regmap,  REG_SLEEP, 0x0000);
	if (ret < 0)
		return;

	/* Exit Test Mode */
	ret = max17333_write(charger->regmap, REG_TESTLOCK3, 0x0000);
	if (ret < 0)
		return;

	pr_info("%s : [%s] clear OCI bit\n", __func__, max17333_battery_type[charger->battery_type]);
	oci_cnt++;
	pr_info("%s : [%s] oci_cnt = %d\n", __func__, max17333_battery_type[charger->battery_type], oci_cnt);

	return;
}

static void max17333_prot_work(struct work_struct *work)
{
	struct max17333_charger_data *charger = container_of(work, struct max17333_charger_data, prot_work.work);
//	int ret, i;
	int ret;
	u16 status, protalrts, protstatus, odsccfg, i2ccmd;
	union power_supply_propval value = {0, };

	/* Check alert type */
	ret = max17333_read(charger->regmap, REG_STATUS, &status);
	if (ret < 0) {
		dev_err(charger->dev, "[%s]: fail to read REG_STATUS\n", max17333_battery_type[charger->battery_type]);
		__pm_relax(charger->protalrt_wake_lock);
		return;
	}

	pr_info("%s : [%s]STATUS (%04xh) Protection Interrupt thread!\n", __func__, max17333_battery_type[charger->battery_type], status);

	/* Check Protection Alert type */
	ret = max17333_read(charger->regmap, REG_PROTALRTS, &protalrts);
	if (ret < 0) {
		dev_err(charger->dev, "[%s]: fail to read REG_PROTALRTS\n", max17333_battery_type[charger->battery_type]);
		__pm_relax(charger->protalrt_wake_lock);
		return;
	}
	ret = max17333_read(charger->regmap, REG_PROTSTATUS, &protstatus);
	if (ret < 0) {
		dev_err(charger->dev, "[%s]: fail to read REG_PROTSTATUS\n", max17333_battery_type[charger->battery_type]);
		__pm_relax(charger->protalrt_wake_lock);
		return;
	}

	if (protalrts & BIT_CHGWDT_INT)
		pr_info("%s : [%s] Protection Alert: Charge Watch Dog Timer!\n", __func__, max17333_battery_type[charger->battery_type]);

	if (protalrts & BIT_TOOHOTC_INT)
		pr_info("%s : [%s] Protection Alert: Overtemperature for Charging!\n", __func__, max17333_battery_type[charger->battery_type]);

	if (protalrts & BIT_FULL_INT)
		pr_info("%s : [%s] Protection Alert: Full Detection!\n", __func__, max17333_battery_type[charger->battery_type]);

	if (protalrts & BIT_TOOCOLDC_INT)
		pr_info("%s : [%s] Protection Alert: Undertemperature!\n", __func__, max17333_battery_type[charger->battery_type]);

	if (protalrts & BIT_OVP_INT) {
		pr_info("%s : [%s] Protection Alert: Overvoltage!\n", __func__, max17333_battery_type[charger->battery_type]);
		value.intval = POWER_SUPPLY_EXT_HEALTH_VBAT_OVP;
		psy_do_property("battery", set, POWER_SUPPLY_PROP_HEALTH, value);
	}

	if (protalrts & BIT_QOVFLW_INT)
		pr_info("%s : [%s] Protection Alert: Q Overflow!\n", __func__, max17333_battery_type[charger->battery_type]);

	if (protalrts & BIT_DIEHOT_INT)
		pr_info("%s : [%s] Protection Alert: Overtemperature for die temperature!\n", __func__, max17333_battery_type[charger->battery_type]);

	if (protalrts & BIT_TOOHOTD_INT)
		pr_info("%s : [%s] Protection Alert: Overtemperature for Discharging!\n", __func__, max17333_battery_type[charger->battery_type]);

	if (protalrts & BIT_UVP_INT)
		pr_info("%s : [%s] Protection Alert: Undervoltage Protection!\n", __func__, max17333_battery_type[charger->battery_type]);

	if (protalrts & BIT_ODCP_INT)
		pr_info("%s : [%s] Protection Alert: Overdischarge current!\n", __func__, max17333_battery_type[charger->battery_type]);

	if (protalrts & BIT_OCCP_INT) {
		pr_info("%s : [%s] Protection Alert: Overcharge Current!\n", __func__, max17333_battery_type[charger->battery_type]);
		ret = max17333_read(charger->regmap, REG_ODSCCFG, &odsccfg);
		if (ret < 0) {
			__pm_relax(charger->protalrt_wake_lock);
			return;
		}

		pr_info("[%s] %s odsccfg = 0x%x\n", __func__, max17333_battery_type[charger->battery_type], odsccfg);
		if ((odsccfg & MAX17333_ODSCCFG_OCI) == MAX17333_ODSCCFG_OCI)
			max17333_oci_clear(charger);
		else
			pr_info("%s : [%s] OCI bit is Already Cleared\n", __func__, max17333_battery_type[charger->battery_type]);

	}

	/* Clear Prot alerts */
	ret = max17333_write(charger->regmap, REG_PROTALRTS, REG_PROTALRTS_MASK);
	if (ret < 0) {
		dev_err(charger->dev, "[%s]: fail to write REG_PROTALRTS\n", max17333_battery_type[charger->battery_type]);
	}

	/* Clear Prot status */
	ret = max17333_write(charger->regmap, REG_PROTSTATUS, REG_PROTSTATUS_MASK);
	if (ret < 0) {
		dev_err(charger->dev, "[%s]: fail to write REG_PROTSTATUS\n", max17333_battery_type[charger->battery_type]);
	}

	/* Clear status */
	ret = max17333_write(charger->regmap, REG_STATUS, REG_STATUS_MASK);
	if (ret < 0) {
		dev_err(charger->dev, "[%s]: fail to write REG_STATUS\n", max17333_battery_type[charger->battery_type]);
	}

	/* Clear Prot status */
	ret = max17333_write(charger->regmap, REG_PROTSTATUS, REG_PROTSTATUS_MASK);
	if (ret < 0) {
		dev_err(charger->dev, "[%s]: fail to write REG_PROTSTATUS\n", max17333_battery_type[charger->battery_type]);
	}
	max17333_read(charger->regmap, REG_PROTALRTS, &protalrts);
	max17333_read(charger->regmap, REG_PROTSTATUS, &protstatus);
	max17333_read(charger->regmap, REG_STATUS, &status);
	max17333_read(charger->regmap_shdw, REG_NI2CCFG_SHDW, &i2ccmd);
	pr_info("[MAX17333][%s] Status: %04xh,%04xh,%04xh,%04xh\n", max17333_battery_type[charger->battery_type],
		protalrts, protstatus, status, i2ccmd);

	__pm_relax(charger->protalrt_wake_lock);
}

static irqreturn_t max17333_protalrt_irq_isr(int irq, void *data)
{
	struct max17333_charger_data *charger = data;
	int ret;
	u16 status, protalrts, odsccfg, i2ccmd;
	
	/* Check alert type */
	ret = max17333_read(charger->regmap, REG_STATUS, &status);
	if (ret < 0) {
		dev_err(charger->dev, "[%s]: fail to read REG_STATUS\n", max17333_battery_type[charger->battery_type]);
		return IRQ_HANDLED;
	}

	ret = max17333_read(charger->regmap, REG_PROTALRTS, &protalrts);
	if (ret < 0) {
		dev_err(charger->dev, "[%s]: fail to read REG_PROTALRTS\n", max17333_battery_type[charger->battery_type]);
		return IRQ_HANDLED;
	}

	if (protalrts & BIT_OCCP_INT) {
		ret = max17333_read(charger->regmap, REG_ODSCCFG, &odsccfg);
		if (ret < 0)
			return IRQ_HANDLED;
		
		if ((odsccfg & MAX17333_ODSCCFG_OCI) == MAX17333_ODSCCFG_OCI) {
			pr_info("%s : [%s]ODSCCFG (%04xh): OCI triggered!\n", __func__, max17333_battery_type[charger->battery_type], odsccfg);
			ret = max17333_read(charger->regmap_shdw, REG_I2CCMD_SHDW, &i2ccmd);
			if (ret < 0) {
				pr_err("%s[%s]: fail to read REG_I2CCMD_SHDW\n", __func__, max17333_battery_type[charger->battery_type]);
				return IRQ_HANDLED;
			}
			pr_info("%s : [%s]STATUS (%04xh): Protection Interrupt Handler! (0x%x)\n", __func__, max17333_battery_type[charger->battery_type], status, i2ccmd);

			__pm_stay_awake(charger->protalrt_wake_lock);
			queue_delayed_work(charger->prot_wqueue, &charger->prot_work, msecs_to_jiffies(0));
			return IRQ_HANDLED;
		}
	}

	pr_info("%s : [%s]STATUS (%04xh): Protection Interrupt Handler!\n", __func__, max17333_battery_type[charger->battery_type], status);
	__pm_stay_awake(charger->protalrt_wake_lock);
	queue_delayed_work(charger->prot_wqueue, &charger->prot_work, msecs_to_jiffies(2000));

	return IRQ_HANDLED;
}
#endif

static int max17333_charger_initialize(struct max17333_charger_data *charger)
{
	int ret = 0;

	/* disable auto shipmode, this should work under 3V */
	max17333_autoship_mode_enable(charger, false);

	charger->chg_vol = 4450;
	charger->chg_curr = 1000;
	ret = max17333_set_chg_voltage(charger, 4450);
	ret |= max17333_set_chg_current(charger, 1000);
#ifdef CONFIG_IFPMIC_LIMITER
	ret |= max17333_set_blockchg(charger);
#else
	ret |= max17333_set_allowchg(charger);
#endif

	return ret;
}

static int max17333_charger_probe(struct platform_device *pdev)
{
	struct max17333_dev *max17333 = dev_get_drvdata(pdev->dev.parent);
	struct max17333_charger_data *charger;
	int ret = 0;
	struct power_supply_config charger_cfg = {};
	u16 odsccfg;

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
	charger->ari_cnt = 0;
	charger->spcom = max17333->pdata->spcom;
	charger->chgen = CHG_DISABLE;

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
	charger->protalrt_wake_lock = wakeup_source_register(&pdev->dev, "max17333-protalrt-wake-lock");
	charger->prot_wqueue = create_singlethread_workqueue("max17333_prot_wqueue");	
	INIT_DELAYED_WORK(&charger->prot_work,  max17333_prot_work);

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

	/* Clear OCI Bit */
	max17333_read(charger->regmap, REG_ODSCCFG, &odsccfg);

	if ((odsccfg & MAX17333_ODSCCFG_OCI) == MAX17333_ODSCCFG_OCI)
		max17333_oci_clear(charger);

	/* Clear alerts */
	max17333_write(charger->regmap, REG_PROTALRTS, REG_PROTALRTS_MASK);
	max17333_write(charger->regmap, REG_PROTSTATUS, REG_PROTSTATUS_MASK);
	max17333_write(charger->regmap, REG_STATUS, REG_STATUS_MASK);
	max17333_write(charger->regmap, REG_PROTSTATUS, REG_PROTSTATUS_MASK);

	/* Enable alert on fuel-gauge outputs */
	max17333_update_bits(charger->regmap, REG_CONFIG, BIT_CONFIG_ALRT_EN, BIT_CONFIG_ALRT_EN);
	max17333_update_bits(charger->regmap, REG_CONFIG, BIT_CONFIG_PROTALRT_EN, BIT_CONFIG_PROTALRT_EN);
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
	if (charger->prot_irq)
		free_irq(charger->prot_irq, charger);
#ifdef USE_STATUS_ALERT
error_protalrt_irq:
	wakeup_source_unregister(charger->protalrt_wake_lock);
	power_supply_unregister(charger->psy_chg);
#endif
error:
	kfree(charger);
	return ret;
}

static int max17333_charger_remove(struct platform_device *pdev)
{
	struct max17333_charger_data *charger =
		platform_get_drvdata(pdev);

	wakeup_source_unregister(charger->protalrt_wake_lock);
	power_supply_unregister(charger->psy_chg);

	kfree(charger);

	return 0;
}

static void  max17333_charger_shutdown(struct platform_device *pdev)
{
#if !defined(CONFIG_SEC_FACTORY)
	struct max17333_charger_data *charger =
		platform_get_drvdata(pdev);

	if (!factory_mode)
		max17333_autoship_mode_enable(charger, true);
#endif
}

#if defined(CONFIG_PM)
static int max17333_charger_suspend(struct device *dev)
{
	pr_info("%s\n", __func__);

	return 0;
}

static int max17333_charger_resume(struct device *dev)
{
	pr_info("%s\n", __func__);

	return 0;
}
#endif

static const struct dev_pm_ops max17333_charger_pm_ops = {
	.suspend = max17333_charger_suspend,
	.resume = max17333_charger_resume,
};

static const struct platform_device_id max17333_charger_id[] = {
	{ "max17333-charger", 0, },
	{ }
};
MODULE_DEVICE_TABLE(platform, max17333_charger_id);

static struct platform_driver max17333_charger_driver = {
	.driver = {
		.name = "max17333-charger",
#if defined(CONFIG_PM)
		.pm = &max17333_charger_pm_ops,
#endif
	},
	.probe = max17333_charger_probe,
	.remove = max17333_charger_remove,
	.shutdown = max17333_charger_shutdown,
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
#if defined(CONFIG_PM)
		.pm = &max17333_charger_pm_ops,
#endif
	},
	.probe = max17333_charger_probe,
	.remove = max17333_charger_remove,
	.shutdown = max17333_charger_shutdown,
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
