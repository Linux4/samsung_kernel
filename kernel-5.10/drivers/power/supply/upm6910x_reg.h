/* SPDX-License-Identifier: GPL-2.0-only */
// upm6910 Charger Driver
// Copyright (c) 2023 unisemipower.

#ifndef _UPM6910_CHARGER_H
#define _UPM6910_CHARGER_H

#include <linux/i2c.h>

#define UPM6910_MANUFACTURER    "upm micro"
#define __UPM6910DH_CHIP_ID__


#ifdef __UPM6910DH_CHIP_ID__
#define UPM6910_NAME        "upm6910DH"
#define UPM6910_PN_ID      BIT(4)
#endif

/*Tab A9 code for AX6739A-115 by qiaodan at 20230514 start*/
#define UPM6910_IRQ_WAKE_TIME (500) /*ms*/
#define UPM6910_WORK_DELAY_TIME (10) /*ms*/
/*Tab A9 code for AX6739A-115 by qiaodan at 20230514 end*/

/*define register*/
#define UPM6910_CHRG_CTRL_0    0x00
#define UPM6910_CHRG_CTRL_1    0x01
#define UPM6910_CHRG_CTRL_2    0x02
#define UPM6910_CHRG_CTRL_3    0x03
#define UPM6910_CHRG_CTRL_4    0x04
#define UPM6910_CHRG_CTRL_5    0x05
#define UPM6910_CHRG_CTRL_6    0x06
#define UPM6910_CHRG_CTRL_7    0x07
#define UPM6910_CHRG_STAT        0x08
#define UPM6910_CHRG_FAULT        0x09
#define UPM6910_CHRG_CTRL_A    0x0a
#define UPM6910_CHRG_CTRL_B    0x0b
#define UPM6910_CHRG_CTRL_C    0x0c
#define UPM6910_CHRG_CTRL_D    0x0d
/*Tab A9 code for AX6739A-2076 by hualei at 20230707 start*/
#define UPM6910_CHRG_CTRL_A9   0xa9
#define UPM6910_CHRG_CTRL_A7   0xa7
#define UPM6910_CHRG_SPECIAL_MODE   0x6e
/*Tab A9 code for AX6739A-2076 by hualei at 20230707 end*/

/* charge status flags  */
/*Tab A9 code for AX6739A-2076 by hualei at 20230707 start*/
#define UPM6910_IGNOREBC_EN        BIT(6)
/*Tab A9 code for AX6739A-2076 by hualei at 20230707 end*/
#define UPM6910_CHRG_EN        BIT(4)
#define UPM6910_HIZ_EN            BIT(7)
#define UPM6910_TERM_EN        BIT(7)
#define UPM6910_VAC_OVP_MASK    GENMASK(7, 6)
//#define UPM6910_DPDM_ONGOING   BIT(7)
#define UPM6910_VBUS_GOOD      BIT(7)

#define UPM6910_BOOSTV         GENMASK(5, 4)
#define UPM6910_BOOST_LIM         BIT(7)
#define UPM6910_OTG_EN            BIT(5)

/* Part ID  */
#define UPM6910_PN_MASK        GENMASK(6, 3)
/*Tab A9 code for SR-AX6739A-01-490 by qiaodan at 20230504 start*/
#define UPM6910_DEV_REV_MASK        GENMASK(2, 0)
#define UPM6910_DEV_REV_DH        4
/*Tab A9 code for SR-AX6739A-01-490 by qiaodan at 20230504 end*/
/*Tab A9 code for SR-AX6739A-01-491 by qiaodan at 20230508 start*/
#define UPM6910_DEV_REV_DS        1
/*Tab A9 code for SR-AX6739A-01-491 by qiaodan at 20230508 end*/


