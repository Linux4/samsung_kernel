// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 *
 * Filename:
 * ---------
 *    mtk_basic_charger.c
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

#include "mtk_charger.h"

static int _uA_to_mA(int uA)
{
	if (uA == -1)
		return -1;
	else
		return uA / 1000;
}

/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 start*/
#ifndef HQ_FACTORY_BUILD	//ss version
static int get_val(struct range_data *range, int threshold, int *val)
{
	int i;

	/* First try to find the matching index without hysteresis */
	for (i = 0; i < MAX_CV_ENTRIES; i++) {
		if (!range[i].high_threshold && !range[i].low_threshold) {
			/* First invalid table entry; exit loop */
			break;
		}

		if (is_between(range[i].low_threshold,
			range[i].high_threshold, threshold)) {
			*val = range[i].value;
			break;
		}
	}

	/*
	 * If the threshold is lesser than the minimum allowed range,
	 * return minimum. So is the maximum.
	 */
	if (threshold > 9999) {
		*val = range[4].value;
	}
	if (threshold < 0) {
		*val = range[0].value;
	}

	return 0;
}

static int ss_battery_aging_update(struct mtk_charger *info)
{
	int rc, cycle_count = 0, vbat = 0;
	union power_supply_propval prop = {0, };
	static int battery_cv_old = 0;

	cycle_count = info->data.ss_batt_cycle;

	rc = get_val(info->data.batt_cv_data, cycle_count, &vbat);
	if (rc < 0) {
		chr_err("Failed to get batt_cv_data, rc=%d\n", rc);
		return -ENODEV;
	}

	if (battery_cv_old == vbat) {
		return 0;
        }
	prop.intval = vbat;

	if (info->sw_jeita.cv != 0)
		prop.intval = min(prop.intval, info->sw_jeita.cv);

	info->data.battery_cv = prop.intval;
	battery_cv_old = info->data.battery_cv;
	chr_info("%s cycle_count=%d, battery_cv=%d\n", __func__,
		cycle_count, info->data.battery_cv);

	return 0;
}
#endif
/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 end*/

static void select_cv(struct mtk_charger *info)
{
	u32 constant_voltage;

	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	if (info->data.ss_batt_aging_enable)
		ss_battery_aging_update(info);
	#endif
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 end*/

	if (info->enable_sw_jeita)
		if (info->sw_jeita.cv != 0) {
			/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 start*/
			#ifndef HQ_FACTORY_BUILD	//ss version
			if (info->data.ss_batt_aging_enable)
				info->setting.cv = min(info->data.battery_cv, info->sw_jeita.cv);
			else
				info->setting.cv = info->sw_jeita.cv;
			#else
			info->setting.cv = info->sw_jeita.cv;
			#endif
			/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 end*/
			return;
		}

	constant_voltage = info->data.battery_cv;
	info->setting.cv = constant_voltage;
}

static bool is_typec_adapter(struct mtk_charger *info)
{
	int rp;

	rp = adapter_dev_get_property(info->pd_adapter, TYPEC_RP_LEVEL);
	if (info->pd_type == MTK_PD_CONNECT_TYPEC_ONLY_SNK &&
			/*HS03s for P210628-01843 by wenyaqi at 20210629 start*/
			rp != 500 && rp != 0 &&
			/*HS03s for P210628-01843 by wenyaqi at 20210629 end*/
			info->chr_type != POWER_SUPPLY_TYPE_USB &&
			info->chr_type != POWER_SUPPLY_TYPE_USB_CDP)
		return true;

	return false;
}

static bool support_fast_charging(struct mtk_charger *info)
{
	struct chg_alg_device *alg;
	int i = 0, state = 0;
	bool ret = false;

	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		if (alg == NULL)
			continue;

		chg_alg_set_current_limit(alg, &info->setting);
		state = chg_alg_is_algo_ready(alg);
		//chr_debug("%s %s ret:%s\n", __func__, dev_name(&alg->dev),
		//	chg_alg_state_to_str(state));

		if (state == ALG_READY || state == ALG_RUNNING) {
			ret = true;
			break;
		}
	}
	return ret;
}

static bool select_charging_current_limit(struct mtk_charger *info,
	struct chg_limit_setting *setting)
{
	struct charger_data *pdata, *pdata2;
	bool is_basic = false;
	u32 ichg1_min = 0, aicr1_min = 0;
	int ret;

	select_cv(info);

	pdata = &info->chg_data[CHG1_SETTING];
	pdata2 = &info->chg_data[CHG2_SETTING];
	if (info->usb_unlimited) {
		pdata->input_current_limit =
					info->data.ac_charger_input_current;
		pdata->charging_current_limit =
					info->data.ac_charger_current;
		is_basic = true;
		goto done;
	}

