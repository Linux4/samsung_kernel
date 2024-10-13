// SPDX-License-Identifier: GPL-2.0
// SGM41606 driver version 2023-1015-001
// Copyright (C) 2023 sgmicro - http://www.sg-micro.com

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
/*A06 code for AL7160A-445 by jiashixian at 20240610 start*/
#include <linux/chg-tcpc_info.h>
/*A06 code for AL7160A-445 by jiashixian at 20240610 end*/
#define CONFIG_MTK_CHARGER_V4P19 1//lzk

#define SGM41606S_DEVICE_ID   1
#define SGM41606S_REGMAX      0x38
#define SGM41606S_ADC_REG25       0x25

#define SGM41606S_IBAT_OCP_MIN_uA 2000000
#define SGM41606S_IBAT_OCP_MAX_uA 11200000
#define SGM41606S_IBAT_STEP_1_uA  100000
#define SGM41606S_IBAT_STEP_2_uA  300000
static bool g_is_sgm41606s_prob_success = false;

enum {
    SGM41606S_STANDALONG = 0,
    SGM41606S_MASTER,
    SGM41606S_SLAVE,
};

// static const char* sgm41606S_psy_name[] = {
//     [SGM41606S_STANDALONG] = "sgm-cp-standalone",
//     [SGM41606S_MASTER] = "sgm-cp-master",
//     [SGM41606S_SLAVE] = "sgm-cp-slave",
// };

static const char* sgm41606S_irq_name[] = {
    [SGM41606S_STANDALONG] = "sgm41606S-standalone-irq",
    [SGM41606S_MASTER] = "sgm41606S-master-irq",
    [SGM41606S_SLAVE] = "sgm41606S-slave-irq",
};

static int sgm41606S_mode_data[] = {
    [SGM41606S_STANDALONG] = SGM41606S_STANDALONG,
    [SGM41606S_MASTER] = SGM41606S_MASTER,
    [SGM41606S_SLAVE] = SGM41606S_SLAVE,
};

enum {
    ADC_IBUS = 0,
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
}SGM41606S_ADC_CH;

enum sgm41606S_notify {
    SGM41606S_NOTIFY_OTHER = 0,
    SGM41606S_NOTIFY_IBUSOCP,
    SGM41606S_NOTIFY_VBUSOVP,
    SGM41606S_NOTIFY_IBATOCP,
    SGM41606S_NOTIFY_VBATOVP,
    SGM41606S_NOTIFY_TDIE,
    SGM41606S_NOTIFY_VAC_OVP,
    SGM41606S_NOTIFY_VOUT_OVP,

};

enum sgm41606S_error_stata {
    ERROR_VBUS_HIGH = 0,
    ERROR_VBUS_LOW,
    ERROR_VBUS_OVP,
    ERROR_IBUS_OCP,
    ERROR_VBAT_OVP,
    ERROR_IBAT_OCP,
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
    { .reg = 0x18, .len = 8, .bit = {
            {.mask = BIT(7), .name = "vbat ovp flag",       .notify = SGM41606S_NOTIFY_VBATOVP},
            {.mask = BIT(6), .name = "vbat ovp alm flag",   .notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(5), .name = "vout ovp flag",       .notify = SGM41606S_NOTIFY_VOUT_OVP},
            {.mask = BIT(4), .name = "ibat ocp flag",       .notify = SGM41606S_NOTIFY_IBATOCP},
            {.mask = BIT(3), .name = "ibat ocp alm flag",   .notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(2), .name = "ibat ucp alm flag",   .notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(1), .name = "bus ovp flag",          .notify = SGM41606S_NOTIFY_VBUSOVP},
            {.mask = BIT(0), .name = "bus ovp alm flag",    .notify = SGM41606S_NOTIFY_OTHER},
        },
    },
    { .reg = 0x19, .len = 6, .bit = {
            {.mask = BIT(7), .name = "bus ocp flag",         .notify = SGM41606S_NOTIFY_IBUSOCP},
            {.mask = BIT(6), .name = "bus ocp alm flag",    .notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(5), .name = "bus ucp flag",        .notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(4), .name = "bus rcp flag",        .notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(3), .name = "bus scp flag",           .notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(2), .name = "cfly short flag",     .notify = SGM41606S_NOTIFY_OTHER},
        },
    },
    { .reg = 0x1A, .len = 8, .bit = {
            {.mask = BIT(7), .name = "vac1 ovp flag",         .notify = SGM41606S_NOTIFY_VAC_OVP},
            {.mask = BIT(6), .name = "vac2 ovp flag",       .notify = SGM41606S_NOTIFY_VAC_OVP},
            {.mask = BIT(5), .name = "vout present flag",   .notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(4), .name = "vac1 present flag",   .notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(3), .name = "vac2 present flag",   .notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(2), .name = "vbus present flag",   .notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(1), .name = "acrb1 config flag",      .notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(0), .name = "acrb2 config flag",   .notify = SGM41606S_NOTIFY_OTHER},
        },
    },

    { .reg = 0x1B, .len = 8, .bit = {
            {.mask = BIT(7), .name = "adc done flag",         .notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(6), .name = "sstimeout flag",       .notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(5), .name = "tsbus tsbat alm flag",.notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(4), .name = "tsbus flt flag",      .notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(3), .name = "tsbat flt flag",       .notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(2), .name = "tdie flt flag",       .notify = SGM41606S_NOTIFY_TDIE},
            {.mask = BIT(1), .name = "tdie alm flag",          .notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(0), .name = "wdt flag",            .notify = SGM41606S_NOTIFY_OTHER},
        },
    },

    { .reg = 0x1C, .len = 8, .bit = {
            {.mask = BIT(7), .name = "regn good flag",         .notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(6), .name = "convert active flag", .notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(5), .name = "vbus errlo flag",        .notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(4), .name = "vbus errhi flag",     .notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(3), .name = "pmid2vout ovp flag",  .notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(2), .name = "pmid2vout uvp flag",  .notify = SGM41606S_NOTIFY_TDIE},
            {.mask = BIT(1), .name = "conv ocp flag",          .notify = SGM41606S_NOTIFY_OTHER},
            {.mask = BIT(0), .name = "cfl rvs ocp flag",    .notify = SGM41606S_NOTIFY_OTHER},
        },
    },
};

enum sgm41606S_reg_range{
    SGM41606S_VBAT_OVP,
    SGM41606S_VBAT_OVP_ALM,
    SGM41606S_IBAT_OCP_ALM,
    SGM41606S_IBAT_UCP_ALM,
    SGM41606S_VBUS_OVP,
    SGM41606S_VBUS_OVP_ALM,
    SGM41606S_IBUS_OCP,
    SGM41606S_IBUS_OCP_ALM,
    SGM41606S_TDIE_FLT,
    SGM41606S_VOUT_OVP,
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

#define SGM41606S_CHG_RANGE(_min, _max, _step, _offset, _ru) \
{ \
    .min = _min, \
    .max = _max, \
    .step = _step, \
    .offset = _offset, \
    .round_up = _ru, \
}

#define SGM41606S_CHG_RANGE_T(_table, _ru) \
    { .table = _table, .num_table = ARRAY_SIZE(_table), .round_up = _ru, }


static const struct reg_range sgm41606S_reg_range[] = {
    [SGM41606S_VBAT_OVP]              = SGM41606S_CHG_RANGE(3500,  4770,  10, 3500, false),
    [SGM41606S_VBAT_OVP_ALM]          = SGM41606S_CHG_RANGE(3500,  4770,  10, 3500, false),
    [SGM41606S_IBAT_OCP_ALM]          = SGM41606S_CHG_RANGE(0,    12700,  10, 0, false),
    [SGM41606S_IBAT_UCP_ALM]          = SGM41606S_CHG_RANGE(0,     4500,  50, 0, false),
    [SGM41606S_VBUS_OVP]              = SGM41606S_CHG_RANGE(7000, 12750,  50, 7000, false),
    [SGM41606S_VBUS_OVP_ALM]          = SGM41606S_CHG_RANGE(7000, 13350,  50, 7000, false),
    [SGM41606S_IBUS_OCP]              = SGM41606S_CHG_RANGE(1000,  6500,  250, 1000, false),
    [SGM41606S_IBUS_OCP_ALM]          = SGM41606S_CHG_RANGE(1000,  8750,  250, 1000, false),
    [SGM41606S_TDIE_FLT]             = SGM41606S_CHG_RANGE(80,     140,  20, 80, false),
    //[SGM41606S_TDIE_ALM]             = SGM41606S_CHG_RANGE(250,     1500,  50, 250, false),
    [SGM41606S_VOUT_OVP]              = SGM41606S_CHG_RANGE(4700,  5000,  100, 4700, false),
};

static const unsigned int WDT_TIME_mS[] = {
    500, 1000, 5000, 30000
};

static const unsigned int VACOVP_STABLE[] = {
    6500, 10500, 12000, 14000, 16000, 18000
};

static const unsigned int VOUTOVP_STABLE[] = {
    4700, 4800, 4900, 5000
};

static const unsigned int FSW_SET_STABLE[] = {
    187, 250, 300, 375,500,750,1000,1500
};

static const unsigned int SS_TIMEOUT_uS[] = {
    6250, 12500, 25000, 50000,100000,400000,1500000,10000000
};

static const unsigned int IBUSUCP_FALL_DG_SEL_uS[] = {
    10, 5000, 50000, 150000
};

enum SGM41606_BIT_OPTION {
    /* REG 00~04*/
    B_BATOVP_DIS,B_BATOVP,
    B_BATOVP_ALM_DIS,B_BATOVP_ALM,
    B_BATOCP_DIS,B_BATOCP,
    B_BATOCP_ALM_DIS,B_BATOCP_ALM,
    B_BATUCP_ALM_DIS,B_BATUCP_ALM,
    /* REG 05*/
    B_BUSUCP_DIS,B_BUSUCP,B_BUSRCP_DIS,B_BUSRCP,B_VBUS_ERRLO_DIS,B_VBUS_ERRHI_DIS,

