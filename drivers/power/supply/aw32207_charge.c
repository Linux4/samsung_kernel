/*
 * Driver for the awinic aw32207 charger.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>
#include <linux/usb/phy.h>
#include <uapi/linux/usb/charger.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/gpio/consumer.h>


#define AW32207_REG_0					0x0
#define AW32207_REG_1					0x1
#define AW32207_REG_2					0x2
#define AW32207_REG_3					0x3
#define AW32207_REG_4					0x4
#define AW32207_REG_5					0x5
#define AW32207_REG_6					0x6
#define AW32207_REG_10					0x10
#define AW32207_REG_NUM                 11

#define AW32207_BATTERY_NAME				"sc27xx-fgu"
#define BIT_DP_DM_BC_ENB				BIT(0)
#define AW32207_OTG_VALID_MS				500
#define AW32207_FEED_WATCHDOG_VALID_MS			50

#define AW32207_REG_HZ_MODE_MASK			GENMASK(1, 1)
#define AW32207_REG_OPA_MODE_MASK			GENMASK(0, 0)

#define AW32207_REG_SAFETY_VOL_MASK			GENMASK(3, 0)
#define AW32207_REG_SAFETY_CUR_MASK			GENMASK(7, 4)
#define AW32207_REG_SAFETY_CUR_SHIFT		4

#define AW32207_REG_RESET_MASK				GENMASK(7, 7)
#define AW32207_REG_RESET				BIT(7)

#define AW32207_REG_WEAK_VOL_THRESHOLD_MASK		GENMASK(5, 4)

#define AW32207_REG_IO_LEVEL_MASK			GENMASK(5, 5)

#define AW32207_REG_VSP_MASK				GENMASK(2, 0)
#define AW32207_REG_VSP				(BIT(2) | BIT(0))

#define AW32207_REG_TERMINAL_CURRENT_MASK		GENMASK(3, 3)
#define AW32207_REG_TERMINAL_CURRENT_SHIFT		3

#define AW32207_REG_TERMINAL_CURRENT			(BIT(1) | BIT(0))
#define AW32207_REG_TERMINAL_VOLTAGE_MASK		GENMASK(7, 2)
#define AW32207_REG_TERMINAL_VOLTAGE_SHIFT		2

#define AW32207_REG_CHARGE_CONTROL_MASK		GENMASK(2, 2)
#define AW32207_REG_CHARGE_DISABLE			BIT(2)
#define AW32207_REG_CHARGE_ENABLE			0

#define AW32207_REG_CURRENT_MASK			GENMASK(6, 3)
#define AW32207_REG_CURRENT_MASK_SHIFT			3

#define AW32207_REG_LIMIT_CURRENT_MASK			GENMASK(7, 6)
#define AW32207_REG_LIMIT_CURRENT_SHIFT		6
#define AW32207_REG_EN_CHARGER_MASK			GENMASK(2, 2)
#define AW32207_REG_EN_CHARGER_SHIFT		2
#define AW32207_REG_EN_CHARGER				(BIT(3))

#define AW32207_REG_TERMINAL_CURRENT_CFG_MASK		GENMASK(2, 0)

#define AW32207_GPIO18_PIN_DOWN                 0




#define AW32207_DISABLE_PIN_MASK_2730			BIT(0)
#define AW32207_DISABLE_PIN_MASK_2721			BIT(15)
#define AW32207_DISABLE_PIN_MASK_2720			BIT(0)

struct aw32207_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	struct power_supply *psy_usb;
	struct power_supply_charge_current cur;
	struct work_struct work;
	struct mutex lock;
	bool charging;
	u32 limit;
	struct delayed_work otg_work;
	//struct delayed_work wdt_work;
	struct regmap *pmic;
	u32 charger_detect;
	u32 charger_pd;
	u32 charger_pd_mask;
	struct gpio_desc *gpiod;
	struct extcon_dev *edev;
	int    gpio18_val;
};
extern char *chg_ic;
//extern int i2c_devinfo_device_write(char *buf);
#if 1
#ifdef dev_info
#undef dev_info 
#define dev_info(dev, fmt, arg...) printk(fmt, ##arg) 
#endif
#endif

#if 0
static int
aw32207_charger_set_limit_current(struct aw32207_charger_info *info,
				  u32 limit_cur);
#endif

static bool aw32207_charger_is_bat_present(struct aw32207_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool present = false;
	int ret;

	psy = power_supply_get_by_name(AW32207_BATTERY_NAME);
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

static int aw32207_read(struct aw32207_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int aw32207_write(struct aw32207_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int aw32207_update_bits(struct aw32207_charger_info *info, u8 reg,
			       u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = aw32207_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return aw32207_write(info, reg, v);
}

static int
aw32207_charger_set_safety_vol(struct aw32207_charger_info *info, u32 vol)
{
	u8 reg_val;
	dev_info(info->dev, "cur = %d", vol);
	if (vol < 4200)
		vol = 4200;
	if (vol > 4440)
		vol = 4440;
	reg_val = (vol - 4200) / 20 + 1;

	return aw32207_update_bits(info, AW32207_REG_6,
				    AW32207_REG_SAFETY_VOL_MASK, reg_val);
}

static int
aw32207_charger_set_termina_vol(struct aw32207_charger_info *info, u32 vol)
{
	u8 reg_val;
	dev_info(info->dev, "cur = %d", vol);

	if (vol < 3500)
		reg_val = 0x0;
	else if (vol >= 4500)
		reg_val = 0x32;
	else
		reg_val = (vol - 3499) / 20;

	return aw32207_update_bits(info, AW32207_REG_2,
				    AW32207_REG_TERMINAL_VOLTAGE_MASK,
				    reg_val << AW32207_REG_TERMINAL_VOLTAGE_SHIFT);
}

static int
aw32207_charger_set_termina_cur(struct aw32207_charger_info *info,  u32 cur)
{
	u8 reg_val;
	dev_info(info->dev, " cur = %d", cur);
	if(info->gpio18_val == AW32207_GPIO18_PIN_DOWN) { 
		if (cur < 50)
			reg_val = 0x0;
		else if (cur >= 50 && cur < 100)
			reg_val = 0x0;
		else if (cur >= 100 && cur < 150)
			reg_val = 0x1;
		else if (cur >= 150 && cur < 200)
			reg_val = 0x2;
		else if (cur >= 200 && cur < 250)
			reg_val = 0x3;
		else if (cur >= 250 && cur < 300)
			reg_val = 0x4;
		else if (cur >= 300 && cur < 350)
			reg_val = 0x5;
		else if (cur >= 350 && cur < 400)
			reg_val = 0x6;
		else if (cur >= 400)
			reg_val = 0x7;
	} else {
		if (cur < 60)
			reg_val = 0x0;
		else if (cur >= 60 && cur < 121)
			reg_val = 0x0;
		else if (cur >= 121 && cur < 181)
			reg_val = 0x1;
		else if (cur >= 181 && cur < 242)
			reg_val = 0x2;
		else if (cur >= 242 && cur < 303)
			reg_val = 0x3;
		else if (cur >= 303 && cur < 364)
			reg_val = 0x4;
		else if (cur >= 364 && cur < 425)
			reg_val = 0x5;
		else if (cur >= 425 && cur < 485)
			reg_val = 0x6;
		else if (cur >= 485)
			reg_val = 0x7;
	}
		
		return aw32207_update_bits(info, AW32207_REG_4,
				    AW32207_REG_TERMINAL_CURRENT_CFG_MASK,
				    reg_val);
	
}


static int
aw32207_charger_set_safety_cur(struct aw32207_charger_info *info, u32 cur)
{
	u8 reg_val;
	dev_info(info->dev, "cur = %d",  cur);
	if (cur < 400000)
		reg_val = 0x0;
	else if (cur >= 400000 && cur < 700000)
		reg_val = 0x1;
	else if (cur >= 700000 && cur < 800000)
		reg_val = 0x2;
	else if (cur >= 800000 && cur < 900000)
		reg_val = 0x3;
	else if (cur >= 900000 && cur < 1000000)
		reg_val = 0x4;
	else if (cur >= 1000000 && cur < 1100000)
		reg_val = 0x5;
	else if (cur >= 1100000 && cur < 1200000)
		reg_val = 0x6;
	else if (cur >= 1200000 && cur < 1300000)
		reg_val = 0x7;
	else if (cur >= 1300000 && cur < 1400000)
		reg_val = 0x8;
	else if (cur >= 1400000 && cur < 1500000)
		reg_val = 0x9;
	else if (cur >= 1500000)
		reg_val = 0xa;

	return aw32207_update_bits(info, AW32207_REG_6,
				    AW32207_REG_SAFETY_CUR_MASK,
				    reg_val << AW32207_REG_SAFETY_CUR_SHIFT);
}

#if 0
static int aw32207_charger_set_dpm_voltage(struct aw32207_charger_info *info, u32 voltage)
{
	u8 reg_val = 0/*,data =0*/;
	int ret = 0;

	switch (voltage) {
	case 4300:
		reg_val = 0x01; //4325mV
		break;
	case 4500:
		reg_val = 0x03; //4475mV
		break;
	case 4600:
		reg_val = 0x05; //4625mV
		break;
	default:
		reg_val = AW32207_REG_VSP;
	}

	pr_info("honor charger_set_dpm_voltage voltage=%d reg_val=%x\n", voltage, reg_val);
	ret = aw32207_update_bits(info, AW32207_REG_5,
			AW32207_REG_VSP_MASK,
			reg_val);
	if (ret) {
		dev_err(info->dev, "honor set vsp failed\n");
		return ret;
	}
	/*aw32207_read(info, AW32207_REG_5, &data);
	pr_info("honor  AW32207_REG_5 =0x%x\n", data);*/

	return ret;
}
#endif
static int aw32207_charger_hw_init(struct aw32207_charger_info *info)
{
	struct power_supply_battery_info bat_info = { };
	int voltage_max_microvolt, current_max_ua, term_current_ma;
	int ret;
	int bat_id = 0;
	
	//bat_id = get_battery_id();
	bat_id = 0;
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
		info->cur.dcp_cur = 1550000;
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

		voltage_max_microvolt =
			bat_info.constant_charge_voltage_max_uv / 1000;
		current_max_ua = bat_info.constant_charge_current_max_ua / 1000;
		term_current_ma = bat_info.charge_term_current_ua / 1000;
		power_supply_put_battery_info(info->psy_usb, &bat_info);

		if (of_device_is_compatible(info->dev->of_node,
					    "awinic,aw32207_chg")) {
			ret = aw32207_charger_set_safety_vol(info,
						voltage_max_microvolt);
			if (ret) {
				dev_err(info->dev,
					"set aw32207 safety vol failed\n");
				return ret;
			}

			ret = aw32207_charger_set_safety_cur(info,
						info->cur.dcp_cur);
			if (ret) {
				dev_err(info->dev,
					"set aw32207 safety cur failed\n");
				return ret;
			}
		}

		/*ret = aw32207_update_bits(info, AW32207_REG_4,
					   AW32207_REG_RESET_MASK,
					   AW32207_REG_RESET);
		if (ret) {
			dev_err(info->dev, "reset aw32207 failed\n");
			return ret;
		}*/
		
		msleep(60); //awinic aw32207 reset need delay 50ms at least


		#if 0
		/* set ready mod */
		ret = aw32207_update_bits(info, AW32207_REG_1,
					   AW32207_REG_WEAK_VOL_THRESHOLD_MASK, 0);
		if (ret) {
			dev_err(info->dev, "set aw32207 weak voltage threshold failed\n");
			return ret;
		}
		ret = aw32207_update_bits(info, AW32207_REG_5,
					   AW32207_REG_IO_LEVEL_MASK, 0);
		if (ret) {
			dev_err(info->dev, "set aw32207 io level failed\n");
			return ret;
		}
		#endif
		
		ret = aw32207_update_bits(info, AW32207_REG_5,
					   AW32207_REG_VSP_MASK,
					   AW32207_REG_VSP);
		if (ret) {
			dev_err(info->dev, "set aw32207 vsp failed\n");
			return ret;
		}

		ret = aw32207_update_bits(info, AW32207_REG_1,
					   AW32207_REG_TERMINAL_CURRENT_MASK, 1<<AW32207_REG_TERMINAL_CURRENT_SHIFT);
		if (ret) {
			dev_err(info->dev, "set aw32207 terminal cur failed\n");
			return ret;
		}

		ret = aw32207_charger_set_termina_vol(info, voltage_max_microvolt);
		if (ret) {
			dev_err(info->dev, "set aw32207 terminal vol failed\n");
			return ret;
		}

		
		ret = aw32207_charger_set_termina_cur(info, term_current_ma);
		if (ret) {
			dev_err(info->dev, "set sy6923d terminal current failed\n");
			return ret;
		}
#if 0
		ret = aw32207_charger_set_limit_current(info,
							info->cur.unknown_cur);
		if (ret)
			dev_err(info->dev, "set aw32207 limit current failed\n");
#endif

	}

	return ret;
}

