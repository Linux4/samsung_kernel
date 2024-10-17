/*
 * BQ2560x battery charging driver
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
#include <mt-plat/v1/charger_type.h>

#include "mtk_charger_intf.h"
/* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/15 start */
#include "mtk_charger_init.h"
/* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/15 end */
#include "upm6910_reg.h"
#include "upm6910.h"

#include <mt-plat/upmu_common.h>

/* hs14 code for SR-AL6528A-01-787 by gaozhengwei at 2022/12/06 start */
#define I2C_RETRY_CNT       3
/* hs14 code for SR-AL6528A-01-787 by gaozhengwei at 2022/12/06 end */

enum {
    PN_UPM6910D,
};

enum upm6910_part_no {
    UPM6910D = 0x02,
};

static int pn_data[] = {
    [PN_UPM6910D] = 0x02,
};

static char *pn_str[] = {
    [PN_UPM6910D] = "upm6910d",
};

struct upm6910 {
    struct device *dev;
    struct i2c_client *client;

    enum upm6910_part_no part_no;
    int revision;

    const char *chg_dev_name;
    const char *eint_name;

    bool chg_det_enable;

    int status;
    int irq;
    u32 intr_gpio;

    struct mutex i2c_rw_lock;

    bool charge_enabled;    /* Register bit status */
    bool power_good;
    bool vbus_gd;
    /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 start */
    bool vbus_stat;
    /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 end */
    /* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 start */
    bool bypass_chgdet_en;
    /* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 end */

    struct upm6910_platform_data *platform_data;
    struct charger_device *chg_dev;

    struct power_supply *psy;
    struct power_supply *bat_psy;
    struct power_supply *usb_psy;

    struct delayed_work psy_dwork;
    struct delayed_work prob_dwork;
    /* hs14 code for AL6528ADEU-722 by qiaodan at 2022/10/20 start */
    struct delayed_work bc12_recheck_dwork;
    /* hs14 code for AL6528ADEU-722 by qiaodan at 2022/10/20 end */
    int psy_usb_type;
    /* hs14 code for AL6528ADEU-2768 by qiaodan at 2022/11/25 start */
    bool hiz_mode_flag;
    /* hs14 code for AL6528ADEU-2768 by qiaodan at 2022/11/25 end */
};

static const struct charger_properties upm6910_chg_props = {
    .alias_name = "upm6910",
};

static int __upm6910_read_reg(struct upm6910 *upm, u8 reg, u8 *data)
{
    s32 ret;
    /* hs14 code for SR-AL6528A-01-787 by gaozhengwei at 2022/12/06 start */
    int i;

    for (i = 0; i < I2C_RETRY_CNT; ++i) {

        ret = i2c_smbus_read_byte_data(upm->client, reg);

        if (ret >= 0)
            break;

        pr_info("%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
            __func__, reg, ret, i + 1, I2C_RETRY_CNT);
    }
    /* hs14 code for SR-AL6528A-01-787 by gaozhengwei at 2022/12/06 end */

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
    /* hs14 code for SR-AL6528A-01-787 by gaozhengwei at 2022/12/06 start */
    int i;

    for (i = 0; i < I2C_RETRY_CNT; ++i) {

        ret = i2c_smbus_write_byte_data(upm->client, reg, val);

        if (ret >= 0)
            break;

        pr_info("%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
            __func__, reg, ret, i + 1, I2C_RETRY_CNT);
    }
    /* hs14 code for SR-AL6528A-01-787 by gaozhengwei at 2022/12/06 end */

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

    if (curr < REG02_ICHG_BASE)
        curr = REG02_ICHG_BASE;

    ichg = (curr - REG02_ICHG_BASE) / REG02_ICHG_LSB;
    return upm6910_update_bits(upm, UPM6910_REG_02, REG02_ICHG_MASK,
                   ichg << REG02_ICHG_SHIFT);

}
/* hs14 code for AL6528ADEU-3951 by qiaodan at 2023/04/07 start */
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
    int curr_value = 0;
    int ret = 0;

    if (!upm) {
        pr_err("%s device is null\n", __func__);
        return -ENODEV;
    }

    ret = upm6910_get_chargecurrent(upm,&curr_value);
    if (!ret) {
        pr_info("%s the curr value is %d\n", __func__ ,curr_value);
        if (curr_value < ICHG_OCP_THRESHOLD_MA) {
            ret = upm6910_set_chargecurrent(upm, ICHG_OCP_THRESHOLD_MA);
        }
    } else {
        pr_err("%s get charge current failed\n", __func__);
    }

    return ret;
}
/* hs14 code for AL6528ADEU-3951 by qiaodan at 2023/04/07 end */

int upm6910_set_term_current(struct upm6910 *upm, int curr)
{
    u8 iterm;

    if (curr < REG03_ITERM_BASE)
        curr = REG03_ITERM_BASE;

    iterm = (curr - REG03_ITERM_BASE) / REG03_ITERM_LSB;

    return upm6910_update_bits(upm, UPM6910_REG_03, REG03_ITERM_MASK,
                   iterm << REG03_ITERM_SHIFT);
}
EXPORT_SYMBOL_GPL(upm6910_set_term_current);

int upm6910_set_prechg_current(struct upm6910 *upm, int curr)
{
    u8 iprechg;

    if (curr < REG03_IPRECHG_BASE)
        curr = REG03_IPRECHG_BASE;

    iprechg = (curr - REG03_IPRECHG_BASE) / REG03_IPRECHG_LSB;

    return upm6910_update_bits(upm, UPM6910_REG_03, REG03_IPRECHG_MASK,
                   iprechg << REG03_IPRECHG_SHIFT);
}
EXPORT_SYMBOL_GPL(upm6910_set_prechg_current);

