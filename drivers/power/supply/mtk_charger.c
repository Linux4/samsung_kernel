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
#include "mtk_battery.h"
#if defined (CONFIG_N23_CHARGER_PRIVATE) || defined (CONFIG_N21_CHARGER_PRIVATE)
#include <linux/fb.h> //bug 682591,xuejizhou.wt,mod,20210807,charge current limit for AP overheat
#include <mtk_musb.h>
#include <charger_class.h>
#endif
#include "mtk_charger.h"
#if defined (CONFIG_N23_CHARGER_PRIVATE)
int mtk_chg_status = 0;
static int pre_chg_stat = 0;

static void ato_charger_limit_soc(struct mtk_charger *info, int min, int max);

int min_soc, max_soc;
extern bool batt_store_mode;
extern bool battery_capacity_limit;
extern bool batt_slate_mode;
#endif
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

#if defined (CONFIG_N23_CHARGER_PRIVATE)
void BATTERY_SetUSBState(int usb_state_value)
{
	static struct mtk_charger *pinfo;
	struct power_supply *psy;

	if (pinfo == NULL) {
		psy = power_supply_get_by_name("mtk-master-charger");
		if (psy == NULL) {
			chr_err("%s: psy is not ready\n", __func__);
			return;
		}

		pinfo = (struct mtk_charger *)power_supply_get_drvdata(psy);
		if (pinfo == NULL) {
			chr_err("%s: mtk_charger is null\n", __func__);
			return;
		}
	}

	if (is_disable_charger(pinfo)) {
		chr_err("%s: in FPGA/EVB, no service\n", __func__);
	} else {
		if (usb_state_value < USB_SUSPEND ||
			usb_state_value > USB_CONFIGURED) {
			chr_err("%s: restore to default value\n", __func__);
			usb_state_value = USB_UNCONFIGURED;
		} else {
			chr_err("%s set usb_state = %d\n", __func__,
				usb_state_value);
			if (pinfo)
				pinfo->usb_state = usb_state_value;
		}
		//+Bug 705135,yangyuhang.wt,ADD,20211124, Connect to USB and it will take about 8 seconds to charge
			if (usb_state_value != USB_SUSPEND) {
				_wake_up_charger(pinfo);
			}
		//-Bug 705135,yangyuhang.wt,ADD,20211124, Connect to USB and it will take about 8 seconds to charge
	}
}
#endif

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
#if defined (CONFIG_N23_CHARGER_PRIVATE) || defined (CONFIG_N21_CHARGER_PRIVATE)
int g_bootmode;
void get_bootmode(struct mtk_charger *info)
{
	g_bootmode = info->bootmode;
}
#endif

