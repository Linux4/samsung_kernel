// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 *
 * Filename:
 * ---------
 *    mtk_charger.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines functions of Battery charging
 *
 * Author:
 * -------
 * Wy Chuang
 *
 */
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/reboot.h>
/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210511 start */
#if !defined(HQ_FACTORY_BUILD)
#include<linux/timer.h>
#include<linux/timex.h>
#include<linux/rtc.h>
#endif
/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210511 end */

#include "mtk_charger.h"

/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 start */
#if !defined(HQ_FACTORY_BUILD)
/*hs04_u for  AL6398AU-178 by shixuanxuan at 20240117 start*/
#ifdef CONFIG_HQ_PROJECT_HS04
#include "gxy_battery_ttf_hs04.h"
#endif
/*hs04_u for  AL6398AU-178 by shixuanxuan at 20240117 end*/
#define TSUSPEND 259200// 3days=259200s
#define ENABLE_BATT_PROTECT 1
#define DISABLE_BATT_PROTECT 0
#define ENABLE_SCHARGING 0
#define DISABLE_SCHARGING 1
#define ENABLE_BATT_CHARGING 1
#define DISABLE_BATT_CHARGING 0
#define HIGHT_CAPACITY 75
#define LOW_CAPACITY 55
#define CHR_COUNT_TIME 60000 // interval time 60s
#endif
/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 end */
/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 start */
#ifdef CONFIG_HQ_PROJECT_OT8
#include "gxy_battery_ttf.h"
struct mtk_charger *ssinfo = NULL;
EXPORT_SYMBOL(ssinfo);
#ifndef HQ_FACTORY_BUILD
typedef enum batt_full_state {
	ENABLE_CHARGE,
	DISABLE_CHARGE,
	PLUG_OUT
} batt_full_state_t;
#endif
#endif
/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 end */

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

int chr_get_debug_level(void)
{
	struct power_supply *psy;
	static struct mtk_charger *info;
	int ret;

	if (info == NULL) {
		psy = power_supply_get_by_name("mtk-master-charger");
		if (psy == NULL)
			ret = CHRLOG_DEBUG_LEVEL;
		else {
			info =
			(struct mtk_charger *)power_supply_get_drvdata(psy);
			if (info == NULL)
				ret = CHRLOG_DEBUG_LEVEL;
			else
				ret = info->log_level;
		}
	} else
		ret = info->log_level;

	return ret;
}

/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 start */
#if !defined(HQ_FACTORY_BUILD)
void hq_enable_power_path(struct mtk_charger *info)
{
	if (info->cap_hold_count < 3)
	{
		info->cmd_discharging = true;
		charger_dev_enable(info->chg1_dev, false);
		charger_dev_do_event(info->chg1_dev, EVENT_DISCHARGE, 0);//disable battery charging
#ifdef CONFIG_HQ_PROJECT_HS03S
		charger_dev_set_hiz_mode(info->chg1_dev, false);
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
		charger_dev_set_hiz_mode(info->chg1_dev, false);
#endif
#ifdef CONFIG_HQ_PROJECT_OT8
		charger_dev_enable_powerpath(info->chg1_dev, true);
#endif
	}
	info->cap_hold_count++;
	info->over_cap_count = 0;
	info->recharge_count = 0;
	chr_err("%s over_cap_count = %d, cap_hold_count = %d, recharge_count = %d \n", __func__,
					info->over_cap_count, info->cap_hold_count, info->recharge_count);
}

void hq_enable_charging(struct mtk_charger *info, bool cval)
{
	info->en_batt_protect = DISABLE_BATT_PROTECT;

	if (cval == true) {
		if (info->recharge_count < 3)
		{
			info->cmd_discharging = false;
			charger_dev_enable(info->chg1_dev, true);
			charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);// enable battery charging
#ifdef CONFIG_HQ_PROJECT_HS03S
			charger_dev_set_hiz_mode(info->chg1_dev, false);
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
			charger_dev_set_hiz_mode(info->chg1_dev, false);
#endif
#ifdef CONFIG_HQ_PROJECT_OT8
			charger_dev_enable_powerpath(info->chg1_dev, true);
#endif
			info->recharge_count++;
			info->over_cap_count = 0;
			info->cap_hold_count = 0;
		}
	} else {
		if (info->over_cap_count < 3)
		{
			info->cmd_discharging = true;
			charger_dev_enable(info->chg1_dev, false);
			charger_dev_do_event(info->chg1_dev, EVENT_DISCHARGE, 0);//disable battery charging
#ifdef CONFIG_HQ_PROJECT_HS03S
			charger_dev_set_hiz_mode(info->chg1_dev, true);
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
			charger_dev_set_hiz_mode(info->chg1_dev, true);
#endif
#ifdef CONFIG_HQ_PROJECT_OT8
			charger_dev_enable_powerpath(info->chg1_dev, false);
#endif
			info->over_cap_count++;
			info->cap_hold_count = 0;
			info->recharge_count = 0;
		}
	}
	chr_err("%s over_cap_count = %d, cap_hold_count = %d, recharge_count = %d \n", __func__,
					info->over_cap_count, info->cap_hold_count, info->recharge_count);
}

void hq_update_charing_count(struct mtk_charger *info)
{
	struct timex txc;

	/* HS03s code for HQ00001 Modify battery protect function by shixuanxuan at 2021/05/10 start */
	int temp_time;

	temp_time = info->current_time;
	/* HS03s Lite code for HQ00001 Modify battery protect function by shixuanxuan at 2021/05/10 end */

	do_gettimeofday(&(txc.time));
	info->current_time = txc.time.tv_sec;
	/* HS03s Lite code for HQ00001 Modify battery protect function by shixuanxuan at 2021/05/10 start */
	if (!info->vbus_rising)
	{
		info->base_time = info->current_time;
		info->interval_time = 0;
	} else {
		if(info->base_time == 0)// if the usb connected and the phone restart base_time=0;
			info->base_time = info->current_time;

		if ((info->current_time - temp_time > 3600) || (info->current_time - temp_time < 0))
		{
			info->base_time = info->current_time;
			info->interval_time = 0;
		} else {
                        /*Tab A7 lite_U/HS04_U code for AX6739A-2748 by lihao at 202300913 start*/
			info->interval_time = 0;
                        /*Tab A7 lite_U/HS04_U code for AX6739A-2748 by lihao at 202300913 end*/
		}
	}
/* HS03s Lite code for HQ00001 Modify battery protect function by shixuanxuan at 2021/05/10 end */
	chr_err("%s charging interval_time: %d (s)\n", __func__, info->interval_time);
}

void hq_update_charge_state(struct mtk_charger *info)
{
	int batt_capcity;

	hq_update_charing_count(info);
#ifdef CONFIG_HQ_PROJECT_HS03S
	if (info->batt_protect_flag == ENABLE_BATT_PROTECT && info->cust_batt_cap == 100)
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
	if (info->batt_protect_flag == ENABLE_BATT_PROTECT && info->cust_batt_cap == 100)
#endif
#ifdef CONFIG_HQ_PROJECT_OT8
        /* TabA7 Lite code for OT8-5454 by shixuanxuan at 20220404 start */
	if(info->batt_protect_flag == ENABLE_BATT_PROTECT && info->cust_batt_cap == 100)
        /* TabA7 Lite code for OT8-5454 by shixuanxuan at 20220404 end */
#endif
	{
		if(info->interval_time > TSUSPEND )
		{
			batt_capcity = get_uisoc(info);
			if (batt_capcity > HIGHT_CAPACITY)
			{
				hq_enable_charging(info, false);
				info->en_batt_protect = ENABLE_BATT_PROTECT;
				chr_err("%s Continue charging over 3 days and capacity > 75, stop charging!! \n", __func__);
			} else if (batt_capcity == HIGHT_CAPACITY) {
				hq_enable_power_path(info);
				info->en_batt_protect = ENABLE_BATT_PROTECT;
				chr_err("%s Continue charging over 3 days and capacity = 75, stop battery charging!! \n", __func__);
			} else if (batt_capcity < LOW_CAPACITY) {
				hq_enable_charging(info, true);
			}
		}
	} else {
		hq_enable_charging(info, true);
		info->en_batt_protect = DISABLE_BATT_PROTECT;
	}
}

static void hq_charging_count_work(struct work_struct *work)
{
	struct mtk_charger *info = container_of(work,
			struct mtk_charger, charging_count_work.work);
	hq_update_charge_state(info);
	schedule_delayed_work(&info->charging_count_work, msecs_to_jiffies(CHR_COUNT_TIME));
}
#endif
/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 end */

void _wake_up_charger(struct mtk_charger *info)
{
	unsigned long flags;

	if (info == NULL)
		return;

	spin_lock_irqsave(&info->slock, flags);
	if (!info->charger_wakelock->active)
		__pm_stay_awake(info->charger_wakelock);
	spin_unlock_irqrestore(&info->slock, flags);
	info->charger_thread_timeout = true;
	wake_up(&info->wait_que);
}

bool is_disable_charger(struct mtk_charger *info)
{
	if (info == NULL)
		return true;

	if (info->disable_charger == true || IS_ENABLED(CONFIG_POWER_EXT))
		return true;
	else
		return false;
}

int _mtk_enable_charging(struct mtk_charger *info,
	bool en)
{
	chr_debug("%s en:%d\n", __func__, en);
	if (info->algo.enable_charging != NULL)
		return info->algo.enable_charging(info, en);
	return false;
}

int mtk_charger_notifier(struct mtk_charger *info, int event)
{
	return srcu_notifier_call_chain(&info->evt_nh, event, NULL);
}

