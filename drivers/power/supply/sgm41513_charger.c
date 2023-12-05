/*
 * SGM41513 battery charging driver
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

#define pr_fmt(fmt)    "[sgm41513]:%s: " fmt, __func__

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

#include "sgm41513_reg.h"
#include "sgm41513.h"
/*HS03s for SR-AL5628-01-162 by zhangjiangbin at 20220915 start*/
#include <linux/hardinfo_charger.h>
/*HS03s for SR-AL5628-01-162 by zhangjiangbin at 20220915 end*/

struct sgm41513 {
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

    struct sgm41513_platform_data *platform_data;
    struct charger_device *chg_dev;

    struct power_supply *psy;
};

static const unsigned int IPRECHG_CURRENT_STABLE[] = {
    5000, 10000, 15000, 20000, 30000, 40000, 50000, 60000,
    80000, 100000, 120000, 140000, 160000, 180000, 200000, 240000
};

static const unsigned int ITERM_CURRENT_STABLE[] = {
    5000, 10000, 15000, 20000, 30000, 40000, 50000, 60000,
    80000, 100000, 120000, 140000, 160000, 180000, 200000, 240000
};

static const struct charger_properties sgm41513_chg_props = {
    .alias_name = "sgm41513",
};

static int __sgm41513_read_reg(struct sgm41513 *sgm, u8 reg, u8 *data)
{
    s32 ret;

    ret = i2c_smbus_read_byte_data(sgm->client, reg);
    if (ret < 0) {
        pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
        return ret;
    }

    *data = (u8) ret;

    return 0;
}

static int __sgm41513_write_reg(struct sgm41513 *sgm, int reg, u8 val)
{
    s32 ret;

    ret = i2c_smbus_write_byte_data(sgm->client, reg, val);
    if (ret < 0) {
        pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
                val, reg, ret);
        return ret;
    }
    return 0;
}

static int sgm41513_read_byte(struct sgm41513 *sgm, u8 reg, u8 *data)
{
    int ret;

    mutex_lock(&sgm->i2c_rw_lock);
    ret = __sgm41513_read_reg(sgm, reg, data);
    mutex_unlock(&sgm->i2c_rw_lock);

    return ret;
}

static int sgm41513_write_byte(struct sgm41513 *sgm, u8 reg, u8 data)
{
    int ret;

    mutex_lock(&sgm->i2c_rw_lock);
    ret = __sgm41513_write_reg(sgm, reg, data);
    mutex_unlock(&sgm->i2c_rw_lock);

    if (ret)
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

    return ret;
}

static int sgm41513_update_bits(struct sgm41513 *sgm, u8 reg, u8 mask, u8 data)
{
    int ret;
    u8 tmp;

    mutex_lock(&sgm->i2c_rw_lock);
    ret = __sgm41513_read_reg(sgm, reg, &tmp);
    if (ret) {
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
        goto out;
    }

    tmp &= ~mask;
    tmp |= data & mask;

    ret = __sgm41513_write_reg(sgm, reg, tmp);
    if (ret)
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
    mutex_unlock(&sgm->i2c_rw_lock);
    return ret;
}

static int sgm41513_enable_otg(struct sgm41513 *sgm)
{
    u8 val = REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT;

    return sgm41513_update_bits(sgm, SGM41513_REG_01, REG01_OTG_CONFIG_MASK,
                    val);

}

static int sgm41513_disable_otg(struct sgm41513 *sgm)
{
    u8 val = REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT;

    return sgm41513_update_bits(sgm, SGM41513_REG_01, REG01_OTG_CONFIG_MASK,
                    val);

}

static int sgm41513_enable_charger(struct sgm41513 *sgm)
{
    int ret;
    u8 val = REG01_CHG_ENABLE << REG01_CHG_CONFIG_SHIFT;

    ret =
        sgm41513_update_bits(sgm, SGM41513_REG_01, REG01_CHG_CONFIG_MASK, val);

    return ret;
}

static int sgm41513_disable_charger(struct sgm41513 *sgm)
{
    int ret;
    u8 val = REG01_CHG_DISABLE << REG01_CHG_CONFIG_SHIFT;

    ret =
        sgm41513_update_bits(sgm, SGM41513_REG_01, REG01_CHG_CONFIG_MASK, val);
    return ret;
}

int sgm41513_set_chargecurrent(struct sgm41513 *sgm, int curr)
{
    u8 ichg;

    if (curr < 0) {
        curr = 0;
    } else if (curr > 3000) {
        curr = 3000;
    }

    if (curr < 40) {
        ichg = curr / 5;
    } else if (curr < 130) {
        ichg = 8 + (curr - 40) / 10;
    } else if (curr < 300) {
        ichg = 16 + (curr - 110) / 20;
    } else if (curr < 540) {
        ichg = 24 + (curr - 300) / 30;
    } else if (curr < 1500) {
        ichg = 32 + (curr - 540) / 60;
    } else {
        ichg = 48 + (curr - 1500) / 120;
    }

    return sgm41513_update_bits(sgm, SGM41513_REG_02, REG02_ICHG_MASK,
                    ichg << REG02_ICHG_SHIFT);

}

int sgm41513_set_term_current(struct sgm41513 *sgm, int curr)
{
    u8 reg_val;

    curr = curr*1000;
    if (curr > ITERM_CURRENT_STABLE[15])
        curr = ITERM_CURRENT_STABLE[15];

    for(reg_val = 1; reg_val < 16 && curr >= ITERM_CURRENT_STABLE[reg_val]; reg_val++)
        ;
    reg_val--;

    return sgm41513_update_bits(sgm, SGM41513_REG_03,
                   REG03_ITERM_MASK, reg_val);
}

