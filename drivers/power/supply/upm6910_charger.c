/*
 * UPM6910 battery charging driver
 *
 * Copyright (C) 2022 SGM
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define pr_fmt(fmt)    "[upm6910]:%s: " fmt, __func__

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

#include <charger_class.h>
#include <mtk_charger.h>

#include "upm6910.h"
/*HS03s for SR-AL5628-01-162 by zhangjiangbin at 20220915 start*/
#include <linux/hardinfo_charger.h>
/*HS03s for SR-AL5628-01-162 by zhangjiangbin at 20220915 end*/

struct upm6910 {
    struct device *dev;
    struct i2c_client *client;

    int part_no;

    const char *chg_dev_name;
    const char *eint_name;

    bool chg_det_enable;

    // enum charger_type chg_type;
    struct power_supply_desc psy_desc;
    int psy_usb_type;

    int status;
    int irq;

    struct mutex i2c_rw_lock;

    bool charge_enabled;    /* Register bit status */
    bool power_good;

    struct upm6910_platform_data *platform_data;
    struct charger_device *chg_dev;

    struct power_supply *psy;
};

static const struct charger_properties upm6910_chg_props = {
    .alias_name = "upm6910",
};

static int __upm6910_read_reg(struct upm6910 *upm, u8 reg, u8 *data)
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

static int __upm6910_write_reg(struct upm6910 *upm, int reg, u8 val)
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

static int upm6910_read_byte(struct upm6910 *upm, u8 reg, u8 *data)
{
    int ret;

    mutex_lock(&upm->i2c_rw_lock);
    ret = __upm6910_read_reg(upm, reg, data);
    mutex_unlock(&upm->i2c_rw_lock);

    return ret;
}

static int upm6910_write_byte(struct upm6910 *upm, u8 reg, u8 data)
{
    int ret;

    mutex_lock(&upm->i2c_rw_lock);
    ret = __upm6910_write_reg(upm, reg, data);
    mutex_unlock(&upm->i2c_rw_lock);

    if (ret)
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

    return ret;
}

static int upm6910_update_bits(struct upm6910 *upm, u8 reg, u8 mask, u8 data)
{
    int ret;
    u8 tmp;

    mutex_lock(&upm->i2c_rw_lock);
    ret = __upm6910_read_reg(upm, reg, &tmp);
    if (ret) {
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
        goto out;
    }

    tmp &= ~mask;
    tmp |= data & mask;

    ret = __upm6910_write_reg(upm, reg, tmp);
    if (ret)
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
    mutex_unlock(&upm->i2c_rw_lock);
    return ret;
}

static int upm6910_enable_otg(struct upm6910 *upm)
{
    u8 val = REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT;

    return upm6910_update_bits(upm, UPM6910_REG_01, REG01_OTG_CONFIG_MASK,
                    val);

}

static int upm6910_disable_otg(struct upm6910 *upm)
{
    u8 val = REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT;

    return upm6910_update_bits(upm, UPM6910_REG_01, REG01_OTG_CONFIG_MASK,
                    val);

}

static int upm6910_enable_charger(struct upm6910 *upm)
{
    int ret;
    u8 val = REG01_CHG_ENABLE << REG01_CHG_CONFIG_SHIFT;

    ret =
        upm6910_update_bits(upm, UPM6910_REG_01, REG01_CHG_CONFIG_MASK, val);

    return ret;
}

static int upm6910_disable_charger(struct upm6910 *upm)
{
    int ret;
    u8 val = REG01_CHG_DISABLE << REG01_CHG_CONFIG_SHIFT;

    ret =
        upm6910_update_bits(upm, UPM6910_REG_01, REG01_CHG_CONFIG_MASK, val);
    return ret;
}

int upm6910_set_chargecurrent(struct upm6910 *upm, int curr)
{
    u8 ichg;

    if (curr < REG02_ICHG_MIN) {
        curr = REG02_ICHG_MIN;
    } else if (curr > REG02_ICHG_MAX) {
        curr = REG02_ICHG_MAX;
    }

    ichg = (curr - REG02_ICHG_BASE) / REG02_ICHG_LSB;
    /*hs04 code for AL6398ADEU-249 by shanxinkai at 2023/04/12 start*/
    pr_err("%s: charge curr = %d\n", __func__, curr);
    /*hs04 code for AL6398ADEU-249 by shanxinkai at 2023/04/12 end*/

    return upm6910_update_bits(upm, UPM6910_REG_02, REG02_ICHG_MASK,
                    ichg << REG02_ICHG_SHIFT);

}

/*hs04 code for AL6398ADEU-249 by shanxinkai at 2023/04/12 start*/
int upm6910_get_chargecurrent(struct upm6910 *upm, int *curr_ma)
{
    u8 reg_val = 0;
    int ichg = 0;
    int ret = 0;

    if (!upm) {
        pr_err("%s device is null\n", __func__);
        return -ENODEV;
    }

    ret = upm6910_read_byte(upm, UPM6910_REG_02, &reg_val);
    if (!ret) {
        ichg = (reg_val & REG02_ICHG_MASK) >> REG02_ICHG_SHIFT;
        ichg = ichg * REG02_ICHG_LSB + REG02_ICHG_BASE;
        *curr_ma = ichg;
    }

    return ret;
}

int upm6910_chargecurrent_recheck(struct upm6910 *upm)
{
    int curr_val = 0;
    int ret = 0;

    if (!upm) {
        pr_err("%s device is null\n", __func__);
        return -ENODEV;
    }

    ret = upm6910_get_chargecurrent(upm, &curr_val);
    if (!ret) {
        pr_info("%s the curr value is %d\n", __func__, curr_val);
        if (curr_val < ICHG_OCP_THRESHOLD_MA) {
            ret = upm6910_set_chargecurrent(upm, ICHG_OCP_THRESHOLD_MA);
        } else {
            pr_err("%s get charge current failed\n", __func__);
        }
    }

    return ret;
}
/*hs04 code for AL6398ADEU-249 by shanxinkai at 2023/04/12 end*/

