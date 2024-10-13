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
#include "nx853x_reg.h"
#ifdef CONFIG_MTK_CLASS
#include "charger_class.h"

#ifdef CONFIG_MTK_CHARGER_V4P19
#include "mtk_charger_intf.h"
#endif /*CONFIG_MTK_CHARGER_V4P19*/

#endif /*CONFIG_MTK_CLASS*/

#ifdef CONFIG_SOUTHCHIP_DVCHG_CLASS
#include "dvchg_class.h"
#endif /*CONFIG_SOUTHCHIP_DVCHG_CLASS*/
/*A06 code for AL7160A-445 by jiashixian at 20240610 start*/
#include <linux/chg-tcpc_info.h>
/*A06 code for AL7160A-445 by jiashixian at 20240610 end*/
#define NX853X_DRV_VERSION              "1.1.0_G"
#define NX853X_DEVICE_ID                0x03
/*A06 code for AL7160A-82 by jiashixian at 20240418 start*/
static bool s_is_nx853x_probe_success = false;
/*A06 code for AL7160A-82 by jiashixian at 20240418 end*/

enum {
    NX853X_STANDALONE,
    NX853X_SLAVE,
    NX853X_MASTER,
};

/* static const char* nx853x_psy_name[] = {
    [NX853X_STANDALONE] = "sc-cp-standalone",
    [NX853X_MASTER] = "sc-cp-master",
    [NX853X_SLAVE] = "sc-cp-slave",
}; */

static const char* nx853x_irq_name[] = {
    [NX853X_STANDALONE] = "nx853x-standalone-irq",
    [NX853X_MASTER] = "nx853x-master-irq",
    [NX853X_SLAVE] = "nx853x-slave-irq",
};

static int nx853x_role_data[] = {
    [NX853X_STANDALONE] = NX853X_STANDALONE,
    [NX853X_MASTER] = NX853X_MASTER,
    [NX853X_SLAVE] = NX853X_SLAVE,
};

enum {
    ADC_IBUS,
    ADC_VBUS,
    ADC_VAC,
    ADC_VOUT,
    ADC_VBAT,
    ADC_IBAT,
    ADC_THEM,
    ADC_TDIE,
    ADC_MAX_NUM,
}ADC_CH_E;


static const u32 nx853x_adc_accuracy_tbl[ADC_MAX_NUM] = {
    150000,	/* IBUS */
    35000,	/* VBUS */
    20000,	/* VOUT */
    20000,	/* VBAT */
    200000,	/* IBAT */
    0,	/* THEM */
    4,	/* TDIE */
};
enum nx853x_notify {
    NX853X_NOTIFY_OTHER = 0,
    NX853X_NOTIFY_IBUSUCPF,
    NX853X_NOTIFY_VBUSOVPALM,
    NX853X_NOTIFY_VBATOVPALM,
    NX853X_NOTIFY_IBUSOCP,
    NX853X_NOTIFY_VBUSOVP,
    NX853X_NOTIFY_IBATOCP,
    NX853X_NOTIFY_VBATOVP,
    NX853X_NOTIFY_VOUTOVP,
    NX853X_NOTIFY_VDROVP,
};


enum nx853x_fields{
    VBAT_OVP_DIS, VBAT_OVP,     /*reg00h*/
    IBAT_OVP_DIS, IBAT_OCP,     /*reg01h*/
    VBUS_OVP_DIS, VAC_OVP_DIS, VAC_OVP, VBUS_OVP,     /*reg02h*/
    VERSION_ID, DEVICE_ID,      /*reg03h*/
    IBUS_OCP_DIS, IBUS_OCP,     /*reg04h*/
    PMID2OUT_OVP_DIS, PMID2OUT_OVP_CFG, PMID2OUT_OVP, /*reg05h*/
    PMID2OUT_UVP_DIS, PMID2OUT_UVP, /*reg06h*/
    SET_IBATREG, SET_IBUSREG, SET_VBATREG, SET_TDIEREG, /*reg07h*/
    THEM_FLT_DIS, THEM_FLT,       /*reg08h*/
    SET_IBAT_SNS_RES, VAC_PD_EN, PMID_PD_EN, VBUS_PD_EN, REG_RST, VOUT_OVP_DIS, MODE, /*reg09h*/
    SS_TIMEOUT, IBUS_UCP_FALL_BLANKING_SET,FORCE_VAC_OK, WD_TIMEOUT,  /*reg0Ah*/
    VOUT_OVP, FREQ_SHIFT, FSW_SET,  /*reg0Bh*/
    CP_EN, VBUS_SHORT_DIS, PIN_DIAG_FALL_DIS, IBATSNS_HS_EN, EN_IBATSNS,/*reg0Ch*/
    WD_TIMEOUT_DIS, ACDRV_MANUAL_EN, ACDRV_EN, OTG_EN, ACDRV_PRE_DIS, ACDRV_DIS,/*reg0Dh*/
        TSHUT_DIS, IBUS_UCP_DIS,
    IBAT_REG_DIS, IBUS_REG_DIS, VBAT_REG_DIS, TDIE_REG_DIS,/*reg0Eh*/
    PMID2OUT_UVP_DG_SET, PMID2OUT_OVP_DG_SET, IBAT_OCP_DG_SET, VBUS_OCP_DG_SET,
        VOUT_OVP_DG_SET,    /*reg0Fh*/
    IBUS_UCP_FALL_DG_SET, IBUS_OCP_DG_SET, VBAT_OVP_DG_SET, /*reg10h*/
    /*A06 code for AL7160A-860 by jiashixian at 20240512 start*/
    CP_SWITCHING_STAT,VBUSPRESENT_STAT,/*reg11h*/
    /*A06 code for AL7160A-860 by jiashixian at 20240512 end*/
    VBUS2OUT_OVP_STAT, VBUS2OUT_UVP_STAT,
    ADC_EN, ADC_RATE, /*reg1Dh*/
    /*A06 code for AL7160A-157 by jiashixian at 20240508 start*/
    /*A06 code for AL7160A-860 by jiashixian at 20240512 start*/
    ADC_REGISTER_MASK, /*reg 19h*/
    /*A06 code for AL7160A-860 by jiashixian at 20240512 end*/
    /*A06 code for AL7160A-157 by jiashixian at 20240508 end*/

    F_MAX_FIELDS,
};

struct nx853x_cfg_e {
    int vbat_ovp_dis;
    int vbat_ovp_th;
    int ibat_ocp_dis;
    int ibat_ocp_th;
    int vbus_ovp_dis;
    int vbus_ovp_th;
    int vac_ovp_dis;
    int vac_ovp_th;
    int ibus_ocp_dis;
    int ibus_ocp_th;
    int pmid2out_ovp_dis;
    int pmid2out_ovp_th;
    int pmid2out_uvp_dis;
    int pmid2out_uvp_th;
    int ibatreg_th;
    int ibusreg_th;
    int vbatreg_th;
    int tdiereg_th;
    int them_flt_th;
    int ibat_sns_res;
    int vac_pd_en;
    int pmid_pd_en;
    int vbus_pd_en;
    int work_mode;
    int ss_timeout;
    int wd_timeout;
    int vout_ovp_th;
    int fsw_freq;
    int wd_timeout_dis;
    int pin_diag_fall_dis;
    int vout_ovp_dis;
    int them_flt_dis;
    int tshut_dis;
    int ibus_ucp_dis;
    int ibat_reg_dis;
    int ibus_reg_dis;
    int vbat_reg_dis;
    int tdie_reg_dis;
    int ibat_ocp_dg;
    int ibus_ucp_fall_dg;
    int ibus_ocp_dg;
    int vbat_ovp_dg;
    int die_temp;
};

struct nx853x_chip {
    struct device *dev;
    struct i2c_client *client;
    struct regmap *regmap;
    struct regmap_field *rmap_fields[F_MAX_FIELDS];

    int device_id;
    int version_id;

    int role;

    bool usb_present;
    int charge_enabled;

    int irq_gpio;
    int irq;

    /* ADC reading */
    int vbat_volt;
    int vbus_volt;
    int ibat_curr;
    int ibus_curr;
    int die_temp;

#ifdef CONFIG_MTK_CLASS
    struct charger_device *chg_dev;
#endif /*CONFIG_MTK_CLASS*/

    const char *chg_dev_name;

    struct nx853x_cfg_e cfg;

