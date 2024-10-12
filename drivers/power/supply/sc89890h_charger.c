/*
 * Driver for the SC SC89890H charger.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/acpi.h>
#include <linux/alarmtimer.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power/charger-manager.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/types.h>
#include <linux/usb/phy.h>
#include <uapi/linux/usb/charger.h>
#include "battery_id_via_adc.h"

#define SC89890H_REG_00                0x00
#define SC89890H_REG_01                0x01
#define SC89890H_REG_02                0x02
#define SC89890H_REG_03                0x03
#define SC89890H_REG_04                0x04
#define SC89890H_REG_05                0x05
#define SC89890H_REG_06                0x06
#define SC89890H_REG_07                0x07
#define SC89890H_REG_08                0x08
#define SC89890H_REG_09                0x09
#define SC89890H_REG_0A                0x0a
#define SC89890H_REG_0B                0x0b
#define SC89890H_REG_0C                0x0c
#define SC89890H_REG_0D                0x0d
#define SC89890H_REG_0E                0x0e
#define SC89890H_REG_0F                0x0f
#define SC89890H_REG_10                0x10
#define SC89890H_REG_11                0x11
#define SC89890H_REG_12                0x12
#define SC89890H_REG_13                0x13
#define SC89890H_REG_14                0x14
#define SC89890H_REG_NUM               21


/* Register 0x00 */
#define REG00_ENHIZ_MASK            BIT(7)
#define REG00_ENHIZ_SHIFT           7
#define REG00_EN_LIM_MASK           BIT(6)
#define REG00_EN_LIM_SHIFT          6
#define REG00_IINLIM_MASK           GENMASK(5, 0)
#define REG00_IINLIM_SHIFT          0

/* Register 0x01 */
#define REG01_DP_DRIVE_MASK         0xe0
#define REG01_DP_DRIVE_SHIFT        5
#define REG01_DM_DRIVE_MASK         0x1c
#define REG01_DM_DRIVE_SHIFT        2
#define REG01_VINDPM_MASK           BIT(0) //defalut 600mV
#define REG01_VINDPM_SHIFT          0

/* Register 0x03 */
#define REG03_WDT_MASK              BIT(6)
#define REG03_WDT_SHIFT             6
#define REG03_OTG_CONFIG_MASK       BIT(5)
#define REG03_OTG_CONFIG_SHIFT      5
#define REG03_ENCHG_MASK            BIT(4) // charger enable
#define REG03_ENCHG_SHIFT           4
#define REG03_SYS_MIN_MASK          0x0e // default 3.5v
#define REG03_SYS_MIN_SHIFT         1
#define REG03_MIN_BAT_SEL_MASK      BIT(0)
#define REG03_MIN_BAT_SEL_SHIFT     0
#define REG03_CHG_DISABLE           0
#define REG03_CHG_ENABLE            1

/* Register 0x04 */
#define REG04_ICHG_MASK             GENMASK(6, 0)
#define REG04_ICHG_SHIFT            0

/* Register 0x05 */
#define REG05_IPRECHG_MASK          GENMASK(7, 4)
#define REG05_IPRECHG_SHIFT         4
#define REG05_ITERM_MASK            GENMASK(3, 0)
#define REG05_ITERM_SHIFT           0

/* Register 0x06 */
#define REG06_VREG_MASK             GENMASK(7, 2)
#define REG06_VREG_SHIFT            2
#define REG06_VRECHG_MASK           BIT(0)
#define REG06_VRECHG_SHIFT          0
/* Register 0x07 */
#define REG07_EN_TERM_MASK          BIT(7)
#define REG07_EN_TERM_SHIFT         7
#define REG07_EN_TIMER_MASK         BIT(3)
#define REG07_EN_TIMER_SHIFT        3
#define REG07_WDT_TIMER_MASK        GENMASK(5, 4)
#define REG07_WDT_TIMER_SHIFT       4
#define REG05_JEITA_ISET_L_MASK     BIT(0) // 0-10degC
#define REG05_JEITA_ISET_L_SHIFT    0

/* Register 0x08 */
#define REG05_TREG_MASK             0x03 //default 12degC
#define REG05_TREG_SHIFT            0

/* Register 0x09 */
#define REG09_TMR2X_EN_MASK         BIT(6)
#define REG09_TMR2X_EN_SHIFT        6
#define REG09_BATFET_DIS_MASK       BIT(5)
#define REG09_BATFET_DIS_SHIFT      5
#define REG09_JEITA_VSET_H_MASK     BIT(4)
#define REG09_JEITA_VSET_H_SHIFT    4
#define REG07_BATFET_DLY_MASK       BIT(3) //default value is different form SGM41511X
#define REG07_BATFET_DLY_SHIFT      3
#define REG07_BATFET_RST_EN_MASK    BIT(2)
#define REG07_BATFET_RST_EN_SHIFT   2
#define REG07_PUMPX_UP_MASK         BIT(1)
#define REG07_PUMPX_UP_SHIFT        1
#define REG07_PUMPX_DN_MASK         BIT(0)
#define REG07_PUMPX_DN_SHIFT        0

/* Register 0x0A */
#define REG06_BOOSTV_MASK           0xf0
#define REG06_BOOSTV_SHIFT          4
#define REG0A_ENPFM_MASK            BIT(3)
#define REG0A_ENPFM_SHIFT           3
#define REG0A_BOOST_LIM_MASK        0x07
#define REG0A_BOOST_LIM_SHIFT       0
#define REG0A_BOOSTV_CURRENT_MAX    5400
#define REG0A_BOOSTV_CURRENT_OFFSET 3900
#define REG0A_BOOSTV_STEP           100

/* Register 0x0B */
#define REG0B_VBUS_STAT_MASK        0xe0 // cold detect HVDCP 100
#define REG0B_VBUS_STAT_SHIFT       5
#define REG0B_CHRG_STAT_MASK        GENMASK(4, 3)
#define REG0B_CHRG_STAT_SHIFT       3
#define REG0B_PG_STAT_MASK          BIT(2)
#define REG0B_PG_STAT_SHIFT         2
#define REG0B_VSYS_STAT_MASK        BIT(0)
#define REG0B_VSYS_STAT_SHIFT       0

/* Register 0x0C */
#define REG0C_WDT_FAULT_MASK        BIT(7)
#define REG0C_WDT_FAULT_SHIFT       7
#define REG0C_BOOST_FAULT_MASK      BIT(6)
#define REG0C_BOOST_FAULT_SHIFT     6
#define REG0C_CHRG_FAULT_MASK       0x30
#define REG0C_CHRG_FAULT_SHIFT      4
#define REG0C_BAT_FAULT_MASK        BIT(3)
#define REG0C_BAT_FAULT_SHIFT       3
#define REG0C_NTC_FAULT_MASK        0x07
#define REG0C_NTC_FAULT_SHIFT       0

/* Register 0x0D */
#define REG0D_FORCE_VINDPM_MASK     BIT(7)
#define REG0D_FORCE_VINDPM_SHIFT    7
#define REG0D_VINDPM_MASK           GENMASK(6, 0)
#define REG0D_VINDPM_SHIFT          0
#define EN_FORCE_VINDPM             1


/* Register 0x11 */
#define REG11_VBUS_GD_MASK          BIT(7)
#define REG11_VBUS_GD_SHIFT         7

/* Register 0x13 */
#define REG13_VINDPM_SATA_MASK      BIT(7)
#define REG13_VINDPM_SATA_SHIFT     7
#define REG13_IINDPM_SATA_MASK      BIT(6)
#define REG13_IINDPM_SATA_SHIFT     6

/* Register 0x14 */
#define REG14_REG_RST_MASK          BIT(7)
#define REG14_REG_RST_SHIFT         7
#define REG14_REG_ICO_STAT_MASK     BIT(6)
#define REG14_REG_ICO_STAT_SHIFT    6
#define REG14_PN_ID_MASK            0x38
#define REG14_PN_ID_SHIFT           3
#define REG14_PN_ID                 4

#define WDT_RESET                1
#define REG_RESET                1
#define OTG_DISABLE              0
#define OTG_ENABLE               1
#define CHG_ENABLE               1
#define CHG_DISABLE              0
/* charge status flags  */
#define HIZ_DISABLE            0
#define HIZ_ENABLE             1

/* WDT TIMER SET  */
#define SC89890H_WDT_TIMER_DISABLE     0
#define SC89890H_WDT_TIMER_40S         1
#define SC89890H_WDT_TIMER_80S         2
#define SC89890H_WDT_TIMER_160S        3

/* termination current  */
#define SC89890H_TERMCHRG_I_MAX_mA        930
#define SC89890H_TERMCHRG_I_MIN_mA        30
#define SC89890H_TERMCHRG_I_OFFSET_mA     30
#define SC89890H_TERMCHRG_I_STEP_mA       60

/* charge current  */
#define SC89890H_ICHRG_CURRENT_STEP       60
#define SC89890H_ICHRG_I_MIN              0
#define SC89890H_ICHRG_I_MAX              5040
#define SC89890H_ICHRG_I_DEF              2040

/* charge voltage  */
#define SC89890H_VBATT_MAX_mv        4848
#define SC89890H_VBATT_MIN_mv        3840
#define SC89890H_VBATT_STEP_mV       16
#define SC89890H_VBATT_OFFSET_mV     3840

/* iindpm current  */
#define SC89890H_IINDPM_I_MIN    100
#define SC89890H_IINDPM_I_MAX    3250
#define SC89890H_IINDPM_STEP     50
#define SC89890H_IINDPM_OFFSET   100

