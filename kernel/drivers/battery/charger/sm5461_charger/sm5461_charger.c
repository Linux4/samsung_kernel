/*
 * sm5461_charger.c - SM5461 Charger device driver for SAMSUNG platform
 *
 * Copyright (C) 2023 SiliconMitus Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/power_supply.h>
#if defined(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#endif /* CONFIG_OF */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#include "../../common/sec_charging_common.h"
#include "../../common/sec_direct_charger.h"
#endif
#include "sm5461_charger.h"

#define SM5461_DC_VERSION  "WK1"

static u32 SM5461_freq_val[] = {
	200, 375, 500, 750, 1000, 1250, 1500
};

static int sm5461_read_reg(struct sm5461_charger *sm5461, u8 reg, u8 *dest)
{
	int cnt, ret;

	for (cnt = 0; cnt < 3; ++cnt) {
		ret = i2c_smbus_read_byte_data(sm5461->i2c, reg);
		if (ret < 0)
			dev_err(sm5461->dev, "%s: fail to i2c_read(ret=%d)\n", __func__, ret);
		else
			break;

		if (cnt == 0)
			msleep(30);
	}

	if (ret < 0)
		return ret;

	*dest = (ret & 0xff);

	return 0;
}

int sm5461_bulk_read(struct sm5461_charger *sm5461, u8 reg, int count, u8 *buf)
{
	int cnt, ret;

	for (cnt = 0; cnt < 3; ++cnt) {
		ret = i2c_smbus_read_i2c_block_data(sm5461->i2c, reg, count, buf);
		if (ret < 0)
			dev_err(sm5461->dev, "%s: fail to i2c_bulk_read(ret=%d)\n", __func__, ret);
		else
			break;

		if (cnt == 0)
			msleep(30);
	}

	return ret;
}

static int sm5461_write_reg(struct sm5461_charger *sm5461, u8 reg, u8 value)
{
	int cnt, ret;

	for (cnt = 0; cnt < 3; ++cnt) {
		ret = i2c_smbus_write_byte_data(sm5461->i2c, reg, value);
		if (ret < 0)
			dev_err(sm5461->dev, "%s: fail to i2c_write(ret=%d)\n", __func__, ret);
		else
			break;

		if (cnt == 0)
			msleep(30);
	}

	return ret;
}

static int sm5461_update_reg(struct sm5461_charger *sm5461, u8 reg,
								u8 val, u8 mask, u8 pos)
{
	int ret;
	u8 old_val;

	mutex_lock(&sm5461->i2c_lock);

	ret = sm5461_read_reg(sm5461, reg, &old_val);
	if (ret == 0) {
		u8 new_val = (val & mask) << pos | (old_val & ~(mask << pos));

		ret = sm5461_write_reg(sm5461, reg, new_val);
	}

	mutex_unlock(&sm5461->i2c_lock);

	return ret;
}

static u8 sm5461_get_flag_status(struct sm5461_charger *sm5461, u8 flag_addr)
{
	u8 reg;

	/* read twice to get correct value */
	mutex_lock(&sm5461->i2c_lock);
	sm5461_read_reg(sm5461, flag_addr, &reg);
	sm5461_read_reg(sm5461, flag_addr, &reg);
	mutex_unlock(&sm5461->i2c_lock);

	return reg;
}

static u32 sm5461_get_vbatreg(struct sm5461_charger *sm5461)
{
	u8 reg;
	u32 vbatovp, offset;

	sm5461_read_reg(sm5461, SM5461_REG_VBAT_OVP, &reg);
	vbatovp = 4000 + ((reg & 0x7F) * 10);
	sm5461_read_reg(sm5461, SM5461_REG_REG1, &reg);
	offset = ((reg & 0x3) * 50) + 50;

	dev_info(sm5461->dev, "%s: vbatovp=%d, offset=%d\n",
		__func__, vbatovp, offset);

	return (vbatovp - offset);
}

static int sm5461_set_vbatreg(struct sm5461_charger *sm5461, u32 vbatreg)
{
	u8 reg;
	u32 vbatovp, offset;

	sm5461_read_reg(sm5461, SM5461_REG_REG1, &reg);
	offset = ((reg & 0x3) * 50) + 50;
	vbatovp = vbatreg + offset;
	if (vbatovp < 4000)
		reg = 0;
	else
		reg = (vbatovp - 4000) / 10;

	dev_info(sm5461->dev, "%s: vbatovp=%d, offset=%d reg=0x%x\n",
		__func__, vbatovp, offset, reg);

	return sm5461_update_reg(sm5461, SM5461_REG_VBAT_OVP, reg, 0x7F, 0);
}

static u32 sm5461_get_ibuslim(struct sm5461_charger *sm5461)
{
	u8 reg;
	u32 ibusocp, offset;

	sm5461_read_reg(sm5461, SM5461_REG_IBUS_PROT, &reg);
	ibusocp = 500 + ((reg & 0x3F) * 100);
	sm5461_read_reg(sm5461, SM5461_REG_CNTL5, &reg);
	offset = ((reg & 0x3) * 100) + 100;

	dev_info(sm5461->dev, "%s: ibusocp=%d, offset=%d\n",
		__func__, ibusocp, offset);

	return ibusocp - offset;
}

static int sm5461_set_ibuslim(struct sm5461_charger *sm5461, u32 ibuslim)
{
	u8 reg;
	u32 ibusocp, offset;

	sm5461_read_reg(sm5461, SM5461_REG_CNTL5, &reg);
	offset = ((reg & 0x3) * 100) + 100;
	ibusocp = ibuslim + offset;
	if (ibusocp > 4800)
		ibusocp = 4800;
	reg = (ibusocp - 500) / 100;

	dev_info(sm5461->dev, "%s: ibusocp=%d, offset=%d reg=0x%x\n",
		__func__, ibusocp, offset, reg);

	return sm5461_update_reg(sm5461, SM5461_REG_IBUS_PROT, reg, 0x3F, 0);
}

static int sm5461_set_freq(struct sm5461_charger *sm5461, u8 freq)
{
	return sm5461_update_reg(sm5461, SM5461_REG_CNTL2, freq, 0x7, 5);
}

static int sm5461_set_opid(struct sm5461_charger *sm5461, u8 opid)
{
	return sm5461_update_reg(sm5461, SM5461_REG_CNTL2, opid, 0x3, 1);
}

static int sm5461_set_op_mode(struct sm5461_charger *sm5461, u8 op_mode)
{
	return sm5461_update_reg(sm5461, SM5461_REG_CNTL1, op_mode, 0x7, 4);
}

static u8 sm5461_get_op_mode(struct sm5461_charger *sm5461)
{
	u8 reg;

	sm5461_read_reg(sm5461, SM5461_REG_CNTL1, &reg);

	return (reg >> 4) & 0x7;
}

static int sm5461_set_wdt_timer(struct sm5461_charger *sm5461, u8 tmr)
{
	return sm5461_update_reg(sm5461, SM5461_REG_CNTL1, tmr, 0x7, 0);
}

static u8 sm5461_get_wdt_timer(struct sm5461_charger *sm5461)
{
	u8 reg = 0x0;

	sm5461_read_reg(sm5461, SM5461_REG_CNTL1, &reg);

	return reg & 0x7;
}

static int sm5461_kick_wdt(struct sm5461_charger *sm5461)
{
	return sm5461_update_reg(sm5461, SM5461_REG_CNTL2, 0x1, 0x1, 0);
}

static int sm5461_set_adcmode(struct sm5461_charger *sm5461, u8 mode)
{
	return sm5461_update_reg(sm5461, SM5461_REG_ADC_CNTL, mode, 0x1, 6);
}

static int sm5461_enable_adc(struct sm5461_charger *sm5461, bool enable)
{
	return sm5461_update_reg(sm5461, SM5461_REG_ADC_CNTL, enable, 0x1, 7);
}

static int sm5461_enable_adc_oneshot(struct sm5461_charger *sm5461, u8 enable)
{
	int ret, cnt;
	u8 reg;

	if (enable) {
		for (cnt = 0; cnt < 3; ++cnt) {
			ret = sm5461_write_reg(sm5461, SM5461_REG_ADC_CNTL, 0xC0);
			/* ADC update time */
			msleep(25);
			sm5461_read_reg(sm5461, SM5461_REG_FLAG3, &reg);
			if ((reg & SM5461_FLAG3_ADCDONE) == 0)
				dev_info(sm5461->dev, "%s: fail to adc done(FLAG3=0x%02X)\n", __func__, reg);
			else
				break;
		}
	} else {
		msleep(30);
		ret = sm5461_write_reg(sm5461, SM5461_REG_ADC_CNTL, 0x0);
	}

	return ret;
}

static int sm5461_get_enadc(struct sm5461_charger *sm5461)
{
	u8 reg = 0x0;

	sm5461_read_reg(sm5461, SM5461_REG_ADC_CNTL, &reg);

	return (reg >> 7) & 0x1;
}

static int sm5461_sw_reset(struct sm5461_charger *sm5461)
{
	u8 i, reg;

	sm5461_update_reg(sm5461, SM5461_REG_CNTL1, 1, 0x1, 7);     /* Do SW Reset */

	for (i = 0; i < 0xff; ++i) {
		usleep_range(1000, 2000);
		sm5461_read_reg(sm5461, SM5461_REG_CNTL1, &reg);
		if (!((reg >> 7) & 0x1))
			break;
	}

	if (i == 0xff) {
		dev_err(sm5461->dev, "%s: didn't clear reset bit\n", __func__);
		return -EBUSY;
	}
	return 0;
}

static u8 sm5461_get_reverse_boost_ocp(struct sm5461_charger *sm5461)
{
	u8 reg, op_mode;
	u8 revbsocp = 0;

	op_mode = sm5461_get_op_mode(sm5461);
	if (op_mode == OP_MODE_INIT) {
		sm5461_read_reg(sm5461, SM5461_REG_FLAG4, &reg);
		dev_info(sm5461->dev, "%s: FLAG4=0x%x\n", __func__, reg);
		if (reg & SM5461_FLAG4_IBUSOCP_RVS) {
			revbsocp = 1;
			dev_err(sm5461->dev, "%s: detect reverse_boost_ocp\n", __func__);
		}
	}
	return revbsocp;
}

