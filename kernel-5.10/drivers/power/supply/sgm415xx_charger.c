// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2023 SGM.
 */

#include <linux/types.h>
#include <linux/init.h>        /* For init/exit macros */
#include <linux/module.h>    /* For MODULE_ marcros  */
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
#include "sgm415xx_charger.h"
#include "charger_class.h"
#include "mtk_charger.h"
#include <linux/power/gxy_psy_sysfs.h>

extern void gxy_bat_set_chginfo(enum gxy_bat_chg_info cinfo_data);

/**********************************************************
 *
 *   [I2C Slave Setting]
 *
 *********************************************************/

#define SGM415XX_REG_NUM    (0xC)

/* SGM415XX REG06 BOOST_LIM[5:4], uV */
static const unsigned int BOOST_VOLT_LIMIT[] = {
    4850000, 5000000, 5150000, 5300000
};
 /* SGM415XX REG02 BOOST_LIM[7:7], uA */
#if (defined(__SGM41542_CHIP_ID__) || defined(__SGM41541_CHIP_ID__)|| defined(__SGM41543_CHIP_ID__)|| defined(__SGM41543D_CHIP_ID__))
static const unsigned int BOOST_CURRENT_LIMIT[] = {
    1200000, 2000000
};
#else
static const unsigned int BOOST_CURRENT_LIMIT[] = {
    500000, 1200000
};
#endif //(defined(__SGM41542_CHIP_ID__) || defined(__SGM41541_CHIP_ID__)|| defined(__SGM41543_CHIP_ID__)|| defined(__SGM41543D_CHIP_ID__))

#if (defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))

static const unsigned int IPRECHG_CURRENT_STABLE[] = {
    5000, 10000, 15000, 20000, 30000, 40000, 50000, 60000,
    80000, 100000, 120000, 140000, 160000, 180000, 200000, 240000
};

static const unsigned int ITERM_CURRENT_STABLE[] = {
    5000, 10000, 15000, 20000, 30000, 40000, 50000, 60000,
    80000, 100000, 120000, 140000, 160000, 180000, 200000, 240000
};
#endif //(defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))

static enum power_supply_usb_type sgm415xx_usb_type[] = {
    POWER_SUPPLY_USB_TYPE_UNKNOWN,
    POWER_SUPPLY_USB_TYPE_SDP,
    POWER_SUPPLY_USB_TYPE_CDP,
    POWER_SUPPLY_USB_TYPE_DCP,
};

static const struct charger_properties sgm415xx_chg_props = {
    .alias_name = SGM415XX_NAME,
};

/**********************************************************
 *
 *   [Global Variable]
 *
 *********************************************************/
static struct power_supply_desc sgm415xx_power_supply_desc;
static struct charger_device *s_chg_dev_otg;

/**********************************************************
 *
 *   [I2C Function For Read/Write sgm415xx]
 *
 *********************************************************/
static int __sgm415xx_read_byte(struct sgm415xx_device *sgm, u8 reg, u8 *data)
{
    int ret = 0;
    int i = 0;

    for (i = 0; i < I2C_RETRY_CNT; ++i) {

        ret = i2c_smbus_read_byte_data(sgm->client, reg);

        if (ret >= 0)
            break;

        pr_info("[%s] reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
            __func__, reg, ret, i + 1, I2C_RETRY_CNT);
    }

    if (ret < 0) {
        pr_err("[%s] i2c read fail: can't read from reg 0x%02X\n", __func__, reg);
        return ret;
    }

    *data = (u8) ret;

    return 0;
}

static int __sgm415xx_write_byte(struct sgm415xx_device *sgm, int reg, u8 val)
{
    int ret = 0;
    int i = 0;

    for (i = 0; i < I2C_RETRY_CNT; ++i) {

        ret = i2c_smbus_write_byte_data(sgm->client, reg, val);

        if (ret >= 0)
            break;

        pr_info("[%s] reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
            __func__, reg, ret, i + 1, I2C_RETRY_CNT);
    }

    if (ret < 0) {
        pr_err("[%s] i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
                __func__,
                val, reg, ret);
        return ret;
    }

    return 0;
}

static int sgm415xx_read_reg(struct sgm415xx_device *sgm, u8 reg, u8 *data)
{
    int ret = 0;

    mutex_lock(&sgm->i2c_rw_lock);
    ret = __sgm415xx_read_byte(sgm, reg, data);
    mutex_unlock(&sgm->i2c_rw_lock);

    return ret;
}

static int sgm415xx_update_bits(struct sgm415xx_device *sgm, u8 reg,
                    u8 mask, u8 val)
{
    int ret = 0;
    u8 tmp = 0;

    mutex_lock(&sgm->i2c_rw_lock);
    ret = __sgm415xx_read_byte(sgm, reg, &tmp);
    if (ret) {
        pr_err("[%s] Failed: reg=%02X, ret=%d\n", __func__, reg, ret);
        goto out;
    }

    tmp &= ~mask;
    tmp |= val & mask;

    ret = __sgm415xx_write_byte(sgm, reg, tmp);
    if (ret) {
        pr_err("[%s] Failed: reg=%02X, ret=%d\n", __func__, reg, ret);
    }

out:
    mutex_unlock(&sgm->i2c_rw_lock);
    return ret;
}

/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/

static int sgm415xx_chg_attach_pre_process(struct sgm415xx_device *sgm, int attach)
{
    mutex_lock(&sgm->lock);
    sgm->typec_attached = attach;
    switch(attach) {
        case ATTACH_TYPE_TYPEC:
            schedule_delayed_work(&sgm->force_detect_dwork, 0);
            break;
        case ATTACH_TYPE_PD_SDP:
            sgm->chg_type = POWER_SUPPLY_TYPE_USB;
            sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
            sgm->typec_attached = true;
            break;
        case ATTACH_TYPE_PD_DCP:
            sgm->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
            sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
            sgm->typec_attached = true;
            break;
        case ATTACH_TYPE_PD_NONSTD:
            sgm->chg_type = POWER_SUPPLY_TYPE_USB;
            sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
            sgm->typec_attached = true;
            break;
        case ATTACH_TYPE_NONE:
            sgm->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
            sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
            sgm->typec_attached = false;
            break;
        default:
            pr_info("[%s] using tradtional bc12 flow!\n", __func__);
            break;
    }

    power_supply_changed(sgm->charger);

    pr_err("[%s] type(%d %d),attach:%d\n", __func__,
            sgm->chg_type, sgm->psy_usb_type, attach);
    mutex_unlock(&sgm->lock);

    return 0;
}

 static int sgm415xx_set_watchdog_timer(struct sgm415xx_device *sgm, int time)
{
    int ret = 0;
    u8 reg_val = 0;

    if (time == 0) {
        reg_val = SGM415XX_WDT_TIMER_DISABLE;
    } else if (time == 40) {
        reg_val = SGM415XX_WDT_TIMER_40S;
    } else if (time == 80) {
        reg_val = SGM415XX_WDT_TIMER_80S;
    } else {
        reg_val = SGM415XX_WDT_TIMER_160S;
    }

    ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_5,
                SGM415XX_WDT_TIMER_MASK, reg_val);

    return ret;
}

static int sgm415xx_set_term_curr(struct charger_device *chg_dev, u32 uA)
{
    u8 reg_val = 0;
    struct sgm415xx_device *sgm = charger_get_data(chg_dev);
    #if (defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))

    for (reg_val = 1; reg_val < 16 && uA >= ITERM_CURRENT_STABLE[reg_val]; reg_val++)
        ;
    reg_val--;
    #else
    if (uA < SGM415XX_TERMCHRG_I_MIN_uA) {
        uA = SGM415XX_TERMCHRG_I_MIN_uA;
    } else if (uA > SGM415XX_TERMCHRG_I_MAX_uA) {
        uA = SGM415XX_TERMCHRG_I_MAX_uA;
    }

    reg_val = (uA - SGM415XX_TERMCHRG_I_MIN_uA) / SGM415XX_TERMCHRG_CURRENT_STEP_uA;
    #endif //(defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))

    return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_3,
                  SGM415XX_TERMCHRG_CUR_MASK, reg_val);
}

static int sgm415xx_set_prechrg_curr(struct sgm415xx_device *sgm, int uA)
{
    u8 reg_val = 0;

    #if (defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))
    for (reg_val = 1; reg_val < 16 && uA >= IPRECHG_CURRENT_STABLE[reg_val]; reg_val++)
        ;
    reg_val--;
    #else
    if (uA < SGM415XX_PRECHRG_I_MIN_uA) {
        uA = SGM415XX_PRECHRG_I_MIN_uA;
    } else if (uA > SGM415XX_PRECHRG_I_MAX_uA) {
        uA = SGM415XX_PRECHRG_I_MAX_uA;
    }

    reg_val = (uA - SGM415XX_PRECHRG_I_MIN_uA) / SGM415XX_PRECHRG_CURRENT_STEP_uA;
    #endif //(defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))
    reg_val = reg_val << 4;
    return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_3,
                  SGM415XX_PRECHRG_CUR_MASK, reg_val);
}