static void mtk_charger_parse_dt(struct mtk_charger *info,
				struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 val = 0;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;

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
#if defined (CONFIG_N23_CHARGER_PRIVATE) || defined (CONFIG_N21_CHARGER_PRIVATE)
	get_bootmode(info);
#endif
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
//+Bug 715587,guoyanjun.wt,ADD,20220108,SW JEITA configuration
#if defined (CONFIG_N26_CHARGER_PRIVATE) || defined (CONFIG_N23_CHARGER_PRIVATE) || defined (CONFIG_N21_CHARGER_PRIVATE)
	if (of_property_read_u32(np, "jeita_temp_above_t4_cc", &val) >= 0)
		info->data.jeita_temp_above_t4_cc = val;
	else {
		chr_err("use default JEITA_TEMP_ABOVE_T4_CC:%d\n",
			JEITA_TEMP_ABOVE_T4_CC);
		info->data.jeita_temp_above_t4_cc = JEITA_TEMP_ABOVE_T4_CC;
	}

	if (of_property_read_u32(np, "jeita_temp_t3_to_t4_cc", &val) >= 0)
		info->data.jeita_temp_t3_to_t4_cc = val;
	else {
		chr_err("use default JEITA_TEMP_T3_TO_T4_CC:%d\n",
			JEITA_TEMP_T3_TO_T4_CC);
		info->data.jeita_temp_t3_to_t4_cc = JEITA_TEMP_T3_TO_T4_CC;
	}

	if (of_property_read_u32(np, "jeita_temp_t2_to_t3_cc", &val) >= 0)
		info->data.jeita_temp_t2_to_t3_cc = val;
	else {
		chr_err("use default JEITA_TEMP_T2_TO_T3_CC:%d\n",
			JEITA_TEMP_T2_TO_T3_CC);
		info->data.jeita_temp_t2_to_t3_cc = JEITA_TEMP_T2_TO_T3_CC;
	}

	if (of_property_read_u32(np, "jeita_temp_t1_to_t2_cc", &val) >= 0)
		info->data.jeita_temp_t1_to_t2_cc = val;
	else {
		chr_err("use default JEITA_TEMP_T1_TO_T2_CC:%d\n",
			JEITA_TEMP_T1_TO_T2_CC);
		info->data.jeita_temp_t1_to_t2_cc = JEITA_TEMP_T1_TO_T2_CC;
	}

	if (of_property_read_u32(np, "jeita_temp_t0_to_t1_cc", &val) >= 0)
		info->data.jeita_temp_t0_to_t1_cc = val;
	else {
		chr_err("use default JEITA_TEMP_T0_TO_T1_CC:%d\n",
			JEITA_TEMP_T0_TO_T1_CC);
		info->data.jeita_temp_t0_to_t1_cc = JEITA_TEMP_T0_TO_T1_CC;
	}

	if (of_property_read_u32(np, "jeita_temp_below_t0_cc", &val) >= 0)
		info->data.jeita_temp_below_t0_cc = val;
	else {
		chr_err("use default JEITA_TEMP_BELOW_T0_CC:%d\n",
			JEITA_TEMP_BELOW_T0_CC);
		info->data.jeita_temp_below_t0_cc= JEITA_TEMP_BELOW_T0_CC;
	}
	if (of_property_read_u32(np, "jeita_temp_above_t4_fast_cc", &val) >= 0)
		info->data.jeita_temp_above_t4_fast_cc = val;
	else {
		chr_err("use default JEITA_TEMP_ABOVE_T4_FAST_CC:%d\n",
			JEITA_TEMP_ABOVE_T4_FAST_CC);
		info->data.jeita_temp_above_t4_fast_cc = JEITA_TEMP_ABOVE_T4_FAST_CC;
	}

	if (of_property_read_u32(np, "jeita_temp_t3_to_t4_fast_cc", &val) >= 0)
		info->data.jeita_temp_t3_to_t4_fast_cc = val;
	else {
		chr_err("use default JEITA_TEMP_T3_TO_T4_FAST_CC:%d\n",
			JEITA_TEMP_T3_TO_T4_FAST_CC);
		info->data.jeita_temp_t3_to_t4_fast_cc = JEITA_TEMP_T3_TO_T4_FAST_CC;
	}

	if (of_property_read_u32(np, "jeita_temp_t2_to_t3_fast_cc", &val) >= 0)
		info->data.jeita_temp_t2_to_t3_fast_cc = val;
	else {
		chr_err("use default JEITA_TEMP_T2_TO_T3_FAST_CC:%d\n",
			JEITA_TEMP_T2_TO_T3_FAST_CC);
		info->data.jeita_temp_t2_to_t3_fast_cc = JEITA_TEMP_T2_TO_T3_FAST_CC;
	}

	if (of_property_read_u32(np, "jeita_temp_t1_to_t2_fast_cc", &val) >= 0)
		info->data.jeita_temp_t1_to_t2_fast_cc = val;
	else {
		chr_err("use default JEITA_TEMP_T1_TO_T2_FAST_CC:%d\n",
			JEITA_TEMP_T1_TO_T2_FAST_CC);
		info->data.jeita_temp_t1_to_t2_fast_cc = JEITA_TEMP_T1_TO_T2_FAST_CC;
	}

	if (of_property_read_u32(np, "jeita_temp_t0_to_t1_fast_cc", &val) >= 0)
		info->data.jeita_temp_t0_to_t1_fast_cc = val;
	else {
		chr_err("use default JEITA_TEMP_T0_TO_T1_FAST_CC:%d\n",
			JEITA_TEMP_T0_TO_T1_FAST_CC);
		info->data.jeita_temp_t0_to_t1_fast_cc = JEITA_TEMP_T0_TO_T1_FAST_CC;
	}

	if (of_property_read_u32(np, "jeita_temp_below_t0_fast_cc", &val) >= 0)
		info->data.jeita_temp_below_t0_fast_cc = val;
	else {
		chr_err("use default JEITA_TEMP_BELOW_T0_FAST_CC:%d\n",
			JEITA_TEMP_BELOW_T0_FAST_CC);
		info->data.jeita_temp_below_t0_fast_cc= JEITA_TEMP_BELOW_T0_FAST_CC;
	}

	if (of_property_read_u32(np, "ap_temp_lcmoff_above_t2_cc", &val) >= 0)
		info->data.ap_temp_lcmoff_above_t2_cc = val;
	else {
		chr_err("can't find ap_temp_lcmoff_above_t2_cc\n");
	}
	if (of_property_read_u32(np, "ap_temp_lcmoff_t1_to_t2_cc", &val) >= 0)
		info->data.ap_temp_lcmoff_t1_to_t2_cc = val;
	else {
		chr_err("can't find ap_temp_lcmoff_t1_to_t2_cc\n");
	}
	if (of_property_read_u32(np, "ap_temp_lcmoff_t0_to_t1_cc", &val) >= 0)
		info->data.ap_temp_lcmoff_t0_to_t1_cc = val;
	else {
		chr_err("can't find ap_temp_lcmoff_t0_to_t1_cc\n");
	}
	if (of_property_read_u32(np, "ap_temp_lcmoff_below_t0_cc", &val) >= 0)
		info->data.ap_temp_lcmoff_below_t0_cc = val;
	else {
		chr_err("can't find ap_temp_lcmoff_below_t0_cc\n");
	}
//+P240111-05098   guhan01.wt 2024031820,Modify the maximum current limit for bright screen charging
#if defined (CONFIG_N26_CHARGER_PRIVATE)
	if (of_property_read_u32(np, "ap_temp_lcmon_above_t4", &val) >= 0)
		info->data.ap_temp_lcmon_above_t4 = val;
	else {
		chr_err("can't find ap_temp_lcmon_above_t4\n");
	}
	if (of_property_read_u32(np, "ap_temp_lcmon_t3_to_t4", &val) >= 0)
		info->data.ap_temp_lcmon_t3_to_t4 = val;
	else {
		chr_err("can't find ap_temp_lcmon_t3_to_t4\n");
	}
	if (of_property_read_u32(np, "ap_temp_lcmon_t2_to_t3", &val) >= 0)
		info->data.ap_temp_lcmon_t2_to_t3 = val;
	else {
		chr_err("can't find ap_temp_lcmon_t2_to_t3\n");
	}
	if (of_property_read_u32(np, "ap_temp_lcmon_t1_to_t2", &val) >= 0)
		info->data.ap_temp_lcmon_t1_to_t2 = val;
	else {
		chr_err("can't find ap_temp_lcmon_t1_to_t2\n");
	}
	if (of_property_read_u32(np, "ap_temp_lcmon_t0_to_t1", &val) >= 0)
		info->data.ap_temp_lcmon_t0_to_t1 = val;
	else {
		chr_err("can't find ap_temp_lcmon_t0_to_t1\n");
	}
	if (of_property_read_u32(np, "ap_temp_lcmon_below_t0", &val) >= 0)
		info->data.ap_temp_lcmon_below_t0 = val;
	else {
		chr_err("can't find ap_temp_lcmon_below_t0\n");
	}
#else
//-P240111-05098   guhan01.wt 2024031820,Modify the maximum current limit for bright screen charging
	if (of_property_read_u32(np, "ap_temp_lcmon_above_t0_cc", &val) >= 0)
		info->data.ap_temp_lcmon_above_t0_cc = val;
	else {
		chr_err("can't find ap_temp_lcmon_above_t0_cc\n");
	}
	if (of_property_read_u32(np, "ap_temp_lcmon_below_t0_cc", &val) >= 0)
		info->data.ap_temp_lcmon_below_t0_cc = val;
	else {
		chr_err("can't find ap_temp_lcmon_below_t0_cc\n");
	}
#endif
	if (of_property_read_u32(np, "ap_lcmoff_t2_thres", &val) >= 0)
		info->data.ap_lcmoff_t2_thres = val;
	else {
		chr_err("can't find ap_lcmoff_t2_thres\n");
	}
	if (of_property_read_u32(np, "ap_lcmoff_t2_thres_plus_x_degree", &val) >= 0)
		info->data.ap_lcmoff_t2_thres_plus_x_degree = val;
	else {
		chr_err("can't find ap_lcmoff_t2_thres_plus_x_degree\n");
	}
	if (of_property_read_u32(np, "ap_lcmoff_t1_thres", &val) >= 0)
		info->data.ap_lcmoff_t1_thres = val;
	else {
		chr_err("can't find ap_lcmoff_t1_thres\n");
	}
	if (of_property_read_u32(np, "ap_lcmoff_t1_thres_plus_x_degree", &val) >= 0)
		info->data.ap_lcmoff_t1_thres_plus_x_degree = val;
	else {
		chr_err("can't find ap_lcmoff_t1_thres_plus_x_degree\n");
	}
	if (of_property_read_u32(np, "ap_lcmoff_t0_thres", &val) >= 0)
		info->data.ap_lcmoff_t0_thres = val;
	else {
		chr_err("can't find ap_lcmoff_t0_thres\n");
	}
	if (of_property_read_u32(np, "ap_lcmoff_t0_thres_plus_x_degree", &val) >= 0)
		info->data.ap_lcmoff_t0_thres_plus_x_degree = val;
	else {
		chr_err("can't find ap_lcmoff_t0_thres_plus_x_degree\n");
	}
//+P240111-05098   guhan01.wt 2024031820,Modify the maximum current limit for bright screen charging
#if defined (CONFIG_N26_CHARGER_PRIVATE)
	if (of_property_read_u32(np, "ap_temp_lcmon_t4", &val) >= 0)
		info->data.ap_temp_lcmon_t4 = val;
	else {
		chr_err("can't find ap_temp_lcmon_t4\n");
	}
	if (of_property_read_u32(np, "ap_temp_lcmon_t3", &val) >= 0)
		info->data.ap_temp_lcmon_t3 = val;
	else {
		chr_err("can't find ap_temp_lcmon_t3\n");
	}
	if (of_property_read_u32(np, "ap_temp_lcmon_t2", &val) >= 0)
		info->data.ap_temp_lcmon_t2 = val;
	else {
		chr_err("can't find ap_temp_lcmon_t2\n");
	}
	if (of_property_read_u32(np, "ap_temp_lcmon_t1", &val) >= 0)
		info->data.ap_temp_lcmon_t1 = val;
	else {
		chr_err("can't find ap_temp_lcmon_t1\n");
	}
	if (of_property_read_u32(np, "ap_temp_lcmon_t0", &val) >= 0)
		info->data.ap_temp_lcmon_t0 = val;
	else {
		chr_err("can't find ap_temp_lcmon_t0\n");
	}

	if (of_property_read_u32(np, "ap_temp_lcmon_t4_anti_shake", &val) >= 0)
		info->data.ap_temp_lcmon_t4_anti_shake = val;
	else {
		chr_err("can't find ap_temp_lcmon_t4_anti_shake\n");
	}
	if (of_property_read_u32(np, "ap_temp_lcmon_t3_anti_shake", &val) >= 0)
		info->data.ap_temp_lcmon_t3_anti_shake = val;
	else {
		chr_err("can't find ap_temp_lcmon_t3_anti_shake\n");
	}
	if (of_property_read_u32(np, "ap_temp_lcmon_t2_anti_shake", &val) >= 0)
		info->data.ap_temp_lcmon_t2_anti_shake = val;
	else {
		chr_err("can't find ap_temp_lcmon_t2_anti_shake\n");
	}
	if (of_property_read_u32(np, "ap_temp_lcmon_t1_anti_shake", &val) >= 0)
		info->data.ap_temp_lcmon_t1_anti_shake = val;
	else {
		chr_err("can't find ap_temp_lcmon_t1_anti_shake\n");
	}
	if (of_property_read_u32(np, "ap_temp_lcmon_t0_anti_shake", &val) >= 0)
		info->data.ap_temp_lcmon_t0_anti_shake = val;
	else {
		chr_err("can't find ap_temp_lcmon_t0_anti_shake\n");
	}
#else
//-P240111-05098   guhan01.wt 2024031820,Modify the maximum current limit for bright screen charging
	if (of_property_read_u32(np, "ap_lcmon_t0_thres", &val) >= 0)
		info->data.ap_lcmon_t0_thres = val;
	else {
		chr_err("can't find ap_lcmon_t0_thres\n");
	}
	if (of_property_read_u32(np, "ap_lcmon_t0_thres_plus_x_degree", &val) >= 0)
		info->data.ap_lcmon_t0_thres_plus_x_degree = val;
	else {
		chr_err("can't find ap_lcmon_t0_thres_plus_x_degree\n");
	}
#endif
#ifdef CONFIG_MTK_DISABLE_TEMP_PROTECT
	info->data.battery_cv = 4100000;
	info->enable_sw_jeita = false;
#endif
#endif
//-Bug 715587,guoyanjun.wt,ADD,20220108,SW JEITA configuration
#if defined (CONFIG_N23_CHARGER_PRIVATE) || defined (CONFIG_N21_CHARGER_PRIVATE)
	info->data.ap_temp_above_t2_cc = AP_TEMP_ABOVE_T2_CC;
	info->data.ap_temp_t1_to_t2_cc = AP_TEMP_T1_TO_T2_CC;
	info->data.ap_temp_t0_to_t1_cc = AP_TEMP_T0_TO_T1_CC;
	info->data.ap_temp_below_t0_cc = AP_TEMP_BELOW_T0_CC;
	info->data.ap_temp_high_lcmon_cc = AP_TEMP_HIGH_CC_LCMON;
	info->data.ap_temp_low_lcmon_cc = AP_TEMP_LOW_CC_LCMON;

	info->data.ap_temp_t2_thres = AP_TEMP_T2_THRES;
	info->data.ap_temp_t2_thres_minus_x_degree = AP_TEMP_T2_THRES_MINUS_X_DEGREE;
	info->data.ap_temp_t1_thres = AP_TEMP_T1_THRES;
	info->data.ap_temp_t1_thres_minus_x_degree = AP_TEMP_T1_THRES_MINUS_X_DEGREE;
	info->data.ap_temp_t0_thres = AP_TEMP_T0_THRES;
	info->data.ap_temp_t0_thres_minus_x_degree = AP_TEMP_T0_THRES_MINUS_X_DEGREE;
	info->data.ap_temp_thres_lcmon = AP_TEMP_THRES_LCMON;
	info->data.ap_temp_thres_minus_x_degree_lcmon = AP_TEMP_THRES_MINUS_X_DEGREE_LCMON;
#endif
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

	if (of_property_read_u32(np, "charging_host_charger_current", &val)
		>= 0) {
		info->data.charging_host_charger_current = val;
	} else {
		chr_err("use default CHARGING_HOST_CHARGER_CURRENT:%d\n",
			CHARGING_HOST_CHARGER_CURRENT);
		info->data.charging_host_charger_current =
					CHARGING_HOST_CHARGER_CURRENT;
	}
#if defined (CONFIG_N26_CHARGER_PRIVATE)
	if (of_property_read_u32(np, "fast_charger_current", &val) >= 0) {
		info->data.fast_charger_current = val;
	} else {
		chr_err("use default FAST_CHARGER_CURRENT:%d\n",
			FAST_CHARGER_CURRENT);
		info->data.fast_charger_current = FAST_CHARGER_CURRENT;
	}

	if (of_property_read_u32(np, "fast_charger_input_current", &val) >= 0)
		info->data.fast_charger_input_current = val;
	else {
		chr_err("use default FAST_CHARGER_INPUT_CURRENT:%d\n",
			FAST_CHARGER_INPUT_CURRENT);
		info->data.fast_charger_input_current = FAST_CHARGER_INPUT_CURRENT;
	}
#endif
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
	time.tv_sec = info->polling_interval;
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

#if defined (CONFIG_N21_CHARGER_PRIVATE) || defined (CONFIG_N23_CHARGER_PRIVATE)
bool fast_charger_connect(struct mtk_charger *info)
{
	int i = 0, ret = 0;
	bool is_fast_charge = false;
	struct chg_alg_device *alg = NULL;
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
	return is_fast_charge;
}
EXPORT_SYMBOL_GPL(fast_charger_connect);
#endif

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
#if defined (CONFIG_N26_CHARGER_PRIVATE)
	if (!is_fast_charge && !info->wtchg_info->is_afc_charger) 
#else
	if (!is_fast_charge)
#endif
	{
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
#if defined (CONFIG_N23_CHARGER_PRIVATE)
	} else if (info->battery_temp >= info->data.temp_t3_thres) {
#else
	} else if (info->battery_temp > info->data.temp_t3_thres) {
		/* control 45 degree to normal behavior */
#endif
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
	
//+Bug 715587,guoyanjun.wt,ADD,20220108,SW JEITA configuration
#if defined (CONFIG_N26_CHARGER_PRIVATE)
	if(sw_jeita->charging){
		int jeita_cc = 0,ap_cc = 0;
		static bool ap_limit = false;
		extern int mtkts_bts_get_hw_temp(void);
		int ap_temp = mtkts_bts_get_hw_temp()/1000;

		switch(sw_jeita->sm){
		case TEMP_ABOVE_T4:
			sw_jeita->cv = info->data.jeita_temp_above_t4_cv;
			if(info->wtchg_info->afc_enable)
				jeita_cc = info->data.jeita_temp_above_t4_fast_cc;
			else
				jeita_cc = info->data.jeita_temp_above_t4_cc;
			break;
		case TEMP_T3_TO_T4:
			sw_jeita->cv = info->data.jeita_temp_t3_to_t4_cv;
			if(info->wtchg_info->afc_enable)
				jeita_cc = info->data.jeita_temp_t3_to_t4_fast_cc;
			else
				jeita_cc = info->data.jeita_temp_t3_to_t4_cc;
			break;
		case TEMP_T2_TO_T3:
			sw_jeita->cv = info->data.jeita_temp_t2_to_t3_cv;
			if(info->wtchg_info->afc_enable)
				jeita_cc = info->data.jeita_temp_t2_to_t3_fast_cc;
			else
				jeita_cc = info->data.jeita_temp_t2_to_t3_cc;
			break;
		case TEMP_T1_TO_T2:
			sw_jeita->cv = info->data.jeita_temp_t1_to_t2_cv;
			if(info->wtchg_info->afc_enable)
				jeita_cc = info->data.jeita_temp_t1_to_t2_fast_cc;
			else
				jeita_cc = info->data.jeita_temp_t1_to_t2_cc;
			break;
		case TEMP_T0_TO_T1:
			sw_jeita->cv = info->data.jeita_temp_t0_to_t1_cv;
			if(info->wtchg_info->afc_enable)
				jeita_cc = info->data.jeita_temp_t0_to_t1_fast_cc;
			else
				jeita_cc = info->data.jeita_temp_t0_to_t1_cc;
			break;
		case TEMP_BELOW_T0:
			sw_jeita->cv = info->data.jeita_temp_below_t0_cv;
			if(info->wtchg_info->afc_enable)
				jeita_cc = info->data.jeita_temp_below_t0_fast_cc;
			else
				jeita_cc = info->data.jeita_temp_below_t0_cc;
			break;
		default:
			sw_jeita->cv = info->data.battery_cv;
			if(info->wtchg_info->afc_enable)
				jeita_cc = info->data.fast_charger_current;
			else
				jeita_cc = info->data.ac_charger_current;
		
		}
/*+S96616AA3-534 lijiawei,wt.battery cycle function and otg control function*/
#if defined (CONFIG_N26_CHARGER_PRIVATE)
	if (1) {
		int cycle_fv = wt_set_batt_cycle_fv(NULL,false);
		if ((cycle_fv != 0) && (sw_jeita->cv > cycle_fv))
			sw_jeita->cv = cycle_fv;
	}
#endif
/*-S96616AA3-534 lijiawei,wt.battery cycle function and otg control function*/
		chr_err("[SW_JEITA] sm = %d, jeita_cc = %d, cv = %d, \n",sw_jeita->sm,jeita_cc,sw_jeita->cv);
		if(info->wtchg_info->lcm_on){
//+P240111-05098   guhan01.wt 2024031820,Modify the maximum current limit for bright screen charging
#if defined (CONFIG_N26_CHARGER_PRIVATE)
			static int ap_lcm_on_state = 0;
			if(ap_temp >= info->data.ap_temp_lcmon_t4){
				ap_lcm_on_state = AP_ABOVE_T4;
				chr_err("[AP_THERMAL] AP Over Temperature(%d) !!\n",
					info->data.ap_temp_lcmon_t4);
			} else {
				if(ap_temp >= info->data.ap_temp_lcmon_t3){
					if(ap_lcm_on_state == AP_ABOVE_T4 && ap_temp >= info->data.ap_temp_lcmon_t4_anti_shake){
						chr_err("[AP_THERMAL] anti_shake AP Temperature between %d and %d !!\n",
							info->data.ap_temp_lcmon_t3,
							info->data.ap_temp_lcmon_t4_anti_shake);
					} else {
						ap_lcm_on_state = TEMP_T3_TO_T4;
						chr_err("[AP_THERMAL] AP Temperature between %d and %d !!\n",
							info->data.ap_temp_lcmon_t3,
							info->data.ap_temp_lcmon_t4);
					}
				} else {
					if(ap_temp >= info->data.ap_temp_lcmon_t2){
						if(ap_lcm_on_state == TEMP_T3_TO_T4 && ap_temp >= info->data.ap_temp_lcmon_t3_anti_shake){
							chr_err("[AP_THERMAL] anti_shake AP Temperature between %d and %d !!\n",
								info->data.ap_temp_lcmon_t2,
								info->data.ap_temp_lcmon_t3_anti_shake);
						} else {
							ap_lcm_on_state = AP_T2_TO_T3;
							chr_err("[AP_THERMAL] AP Temperature between %d and %d !!\n",
								info->data.ap_temp_lcmon_t2,
								info->data.ap_temp_lcmon_t3);
						}
					} else {
						if(ap_temp >= info->data.ap_temp_lcmon_t1){
							if(ap_lcm_on_state == AP_T2_TO_T3 && ap_temp >= info->data.ap_temp_lcmon_t2_anti_shake){
								chr_err("[AP_THERMAL] anti_shake AP Temperature between %d and %d !!\n",
									info->data.ap_temp_lcmon_t1,
									info->data.ap_temp_lcmon_t2_anti_shake);
							} else {
								ap_lcm_on_state = AP_T1_TO_T2;
								chr_err("[AP_THERMAL] AP Temperature between %d and %d !!\n",
									info->data.ap_temp_lcmon_t1,
									info->data.ap_temp_lcmon_t2);
							}
						} else {
							if(ap_temp >= info->data.ap_temp_lcmon_t0){
								if(ap_lcm_on_state == AP_T1_TO_T2 && ap_temp >= info->data.ap_temp_lcmon_t1_anti_shake){
									chr_err("[AP_THERMAL] anti_shake AP Temperature between %d and %d !!\n",
										info->data.ap_temp_lcmon_t0,
										info->data.ap_temp_lcmon_t1_anti_shake);
								} else {
									ap_lcm_on_state = AP_T0_TO_T1;
									chr_err("[AP_THERMAL] AP Temperature between %d and %d !!\n",
										info->data.ap_temp_lcmon_t0,
										info->data.ap_temp_lcmon_t1);
								}
							} else {
								if(ap_lcm_on_state == AP_T0_TO_T1 && ap_temp >= info->data.ap_temp_lcmon_t0_anti_shake){
									chr_err("[AP_THERMAL] anti_shake AP Below Temperature(%d) !!\n",
										info->data.ap_temp_lcmon_t0_anti_shake);
								} else {
									ap_lcm_on_state = AP_BELOW_T0;
									chr_err("[AP_THERMAL]  AP Below Temperature(%d) !!\n",
										info->data.ap_temp_lcmon_t0);
								}
							}
						}
					}
				}
			}
			switch(ap_lcm_on_state)
			{
				case AP_ABOVE_T4:
					ap_cc = info->data.ap_temp_lcmon_above_t4;
					break;
				case TEMP_T3_TO_T4:
					ap_cc = info->data.ap_temp_lcmon_t3_to_t4;
					break;
				case AP_T2_TO_T3:
					ap_cc = info->data.ap_temp_lcmon_t2_to_t3;
					break;
				case AP_T1_TO_T2:
					ap_cc = info->data.ap_temp_lcmon_t1_to_t2;
					break;
				case AP_T0_TO_T1:
					ap_cc = info->data.ap_temp_lcmon_t0_to_t1;
					break;
				case AP_BELOW_T0:
					ap_cc = info->data.ap_temp_lcmon_below_t0;
					break;
				default:
				ap_cc = info->data.ap_temp_lcmon_t2_to_t3;
			}
			chr_err("[AP_THERMAL] lcm on! ap_temp = %d, ap_cc = %d\n",ap_temp, ap_cc);
#else
			if(ap_temp > info->data.ap_lcmon_t0_thres){
				ap_limit = true;
			}else{
				if(ap_limit && ap_temp <= info->data.ap_lcmon_t0_thres_plus_x_degree){
					ap_limit = false;
				}
			}
			if(ap_limit)
				ap_cc = info->data.ap_temp_lcmon_above_t0_cc;
			else
				ap_cc = info->data.ap_temp_lcmon_below_t0_cc;

			chr_err("[AP_THERMAL] lcm on! ap_temp = %d, ap_cc = %d\n",ap_temp, ap_cc);
#endif
//-P240111-05098   guhan01.wt 2024031820,Modify the maximum current limit for bright screen charging
		}else{
			static int ap_state = 0;
			if (ap_temp > info->data.ap_lcmoff_t2_thres) {//>48
				chr_err("[AP_THERMAL] AP Over Temperature(%d) !!\n",info->data.ap_lcmoff_t2_thres);
//+P240111-05098   guhan01.wt 2024031820,Modify the maximum current limit for bright screen charging
				ap_state = TEMP_T2_TO_T3;
//-P240111-05098   guhan01.wt 2024031820,Modify the maximum current limit for bright screen charging
				ap_cc = info->data.ap_temp_lcmoff_above_t2_cc;
			} else if (ap_temp > info->data.ap_lcmoff_t1_thres) {//>45
//+P240111-05098   guhan01.wt 2024031820,Modify the maximum current limit for bright screen charging
				if ((ap_state == TEMP_T2_TO_T3)
//-P240111-05098   guhan01.wt 2024031820,Modify the maximum current limit for bright screen charging
				    && (ap_temp >= info->data.ap_lcmoff_t2_thres_plus_x_degree)) {//>=48
					chr_err("[AP_THERMAL] AP Temperature between %d and %d,not change!!\n",
						info->data.ap_lcmoff_t2_thres_plus_x_degree,
						info->data.ap_lcmoff_t2_thres);
					ap_cc = info->data.ap_temp_lcmoff_above_t2_cc;
				} else {
					chr_err("[AP_THERMAL] AP Temperature between %d and %d !!\n",
						info->data.ap_lcmoff_t1_thres,
						info->data.ap_lcmoff_t2_thres);
					ap_state = AP_T1_TO_T2;
					ap_cc = info->data.ap_temp_lcmoff_t1_to_t2_cc;
				}
			} else if (ap_temp >= info->data.ap_lcmoff_t0_thres) {//>=40
				if ((ap_state == AP_T1_TO_T2)
				    && (ap_temp >= info->data.ap_lcmoff_t1_thres_plus_x_degree)) {//>=45
					chr_err("[AP_THERMAL] AP Temperature between %d and %d,not change!!\n",
						info->data.ap_lcmoff_t1_thres_plus_x_degree,
						info->data.ap_lcmoff_t1_thres);
					ap_cc = info->data.ap_temp_lcmoff_t1_to_t2_cc;
				} else {
					chr_err("[AP_THERMAL] AP Temperature between %d and %d !!\n",
						info->data.ap_lcmoff_t1_thres,
						info->data.ap_lcmoff_t2_thres);
					ap_state = AP_T0_TO_T1;
					ap_cc = info->data.ap_temp_lcmoff_t0_to_t1_cc;
				}
			}else if (ap_temp < info->data.ap_lcmoff_t0_thres) {//<40
				if ((ap_state == AP_T0_TO_T1)
				    && (ap_temp >= info->data.ap_lcmoff_t0_thres_plus_x_degree)) {//>=39
					chr_err("[AP_THERMAL] AP Temperature between %d and %d,not change!!\n",
						info->data.ap_lcmoff_t0_thres_plus_x_degree,
						info->data.ap_lcmoff_t0_thres);
					ap_cc = info->data.ap_temp_lcmoff_t0_to_t1_cc;
				} else {
					chr_err("[AP_THERMAL] AP Temperature below %d !!\n",
						info->data.ap_lcmoff_t1_thres,
						info->data.ap_lcmoff_t2_thres);
					ap_state = AP_BELOW_T0;
					ap_cc = info->data.ap_temp_lcmoff_below_t0_cc;
				}
			}
			chr_err("[AP_THERMAL] lcm off! ap_temp = %d, ap_cc = %d\n",ap_temp, ap_cc);
		}
		if(jeita_cc > ap_cc)
			sw_jeita->cc = ap_cc;
		else
			sw_jeita->cc = jeita_cc;
		chr_err("[SW_JEITA][AP_THERMAL] preState:%d newState:%d tmp:%d cv:%d cc:%d jeita_cc:%d, ap_cc:%d\n",
			sw_jeita->pre_sm, sw_jeita->sm, info->battery_temp,
			sw_jeita->cv,sw_jeita->cc,jeita_cc,ap_cc);
	}
	
#elif defined (CONFIG_N23_CHARGER_PRIVATE) || defined (CONFIG_N21_CHARGER_PRIVATE)
	if (sw_jeita->sm == TEMP_ABOVE_T4) {
		sw_jeita->cv = info->data.jeita_temp_above_t4_cv;
		sw_jeita->cc = info->data.jeita_temp_above_t4_cc;
	} else if (sw_jeita->sm == TEMP_T3_TO_T4) {
		sw_jeita->cv = info->data.jeita_temp_t3_to_t4_cv;
		sw_jeita->cc = info->data.jeita_temp_t3_to_t4_cc;
	} else if (sw_jeita->sm == TEMP_T2_TO_T3) {
		sw_jeita->cv = info->data.jeita_temp_t2_to_t3_cv;;
		sw_jeita->cc = info->data.jeita_temp_t2_to_t3_cc;
	} else if (sw_jeita->sm == TEMP_T1_TO_T2) {
		sw_jeita->cv = info->data.jeita_temp_t1_to_t2_cv;
		sw_jeita->cc = info->data.jeita_temp_t1_to_t2_cc;
	} else if (sw_jeita->sm == TEMP_T0_TO_T1) {
		sw_jeita->cv = info->data.jeita_temp_t0_to_t1_cv;
		sw_jeita->cc = info->data.jeita_temp_t0_to_t1_cc;
	} else if (sw_jeita->sm == TEMP_BELOW_T0) {
		sw_jeita->cv = info->data.jeita_temp_below_t0_cv;
		sw_jeita->cc = info->data.jeita_temp_below_t0_cc;
	} else {
		sw_jeita->cv = info->data.battery_cv;
		sw_jeita->cc = info->data.ac_charger_input_current;
	}
#if defined (CONFIG_N23_CHARGER_PRIVATE)
	if (1) {
		int cycle_fv = wt_set_batt_cycle_fv(NULL);
		if ((cycle_fv != 0) && (sw_jeita->cv > cycle_fv))
			sw_jeita->cv = cycle_fv;
	}
#endif
	chr_err("[SW_JEITA]preState:%d newState:%d tmp:%d cv:%d cc:%d\n",
		sw_jeita->pre_sm, sw_jeita->sm, info->battery_temp,
		sw_jeita->cv,sw_jeita->cc);
#else
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
#endif
//-Bug 715587,guoyanjun.wt,ADD,20220108,SW JEITA configuration

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
#if defined (CONFIG_N23_CHARGER_PRIVATE)
extern int mtkts_bts_get_hw_temp(void);
extern struct mtk_battery *get_mtk_battery(void);
extern int force_get_tbat(struct mtk_battery *gm, bool update);
void ap_thermal_machine(struct mtk_charger *info)
{
	struct sw_jeita_data *lcmoff_data;
	struct sw_jeita_data *lcmon_data;
	int ap_temp;
	int bat_temp;
	struct mtk_battery *gm;

	gm = get_mtk_battery();
	if (!gm)
		return;

	bat_temp = force_get_tbat(gm, true);
	pr_err("%s bat_temp = %d\n", __func__, bat_temp);
	if(info->ap_temp != 0xffff) {
		ap_temp = info->ap_temp;
	} else {
		ap_temp = mtkts_bts_get_hw_temp();
		ap_temp = ap_temp/1000;
	}

	lcmoff_data = &info->ap_thermal_lcmoff;
	lcmoff_data->pre_sm = lcmoff_data->sm;
	lcmon_data = &info->ap_thermal_lcmon;
	lcmon_data->pre_sm = lcmon_data->sm;

	if(info->lcmoff) {
		if (bat_temp >= info->data.ap_temp_t2_thres) {
			chr_err("[AP_THERMAL] AP Over %d Temperature!!\n", info->data.ap_temp_t2_thres);

			lcmoff_data->sm = TEMP_T2_TO_T3;
		} else if (bat_temp >= info->data.ap_temp_t1_thres) {
			if ((lcmoff_data->sm == TEMP_T2_TO_T3)
			    && (bat_temp >= info->data.ap_temp_t2_thres_minus_x_degree)) {
				chr_err("[AP_THERMAL] AP Temperature between %d and %d!!\n",
					info->data.ap_temp_t2_thres_minus_x_degree,
					info->data.ap_temp_t2_thres);
			} else {
				chr_err("[AP_THERMAL] AP Temperature between %d and %d !!\n",
					info->data.ap_temp_t1_thres,
					info->data.ap_temp_t2_thres_minus_x_degree);

				lcmoff_data->sm = TEMP_T1_TO_T2;
			}
		} else if (bat_temp >= info->data.ap_temp_t0_thres) {
			if ((lcmoff_data->sm == TEMP_T1_TO_T2)
			     && (bat_temp >= info->data.ap_temp_t1_thres_minus_x_degree)){
			    chr_err("[AP_THERMAL] AP Temperature between %d and %d!!\n",
					info->data.ap_temp_t1_thres_minus_x_degree,
					info->data.ap_temp_t1_thres);
			} else {
				chr_err("[AP_THERMAL] AP Temperature between %d and %d !!\n",
					info->data.ap_temp_t0_thres,
					info->data.ap_temp_t1_thres_minus_x_degree);
				lcmoff_data->sm = TEMP_T0_TO_T1;
			}
		} else {
			if ((lcmoff_data->sm == TEMP_T0_TO_T1)
				&& (bat_temp >= info->data.ap_temp_t0_thres_minus_x_degree)) {
				chr_err("[AP_THERMAL] AP Temperature between %d and %d !!\n",
					info->data.ap_temp_t0_thres_minus_x_degree,
					info->data.ap_temp_t0_thres);
			} else {
				chr_err("[AP_THERMAL] AP below Temperature(%d) !!\n",
					info->data.ap_temp_t0_thres);
				lcmoff_data->sm = TEMP_BELOW_T0;
			}
		}

		if (lcmoff_data->sm == TEMP_T2_TO_T3) {
			lcmoff_data->cc = info->data.ap_temp_above_t2_cc;
		} else if (lcmoff_data->sm == TEMP_T1_TO_T2) {
			lcmoff_data->cc = info->data.ap_temp_t1_to_t2_cc;
		} else if (lcmoff_data->sm == TEMP_T0_TO_T1) {
			lcmoff_data->cc = info->data.ap_temp_t0_to_t1_cc;
		} else if (lcmoff_data->sm == TEMP_BELOW_T0) {
			lcmoff_data->cc = info->data.ap_temp_below_t0_cc;
		}

		chr_err("[AP_THERMAL]lcd is off!!preState:%d newState:%d tmp:%d cc:%d\n",
			lcmoff_data->pre_sm, lcmoff_data->sm, ap_temp, lcmoff_data->cc);
	} else {
		if(ap_temp >= info->data.ap_temp_thres_lcmon) {
			lcmon_data->sm = TEMP_T0_TO_T1;
			chr_err("[AP_THERMAL] AP Over Temperature(%d) !!\n",
				info->data.ap_temp_thres_lcmon);
		} else {
			if((lcmon_data->sm == TEMP_T0_TO_T1)
				&& (ap_temp >= info->data.ap_temp_thres_minus_x_degree_lcmon)) {
				chr_err("[AP_THERMAL] AP Temperature between %d and %d !!\n",
					info->data.ap_temp_thres_minus_x_degree_lcmon,
					info->data.ap_temp_thres_lcmon);
			} else {
				lcmon_data->sm = TEMP_BELOW_T0;
				chr_err("[AP_THERMAL] AP below Temperature(%d) !!\n",
				info->data.ap_temp_thres_minus_x_degree_lcmon);
			}
		}

		if (lcmon_data->sm == TEMP_BELOW_T0) {
			lcmon_data->cc = info->data.ap_temp_low_lcmon_cc;
		} else if (lcmon_data->sm == TEMP_T0_TO_T1) {
			lcmon_data->cc = info->data.ap_temp_high_lcmon_cc;
		}

		chr_err("[AP_THERMAL]lcd is on!!!preState:%d newState:%d tmp:%d cc:%d\n",
			lcmon_data->pre_sm, lcmon_data->sm, ap_temp, lcmon_data->cc);
	}
}
#endif

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
#if defined (CONFIG_N23_CHARGER_PRIVATE) || defined (CONFIG_N21_CHARGER_PRIVATE)
static ssize_t show_ap_thermal(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->ap_temp);
	return sprintf(buf, "%d\n", pinfo->ap_temp);
}

static ssize_t store_ap_thermal(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		pinfo->ap_temp = temp;
	} else {
		chr_err("%s: format error!\n", __func__);
	}
	chr_err("%s: %d\n", __func__, pinfo->ap_temp);
	return size;
}

