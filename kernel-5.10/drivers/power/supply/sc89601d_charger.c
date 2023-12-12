// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
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
#include <linux/regmap.h>

#define CONFIG_MTK_CLASS

#ifdef CONFIG_MTK_CLASS
#include "charger_class.h"
/*Tab A9 code for SR-AX6739A-01-531 by hualei at 20230523 start*/
#include "mtk_charger.h"
/*Tab A9 code for SR-AX6739A-01-531 by hualei at 20230523 end*/
#endif /*CONFIG_MTK_CLASS*/

/*Tab A9 code for SR-AX6739A-01-489 by wenyaqi at 20230504 start*/
#include <linux/power/gxy_psy_sysfs.h>

/**********************************************************
 *
 *   [extern for other module]
 *
 *********************************************************/
extern void gxy_bat_set_chginfo(enum gxy_bat_chg_info cinfo_data);
/*Tab A9 code for SR-AX6739A-01-489 by wenyaqi at 202305044 end*/

#define SC8960X_DRV_VERSION         "1.0.0_G"

#define SC8960X_REG_7F              0x7F
#define REG7F_KEY1                  0x5A
#define REG7F_KEY2                  0x68
#define REG7F_KEY3                  0x65
#define REG7F_KEY4                  0x6E
#define REG7F_KEY5                  0x67
#define REG7F_KEY6                  0x4C
#define REG7F_KEY7                  0x69
#define REG7F_KEY8                  0x6E


#define SC8960X_REG_81              0x81
#define REG81_HVDCP_EN_MASK         0x02
#define REG81_HVDCP_EN_SHIFT        1
#define REG81_HVDCP_ENABLE          1
#define REG81_HVDCP_DISABLE         0

/*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 start*/
#define REG07_BATFET_DLY_0S         0
#define REG07_BATFET_DLY_10S        1
#define SHIP_DLY                    10
/*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 end*/

#define SC8960X_REG_92              0x92
#define REG92_PFM_VAL               0x71

/*Tab A9 code for P230714-02020 by lina at 20230731 start*/
#define SC8960X_REG_93             0x93
#define SC8960X_REG_94             0x94
/*Tab A9 code for P230714-02020 by lina at 20230731 end*/
/*Tab A9 code for AX6739A-527 by hualei at 20230609 start*/
#define SC8960_ICHG_LIM             500
/*Tab A9 code for AX6739A-527 by hualei at 20230609 end*/

enum sc8960x_part_no {
    SC89601D_ID = 0x03,
};

enum sc8960x_vbus_stat {
    VBUS_STAT_NO_INPUT = 0,
    VBUS_STAT_SDP,
    VBUS_STAT_CDP,
    VBUS_STAT_DCP,
    VBUS_STAT_HVDCP,
    VBUS_STAT_UNKOWN,
    VBUS_STAT_NONSTAND,
    VBUS_STAT_OTG,
};

enum sc8960x_chg_stat {
    CHG_STAT_NOT_CHARGE = 0,
    CHG_STAT_PRE_CHARGE,
    CHG_STAT_FAST_CHARGE,
    CHG_STAT_CHARGE_DONE,
};

/*Tab A9 code for AX6739A-516 by wenyaqi at 20230603 start*/
/* map with mtk_chg_type_det.c */
enum attach_type {
    ATTACH_TYPE_NONE,
    ATTACH_TYPE_PWR_RDY,
    ATTACH_TYPE_TYPEC,
    ATTACH_TYPE_PD,
    ATTACH_TYPE_PD_SDP,
    ATTACH_TYPE_PD_DCP,
    ATTACH_TYPE_PD_NONSTD,
};
/*Tab A9 code for AX6739A-516 by wenyaqi at 20230603 end*/

#define SC8960X_REGMAX              0x0E


enum sc8960x_fields {
    EN_HIZ, EN_STAT_PIN, IINDPM, /*reg00*/
    PFM_DIS, WD_RST, OTG_CFG, CHG_CFG, VSYS_MIN, VBATLOW_OTG, /*reg01*/
    BOOST_LIM, ICC, /*reg02*/
    ITC, ITERM, /*reg03*/
    VBAT_REG, TOP_OFF_TIMER, VRECHG, /*reg04*/
    EN_TERM, TWD, EN_TIMER, CHG_TIMER, TREG, JEITA_COOL_ISET, /*reg05*/
    VAC_OVP, BOOSTV0, VINDPM, /*reg06*/
    FORCE_DPDM, TMR2X_EN, BATFET_DIS, JEITA_WARM_VSET1, BATFET_DLY,
    BATFET_RST_EN, VINDPM_TRACK, /*reg07*/
    VBUS_STAT, CHG_STAT, PG_STAT, THERM_STAT, VSYS_STAT, /*reg08*/
    WD_FAULT, BOOST_FAULT, CHRG_FAULT, BAT_FAULT, NTC_FAULT, /*reg09*/
    VBUS_GD, VINDPM_STAT, IINDPM_STAT, CV_STAT, TOP_OFF_ACTIVE,
    ACOV_STAT, VINDPM_INT_MASK, IINDPM_INT_MASK, /*reg0A*/
    REG_RST, PN, DEV_VERSION, /*reg0B*/
    JEITA_COOL_ISET2, JEITA_WARM_VSET2, JEITA_WARM_ISET, JEITA_COOL_TEMP,
    JEITA_WARM_TEMP, /*reg0C*/
    VBAT_REG_FT, BOOST_NTC_HOT_TEMP, BOOST_NTC_COLD_TEMP, BOOSTV1, ISHORT, /*reg0D*/
    VTC, INPUT_DET_DONE, AUTO_DPDM_EN, BUCK_FREQ, BOOST_FREQ, VSYSOVP,
    NTC_DIS, /*reg0E*/
    EN_HVDCP, DM_DRIVE, DP_DRIVE,/*reg81*/

    F_MAX_FIELDS,
};

enum sc8960_reg_range {
    SC8960X_IINDPM,
    SC8960X_ICHG,
    SC8960X_IBOOST,
    SC8960X_VBAT_REG,
    SC8960X_VINDPM,
    SC8960X_ITERM,
};

struct reg_range {
    u32 min;
    u32 max;
    u32 step;
    u32 offset;
    const u32 *table;
    u16 num_table;
    bool round_up;
};

struct sc8960x_param_e {
    int pfm_dis;
    int vsys_min;
    int vbat_low_otg;
    int pre_chg_curr;
    int term_chg_curr;
    int vbat_cv;
    int top_off_timer;
    int vrechg_volt;
    int term_en;
    int wdt_timer;
    int en_chg_saft_timer;
    int charge_timer;
    int vac_ovp;
    int iboost;
    int vboost;
    int vindpm_track;
    int vindpm_int_mask;
    int iindpm_int_mask;
    int vpre_to_cc;
    int auto_dpdm_en;
    int ntc_pin_dis;
};

struct sc8960x_chip {
    struct device *dev;
    struct i2c_client *client;
    struct regmap *regmap;
    struct regmap_field *rmap_fields[F_MAX_FIELDS];

    struct sc8960x_param_e sc8960x_param;

    int irq_gpio;
    int irq;

    int part_no;
    int dev_version;

    int psy_usb_type;
    int chg_type;
    /*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 start*/
    int shipmode_wr_value;
    /*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 end*/

    bool vbus_good;
    bool power_good;
    bool chg_config;
    /*Tab A9 code for AX6739A-516 | OT11BSP-42 by qiaodan at 20230613 start*/
    int typec_attached;
    /*Tab A9 code for AX6739A-516 | OT11BSP-42 by qiaodan at 20230613 end*/

    struct delayed_work force_detect_dwork;
    int force_detect_count;

    struct delayed_work chg_psy_dwork;
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
    struct delayed_work hvdcp_done_work;
    bool hvdcp_done;
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/
    /*Tab A9 code for SR-AX6739A-01-486 by wenyaqi at 20230619 start*/
    bool hiz_force_dpdm;
    /*Tab A9 code for SR-AX6739A-01-486 by wenyaqi at 20230619 end*/

    struct power_supply_desc psy_desc;
    struct power_supply_config psy_cfg;
    struct power_supply *psy;
    struct power_supply *chg_psy;

#ifdef CONFIG_MTK_CLASS
    struct charger_device *chg_dev;
    struct regulator_dev *otg_rdev;
#endif /*CONFIG_MTK_CLASS*/

    const char *chg_dev_name;
};

/**********************************************************
 *
 *   [Global Variable]
 *
 *********************************************************/
static struct charger_device *s_chg_dev_otg;

static const u32 sc8960x_iboost[] = {
    500, 1200,
};

static const u32 sc8960x_vindpm[] = {
    3900, 4000, 4100, 4200, 4300, 4400, 4500, 4600,
    4700, 4800, 4900, 5000, 5100, 8000, 8200, 8400,
};

#define SC8960x_CHG_RANGE(_min, _max, _step, _offset, _ru) \
{ \
    .min = _min, \
    .max = _max, \
    .step = _step, \
    .offset = _offset, \
    .round_up = _ru, \
}

#define SC8960x_CHG_RANGE_T(_table, _ru) \
    { .table = _table, .num_table = ARRAY_SIZE(_table), .round_up = _ru, }


static struct reg_range sc8960x_reg_range[] = {
    [SC8960X_IINDPM]    = SC8960x_CHG_RANGE(100, 3200, 100, 100, false),
    [SC8960X_ICHG]      = SC8960x_CHG_RANGE(0, 3000, 60, 0, false),
    [SC8960X_IBOOST]    = SC8960x_CHG_RANGE_T(sc8960x_iboost, false),
    [SC8960X_VBAT_REG]  = SC8960x_CHG_RANGE(3848, 4864, 8, 3848, false),
    [SC8960X_VINDPM]    = SC8960x_CHG_RANGE_T(sc8960x_vindpm, false),
    [SC8960X_ITERM]     = SC8960x_CHG_RANGE(60, 960, 60, 60, false),
};

