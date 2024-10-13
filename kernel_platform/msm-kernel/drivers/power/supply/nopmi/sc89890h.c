// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
*/

#define pr_fmt(fmt) "<sc89890h-05180124> [%s,%d] " fmt, __func__, __LINE__

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
#include <linux/version.h>
#include <linux/iio/consumer.h>

#include "../../../usb/typec/tcpc/inc/tcpci_core.h"
#include "../../../usb/typec/tcpc/inc/tcpm.h"
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/power_supply.h>
#include "sc8989x_iio.h"
#include "qcom-pmic-voter.h"


#if 0  /*only for debug*/
#undef pr_debug
#define pr_debug pr_err
#undef pr_info
#define pr_info pr_err
#undef dev_dbg
#define dev_dbg dev_err
#endif

#ifdef CONFIG_MTK_CLASS
#include "charger_class.h"
#include "mtk_charger.h"
#ifdef CONFIG_MTK_CHARGER_V4P19
#include "charger_type.h"
#include "mtk_charger_intf.h"
#endif /*CONFIG_MTK_CHARGER_V4P19*/
#endif /*CONFIG_MTK_CLASS */

// #include "../../usb/typec/tcpc/inc/tcpm.h"
#include "../../../usb/typec/tcpc/inc/tcpm.h"

#define ATTACHEDSRC   7
#define SC8989X_DRV_VERSION         "1.0.0_G"
#define REG01_DPDM_OUT_0P6V 2
#define VINDPM_USB_CDP 4600
extern int afc_set_voltage_workq(unsigned int afc_code);
extern int upm6720_set_adc(bool enable);
extern void oz8806_set_shutdown_mode(void);
extern void oz8806_set_charge_full(bool val);
enum sc8960x_part_no {
    SC89890H_PN_NUM = 0x04,
    SC89895_PN_NUM = 0x04,
    SC8950_PN_NUM = 0x02,
    SC89890W_PN_NUM = 0x07,
};

#define SC8989X_VINDPM_MAX           15300      //mV

#define SC8989X_REG7D                0x7D
#define SC8989X_KEY1                 0x48
#define SC8989X_KEY2                 0x54
#define SC8989X_KEY3                 0x53
#define SC8989X_KEY4                 0x38

#define SC8989X_ADC_EN               0x87
#define SC8989X_DPDM3                0x88
#define SC8989X_PRIVATE              0xF9

#define PROFILE_CHG_VOTER            "PROFILE_CHG_VOTER"
#define MAIN_SET_VOTER               "MAIN_SET_VOTER"
#define BMS_FC_VOTER                 "BMS_FC_VOTER"
#define DCP_CHG_VOTER			"DCP_CHG_VOTER"
#define PD_CHG_VOTER			"PD_CHG_VOTER"
#define AFC_CHG_DARK_VOTER		"AFC_CHG_DARK_VOTER"


#define SC8989X_MAX_ICL              3250
#define SC8989X_MAX_FCC              5040

#define CHG_FCC_CURR_MAX             3600
#define CHG_ICL_CURR_MAX             2120
#define DCP_ICL_CURR_MAX			 2100
#define AFC_ICL_CURR_MAX             1700
#define PD_ICL_CURR_MAX		         1100
#define AFC_COMMON_ICL_CURR_MAX      1800

#define CHG_CDP_CURR_MAX        1500
#define CHG_SDP_CURR_MAX        500
#define CHG_AFC_CURR_MAX        3000
#define CHG_PDO_CURR_MAX        3000
#define CHG_DCP_CURR_MAX        2200
#define CHG_AFC_COMMON_CURR_MAX 2500

#define SC8989X_TERM_OFFSET     30
#define SC8989X_TERM_STEP       60

#define AFC_CHARGE_ITERM        480
#define IC_ITERM_OFFSET         30

#define AFC_QUICK_CHARGE_POWER_CODE          0x46
#define AFC_COMMON_CHARGE_POWER_CODE         0x08


enum sc8989x_vbus_stat {
    VBUS_STAT_NO_INPUT = 0,
    VBUS_STAT_SDP,
    VBUS_STAT_CDP,
    VBUS_STAT_DCP,
    VBUS_STAT_HVDCP,
    VBUS_STAT_UNKOWN,
    VBUS_STAT_NONSTAND,
    VBUS_STAT_OTG,
};

enum sc8989x_chg_stat {
    CHG_STAT_NOT_CHARGE = 0,
    CHG_STAT_PRE_CHARGE,
    CHG_STAT_FAST_CHARGE,
    CHG_STAT_CHARGE_DONE,
};

enum sc8989x_adc_channel {
    SC8989X_ADC_VBAT,
    SC8989X_ADC_VSYS,
    SC8989X_ADC_VBUS,
    SC8989X_ADC_ICC,
    SC8989X_ADC_IBUS,

};

enum {
    SC8989X_VBUSSTAT_NOINPUT = 0,
    SC8989X_VBUSSTAT_SDP,
    SC8989X_VBUSSTAT_CDP,
    SC8989X_VBUSSTAT_DCP,
    SC8989X_VBUSSTAT_HVDCP,
    SC8989X_VBUSSTAT_FLOAT,
    SC8989X_VBUSSTAT_NON_STD,
    SC8989X_VBUSSTAT_OTG,
};

enum sc8989x_iio_type {
         DS28E16,
         BMS,
         NOPMI,
 };


enum sc8989x_fields {
    EN_HIZ, EN_ILIM, IINDPM,
    DP_DRIVE, DM_DRIVE, VINDPM_OS,
    CONV_START, CONV_RATE, BOOST_FRE, ICO_EN, HVDCP_EN, FORCE_DPDM,
        AUTO_DPDM_EN,
    FORCE_DSEL, WD_RST, OTG_CFG, CHG_CFG, VSYS_MIN, VBATMIN_SEL,
    EN_PUMPX, ICC,
    ITC, ITERM,
    CV, VBAT_LOW, VRECHG,
    EN_ITERM, STAT_DIS, TWD, EN_TIMER, TCHG, JEITA_ISET,
    BAT_COMP, VCLAMP, TJREG,
    FORCE_ICO, TMR2X_EN, BATFET_DIS, JETTA_VSET_WARM, BATFET_DLY, BATFET_RST_EN,
        PUMPX_UP, PUMPX_DN,
    V_OTG, PFM_OTG_DIS, IBOOST_LIM,
    VBUS_STAT, CHG_STAT, PG_STAT, VSYS_STAT,
    FORCE_VINDPM, VINDPM,
    ADC_VBAT,
    ADC_VSYS,
    VBUS_GD,ADC_VBUS,
    ADC_ICC,
    VINDPM_STAT, IINDPM_STAT,
    REG_RST, ICO_STAT, PN, NTC_PROFILE, DEV_VERSION,
    VBAT_REG_LSB,
    ADC_IBUS,
    DP3P3V_DM0V_EN,
    F_MAX_FIELDS,
};

enum sc8989x_reg_range {
    SC8989X_IINDPM,
    SC8989X_ICHG,
    SC8989X_IBOOST,
    SC8989X_VBAT_REG,
    SC8989X_VINDPM,
    SC8989X_ITERM,
    SC8989X_VBAT,
    SC8989X_VSYS,
    SC8989X_VBUS,
    SC8989X_ICC,
    SC8989X_IBUS,
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

struct sc8989x_cfg_e {
    const char *chg_name;
    int ico_en;
    int hvdcp_en;
    int auto_dpdm_en;
    int vsys_min;
    int vbatmin_sel;
    int itrick;
    int iterm;
    int vbat_cv;
    int vbat_low;
    int vrechg;
    int en_term;
    int stat_dis;
    int wd_time;
    int en_timer;
    int charge_timer;
    int bat_comp;
    int vclamp;
    int votg;
    int iboost;
    int force_vindpm;
    int vindpm;
};

/* These default values will be applied if there's no property in dts */
static struct sc8989x_cfg_e sc8989x_default_cfg = {
    .chg_name = "primary_chg",
    .ico_en = 0,
    .hvdcp_en = 1,
    .auto_dpdm_en = 1,
    .vsys_min = 5,
    .vbatmin_sel = 0,
    .itrick = 3,
    .iterm = 4,
    .vbat_cv = 23,
    .vbat_low = 1,
    .vrechg = 0,
    .en_term = 1,
    .stat_dis = 0,
    .wd_time = 0,
    .en_timer = 1,
    .charge_timer = 2,
    .bat_comp = 0,
    .vclamp = 0,
    .votg = 12,
    .iboost = 7,
    .force_vindpm = 0,
    .vindpm = 18,
};

struct sc8989x_chip {
    struct device *dev;
    struct i2c_client *client;
    struct regmap *regmap;
    struct regmap_field *rmap_fields[F_MAX_FIELDS];

#ifdef CONFIG_MTK_CLASS
    struct charger_device *chg_dev;
    struct charger_properties chg_props;
#ifdef CONFIG_MTK_CHARGER_V4P19
    enum charger_type adp_type;
    struct power_supply *chg_psy;
    struct delayed_work psy_dwork;
#endif /*CONFIG_MTK_CHARGER_V4P19*/
#endif /*CONFIG_MTK_CLASS */

    int chg_type;

    int psy_usb_type;

    int irq_gpio;
    int irq;
    int charge_afc;
    bool is_disble_afc;
    bool is_disble_afc_backup;
    int pd_active;
    struct delayed_work force_detect_dwork;
    struct delayed_work hvdcp_dwork;
    struct delayed_work disable_afc_dwork;
    struct delayed_work disable_pdo_dwork;
    struct delayed_work hvdcp_qc20_dwork;
    int force_detect_count;
    struct wakeup_source *hvdcp_qc20_ws;

    int power_good;
    int vbus_good;
    int afc_running;
    uint8_t dev_id;
    bool entry_shipmode;

    struct power_supply_desc psy_desc;
    struct sc8989x_cfg_e *cfg;
    struct power_supply *psy;
    struct power_supply *chg_psy;
    struct power_supply *bat_psy;
    struct power_supply *cp_psy;

	struct delayed_work tcpc_dwork;
	struct tcpc_device *tcpc;
    struct notifier_block tcp_nb;

#if 1
	/* longcheer nielianjie10 2023.4.26 add iio start */
	struct iio_dev  		*indio_dev;
	struct iio_chan_spec	*iio_chan;
	struct iio_channel		*int_iio_chans;
	struct iio_channel		**ds_ext_iio_chans;
	struct iio_channel		**fg_ext_iio_chans;
	struct iio_channel		**nopmi_chg_ext_iio_chans;
#endif

	struct	mutex regulator_lock;
	struct	regulator *dpdm_reg;
	bool	dpdm_enabled;

	struct power_supply *usb_psy;
	struct power_supply *ac_psy;

    struct votable			*fcc_votable;
	struct votable			*fv_votable;
	struct votable			*usb_icl_votable;
	struct votable			*chg_dis_votable;
};

static bool vbus_on = false;

static const u32 sc8989x_iboost[] = {
    500, 750, 1200, 1400, 1650, 1875, 2150, 2450,
};

#define SC8989X_CHG_RANGE(_min, _max, _step, _offset, _ru) \
{ \
    .min = _min, \
    .max = _max, \
    .step = _step, \
    .offset = _offset, \
    .round_up = _ru, \
}

#define SC8989X_CHG_RANGE_T(_table, _ru) \
    { .table = _table, .num_table = ARRAY_SIZE(_table), .round_up = _ru, }

static const struct reg_range sc8989x_reg_range_ary[] = {
    [SC8989X_IINDPM] = SC8989X_CHG_RANGE(100, 3250, 50, 100, false),
    [SC8989X_ICHG] = SC8989X_CHG_RANGE(0, 5040, 60, 0, false),
    [SC8989X_ITERM] = SC8989X_CHG_RANGE(30, 930, 60, 30, false),
    [SC8989X_VBAT_REG] = SC8989X_CHG_RANGE(3840, 4848, 16, 3840, false),
    [SC8989X_VINDPM] = SC8989X_CHG_RANGE(3900, 15300, 100, 2600, false),
    [SC8989X_IBOOST] = SC8989X_CHG_RANGE_T(sc8989x_iboost, false),
    [SC8989X_VBAT] = SC8989X_CHG_RANGE(2304, 4848, 20, 2304, false),
    [SC8989X_VSYS] = SC8989X_CHG_RANGE(2304, 4848, 20, 2304, false),
    [SC8989X_VBUS] = SC8989X_CHG_RANGE(2600, 15300, 100, 2600, false),
    [SC8989X_ICC] = SC8989X_CHG_RANGE(0, 6350, 50, 0, false),
    [SC8989X_IBUS] = SC8989X_CHG_RANGE(0, 6350, 50, 0, false),
};

//REGISTER
static const struct reg_field sc8989x_reg_fields[] = {
    /*reg00 */
    [EN_HIZ] = REG_FIELD(0x00, 7, 7),
    [EN_ILIM] = REG_FIELD(0x00, 6, 6),
    [IINDPM] = REG_FIELD(0x00, 0, 5),
    /*reg01 */
    [DP_DRIVE] = REG_FIELD(0x01, 5, 7),
    [DM_DRIVE] = REG_FIELD(0x01, 2, 4),
    [VINDPM_OS] = REG_FIELD(0x01, 0, 0),
    /*reg02 */
    [CONV_START] = REG_FIELD(0x02, 7, 7),
    [CONV_RATE] = REG_FIELD(0x02, 6, 6),
    [BOOST_FRE] = REG_FIELD(0x02, 5, 5),
    [ICO_EN] = REG_FIELD(0x02, 4, 4),
    [HVDCP_EN] = REG_FIELD(0x02, 3, 3),
    [FORCE_DPDM] = REG_FIELD(0x02, 1, 1),
    [AUTO_DPDM_EN] = REG_FIELD(0x02, 0, 0),
    /*reg03 */
    [FORCE_DSEL] = REG_FIELD(0x03, 7, 7),
    [WD_RST] = REG_FIELD(0x03, 6, 6),
    [OTG_CFG] = REG_FIELD(0x03, 5, 5),
    [CHG_CFG] = REG_FIELD(0x03, 4, 4),
    [VSYS_MIN] = REG_FIELD(0x03, 1, 3),
    [VBATMIN_SEL] = REG_FIELD(0x03, 0, 0),
    /*reg04 */
    [EN_PUMPX] = REG_FIELD(0x04, 7, 7),
    [ICC] = REG_FIELD(0x04, 0, 6),
    /*reg05 */
    [ITC] = REG_FIELD(0x05, 4, 7),
    [ITERM] = REG_FIELD(0x05, 0, 3),
    /*reg06 */
    [CV] = REG_FIELD(0x06, 2, 7),
    [VBAT_LOW] = REG_FIELD(0x06, 1, 1),
    [VRECHG] = REG_FIELD(0x06, 0, 0),
    /*reg07 */
    [EN_ITERM] = REG_FIELD(0x07, 7, 7),
    [STAT_DIS] = REG_FIELD(0x07, 6, 6),
    [TWD] = REG_FIELD(0x07, 4, 5),
    [EN_TIMER] = REG_FIELD(0x07, 3, 3),
    [TCHG] = REG_FIELD(0x07, 1, 2),
    [JEITA_ISET] = REG_FIELD(0x07, 0, 0),
    /*reg08 */
    [BAT_COMP] = REG_FIELD(0x08, 5, 7),
    [VCLAMP] = REG_FIELD(0x08, 2, 4),
    [TJREG] = REG_FIELD(0x08, 0, 1),
    /*reg09 */
    [FORCE_ICO] = REG_FIELD(0x09, 7, 7),
    [TMR2X_EN] = REG_FIELD(0x09, 6, 6),
    [BATFET_DIS] = REG_FIELD(0x09, 5, 5),
    [JETTA_VSET_WARM] = REG_FIELD(0x09, 4, 4),
    [BATFET_DLY] = REG_FIELD(0x09, 3, 3),
    [BATFET_RST_EN] = REG_FIELD(0x09, 2, 2),
    [PUMPX_UP] = REG_FIELD(0x09, 1, 1),
    [PUMPX_DN] = REG_FIELD(0x09, 0, 0),
    /*reg0A */
    [V_OTG] = REG_FIELD(0x0A, 4, 7),
    [PFM_OTG_DIS] = REG_FIELD(0x0A, 3, 3),
    [IBOOST_LIM] = REG_FIELD(0x0A, 0, 2),
    /*reg0B */
    [VBUS_STAT] = REG_FIELD(0x0B, 5, 7),
    [CHG_STAT] = REG_FIELD(0x0B, 3, 4),
    [PG_STAT] = REG_FIELD(0x0B, 2, 2),
    [VSYS_STAT] = REG_FIELD(0x0B, 0, 0),
    /*reg0D */
    [FORCE_VINDPM] = REG_FIELD(0x0D, 7, 7),
    [VINDPM] = REG_FIELD(0x0D, 0, 6),
    /*reg0E */
    [ADC_VBAT] = REG_FIELD(0x0E, 0, 6),
    /*reg0F */
    [ADC_VSYS] = REG_FIELD(0x0F, 0, 6),
    /*reg11 */
    [VBUS_GD] = REG_FIELD(0x11, 7, 7),
    [ADC_VBUS] = REG_FIELD(0x11, 0, 6),
    /*reg12 */
    [ADC_ICC] = REG_FIELD(0x12, 0, 6),
    /*reg13 */
    [VINDPM_STAT] = REG_FIELD(0x13, 7, 7),
    [IINDPM_STAT] = REG_FIELD(0x13, 6, 6),
    /*reg14 */
    [REG_RST] = REG_FIELD(0x14, 7, 7),
    [ICO_STAT] = REG_FIELD(0x14, 6, 6),
    [PN] = REG_FIELD(0x14, 3, 5),
    [NTC_PROFILE] = REG_FIELD(0x14, 2, 2),
    [DEV_VERSION] = REG_FIELD(0x14, 0, 1),
    /*reg40 */    
    [VBAT_REG_LSB] = REG_FIELD(0x80, 6, 6),
    /*reg86 */    
    [ADC_IBUS] = REG_FIELD(0x86, 1, 7),
    /*reg83 */
    [DP3P3V_DM0V_EN] = REG_FIELD(0x83, 5, 5),
};

static const struct regmap_config sc8989x_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,

