/*
 * UPM6918D battery charging driver
 *
 * Copyright (C) 2023 Unisemipower
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define pr_fmt(fmt) "<upm6918-05172200> [%s,%d] " fmt, __func__, __LINE__

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/extcon-provider.h>

#include "upm6918_reg.h"
#include "upm6918.h"
#include "../../../usb/typec/tcpc/inc/tcpm.h"
#include "../../../usb/typec/tcpc/inc/tcpci_core.h"
#include <dt-bindings/iio/qti_power_supply_iio.h>

// longcheer nielianjie10 2023.4.26 add iio
#include "upm6918_iio.h"
#include <linux/iio/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/ipc_logging.h>
#include <linux/printk.h>

#if 0  /*only for debug*/
#undef pr_debug
#define pr_debug pr_err
#undef pr_info
#define pr_info pr_err
#undef dev_dbg
#define dev_dbg dev_err
#endif

#define ATTACHEDSRC   7
#define FLOAT_VOLTAGE_DEFAULT_UPM6918 4350
#define FLOAT_VOLTAGE_STEP1_UPM6918   4330
#define FLOAT_VOLTAGE_STEP2_UPM6918   4310
#define FLOAT_VOLTAGE_STEP3_UPM6918   4290
#define FLOAT_VOLTAGE_STEP4_UPM6918   4240

#define FLOAT_VOLTAGE_DEFAULT_UPM6918_PD_AFC_VAL 0x11
#define FLOAT_VOLTAGE_DEFAULT_UPM6918_VAL 0x10

#define FLOAT_VOLTAGE_STEP1_UPM6918_VAL 0xF
#define FLOAT_VOLTAGE_STEP2_UPM6918_VAL 0xE
#define FLOAT_VOLTAGE_STEP3_UPM6918_VAL 0xD
#define FLOAT_VOLTAGE_STEP4_UPM6918_VAL 0xC


extern int sp2130_set_adc(bool val);
extern int sm5602_set_shutdown(int val);
extern int sm5602_CV;
enum {
	PN_UPM6918D,
};

enum upm6918_part_no {
	UPM6918D = 0x02,
};

static int pn_data[] = {
	[PN_UPM6918D] = 0x02,
};

static char *pn_str[] = {
	[PN_UPM6918D] = "upm6918d",
};

struct upm6918 {
	struct device *dev;
	struct i2c_client *client;

	enum upm6918_part_no part_no;
	int revision;
	bool pr_flag;

	const char *chg_dev_name;
	const char *eint_name;

	bool chg_det_enable;
	int charge_afc;
	bool is_disble_afc;
	bool is_disble_afc_backup;
	int status;
	int irq;
	u32 intr_gpio;
	int pd_active;
	enum power_supply_type chg_type;

	struct mutex i2c_rw_lock;

	bool charge_enabled;	/* Register bit status */
	bool power_good;
	bool vbus_gd;

	struct upm6918_platform_data *platform_data;
	struct charger_device *chg_dev;

	struct power_supply *psy;
	struct power_supply *bat_psy;

	struct delayed_work prob_dwork;
	struct delayed_work hvdcp_dwork;
	struct delayed_work hvdcp_qc20_dwork;
	struct delayed_work disable_afc_dwork;
	struct delayed_work disable_pdo_dwork;
	int psy_usb_type;
	bool entry_shipmode;

	struct wakeup_source *hvdcp_qc20_ws;

	struct	mutex regulator_lock;
	struct	regulator *dpdm_reg;
	bool	dpdm_enabled;

	struct power_supply_desc chg_psy_desc;
	struct power_supply_config chg_psy_cfg;
	struct power_supply *charger_psy;

	struct power_supply	*usb_psy;
	struct power_supply	*ac_psy;
	struct power_supply *cp_psy;
	struct extcon_dev	*extcon;

	struct delayed_work tcpc_dwork;
	struct tcpc_device *tcpc;
	struct notifier_block tcp_nb;

	/* longcheer nielianjie10 2023.4.26 add iio start */
	struct iio_dev  		*indio_dev;
	struct iio_chan_spec	*iio_chan;
	struct iio_channel		*int_iio_chans;
	struct iio_channel		**ds_ext_iio_chans;
	struct iio_channel		**fg_ext_iio_chans;
	struct iio_channel		**nopmi_chg_ext_iio_chans;

	struct votable			*fcc_votable;
	struct votable			*fv_votable;
	struct votable			*usb_icl_votable;
	struct votable			*chg_dis_votable;
	/* longcheer nielianjie10 2023.4.26 add iio end */

	struct notifier_block pd_nb;
    	bool unknown_type_recheck;
    	int unknown_type_recheck_cnt;
};

#ifndef MTK_CHARGER
enum {
	CHARGER_UNKNOWN,
	STANDARD_HOST,
	CHARGING_HOST,
	NONSTANDARD_CHARGER,
	STANDARD_CHARGER,
};
#endif

static const unsigned int upm6918_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

static bool vbus_on = false;

int upm6918_set_iio_channel(struct upm6918 *upm, enum upm6918_iio_type type, int channel, int val);

static int __upm6918_read_reg(struct upm6918 *upm, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(upm->client, reg);
	if (ret < 0) {
		pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int __upm6918_write_reg(struct upm6918 *upm, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(upm->client, reg, val);
	if (ret < 0) {
		pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
		       val, reg, ret);
		return ret;
	}
	return 0;
}

static int upm6918_read_byte(struct upm6918 *upm, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&upm->i2c_rw_lock);
	ret = __upm6918_read_reg(upm, reg, data);
	mutex_unlock(&upm->i2c_rw_lock);

	return ret;
}

static int upm6918_write_byte(struct upm6918 *upm, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&upm->i2c_rw_lock);
	ret = __upm6918_write_reg(upm, reg, data);
	mutex_unlock(&upm->i2c_rw_lock);

	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

	return ret;
}

static int upm6918_update_bits(struct upm6918 *upm, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	mutex_lock(&upm->i2c_rw_lock);
	ret = __upm6918_read_reg(upm, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __upm6918_write_reg(upm, reg, tmp);
	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&upm->i2c_rw_lock);
	return ret;
}

enum charg_stat upm6918_get_charging_status(struct upm6918 *upm)
{
        enum charg_stat status = CHRG_STAT_NOT_CHARGING;
        int ret = 0;
        u8 reg_val = 0;

        ret = upm6918_read_byte(upm, UPM6918_REG_08, &reg_val);
        if (ret) {
                pr_err("read UPM6918_REG_08 failed, ret:%d\n", ret);
                return ret;
        }

        status = (reg_val & REG08_CHRG_STAT_MASK) >> REG08_CHRG_STAT_SHIFT;

        return status;
}
EXPORT_SYMBOL_GPL(upm6918_get_charging_status);

int upm6918_set_boost_voltage(struct upm6918 *upm, int volt)
{
	u8 val;

	if (volt <= BOOSTV_4850)
		val = REG06_BOOSTV_4P85V;
	else if (volt <= BOOSTV_5150)
		val = REG06_BOOSTV_5P15V;
	else if (volt <= REG06_BOOSTV_5V)
		val = REG06_BOOSTV_5V;
	else
		val = REG06_BOOSTV_5P3V;

	return upm6918_update_bits(upm, UPM6918_REG_06, REG06_BOOSTV_MASK,
				   val << REG06_BOOSTV_SHIFT);
}
EXPORT_SYMBOL_GPL(upm6918_set_boost_voltage);

int upm6918_enable_otg(struct upm6918 *upm)
{
	u8 val = REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT;

	return upm6918_update_bits(upm, UPM6918_REG_01, REG01_OTG_CONFIG_MASK,
				   val);

}
EXPORT_SYMBOL_GPL(upm6918_enable_otg);

int upm6918_disable_otg(struct upm6918 *upm)
{
	u8 val = REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT;

	return upm6918_update_bits(upm, UPM6918_REG_01, REG01_OTG_CONFIG_MASK,
				   val);

}
EXPORT_SYMBOL_GPL(upm6918_disable_otg);

