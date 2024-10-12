/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

/*
 *
 * Filename:
 * ---------
 *    mtk_switch_charging.c
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
#include <mt-plat/v1/mtk_battery.h>

#include <mt-plat/mtk_boot.h>
#include "mtk_charger_intf.h"
#include "mtk_switch_charging.h"
#include "mtk_intf.h"

#if defined(CONFIG_WT_PROJECT_S96902AA1) //usb if
#ifndef WT_COMPILE_FACTORY_VERSION
#define CONFIG_USBIF_COMPLIANCE 1
#endif /* WT_COMPILE_FACTORY_VERSION */
#endif /* CONFIG_WT_PROJECT_S96902AA1 */

/* +churui1.wt, ADD, 20230602, CP charging control */
#ifdef CONFIG_N28_CHARGER_PRIVATE
#include "../../pd_policy_manager.h"
#include "mtk_charger_init.h"
extern struct cp_charging g_cp_charging;
extern struct usbpd_pm *__pdpm;
#endif
/* -churui1.wt, ADD, 20230602, CP charging control */

//+P240131-05273 liwei19.wt 20240306,one ui 6.1 charging protection
#if defined (CONFIG_W2_CHARGER_PRIVATE) || defined (CONFIG_N28_CHARGER_PRIVATE)
extern int batt_mode;
extern int batt_soc_rechg;
extern int batt_full_capacity;
#endif
//-P240131-05273 liwei19.wt 20240306,one ui 6.1 charging protection

bool g_chg_done = false;
bool first_enter_pdc = false;

#ifdef CONFIG_AFC_CHARGER
#if defined(CONFIG_W2_CHARGER_PRIVATE)
static bool wt_limit_afc_ibus = false;
#endif
#endif

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

static int _uA_to_mA(int uA)
{
	if (uA == -1)
		return -1;
	else
		return uA / 1000;
}

static void _disable_all_charging(struct charger_manager *info)
{
	charger_dev_enable(info->chg1_dev, false);

#ifdef CONFIG_AFC_CHARGER
	/*yuanjian.wt ad for AFC*/
	if (afc_get_is_enable(info)) {
		afc_set_is_enable(info, false);
		if (afc_get_is_connect(info))
			afc_leave(info);
	}
#if defined(CONFIG_W2_CHARGER_PRIVATE)
	wt_limit_afc_ibus = false;
#endif
#endif

	if (mtk_pe20_get_is_enable(info)) {
		mtk_pe20_set_is_enable(info, false);
		if (mtk_pe20_get_is_connect(info))
			mtk_pe20_reset_ta_vchr(info);
	}

	if (mtk_pe_get_is_enable(info)) {
		mtk_pe_set_is_enable(info, false);
		if (mtk_pe_get_is_connect(info))
			mtk_pe_reset_ta_vchr(info);
	}

	if (info->enable_pe_5)
		pe50_stop();

	if (info->enable_pe_4)
		pe40_stop();

	if (pdc_is_ready()) {
#if defined(CONFIG_W2_CHARGER_PRIVATE)
		struct charger_custom_data *pdata = &info->data;
		struct pdc_data *data = NULL;

		data = pdc_get_data();
		chr_err("%s: %d,%d,%d,%d\n", __func__,data->pd_vbus_low_bound,data->pd_vbus_upper_bound,
			pdata->pd_vbus_low_bound,pdata->pd_vbus_upper_bound);
		data->pd_vbus_low_bound = pdata->pd_vbus_low_bound;
		data->pd_vbus_upper_bound = pdata->pd_vbus_upper_bound;
#endif
		pdc_stop();
	}

#ifdef CONFIG_N28_CHARGER_PRIVATE
	g_cp_charging.cp_chg_status |= CP_STOP;
#endif

}