static int sgm415xx_get_ichg_curr(struct charger_device *chg_dev, u32 *uA)
{
    int ret = 0;
    u8 ichg = 0;
    u32 curr = 0;

    struct sgm415xx_device *sgm = charger_get_data(chg_dev);

    ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_2, &ichg);
    if (ret) {
        return ret;
    }

    ichg &= SGM415XX_ICHRG_I_MASK;
    #if (defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))
    if (ichg <= 0x8) {
        curr = ichg * 5000;
    } else if (ichg <= 0xF) {
        curr = 40000 + (ichg - 0x8) * 10000;
    } else if (ichg <= 0x17) {
        curr = 110000 + (ichg - 0xF) * 20000;
    } else if (ichg <= 0x20) {
        curr = 270000 + (ichg - 0x17) * 30000;
    } else if (ichg <= 0x30) {
        curr = 540000 + (ichg - 0x20) * 60000;
    } else if (ichg <= 0x3C) {
        curr = 1500000 + (ichg - 0x30) * 120000;
    } else {
        curr = 3000000;
    }
    #else
    curr = ichg * SGM415XX_ICHRG_I_STEP_uA;
    #endif //(defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))

    *uA = curr;

    return 0;
}

static int sgm415xx_get_minichg_curr(struct charger_device *chg_dev, u32 *uA)
{
    *uA = SGM415XX_ICHRG_I_MIN_uA;

    return 0;
}

static int sgm415xx_set_ichrg(struct sgm415xx_device *sgm, unsigned int uA)
{
    int ret = 0;
    u8 reg_val = 0;

    if (uA < SGM415XX_ICHRG_I_MIN_uA) {
        uA = SGM415XX_ICHRG_I_MIN_uA;
    }
    else if ( uA > sgm->init_data.max_ichg) {
        uA = sgm->init_data.max_ichg;
    }
    #if (defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))
    if (uA <= 40000) {
        reg_val = uA / 5000;
    } else if (uA <= 110000) {
        reg_val = 0x08 + (uA -40000) / 10000;
    } else if (uA <= 270000) {
        reg_val = 0x0F + (uA -110000) / 20000;
    } else if (uA <= 540000) {
        reg_val = 0x17 + (uA -270000) / 30000;
    } else if (uA <= 1500000) {
        reg_val = 0x20 + (uA -540000) / 60000;
    } else if (uA <= 2940000) {
        reg_val = 0x30 + (uA -1500000) / 120000;
    } else {
        reg_val = 0x3d;
    }
    #else
    reg_val = uA / SGM415XX_ICHRG_I_STEP_uA;
    #endif //(defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))
    ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_2,
                  SGM415XX_ICHRG_I_MASK, reg_val);

    return ret;
}

static int sgm415xx_set_ichrg_curr(struct charger_device *chg_dev, unsigned int uA)
{
    int ret = 0;
    struct sgm415xx_device *sgm = charger_get_data(chg_dev);

    ret = sgm415xx_set_ichrg(sgm, uA);

    return ret;
}

static int sgm415xx_set_chrg_volt(struct charger_device *chg_dev, u32 uV)
{
    int ret = 0;
    u8 reg_val = 0;
    struct sgm415xx_device *sgm = charger_get_data(chg_dev);

    if (uV < SGM415XX_VREG_V_MIN_uV) {
        uV = SGM415XX_VREG_V_MIN_uV;
    } else if (uV > sgm->init_data.max_vreg) {
        uV = sgm->init_data.max_vreg;
    }

    reg_val = (uV-SGM415XX_VREG_V_MIN_uV) / SGM415XX_VREG_V_STEP_uV;
    reg_val = reg_val<<3;
    ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_4,
                  SGM415XX_VREG_V_MASK, reg_val);

    return ret;
}

static int sgm415xx_get_chrg_volt(struct charger_device *chg_dev, unsigned int *volt)
{
    struct sgm415xx_device *sgm = charger_get_data(chg_dev);
    int ret = 0;
    u8 vreg_val = 0;

    ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_4, &vreg_val);
    if (ret) {
        return ret;
    }

    vreg_val = (vreg_val & SGM415XX_VREG_V_MASK)>>3;

    if (15 == vreg_val) {
        *volt = 4352000; //default
    } else if (vreg_val < 25) {
        *volt = vreg_val*SGM415XX_VREG_V_STEP_uV + SGM415XX_VREG_V_MIN_uV;
    }

    return 0;
}

static int sgm415xx_get_vbat(struct sgm415xx_device *sgm, unsigned int *volt)
{
    int ret = 0;
    u8 vreg_val = 0;

    ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_4, &vreg_val);
    if (ret) {
        return ret;
    }

    vreg_val = (vreg_val & SGM415XX_VREG_V_MASK)>>3;

    if (15 == vreg_val) {
        *volt = 4352000; //default
    } else if (vreg_val < 25) {
        *volt = vreg_val*SGM415XX_VREG_V_STEP_uV + SGM415XX_VREG_V_MIN_uV;
    }

    return 0;
}

static int sgm415xx_get_vindpm_offset_os(struct sgm415xx_device *sgm)
{
    int ret = 0;
    u8 reg_val = 0;

    ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_f, &reg_val);
    if (ret) {
        return ret;
    }

    reg_val = reg_val & SGM415XX_VINDPM_OS_MASK;

    return reg_val;
}

static int sgm415xx_set_vindpm_offset_os(struct sgm415xx_device *sgm,u8 offset_os)
{
    int ret = 0;

    ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_f,
                  SGM415XX_VINDPM_OS_MASK, offset_os);

    if (ret) {
        pr_err("[%s] fail\n", __func__);
        return ret;
    }

    return ret;
}

static int sgm415xx_set_input_volt_lim(struct charger_device *chg_dev, unsigned int vindpm)
{
    struct sgm415xx_device *sgm = charger_get_data(chg_dev);
    int ret = 0;
    unsigned int offset = 0;
    u8 reg_val = 0;
    u8 os_val = 0;

    if (vindpm < SGM415XX_VINDPM_V_MIN_uV ||
        vindpm > SGM415XX_VINDPM_V_MAX_uV) {
         return -EINVAL;
    }

    if (vindpm < 5900000) {
        os_val = 0;
        offset = 3900000;
    } else if (vindpm >= 5900000 && vindpm < 7500000) {
        os_val = 1;
        offset = 5900000; //uv
    } else if (vindpm >= 7500000 && vindpm < 10500000) {
        os_val = 2;
        offset = 7500000; //uv
    } else {
        os_val = 3;
        offset = 10500000; //uv
    }

    sgm415xx_set_vindpm_offset_os(sgm, os_val);
    reg_val = (vindpm - offset) / SGM415XX_VINDPM_STEP_uV;
    ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_6,
                  SGM415XX_VINDPM_V_MASK, reg_val);

    return ret;
}

static int sgm415xx_set_vac_ovp(struct sgm415xx_device *sgm, int volt)
{
    u8 val;

    if (volt >= VAC_OVP_14000)
        val = SGM415XX_OVP_14P0V;
    else if (volt >= VAC_OVP_10500 && volt < VAC_OVP_14000)
        val = SGM415XX_OVP_10P5V;
    else if (volt >= VAC_OVP_6500 && volt < SGM415XX_OVP_10P5V)
        val = SGM415XX_OVP_6P5V;
    else
        val = SGM415XX_OVP_5P5V;

    pr_notice("[%s] value = %d", __func__, val);

    return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_6, SGM415XX_OVP_MASK,
                    val << SGM415XX_OVP_SHIFT);
}

static int sgm415xx_get_input_volt_lim(struct charger_device *chg_dev, u32 *uV)
{
    struct sgm415xx_device *sgm = charger_get_data(chg_dev);
    int ret = 0;
    int offset = 0;
    u8 vlim = 0;
    int temp = 0;

    ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_6, &vlim);
    if (ret) {
        return ret;
    }

    temp = sgm415xx_get_vindpm_offset_os(sgm);
    if (0 == temp) {
        offset = 3900000; //uv
    } else if (1 == temp) {
        offset = 5900000;
    } else if (2 == temp) {
        offset = 7500000;
    } else if (3 == temp) {
        offset = 10500000;
    } else {
        return temp;
    }

    *uV = offset + (vlim & 0x0F) * SGM415XX_VINDPM_STEP_uV;

    return 0;
}

