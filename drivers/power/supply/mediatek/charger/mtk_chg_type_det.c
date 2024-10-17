/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

#include <generated/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/seq_file.h>
#include <linux/power_supply.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/reboot.h>

#include <linux/of.h>
#include <linux/extcon.h>

#include <mt-plat/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <mt-plat/mtk_boot.h>
#include <mt-plat/v1/charger_type.h>
#include <mt-plat/v1/mtk_charger.h>
#include <pmic.h>
#include <tcpm.h>
#include <tcpci_core.h>

#include "mtk_charger_intf.h"
#ifdef CONFIG_MT6360_PMU_CHARGER
extern bool mt6360_get_is_host(void);
#endif

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

#ifdef CONFIG_EXTCON_USB_CHG
struct usb_extcon_info {
	struct device *dev;
	struct extcon_dev *edev;

	unsigned int vbus_state;
	unsigned long debounce_jiffies;
	struct delayed_work wq_detcable;
};

static const unsigned int usb_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};
#endif

void __attribute__((weak)) fg_charger_in_handler(void)
{
	pr_notice("%s not defined\n", __func__);
}

struct chg_type_info {
	struct device *dev;
	struct charger_consumer *chg_consumer;
	struct tcpc_device *tcpc;
	struct notifier_block pd_nb;
	bool tcpc_kpoc;
	/* Charger Detection */
	struct mutex chgdet_lock;
	bool chgdet_en;
	atomic_t chgdet_cnt;
	wait_queue_head_t waitq;
	struct task_struct *chgdet_task;
	struct workqueue_struct *pwr_off_wq;
	struct work_struct pwr_off_work;
	struct workqueue_struct *chg_in_wq;
	struct work_struct chg_in_work;
	bool ignore_usb;
	bool plugin;
	bool bypass_chgdet;
#ifdef CONFIG_MTK_TYPEC_WATER_DETECT
	bool water_detected;
#endif
#ifdef CONFIG_MACH_MT6771
	struct power_supply *chr_psy;
	struct notifier_block psy_nb;
#endif
};

#ifdef CONFIG_FPGA_EARLY_PORTING
/*  FPGA */
int hw_charging_get_charger_type(void)
{
	return STANDARD_HOST;
}

#else

/* EVB / Phone */
static const char * const mtk_chg_type_name[] = {
	"Charger Unknown",
	"Standard USB Host",
	"Charging USB Host",
	"Non-standard Charger",
	"Standard Charger",
	"Apple 2.4A Charger",
	"Apple 2.1A Charger",
	"Apple 1.0A Charger",
	"Apple 0.5A Charger",
	"Wireless Charger",
	"Samsung Charger",
};

static void dump_charger_name(enum charger_type type)
{
	switch (type) {
	case CHARGER_UNKNOWN:
	case STANDARD_HOST:
	case CHARGING_HOST:
	case NONSTANDARD_CHARGER:
	case STANDARD_CHARGER:
	case APPLE_2_4A_CHARGER:
	case APPLE_2_1A_CHARGER:
	case APPLE_1_0A_CHARGER:
	case APPLE_0_5A_CHARGER:
	case SAMSUNG_CHARGER:
		pr_info("%s: charger type: %d, %s\n", __func__, type,
			mtk_chg_type_name[type]);
		break;
	default:
		pr_info("%s: charger type: %d, Not Defined!!!\n", __func__,
			type);
		break;
	}
}

/* Power Supply */
struct mt_charger {
	struct device *dev;
	struct power_supply_desc chg_desc;
	struct power_supply_config chg_cfg;
	struct power_supply *chg_psy;
	struct power_supply_desc ac_desc;
	struct power_supply_config ac_cfg;
	struct power_supply *ac_psy;
	struct power_supply_desc usb_desc;
	struct power_supply_config usb_cfg;
	struct power_supply *usb_psy;
/* hs14 code for SR-AL6528A-01-245 by wenyaqi at 2022/10/02 start */
#ifndef HQ_FACTORY_BUILD	//ss version
	struct power_supply_desc otg_desc;
	struct power_supply_config otg_cfg;
	struct power_supply *otg_psy;
#endif
/* hs14 code for SR-AL6528A-01-245 by wenyaqi at 2022/10/02 end */
	struct chg_type_info *cti;
	#ifdef CONFIG_EXTCON_USB_CHG
	struct usb_extcon_info *extcon_info;
	struct delayed_work extcon_work;
	#endif
	bool chg_online; /* Has charger in or not */
	enum charger_type chg_type;

	/* hs14 code for SR-AL6528A-01-299 by gaozhengwei at 2022/09/02 start */
	bool input_suspend;
	/* hs14 code for SR-AL6528A-01-299 by gaozhengwei at 2022/09/02 end */

	/* hs14 code for  SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 start*/
	bool shipmode_flag;
	/* hs14 code for  SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 end*/
	/* HS04_U/HS14_U/TabA7 Lite U for P231128-06029 by liufurong at 20231204 start */
	int batt_slate_mode;
	/* HS04_U/HS14_U/TabA7 Lite U for P231128-06029 by liufurong at 20231204 end */
};

static int mt_charger_online(struct mt_charger *mtk_chg)
{
	int ret = 0;
	struct device *dev = NULL;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;
	int boot_mode = 11;//UNKNOWN_BOOT
#ifdef CONFIG_KPOC_GET_SOURCE_CAP_TRY
	struct chg_type_info *cti = mtk_chg->cti;
#endif
	dev = mtk_chg->dev;
	if (dev != NULL){
		boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
		if (!boot_node){
			chr_err("%s: failed to get boot mode phandle\n", __func__);
		}
		else {
			tag = (struct tag_bootmode *)of_get_property(boot_node,
								"atag,boot", NULL);
			if (!tag){
				chr_err("%s: failed to get atag,boot\n", __func__);
			}
			else
				boot_mode = tag->bootmode;
		}
	}

	if (!mtk_chg->chg_online) {
// workaround for mt6768
		//boot_mode = get_boot_mode();
		if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT ||
		    boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
			pr_notice("%s: Unplug Charger/USB\n", __func__);
/* hs14 code for AL6528ADEU-342 by wenyaqi at 2022/10/11 start */
#if defined(CONFIG_HQ_PROJECT_O22)
				pr_notice("%s: system_state=%d\n", __func__,
					system_state);
				if (system_state != SYSTEM_POWER_OFF)
					return ret;
#endif
/* hs14 code for AL6528ADEU-342 by wenyaqi at 2022/10/11 end */
#ifdef CONFIG_KPOC_GET_SOURCE_CAP_TRY
			pr_info("%s error_recovery_once = %d\n", __func__,
				cti->tcpc->pd_port.error_recovery_once);
			if (cti->tcpc->pd_port.error_recovery_once != 1) {
#endif /*CONFIG_KPOC_GET_SOURCE_CAP_TRY*/
				pr_notice("%s: system_state=%d\n", __func__,
					system_state);
				if (system_state != SYSTEM_POWER_OFF)
					kernel_power_off();
#ifdef CONFIG_KPOC_GET_SOURCE_CAP_TRY
			}
#endif
		}
	}

	return ret;
}