static void aw32207_dump_reg(struct aw32207_charger_info *info) {
	int i = 0;
	int ret;
	unsigned char v = 0;
	for (i = 0; i <= 0x0f; i++) {
		ret = aw32207_read(info, i, &v);
		if (ret < 0) {
			dev_err(info->dev, "read [%d] error", i);
		}
		dev_info(info->dev, "reg[%02x] = %02x\n", i, v);
	}
}

static int aw32207_charger_start_charge(struct aw32207_charger_info *info)
{
	int ret;
	dev_err(info->dev, "%s\n", __func__);
	aw32207_dump_reg(info);
	aw32207_update_bits(info, AW32207_REG_1, AW32207_REG_EN_CHARGER_MASK, 0 << AW32207_REG_EN_CHARGER_SHIFT);	
	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask, 0);
	aw32207_dump_reg(info);
	if (ret)
		dev_err(info->dev, "enable aw32207 charge failed\n");
	return ret;
}

static void aw32207_charger_stop_charge(struct aw32207_charger_info *info)
{
	int ret;
	//dev_info(info->dev, "%s\n", __func__);
	//aw32207_update_bits(info, AW32207_REG_1, AW32207_REG_EN_CHARGER_MASK, 1 << AW32207_REG_EN_CHARGER_SHIFT);	
	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask,
 				 info->charger_pd_mask);
	if (ret)
		dev_err(info->dev, "disable aw32207 charge failed\n");
	dev_info(info->dev, "charger_stop_charge ...\n");
}

