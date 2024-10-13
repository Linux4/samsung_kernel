/*
 * sy65153 battery charging driver
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

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
//#include <linux/hardware_info.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <asm/unaligned.h>
#include <linux/pinctrl/consumer.h>
#include <linux/iio/consumer.h>
#ifdef CONFIG_WT_QGKI
#include <linux/hardware_info.h>
#endif /*CONFIG_WT_QGKI*/
#include "sy65153_wl_charger.h"
#include "sy65153_wl_charger_iio.h"

#define CHARGER_IC_NAME "SY65153"

#define SY65153_WAKE_UP_MS			3000
#define SY65153_PG_INT_WORK_MS		msecs_to_jiffies(200)

static DEFINE_MUTEX(sy65153_i2c_lock);

static struct sy_wls_chrg_chip *g_sy;

static struct sy65153_reg_tab reg_tab[SY65153_REG_NUM + 1] = {
	{0, SY65153_REG_00, "Identification Part_number_L reg"},
	{1, SY65153_REG_01, "Identification Part_number_H reg"},
	{2, SY65153_REG_04, "Revision FW_Major_Rev_L reg"},
	{3, SY65153_REG_05, "Revision FW_Major_Rev_H reg"},
	{4, SY65153_REG_06, "Revision FW_Minor_Rev_L reg"},
	{5, SY65153_REG_07, "Revision FW_Minor_Rev_H reg"},
	{6, SY65153_REG_34, "status reg"},
	{7, SY65153_REG_35, "Reserved reg"},
	{8, SY65153_REG_3A, "Battery Charge Status reg"},
	{9, SY65153_REG_3B, "End Power Transfer reg"},
	{10, SY65153_REG_3C, "Output Voltage ADC_VOUT_L reg"},
	{11, SY65153_REG_3D, "Output Voltage ADC_VOUT_H reg"},
	{12, SY65153_REG_3E, "Setting Output Voltage reg"},
	{13, SY65153_REG_40, "VRECT Voltage ADC_VRECT_L reg"},
	{14, SY65153_REG_41, "VRECT Voltage ADC_VRECT_H reg"},
	{15, SY65153_REG_44, "Iout Current RX_IOUT_L reg"},
	{16, SY65153_REG_45, "Iout Current RX_IOUT_H reg"},
	{17, SY65153_REG_46, "Die Temperature ADC_Die_Temp_L reg"},
	{18, SY65153_REG_47, "Die Temperature ADC_Die_Temp_H reg"},
	{19, SY65153_REG_4A, "RX_ILIM reg"},
	{20, SY65153_REG_4E, "Command reg"},
	{21, SY65153_REG_56, "Q-factorreg"},
	{22, SY65153_REG_68, "FOD Calibration I2C_RPPO reg"},
	{23, SY65153_REG_69, "FOD Calibration I2C_RPPG reg"},
	{24, SY65153_REG_6E, "10W protocol enable reg"},
	{25, 0, "null"},
};

static int sy65153_i2c_read_byte(struct sy_wls_chrg_chip *sy_chip, u16 reg, bool single)
{
	struct i2c_client *client = to_i2c_client(sy_chip->dev);
	struct i2c_msg msg[2];
	u8 addr[2] = {reg>>8&0xff, reg&0xff};
	u8 data[2];
	int ret;

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].buf = addr;
	msg[0].len = 2;
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = data;
	if (single)
		msg[1].len = 1;
	else
		msg[1].len = 2;

	mutex_lock(&sy65153_i2c_lock);
	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	mutex_unlock(&sy65153_i2c_lock);
	if (ret < 0)
		return ret;

	if (!single)
		ret = get_unaligned_le16(data);
	else
		ret = data[0];

	return ret;
}

static int sy65153_i2c_write_byte(struct sy_wls_chrg_chip *sy_chip, u8 reg, u8 value)
{
	struct i2c_client *client = to_i2c_client(sy_chip->dev);
	struct i2c_msg msg;
	u8 data[3] = {reg>>8&0xff, reg&0xff, value};
	int ret;

	if (!client->adapter)
		return -ENODEV;

  	msg.addr = client->addr;
  	msg.flags = 0;
  	msg.buf = data;
  	msg.len = 3;
  
  	mutex_lock(&sy65153_i2c_lock);
  	ret = i2c_transfer(client->adapter, &msg, 1);
  	mutex_unlock(&sy65153_i2c_lock);

  	return ret;
}

static int sy65153_i2c_bulk_write(struct sy_wls_chrg_chip *sy_chip,
					  u8 reg, u8 *data, int len)
{
	struct i2c_client *client = to_i2c_client(sy_chip->dev);
	struct i2c_msg msg;
	u8 buf[33];
	int ret;

	if (!client->adapter)
		return -ENODEV;

	buf[0] = reg>>8&0xff;
	buf[1] = reg&0xff;
	memcpy(&buf[2], data, len);
	msg.buf = buf;
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = len + 2;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
		return ret;
	if (ret != 1)
		return -EINVAL;

	return 0;
}

static void sy65153_stay_awake(struct sy65153_wakeup_source *source)
{
	if (__test_and_clear_bit(0, &source->disabled)) {
		__pm_stay_awake(source->source);
		pr_debug("enabled source %s\n", source->source->name);
	}
}

