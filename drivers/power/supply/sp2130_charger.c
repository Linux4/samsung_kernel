/*
 * SP2130 battery charging driver
 *
 * Copyright (C) 2017 Texas Instruments *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define pr_fmt(fmt)	"[sp2130] %s: " fmt, __func__

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
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <asm/neon.h>
#include <linux/hardware_info.h>

#include "sp2130_charger.h"
struct sp2130 *otg_bq;

/* ADC channel*/
enum sp2130_adc_channel{
	ADC_IBUS,
	ADC_VBUS,
	ADC_VAC1,
	ADC_VAC2,
	ADC_VOUT,
	ADC_VBAT,
	ADC_IBAT,
	ADC_TSBUS,
	ADC_TSBAT,
	ADC_TDIE,
	ADC_MAX_NUM,
};

static const u8 sp2130_adc_reg[ADC_MAX_NUM] = {
	0x17,   //IBUS
	0x19,   //VBUS
	0x1B,   //VAC1
	0x1D,   //VAC2
	0x1F,   //VOUT
	0x21,   //VBAT
	0x23,   //IBAT
	0x25,   //TSBUS
	0x27,   //TSBAT
	0X29,   //TDIE
};

#define NOT_SUPPORT	-1

#define SP2130_ROLE_STDALONE   0
#define SP2130_ROLE_SLAVE	1
#define SP2130_ROLE_MASTER	2

#define SP2130_MAX_SHOW_REG_ADDR 0x36

#define SP2130_DEVICE_ID   0x90

enum {
	SP2130_STDALONE,
	SP2130_SLAVE,
	SP2130_MASTER,
};

enum {
	VBUS_ERROR_NONE,
	VBUS_ERROR_LOW,
	VBUS_ERROR_HIGHT,
};

static int sp2130_mode_data[] = {
	[SP2130_STDALONE] = SP2130_STDALONE,
	[SP2130_MASTER] = SP2130_ROLE_MASTER,
	[SP2130_SLAVE] = SP2130_ROLE_SLAVE,
};

#define	BAT_OVP_ALARM		BIT(7)
#define BAT_OCP_ALARM		BIT(6)
#define	BUS_OVP_ALARM		BIT(5)
#define	BUS_OCP_ALARM		BIT(4)
#define	BAT_UCP_ALARM		BIT(3)
//#define	VBUS_INSERT		BIT(2)
#define VBAT_INSERT		    BIT(1)
#define	ADC_DONE		    BIT(0)

#define BAT_OVP_FAULT		BIT(7)
#define BAT_OCP_FAULT		BIT(6)
#define BUS_OVP_FAULT		BIT(5)
#define BUS_OCP_FAULT		BIT(4)
//#define TBUS_TBAT_ALARM		BIT(3)
#define BUS_RCP_FAULT		BIT(3)
//#define TS_BAT_FAULT		BIT(2)
#define TS_ALARM		    BIT(2)
//#define	TS_BUS_FAULT		BIT(1)
#define	TS_FAULT		    BIT(1)
//#define	TS_DIE_FAULT		BIT(0)
#define	TDIE_ALARM		    BIT(0)

#define	VBUS_ERROR_LO		BIT(2)
#define	VBUS_ERROR_HI		BIT(3)

