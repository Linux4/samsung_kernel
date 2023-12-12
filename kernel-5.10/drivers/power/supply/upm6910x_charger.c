// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2023 unisemipower.
 */

#include <linux/types.h>
#include <linux/init.h> /* For init/exit macros */
#include <linux/module.h> /* For MODULE_ marcros  */
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#endif
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include "upm6910x_reg.h"
#include "charger_class.h"
#include "mtk_charger.h"
/*Tab A9 code for SR-AX6739A-01-490 by qiaodan at 20230504 start*/
#include <linux/power/gxy_psy_sysfs.h>


/**********************************************************
 *
 *   [extern for other module]
 *
 *********************************************************/
extern void gxy_bat_set_chginfo(enum gxy_bat_chg_info cinfo_data);
/*Tab A9 code for SR-AX6739A-01-490 by qiaodan at 20230504 end*/

/**********************************************************
 *
 *   [I2C Slave Setting]
 *
 *********************************************************/

#define UPM6910_REG_NUM (0xC)

/* UPM6910 REG06 BOOST_LIM[5:4], uV */
static const unsigned int BOOST_VOLT_LIMIT[] = { 4850000, 5000000, 5150000,
                         5300000 };

static const unsigned int BOOST_CURRENT_LIMIT[] = { 500000, 1200000 };

static enum power_supply_usb_type upm6910x_usb_type[] = {
    POWER_SUPPLY_USB_TYPE_UNKNOWN,
    POWER_SUPPLY_USB_TYPE_SDP,
    POWER_SUPPLY_USB_TYPE_CDP,
    POWER_SUPPLY_USB_TYPE_DCP,
};

static const struct charger_properties upm6910x_chg_props = {
    .alias_name = UPM6910_NAME,
};

/**********************************************************
 *
 *   [Global Variable]
 *
 *********************************************************/
static struct power_supply_desc upm6910x_power_supply_desc;
static struct charger_device *s_chg_dev_otg;

/**********************************************************
 *
 *   [I2C Function For Read/Write upm6910x]
 *
 *********************************************************/
static int __upm6910x_read_byte(struct upm6910x_device *upm, u8 reg, u8 *data)
{
    int ret = 0;

    ret = i2c_smbus_read_byte_data(upm->client, reg);
    if (ret < 0) {
        pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
        return ret;
    }

    *data = (u8)ret;

    return 0;
}

static int __upm6910x_write_byte(struct upm6910x_device *upm, int reg, u8 val)
{
    int ret = 0;

    ret = i2c_smbus_write_byte_data(upm->client, reg, val);
    if (ret < 0) {
        pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
               val, reg, ret);
        return ret;
    }

    return 0;
}

static int upm6910x_read_reg(struct upm6910x_device *upm, u8 reg, u8 *data)
{
    int ret = 0;

    mutex_lock(&upm->i2c_rw_lock);
    ret = __upm6910x_read_byte(upm, reg, data);
    mutex_unlock(&upm->i2c_rw_lock);

    return ret;
}

/*Tab A9 code for AX6739A-2076 by hualei at 20230707 start*/
static int upm6910x_write_byte(struct upm6910x_device *upm, u8 reg, u8 data)
{
    int ret = 0;

    mutex_lock(&upm->i2c_rw_lock);
    ret = __upm6910x_write_byte(upm, reg, data);
    mutex_unlock(&upm->i2c_rw_lock);

    return ret;
}
/*Tab A9 code for AX6739A-2076 by hualei at 20230707 end*/

static int upm6910x_update_bits(struct upm6910x_device *upm, u8 reg, u8 mask,
                u8 val)
{
    int ret = 0;
    u8 tmp = 0;

    mutex_lock(&upm->i2c_rw_lock);
    ret = __upm6910x_read_byte(upm, reg, &tmp);
    if (ret) {
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
        goto out;
    }

    tmp &= ~mask;
    tmp |= val & mask;

    ret = __upm6910x_write_byte(upm, reg, tmp);
    if (ret) {
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
    }

out:
    mutex_unlock(&upm->i2c_rw_lock);
    return ret;
}

/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
static int upm6910x_set_watchdog_timer(struct upm6910x_device *upm, int time)
{
    int ret = 0;
    u8 reg_val = 0;

    if (time == 0) {
        reg_val = UPM6910_WDT_TIMER_DISABLE;
    } else if (time == 40) {
        reg_val = UPM6910_WDT_TIMER_40S;
    } else if (time == 80) {
        reg_val = UPM6910_WDT_TIMER_80S;
    } else {
        reg_val = UPM6910_WDT_TIMER_160S;
    }

    ret = upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_5,
                   UPM6910_WDT_TIMER_MASK, reg_val);

    return ret;
}

static int upm6910x_set_term_curr(struct charger_device *chg_dev, u32 uA)
{
    struct upm6910x_device *upm = charger_get_data(chg_dev);
    u8 reg_val = 0;

    if (uA < UPM6910_TERMCHRG_I_MIN_uA) {
        uA = UPM6910_TERMCHRG_I_MIN_uA;
    } else if (uA > UPM6910_TERMCHRG_I_MAX_uA) {
        uA = UPM6910_TERMCHRG_I_MAX_uA;
    }

    reg_val = (uA - UPM6910_TERMCHRG_I_MIN_uA) /
          UPM6910_TERMCHRG_CURRENT_STEP_uA;

    return upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_3,
                    UPM6910_TERMCHRG_CUR_MASK, reg_val);
}

static int upm6910x_set_prechrg_curr(struct upm6910x_device *upm, int uA)
{
    u8 reg_val = 0;

    if (uA < UPM6910_PRECHRG_I_MIN_uA) {
        uA = UPM6910_PRECHRG_I_MIN_uA;
    } else if (uA > UPM6910_PRECHRG_I_MAX_uA) {
        uA = UPM6910_PRECHRG_I_MAX_uA;
    }

    reg_val = (uA - UPM6910_PRECHRG_I_MIN_uA) /
          UPM6910_PRECHRG_CURRENT_STEP_uA;

    return upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_3,
                    UPM6910_PRECHRG_CUR_MASK, reg_val << UPM6910_PRECHRG_CUR_SHIFT);
}

static int upm6910x_get_ichg_curr(struct charger_device *chg_dev, u32 *uA)
{
    struct upm6910x_device *upm = charger_get_data(chg_dev);
    int ret = 0;
    u8 ichg = 0;
    u32 curr = 0;

    ret = upm6910x_read_reg(upm, UPM6910_CHRG_CTRL_2, &ichg);
    if (ret) {
        return ret;
    }

    ichg &= UPM6910_ICHRG_I_MASK;

    curr = ichg * UPM6910_ICHRG_I_STEP_uA;

    *uA = curr;

    return 0;
}

static int upm6910x_get_minichg_curr(struct charger_device *chg_dev, u32 *uA)
{
    *uA = UPM6910_ICHRG_I_MIN_uA;

    return 0;
}

/*Tab A9 code for AX6739A-1460 by hualei at 20230625 start*/
static int upm6910x_set_ichrg(struct upm6910x_device *upm, unsigned int val)
{
    int ret = 0;
    u8 reg_val = 0;

    if (val == UPM6910_ICHRG_I_LIMIT) {
        reg_val = val / UPM6910_ICHRG_I_STEP_uA;
        ret = upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_2, UPM6910_ICHRG_I_MASK, reg_val);
        pr_notice("[%s] set icc limit = %d", __func__, val);
    }

    return ret;
}
/*Tab A9 code for AX6739A-1460 by hualei at 20230625 end*/

static int upm6910x_set_ichrg_curr(struct charger_device *chg_dev,
                   unsigned int uA)
{
    struct upm6910x_device *upm = charger_get_data(chg_dev);
    int ret = 0;
    u8 reg_val = 0;

    if (uA < UPM6910_ICHRG_I_MIN_uA) {
        uA = UPM6910_ICHRG_I_MIN_uA;
    } else if (uA > upm->init_data.max_ichg) {
        uA = upm->init_data.max_ichg;
    }

    pr_notice("[%s] value = %duA", __func__, uA);

    reg_val = uA / UPM6910_ICHRG_I_STEP_uA;

    ret = upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_2,
                   UPM6910_ICHRG_I_MASK, reg_val);

    return ret;
}

static int upm6910x_set_chrg_volt(struct charger_device *chg_dev, u32 uV)
{
    struct upm6910x_device *upm = charger_get_data(chg_dev);
    int ret = 0;
    int reg_val = 0;

    if (uV < UPM6910_VREG_V_MIN_uV) {
        uV = UPM6910_VREG_V_MIN_uV;
    } else if (uV > upm->init_data.max_vreg) {
        uV = upm->init_data.max_vreg;
    }

    pr_notice("[%s] value = %duV", __func__, uV);

    if (uV == UPM6910_SPECIAL_CHG_VOL_4200) {
        reg_val = UPM6910_SPECIAL_CHG_VOL_4200_VAL;
    } else if (uV == UPM6910_SPECIAL_CHG_VOL_4290) {
        reg_val = UPM6910_SPECIAL_CHG_VOL_4290_VAL;
    } else if (uV == UPM6910_SPECIAL_CHG_VOL_4340) {
        reg_val = UPM6910_SPECIAL_CHG_VOL_4340_VAL;
    } else if (uV == UPM6910_SPECIAL_CHG_VOL_4360) {
        reg_val = UPM6910_SPECIAL_CHG_VOL_4360_VAL;
    } else if (uV == UPM6910_SPECIAL_CHG_VOL_4380) {
        reg_val = UPM6910_SPECIAL_CHG_VOL_4380_VAL;
    } else if (uV == UPM6910_SPECIAL_CHG_VOL_4400) {
        reg_val = UPM6910_SPECIAL_CHG_VOL_4400_VAL;
    } else {
        reg_val = (uV - UPM6910_VREG_V_MIN_uV) / UPM6910_VREG_V_STEP_uV;
    }

    ret = upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_4,
                   UPM6910_VREG_V_MASK, reg_val << UPM6910_VREG_V_SHIFT);

    return ret;
}