    /* REG 06~0A*/
    B_BUS_PD_EN,B_BUSOVP,
    B_BUSOVP_ALM_DIS,B_BUSOVP_ALM,
    B_BUSOCP,
    B_BUSOCP_ALM_DIS,B_BUSOCP_ALM,
    B_TDIE_FLT_DIS,B_TDIE_FLT,B_TDIE_ALM_DIS,B_TSBUS_FLT_DIS,B_TSBAT_FLT_DIS,
    B_TDIE_ALM,B_TSBUS_FLT,B_TSBAT_FLT,
    /* REG 0E*/
    B_VAC1OVP,B_VAC2OVP,B_VAC1_PD_EN,B_VAC2_PD_EN,
    /* REG 0F*/
    B_REG_RST,B_EN_HIZ,B_EN_OTG,B_CHG_EN,B_EN_BYPASS,B_DIS_ACDRV_BOTH,B_ACDRV1_STAT,B_ACDRV2_STAT,

    /* REG 010*/
    B_FSW_SET,B_WDT,B_WDT_DIS,B_CONV_OCP_TIME_SEL,B_CONV_OCP_DIS,
    B_RSNS,B_SS_TIMEOUT,B_IBUSUCP_FALL_DG_SEL,
    B_VOUTOVP_DIS,B_VOUTOVP,B_FREQ_SHIFT,B_FSW_SET_HI,B_MS,
    B_BATOVP_STAT,B_BATOVP_ALM_STAT,B_VOUTOVP_STAT,B_BATOCP_STAT,B_BATOCP_ALM_STAT,B_BATUCP_ALM_STAT,B_BUSOVP_STAT,B_BUSOVP_ALM_STAT,
    B_BUSOCP_STAT,B_BUSOCP_ALM_STAT,B_BUSUCP_STAT,B_BUSRCP_STAT,B_BUSSCP_STAT,B_CFLY_SHORT_STAT,
    B_VAC1OVP_STAT,B_VAC2OVP_STAT,B_VOUTPRESENT_STAT,B_VAC1PRESENT_STAT,B_VAC2PRESENT_STAT,B_VBUSPRESENT_STAT,B_ACRB1_CONFIG_STAT,B_ACRB2_CONFIG_STAT,
    B_ADC_DONE_STAT,B_SS_TIMEOUT_STAT,B_TSBUS_TSBAT_ALM_STAT,B_TSBUS_FLT_STAT,B_TSBAT_FLT_STAT,B_TDIE_FLT_STAT,B_TDIE_ALM_STAT,B_WD_STAT,
    B_REGN_GOOD_STAT,B_CONV_ACTIVE_STAT,B_VBUS_ERRLO_STAT,B_VBUS_ERRHI_STAT,B_PMID2VOUT_OVP_STAT,B_PMID2VOUT_UVP_STAT,B_CONV_OCP_STAT,B_CFL_RVS_OCP_STAT,

