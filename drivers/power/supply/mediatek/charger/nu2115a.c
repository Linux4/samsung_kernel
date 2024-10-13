/*
 * Copyright (C) 2023 Nuvolta Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
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
#include <linux/version.h>
#include "nu2115a.h"
#include "nu2115a_reg.h"
/*A06 code for AL7160A-445 by jiashixian at 20240610 start*/
#include <linux/chg-tcpc_info.h>
/*A06 code for AL7160A-445 by jiashixian at 20240610 end*/
static bool s_is_nu2115a_probe_success = false;

/************************************************************************/
static int __nu2115a_read_byte(struct nu2115a *chip, u8 reg, u8 *data)
{
    s32 ret;

    ret = i2c_smbus_read_byte_data(chip->client, reg);
    if (ret < 0) {
        nu_err("i2c read fail: can't read from reg 0x%02X\n", reg);
        return ret;
    }

    *data = (u8) ret;

    return 0;
}

static int __nu2115a_write_byte(struct nu2115a *chip, int reg, u8 val)
{
    s32 ret;

    ret = i2c_smbus_write_byte_data(chip->client, reg, val);
    if (ret < 0) {
        nu_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
                val, reg, ret);
        return ret;
    }
    return 0;
}

static int nu2115a_read_byte(struct nu2115a *chip, u8 reg, u8 *data)
{
    int ret;

    if (chip->skip_reads) {
        *data = 0;
        return 0;
    }

    mutex_lock(&chip->i2c_rw_lock);
    ret = __nu2115a_read_byte(chip, reg, data);
    mutex_unlock(&chip->i2c_rw_lock);

    return ret;
}

static int nu2115a_write_byte(struct nu2115a *chip, u8 reg, u8 data)
{
    int ret;

    if (chip->skip_writes)
        return 0;

    mutex_lock(&chip->i2c_rw_lock);
    ret = __nu2115a_write_byte(chip, reg, data);
    mutex_unlock(&chip->i2c_rw_lock);

    return ret;
}

static int nu2115a_update_bits(struct nu2115a *chip, u8 reg,
        u8 mask, u8 data)
{
    int ret;
    u8 tmp;

    if (chip->skip_reads || chip->skip_writes)
        return 0;

    mutex_lock(&chip->i2c_rw_lock);
    ret = __nu2115a_read_byte(chip, reg, &tmp);
    if (ret) {
        nu_err("Failed: reg=%02X, ret=%d\n", reg, ret);
        goto out;
    }

    tmp &= ~mask;
    tmp |= data & mask;

    ret = __nu2115a_write_byte(chip, reg, tmp);
    if (ret)
        nu_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
    mutex_unlock(&chip->i2c_rw_lock);
    return ret;
}

/*********************************************************************/
/*static int nu2115a_set_errhi_status_mask(struct nu2115a *chip)
{
    int ret = 0;
    u8 val;

    val = NU2115A_VBUS_ERRHI_MASK_ENABLE;

    val <<= NU2115A_VBUS_ERRHI_MASK_SHIFT;

    nu_info("nu2115a_set_errhi_status_mask, val = %d\n", val);

    ret = nu2115a_update_bits(chip, NU2115A_REG_35, NU2115A_VBUS_ERRHI_MASK_MASK, val);
    return ret;
}

static int nu2115a_set_errlo_status_mask(struct nu2115a *chip)
{
    int ret = 0;
    u8 val;

    val = NU2115A_VBUS_ERRLO_MASK_ENABLE;

    val <<= NU2115A_VBUS_ERRLO_MASK_SHIFT;

    nu_info("nu2115a_set_errlo_status_mask, val = %d\n", val);

    ret = nu2115a_update_bits(chip, NU2115A_REG_35, NU2115A_VBUS_ERRLO_MASK_MASK, val);
    return ret;
}*/

static int nu2115a_disable_cp_ts_detect(struct nu2115a *chip)
{
    int ret;

    nu_err("nu2115a_disable_cp_ts_detect\n");
    ret = nu2115a_write_byte(chip, NU2115A_REG_0E,
            0x06);

    return ret;
}

void nu2115a_dump(struct nu2115a *chip)
{
    u8 addr;
    u8 val;
    int ret;
    for (addr = 0x0; addr <= 0x36; addr++) {
        ret = nu2115a_read_byte(chip, addr, &val);
        if (ret == 0) {
            nu_err("nu2115a reg[%02X]=%02X \n", addr, val);
        }
    }
}

static int nu2115a_init_device(struct nu2115a *chip);
static int _nu2115a_enable_charge(struct nu2115a *chip, bool en)
{
    int ret;
    u8 val;

    ret = nu2115a_read_byte(chip, NU2115A_REG_0E, &val);
    nu_err("nu2115a reg[0E]=%02X\n", val);
    if (!(val & BIT(1)) || !(val & BIT(2))) {
        nu2115a_init_device(chip);
    }
    if (en) {
        /*A06 code for AL7160A-1532 by jiashixian at 20240527 start*/
        ret = nu2115a_write_byte(chip, NU2115A_REG_32, 0x70);
        /*A06 code for AL7160A-1532 by jiashixian at 20240527 end*/
        val = NU2115A_CHG_ENABLE;
    } else {
        val = NU2115A_CHG_DISABLE;
    }

    val <<= NU2115A_CHG_EN_SHIFT;

    nu_err("nu2115a charger %s\n", en == false ? "disable" : "enable");
    ret = nu2115a_update_bits(chip, NU2115A_REG_0E,
            NU2115A_CHG_EN_MASK, val);

    /*A06 code for AL7160A-1532 by jiashixian at 20240527 start*/
    if (en) {
        msleep(30);
        ret = nu2115a_write_byte(chip, NU2115A_REG_32, 0x50);
    }
    /*A06 code for AL7160A-1532 by jiashixian at 20240527 end*/
    nu2115a_dump(chip);
    return ret;
}


static int nu2115a_check_charge_enabled(struct nu2115a *chip, bool *en)
{
    int ret;
    u8 val;

    ret = nu2115a_read_byte(chip, NU2115A_REG_0E, &val);
    nu_info(">>>reg [0x0E] = 0x%02x\n", val);
    if (!ret)
        *en = !!(val & NU2115A_CHG_EN_MASK);

    return ret;
}

static int nu2115a_enable_wdt(struct nu2115a *chip, bool enable)
{
    int ret;
    u8 val;

    if (enable)
        val = NU2115A_WATCHDOG_ENABLE;
    else
        val = NU2115A_WATCHDOG_DISABLE;

    val <<= NU2115A_WATCHDOG_DIS_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_0D,
            NU2115A_WATCHDOG_DIS_MASK, val);

    return ret;
}

static int nu2115a_set_reg_reset(struct nu2115a *chip)
{
    int ret;
    u8 val = 1;

    val = NU2115A_REG_RST_ENABLE;

    val <<= NU2115A_REG_RST_DISABLE;

    ret = nu2115a_update_bits(chip, NU2115A_REG_0D,
            NU2115A_REG_RST_MASK, val);
    return ret;
}

static int nu2115a_enable_batovp(struct nu2115a *chip, bool enable)
{
    int ret;
    u8 val;

    if (enable)
        val = NU2115A_BAT_OVP_DISABLE;
    else
        val = NU2115A_BAT_OVP_ENABLE;

    val <<= NU2115A_BAT_OVP_DIS_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_00,
            NU2115A_BAT_OVP_DIS_MASK, val);
    return ret;
}

static int nu2115a_set_batovp_th(struct nu2115a *chip, int threshold)
{
    int ret;
    u8 val;

    if (threshold < NU2115A_BAT_OVP_BASE)
        threshold = NU2115A_BAT_OVP_BASE;

    val = (threshold - NU2115A_BAT_OVP_BASE) *10/ NU2115A_BAT_OVP_LSB;

    val <<= NU2115A_BAT_OVP_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_00,
            NU2115A_BAT_OVP_MASK, val);
    return ret;
}
/*
static int nu2115a_enable_batovp_alarm(struct nu2115a *chip, bool enable)
{
    int ret;
    u8 val;

    if (enable)
        val = NU2115A_BAT_OVP_ALM_ENABLE;
    else
        val = NU2115A_BAT_OVP_ALM_DISABLE;

    val <<= NU2115A_BAT_OVP_ALM_DIS_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_01,
            NU2115A_BAT_OVP_ALM_DIS_MASK, val);
    return ret;
}*/