static void swchg_select_charging_current_limit(struct charger_manager *info)
{
	struct charger_data *pdata = NULL;
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	u32 ichg1_min = 0, aicr1_min = 0;
	int ret = 0;

	struct device *dev = NULL;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;
	int boot_mode = 11;//UNKNOWN_BOOT

	dev = &(info->pdev->dev);
	if (dev != NULL) {
		boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
		if (!boot_node) {
			chr_err("%s: failed to get boot mode phandle\n", __func__);
		} else {
			tag = (struct tag_bootmode *)of_get_property(boot_node,
								"atag,boot", NULL);
			if (!tag) {
				chr_err("%s: failed to get atag,boot\n", __func__);
			} else
				boot_mode = tag->bootmode;
		}
	}

	if (info->pe5.online) {
		chr_err("In PE5.0\n");
		return;
	}

	pdata = &info->chg1_data;
	mutex_lock(&swchgalg->ichg_aicr_access_mutex);

	/* AICL */
#ifdef CONFIG_AFC_CHARGER
        if (!mtk_pe20_get_is_connect(info) && !mtk_pe_get_is_connect(info) &&
            !mtk_is_TA_support_pd_pps(info) && !mtk_pdc_check_charger(info) && !afc_get_is_connect(info)) {
#else
	if (!mtk_pe20_get_is_connect(info) && !mtk_pe_get_is_connect(info) &&
	    !mtk_is_TA_support_pd_pps(info) && !mtk_pdc_check_charger(info)) {
#endif
		charger_dev_run_aicl(info->chg1_dev,
				&pdata->input_current_limit_by_aicl);
		if (info->enable_dynamic_mivr) {
			if (pdata->input_current_limit_by_aicl >
				info->data.max_dmivr_charger_current)
				pdata->input_current_limit_by_aicl =
					info->data.max_dmivr_charger_current;
		}
	} else {
		pdata->input_current_limit_by_aicl = -1; //Bug774000,gudi.wt,ADD,reset aicl
	}

	if (pdata->force_charging_current > 0) {

		pdata->charging_current_limit = pdata->force_charging_current;
		if (pdata->force_charging_current <= 450000) {
			pdata->input_current_limit = 500000;
		} else {
			pdata->input_current_limit =
					info->data.ac_charger_input_current;
			pdata->charging_current_limit =
					info->data.ac_charger_current;
		}
		goto done;
	}

	if (info->usb_unlimited) {
		if (pdata->input_current_limit_by_aicl != -1) {
			pdata->input_current_limit =
				pdata->input_current_limit_by_aicl;
		} else {
			pdata->input_current_limit =
				info->data.usb_unlimited_current;
		}
		pdata->charging_current_limit =
			info->data.ac_charger_current;
		goto done;
	}

	if (info->water_detected) {
		pdata->input_current_limit = info->data.usb_charger_current;
		pdata->charging_current_limit = info->data.usb_charger_current;
		goto done;
	}

	if ((boot_mode == META_BOOT) ||
		(boot_mode == ADVMETA_BOOT)) {
		pdata->input_current_limit = 200000; /* 200mA */
		goto done;
	}

	if (info->atm_enabled == true && (info->chr_type == STANDARD_HOST ||
	    info->chr_type == CHARGING_HOST)) {
		pdata->input_current_limit = 100000; /* 100mA */
		goto done;
	}

	if (is_typec_adapter(info)) {
		if (adapter_dev_get_property(info->pd_adapter, TYPEC_RP_LEVEL)
			== 3000) {
			pdata->input_current_limit = 3000000;
			pdata->charging_current_limit = 3000000;
		} else if (adapter_dev_get_property(info->pd_adapter,
			TYPEC_RP_LEVEL) == 1500) {
			pdata->input_current_limit = 1500000;
			pdata->charging_current_limit = 2000000;
		} else {
			chr_err("type-C: inquire rp error\n");
			pdata->input_current_limit = 500000;
			pdata->charging_current_limit = 500000;
		}

		chr_err("type-C:%d current:%d\n",
			info->pd_type,
			adapter_dev_get_property(info->pd_adapter,
				TYPEC_RP_LEVEL));
	} else if (info->chr_type == STANDARD_HOST) {
		if (IS_ENABLED(CONFIG_USBIF_COMPLIANCE)) {
#if defined(CONFIG_WT_PROJECT_S96902AA1) //usb if
if(!(boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT || boot_mode == LOW_POWER_OFF_CHARGING_BOOT)){
				if (info->usb_state == USB_SUSPEND) {
					charger_dev_enable_powerpath(info->chg1_dev,false);
					pdata->input_current_limit =
						info->data.usb_charger_current_suspend;
					chr_err("powerpatch flase input_current_limit =%d,info->usb_state = %d\n",pdata->input_current_limit,info->usb_state);
				} else if (info->usb_state == USB_UNCONFIGURED) {
					charger_dev_enable_powerpath(info->chg1_dev,true);
					pdata->input_current_limit =
					info->data.usb_charger_current_unconfigured;
					chr_err("powerpatch en input_current_limit =%d,info->usb_state = %d\n",pdata->input_current_limit,info->usb_state);
				} else if (info->usb_state == USB_CONFIGURED) {
					charger_dev_enable_powerpath(info->chg1_dev,true);
					pdata->input_current_limit =
					info->data.usb_charger_current_configured;
					chr_err("powerpatch en input_current_limit =%d,info->usb_state = %d\n",pdata->input_current_limit,info->usb_state);
				} else {
					charger_dev_enable_powerpath(info->chg1_dev,true);
					pdata->input_current_limit =
					info->data.usb_charger_current_unconfigured;
					chr_err("powerpatch en input_current_limit =%d,info->usb_state = %d\n",pdata->input_current_limit,info->usb_state);
				}
				pdata->charging_current_limit =
						pdata->input_current_limit;
			}
			else{
				pdata->input_current_limit = 500000;
				pdata->charging_current_limit = 500000;
			}
		} else {
			//charger_dev_enable_hz(info->chg1_dev, 0);
			charger_dev_enable_powerpath(info->chg1_dev,true);

			pdata->input_current_limit =
					info->data.usb_charger_current;
			/* it can be larger */
			pdata->charging_current_limit =
					info->data.usb_charger_current;
		}
#else  /* CONFIG_WT_PROJECT_S96902AA1 */
			if (info->usb_state == USB_SUSPEND)
				pdata->input_current_limit =
					info->data.usb_charger_current_suspend;
			else if (info->usb_state == USB_UNCONFIGURED)
				pdata->input_current_limit =
				info->data.usb_charger_current_unconfigured;
			else if (info->usb_state == USB_CONFIGURED)
				pdata->input_current_limit =
				info->data.usb_charger_current_configured;
			else
				pdata->input_current_limit =
				info->data.usb_charger_current_unconfigured;

			pdata->charging_current_limit =
					pdata->input_current_limit;
		} else {
			pdata->input_current_limit =
					info->data.usb_charger_current;
			/* it can be larger */
			pdata->charging_current_limit =
					info->data.usb_charger_current;
		}
#endif /* CONFIG_WT_PROJECT_S96902AA1 */
	} else if (info->chr_type == NONSTANDARD_CHARGER) {
		pdata->input_current_limit =
				info->data.non_std_ac_charger_current;
		pdata->charging_current_limit =
				info->data.non_std_ac_charger_current;
	} else if (info->chr_type == STANDARD_CHARGER) {
		pdata->input_current_limit =
				info->data.ac_charger_input_current;
		pdata->charging_current_limit =
				info->data.ac_charger_current;
#ifdef CONFIG_AFC_CHARGER
		/* yuanjian.wt add for AFC */
		afc_set_charging_current(info,
					&pdata->charging_current_limit,
					&pdata->input_current_limit);
#endif
		mtk_pe20_set_charging_current(info,
					&pdata->charging_current_limit,
					&pdata->input_current_limit);
		mtk_pe_set_charging_current(info,
					&pdata->charging_current_limit,
					&pdata->input_current_limit);
#if defined(CONFIG_W2_CHARGER_PRIVATE)
		if (mtk_pdc_check_charger(info) == false) {
			if ((info->is_camera_on) && (!info->enable_hv_charging)) {
				pdata->input_current_limit = 1200000;
			}
		}
		pr_err("%s: afc input_current_limit=%d\n", __func__, pdata->input_current_limit);
#endif
	} else if (info->chr_type == CHARGING_HOST) {
		pdata->input_current_limit =
				info->data.charging_host_charger_current;
		pdata->charging_current_limit =
				info->data.charging_host_charger_current;
	} else if (info->chr_type == APPLE_1_0A_CHARGER) {
		pdata->input_current_limit =
				info->data.apple_1_0a_charger_current;
		pdata->charging_current_limit =
				info->data.apple_1_0a_charger_current;
	} else if (info->chr_type == APPLE_2_1A_CHARGER) {
		pdata->input_current_limit =
				info->data.apple_2_1a_charger_current;
		pdata->charging_current_limit =
				info->data.apple_2_1a_charger_current;
	}

#ifndef CONFIG_MTK_DISABLE_TEMP_PROTECT
	if (info->enable_sw_jeita) {
		if (IS_ENABLED(CONFIG_USBIF_COMPLIANCE)
		    && info->chr_type == STANDARD_HOST){
			chr_err("USBIF & STAND_HOST skip current check\n");
//gudi.wt, SW JEITA
		}else if (info->sw_jeita.sm != TEMP_T2_TO_T3) {
			pdata->charging_current_limit = info->sw_jeita.cc;
		}
	}

	//sc_select_charging_current(info, pdata);//gudi.wt charging current of the Fix PD is faulty

//+Bug774000,gudi.wt,ADD,charge current limit for AP overheat
#ifndef WT_COMPILE_FACTORY_VERSION
//Bug770440,gudi.wt,ADD,no need to limit current by thermal in LPM
//+Bug668347,lvyuanchuan.wt,modify,20210617,need to limit current by thermal in LPM
#if 0
	if((KERNEL_POWER_OFF_CHARGING_BOOT != get_boot_mode()) &&
	(LOW_POWER_OFF_CHARGING_BOOT != get_boot_mode())) 
#endif
//-Bug668347,lvyuanchuan.wt,modify,20210617,need to limit current by thermal in LPM	
		if(info->lcmoff) {
			if(info->ap_thermal_lcmoff.cc < pdata->charging_current_limit)
				pdata->charging_current_limit = info->ap_thermal_lcmoff.cc;
		} else {
			if(info->ap_thermal_lcmon.cc < pdata->charging_current_limit)
				pdata->charging_current_limit = info->ap_thermal_lcmon.cc;
		}
#endif /*WT_COMPILE_FACTORY_VERSION*/
//-Bug774000,gudi.wt,ADD,charge current limit for AP overheat
	printk("jeita and ap limit: current_limit = %d,current_limit = %d\n",
		pdata->charging_current_limit,pdata->input_current_limit);
	if (pdata->thermal_input_current_limit != -1) {
		if (pdata->thermal_input_current_limit <
		    pdata->input_current_limit)
			pdata->input_current_limit =
					pdata->thermal_input_current_limit;
	}
#endif

#ifdef CONFIG_AFC_CHARGER
	if (pdata->input_current_limit_by_aicl != -1 &&
	    !mtk_pe20_get_is_connect(info) && !mtk_pe_get_is_connect(info) &&
	    !mtk_is_TA_support_pd_pps(info)&& !mtk_pdc_check_charger(info) &&
	    !afc_get_is_connect(info)) {
#else
	if (pdata->input_current_limit_by_aicl != -1 &&
	    !mtk_pe20_get_is_connect(info) && !mtk_pe_get_is_connect(info) &&
	    !mtk_is_TA_support_pd_pps(info)) {
#endif
		if (pdata->input_current_limit_by_aicl <
		    pdata->input_current_limit)
			pdata->input_current_limit =
					pdata->input_current_limit_by_aicl;
	}
done:
	ret = charger_dev_get_min_charging_current(info->chg1_dev, &ichg1_min);
	if (ret != -ENOTSUPP && pdata->charging_current_limit < ichg1_min)
		pdata->charging_current_limit = ichg1_min;

	ret = charger_dev_get_min_input_current(info->chg1_dev, &aicr1_min);
	if (ret != -ENOTSUPP && pdata->input_current_limit < aicr1_min)
		pdata->input_current_limit = aicr1_min;

	chr_err("force:%d thermal:%d,%d pe4:%d,%d,%d setting:%d %d sc:%d,%d,%d type:%d usb_unlimited:%d usbif:%d usbsm:%d aicl:%d atm:%d\n",
		_uA_to_mA(pdata->force_charging_current),
		_uA_to_mA(pdata->thermal_input_current_limit),
		_uA_to_mA(pdata->thermal_charging_current_limit),
		_uA_to_mA(info->pe4.pe4_input_current_limit),
		_uA_to_mA(info->pe4.pe4_input_current_limit_setting),
		_uA_to_mA(info->pe4.input_current_limit),
		_uA_to_mA(pdata->input_current_limit),
		_uA_to_mA(pdata->charging_current_limit),
		_uA_to_mA(info->sc.pre_ibat),
		_uA_to_mA(info->sc.sc_ibat),
		info->sc.solution,
		info->chr_type, info->usb_unlimited,
		IS_ENABLED(CONFIG_USBIF_COMPLIANCE), info->usb_state,
		pdata->input_current_limit_by_aicl, info->atm_enabled);

	charger_dev_set_input_current(info->chg1_dev,
					pdata->input_current_limit);
	charger_dev_set_charging_current(info->chg1_dev,
					pdata->charging_current_limit);

	/* If AICR < 300mA, stop PE+/PE+20 */
	if (pdata->input_current_limit < 300000) {
#ifdef CONFIG_AFC_CHARGER
		/* yuanjian.wt add for AFC */
		if (afc_get_is_enable(info)) {
			afc_set_is_enable(info, false);
			if (afc_get_is_connect(info))
				afc_reset_ta_vchr(info);
		}
#endif
		if (mtk_pe20_get_is_enable(info)) {
			mtk_pe20_set_is_enable(info, false);
			if (mtk_pe20_get_is_connect(info))
				mtk_pe20_reset_ta_vchr(info);
		}

		if (mtk_pe_get_is_enable(info)) {
			mtk_pe_set_is_enable(info, false);
			if (mtk_pe_get_is_connect(info))
				mtk_pe_reset_ta_vchr(info);
		}
	}

	/*
	 * If thermal current limit is larger than charging IC's minimum
	 * current setting, enable the charger immediately
	 */
	if (pdata->input_current_limit > aicr1_min &&
	    pdata->charging_current_limit > ichg1_min && info->can_charging)
		charger_dev_enable(info->chg1_dev, true);
	mutex_unlock(&swchgalg->ichg_aicr_access_mutex);
}

#ifndef CONFIG_N28_CHARGER_PRIVATE
static void first_select_charging_current_limit(struct charger_manager *info)
{
	struct charger_data *pdata = NULL;
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;

	if (get_boot_mode() == NORMAL_BOOT && mt_get_charger_type() == STANDARD_CHARGER) {
		pdata = &info->chg1_data;
		mutex_lock(&swchgalg->ichg_aicr_access_mutex);
		pdata->input_current_limit = pdata->charging_current_limit = info->sw_jeita.cc;
		if(info->lcmoff) {
			if(info->ap_thermal_lcmoff.cc < pdata->charging_current_limit)
				pdata->charging_current_limit = info->ap_thermal_lcmoff.cc;
		} else {
			if(info->ap_thermal_lcmon.cc < pdata->charging_current_limit)
				pdata->charging_current_limit = info->ap_thermal_lcmon.cc;
		}
		chr_err("First[%d]: setting:%d %d \n",info->can_charging,_uA_to_mA(pdata->input_current_limit),_uA_to_mA(pdata->charging_current_limit));
		charger_dev_set_input_current(info->chg1_dev,pdata->input_current_limit);
		charger_dev_set_charging_current(info->chg1_dev,pdata->charging_current_limit);
		if (info->can_charging)
			charger_dev_enable(info->chg1_dev, true);
		mutex_unlock(&swchgalg->ichg_aicr_access_mutex);
      }
}
#endif

static void swchg_select_cv(struct charger_manager *info)
{
	u32 constant_voltage;

	if (info->enable_sw_jeita)
		if (info->sw_jeita.cv != 0) {
			charger_dev_set_constant_voltage(info->chg1_dev,
							info->sw_jeita.cv);
			return;
		}

	/* dynamic cv*/
	constant_voltage = info->data.battery_cv;
	mtk_get_dynamic_cv(info, &constant_voltage);

	charger_dev_set_constant_voltage(info->chg1_dev, constant_voltage);
}

static void swchg_turn_on_charging(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	bool charging_enable = true;

	struct device *dev = NULL;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;
	int boot_mode = 11;//UNKNOWN_BOOT

	dev = &(info->pdev->dev);
	if (dev != NULL) {
		boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
		if (!boot_node) {
			chr_err("%s: failed to get boot mode phandle\n", __func__);
		} else {
			tag = (struct tag_bootmode *)of_get_property(boot_node,
								"atag,boot", NULL);
			if (!tag)
				chr_err("%s: failed to get atag,boot\n", __func__);
			else
				boot_mode = tag->bootmode;
		}
	}

	if (swchgalg->state == CHR_ERROR) {
		charging_enable = false;
		chr_err("[charger]Charger Error, turn OFF charging !\n");
	} else if ((boot_mode == META_BOOT) ||
			(boot_mode == ADVMETA_BOOT)) {
/* +S96818AA1-4520, churui1.wt, MODIFY, 20230529, BT/FT probabilistic test failed ATO mode of sd7601 */
#ifdef	CONFIG_N28_CHARGER_PRIVATE
		charging_enable = true;
		info->chg1_data.input_current_limit = 200000; /* 200mA */
		charger_dev_set_input_current(info->chg1_dev,
					info->chg1_data.input_current_limit);
		chr_err("In meta mode, enable charging and set input current limit to 200mA\n");
/* -S96818AA1-4520, churui1.wt, MODIFY, 20230529, BT/FT probabilistic test failed ATO mode of sd7601 */
#else
		charging_enable = false;
		info->chg1_data.input_current_limit = 200000; /* 200mA */
		charger_dev_set_input_current(info->chg1_dev,
					info->chg1_data.input_current_limit);
		chr_err("In meta mode, disable charging and set input current limit to 200mA\n");
#endif

	} else {
		mtk_pe20_start_algorithm(info);
		if (mtk_pe20_get_is_connect(info) == false)
			mtk_pe_start_algorithm(info);

		swchg_select_charging_current_limit(info);
		if (info->chg1_data.input_current_limit == 0
		    || info->chg1_data.charging_current_limit == 0) {
			charging_enable = false;
			chr_err("[charger]charging current is set 0mA, turn off charging !\n");
		} else {
			swchg_select_cv(info);
		}
	}

	charger_dev_enable(info->chg1_dev, charging_enable);
}

static int mtk_switch_charging_plug_in(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;

	swchgalg->state = CHR_CC;
//+P240329 guhan01.wt 20240412,one ui 6.1 charging protection not recharge
#if defined (CONFIG_W2_CHARGER_PRIVATE) || defined (CONFIG_N28_CHARGER_PRIVATE)
	g_chg_done = false;
#endif
//-P240329 guhan01.wt 20240412,one ui 6.1 charging protection not recharge
	info->polling_interval = CHARGING_INTERVAL;
	swchgalg->disable_charging = false;
	get_monotonic_boottime(&swchgalg->charging_begin_time);

	return 0;
}

static int mtk_switch_charging_plug_out(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;

	swchgalg->total_charging_time = 0;
//zhaosidong.wt, TA plug out
	if (info->cable_out_cnt == 0) {
#ifdef CONFIG_AFC_CHARGER
		afc_plugout_reset(info);
#if defined(CONFIG_W2_CHARGER_PRIVATE)
		wt_limit_afc_ibus = false;
#endif
#endif
	mtk_pe20_set_is_cable_out_occur(info, true);
	mtk_pe_set_is_cable_out_occur(info, true);
	mtk_pdc_plugout(info);

	if (info->enable_pe_5)
		pe50_stop();

	if (info->enable_pe_4)
		pe40_stop();

	info->leave_pe5 = false;
	info->leave_pe4 = false;
	info->leave_pdc = false;

//+P240329 guhan01.wt 20240412,one ui 6.1 charging protection not recharge
#if defined (CONFIG_W2_CHARGER_PRIVATE) || defined (CONFIG_N28_CHARGER_PRIVATE)
	g_chg_done = false;
#endif
//-P240329 guhan01.wt 20240412,one ui 6.1 charging protection not recharge
	}

	return 0;
}

static int mtk_switch_charging_do_charging(struct charger_manager *info,
						bool en)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;

	chr_err("%s: en:%d %s\n", __func__, en, info->algorithm_name);
	if (en) {
		swchgalg->disable_charging = false;
		swchgalg->state = CHR_CC;
		get_monotonic_boottime(&swchgalg->charging_begin_time);
		charger_manager_notifier(info, CHARGER_NOTIFY_NORMAL);
	} else {
		/* disable charging might change state, so call it first */
		_disable_all_charging(info);
		swchgalg->disable_charging = true;
		swchgalg->state = CHR_ERROR;
		charger_manager_notifier(info, CHARGER_NOTIFY_ERROR);
	}

	return 0;
}