static int sgm415xx_set_input_curr_lim(struct charger_device *chg_dev, unsigned int iindpm)
{
    struct sgm415xx_device *sgm = charger_get_data(chg_dev);
    int ret = 0;
    u8 reg_val = 0;

    if (iindpm < SGM415XX_IINDPM_I_MIN_uA ||
            iindpm > SGM415XX_IINDPM_I_MAX_uA) {
        return -EINVAL;
    }

    pr_err("[%s] value = %d", __func__, iindpm);
    #if (defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))
    reg_val = (iindpm-SGM415XX_IINDPM_I_MIN_uA) / SGM415XX_IINDPM_STEP_uA;
    #else
    if (iindpm >= SGM415XX_IINDPM_I_MIN_uA && iindpm <= 3100000) {//default
        reg_val = (iindpm-SGM415XX_IINDPM_I_MIN_uA) / SGM415XX_IINDPM_STEP_uA;
    } else if (iindpm > 3100000 && iindpm < SGM415XX_IINDPM_I_MAX_uA) {
        reg_val = 0x1E;
    } else {
        reg_val = SGM415XX_IINDPM_I_MASK;
    }
    #endif //(defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))
    ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_0,
                  SGM415XX_IINDPM_I_MASK, reg_val);
    return ret;
}

static int sgm415xx_get_input_curr_lim(struct charger_device *chg_dev,unsigned int *ilim)
{
    int ret = 0;
    u8 reg_val = 0;
    struct sgm415xx_device *sgm = charger_get_data(chg_dev);

    ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_0, &reg_val);
    if (ret) {
        return ret;
    }

    if (SGM415XX_IINDPM_I_MASK == (reg_val & SGM415XX_IINDPM_I_MASK)) {
        *ilim =  SGM415XX_IINDPM_I_MAX_uA;
    } else {
        *ilim = (reg_val & SGM415XX_IINDPM_I_MASK)*SGM415XX_IINDPM_STEP_uA + SGM415XX_IINDPM_I_MIN_uA;
    }

    return 0;
}

static int sgm415xx_get_input_mincurr_lim(struct charger_device *chg_dev,u32 *ilim)
{
    *ilim = SGM415XX_IINDPM_I_MIN_uA;

    return 0;
}

static int sgm415xx_chgtype_detect_is_done(struct sgm415xx_device * sgm)
{
    u8 chrg_stat,iindet_stat = 0;
    int ret = 0;

    pr_err("[%s] enter\n", __func__);

    ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_7, &iindet_stat);
    if (ret) {
        pr_err("[%s] Check iindet_stat failed\n", __func__);
        return false;
    }

    ret = sgm415xx_read_reg(sgm, SGM415XX_INPUT_DET, &chrg_stat);
    if (ret) {
        pr_err("[%s] Check DPDM detecte error\n" ,__func__);
    }

    if ((iindet_stat & SGM415XX_FORCE_DPDM) != 0) {
        pr_err("[%s] Reg07 = 0x%.2x\n", __func__, iindet_stat);
        return false;
    }

    if ((chrg_stat & SGM415XX_DPDM_ONGOING) != SGM415XX_DPDM_ONGOING) {
        pr_err("[%s] Reg0E = 0x%.2x\n", __func__, chrg_stat);
        return false;
    }

    return true;
}

static int sgm415xx_get_state(struct sgm415xx_device *sgm,
                 struct sgm415xx_state *state)
{
    u8 chrg_stat = 0;
    u8 chrg_param_0;
    u8 chrg_param_1 = 0;
    int ret = 0;

    ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_STAT, &chrg_stat);
    if (ret) {
        ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_STAT, &chrg_stat);
        if (ret) {
            pr_err("[%s] read SGM415XX_CHRG_STAT fail\n", __func__);
            return ret;
        }
    }

    state->chrg_type = chrg_stat & SGM415XX_VBUS_STAT_MASK;
    state->chrg_stat = chrg_stat & SGM415XX_CHG_STAT_MASK;
    state->online = !!(chrg_stat & SGM415XX_PG_STAT);
    state->vsys_stat = !!(chrg_stat & SGM415XX_VSYS_STAT);

    pr_err("[%s] chrg_type =%d,chrg_stat =%d online = %d\n",
            __func__, state->chrg_type, state->chrg_stat, state->online);

    ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_0, &chrg_param_0);
    if (ret) {
        pr_err("[%s] read SGM415XX_CHRG_CTRL_0 fail\n", __func__);
        return ret;
    }
    state->hiz_en = !!(chrg_param_0 & SGM415XX_HIZ_ENABLE);

    ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_5, &chrg_param_1);
    if (ret) {
        pr_err("[%s] read SGM415XX_CHRG_CTRL_5 fail\n", __func__);
        return ret;
    }
    state->term_en = !!(chrg_param_1 & SGM415XX_TERM_EN);

    return 0;
}

static int sgm415xx_enable_hvdcp(struct sgm415xx_device * sgm)
{
    int ret = 0;
    int dp_val = 0;
    int dm_val = 0;

    /*dp and dm connected, dp 0.6V dm Hiz*/
    dp_val = SGM415XX_CHRG_CTRL_2 << SGM415XX_EN_HVDCP_SHIFT;
    ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_d,
                    SGM415XX_DP_VSEL_MASK, dp_val); //dp 0.6V
    if (ret) {
        return ret;
    }

    ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_d,
                    SGM415XX_DM_VSEL_MASK, dm_val); //dm Hiz

    schedule_delayed_work(&sgm->hvdcp_done_work, msecs_to_jiffies(1500));

    return ret;
}

static int sgm415xx_force_dpdm(struct sgm415xx_device *sgm)
{
    const u8 val = SGM415XX_FORCE_DPDM << SGM415XX_FORCE_DPDM_SHIFT;

    return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_7, SGM415XX_FORCE_DPDM_MASK,
                val);
}

static void sgm415xx_force_detection_dwork_handler(struct work_struct *work)
{
    int ret = 0;
    struct delayed_work *force_detect_dwork = NULL;
    struct sgm415xx_device * sgm = NULL;

    force_detect_dwork = container_of(work, struct delayed_work, work);
    if(force_detect_dwork == NULL) {
        pr_err("[%s] Cann't get force_detect_dwork\n", __func__);
        return;
    }

    sgm = container_of(force_detect_dwork, struct sgm415xx_device, force_detect_dwork);
    if (sgm == NULL) {
        pr_err("[%s] Cann't get sgm415xx_device\n", __func__);
        return;
    }

    if (sgm->vbus_gd) {
        pr_info("[%s] a good VBUS attached!!!\n", __func__);
        msleep(50);
        ret = sgm415xx_force_dpdm(sgm);
        if (ret) {
            pr_err("[%s] force dpdm failed(%d)\n", __func__, ret);
            return;
        }
    } else {
        pr_info("[%s] a good VBUS is not attached, wait~~~\n", __func__);
        schedule_delayed_work(&sgm->force_detect_dwork, 0);
    }

    sgm->force_detect_count++;
}

static int sgm415xx_get_chgtype(struct sgm415xx_device *sgm,
                 struct sgm415xx_state *state)
{
    int retry_cnt = 0;
    int ret = 0;
    u8 chrg_stat = 0;

    if (sgm->typec_attached > ATTACH_TYPE_NONE && sgm->vbus_gd) {
        switch(sgm->typec_attached) {
            case ATTACH_TYPE_PD_SDP:
            case ATTACH_TYPE_PD_DCP:
            case ATTACH_TYPE_PD_NONSTD:
                pr_info("[%s] Attach PD_TYPE, skip bc12 flow!\n", __func__);
                return ret;
            default:
                break;
        }
    }

    do {
        if(!sgm415xx_chgtype_detect_is_done(sgm)) {
            pr_err("[%s] DPDM detecte not done, retry_cnt:%d\n", __func__, retry_cnt);
        } else {
            pr_err("[%s] BC1.2 done\n", __func__);
            break;
        }
        mdelay(60);
    } while (retry_cnt++ < BC12_DONE_TIMEOUT_CHECK_MAX_RETRY);

    ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_STAT, &chrg_stat);
    if (ret) {
        ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_STAT, &chrg_stat);
        if (ret) {
            pr_err("[%s] read SGM415XX_CHRG_STAT fail\n", __func__);
            return ret;
        }
    }

    state->chrg_type = chrg_stat & SGM415XX_VBUS_STAT_MASK;

    pr_err("[%s] chrg_type = %d\n", __func__, state->chrg_type);

    switch (state->chrg_type) {
        case SGM415XX_NOT_CHRGING:
            pr_err("[%s] SGM415XX charger type: NONE\n", __func__);
            sgm->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
            sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
            break;
        case SGM415XX_USB_SDP:
            pr_err("[%s] SGM415XX charger type: SDP\n", __func__);
            sgm->chg_type = POWER_SUPPLY_TYPE_USB;
            sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
            break;
        case SGM415XX_USB_CDP:
            pr_err("[%s] SGM415XX charger type: CDP\n", __func__);
            sgm->chg_type = POWER_SUPPLY_TYPE_USB_CDP;
            sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
            break;
        case SGM415XX_USB_DCP:
            pr_err("[%s] SGM415XX charger type: DCP\n", __func__);
            sgm->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
            sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
            sgm415xx_enable_hvdcp(sgm);
            break;
        case SGM415XX_UNKNOWN:
            pr_err("[%s] SGM415XX charger type: UNKNOWN\n", __func__);
            sgm->chg_type = POWER_SUPPLY_TYPE_USB;
            sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
            if (sgm->force_detect_count < 10) {
                schedule_delayed_work(&sgm->force_detect_dwork, 0);
            }
            break;
        case SGM415XX_NON_STANDARD:
            pr_err("[%s] SGM415XX charger type: UNKNOWN\n", __func__);
            sgm->chg_type = POWER_SUPPLY_TYPE_USB;
            sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
            break;
        default:
            pr_err("[%s] SGM415XX charger type: default\n", __func__);
            sgm->chg_type = POWER_SUPPLY_TYPE_USB;
            sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
            break;
        }

    return 0;
}