static int aw32207_charger_set_current(struct aw32207_charger_info *info,
				       u32 cur)
{
	u8 reg_val;
	dev_info(info->dev, "%s:cur = %d", __func__,cur);
	if(info->gpio18_val == AW32207_GPIO18_PIN_DOWN) { 
		if (cur < 400000)
			reg_val = 0x0;
		else if (cur >= 400000 && cur <500000)
			reg_val = 0x0;
		else if (cur >= 500000 && cur < 700000)
			reg_val = 0x1;
		else if (cur >= 700000 && cur < 800000)
			reg_val = 0x2;
		else if (cur >= 800000 && cur < 900000)
			reg_val = 0x3;
		else if (cur >= 900000 && cur < 1000000)
			reg_val = 0x4;
		else if (cur >= 1000000 && cur < 1100000)
			reg_val = 0x5;
		else if (cur >= 1100000 && cur < 1200000)
			reg_val = 0x6;
		else if (cur >= 1200000 && cur < 1300000)
			reg_val = 0x7;
		else if (cur >= 1300000 && cur < 1400000)
			reg_val = 0x8;
		else if (cur >= 1400000 && cur < 1500000)
			reg_val = 0x9;
		else if (cur >= 1500000)
			reg_val = 0xa;
	} else {
		if (cur < 485000)
			reg_val = 0x0;
		else if (cur >= 485000 && cur < 607000)
			reg_val = 0x0;
		else if (cur >= 607000 && cur < 850000)
			reg_val = 0x1;
		else if (cur >= 850000 && cur < 971000)
			reg_val = 0x2;
		else if (cur >= 971000 && cur < 1092000)
			reg_val = 0x3;
		else if (cur >= 1092000 && cur < 1214000)
			reg_val = 0x4;
		else if (cur >= 1214000 && cur < 1336000)
			reg_val = 0x5;
		else if (cur >= 1336000 && cur < 1457000)
			reg_val = 0x6;
		else if (cur >= 1457000 && cur < 1578000)
			reg_val = 0x7;
		else if (cur >= 1578000 && cur < 1699000)
			reg_val = 0x8;
		else if (cur >= 1699000 && cur < 1821000)
			reg_val = 0x9;
		else if (cur >= 1821000)
			reg_val = 0xa;
	}
	return aw32207_update_bits(info, AW32207_REG_4,
				    AW32207_REG_CURRENT_MASK,
				    reg_val << AW32207_REG_CURRENT_MASK_SHIFT);
}