static int upm6910x_get_chrg_volt(struct charger_device *chg_dev,
                  unsigned int *volt)
{
    struct upm6910x_device *upm = charger_get_data(chg_dev);
    int ret = 0;
    u8 vreg_val = 0;

    ret = upm6910x_read_reg(upm, UPM6910_CHRG_CTRL_4, &vreg_val);
    if (ret) {
        return ret;
    }

    vreg_val = (vreg_val & UPM6910_VREG_V_MASK) >> 3;
    if (vreg_val == UPM6910_SPECIAL_CHG_VOL_4200_VAL) {
        *volt = UPM6910_SPECIAL_CHG_VOL_4200;
    } else if (vreg_val == UPM6910_SPECIAL_CHG_VOL_4290_VAL) {
        *volt = UPM6910_SPECIAL_CHG_VOL_4290;
    } else if (vreg_val == UPM6910_SPECIAL_CHG_VOL_4340_VAL) {
        *volt = UPM6910_SPECIAL_CHG_VOL_4340;
    } else if (vreg_val == UPM6910_SPECIAL_CHG_VOL_4360_VAL) {
        *volt = UPM6910_SPECIAL_CHG_VOL_4360;
    } else if (vreg_val == UPM6910_SPECIAL_CHG_VOL_4380_VAL) {
        *volt = UPM6910_SPECIAL_CHG_VOL_4380;
    } else if (vreg_val == UPM6910_SPECIAL_CHG_VOL_4400_VAL) {
        *volt = UPM6910_SPECIAL_CHG_VOL_4400;
    } else {
        *volt = vreg_val * UPM6910_VREG_V_STEP_uV +
            UPM6910_VREG_V_MIN_uV;
    }

    return 0;
}

static int upm6910x_set_vac_ovp(struct upm6910x_device *upm, int volt)
{
    int ret = 0;
    u8 val = 0;

    if (volt == VAC_OVP_14000) {
        val = REG06_OVP_14P0V;
    } else if (volt == VAC_OVP_10500) {
        val = REG06_OVP_10P5V;
    } else if (volt == VAC_OVP_6500) {
        val = REG06_OVP_6P5V;
    } else {
        val = REG06_OVP_5P5V;
    }

    pr_notice("[%s] value = %d", __func__, val);

    ret = upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_6, REG06_OVP_MASK,
                   val << REG06_OVP_SHIFT);

    return ret;
}

static int upm6910x_set_input_volt_lim(struct charger_device *chg_dev,
                       unsigned int vindpm)
{
    struct upm6910x_device *upm = charger_get_data(chg_dev);
    int ret = 0;
    u8 reg_val = 0;

    if (vindpm < UPM6910_VINDPM_V_MIN_uV) {
        vindpm = UPM6910_VINDPM_V_MIN_uV;
    } else if (vindpm > UPM6910_VINDPM_V_MAX_uV) {
        vindpm = UPM6910_VINDPM_V_MAX_uV;
    }

    pr_notice("[%s] value = %d", __func__, vindpm);

    reg_val = (vindpm - UPM6910_VINDPM_V_MIN_uV) / UPM6910_VINDPM_STEP_uV;
    ret = upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_6,
                   UPM6910_VINDPM_V_MASK, reg_val);

    return ret;
}

static int upm6910x_get_input_volt_lim(struct charger_device *chg_dev, u32 *uV)
{
    struct upm6910x_device *upm = charger_get_data(chg_dev);
    int ret = 0;
    u8 vlim = 0;

    ret = upm6910x_read_reg(upm, UPM6910_CHRG_CTRL_6, &vlim);
    if (ret) {
        return ret;
    }

    *uV = UPM6910_VINDPM_V_MIN_uV + (vlim & 0x0F) * UPM6910_VINDPM_STEP_uV;

    return 0;
}

/*Tab A9 code for AX6739A-2299 by hualei at 20230713 start*/
static int upm6910x_set_input_curr_lim(struct charger_device *chg_dev,
                       unsigned int iindpm)
{
    struct upm6910x_device *upm = charger_get_data(chg_dev);
    int ret = 0;
    u8 reg_val = 0;

    upm->pre_limit = iindpm;

    if (iindpm < UPM6910_IINDPM_I_MIN_uA) {
        iindpm = UPM6910_IINDPM_I_MIN_uA;
    } else if (iindpm > UPM6910_IINDPM_I_MAX_uA) {
        iindpm = UPM6910_IINDPM_I_MAX_uA;
    }

    pr_notice("[%s] value = %d", __func__, iindpm);

    reg_val = (iindpm - UPM6910_IINDPM_I_MIN_uA) / UPM6910_IINDPM_STEP_uA;

    ret = upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_0,
                   UPM6910_IINDPM_I_MASK, reg_val);
    return ret;
}
/*Tab A9 code for AX6739A-2299 by hualei at 20230713 end*/

/*Tab A9 code for AX6739A-2299 by hualei at 20230713 start*/
static int upm6910x_set_input_curr_lim_irq(struct upm6910x_device *upm,
                       unsigned int iindpm)
{
    int ret = 0;
    u8 reg_val = 0;

    if (iindpm < UPM6910_IINDPM_I_MIN_uA) {
        iindpm = UPM6910_IINDPM_I_MIN_uA;
    } else if (iindpm > UPM6910_IINDPM_I_MAX_uA) {
        iindpm = UPM6910_IINDPM_I_MAX_uA;
    }

    pr_notice("[%s] value = %d", __func__, iindpm);

    reg_val = (iindpm - UPM6910_IINDPM_I_MIN_uA) / UPM6910_IINDPM_STEP_uA;

    ret = upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_0,
                   UPM6910_IINDPM_I_MASK, reg_val);
    return ret;

}
/*Tab A9 code for AX6739A-2299 by hualei at 20230713 end*/

/*Tab A9 code for AX6739A-2299 by hualei at 20230713 start*/
static int upm6910x_get_input_curr_lim_irq(struct upm6910x_device *upm,
                       unsigned int *ilim)
{
    int ret = 0;
    u8 reg_val = 0;

    ret = upm6910x_read_reg(upm, UPM6910_CHRG_CTRL_0, &reg_val);
    if (ret) {
        return ret;
    }

    *ilim = (reg_val & UPM6910_IINDPM_I_MASK) * UPM6910_IINDPM_STEP_uA +
        UPM6910_IINDPM_I_MIN_uA;

    pr_notice("[%s] ilim = %d", __func__, *ilim);

    return 0;
}
/*Tab A9 code for AX6739A-2299 by hualei at 20230713 end*/

/*Tab A9 code for AX6739A-516|AX6739A-2076 by hualei at 20230707 start*/
static int upm6910x_chg_attach_pre_process(struct charger_device *chg_dev,
                    int attach)
{
    struct upm6910x_device *upm = charger_get_data(chg_dev);
    upm->typec_attached = attach;
    mutex_lock(&upm->lock);

    switch(attach) {
        case ATTACH_TYPE_TYPEC:
            schedule_delayed_work(&upm->force_detect_dwork, msecs_to_jiffies(220));
            break;
        case ATTACH_TYPE_PD_SDP:
            upm->psy_type = POWER_SUPPLY_TYPE_USB;
            upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
            break;
        case ATTACH_TYPE_PD_DCP:
            upm->psy_type = POWER_SUPPLY_TYPE_USB_DCP;
            upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
            break;
        case ATTACH_TYPE_PD_NONSTD:
            upm->psy_type = POWER_SUPPLY_TYPE_USB;
            upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
            break;
        case ATTACH_TYPE_NONE:
            upm->psy_type = POWER_SUPPLY_TYPE_UNKNOWN;
            upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
            break;
        default:
            dev_info(upm->dev, "%s: using tradtional bc12 flow!\n", __func__);
            break;
    }

    power_supply_changed(upm->charger);

    dev_err(upm->dev, "%s: type(%d %d),attach:%d\n", __func__,
        upm->psy_type, upm->psy_usb_type, attach);
    mutex_unlock(&upm->lock);

    return 0;
}
/*Tab A9 code for AX6739A-516|AX6739A-2076 by hualei at 20230707 end*/

static int upm6910x_get_input_curr_lim(struct charger_device *chg_dev,
                       unsigned int *ilim)
{
    struct upm6910x_device *upm = charger_get_data(chg_dev);
    int ret = 0;
    u8 reg_val = 0;

    ret = upm6910x_read_reg(upm, UPM6910_CHRG_CTRL_0, &reg_val);
    if (ret) {
        return ret;
    }

    *ilim = (reg_val & UPM6910_IINDPM_I_MASK) * UPM6910_IINDPM_STEP_uA +
        UPM6910_IINDPM_I_MIN_uA;

    return 0;
}

static int upm6910x_get_input_mincurr_lim(struct charger_device *chg_dev,
                      u32 *ilim)
{
    *ilim = UPM6910_IINDPM_I_MIN_uA;

    return 0;
}

/*Tab A9 code for AX6739A-765 by hualei at 20230612 start*/
static int upm6910x_get_vreg(struct upm6910x_device *upm,  unsigned int *volt_uv)
{
    int ret = 0;
    u8 vreg_val = 0;

    ret = upm6910x_read_reg(upm, UPM6910_CHRG_CTRL_4, &vreg_val);

    vreg_val = (vreg_val & UPM6910_VREG_V_MASK) >> 3;
    if (vreg_val == UPM6910_SPECIAL_CHG_VOL_4200_VAL) {
        *volt_uv = UPM6910_SPECIAL_CHG_VOL_4200;
    } else if (vreg_val == UPM6910_SPECIAL_CHG_VOL_4290_VAL) {
        *volt_uv = UPM6910_SPECIAL_CHG_VOL_4290;
    } else if (vreg_val == UPM6910_SPECIAL_CHG_VOL_4340_VAL) {
        *volt_uv = UPM6910_SPECIAL_CHG_VOL_4340;
    } else if (vreg_val == UPM6910_SPECIAL_CHG_VOL_4360_VAL) {
        *volt_uv = UPM6910_SPECIAL_CHG_VOL_4360;
    } else if (vreg_val == UPM6910_SPECIAL_CHG_VOL_4380_VAL) {
        *volt_uv = UPM6910_SPECIAL_CHG_VOL_4380;
    } else if (vreg_val == UPM6910_SPECIAL_CHG_VOL_4400_VAL) {
        *volt_uv = UPM6910_SPECIAL_CHG_VOL_4400;
    } else {
        *volt_uv = vreg_val * UPM6910_VREG_V_STEP_uV + UPM6910_VREG_V_MIN_uV;
    }
    return ret;
}
/*Tab A9 code for AX6739A-765 by hualei at 20230612 end*/

