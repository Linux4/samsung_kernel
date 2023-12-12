/* SPDX-License-Identifier: GPL-2.0-only */
// sgm415xx Charger Driver
// Copyright (C) 2021 Texas Instruments Incorporated - http://www.sg-micro.com

#ifndef _SGM415XX_CHARGER_H
#define _SGM415XX_CHARGER_H

#include <linux/i2c.h>

#define SGM415XX_MANUFACTURER    "sgm micro"
//#define __SGM41541_CHIP_ID__
//#define __SGM41542_CHIP_ID__
//#define __SGM41513_CHIP_ID__
//#define __SGM41513A_CHIP_ID__
#define __SGM41513D_CHIP_ID__
//#define __SGM41516_CHIP_ID__
//#define __SGM41516D_CHIP_ID__
//#define __SGM41543_CHIP_ID__
//#define __SGM41543D_CHIP_ID__

#ifdef __SGM41541_CHIP_ID__
#define SGM415XX_NAME        "sgm41541"
#define SGM415XX_PN_ID     (BIT(6)| BIT(5))
#endif

#ifdef __SGM41542_CHIP_ID__
#define SGM415XX_NAME        "sgm41542"
#define SGM415XX_PN_ID      (BIT(6)| BIT(5)| BIT(3))
#endif

#ifdef __SGM41513_CHIP_ID__
#define SGM415XX_NAME        "sgm41513"
#define SGM415XX_PN_ID      0
#endif

#ifdef __SGM41513A_CHIP_ID__
#define SGM415XX_NAME        "sgm41513A"
#define SGM415XX_PN_ID      BIT(3)
#endif

#ifdef __SGM41513D_CHIP_ID__
#define SGM415XX_NAME        "sgm41513D"
#define SGM415XX_PN_ID      BIT(3)
#endif

#ifdef __SGM41516_CHIP_ID__
#define SGM415XX_NAME        "sgm41516"
#define SGM415XX_PN_ID      (BIT(6)| BIT(5))
#endif

#ifdef __SGM41516D_CHIP_ID__
#define SGM415XX_NAME        "sgm41516D"
#define SGM415XX_PN_ID     (BIT(6)| BIT(5)| BIT(3))
#endif

#ifdef __SGM41543_CHIP_ID__
#define SGM415XX_NAME        "sgm41543"
#define SGM415XX_PN_ID      BIT(6)
#endif

#ifdef __SGM41543D_CHIP_ID__
#define SGM415XX_NAME        "sgm41543D"
#define SGM415XX_PN_ID      (BIT(6)| BIT(3))
#endif

/*define register*/
#define SGM415XX_CHRG_CTRL_0    0x00
#define SGM415XX_CHRG_CTRL_1    0x01
#define SGM415XX_CHRG_CTRL_2    0x02
#define SGM415XX_CHRG_CTRL_3    0x03
#define SGM415XX_CHRG_CTRL_4    0x04
#define SGM415XX_CHRG_CTRL_5    0x05
#define SGM415XX_CHRG_CTRL_6    0x06
#define SGM415XX_CHRG_CTRL_7    0x07
#define SGM415XX_CHRG_STAT        0x08
#define SGM415XX_CHRG_FAULT        0x09
#define SGM415XX_CHRG_CTRL_a    0x0a
#define SGM415XX_CHRG_CTRL_b    0x0b
#define SGM415XX_CHRG_CTRL_c    0x0c
#define SGM415XX_CHRG_CTRL_d    0x0d
#define SGM415XX_INPUT_DET       0x0e
#define SGM415XX_CHRG_CTRL_f    0x0f

/* charge status flags  */
#define SGM415XX_CHRG_EN        BIT(4)
#define SGM415XX_HIZ_ENABLE           1
#define SGM415XX_HIZ_DISABLE       0
#define SGM415XX_ENHIZ_SHIFT           7
#define SGM415XX_ENHIZ_MASK            0x80
#define SGM415XX_TERM_EN        BIT(7)
#define SGM415XX_VAC_OVP_MASK    GENMASK(7, 6)
#define SGM415XX_DPDM_ONGOING   BIT(7)
#define SGM415XX_VBUS_GOOD      BIT(7)