static int sgm415xx_get_chrg_stat(struct sgm415xx_device *sgm)
{
    u8 chrg_stat = 0;
    int chrg_value = 0;
    int ret = 0;

    ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_STAT, &chrg_stat);
    if (ret < 0) {
        ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_STAT, &chrg_stat);
        if (ret) {
            pr_err("[%s] read SGM415XX_CHRG_STAT fail\n", __func__);
            return ret;
        }
    }

    chrg_stat = chrg_stat & SGM415XX_CHG_STAT_MASK;
    chrg_value = POWER_SUPPLY_STATUS_CHARGING;
    if (!sgm->state.chrg_type || (sgm->state.chrg_type == SGM415XX_OTG_MODE)) {
        chrg_value = POWER_SUPPLY_STATUS_DISCHARGING;
    } else if (!chrg_stat) {
        if (sgm->chg_config) {
            chrg_value = POWER_SUPPLY_STATUS_CHARGING;
        } else {
            chrg_value = POWER_SUPPLY_STATUS_NOT_CHARGING;
        }
    } else if (chrg_stat == SGM415XX_TERM_CHRG) {
         chrg_value= POWER_SUPPLY_STATUS_FULL;
    } else {
         chrg_value = POWER_SUPPLY_STATUS_CHARGING;
    }

    return chrg_value;
}

static int sgm415xx_enable_charger(struct sgm415xx_device *sgm)
{
    int ret = 0;

    pr_info("[%s] enter\n", __func__);

    ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_1, SGM415XX_CHRG_EN,
                     SGM415XX_CHRG_EN);

    return ret;
}

static int sgm415xx_disable_charger(struct sgm415xx_device *sgm)
{
    int ret = 0;

    pr_info("[%s] enter\n", __func__);

    ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_1, SGM415XX_CHRG_EN,
                     0);

    return ret;
}

static int sgm415xx_is_charging(struct charger_device *chg_dev,bool *en)
{
    struct sgm415xx_device *sgm = charger_get_data(chg_dev);
    int ret = 0;
    u8 val = 0;

    ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_1, &val);
    if (ret) {
        pr_err("[%s] read SGM415XX_CHRG_CTRL_a fail\n", __func__);
        return ret;
    }

    *en = (val&SGM415XX_CHRG_EN)? 1 : 0;

    return ret;
}

static int sgm415xx_charging_switch(struct charger_device *chg_dev,bool enable)
{
    struct sgm415xx_device *sgm = charger_get_data(chg_dev);
    int ret = 0;

    pr_info("[%s] value = %d\n", __func__, enable);

    if (enable) {
        ret = sgm415xx_enable_charger(sgm);
    } else {
        ret = sgm415xx_disable_charger(sgm);
    }

    sgm->chg_config = enable;

    return ret;
}

static int sgm415xx_set_recharge_volt(struct sgm415xx_device *sgm, int mV)
{
    u8 reg_val = 0;

    reg_val = (mV - SGM415XX_VRECHRG_OFFSET_mV) / SGM415XX_VRECHRG_STEP_mV;

    return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_4,
                  SGM415XX_VRECHARGE, reg_val);
}

static int sgm415xx_set_wdt_rst(struct sgm415xx_device *sgm, bool is_rst)
{
    u8 val = 0;

    if (is_rst) {
        val = SGM415XX_WDT_RST_MASK;
    } else {
        val = 0;
    }

    return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_1,
                  SGM415XX_WDT_RST_MASK, val);
}

/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
static int sgm415xx_dump_register(struct charger_device *chg_dev)
{
    struct sgm415xx_device *sgm = charger_get_data(chg_dev);
    unsigned char i = 0;
    unsigned int ret = 0;
    unsigned char sgm415xx_reg[SGM415XX_REG_NUM+1] = { 0 };

    for (i = 0; i < SGM415XX_REG_NUM+1; i++) {
        ret = sgm415xx_read_reg(sgm,i, &sgm415xx_reg[i]);
        if (ret != 0) {
            pr_info("[%s] i2c transfor error\n", __func__);
            return 1;
        }
        pr_info("[%s] [0x%x]=0x%x ", __func__, i, sgm415xx_reg[i]);
    }

    return 0;
}


/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
static int sgm415xx_hw_chipid_detect(struct sgm415xx_device *sgm)
{
    int ret = 0;
    u8 val = 0;

    ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_b, &val);
    if (ret < 0) {
        pr_info("[%s] read SGM415XX_CHRG_CTRL_b fail\n", __func__);
        return ret;
    }
    val = val & SGM415XX_PN_MASK;
    pr_info("[%s] Reg[0x0B]=0x%x\n", __func__, val);

    return val;
}

static int sgm415xx_reset_watch_dog_timer(struct charger_device *chg_dev)
{
    struct sgm415xx_device *sgm = charger_get_data(chg_dev);
    int ret = 0;

    pr_info("[%s] enter\n", __func__);

    ret = sgm415xx_set_wdt_rst(sgm,0x1);    /* RST watchdog */

    return ret;
}

int sgm415xx_reset_chip(struct sgm415xx_device *sgm)
{
    int ret = 0;
    u8 val = SGM415XX_REG_RESET << SGM415XX_REG_RESET_SHIFT;

    ret =
        sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_b, SGM415XX_REG_RESET_MASK, val);
    return ret;
}

static int sgm415xx_get_charging_status(struct charger_device *chg_dev,
                       bool *is_done)
{
    struct sgm415xx_device *sgm = charger_get_data(chg_dev);
    int ret = 0;

    ret = sgm415xx_get_chrg_stat(sgm);
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

static int sgm415xx_set_int_mask(struct sgm415xx_device *sgm)
{
    int ret = 0;

    ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_a,
                SGM415XX_INT_MASK, SGM415XX_INT_MASK);

    return ret;
}

static int sgm415xx_set_en_timer(struct sgm415xx_device *sgm)
{
    int ret = 0;

    ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_5,
                SGM415XX_SAFETY_TIMER_EN, SGM415XX_SAFETY_TIMER_EN);

    return ret;
}

static int sgm415xx_set_disable_timer(struct sgm415xx_device *sgm)
{
    int ret = 0;

    ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_5,
                SGM415XX_SAFETY_TIMER_EN, 0);

    return ret;
}

static int sgm415xx_enable_safetytimer(struct charger_device *chg_dev,bool en)
{
    struct sgm415xx_device *sgm = charger_get_data(chg_dev);
    int ret = 0;

    if (en) {
        ret = sgm415xx_set_en_timer(sgm);
    } else {
        ret = sgm415xx_set_disable_timer(sgm);
    }

    return ret;
}

static int sgm415xx_get_is_safetytimer_enable(struct charger_device
        *chg_dev,bool *en)
{
    int ret = 0;
    u8 val = 0;

    struct sgm415xx_device *sgm = charger_get_data(chg_dev);

    ret = sgm415xx_read_reg(sgm,SGM415XX_CHRG_CTRL_5,&val);
    if (ret < 0) {
        pr_info("[%s] read SGM415XX_CHRG_CTRL_5 fail\n", __func__);
        return ret;
    }

    *en = !!(val & SGM415XX_SAFETY_TIMER_EN);

    return 0;
}

int sgm415xx_enter_hiz_mode(struct sgm415xx_device *sgm)
{
    u8 val = SGM415XX_HIZ_ENABLE << SGM415XX_ENHIZ_SHIFT;

    return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_0, SGM415XX_ENHIZ_MASK, val);

}

int sgm415xx_exit_hiz_mode(struct sgm415xx_device *sgm)
{

    u8 val = SGM415XX_HIZ_DISABLE << SGM415XX_ENHIZ_SHIFT;

    return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_0, SGM415XX_ENHIZ_MASK, val);

}

static int sgm415xx_set_shipmode(struct charger_device *chg_dev, bool en)
{
    int ret = 0;
    u8 val = 0;
    struct sgm415xx_device *sgm = dev_get_drvdata(&chg_dev->dev);

    if (en) {
        ret += sgm415xx_enter_hiz_mode(sgm);
        val = SGM415XX_BATFET_OFF << SGM415XX_BATFET_DLY_SHIFT;
        ret += sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_7, SGM415XX_BATFET_DLY_MASK, val);
        val = SGM415XX_BATFET_OFF << SGM415XX_BATFET_DIS_SHIFT;
        ret += sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_7, SGM415XX_BATFET_DIS_MASK, val);
    } else {
        val = SGM415XX_BATFET_ON << SGM415XX_BATFET_DIS_SHIFT;
        ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_7, SGM415XX_BATFET_DIS_MASK, val);
    }

    pr_err("[%s] %s shipmode %s\n", __func__, en ? "enable" : "disable",
            !ret ? "successfully" : "failed");

    return ret;
}