#define SC89890H_CHARGE_DONE     0x3

#define SC89890H_BATTERY_NAME    "sc27xx-fgu"

#define BIT_DP_DM_BC_ENB         BIT(0)

#define SC89890H_WDT_VALID_MS    50

#define SC89890H_OTG_ALARM_TIMER_MS        15000
#define SC89890H_OTG_VALID_MS              500
#define SC89890H_OTG_RETRY_TIMES           10

#define SC89890H_DISABLE_PIN_MASK_2730     BIT(0)
#define SC89890H_DISABLE_PIN_MASK_2721     BIT(15)
#define SC89890H_DISABLE_PIN_MASK_2720     BIT(0)

#define SC89890H_FAST_CHG_VOL_MAX          10500000
#define SC89890H_NORMAL_CHG_VOL_MAX        6500000

#define SC89890H_WAKE_UP_MS                2000
#define SC89890H_CURRENT_WORK_MS           msecs_to_jiffies(100)

struct sc89890h_charger_sysfs {
    char *name;
    struct attribute_group attr_g;
    struct device_attribute attr_sc89890h_dump_reg;
    struct device_attribute attr_sc89890h_lookup_reg;
    struct device_attribute attr_sc89890h_sel_reg_id;
    struct device_attribute attr_sc89890h_reg_val;
    struct attribute *attrs[5];
    struct sc89890h_charger_info *info;
};

struct sc89890h_charger_info {
    struct i2c_client *client;
    struct device *dev;
    struct usb_phy *usb_phy;
    struct notifier_block usb_notify;
    struct notifier_block pd_swap_notify;
    struct notifier_block extcon_nb;
    struct power_supply *psy_usb;
    struct power_supply_charge_current cur;
    struct work_struct work;
    struct mutex lock;
    bool charging;
    u32 limit;
    struct delayed_work otg_work;
    struct delayed_work wdt_work;
    struct delayed_work cur_work;
    struct regmap *pmic;
    u32 charger_detect;
    u32 charger_pd;
    u32 charger_pd_mask;
    struct gpio_desc *gpiod;
    struct extcon_dev *edev;
    u32 last_limit_current;
    u32 role;
    bool need_disable_Q1;
    int termination_cur;
    int vol_max_mv;
    u32 actual_limit_current;
    u32 actual_limit_voltage;
    bool otg_enable;
    struct alarm otg_timer;
    struct sc89890h_charger_sysfs *sysfs;
    int reg_id;
    bool disable_power_path;
    bool is_sink;
    bool use_typec_extcon;
};

struct sc89890h_charger_reg_tab {
    int id;
    u32 addr;
    char *name;
};

static struct sc89890h_charger_reg_tab reg_tab[SC89890H_REG_NUM + 1] = {
    {0, SC89890H_REG_00, "Input control reg"},
    {1, SC89890H_REG_01, "DPDM driver reg "},
    {2, SC89890H_REG_02, "DPDM control reg"},
    {3, SC89890H_REG_03, "System control reg"},
    {4, SC89890H_REG_04, "Fast charge current config reg"},
    {5, SC89890H_REG_05, "Pre-charge and terminal corrent config reg"},
    {6, SC89890H_REG_06, "Vbatt config reg"},
    {7, SC89890H_REG_07, "System control of WD and JETIA reg"},
    {8, SC89890H_REG_08, "Thermal config reg"},
    {9, SC89890H_REG_09, "Battery FET control reg"},
    {10, SC89890H_REG_0A, "Boost Mode Related Setting reg"},
    {11, SC89890H_REG_0B, "Status of charge reg"},
    {12, SC89890H_REG_0C, "Fault reg"},
    {13, SC89890H_REG_0D, "Setting Vindpm reg"},
    {14, SC89890H_REG_0E, "Battery thermal state reg"},
    {15, SC89890H_REG_0F, "Vsys thermal state reg"},
    {16, SC89890H_REG_10, "Thermal ADC reg"},
    {17, SC89890H_REG_11, "Vbus ADC reg"},
    {18, SC89890H_REG_12, "ICC ADC reg"},
    {19, SC89890H_REG_13, "VDPDM state reg"},
    {20, SC89890H_REG_14, "PN and register reset reg"},
    {21, 0, "null"},
};

static int sc89890h_charger_set_limit_current(struct sc89890h_charger_info *info,
                         u32 limit_cur);

static int sc89890h_read(struct sc89890h_charger_info *info, u8 reg, u8 *data)
{
    int ret;
    int re_try = 3;

    ret = i2c_smbus_read_byte_data(info->client, reg);
    while ((ret < 0) && (re_try != 0)) {
        ret = i2c_smbus_read_byte_data(info->client, reg);
        re_try--;
    }

    if (re_try < 0)
        return ret;

    *data = ret;
    return 0;
}

static int sc89890h_write(struct sc89890h_charger_info *info, u8 reg, u8 data)
{
    return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int sc89890h_update_bits(struct sc89890h_charger_info *info, u8 reg,
                   u8 mask, u8 data)
{
    u8 v;
    int ret;

    ret = sc89890h_read(info, reg, &v);
    if (ret < 0)
        return ret;

    v &= ~mask;
    v |= (data & mask);

    return sc89890h_write(info, reg, v);
}

static void sc89890h_charger_dump_register(struct sc89890h_charger_info *info)
{
    int i, ret, len, idx = 0;
    u8 reg_val;
    char buf[512];

    memset(buf, '\0', sizeof(buf));
    for (i = 0; i < SC89890H_REG_NUM; i++) {
        ret = sc89890h_read(info, reg_tab[i].addr, &reg_val);
        if (ret == 0) {
            len = snprintf(buf + idx, sizeof(buf) - idx,
                       "[REG_0x%.2x]=0x%.2x; ", reg_tab[i].addr,
                       reg_val);
            idx += len;
        }
    }

    dev_err(info->dev, "%s: %s", __func__, buf);
}

static bool sc89890h_charger_is_bat_present(struct sc89890h_charger_info *info)
{
    struct power_supply *psy;
    union power_supply_propval val;
    bool present = false;
    int ret;

    psy = power_supply_get_by_name(SC89890H_BATTERY_NAME);
    if (!psy) {
        dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
        return present;
    }
    ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
                    &val);
    if (!ret && val.intval)
        present = true;
    power_supply_put(psy);

    if (ret)
        dev_err(info->dev, "Failed to get property of present:%d\n", ret);

    return present;
}

static int sc89890h_charger_is_fgu_present(struct sc89890h_charger_info *info)
{
    struct power_supply *psy;

    psy = power_supply_get_by_name(SC89890H_BATTERY_NAME);
    if (!psy) {
        dev_err(info->dev, "Failed to find psy of sc27xx_fgu\n");
        return -ENODEV;
    }
    power_supply_put(psy);

    return 0;
}

static int sc89890h_charger_get_pn_value(struct sc89890h_charger_info *info)
 {
     u8 reg_val;
     int ret;

     ret = sc89890h_read(info, SC89890H_REG_14, &reg_val);
    if (ret < 0) {
        dev_err(info->dev, "sc89890h Failed to read PN value register, ret = %d\n", ret);
        return ret;
    }

    reg_val &= REG14_PN_ID_MASK;
    reg_val >>= REG14_PN_ID_SHIFT;
    if (reg_val != REG14_PN_ID) {
        dev_err(info->dev, "The sc89890h PN value is 0x%x \n", reg_val);
        return -EINVAL;
    }

    return 0;

}

static int sc89890h_charger_set_recharge(struct sc89890h_charger_info *info)
{
    int ret = 0;

    ret = sc89890h_update_bits(info, SC89890H_REG_06,
                  REG06_VRECHG_MASK,
                  0x1 << REG06_VRECHG_SHIFT);

    return ret;
}

static int sc89890h_charger_set_vindpm(struct sc89890h_charger_info *info, u32 vol)
{
    int reg_val;
    int ret;

    ret = sc89890h_update_bits(info, SC89890H_REG_0D,
                   REG0D_FORCE_VINDPM_MASK, EN_FORCE_VINDPM << REG0D_FORCE_VINDPM_SHIFT);
    if (ret) {
        dev_err(info->dev, "force vindpm failed\n");
         return ret;
    }

    reg_val = vol;
    if (vol < 3900) {
        reg_val = 3900;
    } else if (vol > 15300) {
        reg_val = 15300;
    }

    reg_val = (reg_val - 2600) / 100;
    ret = sc89890h_update_bits(info, SC89890H_REG_0D,
                   REG0D_VINDPM_MASK, reg_val << REG0D_VINDPM_SHIFT);
    if (ret) {
        dev_err(info->dev, "set vindpm failed\n");
         return ret;
    }

    return 0;
}

static int sc89890h_charger_set_termina_vol(struct sc89890h_charger_info *info, u32 vol)
{
    int reg_val;
    int ret;
    int retry_cnt = 3;

    if (vol < SC89890H_VBATT_MIN_mv) {
        vol = SC89890H_VBATT_MIN_mv;
    } else if (vol > SC89890H_VBATT_MAX_mv) {
        vol = SC89890H_VBATT_MAX_mv;
    }

    reg_val = (vol-SC89890H_VBATT_OFFSET_mV) / SC89890H_VBATT_STEP_mV;
    do {
        ret = sc89890h_update_bits(info, SC89890H_REG_06, REG06_VREG_MASK,
                   reg_val << REG06_VREG_SHIFT);
        retry_cnt--;
        usleep_range(30, 40);
    } while ((retry_cnt != 0) && (ret != 0));

    if (ret != 0) {
        dev_err(info->dev, "sc89890h set terminal voltage failed\n");
    } else {
        info->actual_limit_voltage = (reg_val * SC89890H_VBATT_STEP_mV) + SC89890H_VBATT_OFFSET_mV;
        dev_err(info->dev, "sc89890h set terminal voltage success, the value is %d\n" ,info->actual_limit_voltage);
    }

    return ret;
}

