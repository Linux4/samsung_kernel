/*
 * Battery driver for Marvell 88PM80x PMIC
 *
 * Copyright (c) 2012 Marvell International Ltd.
 * Author:	Yi Zhang <yizhang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/mfd/88pm80x.h>
#include <linux/delay.h>

#define MONITOR_INTERVAL		(HZ * 60)

#define BAT_WU_LOG			(1<<6)
#define uAh_to_uWh(val)		(val * 37 / 10)	/* Nominal voltage: 3.7v */

static int external_chg;
static int who_is_chg;

struct pm80x_battery_params {
	int status;
	int present;
	int volt;	/* µV */
	int cap;	/* percents: 0~100% */
	int health;
	int tech;
	int temp;
	int chg_full;
	int chg_now;
	int eng_full;
	int eng_now;
};

struct pm80x_battery_info {
	struct pm80x_chip	*chip;
	struct device	*dev;
	struct pm80x_battery_params	bat_params;

	struct power_supply	battery;
	struct delayed_work	monitor_work;
	struct delayed_work	charged_work;
	struct workqueue_struct *bat_wqueue;

	unsigned	present:1;
	int irq;
};

struct capacity_percent {
	int voltage;
	int percent;
};

/* The percent to be reported */
static int cap_data;

/* The voltage to be reported */
static int  vol_data;

static char *pm80x_supply_to[] = {
	"ac",
	"usb",
};

/*
 * State of Charge.
 * The first number is voltage, the second number is percent point.
 */
struct capacity_percent cap_table[] = {
	{4170, 100}, {4154, 99}, {4136, 98}, {4122, 97}, {4107, 96},
	{4102, 95}, {4088, 94}, {4081, 93}, {4070, 92}, {4060, 91},
	{4053, 90}, {4044, 89}, {4035, 88}, {4028, 87}, {4019, 86},
	{4013, 85}, {4006, 84}, {3995, 83}, {3987, 82}, {3982, 81},
	{3976, 80}, {3968, 79}, {3962, 78}, {3954, 77}, {3946, 76},
	{3941, 75}, {3934, 74}, {3929, 73}, {3922, 72}, {3916, 71},
	{3910, 70}, {3904, 69}, {3898, 68}, {3892, 67}, {3887, 66},
	{3880, 65}, {3874, 64}, {3868, 63}, {3862, 62}, {3854, 61},
	{3849, 60}, {3843, 59}, {3840, 58}, {3833, 57}, {3829, 56},
	{3824, 55}, {3818, 54}, {3815, 53}, {3810, 52}, {3808, 51},
	{3804, 50}, {3801, 49}, {3798, 48}, {3796, 47}, {3792, 46},
	{3789, 45}, {3785, 44}, {3784, 43}, {3782, 42}, {3780, 41},
	{3777, 40}, {3776, 39}, {3774, 38}, {3772, 37}, {3771, 36},
	{3769, 35}, {3768, 34}, {3764, 33}, {3763, 32}, {3760, 31},
	{3760, 30}, {3754, 29}, {3750, 28}, {3749, 27}, {3744, 26},
	{3740, 25}, {3734, 24}, {3732, 23}, {3728, 22}, {3726, 21},
	{3720, 20}, {3716, 19}, {3709, 18}, {3703, 17}, {3698, 16},
	{3692, 15}, {3683, 14}, {3675, 13}, {3670, 12}, {3665, 11},
	{3661, 10}, {3657, 9},  {3649, 8},  {3637, 7},  {3622, 6},
	{3609, 5},  {3580, 4},  {3558, 3},  {3540, 2},  {3510, 1},  {3429, 0} };

static int is_charger_online(struct pm80x_battery_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int i, ret = 0;

	for (i = 0; i < info->battery.num_supplicants; i++) {
		psy = power_supply_get_by_name(info->battery.supplied_to[i]);
		if (!psy || !psy->get_property)
			continue;
		ret = psy->get_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret == 0) {
			if (val.intval) {
				who_is_chg = i;
				return val.intval;
			}
		}
	}

	return 0;
}