/* Power Supply Functions */
static int mt_charger_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);
	/* hs14 code for SR-AL6528A-445 by shanxinkai at 2022/10/28 start */
	int ret;
	/* hs14 code for SR-AL6528A-445 by shanxinkai at 2022/10/28 end */

/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
#ifdef CONFIG_AFC_CHARGER
	struct charger_manager *info = ssinfo;
#endif
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */

/* hs14 code for  SR-AL6528A-01-339 by shanxinkai at 2022/09/30 start*/
	int chr_type = 0;
/* hs14 code for  SR-AL6528A-01-339 by shanxinkai at 2022/09/30 end*/

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 0;
		/* Force to 1 in all charger type */
		if (mtk_chg->chg_type != CHARGER_UNKNOWN)
			val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = mtk_chg->chg_type;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		switch (mtk_chg->chg_type) {
		case STANDARD_HOST:
			val->intval = POWER_SUPPLY_USB_TYPE_SDP;
			pr_info("%s: Charger Type: STANDARD_HOST\n", __func__);
			break;
		case NONSTANDARD_CHARGER:
			val->intval = POWER_SUPPLY_USB_TYPE_DCP;
			pr_info("%s: Charger Type: NONSTANDARD_CHARGER\n", __func__);
			break;
		case CHARGING_HOST:
			val->intval = POWER_SUPPLY_USB_TYPE_CDP;
			pr_info("%s: Charger Type: CHARGING_HOST\n", __func__);
			break;
		case STANDARD_CHARGER:
			val->intval = POWER_SUPPLY_USB_TYPE_DCP;
			pr_info("%s: Charger Type: STANDARD_CHARGER\n", __func__);
			break;
		case CHARGER_UNKNOWN:
			val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
			pr_info("%s: Charger Type: CHARGER_UNKNOWN\n", __func__);
			break;
		default:
	/* AL6528A code for SR-AL6528A-01-303 by wenyaqi at 2022/08/31 start */
			break;
		}
		break;
	/* AL6528A code for SR-AL6528A-01-303 by wenyaqi at 2022/08/31 end */
/* hs14 code for SR-AL6528A-01-242 by shanxinkai at 2022/10/12 start */
#ifndef HQ_FACTORY_BUILD	//ss version
	case POWER_SUPPLY_PROP_BATT_SLATE_MODE:
	/* HS04_U/HS14_U/TabA7 Lite U for P231128-06029 by liufurong at 20231204 start */
		val->intval = mtk_chg->batt_slate_mode;
		break;
	/* HS04_U/HS14_U/TabA7 Lite U for P231128-06029 by liufurong at 20231204 end */
#endif
/* hs14 code for SR-AL6528A-01-242 by shanxinkai at 2022/10/12 end */
	/* hs14 code for SR-AL6528A-01-299 by gaozhengwei at 2022/09/02 start */
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		val->intval = mtk_chg->input_suspend;
		break;
	/* hs14 code for SR-AL6528A-01-299 by gaozhengwei at 2022/09/02 end */

	/* hs14 code for  SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 start*/
	case POWER_SUPPLY_PROP_SHIPMODE:
		val->intval = mtk_chg->shipmode_flag;
		break;
	case POWER_SUPPLY_PROP_SHIPMODE_REG:
	/* hs14 code for  SR-AL6528A-01-259 by zhouyuhang at 2022/09/28 start*/
		if (mtk_chg->cti == NULL)
			return -EINVAL;
	/* hs14 code for  SR-AL6528A-01-259 by zhouyuhang at 2022/09/28 end*/
		val->intval = charger_manager_get_shipmode(mtk_chg->cti->chg_consumer, MAIN_CHARGER);
		pr_err("get ship mode reg = %d\n", val->intval);
		break;
	/* hs14 code for  SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 end*/

	/* hs14 code for  SR-AL6528A-01-339 by shanxinkai at 2022/09/30 start*/
	case POWER_SUPPLY_PROP_CHARGE_STATUS:
		if (mtk_chg->cti == NULL)
			return -EINVAL;
		charger_manager_get_chr_type(mtk_chg->cti->chg_consumer, MAIN_CHARGER, &chr_type);
		val->intval = chr_type;
		break;
	/* hs14 code for  SR-AL6528A-01-339 by shanxinkai at 2022/09/30 end*/
	/* hs14 code for SR-AL6528A-445 by shanxinkai at 2022/10/28 start */
	case POWER_SUPPLY_PROP_DUMP_CHARGER_IC:
		if (mtk_chg->cti == NULL)
			return -EINVAL;
		ret = charger_manager_dump_charger_ic(mtk_chg->cti->chg_consumer, MAIN_CHARGER);
		if (ret < 0) {
			val->intval = 0;
		} else {
			val->intval = 1;
		}
		break;
	/* hs14 code for SR-AL6528A-445 by shanxinkai at 2022/10/28 end */
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
#ifdef CONFIG_AFC_CHARGER
	case POWER_SUPPLY_PROP_HV_CHARGER_STATUS:
		if (info == NULL)
			return -EINVAL;
		if (info->hv_disable)
			val->intval = 0;
		else {
			if (ss_fast_charger_status(info))
				val->intval = 1;
			else
				val->intval = 0;
		}
		break;
	case POWER_SUPPLY_PROP_AFC_RESULT:
		if (info == NULL)
			return -EINVAL;
		if (info->afc_sts >= AFC_5V)
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_HV_DISABLE:
		if (info == NULL)
			return -EINVAL;
		val->intval = info->hv_disable;
		break;
#endif
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */
#ifndef HQ_FACTORY_BUILD

	/* hs14 code for SR-AL6528A-01-244 by shanxinkai at 2022/11/04 start */
	case POWER_SUPPLY_PROP_STORE_MODE:
		if (info == NULL) {
			return -EINVAL;
		}
		val->intval = info->store_mode;
		break;
	/* hs14 code for SR-AL6528A-01-244 by shanxinkai at 2022/11/04 end */
#endif
/* hs14 code for SR-AL6528A-01-244 by shanxinkai at 2022/11/04 start */
#ifdef HQ_FACTORY_BUILD
	case POWER_SUPPLY_PROP_BATT_CAP_CONTROL:
		val->intval = info->batt_cap_control;
		break;