static int nu2115a_set_batovp_alarm_th(struct nu2115a *chip, int threshold)
{
    int ret;
    u8 val;

    if (threshold < NU2115A_BAT_OVP_ALM_BASE)
        threshold = NU2115A_BAT_OVP_ALM_BASE;

    val = (threshold - NU2115A_BAT_OVP_ALM_BASE) / NU2115A_BAT_OVP_ALM_LSB;

    val <<= NU2115A_BAT_OVP_ALM_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_01,
            NU2115A_BAT_OVP_ALM_MASK, val);
    return ret;
}

static int nu2115a_enable_batocp(struct nu2115a *chip, bool enable)
{
    int ret;
    u8 val;

    if (enable)
        val = NU2115A_BAT_OCP_DISABLE;
    else
        val = NU2115A_BAT_OCP_ENABLE;

    val <<= NU2115A_BAT_OCP_DIS_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_02,
            NU2115A_BAT_OCP_DIS_MASK, val);
    return ret;
}

static int nu2115a_set_batocp_th(struct nu2115a *chip, int threshold)
{
    int ret;
    u8 val;

    if (threshold < NU2115A_BAT_OCP_BASE)
        threshold = NU2115A_BAT_OCP_BASE;

    val = (threshold - NU2115A_BAT_OCP_BASE) / NU2115A_BAT_OCP_LSB;

    val <<= NU2115A_BAT_OCP_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_02,
            NU2115A_BAT_OCP_MASK, val);
    return ret;
}
/*
static int nu2115a_enable_batocp_alarm(struct nu2115a *chip, bool enable)
{
    int ret;
    u8 val;

    if (enable)
        val = NU2115A_BAT_OCP_ALM_ENABLE;
    else
        val = NU2115A_BAT_OCP_ALM_DISABLE;

    val <<= NU2115A_BAT_OCP_ALM_DIS_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_03,
            NU2115A_BAT_OCP_ALM_DIS_MASK, val);
    return ret;
}*/

static int nu2115a_set_batocp_alarm_th(struct nu2115a *chip, int threshold)
{
    int ret;
    u8 val;

    if (threshold < NU2115A_BAT_OCP_ALM_BASE)
        threshold = NU2115A_BAT_OCP_ALM_BASE;

    val = (threshold - NU2115A_BAT_OCP_ALM_BASE) / NU2115A_BAT_OCP_ALM_LSB;

    val <<= NU2115A_BAT_OCP_ALM_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_03,
            NU2115A_BAT_OCP_ALM_MASK, val);
    return ret;
}

static int nu2115a_set_busovp_th(struct nu2115a *chip, int threshold)
{
    int ret;
    u8 val;

    if (threshold < NU2115A_BUS_OVP_BASE)
        threshold = NU2115A_BUS_OVP_BASE;

    val = (threshold - NU2115A_BUS_OVP_BASE) / NU2115A_BUS_OVP_LSB;

    val <<= NU2115A_BUS_OVP_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_07,
            NU2115A_BUS_OVP_MASK, val);
    return ret;
}
/*
static int nu2115a_enable_busovp_alarm(struct nu2115a *chip, bool enable)
{
    int ret;
    u8 val;

    if (enable)
        val = NU2115A_BUS_OVP_ALM_ENABLE;
    else
        val = NU2115A_BUS_OVP_ALM_DISABLE;

    val <<= NU2115A_BUS_OVP_ALM_DIS_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_08,
            NU2115A_BUS_OVP_ALM_DIS_MASK, val);
    return ret;
}*/

static int nu2115a_set_busovp_alarm_th(struct nu2115a *chip, int threshold)
{
    int ret;
    u8 val;

    if (threshold < NU2115A_BUS_OVP_ALM_BASE)
        threshold = NU2115A_BUS_OVP_ALM_BASE;

    val = (threshold - NU2115A_BUS_OVP_ALM_BASE) / NU2115A_BUS_OVP_ALM_LSB;

    val <<= NU2115A_BUS_OVP_ALM_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_08,
            NU2115A_BUS_OVP_ALM_MASK, val);
    return ret;
}

static int nu2115a_enable_busocp(struct nu2115a *chip, bool enable)
{
    int ret = 0;
    u8 val;

    if (enable)
        val = NU2115A_BUS_OCP_DISABLE;
    else
        val = NU2115A_BUS_OCP_ENABLE;

    val <<= NU2115A_BUS_OCP_DIS_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_09,
            NU2115A_BUS_OCP_DIS_MASK, val);

    return ret;
}

static int nu2115a_set_busocp_th(struct nu2115a *chip, int threshold)
{
    int ret;
    u8 val;

    if (threshold < NU2115A_BUS_OCP_BASE)
        threshold = NU2115A_BUS_OCP_BASE;

    val = (threshold - NU2115A_BUS_OCP_BASE) / (NU2115A_BUS_OCP_LSB);

    val <<= NU2115A_BUS_OCP_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_09,
            NU2115A_BUS_OCP_MASK, val);
    return ret;
}
/*
static int nu2115a_enable_busocp_alarm(struct nu2115a *chip, bool enable)
{
    int ret;
    u8 val;

    if (enable)
        val = NU2115A_BUS_OCP_ALM_ENABLE;
    else
        val = NU2115A_BUS_OCP_ALM_DISABLE;

    val <<= NU2115A_BUS_OCP_ALM_DIS_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_0A,
            NU2115A_BUS_OCP_ALM_DIS_MASK, val);
    return ret;
}*/

static int nu2115a_set_busocp_alarm_th(struct nu2115a *chip, int threshold)
{
    int ret;
    u8 val;

    if (threshold < NU2115A_BUS_OCP_ALM_BASE)
        threshold = NU2115A_BUS_OCP_ALM_BASE;

    val = (threshold - NU2115A_BUS_OCP_ALM_BASE) / NU2115A_BUS_OCP_ALM_LSB;

    val <<= NU2115A_BUS_OCP_ALM_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_0A,
            NU2115A_BUS_OCP_ALM_MASK, val);
    return ret;
}
/*
static int nu2115a_enable_batucp_alarm(struct nu2115a *chip, bool enable)
{
    int ret;
    u8 val;

    if (enable)
        val = NU2115A_BAT_UCP_ALM_ENABLE;
    else
        val = NU2115A_BAT_UCP_ALM_DISABLE;

    val <<= NU2115A_BAT_UCP_ALM_DIS_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_04,
            NU2115A_BAT_UCP_ALM_DIS_MASK, val);
    return ret;
}*/
/*
static int nu2115a_set_batucp_alarm_th(struct nu2115a *chip, int threshold)
{
    int ret;
    u8 val;

    if (threshold < NU2115A_BAT_UCP_ALM_BASE)
        threshold = NU2115A_BAT_UCP_ALM_BASE;

    val = (threshold - NU2115A_BAT_UCP_ALM_BASE) / NU2115A_BAT_UCP_ALM_LSB;

    val <<= NU2115A_BAT_UCP_ALM_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_04,
            NU2115A_BAT_UCP_ALM_MASK, val);
    return ret;
}*/

static int nu2115a_set_reg_ctrl_i2c(struct nu2115a *chip)
{
    int ret;

    ret = nu2115a_write_byte(chip, 0xF7, 0x00);
    ret = nu2115a_write_byte(chip, 0xF7, 0x78);
    ret = nu2115a_write_byte(chip, 0xF7, 0x87);
    ret = nu2115a_write_byte(chip, 0xF7, 0xAA);
    ret = nu2115a_write_byte(chip, 0xF7, 0x55);
    ret = nu2115a_write_byte(chip, 0x8C, 0x20);

    return ret;
}

static int nu2115a_set_acovp_th(struct nu2115a *chip, int threshold)
{
    int ret = 0;
    u8 val;

    if (threshold < NU2115A_AC1_OVP_BASE)
        threshold = NU2115A_AC1_OVP_BASE;

    if (threshold == NU2115A_AC1_OVP_6P5V)
        val = 0x07;
    else
        val = (threshold - NU2115A_AC1_OVP_BASE) /  NU2115A_AC1_OVP_LSB;

    val <<= NU2115A_AC1_OVP_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_05,
            NU2115A_AC1_OVP_MASK, val);

    return ret;
}
/*
static int nu2115a_enable_otg(struct nu2115a *chip, bool en)
{
    int ret = 0;
    u8 val;

    if (en)
        val = NU2115A_OTG_ENABLE;
    else
        val = NU2115A_OTG_DISABLE;

    val <<= NU2115A_EN_OTG_SHIFT;
    ret = nu2115a_update_bits(chip, NU2115A_REG_2F,
            NU2115A_EN_OTG_MASK, val);
    return ret;
}*/
/*
static int nu2115a_check_enable_otg(struct nu2115a *chip)
{
    int ret;
    u8 val;

    ret = nu2115a_read_byte(chip, NU2115A_REG_2F, &val);
    nu_info(">>>reg [0x2F] = 0x%02x\n", val);
    if (!ret)
        ret = !!(val & NU2115A_EN_OTG_MASK);
    return ret;
}*/