static void sy65153_relax(struct sy65153_wakeup_source *source)
{
	if (!__test_and_set_bit(0, &source->disabled)) {
		__pm_relax(source->source);
		pr_debug("disabled source %s\n", source->source->name);
	}
}

static bool sy65153_wake_active(struct sy65153_wakeup_source *source)
{
	return !source->disabled;
}

int sy65153_get_id(struct sy_wls_chrg_chip *sy)
{
	int ret=0;

	if(gpio_get_value(sy->power_good_gpio)){
		ret = sy65153_i2c_read_byte(sy, SY65153_REG_00, 0);
		if(ret<0){
			ret=0;
			sy_wls_log(SY_LOG_ERR, "Cannot get wireless ic id \n");
		}
	}
	sy_wls_log(SY_LOG_ERR, "id =%d \n", ret);

	return ret;
}

struct iio_channel ** sy65153_get_ext_channels(struct device *dev,
		 const char *const *channel_map, int size)
{
	int i, rc = 0;
	struct iio_channel **iio_ch_ext;

	iio_ch_ext = devm_kcalloc(dev, size, sizeof(*iio_ch_ext), GFP_KERNEL);
	if (!iio_ch_ext)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < size; i++) {
		iio_ch_ext[i] = devm_iio_channel_get(dev, channel_map[i]);

		if (IS_ERR(iio_ch_ext[i])) {
			rc = PTR_ERR(iio_ch_ext[i]);
			if (rc != -EPROBE_DEFER)
				dev_err(dev, "%s channel unavailable, %d\n",
						channel_map[i], rc);
			return ERR_PTR(rc);
		}
	}

	return iio_ch_ext;
}

static bool  sy65153_is_wtchg_chan_valid(struct sy_wls_chrg_chip *chg,
		enum wtcharge_iio_channels chan)
{
	int rc;
	struct iio_channel **iio_list;

	if (!chg->wtchg_ext_iio_chans) {
		iio_list = sy65153_get_ext_channels(chg->dev, wtchg_iio_chan_name,
		ARRAY_SIZE(wtchg_iio_chan_name));
		if (IS_ERR(iio_list)) {
			rc = PTR_ERR(iio_list);
			if (rc != -EPROBE_DEFER) {
				dev_err(chg->dev, "Failed to get channels, %d\n",
					rc);
				chg->wtchg_ext_iio_chans = NULL;
			}
			return false;
		}
		chg->wtchg_ext_iio_chans = iio_list;
	}

	return true;
}

static int sy65153_read_iio_prop(struct sy_wls_chrg_chip *sy,
				int channel, int *val)
{
	struct iio_channel *iio_chan_list;
	int rc;

	if (!sy65153_is_wtchg_chan_valid(sy, channel)) {
		return -ENODEV;
	}

	iio_chan_list = sy->wtchg_ext_iio_chans[channel];
	rc = iio_read_channel_processed(iio_chan_list, val);

	return rc < 0 ? rc : 0;
}

static int sy65153_write_iio_prop(struct sy_wls_chrg_chip *sy,
				int channel, int val)
{
	struct iio_channel *iio_chan_list;
	int ret;

	if (!sy65153_is_wtchg_chan_valid(sy, channel)) {
		return -ENODEV;
	}

	iio_chan_list = sy->wtchg_ext_iio_chans[channel];
	ret = iio_write_channel_raw(iio_chan_list, val);

	return ret < 0 ? ret : 0;
}

static void sy65153_wl_charger_dump_register(struct sy_wls_chrg_chip *sy)
{
	int i, len, idx = 0;
	int reg_val;
	char buf[512];

	memset(buf, '\0', sizeof(buf));
	for (i = 0; i < SY65153_REG_NUM; i++) {
		reg_val = sy65153_i2c_read_byte(sy, reg_tab[i].addr, 1);
		len = snprintf(buf + idx, sizeof(buf) - idx,
				 "[REG_0x%.2x]=0x%.2x;\n", reg_tab[i].addr, reg_val);
		idx += len;
	}

	sy_wls_log(SY_LOG_NONE, "%s: %s", __func__, buf);
}

static int sy65153_get_epp_mode(struct sy_wls_chrg_chip *sy)
{
	int ret=0;

	if(gpio_get_value(sy->power_good_gpio)){
		ret = sy65153_i2c_read_byte(sy, SY65153_REG_4B, 1);
		if(ret<0){
			ret=0;
			sy_wls_log(SY_LOG_ERR, "Cannot get wireless epp mode \n");
		}
	}

	return ret/2;
}

static int sy65153_get_wireless_type(struct sy_wls_chrg_chip *sy)
{
	int ret=0;

	ret = sy65153_i2c_read_byte(sy, SY65153_REG_34, 1);
	if(ret<0){
		sy_wls_log(SY_LOG_ERR, "Cannot get wireless type \n");
		return ret;
	}

	sy->wireless_type = ((ret & SY65153_HANDSHAKE_PROTOCOL_MASK)
							>> SY65153_HANDSHAKE_PROTOCOL_SHIFT);

	return sy->wireless_type;
}