static DEVICE_ATTR(ap_thermal, 0664, show_ap_thermal,
		   store_ap_thermal);
#endif

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

	vchr = get_vbus(info) * 1000; /* uV */
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

	vchr = get_vbus(info) * 1000; /* uV */
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

#if defined (CONFIG_N23_CHARGER_PRIVATE)
extern int wt_chg_sw_term;
#define WT_CHG_TERM_CURRENT (180)
#define WT_CHG_CV_HYS		(50)
#define WT_CHG_RECHARGE_HYS		(100)

static int mtk_chg_check_term(struct mtk_charger *info)
{
	int ret = 0;
	int bat_vol = 0, bat_cur = 0, bat_sta = 0;
	int chg_cv = 0;
	int ui_soc = 0;
	static int check_count = 0;
	union power_supply_propval propval;
	struct mtk_battery *gm;
	struct battery_data *bs_data;
	struct sw_jeita_data *sw_jeita;
	struct power_supply *bat_psy = power_supply_get_by_name("battery");
	if (bat_psy == NULL) {
		return ret;
	}

	gm = (struct mtk_battery *)power_supply_get_drvdata(bat_psy);
	if (gm == NULL) {
		return ret;
	}
	bs_data = &gm->bs_data;

	power_supply_get_property(bat_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW,
		&propval);
	bat_vol = propval.intval / 1000;//mV

	power_supply_get_property(bat_psy, POWER_SUPPLY_PROP_CURRENT_NOW,
		&propval);
	bat_cur = propval.intval / 1000;//mA

	power_supply_get_property(bat_psy, POWER_SUPPLY_PROP_STATUS,
		&propval);
	bat_sta = propval.intval;		//status

	ui_soc = get_uisoc(info);

	sw_jeita = &info->sw_jeita;
	chg_cv = sw_jeita->cv / 1000;
	if (!chg_cv)
		chg_cv = 4400;				//normal cv point

	if ((bat_sta == POWER_SUPPLY_STATUS_CHARGING)
			&& (wt_chg_sw_term == 0)) {
		if ((bat_vol > (chg_cv - WT_CHG_CV_HYS))			//vbat > 4350
			&& (bat_cur < WT_CHG_TERM_CURRENT))
		{
			if (check_count > 3) {
				wt_chg_sw_term = 1;
				ret = 1;		//stop charging
				pr_err("charge done, stop charge\n");
			} else
				check_count ++;

		} else {
			pr_err("charging in prograss\n");
			check_count = 0;
		}

	} else if (wt_chg_sw_term
			&& (bat_vol > (chg_cv - WT_CHG_RECHARGE_HYS))) //vbat > 4300
	{
		check_count = 0;
		wt_chg_sw_term = 1;
		pr_err("keep stop charge\n");
		ret = 1;			//keep stop charging
	} else {
		check_count = 0;
		wt_chg_sw_term = 0;
		pr_err("plug out usb/bat drop down, resume charge\n");
		ret = 0;
	}

	pr_err("cv:%d, soc:%d, vbat:%d, ibat:%d, bat status:%d, term flag:%d, count:%d, wt_chg_sw_term:%d\n",
		chg_cv, ui_soc, bat_vol, bat_cur, bat_sta, wt_chg_sw_term, check_count, wt_chg_sw_term);
	return ret;

}
#endif