int upm6910_set_term_current(struct upm6910 *upm, int curr)
{
    u8 reg_val;

    if (curr > REG03_ITERM_MAX) {
        curr = REG03_ITERM_MAX;
    } else if (curr < REG03_ITERM_MIN) {
        curr = REG03_ITERM_MIN;
    }

    reg_val = (curr - REG03_ITERM_BASE) / REG03_ITERM_LSB;

    return upm6910_update_bits(upm, UPM6910_REG_03,
                   REG03_ITERM_MASK, reg_val);
}
EXPORT_SYMBOL_GPL(upm6910_set_term_current);

int upm6910_set_prechg_current(struct upm6910 *upm, int curr)
{
    u8 reg_val;

    if (curr > REG03_IPRECHG_MAX) {
        curr = REG03_IPRECHG_MAX;
    } else if (curr < REG03_IPRECHG_MIN) {
        curr = REG03_IPRECHG_MIN;
    }

    reg_val = (curr - REG03_IPRECHG_BASE) / REG03_IPRECHG_LSB;

    return upm6910_update_bits(upm, UPM6910_REG_03, REG03_IPRECHG_MASK,
                    reg_val << REG03_IPRECHG_SHIFT);
}
EXPORT_SYMBOL_GPL(upm6910_set_prechg_current);

int upm6910_set_chargevolt(struct upm6910 *upm, int volt)
{
    u8 val;

    if (volt < REG04_VREG_MIN) {
        volt = REG04_VREG_MIN;
    } else if (volt > REG04_VREG_MAX) {
        volt = REG04_VREG_MAX;
    }

    if (volt == REG04_VREG_SPECIAL) {
        val = 15;
    } else if (volt == 4200) {
        val = 11;
    } else {
        val = (volt - REG04_VREG_BASE) / REG04_VREG_LSB;
    }

    return upm6910_update_bits(upm, UPM6910_REG_04, REG04_VREG_MASK,
                    val << REG04_VREG_SHIFT);
}

int upm6910_set_input_volt_limit(struct upm6910 *upm, int volt)
{
    u8 val;

    if (volt < REG06_VINDPM_BASE)
        volt = REG06_VINDPM_BASE;

    val = (volt - REG06_VINDPM_BASE) / REG06_VINDPM_LSB;
    return upm6910_update_bits(upm, UPM6910_REG_06, REG06_VINDPM_MASK,
                    val << REG06_VINDPM_SHIFT);
}

int upm6910_set_input_current_limit(struct upm6910 *upm, int curr)
{
    u8 val;

    if (curr < REG00_IINLIM_BASE)
        curr = REG00_IINLIM_BASE;

    val = (curr - REG00_IINLIM_BASE) / REG00_IINLIM_LSB;

    return upm6910_update_bits(upm, UPM6910_REG_00, REG00_IINLIM_MASK,
                    val << REG00_IINLIM_SHIFT);
}

int upm6910_set_watchdog_timer(struct upm6910 *upm, u8 timeout)
{
    u8 temp;

    temp = (u8) (((timeout -
                REG05_WDT_BASE) / REG05_WDT_LSB) << REG05_WDT_SHIFT);

    return upm6910_update_bits(upm, UPM6910_REG_05, REG05_WDT_MASK, temp);
}
EXPORT_SYMBOL_GPL(upm6910_set_watchdog_timer);