static void sm5461_init_reg_param(struct sm5461_charger *sm5461)
{
	sm5461_set_wdt_timer(sm5461, WDT_TIMER_S_40);
	sm5461_set_freq(sm5461, sm5461->pdata->freq); /* FREQ : 500kHz */
	sm5461_update_reg(sm5461, SM5461_REG_IBUS_PROT, 0x1, 0x1, 7); /* IBUSUCP = on */
	sm5461_write_reg(sm5461, SM5461_REG_VBUSOVP, 0xC6); /* VBUS_OVP = 11V */
	sm5461_write_reg(sm5461, SM5461_REG_IBAT_OCP, 0x7C); /* IBATOCP = off */
	sm5461_write_reg(sm5461, SM5461_REG_CNTL3, 0xBF); /* RLTVOVP = 1.5x, RLTVUVP = 1.01x */
	sm5461_write_reg(sm5461, SM5461_REG_CNTL4, 0x03); /* VDSQRBP = off */
	sm5461_write_reg(sm5461, SM5461_REG_CNTL5, 0x3F); /* IBUSOCP_RVS = on, threshold = 4.5A */
	sm5461_write_reg(sm5461, SM5461_REG_DIS_VL_V2P9, 0x20); /* ADC update when VL < 2.9V */
	sm5461_write_reg(sm5461, SM5461_REG_IBATOCP_DG, 0x88); /* Deglitch for IBATOCP & IBUSOCP = 8ms */
	sm5461_update_reg(sm5461, SM5461_REG_VODIV_REF_OP, 0x7, 0x7, 3); /* CN_SHRT prot val */
	sm5461_write_reg(sm5461, SM5461_REG_QRB_DG_N_SNSR, 0x0C); /* Deglitch for VDSQRB = 200ms */
	sm5461_write_reg(sm5461, SM5461_REG_ADC_CLK_SEL, 0x80); /* Digital clk ADC sampling */
	sm5461_update_reg(sm5461, SM5461_REG_M_UVPOVP_SEL, 0xF, 0xF, 4); /* OVP UVP Ref Level */
	if (sm5461->pdata->en_vbatreg)
		sm5461_write_reg(sm5461, SM5461_REG_REG1, 0x07); /* IBATREG = off, VBATREG = VBATOVP - 200mV */
	else
		sm5461_write_reg(sm5461, SM5461_REG_REG1, 0x03); /* IBATOCP & IBATREG & VBATREG = off */

	if (sm5461->pdata->x2bat_mode) {
		if (sm5461->chip_id == SM5461_SUB)
			sm5461_write_reg(sm5461, SM5461_REG_REG1, 0x07); /* IBATREG = off, VBATREG = VBATOVP - 200mV */
	} else {
		if (sm5461->chip_id == SM5461_SUB)
			sm5461_update_reg(sm5461, SM5461_REG_IBUS_PROT, 0x1, 0x1, 6); /* IBUSUCP = 250mA */
	}
}

static struct sm_dc_info *select_sm_dc_info(struct sm5461_charger *sm5461)
{
	if (sm5461->pdata->x2bat_mode)
		return sm5461->x2bat_dc;
	else
		return sm5461->pps_dc;
}

static int sm5461_get_vnow(struct sm5461_charger *sm5461)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5461);
	union power_supply_propval value = {0, };
	int ret;

	if (sm5461->pdata->en_vbatreg)
		return 0;

	value.intval = SEC_BATTERY_VOLTAGE_MV;
	ret = psy_do_property(sm5461->pdata->battery.fuelgauge_name, get,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
	if (ret < 0) {
		dev_err(sm5461->dev, "%s: cannot get vnow from fg\n", __func__);
		return -EINVAL;
	}
	pr_info("%s %s: vnow=%dmV, target_vbat=%dmV\n", sm_dc->name,
		__func__, value.intval, sm_dc->target_vbat);

	return value.intval;
}

static int sm5461_convert_adc(struct sm5461_charger *sm5461, u8 index)
{
	struct power_supply *psy_dc_sub = NULL;
	struct sm5461_charger *sm5461_sub = NULL;
	u8 regs[4] = {0x0, };
	u8 ret = 0x0;
	int adc, cnt;

#if !defined(CONFIG_SEC_FACTORY) && !IS_ENABLED(CONFIG_DUAL_BATTERY)
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5461);

	if (sm_dc_get_current_state(sm_dc) < SM_DC_CHECK_VBAT
		&& !(sm5461->rev_boost) && (sm_dc->chip_id != SM5461_SUB)) {
		/* Didn't worked ADC block during on CHG-OFF status */
		return 0;
	}
#endif

	for (cnt = 0; cnt < 2; ++cnt) {
		if (sm5461_get_enadc(sm5461) == 0)
			sm5461_enable_adc_oneshot(sm5461, 1);

		switch (index) {
		case SM5461_ADC_THEM:   /* unit - mV */
			if (sm5461->chip_id != SM5461_SUB) {
				psy_dc_sub = power_supply_get_by_name("sm5461-charger-sub");
				if (!psy_dc_sub)
					sm5461_sub = NULL;
				else
					sm5461_sub = power_supply_get_drvdata(psy_dc_sub);
			}

			if (sm5461_sub)
				sm5461_bulk_read(sm5461_sub, SM5461_REG_THEM_ADC_H, 2, regs);
			else
				sm5461_bulk_read(sm5461, SM5461_REG_THEM_ADC_H, 2, regs);
			adc = (((int)(regs[0] & 0x0F) << 8) + regs[1] + 1) >> 1;
			break;
		case SM5461_ADC_TDIE:   /* unit - C */
			sm5461_read_reg(sm5461, SM5461_REG_TDIE_ADC, regs);
			adc = -40 + regs[0];
			break;
		case SM5461_ADC_VBUS:
			sm5461_bulk_read(sm5461, SM5461_REG_VBUS_ADC_H, 2, regs);
			adc = (((int)(regs[0] & 0x0F) << 8) + regs[1]) * 4;
			break;
		case SM5461_ADC_IBUS:
			sm5461_bulk_read(sm5461, SM5461_REG_IBUS_ADC_H, 2, regs);
			adc = (((int)(regs[0] & 0x0F) << 8) + regs[1]) * 125 / 100;
			if (sm5461_get_op_mode(sm5461) == OP_MODE_FW_BYPASS)
				adc *= 2;
			break;
		case SM5461_ADC_VBAT:
			sm5461_bulk_read(sm5461, SM5461_REG_VBAT_ADC_H, 2, regs);
			adc = (((int)(regs[0] & 0x0F) << 8) + regs[1]) * 125 / 100;
			break;
		case SM5461_ADC_IBAT:
			sm5461_bulk_read(sm5461, SM5461_REG_IBAT_ADC_H, 2, regs);
			adc = (((int)(regs[0] & 0x0F) << 8) + regs[1]) * 2;
			break;
		default:
			adc = 0;
		}

		/* prevent for reset of register */
		ret = sm5461_get_wdt_timer(sm5461);
		if (ret != WDT_TIMER_S_40) {
			dev_err(sm5461->dev, "%s: detected REG Reset condition(ret=0x%x)\n", __func__, ret);
			sm5461_init_reg_param(sm5461);
		} else {
			break;
		}
	}
	dev_info(sm5461->dev, "%s: index(%d)=[0x%02X,0x%02X], adc=%d\n",
		__func__, index, regs[0], regs[1], adc);

	return adc;
}

static int sm5461_set_adc_mode(struct i2c_client *i2c, u8 mode)
{
	struct sm5461_charger *sm5461 = i2c_get_clientdata(i2c);

	switch (mode) {
	case SM_DC_ADC_MODE_ONESHOT:
		/* covered by continuous mode */
	case SM_DC_ADC_MODE_CONTINUOUS:
		/* SM5461 continuous mode reflash time : 200ms */
		sm5461_set_adcmode(sm5461, 0);
		sm5461_enable_adc(sm5461, 1);
		break;
	case SM_DC_ADC_MODE_OFF:
	default:
		sm5461_set_adcmode(sm5461, 0);
		sm5461_enable_adc(sm5461, 0);
		break;
	}

	return 0;
}

static void sm5461_print_regmap(struct sm5461_charger *sm5461)
{
	u8 print_reg_num, regs[64] = {0x0, };
	char temp_buf[128] = {0,};
	char reg_addr[16] = {0x0C, 0x0E, 0x10, 0x11, 0x60, 0x61, 0x62, 0x64, };
	u8 reg_data;
	int i;

	print_reg_num = SM5461_REG_FLAG1 - SM5461_REG_CNTL1;
	sm5461_bulk_read(sm5461, SM5461_REG_CNTL1, print_reg_num, regs);
	for (i = 0; i < print_reg_num; ++i)
		sprintf(temp_buf+strlen(temp_buf), "0x%02X[0x%02X],", SM5461_REG_CNTL1 + i, regs[i]);

	pr_info("sm5461-charger: regmap: %s\n", temp_buf);
	memset(temp_buf, 0x0, sizeof(temp_buf));

	print_reg_num = 8;
	for (i = 0; i < print_reg_num; ++i) {
		sm5461_read_reg(sm5461, reg_addr[i], &reg_data);
		sprintf(temp_buf+strlen(temp_buf), "0x%02X[0x%02X],", reg_addr[i], reg_data);
	}
	pr_info("sm5461-charger: regmap: %s\n", temp_buf);
	memset(temp_buf, 0x0, sizeof(temp_buf));
}

static int sm5461_reverse_boost_enable(struct sm5461_charger *sm5461, int op_mode)
{
	u8 i, flag1, flag2, flag3, flag4;

	if (op_mode && !sm5461->rev_boost) {
		for (i = 0; i < 2; ++i) {
			flag3 = sm5461_get_flag_status(sm5461, SM5461_REG_FLAG3);
			dev_info(sm5461->dev, "%s: FLAG3:0x%x i=%d\n", __func__, flag3, i);
			if (flag3 & (SM5461_FLAG3_VBUSUVLO | SM5461_FLAG3_VBUSPOK))
				msleep(20);
			else
				break;
		}

		sm5461_set_op_mode(sm5461, op_mode);
		dev_info(sm5461->dev, "%s: set op_mode:0x%x\n",	__func__, op_mode);

		for (i = 0; i < 12; ++i) {
			usleep_range(10000, 11000);
			sm5461_read_reg(sm5461, SM5461_REG_FLAG1, &flag1);
			sm5461_read_reg(sm5461, SM5461_REG_FLAG2, &flag2);
			sm5461_read_reg(sm5461, SM5461_REG_FLAG3, &flag3);
			sm5461_read_reg(sm5461, SM5461_REG_FLAG4, &flag4);
			dev_info(sm5461->dev, "%s: FLAG:0x%x:0x%x:0x%x:0x%x i=%d\n",
				__func__, flag1, flag2, flag3, flag4, i);
			if (flag4 & SM5461_FLAG4_RVSRDY)
				break;
		}
		if (i == 12) {
			dev_err(sm5461->dev, "%s: fail to reverse enable\n", __func__);
			sm5461_set_op_mode(sm5461, OP_MODE_INIT);
			sm5461->rev_boost = false;
			return -EINVAL;
		}
		sm5461_set_adc_mode(sm5461->i2c, SM_DC_ADC_MODE_CONTINUOUS);
		dev_info(sm5461->dev, "%s: ON\n", __func__);
	} else if (!op_mode) {
		sm5461_set_op_mode(sm5461, OP_MODE_INIT);
		sm5461_set_adc_mode(sm5461->i2c, SM_DC_ADC_MODE_OFF);
		dev_info(sm5461->dev, "%s: OFF\n", __func__);
	}
	sm5461->rev_boost = op_mode < 1 ? 0 : 1;

	return 0;
}

static bool sm5461_check_charging_enable(struct sm5461_charger *sm5461)
{
	u8 reg;

	reg = sm5461_get_op_mode(sm5461);
	if (reg == OP_MODE_FW_BOOST || reg == OP_MODE_FW_BYPASS)
		return true;
	else
		return false;
}

