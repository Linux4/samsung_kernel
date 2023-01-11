/*
 * gpadc_thermal.c - thermistor thermal management
 *
 * Author:      Feng Hong <hongfeng@marvell.com>
 *		Yi Zhang <yizhang@marvell.com>
 * Copyright:   (C) 2014 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/thermal.h>
#include <linux/cpufreq.h>
#include <linux/cpu_cooling.h>
#include <linux/of.h>
#include <linux/mfd/88pm80x.h>

/*
 * -40 ~ 125: step -> 1C
 * resistor value multiplied by 1000
 */
static int temp_table[] = {
	/* [-40, -36]*/
	1747919, 1631671, 1523950, 1424075, 1331424,
	1245427, 1165564, 1091357, 1022369, 958201,
	/* [-30, -26]*/
	898485, 842883, 791087, 742813, 697798,
	655802, 616604, 580001, 545804, 513842,
	/* [-20, -16]*/
	483953, 455992, 429821, 405317, 382362,
	360849, 340680, 321762, 304010, 287346,
	/* [-10, -6]*/
	271697, 256994, 243176, 230183, 217962,
	206463, 195624, 185419, 175807, 166751,
	/* [0, 4]*/
	158214, 150165, 142572, 135408, 128645,
	122259, 116227, 110527, 105139, 100045,
	/* [10, 14]*/
	95226, 90667, 86351, 82266, 78396,
	74730, 71255, 67962, 64838, 61875,
	/* [20, 24]*/
	59064, 56396, 53862, 51456, 49171,
	47000, 44936, 42973, 41107, 39332,
	/* [30, 34]*/
	37643, 36035, 34504, 33046, 31657,
	30333, 29073, 27871, 26726, 25633,
	/* [40, 44]*/
	24590, 23596, 22646, 21740, 20874,
	20047, 19258, 18503, 17781, 17092,
	/* [50, 54]*/
	16432, 15801, 15198, 14620, 14067,
	13538, 13031, 12546, 12081, 11636,
	/* [60, 64]*/
	11209, 10800, 10409, 10033, 9673,
	9327, 8996, 8678, 8372, 8079,
	/* [70, 74]*/
	7797, 7526, 7266, 7016, 6775,
	6544, 6322, 6109, 5904, 5707,
	/* [80, 84]*/
	5517, 5335, 5160, 4992, 4829,
	4673, 4522, 4377, 4236, 4101,
	/* [90, 94]*/
	3971, 3846, 3725, 3608, 3496,
	3387, 3283, 3183, 3086, 2992,
	/* [100, 104]*/
	2901, 2814, 2730, 2648, 2570,
	2494, 2420, 2349, 2280, 2213,
	/* [110, 114]*/
	2149, 2087, 2027, 1969, 1913,
	1859, 1807, 1757, 1708, 1661,
	/* [120, 124]*/
	1615, 1570, 1527, 1485, 1445,
};

enum trip_points {
	TRIP_0,
	TRIP_1,
	TRIP_2,
	TRIP_3,
	TRIP_POINTS_NUM,
	TRIP_POINTS_ACTIVE_NUM = TRIP_POINTS_NUM - 1,
};

struct uevent_msg_priv {
	int cur_s;
	int last_s;
};

struct gpadc_thermal_device {
	struct thermal_zone_device *therm_adc;
	int temp_adc;
	struct uevent_msg_priv msg_s[TRIP_POINTS_ACTIVE_NUM];
	int hit_trip_cnt[TRIP_POINTS_NUM];
};

static struct gpadc_thermal_device adc_dev;
static int adc_trips_temp[TRIP_POINTS_NUM] = {
	40000, /* TRIP_0 */
	50000, /* TRIP_1 */
	55000, /* TRIP_2 */
	100000, /* TRIP_3, set critical 100C for don't enable it currently */
};

static int hit_trip_status_get(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i;
	int ret = 0;
	for (i = 0; i < TRIP_POINTS_NUM; i++) {
		ret += sprintf(buf + ret, "trip %d: %d hits\n",
				adc_trips_temp[i],
				adc_dev.hit_trip_cnt[i]);
	}
	return ret;
}
static DEVICE_ATTR(hit_trip_status, 0444, hit_trip_status_get, NULL);

static struct attribute *thermal_attrs[] = {
	&dev_attr_hit_trip_status.attr,
	NULL,
};
static struct attribute_group thermal_attr_grp = {
	.attrs = thermal_attrs,
};