static int aw32207_charger_get_current(struct aw32207_charger_info *info,
				       u32 *cur)
{
	u8 reg_val;
	int ret;

	ret = aw32207_read(info, AW32207_REG_4, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= AW32207_REG_CURRENT_MASK;
	reg_val = reg_val >> AW32207_REG_CURRENT_MASK_SHIFT;
	if(info->gpio18_val == AW32207_GPIO18_PIN_DOWN) { 
		switch (reg_val) {
		case 0:
			*cur = 400000;
			break;
		case 1:
			*cur = 500000;
			break;
		case 2:
			*cur = 700000;
			break;
		case 3:
			*cur = 800000;
			break;
		case 4:
			*cur = 900000;
			break;
		case 5:
			*cur = 1000000;
			break;
		case 6:
			*cur = 1100000;
			break;
		case 7:
			*cur = 1200000;
			break;
		case 8:
			*cur = 1300000;
			break;
		case 9:
			*cur = 1400000;
			break;
		case 0xa:
			*cur = 1500000;
			break;
		default:
			*cur = 1500000;
		}
	} else {
		switch (reg_val) {
		case 0:
			*cur = 485000;
			break;
		case 1:
			*cur = 607000;
			break;
		case 2:
			*cur = 850000;
			break;
		case 3:
			*cur = 971000;
			break;
		case 4:
			*cur = 1092000;
			break;
		case 5:
			*cur = 1214000;
			break;
		case 6:
			*cur = 1336000;
			break;
		case 7:
			*cur = 1457000;
			break;
		case 8:
			*cur = 1578000;
			break;
		case 9:
			*cur = 1699000;
			break;
		case 0xa:
			*cur = 1821000;
			break;
		default:
			*cur = 1821000;
		}
	}
	return 0;
}

#if 0

static int
aw32207_charger_set_limit_current(struct aw32207_charger_info *info,
				  u32 limit_cur)
{
	u8 reg_val;
	int ret;

	if (limit_cur <= 100000)
		reg_val = 0x0;
	else if (limit_cur > 100000 && limit_cur <= 500000)
		reg_val = 0x1;
	else if (limit_cur > 500000 && limit_cur <= 800000)
		reg_val = 0x2;
	else if (limit_cur > 800000)
		reg_val = 0x3;

	ret = aw32207_update_bits(info, AW32207_REG_1,
				   AW32207_REG_LIMIT_CURRENT_MASK,
				   reg_val << AW32207_REG_LIMIT_CURRENT_SHIFT);
	if (ret)
		dev_err(info->dev, "set aw32207 limit cur failed\n");

	return ret;
}
#endif
static u32
aw32207_charger_get_limit_current(struct aw32207_charger_info *info,
				  u32 *limit_cur)
{
/*
	u8 reg_val;
	int ret;

	ret = aw32207_read(info, AW32207_REG_1, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= AW32207_REG_LIMIT_CURRENT_MASK;
	reg_val = reg_val >> AW32207_REG_LIMIT_CURRENT_SHIFT;

	switch (reg_val) {
	case 0:
		*limit_cur = 100000;
		break;
	case 1:
		*limit_cur = 500000;
		break;
	case 2:
		*limit_cur = 800000;
		break;
	case 3:
		*limit_cur = 2000000;
		break;
	default:
		*limit_cur = 100000;
	}
*/
		switch (info->usb_phy->chg_type) {
		case SDP_TYPE:
			*limit_cur = 450000;
			break;
		case DCP_TYPE:
			*limit_cur = 2000000;
			break;
		case CDP_TYPE:
			*limit_cur = 1150000;
			break;
		default:
			*limit_cur = 2000000;
		}
dev_info(info->dev, " ########type = %d",  info->usb_phy->chg_type);
	return 0;
}


static int aw32207_charger_get_health(struct aw32207_charger_info *info,
				      u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int aw32207_charger_get_online(struct aw32207_charger_info *info,
				      u32 *online)
{
	if (info->limit)
		*online = true;
	else
		*online = false;

	return 0;
}

#if 0
static int aw32207_charger_feed_watchdog(struct aw32207_charger_info *info,
					 u32 val)
{
	//int ret;

	//ret = aw32207_update_bits(info, AW32207_REG_0,
	//			   AW32207_REG_RESET_MASK, AW32207_REG_RESET);
	//if (ret)
	//	dev_err(info->dev, "reset aw32207 failed\n");

	return ret;
}
#endif

static int aw32207_charger_get_status(struct aw32207_charger_info *info)
{
	if (info->charging == true)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static int aw32207_charger_set_status(struct aw32207_charger_info *info,
				      int val)
{
	int ret = 0;

	if (!val && info->charging) {
		aw32207_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		ret = aw32207_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}

static void aw32207_charger_work(struct work_struct *data)
{
	struct aw32207_charger_info *info =
		container_of(data, struct aw32207_charger_info, work);
	int limit_cur, cur, ret;
	bool present = aw32207_charger_is_bat_present(info);

	mutex_lock(&info->lock);

	if (info->limit > 0 && !info->charging && present) {
		/* set current limitation and start to charge */
		switch (info->usb_phy->chg_type) {
		case SDP_TYPE:
			limit_cur = info->cur.sdp_limit;
			cur = info->cur.sdp_cur;
			break;
		case DCP_TYPE:
			limit_cur = info->cur.dcp_limit;
			cur = info->cur.dcp_cur;
			break;
		case CDP_TYPE:
			limit_cur = info->cur.cdp_limit;
			cur = info->cur.cdp_cur;
			break;
		default:
			limit_cur = info->cur.unknown_limit;
			cur = info->cur.unknown_cur;
		}

#if 0
		ret = aw32207_charger_set_limit_current(info, limit_cur);
		if (ret)
			goto out;
#endif
		ret = aw32207_charger_set_current(info, cur);
		if (ret)
			goto out;

		
		ret = aw32207_charger_start_charge(info);
		if (ret)
			goto out;

		info->charging = true;
	} else if ((!info->limit && info->charging) || !present) {
		/* Stop charging */
		info->charging = false;
		aw32207_charger_stop_charge(info);
	}
out:
	mutex_unlock(&info->lock);
	dev_info(info->dev, "battery present = %d, charger type = %d\n",
		 present, info->usb_phy->chg_type);
	cm_notify_event(info->psy_usb, CM_EVENT_CHG_START_STOP, NULL);
}


static int aw32207_charger_usb_change(struct notifier_block *nb,
				      unsigned long limit, void *data)
{
	struct aw32207_charger_info *info =
		container_of(nb, struct aw32207_charger_info, usb_notify);
	pr_err("%s\n",__func__);
	info->limit = limit;

	schedule_work(&info->work);
	return NOTIFY_OK;
}

static int aw32207_charger_usb_get_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    union power_supply_propval *val)
{
	struct aw32207_charger_info *info = power_supply_get_drvdata(psy);
	u32 cur, online, health;
	enum usb_charger_type type;
	int ret = 0;
	
	if (info == NULL) {
		dev_info(info->dev, " power_supply_get_drvdata EINVAL\n");
		return -EINVAL;
	}
	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (info->limit)
			val->intval = aw32207_charger_get_status(info);
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = aw32207_charger_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = aw32207_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;
			
			val->intval =cur;
			dev_info(info->dev, " ##### cur = %d",  cur);
		}
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		ret = aw32207_charger_get_online(info, &online);
		if (ret)
			goto out;

		val->intval = online;

		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = aw32207_charger_get_health(info, &health);
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

		default:
			val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		}

		break;
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		ret = aw32207_charger_get_status(info);
		if(ret == POWER_SUPPLY_STATUS_CHARGING){
			val->intval = 1;
		}else{
			val->intval = 0;
		}
		ret = 0;
		break;

	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int aw32207_charger_usb_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct aw32207_charger_info *info = power_supply_get_drvdata(psy);
	int ret;
	if (info == NULL) {
		dev_info(info->dev, " power_supply_get_drvdata EINVAL\n");
		return -EINVAL;
	}
	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = aw32207_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		dev_info(info->dev, " set input current limit = %d\n",  val->intval);
		//ret = aw32207_charger_set_limit_current(info, val->intval);
		//if (ret < 0)
		//	dev_err(info->dev, "set input current limit failed\n");
		ret = 0;
		break;

	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
	case POWER_SUPPLY_PROP_STATUS:
		ret = aw32207_charger_set_status(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;

	case POWER_SUPPLY_PROP_FEED_WATCHDOG:
		//ret = aw32207_charger_feed_watchdog(info, val->intval);
		//if (ret < 0)
		//	dev_err(info->dev, "feed charger watchdog failed\n");
		ret = 0;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = aw32207_charger_set_termina_vol(info, val->intval / 1000);
		if (ret < 0)
			dev_err(info->dev, "failed to set terminate voltage\n");
		break;

	#if 0
	case POWER_SUPPLY_PROP_VINDPM:
		ret = aw32207_charger_set_dpm_voltage(info, val->intval / 1000);
		if (ret < 0)
			dev_err(info->dev, "honor failed to set dpm voltage\n");
		break;
	#endif
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int aw32207_charger_property_is_writeable(struct power_supply *psy,
						 enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
	//case POWER_SUPPLY_PROP_VINDPM:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_usb_type aw32207_charger_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};

static enum power_supply_property aw32207_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CHARGE_ENABLED,
	//POWER_SUPPLY_PROP_VINDPM,
};

static const struct power_supply_desc aw32207_charger_desc = {
	.name			= "aw32207_charger",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= aw32207_usb_props,
	.num_properties		= ARRAY_SIZE(aw32207_usb_props),
	.get_property		= aw32207_charger_usb_get_property,
	.set_property		= aw32207_charger_usb_set_property,
	.property_is_writeable	= aw32207_charger_property_is_writeable,
	.usb_types		= aw32207_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(aw32207_charger_usb_types),
};

static void aw32207_charger_detect_status(struct aw32207_charger_info *info)
{
	int min, max;

	/*
	 * If the USB charger status has been USB_CHARGER_PRESENT before
	 * registering the notifier, we should start to charge with getting
	 * the charge current.
	 */
	if (info->usb_phy->chg_state != USB_CHARGER_PRESENT)
		return;
	pr_err("%s\n",__func__);
	usb_phy_get_charger_current(info->usb_phy, &min, &max);
	info->limit = min;
	schedule_work(&info->work);
}

#if 0
static void
aw32207_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct aw32207_charger_info *info = container_of(dwork,
							 struct aw32207_charger_info,
							 wdt_work);
	int ret;

	ret = aw32207_update_bits(info, AW32207_REG_0,
				   AW32207_REG_RESET_MASK,
				   AW32207_REG_RESET);
	if (ret) {
		dev_err(info->dev, "reset aw32207 failed\n");
		return;
	}
	schedule_delayed_work(&info->wdt_work, HZ * 15);
}
#endif

#ifdef CONFIG_REGULATOR
static void aw32207_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct aw32207_charger_info *info = container_of(dwork,
			struct aw32207_charger_info, otg_work);
	int ret;

	if (!extcon_get_state(info->edev, EXTCON_USB)) {
		ret = aw32207_update_bits(info, AW32207_REG_1,
					   AW32207_REG_HZ_MODE_MASK |
					   AW32207_REG_OPA_MODE_MASK,
					   0x01);
		if (ret)
			dev_err(info->dev, "restart aw32207 charger otg failed\n");
	}

	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(500));
}