int upm6910_set_chargevolt(struct upm6910 *upm, int volt)
{
    u8 val;

    if (volt < REG04_VREG_BASE)
        volt = REG04_VREG_BASE;

    val = (volt - REG04_VREG_BASE) / REG04_VREG_LSB;
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

/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
static int upm6910_get_input_volt_limit(struct upm6910 *upm, int *volt)
{
    u8 val;
    int ret;
    int vindpm;

    ret = upm6910_read_byte(upm, UPM6910_REG_06, &val);
    if (ret == 0){
        pr_err("Reg[%.2x] = 0x%.2x\n", UPM6910_REG_06, val);
    }

    pr_info("vindpm_reg_val:0x%X\n", val);
    vindpm = (val & REG06_VINDPM_MASK) >> REG06_VINDPM_SHIFT;
    vindpm = vindpm * REG06_VINDPM_LSB + REG06_VINDPM_BASE;
    *volt = vindpm;

    return ret;
}
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */

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

/* hs14 code for AL6528A-164|AL6528ADEU-643 by gaozhengwei|wenyaqi at 2022/10/13 start */
static int upm6910_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

    dev_info(upm->dev, "%s event = %d\n", __func__, event);

    switch (event) {
    case EVENT_EOC:
        charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_EOC);
        break;
    case EVENT_RECHARGE:
        charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_RECHG);
        break;
    default:
        break;
    }
    return 0;
}
/* hs14 code for AL6528A-164|AL6528ADEU-643 by gaozhengwei|wenyaqi at 2022/10/13 end */

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

/* hs14 code for AL6528A-341 by wenyaqi at 2022/10/20 start */
int upm6910_set_boost_current(struct upm6910 *upm, int curr)
{
    u8 val;

    if (curr >= BOOSTI_1200)
        val = REG02_BOOST_LIM_1P2A;
    else
        val = REG02_BOOST_LIM_0P5A;

    return upm6910_update_bits(upm, UPM6910_REG_02, REG02_BOOST_LIM_MASK,
                   val << REG02_BOOST_LIM_SHIFT);
}

int upm6910_set_boost_voltage(struct upm6910 *upm, int volt)
{
    u8 val;

    if (volt >= BOOSTV_5300)
        val = REG06_BOOSTV_5P3V;
    else if (volt >= BOOSTV_5150 && volt < BOOSTV_5300)
        val = REG06_BOOSTV_5P15V;
    else if (volt >= BOOSTV_4850 && volt < BOOSTV_5000)
        val = REG06_BOOSTV_4P85V;
    else
        val = REG06_BOOSTV_5V;

    return upm6910_update_bits(upm, UPM6910_REG_06, REG06_BOOSTV_MASK,
                   val << REG06_BOOSTV_SHIFT);
}
EXPORT_SYMBOL_GPL(upm6910_set_boost_voltage);

static int upm6910_set_acovp_threshold(struct upm6910 *upm, int volt)
{
    u8 val;
    /* hs14 code for AL6528ADEU-2768 by qiaodan at 2022/11/25 start */
    if (upm->hiz_mode_flag == true) {
        pr_err("%s now in hiz mode, return", __func__);
        return 0;
    }
    /* hs14 code for AL6528ADEU-2768 by qiaodan at 2022/11/25 end */
    if (volt >= VAC_OVP_14000)
        val = REG06_OVP_14P0V;
    else if (volt >= VAC_OVP_10500 && volt < VAC_OVP_14000)
        val = REG06_OVP_10P5V;
    else if (volt >= VAC_OVP_6500 && volt < VAC_OVP_10500)
        val = REG06_OVP_6P5V;
    else
        val = REG06_OVP_5P5V;

    return upm6910_update_bits(upm, UPM6910_REG_06, REG06_OVP_MASK,
                   val << REG06_OVP_SHIFT);
}
EXPORT_SYMBOL_GPL(upm6910_set_acovp_threshold);
/* hs14 code for AL6528A-341 by wenyaqi at 2022/10/20 end */

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

static int upm6910_force_dpdm(struct upm6910 *upm)
{
    const u8 val = REG07_FORCE_DPDM << REG07_FORCE_DPDM_SHIFT;

    return upm6910_update_bits(upm, UPM6910_REG_07, REG07_FORCE_DPDM_MASK,
                val);
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

/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
static int upm6910_enable_hvdcp(struct upm6910 *upm, bool en)
{
    const u8 val = en << REG0C_EN_HVDCP_SHIFT;

    return upm6910_update_bits(upm, UPM6910_REG_0C, REG0C_EN_HVDCP_MASK,
                   val);
}
EXPORT_SYMBOL_GPL(upm6910_enable_hvdcp);

static int upm6910_set_dp(struct upm6910 *upm, int dp_stat)
{
    const u8 val = dp_stat << REG0C_DP_MUX_SHIFT;

    return upm6910_update_bits(upm, UPM6910_REG_0C, REG0C_DP_MUX_MASK,
                   val);
}
EXPORT_SYMBOL_GPL(upm6910_set_dp);

static int upm6910_set_dm(struct upm6910 *upm, int dm_stat)
{
    const u8 val = dm_stat << REG0C_DM_MUX_SHIFT;

    return upm6910_update_bits(upm, UPM6910_REG_0C, REG0C_DM_MUX_MASK,
                   val);
}
EXPORT_SYMBOL_GPL(upm6910_set_dm);
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */

static int upm6910_dpdm_set_hidden_mode(struct upm6910 *upm)
{
    int ret = 0;
    union power_supply_propval propval;

    if (!upm->bat_psy) {
        upm->bat_psy = power_supply_get_by_name("battery");
        if (!upm->bat_psy) {
            pr_err("%s Couldn't get battery psy\n", __func__);
            return -ENODEV;
        }
    }

    ret = power_supply_get_property(upm->bat_psy,
                POWER_SUPPLY_PROP_VOLTAGE_NOW, &propval);
    /* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
    if (ret || propval.intval <= 3450000) {
    /* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */
        pr_err("%s cannot goto hidden mode\n", __func__);
        return -EINVAL;
    }

    ret = upm6910_write_byte(upm, 0xA9, 0x6E);
    if (ret) {
        pr_err("write 0xA9, 0x6E failed(%d)\n", ret);
    }
    ret = upm6910_write_byte(upm, 0xB1, 0x80);
    if (ret) {
        pr_err("write 0xB1, 0x80 failed(%d)\n", ret);
    }
    ret = upm6910_write_byte(upm, 0xB0, 0x21);
    if (ret) {
        pr_err("write 0xB0, 0x21 failed(%d)\n", ret);
    }
    ret = upm6910_write_byte(upm, 0xB1, 0x00);
    if (ret) {
        pr_err("write 0xB1, 0x00 failed(%d)\n", ret);
    }
    ret = upm6910_write_byte(upm, 0xA9, 0x00);
    if (ret) {
        pr_err("write 0xA9, 0x00 failed(%d)\n", ret);
    }

    return ret;
}
EXPORT_SYMBOL_GPL(upm6910_dpdm_set_hidden_mode);