static int sm5461_start_charging(struct sm5461_charger *sm5461)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5461);
	int state = sm_dc_get_current_state(sm_dc);
	struct power_supply *psy_dc_sub = NULL;
	struct sm5461_charger *sm5461_sub = NULL;
	union power_supply_propval val;
	int ret;

	if (sm5461->cable_online < 1) {
		dev_err(sm5461->dev, "%s: can't detect valid cable connection(online=%d)\n",
			__func__, sm5461->cable_online);
		return -ENODEV;
	}

	/* Get TA type information from battery psy */
	psy_do_property("battery", get, POWER_SUPPLY_PROP_ONLINE, val);

	if (val.intval == SEC_BATTERY_CABLE_FPDO_DC)
		/* The TA type is USBPD charger with only fixed PDO */
		sm5461->ta_type = SM5461_TA_USBPD_2P0;
	else
		sm5461->ta_type = SM5461_TA_USBPD;

	/* Check dc mode */
	if (sm_dc->chip_id != SM5461_SUB) {
		psy_dc_sub = power_supply_get_by_name("sm5461-charger-sub");
		if (!psy_dc_sub) {
			sm_dc->i2c_sub = NULL;
			dev_info(sm5461->dev, "%s: start single dc mode\n", __func__);
		} else {
			sm5461_sub = power_supply_get_drvdata(psy_dc_sub);
			sm_dc->i2c_sub = sm5461_sub->i2c;
			dev_info(sm5461->dev, "%s: start dual dc mode\n", __func__);
		}
	} else {
		dev_info(sm5461->dev, "%s: unable to start sub dc standalone\n", __func__);
		return -EINVAL;
	}

	if (state < SM_DC_CHECK_VBAT) {
		dev_info(sm5461->dev, "%s: charging off state (state=%d)\n", __func__, state);
		ret = sm5461_sw_reset(sm5461);
		if (ret < 0) {
			dev_err(sm5461->dev, "%s: fail to sw reset(ret=%d)\n", __func__, ret);
			return ret;
		}
		sm5461_init_reg_param(sm5461);

		if (sm5461_sub) {
			ret = sm5461_sw_reset(sm5461_sub);
			if (ret < 0) {
				dev_err(sm5461_sub->dev, "%s: fail to sw reset(ret=%d)\n", __func__, ret);
				return ret;
			}
			sm5461_init_reg_param(sm5461_sub);
		}
	} else if (state == SM_DC_CV_MAN) {
		dev_info(sm5461->dev, "%s: skip start charging (state=%d)\n", __func__, state);
		return 0;
	}

	if (sm5461->ta_type == SM5461_TA_USBPD_2P0)
		ret = sm_dc_start_fpdo_charging(sm_dc);
	else
		ret = sm_dc_start_charging(sm_dc);

	if (ret < 0) {
		dev_err(sm5461->dev, "%s: fail to start direct-charging\n", __func__);
		return ret;
	}
	__pm_stay_awake(sm5461->chg_ws);

	return 0;
}

static int sm5461_stop_charging(struct sm5461_charger *sm5461)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5461);

	sm_dc_stop_charging(sm_dc);

	__pm_relax(sm5461->chg_ws);

	return 0;
}

static int sm5461_start_pass_through_charging(struct sm5461_charger *sm5461)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5461);
	int state = sm_dc_get_current_state(sm_dc);
	int ret;

	if (sm5461->cable_online < 1) {
		dev_err(sm5461->dev, "%s: can't detect valid cable connection(online=%d)\n",
			__func__, sm5461->cable_online);
		return -ENODEV;
	}

	if (state < SM_DC_PRESET) {
		pr_err("%s %s: charging off state (state=%d)\n", sm_dc->name, __func__, sm_dc->state);
		return -ENODEV;
	}

	sm5461_stop_charging(sm5461);
	msleep(200);

	/* Disable IBUSUCP & Set freq*/
	sm5461_set_freq(sm5461, sm5461->pdata->freq_byp); /* FREQ */
	sm5461_write_reg(sm5461, SM5461_REG_CNTL3, 0x83); /* RLTVOVP = disabled, RLTVUVP = disabled */
	sm5461_update_reg(sm5461, SM5461_REG_IBUS_PROT, 0, 0x1, 7); /* IBUSUCP = disabled */

	ret = sm_dc_start_manual_charging(sm_dc);
	if (ret < 0) {
		dev_err(sm5461->dev, "%s: fail to start direct-charging\n", __func__);
		return ret;
	}
	__pm_stay_awake(sm5461->chg_ws);

	return 0;
}

static int sm5461_set_pass_through_ta_vol(struct sm5461_charger *sm5461, int delta_soc)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5461);
	int state = sm_dc_get_current_state(sm_dc);

	if (sm5461->cable_online < 1) {
		dev_err(sm5461->dev, "%s: can't detect valid cable connection(online=%d)\n",
			__func__, sm5461->cable_online);
		return -ENODEV;
	}

	if (state != SM_DC_CV_MAN) {
		pr_err("%s %s: pass-through mode was not set.(dc state = %d)\n", sm_dc->name, __func__, sm_dc->state);
		return -ENODEV;
	}

	return sm_dc_set_ta_volt_by_soc(sm_dc, delta_soc);
}

static int psy_chg_get_online(struct sm5461_charger *sm5461)
{
	u8 flag, vbus_pok, cable_online;

	cable_online = sm5461->cable_online < 1 ? 0 : 1;
	flag = sm5461_get_flag_status(sm5461, SM5461_REG_FLAG3);
	dev_info(sm5461->dev, "%s: FLAG3=0x%x\n", __func__, flag);
	vbus_pok = (flag >> 7) & 0x1;

	if (vbus_pok != cable_online) {
		dev_err(sm5461->dev, "%s: mismatched vbus state(vbus_pok:%d cable_online:%d)\n",
		__func__, vbus_pok, sm5461->cable_online);
	}

	return sm5461->cable_online;
}

static int psy_chg_get_status(struct sm5461_charger *sm5461)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	u8 flag1, flag2, flag3, flag4;

	flag1 = sm5461_get_flag_status(sm5461, SM5461_REG_FLAG1);
	flag2 = sm5461_get_flag_status(sm5461, SM5461_REG_FLAG2);
	flag3 = sm5461_get_flag_status(sm5461, SM5461_REG_FLAG3);
	flag4 = sm5461_get_flag_status(sm5461, SM5461_REG_FLAG4);
	dev_info(sm5461->dev, "%s: FLAG:0x%x:0x%x:0x%x:0x%x\n",
		__func__, flag1, flag2, flag3, flag4);

	if (sm5461_check_charging_enable(sm5461)) {
		status = POWER_SUPPLY_STATUS_CHARGING;
	} else {
		if ((flag3 >> 7) & 0x1) { /* check vbus-pok */
			status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		} else {
			status = POWER_SUPPLY_STATUS_DISCHARGING;
		}
	}

	return status;
}

static int psy_chg_get_health(struct sm5461_charger *sm5461)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5461);
	int state = sm_dc_get_current_state(sm_dc);
	int health = POWER_SUPPLY_HEALTH_GOOD;
	u8 flag, chg_on;

	if (state == SM_DC_ERR) {
		health = POWER_SUPPLY_EXT_HEALTH_DC_ERR;
		dev_info(sm5461->dev, "%s: chg_state=%d, health=%d\n",
			__func__, state, health);
	} else if (state >= SM_DC_PRE_CC && state <= SM_DC_CV) {
		chg_on = sm5461_check_charging_enable(sm5461);
		flag = sm5461_get_flag_status(sm5461, SM5461_REG_FLAG3);
		if (chg_on == false) {
			health = POWER_SUPPLY_EXT_HEALTH_DC_ERR;
		} else if (((flag >> 7) & 0x1) == 0x0) {    /* VBUS_POK status is disabled */
			health = POWER_SUPPLY_EXT_HEALTH_DC_ERR;
		}

		dev_info(sm5461->dev, "%s: chg_on=%d, flag3=0x%x, health=%d\n",
			__func__, chg_on, flag, health);
	}

	return health;
}

static int psy_chg_get_chg_vol(struct sm5461_charger *sm5461)
{
	u32 chg_vol = sm5461_get_vbatreg(sm5461);

	dev_info(sm5461->dev, "%s: VBAT_REG=%dmV\n", __func__, chg_vol);

	return chg_vol;
}

static int psy_chg_get_input_curr(struct sm5461_charger *sm5461)
{
	u32 input_curr = sm5461_get_ibuslim(sm5461);

	dev_info(sm5461->dev, "%s: IBUSLIM=%dmA\n", __func__, input_curr);

	return input_curr;
}

static int psy_chg_get_ext_monitor_work(struct sm5461_charger *sm5461)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5461);
	int state = sm_dc_get_current_state(sm_dc);
	int adc_vbus, adc_ibus, adc_vbat, adc_ibat, adc_them, adc_tdie;

	if (state < SM_DC_CHECK_VBAT && (sm_dc->chip_id != SM5461_SUB)) {
		dev_info(sm5461->dev, "%s: charging off state (state=%d)\n", __func__, state);
		return -EINVAL;
	}

	adc_vbus = sm5461_convert_adc(sm5461, SM5461_ADC_VBUS);
	adc_ibus = sm5461_convert_adc(sm5461, SM5461_ADC_IBUS);
	adc_vbat = sm5461_convert_adc(sm5461, SM5461_ADC_VBAT);
	adc_ibat = sm5461_convert_adc(sm5461, SM5461_ADC_IBAT);
	adc_them = sm5461_convert_adc(sm5461, SM5461_ADC_THEM);
	adc_tdie = sm5461_convert_adc(sm5461, SM5461_ADC_TDIE);

	pr_info("sm5461-charger: adc_monitor: vbus:%d:ibus:%d:vbat:%d:ibat:%d:them:%d:tdie:%d\n",
		adc_vbus, adc_ibus, adc_vbat, adc_ibat, adc_them, adc_tdie);
	sm5461_print_regmap(sm5461);

	return 0;
}

static int psy_chg_get_ext_measure_input(struct sm5461_charger *sm5461, int index)
{
	struct sm5461_charger *sm5461_sub = NULL;
	struct power_supply *psy_dc_sub = NULL;
	int adc = 0;

	if (sm5461->chip_id != SM5461_SUB) {
		psy_dc_sub = power_supply_get_by_name("sm5461-charger-sub");
		if (!psy_dc_sub)
			sm5461_sub = NULL;
		else
			sm5461_sub = power_supply_get_drvdata(psy_dc_sub);
	}

	switch (index) {
	case SEC_BATTERY_IIN_MA:
		if (sm5461_sub)
			adc = sm5461_convert_adc(sm5461_sub, SM5461_ADC_IBUS);
		adc += sm5461_convert_adc(sm5461, SM5461_ADC_IBUS);
		break;
	case SEC_BATTERY_IIN_UA:
		if (sm5461_sub)
			adc = sm5461_convert_adc(sm5461_sub, SM5461_ADC_IBUS) * 1000;
		adc += sm5461_convert_adc(sm5461, SM5461_ADC_IBUS) * 1000;
		break;
	case SEC_BATTERY_VIN_MA:
		adc = sm5461_convert_adc(sm5461, SM5461_ADC_VBUS);
		break;
	case SEC_BATTERY_VIN_UA:
		adc = sm5461_convert_adc(sm5461, SM5461_ADC_VBUS) * 1000;
		break;
	default:
		adc = 0;
		break;
	}

	dev_info(sm5461->dev, "%s: index=%d, adc=%d\n", __func__, index, adc);

	return adc;
}