#endif
/* hs14 code for SR-AL6528A-01-244 by shanxinkai at 2022/11/04 end */
	default:
		return -EINVAL;
	}

	return 0;
}

/* hs14 code for SR-AL6528A-01-299 by gaozhengwei at 2022/09/02 start */
static int mt_charger__property_is_writeable(struct power_supply *psy,
					       enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		return 1;
	/* hs14 code for  SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 start*/
	case POWER_SUPPLY_PROP_SHIPMODE:
		return 1;
	/* hs14 code for  SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 end*/
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
#ifdef CONFIG_AFC_CHARGER
	case POWER_SUPPLY_PROP_AFC_RESULT:
		return 1;
	case POWER_SUPPLY_PROP_HV_DISABLE:
		return 1;
#endif
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */
/* hs14 code for SR-AL6528A-01-242 by shanxinkai at 2022/10/12 start */
#ifndef HQ_FACTORY_BUILD	//ss version
	case POWER_SUPPLY_PROP_BATT_SLATE_MODE:
		return 1;
	/* hs14 code for SR-AL6528A-01-244 by shanxinkai at 2022/11/04 start */
	case POWER_SUPPLY_PROP_STORE_MODE:
		return 1;
	/* hs14 code for SR-AL6528A-01-244 by shanxinkai at 2022/11/04 end */
#endif
/* hs14 code for SR-AL6528A-01-242 by shanxinkai at 2022/10/12 end */
/* hs14 code for SR-AL6528A-01-244 by shanxinkai at 2022/11/04 start */
#ifdef HQ_FACTORY_BUILD
	case POWER_SUPPLY_PROP_BATT_CAP_CONTROL:
		return 1;
#endif
/* hs14 code for SR-AL6528A-01-244 by shanxinkai at 2022/11/04 end */
	default:
		return 0;
	}
}
/* hs14 code for SR-AL6528A-01-299 by gaozhengwei at 2022/09/02 end */

#ifdef CONFIG_EXTCON_USB_CHG
static void usb_extcon_detect_cable(struct work_struct *work)
{
	struct usb_extcon_info *info = container_of(to_delayed_work(work),
						struct usb_extcon_info,
						wq_detcable);

	/* check and update cable state */
	if (info->vbus_state)
		extcon_set_state_sync(info->edev, EXTCON_USB, true);
	else
		extcon_set_state_sync(info->edev, EXTCON_USB, false);
}
#endif
/* HS14_U/TabA7 Lite U for AL6528AU-249/AX3565AU-309 by liufurong at 20231212 start */
extern void pd_dpm_send_source_caps_switch(int cur);
extern int fusb302_send_5v_source_caps(int ma);
/* HS14_U/TabA7 Lite U for AL6528AU-249/AX3565AU-309 by liufurong at 20231212 end */
static int mt_charger_set_property(struct power_supply *psy,
	enum power_supply_property psp, const union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);
	struct chg_type_info *cti = NULL;
	#ifdef CONFIG_EXTCON_USB_CHG
	struct usb_extcon_info *info;
	#endif
	bool is_host = 0;

/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
#ifdef CONFIG_AFC_CHARGER
	struct charger_manager *pinfo = ssinfo;
#endif
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */

	pr_info("%s\n", __func__);

	if (!mtk_chg) {
		pr_notice("%s: no mtk chg data\n", __func__);
		return -EINVAL;
	}

#ifdef CONFIG_EXTCON_USB_CHG
	info = mtk_chg->extcon_info;
#endif

	cti = mtk_chg->cti;
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		mtk_chg->chg_online = val->intval;
		mt_charger_online(mtk_chg);
		return 0;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		mtk_chg->chg_type = val->intval;
		if (mtk_chg->chg_type != CHARGER_UNKNOWN)
			charger_manager_force_disable_power_path(
				cti->chg_consumer, MAIN_CHARGER, false);
		else if (!cti->tcpc_kpoc)
			charger_manager_force_disable_power_path(
				cti->chg_consumer, MAIN_CHARGER, true);
		break;
/* hs14 code for SR-AL6528A-01-242 by shanxinkai at 2022/10/12 start */
#ifndef HQ_FACTORY_BUILD	//ss version
	case POWER_SUPPLY_PROP_BATT_SLATE_MODE:
	/* HS04_U/HS14_U/TabA7 Lite U for P231128-06029 by liufurong at 20231204 start */
		if (pinfo == NULL) {
			return -EINVAL;
		}
		/* HS14_U/TabA7 Lite U for AL6528AU-249/AX3565AU-309 by liufurong at 20231212 start */
		if (val->intval == SEC_SLATE_OFF || val->intval == SEC_SLATE_MODE) {
			mtk_chg->input_suspend = val->intval;
			pinfo->input_suspend_flag = mtk_chg->input_suspend;
			pr_err("%s: set input_suspend value = %d\n",__func__, pinfo->input_suspend_flag);
			if (val->intval) {
				charger_manager_input_suspend(cti->chg_consumer,
					MAIN_CHARGER,
					true);
			} else {
				charger_manager_input_suspend(cti->chg_consumer,
					MAIN_CHARGER,
					false);
				if (mtk_chg->batt_slate_mode == SEC_SMART_SWITCH_SRC || mtk_chg->batt_slate_mode == SEC_SMART_SWITCH_SLATE) {
					if (tcpc_info == FUSB302) {
						fusb302_send_5v_source_caps(1000);
					} else {
						pd_dpm_send_source_caps_switch(1000);
					}
				}
			}
		} else if (val->intval == SEC_SMART_SWITCH_SLATE) {
			if (tcpc_info == FUSB302) {
				fusb302_send_5v_source_caps(500);
			} else {
				pd_dpm_send_source_caps_switch(500);
			}
			pr_err("%s:  dont suppot this slate mode %d\n", __func__, val->intval);
		} else if (val->intval == SEC_SMART_SWITCH_SRC) {
			if (tcpc_info == FUSB302) {
				fusb302_send_5v_source_caps(0);
			} else {
				pd_dpm_send_source_caps_switch(0);
			}
			pr_err("%s:  dont suppot this slate mode %d\n", __func__, val->intval);
		}
		/* HS14_U/TabA7 Lite U for AL6528AU-249/AX3565AU-309 by liufurong at 20231212 end */
		mtk_chg->batt_slate_mode = val->intval;;
		pr_err("%s:  set slate mode %d\n", __func__, val->intval);
		break;
	/* HS04_U/HS14_U/TabA7 Lite U for P231128-06029 by liufurong at 20231204 end */