static int mtk_switch_chr_pe50_init(struct charger_manager *info)
{
	int ret;

	ret = pe50_init();

	if (ret == 0)
		set_charger_manager(info);
	else
		chr_err("pe50 init fail\n");

	info->leave_pe5 = false;

	return ret;
}

static int mtk_switch_chr_pe50_run(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	/* struct charger_custom_data *pdata = &info->data; */
	/* struct pe50_data *data; */
	int ret = 0;

	if (info->enable_hv_charging == false)
		goto stop;

	ret = pe50_run();

	if (ret == 1) {
		pr_info("retry pe5\n");
		goto retry;
	}

	if (ret == 2) {
		chr_err("leave pe5\n");
		info->leave_pe5 = true;
		swchgalg->state = CHR_CC;
	}

	return 0;

stop:
	pe50_stop();
retry:
	swchgalg->state = CHR_CC;

	return 0;
}

static int mtk_switch_chr_pe40_init(struct charger_manager *info)
{
	int ret;

	ret = pe40_init();

	if (ret == 0)
		set_charger_manager(info);

	info->leave_pe4 = false;

	return 0;
}

static int select_pe40_charging_current_limit(struct charger_manager *info)
{
	struct charger_data *pdata;
	u32 ichg1_min = 0, aicr1_min = 0;
	int ret = 0;

	pdata = &info->chg1_data;

	pdata->input_current_limit =
		info->data.pe40_single_charger_input_current;
	pdata->charging_current_limit =
		info->data.pe40_single_charger_current;

	sc_select_charging_current(info, pdata);

	if (pdata->thermal_input_current_limit != -1) {
		if (pdata->thermal_input_current_limit <
		    pdata->input_current_limit)
			pdata->input_current_limit =
					pdata->thermal_input_current_limit;
	}

	ret = charger_dev_get_min_charging_current(info->chg1_dev, &ichg1_min);
	if (ret != -ENOTSUPP && pdata->charging_current_limit < ichg1_min)
		pdata->charging_current_limit = 0;

	ret = charger_dev_get_min_input_current(info->chg1_dev, &aicr1_min);
	if (ret != -ENOTSUPP && pdata->input_current_limit < aicr1_min)
		pdata->input_current_limit = 0;

	chr_err("force:%d thermal:%d,%d setting:%d %d sc:%d %d %d type:%d usb_unlimited:%d usbif:%d usbsm:%d aicl:%d atm:%d\n",
		_uA_to_mA(pdata->force_charging_current),
		_uA_to_mA(pdata->thermal_input_current_limit),
		_uA_to_mA(pdata->thermal_charging_current_limit),
		_uA_to_mA(pdata->input_current_limit),
		_uA_to_mA(pdata->charging_current_limit),
		info->sc.pre_ibat,
		info->sc.sc_ibat,
		info->sc.solution,
		info->chr_type, info->usb_unlimited,
		IS_ENABLED(CONFIG_USBIF_COMPLIANCE), info->usb_state,
		pdata->input_current_limit_by_aicl, info->atm_enabled);

	return 0;
}