EXPORT_SYMBOL_GPL(sgm41513_set_term_current);

int sgm41513_set_prechg_current(struct sgm41513 *sgm, int curr)
{
    u8 reg_val;

    curr = curr*1000;
    if (curr > IPRECHG_CURRENT_STABLE[15])
        curr = IPRECHG_CURRENT_STABLE[15];

    for(reg_val = 1; reg_val < 16 && curr >= IPRECHG_CURRENT_STABLE[reg_val]; reg_val++)
        ;
    reg_val--;

    return sgm41513_update_bits(sgm, SGM41513_REG_03, REG03_IPRECHG_MASK,
                    reg_val << REG03_IPRECHG_SHIFT);
}
EXPORT_SYMBOL_GPL(sgm41513_set_prechg_current);

int sgm41513_set_chargevolt(struct sgm41513 *sgm, int volt)
{
    u8 val;

    if (volt < REG04_VREG_BASE)
        volt = REG04_VREG_BASE;

    if (volt == 4352) {
        val = 15;
    /* hs03s_NM code added for DEVAL5626-680 by shixuanxuan at 20220426 start */
    } else if (volt == 4200) {
        val = (volt - REG04_VREG_BASE) / REG04_VREG_LSB + 1;
    /* hs03s_NM code added for DEVAL5626-680 by shixuanxuan at 20220426 end */
    } else {
        val = (volt - REG04_VREG_BASE) / REG04_VREG_LSB;
    }

    return sgm41513_update_bits(sgm, SGM41513_REG_04, REG04_VREG_MASK,
                    val << REG04_VREG_SHIFT);
}

int sgm41513_set_input_volt_limit(struct sgm41513 *sgm, int volt)
{
    u8 val;

    if (volt < REG06_VINDPM_BASE)
        volt = REG06_VINDPM_BASE;

    val = (volt - REG06_VINDPM_BASE) / REG06_VINDPM_LSB;
    return sgm41513_update_bits(sgm, SGM41513_REG_06, REG06_VINDPM_MASK,
                    val << REG06_VINDPM_SHIFT);
}

int sgm41513_set_input_current_limit(struct sgm41513 *sgm, int curr)
{
    u8 val;

    if (curr < REG00_IINLIM_BASE)
        curr = REG00_IINLIM_BASE;

    val = (curr - REG00_IINLIM_BASE) / REG00_IINLIM_LSB;
    msleep(500);
    return sgm41513_update_bits(sgm, SGM41513_REG_00, REG00_IINLIM_MASK,
                    val << REG00_IINLIM_SHIFT);
}

int sgm41513_set_watchdog_timer(struct sgm41513 *sgm, u8 timeout)
{
    u8 temp;

    temp = (u8) (((timeout -
                REG05_WDT_BASE) / REG05_WDT_LSB) << REG05_WDT_SHIFT);

    return sgm41513_update_bits(sgm, SGM41513_REG_05, REG05_WDT_MASK, temp);
}
EXPORT_SYMBOL_GPL(sgm41513_set_watchdog_timer);