int upm6918_enable_charger(struct upm6918 *upm)
{
	int ret;
	u8 val = REG01_CHG_ENABLE << REG01_CHG_CONFIG_SHIFT;

	ret =
	    upm6918_update_bits(upm, UPM6918_REG_01, REG01_CHG_CONFIG_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(upm6918_enable_charger);

int upm6918_disable_charger(struct upm6918 *upm)
{
	int ret;
	u8 val = REG01_CHG_DISABLE << REG01_CHG_CONFIG_SHIFT;

	ret =
	    upm6918_update_bits(upm, UPM6918_REG_01, REG01_CHG_CONFIG_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(upm6918_disable_charger);

int upm6918_set_chargecurrent(struct upm6918 *upm, int curr)
{
	u8 ichg;

	if (curr > REG02_ICHG_MAX)
		curr = REG02_ICHG_MAX;

	if (curr < REG02_ICHG_MIN)
		curr = REG02_ICHG_MIN;
	pr_err("%s:curr=%d\n",__func__,curr);
	ichg = (curr - REG02_ICHG_BASE) / REG02_ICHG_LSB;
	return upm6918_update_bits(upm, UPM6918_REG_02, REG02_ICHG_MASK,
				   ichg << REG02_ICHG_SHIFT);

}

int upm6918_set_term_current(struct upm6918 *upm, int curr)
{
	u8 iterm;

	if (curr < REG03_ITERM_BASE)
		curr = REG03_ITERM_BASE;
	pr_err("%s:curr=%d\n",__func__,curr);
	iterm = (curr - REG03_ITERM_BASE) / REG03_ITERM_LSB;

	return upm6918_update_bits(upm, UPM6918_REG_03, REG03_ITERM_MASK,
				   iterm << REG03_ITERM_SHIFT);
}
EXPORT_SYMBOL_GPL(upm6918_set_term_current);

int upm6918_set_prechg_current(struct upm6918 *upm, int curr)
{
	u8 iprechg;

	if (curr < REG03_IPRECHG_BASE)
		curr = REG03_IPRECHG_BASE;

	iprechg = (curr - REG03_IPRECHG_BASE) / REG03_IPRECHG_LSB;

	return upm6918_update_bits(upm, UPM6918_REG_03, REG03_IPRECHG_MASK,
				   iprechg << REG03_IPRECHG_SHIFT);
}
EXPORT_SYMBOL_GPL(upm6918_set_prechg_current);

int upm6918_set_chargevolt(struct upm6918 *upm, int volt)
{
	u8 val;

	if (volt < REG04_VREG_BASE)
		volt = REG04_VREG_BASE;

	if (volt < REG04_VREG_BASE)
		volt = REG04_VREG_BASE;

    if (volt == 4400) {
		val = 17;
	} else if (volt == 4350) {
		val = 16;
	} else if (volt == 4330) {
		val = 15;
	} else if (volt == 4310) {
		val = 14;
	} else if (volt == 4280) {
		val = 13;
	} else if (volt == 4240) {
		val = 12;
	} else{
		val = (volt - REG04_VREG_BASE) / REG04_VREG_LSB;
	}
	

	pr_err("set_chargevolt = %d\n", volt);

	return upm6918_update_bits(upm, UPM6918_REG_04, REG04_VREG_MASK,
				   val << REG04_VREG_SHIFT);
}

int upm6918_set_input_volt_limit(struct upm6918 *upm, int volt)
{
	u8 val;

	if (volt < REG06_VINDPM_BASE)
		volt = REG06_VINDPM_BASE;

	val = (volt - REG06_VINDPM_BASE) / REG06_VINDPM_LSB;
	return upm6918_update_bits(upm, UPM6918_REG_06, REG06_VINDPM_MASK,
				   val << REG06_VINDPM_SHIFT);
}

int upm6918_get_input_volt_limit(struct upm6918 *upm, int *volt)
{
	u8 val;
	int ret;
	int vindpm;

	ret = upm6918_read_byte(upm, UPM6918_REG_06, &val);
	if (ret == 0){
		pr_err("Reg[%.2x] = 0x%.2x\n", UPM6918_REG_06, val);
	}

	pr_err("vindpm_reg_val:0x%X\n", val);
	vindpm = (val & REG06_VINDPM_MASK) >> REG06_VINDPM_SHIFT;
	vindpm = vindpm * REG06_VINDPM_LSB + REG06_VINDPM_BASE;
	*volt = vindpm;

	return ret;
}
EXPORT_SYMBOL_GPL(upm6918_get_input_volt_limit);

int upm6918_set_input_current_limit(struct upm6918 *upm, int curr)
{
	u8 val;

	if (curr < REG00_IINLIM_BASE)
		curr = REG00_IINLIM_BASE;
	pr_err("%s:curr=%d\n",__func__,curr);
	val = (curr - REG00_IINLIM_BASE) / REG00_IINLIM_LSB;
	return upm6918_update_bits(upm, UPM6918_REG_00, REG00_IINLIM_MASK,
				   val << REG00_IINLIM_SHIFT);
}

int upm6918_get_input_current_limit(struct upm6918 *upm)
{
        u8 reg_val;

	upm6918_read_byte(upm, UPM6918_REG_00, &reg_val);

	reg_val &= REG00_IINLIM_MASK;

        if (reg_val)
		return 0;
	else
		return 1;
}

int upm6918_set_watchdog_timer(struct upm6918 *upm, u8 timeout)
{
	u8 temp;

	temp = (u8) (((timeout -
		       REG05_WDT_BASE) / REG05_WDT_LSB) << REG05_WDT_SHIFT);

	return upm6918_update_bits(upm, UPM6918_REG_05, REG05_WDT_MASK, temp);
}
EXPORT_SYMBOL_GPL(upm6918_set_watchdog_timer);

int upm6918_disable_watchdog_timer(struct upm6918 *upm)
{
	u8 val = REG05_WDT_DISABLE << REG05_WDT_SHIFT;

	return upm6918_update_bits(upm, UPM6918_REG_05, REG05_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(upm6918_disable_watchdog_timer);

int upm6918_reset_watchdog_timer(struct upm6918 *upm)
{
	u8 val = REG01_WDT_RESET << REG01_WDT_RESET_SHIFT;

	return upm6918_update_bits(upm, UPM6918_REG_01, REG01_WDT_RESET_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(upm6918_reset_watchdog_timer);

int upm6918_reset_chip(struct upm6918 *upm)
{
	int ret;
	u8 val = REG0B_REG_RESET << REG0B_REG_RESET_SHIFT;

	ret =
	    upm6918_update_bits(upm, UPM6918_REG_0B, REG0B_REG_RESET_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(upm6918_reset_chip);

int upm6918_enter_hiz_mode(struct upm6918 *upm)
{
	u8 val = REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT;

	return upm6918_update_bits(upm, UPM6918_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(upm6918_enter_hiz_mode);

int upm6918_exit_hiz_mode(struct upm6918 *upm)
{

	u8 val = REG00_HIZ_DISABLE << REG00_ENHIZ_SHIFT;

	return upm6918_update_bits(upm, UPM6918_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(upm6918_exit_hiz_mode);

int upm6918_enable_term(struct upm6918 *upm, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = REG05_TERM_ENABLE << REG05_EN_TERM_SHIFT;
	else
		val = REG05_TERM_DISABLE << REG05_EN_TERM_SHIFT;

	ret = upm6918_update_bits(upm, UPM6918_REG_05, REG05_EN_TERM_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(upm6918_enable_term);

int upm6918_set_boost_current(struct upm6918 *upm, int curr)
{
	u8 val;

	val = REG02_BOOST_LIM_0P5A;
	if (curr == BOOSTI_1200)
		val = REG02_BOOST_LIM_1P2A;

	return upm6918_update_bits(upm, UPM6918_REG_02, REG02_BOOST_LIM_MASK,
				   val << REG02_BOOST_LIM_SHIFT);
}

int upm6918_set_acovp_threshold(struct upm6918 *upm, int volt)
{
	u8 val;

	if (volt == VAC_OVP_14000)
		val = REG06_OVP_14P0V;
	else if (volt == VAC_OVP_10500)
		val = REG06_OVP_10P5V;
	else if (volt == VAC_OVP_6500)
		val = REG06_OVP_6P5V;
	else
		val = REG06_OVP_5P5V;

	return upm6918_update_bits(upm, UPM6918_REG_06, REG06_OVP_MASK,
				   val << REG06_OVP_SHIFT);
}
EXPORT_SYMBOL_GPL(upm6918_set_acovp_threshold);

static int upm6918_set_stat_ctrl(struct upm6918 *upm, int ctrl)
{
	u8 val;

	val = ctrl;

	return upm6918_update_bits(upm, UPM6918_REG_00, REG00_STAT_CTRL_MASK,
				   val << REG00_STAT_CTRL_SHIFT);
}

static int upm6918_set_int_mask(struct upm6918 *upm, int mask)
{
	u8 val;

	val = mask;

	return upm6918_update_bits(upm, UPM6918_REG_0A, REG0A_INT_MASK_MASK,
				   val << REG0A_INT_MASK_SHIFT);
}

int upm6918_enable_batfet(struct upm6918 *upm)
{
	const u8 val = REG07_BATFET_ON << REG07_BATFET_DIS_SHIFT;

	return upm6918_update_bits(upm, UPM6918_REG_07, REG07_BATFET_DIS_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(upm6918_enable_batfet);

int upm6918_disable_batfet(struct upm6918 *upm)
{
	const u8 val = REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT;

	return upm6918_update_bits(upm, UPM6918_REG_07, REG07_BATFET_DIS_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(upm6918_disable_batfet);

int upm6918_batfet_rst_en(struct upm6918 *upm, bool en)
{
	u8 val = 0;

	if (en) {
		val = REG07_BATFET_RST_ENABLE << REG07_BATFET_RST_EN_SHIFT;
	} else {
		val = REG07_BATFET_RST_DISABLE << REG07_BATFET_RST_EN_SHIFT;
	}

	return upm6918_update_bits(upm, UPM6918_REG_07, REG07_BATFET_RST_EN_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(upm6918_batfet_rst_en);

int upm6918_set_batfet_delay(struct upm6918 *upm, uint8_t delay)
{
	u8 val;

	if (delay == 0)
		val = REG07_BATFET_DLY_0S;
	else
		val = REG07_BATFET_DLY_10S;

	val <<= REG07_BATFET_DLY_SHIFT;

	return upm6918_update_bits(upm, UPM6918_REG_07, REG07_BATFET_DLY_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(upm6918_set_batfet_delay);

int upm6918_enable_safety_timer(struct upm6918 *upm)
{
	const u8 val = REG05_CHG_TIMER_ENABLE << REG05_EN_TIMER_SHIFT;

	return upm6918_update_bits(upm, UPM6918_REG_05, REG05_EN_TIMER_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(upm6918_enable_safety_timer);

int upm6918_disable_safety_timer(struct upm6918 *upm)
{
	const u8 val = REG05_CHG_TIMER_DISABLE << REG05_EN_TIMER_SHIFT;

	return upm6918_update_bits(upm, UPM6918_REG_05, REG05_EN_TIMER_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(upm6918_disable_safety_timer);

int upm6918_enable_hvdcp(struct upm6918 *upm, bool en)
{
	const u8 val = en << REG0C_EN_HVDCP_SHIFT;

	return upm6918_update_bits(upm, UPM6918_REG_0C, REG0C_EN_HVDCP_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(upm6918_enable_hvdcp);

int upm6918_set_dp(struct upm6918 *upm, int dp_stat)
{
	const u8 val = dp_stat << REG0C_DP_MUX_SHIFT;

	return upm6918_update_bits(upm, UPM6918_REG_0C, REG0C_DP_MUX_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(upm6918_set_dp);

int upm6918_set_dm(struct upm6918 *upm, int dm_stat)
{
	const u8 val = dm_stat << REG0C_DM_MUX_SHIFT;

	return upm6918_update_bits(upm, UPM6918_REG_0C, REG0C_DM_MUX_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(upm6918_set_dm);

static int upm6918_force_dpdm(struct upm6918 *upm)
{
	int ret = 0;
	const u8 val = REG07_FORCE_DPDM << REG07_FORCE_DPDM_SHIFT;

	ret = upm6918_update_bits(upm, UPM6918_REG_07,
			REG07_FORCE_DPDM_MASK, val);

	return ret;
}

static int upm6918_request_dpdm(struct upm6918 *upm, bool enable)
{
	int ret = 0;

	mutex_lock(&upm->regulator_lock);
	/* fetch the DPDM regulator */
	if (!upm->dpdm_reg && of_get_property(upm->dev->of_node, "dpdm-supply", NULL)) {
		upm->dpdm_reg = devm_regulator_get(upm->dev, "dpdm");
		if (IS_ERR(upm->dpdm_reg)) {
			ret = PTR_ERR(upm->dpdm_reg);
			pr_err("Couldn't get dpdm regulator ret=%d\n", ret);
			upm->dpdm_reg = NULL;
			mutex_unlock(&upm->regulator_lock);
			return ret;
		}
	}

	if (enable) {
		if (upm->dpdm_reg && !upm->dpdm_enabled) {
		pr_err("enabling DPDM regulator\n");
		ret = regulator_enable(upm->dpdm_reg);
		if (ret < 0)
			pr_err("Couldn't enable dpdm regulator ret=%d\n", ret);
		else
			upm->dpdm_enabled = true;
		}
	} else {
		if (upm->dpdm_reg && upm->dpdm_enabled) {
			pr_err("disabling DPDM regulator\n");
			ret = regulator_disable(upm->dpdm_reg);
			if (ret < 0)
				pr_err("Couldn't disable dpdm regulator ret=%d\n", ret);
			else
				upm->dpdm_enabled = false;
		}
	}
	mutex_unlock(&upm->regulator_lock);

	return ret;
}

static struct upm6918_platform_data *upm6918_parse_dt(struct device_node *np,
						      struct upm6918 *upm)
{
	int ret;
	struct upm6918_platform_data *pdata;

	pdata = devm_kzalloc(&upm->client->dev, sizeof(struct upm6918_platform_data),
			     GFP_KERNEL);
	if (!pdata)
		return NULL;

	if (of_property_read_string(np, "charger_name", &upm->chg_dev_name) < 0) {
		upm->chg_dev_name = "primary_chg";
		pr_err("no charger name\n");
	}

	if (of_property_read_string(np, "eint_name", &upm->eint_name) < 0) {
		upm->eint_name = "chr_stat";
		pr_err("no eint name\n");
	}

	ret = of_get_named_gpio(np, "upm,intr_gpio", 0);
	if (ret < 0) {
		pr_err("%s no upm,intr_gpio(%d)\n",
				      __func__, ret);
	} else
		upm->intr_gpio = ret;

	pr_err("%s intr_gpio = %u\n",
			    __func__, upm->intr_gpio);

	upm->chg_det_enable =
	    of_property_read_bool(np, "upm,upm6918,charge-detect-enable");

	ret = of_property_read_u32(np, "upm,upm6918,usb-vlim", &pdata->usb.vlim);
	if (ret) {
		pdata->usb.vlim = 4600;
		pr_err("Failed to read node of upm,upm6918,usb-vlim\n");
	}

	ret = of_property_read_u32(np, "upm,upm6918,usb-ilim", &pdata->usb.ilim);
	if (ret) {
		pdata->usb.ilim = 2000;
		pr_err("Failed to read node of upm,upm6918,usb-ilim\n");
	}

	ret = of_property_read_u32(np, "upm,upm6918,usb-vreg", &pdata->usb.vreg);
	if (ret) {
		pdata->usb.vreg = 4423;
		pr_err("Failed to read node of upm,upm6918,usb-vreg\n");
	}

	ret = of_property_read_u32(np, "upm,upm6918,usb-ichg", &pdata->usb.ichg);
	if (ret) {
		pdata->usb.ichg = 3000;
		pr_err("Failed to read node of upm,upm6918,usb-ichg\n");
	}

	ret = of_property_read_u32(np, "upm,upm6918,stat-pin-ctrl",
				   &pdata->statctrl);
	if (ret) {
		pdata->statctrl = 0;
		pr_err("Failed to read node of upm,upm6918,stat-pin-ctrl\n");
	}

	ret = of_property_read_u32(np, "upm,upm6918,precharge-current",
				   &pdata->iprechg);
	if (ret) {
		pdata->iprechg = 180;
		pr_err("Failed to read node of upm,upm6918,precharge-current\n");
	}

	ret = of_property_read_u32(np, "upm,upm6918,termination-current",
				   &pdata->switch_iterm);
	if (ret) {
		pdata->switch_iterm = 240;
		pr_err
		    ("Failed to read node of upm,upm6918,termination-current\n");
	}
	pdata->iterm = pdata->switch_iterm;

	ret =
	    of_property_read_u32(np, "upm,upm6918,boost-voltage",
				 &pdata->boostv);
	if (ret) {
		pdata->boostv = 5000;
		pr_err("Failed to read node of upm,upm6918,boost-voltage\n");
	}

	ret =
	    of_property_read_u32(np, "upm,upm6918,boost-current",
				 &pdata->boosti);
	if (ret) {
		pdata->boosti = 1200;
		pr_err("Failed to read node of upm,upm6918,boost-current\n");
	}

	ret = of_property_read_u32(np, "upm,upm6918,vac-ovp-threshold",
				   &pdata->vac_ovp);
	if (ret) {
		pdata->vac_ovp = VAC_OVP_14000;
		pr_err("Failed to read node of upm,upm6918,vac-ovp-threshold\n");
	}

	return pdata;
}

static void upm6918_power_changed(struct upm6918 *upm)
{
	if (upm->bat_psy == NULL)
		upm->bat_psy = power_supply_get_by_name("battery");
	if (upm->bat_psy)
		power_supply_changed(upm->bat_psy);
}

static void upm6918_notify_usb_host(struct upm6918 *upm, bool enable);
static void upm6918_notify_device_mode(struct upm6918 *upm, bool enable);


static int upm6918_get_charger_type(struct upm6918 *upm, int *type)
{
	int ret;
	u8 reg_val = 0;
	int vbus_stat = 0;
	int chg_type = CHARGER_UNKNOWN;
	int upm6918_chg_current = CHG_SDP_CURR_MAX;

	ret = upm6918_read_byte(upm, UPM6918_REG_08, &reg_val);
	if (ret)
		return ret;

	vbus_stat = (reg_val & REG08_VBUS_STAT_MASK);
	vbus_stat >>= REG08_VBUS_STAT_SHIFT;

	pr_err("reg08: 0x%02x\n", reg_val);

	if(vbus_stat == REG08_VBUS_TYPE_SDP && upm->unknown_type_recheck_cnt < 2) {
		if(!upm->pr_flag)
			upm6918_request_dpdm(upm, true);
		else
			goto charger_type;
		msleep(5);
		pr_err("xlog\n");
		upm6918_force_dpdm(upm);
		msleep(5);
        	upm->unknown_type_recheck = true;
        	upm->unknown_type_recheck_cnt++;
        	return ret;
	}

charger_type:
	switch (vbus_stat) {
	case REG08_VBUS_TYPE_NONE:
		chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		break;
	case REG08_VBUS_TYPE_SDP:
		chg_type = POWER_SUPPLY_USB_TYPE_SDP;
		break;
	case REG08_VBUS_TYPE_CDP:
		chg_type = POWER_SUPPLY_USB_TYPE_CDP;
		break;
	case REG08_VBUS_TYPE_DCP:
		chg_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	case REG08_VBUS_TYPE_UNKNOWN:
		chg_type = POWER_SUPPLY_USB_TYPE_FLOAT;
		break;
	case REG08_VBUS_TYPE_NON_STD:
		chg_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	case REG08_VBUS_TYPE_OTG:
		chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		break;
	default:
		chg_type = POWER_SUPPLY_USB_TYPE_FLOAT;
		break;
	}

	if (chg_type == POWER_SUPPLY_USB_TYPE_UNKNOWN) {
		pr_err("unknown type\n");
	} else if (chg_type == POWER_SUPPLY_USB_TYPE_SDP ||
		chg_type == POWER_SUPPLY_USB_TYPE_CDP ||
		chg_type == POWER_SUPPLY_USB_TYPE_FLOAT) {
		upm6918_request_dpdm(upm, false);
		upm6918_notify_usb_host(upm, false);
		upm6918_notify_device_mode(upm, true);
		if (chg_type == POWER_SUPPLY_USB_TYPE_CDP) {
			upm6918_chg_current = CHG_CDP_CURR_MAX;
		} else {
			upm6918_chg_current = CHG_SDP_CURR_MAX;
		}
		upm6918_set_chargecurrent(upm, upm6918_chg_current);

	} else {
		pr_err("dcp deteced\n");
		upm6918_set_chargecurrent(upm, CHG_AFC_CURR_MAX);
		upm6918_enable_hvdcp(upm, true);
		upm6918_set_dp(upm, REG0C_DPDM_OUT_0P6V);
		upm6918_set_dm(upm, REG0C_DPDM_OUT_HIZ);
		upm6918_set_input_current_limit(upm, 500);
		schedule_delayed_work(&upm->hvdcp_dwork, msecs_to_jiffies(1400));
	}
	if((chg_type == POWER_SUPPLY_USB_TYPE_SDP) &&
		(upm->tcpc->pd_port.pd_connect_state == PD_CONNECT_PE_READY_SNK ||
		upm->tcpc->pd_port.pd_connect_state == PD_CONNECT_PE_READY_SNK_PD30)){
			upm6918_set_input_volt_limit(upm,4600);
			upm6918_set_input_current_limit(upm, 1200);
			upm6918_set_chargecurrent(upm, 1200);
	}
	pr_info("chg_type: %d,pd_active:%d\n", chg_type,upm->pd_active);
	*type = chg_type;
    	upm->unknown_type_recheck = false;

	return 0;
}

void upm6918_dump_regs(struct upm6918 *upm)
{
	u8 val;
	ssize_t len = 0;
	int ret, addr;
	char buf[128];

	memset(buf, 0, 128);
	for (addr = 0x0; addr <= 0x0C; addr++) {
		ret = upm6918_read_byte(upm, addr, &val);
		if (ret < 0) {
			pr_err("read %02x failed\n", addr);
			return;
		} else
			len += snprintf(buf + len, PAGE_SIZE, "%02x:%02x ", addr, val);
	}
	pr_info("%s\n", buf);
}
EXPORT_SYMBOL_GPL(upm6918_dump_regs);
static int upm6918_set_usb(struct upm6918 *upm, bool online_val)
{
	int ret = 0;
	union power_supply_propval propval;
	if(upm->usb_psy == NULL) {
		upm->usb_psy = power_supply_get_by_name("usb");
		if (upm->usb_psy == NULL) {
			pr_err("%s : fail to get psy usb\n", __func__);
			return -ENODEV;
		}
	}
	propval.intval = online_val;
	ret = power_supply_set_property(upm->usb_psy, POWER_SUPPLY_PROP_ONLINE, &propval);
	if (ret < 0)
		pr_err("%s : set upm6918_set_usb fail ret:%d\n", __func__, ret);
	return ret;
}
static irqreturn_t upm6918_irq_handler(int irq, void *data)
{
	int ret;
	u8 reg_val;
	bool prev_pg;
	bool prev_vbus_gd;
    struct upm6918 *upm = (struct upm6918 *)data;

	pr_err("upm6918 irq enter\n");
	upm6918_dump_regs(upm);

	ret = upm6918_read_byte(upm, UPM6918_REG_0A, &reg_val);
	if (ret)
		return IRQ_HANDLED;

	prev_vbus_gd = upm->vbus_gd;

	upm->vbus_gd = !!(reg_val & REG0A_VBUS_GD_MASK);

	ret = upm6918_read_byte(upm, UPM6918_REG_08, &reg_val);
	if (ret)
		return IRQ_HANDLED;

	prev_pg = upm->power_good;

	upm->power_good = !!(reg_val & REG08_PG_STAT_MASK);

	if (!prev_vbus_gd && upm->vbus_gd) {
		pr_err("adapter/usb inserted\n");
		if(!upm->pr_flag)
		{
			upm6918_request_dpdm(upm, true);
		}
          	ret = upm6918_set_chargevolt(upm, 4400);
		if (ret < 0) {
			pr_err("failed to set chargevoltage\n");
			return ret;
		}
		
		vote(upm->usb_icl_votable, DCP_CHG_VOTER, false, 0);
		vote(upm->usb_icl_votable, PD_CHG_VOTER, false, 0);
		vote(upm->fcc_votable, DCP_CHG_VOTER, false, 0);
		upm6918_set_usb(upm, true);
		upm6918_enable_term(upm, true);
	} else if (prev_vbus_gd && !upm->vbus_gd) {
	    upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	    upm->charge_afc = 0;
		upm6918_set_iio_channel(upm, NOPMI, NOPMI_CHG_CHARGER_AFC,upm->charge_afc);
		upm6918_set_usb(upm, false);
		//schedule_delayed_work(&upm->psy_dwork, 0);
		pr_err("adapter/usb removed\n");
		vote(upm->usb_icl_votable, DCP_CHG_VOTER, false, 0);
		vote(upm->usb_icl_votable, PD_CHG_VOTER, false, 0);
		vote(upm->fcc_votable, DCP_CHG_VOTER, false, 0);
		vote(upm->fcc_votable, STORE_MODE_VOTER, false, 0);
		vote(upm->usb_icl_votable, STORE_MODE_VOTER, false, 0);
		vote(upm->fcc_votable, AFC_CHG_DARK_VOTER, false, 0);
		upm6918_request_dpdm(upm, false);
		upm6918_notify_device_mode(upm, false);
		upm6918_set_iio_channel(upm, NOPMI, NOPMI_CHG_INPUT_SUSPEND, 0);
		upm6918_power_changed(upm);
		upm->pr_flag = false;
        	upm->unknown_type_recheck = false;
        	upm->unknown_type_recheck_cnt = 0;
		return IRQ_HANDLED;
	}

	if ((!prev_pg && upm->power_good) || upm->unknown_type_recheck) {
		ret = upm6918_get_charger_type(upm, &upm->psy_usb_type);
        if (!upm->unknown_type_recheck) {
    		msleep(600);
    		pr_err("--%s:delay 600ms psy changed",__func__);
    		upm6918_power_changed(upm);
        }
		//schedule_delayed_work(&upm->psy_dwork, 0);
	}

	return IRQ_HANDLED;
}

static int upm6918_register_interrupt(struct upm6918 *upm)
{
	int ret = 0;
	ret = devm_gpio_request_one(upm->dev, upm->intr_gpio, GPIOF_DIR_IN,
			devm_kasprintf(upm->dev, GFP_KERNEL,
			"upm6918_intr_gpio.%s", dev_name(upm->dev)));
	if (ret < 0) {
		pr_err("%s gpio request fail(%d)\n",
				      __func__, ret);
		return ret;
	}
	upm->client->irq = gpio_to_irq(upm->intr_gpio);
	if (upm->client->irq < 0) {
		pr_err("%s gpio2irq fail(%d)\n",
				      __func__, upm->client->irq);
		return upm->client->irq;
	}
	pr_err("%s irq = %d\n", __func__, upm->client->irq);

	ret = devm_request_threaded_irq(upm->dev, upm->client->irq, NULL,
					upm6918_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"upm_irq", upm);
	if (ret < 0) {
		pr_err("request thread irq failed:%d\n", ret);
		return ret;
	}

	enable_irq_wake(upm->irq);

	return 0;
}

static int upm6918_init_device(struct upm6918 *upm)
{
	int ret;

	upm6918_disable_watchdog_timer(upm);

	ret = upm6918_set_stat_ctrl(upm, upm->platform_data->statctrl);
	if (ret)
		pr_err("Failed to set stat pin control mode, ret = %d\n", ret);

	ret = upm6918_set_prechg_current(upm, upm->platform_data->iprechg);
	if (ret)
		pr_err("Failed to set prechg current, ret = %d\n", ret);

	upm->platform_data->iterm = upm->platform_data->switch_iterm;
	ret = upm6918_set_term_current(upm, upm->platform_data->iterm);
	if (ret)
		pr_err("Failed to set termination current, ret = %d\n", ret);

	ret = upm6918_set_boost_voltage(upm, upm->platform_data->boostv);
	if (ret)
		pr_err("Failed to set boost voltage, ret = %d\n", ret);

	ret = upm6918_set_boost_current(upm, upm->platform_data->boosti);
	if (ret)
		pr_err("Failed to set boost current, ret = %d\n", ret);

	ret = upm6918_set_acovp_threshold(upm, upm->platform_data->vac_ovp);
	if (ret)
		pr_err("Failed to set acovp threshold, ret = %d\n", ret);

	ret = upm6918_set_chargevolt(upm, 4400);
	if (ret)
		pr_err("Failed to set batteyr vreg, ret = %d\n", ret);

	ret = upm6918_set_chargecurrent(upm, upm->platform_data->usb.ichg);
	if (ret)
		pr_err("Failed to set battery ichg, ret = %d\n", ret);

	ret = upm6918_set_input_volt_limit(upm, upm->platform_data->usb.vlim);
	if (ret)
		pr_err("Failed to set battery vlim, ret = %d\n", ret);

	ret = upm6918_set_int_mask(upm,
				   REG0A_IINDPM_INT_MASK |
				   REG0A_VINDPM_INT_MASK);
	if (ret)
		pr_err("Failed to set vindpm and iindpm int mask\n");

	ret = upm6918_batfet_rst_en(upm, false);
        if (ret)
                pr_err("Failed to set batfet_rst_en\n");

	return 0;
}

static void upm6918_inform_prob_dwork_handler(struct work_struct *work)
{
	struct upm6918 *upm = container_of(work, struct upm6918,
								prob_dwork.work);
	upm6918_force_dpdm(upm);
	msleep(1000);
	upm6918_irq_handler(upm->irq, (void *) upm);
	if((upm->psy_usb_type == POWER_SUPPLY_USB_TYPE_SDP) &&
		(upm->tcpc->pd_port.pd_connect_state == PD_CONNECT_PE_READY_SNK ||
		upm->tcpc->pd_port.pd_connect_state == PD_CONNECT_PE_READY_SNK_PD30)){
			upm6918_set_input_volt_limit(upm,4600);
			upm6918_set_input_current_limit(upm, 1200);
			upm6918_set_chargecurrent(upm, 1200);
			pr_err("%s: set current in prob\n", __func__);
	}
}

static void upm6918_set_otg(struct upm6918 *upm, int enable)
{
	int ret;

	if (enable) {
		
		upm6918_disable_charger(upm);
		
		ret = upm6918_enable_otg(upm);
		if (ret < 0) {
			pr_err("%s:Failed to enable otg-%d\n", __func__, ret);
			return;
		}
	} else{
		ret = upm6918_disable_otg(upm);
		if (ret < 0){
			pr_err("%s:Failed to disable otg-%d\n", __func__, ret);
		}
		upm6918_enable_charger(upm);
		
	}
}

static bool is_ds_chan_valid(struct upm6918 *upm,
		enum ds_ext_iio_channels chan)
{
	int rc;
	if (IS_ERR(upm->ds_ext_iio_chans[chan]))
		return false;
	if (!upm->ds_ext_iio_chans[chan]) {
		upm->ds_ext_iio_chans[chan] = iio_channel_get(upm->dev,
					ds_ext_iio_chan_name[chan]);
		if (IS_ERR(upm->ds_ext_iio_chans[chan])) {
			rc = PTR_ERR(upm->ds_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				upm->ds_ext_iio_chans[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				ds_ext_iio_chan_name[chan], rc);
			return false;
		}
	}
	return true;
}

static bool is_bms_chan_valid(struct upm6918 *upm,
		enum fg_ext_iio_channels chan)
{
	int rc;
	if (IS_ERR(upm->fg_ext_iio_chans[chan]))
		return false;
	if (!upm->fg_ext_iio_chans[chan]) {
		upm->fg_ext_iio_chans[chan] = iio_channel_get(upm->dev,
					fg_ext_iio_chan_name[chan]);
		if (IS_ERR(upm->fg_ext_iio_chans[chan])) {
			rc = PTR_ERR(upm->fg_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				upm->fg_ext_iio_chans[chan] = NULL;
			pr_err("bms_chan_valid Failed to get fg_ext_iio channel %s, rc=%d\n",
				fg_ext_iio_chan_name[chan], rc);
			return false;
		}
	}
	return true;
}

static bool is_nopmi_chg_chan_valid(struct upm6918 *upm,
		enum nopmi_chg_ext_iio_channels chan)
{
	int rc;
	if (IS_ERR(upm->nopmi_chg_ext_iio_chans[chan]))
		return false;
	if (!upm->nopmi_chg_ext_iio_chans[chan]) {
		upm->nopmi_chg_ext_iio_chans[chan] = iio_channel_get(upm->dev,
					nopmi_chg_ext_iio_chan_name[chan]);
		if (IS_ERR(upm->nopmi_chg_ext_iio_chans[chan])) {
			rc = PTR_ERR(upm->nopmi_chg_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				upm->nopmi_chg_ext_iio_chans[chan] = NULL;
			pr_err("nopmi_chg_chan_valid failed to get IIO channel %s, rc=%d\n",
				nopmi_chg_ext_iio_chan_name[chan], rc);
			return false;
		}
	}
	return true;
}

int upm6918_get_iio_channel(struct upm6918 *upm,
			enum upm6918_iio_type type, int channel, int *val)
{
	struct iio_channel *iio_chan_list;
	int rc;
/*
	if(upm->shutdown_flag)
		return -ENODEV;
*/
	switch (type) {
	case DS28E16:
		if (!is_ds_chan_valid(upm, channel))
			return -ENODEV;
		iio_chan_list = upm->ds_ext_iio_chans[channel];
		break;
	case BMS:
		if (!is_bms_chan_valid(upm, channel))
			return -ENODEV;
		iio_chan_list = upm->fg_ext_iio_chans[channel];
		break;
	case NOPMI:
		if (!is_nopmi_chg_chan_valid(upm, channel))
			return -ENODEV;
		iio_chan_list = upm->nopmi_chg_ext_iio_chans[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}
	rc = iio_read_channel_processed(iio_chan_list, val);
	return rc < 0 ? rc : 0;
}

int upm6918_set_iio_channel(struct upm6918 *upm,
			enum upm6918_iio_type type, int channel, int val)
{
	struct iio_channel *iio_chan_list;
	int rc;
/*
	if(chg->shutdown_flag)
		return -ENODEV;
*/
	switch (type) {
	case DS28E16:
		if (!is_ds_chan_valid(upm, channel)){
			pr_err("%s: iio_type is DS28E16 \n",__func__);
			return -ENODEV;
		}
		iio_chan_list = upm->ds_ext_iio_chans[channel];
		break;
	case BMS:
		if (!is_bms_chan_valid(upm, channel)){
			pr_err("%s: iio_type is BMS \n",__func__);
			return -ENODEV;
		}
			
		iio_chan_list = upm->fg_ext_iio_chans[channel];
		break;
	case NOPMI:
		if (!is_nopmi_chg_chan_valid(upm, channel)){
			pr_err("%s: iio_type is NOPMI \n",__func__);
			return -ENODEV;
		}
		iio_chan_list = upm->nopmi_chg_ext_iio_chans[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		pr_err("iio_type %d is not supported\n", type);
		return -EINVAL;
	}
	rc = iio_write_channel_raw(iio_chan_list, val);
	return rc < 0 ? rc : 0;
}

static int get_source_mode(struct tcp_notify *noti)
{
	if (noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC || noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC)
		return QTI_POWER_SUPPLY_TYPEC_SOURCE_DEFAULT;
	switch (noti->typec_state.rp_level) {
	case TYPEC_CC_VOLT_SNK_1_5:
		return QTI_POWER_SUPPLY_TYPEC_SOURCE_MEDIUM;
	case TYPEC_CC_VOLT_SNK_3_0:
		return QTI_POWER_SUPPLY_TYPEC_SOURCE_HIGH;
	case TYPEC_CC_VOLT_SNK_DFT:
		return QTI_POWER_SUPPLY_TYPEC_SOURCE_DEFAULT;
	default:
		break;
	}
	return QTI_POWER_SUPPLY_TYPEC_NONE;
}

static int upm6918_set_cc_orientation(struct upm6918 *upm, int cc_orientation)
{
	int ret = 0;

	union power_supply_propval propval;
	if(upm->usb_psy == NULL) {
		upm->usb_psy = power_supply_get_by_name("usb");
		if (upm->usb_psy == NULL) {
			pr_err("%s : fail to get psy usb\n", __func__);
			return -ENODEV;
		}
	}
	propval.intval = cc_orientation;
	ret = upm6918_set_iio_channel(upm, NOPMI, NOPMI_CHG_TYPEC_CC_ORIENTATION, propval.intval); 
	if (ret < 0)
		pr_err("%s : set prop CC_ORIENTATION fail ret:%d\n", __func__, ret);
	return ret;
}

static int upm6918_set_typec_mode(struct upm6918 *upm, enum power_supply_typec_mode typec_mode)
{
	int ret = 0;
	union power_supply_propval propval;
	
	if(upm->usb_psy == NULL) {
		upm->usb_psy = power_supply_get_by_name("usb");
		if (upm->usb_psy == NULL) {
			pr_err("%s : fail to get psy usb\n", __func__);
			return -ENODEV;
		}
	}
	propval.intval = typec_mode;
	ret = upm6918_set_iio_channel(upm, NOPMI, NOPMI_CHG_TYPEC_MODE, propval.intval);
	if (ret < 0)
		pr_err("%s : set prop TYPEC_MODE fail ret:%d\n", __func__, ret);
	return ret;
}

static int upm6918_tcpc_notifier_call(struct notifier_block *pnb,
                    unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct upm6918 *upm;
	int vbus_stat = 0 ;
	int cc_orientation = 0;
	u8 reg_val = 0;
	int ret;
	enum power_supply_typec_mode typec_mode = QTI_POWER_SUPPLY_TYPEC_NONE;

	upm = container_of(pnb, struct upm6918, tcp_nb);
	if (upm) {
		ret = upm6918_read_byte(upm, UPM6918_REG_08, &reg_val);
	}
	vbus_stat = (reg_val & REG08_VBUS_STAT_MASK);
	vbus_stat >>= REG08_VBUS_STAT_SHIFT;
	switch (event) {
	case TCP_NOTIFY_PD_STATE:
		switch(noti->pd_state.connected) {
			case PD_CONNECT_NONE:
				upm->pd_active = 0;
				break;
			case PD_CONNECT_HARD_RESET:
				break;
			case PD_CONNECT_PE_READY_SNK:
			case PD_CONNECT_PE_READY_SNK_PD30:
				pr_err("ch_log --event[0x%lx]  pd_state[%d]\n",event,noti->pd_state.connected);
				if(upm->psy_usb_type == POWER_SUPPLY_USB_TYPE_SDP){
					upm6918_set_input_volt_limit(upm,4600);
					upm6918_set_input_current_limit(upm, 1200);
					upm6918_set_chargecurrent(upm, 1200);
				}
			case PD_CONNECT_PE_READY_SNK_APDO:
				upm->pd_active = 1;
				break;
		}
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
		    (noti->typec_state.new_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC)) {
			pr_err("%s:USB Plug in, cc_or_pol = %d, state = %d vbus_stat=%d\n",__func__,
					noti->typec_state.polarity, noti->typec_state.new_state, vbus_stat);
			//if( upm6918_get_vbus_type(upm) == UPM6918_VBUS_NONE || upm6918_get_vbus_type(upm) ==  UPM6918_VBUS_UNKNOWN
			//		|| upm->chg_type == POWER_SUPPLY_TYPE_UNKNOWN) {
			if (vbus_stat == REG08_VBUS_TYPE_NONE) {
				pr_err("%s vbus type is none, do force dpdm now.\n",__func__);
				upm6918_init_device(upm);
				upm6918_request_dpdm(upm, true);
			}
			typec_mode = get_source_mode(noti);
			cc_orientation = noti->typec_state.polarity;
			upm6918_set_cc_orientation(upm, cc_orientation);
			pr_debug("%s: cc_or_val = %d\n",__func__, cc_orientation);
		} else if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			typec_mode = QTI_POWER_SUPPLY_TYPEC_SINK;
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_NORP_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_SRC)
			&& noti->typec_state.new_state == TYPEC_UNATTACHED) {
			typec_mode = QTI_POWER_SUPPLY_TYPEC_NONE;
			pr_err("USB Plug out\n");
			upm6918_request_dpdm(upm, false);
			upm->pr_flag = false;
		} else if (noti->typec_state.old_state == TYPEC_ATTACHED_SRC &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SNK) {
			pr_err("Source_to_Sink \n");
			typec_mode = QTI_POWER_SUPPLY_TYPEC_SINK;
		}  else if (noti->typec_state.old_state == TYPEC_ATTACHED_SNK &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			typec_mode = get_source_mode(noti);
			pr_err("Sink_to_Source \n");
		}
		if(typec_mode >= QTI_POWER_SUPPLY_TYPEC_NONE && typec_mode <= QTI_POWER_SUPPLY_TYPEC_NON_COMPLIANT)
			upm6918_set_typec_mode(upm, typec_mode);
		break;

	case TCP_NOTIFY_SOURCE_VBUS:
		pr_err("vbus_on:%d TCP_NOTIFY_SOURCE_VBUS\n", vbus_on);
		if ((noti->vbus_state.mv == TCPC_VBUS_SOURCE_0V) && (vbus_on)) {
			/* disable OTG power output */
			pr_err("otg plug out and power out !\n");
			vbus_on = false;
			upm6918_set_otg(upm, false);
			upm6918_request_dpdm(upm, false);
		} else if ((noti->vbus_state.mv == TCPC_VBUS_SOURCE_5V) && (!vbus_on)) {
			/* enable OTG power output */
			pr_err("otg plug in and power on !\n");
			vbus_on = true;
			upm6918_request_dpdm(upm, false);
			upm6918_set_otg(upm, true);
			//upm6918set_otg_current(upm, upm->cfg.charge_current_1500);
		}
		break;
	case TCP_NOTIFY_PR_SWAP:
		    pr_err("[ch_log]: otg pr_swap start\n");
		    upm->pr_flag = true;
		break;
	}
	return NOTIFY_OK;
}

static void upm6918_register_tcpc_notify_dwork_handler(struct work_struct *work)
{
	struct upm6918 *upm = container_of(work, struct upm6918,
								tcpc_dwork.work);
	int ret;

    if (!upm->tcpc) {
        upm->tcpc = tcpc_dev_get_by_name("type_c_port0");
        if (!upm->tcpc) {
		pr_err("get tcpc dev fail\n");
		schedule_delayed_work(&upm->tcpc_dwork, msecs_to_jiffies(2*1000));
		return;
        }
    }
    if(upm->tcpc) {
        upm->tcp_nb.notifier_call = upm6918_tcpc_notifier_call;
        ret = register_tcp_dev_notifier(upm->tcpc, &upm->tcp_nb,
                        TCP_NOTIFY_TYPE_ALL);
        if (ret < 0) {
            pr_err("register tcpc notifier fail\n");
        }
    }
  
    if(upm->tcpc->typec_state == ATTACHEDSRC)
    {
	pr_err("otg plug in and power on !\n");
	vbus_on = true;
	upm6918_request_dpdm(upm, false);
	upm6918_set_otg(upm, true);
    }
}

static int upm6918_detect_device(struct upm6918 *upm)
{
	int ret;
	u8 data;

	ret = upm6918_read_byte(upm, UPM6918_REG_0B, &data);
	if (!ret) {
		upm->part_no = (data & REG0B_PN_MASK) >> REG0B_PN_SHIFT;
		upm->revision =
		    (data & REG0B_DEV_REV_MASK) >> REG0B_DEV_REV_SHIFT;
	}

	if (upm->part_no != REG0B_PN_UPM6918) {
		pr_err("part number is not upm6918dl, data:0x%x\n", data);
		ret = -EINVAL;
	}

	return ret;
}

static ssize_t upm6918_show_registers(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct upm6918 *upm = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "upm6918 Reg");
	for (addr = 0x0; addr <= 0x0B; addr++) {
		ret = upm6918_read_byte(upm, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
				       "Reg[%.2x] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t upm6918_store_registers(
	struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct upm6918 *upm = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg < 0x0B) {
		upm6918_write_byte(upm, (unsigned char) reg,
				   (unsigned char) val);
	}

	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR,
	upm6918_show_registers, upm6918_store_registers);

static ssize_t upm6918_store_shipmode(
	struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct upm6918 *upm = dev_get_drvdata(dev);

	if (buf[0] == '1') {
		upm->entry_shipmode = true;
	}

	return count;
}

static DEVICE_ATTR(shipmode, S_IWUSR,
	NULL, upm6918_store_shipmode);

static struct attribute *upm6918_attributes[] = {
	&dev_attr_shipmode.attr,
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group upm6918_attr_group = {
	.attrs = upm6918_attributes,
};
static int upm6918_get_pd_active(struct upm6918 *upm)
{
	int pd_active = 0;
	int rc = 0;
	if (!upm->tcpc)
		pr_err("tcpc_dev is null\n");
	else {
		rc = tcpm_get_pd_connect_type(upm->tcpc);
		pr_err("%s: pd is %d\n", __func__, rc);
		if ((rc == PD_CONNECT_PE_READY_SNK_APDO) || (rc == PD_CONNECT_PE_READY_SNK_PD30)
			|| (rc == PD_CONNECT_PE_READY_SNK)) {
			pd_active = 1;
		} else {
			pd_active = 0;
		}
	}


	if (upm->pd_active != pd_active) {
		upm->pd_active = pd_active;
	}
	return pd_active;
}

static int upm6918_get_pdo_active(struct upm6918 *upm)
{
	int pdo_active = 0;
	int rc = 0;
	if (!upm->tcpc)
		pr_err("tcpc_dev is null\n");
	else {
		rc = tcpm_get_pd_connect_type(upm->tcpc);
		pr_err("%s: pdo is %d\n", __func__, rc);
		if ((rc == PD_CONNECT_PE_READY_SNK_PD30) || (rc == PD_CONNECT_PE_READY_SNK)) {
			pdo_active = 1;
		} else {
			pdo_active = 0;
		}
	}

	return pdo_active;
}

static void upm6918_disable_pdo_dwork(struct work_struct *work)
{
	int pdo_icl_current = 0;
	struct upm6918 *upm = container_of(
		work, struct upm6918, disable_pdo_dwork.work);

	if (upm->is_disble_afc) {
		pdo_icl_current = PD_ICL_CURR_MAX;
	} else {
		pdo_icl_current = DCP_ICL_CURR_MAX;
	}

	upm6918_set_input_current_limit(upm, pdo_icl_current);

}

static void upm6918_disable_pdo(struct upm6918 *upm)
{
	upm6918_set_input_current_limit(upm, 500);

	cancel_delayed_work_sync(&upm->disable_pdo_dwork);
	schedule_delayed_work(&upm->disable_pdo_dwork, msecs_to_jiffies(500));
}

static void upm6918_disable_afc_dwork(struct work_struct *work)
{
	unsigned int afc_code = AFC_QUICK_CHARGE_POWER_CODE;
	int afc_icl_current = 0;
	int afc_icharge_current = 0;
	int ret = 0;
	struct upm6918 *upm = container_of(
		work, struct upm6918, disable_afc_dwork.work);

	if (upm->is_disble_afc) {
		afc_code = AFC_COMMIN_CHARGE_POWER_CODE;
		afc_icl_current = AFC_COMMON_ICL_CURR_MAX;
		afc_icharge_current = CHG_AFC_COMMON_CURR_MAX;
	} else {
		afc_code = AFC_QUICK_CHARGE_POWER_CODE;
		afc_icl_current = AFC_ICL_CURR_MAX;
		afc_icharge_current = CHG_AFC_CURR_MAX;
	}

	pr_err("%s: afc_code=%x\n", __func__,afc_code);

	ret = afc_set_voltage_workq(afc_code);
	if (!ret) {
		pr_info("set afc adapter iindpm\n");
		upm6918_set_input_current_limit(upm, afc_icl_current);
		upm6918_set_chargecurrent(upm, afc_icharge_current);
	}
}

static void upm6918_disable_afc(struct upm6918 *upm)
{
	upm6918_set_input_current_limit(upm, 500);

	cancel_delayed_work_sync(&upm->disable_afc_dwork);
	schedule_delayed_work(&upm->disable_afc_dwork, msecs_to_jiffies(500));
}

static void upm6918_hvdcp_dwork(struct work_struct *work)
{
	int ret;
	unsigned int afc_code = AFC_QUICK_CHARGE_POWER_CODE;
	int afc_icl_current = 0;
	int pd_active = 0;
	int pd_pdo_active = 0;
	struct upm6918 *upm = container_of(
		work, struct upm6918, hvdcp_dwork.work);
	pr_err("is_disble_afc=%d\n", upm->is_disble_afc);
	if (upm->is_disble_afc) {
		afc_code = AFC_COMMIN_CHARGE_POWER_CODE;
		afc_icl_current = AFC_COMMON_ICL_CURR_MAX;
	} else {
		afc_code = AFC_QUICK_CHARGE_POWER_CODE;
		afc_icl_current = AFC_ICL_CURR_MAX;
	}
	ret = afc_set_voltage_workq(afc_code);
	if (!ret) {
		pr_err("set afc adapter iindpm to 1700mA\n");
		upm6918_set_input_current_limit(upm, afc_icl_current);
		upm->charge_afc = 1;
		upm6918_set_iio_channel(upm, NOPMI, NOPMI_CHG_CHARGER_AFC,upm->charge_afc);
		pr_err("ch_log_afc   pd_active[%d]   charge_afc[%d]\n",upm->pd_active,upm->charge_afc);
		upm6918_set_term_current(upm, AFC_CHARGE_ITERM);
		if (upm->is_disble_afc) {
			upm6918_set_chargecurrent(upm, CHG_AFC_COMMON_CURR_MAX);
		}
	} else {
		pr_err("afc communication failed, restore adapter iindpm to 3000mA\n");
		upm6918_set_chargecurrent(upm, CHG_DCP_CURR_MAX);
		//upm6918_set_input_current_limit(upm, 2000);
		pd_active = upm6918_get_pd_active(upm);
		pd_pdo_active = upm6918_get_pdo_active(upm);
		if (upm->is_disble_afc && (pd_active == 1)) {
			vote(upm->usb_icl_votable, PD_CHG_VOTER, true, PD_ICL_CURR_MAX);
		} else if (pd_pdo_active == 1) {
			vote(upm->usb_icl_votable, DCP_CHG_VOTER, true, 1600);
		} else {
			vote(upm->usb_icl_votable, DCP_CHG_VOTER, true, DCP_ICL_CURR_MAX);
		}
		upm->charge_afc = 0;
		pr_err("ch_log_qc   pd_active[%d] pd_pdo_active[%d]  charge_afc[%d]\n",upm->pd_active,pd_pdo_active,upm->charge_afc);
		if (!upm->pd_active) {
			schedule_delayed_work(&upm->hvdcp_qc20_dwork, msecs_to_jiffies(300));
		}
	}
}

static void upm6918_hvdcp_qc20_dwork(struct work_struct *work)
{
	int ret;
	union power_supply_propval pval = {0, };
	struct upm6918 *upm = container_of(
		work, struct upm6918, hvdcp_qc20_dwork.work);

	__pm_stay_awake(upm->hvdcp_qc20_ws);

	ret = upm6918_set_input_current_limit(upm, 500);
	if (ret) {
		pr_err("set ibus 500ma failed, ret:%d\n", ret);
		goto qc20_out;
	}

	ret = upm6918_set_dp(upm, REG0C_DPDM_OUT_0P6V);
	if (ret) {
		pr_err("set dp 0.6v failed, ret:%d\n", ret);
		goto qc20_out;
	}

	upm6918_set_dm(upm, REG0C_DPDM_OUT_0V);
	if (ret) {
		pr_err("set dm 0v failed, ret:%d\n", ret);
		goto qc20_out;
	}

	msleep(300);

	ret = upm6918_set_dp(upm, REG0C_DPDM_OUT_3P3V);
	if (ret) {
		pr_err("set dp 3.3v failed, ret:%d\n", ret);
		goto qc20_out;
	}

	upm6918_set_dm(upm, REG0C_DPDM_OUT_0P6V);
	if (ret) {
		pr_err("set dm 0.6v failed, ret:%d\n", ret);
		goto qc20_out;
	}

	msleep(300);
	pr_err("hvdcp qc20 detected\n");

	if (upm->cp_psy == NULL)
	{
		upm->cp_psy = power_supply_get_by_name("charger_standalone");
		if (upm->cp_psy == NULL) {
			pr_err("power_supply_get_by_name bms fail !\n");
			goto qc20_out;
		}
	}
	power_supply_get_property(upm->cp_psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &pval);
	pr_err("ch_log_qc [vbus][%d] [pd_active][%d]\n",pval.intval,upm->pd_active);
	if (pval.intval > 8000  && !upm->pd_active) {
		upm6918_set_input_current_limit(upm, AFC_ICL_CURR_MAX);
		upm6918_set_chargecurrent(upm, CHG_AFC_CURR_MAX);
		upm6918_dump_regs(upm);
	} else {
		goto qc20_out;
	}

	__pm_relax(upm->hvdcp_qc20_ws);
	return;
qc20_out:
	upm6918_set_input_current_limit(upm, DCP_ICL_CURR_MAX);
	__pm_relax(upm->hvdcp_qc20_ws);
	return;
}

static void upm6918_notify_usb_host(struct upm6918 *upm, bool enable)
{
	union extcon_property_value val = {.intval = 0};

	val.intval = 1;
	if (enable) {
		extcon_set_property(upm->extcon, EXTCON_USB_HOST, EXTCON_PROP_USB_SS, val);
	}

	extcon_set_state_sync(upm->extcon, EXTCON_USB_HOST, enable);
}

static void upm6918_notify_extcon_props(struct upm6918 *upm, int id)
{
	union extcon_property_value val;
	//int prop_val;

	//upm6918_get_prop_typec_cc_orientation(chg, &prop_val);
	//val.intval = 1;//((prop_val == 2) ? 1 : 0);
	//extcon_set_property(upm->extcon, id, EXTCON_PROP_USB_TYPEC_POLARITY, val);
	val.intval = true;
	extcon_set_property(upm->extcon, id, EXTCON_PROP_USB_SS, val);
}

static void upm6918_notify_device_mode(struct upm6918 *upm, bool enable)
{
	if (enable)
		upm6918_notify_extcon_props(upm, EXTCON_USB);

	extcon_set_state_sync(upm->extcon, EXTCON_USB, enable);
}

int upm6918_extcon_init(struct upm6918 *upm)
{
	int rc;

	pr_info("start !\n");

	if(!upm){
		pr_err("upm is NULL,Fail !\n");
		return -EINVAL;
	}

	/* extcon registration */
	upm->extcon = devm_extcon_dev_allocate(upm->dev, upm6918_extcon_cable);
	if (IS_ERR(upm->extcon)) {
		rc = PTR_ERR(upm->extcon);
		pr_err("failed to allocate extcon device rc=%d\n", rc);
		return rc;
	}

	rc = devm_extcon_dev_register(upm->dev, upm->extcon);
	if (rc < 0) {
		pr_err("failed to register extcon device rc=%d\n", rc);
		return rc;
	}

	/* Support reporting polarity and speed via properties */
	rc = extcon_set_property_capability(upm->extcon,
			EXTCON_USB, EXTCON_PROP_USB_TYPEC_POLARITY);
	rc |= extcon_set_property_capability(upm->extcon,
			EXTCON_USB, EXTCON_PROP_USB_SS);
	rc |= extcon_set_property_capability(upm->extcon,
			EXTCON_USB_HOST, EXTCON_PROP_USB_TYPEC_POLARITY);
	rc |= extcon_set_property_capability(upm->extcon,
			EXTCON_USB_HOST, EXTCON_PROP_USB_SS);
	if (rc < 0)
		pr_err("failed to configure extcon capabilities\n");

	return rc;
}

static struct of_device_id upm6918_charger_match_table[] = {
	{
	 .compatible = "upm,upm6918d",
	 .data = &pn_data[PN_UPM6918D],
	 },
	{},
};
MODULE_DEVICE_TABLE(of, upm6918_charger_match_table);


static enum power_supply_property upm6918_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_MODEL_NAME,
};

static int upm6918_charger_get_property(struct power_supply *psy,
                enum power_supply_property psp,
                union power_supply_propval *val)
{
    struct upm6918 *upm = power_supply_get_drvdata(psy);
	int ret = 0;
	u8 pg_stat = 0;
	ret = upm6918_read_byte(upm, UPM6918_REG_08, &pg_stat);
	if (!ret)
		pg_stat = !!(pg_stat & REG08_PG_STAT_MASK);
	else
		pg_stat = upm->power_good;
	if(!upm){
		pr_err("upm is NULL,Fail !\n");
		return -EINVAL;
	}

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			ret = upm6918_get_charging_status(upm);
			pr_err("original val=%d\n", ret);
			if(ret >= 0) {
				switch (ret) {
					case CHRG_STAT_NOT_CHARGING:
						if (!pg_stat)
							val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
						else
							val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
						break;
					case CHRG_STAT_PRE_CHARGINT:
					case CHRG_STAT_FAST_CHARGING:
						val->intval = POWER_SUPPLY_STATUS_CHARGING;
						break;
					case CHRG_STAT_CHARGING_TERM:
						if (!pg_stat)
							val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
						else
							val->intval = POWER_SUPPLY_STATUS_FULL;
						break;
					default:
						val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
						break;
				}
				ret = 0;
			} else {
				val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
			}
			break;
		case POWER_SUPPLY_PROP_CHARGE_TYPE:
			if (pg_stat)
				val->intval = upm->psy_usb_type;
			else
				val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
			break;

		case POWER_SUPPLY_PROP_ONLINE:
			val->intval = pg_stat;
			break;
		case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
			val->intval = upm->platform_data->iterm;
			break;
		case POWER_SUPPLY_PROP_MODEL_NAME:
			val->strval = upm6918_driver_name;
			break;
		default:
			pr_err("unsupport power_supply_property: %d.\n",psp);
			ret = -EINVAL;
			break;
	}
	pr_info("prop:%d, ret:%d, val->intval:%d\n", psp, ret, val->intval);

	return ret;
}

static int upm6918_charger_set_property(struct power_supply *psy,
                    enum power_supply_property prop,
                    const union power_supply_propval *val)
{
 	struct upm6918 *upm = power_supply_get_drvdata(psy);
	int ret = 0;

	pr_err("prop %d, val->intval %d", prop, val->intval);
	if(!upm){
		pr_err("upm is NULL,Fail !\n");
		return -EINVAL;
	}

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		upm->chg_type = val->intval;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		upm->chg_type = val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		if (val->intval == 0) {
			ret = upm6918_enable_term(upm, true);
		} else if (val->intval == -1) {
			ret = upm6918_enable_term(upm, false);
		} else {
			upm->platform_data->iterm = val->intval;
			ret = upm6918_set_term_current(upm, upm->platform_data->iterm);
		}
		break;

	default:
		pr_err("Property is unable set: %d.\n", val->intval);
		return -EINVAL;
	}

	return ret;
}

static int upm6918_charger_is_writeable(struct power_supply *psy,
                    enum power_supply_property prop)
{
	int ret = 0;

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		return 1;

	default:
		break;
	}

    return ret;
}

static int upm6918_charger_psy_register(struct upm6918 *upm)
{
	pr_info("start !\n");

	if(!upm){
		pr_err("upm is NULL,Fail !\n");
		return -EINVAL;
	}

    upm->chg_psy_cfg.drv_data = upm;
    upm->chg_psy_cfg.of_node = upm->dev->of_node;

    upm->chg_psy_desc.name = "bbc";
    upm->chg_psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
    upm->chg_psy_desc.properties = upm6918_charger_props;
    upm->chg_psy_desc.num_properties = ARRAY_SIZE(upm6918_charger_props);
    upm->chg_psy_desc.get_property = upm6918_charger_get_property;
    upm->chg_psy_desc.set_property = upm6918_charger_set_property;
    upm->chg_psy_desc.property_is_writeable = upm6918_charger_is_writeable;

    upm->charger_psy = devm_power_supply_register(upm->dev, 
            &upm->chg_psy_desc, &upm->chg_psy_cfg);
    if (IS_ERR(upm->charger_psy)) {
        pr_err("failed to register charger_psy\n");
        return PTR_ERR(upm->charger_psy);
    }

    pr_err("%s power supply register successfully\n", upm->chg_psy_desc.name);

    return 0;
}

/* longcheer nielianjie10 2023.4.26 add iio start */

int upm6918_chg_get_iio_channel(struct upm6918 *upm,
			enum upm6918_iio_type type, int channel, int *val)
{
	struct iio_channel *iio_chan_list = NULL;
	int rc = 0;

	switch (type) {
		default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}

	rc = iio_read_channel_processed(iio_chan_list, val);

	return rc < 0 ? rc : 0;
}
EXPORT_SYMBOL(upm6918_chg_get_iio_channel);

static int upm6918_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct upm6918 *upm = iio_priv(indio_dev);
	int rc = 0;
	*val1 = 0;

	if(!upm){
		pr_err("upm is NULL,Fail !\n");
		return -EINVAL;
	}

	switch (chan->channel) {
	case PSY_IIO_CHARGE_TYPE:
		rc = upm6918_get_charging_status(upm);
		switch (rc) {
			case CHRG_STAT_NOT_CHARGING:
				*val1 = POWER_SUPPLY_CHARGE_TYPE_NONE;
				break;
			case CHRG_STAT_PRE_CHARGINT:
				*val1 = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
				break;
			case CHRG_STAT_FAST_CHARGING:
				*val1 = POWER_SUPPLY_CHARGE_TYPE_FAST;
				break;
			case CHRG_STAT_CHARGING_TERM:
				*val1 = POWER_SUPPLY_CHARGE_TYPE_NONE;
				break;
			default:
				*val1 = POWER_SUPPLY_CHARGE_TYPE_NONE;
				break;
		}
		break;
	case PSY_IIO_USB_MA:
		*val1 = upm6918_get_input_current_limit(upm);
		break;
	case PSY_IIO_CHARGE_AFC:
		*val1 = upm->charge_afc;
		break;
	default:
		pr_err("Unsupported upm6918 IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	pr_info("read IIO channel %d, rc = %d, val1 = %d\n", chan->channel, rc, val1);
	if (rc < 0) {
		pr_err("Couldn't read IIO channel %d, rc = %d\n",
			chan->channel, rc);
		return rc;
	}
	return IIO_VAL_INT;
}

static int upm6918_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct  upm6918 *upm = iio_priv(indio_dev);
	int rc = 0;

	if(!upm){
		pr_err("upm is NULL,Fail !\n");
		return -EINVAL;
	}

	pr_info("Write IIO channel %d, val = %d\n", chan->channel, val1);
	switch (chan->channel) {
	case PSY_IIO_CHARGE_TYPE:
		upm->chg_type = val1;
		break;
	case PSY_IIO_CHARGE_ENABLED:
		upm6918_enable_charger(upm);
		 pr_err("Write IIO channel %d, val = %d\n", chan->channel, val1);
		break;
	case PSY_IIO_CHARGE_DISABLE:
		upm6918_disable_charger(upm);
		pr_err("Write IIO channel %d, val = %d\n", chan->channel, val1);
		break;
	case PSY_IIO_CHARGE_AFC_DISABLE:
		upm->is_disble_afc = val1;

		if (upm->is_disble_afc_backup != upm->is_disble_afc) {
			if (upm->charge_afc == 1) {
				pr_err("disable afc\n");
				upm6918_disable_afc(upm);
			} else if ((upm6918_get_pdo_active(upm) == 1)) {
				pr_err("disable pdo\n");
				upm6918_disable_pdo(upm);
			}
			upm->is_disble_afc_backup = upm->is_disble_afc;
		} else {
			pr_info("No changes, no need adjust vbus\n");
		}
		pr_err("%s: is_disble_afc=%d\n", __func__, upm->is_disble_afc);
		break;
	default:
		pr_err("Unsupported upm6918 IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0){
		pr_err("Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, rc);
		return rc;
	}

	return IIO_VAL_INT;
}

static int upm6918_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct upm6918 *upm = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = upm->iio_chan;
	int i = 0;

	if(!upm){
		pr_err("upm is NULL,Fail !\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(upm6918_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0]){
			return i;
		}
	return -EINVAL;

}

static const struct iio_info upm6918_iio_info = {
	.read_raw	= upm6918_iio_read_raw,
	.write_raw	= upm6918_iio_write_raw,
	.of_xlate	= upm6918_iio_of_xlate,
};

static int upm6918_init_iio_psy(struct upm6918 *upm)
{
	struct iio_dev *indio_dev = upm->indio_dev;
	struct iio_chan_spec *chan = NULL;
	int num_iio_channels = ARRAY_SIZE(upm6918_iio_psy_channels);
	int rc = 0, i = 0;

	pr_info("start !\n");

	if(!upm){
		pr_err("upm is NULL,Fail !\n");
		return -EINVAL;
	}

	upm->iio_chan = devm_kcalloc(upm->dev, num_iio_channels,
						sizeof(*upm->iio_chan), GFP_KERNEL);
	if (!upm->iio_chan) {
		pr_err("upm->iio_chan is null!\n");
		return -ENOMEM;
	}

	upm->int_iio_chans = devm_kcalloc(upm->dev,
				num_iio_channels,
				sizeof(*upm->int_iio_chans),
				GFP_KERNEL);
	if (!upm->int_iio_chans) {
		pr_err("upm->int_iio_chans is null!\n");
		return -ENOMEM;
	}

	indio_dev->info = &upm6918_iio_info;
	indio_dev->dev.parent = upm->dev;
	indio_dev->dev.of_node = upm->dev->of_node;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = upm->iio_chan;
	indio_dev->num_channels = num_iio_channels;
	indio_dev->name = "main_chg";

	for (i = 0; i < num_iio_channels; i++) {
		upm->int_iio_chans[i].indio_dev = indio_dev;
		chan = &upm->iio_chan[i];
		upm->int_iio_chans[i].channel = chan;
		chan->address = i;
		chan->channel = upm6918_iio_psy_channels[i].channel_num;
		chan->type = upm6918_iio_psy_channels[i].type;
		chan->datasheet_name =
			upm6918_iio_psy_channels[i].datasheet_name;
		chan->extend_name =
			upm6918_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate =
			upm6918_iio_psy_channels[i].info_mask;
	}

	rc = devm_iio_device_register(upm->dev, indio_dev);
	if (rc)
		pr_err("Failed to register upm6918 IIO device, rc=%d\n", rc);

	pr_info("upm6918 IIO device, rc=%d\n", rc);
	return rc;
}

static int upm6918_ext_init_iio_psy(struct upm6918 *upm)
{
	if (!upm){
		pr_err("upm6918_ext_init_iio_psy, upm is NULL!\n");
		return -ENOMEM;
	}

	upm->ds_ext_iio_chans = devm_kcalloc(upm->dev,
				ARRAY_SIZE(ds_ext_iio_chan_name),
				sizeof(*upm->ds_ext_iio_chans),
				GFP_KERNEL);
	if (!upm->ds_ext_iio_chans) {
		pr_err("upm->ds_ext_iio_chans is NULL!\n");
		return -ENOMEM;
	}

	upm->fg_ext_iio_chans = devm_kcalloc(upm->dev,
		ARRAY_SIZE(fg_ext_iio_chan_name), sizeof(*upm->fg_ext_iio_chans), GFP_KERNEL);
	if (!upm->fg_ext_iio_chans) {
		pr_err("upm->fg_ext_iio_chans is NULL!\n");
		return -ENOMEM;
	}

	upm->nopmi_chg_ext_iio_chans = devm_kcalloc(upm->dev,
		ARRAY_SIZE(nopmi_chg_ext_iio_chan_name), sizeof(*upm->nopmi_chg_ext_iio_chans), GFP_KERNEL);
	if (!upm->nopmi_chg_ext_iio_chans) {
		pr_err("upm->nopmi_chg_ext_iio_chans is NULL!\n");
		return -ENOMEM;
	}
	return 0;
}
/* longcheer nielianjie10 2023.4.26 add iio end */

/* longcheer nielianjie10 2023.05.17 add vote start*/
static int fcc_vote_callback(struct votable *votable, void *data,
			int fcc_ua, const char *client)
{
	struct upm6918 *upm	= data;
	int rc = 0;
	if (fcc_ua < 0) {
		pr_err("fcc_ua: %d < 0, ERROR!\n", fcc_ua);
		return 0;
	}

	if (fcc_ua > UPM6918_MAX_FCC)
		fcc_ua = UPM6918_MAX_FCC;

	rc = upm6918_set_chargecurrent(upm, fcc_ua);
	if (rc < 0) {
		pr_err("failed to set charge current\n");
		return rc;
	}

	pr_err("ch_log  fcc[%d]\n",fcc_ua);

	return 0;
}

static int chg_dis_vote_callback(struct votable *votable, void *data,
			int disable, const char *client)
{
	struct upm6918 *upm = data;
	int rc = 0;

	if (disable) {
		rc = upm6918_disable_charger(upm);
	} else {
		rc = upm6918_enable_charger(upm);
	}

	if (rc < 0) {
		pr_err("failed to disabl:e%d\n", disable);
		return rc;
	}

	pr_info("disable:%d\n", disable);

	return 0;
}

static int fv_vote_callback(struct votable *votable, void *data,
			int fv_mv, const char *client)
{
	struct upm6918 *upm = data;
	struct power_supply *cp = NULL;
	int pd_active = 0;
	enum power_supply_type chg_type = 0;
	union power_supply_propval prop = {0, };
	int rc = 0, ret = 0;
	u8 val;

	if(fv_mv != 4200) //high temperature
		fv_mv = sm5602_CV;
	else
		pr_err("high temperature,fv = %d\n",fv_mv);
  
	pr_err("fv_vote_callback start");
	if (fv_mv < 0) {
		pr_err("fv_mv: %d < 0, ERROR!\n", fv_mv);
		return 0;
	}
	pd_active = upm6918_get_pd_active(upm);
	cp = power_supply_get_by_name("charger_standalone");
	if (cp)
	{
		ret = power_supply_get_property(cp, POWER_SUPPLY_PROP_STATUS, &prop);
		if (!ret && prop.intval == POWER_SUPPLY_STATUS_CHARGING)
			chg_type = POWER_SUPPLY_TYPE_USB_PD;
	}

	if(fv_mv == FLOAT_VOLTAGE_DEFAULT_UPM6918)
	{
		if ((pd_active == 1) || (chg_type == POWER_SUPPLY_TYPE_USB_PD) || (upm->charge_afc == 1))
		{
			upm6918_update_bits(upm, UPM6918_REG_04, REG04_VREG_MASK,
				   FLOAT_VOLTAGE_DEFAULT_UPM6918_PD_AFC_VAL << REG04_VREG_SHIFT);
			pr_err("is pd or afc pd_active = %d,charge_afc = %d\n", upm->pd_active, upm->charge_afc);
		} else {
			upm6918_update_bits(upm, UPM6918_REG_04, REG04_VREG_MASK,
				   FLOAT_VOLTAGE_DEFAULT_UPM6918_VAL << REG04_VREG_SHIFT);
			pr_err("not is pd or afc pd_active = %d,charge_afc = %d\n", upm->pd_active, upm->charge_afc);
		}
	} else if(fv_mv == FLOAT_VOLTAGE_STEP1_UPM6918) {
		upm6918_update_bits(upm, UPM6918_REG_04, REG04_VREG_MASK,
				   FLOAT_VOLTAGE_STEP1_UPM6918_VAL << REG04_VREG_SHIFT);
	} else if(fv_mv == FLOAT_VOLTAGE_STEP2_UPM6918) {
		upm6918_update_bits(upm, UPM6918_REG_04, REG04_VREG_MASK,
				   FLOAT_VOLTAGE_STEP2_UPM6918_VAL << REG04_VREG_SHIFT);
	} else if(fv_mv == FLOAT_VOLTAGE_STEP3_UPM6918) {
		upm6918_update_bits(upm, UPM6918_REG_04, REG04_VREG_MASK,
				   FLOAT_VOLTAGE_STEP3_UPM6918_VAL << REG04_VREG_SHIFT);
	} else if(fv_mv == FLOAT_VOLTAGE_STEP4_UPM6918) {
		upm6918_update_bits(upm, UPM6918_REG_04, REG04_VREG_MASK,
				   FLOAT_VOLTAGE_STEP4_UPM6918_VAL << REG04_VREG_SHIFT);
	} else if(fv_mv == 4200)//high temperature
	{
		rc = upm6918_set_chargevolt(upm, fv_mv);
		if (rc < 0) {
			pr_err("failed to set chargevoltage\n");
			return rc;
		}
	}
	rc = upm6918_read_byte(upm, UPM6918_REG_04, &val);
    if (rc) {
            pr_err("read UPM6918_REG_04 failed, ret:%d\n", rc);
            return rc;
    }
	pr_err("fv:%d,reg = %x\n", fv_mv, val);
	return 0;
}

static int usb_icl_vote_callback(struct votable *votable, void *data,
			int icl_ma, const char *client)
{
	struct upm6918 *upm = data;
	int rc;

	if (icl_ma < 0){
		pr_err("icl_ma: %d < 0, ERROR!\n", icl_ma);
		return 0;
	}

	if (icl_ma > UPM6918_MAX_ICL)
		icl_ma = UPM6918_MAX_ICL;

	rc = upm6918_set_input_current_limit(upm, icl_ma);
	if (rc < 0) {
		pr_err("failed to set input current limit\n");
		return rc;
	}
	pr_err("icl_ma[%d]\n",icl_ma);

	return 0;
}
/* longcheer nielianjie10 2023.05.17 add vote end*/

static int upm6918_charger_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{
	struct upm6918 *upm = NULL;
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;
	struct iio_dev *indio_dev = NULL;
	int ret = 0;
	int vbus_stat = 0;
	u8 reg_val = 0;

	pr_err("upm6918_charger_probe start!\n");
	if (!client->dev.of_node) {
		pr_err(" client->dev.of_node is null!\n");
		return -ENODEV;
	}

	if (client->dev.of_node) {
		indio_dev = devm_iio_device_alloc(&client->dev, sizeof(struct upm6918));
		if (!indio_dev) {
			pr_err("Failed to allocate memory\n");
			return -ENOMEM;
		}
	}

	upm = iio_priv(indio_dev);
	upm->indio_dev = indio_dev;
	upm->dev = &client->dev;
	upm->client = client;
	upm->pd_active = 0;
	upm->entry_shipmode = false;
	upm->pr_flag = false;
	upm->is_disble_afc = false;
	upm->is_disble_afc_backup = false;
    	upm->unknown_type_recheck = false;
    	upm->unknown_type_recheck_cnt = 0;
	mutex_init(&upm->i2c_rw_lock);
	mutex_init(&upm->regulator_lock);
	i2c_set_clientdata(client, upm);
	//dev_set_name(upm->dev, "bbc_chg");
	ret = upm6918_detect_device(upm);
	if (ret) {
		pr_err("No upm6918 device found!\n");
		ret = -ENODEV;
		goto err_free;
	}
	if (!upm->tcpc) {
		upm->tcpc = tcpc_dev_get_by_name("type_c_port0");
		if (!upm->tcpc) {
			pr_err("get tcpc dev fail\n");
			return -EPROBE_DEFER;
		}
	} else {
		if(!upm->tcpc->tcpc_pd_probe) {
			pr_err("wait tcpc dev probe\n");
			return -EPROBE_DEFER;
		}
	}

	match = of_match_node(upm6918_charger_match_table, node);
	if (match == NULL) {
		pr_err("device tree match not found\n");
		ret = -EINVAL;
		goto err_free;
	}

	if (upm->part_no != *(int *)match->data) {
		pr_err("part no mismatch, hw:%s, devicetree:%s\n",
			pn_str[upm->part_no], pn_str[*(int *) match->data]);
		ret = -EINVAL;
		goto err_free;
	}

	upm->platform_data = upm6918_parse_dt(node, upm);
	if (!upm->platform_data) {
		pr_err("No platform data provided.\n");
		ret = -EINVAL;
		goto err_free;
	}

	upm->hvdcp_qc20_ws = wakeup_source_register(upm->dev, "upm6918_hvdcp_qc20_ws");
	INIT_DELAYED_WORK(&upm->hvdcp_qc20_dwork, upm6918_hvdcp_qc20_dwork);
	INIT_DELAYED_WORK(&upm->hvdcp_dwork, upm6918_hvdcp_dwork);
	INIT_DELAYED_WORK(&upm->prob_dwork, upm6918_inform_prob_dwork_handler);
	INIT_DELAYED_WORK(&upm->tcpc_dwork, upm6918_register_tcpc_notify_dwork_handler);
	INIT_DELAYED_WORK(&upm->disable_afc_dwork, upm6918_disable_afc_dwork);
	INIT_DELAYED_WORK(&upm->disable_pdo_dwork, upm6918_disable_pdo_dwork);
	upm6918_extcon_init(upm);

	ret = upm6918_init_device(upm);
	if (ret) {
		pr_err("Failed to init device\n");
		goto err_free;
	}

	upm6918_dump_regs(upm);
	upm6918_register_interrupt(upm);

	/* longcheer nielianjie10 2023.05.17 add vote start*/
	upm->fcc_votable = create_votable("FCC", VOTE_MIN,
					fcc_vote_callback, upm);
	if (IS_ERR(upm->fcc_votable)) {
		pr_err("fcc_votable is ERROR,goto destroy!\n");
		ret = PTR_ERR(upm->fcc_votable);
		upm->fcc_votable = NULL;
		goto err_free;
	}
	pr_info("fcc_votable create successful !\n");

	upm->chg_dis_votable = create_votable("CHG_DISABLE", VOTE_SET_ANY,
					chg_dis_vote_callback, upm);
	if (IS_ERR(upm->chg_dis_votable)) {
		pr_err("chg_dis_votable is ERROR,goto destroy!\n");
		ret = PTR_ERR(upm->chg_dis_votable);
		upm->chg_dis_votable = NULL;
		goto destroy_votable;
	}
	pr_info("chg_dis_votable create successful !\n");

	upm->fv_votable = create_votable("FV", VOTE_MIN,
					fv_vote_callback, upm);
	if (IS_ERR(upm->fv_votable)) {
		pr_err("fv_votable is ERROR,goto destroy!\n");
		ret = PTR_ERR(upm->fv_votable);
		upm->fv_votable = NULL;
		goto destroy_votable;
	}
	pr_info("fv_votable create successful !\n");

	upm->usb_icl_votable = create_votable("USB_ICL", VOTE_MIN,
					usb_icl_vote_callback,
					upm);
	if (IS_ERR(upm->usb_icl_votable)) {
		pr_err("usb_icl_votable is ERROR,goto destroy!\n");
		ret = PTR_ERR(upm->usb_icl_votable);
		upm->usb_icl_votable = NULL;
		goto destroy_votable;
	}
	pr_info("usb_icl_votable create successful !\n");

	//vote(upm->fcc_votable, MAIN_SET_VOTER, true, 500); /*liwei mark temporarily*/
	//vote(upm->fcc_votable, PROFILE_CHG_VOTER, true, CHG_FCC_CURR_MAX); /*liwei mark temporarily*/
	vote(upm->usb_icl_votable, PROFILE_CHG_VOTER, true, CHG_ICL_CURR_MAX);
	vote(upm->chg_dis_votable, BMS_FC_VOTER, false, 0);
	/* longcheer nielianjie10 2023.05.17 add vote end*/

	ret = sysfs_create_group(&upm->dev->kobj, &upm6918_attr_group);
	if (ret)
		pr_err("failed to register sysfs. err: %d\n", ret);

	ret = upm6918_ext_init_iio_psy(upm);
	if (ret < 0) {
		pr_err("Failed to initialize upm6918_ext_init_iio_psy IIO PSY, rc=%d\n", ret);
	}

	ret = upm6918_charger_psy_register(upm);
    if (ret)
        pr_err("failed to register chg psy. err: %d\n", ret);

	mod_delayed_work(system_wq, &upm->prob_dwork,
					msecs_to_jiffies(2*1000));
	schedule_delayed_work(&upm->tcpc_dwork, msecs_to_jiffies(2*1000));

	ret = upm6918_init_iio_psy(upm);
 	if (ret < 0) {
		pr_err("Failed to initialize upm6918 IIO PSY, ret=%d\n", ret);
		goto destroy_votable;
	}

	vbus_stat = (reg_val & REG08_VBUS_STAT_MASK);
	vbus_stat >>= REG08_VBUS_STAT_SHIFT;
	if(vbus_stat == REG08_VBUS_TYPE_OTG){
		//pr_err("%s:OTG is always online, retry for dpdm !\n", __func__);
		upm6918_request_dpdm(upm,true);
	}

	pr_err("upm6918 probe successfully, Part Num:%d, Revision:%d\n!",
	       upm->part_no, upm->revision);

	return 0;

destroy_votable:
	pr_err("destory votable !\n");
	destroy_votable(upm->fcc_votable);
	destroy_votable(upm->chg_dis_votable);
	destroy_votable(upm->fv_votable);
	destroy_votable(upm->usb_icl_votable);

err_free:
	mutex_destroy(&upm->i2c_rw_lock);
	mutex_destroy(&upm->regulator_lock);
	pr_err("upm6918 probe fail !\n");
	//devm_kfree(&pdev->dev,upm6918);
	return ret;
}

static int upm6918_charger_remove(struct i2c_client *client)
{
	struct upm6918 *upm = i2c_get_clientdata(client);

	mutex_destroy(&upm->i2c_rw_lock);
	mutex_destroy(&upm->regulator_lock);
	sysfs_remove_group(&upm->dev->kobj, &upm6918_attr_group);

	return 0;
}

static void upm6918_charger_shutdown(struct i2c_client *client)
{
	struct upm6918 *upm = i2c_get_clientdata(client);
	int ret;

	if(!upm) {
		pr_err("i2c_get_clientdata error\n");
		return;
	}

	pr_err("entry_shipmode = %d\n",upm->entry_shipmode);
	if(upm->entry_shipmode) {
		ret = upm6918_enter_hiz_mode(upm);
                if (ret) {
                        pr_err("enable hiz failed, upm6918 error\n");
                        return;
                }
		ret = upm6918_disable_batfet(upm);
		if (ret) {
			pr_err("enable ship mode failed, upm6918 error\n");
			return;
		} else {
			ret = sp2130_set_adc(false);
			if (!ret) {
				ret = sm5602_set_shutdown(1);
				if (!ret)
					pr_err("enable ship mode succeeded, tsy battery will disconnet after 10s\n");
				else {
					pr_err("enable ship mode failed, sm5602 error\n");
					return;
				}
			} else {
				pr_err("enable ship mode failed, sp2130 error\n");
				return;
			}
		}
	}


}

static struct i2c_driver upm6918_charger_driver = {
	.driver = {
		.name = "upm6918-charger",
		.owner = THIS_MODULE,
		.of_match_table = upm6918_charger_match_table,
	},
	.probe = upm6918_charger_probe,
	.remove = upm6918_charger_remove,
	.shutdown = upm6918_charger_shutdown,
};

module_i2c_driver(upm6918_charger_driver);
MODULE_DESCRIPTION("Unisemipower UPM6918D Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Unisemepower");