static int aw32207_charger_enable_otg(struct regulator_dev *dev)
{
	struct aw32207_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	/*
	 * Disable charger detection function in case
	 * affecting the OTG timing sequence.
	 */
	ret = regmap_update_bits(info->pmic, info->charger_detect,
				 BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
	if (ret) {
		dev_err(info->dev, "failed to disable bc1.2 detect function.\n");
		return ret;
	}

	ret = aw32207_update_bits(info, AW32207_REG_1,
				   AW32207_REG_HZ_MODE_MASK |
				   AW32207_REG_OPA_MODE_MASK,
				   0x01);
	if (ret) {
		dev_err(info->dev, "enable aw32207 otg failed\n");
		regmap_update_bits(info->pmic, info->charger_detect,
				   BIT_DP_DM_BC_ENB, 0);
		return ret;
	}

	//schedule_delayed_work(&info->wdt_work,
	//		      msecs_to_jiffies(AW32207_FEED_WATCHDOG_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(AW32207_OTG_VALID_MS));

	return 0;
}

static int aw32207_charger_disable_otg(struct regulator_dev *dev)
{
	struct aw32207_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	//cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);
	ret = aw32207_update_bits(info, AW32207_REG_1,
				   AW32207_REG_HZ_MODE_MASK |
				   AW32207_REG_OPA_MODE_MASK,
				   0);
	if (ret) {
		dev_err(info->dev, "disable aw32207 otg failed\n");
		return ret;
	}

	/* Enable charger detection function to identify the charger type */
	return regmap_update_bits(info->pmic, info->charger_detect,
				  BIT_DP_DM_BC_ENB, 0);
}

static int aw32207_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct aw32207_charger_info *info = rdev_get_drvdata(dev);
	int ret;
	u8 val;

	ret = aw32207_read(info, AW32207_REG_1, &val);
	if (ret) {
		dev_err(info->dev, "failed to get aw32207 otg status\n");
		return ret;
	}

	val &= AW32207_REG_OPA_MODE_MASK;

	return val;
}