int sgm41513_disable_watchdog_timer(struct sgm41513 *sgm)
{
    u8 val = REG05_WDT_DISABLE << REG05_WDT_SHIFT;

    return sgm41513_update_bits(sgm, SGM41513_REG_05, REG05_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(sgm41513_disable_watchdog_timer);

int sgm41513_reset_watchdog_timer(struct sgm41513 *sgm)
{
    u8 val = REG01_WDT_RESET << REG01_WDT_RESET_SHIFT;

    return sgm41513_update_bits(sgm, SGM41513_REG_01, REG01_WDT_RESET_MASK,
                    val);
}
EXPORT_SYMBOL_GPL(sgm41513_reset_watchdog_timer);

int sgm41513_reset_chip(struct sgm41513 *sgm)
{
    int ret;
    u8 val = REG0B_REG_RESET << REG0B_REG_RESET_SHIFT;

    ret =
        sgm41513_update_bits(sgm, SGM41513_REG_0B, REG0B_REG_RESET_MASK, val);
    return ret;
}
EXPORT_SYMBOL_GPL(sgm41513_reset_chip);

int sgm41513_enter_hiz_mode(struct sgm41513 *sgm)
{
    u8 val = REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT;

    return sgm41513_update_bits(sgm, SGM41513_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(sgm41513_enter_hiz_mode);

int sgm41513_exit_hiz_mode(struct sgm41513 *sgm)
{

    u8 val = REG00_HIZ_DISABLE << REG00_ENHIZ_SHIFT;

    return sgm41513_update_bits(sgm, SGM41513_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(sgm41513_exit_hiz_mode);

static int sgm41513_enable_term(struct sgm41513 *sgm, bool enable)
{
    u8 val;
    int ret;

    if (enable)
        val = REG05_TERM_ENABLE << REG05_EN_TERM_SHIFT;
    else
        val = REG05_TERM_DISABLE << REG05_EN_TERM_SHIFT;

    ret = sgm41513_update_bits(sgm, SGM41513_REG_05, REG05_EN_TERM_MASK, val);

    return ret;
}
EXPORT_SYMBOL_GPL(sgm41513_enable_term);

int sgm41513_set_boost_current(struct sgm41513 *sgm, int curr)
{
    u8 val;

    val = REG02_BOOST_LIM_0P5A;
    if (curr == BOOSTI_1200)
        val = REG02_BOOST_LIM_1P2A;

    return sgm41513_update_bits(sgm, SGM41513_REG_02, REG02_BOOST_LIM_MASK,
                    val << REG02_BOOST_LIM_SHIFT);
}

int sgm41513_set_boost_voltage(struct sgm41513 *sgm, int volt)
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

    return sgm41513_update_bits(sgm, SGM41513_REG_06, REG06_BOOSTV_MASK,
                    val << REG06_BOOSTV_SHIFT);
}
EXPORT_SYMBOL_GPL(sgm41513_set_boost_voltage);

static int sgm41513_set_acovp_threshold(struct sgm41513 *sgm, int volt)
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

    return sgm41513_update_bits(sgm, SGM41513_REG_06, REG06_OVP_MASK,
                    val << REG06_OVP_SHIFT);
}
EXPORT_SYMBOL_GPL(sgm41513_set_acovp_threshold);

static int sgm41513_set_stat_ctrl(struct sgm41513 *sgm, int ctrl)
{
    u8 val;

    val = ctrl;

    return sgm41513_update_bits(sgm, SGM41513_REG_00, REG00_STAT_CTRL_MASK,
                    val << REG00_STAT_CTRL_SHIFT);
}

static int sgm41513_set_int_mask(struct sgm41513 *sgm, int mask)
{
    u8 val;

    val = mask;

    return sgm41513_update_bits(sgm, SGM41513_REG_0A, REG0A_INT_MASK_MASK,
                    val << REG0A_INT_MASK_SHIFT);
}

static int sgm41513_enable_batfet(struct sgm41513 *sgm)
{
    const u8 val = REG07_BATFET_ON << REG07_BATFET_DIS_SHIFT;

    return sgm41513_update_bits(sgm, SGM41513_REG_07, REG07_BATFET_DIS_MASK,
                    val);
}
EXPORT_SYMBOL_GPL(sgm41513_enable_batfet);

static int sgm41513_disable_batfet(struct sgm41513 *sgm)
{
    const u8 val = REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT;

    return sgm41513_update_bits(sgm, SGM41513_REG_07, REG07_BATFET_DIS_MASK,
                    val);
}
EXPORT_SYMBOL_GPL(sgm41513_disable_batfet);

static int sgm41513_set_batfet_delay(struct sgm41513 *sgm, uint8_t delay)
{
    u8 val;

    if (delay == 0)
        val = REG07_BATFET_DLY_0S;
    else
        val = REG07_BATFET_DLY_10S;

    val <<= REG07_BATFET_DLY_SHIFT;

    return sgm41513_update_bits(sgm, SGM41513_REG_07, REG07_BATFET_DLY_MASK,
                    val);
}
EXPORT_SYMBOL_GPL(sgm41513_set_batfet_delay);

static int sgm41513_enable_safety_timer(struct sgm41513 *sgm)
{
    const u8 val = REG05_CHG_TIMER_ENABLE << REG05_EN_TIMER_SHIFT;

    return sgm41513_update_bits(sgm, SGM41513_REG_05, REG05_EN_TIMER_MASK,
                    val);
}
EXPORT_SYMBOL_GPL(sgm41513_enable_safety_timer);

static int sgm41513_disable_safety_timer(struct sgm41513 *sgm)
{
    const u8 val = REG05_CHG_TIMER_DISABLE << REG05_EN_TIMER_SHIFT;

    return sgm41513_update_bits(sgm, SGM41513_REG_05, REG05_EN_TIMER_MASK,
                    val);
}
EXPORT_SYMBOL_GPL(sgm41513_disable_safety_timer);

static struct sgm41513_platform_data *sgm41513_parse_dt(struct device_node *np,
                                struct sgm41513 *sgm)
{
    int ret;
    struct sgm41513_platform_data *pdata;

    pdata = devm_kzalloc(sgm->dev, sizeof(struct sgm41513_platform_data),
                 GFP_KERNEL);
    if (!pdata)
        return NULL;

    if (of_property_read_string(np, "charger_name", &sgm->chg_dev_name) < 0) {
        sgm->chg_dev_name = "primary_chg";
        pr_warn("no charger name\n");
    }

    if (of_property_read_string(np, "eint_name", &sgm->eint_name) < 0) {
        sgm->eint_name = "chr_stat";
        pr_warn("no eint name\n");
    }

    sgm->chg_det_enable =
        of_property_read_bool(np, "sgm41513,charge-detect-enable");

    ret = of_property_read_u32(np, "sgm41513,usb-vlim", &pdata->usb.vlim);
    if (ret) {
        pdata->usb.vlim = 4500;
        pr_err("Failed to read node of sgm41513,usb-vlim\n");
    }

    ret = of_property_read_u32(np, "sgm41513,usb-ilim", &pdata->usb.ilim);
    if (ret) {
        pdata->usb.ilim = 2000;
        pr_err("Failed to read node of sgm41513,usb-ilim\n");
    }

    ret = of_property_read_u32(np, "sgm41513,usb-vreg", &pdata->usb.vreg);
    if (ret) {
        pdata->usb.vreg = 4200;
        pr_err("Failed to read node of sgm41513,usb-vreg\n");
    }

    ret = of_property_read_u32(np, "sgm41513,usb-ichg", &pdata->usb.ichg);
    if (ret) {
        pdata->usb.ichg = 2000;
        pr_err("Failed to read node of sgm41513,usb-ichg\n");
    }

    ret = of_property_read_u32(np, "sgm41513,stat-pin-ctrl",
                    &pdata->statctrl);
    if (ret) {
        pdata->statctrl = 0;
        pr_err("Failed to read node of sgm41513,stat-pin-ctrl\n");
    }

    ret = of_property_read_u32(np, "sgm41513,precharge-current",
                    &pdata->iprechg);
    if (ret) {
        pdata->iprechg = 180;
        pr_err("Failed to read node of sgm41513,precharge-current\n");
    }

    ret = of_property_read_u32(np, "sgm41513,termination-current",
                    &pdata->iterm);
    if (ret) {
        pdata->iterm = 180;
        pr_err
            ("Failed to read node of sgm41513,termination-current\n");
    }

    ret =
        of_property_read_u32(np, "sgm41513,boost-voltage",
                 &pdata->boostv);
    if (ret) {
        pdata->boostv = 5000;
        pr_err("Failed to read node of sgm41513,boost-voltage\n");
    }

    ret =
        of_property_read_u32(np, "sgm41513,boost-current",
                 &pdata->boosti);
    if (ret) {
        pdata->boosti = 1200;
        pr_err("Failed to read node of sgm41513,boost-current\n");
    }

    ret = of_property_read_u32(np, "sgm41513,vac-ovp-threshold",
                    &pdata->vac_ovp);
    if (ret) {
        pdata->vac_ovp = 6500;
        pr_err("Failed to read node of sgm41513,vac-ovp-threshold\n");
    }

    return pdata;
}

static int sgm41513_get_charger_type(struct sgm41513 *sgm, int *type)
{
    int ret;

    u8 reg_val = 0;
    int vbus_stat = 0;
    int chg_type = POWER_SUPPLY_TYPE_UNKNOWN;

    ret = sgm41513_read_byte(sgm, SGM41513_REG_08, &reg_val);

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
    case REG08_VBUS_TYPE_CDP:
        chg_type = POWER_SUPPLY_TYPE_USB_CDP;
        break;
    case REG08_VBUS_TYPE_DCP:
        chg_type = POWER_SUPPLY_TYPE_USB_DCP;
        break;
    case REG08_VBUS_TYPE_UNKNOWN:
        chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
        break;
    case REG08_VBUS_TYPE_NON_STD:
        chg_type = POWER_SUPPLY_TYPE_USB_FLOAT;
        break;
    default:
        chg_type = POWER_SUPPLY_TYPE_USB_FLOAT;
        break;
    }

    *type = chg_type;

    return 0;
}

static int sgm41513_get_chrg_stat(struct sgm41513 *sgm,
    int *chg_stat)
{
    int ret;
    u8 val;

    ret = sgm41513_read_byte(sgm, SGM41513_REG_08, &val);
    if (!ret) {
        val = val & REG08_CHRG_STAT_MASK;
        val = val >> REG08_CHRG_STAT_SHIFT;
        *chg_stat = val;
    }

    return ret;
}

static int sgm41513_inform_charger_type(struct sgm41513 *sgm)
{
    int ret = 0;
    union power_supply_propval propval;

    if (!sgm->psy) {
        sgm->psy = power_supply_get_by_name("charger");
        if (!sgm->psy) {
            pr_err("%s Couldn't get psy\n", __func__);
            return -ENODEV;
        }
    }

    if (sgm->psy_usb_type != POWER_SUPPLY_TYPE_UNKNOWN)
        propval.intval = 1;
    else
        propval.intval = 0;

    ret = power_supply_set_property(sgm->psy, POWER_SUPPLY_PROP_ONLINE,
                    &propval);

    if (ret < 0)
        pr_notice("inform power supply online failed:%d\n", ret);

    propval.intval = sgm->psy_usb_type;

    ret = power_supply_set_property(sgm->psy,
                    POWER_SUPPLY_PROP_CHARGE_TYPE,
                    &propval);

    if (ret < 0)
        pr_notice("inform power supply charge type failed:%d\n", ret);

    return ret;
}

static irqreturn_t sgm41513_irq_handler(int irq, void *data)
{
    int ret;
    u8 reg_val;
    bool prev_pg;
    int prev_chg_type;
    struct sgm41513 *sgm = (struct sgm41513 *)data;

    ret = sgm41513_read_byte(sgm, SGM41513_REG_08, &reg_val);
    if (ret)
        return IRQ_HANDLED;

    prev_pg = sgm->power_good;

    sgm->power_good = !!(reg_val & REG08_PG_STAT_MASK);

    if (!prev_pg && sgm->power_good)
        pr_notice("adapter/usb inserted\n");
    else if (prev_pg && !sgm->power_good)
        pr_notice("adapter/usb removed\n");

    prev_chg_type = sgm->psy_usb_type;

    ret = sgm41513_get_charger_type(sgm, &sgm->psy_usb_type);
    if (!ret && prev_chg_type != sgm->psy_usb_type && sgm->chg_det_enable)
        sgm41513_inform_charger_type(sgm);

    return IRQ_HANDLED;
}

static int sgm41513_register_interrupt(struct sgm41513 *sgm)
{
    int ret = 0;

    if (! sgm->client->irq) {
        pr_info("sgm->client->irq is NULL\n");//remember to config dws
        return -ENODEV;
    }

    ret = devm_request_threaded_irq(sgm->dev, sgm->client->irq, NULL,
                    sgm41513_irq_handler,
                    IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                    "ti_irq", sgm);
    if (ret < 0) {
        pr_err("request thread irq failed:%d\n", ret);
        return ret;
    }

    enable_irq_wake(sgm->irq);

    return 0;
}

/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 start */
#if defined(CONFIG_HQ_PROJECT_HS04)
static int sgm41513_set_shipmode(struct charger_device *chg_dev, bool en)
{
    int ret;
    u8 val;
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);

    if (en) {
        val = REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT;
        ret = sgm41513_update_bits(sgm, SGM41513_REG_07, REG07_BATFET_DIS_MASK, val);
    } else {
        val = REG07_BATFET_ON << REG07_BATFET_DIS_SHIFT;
        ret = sgm41513_update_bits(sgm, SGM41513_REG_07, REG07_BATFET_DIS_MASK, val);
    }

    pr_err("%s shipmode %s\n", en ? "enable" : "disable",
            !ret ? "successfully" : "failed");

    return ret;
}

static int sgm41513_get_shipmode(struct charger_device *chg_dev)
{
    int ret;
    u8 val;
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);

    msleep(100);
    ret = sgm41513_read_byte(sgm, SGM41513_REG_07, &val);
    if (ret == 0){
        pr_err("Reg[%.2x] = 0x%.2x\n", SGM41513_REG_07, val);
    } else {
        pr_err("%s: get shipmode reg fail! \n",__func__);
    }

    ret = (val & REG07_BATFET_DIS_MASK) >> REG07_BATFET_DIS_SHIFT;
    pr_err("%s:shipmode %s\n",__func__, ret ? "enabled" : "disabled");

    return ret;
}

static int sgm41513_disable_battfet_rst(struct sgm41513 *sgm)
{
    int ret;
    u8 val;

    val = REG07_BATFET_RST_DISABLE << REG07_BATFET_RST_EN_SHIFT;
    ret = sgm41513_update_bits(sgm, SGM41513_REG_07, REG07_BATFET_RST_EN_MASK, val);

    pr_err("disable BATTFET_RST_EN %s\n", !ret ? "successfully" : "failed");

    return ret;
}
#endif
/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 end*/

static int sgm41513_init_device(struct sgm41513 *sgm)
{
    int ret;

    sgm41513_disable_watchdog_timer(sgm);

    ret = sgm41513_set_stat_ctrl(sgm, sgm->platform_data->statctrl);
    if (ret)
        pr_err("Failed to set stat pin control mode, ret = %d\n", ret);

    ret = sgm41513_set_prechg_current(sgm, sgm->platform_data->iprechg);
    if (ret)
        pr_err("Failed to set prechg current, ret = %d\n", ret);

    ret = sgm41513_set_term_current(sgm, sgm->platform_data->iterm);
    if (ret)
        pr_err("Failed to set termination current, ret = %d\n", ret);

    ret = sgm41513_set_boost_voltage(sgm, sgm->platform_data->boostv);
    if (ret)
        pr_err("Failed to set boost voltage, ret = %d\n", ret);

    ret = sgm41513_set_boost_current(sgm, sgm->platform_data->boosti);
    if (ret)
        pr_err("Failed to set boost current, ret = %d\n", ret);

    ret = sgm41513_set_acovp_threshold(sgm, sgm->platform_data->vac_ovp);
    if (ret)
        pr_err("Failed to set acovp threshold, ret = %d\n", ret);

    ret= sgm41513_disable_safety_timer(sgm);
    if (ret)
        pr_err("Failed to set safety_timer stop, ret = %d\n", ret);

    ret = sgm41513_set_int_mask(sgm,
                    REG0A_IINDPM_INT_MASK |
                    REG0A_VINDPM_INT_MASK);
    if (ret)
        pr_err("Failed to set vindpm and iindpm int mask\n");

    /* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 start */
#if defined(CONFIG_HQ_PROJECT_HS04)
    ret = sgm41513_disable_battfet_rst(sgm);
    if (ret)
        pr_err("Failed to disable_battfet\n");
#endif
    /* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 start */
    return 0;
}

static void determine_initial_status(struct sgm41513 *sgm)
{
    sgm41513_irq_handler(sgm->irq, (void *) sgm);
}

static int sgm41513_detect_device(struct sgm41513 *sgm)
{
    int ret;
    u8 data;

    ret = sgm41513_read_byte(sgm, SGM41513_REG_0B, &data);
    if (!ret) {
        sgm->part_no = (data & REG0B_PN_MASK) >> REG0B_PN_SHIFT;
    }

    return ret;
}

static void sgm41513_dump_regs(struct sgm41513 *sgm)
{
    int addr;
    u8 val;
    int ret;

    for (addr = 0x0; addr <= 0x0B; addr++) {
        ret = sgm41513_read_byte(sgm, addr, &val);
        if (ret == 0)
            pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
    }
}

static ssize_t
sgm41513_show_registers(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    struct sgm41513 *sgm = dev_get_drvdata(dev);
    u8 addr;
    u8 val;
    u8 tmpbuf[200];
    int len;
    int idx = 0;
    int ret;

    idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sgm41513 Reg");
    for (addr = 0x0; addr <= 0x0B; addr++) {
        ret = sgm41513_read_byte(sgm, addr, &val);
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
sgm41513_store_registers(struct device *dev,
            struct device_attribute *attr, const char *buf,
            size_t count)
{
    struct sgm41513 *sgm = dev_get_drvdata(dev);
    int ret;
    unsigned int reg;
    unsigned int val;

    ret = sscanf(buf, "%x %x", &reg, &val);
    if (ret == 2 && reg < 0x0B) {
        sgm41513_write_byte(sgm, (unsigned char) reg,
                    (unsigned char) val);
    }

    return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, sgm41513_show_registers,
            sgm41513_store_registers);

static struct attribute *sgm41513_attributes[] = {
    &dev_attr_registers.attr,
    NULL,
};

static const struct attribute_group sgm41513_attr_group = {
    .attrs = sgm41513_attributes,
};

static int sgm41513_charging(struct charger_device *chg_dev, bool enable)
{

    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
    int ret = 0;
    u8 val;

    if (enable)
        ret = sgm41513_enable_charger(sgm);
    else
        ret = sgm41513_disable_charger(sgm);

    pr_err("%s charger %s\n", enable ? "enable" : "disable",
            !ret ? "successfully" : "failed");

    ret = sgm41513_read_byte(sgm, SGM41513_REG_01, &val);

    if (!ret)
        sgm->charge_enabled = !!(val & REG01_CHG_CONFIG_MASK);

    return ret;
}

static int sgm41513_plug_in(struct charger_device *chg_dev)
{

    int ret;

    ret = sgm41513_charging(chg_dev, true);

    if (ret)
        pr_err("Failed to enable charging:%d\n", ret);

    return ret;
}

static int sgm41513_plug_out(struct charger_device *chg_dev)
{
    int ret;

    ret = sgm41513_charging(chg_dev, false);

    if (ret)
        pr_err("Failed to disable charging:%d\n", ret);

    return ret;
}

static int sgm41513_dump_register(struct charger_device *chg_dev)
{
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);

    sgm41513_dump_regs(sgm);

    return 0;
}

static int sgm41513_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);

    *en = sgm->charge_enabled;

    return 0;
}