/*Tab A9 code for SR-AX6739A-01-494 by qiaodan at 20230506 start*/
static int upm6910x_get_chrg_stat(struct upm6910x_device *upm)
{
    u8 chrg_stat = 0;
    int ret = 0;
    /*Tab A9 code for AX6739A-2299 by hualei at 20230713 start*/
    int psy_stat_now = POWER_SUPPLY_STATUS_UNKNOWN;
    static int s_psy_stat_old = POWER_SUPPLY_STATUS_UNKNOWN;
    /*Tab A9 code for AX6739A-2299 by hualei at 20230713 end*/

    ret = upm6910x_read_reg(upm, UPM6910_CHRG_STAT, &chrg_stat);
    if (ret) {
        ret = upm6910x_read_reg(upm, UPM6910_CHRG_STAT, &chrg_stat);
        if (ret) {
            pr_err("%s read UPM6910_CHRG_STAT fail\n", __func__);
            return ret;
        }
    }

    /*Tab A9 code for AX6739A-2299 by hualei at 20230713 start*/
    chrg_stat = chrg_stat & UPM6910_CHG_STAT_MASK;
    if (!upm->state.chrg_type || (upm->state.chrg_type == UPM6910_OTG_MODE)) {
        psy_stat_now = POWER_SUPPLY_STATUS_DISCHARGING;
    } else {
        switch (chrg_stat) {
        case UPM6910_NOT_CHRGING:
            if (upm->chg_config) {
                psy_stat_now = POWER_SUPPLY_STATUS_CHARGING;
            } else {
                psy_stat_now = POWER_SUPPLY_STATUS_NOT_CHARGING;
            }
            break;
        case UPM6910_PRECHRG:
        case UPM6910_FAST_CHRG:
            psy_stat_now = POWER_SUPPLY_STATUS_CHARGING;
            break;
        case UPM6910_TERM_CHRG:
            psy_stat_now = POWER_SUPPLY_STATUS_FULL;
            break;
        }
    }

    if (s_psy_stat_old == POWER_SUPPLY_STATUS_NOT_CHARGING &&
        psy_stat_now == POWER_SUPPLY_STATUS_CHARGING) {
        power_supply_changed(upm->charger);
    }
    s_psy_stat_old = psy_stat_now;

    return psy_stat_now;
    /*Tab A9 code for AX6739A-2299 by hualei at 20230713 end*/
}
/*Tab A9 code for SR-AX6739A-01-494 by qiaodan at 20230506 end*/

/*Tab A9 code for SR-AX6739A-01-500 by wenyaqi at 20230515 start*/
static int upm6910_enable_hvdcp(struct upm6910x_device *upm, bool en)
{
    const u8 val = en << UPM6910_EN_HVDCP_SHIFT;

    return upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_C, UPM6910_EN_HVDCP_MASK, val);
}

static int upm6910_set_dp(struct upm6910x_device *upm, int dp_stat)
{
    const u8 val = dp_stat << UPM6910_DP_MUX_SHIFT;

    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
    schedule_delayed_work(&upm->hvdcp_done_work, msecs_to_jiffies(1500));
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/
    return upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_C, UPM6910_DP_MUX_MASK, val);
}
/*Tab A9 code for SR-AX6739A-01-500 by wenyaqi at 20230515 end*/

static int upm6910x_get_state(struct upm6910x_device *upm,
                  struct upm6910x_state *state)
{
    u8 chrg_stat = 0;
    u8 chrg_param_0 = 0;
    int ret = 0;

    ret = upm6910x_read_reg(upm, UPM6910_CHRG_STAT, &chrg_stat);
    if (ret) {
        ret = upm6910x_read_reg(upm, UPM6910_CHRG_STAT, &chrg_stat);
        if (ret) {
            pr_err("%s read UPM6910_CHRG_STAT fail\n", __func__);
            return ret;
        }
    }
    state->chrg_stat = chrg_stat & UPM6910_CHG_STAT_MASK;
    state->online = !!(chrg_stat & UPM6910_PG_STAT);
    pr_err("%s chrg_type =%d,chrg_stat =%d online = %d\n", __func__,
           state->chrg_type, state->chrg_stat, state->online);

    /* get vbus state*/
    ret = upm6910x_read_reg(upm, UPM6910_CHRG_CTRL_A, &chrg_param_0);
    if (ret) {
        pr_err("%s read UPM6910_CHRG_CTRL_A fail\n", __func__);
        return ret;
    }
    state->vbus_attach = !!(chrg_param_0 & UPM6910_VBUS_GOOD);
    pr_err("%s vbus_attach = %d\n", __func__, state->vbus_attach);

    return 0;
}


/*Tab A9 code for AX6739A-2076 by hualei at 20230707 start*/
static int upm6910x_set_en_ignorebc12(struct upm6910x_device *upm, bool enable)
{
    int ret = 0;

    if (enable) {
        upm6910x_write_byte(upm, UPM6910_CHRG_CTRL_A9, UPM6910_CHRG_SPECIAL_MODE);
        ret = upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_A7,
                    UPM6910_IGNOREBC_EN,
                    UPM6910_IGNOREBC_EN);
    } else {
        ret = upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_A7,
                    UPM6910_IGNOREBC_EN,
                    0);
        upm6910x_write_byte(upm, UPM6910_CHRG_CTRL_A9, 0);
    }

    return ret;
}
/*Tab A9 code for AX6739A-2076 by hualei at 20230707 end*/

static int upm6910x_get_chgtype(struct upm6910x_device *upm,
                struct upm6910x_state *state, bool type_recheck)
{
    int ret = 0;
    u8 chrg_stat = 0;

    /*Tab A9 code for AX6739A-2076 by hualei at 20230707 start*/
    if (upm->typec_attached > ATTACH_TYPE_NONE && upm->state.online) {
        switch(upm->typec_attached) {
            case ATTACH_TYPE_PD_SDP:
            case ATTACH_TYPE_PD_DCP:
            case ATTACH_TYPE_PD_NONSTD:
                dev_info(upm->dev, "%s: Attach PD_TYPE, skip bc12 flow!\n", __func__);
                return ret;
            default:
                break;
        }
    }
    /*Tab A9 code for AX6739A-2076 by hualei at 20230707 end*/

    ret = upm6910x_read_reg(upm, UPM6910_CHRG_STAT, &chrg_stat);
    if (ret) {
        ret = upm6910x_read_reg(upm, UPM6910_CHRG_STAT, &chrg_stat);
        if (ret) {
            pr_err("%s read UPM6910_CHRG_STAT fail\n", __func__);
            return ret;
        }
    }

    state->chrg_type = chrg_stat & UPM6910_VBUS_STAT_MASK;

    pr_err("[%s] chrg_type = 0x%x recheck = %d\n", __func__,
           state->chrg_type, type_recheck);

    /*Tab A9 code for SR-AX6739A-01-494 by qiaodan at 20230506 start*/
    switch (state->chrg_type) {
    case UPM6910_NOT_CHRGING:
        pr_err("UPM6910 charger type: NONE\n");
        upm->psy_type = POWER_SUPPLY_TYPE_UNKNOWN;
        upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
        break;
    case UPM6910_USB_SDP:
        pr_err("UPM6910 charger type: SDP\n");
        upm->psy_type = POWER_SUPPLY_TYPE_USB;
        upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
        break;
    case UPM6910_USB_CDP:
        pr_err("UPM6910 charger type: CDP\n");
        upm->psy_type = POWER_SUPPLY_TYPE_USB_CDP;
        upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
        break;
    case UPM6910_USB_DCP:
        pr_err("UPM6910 charger type: DCP\n");
        upm->psy_type = POWER_SUPPLY_TYPE_USB_DCP;
        upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
        break;
    case UPM6910_NON_STANDARD:
        pr_err("UPM6910 charger type: NON_STANDARD\n");
        upm->psy_type = POWER_SUPPLY_TYPE_USB_DCP;
        upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
        break;
    case UPM6910_UNKNOWN:
        pr_err("UPM6910 charger type: UNKNOWN\n");
        if (type_recheck) {
            upm->psy_type = POWER_SUPPLY_TYPE_USB;
        } else {
            upm->psy_type = POWER_SUPPLY_TYPE_USB_DCP;
        }
        upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
        break;
    default:
        pr_err("UPM6910 charger type: default\n");
        upm->psy_type = POWER_SUPPLY_TYPE_USB;
        upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
        break;
    }
    /*Tab A9 code for SR-AX6739A-01-494 by qiaodan at 20230506 end*/
    /*Tab A9 code for SR-AX6739A-01-500 by wenyaqi at 20230515 start*/
    if (upm->psy_type == POWER_SUPPLY_TYPE_USB_DCP) {
        ret = upm6910_enable_hvdcp(upm, true);
        if (ret) {
            pr_err("Failed to en HVDCP, ret = %d\n", ret);
        }

        ret = upm6910_set_dp(upm, UPM6910_DPDM_OUT_0P6V);
        if (ret) {
            pr_err("Failed to set dp 0.6v, ret = %d\n", ret);
        }
    }
    /*Tab A9 code for SR-AX6739A-01-500 by wenyaqi at 20230515 end*/