int upm6910_disable_watchdog_timer(struct upm6910 *upm)
{
    u8 val = REG05_WDT_DISABLE << REG05_WDT_SHIFT;

    return upm6910_update_bits(upm, UPM6910_REG_05, REG05_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(upm6910_disable_watchdog_timer);

int upm6910_reset_watchdog_timer(struct upm6910 *upm)
{
    u8 val = REG01_WDT_RESET << REG01_WDT_RESET_SHIFT;

    return upm6910_update_bits(upm, UPM6910_REG_01, REG01_WDT_RESET_MASK,
                    val);
}
EXPORT_SYMBOL_GPL(upm6910_reset_watchdog_timer);

int upm6910_reset_chip(struct upm6910 *upm)
{
    int ret;
    u8 val = REG0B_REG_RESET << REG0B_REG_RESET_SHIFT;

    ret =
        upm6910_update_bits(upm, UPM6910_REG_0B, REG0B_REG_RESET_MASK, val);
    return ret;
}
EXPORT_SYMBOL_GPL(upm6910_reset_chip);

int upm6910_enter_hiz_mode(struct upm6910 *upm)
{
    u8 val = REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT;

    return upm6910_update_bits(upm, UPM6910_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(upm6910_enter_hiz_mode);

int upm6910_exit_hiz_mode(struct upm6910 *upm)
{

    u8 val = REG00_HIZ_DISABLE << REG00_ENHIZ_SHIFT;

    return upm6910_update_bits(upm, UPM6910_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(upm6910_exit_hiz_mode);

static int upm6910_enable_term(struct upm6910 *upm, bool enable)
{
    u8 val;
    int ret;

    if (enable)
        val = REG05_TERM_ENABLE << REG05_EN_TERM_SHIFT;
    else
        val = REG05_TERM_DISABLE << REG05_EN_TERM_SHIFT;

    ret = upm6910_update_bits(upm, UPM6910_REG_05, REG05_EN_TERM_MASK, val);

    return ret;
}
EXPORT_SYMBOL_GPL(upm6910_enable_term);

int upm6910_set_boost_current(struct upm6910 *upm, int curr)
{
    u8 val;

    val = REG02_BOOST_LIM_0P5A;
    if (curr == BOOSTI_1200)
        val = REG02_BOOST_LIM_1P2A;

    return upm6910_update_bits(upm, UPM6910_REG_02, REG02_BOOST_LIM_MASK,
                    val << REG02_BOOST_LIM_SHIFT);
}

int upm6910_set_boost_voltage(struct upm6910 *upm, int volt)
{
    u8 val;

    if (volt == BOOSTV_4850)
        val = REG06_BOOSTV_4P85V;
    else if (volt == BOOSTV_5150)
        val = REG06_BOOSTV_5P15V;
    else if (volt == BOOSTV_5300)
        val = REG06_BOOSTV_5P3V;
    else
        val = REG06_BOOSTV_5V;

    return upm6910_update_bits(upm, UPM6910_REG_06, REG06_BOOSTV_MASK,
                    val << REG06_BOOSTV_SHIFT);
}
EXPORT_SYMBOL_GPL(upm6910_set_boost_voltage);

static int upm6910_set_acovp_threshold(struct upm6910 *upm, int volt)
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

    return upm6910_update_bits(upm, UPM6910_REG_06, REG06_OVP_MASK,
                    val << REG06_OVP_SHIFT);
}
EXPORT_SYMBOL_GPL(upm6910_set_acovp_threshold);

static int upm6910_set_stat_ctrl(struct upm6910 *upm, int ctrl)
{
    u8 val;

    val = ctrl;

    return upm6910_update_bits(upm, UPM6910_REG_00, REG00_STAT_CTRL_MASK,
                    val << REG00_STAT_CTRL_SHIFT);
}

static int upm6910_set_int_mask(struct upm6910 *upm, int mask)
{
    u8 val;

    val = mask;

    return upm6910_update_bits(upm, UPM6910_REG_0A, REG0A_INT_MASK_MASK,
                    val << REG0A_INT_MASK_SHIFT);
}

static int upm6910_enable_batfet(struct upm6910 *upm)
{
    const u8 val = REG07_BATFET_ON << REG07_BATFET_DIS_SHIFT;

    return upm6910_update_bits(upm, UPM6910_REG_07, REG07_BATFET_DIS_MASK,
                    val);
}
EXPORT_SYMBOL_GPL(upm6910_enable_batfet);

static int upm6910_disable_batfet(struct upm6910 *upm)
{
    const u8 val = REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT;

    return upm6910_update_bits(upm, UPM6910_REG_07, REG07_BATFET_DIS_MASK,
                    val);
}
EXPORT_SYMBOL_GPL(upm6910_disable_batfet);

static int upm6910_set_batfet_delay(struct upm6910 *upm, uint8_t delay)
{
    u8 val;

    if (delay == 0)
        val = REG07_BATFET_DLY_0S;
    else
        val = REG07_BATFET_DLY_10S;

    val <<= REG07_BATFET_DLY_SHIFT;

    return upm6910_update_bits(upm, UPM6910_REG_07, REG07_BATFET_DLY_MASK,
                    val);
}
EXPORT_SYMBOL_GPL(upm6910_set_batfet_delay);

static int upm6910_enable_safety_timer(struct upm6910 *upm)
{
    const u8 val = REG05_CHG_TIMER_ENABLE << REG05_EN_TIMER_SHIFT;

    return upm6910_update_bits(upm, UPM6910_REG_05, REG05_EN_TIMER_MASK,
                    val);
}
EXPORT_SYMBOL_GPL(upm6910_enable_safety_timer);

static int upm6910_disable_safety_timer(struct upm6910 *upm)
{
    const u8 val = REG05_CHG_TIMER_DISABLE << REG05_EN_TIMER_SHIFT;

    return upm6910_update_bits(upm, UPM6910_REG_05, REG05_EN_TIMER_MASK,
                    val);
}
EXPORT_SYMBOL_GPL(upm6910_disable_safety_timer);

static struct upm6910_platform_data *upm6910_parse_dt(struct device_node *np,
                                struct upm6910 *upm)
{
    int ret;
    struct upm6910_platform_data *pdata;

    pdata = devm_kzalloc(upm->dev, sizeof(struct upm6910_platform_data),
                 GFP_KERNEL);
    if (!pdata)
        return NULL;

    if (of_property_read_string(np, "charger_name", &upm->chg_dev_name) < 0) {
        upm->chg_dev_name = "primary_chg";
        pr_warn("no charger name\n");
    }

    if (of_property_read_string(np, "eint_name", &upm->eint_name) < 0) {
        upm->eint_name = "chr_stat";
        pr_warn("no eint name\n");
    }

    upm->chg_det_enable =
        of_property_read_bool(np, "upm6910,charge-detect-enable");

    ret = of_property_read_u32(np, "upm6910,usb-vlim", &pdata->usb.vlim);
    if (ret) {
        pdata->usb.vlim = 4500;
        pr_err("Failed to read node of upm6910,usb-vlim\n");
    }

    ret = of_property_read_u32(np, "upm6910,usb-ilim", &pdata->usb.ilim);
    if (ret) {
        pdata->usb.ilim = 2000;
        pr_err("Failed to read node of upm6910,usb-ilim\n");
    }

    ret = of_property_read_u32(np, "upm6910,usb-vreg", &pdata->usb.vreg);
    if (ret) {
        pdata->usb.vreg = 4200;
        pr_err("Failed to read node of upm6910,usb-vreg\n");
    }

    ret = of_property_read_u32(np, "upm6910,usb-ichg", &pdata->usb.ichg);
    if (ret) {
        pdata->usb.ichg = 2000;
        pr_err("Failed to read node of upm6910,usb-ichg\n");
    }

    ret = of_property_read_u32(np, "upm6910,stat-pin-ctrl",
                    &pdata->statctrl);
    if (ret) {
        pdata->statctrl = 0;
        pr_err("Failed to read node of upm6910,stat-pin-ctrl\n");
    }

    ret = of_property_read_u32(np, "upm6910,precharge-current",
                    &pdata->iprechg);
    if (ret) {
        pdata->iprechg = 180;
        pr_err("Failed to read node of upm6910,precharge-current\n");
    }

    ret = of_property_read_u32(np, "upm6910,termination-current",
                    &pdata->iterm);
    if (ret) {
        pdata->iterm = 180;
        pr_err
            ("Failed to read node of upm6910,termination-current\n");
    }

    ret =
        of_property_read_u32(np, "upm6910,boost-voltage",
                 &pdata->boostv);
    if (ret) {
        pdata->boostv = 5000;
        pr_err("Failed to read node of upm6910,boost-voltage\n");
    }

    ret =
        of_property_read_u32(np, "upm6910,boost-current",
                 &pdata->boosti);
    if (ret) {
        pdata->boosti = 1200;
        pr_err("Failed to read node of upm6910,boost-current\n");
    }

    ret = of_property_read_u32(np, "upm6910,vac-ovp-threshold",
                    &pdata->vac_ovp);
    if (ret) {
        pdata->vac_ovp = 6500;
        pr_err("Failed to read node of upm6910,vac-ovp-threshold\n");
    }

    return pdata;
}

static int upm6910_get_charger_type(struct upm6910 *upm, int *type)
{
    int ret;

    u8 reg_val = 0;
    int vbus_stat = 0;
    int chg_type = POWER_SUPPLY_TYPE_UNKNOWN;

    ret = upm6910_read_byte(upm, UPM6910_REG_08, &reg_val);

    if (ret)
        return ret;

    vbus_stat = (reg_val & REG08_VBUS_STAT_MASK);
    vbus_stat >>= REG08_VBUS_STAT_SHIFT;

    switch (vbus_stat) {

    case REG08_VBUS_TYPE_NONE:
        chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
        break;
    case REG08_VBUS_TYPE_SDP:
        chg_type = POWER_SUPPLY_TYPE_USB;
        break;
    case REG08_VBUS_TYPE_DCP:
        chg_type = POWER_SUPPLY_TYPE_USB_DCP;
        break;
    default:
        chg_type = POWER_SUPPLY_TYPE_USB_FLOAT;
        break;
    }

    *type = chg_type;

    return 0;
}

static int upm6910_get_chrg_stat(struct upm6910 *upm,
    int *chg_stat)
{
    int ret;
    u8 val;

    ret = upm6910_read_byte(upm, UPM6910_REG_08, &val);
    if (!ret) {
        val = val & REG08_CHRG_STAT_MASK;
        val = val >> REG08_CHRG_STAT_SHIFT;
        *chg_stat = val;
    }

    return ret;
}

static int upm6910_inform_charger_type(struct upm6910 *upm)
{
    int ret = 0;
    union power_supply_propval propval;

    if (!upm->psy) {
        upm->psy = power_supply_get_by_name("charger");
        if (!upm->psy) {
            pr_err("%s Couldn't get psy\n", __func__);
            return -ENODEV;
        }
    }

    if (upm->psy_usb_type != POWER_SUPPLY_TYPE_UNKNOWN)
        propval.intval = 1;
    else
        propval.intval = 0;

    ret = power_supply_set_property(upm->psy, POWER_SUPPLY_PROP_ONLINE,
                    &propval);

    if (ret < 0)
        pr_notice("inform power supply online failed:%d\n", ret);

    if (ret < 0)
        pr_notice("inform power supply charge type failed:%d\n", ret);

    return ret;
}

static irqreturn_t upm6910_irq_handler(int irq, void *data)
{
    int ret;
    u8 reg_val;
    bool prev_pg;
    int prev_chg_type;
    struct upm6910 *upm = (struct upm6910 *)data;

    ret = upm6910_read_byte(upm, UPM6910_REG_08, &reg_val);
    if (ret)
        return IRQ_HANDLED;

    prev_pg = upm->power_good;

    upm->power_good = !!(reg_val & REG08_PG_STAT_MASK);

    if (!prev_pg && upm->power_good)
        pr_notice("adapter/usb inserted\n");
    else if (prev_pg && !upm->power_good)
        pr_notice("adapter/usb removed\n");

    prev_chg_type = upm->psy_usb_type;

    ret = upm6910_get_charger_type(upm, &upm->psy_usb_type);
    if (!ret && prev_chg_type != upm->psy_usb_type && upm->chg_det_enable)
        upm6910_inform_charger_type(upm);

    return IRQ_HANDLED;
}

static int upm6910_register_interrupt(struct upm6910 *upm)
{
    int ret = 0;

    if (! upm->client->irq) {
        pr_info("upm->client->irq is NULL\n");//remember to config dws
        return -ENODEV;
    }

    ret = devm_request_threaded_irq(upm->dev, upm->client->irq, NULL,
                    upm6910_irq_handler,
                    IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                    "ti_irq", upm);
    if (ret < 0) {
        pr_err("request thread irq failed:%d\n", ret);
        return ret;
    }

    enable_irq_wake(upm->irq);

    return 0;
}

/*hs04 code for DEVAL6398A-14 by shixuanxuan at 20220709 start*/
#ifdef CONFIG_HQ_PROJECT_HS04
static int upm6910_set_shipmode(struct charger_device *chg_dev, bool en)
{
    int ret;
    u8 val;
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

    if (en) {
        val = REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT;
        ret = upm6910_update_bits(upm, UPM6910_REG_07, REG07_BATFET_DIS_MASK, val);
    } else {
        val = REG07_BATFET_ON << REG07_BATFET_DIS_SHIFT;
        ret = upm6910_update_bits(upm, UPM6910_REG_07, REG07_BATFET_DIS_MASK, val);
    }

    pr_err("%s shipmode %s\n", en ? "enable" : "disable",
            !ret ? "successfully" : "failed");

    return ret;
}

static int upm6910_get_shipmode(struct charger_device *chg_dev)
{
    int ret;
    u8 val;
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

    msleep(100);
    ret = upm6910_read_byte(upm, UPM6910_REG_07, &val);
    if (ret == 0){
        pr_err("Reg[%.2x] = 0x%.2x\n", UPM6910_REG_07, val);
    } else {
        pr_err("%s: get shipmode reg fail! \n",__func__);
    }

    ret = (val & REG07_BATFET_DIS_MASK) >> REG07_BATFET_DIS_SHIFT;
    pr_err("%s:shipmode %s\n",__func__, ret ? "enabled" : "disabled");

    return ret;
}

static int upm6910_disable_battfet_rst(struct upm6910 *upm)
{
    int ret;
    u8 val;

    val = REG07_BATFET_RST_DISABLE << REG07_BATFET_RST_EN_SHIFT;
    ret = upm6910_update_bits(upm, UPM6910_REG_07, REG07_BATFET_RST_EN_MASK, val);

    pr_err("disable BATTFET_RST_EN %s\n", !ret ? "successfully" : "failed");

    return ret;
}
#endif

static int upm6910_init_device(struct upm6910 *upm)
{
    int ret;

    upm6910_disable_watchdog_timer(upm);

    ret = upm6910_set_stat_ctrl(upm, upm->platform_data->statctrl);
    if (ret)
        pr_err("Failed to set stat pin control mode, ret = %d\n", ret);

    ret = upm6910_set_prechg_current(upm, upm->platform_data->iprechg);
    if (ret)
        pr_err("Failed to set prechg current, ret = %d\n", ret);

    ret = upm6910_set_term_current(upm, upm->platform_data->iterm);
    if (ret)
        pr_err("Failed to set termination current, ret = %d\n", ret);

    ret = upm6910_set_boost_voltage(upm, upm->platform_data->boostv);
    if (ret)
        pr_err("Failed to set boost voltage, ret = %d\n", ret);

    ret = upm6910_set_boost_current(upm, upm->platform_data->boosti);
    if (ret)
        pr_err("Failed to set boost current, ret = %d\n", ret);

    ret = upm6910_set_acovp_threshold(upm, upm->platform_data->vac_ovp);
    if (ret)
        pr_err("Failed to set acovp threshold, ret = %d\n", ret);

    ret= upm6910_disable_safety_timer(upm);
    if (ret)
        pr_err("Failed to set safety_timer stop, ret = %d\n", ret);

    ret = upm6910_set_int_mask(upm,
                    REG0A_IINDPM_INT_MASK |
                    REG0A_VINDPM_INT_MASK);
    if (ret)
        pr_err("Failed to set vindpm and iindpm int mask\n");
    /*hs04 code for DEVAL6398A-14 by shixuanxuan at 20220709 start*/
#ifdef CONFIG_HQ_PROJECT_HS04
    ret = upm6910_disable_battfet_rst(upm);
    if (ret)
        pr_err("Failed to disable_battfet\n");
#endif
    /*hs04 code for DEVAL6398A-14 by shixuanxuan at 20220709 end*/

    return 0;
}

static void determine_initial_status(struct upm6910 *upm)
{
    upm6910_irq_handler(upm->irq, (void *) upm);
}

static int upm6910_detect_device(struct upm6910 *upm)
{
    int ret;
    u8 data;

    ret = upm6910_read_byte(upm, UPM6910_REG_0B, &data);
    if (!ret) {
        upm->part_no = (data & REG0B_PN_MASK) >> REG0B_PN_SHIFT;
    }

    return ret;
}

static void upm6910_dump_regs(struct upm6910 *upm)
{
    int addr;
    u8 val;
    int ret;

    for (addr = 0x0; addr <= 0x0B; addr++) {
        ret = upm6910_read_byte(upm, addr, &val);
        if (ret == 0)
            pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
    }
}

static ssize_t
upm6910_show_registers(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    struct upm6910 *upm = dev_get_drvdata(dev);
    u8 addr;
    u8 val;
    u8 tmpbuf[200];
    int len;
    int idx = 0;
    int ret;

    idx = snprintf(buf, PAGE_SIZE, "%s:\n", "upm6910 Reg");
    for (addr = 0x0; addr <= 0x0B; addr++) {
        ret = upm6910_read_byte(upm, addr, &val);
        if (ret == 0) {
            len = snprintf(tmpbuf, PAGE_SIZE - idx,
                        "Reg[%.2x] = 0x%.2x\n", addr, val);
            memcpy(&buf[idx], tmpbuf, len);
            idx += len;
        }
    }

    return idx;
}

static ssize_t
upm6910_store_registers(struct device *dev,
            struct device_attribute *attr, const char *buf,
            size_t count)
{
    struct upm6910 *upm = dev_get_drvdata(dev);
    int ret;
    unsigned int reg;
    unsigned int val;

    ret = sscanf(buf, "%x %x", &reg, &val);
    if (ret == 2 && reg < 0x0B) {
        upm6910_write_byte(upm, (unsigned char) reg,
                    (unsigned char) val);
    }

    return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, upm6910_show_registers,
            upm6910_store_registers);

static struct attribute *upm6910_attributes[] = {
    &dev_attr_registers.attr,
    NULL,
};

static const struct attribute_group upm6910_attr_group = {
    .attrs = upm6910_attributes,
};

static int upm6910_charging(struct charger_device *chg_dev, bool enable)
{

    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
    int ret = 0;
    u8 val;

    if (enable)
        ret = upm6910_enable_charger(upm);
    else
        ret = upm6910_disable_charger(upm);

    pr_err("%s charger %s\n", enable ? "enable" : "disable",
            !ret ? "successfully" : "failed");

    ret = upm6910_read_byte(upm, UPM6910_REG_01, &val);

    if (!ret)
        upm->charge_enabled = !!(val & REG01_CHG_CONFIG_MASK);

    return ret;
}

static int upm6910_plug_in(struct charger_device *chg_dev)
{

    int ret;

    ret = upm6910_charging(chg_dev, true);

    if (ret)
        pr_err("Failed to enable charging:%d\n", ret);

    return ret;
}

/*hs04 code for AL6398ADEU-249 by shanxinkai at 2023/04/12 start*/
static int upm6910_plug_out(struct charger_device *chg_dev)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
    int ret;

    ret = upm6910_charging(chg_dev, false);

    upm6910_chargecurrent_recheck(upm);

    if (ret)
        pr_err("Failed to disable charging:%d\n", ret);

    return ret;
}
/*hs04 code for AL6398ADEU-249 by shanxinkai at 2023/04/12 end*/

static int upm6910_dump_register(struct charger_device *chg_dev)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

    upm6910_dump_regs(upm);

    return 0;
}

static int upm6910_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

    *en = upm->charge_enabled;

    return 0;
}

static int upm6910_get_charging_status(struct charger_device *chg_dev,
    int *chg_stat)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
    int ret;
    u8 val;

    msleep(50); //modify for charging UI
    ret = upm6910_read_byte(upm, UPM6910_REG_08, &val);
    if (!ret) {
        val = val & REG08_CHRG_STAT_MASK;
        val = val >> REG08_CHRG_STAT_SHIFT;
        *chg_stat = val;
    }

    return ret;
}

static int upm6910_is_charging_done(struct charger_device *chg_dev, bool *done)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
    int ret;
    u8 val;

    ret = upm6910_read_byte(upm, UPM6910_REG_08, &val);
    if (!ret) {
        val = val & REG08_CHRG_STAT_MASK;
        val = val >> REG08_CHRG_STAT_SHIFT;
        *done = (val == REG08_CHRG_STAT_CHGDONE);
    }

    return ret;
}