static int sc89890h_charger_set_termina_cur(struct sc89890h_charger_info *info, u32 cur)
{
    int reg_val;

    if (cur < SC89890H_TERMCHRG_I_MIN_mA)
        cur = SC89890H_TERMCHRG_I_MIN_mA;
    else if (cur > SC89890H_TERMCHRG_I_MAX_mA)
        cur = SC89890H_TERMCHRG_I_MAX_mA;

    reg_val = (cur - SC89890H_TERMCHRG_I_OFFSET_mA) / SC89890H_TERMCHRG_I_STEP_mA;

    return sc89890h_update_bits(info, SC89890H_REG_05, REG05_ITERM_MASK, reg_val << REG05_ITERM_SHIFT);
}

static int sc89890h_charger_en_chg_timer(struct sc89890h_charger_info *info, bool val)
{
    int ret = 0;

    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

    if (val) {
        ret = sc89890h_update_bits(info, SC89890H_REG_07,
                  REG07_EN_TIMER_MASK,
                  0x1 << REG07_EN_TIMER_SHIFT);
        pr_info("sc89890h EN_TIMER is enabled\n");
    } else {
        ret = sc89890h_update_bits(info, SC89890H_REG_07,
                  REG07_EN_TIMER_MASK,
                  0x0 << REG07_EN_TIMER_SHIFT);
        pr_info("sc89890h EN_TIMER is disabled\n");
    }

    return ret;
}

static int sc89890h_charger_hw_init(struct sc89890h_charger_info *info)
{
    struct power_supply_battery_info bat_info;
    int ret;

    int bat_id = 0;

    bat_id = battery_get_bat_id();
    ret = power_supply_get_battery_info(info->psy_usb, &bat_info, bat_id);

    if (ret) {
        dev_warn(info->dev, "no battery information is supplied\n");

        /*
         * If no battery information is supplied, we should set
         * default charge termination current to 100 mA, and default
         * charge termination voltage to 4.2V.
         */
        info->cur.sdp_limit = 500000;
        info->cur.sdp_cur = 500000;
        info->cur.dcp_limit = 5000000;
        info->cur.dcp_cur = 500000;
        info->cur.cdp_limit = 5000000;
        info->cur.cdp_cur = 1500000;
        info->cur.unknown_limit = 5000000;
        info->cur.unknown_cur = 500000;
    } else {
        info->cur.sdp_limit = bat_info.cur.sdp_limit;
        info->cur.sdp_cur = bat_info.cur.sdp_cur;
        info->cur.dcp_limit = bat_info.cur.dcp_limit;
        info->cur.dcp_cur = bat_info.cur.dcp_cur;
        info->cur.cdp_limit = bat_info.cur.cdp_limit;
        info->cur.cdp_cur = bat_info.cur.cdp_cur;
        info->cur.unknown_limit = bat_info.cur.unknown_limit;
        info->cur.unknown_cur = bat_info.cur.unknown_cur;
        info->cur.fchg_limit = bat_info.cur.fchg_limit;
        info->cur.fchg_cur = bat_info.cur.fchg_cur;
#ifdef HQ_D85_BUILD
        info->vol_max_mv = 4000;
#else
        info->vol_max_mv = bat_info.constant_charge_voltage_max_uv / 1000;
#endif
        info->termination_cur = bat_info.charge_term_current_ua / 1000;
        power_supply_put_battery_info(info->psy_usb, &bat_info);

        ret = sc89890h_update_bits(info, SC89890H_REG_14, REG14_REG_RST_MASK,
                      REG_RESET << REG14_REG_RST_SHIFT);
        if (ret) {
            dev_err(info->dev, "reset sc89890h failed\n");
            return ret;
        }

        ret = sc89890h_charger_set_vindpm(info, info->vol_max_mv);
        if (ret) {
            dev_err(info->dev, "set sc89890h vindpm vol failed\n");
            return ret;
        }

        ret = sc89890h_charger_set_termina_vol(info,
                              info->vol_max_mv);
        if (ret) {
            dev_err(info->dev, "set sc89890h terminal vol failed\n");
            return ret;
        }

        ret = sc89890h_charger_set_termina_cur(info, info->termination_cur);
        if (ret) {
            dev_err(info->dev, "set sc89890h terminal cur failed\n");
            return ret;
        }

        ret = sc89890h_charger_set_limit_current(info,
                            info->cur.unknown_cur);
        if (ret) {
            dev_err(info->dev, "set sc89890h limit current failed\n");
            return ret;
        }

        ret = sc89890h_charger_set_recharge(info);
        if (ret) {
            dev_err(info->dev, "set sc89890h recharge failed\n");
            return ret;
        }

        ret = sc89890h_charger_en_chg_timer(info, false);
        if (ret) {
            dev_err(info->dev, "disable charger safe timer failed\n");
            return ret;
        }
        /* disable ILIM */
        ret = sc89890h_update_bits(info, SC89890H_REG_00,
                   REG00_EN_LIM_MASK, 0 << REG00_EN_LIM_SHIFT);
        if (ret) {
            dev_err(info->dev, "disable ILIM failed\n");
           return ret;
        }
    }

    return ret;
}

static int sc89890h_enter_hiz_mode(struct sc89890h_charger_info *info)
{
    int ret;

    ret = sc89890h_update_bits(info, SC89890H_REG_00,
                  REG00_ENHIZ_MASK, HIZ_ENABLE << REG00_ENHIZ_SHIFT);
    if (ret)
        dev_err(info->dev, "enter HIZ mode failed\n");

    return ret;
}

static int sc89890h_exit_hiz_mode(struct sc89890h_charger_info *info)
{
    int ret;

    ret = sc89890h_update_bits(info, SC89890H_REG_00,
                  REG00_ENHIZ_MASK, HIZ_DISABLE);
    if (ret)
        dev_err(info->dev, "exit HIZ mode failed\n");

    return ret;
}

static int sc89890h_get_hiz_mode(struct sc89890h_charger_info *info,u32 *value)
{
    u8 buf;
    int ret;

    ret = sc89890h_read(info, SC89890H_REG_00, &buf);
    *value = (buf & REG00_ENHIZ_MASK) >> REG00_ENHIZ_SHIFT;

    return ret;
}

static int sc89890h_charger_get_charge_voltage(struct sc89890h_charger_info *info,
                          u32 *charge_vol)
{
    struct power_supply *psy;
    union power_supply_propval val;
    int ret;

    psy = power_supply_get_by_name(SC89890H_BATTERY_NAME);
    if (!psy) {
        dev_err(info->dev, "failed to get SC89890H_BATTERY_NAME\n");
        return -ENODEV;
    }

    ret = power_supply_get_property(psy,
                    POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
                    &val);
    power_supply_put(psy);
    if (ret) {
        dev_err(info->dev, "failed to get CONSTANT_CHARGE_VOLTAGE\n");
        return ret;
    }

    *charge_vol = val.intval;

    return 0;
}

static int sc89890h_charger_start_charge(struct sc89890h_charger_info *info)
{
    int ret;

    ret = sc89890h_update_bits(info, SC89890H_REG_00,
                  REG00_ENHIZ_MASK, HIZ_DISABLE << REG00_ENHIZ_SHIFT);
    if (ret)
        dev_err(info->dev, "disable HIZ mode failed\n");

    ret = sc89890h_update_bits(info, SC89890H_REG_07, REG07_WDT_TIMER_MASK,
                  SC89890H_WDT_TIMER_40S << REG07_WDT_TIMER_SHIFT);
    if (ret) {
        dev_err(info->dev, "Failed to enable sc89890h watchdog\n");
        return ret;
    }

    ret = regmap_update_bits(info->pmic, info->charger_pd,
                 info->charger_pd_mask, 0);
    if (ret) {
        dev_err(info->dev, "enable sc89890h charge failed\n");
            return ret;
        }

    ret = sc89890h_charger_set_limit_current(info,
                        info->last_limit_current);
    if (ret) {
        dev_err(info->dev, "failed to set limit current\n");
        return ret;
    }

    ret = sc89890h_charger_set_termina_cur(info, info->termination_cur);
    if (ret)
        dev_err(info->dev, "set sc89890h terminal cur failed\n");

    return ret;
}

static void sc89890h_charger_stop_charge(struct sc89890h_charger_info *info)
{
    int ret;
    bool present = sc89890h_charger_is_bat_present(info);

    if (!present || info->need_disable_Q1) {
        ret = sc89890h_update_bits(info, SC89890H_REG_00, REG00_ENHIZ_MASK,
                      HIZ_ENABLE << REG00_ENHIZ_SHIFT);
        if (ret)
            dev_err(info->dev, "enable HIZ mode failed\n");
        info->need_disable_Q1 = false;
    }

    ret = regmap_update_bits(info->pmic, info->charger_pd,
                 info->charger_pd_mask,
                 info->charger_pd_mask);
    if (ret)
        dev_err(info->dev, "disable sc89890h charge failed\n");

    ret = sc89890h_update_bits(info, SC89890H_REG_07, REG07_WDT_TIMER_MASK,
                  SC89890H_WDT_TIMER_DISABLE << REG07_WDT_TIMER_SHIFT);
    if (ret)
        dev_err(info->dev, "Failed to disable sc89890h watchdog\n");

}