/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 start*/
#ifndef HQ_FACTORY_BUILD
static int read_range_data_from_node(struct device_node *node,
		const char *prop_str, struct range_data *ranges,
		u32 max_threshold, u32 max_value)
{
	int rc = 0, i, length, per_tuple_length, tuples;

	rc = of_property_count_elems_of_size(node, prop_str, sizeof(u32));
	if (rc < 0) {
		pr_err("Count %s failed, rc=%d\n", prop_str, rc);
		return rc;
	}

	length = rc;
	per_tuple_length = sizeof(struct range_data) / sizeof(u32);
	if (length % per_tuple_length) {
		pr_err("%s length (%d) should be multiple of %d\n",
				prop_str, length, per_tuple_length);
		return -EINVAL;
	}
	tuples = length / per_tuple_length;

	if (tuples > MAX_CV_ENTRIES) {
		pr_err("too many entries(%d), only %d allowed\n",
				tuples, MAX_CV_ENTRIES);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(node, prop_str,
			(u32 *)ranges, length);
	if (rc) {
		pr_err("Read %s failed, rc=%d", prop_str, rc);
		return rc;
	}

	for (i = 0; i < tuples; i++) {
		if (ranges[i].low_threshold >
				ranges[i].high_threshold) {
			pr_err("%s thresholds should be in ascendant ranges\n",
						prop_str);
			rc = -EINVAL;
			goto clean;
		}

		if (i != 0) {
			if (ranges[i - 1].high_threshold >
					ranges[i].low_threshold) {
				pr_err("%s thresholds should be in ascendant ranges\n",
							prop_str);
				rc = -EINVAL;
				goto clean;
			}
		}

		if (ranges[i].low_threshold > max_threshold)
			ranges[i].low_threshold = max_threshold;
		if (ranges[i].high_threshold > max_threshold)
			ranges[i].high_threshold = max_threshold;
		if (ranges[i].value > max_value)
			ranges[i].value = max_value;
	}

	return rc;
clean:
	memset(ranges, 0, tuples * sizeof(struct range_data));
	return rc;
}
#endif
/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 end*/

/*HS03s for SR-AL5625-01-426 by wenyaqi at 20210515 start*/
#define PCBAINFO_STR_LEN 6
static char pcbainfo_from_cmdline[PCBAINFO_STR_LEN + 1];
static int __init pcbainfo_setup(char *str)
{
	strlcpy(pcbainfo_from_cmdline, str,
		ARRAY_SIZE(pcbainfo_from_cmdline));

	return 1;
}
__setup("androidboot.pcbainfo=", pcbainfo_setup);

bool pcbainfo_is(char* str)
{
	return !strncmp(pcbainfo_from_cmdline, str,
				PCBAINFO_STR_LEN + 1);
}

bool is_latam(void)
{
	if(pcbainfo_is("G_25CA") || pcbainfo_is("G_25CC") || pcbainfo_is("G_26CA"))
		return true;
	else
		return false;
}
/*HS03s for SR-AL5625-01-426 by wenyaqi at 20210515 end*/

static void mtk_charger_parse_dt(struct mtk_charger *info,
				struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 val = 0;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	int rc;
	#endif
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 end*/

	boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
	if (!boot_node)
		chr_err("%s: failed to get boot mode phandle\n", __func__);
	else {
		tag = (struct tag_bootmode *)of_get_property(boot_node,
							"atag,boot", NULL);
		if (!tag)
			chr_err("%s: failed to get atag,boot\n", __func__);
		else {
			chr_err("%s: size:0x%x tag:0x%x bootmode:0x%x boottype:0x%x\n",
				__func__, tag->size, tag->tag,
				tag->bootmode, tag->boottype);
			info->bootmode = tag->bootmode;
			info->boottype = tag->boottype;
		}
	}

	if (of_property_read_string(np, "algorithm_name",
		&info->algorithm_name) < 0) {
		chr_err("%s: no algorithm_name name\n", __func__);
		info->algorithm_name = "Basic";
	}

	if (strcmp(info->algorithm_name, "Basic") == 0) {
		chr_err("found Basic\n");
		mtk_basic_charger_init(info);
	} else if (strcmp(info->algorithm_name, "Pulse") == 0) {
		chr_err("found Pulse\n");
		mtk_pulse_charger_init(info);
	}

	info->disable_charger = of_property_read_bool(np, "disable_charger");
	info->enable_sw_safety_timer =
			of_property_read_bool(np, "enable_sw_safety_timer");
	info->sw_safety_timer_setting = info->enable_sw_safety_timer;

	/* common */

	if (of_property_read_u32(np, "charger_configuration", &val) >= 0)
		info->config = val;
	else {
		chr_err("use default charger_configuration:%d\n",
			SINGLE_CHARGER);
		info->config = SINGLE_CHARGER;
	}

	if (of_property_read_u32(np, "battery_cv", &val) >= 0)
		info->data.battery_cv = val;
	else {
		chr_err("use default BATTERY_CV:%d\n", BATTERY_CV);
		info->data.battery_cv = BATTERY_CV;
	}

	if (of_property_read_u32(np, "max_charger_voltage", &val) >= 0)
		info->data.max_charger_voltage = val;
	else {
		chr_err("use default V_CHARGER_MAX:%d\n", V_CHARGER_MAX);
		info->data.max_charger_voltage = V_CHARGER_MAX;
	}
	info->data.max_charger_voltage_setting = info->data.max_charger_voltage;

	if (of_property_read_u32(np, "min_charger_voltage", &val) >= 0)
		info->data.min_charger_voltage = val;
	else {
		chr_err("use default V_CHARGER_MIN:%d\n", V_CHARGER_MIN);
		info->data.min_charger_voltage = V_CHARGER_MIN;
	}

	/* sw jeita */
	info->enable_sw_jeita = of_property_read_bool(np, "enable_sw_jeita");
	if (of_property_read_u32(np, "jeita_temp_above_t4_cv", &val) >= 0)
		info->data.jeita_temp_above_t4_cv = val;
	else {
		chr_err("use default JEITA_TEMP_ABOVE_T4_CV:%d\n",
			JEITA_TEMP_ABOVE_T4_CV);
		info->data.jeita_temp_above_t4_cv = JEITA_TEMP_ABOVE_T4_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t3_to_t4_cv", &val) >= 0)
		info->data.jeita_temp_t3_to_t4_cv = val;
	else {
		chr_err("use default JEITA_TEMP_T3_TO_T4_CV:%d\n",
			JEITA_TEMP_T3_TO_T4_CV);
		info->data.jeita_temp_t3_to_t4_cv = JEITA_TEMP_T3_TO_T4_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t2_to_t3_cv", &val) >= 0)
		info->data.jeita_temp_t2_to_t3_cv = val;
	else {
		chr_err("use default JEITA_TEMP_T2_TO_T3_CV:%d\n",
			JEITA_TEMP_T2_TO_T3_CV);
		info->data.jeita_temp_t2_to_t3_cv = JEITA_TEMP_T2_TO_T3_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t1_to_t2_cv", &val) >= 0)
		info->data.jeita_temp_t1_to_t2_cv = val;
	else {
		chr_err("use default JEITA_TEMP_T1_TO_T2_CV:%d\n",
			JEITA_TEMP_T1_TO_T2_CV);
		info->data.jeita_temp_t1_to_t2_cv = JEITA_TEMP_T1_TO_T2_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t0_to_t1_cv", &val) >= 0)
		info->data.jeita_temp_t0_to_t1_cv = val;
	else {
		chr_err("use default JEITA_TEMP_T0_TO_T1_CV:%d\n",
			JEITA_TEMP_T0_TO_T1_CV);
		info->data.jeita_temp_t0_to_t1_cv = JEITA_TEMP_T0_TO_T1_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_below_t0_cv", &val) >= 0)
		info->data.jeita_temp_below_t0_cv = val;
	else {
		chr_err("use default JEITA_TEMP_BELOW_T0_CV:%d\n",
			JEITA_TEMP_BELOW_T0_CV);
		info->data.jeita_temp_below_t0_cv = JEITA_TEMP_BELOW_T0_CV;
	}

	/*HS03s for SR-AL5625-01-261 by wenyaqi at 20210425 start*/
	if (of_property_read_u32(np, "jeita_temp_above_t4_cur", &val) >= 0)
		info->data.jeita_temp_above_t4_cur = val;
	else {
		chr_err("use default JEITA_TEMP_ABOVE_T4_CUR:%d\n",
			JEITA_TEMP_ABOVE_T4_CUR);
		info->data.jeita_temp_above_t4_cur = JEITA_TEMP_ABOVE_T4_CUR;
	}

	if (of_property_read_u32(np, "jeita_temp_t3_to_t4_cur", &val) >= 0)
		info->data.jeita_temp_t3_to_t4_cur = val;
	else {
		chr_err("use default JEITA_TEMP_T3_TO_T4_CUR:%d\n",
			JEITA_TEMP_T3_TO_T4_CUR);
		info->data.jeita_temp_t3_to_t4_cur = JEITA_TEMP_T3_TO_T4_CUR;
	}

	if (of_property_read_u32(np, "jeita_temp_t2_to_t3_cur", &val) >= 0)
		info->data.jeita_temp_t2_to_t3_cur = val;
	else {
		chr_err("use default JEITA_TEMP_T2_TO_T3_CUR:%d\n",
			JEITA_TEMP_T2_TO_T3_CUR);
		info->data.jeita_temp_t2_to_t3_cur = JEITA_TEMP_T2_TO_T3_CUR;
	}

	if (of_property_read_u32(np, "jeita_temp_t1_to_t2_cur", &val) >= 0)
		info->data.jeita_temp_t1_to_t2_cur = val;
	else {
		chr_err("use default JEITA_TEMP_T1_TO_T2_CUR:%d\n",
			JEITA_TEMP_T1_TO_T2_CUR);
		info->data.jeita_temp_t1_to_t2_cur = JEITA_TEMP_T1_TO_T2_CUR;
	}

	if (of_property_read_u32(np, "jeita_temp_t0_to_t1_cur", &val) >= 0)
		info->data.jeita_temp_t0_to_t1_cur = val;
	else {
		chr_err("use default JEITA_TEMP_T0_TO_T1_CUR:%d\n",
			JEITA_TEMP_T0_TO_T1_CUR);
		info->data.jeita_temp_t0_to_t1_cur = JEITA_TEMP_T0_TO_T1_CUR;
	}

	if (of_property_read_u32(np, "jeita_temp_below_t0_cur", &val) >= 0)
		info->data.jeita_temp_below_t0_cur = val;
	else {
		chr_err("use default JEITA_TEMP_BELOW_T0_CUR:%d\n",
			JEITA_TEMP_BELOW_T0_CUR);
		info->data.jeita_temp_below_t0_cur = JEITA_TEMP_BELOW_T0_CUR;
	}
	/*HS03s for SR-AL5625-01-261 by wenyaqi at 20210425 end*/

	if (of_property_read_u32(np, "temp_t4_thres", &val) >= 0)
		info->data.temp_t4_thres = val;
	else {
		chr_err("use default TEMP_T4_THRES:%d\n",
			TEMP_T4_THRES);
		info->data.temp_t4_thres = TEMP_T4_THRES;
	}

	if (of_property_read_u32(np, "temp_t4_thres_minus_x_degree", &val) >= 0)
		info->data.temp_t4_thres_minus_x_degree = val;
	else {
		chr_err("use default TEMP_T4_THRES_MINUS_X_DEGREE:%d\n",
			TEMP_T4_THRES_MINUS_X_DEGREE);
		info->data.temp_t4_thres_minus_x_degree =
					TEMP_T4_THRES_MINUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t3_thres", &val) >= 0)
		info->data.temp_t3_thres = val;
	else {
		chr_err("use default TEMP_T3_THRES:%d\n",
			TEMP_T3_THRES);
		info->data.temp_t3_thres = TEMP_T3_THRES;
	}

	if (of_property_read_u32(np, "temp_t3_thres_minus_x_degree", &val) >= 0)
		info->data.temp_t3_thres_minus_x_degree = val;
	else {
		chr_err("use default TEMP_T3_THRES_MINUS_X_DEGREE:%d\n",
			TEMP_T3_THRES_MINUS_X_DEGREE);
		info->data.temp_t3_thres_minus_x_degree =
					TEMP_T3_THRES_MINUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t2_thres", &val) >= 0)
		info->data.temp_t2_thres = val;
	else {
		chr_err("use default TEMP_T2_THRES:%d\n",
			TEMP_T2_THRES);
		info->data.temp_t2_thres = TEMP_T2_THRES;
	}

	if (of_property_read_u32(np, "temp_t2_thres_plus_x_degree", &val) >= 0)
		info->data.temp_t2_thres_plus_x_degree = val;
	else {
		chr_err("use default TEMP_T2_THRES_PLUS_X_DEGREE:%d\n",
			TEMP_T2_THRES_PLUS_X_DEGREE);
		info->data.temp_t2_thres_plus_x_degree =
					TEMP_T2_THRES_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t1_thres", &val) >= 0)
		info->data.temp_t1_thres = val;
	else {
		chr_err("use default TEMP_T1_THRES:%d\n",
			TEMP_T1_THRES);
		info->data.temp_t1_thres = TEMP_T1_THRES;
	}

	if (of_property_read_u32(np, "temp_t1_thres_plus_x_degree", &val) >= 0)
		info->data.temp_t1_thres_plus_x_degree = val;
	else {
		chr_err("use default TEMP_T1_THRES_PLUS_X_DEGREE:%d\n",
			TEMP_T1_THRES_PLUS_X_DEGREE);
		info->data.temp_t1_thres_plus_x_degree =
					TEMP_T1_THRES_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t0_thres", &val) >= 0)
		info->data.temp_t0_thres = val;
	else {
		chr_err("use default TEMP_T0_THRES:%d\n",
			TEMP_T0_THRES);
		info->data.temp_t0_thres = TEMP_T0_THRES;
	}

	if (of_property_read_u32(np, "temp_t0_thres_plus_x_degree", &val) >= 0)
		info->data.temp_t0_thres_plus_x_degree = val;
	else {
		chr_err("use default TEMP_T0_THRES_PLUS_X_DEGREE:%d\n",
			TEMP_T0_THRES_PLUS_X_DEGREE);
		info->data.temp_t0_thres_plus_x_degree =
					TEMP_T0_THRES_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_neg_10_thres", &val) >= 0)
		info->data.temp_neg_10_thres = val;
	else {
		chr_err("use default TEMP_NEG_10_THRES:%d\n",
			TEMP_NEG_10_THRES);
		info->data.temp_neg_10_thres = TEMP_NEG_10_THRES;
	}

	/* battery temperature protection */
	info->thermal.sm = BAT_TEMP_NORMAL;
	info->thermal.enable_min_charge_temp =
		of_property_read_bool(np, "enable_min_charge_temp");

	if (of_property_read_u32(np, "min_charge_temp", &val) >= 0)
		info->thermal.min_charge_temp = val;
	else {
		chr_err("use default MIN_CHARGE_TEMP:%d\n",
			MIN_CHARGE_TEMP);
		info->thermal.min_charge_temp = MIN_CHARGE_TEMP;
	}

	if (of_property_read_u32(np, "min_charge_temp_plus_x_degree", &val)
		>= 0) {
		info->thermal.min_charge_temp_plus_x_degree = val;
	} else {
		chr_err("use default MIN_CHARGE_TEMP_PLUS_X_DEGREE:%d\n",
			MIN_CHARGE_TEMP_PLUS_X_DEGREE);
		info->thermal.min_charge_temp_plus_x_degree =
					MIN_CHARGE_TEMP_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "max_charge_temp", &val) >= 0)
		info->thermal.max_charge_temp = val;
	else {
		chr_err("use default MAX_CHARGE_TEMP:%d\n",
			MAX_CHARGE_TEMP);
		info->thermal.max_charge_temp = MAX_CHARGE_TEMP;
	}

	if (of_property_read_u32(np, "max_charge_temp_minus_x_degree", &val)
		>= 0) {
		info->thermal.max_charge_temp_minus_x_degree = val;
	} else {
		chr_err("use default MAX_CHARGE_TEMP_MINUS_X_DEGREE:%d\n",
			MAX_CHARGE_TEMP_MINUS_X_DEGREE);
		info->thermal.max_charge_temp_minus_x_degree =
					MAX_CHARGE_TEMP_MINUS_X_DEGREE;
	}

	/* charging current */
	if (of_property_read_u32(np, "usb_charger_current", &val) >= 0) {
		info->data.usb_charger_current = val;
	} else {
		chr_err("use default USB_CHARGER_CURRENT:%d\n",
			USB_CHARGER_CURRENT);
		info->data.usb_charger_current = USB_CHARGER_CURRENT;
	}

	if (of_property_read_u32(np, "ac_charger_current", &val) >= 0) {
		info->data.ac_charger_current = val;
	} else {
		chr_err("use default AC_CHARGER_CURRENT:%d\n",
			AC_CHARGER_CURRENT);
		info->data.ac_charger_current = AC_CHARGER_CURRENT;
	}

	if (of_property_read_u32(np, "ac_charger_input_current", &val) >= 0)
		info->data.ac_charger_input_current = val;
	else {
		chr_err("use default AC_CHARGER_INPUT_CURRENT:%d\n",
			AC_CHARGER_INPUT_CURRENT);
		info->data.ac_charger_input_current = AC_CHARGER_INPUT_CURRENT;
	}
	/*HS03s for SR-AL5625-01-426 by wenyaqi at 20210515 start*/
	if (is_latam()) {
		chr_err("LATAM use AC_CHARGER_INPUT_CURRENT:1000mA\n");
		info->data.ac_charger_input_current = AC_CHARGER_INPUT_CURRENT_LATAM;
	} else {
		chr_err("not LATAM, use default setting\n");
	}
	/*HS03s for SR-AL5625-01-426 by wenyaqi at 20210515 start*/

	if (of_property_read_u32(np, "charging_host_charger_current", &val)
		>= 0) {
		info->data.charging_host_charger_current = val;
	} else {
		chr_err("use default CHARGING_HOST_CHARGER_CURRENT:%d\n",
			CHARGING_HOST_CHARGER_CURRENT);
		info->data.charging_host_charger_current =
					CHARGING_HOST_CHARGER_CURRENT;
	}

	/* dynamic mivr */
	info->enable_dynamic_mivr =
			of_property_read_bool(np, "enable_dynamic_mivr");

	if (of_property_read_u32(np, "min_charger_voltage_1", &val) >= 0)
		info->data.min_charger_voltage_1 = val;
	else {
		chr_err("use default V_CHARGER_MIN_1: %d\n", V_CHARGER_MIN_1);
		info->data.min_charger_voltage_1 = V_CHARGER_MIN_1;
	}

	if (of_property_read_u32(np, "min_charger_voltage_2", &val) >= 0)
		info->data.min_charger_voltage_2 = val;
	else {
		chr_err("use default V_CHARGER_MIN_2: %d\n", V_CHARGER_MIN_2);
		info->data.min_charger_voltage_2 = V_CHARGER_MIN_2;
	}

	if (of_property_read_u32(np, "max_dmivr_charger_current", &val) >= 0)
		info->data.max_dmivr_charger_current = val;
	else {
		chr_err("use default MAX_DMIVR_CHARGER_CURRENT: %d\n",
			MAX_DMIVR_CHARGER_CURRENT);
		info->data.max_dmivr_charger_current =
					MAX_DMIVR_CHARGER_CURRENT;
	}

	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	info->data.ss_batt_aging_enable = of_property_read_bool(np,
		"ss-batt-aging-enable");

	rc = read_range_data_from_node(np, "ss,cv-ranges",
		info->data.batt_cv_data,
		MAX_CYCLE_COUNT, info->data.battery_cv);
	if (rc < 0) {
		pr_err("Read ss,cv-ranges failed from dtsi, rc=%d\n", rc);
		info->data.ss_batt_aging_enable = false;
	}
	#endif
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 end*/
	/*HS03s for SR-AL5625-01-248 by wenyaqi at 20210429 start*/
	#ifdef HQ_D85_BUILD
	info->data.battery_cv = D85_BATTERY_CV;
	info->data.jeita_temp_above_t4_cv = D85_JEITA_TEMP_CV;
	info->data.jeita_temp_t3_to_t4_cv = D85_JEITA_TEMP_CV;
	info->data.jeita_temp_t2_to_t3_cv = D85_JEITA_TEMP_CV;
	info->data.jeita_temp_t1_to_t2_cv = D85_JEITA_TEMP_CV;
	info->data.jeita_temp_t0_to_t1_cv = D85_JEITA_TEMP_CV;
	#endif
	/*HS03s for SR-AL5625-01-248 by wenyaqi at 20210429 end*/
}

static void mtk_charger_start_timer(struct mtk_charger *info)
{
	struct timespec time, time_now;
	ktime_t ktime;
	int ret = 0;

	/* If the timer was already set, cancel it */
	ret = alarm_try_to_cancel(&info->charger_timer);
	if (ret < 0) {
		chr_err("%s: callback was running, skip timer\n", __func__);
		return;
	}

	get_monotonic_boottime(&time_now);
#ifdef CONFIG_HQ_PROJECT_HS03S
    /* modify code for O6 */
	time.tv_sec = info->polling_interval;
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
    /* modify code for O6 */
	time.tv_sec = info->polling_interval;
#endif
#ifdef CONFIG_HQ_PROJECT_OT8
    /* modify code for OT8 */
	/*TabA7 Lite code for P210511-00511 by wenyaqi at 20210518 start*/
	#ifndef HQ_FACTORY_BUILD
	if (info->ss_chr_type_detect <= 3) {
		time.tv_sec = 2;
		info->ss_chr_type_detect++;
	 } else
		time.tv_sec = info->polling_interval;
	#else
	time.tv_sec = info->polling_interval;
	#endif
	/*TabA7 Lite code for P210511-00511 by wenyaqi at 20210518 end*/
#endif
	time.tv_nsec = 0;
	info->endtime = timespec_add(time_now, time);
	ktime = ktime_set(info->endtime.tv_sec, info->endtime.tv_nsec);

	chr_err("%s: alarm timer start:%d, %ld %ld\n", __func__, ret,
		info->endtime.tv_sec, info->endtime.tv_nsec);
	alarm_start(&info->charger_timer, ktime);
}

static void check_battery_exist(struct mtk_charger *info)
{
	unsigned int i = 0;
	int count = 0;
	//int boot_mode = get_boot_mode();

	if (is_disable_charger(info))
		return;

	for (i = 0; i < 3; i++) {
		if (is_battery_exist(info) == false)
			count++;
	}

#ifdef FIXME
	if (count >= 3) {
		/*1 = META_BOOT, 5 = ADVMETA_BOOT*/
		/*6 = ATE_FACTORY_BOOT */
		if (boot_mode == 1 || boot_mode == 5 ||
		    boot_mode == 6)
			chr_info("boot_mode = %d, bypass battery check\n",
				boot_mode);
		else {
			chr_err("battery doesn't exist, shutdown\n");
			orderly_poweroff(true);
		}
	}
#endif
}

/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
#ifdef CONFIG_AFC_CHARGER
/* detect PE/PE2.0/PE4.0 */
static bool ss_pump_express_status(struct mtk_charger *info)
{
	int i = 0, ret = 0;
	struct chg_alg_device *alg = NULL;

	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		if (alg == NULL)
			continue;
		ret = chg_alg_is_algo_ready(alg);
		if (ret == ALG_RUNNING || ret == ALG_DONE) {
			return true;
		}
	}
	return false;
}

static bool ss_fast_charger_status(struct mtk_charger *info)
{
	if (info->afc_sts >= AFC_5V || info->afc.afc_retry_flag == true ||
		ss_pump_express_status(info) || (info->chr_type == POWER_SUPPLY_TYPE_USB_DCP &&
		(info->pd_type == MTK_PD_CONNECT_PE_READY_SNK ||
		info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30 ||
		info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO)))
		return true;
	return false;
}
#endif
/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/

static void check_dynamic_mivr(struct mtk_charger *info)
{
	int i = 0, ret = 0;
	int vbat = 0;
	bool is_fast_charge = false;
	struct chg_alg_device *alg = NULL;

	if (!info->enable_dynamic_mivr)
		return;

	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		if (alg == NULL)
			continue;
		ret = chg_alg_is_algo_ready(alg);
		if (ret == ALG_RUNNING) {
			is_fast_charge = true;
			break;
		}
	}

	if (!is_fast_charge) {
		vbat = get_battery_voltage(info);
		if (vbat < info->data.min_charger_voltage_2 / 1000 - 200)
			charger_dev_set_mivr(info->chg1_dev,
				info->data.min_charger_voltage_2);
		else if (vbat < info->data.min_charger_voltage_1 / 1000 - 200)
			charger_dev_set_mivr(info->chg1_dev,
				info->data.min_charger_voltage_1);
		else
			charger_dev_set_mivr(info->chg1_dev,
				info->data.min_charger_voltage);
	}
}

/*HS03s for DEVAL5626-469 by liujie at 20210819 start*/
void init_jeita_state_machine(struct mtk_charger *info)
{
    struct sw_jeita_data *sw_jeita;

    info->battery_temp=get_battery_temperature(info);
    sw_jeita = &info->sw_jeita;

    if (info->battery_temp >= info->data.temp_t4_thres)
        sw_jeita->sm = TEMP_ABOVE_T4;
    else if (info->battery_temp > info->data.temp_t3_thres)
        sw_jeita->sm = TEMP_T3_TO_T4;
    else if (info->battery_temp >= info->data.temp_t2_thres)
        sw_jeita->sm = TEMP_T2_TO_T3;
    else if (info->battery_temp >= info->data.temp_t1_thres)
        sw_jeita->sm = TEMP_T1_TO_T2;
    else if (info->battery_temp >= info->data.temp_t0_thres)
        sw_jeita->sm = TEMP_T0_TO_T1;
    else
        sw_jeita->sm = TEMP_BELOW_T0;
}
/*HS03s for DEVAL5626-469 by liujie at 20210819 end*/

/* sw jeita */
void do_sw_jeita_state_machine(struct mtk_charger *info)
{
	struct sw_jeita_data *sw_jeita;

	sw_jeita = &info->sw_jeita;
	sw_jeita->pre_sm = sw_jeita->sm;
	sw_jeita->charging = true;

	/* JEITA battery temp Standard */
	if (info->battery_temp >= info->data.temp_t4_thres) {
		chr_err("[SW_JEITA] Battery Over high Temperature(%d) !!\n",
			info->data.temp_t4_thres);

		sw_jeita->sm = TEMP_ABOVE_T4;
		sw_jeita->charging = false;
	} else if (info->battery_temp > info->data.temp_t3_thres) {
		/* control 45 degree to normal behavior */
		if ((sw_jeita->sm == TEMP_ABOVE_T4)
		    && (info->battery_temp
			>= info->data.temp_t4_thres_minus_x_degree)) {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d,not allow charging yet!!\n",
				info->data.temp_t4_thres_minus_x_degree,
				info->data.temp_t4_thres);

			sw_jeita->charging = false;
		} else {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d !!\n",
				info->data.temp_t3_thres,
				info->data.temp_t4_thres);

			sw_jeita->sm = TEMP_T3_TO_T4;
		}
	} else if (info->battery_temp >= info->data.temp_t2_thres) {
		if (((sw_jeita->sm == TEMP_T3_TO_T4)
		     && (info->battery_temp
			 >= info->data.temp_t3_thres_minus_x_degree))
		    || ((sw_jeita->sm == TEMP_T1_TO_T2)
			&& (info->battery_temp
			    <= info->data.temp_t2_thres_plus_x_degree))) {
			chr_err("[SW_JEITA] Battery Temperature not recovery to normal temperature charging mode yet!!\n");
		} else {
			chr_err("[SW_JEITA] Battery Normal Temperature between %d and %d !!\n",
				info->data.temp_t2_thres,
				info->data.temp_t3_thres);
			sw_jeita->sm = TEMP_T2_TO_T3;
		}
	} else if (info->battery_temp >= info->data.temp_t1_thres) {
		if ((sw_jeita->sm == TEMP_T0_TO_T1
		     || sw_jeita->sm == TEMP_BELOW_T0)
		    && (info->battery_temp
			<= info->data.temp_t1_thres_plus_x_degree)) {
			if (sw_jeita->sm == TEMP_T0_TO_T1) {
				chr_err("[SW_JEITA] Battery Temperature between %d and %d !!\n",
					info->data.temp_t1_thres_plus_x_degree,
					info->data.temp_t2_thres);
			}
			if (sw_jeita->sm == TEMP_BELOW_T0) {
				chr_err("[SW_JEITA] Battery Temperature between %d and %d,not allow charging yet!!\n",
					info->data.temp_t1_thres,
					info->data.temp_t1_thres_plus_x_degree);
				sw_jeita->charging = false;
			}
		} else {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d !!\n",
				info->data.temp_t1_thres,
				info->data.temp_t2_thres);

			sw_jeita->sm = TEMP_T1_TO_T2;
		}
	} else if (info->battery_temp >= info->data.temp_t0_thres) {
		if ((sw_jeita->sm == TEMP_BELOW_T0)
		    && (info->battery_temp
			<= info->data.temp_t0_thres_plus_x_degree)) {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d,not allow charging yet!!\n",
				info->data.temp_t0_thres,
				info->data.temp_t0_thres_plus_x_degree);

			sw_jeita->charging = false;
		} else {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d !!\n",
				info->data.temp_t0_thres,
				info->data.temp_t1_thres);

			sw_jeita->sm = TEMP_T0_TO_T1;
		}
	} else {
		chr_err("[SW_JEITA] Battery below low Temperature(%d) !!\n",
			info->data.temp_t0_thres);
		sw_jeita->sm = TEMP_BELOW_T0;
		sw_jeita->charging = false;
	}

	/* set CV after temperature changed */
	/* In normal range, we adjust CV dynamically */
	if (sw_jeita->sm != TEMP_T2_TO_T3) {
		if (sw_jeita->sm == TEMP_ABOVE_T4)
			sw_jeita->cv = info->data.jeita_temp_above_t4_cv;
		else if (sw_jeita->sm == TEMP_T3_TO_T4)
			sw_jeita->cv = info->data.jeita_temp_t3_to_t4_cv;
		else if (sw_jeita->sm == TEMP_T2_TO_T3)
			sw_jeita->cv = 0;
		else if (sw_jeita->sm == TEMP_T1_TO_T2)
			sw_jeita->cv = info->data.jeita_temp_t1_to_t2_cv;
		else if (sw_jeita->sm == TEMP_T0_TO_T1)
			sw_jeita->cv = info->data.jeita_temp_t0_to_t1_cv;
		else if (sw_jeita->sm == TEMP_BELOW_T0)
			sw_jeita->cv = info->data.jeita_temp_below_t0_cv;
		else
			sw_jeita->cv = info->data.battery_cv;
	} else {
		sw_jeita->cv = 0;
	}

	chr_err("[SW_JEITA]preState:%d newState:%d tmp:%d cv:%d\n",
		sw_jeita->pre_sm, sw_jeita->sm, info->battery_temp,
		sw_jeita->cv);
}

static int mtk_chgstat_notify(struct mtk_charger *info)
{
	int ret = 0;
	char *env[2] = { "CHGSTAT=1", NULL };

	chr_err("%s: 0x%x\n", __func__, info->notify_code);
	ret = kobject_uevent_env(&info->pdev->dev.kobj, KOBJ_CHANGE, env);
	if (ret)
		chr_err("%s: kobject_uevent_fail, ret=%d", __func__, ret);

	return ret;
}

static ssize_t sw_jeita_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->enable_sw_jeita);
	return sprintf(buf, "%d\n", pinfo->enable_sw_jeita);
}

static ssize_t sw_jeita_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp == 0)
			pinfo->enable_sw_jeita = false;
		else
			pinfo->enable_sw_jeita = true;

	} else {
		chr_err("%s: format error!\n", __func__);
	}
	return size;
}

static DEVICE_ATTR_RW(sw_jeita);
/* sw jeita end*/

static ssize_t chr_type_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->chr_type);
	return sprintf(buf, "%d\n", pinfo->chr_type);
}

static ssize_t chr_type_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0)
		pinfo->chr_type = temp;
	else
		chr_err("%s: format error!\n", __func__);

	return size;
}

static DEVICE_ATTR_RW(chr_type);

static ssize_t Pump_Express_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int ret = 0, i = 0;
	bool is_ta_detected = false;
	struct mtk_charger *pinfo = dev->driver_data;
	struct chg_alg_device *alg = NULL;

	if (!pinfo) {
		chr_err("%s: pinfo is null\n", __func__);
		return sprintf(buf, "%d\n", is_ta_detected);
	}

	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = pinfo->alg[i];
		if (alg == NULL)
			continue;
		ret = chg_alg_is_algo_ready(alg);
		if (ret == ALG_RUNNING) {
			is_ta_detected = true;
			break;
		}
	}
	chr_err("%s: idx = %d, detect = %d\n", __func__, i, is_ta_detected);
	return sprintf(buf, "%d\n", is_ta_detected);
}

static DEVICE_ATTR_RO(Pump_Express);

static ssize_t ADC_Charger_Voltage_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;
	int vbus = get_vbus(pinfo); /* mV */

	chr_err("%s: %d\n", __func__, vbus);
	return sprintf(buf, "%d\n", vbus);
}

static DEVICE_ATTR_RO(ADC_Charger_Voltage);

static ssize_t Charger_Config_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;
	int chg_cfg = pinfo->config;

	chr_err("%s: %d\n", __func__, chg_cfg);
	return sprintf(buf, "%d\n", chg_cfg);
}

static DEVICE_ATTR_RO(Charger_Config);

static ssize_t input_current_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;
	int aicr = 0;

	aicr = pinfo->chg_data[CHG1_SETTING].thermal_input_current_limit;
	chr_err("%s: %d\n", __func__, aicr);
	return sprintf(buf, "%d\n", aicr);
}

static ssize_t input_current_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	struct charger_data *chg_data;
	signed int temp;

	chg_data = &pinfo->chg_data[CHG1_SETTING];
	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp < 0)
			chg_data->thermal_input_current_limit = 0;
		else
			chg_data->thermal_input_current_limit = temp;
	} else {
		chr_err("%s: format error!\n", __func__);
	}
	return size;
}

static DEVICE_ATTR_RW(input_current);

static ssize_t charger_log_level_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->log_level);
	return sprintf(buf, "%d\n", pinfo->log_level);
}

static ssize_t charger_log_level_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp < 0) {
			chr_err("%s: val is invalid: %ld\n", __func__, temp);
			temp = 0;
		}
		pinfo->log_level = temp;
		chr_err("%s: log_level=%d\n", __func__, pinfo->log_level);

	} else {
		chr_err("%s: format error!\n", __func__);
	}
	return size;
}

static DEVICE_ATTR_RW(charger_log_level);

static ssize_t BatteryNotify_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_info("%s: 0x%x\n", __func__, pinfo->notify_code);

	return sprintf(buf, "%u\n", pinfo->notify_code);
}