static int upm6910_set_ichg(struct charger_device *chg_dev, u32 curr)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
    /*hs04 code for AL6398ADEU-249 by shanxinkai at 2023/04/12 start*/
    int ret = 0;
    u8 reg_val = 0;
    bool power_good = false;

    ret = upm6910_read_byte(upm, UPM6910_REG_08, &reg_val);

    if (!ret) {
        power_good = !!(reg_val & REG08_PG_STAT_MASK);
        if (!power_good) {
            curr = ICHG_OCP_THRESHOLD_MA * 1000;
        }
    }

    /*hs04 code for AL6398ADEU-249 by shanxinkai at 2023/04/12 end*/
    pr_err("charge curr = %d\n", curr);

    return upm6910_set_chargecurrent(upm, curr / 1000);
}

static int upm6910_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
    u8 reg_val;
    int ichg;
    int ret;

    ret = upm6910_read_byte(upm, UPM6910_REG_02, &reg_val);
    if (!ret) {
        ichg = (reg_val & REG02_ICHG_MASK) >> REG02_ICHG_SHIFT;
        ichg = ichg * REG02_ICHG_LSB;
        *curr = ichg * 1000;
    }

    return ret;
}

static int upm6910_set_iterm(struct charger_device *chg_dev, u32 uA)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

    pr_err("termination curr = %d\n", uA);

    return upm6910_set_term_current(upm, uA / 1000);
}