static void charger_check_status(struct mtk_charger *info)
{
	bool charging = true;
	int temperature;
	struct battery_thermal_protection_data *thermal;

	if (get_charger_type(info) == POWER_SUPPLY_TYPE_UNKNOWN)
		return;

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
#if defined (CONFIG_N23_CHARGER_PRIVATE) || defined (CONFIG_N21_CHARGER_PRIVATE)
	ap_thermal_machine(info); //bug 682591,xuejizhou.wt,mod,20210807,charge current limit for AP overheat
#endif
	mtk_chg_get_tchg(info);
#if defined (CONFIG_N23_CHARGER_PRIVATE)
	if(mtk_chg_check_term(info))
	{
		charging = false;
	}
#endif
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
#if defined (CONFIG_N23_CHARGER_PRIVATE)
	if (mtk_chg_status != 0)
		charging = false;
#endif

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
	if (alg == NULL)
		chr_err("get pd fail\n");
	else {
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

static int mtk_charger_plug_out(struct mtk_charger *info)
{
	struct charger_data *pdata1 = &info->chg_data[CHG1_SETTING];
	struct charger_data *pdata2 = &info->chg_data[CHG2_SETTING];
	struct chg_alg_device *alg;
	struct chg_alg_notify notify;
	int i;

	chr_err("%s\n", __func__);
#if defined (CONFIG_N23_CHARGER_PRIVATE)
	wt_chg_sw_term = 0;
#endif
	info->chr_type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->charger_thread_polling = false;

	pdata1->disable_charging_count = 0;
	pdata1->input_current_limit_by_aicl = -1;
	pdata2->disable_charging_count = 0;

	notify.evt = EVT_PLUG_OUT;
	notify.value = 0;
	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		chg_alg_notifier_call(alg, &notify);
	}

	charger_dev_set_input_current(info->chg1_dev, 100000);
	charger_dev_set_mivr(info->chg1_dev, info->data.min_charger_voltage);
	charger_dev_plug_out(info->chg1_dev);

	return 0;
}

static int mtk_charger_plug_in(struct mtk_charger *info,
				int chr_type)
{
	struct chg_alg_device *alg;
	struct chg_alg_notify notify;
	int i;

	chr_debug("%s\n",
		__func__);

	info->chr_type = chr_type;
	info->charger_thread_polling = true;

	info->can_charging = true;
	//info->enable_dynamic_cv = true;
	info->safety_timeout = false;
	info->vbusov_stat = false;

	chr_err("mtk_is_charger_on plug in, type:%d\n", chr_type);

	notify.evt = EVT_PLUG_IN;
	notify.value = 0;
	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		chg_alg_notifier_call(alg, &notify);
	}
	
#if defined (CONFIG_N26_CHARGER_PRIVATE)
	if(info->chg1_dev == NULL){
		info->chg1_dev = get_charger_by_name("primary_chg");
		if (info->chg1_dev)
			chr_err("Found primary charger\n");
		else {
			chr_err("*** Error : can't find primary charger ***\n");
		}
	}
#endif
	charger_dev_plug_in(info->chg1_dev);

	return 0;
}