static int sgm41513_get_charging_status(struct charger_device *chg_dev,
    int *chg_stat)
{
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
    int ret;
    u8 val;

    ret = sgm41513_read_byte(sgm, SGM41513_REG_08, &val);
    if (!ret) {
        val = val & REG08_CHRG_STAT_MASK;
        val = val >> REG08_CHRG_STAT_SHIFT;
        *chg_stat = val;
    }

    return ret;
}

static int sgm41513_is_charging_done(struct charger_device *chg_dev, bool *done)
{
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
    int ret;
    u8 val;

    ret = sgm41513_read_byte(sgm, SGM41513_REG_08, &val);
    if (!ret) {
        val = val & REG08_CHRG_STAT_MASK;
        val = val >> REG08_CHRG_STAT_SHIFT;
        *done = (val == REG08_CHRG_STAT_CHGDONE);
    }

    return ret;
}

static int sgm41513_set_ichg(struct charger_device *chg_dev, u32 curr)
{
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);

    pr_err("charge curr = %d\n", curr);

    return sgm41513_set_chargecurrent(sgm, curr / 1000);
}

static int sgm41513_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
    u8 reg_val;
    int ichg;
    int ret;

    ret = sgm41513_read_byte(sgm, SGM41513_REG_02, &reg_val);
    if (!ret) {
        ichg = (reg_val & REG02_ICHG_MASK) >> REG02_ICHG_SHIFT;
        if (ichg < 8) {
            ichg = ichg * 5;
        } else if (ichg < 16) {
            ichg = (ichg - 8 ) * 10 + 40;
        } else if (ichg < 24) {
            ichg = (ichg - 16 ) * 20 + 130;
        } else if (ichg < 32) {
            ichg = (ichg - 24) * 30 + 300;
        } else if (ichg < 48) {
            ichg = (ichg - 32) * 60 + 540;
        } else {
            ichg = (ichg - 48) * 120 + 1500;
        }

        *curr = ichg * 1000;
    }

    return ret;
}