static int upm6910_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
    *curr = 60 * 1000;

    return 0;
}

static int upm6910_get_min_aicr(struct charger_device *chg_dev, u32 *uA)
{
    *uA = 100 * 1000;
    return 0;
}

static int upm6910_set_vchg(struct charger_device *chg_dev, u32 volt)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

    pr_err("charge volt = %d\n", volt);

    return upm6910_set_chargevolt(upm, volt / 1000);
}

static int upm6910_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
    u8 reg_val;
    int vchg;
    int ret;

    ret = upm6910_read_byte(upm, UPM6910_REG_04, &reg_val);
    if (!ret) {
        vchg = (reg_val & REG04_VREG_MASK) >> REG04_VREG_SHIFT;
        if ( vchg == 15) {
            vchg = REG04_VREG_SPECIAL;
        } else {
            vchg = vchg * REG04_VREG_LSB + REG04_VREG_BASE;
        }

        *volt = vchg * 1000;
    }

    return ret;
}

static int upm6910_get_ivl_state(struct charger_device *chg_dev, bool *in_loop)
{
    int ret = 0;
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
    u8 reg_val;

    ret = upm6910_read_byte(upm, UPM6910_REG_0A, &reg_val);
    if (!ret)
        *in_loop = (ret & REG0A_VINDPM_STAT_MASK) >> REG0A_VINDPM_STAT_SHIFT;

    return ret;
}