static ssize_t BatteryNotify_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	unsigned int reg = 0;
	int ret = 0;

	if (buf != NULL && size != 0) {
		ret = kstrtouint(buf, 16, &reg);
		if (ret < 0) {
			chr_err("%s: failed, ret = %d\n", __func__, ret);
			return ret;
		}
		pinfo->notify_code = reg;
		chr_info("%s: store code=0x%x\n", __func__, pinfo->notify_code);
		mtk_chgstat_notify(pinfo);
	}
	return size;
}

static DEVICE_ATTR_RW(BatteryNotify);

/* procfs */
static int mtk_chg_current_cmd_show(struct seq_file *m, void *data)
{
	struct mtk_charger *pinfo = m->private;

	seq_printf(m, "%d %d\n", pinfo->usb_unlimited, pinfo->cmd_discharging);
	return 0;
}

static int mtk_chg_current_cmd_open(struct inode *node, struct file *file)
{
	return single_open(file, mtk_chg_current_cmd_show, PDE_DATA(node));
}

static ssize_t mtk_chg_current_cmd_write(struct file *file,
		const char *buffer, size_t count, loff_t *data)
{
	int len = 0;
	char desc[32] = {0};
	int current_unlimited = 0;
	int cmd_discharging = 0;
	struct mtk_charger *info = PDE_DATA(file_inode(file));

	if (!info)
		return -EINVAL;
	if (count <= 0)
		return -EINVAL;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d", &current_unlimited, &cmd_discharging) == 2) {
		info->usb_unlimited = current_unlimited;
		if (cmd_discharging == 1) {
			info->cmd_discharging = true;
			charger_dev_enable(info->chg1_dev, false);
			charger_dev_do_event(info->chg1_dev,
					EVENT_DISCHARGE, 0);
		} else if (cmd_discharging == 0) {
			info->cmd_discharging = false;
			charger_dev_enable(info->chg1_dev, true);
			charger_dev_do_event(info->chg1_dev,
					EVENT_RECHARGE, 0);
		}

		chr_info("%s: current_unlimited=%d, cmd_discharging=%d\n",
			__func__, current_unlimited, cmd_discharging);
		return count;
	}

	chr_err("bad argument, echo [usb_unlimited] [disable] > current_cmd\n");
	return count;
}

static const struct file_operations mtk_chg_current_cmd_fops = {
	.owner = THIS_MODULE,
	.open = mtk_chg_current_cmd_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = mtk_chg_current_cmd_write,
};

static int mtk_chg_en_power_path_show(struct seq_file *m, void *data)
{
	struct mtk_charger *pinfo = m->private;
	bool power_path_en = true;

	charger_dev_is_powerpath_enabled(pinfo->chg1_dev, &power_path_en);
	seq_printf(m, "%d\n", power_path_en);

	return 0;
}

static int mtk_chg_en_power_path_open(struct inode *node, struct file *file)
{
	return single_open(file, mtk_chg_en_power_path_show, PDE_DATA(node));
}

static ssize_t mtk_chg_en_power_path_write(struct file *file,
		const char *buffer, size_t count, loff_t *data)
{
	int len = 0, ret = 0;
	char desc[32] = {0};
	unsigned int enable = 0;
	struct mtk_charger *info = PDE_DATA(file_inode(file));

	if (!info)
		return -EINVAL;
	if (count <= 0)
		return -EINVAL;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;

	desc[len] = '\0';

	ret = kstrtou32(desc, 10, &enable);
	if (ret == 0) {
		charger_dev_enable_powerpath(info->chg1_dev, enable);
		chr_info("%s: enable power path = %d\n", __func__, enable);
		return count;
	}

	chr_err("bad argument, echo [enable] > en_power_path\n");
	return count;
}

static const struct file_operations mtk_chg_en_power_path_fops = {
	.owner = THIS_MODULE,
	.open = mtk_chg_en_power_path_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = mtk_chg_en_power_path_write,
};

static int mtk_chg_en_safety_timer_show(struct seq_file *m, void *data)
{
	struct mtk_charger *pinfo = m->private;
	bool safety_timer_en = false;

	charger_dev_is_safety_timer_enabled(pinfo->chg1_dev, &safety_timer_en);
	seq_printf(m, "%d\n", safety_timer_en);

	return 0;
}

static int mtk_chg_en_safety_timer_open(struct inode *node, struct file *file)
{
	return single_open(file, mtk_chg_en_safety_timer_show, PDE_DATA(node));
}

static ssize_t mtk_chg_en_safety_timer_write(struct file *file,
	const char *buffer, size_t count, loff_t *data)
{
	int len = 0, ret = 0;
	char desc[32] = {0};
	unsigned int enable = 0;
	struct mtk_charger *info = PDE_DATA(file_inode(file));

	if (!info)
		return -EINVAL;
	if (count <= 0)
		return -EINVAL;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;

	desc[len] = '\0';

	ret = kstrtou32(desc, 10, &enable);
	if (ret == 0) {
		charger_dev_enable_safety_timer(info->chg1_dev, enable);
		chr_info("%s: enable safety timer = %d\n", __func__, enable);

		/* SW safety timer */
		if (info->sw_safety_timer_setting == true) {
			if (enable)
				info->enable_sw_safety_timer = true;
			else
				info->enable_sw_safety_timer = false;
		}

		return count;
	}

	chr_err("bad argument, echo [enable] > en_safety_timer\n");
	return count;
}

static const struct file_operations mtk_chg_en_safety_timer_fops = {
	.owner = THIS_MODULE,
	.open = mtk_chg_en_safety_timer_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = mtk_chg_en_safety_timer_write,
};

int mtk_chg_enable_vbus_ovp(bool enable)
{
	static struct mtk_charger *pinfo;
	int ret = 0;
	u32 sw_ovp = 0;
	struct power_supply *psy;

	if (pinfo == NULL) {
		psy = power_supply_get_by_name("mtk-master-charger");
		if (psy == NULL) {
			chr_err("[%s]psy is not rdy\n", __func__);
			return -1;
		}

		pinfo = (struct mtk_charger *)power_supply_get_drvdata(psy);
		if (pinfo == NULL) {
			chr_err("[%s]mtk_gauge is not rdy\n", __func__);
			return -1;
		}
	}

	if (enable)
		sw_ovp = pinfo->data.max_charger_voltage_setting;
	else
		sw_ovp = 15000000;

	/* Enable/Disable SW OVP status */
	pinfo->data.max_charger_voltage = sw_ovp;

	disable_hw_ovp(pinfo, enable);

	chr_err("[%s] en:%d ovp:%d\n",
			    __func__, enable, sw_ovp);
	return ret;
}

/* return false if vbus is over max_charger_voltage */
static bool mtk_chg_check_vbus(struct mtk_charger *info)
{
	int vchr = 0;

	if (info->ss_vchr != 0)
		vchr = info->ss_vchr;
	else {
		vchr = get_vbus(info) * 1000; /* uV */
		info->ss_vchr = vchr;
	}
	if (vchr > info->data.max_charger_voltage) {
		chr_err("%s: vbus(%d mV) > %d mV\n", __func__, vchr / 1000,
			info->data.max_charger_voltage / 1000);
		return false;
	}
	return true;
}

static void mtk_battery_notify_VCharger_check(struct mtk_charger *info)
{
#if defined(BATTERY_NOTIFY_CASE_0001_VCHARGER)
	int vchr = 0;

	if (info->ss_vchr != 0)
		vchr = info->ss_vchr;
	else {
		vchr = get_vbus(info) * 1000; /* uV */
		info->ss_vchr = vchr;
	}
	if (vchr < info->data.max_charger_voltage)
		info->notify_code &= ~CHG_VBUS_OV_STATUS;
	else {
		info->notify_code |= CHG_VBUS_OV_STATUS;
		chr_err("[BATTERY] charger_vol(%d mV) > %d mV\n",
			vchr / 1000, info->data.max_charger_voltage / 1000);
		mtk_chgstat_notify(info);
	}
#endif
}

static void mtk_battery_notify_VBatTemp_check(struct mtk_charger *info)
{
#if defined(BATTERY_NOTIFY_CASE_0002_VBATTEMP)
	if (info->battery_temp >= info->thermal.max_charge_temp) {
		info->notify_code |= CHG_BAT_OT_STATUS;
		chr_err("[BATTERY] bat_temp(%d) out of range(too high)\n",
			info->battery_temp);
		mtk_chgstat_notify(info);
	} else {
		info->notify_code &= ~CHG_BAT_OT_STATUS;
	}

	if (info->enable_sw_jeita == true) {
		if (info->battery_temp < info->data.temp_neg_10_thres) {
			info->notify_code |= CHG_BAT_LT_STATUS;
			chr_err("bat_temp(%d) out of range(too low)\n",
				info->battery_temp);
			mtk_chgstat_notify(info);
		} else {
			info->notify_code &= ~CHG_BAT_LT_STATUS;
		}
	} else {
#ifdef BAT_LOW_TEMP_PROTECT_ENABLE
		if (info->battery_temp < info->thermal.min_charge_temp) {
			info->notify_code |= CHG_BAT_LT_STATUS;
			chr_err("bat_temp(%d) out of range(too low)\n",
				info->battery_temp);
			mtk_chgstat_notify(info);
		} else {
			info->notify_code &= ~CHG_BAT_LT_STATUS;
		}
#endif
	}
#endif
}

static void mtk_battery_notify_UI_test(struct mtk_charger *info)
{
	switch (info->notify_test_mode) {
	case 1:
		info->notify_code = CHG_VBUS_OV_STATUS;
		chr_debug("[%s] CASE_0001_VCHARGER\n", __func__);
		break;
	case 2:
		info->notify_code = CHG_BAT_OT_STATUS;
		chr_debug("[%s] CASE_0002_VBATTEMP\n", __func__);
		break;
	case 3:
		info->notify_code = CHG_OC_STATUS;
		chr_debug("[%s] CASE_0003_ICHARGING\n", __func__);
		break;
	case 4:
		info->notify_code = CHG_BAT_OV_STATUS;
		chr_debug("[%s] CASE_0004_VBAT\n", __func__);
		break;
	case 5:
		info->notify_code = CHG_ST_TMO_STATUS;
		chr_debug("[%s] CASE_0005_TOTAL_CHARGINGTIME\n", __func__);
		break;
	case 6:
		info->notify_code = CHG_BAT_LT_STATUS;
		chr_debug("[%s] CASE6: VBATTEMP_LOW\n", __func__);
		break;
	case 7:
		info->notify_code = CHG_TYPEC_WD_STATUS;
		chr_debug("[%s] CASE7: Moisture Detection\n", __func__);
		break;
	default:
		chr_debug("[%s] Unknown BN_TestMode Code: %x\n",
			__func__, info->notify_test_mode);
	}
	mtk_chgstat_notify(info);
}

static void mtk_battery_notify_check(struct mtk_charger *info)
{
	if (info->notify_test_mode == 0x0000) {
		mtk_battery_notify_VCharger_check(info);
		mtk_battery_notify_VBatTemp_check(info);
	} else {
		mtk_battery_notify_UI_test(info);
	}
}

static void mtk_chg_get_tchg(struct mtk_charger *info)
{
	int ret;
	int tchg_min = -127, tchg_max = -127;
	struct charger_data *pdata;

	pdata = &info->chg_data[CHG1_SETTING];
	ret = charger_dev_get_temperature(info->chg1_dev, &tchg_min, &tchg_max);
	if (ret < 0) {
		pdata->junction_temp_min = -127;
		pdata->junction_temp_max = -127;
	} else {
		pdata->junction_temp_min = tchg_min;
		pdata->junction_temp_max = tchg_max;
	}

	if (info->chg2_dev) {
		pdata = &info->chg_data[CHG2_SETTING];
		ret = charger_dev_get_temperature(info->chg2_dev,
			&tchg_min, &tchg_max);

		if (ret < 0) {
			pdata->junction_temp_min = -127;
			pdata->junction_temp_max = -127;
		} else {
			pdata->junction_temp_min = tchg_min;
			pdata->junction_temp_max = tchg_max;
		}
	}
}
#ifdef CONFIG_HQ_PROJECT_OT8
/* TabA7 Lite code for OT8-5454 by shixuanxuan at 20220404 start */
#ifndef HQ_FACTORY_BUILD
static void get_battery_information(struct mtk_charger *info)
{
	struct power_supply *psy;
	union power_supply_propval pval = {0, };
	int rc;
	psy = power_supply_get_by_name("battery");
	if (!psy) {
		return;
	}

	rc = power_supply_get_property(psy, POWER_SUPPLY_PROP_BATT_FULL_CAPACITY, &pval);
	info->cust_batt_cap = pval.intval;

	rc = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	info->capacity = pval.intval;

	rc = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS, &pval);
	/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 start */
	info->batt_status = pval.intval;
	rc = power_supply_get_property(psy, POWER_SUPPLY_PROP_BATT_SOC_RECHG, &pval);
	info->cust_batt_rechg = pval.intval;
	/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 end */
}
#endif
#endif
/* TabA7 Lite code for OT8-5454 by shixuanxuan at 20220404 end */
/*HS03s for AL5626TDEV-224 by liuhong at 20220921 start*/
#ifdef CONFIG_HQ_PROJECT_HS03S
#ifndef HQ_FACTORY_BUILD
static void get_battery_information(struct mtk_charger *info)
{
	struct power_supply *psy;
	union power_supply_propval pval = {0, };
	int rc;
	psy = power_supply_get_by_name("battery");
	if (!psy) {
		return;
	}

	rc = power_supply_get_property(psy, POWER_SUPPLY_PROP_BATT_FULL_CAPACITY, &pval);
	info->cust_batt_cap = pval.intval;

	rc = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	info->capacity = pval.intval;

	rc = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS, &pval);
	info->batt_status = (pval.intval == POWER_SUPPLY_STATUS_CHARGING) ? 1 : 0;
}
#endif
#endif
/*HS03s for AL5626TDEV-224 by liuhong at 20220921 end*/
/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 start */
#ifdef CONFIG_HQ_PROJECT_HS04
#ifndef HQ_FACTORY_BUILD
static void get_battery_information(struct mtk_charger *info)
{
	struct power_supply *psy;
	union power_supply_propval pval = {0, };
	int rc;
	psy = power_supply_get_by_name("battery");
	if (!psy) {
		return;
	}

	rc = power_supply_get_property(psy, POWER_SUPPLY_PROP_BATT_FULL_CAPACITY, &pval);
	info->cust_batt_cap = pval.intval;

	rc = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	info->capacity = pval.intval;

	rc = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS, &pval);
	/*hs04_u for  AL6398AU-178 by shixuanxuan at 20240117 start*/
	//info->batt_status = (pval.intval == POWER_SUPPLY_STATUS_CHARGING) ? 1 : 0;
	info->batt_status = pval.intval;
	rc = power_supply_get_property(psy, POWER_SUPPLY_PROP_BATT_SOC_RECHG, &pval);
	info->cust_batt_rechg = pval.intval;
	/*hs04_u for  AL6398AU-178 by shixuanxuan at 20240117 end*/
}
#endif
#endif
/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 end */