static int sy65153_get_vout(struct sy_wls_chrg_chip *sy)
{
	int vout_l = 0;
	int vout_h = 0;
	int vout = 0;

	vout_l = sy65153_i2c_read_byte(sy, SY65153_REG_3C, 1);
	vout_h = sy65153_i2c_read_byte(sy, SY65153_REG_3D, 1);
	vout_h = (u8)vout_h & SY65153_VOUT_HIGH_BYTE_MASK;
	vout = ((vout_h << 8) | (u8)vout_l);//* 6 * 2.1 / 4095;
	vout = vout * 6 * 21 / 10 / 4095 * 1000;

	return vout;
}

static int sy65153_get_charger_online(struct sy_wls_chrg_chip *sy)
{
	int ret = 0;

	if(sy->power_good_gpio){
		ret = gpio_get_value(sy->power_good_gpio);
	}

	return ret;
}

static int sy65153_get_charger_present(struct sy_wls_chrg_chip *sy)
{
	int ret = 0;

	ret = sy65153_get_id(sy);

	if(ret != SY_ID)
		ret = 0;
	else
		ret = 1;

	return ret;
}

static int sy65153_get_charger_enable(struct sy_wls_chrg_chip *sy)
{
	int ret=0;
	ret = gpio_get_value(sy->power_good_gpio);
	if(ret<0){
		sy_wls_log(SY_LOG_ERR, "Cannot get power_good_gpio pin state \n");
		return 0;
	}

	return ret;
}

static int sy65153_get_wtchg_otg_status(struct sy_wls_chrg_chip *sy)
{
	int ret;
	int val;

	ret = sy65153_read_iio_prop(sy, WTCHG_OTG_ENABLE, &val);
	if (ret < 0) {
		dev_err(sy->dev, "%s: fail: %d\n", __func__, ret);
		return 0;
	}

	return val;
}

static void sy65153_set_reset_pd_chip_ic(struct sy_wls_chrg_chip *sy)
{
	int ret;

	ret = sy65153_write_iio_prop(sy, LOGIC_IC_RESET, WL);
	if (ret < 0) {
		dev_err(sy->dev, "%s: fail: %d\n", __func__, ret);
	}
}

static int sy65153_set_charger_enable(struct sy_wls_chrg_chip *sy, bool en)
{
	gpio_direction_output(sy->sleep_mode_gpio, !en);
	msleep(5);

	return 0;
}

static int sy65153_set_vout(struct sy_wls_chrg_chip *sy, int volt)
{
	int ret=0;
	int epp_mode=0;
	u8 reg_get;
	u8 reg_set;

	if(!sy65153_wake_active(&sy->sy65153_wake_source)){
		sy65153_stay_awake(&sy->sy65153_wake_source);
	}

	if (volt > SY65153_VOUT_MAX)
		volt = SY65153_VOUT_MAX;
	else if (volt < SY65153_VOUT_MIN)
		volt = SY65153_VOUT_MIN;

	if (SY65153_VOUT_MAX == volt) {
		epp_mode = sy65153_get_epp_mode(sy);
		if(epp_mode)
			volt = SY65153_VOUT_9V;
	}

	reg_set = ((volt - SY65153_VOUT_MIN) / SY65153_VOUT_STEP);
	reg_get = sy65153_i2c_read_byte(sy, SY65153_REG_3E, 1);
	if (reg_get != reg_set) {
		ret = sy65153_i2c_write_byte(sy, SY65153_REG_3E, reg_set);
		if (ret < 0) {
			sy_wls_log(SY_LOG_ERR, "i2c write error\n");
		}
	}

	volt = sy65153_get_vout(sy);
	sy_wls_log(SY_LOG_ERR, "epp_mode = %d syvout = %d reg_get=%x reg_set=%x\n",epp_mode, volt, reg_get, reg_set);

	if(sy65153_wake_active(&sy->sy65153_wake_source)){
		sy65153_relax(&sy->sy65153_wake_source);
	}

	return ret;
}

static void sy65153_set_rx_params(struct sy_wls_chrg_chip *sy)
{
	u8 data[2]={0,0};
	int reg_56_value, reg_68_value, reg_69_value;

	// Configure Q-factor value, note that the value need to be updated based on IOT laboratory test result
	sy65153_i2c_write_byte(sy, SY65153_REG_56, Q_FACTOR_VALUE);
	reg_56_value = sy65153_i2c_read_byte(sy, SY65153_REG_56, 1);
	if (reg_56_value != Q_FACTOR_VALUE) {
		msleep(100);
		sy65153_i2c_write_byte(sy, SY65153_REG_56, Q_FACTOR_VALUE);
	}
	// Disable 10W protocol: 0x01 is disable; default =0x00 is enable
	sy65153_i2c_write_byte(sy, SY65153_REG_6E, DISABLE_10W_MODE);

	// Config FOD value
	data[0] = FOD_68_VALUE;
	data[1] = FOD_69_VALUE;
	sy65153_i2c_bulk_write(sy, SY65153_REG_68, data, 2);
	reg_68_value = sy65153_i2c_read_byte(sy, SY65153_REG_68, 1);
	reg_69_value = sy65153_i2c_read_byte(sy, SY65153_REG_69, 1);
	if (reg_68_value != FOD_68_VALUE || reg_69_value != FOD_69_VALUE) {
		sy65153_i2c_bulk_write(sy, SY65153_REG_68, data, 2);
	}

}