static int sgm41513_set_iterm(struct charger_device *chg_dev, u32 uA)
{
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);

    pr_err("termination curr = %d\n", uA);

    return sgm41513_set_term_current(sgm, uA / 1000);
}

static int sgm41513_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
    *curr = 60 * 1000;

    return 0;
}

static int sgm41513_get_min_aicr(struct charger_device *chg_dev, u32 *uA)
{
    *uA = 100 * 1000;
    return 0;
}

static int sgm41513_set_vchg(struct charger_device *chg_dev, u32 volt)
{
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);

    pr_err("charge volt = %d\n", volt);

    return sgm41513_set_chargevolt(sgm, volt / 1000);
}

static int sgm41513_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
    u8 reg_val;
    int vchg;
    int ret;

    ret = sgm41513_read_byte(sgm, SGM41513_REG_04, &reg_val);
    if (!ret) {
        vchg = (reg_val & REG04_VREG_MASK) >> REG04_VREG_SHIFT;
        if ( vchg == 15) {
            vchg = 3852;
        } else {
            vchg = vchg * REG04_VREG_LSB + REG04_VREG_BASE;
        }

        *volt = vchg * 1000;
    }

    return ret;
}

static int sgm41513_get_ivl_state(struct charger_device *chg_dev, bool *in_loop)
{
    int ret = 0;
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
    u8 reg_val;

    ret = sgm41513_read_byte(sgm, SGM41513_REG_0A, &reg_val);
    if (!ret)
        *in_loop = (ret & REG0A_VINDPM_STAT_MASK) >> REG0A_VINDPM_STAT_SHIFT;

    return ret;
}