static const char * const sm5461_dc_state_str[] = {
	"NO_CHARGING", "DC_ERR", "CHARGING_DONE", "CHECK_VBAT", "PRESET_DC",
	"ADJUST_CC", "UPDATE_BAT", "CC_MODE", "CV_MODE", "CV_MAN_MODE", "CV_FPDO_MODE"
};
static int psy_chg_get_ext_direct_charger_chg_status(
		struct sm5461_charger *sm5461, union power_supply_propval *val)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5461);
	int state = sm_dc_get_current_state(sm_dc);

	val->strval = sm5461_dc_state_str[state];
	pr_info("%s: CHARGER_STATUS(%s)\n", __func__, val->strval);

	return 0;
}

static int sm5461_chg_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct sm5461_charger *sm5461 = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp =
		(enum power_supply_ext_property)psp;
	int adc_them;

	dev_info(sm5461->dev, "%s: psp=%d\n", __func__, psp);

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = psy_chg_get_online(sm5461);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = psy_chg_get_status(sm5461);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = psy_chg_get_health(sm5461);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		val->intval = psy_chg_get_chg_vol(sm5461);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = psy_chg_get_input_curr(sm5461);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		val->intval = sm5461->target_ibat;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		adc_them = sm5461_convert_adc(sm5461, SM5461_ADC_THEM);
		val->intval = adc_them;
		break;
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = sm5461_convert_adc(sm5461, SM5461_ADC_VBAT);
		break;
#endif
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			val->intval = sm5461_check_charging_enable(sm5461);
			break;
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			psy_chg_get_ext_monitor_work(sm5461);
			break;
		case POWER_SUPPLY_EXT_PROP_MEASURE_INPUT:
			val->intval = psy_chg_get_ext_measure_input(sm5461, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_MEASURE_SYS:
			dev_err(sm5461->dev, "%s: need to works\n", __func__);
			/*
			 *  Need to check operation details.. by SEC.
			 */
			val->intval = -EINVAL;
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_CHG_STATUS:
			psy_chg_get_ext_direct_charger_chg_status(sm5461, val);
			break;

		case POWER_SUPPLY_EXT_PROP_PASS_THROUGH_MODE:
			break;

		case POWER_SUPPLY_EXT_PROP_PASS_THROUGH_MODE_TA_VOL:
			break;

		case POWER_SUPPLY_EXT_PROP_D2D_REVERSE_VOLTAGE:
			val->intval = sm5461->rev_boost ? 1 : 0;
			break;

		case POWER_SUPPLY_EXT_PROP_D2D_REVERSE_OCP:
			val->intval = sm5461_get_reverse_boost_ocp(sm5461);
			break;

		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int psy_chg_set_online(struct sm5461_charger *sm5461, int online)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5461);
	int ret = 0;

	dev_info(sm5461->dev, "%s: online=%d\n", __func__, online);

	if (online < 2) {
		if (sm_dc_get_current_state(sm_dc) > SM_DC_EOC)
			sm5461_stop_charging(sm5461);
	}
	sm5461->cable_online = online;

	sm5461->vbus_in = sm5461->cable_online > 2 ? true : false;

	return ret;
}

static int psy_chg_set_const_chg_voltage(struct sm5461_charger *sm5461, int vbat)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5461);
	int state = sm_dc_get_current_state(sm_dc);
	int ret = 0;

	dev_info(sm5461->dev, "%s: [%dmV] to [%dmV]\n", __func__, sm5461->target_vbat, vbat);

	if (state == SM_DC_CV_FPDO) {
		dev_info(sm5461->dev, "%s: voltage cannot be changed during FPDO DC.", __func__);
		return -EINVAL;
	}

	if (sm5461->target_vbat != vbat || state < SM_DC_CHECK_VBAT) {
		sm5461->target_vbat = vbat;
		ret = sm_dc_set_target_vbat(sm_dc, sm5461->target_vbat);
	}

	return ret;
}

static int psy_chg_set_chg_curr(struct sm5461_charger *sm5461, int ibat)
{
	int ret = 0;

	dev_info(sm5461->dev, "%s: dldn't support cc_loop\n", __func__);
	sm5461->target_ibat = ibat;

	return ret;
}

static int psy_chg_set_input_curr(struct sm5461_charger *sm5461, int ibus)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5461);
	int state = sm_dc_get_current_state(sm_dc);
	int ret = 0;

	dev_info(sm5461->dev, "%s: ibus [%dmA] to [%dmA]\n", __func__, sm5461->target_ibus, ibus);

	if (state == SM_DC_CV_FPDO) {
		dev_info(sm5461->dev, "%s: ibus cannot be changed during FPDO DC.", __func__);
		return -EINVAL;
	}

	if (sm5461->target_ibus != ibus || state < SM_DC_CHECK_VBAT) {
		sm5461->target_ibus = ibus;
		if (sm5461->target_ibus < SM5461_TA_MIN_CURRENT) {
			dev_info(sm5461->dev, "%s: can't used less then ta_min_current(%dmA)\n",
				__func__, SM5461_TA_MIN_CURRENT);
			sm5461->target_ibus = SM5461_TA_MIN_CURRENT;
		}
		ret = sm_dc_set_target_ibus(sm_dc, sm5461->target_ibus);
	}

	return ret;
}

static int psy_chg_set_const_chg_voltage_max(struct sm5461_charger *sm5461, int max_vbat)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5461);

	dev_info(sm5461->dev, "%s: max_vbat [%dmA] to [%dmA]\n",
		__func__, sm5461->max_vbat, max_vbat);

	sm5461->max_vbat = max_vbat;
	sm_dc->config.chg_float_voltage = max_vbat;

	return 0;

}

static int psy_chg_ext_wdt_control(struct sm5461_charger *sm5461, int wdt_control)
{
	if (wdt_control)
		sm5461->wdt_disable = 1;
	else
		sm5461->wdt_disable = 0;

	dev_info(sm5461->dev, "%s: wdt_disable=%d\n", __func__, sm5461->wdt_disable);

	return 0;
}

static int sm5461_chg_set_property(struct power_supply *psy,
	enum power_supply_property psp, const union power_supply_propval *val)
{
	struct sm5461_charger *sm5461 = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp =
		(enum power_supply_ext_property) psp;

	int ret = 0;