//REGISTER
static const struct reg_field sc8960x_reg_fields[] = {
    /*reg00*/
    [EN_HIZ] = REG_FIELD(0x00, 7, 7),
    [EN_STAT_PIN] = REG_FIELD(0x00, 5, 6),
    [IINDPM] = REG_FIELD(0x00, 0, 4),
    /*reg01*/
    [PFM_DIS] = REG_FIELD(0x01, 7, 7),
    [WD_RST] = REG_FIELD(0x01, 6, 6),
    [OTG_CFG] = REG_FIELD(0x01, 5, 5),
    [CHG_CFG] = REG_FIELD(0x01, 4, 4),
    [VSYS_MIN] = REG_FIELD(0x01, 1, 3),
    [VBATLOW_OTG] = REG_FIELD(0x01, 0, 0),
    /*reg02*/
    [BOOST_LIM] = REG_FIELD(0x02, 7, 7),
    [ICC] = REG_FIELD(0x02, 0, 5),
    /*reg03*/
    [ITC] = REG_FIELD(0x03, 4, 7),
    [ITERM] = REG_FIELD(0x03, 0, 3),
    /*reg04*/
    [VBAT_REG] = REG_FIELD(0x04, 3, 7),
    [TOP_OFF_TIMER] = REG_FIELD(0x04, 1, 2),
    [VRECHG] = REG_FIELD(0x04, 0, 0),
    /*reg05*/
    [EN_TERM] = REG_FIELD(0x05, 7, 7),
    [TWD] = REG_FIELD(0x05, 4, 5),
    [EN_TIMER] = REG_FIELD(0x05, 3, 3),
    [CHG_TIMER] = REG_FIELD(0x05, 2, 2),
    [TREG] = REG_FIELD(0x05, 1, 1),
    [JEITA_COOL_ISET] = REG_FIELD(0x05, 0, 0),
    /*reg06*/
    [VAC_OVP] = REG_FIELD(0x06, 6, 7),
    [BOOSTV0] = REG_FIELD(0x06, 4, 5),
    [VINDPM] = REG_FIELD(0x06, 0, 3),
    /*reg07*/
    [FORCE_DPDM] = REG_FIELD(0x07, 7, 7),
    [TMR2X_EN] = REG_FIELD(0x07, 6, 6),
    [BATFET_DIS] = REG_FIELD(0x07, 5, 5),
    [JEITA_WARM_VSET1] = REG_FIELD(0x07, 4, 4),
    [BATFET_DLY] = REG_FIELD(0x07, 3, 3),
    [BATFET_RST_EN] = REG_FIELD(0x07, 2, 2),
    [VINDPM_TRACK] = REG_FIELD(0x07, 0, 1),
    /*reg08*/
    [VBUS_STAT] = REG_FIELD(0x08, 5, 7),
    [CHG_STAT] = REG_FIELD(0x08, 3, 4),
    [PG_STAT] = REG_FIELD(0x08, 2, 2),
    [THERM_STAT] = REG_FIELD(0x08, 1, 1),
    [VSYS_STAT] = REG_FIELD(0x08, 0, 0),
    /*reg09*/
    [WD_FAULT] = REG_FIELD(0x09, 7, 7),
    [BOOST_FAULT] = REG_FIELD(0x09, 6, 6),
    [CHRG_FAULT] = REG_FIELD(0x09, 4, 5),
    [BAT_FAULT] = REG_FIELD(0x09, 3, 3),
    [NTC_FAULT] = REG_FIELD(0x09, 0, 2),
    /*reg0A*/
    [VBUS_GD] = REG_FIELD(0x0A, 7, 7),
    [VINDPM_STAT] = REG_FIELD(0x0A, 6, 6),
    [IINDPM_STAT] = REG_FIELD(0x0A, 5, 5),
    [CV_STAT] = REG_FIELD(0x0A, 4, 4),
    [TOP_OFF_ACTIVE] = REG_FIELD(0x0A, 3, 3),
    [ACOV_STAT] = REG_FIELD(0x0A, 2, 2),
    [VINDPM_INT_MASK] = REG_FIELD(0x0A, 1, 1),
    [IINDPM_INT_MASK] = REG_FIELD(0x0A, 0, 0),
    /*reg0B*/
    [REG_RST] = REG_FIELD(0x0B, 7, 7),
    [PN] = REG_FIELD(0x0B, 3, 6),
    [DEV_VERSION] = REG_FIELD(0x0B, 0, 2),
    /*reg0C*/
    [JEITA_COOL_ISET2] = REG_FIELD(0x0C, 7, 7),
    [JEITA_WARM_VSET2] = REG_FIELD(0x0C, 6, 6),
    [JEITA_WARM_ISET] = REG_FIELD(0x0C, 4, 5),
    [JEITA_COOL_TEMP] = REG_FIELD(0x0C, 2, 3),
    [JEITA_WARM_TEMP] = REG_FIELD(0x0C, 0, 1),
    /*reg0D*/
    [VBAT_REG_FT] = REG_FIELD(0x0D, 6, 7),
    [BOOST_NTC_HOT_TEMP] = REG_FIELD(0x0D, 4, 5),
    [BOOST_NTC_COLD_TEMP] = REG_FIELD(0x0D, 3, 3),
    [BOOSTV1] = REG_FIELD(0x0D, 1, 2),
    [ISHORT] = REG_FIELD(0x0D, 0, 0),
    /*reg0E*/
    [VTC] = REG_FIELD(0x0E, 7, 7),
    [INPUT_DET_DONE] = REG_FIELD(0x0E, 6, 6),
    [AUTO_DPDM_EN] = REG_FIELD(0x0E, 5, 5),
    [BUCK_FREQ] = REG_FIELD(0x0E, 4, 4),
    [BOOST_FREQ] = REG_FIELD(0x0E, 3, 3),
    [VSYSOVP] = REG_FIELD(0x0E, 1, 2),
    [NTC_DIS] = REG_FIELD(0x0E, 0, 0),
    /*reg81*/
    [DP_DRIVE] = REG_FIELD(0x81, 5, 7),
    [DM_DRIVE] = REG_FIELD(0x81, 2, 4),
    [EN_HVDCP] = REG_FIELD(0x81, 1, 1),
};

static const struct regmap_config sc8960x_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,
    // .max_register = SC8960X_REGMAX,
};

/********************COMMON API***********************/
static u8 val2reg(enum sc8960_reg_range id, u32 val)
{
    int i;
    u8 reg;
    const struct reg_range *range = &sc8960x_reg_range[id];

    if (!range)
        return val;

    if (range->table) {
        if (val <= range->table[0])
            return 0;
        for (i = 1; i < range->num_table - 1; i++) {
            if (val == range->table[i])
                return i;
            if (val > range->table[i] &&
                val < range->table[i + 1])
                return range->round_up ? i + 1 : i;
        }
        return range->num_table - 1;
    }
    if (val <= range->min)
        reg = 0;
    else if (val >= range->max)
        reg = (range->max - range->offset) / range->step;
    else if (range->round_up)
        reg = (val - range->offset) / range->step + 1;
    else
        reg = (val - range->offset) / range->step;
    return reg;
}

static u32 reg2val(enum sc8960_reg_range id, u8 reg)
{
    const struct reg_range *range = &sc8960x_reg_range[id];
    if (!range)
        return reg;
    return range->table ? range->table[reg] :
                  range->offset + range->step * reg;
}

/*********************I2C API*********************/
static int sc8960x_field_read(struct sc8960x_chip *sc,
                enum sc8960x_fields field_id, int *val)
{
    int ret;

    ret = regmap_field_read(sc->rmap_fields[field_id], val);
    if (ret < 0) {
        dev_err(sc->dev, "sc8960x read field %d fail: %d\n", field_id, ret);
    }

    return ret;
}

static int sc8960x_field_write(struct sc8960x_chip *sc,
                enum sc8960x_fields field_id, int val)
{
    int ret;

    ret = regmap_field_write(sc->rmap_fields[field_id], val);
    if (ret < 0) {
        dev_err(sc->dev, "sc8960x read field %d fail: %d\n", field_id, ret);
    }

    return ret;
}


/*********************CHIP API*********************/
static int sc8960x_set_key(struct sc8960x_chip *sc)
{
    regmap_write(sc->regmap, SC8960X_REG_7F, REG7F_KEY1);
    regmap_write(sc->regmap, SC8960X_REG_7F, REG7F_KEY2);
    regmap_write(sc->regmap, SC8960X_REG_7F, REG7F_KEY3);
    regmap_write(sc->regmap, SC8960X_REG_7F, REG7F_KEY4);
    regmap_write(sc->regmap, SC8960X_REG_7F, REG7F_KEY5);
    regmap_write(sc->regmap, SC8960X_REG_7F, REG7F_KEY6);
    regmap_write(sc->regmap, SC8960X_REG_7F, REG7F_KEY7);
    return regmap_write(sc->regmap, SC8960X_REG_7F, REG7F_KEY8);
}

__attribute__((unused)) static int sc8960x_soft_enable_hvdcp(struct sc8960x_chip *sc)
{
    int val = 0;
    sc8960x_set_key(sc);

    /*dp and dm connected,dp 0.6V dm Hiz*/
    sc8960x_field_write(sc, DP_DRIVE, 0x02);
    sc8960x_field_write(sc, DM_DRIVE, 0x00);
    regmap_read(sc->regmap, SC8960X_REG_81, &val);
    pr_err("%s ----> reg 81 = 0x%02x\n", __func__, val);
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
    schedule_delayed_work(&sc->hvdcp_done_work, msecs_to_jiffies(1500));
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/

    return sc8960x_set_key(sc);
}

__attribute__((unused)) static int sc8960x_enable_hvdcp(struct sc8960x_chip *sc, bool enable)
{
    int val = (enable == true) ?
        REG81_HVDCP_ENABLE : REG81_HVDCP_DISABLE;

    sc8960x_set_key(sc);

    sc8960x_field_write(sc, EN_HVDCP, val);
    regmap_read(sc->regmap, SC8960X_REG_81, &val);
    dev_err(sc->dev, "----> reg 81 = 0x%02x\n", val);

    return sc8960x_set_key(sc);
}

static int sc8960x_set_wa(struct sc8960x_chip *sc)
{
    int ret;
    int reg_val;

    ret = regmap_read(sc->regmap, SC8960X_REG_92, &reg_val);
    if (ret) {
        sc8960x_set_key(sc);
    }
    regmap_write(sc->regmap, SC8960X_REG_92, REG92_PFM_VAL);

    return sc8960x_set_key(sc);
}