static int sc89890h_charger_set_current(struct sc89890h_charger_info *info,
                       u32 cur)
{
    int ret;
    int reg_val;

    cur = cur / 1000;
    if ( cur > SC89890H_ICHRG_I_MAX) {
        cur = SC89890H_ICHRG_I_MAX;
    } else if (cur < SC89890H_ICHRG_I_MIN) {
        cur = SC89890H_ICHRG_I_MIN;
    }

    reg_val = cur / SC89890H_ICHRG_CURRENT_STEP;

    ret = sc89890h_update_bits(info, SC89890H_REG_04, REG04_ICHG_MASK, reg_val);

    return ret;
}

static int sc89890h_charger_get_current(struct sc89890h_charger_info *info,
                       u32 *cur)
{
    u8 reg_val;
    int ret;

    ret = sc89890h_read(info, SC89890H_REG_04, &reg_val);
    if (ret < 0)
        return ret;

    reg_val &= REG04_ICHG_MASK;
    *cur = reg_val * REG04_ICHG_SHIFT * 1000;

    return 0;
}

static int sc89890h_charger_set_limit_current(struct sc89890h_charger_info *info,
                         u32 iindpm)
{
    int ret;
    int reg_val;
    info->last_limit_current = iindpm;
    iindpm = iindpm / 1000;
    if (iindpm < SC89890H_IINDPM_I_MIN) {
        iindpm = SC89890H_IINDPM_I_MIN;
    } else if (iindpm >= SC89890H_IINDPM_I_MAX) {
        iindpm = SC89890H_IINDPM_I_MAX;
    }

    reg_val = (iindpm - SC89890H_IINDPM_OFFSET) / SC89890H_IINDPM_STEP;
    iindpm = reg_val * SC89890H_IINDPM_STEP + SC89890H_IINDPM_I_MIN;

    ret = sc89890h_update_bits(info, SC89890H_REG_00, REG00_IINLIM_MASK, reg_val);
    if (ret)
        dev_err(info->dev, "set SC89890H limit cur failed\n");
    info->actual_limit_current = iindpm * 1000;
    return ret;
}

static u32 sc89890h_charger_get_limit_current(struct sc89890h_charger_info *info,
                         u32 *limit_cur)
{
    u8 reg_val;
    int ret;

    ret = sc89890h_read(info, SC89890H_REG_00, &reg_val);
    if (ret < 0)
        return ret;

    reg_val &= REG00_IINLIM_MASK;
    *limit_cur = reg_val * SC89890H_IINDPM_STEP + SC89890H_IINDPM_OFFSET;
    if (*limit_cur >= SC89890H_IINDPM_I_MAX)
        *limit_cur = SC89890H_IINDPM_I_MAX * 1000;
    else
        *limit_cur = *limit_cur * 1000;

    pr_err("%s: limit_cur = %d\n", __func__, limit_cur);

    return 0;
}

static inline int sc89890h_charger_get_health(struct sc89890h_charger_info *info,
                      u32 *health)
{
    *health = POWER_SUPPLY_HEALTH_GOOD;

    return 0;
}

static inline int sc89890h_charger_get_online(struct sc89890h_charger_info *info,
                      u32 *online)
{
    if (info->limit)
        *online = true;
    else
        *online = false;

    return 0;
}

static int sc89890h_charger_feed_watchdog(struct sc89890h_charger_info *info,
                     u32 val)
{
    int ret;
    u32 limit_cur = 0;
    u32 limit_voltage = 4200;

    ret = sc89890h_update_bits(info, SC89890H_REG_03, REG03_WDT_MASK,
                  WDT_RESET << REG03_WDT_SHIFT);
    if (ret) {
        dev_err(info->dev, "reset SC89890H failed\n");
        return ret;
    }

    ret = sc89890h_charger_get_limit_current(info, &limit_cur);
    if (ret) {
        dev_err(info->dev, "get limit cur failed\n");
        return ret;
    }

    if (info->actual_limit_voltage != limit_voltage) {
        ret = sc89890h_charger_set_termina_vol(info, info->actual_limit_voltage);
        if (ret) {
            dev_err(info->dev, "set terminal voltage failed\n");
            return ret;
        }

        ret = sc89890h_charger_set_recharge(info);
        if (ret) {
            dev_err(info->dev, "set sc89890h recharge failed\n");
            return ret;
        }
    }

    ret = sc89890h_charger_get_limit_current(info, &limit_cur);
    if (ret) {
        dev_err(info->dev, "get limit cur failed\n");
        return ret;
    }

    if (info->actual_limit_current == limit_cur)
        return 0;

    ret = sc89890h_charger_set_limit_current(info, info->actual_limit_current);
    if (ret) {
        dev_err(info->dev, "set limit cur failed\n");
        return ret;
    }

    return 0;
}

static int sc89890h_charger_set_fchg_current(struct sc89890h_charger_info *info,
                        u32 val)
{
    int ret, limit_cur, cur;

    if (val == CM_FAST_CHARGE_ENABLE_CMD) {
        limit_cur = info->cur.fchg_limit;
        cur = info->cur.fchg_cur;
    } else if (val == CM_FAST_CHARGE_DISABLE_CMD) {
        limit_cur = info->cur.dcp_limit;
        cur = info->cur.dcp_cur;
    } else {
        return 0;
    }

    ret = sc89890h_charger_set_limit_current(info, limit_cur);
    if (ret) {
        dev_err(info->dev, "failed to set fchg limit current\n");
        return ret;
    }

    ret = sc89890h_charger_set_current(info, cur);
    if (ret) {
        dev_err(info->dev, "failed to set fchg current\n");
        return ret;
    }

    return 0;
}