/*
static int nu2115a_enable_acdrv1(struct nu2115a *chip, bool en)
{
    int ret = 0;
    u8 val;

    if (en)
        val = NU2115A_ACDRV1_ENABLE;
    else
        val = NU2115A_ACDRV1_DISABLE;

    val <<= NU2115A_EN_ACDRV1_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_30,
            NU2115A_EN_ACDRV1_MASK, val);

    return ret;
}*/

/*
static int nu2115a_check_enable_acdrv1(struct nu2115a *chip, bool *enable)
{
    int ret;
    u8 val;

    ret = nu2115a_read_byte(chip, NU2115A_REG_30, &val);
    nu_info(">>>reg [0x30] = 0x%02x\n", val);
    if (!ret)
        *enable = !!(val & NU2115A_EN_ACDRV1_MASK);
    return ret;
}*/


/*
static int nu2115a_enable_acdrv2(struct nu2115a *chip, bool enable)
{
    int ret = 0;
    u8 val;

    if (enable)
        val = NU2115A_ACDRV2_ENABLE;
    else
        val = NU2115A_ACDRV2_DISABLE;

    val <<= NU2115A_EN_ACDRV2_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_30,
            NU2115A_EN_ACDRV2_MASK, val);

    return ret;
}*/


/*
static int nu2115a_check_enable_acdrv2(struct nu2115a *chip, bool *enable)
{
    int ret;
    u8 val;

    ret = nu2115a_read_byte(chip, NU2115A_REG_30, &val);
    nu_info(">>>reg [0x30] = 0x%02x\n", val);
    if (!ret)
        *enable = !!(val & NU2115A_EN_ACDRV2_MASK);
    return ret;
}*/

static int nu2115a_enable_adc(struct nu2115a *chip, bool enable)
{
    int ret;
    u8 val;

    dev_err(chip->dev,"nu2115a set adc :%d\n", enable);

    if (enable)
        val = NU2115A_ADC_ENABLE;
    else
        val = NU2115A_ADC_DISABLE;

    val <<= NU2115A_ADC_EN_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_15,
            NU2115A_ADC_EN_MASK, val);
    return ret;
}

static int nu2115a_set_adc_scanrate(struct nu2115a *chip, bool oneshot)
{
    int ret;
    u8 val;

    if (oneshot)
        val = NU2115A_ADC_RATE_ONESHOT;
    else
        val = NU2115A_ADC_RATE_CONTINOUS;

    val <<= NU2115A_ADC_RATE_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_15,
            NU2115A_ADC_RATE_MASK, val);
    return ret;
}

static int nu2115a_get_adc_data(struct nu2115a *chip, int channel, int *data)
{
    int ret;
    u8 val_l, val_h;
    u16 val;

    /*nu_err("[nu2115a_get_adc_data]: channel = %d", channel);*/

    if(channel >= ADC_MAX_NUM) return -EINVAL;

    ret = nu2115a_read_byte(chip, ADC_REG_BASE + (channel << 1), &val_h);
    ret = nu2115a_read_byte(chip, ADC_REG_BASE + (channel << 1) + 1, &val_l);

    if (ret < 0)
        return ret;
    val = (val_h << 8) | val_l;
    // if((channel == ADC_VBUS) || (channel == ADC_IBUS))
    // {
	// nu_err("[nu2115a_get_adc_data] false666: ret = %d", ret);
    //    nu2115a_dump(chip);
    // }
//    nu_err("[nu2115a_get_adc_data]: val = %d", val);

    if((channel == ADC_TSBUS) || (channel == ADC_TSBAT))
        val = val * 9766/100000;
    else if(channel == ADC_TDIE)
        val = val * 5/10;

    *data = val;

    return ret;
}

static int nu2115a_set_adc_scan(struct nu2115a *chip, int channel, bool enable)
{
    int ret;
    u8 reg;
    u8 mask;
    u8 shift;
    u8 val;

    if (channel > ADC_MAX_NUM)
        return -EINVAL;

    if (channel == ADC_IBUS) {
        reg = NU2115A_REG_15;
        shift = NU2115A_IBUS_ADC_DIS_SHIFT;
        mask = NU2115A_IBUS_ADC_DIS_MASK;
    } else if (channel == ADC_VBUS) {
        reg = NU2115A_REG_15;
        shift = NU2115A_IBUS_ADC_DIS_SHIFT;
        mask = NU2115A_IBUS_ADC_DIS_MASK;
    } else {
        reg = NU2115A_REG_16;
        shift = 9 - channel;
        mask = 1 << shift;
    }

    if (enable)
        val = 0 << shift;
    else
        val = 1 << shift;

    ret = nu2115a_update_bits(chip, reg, mask, val);

    return ret;
}


/*init mask*/
static int nu2115a_set_alarm_int_mask(struct nu2115a *chip, u8 mask)
{
    int ret;
    u8 val;

    ret = nu2115a_read_byte(chip, NU2115A_REG_0F, &val);
    if (ret)
        return ret;

    val |= mask;

    ret = nu2115a_write_byte(chip, NU2115A_REG_0F, val);

    return ret;
}

/*
static int NU2115A_clear_alarm_int_mask(struct nu2115a *chip, u8 mask)
{
  int ret;
  u8 val;

  ret = nu2115a_read_byte(chip, NU2115A_REG_0F, &val);
  if (ret)
  return ret;

  val &= ~mask;

  ret = nu2115a_write_byte(chip, NU2115A_REG_0F, val);

  return ret;
}*/

/*
static int nu2115a_set_fault_int_mask(struct nu2115a *chip, u8 mask)
{
  int ret;
  u8 val;

  ret = nu2115a_read_byte(chip, NU2115A_REG_12, &val);
  if (ret)
  return ret;

  val |= mask;

  ret = nu2115a_write_byte(chip, NU2115A_REG_12, val);

  return ret;
}*/


/*
static int nu2115a_clear_fault_int_mask(struct nu2115a *chip, u8 mask)
{
  int ret;
  u8 val;

  ret = nu2115a_read_byte(chip, NU2115A_REG_12, &val);
  if (ret)
  return ret;

  val &= ~mask;

  ret = nu2115a_write_byte(chip, NU2115A_REG_12, val);

  return ret;
}*/

static int nu2115a_set_sense_resistor(struct nu2115a *chip, int r_mohm)
{
    int ret = 0;

    u8 val;

    if (r_mohm == 2)
        val = NU2115A_SET_IBAT_SNS_RES_2MHM;
    else if (r_mohm == 5)
        val = NU2115A_SET_IBAT_SNS_RES_5MHM;
    else
        return -EINVAL;

    val <<= NU2115A_SET_IBAT_SNS_RES_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_2E,
            NU2115A_SET_IBAT_SNS_RES_MASK,
            val);

    return ret;
}

#if 0
static int nu2115a_enable_regulation(struct nu2115a *chip, bool enable)
{
    int ret = 0;



    u8 val;

    if (enable)
        val = NU2115A_EN_REGULATION_ENABLE;
    else
        val = NU2115A_EN_REGULATION_DISABLE;

    val <<= NU2115A_EN_REGULATION_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_2B,
            NU2115A_EN_REGULATION_MASK,
            val);
    return ret;
}

/*
static int nu2115a_set_ss_timeout(struct nu2115a *chip, int timeout)
{
    int ret =0;
    u8 val;

    switch (timeout) {
        case 0:
            val = NU2115A_SS_TIMEOUT_DISABLE;
            break;
        case 12:
            val = NU2115A_SS_TIMEOUT_12P5MS;
            break;
        case 25:
            val = NU2115A_SS_TIMEOUT_25MS;
            break;
        case 50:
            val = NU2115A_SS_TIMEOUT_50MS;
            break;
        case 100:
            val = NU2115A_SS_TIMEOUT_100MS;
            break;
        case 400:
            val = NU2115A_SS_TIMEOUT_400MS;
            break;
        case 1500:
            val = NU2115A_SS_TIMEOUT_1500MS;
            break;
        case 100000:
            val = NU2115A_SS_TIMEOUT_100000MS;
            break;
        default:
            val = NU2115A_SS_TIMEOUT_DISABLE;
            break;
    }

    val <<= NU2115A_SS_TIMEOUT_SET_SHIFT;;

    ret = nu2115a_update_bits(chip, NU2115A_REG_2E,
            NU2115A_SS_TIMEOUT_SET_MASK,
            val);

    return ret;
}*/
#endif
/*there is no set_ibat reg*/
#if 0
static int nu2115a_set_ibat_reg_th(struct nu2115a *chip, int th_ma)
{
    int ret =0;
    u8 val;

    if (th_ma == 200)
        val = NU2115A_IBAT_REG_200MA;
    else if (th_ma == 300)
        val = NU2115A_IBAT_REG_300MA;
    else if (th_ma == 400)
        val = NU2115A_IBAT_REG_400MA;
    else if (th_ma == 500)
        val = NU2115A_IBAT_REG_500MA;
    else
        val = NU2115A_IBAT_REG_500MA;

    val <<= NU2115A_IBAT_REG_SHIFT;
    ret = nu2115a_update_bits(chip, NU2115A_REG_2C,
            NU2115A_IBAT_REG_MASK,
            val);

    return ret;
}
#endif

