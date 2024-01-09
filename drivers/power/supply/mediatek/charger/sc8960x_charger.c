// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
*/
#define pr_fmt(fmt)    "[sc8960x]:%s: " fmt, __func__

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
#include "sc8960x_reg.h"
#include "sc8960x.h"

/* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
#include <mt-plat/upmu_common.h>
/* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 end */

/* hs14 code for SR-AL6528A-01-787 by gaozhengwei at 2022/12/06 start */
#define I2C_RETRY_CNT       3
/* hs14 code for SR-AL6528A-01-787 by gaozhengwei at 2022/12/06 end */

enum {
    PN_SC89601D,
};

enum sc8960x_part_no {
    SC89601D = 0x03,
};

static int pn_data[] = {
    [PN_SC89601D] = 0x03,
};

static char *pn_str[] = {
    [PN_SC89601D] = "sc89601d",
};

struct sc8960x {
    struct device *dev;
    struct i2c_client *client;

    enum sc8960x_part_no part_no;
    int revision;

    const char *chg_dev_name;
    const char *eint_name;

    bool chg_det_enable;

    int psy_usb_type;
    struct delayed_work psy_dwork;
    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
    struct delayed_work prob_dwork;
    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 end */

    int status;
    int irq;
    u32 intr_gpio;

    struct mutex i2c_rw_lock;

    bool charge_enabled;    /* Register bit status */
    bool power_good;
    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
    bool vbus_gd;
    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 end */
    /* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 start */
    bool unkown_detect;
    /* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 end */
    /* hs14 code for AL6528ADEU-28 by gaozhengwei at 2022/09/29 start */
    bool first_force;
    /* hs14 code for AL6528ADEU-28 by gaozhengwei at 2022/09/29 end */
    /* hs14 code for AL6528A-1023 by gaozhengwei at 2022/12/06 start */
    bool sdp_detect;
    /* hs14 code for AL6528A-1023 by gaozhengwei at 2022/12/06 end */
    /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 start */
    bool vbus_stat;
    /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 end */
    /* hs14 code for AL6528A-371 by qiaodan at 2022/10/25 start */
    int shipmode_wr_value;
    /* hs14 code for AL6528A-371 by qiaodan at 2022/10/25 end */
    /* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 start */
    bool dpdm_done;
    /* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 end */
    /* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 start */
    bool bypass_chgdet_en;
    /* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 end */

    struct sc8960x_platform_data *platform_data;
    struct charger_device *chg_dev;

    struct power_supply *psy;
};

static const struct charger_properties sc8960x_chg_props = {
    .alias_name = "sc8960x",
};