static inline int sc89890h_charger_get_status(struct sc89890h_charger_info *info)
{
    if (info->charging)
        return POWER_SUPPLY_STATUS_CHARGING;
    else
        return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static int sc89890h_charger_get_charge_done(struct sc89890h_charger_info *info,
    union power_supply_propval *val)
{
    int ret = 0;
    u8 reg_val = 0;

    if (!info || !val) {
        dev_err(info->dev, "[%s]line=%d: info or val is NULL\n", __FUNCTION__, __LINE__);
        return ret;
    }

    ret = sc89890h_read(info, SC89890H_REG_0B, &reg_val);
    if (ret < 0) {
        dev_err(info->dev, "Failed to get charge_done, ret = %d\n", ret);
        return ret;
    }

    reg_val &= REG0B_CHRG_STAT_MASK;
    reg_val >>= REG0B_CHRG_STAT_SHIFT;
    val->intval = (reg_val == SC89890H_CHARGE_DONE);

    return 0;
}

static int sc89890h_charger_set_status(struct sc89890h_charger_info *info,
                      int val)
{
    int ret = 0;
    u32 input_vol;

    if (val == CM_FAST_CHARGE_ENABLE_CMD) {
        ret = sc89890h_charger_set_fchg_current(info, val);
        if (ret) {
            dev_err(info->dev, "failed to set 9V fast charge current\n");
            return ret;
        }
    } else if (val == CM_FAST_CHARGE_DISABLE_CMD) {
        ret = sc89890h_charger_set_fchg_current(info, val);
        if (ret) {
            dev_err(info->dev, "failed to set 5V normal charge current\n");
            return ret;
        }
        ret = sc89890h_charger_get_charge_voltage(info, &input_vol);
        if (ret) {
            dev_err(info->dev, "failed to get charge voltage\n");
            return ret;
        }
        if (input_vol > CM_FAST_CHARGE_DISABLE_CMD)  //SXX
            info->need_disable_Q1 = true;
    } else if (val == false) {
        ret = sc89890h_charger_get_charge_voltage(info, &input_vol);
        if (ret) {
            dev_err(info->dev, "failed to get 5V charge voltage\n");
            return ret;
        }
        if (input_vol > SC89890H_NORMAL_CHG_VOL_MAX)
            info->need_disable_Q1 = true;
    }

    if (val > CM_FAST_CHARGE_NORMAL_CMD)
        return 0;

    if (!val && info->charging) {
        sc89890h_charger_stop_charge(info);
        info->charging = false;
    } else if (val && !info->charging) {
        ret = sc89890h_charger_start_charge(info);
        if (ret)
            dev_err(info->dev, "start charge failed\n");
        else
            info->charging = true;
    }

    return ret;
}

static void sc89890h_charger_work(struct work_struct *data)
{
    struct sc89890h_charger_info *info =
        container_of(data, struct sc89890h_charger_info, work);
    bool present;
    int retry_cnt = 12;

    present = sc89890h_charger_is_bat_present(info);

    if (info->use_typec_extcon && info->limit) {
        /* if use typec extcon notify charger,
         * wait for BC1.2 detect charger type.
        */
        while (retry_cnt > 0) {
            if (info->usb_phy->chg_type != UNKNOWN_TYPE) {
                break;
            }
            retry_cnt--;
            msleep(50);
        }
        dev_info(info->dev, "retry_cnt = %d\n", retry_cnt);
    }

    dev_info(info->dev, "battery present = %d, charger type = %d\n",
         present, info->usb_phy->chg_type);
    cm_notify_event(info->psy_usb, CM_EVENT_CHG_START_STOP, NULL);
}

static ssize_t sc89890h_reg_val_show(struct device *dev,
                       struct device_attribute *attr,
                       char *buf)
{
    struct sc89890h_charger_sysfs *sc89890h_sysfs =
        container_of(attr, struct sc89890h_charger_sysfs,
                 attr_sc89890h_reg_val);
    struct sc89890h_charger_info *info = sc89890h_sysfs->info;
    u8 val;
    int ret;

    if (!info)
        return sprintf(buf, "%s sc89890h_sysfs->info is null\n", __func__);

    ret = sc89890h_read(info, reg_tab[info->reg_id].addr, &val);
    if (ret) {
        dev_err(info->dev, "fail to get SC89890H_REG_0x%.2x value, ret = %d\n",
            reg_tab[info->reg_id].addr, ret);
        return sprintf(buf, "fail to get SC89890H_REG_0x%.2x value\n",
                   reg_tab[info->reg_id].addr);
    }

    return sprintf(buf, "SC89890H_REG_0x%.2x = 0x%.2x\n",
               reg_tab[info->reg_id].addr, val);
}

static ssize_t sc89890h_reg_val_store(struct device *dev,
                        struct device_attribute *attr,
                        const char *buf, size_t count)
{
    struct sc89890h_charger_sysfs *sc89890h_sysfs =
        container_of(attr, struct sc89890h_charger_sysfs,
                 attr_sc89890h_reg_val);
    struct sc89890h_charger_info *info = sc89890h_sysfs->info;
    u8 val;
    int ret;

    if (!info) {
        dev_err(dev, "%s sc89890h_sysfs->info is null\n", __func__);
        return count;
    }

    ret =  kstrtou8(buf, 16, &val);
    if (ret) {
        dev_err(info->dev, "fail to get addr, ret = %d\n", ret);
        return count;
    }

    ret = sc89890h_write(info, reg_tab[info->reg_id].addr, val);
    if (ret) {
        dev_err(info->dev, "fail to wite 0x%.2x to REG_0x%.2x, ret = %d\n",
                val, reg_tab[info->reg_id].addr, ret);
        return count;
    }

    dev_info(info->dev, "wite 0x%.2x to REG_0x%.2x success\n", val,
         reg_tab[info->reg_id].addr);
    return count;
}

static ssize_t sc89890h_reg_id_store(struct device *dev,
                     struct device_attribute *attr,
                     const char *buf, size_t count)
{
    struct sc89890h_charger_sysfs *sc89890h_sysfs =
        container_of(attr, struct sc89890h_charger_sysfs,
                 attr_sc89890h_sel_reg_id);
    struct sc89890h_charger_info *info = sc89890h_sysfs->info;
    int ret, id;

    if (!info) {
        dev_err(dev, "%s sc89890h_sysfs->info is null\n", __func__);
        return count;
    }

    ret =  kstrtoint(buf, 10, &id);
    if (ret) {
        dev_err(info->dev, "%s store register id fail\n", sc89890h_sysfs->name);
        return count;
    }

    if (id < 0 || id >= SC89890H_REG_NUM) {
        dev_err(info->dev, "%s store register id fail, id = %d is out of range\n",
            sc89890h_sysfs->name, id);
        return count;
    }

    info->reg_id = id;

    dev_info(info->dev, "%s store register id = %d success\n", sc89890h_sysfs->name, id);
    return count;
}

static ssize_t sc89890h_reg_id_show(struct device *dev,
                    struct device_attribute *attr,
                    char *buf)
{
    struct sc89890h_charger_sysfs *sc89890h_sysfs =
        container_of(attr, struct sc89890h_charger_sysfs,
                 attr_sc89890h_sel_reg_id);
    struct sc89890h_charger_info *info = sc89890h_sysfs->info;

    if (!info)
        return sprintf(buf, "%s sc89890h_sysfs->info is null\n", __func__);

    return sprintf(buf, "Cuurent register id = %d\n", info->reg_id);
}

static ssize_t sc89890h_reg_table_show(struct device *dev,
                       struct device_attribute *attr,
                       char *buf)
{
    struct sc89890h_charger_sysfs *sc89890h_sysfs =
        container_of(attr, struct sc89890h_charger_sysfs,
                 attr_sc89890h_lookup_reg);
    struct sc89890h_charger_info *info = sc89890h_sysfs->info;
    int i, len, idx = 0;
    char reg_tab_buf[2048];

    if (!info)
        return sprintf(buf, "%s sc89890h_sysfs->info is null\n", __func__);

    memset(reg_tab_buf, '\0', sizeof(reg_tab_buf));
    len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
               "Format: [id] [addr] [desc]\n");
    idx += len;

    for (i = 0; i < SC89890H_REG_NUM; i++) {
        len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
                   "[%d] [REG_0x%.2x] [%s]; \n",
                   reg_tab[i].id, reg_tab[i].addr, reg_tab[i].name);
        idx += len;
    }

    return sprintf(buf, "%s\n", reg_tab_buf);
}

static ssize_t sc89890h_dump_reg_show(struct device *dev,
                      struct device_attribute *attr,
                      char *buf)
{
    struct sc89890h_charger_sysfs *sc89890h_sysfs =
        container_of(attr, struct sc89890h_charger_sysfs,
                 attr_sc89890h_dump_reg);
    struct sc89890h_charger_info *info = sc89890h_sysfs->info;

    if (!info)
        return sprintf(buf, "%s sc89890h_sysfs->info is null\n", __func__);

    sc89890h_charger_dump_register(info);

    return sprintf(buf, "%s\n", sc89890h_sysfs->name);
}

static int sc89890h_register_sysfs(struct sc89890h_charger_info *info)
{
    struct sc89890h_charger_sysfs *sc89890h_sysfs;
    int ret;

    sc89890h_sysfs = devm_kzalloc(info->dev, sizeof(*sc89890h_sysfs), GFP_KERNEL);
    if (!sc89890h_sysfs)
        return -ENOMEM;

    info->sysfs = sc89890h_sysfs;
    sc89890h_sysfs->name = "sc89890h_sysfs";
    sc89890h_sysfs->info = info;
    sc89890h_sysfs->attrs[0] = &sc89890h_sysfs->attr_sc89890h_dump_reg.attr;
    sc89890h_sysfs->attrs[1] = &sc89890h_sysfs->attr_sc89890h_lookup_reg.attr;
    sc89890h_sysfs->attrs[2] = &sc89890h_sysfs->attr_sc89890h_sel_reg_id.attr;
    sc89890h_sysfs->attrs[3] = &sc89890h_sysfs->attr_sc89890h_reg_val.attr;
    sc89890h_sysfs->attrs[4] = NULL;
    sc89890h_sysfs->attr_g.name = "debug";
    sc89890h_sysfs->attr_g.attrs = sc89890h_sysfs->attrs;

    sysfs_attr_init(&sc89890h_sysfs->attr_sc89890h_dump_reg.attr);
    sc89890h_sysfs->attr_sc89890h_dump_reg.attr.name = "sc89890h_dump_reg";
    sc89890h_sysfs->attr_sc89890h_dump_reg.attr.mode = 0444;
    sc89890h_sysfs->attr_sc89890h_dump_reg.show = sc89890h_dump_reg_show;

    sysfs_attr_init(&sc89890h_sysfs->attr_sc89890h_lookup_reg.attr);
    sc89890h_sysfs->attr_sc89890h_lookup_reg.attr.name = "sc89890h_lookup_reg";
    sc89890h_sysfs->attr_sc89890h_lookup_reg.attr.mode = 0444;
    sc89890h_sysfs->attr_sc89890h_lookup_reg.show = sc89890h_reg_table_show;

    sysfs_attr_init(&sc89890h_sysfs->attr_sc89890h_sel_reg_id.attr);
    sc89890h_sysfs->attr_sc89890h_sel_reg_id.attr.name = "sc89890h_sel_reg_id";
    sc89890h_sysfs->attr_sc89890h_sel_reg_id.attr.mode = 0644;
    sc89890h_sysfs->attr_sc89890h_sel_reg_id.show = sc89890h_reg_id_show;
    sc89890h_sysfs->attr_sc89890h_sel_reg_id.store = sc89890h_reg_id_store;

    sysfs_attr_init(&sc89890h_sysfs->attr_sc89890h_reg_val.attr);
    sc89890h_sysfs->attr_sc89890h_reg_val.attr.name = "sc89890h_reg_val";
    sc89890h_sysfs->attr_sc89890h_reg_val.attr.mode = 0644;
    sc89890h_sysfs->attr_sc89890h_reg_val.show = sc89890h_reg_val_show;
    sc89890h_sysfs->attr_sc89890h_reg_val.store = sc89890h_reg_val_store;

    ret = sysfs_create_group(&info->psy_usb->dev.kobj, &sc89890h_sysfs->attr_g);
    if (ret < 0)
        dev_err(info->dev, "Cannot create sysfs , ret = %d\n", ret);

    return ret;
}

static void sc89890h_current_work(struct work_struct *data)
{
    struct delayed_work *dwork = to_delayed_work(data);
    struct sc89890h_charger_info *info =
     container_of(dwork, struct sc89890h_charger_info, cur_work);
    u32 limit_cur = 0;
    int ret = 0;

    ret = sc89890h_charger_get_limit_current(info, &limit_cur);
    if (ret) {
        dev_err(info->dev, "get limit cur failed\n");
        return;
    }

    if (info->actual_limit_current == limit_cur)
        return;

    ret = sc89890h_charger_set_limit_current(info, info->actual_limit_current);
    if (ret) {
        dev_err(info->dev, "set limit cur failed\n");
        return;
    }
}

static int sc89890h_charger_usb_change(struct notifier_block *nb,
                      unsigned long limit, void *data)
{
    struct sc89890h_charger_info *info =
        container_of(nb, struct sc89890h_charger_info, usb_notify);

    info->limit = limit;

    pm_wakeup_event(info->dev, SC89890H_WAKE_UP_MS);

    schedule_work(&info->work);
    return NOTIFY_OK;
}

