/*
 * Driver for the TI sgm41511 charger.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/alarmtimer.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/sysfs.h>
#include <linux/usb/phy.h>
#include <linux/pm_wakeup.h>
#include <uapi/linux/usb/charger.h>
/* HS03 code for SR-SL6215-01-181 by gaochao at 20210719 start */
#include "battery_id_via_adc.h"
/* HS03 code for SR-SL6215-01-181 by gaochao at 20210719 end */

#define SGM41511_REG_0				0x0
#define SGM41511_REG_1				0x1
#define SGM41511_REG_2				0x2
#define SGM41511_REG_3				0x3
#define SGM41511_REG_4				0x4
#define SGM41511_REG_5				0x5
#define SGM41511_REG_6				0x6
#define SGM41511_REG_7				0x7
#define SGM41511_REG_8				0x8
#define SGM41511_REG_9				0x9
#define SGM41511_REG_A				0xa
#define SGM41511_REG_B				0xb
#define SGM41511_REG_NUM				12

#define SGM41511_BATTERY_NAME			"sc27xx-fgu"
#define BIT_DP_DM_BC_ENB			BIT(0)
#define SGM41511_OTG_ALARM_TIMER_MS		15000

#define	SGM41511_REG_IINLIM_BASE			100

#define SGM41511_REG_ICHG_LSB			60

#define SGM41511_REG_ICHG_MASK			GENMASK(5, 0)

/* HS03 code for SL6215DEV-734 by shixuanxuan at 20210906 start */
#define SGM41511_REG_EN_TIMER_MASK		GENMASK(3, 3)
#define SGM41511_REG_EN_TIMER_SHIFT		3
/* HS03 code for SL6215DEV-734 by shixuanxuan at 20210906 end */

#define SGM41511_REG_CHG_MASK			GENMASK(4, 4)
#define SGM41511_REG_CHG_SHIFT			4


#define SGM41511_REG_RESET_MASK			GENMASK(6, 6)

#define SGM41511_REG_OTG_MASK			GENMASK(5, 5)
#define SGM41511_REG_BOOST_FAULT_MASK		GENMASK(6, 6)

#define SGM41511_REG_WATCHDOG_MASK		GENMASK(6, 6)

#define SGM41511_REG_WATCHDOG_TIMER_MASK		GENMASK(5, 4)
#define SGM41511_REG_WATCHDOG_TIMER_SHIFT	4

#define SGM41511_REG_TERMINAL_VOLTAGE_MASK	GENMASK(7, 3)
#define SGM41511_REG_TERMINAL_VOLTAGE_SHIFT	3

#define SGM41511_REG_TERMINAL_CUR_MASK		GENMASK(3, 0)

#define SGM41511_REG_VINDPM_VOLTAGE_MASK		GENMASK(3, 0)
#define SGM41511_REG_OVP_MASK			GENMASK(7, 6)
#define SGM41511_REG_OVP_SHIFT			6

#define SGM41511_REG_EN_HIZ_MASK			GENMASK(7, 7)
#define SGM41511_REG_EN_HIZ_SHIFT		7

#define SGM41511_REG_LIMIT_CURRENT_MASK		GENMASK(4, 0)

/* HS03 code for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 start */
#define SGM41511_REG_VENDOR_ID_MASK		GENMASK(6, 3)
#define SGM41511_REG_VENDOR_ID_SHIFT		3
#define SGM41511_VENDOR_ID			0x2

#define SGM41511_REG_PART_MASK			GENMASK(2, 2)
#define SGM41511_REG_PART_SHIFT			2
#define SGM41511_PART_VALUE				1
/* HS03 code for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 end */

#define SGM41511_DISABLE_PIN_MASK		BIT(0)
#define SGM41511_DISABLE_PIN_MASK_2721		BIT(15)

#define SGM41511_OTG_VALID_MS			500
#define SGM41511_FEED_WATCHDOG_VALID_MS		50
#define SGM41511_OTG_RETRY_TIMES			10
#define SGM41511_LIMIT_CURRENT_MAX		3200000
#define SGM41511_LIMIT_CURRENT_OFFSET		100000
#define SGM41511_REG_IINDPM_LSB			100

#define SGM41511_ROLE_MASTER_DEFAULT		1
#define SGM41511_ROLE_SLAVE			2

#define SGM41511_FCHG_OVP_6V			6000
#define SGM41511_FCHG_OVP_9V			9000
#define SGM41511_FCHG_OVP_14V			14000
#define SGM41511_FAST_CHARGER_VOLTAGE_MAX	10500000
#define SGM41511_NORMAL_CHARGER_VOLTAGE_MAX	6500000

/* HS03 code for SR-SL6215-01-606 by gaochao at 20210813 start */
#define SGM41511_REG_CHARGE_DONE_MASK		GENMASK(4, 3)
#define SGM41511_REG_CHARGE_DONE_SHIFT		3
#define SGM41511_CHARGE_DONE				0x3

#define SGM41511_REG_RECHG_MASK			GENMASK(0, 0)
#define SGM41511_REG_RECHG_SHIFT			0
/* HS03 code for SR-SL6215-01-606 by gaochao at 20210813 end */
#define SGM41511_WAKE_UP_MS			1000
#define SGM41511_CURRENT_WORK_MS			msecs_to_jiffies(100)

struct sgm41511_charger_sysfs {
	char *name;
	struct attribute_group attr_g;
	struct device_attribute attr_sgm41511_dump_reg;
	struct device_attribute attr_sgm41511_lookup_reg;
	struct device_attribute attr_sgm41511_sel_reg_id;
	struct device_attribute attr_sgm41511_reg_val;
	struct attribute *attrs[5];

	struct sgm41511_charger_info *info;
};

struct sgm41511_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	struct power_supply *psy_usb;
	struct power_supply_charge_current cur;
	struct work_struct work;
	struct mutex lock;
	struct delayed_work otg_work;
	struct delayed_work wdt_work;
	struct delayed_work cur_work;
	struct regmap *pmic;
	struct gpio_desc *gpiod;
	struct extcon_dev *edev;
	struct alarm otg_timer;
	struct sgm41511_charger_sysfs *sysfs;
	u32 charger_detect;
	u32 charger_pd;
	u32 charger_pd_mask;
	u32 limit;
	u32 new_charge_limit_cur;
	u32 current_charge_limit_cur;
	u32 new_input_limit_cur;
	u32 current_input_limit_cur;
	u32 last_limit_cur;
	u32 actual_limit_cur;
	/* HS03 code for SL6215DEV-3879 by Ditong at 20211221 start */
	u32 actual_limit_voltage;
	/* HS03 code for SL6215DEV-3879 by Ditong at 20211221 end */
	u32 role;
	bool charging;
	bool need_disable_Q1;
	int termination_cur;
	bool otg_enable;
	unsigned int irq_gpio;
	bool is_wireless_charge;

	int reg_id;
	bool disable_power_path;
};

struct sgm41511_charger_reg_tab {
	int id;
	u32 addr;
	char *name;
};

static struct sgm41511_charger_reg_tab reg_tab[SGM41511_REG_NUM + 1] = {
	{0, SGM41511_REG_0, "EN_HIZ/EN_ICHG_MON/IINDPM"},
	{1, SGM41511_REG_1, "PFM _DIS/WD_RST/OTG_CONFIG/CHG_CONFIG/SYS_Min/Min_VBAT_SEL"},
	{2, SGM41511_REG_2, "BOOST_LIM/Q1_FULLON/ICHG"},
	{3, SGM41511_REG_3, "IPRECHG/ITERM"},
	{4, SGM41511_REG_4, "VREG/TOPOFF_TIMER/VRECHG"},
	{5, SGM41511_REG_5, "EN_TERM/WATCHDOG/EN_TIMER/CHG_TIMER/TREG/JEITA_ISET"},
	{6, SGM41511_REG_6, "OVP/BOOSTV/VINDPM"},
	{7, SGM41511_REG_7, "IINDET_EN/TMR2X_EN/BATFET_DIS/JEITA_VSET/BATFET_DLY/"
				"BATFET_RST_EN/VDPM_BAT_TRACK"},
	{8, SGM41511_REG_8, "VBUS_STAT/CHRG_STAT/PG_STAT/THERM_STAT/VSYS_STAT"},
	{9, SGM41511_REG_9, "WATCHDOG_FAULT/BOOST_FAULT/CHRG_FAULT/BAT_FAULT/NTC_FAULT"},
	{10, SGM41511_REG_A, "VBUS_GD/VINDPM_STAT/IINDPM_STAT/TOPOFF_ACTIVE/ACOV_STAT/"
				"VINDPM_INT_ MASK/IINDPM_INT_ MASK"},
	{11, SGM41511_REG_B, "REG_RST/PN/DEV_REV"},
	{12, 0, "null"},
};

static void power_path_control(struct sgm41511_charger_info *info)
{
	extern char *saved_command_line;
	char result[5];
	char *match = strstr(saved_command_line, "androidboot.mode=");

	if (match) {
		memcpy(result, (match + strlen("androidboot.mode=")),
		       sizeof(result) - 1);
		if ((!strcmp(result, "cali")) || (!strcmp(result, "auto")))
			info->disable_power_path = true;
	}
}