    .max_register = 0xFF,
};

int sc8989x_set_iio_channel(struct sc8989x_chip *upm, enum sc8989x_iio_type type, int channel, int val);

static int sc8989x_request_dpdm(struct sc8989x_chip *sc, bool enable)
{
	int ret = 0;

	mutex_lock(&sc->regulator_lock);
	/* fetch the DPDM regulator */
	if (!sc->dpdm_reg && of_get_property(sc->dev->of_node, "dpdm-supply", NULL)) {
		sc->dpdm_reg = devm_regulator_get(sc->dev, "dpdm");
		if (IS_ERR(sc->dpdm_reg)) {
			ret = PTR_ERR(sc->dpdm_reg);
			pr_err("Couldn't get dpdm regulator ret=%d\n", ret);
			sc->dpdm_reg = NULL;
			mutex_unlock(&sc->regulator_lock);
			return ret;
		}
	}

	if (enable) {
		if (sc->dpdm_reg && !sc->dpdm_enabled) {
		pr_err("enabling DPDM regulator\n");
		ret = regulator_enable(sc->dpdm_reg);
		if (ret < 0)
			pr_err("Couldn't enable dpdm regulator ret=%d\n", ret);
		else
			sc->dpdm_enabled = true;
		}
	} else {
		if (sc->dpdm_reg && sc->dpdm_enabled) {
			pr_err("disabling DPDM regulator\n");
			ret = regulator_disable(sc->dpdm_reg);
			if (ret < 0)
				pr_err("Couldn't disable dpdm regulator ret=%d\n", ret);
			else
				sc->dpdm_enabled = false;
		}
	}
	mutex_unlock(&sc->regulator_lock);

	return ret;
}

/********************COMMON API***********************/
static u8 val2reg(enum sc8989x_reg_range id, u32 val) {
    int i;
    u8 reg;
    const struct reg_range *range = &sc8989x_reg_range_ary[id];

    if (!range)
        return val;

    if (range->table) {
        if (val <= range->table[0])
            return 0;
        for (i = 1; i < range->num_table - 1; i++) {
            if (val == range->table[i])
                return i;
            if (val > range->table[i] && val < range->table[i + 1])
                return range->round_up ? i + 1 : i;
        }
        return range->num_table - 1;
    }
    if (val <= range->min)
        reg = (range->min - range->offset) / range->step;
    else if (val >= range->max)
        reg = (range->max - range->offset) / range->step;
    else if (range->round_up)
        reg = (val - range->offset) / range->step + 1;
    else
        reg = (val - range->offset) / range->step;
    return reg;
}

static u32 reg2val(enum sc8989x_reg_range id, u8 reg) {
    const struct reg_range *range = &sc8989x_reg_range_ary[id];
    if (!range)
        return reg;
    return range->table ? range->table[reg] : range->offset + range->step * reg;
}

/*********************I2C API*********************/
static int sc8989x_field_read(struct sc8989x_chip *sc,
                            enum sc8989x_fields field_id, int *val) {
    int ret;

    ret = regmap_field_read(sc->rmap_fields[field_id], val);
    if (ret < 0) {
        dev_err(sc->dev, "sc8989x read field %d fail: %d\n", field_id, ret);
    }

    return ret;
}

static int sc8989x_field_write(struct sc8989x_chip *sc,
                            enum sc8989x_fields field_id, int val) {
    int ret;

    ret = regmap_field_write(sc->rmap_fields[field_id], val);
    if (ret < 0) {
        dev_err(sc->dev, "sc8989x read field %d fail: %d\n", field_id, ret);
    }

    return ret;
}

/*********************CHIP API*********************/
static int sc8989x_set_key(struct sc8989x_chip *sc) {
    regmap_write(sc->regmap, SC8989X_REG7D, SC8989X_KEY1);
    regmap_write(sc->regmap, SC8989X_REG7D, SC8989X_KEY2);
    regmap_write(sc->regmap, SC8989X_REG7D, SC8989X_KEY3);
    return regmap_write(sc->regmap, SC8989X_REG7D, SC8989X_KEY4);
}

static int sc8989x_set_wa(struct sc8989x_chip *sc) {
    int ret;
    int val;

    ret = regmap_read(sc->regmap, SC8989X_DPDM3, &val);
    if (ret < 0) {
        sc8989x_set_key(sc);
    }

    regmap_write(sc->regmap, SC8989X_DPDM3, SC8989X_PRIVATE);

    return sc8989x_set_key(sc);
}

__maybe_unused static int sc8989x_set_vbat_lsb(struct sc8989x_chip *sc, bool en) {
    int ret;
    int val;

    ret = sc8989x_field_read(sc, VBAT_REG_LSB, &val);
    if (ret < 0) {
        sc8989x_set_key(sc);
    }

    sc8989x_field_write(sc, VBAT_REG_LSB, en);

    return sc8989x_set_key(sc);
}

__maybe_unused static int sc8989x_adc_ibus_en(struct sc8989x_chip *sc, bool en) {
    int ret;
    int val;

    ret = regmap_read(sc->regmap, SC8989X_ADC_EN, &val);
    if (ret < 0) {
        sc8989x_set_key(sc);
    }
    ret = regmap_read(sc->regmap, SC8989X_ADC_EN, &val);
    val = en ? val | BIT(2) : val & ~BIT(2);

    regmap_write(sc->regmap, SC8989X_ADC_EN, val);

    return sc8989x_set_key(sc);    
}

__maybe_unused static int sc8989x_get_adc_ibus(struct sc8989x_chip *sc, int *val)
{
    int ret;
    int reg = 0;

    ret = sc8989x_field_read(sc, ADC_IBUS, &reg);
    if (ret < 0) {
        sc8989x_set_key(sc);
    }
    ret = sc8989x_field_read(sc, ADC_IBUS, &reg);
    *val = reg2val(SC8989X_IBUS, (u8)reg);
    
    return 0;
}

__maybe_unused static int __sc8989x_get_adc(struct sc8989x_chip *sc, enum sc8989x_adc_channel chan,
                            int *val) {
    int reg_val, ret = 0;
    enum sc8989x_fields field_id;
    enum sc8989x_reg_range range_id;
    //stat adc conversion  default: one shot
    sc8989x_field_write(sc, CONV_START, true);
    msleep(20);
    switch (chan) {
    case SC8989X_ADC_VBAT:
        field_id = ADC_VBAT;
        range_id = SC8989X_VBAT;
        break;
    case SC8989X_ADC_VSYS:
        field_id = ADC_VSYS;
        range_id = SC8989X_VSYS;
        break;
    case SC8989X_ADC_VBUS:
        field_id = ADC_VBUS;
        range_id = SC8989X_VBUS;
        break;
    case SC8989X_ADC_ICC:
        field_id = ADC_ICC;
        range_id = SC8989X_VBAT;
        break;
    case SC8989X_ADC_IBUS:
        sc8989x_get_adc_ibus(sc, val);
        dev_err(sc->dev, "get_adc channel: %d val: %d", chan, *val);  
        return 0;
    default:
        goto err;    
    }

    ret = sc8989x_field_read(sc, field_id, &reg_val);
    if (ret < 0)
        goto err;
    *val = reg2val(range_id, reg_val);
    dev_err(sc->dev, "get_adc channel: %d val: %d", chan, *val);    
    return 0;
err:
    dev_err(sc->dev, "get_adc fail channel: %d", chan);
    return -EINVAL;
}

__maybe_unused static int sc8989x_set_hiz(struct sc8989x_chip *sc, bool enable) {
    int reg_val = enable ? 1 : 0;

    return sc8989x_field_write(sc, EN_HIZ, reg_val);
}

static int sc8989x_set_iindpm(struct sc8989x_chip *sc, int curr_ma) {
    int reg_val = val2reg(SC8989X_IINDPM, curr_ma);

    sc8989x_field_write(sc, EN_ILIM, 0);

    return sc8989x_field_write(sc, IINDPM, reg_val);
}

static int sc8989x_get_iindpm(struct sc8989x_chip *sc, int *curr_ma) {
    int ret, reg_val;

    ret = sc8989x_field_read(sc, IINDPM, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read iindpm failed(%d)\n", ret);
        return ret;
    }

    *curr_ma = reg2val(SC8989X_IINDPM, reg_val);

    return ret;
}

__maybe_unused static int sc8989x_reset_wdt(struct sc8989x_chip *sc) {
    return sc8989x_field_write(sc, WD_RST, 1);
}

static int sc8989x_set_chg_enable(struct sc8989x_chip *sc, bool enable) {
    int reg_val = enable ? 1 : 0;

    return sc8989x_field_write(sc, CHG_CFG, reg_val);
}

__maybe_unused static int sc8989x_check_chg_enabled(struct sc8989x_chip *sc,
                                                    bool * enable) {
    int ret, reg_val;

    ret = sc8989x_field_read(sc, CHG_CFG, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read charge enable failed(%d)\n", ret);
        return ret;
    }
    *enable = ! !reg_val;

    return ret;
}

__maybe_unused static int sc8989x_set_otg_enable(struct sc8989x_chip *sc,
                                                bool enable) {
    int reg_val = enable ? 1 : 0;

    return sc8989x_field_write(sc, OTG_CFG, reg_val);
}

__maybe_unused static int sc8989x_set_iboost(struct sc8989x_chip *sc,
                                            int curr_ma) {
    int reg_val = val2reg(SC8989X_IBOOST, curr_ma);

    return sc8989x_field_write(sc, IBOOST_LIM, reg_val);
}

static int sc8989x_set_ichg(struct sc8989x_chip *sc, int curr_ma) {
    int reg_val = val2reg(SC8989X_ICHG, curr_ma);
    pr_err("%s:curr_ma=%d\n",__func__,curr_ma);
    return sc8989x_field_write(sc, ICC, reg_val);
}

static int sc8989x_get_ichg(struct sc8989x_chip *sc, int *curr_ma) {
    int ret, reg_val;
    ret = sc8989x_field_read(sc, ICC, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read ICC failed(%d)\n", ret);
        return ret;
    }

    *curr_ma = reg2val(SC8989X_ICHG, reg_val);

    return ret;
}

static int sc8989x_set_term_curr(struct sc8989x_chip *sc, int curr_ma) {
    int reg_val = val2reg(SC8989X_ITERM, curr_ma);
    pr_err("%s: curr_ma=%d,reg_val=%d\n",__func__,curr_ma,reg_val);
    return sc8989x_field_write(sc, ITERM, reg_val);
}

static int sc8989x_get_term_curr(struct sc8989x_chip *sc, int *curr_ma) {
    int ret, reg_val;

    ret = sc8989x_field_read(sc, ITERM, &reg_val);
    if (ret)
        return ret;

    *curr_ma = reg2val(SC8989X_ITERM, reg_val);

    return ret;
}

static int sc8989x_set_en_term(struct sc8989x_chip *sc, int val) {

    pr_err("%s: val=%d\n",__func__ ,val);
    return sc8989x_field_write(sc, EN_ITERM, val);
}