static void gpadc_set_interval(int ms)
{
	(adc_dev.therm_adc)->polling_delay = ms;
}

/* step: the temperature step */
static int get_thermal_temp(int gp_id, unsigned int step, int lowest)
{
	int temp, tbat, volt, i;
	unsigned int bias_current[5] = {31, 61, 16, 11, 6};

	/*
	 *1) set bias as 31uA firstly for room temperature,
	 *2) check the voltage whether it's in [0.3V, 1.25V];
	 *   if yes, tbat = tbat/31; break;
	 *   else
	 *   set bias as 61uA/16uA/11uA/6uA...
	 *   for lower or higher temperature;
	 */
	for (i = 0; i < ARRAY_SIZE(bias_current); i++) {
		volt = extern_get_gpadc_bias_volt(gp_id, bias_current[i]);
		if ((volt > 300) && (volt < 1250)) {
			volt *= 1000;
			tbat = volt / bias_current[i];
			break;
		}
	}

	/* report the fake value 25C */
	if (i == ARRAY_SIZE(bias_current)) {
		pr_debug("thermal fake raw temp is 25C\n");
		temp = 25;
		return temp;
	}
	pr_debug("%s: tbat = %dKohm, i = %d\n", __func__, tbat, i);

	for (i = 0; i < ARRAY_SIZE(temp_table); i++) {
		if (tbat >= temp_table[i]) {
			temp = lowest + i * step;
			break;
		}
	}

	/* max temperature */
	if (i == ARRAY_SIZE(temp_table))
		temp = temp_table[i - 1];

	pr_debug("raw temperature is %dC\n", temp);

	return temp;
}

static int gpadc_sys_get_temp(struct thermal_zone_device *thermal,
		unsigned long *temp)
{
	int ret = 0;
	struct gpadc_thermal_device *t_dev = &adc_dev;
	char *temp_info[3]    = { "TYPE=thsens_adc", "TEMP=10000", NULL };
	int mon_interval;

	t_dev->temp_adc = get_thermal_temp(3, 1, -40);
	*temp = t_dev->temp_adc * 1000;

	if (t_dev->therm_adc) {
		if (t_dev->temp_adc >= adc_trips_temp[TRIP_2]) {
			t_dev->hit_trip_cnt[TRIP_2]++;
			t_dev->msg_s[TRIP_2].cur_s = 1;
			t_dev->msg_s[TRIP_1].cur_s = 1;
			t_dev->msg_s[TRIP_0].cur_s = 1;
			mon_interval = 2000;
		} else if ((t_dev->temp_adc >= adc_trips_temp[TRIP_1]) &&
				(t_dev->temp_adc < adc_trips_temp[TRIP_2])) {
			t_dev->hit_trip_cnt[TRIP_1]++;
			t_dev->msg_s[TRIP_2].cur_s = 0;
			t_dev->msg_s[TRIP_1].cur_s = 1;
			t_dev->msg_s[TRIP_0].cur_s = 1;
			mon_interval = 3000;
		} else if ((t_dev->temp_adc >= adc_trips_temp[TRIP_0]) &&
				(t_dev->temp_adc < adc_trips_temp[TRIP_1])) {
			t_dev->hit_trip_cnt[TRIP_0]++;
			t_dev->msg_s[TRIP_2].cur_s = 0;
			t_dev->msg_s[TRIP_1].cur_s = 0;
			t_dev->msg_s[TRIP_0].cur_s = 1;
			mon_interval = 4000;
		} else {
			t_dev->msg_s[TRIP_2].cur_s = 0;
			t_dev->msg_s[TRIP_1].cur_s = 0;
			t_dev->msg_s[TRIP_0].cur_s = 0;
			mon_interval = 5000;
		}

		if ((t_dev->msg_s[TRIP_2].cur_s !=
			t_dev->msg_s[TRIP_2].last_s) ||
			(t_dev->msg_s[TRIP_1].cur_s !=
			 t_dev->msg_s[TRIP_1].last_s) ||
			(t_dev->msg_s[TRIP_0].cur_s !=
			 t_dev->msg_s[TRIP_0].last_s)) {
			gpadc_set_interval(mon_interval);
			t_dev->msg_s[TRIP_2].last_s =
				t_dev->msg_s[TRIP_2].cur_s;
			t_dev->msg_s[TRIP_1].last_s =
				t_dev->msg_s[TRIP_1].cur_s;
			t_dev->msg_s[TRIP_0].last_s =
				t_dev->msg_s[TRIP_0].cur_s;
			pr_info("board adc thermal %dC\n",
					t_dev->temp_adc / 1000);
			sprintf(temp_info[1], "TEMP=%d", t_dev->temp_adc);
			/* TODO notify user for trip point cross */
			/*
			kobject_uevent_env(&((t_dev->therm_adc)->
				device.kobj), KOBJ_CHANGE, temp_info);
			*/
		}
	}
	return ret;
}