static int sgm415xx_get_shipmode(struct charger_device *chg_dev)
{
    int ret = 0;
    u8 val = 0;
    struct sgm415xx_device *sgm = dev_get_drvdata(&chg_dev->dev);

    msleep(100);
    ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_7, &val);
    if (ret == 0){
        pr_err("[%s] Reg[%.2x] = 0x%.2x\n", __func__, SGM415XX_CHRG_CTRL_7, val);
    } else {
        pr_err("[%s] get shipmode reg fail! \n",__func__);
        return ret;
    }
    ret = (val & SGM415XX_BATFET_DIS_MASK) >> SGM415XX_BATFET_DIS_SHIFT;
    pr_err("[%s] shipmode %s\n", __func__, ret ? "enabled" : "disabled");

    return ret;
}
static int sgm415xx_disable_battfet_rst(struct sgm415xx_device *sgm)
{
    int ret = 0;
    u8 val = 0;

    val = SGM415XX_BATFET_RST_DISABLE << SGM415XX_BATFET_RST_EN_SHIFT;
    ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_7, SGM415XX_BATFET_RST_EN_MASK, val);

    pr_err("[%s] disable BATTFET_RST_EN %s\n", __func__, !ret ? "successfully" : "failed");

    return ret;
}

#if (defined(__SGM41542_CHIP_ID__)|| defined(__SGM41516D_CHIP_ID__)|| defined(__SGM41543D_CHIP_ID__))
static int sgm415xx_en_pe_current_partern(struct charger_device
        *chg_dev,bool is_up)
{
    int ret = 0;
    struct sgm415xx_device *sgm = charger_get_data(chg_dev);

    ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_d,
                SGM415XX_EN_PUMPX, SGM415XX_EN_PUMPX);
    if (ret < 0) {
        pr_info("[%s] read SGM415XX_CHRG_CTRL_d fail\n", __func__);
        return ret;
    }
    if (is_up) {
        ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_d,
                SGM415XX_PUMPX_UP, SGM415XX_PUMPX_UP);
    } else {
        ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_d,
                SGM415XX_PUMPX_DN, SGM415XX_PUMPX_DN);
    }

    return ret;
}
#endif //(defined(__SGM41542_CHIP_ID__)|| defined(__SGM41516D_CHIP_ID__)|| defined(__SGM41543D_CHIP_ID__))

static enum power_supply_property sgm415xx_power_supply_props[] = {
    POWER_SUPPLY_PROP_MANUFACTURER,
    POWER_SUPPLY_PROP_ONLINE,
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
    POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
    POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
    POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
    POWER_SUPPLY_PROP_USB_TYPE,
    POWER_SUPPLY_PROP_CURRENT_MAX,
    POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static int sgm415xx_property_is_writeable(struct power_supply *psy,
                     enum power_supply_property prop)
{
    switch (prop) {
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
    case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
    case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
    case POWER_SUPPLY_PROP_STATUS:
    case POWER_SUPPLY_PROP_ONLINE:
    case POWER_SUPPLY_PROP_ENERGY_EMPTY:
        return 1;
    default:
        return 0;
    }
    return 0;
}

static int sgm415xx_charger_set_property(struct power_supply *psy,
        enum power_supply_property prop,
        const union power_supply_propval *val)
{
    int ret = 0;
    struct sgm415xx_device *sgm = power_supply_get_drvdata(psy);

    switch (prop) {
        case POWER_SUPPLY_PROP_ONLINE:
            pr_info("[%s] %d\n", __func__, val->intval);
            ret = sgm415xx_chg_attach_pre_process(sgm, val->intval);
            break;
        case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
            ret = sgm415xx_set_input_curr_lim(s_chg_dev_otg, val->intval);
            break;
        default:
            return -EINVAL;
    }

    return ret;
}

static int sgm415xx_charger_get_property(struct power_supply *psy,
                enum power_supply_property psp,
                union power_supply_propval *val)
{
    struct sgm415xx_device *sgm = power_supply_get_drvdata(psy);
    struct sgm415xx_state state;
    int ret = 0;
    int data = 0;

    mutex_lock(&sgm->lock);
    state = sgm->state;
    mutex_unlock(&sgm->lock);
    if (ret) {
        return ret;
    }

    switch (psp) {
    case POWER_SUPPLY_PROP_STATUS:
        ret = sgm415xx_get_chrg_stat(sgm);
        if (ret < 0) {
            break;
        }
        val->intval = ret;
        break;
    case POWER_SUPPLY_PROP_CHARGE_TYPE:
        switch (state.chrg_stat) {
        case SGM415XX_PRECHRG:
            val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
            break;
        case SGM415XX_FAST_CHRG:
            val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
            break;
        case SGM415XX_TERM_CHRG:
            val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
            break;
        case SGM415XX_NOT_CHRGING:
            val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
            break;
        default:
            val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
        }
        break;
    case POWER_SUPPLY_PROP_MANUFACTURER:
        val->strval = SGM415XX_MANUFACTURER;
        break;
    case POWER_SUPPLY_PROP_USB_TYPE:
        val->intval = sgm->psy_usb_type;
        break;
    case POWER_SUPPLY_PROP_MODEL_NAME:
        val->strval = SGM415XX_NAME;
        break;

    case POWER_SUPPLY_PROP_ONLINE:
        // val->intval = sgm->vbus_gd | (!!sgm->typec_attached);
        if (((sgm->typec_attached > ATTACH_TYPE_NONE) || sgm->vbus_gd) &&
            (sgm->chg_type == POWER_SUPPLY_TYPE_UNKNOWN)) {
            val->intval = 0;
        } else {
            val->intval = sgm->vbus_gd | (!!sgm->typec_attached);
        }
        break;
    case POWER_SUPPLY_PROP_PRESENT:
        val->intval = sgm->vbus_gd;
        break;
    case POWER_SUPPLY_PROP_TYPE:
        val->intval = sgm->chg_type;
        break;

    case POWER_SUPPLY_PROP_HEALTH:
        if (state.chrg_fault & 0xF8) {
            val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
        } else {
            val->intval = POWER_SUPPLY_HEALTH_GOOD;
        }

        switch (state.health) {
        case SGM415XX_TEMP_HOT:
            val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
            break;
        case SGM415XX_TEMP_WARM:
            val->intval = POWER_SUPPLY_HEALTH_GOOD;
            break;
        case SGM415XX_TEMP_COOL:
            val->intval = POWER_SUPPLY_HEALTH_GOOD;
            break;
        case SGM415XX_TEMP_COLD:
            val->intval = POWER_SUPPLY_HEALTH_COLD;
            break;
        }
        break;

    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
        //val->intval = state.vbus_adc;
        break;

    case POWER_SUPPLY_PROP_CURRENT_NOW:
        //val->intval = state.ibus_adc;
        break;

    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        break;

    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
        ret = sgm415xx_get_vbat(sgm, &data);
        if (ret < 0) {
            pr_err("[%s] Cann't get vreg\n", __func__);
            break;
        }
        val->intval = data / 1000;
        break;

    default:
        return -EINVAL;
    }