    struct power_supply_desc psy_desc;
    struct power_supply_config psy_cfg;
    struct power_supply *sc_cp_psy;
    /*A06 code for AL7160A-607 by shixuanxuan at 20240507 start*/
    bool otg_tx_mode;
    /*A06 code for AL7160A-607 by shixuanxuan at 20240507 end*/
};

//nx853x registers
static const struct reg_field nx853x_reg_fields[] = {
    /*reg00*/
    [VBAT_OVP_DIS] = REG_FIELD(NX853X_REG_00, 7, 7),
    [VBAT_OVP] = REG_FIELD(NX853X_REG_00, 0, 6),
    /*reg01*/
    [IBAT_OVP_DIS] = REG_FIELD(NX853X_REG_01, 7, 7),
    [IBAT_OCP] = REG_FIELD(NX853X_REG_01, 0, 6),
    /*reg02*/
    [VBUS_OVP_DIS] = REG_FIELD(NX853X_REG_02, 7, 7),
    [VAC_OVP_DIS] = REG_FIELD(NX853X_REG_02, 6, 6),
    [VAC_OVP] = REG_FIELD(NX853X_REG_02, 3, 5),
    [VBUS_OVP] = REG_FIELD(NX853X_REG_02, 0, 2),
    /*reg03*/
    [VERSION_ID] = REG_FIELD(NX853X_REG_03, 4, 7),
    [DEVICE_ID] = REG_FIELD(NX853X_REG_03, 0, 3),
    /*reg04*/
    [IBUS_OCP_DIS] = REG_FIELD(NX853X_REG_04, 7, 7),
    [IBUS_OCP] = REG_FIELD(NX853X_REG_04, 0, 6),
    /*reg05*/
    [PMID2OUT_OVP_DIS] = REG_FIELD(NX853X_REG_05, 7, 7),
    [PMID2OUT_OVP_CFG] = REG_FIELD(NX853X_REG_05, 5, 5),
    [PMID2OUT_OVP] = REG_FIELD(NX853X_REG_05, 0, 2),
    /*reg06*/
    [PMID2OUT_UVP_DIS] = REG_FIELD(NX853X_REG_06, 7, 7),
    [PMID2OUT_UVP] = REG_FIELD(NX853X_REG_06, 0, 2),
    /*reg07*/
    [SET_IBATREG] = REG_FIELD(NX853X_REG_07, 6, 7),
    [SET_IBUSREG] = REG_FIELD(NX853X_REG_07, 4, 5),
    [SET_VBATREG] = REG_FIELD(NX853X_REG_07, 2, 3),
    [SET_TDIEREG] = REG_FIELD(NX853X_REG_07, 0, 1),
    /*reg08*/
    [THEM_FLT_DIS] = REG_FIELD(NX853X_REG_08, 7, 7),
    [THEM_FLT] = REG_FIELD(NX853X_REG_08, 0, 5),
    /*reg09*/
    [SET_IBAT_SNS_RES] = REG_FIELD(NX853X_REG_09, 7, 7),
    [VAC_PD_EN] = REG_FIELD(NX853X_REG_09, 6, 6),
    [PMID_PD_EN] = REG_FIELD(NX853X_REG_09, 5, 5),
    [VBUS_PD_EN] = REG_FIELD(NX853X_REG_09, 4, 4),
    [REG_RST] = REG_FIELD(NX853X_REG_09, 3, 3),
    [VOUT_OVP_DIS] = REG_FIELD(NX853X_REG_09, 2, 2),
    [MODE] = REG_FIELD(NX853X_REG_09, 0, 1),
    /*reg0A*/
    [SS_TIMEOUT] = REG_FIELD(NX853X_REG_0A, 5, 7),
    [IBUS_UCP_FALL_BLANKING_SET] = REG_FIELD(NX853X_REG_0A, 3, 4),
    [FORCE_VAC_OK] = REG_FIELD(NX853X_REG_0A, 2, 2),
    [WD_TIMEOUT] = REG_FIELD(NX853X_REG_0A, 0, 1),
    /*reg0B*/
    [VOUT_OVP] = REG_FIELD(NX853X_REG_0B, 7, 7),
    [FREQ_SHIFT] = REG_FIELD(NX853X_REG_0B, 4, 5),
    [FSW_SET] = REG_FIELD(NX853X_REG_0B, 0, 3),
    /*reg0C*/
    [CP_EN] = REG_FIELD(NX853X_REG_0C, 7, 7),
    [VBUS_SHORT_DIS] = REG_FIELD(NX853X_REG_0C, 5, 5),
    [PIN_DIAG_FALL_DIS] = REG_FIELD(NX853X_REG_0C, 4, 4),
    [IBATSNS_HS_EN] = REG_FIELD(NX853X_REG_0C, 1, 1),
    [EN_IBATSNS] = REG_FIELD(NX853X_REG_0C, 0, 0),
    /*reg0D*/
    [WD_TIMEOUT_DIS] = REG_FIELD(NX853X_REG_0D, 7, 7),
    [ACDRV_MANUAL_EN] = REG_FIELD(NX853X_REG_0D, 6, 6),
    [ACDRV_EN] = REG_FIELD(NX853X_REG_0D, 5, 5),
    [OTG_EN] = REG_FIELD(NX853X_REG_0D, 4, 4),
    [ACDRV_PRE_DIS] = REG_FIELD(NX853X_REG_0D, 3, 3),
    [ACDRV_DIS] = REG_FIELD(NX853X_REG_0D, 2, 2),
    [TSHUT_DIS] = REG_FIELD(NX853X_REG_0D, 1, 1),
    [IBUS_UCP_DIS] = REG_FIELD(NX853X_REG_0D, 0, 0),
    /*reg0E*/
    [IBAT_REG_DIS] = REG_FIELD(NX853X_REG_0E, 3, 3),
    [IBUS_REG_DIS] = REG_FIELD(NX853X_REG_0E, 2, 2),
    [VBAT_REG_DIS] = REG_FIELD(NX853X_REG_0E, 1, 1),
    [TDIE_REG_DIS] = REG_FIELD(NX853X_REG_0E, 0, 0),
    /*reg0F*/
    [PMID2OUT_UVP_DG_SET] = REG_FIELD(NX853X_REG_0F, 7, 7),
    [PMID2OUT_OVP_DG_SET] = REG_FIELD(NX853X_REG_0F, 6, 6),
    [IBAT_OCP_DG_SET] = REG_FIELD(NX853X_REG_0F, 4, 5),
    [VBUS_OCP_DG_SET] = REG_FIELD(NX853X_REG_0F, 2, 2),
    [VOUT_OVP_DG_SET] = REG_FIELD(NX853X_REG_0F, 1, 1),
    /*reg10*/
    [IBUS_UCP_FALL_DG_SET] = REG_FIELD(NX853X_REG_10, 6, 7),
    [IBUS_OCP_DG_SET] = REG_FIELD(NX853X_REG_10, 4, 5),
    [VBAT_OVP_DG_SET] = REG_FIELD(NX853X_REG_10, 2, 3),
    /*reg11*/
    [CP_SWITCHING_STAT] = REG_FIELD(NX853X_REG_11, 7, 7),
     /*A06 code for AL7160A-860 by jiashixian at 20240512 start*/
    [VBUSPRESENT_STAT] = REG_FIELD(NX853X_REG_11, 2, 2),
    /*A06 code for AL7160A-860 by jiashixian at 20240512 start*/
    /*reg14*/
    [VBUS2OUT_OVP_STAT] = REG_FIELD(NX853X_REG_14, 4, 4),
    [VBUS2OUT_UVP_STAT] = REG_FIELD(NX853X_REG_14, 5, 5),
    /*A06 code for 20240508 by jiashixian at 2024 start*/
    /*A06 code for AL7160A-860 by jiashixian at 20240512 start*/
    /*reg19*/
    [ADC_REGISTER_MASK] = REG_FIELD(NX853X_REG_19, 3, 3),
    /*A06 code for AL7160A-860 by jiashixian at 20240512 end*/
    /*A06 code for 20240508 by jiashixian at 20240407 end*/
    /*reg1D*/
    [ADC_EN] = REG_FIELD(NX853X_REG_1D, 7, 7),
    [ADC_RATE] = REG_FIELD(NX853X_REG_1D, 6, 6),
};

static const int nx853x_adc_m[] =
    {25, 375, 5, 125, 125, 3125, 44, 5};

static const int nx853x_adc_l[] =
    {10, 100, 1, 100, 100, 1000, 100, 10};

