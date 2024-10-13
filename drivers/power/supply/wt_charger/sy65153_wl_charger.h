/*
* Copyright © 2020, ConvenientPower
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
*/

#ifndef __SY65153_WL_CHARGER_H__
#define __SY65153_WL_CHARGER_H__


#include <linux/regulator/consumer.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/module.h>

#define SY_WLS_FAIL    -1
#define SY_WLS_SUCCESS 0


/*****************************************************************************
 *  Log
 ****************************************************************************/
#define SY_LOG_NONE    0
#define SY_LOG_ERR     1
#define SY_LOG_DEBG    2
#define SY_LOG_FULL    3

#define ENABLE_SY_LOG SY_LOG_FULL

#define sy_wls_log(num, fmt, args...) \
    do { \
            if (ENABLE_SY_LOG >= (int)num) \
                pr_err(fmt, ##args); \
    } while (0)

/*****************************************************************************
 *  Register
 ****************************************************************************/
#define SY_REG_CHIP_ID	 0x0000

#define SY65153_REG_00	0x0
#define SY65153_REG_01	0x1
#define SY65153_REG_04	0x4
#define SY65153_REG_05	0x5
#define SY65153_REG_06	0x6
#define SY65153_REG_07	0x7

#define SY65153_REG_34	0x34
#define SY65153_LDO_STATUS_MASK			0x80
#define SY65153_LDO_STATUS_SHIFT		7
#define SY65153_HANDSHAKE_PROTOCOL_MASK	 	0x30
#define SY65153_HANDSHAKE_PROTOCOL_SHIFT	4
#define SY65153_OVER_TEMP_MASK				0x04
#define SY65153_OVER_TEMP_SHIFT				3
#define SY65153_OVER_VOLTAGE_MASK			0x02
#define SY65153_OVER_VOLTAGE_SHIFT			2
#define SY65153_OVER_CURRENT_MASK			0x01
#define SY65153_OVER_CURRENT_SHIFT			1

#define SY65153_REG_35	0x35
#define SY65153_REG_3A	0x3A
#define SY65153_REG_3B	0x3B

#define SY65153_REG_3C	0x3C
#define SY65153_VOUT_LOW_BYTE_MASK			0xFF
#define SY65153_VOUT_LOW_BYTE_SHIFT			1

#define SY65153_REG_3D	0x3D
#define SY65153_VOUT_HIGH_BYTE_MASK		0x0F
#define SY65153_VOUT_HIGH_BYTE_SHIFT		1

#define SY65153_REG_3E				0x3E
#define SY65153_VOUT_SET_5V			0x0f
#define SY65153_VOUT_SET_9V			0x37
#define SY65153_VOUT_SET_12V		0x55

#define SY65153_REG_40	0x40
#define SY65153_REG_41	0x41
#define SY65153_REG_44	0x44
#define SY65153_REG_45	0x45
#define SY65153_REG_46	0x46
#define SY65153_REG_47	0x47
#define SY65153_REG_4A	0x4A
#define SY65153_REG_4B	0x4B
#define SY65153_REG_4E	0x4E
#define SY65153_REG_56	0x56

#define SY65153_REG_68	0x68
#define SY65153_RPPO_MASK			0xFF

#define SY65153_REG_69	0x69
#define SY65153_RPPG_MASK			0xFF

#define SY65153_REG_6E	0x6E

#define SY65153_REG_NUM	25

#define SY_ID  24915

#define SY65153_VOUT_MAX			12000
#define SY65153_VOUT_MIN			3500
#define SY65153_VOUT_OFFSET			3500
#define SY65153_VOUT_STEP			100
#define SY65153_VOUT_5V				5000
#define SY65153_VOUT_9V				9000
#define SY65153_VOUT_12V			12000

#define FOD_68_VALUE	111
#define FOD_69_VALUE	160
#define FOD_68_DYNAMIC_VALUE	111
#define FOD_69_DYNAMIC_VALUE	170

#define Q_FACTOR_VALUE		38
#define DISABLE_10W_MODE	0x01

enum sy65153_wireless_type {
	SY65153_VBUS_NONE,
	SY65153_VBUS_WL_BPP,
	SY65153_VBUS_WL_10W_MODE,
	SY65153_VBUS_WL_EPP,
};

/*-------------------------------------------------------------------*/
#define MAX_PROP_SIZE 32			
/* This structure keeps information per regulator */
struct sy65153_msm_reg_data {
	/* voltage regulator handle */
	struct regulator *reg;
	/* regulator name */
	const char *name;
	/* voltage level to be set */
	u32 low_vol_level;
	u32 high_vol_level;
	/* Load values for low power and high power mode */
	u32 lpm_uA;
	u32 hpm_uA;

	/* is this regulator enabled? */
	bool is_enabled;
	/* is this regulator needs to be always on? */
	bool is_always_on;
	/* is low power mode setting required for this regulator? */
	bool lpm_sup;
	bool set_voltage_sup;
};
			
struct sy65153_msm_vreg_data {
	/* keeps VDD/VCC regulator info */
	struct sy65153_msm_reg_data *vdd_data;
};

struct sy65153_wakeup_source {
	struct wakeup_source	*source;
	unsigned long		disabled;
};

struct sy65153_reg_tab {
	int id;
	u32 addr;
	char *name;
};

struct sy_wls_chrg_chip {
    struct i2c_client *client;
    struct device *dev;
    char *name;
    struct power_supply *wl_psy;
    struct power_supply *batt_psy;
    struct power_supply *usb_psy;
    struct power_supply *dc_psy;
    struct power_supply_desc wl_psd;
    struct power_supply_config wl_psc;
    struct power_supply_desc batt_psd;
    struct power_supply_desc usb_psd;
    struct power_supply_desc dc_psd;

	// pinctrl
	struct pinctrl *sy_pinctrl;
	struct pinctrl_state *pinctrl_state_default;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspend;

	int sleep_mode_gpio;
	int irq_gpio;
	int power_good_gpio;

	// iio
	struct iio_dev *indio_dev;
	struct iio_chan_spec *iio_chan;
	struct iio_channel	*int_iio_chans;

	struct iio_channel	**wtchg_ext_iio_chans;

	// lock
    struct wakeup_source *sy_wls_wake_lock;
    struct mutex   irq_lock;
    struct mutex   i2c_rw_lock;
    int state;
    int wls_charge_int;
    int sy_wls_irq;
    struct work_struct wireless_work;
    bool charger_detect;
    bool irq_work_finish;

	struct delayed_work irq_work;
	struct delayed_work monitor_work;

	struct mutex	resume_complete_lock;
	int resume_completed;

	// flag
	int wl_online;
	int pre_wl_online;
	int pre_wl_type;
	int set_vchar_state;

	int wireless_type;
	int wireless_type_pre;

	int pg_retry_cnt;

	int dynamic_fod;

	struct sy65153_wakeup_source	sy65153_wake_source;

	struct sy65153_msm_vreg_data *vreg_data;
};

MODULE_LICENSE("GPL v2");

#endif