/* HS03s code for SR-AL5625-01-35 by wenyaqi at 20210420 start */
/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
#ifndef HQ_FACTORY_BUILD	//ss version
static void basic_retail_app(struct mtk_charger *info);
#endif
static void ss_charger_check_status(struct mtk_charger *info)
{
	static bool input_suspend_old;
	static bool ovp_disable_old;
	/*HS03s added for DEVAL5626-463 by wangzikang at 20210729 start */
	bool input_suspend_hw;
	/*HS03s added for DEVAL5626-463 by wangzikang at 20210729 end */
	#ifdef HQ_FACTORY_BUILD //factory version
	static bool batt_cap_control_old;
	#endif
	/*hs04_u for  AL6398AU-178 by shixuanxuan at 20240117 start*/
	#ifdef CONFIG_HQ_PROJECT_HS04
	struct power_supply *psys = NULL;
	#endif
	/*hs04_u for  AL6398AU-178 by shixuanxuan at 20240117 end*/

	#ifndef HQ_FACTORY_BUILD	//ss version
	static bool store_mode_old;
#ifdef CONFIG_HQ_PROJECT_OT8
        /* TabA7 Lite code for OT8-5454 by shixuanxuan at 20220404 start */
#ifndef HQ_FACTORY_BUILD
	get_battery_information(info);

	/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 start */
	chr_err("%s:batt_protection info->cust_batt_cap = %d, info->cust_batt_rechg = %d, info->batt_status = %d, info->batt_full_flag = %d, info->capacity = %d, info->batt_protection_mode = %d\n",
			__func__, info->cust_batt_cap, info->cust_batt_rechg, info->batt_status, info->batt_full_flag, info->capacity, info->batt_protection_mode);
	if (info->cust_batt_cap == 100 && info->cust_batt_rechg == 1) {
		// 95 recharge
		if (info->batt_status == POWER_SUPPLY_STATUS_FULL && info->batt_full_flag == ENABLE_CHARGE) {
			info->batt_full_flag = DISABLE_CHARGE;
			info->cmd_discharging = true;
			charger_dev_enable(info->chg1_dev, false);
			charger_dev_do_event(info->chg1_dev, EVENT_DISCHARGE, 0);
			charger_dev_enable_powerpath(info->chg1_dev, true);
		} else if (info->capacity > 95 && info->batt_full_flag == DISABLE_CHARGE) {
			info->cmd_discharging = true;
			charger_dev_enable(info->chg1_dev, false);
			charger_dev_do_event(info->chg1_dev, EVENT_DISCHARGE, 0);
			charger_dev_enable_powerpath(info->chg1_dev, true);
		} else {
			if (info->batt_full_flag == DISABLE_CHARGE || info->batt_full_flag == PLUG_OUT) {
				info->batt_full_flag = ENABLE_CHARGE;
				info->cmd_discharging = false;
				charger_dev_enable(info->chg1_dev, true);
				charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
				charger_dev_enable_powerpath(info->chg1_dev, true);
			}
		}
	} else if (info->cust_batt_cap != 100) {
		//80 stop charge
		if ((info->batt_protection_mode == OPTION_MODE || info->batt_protection_mode == SLEEP_MODE)
			&& info->capacity >= info->cust_batt_cap
			&& (info->batt_full_flag == ENABLE_CHARGE || info->batt_status == POWER_SUPPLY_STATUS_CHARGING)) {
			info->batt_full_flag = DISABLE_CHARGE;
			info->cmd_discharging = true;
			charger_dev_enable(info->chg1_dev, false);
			charger_dev_do_event(info->chg1_dev, EVENT_DISCHARGE, 0);
			charger_dev_enable_powerpath(info->chg1_dev, true);
		} else if (info->batt_protection_mode == HIGHSOC_MODE
					&& info->capacity >= info->cust_batt_cap
					&& (info->batt_full_flag == ENABLE_CHARGE || info->batt_status == POWER_SUPPLY_STATUS_CHARGING)) {
			info->batt_full_flag = DISABLE_CHARGE;
			info->cmd_discharging = true;
			charger_dev_enable(info->chg1_dev, false);
			charger_dev_do_event(info->chg1_dev, EVENT_DISCHARGE, 0);
			charger_dev_enable_powerpath(info->chg1_dev, false);
		}
		if (((info->capacity <= info->cust_batt_cap - 2) && info->batt_full_flag == DISABLE_CHARGE) || info->batt_full_flag == PLUG_OUT) {
			info->batt_full_flag = ENABLE_CHARGE;
			info->cmd_discharging = false;
			charger_dev_enable(info->chg1_dev, true);
			charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
			charger_dev_enable_powerpath(info->chg1_dev, true);
		}
	} else {
		if (info->batt_full_flag == DISABLE_CHARGE || info->batt_full_flag == PLUG_OUT) {
			info->batt_full_flag = ENABLE_CHARGE;
			info->cmd_discharging = false;
			charger_dev_enable(info->chg1_dev, true);
			charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
			charger_dev_enable_powerpath(info->chg1_dev, true);
		}
	}
	/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 end */

/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 start */
#if 0
	if (info->cust_batt_cap != 100 && info->capacity >= info->cust_batt_cap
			&& (info->batt_full_flag == 0 || info->batt_status)) {
		info->batt_full_flag = 1;
		info->cmd_discharging = true;
		charger_dev_enable(info->chg1_dev, false);
		charger_dev_do_event(info->chg1_dev, EVENT_DISCHARGE, 0);
		charger_dev_enable_powerpath(info->chg1_dev, true);
	}

	/* Tab A7 lite_T for P221021-05487 by duanweiping at 20221024 start */
	/* Tab A7 lite_T for P221109-03592 by duanweiping at 20221109 start */
	if ((info->cust_batt_cap == 100 || info->capacity <= info->cust_batt_cap - 2)
	      && info->batt_full_flag == 1) {
		info->batt_full_flag = 0;
		info->cmd_discharging = false;
		charger_dev_enable(info->chg1_dev, true);
		charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
		charger_dev_enable_powerpath(info->chg1_dev, true);
	}
	/* Tab A7 lite_T for P221109-03592 by duanweiping at 20221109 end */
	/* Tab A7 lite_T for P221021-05487 by duanweiping at 20221024 end */
#endif
/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 end */

#endif
#endif
/*HS03s for AL5626TDEV-224 by liuhong at 20220921 start*/
#ifdef CONFIG_HQ_PROJECT_HS03S
#ifndef HQ_FACTORY_BUILD
	get_battery_information(info);

	if (info->cust_batt_cap != 100 && info->capacity >= info->cust_batt_cap
			&& (info->batt_full_flag == 0 || info->batt_status)) {
		info->batt_full_flag = 1;
		info->cmd_discharging = true;
		charger_dev_enable(info->chg1_dev, false);
		charger_dev_do_event(info->chg1_dev, EVENT_DISCHARGE, 0);
		charger_dev_enable_powerpath(info->chg1_dev, true);
	}

	/* HS03s_T for P221021-05487 by duanweiping at 20221024 start */
	/* HS03s_T for P221109-03592 by duanweiping at 20221109 start */
	if ((info->cust_batt_cap == 100 || info->capacity <= info->cust_batt_cap - 2)
	      && info->batt_full_flag == 1) {
		info->batt_full_flag = 0;
		info->cmd_discharging = false;
		charger_dev_enable(info->chg1_dev, true);
		charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
		charger_dev_enable_powerpath(info->chg1_dev, true);
	}
	/* HS03s_T for P221109-03592 by duanweiping at 20221109 end */
	/* HS03s_T for P221021-05487 by duanweiping at 20221024 end */

#endif
#endif
/*HS03s for AL5626TDEV-224 by liuhong at 20220921 end*/
/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 start */
#ifdef CONFIG_HQ_PROJECT_HS04
#ifndef HQ_FACTORY_BUILD
	get_battery_information(info);
/*hs04_u for  AL6398AU-178 by shixuanxuan at 20240117 start*/

	chr_info("%s:chg: batt_protection  cust_batt_cap = %d, cust_batt_rechg = %d, batt_status = %d,\
		batt_full_flag = %d, capacity = %d, batt_protection_mode = %d\n",
			__func__, info->cust_batt_cap, info->cust_batt_rechg, info->batt_status, info->batt_full_flag,
			info->capacity, info->batt_protection_mode);
	if (info->cust_batt_cap == 100 && info->cust_batt_rechg == 1) {
		// 95 recharge
		if (info->batt_status == POWER_SUPPLY_STATUS_FULL && info->batt_full_flag == ENABLE_CHARGE) {
			info->batt_full_flag = DISABLE_CHARGE;
			info->cmd_discharging = true;
			charger_dev_enable(info->chg1_dev, false);
			charger_dev_set_hiz_mode(info->chg1_dev, false);
			charger_dev_do_event(info->chg1_dev, EVENT_DISCHARGE, 0);
		} else if (info->capacity > 95 && info->batt_full_flag != ENABLE_CHARGE) {
			info->cmd_discharging = true;
			charger_dev_enable(info->chg1_dev, false);
			charger_dev_set_hiz_mode(info->chg1_dev, false);
			charger_dev_do_event(info->chg1_dev, EVENT_DISCHARGE, 0);
		} else {
			if (info->batt_full_flag != ENABLE_CHARGE) {
				info->batt_full_flag = ENABLE_CHARGE;
				info->cmd_discharging = false;
				charger_dev_enable(info->chg1_dev, true);
				charger_dev_set_hiz_mode(info->chg1_dev, false);
				charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
			}
		}
	} else if (info->cust_batt_cap != 100) {
		//80 stop charge
		if ((info->batt_protection_mode == OPTION_MODE || info->batt_protection_mode == SLEEP_MODE)
			&& info->capacity >= info->cust_batt_cap
			&& (info->batt_full_flag != DISABLE_CHARGE || info->batt_status == POWER_SUPPLY_STATUS_CHARGING)) {
			info->batt_full_flag = DISABLE_CHARGE;
			info->cmd_discharging = true;
			charger_dev_enable(info->chg1_dev, false);
			charger_dev_set_hiz_mode(info->chg1_dev, false);
			charger_dev_do_event(info->chg1_dev, EVENT_DISCHARGE, 0);
		} else if (info->batt_protection_mode == HIGHSOC_MODE
					&& info->capacity >= info->cust_batt_cap
					&& (info->batt_full_flag != DISABLE_CHARGE_HISOC || info->batt_status == POWER_SUPPLY_STATUS_CHARGING)) {
			info->batt_full_flag = DISABLE_CHARGE_HISOC;
			info->cmd_discharging = true;
			charger_dev_enable(info->chg1_dev, false);
			charger_dev_set_hiz_mode(info->chg1_dev, true);
			charger_dev_do_event(info->chg1_dev, EVENT_DISCHARGE, 0);
			pr_err("SXX hisoc mode");
		}
		if ((info->capacity <= info->cust_batt_cap - 2) && info->batt_full_flag != ENABLE_CHARGE) {
			info->batt_full_flag = ENABLE_CHARGE;
			info->cmd_discharging = false;
			charger_dev_enable(info->chg1_dev, true);
			charger_dev_set_hiz_mode(info->chg1_dev, false);
			charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
		}
	} else {
		if (info->batt_full_flag !=ENABLE_CHARGE) {
			info->batt_full_flag = ENABLE_CHARGE;
			info->cmd_discharging = false;
			charger_dev_enable(info->chg1_dev, true);
			charger_dev_set_hiz_mode(info->chg1_dev, false);
			charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
		}
	}
/* Remove Old battery protection */
/* 	if (info->cust_batt_cap != 100 && info->capacity >= info->cust_batt_cap
			&& (info->batt_full_flag == 0 || info->batt_status)) {
		info->batt_full_flag = 1;
		info->cmd_discharging = true;
		charger_dev_enable(info->chg1_dev, false);
		charger_dev_do_event(info->chg1_dev, EVENT_DISCHARGE, 0);
		charger_dev_enable_powerpath(info->chg1_dev, true);
	}
	if ((info->cust_batt_cap == 100 || info->capacity <= info->cust_batt_cap - 2)
			&& info->batt_full_flag == 1) {
		info->batt_full_flag = 0;
		info->cmd_discharging = false;
		charger_dev_enable(info->chg1_dev, true);
		charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
		charger_dev_enable_powerpath(info->chg1_dev, true);
	} */
/*hs04_u for  AL6398AU-178 by shixuanxuan at 20240117 end*/
#endif
#endif
/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 end*/
/* TabA7 Lite code for OT8-5454 by shixuanxuan at 20220404 end */
	if (info->batt_store_mode) {
		chr_debug("%s:store mode is working\n", __func__);
		basic_retail_app(info);
	} else if (info->batt_store_mode == false &&
		store_mode_old == true && info->input_suspend == true) {
		chr_debug("%s:store mode exit, restore input_suspend\n", __func__);
		info->input_suspend = false;
	}
	store_mode_old = info->batt_store_mode;
	#endif
	if (!mtk_chg_check_vbus(info)) {
		info->ovp_disable = true;
	} else {
		info->ovp_disable = false;
	}
/*hs04_u for  AL6398AU-178 by shixuanxuan at 20240117 start*/
#ifdef CONFIG_HQ_PROJECT_HS04
	if (info->batt_protection_mode == HIGHSOC_MODE && info->batt_full_flag != ENABLE_CHARGE) {
		return;
	}
#endif
/*hs04_u for  AL6398AU-178 by shixuanxuan at 20240117 end*/
	/*HS03s added for DEVAL5626-463 by wangzikang at 20210729 start */
	input_suspend_hw = charger_dev_get_hiz_mode(info->chg1_dev);
	/* TabA7 Lite code for P220922-06047 by duanweiping at 20220927 start */
#ifdef CONFIG_HQ_PROJECT_OT8
	charger_dev_is_powerpath_enabled(info->chg1_dev,&input_suspend_hw);
	/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 start */
	if(info->batt_protection_mode == HIGHSOC_MODE || info->batt_full_flag == DISABLE_CHARGE || info->batt_full_flag == PLUG_OUT)
	{
		chr_err("%s:80 protection:info->batt_protection_mode=%d, info->batt_full_flag=%d\n", __func__,
				info->batt_protection_mode, info->batt_full_flag);
		return;
	}
	/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 end */
#endif
	/* TabA7 Lite code for P220922-06047 by duanweiping at 20220927 end */
	chr_err("%s: input_suspend_hw=%d",__func__,input_suspend_hw);
	/*HS03s added for DEVAL5626-463 by wangzikang at 20210729 end */
	if (input_suspend_old == info->input_suspend &&
		/*HS03s added for DEVAL5626-463 by wangzikang at 20210729 start */
#ifdef CONFIG_HQ_PROJECT_HS03S
                /* modify code for O6 */
		(input_suspend_hw == info->input_suspend) &&
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
                /* modify code for O6 */
		(input_suspend_hw == info->input_suspend) &&
#endif
	/* TabA7 Lite code for P220922-06047 by duanweiping at 20220927 start */
#ifdef CONFIG_HQ_PROJECT_OT8
                /* modify code for OT8 */
		(!input_suspend_hw == info->input_suspend) &&
#endif
	/* TabA7 Lite code for P220922-06047 by duanweiping at 20220927 end */
		/*HS03s added for DEVAL5626-463 by wangzikang at 20210729 end */
		#ifdef HQ_FACTORY_BUILD //factory version
		ovp_disable_old == info->ovp_disable &&
		batt_cap_control_old == info->batt_cap_control) {
		#else
		ovp_disable_old == info->ovp_disable) {
		#endif
			#ifdef HQ_FACTORY_BUILD //factory version
			batt_cap_control_old = info->batt_cap_control;
			chr_err("%s:same:input_suspend=%d, ovp_disable=%d, batt_cap_control=%d\n", __func__,
				info->input_suspend, info->ovp_disable, info->batt_cap_control);
			#else
			chr_err("%s:same:input_suspend=%d, ovp_disable=%d\n", __func__,
				info->input_suspend, info->ovp_disable);
			#endif
			return;
		}
	#ifdef HQ_FACTORY_BUILD //factory version
	if (info->input_suspend == true || info->ovp_disable == true ||
		info->batt_cap_control == true) {
	#else
	if (info->input_suspend == true || info->ovp_disable == true) {
	#endif
		info->cmd_discharging = true;
		charger_dev_enable(info->chg1_dev, false);
		charger_dev_do_event(info->chg1_dev,
			EVENT_DISCHARGE, 0);
		/*HS03s for DEVAL5625-1125 by wenyaqi at 20210607 start*/
#ifdef CONFIG_HQ_PROJECT_HS03S
		charger_dev_set_hiz_mode(info->chg1_dev, true);
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
		charger_dev_set_hiz_mode(info->chg1_dev, true);
#endif
#ifdef CONFIG_HQ_PROJECT_OT8
		charger_dev_enable_powerpath(info->chg1_dev, false);
#endif
	} else {
		info->cmd_discharging = false;
		charger_dev_enable(info->chg1_dev, true);
		charger_dev_do_event(info->chg1_dev,
			EVENT_RECHARGE, 0);
#ifdef CONFIG_HQ_PROJECT_HS03S
		charger_dev_set_hiz_mode(info->chg1_dev, false);
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
		charger_dev_set_hiz_mode(info->chg1_dev, false);
#endif
#ifdef CONFIG_HQ_PROJECT_OT8
		charger_dev_enable_powerpath(info->chg1_dev, true);
#endif
		/*HS03s for DEVAL5625-1125 by wenyaqi at 20210607 end*/
	}

	input_suspend_old = info->input_suspend;
	ovp_disable_old = info->ovp_disable;
	#ifdef HQ_FACTORY_BUILD //factory version
	batt_cap_control_old = info->batt_cap_control;
	chr_err("%s:input_suspend=%d, ovp_disable=%d, batt_cap_control=%d\n", __func__,
		info->input_suspend, info->ovp_disable, info->batt_cap_control);
	#else
	chr_err("%s:input_suspend=%d, ovp_disable=%d\n", __func__, info->input_suspend, info->ovp_disable);
	#endif
}
/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 end*/
/* HS03s code for SR-AL5625-01-35 by wenyaqi at 20210420 end */

static void charger_check_status(struct mtk_charger *info)
{
	bool charging = true;
	int temperature;
	struct battery_thermal_protection_data *thermal;

	if (get_charger_type(info) == POWER_SUPPLY_TYPE_UNKNOWN) {
		if (!mtk_chg_check_vbus(info)) {
			mtk_battery_notify_check(info);
		}
		return;
	}

	temperature = info->battery_temp;
	thermal = &info->thermal;

	if (info->enable_sw_jeita == true) {
		do_sw_jeita_state_machine(info);
		if (info->sw_jeita.charging == false) {
			charging = false;
			goto stop_charging;
		}
	} else {

		if (thermal->enable_min_charge_temp) {
			if (temperature < thermal->min_charge_temp) {
				chr_err("Battery Under Temperature or NTC fail %d %d\n",
					temperature, thermal->min_charge_temp);
				thermal->sm = BAT_TEMP_LOW;
				charging = false;
				goto stop_charging;
			} else if (thermal->sm == BAT_TEMP_LOW) {
				if (temperature >=
				    thermal->min_charge_temp_plus_x_degree) {
					chr_err("Battery Temperature raise from %d to %d(%d), allow charging!!\n",
					thermal->min_charge_temp,
					temperature,
					thermal->min_charge_temp_plus_x_degree);
					thermal->sm = BAT_TEMP_NORMAL;
				} else {
					charging = false;
					goto stop_charging;
				}
			}
		}

		if (temperature >= thermal->max_charge_temp) {
			chr_err("Battery over Temperature or NTC fail %d %d\n",
				temperature, thermal->max_charge_temp);
			thermal->sm = BAT_TEMP_HIGH;
			charging = false;
			goto stop_charging;
		} else if (thermal->sm == BAT_TEMP_HIGH) {
			if (temperature
			    < thermal->max_charge_temp_minus_x_degree) {
				chr_err("Battery Temperature raise from %d to %d(%d), allow charging!!\n",
				thermal->max_charge_temp,
				temperature,
				thermal->max_charge_temp_minus_x_degree);
				thermal->sm = BAT_TEMP_NORMAL;
			} else {
				charging = false;
				goto stop_charging;
			}
		}
	}

	mtk_chg_get_tchg(info);

	if (!mtk_chg_check_vbus(info)) {
		charging = false;
		goto stop_charging;
	}

	if (info->cmd_discharging)
		charging = false;
	if (info->safety_timeout)
		charging = false;
	if (info->vbusov_stat)
		charging = false;

stop_charging:
	mtk_battery_notify_check(info);

	chr_err("tmp:%d (jeita:%d sm:%d cv:%d en:%d) (sm:%d) en:%d c:%d s:%d ov:%d %d %d\n",
		temperature, info->enable_sw_jeita, info->sw_jeita.sm,
		info->sw_jeita.cv, info->sw_jeita.charging, thermal->sm,
		charging, info->cmd_discharging, info->safety_timeout,
		info->vbusov_stat, info->can_charging, charging);

	if (charging != info->can_charging)
		_mtk_enable_charging(info, charging);

	info->can_charging = charging;
}

static bool charger_init_algo(struct mtk_charger *info)
{
	struct chg_alg_device *alg;
	int idx = 0;

	alg = get_chg_alg_by_name("pe4");
	info->alg[idx] = alg;
	if (alg == NULL)
		chr_err("get pe4 fail\n");
	else {
		chr_err("get pe4 success\n");
		alg->config = info->config;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}
	idx++;

	alg = get_chg_alg_by_name("pd");
	info->alg[idx] = alg;
	if (alg == NULL) {
		chr_err("get pd fail\n");
		return false;
	} else {
		chr_err("get pd success\n");
		alg->config = info->config;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}
	idx++;

	alg = get_chg_alg_by_name("pe2");
	info->alg[idx] = alg;
	if (alg == NULL)
		chr_err("get pe2 fail\n");
	else {
		chr_err("get pe2 success\n");
		alg->config = info->config;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}
	idx++;

	alg = get_chg_alg_by_name("pe");
	info->alg[idx] = alg;
	if (alg == NULL)
		chr_err("get pe fail\n");
	else {
		chr_err("get pe success\n");
		alg->config = info->config;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}

	info->chg1_dev = get_charger_by_name("primary_chg");
	if (info->chg1_dev)
		chr_err("Found primary charger\n");
	else {
		chr_err("*** Error : can't find primary charger ***\n");
		return false;
	}

	chr_err("config is %d\n", info->config);
	if (info->config == DUAL_CHARGERS_IN_SERIES) {
		info->chg2_dev = get_charger_by_name("secondary_chg");
		if (info->chg2_dev)
			chr_err("Found secondary charger\n");
		else {
			chr_err("*** Error : can't find secondary charger ***\n");
			return false;
		}
	}

	chr_err("register chg1 notifier %d %d\n",
		info->chg1_dev != NULL, info->algo.do_event != NULL);
	if (info->chg1_dev != NULL && info->algo.do_event != NULL) {
		chr_err("register chg1 notifier done\n");
		info->chg1_nb.notifier_call = info->algo.do_event;
		register_charger_device_notifier(info->chg1_dev,
						&info->chg1_nb);
		charger_dev_set_drvdata(info->chg1_dev, info);
	}

	return true;
}

#ifdef CONFIG_HQ_PROJECT_HS03S
/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
#ifndef HQ_FACTORY_BUILD	//ss version
#define RETAIL_APP_DETECT_TIMER     60000
#define SALE_CODE_STR_LEN 3
static char sales_code_from_cmdline[SALE_CODE_STR_LEN + 1];

static int __init sales_code_setup(char *str)
{
	strlcpy(sales_code_from_cmdline, str,
		ARRAY_SIZE(sales_code_from_cmdline));

	return 1;
}
__setup("androidboot.sales_code=", sales_code_setup);

bool sales_code_is(char* str)
{
	return !strncmp(sales_code_from_cmdline, str,
				SALE_CODE_STR_LEN + 1);
}

int sum_get_prop_from_battery(struct mtk_charger *info,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct power_supply *psy;
	int rc;
	psy = power_supply_get_by_name("battery");
	if (!psy) {
		return -EINVAL;
	}

	rc = power_supply_get_property(psy, psp, val);
	return rc;
}

static void basic_retail_app(struct mtk_charger *info)
{
	union power_supply_propval pval = {0, };
	int ret;
	int retail_app_dischg_threshold = 0;
	int retail_app_chg_threshold = 0;

	if(sales_code_is("VZW") || sales_code_is("VPP")) {
		retail_app_dischg_threshold = 35;
		retail_app_chg_threshold = 30;
	} else {
		retail_app_dischg_threshold = 70;
		retail_app_chg_threshold = 60;
	}

	ret = sum_get_prop_from_battery(info, POWER_SUPPLY_PROP_CAPACITY ,&pval);
	if (pval.intval >= retail_app_dischg_threshold) {
		info->input_suspend = true;
	} else if (pval.intval < retail_app_chg_threshold) {
		info->input_suspend = false;
	}

	chr_err("%s: sales_code:%s,dischg_thres:%d,chg_thres:%d,soc_now:%d,c:%d,input_suspend:%d\n",
		__func__, sales_code_from_cmdline, retail_app_dischg_threshold, retail_app_chg_threshold,
		pval.intval, info->cmd_discharging, info->input_suspend);
}

static void ss_set_batt_store_mode(struct mtk_charger *info,
	const union power_supply_propval *val)
{
	int store_mode = val->intval;

	if (store_mode == 1) {
		info->batt_store_mode = true;
		schedule_delayed_work(&info->retail_app_status_change_work, 0);
	} else {
		info->batt_store_mode = false;
		cancel_delayed_work_sync(&info->retail_app_status_change_work);
	}
	chr_err("%s: batt_store_mode:%d\n", __func__, info->batt_store_mode);
}

static void ss_retail_app_status_change_work(struct work_struct *work)
{
	struct mtk_charger *info = container_of(work,
			struct mtk_charger, retail_app_status_change_work.work);

	basic_retail_app(info);

	schedule_delayed_work(&info->retail_app_status_change_work, msecs_to_jiffies(RETAIL_APP_DETECT_TIMER));
}
#endif
/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 end*/

#endif
#ifdef CONFIG_HQ_PROJECT_HS04
/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
#ifndef HQ_FACTORY_BUILD	//ss version
#define RETAIL_APP_DETECT_TIMER     60000
#define SALE_CODE_STR_LEN 3
static char sales_code_from_cmdline[SALE_CODE_STR_LEN + 1];

static int __init sales_code_setup(char *str)
{
	strlcpy(sales_code_from_cmdline, str,
		ARRAY_SIZE(sales_code_from_cmdline));

	return 1;
}
__setup("androidboot.sales_code=", sales_code_setup);

bool sales_code_is(char* str)
{
	return !strncmp(sales_code_from_cmdline, str,
				SALE_CODE_STR_LEN + 1);
}

int sum_get_prop_from_battery(struct mtk_charger *info,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct power_supply *psy;
	int rc;
	psy = power_supply_get_by_name("battery");
	if (!psy) {
		return -EINVAL;
	}

	rc = power_supply_get_property(psy, psp, val);
	return rc;
}

static void basic_retail_app(struct mtk_charger *info)
{
	union power_supply_propval pval = {0, };
	int ret;
	int retail_app_dischg_threshold = 0;
	int retail_app_chg_threshold = 0;

	if(sales_code_is("VZW") || sales_code_is("VPP")) {
		retail_app_dischg_threshold = 35;
		retail_app_chg_threshold = 30;
	} else {
		retail_app_dischg_threshold = 70;
		retail_app_chg_threshold = 60;
	}

	ret = sum_get_prop_from_battery(info, POWER_SUPPLY_PROP_CAPACITY ,&pval);
	if (pval.intval >= retail_app_dischg_threshold) {
		info->input_suspend = true;
	} else if (pval.intval < retail_app_chg_threshold) {
		info->input_suspend = false;
	}

	chr_err("%s: sales_code:%s,dischg_thres:%d,chg_thres:%d,soc_now:%d,c:%d,input_suspend:%d\n",
		__func__, sales_code_from_cmdline, retail_app_dischg_threshold, retail_app_chg_threshold,
		pval.intval, info->cmd_discharging, info->input_suspend);
}

static void ss_set_batt_store_mode(struct mtk_charger *info,
	const union power_supply_propval *val)
{
	int store_mode = val->intval;

	if (store_mode == 1) {
		info->batt_store_mode = true;
		schedule_delayed_work(&info->retail_app_status_change_work, 0);
	} else {
		info->batt_store_mode = false;
		cancel_delayed_work_sync(&info->retail_app_status_change_work);
	}
	chr_err("%s: batt_store_mode:%d\n", __func__, info->batt_store_mode);
}

static void ss_retail_app_status_change_work(struct work_struct *work)
{
	struct mtk_charger *info = container_of(work,
			struct mtk_charger, retail_app_status_change_work.work);

	basic_retail_app(info);

	schedule_delayed_work(&info->retail_app_status_change_work, msecs_to_jiffies(RETAIL_APP_DETECT_TIMER));
}
#endif
/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 end*/

#endif
#ifdef CONFIG_HQ_PROJECT_OT8
//for o8

/*TabA7 Lite  code for SR-AX3565-01-109 by gaoxugang at 20201124 start*/
#if !defined(HQ_FACTORY_BUILD)
#define RETAIL_APP_DETECT_TIMER     60000
extern int battery_store_mode;
/*TabA7 Lite  code for SR-AX3565-01-187 by gaoxugang at 20201203 start*/
#define SALE_CODE_STR_LEN 3
static char sales_code_from_cmdline[SALE_CODE_STR_LEN + 1];

static int __init sales_code_setup(char *str)
{
	strlcpy(sales_code_from_cmdline, str,
		ARRAY_SIZE(sales_code_from_cmdline));

	return 1;
}
__setup("androidboot.sales_code=", sales_code_setup);

bool sales_code_is(char* str)
{
	return !strncmp(sales_code_from_cmdline, str,
				SALE_CODE_STR_LEN + 1);
}
/*TabA7 Lite  code for SR-AX3565-01-187 by gaoxugang at 20201203 end*/
int sum_get_prop_from_battery(struct mtk_charger *info,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct power_supply *psy;
	int rc;
	psy = power_supply_get_by_name("battery");
	if (!psy) {
		return -EINVAL;
	}

	rc = power_supply_get_property(psy, psp, val);
	return rc;
}
/* AX3565 for SR-AX3565-01-737 add battery protect function by shixuanxuan at 2021/01/13 start */
//#if !defined(HQ_FACTORY_BUILD)
/*static void hq_charging_count_work(struct work_struct *work)
{
	struct mtk_charger *info = container_of(work,
			struct mtk_charger, charging_count_work.work);
	hq_update_charge_state(info);
	schedule_delayed_work(&info->charging_count_work, msecs_to_jiffies(CHR_COUNT_TIME));
}
#endif*/
/* AX3565 for SR-AX3565-01-737 add battery protect function by shixuanxuan at 2021/01/13 end */
/*TabA7 Lite code for P210330-05709 by wenyaqi at 20210401 start*/
static void basic_retail_app(struct mtk_charger *info)
{
	union power_supply_propval pval = {0, };
	int ret;
	int retail_app_dischg_threshold = 0;
	int retail_app_chg_threshold = 0;

	if(sales_code_is("VZW") || sales_code_is("VPP")) {
		retail_app_dischg_threshold = 35;
		retail_app_chg_threshold = 30;
	} else {
		retail_app_dischg_threshold = 70;
		retail_app_chg_threshold = 60;
	}

	ret = sum_get_prop_from_battery(info, POWER_SUPPLY_PROP_CAPACITY ,&pval);
	if (pval.intval >= retail_app_dischg_threshold) {
		info->input_suspend = true;
	} else if (pval.intval < retail_app_chg_threshold) {
		info->input_suspend = false;
	}

	chr_err("%s: sales_code:%s,dischg_thres:%d,chg_thres:%d,soc_now:%d,c:%d,input_suspend:%d\n",
		__func__, sales_code_from_cmdline, retail_app_dischg_threshold, retail_app_chg_threshold,
		pval.intval, info->cmd_discharging, info->input_suspend);
}

/*TabA7 Lite code for SR-AX3565-01-109 by wenyaqi at 20210416 start*/
static void ss_set_batt_store_mode(struct mtk_charger *info,
	const union power_supply_propval *val)
{
	int store_mode = val->intval;

	if (store_mode == 1) {
		info->batt_store_mode = true;
		schedule_delayed_work(&info->retail_app_status_change_work, 0);
	} else {
		info->batt_store_mode = false;
		cancel_delayed_work_sync(&info->retail_app_status_change_work);
	}
	chr_err("%s: batt_store_mode:%d\n", __func__, info->batt_store_mode);
}
/*TabA7 Lite code for SR-AX3565-01-109 by wenyaqi at 20210416 end*/

static void ss_retail_app_status_change_work(struct work_struct *work)
{
	struct mtk_charger *info = container_of(work,
			struct mtk_charger, retail_app_status_change_work.work);

	basic_retail_app(info);

	schedule_delayed_work(&info->retail_app_status_change_work, msecs_to_jiffies(RETAIL_APP_DETECT_TIMER));
}
#endif
/*TabA7 Lite code for P210330-05709 by wenyaqi at 20210401 end*/
/*TabA7 Lite  code for SR-AX3565-01-109 by gaoxugang at 20201124 end*/

#endif

static int mtk_charger_plug_out(struct mtk_charger *info)
{
	struct charger_data *pdata1 = &info->chg_data[CHG1_SETTING];
	struct charger_data *pdata2 = &info->chg_data[CHG2_SETTING];
	struct chg_alg_device *alg;
	struct chg_alg_notify notify;
	int i;

	chr_err("%s\n", __func__);
	info->chr_type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->charger_thread_polling = false;
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 start */
	#if !defined(HQ_FACTORY_BUILD)
	/* HS03s code for HQ00001 Modify battery protect function by shixuanxuan at 2021/05/10 start */
	info->vbus_rising = 0;
	/* TabA7 Lite code for HQ00001 Modify battery protect function by shixuanxuan at 2021/05/10 end */
#ifdef CONFIG_HQ_PROJECT_OT8
    /* TabA7 Lite code for OT8-5454 by shixuanxuan at 20220404 start */
	info->batt_full_flag = 0;
	info->cmd_discharging = false;
	charger_dev_enable(info->chg1_dev, true);
	charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
	charger_dev_enable_powerpath(info->chg1_dev, true);
	/* TabA7 Lite code for OT8-5454 by shixuanxuan at 20220404 end */
/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 start */
#ifndef HQ_FACTORY_BUILD
	if (info->batt_full_flag == ENABLE_CHARGE) {
		info->batt_full_flag = PLUG_OUT;
	}
	chr_err("mtk_charger_plug_out batt_full_flag = %d\n", info->batt_full_flag);
#endif
/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 end */
#endif
/*HS03s for AL5626TDEV-224 by liuhong at 20220921 start*/
#ifdef CONFIG_HQ_PROJECT_HS03S
	info->batt_full_flag = 0;
	info->cmd_discharging = false;
	charger_dev_enable(info->chg1_dev, true);
	charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
	charger_dev_enable_powerpath(info->chg1_dev, true);
#endif
/*HS03s for AL5626TDEV-224 by liuhong at 20220921 end*/
/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 start */
#ifdef CONFIG_HQ_PROJECT_HS04
	info->batt_full_flag = 0;
	info->cmd_discharging = false;
	charger_dev_enable(info->chg1_dev, true);
	charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
	charger_dev_enable_powerpath(info->chg1_dev, true);
#endif
/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 end*/
	info->over_cap_count = 0;
	info->cap_hold_count = 0;
	info->recharge_count = 0;
	info->interval_time = 0;
	cancel_delayed_work_sync(&info->charging_count_work);
	hq_enable_charging(info, true);
	#endif
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 end */

	pdata1->disable_charging_count = 0;
	pdata1->input_current_limit_by_aicl = -1;
	pdata2->disable_charging_count = 0;

	notify.evt = EVT_PLUG_OUT;
	notify.value = 0;
	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		chg_alg_notifier_call(alg, &notify);
	}

	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
	#ifdef CONFIG_AFC_CHARGER
	afc_set_is_enable(info, false);
	afc_plugout_reset(info);
	info->afc_sts = AFC_INIT;
	#endif
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/
	charger_dev_set_input_current(info->chg1_dev, 100000);
	charger_dev_set_mivr(info->chg1_dev, info->data.min_charger_voltage);
	charger_dev_plug_out(info->chg1_dev);

	return 0;
}