static void sy65153_dynamic_set_rx_params(struct sy_wls_chrg_chip *sy, int dynamic_fod)
{
	u8 data[2]={0,0};
	int reg_68_value, reg_69_value;

	if (dynamic_fod == sy->dynamic_fod)
		return;

	if (!dynamic_fod) {
		data[0] = FOD_68_VALUE;
		data[1] = FOD_69_VALUE;
	} else {
		data[0] = FOD_68_DYNAMIC_VALUE;
		data[1] = FOD_69_DYNAMIC_VALUE;
	}

	sy_wls_log(SY_LOG_ERR, "dynamic setting FOD_68_VALUE = %d FOD_69_VALUE = %d \n", data[0], data[1]);
	sy65153_i2c_bulk_write(sy, SY65153_REG_68, data, 2);
	reg_68_value = sy65153_i2c_read_byte(sy, SY65153_REG_68, 1);
	reg_69_value = sy65153_i2c_read_byte(sy, SY65153_REG_69, 1);
	if (reg_68_value != data[0] || reg_69_value != data[1]) {
		sy_wls_log(SY_LOG_ERR, "dynamic update fod again \n");
		sy65153_i2c_bulk_write(sy, SY65153_REG_68, data, 2);
	}

	sy->dynamic_fod = dynamic_fod;

	return;
}

static void sy65153_monitor_workfunc(struct work_struct *work)
{
	
	struct sy_wls_chrg_chip *sy = container_of(work, struct sy_wls_chrg_chip, monitor_work.work);

	if(gpio_get_value(sy->power_good_gpio)){
		sy65153_wl_charger_dump_register(sy);
	}
	schedule_delayed_work(&sy->monitor_work, msecs_to_jiffies(30000));
}

static void sy65153_charger_irq_workfunc(struct work_struct *work)
{
	struct sy_wls_chrg_chip *sy = container_of(work, struct sy_wls_chrg_chip, irq_work.work);

	dev_err(sy->dev, "sy65153_charger_irq_workfunc enter \n");

	if(sy65153_get_wtchg_otg_status(sy)){
		dev_err(sy->dev, "sy65153_charger_irq_workfunc otg enable and cannot enable wireless charge \n");
		return;
	}
	sy->wl_online = gpio_get_value(sy->power_good_gpio);

	if (sy->wl_online) {
		sy65153_set_charger_enable(sy, true);
		sy65153_set_rx_params(sy);
	} else {
		sy->wireless_type = SY65153_VBUS_NONE;
		sy->dynamic_fod = false;
		sy65153_set_charger_enable(sy, false);
	}

	msleep(200);

	if ((sy->wl_online != sy->pre_wl_online) && sy->wl_psy) {
		power_supply_changed(sy->wl_psy);
		if(!sy->wl_online) {
			sy65153_set_reset_pd_chip_ic(sy);
		}
	}

	sy->pre_wl_type = sy->wireless_type;
	sy->pre_wl_online = sy->wl_online;
}

static irqreturn_t sy65153_charger_interrupt(int irq, void *data)
{
	struct sy_wls_chrg_chip *sy = data;

	dev_err(sy->dev, "sy65153_charger_interrupt enter \n");
	pm_wakeup_event(sy->dev, SY65153_WAKE_UP_MS);

	schedule_delayed_work(&sy->irq_work, 0);
	return IRQ_HANDLED;
}

static int sy65153_dt_parse_vreg_info(struct device *dev,
		struct sy65153_msm_reg_data **vreg_data, const char *vreg_name)
{
	int len, ret = 0;
	const __be32 *prop;
	char prop_name[MAX_PROP_SIZE];
	struct sy65153_msm_reg_data *vreg;
	struct device_node *np = dev->of_node;

	snprintf(prop_name, MAX_PROP_SIZE, "%s-supply", vreg_name);
	if (!of_parse_phandle(np, prop_name, 0)) {
		dev_info(dev, "No vreg data found for %s\n", vreg_name);
		return ret;
	}

	vreg = devm_kzalloc(dev, sizeof(*vreg), GFP_KERNEL);
	if (!vreg) {
		ret = -ENOMEM;
		return ret;
	}

	vreg->name = vreg_name;

	snprintf(prop_name, MAX_PROP_SIZE,
			"qcom,%s-always-on", vreg_name);
	if (of_get_property(np, prop_name, NULL))
		vreg->is_always_on = true;

	snprintf(prop_name, MAX_PROP_SIZE,
			"qcom,%s-lpm-sup", vreg_name);
	if (of_get_property(np, prop_name, NULL))
		vreg->lpm_sup = true;

	snprintf(prop_name, MAX_PROP_SIZE,
			"qcom,%s-voltage-level", vreg_name);
	prop = of_get_property(np, prop_name, &len);
	if (!prop || (len != (2 * sizeof(__be32)))) {
		dev_warn(dev, "%s %s property\n",
			prop ? "invalid format" : "no", prop_name);
	} else {
		vreg->low_vol_level = be32_to_cpup(&prop[0]);
		vreg->high_vol_level = be32_to_cpup(&prop[1]);
	}