#define bq_err(fmt, ...)								\
do {											\
	if (bq->mode == SP2130_ROLE_MASTER)						\
		printk(KERN_ERR "[sp2130-MASTER]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else if (bq->mode == SP2130_ROLE_SLAVE)					\
		printk(KERN_ERR "[sp2130-SLAVE]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else										\
		printk(KERN_ERR "[sp2130-STANDALONE]:%s:" fmt, __func__, ##__VA_ARGS__);\
} while (0);

#define bq_info(fmt, ...)								\
do {											\
	if (bq->mode == SP2130_ROLE_MASTER)						\
		printk(KERN_INFO "[sp2130-MASTER]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else if (bq->mode == SP2130_ROLE_SLAVE)					\
		printk(KERN_INFO "[sp2130-SLAVE]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else										\
		printk(KERN_INFO "[sp2130-STANDALONE]:%s:" fmt, __func__, ##__VA_ARGS__);\
} while (0);

#define bq_dbg(fmt, ...)								\
do {											\
	if (bq->mode == SP2130_ROLE_MASTER)						\
		printk(KERN_DEBUG "[sp2130-MASTER]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else if (bq->mode == SP2130_ROLE_SLAVE)					\
		printk(KERN_DEBUG "[sp2130-SLAVE]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else										\
		printk(KERN_DEBUG "[sp2130-STANDALONE]:%s:" fmt, __func__, ##__VA_ARGS__);\
} while (0);

#undef bq_info
#define bq_info  bq_err
#undef bq_debug
#define bq_debug  bq_err

struct sp2130_cfg {
	bool bat_ovp_disable;
	bool bat_ocp_disable;
	bool bat_ovp_alm_disable;
	bool bat_ocp_alm_disable;

	int bat_ovp_th;
	int bat_ovp_alm_th;
	int bat_ocp_th;
	int bat_ocp_alm_th;

	bool bus_ovp_alm_disable;
	bool bus_ocp_disable;
	bool bus_ocp_alm_disable;

	int bus_ovp_th;
	int bus_ovp_alm_th;
	int bus_ocp_th;
	int bus_ocp_alm_th;

	bool bat_ucp_alm_disable;

	int bat_ucp_alm_th;
	int ac_ovp_th;

	bool bat_therm_disable;
	bool bus_therm_disable;
	bool die_therm_disable;

	int bat_therm_th; /*in %*/
	int bus_therm_th; /*in %*/
	int die_therm_th; /*in degC*/

	int sense_r_mohm;
};

struct sp2130 {
	struct device *dev;
	struct i2c_client *client;

	int part_no;
	int revision;

	int chip_vendor;

	int mode;

	struct mutex data_lock;
	struct mutex i2c_rw_lock;
	struct mutex charging_disable_lock;
	struct mutex irq_complete;

	bool irq_waiting;
	bool irq_disabled;
	bool resume_completed;

	bool batt_present;
	bool vbus_present;

	bool usb_present;
	bool charge_enabled;	/* Register bit status */

	/* ADC reading */
	int vbat_volt;
	int vbus_volt;
	int vout_volt;
	int vac_volt;

	int ibat_curr;
	int ibus_curr;

	int bat_temp;
	int bus_temp;
	int die_temp;

	/* alarm/fault status */
	bool bat_ovp_fault;
	bool bat_ocp_fault;
	bool bus_ovp_fault;
	bool bus_ocp_fault;

	bool bat_ovp_alarm;
	bool bat_ocp_alarm;
	bool bus_ovp_alarm;
	bool bus_ocp_alarm;

	bool bat_ucp_alarm;

	bool tdie_alarm;

	bool bus_rcp_fault;
	bool ts_alarm;
	bool ts_fault;
	bool die_therm_alarm;

	bool therm_shutdown_flag;
	bool therm_shutdown_stat;

	//bool vbat_reg;
	//bool ibat_reg;

	int  prev_alarm;
	int  prev_fault;

	int chg_ma;
	int chg_mv;

	int charge_state;

	struct sp2130_cfg *cfg;

	int skip_writes;
	int skip_reads;

	struct sp2130_platform_data *platform_data;

	struct delayed_work monitor_work;

	struct dentry *debug_root;

	struct power_supply_desc psy_desc;
	struct power_supply_config psy_cfg;
	struct power_supply *fc2_psy;
	//air add
	struct power_supply *usb_psy;

	int hv_charge_enable;
	int bypass_mode_enable;
};


/************************************************************************/
static int __sp2130_read_byte(struct sp2130 *bq, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(bq->client, reg);
	if (ret < 0) {
		bq_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int __sp2130_write_byte(struct sp2130 *bq, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(bq->client, reg, val);
	if (ret < 0) {
		bq_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
		       val, reg, ret);
		return ret;
	}
	return 0;
}

static int __sp2130_read_word(struct sp2130 *bq, u8 reg, u16 *data)
{
	s32 ret;

	ret = i2c_smbus_read_word_data(bq->client, reg);
	if (ret < 0) {
		bq_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
    }

	*data = (u16) ret;

	return 0;
}

static int sp2130_read_byte(struct sp2130 *bq, u8 reg, u8 *data)
{
	int ret;

	if (bq->skip_reads) {
		*data = 0;
		return 0;
	}

	mutex_lock(&bq->i2c_rw_lock);
	ret = __sp2130_read_byte(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

/*
 * block read function
 */
static int sp2130_i2c_read_block(struct sp2130 *bq, u8 reg, u32 len, u8 *data)
{
	int ret;

    mutex_lock(&bq->i2c_rw_lock);
	ret = i2c_smbus_read_i2c_block_data(bq->client, reg, len, data);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

static int sp2130_write_byte(struct sp2130 *bq, u8 reg, u8 data)
{
	int ret;

	if (bq->skip_writes)
		return 0;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __sp2130_write_byte(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

static int sp2130_read_word(struct sp2130 *bq, u8 reg, u16 *data)
{
	int ret;

	if (bq->skip_reads) {
		*data = 0;
		return 0;
	}

	mutex_lock(&bq->i2c_rw_lock);
	ret = __sp2130_read_word(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

static int sp2130_update_bits(struct sp2130 *bq, u8 reg,
				    u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	if (bq->skip_reads || bq->skip_writes)
		return 0;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __sp2130_read_byte(bq, reg, &tmp);
	if (ret) {
		bq_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __sp2130_write_byte(bq, reg, tmp);
	if (ret)
		bq_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&bq->i2c_rw_lock);
	return ret;
}

/*********************************************************************/

static int sp2130_enable_adc(struct sp2130 *bq, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_ADC_ENABLE;
	else
		val = SP2130_ADC_DISABLE;

	val <<= SP2130_ADC_EN_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_15,
				SP2130_ADC_EN_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_enable_adc);

/*
 *check adc whether enable or disable
 */
static bool sp2130_check_adc_enabled(struct sp2130 *bq)
{
	int ret;
	u8 val;
	bool enabled = false;

	ret = sp2130_read_byte(bq, SP2130_REG_15, &val);
	if (!ret)
		enabled = !!(val & SP2130_ADC_EN_MASK);
	return enabled;
}

static int sp2130_enable_charge(struct sp2130 *bq, bool enable)
{
	int ret;
	u8 val;

	#if 0
	if (enable && (sp2130_check_adc_enabled(bq) == true))
	{
		sp2130_enable_adc(bq, false);
		sp2130_enable_adc(bq, true);
	}
    #endif
	if (enable)
		val = SP2130_CHG_ENABLE;
	else
		val = SP2130_CHG_DISABLE;

	val <<= SP2130_CHG_EN_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_0E,
				SP2130_CHG_EN_MASK, val);

	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_enable_charge);

static int sp2130_check_charge_enabled(struct sp2130 *bq, bool *enabled)
{
	int ret;
	u8 val;

	ret = sp2130_read_byte(bq, SP2130_REG_0E, &val);
	if (!ret)
		*enabled = !!(val & SP2130_CHG_EN_MASK);
	return ret;
}

static int sp2130_enable_wdt(struct sp2130 *bq, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_WATCHDOG_ENABLE;
	else
		val = SP2130_WATCHDOG_DISABLE;

	val <<= SP2130_WATCHDOG_DIS_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_0D,
				SP2130_WATCHDOG_DIS_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_enable_wdt);

static int sp2130_set_wdt(struct sp2130 *bq, int ms)
{
	int ret;
	u8 val;

	if (ms == 500)
		val = SP2130_WATCHDOG_0P5S;
	else if (ms == 1000)
		val = SP2130_WATCHDOG_1S;
	else if (ms == 5000)
		val = SP2130_WATCHDOG_5S;
	else if (ms == 30000)
		val = SP2130_WATCHDOG_30S;
	else
		val = SP2130_WATCHDOG_0P5S;

	val <<= SP2130_WATCHDOG_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_0D,
				SP2130_WATCHDOG_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_set_wdt);

static int sp2130_enable_batovp(struct sp2130 *bq, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_BAT_OVP_ENABLE;
	else
		val = SP2130_BAT_OVP_DISABLE;

	val <<= SP2130_BAT_OVP_DIS_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_00,
				SP2130_BAT_OVP_DIS_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_enable_batovp);

static int sp2130_set_batovp_th(struct sp2130 *bq, int threshold)
{
	int ret, ovp_base;
	u8 val;

	threshold = threshold * 10;

	ovp_base = SP2130_BAT_OVP_BASE * 10;

	if (threshold < ovp_base)
		threshold = ovp_base;

	val = (threshold - ovp_base) / SP2130_BAT_OVP_LSB;

	val <<= SP2130_BAT_OVP_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_00,
				SP2130_BAT_OVP_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_set_batovp_th);

static int sp2130_enable_batovp_alarm(struct sp2130 *bq, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_BAT_OVP_ALM_ENABLE;
	else
		val = SP2130_BAT_OVP_ALM_DISABLE;

	val <<= SP2130_BAT_OVP_ALM_DIS_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_01,
				SP2130_BAT_OVP_ALM_DIS_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_enable_batovp_alarm);

static int sp2130_set_batovp_alarm_th(struct sp2130 *bq, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SP2130_BAT_OVP_ALM_BASE)
		threshold = SP2130_BAT_OVP_ALM_BASE;

	val = (threshold - SP2130_BAT_OVP_ALM_BASE) / SP2130_BAT_OVP_ALM_LSB;

	val <<= SP2130_BAT_OVP_ALM_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_01,
				SP2130_BAT_OVP_ALM_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_set_batovp_alarm_th);

static int sp2130_enable_batocp(struct sp2130 *bq, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_BAT_OCP_ENABLE;
	else
		val = SP2130_BAT_OCP_DISABLE;

	val <<= SP2130_BAT_OCP_DIS_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_02,
				SP2130_BAT_OCP_DIS_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_enable_batocp);

static int sp2130_set_batocp_th(struct sp2130 *bq, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SP2130_BAT_OCP_BASE)
		threshold = SP2130_BAT_OCP_BASE;

	val = (threshold - SP2130_BAT_OCP_BASE) / SP2130_BAT_OCP_LSB;

	val <<= SP2130_BAT_OCP_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_02,
				SP2130_BAT_OCP_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_set_batocp_th);

static int sp2130_enable_batocp_alarm(struct sp2130 *bq, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_BAT_OCP_ALM_ENABLE;
	else
		val = SP2130_BAT_OCP_ALM_DISABLE;

	val <<= SP2130_BAT_OCP_ALM_DIS_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_03,
				SP2130_BAT_OCP_ALM_DIS_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_enable_batocp_alarm);

static int sp2130_set_batocp_alarm_th(struct sp2130 *bq, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SP2130_BAT_OCP_ALM_BASE)
		threshold = SP2130_BAT_OCP_ALM_BASE;

	val = (threshold - SP2130_BAT_OCP_ALM_BASE) / SP2130_BAT_OCP_ALM_LSB;

	val <<= SP2130_BAT_OCP_ALM_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_03,
				SP2130_BAT_OCP_ALM_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_set_batocp_alarm_th);


static int sp2130_set_busovp_th(struct sp2130 *bq, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SP2130_BUS_OVP_BASE)
		threshold = SP2130_BUS_OVP_BASE;

	val = (threshold - SP2130_BUS_OVP_BASE) / SP2130_BUS_OVP_LSB;

	val <<= SP2130_BUS_OVP_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_07,
				SP2130_BUS_OVP_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_set_busovp_th);

static int sp2130_enable_busovp_alarm(struct sp2130 *bq, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_BUS_OVP_ALM_ENABLE;
	else
		val = SP2130_BUS_OVP_ALM_DISABLE;

	val <<= SP2130_BUS_OVP_ALM_DIS_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_08,
				SP2130_BUS_OVP_ALM_DIS_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_enable_busovp_alarm);

static int sp2130_set_busovp_alarm_th(struct sp2130 *bq, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SP2130_BUS_OVP_ALM_BASE)
		threshold = SP2130_BUS_OVP_ALM_BASE;

	val = (threshold - SP2130_BUS_OVP_ALM_BASE) / SP2130_BUS_OVP_ALM_LSB;

	val <<= SP2130_BUS_OVP_ALM_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_08,
				SP2130_BUS_OVP_ALM_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_set_busovp_alarm_th);

static int sp2130_enable_busocp(struct sp2130 *bq, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_BUS_OCP_ENABLE;
	else
		val = SP2130_BUS_OCP_DISABLE;

	val <<= SP2130_BUS_OCP_DIS_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_09,
				SP2130_BUS_OCP_DIS_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_enable_busocp);


static int sp2130_set_busocp_th(struct sp2130 *bq, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SP2130_BUS_OCP_BASE)
		threshold = SP2130_BUS_OCP_BASE;

	val = (threshold - SP2130_BUS_OCP_BASE) / SP2130_BUS_OCP_LSB;

	val <<= SP2130_BUS_OCP_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_09,
				SP2130_BUS_OCP_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_set_busocp_th);

static int sp2130_enable_busocp_alarm(struct sp2130 *bq, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_BUS_OCP_ALM_ENABLE;
	else
		val = SP2130_BUS_OCP_ALM_DISABLE;

	val <<= SP2130_BUS_OCP_ALM_DIS_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_0A,
				SP2130_BUS_OCP_ALM_DIS_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_enable_busocp_alarm);

static int sp2130_set_busocp_alarm_th(struct sp2130 *bq, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SP2130_BUS_OCP_ALM_BASE)
		threshold = SP2130_BUS_OCP_ALM_BASE;

	val = (threshold - SP2130_BUS_OCP_ALM_BASE) / SP2130_BUS_OCP_ALM_LSB;

	val <<= SP2130_BUS_OCP_ALM_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_0A,
				SP2130_BUS_OCP_ALM_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_set_busocp_alarm_th);

static int sp2130_enable_batucp_alarm(struct sp2130 *bq, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_BAT_UCP_ALM_ENABLE;
	else
		val = SP2130_BAT_UCP_ALM_DISABLE;

	val <<= SP2130_BAT_UCP_ALM_DIS_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_04,
				SP2130_BAT_UCP_ALM_DIS_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_enable_batucp_alarm);

static int sp2130_set_batucp_alarm_th(struct sp2130 *bq, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SP2130_BAT_UCP_ALM_BASE)
		threshold = SP2130_BAT_UCP_ALM_BASE;

	val = (threshold - SP2130_BAT_UCP_ALM_BASE) / SP2130_BAT_UCP_ALM_LSB;

	val <<= SP2130_BAT_UCP_ALM_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_04,
				SP2130_BAT_UCP_ALM_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_set_batucp_alarm_th);

static int sp2130_set_ac1ovp_th(struct sp2130 *bq, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SP2130_AC1_OVP_BASE)
		threshold = SP2130_AC1_OVP_BASE;

	if (threshold > SP2130_AC1_OVP_6P5V)
		val = 0x07;
	else
		val = (threshold - SP2130_AC1_OVP_BASE) /  SP2130_AC1_OVP_LSB;

	val <<= SP2130_AC1_OVP_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_05,
				SP2130_AC1_OVP_MASK, val);

	return ret;

}
//EXPORT_SYMBOL_GPL(sp2130_set_ac1ovp_th);

static int sp2130_set_ac2ovp_th(struct sp2130 *bq, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SP2130_AC2_OVP_BASE)
		threshold = SP2130_AC2_OVP_BASE;

	if (threshold > SP2130_AC2_OVP_6P5V)
		val = 0x07;
	else
		val = (threshold - SP2130_AC2_OVP_BASE) /  SP2130_AC2_OVP_LSB;

	val <<= SP2130_AC2_OVP_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_06,
				SP2130_AC2_OVP_MASK, val);

	return ret;

}
//EXPORT_SYMBOL_GPL(sp2130_set_ac2ovp_th);

static int sp2130_enable_bat_therm(struct sp2130 *bq, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_TSBAT_ENABLE;
	else
		val = SP2130_TSBAT_DISABLE;

	val <<= SP2130_TSBAT_DIS_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_0E,
				SP2130_TSBAT_DIS_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_enable_bat_therm);

/*
 * the input threshold is the raw value that would write to register directly.
 */
static int sp2130_set_bat_therm_th(struct sp2130 *bq, u8 threshold)
{
	int ret;

	ret = sp2130_write_byte(bq, SP2130_REG_2C, threshold);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_set_bat_therm_th);

static int sp2130_enable_bus_therm(struct sp2130 *bq, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_TSBUS_ENABLE;
	else
		val = SP2130_TSBUS_DISABLE;

	val <<= SP2130_TSBUS_DIS_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_0E,
				SP2130_TSBUS_DIS_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_enable_bus_therm);

/*
 * the input threshold is the raw value that would write to register directly.
 */
static int sp2130_set_bus_therm_th(struct sp2130 *bq, u8 threshold)
{
	int ret;

	ret = sp2130_write_byte(bq, SP2130_REG_2B, threshold);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_set_bus_therm_th);


static int sp2130_enable_die_therm(struct sp2130 *bq, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_TDIE_ENABLE;
	else
		val = SP2130_TDIE_DISABLE;

	val <<= SP2130_TDIE_DIS_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_0E,
				SP2130_TDIE_DIS_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_enable_die_therm);

/*
 * please be noted that the unit here is degC
 */
 /*
static int sp2130_set_die_therm_th(struct sp2130 *bq, u8 threshold)
{
	int ret;
	u8 val;

	// BE careful, LSB is here is 1/LSB, so we use multiply here
	val = (threshold - SP2130_TDIE_ALM_BASE) * SP2130_TDIE_ALM_LSB;
	val <<= SP2130_TDIE_ALM_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_2D,
				SP2130_TDIE_ALM_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_set_die_therm_th);
*/

static int sp2130_set_adc_average(struct sp2130 *bq, bool avg)
{
	int ret;
	u8 val;

	if (avg)
		val = SP2130_ADC_AVG_ENABLE;
	else
		val = SP2130_ADC_AVG_DISABLE;

	val <<= SP2130_ADC_AVG_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_15,
				SP2130_ADC_AVG_MASK, val);
	return 0;
}
//EXPORT_SYMBOL_GPL(sp2130_set_adc_average);