static int mtk_switch_chr_pe40_run(struct charger_manager *info)
{
	struct charger_custom_data *pdata = &info->data;
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	struct pe40_data *data = NULL;
	int ret = 0;

	charger_dev_enable(info->chg1_dev, true);
	select_pe40_charging_current_limit(info);

	data = pe40_get_data();
	if (!data) {
		chr_err("%s: data is NULL\n", __func__);
		goto stop;
	}

	data->input_current_limit = info->chg1_data.input_current_limit;
	data->charging_current_limit = info->chg1_data.charging_current_limit;
	data->pe40_max_vbus = pdata->pe40_max_vbus;
	data->high_temp_to_leave_pe40 = pdata->high_temp_to_leave_pe40;
	data->high_temp_to_enter_pe40 = pdata->high_temp_to_enter_pe40;
	data->low_temp_to_leave_pe40 = pdata->low_temp_to_leave_pe40;
	data->low_temp_to_enter_pe40 = pdata->low_temp_to_enter_pe40;
	data->pe40_r_cable_1a_lower = pdata->pe40_r_cable_1a_lower;
	data->pe40_r_cable_2a_lower = pdata->pe40_r_cable_2a_lower;
	data->pe40_r_cable_3a_lower = pdata->pe40_r_cable_3a_lower;

	data->battery_cv = pdata->battery_cv;
	if (info->enable_sw_jeita) {
		if (info->sw_jeita.cv != 0)
			data->battery_cv = info->sw_jeita.cv;
	}

	if (info->enable_hv_charging == false)
		goto stop;
	if (info->pd_reset == true) {
		chr_err("encounter hard reset, stop pe4.0\n");
		info->pd_reset = false;
		goto stop;
	}

	ret = pe40_run();

	if (ret == 1) {
		chr_err("retry pe4\n");
		goto retry;
	}

	if (ret == 2 &&
		info->chg1_data.thermal_charging_current_limit == -1 &&
		info->chg1_data.thermal_input_current_limit == -1) {
		chr_err("leave pe4\n");
		info->leave_pe4 = true;
		swchgalg->state = CHR_CC;
	}

	return 0;

stop:
	pe40_stop();
retry:
	swchgalg->state = CHR_CC;

	return 0;
}