/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
static int upm6910_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
    u8 reg_val;
    int ichg;
    int ret;

    ret = upm6910_read_byte(upm, UPM6910_REG_02, &reg_val);
    if (!ret) {
        ichg = (reg_val & REG02_ICHG_MASK) >> REG02_ICHG_SHIFT;
        ichg = ichg * REG02_ICHG_LSB + REG02_ICHG_BASE;
        *curr = ichg * 1000;
    }

    return ret;
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
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */

static struct upm6910_platform_data *upm6910_parse_dt(struct device_node *np,
                              struct upm6910 *upm)
{
    int ret;
    struct upm6910_platform_data *pdata;

    pdata = devm_kzalloc(&upm->client->dev, sizeof(struct upm6910_platform_data),
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

    ret = of_get_named_gpio(np, "upm,intr_gpio", 0);
    if (ret < 0) {
        pr_err("%s no upm,intr_gpio(%d)\n",
                      __func__, ret);
    } else
        upm->intr_gpio = ret;

    pr_info("%s intr_gpio = %u\n",
                __func__, upm->intr_gpio);

    upm->chg_det_enable =
        of_property_read_bool(np, "upm,upm6910,charge-detect-enable");

    ret = of_property_read_u32(np, "upm,upm6910,usb-vlim", &pdata->usb.vlim);
    if (ret) {
        pdata->usb.vlim = 4500;
        pr_err("Failed to read node of upm,upm6910,usb-vlim\n");
    }

    ret = of_property_read_u32(np, "upm,upm6910,usb-ilim", &pdata->usb.ilim);
    if (ret) {
        pdata->usb.ilim = 2000;
        pr_err("Failed to read node of upm,upm6910,usb-ilim\n");
    }

    ret = of_property_read_u32(np, "upm,upm6910,usb-vreg", &pdata->usb.vreg);
    if (ret) {
        pdata->usb.vreg = 4200;
        pr_err("Failed to read node of upm,upm6910,usb-vreg\n");
    }

    ret = of_property_read_u32(np, "upm,upm6910,usb-ichg", &pdata->usb.ichg);
    if (ret) {
        pdata->usb.ichg = 2000;
        pr_err("Failed to read node of upm,upm6910,usb-ichg\n");
    }

    ret = of_property_read_u32(np, "upm,upm6910,stat-pin-ctrl",
                   &pdata->statctrl);
    if (ret) {
        pdata->statctrl = 0;
        pr_err("Failed to read node of upm,upm6910,stat-pin-ctrl\n");
    }

    ret = of_property_read_u32(np, "upm,upm6910,precharge-current",
                   &pdata->iprechg);
    if (ret) {
        pdata->iprechg = 180;
        pr_err("Failed to read node of upm,upm6910,precharge-current\n");
    }

    ret = of_property_read_u32(np, "upm,upm6910,termination-current",
                   &pdata->iterm);
    if (ret) {
        pdata->iterm = 180;
        pr_err
            ("Failed to read node of upm,upm6910,termination-current\n");
    }

    ret =
        of_property_read_u32(np, "upm,upm6910,boost-voltage",
                 &pdata->boostv);
    if (ret) {
        pdata->boostv = 5000;
        pr_err("Failed to read node of upm,upm6910,boost-voltage\n");
    }

    ret =
        of_property_read_u32(np, "upm,upm6910,boost-current",
                 &pdata->boosti);
    if (ret) {
        pdata->boosti = 1200;
        pr_err("Failed to read node of upm,upm6910,boost-current\n");
    }

    ret = of_property_read_u32(np, "upm,upm6910,vac-ovp-threshold",
                   &pdata->vac_ovp);
    if (ret) {
        pdata->vac_ovp = 6500;
        pr_err("Failed to read node of upm,upm6910,vac-ovp-threshold\n");
    }

    return pdata;
}

/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
static bool is_hiz_mode_trigger_interrupt(struct upm6910 *upm)
{
    bool exist = false;
    int ret = 0, vbus = 0, hiz_en = 0;
    u8 reg_val = 0, vbus_stat = 0;
    union power_supply_propval propval;

    if (!upm->usb_psy) {
        upm->usb_psy = power_supply_get_by_name("usb");
        if (!upm->usb_psy) {
            pr_err("%s Couldn't get usb psy\n", __func__);
            return -ENODEV;
        }
    }

    ret = power_supply_get_property(upm->usb_psy,
                POWER_SUPPLY_PROP_VOLTAGE_NOW, &propval);
    if (!ret) {
        vbus = propval.intval;
    }

    ret = upm6910_read_byte(upm, UPM6910_REG_08, &reg_val);
    if (!ret) {
        vbus_stat = (reg_val & REG08_VBUS_STAT_MASK);
        vbus_stat >>= REG08_VBUS_STAT_SHIFT;
    }

    ret = upm6910_read_byte(upm, UPM6910_REG_00, &reg_val);
    if (!ret) {
        hiz_en = !!(reg_val & REG00_ENHIZ_MASK);
    }

    if (vbus > 4000 && vbus_stat != REG08_VBUS_TYPE_OTG && hiz_en) {
        exist = true;
    }

    return exist;
}

static int upm6910_get_charger_type(struct upm6910 *upm, int *type)
{
    int ret;

    u8 reg_val = 0;
    int vbus_stat = 0;
    int chg_type = CHARGER_UNKNOWN;

    ret = upm6910_read_byte(upm, UPM6910_REG_08, &reg_val);

    if (ret)
        return ret;

    vbus_stat = (reg_val & REG08_VBUS_STAT_MASK);
    vbus_stat >>= REG08_VBUS_STAT_SHIFT;

    /* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
    pr_info("[%s] reg08: 0x%02x, vbus state: %d, ret: %d\n", __func__, reg_val, vbus_stat, ret);
    /* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */

    switch (vbus_stat) {

    case REG08_VBUS_TYPE_NONE:
        chg_type = CHARGER_UNKNOWN;
        break;
    case REG08_VBUS_TYPE_SDP:
        chg_type = STANDARD_HOST;
        break;
    case REG08_VBUS_TYPE_CDP:
        chg_type = CHARGING_HOST;
        break;
    case REG08_VBUS_TYPE_DCP:
        chg_type = STANDARD_CHARGER;
        break;
    case REG08_VBUS_TYPE_UNKNOWN:
        chg_type = NONSTANDARD_CHARGER;
        break;
    case REG08_VBUS_TYPE_NON_STD:
        chg_type = STANDARD_CHARGER;
        break;
    default:
        chg_type = NONSTANDARD_CHARGER;
        break;
    }

    /* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/10 start */
    /* hs14 code for AL6528ADEU-2064 by lina at 2022/11/17 start */
    if (chg_type == STANDARD_HOST ||
        chg_type == CHARGING_HOST ||
        chg_type == CHARGER_UNKNOWN) {
    /*hs14 code for AL6528ADEU-2064 by lina at 2022/11/17 end*/
        Charger_Detect_Release();
    } else {
        ret = upm6910_enable_hvdcp(upm, true);
        if (ret)
            pr_err("Failed to en HVDCP, ret = %d\n", ret);

        ret = upm6910_set_dp(upm, REG0C_DPDM_OUT_0P6V);
        if (ret)
            pr_err("Failed to set dp 0.6v, ret = %d\n", ret);
    }
    /* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */

    *type = chg_type;

    return 0;
}

static void upm6910_inform_psy_dwork_handler(struct work_struct *work)
{
    int ret = 0;
    union power_supply_propval propval;
    struct upm6910 *upm = container_of(work, struct upm6910,
                                psy_dwork.work);

    if (!upm->psy) {
        upm->psy = power_supply_get_by_name("charger");
        if (!upm->psy) {
            pr_err("%s Couldn't get psy\n", __func__);
            mod_delayed_work(system_wq, &upm->psy_dwork,
                    msecs_to_jiffies(2*1000));
            return;
        }
    }

    if (upm->psy_usb_type != CHARGER_UNKNOWN)
        propval.intval = 1;
    else
        propval.intval = 0;

    ret = power_supply_set_property(upm->psy, POWER_SUPPLY_PROP_ONLINE,
                    &propval);

    if (ret < 0)
        pr_notice("inform power supply online failed:%d\n", ret);

    propval.intval = upm->psy_usb_type;

    ret = power_supply_set_property(upm->psy,
                    POWER_SUPPLY_PROP_CHARGE_TYPE,
                    &propval);

    if (ret < 0)
        pr_notice("inform power supply charge type failed:%d\n", ret);

    return;
}
/*hs14 code for AL6528ADEU-2064 by lina at 2022/11/17 start */
static int upm6910_vbus_source_capability_detect(struct upm6910 *upm, int *type)
{
    int ret = 0;
    int vindpm_stat = 0;
    u8 reg_val = 0;
    int pre_ichg = 0;
    int pre_icl = 0;
    int pre_vindpm = 0;

    /* get previous setting */
    ret |= upm6910_get_ichg(upm->chg_dev, &pre_ichg);
    ret |= upm6910_get_icl(upm->chg_dev, &pre_icl);
    ret |= upm6910_get_input_volt_limit(upm, &pre_vindpm);
    pr_info("pre_ichg:%d, pre_icl:%d, pre_vindpm:%d, ret:%d\n",
            pre_ichg, pre_icl, pre_vindpm, ret);

    /* set a power to test adapter */
    ret |= upm6910_set_input_volt_limit(upm, POWER_DETECT_VINDPM);
    ret |= upm6910_set_chargecurrent(upm, POWER_DETECT_ICHG);
    ret |= upm6910_set_input_current_limit(upm, POWER_DETECT_ICL);

    /* if no vindpm, report DCP */
    ret |= upm6910_read_byte(upm, UPM6910_REG_0A, &reg_val);
    vindpm_stat = !!(reg_val & REG0A_VINDPM_STAT_MASK);
    if (!vindpm_stat) {
        *type = STANDARD_CHARGER;
    } else {
        *type =  NONSTANDARD_CHARGER;
    }
    pr_info("vindpm_stat:%d\n", vindpm_stat);

    /* restore previous stat */
    ret |= upm6910_set_chargecurrent(upm, pre_ichg);
    ret |= upm6910_set_input_current_limit(upm, pre_icl);
    ret |= upm6910_set_input_volt_limit(upm, pre_vindpm);
    if (ret) {
        pr_err("%s: %d\n", __func__, ret);
    }

    return ret;
}
/*hs14 code for AL6528ADEU-2064 by lina at 2022/11/17 end*/
static void upm6910_bc12_recheck_dwork_handler(struct work_struct *work)
{
    int retry_cnt = 0;
    int ret = 0;
    u8 reg_val = 0;
    int vbus_stat = 0;
    /* hs14 code for AL6528ADEU-1146 by gaozhengwei at 2022/10/25 start */
    struct upm6910 *upm = container_of(work, struct upm6910,
                                bc12_recheck_dwork.work);
    /* hs14 code for AL6528ADEU-1146 by gaozhengwei at 2022/10/25 end */
    /*hs14 code for AL6528ADEU-2064 by lina at 2022/11/17 start */
    union power_supply_propval propval;
    int vbus = 0;

    if (!upm->usb_psy) {
        upm->usb_psy = power_supply_get_by_name("usb");
        if (!upm->usb_psy) {
            pr_err("%s Couldn't get usb psy\n", __func__);
            return ;
        }
    }

    ret = power_supply_get_property(upm->usb_psy,
                POWER_SUPPLY_PROP_VOLTAGE_NOW, &propval);
    if (!ret) {
        vbus = propval.intval;
        pr_info("vbus:%d", vbus);
    }
    if (vbus > POWER_FAST_CHARGE_VOLT_RISE) {
        return;
    } else {
        ret = upm6910_enable_hvdcp(upm, true);
        if (ret)
            pr_err("Failed to en HVDCP, ret = %d\n", ret);

        ret = upm6910_set_dp(upm, REG0C_DPDM_OUT_HIZ);
        if (ret)
            pr_err("Failed to set dp HIZ, ret = %d\n", ret);
    }
    /*hs14 code for AL6528ADEU-2064 by lina at 2022/11/17 end*/

    disable_irq(upm->client->irq);
    Charger_Detect_Init();
    upm6910_dpdm_set_hidden_mode(upm);
    msleep(200);

    do {
        ret = upm6910_read_byte(upm, UPM6910_REG_08, &reg_val);
        if (ret) {
            pr_err("get UPM6910_REG_08 failed\n");
        }
        vbus_stat = (reg_val & REG08_VBUS_STAT_MASK) >> REG08_VBUS_STAT_SHIFT;
        pr_info("reg08:0x%02x, vbus_stat:%d, cnt:%d\n", reg_val, vbus_stat, retry_cnt);
        if (vbus_stat != REG08_VBUS_TYPE_NONE) {
            break;
        }
        msleep(50);
    } while(retry_cnt++ < 16);

/*hs14 code for AL6528ADEU-2064 by lina at 2022/11/17 start */
    if (retry_cnt > DCD_RETRY_TIMEOUT) {
        pr_info("bc1.2 data contact detect timeout, no DP/DM\n");
        upm->psy_usb_type = NONSTANDARD_CHARGER;
    } else {
        if (vbus_stat == REG08_VBUS_TYPE_UNKNOWN)
            upm6910_vbus_source_capability_detect(upm, &upm->psy_usb_type);
    }

    if (upm->psy_usb_type != STANDARD_CHARGER) {
        Charger_Detect_Release();
    } else {
        ret = upm6910_enable_hvdcp(upm, true);
        if (ret)
            pr_err("Failed to en HVDCP, ret = %d\n", ret);
/*hs14 code for AL6528ADEU-2064 by lina at 2022/11/17 end*/
        ret = upm6910_set_dp(upm, REG0C_DPDM_OUT_0P6V);
        if (ret) {
            pr_err("Failed to set dp 0.6v, ret = %d\n", ret);
        }
    }

    schedule_delayed_work(&upm->psy_dwork, 0);
    enable_irq(upm->client->irq);
}
/* hs14 code for AL6528ADEU-722 by qiaodan at 2022/11/03 end */
/* hs14 code for AL6528ADEU-2768 by qiaodan at 2022/11/25 start */
static int upm6910_set_hiz_mode(struct charger_device *chg_dev, bool en)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
    int ret = 0;