	snprintf(prop_name, MAX_PROP_SIZE,
			"qcom,%s-current-level", vreg_name);
	prop = of_get_property(np, prop_name, &len);
	if (!prop || (len != (2 * sizeof(__be32)))) {
		dev_warn(dev, "%s %s property\n",
			prop ? "invalid format" : "no", prop_name);
	} else {
		vreg->lpm_uA = be32_to_cpup(&prop[0]);
		vreg->hpm_uA = be32_to_cpup(&prop[1]);
	}

	*vreg_data = vreg;
	dev_err(dev, "%s: %s %s vol=[%d %d]uV, curr=[%d %d]uA\n",
		vreg->name, vreg->is_always_on ? "always_on," : "",
		vreg->lpm_sup ? "lpm_sup," : "", vreg->low_vol_level,
		vreg->high_vol_level, vreg->lpm_uA, vreg->hpm_uA);

	return ret;
}

/* Regulator utility functions */
static int sy65153_vreg_init_reg(struct device *dev,
					struct sy65153_msm_reg_data *vreg)
{
	int ret = 0;

	/* check if regulator is already initialized? */
	if (vreg->reg)
		goto out;

	/* Get the regulator handle */
	vreg->reg = devm_regulator_get(dev, vreg->name);
	if (IS_ERR(vreg->reg)) {
		ret = PTR_ERR(vreg->reg);
		pr_err("%s: devm_regulator_get(%s) failed. ret=%d\n",
			__func__, vreg->name, ret);
		goto out;
	}

	if (regulator_count_voltages(vreg->reg) > 0) {
		vreg->set_voltage_sup = true;
		/* sanity check */
		if (!vreg->high_vol_level || !vreg->hpm_uA) {
			pr_err("%s: %s invalid constraints specified\n",
				   __func__, vreg->name);
			ret = -EINVAL;
		}
	}
out:
	return ret;
}

static int sy65153_vreg_set_optimum_mode(struct sy65153_msm_reg_data
						  *vreg, int uA_load)
{
	int ret = 0;

	/*
	 * regulators that do not support regulator_set_voltage also
	 * do not support regulator_set_optimum_mode
	 */
	if (vreg->set_voltage_sup) {
		ret = regulator_set_load(vreg->reg, uA_load);
		if (ret < 0)
			pr_err("%s: regulator_set_load(reg=%s,uA_load=%d) failed. ret=%d\n",
				   __func__, vreg->name, uA_load, ret);
		else
			/*
			 * regulator_set_load() can return non zero
			 * value even for success case.
			 */
			ret = 0;
	}
	return ret;
}

static int sy65153_vreg_set_voltage(struct sy65153_msm_reg_data *vreg,
					int min_uV, int max_uV)
{
	int ret = 0;

	if (vreg->set_voltage_sup) {
		ret = regulator_set_voltage(vreg->reg, min_uV, max_uV);
		if (ret) {
			pr_err("%s: regulator_set_voltage(%s)failed. min_uV=%d,max_uV=%d,ret=%d\n",
				   __func__, vreg->name, min_uV, max_uV, ret);
		}
	}

	return ret;
}

static int sy65153_vreg_enable(struct sy65153_msm_reg_data *vreg)
{
	int ret = 0;

	/* Put regulator in HPM (high power mode) */
	ret = sy65153_vreg_set_optimum_mode(vreg, vreg->hpm_uA);
	if (ret < 0)
		return ret;

	if (!vreg->is_enabled) {
		/* Set voltage level */
		ret = sy65153_vreg_set_voltage(vreg, vreg->low_vol_level,
						vreg->high_vol_level);
		if (ret)
			return ret;
	}
	ret = regulator_enable(vreg->reg);
	if (ret) {
		pr_err("%s: regulator_enable(%s) failed. ret=%d\n",
				__func__, vreg->name, ret);
		return ret;
	}
	vreg->is_enabled = true;
	return ret;
}

static int sy65153_vreg_disable(struct sy65153_msm_reg_data *vreg)
{
	int ret = 0;

	/* Never disable regulator marked as always_on */
	if (vreg->is_enabled && !vreg->is_always_on) {
		ret = regulator_disable(vreg->reg);
		if (ret) {
			pr_err("%s: regulator_disable(%s) failed. ret=%d\n",
				__func__, vreg->name, ret);
			goto out;
		}
		vreg->is_enabled = false;

		ret = sy65153_vreg_set_optimum_mode(vreg, 0);
		if (ret < 0)
			goto out;

		/* Set min. voltage level to 0 */
		ret = sy65153_vreg_set_voltage(vreg, 0, vreg->high_vol_level);
		if (ret)
			goto out;
	} else if (vreg->is_enabled && vreg->is_always_on) {
		if (vreg->lpm_sup) {
			/* Put always_on regulator in LPM (low power mode) */
			ret = sy65153_vreg_set_optimum_mode(vreg,
								  vreg->lpm_uA);
			if (ret < 0)
				goto out;
		}
	}
out:
	return ret;
}