#define SGM415XX_BOOSTV         GENMASK(5, 4)
#define SGM415XX_BOOST_LIM         BIT(7)
#define SGM415XX_OTG_EN            BIT(5)

/* Part ID  */
#define SGM415XX_PN_MASK        GENMASK(6, 3)

/* WDT TIMER SET  */
#define SGM415XX_WDT_TIMER_MASK        GENMASK(5, 4)
#define SGM415XX_WDT_TIMER_DISABLE     0
#define SGM415XX_WDT_TIMER_40S         BIT(4)
#define SGM415XX_WDT_TIMER_80S         BIT(5)
#define SGM415XX_WDT_TIMER_160S        (BIT(4)| BIT(5))

#define SGM415XX_WDT_RST_MASK          BIT(6)

/* SAFETY TIMER SET  */
#define SGM415XX_SAFETY_TIMER_MASK     GENMASK(3, 3)
#define SGM415XX_SAFETY_TIMER_DISABLE     0
#define SGM415XX_SAFETY_TIMER_EN       BIT(3)
#define SGM415XX_SAFETY_TIMER_5H         0
#define SGM415XX_SAFETY_TIMER_10H      BIT(2)

/* recharge voltage  */
#define SGM415XX_VRECHARGE              BIT(0)
#define SGM415XX_VRECHRG_STEP_mV        100
#define SGM415XX_VRECHRG_OFFSET_mV        100

/* charge status  */
#define SGM415XX_VSYS_STAT        BIT(0)
#define SGM415XX_THERM_STAT        BIT(1)
#define SGM415XX_PG_STAT        BIT(2)
#define SGM415XX_CHG_STAT_MASK    GENMASK(4, 3)
#define SGM415XX_PRECHRG        BIT(3)
#define SGM415XX_FAST_CHRG        BIT(4)
#define SGM415XX_TERM_CHRG        (BIT(3)| BIT(4))

/* charge type  */
#define SGM415XX_VBUS_STAT_MASK    GENMASK(7, 5)
#define SGM415XX_NOT_CHRGING    0
#define SGM415XX_USB_SDP        BIT(5)
#define SGM415XX_USB_CDP        BIT(6)
#define SGM415XX_USB_DCP        (BIT(5) | BIT(6))
#define SGM415XX_UNKNOWN        (BIT(7) | BIT(5))
#define SGM415XX_NON_STANDARD    (BIT(7) | BIT(6))
#define SGM415XX_OTG_MODE        (BIT(7) | BIT(6) | BIT(5))

/* charge type  detect */
#define SGM415XX_FORCE_DPDM        1
#define SGM415XX_FORCE_DPDM_MASK       GENMASK(4, 0)
#define SGM415XX_FORCE_DPDM_SHIFT      7
#define BC12_DONE_TIMEOUT_CHECK_MAX_RETRY 10

/* HVDCP */
#define SGM415XX_EN_HVDCP_SHIFT        3

/* TEMP Status  */
#define SGM415XX_TEMP_MASK        GENMASK(2, 0)
#define SGM415XX_TEMP_NORMAL    BIT(0)
#define SGM415XX_TEMP_WARM        BIT(1)
#define SGM415XX_TEMP_COOL        (BIT(0) | BIT(1))
#define SGM415XX_TEMP_COLD        (BIT(0) | BIT(3))
#define SGM415XX_TEMP_HOT        (BIT(2) | BIT(3))

/* precharge current  */
#define SGM415XX_PRECHRG_CUR_MASK        GENMASK(7, 4)
#define SGM415XX_PRECHRG_CURRENT_STEP_uA        60000
#define SGM415XX_PRECHRG_I_MIN_uA        60000
#define SGM415XX_PRECHRG_I_MAX_uA        780000
#define SGM415XX_PRECHRG_I_DEF_uA        180000

/* termination current  */
#define SGM415XX_TERMCHRG_CUR_MASK        GENMASK(3, 0)
#define SGM415XX_TERMCHRG_CURRENT_STEP_uA    60000
#define SGM415XX_TERMCHRG_I_MIN_uA        60000
#define SGM415XX_TERMCHRG_I_MAX_uA        960000
#define SGM415XX_TERMCHRG_I_DEF_uA        180000