	dev_info(sm5461->dev, "%s: psp=%d, val-intval=%d\n", __func__, psp, val->intval);

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		psy_chg_set_online(sm5461, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = psy_chg_set_const_chg_voltage(sm5461, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = psy_chg_set_const_chg_voltage_max(sm5461, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = psy_chg_set_chg_curr(sm5461, val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = psy_chg_set_input_curr(sm5461, val->intval);
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_DIRECT_WDT_CONTROL:
			ret = psy_chg_ext_wdt_control(sm5461, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			if (val->intval)
				ret = sm5461_start_charging(sm5461);
			else
				ret = sm5461_stop_charging(sm5461);

			sm5461_print_regmap(sm5461);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CURRENT_MAX:
			ret = psy_chg_set_input_curr(sm5461, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_ADC_CTRL:
			if (val->intval)
				sm5461_enable_adc_oneshot(sm5461, 1);
			else
				sm5461_enable_adc_oneshot(sm5461, 0);
			pr_info("%s: ADC_CTRL : %d\n", __func__, val->intval);
			break;

		case POWER_SUPPLY_EXT_PROP_PASS_THROUGH_MODE:
			pr_info("[PASS_THROUGH] %s: called\n", __func__);
			if (val->intval)
				ret = sm5461_start_pass_through_charging(sm5461);
			else
				ret = sm5461_stop_charging(sm5461);
			break;

		case POWER_SUPPLY_EXT_PROP_PASS_THROUGH_MODE_TA_VOL:
			pr_info("[PASS_THROUGH_VOL] %s: called\n", __func__);
			sm5461_set_pass_through_ta_vol(sm5461, val->intval);
			break;

		case POWER_SUPPLY_EXT_PROP_D2D_REVERSE_VOLTAGE:
			if (val->intval)
				ret = sm5461_reverse_boost_enable(sm5461, OP_MODE_REV_BOOST);
			else
				ret = sm5461_reverse_boost_enable(sm5461, 0);
			break;

		case POWER_SUPPLY_EXT_PROP_DC_REVERSE_MODE:
			if (val->intval == POWER_SUPPLY_DC_REVERSE_1TO2)
				ret = sm5461_reverse_boost_enable(sm5461, OP_MODE_REV_BOOST);
			else if (val->intval == POWER_SUPPLY_DC_REVERSE_BYP)
				ret = sm5461_reverse_boost_enable(sm5461, OP_MODE_REV_BYPASS);
			else
				ret = sm5461_reverse_boost_enable(sm5461, 0);
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	if (ret < 0)
		return ret;

	return 0;
}

static char *sm5461_supplied_to[] = {
	"sm5461-charger",
};

static enum power_supply_property sm5461_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static const struct power_supply_desc sm5461_charger_power_supply_desc = {
	.name		= "sm5461-charger",
	.type		= POWER_SUPPLY_TYPE_UNKNOWN,
	.get_property	= sm5461_chg_get_property,
	.set_property	= sm5461_chg_set_property,
	.properties	= sm5461_charger_props,
	.num_properties	= ARRAY_SIZE(sm5461_charger_props),
};

static const struct power_supply_desc sm5461_charger_power_supply_sub_desc = {
	.name		= "sm5461-charger-sub",
	.type		= POWER_SUPPLY_TYPE_UNKNOWN,
	.get_property	= sm5461_chg_get_property,
	.set_property	= sm5461_chg_set_property,
	.properties	= sm5461_charger_props,
	.num_properties	= ARRAY_SIZE(sm5461_charger_props),
};

static int sm5461_get_adc_value(struct i2c_client *i2c, u8 adc_ch)
{
	struct sm5461_charger *sm5461 = i2c_get_clientdata(i2c);
	int adc;

	switch (adc_ch) {
	case SM_DC_ADC_THEM:
		adc = sm5461_convert_adc(sm5461, SM5461_ADC_THEM);
		break;
	case SM_DC_ADC_DIETEMP:
		adc = sm5461_convert_adc(sm5461, SM5461_ADC_TDIE);
		break;
	case SM_DC_ADC_VBAT:
		adc = sm5461_convert_adc(sm5461, SM5461_ADC_VBAT);
		break;
	case SM_DC_ADC_IBAT:
		adc = sm5461_convert_adc(sm5461, SM5461_ADC_IBAT);
		break;
	case SM_DC_ADC_VBUS:
		adc = sm5461_convert_adc(sm5461, SM5461_ADC_VBUS);
		break;
	case SM_DC_ADC_IBUS:
		adc = sm5461_convert_adc(sm5461, SM5461_ADC_IBUS);
		break;
	case SM_DC_ADC_VOUT:
	default:
		adc = 0;
		break;
	}

	return adc;
}

static int sm5461_get_charging_enable(struct i2c_client *i2c)
{
	struct sm5461_charger *sm5461 = i2c_get_clientdata(i2c);

	return sm5461_check_charging_enable(sm5461);
}

static int sm5461_set_charging_enable(struct i2c_client *i2c, bool enable)
{
	struct sm5461_charger *sm5461 = i2c_get_clientdata(i2c);
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5461);

	if (enable) {
		if (sm_dc->ta.v_max < SM_DC_BYPASS_TA_MAX_VOL)
			sm5461_set_op_mode(sm5461, OP_MODE_FW_BYPASS);
		else
			sm5461_set_op_mode(sm5461, OP_MODE_FW_BOOST);
	} else {
		sm5461_set_op_mode(sm5461, OP_MODE_INIT);
	}

	sm5461_print_regmap(sm5461);

	return 0;
}

static int sm5461_dc_set_charging_config(struct i2c_client *i2c, u32 cv_gl, u32 ci_gl, u32 cc_gl)
{
	struct sm5461_charger *sm5461 = i2c_get_clientdata(i2c);
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5461);
	u32 vbatreg, ibuslim, freq;

	vbatreg = cv_gl + SM5461_CV_OFFSET;
	if (ci_gl <= SM5461_TA_MIN_CURRENT)
		ibuslim = ci_gl + (SM5461_CI_OFFSET * 2);
	else
		ibuslim = ci_gl + SM5461_CI_OFFSET;

	if (ibuslim % 100)
		ibuslim = ((ibuslim / 100) * 100) + 100;

	if (ci_gl <= SM5461_SIOP_LEV1)
		freq = sm5461->pdata->freq_siop[0];
	else if (ci_gl <= SM5461_SIOP_LEV2)
		freq = sm5461->pdata->freq_siop[1];
	else
		freq = sm5461->pdata->freq;

	sm5461_set_ibuslim(sm5461, ibuslim);
	sm5461_set_vbatreg(sm5461, vbatreg);
	sm5461_set_freq(sm5461, freq);

	pr_info("%s %s: vbat_reg=%dmV, ibus_lim=%dmA, freq=%dkHz\n", sm_dc->name,
		__func__, vbatreg, ibuslim, SM5461_freq_val[freq]);

	return 0;
}

static int sm5461_x2bat_dc_set_charging_config(struct i2c_client *i2c, u32 cv_gl, u32 ci_gl, u32 cc_gl)
{
	struct sm5461_charger *sm5461 = i2c_get_clientdata(i2c);
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5461);
	u32 vbatreg, ibuslim, freq;

	vbatreg = cv_gl + SM5461_CV_OFFSET;

	if (sm_dc->chip_id == SM5461_SUB) {
		if (ci_gl <= SM5461_TA_MIN_CURRENT)
			ibuslim = ci_gl + SM_DC_CI_OFFSET_X2BAT + SM5461_CI_OFFSET;
		else
			ibuslim = ci_gl + SM_DC_CI_OFFSET_X2BAT;

		if (ibuslim % 100)
			ibuslim = ((ibuslim / 100) * 100) + 100;
	} else {
		ibuslim = ((ci_gl - 100) / 100) * 100;
	}

	if (ci_gl <= SM5461_SIOP_LEV1)
		freq = sm5461->pdata->freq_siop[0];
	else if (ci_gl <= SM5461_SIOP_LEV2)
		freq = sm5461->pdata->freq_siop[1];
	else
		freq = sm5461->pdata->freq;

	sm5461_set_ibuslim(sm5461, ibuslim);
	sm5461_set_vbatreg(sm5461, vbatreg);
	sm5461_set_freq(sm5461, freq);

	pr_info("%s %s: vbat_reg=%dmV, ibus_lim=%dmA, freq=%dkHz\n", sm_dc->name,
		__func__, vbatreg, ibuslim, SM5461_freq_val[freq]);

	return 0;
}

static u32 sm5461_get_dc_error_status(struct i2c_client *i2c)
{
	struct sm5461_charger *sm5461 = i2c_get_clientdata(i2c);
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5461);
	u8 flag1, flag2, flag3, flag4, op_mode;
	u8 flag1_s, flag2_s, flag3_s, flag4_s;
	u32 err = SM_DC_ERR_NONE;
	int vnow = 0;

	mutex_lock(&sm5461->i2c_lock);
	sm5461_read_reg(sm5461, SM5461_REG_FLAG1, &flag1);
	sm5461_read_reg(sm5461, SM5461_REG_FLAG2, &flag2);
	sm5461_read_reg(sm5461, SM5461_REG_FLAG3, &flag3);
	sm5461_read_reg(sm5461, SM5461_REG_FLAG4, &flag4);
	pr_info("%s %s: FLAG:0x%x:0x%x:0x%x:0x%x\n", sm_dc->name,
		__func__, flag1, flag2, flag3, flag4);

	sm5461_read_reg(sm5461, SM5461_REG_FLAG1, &flag1_s);
	sm5461_read_reg(sm5461, SM5461_REG_FLAG2, &flag2_s);
	sm5461_read_reg(sm5461, SM5461_REG_FLAG3, &flag3_s);
	sm5461_read_reg(sm5461, SM5461_REG_FLAG4, &flag4_s);
	op_mode = sm5461_get_op_mode(sm5461);
	pr_info("%s %s: FLAG:0x%x:0x%x:0x%x:0x%x op_mode=0x%x\n", sm_dc->name,
		__func__, flag1_s, flag2_s, flag3_s, flag4_s, op_mode);
	mutex_unlock(&sm5461->i2c_lock);

	if (sm_dc->chip_id != SM5461_SUB)
		vnow = sm5461_get_vnow(sm5461);

	if ((op_mode == OP_MODE_INIT) && (sm_dc->wq.retry_cnt < 3) && (sm_dc->chip_id != SM5461_SUB)) {
		pr_info("%s %s: try to retry, cnt:%d\n", sm_dc->name, __func__, sm_dc->wq.retry_cnt);
		sm_dc->wq.retry_cnt++;
		err = SM_DC_ERR_RETRY;
	} else if (op_mode == OP_MODE_INIT) {
		if ((flag1 & SM5461_FLAG1_IBUSUCP) || (flag1_s & SM5461_FLAG1_IBUSUCP))
			err += SM_DC_ERR_IBUSUCP;
		if ((!(flag3 & SM5461_FLAG3_VBUSPOK)) || (!(flag3_s & SM5461_FLAG3_VBUSPOK)))
			err += SM_DC_ERR_VBUSUVLO;
		if ((flag1 & SM5461_FLAG1_VBUSOVP) || (flag1_s & SM5461_FLAG1_VBUSOVP))
			err += SM_DC_ERR_VBUSOVP;
		if ((flag1 & SM5461_FLAG1_IBUSOCP) || (flag1_s & SM5461_FLAG1_IBUSOCP))
			err += SM_DC_ERR_IBUSOCP;
		if ((flag2 & SM5461_FLAG2_CNSHTP) || (flag2_s & SM5461_FLAG2_CNSHTP))
			err += SM_DC_ERR_CN_SHORT;
		if ((flag3 & SM5461_FLAG3_CFLYSHTP) || (flag3_s & SM5461_FLAG3_CFLYSHTP))
			err += SM_DC_ERR_CFLY_SHORT;

		if (err == SM_DC_ERR_NONE)
			err += SM_DC_ERR_UNKNOWN;

		if (err == SM_DC_ERR_UNKNOWN && sm_dc->chip_id == SM5461_SUB)
			pr_info("%s %s: SM_DC_ERR_NONE\n", sm_dc->name, __func__);
		else
			pr_info("%s %s: SM_DC_ERR(err=0x%x)\n", sm_dc->name, __func__, err);
	} else {
		if (!(flag3_s & SM5461_FLAG3_VBUSPOK)) {
			pr_info("%s %s: vbus uvlo detected, try to retry\n", sm_dc->name, __func__);
			err = SM_DC_ERR_RETRY;
		} else if ((flag2 & SM5461_FLAG2_VBATREG) || (flag2_s & SM5461_FLAG2_VBATREG) ||
					(sm_dc->target_vbat <= vnow)) {
			pr_info("%s %s: VBATREG detected\n", sm_dc->name, __func__);
			err = SM_DC_ERR_VBATREG;
		} else if (flag4_s & SM5461_FLAG4_IBUSREG) {
			pr_info("%s %s: IBUSREG detected\n", sm_dc->name, __func__);
			err = SM_DC_ERR_IBUSREG;
		} else if (vnow < 0) {
			err = SM_DC_ERR_INVAL_VBAT;
			pr_info("%s %s: SM_DC_ERR(err=0x%x)\n", sm_dc->name, __func__, err);
		}
	}

	if (!sm5461->wdt_disable)
		sm5461_kick_wdt(sm5461);

	return err;
}

static int sm5461_send_pd_msg(struct i2c_client *i2c, struct sm_dc_power_source_info *ta)
{
	struct sm5461_charger *sm5461 = i2c_get_clientdata(i2c);
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5461);
	int ret;

	if (sm5461->ta_type == SM5461_TA_USBPD_2P0) {
		pr_err("%s %s: ta_type is fpdo, skip pd_select_pps\n", sm_dc->name, __func__);
		return 0;
	}

	mutex_lock(&sm5461->pd_lock);

	ret = sec_pd_select_pps(ta->pdo_pos, ta->v, ta->c);
	if (ret == -EBUSY) {
		pr_info("%s %s: request again\n", sm_dc->name, __func__);
		msleep(100);
		ret = sec_pd_select_pps(ta->pdo_pos, ta->v, ta->c);
	}

	mutex_unlock(&sm5461->pd_lock);

	return ret;
	}

static int sm5461_get_apdo_max_power(struct i2c_client *i2c, struct sm_dc_power_source_info *ta)
{
	struct sm5461_charger *sm5461 = i2c_get_clientdata(i2c);
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5461);
	int ret;

	ta->pdo_pos = 0;        /* set '0' else return error */
	ta->v_max = 10000;      /* request voltage level */
	ta->c_max = 0;
	ta->p_max = 0;

	ret = sec_pd_get_apdo_max_power(&ta->pdo_pos, &ta->v_max, &ta->c_max, &ta->p_max);
	if (ret < 0) {
		dev_err(sm5461->dev, "%s: error:sec_pd_get_apdo_max_power\n", __func__);
	} else {
		sm_dc->ta.pdo_pos = ta->pdo_pos;
		sm_dc->ta.v_max = ta->v_max;
		sm_dc->ta.c_max = ta->c_max;
		sm_dc->ta.p_max = ta->p_max;
	}

	dev_info(sm5461->dev, "%s: pdo_pos:%d, max_vol:%dmV, max_cur:%dmA, max_pwr:%dmW\n",
			__func__, ta->pdo_pos, ta->v_max, ta->c_max, ta->p_max);
	return ret;
}


static u32 sm5461_get_target_ibus(struct i2c_client *i2c)
{
	struct sm5461_charger *sm5461 = i2c_get_clientdata(i2c);

	dev_info(sm5461->dev, "%s: %dmA\n", __func__, sm5461->target_ibus);
	return sm5461->target_ibus;
}