int thub_chr_type = 0;
static int mtk_charger_plug_in(struct mtk_charger *info,
				int chr_type)
{
	struct chg_alg_device *alg;
	struct chg_alg_notify notify;
	int i;

	chr_debug("%s\n",
		__func__);

	info->chr_type = chr_type;
	thub_chr_type = info->chr_type;
	info->charger_thread_polling = true;
	info->can_charging = true;
	//info->enable_dynamic_cv = true;
	info->safety_timeout = false;
	info->vbusov_stat = false;

	chr_err("mtk_is_charger_on plug in, zhi type:%d, info->chr_type = %d\n", chr_type, info->chr_type);

	notify.evt = EVT_PLUG_IN;
	notify.value = 0;
	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		chg_alg_notifier_call(alg, &notify);
	}

	charger_dev_plug_in(info->chg1_dev);
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 start */
	#if !defined(HQ_FACTORY_BUILD)
	/* HS03s code for HQ00001 Modify battery protect function by shixuanxuan at 2021/05/10 start */
	info->vbus_rising = 1;
  	info->base_time = 0;
	/* HS03s code for HQ00001 Modify battery protect function by shixuanxuan at 2021/05/10 end */
	info->interval_time = 0;
	schedule_delayed_work(&info->charging_count_work, 0);
	#endif
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 end */
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
	#ifdef CONFIG_AFC_CHARGER
	if (info->chr_type == POWER_SUPPLY_TYPE_USB_DCP &&
		info->pd_type != MTK_PD_CONNECT_PE_READY_SNK &&      //PD2.0
		info->pd_type != MTK_PD_CONNECT_PE_READY_SNK_PD30 && //PD3.0
		info->pd_type != MTK_PD_CONNECT_PE_READY_SNK_APDO)   //PPS
		afc_set_is_enable(info, true);
	#endif
	/*hs04_u for  AL6398AU-178 by shixuanxuan at 20240117 start*/
	#ifdef CONFIG_HQ_PROJECT_HS04
	gxy_ttf_work_start();
	#endif
	/*hs04_u for  AL6398AU-178 by shixuanxuan at 20240117 end*/
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/

	return 0;
}


static bool mtk_is_charger_on(struct mtk_charger *info)
{
	int chr_type;

	chr_type = get_charger_type(info);
	if (chr_type == POWER_SUPPLY_TYPE_UNKNOWN) {
		if (info->chr_type != POWER_SUPPLY_TYPE_UNKNOWN) {
			mtk_charger_plug_out(info);
#ifdef CONFIG_HQ_PROJECT_HS03S
    			/* modify code for O6 */
			/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
			#ifndef HQ_FACTORY_BUILD	//ss version
			if (info->batt_store_mode) {
				__pm_relax(info->charger_wakelock_app);
				cancel_delayed_work_sync(&info->retail_app_status_change_work);
			}
			#endif
			/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 end*/
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
    			/* modify code for O6 */
			/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
			#ifndef HQ_FACTORY_BUILD	//ss version
			if (info->batt_store_mode) {
				__pm_relax(info->charger_wakelock_app);
				cancel_delayed_work_sync(&info->retail_app_status_change_work);
			}
			#endif
			/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 end*/
#endif
#ifdef CONFIG_HQ_PROJECT_OT8
    			/* modify code for O8 */
			/*TabA7 Lite  code for SR-AX3565-01-109 by gaoxugang at 20201124 start*/
			#if !defined(HQ_FACTORY_BUILD)
			if (battery_store_mode) {
				__pm_relax(info->charger_wakelock_app);
				cancel_delayed_work_sync(&info->retail_app_status_change_work);
			}
#endif
			/*TabA7 Lite  code for SR-AX3565-01-109 by gaoxugang at 20201124 end*/
			#endif
			mutex_lock(&info->cable_out_lock);
			info->cable_out_cnt = 0;
			mutex_unlock(&info->cable_out_lock);
		}
	} else {
		if (info->chr_type == POWER_SUPPLY_TYPE_UNKNOWN)
			mtk_charger_plug_in(info, chr_type);
		else {
			info->chr_type = chr_type;
			thub_chr_type = info->chr_type;
		}

		if (info->cable_out_cnt > 0) {
			mtk_charger_plug_out(info);
			mtk_charger_plug_in(info, chr_type);
			mutex_lock(&info->cable_out_lock);
			info->cable_out_cnt = 0;
			mutex_unlock(&info->cable_out_lock);
		}
#ifdef CONFIG_HQ_PROJECT_HS03S
    	/* modify code for O6 */
		/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
		#ifndef HQ_FACTORY_BUILD	//ss version
		if (info->batt_store_mode) {
			__pm_stay_awake(info->charger_wakelock_app);
			schedule_delayed_work(&info->retail_app_status_change_work, 0);
		}
		#endif
		/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 end*/
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
    	/* modify code for O6 */
		/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
		#ifndef HQ_FACTORY_BUILD	//ss version
		if (info->batt_store_mode) {
			__pm_stay_awake(info->charger_wakelock_app);
			schedule_delayed_work(&info->retail_app_status_change_work, 0);
		}
		#endif
		/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 end*/
#endif
#ifdef CONFIG_HQ_PROJECT_OT8
    	/* modify code for O8 */
		/*TabA7 Lite  code for SR-AX3565-01-109 by gaoxugang at 20201124 start*/
		#if !defined(HQ_FACTORY_BUILD)
		if (battery_store_mode) {
			__pm_stay_awake(info->charger_wakelock_app);
			schedule_delayed_work(&info->retail_app_status_change_work, 0);
		}
		#endif
		/*TabA7 Lite  code for SR-AX3565-01-109 by gaoxugang at 20201124 end*/
#endif
	}

	if (chr_type == POWER_SUPPLY_TYPE_UNKNOWN)
		return false;

	return true;
}

/*HS03s for P210730-04606 by wangzikang at 20210816 start*/
#ifndef HQ_FACTORY_BUILD	//ss version
#ifdef CONFIG_HQ_PROJECT_HS03S
    /* modify code for O6 */
static void offmode_delay_work(struct work_struct *work)
{
	struct mtk_charger *info = container_of(work,
			struct mtk_charger, poweroff_dwork.work);
	int vbus = 0;

	chr_err("%s: ENTER\n", __func__);
	vbus = get_vbus(info);
	if (vbus >= 0 && vbus < 2500) {
		chr_err("Recheck:Unplug Charger/USB in KPOC mode, vbus=%d, shutdown\n", vbus);
		kernel_power_off();
	} else {
		chr_err("Recheck:Vbus recovery.\n");
	}
}
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
    /* modify code for O6 */
static void offmode_delay_work(struct work_struct *work)
{
	struct mtk_charger *info = container_of(work,
			struct mtk_charger, poweroff_dwork.work);
	int vbus = 0;

	chr_err("%s: ENTER\n", __func__);
	vbus = get_vbus(info);
	if (vbus >= 0 && vbus < 2500) {
		chr_err("Recheck:Unplug Charger/USB in KPOC mode, vbus=%d, shutdown\n", vbus);
		kernel_power_off();
	} else {
		chr_err("Recheck:Vbus recovery.\n");
	}
}
#endif
/*HS03s for P210730-04606 by wangzikang at 20210816 start*/

#define RECHECK_VBUS_TIME_MS 3000
static void kpoc_power_off_check(struct mtk_charger *info)
{
	unsigned int boot_mode = info->bootmode;
	int vbus = 0;

	/* 8 = KERNEL_POWER_OFF_CHARGING_BOOT */
	/* 9 = LOW_POWER_OFF_CHARGING_BOOT */
	if (boot_mode == 8 || boot_mode == 9) {
		vbus = get_vbus(info);
		if (vbus >= 0 && vbus < 2500 && !mtk_is_charger_on(info) && !info->pd_reset) {
#ifdef CONFIG_HQ_PROJECT_HS03S
			/* modify code for O6 */
			chr_err("Unplug Charger/USB in KPOC mode, vbus=%d, schedule the power off delay work\n", vbus);
			schedule_delayed_work(&info->poweroff_dwork, msecs_to_jiffies(RECHECK_VBUS_TIME_MS));
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
			/* modify code for O6 */
			chr_err("Unplug Charger/USB in KPOC mode, vbus=%d, schedule the power off delay work\n", vbus);
			schedule_delayed_work(&info->poweroff_dwork, msecs_to_jiffies(RECHECK_VBUS_TIME_MS));
#endif
#ifdef CONFIG_HQ_PROJECT_OT8
			/* modify code for O8 */
			chr_err("Unplug Charger/USB in KPOC mode, vbus=%d, shutdown\n", vbus);
			kernel_power_off();
#endif
		}
	}
}
#endif
/*HS03s for P210730-04606 by wangzikang at 20210816 end*/

static char *dump_charger_type(int type)
{
	switch (type) {
	case POWER_SUPPLY_TYPE_UNKNOWN:
		return "none";
	case POWER_SUPPLY_TYPE_USB:
		return "usb";
	case POWER_SUPPLY_TYPE_USB_CDP:
		return "usb-h";
	case POWER_SUPPLY_TYPE_USB_DCP:
		return "std";
	case POWER_SUPPLY_TYPE_USB_FLOAT:
		return "nonstd";
	default:
		return "unknown";
	}
}

static int touchscreen_usb_plug_status = 0;
int mtk_touch_get_usb_status(void)
{
	return touchscreen_usb_plug_status;
}
EXPORT_SYMBOL(mtk_touch_get_usb_status);

int ss_charger_status = 0;
EXPORT_SYMBOL(ss_charger_status);
static int charger_routine_thread(void *arg)
{
	struct mtk_charger *info = arg;
	unsigned long flags;
	static bool is_module_init_done;
	bool is_charger_on;

	while (1) {
		wait_event(info->wait_que,
			(info->charger_thread_timeout == true));

		while (is_module_init_done == false) {
			if (charger_init_algo(info) == true)
				is_module_init_done = true;
			else {
				chr_err("charger_init fail\n");
				msleep(5000);
			}
		}

		mutex_lock(&info->charger_lock);
		spin_lock_irqsave(&info->slock, flags);
		if (!info->charger_wakelock->active)
			__pm_stay_awake(info->charger_wakelock);
		spin_unlock_irqrestore(&info->slock, flags);
		info->charger_thread_timeout = false;

		info->battery_temp = get_battery_temperature(info);
		chr_err("Vbat=%d vbus:%d ibus:%d I=%d T=%d uisoc:%d type:%s>%s pd:%d\n",
			get_battery_voltage(info),
			get_vbus(info),
			get_ibus(info),
			get_battery_current(info),
			info->battery_temp,
			get_uisoc(info),
			dump_charger_type(info->chr_type),
			dump_charger_type(get_charger_type(info)),
			info->pd_type);

		is_charger_on = mtk_is_charger_on(info);
		touchscreen_usb_plug_status = is_charger_on;

		if (info->charger_thread_polling == true)
			mtk_charger_start_timer(info);

		check_battery_exist(info);
		check_dynamic_mivr(info);
		info->ss_vchr = get_vbus(info) * 1000; /* uV */
		charger_check_status(info);
		/*HS03s for P210730-04606 by wangzikang at 20210816 start*/
		#ifndef HQ_FACTORY_BUILD	//ss version
		kpoc_power_off_check(info);
		#endif
		/*HS03s for P210730-04606 by wangzikang at 20210816 end*/
/* A03s code for SR-AL5625-01-35 by wenyaqi at 20210420 start */
		ss_charger_check_status(info);
/* A03s code for SR-AL5625-01-35 by wenyaqi at 20210420 end */
		/*HS03s for P210730-04606 by wangzikang at 20210816 end*/
		if (info->cmd_discharging == true)
			ss_charger_status = 1;
		else
			ss_charger_status = 0;
		/* HS03s code for SR-AL5625-01-35 by wenyaqi at 20210420 end */
/* TabA7 Lite code for P220922-06047 by duanweiping at 20220927 start */
		if (is_disable_charger(info) == false &&
			is_charger_on == true &&
			info->can_charging == true &&
			info->input_suspend == false) {
			if (info->algo.do_algorithm)
				info->algo.do_algorithm(info);
/* TabA7 Lite code for P220922-06047 by duanweiping at 20220927 end */
		} else
			chr_err("disable charging %d %d %d\n",
			is_disable_charger(info),
			is_charger_on,
			info->can_charging);

		spin_lock_irqsave(&info->slock, flags);
		__pm_relax(info->charger_wakelock);
		spin_unlock_irqrestore(&info->slock, flags);
		chr_debug("%s zhi end , %d\n",
			__func__, info->charger_thread_timeout);
		mutex_unlock(&info->charger_lock);
/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 start */
#ifdef CONFIG_BATT_TIME_TO_FULL
		gxy_ttf_work_start();
#endif
/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 end */
#ifdef CONFIG_HQ_PROJECT_HS03S
		msleep(5000);
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
	/*hs04_u for  AL6398AU-178 by shixuanxuan at 20240117 start*/
		gxy_ttf_work_start();
	/*hs04_u for  AL6398AU-178 by shixuanxuan at 20240117 end*/
		msleep(50);
#endif
	}

	return 0;
}


#ifdef CONFIG_PM
static int charger_pm_event(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	struct timespec now;
	struct mtk_charger *info;

	info = container_of(notifier,
		struct mtk_charger, pm_notifier);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		info->is_suspend = true;
		chr_debug("%s: enter PM_SUSPEND_PREPARE\n", __func__);
		break;
	case PM_POST_SUSPEND:
		info->is_suspend = false;
		chr_debug("%s: enter PM_POST_SUSPEND\n", __func__);
		get_monotonic_boottime(&now);

		if (timespec_compare(&now, &info->endtime) >= 0 &&
			info->endtime.tv_sec != 0 &&
			info->endtime.tv_nsec != 0) {
			chr_err("%s: alarm timeout, wake up charger\n",
				__func__);
			__pm_relax(info->charger_wakelock);
			info->endtime.tv_sec = 0;
			info->endtime.tv_nsec = 0;
			_wake_up_charger(info);
		}
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}
#endif /* CONFIG_PM */

static enum alarmtimer_restart
	mtk_charger_alarm_timer_func(struct alarm *alarm, ktime_t now)
{
	struct mtk_charger *info =
	container_of(alarm, struct mtk_charger, charger_timer);

	if (info->is_suspend == false) {
		chr_err("%s: not suspend, wake up charger\n", __func__);
		_wake_up_charger(info);
	} else {
		chr_err("%s: alarm timer timeout\n", __func__);
		__pm_stay_awake(info->charger_wakelock);
	}

	return ALARMTIMER_NORESTART;
}

static void mtk_charger_init_timer(struct mtk_charger *info)
{
	alarm_init(&info->charger_timer, ALARM_BOOTTIME,
			mtk_charger_alarm_timer_func);
	mtk_charger_start_timer(info);

#ifdef CONFIG_PM
	if (register_pm_notifier(&info->pm_notifier))
		chr_err("%s: register pm failed\n", __func__);
#endif /* CONFIG_PM */
}