/* WDT TIMER SET  */
#define UPM6910_WDT_TIMER_MASK        GENMASK(5, 4)
#define UPM6910_WDT_TIMER_DISABLE     0
#define UPM6910_WDT_TIMER_40S         BIT(4)
#define UPM6910_WDT_TIMER_80S         BIT(5)
#define UPM6910_WDT_TIMER_160S        (BIT(4)| BIT(5))

#define UPM6910_WDT_RST_MASK          BIT(6)

/* SAFETY TIMER SET  */
#define UPM6910_SAFETY_TIMER_MASK     GENMASK(3, 3)
#define UPM6910_SAFETY_TIMER_DISABLE     0
#define UPM6910_SAFETY_TIMER_EN       BIT(3)
#define UPM6910_SAFETY_TIMER_5H         0
#define UPM6910_SAFETY_TIMER_10H      BIT(2)

/* recharge voltage  */
#define UPM6910_VRECHARGE              BIT(0)
#define UPM6910_VRECHRG_STEP_mV        100
#define UPM6910_VRECHRG_OFFSET_mV        100

/* charge status  */
#define UPM6910_VSYS_STAT        BIT(0)
#define UPM6910_THERM_STAT        BIT(1)
#define UPM6910_PG_STAT        BIT(2)
#define UPM6910_CHG_STAT_MASK    GENMASK(4, 3)
#define UPM6910_PRECHRG        BIT(3)
#define UPM6910_FAST_CHRG        BIT(4)
#define UPM6910_TERM_CHRG        (BIT(3)| BIT(4))

/* charge type  */
#define UPM6910_VBUS_STAT_MASK    GENMASK(7, 5)
#define UPM6910_NOT_CHRGING    0
#define UPM6910_USB_SDP        BIT(5)
#define UPM6910_USB_CDP        BIT(6)
#define UPM6910_USB_DCP        (BIT(5) | BIT(6))
#define UPM6910_UNKNOWN        (BIT(7) | BIT(5))
#define UPM6910_NON_STANDARD    (BIT(7) | BIT(6))
#define UPM6910_OTG_MODE        (BIT(7) | BIT(6) | BIT(5))

/* charge type  detect */
#define UPM6910_FORCE_DPDM        BIT(7)
//#define BC12_DONE_TIMEOUT_CHECK_MAX_RETRY 10
/* TEMP Status  */
#define UPM6910_TEMP_MASK        GENMASK(2, 0)
#define UPM6910_TEMP_NORMAL    BIT(0)
#define UPM6910_TEMP_WARM        BIT(1)
#define UPM6910_TEMP_COOL        (BIT(0) | BIT(1))
#define UPM6910_TEMP_COLD        (BIT(0) | BIT(3))
#define UPM6910_TEMP_HOT        (BIT(2) | BIT(3))

/* precharge current  */
#define UPM6910_PRECHRG_CUR_MASK        GENMASK(7, 4)
#define UPM6910_PRECHRG_CUR_SHIFT        4
#define UPM6910_PRECHRG_CURRENT_STEP_uA        60000
#define UPM6910_PRECHRG_I_MIN_uA        60000
#define UPM6910_PRECHRG_I_MAX_uA        780000
#define UPM6910_PRECHRG_I_DEF_uA        180000

/* termination current  */
#define UPM6910_TERMCHRG_CUR_MASK        GENMASK(3, 0)
#define UPM6910_TERMCHRG_CURRENT_STEP_uA    60000
#define UPM6910_TERMCHRG_I_MIN_uA        60000
#define UPM6910_TERMCHRG_I_MAX_uA        960000
#define UPM6910_TERMCHRG_I_DEF_uA        180000

/* charge current  */
#define UPM6910_ICHRG_I_MASK        GENMASK(5, 0)

#define UPM6910_ICHRG_I_MIN_uA            0


#define UPM6910_ICHRG_I_STEP_uA        60000
#define UPM6910_ICHRG_I_MAX_uA            3780000
#define UPM6910_ICHRG_I_DEF_uA           1000000
/*Tab A9 code for AX6739A-1460 by hualei at 20230625 start*/
#define UPM6910_ICHRG_I_LIMIT            500000
/*Tab A9 code for AX6739A-1460 by hualei at 20230625 end*/