static int upm6910_get_ivl(struct charger_device *chg_dev, u32 *volt)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
    u8 reg_val;
    int ivl;
    int ret;

    ret = upm6910_read_byte(upm, UPM6910_REG_06, &reg_val);
    if (!ret) {
        ivl = (reg_val & REG06_VINDPM_MASK) >> REG06_VINDPM_SHIFT;
        ivl = ivl * REG06_VINDPM_LSB + REG06_VINDPM_BASE;
        *volt = ivl * 1000;
    }

    return ret;
}

static int upm6910_set_ivl(struct charger_device *chg_dev, u32 volt)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

    pr_err("vindpm volt = %d\n", volt);

    return upm6910_set_input_volt_limit(upm, volt / 1000);

}

static int upm6910_set_icl(struct charger_device *chg_dev, u32 curr)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

    pr_err("indpm curr = %d\n", curr);

    return upm6910_set_input_current_limit(upm, curr / 1000);
}

static int upm6910_get_icl(struct charger_device *chg_dev, u32 *curr)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
    u8 reg_val;
    int icl;
    int ret;

    ret = upm6910_read_byte(upm, UPM6910_REG_00, &reg_val);
    if (!ret) {
        icl = (reg_val & REG00_IINLIM_MASK) >> REG00_IINLIM_SHIFT;
        icl = icl * REG00_IINLIM_LSB + REG00_IINLIM_BASE;
        *curr = icl * 1000;
    }

    return ret;

}

static int upm6910_enable_te(struct charger_device *chg_dev, bool en)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

    pr_err("enable_term = %d\n", en);

    return upm6910_enable_term(upm, en);
}

static int upm6910_kick_wdt(struct charger_device *chg_dev)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

    return upm6910_reset_watchdog_timer(upm);
}

static int upm6910_set_otg(struct charger_device *chg_dev, bool en)
{
    int ret;
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

    if (en)
        ret = upm6910_enable_otg(upm);
    else
        ret = upm6910_disable_otg(upm);

    pr_err("%s OTG %s\n", en ? "enable" : "disable",
            !ret ? "successfully" : "failed");

    return ret;
}