static ssize_t sy65153_show_registers(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u8 addr;
	u8 val;
	u8 tmpbuf[300];
	int len;
	int idx = 0;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "Charger 1");
	for (addr = 0x0; addr <= 0x14; addr++) {
		val = sy65153_i2c_read_byte(g_sy, addr, 1);
		len = snprintf(tmpbuf, PAGE_SIZE - idx,"Reg[0x%.2x] = 0x%.2x\n", addr, val);
		memcpy(&buf[idx], tmpbuf, len);
		idx += len;
	}
	return idx;
}

static DEVICE_ATTR(registers, S_IRUGO, sy65153_show_registers, NULL);

static struct attribute *sy65153_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group sy65153_attr_group = {
	.attrs = sy65153_attributes,
};

static struct iio_channel **get_ext_channels(struct device *dev,
		 const char *const *channel_map, int size)
{
	int i, rc = 0;
	struct iio_channel **iio_ch_ext;

	iio_ch_ext = devm_kcalloc(dev, size, sizeof(*iio_ch_ext), GFP_KERNEL);
	if (!iio_ch_ext)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < size; i++) {
		iio_ch_ext[i] = devm_iio_channel_get(dev, channel_map[i]);

		if (IS_ERR(iio_ch_ext[i])) {
			rc = PTR_ERR(iio_ch_ext[i]);
			if (rc != -EPROBE_DEFER)
				dev_err(dev, "%s channel unavailable, %d\n",
						channel_map[i], rc);
			return ERR_PTR(rc);
		}
		pr_err(" get_ext_channels and write raw chan %s \n", channel_map[i]);
	}

	return iio_ch_ext;
}

static bool sy65153_ext_iio_init(struct sy_wls_chrg_chip *chip)
{
	int rc = 0;
	struct iio_channel **iio_list;

	if (!chip->wtchg_ext_iio_chans) {
		iio_list = get_ext_channels(chip->dev,
			wtchg_iio_chan_name, ARRAY_SIZE(wtchg_iio_chan_name));
	 if (IS_ERR(iio_list)) {
		 rc = PTR_ERR(iio_list);
		 if (rc != -EPROBE_DEFER) {
			 dev_err(chip->dev, "Failed to get channels, rc=%d\n",
					 rc);
			 chip->wtchg_ext_iio_chans = NULL;
		 }
		 return false;
	 }
	 chip->wtchg_ext_iio_chans = iio_list;
	}

	return true;
}

static int sy65153_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct sy_wls_chrg_chip *sy = iio_priv(indio_dev);
	int ret = 0;
	*val1 = 0;

	switch (chan->channel) {
	case PSY_IIO_SC_VBUS_PRESENT:
		*val1 = sy65153_get_charger_present(sy);
		break;
	case PSY_IIO_WL_CHARGING_ENABLED:
		*val1 = sy65153_get_charger_enable(sy);
		break;
	case PSY_IIO_CHARGE_TYPE:
		*val1 = sy65153_get_wireless_type(sy);
		break;
	case PSY_IIO_ONLINE:
		*val1 = sy65153_get_charger_online(sy);
		break;
	default:
		dev_err(sy->dev, "Unsupported sy65153 IIO chan %d\n", chan->channel);
		ret = -EINVAL;
		break;
	}

	if (ret < 0) {
		dev_err(sy->dev, "Couldn't read IIO channel %d, rc = %d\n",
			chan->channel, ret);
		return ret;
	}

	return IIO_VAL_INT;
}

static int sy65153_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct sy_wls_chrg_chip *sy = iio_priv(indio_dev);
	int ret = 0;

	switch (chan->channel) {
	case PSY_IIO_WL_CHARGING_ENABLED:
		sy65153_set_charger_enable(sy, val1);
		break;
	case PSY_IIO_SC_BUS_VOLTAGE:
		sy65153_set_vout(sy, val1);
		break;
	case PSY_IIO_WL_CHARGING_FOD_UPDATE:
		sy65153_dynamic_set_rx_params(sy, val1);
		break;
	default:
		dev_err(sy->dev, "Unsupported sy65153 IIO chan %d\n", chan->channel);
		ret = -EINVAL;
		break;
	}

	if (ret < 0)
		dev_err(sy->dev, "Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, ret);

	return ret;
}