/* charge current  */
#define SGM415XX_ICHRG_I_MASK        GENMASK(5, 0)

#define SGM415XX_ICHRG_I_MIN_uA            0
#if (defined(__SGM41513_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))
#define SGM415XX_ICHRG_I_MAX_uA            3000000
#define SGM415XX_ICHRG_I_DEF_uA            500000
#else
#define SGM415XX_ICHRG_I_STEP_uA        60000
#define SGM415XX_ICHRG_I_MAX_uA            3780000
#define SGM415XX_ICHRG_I_DEF_uA           1000000
#endif
/* charge voltage  */
#define SGM415XX_VREG_V_MASK        GENMASK(7, 3)
#define SGM415XX_VREG_V_MAX_uV        4624000
#define SGM415XX_VREG_V_MIN_uV        3856000
#define SGM415XX_VREG_V_DEF_uV        4400000
#define SGM415XX_VREG_V_STEP_uV        32000

/* iindpm current  */
#define SGM415XX_IINDPM_I_MASK        GENMASK(4, 0)
#define SGM415XX_IINDPM_I_MIN_uA    100000
#if (defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))
#define SGM415XX_IINDPM_I_MAX_uA    3200000
#else
#define SGM415XX_IINDPM_I_MAX_uA    3800000
#endif
#define SGM415XX_IINDPM_STEP_uA        100000
#define SGM415XX_IINDPM_DEF_uA        1000000

/* vindpm voltage  */
#define SGM415XX_VINDPM_V_MASK      GENMASK(3, 0)
#define SGM415XX_VINDPM_V_MIN_uV    3900000
#define SGM415XX_VINDPM_V_MAX_uV    12000000
#define SGM415XX_VINDPM_STEP_uV     100000
#define SGM415XX_VINDPM_DEF_uV        4600000
#define SGM415XX_VINDPM_OS_MASK     GENMASK(1, 0)

/* DP DM SEL  */
#define SGM415XX_DP_VSEL_MASK       GENMASK(4, 3)
#define SGM415XX_DM_VSEL_MASK       GENMASK(2, 1)

/* PUMPX SET  */
#define SGM415XX_EN_PUMPX           BIT(7)
#define SGM415XX_PUMPX_UP           BIT(6)
#define SGM415XX_PUMPX_DN           BIT(5)

/* INT CONTROL*/
#define SGM415XX_INT_MASK           GENMASK(1, 0)
#define SGM415XX_VINDPM_INT           BIT(1)
#define SGM415XX_IINDPM_INT           BIT(0)

/* shipmode*/
#define SGM415XX_BATFET_DIS_MASK       0x20
#define SGM415XX_BATFET_DIS_SHIFT      5
#define SGM415XX_BATFET_DLY_MASK       0x08
#define SGM415XX_BATFET_DLY_SHIFT      3
#define SGM415XX_BATFET_OFF            1
#define SGM415XX_BATFET_ON             0

#define SGM415XX_BATFET_RST_EN_MASK    0x04
#define SGM415XX_BATFET_RST_EN_SHIFT   2
#define SGM415XX_BATFET_RST_DISABLE    0
#define SGM415XX_BATFET_RST_ENABLE     1

/* RESET*/
#define SGM415XX_REG_RESET_MASK        0x80
#define SGM415XX_REG_RESET_SHIFT       7
#define SGM415XX_REG_RESET             1

/* OVP*/
#define SGM415XX_OVP_MASK              0xC0
#define SGM415XX_OVP_SHIFT             0x6
#define SGM415XX_OVP_5P5V              0
#define SGM415XX_OVP_6P5V              1
#define SGM415XX_OVP_10P5V             2
#define SGM415XX_OVP_14P0V             3

#define I2C_RETRY_CNT       3

#define SGM415XX_ICHG_LIM             500000 //500ma

/* customer define jeita paramter */
#if 0 //temp_note
#define JEITA_TEMP_ABOVE_T4_CV    0
#define JEITA_TEMP_T3_TO_T4_CV    4100000
#define JEITA_TEMP_T2_TO_T3_CV    4350000
#define JEITA_TEMP_T1_TO_T2_CV    4350000
#define JEITA_TEMP_T0_TO_T1_CV    0
#define JEITA_TEMP_BELOW_T0_CV    0