static const struct regulator_ops aw32207_charger_vbus_ops = {
	.enable = aw32207_charger_enable_otg,
	.disable = aw32207_charger_disable_otg,
	.is_enabled = aw32207_charger_vbus_is_enabled,
};

static const struct regulator_desc aw32207_charger_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &aw32207_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int
aw32207_charger_register_vbus_regulator(struct aw32207_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev,
				      &aw32207_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}

#else
static int
aw32207_charger_register_vbus_regulator(struct aw32207_charger_info *info)
{
	return 0;
}
#endif
				
static ssize_t aw32207_set_reg(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf,
					   size_t count)
{
	struct aw32207_charger_info *info  = dev_get_drvdata(dev);
	ssize_t ret = 0;
	u32 reg;
	u32 val;

	if (sscanf(buf, "%x %x", &reg, &val) != 2)
		return -EINVAL;
	if (reg > AW32207_REG_NUM || val > 255)
		return -EINVAL;
	ret = aw32207_write(info, (u8)reg, (u8)val);
	if (ret < 0)
		return ret;
	
	return count;
	
}
static ssize_t aw32207_get_reg(struct device *dev,
							struct device_attribute *attr,
							char *buf)
{
	struct aw32207_charger_info *info  = dev_get_drvdata(dev);
	int ret,i,len = 0;
	u8 val;
	
	for(i=0; i < AW32207_REG_NUM; i++) {
		ret = aw32207_read(info, i, &val);
		if (ret) {
			dev_err(info->dev, "failed to aw32207_read status\n");
			return ret;
		}
		len += snprintf(buf+len, PAGE_SIZE-len, "reg[0x%x] = 0x%x\n",
				i, val);
	}
	return len;
}