static int __sc8960x_read_reg(struct sc8960x *sc, u8 reg, u8 *data)
{
    s32 ret;
    /* hs14 code for SR-AL6528A-01-787 by gaozhengwei at 2022/12/06 start */
    int i;

    for (i = 0; i < I2C_RETRY_CNT; ++i) {

        ret = i2c_smbus_read_byte_data(sc->client, reg);

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

static int __sc8960x_write_reg(struct sc8960x *sc, int reg, u8 val)
{
    s32 ret;
    /* hs14 code for SR-AL6528A-01-787 by gaozhengwei at 2022/12/06 start */
    int i;

    for (i = 0; i < I2C_RETRY_CNT; ++i) {

        ret = i2c_smbus_write_byte_data(sc->client, reg, val);

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

static int sc8960x_read_byte(struct sc8960x *sc, u8 reg, u8 *data)
{
    int ret;

    mutex_lock(&sc->i2c_rw_lock);
    ret = __sc8960x_read_reg(sc, reg, data);
    mutex_unlock(&sc->i2c_rw_lock);

    return ret;
}

static int sc8960x_write_byte(struct sc8960x *sc, u8 reg, u8 data)
{
    int ret;

    mutex_lock(&sc->i2c_rw_lock);
    ret = __sc8960x_write_reg(sc, reg, data);
    mutex_unlock(&sc->i2c_rw_lock);

    if (ret)
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

    return ret;
}

static int sc8960x_update_bits(struct sc8960x *sc, u8 reg, u8 mask, u8 data)
{
    int ret;
    u8 tmp;

    mutex_lock(&sc->i2c_rw_lock);
    ret = __sc8960x_read_reg(sc, reg, &tmp);
    if (ret) {
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
        goto out;
    }

    tmp &= ~mask;
    tmp |= data & mask;

    ret = __sc8960x_write_reg(sc, reg, tmp);
    if (ret)
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
    mutex_unlock(&sc->i2c_rw_lock);
    return ret;
}

/* hs14 code for AL6528ADEU-28 by gaozhengwei at 2022/09/29 start */
static int sc8960x_set_wa_key(struct sc8960x *sc)
{
    sc8960x_write_byte(sc, SC8960X_REG_7F, REG7F_KEY1);
    sc8960x_write_byte(sc, SC8960X_REG_7F, REG7F_KEY2);
    sc8960x_write_byte(sc, SC8960X_REG_7F, REG7F_KEY3);
    sc8960x_write_byte(sc, SC8960X_REG_7F, REG7F_KEY4);
    sc8960x_write_byte(sc, SC8960X_REG_7F, REG7F_KEY5);
    sc8960x_write_byte(sc, SC8960X_REG_7F, REG7F_KEY6);
    sc8960x_write_byte(sc, SC8960X_REG_7F, REG7F_KEY7);
    return sc8960x_write_byte(sc, SC8960X_REG_7F, REG7F_KEY8);
}

static int sc8960x_set_wa(struct sc8960x *sc)
{
    int ret;
    u8 reg_val;

    ret = sc8960x_read_byte(sc, SC8960X_REG_99, &reg_val);
    if (ret) {
        sc8960x_set_wa_key(sc);
    }
    sc8960x_write_byte(sc, SC8960X_REG_92, REG92_PFM_VAL);
    ret = sc8960x_read_byte(sc, SC8960X_REG_B8, &reg_val);
    sc8960x_write_byte(sc, SC8960X_REG_94, REG94_VAL);
    sc8960x_write_byte(sc, SC8960X_REG_96, REG96_VAL(reg_val));
    sc8960x_write_byte(sc, SC8960X_REG_93, REG93_VAL(reg_val));
    sc8960x_write_byte(sc, SC8960X_REG_0E, REG0E_VAL(reg_val));
    sc8960x_write_byte(sc, SC8960X_REG_99, REG99_VAL(reg_val));

    return sc8960x_set_wa_key(sc);
}
/* hs14 code for AL6528ADEU-28 by gaozhengwei at 2022/09/29 end */

/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
static int sc8960x_set_key(struct sc8960x *sc)
{
    sc8960x_write_byte(sc, SC8960X_REG_7D, REG7D_KEY1);
    sc8960x_write_byte(sc, SC8960X_REG_7D, REG7D_KEY2);
    sc8960x_write_byte(sc, SC8960X_REG_7D, REG7D_KEY3);
    return sc8960x_write_byte(sc, SC8960X_REG_7D, REG7D_KEY4);
}

/* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 start */
static int sc8960x_enable_bc12(struct sc8960x *sc, bool en)
{
    int ret;
    u8 reg_val;

    ret = sc8960x_read_byte(sc, SC8960X_REG_99, &reg_val);
    if (ret) {
        sc8960x_set_wa_key(sc);
    }

    if (en) {
        sc8960x_write_byte(sc, SC8960X_REG_99, REG_99_BC12_enable);
    } else {
        sc8960x_write_byte(sc, SC8960X_REG_99, REG_99_BC12_disable);
    }

    return sc8960x_set_wa_key(sc);
}

static int sc8960x_set_dpdm_sink(struct sc8960x *sc)
{
    int ret;
    u8 reg_val;

    ret = sc8960x_read_byte(sc, SC8960X_REG_81, &reg_val);
    if (ret) {
        sc8960x_set_key(sc);
    }

    sc8960x_update_bits(sc, SC8960X_REG_80, REG80_DPDM_SINK_MASK, REG80_DPDM_SINK_HIGH);

    return sc8960x_set_key(sc);
}
/* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 end */

/* hs14 code for AL6528A-1090 by shanxinkai at 2023/02/10 start */
static int sc8960x_enable_hvdcp(struct sc8960x *sc)
{
    int ret;
    int dp_val, dm_val;
    u8 reg_val;

    ret = sc8960x_read_byte(sc, SC8960X_REG_81, &reg_val);
    if (ret) {
        sc8960x_set_key(sc);
    }

    /*dp and dm connected,dp 0.6V dm Hiz*/
    dp_val = REG81_DP_DRIVE_06V << REG81_DP_DRIVE_SHIFT;
    ret = sc8960x_update_bits(sc, SC8960X_REG_81,
                    REG81_DP_DRIVE_MASK, dp_val); //dp 0.6V

    dm_val = 0;
    ret = sc8960x_update_bits(sc, SC8960X_REG_81,
                    REG81_DM_DRIVE_MASK, dm_val); //dm Hiz

    ret = sc8960x_read_byte(sc, SC8960X_REG_81, &reg_val);
    pr_err("%s ----> reg 81 = 0x%02x\n", __func__, reg_val);

    return sc8960x_set_key(sc);
}
/* hs14 code for AL6528A-1090 by shanxinkai at 2023/02/10 end */
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */

static int sc8960x_enable_otg(struct sc8960x *sc)
{
    u8 val = REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT;
    return sc8960x_update_bits(sc, SC8960X_REG_01, REG01_OTG_CONFIG_MASK,
                val);

}

static int sc8960x_disable_otg(struct sc8960x *sc)
{
    u8 val = REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT;
    return sc8960x_update_bits(sc, SC8960X_REG_01, REG01_OTG_CONFIG_MASK,
                val);

}

static int sc8960x_enable_charger(struct sc8960x *sc)
{
    int ret;
    u8 val = REG01_CHG_ENABLE << REG01_CHG_CONFIG_SHIFT;

    ret =
        sc8960x_update_bits(sc, SC8960X_REG_01, REG01_CHG_CONFIG_MASK, val);

    return ret;
}

static int sc8960x_disable_charger(struct sc8960x *sc)
{
    int ret;
    u8 val = REG01_CHG_DISABLE << REG01_CHG_CONFIG_SHIFT;

    ret =
        sc8960x_update_bits(sc, SC8960X_REG_01, REG01_CHG_CONFIG_MASK, val);
    return ret;
}

int sc8960x_set_chargecurrent(struct sc8960x *sc, int curr)
{
    u8 ichg;

    if (curr < REG02_ICHG_BASE)
        curr = REG02_ICHG_BASE;

    ichg = (curr - REG02_ICHG_BASE) / REG02_ICHG_LSB;
    return sc8960x_update_bits(sc, SC8960X_REG_02, REG02_ICHG_MASK,
                ichg << REG02_ICHG_SHIFT);

}

int sc8960x_set_term_current(struct sc8960x *sc, int curr)
{
    u8 iterm;

    if (curr < REG03_ITERM_BASE)
        curr = REG03_ITERM_BASE;

    iterm = (curr - REG03_ITERM_BASE) / REG03_ITERM_LSB;

    return sc8960x_update_bits(sc, SC8960X_REG_03, REG03_ITERM_MASK,
                iterm << REG03_ITERM_SHIFT);
}
EXPORT_SYMBOL_GPL(sc8960x_set_term_current);

int sc8960x_set_prechg_current(struct sc8960x *sc, int curr)
{
    u8 iprechg;

    if (curr < REG03_IPRECHG_BASE)
        curr = REG03_IPRECHG_BASE;

    iprechg = (curr - REG03_IPRECHG_BASE) / REG03_IPRECHG_LSB;

    return sc8960x_update_bits(sc, SC8960X_REG_03, REG03_IPRECHG_MASK,
                iprechg << REG03_IPRECHG_SHIFT);
}
EXPORT_SYMBOL_GPL(sc8960x_set_prechg_current);

int sc8960x_set_chargevolt(struct sc8960x *sc, int volt)
{
    u8 val;
    u8 vreg_ft;
    int ret;

    if (volt < REG04_VREG_BASE)
        volt = REG04_VREG_BASE;

    if (((volt - 8 - REG04_VREG_BASE) % REG04_VREG_LSB) == 0) {
        volt -= 8;
        vreg_ft = REG0D_VREG_FT_INC8MV;
    }
    else if (((volt - 16 - REG04_VREG_BASE) % REG04_VREG_LSB) == 0) {
        volt -= 16;
        vreg_ft = REG0D_VREG_FT_INC16MV;
    }
    else if (((volt - 24 - REG04_VREG_BASE) % REG04_VREG_LSB) == 0) {
        volt -= 24;
        vreg_ft = REG0D_VREG_FT_INC24MV;
    }
    else
        vreg_ft = REG0D_VREG_FT_DEFAULT;

    val = (volt - REG04_VREG_BASE) / REG04_VREG_LSB;
    ret = sc8960x_update_bits(sc, SC8960X_REG_04, REG04_VREG_MASK,
                   val << REG04_VREG_SHIFT);
    if (ret) {
        dev_err(sc->dev, "%s: failed to set charger volt\n", __func__);
        return ret;
    }

    ret = sc8960x_update_bits(sc, SC8960X_REG_0D, REG0D_VBAT_REG_FT_MASK,
                    vreg_ft << REG0D_VBAT_REG_FT_SHIFT);
    if (ret) {
        dev_err(sc->dev, "%s: failed to set charger volt ft\n", __func__);
        return ret;
    }

    return 0;
}

int sc8960x_set_input_volt_limit(struct sc8960x *sc, int volt)
{
    u8 val;

    if (volt < REG06_VINDPM_BASE)
        volt = REG06_VINDPM_BASE;

    /* hs14 code for AL6528A-1090 by shanxinkai at 2023/02/10 start */
    // Set the gear below 5V to 4.6V, avoid entry sleep mode
    if (volt <= REG06_MIN_VINDPM_THRES) {
        val = REG06_MIN_VINDPM_VAL;
        return sc8960x_update_bits(sc, SC8960X_REG_06, REG06_VINDPM_MASK,
                    val << REG06_VINDPM_SHIFT);
    } else if ((volt >= REG06_VINDPM_HV_LOW) && (volt < REG06_VINDPM_HV_TOP)) {
    /* hs14 code for AL6528A-1090 by shanxinkai at 2023/02/10 end */
    /* hs14 code for AL6528A-1072 by zhangzhihao at 2023/1/18 start */
        val = REG06_VINDPM_HV_VAL;
        return sc8960x_update_bits(sc, SC8960X_REG_06, REG06_VINDPM_MASK,
                    val << REG06_VINDPM_SHIFT);
    }
    /* hs14 code for AL6528A-1072 by zhangzhihao at 2023/1/18 end */

    val = (volt - REG06_VINDPM_BASE) / REG06_VINDPM_LSB;
    return sc8960x_update_bits(sc, SC8960X_REG_06, REG06_VINDPM_MASK,
                val << REG06_VINDPM_SHIFT);
}

int sc8960x_set_input_current_limit(struct sc8960x *sc, int curr)
{
    u8 val;

    if (curr < REG00_IINLIM_BASE)
        curr = REG00_IINLIM_BASE;

    val = (curr - REG00_IINLIM_BASE) / REG00_IINLIM_LSB;
    return sc8960x_update_bits(sc, SC8960X_REG_00, REG00_IINLIM_MASK,
                val << REG00_IINLIM_SHIFT);
}

int sc8960x_set_watchdog_timer(struct sc8960x *sc, u8 timeout)
{
    u8 temp;

    temp = (u8) (((timeout -
            REG05_WDT_BASE) / REG05_WDT_LSB) << REG05_WDT_SHIFT);

    return sc8960x_update_bits(sc, SC8960X_REG_05, REG05_WDT_MASK, temp);
}
EXPORT_SYMBOL_GPL(sc8960x_set_watchdog_timer);

int sc8960x_disable_watchdog_timer(struct sc8960x *sc)
{
    u8 val = REG05_WDT_DISABLE << REG05_WDT_SHIFT;

    return sc8960x_update_bits(sc, SC8960X_REG_05, REG05_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(sc8960x_disable_watchdog_timer);

int sc8960x_reset_watchdog_timer(struct sc8960x *sc)
{
    u8 val = REG01_WDT_RESET << REG01_WDT_RESET_SHIFT;

    return sc8960x_update_bits(sc, SC8960X_REG_01, REG01_WDT_RESET_MASK,
                val);
}
EXPORT_SYMBOL_GPL(sc8960x_reset_watchdog_timer);

int sc8960x_reset_chip(struct sc8960x *sc)
{
    int ret;
    u8 val = REG0B_REG_RESET << REG0B_REG_RESET_SHIFT;

    ret =
        sc8960x_update_bits(sc, SC8960X_REG_0B, REG0B_REG_RESET_MASK, val);
    return ret;
}
EXPORT_SYMBOL_GPL(sc8960x_reset_chip);

int sc8960x_enter_hiz_mode(struct sc8960x *sc)
{
    u8 val = REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT;

    return sc8960x_update_bits(sc, SC8960X_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(sc8960x_enter_hiz_mode);

int sc8960x_exit_hiz_mode(struct sc8960x *sc)
{

    u8 val = REG00_HIZ_DISABLE << REG00_ENHIZ_SHIFT;

    return sc8960x_update_bits(sc, SC8960X_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(sc8960x_exit_hiz_mode);

/* hs14 code for SR-AL6528A-01-299 by gaozhengwei at 2022/09/02 start */
static int sc8960x_set_hiz_mode(struct charger_device *chg_dev, bool en)
{
    int ret;
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

    if (en)
        ret = sc8960x_enter_hiz_mode(sc);
    else
        ret = sc8960x_exit_hiz_mode(sc);

    pr_err("%s hiz mode %s\n", en ? "enable" : "disable",
            !ret ? "successfully" : "failed");

    return ret;
}

static int sc8960x_get_hiz_mode(struct charger_device *chg_dev)
{
    int ret;
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
    u8 val;
    ret = sc8960x_read_byte(sc, SC8960X_REG_00, &val);
    if (ret == 0){
        pr_err("Reg[%.2x] = 0x%.2x\n", SC8960X_REG_00, val);
    }

    ret = (val & REG00_ENHIZ_MASK) >> REG00_ENHIZ_SHIFT;
    pr_err("%s:hiz mode %s\n",__func__, ret ? "enabled" : "disabled");

    return ret;
}
/* hs14 code for SR-AL6528A-01-299 by gaozhengwei at 2022/09/02 end */

/* hs14 code for SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 start*/
/* hs14 code for AL6528A-371 by qiaodan at 2022/10/25 start */
static int sc8960x_set_batfet_delay(struct sc8960x *sc, uint8_t delay);
static int sc8960x_set_shipmode(struct charger_device *chg_dev, bool en)
{
    int ret;
    u8 val, i;
    u8 shipmode_trycnt = 3;
    u8 shipmode_delay = 10;
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

    sc->shipmode_wr_value = 0;

    if (en) {
        sc8960x_set_batfet_delay(sc, shipmode_delay);

        val = REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT;
        ret = sc8960x_update_bits(sc, SC8960X_REG_07, REG07_BATFET_DIS_MASK, val);
    } else {
        val = REG07_BATFET_ON << REG07_BATFET_DIS_SHIFT;

        /*
          * The sc89601 charging chip [REG7_bit 5] returns a non control value.
          * Importing the WR scheme to write three times and
          * return the result of communication with i2c instead of the read value
          * to reduces the probability of write failure
        */
        for(i = 0; i < shipmode_trycnt; i++) {
                ret = sc8960x_update_bits(sc, SC8960X_REG_07, REG07_BATFET_DIS_MASK, val);
                msleep(10);
        }
    }

    if (!ret) {
        sc->shipmode_wr_value = 1;
    }

    pr_err("%s shipmode %s\n", en ? "enable" : "disable",
            !ret ? "successfully" : "failed");

    return ret;
}

static int sc8960x_get_shipmode(struct charger_device *chg_dev)
{
    int ret;
    // u8 val;
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

    /*
      * The sc89601 charging chip [REG7_bit 5] returns a non control value.
      * Importing the WR scheme to write three times and
      * return the result of communication with i2c instead of the read value
      * to reduces the probability of write failure
    */
    /*
    msleep(100);
    ret = sc8960x_read_byte(sc, SC8960X_REG_07, &val);
    if (ret == 0){
        pr_err("Reg[%.2x] = 0x%.2x\n", SC8960X_REG_07, val);
    } else {
        pr_err("%s: get shipmode reg fail! \n",__func__);
        return ret;
    }

    ret = (val & REG07_BATFET_DIS_MASK) >> REG07_BATFET_DIS_SHIFT;
    */
    ret = sc->shipmode_wr_value;

    pr_err("%s:shipmode_wr %s\n",__func__, ret ? "enabled" : "disabled");

    return ret;
}
/* hs14 code for AL6528A-371 by qiaodan at 2022/10/25 end */
static int sc8960x_disable_battfet_rst(struct sc8960x *sc)
{
    int ret;
    u8 val;

    val = REG07_BATFET_RST_DISABLE << REG07_BATFET_RST_EN_SHIFT;
    ret = sc8960x_update_bits(sc, SC8960X_REG_07, REG07_BATFET_RST_EN_MASK, val);

    pr_err("disable BATTFET_RST_EN %s\n", !ret ? "successfully" : "failed");

    return ret;
}
/* hs14 code for SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 end*/

/* hs14 code for  SR-AL6528A-01-339 by shanxinkai at 2022/09/30 start*/
static int sc8960x_get_charging_status(struct charger_device *chg_dev,
    int *chg_stat)
{
    int ret;
    u8 val;
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

    ret = sc8960x_read_byte(sc, SC8960X_REG_08, &val);
    if (!ret) {
        val = val & REG08_CHRG_STAT_MASK;
        val = val >> REG08_CHRG_STAT_SHIFT;
        *chg_stat = val;
    }

    return ret;
}
/* hs14 code for  SR-AL6528A-01-339 by shanxinkai at 2022/09/30 end*/

static int sc8960x_enable_term(struct sc8960x *sc, bool enable)
{
    u8 val;
    int ret;

    if (enable)
        val = REG05_TERM_ENABLE << REG05_EN_TERM_SHIFT;
    else
        val = REG05_TERM_DISABLE << REG05_EN_TERM_SHIFT;

    ret = sc8960x_update_bits(sc, SC8960X_REG_05, REG05_EN_TERM_MASK, val);

    return ret;
}
EXPORT_SYMBOL_GPL(sc8960x_enable_term);

/* hs14 code for AL6528A-341 by wenyaqi at 2022/10/20 start */
int sc8960x_set_boost_current(struct sc8960x *sc, int curr)
{
    u8 val;

    if (curr >= BOOSTI_1200)
        val = REG02_BOOST_LIM_1P2A;
    else
        val = REG02_BOOST_LIM_0P5A;

    return sc8960x_update_bits(sc, SC8960X_REG_02, REG02_BOOST_LIM_MASK,
                val << REG02_BOOST_LIM_SHIFT);
}

int sc8960x_set_boost_voltage(struct sc8960x *sc, int volt)
{
    u8 val;

    if (volt >= BOOSTV_5300)
        val = REG06_BOOSTV_5P3V;
    else if (volt >= BOOSTV_4900 && volt < BOOSTV_5100)
        val = REG06_BOOSTV_4P9V;
    else if (volt >= BOOSTV_4700 && volt < BOOSTV_4900)
        val = REG06_BOOSTV_4P7V;
    else
        val = REG06_BOOSTV_5P1V;

    return sc8960x_update_bits(sc, SC8960X_REG_06, REG06_BOOSTV_MASK,
                val << REG06_BOOSTV_SHIFT);
}
EXPORT_SYMBOL_GPL(sc8960x_set_boost_voltage);

static int sc8960x_set_acovp_threshold(struct sc8960x *sc, int volt)
{
    u8 val;

    if (volt >= VAC_OVP_14200)
        val = REG06_OVP_14P2V;
    else if (volt >= VAC_OVP_11000 && volt < VAC_OVP_14200)
        val = REG06_OVP_11V;
    else if (volt >= VAC_OVP_6400 && volt < VAC_OVP_11000)
        val = REG06_OVP_6P4V;
    else
        val = REG06_OVP_5P8V;

    return sc8960x_update_bits(sc, SC8960X_REG_06, REG06_OVP_MASK,
                val << REG06_OVP_SHIFT);
}
EXPORT_SYMBOL_GPL(sc8960x_set_acovp_threshold);
/* hs14 code for AL6528A-341 by wenyaqi at 2022/10/20 end */

static int sc8960x_set_stat_ctrl(struct sc8960x *sc, int ctrl)
{
    u8 val;

    val = ctrl;

    return sc8960x_update_bits(sc, SC8960X_REG_00, REG00_STAT_CTRL_MASK,
                val << REG00_STAT_CTRL_SHIFT);
}

static int sc8960x_set_int_mask(struct sc8960x *sc, int mask)
{
    u8 val;

    val = mask;

    return sc8960x_update_bits(sc, SC8960X_REG_0A, REG0A_INT_MASK_MASK,
                val << REG0A_INT_MASK_SHIFT);
}

/* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
/* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 start */
static int sc8960x_force_dpdm(struct sc8960x *sc)
{
    int ret;
    u8 reg_val;
    const u8 val = REG07_FORCE_DPDM << REG07_FORCE_DPDM_SHIFT;

    ret = sc8960x_read_byte(sc, SC8960X_REG_81, &reg_val);
    if (ret) {
        sc8960x_set_key(sc);
    }

    sc8960x_update_bits(sc, SC8960X_REG_81, REG81_DP_DRIVE_MASK,
            1 << REG81_DP_DRIVE_SHIFT);

    sc8960x_update_bits(sc, SC8960X_REG_81, REG81_DM_DRIVE_MASK,
            1 << REG81_DM_DRIVE_SHIFT);

    sc8960x_set_key(sc);
    msleep(100);

    pr_err("%s force dpdm\n");

    return sc8960x_update_bits(sc, SC8960X_REG_07, REG07_FORCE_DPDM_MASK,
                val);
}
/* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 end */
/* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 end */

static int sc8960x_enable_batfet(struct sc8960x *sc)
{
    const u8 val = REG07_BATFET_ON << REG07_BATFET_DIS_SHIFT;

    return sc8960x_update_bits(sc, SC8960X_REG_07, REG07_BATFET_DIS_MASK,
                val);
}
EXPORT_SYMBOL_GPL(sc8960x_enable_batfet);

static int sc8960x_disable_batfet(struct sc8960x *sc)
{
    const u8 val = REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT;

    return sc8960x_update_bits(sc, SC8960X_REG_07, REG07_BATFET_DIS_MASK,
                val);
}
EXPORT_SYMBOL_GPL(sc8960x_disable_batfet);

static int sc8960x_set_batfet_delay(struct sc8960x *sc, uint8_t delay)
{
    u8 val;

    if (delay == 0)
        val = REG07_BATFET_DLY_0S;
    else
        val = REG07_BATFET_DLY_10S;

    val <<= REG07_BATFET_DLY_SHIFT;

    return sc8960x_update_bits(sc, SC8960X_REG_07, REG07_BATFET_DLY_MASK,
                val);
}
EXPORT_SYMBOL_GPL(sc8960x_set_batfet_delay);

static int sc8960x_enable_safety_timer(struct sc8960x *sc)
{
    const u8 val = REG05_CHG_TIMER_ENABLE << REG05_EN_TIMER_SHIFT;

    return sc8960x_update_bits(sc, SC8960X_REG_05, REG05_EN_TIMER_MASK,
                val);
}
EXPORT_SYMBOL_GPL(sc8960x_enable_safety_timer);

static int sc8960x_disable_safety_timer(struct sc8960x *sc)
{
    const u8 val = REG05_CHG_TIMER_DISABLE << REG05_EN_TIMER_SHIFT;

    return sc8960x_update_bits(sc, SC8960X_REG_05, REG05_EN_TIMER_MASK,
                val);
}
EXPORT_SYMBOL_GPL(sc8960x_disable_safety_timer);

static struct sc8960x_platform_data *sc8960x_parse_dt(struct device_node *np,
                            struct sc8960x *sc)
{
    int ret;
    struct sc8960x_platform_data *pdata;

    pdata = devm_kzalloc(&sc->client->dev, sizeof(struct sc8960x_platform_data),
                GFP_KERNEL);
    if (!pdata)
        return NULL;

    if (of_property_read_string(np, "charger_name", &sc->chg_dev_name) < 0) {
        sc->chg_dev_name = "primary_chg";
        pr_warn("no charger name\n");
    }

    if (of_property_read_string(np, "eint_name", &sc->eint_name) < 0) {
        sc->eint_name = "chr_stat";
        pr_warn("no eint name\n");
    }

    ret = of_get_named_gpio(np, "sc,intr_gpio", 0);
    if (ret < 0) {
        pr_err("%s no sc,intr_gpio(%d)\n",
                      __func__, ret);
    } else
        sc->intr_gpio = ret;

    pr_info("%s intr_gpio = %u\n",
                __func__, sc->intr_gpio);

    sc->chg_det_enable =
        of_property_read_bool(np, "sc,sc8960x,charge-detect-enable");

    ret = of_property_read_u32(np, "sc,sc8960x,usb-vlim", &pdata->usb.vlim);
    if (ret) {
        pdata->usb.vlim = 4500;
        pr_err("Failed to read node of sc,sc8960x,usb-vlim\n");
    }

    ret = of_property_read_u32(np, "sc,sc8960x,usb-ilim", &pdata->usb.ilim);
    if (ret) {
        pdata->usb.ilim = 2000;
        pr_err("Failed to read node of sc,sc8960x,usb-ilim\n");
    }

    ret = of_property_read_u32(np, "sc,sc8960x,usb-vreg", &pdata->usb.vreg);
    if (ret) {
        pdata->usb.vreg = 4200;
        pr_err("Failed to read node of sc,sc8960x,usb-vreg\n");
    }

    ret = of_property_read_u32(np, "sc,sc8960x,usb-ichg", &pdata->usb.ichg);
    if (ret) {
        pdata->usb.ichg = 2000;
        pr_err("Failed to read node of sc,sc8960x,usb-ichg\n");
    }

    ret = of_property_read_u32(np, "sc,sc8960x,stat-pin-ctrl",
                &pdata->statctrl);
    if (ret) {
        pdata->statctrl = 0;
        pr_err("Failed to read node of sc,sc8960x,stat-pin-ctrl\n");
    }

    ret = of_property_read_u32(np, "sc,sc8960x,precharge-current",
                &pdata->iprechg);
    if (ret) {
        pdata->iprechg = 180;
        pr_err("Failed to read node of sc,sc8960x,precharge-current\n");
    }

    ret = of_property_read_u32(np, "sc,sc8960x,termination-current",
                &pdata->iterm);
    if (ret) {
        pdata->iterm = 180;
        pr_err
            ("Failed to read node of sc,sc8960x,termination-current\n");
    }

    ret =
        of_property_read_u32(np, "sc,sc8960x,boost-voltage",
                &pdata->boostv);
    if (ret) {
        pdata->boostv = 5100;
        pr_err("Failed to read node of sc,sc8960x,boost-voltage\n");
    }

    ret =
        of_property_read_u32(np, "sc,sc8960x,boost-current",
                &pdata->boosti);
    if (ret) {
        pdata->boosti = 1200;
        pr_err("Failed to read node of sc,sc8960x,boost-current\n");
    }

    ret = of_property_read_u32(np, "sc,sc8960x,vac-ovp-threshold",
                &pdata->vac_ovp);
    if (ret) {
        pdata->vac_ovp = 6400;
        pr_err("Failed to read node of sc,sc8960x,vac-ovp-threshold\n");
    }

    return pdata;
}

static int sc8960x_get_charger_type(struct sc8960x *sc, int *type)
{
    int ret;

    u8 reg_val = 0;
    int vbus_stat = 0;
    int chg_type = CHARGER_UNKNOWN;

    ret = sc8960x_read_byte(sc, SC8960X_REG_08, &reg_val);

    if (ret)
        return ret;

    vbus_stat = (reg_val & REG08_VBUS_STAT_MASK);
    vbus_stat >>= REG08_VBUS_STAT_SHIFT;

    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
    pr_info("[%s] reg08: 0x%02x :vbus state %d\n", __func__, reg_val, vbus_stat);
    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 end */

    switch (vbus_stat) {

    case REG08_VBUS_TYPE_NONE:
        chg_type = CHARGER_UNKNOWN;
        break;
    case REG08_VBUS_TYPE_SDP:
        /* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 start */
        /* hs14 code for AL6528A-1023 by gaozhengwei at 2022/12/06 start */
        if (sc->sdp_detect) {
            sc->power_good = false;
            sc8960x_enable_bc12(sc, false);
            Charger_Detect_Init();
            sc8960x_force_dpdm(sc);
            sc->sdp_detect = false;
            return 0;
        }
        /* hs14 code for AL6528A-1023 by gaozhengwei at 2022/12/06 end */
        /* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 end */
        chg_type = STANDARD_HOST;
        break;
    case REG08_VBUS_TYPE_CDP:
        chg_type = CHARGING_HOST;
        break;
    case REG08_VBUS_TYPE_DCP:
    /* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
    case REG08_VBUS_TYPE_HVDCP:
    /* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */
        /* hs14 code for AL6528ADEU-28 by gaozhengwei at 2022/09/29 start */
        if (sc->first_force) {
            /* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 start */
            Charger_Detect_Init();
            /* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 end */
            sc->power_good = false;
            sc8960x_force_dpdm(sc);
            sc->first_force = false;
            /* hs14 code for AL6528A-1090 by shanxinkai at 2023/02/10 start */
            return 0;
            /* hs14 code for AL6528A-1090 by shanxinkai at 2023/02/10 end */
        }
        /* hs14 code for AL6528ADEU-28 by gaozhengwei at 2022/09/29 end */
        chg_type = STANDARD_CHARGER;
        break;
    case REG08_VBUS_TYPE_UNKNOWN:
    case REG08_VBUS_TYPE_NON_STD:
        /* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 start */
        if (sc->unkown_detect) {
            sc->power_good = false;
            sc8960x_enable_bc12(sc, true);
            Charger_Detect_Init();
            msleep(100);
            sc8960x_force_dpdm(sc);
            sc->unkown_detect = false;
            return 0;
        }
        /* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 end */
        chg_type = NONSTANDARD_CHARGER;
        break;
    default:
        chg_type = NONSTANDARD_CHARGER;
        break;
    }

    /* hs14 code for AL6528A-1090 by shanxinkai at 2023/02/10 start */
    if (chg_type != STANDARD_CHARGER) {
        Charger_Detect_Release();
    } else {
        ret = sc8960x_enable_hvdcp(sc);
        if (ret) {
            pr_err("Failed to en HVDCP, ret = %d\n", ret);
        }
    }
    /* hs14 code for AL6528A-1090 by shanxinkai at 2023/02/10 end */
    *type = chg_type;

    return 0;
}

static void sc8960x_inform_psy_dwork_handler(struct work_struct *work)
{
    int ret = 0;
    union power_supply_propval propval;
    struct sc8960x *sc = container_of(work, struct sc8960x,
                                psy_dwork.work);

    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
    if (!sc->psy) {
        sc->psy = power_supply_get_by_name("charger");
        if (!sc->psy) {
            pr_err("%s Couldn't get psy\n", __func__);
            mod_delayed_work(system_wq, &sc->psy_dwork,
                    msecs_to_jiffies(2000));
            return;
        }
    }

    if (sc->psy_usb_type != CHARGER_UNKNOWN)
        propval.intval = 1;
    else
        propval.intval = 0;

    ret = power_supply_set_property(sc->psy, POWER_SUPPLY_PROP_ONLINE,
                    &propval);

    if (ret < 0)
        pr_notice("inform power supply online failed:%d\n", ret);

    propval.intval = sc->psy_usb_type;
    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 end */

    ret = power_supply_set_property(sc->psy,
                    POWER_SUPPLY_PROP_CHARGE_TYPE,
                    &propval);

    if (ret < 0)
        pr_notice("inform power supply charge type failed:%d\n", ret);

    return;
}

/* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
static irqreturn_t sc8960x_irq_handler(int irq, void *data)
{
    int ret;
    u8 reg_val;
    bool prev_pg;
    bool prev_vbus_gd;

    struct sc8960x *sc = (struct sc8960x *)data;

    /* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 start */
    if (sc->bypass_chgdet_en == true) {
        pr_err("%s:bypass_chgdet_en=%d, skip bc12\n", __func__, sc->bypass_chgdet_en);
        return IRQ_HANDLED;
    }
    /* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 end */

    ret = sc8960x_read_byte(sc, SC8960X_REG_0A, &reg_val);
    if (ret)
        return IRQ_HANDLED;

    prev_vbus_gd = sc->vbus_gd;

    sc->vbus_gd = !!(reg_val & REG0A_VBUS_GD_MASK);

    ret = sc8960x_read_byte(sc, SC8960X_REG_08, &reg_val);
    if (ret)
        return IRQ_HANDLED;

    prev_pg = sc->power_good;

    sc->power_good = !!(reg_val & REG08_PG_STAT_MASK);

    /* hs14 code for AL6528ADEU-28 by gaozhengwei at 2022/09/29 start */
    if (!prev_vbus_gd && sc->vbus_gd) {
        /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 start */
        sc->vbus_stat = true;
        /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 end */
        /* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 start */
        pr_notice("adapter/usb inserted\n");
        Charger_Detect_Init();
        sc->first_force = true;
        sc->unkown_detect = true;
        /* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 end */
        /* hs14 code for AL6528A-1023 by gaozhengwei at 2022/12/06 start */
        sc->sdp_detect = true;
        /* hs14 code for AL6528A-1023 by gaozhengwei at 2022/12/06 end */
    }
    else if (prev_vbus_gd && !sc->vbus_gd) {
        /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 start */
        sc->vbus_stat = false;
        /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 end */
        sc->psy_usb_type = CHARGER_UNKNOWN;
        schedule_delayed_work(&sc->psy_dwork, 0);
        pr_notice("adapter/usb removed\n");
        /* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 start */
        sc8960x_enable_bc12(sc, true);
        Charger_Detect_Release();
        /* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 end */
        return IRQ_HANDLED;
    }

    if (!prev_pg && sc->power_good) {
        pr_err("BC1.2 done\n");
        ret = sc8960x_get_charger_type(sc, &sc->psy_usb_type);
        schedule_delayed_work(&sc->psy_dwork, 0);
    }
    /* hs14 code for AL6528ADEU-28 by gaozhengwei at 2022/09/29 end */

    return IRQ_HANDLED;
}
/* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 end */

static int sc8960x_register_interrupt(struct sc8960x *sc)
{
    int ret = 0;

    ret = devm_gpio_request_one(sc->dev, sc->intr_gpio, GPIOF_DIR_IN,
            devm_kasprintf(sc->dev, GFP_KERNEL,
            "sc8960x_intr_gpio.%s", dev_name(sc->dev)));
    if (ret < 0) {
        pr_err("%s gpio request fail(%d)\n",
                      __func__, ret);
        return ret;
    }
    sc->client->irq = gpio_to_irq(sc->intr_gpio);
    if (sc->client->irq < 0) {
        pr_err("%s gpio2irq fail(%d)\n",
                      __func__, sc->client->irq);
        return sc->client->irq;
    }
    pr_info("%s irq = %d\n", __func__, sc->client->irq);

    ret = devm_request_threaded_irq(sc->dev, sc->client->irq, NULL,
                    sc8960x_irq_handler,
                    IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND,
                    "sc_irq", sc);
    if (ret < 0) {
        pr_err("request thread irq failed:%d\n", ret);
        return ret;
    }

    enable_irq_wake(sc->irq);

    return 0;
}

static int sc8960x_init_device(struct sc8960x *sc)
{
    int ret;

    /* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 start */
    sc8960x_reset_chip(sc);
    /* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 end */

    sc8960x_disable_watchdog_timer(sc);

    ret = sc8960x_set_stat_ctrl(sc, sc->platform_data->statctrl);
    if (ret)
        pr_err("Failed to set stat pin control mode, ret = %d\n", ret);

    ret = sc8960x_set_prechg_current(sc, sc->platform_data->iprechg);
    if (ret)
        pr_err("Failed to set prechg current, ret = %d\n", ret);

    ret = sc8960x_set_term_current(sc, sc->platform_data->iterm);
    if (ret)
        pr_err("Failed to set termination current, ret = %d\n", ret);

    ret = sc8960x_set_boost_voltage(sc, sc->platform_data->boostv);
    if (ret)
        pr_err("Failed to set boost voltage, ret = %d\n", ret);

    ret = sc8960x_set_boost_current(sc, sc->platform_data->boosti);
    if (ret)
        pr_err("Failed to set boost current, ret = %d\n", ret);

    ret = sc8960x_set_acovp_threshold(sc, sc->platform_data->vac_ovp);
    if (ret)
        pr_err("Failed to set acovp threshold, ret = %d\n", ret);

    /* hs14 code for AL6528ADEU-1863 by wenyaqi at 2022/11/03 start */
    ret= sc8960x_disable_safety_timer(sc);
    if (ret)
        pr_err("Failed to set safety_timer stop, ret = %d\n", ret);
    /* hs14 code for AL6528ADEU-1863 by wenyaqi at 2022/11/03 end */

    ret = sc8960x_set_int_mask(sc,
                REG0A_IINDPM_INT_MASK |
                REG0A_VINDPM_INT_MASK);
    if (ret)
        pr_err("Failed to set vindpm and iindpm int mask\n");
    /* hs14 code for SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 start*/
    ret = sc8960x_disable_battfet_rst(sc);
    if (ret)
        pr_err("Failed to disable_battfet\n");
    /* hs14 code for SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 end*/

    /* hs14 code for SR-AL6528A-01-321|AL6528ADEU-28 by gaozhengwei at 2022/09/29 start */

    ret = sc8960x_set_wa(sc);
    if (ret)
        pr_err("Failed to set private\n");
    /* hs14 code for SR-AL6528A-01-321|AL6528ADEU-28 by gaozhengwei at 2022/09/29 end */

    /* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 start */
    ret = sc8960x_set_dpdm_sink(sc);
    if (ret)
        pr_err("Failed to set dpdm sink\n");

    ret = sc8960x_enable_bc12(sc, true);
    if (ret)
        pr_err("Failed to open bc12\n");
    /* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 end */

    return 0;
}

/* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
static void sc8960x_inform_prob_dwork_handler(struct work_struct *work)
{
    struct sc8960x *sc = container_of(work, struct sc8960x,
                                prob_dwork.work);

    sc8960x_force_dpdm(sc);

    sc8960x_irq_handler(sc->irq, (void *) sc);
}
/* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 end */

static int sc8960x_detect_device(struct sc8960x *sc)
{
    int ret;
    u8 data;

    ret = sc8960x_read_byte(sc, SC8960X_REG_0B, &data);
    if (!ret) {
        sc->part_no = (data & REG0B_PN_MASK) >> REG0B_PN_SHIFT;
        sc->revision =
            (data & REG0B_DEV_REV_MASK) >> REG0B_DEV_REV_SHIFT;
    }

    return ret;
}

static void sc8960x_dump_regs(struct sc8960x *sc)
{
    int addr;
    u8 val;
    int ret;

    for (addr = 0x0; addr <= 0x0B; addr++) {
        ret = sc8960x_read_byte(sc, addr, &val);
        if (ret == 0)
            pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
    }
}

static ssize_t
sc8960x_show_registers(struct device *dev, struct device_attribute *attr,
            char *buf)
{
    struct sc8960x *sc = dev_get_drvdata(dev);
    u8 addr;
    u8 val;
    u8 tmpbuf[200];
    int len;
    int idx = 0;
    int ret;

    idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc8960x Reg");
    for (addr = 0x0; addr <= 0x0B; addr++) {
        ret = sc8960x_read_byte(sc, addr, &val);
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
sc8960x_store_registers(struct device *dev,
            struct device_attribute *attr, const char *buf,
            size_t count)
{
    struct sc8960x *sc = dev_get_drvdata(dev);
    int ret;
    unsigned int reg;
    unsigned int val;

    ret = sscanf(buf, "%x %x", &reg, &val);
    if (ret == 2 && reg < 0x0B) {
        sc8960x_write_byte(sc, (unsigned char) reg,
                (unsigned char) val);
    }

    return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, sc8960x_show_registers,
        sc8960x_store_registers);

static struct attribute *sc8960x_attributes[] = {
    &dev_attr_registers.attr,
    NULL,
};

static const struct attribute_group sc8960x_attr_group = {
    .attrs = sc8960x_attributes,
};

static int sc8960x_charging(struct charger_device *chg_dev, bool enable)
{

    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
    int ret = 0;
    u8 val;
    /* hs14 code for AL6528A-389 by wenyaqi at 2022/11/14 start */
    bool otg_en = false;

    ret = sc8960x_read_byte(sc, SC8960X_REG_01, &val);

    if (!ret)
        otg_en = !!(val & REG01_OTG_CONFIG_MASK);
    if (otg_en == true && enable == true) {
        pr_err("%s:otg_en=%d,skip chg_en\n", __func__, otg_en);
        return -ENODEV;
    }
    /* hs14 code for AL6528A-389 by wenyaqi at 2022/11/14 end */

    if (enable)
        ret = sc8960x_enable_charger(sc);
    else
        ret = sc8960x_disable_charger(sc);

    pr_err("%s charger %s\n", enable ? "enable" : "disable",
        !ret ? "successfully" : "failed");

    ret = sc8960x_read_byte(sc, SC8960X_REG_01, &val);

    if (!ret)
        sc->charge_enabled = !!(val & REG01_CHG_CONFIG_MASK);

    return ret;
}

static int sc8960x_plug_in(struct charger_device *chg_dev)
{

    int ret;

    ret = sc8960x_charging(chg_dev, true);

    if (ret)
        pr_err("Failed to enable charging:%d\n", ret);

    return ret;
}

static int sc8960x_plug_out(struct charger_device *chg_dev)
{
    int ret;

    ret = sc8960x_charging(chg_dev, false);

    if (ret)
        pr_err("Failed to disable charging:%d\n", ret);

    return ret;
}

/* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 start */
static int sc8960x_bypass_chgdet(struct charger_device *chg_dev, bool bypass_chgdet_en)
{
    int ret = 0;
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

    sc->bypass_chgdet_en = bypass_chgdet_en;
    dev_info(sc->dev, "%s bypass_chgdet_en = %d\n", __func__, bypass_chgdet_en);

    return ret;
}
/* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 end */

/* hs14 code for SR-AL6528A-01-255|P221117-03133 by wenyaqi at 2022/11/24 start */
static int sc8960x_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
    int ret = 0;
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
    union power_supply_propval propval;

    dev_info(sc->dev, "%s en = %d\n", __func__, en);

    if (!sc->psy) {
        sc->psy = power_supply_get_by_name("charger");
        if (!sc->psy) {
            pr_err("%s Couldn't get psy\n", __func__);
            return -ENODEV;
        }
    }

    if (en == false) {
        propval.intval = 0;
    } else {
        propval.intval = 1;
    }

    ret = power_supply_set_property(sc->psy,
                    POWER_SUPPLY_PROP_ONLINE,
                    &propval);
    if (ret < 0)
        pr_notice("inform power supply online failed:%d\n", ret);

    if (en == false) {
        propval.intval = CHARGER_UNKNOWN;
    } else {
        propval.intval = sc->psy_usb_type;
    }

    ret = power_supply_set_property(sc->psy,
                    POWER_SUPPLY_PROP_CHARGE_TYPE,
                    &propval);
    if (ret < 0)
        pr_notice("inform power supply charge type failed:%d\n", ret);

    return ret;
}
/* hs14 code for SR-AL6528A-01-255|P221117-03133 by wenyaqi at 2022/11/24 end */

static int sc8960x_dump_register(struct charger_device *chg_dev)
{
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

    sc8960x_dump_regs(sc);

    return 0;
}

static int sc8960x_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

    *en = sc->charge_enabled;

    return 0;
}

static int sc8960x_is_charging_done(struct charger_device *chg_dev, bool *done)
{
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
    int ret;
    u8 val;

    ret = sc8960x_read_byte(sc, SC8960X_REG_08, &val);
    if (!ret) {
        val = val & REG08_CHRG_STAT_MASK;
        val = val >> REG08_CHRG_STAT_SHIFT;
        *done = (val == REG08_CHRG_STAT_CHGDONE);
    }

    return ret;
}

static int sc8960x_set_ichg(struct charger_device *chg_dev, u32 curr)
{
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

    pr_err("charge curr = %d\n", curr);

    return sc8960x_set_chargecurrent(sc, curr / 1000);
}

static int sc8960x_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
    u8 reg_val;
    int ichg;
    int ret;

    ret = sc8960x_read_byte(sc, SC8960X_REG_02, &reg_val);
    if (!ret) {
        ichg = (reg_val & REG02_ICHG_MASK) >> REG02_ICHG_SHIFT;
        ichg = ichg * REG02_ICHG_LSB + REG02_ICHG_BASE;
        *curr = ichg * 1000;
    }

    return ret;
}

static int sc8960x_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
    *curr = 60 * 1000;

    return 0;
}

static int sc8960x_set_vchg(struct charger_device *chg_dev, u32 volt)
{
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

    pr_err("charge volt = %d\n", volt);

    return sc8960x_set_chargevolt(sc, volt / 1000);
}

static int sc8960x_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
    u8 reg_val;
    int vchg;
    int ret;

    ret = sc8960x_read_byte(sc, SC8960X_REG_04, &reg_val);
    if (!ret) {
        vchg = (reg_val & REG04_VREG_MASK) >> REG04_VREG_SHIFT;
        vchg = vchg * REG04_VREG_LSB + REG04_VREG_BASE;
        ret = sc8960x_read_byte(sc, SC8960X_REG_0D, &reg_val);
        if (!ret) {
            vchg += (((reg_val & REG0D_VBAT_REG_FT_MASK) >>
                REG0D_VBAT_REG_FT_SHIFT) * 8);
        }
        *volt = vchg * 1000;
    }

    return ret;
}

static int sc8960x_set_ivl(struct charger_device *chg_dev, u32 volt)
{
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

    pr_err("vindpm volt = %d\n", volt);

    return sc8960x_set_input_volt_limit(sc, volt / 1000);

}

static int sc8960x_set_icl(struct charger_device *chg_dev, u32 curr)
{
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

    pr_err("indpm curr = %d\n", curr);

    return sc8960x_set_input_current_limit(sc, curr / 1000);
}

static int sc8960x_get_icl(struct charger_device *chg_dev, u32 *curr)
{
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
    u8 reg_val;
    int icl;
    int ret;

    ret = sc8960x_read_byte(sc, SC8960X_REG_00, &reg_val);
    if (!ret) {
        icl = (reg_val & REG00_IINLIM_MASK) >> REG00_IINLIM_SHIFT;
        icl = icl * REG00_IINLIM_LSB + REG00_IINLIM_BASE;
        *curr = icl * 1000;
    }

    return ret;

}

/* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/15 start */
static int sc8960x_get_ibus(struct charger_device *chg_dev, u32 *curr)
{
    /*return 1650mA as ibus*/
    *curr = PD_INPUT_CURRENT;

    return 0;
}
/* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/15 end */

static int sc8960x_kick_wdt(struct charger_device *chg_dev)
{
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

    return sc8960x_reset_watchdog_timer(sc);
}

static int sc8960x_set_otg(struct charger_device *chg_dev, bool en)
{
    int ret;
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

    if (en) {
        ret = sc8960x_disable_charger(sc);
        ret = sc8960x_enable_otg(sc);
    }
    else {
        ret = sc8960x_disable_otg(sc);
        ret = sc8960x_enable_charger(sc);
    }

    pr_err("%s OTG %s\n", en ? "enable" : "disable",
        !ret ? "successfully" : "failed");

    return ret;
}

static int sc8960x_set_safety_timer(struct charger_device *chg_dev, bool en)
{
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
    int ret;

    if (en)
        ret = sc8960x_enable_safety_timer(sc);
    else
        ret = sc8960x_disable_safety_timer(sc);

    return ret;
}

static int sc8960x_is_safety_timer_enabled(struct charger_device *chg_dev,
                    bool *en)
{
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
    int ret;
    u8 reg_val;

    ret = sc8960x_read_byte(sc, SC8960X_REG_05, &reg_val);

    if (!ret)
        *en = !!(reg_val & REG05_EN_TIMER_MASK);

    return ret;
}

static int sc8960x_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
    int ret;

    pr_err("otg curr = %d\n", curr);

    ret = sc8960x_set_boost_current(sc, curr / 1000);

    return ret;
}

/* hs14 code for SR-AL6528A-01-306|AL6528A-164|AL6528ADEU-643 by gaozhengwei|wenyaqi at 2022/10/13 start */
static int sc8960x_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

    if (!sc->psy) {
        dev_notice(sc->dev, "%s: cannot get psy\n", __func__);
        return -ENODEV;
    }

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
/* hs14 code for SR-AL6528A-01-306|AL6528A-164|AL6528ADEU-643 by gaozhengwei|wenyaqi at 2022/10/13 end */

/* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 start */
static int sc8960x_get_vbus_status(struct charger_device *chg_dev)
{
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

    return sc->vbus_stat;
}

static int sc8960x_dynamic_set_hwovp_threshold(struct charger_device *chg_dev,
                    int adapter_type)
{
    struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
    u8 val = 0;

    if (adapter_type == SC_ADAPTER_NORMAL)
        val = REG06_OVP_6P4V;
    else if (adapter_type == SC_ADAPTER_HV)
        val = REG06_OVP_11V;

    return sc8960x_update_bits(sc, SC8960X_REG_06, REG06_OVP_MASK,
                val << REG06_OVP_SHIFT);
}
/* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 end */

static struct charger_ops sc8960x_chg_ops = {
    /* Normal charging */
    .plug_in = sc8960x_plug_in,
    .plug_out = sc8960x_plug_out,
    .dump_registers = sc8960x_dump_register,
    .enable = sc8960x_charging,
    .is_enabled = sc8960x_is_charging_enable,
    .get_charging_current = sc8960x_get_ichg,
    .set_charging_current = sc8960x_set_ichg,
    .get_input_current = sc8960x_get_icl,
    .set_input_current = sc8960x_set_icl,
    .get_constant_voltage = sc8960x_get_vchg,
    .set_constant_voltage = sc8960x_set_vchg,
    .kick_wdt = sc8960x_kick_wdt,
    .set_mivr = sc8960x_set_ivl,
    .is_charging_done = sc8960x_is_charging_done,
    .get_min_charging_current = sc8960x_get_min_ichg,

    /* Safety timer */
    .enable_safety_timer = sc8960x_set_safety_timer,
    .is_safety_timer_enabled = sc8960x_is_safety_timer_enabled,

    /* Power path */
    .enable_powerpath = NULL,
    .is_powerpath_enabled = NULL,

    /* OTG */
    .enable_otg = sc8960x_set_otg,
    .set_boost_current_limit = sc8960x_set_boost_ilmt,
    .enable_discharge = NULL,

    /* PE+/PE+20 */
    .send_ta_current_pattern = NULL,
    .set_pe20_efficiency_table = NULL,
    .send_ta20_current_pattern = NULL,
    .enable_cable_drop_comp = NULL,

    /* ADC */
    .get_tchg_adc = NULL,
    /* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/15 start */
    .get_ibus_adc = sc8960x_get_ibus,
    /* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/15 end */

    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
    /* Event */
    .event = sc8960x_do_event,
    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 end */

    /* hs14 code for SR-AL6528A-01-299 by gaozhengwei at 2022/09/02 start */
    .set_hiz_mode = sc8960x_set_hiz_mode,
    .get_hiz_mode = sc8960x_get_hiz_mode,
    /* hs14 code for SR-AL6528A-01-299 by gaozhengwei at 2022/09/02 end */

    /* hs14 code for SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 start*/
    .get_ship_mode = sc8960x_get_shipmode,
    .set_ship_mode = sc8960x_set_shipmode,
    /* hs14 code for SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 end*/

    /* hs14 code for  SR-AL6528A-01-339 by shanxinkai at 2022/09/30 start*/
    .get_chr_status = sc8960x_get_charging_status,
    /* hs14 code for  SR-AL6528A-01-339 by shanxinkai at 2022/09/30 end*/

    /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 start */
    .get_vbus_status = sc8960x_get_vbus_status,
    .dynamic_set_hwovp_threshold = sc8960x_dynamic_set_hwovp_threshold,
    /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 end */

    /* hs14 code for SR-AL6528A-01-255 by wenyaqi at 2022/10/26 start */
    /* charger type detection */
    .enable_chg_type_det = sc8960x_enable_chg_type_det,
    /* hs14 code for SR-AL6528A-01-255 by wenyaqi at 2022/10/26 end */
    /* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 start */
    .bypass_chgdet = sc8960x_bypass_chgdet,
    /* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 end */
};

static struct of_device_id sc8960x_charger_match_table[] = {
    {
    .compatible = "sc,sc89601d",
    .data = &pn_data[PN_SC89601D],
    },
    {},
};
MODULE_DEVICE_TABLE(of, sc8960x_charger_match_table);


/* hs14 code for SR-AL6528A-01-304 by gaozhengwei at 2022/09/07 start */
static int sc8960x_charger_remove(struct i2c_client *client);
/* hs14 code for SR-AL6528A-01-304 by gaozhengwei at 2022/09/07 end */
static int sc8960x_charger_probe(struct i2c_client *client,
                const struct i2c_device_id *id)
{
    struct sc8960x *sc;
    const struct of_device_id *match;
    struct device_node *node = client->dev.of_node;

    int ret = 0;

    sc = devm_kzalloc(&client->dev, sizeof(struct sc8960x), GFP_KERNEL);
    if (!sc)
        return -ENOMEM;

    sc->dev = &client->dev;
    sc->client = client;

    i2c_set_clientdata(client, sc);

    mutex_init(&sc->i2c_rw_lock);

    ret = sc8960x_detect_device(sc);
    if (ret) {
        pr_err("No sc8960x device found!\n");
        return -ENODEV;
    }

    match = of_match_node(sc8960x_charger_match_table, node);
    if (match == NULL) {
        pr_err("device tree match not found\n");
        return -EINVAL;
    }

    /* hs14 code for SR-AL6528A-01-304 by gaozhengwei at 2022/09/07 start */
    if (sc->part_no != *(int *)match->data) {
        pr_info("part no mismatch, hw:%s, devicetree:%s\n",
            pn_str[sc->part_no], pn_str[*(int *) match->data]);
        sc8960x_charger_remove(client);
        return -EINVAL;
    }
    /* hs14 code for SR-AL6528A-01-304 by gaozhengwei at 2022/09/07 end */

    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
    INIT_DELAYED_WORK(&sc->psy_dwork, sc8960x_inform_psy_dwork_handler);
    INIT_DELAYED_WORK(&sc->prob_dwork, sc8960x_inform_prob_dwork_handler);
    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 end */
    /* hs14 code for AL6528A-371 by qiaodan at 2022/10/25 start */
    sc->shipmode_wr_value = 0;
    /* hs14 code for AL6528A-371 by qiaodan at 2022/10/25 end */
    /* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 start */
    sc->bypass_chgdet_en = false;
    /* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 end */
    sc->platform_data = sc8960x_parse_dt(node, sc);

    if (!sc->platform_data) {
        pr_err("No platform data provided.\n");
        return -EINVAL;
    }

    ret = sc8960x_init_device(sc);
    if (ret) {
        pr_err("Failed to init device\n");
        return ret;
    }

    sc8960x_register_interrupt(sc);

    sc->chg_dev = charger_device_register(sc->chg_dev_name,
                        &client->dev, sc,
                        &sc8960x_chg_ops,
                        &sc8960x_chg_props);
    if (IS_ERR_OR_NULL(sc->chg_dev)) {
    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
        cancel_delayed_work_sync(&sc->prob_dwork);
        cancel_delayed_work_sync(&sc->psy_dwork);
    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 end */
        ret = PTR_ERR(sc->chg_dev);
        return ret;
    }

    ret = sysfs_create_group(&sc->dev->kobj, &sc8960x_attr_group);
    if (ret)
        dev_err(sc->dev, "failed to register sysfs. err: %d\n", ret);

    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
    Charger_Detect_Init();

    mod_delayed_work(system_wq, &sc->prob_dwork,
                    msecs_to_jiffies(2*1000));
    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */

    pr_err("sc8960x probe successfully, Part Num:%d, Revision:%d\n!",
        sc->part_no, sc->revision);

    /* hs14 code for SR-AL6528A-01-258 by shanxinkai at 2022/09/13 start */
    chg_info = SC89601;
    /* hs14 code for SR-AL6528A-01-258 by shanxinkai at 2022/09/13 end */

    return 0;
}

static int sc8960x_charger_remove(struct i2c_client *client)
{
    struct sc8960x *sc = i2c_get_clientdata(client);

    mutex_destroy(&sc->i2c_rw_lock);

    sysfs_remove_group(&sc->dev->kobj, &sc8960x_attr_group);

    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
    cancel_delayed_work_sync(&sc->prob_dwork);
    cancel_delayed_work_sync(&sc->psy_dwork);
    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 end */

    return 0;
}

static void sc8960x_charger_shutdown(struct i2c_client *client)
{
    /* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 start */
    struct sc8960x *sc = i2c_get_clientdata(client);

    cancel_delayed_work_sync(&sc->psy_dwork);
    /* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 end */
}

static int sc8960x_suspend(struct device *dev)
{
    struct sc8960x *sc = dev_get_drvdata(dev);

    if (device_may_wakeup(dev))
            enable_irq_wake(sc->client->irq);

    disable_irq(sc->client->irq);

    return 0;
}

static int sc8960x_resume(struct device *dev)
{
    struct sc8960x *sc = dev_get_drvdata(dev);

    enable_irq(sc->client->irq);
    if (device_may_wakeup(dev))
        disable_irq_wake(sc->client->irq);

    return 0;
}

static SIMPLE_DEV_PM_OPS(sc8960x_pm_ops, sc8960x_suspend, sc8960x_resume);

static struct i2c_driver sc8960x_charger_driver = {
    .driver = {
        .name = "sc8960x-charger",
        .owner = THIS_MODULE,
        .of_match_table = sc8960x_charger_match_table,
        .pm = &sc8960x_pm_ops,
        },

    .probe = sc8960x_charger_probe,
    .remove = sc8960x_charger_remove,
    .shutdown = sc8960x_charger_shutdown,

};

module_i2c_driver(sc8960x_charger_driver);

MODULE_DESCRIPTION("SC SC8960x Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("South Chip");