static int
sgm41511_charger_set_limit_current(struct sgm41511_charger_info *info,
				  u32 limit_cur);

static bool sgm41511_charger_is_bat_present(struct sgm41511_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool present = false;
	int ret;

	psy = power_supply_get_by_name(SGM41511_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
		return present;
	}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
					&val);
	if (ret == 0 && val.intval)
		present = true;
	power_supply_put(psy);

	if (ret)
		dev_err(info->dev,
			"Failed to get property of present:%d\n", ret);

	return present;
}

static int sgm41511_charger_is_fgu_present(struct sgm41511_charger_info *info)
{
	struct power_supply *psy;

	psy = power_supply_get_by_name(SGM41511_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to find psy of sc27xx_fgu\n");
		return -ENODEV;
	}
	power_supply_put(psy);

	return 0;
}

static int sgm41511_read(struct sgm41511_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int sgm41511_write(struct sgm41511_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int sgm41511_update_bits(struct sgm41511_charger_info *info, u8 reg,
			       u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = sgm41511_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return sgm41511_write(info, reg, v);
}

/* HS03 code for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 start */
static int sgm41511_charger_get_vendor_id_part_value(struct sgm41511_charger_info *info)
{
	u8 reg_val;
	u8 reg_part_val;
	int ret;

	ret = sgm41511_read(info, SGM41511_REG_B, &reg_val);
	if (ret < 0) {
		dev_err(info->dev, "Failed to get vendor id, ret = %d\n", ret);
		return ret;
	}
	reg_part_val = reg_val;

	reg_val &= SGM41511_REG_VENDOR_ID_MASK;
	reg_val >>= SGM41511_REG_VENDOR_ID_SHIFT;
	if (reg_val != SGM41511_VENDOR_ID) {
		dev_err(info->dev, "The vendor id is 0x%x\n", reg_val);
		return -EINVAL;
	}

	reg_part_val &= SGM41511_REG_PART_MASK;
	reg_part_val >>= SGM41511_REG_PART_SHIFT;
	if (reg_part_val != SGM41511_PART_VALUE) {
		dev_err(info->dev, "The part value is 0x%x\n", reg_part_val);
		return -EINVAL;
	}

	return 0;
}
/* HS03 code for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 end */

static int
sgm41511_charger_set_vindpm(struct sgm41511_charger_info *info, u32 vol)
{
	u8 reg_val;

	if (vol < 3900)
		reg_val = 0x0;
	else if (vol > 5400)
		reg_val = 0x0f;
	else
		reg_val = (vol - 3900) / 100;

	return sgm41511_update_bits(info, SGM41511_REG_6,
				   SGM41511_REG_VINDPM_VOLTAGE_MASK, reg_val);
}

static int
sgm41511_charger_set_ovp(struct sgm41511_charger_info *info, u32 vol)
{
	u8 reg_val;

	if (vol < 5500)
		reg_val = 0x0;
	else if (vol > 5500 && vol < 6500)
		reg_val = 0x01;
	else if (vol > 6500 && vol < 10500)
		reg_val = 0x02;
	else
		reg_val = 0x03;

	return sgm41511_update_bits(info, SGM41511_REG_6,
				   SGM41511_REG_OVP_MASK,
				   reg_val << SGM41511_REG_OVP_SHIFT);
}

/* HS03 code for SL6215DEV-3879 by Ditong at 20211221 start */
static int
sgm41511_charger_set_termina_vol(struct sgm41511_charger_info *info, u32 vol)
{
	u8 reg_val;
	int ret;

	if (vol < 3500)
		reg_val = 0x0;
	else if (vol >= 4440)
		reg_val = 0x2e;
	else
		reg_val = (vol - 3856) / 32;

	ret = sgm41511_update_bits(info, SGM41511_REG_4,
				   SGM41511_REG_TERMINAL_VOLTAGE_MASK,
				   reg_val << SGM41511_REG_TERMINAL_VOLTAGE_SHIFT);

	if (ret != 0) {
		dev_err(info->dev, "sgm41511 set terminal voltage failed\n");
	} else {
		info->actual_limit_voltage = (reg_val * 32) + 3856;
		dev_err(info->dev, "sgm41511 set terminal voltage success, the value is %d\n" ,info->actual_limit_voltage);
	}

	return ret;
}
/* HS03 code for SL6215DEV-3879 by Ditong at 20211221 end */

static int
sgm41511_charger_set_termina_cur(struct sgm41511_charger_info *info, u32 cur)
{
	u8 reg_val;

	if (cur <= 60)
		reg_val = 0x0;
	else if (cur >= 480)
		reg_val = 0x8;
	else
		reg_val = (cur - 60) / 60;

	return sgm41511_update_bits(info, SGM41511_REG_3,
				   SGM41511_REG_TERMINAL_CUR_MASK,
				   reg_val);
}

/* HS03 code for SR-SL6215-01-606 by gaochao at 20210813 start */
static int sgm41511_charger_set_recharge(struct sgm41511_charger_info *info)
{
	int ret = 0;

	ret = sgm41511_update_bits(info, SGM41511_REG_4,
				  SGM41511_REG_RECHG_MASK,
				  0x1 << SGM41511_REG_RECHG_SHIFT);
	if (ret) {
		dev_err(info->dev, "set sgm41511 recharge failed\n");
		return ret;
	}

	return ret;
}
/* HS03 code for SR-SL6215-01-606 by gaochao at 20210813 end */

/* HS03 code for SL6215DEV-734 by shixuanxuan at 20210906 start */
static int sgm41511_charger_en_chg_timer(struct sgm41511_charger_info *info, bool val)
{
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (val) {
		ret = sgm41511_update_bits(info, SGM41511_REG_5,
				SGM41511_REG_EN_TIMER_MASK,
				0x1 << SGM41511_REG_EN_TIMER_SHIFT);
		pr_info("SGM41511 EN_TIMER is enabled\n");
	} else {
		ret = sgm41511_update_bits(info, SGM41511_REG_5,
				SGM41511_REG_EN_TIMER_MASK,
				0x0 << SGM41511_REG_EN_TIMER_SHIFT);
		pr_info("SGM41511 EN_TIMER is disabled\n");
	}

	if (ret) {
		pr_err("%s: disable SGM41511 chg_timer failed\n", __func__);
	}

	return ret;
}
/* HS03 code for SL6215DEV-734 by shixuanxuan at 20210906 end */

static int sgm41511_charger_hw_init(struct sgm41511_charger_info *info)
{
	struct power_supply_battery_info bat_info = { };
	int voltage_max_microvolt, termination_cur;
	int ret;
	/* HS03 code for SR-SL6215-01-181 by gaochao at 20210719 start */
	int bat_id = 0;

	bat_id = battery_get_bat_id();
	ret = power_supply_get_battery_info(info->psy_usb, &bat_info, bat_id);
	// ret = power_supply_get_battery_info(info->psy_usb, &bat_info, 0);
	/* HS03 code for SR-SL6215-01-181 by gaochao at 20210719 end */
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

		voltage_max_microvolt =
			bat_info.constant_charge_voltage_max_uv / 1000;
		termination_cur = bat_info.charge_term_current_ua / 1000;
		info->termination_cur = termination_cur;
		power_supply_put_battery_info(info->psy_usb, &bat_info);

		ret = sgm41511_update_bits(info, SGM41511_REG_B,
					  SGM41511_REG_RESET_MASK,
					  SGM41511_REG_RESET_MASK);
		if (ret) {
			dev_err(info->dev, "reset sgm41511 failed\n");
			return ret;
		}

		if (info->role == SGM41511_ROLE_MASTER_DEFAULT) {
			ret = sgm41511_charger_set_ovp(info, SGM41511_FCHG_OVP_6V);
			if (ret) {
				dev_err(info->dev, "set sgm41511 ovp failed\n");
				return ret;
			}
		} else if (info->role == SGM41511_ROLE_SLAVE) {
			ret = sgm41511_charger_set_ovp(info, SGM41511_FCHG_OVP_9V);
			if (ret) {
				dev_err(info->dev, "set sgm41511 slave ovp failed\n");
				return ret;
			}
		}

		ret = sgm41511_charger_set_vindpm(info, voltage_max_microvolt);
		if (ret) {
			dev_err(info->dev, "set sgm41511 vindpm vol failed\n");
			return ret;
		}

		ret = sgm41511_charger_set_termina_vol(info,
						      voltage_max_microvolt);
		if (ret) {
			dev_err(info->dev, "set sgm41511 terminal vol failed\n");
			return ret;
		}

		ret = sgm41511_charger_set_termina_cur(info, termination_cur);
		if (ret) {
			dev_err(info->dev, "set sgm41511 terminal cur failed\n");
			return ret;
		}

		ret = sgm41511_charger_set_limit_current(info,
							info->cur.unknown_cur);
		if (ret)
			dev_err(info->dev, "set sgm41511 limit current failed\n");

		/* HS03 code for SR-SL6215-01-606 by gaochao at 20210813 start */
		sgm41511_charger_set_recharge(info);
		/* HS03 code for SR-SL6215-01-606 by gaochao at 20210813 end */
		/* HS03 code for SL6215DEV-734 by shixuanxuan at 20210906 start */
		ret = sgm41511_charger_en_chg_timer(info, false);
		if (ret)
			pr_err("failed to disable chg_timer \n");
		/* HS03 code for SL6215DEV-734 by shixuanxuan at 20210906 end */
	}

	info->current_charge_limit_cur = SGM41511_REG_ICHG_LSB * 1000;
	info->current_input_limit_cur = SGM41511_REG_IINDPM_LSB * 1000;

	return ret;
}

/* HS03 code for SL6215DEV-28 by qiaodan at 20210805 start */
static int sgm41511_enter_hiz_mode(struct sgm41511_charger_info *info)
{
	int ret;

	ret = sgm41511_update_bits(info, SGM41511_REG_0,
				  SGM41511_REG_EN_HIZ_MASK, 0x1 << SGM41511_REG_EN_HIZ_SHIFT);
	if (ret)
		dev_err(info->dev, "enter HIZ mode failed\n");

	return ret;
}

static int sgm41511_exit_hiz_mode(struct sgm41511_charger_info *info)
{
	int ret;

	ret = sgm41511_update_bits(info, SGM41511_REG_0,
				  SGM41511_REG_EN_HIZ_MASK, 0);
	if (ret)
		dev_err(info->dev, "exit HIZ mode failed\n");

	return ret;
}

static int sgm41511_get_hiz_mode(struct sgm41511_charger_info *info,u32 *value)
{
	u8 buf;
	int ret;

	ret = sgm41511_read(info, SGM41511_REG_0, &buf);
	*value = (buf & SGM41511_REG_EN_HIZ_MASK) >> SGM41511_REG_EN_HIZ_SHIFT;

	return ret;
}
/* HS03 code for SL6215DEV-28 by qiaodan at 20210805 end */
static int
sgm41511_charger_get_charge_voltage(struct sgm41511_charger_info *info,
				   u32 *charge_vol)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(SGM41511_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "failed to get SGM41511_BATTERY_NAME\n");
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

static int sgm41511_charger_start_charge(struct sgm41511_charger_info *info)
{
	int ret = 0;

	ret = sgm41511_update_bits(info, SGM41511_REG_0,
				  SGM41511_REG_EN_HIZ_MASK, 0);
	if (ret)
		dev_err(info->dev, "disable HIZ mode failed\n");

	ret = sgm41511_update_bits(info, SGM41511_REG_5,
				 SGM41511_REG_WATCHDOG_TIMER_MASK,
				 0x01 << SGM41511_REG_WATCHDOG_TIMER_SHIFT);
	if (ret) {
		dev_err(info->dev, "Failed to enable sgm41511 watchdog\n");
		return ret;
	}

	if (info->role == SGM41511_ROLE_MASTER_DEFAULT) {
		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask, 0);
		if (ret) {
			dev_err(info->dev, "enable sgm41511 charge failed\n");
			return ret;
		}

		ret = sgm41511_update_bits(info, SGM41511_REG_1,
					  SGM41511_REG_CHG_MASK,
					  0x1 << SGM41511_REG_CHG_SHIFT);
		if (ret) {
			dev_err(info->dev, "enable sgm41511 charge en failed\n");
			return ret;
		}
	} else if (info->role == SGM41511_ROLE_SLAVE) {
		gpiod_set_value_cansleep(info->gpiod, 0);
	}

	ret = sgm41511_charger_set_limit_current(info,
						info->last_limit_cur);
	if (ret) {
		dev_err(info->dev, "failed to set limit current\n");
		return ret;
	}

	ret = sgm41511_charger_set_termina_cur(info, info->termination_cur);
	if (ret)
		dev_err(info->dev, "set sgm41511 terminal cur failed\n");

	return ret;
}