static DEVICE_ATTR(reg, 0664, aw32207_get_reg,  aw32207_set_reg);


static int aw32207_create_attr(struct i2c_client *client)
{
	int err;
	struct device *dev = &client->dev;
	err = device_create_file(dev, &dev_attr_reg);
	return err;
}

static int aw32207_charger_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct aw32207_charger_info *info;
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	struct gpio_desc *gpiod_Rsense;
	int ret;
  int err = 0;
	u8  aw32207_data=0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "No support for SMBUS_BYTE_DATA\n");
		//i2c_devinfo_device_write("charger_ic:0;");
		return -ENODEV;
	}

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	gpiod_Rsense = devm_gpiod_get(dev, "linux,rsense", GPIOD_IN);
	if (IS_ERR(gpiod_Rsense)) {
		dev_err(dev, "failed to get gpio18 GPIO\n");
	}
	if(gpiod_Rsense != NULL) {
		info->gpio18_val = gpiod_get_value_cansleep(gpiod_Rsense);
		dev_info(dev,"gpio18 = %d \n", info->gpio18_val);
	}
	info->client = client;
	info->dev = dev;
	aw32207_read(info, AW32207_REG_3, &aw32207_data);
	if( (aw32207_data & 0x18) != 0)
	{
		printk("dont one supplier charger ic.\n");
		devm_kfree(dev, info);
		return -ENODEV;
	}

	mutex_init(&info->lock);
	INIT_WORK(&info->work, aw32207_charger_work);

	i2c_set_clientdata(client, info);

	info->usb_phy = devm_usb_get_phy_by_phandle(dev, "phys", 0);
	if (IS_ERR(info->usb_phy)) {
		printk("failed to find USB phy\n");
		return PTR_ERR(info->usb_phy);
	}

	info->edev = extcon_get_edev_by_phandle(info->dev, 0);
	if (IS_ERR(info->edev)) {
		dev_err(dev, "failed to find vbus extcon device.\n");
		return PTR_ERR(info->edev);
	}

	ret = aw32207_charger_register_vbus_regulator(info);
	if (ret) {
		dev_err(dev, "failed to register vbus regulator.\n");
		return ret;
	}

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np) {
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

	if (of_device_is_compatible(regmap_np->parent, "sprd,sc2730"))
		info->charger_pd_mask = AW32207_DISABLE_PIN_MASK_2730;
	else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
		info->charger_pd_mask = AW32207_DISABLE_PIN_MASK_2721;
	else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2720"))
		info->charger_pd_mask = AW32207_DISABLE_PIN_MASK_2720;
	else {
		dev_err(dev, "failed to get charger_pd mask\n");
		return -EINVAL;
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

	info->usb_notify.notifier_call = aw32207_charger_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		dev_err(dev, "failed to register notifier:%d\n", ret);
		return ret;
	}

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	info->psy_usb = devm_power_supply_register(dev,
						   &aw32207_charger_desc,
						   &charger_cfg);
	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		usb_unregister_notifier(info->usb_phy, &info->usb_notify);
		return PTR_ERR(info->psy_usb);
	}

	ret = aw32207_charger_hw_init(info);
	if (ret) {
		usb_unregister_notifier(info->usb_phy, &info->usb_notify);
		return ret;
	}
	aw32207_charger_detect_status(info);
	INIT_DELAYED_WORK(&info->otg_work, aw32207_charger_otg_work);
	//INIT_DELAYED_WORK(&info->wdt_work,
	//		  aw32207_charger_feed_watchdog_work);
	//i2c_devinfo_device_write("charger_ic:1;");
  
  err = aw32207_create_attr(client);
	if (err) {
		pr_err("create attribute err = %d\n", err);
		return err;
	}
	printk("%s:successful\n",__func__);
	return 0;
}