    if (en) {
        ret += upm6910_set_acovp_threshold(upm, VAC_OVP_6500);
        ret += upm6910_set_input_volt_limit(upm, HIZ_CONTROL_VINDPM);
        upm->hiz_mode_flag = true;
    } else {
        upm->hiz_mode_flag = false;
        ret += upm6910_set_acovp_threshold(upm, VAC_OVP_10500);
        ret += upm6910_set_input_volt_limit(upm, POWER_DETECT_VINDPM);
    }

    /*
    if (en)
        ret = upm6910_enter_hiz_mode(upm);
    else
        ret = upm6910_exit_hiz_mode(upm);
    */

    pr_err("%s hiz mode %s\n", en ? "enable" : "disable",
            !ret ? "successfully" : "failed");

    return ret;
}
EXPORT_SYMBOL_GPL(upm6910_set_hiz_mode);

static int upm6910_get_hiz_mode(struct charger_device *chg_dev)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

    /*
    ret = upm6910_read_byte(upm, UPM6910_REG_00, &val);
    if (ret == 0){
        pr_err("Reg[%.2x] = 0x%.2x\n", UPM6910_REG_00, val);
    }
    */

    pr_err("%s:hiz mode %s\n",__func__, upm->hiz_mode_flag ? "enabled" : "disabled");

    return upm->hiz_mode_flag;
}
EXPORT_SYMBOL_GPL(upm6910_get_hiz_mode);
/* hs14 code for AL6528ADEU-2768 by qiaodan at 2022/11/25 end */
/* hs14 code for SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 start*/
static int upm6910_set_shipmode(struct charger_device *chg_dev, bool en)
{
    int ret = 0;
    u8 val;
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
    /* hs14 code for AL6528ADEU-2944 by qiaodan at 2022/11/29 start */
    if (en) {
        ret += upm6910_enter_hiz_mode(upm);
        val = REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT;
        ret += upm6910_update_bits(upm, UPM6910_REG_07, REG07_BATFET_DIS_MASK, val);
    } else {
        val = REG07_BATFET_ON << REG07_BATFET_DIS_SHIFT;
        ret = upm6910_update_bits(upm, UPM6910_REG_07, REG07_BATFET_DIS_MASK, val);
    }
    /* hs14 code for AL6528ADEU-2944 by qiaodan at 2022/11/29 end */

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
        return ret;
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
/* hs14 code for SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 end*/

/* hs14 code for  SR-AL6528A-01-339 by shanxinkai at 2022/09/30 start*/
static int upm6910_get_charging_status(struct charger_device *chg_dev,
    int *chg_stat)
{
    int ret;
    u8 val;
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

    ret = upm6910_read_byte(upm, UPM6910_REG_08, &val);
    if (!ret) {
        val = val & REG08_CHRG_STAT_MASK;
        val = val >> REG08_CHRG_STAT_SHIFT;
        *chg_stat = val;
    }

    return ret;
}
/* hs14 code for  SR-AL6528A-01-339 by shanxinkai at 2022/09/30 end*/

static irqreturn_t upm6910_irq_handler(int irq, void *data)
{
    int ret;
    u8 reg_val;
    bool prev_pg;
    bool prev_vbus_gd;
    /* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
    bool hiz_irq = false;
    /* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */

    struct upm6910 *upm = (struct upm6910 *)data;

    /* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 start */
    if (upm->bypass_chgdet_en == true) {
        pr_err("%s:bypass_chgdet_en=%d, skip bc12\n", __func__, upm->bypass_chgdet_en);
        return IRQ_HANDLED;
    }
    /* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 end */

    ret = upm6910_read_byte(upm, UPM6910_REG_0A, &reg_val);
    if (ret)
        return IRQ_HANDLED;

    prev_vbus_gd = upm->vbus_gd;

    upm->vbus_gd = !!(reg_val & REG0A_VBUS_GD_MASK);

    ret = upm6910_read_byte(upm, UPM6910_REG_08, &reg_val);
    if (ret)
        return IRQ_HANDLED;

    prev_pg = upm->power_good;

    upm->power_good = !!(reg_val & REG08_PG_STAT_MASK);

    /* hs14 code for SR-AL6528A-01-321|AL6528ADEU-28 by gaozhengwei at 2022/09/29 start */
    if (!prev_vbus_gd && upm->vbus_gd) {
        /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 start */
        upm->vbus_stat = true;
        /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 end */
        pr_notice("adapter/usb inserted\n");
        Charger_Detect_Init();
    } else if (prev_vbus_gd && !upm->vbus_gd) {
        /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 start */
        upm->vbus_stat = false;
        /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 end */
        hiz_irq = is_hiz_mode_trigger_interrupt(upm);
        if (hiz_irq) {
            pr_notice("hiz mode trigger interrupt\n");
            upm->vbus_gd = true;
            upm->power_good = true;
            return IRQ_HANDLED;
        }
        /*hs14 code for AL6528ADEU-2064 by lina at 2022/11/17 start */
        cancel_delayed_work(&upm->bc12_recheck_dwork);
        /*hs14 code for AL6528ADEU-2064 by lina at 2022/11/17 end*/
        upm->psy_usb_type = CHARGER_UNKNOWN;
        schedule_delayed_work(&upm->psy_dwork, 0);
        /* hs14 code for AL6528ADEU-3951 by qiaodan at 2023/04/07 start */
        upm6910_chargecurrent_recheck(upm);
        /* hs14 code for AL6528ADEU-3951 by qiaodan at 2023/04/07 end */
        pr_notice("adapter/usb removed\n");
        Charger_Detect_Release();
        return IRQ_HANDLED;
    }
    /* hs14 code for SR-AL6528A-01-321|AL6528ADEU-28 by gaozhengwei at 2022/09/29 end */

    /* hs14 code for AL6528ADEU-722 by qiaodan at 2022/10/20 start */
    if (!prev_pg && upm->power_good) {
        ret = upm6910_get_charger_type(upm, &upm->psy_usb_type);
        if (upm->psy_usb_type == NONSTANDARD_CHARGER) {
            pr_err("start charger_redetect\n");
            /*hs14 code for AL6528ADEU-2064 by lina at 2022/11/17 start */
            upm->psy_usb_type = STANDARD_CHARGER;
            schedule_delayed_work(&upm->bc12_recheck_dwork, msecs_to_jiffies(3*1000));
            ret = upm6910_enable_hvdcp(upm, true);
            if (ret)
                pr_err("Failed to en HVDCP, ret = %d\n", ret);

            ret = upm6910_set_dp(upm, REG0C_DPDM_OUT_0P6V);
            if (ret)
                pr_err("Failed to set dp 0.6v, ret = %d\n", ret);
        }
        schedule_delayed_work(&upm->psy_dwork, 0);
        /*hs14 code for AL6528ADEU-2064 by lina at 2022/11/17 end*/
    }
    /* hs14 code for AL6528ADEU-722 by qiaodan at 2022/10/20 end */

    return IRQ_HANDLED;
}

static int upm6910_register_interrupt(struct upm6910 *upm)
{
    int ret = 0;
    ret = devm_gpio_request_one(upm->dev, upm->intr_gpio, GPIOF_DIR_IN,
            devm_kasprintf(upm->dev, GFP_KERNEL,
            "upm6910_intr_gpio.%s", dev_name(upm->dev)));
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
    pr_info("%s irq = %d\n", __func__, upm->client->irq);

    ret = devm_request_threaded_irq(upm->dev, upm->client->irq, NULL,
                    upm6910_irq_handler,
                    IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                    "upm_irq", upm);
    if (ret < 0) {
        pr_err("request thread irq failed:%d\n", ret);
        return ret;
    }

    enable_irq_wake(upm->irq);

    return 0;
}

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

    /* hs14 code for AL6528ADEU-1863 by wenyaqi at 2022/11/03 start */
    ret= upm6910_disable_safety_timer(upm);
    if (ret)
        pr_err("Failed to set safety_timer stop, ret = %d\n", ret);
    /* hs14 code for AL6528ADEU-1863 by wenyaqi at 2022/11/03 end */

    ret = upm6910_set_int_mask(upm,
                   REG0A_IINDPM_INT_MASK |
                   REG0A_VINDPM_INT_MASK);
    if (ret)
        pr_err("Failed to set vindpm and iindpm int mask\n");
    /* hs14 code for SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 start*/
    ret = upm6910_disable_battfet_rst(upm);
    if (ret)
        pr_err("Failed to disable_battfet\n");
    /* hs14 code for SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 end*/
    return 0;
}