#endif
/* hs14 code for SR-AL6528A-01-242 by shanxinkai at 2022/10/12 end */
	/* hs14 code for SR-AL6528A-01-299 by gaozhengwei at 2022/09/02 start */
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		/* hs14 code for SR-AL6528A-01-244 by qiaodan at 2022/11/23 start */
		if (pinfo == NULL) {
			return -EINVAL;
		}
		mtk_chg->input_suspend = val->intval;
		pinfo->input_suspend_flag = mtk_chg->input_suspend;
		pr_err("%s: set input_suspend value = %d\n",__func__, pinfo->input_suspend_flag);
		/* hs14 code for SR-AL6528A-01-244 by qiaodan at 2022/11/23 end */
		if (val->intval)
			charger_manager_input_suspend(cti->chg_consumer,
				MAIN_CHARGER,
				true);
		else
			charger_manager_input_suspend(cti->chg_consumer,
				MAIN_CHARGER,
				false);
		break;
	/* hs14 code for SR-AL6528A-01-299 by gaozhengwei at 2022/09/02 end */

	/* hs14 code for  SR-AL6528A-01-259 by qiaodan at 2022/10/17 start*/
	case POWER_SUPPLY_PROP_SHIPMODE:
		mtk_chg->shipmode_flag = val->intval;
		if (mtk_chg->cti == NULL) {
			return -EINVAL;
		}
		charger_manager_set_shipmode(mtk_chg->cti->chg_consumer, MAIN_CHARGER);
		pr_err("%s: enable shipmode val = %d\n",__func__, val->intval);
		break;
	/* hs14 code for  SR-AL6528A-01-259 by qiaodan at 2022/10/17 end*/
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
#ifdef CONFIG_AFC_CHARGER
	case POWER_SUPPLY_PROP_AFC_RESULT:
		if (pinfo == NULL)
			return -EINVAL;
		is_afc_result(pinfo, val->intval);
		break;
	case POWER_SUPPLY_PROP_HV_DISABLE:
		if (pinfo == NULL)
			return -EINVAL;
		pinfo->hv_disable = val->intval;
		break;
#endif
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */
#ifndef HQ_FACTORY_BUILD
	/* hs14 code for SR-AL6528A-01-244 by qiaodan at 2022/11/23 start */
	case POWER_SUPPLY_PROP_STORE_MODE:
		if (pinfo == NULL) {
			return -EINVAL;
		}
		pinfo->store_mode = val->intval;
		break;
	/* hs14 code for SR-AL6528A-01-244 by qiaodan at 2022/11/23 end */
#endif
/* hs14 code for SR-AL6528A-01-244 by qiaodan at 2022/11/23 start */
#ifdef HQ_FACTORY_BUILD
	case POWER_SUPPLY_PROP_BATT_CAP_CONTROL:
		if (pinfo == NULL) {
			return -EINVAL;
		}
		if (val->intval)
			pinfo->batt_cap_control = true;
		else
			pinfo->batt_cap_control = false;
		break;
#endif
/* hs14 code for SR-AL6528A-01-244 by shanxinkai at 2022/11/23 end */
	default:
		return -EINVAL;
	}

	dump_charger_name(mtk_chg->chg_type);

#ifdef CONFIG_MT6360_PMU_CHARGER
	is_host = mt6360_get_is_host();
#endif

	if (!cti->ignore_usb && !is_host) {
		/* usb */
		if ((mtk_chg->chg_type == STANDARD_HOST) ||
			(mtk_chg->chg_type == CHARGING_HOST) ||
			(mtk_chg->chg_type == NONSTANDARD_CHARGER)) {
				mt_usb_connect_v1();
			#ifdef CONFIG_EXTCON_USB_CHG
			info->vbus_state = 1;
			#endif
		} else {
			mt_usb_disconnect_v1();
			#ifdef CONFIG_EXTCON_USB_CHG
			info->vbus_state = 0;
			#endif
		}
	}

	queue_work(cti->chg_in_wq, &cti->chg_in_work);
	#ifdef CONFIG_EXTCON_USB_CHG
	if (!IS_ERR(info->edev))
		queue_delayed_work(system_power_efficient_wq,
			&info->wq_detcable, info->debounce_jiffies);
	#endif

#ifdef CONFIG_MACH_MT6771
	power_supply_changed(mtk_chg->chg_psy);
#endif
	power_supply_changed(mtk_chg->ac_psy);
	power_supply_changed(mtk_chg->usb_psy);

	return 0;
}

static int mt_ac_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 0;
		/* Force to 1 in all charger type */
		if (mtk_chg->chg_type != CHARGER_UNKNOWN)
			val->intval = 1;
		/* Reset to 0 if charger type is USB */
		if ((mtk_chg->chg_type == STANDARD_HOST) ||
			(mtk_chg->chg_type == CHARGING_HOST))
			val->intval = 0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mt_usb_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);
	/* hs14 code for SR-AL6528A-01-300 by wenyaqi at 2022/09/08 start */
	struct power_supply *typec_port = NULL;
	/* hs14 code for SR-AL6528A-01-300 by wenyaqi at 2022/09/08 end */

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if ((mtk_chg->chg_type == STANDARD_HOST) ||
			(mtk_chg->chg_type == CHARGING_HOST))
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = 500000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = 5000000;
		break;
	/* AL6528A code for SR-AL6528A-01-301 by gaozhengwei at 2022/08/31 start */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = battery_get_vbus();;
		break;
	/* AL6528A code for SR-AL6528A-01-301 by gaozhengwei at 2022/08/31 end */
	/* hs14 code for SR-AL6528A-01-300 by wenyaqi at 2022/09/08 start */
	case POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION:
		typec_port = power_supply_get_by_name("typec_port");
		if (!typec_port) {
			return -EINVAL;
			break;
		}
		power_supply_get_property(typec_port, POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION, val);
		break;
	/* hs14 code for SR-AL6528A-01-300 by wenyaqi at 2022/09/08 end */
	default:
		return -EINVAL;
	}

	return 0;
}

/* hs14 code for SR-AL6528A-01-245 by wenyaqi at 2022/10/02 start */
#ifndef HQ_FACTORY_BUILD	//ss version
extern bool ss_musb_is_host(void);
static int mt_otg_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = ss_musb_is_host();
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
#endif
/* hs14 code for SR-AL6528A-01-245 by wenyaqi at 2022/10/02 end */