/*there is no set_vbat reg*/
#if 0
static int nu2115a_set_vbat_reg_th(struct nu2115a *chip, int th_mv)
{
    int ret = 0;
    u8 val;

    if (th_mv == 50)
        val = NU2115A_VBAT_REG_50MV;
    else if (th_mv == 100)
        val = NU2115A_VBAT_REG_100MV;
    else if (th_mv == 150)
        val = NU2115A_VBAT_REG_150MV;
    else
        val = NU2115A_VBAT_REG_200MV;

    val <<= NU2115A_VBAT_REG_SHIFT;

    ret = nu2115a_update_bits(chip, NU2115A_REG_2C,
            NU2115A_VBAT_REG_MASK,
            val);

    return ret;
}
#endif
/*
static int nu2115a_check_vbus_error_status(struct nu2115a *chip, uint32_t *state)
{
    int ret;
    u8 data;

    ret = nu2115a_read_byte(chip, NU2115A_REG_35, &data);
    if(ret == 0){
        chip->vbus_error_low = !!(data & (1 << NU2115A_VBUS_ERRLO_STAT_SHIFT));
        chip->vbus_error_high = !!(data & (1 << NU2115A_VBUS_ERRHI_STAT_SHIFT));
        nu_info("vbus_error_low:%d, vbus_error_high:%d \n", chip->vbus_error_low, chip->vbus_error_high);
    }

    if (chip->vbus_error_low != 0)
        *state |= BIT(VBUS_ERROR_L);

    if (chip->vbus_error_high != 0)
        *state |= BIT(VBUS_ERROR_H);

    return ret;
}*/

static int nu2115a_detect_device(struct nu2115a *chip)
{
    int ret;
    u8 data;

    ret = nu2115a_read_byte(chip, NU2115A_REG_31, &data);
    if (ret == 0) {
        chip->part_no = (data & NU2115A_DEV_ID_MASK);
        chip->part_no >>= NU2115A_DEV_ID_SHIFT;
    }

    return ret;
}

void nu2115a_check_alarm_status(struct nu2115a *chip)
{
    int ret1;
    int ret2;
    u8 flag = 0;
    u8 stat1 = 0;
    u8 stat2 = 0;

    mutex_lock(&chip->data_lock);
#if 0
    ret = nu2115a_read_byte(chip, NU2115A_REG_08, &flag);
    if (!ret && (flag & NU2115A_IBUS_UCP_FALL_FLAG_MASK))
        nu_dbg("UCP_FLAG =0x%02X\n",
                !!(flag & NU2115A_IBUS_UCP_FALL_FLAG_MASK));

    ret = nu2115a_read_byte(chip, NU2115A_REG_2D, &flag);
    if (!ret && (flag & NU2115A_VDROP_OVP_FLAG_MASK))
        nu_dbg("VDROP_OVP_FLAG =0x%02X\n",
                !!(flag & NU2115A_VDROP_OVP_FLAG_MASK));
#endif
    /*read to clear alarm flag*/
    ret1 = nu2115a_read_byte(chip, NU2115A_REG_10, &flag);
    if (!ret1 && flag)
        nu_dbg("INT_FLAG =0x%02X\n", flag);

    ret1 = nu2115a_read_byte(chip, NU2115A_REG_0F, &stat1);
    ret2 = nu2115a_read_byte(chip, NU2115A_REG_36, &stat2);

    if (!ret1 && !ret2 && stat1 != chip->prev_alarm && stat2 != chip->prev_alarm) {
        nu_dbg("INT_STAT = stat1 0X%02x, stat2 0X%02x\n", stat1, stat2);
        chip->prev_alarm = stat1;
        chip->bat_ovp_alarm = !!(stat1 & BAT_OVP_ALARM);
        chip->bat_ocp_alarm = !!(stat1 & BAT_OCP_ALARM);
        chip->bus_ovp_alarm = !!(stat1 & BUS_OVP_ALARM);
        chip->bus_ocp_alarm = !!(stat1 & BUS_OCP_ALARM);
        chip->batt_present  = !!(stat1 & VBAT_INSERT);
        chip->bat_ucp_alarm = !!(stat1 & BAT_UCP_ALARM);
        chip->vbus_present  = !!(stat2 & VBUS_INSERT);
    }


#if 0
    ret = nu2115a_read_byte(chip, NU2115A_REG_08, &stat);
    if (!ret && (stat & 0x50))
        nu_err("Reg[05]BUS_UCPOVP = 0x%02X\n", stat);

    ret = nu2115a_read_byte(chip, NU2115A_REG_0A, &stat);
    if (!ret && (stat & 0x02))
        nu_err("Reg[0A]CONV_OCP = 0x%02X\n", stat);
#endif
    mutex_unlock(&chip->data_lock);
}

void nu2115a_check_fault_status(struct nu2115a *chip)
{
    int ret;
    u8 flag = 0;
    u8 stat = 0;
    bool changed = false;

    mutex_lock(&chip->data_lock);
    ret = nu2115a_read_byte(chip, NU2115A_REG_05, &stat);
    if (!ret && stat)
        nu_err("FAULT_STAT_05 = 0x%02X\n", stat);

    ret = nu2115a_read_byte(chip, NU2115A_REG_06, &stat);
    if (!ret && stat)
        nu_err("FAULT_STAT_06 = 0x%02X\n", stat);

    ret = nu2115a_read_byte(chip, NU2115A_REG_09, &stat);
    if (!ret && stat)
        nu_err("FAULT_STAT_09 = 0x%02X\n", stat);

    ret = nu2115a_read_byte(chip, NU2115A_REG_0A, &stat);
    if (!ret && stat)
        nu_err("FAULT_STAT_0a = 0x%02X\n", stat);

    ret = nu2115a_read_byte(chip, NU2115A_REG_0B, &stat);
    if (!ret && stat)
        nu_err("FAULT_STAT_0b = 0x%02X\n", stat);

    ret = nu2115a_read_byte(chip, NU2115A_REG_0C, &stat);
    if (!ret && stat)
        nu_err("FAULT_STAT_0c = 0x%02X\n", stat);

    ret = nu2115a_read_byte(chip, NU2115A_REG_12, &stat);
    if (!ret && stat)
        nu_err("FAULT_STAT_12 = 0x%02X\n", stat);

    ret = nu2115a_read_byte(chip, NU2115A_REG_13, &flag);
    if (!ret && flag)
        nu_err("FAULT_FLAG_13 = 0x%02X\n", flag);

    if (!ret && flag != chip->prev_fault) {
        changed = true;
        chip->prev_fault = flag;
        chip->bat_ovp_fault = !!(flag & BAT_OVP_FAULT);
        chip->bat_ocp_fault = !!(flag & BAT_OCP_FAULT);
        chip->bus_ovp_fault = !!(flag & BUS_OVP_FAULT);
        chip->bus_ocp_fault = !!(flag & BUS_OCP_FAULT);
        chip->bat_therm_fault = !!(flag & TS_BAT_FAULT);
        chip->bus_therm_fault = !!(flag & TS_BUS_FAULT);

        chip->bat_therm_alarm = !!(flag & TBUS_TBAT_ALARM);
        chip->bus_therm_alarm = !!(flag & TBUS_TBAT_ALARM);
    }

    mutex_unlock(&chip->data_lock);
}

/*static int nu2115a_check_reg_status(struct nu2115a *chip)
  {
  int ret;
  u8 val;

  ret = nu2115a_read_byte(chip, NU2115A_REG_2C, &val);
  if (!ret) {
  chip->vbat_reg = !!(val & NU2115A_VBAT_REG_ACTIVE_STAT_MASK);
  chip->ibat_reg = !!(val & NU2115A_IBAT_REG_ACTIVE_STAT_MASK);
  }


  return ret;
  }*/