static void sgm41511_charger_stop_charge(struct sgm41511_charger_info *info)
{
	int ret;
	bool present = sgm41511_charger_is_bat_present(info);

	if (info->role == SGM41511_ROLE_MASTER_DEFAULT) {
		if (!present || info->need_disable_Q1) {
			ret = sgm41511_update_bits(info, SGM41511_REG_0,
						  SGM41511_REG_EN_HIZ_MASK,
						  0x01 << SGM41511_REG_EN_HIZ_SHIFT);
			if (ret)
				dev_err(info->dev, "enable HIZ mode failed\n");

			info->need_disable_Q1 = false;
		}

		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask,
					 info->charger_pd_mask);
		if (ret)
			dev_err(info->dev, "disable sgm41511 charge failed\n");

		if (info->is_wireless_charge) {
			ret = sgm41511_update_bits(info, SGM41511_REG_1,
						SGM41511_REG_CHG_MASK,
						0x0);
			if (ret)
				dev_err(info->dev, "disable sgm41511 charge en failed\n");
		}
	} else if (info->role == SGM41511_ROLE_SLAVE) {
		ret = sgm41511_update_bits(info, SGM41511_REG_0,
					  SGM41511_REG_EN_HIZ_MASK,
					  0x01 << SGM41511_REG_EN_HIZ_SHIFT);
		if (ret)
			dev_err(info->dev, "enable HIZ mode failed\n");

		gpiod_set_value_cansleep(info->gpiod, 1);
	}

	if (info->disable_power_path) {
		ret = sgm41511_update_bits(info, SGM41511_REG_0,
					  SGM41511_REG_EN_HIZ_MASK,
					  0x01 << SGM41511_REG_EN_HIZ_SHIFT);
		if (ret)
			dev_err(info->dev, "Failed to disable power path\n");
	}

	ret = sgm41511_update_bits(info, SGM41511_REG_5,
                                 SGM41511_REG_WATCHDOG_TIMER_MASK, 0);
	if (ret)
		dev_err(info->dev, "Failed to disable sgm41511 watchdog\n");
}

static int sgm41511_charger_set_current(struct sgm41511_charger_info *info,
				       u32 cur)
{
	u8 reg_val;

	cur = cur / 1000;
	if (cur > 3000) {
		reg_val = 0x32;
	} else {
		reg_val = cur / SGM41511_REG_ICHG_LSB;
		reg_val &= SGM41511_REG_ICHG_MASK;
	}

	return sgm41511_update_bits(info, SGM41511_REG_2,
				   SGM41511_REG_ICHG_MASK,
				   reg_val);
}