static int aw32207_charger_remove(struct i2c_client *client)
{
	struct aw32207_charger_info *info = i2c_get_clientdata(client);

	usb_unregister_notifier(info->usb_phy, &info->usb_notify);

	return 0;
}
static void aw32207_charger_shutdown(struct i2c_client *client)
{
	int ret = 0;
	struct aw32207_charger_info *info = i2c_get_clientdata(client);
	ret = aw32207_charger_start_charge(info);
	dev_info(info->dev,"charger_shutdown running...\n");
	if (ret) {
		dev_err(info->dev, "enable charger failed\n");
	}
}

static const struct i2c_device_id aw32207_i2c_id[] = {
	{"aw32207_chg", 0},
	{}
};

static const struct of_device_id aw32207_charger_of_match[] = {
	{ .compatible = "awinic,aw32207_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, aw32207_charger_of_match);

static struct i2c_driver aw32207_charger_driver = {
	.driver = {
		.name = "aw32207_chg",
		.of_match_table = aw32207_charger_of_match,
	},
	.probe = aw32207_charger_probe,
	.remove = aw32207_charger_remove,
	.shutdown = aw32207_charger_shutdown,
	.id_table = aw32207_i2c_id,
};

static int32_t __init aw32207_driver_init(void)
{
	int32_t ret = 0;

	if ((chg_ic != NULL) && strncmp("0", chg_ic, 1) == 0) {
		ret = i2c_add_driver(&aw32207_charger_driver);
		if (ret) {
			printk("%s, driver register failed\n", __func__);
			goto err_driver;
		}
	} else {
		printk("first supplier not match");
		goto err_driver;
	}
	printk("charger, finished...\n");

err_driver:
	return ret;
}

static void __exit aw32207_driver_exit(void)
{
	if ((chg_ic != NULL) && strncmp("0", chg_ic, 1) == 0) {
		i2c_del_driver(&aw32207_charger_driver);
	}
}

//module_i2c_driver(aw32207_charger_driver);
module_init(aw32207_driver_init);
module_exit(aw32207_driver_exit);
MODULE_DESCRIPTION("AW32207 Charger Driver");
MODULE_LICENSE("GPL v2");