static const struct sm_dc_ops sm5461_dc_pps_ops = {
	.get_adc_value          = sm5461_get_adc_value,
	.set_adc_mode           = sm5461_set_adc_mode,
	.get_charging_enable    = sm5461_get_charging_enable,
	.get_dc_error_status    = sm5461_get_dc_error_status,
	.set_charging_enable    = sm5461_set_charging_enable,
	.set_charging_config    = sm5461_dc_set_charging_config,
	.send_power_source_msg  = sm5461_send_pd_msg,
	.get_apdo_max_power     = sm5461_get_apdo_max_power,
	.get_target_ibus        = sm5461_get_target_ibus,
};

static const struct sm_dc_ops sm5461_x2bat_dc_pps_ops = {
	.get_adc_value          = sm5461_get_adc_value,
	.set_adc_mode           = sm5461_set_adc_mode,
	.get_charging_enable    = sm5461_get_charging_enable,
	.get_dc_error_status    = sm5461_get_dc_error_status,
	.set_charging_enable    = sm5461_set_charging_enable,
	.set_charging_config    = sm5461_x2bat_dc_set_charging_config,
	.send_power_source_msg  = sm5461_send_pd_msg,
	.get_apdo_max_power     = sm5461_get_apdo_max_power,
	.get_target_ibus        = sm5461_get_target_ibus,
};

static irqreturn_t sm5461_irq_thread(int irq, void *data)
{
	struct sm5461_charger *sm5461 = (struct sm5461_charger *)data;
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5461);
	u8 op_mode = sm5461_get_op_mode(sm5461);
	u8 flag1, flag2, flag3, flag4;
	u32 err = 0x0;

	sm5461_read_reg(sm5461, SM5461_REG_FLAG1, &flag1);
	sm5461_read_reg(sm5461, SM5461_REG_FLAG2, &flag2);
	sm5461_read_reg(sm5461, SM5461_REG_FLAG3, &flag3);
	sm5461_read_reg(sm5461, SM5461_REG_FLAG4, &flag4);
	dev_info(sm5461->dev, "%s: FLAG:0x%x:0x%x:0x%x:0x%x\n",
		__func__, flag1, flag2, flag3, flag4);

	/* check forced CUT-OFF status */
	if (sm_dc_get_current_state(sm_dc) > SM_DC_PRESET &&
		op_mode == OP_MODE_INIT) {
		if (flag1 & SM5461_FLAG1_VBUSOVP)
			err += SM_DC_ERR_VBUSOVP;

		if (flag1 & SM5461_FLAG1_IBUSOCP)
			err += SM_DC_ERR_IBUSOCP;

		if (flag1 & SM5461_FLAG1_IBUSUCP)
			err += SM_DC_ERR_IBUSUCP;

		if (flag2 & SM5461_FLAG2_VBATOVP)
			err += SM_DC_ERR_VBATOVP;

/* un-used IBATOCP protection
 *		if (flag2 & SM5461_FLAG2_IBATOCP)
 *			err += SM_DC_ERR_IBATOCP;
 */
		if (flag2 & SM5461_FLAG2_TSD)
			err += SM_DC_ERR_TSD;

		if (flag2 & SM5461_FLAG2_RLTVOVP)
			err += SM_DC_ERR_STUP_FAIL;

		if (flag2 & SM5461_FLAG2_RLTVUVP)
			err += SM_DC_ERR_STUP_FAIL;

		if (flag2 & SM5461_FLAG2_CNSHTP)
			err += SM_DC_ERR_CN_SHORT;

		if (flag3 & SM5461_FLAG3_VBUSUVLO)
			err += SM_DC_ERR_VBUSUVLO;

		if (flag3 & SM5461_FLAG3_CFLYSHTP)
			err += SM_DC_ERR_CFLY_SHORT;

		dev_err(sm5461->dev, "%s: forced charge cut-off(err=0x%x)\n", __func__, err);
		sm_dc_report_error_status(sm_dc, err);
	}

	if (flag2 & SM5461_FLAG2_VBATREG) {
		dev_info(sm5461->dev, "%s: VBATREG detected\n", __func__);
		sm_dc_report_interrupt_event(sm_dc, SM_DC_INT_VBATREG);
	}
	if (flag2 & SM5461_FLAG2_IBATREG)
		dev_info(sm5461->dev, "%s: IBATREG detected\n", __func__);

	if (flag4 & SM5461_FLAG4_IBUSREG)
		dev_info(sm5461->dev, "%s: IBUSREG detected\n", __func__);

	if (flag3 & SM5461_FLAG3_VBUSPOK)
		dev_info(sm5461->dev, "%s: VBUSPOK detected\n", __func__);

	if (flag3 & SM5461_FLAG3_VBUSUVLO)
		dev_info(sm5461->dev, "%s: VBUSUVLO detected\n", __func__);

	if (flag3 & SM5461_FLAG3_WTDTMR) {
		dev_info(sm5461->dev, "%s: Watchdog Timer expired\n", __func__);
		sm_dc_report_interrupt_event(sm_dc, SM_DC_INT_WDTOFF);
	}

	dev_info(sm5461->dev, "closed %s\n", __func__);

	return IRQ_HANDLED;
}

static int sm5461_irq_init(struct sm5461_charger *sm5461)
{
	int ret;
	u8 reg;

	sm5461->irq = gpio_to_irq(sm5461->pdata->irq_gpio);
	dev_info(sm5461->dev, "%s: irq_gpio=%d, irq=%d\n", __func__,
	sm5461->pdata->irq_gpio, sm5461->irq);

	ret = gpio_request(sm5461->pdata->irq_gpio, "sm5540_irq");
	if (ret) {
		dev_err(sm5461->dev, "%s: fail to request gpio(ret=%d)\n",
			__func__, ret);
		return ret;
	}
	gpio_direction_input(sm5461->pdata->irq_gpio);
	gpio_free(sm5461->pdata->irq_gpio);

	sm5461_write_reg(sm5461, SM5461_REG_FLAGMSK1, 0xE2);
	sm5461_write_reg(sm5461, SM5461_REG_FLAGMSK2, 0x00);
	sm5461_write_reg(sm5461, SM5461_REG_FLAGMSK3, 0xD6);
	sm5461_write_reg(sm5461, SM5461_REG_FLAGMSK4, 0xDA);
	sm5461_read_reg(sm5461, SM5461_REG_FLAG1, &reg);
	sm5461_read_reg(sm5461, SM5461_REG_FLAG2, &reg);
	sm5461_read_reg(sm5461, SM5461_REG_FLAG3, &reg);
	sm5461_read_reg(sm5461, SM5461_REG_FLAG4, &reg);

	ret = request_threaded_irq(sm5461->irq, NULL, sm5461_irq_thread,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT,
		"sm5461-irq", sm5461);
	if (ret) {
		dev_err(sm5461->dev, "%s: fail to request irq(ret=%d)\n",
			__func__, ret);
		return ret;
	}

	return 0;
}

static int sm5461_hw_init_config(struct sm5461_charger *sm5461)
{
	int ret;
	u8 reg;

	/* check to valid I2C transfer & register control */
	ret = sm5461_read_reg(sm5461, SM5461_REG_DEVICE_ID, &reg);
	if (ret < 0 || (reg & 0xF) != 0x0) {
		dev_err(sm5461->dev, "%s: device not found on this channel (reg=0x%x)\n",
			__func__, reg);
		return -ENODEV;
	}
	sm5461->pdata->rev_id = (reg >> 4) & 0xf;

	sm5461_init_reg_param(sm5461);
	if (sm5461->chip_id == SM5461_SUB)
		sm5461_set_opid(sm5461, SM5461_SUB);
	else if (sm5461->pdata->single_mode)
		sm5461_set_opid(sm5461, SM5461_ALONE);
	else
		sm5461_set_opid(sm5461, SM5461_MAIN);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	psy_chg_set_const_chg_voltage(sm5461, sm5461->pdata->battery.chg_float_voltage);
	sm5461_reverse_boost_enable(sm5461, 0);
#endif

	return 0;
}

#if defined(CONFIG_OF)
static int sm5461_charger_parse_dt(struct device *dev,
									struct sm5461_platform_data *pdata)
{
	struct device_node *np_sm5461 = dev->of_node;
	struct device_node *np_battery;
	int ret;

	/* Parse: sm5461 node */
	if (!np_sm5461) {
		dev_err(dev, "%s: empty of_node for sm5461_dev\n", __func__);
		return -EINVAL;
	}
	pdata->irq_gpio = of_get_named_gpio(np_sm5461, "sm5461,irq-gpio", 0);
	dev_info(dev, "parse_dt: irq_gpio=%d\n", pdata->irq_gpio);

	ret = of_property_read_u32(np_sm5461, "sm5461,r_ttl", &pdata->r_ttl);
	if (ret) {
		dev_info(dev, "%s: sm5461,r_ttl is Empty\n", __func__);
		pdata->r_ttl = 320000;
	}
	dev_info(dev, "parse_dt: r_ttl=%d\n", pdata->r_ttl);

	ret = of_property_read_u32(np_sm5461, "sm5461,freq", &pdata->freq);
	if (ret) {
		dev_info(dev, "%s: sm5461,freq is Empty\n", __func__);
		pdata->freq = SM5461_FREQ_750KHZ;
	}
	dev_info(dev, "parse_dt: freq=%dkHz\n", SM5461_freq_val[pdata->freq]);

	ret = of_property_read_u32(np_sm5461, "sm5461,freq_byp", &pdata->freq_byp);
	if (ret) {
		dev_info(dev, "%s: sm5461,freq_byp is Empty\n", __func__);
		pdata->freq_byp = SM5461_FREQ_375KHZ;
	}
	dev_info(dev, "parse_dt: freq_byp=%dkHz\n", SM5461_freq_val[pdata->freq_byp]);

	ret = of_property_read_u32(np_sm5461, "sm5461,freq_fpdo", &pdata->freq_fpdo);
	if (ret) {
		dev_info(dev, "%s: sm5461,freq_fpdo is Empty\n", __func__);
		pdata->freq_fpdo = SM5461_FREQ_750KHZ;
	}
	dev_info(dev, "parse_dt: freq_fpdo=%dkHz\n", SM5461_freq_val[pdata->freq_fpdo]);

	ret = of_property_read_u32_array(np_sm5461, "sm5461,freq_siop", pdata->freq_siop, 2);
	if (ret) {
		dev_info(dev, "%s: sm5461,freq_siop is Empty\n", __func__);
		pdata->freq_siop[0] = SM5461_FREQ_200KHZ;
		pdata->freq_siop[1] = SM5461_FREQ_375KHZ;
	}
	dev_info(dev, "parse_dt: freq_siop=%dkHz, %dkHz\n",
		SM5461_freq_val[pdata->freq_siop[0]], SM5461_freq_val[pdata->freq_siop[1]]);

	ret = of_property_read_u32(np_sm5461, "sm5461,topoff", &pdata->topoff);
	if (ret) {
		dev_info(dev, "%s: sm5461,topoff is Empty\n", __func__);
		pdata->topoff = 900;
	}
	dev_info(dev, "parse_dt: topoff=%d\n", pdata->topoff);

	ret = of_property_read_u32(np_sm5461, "sm5461,x2bat_mode", &pdata->x2bat_mode);
	if (ret) {
		dev_info(dev, "%s: sm5461,x2bat_mode is Empty\n", __func__);
		pdata->x2bat_mode = 0;
	}
	dev_info(dev, "parse_dt: x2bat_mode=%d\n", pdata->x2bat_mode);