static ssize_t nu2115a_show_registers(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct nu2115a *chip = dev_get_drvdata(dev);
    u8 addr;
    u8 val;
    u8 tmpbuf[400];
    int len;
    int idx = 0;
    int ret;

    idx = snprintf(buf, PAGE_SIZE, "%s:\n", "nu2115a");
    for (addr = 0x0; addr <= 0x36; addr++) {
        ret = nu2115a_read_byte(chip, addr, &val);
        if (ret == 0) {
            len = snprintf(tmpbuf, PAGE_SIZE - idx,
                    "Reg[%.2X] = 0x%.2x\n", addr, val);
            memcpy(&buf[idx], tmpbuf, len);
            idx += len;
        }
    }

    return idx;
}

static ssize_t nu2115a_store_register(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct nu2115a *chip = dev_get_drvdata(dev);
    int ret;
    unsigned int reg;
    unsigned int val;

    ret = sscanf(buf, "%x %x", &reg, &val);
    if (ret == 2 && reg <= 0x31)
        nu2115a_write_byte(chip, (unsigned char)reg, (unsigned char)val);

    return count;
}

static DEVICE_ATTR(registers, 0660, nu2115a_show_registers, nu2115a_store_register);

static void nu2115a_create_device_node(struct device *dev)
{
    device_create_file(dev, &dev_attr_registers);
}

static int nu2115a_init_adc(struct nu2115a *chip)
{
    nu2115a_set_adc_scanrate(chip, false);
    nu2115a_set_adc_scan(chip, ADC_IBUS, true);
    nu2115a_set_adc_scan(chip, ADC_VBUS, true);
    nu2115a_set_adc_scan(chip, ADC_VOUT, true);
    nu2115a_set_adc_scan(chip, ADC_VBAT, true);
    nu2115a_set_adc_scan(chip, ADC_IBAT, true);
    nu2115a_set_adc_scan(chip, ADC_TSBUS, false);
    nu2115a_set_adc_scan(chip, ADC_TSBAT, false);
    nu2115a_set_adc_scan(chip, ADC_TDIE, true);
    nu2115a_set_adc_scan(chip, ADC_VAC1, true);
    nu2115a_set_adc_scan(chip, ADC_VAC2, true);

    nu2115a_enable_adc(chip, true);

    return 0;
}

static int nu2115a_init_int_src(struct nu2115a *chip)
{
    int ret = 0;
    /*TODO:be careful ts bus and ts bat alarm bit mask is in
     *    fault mask register, so you need call
     *    nu2115a_set_fault_int_mask for tsbus and tsbat alarm
     */
#if 1
    ret = nu2115a_set_alarm_int_mask(chip, ADC_DONE
            /*            | BAT_UCP_ALARM */
            | BAT_OVP_ALARM);
    if (ret) {
        nu_err("failed to set alarm mask:%d\n", ret);
        return ret;
    }
#endif
#if 0
    ret = nu2115a_set_fault_int_mask(chip, TS_BUS_FAULT);
    if (ret) {
        nu_err("failed to set fault mask:%d\n", ret);
        return ret;
    }
#endif
    return ret;
}

static int nu2115a_init_protection(struct nu2115a *chip)
{
    int ret;

    ret = nu2115a_enable_batovp(chip, chip->cfg->bat_ovp_disable);
    nu_err("%s bat ovp %s\n",
            chip->cfg->bat_ovp_disable ? "disable" : "enable",
            !ret ? "successfullly" : "failed");

    ret = nu2115a_enable_batocp(chip, chip->cfg->bat_ocp_disable);
    nu_err("%s bat ocp %s\n",
            chip->cfg->bat_ocp_disable ? "disable" : "enable",
            !ret ? "successfullly" : "failed");

    ret = nu2115a_enable_busocp(chip, chip->cfg->bus_ocp_disable);
    nu_err("%s bus ocp %s\n",
            chip->cfg->bus_ocp_disable ? "disable" : "enable",
            !ret ? "successfullly" : "failed");

    ret = nu2115a_set_batovp_th(chip, chip->cfg->bat_ovp_th);
    nu_err("set bat ovp th %d %s\n", chip->cfg->bat_ovp_th,
            !ret ? "successfully" : "failed");

    ret = nu2115a_set_batocp_th(chip, chip->cfg->bat_ocp_th);
    nu_err("set bat ocp threshold %d %s\n", chip->cfg->bat_ocp_th,
            !ret ? "successfully" : "failed");

    ret = nu2115a_set_batocp_alarm_th(chip, chip->cfg->bat_ocp_alm_th);
    nu_err("set bat ocp alarm threshold %d %s\n", chip->cfg->bat_ocp_alm_th,
            !ret ? "successfully" : "failed");

    ret = nu2115a_set_busovp_th(chip, chip->cfg->bus_ovp_th);
    nu_err("set bus ovp threshold %d %s\n", chip->cfg->bus_ovp_th,
            !ret ? "successfully" : "failed");

    ret = nu2115a_set_busovp_alarm_th(chip, chip->cfg->bus_ovp_alm_th);
    nu_err("set bus ovp alarm threshold %d %s\n", chip->cfg->bus_ovp_alm_th,
            !ret ? "successfully" : "failed");

    ret = nu2115a_set_busocp_th(chip, chip->cfg->bus_ocp_th);
    nu_err("set bus ocp threshold %d %s\n", chip->cfg->bus_ocp_th,
            !ret ? "successfully" : "failed");

    ret = nu2115a_set_busocp_alarm_th(chip, chip->cfg->bus_ocp_alm_th);
    nu_err("set bus ocp alarm th %d %s\n", chip->cfg->bus_ocp_alm_th,
            !ret ? "successfully" : "failed");

    ret = nu2115a_set_reg_ctrl_i2c(chip);
    nu_err("ctlr i2c %s\n",!ret ? "successfully" : "failed");

    ret = nu2115a_set_acovp_th(chip, chip->cfg->ac_ovp_th);
    nu_err("set ac ovp threshold %d %s\n", chip->cfg->ac_ovp_th,
            !ret ? "successfully" : "failed");
    return 0;
}

static int nu2115a_init_regulation(struct nu2115a *chip)
{
    //nu2115a_set_ibat_reg_th(chip, 300);
    //nu2115a_set_vbat_reg_th(chip, 100);

    //nu2115a_enable_regulation(chip, false);
#if 0
    nu2115a_write_byte(chip, NU2115A_REG_2E, 0x08);
    nu2115a_write_byte(chip, NU2115A_REG_34, 0x01);
#endif
    return 0;
}

static int nu2115a_init_device(struct nu2115a *chip)
{
    nu2115a_set_reg_reset(chip);
    nu2115a_enable_wdt(chip, false);
    nu2115a_disable_cp_ts_detect(chip);
    nu2115a_set_batocp_alarm_th(chip, 12000);
    nu2115a_set_batovp_alarm_th(chip, 5390);
//#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
    //nu2115a_set_errhi_status_mask(chip);
    //nu2115a_set_errlo_status_mask(chip);
//#endif

    //nu2115a_set_ss_timeout(chip, 10000);
    nu2115a_set_sense_resistor(chip, chip->cfg->sense_r_mohm);

    nu2115a_init_protection(chip);
    nu2115a_init_adc(chip);
    nu2115a_init_int_src(chip);

    nu2115a_init_regulation(chip);
    nu2115a_write_byte(chip, NU2115A_REG_00, 0x47);
    nu2115a_write_byte(chip, NU2115A_REG_33, 0x0D);
    nu2115a_write_byte(chip, NU2115A_REG_11, 0xF8);

    return 0;
}

static int nu2115a_set_present(struct nu2115a *chip, bool present)
{
    chip->usb_present = present;

    if (present)
        nu2115a_init_device(chip);
    return 0;
}

/*A06 code for AL7160A-860 by jiashixian at 20240512 start*/
static int nu2115a_get_vbus_present_status(struct nu2115a *chip)
{
    int ret;
    u8 val;
    int s_vbus_present = 0;
    ret = nu2115a_read_byte(chip, NU2115A_REG_36, &val);
    nu_info(">>>reg [0x36] = 0x%02x\n", val);
    if (!ret) {
        s_vbus_present = !!(val & VBUS_INSERT);
    }
    nu_info("s_vbus_present = %d\n", s_vbus_present);
    return s_vbus_present;
}
/*A06 code for AL7160A-860 by jiashixian at 20240512 end*/
static irqreturn_t nu2115a_irq_handler(int irq, void *data)
{
    struct nu2115a *chip = data;

    dev_err(chip->dev,"INT OCCURED\n");

    nu2115a_check_fault_status(chip);

    power_supply_changed(chip->psy);

    return IRQ_HANDLED;
}