/*
 * register 1 bit[7:0] -- bit[11:4] of measured value of voltage
 * register 0 bit[3:0] -- bit[3:0] of measured value of voltage
 */
static int get_batt_vol(struct pm80x_battery_info *info, int *data, int slp_en)
{
	int ret;
	unsigned char buf[2];
	if (!data)
		return -EINVAL;

	if (slp_en) {
		ret = regmap_bulk_read(info->chip->subchip->regmap_gpadc,
				       PM800_VBAT_MEAS1, buf, 2);
		if (ret < 0)
			return ret;
	} else {
		ret = regmap_bulk_read(info->chip->subchip->regmap_gpadc,
				       PM800_VBAT_SLP, buf, 2);
		if (ret < 0)
			return ret;
	}

	*data = ((buf[0] & 0xff) << 4) | (buf[1] & 0x0f);
	/* measure(mv) = value * 4 * 1.4 *1000/(2^12) */
	*data = ((*data & 0xfff) * 7 * 100) >> 9;

	return 0;
}

#define SMOOTH_CNT	(16)
static int calc_avg_vol(struct pm80x_battery_info *info, int *vol)
{
	int i, act_vol, slp_vol;
	int act_sum = 0, slp_sum = 0;
	int ret = -EINVAL;

	if (!vol)
		return ret;

	for (i = 0; i < SMOOTH_CNT; i++) {
		ret = get_batt_vol(info, &act_vol, 1);
		if (ret)
			return -EINVAL;
		act_sum += act_vol;
	}

	if (!is_charger_online(info)) {
		for (i = 0; i < SMOOTH_CNT; i++) {
			ret = get_batt_vol(info, &slp_vol, 0);
			if (ret)
				return -EINVAL;
			slp_sum += slp_vol;
		}
		*vol = (((slp_sum*7 + act_sum*3))/10) >> 4;
	} else
		*vol = act_sum >> 4;

	return 0;
}

/* Calculate State of Charge (percent points) */
static int calc_cap(struct pm80x_battery_info *info, int *percent)
{
	int i, batt_vol, count, ret = -EINVAL;

	if (!percent)
		return ret;
	calc_avg_vol(info, &batt_vol);
	pr_debug("%s: batt_vol = %d\n", __func__, batt_vol);

	count = ARRAY_SIZE(cap_table);
	if (batt_vol < cap_table[count - 1].voltage) {
		*percent = 0;
		return 0;
	}

	for (i = 0; i < count; i++) {
		if (batt_vol >= cap_table[i].voltage) {
			*percent = cap_table[i].percent;
			break;
		}
	}

	if ((*percent <= 100) && (*percent >= 97))
		*percent = 100;
	else if (*percent >= 95)
		*percent = 95;
	else if (*percent >= 90)
		*percent = 90;
	else if (*percent >= 80)
		*percent = 85;
	else if (*percent >= 70)
		*percent = 75;
	else if (*percent >= 50)
		*percent = 55;
	else if (*percent >= 30)
		*percent = 35;
	else if (*percent >= 10)
		*percent = 20;
	else if (*percent >= 5)
		*percent = 5;
	else
		*percent = 0;
	/*
	 * store the capacity to RTC domain register,
	 * after next power up, it will be restored
	 */
	regmap_write(info->chip->regmap, PM800_USER_DATA1, *percent);
	return 0;
}