static int sy65153_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct sy_wls_chrg_chip *sy = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = sy->iio_chan;
	int i;

	for (i = 0; i < ARRAY_SIZE(sy65153_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;

	return -EINVAL;
}

static const struct iio_info sy65153_iio_info = {
	.read_raw	= sy65153_iio_read_raw,
	.write_raw	= sy65153_iio_write_raw,
	.of_xlate	= sy65153_iio_of_xlate,
};

static int sy65153_init_iio_psy(struct sy_wls_chrg_chip *sy)
{
	struct iio_dev *indio_dev = sy->indio_dev;
	struct iio_chan_spec *chan;
	int num_iio_channels = ARRAY_SIZE(sy65153_iio_psy_channels);
	int ret, i;

	sy->iio_chan = devm_kcalloc(sy->dev, num_iio_channels,
				sizeof(*sy->iio_chan), GFP_KERNEL);
	if (!sy->iio_chan)
		return -ENOMEM;

	sy->int_iio_chans = devm_kcalloc(sy->dev,
				num_iio_channels,
				sizeof(*sy->int_iio_chans),
				GFP_KERNEL);
	if (!sy->int_iio_chans)
		return -ENOMEM;

	indio_dev->info = &sy65153_iio_info;
	indio_dev->dev.parent = sy->dev;
	indio_dev->dev.of_node = sy->dev->of_node;
	indio_dev->name = "sy65153_wl_chg";
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = sy->iio_chan;
	indio_dev->num_channels = num_iio_channels;

	for (i = 0; i < num_iio_channels; i++) {
		sy->int_iio_chans[i].indio_dev = indio_dev;
		chan = &sy->iio_chan[i];
		sy->int_iio_chans[i].channel = chan;
		chan->address = i;
		chan->channel = sy65153_iio_psy_channels[i].channel_num;
		chan->type = sy65153_iio_psy_channels[i].type;
		chan->datasheet_name =
			sy65153_iio_psy_channels[i].datasheet_name;
		chan->extend_name =
			sy65153_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate =
			sy65153_iio_psy_channels[i].info_mask;
	}

	ret = devm_iio_device_register(sy->dev, indio_dev);
	if (ret)
		dev_err(sy->dev, "Failed to register QG IIO device, rc=%d\n", ret);

	dev_err(sy->dev, "SY65153 IIO device, rc=%d\n", ret);
	return ret;
}

static int sy65153_parse_dt(struct device *dev, struct sy_wls_chrg_chip *sy)
{
	struct device_node *np = dev->of_node;

	sy->sleep_mode_gpio = of_get_named_gpio(np, "sy65153,sleep-gpio", 0);
    if (sy->sleep_mode_gpio < 0) {
		pr_err("%s get sleep_mode_gpio false\n", __func__);			
	} else {
		pr_err("%s sleep_mode_gpio info %d\n", __func__, sy->sleep_mode_gpio);
	}

	sy->irq_gpio = of_get_named_gpio(np, "sy65153,irq-gpio", 0);
	if (sy->irq_gpio < 0) {
		pr_err("%s get irq_gpio fail !\n", __func__);
	} else {
		pr_err("%s irq_gpio info %d\n", __func__, sy->irq_gpio);
	}

	sy->power_good_gpio = of_get_named_gpio(np, "sy65153,pg-gpio", 0);
	if (sy->power_good_gpio < 0) {
		pr_err("%s get power_good_gpio fail !\n", __func__);
	} else {
		pr_err("%s power_good_gpio info %d\n", __func__, sy->power_good_gpio);
	}

	return 0;
}

static void sy65153_charger_init_parameter(struct sy_wls_chrg_chip *sy)
{
	sy->wl_online = 0;
	sy->pre_wl_online=0;
	sy->set_vchar_state=0;

	sy->pre_wl_type = SY65153_VBUS_NONE;

	sy->resume_completed = true;

	sy->dynamic_fod = false;
}

static int sy65153_charger_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	int ret;
	int irqn;
	struct iio_dev *indio_dev;
	struct sy_wls_chrg_chip *sy;

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*sy));
	sy = iio_priv(indio_dev);
	if (!sy) {
		dev_err(&client->dev, "%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	sy->indio_dev = indio_dev;
	sy->dev = &client->dev;
	sy->client = client;
	i2c_set_clientdata(client, sy);

	pr_err("sy65153 driver probe start \n");
	// regulator
	sy->vreg_data = devm_kzalloc(&client->dev, sizeof(struct sy65153_msm_vreg_data), GFP_KERNEL);
	if (!sy->vreg_data) {
		dev_err(&client->dev, "failed to allocate memory for vreg data\n");
		return -ENOMEM;
	}

	if (sy65153_dt_parse_vreg_info(&client->dev, &sy->vreg_data->vdd_data, "vdd")) {
		dev_err(&client->dev, "failed parsing vdd data\n");
		return -ENOMEM;
	}

	sy65153_charger_init_parameter(sy);

	mutex_init(&sy->resume_complete_lock);

	sy->sy65153_wake_source.source = wakeup_source_register(NULL,
							"sy65153_wake");
	sy->sy65153_wake_source.disabled = 1;

	g_sy = sy;
	ret = sy65153_init_iio_psy(sy);
	if (ret < 0) {
		dev_err(sy->dev, "Failed to initialize sy65153 iio psy: %d\n", ret);
		goto err_1;
	}

	sy65153_ext_iio_init(sy);

    if (!sy->wl_psy) {
        sy->wl_psy = power_supply_get_by_name("wireless_chg");
        if (!sy->wl_psy) {
			dev_err(sy->dev, "Failed to initialize wireless psy \n");
        }
    }

	if (client->dev.of_node)
		sy65153_parse_dt(&client->dev, sy);

	ret = gpio_request(sy->sleep_mode_gpio, "sy65153 sleep pin");
	if (ret) {
		dev_err(sy->dev, "%s: %d sleep_mode_gpio request failed\n",
					__func__, sy->sleep_mode_gpio);
		goto err_0;
	}
	gpio_direction_output(sy->sleep_mode_gpio, 0);

	ret = gpio_request(sy->irq_gpio, "sy65153 irq pin");
	if (ret) {
		dev_err(sy->dev, "%s: %d gpio request failed\n", __func__, sy->irq_gpio);
		goto err_0;
	}
	gpio_direction_input(sy->irq_gpio);

	irqn = gpio_to_irq(sy->irq_gpio);
	if (irqn < 0) {
		dev_err(sy->dev, "%s:%d gpio_to_irq failed\n", __func__, irqn);
		ret = irqn;
		goto err_0;
	}
	client->irq = irqn;

	ret = gpio_request(sy->power_good_gpio, "sy65153 power good pin");
	if (ret) {
		dev_err(sy->dev, "%s: %d gpio request failed\n", __func__, sy->power_good_gpio);
		goto err_0;
	}
	gpio_direction_input(sy->power_good_gpio);

	irqn = gpio_to_irq(sy->power_good_gpio);
	if (irqn < 0) {
		dev_err(sy->dev, "%s:%d gpio_to_irq failed\n", __func__, irqn);
		ret = irqn;
		goto err_0;
	}
	client->irq = irqn;

	INIT_DELAYED_WORK(&sy->irq_work, sy65153_charger_irq_workfunc);
	INIT_DELAYED_WORK(&sy->monitor_work, sy65153_monitor_workfunc);

	ret = request_irq(client->irq, sy65153_charger_interrupt,
						IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
						"sy65153_charger1_irq", sy);
	if (ret) {
		dev_err(sy->dev, "%s:Request IRQ %d failed: %d\n", __func__, client->irq, ret);
		//goto err_irq;
	} else {
		dev_err(sy->dev, "%s:irq = %d\n", __func__, client->irq);
	}

	ret = sysfs_create_group(&sy->dev->kobj, &sy65153_attr_group);
	if (ret) {
		dev_err(sy->dev, "failed to register sysfs. err: %d\n", ret);
		goto err_irq;
	}

	// init regulator
	ret = sy65153_vreg_init_reg(&client->dev, sy->vreg_data->vdd_data);
	if(ret>=0){
		sy65153_vreg_enable(sy->vreg_data->vdd_data);
	}else{
		sy65153_vreg_disable(sy->vreg_data->vdd_data);
	}

	enable_irq_wake(irqn);

#ifdef CONFIG_WT_QGKI
	// SET hardware info
	hardwareinfo_set_prop(HARDWARE_WIRELESS_CHARGER_IC, CHARGER_IC_NAME);
#endif /* CONFIG_WT_QGKI */

	pr_err("sy65153 driver probe succeed !!! \n");

	schedule_delayed_work(&sy->irq_work, msecs_to_jiffies(1500));
	schedule_delayed_work(&sy->monitor_work, msecs_to_jiffies(10000));

	return 0;

err_irq:
	cancel_delayed_work_sync(&sy->irq_work);
	cancel_delayed_work_sync(&sy->monitor_work);
err_1:
err_0:
	mutex_destroy(&sy->resume_complete_lock);

	devm_kfree(&client->dev, sy);
	sy = NULL;
	g_sy = NULL;
	return ret;
}

static void sy65153_charger_shutdown(struct i2c_client *client)
{
	struct sy_wls_chrg_chip *sy = i2c_get_clientdata(client);

	if (sy->wl_online)
		sy65153_set_vout(sy, SY65153_VOUT_5V);

	mutex_destroy(&sy->resume_complete_lock);

	sysfs_remove_group(&sy->dev->kobj, &sy65153_attr_group);

	cancel_delayed_work_sync(&sy->irq_work);
	cancel_delayed_work_sync(&sy->monitor_work);

	free_irq(sy->client->irq, NULL);
	g_sy = NULL;
}

static int sy65153_charger_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sy_wls_chrg_chip *sy = i2c_get_clientdata(client);

	mutex_lock(&sy->resume_complete_lock);
	sy->resume_completed = true;
	mutex_unlock(&sy->resume_complete_lock);

	return 0;
}