static enum power_supply_property mt_charger_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	/* hs14 code for SR-AL6528A-01-299 by gaozhengwei at 2022/09/02 start */
	POWER_SUPPLY_PROP_INPUT_SUSPEND,
	/* hs14 code for SR-AL6528A-01-299 by gaozhengwei at 2022/09/02 end */

	/* hs14 code for SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 start*/
	POWER_SUPPLY_PROP_SHIPMODE,
	POWER_SUPPLY_PROP_SHIPMODE_REG,
	/* hs14 code for  SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 end*/
	/* hs14 code for SR-AL6528A-445 by shanxinkai at 2022/10/28 start */
	POWER_SUPPLY_PROP_DUMP_CHARGER_IC,
	/* hs14 code for SR-AL6528A-445 by shanxinkai at 2022/10/28 end */
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
#ifdef CONFIG_AFC_CHARGER
	POWER_SUPPLY_PROP_HV_CHARGER_STATUS,
	POWER_SUPPLY_PROP_AFC_RESULT,
	POWER_SUPPLY_PROP_HV_DISABLE,
#endif
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */
#ifndef HQ_FACTORY_BUILD
/* hs14 code for SR-AL6528A-01-242 by shanxinkai at 2022/10/12 start */
	POWER_SUPPLY_PROP_BATT_SLATE_MODE,
/* hs14 code for SR-AL6528A-01-242 by shanxinkai at 2022/10/12 end */
/* hs14 code for SR-AL6528A-01-244 by shanxinkai at 2022/11/04 start */
	POWER_SUPPLY_PROP_STORE_MODE,
/* hs14 code for SR-AL6528A-01-244 by shanxinkai at 2022/11/04 end */
#endif
/* hs14 code for SR-AL6528A-01-244 by shanxinkai at 2022/11/04 start */
#ifdef HQ_FACTORY_BUILD
	POWER_SUPPLY_PROP_BATT_CAP_CONTROL,
#endif
/* hs14 code for SR-AL6528A-01-244 by shanxinkai at 2022/11/04 end */
};

static enum power_supply_property mt_ac_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property mt_usb_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	/* AL6528A code for SR-AL6528A-01-301 by gaozhengwei at 2022/08/31 start */
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	/* AL6528A code for SR-AL6528A-01-301 by gaozhengwei at 2022/08/31 end */
	/* hs14 code for SR-AL6528A-01-300 by wenyaqi at 2022/09/08 start */
	POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION,
	/* hs14 code for SR-AL6528A-01-300 by wenyaqi at 2022/09/08 end */
};

/* hs14 code for SR-AL6528A-01-245 by wenyaqi at 2022/10/02 start */
#ifndef HQ_FACTORY_BUILD	//ss version
static enum power_supply_property mt_otg_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};
#endif
/* hs14 code for SR-AL6528A-01-245 by wenyaqi at 2022/10/02 end */

static void tcpc_power_off_work_handler(struct work_struct *work)
{
	pr_info("%s\n", __func__);
	kernel_power_off();
}

static void charger_in_work_handler(struct work_struct *work)
{
	mtk_charger_int_handler();
	fg_charger_in_handler();
}

#ifdef CONFIG_TCPC_CLASS
static void plug_in_out_handler(struct chg_type_info *cti, bool en, bool ignore)
{
#ifdef CONFIG_MTK_TYPEC_WATER_DETECT
	if (cti->water_detected && cti->tcpc_kpoc) {
		pr_info("%s: water detected in KPOC, bypass bc1.2\n",
			__func__);
		return;
	}
#endif

	mutex_lock(&cti->chgdet_lock);
	if (cti->chgdet_en == en)
		goto skip;
	cti->chgdet_en = en;
	cti->ignore_usb = ignore;
	cti->plugin = en;
	atomic_inc(&cti->chgdet_cnt);
	wake_up_interruptible(&cti->waitq);
skip:
	mutex_unlock(&cti->chgdet_lock);
}

static int pd_tcp_notifier_call(struct notifier_block *pnb,
				unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct chg_type_info *cti = container_of(pnb,
		struct chg_type_info, pd_nb);
	int vbus = 0;
	struct power_supply *ac_psy = power_supply_get_by_name("ac");
	struct power_supply *usb_psy = power_supply_get_by_name("usb");
	struct mt_charger *mtk_chg_ac;
	struct mt_charger *mtk_chg_usb;

	if (IS_ERR_OR_NULL(usb_psy)) {
		chr_err("%s, fail to get usb_psy\n", __func__);
		return NOTIFY_BAD;
	}
	if (IS_ERR_OR_NULL(ac_psy)) {
		chr_err("%s, fail to get ac_psy\n", __func__);
		return NOTIFY_BAD;
	}

	mtk_chg_ac = power_supply_get_drvdata(ac_psy);
	if (IS_ERR_OR_NULL(mtk_chg_ac)) {
		chr_err("%s, fail to get mtk_chg_ac\n", __func__);
		return NOTIFY_BAD;
	}

	mtk_chg_usb = power_supply_get_drvdata(usb_psy);
	if (IS_ERR_OR_NULL(mtk_chg_usb)) {
		chr_err("%s, fail to get mtk_chg_usb\n", __func__);
		return NOTIFY_BAD;
	}

	switch (event) {
	case TCP_NOTIFY_SINK_VBUS:
		if (tcpm_inquire_typec_attach_state(cti->tcpc) ==
						   TYPEC_ATTACHED_AUDIO)
			plug_in_out_handler(cti, !!noti->vbus_state.mv, true);
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
		    (noti->typec_state.new_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC)) {
			pr_info("%s USB Plug in, pol = %d\n", __func__,
					noti->typec_state.polarity);
			plug_in_out_handler(cti, true, false);
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
		    noti->typec_state.old_state == TYPEC_ATTACHED_NORP_SRC ||
		    noti->typec_state.old_state == TYPEC_ATTACHED_AUDIO)
			&& noti->typec_state.new_state == TYPEC_UNATTACHED) {
			if (cti->tcpc_kpoc) {
#ifdef CONFIG_KPOC_GET_SOURCE_CAP_TRY
				pr_info("%s error_recovery_once = %d\n", __func__,
					cti->tcpc->pd_port.error_recovery_once);
				if (cti->tcpc->pd_port.error_recovery_once == 1) {
					pr_info("%s KPOC error recovery once\n",
					__func__);
					plug_in_out_handler(cti, false, false);
					break;
				}
#endif /*CONFIG_KPOC_GET_SOURCE_CAP_TRY*/
				vbus = battery_get_vbus();
				pr_info("%s KPOC Plug out, vbus = %d\n",
					__func__, vbus);
				mtk_chg_ac->chg_type = CHARGER_UNKNOWN;
				mtk_chg_usb->chg_type = CHARGER_UNKNOWN;
				power_supply_changed(ac_psy);
				power_supply_changed(usb_psy);
				queue_work_on(cpumask_first(cpu_online_mask),
					      cti->pwr_off_wq,
					      &cti->pwr_off_work);
				break;
			}
			pr_info("%s USB Plug out\n", __func__);
			plug_in_out_handler(cti, false, false);
		} else if (noti->typec_state.old_state == TYPEC_ATTACHED_SRC &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SNK) {
			pr_info("%s Source_to_Sink\n", __func__);
			plug_in_out_handler(cti, true, true);
		}  else if (noti->typec_state.old_state == TYPEC_ATTACHED_SNK &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			pr_info("%s Sink_to_Source\n", __func__);
			plug_in_out_handler(cti, false, true);
		}
		break;