static void upm6910_inform_prob_dwork_handler(struct work_struct *work)
{
    struct upm6910 *upm = container_of(work, struct upm6910,
                                prob_dwork.work);

    /* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
    Charger_Detect_Init();
    msleep(50);
    upm6910_dpdm_set_hidden_mode(upm);
    msleep(700);
    upm6910_force_dpdm(upm);
    msleep(300);
    /* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */

    upm6910_irq_handler(upm->irq, (void *) upm);
}

static int upm6910_detect_device(struct upm6910 *upm)
{
    int ret;
    u8 data;

    ret = upm6910_read_byte(upm, UPM6910_REG_0B, &data);
    if (!ret) {
        upm->part_no = (data & REG0B_PN_MASK) >> REG0B_PN_SHIFT;
        upm->revision =
            (data & REG0B_DEV_REV_MASK) >> REG0B_DEV_REV_SHIFT;
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

static int upm6910_plug_out(struct charger_device *chg_dev)
{
    int ret;

    ret = upm6910_charging(chg_dev, false);

    if (ret)
        pr_err("Failed to disable charging:%d\n", ret);

    return ret;
}

/* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 start */
static int upm6910_bypass_chgdet(struct charger_device *chg_dev, bool bypass_chgdet_en)
{
    int ret = 0;
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

    upm->bypass_chgdet_en = bypass_chgdet_en;
    dev_info(upm->dev, "%s bypass_chgdet_en = %d\n", __func__, bypass_chgdet_en);

    return ret;
}
/* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 end */

/* hs14 code for SR-AL6528A-01-255|P221117-03133 by wenyaqi at 2022/11/24 start */
static int upm6910_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
    int ret = 0;
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
    union power_supply_propval propval;

    dev_info(upm->dev, "%s en = %d\n", __func__, en);

    if (!upm->psy) {
        upm->psy = power_supply_get_by_name("charger");
        if (!upm->psy) {
            pr_err("%s Couldn't get psy\n", __func__);
            return -ENODEV;
        }
    }

    if (en == false) {
        propval.intval = 0;
    } else {
        propval.intval = 1;
    }

    ret = power_supply_set_property(upm->psy,
                    POWER_SUPPLY_PROP_ONLINE,
                    &propval);
    if (ret < 0)
        pr_notice("inform power supply online failed:%d\n", ret);

    if (en == false) {
        propval.intval = CHARGER_UNKNOWN;
    } else {
        propval.intval = upm->psy_usb_type;
    }

    ret = power_supply_set_property(upm->psy,
                    POWER_SUPPLY_PROP_CHARGE_TYPE,
                    &propval);
    if (ret < 0)
        pr_notice("inform power supply charge type failed:%d\n", ret);

    return ret;
}
/* hs14 code for SR-AL6528A-01-255|P221117-03133 by wenyaqi at 2022/11/24 end */

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
    /* hs14 code for AL6528ADEU-3951 by qiaodan at 2023/04/07 start */
    if (!upm) {
        pr_err("%s device is null\n", __func__);
        return -ENODEV;
    }

    if (!upm->power_good) {
        curr = ICHG_OCP_THRESHOLD_MA * 1000;
    }
    /* hs14 code for AL6528ADEU-3951 by qiaodan at 2023/04/07 end */

    pr_err("charge curr = %d\n", curr);

    return upm6910_set_chargecurrent(upm, curr / 1000);
}