__attribute__((unused)) static int sc8960x_set_hiz(struct sc8960x_chip *sc, bool enable)
{
    int reg_val = enable ? 1 : 0;
    /*Tab A9 code for SR-AX6739A-01-486 by wenyaqi at 20230619 start*/
    static bool s_hiz_stat_old = false;

    if (enable == true) {
        sc->hvdcp_done = false;
    }
    if (enable == false && s_hiz_stat_old == true) {
        dev_info(sc->dev, "%s exit hiz to rerun bc12\n", __func__);
        sc->hiz_force_dpdm = true;
    }
    s_hiz_stat_old = enable;
    /*Tab A9 code for SR-AX6739A-01-486 by wenyaqi at 20230619 end*/

    return sc8960x_field_write(sc, EN_HIZ, reg_val);
}

/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 start*/
static int sc8960x_get_hiz_status(struct sc8960x_chip *sc, bool *enable)
{
    int ret, reg_val = 0;

    ret = sc8960x_field_read(sc, EN_HIZ, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read EN_HIZ failed(%d)\n", ret);
        return ret;
    }

    if (reg_val) {
        *enable = true;
    } else {
        *enable = false;
    }

    return ret;
}
/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 end*/

static int sc8960x_set_iindpm(struct sc8960x_chip *sc, int curr_ma)
{
    int reg_val = val2reg(SC8960X_IINDPM, curr_ma);

    return sc8960x_field_write(sc, IINDPM, reg_val);
}

static int sc8960x_get_iindpm(struct sc8960x_chip *sc, int *curr_ma)
{
    int ret, reg_val;

    ret = sc8960x_field_read(sc, IINDPM, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read iindpm failed(%d)\n", ret);
        return ret;
    }

    *curr_ma = reg2val(SC8960X_IINDPM, reg_val);

    return ret;
}

__attribute__((unused)) static int sc8960x_reset_wdt(struct sc8960x_chip *sc)
{
    return sc8960x_field_write(sc, WD_RST, 1);
}

static int sc8960x_set_chg_enable(struct sc8960x_chip *sc, bool enable)
{
    int reg_val = enable ? 1 : 0;

    sc->chg_config = enable;

    return sc8960x_field_write(sc, CHG_CFG, reg_val);
}

__attribute__((unused)) static int sc8960x_check_chg_enabled(struct sc8960x_chip *sc, bool *enable)
{
    int ret, reg_val;

    ret = sc8960x_field_read(sc, CHG_CFG, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read charge enable failed(%d)\n", ret);
        return ret;
    }
    *enable = !!reg_val;

    return ret;
}

__attribute__((unused)) static int sc8960x_set_otg_enable(struct sc8960x_chip *sc, bool enable)
{
    int reg_val = enable ? 1 : 0;

    return sc8960x_field_write(sc, OTG_CFG, reg_val);
}

__attribute__((unused)) static int sc8960x_check_otg_enabled(struct sc8960x_chip *sc, bool *enable)
{
    int ret, reg_val;

    ret = sc8960x_field_read(sc, OTG_CFG, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read otg enable failed(%d)\n", ret);
        return ret;
    }
    *enable = !!reg_val;

    return ret;
}

__attribute__((unused)) static int sc8960x_set_iboost(struct sc8960x_chip *sc, int curr_ma)
{
    int reg_val = val2reg(SC8960X_IBOOST, curr_ma);

    return sc8960x_field_write(sc, BOOST_LIM, reg_val);
}

static int sc8960x_set_ichg(struct sc8960x_chip *sc, int curr_ma)
{
    int reg_val = val2reg(SC8960X_ICHG, curr_ma);

    return sc8960x_field_write(sc, ICC, reg_val);
}

static int sc8960x_get_ichg(struct sc8960x_chip *sc, int *curr_ma)
{
    int ret, reg_val;

    ret = sc8960x_field_read(sc, ICC, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read ICC failed(%d)\n", ret);
        return ret;
    }

    *curr_ma = reg2val(SC8960X_ICHG, reg_val);

    return ret;
}

static int sc8960x_set_term_curr(struct sc8960x_chip *sc, int curr_ma)
{
    int reg_val = val2reg(SC8960X_ITERM, curr_ma);

    return sc8960x_field_write(sc, ITERM, reg_val);
}

static int sc8960x_get_term_curr(struct sc8960x_chip *sc, int *curr_ma)
{
    int ret, reg_val;

    ret = sc8960x_field_read(sc, ITERM, &reg_val);
    if (ret)
        return ret;

    *curr_ma = reg2val(SC8960X_ITERM, reg_val);

    return ret;
}

__attribute__((unused)) static int sc8960x_set_safet_timer(struct sc8960x_chip *sc, bool enable)
{
    int reg_val = enable ? 1 : 0;

    return sc8960x_field_write(sc, EN_TIMER, reg_val);
}

__attribute__((unused)) static int sc8960x_check_safet_timer(struct sc8960x_chip *sc, bool *enabled)
{
    int ret, reg_val;

    ret = sc8960x_field_read(sc, EN_TIMER, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read ICEN_TIMERC failed(%d)\n", ret);
        return ret;
    }

    *enabled = reg_val ? true : false;

    return ret;
}

static int sc8960x_set_vbat(struct sc8960x_chip *sc, int volt_mv)
{
    int reg_val = val2reg(SC8960X_VBAT_REG, volt_mv);

    sc8960x_field_write(sc, VBAT_REG_FT, reg_val & 0x03);
    return sc8960x_field_write(sc, VBAT_REG, reg_val >> 2);
}

static int sc8960x_get_vbat(struct sc8960x_chip *sc, int *volt_mv)
{
    int ret, reg_val_l, reg_val_h;

    ret = sc8960x_field_read(sc, VBAT_REG_FT, &reg_val_l);
    ret |= sc8960x_field_read(sc, VBAT_REG, &reg_val_h);
    if (ret) {
        dev_err(sc->dev, "read vbat reg failed(%d)\n", ret);
        return ret;
    }

    *volt_mv = reg2val(SC8960X_VBAT_REG, (reg_val_h << 2) | reg_val_l);

    return ret;
}

static int sc8960x_set_vindpm(struct sc8960x_chip *sc, int volt_mv)
{
    int reg_val = val2reg(SC8960X_VINDPM, volt_mv);

    return sc8960x_field_write(sc, VINDPM, reg_val);
}

static int sc8960x_get_vindpm(struct sc8960x_chip *sc, int *volt_mv)
{
    int ret, reg_val;

    ret = sc8960x_field_read(sc, VINDPM, &reg_val);
    if (ret)
        return ret;

    *volt_mv = reg2val(SC8960X_VINDPM, reg_val);

    return ret;
}

static int sc8960x_force_dpdm(struct sc8960x_chip *sc)
{
    /*Tab A9 code for AX6739A-2516 by lina at 20230729 start*/
    int reg_val = 0;
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

    dev_info(sc->dev, "%s\n", __func__);

    if(s_info->hv_voltage_switch1) {
        dev_info(sc->dev, "%s: PD enter,skip bc12\n", __func__);
        return ret;
    }
    /*Tab A9 code for AX6739A-2516 by lina at 20230729 end*/
    sc8960x_set_vindpm(sc, 4800);
    ret = regmap_read(sc->regmap, 0x07, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read reg 0x07 failed(%d)\n", ret);
        return ret;
    }

    return regmap_write(sc->regmap, 0x07, reg_val | 0x80);
}

/*Tab A9 code for P230612-08698 by wenyaqi at 20230615 start*/
static int sc8960x_get_charge_stat(struct sc8960x_chip *sc)
{
    int ret;
    int chg_stat, vbus_stat;
    int psy_stat_now = POWER_SUPPLY_STATUS_UNKNOWN;
    static int s_psy_stat_old = POWER_SUPPLY_STATUS_UNKNOWN;

    ret = sc8960x_field_read(sc, CHG_STAT, &chg_stat);
    if (ret)
        return ret;

    ret = sc8960x_field_read(sc, VBUS_STAT, &vbus_stat);
    if (ret)
        return ret;

    dev_info(sc->dev, "%s chg_stat=%d,vbus_stat=%d\n", __func__, chg_stat, vbus_stat);

    /*Tab A9 code for P230609-02043 by wenyaqi at 20230612 start*/
    if (vbus_stat == VBUS_STAT_OTG || sc->chg_type == POWER_SUPPLY_TYPE_UNKNOWN) {
    /*Tab A9 code for P230609-02043 by wenyaqi at 20230612 end*/
        psy_stat_now = POWER_SUPPLY_STATUS_DISCHARGING;
    } else {
        switch (chg_stat) {
        case CHG_STAT_NOT_CHARGE:
            if (sc->chg_config) {
                psy_stat_now = POWER_SUPPLY_STATUS_CHARGING;
            } else {
                psy_stat_now = POWER_SUPPLY_STATUS_NOT_CHARGING;
            }
            break;
        case CHG_STAT_PRE_CHARGE:
        case CHG_STAT_FAST_CHARGE:
            psy_stat_now = POWER_SUPPLY_STATUS_CHARGING;
            break;
        case CHG_STAT_CHARGE_DONE:
            psy_stat_now = POWER_SUPPLY_STATUS_FULL;
            break;
        }
    }

    if (s_psy_stat_old == POWER_SUPPLY_STATUS_NOT_CHARGING &&
        psy_stat_now == POWER_SUPPLY_STATUS_CHARGING) {
        power_supply_changed(sc->psy);
    }
    s_psy_stat_old = psy_stat_now;

    return psy_stat_now;
}
/*Tab A9 code for P230612-08698 by wenyaqi at 20230615 end*/

__attribute__((unused)) static int sc8960x_check_charge_done(struct sc8960x_chip *sc, bool *chg_done)
{
    int ret, reg_val;

    ret = sc8960x_field_read(sc, CHG_STAT, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read charge stat failed(%d)\n", ret);
        return ret;
    }

    *chg_done = (reg_val == CHG_STAT_CHARGE_DONE) ? true : false;
    return ret;
}