#ifdef CONFIG_MTK_TYPEC_WATER_DETECT
	case TCP_NOTIFY_WD_STATUS:
		if (noti->wd_status.water_detected)
			cti->water_detected = true;
		else
			cti->water_detected = false;
		break;
#endif
	}
	return NOTIFY_OK;
}
#endif

static int chgdet_task_threadfn(void *data)
{
	struct chg_type_info *cti = data;
	bool attach = false, ignore_usb = false;
	int ret = 0;
	struct power_supply *psy = power_supply_get_by_name("charger");
	union power_supply_propval val = {.intval = 0};

	if (!psy) {
		pr_notice("%s: power supply get fail\n", __func__);
		return -ENODEV;
	}

	pr_info("%s: ++\n", __func__);
	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(cti->waitq,
					     atomic_read(&cti->chgdet_cnt) > 0);
		if (ret < 0) {
			pr_info("%s: wait event been interrupted(%d)\n",
				__func__, ret);
			continue;
		}

		pm_stay_awake(cti->dev);
		mutex_lock(&cti->chgdet_lock);
		atomic_set(&cti->chgdet_cnt, 0);
		attach = cti->chgdet_en;
		ignore_usb = cti->ignore_usb;
		mutex_unlock(&cti->chgdet_lock);

		if (attach && ignore_usb) {
			cti->bypass_chgdet = true;
			goto bypass_chgdet;
		} else if (!attach && cti->bypass_chgdet) {
			cti->bypass_chgdet = false;
			goto bypass_chgdet;
		}
		/* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 start */
		charger_manager_bypass_chgdet(cti->chg_consumer, MAIN_CHARGER, false);
		/* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 end */

#ifdef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
		if (cti->chg_consumer)
			charger_manager_enable_chg_type_det(cti->chg_consumer,
							attach);
#else
		mtk_pmic_enable_chr_type_det(attach);
#endif
		goto pm_relax;
bypass_chgdet:
		/* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 start */
		charger_manager_bypass_chgdet(cti->chg_consumer, MAIN_CHARGER, true);
		/* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 end */
		val.intval = attach;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_ONLINE,
						&val);
		if (ret < 0)
			pr_notice("%s: power supply set online fail(%d)\n",
				  __func__, ret);
		if (tcpm_inquire_typec_attach_state(cti->tcpc) ==
						   TYPEC_ATTACHED_AUDIO)
			val.intval = attach ? NONSTANDARD_CHARGER :
					      CHARGER_UNKNOWN;
		else
			val.intval = attach ? STANDARD_HOST : CHARGER_UNKNOWN;
		ret = power_supply_set_property(psy,
						POWER_SUPPLY_PROP_CHARGE_TYPE,
						&val);
		if (ret < 0)
			pr_notice("%s: power supply set charge type fail(%d)\n",
				  __func__, ret);
pm_relax:
		pm_relax(cti->dev);
	}
	pr_info("%s: --\n", __func__);
	return 0;
}

#ifdef CONFIG_EXTCON_USB_CHG
static void init_extcon_work(struct work_struct *work)
{
	struct delayed_work *dw = to_delayed_work(work);
	struct mt_charger *mt_chg =
		container_of(dw, struct mt_charger, extcon_work);
	struct device_node *node = mt_chg->dev->of_node;
	struct usb_extcon_info *info;

	info = mt_chg->extcon_info;
	if (!info)
		return;

	if (of_property_read_bool(node, "extcon")) {
		info->edev = extcon_get_edev_by_phandle(mt_chg->dev, 0);
		if (IS_ERR(info->edev)) {
			schedule_delayed_work(&mt_chg->extcon_work,
				msecs_to_jiffies(50));
			return;
		}
	}

	INIT_DELAYED_WORK(&info->wq_detcable, usb_extcon_detect_cable);
}
#endif

#ifdef CONFIG_MACH_MT6771
static int mtk_6370_psy_notifier(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct power_supply *psy = data;
	struct power_supply *type_psy = NULL;

	struct chg_type_info *cti = container_of(nb,
					struct chg_type_info, psy_nb);

	union power_supply_propval pval;
	int ret;

	cti->chr_psy = power_supply_get_by_name("mt6370_pmu_charger");
	if (IS_ERR_OR_NULL(cti->chr_psy)) {
		pr_info("fail to get chr_psy\n");
		cti->chr_psy = NULL;
		return NOTIFY_DONE;
	}

	/*psy is mt6370, type_psy is charger_type psy*/
	if (event != PSY_EVENT_PROP_CHANGED || psy != cti->chr_psy) {
		pr_info("event or power supply not equal\n");
		return NOTIFY_DONE;

	}

	type_psy = power_supply_get_by_name("charger");
	if (!type_psy) {
		pr_info("%s: get power supply failed\n",
			__func__);
		return NOTIFY_DONE;
	}

	if (event != PSY_EVENT_PROP_CHANGED) {
		pr_info("%s: get event power supply failed\n",
			__func__);
		return NOTIFY_DONE;
	}

	ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_ONLINE, &pval);
	if (ret < 0) {
		pr_info("psy failed to get online prop\n");
		return NOTIFY_DONE;
	}

	ret = power_supply_set_property(type_psy, POWER_SUPPLY_PROP_ONLINE,
		&pval);
	if (ret < 0)
		pr_info("%s: type_psy online failed, ret = %d\n",
			__func__, ret);

	ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_USB_TYPE, &pval);
	if (ret < 0) {
		pr_info("failed to get usb type prop\n");
		return NOTIFY_DONE;
	}

	switch (pval.intval) {
	case POWER_SUPPLY_USB_TYPE_SDP:
		pval.intval = STANDARD_HOST;
		break;
	case POWER_SUPPLY_USB_TYPE_DCP:
		pval.intval = STANDARD_CHARGER;
		break;
	case POWER_SUPPLY_USB_TYPE_CDP:
		pval.intval = CHARGING_HOST;
		break;
	default:
		pval.intval = CHARGER_UNKNOWN;
		break;
	}

	ret = power_supply_set_property(type_psy, POWER_SUPPLY_PROP_CHARGE_TYPE,
		&pval);
	if (ret < 0)
		pr_info("%s: type_psy type failed, ret = %d\n",
			__func__, ret);

	return NOTIFY_DONE;
}
#endif