static bool mtk_is_charger_on(struct mtk_charger *info)
{
	int chr_type;

	chr_type = get_charger_type(info);
	if (chr_type == POWER_SUPPLY_TYPE_UNKNOWN) {
		if (info->chr_type != POWER_SUPPLY_TYPE_UNKNOWN) {
			mtk_charger_plug_out(info);
			mutex_lock(&info->cable_out_lock);
			info->cable_out_cnt = 0;
			mutex_unlock(&info->cable_out_lock);
		}
	} else {
		if (info->chr_type == POWER_SUPPLY_TYPE_UNKNOWN)
			mtk_charger_plug_in(info, chr_type);
		else
			info->chr_type = chr_type;

		if (info->cable_out_cnt > 0) {
			mtk_charger_plug_out(info);
			mtk_charger_plug_in(info, chr_type);
			mutex_lock(&info->cable_out_lock);
			info->cable_out_cnt = 0;
			mutex_unlock(&info->cable_out_lock);
		}
	}

	if (chr_type == POWER_SUPPLY_TYPE_UNKNOWN)
		return false;

	return true;
}

static void kpoc_power_off_check(struct mtk_charger *info)
{
	unsigned int boot_mode = info->bootmode;
	int vbus = 0;
#if defined (CONFIG_N23_CHARGER_PRIVATE) || defined (CONFIG_N26_CHARGER_PRIVATE)
	return;
#endif

	/* 8 = KERNEL_POWER_OFF_CHARGING_BOOT */
	/* 9 = LOW_POWER_OFF_CHARGING_BOOT */
	if (boot_mode == 8 || boot_mode == 9) {
		vbus = get_vbus(info);
		if (vbus >= 0 && vbus < 2500 && !mtk_is_charger_on(info) && !info->pd_reset) {
			chr_err("Unplug Charger/USB in KPOC mode, vbus=%d, shutdown\n", vbus);
			kernel_power_off();
		}
	}
}

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
#if defined (CONFIG_N26_CHARGER_PRIVATE)
	case POWER_SUPPLY_TYPE_USB_AFC:
		return "afc";