static int sc8960x_detect_device(struct sc8960x_chip *sc)
{
    int ret;

    ret = sc8960x_field_read(sc, DEV_VERSION, &sc->dev_version);
    if (ret) {
        dev_err(sc->dev, "read device version failed(%d)\n", ret);
        return ret;
    }

    return sc8960x_field_read(sc, PN, &sc->part_no);
}

static int sc8960x_reg_reset(struct sc8960x_chip *sc)
{
    return sc8960x_field_write(sc, REG_RST, 1);
}

static int sc8960x_dump_register(struct sc8960x_chip *sc)
{
    int addr;
    int val;
    int ret;

    for (addr = 0x0; addr <= SC8960X_REGMAX; addr++) {
        ret = regmap_read(sc->regmap, addr, &val);
        if (ret) {
            dev_err(sc->dev, "read reg 0x%02x failed(%d)\n", addr, ret);
            return ret;
        }
        dev_info(sc->dev, "Reg[0x%02x] = 0x%02x\n", addr, val);
    }
    return ret;
}


#ifdef CONFIG_MTK_CLASS
/********************MTK OPS***********************/
static int sc8960x_plug_in(struct charger_device *chg_dev)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int ret;

    dev_info(sc->dev, "%s\n", __func__);

    ret = sc8960x_set_chg_enable(sc, true);
    if (ret) {
        dev_err(sc->dev, "Failed to enable charging:%d\n", ret);
    }

    return ret;
}

static int sc8960x_plug_out(struct charger_device *chg_dev)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int ret;

    dev_info(sc->dev, "%s\n", __func__);

    ret = sc8960x_set_chg_enable(sc, false);
    if (ret) {
        dev_err(sc->dev, "Failed to disable charging:%d\n", ret);
    }

    return ret;
}

static int sc8960x_enable(struct charger_device *chg_dev, bool enable)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int ret = 0;
    bool otg_en = false;

    ret = sc8960x_check_otg_enabled(sc, &otg_en);
    if (!ret) {
        if (otg_en == true && enable == true) {
            // avoid otg fail when chg_en is true
            dev_info(sc->dev, "%s:otg_en=%d,skip chg_en\n", __func__, otg_en);
            return -ENODEV;
        }
    }

    ret = sc8960x_set_chg_enable(sc, enable);

    dev_info(sc->dev, "%s charger %s\n", enable ? "enable" : "disable",
        !ret ? "successfully" : "failed");

    return ret;
}

static int sc8960x_is_enabled(struct charger_device *chg_dev, bool *enabled)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int ret;

    ret = sc8960x_check_chg_enabled(sc, enabled);
    dev_info(sc->dev, "charger is %s\n", *enabled ? "charging" : "not charging");

    return ret;
}

static int sc8960x_get_charging_current(struct charger_device *chg_dev, u32 *curr)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int curr_ma;
    int ret;

    dev_info(sc->dev, "%s\n", __func__);

    ret = sc8960x_get_ichg(sc, &curr_ma);
    if (!ret) {
        *curr = curr_ma * 1000;
    }

    return ret;
}

static int sc8960x_set_charging_current(struct charger_device *chg_dev, u32 curr)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s: charge curr = %duA\n", __func__, curr);

    return sc8960x_set_ichg(sc, curr / 1000);
}

static int sc8960x_get_input_current(struct charger_device *chg_dev, u32 *curr)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int curr_ma;
    int ret;

    dev_info(sc->dev, "%s\n", __func__);

    ret = sc8960x_get_iindpm(sc, &curr_ma);
    if (!ret) {
        *curr = curr_ma * 1000;
    }

    return ret;
}

/*Tab A9 code for SR-AX6739A-01-503 by qiaodan at 20230510 start*/
static int sc8960x_get_min_input_current(struct charger_device *chg_dev, u32 *curr)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int ret = 0;

    dev_info(sc->dev, "%s\n", __func__);
    *curr = sc8960x_reg_range[SC8960X_IINDPM].min * 1000;

    return ret;
}
/*Tab A9 code for SR-AX6739A-01-503 by qiaodan at 20230510 end*/

static int sc8960x_set_input_current(struct charger_device *chg_dev, u32 curr)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s: iindpm curr = %duA\n", __func__, curr);

    return sc8960x_set_iindpm(sc, curr / 1000);
}

static int sc8960x_get_constant_voltage(struct charger_device *chg_dev, u32 *volt)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int volt_mv;
    int ret;

    dev_info(sc->dev, "%s\n", __func__);

    ret = sc8960x_get_vbat(sc, &volt_mv);
    if (!ret) {
        *volt = volt_mv * 1000;
    }

    return ret;
}

static int sc8960x_set_constant_voltage(struct charger_device *chg_dev, u32 volt)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s: charge volt = %dmV\n", __func__, volt);

    return sc8960x_set_vbat(sc, volt / 1000);
}

static int sc8960x_kick_wdt(struct charger_device *chg_dev)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s\n", __func__);
    return sc8960x_reset_wdt(sc);
}

static int sc8960x_set_ivl(struct charger_device *chg_dev, u32 volt)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s: vindpm volt = %d\n", __func__, volt);

    return sc8960x_set_vindpm(sc, volt / 1000);
}

static int sc8960x_is_charging_done(struct charger_device *chg_dev, bool *done)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int ret;

    ret = sc8960x_check_charge_done(sc, done);

    dev_info(sc->dev, "%s: charge %s done\n", __func__, *done ? "is" : "not");
    return ret;
}

static int sc8960x_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    *curr = 60 * 1000;
    dev_err(sc->dev, "%s\n", __func__);
    return 0;
}

static int sc8960x_dump_registers(struct charger_device *chg_dev)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s\n", __func__);

    return sc8960x_dump_register(sc);
}

static int sc8960x_send_ta_current_pattern(struct charger_device *chg_dev,
        bool is_increase)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int ret;
    int i;

    dev_info(sc->dev, "%s: %s\n", __func__, is_increase ?
            "pumpx up" : "pumpx dn");
    //pumpx start
    ret = sc8960x_set_iindpm(sc, 100);
    if (ret)
        return ret;
    msleep(10);

    for (i = 0; i < 6; i++) {
        if (i < 3) {
            sc8960x_set_iindpm(sc, 800);
            is_increase ? msleep(100) : msleep(300);
        } else {
            sc8960x_set_iindpm(sc, 800);
            is_increase ? msleep(300) : msleep(100);
        }
        sc8960x_set_iindpm(sc, 100);
        msleep(100);
    }

    //pumpx stop
    sc8960x_set_iindpm(sc, 800);
    msleep(500);
    //pumpx wdt, max 240ms
    sc8960x_set_iindpm(sc, 100);
    msleep(100);

    return sc8960x_set_iindpm(sc, 1500);
}

static int sc8960x_send_ta20_current_pattern(struct charger_device *chg_dev,
        u32 uV)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    u8 val = 0;
    int i;

    if (uV < 5500000) {
        uV = 5500000;
    } else if (uV > 15000000) {
        uV = 15000000;
    }

    val = (uV - 5500000) / 500000;

    dev_info(sc->dev, "%s ta20 vol=%duV, val=%d\n", __func__, uV, val);

    sc8960x_set_iindpm(sc, 100);
    msleep(150);
    for (i = 4; i >= 0; i--) {
        sc8960x_set_iindpm(sc, 800);
        (val & (1 << i)) ? msleep(100) : msleep(50);
        sc8960x_set_iindpm(sc, 100);
        (val & (1 << i)) ? msleep(50) : msleep(100);
    }
    sc8960x_set_iindpm(sc, 800);
    msleep(150);
    sc8960x_set_iindpm(sc, 100);
    msleep(240);

    return sc8960x_set_iindpm(sc, 800);
}

static int sc8960x_set_ta20_reset(struct charger_device *chg_dev)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int curr;
    int ret;

    ret = sc8960x_get_iindpm(sc, &curr);

    ret = sc8960x_set_iindpm(sc, 100);
    msleep(300);
    return sc8960x_set_iindpm(sc, curr);
}

static int sc8960x_get_adc(struct charger_device *chg_dev, enum adc_channel chan,
            int *min, int *max)
{
    switch (chan) {
    case ADC_CHANNEL_VSYS:
        *min = 4500 * 1000;
        *max = 4500 * 1000;
        return 0;
        break;

    default:
        break;
    }

    return -95;
}

/*Tab A9 code for SR-AX6739A-01-487 by qiaodan at 20230515 start*/
static int sc8960x_enable_vbus(struct regulator_dev *rdev)
{
    struct sc8960x_chip *sc = charger_get_data(s_chg_dev_otg);
    int ret = 0;

    pr_notice("%s enter\n", __func__);

    /*we should ensure that the powerpath is enabled before enable OTG*/
    ret = sc8960x_set_hiz(sc, false);
    if (ret) {
        pr_err("%s exit hiz failed\n" ,__func__);
    }

    ret |= sc8960x_set_otg_enable(sc, true);
    ret |= sc8960x_set_chg_enable(sc, false);

    return ret;
}
/*Tab A9 code for SR-AX6739A-01-487 by qiaodan at 20230515 end*/

static int sc8960x_disable_vbus(struct regulator_dev *rdev)
{
    struct sc8960x_chip *sc = charger_get_data(s_chg_dev_otg);
    int ret = 0;

    pr_notice("%s enter\n", __func__);

    ret = sc8960x_set_otg_enable(sc, false);
    ret |= sc8960x_set_chg_enable(sc, true);

    return ret;
}

static int sc8960x_is_enabled_vbus(struct regulator_dev *rdev)
{
    struct sc8960x_chip *sc = charger_get_data(s_chg_dev_otg);
    bool otg_en  = false;

    pr_notice("%s enter\n", __func__);

    sc8960x_check_otg_enabled(sc, &otg_en);

    return otg_en? 1 : 0;
}

static int sc8960x_set_otg(struct charger_device *chg_dev, bool enable)
{
    int ret;
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    ret = sc8960x_set_otg_enable(sc, enable);
    ret |= sc8960x_set_chg_enable(sc, !enable);

    dev_info(sc->dev, "%s OTG %s\n", enable ? "enable" : "disable",
        !ret ? "successfully" : "failed");

    return ret;
}

static int sc8960x_set_safety_timer(struct charger_device *chg_dev, bool enable)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s  %s\n", __func__, enable ? "enable" : "disable");

    return sc8960x_set_safet_timer(sc, enable);
}