__maybe_unused static int sc8989x_get_en_term(struct sc8989x_chip *sc, int *val) {

    return sc8989x_field_read(sc, EN_ITERM, val);
}

__maybe_unused static int sc8989x_set_safet_timer(struct sc8989x_chip *sc,
                                                bool enable) {
    int reg_val = enable ? 1 : 0;

    return sc8989x_field_write(sc, EN_TIMER, reg_val);
}

__maybe_unused static int sc8989x_check_safet_timer(struct sc8989x_chip *sc,
                                                    bool * enabled) {
    int ret, reg_val;

    ret = sc8989x_field_read(sc, EN_TIMER, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read ICEN_TIMERC failed(%d)\n", ret);
        return ret;
    }

    *enabled = reg_val ? true : false;

    return ret;
}

static int sc8989x_set_vbat(struct sc8989x_chip *sc, int volt_mv) {
    int reg_val = val2reg(SC8989X_VBAT_REG, volt_mv);

    return sc8989x_field_write(sc, CV, reg_val);
}

static int sc8989x_get_vbat(struct sc8989x_chip *sc, int *volt_mv) {
    int ret, reg_val;

    ret = sc8989x_field_read(sc, CV, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read vbat reg failed(%d)\n", ret);
        return ret;
    }

    *volt_mv = reg2val(SC8989X_VBAT_REG, reg_val);

    return ret;
}

static int sc8989x_set_vindpm(struct sc8989x_chip *sc, int volt_mv) {
    int reg_val = val2reg(SC8989X_VINDPM, volt_mv);

    sc8989x_field_write(sc, FORCE_VINDPM, 1);

    return sc8989x_field_write(sc, VINDPM, reg_val);
}

static int sc8989x_get_vindpm(struct sc8989x_chip *sc, int *volt_mv) {
    int ret, reg_val;

    ret = sc8989x_field_read(sc, VINDPM, &reg_val);
    if (ret)
        return ret;

    *volt_mv = reg2val(SC8989X_VINDPM, reg_val);

    return ret;
}

static int sc8989x_force_dpdm(struct sc8989x_chip *sc) {
    int ret;
    int val;

    //Charger_Detect_Init();
    dev_err(sc->dev, "sc8989x_force_dpdm\n");
    sc8989x_set_vindpm(sc, 4800);
    ret = regmap_read(sc->regmap, 0x02, &val);
    if (ret < 0) {
        return ret;
    }
    val |= 0x02;
    ret = regmap_write(sc->regmap, 0x02, val);

    sc->power_good = 0;

    return ret;
}

static int sc8989x_get_charge_stat(struct sc8989x_chip *sc,int *get_chgstat) {
    int ret;
    int chg_stat, vbus_stat;

    ret = sc8989x_field_read(sc, CHG_STAT, &chg_stat);
    if (ret)
        return ret;

    ret = sc8989x_field_read(sc, VBUS_STAT, &vbus_stat);
    if (ret)
        return ret;

    if (vbus_stat == VBUS_STAT_OTG || vbus_stat == VBUS_STAT_NO_INPUT) {
		if(get_chgstat != NULL)
			*get_chgstat = CHG_STAT_NOT_CHARGE;
        return POWER_SUPPLY_STATUS_DISCHARGING;
    } else {
		if(get_chgstat != NULL)
			*get_chgstat = chg_stat;
        switch (chg_stat) {
        case CHG_STAT_NOT_CHARGE:
            return POWER_SUPPLY_STATUS_NOT_CHARGING;
        case CHG_STAT_PRE_CHARGE:
        case CHG_STAT_FAST_CHARGE:
			oz8806_set_charge_full(false);
            return POWER_SUPPLY_STATUS_CHARGING;
        case CHG_STAT_CHARGE_DONE:
			oz8806_set_charge_full(true);
            return POWER_SUPPLY_STATUS_FULL;
        }
    }

    return POWER_SUPPLY_STATUS_UNKNOWN;
}

__maybe_unused static int sc8989x_check_charge_done(struct sc8989x_chip *sc,
                                                    bool * chg_done) {
    int ret, reg_val;

    ret = sc8989x_field_read(sc, CHG_STAT, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read charge stat failed(%d)\n", ret);
        return ret;
    }

    *chg_done = (reg_val == CHG_STAT_CHARGE_DONE) ? true : false;
    return ret;
}

static bool sc8989x_detect_device(struct sc8989x_chip *sc) {
    int ret;
    int val;

    ret = sc8989x_field_read(sc, PN, &val);
    if (ret < 0 || !(val == SC89890H_PN_NUM || val == SC89895_PN_NUM ||
        val == SC8950_PN_NUM || val == SC89890W_PN_NUM)) {
        dev_err(sc->dev, "not find sc8989x, part_no = %d\n", val);
        return false;
    }

    sc->dev_id = val;

    return true;
}

static int sc8989x_dump_register(struct sc8989x_chip *sc) {
    int val;
    ssize_t len = 0;
    int ret, addr;
    char buf[128];

    memset(buf, 0, 128);
    for (addr = 0x0; addr <= 0x14; addr++) {
        ret = regmap_read(sc->regmap, addr, &val);
        if (ret < 0) {
            pr_err("read %02x failed\n", addr);
            return -1;
        } else
            len += snprintf(buf + len, PAGE_SIZE, "%02x:%02x ", addr, val);
    }
    pr_err("%s\n", buf);

    return 0;
}

#ifdef CONFIG_MTK_CLASS
/********************MTK OPS***********************/
static int sc8989x_plug_in(struct charger_device *chg_dev) {
    int ret = 0;
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_err(sc->dev, "%s\n", __func__);

    /* Enable charging */
    ret = sc8989x_set_chg_enable(sc, true);
    if (ret) {
        dev_err(sc->dev, "Failed to enable charging:%d\n", ret);
    }

    return ret;
}

static int sc8989x_plug_out(struct charger_device *chg_dev) {
    int ret = 0;
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_err(sc->dev, "%s\n", __func__);

    ret = sc8989x_set_chg_enable(sc, false);
    if (ret) {
        dev_err(sc->dev, "Failed to disable charging:%d\n", ret);
    }
    return ret;
}

static int sc8989x_enable(struct charger_device *chg_dev, bool en) {
    int ret = 0;
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    ret = sc8989x_set_chg_enable(sc, en);

    dev_err(sc->dev, "%s charger %s\n", en ? "enable" : "disable",
            !ret ? "successfully" : "failed");

    return ret;
}

static int sc8989x_is_enabled(struct charger_device *chg_dev, bool * enabled) {
    int ret;
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    ret = sc8989x_check_chg_enabled(sc, enabled);
    dev_err(sc->dev, "charger is %s\n",
            *enabled ? "charging" : "not charging");

    return ret;
}

static int sc8989x_get_charging_current(struct charger_device *chg_dev,
                                        u32 * curr) {
    int ret = 0;
    int curr_ma;
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    ret = sc8989x_get_ichg(sc, &curr_ma);
    if (!ret) {
        *curr = curr_ma * 1000;
    }

    return ret;
}

static int sc8989x_set_charging_current(struct charger_device *chg_dev,
                                        u32 curr) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_err(sc->dev, "%s: charge curr = %duA\n", __func__, curr);

    return sc8989x_set_ichg(sc, curr / 1000);
}

static int sc8989x_get_input_current(struct charger_device *chg_dev, u32 * curr) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int curr_ma;
    int ret;

    dev_err(sc->dev, "%s\n", __func__);

    ret = sc8989x_get_iindpm(sc, &curr_ma);
    if (!ret) {
        *curr = curr_ma * 1000;
    }

    return ret;
}

static int sc8989x_set_input_current(struct charger_device *chg_dev, u32 curr) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_err(sc->dev, "%s: iindpm curr = %duA\n", __func__, curr);

    return sc8989x_set_iindpm(sc, curr / 1000);
}

static int sc8989x_get_constant_voltage(struct charger_device *chg_dev,
                                        u32 * volt) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int volt_mv;
    int ret;

    dev_err(sc->dev, "%s\n", __func__);

    ret = sc8989x_get_vbat(sc, &volt_mv);
    if (!ret) {
        *volt = volt_mv * 1000;
    }

    return ret;
}

static int sc8989x_set_constant_voltage(struct charger_device *chg_dev,
                                        u32 volt) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_err(sc->dev, "%s: charge volt = %duV\n", __func__, volt);

    return sc8989x_set_vbat(sc, volt / 1000);
}

static int sc8989x_kick_wdt(struct charger_device *chg_dev) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_err(sc->dev, "%s\n", __func__);
    return sc8989x_reset_wdt(sc);
}

static int sc8989x_set_ivl(struct charger_device *chg_dev, u32 volt) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_err(sc->dev, "%s: vindpm volt = %d\n", __func__, volt);

    return sc8989x_set_vindpm(sc, volt / 1000);
}

static int sc8989x_is_charging_done(struct charger_device *chg_dev, bool * done) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int ret;

    ret = sc8989x_check_charge_done(sc, done);

    dev_err(sc->dev, "%s: charge %s done\n", __func__, *done ? "is" : "not");
    return ret;
}

static int sc8989x_get_min_ichg(struct charger_device *chg_dev, u32 * curr) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    *curr = 60 * 1000;
    dev_err(sc->dev, "%s\n", __func__);
    return 0;
}

static int sc8989x_dump_registers(struct charger_device *chg_dev) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_err(sc->dev, "%s\n", __func__);

    return sc8989x_dump_register(sc);
}

static int sc8989x_send_ta_current_pattern(struct charger_device *chg_dev,
                                        bool is_increase) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int ret;
    int i;

    dev_err(sc->dev, "%s: %s\n", __func__, is_increase ?
            "pumpx up" : "pumpx dn");
    //pumpx start
    ret = sc8989x_set_iindpm(sc, 100);
    if (ret)
        return ret;
    msleep(10);

    for (i = 0; i < 6; i++) {
        if (i < 3) {
            sc8989x_set_iindpm(sc, 800);
            is_increase ? msleep(100) : msleep(300);
        } else {
            sc8989x_set_iindpm(sc, 800);
            is_increase ? msleep(300) : msleep(100);
        }
        sc8989x_set_iindpm(sc, 100);
        msleep(100);
    }

    //pumpx stop
    sc8989x_set_iindpm(sc, 800);
    msleep(500);
    //pumpx wdt, max 240ms
    sc8989x_set_iindpm(sc, 100);
    msleep(100);

    return sc8989x_set_iindpm(sc, 1500);
}

static int sc8989x_send_ta20_current_pattern(struct charger_device *chg_dev,
                                            u32 uV) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    u8 val = 0;
    int i;

    if (uV < 5500000) {
        uV = 5500000;
    } else if (uV > 15000000) {
        uV = 15000000;
    }

    val = (uV - 5500000) / 500000;

    dev_err(sc->dev, "%s ta20 vol=%duV, val=%d\n", __func__, uV, val);

    sc8989x_set_iindpm(sc, 100);
    msleep(150);
    for (i = 4; i >= 0; i--) {
        sc8989x_set_iindpm(sc, 800);
        (val & (1 << i)) ? msleep(100) : msleep(50);
        sc8989x_set_iindpm(sc, 100);
        (val & (1 << i)) ? msleep(50) : msleep(100);
    }
    sc8989x_set_iindpm(sc, 800);
    msleep(150);
    sc8989x_set_iindpm(sc, 100);
    msleep(240);

    return sc8989x_set_iindpm(sc, 800);
}

static int sc8989x_set_ta20_reset(struct charger_device *chg_dev) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int curr;
    int ret;

    ret = sc8989x_get_iindpm(sc, &curr);

    ret = sc8989x_set_iindpm(sc, 100);
    msleep(300);
    return sc8989x_set_iindpm(sc, curr);
}

static int sc8989x_get_adc(struct charger_device *chg_dev,
                        enum adc_channel chan, int *min, int *max) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);                        
    enum sc8989x_adc_channel sc_chan;
    switch (chan) {
    case ADC_CHANNEL_VBAT:
        sc_chan = SC8989X_ADC_VBAT;
        break;
    case ADC_CHANNEL_VSYS:
        sc_chan = SC8989X_ADC_VSYS;
        break;
    case ADC_CHANNEL_VBUS:
        sc_chan = SC8989X_ADC_VBUS;
        break;
    case ADC_CHANNEL_IBUS:
        sc_chan = SC8989X_ADC_IBUS;
        break;
    default:
        return -95; 
    }

    __sc8989x_get_adc(sc, sc_chan, min);
    *min = *min * 1000;
    *max = *min;
    return 0;
}

static int sc8989x_set_otg(struct charger_device *chg_dev, bool enable) {
    int ret;
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    ret = sc8989x_set_otg_enable(sc, enable);
    ret |= sc8989x_set_chg_enable(sc, !enable);

    dev_err(sc->dev, "%s OTG %s\n", enable ? "enable" : "disable",
            !ret ? "successfully" : "failed");

    return ret;
}

static int sc8989x_set_safety_timer(struct charger_device *chg_dev, bool enable) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_err(sc->dev, "%s  %s\n", __func__, enable ? "enable" : "disable");

    return sc8989x_set_safet_timer(sc, enable);
}

static int sc8989x_is_safety_timer_enabled(struct charger_device *chg_dev,
                                        bool * enabled) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    return sc8989x_check_safet_timer(sc, enabled);
}

static int sc8989x_set_boost_ilmt(struct charger_device *chg_dev, u32 curr) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_err(sc->dev, "%s otg curr = %d\n", __func__, curr);
    return sc8989x_set_iboost(sc, curr / 1000);
}

static int sc8989x_do_event(struct charger_device *chg_dev, u32 event, u32 args) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    dev_err(sc->dev, "%s\n", __func__);

#ifdef CONFIG_MTK_CHARGER_V4P19
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
#else
    switch (event) {
    case EVENT_FULL:
    case EVENT_RECHARGE:
    case EVENT_DISCHARGE:
        power_supply_changed(sc->psy);
        break;
    default:
        break;
    }
#endif /*CONFIG_MTK_CHARGER_V4P19*/

    return 0;
}

static int sc8989x_enable_hz(struct charger_device *chg_dev, bool enable) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_err(sc->dev, "%s %s\n", __func__, enable ? "enable" : "disable");

    return sc8989x_set_hiz(sc, enable);
}

static int sc8989x_enable_powerpath(struct charger_device *chg_dev, bool en) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    u32 mivr = (en ? sc->cfg->vindpm : SC8989X_VINDPM_MAX);
    int reg_val = 0;

    dev_err(sc->dev, "%s en = %d\n", __func__, en);

    reg_val = val2reg(SC8989X_VINDPM, mivr);
    sc8989x_field_write(sc, FORCE_VINDPM, true);
    return sc8989x_field_write(sc, VINDPM, reg_val);
}