    /*Tab A9 code for AX6739A-2076 by hualei at 20230707 start*/
    power_supply_changed(upm->charger);
    /*Tab A9 code for AX6739A-2076 by hualei at 20230707 end*/

    return 0;
}

/*Tab A9 code for AX6739A-2076 by hualei at 20230707 start*/
static int upm6910x_force_dpdm(struct upm6910x_device *upm)
{
    /*Tab A9 code for AX6739A-2516 by lina at 20230729 start*/
    static struct mtk_charger *s_info = NULL;
    struct power_supply *chg_psy = NULL;
    int ret = 0;

    if (s_info == NULL) {
        if (chg_psy == NULL) {
            chg_psy = power_supply_get_by_name("mtk-master-charger");
        }
        if (chg_psy == NULL || IS_ERR(chg_psy)) {
            pr_notice("%s Couldn't get chg_psy\n", __func__);
            return -EPERM;
        }

        s_info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
        if (s_info == NULL) {
            pr_notice("%s Couldn't get s_info\n", __func__);
            return -EPERM;
        }
    }
    if(s_info->hv_voltage_switch1) {
        dev_info(upm->dev, "%s: PD enter,skip bc12\n", __func__);
        return ret;
    }
    /*Tab A9 code for AX6739A-2516 by lina at 20230729 end*/
    return upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_7,
                    UPM6910_FORCE_DPDM, UPM6910_FORCE_DPDM);
}

static void upm6910x_force_detection_dwork_handler(struct work_struct *work)
{
    int ret = 0;
    struct delayed_work *force_detect_dwork = NULL;
    struct upm6910x_device * upm = NULL;


    force_detect_dwork = container_of(work, struct delayed_work, work);
    if(force_detect_dwork == NULL) {
        pr_err("[%s] Cann't get force_detect_dwork\n", __func__);
        return;
    }

    upm = container_of(force_detect_dwork, struct upm6910x_device, force_detect_dwork);
    if (upm == NULL) {
        pr_err("[%s] Cann't get sgm415xx_device\n", __func__);
        return;
    }

    if (upm->state.vbus_attach) {
        pr_info("[%s] a good VBUS attached!!!\n", __func__);
        upm6910x_set_en_ignorebc12(upm, 0);
        msleep(20);
        ret = upm6910x_force_dpdm(upm);
        /*Tab A9 code for AX6739A-2344 by hualei at 20230717 start*/
        upm->force_dpdm = true;
        /*Tab A9 code for AX6739A-2344 by hualei at 20230717 end*/
        if (ret) {
            pr_err("[%s] force dpdm failed(%d)\n", __func__, ret);
            return;
        }
    } else if ((upm->state.vbus_attach == false) && (upm->typec_attached == ATTACH_TYPE_TYPEC)) {
        pr_info("[%s] a good VBUS is not attached, wait~~~\n", __func__);
        schedule_delayed_work(&upm->force_detect_dwork, msecs_to_jiffies(10));
    }

}
/*Tab A9 code for AX6739A-2076 by hualei at 20230707 end*/

/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 start*/
static int upm6910x_enable_hiz_mode(struct upm6910x_device *upm)
{
    int ret = 0;

    pr_info("%s enter\n", __func__);

    ret = upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_0, UPM6910_HIZ_EN,
                   UPM6910_HIZ_EN);

    /*Tab A9 code for AX6739A-2076 by hualei at 20230707 start*/
    upm->hvdcp_done = false;
    /*Tab A9 code for AX6739A-2076 by hualei at 20230707 end*/

    return ret;
}

static int upm6910x_disable_hiz_mode(struct upm6910x_device *upm)
{
    int ret = 0;

    pr_info("%s enter\n", __func__);

    ret = upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_0, UPM6910_HIZ_EN,
                   0);

    return ret;
}

static int upm6910x_get_hiz_mode(struct upm6910x_device *upm, bool *enable)
{
    int ret = 0;
    u8 val = 0;

    ret = upm6910x_read_reg(upm, UPM6910_CHRG_CTRL_0, &val);
    if (ret < 0) {
        pr_info("[%s] read UPM6910_CHRG_CTRL_0 fail\n", __func__);
        return ret;
    }

    val = val & UPM6910_HIZ_EN;
    if (val) {
        *enable  = true;
    } else {
        *enable  = false;
    }
    dev_info(upm->dev, "%s enter value = %d\n", __func__, *enable);

    return ret;
}
/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 end*/

static int upm6910x_enable_charger(struct upm6910x_device *upm)
{
    int ret = 0;

    pr_info("%s enter\n", __func__);

    ret = upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_1, UPM6910_CHRG_EN,
                   UPM6910_CHRG_EN);

    return ret;
}

static int upm6910x_disable_charger(struct upm6910x_device *upm)
{
    int ret = 0;

    pr_info("%s enter\n", __func__);

    ret = upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_1, UPM6910_CHRG_EN,
                   0);

    return ret;
}

static int upm6910x_is_charging(struct charger_device *chg_dev, bool *en)
{
    struct upm6910x_device *upm = charger_get_data(chg_dev);
    int ret = 0;
    u8 val = 0;

    ret = upm6910x_read_reg(upm, UPM6910_CHRG_CTRL_1, &val);
    if (ret) {
        pr_err("%s read UPM6910_CHRG_CTRL_A fail\n", __func__);
        return ret;
    }

    *en = (val & UPM6910_CHRG_EN) ? 1 : 0;

    return ret;
}

static int upm6910x_charging_switch(struct charger_device *chg_dev, bool enable)
{
    struct upm6910x_device *upm = charger_get_data(chg_dev);
    int ret = 0;

    dev_info(upm->dev, "%s value = %d\n", __func__, enable);

    if (enable) {
        ret = upm6910x_enable_charger(upm);
    } else {
        ret = upm6910x_disable_charger(upm);
    }
    /*Tab A9 code for SR-AX6739A-01-494 by qiaodan at 20230506 start*/
    upm->chg_config = enable;
    /*Tab A9 code for SR-AX6739A-01-494 by qiaodan at 20230506 end*/

    return ret;
}

/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 start*/
static int upm6910x_enable_port_charging(struct charger_device *chg_dev, bool enable)
{
    struct upm6910x_device *upm = charger_get_data(chg_dev);
    int ret = 0;

    dev_info(upm->dev, "%s value = %d\n", __func__, enable);

    if (enable) {
        ret = upm6910x_disable_hiz_mode(upm);
        /*Tab A9 code for AX6739A-2076 by hualei at 20230707 start*/
        schedule_delayed_work(&upm->force_detect_dwork, msecs_to_jiffies(200));
        /*Tab A9 code for AX6739A-2076 by hualei at 20230707 end*/
    } else {
        ret = upm6910x_enable_hiz_mode(upm);
    }

    return ret;
}

static int upm6910x_is_port_charging_enabled(struct charger_device *chg_dev, bool *enable)
{
    struct upm6910x_device *upm = charger_get_data(chg_dev);
    int ret = 0;
    bool hiz_enable = false;

    dev_info(upm->dev, "%s enter\n", __func__);

    ret = upm6910x_get_hiz_mode(upm, &hiz_enable);
    *enable = !hiz_enable;

    return ret;
}
/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 end*/

/*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 start*/
static int upm6910x_set_shipmode(struct charger_device *chg_dev, bool en)
{
    int ret = 0;
    u8 val = 0;

    struct upm6910x_device *upm = dev_get_drvdata(&chg_dev->dev);
    if (en) {
        ret += upm6910x_enable_hiz_mode(upm);
        val = REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT;
        ret += upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_7, REG07_BATFET_DIS_MASK, val);
    } else {
        val = REG07_BATFET_ON << REG07_BATFET_DIS_SHIFT;
        ret = upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_7, REG07_BATFET_DIS_MASK, val);
    }
    pr_err(" shipmode %s\n", !ret ? "successfully" : "failed");
    return ret;
}

static int upm6910x_get_shipmode(struct charger_device *chg_dev)
{
    int ret = 0;
    u8 val = 0;
    struct upm6910x_device *upm = dev_get_drvdata(&chg_dev->dev);

    msleep(100);
    ret = __upm6910x_read_byte(upm, UPM6910_CHRG_CTRL_7, &val);
    if (ret == 0) {
        pr_err("Reg[%.2x] = 0x%.2x\n", UPM6910_CHRG_CTRL_7, val);
    } else {
        pr_err("%s: get shipmode reg fail! \n", __func__);
        return ret;
    }
    ret = (val & REG07_BATFET_DIS_MASK) >> REG07_BATFET_DIS_SHIFT;
    pr_err("%s:shipmode %s\n", __func__, ret ? "enabled" : "disabled");

    return ret;
}

static int upm6910x_disable_battfet_rst(struct upm6910x_device *upm)
{
    int ret = 0;
    u8 val = 0;

    val = REG07_BATFET_RST_DISABLE << REG07_BATFET_RST_EN_SHIFT;
    ret = upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_7,
                               REG07_BATFET_RST_EN_MASK, val);

    pr_err("disable BATTFET_RST_EN %s\n", !ret ? "successfully" : "failed");

    return ret;
}
/*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 end*/