static int sgm41511_charger_get_current(struct sgm41511_charger_info *info,
				       u32 *cur)
{
	u8 reg_val;
	int ret;

	ret = sgm41511_read(info, SGM41511_REG_2, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= SGM41511_REG_ICHG_MASK;
	*cur = reg_val * SGM41511_REG_ICHG_LSB * 1000;

	return 0;
}

static int
sgm41511_charger_set_limit_current(struct sgm41511_charger_info *info,
				  u32 limit_cur)
{
	u8 reg_val;
	int ret;

	if (limit_cur >= SGM41511_LIMIT_CURRENT_MAX)
		limit_cur = SGM41511_LIMIT_CURRENT_MAX;

	info->last_limit_cur = limit_cur;
	limit_cur -= SGM41511_LIMIT_CURRENT_OFFSET;
	limit_cur = limit_cur / 1000;
	reg_val = limit_cur / SGM41511_REG_IINLIM_BASE;

	ret = sgm41511_update_bits(info, SGM41511_REG_0,
				  SGM41511_REG_LIMIT_CURRENT_MASK,
				  reg_val);
	if (ret)
		dev_err(info->dev, "set sgm41511 limit cur failed\n");

	info->actual_limit_cur = reg_val * SGM41511_REG_IINLIM_BASE * 1000;
	info->actual_limit_cur += SGM41511_LIMIT_CURRENT_OFFSET;

	return ret;
}

/* HS03 code for SL6215DEV-3879 by Ditong at 20211221 start */
static u32 sgm41511_charger_get_limit_voltage(struct sgm41511_charger_info *info,
					     u32 *limit_vol)
{
	u8 reg_val;
	int ret;

	ret = sgm41511_read(info, SGM41511_REG_4, &reg_val);
	if (ret < 0) {
		return ret;
	}

	reg_val &= SGM41511_REG_TERMINAL_VOLTAGE_MASK;
	*limit_vol = ((reg_val >> SGM41511_REG_TERMINAL_VOLTAGE_SHIFT) * 32) + 3856;

	if (*limit_vol < 3500) {
		*limit_vol = 3500;
	} else if (*limit_vol >= 4440) {
		*limit_vol = 4440;
	}

	dev_err(info->dev, "limit voltage is %d, actual_limt is %d\n", *limit_vol, info->actual_limit_voltage);

	return 0;
}
/* HS03 code for SL6215DEV-3879 by Ditong at 20211221 end */

static u32
sgm41511_charger_get_limit_current(struct sgm41511_charger_info *info,
				  u32 *limit_cur)
{
	u8 reg_val;
	int ret;

	ret = sgm41511_read(info, SGM41511_REG_0, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= SGM41511_REG_LIMIT_CURRENT_MASK;
	*limit_cur = reg_val * SGM41511_REG_IINLIM_BASE * 1000;
	*limit_cur += SGM41511_LIMIT_CURRENT_OFFSET;
	if (*limit_cur >= SGM41511_LIMIT_CURRENT_MAX)
		*limit_cur = SGM41511_LIMIT_CURRENT_MAX;

	return 0;
}

static int sgm41511_charger_get_health(struct sgm41511_charger_info *info,
				      u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int sgm41511_charger_get_online(struct sgm41511_charger_info *info,
				      u32 *online)
{
	if (info->limit)
		*online = true;
	else
		*online = false;

	return 0;
}

static void sgm41511_dump_register(struct sgm41511_charger_info *info)
{
	int i, ret, len, idx = 0;
	u8 reg_val;
	char buf[256];

	memset(buf, '\0', sizeof(buf));
	for (i = 0; i < SGM41511_REG_NUM; i++) {
		ret = sgm41511_read(info,  reg_tab[i].addr, &reg_val);
		if (ret == 0) {
			len = snprintf(buf + idx, sizeof(buf) - idx,
				       "[REG_0x%.2x]=0x%.2x  ",
				       reg_tab[i].addr, reg_val);
			idx += len;
		}
	}

	dev_err(info->dev, "%s: %s", __func__, buf);
}

/* HS03 code for SL6215DEV-3879 by Ditong at 20211221 start */
static int sgm41511_charger_feed_watchdog(struct sgm41511_charger_info *info,
					 u32 val)
{
	int ret;
	u32 limit_cur = 0;
	u32 limit_voltage = 4208;

	ret = sgm41511_update_bits(info, SGM41511_REG_1,
				  SGM41511_REG_RESET_MASK,
				  SGM41511_REG_RESET_MASK);
	if (ret) {
		dev_err(info->dev, "reset sgm41511 failed\n");
		return ret;
	}

	ret = sgm41511_charger_get_limit_voltage(info, &limit_voltage);
	if (ret) {
		dev_err(info->dev, "get limit voltage failed\n");
		return ret;
	}

	if (info->actual_limit_voltage != limit_voltage) {
		ret = sgm41511_charger_set_termina_vol(info, info->actual_limit_voltage);
		if (ret) {
			dev_err(info->dev, "set terminal voltage failed\n");
			return ret;
		}

		ret = sgm41511_charger_set_recharge(info);
		if (ret) {
			dev_err(info->dev, "set sgm41511 recharge failed\n");
			return ret;
		}
	}

	ret = sgm41511_charger_get_limit_current(info, &limit_cur);
	if (ret) {
		dev_err(info->dev, "get limit cur failed\n");
		return ret;
	}

	if (info->actual_limit_cur == limit_cur)
		return 0;

	ret = sgm41511_charger_set_limit_current(info, info->actual_limit_cur);
	if (ret) {
		dev_err(info->dev, "set limit cur failed\n");
		return ret;
	}

	return 0;
}
/* HS03 code for SL6215DEV-3879 by Ditong at 20211221 end*/

/* HS03 code for SR-SL6215-01-606 by gaochao at 20210813 start */
/*
static irqreturn_t sgm41511_int_handler(int irq, void *dev_id)
{
	struct sgm41511_charger_info *info = dev_id;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return IRQ_HANDLED;
	}

	dev_info(info->dev, "interrupt occurs\n");
	sgm41511_dump_register(info);

	return IRQ_HANDLED;
}
*/
/* HS03 code for SR-SL6215-01-606 by gaochao at 20210813 end */

static int sgm41511_charger_set_fchg_current(struct sgm41511_charger_info *info,
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

	ret = sgm41511_charger_set_limit_current(info, limit_cur);
	if (ret) {
		dev_err(info->dev, "failed to set fchg limit current\n");
		return ret;
	}

	ret = sgm41511_charger_set_current(info, cur);
	if (ret) {
		dev_err(info->dev, "failed to set fchg current\n");
		return ret;
	}

	return 0;
}

static int sgm41511_charger_get_status(struct sgm41511_charger_info *info)
{
	if (info->charging)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

/* HS03 code for SR-SL6215-01-606 by gaochao at 20210813 start */
static int sgm41511_charger_get_charge_done(struct sgm41511_charger_info *info,
	union power_supply_propval *val)
{
	int ret = 0;
	u8 reg_val = 0;

	if (!info || !val) {
		dev_err(info->dev, "[%s]line=%d: info or val is NULL\n", __FUNCTION__, __LINE__);
		return ret;
	}

	ret = sgm41511_read(info, SGM41511_REG_8, &reg_val);
	if (ret < 0) {
		dev_err(info->dev, "Failed to get charge_done, ret = %d\n", ret);
		return ret;
	}

	reg_val &= SGM41511_REG_CHARGE_DONE_MASK;
	reg_val >>= SGM41511_REG_CHARGE_DONE_SHIFT;
	val->intval = (reg_val == SGM41511_CHARGE_DONE);

	return 0;
}
/* HS03 code for SR-SL6215-01-606 by gaochao at 20210813 end */
static void sgm41511_check_wireless_charge(struct sgm41511_charger_info *info, bool enable)
{
	int ret;

	if (!enable)
		cancel_delayed_work_sync(&info->cur_work);

	if (info->is_wireless_charge && enable) {
		cancel_delayed_work_sync(&info->cur_work);
		ret = sgm41511_charger_set_current(info, info->current_charge_limit_cur);
		if (ret < 0)
			dev_err(info->dev, "%s:set charge current failed\n", __func__);

		ret = sgm41511_charger_set_current(info, info->current_input_limit_cur);
		if (ret < 0)
			dev_err(info->dev, "%s:set charge current failed\n", __func__);

		pm_wakeup_event(info->dev, SGM41511_WAKE_UP_MS);
		schedule_delayed_work(&info->cur_work, SGM41511_CURRENT_WORK_MS);
	} else if (info->is_wireless_charge && !enable) {
		info->new_charge_limit_cur = info->current_charge_limit_cur;
		info->current_charge_limit_cur = SGM41511_REG_ICHG_LSB * 1000;
		info->new_input_limit_cur = info->current_input_limit_cur;
		info->current_input_limit_cur = SGM41511_REG_IINDPM_LSB * 1000;
	} else if (!info->is_wireless_charge && !enable) {
		info->new_charge_limit_cur = SGM41511_REG_ICHG_LSB * 1000;
		info->current_charge_limit_cur = SGM41511_REG_ICHG_LSB * 1000;
		info->new_input_limit_cur = SGM41511_REG_IINDPM_LSB * 1000;
		info->current_input_limit_cur = SGM41511_REG_IINDPM_LSB * 1000;
	}
}

static int sgm41511_charger_set_status(struct sgm41511_charger_info *info,
				      int val)
{
	int ret = 0;
	u32 input_vol;

	if (val == CM_FAST_CHARGE_ENABLE_CMD) {
		ret = sgm41511_charger_set_fchg_current(info, val);
		if (ret) {
			dev_err(info->dev, "failed to set 9V fast charge current\n");
			return ret;
		}
		ret = sgm41511_charger_set_ovp(info, SGM41511_FCHG_OVP_9V);
		if (ret) {
			dev_err(info->dev, "failed to set fast charge 9V ovp\n");
			return ret;
		}
	} else if (val == CM_FAST_CHARGE_DISABLE_CMD) {
		ret = sgm41511_charger_set_fchg_current(info, val);
		if (ret) {
			dev_err(info->dev, "failed to set 5V normal charge current\n");
			return ret;
		}
		ret = sgm41511_charger_set_ovp(info, SGM41511_FCHG_OVP_6V);
		if (ret) {
			dev_err(info->dev, "failed to set fast charge 5V ovp\n");
			return ret;
		}
		if (info->role == SGM41511_ROLE_MASTER_DEFAULT) {
			ret = sgm41511_charger_get_charge_voltage(info, &input_vol);
			if (ret) {
				dev_err(info->dev, "failed to get 9V charge voltage\n");
				return ret;
			}
			if (input_vol > SGM41511_FAST_CHARGER_VOLTAGE_MAX)
				info->need_disable_Q1 = true;
		}
	} else if ((val == false) &&
		   (info->role == SGM41511_ROLE_MASTER_DEFAULT)) {
		ret = sgm41511_charger_get_charge_voltage(info, &input_vol);
		if (ret) {
			dev_err(info->dev, "failed to get 5V charge voltage\n");
			return ret;
		}
		if (input_vol > SGM41511_NORMAL_CHARGER_VOLTAGE_MAX)
			info->need_disable_Q1 = true;
	}

	if (val > CM_FAST_CHARGE_NORMAL_CMD)
		return 0;

	if (!val && info->charging) {
		sgm41511_check_wireless_charge(info, false);
		sgm41511_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		sgm41511_check_wireless_charge(info, true);
		ret = sgm41511_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}

static void sgm41511_charger_work(struct work_struct *data)
{
	struct sgm41511_charger_info *info =
		container_of(data, struct sgm41511_charger_info, work);
	bool present = sgm41511_charger_is_bat_present(info);

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	dev_info(info->dev, "battery present = %d, charger type = %d\n",
		 present, info->usb_phy->chg_type);
	cm_notify_event(info->psy_usb, CM_EVENT_CHG_START_STOP, NULL);
}

static void sgm41511_current_work(struct work_struct *data)
{
	struct delayed_work *dwork = to_delayed_work(data);
	struct sgm41511_charger_info *info =
		container_of(dwork, struct sgm41511_charger_info, cur_work);
	int ret = 0;
	bool need_return = false;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	if (info->current_charge_limit_cur > info->new_charge_limit_cur) {
		ret = sgm41511_charger_set_current(info, info->new_charge_limit_cur);
		if (ret < 0)
			dev_err(info->dev, "%s: set charge limit cur failed\n", __func__);
		return;
	}

	if (info->current_input_limit_cur > info->new_input_limit_cur) {
		ret = sgm41511_charger_set_limit_current(info, info->new_input_limit_cur);
		if (ret < 0)
			dev_err(info->dev, "%s: set input limit cur failed\n", __func__);
		return;
	}

	if (info->current_charge_limit_cur + SGM41511_REG_ICHG_LSB * 1000 <=
	    info->new_charge_limit_cur)
		info->current_charge_limit_cur += SGM41511_REG_ICHG_LSB * 1000;
	else
		need_return = true;

	if (info->current_input_limit_cur + SGM41511_REG_IINDPM_LSB * 1000 <=
	    info->new_input_limit_cur)
		info->current_input_limit_cur += SGM41511_REG_IINDPM_LSB * 1000;
	else if (need_return)
		return;

	ret = sgm41511_charger_set_current(info, info->current_charge_limit_cur);
	if (ret < 0) {
		dev_err(info->dev, "set charge limit current failed\n");
		return;
	}

	ret = sgm41511_charger_set_limit_current(info, info->current_input_limit_cur);
	if (ret < 0) {
		dev_err(info->dev, "set input limit current failed\n");
		return;
	}

	dev_info(info->dev, "set charge_limit_cur %duA, input_limit_curr %duA\n",
		info->current_charge_limit_cur, info->current_input_limit_cur);

	schedule_delayed_work(&info->cur_work, SGM41511_CURRENT_WORK_MS);
}


static int sgm41511_charger_usb_change(struct notifier_block *nb,
				      unsigned long limit, void *data)
{
	struct sgm41511_charger_info *info =
		container_of(nb, struct sgm41511_charger_info, usb_notify);

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return NOTIFY_OK;
	}

	info->limit = limit;

	/*
	 * only master should do work when vbus change.
	 * let info->limit = limit, slave will online, too.
	 */
	if (info->role == SGM41511_ROLE_SLAVE)
		return NOTIFY_OK;

	pm_wakeup_event(info->dev, SGM41511_WAKE_UP_MS);

	schedule_work(&info->work);
	return NOTIFY_OK;
}

static int sgm41511_charger_usb_get_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    union power_supply_propval *val)
{
	struct sgm41511_charger_info *info = power_supply_get_drvdata(psy);
	u32 cur, online, health, enabled = 0;
	enum usb_charger_type type;
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (info->limit || info->is_wireless_charge)
			val->intval = sgm41511_charger_get_status(info);
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;

	/* HS03 code for SR-SL6215-01-606 by gaochao at 20210813 start */
	case POWER_SUPPLY_PROP_CHARGE_DONE:
		// get charge_done from charger ic
		sgm41511_charger_get_charge_done(info, val);
		break;
	/* HS03 code for SR-SL6215-01-606 by gaochao at 20210813 end */
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sgm41511_charger_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sgm41511_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		ret = sgm41511_charger_get_online(info, &online);
		if (ret)
			goto out;

		val->intval = online;

		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = sgm41511_charger_get_health(info, &health);
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
		/* HS03 code for SR-SL6215-01-545 by shixuanxuan at 20210818 start */
		#if !defined(HQ_FACTORY_BUILD)
		case FLOAT_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_FLOAT;
			break;
		#endif
		/* HS03 code forSR-SL6215-01-545 by shixuanxuan at 20210818 end */
		default:
			val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		}

		break;

	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		if (info->role == SGM41511_ROLE_MASTER_DEFAULT) {
			ret = regmap_read(info->pmic, info->charger_pd, &enabled);
			if (ret) {
				dev_err(info->dev, "get sgm41511 charge status failed\n");
				goto out;
			}
		} else if (info->role == SGM41511_ROLE_SLAVE) {
			enabled = gpiod_get_value_cansleep(info->gpiod);
		}

		val->intval = !enabled;
		break;
	/* HS03 code for SL6215DEV-28 by qiaodan at 20210805 start */
	case POWER_SUPPLY_PROP_POWER_PATH_ENABLED:
		ret = sgm41511_get_hiz_mode(info, &enabled);
		val->intval = !enabled;
		break;
	/* HS03 code for SL6215DEV-28 by qiaodan at 20210805 end */
	/* HS03 code for SR-SL6215-01-63 by gaochao at 20210805 start */
	case POWER_SUPPLY_PROP_DUMP_CHARGER_IC:
		sgm41511_dump_register(info);
		break;
	/* HS03 code for SR-SL6215-01-63 by gaochao at 20210805 end */
	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int sgm41511_charger_usb_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct sgm41511_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (info->is_wireless_charge) {
			cancel_delayed_work_sync(&info->cur_work);
			info->new_charge_limit_cur = val->intval;
			pm_wakeup_event(info->dev, SGM41511_WAKE_UP_MS);
			schedule_delayed_work(&info->cur_work, SGM41511_CURRENT_WORK_MS * 2);
			break;
		}

		ret = sgm41511_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (info->is_wireless_charge) {
			cancel_delayed_work_sync(&info->cur_work);
			info->new_input_limit_cur = val->intval;
			pm_wakeup_event(info->dev, SGM41511_WAKE_UP_MS);
			schedule_delayed_work(&info->cur_work, SGM41511_CURRENT_WORK_MS * 2);
			break;
		}

		ret = sgm41511_charger_set_limit_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		break;

	case POWER_SUPPLY_PROP_STATUS:
		ret = sgm41511_charger_set_status(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;

	case POWER_SUPPLY_PROP_FEED_WATCHDOG:
		ret = sgm41511_charger_feed_watchdog(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "feed charger watchdog failed\n");
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = sgm41511_charger_set_termina_vol(info, val->intval / 1000);
		if (ret < 0)
			dev_err(info->dev, "failed to set terminate voltage\n");
		break;

	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		if (val->intval == true) {
			sgm41511_check_wireless_charge(info, true);
			ret = sgm41511_charger_start_charge(info);
			if (ret)
				dev_err(info->dev, "start charge failed\n");
		} else if (val->intval == false) {
			sgm41511_check_wireless_charge(info, false);
			sgm41511_charger_stop_charge(info);
		}
		break;
	case POWER_SUPPLY_PROP_WIRELESS_TYPE:
		if (val->intval == POWER_SUPPLY_WIRELESS_CHARGER_TYPE_BPP) {
			info->is_wireless_charge = true;
			ret = sgm41511_charger_set_ovp(info, SGM41511_FCHG_OVP_6V);
		} else if (val->intval == POWER_SUPPLY_WIRELESS_CHARGER_TYPE_EPP) {
			info->is_wireless_charge = true;
			ret = sgm41511_charger_set_ovp(info, SGM41511_FCHG_OVP_14V);
		} else {
			info->is_wireless_charge = false;
			ret = sgm41511_charger_set_ovp(info, SGM41511_FCHG_OVP_6V);
		}
		if (ret)
			dev_err(info->dev, "failed to set fast charge ovp\n");

		break;
	/* HS03 code for SL6215DEV-28 by qiaodan at 20210805 start */
	case POWER_SUPPLY_PROP_POWER_PATH_ENABLED:
		if (val->intval) {
			sgm41511_exit_hiz_mode(info);
		} else {
			sgm41511_enter_hiz_mode(info);
		}
		break;
	/* HS03 code for SL6215DEV-28 by qiaodan at 20210805 end */
	/* HS03 code for SL6215DEV-734 by shixuanxuan at 20210906 start */
	case POWER_SUPPLY_PROP_EN_CHG_TIMER:
		if (val->intval) {
			ret = sgm41511_charger_en_chg_timer(info, true);
		} else {
			ret = sgm41511_charger_en_chg_timer(info, false);
		}
		if (ret)
			dev_err(info->dev, "failed to disable chg_timer \n");
		break;
	/* HS03 code for SL6215DEV-734 by shixuanxuan at 20210906 end */
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int sgm41511_charger_property_is_writeable(struct power_supply *psy,
						 enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
	case POWER_SUPPLY_PROP_WIRELESS_TYPE:
	case POWER_SUPPLY_PROP_STATUS:
	/* HS03 code for SL6215DEV-28 by qiaodan at 20210805 start */
	case POWER_SUPPLY_PROP_POWER_PATH_ENABLED:
	/* HS03 code for SL6215DEV-28 by qiaodan at 20210805 end */
	/* HS03 code for SL6215DEV-734 by shixuanxuan at 20210906 start */
	case POWER_SUPPLY_PROP_EN_CHG_TIMER:
	/* HS03 code for SL6215DEV-734 by shixuanxuan at 20210906 end */
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_usb_type sgm41511_charger_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID,
	/* HS03 code for SR-SL6215-01-545 by shixuanxuan at 20210818 start */
	#if !defined(HQ_FACTORY_BUILD)
	POWER_SUPPLY_USB_TYPE_FLOAT,
	#endif
	/* HS03 code forSR-SL6215-01-545 by shixuanxuan at 20210818 end */
};

static enum power_supply_property sgm41511_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	/* HS03 code for SR-SL6215-01-606 by gaochao at 20210813 start */
	POWER_SUPPLY_PROP_CHARGE_DONE,
	/* HS03 code for SR-SL6215-01-606 by gaochao at 20210813 end */
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CHARGE_ENABLED,
	POWER_SUPPLY_PROP_WIRELESS_TYPE,
	/* HS03 code for SL6215DEV-28 by qiaodan at 20210805 start */
	POWER_SUPPLY_PROP_POWER_PATH_ENABLED,
	/* HS03 code for SL6215DEV-28 by qiaodan at 20210805 end */
	/* HS03 code for SR-SL6215-01-63 by gaochao at 20210805 start */
	POWER_SUPPLY_PROP_DUMP_CHARGER_IC,
	/* HS03 code for SR-SL6215-01-63 by gaochao at 20210805 end */
};

static const struct power_supply_desc sgm41511_charger_desc = {
	.name			= "sgm41511_charger",
	/* HS03 code for P211012-03864 by yuli at 20211027 start */
	//.type			= POWER_SUPPLY_TYPE_USB,
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	/* HS03 code for P211012-03864 by yuli at 20211027 end */
	.properties		= sgm41511_usb_props,
	.num_properties		= ARRAY_SIZE(sgm41511_usb_props),
	.get_property		= sgm41511_charger_usb_get_property,
	.set_property		= sgm41511_charger_usb_set_property,
	.property_is_writeable	= sgm41511_charger_property_is_writeable,
	.usb_types		= sgm41511_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(sgm41511_charger_usb_types),
};

static const struct power_supply_desc sgm41511_slave_charger_desc = {
	.name			= "sgm41511_slave_charger",
	/* HS03 code for P211012-03864 by yuli at 20211027 start */
	//.type			= POWER_SUPPLY_TYPE_USB,
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	/* HS03 code for P211012-03864 by yuli at 20211027 end */
	.properties		= sgm41511_usb_props,
	.num_properties		= ARRAY_SIZE(sgm41511_usb_props),
	.get_property		= sgm41511_charger_usb_get_property,
	.set_property		= sgm41511_charger_usb_set_property,
	.property_is_writeable	= sgm41511_charger_property_is_writeable,
	.usb_types		= sgm41511_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(sgm41511_charger_usb_types),
};

static ssize_t sgm41511_register_value_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sgm41511_charger_sysfs *sgm41511_sysfs =
		container_of(attr, struct sgm41511_charger_sysfs,
			     attr_sgm41511_reg_val);
	struct  sgm41511_charger_info *info =  sgm41511_sysfs->info;
	u8 val;
	int ret;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s  sgm41511_sysfs->info is null\n", __func__);

	ret = sgm41511_read(info, reg_tab[info->reg_id].addr, &val);
	if (ret) {
		dev_err(info->dev, "fail to get  SGM41511_REG_0x%.2x value, ret = %d\n",
			reg_tab[info->reg_id].addr, ret);
		return snprintf(buf, PAGE_SIZE, "fail to get  SGM41511_REG_0x%.2x value\n",
			       reg_tab[info->reg_id].addr);
	}

	return snprintf(buf, PAGE_SIZE, "SGM41511_REG_0x%.2x = 0x%.2x\n",
			reg_tab[info->reg_id].addr, val);
}

static ssize_t sgm41511_register_value_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	struct sgm41511_charger_sysfs *sgm41511_sysfs =
		container_of(attr, struct sgm41511_charger_sysfs,
			     attr_sgm41511_reg_val);
	struct sgm41511_charger_info *info = sgm41511_sysfs->info;
	u8 val;
	int ret;

	if (!info) {
		dev_err(dev, "%s sgm41511_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtou8(buf, 16, &val);
	if (ret) {
		dev_err(info->dev, "fail to get addr, ret = %d\n", ret);
		return count;
	}

	ret = sgm41511_write(info, reg_tab[info->reg_id].addr, val);
	if (ret) {
		dev_err(info->dev, "fail to wite 0x%.2x to REG_0x%.2x, ret = %d\n",
				val, reg_tab[info->reg_id].addr, ret);
		return count;
	}

	dev_info(info->dev, "wite 0x%.2x to REG_0x%.2x success\n", val, reg_tab[info->reg_id].addr);
	return count;
}

static ssize_t sgm41511_register_id_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct sgm41511_charger_sysfs *sgm41511_sysfs =
		container_of(attr, struct sgm41511_charger_sysfs,
			     attr_sgm41511_sel_reg_id);
	struct sgm41511_charger_info *info = sgm41511_sysfs->info;
	int ret, id;

	if (!info) {
		dev_err(dev, "%s sgm41511_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtoint(buf, 10, &id);
	if (ret) {
		dev_err(info->dev, "%s store register id fail\n", sgm41511_sysfs->name);
		return count;
	}

	if (id < 0 || id >= SGM41511_REG_NUM) {
		dev_err(info->dev, "%s store register id fail, id = %d is out of range\n",
			sgm41511_sysfs->name, id);
		return count;
	}

	info->reg_id = id;

	dev_info(info->dev, "%s store register id = %d success\n", sgm41511_sysfs->name, id);
	return count;
}

static ssize_t sgm41511_register_id_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sgm41511_charger_sysfs *sgm41511_sysfs =
		container_of(attr, struct sgm41511_charger_sysfs,
			     attr_sgm41511_sel_reg_id);
	struct sgm41511_charger_info *info = sgm41511_sysfs->info;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s sgm41511_sysfs->info is null\n", __func__);

	return snprintf(buf, PAGE_SIZE, "Curent register id = %d\n", info->reg_id);
}

static ssize_t sgm41511_register_table_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sgm41511_charger_sysfs *sgm41511_sysfs =
		container_of(attr, struct sgm41511_charger_sysfs,
			     attr_sgm41511_lookup_reg);
	struct sgm41511_charger_info *info = sgm41511_sysfs->info;
	int i, len, idx = 0;
	char reg_tab_buf[2048];

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s sgm41511_sysfs->info is null\n", __func__);

	memset(reg_tab_buf, '\0', sizeof(reg_tab_buf));
	len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
		       "Format: [id] [addr] [desc]\n");
	idx += len;

	for (i = 0; i < SGM41511_REG_NUM; i++) {
		len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
			       "[%d] [REG_0x%.2x] [%s]; \n",
			       reg_tab[i].id, reg_tab[i].addr, reg_tab[i].name);
		idx += len;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", reg_tab_buf);
}

static ssize_t sgm41511_dump_register_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct sgm41511_charger_sysfs *sgm41511_sysfs =
		container_of(attr, struct sgm41511_charger_sysfs,
			     attr_sgm41511_dump_reg);
	struct sgm41511_charger_info *info = sgm41511_sysfs->info;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s sgm41511_sysfs->info is null\n", __func__);

	sgm41511_dump_register(info);

	return snprintf(buf, PAGE_SIZE, "%s\n", sgm41511_sysfs->name);
}