static int nu2115a_register_interrupt(struct nu2115a *chip)
{
    int ret;

    if (gpio_is_valid(chip->irq_gpio)) {
        ret = gpio_request_one(chip->irq_gpio, GPIOF_DIR_IN,"nu2115a_irq");
        if (ret) {
            dev_err(chip->dev,"failed to request nu2115a_irq\n");
            return -EINVAL;
        }
        chip->irq = gpio_to_irq(chip->irq_gpio);
        if (chip->irq < 0) {
            dev_err(chip->dev,"failed to gpio_to_irq\n");
            return -EINVAL;
        }
    } else {
        dev_err(chip->dev,"irq gpio not provided\n");
        return -EINVAL;
    }

    if (chip->irq) {
        ret = devm_request_threaded_irq(&chip->client->dev, chip->irq,
                NULL, nu2115a_irq_handler,
                IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                nu2115a_irq_name[chip->mode], chip);

        if (ret < 0) {
            dev_err(chip->dev,"request irq for irq=%d failed, ret =%d\n",
                            chip->irq, ret);
            return ret;
        }
        enable_irq_wake(chip->irq);
    }

    return ret;
}
/********************interrupte end*************************************************/

/*
#define ADC_GET_VBUS    1
#define ADC_GET_VBAT    2
#define ADC_GET_IBUS    3
#define ADC_GET_IBAT    4
#define ADC_GET_TDIE    5
static inline int to_nu2115a_adc(u8 chan)
{
    switch (chan) {
    case ADC_GET_VBUS:
        return ADC_VBUS;
    case ADC_GET_VBAT:
        return ADC_VBAT;
    case ADC_GET_IBUS:
        return ADC_IBUS;
    case ADC_GET_IBAT:
        return ADC_IBAT;
    case ADC_GET_TDIE:
        return ADC_TDIE;
    default:
        break;
    }
    return ADC_MAX_NUM;
}*/

/*
static int nu2115a_enable_charge(struct charger_device *cp_dev, bool enable)
{
    struct nu2115a *chip = charger_get_data(cp_dev);
    return _nu2115a_enable_charge(chip, enable);
}


static int nu2115a_get_is_enable(struct charger_device *cp_dev, bool *enable)
{
    struct nu2115a *chip = charger_get_data(cp_dev);
    return nu2115a_check_charge_enabled(chip, enable);
}*/

/*
static int nu2115a_get_status(struct charger_device *cp_dev, uint32_t *status)
{
    struct nu2115a *chip = charger_get_data(cp_dev);
    return nu2115a_check_vbus_error_status(chip, status);
}*/

/*
static int nu2115a_get_adc_value(struct charger_device *cp_dev, u8 ch, int *value)
{
    struct nu2115a *chip = charger_get_data(cp_dev);
    return nu2115a_get_adc_data(chip, to_nu2115a_adc(ch), value);
}*/

/*
static int nu2115a_adc_enable(struct charger_device *cp_dev, bool en)
{
    struct nu2115a *chip = charger_get_data(cp_dev);
    return nu2115a_enable_adc(chip, en);
}*/

/*
static int nu2115a_get_chip_id(struct charger_device *cp_dev, int *value)
{
    struct nu2115a *chip = charger_get_data(cp_dev);

    *value = chip->part_no;
    return 0;
}*/

/*
static int nu2115a_set_cp_workmode(struct charger_device *cp_dev, int workmode)
{
    int ret = 0;
    u8 val;
    struct nu2115a *chip = charger_get_data(cp_dev);

    if (workmode) {
        val = NU2115A_1_1_MODE;
        nu2115a_write_byte(chip, NU2115A_REG_07, 0x28);
    } else {
        val = NU2115A_2_1_MODE;
        nu2115a_write_byte(chip, NU2115A_REG_07, 0x3F);
    }

    val <<= NU2115A_CHG_MODE_SHIFT;

    dev_info(chip->dev, "enter nu2115a_set_cp_workmode = %d, val = %d\n", workmode, val);

    ret = nu2115a_update_bits(chip, NU2115A_REG_0E, NU2115A_CHG_MODE_MASK, val);
    return ret;
}*/

/*
static int nu2115a_get_cp_workmode(struct charger_device *cp_dev, int *workmode)
{
    int ret;
    u8 val;
    struct nu2115a *chip = charger_get_data(cp_dev);

    ret = nu2115a_read_byte(chip, NU2115A_REG_0E, &val);

    val = val & NU2115A_CHG_MODE_MASK;
    dev_info(chip->dev,"nu2115a in get_cp_workmode val = %d, ret = %d", val, ret);

    if (!ret)
        *workmode = (val == 0x20? NU2115A_1_1_MODE:(val == 0? NU2115A_2_1_MODE: NU2115A_2_1_MODE));

    dev_info(chip->dev,"nu2115a_get_cp_workmode = %d", *workmode);
    return ret;
}*/


//static struct charger_ops nu2115a_ops = {
//.enable_chg = nu2115a_enable_charge,
//.adc_enable = nu2115a_adc_enable,
//.set_enable = nu2115a_set_enable,
//.get_status = nu2115a_get_status,
//.get_is_enable = nu2115a_get_is_enable,
//.get_adc_value = nu2115a_get_adc_value,
//.set_enable_adc = nu2115a_set_enable_adc,
//.get_chip_id = nu2115a_get_chip_id,
//.set_cp_workmode = nu2115a_set_cp_workmode,
//.get_cp_workmode = nu2115a_get_cp_workmode,

/*
.enable_chg = nu2115_enable_charge,
.adc_enable = nu2115_adc_enable,
.enable_acdrv1 = nu2115_enable_acdrv1,
.enable_otg = nu2115_enable_otg,
.get_batt_volt = nu2115_get_bat_volt,
.get_batt_curr = nu2115_get_bat_curr,
.get_bus_volt = nu2115_get_vbus_volt,
.get_bus_curr = nu2115_get_ibus_curr,
.get_vbus_error = nu2115_get_vbus_error,
.get_alarm_status = nu2115_get_alarm_status,
.get_fault_status = nu2115_get_fault_status,
.get_die_temp = nu2115_get_tdie_temp,
.get_vbus_temp = nu2115_get_vbus_temp,
.get_vbat_temp = nu2115_get_vbat_temp,

.get_chg_status = nu2115_check_charge_enabled,
.get_otg_status = nu2115_check_enable_otg,

.get_input_volt_lmt = nu2115_get_input_volt_lmt,
.get_input_curr_lmt = nu2115_get_input_curr_lmt,*/
//};

/************************psy start**************************************/
    /* A06 code for AL7160A-42 by jiashixian at 2024/04/07 start */
static enum power_supply_property nu2115a_charger_props[] = {
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_CHARGING_ENABLED,
    POWER_SUPPLY_PROP_CPM_VBUS_VOLT,
    POWER_SUPPLY_PROP_CPM_IBUS_CURRENT,
    POWER_SUPPLY_PROP_CPM_VBAT_VOLT,
    POWER_SUPPLY_PROP_CPM_IBAT_CURRENT,
    POWER_SUPPLY_PROP_CPM_TEMP,
    /*A06 code for SR-AL7160A-01-265 by shixuanxuan at 20240407 start*/
    POWER_SUPPLY_PROP_CP_OTG_ENABLE,
    /*A06 code for SR-AL7160A-01-265 by shixuanxuan at 20240407 end*/
    /*A06 code for AL7160A-1232 by jiashixian at 20240518 start*/
    POWER_SUPPLY_PROP_CP_ADC_ENABLE,
    /*A06 code for AL7160A-1232 by jiashixian at 20240518 end*/
};