    /* REG 1D*/
    B_BATOVP_MASK,B_BATOVP_ALM_MASK,B_VOUTOVP_MASK,B_BATOCP_MASK,B_BATOCP_ALM_MASK,B_BATUCP_ALM_MASK,B_BUSOVP_MASK,B_BUSOVP_ALM_MASK,
    B_BUSOCP_MASK,B_BUSOCP_ALM_MASK,B_BUSUCP_MASK,B_BUSRCP_MASK,B_BUSSCP_MASK,B_CFLY_SHORT_MASK,
    B_VAC1OVP_MASK,B_VAC2OVP_MASK,B_VOUTPRESENT_MASK,B_VAC1PRESENT_MASK,B_VAC2PRESENT_MASK,B_VBUSPRESENT_MASK,B_ACRB1_CONFIG_MASK,B_ACRB2_CONFIG_MASK,
    B_ADC_DONE_MASK,B_SS_TIMEOUT_MASK,B_TSBUS_TSBAT_ALM_MASK,B_TSBUS_FLT_MASK,B_TSBAT_FLT_MASK,B_TDIE_FLT_MASK,B_TDIE_ALM_MASK,B_WD_MASK,
    B_REGN_GOOD_MASK,B_CONV_ACTIVE_MASK,B_VBUS_ERRLO_MASK,B_VBUS_ERRHI_MASK,B_PMID2VOUTOVP_MASK,B_PMID2VOUTUVP_MASK,B_CONV_OCP_MASK,B_CFL_RVS_OCP_MASK,
    /* REG 22*/
    B_DEVICE_REV,B_DEVICE_ID,
    B_ADC_EN,B_ADC_RATE,B_ADC_AVG,B_ADC_AVG_INIT,B_ADC_SAMPLE,B_IBUS_ADC_DIS,B_VBUS_ADC_DIS,
    B_VAC1_ADC_DIS,B_VAC2_ADC_DIS,B_VOUT_ADC_DIS,B_VBAT_ADC_DIS,B_IBAT_ADC_DIS,B_TSBUS_ADC_DIS,B_TSBAT_ADC_DIS,B_TDIE_ADC_DIS,
    B_BIT_MAX,
};

static const struct reg_field sgm41606S_regs[B_BIT_MAX] = {
    [B_BATOVP_DIS]            = REG_FIELD(0x00, 7, 7),
    [B_BATOVP]                  = REG_FIELD(0x00, 0, 6),
    [B_BATOVP_ALM_DIS]        = REG_FIELD(0x01, 7, 7),
    [B_BATOVP_ALM]            = REG_FIELD(0x01, 0, 6),
    [B_BATOCP_DIS]            = REG_FIELD(0x02, 7, 7),
    [B_BATOCP]                = REG_FIELD(0x02, 0, 6),
    [B_BATOCP_ALM_DIS]        = REG_FIELD(0x03, 7, 7),
    [B_BATOCP_ALM]            = REG_FIELD(0x03, 0, 6),
    [B_BATUCP_ALM_DIS]        = REG_FIELD(0x04, 7, 7),
    [B_BATUCP_ALM]            = REG_FIELD(0x04, 0, 6),
    [B_BUSUCP_DIS]            = REG_FIELD(0x05, 7, 7),
    [B_BUSUCP]                = REG_FIELD(0x05, 6, 6),
    [B_BUSRCP_DIS]            = REG_FIELD(0x05, 5, 5),
    [B_BUSRCP]                = REG_FIELD(0x05, 4, 4),
    [B_VBUS_ERRLO_DIS]        = REG_FIELD(0x05, 3, 3),
    [B_VBUS_ERRHI_DIS]        = REG_FIELD(0x05, 2, 2),
    [B_BUS_PD_EN]             = REG_FIELD(0x06, 7, 7),
    [B_BUSOVP]               = REG_FIELD(0x06, 0, 6),

    [B_BUSOVP_ALM_DIS]        = REG_FIELD(0x07, 7, 7),
    [B_BUSOVP_ALM]            = REG_FIELD(0x07, 0, 6),
    [B_BUSOCP]                = REG_FIELD(0x08, 0, 4),
    [B_BUSOCP_ALM_DIS]        = REG_FIELD(0x09, 7, 7),
    [B_BUSOCP_ALM]            = REG_FIELD(0x09, 0, 4),

    [B_TDIE_FLT_DIS]          = REG_FIELD(0x0A, 7, 7),
    [B_TDIE_FLT]              = REG_FIELD(0x0A, 5, 6),
    [B_TDIE_ALM_DIS]          = REG_FIELD(0x0A, 4, 4),
    [B_TSBUS_FLT_DIS]         = REG_FIELD(0x0A, 3, 3),
    [B_TSBAT_FLT_DIS]         = REG_FIELD(0x0A, 2, 2),

    [B_TDIE_ALM]              = REG_FIELD(0x0A, 0, 7),
    [B_TSBUS_FLT]             = REG_FIELD(0x0A, 0, 7),
    [B_TSBAT_FLT]             = REG_FIELD(0x0A, 0, 7),

    [B_VAC1OVP]                  = REG_FIELD(0x0E, 5, 7),
    [B_VAC2OVP]                 = REG_FIELD(0x0E, 2, 4),
    [B_VAC1_PD_EN]            = REG_FIELD(0x0E, 1, 1),
    [B_VAC2_PD_EN]            = REG_FIELD(0x0E, 0, 0),

    [B_REG_RST]               = REG_FIELD(0x0F, 7, 7),
    [B_EN_HIZ]                = REG_FIELD(0x0F, 6, 6),
    [B_EN_OTG]                = REG_FIELD(0x0F, 5, 5),
    [B_CHG_EN]                = REG_FIELD(0x0F, 4, 4),
    [B_EN_BYPASS]             = REG_FIELD(0x0F, 3, 3),
    [B_DIS_ACDRV_BOTH]        = REG_FIELD(0x0F, 2, 2),
    [B_ACDRV1_STAT]           = REG_FIELD(0x0F, 1, 1),
    [B_ACDRV2_STAT]           = REG_FIELD(0x0F, 0, 0),

    [B_FSW_SET]               = REG_FIELD(0x10, 5, 7),
    [B_WDT]                   = REG_FIELD(0x10, 3, 4),
    [B_WDT_DIS]               = REG_FIELD(0x10, 2, 2),
    [B_CONV_OCP_TIME_SEL]     = REG_FIELD(0x10, 1, 1),
    [B_CONV_OCP_DIS]          = REG_FIELD(0x10, 0, 0),

    [B_RSNS]                  = REG_FIELD(0x11, 7, 7),
    [B_SS_TIMEOUT]            = REG_FIELD(0x11, 4, 6),
    [B_IBUSUCP_FALL_DG_SEL]   = REG_FIELD(0x11, 2, 3),

    [B_VOUTOVP_DIS]           = REG_FIELD(0x12, 7, 7),
    [B_VOUTOVP]               = REG_FIELD(0x12, 5, 6),
    [B_FREQ_SHIFT]            = REG_FIELD(0x12, 3, 4),
    [B_FSW_SET_HI]            = REG_FIELD(0x12, 2, 2),
    [B_MS]                      = REG_FIELD(0x12, 0, 1),

    [B_BATOVP_STAT]           = REG_FIELD(0x13, 7, 7),
    [B_BATOVP_ALM_STAT]       = REG_FIELD(0x13, 6, 6),
    [B_VOUTOVP_STAT]          = REG_FIELD(0x13, 5, 5),
    [B_BATOCP_STAT]           = REG_FIELD(0x13, 4, 4),
    [B_BATOCP_ALM_STAT]       = REG_FIELD(0x13, 3, 3),
    [B_BATUCP_ALM_STAT]       = REG_FIELD(0x13, 2, 2),
    [B_BUSOVP_STAT]           = REG_FIELD(0x13, 1, 1),
    [B_BUSOVP_ALM_STAT]       = REG_FIELD(0x13, 0, 0),

    [B_BUSOCP_STAT]           = REG_FIELD(0x14, 7, 7),
    [B_BUSOCP_ALM_STAT]       = REG_FIELD(0x14, 6, 6),
    [B_BUSUCP_STAT]           = REG_FIELD(0x14, 5, 5),
    [B_BUSRCP_STAT]           = REG_FIELD(0x14, 4, 4),
    [B_BUSSCP_STAT]           = REG_FIELD(0x14, 3, 3),
    [B_CFLY_SHORT_STAT]       = REG_FIELD(0x14, 2, 2),

    [B_VAC1OVP_STAT]          = REG_FIELD(0x15, 7, 7),
    [B_VAC2OVP_STAT]          = REG_FIELD(0x15, 6, 6),
    [B_VOUTPRESENT_STAT]      = REG_FIELD(0x15, 5, 5),
    [B_VAC1PRESENT_STAT]      = REG_FIELD(0x15, 4, 4),
    [B_VAC2PRESENT_STAT]      = REG_FIELD(0x15, 3, 3),
    [B_VBUSPRESENT_STAT]      = REG_FIELD(0x15, 2, 2),
    [B_ACRB1_CONFIG_STAT]     = REG_FIELD(0x15, 1, 1),
    [B_ACRB2_CONFIG_STAT]     = REG_FIELD(0x15, 0, 0),

    [B_ADC_DONE_STAT]         = REG_FIELD(0x16, 7, 7),
    [B_SS_TIMEOUT_STAT]       = REG_FIELD(0x16, 6, 6),
    [B_TSBUS_TSBAT_ALM_STAT]  = REG_FIELD(0x16, 5, 5),
    [B_TSBUS_FLT_STAT]        = REG_FIELD(0x16, 4, 4),
    [B_TSBAT_FLT_STAT]        = REG_FIELD(0x16, 3, 3),
    [B_TDIE_FLT_STAT]         = REG_FIELD(0x16, 2, 2),
    [B_TDIE_ALM_STAT]         = REG_FIELD(0x16, 1, 1),
    [B_WD_STAT]               = REG_FIELD(0x16, 0, 0),

    [B_REGN_GOOD_STAT]         = REG_FIELD(0x17, 7, 7),
    [B_CONV_ACTIVE_STAT]       = REG_FIELD(0x17, 6, 6),
    [B_VBUS_ERRLO_STAT]         = REG_FIELD(0x17, 5, 5),
    [B_VBUS_ERRHI_STAT]        = REG_FIELD(0x17, 4, 4),
    [B_PMID2VOUT_OVP_STAT]     = REG_FIELD(0x17, 3, 3),
    [B_PMID2VOUT_UVP_STAT]     = REG_FIELD(0x17, 2, 2),
    [B_CONV_OCP_STAT]          = REG_FIELD(0x17, 1, 1),
    [B_CFL_RVS_OCP_STAT]       = REG_FIELD(0x17, 0, 0),

    [B_BATOVP_MASK]           = REG_FIELD(0x1D, 7, 7),
    [B_BATOVP_ALM_MASK]       = REG_FIELD(0x1D, 6, 6),
    [B_VOUTOVP_MASK]          = REG_FIELD(0x1D, 5, 5),
    [B_BATOCP_MASK]           = REG_FIELD(0x1D, 4, 4),
    [B_BATOCP_ALM_MASK]       = REG_FIELD(0x1D, 3, 3),
    [B_BATUCP_ALM_MASK]       = REG_FIELD(0x1D, 2, 2),
    [B_BUSOVP_MASK]           = REG_FIELD(0x1D, 1, 1),
    [B_BUSOVP_ALM_MASK]       = REG_FIELD(0x1D, 0, 0),

    [B_BUSOCP_MASK]           = REG_FIELD(0x1E, 7, 7),
    [B_BUSOCP_ALM_MASK]       = REG_FIELD(0x1E, 6, 6),
    [B_BUSUCP_MASK]           = REG_FIELD(0x1E, 5, 5),
    [B_BUSRCP_MASK]           = REG_FIELD(0x1E, 4, 4),
    [B_BUSSCP_MASK]           = REG_FIELD(0x1E, 3, 3),
    [B_CFLY_SHORT_MASK]       = REG_FIELD(0x1E, 2, 2),

    [B_VAC1OVP_MASK]          = REG_FIELD(0x1F, 7, 7),
    [B_VAC2OVP_MASK]          = REG_FIELD(0x1F, 6, 6),
    [B_VOUTPRESENT_MASK]      = REG_FIELD(0x1F, 5, 5),
    [B_VAC1PRESENT_MASK]      = REG_FIELD(0x1F, 4, 4),
    [B_VAC2PRESENT_MASK]      = REG_FIELD(0x1F, 3, 3),
    [B_VBUSPRESENT_MASK]      = REG_FIELD(0x1F, 2, 2),
    [B_ACRB1_CONFIG_MASK]     = REG_FIELD(0x1F, 1, 1),
    [B_ACRB2_CONFIG_MASK]     = REG_FIELD(0x1F, 0, 0),

    [B_ADC_DONE_MASK]         = REG_FIELD(0x20, 7, 7),
    [B_SS_TIMEOUT_MASK]       = REG_FIELD(0x20, 6, 6),
    [B_TSBUS_TSBAT_ALM_MASK]  = REG_FIELD(0x20, 5, 5),
    [B_TSBUS_FLT_MASK]        = REG_FIELD(0x20, 4, 4),
    [B_TSBAT_FLT_MASK]        = REG_FIELD(0x20, 3, 3),
    [B_TDIE_FLT_MASK]         = REG_FIELD(0x20, 2, 2),
    [B_TDIE_ALM_MASK]         = REG_FIELD(0x20, 1, 1),
    [B_WD_MASK]               = REG_FIELD(0x20, 0, 0),

    [B_REGN_GOOD_MASK]        = REG_FIELD(0x21, 7, 7),
    [B_CONV_ACTIVE_MASK]      = REG_FIELD(0x21, 6, 6),
    [B_VBUS_ERRLO_MASK]       = REG_FIELD(0x21, 5, 5),
    [B_VBUS_ERRHI_MASK]       = REG_FIELD(0x21, 4, 4),
    [B_PMID2VOUTOVP_MASK]     = REG_FIELD(0x21, 3, 3),
    [B_PMID2VOUTUVP_MASK]    = REG_FIELD(0x21, 2, 2),
    [B_CONV_OCP_MASK]         = REG_FIELD(0x21, 1, 1),
    [B_CFL_RVS_OCP_MASK]      = REG_FIELD(0x21, 0, 0),

    [B_DEVICE_REV]            = REG_FIELD(0x22, 4, 7),
    [B_DEVICE_ID]             = REG_FIELD(0x22, 0, 3),

    [B_ADC_EN]                = REG_FIELD(0x23, 7, 7),
    [B_ADC_RATE]              = REG_FIELD(0x23, 6, 6),
    [B_ADC_AVG]               = REG_FIELD(0x23, 5, 5),
    [B_ADC_AVG_INIT]          = REG_FIELD(0x23, 4, 4),
    [B_ADC_SAMPLE]            = REG_FIELD(0x23, 2, 3),
    [B_IBUS_ADC_DIS]          = REG_FIELD(0x23, 1, 1),
    [B_VBUS_ADC_DIS]          = REG_FIELD(0x23, 0, 0),

    [B_VAC1_ADC_DIS]          = REG_FIELD(0x24, 7, 7),
    [B_VAC2_ADC_DIS]          = REG_FIELD(0x24, 6, 6),
    [B_VOUT_ADC_DIS]          = REG_FIELD(0x24, 5, 5),
    [B_VBAT_ADC_DIS]          = REG_FIELD(0x24, 4, 4),
    [B_IBAT_ADC_DIS]          = REG_FIELD(0x24, 3, 3),
    [B_TSBUS_ADC_DIS]         = REG_FIELD(0x24, 2, 2),
    [B_TSBAT_ADC_DIS]         = REG_FIELD(0x24, 1, 1),
    [B_TDIE_ADC_DIS]          = REG_FIELD(0x24, 0, 0),

};

static const struct regmap_config sgm41606S_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,