static int sgm41511_register_sysfs(struct sgm41511_charger_info *info)
{
	struct sgm41511_charger_sysfs *sgm41511_sysfs;
	int ret;

	sgm41511_sysfs = devm_kzalloc(info->dev, sizeof(*sgm41511_sysfs), GFP_KERNEL);
	if (!sgm41511_sysfs)
		return -ENOMEM;

	info->sysfs = sgm41511_sysfs;
	sgm41511_sysfs->name = "sgm41511_sysfs";
	sgm41511_sysfs->info = info;
	sgm41511_sysfs->attrs[0] = &sgm41511_sysfs->attr_sgm41511_dump_reg.attr;
	sgm41511_sysfs->attrs[1] = &sgm41511_sysfs->attr_sgm41511_lookup_reg.attr;
	sgm41511_sysfs->attrs[2] = &sgm41511_sysfs->attr_sgm41511_sel_reg_id.attr;
	sgm41511_sysfs->attrs[3] = &sgm41511_sysfs->attr_sgm41511_reg_val.attr;
	sgm41511_sysfs->attrs[4] = NULL;
	sgm41511_sysfs->attr_g.name = "debug";
	sgm41511_sysfs->attr_g.attrs = sgm41511_sysfs->attrs;

	sysfs_attr_init(&sgm41511_sysfs->attr_sgm41511_dump_reg.attr);
	sgm41511_sysfs->attr_sgm41511_dump_reg.attr.name = "sgm41511_dump_reg";
	sgm41511_sysfs->attr_sgm41511_dump_reg.attr.mode = 0444;
	sgm41511_sysfs->attr_sgm41511_dump_reg.show = sgm41511_dump_register_show;

	sysfs_attr_init(&sgm41511_sysfs->attr_sgm41511_lookup_reg.attr);
	sgm41511_sysfs->attr_sgm41511_lookup_reg.attr.name = "sgm41511_lookup_reg";
	sgm41511_sysfs->attr_sgm41511_lookup_reg.attr.mode = 0444;
	sgm41511_sysfs->attr_sgm41511_lookup_reg.show = sgm41511_register_table_show;

	sysfs_attr_init(&sgm41511_sysfs->attr_sgm41511_sel_reg_id.attr);
	sgm41511_sysfs->attr_sgm41511_sel_reg_id.attr.name = "sgm41511_sel_reg_id";
	sgm41511_sysfs->attr_sgm41511_sel_reg_id.attr.mode = 0644;
	sgm41511_sysfs->attr_sgm41511_sel_reg_id.show = sgm41511_register_id_show;
	sgm41511_sysfs->attr_sgm41511_sel_reg_id.store = sgm41511_register_id_store;

	sysfs_attr_init(&sgm41511_sysfs->attr_sgm41511_reg_val.attr);
	sgm41511_sysfs->attr_sgm41511_reg_val.attr.name = "sgm41511_reg_val";
	sgm41511_sysfs->attr_sgm41511_reg_val.attr.mode = 0644;
	sgm41511_sysfs->attr_sgm41511_reg_val.show = sgm41511_register_value_show;
	sgm41511_sysfs->attr_sgm41511_reg_val.store = sgm41511_register_value_store;

	ret = sysfs_create_group(&info->psy_usb->dev.kobj, &sgm41511_sysfs->attr_g);
	if (ret < 0)
		dev_err(info->dev, "Cannot create sysfs , ret = %d\n", ret);

	return ret;
}