static int sp2130_set_adc_scanrate(struct sp2130 *bq, bool oneshot)
{
	int ret;
	u8 val;

	if (oneshot)
		val = SP2130_ADC_RATE_ONESHOT;
	else
		val = SP2130_ADC_RATE_CONTINOUS;

	val <<= SP2130_ADC_RATE_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_15,
				SP2130_ADC_EN_MASK, val);
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_set_adc_scanrate);

static int sp2130_get_adc_data(struct sp2130 *bq, int channel,  int *result)
{
	int ret;
	u16 temp;
	u8 data[2];

	if (channel > ADC_MAX_NUM)
		return -EINVAL;

	ret = sp2130_i2c_read_block(bq, sp2130_adc_reg[channel], 2, data);
	if (ret < 0)
	{
	    bq_err("i2c read block error, channel:%d  !!!\n", channel);
		goto out;
	}

    temp = (data[0] << 8) + data[1];
	switch (channel) {
	case ADC_IBUS:
	case ADC_VBUS:
	case ADC_VAC1:
	case ADC_VAC2:
	case ADC_VOUT:
	case ADC_VBAT:
	case ADC_IBAT:
        if (channel == ADC_IBAT) {
            if (data[0] & 0x80) {
                *result = 0;
            } else {
                *result = temp;
            }
            bq_info("ADC_IBAT: data[0] %x, data[1] %x\n", data[0], data[1]);
		} else {
            *result = temp;
            bq_info("channel: %d, adc: %d\n", channel, temp);
        }
		break;
	case ADC_TDIE:
		*result = temp >> 1;
		bq_info("TDIE adc: %d.%cdegC (0x%04x)\n", *result, (temp & 0x1 ? '5' : '0'), temp);
		break;
	case ADC_TSBUS:
	case ADC_TSBAT:
		*result = temp * 100 / 1024;
		bq_info("TS adc: %d%% (0x%04x)\n", *result, temp);
		break;
	default:
		bq_err("channel invalid !!!\n");
		break;
	}

out:
	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_get_adc_data);

static int sp2130_set_adc_scan(struct sp2130 *bq, int channel, bool enable)
{
	int ret;
	u8 reg;
	u8 mask;
	u8 val;

	if (channel > ADC_MAX_NUM)
		return -EINVAL;

	switch (channel) {
	case ADC_IBUS:
        reg = 0x15;
	    mask = 0x02;

		break;
	case ADC_VBUS:
		reg = 0x15;
	    mask = 0x01;

		break;
	case ADC_VAC1:
		reg = 0x16;
		mask = 0x80;

		break;
	case ADC_VAC2:
		reg = 0x16;
		mask = 0x40;

		break;
	case ADC_VOUT:
		reg = 0x16;
		mask = 0x20;

		break;
	case ADC_VBAT:
		reg = 0x16;
		mask = 0x10;

		break;
	case ADC_IBAT:
		reg = 0x16;
		mask = 0x08;

		break;
	case ADC_TSBUS:
		reg = 0x16;
		mask = 0x04;

		break;
	case ADC_TSBAT:
		reg = 0x16;
		mask = 0x02;

		break;
	case ADC_TDIE:
		reg = 0x16;
		mask = 0x01;

		break;
	default:
		bq_err("channel invalid !!!\n");
		break;
	}

	if (enable)
		val = 0;
	else
		val = 1;

	ret = sp2130_update_bits(bq, reg, mask, val);

	return ret;
}

static int sp2130_set_alarm_int_mask(struct sp2130 *bq, u8 mask)
{
	int ret;
	u8 val;

	ret = sp2130_read_byte(bq, SP2130_REG_11, &val);
	if (ret)
		return ret;

	val |= mask;

	ret = sp2130_write_byte(bq, SP2130_REG_11, val);

	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_set_alarm_int_mask);

static int sp2130_clear_alarm_int_mask(struct sp2130 *bq, u8 mask)
{
	int ret;
	u8 val;

	ret = sp2130_read_byte(bq, SP2130_REG_11, &val);
	if (ret)
		return ret;

	val &= ~mask;

	ret = sp2130_write_byte(bq, SP2130_REG_11, val);

	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_clear_alarm_int_mask);

static int sp2130_set_fault_int_mask(struct sp2130 *bq, u8 mask)
{
	int ret;
	u8 val;

	ret = sp2130_read_byte(bq, SP2130_REG_14, &val);
	if (ret)
		return ret;

	val |= mask;

	ret = sp2130_write_byte(bq, SP2130_REG_14, val);

	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_set_fault_int_mask);

static int sp2130_clear_fault_int_mask(struct sp2130 *bq, u8 mask)
{
	int ret;
	u8 val;

	ret = sp2130_read_byte(bq, SP2130_REG_14, &val);
	if (ret)
		return ret;

	val &= ~mask;

	ret = sp2130_write_byte(bq, SP2130_REG_14, val);

	return ret;
}
//EXPORT_SYMBOL_GPL(sp2130_clear_fault_int_mask);

static int sp2130_set_sense_resistor(struct sp2130 *bq, int r_mohm)
{
	int ret;
	u8 val;

	if (r_mohm == 2)
		val = SP2130_SET_IBAT_SNS_RES_2MHM;
	else if (r_mohm == 5)
		val = SP2130_SET_IBAT_SNS_RES_5MHM;
	else
		return -EINVAL;

	val <<= SP2130_SET_IBAT_SNS_RES_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_2E,
				SP2130_SET_IBAT_SNS_RES_MASK,
				val);
	return ret;
}

static int sp2130_set_ibus_ucp_thr(struct sp2130 *bq, int ibus_ucp_thr)
{
	int ret;
	u8 val;

	if (ibus_ucp_thr == 300)
		val = SP2130_IBUS_UCP_RISE_300MA;
	else if (ibus_ucp_thr == 500)
		val = SP2130_IBUS_UCP_RISE_500MA;
	else
		return -EINVAL;

	val <<= SP2130_IBUS_UCP_RISE_TH_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_2E,
				SP2130_IBUS_UCP_RISE_TH_MASK,
				val);
	return ret;
}

static int sp2130_set_ss_timeout(struct sp2130 *bq, int timeout)
{
	int ret;
	u8 val;

	switch (timeout) {
	case 0:
		val = SP2130_SS_TIMEOUT_DISABLE;
		break;
	case 12:
		val = SP2130_SS_TIMEOUT_12P5MS;
		break;
	case 25:
		val = SP2130_SS_TIMEOUT_25MS;
		break;
	case 50:
		val = SP2130_SS_TIMEOUT_50MS;
		break;
	case 100:
		val = SP2130_SS_TIMEOUT_100MS;
		break;
	case 400:
		val = SP2130_SS_TIMEOUT_400MS;
		break;
	case 1500:
		val = SP2130_SS_TIMEOUT_1500MS;
		break;
	case 100000:
		val = SP2130_SS_TIMEOUT_100000MS;
		break;
	default:
		val = SP2130_SS_TIMEOUT_100000MS;
		break;
	}

	val <<= SP2130_SS_TIMEOUT_SET_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_2E,
				SP2130_SS_TIMEOUT_SET_MASK,
				val);

	return ret;
}