static int mtk_switch_chr_pdc_init(struct charger_manager *info)
{
	int ret;

	ret = pdc_init();

	if (ret == 0)
		set_charger_manager(info);

	info->leave_pdc = false;

	return 0;
}

static int select_pdc_charging_current_limit(struct charger_manager *info)
{
	struct charger_data *pdata;
	u32 ichg1_min = 0, aicr1_min = 0;
	int ret = 0;

	pdata = &info->chg1_data;

	//Extb+ P210310-04076,lvyuanchuan.wt,Modify,20210406,config input current for SDP
	if((info->chr_type == STANDARD_HOST) || (info->chr_type == CHARGER_UNKNOWN)){
		// lvyuanchuan.wt, USB Charging current
		pdata->input_current_limit = 500000;
		pdata->charging_current_limit = 500000;
	}else{
		pdata->input_current_limit = info->data.pd_charger_current;
		pdata->charging_current_limit = info->data.pd_charger_current;
	//Extb- P210310-04076,lvyuanchuan.wt,Modify,20210406,config input current for SDP
//Extb+ P210723-05167,lvyuanchuan.wt,Modify,20210729,config charging current limit
#ifndef CONFIG_MTK_DISABLE_TEMP_PROTECT
		if (info->enable_sw_jeita) {
			 if (info->sw_jeita.sm != TEMP_T2_TO_T3) {
				pdata->charging_current_limit = info->sw_jeita.cc;
			}
		}

		//sc_select_charging_current(info, pdata);

#ifndef WT_COMPILE_FACTORY_VERSION
		if(info->lcmoff) {
			if(info->ap_thermal_lcmoff.cc < pdata->charging_current_limit)
				pdata->charging_current_limit = info->ap_thermal_lcmoff.cc;
		} else {
			if(info->ap_thermal_lcmon.cc < pdata->charging_current_limit)
				pdata->charging_current_limit = info->ap_thermal_lcmon.cc;
		}
#endif /*WT_COMPILE_FACTORY_VERSION*/

		if (pdata->thermal_input_current_limit != -1) {
			if (pdata->thermal_input_current_limit <
			    pdata->input_current_limit)
				pdata->input_current_limit =
						pdata->thermal_input_current_limit;
		}
		chr_err("[pd] input_current_limit :[%d],charging_current_limit :[%d]",_uA_to_mA(pdata->input_current_limit),_uA_to_mA(pdata->charging_current_limit));
#endif	
//Extb- P210723-05167,lvyuanchuan.wt,Modify,20210729,config charging current limit
	}
	ret = charger_dev_get_min_charging_current(info->chg1_dev, &ichg1_min);
	if (ret != -ENOTSUPP && pdata->charging_current_limit < ichg1_min)
		pdata->charging_current_limit = 0;

	ret = charger_dev_get_min_input_current(info->chg1_dev, &aicr1_min);
	if (ret != -ENOTSUPP && pdata->input_current_limit < aicr1_min)
		pdata->input_current_limit = 0;

	chr_err("force:%d thermal:%d,%d setting:%d %d sc:%d %d %d type:%d usb_unlimited:%d usbif:%d usbsm:%d aicl:%d atm:%d\n",
		_uA_to_mA(pdata->force_charging_current),
		_uA_to_mA(pdata->thermal_input_current_limit),
		_uA_to_mA(pdata->thermal_charging_current_limit),
		_uA_to_mA(pdata->input_current_limit),
		_uA_to_mA(pdata->charging_current_limit),
		info->sc.pre_ibat,
		info->sc.sc_ibat,
		info->sc.solution,
		info->chr_type, info->usb_unlimited,
		IS_ENABLED(CONFIG_USBIF_COMPLIANCE), info->usb_state,
		pdata->input_current_limit_by_aicl, info->atm_enabled);

	return 0;
}