/*A06 code for SR-AL7160A-01-265 by shixuanxuan at 20240407 start*/
static void nu2115a_set_otg_txmode(struct nu2115a *chip, int enable)
{
    int ret = 0;
    u8 val1 = 0, val2 = 0, val3 = 0;

    if (enable) {
        val1 = NU2115A_OTG_ENABLE << NU2115A_EN_OTG_SHIFT;
        val2 = NU2115A_DIS_ACDRV_ENABLE << NU2115A_DIS_ACDRV_SHIFT;
        val3 = NU2115A_ACDRV1_ENABLE << NU2115A_EN_ACDRV1_SHIFT;
        ret |= nu2115a_update_bits(chip, NU2115A_REG_2F, NU2115A_EN_OTG_MASK, val1);
        ret |= nu2115a_update_bits(chip, NU2115A_REG_2F, NU2115A_DIS_ACDRV_MASK, val2);
        ret |= nu2115a_update_bits(chip, NU2115A_REG_30, NU2115A_EN_ACDRV1_MASK, val3);
    } else {
        val1 = NU2115A_OTG_DISABLE << NU2115A_EN_OTG_SHIFT;
        val2 = NU2115A_ACDRV2_ENABLE << NU2115A_EN_ACDRV2_SHIFT;
        val3 = NU2115A_ACDRV1_DISABLE << NU2115A_EN_ACDRV1_SHIFT;
        ret |= nu2115a_update_bits(chip, NU2115A_REG_2F, NU2115A_EN_OTG_MASK, val1);
        ret |= nu2115a_update_bits(chip, NU2115A_REG_30, NU2115A_EN_ACDRV2_MASK, val2);
        ret |= nu2115a_update_bits(chip, NU2115A_REG_30, NU2115A_EN_ACDRV1_MASK, val3);
    }

    if (ret ) {
        pr_info("nu2115a_set_otg_txmode fail ! \n");
    } else {
        pr_info("nu2115a_set_otg_txmode = %d\n", enable);
    }
    /*A06 code for AL7160A-607 by shixuanxuan at 20240507 start*/
    chip->otg_tx_mode = enable;
    /*A06 code for AL7160A-607 by shixuanxuan at 20240507 end*/
}
/*A06 code for SR-AL7160A-01-265 by shixuanxuan at 20240407 end*/

static int nu2115a_charger_get_property(struct power_supply *psy,
                enum power_supply_property psp,
                union power_supply_propval *val)
{
    struct nu2115a *chip = power_supply_get_drvdata(psy);
    int result;
    int ret;
    bool en;

    switch (psp) {
    case POWER_SUPPLY_PROP_CHARGING_ENABLED:
     	ret =    nu2115a_check_charge_enabled(chip, &en);
        if (!ret) {
            chip->charge_enabled = en;
        }
        val->intval = chip->charge_enabled;
        break;
    case POWER_SUPPLY_PROP_PRESENT:
        val->intval = chip->usb_present;
         ret = 0;
        break;
    case POWER_SUPPLY_PROP_CPM_VBUS_VOLT:
        ret = nu2115a_get_adc_data(chip, ADC_VBUS, &result);
        if (!ret)
            chip->vbus_volt = result;
        val->intval = chip->vbus_volt;
        break;
    case POWER_SUPPLY_PROP_CPM_IBUS_CURRENT:
        ret = nu2115a_get_adc_data(chip, ADC_IBUS, &result);
        if (!ret)
            chip->ibus_curr = result;
        val->intval = chip->ibus_curr;
        break;
    case POWER_SUPPLY_PROP_CPM_VBAT_VOLT:
        ret = nu2115a_get_adc_data(chip, ADC_VBAT, &result);
        if (!ret)
            chip->vbat_volt = result;
        val->intval = chip->vbat_volt;
        break;
    case POWER_SUPPLY_PROP_CPM_IBAT_CURRENT:
        ret = nu2115a_get_adc_data(chip, ADC_IBAT, &result);
        if (!ret)
            chip->ibat_curr = result;
        val->intval = chip->ibat_curr;
        break;
    case POWER_SUPPLY_PROP_CPM_TEMP:
        ret = nu2115a_get_adc_data(chip, ADC_TDIE, &result);
        if (!ret)
            chip->die_temp = result;
        val->intval = chip->die_temp;
        break;
    /*A06 code for AL7160A-607 by shixuanxuan at 20240507 start*/
    case POWER_SUPPLY_PROP_CP_OTG_ENABLE:
        val->intval = chip->otg_tx_mode;
        break;
    /*A06 code for AL7160A-607 by shixuanxuan at 20240507 end*/

    default:
        return -EINVAL;
    }

    return 0;
}
    /* A06 code for AL7160A-42 by jiashixian at 2024/04/07 end */

static int nu2115a_charger_set_property(struct power_supply *psy,
                    enum power_supply_property prop,
                    const union power_supply_propval *val)
{
    struct nu2115a *chip = power_supply_get_drvdata(psy);

    switch (prop) {
    case POWER_SUPPLY_PROP_CHARGING_ENABLED:
        _nu2115a_enable_charge(chip, val->intval);
         nu2115a_check_charge_enabled(chip, &chip->charge_enabled);
        dev_info(chip->dev, "POWER_SUPPLY_PROP_CHARGING_ENABLED: %s\n",
                val->intval ? "enable" : "disable");
        break;
    case POWER_SUPPLY_PROP_PRESENT:
        nu2115a_set_present(chip, !!val->intval);
        break;
    /*A06 code for SR-AL7160A-01-265 by shixuanxuan at 20240407 start*/
    case POWER_SUPPLY_PROP_CP_OTG_ENABLE:
        nu2115a_set_otg_txmode(chip, !!val->intval);
        break;
    /*A06 code for SR-AL7160A-01-265 by shixuanxuan at 20240407 end*/
    /*A06 code for AL7160A-1232 by jiashixian at 20240518 start*/
    case POWER_SUPPLY_PROP_CP_ADC_ENABLE:
        nu2115a_enable_adc(chip, !!val->intval);
        break;
    /*A06 code for AL7160A-1232 by jiashixian at 20240518 end*/
    default:
        return -EINVAL;
    }

    return 0;
}

static int nu2115a_charger_is_writeable(struct power_supply *psy,
                    enum power_supply_property prop)
{
    return 0;
}

static int nu2115a_psy_register(struct nu2115a *chip)
{
    chip->psy_cfg.drv_data = chip;
    chip->psy_cfg.of_node = chip->dev->of_node;

    chip->psy_desc.name = "cpm-standalone";//nu2115a_psy_name[chip->mode];

    chip->psy_desc.type = POWER_SUPPLY_TYPE_MAINS;
    chip->psy_desc.properties = nu2115a_charger_props;
    chip->psy_desc.num_properties = ARRAY_SIZE(nu2115a_charger_props);
    chip->psy_desc.get_property = nu2115a_charger_get_property;
    chip->psy_desc.set_property = nu2115a_charger_set_property;
    chip->psy_desc.property_is_writeable = nu2115a_charger_is_writeable;


    chip->psy = devm_power_supply_register(chip->dev,
            &chip->psy_desc, &chip->psy_cfg);
    if (IS_ERR(chip->psy)) {
        dev_err(chip->dev, "%s failed to register psy\n", __func__);
        return PTR_ERR(chip->psy);
    }

    dev_info(chip->dev, "%s power supply register successfully\n", chip->psy_desc.name);

    return 0;
}
/************************psy end**************************************/

static int nu2115a_set_work_mode(struct nu2115a *chip, int mode)
{
    chip->mode = mode;

    dev_err(chip->dev,"work mode is %s\n", chip->mode == NU2115A_STANDALONG
        ? "standalone" : (chip->mode == NU2115A_MASTER ? "master" : "slave"));

    return 0;
}


static int nu2115a_parse_dt(struct nu2115a *chip, struct device *dev)
{
    int ret;
    struct device_node *np = dev->of_node;

    chip->cfg = devm_kzalloc(dev, sizeof(struct nu2115a_cfg),
            GFP_KERNEL);

    if (!chip->cfg)
        return -ENOMEM;

    ret = of_property_read_u32(np, "nuvolta,nu2115a,bat-ovp-disable",
            &chip->cfg->bat_ovp_disable);
    ret = of_property_read_u32(np, "nuvolta,nu2115a,bat-ocp-disable",
            &chip->cfg->bat_ocp_disable);
    ret = of_property_read_u32(np, "nuvolta,nu2115a,bus-ocp-disable",
            &chip->cfg->bus_ocp_disable);

    ret = of_property_read_u32(np, "nuvolta,nu2115a,bat-ovp-threshold",
            &chip->cfg->bat_ovp_th);
    if (ret) {
        nu_err("failed to read bat-ovp-threshold\n");
        return ret;
    }
    ret = of_property_read_u32(np, "nuvolta,nu2115a,bat-ocp-threshold",
            &chip->cfg->bat_ocp_th);
    if (ret) {
        nu_err("failed to read bat-ocp-threshold\n");
        return ret;
    }
    ret = of_property_read_u32(np, "nuvolta,nu2115a,bus-ovp-threshold",
            &chip->cfg->bus_ovp_th);
    if (ret) {
        nu_err("failed to read bus-ovp-threshold\n");
        return ret;
    }
    ret = of_property_read_u32(np, "nuvolta,nu2115a,bus-ovp-alarm-threshold",
            &chip->cfg->bus_ovp_alm_th);
    if (ret) {
        nu_err("failed to read bus-ovp-alarm-threshold\n");
        return ret;
    }
    ret = of_property_read_u32(np, "nuvolta,nu2115a,bus-ocp-threshold",
            &chip->cfg->bus_ocp_th);
    if (ret) {
        nu_err("failed to read bus-ocp-threshold\n");
        return ret;
    }
    ret = of_property_read_u32(np, "nuvolta,nu2115a,bus-ocp-alarm-threshold",
            &chip->cfg->bus_ocp_alm_th);
    if (ret) {
        nu_err("failed to read bus-ocp-alarm-threshold\n");
        return ret;
    }
    ret = of_property_read_u32(np, "nuvolta,nu2115a,ac-ovp-threshold",
            &chip->cfg->ac_ovp_th);
    if (ret) {
        nu_err("failed to read ac-ovp-threshold\n");
        return ret;
    }

    ret = of_property_read_u32(np, "nuvolta,nu2115a,sense-resistor-mohm",
            &chip->cfg->sense_r_mohm);
    if (ret) {
        nu_err("failed to read sense-resistor-mohm\n");
        return ret;
    }

    chip->irq_gpio = of_get_named_gpio(np, "nu2115a,intr_gpio", 0);
    if (!gpio_is_valid(chip->irq_gpio)) {
        dev_err(chip->dev,"fail to valid gpio : %d\n", chip->irq_gpio);
        return -EINVAL;
    }
    return 0;
}