static int sp2130_get_work_mode(struct sp2130 *bq, int *mode)
{
	int ret;
	u8 val;

	ret = sp2130_read_byte(bq, SP2130_REG_2E, &val);

	if (ret) {
		bq_err("Failed to read operation mode register\n");
		return ret;
	}

	val = (val & SP2130_MS_MASK) >> SP2130_MS_SHIFT;
	if (val == SP2130_MS_MASTER)
		*mode = SP2130_ROLE_MASTER;
	else if (val == SP2130_MS_SLAVE)
		*mode = SP2130_ROLE_SLAVE;
	else
		*mode = SP2130_ROLE_STDALONE;

	bq_info("work mode:%s\n", *mode == SP2130_ROLE_STDALONE ? "Standalone" :
			(*mode == SP2130_ROLE_SLAVE ? "Slave" : "Master"));
	return ret;
}

static int sp2130_detect_device(struct sp2130 *bq)
{
	int ret;
	u8 data;

	ret = sp2130_read_byte(bq, SP2130_REG_31, &data);
	if (ret == 0) {
		bq->part_no = (data & SP2130_DEV_ID_MASK);
		bq->part_no >>= SP2130_DEV_ID_SHIFT;
	}

	return ret;
}

/*
 * enable otg mode
 */
static int sp2130_enable_otg(struct sp2130 *bq, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_OTG_ENABLE;
	else
		val = SP2130_OTG_DISABLE;

	val <<= SP2130_EN_OTG_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_2F,
				SP2130_EN_OTG_MASK, val);

	return ret;
}


/*
 * enable acdrv1
 */
static int sp2130_enable_acdrv1(struct sp2130 *bq, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_ACDRV1_ENABLE;
	else
		val = SP2130_ACDRV1_DISABLE;

	val <<= SP2130_EN_ACDRV1_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_30,
				SP2130_EN_ACDRV1_MASK, val);

	return ret;
}

/*
 * enable acdrv2
 */
static int sp2130_enable_acdrv2(struct sp2130 *bq, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_ACDRV2_ENABLE;
	else
		val = SP2130_ACDRV2_DISABLE;

	val <<= SP2130_EN_ACDRV2_SHIFT;

	ret = sp2130_update_bits(bq, SP2130_REG_30,
				SP2130_EN_ACDRV2_MASK, val);

	return ret;
}