/* charge voltage  */
#define UPM6910_VREG_V_MASK        GENMASK(7, 3)
#define UPM6910_VREG_V_SHIFT        3
#define UPM6910_VREG_V_MAX_uV        4624000
#define UPM6910_VREG_V_MIN_uV        3832000
#define UPM6910_VREG_V_DEF_uV        4400000
#define UPM6910_VREG_V_STEP_uV        32000
#define UPM6910_SPECIAL_CHG_VOL_4200       4200000
#define UPM6910_SPECIAL_CHG_VOL_4200_VAL   0xB
#define UPM6910_SPECIAL_CHG_VOL_4290       4290000
#define UPM6910_SPECIAL_CHG_VOL_4290_VAL   0xE
#define UPM6910_SPECIAL_CHG_VOL_4340       4340000
#define UPM6910_SPECIAL_CHG_VOL_4340_VAL   0xF
#define UPM6910_SPECIAL_CHG_VOL_4360       4360000
#define UPM6910_SPECIAL_CHG_VOL_4360_VAL   0x10
#define UPM6910_SPECIAL_CHG_VOL_4380       4380000
#define UPM6910_SPECIAL_CHG_VOL_4380_VAL   0x11
#define UPM6910_SPECIAL_CHG_VOL_4400       4400000
#define UPM6910_SPECIAL_CHG_VOL_4400_VAL   0x12


/* iindpm current  */
#define UPM6910_IINDPM_I_MASK        GENMASK(4, 0)
#define UPM6910_IINDPM_I_MIN_uA    100000
#define UPM6910_IINDPM_I_MAX_uA    3200000
#define UPM6910_IINDPM_STEP_uA        100000
#define UPM6910_IINDPM_DEF_uA        1000000

/* vindpm voltage  */
#define UPM6910_VINDPM_V_MASK      GENMASK(3, 0)
#define UPM6910_VINDPM_V_MIN_uV    3900000
#define UPM6910_VINDPM_V_MAX_uV    5400000
#define UPM6910_VINDPM_STEP_uV     100000
#define UPM6910_VINDPM_DEF_uV        4600000

/* DP DM SEL  */
#define UPM6910_DP_VSEL_MASK       GENMASK(4, 3)
#define UPM6910_DM_VSEL_MASK       GENMASK(2, 1)


/* INT CONTROL*/
#define UPM6910_INT_MASK           GENMASK(1, 0)
#define UPM6910_VINDPM_INT           BIT(1)
#define UPM6910_IINDPM_INT           BIT(0)

/*Tab A9 code for SR-AX6739A-01-500 by wenyaqi at 20230515 start*/
/* HVDCP SEL */
#define UPM6910_EN_HVDCP_MASK        0x80
#define UPM6910_EN_HVDCP_SHIFT       7
#define UPM6910_HVDCP_ENABLE         1
#define UPM6910_HVDCP_DISABLE        0

#define UPM6910_DP_MUX_MASK          0x18
#define UPM6910_DP_MUX_SHIFT         3

#define UPM6910_DM_MUX_MASK          0x06
#define UPM6910_DM_MUX_SHIFT         1

#define UPM6910_DPDM_OUT_HIZ         0
#define UPM6910_DPDM_OUT_0V          1
#define UPM6910_DPDM_OUT_0P6V        2
#define UPM6910_DPDM_OUT_3P3V        3
/*Tab A9 code for SR-AX6739A-01-500 by wenyaqi at 20230515 end*/

#define    REG06_OVP_MASK                0xC0
#define    REG06_OVP_SHIFT                0x6
#define    REG06_OVP_5P5V                0
#define    REG06_OVP_6P5V                1
#define    REG06_OVP_10P5V                2
#define    REG06_OVP_14P0V                3