    .max_register = SGM41606S_REGMAX,
};

struct sgm41606S_cfg_e {
    int vbat_ovp_dis;
    int vbat_ovp;
    int vbat_ovp_alm;
    int ibat_ocp_dis;
    int ibat_ocp;
    int ibat_ocp_alm;
    int vac_ovp_en;
    int vac_ovp;
    int vac2_ovp;
    int vbus_ovp_en;
    int vbus_ovp;
    int vbus_ovp_alm;
    int tdie_flt_dis;
    int vbus_ucp_dis;
    int vbus_ucp;
    int tsbus_flt_dis;
    int tsbat_flt_dis;
    int ibus_ocp;
    int ibat_ucp_alm_dis;
    int ibat_ucp_alm;
    int vbus_low_dis;
    int vbus_hi_dis;
    int vbus_rcp_dis;
    int fsw_set;
    int wdt_dis;
    int wd_timeout;
    int chg_en;
    int mode;
    int rsns;
    int voutovp_dis;
    int enable_adc;
    int enable_vac1_adc;

};

struct sgm41606S_chip {
    struct device *dev;
    struct i2c_client *client;
    struct regmap *regmap;
    struct regmap_field *rmap_fields[B_BIT_MAX];

    struct sgm41606S_cfg_e cfg;
    int irq_gpio;
    int lpm_gpio;
    int irq;

    int mode;

    bool charge_enabled;
    int usb_present;
    int vbus_volt;
    int ibus_curr;
    int vbat_volt;
    int ibat_curr;
    int die_temp;

    const char *chg_dev_name;

    struct power_supply_desc psy_desc;
    struct power_supply_config psy_cfg;
    struct power_supply *psy;
    /*A06 code for AL7160A-607 by shixuanxuan at 20240507 start*/
    bool otg_tx_mode;
    /*A06 code for AL7160A-607 by shixuanxuan at 20240507 end*/
};