/*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
static int upm6910x_get_hvdcp_status(struct charger_device *chg_dev)
{
    int ret = 0;
    struct upm6910x_device *upm = charger_get_data(chg_dev);

    if (upm == NULL) {
        pr_info("[%s] upm6910x_get_hvdcp_status fail\n", __func__);
        return ret;
    }

    ret = (upm->hvdcp_done ? 1 : 0);

    return ret;
}
/*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/

static int upm6910x_set_recharge_volt(struct upm6910x_device *upm, int mV)
{
    u8 reg_val = 0;

    reg_val = (mV - UPM6910_VRECHRG_OFFSET_mV) / UPM6910_VRECHRG_STEP_mV;

    return upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_4, UPM6910_VRECHARGE,
                    reg_val);
}

static int upm6910x_dump_register(struct charger_device *chg_dev)
{
    struct upm6910x_device *upm = charger_get_data(chg_dev);
    unsigned char i = 0;
    unsigned int ret = 0;
    unsigned char upm6910x_reg[UPM6910_REG_NUM + 1] = { 0 };

    for (i = 0; i < UPM6910_REG_NUM + 1; i++) {
        ret = upm6910x_read_reg(upm, i, &upm6910x_reg[i]);
        if (ret != 0) {
            pr_info("[upm6910x] i2c transfor error\n");
            return 1;
        }
        pr_info("%s,[0x%x]=0x%x ", __func__, i, upm6910x_reg[i]);
    }

    return 0;
}

/*Tab A9 code for AX6739A-2299 by hualei at 20230713 start*/
static int upm6910x_plug_in(struct charger_device *chg_dev)
{
    struct upm6910x_device *upm = dev_get_drvdata(&chg_dev->dev);
    int ret = 0;

    dev_info(upm->dev, "%s\n", __func__);

    ret = upm6910x_charging_switch(chg_dev, true);
    if (ret) {
        dev_err(upm->dev, "Failed to enable charging:%d\n", ret);
    }

    return ret;
}
/*Tab A9 code for AX6739A-2299 by hualei at 20230713 end*/

/*Tab A9 code for AX6739A-2299 by hualei at 20230713 start*/
static int upm6910x_plug_out(struct charger_device *chg_dev)
{
    struct upm6910x_device *upm = dev_get_drvdata(&chg_dev->dev);
    int ret = 0;

    dev_info(upm->dev, "%s\n", __func__);

    ret = upm6910x_charging_switch(chg_dev, false);
    if (ret) {
        dev_err(upm->dev, "Failed to disable charging:%d\n", ret);
    }

    return ret;
}
/*Tab A9 code for AX6739A-2299 by hualei at 20230713 end*/

/*Tab A9 code for SR-AX6739A-01-531 by hualei at 20230523 start*/
static int upm6910x_do_event(struct charger_device *chg_dev,u32 event, u32 args)
{
    struct upm6910x_device *upm = charger_get_data(chg_dev);
    dev_info(upm->dev, "%s\n", __func__);

    switch (event) {
        case EVENT_FULL:
        case EVENT_RECHARGE:
        case EVENT_DISCHARGE:
            power_supply_changed(upm->charger);
            break;
        default:
            break;
    }

    return 0;
}
/*Tab A9 code for SR-AX6739A-01-531 by hualei at 20230523 end*/

static int upm6910x_hw_chipid_detect(struct upm6910x_device *upm)
{
    int ret = 0;
    u8 val = 0;

    ret = upm6910x_read_reg(upm, UPM6910_CHRG_CTRL_B, &val);
    if (ret < 0) {
        pr_info("[%s] read UPM6910_CHRG_CTRL_B fail\n", __func__);
        return ret;
    }
    val = val & UPM6910_PN_MASK;
    pr_info("[%s] Reg[0x0B]=0x%x\n", __func__, val);

    return val;
}

/*Tab A9 code for SR-AX6739A-01-490 | SR-AX6739A-01-491 by qiaodan at 20230508 start*/
static int upm6910x_hw_dev_rev_detect(struct upm6910x_device *upm)
{
    int ret = 0;
    u8 val = 0;

    ret = upm6910x_read_reg(upm, UPM6910_CHRG_CTRL_B, &val);
    if (ret < 0) {
        pr_err("[%s] read UPM6910_CHRG_CTRL_B fail\n", __func__);
        return ret;
    }

    val = val & UPM6910_DEV_REV_MASK;
    if (val == UPM6910_DEV_REV_DH) {
        gxy_bat_set_chginfo(GXY_BAT_CHG_INFO_UPM6910DH);
    } else if (val == UPM6910_DEV_REV_DS) {
        gxy_bat_set_chginfo(GXY_BAT_CHG_INFO_UPM6910DS);
    }
    pr_info("[%s] Reg[0x0B] dev =0x%x\n", __func__, val);

    return ret;
}
/*Tab A9 code for SR-AX6739A-01-490 | SR-AX6739A-01-491 by qiaodan at 20230508 end*/

/*Tab A9 code for SR-AX6739A-01-494 | SR-AX6739A-01-486 by qiaodan at 20230512 start*/
static int upm6910x_get_charging_status(struct charger_device *chg_dev,
                    bool *is_done)
{
    struct upm6910x_device *upm = charger_get_data(chg_dev);
    int ret = 0;

    ret = upm6910x_get_chrg_stat(upm);
    if (ret < 0) {
        pr_err("[%s] read charge stat failed\n", __func__);
        return ret;
    }

    if (ret == POWER_SUPPLY_STATUS_FULL) {
        *is_done = true;
    } else {
        *is_done = false;
    }

    return 0;
}
/*Tab A9 code for SR-AX6739A-01-494 | SR-AX6739A-01-486 by qiaodan at 20230512 end*/

static int upm6910x_set_int_mask(struct upm6910x_device *upm)
{
    int ret = 0;

    ret = upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_A, UPM6910_INT_MASK,
                   UPM6910_INT_MASK);

    return ret;
}

static int upm6910x_set_en_timer(struct upm6910x_device *upm)
{
    int ret = 0;

    ret = upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_5,
                   UPM6910_SAFETY_TIMER_EN,
                   UPM6910_SAFETY_TIMER_EN);

    return ret;
}

static int upm6910x_set_disable_timer(struct upm6910x_device *upm)
{
    int ret = 0;

    ret = upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_5,
                   UPM6910_SAFETY_TIMER_EN, 0);

    return ret;
}

static int upm6910x_enable_safetytimer(struct charger_device *chg_dev, bool en)
{
    struct upm6910x_device *upm = charger_get_data(chg_dev);
    int ret = 0;

    if (en) {
        ret = upm6910x_set_en_timer(upm);
    } else {
        ret = upm6910x_set_disable_timer(upm);
    }

    return ret;
}

static int upm6910x_get_is_safetytimer_enable(struct charger_device *chg_dev,
                          bool *en)
{
    int ret = 0;
    u8 val = 0;

    struct upm6910x_device *upm = charger_get_data(chg_dev);

    ret = upm6910x_read_reg(upm, UPM6910_CHRG_CTRL_5, &val);
    if (ret < 0) {
        pr_info("[%s] read UPM6910_CHRG_CTRL_5 fail\n", __func__);
        return ret;
    }

    *en = !!(val & UPM6910_SAFETY_TIMER_EN);

    return 0;
}

static enum power_supply_property upm6910x_power_supply_props[] = {
    POWER_SUPPLY_PROP_MANUFACTURER, POWER_SUPPLY_PROP_MODEL_NAME,
    POWER_SUPPLY_PROP_STATUS,    POWER_SUPPLY_PROP_ONLINE,
    POWER_SUPPLY_PROP_HEALTH,    POWER_SUPPLY_PROP_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_CURRENT_NOW,    POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
    POWER_SUPPLY_PROP_CHARGE_TYPE,    POWER_SUPPLY_PROP_USB_TYPE,
    POWER_SUPPLY_PROP_PRESENT,
    /*Tab A9 code for AX6739A-765 by hualei at 20230612 start*/
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
    /*Tab A9 code for AX6739A-765 by hualei at 20230612 end*/
};

static int upm6910x_property_is_writeable(struct power_supply *psy,
                      enum power_supply_property prop)
{
    switch (prop) {
        /*Tab A9 code for AX6739A-516 by wenyaqi at 20230603 start*/
        case POWER_SUPPLY_PROP_ONLINE:
        /*Tab A9 code for AX6739A-516 by wenyaqi at 20230603 end*/
        case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
        case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        case POWER_SUPPLY_PROP_PRECHARGE_CURRENT:
        case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
            return true;
        default:
            return false;
    }
}
static int upm6910x_charger_set_property(struct power_supply *psy,
                     enum power_supply_property prop,
                     const union power_supply_propval *val)
{
    int ret = -EINVAL;

    switch (prop) {
        /*Tab A9 code for AX6739A-516 by wenyaqi at 20230603 start*/
        case POWER_SUPPLY_PROP_ONLINE:
            pr_info("%s  %d\n", __func__, val->intval);
            ret = upm6910x_chg_attach_pre_process(s_chg_dev_otg, val->intval);
            break;
        /*Tab A9 code for AX6739A-516 by wenyaqi at 20230603 end*/
        case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
            ret = upm6910x_set_input_curr_lim(s_chg_dev_otg, val->intval);
            break;
        default:
            return -EINVAL;
    }

    return ret;
}

static int upm6910x_charger_get_property(struct power_supply *psy,
                     enum power_supply_property psp,
                     union power_supply_propval *val)
{
    struct upm6910x_device *upm = power_supply_get_drvdata(psy);
    struct upm6910x_state state;
    int ret = 0;
    /*Tab A9 code for AX6739A-765 by hualei at 20230612 start*/
    unsigned int data = 0;
    /*Tab A9 code for AX6739A-765 by hualei at 20230612 end*/

    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 start*/
    mutex_lock(&upm->lock);
    memcpy(&state, &upm->state, sizeof(upm->state));
    mutex_unlock(&upm->lock);
    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 end*/