static int sgm41513_get_ivl(struct charger_device *chg_dev, u32 *volt)
{
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
    u8 reg_val;
    int ivl;
    int ret;

    ret = sgm41513_read_byte(sgm, SGM41513_REG_06, &reg_val);
    if (!ret) {
        ivl = (reg_val & REG06_VINDPM_MASK) >> REG06_VINDPM_SHIFT;
        ivl = ivl * REG06_VINDPM_LSB + REG06_VINDPM_BASE;
        *volt = ivl * 1000;
    }

    return ret;
}

static int sgm41513_set_ivl(struct charger_device *chg_dev, u32 volt)
{
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);

    pr_err("vindpm volt = %d\n", volt);

    return sgm41513_set_input_volt_limit(sgm, volt / 1000);

}

static int sgm41513_set_icl(struct charger_device *chg_dev, u32 curr)
{
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);

    pr_err("indpm curr = %d\n", curr);

    return sgm41513_set_input_current_limit(sgm, curr / 1000);
}

static int sgm41513_get_icl(struct charger_device *chg_dev, u32 *curr)
{
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
    u8 reg_val;
    int icl;
    int ret;

    ret = sgm41513_read_byte(sgm, SGM41513_REG_00, &reg_val);
    if (!ret) {
        icl = (reg_val & REG00_IINLIM_MASK) >> REG00_IINLIM_SHIFT;
        icl = icl * REG00_IINLIM_LSB + REG00_IINLIM_BASE;
        *curr = icl * 1000;
    }

    return ret;

}