static int upm6910_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
    *curr = 60 * 1000;

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
        vchg = vchg * REG04_VREG_LSB + REG04_VREG_BASE;
        *volt = vchg * 1000;
    }

    return ret;
}

static int upm6910_set_ivl(struct charger_device *chg_dev, u32 volt)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

    pr_err("vindpm volt = %d\n", volt);
    /* hs14 code for AL6528ADEU-2768 by qiaodan at 2022/11/25 start */
    if (upm->hiz_mode_flag == true) {
        pr_err("%s now in hiz mode, return", __func__);
        return 0;
    }
    /* hs14 code for AL6528ADEU-2768 by qiaodan at 2022/11/25 end */
    return upm6910_set_input_volt_limit(upm, volt / 1000);

}

static int upm6910_set_icl(struct charger_device *chg_dev, u32 curr)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

    pr_err("indpm curr = %d\n", curr);

    return upm6910_set_input_current_limit(upm, curr / 1000);
}

/* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/15 start */
static int upm6910_get_ibus(struct charger_device *chg_dev, u32 *curr)
{
    /*return 1650mA as ibus*/
    *curr = PD_INPUT_CURRENT;

    return 0;
}
/* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/15 end */

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

static int upm6910_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
    int ret;

    pr_err("otg curr = %d\n", curr);

    ret = upm6910_set_boost_current(upm, curr / 1000);

    return ret;
}

/* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 start */
static int upm6910_get_vbus_status(struct charger_device *chg_dev)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

    return upm->vbus_stat;
}

static int upm6910_dynamic_set_hwovp_threshold(struct charger_device *chg_dev,
                    int adapter_type)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
    u8 val = 0;
    /* hs14 code for AL6528ADEU-2768 by qiaodan at 2022/11/25 start */
    if (upm->hiz_mode_flag == true) {
        pr_err("%s now in hiz mode, return", __func__);
        return 0;
    }
    /* hs14 code for AL6528ADEU-2768 by qiaodan at 2022/11/25 end */
    /* hs14 code for AL6528A-677 by qiaodan at 2022/11/10 start */
    dev_info(upm->dev, "%s adapter_type =%d\n", __func__, adapter_type);
    /* hs14 code for AL6528A-677 by qiaodan at 2022/11/10 end */

    if (adapter_type == SC_ADAPTER_NORMAL)
        val = REG06_OVP_6P5V;
    else if (adapter_type == SC_ADAPTER_HV)
        val = REG06_OVP_10P5V;

    return upm6910_update_bits(upm, UPM6910_REG_06, REG06_OVP_MASK,
                   val << REG06_OVP_SHIFT);
}
/* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 end */

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
    .is_charging_done = upm6910_is_charging_done,
    .get_min_charging_current = upm6910_get_min_ichg,

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

   /* Event */
    .event = upm6910_do_event,

   /* HIZ */
    .set_hiz_mode = upm6910_set_hiz_mode,
    .get_hiz_mode = upm6910_get_hiz_mode,

    /* ADC */
    .get_tchg_adc = NULL,
    /* hs14 code for SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 start*/
    .get_ship_mode = upm6910_get_shipmode,
    .set_ship_mode = upm6910_set_shipmode,
    /* hs14 code for SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 end*/
    /* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/15 start */
    .get_ibus_adc = upm6910_get_ibus,
    /* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/15 end */

    /* hs14 code for  SR-AL6528A-01-339 by shanxinkai at 2022/09/30 start*/
    .get_chr_status = upm6910_get_charging_status,
    /* hs14 code for  SR-AL6528A-01-339 by shanxinkai at 2022/09/30 end*/

    /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 start */
    .get_vbus_status = upm6910_get_vbus_status,
    .dynamic_set_hwovp_threshold = upm6910_dynamic_set_hwovp_threshold,
    /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 end */

    /* hs14 code for SR-AL6528A-01-255 by wenyaqi at 2022/10/26 start */
    /* charger type detection */
    .enable_chg_type_det = upm6910_enable_chg_type_det,
    /* hs14 code for SR-AL6528A-01-255 by wenyaqi at 2022/10/26 end */
    /* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 start */
    .bypass_chgdet = upm6910_bypass_chgdet,
    /* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 end */
};