/********************COMMON API***********************/
__maybe_unused static u8 val2reg(enum sgm41606S_reg_range id, u32 val)
{
    int i;
    u8 reg;
    const struct reg_range *range= &sgm41606S_reg_range[id];

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

__maybe_unused static u32 reg2val(enum sgm41606S_reg_range id, u8 reg)
{
    const struct reg_range *range= &sgm41606S_reg_range[id];
    if (!range)
        return reg;
    return range->table ? range->table[reg] :
                  range->offset + range->step * reg;
}
/*********************************************************/
static int sgm41606S_field_read(struct sgm41606S_chip *sgm,
                enum SGM41606_BIT_OPTION field_id, int *val)
{
    int ret;

    ret = regmap_field_read(sgm->rmap_fields[field_id], val);
    if (ret < 0) {
        dev_err(sgm->dev, "sgm41606S read field %d fail: %d\n", field_id, ret);
    }

    return ret;
}

static int sgm41606S_field_write(struct sgm41606S_chip *sgm,
                enum SGM41606_BIT_OPTION field_id, int val)
{
    int ret;

    ret = regmap_field_write(sgm->rmap_fields[field_id], val);
    if (ret < 0) {
        dev_err(sgm->dev, "sgm41606S read field %d fail: %d\n", field_id, ret);
    }

    return ret;
}

static int sgm41606S_read_block(struct sgm41606S_chip *sgm,
                int reg, uint8_t *val, int len)
{
    int ret;

    ret = regmap_bulk_read(sgm->regmap, reg, val, len);
    if (ret < 0) {
        dev_err(sgm->dev, "sgm41606S read %02x block failed %d\n", reg, ret);
    }

    return ret;
}

/*******************************************************/
__maybe_unused static int sgm41606S_detect_device(struct sgm41606S_chip *sgm)
{
    int ret;
    int val;

    ret = sgm41606S_field_read(sgm, B_DEVICE_ID, &val);
    if (ret < 0) {
        dev_err(sgm->dev, "%s fail(%d)\n", __func__, ret);
        return ret;
    }

    if (val != SGM41606S_DEVICE_ID) {
        dev_err(sgm->dev, "%s not find SGM41606S, ID = 0x%02x\n", __func__, ret);
        return -EINVAL;
    }

    return ret;
}

__maybe_unused static int sgm41606S_reg_reset(struct sgm41606S_chip *sgm)
{
    return sgm41606S_field_write(sgm, B_REG_RST, 1);
}

__maybe_unused static int sgm41606S_dump_reg(struct sgm41606S_chip *sgm)
{
    int ret;
    int i;
    int val;

    for (i = 0; i <= SGM41606S_REGMAX; i++) {
        ret = regmap_read(sgm->regmap, i, &val);
        dev_err(sgm->dev, "%s reg[0x%02x] = 0x%02x\n",
                __func__, i, val);
    }

    return ret;
}

__maybe_unused static int sgm41606S_enable_charge(struct sgm41606S_chip *sgm, bool en)
{
    int ret;
    dev_info(sgm->dev,"%s:%d",__func__,en);

    if (en)
        ret = sgm41606S_field_write(sgm, B_CHG_EN, 1);
    else
        ret = sgm41606S_field_write(sgm, B_CHG_EN, 0);

    return ret;
}

__maybe_unused static int sgm41606S_check_charge_enabled(struct sgm41606S_chip *sgm, bool *enabled)
{
    int ret, val;

    ret = sgm41606S_field_read(sgm, B_CHG_EN, &val);

    if (val == 1)
        *enabled = true;
    else
        *enabled = false;

    dev_info(sgm->dev,"%s:%d", __func__, val);

    return ret;
}

__maybe_unused static int sgm41606S_get_status(struct sgm41606S_chip *sgm, uint32_t *status)
{
    int ret, val;
    *status = 0;

    ret = sgm41606S_read_block(sgm,0x17,(uint8_t *)&val,1);
    if (ret < 0) {
        dev_err(sgm->dev, "%s fail to read  FLT_FLAG2\n", __func__);
        return ret;
    }
    if (val & BIT(4))
        *status |= BIT(ERROR_VBUS_HIGH);


    if (val & BIT(5))
        *status |= BIT(ERROR_VBUS_LOW);

    return ret;

}

__maybe_unused static int sgm41606S_enable_adc(struct sgm41606S_chip *sgm, bool en)
{
    dev_info(sgm->dev,"%s:%d", __func__, en);
    return sgm41606S_field_write(sgm, B_ADC_EN, !!en);
}

__maybe_unused static int sgm41606S_set_adc_scanrate(struct sgm41606S_chip *sgm, bool oneshot)
{
    dev_info(sgm->dev,"%s:%d",__func__,oneshot);
    return sgm41606S_field_write(sgm, B_ADC_RATE, !!oneshot);
}

static int sgm41606S_get_adc_data(struct sgm41606S_chip *sgm,
            int channel, int *result)
{
    uint8_t val[2] = {0};
    int ret;
    int step = 0;
    uint16_t temp = 0;
    if(channel >= ADC_MAX_NUM)
        return -EINVAL;

    ret = sgm41606S_read_block(sgm, SGM41606S_ADC_REG25 + (channel << 1), val, 2);
    if (ret < 0) {
        return ret;
    }

    switch(channel)
    {
        case ADC_IBUS:
        case ADC_VBUS:
        case ADC_VAC1:
        case ADC_VAC2:
        case ADC_VOUT:
        case ADC_VBAT:
        case ADC_IBAT:
            step = 1;
            *result = (val[1] | (val[0] << 8)) * step;
            break;
        case ADC_TSBUS:
        case ADC_TSBAT:
            step = 9766;
            *result = (val[1] | (val[0] << 8)) * step/100000;
            break;
        case ADC_TDIE:
            step = 5;
            temp = (val[1] | (val[0] << 8)) & 0xFFFF;
            if (val[0] & 0x80)
                *result = -((~(temp - 1)) & 0x7FFF) * step /10;
            else
                *result = temp * step/10;
            break;

        default:
            break;
    }

    dev_info(sgm->dev,"%s %d %d", __func__, channel, *result);


    return ret;
}

__maybe_unused static int sgm41606S_set_busovp_th(struct sgm41606S_chip *sgm, int threshold)
{
    dev_info(sgm->dev,"%s:%d-%#x", __func__, threshold, val2reg(SGM41606S_VBUS_OVP,threshold));
    return sgm41606S_field_write(sgm, B_BUSOVP, val2reg(SGM41606S_VBUS_OVP,threshold));
}

__maybe_unused static int sgm41606S_set_busocp_th(struct sgm41606S_chip *sgm, int threshold)
{
    dev_info(sgm->dev,"%s:%d-%#x", __func__, threshold, val2reg(SGM41606S_IBUS_OCP,threshold));
    return sgm41606S_field_write(sgm, B_BUSOCP, val2reg(SGM41606S_IBUS_OCP,threshold));
}

__maybe_unused static int sgm41606S_set_batovp_th(struct sgm41606S_chip *sgm, int threshold)
{
    dev_info(sgm->dev,"%s:%d-%#x", __func__, threshold, val2reg(SGM41606S_VBAT_OVP,threshold));
    return sgm41606S_field_write(sgm, B_BATOVP, val2reg(SGM41606S_VBAT_OVP,threshold));
}

__maybe_unused static int sgm41606S_set_batocp_th(struct sgm41606S_chip *sgm, int threshold)
{
    u8 val;

    if (threshold > SGM41606S_IBAT_OCP_MAX_uA)
        threshold = SGM41606S_IBAT_OCP_MAX_uA;
    else if (threshold < SGM41606S_IBAT_OCP_MIN_uA)
        threshold = SGM41606S_IBAT_OCP_MIN_uA;

    if (threshold <= 8500000)
        val = (threshold - SGM41606S_IBAT_OCP_MIN_uA)/SGM41606S_IBAT_STEP_1_uA;
    else{
        val = (threshold - 8500000)/SGM41606S_IBAT_STEP_2_uA;
        val += 85;
    }

    dev_info(sgm->dev,"%s:%d-%#x", __func__, threshold, val);

    return sgm41606S_field_write(sgm, B_BATOCP, val);
}

__maybe_unused static int sgm41606S_set_vbusovp_alarm(struct sgm41606S_chip *sgm, int threshold)
{
    dev_info(sgm->dev,"%s:%d-%#x", __func__, threshold, val2reg(SGM41606S_VBUS_OVP_ALM,threshold));
    return sgm41606S_field_write(sgm, B_BUSOVP_ALM, val2reg(SGM41606S_VBUS_OVP_ALM,threshold));
}

__maybe_unused static int sgm41606S_set_vbatovp_alarm(struct sgm41606S_chip *sgm, int threshold)
{
    dev_info(sgm->dev,"%s:%d-%#x", __func__, threshold, val2reg(SGM41606S_VBAT_OVP_ALM,threshold));
    return sgm41606S_field_write(sgm, B_BATOVP_ALM, val2reg(SGM41606S_VBAT_OVP_ALM,threshold));
}

__maybe_unused static int sgm41606S_is_vbuslowerr(struct sgm41606S_chip *sgm, bool *err)
{
    int ret;
    int val;

    ret = sgm41606S_field_read(sgm, B_VBUS_ERRHI_STAT, &val);
    if(ret < 0) {
        return ret;
    }

    dev_info(sgm->dev,"%s:%d",__func__,val);
    *err = (bool)val;

    return ret;
}

/*A06 code for AL7160A-860 by jiashixian at 20240512 start*/
__maybe_unused static int sgm41606S_get_vbus_present_status(struct sgm41606S_chip *sgm)
{
    int ret;
    int val;
    ret = sgm41606S_field_read(sgm, B_VBUSPRESENT_STAT, &val);
    if(ret < 0) {
        return ret;
    }
    dev_info(sgm->dev,"%s:%d",__func__,val);
    return val;
}
/*A06 code for AL7160A-860 by jiashixian at 20240512 end*/

/*A06 code for AL7160A-2141 by jiashixian at 20240703 start*/
__maybe_unused static int sgm41606S_get_wdt_status(struct sgm41606S_chip *sgm)
{
    int ret;
    int val;
    ret = sgm41606S_field_read(sgm, B_WDT_DIS, &val);
    if(ret < 0) {
        return ret;
    }
    dev_info(sgm->dev,"%s:%d",__func__,val);
    return val;
}

__maybe_unused static int sgm41606S_disable_watch_dog(struct sgm41606S_chip *sgm, bool en)
{
    dev_info(sgm->dev,"%s:%d", __func__, en);
    return sgm41606S_field_write(sgm, B_WDT_DIS, !!en);
}
/*A06 code for AL7160A-2141 by jiashixian at 20240703 end*/

__maybe_unused static int sgm41606S_init_device(struct sgm41606S_chip *sgm)
{
    int ret = 0;
    int i;
    struct {
        enum SGM41606_BIT_OPTION field_id;
        int conv_data;
    } props[] = {
        {B_BATOVP_DIS, sgm->cfg.vbat_ovp_dis},
        {B_BATOVP, sgm->cfg.vbat_ovp},
        {B_BATOVP_ALM, sgm->cfg.vbat_ovp_alm},
        {B_BATOCP_DIS, sgm->cfg.ibat_ocp_dis},
        {B_BATOCP, sgm->cfg.ibat_ocp},
        {B_BATOCP_ALM, sgm->cfg.ibat_ocp_alm},
        //{B_VAC1_PD_EN, sgm->cfg.vac_ovp_en},
        {B_VAC1OVP, sgm->cfg.vac_ovp},
        //{B_BUS_PD_EN, sgm->cfg.vbus_ovp_en},
        {B_VAC2OVP, sgm->cfg.vac2_ovp},
        {B_BUSOVP, sgm->cfg.vbus_ovp},
        /*A06 code for AL7160A-1064 by xiongxiaoliang at 20240513 start*/
        {B_BUSOVP_ALM, sgm->cfg.vbus_ovp_alm},
        /*A06 code for AL7160A-1064 by xiongxiaoliang at 20240513 end*/
        {B_TDIE_FLT_DIS, sgm->cfg.tdie_flt_dis},
        {B_BUSUCP_DIS, sgm->cfg.vbus_ucp_dis},
        {B_BUSUCP, sgm->cfg.vbus_ucp},
        {B_BUSOCP, sgm->cfg.ibus_ocp},
        {B_TSBUS_FLT_DIS, sgm->cfg.tsbus_flt_dis},
        {B_TSBAT_FLT_DIS, sgm->cfg.tsbat_flt_dis},
        {B_BUSRCP_DIS, sgm->cfg.vbus_rcp_dis},
        {B_VBUS_ERRHI_DIS , sgm->cfg.vbus_hi_dis},
        {B_FSW_SET, sgm->cfg.fsw_set},
        {B_WDT_DIS, sgm->cfg.wdt_dis},
        {B_WDT, sgm->cfg.wd_timeout},
        {B_CHG_EN, sgm->cfg.chg_en},
        //{B_EN_BYPASS, sgm->cfg.mode},
        {B_RSNS, sgm->cfg.rsns},
        {B_VOUTOVP_DIS, sgm->cfg.voutovp_dis},
        {B_ADC_EN, sgm->cfg.enable_adc},
        {B_VAC1_ADC_DIS, sgm->cfg.enable_vac1_adc},
    };

    ret = sgm41606S_reg_reset(sgm);
    if (ret < 0) {
        dev_err(sgm->dev, "%s Failed to reset registers(%d)\n", __func__, ret);
    }
    msleep(10);

    for (i = 0; i < ARRAY_SIZE(props); i++) {
        ret = sgm41606S_field_write(sgm, props[i].field_id, props[i].conv_data);
    }
   // sgm41606S_set_vbatovp_alarm(sgm,4550);
   // sgm41606S_set_vbusovp_alarm(sgm, 11000);

    sgm41606S_enable_adc(sgm, true);
    sgm41606S_dump_reg(sgm);

    return ret;
}


/*********************mtk charger interface start**********************************/
#ifdef CONFIG_MTK_CLASS
static inline int to_sgm41606S_adc(enum adc_channel chan)
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
    default:
        break;
    }
    return ADC_MAX_NUM;
}


static int mtk_sgm41606S_is_chg_enabled(struct charger_device *chg_dev, bool *en)
{
    struct sgm41606S_chip *sgm = charger_get_data(chg_dev);
    int ret;

    ret = sgm41606S_check_charge_enabled(sgm, en);

    return ret;
}


static int mtk_sgm41606S_enable_chg(struct charger_device *chg_dev, bool en)
{
    struct sgm41606S_chip *sgm = charger_get_data(chg_dev);
    int ret;

    ret = sgm41606S_enable_charge(sgm,en);

    return ret;
}


static int mtk_sgm41606S_set_vbusovp(struct charger_device *chg_dev, u32 uV)
{
    struct sgm41606S_chip *sgm = charger_get_data(chg_dev);

    return sgm41606S_set_busovp_th(sgm, uV/1000);
}
static int mtk_sgm41606S_set_mode(struct charger_device *chg_dev, u32 mode)
{
    struct sgm41606S_chip *sgm = charger_get_data(chg_dev);
    int val,ret;

    ret = sgm41606S_field_write(sgm, B_EN_BYPASS, mode);
    sgm->cfg.mode = mode;

    ret = sgm41606S_field_read(sgm, B_EN_BYPASS, &val);
    if (ret < 0) {
        dev_err(sgm->dev, "%s fail to read MODE(%d)\n", __func__, ret);
    }
    dev_err(sgm->dev, "%s after set mode ,read MODE(%d)\n", __func__,val);

    return ret;
}



static int mtk_sgm41606S_set_ibusocp(struct charger_device *chg_dev, u32 uA)
{
    struct sgm41606S_chip *sgm = charger_get_data(chg_dev);

    return sgm41606S_set_busocp_th(sgm, uA/1000);
}

static int mtk_sgm41606S_set_vbatovp(struct charger_device *chg_dev, u32 uV)
{
    struct sgm41606S_chip *sgm = charger_get_data(chg_dev);
    int ret;

    ret = sgm41606S_set_batovp_th(sgm, uV/1000);
    if (ret < 0)
        return ret;

    return ret;
}