static int gpadc_sys_get_trip_type(struct thermal_zone_device *thermal,
		int trip, enum thermal_trip_type *type)
{
	if ((trip >= 0) && (trip < TRIP_POINTS_ACTIVE_NUM))
		*type = THERMAL_TRIP_ACTIVE;
	else if (TRIP_POINTS_ACTIVE_NUM == trip)
		*type = THERMAL_TRIP_CRITICAL;
	else
		*type = (enum thermal_trip_type)(-1);
	return 0;
}

static int gpadc_sys_get_trip_temp(struct thermal_zone_device *thermal,
		int trip, unsigned long *temp)
{
	if ((trip >= 0) && (trip < TRIP_POINTS_NUM))
		*temp = adc_trips_temp[trip];
	else
		*temp = -1;
	return 0;
}

static int gpadc_sys_set_trip_temp(struct thermal_zone_device *thermal,
		int trip, unsigned long temp)
{
	if ((trip >= 0) && (trip < TRIP_POINTS_NUM))
		adc_trips_temp[trip] = temp;
	return 0;
}

static int gpadc_sys_get_crit_temp(struct thermal_zone_device *thermal,
		unsigned long *temp)
{
	return adc_trips_temp[TRIP_POINTS_NUM - 1];
}

static struct thermal_zone_device_ops adc_thermal_ops = {
	.get_temp = gpadc_sys_get_temp,
	.get_trip_type = gpadc_sys_get_trip_type,
	.get_trip_temp = gpadc_sys_get_trip_temp,
	.set_trip_temp = gpadc_sys_set_trip_temp,
	.get_crit_temp = gpadc_sys_get_crit_temp,
};

#ifdef CONFIG_PM_SLEEP
static int thermal_suspend(struct device *dev)
{
	return 0;
}

static int thermal_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(thermal_pm_ops,
		thermal_suspend, thermal_resume);
#define PXA_TMU_PM      (&thermal_pm_ops)
#else
#define PXA_TMU_PM      NULL
#endif

static int gpadc_register_thermal(void)
{
	/*struct cpumask mask_val;*/
	int i, trip_w_mask = 0;
	int tmp = 0;

	for (i = 0; i < TRIP_POINTS_NUM; i++)
		trip_w_mask |= (1 << i);
	adc_dev.therm_adc = thermal_zone_device_register("thsens_adc",
			TRIP_POINTS_NUM, trip_w_mask, NULL,
			&adc_thermal_ops, NULL, 0, 5000);
	if (IS_ERR(adc_dev.therm_adc)) {
		pr_err("Failed to register board thermal zone device\n");
		return PTR_ERR(adc_dev.therm_adc);
	}

	tmp = sysfs_create_group(&((adc_dev.therm_adc->device).kobj),
			&thermal_attr_grp);
	if (tmp < 0)
		pr_err("Failed to register private adc thermal interface\n");

	return 0;
}

static int gpadc_thermal_probe(struct platform_device *pdev)
{
	/*
	 * init thermal framework
	 * all of initialization has been done in mfd driver;
	 * so currently, we need to make sure this driver is later
	 * than PMIC driver;
	 */
	return gpadc_register_thermal();
}

static int gpadc_thermal_remove(struct platform_device *pdev)
{
	if (adc_dev.therm_adc)
		thermal_zone_device_unregister(adc_dev.therm_adc);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id gpadc_tmu_match[] = {
	{ .compatible = "marvell,gpadc-thermal", },
	{},
};
MODULE_DEVICE_TABLE(of, gpadc_tmu_match);
#endif

static struct platform_driver gpadc_thermal_driver = {
	.driver = {
		.name   = "gpadc-thermal",
		.pm     = PXA_TMU_PM,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(gpadc_tmu_match),
#endif
	},
	.probe = gpadc_thermal_probe,
	.remove = gpadc_thermal_remove,
};
module_platform_driver(gpadc_thermal_driver);

MODULE_AUTHOR("Marvell Semiconductor");
MODULE_DESCRIPTION("GPADC thermal driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:gpadc-thermal");