static int sgm41513_enable_te(struct charger_device *chg_dev, bool en)
{
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);

    pr_err("enable_term = %d\n", en);

    return sgm41513_enable_term(sgm, en);
}

static int sgm41513_kick_wdt(struct charger_device *chg_dev)
{
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);

    return sgm41513_reset_watchdog_timer(sgm);
}

static int sgm41513_set_otg(struct charger_device *chg_dev, bool en)
{
    int ret;
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);

    if (en)
        ret = sgm41513_enable_otg(sgm);
    else
        ret = sgm41513_disable_otg(sgm);

    pr_err("%s OTG %s\n", en ? "enable" : "disable",
            !ret ? "successfully" : "failed");

    return ret;
}

static int sgm41513_set_safety_timer(struct charger_device *chg_dev, bool en)
{
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
    int ret;

    if (en)
        ret = sgm41513_enable_safety_timer(sgm);
    else
        ret = sgm41513_disable_safety_timer(sgm);

    return ret;
}

static int sgm41513_is_safety_timer_enabled(struct charger_device *chg_dev,
                        bool *en)
{
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
    int ret;
    u8 reg_val;

    ret = sgm41513_read_byte(sgm, SGM41513_REG_05, &reg_val);

    if (!ret)
        *en = !!(reg_val & REG05_EN_TIMER_MASK);

    return ret;
}

static int sgm41513_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);

    if (!sgm->psy) {
        dev_notice(sgm->dev, "%s: cannot get psy\n", __func__);
        return -ENODEV;
    }

    switch (event) {
    case EVENT_FULL:
    case EVENT_RECHARGE:
    case EVENT_DISCHARGE:
        power_supply_changed(sgm->psy);
        break;
    default:
        break;
    }
    return 0;
}

static int sgm41513_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
    int ret;

    pr_err("otg curr = %d\n", curr);

    ret = sgm41513_set_boost_current(sgm, curr / 1000);

    return ret;
}

static int sgm41513_set_hiz_mode(struct charger_device *chg_dev, bool en)
{
    int ret;
    struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);

    if (en)
        ret = sgm41513_enter_hiz_mode(sgm);
    else
        ret = sgm41513_exit_hiz_mode(sgm);

    pr_err("%s hiz mode %s\n", en ? "enable" : "disable",
            !ret ? "successfully" : "failed");

    return ret;
}

static int sgm41513_get_hiz_mode(struct charger_device *chg_dev)
{
    int ret;
    struct sgm41513 *bq = dev_get_drvdata(&chg_dev->dev);
    u8 val;
    ret = sgm41513_read_byte(bq, SGM41513_REG_00, &val);
    if (ret == 0){
        pr_err("Reg[%.2x] = 0x%.2x\n", SGM41513_REG_00, val);
    }

    ret = (val & REG00_ENHIZ_MASK) >> REG00_ENHIZ_SHIFT;
    pr_err("%s:hiz mode %s\n",__func__, ret ? "enabled" : "disabled");

    return ret;
}

static struct charger_ops sgm41513_chg_ops = {
    /* Normal charging */
    .plug_in = sgm41513_plug_in,
    .plug_out = sgm41513_plug_out,
    .dump_registers = sgm41513_dump_register,
    .enable = sgm41513_charging,
    .is_enabled = sgm41513_is_charging_enable,
    .get_charging_current = sgm41513_get_ichg,
    .set_charging_current = sgm41513_set_ichg,
    .get_input_current = sgm41513_get_icl,
    .set_input_current = sgm41513_set_icl,
    .get_constant_voltage = sgm41513_get_vchg,
    .set_constant_voltage = sgm41513_set_vchg,
    .kick_wdt = sgm41513_kick_wdt,
    .set_mivr = sgm41513_set_ivl,
    .get_mivr = sgm41513_get_ivl,
    .get_mivr_state = sgm41513_get_ivl_state,
    .is_charging_done = sgm41513_is_charging_done,
    .set_eoc_current = sgm41513_set_iterm,
    .enable_termination = sgm41513_enable_te,
    .reset_eoc_state = NULL,
    .get_min_charging_current = sgm41513_get_min_ichg,
    .get_min_input_current = sgm41513_get_min_aicr,

    /* Safety timer */
    .enable_safety_timer = sgm41513_set_safety_timer,
    .is_safety_timer_enabled = sgm41513_is_safety_timer_enabled,

    /* Power path */
    .enable_powerpath = NULL,
    .is_powerpath_enabled = NULL,

    /* OTG */
    .enable_otg = sgm41513_set_otg,
    .set_boost_current_limit = sgm41513_set_boost_ilmt,
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
    .event = sgm41513_do_event,

    .get_chr_status = sgm41513_get_charging_status,
    .set_hiz_mode = sgm41513_set_hiz_mode,
    .get_hiz_mode = sgm41513_get_hiz_mode,
    /* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 start */
#if defined(CONFIG_HQ_PROJECT_HS04)
    .get_ship_mode = sgm41513_get_shipmode,
    .set_ship_mode = sgm41513_set_shipmode,
#endif
    /* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 end */
};

static struct of_device_id sgm41513_charger_match_table[] = {
    {
     .compatible = "sgm41513",
     },
    {},
};
MODULE_DEVICE_TABLE(of, sgm41513_charger_match_table);

/* ======================= */
/* charger ic Power Supply Ops */
/* ======================= */

enum CHG_STATUS {
    CHG_STATUS_NOT_CHARGING = 0,
    CHG_STATUS_PRE_CHARGE,
    CHG_STATUS_FAST_CHARGING,
    CHG_STATUS_DONE,
};