    return ret;
}

static void charger_detect_work_func(struct work_struct *work)
{
    struct delayed_work *charge_detect_delayed_work = NULL;
    struct sgm415xx_device * sgm = NULL;
    int curr_in_limit = 0;
    struct sgm415xx_state state;
    int ret = 0;

    charge_detect_delayed_work = container_of(work, struct delayed_work, work);
    if(charge_detect_delayed_work == NULL) {
        pr_err("[%s] Cann't get charge_detect_delayed_work\n", __func__);
        return;
    }

    sgm = container_of(charge_detect_delayed_work, struct sgm415xx_device, charge_detect_delayed_work);
    if (sgm == NULL) {
        pr_err("[%s] Cann't get sgm415xx_device\n", __func__);
        return ;
    }

    if (!sgm->charger_wakelock->active) {
        __pm_stay_awake(sgm->charger_wakelock);
    }

    sgm415xx_get_chgtype(sgm, &state);
    mutex_lock(&sgm->lock);
    sgm->state = state;
    mutex_unlock(&sgm->lock);

    switch(sgm->state.chrg_type) {
        case SGM415XX_USB_SDP:
            pr_err("[%s] SGM415XX charger type: SDP\n", __func__);
            curr_in_limit = 500000;
            break;

        case SGM415XX_USB_CDP:
            pr_err("[%s] SGM415XX charger type: CDP\n", __func__);
            curr_in_limit = 1000000;
            break;

        case SGM415XX_USB_DCP:
            pr_err("[%s] SGM415XX charger type: DCP\n", __func__);
            curr_in_limit = 1500000;
            break;

        case SGM415XX_UNKNOWN:
            pr_err("[%s] SGM415XX charger type: UNKNOWN\n", __func__);
            curr_in_limit = 500000;
            break;

        default:
            pr_err("[%s] SGM415XX charger type: default\n", __func__);
            curr_in_limit = 500000;
            break;
    }

    //set charge parameters
    pr_err("[%s] update: curr_in_limit = %d\n", __func__, curr_in_limit);
    sgm415xx_set_input_curr_lim(sgm->chg_dev, curr_in_limit);

    sgm415xx_dump_register(sgm->chg_dev);

   ret = sgm415xx_get_state(sgm, &state);
    mutex_lock(&sgm->lock);
    sgm->state = state;
    mutex_unlock(&sgm->lock);
    //release wakelock
    power_supply_changed(sgm->charger);
    pr_err("[%s] Relax wakelock\n", __func__);
    __pm_relax(sgm->charger_wakelock);
    return;
}

static irqreturn_t sgm415xx_irq_handler_thread(int irq, void *private)
{
    struct sgm415xx_device *sgm = private;
    int ret = 0;
    u8 reg_val = 0;
    bool vbus_gd_pre = false;
    bool power_good_pre = false;

    //lock wakelock
    __pm_wakeup_event(sgm->charger_wakelock, 500);

    ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_a, &reg_val);
    if (ret) {
        pr_err("[%s] read SGM415XX_CHRG_CTRL_a fail\n", __func__);
        return IRQ_HANDLED;
    }
    vbus_gd_pre = sgm->vbus_gd;
    sgm->vbus_gd = !!(reg_val & SGM415XX_VBUS_GOOD);

    ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_STAT, &reg_val);
    if (ret) {
        ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_STAT, &reg_val);
        if (ret) {
            pr_err("[%s] read SGM415XX_CHRG_STAT fail\n", __func__);
            return IRQ_HANDLED;
        }
    }
    power_good_pre = sgm->power_good;
    sgm->power_good = !!(reg_val & SGM415XX_PG_STAT);

    if (!vbus_gd_pre && sgm->vbus_gd) {
        pr_err("[%s] adapter/usb inserted\n", __func__);
        sgm->force_detect_count = 0;
        sgm->chg_config = true;

    } else if (vbus_gd_pre && !sgm->vbus_gd) {

        pr_err("[%s] adapter/usb remove\n", __func__);
        sgm415xx_set_ichrg(sgm, SGM415XX_ICHG_LIM);
        sgm->chg_config = false;
        cancel_delayed_work_sync(&sgm->force_detect_dwork);
        cancel_delayed_work_sync(&sgm->hvdcp_done_work);
        sgm->hvdcp_done = false;
        return IRQ_HANDLED;
    }

    /* avoid PD power_off_charging reboot, and AC exit HIZ need redo HVDCP */
    if ((sgm->typec_attached == ATTACH_TYPE_TYPEC) &&
        (sgm->chg_type == POWER_SUPPLY_TYPE_UNKNOWN) &&
        (!power_good_pre && sgm->power_good)) {
        pr_info("[%s] trigger BC12 irq\n", __func__);
        schedule_delayed_work(&sgm->charge_detect_delayed_work, 0);
    }

    power_supply_changed(sgm->charger);

    sgm415xx_dump_register(sgm->chg_dev);

    return IRQ_HANDLED;
}

static void sgm415xx_inform_prob_dwork_handler(struct work_struct *work)
{
    struct sgm415xx_device *sgm = container_of(work, struct sgm415xx_device,
                                prob_dwork.work);

    pr_info("[%s] start\n", __func__);

    sgm415xx_irq_handler_thread(sgm->client->irq, (void *) sgm);
}

static void sgm415xx_hvdcp_done_work_func(struct work_struct *work)
{
    struct delayed_work *hvdcp_done_work = NULL;
    struct sgm415xx_device *sgm = NULL;

    hvdcp_done_work = container_of(work, struct delayed_work, work);
    if (hvdcp_done_work == NULL) {
        pr_err("[%s] Cann't get hvdcp_done_work\n", __func__);
        return;
    }

    sgm = container_of(hvdcp_done_work, struct sgm415xx_device,
                      hvdcp_done_work);
    if (sgm == NULL) {
        pr_err("[%s] Cann't get sc8960x_chip\n", __func__);
        return;
    }

    sgm->hvdcp_done = true;
    power_supply_changed(sgm->charger);
    pr_info("[%s] HVDCP end\n", __func__);
}

static char *sgm415xx_charger_supplied_to[] = {
    "battery",
    "mtk-master-charger",
};

static struct power_supply_desc sgm415xx_power_supply_desc = {
    .name = "charger",
    .type = POWER_SUPPLY_TYPE_UNKNOWN, // avoid healthd chg=au
    .usb_types = sgm415xx_usb_type,
    .num_usb_types = ARRAY_SIZE(sgm415xx_usb_type),
    .properties = sgm415xx_power_supply_props,
    .num_properties = ARRAY_SIZE(sgm415xx_power_supply_props),
    .get_property = sgm415xx_charger_get_property,
    .set_property = sgm415xx_charger_set_property,
    .property_is_writeable = sgm415xx_property_is_writeable,
};

static int sgm415xx_power_supply_init(struct sgm415xx_device *sgm,
                            struct device *dev)
{
    struct power_supply_config psy_cfg = {
        .drv_data = sgm,
        .of_node = dev->of_node,
        .supplied_to = sgm415xx_charger_supplied_to,
        .num_supplicants = ARRAY_SIZE(sgm415xx_charger_supplied_to),
    };

    memcpy(&sgm->psy_desc, &sgm415xx_power_supply_desc, sizeof(sgm->psy_desc));
    sgm->charger = devm_power_supply_register(sgm->dev,
                         &sgm->psy_desc,
                         &psy_cfg);
    if (IS_ERR(sgm->charger))
        return -EINVAL;

    return 0;
}

static int sgm415xx_hw_init(struct sgm415xx_device *sgm)
{
    struct power_supply_battery_info bat_info = { };
    int ret = 0;

    sgm415xx_reset_chip(sgm);

    bat_info.constant_charge_current_max_ua =
            SGM415XX_ICHRG_I_DEF_uA;

    bat_info.constant_charge_voltage_max_uv =
            SGM415XX_VREG_V_DEF_uV;

    bat_info.precharge_current_ua =
            SGM415XX_PRECHRG_I_DEF_uA;

    bat_info.charge_term_current_ua =
            SGM415XX_TERMCHRG_I_DEF_uA;

    sgm->init_data.max_ichg =
            SGM415XX_ICHRG_I_MAX_uA;

    sgm->init_data.max_vreg =
            SGM415XX_VREG_V_MAX_uV;

    sgm415xx_set_watchdog_timer(sgm,0);

    ret = sgm415xx_set_ichrg_curr(s_chg_dev_otg,
                bat_info.constant_charge_current_max_ua);
    if (ret) {
        goto err_out;
    }

    ret = sgm415xx_set_prechrg_curr(sgm, bat_info.precharge_current_ua);
    if (ret) {
        goto err_out;
    }

    ret = sgm415xx_set_chrg_volt(s_chg_dev_otg,
                bat_info.constant_charge_voltage_max_uv);
    if (ret) {
        goto err_out;
    }

    ret = sgm415xx_set_term_curr(s_chg_dev_otg, bat_info.charge_term_current_ua);
    if (ret) {
        goto err_out;
    }


    ret = sgm415xx_set_input_volt_lim(s_chg_dev_otg, sgm->init_data.vlim);
    if (ret)
        goto err_out;

    ret = sgm415xx_set_input_curr_lim(s_chg_dev_otg, sgm->init_data.ilim);
    if (ret) {
        goto err_out;
    }

    ret = sgm415xx_set_vac_ovp(sgm, VAC_OVP_14000);//14V
    if (ret)
        goto err_out;

    ret = sgm415xx_set_recharge_volt(sgm, 200);//100~200mv
    if (ret)
        goto err_out;

    ret = sgm415xx_set_int_mask(sgm);
    if (ret)
        goto err_out;

    ret = sgm415xx_disable_battfet_rst(sgm);
    if (ret)
        goto err_out;

    pr_info("[%s] ichrg_curr:%d prechrg_curr:%d chrg_vol:%d term_curr:%d input_curr_lim:%d",
             __func__,
             bat_info.constant_charge_current_max_ua,
             bat_info.precharge_current_ua,
             bat_info.constant_charge_voltage_max_uv,
             bat_info.charge_term_current_ua,
             sgm->init_data.ilim);

    return 0;

err_out:
    return ret;

}