static void sgm41511_charger_detect_status(struct sgm41511_charger_info *info)
{
	unsigned int min, max;

	/*
	 * If the USB charger status has been USB_CHARGER_PRESENT before
	 * registering the notifier, we should start to charge with getting
	 * the charge current.
	 */
	if (info->usb_phy->chg_state != USB_CHARGER_PRESENT)
		return;

	usb_phy_get_charger_current(info->usb_phy, &min, &max);
	info->limit = min;

	/*
	 * slave no need to start charge when vbus change.
	 * due to charging in shut down will check each psy
	 * whether online or not, so let info->limit = min.
	 */
	if (info->role == SGM41511_ROLE_SLAVE)
		return;
	schedule_work(&info->work);
}

static void
sgm41511_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sgm41511_charger_info *info = container_of(dwork,
							 struct sgm41511_charger_info,
							 wdt_work);
	int ret;

	ret = sgm41511_update_bits(info, SGM41511_REG_1,
				  SGM41511_REG_WATCHDOG_MASK,
				  SGM41511_REG_WATCHDOG_MASK);
	if (ret) {
		dev_err(info->dev, "reset sgm41511 failed\n");
		return;
	}
	schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#ifdef CONFIG_REGULATOR
static bool sgm41511_charger_check_otg_valid(struct sgm41511_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = false;

	ret = sgm41511_read(info, SGM41511_REG_1, &value);
	if (ret) {
		dev_err(info->dev, "get sgm41511 charger otg valid status failed\n");
		return status;
	}

	if (value & SGM41511_REG_OTG_MASK)
		status = true;
	else
		dev_err(info->dev, "otg is not valid, REG_1 = 0x%x\n", value);

	return status;
}