	if (info->water_detected) {
		pdata->input_current_limit = info->data.usb_charger_current;
		pdata->charging_current_limit = info->data.usb_charger_current;
		is_basic = true;
		goto done;
	}

	if ((info->bootmode == 1) ||
	    (info->bootmode == 5)) {
		pdata->input_current_limit = 200000; /* 200mA */
		is_basic = true;
		goto done;
	}

	if (info->atm_enabled == true
		&& (info->chr_type == POWER_SUPPLY_TYPE_USB ||
		info->chr_type == POWER_SUPPLY_TYPE_USB_CDP)
		) {
		pdata->input_current_limit = 100000; /* 100mA */
		is_basic = true;
		goto done;
	}

	if (info->chr_type == POWER_SUPPLY_TYPE_USB) {
		pdata->input_current_limit =
				info->data.usb_charger_current;
		/* it can be larger */
		pdata->charging_current_limit =
				info->data.usb_charger_current;
		is_basic = true;
	} else if (info->chr_type == POWER_SUPPLY_TYPE_USB_CDP) {
		pdata->input_current_limit =
			info->data.charging_host_charger_current;
		pdata->charging_current_limit =
			info->data.charging_host_charger_current;
		is_basic = true;

	} else if (info->chr_type == POWER_SUPPLY_TYPE_USB_DCP) {
		/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
		#ifdef CONFIG_AFC_CHARGER
		if(info->afc_sts >= AFC_9V) {
			pdata->input_current_limit =
				info->data.afc_charger_input_current;
			pdata->charging_current_limit =
				info->data.afc_charger_current;
		} else {
			pdata->input_current_limit =
				info->data.ac_charger_input_current;
			pdata->charging_current_limit =
				info->data.ac_charger_current;
		}
		#else
		pdata->input_current_limit =
			info->data.ac_charger_input_current;
		pdata->charging_current_limit =
			info->data.ac_charger_current;
		#endif
		/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/
		if (info->config == DUAL_CHARGERS_IN_SERIES) {
			pdata2->input_current_limit =
				pdata->input_current_limit;
			pdata2->charging_current_limit = 2000000;
		}
	} else if (info->chr_type == POWER_SUPPLY_TYPE_USB_FLOAT) {
		/* NONSTANDARD_CHARGER */
		pdata->input_current_limit =
			info->data.usb_charger_current;
		pdata->charging_current_limit =
			info->data.usb_charger_current;
		is_basic = true;
	}

	if (support_fast_charging(info))
		is_basic = false;
	else {
		is_basic = true;
		/* AICL */
		charger_dev_run_aicl(info->chg1_dev,
			&pdata->input_current_limit_by_aicl);
		if (info->enable_dynamic_mivr) {
			if (pdata->input_current_limit_by_aicl >
				info->data.max_dmivr_charger_current)
				pdata->input_current_limit_by_aicl =
					info->data.max_dmivr_charger_current;
		}
		if (is_typec_adapter(info)) {
			if (adapter_dev_get_property(info->pd_adapter, TYPEC_RP_LEVEL)
				== 3000) {
				pdata->input_current_limit = 3000000;
				pdata->charging_current_limit = 3000000;
			} else if (adapter_dev_get_property(info->pd_adapter,
				TYPEC_RP_LEVEL) == 1500) {
				/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
				#ifdef CONFIG_AFC_CHARGER
				if(info->afc_sts >= AFC_9V) {
					pdata->input_current_limit =
						min(pdata->input_current_limit, info->data.afc_charger_input_current);
					pdata->charging_current_limit =
						min(pdata->charging_current_limit, info->data.afc_charger_current);
				} else {
					pdata->input_current_limit = min(pdata->input_current_limit, 1500000);
					pdata->charging_current_limit = min(pdata->input_current_limit, 2000000);
				}
				#else
				pdata->input_current_limit = 1500000;
				pdata->charging_current_limit = 2000000;
				#endif
				/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/
			} else {
				chr_err("type-C: inquire rp error\n");
				pdata->input_current_limit = 500000;
				pdata->charging_current_limit = 500000;
			}

			chr_err("type-C:%d current:%d\n",
				info->pd_type,
				adapter_dev_get_property(info->pd_adapter,
					TYPEC_RP_LEVEL));
		}
	}