static int sgm415xx_parse_dt(struct sgm415xx_device *sgm)
{
    int ret = 0;
    int irq_gpio = 0;
    int irqn = 0;

    pr_err("[%s] enter\n", __func__);

    ret = device_property_read_u32(sgm->dev,
                       "input-voltage-limit-microvolt",
                       &sgm->init_data.vlim);
    if (ret) {
        sgm->init_data.vlim = SGM415XX_VINDPM_DEF_uV;
    }

    if (sgm->init_data.vlim > SGM415XX_VINDPM_V_MAX_uV ||
        sgm->init_data.vlim < SGM415XX_VINDPM_V_MIN_uV) {
        return -EINVAL;
    }

    ret = device_property_read_u32(sgm->dev,
                       "input-current-limit-microamp",
                       &sgm->init_data.ilim);
    if (ret) {
        sgm->init_data.ilim = SGM415XX_IINDPM_DEF_uA;
    }

    if (sgm->init_data.ilim > SGM415XX_IINDPM_I_MAX_uA ||
        sgm->init_data.ilim < SGM415XX_IINDPM_I_MIN_uA) {
        return -EINVAL;
    }

    irq_gpio = of_get_named_gpio(sgm->dev->of_node, "sgm,intr_gpio", 0);
    if (!gpio_is_valid(irq_gpio)) {
        pr_err("[%s] %d gpio get failed\n", __func__, irq_gpio);
        return -EINVAL;
    }
    ret = gpio_request(irq_gpio, "sgm415xx irq pin");
    if (ret) {
        pr_err("[%s] %d gpio request failed\n", __func__, irq_gpio);
        return ret;
    }
    gpio_direction_input(irq_gpio);
    irqn = gpio_to_irq(irq_gpio);
    if (irqn < 0) {
        pr_err("[%s] %d gpio_to_irq failed\n", __func__, irqn);
        return irqn;
    }
    sgm->client->irq = irqn;

    pr_err("[%s] loaded\n", __func__);

    return 0;
}

static int sgm415xx_enable_vbus(struct regulator_dev *rdev)
{
    struct sgm415xx_device *sgm = charger_get_data(s_chg_dev_otg);
    int ret = 0;

    pr_info("[%s] enter\n", __func__);

    ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_1, SGM415XX_OTG_EN,
                     SGM415XX_OTG_EN);

    return ret;
}

static int sgm415xx_disable_vbus(struct regulator_dev *rdev)
{
    struct sgm415xx_device *sgm = charger_get_data(s_chg_dev_otg);
    int ret = 0;

    pr_info("[%s] enter\n", __func__);

    ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_1, SGM415XX_OTG_EN,
                     0);

    return ret;
}

static int sgm415xx_is_enabled_vbus(struct regulator_dev *rdev)
{
    struct sgm415xx_device *sgm = charger_get_data(s_chg_dev_otg);
    u8 temp = 0;
    int ret = 0;

    pr_info("[%s] enter\n", __func__);

    ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_1, &temp);
    return (temp&SGM415XX_OTG_EN)? 1 : 0;
}

static int sgm415xx_enable_otg(struct charger_device *chg_dev, bool en)
{
    int ret = 0;

    pr_info("[%s] en = %d\n", __func__, en);
    if (en) {
        ret = sgm415xx_enable_vbus(NULL);
    } else {
        ret = sgm415xx_disable_vbus(NULL);
    }
    return ret;
}

static int sgm415xx_set_boost_current_limit(struct charger_device *chg_dev, u32 uA)
{
    struct sgm415xx_device *sgm = charger_get_data(chg_dev);
    int ret = 0;

    if (uA == BOOST_CURRENT_LIMIT[0]) {
        ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_2, SGM415XX_BOOST_LIM,
                     0);
    }

    else if (uA == BOOST_CURRENT_LIMIT[1]) {
        ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_2, SGM415XX_BOOST_LIM,
                     BIT(7));
    }
    return ret;
}

static int sgm415xx_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
    struct sgm415xx_device *sgm = charger_get_data(chg_dev);
    pr_info("[%s]\n", __func__);

    switch (event) {
        case EVENT_FULL:
        case EVENT_RECHARGE:
        case EVENT_DISCHARGE:
            power_supply_changed(sgm->charger);
            break;
        default:
            break;
    }

    return 0;
}

static int sgm415xx_get_hvdcp_status(struct charger_device *chg_dev)
{
    int ret = 0;
    struct sgm415xx_device *sgm = charger_get_data(chg_dev);

    if (sgm == NULL) {
        pr_info("[%s] fail\n", __func__);
        return ret;
    }

    ret = (sgm->hvdcp_done ? 1 : 0);

    return ret;
}

static struct regulator_ops sgm415xx_vbus_ops = {
    .enable = sgm415xx_enable_vbus,
    .disable = sgm415xx_disable_vbus,
    .is_enabled = sgm415xx_is_enabled_vbus,
};

static const struct regulator_desc sgm415xx_otg_rdesc = {
    .of_match = "usb-otg-vbus",
    .name = "usb-otg-vbus",
    .ops = &sgm415xx_vbus_ops,
    .owner = THIS_MODULE,
    .type = REGULATOR_VOLTAGE,
    .fixed_uV = 5000000,
    .n_voltages = 1,
};

static int sgm415xx_vbus_regulator_register(struct sgm415xx_device *sgm)
{
    struct regulator_config config = {};
    int ret = 0;
    /* otg regulator */
    config.dev = sgm->dev;
    config.driver_data = sgm;
    sgm->otg_rdev = devm_regulator_register(sgm->dev,
                        &sgm415xx_otg_rdesc, &config);
    sgm->otg_rdev->constraints->valid_ops_mask |= REGULATOR_CHANGE_STATUS;
    if (IS_ERR(sgm->otg_rdev)) {
        ret = PTR_ERR(sgm->otg_rdev);
        pr_info("[%s] register otg regulator failed (%d)\n", __func__, ret);
    }
    return ret;
}

static int sgm415xx_enable_port_charging(struct charger_device *chg_dev, bool enable)
{
    int ret;
    struct sgm415xx_device *sgm = dev_get_drvdata(&chg_dev->dev);

    if (enable) {
        ret = sgm415xx_exit_hiz_mode(sgm);
        if (sgm->chg_type == POWER_SUPPLY_TYPE_USB_DCP) {
            sgm->hvdcp_done = false;
            sgm->force_detect_count = 0;
            sgm->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
            schedule_delayed_work(&sgm->force_detect_dwork, 0);
        }
    } else {
        ret = sgm415xx_enter_hiz_mode(sgm);
    }

    pr_err("[%s] %s hiz mode %s\n",
            __func__,
            !enable ? "enable" : "disable",
            !ret ? "successfully" : "failed");

    return ret;
}

static int sgm415xx_is_port_charging_enabled(struct charger_device *chg_dev, bool *enable)
{
    int ret = 0;
    struct sgm415xx_device *sgm = dev_get_drvdata(&chg_dev->dev);
    u8 val = 0;
    ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_0, &val);
    if (ret == 0) {
        pr_err("[%s] Reg[%.2x] = 0x%.2x\n", __func__, SGM415XX_CHRG_CTRL_0, val);
    }

    ret = (val & SGM415XX_ENHIZ_MASK) >> SGM415XX_ENHIZ_SHIFT;
    pr_err("[%s] hiz mode %s\n",__func__, ret ? "enabled" : "disabled");

    *enable = ret;

    return ret;
}

static struct charger_ops sgm415xx_chg_ops = {