static int mtk_sgm41606S_set_ibatocp(struct charger_device *chg_dev, u32 uA)
{
    struct sgm41606S_chip *sgm = charger_get_data(chg_dev);
    int ret;

    ret = sgm41606S_set_batocp_th(sgm, uA);
    if (ret < 0)
        return ret;

    return ret;
}


static int mtk_sgm41606S_get_adc(struct charger_device *chg_dev, enum adc_channel chan,
              int *min, int *max)
{
    struct sgm41606S_chip *sgm = charger_get_data(chg_dev);

    sgm41606S_get_adc_data(sgm, to_sgm41606S_adc(chan), max);

    if(chan != ADC_CHANNEL_TEMP_JC)
        *max = *max * 1000;

    if (min != max)
        *min = *max;

    return 0;
}

static int mtk_sgm41606S_get_adc_accuracy(struct charger_device *chg_dev,
                   enum adc_channel chan, int *min, int *max)
{

    return 0;
}

static int mtk_sgm41606S_is_vbuslowerr(struct charger_device *chg_dev, bool *err)
{
    struct sgm41606S_chip *sgm = charger_get_data(chg_dev);

    return sgm41606S_is_vbuslowerr(sgm,err);
}

static int mtk_sgm41606S_set_vbatovp_alarm(struct charger_device *chg_dev, u32 uV)
{
    struct sgm41606S_chip *sgm = charger_get_data(chg_dev);
    int ret;

    ret = sgm41606S_set_vbatovp_alarm(sgm, uV/1000);
    if (ret < 0)
        return ret;

    return ret;
}

static int mtk_sgm41606S_reset_vbatovp_alarm(struct charger_device *chg_dev)
{
    struct sgm41606S_chip *sgm = charger_get_data(chg_dev);
    dev_err(sgm->dev,"%s",__func__);
    return 0;
}

static int mtk_sgm41606S_set_vbusovp_alarm(struct charger_device *chg_dev, u32 uV)
{
    struct sgm41606S_chip *sgm = charger_get_data(chg_dev);
    int ret;

    ret = sgm41606S_set_vbusovp_alarm(sgm, uV/1000);
    if (ret < 0)
        return ret;

    return ret;
}

#endif /*CONFIG_MTK_CLASS*/
/********************mtk charger interface end*************************************************/

#ifdef CONFIG_SGM_DVCHG_CLASS
static int sgm41606S_set_enable(struct dvchg_dev *charger_pump, bool enable)
{
    struct sgm41606S_chip *sgm = dvchg_get_private(charger_pump);
    int ret;

    ret = sgm41606S_enable_charge(sgm,enable);

    return ret;
}

static int sgm41606S_get_is_enable(struct dvchg_dev *charger_pump, bool *enable)
{
    struct sgm41606S_chip *sgm = dvchg_get_private(charger_pump);
    int ret;

    ret = sgm41606S_check_charge_enabled(sgm, enable);

    return ret;
}

static int sgm41606S_get_status(struct dvchg_dev *charger_pump, uint32_t *status)
{
    struct sgm41606S_chip *sgm = dvchg_get_private(charger_pump);
    int ret = 0;

    ret = sgm41606S_get_status(sgm, status);

    return ret;
}

static int sgm41606S_get_adc_value(struct dvchg_dev *charger_pump, enum SGM41606S_ADC_CH ch, int *value)
{
    struct sgm41606S_chip *sgm = dvchg_get_private(charger_pump);
    int ret = 0;

    ret = sgm41606S_get_adc_data(sgm, ch, value);

    return ret;
}

#endif /*CONFIG_SGM_DVCHG_CLASS*/

/********************creat devices note start*************************************************/
static ssize_t sgm41606S_show_registers(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    struct sgm41606S_chip *sgm = dev_get_drvdata(dev);
    u8 addr;
    int val;
    u8 tmpbuf[300];
    int len;
    int idx = 0;
    int ret;

    idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sgm41606S");
    for (addr = 0x0; addr <= SGM41606S_REGMAX; addr++) {
        ret = regmap_read(sgm->regmap, addr, &val);
        if (ret == 0) {
            len = snprintf(tmpbuf, PAGE_SIZE - idx,
                    "Reg[%.2X] = 0x%.2x\n", addr, val);
            memcpy(&buf[idx], tmpbuf, len);
            idx += len;
        }
    }

    return idx;
}

static ssize_t sgm41606S_store_register(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct sgm41606S_chip *sgm = dev_get_drvdata(dev);
    int ret;
    unsigned int reg;
    unsigned int val;

    ret = sscanf(buf, "%x %x", &reg, &val);
    if (ret == 2 && reg <= SGM41606S_REGMAX)
        regmap_write(sgm->regmap, (unsigned char)reg, (unsigned char)val);

    return count;
}

static DEVICE_ATTR(registers, 0660, sgm41606S_show_registers, sgm41606S_store_register);

static void sgm41606S_create_device_node(struct device *dev)
{
    device_create_file(dev, &dev_attr_registers);
}
/********************creat devices note end*************************************************/


static void sgm41606S_check_fault_status(struct sgm41606S_chip *sgm)
{
    int ret;
    u8 flag = 0;
    int i,j;

    for (i=0;i < ARRAY_SIZE(cp_intr_flag);i++) {
        ret = sgm41606S_read_block(sgm, cp_intr_flag[i].reg, &flag, 1);
        for (j=0; j <  cp_intr_flag[i].len; j++) {
            if (flag & cp_intr_flag[i].bit[j].mask) {
                dev_err(sgm->dev,"%s, trigger :%s\n", __func__, cp_intr_flag[i].bit[j].name);
            }
        }
    }
}

static irqreturn_t sgm41606S_irq_handler(int irq, void *data)
{
    struct sgm41606S_chip *sgm = data;
    /* A06 code for AL7160A-3570 by shixuanxuan at 20240622 start */
    int result = 0;

    sgm41606S_field_read(sgm, B_EN_OTG, &result);
    dev_err(sgm->dev,"sgm41606S INT OCCURED\n");
    /*A06 code for AL7160A-989 by shixuanxuan at 20220618 start*/
    if(sgm->otg_tx_mode && !result) {
        sgm41606S_field_write(sgm, B_EN_OTG, true);
        sgm41606S_field_write(sgm, B_DIS_ACDRV_BOTH, false);
        sgm41606S_field_write(sgm, B_ACDRV1_STAT, true);
        pr_err("set irq otg mode = 1\n");
    }
    /*A06 code for AL7160A-989 by shixuanxuan at 20220618 end*/
    /* A06 code for AL7160A-3570 by shixuanxuan at 20240622 end */
    sgm41606S_check_fault_status(sgm);
    /* A06 code for AL7160A-2141 by shixuanxuan at 20240703 start */
    if (!sgm41606S_get_wdt_status(sgm)) {
        sgm41606S_disable_watch_dog(sgm, true);
     }
    /* A06 code for AL7160A-2141 by shixuanxuan at 20240703 end */
    power_supply_changed(sgm->psy);

    return IRQ_HANDLED;
}

static int sgm41606S_register_interrupt(struct sgm41606S_chip *sgm)
{
    int ret;

    if (gpio_is_valid(sgm->irq_gpio)) {
        ret = gpio_request_one(sgm->irq_gpio, GPIOF_DIR_IN,"sgm41606S_irq");
        if (ret) {
            dev_err(sgm->dev,"failed to request sgm41606S_irq\n");
            return -EINVAL;
        }
        sgm->irq = gpio_to_irq(sgm->irq_gpio);
        if (sgm->irq < 0) {
            dev_err(sgm->dev,"sgm41606S failed to gpio_to_irq\n");
            return -EINVAL;
        }
    } else {
        dev_err(sgm->dev,"sgm41606S irq gpio not provided\n");
        return -EINVAL;
    }

    if (sgm->irq) {
        ret = devm_request_threaded_irq(&sgm->client->dev, sgm->irq,
                NULL, sgm41606S_irq_handler,
                IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                sgm41606S_irq_name[sgm->mode], sgm);

        if (ret < 0) {
            dev_err(sgm->dev,"request irq for irq=%d failed, ret =%d\n",
                            sgm->irq, ret);
            return ret;
        }
        enable_irq_wake(sgm->irq);
    }

    return ret;
}
static int sgm41606S_set_present(struct sgm41606S_chip *chip, bool present)
{
    chip->usb_present = present;

    if (present)
    sgm41606S_init_device(chip);
    return 0;
}
/********************interrupte end*************************************************/


/************************psy start**************************************/
    /* A06 code for AL7160A-42 by jiashixian at 2024/04/07 start */
static enum power_supply_property sgm41606S_charger_props[] = {
    POWER_SUPPLY_PROP_CHARGING_ENABLED,
    POWER_SUPPLY_PROP_PRESENT,
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

static void sgm41606S_set_otg_txmode(struct sgm41606S_chip *sgm, int enable)
{
    sgm->otg_tx_mode = enable;
    /*A06 code for AL7160A-989 by shixuanxuan at 20220512 start*/
    if (!enable) {
        sgm41606S_field_write(sgm, B_EN_OTG, false);
    }
    /*A06 code for AL7160A-989 by shixuanxuan at 20220512 end*/
}
/*A06 code for SR-AL7160A-01-265 by shixuanxuan at 20240407 end*/
/*A06 code for AL7160A-989 by shixuanxuan at 20240512 end*/

static int sgm41606S_charger_get_property(struct power_supply *psy,
                enum power_supply_property psp,
                union power_supply_propval *val)
{
    struct sgm41606S_chip *sgm = power_supply_get_drvdata(psy);
    int result;
    int ret;
    bool en;