static int pm80x_is_bat_present(struct pm80x_battery_info *info)
{
	u8 chg_online;
	int ret;
	struct power_supply *psy;
	union power_supply_propval val;

	chg_online = is_charger_online(info);
	if (chg_online) {
		psy = power_supply_get_by_name(
			info->battery.supplied_to[who_is_chg]);
		if (!psy || !psy->get_property) {
			info->present = 1;
			return info->present;
		}
		ret = psy->get_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
		if (ret == 0) {
			switch (val.intval) {
			case POWER_SUPPLY_STATUS_CHARGING:
				info->present = 1;
				break;
			case POWER_SUPPLY_STATUS_NOT_CHARGING:
				if (info->bat_params.status ==
				    POWER_SUPPLY_STATUS_FULL)
					info->present = 1;
				else
					info->present = 0;
				break;
			default:
				pr_err("charger status error: %d\n",
				       val.intval);
				info->present = 0;
				break;
			}
		}
	} else
		info->present = 1;
	return info->present;
}

static int pm80x_get_battery_temp(struct pm80x_battery_info *info)
{
	int temp, ret;
	unsigned char buf[2];
	/*
	 * There is no way to measure the temperature of battery,
	 * report the pmic temperature value
	 */
	ret = regmap_bulk_read(info->chip->subchip->regmap_gpadc,
			      PM800_TINT_MEAS1, buf, 2);
	if (ret < 0)
		return ret;

	temp = ((buf[0] & 0xff) << 4) | (buf[1] & 0x0f);
	/* temp = value * 1.4 *1000/(2^12) */
	temp = ((temp & 0xfff) * 7 * 100) >> 11;
	temp = (temp - 884) * 1000 / 3611;

	return temp;
}

/* Update battery status */
static void pm80x_bat_update_status(struct pm80x_battery_info *info)
{
	/* NOTE: hardcode battery type[Lion] and health[Good] */
	info->bat_params.health = POWER_SUPPLY_HEALTH_GOOD;
	info->bat_params.tech = POWER_SUPPLY_TECHNOLOGY_LION;
	info->bat_params.temp = pm80x_get_battery_temp(info);
	/* Battery presence state */
	info->bat_params.present = pm80x_is_bat_present(info);

	/*
	 * Report volt, cap, chg, eng info
	 * when charger not change,
	 * in case voltage surge
	 */
	if (external_chg == 0) {
		/* Voltage */
		calc_avg_vol(info, &vol_data);
		info->bat_params.volt = vol_data;

		/* Capacity: % */
		calc_cap(info, &cap_data);
		info->bat_params.cap = cap_data;
		if (info->bat_params.present)
			pr_debug("%s: cap_data = %d\n", __func__, cap_data);

	}

	/* Charging status */
	if (info->bat_params.present) {
		/* Report charger online timely */
		if (power_supply_am_i_supplied(&info->battery))
			info->bat_params.status = POWER_SUPPLY_STATUS_CHARGING;
		else
			info->bat_params.status =
				POWER_SUPPLY_STATUS_DISCHARGING;
		if (info->bat_params.cap >= 100)
			info->bat_params.status = POWER_SUPPLY_STATUS_FULL;

	} else
		info->bat_params.status = POWER_SUPPLY_STATUS_UNKNOWN;
}

static void pm80x_battery_work(struct work_struct *work)
{
	struct pm80x_battery_info *info =
		container_of(work, struct pm80x_battery_info,
			     monitor_work.work);
	external_chg = 0;

	pm80x_bat_update_status(info);
	power_supply_changed(&info->battery);
	queue_delayed_work(info->bat_wqueue, &info->monitor_work,
			   MONITOR_INTERVAL);
}

static void pm80x_charged_work(struct work_struct *work)
{
	struct pm80x_battery_info *info =
		container_of(work, struct pm80x_battery_info,
			     charged_work.work);
	external_chg = 1;
	pm80x_bat_update_status(info);
	power_supply_changed(&info->battery);
	/* NO need to be scheduled again */
	return;
}

static irqreturn_t pm80x_bat_handler(int irq, void *data)
{
	struct pm80x_battery_info *info = data;
	dev_dbg(info->dev,
		"%s is triggered!\n", __func__);
	/*
	 * report the battery status
	 * when the voltage is abnormal
	 */
	pm80x_bat_update_status(info);
	return IRQ_HANDLED;
}