    .dump_registers = sgm415xx_dump_register,
    /* cable plug in/out */
   //.plug_in = mt6375_plug_in,
   //.plug_out = mt6375_plug_out,
   /* enable */
   .enable = sgm415xx_charging_switch,
   .is_enabled = sgm415xx_is_charging,
   /* charging current */
   .set_charging_current = sgm415xx_set_ichrg_curr,
   .get_charging_current = sgm415xx_get_ichg_curr,
   .get_min_charging_current = sgm415xx_get_minichg_curr,
   /* charging voltage */
   .set_constant_voltage = sgm415xx_set_chrg_volt,
   .get_constant_voltage = sgm415xx_get_chrg_volt,
   /* input current limit */
   .set_input_current = sgm415xx_set_input_curr_lim,
   .get_input_current = sgm415xx_get_input_curr_lim,
   .get_min_input_current = sgm415xx_get_input_mincurr_lim,
   /* MIVR */
   .set_mivr = sgm415xx_set_input_volt_lim,
   .get_mivr = sgm415xx_get_input_volt_lim,
   /* ADC */
   //.get_adc = mt6375_get_adc,
   //.get_vbus_adc = mt6375_get_vbus,
   //.get_ibus_adc = mt6375_get_ibus,
   //.get_ibat_adc = mt6375_get_ibat,
   //.get_tchg_adc = mt6375_get_tchg,
   //.get_zcv = mt6375_get_zcv,
   /* charing termination */
   .set_eoc_current = sgm415xx_set_term_curr,
   //.enable_termination = mt6375_enable_te,
   //.reset_eoc_state = mt6375_reset_eoc_state,
   //.safety_check = mt6375_sw_check_eoc,
   .is_charging_done = sgm415xx_get_charging_status,
   /* power path */
   //.enable_powerpath = mt6375_enable_buck,
   //.is_powerpath_enabled = mt6375_is_buck_enabled,
   /* timer */
   .enable_safety_timer = sgm415xx_enable_safetytimer,
   .is_safety_timer_enabled = sgm415xx_get_is_safetytimer_enable,
   .kick_wdt = sgm415xx_reset_watch_dog_timer,
   /* AICL */
   //.run_aicl = mt6375_run_aicc,
   /* PE+/PE+20 */
   #if (defined(__SGM41542_CHIP_ID__)|| defined(__SGM41516D_CHIP_ID__)|| defined(__SGM41543D_CHIP_ID__))
    .send_ta_current_pattern = sgm415xx_en_pe_current_partern,
    #else
    .send_ta_current_pattern = NULL,
    #endif //(defined(__SGM41542_CHIP_ID__)|| defined(__SGM41516D_CHIP_ID__)|| defined(__SGM41543D_CHIP_ID__))
   //.set_pe20_efficiency_table = mt6375_set_pe20_efficiency_table,
   //.send_ta20_current_pattern = mt6375_set_pe20_current_pattern,
   //.reset_ta = mt6375_reset_pe_ta,
   //.enable_cable_drop_comp = mt6
   /* OTG */
    .enable_otg = sgm415xx_enable_otg,
    .set_boost_current_limit = sgm415xx_set_boost_current_limit,
    .event = sgm415xx_do_event,
    .get_hvdcp_status = sgm415xx_get_hvdcp_status,

   /* input_suspend */
    .enable_port_charging = sgm415xx_enable_port_charging,
    .is_port_charging_enabled= sgm415xx_is_port_charging_enabled,

   /* shipmode */
    .get_ship_mode = sgm415xx_get_shipmode,
    .set_ship_mode = sgm415xx_set_shipmode,
};

static int sgm415xx_driver_probe(struct i2c_client *client,
                const struct i2c_device_id *id)
{
    struct device *dev = &client->dev;
    struct sgm415xx_device *sgm;
    int ret = 0;

    char *name = NULL;

    pr_info("[%s] enter\n", __func__);

    sgm = devm_kzalloc(dev, sizeof(*sgm), GFP_KERNEL);
    if (!sgm) {
        return -ENOMEM;
    }

    sgm->client = client;
    sgm->dev = dev;

    mutex_init(&sgm->lock);
    mutex_init(&sgm->i2c_rw_lock);

    i2c_set_clientdata(client, sgm);

    ret = sgm415xx_hw_chipid_detect(sgm);
    if (ret != SGM415XX_PN_ID) {
        pr_err("[%s] device not found !!!\n", __func__);
        return ret;
    }

    /* set for chg_info */
    gxy_bat_set_chginfo(GXY_BAT_CHG_INFO_SGM41515D);

    ret = sgm415xx_parse_dt(sgm);
    if (ret) {
        return ret;
    }

    name = devm_kasprintf(sgm->dev, GFP_KERNEL, "%s","sgm415xx suspend wakelock");
    sgm->charger_wakelock =    wakeup_source_register(NULL, name);

    /* Register charger device */
    sgm->chg_dev = charger_device_register("primary_chg",
                        &client->dev, sgm,
                        &sgm415xx_chg_ops,
                        &sgm415xx_chg_props);
    if (IS_ERR_OR_NULL(sgm->chg_dev)) {
        pr_info("[%s] register charger device  failed\n", __func__);
        ret = PTR_ERR(sgm->chg_dev);
        return ret;
    }

    /* otg regulator */
    s_chg_dev_otg=sgm->chg_dev;

    INIT_DELAYED_WORK(&sgm->prob_dwork, sgm415xx_inform_prob_dwork_handler);
    INIT_DELAYED_WORK(&sgm->force_detect_dwork, sgm415xx_force_detection_dwork_handler);
    INIT_DELAYED_WORK(&sgm->charge_detect_delayed_work, charger_detect_work_func);
    INIT_DELAYED_WORK(&sgm->hvdcp_done_work, sgm415xx_hvdcp_done_work_func);
    sgm->hvdcp_done = false;

    ret = sgm415xx_power_supply_init(sgm, dev);
    if (ret) {
        pr_err("[%s] Failed to register power supply\n", __func__);
        return ret;
    }

    sgm->chg_config = false;

    ret = sgm415xx_hw_init(sgm);
    if (ret) {
        pr_err("[%s] Cannot initialize the chip.\n", __func__);
        return ret;
    }

    if (client->irq) {
        ret = devm_request_threaded_irq(dev, client->irq, NULL,
                        sgm415xx_irq_handler_thread,
                        IRQF_TRIGGER_FALLING |
                        IRQF_ONESHOT,
                        dev_name(&client->dev), sgm);
        if (ret) {
            pr_err("[%s] sgm irq fail.\n", __func__);
            return ret;
        }

        enable_irq_wake(client->irq);
        device_init_wakeup(sgm->dev, true);
    }

    ret = sgm415xx_vbus_regulator_register(sgm);

    pr_err("[%s] success\n", __func__);

    schedule_delayed_work(&sgm->prob_dwork, msecs_to_jiffies(1000));

    return ret;

}

static int sgm415xx_charger_remove(struct i2c_client *client)
{
    struct sgm415xx_device *sgm = i2c_get_clientdata(client);

    regulator_unregister(sgm->otg_rdev);

    power_supply_unregister(sgm->charger);

    mutex_destroy(&sgm->lock);
    mutex_destroy(&sgm->i2c_rw_lock);

    return 0;
}

static void sgm415xx_charger_shutdown(struct i2c_client *client)
{
    struct sgm415xx_device *sgm = i2c_get_clientdata(client);
    int ret = 0;

    ret = sgm415xx_disable_charger(sgm);
    if (ret) {
        pr_err("[%s] Failed to disable charger, ret = %d\n", __func__, ret);
    }

    pr_info("[%s] enter\n", __func__);
}

static const struct i2c_device_id sgm415xx_i2c_ids[] = {
    { "sgm41541", 0 },
    { "sgm41542", 1 },
    { "sgm41543", 2 },
    { "sgm41543D", 3 },
    { "sgm41513", 4 },
    { "sgm41513A", 5 },
    { "sgm41513D", 6 },
    { "sgm41516", 7 },
    { "sgm41516D", 8 },
    {},
};
MODULE_DEVICE_TABLE(i2c, sgm415xx_i2c_ids);

static const struct of_device_id sgm415xx_of_match[] = {
    { .compatible = "sgm,sgm41541", },
    { .compatible = "sgm,sgm41542", },
    { .compatible = "sgm,sgm41543", },
    { .compatible = "sgm,sgm41543D", },
    { .compatible = "sgm,sgm41513", },
    { .compatible = "sgm,sgm41513A", },
    { .compatible = "sgm,sgm41513D", },
    { .compatible = "sgm,sgm41516", },
    { .compatible = "sgm,sgm41516D", },
    { },
};
MODULE_DEVICE_TABLE(of, sgm415xx_of_match);

#ifdef CONFIG_PM_SLEEP
static int sgm415xx_suspend(struct device *dev)
{
    struct sgm415xx_device *sgm = dev_get_drvdata(dev);

    pr_info("[%s] enter\n", __func__);

    if (device_may_wakeup(dev)) {
        enable_irq_wake(sgm->client->irq);
    }

    disable_irq(sgm->client->irq);

    return 0;
}

static int sgm415xx_resume(struct device *dev)
{
    struct sgm415xx_device *sgm = dev_get_drvdata(dev);

    pr_info("[%s] enter\n", __func__);

    enable_irq(sgm->client->irq);
    if (device_may_wakeup(dev)) {
        disable_irq_wake(sgm->client->irq);
    }

    return 0;
}

static SIMPLE_DEV_PM_OPS(sgm415xx_pm_ops, sgm415xx_suspend, sgm415xx_resume);
#endif /*CONFIG_PM_SLEEP*/

static struct i2c_driver sgm415xx_driver = {
    .driver = {
        .name = "sgm415xx-charger",
        .of_match_table = sgm415xx_of_match,
        #ifdef CONFIG_PM_SLEEP
        .pm = &sgm415xx_pm_ops,
        #endif /*CONFIG_PM_SLEEP*/
    },
    .probe = sgm415xx_driver_probe,
    .remove = sgm415xx_charger_remove,
    .shutdown = sgm415xx_charger_shutdown,
    .id_table = sgm415xx_i2c_ids,
};
module_i2c_driver(sgm415xx_driver);

MODULE_AUTHOR(" qhq <Allen_qin@sg-micro.com> && Lucas-Gao");
MODULE_DESCRIPTION("sgm415xx charger driver");
MODULE_LICENSE("GPL v2");