#endif
	default:
		return "unknown";
	}
}

#if defined (CONFIG_N23_CHARGER_PRIVATE)
static void ato_charger_limit_soc(struct mtk_charger *info, int min, int max)
{
	int limit_soc;
	struct power_supply *bat_psy;
	union power_supply_propval val;

	bat_psy = power_supply_get_by_name("battery");
	if (bat_psy == NULL) {
		pr_err("get bat_psy fail\n");
		return;
	}

	power_supply_get_property(bat_psy,
			POWER_SUPPLY_PROP_CAPACITY, &val);
    limit_soc = val.intval;

	pr_err("limit_soc:%d\n",limit_soc);

	if(limit_soc >= max) {
		charger_dev_hz_mode(info->chg1_dev, 1);
		charger_dev_plug_out(info->chg1_dev);
		pr_err("ato_charger_limit_soc:disable charging\n");
	}

    if(limit_soc <= min){
		charger_dev_hz_mode(info->chg1_dev, 0);
		charger_dev_plug_in(info->chg1_dev);
		pr_err("ato_charger_limit_soc:enable charging\n");
	}
}

//+bug 790826,yangchaojun,wt,add batt_full_capacity node
extern int batt_full_capacity;
void wt_batt_full_capacity_check(void)
{
	int uisoc = 0;
	bool is_charger_on = false;
	static struct mtk_charger *pinfo;
	struct power_supply *psy;

	if (pinfo == NULL) {
		psy = power_supply_get_by_name("mtk-master-charger");
		if (psy == NULL) {
			chr_err("%s: psy is not ready\n", __func__);
			return;
		}

		pinfo = (struct mtk_charger *)power_supply_get_drvdata(psy);
		if (pinfo == NULL) {
			chr_err("%s: mtk_charger is null\n", __func__);
			return;
		}
	}
	is_charger_on = mtk_is_charger_on(pinfo);
	uisoc = get_uisoc(pinfo);
	chr_err("wt_batt_full_capacity_check:%d %d %d\n",uisoc,batt_full_capacity,is_charger_on);
	//+bug 790826,yangchaojun,wt,add batt_full_capacity node optimize
	if(batt_full_capacity != 100) {
		if((uisoc >= batt_full_capacity) && (is_charger_on == true)) {
			mtk_chg_status |= 8;
			charger_dev_hz_mode(pinfo->chg1_dev, 1);
			charger_dev_plug_out(pinfo->chg1_dev);
			chr_err("batt_full_capacity:disable charging\n");
		}else if((mtk_chg_status & 8) && (uisoc <= (batt_full_capacity-2)) && (is_charger_on == true)) {
			mtk_chg_status &= ~8;
			if(!mtk_chg_status) {
				charger_dev_hz_mode(pinfo->chg1_dev, 0);
				charger_dev_plug_in(pinfo->chg1_dev);
				chr_err("batt_full_capacity:enable charging\n");
			}
		}
	}else if(mtk_chg_status & 8) {
		mtk_chg_status &= ~8;
		if(!mtk_chg_status) {
			charger_dev_hz_mode(pinfo->chg1_dev, 0);
			charger_dev_plug_in(pinfo->chg1_dev);
			chr_err("batt_full_capacity:echo 100%\n");
		}
	}
	//-bug 790826,yangchaojun,wt,add batt_full_capacity node optimize
}
//-bug 790826,yangchaojun,wt,add batt_full_capacity node