    switch (psp) {
    case POWER_SUPPLY_PROP_STATUS:
        /*Tab A9 code for SR-AX6739A-01-494 by qiaodan at 20230506 start*/
        ret = upm6910x_get_chrg_stat(upm);
        if (ret < 0) {
            break;
        }
        val->intval = ret;
        break;
        /*Tab A9 code for SR-AX6739A-01-494 by qiaodan at 20230506 end*/
    case POWER_SUPPLY_PROP_CHARGE_TYPE:
        switch (state.chrg_stat) {
        case UPM6910_PRECHRG:
            val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
            break;
        case UPM6910_FAST_CHRG:
            val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
            break;
        case UPM6910_TERM_CHRG:
            val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
            break;
        case UPM6910_NOT_CHRGING:
            val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
            break;
        default:
            val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
        }
        break;
    case POWER_SUPPLY_PROP_MANUFACTURER:
        val->strval = UPM6910_MANUFACTURER;
        break;
    case POWER_SUPPLY_PROP_USB_TYPE:
        val->intval = upm->psy_usb_type;
        break;
    case POWER_SUPPLY_PROP_MODEL_NAME:
        val->strval = UPM6910_NAME;
        break;
    case POWER_SUPPLY_PROP_ONLINE:
        /*Tab A9 code for AX6739A-516|P230609-02043|AX6739A-2076 by hualei at 20230707 start*/
        val->intval = state.vbus_attach | (!!upm->typec_attached);
        if (upm->psy_type == POWER_SUPPLY_TYPE_UNKNOWN) {
            val->intval = 0;
        }
        /*Tab A9 code for AX6739A-516|P230609-02043|AX6739A-2076 by hualei at 20230707 end*/
        break;
    case POWER_SUPPLY_PROP_PRESENT:
        val->intval = state.vbus_attach;
        break;
    case POWER_SUPPLY_PROP_TYPE:
        val->intval = upm->psy_type;
        break;
    case POWER_SUPPLY_PROP_HEALTH:
        val->intval = POWER_SUPPLY_HEALTH_GOOD;
        break;
    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
        //val->intval = state.vbus_adc;
        break;
    case POWER_SUPPLY_PROP_CURRENT_NOW:
        //val->intval = state.ibus_adc;
        break;
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        break;
    /*Tab A9 code for AX6739A-765 by hualei at 20230612 start*/
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
        ret = upm6910x_get_vreg(upm, &data);
        if (ret < 0) {
            pr_err("Cann't get vreg\n");
            break;
        }
        val->intval = data / 1000;
        break;
    /*Tab A9 code for AX6739A-765 by hualei at 20230612 end*/
    default:
        return -EINVAL;
    }

    return ret;
}

static void charger_detect_work_func(struct work_struct *work)
{
    struct delayed_work *charge_detect_delayed_work = NULL;
    struct upm6910x_device *upm = NULL;
    struct upm6910x_state state;
    int ret = 0;
    bool vbus_attach_pre = false;
    bool online_pre = false;
    /*Tab A9 code for AX6739A-2299 by hualei at 20230713 start*/
    u32 now_limit = 0;
    /*Tab A9 code for AX6739A-2299 by hualei at 20230713 end*/

    /*Tab A9 code for AX6739A-237 by qiaodan at 20230530 start*/
    pr_notice("%s enter\r\n",__func__);
    /*Tab A9 code for AX6739A-237 by qiaodan at 20230530 end*/

    charge_detect_delayed_work =
        container_of(work, struct delayed_work, work);
    if (charge_detect_delayed_work == NULL) {
        pr_err("Cann't get charge_detect_delayed_work\n");
        return;
    }

    upm = container_of(charge_detect_delayed_work, struct upm6910x_device,
               charge_detect_delayed_work);
    if (upm == NULL) {
        pr_err("Cann't get upm6910x_device\n");
        return;
    }

    vbus_attach_pre = upm->state.vbus_attach;
    online_pre = upm->state.online;
    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 start*/
    mutex_lock(&upm->lock);
    memcpy(&state, &upm->state, sizeof(upm->state));
    ret = upm6910x_get_state(upm, &state);
    memcpy(&upm->state, &state, sizeof(state));
    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 end*/
    mutex_unlock(&upm->lock);

    pr_err("%s v_pre = %d,v_now = %d\n", __func__,
           vbus_attach_pre, state.vbus_attach);
    /*Tab A9 code for SR-AX6739A-01-494 by qiaodan at 20230506 start*/
    if (!vbus_attach_pre && state.vbus_attach) {
        pr_notice("adapter/usb inserted\n");
        upm->chg_config = true;
    } else if (vbus_attach_pre && !state.vbus_attach) {
        pr_notice("adapter/usb removed\n");
        /*Tab A9 code for AX6739A-1460 by hualei at 20230625 start*/
        upm6910x_set_ichrg(upm, UPM6910_ICHRG_I_LIMIT);
        /*Tab A9 code for AX6739A-1460 by hualei at 20230625 end*/
        /*Tab A9 code for AX6739A-516 by wenyaqi at 20230603 start*/
        // upm->psy_type = POWER_SUPPLY_TYPE_UNKNOWN;
        /*Tab A9 code for AX6739A-516 by wenyaqi at 20230603 end*/
        upm->chg_config = false;
        /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
        cancel_delayed_work_sync(&upm->hvdcp_done_work);
        /*Tab A9 code for AX6739A-2076 by hualei at 20230707 start*/
        cancel_delayed_work_sync(&upm->force_detect_dwork);
        /*Tab A9 code for AX6739A-2076 by hualei at 20230707 end*/
        upm->hvdcp_done = false;
        /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/
        /*Tab A9 code for AX6739A-2344 by hualei at 20230717 start*/
        upm6910x_set_en_ignorebc12(upm, true);
        upm->force_dpdm = false;
        /*Tab A9 code for AX6739A-2344 by hualei at 20230717 end*/
    }
    /*Tab A9 code for SR-AX6739A-01-494 by qiaodan at 20230506 end*/

    /* bc12 done, get charger type */
    /*Tab A9 code for AX6739A-2299 by hualei at 20230713 start*/
    if (!online_pre && state.online) {
        upm6910x_get_input_curr_lim_irq(upm, &now_limit);
        if(now_limit != upm->pre_limit) {
            upm6910x_set_input_curr_lim_irq(upm, upm->pre_limit);
        }
    }
    /*Tab A9 code for AX6739A-2299 by hualei at 20230713 end*/

    /*Tab A9 code for AX6739A-2344 by hualei at 20230717 start*/
    if ((upm->force_dpdm == true) && (state.online == true)) {
        pr_notice("start get chgtype!\n");
        upm6910x_get_chgtype(upm, &upm->state, true);
        upm6910x_set_en_ignorebc12(upm, true);
        upm->force_dpdm = false;
    }
    /*Tab A9 code for AX6739A-2344 by hualei at 20230717 end*/
    upm6910x_dump_register(upm->chg_dev);

    return;

}

static void charger_detect_recheck_work_func(struct work_struct *work)
{
    struct delayed_work *charge_detect_recheck_delay_work = NULL;
    struct upm6910x_device *upm = NULL;
    struct upm6910x_state state;

    charge_detect_recheck_delay_work =
        container_of(work, struct delayed_work, work);
    if (charge_detect_recheck_delay_work == NULL) {
        pr_err("Cann't get charge_detect_delayed_work\n");
        return;
    }

    upm = container_of(charge_detect_recheck_delay_work,
               struct upm6910x_device,
               charge_detect_recheck_delay_work);
    if (upm == NULL) {
        pr_err("Cann't get upm6910x_device\n");
        return;
    }

    if (!upm->charger_wakelock->active) {
        __pm_stay_awake(upm->charger_wakelock);
    }

    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 start*/
    mutex_lock(&upm->lock);
    memcpy(&state, &upm->state, sizeof(upm->state));
    memcpy(&upm->state, &state, sizeof(state));
    mutex_unlock(&upm->lock);
    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 end*/

    /* release wakelock */

    dev_err(upm->dev, "Relax wakelock\n");
    __pm_relax(upm->charger_wakelock);

    return;
}

/*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
static void hvdcp_done_work_func(struct work_struct *work)
{
    struct delayed_work *hvdcp_done_work = NULL;
    struct upm6910x_device *upm = NULL;

    hvdcp_done_work = container_of(work, struct delayed_work, work);
    if (hvdcp_done_work == NULL) {
        pr_err("Cann't get hvdcp_done_work\n");
        return;
    }

    upm = container_of(hvdcp_done_work, struct upm6910x_device,
                       hvdcp_done_work);
    if (upm == NULL) {
        pr_err("Cann't get upm6910x_device\n");
        return;
    }

    upm->hvdcp_done = true;
    power_supply_changed(upm->charger);
    pr_err("%s HVDCP end\n", __func__);
}
/*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/

/*Tab A9 code for AX6739A-115 by qiaodan at 20230514 start*/
static irqreturn_t upm6910x_irq_handler_thread(int irq, void *private)
{
    struct upm6910x_device *upm = private;

    __pm_wakeup_event(upm->charger_wakelock, UPM6910_IRQ_WAKE_TIME);

    /*Tab A9 code for AX6739A-237 by qiaodan at 20230530 start*/
    pr_notice("%s enter \r\n",__func__);
    /*Tab A9 code for AX6739A-237 by qiaodan at 20230530 end*/

    schedule_delayed_work(&upm->charge_detect_delayed_work,
            msecs_to_jiffies(UPM6910_WORK_DELAY_TIME));

    return IRQ_HANDLED;
}
/*Tab A9 code for AX6739A-115 by qiaodan at 20230514 end*/

/*Tab A9 code for SR-AX6739A-01-494 by qiaodan at 20230506 start*/
static char *upm6910x_charger_supplied_to[] = {
    "battery",
    "mtk-master-charger",
};
/*Tab A9 code for SR-AX6739A-01-494 by qiaodan at 20230506 end*/

static struct power_supply_desc upm6910x_power_supply_desc = {
    .name = "charger",
    /*Tab A9 code for SR-AX6739A-01-494 by qiaodan at 20230506 start*/
    .type = POWER_SUPPLY_TYPE_UNKNOWN,
    /*Tab A9 code for SR-AX6739A-01-494 by qiaodan at 20230506 end*/
    .usb_types = upm6910x_usb_type,
    .num_usb_types = ARRAY_SIZE(upm6910x_usb_type),
    .properties = upm6910x_power_supply_props,
    .num_properties = ARRAY_SIZE(upm6910x_power_supply_props),
    .get_property = upm6910x_charger_get_property,
    .set_property = upm6910x_charger_set_property,
    .property_is_writeable = upm6910x_property_is_writeable,
};