static int mtk_charger_setup_files(struct platform_device *pdev)
{
	int ret = 0;
	struct proc_dir_entry *battery_dir = NULL, *entry = NULL;
	struct mtk_charger *info = platform_get_drvdata(pdev);

	ret = device_create_file(&(pdev->dev), &dev_attr_sw_jeita);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_chr_type);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_Pump_Express);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_ADC_Charger_Voltage);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_Charger_Config);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_input_current);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_charger_log_level);
	if (ret)
		goto _out;

	/* Battery warning */
	ret = device_create_file(&(pdev->dev), &dev_attr_BatteryNotify);
	if (ret)
		goto _out;

	battery_dir = proc_mkdir("mtk_battery_cmd", NULL);
	if (!battery_dir) {
		chr_err("%s: mkdir /proc/mtk_battery_cmd failed\n", __func__);
		return -ENOMEM;
	}

	entry = proc_create_data("current_cmd", 0644, battery_dir,
			&mtk_chg_current_cmd_fops, info);
	if (!entry) {
		ret = -ENODEV;
		goto fail_procfs;
	}
	entry = proc_create_data("en_power_path", 0644, battery_dir,
			&mtk_chg_en_power_path_fops, info);
	if (!entry) {
		ret = -ENODEV;
		goto fail_procfs;
	}
	entry = proc_create_data("en_safety_timer", 0644, battery_dir,
			&mtk_chg_en_safety_timer_fops, info);
	if (!entry) {
		ret = -ENODEV;
		goto fail_procfs;
	}

	return 0;

fail_procfs:
	remove_proc_subtree("mtk_battery_cmd", NULL);
_out:
	return ret;
}

void mtk_charger_get_atm_mode(struct mtk_charger *info)
{
	char atm_str[64] = {0};
	char *ptr = NULL, *ptr_e = NULL;
	char keyword[] = "androidboot.atm=";
	int size = 0;

	info->atm_enabled = false;

	ptr = strstr(saved_command_line, keyword);
	if (ptr != 0) {
		ptr_e = strstr(ptr, " ");
		if (ptr_e == 0)
			goto end;

		size = ptr_e - (ptr + strlen(keyword));
		if (size <= 0)
			goto end;
		strncpy(atm_str, ptr + strlen(keyword), size);
		atm_str[size] = '\0';

		if (!strncmp(atm_str, "enable", strlen("enable")))
			info->atm_enabled = true;
	}
end:
	chr_err("%s: atm_enabled = %d\n", __func__, info->atm_enabled);
}

#ifdef CONFIG_HQ_PROJECT_HS03S
static int psy_charger_property_is_writeable(struct power_supply *psy,
					       enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		return 1;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		return 1;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		return 1;
	/* HS03s code for SR-AL5625-01-35 by wenyaqi at 20210420 start */
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
	/* HS03s code for SR-AL5625-01-35 by wenyaqi at 20210420 end */
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
	#ifdef CONFIG_AFC_CHARGER
	case POWER_SUPPLY_PROP_AFC_RESULT:
	case POWER_SUPPLY_PROP_HV_DISABLE:
	#endif
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 start*/
	case POWER_SUPPLY_PROP_BATTERY_CYCLE:
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 end*/
	/*HS03s for SR-AL5625-01-276 by wenyaqi at 20210426 start*/
	case POWER_SUPPLY_PROP_BATT_SLATE_MODE:
	/*HS03s for SR-AL5625-01-276 by wenyaqi at 20210426 end*/
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 start */
	case POWER_SUPPLY_PROP_BATT_PROTECT_ENABLE:
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 end */
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
	case POWER_SUPPLY_PROP_STORE_MODE:
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 end*/
	#endif
	/*HS03s for SR-AL5625-01-272 by wenyaqi at 20210427 start*/
	#ifdef HQ_FACTORY_BUILD //factory version
	case POWER_SUPPLY_PROP_BATT_CAP_CONTROL:
	#endif
	/*HS03s for SR-AL5625-01-272 by wenyaqi at 20210427 end*/
	/* hs04 code for P221123-05653 by shixuanxuan at 20221201 start */
	#ifndef HQ_FACTORY_BUILD
	case POWER_SUPPLY_PROP_BATT_FULL_CAPACITY:
	#endif
	/* hs04 code for P221123-05653 by shixuanxuan at 20221201 end */
		return 1;
	default:
		return 0;
	}
}

static enum power_supply_property charger_psy_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	/* HS03s code for SR-AL5625-01-35 by wenyaqi at 20210420 start */
	POWER_SUPPLY_PROP_INPUT_SUSPEND,
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
	#ifdef CONFIG_AFC_CHARGER
	POWER_SUPPLY_PROP_HV_CHARGER_STATUS,
	POWER_SUPPLY_PROP_AFC_RESULT,
	POWER_SUPPLY_PROP_HV_DISABLE,
	#endif
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 start*/
	POWER_SUPPLY_PROP_BATTERY_CYCLE,
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 end*/
	/*HS03s for SR-AL5625-01-276 by wenyaqi at 20210426 start*/
	POWER_SUPPLY_PROP_BATT_SLATE_MODE,
	/*HS03s for SR-AL5625-01-276 by wenyaqi at 20210426 end*/
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 start */
	POWER_SUPPLY_PROP_BATT_PROTECT,
	POWER_SUPPLY_PROP_BATT_PROTECT_ENABLE,
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 end */
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
	POWER_SUPPLY_PROP_STORE_MODE,
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 end*/
	#endif
	/*HS03s for SR-AL5625-01-272 by wenyaqi at 20210427 start*/
	#ifdef HQ_FACTORY_BUILD //factory version
	POWER_SUPPLY_PROP_BATT_CAP_CONTROL,
	#endif
	/*HS03s for SR-AL5625-01-272 by wenyaqi at 20210427 end*/
};

static int psy_charger_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mtk_charger *info;
	struct charger_device *chg;

	info = (struct mtk_charger *)power_supply_get_drvdata(psy);

	chr_err("%s psp:%d\n",
		__func__, psp);


	if (info->psy1 != NULL &&
		info->psy1 == psy)
		chg = info->chg1_dev;
	else if (info->psy2 != NULL &&
		info->psy2 == psy)
		chg = info->chg2_dev;
	else {
		chr_err("%s fail\n", __func__);
		return 0;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = is_charger_exist(info);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		if (chg != NULL)
			val->intval = true;
		else
			val->intval = false;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = info->enable_hv_charging;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = get_vbus(info);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (chg == info->chg1_dev)
			val->intval =
				info->chg_data[CHG1_SETTING].junction_temp_max;
		else if (chg == info->chg2_dev)
			val->intval =
				info->chg_data[CHG2_SETTING].junction_temp_max;
		else
			val->intval = -127;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = get_charger_charging_current(info, chg);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = get_charger_input_current(info, chg);
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = info->chr_type;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_BOOT:
		val->intval = get_charger_zcv(info, chg);
		break;
	/*HS03s for SR-AL5625-01-276 by wenyaqi at 20210426 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	case POWER_SUPPLY_PROP_BATT_SLATE_MODE:
	#endif
	/*HS03s for SR-AL5625-01-276 by wenyaqi at 20210426 end*/
	/* HS03s code for SR-AL5625-01-35 by wenyaqi at 20210420 start */
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		val->intval = info->input_suspend;
		break;
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
	#ifdef CONFIG_AFC_CHARGER
	case POWER_SUPPLY_PROP_HV_CHARGER_STATUS:
		if (info->hv_disable)
			val->intval = 0;
		else {
			if(ss_fast_charger_status(info))
				val->intval = 1;
			else
				val->intval = 0;
		}
		break;
	case POWER_SUPPLY_PROP_AFC_RESULT:
		if (info->afc_sts >= AFC_5V)
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_HV_DISABLE:
		val->intval = info->hv_disable;
		break;
	#endif
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 start*/
	case POWER_SUPPLY_PROP_BATTERY_CYCLE:
		val->intval = info->data.ss_batt_cycle;
		break;
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 end*/
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 start */
	#if !defined(HQ_FACTORY_BUILD)
	case POWER_SUPPLY_PROP_BATT_PROTECT:
		val->intval = info->en_batt_protect;
		break;
	case POWER_SUPPLY_PROP_BATT_PROTECT_ENABLE:
		val->intval = info->batt_protect_flag;
		break;
	#endif
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 end */
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
	case POWER_SUPPLY_PROP_STORE_MODE:
		val->intval = info->batt_store_mode;
		break;
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 end*/
	#endif
	/*HS03s for SR-AL5625-01-272 by wenyaqi at 20210427 start*/
	#ifdef HQ_FACTORY_BUILD //factory version
	case POWER_SUPPLY_PROP_BATT_CAP_CONTROL:
		val->intval = info->batt_cap_control;
		break;
	#endif
	/*HS03s for SR-AL5625-01-272 by wenyaqi at 20210427 end*/
	default:
		return -EINVAL;
	}

	return 0;
}

int psy_charger_set_property(struct power_supply *psy,
			enum power_supply_property psp,
			const union power_supply_propval *val)
{
	struct mtk_charger *info;
	int idx;
/*hs04 code for P221123-05653 by shixuanxuan at 20221224 start*/
#ifndef HQ_FACTORY_BUILD
	struct power_supply *bat_psy;

	bat_psy = power_supply_get_by_name("battery");
	if (bat_psy == NULL) {
		chr_err("%s: get battery psy failed\n", __func__);
	}
#endif
/*hs04 code for P221123-05653 by shixuanxuan at 20221224 end*/

	chr_err("%s: prop:%d %d\n", __func__, psp, val->intval);

	info = (struct mtk_charger *)power_supply_get_drvdata(psy);

	if (info->psy1 != NULL &&
		info->psy1 == psy)
		idx = CHG1_SETTING;
	else if (info->psy2 != NULL &&
		info->psy2 == psy)
		idx = CHG2_SETTING;
	else {
		chr_err("%s fail\n", __func__);
		return 0;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (val->intval > 0)
			info->enable_hv_charging = true;
		else
			info->enable_hv_charging = false;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		info->chg_data[idx].thermal_charging_current_limit =
			val->intval;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		info->chg_data[idx].thermal_input_current_limit =
			val->intval;
		break;
	/*HS03s for SR-AL5625-01-276 by wenyaqi at 20210426 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	case POWER_SUPPLY_PROP_BATT_SLATE_MODE:
	#endif
	/*HS03s for SR-AL5625-01-276 by wenyaqi at 20210426 end*/
	/* HS03s code for SR-AL5625-01-35 by wenyaqi at 20210420 start */
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		if (val->intval)
			info->input_suspend = true;
		else
			info->input_suspend = false;
		break;
	/* HS03s code for SR-AL5625-01-35 by wenyaqi at 20210420 end */
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
	#ifdef CONFIG_AFC_CHARGER
	case POWER_SUPPLY_PROP_AFC_RESULT:
		is_afc_result(info, val->intval);
		break;
	case POWER_SUPPLY_PROP_HV_DISABLE:
		info->hv_disable = val->intval;
		break;
	#endif
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 start*/
	case POWER_SUPPLY_PROP_BATTERY_CYCLE:
		info->data.ss_batt_cycle = val->intval;
		break;
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 end*/
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 start */
	#if !defined(HQ_FACTORY_BUILD)
	case POWER_SUPPLY_PROP_BATT_PROTECT_ENABLE:
		info->batt_protect_flag = val->intval;
		break;
	#endif
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 end */
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
	case POWER_SUPPLY_PROP_STORE_MODE:
		ss_set_batt_store_mode(info, val);
		break;
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
	#endif
	/*HS03s for SR-AL5625-01-272 by wenyaqi at 20210427 start*/
	#ifdef HQ_FACTORY_BUILD //factory version
	case POWER_SUPPLY_PROP_BATT_CAP_CONTROL:
		if (val->intval)
			info->batt_cap_control = true;
		else
			info->batt_cap_control = false;
		break;
	#endif
	/*HS03s for SR-AL5625-01-272 by wenyaqi at 20210427 end*/
	/* hs04 code for P221123-05653 by shixuanxuan at 20221201 start */
	#ifndef HQ_FACTORY_BUILD
	case POWER_SUPPLY_PROP_BATT_FULL_CAPACITY:
		ss_charger_check_status(info);
		/*hs04 code for P221123-05653 by shixuanxuan at 20221224 start*/
		if (bat_psy) {
			pr_err("%s: battery psy changed\n",__func__);
			power_supply_changed(bat_psy);
		}
		/*hs04 code for P221123-05653 by shixuanxuan at 20221224 end*/
		break;
	#endif
	/* hs04 code for P221123-05653 by shixuanxuan at 20221201 end */
	default:
		return -EINVAL;
	}
	_wake_up_charger(info);

	return 0;
}
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
static int psy_charger_property_is_writeable(struct power_supply *psy,
					       enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		return 1;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		return 1;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		return 1;
	/* HS03s code for SR-AL5625-01-35 by wenyaqi at 20210420 start */
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
	/* HS03s code for SR-AL5625-01-35 by wenyaqi at 20210420 end */
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
	#ifdef CONFIG_AFC_CHARGER
	case POWER_SUPPLY_PROP_AFC_RESULT:
	case POWER_SUPPLY_PROP_HV_DISABLE:
	#endif
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 start*/
	case POWER_SUPPLY_PROP_BATTERY_CYCLE:
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 end*/
	/*HS03s for SR-AL5625-01-276 by wenyaqi at 20210426 start*/
	case POWER_SUPPLY_PROP_BATT_SLATE_MODE:
	/*HS03s for SR-AL5625-01-276 by wenyaqi at 20210426 end*/
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 start */
	case POWER_SUPPLY_PROP_BATT_PROTECT_ENABLE:
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 end */
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
	case POWER_SUPPLY_PROP_STORE_MODE:
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 end*/
	#endif
	/*HS03s for SR-AL5625-01-272 by wenyaqi at 20210427 start*/
	#ifdef HQ_FACTORY_BUILD //factory version
	case POWER_SUPPLY_PROP_BATT_CAP_CONTROL:
	#endif
	/*HS03s for SR-AL5625-01-272 by wenyaqi at 20210427 end*/
	/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 start */
	case POWER_SUPPLY_PROP_SHIPMODE:
	/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 end*/
	/* hs04 code for P221123-05653 by shixuanxuan at 20221201 start */
	#ifndef HQ_FACTORY_BUILD
	case POWER_SUPPLY_PROP_BATT_FULL_CAPACITY:
	#endif
	/* hs04 code for P221123-05653 by shixuanxuan at 20221201 end */
		return 1;
	default:
		return 0;
	}
}

static enum power_supply_property charger_psy_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	/* HS03s code for SR-AL5625-01-35 by wenyaqi at 20210420 start */
	POWER_SUPPLY_PROP_INPUT_SUSPEND,
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
	#ifdef CONFIG_AFC_CHARGER
	POWER_SUPPLY_PROP_HV_CHARGER_STATUS,
	POWER_SUPPLY_PROP_AFC_RESULT,
	POWER_SUPPLY_PROP_HV_DISABLE,
	#endif
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 start*/
	POWER_SUPPLY_PROP_BATTERY_CYCLE,
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 end*/
	/*HS03s for SR-AL5625-01-276 by wenyaqi at 20210426 start*/
	POWER_SUPPLY_PROP_BATT_SLATE_MODE,
	/*HS03s for SR-AL5625-01-276 by wenyaqi at 20210426 end*/
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 start */
	POWER_SUPPLY_PROP_BATT_PROTECT,
	POWER_SUPPLY_PROP_BATT_PROTECT_ENABLE,
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 end */
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
	POWER_SUPPLY_PROP_STORE_MODE,
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 end*/
	#endif
	/*HS03s for SR-AL5625-01-272 by wenyaqi at 20210427 start*/
	#ifdef HQ_FACTORY_BUILD //factory version
	POWER_SUPPLY_PROP_BATT_CAP_CONTROL,
	#endif
	/*HS03s for SR-AL5625-01-272 by wenyaqi at 20210427 end*/
	/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 start */
	POWER_SUPPLY_PROP_SHIPMODE,
	POWER_SUPPLY_PROP_SHIPMODE_REG,
	/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 end*/
};

static int psy_charger_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mtk_charger *info;
	struct charger_device *chg;

	info = (struct mtk_charger *)power_supply_get_drvdata(psy);

	chr_err("%s psp:%d\n",
		__func__, psp);


	if (info->psy1 != NULL &&
		info->psy1 == psy)
		chg = info->chg1_dev;
	else if (info->psy2 != NULL &&
		info->psy2 == psy)
		chg = info->chg2_dev;
	else {
		chr_err("%s fail\n", __func__);
		return 0;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = is_charger_exist(info);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		if (chg != NULL)
			val->intval = true;
		else
			val->intval = false;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = info->enable_hv_charging;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = get_vbus(info);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (chg == info->chg1_dev)
			val->intval =
				info->chg_data[CHG1_SETTING].junction_temp_max;
		else if (chg == info->chg2_dev)
			val->intval =
				info->chg_data[CHG2_SETTING].junction_temp_max;
		else
			val->intval = -127;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = get_charger_charging_current(info, chg);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = get_charger_input_current(info, chg);
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = info->chr_type;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_BOOT:
		val->intval = get_charger_zcv(info, chg);
		break;
	/*HS03s for SR-AL5625-01-276 by wenyaqi at 20210426 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	case POWER_SUPPLY_PROP_BATT_SLATE_MODE:
	/* HS04_U/HS14_U/TabA7 Lite U for P231128-06029 by liufurong at 20231204 start */
		val->intval =  info->batt_slate_mode;
		break;
	/* HS04_U/HS14_U/TabA7 Lite U for P231128-06029 by liufurong at 20231204 end */
	#endif
	/*HS03s for SR-AL5625-01-276 by wenyaqi at 20210426 end*/
	/* HS03s code for SR-AL5625-01-35 by wenyaqi at 20210420 start */
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		val->intval = info->input_suspend;
		break;
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
	#ifdef CONFIG_AFC_CHARGER
	case POWER_SUPPLY_PROP_HV_CHARGER_STATUS:
		if (info->hv_disable)
			val->intval = 0;
		else {
			if(ss_fast_charger_status(info))
				val->intval = 1;
			else
				val->intval = 0;
		}
		break;
	case POWER_SUPPLY_PROP_AFC_RESULT:
		if (info->afc_sts >= AFC_5V)
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_HV_DISABLE:
		val->intval = info->hv_disable;
		break;
	#endif
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 start*/
	case POWER_SUPPLY_PROP_BATTERY_CYCLE:
		val->intval = info->data.ss_batt_cycle;
		break;
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 end*/
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 start */
	#if !defined(HQ_FACTORY_BUILD)
	case POWER_SUPPLY_PROP_BATT_PROTECT:
		val->intval = info->en_batt_protect;
		break;
	case POWER_SUPPLY_PROP_BATT_PROTECT_ENABLE:
		val->intval = info->batt_protect_flag;
		break;
	#endif
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 end */
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
	case POWER_SUPPLY_PROP_STORE_MODE:
		val->intval = info->batt_store_mode;
		break;
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 end*/
	#endif
	/*HS03s for SR-AL5625-01-272 by wenyaqi at 20210427 start*/
	#ifdef HQ_FACTORY_BUILD //factory version
	case POWER_SUPPLY_PROP_BATT_CAP_CONTROL:
		val->intval = info->batt_cap_control;
		break;
	#endif
	/*HS03s for SR-AL5625-01-272 by wenyaqi at 20210427 end*/
	/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 start */
	case POWER_SUPPLY_PROP_SHIPMODE:
		val->intval = charger_dev_get_shipmode(info->chg1_dev);
		pr_err("get ship mode reg = %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_SHIPMODE_REG:
		val->intval = charger_dev_get_shipmode(info->chg1_dev);
		pr_err("get ship mode reg = %d\n", val->intval);
		break;
	/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 end*/
	default:
		return -EINVAL;
	}

	return 0;
}
/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 start */
static void set_shipmode(struct mtk_charger *info)
{
	int ret = 0;
	int i = 0;
	int num = 3;

	do {
		charger_dev_set_shipmode(info->chg1_dev, true);
		msleep(100);
		ret = charger_dev_get_shipmode(info->chg1_dev);
	} while (i++ < num && !ret);

	if (!ret) {
		pr_err("Set shipmode Fail!\n");
	}
}
/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 end */
int psy_charger_set_property(struct power_supply *psy,
			enum power_supply_property psp,
			const union power_supply_propval *val)
{
	struct mtk_charger *info;
	int idx;
/*hs04 code for P221123-05653 by shixuanxuan at 20221224 start*/
#ifndef HQ_FACTORY_BUILD
	struct power_supply *bat_psy;

	bat_psy = power_supply_get_by_name("battery");
	if (bat_psy == NULL) {
		chr_err("%s: get battery psy failed\n", __func__);
	}
#endif
/*hs04 code for P221123-05653 by shixuanxuan at 20221224 end*/

	chr_err("%s: prop:%d %d\n", __func__, psp, val->intval);

	info = (struct mtk_charger *)power_supply_get_drvdata(psy);

	if (info->psy1 != NULL &&
		info->psy1 == psy)
		idx = CHG1_SETTING;
	else if (info->psy2 != NULL &&
		info->psy2 == psy)
		idx = CHG2_SETTING;
	else {
		chr_err("%s fail\n", __func__);
		return 0;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (val->intval > 0)
			info->enable_hv_charging = true;
		else
			info->enable_hv_charging = false;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		info->chg_data[idx].thermal_charging_current_limit =
			val->intval;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		info->chg_data[idx].thermal_input_current_limit =
			val->intval;
		break;
	/*HS03s for SR-AL5625-01-276 by wenyaqi at 20210426 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	case POWER_SUPPLY_PROP_BATT_SLATE_MODE:
	/* HS04_U/HS14_U/TabA7 Lite U for P231128-06029 by liufurong at 20231204 start */
		if (val->intval == SEC_SLATE_OFF) {
			info->input_suspend = false;
		} else if (val->intval == SEC_SLATE_MODE) {
			info->input_suspend = true;
		} else if (val->intval == SEC_SMART_SWITCH_SLATE) {
			chr_err("%s:  dont suppot this slate mode %d\n", __func__, val->intval);
		} else if (val->intval == SEC_SMART_SWITCH_SRC) {
			chr_err("%s:  dont suppot this slate mode %d\n", __func__, val->intval);
		}
		info->batt_slate_mode = val->intval;
		chr_err("%s:  set slate mode %d\n", __func__, val->intval);
		break;
	/* HS04_U/HS14_U/TabA7 Lite U for P231128-06029 by liufurong at 20231204 end */
	#endif
	/*HS03s for SR-AL5625-01-276 by wenyaqi at 20210426 end*/
	/* HS03s code for SR-AL5625-01-35 by wenyaqi at 20210420 start */
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		if (val->intval)
			info->input_suspend = true;
		else
			info->input_suspend = false;
		break;
	/* HS03s code for SR-AL5625-01-35 by wenyaqi at 20210420 end */
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
	#ifdef CONFIG_AFC_CHARGER
	case POWER_SUPPLY_PROP_AFC_RESULT:
		is_afc_result(info, val->intval);
		break;
	case POWER_SUPPLY_PROP_HV_DISABLE:
		info->hv_disable = val->intval;
		break;
	#endif
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 start*/
	case POWER_SUPPLY_PROP_BATTERY_CYCLE:
		info->data.ss_batt_cycle = val->intval;
		break;
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 end*/
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 start */
	#if !defined(HQ_FACTORY_BUILD)
	case POWER_SUPPLY_PROP_BATT_PROTECT_ENABLE:
		info->batt_protect_flag = val->intval;
		break;
	#endif
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 end */
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
	case POWER_SUPPLY_PROP_STORE_MODE:
		ss_set_batt_store_mode(info, val);
		break;
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
	#endif
	/*HS03s for SR-AL5625-01-272 by wenyaqi at 20210427 start*/
	#ifdef HQ_FACTORY_BUILD //factory version
	case POWER_SUPPLY_PROP_BATT_CAP_CONTROL:
		if (val->intval)
			info->batt_cap_control = true;
		else
			info->batt_cap_control = false;
		break;
	#endif
	/*HS03s for SR-AL5625-01-272 by wenyaqi at 20210427 end*/
	/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 start */
	case POWER_SUPPLY_PROP_SHIPMODE:
		if (val->intval) {
			set_shipmode(info);
		}
		pr_err("%s: enable shipmode val = %d\n",__func__, val->intval);
		break;
	/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 end*/
	/* hs04 code for P221123-05653 by shixuanxuan at 20221201 start */
	#ifndef HQ_FACTORY_BUILD
	case POWER_SUPPLY_PROP_BATT_FULL_CAPACITY:
		ss_charger_check_status(info);
		/*hs04 code for P221123-05653 by shixuanxuan at 20221224 start*/
		if (bat_psy) {
			pr_err("%s: battery psy changed\n",__func__);
			power_supply_changed(bat_psy);
		}
		/*hs04 code for P221123-05653 by shixuanxuan at 20221224 end*/
		break;
	#endif
	/* hs04 code for P221123-05653 by shixuanxuan at 20221201 end */
	default:
		return -EINVAL;
	}
	_wake_up_charger(info);

	return 0;
}
#endif
#ifdef CONFIG_HQ_PROJECT_OT8
//for 08
/* HS14_U/TabA7 Lite U for AL6528AU-249/AX3565AU-309 by liufurong at 20231212 start */
extern void pd_dpm_send_source_caps_switch(int cur);
/* HS14_U/TabA7 Lite U for AL6528AU-249/AX3565AU-309 by liufurong at 20231212 end */
static int psy_charger_property_is_writeable(struct power_supply *psy,
					       enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		return 1;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		return 1;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		return 1;
	/*TabA7 Lite code for  SR-AX3565-01-13 add sysFS node named battery/input_suspend by wenyaqi at 20201130 start*/
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		return 1;
	/*TabA7 Lite code for  SR-AX3565-01-13 add sysFS node named battery/input_suspend by wenyaqi at 20201130 end*/
	/*TabA7 Lite  code for SR-AX3565-01-108 by gaoxugang at 20201124 start*/
	#if !defined(HQ_FACTORY_BUILD)
	case POWER_SUPPLY_PROP_BATT_SLATE_MODE:
		return 1;
	#endif
	/*TabA7 Lite  code for SR-AX3565-01-108 by gaoxugang at 20201124 end*/
	/*TabA7 Lite code for OT8-106 add afc charger driver by wenyaqi at 20201210 start*/
	#ifdef CONFIG_AFC_CHARGER
	case POWER_SUPPLY_PROP_AFC_RESULT:
		return 1;
	case POWER_SUPPLY_PROP_HV_DISABLE:
		return 1;
	#endif
	/*TabA7 Lite code for OT8-106 add afc charger driver by wenyaqi at 20201210 end*/
	/*TabA7 Lite code for SR-AX3565-01-124 Import battery aging by wenyaqi at 20201221 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	case POWER_SUPPLY_PROP_BATTERY_CYCLE:
		return 1;
	#endif
	/*TabA7 Lite code for SR-AX3565-01-124 Import battery aging by wenyaqi at 20201221 end*/
	/*TabA7 Lite code for discharging over 80 by wenyaqi at 20210101 start*/
	#ifdef HQ_FACTORY_BUILD //factory version
	case POWER_SUPPLY_PROP_BATT_CAP_CONTROL:
		return 1;
	#endif
	/*TabA7 Lite code for discharging over 80 by wenyaqi at 20210101 end*/
	/* AX3565 for SR-AX3565-01-737 add battery protect function by shixuanxuan at 2021/01/13 start */
	#if !defined(HQ_FACTORY_BUILD)
	case POWER_SUPPLY_PROP_BATT_PROTECT_ENABLE:
		return 1;
	#endif
	/* AX3565 for SR-AX3565-01-737 add battery protect function by shixuanxuan at 2021/01/13 end */
	/*TabA7 Lite code for P210330-05709 by wenyaqi at 20210401 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	case POWER_SUPPLY_PROP_STORE_MODE:
		return 1;
	/*TabA7 Lite code for P210330-05709 by wenyaqi at 20210401 end*/
	/* hs04 code for P221123-05653 by shixuanxuan at 20221201 start */
	#ifndef HQ_FACTORY_BUILD
	case POWER_SUPPLY_PROP_BATT_FULL_CAPACITY:
		return 1;
	#endif
	/* hs04 code for P221123-05653 by shixuanxuan at 20221201 end */
	#endif
	default:
		return 0;
	}
}