static int sc8989x_is_powerpath_enabled(struct charger_device *chg_dev,
                                        bool * en) {
    int ret;
    int val = 0;
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    ret = sc8989x_field_read(sc, VINDPM, &val);
    if (!ret) {
        val = reg2val(SC8989X_VINDPM, (u8) val);
        *en = (val == SC8989X_VINDPM_MAX) ? false : true;
    }
    return ret;
}

static int sc8989x_set_constant_voltage(struct sc8989x_chip *sc, u32 volt) {
    dev_err(sc->dev, "%s: charge volt = %duV\n", __func__, volt);
    return sc8989x_set_vbat(sc, volt / 1000);
}

static struct charger_ops sc8989x_chg_ops = {
    /* Normal charging */
    .plug_in = sc8989x_plug_in,
    .plug_out = sc8989x_plug_out,
    .enable = sc8989x_enable,
    .is_enabled = sc8989x_is_enabled,
    .get_charging_current = sc8989x_get_charging_current,
    .set_charging_current = sc8989x_set_charging_current,
    .get_input_current = sc8989x_get_input_current,
    .set_input_current = sc8989x_set_input_current,
    .get_constant_voltage = sc8989x_get_constant_voltage,
    .set_constant_voltage = sc8989x_set_constant_voltage,
    .kick_wdt = sc8989x_kick_wdt,
    .set_mivr = sc8989x_set_ivl,
    .is_charging_done = sc8989x_is_charging_done,
    .get_min_charging_current = sc8989x_get_min_ichg,
    .dump_registers = sc8989x_dump_registers,

    /* Safety timer */
    .enable_safety_timer = sc8989x_set_safety_timer,
    .is_safety_timer_enabled = sc8989x_is_safety_timer_enabled,

    /* Power path */
    .enable_powerpath = sc8989x_enable_powerpath,
    .is_powerpath_enabled = sc8989x_is_powerpath_enabled,

    /* OTG */
    .enable_otg = sc8989x_set_otg,
    .set_boost_current_limit = sc8989x_set_boost_ilmt,
    .enable_discharge = NULL,

    /* PE+/PE+20 */
    .send_ta_current_pattern = sc8989x_send_ta_current_pattern,
    .set_pe20_efficiency_table = NULL,
    .send_ta20_current_pattern = sc8989x_send_ta20_current_pattern,
    .reset_ta = sc8989x_set_ta20_reset,
    .enable_cable_drop_comp = NULL,

    /* ADC */
    .get_adc = sc8989x_get_adc,

    .event = sc8989x_do_event,
    .enable_hz = sc8989x_enable_hz,
};

static const struct charger_properties sc8989x_chg_props = {
    .alias_name = "sc8989x_chg",
};

#ifdef CONFIG_MTK_CHARGER_V4P19
static void sc8989x_inform_psy_dwork_handler(struct work_struct *work) 
{
    int ret = 0;
    union power_supply_propval propval;
    struct sc8989x_chip *sc = container_of(work,
                                        struct sc8989x_chip,
                                        psy_dwork.work);
    if (!sc->chg_psy) {
        sc->chg_psy = power_supply_get_by_name("charger");
        if (!sc->chg_psy) {
            pr_err("%s get power supply fail\n", __func__);
            mod_delayed_work(system_wq, &sc->psy_dwork, 
                    msecs_to_jiffies(2000));
            return ;
        }
    }

    if (sc->adp_type != CHARGER_UNKNOWN)
        propval.intval = 1;
    else
        propval.intval = 0;

    ret = power_supply_set_property(sc->chg_psy, POWER_SUPPLY_PROP_ONLINE,
                    &propval);

    if (ret < 0)
        pr_notice("inform power supply online failed:%d\n", ret);

    propval.intval = sc->adp_type;

    ret = power_supply_set_property(sc->chg_psy,
                    POWER_SUPPLY_PROP_CHARGE_TYPE,
                    &propval);
    if (ret < 0)
        pr_notice("inform power supply charge type failed:%d\n", ret);

}
#endif /*CONFIG_MTK_CHARGER_V4P19*/

static void sc8989x_set_chg_type(struct sc8989x_chip *sc, int type)
{
#ifdef CONFIG_MTK_CHARGER_V4P19
    switch (type) {
        case VBUS_STAT_NO_INPUT:
            sc->adp_type = CHARGER_UNKNOWN;
            break;
        case VBUS_STAT_SDP:
        case VBUS_STAT_CDP:
        case VBUS_STAT_UNKOWN:
            sc->adp_type = STANDARD_HOST;
            //Charger_detect_Release();
            break;
        case VBUS_STAT_DCP:
        case VBUS_STAT_HVDCP:
            sc->adp_type = STANDARD_CHARGER;
            break;
        case VBUS_STAT_NONSTAND:
            sc->adp_type = NONSTANDARD_CHARGER;
            break;
        default:
            sc->adp_type = CHARGER_UNKNOWN;
            break;
    }

    schedule_delayed_work(&sc->psy_dwork, 0);
#endif /*CONFIG_MTK_CHARGER_V4P19*/
}

#endif /*CONFIG_MTK_CLASS */

/**********************interrupt*********************/
static void sc8989x_force_detection_dwork_handler(struct work_struct *work) {
    int ret;
    struct sc8989x_chip *sc = container_of(work,
                                        struct sc8989x_chip,
                                        force_detect_dwork.work);

    ret = sc8989x_force_dpdm(sc);
    if (ret) {
        dev_err(sc->dev, "%s: force dpdm failed(%d)\n", __func__, ret);
        return;
    }

    sc->force_detect_count++;
}

__maybe_unused static int sc8989x_set_dp(struct sc8989x_chip *sc, int val)
{
	pr_err("%s set dp_drive = %d\n", __func__, val);
	return sc8989x_field_write(sc, DP_DRIVE, val);
}

__maybe_unused static int sc8989x_set_dm(struct sc8989x_chip *sc, int val)
{
	pr_err("%s set dm_drive = %d\n", __func__, val);
	return sc8989x_field_write(sc, DM_DRIVE, val);
}

static int sc8989x_get_charger_type(struct sc8989x_chip *sc) {
    int ret;
    int reg_val = 0;

    ret = sc8989x_field_read(sc, VBUS_STAT, &reg_val);
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
        sc8989x_request_dpdm(sc, false);
        sc8989x_set_vindpm(sc, 4600);
        if(sc->tcpc->pd_port.pd_connect_state == PD_CONNECT_PE_READY_SNK ||
		sc->tcpc->pd_port.pd_connect_state == PD_CONNECT_PE_READY_SNK_PD30){
            sc8989x_set_iindpm(sc,1200);
            sc8989x_set_ichg(sc,1200);
        }else{
            sc8989x_set_ichg(sc, CHG_SDP_CURR_MAX);
        }
        break;
    case VBUS_STAT_CDP:
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
        sc->chg_type = POWER_SUPPLY_TYPE_USB_CDP;
	sc8989x_set_vindpm(sc, VINDPM_USB_CDP);
        sc8989x_request_dpdm(sc, false);
        sc8989x_set_ichg(sc, CHG_CDP_CURR_MAX);
        break;
    case VBUS_STAT_DCP:
    case VBUS_STAT_HVDCP:
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
        sc->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
	    if (sc->afc_running)
	        break;
	    sc->afc_running = 1;
	    pr_err("afc deteced\n");
	    sc8989x_request_dpdm(sc, true);
	    sc8989x_set_dp(sc, REG01_DPDM_OUT_0P6V);
	    sc8989x_set_iindpm(sc, 500);
	    sc8989x_set_vindpm(sc, 4600);
	    sc8989x_set_ichg(sc, CHG_AFC_CURR_MAX);
	    schedule_delayed_work(&sc->hvdcp_dwork, msecs_to_jiffies(1400));
        break;
    case VBUS_STAT_UNKOWN:
        sc8989x_set_vindpm(sc, 4600);
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_FLOAT;
        sc->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
        if (sc->force_detect_count < 10) {
            schedule_delayed_work(&sc->force_detect_dwork,
                                msecs_to_jiffies(100));
        }
        break;
    case VBUS_STAT_NONSTAND:
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
        sc8989x_request_dpdm(sc, false);
        sc8989x_set_ichg(sc, CHG_SDP_CURR_MAX);
        break;
    default:
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_FLOAT;
        sc->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
        break;
    }

#ifdef CONFIG_MTK_CLASS
    sc8989x_set_chg_type(sc, reg_val);
#endif /*CONFIG_MTK_CLASS */

    dev_err(sc->dev, "%s vbus stat: 0x%02x,pd_active:%d\n", __func__, reg_val,sc->pd_active);

    return ret;
}

static int sc8989x_set_usb(struct sc8989x_chip *sc, bool online_val)
{
	int ret = 0;
	union power_supply_propval propval;
	if(sc->usb_psy == NULL) {
		sc->usb_psy = power_supply_get_by_name("usb");
		if (sc->usb_psy == NULL) {
			pr_err("%s : fail to get psy usb\n", __func__);
			return -ENODEV;
		}
	}
	propval.intval = online_val;
	ret = power_supply_set_property(sc->usb_psy, POWER_SUPPLY_PROP_ONLINE, &propval);
	if (ret < 0)
		pr_err("%s : set sc8989x_set_usb fail ret:%d\n", __func__, ret);
	return ret;
}

static irqreturn_t sc8989x_irq_handler(int irq, void *data) {
    int ret;
    int reg_val;
    bool prev_vbus_gd;
    int sc8989x_term_curr = SC8989X_TERM_OFFSET;
    struct sc8989x_chip *sc = (struct sc8989x_chip *)data;

    dev_err(sc->dev, "%s: sc8989x_irq_handler\n", __func__);

    ret = sc8989x_field_read(sc, VBUS_GD, &reg_val);
    if (ret) {
        return IRQ_HANDLED;
    }
    prev_vbus_gd = sc->vbus_good;
    sc->vbus_good = ! !reg_val;

    if (!prev_vbus_gd && sc->vbus_good) {
        sc->force_detect_count = 0;
        sc8989x_field_write(sc, EN_ILIM, false);
        dev_err(sc->dev, "%s: adapter/usb inserted\n", __func__);
        sc8989x_request_dpdm(sc, true);
        sc8989x_term_curr = sc->cfg->iterm * SC8989X_TERM_STEP + SC8989X_TERM_OFFSET;
        sc8989x_set_term_curr(sc, sc8989x_term_curr);
		sc8989x_set_en_term(sc, 1);
		sc8989x_set_usb(sc,true);
        vote(sc->usb_icl_votable, DCP_CHG_VOTER, false, 0);
		vote(sc->usb_icl_votable, PD_CHG_VOTER, false, 0);
		vote(sc->fcc_votable, DCP_CHG_VOTER, false, 0);
//#ifndef CONFIG_MTK_CHARGER_V5P10
        schedule_delayed_work(&sc->force_detect_dwork,
                            msecs_to_jiffies(100));
//#endif /* CONFIG_MTK_CHARGER_V5P10 */
    } else if (prev_vbus_gd && !sc->vbus_good) {
        dev_err(sc->dev, "%s: adapter/usb removed\n", __func__);
        vote(sc->usb_icl_votable, DCP_CHG_VOTER, false, 0);
		vote(sc->usb_icl_votable, PD_CHG_VOTER, false, 0);
		vote(sc->fcc_votable, DCP_CHG_VOTER, false, 0);
		vote(sc->fcc_votable, AFC_CHG_DARK_VOTER, false, 0);
        sc->afc_running = 0;
		sc8989x_set_usb(sc,false);
	sc->charge_afc = 0;
        sc8989x_set_iio_channel(sc, NOPMI, NOPMI_CHG_CHARGER_AFC,sc->charge_afc);
        sc8989x_request_dpdm(sc, false);
        sc8989x_set_iio_channel(sc, NOPMI, NOPMI_CHG_INPUT_SUSPEND, 0);
    }

    if(sc->force_detect_count != 0)
        sc8989x_get_charger_type(sc);
    //power_supply_changed(sc->psy);
    if (sc->bat_psy == NULL)
        sc->bat_psy = power_supply_get_by_name("battery");
    if (sc->bat_psy)
        power_supply_changed(sc->bat_psy);

    sc8989x_dump_register(sc);
    return IRQ_HANDLED;
}

static int sc8989x_get_pd_active(struct sc8989x_chip *sc)
{
	int pd_active = 0;
	int rc = 0;
	if (!sc->tcpc)
		pr_err("tcpc_dev is null\n");
	else {
		rc = tcpm_get_pd_connect_type(sc->tcpc);
		pr_err("%s: pd is %d\n", __func__, rc);
		if ((rc == PD_CONNECT_PE_READY_SNK_APDO) || (rc == PD_CONNECT_PE_READY_SNK_PD30)
			|| (rc == PD_CONNECT_PE_READY_SNK)) {
			pd_active = 1;
		} else {
			pd_active = 0;
		}
	}


	if (sc->pd_active != pd_active) {
		sc->pd_active = pd_active;
	}
	return pd_active;
}

static int sc8989x_get_pdo_active(struct sc8989x_chip *sc)
{
	int pdo_active = 0;
	int rc = 0;
	if (!sc->tcpc)
		pr_err("tcpc_dev is null\n");
	else {
		rc = tcpm_get_pd_connect_type(sc->tcpc);
		pr_err("%s: pdo is %d\n", __func__, rc);
		if ((rc == PD_CONNECT_PE_READY_SNK_PD30) || (rc == PD_CONNECT_PE_READY_SNK)) {
			pdo_active = 1;
		} else {
			pdo_active = 0;
		}
	}

	return pdo_active;
}

static void sc8989x_disable_pdo_dwork(struct work_struct *work)
{
	int pdo_icl_current = 0;
	struct sc8989x_chip *sc = container_of(
		work, struct sc8989x_chip, disable_pdo_dwork.work);

	if (sc->is_disble_afc) {
		pdo_icl_current = PD_ICL_CURR_MAX;
	} else {
		pdo_icl_current = DCP_ICL_CURR_MAX;
	}

	sc8989x_set_iindpm(sc, pdo_icl_current);

}

static void sc8989x_disable_pdo(struct sc8989x_chip *sc)
{
	sc8989x_set_iindpm(sc, 500);

	cancel_delayed_work_sync(&sc->disable_pdo_dwork);
	schedule_delayed_work(&sc->disable_pdo_dwork, msecs_to_jiffies(500));
}