static int upm6910x_power_supply_init(struct upm6910x_device *upm,
                      struct device *dev)
{
    struct power_supply_config psy_cfg = {
        .drv_data = upm,
        .of_node = dev->of_node,
    };

    psy_cfg.supplied_to = upm6910x_charger_supplied_to;
    psy_cfg.num_supplicants = ARRAY_SIZE(upm6910x_charger_supplied_to);

    memcpy(&upm->psy_desc, &upm6910x_power_supply_desc,
           sizeof(upm->psy_desc));
    upm->charger =
        devm_power_supply_register(upm->dev, &upm->psy_desc, &psy_cfg);
    if (IS_ERR(upm->charger)) {
        return -EINVAL;
    }

    return 0;
}

static int upm6910x_hw_init(struct upm6910x_device *upm)
{
    struct power_supply_battery_info bat_info = {};
    int ret = 0;

    bat_info.constant_charge_current_max_ua = UPM6910_ICHRG_I_DEF_uA;

    bat_info.constant_charge_voltage_max_uv = UPM6910_VREG_V_DEF_uV;

    bat_info.precharge_current_ua = UPM6910_PRECHRG_I_DEF_uA;

    bat_info.charge_term_current_ua = UPM6910_TERMCHRG_I_DEF_uA;

    upm->init_data.max_ichg = UPM6910_ICHRG_I_MAX_uA;

    upm->init_data.max_vreg = UPM6910_VREG_V_MAX_uV;

    upm6910x_set_watchdog_timer(upm, 0);

    ret = upm6910x_set_ichrg_curr(s_chg_dev_otg,
                      bat_info.constant_charge_current_max_ua);
    if (ret) {
        goto err_out;
    }

    ret = upm6910x_set_prechrg_curr(upm, bat_info.precharge_current_ua);
    if (ret) {
        goto err_out;
    }

    ret = upm6910x_set_chrg_volt(s_chg_dev_otg,
                     bat_info.constant_charge_voltage_max_uv);
    if (ret) {
        goto err_out;
    }

    ret = upm6910x_set_term_curr(s_chg_dev_otg,
                     bat_info.charge_term_current_ua);
    if (ret) {
        goto err_out;
    }

    ret = upm6910x_set_input_volt_lim(s_chg_dev_otg, upm->init_data.vlim);
    if (ret) {
        goto err_out;
    }

    ret = upm6910x_set_input_curr_lim(s_chg_dev_otg, upm->init_data.ilim);
    if (ret) {
        goto err_out;
    }

    ret = upm6910x_set_vac_ovp(upm, VAC_OVP_14000); //14V
    if (ret) {
        goto err_out;
    }

    ret = upm6910x_set_recharge_volt(upm, 100); //100~200mv
    if (ret) {
        goto err_out;
    }

    ret = upm6910x_set_int_mask(upm);
    if (ret) {
        goto err_out;
    }

    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 start*/
    ret = upm6910x_disable_hiz_mode(upm);
    if (ret) {
        goto err_out;
    }
    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 end*/

    /*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 start*/
    ret = upm6910x_disable_battfet_rst(upm);
    if (ret) {
        pr_err("Failed to disable_battfet\n");
    }
    /*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 end*/

    /*Tab A9 code for AX6739A-399 by shanxinkai at 20230530 start*/
    ret = upm6910x_set_disable_timer(upm);
    if (ret) {
        pr_err("Failed to set safety_timer stop, ret = %d\n", ret);
    }
    /*Tab A9 code for AX6739A-399 by shanxinkai at 20230530 end*/

    dev_notice(upm->dev,
           "ichrg_curr:%d prechrg_curr:%d chrg_vol:%d"
           " term_curr:%d input_curr_lim:%d",
           bat_info.constant_charge_current_max_ua,
           bat_info.precharge_current_ua,
           bat_info.constant_charge_voltage_max_uv,
           bat_info.charge_term_current_ua, upm->init_data.ilim);

    return 0;

err_out:
    return ret;
}

static int upm6910x_parse_dt(struct upm6910x_device *upm)
{
    int ret = 0;
    int irq_gpio = 0, irqn = 0;

    dev_err(upm->dev, "[%s] enter\n", __func__);

    ret = device_property_read_u32(upm->dev,
                       "input-voltage-limit-microvolt",
                       &upm->init_data.vlim);
    if (ret) {
        upm->init_data.vlim = UPM6910_VINDPM_DEF_uV;
    }

    if (upm->init_data.vlim > UPM6910_VINDPM_V_MAX_uV ||
        upm->init_data.vlim < UPM6910_VINDPM_V_MIN_uV) {
        return -EINVAL;
    }

    ret = device_property_read_u32(upm->dev, "input-current-limit-microamp",
                       &upm->init_data.ilim);
    if (ret) {
        upm->init_data.ilim = UPM6910_IINDPM_DEF_uA;
    }

    if (upm->init_data.ilim > UPM6910_IINDPM_I_MAX_uA ||
        upm->init_data.ilim < UPM6910_IINDPM_I_MIN_uA) {
        return -EINVAL;
    }

    irq_gpio = of_get_named_gpio(upm->dev->of_node, "upm,intr_gpio", 0);
    if (!gpio_is_valid(irq_gpio)) {
        dev_err(upm->dev, "%s: %d gpio get failed\n", __func__,
            irq_gpio);
        return -EINVAL;
    }
    ret = gpio_request(irq_gpio, "upm6910x irq pin");
    if (ret) {
        dev_err(upm->dev, "%s: %d gpio request failed\n", __func__,
            irq_gpio);
        return ret;
    }
    gpio_direction_input(irq_gpio);
    irqn = gpio_to_irq(irq_gpio);
    if (irqn < 0) {
        dev_err(upm->dev, "%s:%d gpio_to_irq failed\n", __func__, irqn);
        return irqn;
    }
    upm->client->irq = irqn;

    dev_err(upm->dev, "[%s] loaded\n", __func__);

    return 0;
}

/*Tab A9 code for SR-AX6739A-01-487 by qiaodan at 20230515 start*/
static int upm6910x_enable_vbus(struct regulator_dev *rdev)
{
    struct upm6910x_device *upm = charger_get_data(s_chg_dev_otg);
    int ret = 0;

    pr_notice("%s ente\r\n", __func__);

    /*we should ensure that the powerpath is enabled before enable OTG*/
    ret = upm6910x_disable_hiz_mode(upm);
    if (ret) {
        pr_err("%s exit hiz failed\r\n", __func__);
    }

    ret |= upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_1, UPM6910_OTG_EN,
                   UPM6910_OTG_EN);

    return ret;
}
/*Tab A9 code for SR-AX6739A-01-487 by qiaodan at 20230515 end*/

static int upm6910x_disable_vbus(struct regulator_dev *rdev)
{
    struct upm6910x_device *upm = charger_get_data(s_chg_dev_otg);
    int ret = 0;

    pr_notice("%s enter\n", __func__);

    ret = upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_1, UPM6910_OTG_EN, 0);

    return ret;
}

static int upm6910x_is_enabled_vbus(struct regulator_dev *rdev)
{
    struct upm6910x_device *upm = charger_get_data(s_chg_dev_otg);
    u8 temp = 0;
    int ret = 0;

    pr_notice("%s enter\n", __func__);

    ret = upm6910x_read_reg(upm, UPM6910_CHRG_CTRL_1, &temp);

    return (temp & UPM6910_OTG_EN) ? 1 : 0;
}

static int upm6910x_enable_otg(struct charger_device *chg_dev, bool en)
{
    int ret = 0;

    pr_info("%s en = %d\n", __func__, en);

    if (en) {
        ret = upm6910x_enable_vbus(NULL);
    } else {
        ret = upm6910x_disable_vbus(NULL);
    }

    return ret;
}

static int upm6910x_set_boost_current_limit(struct charger_device *chg_dev,
                        u32 uA)
{
    struct upm6910x_device *upm = charger_get_data(chg_dev);
    int ret = 0;

    if (uA == BOOST_CURRENT_LIMIT[0]) {
        ret = upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_2,
                       UPM6910_BOOST_LIM, 0);
    }

    else if (uA == BOOST_CURRENT_LIMIT[1]) {
        ret = upm6910x_update_bits(upm, UPM6910_CHRG_CTRL_2,
                       UPM6910_BOOST_LIM, BIT(7));
    }
    return ret;
}

static struct regulator_ops upm6910x_vbus_ops = {
    .enable = upm6910x_enable_vbus,
    .disable = upm6910x_disable_vbus,
    .is_enabled = upm6910x_is_enabled_vbus,
};

static const struct regulator_desc upm6910x_otg_rdesc = {
    .of_match = "usb-otg-vbus",
    .name = "usb-otg-vbus",
    .ops = &upm6910x_vbus_ops,
    .owner = THIS_MODULE,
    .type = REGULATOR_VOLTAGE,
    .fixed_uV = 5000000,
    .n_voltages = 1,
};

static int upm6910x_vbus_regulator_register(struct upm6910x_device *upm)
{
    struct regulator_config config = {};
    int ret = 0;
    /* otg regulator */
    config.dev = upm->dev;
    config.driver_data = upm;
    upm->otg_rdev =
        devm_regulator_register(upm->dev, &upm6910x_otg_rdesc, &config);
    upm->otg_rdev->constraints->valid_ops_mask |= REGULATOR_CHANGE_STATUS;
    if (IS_ERR(upm->otg_rdev)) {
        ret = PTR_ERR(upm->otg_rdev);
        pr_info("%s: register otg regulator failed (%d)\n", __func__,
            ret);
    }
    return ret;
}

static struct charger_ops upm6910x_chg_ops = {