static enum power_supply_property charger_psy_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	/*TabA7 Lite code for  SR-AX3565-01-13 add sysFS node named battery/input_suspend by wenyaqi at 20201130 start*/
	POWER_SUPPLY_PROP_INPUT_SUSPEND,
	/*TabA7 Lite code for  SR-AX3565-01-13 add sysFS node named battery/input_suspend by wenyaqi at 20201130 end*/
	/*TabA7 Lite  code for SR-AX3565-01-108 by gaoxugang at 20201124 start*/
	#if !defined(HQ_FACTORY_BUILD)
	POWER_SUPPLY_PROP_BATT_SLATE_MODE,
	#endif
	/*TabA7 Lite  code for SR-AX3565-01-108 by gaoxugang at 20201124 end*/
	/*TabA7 Lite code for OT8-106 add afc charger driver by wenyaqi at 20201210 start*/
	#ifdef CONFIG_AFC_CHARGER
	POWER_SUPPLY_PROP_HV_CHARGER_STATUS,
	POWER_SUPPLY_PROP_AFC_RESULT,
	POWER_SUPPLY_PROP_HV_DISABLE,
	#endif
	/*TabA7 Lite code for OT8-106 add afc charger driver by wenyaqi at 20201210 end*/
	/*TabA7 Lite code for SR-AX3565-01-124 Import battery aging by wenyaqi at 20201221 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	POWER_SUPPLY_PROP_BATTERY_CYCLE,
	#endif
	/*TabA7 Lite code for SR-AX3565-01-124 Import battery aging by wenyaqi at 20201221 end*/
	/*TabA7 Lite code for discharging over 80 by wenyaqi at 20210101 start*/
	#ifdef HQ_FACTORY_BUILD //factory version
	POWER_SUPPLY_PROP_BATT_CAP_CONTROL,
	#endif
	/*TabA7 Lite code for discharging over 80 by wenyaqi at 20210101 end*/
	/* AX3565 for SR-AX3565-01-737 add battery protect function by shixuanxuan at 2021/01/13 start */
	#if !defined(HQ_FACTORY_BUILD)
	POWER_SUPPLY_PROP_BATT_PROTECT,
	POWER_SUPPLY_PROP_BATT_PROTECT_ENABLE,
	#endif
	/* AX3565 for SR-AX3565-01-737 add battery protect function by shixuanxuan at 2021/01/13 end */
	/*TabA7 Lite code for P210330-05709 by wenyaqi at 20210401 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	POWER_SUPPLY_PROP_STORE_MODE,
	#endif
	/*TabA7 Lite code for P210330-05709 by wenyaqi at 20210401 end*/
};

static int psy_charger_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mtk_charger *info;
	struct charger_device *chg;

	info = (struct mtk_charger *)power_supply_get_drvdata(psy);

	chr_err("%s psp:%d\n",
		__func__, psp);


	if (info->psy1 != NULL &&
		info->psy1 == psy)
		chg = info->chg1_dev;
	else if (info->psy2 != NULL &&
		info->psy2 == psy)
		chg = info->chg2_dev;
	else {
		chr_err("%s fail\n", __func__);
		return 0;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = is_charger_exist(info);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		if (chg != NULL)
			val->intval = true;
		else
			val->intval = false;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = info->enable_hv_charging;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = get_vbus(info);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (chg == info->chg1_dev)
			val->intval =
				info->chg_data[CHG1_SETTING].junction_temp_max;
		else if (chg == info->chg2_dev)
			val->intval =
				info->chg_data[CHG2_SETTING].junction_temp_max;
		else
			val->intval = -127;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = get_charger_charging_current(info, chg);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = get_charger_input_current(info, chg);
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = info->chr_type;
		pr_err("%s:ot8 chr_type = %d\n", __func__, info->chr_type);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_BOOT:
		val->intval = get_charger_zcv(info, chg);
		break;
	/*TabA7 Lite code for OT8-106 add afc charger driver by wenyaqi at 20201210 start*/
	/* AX3565 for SR-AX3565-01-737 add battery protect function by shixuanxuan at 2021/01/13 start */
	#if !defined(HQ_FACTORY_BUILD)
	case POWER_SUPPLY_PROP_BATT_PROTECT:
		val->intval = info->en_batt_protect;
		break;
	case POWER_SUPPLY_PROP_BATT_PROTECT_ENABLE:
		val->intval = info->batt_protect_flag;
		break;
	#endif
	/* AX3565 for SR-AX3565-01-737 add battery protect function by shixuanxuan at 2021/01/13 end */
	#ifdef CONFIG_AFC_CHARGER
	case POWER_SUPPLY_PROP_HV_CHARGER_STATUS:
		if (info->hv_disable)
			val->intval = 0;
		else {
			/*TabA7 Lite code for P210209-04223 fix HV_CHARGER_STATUS by wenyaqi at 20210212 start*/
			if(ss_fast_charger_status(info))
				val->intval = 1;
			/*TabA7 Lite code for P210209-04223 fix HV_CHARGER_STATUS by wenyaqi at 20210212 end*/
			else
				val->intval = 0;
		}
		break;
	case POWER_SUPPLY_PROP_AFC_RESULT:
		if (info->afc_sts >= AFC_5V)
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_HV_DISABLE:
		val->intval = info->hv_disable;
		break;
	#endif
	/*TabA7 Lite code for OT8-106 add afc charger driver by wenyaqi at 20201210 end*/
	/*TabA7 Lite code for SR-AX3565-01-124 Import battery aging by wenyaqi at 20201221 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	case POWER_SUPPLY_PROP_BATTERY_CYCLE:
		val->intval = info->data.ss_batt_cycle;
		break;
	#endif
	/*TabA7 Lite code for SR-AX3565-01-124 Import battery aging by wenyaqi at 20201221 end*/
	/*TabA7 Lite code for discharging over 80 by wenyaqi at 20210101 start*/
	#ifdef HQ_FACTORY_BUILD //factory version
	case POWER_SUPPLY_PROP_BATT_CAP_CONTROL:
		val->intval = info->batt_cap_control;
		break;
	#endif
	/*TabA7 Lite code for discharging over 80 by wenyaqi at 20210101 end*/
	/*TabA7 Lite code for P210330-05709 by wenyaqi at 20210401 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	case POWER_SUPPLY_PROP_STORE_MODE:
		val->intval = info->batt_store_mode;
		break;
	#endif
	/*TabA7 Lite code for P210330-05709 by wenyaqi at 20210401 end*/
	default:
		return -EINVAL;
	}

	return 0;
}

int psy_charger_set_property(struct power_supply *psy,
			enum power_supply_property psp,
			const union power_supply_propval *val)
{
	struct mtk_charger *info;
	int idx;
/*hs04 code for P221123-05653 by shixuanxuan at 20221224 start*/
#ifndef HQ_FACTORY_BUILD
	struct power_supply *bat_psy;

	bat_psy = power_supply_get_by_name("battery");
	if (bat_psy == NULL) {
		chr_err("%s: get battery psy failed\n", __func__);
	}
#endif
/*hs04 code for P221123-05653 by shixuanxuan at 20221224 end*/

	chr_err("%s: prop:%d %d\n", __func__, psp, val->intval);

	info = (struct mtk_charger *)power_supply_get_drvdata(psy);

	if (info->psy1 != NULL &&
		info->psy1 == psy)
		idx = CHG1_SETTING;
	else if (info->psy2 != NULL &&
		info->psy2 == psy)
		idx = CHG2_SETTING;
	else {
		chr_err("%s fail\n", __func__);
		return 0;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (val->intval > 0)
			info->enable_hv_charging = true;
		else
			info->enable_hv_charging = false;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		info->chg_data[idx].thermal_charging_current_limit =
			val->intval;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		info->chg_data[idx].thermal_input_current_limit =
			val->intval;
		break;
	/*TabA7 Lite  code for SR-AX3565-01-108 by gaoxugang at 20201124 start*/
	#if !defined(HQ_FACTORY_BUILD)
	case POWER_SUPPLY_PROP_BATT_SLATE_MODE:
	/* HS04_U/HS14_U/TabA7 Lite U for P231128-06029 by liufurong at 20231204 start */
		/* HS14_U/TabA7 Lite U for AL6528AU-249/AX3565AU-309 by liufurong at 20231212 start */
		if (val->intval == SEC_SLATE_OFF) {
			info->input_suspend = false;
			charger_dev_enable(info->chg1_dev, true);
			charger_dev_do_event(info->chg1_dev,EVENT_RECHARGE, 0);
			charger_dev_enable_powerpath(info->chg1_dev, true);
			printk("ltk-1\n");
			if (info->batt_slate_mode == SEC_SMART_SWITCH_SRC || info->batt_slate_mode == SEC_SMART_SWITCH_SLATE) {
				pd_dpm_send_source_caps_switch(1000);
			}
		} else if (val->intval == SEC_SLATE_MODE) {
			info->input_suspend = true;
			charger_dev_enable(info->chg1_dev, false);
			charger_dev_do_event(info->chg1_dev,EVENT_DISCHARGE, 0);
			charger_dev_enable_powerpath(info->chg1_dev, false);
			printk("ltk-0\n");
		} else if (val->intval == SEC_SMART_SWITCH_SLATE) {
			pd_dpm_send_source_caps_switch(500);
			chr_err("%s:  dont suppot this slate mode %d\n", __func__, val->intval);
		} else if (val->intval == SEC_SMART_SWITCH_SRC) {
			pd_dpm_send_source_caps_switch(0);
			chr_err("%s:  dont suppot this slate mode %d\n", __func__, val->intval);
		}
		/* HS14_U/TabA7 Lite U for AL6528AU-249/AX3565AU-309 by liufurong at 20231212 end */
		info->batt_slate_mode = val->intval;
		chr_err("%s:  set slate mode %d\n", __func__, val->intval);
		break;
	/* HS04_U/HS14_U/TabA7 Lite U for P231128-06029 by liufurong at 20231204 end */
	#endif
	/*TabA7 Lite  code for SR-AX3565-01-108 by gaoxugang at 20201124 end*/
	/*TabA7 Lite code for OT8-384 fix confliction between input_suspend and sw_ovp by wenyaqi at 20201224 start*/
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		if (val->intval){
			info->input_suspend = true;
                charger_dev_enable(info->chg1_dev, false);
                charger_dev_do_event(info->chg1_dev,EVENT_DISCHARGE, 0);
                charger_dev_enable_powerpath(info->chg1_dev, false);
                printk("ltk-0\n");
                }
		else{
			info->input_suspend = false;
                charger_dev_enable(info->chg1_dev, true);
                charger_dev_do_event(info->chg1_dev,EVENT_RECHARGE, 0);
                charger_dev_enable_powerpath(info->chg1_dev, true);
                printk("ltk-1\n");
                }
		break;
	/*TabA7 Lite code for OT8-384 fix confliction between input_suspend and sw_ovp by wenyaqi at 20201224 start*/
	/* AX3565 for SR-AX3565-01-737 add battery protect function by shixuanxuan at 2021/01/13 start */
	#if !defined(HQ_FACTORY_BUILD)
	case POWER_SUPPLY_PROP_BATT_PROTECT_ENABLE:
		info->batt_protect_flag = val->intval;
		break;
	#endif
	/* AX3565 for SR-AX3565-01-737 add battery protect function by shixuanxuan at 2021/01/13 end */
	/*TabA7 Lite code for OT8-106 add afc charger driver by wenyaqi at 20201210 start*/
	#ifdef CONFIG_AFC_CHARGER
	case POWER_SUPPLY_PROP_AFC_RESULT:
		is_afc_result(info, val->intval);
		break;
	case POWER_SUPPLY_PROP_HV_DISABLE:
		info->hv_disable = val->intval;
		break;
	#endif
	/*TabA7 Lite code for OT8-106 add afc charger driver by wenyaqi at 20201210 end*/
	/*TabA7 Lite code for SR-AX3565-01-124 Import battery aging by wenyaqi at 20201221 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	case POWER_SUPPLY_PROP_BATTERY_CYCLE:
		info->data.ss_batt_cycle = val->intval;
		break;
	#endif
	/*TabA7 Lite code for SR-AX3565-01-124 Import battery aging by wenyaqi at 20201221 end*/
	/*TabA7 Lite code for discharging over 80 by wenyaqi at 20210101 start*/
	#ifdef HQ_FACTORY_BUILD //factory version
	case POWER_SUPPLY_PROP_BATT_CAP_CONTROL:
		if (val->intval)
			info->batt_cap_control = true;
		else
			info->batt_cap_control = false;
		break;
	#endif
	/*TabA7 Lite code for discharging over 80 by wenyaqi at 20210101 end*/
	/*TabA7 Lite code for SR-AX3565-01-109 by wenyaqi at 20210416 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	case POWER_SUPPLY_PROP_STORE_MODE:
		ss_set_batt_store_mode(info, val);
		break;
	#endif
	/*TabA7 Lite code for SR-AX3565-01-109 by wenyaqi at 20210416 end*/
	/* hs04 code for P221123-05653 by shixuanxuan at 20221201 start */
	#ifndef HQ_FACTORY_BUILD
	case POWER_SUPPLY_PROP_BATT_FULL_CAPACITY:
		ss_charger_check_status(info);
		/*hs04 code for P221123-05653 by shixuanxuan at 20221224 start*/
		if (bat_psy) {
			pr_err("%s: battery psy changed\n",__func__);
			power_supply_changed(bat_psy);
		}
		break;
	#endif
	/* hs04 code for P221123-05653 by shixuanxuan at 20221201 end */
	default:
		return -EINVAL;
	}
	_wake_up_charger(info);

	return 0;
}

#endif//for 08
static void mtk_charger_external_power_changed(struct power_supply *psy)
{
	struct mtk_charger *info;
	union power_supply_propval prop, prop2;
	struct power_supply *chg_psy = NULL;
	int ret;

	info = (struct mtk_charger *)power_supply_get_drvdata(psy);
	chg_psy = info->chg_psy;

	if (IS_ERR_OR_NULL(chg_psy)) {
		pr_notice("%s Couldn't get chg_psy\n", __func__);
		chg_psy = devm_power_supply_get_by_phandle(&info->pdev->dev,
			"charger");
		info->chg_psy = chg_psy;
	} else {
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_ONLINE, &prop);
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_USB_TYPE, &prop2);
	}

	pr_err("%s event, name:%s online:%d type:%d vbus:%d\n", __func__,
		psy->desc->name, prop.intval, prop2.intval,
		get_vbus(info));

	mtk_is_charger_on(info);
	_wake_up_charger(info);
}

int notify_adapter_event(struct notifier_block *notifier,
			unsigned long evt, void *val)
{
	struct mtk_charger *pinfo = NULL;
	struct power_supply *psy;

	psy = power_supply_get_by_name("battery");
	if (!psy) {
		chr_err("get battery psy failed\n");
	}

	chr_err("%s %d\n", __func__, evt);

	pinfo = container_of(notifier,
		struct mtk_charger, pd_nb);

	switch (evt) {
	case  MTK_PD_CONNECT_NONE:
		mutex_lock(&pinfo->pd_lock);
		chr_err("PD Notify Detach\n");
		pinfo->pd_type = MTK_PD_CONNECT_NONE;
		mutex_unlock(&pinfo->pd_lock);
		/* reset PE40 */
		break;

	case MTK_PD_CONNECT_HARD_RESET:
		mutex_lock(&pinfo->pd_lock);
		chr_err("PD Notify HardReset\n");
		pinfo->pd_type = MTK_PD_CONNECT_NONE;
		pinfo->pd_reset = true;
		mutex_unlock(&pinfo->pd_lock);
		_wake_up_charger(pinfo);
		/* reset PE40 */
		break;

	case MTK_PD_CONNECT_PE_READY_SNK:
		mutex_lock(&pinfo->pd_lock);
		chr_err("PD Notify fixe voltage ready\n");
		pinfo->pd_type = MTK_PD_CONNECT_PE_READY_SNK;
		mutex_unlock(&pinfo->pd_lock);
		/* PD is ready */
		break;

	case MTK_PD_CONNECT_PE_READY_SNK_PD30:
		mutex_lock(&pinfo->pd_lock);
		chr_err("PD Notify PD30 ready\r\n");
		pinfo->pd_type = MTK_PD_CONNECT_PE_READY_SNK_PD30;
		mutex_unlock(&pinfo->pd_lock);
		/* PD30 is ready */
		break;

	case MTK_PD_CONNECT_PE_READY_SNK_APDO:
		mutex_lock(&pinfo->pd_lock);
		chr_err("PD Notify APDO Ready\n");
		pinfo->pd_type = MTK_PD_CONNECT_PE_READY_SNK_APDO;
		mutex_unlock(&pinfo->pd_lock);
		/* PE40 is ready */
		_wake_up_charger(pinfo);
		break;

	case MTK_PD_CONNECT_TYPEC_ONLY_SNK:
		mutex_lock(&pinfo->pd_lock);
		chr_err("PD Notify Type-C Ready\n");
		pinfo->pd_type = MTK_PD_CONNECT_TYPEC_ONLY_SNK;
		mutex_unlock(&pinfo->pd_lock);
		/* type C is ready */
		_wake_up_charger(pinfo);
		break;
	case MTK_TYPEC_WD_STATUS:
		chr_err("wd status = %d\n", *(bool *)val);
		pinfo->water_detected = *(bool *)val;
		if (pinfo->water_detected == true)
			pinfo->notify_code |= CHG_TYPEC_WD_STATUS;
		else
			pinfo->notify_code &= ~CHG_TYPEC_WD_STATUS;
		mtk_chgstat_notify(pinfo);
		break;
	}

	if (psy) {
		power_supply_changed(psy);
	}

	return NOTIFY_DONE;
}