static int sc89890h_charger_extcon_event(struct notifier_block *nb,
                  unsigned long event, void *param)
{
    struct sc89890h_charger_info *info =
        container_of(nb, struct sc89890h_charger_info, extcon_nb);
    int state = 0;

    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return NOTIFY_OK;
    }

    state = extcon_get_state(info->edev, EXTCON_SINK);
    if (state < 0) {
        return NOTIFY_OK;
    }

    if (info->is_sink == state) {
        return NOTIFY_OK;
    }

    info->is_sink = state;

    if (info->is_sink) {
        info->limit = 500;
    } else {
        info->limit = 0;
    }

    pm_wakeup_event(info->dev, SC89890H_WAKE_UP_MS);

    schedule_work(&info->work);
    return NOTIFY_OK;
}

static int sc89890h_charger_usb_get_property(struct power_supply *psy,
                        enum power_supply_property psp,
                        union power_supply_propval *val)
{
    struct sc89890h_charger_info *info = power_supply_get_drvdata(psy);
    u32 cur, online, health, enabled = 0;
    enum usb_charger_type type;
    int ret = 0;

    if (!info)
        return -ENOMEM;

    mutex_lock(&info->lock);

    switch (psp) {
    case POWER_SUPPLY_PROP_STATUS:
        if (info->limit)
            val->intval = sc89890h_charger_get_status(info);
        else
            val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
        break;

    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        if (!info->charging) {
            val->intval = 0;
        } else {
            ret = sc89890h_charger_get_current(info, &cur);
            if (ret)
                goto out;

            val->intval = cur;
        }
        break;

    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        if (!info->charging) {
            val->intval = 0;
        } else {
            ret = sc89890h_charger_get_limit_current(info, &cur);
            if (ret)
                goto out;

            val->intval = cur;
        }
        break;

    case POWER_SUPPLY_PROP_ONLINE:
        ret = sc89890h_charger_get_online(info, &online);
        if (ret)
            goto out;

        val->intval = online;

        break;

    case POWER_SUPPLY_PROP_HEALTH:
        if (info->charging) {
            val->intval = 0;
        } else {
            ret = sc89890h_charger_get_health(info, &health);
            if (ret)
                goto out;

            val->intval = health;
        }
        break;

    case POWER_SUPPLY_PROP_USB_TYPE:
        type = info->usb_phy->chg_type;

        switch (type) {
        case SDP_TYPE:
            val->intval = POWER_SUPPLY_USB_TYPE_SDP;
            break;

        case DCP_TYPE:
            val->intval = POWER_SUPPLY_USB_TYPE_DCP;
            break;

        case CDP_TYPE:
            val->intval = POWER_SUPPLY_USB_TYPE_CDP;
            break;

#if !defined(HQ_FACTORY_BUILD)
        case FLOAT_TYPE:
            val->intval = POWER_SUPPLY_USB_TYPE_FLOAT;
            break;
#endif

        default:
            val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
        }

        break;

    case POWER_SUPPLY_PROP_CHARGE_ENABLED:
        ret = regmap_read(info->pmic, info->charger_pd, &enabled);
        if (ret) {
            dev_err(info->dev, "get sc89890h charge status failed\n");
            goto out;
        }

        val->intval = !enabled;
        break;
    case POWER_SUPPLY_PROP_POWER_PATH_ENABLED:
        ret = sc89890h_get_hiz_mode(info, &enabled);
        val->intval = !enabled;
        break;
    case POWER_SUPPLY_PROP_DUMP_CHARGER_IC:
        sc89890h_charger_dump_register(info);
        break;
    case POWER_SUPPLY_PROP_CHARGE_DONE:
        sc89890h_charger_get_charge_done(info, val);
        break;
    default:
        ret = -EINVAL;
    }

out:
    mutex_unlock(&info->lock);
    return ret;
}

static int sc89890h_charger_usb_set_property(struct power_supply *psy,
                        enum power_supply_property psp,
                        const union power_supply_propval *val)
{
    struct sc89890h_charger_info *info = power_supply_get_drvdata(psy);
    int ret = 0;

    if (!info)
        return -ENOMEM;

    mutex_lock(&info->lock);

    switch (psp) {
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        ret = sc89890h_charger_set_current(info, val->intval);
        if (ret < 0)
            dev_err(info->dev, "set charge current failed\n");

        break;
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        ret = sc89890h_charger_set_limit_current(info, val->intval);
        if (ret < 0)
            dev_err(info->dev, "set input current limit failed\n");

        schedule_delayed_work(&info->cur_work, SC89890H_CURRENT_WORK_MS*5);
        break;

    case POWER_SUPPLY_PROP_STATUS:
        ret = sc89890h_charger_set_status(info, val->intval);
        if (ret < 0)
            dev_err(info->dev, "set charge status failed\n");
        break;

    case POWER_SUPPLY_PROP_FEED_WATCHDOG:
        ret = sc89890h_charger_feed_watchdog(info, val->intval);
        if (ret < 0)
            dev_err(info->dev, "feed charger watchdog failed\n");
        break;

    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
        ret = sc89890h_charger_set_termina_vol(info, val->intval / 1000);
#ifdef HQ_D85_BUILD
        if (ret < 0) {
            dev_err(info->dev, "failed to set terminate voltage\n");
        } else {
            ret = sc89890h_charger_set_vindpm(info, 4300);
            if (ret < 0) {
                dev_err(info->dev, "failed to set vindpm\n");
            }
        }
#else
        if (ret < 0) {
            dev_err(info->dev, "failed to set terminate voltage\n");
        }
#endif
        break;

    case POWER_SUPPLY_PROP_CHARGE_ENABLED:
        if (val->intval == true) {
            ret = sc89890h_charger_start_charge(info);
            if (ret)
                dev_err(info->dev, "start charge failed\n");
        } else if (val->intval == false) {
            sc89890h_charger_stop_charge(info);
        }
        break;
    case POWER_SUPPLY_PROP_POWER_PATH_ENABLED:
        if(val->intval) {
            sc89890h_exit_hiz_mode(info);
        } else {
            sc89890h_enter_hiz_mode(info);
        }
        break;

    case POWER_SUPPLY_PROP_EN_CHG_TIMER:
        if(val->intval)
            ret = sc89890h_charger_en_chg_timer(info, true);
        else
            ret = sc89890h_charger_en_chg_timer(info, false);
        break;

    default:
        ret = -EINVAL;
    }

    mutex_unlock(&info->lock);
    return ret;
}

static int sc89890h_charger_property_is_writeable(struct power_supply *psy,
                         enum power_supply_property psp)
{
    int ret;

    switch (psp) {
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
    case POWER_SUPPLY_PROP_STATUS:
    case POWER_SUPPLY_PROP_EN_CHG_TIMER:
    case POWER_SUPPLY_PROP_CHARGE_ENABLED:
    case POWER_SUPPLY_PROP_POWER_PATH_ENABLED:
        ret = 1;
        break;

    default:
        ret = 0;
    }

    return ret;
}

static enum power_supply_usb_type sc89890h_charger_usb_types[] = {
    POWER_SUPPLY_USB_TYPE_UNKNOWN,
    POWER_SUPPLY_USB_TYPE_SDP,
    POWER_SUPPLY_USB_TYPE_DCP,
    POWER_SUPPLY_USB_TYPE_CDP,
    POWER_SUPPLY_USB_TYPE_C,
    POWER_SUPPLY_USB_TYPE_PD,
    POWER_SUPPLY_USB_TYPE_PD_DRP,
    POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID,
#if !defined(HQ_FACTORY_BUILD)
    POWER_SUPPLY_USB_TYPE_FLOAT,
#endif
};

static enum power_supply_property sc89890h_usb_props[] = {
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
    POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
    POWER_SUPPLY_PROP_ONLINE,
    POWER_SUPPLY_PROP_HEALTH,
    POWER_SUPPLY_PROP_USB_TYPE,
    POWER_SUPPLY_PROP_CHARGE_ENABLED,
    POWER_SUPPLY_PROP_DUMP_CHARGER_IC,
    POWER_SUPPLY_PROP_POWER_PATH_ENABLED,
    POWER_SUPPLY_PROP_CHARGE_DONE,
};

static const struct power_supply_desc sc89890h_charger_desc = {
    .name            = "sc89890h_charger",
    .type            = POWER_SUPPLY_TYPE_UNKNOWN,
    .properties        = sc89890h_usb_props,
    .num_properties        = ARRAY_SIZE(sc89890h_usb_props),
    .get_property        = sc89890h_charger_usb_get_property,
    .set_property        = sc89890h_charger_usb_set_property,
    .property_is_writeable    = sc89890h_charger_property_is_writeable,
    .usb_types        = sc89890h_charger_usb_types,
    .num_usb_types        = ARRAY_SIZE(sc89890h_charger_usb_types),
};

static void sc89890h_charger_detect_status(struct sc89890h_charger_info *info)
{
    int state = 0;

    if (info->use_typec_extcon) {
        state = extcon_get_state(info->edev, EXTCON_SINK);
        if (state < 0) {
            return;
        }
        if (state == 0) {
            return;
        }
        info->is_sink = state;

        if (info->is_sink) {
            info->limit = 500;
        }

        schedule_work(&info->work);
    } else {
        unsigned int min, max;

        /*
        * If the USB charger status has been USB_CHARGER_PRESENT before
        * registering the notifier, we should start to charge with getting
        * the charge current.
        */
        if (info->usb_phy->chg_state != USB_CHARGER_PRESENT) {
            return;
        }

        usb_phy_get_charger_current(info->usb_phy, &min, &max);
        info->limit = min;

        /*
        * slave no need to start charge when vbus change.
        * due to charging in shut down will check each psy
        * whether online or not, so let info->limit = min.
        */
        schedule_work(&info->work);
    }
}