static struct of_device_id upm6910_charger_match_table[] = {
    {
     .compatible = "upm,upm6910d",
     .data = &pn_data[PN_UPM6910D],
     },
    {},
};
MODULE_DEVICE_TABLE(of, upm6910_charger_match_table);


static int upm6910_charger_remove(struct i2c_client *client);
static int upm6910_charger_probe(struct i2c_client *client,
                 const struct i2c_device_id *id)
{
    struct upm6910 *upm;
    const struct of_device_id *match;
    struct device_node *node = client->dev.of_node;

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

    if (upm->part_no != *(int *)match->data) {
        pr_info("part no mismatch, hw:%s, devicetree:%s\n",
            pn_str[upm->part_no], pn_str[*(int *) match->data]);
        upm6910_charger_remove(client);
        return -EINVAL;
    }

    upm->platform_data = upm6910_parse_dt(node, upm);
    /* hs14 code for AL6528ADEU-2768 by qiaodan at 2022/11/25 start */
    upm->hiz_mode_flag = false;
    /* hs14 code for AL6528ADEU-2768 by qiaodan at 2022/11/25 end */

    INIT_DELAYED_WORK(&upm->psy_dwork, upm6910_inform_psy_dwork_handler);
    /* hs14 code for AL6528ADEU-722 by qiaodan at 2022/10/20 start */
    INIT_DELAYED_WORK(&upm->bc12_recheck_dwork, upm6910_bc12_recheck_dwork_handler);
    /* hs14 code for AL6528ADEU-722 by qiaodan at 2022/10/20 end */
    INIT_DELAYED_WORK(&upm->prob_dwork, upm6910_inform_prob_dwork_handler);
    /* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 start */
    upm->bypass_chgdet_en = false;
    /* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 end */

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

    if (!upm->usb_psy) {
        upm->usb_psy = power_supply_get_by_name("usb");
        if (!upm->usb_psy) {
            pr_err("%s Couldn't get usb psy\n", __func__);
        }
    }

    ret = sysfs_create_group(&upm->dev->kobj, &upm6910_attr_group);
    if (ret)
        dev_err(upm->dev, "failed to register sysfs. err: %d\n", ret);

    mod_delayed_work(system_wq, &upm->prob_dwork,
                    msecs_to_jiffies(2*1000));

    pr_err("upm6910 probe successfully, Part Num:%d, Revision:%d\n!",
           upm->part_no, upm->revision);

    /* hs14 code for SR-AL6528A-01-258 by shanxinkai at 2022/09/13 start */
    chg_info = UPM6910;
    /* hs14 code for SR-AL6528A-01-258 by shanxinkai at 2022/09/13 end */

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

MODULE_DESCRIPTION("TI BQ2560x Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Texas Instruments");