/*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 start*/
#define REG07_BATFET_DIS_MASK         0x20
#define REG07_BATFET_DIS_SHIFT        5
#define REG07_BATFET_OFF              1
#define REG07_BATFET_ON               0

#define REG07_BATFET_RST_EN_MASK      0x04
#define REG07_BATFET_RST_EN_SHIFT     2
#define REG07_BATFET_RST_DISABLE      0
#define REG07_BATFET_RST_ENABLE       1
/*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 end*/

enum vac_ovp {
    VAC_OVP_5500 = 5500,
    VAC_OVP_6500 = 6500,
    VAC_OVP_10500 = 10500,
    VAC_OVP_14000 = 14000,
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

struct upm6910x_init_data {
    u32 ichg;    /* charge current        */
    u32 ilim;    /* input current        */
    u32 vreg;    /* regulation voltage        */
    u32 iterm;    /* termination current        */
    u32 iprechg;    /* precharge current        */
    u32 vlim;    /* minimum system voltage limit */
    u32 max_ichg;
    u32 max_vreg;
};

struct upm6910x_state {
    bool therm_stat;
    bool online;
    u8 chrg_stat;
    u8 vbus_status;
    u8 vbus_attach;
    bool chrg_en;
    bool hiz_en;
    bool term_en;
    u8 chrg_type;
    u8 health;
    u8 chrg_fault;
    u8 ntc_fault;
};

;

struct upm6910x_device {
    struct i2c_client *client;
    struct device *dev;
    struct power_supply_desc psy_desc;
    struct power_supply *charger;
    struct power_supply *usb;
    struct power_supply *ac;
    struct mutex lock;
    struct mutex i2c_rw_lock;

    struct usb_phy *usb2_phy;
    struct usb_phy *usb3_phy;
    struct notifier_block usb_nb;
    struct work_struct usb_work;
    unsigned long usb_event;
    struct regmap *regmap;

    char model_name[I2C_NAME_SIZE];
    int device_id;

    struct upm6910x_init_data init_data;
    struct upm6910x_state state;
    u32 watchdog_timer;
    #if 1//defined(CONFIG_MTK_GAUGE_VERSION) && (CONFIG_MTK_GAUGE_VERSION == 30)
    struct charger_device *chg_dev;
    struct regulator_dev *otg_rdev;
    #endif

    struct delayed_work charge_detect_delayed_work;
    struct delayed_work charge_detect_recheck_delay_work;
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
    struct delayed_work hvdcp_done_work;
    bool hvdcp_done;
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/
    struct notifier_block pm_nb;
    bool upm6910x_suspend_flag;
    bool power_good;
    /*Tab A9 code for SR-AX6739A-01-494 by qiaodan at 20230506 start*/
    int psy_type;
    bool chg_config;
    /*Tab A9 code for SR-AX6739A-01-494 by qiaodan at 20230506 end*/
    int psy_usb_type;
    bool recheck_flag;
    struct wakeup_source *charger_wakelock;
    bool enable_sw_jeita;
    /*Tab A9 code for AX6739A-2076 by hualei at 20230707 start*/
    /*Tab A9 code for AX6739A-516 by wenyaqi at 20230603 start*/
    int typec_attached;
    /*Tab A9 code for AX6739A-516 by wenyaqi at 20230603 end*/
    struct delayed_work force_detect_dwork;
    /*Tab A9 code for AX6739A-2076 by hualei at 20230707 end*/
    /*Tab A9 code for AX6739A-2299 by hualei at 20230713 start*/
    u32 pre_limit;
    /*Tab A9 code for AX6739A-2299 by hualei at 20230713 end*/
    /*Tab A9 code for AX6739A-2344 by hualei at 20230717 start*/
    bool force_dpdm;
    /*Tab A9 code for AX6739A-2344 by hualei at 20230717 end*/
};

#endif /* _UPM6910_CHARGER_H */