static int sy65153_charger_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sy_wls_chrg_chip *sy = i2c_get_clientdata(client);

	mutex_lock(&sy->resume_complete_lock);
	sy->resume_completed = false;
	mutex_unlock(&sy->resume_complete_lock);

	return 0;
}

static struct of_device_id sy65153_charger_match_table[] = {
	{.compatible = "silergy,sy65153",},
	{},
};

static const struct i2c_device_id sy65153_charger_id[] = {
	{ "sy65153", 0 },
	{},
};

static const struct dev_pm_ops sy65153_charger_pm_ops = {
	.resume		= sy65153_charger_resume,
	.suspend	= sy65153_charger_suspend,
};

MODULE_DEVICE_TABLE(i2c, sy65153_charger_id);

static struct i2c_driver sy65153_charger_driver = {
	.driver		= {
		.name	= "sy65153",
		.of_match_table = sy65153_charger_match_table,
		.pm		= &sy65153_charger_pm_ops,
	},
	.id_table	= sy65153_charger_id,

	.probe		= sy65153_charger_probe,
	.shutdown   = sy65153_charger_shutdown,
};

module_i2c_driver(sy65153_charger_driver);

MODULE_DESCRIPTION("SY65153 Wireless Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Wingtech");
MODULE_ALIAS("i2c:sy_wls_charger");