static bool sgm41511_charger_check_otg_fault(struct sgm41511_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = true;

	ret = sgm41511_read(info, SGM41511_REG_9, &value);
	if (ret) {
		dev_err(info->dev, "get sgm41511 charger otg fault status failed\n");
		return status;
	}

	if (!(value & SGM41511_REG_BOOST_FAULT_MASK))
		status = false;
	else
		dev_err(info->dev, "boost fault occurs, REG_9 = 0x%x\n", value);

	return status;
}

static void sgm41511_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sgm41511_charger_info *info = container_of(dwork,
			struct sgm41511_charger_info, otg_work);
	bool otg_valid = sgm41511_charger_check_otg_valid(info);
	bool otg_fault;
	int ret, retry = 0;

	if (otg_valid)
		goto out;

	do {
		otg_fault = sgm41511_charger_check_otg_fault(info);
		if (!otg_fault) {
			ret = sgm41511_update_bits(info, SGM41511_REG_1,
						  SGM41511_REG_OTG_MASK,
						  SGM41511_REG_OTG_MASK);
			if (ret)
				dev_err(info->dev, "restart sgm41511 charger otg failed\n");
		}

		otg_valid = sgm41511_charger_check_otg_valid(info);
	} while (!otg_valid && retry++ < SGM41511_OTG_RETRY_TIMES);

	if (retry >= SGM41511_OTG_RETRY_TIMES) {
		dev_err(info->dev, "Restart OTG failed\n");
		return;
	}

out:
	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(1500));
}

static int sgm41511_charger_enable_otg(struct regulator_dev *dev)
{
	struct sgm41511_charger_info *info = rdev_get_drvdata(dev);
	int ret = 0;

	/* HS03 code for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 start */
	dev_info(info->dev, "%s:line%d enter\n", __func__, __LINE__);
	/* HS03 code for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 end */
	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	/*
	 * Disable charger detection function in case
	 * affecting the OTG timing sequence.
	 */
	ret = regmap_update_bits(info->pmic, info->charger_detect,
				 BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
	if (ret) {
		dev_err(info->dev, "failed to disable bc1.2 detect function.\n");
		goto out;
	}

	ret = sgm41511_update_bits(info, SGM41511_REG_1,
				  SGM41511_REG_OTG_MASK,
				  SGM41511_REG_OTG_MASK);
	if (ret) {
		dev_err(info->dev, "enable sgm41511 otg failed\n");
		regmap_update_bits(info->pmic, info->charger_detect,
				   BIT_DP_DM_BC_ENB, 0);
		goto out;
	}

	info->otg_enable = true;
	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(SGM41511_FEED_WATCHDOG_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(SGM41511_OTG_VALID_MS));
out:
	mutex_unlock(&info->lock);
	return ret;
}

static int sgm41511_charger_disable_otg(struct regulator_dev *dev)
{
	struct sgm41511_charger_info *info = rdev_get_drvdata(dev);
	int ret = 0;

	/* HS03 code for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 start */
	dev_info(info->dev, "%s:line%d enter\n", __func__, __LINE__);
	/* HS03 code for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 end */
	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	info->otg_enable = false;
	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);
	ret = sgm41511_update_bits(info, SGM41511_REG_1,
				  SGM41511_REG_OTG_MASK,
				  0);
	if (ret) {
		dev_err(info->dev, "disable sgm41511 otg failed\n");
		goto out;
	}

	/* Enable charger detection function to identify the charger type */
	ret = regmap_update_bits(info->pmic, info->charger_detect,
				  BIT_DP_DM_BC_ENB, 0);
	if (ret)
		dev_err(info->dev, "enable BC1.2 failed\n");

out:
	mutex_unlock(&info->lock);
	return ret;


}

static int sgm41511_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct sgm41511_charger_info *info = rdev_get_drvdata(dev);
	int ret;
	u8 val;

	/* HS03 code for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 start */
	dev_info(info->dev, "%s:line%d enter\n", __func__, __LINE__);
	/* HS03 code for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 end */
	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	ret = sgm41511_read(info, SGM41511_REG_1, &val);
	if (ret) {
		dev_err(info->dev, "failed to get sgm41511 otg status\n");
		mutex_unlock(&info->lock);
		return ret;
	}

	val &= SGM41511_REG_OTG_MASK;
	/* HS03 code for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 start */
	dev_info(info->dev, "%s:line%d val = %d\n", __func__, __LINE__, val);
	/* HS03 code for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 end */

	mutex_unlock(&info->lock);
	return val;
}

static const struct regulator_ops sgm41511_charger_vbus_ops = {
	.enable = sgm41511_charger_enable_otg,
	.disable = sgm41511_charger_disable_otg,
	.is_enabled = sgm41511_charger_vbus_is_enabled,
};