static void sc8989x_disable_afc_dwork(struct work_struct *work)
{
	unsigned int afc_code = AFC_QUICK_CHARGE_POWER_CODE;
	int afc_icl_current = 0;
	int afc_icharge_current = 0;
	int ret = 0;
	struct sc8989x_chip *sc = container_of(
		work, struct sc8989x_chip, disable_afc_dwork.work);

	if (sc->is_disble_afc) {
		afc_code = AFC_COMMON_CHARGE_POWER_CODE;
		afc_icl_current = AFC_COMMON_ICL_CURR_MAX;
		afc_icharge_current = CHG_AFC_COMMON_CURR_MAX;
	} else {
		afc_code = AFC_QUICK_CHARGE_POWER_CODE;
		afc_icl_current = AFC_ICL_CURR_MAX;
		afc_icharge_current = CHG_AFC_CURR_MAX;
	}

	pr_err("%s: afc_code=%x\n", __func__,afc_code);

	ret = afc_set_voltage_workq(afc_code);
	if (!ret) {
		pr_info("set afc adapter iindpm\n");
		sc8989x_set_iindpm(sc, afc_icl_current);
		sc8989x_set_ichg(sc, afc_icharge_current);
	}
}

static void sc8989x_disable_afc(struct sc8989x_chip *sc)
{
	sc8989x_set_iindpm(sc, 500);

	cancel_delayed_work_sync(&sc->disable_afc_dwork);
	schedule_delayed_work(&sc->disable_afc_dwork, msecs_to_jiffies(500));
}

static void sc89890h_hvdcp_dwork(struct work_struct *work)
{
	int ret;
	unsigned int afc_code = AFC_QUICK_CHARGE_POWER_CODE;
	int afc_icl_current = 0;
	int pd_active = 0;
	int pd_pdo_active = 0;
	struct sc8989x_chip *sc = container_of(
		work, struct sc8989x_chip, hvdcp_dwork.work);

	pr_err("is_disble_afc=%d\n", sc->is_disble_afc);
	if (sc->is_disble_afc) {
		afc_code = AFC_COMMON_CHARGE_POWER_CODE;
		afc_icl_current = AFC_COMMON_ICL_CURR_MAX;
	} else {
		afc_code = AFC_QUICK_CHARGE_POWER_CODE;
		afc_icl_current = AFC_ICL_CURR_MAX;
	}

	ret = afc_set_voltage_workq(afc_code);
	if (!ret) {
		pr_err("set afc adapter iindpm to 1700mA\n");
		sc8989x_set_iindpm(sc, afc_icl_current);
		sc->charge_afc = 1;
                sc8989x_set_iio_channel(sc, NOPMI, NOPMI_CHG_CHARGER_AFC,sc->charge_afc);
		sc8989x_set_term_curr(sc, AFC_CHARGE_ITERM);
		if (sc->is_disble_afc) {
			sc8989x_set_ichg(sc, CHG_AFC_COMMON_CURR_MAX);
		}
	} else {
		pr_err("afc communication failed, restore adapter iindpm to 2000mA\n");
		//sc8989x_set_iindpm(sc, 2100);
		pd_active = sc8989x_get_pd_active(sc);
		pd_pdo_active = sc8989x_get_pdo_active(sc);
		if (sc->is_disble_afc && (pd_active == 1)) {
			vote(sc->usb_icl_votable, PD_CHG_VOTER, true, PD_ICL_CURR_MAX);
			sc8989x_set_ichg(sc, CHG_DCP_CURR_MAX);
		}  else if (pd_pdo_active == 1) {
			vote(sc->usb_icl_votable, DCP_CHG_VOTER, true, 1600);
			sc8989x_set_ichg(sc, CHG_PDO_CURR_MAX);
		} else {
			vote(sc->usb_icl_votable, DCP_CHG_VOTER, true, DCP_ICL_CURR_MAX);
			sc8989x_set_ichg(sc, CHG_DCP_CURR_MAX);
		}
		//sc8989x_set_ichg(sc, CHG_DCP_CURR_MAX);
			//sc8989x_set_dp(sc, 0);
		sc->charge_afc = 0;

		pr_err("ch_log_qc   pd_active[%d]   charge_afc[%d]\n",sc->pd_active,sc->charge_afc);
		if (!sc->pd_active)
		    schedule_delayed_work(&sc->hvdcp_qc20_dwork, msecs_to_jiffies(300));
	}
}

static void sc89890h_hvdcp_qc20_dwork(struct work_struct *work)
{
	int ret;
	union power_supply_propval pval = {0, };
	struct sc8989x_chip *sc = container_of(
		work, struct sc8989x_chip, hvdcp_qc20_dwork.work);

	__pm_stay_awake(sc->hvdcp_qc20_ws);

	ret = sc8989x_set_dp(sc, REG01_DPDM_OUT_0P6V);
	if (ret) {
		pr_err("set dp 0.6v failed, ret:%d\n", ret);
		goto qc20_out;
	}

	ret = sc8989x_set_dm(sc, 1);
	if (ret) {
		pr_err("set dm 0v failed, ret:%d\n", ret);
		goto qc20_out;
	}

	msleep(300);

	ret = sc8989x_set_dp(sc, 6);
	if (ret) {
		pr_err("set dp 3.3v failed, ret:%d\n", ret);
		goto qc20_out;
	}

	ret = sc8989x_set_dm(sc, 2);
	if (ret) {
		pr_err("set dm 0.6v failed, ret:%d\n", ret);
		goto qc20_out;
	}

	msleep(300);
	pr_err("hvdcp qc20 detected\n");

	if (sc->cp_psy == NULL)
	{
		sc->cp_psy = power_supply_get_by_name("charger_standalone");
		if (sc->cp_psy == NULL) {
			pr_err("power_supply_get_by_name bms fail !\n");
			goto qc20_out;
		}
	}
	power_supply_get_property(sc->cp_psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &pval);
	pr_err("ch_log_qc [vbus][%d] [pd_active][%d]\n",pval.intval,sc->pd_active);
	if (pval.intval > 8000  && !sc->pd_active) {
		sc8989x_set_iindpm(sc, AFC_ICL_CURR_MAX);
		sc8989x_set_ichg(sc, CHG_AFC_CURR_MAX);
		sc8989x_dump_register(sc);
	} else {
		goto qc20_out;
	}

	__pm_relax(sc->hvdcp_qc20_ws);
	return;
qc20_out:
	sc8989x_set_iindpm(sc, DCP_ICL_CURR_MAX);
	sc8989x_set_ichg(sc, CHG_DCP_CURR_MAX);
	sc8989x_set_dp(sc, 0);
	__pm_relax(sc->hvdcp_qc20_ws);
	return;
}

/**********************system*********************/
static int sc8989x_parse_dt(struct sc8989x_chip *sc) {
    struct device_node *np = sc->dev->of_node;
    int i;
    int ret = 0;
    struct {
        char *name;
        int *conv_data;
    } props[] = {
        {"sc,sc8989x,ico-en", &(sc->cfg->ico_en)}, 
        {"sc,sc8989x,hvdcp-en", &(sc->cfg->hvdcp_en)}, 
        {"sc,sc8989x,auto-dpdm-en", &(sc->cfg->auto_dpdm_en)}, 
        {"sc,sc8989x,vsys-min", &(sc->cfg->vsys_min)}, 
        {"sc,sc8989x,vbatmin-sel", &(sc->cfg->vbatmin_sel)}, 
        {"sc,sc8989x,itrick", &(sc->cfg->itrick)}, 
        {"sc,sc8989x,iterm", &(sc->cfg->iterm)}, 
        {"sc,sc8989x,vbat-cv", &(sc->cfg->vbat_cv)}, 
        {"sc,sc8989x,vbat-low", &(sc->cfg->vbat_low)}, 
        {"sc,sc8989x,vrechg", &(sc->cfg->vrechg)}, 
        {"sc,sc8989x,en-term", &(sc->cfg->en_term)}, 
        {"sc,sc8989x,stat-dis", &(sc->cfg->stat_dis)}, 
        {"sc,sc8989x,wd-time", &(sc->cfg->wd_time)}, 
        {"sc,sc8989x,en-timer", &(sc->cfg->en_timer)}, 
        {"sc,sc8989x,charge-timer", &(sc->cfg->charge_timer)}, 
        {"sc,sc8989x,bat-comp", &(sc->cfg->bat_comp)}, 
        {"sc,sc8989x,vclamp", &(sc->cfg->vclamp)}, 
        {"sc,sc8989x,votg", &(sc->cfg->votg)}, 
        {"sc,sc8989x,iboost", &(sc->cfg->iboost)}, 
        {"sc,sc8989x,vindpm", &(sc->cfg->vindpm)},};

    sc->irq_gpio = of_get_named_gpio(np, "sc,intr-gpio", 0);
    if (sc->irq_gpio < 0)
        dev_err(sc->dev, "%s sc,intr-gpio is not available\n", __func__);

    if (of_property_read_string(np, "charger_name", &sc->cfg->chg_name) < 0)
        dev_err(sc->dev, "%s no charger name\n", __func__);

    /* initialize data for optional properties */
    for (i = 0; i < ARRAY_SIZE(props); i++) {
        ret = of_property_read_u32(np, props[i].name, props[i].conv_data);
        if (ret < 0) {
            dev_err(sc->dev, "%s not find\n", props[i].name);
            continue;
        }
    }

    return 0;
}

static int sc8989x_init_device(struct sc8989x_chip *sc) {
    int ret = 0;
    int i;

    struct {
        enum sc8989x_fields field_id;
        int conv_data;
    } props[] = {
        {ICO_EN, sc->cfg->ico_en}, 
        {HVDCP_EN, sc->cfg->hvdcp_en}, 
        {AUTO_DPDM_EN, sc->cfg->auto_dpdm_en}, 
        {VSYS_MIN, sc->cfg->vsys_min}, 
        {VBATMIN_SEL, sc->cfg->vbatmin_sel}, 
        {ITC, sc->cfg->itrick}, 
        {ITERM, sc->cfg->iterm}, 
        {CV, sc->cfg->vbat_cv}, 
        {VBAT_LOW, sc->cfg->vbat_low}, 
        {VRECHG, sc->cfg->vrechg}, 
        {EN_ITERM, sc->cfg->en_term}, 
        {STAT_DIS, sc->cfg->stat_dis}, 
        {TWD, sc->cfg->wd_time}, 
        {EN_TIMER, sc->cfg->en_timer}, 
        {TCHG, sc->cfg->charge_timer}, 
        {BAT_COMP, sc->cfg->bat_comp}, 
        {VCLAMP, sc->cfg->vclamp}, 
        {V_OTG, sc->cfg->votg}, 
        {IBOOST_LIM, sc->cfg->iboost},
        {VINDPM, sc->cfg->vindpm},};

    //reg reset;
    sc8989x_field_write(sc, REG_RST, 1);

    for (i = 0; i < ARRAY_SIZE(props); i++) {
        dev_err(sc->dev, "%d--->%d\n", props[i].field_id, props[i].conv_data);
        ret = sc8989x_field_write(sc, props[i].field_id, props[i].conv_data);
    }
    
    sc8989x_adc_ibus_en(sc, true);

    sc8989x_set_wa(sc);

    sc8989x_field_write(sc, BATFET_RST_EN, 0);

    return sc8989x_dump_register(sc);
}

static int sc8989x_register_interrupt(struct sc8989x_chip *sc) {
    int ret = 0;

    ret = devm_gpio_request(sc->dev, sc->irq_gpio, "chr-irq");
    if (ret < 0) {
        dev_err(sc->dev, "failed to request GPIO%d ; ret = %d", sc->irq_gpio, ret);
        return ret;
    }

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

    ret = devm_request_threaded_irq(sc->dev, sc->irq, NULL,
                                    sc8989x_irq_handler,
                                    IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                                    "chr_stat", sc);
    if (ret < 0) {
        dev_err(sc->dev, "request thread irq failed:%d\n", ret);
        return ret;
    } else {
        dev_err(sc->dev, "request thread irq pass:%d  sc->irq =%d\n", ret, sc->irq);
    }

    enable_irq_wake(sc->irq);

    return 0;
}

static void determine_initial_status(struct sc8989x_chip *sc) {
    sc8989x_irq_handler(sc->irq, (void *)sc);
}

//reigster
static ssize_t sc8989x_show_registers(struct device *dev,
                                    struct device_attribute *attr, char *buf) 
{
    struct sc8989x_chip *sc = dev_get_drvdata(dev);
    uint8_t addr;
    unsigned int val;
    uint8_t tmpbuf[300];
    int len;
    int idx = 0;
    int ret;

    idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc8989x");
    for (addr = 0x1; addr <= 0x14; addr++) {
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

static ssize_t sc8989x_store_register(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf, size_t count) {
    struct sc8989x_chip *sc = dev_get_drvdata(dev);
    int ret;
    unsigned int val;
    unsigned int reg;
    ret = sscanf(buf, "%x %x", &reg, &val);
    if (ret == 2 && reg <= 0x14)
        regmap_write(sc->regmap, (unsigned char)reg, (unsigned char)val);

    return count;
}

static int sc8989x_set_batfet(struct sc8989x_chip *sc, bool enable) {
	int reg_val = enable ? 1 : 0;
	sc8989x_field_write(sc, BATFET_DLY, reg_val);

	return sc8989x_field_write(sc, BATFET_DIS, reg_val);
}

static ssize_t sc8989x_store_shipmode(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf, size_t count) {
	struct sc8989x_chip *sc = dev_get_drvdata(dev);

	if (buf[0] == '1') {
		sc->entry_shipmode = true;
	}
	return count;
}


static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, sc8989x_show_registers,
                sc8989x_store_register);

static DEVICE_ATTR(shipmode, S_IWUSR, NULL, sc8989x_store_shipmode);

static void sc8989x_create_device_node(struct device *dev) {
    device_create_file(dev, &dev_attr_registers);
	device_create_file(dev, &dev_attr_shipmode);
}

static enum power_supply_property sc8989x_chg_psy_properties[] = {
    POWER_SUPPLY_PROP_MANUFACTURER,
    POWER_SUPPLY_PROP_ONLINE,
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
    POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
    POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
    POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
    POWER_SUPPLY_PROP_USB_TYPE,
    POWER_SUPPLY_PROP_CHARGE_TYPE,
    POWER_SUPPLY_PROP_CURRENT_MAX,
    POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static enum power_supply_usb_type sc8989x_chg_psy_usb_types[] = {
    POWER_SUPPLY_USB_TYPE_UNKNOWN,
    POWER_SUPPLY_USB_TYPE_SDP,
    POWER_SUPPLY_USB_TYPE_CDP,
    POWER_SUPPLY_USB_TYPE_DCP,
};

static int sc8989x_chg_property_is_writeable(struct power_supply *psy,
                                            enum power_supply_property psp) {
    int ret;