static int upm6910_set_safety_timer(struct charger_device *chg_dev, bool en)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
    int ret;

    if (en)
        ret = upm6910_enable_safety_timer(upm);
    else
        ret = upm6910_disable_safety_timer(upm);

    return ret;
}

static int upm6910_is_safety_timer_enabled(struct charger_device *chg_dev,
                        bool *en)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
    int ret;
    u8 reg_val;

    ret = upm6910_read_byte(upm, UPM6910_REG_05, &reg_val);

    if (!ret)
        *en = !!(reg_val & REG05_EN_TIMER_MASK);

    return ret;
}

static int upm6910_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

    if (!upm->psy) {
        dev_notice(upm->dev, "%s: cannot get psy\n", __func__);
        return -ENODEV;
    }

    switch (event) {
    case EVENT_FULL:
    case EVENT_RECHARGE:
    case EVENT_DISCHARGE:
        power_supply_changed(upm->psy);
        break;
    default:
        break;
    }
    return 0;
}

static int upm6910_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
    int ret;

    pr_err("otg curr = %d\n", curr);

    ret = upm6910_set_boost_current(upm, curr / 1000);

    return ret;
}

static int upm6910_set_hiz_mode(struct charger_device *chg_dev, bool en)
{
    int ret;
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

    if (en)
        ret = upm6910_enter_hiz_mode(upm);
    else
        ret = upm6910_exit_hiz_mode(upm);

    pr_err("%s hiz mode %s\n", en ? "enable" : "disable",
            !ret ? "successfully" : "failed");

    return ret;
}

static int upm6910_get_hiz_mode(struct charger_device *chg_dev)
{
    int ret;
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
    u8 val;
    ret = upm6910_read_byte(upm, UPM6910_REG_00, &val);
    if (ret == 0){
        pr_err("Reg[%.2x] = 0x%.2x\n", UPM6910_REG_00, val);
    }

    ret = (val & REG00_ENHIZ_MASK) >> REG00_ENHIZ_SHIFT;
    pr_err("%s:hiz mode %s\n",__func__, ret ? "enabled" : "disabled");

    return ret;
}

static struct charger_ops upm6910_chg_ops = {
    /* Normal charging */
    .plug_in = upm6910_plug_in,
    .plug_out = upm6910_plug_out,
    .dump_registers = upm6910_dump_register,
    .enable = upm6910_charging,
    .is_enabled = upm6910_is_charging_enable,
    .get_charging_current = upm6910_get_ichg,
    .set_charging_current = upm6910_set_ichg,
    .get_input_current = upm6910_get_icl,
    .set_input_current = upm6910_set_icl,
    .get_constant_voltage = upm6910_get_vchg,
    .set_constant_voltage = upm6910_set_vchg,
    .kick_wdt = upm6910_kick_wdt,
    .set_mivr = upm6910_set_ivl,
    .get_mivr = upm6910_get_ivl,
    .get_mivr_state = upm6910_get_ivl_state,
    .is_charging_done = upm6910_is_charging_done,
    .set_eoc_current = upm6910_set_iterm,
    .enable_termination = upm6910_enable_te,
    .reset_eoc_state = NULL,
    .get_min_charging_current = upm6910_get_min_ichg,
    .get_min_input_current = upm6910_get_min_aicr,

    /* Safety timer */
    .enable_safety_timer = upm6910_set_safety_timer,
    .is_safety_timer_enabled = upm6910_is_safety_timer_enabled,

    /* Power path */
    .enable_powerpath = NULL,
    .is_powerpath_enabled = NULL,

    /* OTG */
    .enable_otg = upm6910_set_otg,
    .set_boost_current_limit = upm6910_set_boost_ilmt,
    .enable_discharge = NULL,

    /* PE+/PE+20 */
    .send_ta_current_pattern = NULL,
    .set_pe20_efficiency_table = NULL,
    .send_ta20_current_pattern = NULL,
    .enable_cable_drop_comp = NULL,

    /* ADC */
    .get_tchg_adc = NULL,
    .get_ibus_adc = NULL,

    /* Event */
    .event = upm6910_do_event,

    .get_chr_status = upm6910_get_charging_status,
    .set_hiz_mode = upm6910_set_hiz_mode,
    .get_hiz_mode = upm6910_get_hiz_mode,
    /*hs04 code for DEVAL6398A-14 by shixuanxuan at 20220709 start*/
#ifdef CONFIG_HQ_PROJECT_HS04
    .get_ship_mode = upm6910_get_shipmode,
    .set_ship_mode = upm6910_set_shipmode,
#endif
    /*hs04 code for DEVAL6398A-14 by shixuanxuan at 20220709 end*/
};

static struct of_device_id upm6910_charger_match_table[] = {
    {
     .compatible = "upm6910",
     },
    {},
};
MODULE_DEVICE_TABLE(of, upm6910_charger_match_table);

/* ======================= */
/* charger ic Power Supply Ops */
/* ======================= */

enum CHG_STATUS {
    CHG_STATUS_NOT_CHARGING = 0,
    CHG_STATUS_PRE_CHARGE,
    CHG_STATUS_FAST_CHARGING,
    CHG_STATUS_DONE,
};

static int charger_ic_get_online(struct upm6910 *upm,
                     bool *val)
{
    bool pwr_rdy = false;

    upm6910_get_charger_type(upm, &upm->psy_usb_type);
    if(upm->psy_usb_type != POWER_SUPPLY_TYPE_UNKNOWN)
        pwr_rdy = false;
    else
        pwr_rdy = true;

    dev_info(upm->dev, "%s: online = %d\n", __func__, pwr_rdy);
    *val = pwr_rdy;
    return 0;
}

static int charger_ic_get_property(struct power_supply *psy,
                       enum power_supply_property psp,
                       union power_supply_propval *val)
{
    struct upm6910 *upm = power_supply_get_drvdata(psy);
    bool pwr_rdy = false;
    int ret = 0;
    int chr_status = 0;