static const int nx853x_vac_ovp[] =
    {6500, 11000, 12000, 13000, 14000, 15000, 15000, 15000};

static const struct regmap_config nx853x_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,
    .max_register = NX853X_REGMAX,
};


enum nx853x_reg_range {
    NX853X_VBAT_OVP,
    NX853X_IBAT_OCP,
    NX853X_VBUS_OVP,
    NX853X_IBUS_OCP,
    NX853X_VAC_OVP,
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

#define NX853X_CHG_RANGE(_min, _max, _step, _offset, _ru) \
{ \
    .min = _min, \
    .max = _max, \
    .step = _step, \
    .offset = _offset, \
    .round_up = _ru, \
}

#define NX853X_CHG_RANGE_T(_table, _ru) \
    { .table = _table, .num_table = ARRAY_SIZE(_table), .round_up = _ru, }


static const struct reg_range nx853x_reg_range[] = {
    [NX853X_VBAT_OVP]      = NX853X_CHG_RANGE(3840, 5110, 10, 4040, false),
    [NX853X_IBAT_OCP]      = NX853X_CHG_RANGE(0, 12700, 100, 500, false),
    [NX853X_VBUS_OVP]      = NX853X_CHG_RANGE(9250, 11000, 250, 9250, false),
    [NX853X_IBUS_OCP]      = NX853X_CHG_RANGE(1000, 4750, 250, 0, false),
    [NX853X_VAC_OVP]       = NX853X_CHG_RANGE_T(nx853x_vac_ovp, false),
};

struct flag_bit {
    int notify;
    int mask;
    char *name;
};

struct intr_flag {
    int reg;
    int len;
    struct flag_bit bit[8];
};

static struct intr_flag cp_intr_flag[] = {
    { .reg = NX853X_REG_15, .len = 7, .bit = {
                {.mask = NX853X_CP_SWITCH_MASK, .name = "cp switching flag", .notify = NX853X_NOTIFY_OTHER},
                {.mask = NX853X_VBUS_ERRORHI_FLAG_MASK, .name = "vbus errorhi flag", .notify = NX853X_NOTIFY_OTHER},
                {.mask = NX853X_VBUS_ERRORLO_FLAG_MASK, .name = "vbus errorlo flag", .notify = NX853X_NOTIFY_OTHER},
                {.mask = NX853X_VOUT_REV_EN_MASK, .name = "vout th rev en flag", .notify = NX853X_NOTIFY_OTHER},
                {.mask = NX853X_VOUT_CHG_EN_MASK, .name = "vout th chg en flag", .notify = NX853X_NOTIFY_OTHER},
                {.mask = NX853X_VBUS_INSERT_FLAG_MASK, .name = "vbus insert flag", .notify = NX853X_NOTIFY_OTHER},
                {.mask = NX853X_VOUT_INSERT_FLAG_MASK, .name = "vout insert flag", .notify = NX853X_NOTIFY_OTHER},
                },
    },
    { .reg = NX853X_REG_16, .len = 7, .bit = {
                {.mask = NX853X_ADC_DONE_STAT_MASK, .name = "adc done flag", .notify = NX853X_NOTIFY_OTHER},
                {.mask = NX853X_PIN_DIAG_FAIL_FLAG_MASK, .name = "pin diag fail flag", .notify = NX853X_NOTIFY_OTHER},
                {.mask = NX853X_IBUS_UCP_RISE_FLAG_MASK, .name = "ibus ucp rise flag", .notify = NX853X_NOTIFY_IBUSUCPF},
                {.mask = NX853X_TDIE_ALM_FLAG_MASK, .name = "tdie alm flag", .notify = NX853X_NOTIFY_OTHER},
                {.mask = NX853X_VBAT_REG_ACTIVE_MASK, .name = "vbat reg active flag", .notify = NX853X_NOTIFY_OTHER},
                {.mask = NX853X_IBAT_REG_ACTIVE_MASK, .name = "ibat reg active flag", .notify = NX853X_NOTIFY_OTHER},
                {.mask = NX853X_IBUS_REG_ACTIVE_MASK, .name = "ibus reg active flag", .notify = NX853X_NOTIFY_OTHER},
                },
    },
    { .reg = NX853X_REG_17, .len = 8, .bit = {
                {.mask = NX853X_POR_FLAG_MASK, .name = "por flag", .notify = NX853X_NOTIFY_OTHER},
                {.mask = NX853X_SS_FAIL_FLAG_MASK, .name = "ss fail flag", .notify = NX853X_NOTIFY_OTHER},
                {.mask = NX853X_IBUS_UCP_FAIL_FLAG_MASK, .name = "ibus ucp fall flag", .notify = NX853X_NOTIFY_IBUSUCPF},
                {.mask = NX853X_IBUS_OCP_FAIL_FLAG_MASK, .name = "ibus ocp flag", .notify = NX853X_NOTIFY_OTHER},
                {.mask = NX853X_IBAT_OCP_FLAG_MASK, .name = "ibat ocp flag", .notify = NX853X_NOTIFY_OTHER},
                {.mask = NX853X_VBUS_OVP_FLAG_MASK, .name = "vbus ovp flag", .notify = NX853X_NOTIFY_OTHER},
                {.mask = NX853X_VOUT_OVP_FLAG_MASK, .name = "vout ovp flag", .notify = NX853X_NOTIFY_OTHER},
                {.mask = NX853X_VBAT_OVP_FLAG_MASK, .name = "vbat ovp flag", .notify = NX853X_NOTIFY_OTHER},
                },
    },
    { .reg = NX853X_REG_18, .len = 6, .bit = {
                {.mask = NX853X_PMID2OUT_UVP_FLAG_MASK, .name = "pmid2out uvp flag", .notify = NX853X_NOTIFY_OTHER},
                {.mask = NX853X_PMID2OUT_OVP_FLAG_MASK, .name = "pmid2out ovp flag", .notify = NX853X_NOTIFY_OTHER},
                {.mask = NX853X_TSHUT_FLAG_MASK, .name = "tshut flag", .notify = NX853X_NOTIFY_OTHER},
                {.mask = NX853X_THEM_FLT_FLAG_MASK, .name = "them flt flag", .notify = NX853X_NOTIFY_OTHER},
                {.mask = NX853X_SS_TIMOUT_FLAG_MASK, .name = "ss timeout flag", .notify = NX853X_NOTIFY_OTHER},
                {.mask = NX853X_WD_TIMOUT_FLAG_MASK, .name = "wd timeout flag", .notify = NX853X_NOTIFY_OTHER},
                },
    },
};

/********************COMMON API***********************/
__maybe_unused static u8 val2reg(enum nx853x_reg_range id, u32 val)
{
    int i;
    u8 reg;
    const struct reg_range *range= &nx853x_reg_range[id];

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

__maybe_unused static u32 reg2val(enum nx853x_reg_range id, u8 reg)
{
    const struct reg_range *range= &nx853x_reg_range[id];
    if (!range)
        return reg;
    return range->table ? range->table[reg] :
                  range->offset + range->step * reg;
}

static int nx853x_init_device(struct nx853x_chip *sc);
/*********************************************************/
static int nx853x_field_read(struct nx853x_chip *sc,
                enum nx853x_fields field_id, unsigned int *val)
{
    int ret;

    ret = regmap_field_read(sc->rmap_fields[field_id], val);
    if (ret < 0) {
        dev_err(sc->dev, "nx853x read field %d fail: %d\n", field_id, ret);
    }

    return ret;
}

static int nx853x_field_write(struct nx853x_chip *sc,
                enum nx853x_fields field_id, int val)
{
    int ret;

    ret = regmap_field_write(sc->rmap_fields[field_id], val);
    if (ret < 0) {
        dev_err(sc->dev, "nx853x read field %d fail: %d\n", field_id, ret);
    }

    return ret;
}

static int nx853x_read_bulk(struct nx853x_chip *sc,
                uint8_t addr, uint8_t *val, uint8_t count)
{
    int ret;

    ret = regmap_bulk_read(sc->regmap, addr, val, count);
    if (ret < 0) {
        dev_err(sc->dev, "nx853x read %02x block failed %d\n", addr, ret);
    }

    return ret;
}

/*******************************************************/
static int nx853x_reg_reset(struct nx853x_chip *sc)
{
    return nx853x_field_write(sc, REG_RST, 1);
}

int nx853x_detect_device(struct nx853x_chip *sc)
{
    int ret;
    int device_id;

    ret = nx853x_field_read(sc, DEVICE_ID, &device_id);
    if (ret < 0) {
        dev_err(sc->dev, "get device id fail\n");
        return ret;
    }

    if (device_id != NX853X_DEVICE_ID) {
        dev_err(sc->dev, "%s not find nx853x, ID = 0x%02x\n", __func__, ret);
        return -EINVAL;
    }
    sc->device_id = device_id;

    ret = nx853x_field_read(sc, VERSION_ID, &sc->version_id);

    dev_info(sc->dev, "%s Device id: 0x%02x, Version id: 0x%02x\n", __func__,
        sc->device_id, sc->version_id);

    return ret;
}

static int nx853x_check_charge_enabled(struct nx853x_chip *sc, int *enabled)
{
    return nx853x_field_read(sc, CP_SWITCHING_STAT, enabled);
}

__maybe_unused static int nx853x_set_busovp_th(struct nx853x_chip *sc, int threshold)
{
    int reg_val = val2reg(NX853X_VBUS_OVP, threshold);

    dev_info(sc->dev,"%s:%d-%d", __func__, threshold, reg_val);

    return nx853x_field_write(sc, VBUS_OVP, reg_val);
}

__maybe_unused static int nx853x_set_busocp_th(struct nx853x_chip *sc, int threshold)
{
    int reg_val = val2reg(NX853X_IBUS_OCP, threshold);

    dev_info(sc->dev,"%s:%d-%d", __func__, threshold, reg_val);

    return nx853x_field_write(sc, IBUS_OCP, reg_val);
}

__maybe_unused static int nx853x_set_batovp_th(struct nx853x_chip *sc, int threshold)
{
    int reg_val = val2reg(NX853X_VBAT_OVP, threshold);

    dev_info(sc->dev,"%s:%d-%d", __func__, threshold, reg_val);

    return nx853x_field_write(sc, VBAT_OVP, reg_val);
}

__maybe_unused static int nx853x_set_batocp_th(struct nx853x_chip *sc, int threshold)
{
    int reg_val = val2reg(NX853X_IBAT_OCP, threshold);

    dev_info(sc->dev,"%s:%d-%d", __func__, threshold, reg_val);

    return nx853x_field_write(sc, IBAT_OCP, reg_val);
}

__maybe_unused static int nx853x_is_vbuslowerr(struct nx853x_chip *sc, bool *err)
{
    int ret;
    int val;

    ret = nx853x_field_read(sc, VBUS2OUT_UVP_STAT, &val);
    if(ret < 0) {
        return ret;
    }

    dev_info(sc->dev,"%s:%d",__func__,val);

    *err = (bool)val;

    return ret;
}

static int nx853x_enable_adc(struct nx853x_chip *sc, bool en)
{
    /*A06 code for AL7160A-1232 by jiashixian at 20240518 start*/
    dev_info(sc->dev,"%s:%d", __func__, en);
    /*A06 code for AL7160A-1232 by jiashixian at 20240518 end*/
    return nx853x_field_write(sc, ADC_EN, !!en);
}

static int nx853x_get_adc_data(struct nx853x_chip *sc,
        int channel,  int *result)
{
    int ret;
    int reg = NX853X_REG_1F + channel * 2;
    uint8_t reg_val[2];

    if (channel >= ADC_MAX_NUM)
        return -EINVAL;

    //nx853x_enable_adc(sc, true);
    //msleep(50);

    ret = nx853x_read_bulk(sc, reg, reg_val, 2);

    if (ret < 0)
        return ret;

    *result = ((reg_val[0] << 8) | reg_val[1]) *
        nx853x_adc_m[channel] / nx853x_adc_l[channel];

    //nx853x_enable_adc(sc, false);

    return ret;
}

static int nx853x_dump_reg(struct nx853x_chip *sc)
{
    int ret;
    int i;
    int val;

    for (i = 0; i <= NX853X_REGMAX; i++) {
        ret = regmap_read(sc->regmap, i, &val);
        dev_err(sc->dev, "%s reg[0x%02x] = 0x%02x\n",
                __func__, i, val);
    }

    return ret;
}

/*A06 code for AL7160A-157 by jiashixian at 20240508 start*/
/*A06 code for AL7160A-860 by jiashixian at 20240512 start*/
static int nx853x_enable_adc_irq_mask(struct nx853x_chip *sc,bool en)
{
    return nx853x_field_write(sc, ADC_REGISTER_MASK, !!en);
}

static int nx853x_get_vbus_present_status(struct nx853x_chip *sc)
{
    int ret;
    int val;

    ret = nx853x_field_read(sc, VBUSPRESENT_STAT, &val);
    if(ret < 0) {
        return ret;
    }

    dev_info(sc->dev,"%s:%d",__func__,val);
    return val;
}
/*A06 code for AL7160A-860 by jiashixian at 20240512 end*/
/*A06 code for AL7160A-157 by jiashixian at 20240508 end*/

__maybe_unused static int nx853x_enable_charge(struct nx853x_chip *sc, bool en)
{
    int ret = 0;
    int vbus_value = 0, vout_value = 0, value = 0;
    int vbus_hi = 0, vbus_low = 0;
    dev_info(sc->dev,"%s:%d",__func__,en);

    if (!en) {
        ret |= nx853x_field_write(sc, CP_EN, !!en);

        return ret;
    } else {
        ret = nx853x_get_adc_data(sc, ADC_VBUS, &vbus_value);
        ret |= nx853x_get_adc_data(sc, ADC_VOUT, &vout_value);
        dev_info(sc->dev,"%s: vbus/vout:%d / %d = %d \r\n", __func__, vbus_value, vout_value, vbus_value*100/vout_value);

        ret |= nx853x_field_read(sc, MODE, &value);
        dev_info(sc->dev,"%s: mode:%d %s \r\n", __func__, value, (value == 0 ?"4:1":(value == 1 ?"2:1":"else")));

        ret |= nx853x_field_read(sc, VBUS2OUT_UVP_STAT, &vbus_low);
        ret |= nx853x_field_read(sc, VBUS2OUT_OVP_STAT, &vbus_hi);
        dev_info(sc->dev,"%s: high:%d  low:%d \r\n", __func__, vbus_hi, vbus_low);

        ret |= nx853x_field_write(sc, CP_EN, !!en);

        disable_irq(sc->irq);

        mdelay(300);

        ret |= nx853x_field_read(sc, CP_SWITCHING_STAT, &value);
        if (!value) {
            dev_info(sc->dev,"%s:enable fail \r\n", __func__);
            nx853x_dump_reg(sc);
        } else {
            dev_info(sc->dev,"%s:enable success \r\n", __func__);
        }

        enable_irq(sc->irq);
    }

    return ret;
}

/*********************mtk charger interface start**********************************/
#ifdef CONFIG_MTK_CLASS
static inline int to_nx853x_adc(enum adc_channel chan)
{
    switch (chan) {
    case ADC_CHANNEL_VBUS:
        return ADC_VBUS;
    case ADC_CHANNEL_VBAT:
        return ADC_VBAT;
    case ADC_CHANNEL_IBUS:
        return ADC_IBUS;
    case ADC_CHANNEL_IBAT:
        return ADC_IBAT;
    case ADC_CHANNEL_TEMP_JC:
        return ADC_TDIE;
    case ADC_CHANNEL_VOUT:
        return ADC_VOUT;
    default:
        break;
    }
    return ADC_MAX_NUM;
}


static int mtk_nx853x_is_chg_enabled(struct charger_device *chg_dev, bool *en)
{
    struct nx853x_chip *sc = charger_get_data(chg_dev);
    int ret;
    int val;

    ret = nx853x_check_charge_enabled(sc, &val);

    *en = !!val;

    return ret;
}


static int mtk_nx853x_enable_chg(struct charger_device *chg_dev, bool en)
{
    struct nx853x_chip *sc = charger_get_data(chg_dev);
    int ret;

    ret = nx853x_enable_charge(sc,en);

    return ret;
}


static int mtk_nx853x_set_vbusovp(struct charger_device *chg_dev, u32 uV)
{
    struct nx853x_chip *sc = charger_get_data(chg_dev);
    int mv;
    mv = uV / 1000;

    return nx853x_set_busovp_th(sc, mv);
}

static int mtk_nx853x_set_ibusocp(struct charger_device *chg_dev, u32 uA)
{
    struct nx853x_chip *sc = charger_get_data(chg_dev);
    int ma;
    ma = uA / 1000;

    return nx853x_set_busocp_th(sc, ma);
}

static int mtk_nx853x_set_vbatovp(struct charger_device *chg_dev, u32 uV)
{
    struct nx853x_chip *sc = charger_get_data(chg_dev);
    int ret;

    ret = nx853x_set_batovp_th(sc, uV/1000);
    if (ret < 0)
        return ret;

    return ret;
}

static int mtk_nx853x_set_ibatocp(struct charger_device *chg_dev, u32 uA)
{
    struct nx853x_chip *sc = charger_get_data(chg_dev);
    int ret;

    ret = nx853x_set_batocp_th(sc, uA/1000);
    if (ret < 0)
        return ret;

    return ret;
}


static int mtk_nx853x_get_adc(struct charger_device *chg_dev, enum adc_channel chan,
            int *min, int *max)
{
    struct nx853x_chip *sc = charger_get_data(chg_dev);

    nx853x_get_adc_data(sc, to_nx853x_adc(chan), max);

    if(chan != ADC_CHANNEL_TEMP_JC)
        *max = *max * 1000;

    if (min != max)
        *min = *max;

    return 0;
}

static int mtk_nx853x_get_adc_accuracy(struct charger_device *chg_dev,
                enum adc_channel chan, int *min, int *max)
{
    *min = *max = nx853x_adc_accuracy_tbl[to_nx853x_adc(chan)];
    return 0;
}

static int mtk_nx853x_is_vbuslowerr(struct charger_device *chg_dev, bool *err)
{
    struct nx853x_chip *sc = charger_get_data(chg_dev);

    return nx853x_is_vbuslowerr(sc,err);
}

static int mtk_nx853x_set_vbatovp_alarm(struct charger_device *chg_dev, u32 uV)
{
    return 0;
}

static int mtk_nx853x_reset_vbatovp_alarm(struct charger_device *chg_dev)
{
    return 0;
}

static int mtk_nx853x_set_vbusovp_alarm(struct charger_device *chg_dev, u32 uV)
{
    return 0;
}

static int mtk_nx853x_reset_vbusovp_alarm(struct charger_device *chg_dev)
{
    return 0;
}

static int mtk_nx853x_init_chip(struct charger_device *chg_dev)
{
    struct nx853x_chip *sc = charger_get_data(chg_dev);

    return nx853x_init_device(sc);
}

static const struct charger_ops nx853x_chg_ops = {
    .enable = mtk_nx853x_enable_chg,
    .is_enabled = mtk_nx853x_is_chg_enabled,
    .get_adc = mtk_nx853x_get_adc,
    .get_adc_accuracy = mtk_nx853x_get_adc_accuracy,
    .set_vbusovp = mtk_nx853x_set_vbusovp,
    .set_ibusocp = mtk_nx853x_set_ibusocp,
    .set_vbatovp = mtk_nx853x_set_vbatovp,
    .set_ibatocp = mtk_nx853x_set_ibatocp,
    .init_chip = mtk_nx853x_init_chip,
    .is_vbuslowerr = mtk_nx853x_is_vbuslowerr,
    .set_vbatovp_alarm = mtk_nx853x_set_vbatovp_alarm,
    .reset_vbatovp_alarm = mtk_nx853x_reset_vbatovp_alarm,
    .set_vbusovp_alarm = mtk_nx853x_set_vbusovp_alarm,
    .reset_vbusovp_alarm = mtk_nx853x_reset_vbusovp_alarm,
};

static const struct charger_properties nx853x_chg_props = {
    .alias_name = "nx853x_chg",
};

#endif /*CONFIG_MTK_CLASS*/



static int nx853x_set_present(struct nx853x_chip *sc, bool present)
{
    int ret = 0;

    sc->usb_present = present;
    if (present) {
        ret = nx853x_init_device(sc);
    }
    return ret;
}

    /* A06 code for AL7160A-42 by jiashixian at 2024/04/07 start */
static enum power_supply_property nx853x_charger_props[] = {
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
int nx853x_charger_enable_otg(struct nx853x_chip *sc, int enable)
{
    int ret = 0;

    ret |= nx853x_field_write(sc, ACDRV_MANUAL_EN, enable);
    ret |= nx853x_field_write(sc, ACDRV_EN, enable);
    ret |= nx853x_field_write(sc, ACDRV_PRE_DIS, enable);
    if (ret < 0) {
        dev_err(sc->dev, "nx853x enable otg fail, ret = %d\n", ret);
    } else {
        dev_err(sc->dev, "nx853x enable otg, ret = %d\n", ret);
    }
    /*A06 code for AL7160A-607 by shixuanxuan at 20240507 start*/
    sc->otg_tx_mode = enable;
    /*A06 code for AL7160A-607 by shixuanxuan at 20240507 end*/

    return ret;
}
/*A06 code for SR-AL7160A-01-265 by shixuanxuan at 20240407 end*/

static int nx853x_charger_get_property(struct power_supply *psy,
                enum power_supply_property psp,
                union power_supply_propval *val)
{
    struct nx853x_chip *sc = power_supply_get_drvdata(psy);
    int result;
    int ret;

    switch (psp) {
    case POWER_SUPPLY_PROP_CHARGING_ENABLED:
        nx853x_check_charge_enabled(sc, &sc->charge_enabled);
        val->intval = sc->charge_enabled;
        break;
    case POWER_SUPPLY_PROP_PRESENT:
        val->intval = sc->usb_present;
        ret = 0;
        break;
    case POWER_SUPPLY_PROP_CPM_VBUS_VOLT:
        ret = nx853x_get_adc_data(sc, ADC_VBUS, &result);
        if (!ret)
            sc->vbus_volt = result;
        val->intval = sc->vbus_volt;
        break;
    case POWER_SUPPLY_PROP_CPM_IBUS_CURRENT:
        ret = nx853x_get_adc_data(sc, ADC_IBUS, &result);
        if (!ret)
            sc->ibus_curr = result;
        val->intval = sc->ibus_curr;
        break;
    case POWER_SUPPLY_PROP_CPM_VBAT_VOLT:
        ret = nx853x_get_adc_data(sc, ADC_VBAT, &result);
        if (!ret)
            sc->vbat_volt = result;
        val->intval = sc->vbat_volt;
        break;
    case POWER_SUPPLY_PROP_CPM_IBAT_CURRENT:
        ret = nx853x_get_adc_data(sc, ADC_IBAT, &result);
        if (!ret)
            sc->ibat_curr = result;
        val->intval = sc->ibat_curr;
        break;

    case POWER_SUPPLY_PROP_CPM_TEMP:
        ret = nx853x_get_adc_data(sc, ADC_TDIE, &result);
        if (!ret) {
            sc->die_temp = result;
            val->intval = sc->die_temp;
        } else {
            val->intval = 0;
        }
        break;
    /*A06 code for AL7160A-607 by shixuanxuan at 20240507 start*/
    case POWER_SUPPLY_PROP_CP_OTG_ENABLE:
        val->intval = sc->otg_tx_mode;
        break;
    /*A06 code for AL7160A-607 by shixuanxuan at 20240507 end*/

    default:
        return -EINVAL;

    }

    return 0;
}
    /* A06 code for AL7160A-42 by jiashixian at 2024/04/07 end */

static int nx853x_charger_set_property(struct power_supply *psy,
                    enum power_supply_property prop,
                    const union power_supply_propval *val)
{
    struct nx853x_chip *sc = power_supply_get_drvdata(psy);

    switch (prop) {
    case POWER_SUPPLY_PROP_CHARGING_ENABLED:
        nx853x_enable_charge(sc, !!val->intval);
        dev_info(sc->dev, "POWER_SUPPLY_PROP_CHARGING_ENABLED: %s\n",
                val->intval ? "enable" : "disable");
        break;
    case POWER_SUPPLY_PROP_PRESENT:
        nx853x_set_present(sc, !!val->intval);
        break;
    /*A06 code for SR-AL7160A-01-265 by shixuanxuan at 20240407 start*/
    case POWER_SUPPLY_PROP_CP_OTG_ENABLE:
        nx853x_charger_enable_otg(sc, !!val->intval);
        break;
    /*A06 code for SR-AL7160A-01-265 by shixuanxuan at 20240407 end*/
    /*A06 code for AL7160A-1232 by jiashixian at 20240518 start*/
    case POWER_SUPPLY_PROP_CP_ADC_ENABLE:
        nx853x_enable_adc(sc, !!val->intval);
        break;
    /*A06 code for AL7160A-1232 by jiashixian at 20240518 start*/
    default:
        return -EINVAL;
    }

    return 0;
}


static int nx853x_psy_register(struct nx853x_chip *sc)
{
    sc->psy_cfg.drv_data = sc;
    sc->psy_cfg.of_node = sc->dev->of_node;

    sc->psy_desc.name = "cpm-standalone";
    sc->psy_desc.type = POWER_SUPPLY_TYPE_MAINS;
    sc->psy_desc.properties = nx853x_charger_props;
    sc->psy_desc.num_properties = ARRAY_SIZE(nx853x_charger_props);
    sc->psy_desc.get_property = nx853x_charger_get_property;
    sc->psy_desc.set_property = nx853x_charger_set_property;

    sc->sc_cp_psy = devm_power_supply_register(sc->dev,
            &sc->psy_desc, &sc->psy_cfg);
    if (IS_ERR(sc->sc_cp_psy)) {
        dev_err(sc->dev, "%s failed to register psy\n", __func__);
        return PTR_ERR(sc->sc_cp_psy);
    }

    dev_info(sc->dev, "%s power supply register successfully\n", sc->psy_desc.name);
    return 0;
}

#ifdef CONFIG_MTK_CLASS
static inline int status_reg_to_charger(enum nx853x_notify notify)
{
	switch (notify) {
	case NX853X_NOTIFY_IBUSUCPF:
		return CHARGER_DEV_NOTIFY_IBUSUCP_FALL;
    case NX853X_NOTIFY_VBUSOVPALM:
		return CHARGER_DEV_NOTIFY_VBUSOVP_ALARM;
    case NX853X_NOTIFY_VBATOVPALM:
		return CHARGER_DEV_NOTIFY_VBATOVP_ALARM;
    case NX853X_NOTIFY_IBUSOCP:
		return CHARGER_DEV_NOTIFY_IBUSOCP;
    case NX853X_NOTIFY_VBUSOVP:
		return CHARGER_DEV_NOTIFY_VBUS_OVP;
    case NX853X_NOTIFY_IBATOCP:
		return CHARGER_DEV_NOTIFY_IBATOCP;
    case NX853X_NOTIFY_VBATOVP:
		return CHARGER_DEV_NOTIFY_BAT_OVP;
    case NX853X_NOTIFY_VOUTOVP:
		return CHARGER_DEV_NOTIFY_VOUTOVP;
	default:
        return -EINVAL;
		break;
	}
	return -EINVAL;
}
#endif /*CONFIG_MTK_CLASS*/

__maybe_unused
static void nx853x_dump_check_fault_status(struct nx853x_chip *sc)
{
    int ret;
    u8 flag = 0;
    int i,j,k;
#ifdef CONFIG_MTK_CLASS
    int noti;
#endif /*CONFIG_MTK_CLASS*/

    for (i = 0; i <= NX853X_REGMAX; i++) {
        ret = nx853x_read_bulk(sc, i, &flag, 1);
        dev_err(sc->dev, "%s reg[0x%02x] = 0x%02x\n", __func__, i, flag);
        for (k=0; k < ARRAY_SIZE(cp_intr_flag); k++) {
            if (cp_intr_flag[k].reg == i){
                for (j=0; j <  cp_intr_flag[k].len; j++) {
                    if (flag & cp_intr_flag[k].bit[j].mask) {
                        dev_err(sc->dev,"trigger :%s\n",cp_intr_flag[k].bit[j].name);
        #ifdef CONFIG_MTK_CLASS
                        noti = status_reg_to_charger(cp_intr_flag[k].bit[j].notify);
                        if(noti >= 0) {
                            charger_dev_notify(sc->chg_dev, noti);
                        }
        #endif /*CONFIG_MTK_CLASS*/
                    }
                }
            }
        }
    }
}

static irqreturn_t nx853x_irq_handler(int irq, void *dev_id)
{
    struct nx853x_chip *sc = dev_id;

    dev_err(sc->dev, "%s irq trigger\n", __func__);

    nx853x_dump_check_fault_status(sc);

    power_supply_changed(sc->sc_cp_psy);

    return IRQ_HANDLED;
}

static ssize_t nx853x_show_registers(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    struct nx853x_chip *sc = dev_get_drvdata(dev);
    uint8_t addr;
    unsigned int val;
    uint8_t tmpbuf[300];
    int len;
    int idx = 0;
    int ret;

    idx = snprintf(buf, PAGE_SIZE, "%s:\n", "nx853x");
    for (addr = 0x0; addr <= NX853X_REGMAX; addr++) {
        ret = regmap_read(sc->regmap, addr, &val);
        if (ret == 0) {
            len = snprintf(tmpbuf, PAGE_SIZE - idx,
                    "Reg[%.2X] = 0x%.2x\n", addr, val);
            memcpy(&buf[idx], tmpbuf, len);
            idx += len;
        }
    }

    return idx;
}

static ssize_t nx853x_store_register(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct nx853x_chip *sc = dev_get_drvdata(dev);
    int ret;
    unsigned int reg;
    unsigned int val;

    ret = sscanf(buf, "%x %x", &reg, &val);
    if (ret == 2 && reg <= NX853X_REGMAX)
        regmap_write(sc->regmap, (unsigned char)reg, (unsigned char)val);

    return count;
}

static DEVICE_ATTR(registers, 0660, nx853x_show_registers, nx853x_store_register);

static void nx853x_create_device_node(struct device *dev)
{
    device_create_file(dev, &dev_attr_registers);
}


static int nx853x_parse_dt(struct nx853x_chip *sc, struct device *dev)
{
    struct device_node *np = dev->of_node;
    int i;
    int ret;
    struct {
        char *name;
        int *conv_data;
    } props[] = {
        {"sc,nx853x,vbat-ovp-dis", &(sc->cfg.vbat_ovp_dis)},
        {"sc,nx853x,vbat-ovp", &(sc->cfg.vbat_ovp_th)},
        {"sc,nx853x,ibat-ocp-dis", &(sc->cfg.ibat_ocp_dis)},
        {"sc,nx853x,ibat-ocp", &(sc->cfg.ibat_ocp_th)},
        {"sc,nx853x,vbus-ovp-dis", &(sc->cfg.vbus_ovp_dis)},
        {"sc,nx853x,vbus-ovp", &(sc->cfg.vbus_ovp_th)},
        {"sc,nx853x,vac-ovp-dis", &(sc->cfg.vac_ovp_dis)},
        {"sc,nx853x,vac-ovp", &(sc->cfg.vac_ovp_th)},
        {"sc,nx853x,ibus-ocp-dis", &(sc->cfg.ibus_ocp_dis)},
        {"sc,nx853x,ibus-ocp", &(sc->cfg.ibus_ocp_th)},
        {"sc,nx853x,pmid2out-ovp-dis", &(sc->cfg.pmid2out_ovp_dis)},
        {"sc,nx853x,pmid2out-ovp", &(sc->cfg.pmid2out_ovp_th)},
        {"sc,nx853x,pmid2out-uvp-dis", &(sc->cfg.pmid2out_uvp_dis)},
        {"sc,nx853x,pmid2out-uvp", &(sc->cfg.pmid2out_uvp_th)},
        {"sc,nx853x,ibatreg", &(sc->cfg.ibatreg_th)},
        {"sc,nx853x,ibusreg", &(sc->cfg.ibusreg_th)},
        {"sc,nx853x,vbatreg", &(sc->cfg.vbatreg_th)},
        {"sc,nx853x,tdiereg", &(sc->cfg.tdiereg_th)},
        {"sc,nx853x,them-flt", &(sc->cfg.them_flt_th)},
        {"sc,nx853x,ibat-sns-res", &(sc->cfg.ibat_sns_res)},
        {"sc,sc8553,vac-pd-en", &(sc->cfg.vac_pd_en)},
        {"sc,nx853x,pmid-pd-en", &(sc->cfg.pmid_pd_en)},
        {"sc,nx853x,vbus-pd-en", &(sc->cfg.vbus_pd_en)},
        {"sc,nx853x,work-mode", &(sc->cfg.work_mode)},
        {"sc,nx853x,ss-timeout", &(sc->cfg.ss_timeout)},
        {"sc,nx853x,wd-timeout", &(sc->cfg.wd_timeout)},
        {"sc,nx853x,vout-ovp", &(sc->cfg.vout_ovp_th)},
        {"sc,nx853x,fsw-freq", &(sc->cfg.fsw_freq)},
        {"sc,nx853x,wd-timeout-dis", &(sc->cfg.wd_timeout_dis)},
        {"sc,nx853x,pin-diag-fall-dis", &(sc->cfg.pin_diag_fall_dis)},
        {"sc,nx853x,vout-ovp-dis", &(sc->cfg.vout_ovp_dis)},
        {"sc,nx853x,them-flt-dis", &(sc->cfg.them_flt_dis)},
        {"sc,nx853x,tshut-dis", &(sc->cfg.tshut_dis)},
        {"sc,nx853x,ibus-ucp-dis", &(sc->cfg.ibus_ucp_dis)},
        {"sc,nx853x,ibat-reg-dis", &(sc->cfg.ibat_reg_dis)},
        {"sc,nx853x,ibus-reg-dis", &(sc->cfg.ibus_reg_dis)},
        {"sc,nx853x,vbat-reg-dis", &(sc->cfg.vbat_reg_dis)},
        {"sc,nx853x,tdie-reg-dis", &(sc->cfg.tdie_reg_dis)},
        {"sc,nx853x,ibat-ocp-dg", &(sc->cfg.ibat_ocp_dg)},
        {"sc,nx853x,ibus-ucp-fall-dg", &(sc->cfg.ibus_ucp_fall_dg)},
        {"sc,nx853x,ibus-ocp-dg", &(sc->cfg.ibus_ocp_dg)},
        {"sc,nx853x,vbat-ovp-dg", &(sc->cfg.ibus_ocp_dg)},
    };

    sc->irq_gpio = of_get_named_gpio(np, "nx853x,intr_gpio", 0);
    if (!gpio_is_valid(sc->irq_gpio)) {
        dev_err(sc->dev, "no intr_gpio info\n");
        return -EINVAL;
    }

#ifdef CONFIG_MTK_CHARGER_V5P10
    if (of_property_read_string(np, "charger_name", &sc->chg_dev_name) < 0) {
        sc->chg_dev_name = "charger";
        dev_err(sc->dev,"no charger name\n");
    }
#elif defined(CONFIG_MTK_CHARGER_V4P19)
    if (of_property_read_string(np, "charger_name_v4_19", &sc->chg_dev_name) < 0) {
        sc->chg_dev_name = "charger";
        dev_err(sc->dev,"no charger name\n");
    }
#endif /*CONFIG_MTK_CHARGER_V4P19*/

    /* initialize data for optional properties */
    for (i = 0; i < ARRAY_SIZE(props); i++) {
        ret = of_property_read_u32(np, props[i].name,
                        props[i].conv_data);
        if (ret < 0) {
            dev_err(sc->dev, "can not read %s \n", props[i].name);
            continue;
        }
    }

    return ret;
}

static int nx853x_init_device(struct nx853x_chip *sc)
{
    int ret = 0;
    int i;
    struct {
        enum nx853x_fields field_id;
        int conv_data;
    } props[] = {
        {VBAT_OVP_DIS, sc->cfg.vbat_ovp_dis},
        {VBAT_OVP, sc->cfg.vbat_ovp_th},
        {IBAT_OVP_DIS, sc->cfg.ibat_ocp_dis},
        {IBAT_OCP, sc->cfg.ibat_ocp_th},
        {VBUS_OVP_DIS, sc->cfg.vbus_ovp_dis},
        {VBUS_OVP, sc->cfg.vbus_ovp_th},
        {VAC_OVP_DIS, sc->cfg.vac_ovp_dis},
        {VAC_OVP, sc->cfg.vac_ovp_th},
        {IBUS_OCP_DIS, sc->cfg.ibus_ocp_dis},
        {IBUS_OCP, sc->cfg.ibus_ocp_th},
        {PMID2OUT_OVP_DIS, sc->cfg.pmid2out_ovp_dis},
        {PMID2OUT_OVP, sc->cfg.pmid2out_ovp_th},
        {PMID2OUT_UVP_DIS, sc->cfg.pmid2out_uvp_dis},
        {PMID2OUT_UVP, sc->cfg.pmid2out_uvp_th},
        {SET_IBATREG, sc->cfg.ibatreg_th},
        {SET_IBUSREG, sc->cfg.ibusreg_th},
        {SET_VBATREG, sc->cfg.vbatreg_th},
        {SET_TDIEREG, sc->cfg.tdiereg_th},
        {THEM_FLT, sc->cfg.them_flt_th},
        {SET_IBAT_SNS_RES, sc->cfg.ibat_sns_res},
        {VAC_PD_EN, sc->cfg.vac_pd_en},
        {PMID_PD_EN, sc->cfg.pmid_pd_en},
        {VBUS_PD_EN, sc->cfg.vbus_pd_en},
        {MODE, sc->cfg.work_mode},
        {SS_TIMEOUT, sc->cfg.ss_timeout},
        {WD_TIMEOUT, sc->cfg.wd_timeout},
        {VOUT_OVP, sc->cfg.vout_ovp_th},
        {FSW_SET, sc->cfg.fsw_freq},
        {WD_TIMEOUT_DIS, sc->cfg.wd_timeout_dis},
        {PIN_DIAG_FALL_DIS, sc->cfg.pin_diag_fall_dis},
        {VOUT_OVP_DIS, sc->cfg.vout_ovp_dis},
        {THEM_FLT_DIS, sc->cfg.them_flt_dis},
        {TSHUT_DIS, sc->cfg.tshut_dis},
        {IBUS_UCP_DIS, sc->cfg.ibus_ucp_dis},
        {IBAT_REG_DIS, sc->cfg.ibat_reg_dis},
        {IBUS_REG_DIS, sc->cfg.ibus_reg_dis},
        {VBAT_REG_DIS, sc->cfg.vbat_reg_dis},
        {TDIE_REG_DIS, sc->cfg.tdie_reg_dis},
        {IBAT_OCP_DG_SET, sc->cfg.ibat_ocp_dg},
        {IBUS_UCP_FALL_DG_SET, sc->cfg.ibus_ucp_fall_dg},
        {IBUS_OCP_DG_SET, sc->cfg.ibus_ocp_dg},
        {VBAT_OVP_DG_SET, sc->cfg.ibus_ocp_dg},
    };

    ret = nx853x_reg_reset(sc);
    if (ret < 0) {
        dev_err(sc->dev, "%s Failed to reset registers(%d)\n", __func__, ret);
    }
    msleep(10);

    for (i = 0; i < ARRAY_SIZE(props); i++) {
        ret = nx853x_field_write(sc, props[i].field_id, props[i].conv_data);
    }
    /*A06 code for SR-AL7160A-01-351 by shixuanxuan at 20240407 start*/
    ret = nx853x_enable_adc(sc, true);
    if (ret < 0) {
        dev_err(sc->dev, "nx853x enable adc fail\n");
    }
    /*A06 code for SR-AL7160A-01-351 by shixuanxuan at 20240407 end*/

    return nx853x_dump_reg(sc);
}

static int nx853x_irq_register(struct nx853x_chip *sc)
{
    int ret = 0;
    struct device_node *node = sc->dev->of_node;

    if (!node) {
        dev_err(sc->dev, "device tree node missing\n");
        return -EINVAL;
    }

    sc->irq_gpio = of_get_named_gpio(node, "nx853x,intr_gpio", 0);
    if (!gpio_is_valid(sc->irq_gpio)) {
        dev_err(sc->dev, "fail to valid gpio : %d\n", sc->irq_gpio);
        return -EINVAL;
    }

/*     ret = devm_gpio_request(sc->dev, sc->irq_gpio, "nx853x,intr_gpio");
    if (ret < 0) {
        dev_err(sc->dev, "failed to request GPIO%d ; ret = %d", sc->irq_gpio, ret);
        return ret;
    }
 */
    ret = gpio_direction_input(sc->irq_gpio);
    if (ret < 0) {
        dev_err(sc->dev, "failed to set GPIO%d ; ret = %d", sc->irq_gpio, ret);
        return ret;
    }

    sc->irq = gpio_to_irq(sc->irq_gpio);
    if (ret < 0) {
        dev_err(sc->dev, "failed gpio to irq GPIO%d ; ret = %d", sc->irq_gpio, ret);
        return ret;
    }

    pr_err("nx853x register irq gpio: %d, irq: %d", sc->irq_gpio, sc->irq);

    ret = devm_request_threaded_irq(sc->dev, sc->irq, NULL,
                    nx853x_irq_handler,
                    IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                    nx853x_irq_name[sc->role], sc);
    if (ret < 0) {
        dev_err(sc->dev, "request thread irq failed:%d\n", ret);
        return ret;
    }

    enable_irq_wake(sc->irq);

    return ret;
}

static struct of_device_id nx853x_charger_match_table[] = {
    {   .compatible = "southchip,nx853x-standalone",
        .data = &nx853x_role_data[NX853X_STANDALONE], },
    {   .compatible = "southchip,nx853x-master",
        .data = &nx853x_role_data[NX853X_MASTER], },
    {   .compatible = "southchip,nx853x-slave",
        .data = &nx853x_role_data[NX853X_SLAVE], },
    {},
};

static int nx853x_charger_probe(struct i2c_client *client,
                    const struct i2c_device_id *id)
{
    struct nx853x_chip *sc;
    const struct of_device_id *match;
    struct device_node *node = client->dev.of_node;
    int ret = 0;
    int i;

    pr_err("%s (%s)\n", __func__, NX853X_DRV_VERSION);

    sc = devm_kzalloc(&client->dev, sizeof(struct nx853x_chip), GFP_KERNEL);
    if (!sc)
        return -ENOMEM;

    sc->dev = &client->dev;
    sc->client = client;

    sc->regmap = devm_regmap_init_i2c(client,
                            &nx853x_regmap_config);
    if (IS_ERR(sc->regmap)) {
        dev_err(sc->dev, "Failed to initialize regmap\n");
        return -EINVAL;
    }

    for (i = 0; i < ARRAY_SIZE(nx853x_reg_fields); i++) {
        const struct reg_field *reg_fields = nx853x_reg_fields;

        sc->rmap_fields[i] =
            devm_regmap_field_alloc(sc->dev,
                        sc->regmap,
                        reg_fields[i]);
        if (IS_ERR(sc->rmap_fields[i])) {
            dev_err(sc->dev, "cannot allocate regmap field\n");
            return PTR_ERR(sc->rmap_fields[i]);
        }
    }

    ret = nx853x_detect_device(sc);
    if (ret < 0) {
        dev_err(sc->dev, "%s detect device fail\n", __func__);
        goto err_detect_device;
    }

    i2c_set_clientdata(client, sc);
    nx853x_create_device_node(&(client->dev));

    match = of_match_node(nx853x_charger_match_table, node);
    if (match == NULL) {
        dev_err(sc->dev, "device tree match not found!\n");
        goto err_get_match;
    }

    sc->role = *(int *)match->data;

    ret = nx853x_parse_dt(sc, &client->dev);
    if (ret < 0) {
        dev_err(sc->dev, "%s parse dt failed(%d)\n", __func__, ret);
        goto err_parse_dt;
    }

    ret = nx853x_init_device(sc);
    if (ret < 0) {
        dev_err(sc->dev, "%s init device failed(%d)\n", __func__, ret);
        goto err_init_device;
    }

    ret = nx853x_psy_register(sc);
    if (ret < 0) {
        dev_err(sc->dev, "%s psy register failed(%d)\n", __func__, ret);
        goto err_psy_register;
    }

    ret = nx853x_irq_register(sc);
    if (ret < 0) {
        dev_err(sc->dev, "%s register irq fail(%d)\n",
                    __func__, ret);
        goto err_register_irq;
    }

#ifdef CONFIG_MTK_CLASS
    sc->chg_dev = charger_device_register(sc->chg_dev_name,
					      &client->dev, sc,
					      &nx853x_chg_ops,
					      &nx853x_chg_props);
	if (IS_ERR_OR_NULL(sc->chg_dev)) {
		ret = PTR_ERR(sc->chg_dev);
		dev_err(sc->dev,"Fail to register charger!\n");
        goto err_register_mtk_charger;
	}
#endif /*CONFIG_MTK_CLASS*/
    /*A06 code for AL7160A-82 by jiashixian at 20240418 start*/
    s_is_nx853x_probe_success = true;
    /*A06 code for AL7160A-82 by jiashixian at 20240418 end*/
    /* A06 code for AL7160A-445 by jiashixian at 20240610 start */
    cp_info = NX853X;
    /* A06 code for AL7160A-445 by jiashixian at 20240610 end */
    /*A06 code for AL7160A-607 by shixuanxuan at 20240507 start*/
    sc->otg_tx_mode = false;
    /*A06 code for AL7160A-607 by shixuanxuan at 20240507 end*/
    dev_err(sc->dev, "nx853x[%s] probe successfully!\n",
            sc->role == NX853X_STANDALONE ? "standalone" :
            (sc->role == NX853X_MASTER ? "master" : "slave"));
    return 0;
#ifdef CONFIG_MTK_CLASS
err_register_mtk_charger:
#endif /*CONFIG_MTK_CLASS*/
err_register_irq:
err_psy_register:
    power_supply_unregister(sc->sc_cp_psy);
err_init_device:
err_parse_dt:
err_get_match:
err_detect_device:
    dev_err(sc->dev, "nx853x probe failed!\n");
    devm_kfree(sc->dev, sc);
    return ret;
}


static int nx853x_charger_remove(struct i2c_client *client)
{
    struct nx853x_chip *sc = i2c_get_clientdata(client);

    nx853x_enable_adc(sc, false);
    power_supply_unregister(sc->sc_cp_psy);

    return 0;
}

/*A06 code for AL7160A-82 by jiashixian at 20240418 start*/
static void nx853x_charger_shutdown(struct i2c_client *client)
{
    if (s_is_nx853x_probe_success) {
        struct nx853x_chip *sc = i2c_get_clientdata(client);
        nx853x_enable_adc(sc, false);
    }
}
/*A06 code for AL7160A-82 by jiashixian at 20240418 end*/

#ifdef CONFIG_PM_SLEEP
static int nx853x_suspend(struct device *dev)
{
    struct nx853x_chip *sc = dev_get_drvdata(dev);

    dev_info(sc->dev, "Suspend successfully!");
    if (device_may_wakeup(dev))
        enable_irq_wake(sc->irq);
    disable_irq(sc->irq);
    /*A06 code for AL7160A-157 by jiashixian at 20240508 start*/
    /*A06 code for AL7160A-860 by jiashixian at 20240512 start*/
    if(!nx853x_get_vbus_present_status(sc)) {
        nx853x_enable_adc_irq_mask(sc,true);
        msleep(5);
        nx853x_enable_adc(sc, false);
    }
    /*A06 code for AL7160A-860 by jiashixian at 20240512 end*/
    /*A06 code for AL7160A-157 by jiashixian at 20240508 end*/

    return 0;
}

static int nx853x_resume(struct device *dev)
{
    struct nx853x_chip *sc = dev_get_drvdata(dev);

    dev_info(sc->dev, "Resume successfully!");
    if (device_may_wakeup(dev))
        disable_irq_wake(sc->irq);
    enable_irq(sc->irq);
    /*A06 code for AL7160A-157 by jiashixian at 20240508 start*/
    /*A06 code for AL7160A-860 by jiashixian at 20240509 start*/
    nx853x_enable_adc(sc, true);
    nx853x_enable_adc_irq_mask(sc,false);
    /*A06 code for AL7160A-860 by jiashixian at 20240509 end*/
    /*A06 code for AL7160A-157 by jiashixian at 20240508 end*/

    return 0;
}

static const struct dev_pm_ops nx853x_pm_ops = {
    .resume		= nx853x_resume,
    .suspend	= nx853x_suspend,
};
#endif /*CONFIG_PM_SLEEP*/
/*A06 code for AL7160A-82 by jiashixian at 20240418 start*/
static struct i2c_driver nx853x_charger_driver = {
    .driver     = {
        .name   = "nx853x-charger",
        .owner  = THIS_MODULE,
        .of_match_table = nx853x_charger_match_table,
#ifdef CONFIG_PM_SLEEP
        .pm = &nx853x_pm_ops,
#endif
    },
    .probe      = nx853x_charger_probe,
    .remove     = nx853x_charger_remove,
    .shutdown   = nx853x_charger_shutdown,
};
/*A06 code for AL7160A-82 by jiashixian at 20240418 end*/

module_i2c_driver(nx853x_charger_driver);

MODULE_DESCRIPTION("SC nx853x Charge Pump Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("South Chip <Aiden-yu@southchip.com>");