static int sc8960x_is_safety_timer_enabled(struct charger_device *chg_dev,
                    bool *enabled)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    return sc8960x_check_safet_timer(sc, enabled);
}

static int sc8960x_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s otg curr = %d\n", __func__, curr);
    return sc8960x_set_iboost(sc, curr / 1000);
}

static int sc8960x_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    dev_info(sc->dev, "%s\n", __func__);

    /*Tab A9 code for SR-AX6739A-01-531 by hualei at 20230523 start*/
    switch (event) {
        case EVENT_FULL:
        case EVENT_RECHARGE:
        case EVENT_DISCHARGE:
            power_supply_changed(sc->psy);
            break;
        default:
            break;
    }
    /*Tab A9 code for SR-AX6739A-01-531 by hualei at 20230523 end*/

    return 0;
}

static int sc8960x_enable_hz(struct charger_device *chg_dev, bool enable)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s %s\n", __func__, enable ? "enable" : "disable");

    return sc8960x_set_hiz(sc, enable);
}

/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 start*/
static int sc8960x_enable_port_charging(struct charger_device *chg_dev, bool enable)
{
    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int ret = 0;

    dev_info(sc->dev, "%s value = %d\n", __func__, enable);

    ret = sc8960x_set_hiz(sc, !enable);

    return ret;
}


static int sc8960x_is_port_charging_enabled(struct charger_device *chg_dev, bool *enable)
{
    struct sc8960x_chip *sc = charger_get_data(chg_dev);
    int ret = 0;
    bool hiz_enable = false;

    dev_info(sc->dev, "%s enter\n", __func__);

    ret = sc8960x_get_hiz_status(sc, &hiz_enable);
    *enable = !hiz_enable;

    return ret;
}
/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 end*/

/*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 start*/
static int sc8960x_disable_battfet_rst(struct sc8960x_chip *sc)
{
    int ret = 0;
    u8 val = 0;
    ret = sc8960x_field_write(sc, BATFET_RST_EN, val);

    pr_err("disable BATTFET_RST_EN %s\n", !ret ? "successfully" : "failed");

    return ret;
}

int sc8960x_set_shipmode(struct charger_device *chg_dev, bool en)
{
    int ret = 0;

    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    sc->shipmode_wr_value = 0;

    if (en) {
        sc8960x_set_hiz(sc, en);
        ret += sc8960x_field_write(sc, BATFET_DLY, en);
        ret += sc8960x_field_write(sc, BATFET_DIS, en);
    } else {
        ret = sc8960x_field_write(sc, BATFET_DIS, en);
    }

    if (!ret) {
        sc->shipmode_wr_value = true;
    }

     pr_err("%s shipmode %s\n", en ? "enable" : "disable",
            !ret ? "successfully" : "failed");

     return ret;
}

int sc8960x_get_shipmode(struct charger_device *chg_dev)
{
    int ret = 0;

    struct sc8960x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    ret = sc->shipmode_wr_value;
    pr_err("%s:shipmode_wr %s\n",__func__, ret ? "enabled" : "disabled");

    return ret;
}
/*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 end*/

/*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
static int sc8960x_get_hvdcp_status(struct charger_device *chg_dev)
{
    int ret = 0;
    struct sc8960x_chip *sc = charger_get_data(chg_dev);

    if (sc == NULL) {
        pr_info("[%s] sc8960x_get_hvdcp_status fail\n", __func__);
        return ret;
    }

    ret = (sc->hvdcp_done ? 1 : 0);

    return ret;
}
/*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/

static struct regulator_ops sc8960x_vbus_ops = {
    .enable = sc8960x_enable_vbus,
    .disable = sc8960x_disable_vbus,
    .is_enabled = sc8960x_is_enabled_vbus,
};

static const struct regulator_desc sc8960x_otg_rdesc = {
    .of_match = "usb-otg-vbus",
    .name = "usb-otg-vbus",
    .ops = &sc8960x_vbus_ops,
    .owner = THIS_MODULE,
    .type = REGULATOR_VOLTAGE,
    .fixed_uV = 5000000,
    .n_voltages = 1,
};

static int sc8960x_vbus_regulator_register(struct sc8960x_chip *sc)
{
    struct regulator_config config = {};
    int ret = 0;
    /* otg regulator */
    config.dev = sc->dev;
    config.driver_data = sc;
    sc->otg_rdev = devm_regulator_register(sc->dev,
                        &sc8960x_otg_rdesc, &config);
    sc->otg_rdev->constraints->valid_ops_mask |= REGULATOR_CHANGE_STATUS;
    if (IS_ERR(sc->otg_rdev)) {
        ret = PTR_ERR(sc->otg_rdev);
        pr_info("%s: register otg regulator failed (%d)\n", __func__, ret);
    }
    return ret;
}

static struct charger_ops sc8960x_chg_ops = {
    /* Normal charging */
    .plug_in = sc8960x_plug_in,
    .plug_out = sc8960x_plug_out,
    .enable = sc8960x_enable,
    .is_enabled = sc8960x_is_enabled,
    .get_charging_current = sc8960x_get_charging_current,
    .set_charging_current = sc8960x_set_charging_current,
    .get_input_current = sc8960x_get_input_current,
    /*Tab A9 code for SR-AX6739A-01-503 by qiaodan at 20230510 start*/
    .get_min_input_current  = sc8960x_get_min_input_current,
    /*Tab A9 code for SR-AX6739A-01-503 by qiaodan at 20230510 end*/
    .set_input_current = sc8960x_set_input_current,
    .get_constant_voltage = sc8960x_get_constant_voltage,
    .set_constant_voltage = sc8960x_set_constant_voltage,
    .kick_wdt = sc8960x_kick_wdt,
    .set_mivr = sc8960x_set_ivl,
    .is_charging_done = sc8960x_is_charging_done,
    .get_min_charging_current = sc8960x_get_min_ichg,
    .dump_registers = sc8960x_dump_registers,

    /* Safety timer */
    .enable_safety_timer = sc8960x_set_safety_timer,
    .is_safety_timer_enabled = sc8960x_is_safety_timer_enabled,

    /* Power path */
    .enable_powerpath = NULL,
    .is_powerpath_enabled = NULL,
    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 start*/
    .enable_port_charging = sc8960x_enable_port_charging,
    .is_port_charging_enabled= sc8960x_is_port_charging_enabled,
    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 end*/

    /* OTG */
    .enable_otg = sc8960x_set_otg,
    .set_boost_current_limit = sc8960x_set_boost_ilmt,
    .enable_discharge = NULL,

    /* PE+/PE+20 */
    .send_ta_current_pattern = sc8960x_send_ta_current_pattern,
    .set_pe20_efficiency_table = NULL,
    .send_ta20_current_pattern = sc8960x_send_ta20_current_pattern,
    .reset_ta = sc8960x_set_ta20_reset,
    .enable_cable_drop_comp = NULL,

    /* ADC */
    .get_adc = sc8960x_get_adc,

    /*Tab A9 code for SR-AX6739A-01-531 by hualei at 20230523 start*/
    .event = sc8960x_do_event,
    /*Tab A9 code for SR-AX6739A-01-531 by hualei at 20230523 end*/
    .enable_hz = sc8960x_enable_hz,

    /*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 start*/
    .get_ship_mode = sc8960x_get_shipmode,
    .set_ship_mode = sc8960x_set_shipmode,
    /*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 end*/
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
    .get_hvdcp_status = sc8960x_get_hvdcp_status,
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/
};

static const struct charger_properties sc8960x_chg_props = {
    .alias_name = "sc89601x_chg",
};
#endif /*CONFIG_MTK_CLASS*/

/**********************interrupt*********************/
/*Tab A9 code for AX6739A-516 | OT11BSP-42 by qiaodan at 20230613 start*/
static int sc8960x_chg_attach_pre_process(struct sc8960x_chip *sc, int attach)
{
    sc->typec_attached = attach;
    switch(attach) {
        case ATTACH_TYPE_TYPEC:
            /*Tab A9 code for AX6739A-2077 by qiaodan at 20230707 start*/
            #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
            schedule_delayed_work(&sc->force_detect_dwork, msecs_to_jiffies(150));
            #else
            schedule_delayed_work(&sc->force_detect_dwork, msecs_to_jiffies(220));
            #endif // !CONFIG_ODM_CUSTOM_FACTORY_BUILD
            /*Tab A9 code for AX6739A-2077 by qiaodan at 20230707 end*/
            break;
        case ATTACH_TYPE_PD_SDP:
            sc->chg_type = POWER_SUPPLY_TYPE_USB;
            sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
            break;
        case ATTACH_TYPE_PD_DCP:
            sc->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
            sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
            break;
        case ATTACH_TYPE_PD_NONSTD:
            sc->chg_type = POWER_SUPPLY_TYPE_USB;
            sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
            break;
        case ATTACH_TYPE_NONE:
            sc->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
            sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
            break;
        default:
            dev_info(sc->dev, "%s: using tradtional bc12 flow!\n", __func__);
            break;
    }

    power_supply_changed(sc->psy);
    dev_err(sc->dev, "%s: type(%d %d),attach:%d\n", __func__,
        sc->chg_type, sc->psy_usb_type, attach);

    return 0;
}
/*Tab A9 code for AX6739A-516 | OT11BSP-42 by qiaodan at 20230613 end*/

static void sc8960x_force_detection_dwork_handler(struct work_struct *work)
{
    int ret;
    struct sc8960x_chip *sc = container_of(work, struct sc8960x_chip, force_detect_dwork.work);

    //Charger_Detect_Init();
    /*Tab A9 code for AX6739A-2077 by qiaodan at 20230707 start*/
    #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
    if ((sc->vbus_good == false) && (sc->typec_attached == ATTACH_TYPE_TYPEC)) {
        dev_err(sc->dev, "%s: wait vbus to be good\n", __func__);
        schedule_delayed_work(&sc->force_detect_dwork, msecs_to_jiffies(10));
        return;
    } else if (sc->vbus_good == false) {
        dev_err(sc->dev, "%s: TypeC has been plug out\n", __func__);
        return;
    }

    msleep(100);
    #endif // !CONFIG_ODM_CUSTOM_FACTORY_BUILD
    /*Tab A9 code for AX6739A-2077 by qiaodan at 20230707 end*/
    ret = sc8960x_force_dpdm(sc);
    if (ret) {
        dev_err(sc->dev, "%s: force dpdm failed(%d)\n", __func__, ret);
        return;
    }

    sc->force_detect_count++;
}