static int charger_ic_get_online(struct sgm41513 *sgm,
                     bool *val)
{
    bool pwr_rdy = false;

    sgm41513_get_charger_type(sgm, &sgm->psy_usb_type);
    if(sgm->psy_usb_type != POWER_SUPPLY_TYPE_UNKNOWN)
        pwr_rdy = false;
    else
        pwr_rdy = true;

    dev_info(sgm->dev, "%s: online = %d\n", __func__, pwr_rdy);
    *val = pwr_rdy;
    return 0;
}

static int charger_ic_get_property(struct power_supply *psy,
                       enum power_supply_property psp,
                       union power_supply_propval *val)
{
    struct sgm41513 *sgm = power_supply_get_drvdata(psy);
    bool pwr_rdy = false;
    int ret = 0;
    int chr_status = 0;

    dev_dbg(sgm->dev, "%s: prop = %d\n", __func__, psp);
    switch (psp) {
    case POWER_SUPPLY_PROP_ONLINE:
        ret = charger_ic_get_online(sgm, &pwr_rdy);
        val->intval = pwr_rdy;
        break;
    case POWER_SUPPLY_PROP_STATUS:
        if (sgm->psy_usb_type == POWER_SUPPLY_USB_TYPE_UNKNOWN)
        {
            val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
            break;
        }
        ret = sgm41513_get_chrg_stat(sgm, &chr_status);
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
            if(sgm->charge_enabled)
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

static int sgm41513_charger_remove(struct i2c_client *client);
static int sgm41513_charger_probe(struct i2c_client *client,
                 const struct i2c_device_id *id)
{
    struct sgm41513 *sgm;
    const struct of_device_id *match;
    struct device_node *node = client->dev.of_node;
    struct power_supply_config charger_cfg = {};

    int ret = 0;

    sgm = devm_kzalloc(&client->dev, sizeof(struct sgm41513), GFP_KERNEL);
    if (!sgm)
        return -ENOMEM;

    client->addr = 0x1A;
    sgm->dev = &client->dev;
    sgm->client = client;

    i2c_set_clientdata(client, sgm);

    mutex_init(&sgm->i2c_rw_lock);

    ret = sgm41513_detect_device(sgm);
    if (ret) {
        pr_err("No sgm41513 device found!\n");
        return -ENODEV;
    }

    match = of_match_node(sgm41513_charger_match_table, node);
    if (match == NULL) {
        pr_err("device tree match not found\n");
        return -EINVAL;
    }

    if (sgm->part_no == 0x00)
    {
        pr_info("SGM41513 part number match success \n");
    } else {
        pr_info("SGM41513 part number match fial \n");
        sgm41513_charger_remove(client);
        return -EINVAL;
    }

    sgm->platform_data = sgm41513_parse_dt(node, sgm);

    if (!sgm->platform_data) {
        pr_err("No platform data provided.\n");
        return -EINVAL;
    }

    ret = sgm41513_init_device(sgm);
    if (ret) {
        pr_err("Failed to init device\n");
        return ret;
    }

    sgm41513_register_interrupt(sgm);

    sgm->chg_dev = charger_device_register(sgm->chg_dev_name,
                            &client->dev, sgm,
                            &sgm41513_chg_ops,
                            &sgm41513_chg_props);
    if (IS_ERR_OR_NULL(sgm->chg_dev)) {
        ret = PTR_ERR(sgm->chg_dev);
        return ret;
    }

    ret = sysfs_create_group(&sgm->dev->kobj, &sgm41513_attr_group);
    if (ret)
        dev_err(sgm->dev, "failed to register sysfs. err: %d\n", ret);

    determine_initial_status(sgm);

    /* power supply register */
    memcpy(&sgm->psy_desc,
        &charger_ic_desc, sizeof(sgm->psy_desc));
    sgm->psy_desc.name = dev_name(&client->dev);

    charger_cfg.drv_data = sgm;
    charger_cfg.of_node = sgm->dev->of_node;
    charger_cfg.supplied_to = charger_ic_supplied_to;
    charger_cfg.num_supplicants = ARRAY_SIZE(charger_ic_supplied_to);
    sgm->psy = devm_power_supply_register(&client->dev,
                    &sgm->psy_desc, &charger_cfg);
    if (IS_ERR(sgm->psy)) {
        dev_notice(&client->dev, "Fail to register power supply dev\n");
        ret = PTR_ERR(sgm->psy);
    }

	/*HS03s for SR-AL5628-01-162 by zhangjiangbin at 20220915 start*/
	set_hardinfo_charger_data(CHG_INFO, "SGM41513");
	/*HS03s for SR-AL5628-01-162 by zhangjiangbin at 20220915 end*/

    pr_err("sgm41513 probe successfully, Part Num:%d\n!",
            sgm->part_no);

    return 0;
}

static int sgm41513_charger_remove(struct i2c_client *client)
{
    struct sgm41513 *sgm = i2c_get_clientdata(client);

    mutex_destroy(&sgm->i2c_rw_lock);

    sysfs_remove_group(&sgm->dev->kobj, &sgm41513_attr_group);

    return 0;
}

static void sgm41513_charger_shutdown(struct i2c_client *client)
{

}

static struct i2c_driver sgm41513_charger_driver = {
    .driver = {
            .name = "sgm41513-charger",
            .owner = THIS_MODULE,
            .of_match_table = sgm41513_charger_match_table,
            },

    .probe = sgm41513_charger_probe,
    .remove = sgm41513_charger_remove,
    .shutdown = sgm41513_charger_shutdown,

};

module_i2c_driver(sgm41513_charger_driver);

MODULE_DESCRIPTION("SGM41513 Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SGM");