    .dump_registers = upm6910x_dump_register,
    /* cable plug in/out */
    /*Tab A9 code for AX6739A-2299 by hualei at 20230713 start*/
    .plug_in = upm6910x_plug_in,
    .plug_out = upm6910x_plug_out,
    /*Tab A9 code for AX6739A-2299 by hualei at 20230713 end*/
    /* enable */
    .enable = upm6910x_charging_switch,
    .is_enabled = upm6910x_is_charging,
    /* charging current */
    .set_charging_current = upm6910x_set_ichrg_curr,
    .get_charging_current = upm6910x_get_ichg_curr,
    .get_min_charging_current = upm6910x_get_minichg_curr,
    /* charging voltage */
    .set_constant_voltage = upm6910x_set_chrg_volt,
    .get_constant_voltage = upm6910x_get_chrg_volt,
    /* input current limit */
    .set_input_current = upm6910x_set_input_curr_lim,
    .get_input_current = upm6910x_get_input_curr_lim,
    .get_min_input_current = upm6910x_get_input_mincurr_lim,
    /* MIVR */
    .set_mivr = upm6910x_set_input_volt_lim,
    .get_mivr = upm6910x_get_input_volt_lim,
    //.get_mivr_state = researved;
    /* charing termination */
    .set_eoc_current = upm6910x_set_term_curr,
    //.enable_termination = researved,
    //.reset_eoc_state = researved,
    //.safety_check = researved,
    .is_charging_done = upm6910x_get_charging_status,
    /* power path */
    //.enable_powerpath = researved,
    //.is_powerpath_enabled = researved,
    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 start*/
    .enable_port_charging = upm6910x_enable_port_charging,
    .is_port_charging_enabled= upm6910x_is_port_charging_enabled,
    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 end*/
    /* timer */
    .enable_safety_timer = upm6910x_enable_safetytimer,
    .is_safety_timer_enabled = upm6910x_get_is_safetytimer_enable,
    /* AICL */
    //.run_aicl = researved,
    /* PE+/PE+20 */
     //send_ta_current_pattern = researved,
    //.set_pe20_efficiency_table = researved,
    //.send_ta20_current_pattern = researved,
    //.reset_ta = researved,
    //.enable_cable_drop_comp = researved
    /* OTG */
    .enable_otg = upm6910x_enable_otg,
    .set_boost_current_limit = upm6910x_set_boost_current_limit,
    /*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 start*/
    .get_ship_mode = upm6910x_get_shipmode,
    .set_ship_mode = upm6910x_set_shipmode,
    /*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 end*/
    /*Tab A9 code for SR-AX6739A-01-531 by hualei at 20230523 start*/
    .event = upm6910x_do_event,
    /*Tab A9 code for SR-AX6739A-01-531 by hualei at 20230523 end*/
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
    .get_hvdcp_status = upm6910x_get_hvdcp_status,
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/
};

static int upm6910x_driver_probe(struct i2c_client *client,
                 const struct i2c_device_id *id)
{
    struct device *dev = &client->dev;
    struct upm6910x_device *upm;
    int ret = 0;

    char *name = NULL;

    pr_info("[%s] enter\n", __func__);

    upm = devm_kzalloc(dev, sizeof(*upm), GFP_KERNEL);
    if (!upm) {
        return -ENOMEM;
    }

    upm->client = client;
    upm->dev = dev;

    mutex_init(&upm->lock);
    mutex_init(&upm->i2c_rw_lock);

    i2c_set_clientdata(client, upm);

    ret = upm6910x_hw_chipid_detect(upm);
    if (ret != UPM6910_PN_ID) {
        pr_info("[%s] device not found !!!\n", __func__);
        return ret;
    }

    /*Tab A9 code for SR-AX6739A-01-490 by qiaodan at 20230504 start*/
    ret = upm6910x_hw_dev_rev_detect(upm);
    if (ret) {
        pr_info("[%s] dev_rev get failed\n", __func__);
        return ret;
    }
    /*Tab A9 code for SR-AX6739A-01-490 by qiaodan at 20230504 end*/

    ret = upm6910x_parse_dt(upm);
    if (ret) {
        return ret;
    }

    name = devm_kasprintf(upm->dev, GFP_KERNEL, "%s",
                  "upm6910x suspend wakelock");
    upm->charger_wakelock = wakeup_source_register(NULL, name);

    /* Register charger device */
    upm->chg_dev =
        charger_device_register("primary_chg", &client->dev, upm,
                    &upm6910x_chg_ops, &upm6910x_chg_props);
    if (IS_ERR_OR_NULL(upm->chg_dev)) {
        pr_info("%s: register charger device  failed\n", __func__);
        ret = PTR_ERR(upm->chg_dev);
        return ret;
    }

    /* otg regulator */
    s_chg_dev_otg = upm->chg_dev;

    INIT_DELAYED_WORK(&upm->charge_detect_delayed_work,
              charger_detect_work_func);
    INIT_DELAYED_WORK(&upm->charge_detect_recheck_delay_work,
              charger_detect_recheck_work_func);
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
    INIT_DELAYED_WORK(&upm->hvdcp_done_work, hvdcp_done_work_func);
    /*Tab A9 code for AX6739A-2076 by hualei at 20230707 start*/
    INIT_DELAYED_WORK(&upm->force_detect_dwork, upm6910x_force_detection_dwork_handler);
    /*Tab A9 code for AX6739A-2076 by hualei at 20230707 end*/
    upm->hvdcp_done = false;
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/

    ret = upm6910x_power_supply_init(upm, dev);
    if (ret) {
        pr_err("Failed to register power supply\n");
        return ret;
    }
    /*Tab A9 code for SR-AX6739A-01-494 by qiaodan at 20230506 start*/
    upm->chg_config = false;
    /*Tab A9 code for SR-AX6739A-01-494 by qiaodan at 20230506 end*/

    ret = upm6910x_hw_init(upm);
    if (ret) {
        dev_err(dev, "Cannot initialize the chip.\n");
        return ret;
    }

    if (client->irq) {
        ret = devm_request_threaded_irq(
            dev, client->irq, NULL, upm6910x_irq_handler_thread,
            IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
            dev_name(&client->dev), upm);
        if (ret) {
            return ret;
        }

        /*Tab A9 code for AX6739A-2076 by hualei at 20230707 start*/
        upm6910x_set_en_ignorebc12(upm, 1);
        /*Tab A9 code for AX6739A-2076 by hualei at 20230707 end*/
        enable_irq_wake(client->irq);
        /*Tab A9 code for AX6739A-115 by qiaodan at 20230514 start*/
        device_init_wakeup(upm->dev, true);
        /*Tab A9 code for AX6739A-115 by qiaodan at 20230514 end*/
    }

    /*Tab A9 code for AX6739A-2076 by hualei at 20230707 start*/
    upm6910x_get_state(upm, &upm->state);
    /*Tab A9 code for AX6739A-2076 by hualei at 20230707 end*/
    ret = upm6910x_vbus_regulator_register(upm);

    pr_err("[%s] probe success5\n", __func__);

    return ret;
}

static int upm6910x_charger_remove(struct i2c_client *client)
{
    struct upm6910x_device *upm = i2c_get_clientdata(client);

    regulator_unregister(upm->otg_rdev);

    power_supply_unregister(upm->charger);

    mutex_destroy(&upm->lock);
    mutex_destroy(&upm->i2c_rw_lock);

    return 0;
}

static void upm6910x_charger_shutdown(struct i2c_client *client)
{
    struct upm6910x_device *upm = i2c_get_clientdata(client);
    int ret = 0;

    ret = upm6910x_disable_charger(upm);
    if (ret) {
        pr_err("Failed to disable charger, ret = %d\n", ret);
    }

    pr_info("[%s] enter\n", __func__);
}

static const struct i2c_device_id upm6910x_i2c_ids[] = {
    { "upm6910dh", 0 },
    {},
};
MODULE_DEVICE_TABLE(i2c, upm6910x_i2c_ids);

static const struct of_device_id upm6910x_of_match[] = {
    {
        .compatible = "upm,upm6910dh",
    },
    {},
};
MODULE_DEVICE_TABLE(of, upm6910x_of_match);

/*Tab A9 code for AX6739A-115 by qiaodan at 20230514 start*/
#ifdef CONFIG_PM_SLEEP
static int upm6910x_suspend(struct device *dev)
{
    struct upm6910x_device *upm = dev_get_drvdata(dev);

    dev_info(dev, "%s\n", __func__);

    if (device_may_wakeup(dev)) {
        enable_irq_wake(upm->client->irq);
    }

    disable_irq(upm->client->irq);

    return 0;
}

static int upm6910x_resume(struct device *dev)
{
    struct upm6910x_device *upm = dev_get_drvdata(dev);

    dev_info(dev, "%s\n", __func__);

    enable_irq(upm->client->irq);
    if (device_may_wakeup(dev)) {
        disable_irq_wake(upm->client->irq);
    }

    return 0;
}

static SIMPLE_DEV_PM_OPS(upm6910x_pm_ops, upm6910x_suspend, upm6910x_resume);
#endif /*CONFIG_PM_SLEEP*/
/*Tab A9 code for AX6739A-115 by qiaodan at 20230514 end*/

static struct i2c_driver upm6910x_driver = {
    .driver = {
        .name = "upm6910x-charger",
        .of_match_table = upm6910x_of_match,
        /*Tab A9 code for AX6739A-115 by qiaodan at 20230514 start*/
#ifdef CONFIG_PM_SLEEP
        .pm = &upm6910x_pm_ops,
#endif /*CONFIG_PM_SLEEP*/
        /*Tab A9 code for AX6739A-115 by qiaodan at 20230514 end*/
    },
    .probe = upm6910x_driver_probe,
    .remove = upm6910x_charger_remove,
    .shutdown = upm6910x_charger_shutdown,
    .id_table = upm6910x_i2c_ids,
};
module_i2c_driver(upm6910x_driver);

MODULE_AUTHOR(" zjc <jianchuang.zhao@unisemipower.com>");
MODULE_DESCRIPTION("upm6910x charger driver");
MODULE_LICENSE("GPL v2");