#define SALE_CODE_STR_LEN  3
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

static void batt_slate_mode_control(struct mtk_charger *info)
{
	if (batt_slate_mode) {
		mtk_chg_status |= 2;
		charger_dev_hz_mode(info->chg1_dev, 1);
		charger_dev_plug_out(info->chg1_dev);
		chr_err("batt_slate_mode_control:disable charging\n");
	} else if(mtk_chg_status & 2 && (batt_slate_mode == 0)) {
		mtk_chg_status &= ~2;
		if(!mtk_chg_status) {
			charger_dev_hz_mode(info->chg1_dev, 0);
			charger_dev_plug_in(info->chg1_dev);
			chr_err("batt_slate_mode_control:enable charging\n");
		}
	} else {
		pr_err("%s do no thing\n", __func__);
	}
}
#endif
//+P240305-05569  guhan01.wt 20240318,one ui 6.1 charging protection
#if defined (CONFIG_N26_CHARGER_PRIVATE)
int g_chg_done  = 0;
#endif
//-P240305-05569  guhan01.wt 20240318,one ui 6.1 charging protection
static int charger_routine_thread(void *arg)
{
	struct mtk_charger *info = arg;
	unsigned long flags;
	static bool is_module_init_done;
	bool is_charger_on,chg_done;
	int chg_cur = 0,input_cur = 0;
	struct mtk_battery *gm = get_mtk_battery();
#if defined (CONFIG_N23_CHARGER_PRIVATE)
int mtk_chg_status = 0;
struct power_supply *bat_psy = power_supply_get_by_name("mtk_charger_type");
#ifdef WT_COMPILE_FACTORY_VERSION
	bool is_hz_mode = false;
#endif
#endif

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
		charger_dev_get_charging_current(info->chg1_dev,&chg_cur);
		charger_dev_get_input_current(info->chg1_dev,&input_cur);
		charger_dev_is_charging_done(info->chg1_dev, &chg_done);
//+P240305-05569  guhan01.wt 20240318,one ui 6.1 charging protection
#if defined (CONFIG_N26_CHARGER_PRIVATE)
		g_chg_done = chg_done;
#endif
//-P240305-05569  guhan01.wt 20240318,one ui 6.1 charging protection
		chr_err("Vbat=%d vbus:%d ibus:%d I=%d T=%d uisoc:%d type:%s>%s pd:%d chgset:%d %d chg_done:%d\n",
			get_battery_voltage(info),
			get_vbus(info),
			get_ibus(info),
			get_battery_current(info),
			info->battery_temp,
			get_uisoc(info),
			dump_charger_type(info->chr_type),
			dump_charger_type(get_charger_type(info)),
			info->pd_type,input_cur,chg_cur,chg_done
			);

		is_charger_on = mtk_is_charger_on(info);
		if (IS_ERR_OR_NULL(gm)) {
			gm = get_mtk_battery();
		} else {
			pr_err("gm->bs_data.bat_status=%d\n",gm->bs_data.bat_status);
			if(chg_done && get_uisoc(info) == 100 && gm->bs_data.bat_status != POWER_SUPPLY_STATUS_FULL){
				pr_err("uisoc is 100%, set bat status full!\n");
				charger_dev_do_event(info->chg1_dev, EVENT_FULL, 0);
			}
		}
		

#if defined (CONFIG_N23_CHARGER_PRIVATE)
#ifdef WT_COMPILE_FACTORY_VERSION
	if(is_charger_on) {
			if (get_uisoc(info) >= 80)
				mtk_chg_status |= 4;
			else if (get_uisoc(info) <= 30)
				mtk_chg_status &= ~4;
			ato_charger_limit_soc(info, 60, 80);
	} else {
			charger_dev_is_hz_mode(info->chg1_dev, &is_hz_mode);
			if (is_hz_mode)
				charger_dev_hz_mode(info->chg1_dev, 0);
	}
#endif
	if (is_charger_on && batt_store_mode) {
		if (get_uisoc(info) >= max_soc)
			mtk_chg_status |= 1;
		else if (get_uisoc(info) <= min_soc)
			mtk_chg_status &= ~1;
		ato_charger_limit_soc(info, min_soc, max_soc);
	}
		if (is_charger_on && (batt_store_mode == 0) && (mtk_chg_status & 1)) {
		mtk_chg_status &= ~1;
		if (!mtk_chg_status) {
			charger_dev_hz_mode(info->chg1_dev, 0);
			charger_dev_plug_in(info->chg1_dev);
			pr_err("is_charger_on and batt_store_mode == 0:enable charging\n");
		}
	}
	if (is_charger_on)
		batt_slate_mode_control(info);

	if (gm != NULL) {
			if (is_charger_on) {
				pr_err("%s schedule get ibus\n", __func__);
				schedule_delayed_work(&gm->get_ibus_adc_work, 0);
			} else {
				//none:in this case do nothing
			}
	} else {
			gm = get_mtk_battery();
	}
//+bug 790826,yangchaojun,wt,add batt_full_capacity node
	wt_batt_full_capacity_check();
#endif
		if (info->charger_thread_polling == true)
			mtk_charger_start_timer(info);

		check_battery_exist(info);
		check_dynamic_mivr(info);
		charger_check_status(info);
		kpoc_power_off_check(info);

		if (is_disable_charger(info) == false &&
			is_charger_on == true &&
			info->can_charging == true) {
			if (info->algo.do_algorithm)
				info->algo.do_algorithm(info);
		} else
			chr_debug("disable charging %d %d %d\n",
			is_disable_charger(info),
			is_charger_on,
			info->can_charging);
			charger_dev_dump_registers(info->chg1_dev);
#if defined (CONFIG_N23_CHARGER_PRIVATE)
		if (pre_chg_stat != mtk_chg_status) {
			if(!IS_ERR_OR_NULL(bat_psy)) {
				mdelay(500);
				power_supply_changed(bat_psy);
				pr_err("power_supply_changed\n");
			}
		}
		pre_chg_stat = mtk_chg_status;
#endif
		spin_lock_irqsave(&info->slock, flags);
		__pm_relax(info->charger_wakelock);
		spin_unlock_irqrestore(&info->slock, flags);
		chr_debug("%s end , %d\n",
			__func__, info->charger_thread_timeout);
		mutex_unlock(&info->charger_lock);
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

#if defined (CONFIG_N23_CHARGER_PRIVATE) || defined (CONFIG_N21_CHARGER_PRIVATE)
	ret = device_create_file(&(pdev->dev), &dev_attr_ap_thermal);
	if (ret)
		goto _out;
#endif

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
	default:
		return -EINVAL;
	}
	_wake_up_charger(info);

	return 0;
}

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

	pr_notice("%s event, name:%s online:%d type:%d vbus:%d\n", __func__,
		psy->desc->name, prop.intval, prop2.intval,
		get_vbus(info));

	mtk_is_charger_on(info);
	_wake_up_charger(info);
}