	ret = of_property_read_u32(np_sm5461, "sm5461,en_vbatreg", &pdata->en_vbatreg);
	if (ret) {
		dev_info(dev, "%s: sm5461,en_vbatreg is Empty\n", __func__);
		pdata->en_vbatreg = 0;
	}
	dev_info(dev, "parse_dt: en_vbatreg=%d\n", pdata->en_vbatreg);

	ret = of_property_read_u32(np_sm5461, "sm5461,snsr", &pdata->snsr);
	if (ret) {
		dev_info(dev, "%s: sm5461,snsr is Empty\n", __func__);
		pdata->snsr = 0;
	}
	dev_info(dev, "parse_dt: snsr=%d\n", pdata->snsr);

	ret = of_property_read_u32(np_sm5461, "sm5461,single_mode", &pdata->single_mode);
	if (ret) {
		dev_info(dev, "%s: sm5461,single_mode is Empty\n", __func__);
		pdata->single_mode = 0;
	}
	dev_info(dev, "parse_dt: single_mode=%d\n", pdata->single_mode);

	ret = of_property_read_u32(np_sm5461, "sm5461,fpdo_topoff", &pdata->fpdo_topoff);
	if (ret) {
		pr_info("%s: sm5461,fpdo_topoff is Empty\n", __func__);
		pdata->fpdo_topoff = 1500000; /* 1500mA */
	}
	pr_info("%s: sm5461,fpdo_topoff is %d\n", __func__, pdata->fpdo_topoff);

	ret = of_property_read_u32(np_sm5461, "sm5461,fpdo_mainvbat_reg", &pdata->fpdo_mainvbat_reg);
	if (ret) {
		pr_info("%s: sm5461,fpdo_mainvbat_reg is Empty\n", __func__);
		pdata->fpdo_mainvbat_reg = 4230000; /* 4230mV */
	}
	pr_info("%s: sm5461,fpdo_mainvbat_reg is %d\n", __func__, pdata->fpdo_mainvbat_reg);

	ret = of_property_read_u32(np_sm5461, "sm5461,fpdo_subvbat_reg", &pdata->fpdo_subvbat_reg);
	if (ret) {
		pr_info("%s: sm5461,fpdo_subvbat_reg is Empty\n", __func__);
		pdata->fpdo_subvbat_reg = 4200000; /* 4200mV */
	}
	pr_info("%s: sm5461,fpdo_subvbat_reg is %d\n", __func__, pdata->fpdo_subvbat_reg);

	ret = of_property_read_u32(np_sm5461, "sm5461,fpdo_vnow_reg", &pdata->fpdo_vnow_reg);
	if (ret) {
		pr_info("%s: sm5461,fpdo_vnow_reg is Empty\n", __func__);
		pdata->fpdo_vnow_reg = 5000000; /* 5000mV means disable */
	}
	pr_info("%s: sm5461,fpdo_vnow_reg is %d\n", __func__, pdata->fpdo_vnow_reg);


	/* Parse: battery node */
	np_battery = of_find_node_by_name(NULL, "battery");
	if (!np_battery) {
		dev_err(dev, "%s: empty of_node for battery\n", __func__);
		return -EINVAL;
	}
	ret = of_property_read_u32(np_battery, "battery,chg_float_voltage",
		&pdata->battery.chg_float_voltage);
	if (ret) {
		dev_info(dev, "%s: battery,chg_float_voltage is Empty\n", __func__);
		pdata->battery.chg_float_voltage = 4200;
	}
	ret = of_property_read_string(np_battery, "battery,charger_name",
		(char const **)&pdata->battery.sec_dc_name);
	if (ret) {
		dev_info(dev, "%s: battery,charger_name is Empty\n", __func__);
		pdata->battery.sec_dc_name = "sec-direct-charger";
	}
	ret = of_property_read_string(np_battery, "battery,fuelgauge_name",
		(char const **)&pdata->battery.fuelgauge_name);
	if (ret) {
		dev_info(dev, "%s: battery,fuelgauge_name is Empty\n", __func__);
		pdata->battery.fuelgauge_name = "sec-fuelgauge";
	}
	ret = of_property_read_u32(np_battery, "battery,fpdo_dc_charge_power",
		&pdata->battery.fpdo_chg_curr);
	pdata->battery.fpdo_chg_curr /= 9; /* power to current */
	if (ret) {
		dev_info(dev, "%s: battery,fpdo_dc_charge_power is Empty\n", __func__);
		pdata->battery.fpdo_chg_curr = 10000; /* 10A */
	}

	dev_info(dev,
	"parse_dt: float_v=%d, sec_dc_name=%s, fuelgauge_name=%s, fpdo_chg_curr=%dmA\n",
	pdata->battery.chg_float_voltage, pdata->battery.sec_dc_name,
	pdata->battery.fuelgauge_name, pdata->battery.fpdo_chg_curr);

	return 0;
}
#endif  /* CONFIG_OF */

enum {
	ADDR = 0,
	SIZE,
	DATA,
	UPDATE,
	TA_MIN,
};
static ssize_t chg_show_attrs(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t chg_store_attrs(struct device *dev,
	struct device_attribute *attr,
const char *buf, size_t count);
#define CHARGER_ATTR(_name)				\
{							\
	.attr = {.name = #_name, .mode = 0660},	\
	.show = chg_show_attrs,			\
	.store = chg_store_attrs,			\
}
static struct device_attribute charger_attrs[] = {
	CHARGER_ATTR(addr),
	CHARGER_ATTR(size),
	CHARGER_ATTR(data),
	CHARGER_ATTR(update),
	CHARGER_ATTR(ta_min_v),
};
static int chg_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < (int)ARRAY_SIZE(charger_attrs); i++) {
		rc = device_create_file(dev, &charger_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	return rc;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &charger_attrs[i]);
	return rc;
}

static ssize_t chg_show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sm5461_charger *sm5461 = dev_get_drvdata(dev->parent);
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5461);
	const ptrdiff_t offset = attr - charger_attrs;
	int i = 0;
	u8 addr;
	u8 val;

	switch (offset) {
	case ADDR:
		i += sprintf(buf, "0x%x\n", sm5461->addr);
		break;
	case SIZE:
		i += sprintf(buf, "0x%x\n", sm5461->size);
		break;
	case DATA:
		for (addr = sm5461->addr; addr < (sm5461->addr+sm5461->size); addr++) {
			sm5461_read_reg(sm5461, addr, &val);
			i += scnprintf(buf + i, PAGE_SIZE - i,
				"0x%04x : 0x%02x\n", addr, val);
		}
		break;
	case TA_MIN:
		i += sprintf(buf, "%d\n", sm_dc->config.ta_min_voltage);
		break;
	default:
		return -EINVAL;
	}
	return i;
}