static void pm80x_init_battery(struct pm80x_battery_info *info)
{
	int data = 0;
	int bat_remove;

	/*
	 * enable VBAT
	 * VBAT is used to measure voltage
	 */
	regmap_read(info->chip->subchip->regmap_gpadc,
		    PM800_GPADC_MEAS_EN1, &data);
	data |= PM800_MEAS_EN1_VBAT;
	regmap_write(info->chip->subchip->regmap_gpadc,
		     PM800_GPADC_MEAS_EN1, data);

	/* set VBAT low threshold as 3.5V */
	regmap_write(info->chip->subchip->regmap_gpadc,
		     PM800_VBAT_LOW_TH, 0xa0);

	/* check whether battery present */
	info->bat_params.present = pm80x_is_bat_present(info);
	pr_info("%s: 88pm80x battery present: %d\n", __func__,
		info->bat_params.present);

	if (info->bat_params.present) {
		/* check if battery plug in/out or not */
		regmap_read(info->chip->regmap, PM800_POWER_UP_LOG, &data);

		bat_remove = data & BAT_WU_LOG;
		dev_dbg(info->dev, "battery wake up? %s\n",
			bat_remove != 0 ? "yes" : "no");
		if (bat_remove == 0) {
			/* restore capacity */
			regmap_read(info->chip->regmap,
				    PM800_USER_DATA1, &data);
			calc_cap(info, &cap_data);
			if ((data > cap_data + 15) ||
			    (data < cap_data - 15))
				info->bat_params.cap = cap_data;
			else
				info->bat_params.cap = data;
		} else {
			calc_avg_vol(info, &vol_data);
			info->bat_params.volt = vol_data;
			calc_cap(info, &cap_data);
			info->bat_params.cap = cap_data;
		}
	}
	return;
}

static void pm80x_external_power_changed(struct power_supply *psy)
{
	struct pm80x_battery_info *info;
	external_chg = 1;

	info = container_of(psy, struct pm80x_battery_info, battery);
	queue_delayed_work(info->bat_wqueue,
			   &info->charged_work, HZ);
	return;
}

static int pm80x_batt_get_prop(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	struct pm80x_battery_info *info = dev_get_drvdata(psy->dev->parent);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = info->bat_params.status;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = info->bat_params.present;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		/* report fake capacity without battery */
		if (!info->bat_params.present)
			info->bat_params.cap = 80;
		val->intval = info->bat_params.cap;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = info->bat_params.tech;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = info->bat_params.volt;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (!info->bat_params.present)
			info->bat_params.temp = 25;
		val->intval = info->bat_params.temp;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		info->bat_params.chg_full = 1500 * 1000;
		val->intval = info->bat_params.chg_full;	/* uAh */
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		if (info->bat_params.present)
			info->bat_params.chg_now =
				15000 * info->bat_params.cap;
		else
			info->bat_params.chg_now = 15000 * 80;
		val->intval = info->bat_params.chg_now;	/* uAh */
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL:
		info->bat_params.eng_full = uAh_to_uWh(1500 * 1000);
		val->intval = info->bat_params.eng_full;
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		if (info->bat_params.present)
			info->bat_params.eng_now =
				uAh_to_uWh(info->bat_params.chg_now);
		else
			info->bat_params.eng_now =
				uAh_to_uWh(15000 * 80);
		val->intval = info->bat_params.eng_now;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = info->bat_params.health;
		break;
	default:
		return -ENODEV;
	}
	return 0;
}

static enum power_supply_property pm80x_batt_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_ENERGY_FULL,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_HEALTH,
};