    switch (psp) {
    case POWER_SUPPLY_PROP_CHARGING_ENABLED:
        ret = sgm41606S_check_charge_enabled(sgm, &en);
        if (!ret) {
            sgm->charge_enabled = en;
        }
        val->intval = sgm->charge_enabled;
        break;
    case POWER_SUPPLY_PROP_PRESENT:
        val->intval = sgm->usb_present;
         ret = 0;
        break;
    case POWER_SUPPLY_PROP_CPM_VBUS_VOLT:
        ret = sgm41606S_get_adc_data(sgm, ADC_VBUS, &result);
        if (!ret)
            sgm->vbus_volt = result;
        val->intval = sgm->vbus_volt;
        break;
    case POWER_SUPPLY_PROP_CPM_IBUS_CURRENT:
        ret = sgm41606S_get_adc_data(sgm, ADC_IBUS, &result);
        if (!ret)
            sgm->ibus_curr = result;
        val->intval = sgm->ibus_curr;
        break;
    case POWER_SUPPLY_PROP_CPM_VBAT_VOLT:
        ret = sgm41606S_get_adc_data(sgm, ADC_VBAT, &result);
        if (!ret)
            sgm->vbat_volt = result;
        val->intval = sgm->vbat_volt;
        break;
    case POWER_SUPPLY_PROP_CPM_IBAT_CURRENT:
        ret = sgm41606S_get_adc_data(sgm, ADC_IBAT, &result);
        if (!ret)
            sgm->ibat_curr = result;
        val->intval = sgm->ibat_curr;
        break;
    case POWER_SUPPLY_PROP_CPM_TEMP:
        ret = sgm41606S_get_adc_data(sgm, ADC_TDIE, &result);
        if (!ret)
            sgm->die_temp = result;
        val->intval = sgm->die_temp;
        break;
    /*A06 code for AL7160A-607 by shixuanxuan at 20240507 start*/
    case POWER_SUPPLY_PROP_CP_OTG_ENABLE:
        val->intval = sgm->otg_tx_mode;
        break;
    /*A06 code for AL7160A-607 by shixuanxuan at 20240507 end*/
    default:
        return -EINVAL;
    }