static int sc8960x_get_charger_type(struct sc8960x_chip *sc)
{
    /*Tab A9 code for OT11BSP-42 by qiaodan at 20230613 start*/
    int ret = 0;
    int reg_val = 0;

    if (sc->typec_attached > ATTACH_TYPE_NONE && sc->vbus_good) {
        switch(sc->typec_attached) {
            case ATTACH_TYPE_PD_SDP:
            case ATTACH_TYPE_PD_DCP:
            case ATTACH_TYPE_PD_NONSTD:
                dev_info(sc->dev, "%s: Attach PD_TYPE, skip bc12 flow!\n", __func__);
                return ret;
            default:
                break;
        }
    }
    /*Tab A9 code for OT11BSP-42 by qiaodan at 20230613 end*/

    ret = sc8960x_field_read(sc, VBUS_STAT, &reg_val);
    if (ret) {
        return ret;
    }

    switch (reg_val) {
    case VBUS_STAT_NO_INPUT:
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
        sc->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
        break;
    case VBUS_STAT_SDP:
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
        sc->chg_type = POWER_SUPPLY_TYPE_USB;
        //Charger_detect_Release();
        break;
    case VBUS_STAT_CDP:
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
        sc->chg_type = POWER_SUPPLY_TYPE_USB_CDP;
        //Charger_detect_Release();
        break;
    case VBUS_STAT_DCP:
    case VBUS_STAT_HVDCP:
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
        sc->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
        sc8960x_soft_enable_hvdcp(sc);
        break;
    case VBUS_STAT_UNKOWN:
        /*Tab A9 code for SR-AX6739A-01-482 by qiaodan at 20230516 start*/
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
        /*Tab A9 code for SR-AX6739A-01-482 by qiaodan at 20230516 end*/
        sc->chg_type = POWER_SUPPLY_TYPE_USB;
        if (sc->force_detect_count < 10) {
            schedule_delayed_work(&sc->force_detect_dwork, msecs_to_jiffies(100));
        }
        break;
    case VBUS_STAT_NONSTAND:
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
        sc->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
        break;
    default:
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
        sc->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
        break;
    }

    dev_info(sc->dev, "%s vbus stat: 0x%02x\n", __func__, reg_val);
    /*Tab A9 code for AX6739A-1484 by hualei at 20230628 start*/
    power_supply_changed(sc->psy);
    /*Tab A9 code for AX6739A-1484 by hualei at 20230628 end*/

    return ret;
}

static irqreturn_t sc8960x_irq_handler(int irq, void *data)
{
    int ret;
    int reg_val;
    bool prev_vbus_gd;

    struct sc8960x_chip *sc = (struct sc8960x_chip *)data;

    dev_info(sc->dev, "%s: sc8960x_irq_handler\n", __func__);

    ret = sc8960x_field_read(sc, VBUS_GD, &reg_val);
    if (ret) {
        return IRQ_HANDLED;
    }
    prev_vbus_gd = sc->vbus_good;
    sc->vbus_good = !!reg_val;

    if (!prev_vbus_gd && sc->vbus_good) {
        sc->force_detect_count = 0;
        dev_info(sc->dev, "%s: adapter/usb inserted\n", __func__);
        sc->chg_config = true;
        /*Tab A9 code for SR-AX6739A-01-486 by wenyaqi at 20230619 start*/
        if (sc->hiz_force_dpdm == true && sc->chg_type != POWER_SUPPLY_TYPE_UNKNOWN) {
            msleep(100);
            sc8960x_force_dpdm(sc);
        }
        /*Tab A9 code for SR-AX6739A-01-486 by wenyaqi at 20230619 end*/
    } else if (prev_vbus_gd && !sc->vbus_good) {
        dev_info(sc->dev, "%s: adapter/usb removed\n", __func__);
        /*Tab A9 code for AX6739A-527 by hualei at 20230609 start*/
        sc8960x_set_ichg(sc, SC8960_ICHG_LIM);
        /*Tab A9 code for AX6739A-527 by hualei at 20230609 end*/
        /*Tab A9 code for AX6739A-516 by wenyaqi at 20230603 start*/
        // sc->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
        /*Tab A9 code for AX6739A-516 by wenyaqi at 20230603 end*/
        sc->chg_config = false;
        /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
        cancel_delayed_work_sync(&sc->hvdcp_done_work);
        /*Tab A9 code for OT11BSP-42 by qiaodan at 20230613 start*/
        cancel_delayed_work_sync(&sc->force_detect_dwork);
        /*Tab A9 code for OT11BSP-42 by qiaodan at 20230613 end*/
        sc->hvdcp_done = false;
        /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/
    }

    /*Tab A9 code for OT11BSP-42 | P230612-08698 | SR-AX6739A-01-486 by wenyaqi at 20230619 start*/
    if (sc->vbus_good == true && sc->chg_type == POWER_SUPPLY_TYPE_UNKNOWN) {
        sc8960x_get_charger_type(sc);
    } else if (prev_vbus_gd && sc->vbus_good && sc->hiz_force_dpdm == true) {
        sc->hiz_force_dpdm = false;
        sc8960x_get_charger_type(sc);
    }
    /*Tab A9 code for OT11BSP-42 | P230612-08698 | SR-AX6739A-01-486 by wenyaqi at 20230619 end*/

    sc8960x_dump_register(sc);
    return IRQ_HANDLED;
}

/*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
static void hvdcp_done_work_func(struct work_struct *work)
{
    struct delayed_work *hvdcp_done_work = NULL;
    struct sc8960x_chip *sc = NULL;

    hvdcp_done_work = container_of(work, struct delayed_work, work);
    if (hvdcp_done_work == NULL) {
        pr_err("Cann't get hvdcp_done_work\n");
        return;
    }

    sc = container_of(hvdcp_done_work, struct sc8960x_chip,
                      hvdcp_done_work);
    if (sc == NULL) {
        pr_err("Cann't get sc8960x_chip\n");
        return;
    }

    sc->hvdcp_done = true;
    power_supply_changed(sc->psy);
    pr_info("%s HVDCP end\n", __func__);
}
/*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/

/**********************system*********************/
static int sc8960x_parse_dt(struct sc8960x_chip *sc)
{
    struct device_node *np = sc->client->dev.of_node;
    int ret, i;
    const struct {
        const char *name;
        u32 *val;
    } param_data[] = {
        {"sc8960x,pfm-dis",           &(sc->sc8960x_param.pfm_dis)},
        {"sc8960x,vsys-min",          &(sc->sc8960x_param.vsys_min)},
        {"sc8960x,vbat-low-otg",      &(sc->sc8960x_param.vbat_low_otg)},
        {"sc8960x,ipre-chg",          &(sc->sc8960x_param.pre_chg_curr)},
        {"sc8960x,itermination",      &(sc->sc8960x_param.term_chg_curr)},
        {"sc8960x,vbat-volt",         &(sc->sc8960x_param.vbat_cv)},
        {"sc8960x,top-off-timer",     &(sc->sc8960x_param.top_off_timer)},
        {"sc8960x,vrechg-volt",       &(sc->sc8960x_param.vrechg_volt)},
        {"sc8960x,en-termination",    &(sc->sc8960x_param.term_en)},
        {"sc8960x,wdt-timer",         &(sc->sc8960x_param.wdt_timer)},
        {"sc8960x,en-chg-saft-timer", &(sc->sc8960x_param.en_chg_saft_timer)},
        {"sc8960x,charge-timer",      &(sc->sc8960x_param.charge_timer)},
        {"sc8960x,vac-ovp",           &(sc->sc8960x_param.vac_ovp)},
        {"sc8960x,iboost",            &(sc->sc8960x_param.iboost)},
        {"sc8960x,vboost",            &(sc->sc8960x_param.vboost)},
        {"sc8960x,vindpm-track",      &(sc->sc8960x_param.vindpm_track)},
        {"sc8960x,vindpm-int-mask",   &(sc->sc8960x_param.vindpm_int_mask)},
        {"sc8960x,iindpm-int-mask",   &(sc->sc8960x_param.iindpm_int_mask)},
        {"sc8960x,vpre-to-cc",        &(sc->sc8960x_param.vpre_to_cc)},
        {"sc8960x,auto-dpdm-en",      &(sc->sc8960x_param.auto_dpdm_en)},
        {"sc8960x,ntc-pin-dis",       &(sc->sc8960x_param.ntc_pin_dis)},
    };
    if(of_property_read_string(np, "sc8960x,charger_name", &sc->chg_dev_name)) {
        sc->chg_dev_name = "primary_chg";
        dev_err(sc->dev, "no charger name\n");
    }

    for (i = 0;i < ARRAY_SIZE(param_data);i++) {
        ret = of_property_read_u32(np, param_data[i].name, param_data[i].val);
        if (ret < 0) {
            dev_err(sc->dev, "not find property %s\n", param_data[i].name);
            return ret;
        } else {
            dev_info(sc->dev,"%s: %d\n", param_data[i].name,
                        (int)*param_data[i].val);
        }
    }

    sc->irq_gpio = of_get_named_gpio(np, "sc8960x,intr_gpio", 0);
    if (!gpio_is_valid(sc->irq_gpio)) {
        dev_err(sc->dev, "fail to valid gpio : %d\n", sc->irq_gpio);
        return -EINVAL;
    }

    return ret;
}
/*Tab A9 code for P230714-02020 by lina at 20230731 start*/
static int sc8960x_set_otg_uvp(struct sc8960x_chip *sc)
{
    int ret = 0;
    int reg_val = 0;

    ret = regmap_read(sc->regmap, SC8960X_REG_93, &reg_val);
    if (ret) {
        sc8960x_set_key(sc);
    }
    ret = regmap_read(sc->regmap, SC8960X_REG_93, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read reg SC8960X_REG_93 failed(%d)\n", ret);
        return ret;
    }
    //Set UVP threshold vbat-200mV
    ret = regmap_write(sc->regmap, SC8960X_REG_93, reg_val | 0x04);
    ret = regmap_read(sc->regmap, SC8960X_REG_93, &reg_val);
    dev_err(sc->dev, "read reg SC8960X_REG_93 0x%x\n", reg_val);

    ret = regmap_read(sc->regmap, SC8960X_REG_94, &reg_val);
    //Set the shaking time 15us
    ret = regmap_write(sc->regmap, SC8960X_REG_94, reg_val | 0x80);
    ret = regmap_read(sc->regmap, SC8960X_REG_94, &reg_val);
    dev_err(sc->dev, "read reg SC8960X_REG_94 0x%x\n", reg_val);
    return sc8960x_set_key(sc);
}
/*Tab A9 code for P230714-02020 by lina at 20230731 end*/
static int sc8960x_hw_init(struct sc8960x_chip *sc)
{
    int ret, i = 0;
    const struct {
        enum sc8960x_fields field;
        int val;
    } param_init[] = {
        {PFM_DIS,         sc->sc8960x_param.pfm_dis},
        {VSYS_MIN,        sc->sc8960x_param.vsys_min},
        {VBATLOW_OTG,     sc->sc8960x_param.vbat_low_otg},
        {ITC,             sc->sc8960x_param.pre_chg_curr},
        {ITERM,           sc->sc8960x_param.term_chg_curr},
        {TOP_OFF_TIMER,   sc->sc8960x_param.top_off_timer},
        {VRECHG,          sc->sc8960x_param.vrechg_volt},
        {EN_TERM,         sc->sc8960x_param.term_en},
        {TWD,             sc->sc8960x_param.wdt_timer},
        {EN_TIMER,        sc->sc8960x_param.en_chg_saft_timer},
        {CHG_TIMER,       sc->sc8960x_param.charge_timer},
        {VAC_OVP,         sc->sc8960x_param.vac_ovp},
        {BOOST_LIM,       sc->sc8960x_param.iboost},
        {VINDPM_TRACK,    sc->sc8960x_param.vindpm_track},
        {VINDPM_INT_MASK, sc->sc8960x_param.vindpm_int_mask},
        {IINDPM_INT_MASK, sc->sc8960x_param.iindpm_int_mask},
        {VTC,             sc->sc8960x_param.vpre_to_cc},
        {AUTO_DPDM_EN,    sc->sc8960x_param.auto_dpdm_en},
        {NTC_DIS,         sc->sc8960x_param.ntc_pin_dis},
    };

    for (i = 0;i < ARRAY_SIZE(param_init);i++) {
        ret = sc8960x_field_write(sc, param_init[i].field, param_init[i].val);
        if (ret) {
            dev_err(sc->dev, "write field %d failed\n", param_init[i].field);
            return ret;
        }
    }

    //set vbat cv
    ret = sc8960x_field_write(sc, VBAT_REG, sc->sc8960x_param.vbat_cv >> 2);
    ret |= sc8960x_field_write(sc, VBAT_REG_FT, sc->sc8960x_param.vbat_cv & 0x03);
    if (ret) {
        dev_err(sc->dev, "set vbat cv failed\n");
        return ret;
    }

    //set vboost
    ret = sc8960x_field_write(sc, BOOSTV0, (sc->sc8960x_param.vboost & 0x06) >> 1);
    ret |= sc8960x_field_write(sc, BOOSTV1,
        ((sc->sc8960x_param.vboost & 0x08) >> 2) | (sc->sc8960x_param.vboost & 0x01));
    if (ret) {
        dev_err(sc->dev, "set vboost failed\n");
        return ret;
    }

    ret = sc8960x_set_wa(sc);
    if (ret) {
       dev_err(sc->dev, "set private failed\n");
    }
    ret = sc8960x_enable_hvdcp(sc, false);
    if (ret) {
       dev_err(sc->dev, "set hvdcp failed\n");
    }

    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 start*/
    sc8960x_set_hiz(sc, false);
    if (ret) {
        dev_err(sc->dev, "exit hiz failed\n");
    }
    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 end*/

    /*Tab A9 code for AX6739A-399 by shanxinkai at 20230530 start*/
    ret = sc8960x_set_safet_timer(sc, false);
    if (ret) {
        dev_err(sc->dev, "Failed to set safety_timer stop\n");
    }
    /*Tab A9 code for AX6739A-399 by shanxinkai at 20230530 end*/
    /*Tab A9 code for P230714-02020 by lina at 20230731 start*/
    sc8960x_set_otg_uvp(sc);
    /*Tab A9 code for P230714-02020 by lina at 20230731 end*/
    return ret;
}