    dev_dbg(upm->dev, "%s: prop = %d\n", __func__, psp);
    switch (psp) {
    case POWER_SUPPLY_PROP_ONLINE:
        ret = charger_ic_get_online(upm, &pwr_rdy);
        val->intval = pwr_rdy;
        break;
    case POWER_SUPPLY_PROP_STATUS:
        if (upm->psy_usb_type == POWER_SUPPLY_USB_TYPE_UNKNOWN)
        {
            val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
            break;
        }
        ret = upm6910_get_chrg_stat(upm, &chr_status);
        if (ret < 0) {
            val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
            break;
        }
        switch(chr_status) {
        case CHG_STATUS_NOT_CHARGING:
            val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
            break;
        case CHG_STATUS_PRE_CHARGE:
        case CHG_STATUS_FAST_CHARGING:
            if(upm->charge_enabled)
                val->intval = POWER_SUPPLY_STATUS_CHARGING;
            else
                val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
            break;
        case CHG_STATUS_DONE:
            val->intval = POWER_SUPPLY_STATUS_FULL;
            break;
        default:
            ret = -ENODATA;
            break;
        }
        break;
    default:
        ret = -ENODATA;
    }
    return ret;
}

static enum power_supply_property charger_ic_properties[] = {
    POWER_SUPPLY_PROP_ONLINE,
    POWER_SUPPLY_PROP_STATUS,
};

static const struct power_supply_desc charger_ic_desc = {
    .properties        = charger_ic_properties,
    .num_properties        = ARRAY_SIZE(charger_ic_properties),
    .get_property        = charger_ic_get_property,
};

static char *charger_ic_supplied_to[] = {
    "battery",
    "mtk-master-charger"
};

static int upm6910_charger_remove(struct i2c_client *client);
static int upm6910_charger_probe(struct i2c_client *client,
                 const struct i2c_device_id *id)
{
    struct upm6910 *upm;
    const struct of_device_id *match;
    struct device_node *node = client->dev.of_node;
    struct power_supply_config charger_cfg = {};

    int ret = 0;

    upm = devm_kzalloc(&client->dev, sizeof(struct upm6910), GFP_KERNEL);
    if (!upm)
        return -ENOMEM;

    client->addr = 0x6B;
    upm->dev = &client->dev;
    upm->client = client;

    i2c_set_clientdata(client, upm);

    mutex_init(&upm->i2c_rw_lock);

    ret = upm6910_detect_device(upm);
    if (ret) {
        pr_err("No upm6910 device found!\n");
        return -ENODEV;
    }

    match = of_match_node(upm6910_charger_match_table, node);
    if (match == NULL) {
        pr_err("device tree match not found\n");
        return -EINVAL;
    }

    if (upm->part_no == UPM6910_PN_VAL)
    {
        pr_info("UPM6910 part number match success \n");
    } else {
        pr_info("UPM6910 part number match fial \n");
        upm6910_charger_remove(client);
        return -EINVAL;
    }

    upm->platform_data = upm6910_parse_dt(node, upm);

    if (!upm->platform_data) {
        pr_err("No platform data provided.\n");
        return -EINVAL;
    }

    ret = upm6910_init_device(upm);
    if (ret) {
        pr_err("Failed to init device\n");
        return ret;
    }

    upm6910_register_interrupt(upm);

    upm->chg_dev = charger_device_register(upm->chg_dev_name,
                            &client->dev, upm,
                            &upm6910_chg_ops,
                            &upm6910_chg_props);
    if (IS_ERR_OR_NULL(upm->chg_dev)) {
        ret = PTR_ERR(upm->chg_dev);
        return ret;
    }

    ret = sysfs_create_group(&upm->dev->kobj, &upm6910_attr_group);
    if (ret)
        dev_err(upm->dev, "failed to register sysfs. err: %d\n", ret);

    determine_initial_status(upm);

    /* power supply register */
    memcpy(&upm->psy_desc,
        &charger_ic_desc, sizeof(upm->psy_desc));
    upm->psy_desc.name = dev_name(&client->dev);

    charger_cfg.drv_data = upm;
    charger_cfg.of_node = upm->dev->of_node;
    charger_cfg.supplied_to = charger_ic_supplied_to;
    charger_cfg.num_supplicants = ARRAY_SIZE(charger_ic_supplied_to);
    upm->psy = devm_power_supply_register(&client->dev,
                    &upm->psy_desc, &charger_cfg);
    if (IS_ERR(upm->psy)) {
        dev_notice(&client->dev, "Fail to register power supply dev\n");
        ret = PTR_ERR(upm->psy);
    }

    /*hs04 code for AL6398ADEU-249 by shanxinkai at 2023/04/12 start*/
    ret = upm6910_set_chargecurrent(upm, ICHG_OCP_THRESHOLD_MA);
    if (ret) {
        pr_err("set charge current failed!\n");
        return -ENODEV;
    }
    /*hs04 code for AL6398ADEU-249 by shanxinkai at 2023/04/12 end*/

	/*HS03s for SR-AL5628-01-162 by zhangjiangbin at 20220915 start*/
	set_hardinfo_charger_data(CHG_INFO, "UPM6910");
	/*HS03s for SR-AL5628-01-162 by zhangjiangbin at 20220915 end*/

    pr_err("upm6910 probe successfully, Part Num:%d\n!",
            upm->part_no);

    return 0;
}

static int upm6910_charger_remove(struct i2c_client *client)
{
    struct upm6910 *upm = i2c_get_clientdata(client);

    mutex_destroy(&upm->i2c_rw_lock);

    sysfs_remove_group(&upm->dev->kobj, &upm6910_attr_group);

    return 0;
}

static void upm6910_charger_shutdown(struct i2c_client *client)
{

}

static struct i2c_driver upm6910_charger_driver = {
    .driver = {
            .name = "upm6910-charger",
            .owner = THIS_MODULE,
            .of_match_table = upm6910_charger_match_table,
            },

    .probe = upm6910_charger_probe,
    .remove = upm6910_charger_remove,
    .shutdown = upm6910_charger_shutdown,

};

module_i2c_driver(upm6910_charger_driver);

MODULE_DESCRIPTION("UPM6910 Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SGM");