static struct of_device_id nu2115a_charger_match_table[] = {
    {
        .compatible = "nuvolta,nu2115a-standalone",
        .data = &nu2115a_mode_data[NU2115A_STANDALONG],
    },
    {
        .compatible = "nuvolta,nu2115a-master",
        .data = &nu2115a_mode_data[NU2115A_MASTER],
    },

    {
        .compatible = "nuvolta,nu2115a-slave",
        .data = &nu2115a_mode_data[NU2115A_SLAVE],
    },
    {},
};
MODULE_DEVICE_TABLE(of, nu2115a_charger_match_table);

static int nu2115a_charger_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
{
    struct nu2115a *chip;
    const struct of_device_id *match;
    struct device_node *node = client->dev.of_node;
    int ret;

    dev_err(&client->dev, "%s\n", __func__);

    chip =  devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
    if (!chip) {
        ret = -ENOMEM;
        goto err_kzalloc;
    }
    chip->dev = &client->dev;
    chip->client = client;

    mutex_init(&chip->i2c_rw_lock);
    mutex_init(&chip->data_lock);

    ret = nu2115a_detect_device(chip);
    if (ret) {
        nu_err("No nu2115a device found!\n");
        ret = -ENODEV;
        goto err_detect_dev;
    }
    i2c_set_clientdata(client, chip);
    nu2115a_create_device_node(&(client->dev));

    dev_err(chip->dev, "%s\n nu2115a ", __func__);
    match = of_match_node(nu2115a_charger_match_table, node);
    if (match == NULL) {
        nu_err("device tree match not found!\n");
        goto err_match_node;
    }

    ret = nu2115a_set_work_mode(chip, *(int *)match->data);
    if (ret) {
        dev_err(chip->dev,"Fail to set work mode!\n");
        goto err_set_mode;
    }

    ret = nu2115a_parse_dt(chip, &client->dev);
    if (ret < 0) {
        dev_err(chip->dev, "%s parse dt failed(%d)\n", __func__, ret);
        goto err_parse_dt;
    }

    ret = nu2115a_init_device(chip);
    if (ret < 0) {
        dev_err(chip->dev, "%s init device failed(%d)\n", __func__, ret);
        goto err_init_device;
    }

    ret = nu2115a_psy_register(chip);
    if (ret < 0) {
        dev_err(chip->dev, "%s psy register failed(%d)\n", __func__, ret);
        goto err_register_psy;
    }

    ret = nu2115a_register_interrupt(chip);
    if (ret < 0) {
        dev_err(chip->dev, "%s register irq fail(%d)\n",
                    __func__, ret);
        goto err_register_irq;
    }

    device_init_wakeup(chip->dev, 1);//add
    /*A06 code for AL7160A-82 by jiashixian at 20240418 start*/
    s_is_nu2115a_probe_success = true;
    /*A06 code for AL7160A-82 by jiashixian at 20240418 end*/
    /* A06 code for AL7160A-445 by jiashixian at 20240610 start */
    cp_info = NU2115A;
    /* A06 code for AL7160A-445 by jiashixian at 20240610 end */
    /*A06 code for AL7160A-607 by shixuanxuan at 20240507 start*/
    chip->otg_tx_mode = false;
    /*A06 code for AL7160A-607 by shixuanxuan at 20240507 end*/
    nu_err("nu2115a probe successfully, Part Num:%d\n!", chip->part_no);

    return 0;

err_register_psy:
err_register_irq:
//err_register_nu_charger:
err_init_device:
    power_supply_unregister(chip->psy);
err_detect_dev:
err_match_node:
err_set_mode:
err_parse_dt:
    devm_kfree(&client->dev, chip);
err_kzalloc:
    dev_err(&client->dev,"nu2115a probe fail\n");
    return ret;
}


static int nu2115a_charger_remove(struct i2c_client *client)
{
    if (s_is_nu2115a_probe_success) {
        struct nu2115a *chip = i2c_get_clientdata(client);
        nu2115a_enable_adc(chip, false);
        mutex_destroy(&chip->data_lock);
        mutex_destroy(&chip->i2c_rw_lock);
        power_supply_unregister(chip->psy);
        devm_kfree(&client->dev, chip);
    }
    return 0;
}

/*A06 code for AL7160A-82 by jiashixian at 20240418 start*/
static void nu2115a_charger_shutdown(struct i2c_client *client)
{
    if (s_is_nu2115a_probe_success) {
        struct nu2115a *chip = i2c_get_clientdata(client);
        nu2115a_enable_adc(chip, false);
    }
}
/*A06 code for AL7160A-82 by jiashixian at 20240418 end*/

static int nu2115a_suspend(struct device *dev)
{
    if (s_is_nu2115a_probe_success) {
        struct nu2115a *chip = dev_get_drvdata(dev);
        dev_info(chip->dev, "Suspend successfully!");
        if (device_may_wakeup(dev)) {
            enable_irq_wake(chip->irq);
        }
        disable_irq(chip->irq);
        /*A06 code for AL7160A-157 by jiashixian at 20240429 start*/
        /*A06 code for AL7160A-860 by jiashixian at 20240512 start*/
        if(!nu2115a_get_vbus_present_status(chip)) {
            nu2115a_enable_adc(chip, false);
        }
        /*A06 code for AL7160A-860 by jiashixian at 20240512 end*/
        /*A06 code for AL7160A-157 by jiashixian at 20240429 end*/
    }
    return 0;
}

static int nu2115a_resume(struct device *dev)
{
    if (s_is_nu2115a_probe_success) {
        struct nu2115a *chip = dev_get_drvdata(dev);
        dev_info(chip->dev, "Resume successfully!");
        if (device_may_wakeup(dev)) {
            disable_irq_wake(chip->irq);
        }
        enable_irq(chip->irq);
        /*A06 code for AL7160A-157 by jiashixian at 20240429 start*/
        /*A06 code for AL7160A-860 by jiashixian at 20240512 start*/
        nu2115a_enable_adc(chip, true);
        /*A06 code for AL7160A-860 by jiashixian at 20240512 end*/
        /*A06 code for AL7160A-157 by jiashixian at 20240429 end*/
    }
    return 0;
}
static const struct dev_pm_ops nu2115a_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(nu2115a_suspend, nu2115a_resume)
};

static const struct i2c_device_id nu2115a_charger_id[] = {
    {"nu2115a-standalone", NU2115A_STANDALONG},
    {},
};
MODULE_DEVICE_TABLE(i2c, nu2115a_charger_id);

static struct i2c_driver nu2115a_charger_driver = {
    .driver        = {
        .name    = "nu2115a-charger",
        .owner    = THIS_MODULE,
        .of_match_table = nu2115a_charger_match_table,
        .pm    = &nu2115a_pm_ops,
    },
    .id_table    = nu2115a_charger_id,

    .probe        = nu2115a_charger_probe,
    .remove        = nu2115a_charger_remove,
    .shutdown    = nu2115a_charger_shutdown,
};

module_i2c_driver(nu2115a_charger_driver);

MODULE_DESCRIPTION("Nuvolta NU2115A Charge Pump Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("mick.ye@nuvoltatech.com");