    return 0;
}
    /* A06 code for AL7160A-42 by jiashixian at 2024/04/07 end */

static int sgm41606S_charger_set_property(struct power_supply *psy,
                    enum power_supply_property prop,
                    const union power_supply_propval *val)
{
    struct sgm41606S_chip *sgm = power_supply_get_drvdata(psy);

    switch (prop) {
    case POWER_SUPPLY_PROP_CHARGING_ENABLED:
        sgm41606S_enable_charge(sgm, val->intval);
        sgm41606S_check_charge_enabled(sgm,&sgm->charge_enabled);
        dev_info(sgm->dev, "POWER_SUPPLY_PROP_CHARGING_ENABLED: %s\n",
                val->intval ? "enable" : "disable");
        break;
    case POWER_SUPPLY_PROP_PRESENT:
        sgm41606S_set_present(sgm, !!val->intval);
    /*A06 code for SR-AL7160A-01-265 by shixuanxuan at 20240407 start*/
    case POWER_SUPPLY_PROP_CP_OTG_ENABLE:
        sgm41606S_set_otg_txmode(sgm, !!val->intval);
        pr_info("set otg mode:%d\n", val->intval);
        break;
    /*A06 code for SR-AL7160A-01-265 by shixuanxuan at 20240407 end*/
    /*A06 code for AL7160A-1232 by jiashixian at 20240518 start*/
    case POWER_SUPPLY_PROP_CP_ADC_ENABLE:
        sgm41606S_enable_adc(sgm, !!val->intval);
        break;
    /*A06 code for AL7160A-1232 by jiashixian at 20240518 end*/
    default:
        return -EINVAL;
    }

    return 0;
}

static int sgm41606S_charger_is_writeable(struct power_supply *psy,
                    enum power_supply_property prop)
{
    switch (prop) {
    case POWER_SUPPLY_PROP_CHARGING_ENABLED:
        return true;
    default:
        return false;
        }
    return 0;
}

static int sgm41606S_psy_register(struct sgm41606S_chip *sgm)
{
    sgm->psy_cfg.drv_data = sgm;
    sgm->psy_cfg.of_node = sgm->dev->of_node;

    sgm->psy_desc.name = "cpm-standalone";//sgm41606S_psy_name[sgm->mode];

    sgm->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
    sgm->psy_desc.properties = sgm41606S_charger_props;
    sgm->psy_desc.num_properties = ARRAY_SIZE(sgm41606S_charger_props);
    sgm->psy_desc.get_property = sgm41606S_charger_get_property;
    sgm->psy_desc.set_property = sgm41606S_charger_set_property;
    sgm->psy_desc.property_is_writeable = sgm41606S_charger_is_writeable;

    sgm->psy = devm_power_supply_register(sgm->dev,
            &sgm->psy_desc, &sgm->psy_cfg);
    if (IS_ERR(sgm->psy)) {
        dev_err(sgm->dev, "%s failed to register psy\n", __func__);
        return PTR_ERR(sgm->psy);
    }

    dev_info(sgm->dev, "%s power supply register successfully\n", sgm->psy_desc.name);

    return 0;
}


/************************psy end**************************************/

static int sgm41606S_set_work_mode(struct sgm41606S_chip *sgm, int mode)
{
    sgm->mode = mode;

    dev_err(sgm->dev,"SGM41606S work mode is %s\n", sgm->mode == SGM41606S_STANDALONG
        ? "standalone" : (sgm->mode == SGM41606S_MASTER ? "master" : "slave"));

    return 0;
}

static int sgm41606S_parse_dt(struct sgm41606S_chip *sgm, struct device *dev)
{
    struct device_node *np = dev->of_node;
    int i;
    int ret;
    struct {
        char *name;
        int *conv_data;
    } props[] = {
        {"sgm,sgm41606S,vbat-ovp-dis", &(sgm->cfg.vbat_ovp_dis)},
        {"sgm,sgm41606S,vbat-ovp", &(sgm->cfg.vbat_ovp)},
        {"sgm,sgm41606S,vbat-ovp-alm", &(sgm->cfg.vbat_ovp_alm)},
        {"sgm,sgm41606S,ibat-ocp-dis", &(sgm->cfg.ibat_ocp_dis)},
        {"sgm,sgm41606S,ibat-ocp", &(sgm->cfg.ibat_ocp)},
        {"sgm,sgm41606S,ibat-ocp_alm", &(sgm->cfg.ibat_ocp_alm)},
        //{"sgm,sgm41606S,vac-ovp-en", &(sgm->cfg.vac_ovp_en)},
        {"sgm,sgm41606S,vac-ovp", &(sgm->cfg.vac_ovp)},
        {"sgm,sgm41606S,vac2-ovp", &(sgm->cfg.vac2_ovp)},
        //{"sgm,sgm41606S,vbus-ovp-en", &(sgm->cfg.vbus_ovp_en)},
        {"sgm,sgm41606S,vbus-ovp", &(sgm->cfg.vbus_ovp)},
        {"sgm,sgm41606S,vbus-ovp-alm", &(sgm->cfg.vbus_ovp_alm)},
        {"sgm,sgm41606S,tdie-flt-dis", &(sgm->cfg.tdie_flt_dis)},
        {"sgm,sgm41606S,vbus-ucp-dis", &(sgm->cfg.vbus_ucp_dis)},
        {"sgm,sgm41606S,vbus-ucp", &(sgm->cfg.vbus_ucp)},
        {"sgm,sgm41606S,ibus-ocp", &(sgm->cfg.ibus_ocp)},
        {"sgm,sgm41606S,tsbus-flt-dis", &(sgm->cfg.tsbus_flt_dis)},
        {"sgm,sgm41606S,tsbat-flt-dis", &(sgm->cfg.tsbat_flt_dis)},
        //{"sgm,sgm41606S,ibus-ucp-dis", &(sgm->cfg.ibat_ucp_alm_dis)},
        //{"sgm,sgm41606S,ibus-ucp-alm", &(sgm->cfg.ibat_ucp_alm)},

        //{"sgm,sgm41606S,vbus-low-dis", &(sgm->cfg.vbus_low_dis)},
        {"sgm,sgm41606S,vbus-rcp-dis", &(sgm->cfg.vbus_rcp_dis)},
        {"sgm,sgm41606S,vbus-high-dis", &(sgm->cfg.vbus_hi_dis)},
        {"sgm,sgm41606S,fsw-set", &(sgm->cfg.fsw_set)},
        {"sgm,sgm41606S,wdt-dis", &(sgm->cfg.wdt_dis)},
        {"sgm,sgm41606S,wd-timeout", &(sgm->cfg.wd_timeout)},
    {"sgm,sgm41606S,chg-en", &(sgm->cfg.chg_en)},
        //{"sgm,sgm41606S,mode", &(sgm->cfg.mode)},
        {"sgm,sgm41606S,rsns", &(sgm->cfg.rsns)},
        {"sgm,sgm41606S,voutovp-dis", &(sgm->cfg.voutovp_dis)},
        {"sgm,sgm41606S,enable-adc", &(sgm->cfg.enable_adc)},
        {"sgm,sgm41606S,enable-vac1-adc", &(sgm->cfg.enable_vac1_adc)},
    };

    /* initialize data for optional properties */
    for (i = 0; i < ARRAY_SIZE(props); i++) {
        ret = of_property_read_u32(np, props[i].name,
                        props[i].conv_data);
        if (ret < 0) {
            dev_err(sgm->dev, "%s, can not read %s \n", __func__, props[i].name);
            return ret;
        }
    }

    sgm->irq_gpio = of_get_named_gpio(np, "sgm41606S,intr_gpio", 0);
    if (!gpio_is_valid(sgm->irq_gpio)) {
        dev_err(sgm->dev,"%s, fail to valid gpio : %d\n",__func__, sgm->irq_gpio);
        return -EINVAL;
    }

    dev_err(sgm->dev, "%s: end\n", __func__);
    return 0;
}

static struct of_device_id sgm41606S_charger_match_table[] = {
    {   .compatible = "sgm,sgm41606S-standalone",
        .data = &sgm41606S_mode_data[SGM41606S_STANDALONG], },
    {   .compatible = "sgm,sgm41606S-master",
        .data = &sgm41606S_mode_data[SGM41606S_MASTER], },
    {   .compatible = "sgm,sgm41606S-slave",
        .data = &sgm41606S_mode_data[SGM41606S_SLAVE], },
};

static int sgm41606S_charger_probe(struct i2c_client *client,
                    const struct i2c_device_id *id)
{
    struct sgm41606S_chip *sgm;
    const struct of_device_id *match;
    struct device_node *node = client->dev.of_node;
    int ret, i;

    dev_err(&client->dev, "%s \n", __func__);

    sgm = devm_kzalloc(&client->dev, sizeof(struct sgm41606S_chip), GFP_KERNEL);
    if (!sgm) {
        ret = -ENOMEM;
        goto err_kzalloc;
    }
    client->addr = 0x65;
    sgm->dev = &client->dev;
    sgm->client = client;

    sgm->regmap = devm_regmap_init_i2c(client,
                            &sgm41606S_regmap_config);
    if (IS_ERR(sgm->regmap)) {
        dev_err(sgm->dev, "%s,Failed to initialize regmap\n",__func__);
        ret = PTR_ERR(sgm->regmap);
        goto err_regmap_init;
    }

    for (i = 0; i < ARRAY_SIZE(sgm41606S_regs); i++) {
        const struct reg_field *reg_fields = sgm41606S_regs;

        sgm->rmap_fields[i] =
            devm_regmap_field_alloc(sgm->dev,
                        sgm->regmap,
                        reg_fields[i]);
        if (IS_ERR(sgm->rmap_fields[i])) {
            dev_err(sgm->dev, "%s, cannot allocate regmap field\n", __func__);
            ret = PTR_ERR(sgm->rmap_fields[i]);
            goto err_regmap_field;
        }
    }

    ret = sgm41606S_detect_device(sgm);
    if (ret < 0) {
        dev_err(sgm->dev, "%s detect device fail\n", __func__);
        goto err_detect_dev;
    }

    i2c_set_clientdata(client, sgm);
    sgm41606S_create_device_node(&(client->dev));

    match = of_match_node(sgm41606S_charger_match_table, node);
    if (match == NULL) {
        dev_err(sgm->dev, "%s, device tree match not found!\n", __func__);
        goto err_match_node;
    }


    ret = sgm41606S_parse_dt(sgm, &client->dev);//lzk
    if (ret < 0) {
        dev_err(sgm->dev, "%s parse dt failed(%d)\n", __func__, ret);
        goto err_parse_dt;
    }

    ret = sgm41606S_set_work_mode(sgm, *(int *)match->data);
    if (ret) {
        dev_err(sgm->dev,"%s, Fail to set work mode!\n", __func__);
        goto err_set_mode;
    }

    ret = sgm41606S_init_device(sgm);
    if (ret < 0) {
        dev_err(sgm->dev, "%s init device failed(%d)\n", __func__, ret);
        goto err_init_device;
    }

    ret = sgm41606S_psy_register(sgm);
    if (ret < 0) {
        dev_err(sgm->dev, "%s psy register failed(%d)\n", __func__, ret);
        goto err_register_psy;
    }

    ret = sgm41606S_register_interrupt(sgm);
    if (ret < 0) {
        dev_err(sgm->dev, "%s register irq fail(%d)\n",
                    __func__, ret);
        goto err_register_irq;
    }
    /*A06 code for AL7160A-82 by jiashixian at 20240418 start*/
    g_is_sgm41606s_prob_success = true;
    /*A06 code for AL7160A-82 by jiashixian at 20240418 end*/
    /* A06 code for AL7160A-445 by jiashixian at 20240610 start */
    cp_info = SGM41606S;
    /* A06 code for AL7160A-445 by jiashixian at 20240610 end */
    /*A06 code for AL7160A-607 by shixuanxuan at 20240507 start*/
    sgm->otg_tx_mode = false;
    /*A06 code for AL7160A-607 by shixuanxuan at 20240507 end*/
    dev_err(sgm->dev, "sgm41606S[%s] probe successfully!\n",
            sgm->mode == SGM41606S_MASTER ? "master" : "slave");
    return 0;

err_register_psy:
err_register_irq:
err_init_device:
    power_supply_unregister(sgm->psy);
err_detect_dev:
err_match_node:
err_set_mode:
err_parse_dt:
err_regmap_init:
err_regmap_field:
    devm_kfree(&client->dev, sgm);
err_kzalloc:
    dev_err(&client->dev,"sgm41606S probe fail\n");
    return ret;
}


static int sgm41606S_charger_remove(struct i2c_client *client)
{
    if (g_is_sgm41606s_prob_success) {
        struct sgm41606S_chip *sgm = i2c_get_clientdata(client);
        sgm41606S_enable_adc(sgm, false);
        power_supply_unregister(sgm->psy);
        devm_kfree(&client->dev, sgm);
    }

    return 0;
}

/*A06 code for AL7160A-82 by jiashixian at 20240418 start*/
static void sgm41606S_charger_shutdown(struct i2c_client *client)
{
    if (g_is_sgm41606s_prob_success) {
        struct sgm41606S_chip *sgm = i2c_get_clientdata(client);
        sgm41606S_enable_adc(sgm, false);
    }
}
/*A06 code for AL7160A-82 by jiashixian at 20240418 end*/

#ifdef CONFIG_PM_SLEEP
static int sgm41606S_suspend(struct device *dev)
{
    if (g_is_sgm41606s_prob_success) {
        struct sgm41606S_chip *sgm = dev_get_drvdata(dev);

        dev_info(sgm->dev, "Suspend successfully!");
        if (device_may_wakeup(dev)) {
            enable_irq_wake(sgm->irq);
        }
        disable_irq(sgm->irq);
        /*A06 code for AL7160A-157 by jiashixian at 20240429 start*/
        /*A06 code for AL7160A-860 by jiashixian at 20240512 start*/
        if(!sgm41606S_get_vbus_present_status(sgm)) {
            sgm41606S_enable_adc(sgm, false);
        }
        /*A06 code for AL7160A-860 by jiashixian at 20240512 end*/
        /*A06 code for AL7160A-157 by jiashixian at 20240429 end*/
        /*A06 code for AL7160A-2141 by jiashixian at 20240602 start*/
       // sgm41606S_disable_watch_dog(sgm, true);
        /*A06 code for AL7160A-2141 by jiashixian at 20240602 end*/
    }
    return 0;
}
static int sgm41606S_resume(struct device *dev)
{
    if (g_is_sgm41606s_prob_success) {
        struct sgm41606S_chip *sgm = dev_get_drvdata(dev);
        dev_info(sgm->dev, "Resume successfully!");
        if (device_may_wakeup(dev)) {
            disable_irq_wake(sgm->irq);
        }
        enable_irq(sgm->irq);
        /*A06 code for AL7160A-157 by jiashixian at 20240429 start*/
        /*A06 code for AL7160A-860 by jiashixian at 20240512 start*/
        sgm41606S_enable_adc(sgm, true);
        /*A06 code for AL7160A-860 by jiashixian at 20240512 end*/
        /*A06 code for AL7160A-157 by jiashixian at 20240429 end*/
    }
    return 0;
}

static const struct dev_pm_ops sgm41606S_pm = {
    SET_SYSTEM_SLEEP_PM_OPS(sgm41606S_suspend, sgm41606S_resume)
};
#endif

/*A06 code for AL7160A-82 by jiashixian at 20240418 start*/
static struct i2c_driver sgm41606S_charger_driver = {
    .driver     = {
        .name   = "sgm41606S",
        .owner  = THIS_MODULE,
        .of_match_table = sgm41606S_charger_match_table,
#ifdef CONFIG_PM_SLEEP
        .pm = &sgm41606S_pm,
#endif
    },
    .probe      = sgm41606S_charger_probe,
    .remove     = sgm41606S_charger_remove,
    .shutdown    = sgm41606S_charger_shutdown,
};
/*A06 code for AL7160A-82 by jiashixian at 20240418 end*/

module_i2c_driver(sgm41606S_charger_driver);

MODULE_DESCRIPTION("SGM SGM41606S Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("hongqiang_qin@sg-micro.com");