static const struct regulator_desc sgm41511_charger_vbus_desc = {
	/* HS03 code for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 start */
	// .name = "otg-vbus",
	// .of_match = "otg-vbus",
	.name = "sgm41511_otg_vbus",
	.of_match = "sgm41511_otg_vbus",
	/* HS03 code for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 end */
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &sgm41511_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int
sgm41511_charger_register_vbus_regulator(struct sgm41511_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev,
				      &sgm41511_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}

#else
static int
sgm41511_charger_register_vbus_regulator(struct sgm41511_charger_info *info)
{
	return 0;
}
#endif

static int sgm41511_charger_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct sgm41511_charger_info *info;
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	int ret;

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

	/* HS03 for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 start */
	client->addr = 0x6B;
	/* HS03 for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 end */
	info->client = client;
	info->dev = dev;

	/* HS03 code for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 start */
	ret = sgm41511_charger_get_vendor_id_part_value(info);
	if (ret) {
		dev_err(dev, "failed to get vendor id, part value\n");
		return ret;
	}
	/* HS03 code for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 end */

	i2c_set_clientdata(client, info);
	power_path_control(info);

	info->usb_phy = devm_usb_get_phy_by_phandle(dev, "phys", 0);
	if (IS_ERR(info->usb_phy)) {
		dev_err(dev, "failed to find USB phy\n");
		return -EPROBE_DEFER;
	}

	info->edev = extcon_get_edev_by_phandle(info->dev, 0);
	if (IS_ERR(info->edev)) {
		dev_err(dev, "failed to find vbus extcon device.\n");
		return -EPROBE_DEFER;
	}

	ret = sgm41511_charger_is_fgu_present(info);
	if (ret) {
		dev_err(dev, "sc27xx_fgu not ready.\n");
		return -EPROBE_DEFER;
	}

	ret = device_property_read_bool(dev, "role-slave");
	if (ret)
		info->role = SGM41511_ROLE_SLAVE;
	else
		info->role = SGM41511_ROLE_MASTER_DEFAULT;

	if (info->role == SGM41511_ROLE_SLAVE) {
		info->gpiod = devm_gpiod_get(dev, "enable", GPIOD_OUT_HIGH);
		if (IS_ERR(info->gpiod)) {
			dev_err(dev, "failed to get enable gpio\n");
			return PTR_ERR(info->gpiod);
		}
	}

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np)
		regmap_np = of_find_compatible_node(NULL, NULL, "sprd,ump962x-syscon");

	if (regmap_np) {
		if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
			info->charger_pd_mask = SGM41511_DISABLE_PIN_MASK_2721;
		else
			info->charger_pd_mask = SGM41511_DISABLE_PIN_MASK;
	} else {
		dev_err(dev, "unable to get syscon node\n");
		return -ENODEV;
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
	mutex_init(&info->lock);
	mutex_lock(&info->lock);

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	if (info->role == SGM41511_ROLE_MASTER_DEFAULT) {
		info->psy_usb = devm_power_supply_register(dev,
							   &sgm41511_charger_desc,
							   &charger_cfg);
	} else if (info->role == SGM41511_ROLE_SLAVE) {
		info->psy_usb = devm_power_supply_register(dev,
							   &sgm41511_slave_charger_desc,
							   &charger_cfg);
	}

	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		ret = PTR_ERR(info->psy_usb);
		goto err_regmap_exit;
	}

	ret = sgm41511_charger_hw_init(info);
	if (ret) {
		dev_err(dev, "failed to sgm41511_charger_hw_init\n");
		goto err_psy_usb;
	}

	sgm41511_charger_stop_charge(info);

	device_init_wakeup(info->dev, true);

	alarm_init(&info->otg_timer, ALARM_BOOTTIME, NULL);
	INIT_DELAYED_WORK(&info->otg_work, sgm41511_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work, sgm41511_charger_feed_watchdog_work);

	/*
	 * only master to support otg
	 */
	if (info->role == SGM41511_ROLE_MASTER_DEFAULT) {
		ret = sgm41511_charger_register_vbus_regulator(info);
		if (ret) {
			dev_err(dev, "failed to register vbus regulator.\n");
			goto err_psy_usb;
		}
	}

	INIT_WORK(&info->work, sgm41511_charger_work);
	INIT_DELAYED_WORK(&info->cur_work, sgm41511_current_work);

	info->usb_notify.notifier_call = sgm41511_charger_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		dev_err(dev, "failed to register notifier:%d\n", ret);
		goto err_psy_usb;
	}

	ret = sgm41511_register_sysfs(info);
	if (ret) {
		dev_err(info->dev, "register sysfs fail, ret = %d\n", ret);
		goto error_sysfs;
	}

	info->irq_gpio = of_get_named_gpio(info->dev->of_node, "irq-gpio", 0);
	if (gpio_is_valid(info->irq_gpio)) {
		ret = devm_gpio_request_one(info->dev, info->irq_gpio,
					    GPIOF_DIR_IN, "sgm41511_int");
		if (!ret)
			info->client->irq = gpio_to_irq(info->irq_gpio);
		else
			dev_err(dev, "int request failed, ret = %d\n", ret);

		if (info->client->irq < 0) {
			dev_err(dev, "failed to get irq no\n");
			gpio_free(info->irq_gpio);
		} else {
			/* HS03 code for SR-SL6215-01-606 by gaochao at 20210813 start */
			/*
			ret = devm_request_threaded_irq(&info->client->dev, info->client->irq,
							NULL, sgm41511_int_handler,
							IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
							"sgm41511 interrupt", info);
			if (ret)
				dev_err(info->dev, "Failed irq = %d ret = %d\n",
					info->client->irq, ret);
			else
				enable_irq_wake(client->irq);
			*/
			/* HS03 code for SR-SL6215-01-606 by gaochao at 20210813 end */
		}
	} else {
		dev_err(dev, "failed to get irq gpio\n");
	}

	mutex_unlock(&info->lock);
	sgm41511_charger_detect_status(info);
	/* HS03 code for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 start */
	pr_info("[%s]line=%d: probe success\n", __FUNCTION__, __LINE__);
	/* HS03 code for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 end */

	return 0;

error_sysfs:
	sysfs_remove_group(&info->psy_usb->dev.kobj, &info->sysfs->attr_g);
	usb_unregister_notifier(info->usb_phy, &info->usb_notify);
err_psy_usb:
	power_supply_unregister(info->psy_usb);
	if (info->irq_gpio)
		gpio_free(info->irq_gpio);
err_regmap_exit:
	regmap_exit(info->pmic);
	mutex_unlock(&info->lock);
	mutex_destroy(&info->lock);
	return ret;
}

static void sgm41511_charger_shutdown(struct i2c_client *client)
{
	struct sgm41511_charger_info *info = i2c_get_clientdata(client);
	int ret = 0;

	cancel_delayed_work_sync(&info->wdt_work);
	if (info->otg_enable) {
		info->otg_enable = false;
		cancel_delayed_work_sync(&info->otg_work);
		ret = sgm41511_update_bits(info, SGM41511_REG_1,
					  SGM41511_REG_OTG_MASK,
					  0);
		if (ret)
			dev_err(info->dev, "disable sgm41511 otg failed ret = %d\n", ret);

		/* Enable charger detection function to identify the charger type */
		ret = regmap_update_bits(info->pmic, info->charger_detect,
					 BIT_DP_DM_BC_ENB, 0);
		if (ret)
			dev_err(info->dev,
				"enable charger detection function failed ret = %d\n", ret);
	}
}

static int sgm41511_charger_remove(struct i2c_client *client)
{
	struct sgm41511_charger_info *info = i2c_get_clientdata(client);

	usb_unregister_notifier(info->usb_phy, &info->usb_notify);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sgm41511_charger_suspend(struct device *dev)
{
	struct sgm41511_charger_info *info = dev_get_drvdata(dev);
	ktime_t now, add;
	unsigned int wakeup_ms = SGM41511_OTG_ALARM_TIMER_MS;
	int ret;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!info->otg_enable)
		return 0;

	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->cur_work);

	/* feed watchdog first before suspend */
	ret = sgm41511_update_bits(info, SGM41511_REG_1,
				   SGM41511_REG_RESET_MASK,
				   SGM41511_REG_RESET_MASK);
	if (ret)
		dev_warn(info->dev, "reset sgm41511 failed before suspend\n");

	now = ktime_get_boottime();
	add = ktime_set(wakeup_ms / MSEC_PER_SEC,
			(wakeup_ms % MSEC_PER_SEC) * NSEC_PER_MSEC);
	alarm_start(&info->otg_timer, ktime_add(now, add));

	return 0;
}

static int sgm41511_charger_resume(struct device *dev)
{
	struct sgm41511_charger_info *info = dev_get_drvdata(dev);
	int ret;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!info->otg_enable)
		return 0;

	alarm_cancel(&info->otg_timer);

	/* feed watchdog first after resume */
	ret = sgm41511_update_bits(info, SGM41511_REG_1,
				   SGM41511_REG_RESET_MASK,
				   SGM41511_REG_RESET_MASK);
	if (ret)
		dev_warn(info->dev, "reset sgm41511 failed after resume\n");

	schedule_delayed_work(&info->wdt_work, HZ * 15);
	schedule_delayed_work(&info->cur_work, 0);

	return 0;
}
#endif

static const struct dev_pm_ops sgm41511_charger_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sgm41511_charger_suspend,
				sgm41511_charger_resume)
};

static const struct i2c_device_id sgm41511_i2c_id[] = {
	{"sgm41511_chg", 0},
	{}
};

static const struct of_device_id sgm41511_charger_of_match[] = {
	{ .compatible = "sgm,sgm41511_chg", },
	{ }
};

static const struct i2c_device_id sgm41511_slave_i2c_id[] = {
	{"sgm41511_slave_chg", 0},
	{}
};

static const struct of_device_id sgm41511_slave_charger_of_match[] = {
	{ .compatible = "ti,sgm41511_slave_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, sgm41511_charger_of_match);
MODULE_DEVICE_TABLE(of, sgm41511_slave_charger_of_match);

static struct i2c_driver sgm41511_master_charger_driver = {
	.driver = {
		.name = "sgm41511_chg",
		.of_match_table = sgm41511_charger_of_match,
		.pm = &sgm41511_charger_pm_ops,
	},
	.probe = sgm41511_charger_probe,
	.remove = sgm41511_charger_remove,
	.id_table = sgm41511_i2c_id,
};

static struct i2c_driver sgm41511_slave_charger_driver = {
	.driver = {
		.name = "bq2560_slave_chg",
		.of_match_table = sgm41511_slave_charger_of_match,
		.pm = &sgm41511_charger_pm_ops,
	},
	.probe = sgm41511_charger_probe,
	.shutdown = sgm41511_charger_shutdown,
	.remove = sgm41511_charger_remove,
	.id_table = sgm41511_slave_i2c_id,
};

module_i2c_driver(sgm41511_master_charger_driver);
module_i2c_driver(sgm41511_slave_charger_driver);
MODULE_DESCRIPTION("SGM41511 Charger Driver");
MODULE_LICENSE("GPL v2");