static ssize_t chg_store_attrs(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sm5461_charger *sm5461 = dev_get_drvdata(dev->parent);
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5461);
	const ptrdiff_t offset = attr - charger_attrs;
	int ret = 0;
	int x, y, z, k;

	switch (offset) {
	case ADDR:
		if (sscanf(buf, "0x%4x\n", &x) == 1)
			sm5461->addr = x;
		ret = count;
		break;
	case SIZE:
		if (sscanf(buf, "%5d\n", &x) == 1)
			sm5461->size = x;
		ret = count;
		break;
	case DATA:
		if (sscanf(buf, "0x%8x 0x%8x", &x, &y) == 2) {
			if ((x >= SM5461_REG_CNTL1 && x <= SM5461_REG_REG1) ||
				(x >= SM5461_REG_ADC_CNTL && x <= SM5461_REG_TOPOFF)) {
				u8 addr = x;
				u8 data = y;

				if (sm5461_write_reg(sm5461, addr, data) < 0) {
					dev_info(sm5461->dev,
					"%s: addr: 0x%x write fail\n", __func__, addr);
				}
			} else {
				dev_info(sm5461->dev,
				"%s: addr: 0x%x is wrong\n", __func__, x);
			}
		}
		ret = count;
		break;
	case UPDATE:
		if (sscanf(buf, "0x%8x 0x%8x 0x%8x %d", &x, &y, &z, &k) == 4) {
			if ((x >= SM5461_REG_CNTL1 && x <= SM5461_REG_REG1) ||
				(x >= SM5461_REG_ADC_CNTL && x <= SM5461_REG_TOPOFF)) {
				u8 addr = x, data = y, val = z, pos = k;

				if (sm5461_update_reg(sm5461, addr, data, val, pos)) {
					dev_info(sm5461->dev,
					"%s: addr: 0x%x write fail\n", __func__, addr);
				}
			} else {
				dev_info(sm5461->dev,
				"%s: addr: 0x%x is wrong\n", __func__, x);
			}
		}
		ret = count;
		break;
	case TA_MIN:
		if (sscanf(buf, "%5d\n", &x) == 1)
			sm_dc->config.ta_min_voltage = x;
		ret = count;
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int sm5461_dbg_read_reg(void *data, u64 *val)
{
	struct sm5461_charger *sm5461 = data;
	int ret;
	u8 reg;

	ret = sm5461_read_reg(sm5461, sm5461->debug_address, &reg);
	if (ret < 0) {
		dev_err(sm5461->dev, "%s: failed read 0x%02x\n",
			__func__, sm5461->debug_address);
		return ret;
	}
	*val = reg;

	return 0;
}

static int sm5461_dbg_write_reg(void *data, u64 val)
{
	struct sm5461_charger *sm5461 = data;
	int ret;

	ret = sm5461_write_reg(sm5461, sm5461->debug_address, (u8)val);
	if (ret < 0) {
		dev_err(sm5461->dev, "%s: failed write 0x%02x to 0x%02x\n",
			__func__, (u8)val, sm5461->debug_address);
		return ret;
	}

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(register_debug_ops, sm5461_dbg_read_reg,
	sm5461_dbg_write_reg, "0x%02llx\n");

static int sm5461_create_debugfs_entries(struct sm5461_charger *sm5461)
{
	struct dentry *ent;

	sm5461->debug_root = debugfs_create_dir("charger-sm5461", NULL);
	if (!sm5461->debug_root) {
		dev_err(sm5461->dev, "%s: can't create dir\n", __func__);
		return -ENOENT;
	}

	debugfs_create_x32("address", 0644,
		sm5461->debug_root, &(sm5461->debug_address));

	ent = debugfs_create_file("data", 0644,
		sm5461->debug_root, sm5461, &register_debug_ops);
	if (!ent) {
		dev_err(sm5461->dev, "%s: can't create data\n", __func__);
		return -ENOENT;
	}

	return 0;
}

static const struct i2c_device_id sm5461_charger_id_table[] = {
	{ "sm5461-charger", .driver_data = SM5461_MAIN },
	{ "sm5461-charger-sub", .driver_data =  SM5461_SUB},
	{ }
};
MODULE_DEVICE_TABLE(i2c, sm5461_charger_id_table);

#if defined(CONFIG_OF)
static const struct of_device_id sm5461_of_match_table[] = {
	{ .compatible = "siliconmitus,sm5461", .data = (void *)SM5461_MAIN },
	{ .compatible = "siliconmitus,sm5461-sub", .data = (void *)SM5461_SUB },
	{ },
};
MODULE_DEVICE_TABLE(of, sm5461_of_match_table);
#endif /* CONFIG_OF */

static int sm5461_charger_probe(struct i2c_client *i2c,
			const struct i2c_device_id *id)
{
	struct sm5461_charger *sm5461;
	struct sm_dc_info *pps_dc, *x2bat_dc;
	struct sm5461_platform_data *pdata;
	struct power_supply_config psy_cfg = {};
	const struct of_device_id *of_id;
	int ret, chip;

	dev_info(&i2c->dev, "%s: probe start\n", __func__);

	of_id = of_match_device(sm5461_of_match_table, &i2c->dev);
	if (!of_id) {
		dev_info(&i2c->dev, "sm5461-Charger matching on node name, compatible is preferred\n");
		chip = (enum sm5461_chip_id)id->driver_data;
	} else {
		chip = (enum sm5461_chip_id)of_id->data;
	}
	dev_info(&i2c->dev, "%s: chip:%d\n", __func__, chip);

	sm5461 = devm_kzalloc(&i2c->dev, sizeof(struct sm5461_charger), GFP_KERNEL);
	if (!sm5461)
		return -ENOMEM;

#if defined(CONFIG_OF)
	pdata = devm_kzalloc(&i2c->dev, sizeof(struct sm5461_platform_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	ret = sm5461_charger_parse_dt(&i2c->dev, pdata);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: fail to parse_dt\n", __func__);
		pdata = NULL;
	}
#else   /* CONFIG_OF */
	pdata = NULL;
#endif  /* CONFIG_OF */
	if (!pdata) {
		dev_err(&i2c->dev, "%s: we didn't support fixed platform_data yet\n", __func__);
		return -EINVAL;
	}

	/* create sm direct-charging instance for PD3.0(PPS) */
	if (chip == SM5461_SUB)
		pps_dc = sm_dc_create_pd_instance("sm5461-sub-DC", i2c);
	else
		pps_dc = sm_dc_create_pd_instance("sm5461-PD-DC", i2c);
	if (IS_ERR(pps_dc)) {
		dev_err(&i2c->dev, "%s: fail to create PD-DC module\n", __func__);
		return -ENOMEM;
	}
	pps_dc->config.ta_min_current = SM5461_TA_MIN_CURRENT;
	pps_dc->config.ta_min_voltage = 8200;
	pps_dc->config.dc_min_vbat = 3100;
	pps_dc->config.dc_vbus_ovp_th = 11000;
	pps_dc->config.r_ttl = pdata->r_ttl;
	pps_dc->config.topoff_current = pdata->topoff;
	pps_dc->config.fpdo_topoff = pdata->fpdo_topoff;
	pps_dc->config.fpdo_mainvbat_reg = pdata->fpdo_mainvbat_reg;
	pps_dc->config.fpdo_subvbat_reg = pdata->fpdo_subvbat_reg;
	pps_dc->config.fpdo_vnow_reg = pdata->fpdo_vnow_reg;
	pps_dc->config.fpdo_chg_curr = pdata->battery.fpdo_chg_curr;
	pps_dc->config.need_to_sw_ocp = 0;
	pps_dc->config.support_pd_remain = 1; /* if pdic can't support PPS remaining, plz activate it. */
	pps_dc->config.chg_float_voltage = pdata->battery.chg_float_voltage;
	pps_dc->config.sec_dc_name = pdata->battery.sec_dc_name;
	pps_dc->ops = &sm5461_dc_pps_ops;
	pps_dc->chip_id = chip;
	ret = sm_dc_verify_configuration(pps_dc);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: fail to verify sm_dc(ret=%d)\n", __func__, ret);
		goto err_devmem;
	}
	sm5461->pps_dc = pps_dc;

	if (pdata->x2bat_mode) {
		if (chip == SM5461_SUB)
			x2bat_dc = sm_dc_create_x2bat_instance("sm5461-x2sub-DC", i2c);
		else
			x2bat_dc = sm_dc_create_x2bat_instance("sm5461-x2PD-DC", i2c);
		if (IS_ERR(x2bat_dc)) {
			dev_err(&i2c->dev, "%s: fail to create PD-DC module\n", __func__);
			return -ENOMEM;
		}
		x2bat_dc->config.ta_min_current = SM5461_TA_MIN_CURRENT;
		x2bat_dc->config.ta_min_voltage = 8200;
		x2bat_dc->config.dc_min_vbat = 3100;
		x2bat_dc->config.dc_vbus_ovp_th = 11000;
		x2bat_dc->config.r_ttl = pdata->r_ttl;
		x2bat_dc->config.topoff_current = pdata->topoff;
		x2bat_dc->config.need_to_sw_ocp = 0;
		x2bat_dc->config.support_pd_remain = 1; /* if pdic can't support PPS remaining, plz activate it. */
		x2bat_dc->config.chg_float_voltage = pdata->battery.chg_float_voltage;
		x2bat_dc->config.sec_dc_name = pdata->battery.sec_dc_name;
		x2bat_dc->ops = &sm5461_x2bat_dc_pps_ops;
		x2bat_dc->chip_id = chip;
		ret = sm_dc_verify_configuration(x2bat_dc);
		if (ret < 0) {
			dev_err(&i2c->dev, "%s: fail to verify sm_dc(ret=%d)\n", __func__, ret);
			goto err_devmem;
		}
	} else {
		x2bat_dc = NULL;
	}
	sm5461->x2bat_dc = x2bat_dc;

	sm5461->dev = &i2c->dev;
	sm5461->i2c = i2c;
	sm5461->pdata = pdata;
	sm5461->chip_id = chip;
	mutex_init(&sm5461->i2c_lock);
	mutex_init(&sm5461->pd_lock);
	sm5461->chg_ws = wakeup_source_register(&i2c->dev, "sm5461-charger");
	i2c_set_clientdata(i2c, sm5461);

	ret = sm5461_hw_init_config(sm5461);
	if (ret < 0) {
		dev_err(sm5461->dev, "%s: fail to init config(ret=%d)\n", __func__, ret);
		goto err_devmem;
	}

	psy_cfg.drv_data = sm5461;
	psy_cfg.supplied_to = sm5461_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(sm5461_supplied_to);
	if (chip == SM5461_SUB)
		sm5461->psy_chg = power_supply_register(sm5461->dev,
			&sm5461_charger_power_supply_sub_desc, &psy_cfg);
	else
		sm5461->psy_chg = power_supply_register(sm5461->dev,
			&sm5461_charger_power_supply_desc, &psy_cfg);
	if (IS_ERR(sm5461->psy_chg)) {
		dev_err(sm5461->dev, "%s: fail to register psy_chg\n", __func__);
		ret = PTR_ERR(sm5461->psy_chg);
		goto err_devmem;
	}

	if (sm5461->pdata->irq_gpio >= 0) {
		ret = sm5461_irq_init(sm5461);
		if (ret < 0) {
			dev_err(sm5461->dev, "%s: fail to init irq(ret=%d)\n", __func__, ret);
			goto err_psy_chg;
		}
	} else {
		dev_warn(sm5461->dev, "%s: didn't assigned irq_gpio\n", __func__);
	}

	ret = chg_create_attrs(&sm5461->psy_chg->dev);
	if (ret)
		dev_err(sm5461->dev, "%s : Failed to create_attrs\n", __func__);

	ret = sm5461_create_debugfs_entries(sm5461);
	if (ret < 0) {
		dev_err(sm5461->dev, "%s: fail to create debugfs(ret=%d)\n", __func__, ret);
		goto err_psy_chg;
	}

	sec_chg_set_dev_init(SC_DEV_DIR_CHG);

	dev_info(sm5461->dev, "%s: done. (rev_id=0x%x)[%s]\n", __func__,
		sm5461->pdata->rev_id, SM5461_DC_VERSION);

	return 0;

err_psy_chg:
	power_supply_unregister(sm5461->psy_chg);

err_devmem:
	mutex_destroy(&sm5461->i2c_lock);
	mutex_destroy(&sm5461->pd_lock);
	wakeup_source_unregister(sm5461->chg_ws);
	sm_dc_destroy_instance(sm5461->pps_dc);
	sm_dc_destroy_instance(sm5461->x2bat_dc);

	return ret;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
static int sm5461_charger_remove(struct i2c_client *i2c)
#else
static void sm5461_charger_remove(struct i2c_client *i2c)
#endif
{
	struct sm5461_charger *sm5461 = i2c_get_clientdata(i2c);

	sm5461_stop_charging(sm5461);
	sm5461_sw_reset(sm5461);

	power_supply_unregister(sm5461->psy_chg);
	mutex_destroy(&sm5461->i2c_lock);
	mutex_destroy(&sm5461->pd_lock);
	wakeup_source_unregister(sm5461->chg_ws);
	sm_dc_destroy_instance(sm5461->pps_dc);
	sm_dc_destroy_instance(sm5461->x2bat_dc);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
	return 0;
#endif
}

static void sm5461_charger_shutdown(struct i2c_client *i2c)
{
	struct sm5461_charger *sm5461 = i2c_get_clientdata(i2c);

	sm5461_stop_charging(sm5461);
	sm5461_reverse_boost_enable(sm5461, 0);
}

#if defined(CONFIG_PM)
static int sm5461_charger_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5461_charger *sm5461 = i2c_get_clientdata(i2c);

	if (device_may_wakeup(dev))
		enable_irq_wake(sm5461->irq);

	disable_irq(sm5461->irq);

	return 0;
}

static int sm5461_charger_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5461_charger *sm5461 = i2c_get_clientdata(i2c);

	if (device_may_wakeup(dev))
		disable_irq_wake(sm5461->irq);

	enable_irq(sm5461->irq);

	return 0;
}
#else   /* CONFIG_PM */
#define sm5461_charger_suspend		NULL
#define sm5461_charger_resume		NULL
#endif  /* CONFIG_PM */

const struct dev_pm_ops sm5461_pm_ops = {
	.suspend = sm5461_charger_suspend,
	.resume = sm5461_charger_resume,
};

static struct i2c_driver sm5461_charger_driver = {
	.driver = {
		.name = "sm5461-charger",
		.owner	= THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table = sm5461_of_match_table,
#endif  /* CONFIG_OF */
#if defined(CONFIG_PM)
		.pm = &sm5461_pm_ops,
#endif  /* CONFIG_PM */
	},
	.probe        = sm5461_charger_probe,
	.remove       = sm5461_charger_remove,
	.shutdown     = sm5461_charger_shutdown,
	.id_table     = sm5461_charger_id_table,
};

static int __init sm5461_i2c_init(void)
{
	pr_info("sm5461-charger: %s\n", __func__);
	return i2c_add_driver(&sm5461_charger_driver);
}
module_init(sm5461_i2c_init);

static void __exit sm5461_i2c_exit(void)
{
	i2c_del_driver(&sm5461_charger_driver);
}
module_exit(sm5461_i2c_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SiliconMitus <hwangjoo.jang@SiliconMitus.com>");
MODULE_DESCRIPTION("Charger driver for SM5461");
MODULE_VERSION(SM5461_DC_VERSION);