static int sc8960x_irq_register(struct sc8960x_chip *sc)
{
    int ret;

    ret = gpio_request_one(sc->irq_gpio, GPIOF_DIR_IN, "sc8960x_irq");
    if (ret) {
        dev_err(sc->dev, "failed to request %d\n", sc->irq_gpio);
        return -EINVAL;
    }

    sc->irq = gpio_to_irq(sc->irq_gpio);
    if (sc->irq < 0) {
        dev_err(sc->dev, "failed to gpio_to_irq\n");
        return -EINVAL;
    }

    ret = devm_request_threaded_irq(sc->dev, sc->irq, NULL,
                    sc8960x_irq_handler,
                    IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                    "sc_irq", sc);
    if (ret < 0) {
        dev_err(sc->dev, "request thread irq failed:%d\n", ret);
        return ret;
    }

    enable_irq_wake(sc->irq);
    device_init_wakeup(sc->dev, true);

    return ret;
}

static ssize_t sc8960x_show_registers(struct device *dev,
            struct device_attribute *attr,
            char *buf)
{
    struct sc8960x_chip *sc = dev_get_drvdata(dev);
    u8 tmpbuf[256];
    int addr, val, len, ret, idx = 0;

    idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc8960x Reg");
    for (addr = 0x0; addr <= SC8960X_REGMAX; addr++) {
        ret = regmap_read(sc->regmap, addr, &val);
        if (ret == 0) {
            len = snprintf(tmpbuf, PAGE_SIZE - idx,
                    "Reg[%.2x] = 0x%.2x\n", addr, val);
            memcpy(&buf[idx], tmpbuf, len);
            idx += len;
        }
    }

    return idx;
}

static ssize_t sc8960x_store_registers(struct device *dev,
            struct device_attribute *attr,
            const char *buf,
            size_t count)
{
    struct sc8960x_chip *sc = dev_get_drvdata(dev);
    int ret;
    int reg, val;

    ret = sscanf(buf, "%x %x", &reg, &val);
    if (ret == 2 && reg < SC8960X_REGMAX) {
        regmap_write(sc->regmap, reg, val);
    }

    return count;
}

static DEVICE_ATTR(registers, 0660, sc8960x_show_registers, sc8960x_store_registers);

static void sc8960x_create_device_node(struct device *dev)
{
    device_create_file(dev, &dev_attr_registers);
}

static enum power_supply_usb_type sc8960x_chg_psy_usb_types[] = {
    POWER_SUPPLY_USB_TYPE_UNKNOWN,
    POWER_SUPPLY_USB_TYPE_SDP,
    POWER_SUPPLY_USB_TYPE_CDP,
    POWER_SUPPLY_USB_TYPE_DCP,
};