static int mt_charger_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct chg_type_info *cti = NULL;
	struct mt_charger *mt_chg = NULL;
	#ifdef CONFIG_EXTCON_USB_CHG
	struct usb_extcon_info *info;
	#endif
	struct device *dev = NULL;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;
	int boot_mode = 11;//UNKNOWN_BOOT

	pr_info("%s\n", __func__);

	mt_chg = devm_kzalloc(&pdev->dev, sizeof(*mt_chg), GFP_KERNEL);
	if (!mt_chg)
		return -ENOMEM;

	mt_chg->dev = &pdev->dev;
	mt_chg->chg_online = false;
	mt_chg->chg_type = CHARGER_UNKNOWN;

	mt_chg->chg_desc.name = "charger";
	mt_chg->chg_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	mt_chg->chg_desc.properties = mt_charger_properties;
	mt_chg->chg_desc.num_properties = ARRAY_SIZE(mt_charger_properties);
	mt_chg->chg_desc.set_property = mt_charger_set_property;
	mt_chg->chg_desc.get_property = mt_charger_get_property;
	/* hs14 code for SR-AL6528A-01-299 by gaozhengwei at 2022/09/02 start */
	mt_chg->chg_desc.property_is_writeable = mt_charger__property_is_writeable;
	/* hs14 code for SR-AL6528A-01-299 by gaozhengwei at 2022/09/02 end */
	mt_chg->chg_cfg.drv_data = mt_chg;

	mt_chg->ac_desc.name = "ac";
	mt_chg->ac_desc.type = POWER_SUPPLY_TYPE_MAINS;
	mt_chg->ac_desc.properties = mt_ac_properties;
	mt_chg->ac_desc.num_properties = ARRAY_SIZE(mt_ac_properties);
	mt_chg->ac_desc.get_property = mt_ac_get_property;
	mt_chg->ac_cfg.drv_data = mt_chg;

	mt_chg->usb_desc.name = "usb";
	mt_chg->usb_desc.type = POWER_SUPPLY_TYPE_USB;
	mt_chg->usb_desc.properties = mt_usb_properties;
	mt_chg->usb_desc.num_properties = ARRAY_SIZE(mt_usb_properties);
	mt_chg->usb_desc.get_property = mt_usb_get_property;
	mt_chg->usb_cfg.drv_data = mt_chg;

/* hs14 code for SR-AL6528A-01-245 by wenyaqi at 2022/10/02 start */
#ifndef HQ_FACTORY_BUILD	//ss version
	mt_chg->otg_desc.name = "otg";
	mt_chg->otg_desc.type = POWER_SUPPLY_TYPE_OTG;
	mt_chg->otg_desc.properties = mt_otg_properties;
	mt_chg->otg_desc.num_properties = ARRAY_SIZE(mt_otg_properties);
	mt_chg->otg_desc.get_property = mt_otg_get_property;
	mt_chg->otg_cfg.drv_data = mt_chg;
#endif
/* hs14 code for SR-AL6528A-01-245 by wenyaqi at 2022/10/02 end */

	mt_chg->chg_psy = power_supply_register(&pdev->dev,
		&mt_chg->chg_desc, &mt_chg->chg_cfg);
	if (IS_ERR(mt_chg->chg_psy)) {
		dev_notice(&pdev->dev, "Failed to register power supply: %ld\n",
			PTR_ERR(mt_chg->chg_psy));
		ret = PTR_ERR(mt_chg->chg_psy);
		return ret;
	}

	mt_chg->ac_psy = power_supply_register(&pdev->dev, &mt_chg->ac_desc,
		&mt_chg->ac_cfg);
	if (IS_ERR(mt_chg->ac_psy)) {
		dev_notice(&pdev->dev, "Failed to register power supply: %ld\n",
			PTR_ERR(mt_chg->ac_psy));
		ret = PTR_ERR(mt_chg->ac_psy);
		goto err_ac_psy;
	}

	mt_chg->usb_psy = power_supply_register(&pdev->dev, &mt_chg->usb_desc,
		&mt_chg->usb_cfg);
	if (IS_ERR(mt_chg->usb_psy)) {
		dev_notice(&pdev->dev, "Failed to register power supply: %ld\n",
			PTR_ERR(mt_chg->usb_psy));
		ret = PTR_ERR(mt_chg->usb_psy);
		goto err_usb_psy;
	}

/* hs14 code for SR-AL6528A-01-245 by wenyaqi at 2022/10/02 start */
#ifndef HQ_FACTORY_BUILD	//ss version
	mt_chg->otg_psy = power_supply_register(&pdev->dev, &mt_chg->otg_desc,
		&mt_chg->otg_cfg);
	if (IS_ERR(mt_chg->otg_psy)) {
		dev_notice(&pdev->dev, "Failed to register power supply: %ld\n",
			PTR_ERR(mt_chg->otg_psy));
		ret = PTR_ERR(mt_chg->otg_psy);
		goto err_otg_psy;
	}
#endif
/* hs14 code for SR-AL6528A-01-245 by wenyaqi at 2022/10/02 end */

	cti = devm_kzalloc(&pdev->dev, sizeof(*cti), GFP_KERNEL);
	if (!cti) {
		ret = -ENOMEM;
		goto err_no_mem;
	}
	cti->dev = &pdev->dev;

	cti->chg_consumer = charger_manager_get_by_name(cti->dev,
							"charger_port1");
	if (!cti->chg_consumer) {
		pr_info("%s: get charger consumer device failed\n", __func__);
		ret = -EINVAL;
		goto err_get_tcpc_dev;
	}

	dev = &(pdev->dev);
	if (dev != NULL) {
		boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
		if (!boot_node) {
			chr_err("%s: failed to get boot mode phandle\n", __func__);
		} else {
			tag = (struct tag_bootmode *)of_get_property(boot_node, "atag,boot", NULL);
			if (!tag)
				chr_err("%s: failed to get atag,boot\n", __func__);
			else
				boot_mode = tag->bootmode;
		}
	}
//	ret = get_boot_mode();
	if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT ||
	    boot_mode == LOW_POWER_OFF_CHARGING_BOOT)
		cti->tcpc_kpoc = true;
	pr_info("%s KPOC(%d)\n", __func__, cti->tcpc_kpoc);

	/* Init Charger Detection */
	mutex_init(&cti->chgdet_lock);
	atomic_set(&cti->chgdet_cnt, 0);

	init_waitqueue_head(&cti->waitq);
	cti->chgdet_task = kthread_run(
				chgdet_task_threadfn, cti, "chgdet_thread");
	ret = PTR_ERR_OR_ZERO(cti->chgdet_task);
	if (ret < 0) {
		pr_info("%s: create chg det work fail\n", __func__);
		return ret;
	}

	/* Init power off work */
	cti->pwr_off_wq = create_singlethread_workqueue("tcpc_power_off");
	INIT_WORK(&cti->pwr_off_work, tcpc_power_off_work_handler);

	cti->chg_in_wq = create_singlethread_workqueue("charger_in");
	INIT_WORK(&cti->chg_in_work, charger_in_work_handler);

	mt_chg->cti = cti;
	platform_set_drvdata(pdev, mt_chg);
	device_init_wakeup(&pdev->dev, true);