	if (info->enable_sw_jeita) {
		if (IS_ENABLED(CONFIG_USBIF_COMPLIANCE)
			&& info->chr_type == POWER_SUPPLY_TYPE_USB)
			chr_debug("USBIF & STAND_HOST skip current check\n");
		else {
			/*HS03s for SR-AL5625-01-261 by wenyaqi at 20210425 start*/
			/*
			if (info->sw_jeita.sm == TEMP_T0_TO_T1) {
				pdata->input_current_limit = 500000;
				pdata->charging_current_limit = 350000;
			}
			*/
			/* set current after temperature changed */
			switch (info->sw_jeita.sm) {
			case TEMP_ABOVE_T4:
				pdata->charging_current_limit =
					min(pdata->charging_current_limit, info->data.jeita_temp_above_t4_cur);
				break;
			case TEMP_T3_TO_T4:
				pdata->charging_current_limit =
					min(pdata->charging_current_limit, info->data.jeita_temp_t3_to_t4_cur);
				break;
			case TEMP_T2_TO_T3:
				/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
				#ifdef CONFIG_AFC_CHARGER
				if(info->afc_sts >= AFC_9V)
					pdata->charging_current_limit =
						min(pdata->charging_current_limit, info->data.afc_charger_current);
				else
					pdata->charging_current_limit =
						min(pdata->charging_current_limit, info->data.jeita_temp_t2_to_t3_cur);
				break;
				#else
				pdata->charging_current_limit =
					min(pdata->charging_current_limit, info->data.jeita_temp_t2_to_t3_cur);
				break;
				#endif
				/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/
			case TEMP_T1_TO_T2:
				pdata->charging_current_limit =
					min(pdata->charging_current_limit, info->data.jeita_temp_t1_to_t2_cur);
				break;
			case TEMP_T0_TO_T1:
				pdata->charging_current_limit =
					min(pdata->charging_current_limit, info->data.jeita_temp_t0_to_t1_cur);
				break;
			case TEMP_BELOW_T0:
				pdata->charging_current_limit =
					min(pdata->charging_current_limit, info->data.jeita_temp_below_t0_cur);
				break;
			default:
				break;
			}
			chr_err("[%s] sw_jeita->sm:%d charging_current_limit:%d\n",
				__func__, info->sw_jeita.sm, _uA_to_mA(pdata->charging_current_limit));
			/*HS03s for SR-AL5625-01-261 by wenyaqi at 20210425 end*/
		}
	}

	if (pdata->thermal_charging_current_limit != -1) {
		if (pdata->thermal_charging_current_limit <
			pdata->charging_current_limit) {
			pdata->charging_current_limit =
					pdata->thermal_charging_current_limit;
			info->setting.charging_current_limit1 =
					pdata->thermal_charging_current_limit;
		}
	} else
		info->setting.charging_current_limit1 = -1;

	if (pdata->thermal_input_current_limit != -1) {
		if (pdata->thermal_input_current_limit <
			pdata->input_current_limit) {
			pdata->input_current_limit =
					pdata->thermal_input_current_limit;
			info->setting.input_current_limit1 =
					pdata->input_current_limit;
		}
	} else
		info->setting.input_current_limit1 = -1;

	if (pdata2->thermal_charging_current_limit != -1) {
		if (pdata2->thermal_charging_current_limit <
			pdata2->charging_current_limit) {
			pdata2->charging_current_limit =
					pdata2->thermal_charging_current_limit;
			info->setting.charging_current_limit2 =
					pdata2->charging_current_limit;
		}
	} else
		info->setting.charging_current_limit2 = -1;

	if (pdata2->thermal_input_current_limit != -1) {
		if (pdata2->thermal_input_current_limit <
			pdata2->input_current_limit) {
			pdata2->input_current_limit =
					pdata2->thermal_input_current_limit;
			info->setting.input_current_limit2 =
					pdata2->input_current_limit;
		}
	} else
		info->setting.input_current_limit2 = -1;