static enum power_supply_property sc8960x_chg_psy_properties[] = {
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

static int sc8960x_chg_property_is_writeable(struct power_supply *psy,
                        enum power_supply_property psp)
{
    switch (psp) {
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

static int sc8960x_chg_get_property(struct power_supply *psy,
                enum power_supply_property psp,
                union power_supply_propval *val)
{
    int ret = 0;
    int data = 0;
    struct sc8960x_chip *sc = power_supply_get_drvdata(psy);

    switch (psp) {
    case POWER_SUPPLY_PROP_MANUFACTURER:
        val->strval = "SouthChip";
        break;
    case POWER_SUPPLY_PROP_ONLINE:
        /*Tab A9 code for AX6739A-516 | P230609-02043 | OT11BSP-42 by qiaodan at 20230613 start*/
        val->intval = sc->vbus_good | (!!sc->typec_attached);
        if (sc->chg_type == POWER_SUPPLY_TYPE_UNKNOWN) {
            val->intval = 0;
        }
        /*Tab A9 code for AX6739A-516 | P230609-02043 | OT11BSP-42 by qiaodan at 20230613 end*/
        break;
    case POWER_SUPPLY_PROP_STATUS:
        ret = sc8960x_get_charge_stat(sc);
        if (ret < 0)
            break;
        val->intval = ret;
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        ret = sc8960x_get_ichg(sc, &data);
        if (ret)
            break;
        val->intval = data * 1000;
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
        ret = sc8960x_get_vbat(sc, &data);
        if (ret < 0)
            break;
        /*Tab A9 code for AX6739A-765 by hualei at 20230612 start*/
        val->intval = data;
        /*Tab A9 code for AX6739A-765 by hualei at 20230612 end*/
        break;
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        ret = sc8960x_get_iindpm(sc, &data);
        if (ret < 0)
            break;
        val->intval = data * 1000;
        break;
    case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
        ret = sc8960x_get_vindpm(sc, &data);
        if (ret < 0)
            break;
        val->intval = data * 1000;
        break;
    case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
        ret = sc8960x_get_term_curr(sc, &data);
        if (ret < 0)
            break;
        val->intval = data * 1000;
        break;
    case POWER_SUPPLY_PROP_USB_TYPE:
        val->intval = sc->psy_usb_type;
        break;
    case POWER_SUPPLY_PROP_CURRENT_MAX:
        if (sc->chg_type == POWER_SUPPLY_TYPE_USB)
            val->intval = 500000;
        else
            val->intval = 1500000;
        break;
    case POWER_SUPPLY_PROP_VOLTAGE_MAX:
        if (sc->chg_type == POWER_SUPPLY_TYPE_USB)
            val->intval = 5000000;
        else
            val->intval = 12000000;
        break;
    case POWER_SUPPLY_PROP_TYPE:
        val->intval = sc->chg_type;
        break;
    default:
        ret = -EINVAL;
        break;
    }
    return ret;
}

static int sc8960x_chg_set_property(struct power_supply *psy,
                enum power_supply_property psp,
                const union power_supply_propval *val)
{
    int ret = 0;
    struct sc8960x_chip *sc = power_supply_get_drvdata(psy);

    switch (psp) {
    case POWER_SUPPLY_PROP_ONLINE:
        dev_info(sc->dev, "%s  %d\n", __func__, val->intval);
        /*Tab A9 code for AX6739A-516 by wenyaqi at 20230603 start*/
        ret = sc8960x_chg_attach_pre_process(sc, val->intval);
        /*Tab A9 code for AX6739A-516 by wenyaqi at 20230603 end*/
        break;
    case POWER_SUPPLY_PROP_STATUS:
        ret = sc8960x_set_chg_enable(sc, !!val->intval);
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        ret = sc8960x_set_ichg(sc, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
        ret = sc8960x_set_vbat(sc, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        ret = sc8960x_set_iindpm(sc, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
        ret = sc8960x_set_vindpm(sc, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
        ret = sc8960x_set_term_curr(sc, val->intval / 1000);
        break;
    default:
        ret = -EINVAL;
        break;
    }
    return ret;
}

static char *sc8960x_psy_supplied_to[] = {
    "battery",
    "mtk-master-charger",
};

static const struct power_supply_desc sc8960x_psy_desc = {
    .name = "charger",
    /*Tab A9 code for SR-AX6739A-01-493 by wenyaqi at 20230503 start*/
    // avoid healthd chg=au
    .type = POWER_SUPPLY_TYPE_UNKNOWN,
    /*Tab A9 code for SR-AX6739A-01-493 by wenyaqi at 20230503 end*/
    .usb_types = sc8960x_chg_psy_usb_types,
    .num_usb_types = ARRAY_SIZE(sc8960x_chg_psy_usb_types),
    .properties = sc8960x_chg_psy_properties,
    .num_properties = ARRAY_SIZE(sc8960x_chg_psy_properties),
    .property_is_writeable = sc8960x_chg_property_is_writeable,
    .get_property = sc8960x_chg_get_property,
    .set_property = sc8960x_chg_set_property,
};

static int sc8960x_psy_register(struct sc8960x_chip *sc)
{
    struct power_supply_config cfg = {
        .drv_data = sc,
        .of_node = sc->dev->of_node,
        .supplied_to = sc8960x_psy_supplied_to,
        .num_supplicants = ARRAY_SIZE(sc8960x_psy_supplied_to),
    };

    memcpy(&sc->psy_desc, &sc8960x_psy_desc, sizeof(sc->psy_desc));
    sc->psy = devm_power_supply_register(sc->dev, &sc->psy_desc,
                        &cfg);
    return IS_ERR(sc->psy) ? PTR_ERR(sc->psy) : 0;
}

/*Tab A9 code for SR-AX6739A-01-493 by wenyaqi at 20230505 start*/
static struct of_device_id sc8960x_charger_match_table[] = {
    { .compatible = "sc,sc89601d", },
    { },
};
MODULE_DEVICE_TABLE(of, sc8960x_charger_match_table);
/*Tab A9 code for SR-AX6739A-01-493 by wenyaqi at 20230505 end*/

static int sc8960x_charger_probe(struct i2c_client *client,
                const struct i2c_device_id *id)
{
    struct sc8960x_chip *sc;
    int i, ret = 0;
    /*Tab A9 code for AX6739A-2077 by qiaodan at 20230707 start*/
    #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
    int reg_val = 0;
    #endif // !CONFIG_ODM_CUSTOM_FACTORY_BUILD
    /*Tab A9 code for AX6739A-2077 by qiaodan at 20230707 end*/

    dev_info(&client->dev, "%s (%s)\n", __func__, SC8960X_DRV_VERSION);

    sc = devm_kzalloc(&client->dev, sizeof(struct sc8960x_chip), GFP_KERNEL);
    if (!sc)
        return -ENOMEM;

    client->addr = 0x6B;
    sc->dev = &client->dev;
    sc->client = client;
    sc->regmap = devm_regmap_init_i2c(client,
                            &sc8960x_regmap_config);
    if (IS_ERR(sc->regmap)) {
        dev_err(sc->dev, "Failed to initialize regmap\n");
        return -EINVAL;
    }

    for (i = 0; i < ARRAY_SIZE(sc8960x_reg_fields); i++) {
        const struct reg_field *reg_fields = sc8960x_reg_fields;
        sc->rmap_fields[i] = devm_regmap_field_alloc(sc->dev,
                                                sc->regmap,
                                                reg_fields[i]);
        if (IS_ERR(sc->rmap_fields[i])) {
            dev_err(sc->dev, "cannot allocate regmap field\n");
            return PTR_ERR(sc->rmap_fields[i]);
        }
    }

    i2c_set_clientdata(client, sc);
    sc8960x_create_device_node(&(client->dev));

    INIT_DELAYED_WORK(&sc->force_detect_dwork, sc8960x_force_detection_dwork_handler);
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
    INIT_DELAYED_WORK(&sc->hvdcp_done_work, hvdcp_done_work_func);
    sc->hvdcp_done = false;
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/
    sc->hiz_force_dpdm = false;
    ret = sc8960x_detect_device(sc);
    if (ret) {
        dev_err(sc->dev, "No sc8960x device found!\n");
        goto err_detect_dev;
    }

    /*Tab A9 code for SR-AX6739A-01-493 by wenyaqi at 20230505 start*/
    if (sc->part_no != SC89601D_ID) {
        dev_err(sc->dev,"not find SC89601D device, part_no = 0x%02x\n", sc->part_no);
        goto err_detect_dev;
    }
    /*Tab A9 code for SR-AX6739A-01-493 by wenyaqi at 20230505 end*/

    /*Tab A9 code for SR-AX6739A-01-489 by wenyaqi at 20230504 start*/
    /* set for chg_info */
    gxy_bat_set_chginfo(GXY_BAT_CHG_INFO_SC89601D);
    /*Tab A9 code for SR-AX6739A-01-489 by wenyaqi at 20230504 end*/

    ret = sc8960x_parse_dt(sc);
    if (ret) {
        dev_err(sc->dev, "%s parse dt failed(%d)\n", __func__, ret);
        goto err_parse_dt;
    }

    sc->chg_config = false;
    ret = sc8960x_hw_init(sc);
    if (ret) {
        dev_err(sc->dev, "%s failed to init device(%d)\n", __func__, ret);
        goto err_init_device;
    }

    ret = sc8960x_psy_register(sc);
    if (ret) {
        dev_err(sc->dev, "%s psy register failed(%d)\n", __func__, ret);
        goto err_psy_register;
    }

    ret = sc8960x_irq_register(sc);
    if (ret) {
        dev_err(sc->dev, "%s irq register failed(%d)\n", __func__, ret);
        goto err_irq_register;
    }

    /*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 start*/
    ret = sc8960x_disable_battfet_rst(sc);
    if (ret) {
        dev_err(sc->dev, "%s battfet rst failed(%d)\n", __func__, ret);
        goto err_irq_register;
    }
    /*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 end*/

#ifdef CONFIG_MTK_CLASS
    sc->chg_dev = charger_device_register(sc->chg_dev_name,
                        &client->dev, sc,
                        &sc8960x_chg_ops,
                        &sc8960x_chg_props);
    if (IS_ERR_OR_NULL(sc->chg_dev)) {
        ret = PTR_ERR(sc->chg_dev);
        devm_kfree(&client->dev, sc);
        return ret;
    }
#endif /*CONFIG_MTK_CLASS*/

    /* otg regulator */
    s_chg_dev_otg=sc->chg_dev;
    sc8960x_vbus_regulator_register(sc);

    /*Tab A9 code for AX6739A-2077 by qiaodan at 20230707 start*/
    #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
    ret = sc8960x_field_read(sc, VBUS_GD, &reg_val);
    if (ret) {
        dev_err(sc->dev, "%s get vbus_gd failed(%d)\n", __func__, ret);
    }
    sc->vbus_good = !!reg_val;
    dev_info(sc->dev, "%s get vbus_good=%d\n", __func__, sc->vbus_good);
    #endif // !CONFIG_ODM_CUSTOM_FACTORY_BUILD
    /*Tab A9 code for AX6739A-2077 by qiaodan at 20230707 end*/

    dev_info(sc->dev, "sc8960x probe successfully, Part Num:%d, Revision:%d\n!",
        sc->part_no, sc->dev_version);

    return ret;

err_irq_register:
err_psy_register:
err_parse_dt:
err_init_device:
err_detect_dev:
    dev_info(sc->dev, "sc8960x probe failed\n");
    devm_kfree(&client->dev, sc);
    return -ENODEV;
}

static int sc8960x_charger_remove(struct i2c_client *client)
{
    struct sc8960x_chip *sc = i2c_get_clientdata(client);

    sc8960x_reg_reset(sc);
    disable_irq(sc->irq);

    return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sc8960x_suspend(struct device *dev)
{
    struct sc8960x_chip *sc = dev_get_drvdata(dev);

    dev_info(dev, "%s\n", __func__);

    if (device_may_wakeup(dev))
        enable_irq_wake(sc->client->irq);

    disable_irq(sc->client->irq);

    return 0;
}

static int sc8960x_resume(struct device *dev)
{
    struct sc8960x_chip *sc = dev_get_drvdata(dev);

    dev_info(dev, "%s\n", __func__);

    enable_irq(sc->client->irq);
    if (device_may_wakeup(dev))
        disable_irq_wake(sc->client->irq);

    return 0;
}

static SIMPLE_DEV_PM_OPS(sc8960x_pm_ops, sc8960x_suspend, sc8960x_resume);
#endif /*CONFIG_PM_SLEEP*/

static struct i2c_driver sc8960x_charger_driver = {
    .driver = {
        .name = "sc8960x-charger",
        .owner = THIS_MODULE,
        .of_match_table = sc8960x_charger_match_table,
#ifdef CONFIG_PM_SLEEP
        .pm = &sc8960x_pm_ops,
#endif /*CONFIG_PM_SLEEP*/
    },

    .probe = sc8960x_charger_probe,
    .remove = sc8960x_charger_remove,
};

module_i2c_driver(sc8960x_charger_driver);

MODULE_AUTHOR("SouthChip<Aiden-yu@southchip.com>");
MODULE_DESCRIPTION("SC8960x Charger Driver");
MODULE_LICENSE("GPL v2");