#define JEITA_TEMP_ABOVE_T4_CC_CURRENT    0
#define JEITA_TEMP_T3_TO_T4_CC_CURRENT    1000000
#define JEITA_TEMP_T2_TO_T3_CC_CURRENT    2400000
#define JEITA_TEMP_T1_TO_T2_CC_CURRENT    2000000
#define JEITA_TEMP_T0_TO_T1_CC_CURRENT    0
#define JEITA_TEMP_BELOW_T0_CC_CURRENT    0

#define TEMP_T4_THRES  50
#define TEMP_T4_THRES_MINUS_X_DEGREE 47
#define TEMP_T3_THRES  45
#define TEMP_T3_THRES_MINUS_X_DEGREE 39
#define TEMP_T2_THRES  20
#define TEMP_T2_THRES_PLUS_X_DEGREE 16
#define TEMP_T1_THRES  0
#define TEMP_T1_THRES_PLUS_X_DEGREE 6
#define TEMP_T0_THRES  0
#define TEMP_T0_THRES_PLUS_X_DEGREE  0
#define TEMP_NEG_10_THRES 0
#endif

struct sgm415xx_init_data {
    u32 ichg;    /* charge current        */
    u32 ilim;    /* input current        */
    u32 vreg;    /* regulation voltage        */
    u32 iterm;    /* termination current        */
    u32 iprechg;    /* precharge current        */
    u32 vlim;    /* minimum system voltage limit */
    u32 max_ichg;
    u32 max_vreg;
};

struct sgm415xx_state {
    bool vsys_stat;
    bool therm_stat;
    bool online;
    u8 chrg_stat;
    u8 vbus_status;
    bool chrg_en;
    bool hiz_en;
    bool term_en;
    u8 chrg_type;
    u8 health;
    u8 chrg_fault;
    u8 ntc_fault;
};

struct sgm415xx_jeita {
    int jeita_temp_above_t4_cv;
    int jeita_temp_t3_to_t4_cv;
    int jeita_temp_t2_to_t3_cv;
    int jeita_temp_t1_to_t2_cv;
    int jeita_temp_t0_to_t1_cv;
    int jeita_temp_below_t0_cv;
    int jeita_temp_above_t4_cc_current;
    int jeita_temp_t3_to_t4_cc_current;
    int jeita_temp_t2_to_t3_cc_current;
    int jeita_temp_t1_to_t2_cc_current;
    int jeita_temp_below_t0_cc_current;
    int temp_t4_thres;
    int temp_t4_thres_minus_x_degree;
    int temp_t3_thres;
    int temp_t3_thres_minus_x_degree;
    int temp_t2_thres;
    int temp_t2_thres_plus_x_degree;
    int temp_t1_thres;
    int temp_t1_thres_plus_x_degree;
    int temp_t0_thres;
    int temp_t0_thres_plus_x_degree;
    int temp_neg_10_thres;
};

enum vac_ovp {
    VAC_OVP_5500 = 5500,
    VAC_OVP_6500 = 6500,
    VAC_OVP_10500 = 10500,
    VAC_OVP_14000 = 14000,
};

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

struct sgm415xx_device {
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

    struct sgm415xx_init_data init_data;
    struct sgm415xx_state state;
    u32 watchdog_timer;
    struct charger_device *chg_dev;
    struct regulator_dev *otg_rdev;

    struct delayed_work charge_detect_delayed_work;
    struct delayed_work charge_monitor_work;
    struct notifier_block pm_nb;
    bool sgm415xx_suspend_flag;
    bool power_good;
    bool vbus_gd;
    int psy_usb_type;
    int chg_type;
    struct wakeup_source *charger_wakelock;
    bool enable_sw_jeita;
    struct sgm415xx_jeita data;

    bool chg_config;

    int typec_attached;
    struct delayed_work force_detect_dwork;
    int force_detect_count;

    struct delayed_work prob_dwork;

    struct delayed_work hvdcp_done_work;
    bool hvdcp_done;
};

#endif /* _SGM415XX_CHARGER_H */