	if (info->setting.input_current_limit1 == -1 &&
		info->setting.input_current_limit2 == -1 &&
		info->setting.charging_current_limit1 == -1 &&
		info->setting.charging_current_limit2 == -1)
		info->enable_hv_charging = true;

	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
	/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 start */
	#ifdef CONFIG_AFC_CHARGER
	if (support_fast_charging(info) && info->afc_sts < AFC_9V) {
	#else
	if (support_fast_charging(info)) {
	#endif
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/
#ifdef CONFIG_HQ_PROJECT_HS04
		is_basic = true;
#else
		is_basic = false;
#endif
	} else {
	/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 end */
		is_basic = true;
		/* AICL */
		charger_dev_run_aicl(info->chg1_dev,
			&pdata->input_current_limit_by_aicl);
		if (info->enable_dynamic_mivr) {
			if (pdata->input_current_limit_by_aicl >
				info->data.max_dmivr_charger_current)
				pdata->input_current_limit_by_aicl =
					info->data.max_dmivr_charger_current;
		}
		if (is_typec_adapter(info)) {
			if (adapter_dev_get_property(info->pd_adapter, TYPEC_RP_LEVEL)
				== 3000) {
				pdata->input_current_limit = 3000000;
				pdata->charging_current_limit = 3000000;
			} else if (adapter_dev_get_property(info->pd_adapter,
				TYPEC_RP_LEVEL) == 1500) {
				/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
				#ifdef CONFIG_AFC_CHARGER
				if(info->afc_sts >= AFC_9V) {
					pdata->input_current_limit =
						min(pdata->input_current_limit, info->data.afc_charger_input_current);
					pdata->charging_current_limit =
						min(pdata->charging_current_limit, info->data.afc_charger_current);
				} else {
					pdata->input_current_limit = min(pdata->input_current_limit, 1500000);
					pdata->charging_current_limit = min(pdata->input_current_limit, 2000000);
				}
				#else
				pdata->input_current_limit = 1500000;
				pdata->charging_current_limit = 2000000;
				#endif
				/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/
			} else {
				chr_err("type-C: inquire rp error\n");
				pdata->input_current_limit = 500000;
				pdata->charging_current_limit = 500000;
			}

			chr_err("type-C:%d current:%d\n",
				info->pd_type,
				adapter_dev_get_property(info->pd_adapter,
					TYPEC_RP_LEVEL));
		}
	}

	if (is_basic == true && pdata->input_current_limit_by_aicl != -1) {
		if (pdata->input_current_limit_by_aicl <
		    pdata->input_current_limit)
			pdata->input_current_limit =
					pdata->input_current_limit_by_aicl;
	}
done:
#ifdef CONFIG_HQ_PROJECT_HS03S
    /* modify code for O6 */
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
	#ifdef CONFIG_AFC_CHARGER
	ret = charger_dev_get_min_charging_current(info->chg1_dev, &ichg1_min);
	if (ret != -ENOTSUPP && pdata->charging_current_limit < ichg1_min) {
		if(info->afc.is_connect == true) {
			chr_err("ignore min_charging_current compare when detect afc\n");
		} else {
			pdata->charging_current_limit = 0;
			chr_err("min_charging_current is too low %d %d\n",
				pdata->charging_current_limit, ichg1_min);
			is_basic = true;
			info->enable_hv_charging = false;
		}
	}

	ret = charger_dev_get_min_input_current(info->chg1_dev, &aicr1_min);
	if (ret != -ENOTSUPP && pdata->input_current_limit < aicr1_min) {
		if(info->afc.is_connect == true) {
			chr_err("ignore min_input_current compare when detect afc\n");
		} else {
			pdata->input_current_limit = 0;
			chr_err("min_input_current is too low %d %d\n",
				pdata->input_current_limit, aicr1_min);
			is_basic = true;
			info->enable_hv_charging = false;
		}
	}
	#else
	ret = charger_dev_get_min_charging_current(info->chg1_dev, &ichg1_min);
	if (ret != -ENOTSUPP && pdata->charging_current_limit < ichg1_min) {
		pdata->charging_current_limit = 0;
		chr_err("min_charging_current is too low %d %d\n",
			pdata->charging_current_limit, ichg1_min);
		is_basic = true;
	}

	ret = charger_dev_get_min_input_current(info->chg1_dev, &aicr1_min);
	if (ret != -ENOTSUPP && pdata->input_current_limit < aicr1_min) {
		pdata->input_current_limit = 0;
		chr_err("min_input_current is too low %d %d\n",
			pdata->input_current_limit, aicr1_min);
		is_basic = true;
	}
	#endif
/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	if (info->batt_store_mode) {
		pdata->charging_current_limit = min(pdata->input_current_limit,
			info->data.usb_charger_current);
		chr_err("store_mode running, set ichg to %dmA\n", _uA_to_mA(pdata->charging_current_limit));
	}
	#endif
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
    /* modify code for O6 */
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
	#ifdef CONFIG_AFC_CHARGER
	ret = charger_dev_get_min_charging_current(info->chg1_dev, &ichg1_min);
	if (ret != -ENOTSUPP && pdata->charging_current_limit < ichg1_min) {
		if(info->afc.is_connect == true) {
			chr_err("ignore min_charging_current compare when detect afc\n");
		} else {
			pdata->charging_current_limit = 0;
			chr_err("min_charging_current is too low %d %d\n",
				pdata->charging_current_limit, ichg1_min);
			is_basic = true;
			info->enable_hv_charging = false;
		}
	}

	ret = charger_dev_get_min_input_current(info->chg1_dev, &aicr1_min);
	if (ret != -ENOTSUPP && pdata->input_current_limit < aicr1_min) {
		if(info->afc.is_connect == true) {
			chr_err("ignore min_input_current compare when detect afc\n");
		} else {
			pdata->input_current_limit = 0;
			chr_err("min_input_current is too low %d %d\n",
				pdata->input_current_limit, aicr1_min);
			is_basic = true;
			info->enable_hv_charging = false;
		}
	}
	#else
	ret = charger_dev_get_min_charging_current(info->chg1_dev, &ichg1_min);
	if (ret != -ENOTSUPP && pdata->charging_current_limit < ichg1_min) {
		pdata->charging_current_limit = 0;
		chr_err("min_charging_current is too low %d %d\n",
			pdata->charging_current_limit, ichg1_min);
		is_basic = true;
	}

	ret = charger_dev_get_min_input_current(info->chg1_dev, &aicr1_min);
	if (ret != -ENOTSUPP && pdata->input_current_limit < aicr1_min) {
		pdata->input_current_limit = 0;
		chr_err("min_input_current is too low %d %d\n",
			pdata->input_current_limit, aicr1_min);
		is_basic = true;
	}
	#endif
/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	if (info->batt_store_mode) {
		pdata->charging_current_limit = min(pdata->input_current_limit,
			info->data.usb_charger_current);
		chr_err("store_mode running, set ichg to %dmA\n", _uA_to_mA(pdata->charging_current_limit));
	}
	#endif
#endif
#ifdef CONFIG_HQ_PROJECT_OT8
	/* modify code for O8 */
	/*TabA7 Lite code for OT8-106 add afc charger driver by wenyaqi at 20201210 start*/
	#ifdef CONFIG_AFC_CHARGER
	ret = charger_dev_get_min_charging_current(info->chg1_dev, &ichg1_min);
	if (ret != -ENOTSUPP && pdata->charging_current_limit < ichg1_min) {
		if(info->afc.is_connect == true) {
			chr_err("ignore min_charging_current compare when detect afc\n");
		} else {
			pdata->charging_current_limit = 0;
			chr_err("min_charging_current is too low %d %d\n",
				pdata->charging_current_limit, ichg1_min);
			is_basic = true;
			info->enable_hv_charging = false;
		}
	}

	ret = charger_dev_get_min_input_current(info->chg1_dev, &aicr1_min);
	if (ret != -ENOTSUPP && pdata->input_current_limit < aicr1_min) {
		if(info->afc.is_connect == true) {
			chr_err("ignore min_input_current compare when detect afc\n");
		} else {
			pdata->input_current_limit = 0;
			chr_err("min_input_current is too low %d %d\n",
				pdata->input_current_limit, aicr1_min);
			is_basic = true;
			info->enable_hv_charging = false;
		}
	}
	#else
	ret = charger_dev_get_min_charging_current(info->chg1_dev, &ichg1_min);
	if (ret != -ENOTSUPP && pdata->charging_current_limit < ichg1_min) {
		pdata->charging_current_limit = 0;
		chr_err("min_charging_current is too low %d %d\n",
			pdata->charging_current_limit, ichg1_min);
		is_basic = true;
		info->enable_hv_charging = false;
	}

	ret = charger_dev_get_min_input_current(info->chg1_dev, &aicr1_min);
	if (ret != -ENOTSUPP && pdata->input_current_limit < aicr1_min) {
		pdata->input_current_limit = 0;
		chr_err("min_input_current is too low %d %d\n",
			pdata->input_current_limit, aicr1_min);
		is_basic = true;
		info->enable_hv_charging = false;
	}
	#endif
	#ifndef HQ_FACTORY_BUILD	//ss version
	if (info->batt_store_mode) {
		pdata->charging_current_limit = min(pdata->input_current_limit,
			info->data.usb_charger_current);
		chr_err("store_mode running, set ichg to %dmA\n", _uA_to_mA(pdata->charging_current_limit));
	}
	#endif
#endif
	/*TabA7 Lite code for OT8-106 add afc charger driver by wenyaqi at 20201210 end*/
/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 end*/
/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/
	chr_err("m:%d chg1:%d,%d,%d,%d chg2:%d,%d,%d,%d type:%d:%d usb_unlimited:%d usbif:%d usbsm:%d aicl:%d atm:%d bm:%d b:%d\n",
		info->config,
		_uA_to_mA(pdata->thermal_input_current_limit),
		_uA_to_mA(pdata->thermal_charging_current_limit),
		_uA_to_mA(pdata->input_current_limit),
		_uA_to_mA(pdata->charging_current_limit),
		_uA_to_mA(pdata2->thermal_input_current_limit),
		_uA_to_mA(pdata2->thermal_charging_current_limit),
		_uA_to_mA(pdata2->input_current_limit),
		_uA_to_mA(pdata2->charging_current_limit),
		info->chr_type, info->pd_type,
		info->usb_unlimited,
		IS_ENABLED(CONFIG_USBIF_COMPLIANCE), info->usb_state,
		pdata->input_current_limit_by_aicl, info->atm_enabled,
		info->bootmode, is_basic);

	return is_basic;
}
#if 0
    /* modify code for O6 */
static int do_algorithm(struct mtk_charger *info)
{
	struct chg_alg_device *alg;
	struct charger_data *pdata;
	struct chg_alg_notify notify;
	bool is_basic = true;
	bool chg_done = false;
	int i;
	int ret;
	int val = 0;

	pdata = &info->chg_data[CHG1_SETTING];
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
	charger_dev_is_charging_done(info->chg1_dev, &chg_done);
#ifdef CONFIG_HQ_PROJECT_OT8
	/* modify code for O8 */
	/*TabA7 Lite code for OT8-106 add afc charger driver by wenyaqi at 20201210 start*/
	#ifdef CONFIG_AFC_CHARGER
	if (info->chr_type == POWER_SUPPLY_TYPE_USB_DCP &&
		info->pd_type != MTK_PD_CONNECT_PE_READY_SNK &&      //PD2.0
		info->pd_type != MTK_PD_CONNECT_PE_READY_SNK_PD30 && //PD3.0
		info->pd_type != MTK_PD_CONNECT_PE_READY_SNK_APDO) { //PPS
		afc_check_charger(info);
		afc_start_algorithm(info);
	}
	#endif
	/*TabA7 Lite code for OT8-106 add afc charger driver by wenyaqi at 20201210 end*/
#endif
	#ifdef CONFIG_AFC_CHARGER
	afc_check_charger(info);
	#endif
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/
	is_basic = select_charging_current_limit(info, &info->setting);

	if (info->is_chg_done != chg_done) {
		if (chg_done) {
			charger_dev_do_event(info->chg1_dev, EVENT_FULL, 0);
			chr_err("%s battery full\n", __func__);
		} else {
			charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
			chr_err("%s battery recharge\n", __func__);
		}
	}

	chr_err("%s is_basic:%d\n", __func__, is_basic);
	if (is_basic != true) {
		is_basic = true;
		for (i = 0; i < MAX_ALG_NO; i++) {
			alg = info->alg[i];
			if (alg == NULL)
				continue;

			if (!info->enable_hv_charging ||
			    pdata->charging_current_limit == 0 ||
			    pdata->input_current_limit == 0) {
				chg_alg_get_prop(alg, ALG_MAX_VBUS, &val);
				if (val > 5000)
					chg_alg_stop_algo(alg);
				chr_err("%s: alg:%s alg_vbus:%d\n", __func__,
					dev_name(&alg->dev), val);
				continue;
			}

			if (chg_done != info->is_chg_done) {
				if (chg_done) {
					notify.evt = EVT_FULL;
					notify.value = 0;
				} else {
					notify.evt = EVT_RECHARGE;
					notify.value = 0;
				}
				chg_alg_notifier_call(alg, &notify);
				chr_err("%s notify:%d\n", __func__, notify.evt);
			}

			chg_alg_set_current_limit(alg, &info->setting);
			ret = chg_alg_is_algo_ready(alg);

			chr_err("%s %s ret:%s\n", __func__,
				dev_name(&alg->dev),
				chg_alg_state_to_str(ret));

			if (ret == ALG_INIT_FAIL || ret == ALG_TA_NOT_SUPPORT) {
				/* try next algorithm */
				continue;
			} else if (ret == ALG_TA_CHECKING || ret == ALG_DONE ||
						ret == ALG_NOT_READY) {
				/* wait checking , use basic first */
				is_basic = true;
				break;
			} else if (ret == ALG_READY || ret == ALG_RUNNING) {
				is_basic = false;
				//chg_alg_set_setting(alg, &info->setting);
				chg_alg_start_algo(alg);
				break;
			} else {
				chr_err("algorithm ret is error");
				is_basic = true;
			}
		}
	} else {
		if (info->enable_hv_charging != true ||
		    pdata->charging_current_limit == 0 ||
		    pdata->input_current_limit == 0) {
			for (i = 0; i < MAX_ALG_NO; i++) {
				alg = info->alg[i];
				if (alg == NULL)
					continue;

				chg_alg_get_prop(alg, ALG_MAX_VBUS, &val);
				if (val > 5000 && chg_alg_is_algo_running(alg))
					chg_alg_stop_algo(alg);

				chr_err("%s: Stop hv charging. en_hv:%d alg:%s alg_vbus:%d\n",
					__func__, info->enable_hv_charging,
					dev_name(&alg->dev), val);
			}
		}
	}
	info->is_chg_done = chg_done;

	if (is_basic == true) {
#ifdef CONFIG_HQ_PROJECT_OT8
/* modify code for O8 */
	/*TabA7 Lite  code for SR-AX3565-01-107 by gaoxugang at 20201124 start*/
		#if !defined(HQ_FACTORY_BUILD)
		if (info->store_mode_of_fcc)
			pdata->charging_current_limit = 500000;
		#endif
		/*TabA7 Lite  code for SR-AX3565-01-107 by gaoxugang at 20201124 end*/
#endif
		charger_dev_set_input_current(info->chg1_dev,
			pdata->input_current_limit);
		charger_dev_set_charging_current(info->chg1_dev,
			pdata->charging_current_limit);
		charger_dev_set_constant_voltage(info->chg1_dev,
			info->setting.cv);
	}

	if (pdata->input_current_limit == 0 ||
	    pdata->charging_current_limit == 0)
		charger_dev_enable(info->chg1_dev, false);
	else
		charger_dev_enable(info->chg1_dev, true);

	if (info->chg1_dev != NULL)
		charger_dev_dump_registers(info->chg1_dev);

	if (info->chg2_dev != NULL)
		charger_dev_dump_registers(info->chg2_dev);

	return 0;
}
#endif
    /* modify code for OT8 */
static int do_algorithm(struct mtk_charger *info)
{
	struct chg_alg_device *alg;
	struct charger_data *pdata;
	struct chg_alg_notify notify;
	bool is_basic = true;
	bool chg_done = false;
	int i;
	int ret;
	int val = 0;

	pdata = &info->chg_data[CHG1_SETTING];
	/*TabA7 Lite code for OT8-106 add afc charger driver by wenyaqi at 20201210 start*/
	/*TabA7 Lite code for P201230-05004 optimize afc driver by wenyaqi at 20210114 start*/
	/*TabA7 Lite code for OT8-2091 re_detect afc when vbus drop by wenyaqi at 20210126 start*/
	/*TabA7 Lite code for P210209-04223 fix HV_CHARGER_STATUS by wenyaqi at 20210212 start*/
	#ifdef CONFIG_AFC_CHARGER
	if (info->hv_disable == HV_ENABLE &&
		info->chr_type == POWER_SUPPLY_TYPE_USB_DCP) {
		afc_check_charger(info);
		if(info->afc_sts < AFC_5V && info->afc.afc_retry_flag == true &&
			info->afc.afc_loop_algo < ALGO_LOOP_MAX) {
			chr_err("afc failed %d times,try again\n", info->afc.afc_loop_algo++);
			info->afc.to_check_chr_type = true;
			info->afc.is_connect = true;
		}
		if(info->afc_sts > AFC_5V && get_vbus(info) < 8000) {
			chr_err("vbus droped,try afc again\n");
			info->afc.to_check_chr_type = true;
			info->afc.is_connect = true;
		}
	} else {
		chr_err("%s: Do not start AFC, hv_disable(%d)\n",
					__func__, info->hv_disable);
	}
	#endif
	/*TabA7 Lite code for P210209-04223 fix HV_CHARGER_STATUS by wenyaqi at 20210212 end*/
	/*TabA7 Lite code for OT8-2091 re_detect afc when vbus drop by wenyaqi at 20210126 end*/
	/*TabA7 Lite code for P201230-05004 optimize afc driver by wenyaqi at 20210114 end*/
	/*TabA7 Lite code for OT8-106 add afc charger driver by wenyaqi at 20201210 end*/
	charger_dev_is_charging_done(info->chg1_dev, &chg_done);
	is_basic = select_charging_current_limit(info, &info->setting);
	chr_err("%s is_chg_done:%d chg_done:%d \n", __func__, info->is_chg_done, chg_done);
	if (info->is_chg_done != chg_done) {
		if (chg_done) {
			charger_dev_do_event(info->chg1_dev, EVENT_FULL, 0);
			chr_err("%s battery full\n", __func__);
		} else {
			charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
			chr_err("%s battery recharge\n", __func__);
		}
	}
/* TabA7 Lite code for P210524-06376 by gaochao at 20210604 start */
	if (chg_done) {
		ret = get_uisoc(info);
		if(ret < 100)
			charger_dev_do_event(info->chg1_dev, EVENT_FULL, 0);
	}
/* TabA7 Lite code for P210524-06376 by gaochao at 20210604 end */
	chr_err("%s is_basic:%d\n", __func__, is_basic);
	if (is_basic != true) {
		is_basic = true;
		for (i = 0; i < MAX_ALG_NO; i++) {
			alg = info->alg[i];
			if (alg == NULL)
				continue;

			if (!info->enable_hv_charging) {
				chg_alg_get_prop(alg, ALG_MAX_VBUS, &val);
				if (val > 5000)
					chg_alg_stop_algo(alg);
				chr_err("%s: alg:%s alg_vbus:%d\n", __func__,
					dev_name(&alg->dev), val);
				continue;
			}

			if (chg_done != info->is_chg_done) {
				if (chg_done) {
					notify.evt = EVT_FULL;
					notify.value = 0;
				} else {
					notify.evt = EVT_RECHARGE;
					notify.value = 0;
				}
				chg_alg_notifier_call(alg, &notify);
				chr_err("%s notify:%d\n", __func__, notify.evt);
			}

			chg_alg_set_current_limit(alg, &info->setting);
			ret = chg_alg_is_algo_ready(alg);

			chr_err("%s %s ret:%s\n", __func__,
				dev_name(&alg->dev),
				chg_alg_state_to_str(ret));

			if (ret == ALG_INIT_FAIL || ret == ALG_TA_NOT_SUPPORT) {
				/* try next algorithm */
				continue;
			} else if (ret == ALG_TA_CHECKING || ret == ALG_DONE ||
						ret == ALG_NOT_READY) {
				/* wait checking , use basic first */
				is_basic = true;
				break;
			} else if (ret == ALG_READY || ret == ALG_RUNNING) {
				is_basic = false;
				//chg_alg_set_setting(alg, &info->setting);
				chg_alg_start_algo(alg);
				break;
			} else {
				chr_err("algorithm ret is error");
				is_basic = true;
			}
		}
	} else {
		if (info->enable_hv_charging != true) {
			for (i = 0; i < MAX_ALG_NO; i++) {
				alg = info->alg[i];
				if (alg == NULL)
					continue;

				chg_alg_get_prop(alg, ALG_MAX_VBUS, &val);
				if (val > 5000 && chg_alg_is_algo_running(alg))
					chg_alg_stop_algo(alg);

				chr_err("%s: Stop hv charging. en_hv:%d alg:%s alg_vbus:%d\n",
					__func__, info->enable_hv_charging,
					dev_name(&alg->dev), val);
			}
		}
	}
	info->is_chg_done = chg_done;
	/* TabA7 Lite code for P210524-06376 by gaochao at 20210604 start */
	chr_err("%s is_chg_done:%d chg_done:%d \n", __func__, info->is_chg_done, chg_done);
	/* TabA7 Lite code for P210524-06376 by gaochao at 20210604 end */

	if (is_basic == true) {
		charger_dev_set_input_current(info->chg1_dev,
			pdata->input_current_limit);
		charger_dev_set_charging_current(info->chg1_dev,
			pdata->charging_current_limit);
		charger_dev_set_constant_voltage(info->chg1_dev,
			info->setting.cv);

		if (pdata->input_current_limit == 0 ||
		    pdata->charging_current_limit == 0)
			charger_dev_enable(info->chg1_dev, false);
		else
			charger_dev_enable(info->chg1_dev, true);
	}

	if (info->chg1_dev != NULL)
		charger_dev_dump_registers(info->chg1_dev);

	if (info->chg2_dev != NULL)
		charger_dev_dump_registers(info->chg2_dev);

	return 0;
}

static int enable_charging(struct mtk_charger *info,
						bool en)
{
	int i;
	struct chg_alg_device *alg;


	chr_err("%s %d\n", __func__, en);

	if (en == false) {
		for (i = 0; i < MAX_ALG_NO; i++) {
			alg = info->alg[i];
			if (alg == NULL)
				continue;
			chg_alg_stop_algo(alg);
		}
		charger_dev_enable(info->chg1_dev, false);
		charger_dev_do_event(info->chg1_dev, EVENT_DISCHARGE, 0);
	} else {
		charger_dev_enable(info->chg1_dev, true);
		charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
	}

	return 0;
}

static int charger_dev_event(struct notifier_block *nb, unsigned long event,
				void *v)
{
	struct chg_alg_device *alg;
	struct chg_alg_notify notify;
	struct mtk_charger *info =
			container_of(nb, struct mtk_charger, chg1_nb);
	struct chgdev_notify *data = v;
	int i;

	chr_err("%s %d\n", __func__, event);

	switch (event) {
	case CHARGER_DEV_NOTIFY_EOC:
		notify.evt = EVT_FULL;
		notify.value = 0;
	for (i = 0; i < 10; i++) {
		alg = info->alg[i];
		chg_alg_notifier_call(alg, &notify);
	}

		break;
	case CHARGER_DEV_NOTIFY_RECHG:
		pr_info("%s: recharge\n", __func__);
		break;
	case CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT:
		info->safety_timeout = true;
		pr_info("%s: safety timer timeout\n", __func__);
		break;
	case CHARGER_DEV_NOTIFY_VBUS_OVP:
		info->vbusov_stat = data->vbusov_stat;
		pr_info("%s: vbus ovp = %d\n", __func__, info->vbusov_stat);
		break;
	default:
		return NOTIFY_DONE;
	}

	if (info->chg1_dev->is_polling_mode == false)
		_wake_up_charger(info);

	return NOTIFY_DONE;
}



int mtk_basic_charger_init(struct mtk_charger *info)
{

	info->algo.do_algorithm = do_algorithm;
	info->algo.enable_charging = enable_charging;
	info->algo.do_event = charger_dev_event;
	//info->change_current_setting = mtk_basic_charging_current;
	return 0;
}