static int sp2130_parse_dt(struct sp2130 *bq, struct device *dev)
{
	int ret;
	struct device_node *np = dev->of_node;

	bq->cfg = devm_kzalloc(dev, sizeof(struct sp2130_cfg),
					GFP_KERNEL);

	if (!bq->cfg)
		return -ENOMEM;

	bq->cfg->bat_ovp_disable = of_property_read_bool(np,
			"sp2130,bat-ovp-disable");
	bq->cfg->bat_ocp_disable = of_property_read_bool(np,
			"sp2130,bat-ocp-disable");
	bq->cfg->bat_ovp_alm_disable = of_property_read_bool(np,
			"sp2130,bat-ovp-alarm-disable");
	bq->cfg->bat_ocp_alm_disable = of_property_read_bool(np,
			"sp2130,bat-ocp-alarm-disable");
	bq->cfg->bus_ocp_disable = of_property_read_bool(np,
			"sp2130,bus-ocp-disable");
	bq->cfg->bus_ovp_alm_disable = of_property_read_bool(np,
			"sp2130,bus-ovp-alarm-disable");
	bq->cfg->bus_ocp_alm_disable = of_property_read_bool(np,
			"sp2130,bus-ocp-alarm-disable");
	bq->cfg->bat_ucp_alm_disable = of_property_read_bool(np,
			"sp2130,bat-ucp-alarm-disable");
	bq->cfg->bat_therm_disable = of_property_read_bool(np,
			"sp2130,bat-therm-disable");
	bq->cfg->bus_therm_disable = of_property_read_bool(np,
			"sp2130,bus-therm-disable");
	bq->cfg->die_therm_disable = of_property_read_bool(np,
			"sp2130,die-therm-disable");

	ret = of_property_read_u32(np, "sp2130,bat-ovp-threshold",
			&bq->cfg->bat_ovp_th);
	if (ret) {
		bq_err("failed to read bat-ovp-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "sp2130,bat-ovp-alarm-threshold",
			&bq->cfg->bat_ovp_alm_th);
	if (ret) {
		bq_err("failed to read bat-ovp-alarm-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "sp2130,bat-ocp-threshold",
			&bq->cfg->bat_ocp_th);
	if (ret) {
		bq_err("failed to read bat-ocp-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "sp2130,bat-ocp-alarm-threshold",
			&bq->cfg->bat_ocp_alm_th);
	if (ret) {
		bq_err("failed to read bat-ocp-alarm-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "sp2130,bus-ovp-threshold",
			&bq->cfg->bus_ovp_th);
	if (ret) {
		bq_err("failed to read bus-ovp-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "sp2130,bus-ovp-alarm-threshold",
			&bq->cfg->bus_ovp_alm_th);
	if (ret) {
		bq_err("failed to read bus-ovp-alarm-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "sp2130,bus-ocp-threshold",
			&bq->cfg->bus_ocp_th);
	if (ret) {
		bq_err("failed to read bus-ocp-threshold\n");
		//return ret;
	}
	ret = of_property_read_u32(np, "sp2130,bus-ocp-alarm-threshold",
			&bq->cfg->bus_ocp_alm_th);
	if (ret) {
		bq_err("failed to read bus-ocp-alarm-threshold\n");
		//return ret;
	}
	/*ret = of_property_read_u32(np, "sp2130,bat-ucp-alarm-threshold",
			&bq->cfg->bat_ucp_alm_th);
	if (ret) {
		bq_err("failed to read bat-ucp-alarm-threshold\n");
		return ret;
	}
	*/
	ret = of_property_read_u32(np, "sp2130,bat-therm-threshold",
			&bq->cfg->bat_therm_th);
	if (ret) {
		bq_err("failed to read bat-therm-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "sp2130,bus-therm-threshold",
			&bq->cfg->bus_therm_th);
	if (ret) {
		bq_err("failed to read bus-therm-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "sp2130,die-therm-threshold",
			&bq->cfg->die_therm_th);
	if (ret) {
		bq_err("failed to read die-therm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sp2130,ac-ovp-threshold",
			&bq->cfg->ac_ovp_th);
	if (ret) {
		bq_err("failed to read ac-ovp-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sp2130,sense-resistor-mohm",
			&bq->cfg->sense_r_mohm);
	if (ret) {
		bq_err("failed to read sense-resistor-mohm\n");
		return ret;
	}

	return 0;
}

static int sp2130_init_protection(struct sp2130 *bq)
{
	int ret;

	ret = sp2130_enable_batovp(bq, !bq->cfg->bat_ovp_disable);
	bq_info("%s bat ovp %s\n",
		bq->cfg->bat_ovp_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sp2130_enable_batocp(bq, !bq->cfg->bat_ocp_disable);
	bq_info("%s bat ocp %s\n",
		bq->cfg->bat_ocp_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sp2130_enable_batovp_alarm(bq, !bq->cfg->bat_ovp_alm_disable);
	bq_info("%s bat ovp alarm %s\n",
		bq->cfg->bat_ovp_alm_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sp2130_enable_batocp_alarm(bq, !bq->cfg->bat_ocp_alm_disable);
	bq_info("%s bat ocp alarm %s\n",
		bq->cfg->bat_ocp_alm_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sp2130_enable_batucp_alarm(bq, !bq->cfg->bat_ucp_alm_disable);
	bq_info("%s bat ocp alarm %s\n",
		bq->cfg->bat_ucp_alm_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sp2130_enable_busovp_alarm(bq, !bq->cfg->bus_ovp_alm_disable);
	bq_info("%s bus ovp alarm %s\n",
		bq->cfg->bus_ovp_alm_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sp2130_enable_busocp(bq, !bq->cfg->bus_ocp_disable);
	bq_info("%s bus ocp %s\n",
		bq->cfg->bus_ocp_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sp2130_enable_busocp_alarm(bq, !bq->cfg->bus_ocp_alm_disable);
	bq_info("%s bus ocp alarm %s\n",
		bq->cfg->bus_ocp_alm_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sp2130_enable_bat_therm(bq, !bq->cfg->bat_therm_disable);
	bq_info("%s bat therm %s\n",
		bq->cfg->bat_therm_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sp2130_enable_bus_therm(bq, !bq->cfg->bus_therm_disable);
	bq_info("%s bus therm %s\n",
		bq->cfg->bus_therm_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sp2130_enable_die_therm(bq, !bq->cfg->die_therm_disable);
	bq_info("%s die therm %s\n",
		bq->cfg->die_therm_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sp2130_set_batovp_th(bq, bq->cfg->bat_ovp_th);
	bq_info("set bat ovp th %d %s\n", bq->cfg->bat_ovp_th,
		!ret ? "successfully" : "failed");

	ret = sp2130_set_batovp_alarm_th(bq, bq->cfg->bat_ovp_alm_th);
	bq_info("set bat ovp alarm threshold %d %s\n", bq->cfg->bat_ovp_alm_th,
		!ret ? "successfully" : "failed");

	ret = sp2130_set_batocp_th(bq, bq->cfg->bat_ocp_th);
	bq_info("set bat ocp threshold %d %s\n", bq->cfg->bat_ocp_th,
		!ret ? "successfully" : "failed");

	ret = sp2130_set_batocp_alarm_th(bq, bq->cfg->bat_ocp_alm_th);
	bq_info("set bat ocp alarm threshold %d %s\n", bq->cfg->bat_ocp_alm_th,
		!ret ? "successfully" : "failed");

	ret = sp2130_set_busovp_th(bq, bq->cfg->bus_ovp_th);
	bq_info("set bus ovp threshold %d %s\n", bq->cfg->bus_ovp_th,
		!ret ? "successfully" : "failed");

	ret = sp2130_set_busovp_alarm_th(bq, bq->cfg->bus_ovp_alm_th);
	bq_info("set bus ovp alarm threshold %d %s\n", bq->cfg->bus_ovp_alm_th,
		!ret ? "successfully" : "failed");

	ret = sp2130_set_busocp_th(bq, bq->cfg->bus_ocp_th);
	bq_info("set bus ocp threshold %d %s\n", bq->cfg->bus_ocp_th,
		!ret ? "successfully" : "failed");

	ret = sp2130_set_busocp_alarm_th(bq, bq->cfg->bus_ocp_alm_th);
	bq_info("set bus ocp alarm th %d %s\n", bq->cfg->bus_ocp_alm_th,
		!ret ? "successfully" : "failed");

	ret = sp2130_set_batucp_alarm_th(bq, bq->cfg->bat_ucp_alm_th);
	bq_info("set bat ucp threshold %d %s\n", bq->cfg->bat_ucp_alm_th,
		!ret ? "successfully" : "failed");

	ret = sp2130_set_bat_therm_th(bq, bq->cfg->bat_therm_th);
	bq_info("set die therm threshold %d %s\n", bq->cfg->bat_therm_th,
		!ret ? "successfully" : "failed");
	ret = sp2130_set_bus_therm_th(bq, bq->cfg->bus_therm_th);
	bq_info("set bus therm threshold %d %s\n", bq->cfg->bus_therm_th,
		!ret ? "successfully" : "failed");
/*
	ret = sp2130_set_die_therm_th(bq, bq->cfg->die_therm_th);
	bq_info("set die therm threshold %d %s\n", bq->cfg->die_therm_th,
		!ret ? "successfully" : "failed");
*/
	ret = sp2130_set_ac1ovp_th(bq, bq->cfg->ac_ovp_th);
	bq_info("set ac ovp threshold %d %s\n", bq->cfg->ac_ovp_th,
		!ret ? "successfully" : "failed");

	return 0;
}

static int sp2130_init_adc(struct sp2130 *bq)
{
	sp2130_set_adc_scanrate(bq, false);
/*
	sp2130_set_adc_average(bq, true);
	sp2130_set_adc_scan(bq, ADC_IBUS, true);
	sp2130_set_adc_scan(bq, ADC_VBUS, true);
	sp2130_set_adc_scan(bq, ADC_VOUT, false);
	sp2130_set_adc_scan(bq, ADC_VBAT, true);
	sp2130_set_adc_scan(bq, ADC_IBAT, true);
	sp2130_set_adc_scan(bq, ADC_TSBUS, false);
	sp2130_set_adc_scan(bq, ADC_TSBAT, false);
	sp2130_set_adc_scan(bq, ADC_TDIE, false);
	sp2130_set_adc_scan(bq, ADC_VAC1, true);
*/
    sp2130_write_byte(bq, SP2130_REG_16, 0x06);
	sp2130_enable_adc(bq, true);

	return 0;
}

static int sp2130_init_int_src(struct sp2130 *bq)
{
	int ret;
	/*TODO:be careful ts bus and ts bat alarm bit mask is in
	 *	fault mask register, so you need call
	 *	sp2130_set_fault_int_mask for tsbus and tsbat alarm
	 */
	ret = sp2130_set_alarm_int_mask(bq, ADC_DONE
					| BAT_OCP_ALARM | BAT_UCP_ALARM
					| BAT_OVP_ALARM);
	if (ret) {
		bq_err("failed to set alarm mask:%d\n", ret);
		return ret;
	}
//#if 0
	ret = sp2130_set_fault_int_mask(bq,
			TS_FAULT | TDIE_ALARM | TS_ALARM | BAT_OCP_FAULT);
	if (ret) {
		bq_err("failed to set fault mask:%d\n", ret);
		return ret;
	}
//#endif
	return ret;
}

static int sp2130_init_device(struct sp2130 *bq)
{
	sp2130_enable_wdt(bq, false);

	sp2130_set_ss_timeout(bq, 100000);
	sp2130_set_ibus_ucp_thr(bq, 300);
	sp2130_set_sense_resistor(bq, bq->cfg->sense_r_mohm);

	sp2130_init_protection(bq);
	sp2130_init_adc(bq);
	sp2130_init_int_src(bq);

    //init regs
    sp2130_write_byte(bq, SP2130_REG_00, 0x42);
    sp2130_write_byte(bq, SP2130_REG_01, 0x3C);
    //sp2130_write_byte(bq, SP2130_REG_02, 0x32);
    sp2130_write_byte(bq, SP2130_REG_02, 0xB2);
    sp2130_write_byte(bq, SP2130_REG_03, 0x28);
    sp2130_write_byte(bq, SP2130_REG_04, 0x80);
    sp2130_write_byte(bq, SP2130_REG_05, 0x02); //set AC_OVP as 11v
    //sp2130_write_byte(bq, SP2130_REG_06, 0x41);
    sp2130_write_byte(bq, SP2130_REG_07, 0x32); //set BUS_OVP as 11v
    sp2130_write_byte(bq, SP2130_REG_08, 0x32); //set BUS_OVP_ALM as 11v
    sp2130_write_byte(bq, SP2130_REG_09, 0x06);
    sp2130_write_byte(bq, SP2130_REG_0A, 0x0A);
    //sp2130_write_byte(bq, SP2130_REG_0B, 0x41);
    //sp2130_write_byte(bq, SP2130_REG_0C, 0x41);
    sp2130_write_byte(bq, SP2130_REG_0D, 0x24);
    sp2130_write_byte(bq, SP2130_REG_0E, 0x07);
    //sp2130_write_byte(bq, SP2130_REG_0F, 0x41);
    //sp2130_write_byte(bq, SP2130_REG_10, 0x41);
    sp2130_write_byte(bq, SP2130_REG_11, 0xCB);
    //sp2130_write_byte(bq, SP2130_REG_12, 0x41);
    //sp2130_write_byte(bq, SP2130_REG_13, 0x41);
    sp2130_write_byte(bq, SP2130_REG_14, 0x07);
    //sp2130_write_byte(bq, SP2130_REG_15, 0x41);
    sp2130_write_byte(bq, SP2130_REG_16, 0x0E);//0x06

    sp2130_write_byte(bq, SP2130_REG_32, 0x90);
/* +S96818AA1-8255, zhouxiaopeng2.wt, MODIFY, 20230629, increase delay to prevent current jitter effets */
	sp2130_write_byte(bq, SP2130_REG_2E, 0xE4);
	sp2130_write_byte(bq, SP2130_REG_33, 0X0B);
/* -S96818AA1-8255, zhouxiaopeng2.wt, MODIFY, 20230629, increase delay to prevent current jitter effets */
	return 0;
}

static int sp2130_set_present(struct sp2130 *bq, bool present)
{
	bq->usb_present = present;

	if (present)
		sp2130_init_device(bq);
	return 0;
}

static ssize_t sp2130_show_registers(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sp2130 *bq = dev_get_drvdata(dev);
	u8 addr, max_addr;
	u8 val;
	u8 tmpbuf[300];
	int len;
	int idx = 0;
	int ret;
	
	max_addr = SP2130_MAX_SHOW_REG_ADDR;
	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sp2130");

	for (addr = 0x0; addr <= max_addr; addr++) {
		ret = sp2130_read_byte(bq, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
					"Reg[%.2X] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t sp2130_store_register(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sp2130 *bq = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg <= SP2130_MAX_SHOW_REG_ADDR)
		sp2130_write_byte(bq, (unsigned char)reg, (unsigned char)val);

	return count;
}

static DEVICE_ATTR(registers, 0660, sp2130_show_registers, sp2130_store_register);

static struct attribute *sp2130_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group sp2130_attr_group = {
	.attrs = sp2130_attributes,
};

static enum power_supply_property sp2130_charger_props[] = {
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_CHARGING_ENABLED,
    POWER_SUPPLY_PROP_UPM_BUS_VOLTAGE,
    POWER_SUPPLY_PROP_UPM_BUS_CURRENT,
    POWER_SUPPLY_PROP_UPM_BAT_VOLTAGE,
    POWER_SUPPLY_PROP_UPM_BAT_CURRENT,
    POWER_SUPPLY_PROP_UPM_DIE_TEMP,
    POWER_SUPPLY_PROP_UPM_VBUS_STATUS,
    POWER_SUPPLY_PROP_MODEL_NAME,
};

static void sp2130_check_alarm_status(struct sp2130 *bq);
static void sp2130_check_fault_status(struct sp2130 *bq);

#define VBUS_INSERT     BIT(7)
static int sp2130_charger_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct sp2130 *bq = power_supply_get_drvdata(psy);
	int result;
	int ret;
	u8 reg_val;

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		sp2130_check_charge_enabled(bq, &bq->charge_enabled);
		val->intval = bq->charge_enabled;
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = bq->usb_present;
		break;

	case POWER_SUPPLY_PROP_UPM_BAT_VOLTAGE:
		ret = sp2130_get_adc_data(bq, ADC_VBAT, &result);
		//if (!ret)
			bq->vbat_volt = result;

		val->intval = bq->vbat_volt;
		break;
	case POWER_SUPPLY_PROP_UPM_BAT_CURRENT:
		ret = sp2130_get_adc_data(bq, ADC_IBAT, &result);
		//if (!ret)
			bq->ibat_curr = result;

		val->intval = bq->ibat_curr;
        bq_err("POWER_SUPPLY_PROP_UPM_BAT_CURRENT: %d\n", result);
		break;

	case POWER_SUPPLY_PROP_UPM_BUS_VOLTAGE:
		ret = sp2130_get_adc_data(bq, ADC_VBUS, &result);
		//if (!ret)
			bq->vbus_volt = result;

		val->intval = bq->vbus_volt;
		break;
	case POWER_SUPPLY_PROP_UPM_BUS_CURRENT:
		ret = sp2130_get_adc_data(bq, ADC_IBUS, &result);
		//if (!ret)
			bq->ibus_curr = result;

		val->intval = bq->ibus_curr;
        bq_err("POWER_SUPPLY_PROP_UPM_BUS_CURRENT: %d\n", result);
		break;

	case POWER_SUPPLY_PROP_UPM_DIE_TEMP:
		ret = sp2130_get_adc_data(bq, ADC_TDIE, &result);
		//if (!ret)
			bq->die_temp = result;

		val->intval = bq->die_temp;
		break;

    case POWER_SUPPLY_PROP_UPM_VBUS_STATUS:
		ret = sp2130_read_byte(bq, SP2130_REG_35, &reg_val);
		if (!ret)
			val->intval = (int)reg_val;
		break;

	case POWER_SUPPLY_PROP_MODEL_NAME:
		ret = sp2130_get_work_mode(bq, &bq->mode);
		if (ret) {
			val->strval = "unknown";
		} else {
			if (bq->mode == SP2130_ROLE_MASTER)
				val->strval = "sp2130-master";
			else if (bq->mode == SP2130_ROLE_SLAVE)
				val->strval = "sp2130-slave";
			else
				val->strval = "sp2130-standalone";
		}
		break;

	default:
		return -EINVAL;

	}

	return 0;
}

//static void sp2130_dump_reg(struct sp2130 *bq);
static int sp2130_charger_set_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       const union power_supply_propval *val)
{
	struct sp2130 *bq = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		sp2130_enable_charge(bq, val->intval);
		sp2130_check_charge_enabled(bq, &bq->charge_enabled);
		bq_info("POWER_SUPPLY_PROP_CHARGING_ENABLED: %s\n",
				val->intval ? "enable" : "disable");
        //sp2130_dump_reg(bq);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		sp2130_set_present(bq, !!val->intval);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int sp2130_charger_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static struct sp2130 *g_bq;

void sp2130_set_psy(void)
{
	power_supply_changed(g_bq->usb_psy);
}

static int sp2130_psy_register(struct sp2130 *bq)
{
	bq->psy_cfg.drv_data = bq;
	bq->psy_cfg.of_node = bq->dev->of_node;

	if (bq->mode == SP2130_ROLE_MASTER)
		bq->psy_desc.name = "sp2130-master";
	else if (bq->mode == SP2130_ROLE_SLAVE)
		bq->psy_desc.name = "sp2130-slave";
	else
		bq->psy_desc.name = "cp-standalone";

	bq->psy_desc.type = POWER_SUPPLY_TYPE_MAINS;
	bq->psy_desc.properties = sp2130_charger_props;
	bq->psy_desc.num_properties = ARRAY_SIZE(sp2130_charger_props);
	bq->psy_desc.get_property = sp2130_charger_get_property;
	bq->psy_desc.set_property = sp2130_charger_set_property;
	bq->psy_desc.property_is_writeable = sp2130_charger_is_writeable;


	bq->fc2_psy = devm_power_supply_register(bq->dev,
			&bq->psy_desc, &bq->psy_cfg);
	if (IS_ERR(bq->fc2_psy)) {
		bq_err("failed to register fc2_psy:%d\n",
				PTR_ERR(bq->fc2_psy));
		return PTR_ERR(bq->fc2_psy);
	}

	bq_info("%s power supply register successfully\n", bq->psy_desc.name);
	g_bq = bq;

	return 0;
}

static void sp2130_dump_reg(struct sp2130 *bq)
{
	int ret;
	u8 val;
	u8 addr;

	for (addr = 0x00; addr <= SP2130_MAX_SHOW_REG_ADDR; addr++) {
		ret = sp2130_read_byte(bq, addr, &val);
		if (!ret)
			bq_err("Reg[%02X] = 0x%02X\n", addr, val);
	}
}
//EXPORT_SYMBOL_GPL(sp2130_dump_reg);

static void sp2130_check_alarm_status(struct sp2130 *bq)
{
	int ret;
	u8 flag = 0;
	u8 stat = 0;

	mutex_lock(&bq->data_lock);

	ret = sp2130_read_byte(bq, SP2130_REG_0F, &stat);
	if (!ret && stat != bq->prev_alarm) {
		bq_info("INT_STAT = 0X%02x\n", stat);
		bq->prev_alarm = stat;
		bq->bat_ovp_alarm = !!(stat & BAT_OVP_ALARM);
		bq->bat_ocp_alarm = !!(stat & BAT_OCP_ALARM);
		bq->bus_ovp_alarm = !!(stat & BUS_OVP_ALARM);
		bq->bus_ocp_alarm = !!(stat & BUS_OCP_ALARM);
		bq->batt_present  = !!(stat & VBAT_INSERT);
		//bq->vbus_present  = !!(stat & VBUS_INSERT);
		bq->bat_ucp_alarm = !!(stat & BAT_UCP_ALARM);
	}

	mutex_unlock(&bq->data_lock);
}

static void sp2130_check_fault_status(struct sp2130 *bq)
{
	int ret;
	u8 stat = 0;
	bool changed = false;

	mutex_lock(&bq->data_lock);

	ret = sp2130_read_byte(bq, SP2130_REG_12, &stat);
	if (!ret && stat)
		bq_err("FAULT_STAT = 0x%02X\n", stat);

	if (!ret && stat != bq->prev_fault) {
		changed = true;
		bq->prev_fault = stat;
		bq->bat_ovp_fault = !!(stat & BAT_OVP_FAULT);
		bq->bat_ocp_fault = !!(stat & BAT_OCP_FAULT);
		bq->bus_ovp_fault = !!(stat & BUS_OVP_FAULT);
		bq->bus_ocp_fault = !!(stat & BUS_OCP_FAULT);
		bq->bus_rcp_fault = !!(stat & BUS_RCP_FAULT);
		bq->ts_alarm = !!(stat & TS_ALARM);

		bq->ts_fault = !!(stat & TS_FAULT);
		bq->tdie_alarm = !!(stat & TDIE_ALARM);
	}

	mutex_unlock(&bq->data_lock);
}

/*
 * check stat and flag regs for IC status
 */
static void sp2130_check_status_flags(struct sp2130 *bq)
{
    int ret;
	u8 stat = 0;
	u8 temp = 0;
	u8 sf_reg[22] = {0};

	bq_info("---------%s---------\n", __func__);

	ret = sp2130_i2c_read_block(bq, SP2130_REG_05, 2, &sf_reg[0]);
	if (1)
	{
	    if (sf_reg[0] & SP2130_AC1_OVP_STAT_MASK)
			bq_info("REG_05:%x, AC1_OVP_STAT\n", sf_reg[0]);

		if (sf_reg[0] & SP2130_AC1_OVP_FLAG_MASK)
			bq_info("REG_05:%x, AC1_OVP_FLAG\n", sf_reg[0]);

		if (sf_reg[1] & SP2130_AC2_OVP_STAT_MASK)
			bq_info("REG_06:%x, AC2_OVP_STAT\n", sf_reg[1]);

		if (sf_reg[1] & SP2130_AC2_OVP_FLAG_MASK)
			bq_info("REG_06:%x, AC2_OVP_FLAG\n", sf_reg[1]);
	}

	ret = sp2130_i2c_read_block(bq, SP2130_REG_09, 11, &sf_reg[2]);

	if (1)
	{
	    if (sf_reg[2] & SP2130_IBUS_UCP_RISE_FLAG_MASK)
			bq_info("REG_09:%x, IBUS_UCP_RISE_FLAG\n", sf_reg[2]);

		if (sf_reg[3] & SP2130_IBUS_UCP_FALL_FLAG_MASK)
			bq_info("REG_0A:%x, IBUS_UCP_FALL_FLAG\n", sf_reg[3]);

		if (sf_reg[4] & SP2130_VOUT_OVP_STAT_MASK)
			bq_info("REG_0B:%x, VOUT_OVP_STAT\n", sf_reg[4]);
		if (sf_reg[4] & SP2130_VOUT_OVP_FLAG_MASK)
			bq_info("REG_0B:%x, VOUT_OVP_FLAG\n", sf_reg[4]);

		if (sf_reg[5] & SP2130_TSD_FLAG_MASK)
			bq_info("REG_0C:%x, TSD_FLAG\n", sf_reg[5]);
		if (sf_reg[5] & SP2130_TSD_STAT_MASK)
			bq_info("REG_0C:%x, TSD_STAT\n", sf_reg[5]);
		if (sf_reg[5] & SP2130_VBUS_ERRLO_FLAG_MASK)
			bq_info("REG_0C:%x, VBUS_ERRLO_FLAG\n", sf_reg[5]);
		if (sf_reg[5] & SP2130_VBUS_ERRHI_FLAG_MASK)
			bq_info("REG_0C:%x, VBUS_ERRHI_FLAG\n", sf_reg[5]);
        if (sf_reg[5] & SP2130_SS_TIMEOUT_FLAG_MASK)
			bq_info("REG_0C:%x, SS_TIMEOUT_FLAG\n", sf_reg[5]);
		if (sf_reg[5] & SP2130_CONV_ACTIVE_STAT_MASK)
			bq_info("REG_0C:%x, CONV_ACTIVE_STAT\n", sf_reg[5]);
		if (sf_reg[5] & SP2130_PIN_DIAG_FAIL_FLAG_MASK)
			bq_info("REG_0C:%x, PIN_DIAG_FAIL_FLAG\n", sf_reg[5]);

		if (sf_reg[6] & SP2130_WD_TIMEOUT_FLAG_MASK)
			bq_info("REG_0D:%x, WD_TIMEOUT_FLAG\n", sf_reg[6]);

		if (sf_reg[8] & SP2130_BAT_OVP_ALM_STAT_MASK)
			bq_info("REG_0F:%x, BAT_OVP_ALM_STAT\n", sf_reg[8]);
		if (sf_reg[8] & SP2130_BAT_OCP_ALM_STAT_MASK)
			bq_info("REG_0F:%x, BAT_OCP_ALM_STAT\n", sf_reg[8]);
		if (sf_reg[8] & SP2130_BUS_OVP_ALM_STAT_MASK)
			bq_info("REG_0F:%x, BUS_OVP_ALM_STAT\n", sf_reg[8]);
		if (sf_reg[8] & SP2130_BUS_OCP_ALM_STAT_MASK)
			bq_info("REG_0F:%x, BUS_OCP_ALM_STAT\n", sf_reg[8]);
		if (sf_reg[8] & SP2130_BAT_UCP_ALM_STAT_MASK)
			bq_info("REG_0F:%x, BAT_UCP_ALM_STAT\n", sf_reg[8]);
		if (sf_reg[8] & SP2130_VBAT_INSERT_STAT_MASK)
			bq_info("REG_0F:%x, VBAT_INSERT_STAT\n", sf_reg[8]);
		if (sf_reg[8] & SP2130_ADC_DONE_STAT_MASK)
			bq_info("REG_0F:%x, ADC_DONE_STAT\n", sf_reg[8]);

		if (sf_reg[9] & SP2130_BAT_OVP_ALM_FLAG_MASK)
			bq_info("REG_10:%x, BAT_OVP_ALM_FLAG\n", sf_reg[9]);
		if (sf_reg[9] & SP2130_BAT_OCP_ALM_FLAG_MASK)
			bq_info("REG_10:%x, BAT_OCP_ALM_FLAG\n", sf_reg[9]);
		if (sf_reg[9] & SP2130_BUS_OVP_ALM_FLAG_MASK)
			bq_info("REG_10:%x, BUS_OVP_ALM_FLAG\n", sf_reg[9]);
		if (sf_reg[9] & SP2130_BUS_OCP_ALM_FLAG_MASK)
			bq_info("REG_10:%x, BUS_OCP_ALM_FLAG\n", sf_reg[9]);
		if (sf_reg[9] & SP2130_BAT_UCP_ALM_FLAG_MASK)
			bq_info("REG_10:%x, BAT_UCP_ALM_FLAG\n", sf_reg[9]);
		if (sf_reg[9] & SP2130_VBAT_INSERT_FLAG_MASK)
			bq_info("REG_10:%x, VBAT_INSERT_FLAG\n", sf_reg[9]);
		if (sf_reg[9] & SP2130_ADC_DONE_FLAG_MASK)
			bq_info("REG_10:%x, ADC_DONE_FLAG\n", sf_reg[9]);

		if (sf_reg[11] & SP2130_BAT_OVP_FLT_STAT_MASK)
			bq_info("REG_12:%x, BAT_OVP_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & SP2130_BAT_OCP_FLT_STAT_MASK)
			bq_info("REG_12:%x, BAT_OCP_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & SP2130_BUS_OVP_FLT_STAT_MASK)
			bq_info("REG_12:%x, BUS_OVP_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & SP2130_BUS_OCP_FLT_STAT_MASK)
			bq_info("REG_12:%x, BUS_OCP_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & SP2130_BUS_RCP_FLT_STAT_MASK)
			bq_info("REG_12:%x, BUS_RCP_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & SP2130_TS_ALM_STAT_MASK)
			bq_info("REG_12:%x, TS_ALM_STAT\n", sf_reg[11]);
		if (sf_reg[11] & SP2130_TS_FLT_STAT_MASK)
			bq_info("REG_12:%x, TS_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & SP2130_TDIE_ALM_STAT_MASK)
			bq_info("REG_12:%x, TDIE_ALM_STAT\n", sf_reg[11]);

		if (sf_reg[12] & SP2130_BAT_OVP_FLT_FLAG_MASK)
			bq_info("REG_13:%x, BAT_OVP_FLT_FLAG\n", sf_reg[12]);
		if (sf_reg[12] & SP2130_BAT_OCP_FLT_FLAG_MASK)
			bq_info("REG_13:%x, BAT_OCP_FLT_FLAG\n", sf_reg[12]);
		if (sf_reg[12] & SP2130_BUS_OVP_FLT_FLAG_MASK)
			bq_info("REG_13:%x, BUS_OVP_FLT_FLAG\n", sf_reg[12]);
		if (sf_reg[12] & SP2130_BUS_OCP_FLT_FLAG_MASK)
			bq_info("REG_13:%x, BUS_OCP_FLT_FLAG\n", sf_reg[12]);
		if (sf_reg[12] & SP2130_BUS_RCP_FLT_FLAG_MASK)
			bq_info("REG_13:%x, BUS_RCP_FLT_FLAG\n", sf_reg[12]);
		if (sf_reg[12] & SP2130_TS_ALM_FLAG_MASK)
			bq_info("REG_13:%x, TS_ALM_FLAG\n", sf_reg[12]);
		if (sf_reg[12] & SP2130_TS_FLT_FLAG_MASK)
			bq_info("REG_13:%x, TS_FLT_FLAG\n", sf_reg[12]);
		if (sf_reg[12] & SP2130_TDIE_ALM_FLAG_MASK)
			bq_info("REG_13:%x, TDIE_ALM_FLAG\n", sf_reg[12]);
	}

	ret = sp2130_i2c_read_block(bq, SP2130_REG_2E, 10, &sf_reg[13]);
	if (1)
	{
		if (sf_reg[14] & SP2130_VAC1PRESENT_STAT_MASK)
		    bq_info("REG_2F:%x, VAC1PRESENT_STAT\n", sf_reg[14]);
		if (sf_reg[14] & SP2130_VAC1PRESENT_FLAG_MASK)
		    bq_info("REG_2F:%x, VAC1PRESENT_FLAG\n", sf_reg[14]);
		if (sf_reg[14] & SP2130_VAC2PRESENT_STAT_MASK)
		    bq_info("REG_2F:%x, VAC2PRESENT_STAT\n", sf_reg[14]);
		if (sf_reg[14] & SP2130_VAC2PRESENT_FLAG_MASK)
		    bq_info("REG_2F:%x, VAC2PRESENT_FLAG\n", sf_reg[14]);

		if (sf_reg[15] & SP2130_ACRB1_STAT_MASK)
		    bq_info("REG_30:%x, ACRB1_STAT\n", sf_reg[15]);
		if (sf_reg[15] & SP2130_ACRB1_FLAG_MASK)
		    bq_info("REG_30:%x, ACRB1_FLAG\n", sf_reg[15]);
		if (sf_reg[15] & SP2130_ACRB2_STAT_MASK)
		    bq_info("REG_30:%x, ACRB2_STAT\n", sf_reg[15]);
		if (sf_reg[15] & SP2130_ACRB2_FLAG_MASK)
		    bq_info("REG_30:%x, ACRB2_FLAG\n", sf_reg[15]);

        if (sf_reg[17] & SP2130_PMID2VOUT_UVP_FLAG_MASK)
		    bq_info("REG_32:%x, PMID2VOUT_UVP_FLAG\n", sf_reg[17]);

		if (sf_reg[17] & SP2130_PMID2VOUT_OVP_FLAG_MASK)
		    bq_info("REG_32:%x, PMID2VOUT_OVP_FLAG\n", sf_reg[17]);

		if (sf_reg[19] & SP2130_POWER_NG_FLAG_MASK)
		    bq_info("REG_34:%x, POWER_NG_FLAG\n", sf_reg[19]);

		if (sf_reg[21] & SP2130_VBUS_PRESENT_STAT_MASK)
		    bq_info("REG_36:%x, VBUS_PRESENT_STAT\n", sf_reg[21]);
		if (sf_reg[21] & SP2130_VBUS_PRESENT_FLAG_MASK)
		    bq_info("REG_36:%x, VBUS_PRESENT_FLAG\n", sf_reg[21]);
	}
}

/*
 * interrupt does nothing, just info event chagne, other module could get info
 * through power supply interface
 */
static irqreturn_t sp2130_charger_interrupt(int irq, void *dev_id)
{
	struct sp2130 *bq = dev_id;

	bq_info("INT OCCURED\n");

	mutex_lock(&bq->irq_complete);
	/* dump some impoartant registers and alarm fault status for debug */
	//sp2130_dump_important_regs(bq);
	//sp2130_check_alarm_status(bq);
	//sp2130_check_fault_status(bq);
	sp2130_check_status_flags(bq);
	mutex_unlock(&bq->irq_complete);

	/* power_supply_changed(bq->fc2_psy); */

	return IRQ_HANDLED;
}

static void determine_initial_status(struct sp2130 *bq)
{
	if (bq->client->irq)
		sp2130_charger_interrupt(bq->client->irq, bq);
}

static int show_registers(struct seq_file *m, void *data)
{
	struct sp2130 *bq = m->private;
	u8 addr, max_addr;
	int ret;
	u8 val;

	max_addr = SP2130_MAX_SHOW_REG_ADDR;

	for (addr = 0x0; addr <= max_addr; addr++) {
		ret = sp2130_read_byte(bq, addr, &val);
		if (!ret)
			seq_printf(m, "Reg[%02X] = 0x%02X\n", addr, val);
	}
	return 0;
}

static int reg_debugfs_open(struct inode *inode, struct file *file)
{
	struct sp2130 *bq = inode->i_private;

	return single_open(file, show_registers, bq);
}

static const struct file_operations reg_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= reg_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void create_debugfs_entry(struct sp2130 *bq)
{
	if (bq->mode == SP2130_ROLE_MASTER)
		bq->debug_root = debugfs_create_dir("sp2130-master", NULL);
	else if (bq->mode == SP2130_ROLE_SLAVE)
		bq->debug_root = debugfs_create_dir("sp2130-slave", NULL);
	else
		bq->debug_root = debugfs_create_dir("sp2130-standalone", NULL);

	if (!bq->debug_root)
		bq_err("Failed to create debug dir\n");

	if (bq->debug_root) {
		debugfs_create_file("registers",
					S_IFREG | S_IRUGO,
					bq->debug_root, bq, &reg_debugfs_ops);

		debugfs_create_x32("skip_reads",
					S_IFREG | S_IWUSR | S_IRUGO,
					bq->debug_root,
					&(bq->skip_reads));
		debugfs_create_x32("skip_writes",
					S_IFREG | S_IWUSR | S_IRUGO,
					bq->debug_root,
					&(bq->skip_writes));
	}
}

static struct of_device_id sp2130_charger_match_table[] = {
	{
		.compatible = "sp2130-standalone",
		.data = &sp2130_mode_data[SP2130_STDALONE],
	},
	{
		.compatible = "sp2130-master",
		.data = &sp2130_mode_data[SP2130_MASTER],
	},
	{
		.compatible = "sp2130-slave",
		.data = &sp2130_mode_data[SP2130_SLAVE],
	},
	{},
};
//MODULE_DEVICE_TABLE(of, sp2130_charger_match_table);

static int sp2130_irq_init(struct sp2130 *chip)
{
	struct gpio_desc *irq_gpio;
	int irq;
	int ret = 0;
	irq_gpio = devm_gpiod_get(chip->dev, "irq", GPIOD_IN);
	if (IS_ERR(irq_gpio))
		return PTR_ERR(irq_gpio);

	irq = gpiod_to_irq(irq_gpio);
	chip->client->irq = irq;
	if (irq < 0) {
		dev_err(chip->dev, "%s irq mapping fail(%d)\n", __func__, irq);
		return ret;
	}
	dev_info(chip->dev, "%s irq = %d\n", __func__, irq);

	ret = devm_request_threaded_irq(chip->dev, irq, NULL,
					sp2130_charger_interrupt,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"sp2130_irq", chip);
	if (ret) {
		dev_err(chip->dev, "failed to request irq %d\n", irq);
		return ret;
	}

	return ret;
}

/* +S96818AA1-1936,zhouxiaopeng2.wt,add,20230803,Modify OTG current path */
int sp2130_set_otg_txmode(bool enable)
{
	int ret = 0;
	u8 val1,val2;
	if (!otg_bq)
		return 0;
	if (enable) {
		val1 = SP2130_OTG_ENABLE;
		val2 = SP2130_ACDRV1_ENABLE;
		sp2130_update_bits(otg_bq, SP2130_REG_2F,SP2130_EN_OTG_MASK, val1 <<= SP2130_EN_OTG_SHIFT);
		ret = sp2130_update_bits(otg_bq, SP2130_REG_30,SP2130_EN_ACDRV1_MASK, val2 <<= SP2130_EN_ACDRV1_SHIFT);
	} else {
		val1 = SP2130_ACDRV1_DISABLE;
		val2 = SP2130_OTG_DISABLE;
		sp2130_update_bits(otg_bq, SP2130_REG_30,SP2130_EN_ACDRV1_MASK, val1 <<= SP2130_EN_ACDRV1_SHIFT);
		ret = sp2130_update_bits(otg_bq, SP2130_REG_2F,SP2130_EN_OTG_MASK, val2 <<= SP2130_EN_OTG_SHIFT);
	}
	printk("sp2130_set_otg_txmode = %d \n", enable);
	return ret;
}
EXPORT_SYMBOL(sp2130_set_otg_txmode);
/* -S96818AA1-1936,zhouxiaopeng2.wt,add,20230803,Modify OTG current path */

static int sp2130_charger_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct sp2130 *bq;
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;
	int ret;

	bq_info("client->irq=%d", client->irq);
	ret = i2c_smbus_read_byte_data(client, SP2130_REG_31);
	if (ret != SP2130_DEVICE_ID) {
		bq_err("failed to communicate with chip, ret=%d\n", ret);
		return -ENODEV;
	}
	bq_info("bq device id=0x%x\n", ret);

	bq = devm_kzalloc(&client->dev, sizeof(struct sp2130), GFP_KERNEL);
	if (!bq)
		return -ENOMEM;

	bq->dev = &client->dev;

	bq->client = client;
	i2c_set_clientdata(client, bq);

	mutex_init(&bq->i2c_rw_lock);
	mutex_init(&bq->data_lock);
	mutex_init(&bq->charging_disable_lock);
	mutex_init(&bq->irq_complete);

	bq->resume_completed = true;
	bq->irq_waiting = false;
	bq->hv_charge_enable = 1;

	ret = sp2130_detect_device(bq);
	if (ret) {
		bq_err("No sp2130 device found!\n");
		return -ENODEV;
	}

	bq->bypass_mode_enable = 0;

	match = of_match_node(sp2130_charger_match_table, node);
	if (match == NULL) {
		bq_err("device tree match not found!\n");
		return -ENODEV;
	}

	sp2130_get_work_mode(bq, &bq->mode);

	if (bq->mode !=  *(int *)match->data) {
		bq_err("device operation mode mismatch with dts configuration\n");
		return -EINVAL;
	}

	ret = sp2130_parse_dt(bq, &client->dev);
	if (ret)
		return -EIO;

	ret = sp2130_init_device(bq);
	if (ret) {
		bq_err("Failed to init device\n");
		return ret;
	}
	ret = sp2130_irq_init(bq);
	if (ret){
		dev_err(&client->dev, "sp2130_irq_init failed.\n");
		return ret;
	}
	determine_initial_status(bq);
    sp2130_dump_reg(bq);

	ret = sp2130_psy_register(bq);
	if (ret)
		return ret;

	device_init_wakeup(bq->dev, 1);
	create_debugfs_entry(bq);

	ret = sysfs_create_group(&bq->dev->kobj, &sp2130_attr_group);
	if (ret) {
		bq_err("failed to register sysfs. err: %d\n", ret);
		goto err_1;
	}

	/* determine_initial_status(bq); */
	bq->usb_psy = power_supply_get_by_name("usb");
	if(!bq->usb_psy){
		bq_info("usb psy not found\n");
	}

	bq_info("sp2130 probe successfully, Part Num:%d, Chip Vendor:%d\n!",
				bq->part_no, bq->chip_vendor);
	/* +Req S96818AA1-5169,zhouxiaopeng2.wt,20230519, mode information increased */
	hardwareinfo_set_prop(HARDWARE_CHARGER_PUMP, "SP2130");
	/* -Req S96818AA1-5169,zhouxiaopeng2.wt,20230519, mode information increased */

	otg_bq = bq;
	return 0;

err_1:
	power_supply_unregister(bq->fc2_psy);
	return ret;
}

static inline bool is_device_suspended(struct sp2130 *bq)
{
	return !bq->resume_completed;
}

static int sp2130_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sp2130 *bq = i2c_get_clientdata(client);

	mutex_lock(&bq->irq_complete);
	bq->resume_completed = false;
	mutex_unlock(&bq->irq_complete);
	sp2130_enable_adc(bq, false);
	bq_err("Suspend successfully!");

	return 0;
}

static int sp2130_suspend_noirq(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sp2130 *bq = i2c_get_clientdata(client);

	if (bq->irq_waiting) {
		pr_err_ratelimited("Aborting suspend, an interrupt was detected while suspending\n");
		return -EBUSY;
	}
	return 0;
}

static int sp2130_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sp2130 *bq = i2c_get_clientdata(client);

	mutex_lock(&bq->irq_complete);
	bq->resume_completed = true;
	if (bq->irq_waiting) {
		bq->irq_disabled = false;
		enable_irq(client->irq);
		mutex_unlock(&bq->irq_complete);
		//sp2130_charger_interrupt(client->irq, bq);
	} else {
		mutex_unlock(&bq->irq_complete);
	}

	sp2130_enable_adc(bq, true);

	power_supply_changed(bq->fc2_psy);
	bq_err("Resume successfully!");

	return 0;
}

static int sp2130_charger_remove(struct i2c_client *client)
{
	struct sp2130 *bq = i2c_get_clientdata(client);

	sp2130_enable_adc(bq, false);

	power_supply_unregister(bq->fc2_psy);

	mutex_destroy(&bq->charging_disable_lock);
	mutex_destroy(&bq->data_lock);
	mutex_destroy(&bq->i2c_rw_lock);
	mutex_destroy(&bq->irq_complete);

	debugfs_remove_recursive(bq->debug_root);

	sysfs_remove_group(&bq->dev->kobj, &sp2130_attr_group);

	return 0;
}

static void sp2130_charger_shutdown(struct i2c_client *client)
{
	struct sp2130 *bq = i2c_get_clientdata(client);

	sp2130_enable_adc(bq, false);
}

static const struct dev_pm_ops sp2130_pm_ops = {
	.resume		= sp2130_resume,
	.suspend_noirq = sp2130_suspend_noirq,
	.suspend	= sp2130_suspend,
};

static const struct i2c_device_id sp2130_charger_id[] = {
	{"sp2130-standalone", SP2130_ROLE_STDALONE},
	{"sp2130-master", SP2130_ROLE_MASTER},
	{"sp2130-slave", SP2130_ROLE_SLAVE},
	{},
};

static struct i2c_driver sp2130_charger_driver = {
	.driver		= {
		.name	= "sp2130_charger",
		.owner	= THIS_MODULE,
		.of_match_table = sp2130_charger_match_table,
		.pm	= &sp2130_pm_ops,
	},
	.id_table	= sp2130_charger_id,

	.probe		= sp2130_charger_probe,
	.remove		= sp2130_charger_remove,
	.shutdown	= sp2130_charger_shutdown,
};

module_i2c_driver(sp2130_charger_driver);

MODULE_DESCRIPTION("SP2130 Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Nuvolta");