static void sc89890h_charger_feed_watchdog_work(struct work_struct *work)
{
    struct delayed_work *dwork = to_delayed_work(work);
    struct sc89890h_charger_info *info = container_of(dwork,
                             struct sc89890h_charger_info,
                             wdt_work);
    int ret;

    ret = sc89890h_update_bits(info, SC89890H_REG_03, REG03_WDT_MASK,
                  WDT_RESET << REG03_WDT_SHIFT);
    if (ret) {
        dev_err(info->dev, "reset sc89890h failed\n");
        return;
    }
    schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#ifdef CONFIG_REGULATOR
static bool sc89890h_charger_check_otg_valid(struct sc89890h_charger_info *info)
{
    int ret;
    u8 value = 0;
    bool status = false;

    ret = sc89890h_read(info, SC89890H_REG_03, &value);
    if (ret) {
        dev_err(info->dev, "get sc89890h charger otg valid status failed\n");
        return status;
    }

    if (value & REG03_OTG_CONFIG_MASK)
        status = true;
    else
        dev_err(info->dev, "otg is not valid, REG_1= 0x%x\n", value);

    return status;
}

static bool sc89890h_charger_check_otg_fault(struct sc89890h_charger_info *info)
{
    int ret;
    u8 value = 0;
    bool status = true;

    ret = sc89890h_read(info, SC89890H_REG_0C, &value);
    if (ret) {
        dev_err(info->dev, "get SC89890H charger otg fault status failed\n");
        return status;
    }

    if (!(value & REG0C_BOOST_FAULT_MASK))
        status = false;
    else
        dev_err(info->dev, "boost fault occurs, REG_0C = 0x%x\n",
            value);

    return status;
}

static void sc89890h_charger_otg_work(struct work_struct *work)
{
    struct delayed_work *dwork = to_delayed_work(work);
    struct sc89890h_charger_info *info = container_of(dwork,
            struct sc89890h_charger_info, otg_work);
    bool otg_valid = sc89890h_charger_check_otg_valid(info);
    bool otg_fault;
    int ret, retry = 0;

    if (otg_valid)
        goto out;

    do {
        otg_fault = sc89890h_charger_check_otg_fault(info);
        if (!otg_fault) {
            ret = sc89890h_update_bits(info, SC89890H_REG_03,
                          REG03_OTG_CONFIG_MASK,
                          OTG_ENABLE << REG03_OTG_CONFIG_SHIFT);
            if (ret)
                dev_err(info->dev, "restart sc89890h charger otg failed\n");
        }

        otg_valid = sc89890h_charger_check_otg_valid(info);
    } while (!otg_valid && retry++ < SC89890H_OTG_RETRY_TIMES);

    if (retry >= SC89890H_OTG_RETRY_TIMES) {
        dev_err(info->dev, "Restart OTG failed\n");
        return;
    }

out:
    schedule_delayed_work(&info->otg_work, msecs_to_jiffies(1500));
}

static int sc89890h_charger_enable_otg(struct regulator_dev *dev)
{
    struct sc89890h_charger_info *info = rdev_get_drvdata(dev);
    int ret = 0;

    dev_info(info->dev, "%s:line%d enter\n", __func__, __LINE__);
    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

    mutex_lock(&info->lock);
    if (!info->use_typec_extcon) {
        ret = regmap_update_bits(info->pmic, info->charger_detect,
                    BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
        if (ret) {
            dev_err(info->dev, "failed to disable bc1.2 detect function.\n");
            goto out ;
        }
    }

    ret = sc89890h_update_bits(info, SC89890H_REG_03, REG03_OTG_CONFIG_MASK,
                  OTG_ENABLE << REG03_OTG_CONFIG_SHIFT);

    if (ret) {
        dev_err(info->dev, "enable sc89890h otg failed\n");
        regmap_update_bits(info->pmic, info->charger_detect,
                   BIT_DP_DM_BC_ENB, 0);
        goto out ;
    }

    info->otg_enable = true;

    ret = sc89890h_update_bits(info, SC89890H_REG_03, REG03_ENCHG_MASK,
                  CHG_DISABLE << REG03_ENCHG_SHIFT);

    if (ret) {
        dev_err(info->dev, "disable sc89890h chg failed\n");
        regmap_update_bits(info->pmic, info->charger_detect,
                   BIT_DP_DM_BC_ENB, 0);
        goto out ;
    }
    schedule_delayed_work(&info->wdt_work,
                  msecs_to_jiffies(SC89890H_WDT_VALID_MS));
    schedule_delayed_work(&info->otg_work,
                  msecs_to_jiffies(SC89890H_OTG_VALID_MS));
out:
    mutex_unlock(&info->lock);

    return 0;
}

static int sc89890h_charger_disable_otg(struct regulator_dev *dev)
{
    struct sc89890h_charger_info *info = rdev_get_drvdata(dev);
    int ret=0;

    dev_info(info->dev, "%s:line%d enter\n", __func__, __LINE__);
    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

    mutex_lock(&info->lock);

    info->otg_enable = false;
    cancel_delayed_work_sync(&info->wdt_work);
    cancel_delayed_work_sync(&info->otg_work);
    ret = sc89890h_update_bits(info, SC89890H_REG_03,
                   REG03_OTG_CONFIG_MASK, OTG_DISABLE);
    if (ret) {
       dev_err(info->dev, "disable sc89890h otg failed\n");
       goto out;
    }

    ret = sc89890h_update_bits(info, SC89890H_REG_03, REG03_ENCHG_MASK,
                  CHG_ENABLE << REG03_ENCHG_SHIFT);

    if (ret) {
        dev_err(info->dev, "enable sc89890h chg failed\n");
        regmap_update_bits(info->pmic, info->charger_detect,
                   BIT_DP_DM_BC_ENB, 0);
        goto out ;
    }

    /* Enable charger detection function to identify the charger type */
    ret =  regmap_update_bits(info->pmic, info->charger_detect,
                   BIT_DP_DM_BC_ENB, 0);
    if(ret)
        dev_err(info->dev, "enable BC1.2 failed\n");

out:
    mutex_unlock(&info->lock);
    return ret;
}

static int sc89890h_charger_vbus_is_enabled(struct regulator_dev *dev)
{
    struct sc89890h_charger_info *info = rdev_get_drvdata(dev);
     int ret;
     u8 val;

    dev_info(info->dev, "%s:line%d enter\n", __func__, __LINE__);
    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

    mutex_lock(&info->lock);
    ret = sc89890h_read(info, SC89890H_REG_03, &val);
     if (ret) {
        dev_err(info->dev, "failed to get sc89890h otg status\n");
        mutex_unlock(&info->lock);
         return ret;
     }

     val &= REG03_OTG_CONFIG_MASK;

    dev_info(info->dev, "%s:line%d config otg mask val = %d\n", __func__, __LINE__, val);

    mutex_unlock(&info->lock);
    return val;
}

static const struct regulator_ops sc89890h_charger_vbus_ops = {
    .enable = sc89890h_charger_enable_otg,
    .disable = sc89890h_charger_disable_otg,
    .is_enabled = sc89890h_charger_vbus_is_enabled,
};

static const struct regulator_desc sc89890h_charger_vbus_desc = {
    .name = "sc89890h_otg_vbus",
    .of_match = "sc89890h_otg_vbus",
    .type = REGULATOR_VOLTAGE,
    .owner = THIS_MODULE,
    .ops = &sc89890h_charger_vbus_ops,
    .fixed_uV = 5000000,
    .n_voltages = 1,
};

static int sc89890h_charger_register_vbus_regulator(struct sc89890h_charger_info *info)
{
    struct regulator_config cfg = { };
    struct regulator_dev *reg;
    int ret = 0;

    cfg.dev = info->dev;
    cfg.driver_data = info;
    reg = devm_regulator_register(info->dev,
                      &sc89890h_charger_vbus_desc, &cfg);
    if (IS_ERR(reg)) {
        ret = PTR_ERR(reg);
        dev_err(info->dev, "Can't register regulator:%d\n", ret);
    }

    return ret;
}

#else
static int sc89890h_charger_register_vbus_regulator(struct sc89890h_charger_info *info)
{
    return 0;
}
#endif

static int sc89890h_charger_probe(struct i2c_client *client,
                 const struct i2c_device_id *id)
{
    struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
    struct device *dev = &client->dev;
    struct power_supply_config charger_cfg = { };
    struct sc89890h_charger_info *info;
    struct device_node *regmap_np;
    struct platform_device *regmap_pdev;
    int ret;
    int retry_cnt = 3;

    pr_err("%s: start!\n", __func__);

    if (!adapter) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

    if (!dev) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

    if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
        dev_err(dev, "No support for SMBUS_BYTE_DATA\n");
        return -ENODEV;
    }

    info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
    if (!info)
        return -ENOMEM;

    /* Tab A7 Lite T618 code for AX6189DEV-373 by shixuanxuan at 20220111 start */
    client->addr = 0x6A;
    /* Tab A7 Lite T618 code for AX6189DEV-373 by shixuanxuan at 20220111 end */
    info->client = client;
    info->dev = dev;

    ret = sc89890h_charger_get_pn_value(info);
    if (ret) {
        dev_err(dev, "failed to get sgm pn value \n");
        return ret;
    } else {
        pr_err("sc89890h get pn value success !\n");
    }

    /* disable ILIM */
    ret = sc89890h_update_bits(info, SC89890H_REG_00,
                   REG00_EN_LIM_MASK, 0 << REG00_EN_LIM_SHIFT);
    if (ret) {
        dev_err(info->dev, "disable ILIM failed\n");
         return ret;
    }

    info->use_typec_extcon =
        device_property_read_bool(dev, "use-typec-extcon");
    alarm_init(&info->otg_timer, ALARM_BOOTTIME, NULL);

    INIT_WORK(&info->work, sc89890h_charger_work);
    INIT_DELAYED_WORK(&info->cur_work, sc89890h_current_work);

    i2c_set_clientdata(client, info);

    info->usb_phy = devm_usb_get_phy_by_phandle(dev, "phys", 0);
    if (IS_ERR(info->usb_phy)) {
        dev_err(dev, "failed to find USB phy\n");
        return PTR_ERR(info->usb_phy);
    }

    info->edev = extcon_get_edev_by_phandle(info->dev, 0);
    if (IS_ERR(info->edev)) {
        dev_err(dev, "failed to find vbus extcon device.\n");
        return PTR_ERR(info->edev);
    }

    ret = sc89890h_charger_is_fgu_present(info);
    if (ret) {
        dev_err(dev, "sc27xx_fgu not ready.\n");
        return -EPROBE_DEFER;
    }

    if (info->use_typec_extcon) {
        info->extcon_nb.notifier_call = sc89890h_charger_extcon_event;
        ret = devm_extcon_register_notifier_all(dev,
                            info->edev,
                            &info->extcon_nb);
        if (ret) {
            dev_err(dev, "Can't register extcon\n");
            return ret;
        }
     }

    regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
    if (!regmap_np)
        regmap_np = of_find_compatible_node(NULL, NULL, "sprd,ump962x-syscon");

    if (of_device_is_compatible(regmap_np->parent, "sprd,sc2730"))
        info->charger_pd_mask = SC89890H_DISABLE_PIN_MASK_2730;
    else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
        info->charger_pd_mask = SC89890H_DISABLE_PIN_MASK_2721;
    else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2720"))
        info->charger_pd_mask = SC89890H_DISABLE_PIN_MASK_2720;
    else {
        dev_err(dev, "failed to get charger_pd mask\n");
        return -EINVAL;
    }

    ret = of_property_read_u32_index(regmap_np, "reg", 1,
                     &info->charger_detect);
    if (ret) {
        dev_err(dev, "failed to get charger_detect\n");
        return -EINVAL;
    }

    ret = of_property_read_u32_index(regmap_np, "reg", 2,
                     &info->charger_pd);
    if (ret) {
        dev_err(dev, "failed to get charger_pd reg\n");
        return ret;
    }

    regmap_pdev = of_find_device_by_node(regmap_np);
    if (!regmap_pdev) {
        of_node_put(regmap_np);
        dev_err(dev, "unable to get syscon device\n");
        return -ENODEV;
    }

    of_node_put(regmap_np);
    info->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
    if (!info->pmic) {
        dev_err(dev, "unable to get pmic regmap device\n");
        return -ENODEV;
    }

    charger_cfg.drv_data = info;
    charger_cfg.of_node = dev->of_node;
    info->psy_usb = devm_power_supply_register(dev,
                           &sc89890h_charger_desc,
                           &charger_cfg);

    mutex_init(&info->lock);
    mutex_lock(&info->lock);

    if (IS_ERR(info->psy_usb)) {
        dev_err(dev, "failed to register power supply\n");
        ret = PTR_ERR(info->psy_usb);
        goto err_mutex_lock;
    }

    do {
        ret = sc89890h_charger_hw_init(info);
        if (ret) {
            dev_err(dev, "try to sc89890h_charger_hw_init fail:%d ,ret = %d\n" ,retry_cnt ,ret);
        }
        retry_cnt--;
    } while ((retry_cnt != 0) && (ret != 0));

    if (ret) {
        dev_err(dev, "failed to sc89890h_charger_hw_init\n");
        goto err_mutex_lock;
    }

    sc89890h_charger_stop_charge(info);

    ret = sc89890h_charger_register_vbus_regulator(info);
    if (ret) {
        dev_err(dev, "failed to register vbus regulator.\n");
        goto err_psy_usb;
    }

    device_init_wakeup(info->dev, true);
    if (!info->use_typec_extcon) {
        info->usb_notify.notifier_call = sc89890h_charger_usb_change;
        ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
        if (ret) {
            dev_err(dev, "failed to register notifier:%d\n", ret);
            goto err_psy_usb;
        }
     }

    ret = sc89890h_register_sysfs(info);
    if (ret) {
        dev_err(info->dev, "register sysfs fail, ret = %d\n", ret);
        goto err_sysfs;
    }

    sc89890h_charger_detect_status(info);

    INIT_DELAYED_WORK(&info->otg_work, sc89890h_charger_otg_work);
    INIT_DELAYED_WORK(&info->wdt_work,sc89890h_charger_feed_watchdog_work);

    pr_info("[%s]line=%d: probe success\n", __FUNCTION__, __LINE__);

    mutex_unlock(&info->lock);
    return 0;

err_sysfs:
    sysfs_remove_group(&info->psy_usb->dev.kobj, &info->sysfs->attr_g);
    usb_unregister_notifier(info->usb_phy, &info->usb_notify);
err_psy_usb:
    power_supply_unregister(info->psy_usb);
err_mutex_lock:
    mutex_unlock(&info->lock);
    mutex_destroy(&info->lock);
    return ret;
}

static void sc89890h_charger_shutdown(struct i2c_client *client)
{
    struct sc89890h_charger_info *info = i2c_get_clientdata(client);
    int ret = 0;

    cancel_delayed_work_sync(&info->wdt_work);
    cancel_delayed_work_sync(&info->cur_work);
    if (info->otg_enable) {
        info->otg_enable = false;
        cancel_delayed_work_sync(&info->otg_work);
        ret = sc89890h_update_bits(info, SC89890H_REG_03,
                      REG03_OTG_CONFIG_MASK,
                      0);
        if (ret)
            dev_err(info->dev, "disable sc89890h otg failed ret = %d\n", ret);

        if (!info->use_typec_extcon) {
            ret = regmap_update_bits(info->pmic, info->charger_detect,
                        BIT_DP_DM_BC_ENB, 0);
            if (ret) {
                dev_err(info->dev,
                    "enable charger detection function failed ret = %d\n", ret);
            }
        }
    }
}

static int sc89890h_charger_remove(struct i2c_client *client)
{
    struct sc89890h_charger_info *info = i2c_get_clientdata(client);

    usb_unregister_notifier(info->usb_phy, &info->usb_notify);

    return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sc89890h_charger_suspend(struct device *dev)
{
    struct sc89890h_charger_info *info = dev_get_drvdata(dev);
    ktime_t now, add;
    unsigned int wakeup_ms = SC89890H_OTG_ALARM_TIMER_MS;
    int ret;

    if (!info->otg_enable)
        return 0;

    cancel_delayed_work_sync(&info->wdt_work);
    cancel_delayed_work_sync(&info->cur_work);

    /* feed watchdog first before suspend */
    ret = sc89890h_update_bits(info, SC89890H_REG_01, REG03_WDT_MASK,
                  WDT_RESET << REG03_WDT_SHIFT);
    if (ret)
        dev_warn(info->dev, "reset sc89890h failed before suspend\n");

    now = ktime_get_boottime();
    add = ktime_set(wakeup_ms / MSEC_PER_SEC,
               (wakeup_ms % MSEC_PER_SEC) * NSEC_PER_MSEC);
    alarm_start(&info->otg_timer, ktime_add(now, add));

    return 0;
}

static int sc89890h_charger_resume(struct device *dev)
{
    struct sc89890h_charger_info *info = dev_get_drvdata(dev);
    int ret;

    if (!info->otg_enable)
        return 0;

    alarm_cancel(&info->otg_timer);

    /* feed watchdog first after resume */
    ret = sc89890h_update_bits(info, SC89890H_REG_03, REG03_WDT_MASK,
                  WDT_RESET << REG03_WDT_SHIFT);
    if (ret)
        dev_warn(info->dev, "reset sc89890h failed after resume\n");

    schedule_delayed_work(&info->wdt_work, HZ * 15);
    schedule_delayed_work(&info->cur_work, 0);

    return 0;
}
#endif

static const struct dev_pm_ops sc89890h_charger_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(sc89890h_charger_suspend,
                sc89890h_charger_resume)
};

static const struct i2c_device_id sc89890h_i2c_id[] = {
    {"sc89890h_chg", 0},
    {}
};

static const struct of_device_id sc89890h_charger_of_match[] = {
    { .compatible = "sc,sc89890h_chg", },
    { }
};

MODULE_DEVICE_TABLE(of, sc89890h_charger_of_match);

static struct i2c_driver sc89890h_charger_driver = {
     .driver = {
         .name = "sc89890h_chg",
        .of_match_table = sc89890h_charger_of_match,
        .pm = &sc89890h_charger_pm_ops,
     },
    .probe = sc89890h_charger_probe,
    .shutdown = sc89890h_charger_shutdown,
    .remove = sc89890h_charger_remove,
    .id_table = sc89890h_i2c_id,
 };

module_i2c_driver(sc89890h_charger_driver);

MODULE_AUTHOR("shixuanxnuan <shixuanxuan@huaqin.com>");
MODULE_DESCRIPTION("SC89890H Charger Driver");
MODULE_LICENSE("GPL");