    switch (psp) {
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
    case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
    case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
    case POWER_SUPPLY_PROP_STATUS:
    case POWER_SUPPLY_PROP_ONLINE:
    case POWER_SUPPLY_PROP_ENERGY_EMPTY:
        ret = 1;
        break;

    default:
        ret = 0;
    }

    return ret;
}

static int sc8989x_chg_get_property(struct power_supply *psy,
                                    enum power_supply_property psp,
                                    union power_supply_propval *val) {
    struct sc8989x_chip *sc = power_supply_get_drvdata(psy);
    int ret = 0;
    int data = 0;

    if (!sc) {
        dev_err(sc->dev, "%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

    switch (psp) {
    case POWER_SUPPLY_PROP_MANUFACTURER:
        val->strval = "SouthChip";
        break;
    case POWER_SUPPLY_PROP_ONLINE:
        val->intval = sc->vbus_good;
        break;
    case POWER_SUPPLY_PROP_STATUS:
        ret = sc8989x_get_charge_stat(sc,NULL);
        if (ret < 0)
            break;
        val->intval = ret;
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        ret = sc8989x_get_ichg(sc, &data);
        if (ret)
            break;
        val->intval = data * 1000;
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
        ret = sc8989x_get_vbat(sc, &data);
        if (ret < 0)
            break;
        val->intval = data * 1000;
        break;
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        ret = sc8989x_get_iindpm(sc, &data);
        if (ret < 0)
            break;
        val->intval = data * 1000;
        break;
    case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
        ret = sc8989x_get_vindpm(sc, &data);
        if (ret < 0)
            break;
        val->intval = data * 1000;
        break;
    case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
        ret = sc8989x_get_term_curr(sc, &data);
        if (ret < 0)
            break;
        val->intval = data + IC_ITERM_OFFSET;
        break;
    case POWER_SUPPLY_PROP_USB_TYPE:
        val->intval = sc->psy_usb_type;
        break;
    case POWER_SUPPLY_PROP_CHARGE_TYPE:
         val->intval = sc->psy_usb_type;
         break;
    case POWER_SUPPLY_PROP_CURRENT_MAX:
        if (sc->psy_usb_type == POWER_SUPPLY_USB_TYPE_SDP)
            val->intval = 500000;
        else
            val->intval = 1500000;
        break;
    case POWER_SUPPLY_PROP_VOLTAGE_MAX:
        if (sc->psy_usb_type == POWER_SUPPLY_USB_TYPE_SDP)
            val->intval = 5000000;
        else
            val->intval = 12000000;
        break;
    case POWER_SUPPLY_PROP_TYPE:
        val->intval = sc->psy_usb_type;
        break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = sc89890_driver_name;
		break;
    default:
        pr_err("unsupport get power_supply_property: %d.\n",psp);
        ret = -EINVAL;
        break;
    }
    return ret;
}

static int sc8989x_chg_set_property(struct power_supply *psy,
                                    enum power_supply_property psp,
                                    const union power_supply_propval *val) {
    struct sc8989x_chip *sc = power_supply_get_drvdata(psy);
    int ret = 0;
    dev_err(sc->dev, "%s  %d\n", __func__, val->intval);
    switch (psp) {
    case POWER_SUPPLY_PROP_ONLINE:
        if (val->intval == 2) {
            schedule_delayed_work(&sc->force_detect_dwork,
                                msecs_to_jiffies(300));
        } else if (val->intval == 4) {
            sc->psy_desc.type = POWER_SUPPLY_TYPE_USB;
            sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
        }
        break;
    case POWER_SUPPLY_PROP_STATUS:
        ret = sc8989x_set_chg_enable(sc, ! !val->intval);
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        ret = sc8989x_set_ichg(sc, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
        ret = sc8989x_set_vbat(sc, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        ret = sc8989x_set_iindpm(sc, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
        ret = sc8989x_set_vindpm(sc, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
        if (val->intval == 0) {
            ret = sc8989x_set_en_term(sc, 1);
        } else if (val->intval == -1) {
            ret = sc8989x_set_en_term(sc, 0);
        } else {
            ret = sc8989x_set_term_curr(sc, val->intval);
        }
        break;
    default:
        pr_err("unsupport set power_supply_property: %d.\n",psp);
        ret = -EINVAL;
        break;
    }
    return ret;
}

static char *sc8989x_psy_supplied_to[] = {
    "battery",
    "mtk-master-charger",
};

static const struct power_supply_desc sc8989x_psy_desc = {
    .name = "bbc",
    .type = POWER_SUPPLY_TYPE_USB,
    .usb_types = sc8989x_chg_psy_usb_types,
    .num_usb_types = ARRAY_SIZE(sc8989x_chg_psy_usb_types),
    .properties = sc8989x_chg_psy_properties,
    .num_properties = ARRAY_SIZE(sc8989x_chg_psy_properties),
    .property_is_writeable = sc8989x_chg_property_is_writeable,
    .get_property = sc8989x_chg_get_property,
    .set_property = sc8989x_chg_set_property,
};

static int sc8989x_psy_register(struct sc8989x_chip *sc) {
    struct power_supply_config cfg = {
        .drv_data = sc,
        .of_node = sc->dev->of_node,
        .supplied_to = sc8989x_psy_supplied_to,
        .num_supplicants = ARRAY_SIZE(sc8989x_psy_supplied_to),
    };

    memcpy(&sc->psy_desc, &sc8989x_psy_desc, sizeof(sc->psy_desc));
    sc->psy = devm_power_supply_register(sc->dev, &sc->psy_desc, &cfg);
    return IS_ERR(sc->psy) ? PTR_ERR(sc->psy) : 0;
}

static struct of_device_id sc8989x_of_device_id[] = {
    {.compatible = "southchip,sc89890h",},
    {.compatible = "southchip,sc89890w",},
    {.compatible = "southchip,sc89895",},
    {.compatible = "southchip,sc8950",},
    {},
};
MODULE_DEVICE_TABLE(of, sc8989x_of_device_id);

#if 0
/* longcheer nielianjie10 2023.05.17 add vote start*/
static int fcc_vote_callback(struct votable *votable, void *data,
			int fcc_ua, const char *client)
{
	struct sc8989x_chip *sc	= data;
	int rc = 0;

	if (fcc_ua < 0) {
		pr_err("fcc_ua: %d < 0, ERROR!\n", fcc_ua);
		return 0;
	}

	if (fcc_ua > SC8989X_MAX_FCC)
		fcc_ua = SC8989X_MAX_FCC;

	rc = sc8989x_set_charging_current(sc, fcc_ua);
	if (rc < 0) {
		pr_err("failed to set charge current\n");
		return rc;
	}

	pr_info(" fcc:%d\n", fcc_ua);

	return 0;
}

static int chg_dis_vote_callback(struct votable *votable, void *data,
			int disable, const char *client)
{
	struct sc8989x_chip *sc = data;
	int rc = 0;

	if (disable) {
		rc = sc8989x_set_chg_enable(sc, !disable);
	} else {
		rc = sc8989x_set_chg_enable(sc, disable);;
	}

	if (rc < 0) {
		pr_err("failed to disabl:e%d\n", disable);
		return rc;
	}

	pr_info("disable:%d\n", disable);

	return 0;
}



static int usb_icl_vote_callback(struct votable *votable, void *data,
			int icl_ma, const char *client)
{
	struct sc8989x_chip *sc = data;
	int rc;

	if (icl_ma < 0){
		pr_err("icl_ma: %d < 0, ERROR!\n", icl_ma);
		return 0;
	}

	if (icl_ma > SC8989X_MAX_ICL)
		icl_ma = SC8989X_MAX_ICL;

	rc = sc8989x_set_input_current(sc, icl_ma);
	if (rc < 0) {
		pr_err("failed to set input current limit\n");
		return rc;
	}

	return 0;
}
/* longcheer nielianjie10 2023.05.17 add vote end*/
#endif
static int fv_vote_callback(struct votable *votable, void *data,
			int fv_mv, const char *client)
{
	struct sc8989x_chip *sc = data;
	int rc = 0;

	if (fv_mv < 0) {
		pr_err("fv_mv: %d < 0, ERROR!\n", fv_mv);
		return 0;
	}

	rc = sc8989x_set_vbat(sc, fv_mv);
	if (rc < 0) {
		pr_err("failed to set chargevoltage\n");
		return rc;
	}

	pr_info(" fv:%d\n", fv_mv);
	return 0;
}
#if 1
static bool is_ds_chan_valid(struct sc8989x_chip *upm,
		enum ds_ext_iio_channels chan)
{
	int rc;
	if (IS_ERR(upm->ds_ext_iio_chans[chan]))
		return false;
	if (!upm->ds_ext_iio_chans[chan]) {
		upm->ds_ext_iio_chans[chan] = iio_channel_get(upm->dev,
					ds_ext_iio_chan_name[chan]);
		if (IS_ERR(upm->ds_ext_iio_chans[chan])) {
			rc = PTR_ERR(upm->ds_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				upm->ds_ext_iio_chans[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				ds_ext_iio_chan_name[chan], rc);
			return false;
		}
	}
	return true;
}
static bool is_bms_chan_valid(struct sc8989x_chip *upm,
		enum fg_ext_iio_channels chan)
{
	int rc;
	if (IS_ERR(upm->fg_ext_iio_chans[chan]))
		return false;
	if (!upm->fg_ext_iio_chans[chan]) {
		upm->fg_ext_iio_chans[chan] = iio_channel_get(upm->dev,
					fg_ext_iio_chan_name[chan]);
		if (IS_ERR(upm->fg_ext_iio_chans[chan])) {
			rc = PTR_ERR(upm->fg_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				upm->fg_ext_iio_chans[chan] = NULL;
			pr_err("bms_chan_valid Failed to get fg_ext_iio channel %s, rc=%d\n",
				fg_ext_iio_chan_name[chan], rc);
			return false;
		}
	}
	return true;
}
static bool is_nopmi_chg_chan_valid(struct sc8989x_chip *upm,
		enum nopmi_chg_ext_iio_channels chan)
{
	int rc;
	if (IS_ERR(upm->nopmi_chg_ext_iio_chans[chan]))
		return false;
	if (!upm->nopmi_chg_ext_iio_chans[chan]) {
		upm->nopmi_chg_ext_iio_chans[chan] = iio_channel_get(upm->dev,
					nopmi_chg_ext_iio_chan_name[chan]);
		if (IS_ERR(upm->nopmi_chg_ext_iio_chans[chan])) {
			rc = PTR_ERR(upm->nopmi_chg_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				upm->nopmi_chg_ext_iio_chans[chan] = NULL;
			pr_err("nopmi_chg_chan_valid failed to get IIO channel %s, rc=%d\n",
				nopmi_chg_ext_iio_chan_name[chan], rc);
			return false;
		}
	}
	return true;
}
int sc8989x_get_iio_channel(struct sc8989x_chip *upm,
			enum sc8989x_iio_type type, int channel, int *val)
{
	struct iio_channel *iio_chan_list;
	int rc;
/*
	if(upm->shutdown_flag)
		return -ENODEV;
*/
	switch (type) {
	case DS28E16:
		if (!is_ds_chan_valid(upm, channel))
			return -ENODEV;
		iio_chan_list = upm->ds_ext_iio_chans[channel];
		break;
	case BMS:
		if (!is_bms_chan_valid(upm, channel))
			return -ENODEV;
		iio_chan_list = upm->fg_ext_iio_chans[channel];
		break;
	case NOPMI:
		if (!is_nopmi_chg_chan_valid(upm, channel))
			return -ENODEV;
		iio_chan_list = upm->nopmi_chg_ext_iio_chans[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}
	rc = iio_read_channel_processed(iio_chan_list, val);
	return rc < 0 ? rc : 0;
}
int sc8989x_set_iio_channel(struct sc8989x_chip *upm,
			enum sc8989x_iio_type type, int channel, int val)
{
	struct iio_channel *iio_chan_list;
	int rc;
/*
	if(chg->shutdown_flag)
		return -ENODEV;
*/
	switch (type) {
	case DS28E16:
		if (!is_ds_chan_valid(upm, channel)){
			pr_err("%s: iio_type is DS28E16 \n",__func__);
			return -ENODEV;
		}
		iio_chan_list = upm->ds_ext_iio_chans[channel];
		break;
	case BMS:
		if (!is_bms_chan_valid(upm, channel)){
			pr_err("%s: iio_type is BMS \n",__func__);
			return -ENODEV;
		}
			
		iio_chan_list = upm->fg_ext_iio_chans[channel];
		break;
	case NOPMI:
		if (!is_nopmi_chg_chan_valid(upm, channel)){
			pr_err("%s: iio_type is NOPMI \n",__func__);
			return -ENODEV;
		}
		iio_chan_list = upm->nopmi_chg_ext_iio_chans[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		pr_err("iio_type %d is not supported\n", type);
		return -EINVAL;
	}
	rc = iio_write_channel_raw(iio_chan_list, val);
	return rc < 0 ? rc : 0;
}
static int get_source_mode(struct tcp_notify *noti)
{
	if (noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC || noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC)
		return QTI_POWER_SUPPLY_TYPEC_SOURCE_DEFAULT;
	switch (noti->typec_state.rp_level) {
	case TYPEC_CC_VOLT_SNK_1_5:
		return QTI_POWER_SUPPLY_TYPEC_SOURCE_MEDIUM;
	case TYPEC_CC_VOLT_SNK_3_0:
		return QTI_POWER_SUPPLY_TYPEC_SOURCE_HIGH;
	case TYPEC_CC_VOLT_SNK_DFT:
		return QTI_POWER_SUPPLY_TYPEC_SOURCE_DEFAULT;
	default:
		break;
	}
	return QTI_POWER_SUPPLY_TYPEC_NONE;
}
static int sc8989x_set_cc_orientation(struct sc8989x_chip *upm, int cc_orientation)
{
	int ret = 0;

	union power_supply_propval propval;
	if(upm->usb_psy == NULL) {
		upm->usb_psy = power_supply_get_by_name("usb");
		if (upm->usb_psy == NULL) {
			pr_err("%s : fail to get psy usb\n", __func__);
			return -ENODEV;
		}
	}
	propval.intval = cc_orientation;
	ret = sc8989x_set_iio_channel(upm, NOPMI, NOPMI_CHG_TYPEC_CC_ORIENTATION, propval.intval); 
	if (ret < 0)
		pr_err("%s : set prop CC_ORIENTATION fail ret:%d\n", __func__, ret);
	return ret;
}
static int sc8989x_set_typec_mode(struct sc8989x_chip *upm, enum power_supply_typec_mode typec_mode)
{
	int ret = 0;
	union power_supply_propval propval;
	
	if(upm->usb_psy == NULL) {
		upm->usb_psy = power_supply_get_by_name("usb");
		if (upm->usb_psy == NULL) {
			pr_err("%s : fail to get psy usb\n", __func__);
			return -ENODEV;
		}
	}
	propval.intval = typec_mode;
	ret = sc8989x_set_iio_channel(upm, NOPMI, NOPMI_CHG_TYPEC_MODE, propval.intval);
	if (ret < 0)
		pr_err("%s : set prop TYPEC_MODE fail ret:%d\n", __func__, ret);
	return ret;
}

#endif
static int sc8989x_tcpc_notifier_call(struct notifier_block *pnb,
                    unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct sc8989x_chip *sc;
	int ret = 0	;
	int cc_orientation = 0;
	enum power_supply_typec_mode typec_mode = QTI_POWER_SUPPLY_TYPEC_NONE;

	sc = container_of(pnb, struct sc8989x_chip, tcp_nb);
	
	switch (event) {
	case TCP_NOTIFY_PD_STATE:
		 switch(noti->pd_state.connected) {
			case PD_CONNECT_NONE:
				sc->pd_active = 0;
				break;
			case PD_CONNECT_HARD_RESET:
				break;
			case PD_CONNECT_PE_READY_SNK:
			case PD_CONNECT_PE_READY_SNK_PD30:
				if(sc->chg_type == POWER_SUPPLY_TYPE_USB)
				{
					sc8989x_set_vindpm(sc, 4600);
					sc8989x_set_iindpm(sc,1200);
					sc8989x_set_ichg(sc,1200);
				}
			case PD_CONNECT_PE_READY_SNK_APDO:
				sc->pd_active = 1;
				break;
		 }
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
		    (noti->typec_state.new_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC)) {
			pr_err("%s:USB Plug in, cc_or_pol = %d, state = %d\n",__func__,
					noti->typec_state.polarity, noti->typec_state.new_state);

			if(sc->chg_type == POWER_SUPPLY_USB_TYPE_UNKNOWN){
				pr_err("%s: chg_type = %d vbus type is none, do force dpdm now.\n",__func__, sc->chg_type);
				sc8989x_init_device(sc);
				sc8989x_request_dpdm(sc, true);
			}
			typec_mode = get_source_mode(noti);
			cc_orientation = noti->typec_state.polarity;
			sc8989x_set_cc_orientation(sc, cc_orientation);
			pr_debug("%s: cc_or_val = %d\n",__func__, cc_orientation);
		} else if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			typec_mode = QTI_POWER_SUPPLY_TYPEC_SINK;
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_NORP_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_SRC)
			&& noti->typec_state.new_state == TYPEC_UNATTACHED) {
			typec_mode = QTI_POWER_SUPPLY_TYPEC_NONE;
			pr_info("USB Plug out\n");
			sc8989x_request_dpdm(sc, false);
		} else if (noti->typec_state.old_state == TYPEC_ATTACHED_SRC &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SNK) {
			pr_info("Source_to_Sink \n");
			typec_mode = QTI_POWER_SUPPLY_TYPEC_SINK;
		}  else if (noti->typec_state.old_state == TYPEC_ATTACHED_SNK &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			typec_mode = get_source_mode(noti);
			pr_info("Sink_to_Source \n");
		}
		if(typec_mode >= QTI_POWER_SUPPLY_TYPEC_NONE && typec_mode <= QTI_POWER_SUPPLY_TYPEC_NON_COMPLIANT)
			sc8989x_set_typec_mode(sc, typec_mode);
		break;
	case TCP_NOTIFY_SOURCE_VBUS:
		pr_info("%s source vbus %dmV %dmA type(0x%02X)\n",
				    __func__, noti->vbus_state.mv,
				    noti->vbus_state.ma, noti->vbus_state.type);
#if 0
		if (noti->vbus_state.mv) {
			//en otg
            ret = sc8989x_set_otg_enable(sc, true);
            ret |= sc8989x_set_chg_enable(sc, false);
		} else {
			//dis otg
            ret = sc8989x_set_otg_enable(sc, false);
            ret |= sc8989x_set_chg_enable(sc, true);
		}
#endif
		if ((noti->vbus_state.mv == TCPC_VBUS_SOURCE_0V) && (vbus_on)) {
			/* disable OTG power output */
			pr_err("chj otg plug out and power out !\n");
			vbus_on = false;
			ret = sc8989x_set_otg_enable(sc, false);
			ret |= sc8989x_set_chg_enable(sc, true);
			//sc8989x_set_otg(sc, false);
			sc8989x_request_dpdm(sc, false);
		} else if ((noti->vbus_state.mv == TCPC_VBUS_SOURCE_5V) && (!vbus_on)) {
			/* enable OTG power output */
			pr_err("chj otg plug in and power on !\n");
			vbus_on = true;
                    	sc8989x_request_dpdm(sc, false);
			mdelay(500);
			ret = sc8989x_set_otg_enable(sc, true);
			pr_err("otg power on delay 500ms!\n");
			ret |= sc8989x_set_chg_enable(sc, false);
                    	sc8989x_dump_register(sc);
			//sc8989x_set_otg(sc, true);
			//sc8989xset_otg_current(sc, sc->cfg.charge_current_1500);
		}

        break;
	}
	return ret;
}

static void sc8989x_register_tcpc_notify_dwork_handler(struct work_struct *work)
{
	struct sc8989x_chip *sc = container_of(work, struct sc8989x_chip,
								tcpc_dwork.work);
	int ret;

    if (!sc->tcpc) {
        sc->tcpc = tcpc_dev_get_by_name("type_c_port0");
        if (!sc->tcpc) {
            pr_err("get tcpc dev fail\n");
			schedule_delayed_work(&sc->tcpc_dwork, msecs_to_jiffies(2*1000));
            return;
        }
    }
    if(sc->tcpc){
        sc->tcp_nb.notifier_call = sc8989x_tcpc_notifier_call;
        ret = register_tcp_dev_notifier(sc->tcpc, &sc->tcp_nb,
                        TCP_NOTIFY_TYPE_ALL);
        if (ret < 0) {
            pr_err("register tcpc notifier fail\n");
        }
    }

    if(sc->tcpc->typec_state == ATTACHEDSRC)
    {
        pr_err("chj otg plug in and power on !\n");
        vbus_on = true;
        sc8989x_request_dpdm(sc, false);
        ret = sc8989x_set_otg_enable(sc, true);
        ret |= sc8989x_set_chg_enable(sc, false);

    }
}

int sc8989x_chg_get_iio_channel(struct sc8989x_chip *upm,
			enum sc8989x_iio_type type, int channel, int *val)
{
	struct iio_channel *iio_chan_list = NULL;
	int rc = 0;

	switch (type) {
		default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}

	rc = iio_read_channel_processed(iio_chan_list, val);

	return rc < 0 ? rc : 0;
}
EXPORT_SYMBOL(sc8989x_chg_get_iio_channel);

static int sc8989x_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct sc8989x_chip *sc = iio_priv(indio_dev);
	int rc = 0;
	int t_val = 0;
	*val1 = 0;

	switch (chan->channel) {
	case PSY_IIO_CHARGE_TYPE:
		rc = sc8989x_get_charge_stat(sc,&t_val);
		if(rc < 0)
			break;
		switch (t_val) {
			case CHG_STAT_NOT_CHARGE:
				*val1 = POWER_SUPPLY_CHARGE_TYPE_NONE;
				break;
			case CHG_STAT_PRE_CHARGE:
				*val1 = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
				break;
			case CHG_STAT_FAST_CHARGE:
				*val1 = POWER_SUPPLY_CHARGE_TYPE_FAST;
				break;
			case CHG_STAT_CHARGE_DONE:
				*val1 = POWER_SUPPLY_CHARGE_TYPE_NONE;
				break;
			default:
				*val1 = POWER_SUPPLY_CHARGE_TYPE_NONE;
				break;
		}
		break;
	case PSY_IIO_CHARGE_AFC:
		*val1 = sc->charge_afc;
		break;
	default:
		pr_err("Unsupported upm6918 IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}
	pr_info("read IIO channel %d, rc = %d, val1 = %d, t_val = %d\n", chan->channel, rc, val1, t_val);
	if (rc < 0) {
		pr_err("Couldn't read IIO channel %d, rc = %d\n",
			chan->channel, rc);
		return rc;
	}
	return IIO_VAL_INT;
}

static int sc8989x_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	int rc = 0;
        struct sc8989x_chip *sc = iio_priv(indio_dev);

	switch (chan->channel) {
	case PSY_IIO_CHARGE_TYPE:
		sc->chg_type = val1;
		break;
	case PSY_IIO_CHARGE_ENABLED:
		sc8989x_set_chg_enable(sc, true);
                pr_err("Write IIO channel %d, val = %d\n", chan->channel, val1);
		break;
	case PSY_IIO_CHARGE_DISABLE:
		sc8989x_set_chg_enable(sc, false);
		pr_err("Write IIO channel %d, val = %d\n", chan->channel, val1);
		break;
	case PSY_IIO_CHARGE_AFC_DISABLE:
		sc->is_disble_afc = val1;

		if (sc->is_disble_afc_backup != sc->is_disble_afc) {
			if (sc->charge_afc == 1) {
				pr_err("disable afc\n");
				sc8989x_disable_afc(sc);
			} else if ((sc8989x_get_pdo_active(sc) == 1)) {
				pr_err("disable pdo\n");
				sc8989x_disable_pdo(sc);
			}
			sc->is_disble_afc_backup = sc->is_disble_afc;
		} else {
			pr_info("No changes, no need adjust vbus\n");
		}
		pr_err("%s: is_disble_afc=%d\n", __func__, sc->is_disble_afc);
		break;

	default:
		pr_err("Unsupported sc8989x IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0){
		pr_err("Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, rc);
		return rc;
	}

	return IIO_VAL_INT;
}

static int sc8989x_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct sc8989x_chip *sc = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = sc->iio_chan;
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(sc8989x_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0]){
			return i;
		}
	return -EINVAL;
}
				
static const struct iio_info sc8989x_iio_info = {
	.read_raw	= sc8989x_iio_read_raw,
	.write_raw	= sc8989x_iio_write_raw,
	.of_xlate	= sc8989x_iio_of_xlate,
};

static int sc8989x_init_iio_psy(struct sc8989x_chip *sc)
{
	struct iio_dev *indio_dev = sc->indio_dev;
	struct iio_chan_spec *chan = NULL;
	int num_iio_channels = ARRAY_SIZE(sc8989x_iio_psy_channels);
	int rc = 0, i = 0;

	pr_info("sc8989x_init_iio_psy start\n");
	sc->iio_chan = devm_kcalloc(sc->dev, num_iio_channels,
						sizeof(*sc->iio_chan), GFP_KERNEL);
	if (!sc->iio_chan) {
		pr_err("upm->iio_chan is null!\n");
		return -ENOMEM;
	}

	sc->int_iio_chans = devm_kcalloc(sc->dev,
				num_iio_channels,
				sizeof(*sc->int_iio_chans),
				GFP_KERNEL);
	if (!sc->int_iio_chans) {
		pr_err("upm->int_iio_chans is null!\n");
		return -ENOMEM;
	}

	indio_dev->info = &sc8989x_iio_info;
	indio_dev->dev.parent = sc->dev;
	indio_dev->dev.of_node = sc->dev->of_node;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = sc->iio_chan;
	indio_dev->num_channels = num_iio_channels;
	indio_dev->name = "main_chg";

	for (i = 0; i < num_iio_channels; i++) {
		sc->int_iio_chans[i].indio_dev = indio_dev;
		chan = &sc->iio_chan[i];
		sc->int_iio_chans[i].channel = chan;
		chan->address = i;
		chan->channel = sc8989x_iio_psy_channels[i].channel_num;
		chan->type = sc8989x_iio_psy_channels[i].type;
		chan->datasheet_name =
			sc8989x_iio_psy_channels[i].datasheet_name;
		chan->extend_name =
			sc8989x_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate =
			sc8989x_iio_psy_channels[i].info_mask;
	}

	rc = devm_iio_device_register(sc->dev, indio_dev);
	if (rc)
		pr_err("Failed to register sc8989x IIO device, rc=%d\n", rc);

	pr_info("sc8989x IIO device, rc=%d\n", rc);
	return rc;
}

static int sc8989x_ext_init_iio_psy(struct sc8989x_chip *sc)
{
	if (!sc)
	{
		pr_err("sc8989x_ext_init_iio_psy, upm is NULL!\n");
		return -ENOMEM;
	}
	sc->ds_ext_iio_chans = devm_kcalloc(sc->dev,
				ARRAY_SIZE(ds_ext_iio_chan_name),
				sizeof(*sc->ds_ext_iio_chans),
				GFP_KERNEL);
	if (!sc->ds_ext_iio_chans) {
		pr_err("sc->ds_ext_iio_chans is NULL!\n");
		return -ENOMEM;
	}

	sc->fg_ext_iio_chans = devm_kcalloc(sc->dev,
		ARRAY_SIZE(fg_ext_iio_chan_name), sizeof(*sc->fg_ext_iio_chans), GFP_KERNEL);
	if (!sc->fg_ext_iio_chans) {
		pr_err("sc->fg_ext_iio_chans is NULL!\n");
		return -ENOMEM;
	}

	sc->nopmi_chg_ext_iio_chans = devm_kcalloc(sc->dev,
		ARRAY_SIZE(nopmi_chg_ext_iio_chan_name), sizeof(*sc->nopmi_chg_ext_iio_chans), GFP_KERNEL);
	if (!sc->nopmi_chg_ext_iio_chans) {
		pr_err("upm->nopmi_chg_ext_iio_chans is NULL!\n");
		return -ENOMEM;
	}
	return 0;
}
static int fcc_vote_callback(struct votable *votable, void *data,
			int fcc_ua, const char *client)
{
	struct sc8989x_chip *sc	= data;
	int rc = 0;

	if (fcc_ua < 0) {
		pr_err("fcc_ua: %d < 0, ERROR!\n", fcc_ua);
		return 0;
	}

	if (fcc_ua > SC8989X_MAX_FCC)
		fcc_ua = SC8989X_MAX_FCC;

	rc = sc8989x_set_ichg(sc, fcc_ua);
	if (rc < 0) {
		pr_err("failed to set charge current\n");
		return rc;
	}

	pr_info(" fcc:%d\n", fcc_ua);

	return 0;
}

static int chg_dis_vote_callback(struct votable *votable, void *data,
			int disable, const char *client)
{
	struct sc8989x_chip *sc = data;
	int rc = 0;

	if (disable) {
		rc = sc8989x_set_chg_enable(sc, !disable);
	} else {
		rc = sc8989x_set_chg_enable(sc, !disable);;
	}

	if (rc < 0) {
		pr_err("failed to disable:%d\n", disable);
		return rc;
	}

	pr_err("disable:%d\n", disable);

	return 0;
}

static int usb_icl_vote_callback(struct votable *votable, void *data,
			int icl_ma, const char *client)
{
	struct sc8989x_chip *sc = data;
	int rc;
	pr_err("%s:icl_ma=%d\n",__func__,icl_ma);
	if (icl_ma < 0){
		pr_err("icl_ma: %d < 0, ERROR!\n", icl_ma);
		return 0;
	}

	if (icl_ma > SC8989X_MAX_ICL)
		icl_ma = SC8989X_MAX_ICL;

	rc = sc8989x_set_iindpm(sc, icl_ma);
	if (rc < 0) {
		pr_err("failed to set input current limit\n");
		return rc;
	}

	return 0;
}
static int sc8989x_charger_probe(struct i2c_client *client,
                                const struct i2c_device_id *id) {
    struct sc8989x_chip *sc;
    int ret = 0;
    int i;
    int vbus_stat;
	struct iio_dev *indio_dev = NULL;

	if (client->dev.of_node) {
		indio_dev = devm_iio_device_alloc(&client->dev, sizeof(struct sc8989x_chip));
		if (!indio_dev) {
			pr_err("Failed to allocate memory\n");
			return -ENOMEM;
		}
	}	

	sc = iio_priv(indio_dev);
	sc->indio_dev = indio_dev;
#if 0	
    sc = devm_kzalloc(&client->dev, sizeof(struct sc8989x_chip), GFP_KERNEL);
    if (!sc)
        return -ENOMEM;
#endif

    sc->dev = &client->dev;
    sc->client = client;
    sc->pd_active = 0;
    sc->is_disble_afc = false;
    sc->is_disble_afc_backup = false;
    dev_err(sc->dev, "sc8989x_charger_probe start\n");

    sc->regmap = devm_regmap_init_i2c(client, &sc8989x_regmap_config);
    if (IS_ERR(sc->regmap)) {
        dev_err(sc->dev, "Failed to initialize regmap\n");
        return -EINVAL;
    }

    for (i = 0; i < ARRAY_SIZE(sc8989x_reg_fields); i++) {
        const struct reg_field *reg_fields = sc8989x_reg_fields;
        sc->rmap_fields[i] =
            devm_regmap_field_alloc(sc->dev, sc->regmap, reg_fields[i]);
        if (IS_ERR(sc->rmap_fields[i])) {
            dev_err(sc->dev, "cannot allocate regmap field\n");
            return PTR_ERR(sc->rmap_fields[i]);
        }
    }

    mutex_init(&sc->regulator_lock);
    i2c_set_clientdata(client, sc);
    sc->entry_shipmode = false;

    if (!sc8989x_detect_device(sc)) {
        ret = -ENODEV;
        pr_err("No found sc8989x_detect_device !\n");
        goto err_nodev;
    }

    sc8989x_create_device_node(&(client->dev));
    sc->hvdcp_qc20_ws = wakeup_source_register(sc->dev, "sc89890h_hvdcp_qc20_ws");
    INIT_DELAYED_WORK(&sc->hvdcp_qc20_dwork, sc89890h_hvdcp_qc20_dwork);
    INIT_DELAYED_WORK(&sc->hvdcp_dwork, sc89890h_hvdcp_dwork);
    INIT_DELAYED_WORK(&sc->force_detect_dwork,
                        sc8989x_force_detection_dwork_handler);
    INIT_DELAYED_WORK(&sc->disable_afc_dwork, sc8989x_disable_afc_dwork);
    INIT_DELAYED_WORK(&sc->disable_pdo_dwork, sc8989x_disable_pdo_dwork);
#ifdef CONFIG_MTK_CHARGER_V4P19
    INIT_DELAYED_WORK(&sc->psy_dwork,
                        sc8989x_inform_psy_dwork_handler);
#endif /*CONFIG_MTK_CHARGER_V4P19*/
    if (!sc->tcpc) {
        sc->tcpc = tcpc_dev_get_by_name("type_c_port0");
        if (!sc->tcpc) {
            pr_err("get tcpc dev fail\n");
            return -EPROBE_DEFER;
        }
    } else {
        if(!sc->tcpc->tcpc_pd_probe) {
            pr_err("wait tcpc dev probe\n");
            return -EPROBE_DEFER;
        }
    }

	INIT_DELAYED_WORK(&sc->tcpc_dwork, 
                        sc8989x_register_tcpc_notify_dwork_handler);

    sc->cfg = &sc8989x_default_cfg;
    ret = sc8989x_parse_dt(sc);
    if (ret < 0) {
        dev_err(sc->dev, "parse dt fail(%d)\n", ret);
        goto err_parse_dt;
    }

    ret = sc8989x_init_device(sc);
    if (ret < 0) {
        dev_err(sc->dev, "init device fail(%d)\n", ret);
        goto err_init_dev;
    }

	ret = sc8989x_ext_init_iio_psy(sc);
	if (ret < 0) {
		pr_err("Failed to initialize sc8989x_ext_init_iio_psy IIO PSY, rc=%d\n", ret);
	}
#if 0
    /* longcheer nielianjie10 2023.05.17 add vote start*/
	sc->fcc_votable = create_votable("FCC", VOTE_MIN,
					fcc_vote_callback, sc);
	if (IS_ERR(sc->fcc_votable)) {
		pr_err("fcc_votable is ERROR,goto destroy!\n");
		ret = PTR_ERR(sc->fcc_votable);
		sc->fcc_votable = NULL;
		goto destroy_votable;
	}
	pr_info("fcc_votable create successful !\n");

	sc->chg_dis_votable = create_votable("CHG_DISABLE", VOTE_SET_ANY,
					chg_dis_vote_callback, sc);
	if (IS_ERR(sc->chg_dis_votable)) {
		pr_err("chg_dis_votable is ERROR,goto destroy!\n");
		ret = PTR_ERR(sc->chg_dis_votable);
		sc->chg_dis_votable = NULL;
		goto destroy_votable;
	}
	pr_info("chg_dis_votable create successful !\n");

	sc->fv_votable = create_votable("FV", VOTE_MIN,
					fv_vote_callback, sc);
	if (IS_ERR(sc->fv_votable)) {
		pr_err("fv_votable is ERROR,goto destroy!\n");
		ret = PTR_ERR(sc->fv_votable);
		sc->fv_votable = NULL;
		goto destroy_votable;
	}
	pr_info("fv_votable create successful !\n");

	sc->usb_icl_votable = create_votable("USB_ICL", VOTE_MIN,
					usb_icl_vote_callback,
					sc);
	if (IS_ERR(sc->usb_icl_votable)) {
		pr_err("usb_icl_votable is ERROR,goto destroy!\n");
		ret = PTR_ERR(sc->usb_icl_votable);
		sc->usb_icl_votable = NULL;
		goto destroy_votable;
	}
	pr_info("usb_icl_votable create successful !\n");

	vote(sc->fcc_votable, MAIN_SET_VOTER, true, 500);
	vote(sc->fcc_votable, PROFILE_CHG_VOTER, true, CHG_FCC_CURR_MAX);
	vote(sc->usb_icl_votable, PROFILE_CHG_VOTER, true, CHG_ICL_CURR_MAX);
	vote(sc->chg_dis_votable, BMS_FC_VOTER, false, 0);
	/* longcheer nielianjie10 2023.05.17 add vote end*/
#endif

	sc->chg_dis_votable = create_votable("CHG_DISABLE", VOTE_SET_ANY,
					chg_dis_vote_callback, sc);
	if (IS_ERR(sc->chg_dis_votable)) {
		pr_err("chg_dis_votable is ERROR,goto destroy!\n");
		ret = PTR_ERR(sc->chg_dis_votable);
		sc->chg_dis_votable = NULL;
		goto destroy_votable;
	}
	pr_info("chg_dis_votable create successful !\n");

	sc->fcc_votable = create_votable("FCC", VOTE_MIN,
					fcc_vote_callback, sc);
	if (IS_ERR(sc->fcc_votable)) {
		pr_err("fcc_votable is ERROR,goto destroy!\n");
		ret = PTR_ERR(sc->fcc_votable);
		sc->fcc_votable = NULL;
		goto destroy_votable;
	}
	pr_info("fcc_votable create successful !\n");
	sc->fv_votable = create_votable("FV", VOTE_MIN,
					fv_vote_callback, sc);
	if (IS_ERR(sc->fv_votable)) {
		pr_err("fv_votable is ERROR,goto destroy!\n");
		ret = PTR_ERR(sc->fv_votable);
		sc->fv_votable = NULL;
		goto destroy_votable;
	}
	pr_info("t_c_c fv_votable create successful !\n");
	sc->usb_icl_votable = create_votable("USB_ICL", VOTE_MIN,
					usb_icl_vote_callback,
					sc);
	if (IS_ERR(sc->usb_icl_votable)) {
		pr_err("usb_icl_votable is ERROR,goto destroy!\n");
		ret = PTR_ERR(sc->usb_icl_votable);
		sc->usb_icl_votable = NULL;
		goto destroy_votable;
	}
	pr_info("usb_icl_votable create successful !\n");
	vote(sc->usb_icl_votable, PROFILE_CHG_VOTER, true, CHG_ICL_CURR_MAX);
    ret = sc8989x_psy_register(sc);
    if (ret) {
        pr_err("psy register fail(%d), goto err_psy\n", ret);
        goto err_psy;
    }
    pr_err("sc8989x_psy_register successful !\n");

    ret = sc8989x_register_interrupt(sc);
    if (ret < 0) {
        dev_err(sc->dev, "%s register irq fail(%d)\n", __func__, ret);
        goto err_register_irq;
    }

	
	ret = sc8989x_init_iio_psy(sc);
		if (ret < 0) {
			pr_err("Failed to initialize upm6918 IIO PSY, ret=%d\n", ret);
			goto err_psy;
		}
#ifdef CONFIG_MTK_CLASS
    /* Register charger device */
    sc->chg_dev = charger_device_register(sc->cfg->chg_name,
                                        sc->dev, sc, &sc8989x_chg_ops,
                                        &sc->chg_props);
    if (IS_ERR_OR_NULL(sc->chg_dev)) {
        ret = PTR_ERR(sc->chg_dev);
        dev_notice(sc->dev, "%s register chg dev fail(%d)\n", __func__, ret);
        goto err_register_chg_dev;
    }
#endif                          /*CONFIG_MTK_CLASS */
    device_init_wakeup(sc->dev, 1);
    
    determine_initial_status(sc);
    sc8989x_dump_register(sc);

	schedule_delayed_work(&sc->tcpc_dwork, msecs_to_jiffies(2*1000));

    ret = sc8989x_field_read(sc, VBUS_STAT, &vbus_stat);
    if (ret){
        return ret;
    }

    if (vbus_stat == VBUS_STAT_OTG){
        //pr_err("%s:OTG is always online, retry for dpdm !\n", __func__);
        sc8989x_request_dpdm(sc, true);
    }

    dev_err(sc->dev, "sc8989x probe successfully\n!");

    return 0;

destroy_votable:
	pr_err("destory votable !\n");
	destroy_votable(sc->chg_dis_votable);
	destroy_votable(sc->fcc_votable);
	destroy_votable(sc->fv_votable);
	destroy_votable(sc->usb_icl_votable);


err_register_irq:
#ifdef CONFIG_MTK_CLASS
err_register_chg_dev:
#endif                          /*CONFIG_MTK_CLASS */
err_psy:
err_init_dev:
err_parse_dt:
err_nodev:
    dev_err(sc->dev, "sc8989x probe failed!\n");
    mutex_destroy(&sc->regulator_lock);
    devm_kfree(&client->dev, sc);
    return -ENODEV;
}

static int sc8989x_charger_remove(struct i2c_client *client) {
    struct sc8989x_chip *sc = i2c_get_clientdata(client);

    if (sc) {
        dev_err(sc->dev, "%s\n", __func__);
#ifdef CONFIG_MTK_CLASS
        charger_device_unregister(sc->chg_dev);
#endif                          /*CONFIG_MTK_CLASS */
        mutex_destroy(&sc->regulator_lock);
        power_supply_put(sc->psy);
    }
    return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sc8989x_suspend(struct device *dev) {
    struct sc8989x_chip *sc = dev_get_drvdata(dev);

    dev_err(dev, "%s\n", __func__);
    if (device_may_wakeup(dev))
        enable_irq_wake(sc->irq);
    disable_irq(sc->irq);

    return 0;
}

static int sc8989x_resume(struct device *dev) {
    struct sc8989x_chip *sc = dev_get_drvdata(dev);

    dev_err(dev, "%s\n", __func__);
    enable_irq(sc->irq);
    if (device_may_wakeup(dev))
        disable_irq_wake(sc->irq);

    return 0;
}

static void sc8989x_charger_shutdown(struct i2c_client *client)
{
	struct sc8989x_chip *sc;
	int ret;

	sc = i2c_get_clientdata(client);
	if(!sc) {
		pr_err("i2c_get_clientdata fail\n");
		return;
	}
	pr_err("entry_shipmode:%d\n",sc->entry_shipmode);
	if(sc->entry_shipmode) {
                ret = sc8989x_set_hiz(sc, true);
                if (ret) {
                        pr_err("enable hiz failed, sc8989x error\n");
                        return;
                }
		ret = sc8989x_set_batfet(sc, true);
		if (ret) {
			pr_err("enable ship mode failed, sc8989x error\n");
			return;
		} else {
			ret = upm6720_set_adc(false);
			if (!ret) {
				oz8806_set_shutdown_mode();
				pr_err("enable ship mode succeeded\n");
			} else {
				pr_err("enable ship mode failed, upm6720 error\n");
				return;
			}
		}
	}

}

static const struct dev_pm_ops sc8989x_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(sc8989x_suspend, sc8989x_resume)
};
#endif                          /* CONFIG_PM_SLEEP */

static struct i2c_driver sc8989x_charger_driver = {
    .driver = {
            .name = "sc8989x",
            .owner = THIS_MODULE,
            .of_match_table = of_match_ptr(sc8989x_of_device_id),
#ifdef CONFIG_PM_SLEEP
            .pm = &sc8989x_pm_ops,
#endif
            },
    .probe = sc8989x_charger_probe,
    .remove = sc8989x_charger_remove,
    .shutdown = sc8989x_charger_shutdown,
};

module_i2c_driver(sc8989x_charger_driver);

MODULE_DESCRIPTION("SC SC8989X Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("South Chip <Aiden-yu@southchip.com>");