#ifdef CONFIG_MACH_MT6771
	cti->psy_nb.notifier_call = mtk_6370_psy_notifier;
	ret = power_supply_reg_notifier(&cti->psy_nb);
	if (ret)
		pr_info("fail to register notifer\n");
#endif

	#ifdef CONFIG_EXTCON_USB_CHG
	info = devm_kzalloc(mt_chg->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = mt_chg->dev;
	mt_chg->extcon_info = info;

	INIT_DELAYED_WORK(&mt_chg->extcon_work, init_extcon_work);
	schedule_delayed_work(&mt_chg->extcon_work, 0);
	#endif

	pr_info("%s done\n", __func__);
	return 0;

err_get_tcpc_dev:
	devm_kfree(&pdev->dev, cti);
err_no_mem:
/* hs14 code for SR-AL6528A-01-245 by wenyaqi at 2022/10/02 start */
#ifndef HQ_FACTORY_BUILD	//ss version
	power_supply_unregister(mt_chg->otg_psy);
err_otg_psy:
#endif
/* hs14 code for SR-AL6528A-01-245 by wenyaqi at 2022/10/02 end */
	power_supply_unregister(mt_chg->usb_psy);
err_usb_psy:
	power_supply_unregister(mt_chg->ac_psy);
err_ac_psy:
	power_supply_unregister(mt_chg->chg_psy);
	return ret;
}

static int mt_charger_remove(struct platform_device *pdev)
{
	struct mt_charger *mt_charger = platform_get_drvdata(pdev);
	struct chg_type_info *cti = mt_charger->cti;

	power_supply_unregister(mt_charger->chg_psy);
	power_supply_unregister(mt_charger->ac_psy);
	power_supply_unregister(mt_charger->usb_psy);

	pr_info("%s\n", __func__);
	if (cti->chgdet_task) {
		kthread_stop(cti->chgdet_task);
		atomic_inc(&cti->chgdet_cnt);
		wake_up_interruptible(&cti->waitq);
	}

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mt_charger_suspend(struct device *dev)
{
	/* struct mt_charger *mt_charger = dev_get_drvdata(dev); */
	return 0;
}

static int mt_charger_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mt_charger *mt_charger = platform_get_drvdata(pdev);

	if (!mt_charger) {
		pr_info("%s: get mt_charger failed\n", __func__);
		return -ENODEV;
	}

	power_supply_changed(mt_charger->chg_psy);
	power_supply_changed(mt_charger->ac_psy);
	power_supply_changed(mt_charger->usb_psy);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(mt_charger_pm_ops, mt_charger_suspend,
	mt_charger_resume);

static const struct of_device_id mt_charger_match[] = {
	{ .compatible = "mediatek,mt-charger", },
	{ },
};
static struct platform_driver mt_charger_driver = {
	.probe = mt_charger_probe,
	.remove = mt_charger_remove,
	.driver = {
		.name = "mt-charger-det",
		.owner = THIS_MODULE,
		.pm = &mt_charger_pm_ops,
		.of_match_table = mt_charger_match,
	},
};

/* Legacy api to prevent build error */
bool upmu_is_chr_det(void)
{
	struct mt_charger *mtk_chg = NULL;
	struct power_supply *psy = power_supply_get_by_name("charger");

	if (!psy) {
		pr_info("%s: get power supply failed\n", __func__);
		return -EINVAL;
	}
	mtk_chg = power_supply_get_drvdata(psy);
	return mtk_chg->chg_online;
}

/* Legacy api to prevent build error */
bool pmic_chrdet_status(void)
{
	if (upmu_is_chr_det())
		return true;

	pr_notice("%s: No charger\n", __func__);
	return false;
}

enum charger_type mt_get_charger_type(void)
{
	struct mt_charger *mtk_chg = NULL;
	struct power_supply *psy = power_supply_get_by_name("charger");

	if (!psy) {
		pr_info("%s: get power supply failed\n", __func__);
		return -EINVAL;
	}
	mtk_chg = power_supply_get_drvdata(psy);
	return mtk_chg->chg_type;
}

bool mt_charger_plugin(void)
{
	struct mt_charger *mtk_chg = NULL;
	struct power_supply *psy = power_supply_get_by_name("charger");
	struct chg_type_info *cti = NULL;

	if (!psy) {
		pr_info("%s: get power supply failed\n", __func__);
		return -EINVAL;
	}
	mtk_chg = power_supply_get_drvdata(psy);
	cti = mtk_chg->cti;
	pr_info("%s plugin:%d\n", __func__, cti->plugin);

	return cti->plugin;
}

static s32 __init mt_charger_det_init(void)
{
	return platform_driver_register(&mt_charger_driver);
}

static void __exit mt_charger_det_exit(void)
{
	platform_driver_unregister(&mt_charger_driver);
}

device_initcall(mt_charger_det_init);
module_exit(mt_charger_det_exit);

#ifdef CONFIG_TCPC_CLASS
static int __init mt_charger_det_notifier_call_init(void)
{
	int ret = 0;
	struct power_supply *psy = power_supply_get_by_name("charger");
	struct mt_charger *mt_chg = NULL;
	struct chg_type_info *cti = NULL;

	if (!psy) {
		pr_notice("%s: get power supply fail\n", __func__);
		return -ENODEV;
	}
	mt_chg = power_supply_get_drvdata(psy);
	cti = mt_chg->cti;

	cti->tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (cti->tcpc == NULL) {
		pr_notice("%s: get tcpc dev fail\n", __func__);
		ret = -ENODEV;
		goto out;
	}
	cti->pd_nb.notifier_call = pd_tcp_notifier_call;
	ret = register_tcp_dev_notifier(cti->tcpc,
		&cti->pd_nb, TCP_NOTIFY_TYPE_ALL);
	if (ret < 0) {
		pr_notice("%s: register tcpc notifier fail(%d)\n",
			  __func__, ret);
		goto out;
	}
	pr_info("%s done\n", __func__);
out:
	power_supply_put(psy);
	return ret;
}
late_initcall(mt_charger_det_notifier_call_init);
#endif

MODULE_DESCRIPTION("mt-charger-detection");
MODULE_AUTHOR("MediaTek");
MODULE_LICENSE("GPL v2");

#endif /* CONFIG_MTK_FPGA */