int notify_adapter_event(struct notifier_block *notifier,
			unsigned long evt, void *val)
{
	struct mtk_charger *pinfo = NULL;

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
	return NOTIFY_DONE;
}

int chg_alg_event(struct notifier_block *notifier,
			unsigned long event, void *data)
{
	chr_err("%s: evt:%d\n", __func__, event);

	return NOTIFY_DONE;
}

//+bug 682591,caoyachun.wt,mod,20220318,charge current limit for AP overheat
#if defined (CONFIG_N23_CHARGER_PRIVATE) || defined (CONFIG_N21_CHARGER_PRIVATE)
static void lcmoff_switch(int onoff)
{
	struct mtk_charger *pinfo = NULL;
	struct power_supply *psy;
	psy = power_supply_get_by_name("mtk-master-charger");
	if (psy == NULL || IS_ERR(psy)) {
		pr_notice("%s Couldn't get psy\n", __func__);
	} else {
		pinfo = (struct mtk_charger *)power_supply_get_drvdata(psy);
	}
	chr_err("%s: onoff = %d\n", __func__, onoff);

	if(pinfo == NULL)
		return;

	/* onoff = 0: LCM OFF */
	/* others: LCM ON */
	if (onoff) {
		/* deactivate lcmoff policy */
		pinfo->lcmoff = 0;
	} else {
		/* activate lcmoff policy */
		pinfo->lcmoff = 1;
	}
}

static int lcmoff_fb_notifier_callback(
struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int blank;

	/* skip if it's not a blank event */
	if ((event != FB_EVENT_BLANK) || (data == NULL))
		return 0;

	blank = *(int *)evdata->data;

	switch (blank) {
	/* LCM ON */
	case FB_BLANK_UNBLANK:
		lcmoff_switch(1);
		break;
		/* LCM OFF */
	case FB_BLANK_POWERDOWN:
		lcmoff_switch(0);
		break;
	default:
		break;
	}

	return 0;
}

static struct notifier_block lcmoff_fb_notifier = {
	.notifier_call = lcmoff_fb_notifier_callback,
};
//-bug 682591,caoyachun.wt,mod,20220318,charge current limit for AP overheat
int charger_manager_disable_charging_new(struct mtk_charger *info, bool en)
{
	/* cmd disable charging */
	if (en) {
		charger_dev_hz_mode(info->chg1_dev, 1);
		_mtk_enable_charging(info, 0);
		info->cmd_discharging = 1;
		chr_err("charger_manager_enable_charging_new:disable charging\n");
		return 1;
	} else {
		charger_dev_hz_mode(info->chg1_dev, 0);
		_mtk_enable_charging(info, 1);
		info->cmd_discharging = 0;
		chr_err("charger_manager_enable_charging_new:enable charging\n");
		return 0;
	}

	return 0;
}
#endif

static int mtk_charger_probe(struct platform_device *pdev)
{
	struct mtk_charger *info = NULL;
	int i;
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
#if defined (CONFIG_N26_CHARGER_PRIVATE)
	info->sw_jeita.sm = TEMP_T2_TO_T3;
#endif

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
#if defined (CONFIG_N26_CHARGER_PRIVATE)
	else{
		info->wtchg_info = (struct wtchg_info *)power_supply_get_drvdata(info->chg_psy);
		info->wtchg_info->mtk_charger = info;
	}
#endif
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

	info->chg_alg_nb.notifier_call = chg_alg_event;

//+bug 682591,caoyachun.wt,mod,20220318,charge current limit for AP overheat
#if defined (CONFIG_N23_CHARGER_PRIVATE) || defined (CONFIG_N21_CHARGER_PRIVATE)
	info->ap_temp = 0xffff;
	info->lcmoff = true;  //temporarily force lcd off status
	if(fb_register_client(&lcmoff_fb_notifier))
		chr_err("fb_register_client failed\n");
#endif
//-bug 682591,caoyachun.wt,mod,20220318,charge current limit for AP overheat
	kthread_run(charger_routine_thread, info, "charger_thread");
#if defined (CONFIG_N23_CHARGER_PRIVATE)
	fast_charger_connect(info);
	if (sales_code_is("VZW")) {
		min_soc = 30;
		max_soc = 35;
	} else {
		min_soc = 60;
		max_soc = 70;
	}
#endif

	return 0;
}

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