static __devinit int pm80x_battery_probe(struct platform_device *pdev)
{
	struct pm80x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm80x_battery_info *info;
	int irq, ret;

	info = kzalloc(sizeof(struct pm80x_battery_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->chip = chip;
	info->dev = &pdev->dev;
	info->bat_params.status = POWER_SUPPLY_STATUS_UNKNOWN;

	platform_set_drvdata(pdev, info);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "No IRQ resource!\n");
		ret = -EINVAL;
		goto out;
	}

	info->irq = irq + chip->irq_base;

	pm80x_init_battery(info);

	info->battery.name = "battery";
	info->battery.type = POWER_SUPPLY_TYPE_BATTERY;
	info->battery.properties = pm80x_batt_props;
	info->battery.num_properties = ARRAY_SIZE(pm80x_batt_props);
	info->battery.get_property = pm80x_batt_get_prop;
	info->battery.external_power_changed = pm80x_external_power_changed;
	info->battery.supplied_to = pm80x_supply_to;
	info->battery.num_supplicants = ARRAY_SIZE(pm80x_supply_to);

	ret = power_supply_register(&pdev->dev, &info->battery);
	if (ret)
		goto out;
	info->battery.dev->parent = &pdev->dev;

	ret = pm80x_request_irq(info->chip, info->irq, pm80x_bat_handler,
				IRQF_ONESHOT, "battery", info);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to request IRQ: #%d: %d\n",
			info->irq, ret);
		goto out;
	}

	info->bat_wqueue = create_singlethread_workqueue("bat-88pm800");
	if (!info->bat_wqueue) {
		dev_info(chip->dev,
			"[%s]Failed to create bat_wqueue\n", __func__);
		ret = -ESRCH;
		goto out;
	}

	INIT_DELAYED_WORK_DEFERRABLE(&info->monitor_work, pm80x_battery_work);
	INIT_DELAYED_WORK(&info->charged_work, pm80x_charged_work);
	queue_delayed_work(info->bat_wqueue, &info->monitor_work,
			   MONITOR_INTERVAL);

	device_init_wakeup(&pdev->dev, 1);
	return 0;

out:
	power_supply_unregister(&info->battery);
	kfree(info);
	return ret;
}

static int __devexit pm80x_battery_remove(struct platform_device *pdev)
{
	struct pm80x_battery_info *info = platform_get_drvdata(pdev);

	cancel_delayed_work(&info->monitor_work);
	cancel_delayed_work(&info->charged_work);
	flush_workqueue(info->bat_wqueue);
	power_supply_unregister(&info->battery);
	kfree(info);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_PM
static int pm80x_battery_suspend(struct device *dev)
{
	struct pm80x_battery_info *info = dev_get_drvdata(dev);
	cancel_delayed_work_sync(&info->monitor_work);
	return pm80x_dev_suspend(dev);
}

static int pm80x_battery_resume(struct device *dev)
{
	struct pm80x_battery_info *info = dev_get_drvdata(dev);
	calc_cap(info, &cap_data);
	queue_delayed_work(info->bat_wqueue, &info->monitor_work,
			   MONITOR_INTERVAL);

	return pm80x_dev_resume(dev);
}

static const struct dev_pm_ops pm80x_battery_pm_ops = {
	.suspend	= pm80x_battery_suspend,
	.resume		= pm80x_battery_resume,
};
#endif

static struct platform_driver pm80x_battery_driver = {
	.driver		= {
		.name	= "88pm80x-bat",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &pm80x_battery_pm_ops,
#endif
	},
	.probe		= pm80x_battery_probe,
	.remove		= __devexit_p(pm80x_battery_remove),
};

static int __init pm80x_battery_init(void)
{
	return platform_driver_register(&pm80x_battery_driver);
}
module_init(pm80x_battery_init);

static void __exit pm80x_battery_exit(void)
{
	platform_driver_unregister(&pm80x_battery_driver);
}
module_exit(pm80x_battery_exit);

MODULE_DESCRIPTION("Marvell 88PM80x Battery driver");
MODULE_LICENSE("GPL");