static int mtk_switch_chr_pdc_run(struct charger_manager *info)
{
	struct charger_custom_data *pdata = &info->data;
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	struct pdc_data *data = NULL;
	int ret = 0;

	charger_dev_enable(info->chg1_dev, true);
	select_pdc_charging_current_limit(info);

	data = pdc_get_data();

	data->input_current_limit = info->chg1_data.input_current_limit;
	data->charging_current_limit = info->chg1_data.charging_current_limit;
	data->pd_vbus_low_bound = pdata->pd_vbus_low_bound;
	data->pd_vbus_upper_bound = pdata->pd_vbus_upper_bound;

	data->battery_cv = pdata->battery_cv;
	if (info->enable_sw_jeita) {
		if (info->sw_jeita.cv != 0)
			data->battery_cv = info->sw_jeita.cv;
	}

	if (info->enable_hv_charging == false)
		goto stop;
	info->is_pdc_run = true;
	ret = pdc_run();

	if (ret == 2 &&
		info->chg1_data.thermal_charging_current_limit == -1 &&
		info->chg1_data.thermal_input_current_limit == -1 ) {
		chr_err("leave pdc\n");
		info->leave_pdc = true;
		info->is_pdc_run = false;
		swchgalg->state = CHR_CC;
	}

	return 0;

stop:
	pdc_stop();
	swchgalg->state = CHR_CC;
	info->is_pdc_run = false;
	return 0;
}
/*+S96818AA1-9230 lijiawei,wt.modify upm6910 safetytimer function logic*/
#ifdef CONFIG_N28_CHARGER_PRIVATE
extern bool is_upm6910;
#endif
/*-S96818AA1-9230 lijiawei,wt.modify upm6910 safetytimer function logic*/
/* return false if total charging time exceeds max_charging_time */
static bool mtk_switch_check_charging_time(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	struct timespec time_now;
/*+S96818AA1-9230 lijiawei,wt.modify upm6910 safetytimer function logic*/
#ifdef CONFIG_N28_CHARGER_PRIVATE
	if (info->enable_sw_safety_timer && is_upm6910) {
#else
	if (info->enable_sw_safety_timer) {
#endif
/*-S96818AA1-9230 lijiawei,wt.modify upm6910 safetytimer function logic*/
		get_monotonic_boottime(&time_now);
		chr_debug("%s: begin: %ld, now: %ld\n", __func__,
			swchgalg->charging_begin_time.tv_sec, time_now.tv_sec);

		if (swchgalg->total_charging_time >=
		    info->data.max_charging_time) {
			chr_err("%s: SW safety timeout: %d sec > %d sec\n",
				__func__, swchgalg->total_charging_time,
				info->data.max_charging_time);
			charger_dev_notify(info->chg1_dev,
					CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT);
			return false;
		}
	}

	return true;
}

extern int mtkts_bts_get_hw_temp(void);
static int mtk_switch_chr_cc(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	struct timespec time_now, charging_time;
	int tmp = battery_get_bat_temperature();
	int ap_temp;
//zhaosidong.wt, SOC is mantained for a long time
	u_int fg_daemon_init = false;

	/* check bif */
	if (IS_ENABLED(CONFIG_MTK_BIF_SUPPORT)) {
		if (pmic_is_bif_exist() != 1) {
			chr_err("CONFIG_MTK_BIF_SUPPORT but no bif , stop charging\n");
			swchgalg->state = CHR_ERROR;
			charger_manager_notifier(info, CHARGER_NOTIFY_ERROR);
		}
	}

	get_monotonic_boottime(&time_now);
	charging_time = timespec_sub(time_now, swchgalg->charging_begin_time);

	swchgalg->total_charging_time = charging_time.tv_sec;

	chr_err("pe40_ready:%d pps:%d hv:%d thermal:%d,%d tmp:%d,%d,%d\n",
		info->enable_pe_4,
		pe40_is_ready(),
		info->enable_hv_charging,
		info->chg1_data.thermal_charging_current_limit,
		info->chg1_data.thermal_input_current_limit,
		tmp,
		info->data.high_temp_to_enter_pe40,
		info->data.low_temp_to_enter_pe40);

	if (info->enable_pe_5 && pe50_is_ready() && !info->leave_pe5) {
		if (info->enable_hv_charging == true) {
			chr_err("enter PE5.0\n");
			swchgalg->state = CHR_PE50;
			info->pe5.online = true;
			if (mtk_pe20_get_is_enable(info)) {
				mtk_pe20_set_is_enable(info, false);
				if (mtk_pe20_get_is_connect(info))
					mtk_pe20_reset_ta_vchr(info);
			}

			if (mtk_pe_get_is_enable(info)) {
				mtk_pe_set_is_enable(info, false);
				if (mtk_pe_get_is_connect(info))
					mtk_pe_reset_ta_vchr(info);
			}
			return 1;
		}
	}

	if (info->enable_pe_4 &&
		pe40_is_ready() &&
		!info->leave_pe4) {
		if (info->enable_hv_charging == true &&
			info->chg1_data.thermal_charging_current_limit == -1 &&
			info->chg1_data.thermal_input_current_limit == -1) {
			chr_err("enter PE4.0!\n");
			swchgalg->state = CHR_PE40;
			if (mtk_pe20_get_is_enable(info)) {
				mtk_pe20_set_is_enable(info, false);
				if (mtk_pe20_get_is_connect(info))
					mtk_pe20_reset_ta_vchr(info);
			}

			if (mtk_pe_get_is_enable(info)) {
				mtk_pe_set_is_enable(info, false);
				if (mtk_pe_get_is_connect(info))
					mtk_pe_reset_ta_vchr(info);
			}
			return 1;
		}
	}

/* +BugS96818AA1-8318, -wangshewen.wt, ADD, 20230711, need to limit current by thermal */
	if(info->ap_temp != 0xffff) {
		ap_temp = info->ap_temp;
	} else {
		ap_temp = mtkts_bts_get_hw_temp();
		ap_temp = ap_temp / 1000;
	}
/* -BugS96818AA1-8318, -wangshewen.wt, ADD, 20230711, need to limit current by thermal */

#ifdef CONFIG_N28_CHARGER_PRIVATE
	if (adapter_is_support_pd_pps()) { //identified as APDO, run pps protocol
/* +P230703-01143, zhouxiaopeng2.wt, MODIFY, 20230710, the PD item of the mold cannot pass */
		g_pd_work_status = 1;
/* -P230703-01143, zhouxiaopeng2.wt, MODIFY, 20230710, the PD item of the mold cannot pass */
#ifndef WT_COMPILE_FACTORY_VERSION //charging unlimited current on ATO version
		if (info->lcmoff) {
			chr_err("enable_hv_charging=%d, ap_temp=%d, battery_temp=%d \n",
				info->enable_hv_charging, ap_temp, info->battery_temp);
			if (info->enable_hv_charging == true && //set hv_disable
				ap_temp <= AP_TEMP_T3_CP_THRES && //AP temp higher than 45??
				info->battery_temp > TEMP_T1_THRES_PLUS_X_DEGREE) { //batt temp lower than 5??
				g_cp_charging.cp_chg_status &= ~CP_EXIT;
			} else {
				g_cp_charging.cp_chg_status |= CP_EXIT;
//+P240111-05098   guhan01.wt 2024031820,Modify the maximum current limit for bright screen charging
				chr_err("lcd off off temp higher than 45 exit CP charging \n");
			}
		} else {
			if (info->enable_hv_charging == true && //set hv_disable
				ap_temp <= AP_TEMP_T3_CP_THRES && //AP temp higher than 45℃
				info->battery_temp > TEMP_T1_THRES_PLUS_X_DEGREE) { //batt temp lower than 5℃
				g_cp_charging.cp_chg_status &= ~CP_EXIT;
			} else {
				g_cp_charging.cp_chg_status |= CP_EXIT;
				g_cp_charging.cp_chg_status |= CP_REENTER;
				chr_err("lcd id on Turn off fast charging\n");
			}
			//no CP charging when lcd is on
			//g_cp_charging.cp_chg_status |= CP_EXIT;
			//g_cp_charging.cp_chg_status |= CP_REENTER;
			//chr_err("Lcm on -> Lcm off re-enter cp\n");
		}
//-P240111-05098   guhan01.wt 2024031820,Modify the maximum current limit for bright screen charging
#else
		g_cp_charging.cp_chg_status &= ~CP_EXIT; //don't exit cp on ATO version
#endif
//+P240111-05098   guhan01.wt 2024031820,Modify the maximum current limit for bright screen charging
		chr_err("CP_REENTER=%d,CP_DONE=%d,CP_EXIT=%d\n",
		g_cp_charging.cp_chg_status & CP_EXIT,
		g_cp_charging.cp_chg_status & CP_DONE,
		g_cp_charging.cp_chg_status & CP_REENTER);
//-P240111-05098   guhan01.wt 2024031820,Modify the maximum current limit for bright screen charging
		if (!(g_cp_charging.cp_chg_status & CP_EXIT) &&
			!(g_cp_charging.cp_chg_status & CP_DONE)) {
			if (g_cp_charging.cp_chg_status & CP_REENTER) {
				g_cp_charging.cp_chg_status &= ~CP_REENTER;
				cancel_work_sync(&__pdpm->usb_psy_change_work);
				schedule_work(&__pdpm->usb_psy_change_work);
				chr_err("CP Reenter!\n");
			}
			charger_dev_enable(info->chg1_dev, false);
			chr_err("CP charging!\n");
			return 0;
		} else { //when cp done, keep cp exit
			g_cp_charging.cp_chg_status |= CP_EXIT;
		}
		info->leave_pdc = 1; //doesn't enter PDC when CP not charged
	}
#endif

	if (pdc_is_ready() &&
		!info->leave_pdc) {
		if (info->enable_hv_charging == true) {
			chr_err("enter PDC!\n");
			swchgalg->state = CHR_PDC;
			if (mtk_pe20_get_is_enable(info)) {
				mtk_pe20_set_is_enable(info, false);
				if (mtk_pe20_get_is_connect(info))
					mtk_pe20_reset_ta_vchr(info);
			}

			if (mtk_pe_get_is_enable(info)) {
				mtk_pe_set_is_enable(info, false);
				if (mtk_pe_get_is_connect(info))
					mtk_pe_reset_ta_vchr(info);
			}
			return 1;
		}
	}

	swchg_turn_on_charging(info);

	charger_dev_is_charging_done(info->chg1_dev, &g_chg_done);
//zhaosidong.wt, SOC is mantained for a long time
	fg_daemon_init = battery_get_fg_init_done();
	chr_err("g_chg_done:%d fg_daemon_init:%d!\n", g_chg_done, fg_daemon_init);
	if (g_chg_done && fg_daemon_init) {
		swchgalg->state = CHR_BATFULL;
		charger_dev_do_event(info->chg1_dev, EVENT_EOC, 0);
		chr_err("battery full!\n");
	}

	/* If it is not disabled by throttling,
	 * enable PE+/PE+20, if it is disabled
	 */
	if (info->chg1_data.thermal_input_current_limit != -1 &&
		info->chg1_data.thermal_input_current_limit < 300)
		return 0;
#ifdef CONFIG_AFC_CHARGER
/* yuanjian.wt add for AFC */
	if (!afc_get_is_enable(info)) {
		afc_set_is_enable(info, true);
		afc_set_to_check_chr_type(info, true);
	}
#endif
	if (!mtk_pe20_get_is_enable(info)) {
		mtk_pe20_set_is_enable(info, true);
		mtk_pe20_set_to_check_chr_type(info, true);
	}

	if (!mtk_pe_get_is_enable(info)) {
		mtk_pe_set_is_enable(info, true);
		mtk_pe_set_to_check_chr_type(info, true);
	}
	return 0;
}

static int mtk_switch_chr_err(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;

	if (info->enable_sw_jeita) {
		if ((info->sw_jeita.sm == TEMP_BELOW_T0) ||
			(info->sw_jeita.sm == TEMP_ABOVE_T4))
			info->sw_jeita.error_recovery_flag = false;

		if ((info->sw_jeita.error_recovery_flag == false) &&
			(info->sw_jeita.sm != TEMP_BELOW_T0) &&
			(info->sw_jeita.sm != TEMP_ABOVE_T4)) {
			info->sw_jeita.error_recovery_flag = true;
			swchgalg->state = CHR_CC;
			get_monotonic_boottime(&swchgalg->charging_begin_time);
		}
	}

	swchgalg->total_charging_time = 0;

	_disable_all_charging(info);
	return 0;
}

static int mtk_switch_chr_full(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
//+P240131-05273 liwei19.wt 20240306,one ui 6.1 charging protection
#if defined (CONFIG_W2_CHARGER_PRIVATE) || defined (CONFIG_N28_CHARGER_PRIVATE)
	int uisoc = 0;
#endif
//-P240131-05273 liwei19.wt 20240306,one ui 6.1 charging protection
	swchgalg->total_charging_time = 0;

	/* turn off LED */

	/*
	 * If CV is set to lower value by JEITA,
	 * Reset CV to normal value if temperture is in normal zone
	 */
#ifdef CONFIG_AFC_CHARGER
#if defined(CONFIG_W2_CHARGER_PRIVATE)
	if (wt_limit_afc_ibus) {
		info->chg1_data.input_current_limit = 1200000;
		charger_dev_set_input_current(info->chg1_dev,
		info->chg1_data.input_current_limit);
	}
#endif
#endif
	swchg_select_cv(info);
	info->polling_interval = CHARGING_FULL_INTERVAL;
//+P240329 guhan01.wt 20240412,one ui 6.1 charging protection not recharge
	charger_dev_is_charging_done(info->chg1_dev, &g_chg_done);
#if defined (CONFIG_W2_CHARGER_PRIVATE) || defined (CONFIG_N28_CHARGER_PRIVATE)
	if ((batt_soc_rechg == 1) && (batt_mode == 0) && (batt_full_capacity == 100)) {
		uisoc = battery_get_uisoc();
		if(uisoc > 95) {
			chr_err("It's basic mode. Soc is greater than 95%, then it does not rechg!\n");
			return 0;
		}
	}
#endif
//-P240329 guhan01.wt 20240412,one ui 6.1 charging protection not recharge

	if (!g_chg_done) {
		swchgalg->state = CHR_CC;
		charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
#ifdef CONFIG_AFC_CHARGER
		/* yuanjian.wt add for AFC */
		afc_set_to_check_chr_type(info, true);
#endif
		mtk_pe20_set_to_check_chr_type(info, true);
		mtk_pe_set_to_check_chr_type(info, true);
		info->enable_dynamic_cv = true;
		get_monotonic_boottime(&swchgalg->charging_begin_time);
		chr_err("battery recharging!\n");
		info->polling_interval = CHARGING_INTERVAL;
	}

	return 0;
}

static int mtk_switch_charging_current(struct charger_manager *info)
{
	swchg_select_charging_current_limit(info);
	return 0;
}

static int mtk_switch_charging_run(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	int ret = 0;

	chr_err("%s [%d %d], timer=%d\n", __func__, swchgalg->state,
		info->pd_type,
		swchgalg->total_charging_time);

#ifndef CONFIG_N28_CHARGER_PRIVATE
	if (swchgalg->total_charging_time == 0 && swchgalg->state == CHR_CC && swchgalg->disable_charging == false) {
		first_select_charging_current_limit(info);
	}
#endif

	if (mtk_pdc_check_charger(info) == false &&
	    mtk_is_TA_support_pd_pps(info) == false) {
#ifdef CONFIG_AFC_CHARGER
		chr_err("afc check before2\n");
		if (info->chr_type == STANDARD_CHARGER) {
#if defined(CONFIG_W2_CHARGER_PRIVATE)
			if ((!info->enable_hv_charging) && (info->is_camera_on)) {// && !wt_limit_afc_ibus) {
				pr_err("%s: set ibus for afc\n", __func__);
				info->chg1_data.input_current_limit = 500000;
				charger_dev_set_input_current(info->chg1_dev,
				info->chg1_data.input_current_limit);
				wt_limit_afc_ibus = true;
				msleep(30);
			} else if (!info->is_camera_on) {
				wt_limit_afc_ibus = false;
				pr_err("%s: do not limit ibus for afc\n", __func__);
			}
#endif
			afc_check_charger(info);
		}
		if (afc_get_is_connect(info) == false)
		    mtk_pe20_check_charger(info);
#else
		mtk_pe20_check_charger(info);
#endif
		if (mtk_pe20_get_is_connect(info) == false)
			mtk_pe_check_charger(info);
	}

	do {
		chr_err("%s_1 [%d] %d\n", __func__, swchgalg->state,info->pd_type);
		switch (swchgalg->state) {
		case CHR_CC:
			ret = mtk_switch_chr_cc(info);
			break;

		case CHR_PE50:
			ret = mtk_switch_chr_pe50_run(info);
			break;

		case CHR_PE40:
			ret = mtk_switch_chr_pe40_run(info);
			break;

		case CHR_PDC:
			ret = mtk_switch_chr_pdc_run(info);
			break;

		case CHR_BATFULL:
			ret = mtk_switch_chr_full(info);
			break;

		case CHR_ERROR:
			ret = mtk_switch_chr_err(info);
			break;
		}
	} while (ret != 0);
	mtk_switch_check_charging_time(info);

	charger_dev_dump_registers(info->chg1_dev);
	return 0;
}

static int charger_dev_event(struct notifier_block *nb,
	unsigned long event, void *v)
{
	struct charger_manager *info =
			container_of(nb, struct charger_manager, chg1_nb);
	struct chgdev_notify *data = v;

	chr_info("%s %ld", __func__, event);

	switch (event) {
	case CHARGER_DEV_NOTIFY_EOC:
		charger_manager_notifier(info, CHARGER_NOTIFY_EOC);
		pr_info("%s: end of charge\n", __func__);
		break;
	case CHARGER_DEV_NOTIFY_RECHG:
		charger_manager_notifier(info, CHARGER_NOTIFY_START_CHARGING);
		pr_info("%s: recharge\n", __func__);
		break;
	case CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT:
		info->safety_timeout = true;
		chr_err("%s: safety timer timeout\n", __func__);

		/* If sw safety timer timeout, do not wake up charger thread */
		if (info->enable_sw_safety_timer)
			return NOTIFY_DONE;
		break;
	case CHARGER_DEV_NOTIFY_VBUS_OVP:
		info->vbusov_stat = data->vbusov_stat;
		chr_err("%s: vbus ovp = %d\n", __func__, info->vbusov_stat);
		break;
	default:
		return NOTIFY_DONE;
	}

	if (info->chg1_dev->is_polling_mode == false)
		_wake_up_charger(info);

	return NOTIFY_DONE;
}

static int dvchg1_dev_event(struct notifier_block *nb, unsigned long event,
			    void *data)
{
	struct charger_manager *info =
			container_of(nb, struct charger_manager, dvchg1_nb);

	chr_info("%s %ld", __func__, event);

	return mtk_pe50_notifier_call(info, MTK_PE50_NOTISRC_CHG, event, data);
}

static int dvchg2_dev_event(struct notifier_block *nb, unsigned long event,
			    void *data)
{
	struct charger_manager *info =
			container_of(nb, struct charger_manager, dvchg2_nb);

	chr_info("%s %ld", __func__, event);

	return mtk_pe50_notifier_call(info, MTK_PE50_NOTISRC_CHG, event, data);
}

int mtk_switch_charging_init2(struct charger_manager *info)
{
	struct switch_charging_alg_data *swch_alg;

	swch_alg = devm_kzalloc(&info->pdev->dev,
				sizeof(*swch_alg), GFP_KERNEL);
	if (!swch_alg)
		return -ENOMEM;

	info->chg1_dev = get_charger_by_name("primary_chg");
	if (info->chg1_dev)
		chr_err("Found primary charger [%s]\n",
			info->chg1_dev->props.alias_name);
	else
		chr_err("*** Error : can't find primary charger ***\n");

	info->dvchg1_dev = get_charger_by_name("primary_divider_chg");
	if (info->dvchg1_dev) {
		chr_err("Found primary divider charger [%s]\n",
			info->dvchg1_dev->props.alias_name);
		info->dvchg1_nb.notifier_call = dvchg1_dev_event;
		register_charger_device_notifier(info->dvchg1_dev,
						 &info->dvchg1_nb);
	} else
		chr_err("Can't find primary divider charger\n");
	info->dvchg2_dev = get_charger_by_name("secondary_divider_chg");
	if (info->dvchg2_dev) {
		chr_err("Found secondary divider charger [%s]\n",
			info->dvchg2_dev->props.alias_name);
		info->dvchg2_nb.notifier_call = dvchg2_dev_event;
		register_charger_device_notifier(info->dvchg2_dev,
						 &info->dvchg2_nb);
	} else
		chr_err("Can't find secondary divider charger\n");

	mutex_init(&swch_alg->ichg_aicr_access_mutex);

	info->algorithm_data = swch_alg;
	info->do_algorithm = mtk_switch_charging_run;
	info->plug_in = mtk_switch_charging_plug_in;
	info->plug_out = mtk_switch_charging_plug_out;
	info->do_charging = mtk_switch_charging_do_charging;
	info->do_event = charger_dev_event;
	info->change_current_setting = mtk_switch_charging_current;

	mtk_switch_chr_pe50_init(info);
	mtk_switch_chr_pe40_init(info);
	mtk_switch_chr_pdc_init(info);

	return 0;
}