int chg_alg_event(struct notifier_block *notifier,
			unsigned long event, void *data)
{
	chr_err("%s: evt:%d\n", __func__, event);

	return NOTIFY_DONE;
}


#ifdef CONFIG_HQ_PROJECT_HS03S
//for 06
static int mtk_charger_probe(struct platform_device *pdev)
{
	struct mtk_charger *info = NULL;
	int i;
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
	#ifdef CONFIG_AFC_CHARGER
	int ret;
	#endif
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/
	char *name = NULL;

	chr_err("%s: starts\n", __func__);

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	platform_set_drvdata(pdev, info);
	info->pdev = pdev;

	mtk_charger_parse_dt(info, &pdev->dev);

	mutex_init(&info->cable_out_lock);
	mutex_init(&info->charger_lock);
	mutex_init(&info->pd_lock);
	name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "%s",
		"charger suspend wakelock");
	info->charger_wakelock =
		wakeup_source_register(NULL, name);
	spin_lock_init(&info->slock);

	init_waitqueue_head(&info->wait_que);
	info->polling_interval = CHARGING_INTERVAL;
	mtk_charger_init_timer(info);
#ifdef CONFIG_PM
	info->pm_notifier.notifier_call = charger_pm_event;
#endif /* CONFIG_PM */
	srcu_init_notifier_head(&info->evt_nh);
	mtk_charger_setup_files(pdev);
	mtk_charger_get_atm_mode(info);

	for (i = 0; i < CHGS_SETTING_MAX; i++) {
		info->chg_data[i].thermal_charging_current_limit = -1;
		info->chg_data[i].thermal_input_current_limit = -1;
		info->chg_data[i].input_current_limit_by_aicl = -1;
	}
	info->enable_hv_charging = true;
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	info->batt_store_mode = false;
	info->charger_wakelock_app =
		wakeup_source_register(NULL, "wakelockapp");
	INIT_DELAYED_WORK(&info->retail_app_status_change_work, ss_retail_app_status_change_work);
	INIT_DELAYED_WORK(&info->poweroff_dwork, offmode_delay_work);
	#endif
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 end*/
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 start */
	#if !defined(HQ_FACTORY_BUILD)
	INIT_DELAYED_WORK(&info->charging_count_work, hq_charging_count_work);
	//schedule_delayed_work(&info->charging_count_work, msecs_to_jiffies(CHR_COUNT_TIME));
	#endif
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 end */
	/* HS03s code for HQ00001 Modify battery protect function by shixuanxuan at 2021/05/10 start */
    #if !defined(HQ_FACTORY_BUILD)
	info->current_time = 0;
  	#endif
	/* HS03s code for HQ00001 Modify battery protect function by shixuanxuan at 2021/05/10 end */
	info->psy_desc1.name = "mtk-master-charger";
	info->psy_desc1.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_desc1.properties = charger_psy_properties;
	info->psy_desc1.num_properties = ARRAY_SIZE(charger_psy_properties);
	info->psy_desc1.get_property = psy_charger_get_property;
	info->psy_desc1.set_property = psy_charger_set_property;
	info->psy_desc1.property_is_writeable =
			psy_charger_property_is_writeable;
	info->psy_desc1.external_power_changed =
		mtk_charger_external_power_changed;
	info->psy_cfg1.drv_data = info;
	info->psy1 = power_supply_register(&pdev->dev, &info->psy_desc1,
			&info->psy_cfg1);

	info->chg_psy = devm_power_supply_get_by_phandle(&pdev->dev,
		"charger");
	if (IS_ERR_OR_NULL(info->chg_psy))
		chr_err("%s: devm power fail to get chg_psy\n", __func__);

	info->bat_psy = devm_power_supply_get_by_phandle(&pdev->dev,
		"gauge");
	if (IS_ERR_OR_NULL(info->bat_psy))
		chr_err("%s: devm power fail to get bat_psy\n", __func__);

	if (IS_ERR(info->psy1))
		chr_err("register psy1 fail:%d\n",
			PTR_ERR(info->psy1));

	info->psy_desc2.name = "mtk-slave-charger";
	info->psy_desc2.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_desc2.properties = charger_psy_properties;
	info->psy_desc2.num_properties = ARRAY_SIZE(charger_psy_properties);
	info->psy_desc2.get_property = psy_charger_get_property;
	info->psy_desc2.set_property = psy_charger_set_property;
	info->psy_desc2.property_is_writeable =
			psy_charger_property_is_writeable;
	info->psy_cfg2.drv_data = info;
	info->psy2 = power_supply_register(&pdev->dev, &info->psy_desc2,
			&info->psy_cfg2);
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 start */
	#if !defined(HQ_FACTORY_BUILD)
	info->batt_protect_flag = ENABLE_BATT_PROTECT;
	info->en_batt_protect = DISABLE_BATT_PROTECT;
	info->over_cap_count = 0;
	info->cap_hold_count = 0;
	info->recharge_count = 0;
	info->interval_time = 0;
	#endif
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 end */
	if (IS_ERR(info->psy2))
		chr_err("register psy2 fail:%d\n",
			PTR_ERR(info->psy2));

	info->log_level = CHRLOG_DEBUG_LEVEL;

	info->pd_adapter = get_adapter_by_name("pd_adapter");
	if (!info->pd_adapter)
		chr_err("%s: No pd adapter found\n");
	else {
		info->pd_nb.notifier_call = notify_adapter_event;
		register_adapter_device_notifier(info->pd_adapter,
						 &info->pd_nb);
	}
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
	#ifdef CONFIG_AFC_CHARGER
	ret = afc_charge_init(info);
	if (ret < 0)
	{
		info->enable_afc = false;
		info->hv_disable = HV_DISABLE;
	} else {
		info->enable_afc = true;
		info->hv_disable = HV_ENABLE;
	}
	#endif
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/

	info->chg_alg_nb.notifier_call = chg_alg_event;
	/*HS03s for DEVAL5626-469 by liujie at 20210819 start*/
    init_jeita_state_machine(info);
	/*HS03s for DEVAL5626-469 by liujie at 20210819 end*/
	kthread_run(charger_routine_thread, info, "charger_thread");

	return 0;
}
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
//for 06
static int mtk_charger_probe(struct platform_device *pdev)
{
	struct mtk_charger *info = NULL;
	int i;
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
	#ifdef CONFIG_AFC_CHARGER
	int ret;
	#endif
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/
	char *name = NULL;

	chr_err("%s: starts\n", __func__);

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	platform_set_drvdata(pdev, info);
	info->pdev = pdev;

	mtk_charger_parse_dt(info, &pdev->dev);

	mutex_init(&info->cable_out_lock);
	mutex_init(&info->charger_lock);
	mutex_init(&info->pd_lock);
	name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "%s",
		"charger suspend wakelock");
	info->charger_wakelock =
		wakeup_source_register(NULL, name);
	spin_lock_init(&info->slock);

	init_waitqueue_head(&info->wait_que);
	info->polling_interval = CHARGING_INTERVAL;
	mtk_charger_init_timer(info);
#ifdef CONFIG_PM
	info->pm_notifier.notifier_call = charger_pm_event;
#endif /* CONFIG_PM */
	srcu_init_notifier_head(&info->evt_nh);
	mtk_charger_setup_files(pdev);
	mtk_charger_get_atm_mode(info);

	for (i = 0; i < CHGS_SETTING_MAX; i++) {
		info->chg_data[i].thermal_charging_current_limit = -1;
		info->chg_data[i].thermal_input_current_limit = -1;
		info->chg_data[i].input_current_limit_by_aicl = -1;
	}
	info->enable_hv_charging = true;
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	info->batt_store_mode = false;
	info->charger_wakelock_app =
		wakeup_source_register(NULL, "wakelockapp");
	INIT_DELAYED_WORK(&info->retail_app_status_change_work, ss_retail_app_status_change_work);
	INIT_DELAYED_WORK(&info->poweroff_dwork, offmode_delay_work);
	#endif
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 end*/
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 start */
	#if !defined(HQ_FACTORY_BUILD)
	INIT_DELAYED_WORK(&info->charging_count_work, hq_charging_count_work);
	//schedule_delayed_work(&info->charging_count_work, msecs_to_jiffies(CHR_COUNT_TIME));
	#endif
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 end */
	/* HS03s code for HQ00001 Modify battery protect function by shixuanxuan at 2021/05/10 start */
    #if !defined(HQ_FACTORY_BUILD)
	info->current_time = 0;
  	#endif
	/* HS03s code for HQ00001 Modify battery protect function by shixuanxuan at 2021/05/10 end */
	info->psy_desc1.name = "mtk-master-charger";
	info->psy_desc1.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_desc1.properties = charger_psy_properties;
	info->psy_desc1.num_properties = ARRAY_SIZE(charger_psy_properties);
	info->psy_desc1.get_property = psy_charger_get_property;
	info->psy_desc1.set_property = psy_charger_set_property;
	info->psy_desc1.property_is_writeable =
			psy_charger_property_is_writeable;
	info->psy_desc1.external_power_changed =
		mtk_charger_external_power_changed;
	info->psy_cfg1.drv_data = info;
	info->psy1 = power_supply_register(&pdev->dev, &info->psy_desc1,
			&info->psy_cfg1);

	info->chg_psy = devm_power_supply_get_by_phandle(&pdev->dev,
		"charger");
	if (IS_ERR_OR_NULL(info->chg_psy))
		chr_err("%s: devm power fail to get chg_psy\n", __func__);

	info->bat_psy = devm_power_supply_get_by_phandle(&pdev->dev,
		"gauge");
	if (IS_ERR_OR_NULL(info->bat_psy))
		chr_err("%s: devm power fail to get bat_psy\n", __func__);

	if (IS_ERR(info->psy1))
		chr_err("register psy1 fail:%d\n",
			PTR_ERR(info->psy1));

	info->psy_desc2.name = "mtk-slave-charger";
	info->psy_desc2.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_desc2.properties = charger_psy_properties;
	info->psy_desc2.num_properties = ARRAY_SIZE(charger_psy_properties);
	info->psy_desc2.get_property = psy_charger_get_property;
	info->psy_desc2.set_property = psy_charger_set_property;
	info->psy_desc2.property_is_writeable =
			psy_charger_property_is_writeable;
	info->psy_cfg2.drv_data = info;
	info->psy2 = power_supply_register(&pdev->dev, &info->psy_desc2,
			&info->psy_cfg2);
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 start */
	#if !defined(HQ_FACTORY_BUILD)
	info->batt_protect_flag = ENABLE_BATT_PROTECT;
	info->en_batt_protect = DISABLE_BATT_PROTECT;
	info->over_cap_count = 0;
	info->cap_hold_count = 0;
	info->recharge_count = 0;
	info->interval_time = 0;
	/*hs04_u for  AL6398AU-178 by shixuanxuan at 20240117 start*/
	gxy_ttf_init();
	/*hs04_u for  AL6398AU-178 by shixuanxuan at 20240117 end*/
	#endif
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 end */
	if (IS_ERR(info->psy2))
		chr_err("register psy2 fail:%d\n",
			PTR_ERR(info->psy2));

	info->log_level = CHRLOG_DEBUG_LEVEL;

	info->pd_adapter = get_adapter_by_name("pd_adapter");
	if (!info->pd_adapter)
		chr_err("%s: No pd adapter found\n");
	else {
		info->pd_nb.notifier_call = notify_adapter_event;
		register_adapter_device_notifier(info->pd_adapter,
						 &info->pd_nb);
	}
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
	#ifdef CONFIG_AFC_CHARGER
	ret = afc_charge_init(info);
	if (ret < 0)
	{
		info->enable_afc = false;
		info->hv_disable = HV_DISABLE;
	} else {
		info->enable_afc = true;
		info->hv_disable = HV_ENABLE;
	}
	#endif
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/

	info->chg_alg_nb.notifier_call = chg_alg_event;
	/*HS03s for DEVAL5626-469 by liujie at 20210819 start*/
    init_jeita_state_machine(info);
	/*HS03s for DEVAL5626-469 by liujie at 20210819 end*/
	kthread_run(charger_routine_thread, info, "charger_thread");

	return 0;
}
#endif
#ifdef CONFIG_HQ_PROJECT_OT8
#ifndef HQ_FACTORY_BUILD
bool sales_code_is_vzw = false;
EXPORT_SYMBOL(sales_code_is_vzw);
#endif
//for 08
static int mtk_charger_probe(struct platform_device *pdev)
{
	struct mtk_charger *info = NULL;
	int i;
	/*TabA7 Lite code for OT8-106 add afc charger driver by wenyaqi at 20201210 start*/
	#ifdef CONFIG_AFC_CHARGER
	int ret;
	#endif
	/*TabA7 Lite code for OT8-106 add afc charger driver by wenyaqi at 20201210 end*/
	char *name = NULL;

	chr_err("%s: starts\n", __func__);

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	platform_set_drvdata(pdev, info);
	info->pdev = pdev;

	mtk_charger_parse_dt(info, &pdev->dev);

	mutex_init(&info->cable_out_lock);
	mutex_init(&info->charger_lock);
	mutex_init(&info->pd_lock);
	name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "%s",
		"charger suspend wakelock");
	info->charger_wakelock =
		wakeup_source_register(NULL, name);
	spin_lock_init(&info->slock);

	init_waitqueue_head(&info->wait_que);
	info->polling_interval = CHARGING_INTERVAL;
	mtk_charger_init_timer(info);
#ifdef CONFIG_PM
	info->pm_notifier.notifier_call = charger_pm_event;
#endif /* CONFIG_PM */
	srcu_init_notifier_head(&info->evt_nh);
	mtk_charger_setup_files(pdev);
	mtk_charger_get_atm_mode(info);

	for (i = 0; i < CHGS_SETTING_MAX; i++) {
		info->chg_data[i].thermal_charging_current_limit = -1;
		info->chg_data[i].thermal_input_current_limit = -1;
		info->chg_data[i].input_current_limit_by_aicl = -1;
	}
	info->enable_hv_charging = true;
	/*TabA7 Lite  code for SR-AX3565-01-109 by gaoxugang at 20201124 start*/
	#if !defined(HQ_FACTORY_BUILD)
	info->charger_wakelock_app =
		wakeup_source_register(NULL, "wakelockapp");
	INIT_DELAYED_WORK(&info->retail_app_status_change_work, ss_retail_app_status_change_work);
	#endif
	/*TabA7 Lite  code for SR-AX3565-01-109 by gaoxugang at 20201124 end*/
	/* AX3565 for SR-AX3565-01-737 add battery protect function by shixuanxuan at 2021/01/13 start */
	#if !defined(HQ_FACTORY_BUILD)
	INIT_DELAYED_WORK(&info->charging_count_work, hq_charging_count_work);

	//schedule_delayed_work(&info->charging_count_work, msecs_to_jiffies(CHR_COUNT_TIME));
	#endif
	/* AX3565 for SR-AX3565-01-737 add battery protect function by shixuanxuan at 2021/01/13 start */
	/* TabA7 Lite code for HQ00001 Modify battery protect function by shixuanxuan at 2021/05/10 start */
        #if !defined(HQ_FACTORY_BUILD)
	info->current_time = 0;
  	#endif
	/* TabA7 Lite code for HQ00001 Modify battery protect function by shixuanxuan at 2021/05/10 end */
	info->psy_desc1.name = "mtk-master-charger";
	info->psy_desc1.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_desc1.properties = charger_psy_properties;
	info->psy_desc1.num_properties = ARRAY_SIZE(charger_psy_properties);
	info->psy_desc1.get_property = psy_charger_get_property;
	info->psy_desc1.set_property = psy_charger_set_property;
	info->psy_desc1.property_is_writeable =
			psy_charger_property_is_writeable;
	info->psy_desc1.external_power_changed =
		mtk_charger_external_power_changed;
	info->psy_cfg1.drv_data = info;
	info->psy1 = power_supply_register(&pdev->dev, &info->psy_desc1,
			&info->psy_cfg1);

	if (IS_ERR(info->psy1))
		chr_err("register psy1 fail:%d\n",
			PTR_ERR(info->psy1));

	info->psy_desc2.name = "mtk-slave-charger";
	info->psy_desc2.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_desc2.properties = charger_psy_properties;
	info->psy_desc2.num_properties = ARRAY_SIZE(charger_psy_properties);
	info->psy_desc2.get_property = psy_charger_get_property;
	info->psy_desc2.set_property = psy_charger_set_property;
	info->psy_desc2.property_is_writeable =
			psy_charger_property_is_writeable;
	info->psy_cfg2.drv_data = info;
	info->psy2 = power_supply_register(&pdev->dev, &info->psy_desc2,
			&info->psy_cfg2);

	if (IS_ERR(info->psy2))
		chr_err("register psy2 fail:%d\n",
			PTR_ERR(info->psy2));

	info->log_level = CHRLOG_DEBUG_LEVEL;
	/* AX3565 for SR-AX3565-01-737 add battery protect function by shixuanxuan at 2021/01/13 start */
	#if !defined(HQ_FACTORY_BUILD) //factory version
	/* AX3565 for P210308-05696 Modify battery protect function by shixuanxuan at 2021/05/10 start */
	info->batt_protect_flag = ENABLE_BATT_PROTECT;
	info->en_batt_protect = DISABLE_BATT_PROTECT;
	info->over_cap_count = 0;
	info->cap_hold_count = 0;
	info->recharge_count = 0;
	info->interval_time = 0;
	/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 start */
	#ifdef CONFIG_BATT_TIME_TO_FULL
	gxy_ttf_init();
	#endif
	/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 start */
	/* AX3565 for P210308-05696 Modify battery protect function by shixuanxuan at 2021/05/10 end */
	#endif
	/* AX3565 for SR-AX3565-01-737 add battery protect function by shixuanxuan at 2021/01/13 end */
	/*TabA7 Lite code for P210330-05709 by wenyaqi at 20210401 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	info->batt_store_mode = false;
	#endif
	/*TabA7 Lite code for P210330-05709 by wenyaqi at 20210401 end*/
	/*TabA7 Lite code for P210511-00511 by wenyaqi at 20210518 start*/
	#ifndef HQ_FACTORY_BUILD
	//info->ss_chr_type_detect = 0;
	#endif
	/*TabA7 Lite code for P210511-00511 by wenyaqi at 20210518 end*/
	/*TabA7 Lite code for P210511-00533 by wenyaqi at 20210713 start*/

	#ifndef HQ_FACTORY_BUILD
	if(sales_code_is("VZW"))
		sales_code_is_vzw = true;
	else
		sales_code_is_vzw = false;
	#endif
	/*TabA7 Lite code for P210511-00533 by wenyaqi at 20210713 end*/
	info->pd_adapter = get_adapter_by_name("pd_adapter");
	if (!info->pd_adapter)
		chr_err("%s: No pd adapter found\n");
	else {
		info->pd_nb.notifier_call = notify_adapter_event;
		register_adapter_device_notifier(info->pd_adapter,
						 &info->pd_nb);
	}

	/*TabA7 Lite code for OT8-106 add afc charger driver by wenyaqi at 20201210 start*/
	/*TabA7 Lite code for P210209-04223 fix HV_CHARGER_STATUS by wenyaqi at 20210212 start*/
	#ifdef CONFIG_AFC_CHARGER
	ret = afc_charge_init(info);
	if (ret < 0)
	{
		info->enable_afc = false;
		info->hv_disable = HV_DISABLE;
	} else {
		info->enable_afc = true;
		info->hv_disable = HV_ENABLE;
	}
	#endif
	/*TabA7 Lite code for P210209-04223 fix HV_CHARGER_STATUS by wenyaqi at 20210212 end*/
	/*TabA7 Lite code for OT8-106 add afc charger driver by wenyaqi at 20201210 end*/

	info->chg_alg_nb.notifier_call = chg_alg_event;

	kthread_run(charger_routine_thread, info, "charger_thread");

	return 0;
}

#endif//for o8

static int mtk_charger_remove(struct platform_device *dev)
{
	return 0;
}

static void mtk_charger_shutdown(struct platform_device *dev)
{
	struct mtk_charger *info = platform_get_drvdata(dev);
	int i;

	for (i = 0; i < MAX_ALG_NO; i++) {
		if (info->alg[i] == NULL)
			continue;
		chg_alg_stop_algo(info->alg[i]);
	}
}

static const struct of_device_id mtk_charger_of_match[] = {
	{.compatible = "mediatek,charger",},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_charger_of_match);

struct platform_device mtk_charger_device = {
	.name = "charger",
	.id = -1,
};

static struct platform_driver mtk_charger_driver = {
	.probe = mtk_charger_probe,
	.remove = mtk_charger_remove,
	.shutdown = mtk_charger_shutdown,
	.driver = {
		   .name = "charger",
		   .of_match_table = mtk_charger_of_match,
	},
};

static int __init mtk_charger_init(void)
{
	return platform_driver_register(&mtk_charger_driver);
}
late_initcall(mtk_charger_init);

static void __exit mtk_charger_exit(void)
{
	platform_driver_unregister(&mtk_charger_driver);
}
module_exit(mtk_charger_exit);


MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK Charger Driver");
MODULE_LICENSE("GPL");
