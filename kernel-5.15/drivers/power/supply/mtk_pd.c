// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 *
 * Filename:
 * ---------
 *    mtk_pd.c
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

#include "mtk_pd.h"
#include "mtk_charger_algorithm_class.h"
#include "adapter_class.h"

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_notifier.h>
#endif
#include <linux/battery/sec_pd.h>
#endif
#include <tcpm.h>
#include <../drivers/battery/common/sec_charging_common.h>

static int pd_dbg_level = PD_DEBUG_LEVEL;
#define PD_VBUS_IR_DROP_THRESHOLD 1200

static bool algo_waiver_test;
module_param(algo_waiver_test, bool, 0644);

int pd_get_debug_level(void)
{
	return pd_dbg_level;
}

static char *pd_state_to_str(int state)
{
	switch (state) {
	case PD_HW_UNINIT:
		return "PD_HW_UNINIT";
	case PD_HW_FAIL:
		return "PD_HW_FAIL";
	case PD_HW_READY:
		return "PD_HW_READY";
	case PD_TA_NOT_SUPPORT:
		return "PD_TA_NOT_SUPPORT";
	case PD_RUN:
		return "PD_RUN";
	case PD_TUNING:
		return "PD_TUNING";
	case PD_POSTCC:
		return "PD_POSTCC";
	default:
		break;
	}
	pd_err("%s unknown state:%d\n", __func__
		, state);
	return "PD_UNKNOWN";
}

static int _pd_init_algo(struct chg_alg_device *alg)
{
	struct mtk_pd *pd;
	int cnt, log_level;

	pd = dev_get_drvdata(&alg->dev);
	pd_dbg("%s\n", __func__);

	mutex_lock(&pd->access_lock);
	if (pd_hal_init_hardware(alg) != 0) {
		pd->state = PD_HW_FAIL;
		pd_err("%s:init hw fail\n", __func__);
	} else
		pd->state = PD_HW_READY;

	pd_hal_vbat_mon_en(alg, CHG1, false);
	pd->old_cv = 0;
	pd->stop_6pin_re_en = 0;

	if (alg->config == DUAL_CHARGERS_IN_PARALLEL) {
		pd_err("%s does not support DUAL_CHARGERS_IN_PARALLEL\n",
			__func__);
		alg->config = SINGLE_CHARGER;
	} else if (alg->config == DUAL_CHARGERS_IN_SERIES) {
		cnt = pd_hal_get_charger_cnt(alg);
		if (cnt == 2)
			alg->config = DUAL_CHARGERS_IN_SERIES;
		else
			alg->config = SINGLE_CHARGER;
	} else
		alg->config = SINGLE_CHARGER;

	log_level = pd_hal_get_log_level(alg);
	pr_notice("%s: log_level=%d", __func__, log_level);
	if (log_level > 0)
		pd_dbg_level = log_level;

	pd->pdc_max_watt_setting = -1;

	pd->check_impedance = true;
	pd->pd_cap_max_watt = -1;
	pd->pd_idx = -1;
	pd->pd_reset_idx = -1;
	pd->pd_boost_idx = 0;
	pd->pd_buck_idx = 0;

	mutex_unlock(&pd->access_lock);
	return 0;
}

static int _pd_is_algo_ready(struct chg_alg_device *alg)
{
	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);
	int ret_value;
#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	int uisoc;
#endif

	if (algo_waiver_test) {
		ret_value = ALG_WAIVER;
		goto skip;
	}

	pd_dbg("%s %d\n", __func__, pd->state);
	switch (pd->state) {
	case PD_HW_UNINIT:
	case PD_HW_FAIL:
		ret_value = ALG_INIT_FAIL;
		break;
	case PD_HW_READY:
		ret_value = pd_hal_is_pd_adapter_ready(alg);
		if (ret_value == ALG_READY) {
#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			uisoc = pd_hal_get_uisoc(alg);
			if (pd->input_current_limit1 != -1 ||
				pd->charging_current_limit1 != -1 ||
				pd->input_current_limit2 != -1 ||
				pd->charging_current_limit2 != -1)
				ret_value = ALG_NOT_READY;
			else if (uisoc >= pd->pd_stop_battery_soc ||
				(uisoc == -1 && pd->ref_vbat > pd->vbat_threshold))
				ret_value = ALG_WAIVER;
#endif
		} else if (ret_value == ALG_TA_NOT_SUPPORT)
			pd->state = PD_TA_NOT_SUPPORT;
		else if (ret_value == ALG_TA_CHECKING)
			pd->state = PD_HW_READY;
		else
			pd->state = PD_TA_NOT_SUPPORT;

		break;
	case PD_TA_NOT_SUPPORT:
		ret_value = ALG_TA_NOT_SUPPORT;
		break;
	case PD_RUN:
	case PD_TUNING:
	case PD_POSTCC:
		ret_value = ALG_RUNNING;
		break;
	default:
		pd_err("PD unknown state:%d\n", pd->state);
		ret_value = ALG_INIT_FAIL;
		break;
	}
skip:
	return ret_value;
}

void __mtk_pdc_init_table(struct chg_alg_device *alg)
{
	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);

	pd->cap.nr = 0;
	pd->cap.selected_cap_idx = -1;

	if (pd_hal_is_pd_adapter_ready(alg) == ALG_READY)
		pd_hal_get_adapter_cap(alg, &pd->cap);
	else
		pd_err("mtk_is_pdc_ready is fail\n");

	pd_dbg("[%s] nr:%d default:%d\n", __func__, pd->cap.nr,
	pd->cap.selected_cap_idx);
}

void __mtk_pdc_get_reset_idx(struct chg_alg_device *alg)
{
	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);
	struct pd_power_cap *cap;
	int i = 0;
	int idx = 0;

	cap = &pd->cap;

	if (pd->pd_reset_idx == -1) {
		for (i = 0; i < cap->nr; i++) {

			if (cap->min_mv[i] < pd->vbus_l ||
				cap->max_mv[i] < pd->vbus_l ||
				cap->min_mv[i] > pd->vbus_l ||
				cap->max_mv[i] > pd->vbus_l) {
				continue;
			}
			idx = i;
		}
		pd->pd_reset_idx = idx;
		pd_dbg("[%s]reset idx:%d vbus:%d %d\n", __func__,
			idx, cap->min_mv[idx], cap->max_mv[idx]);
	}
}

void __mtk_pdc_get_cap_max_watt(struct chg_alg_device *alg)
{
	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);
	struct pd_power_cap *cap;
	int i = 0;
	int idx = 0;

	cap = &pd->cap;

	if (pd->pd_cap_max_watt == -1) {
		for (i = 0; i < cap->nr; i++) {
			if (cap->min_mv[i] <= pd->vbus_h &&
				cap->min_mv[i] >= pd->vbus_l &&
				cap->max_mv[i] <= pd->vbus_h &&
				cap->max_mv[i] >= pd->vbus_l) {

				if (cap->maxwatt[i] > pd->pd_cap_max_watt) {
					pd->pd_cap_max_watt = cap->maxwatt[i];
					idx = i;
				}
				pd_dbg("%d %d %d %d %d %d\n",
					cap->min_mv[i],
					cap->max_mv[i],
					pd->vbus_h,
					pd->vbus_l,
					cap->maxwatt[i],
					pd->pd_cap_max_watt);
				continue;
			}
		}
		pd_dbg("[%s]idx:%d vbus:%d %d maxwatt:%d\n", __func__,
			idx, cap->min_mv[idx], cap->max_mv[idx],
			pd->pd_cap_max_watt);
	}
}

int __mtk_pdc_get_idx(struct chg_alg_device *alg, int selected_idx,
	int *boost_idx, int *buck_idx)
{
	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);
	struct pd_power_cap *cap;
	int i = 0;
	int idx = 0;

	cap = &pd->cap;
	idx = selected_idx;

	if (idx < 0) {
		pd_err("[%s] invalid idx:%d\n", __func__, idx);
		*boost_idx = 0;
		*buck_idx = 0;
		return -1;
	}

	/* get boost_idx */
	for (i = 0; i < cap->nr; i++) {

		if (cap->min_mv[i] < pd->vbus_l ||
			cap->max_mv[i] < pd->vbus_l) {
			pd_err("min_mv error:%d %d %d\n",
					cap->min_mv[i],
					cap->max_mv[i],
					pd->vbus_l);
			continue;
		}

		if (cap->min_mv[i] > pd->vbus_h ||
			cap->max_mv[i] > pd->vbus_h) {
			pd_err("max_mv error:%d %d %d\n",
					cap->min_mv[i],
					cap->max_mv[i],
					pd->vbus_h);
			continue;
		}

		if (idx == selected_idx) {
			if (cap->maxwatt[i] > cap->maxwatt[idx])
				idx = i;
		} else {
			if (cap->maxwatt[i] < cap->maxwatt[idx] &&
				cap->maxwatt[i] > cap->maxwatt[selected_idx])
				idx = i;
		}
	}
	*boost_idx = idx;
	idx = selected_idx;

	/* get buck_idx */
	for (i = 0; i < cap->nr; i++) {

		if (cap->min_mv[i] < pd->vbus_l ||
			cap->max_mv[i] < pd->vbus_l) {
			pd_err("min_mv error:%d %d %d\n",
					cap->min_mv[i],
					cap->max_mv[i],
					pd->vbus_l);
			continue;
		}

		if (cap->min_mv[i] > pd->vbus_h ||
			cap->max_mv[i] > pd->vbus_h) {
			pd_err("max_mv error:%d %d %d\n",
					cap->min_mv[i],
					cap->max_mv[i],
					pd->vbus_h);
			continue;
		}

		if (idx == selected_idx) {
			if (cap->maxwatt[i] < cap->maxwatt[idx])
				idx = i;
		} else {
			if (cap->maxwatt[i] > cap->maxwatt[idx] &&
				cap->maxwatt[i] < cap->maxwatt[selected_idx])
				idx = i;
		}
	}
	*buck_idx = idx;

	return 0;
}
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
int pdc_get_apdo_max_power(unsigned int *pdo_pos,
		unsigned int *taMaxVol, unsigned int *taMaxCur, unsigned int *taMaxPwr)
{
	int i;
	int ret = 0;
	int max_current = 0, max_voltage = 0, max_power = 0;

	if (!pd_noti.sink_status.has_apdo) {
		pr_info("%s: pd don't have apdo\n",	__func__);
		return -1;
	}

	/* First, get TA maximum power from the fixed PDO */
	for (i = 1; i <= pd_noti.sink_status.available_pdo_num; i++) {
		if (!(pd_noti.sink_status.power_list[i].apdo)) {
			max_voltage = pd_noti.sink_status.power_list[i].max_voltage;
			max_current = pd_noti.sink_status.power_list[i].max_current;
			max_power = (max_voltage * max_current > max_power) ? (max_voltage * max_current) : max_power;
			*taMaxPwr = max_power;	/* mW */
		}
	}

	if (*pdo_pos == 0) {
		/* Get the proper PDO */
		for (i = 1; i <= pd_noti.sink_status.available_pdo_num; i++) {
			if (pd_noti.sink_status.power_list[i].apdo) {
				if (pd_noti.sink_status.power_list[i].max_voltage >= *taMaxVol) {
					*pdo_pos = i;
					*taMaxVol = pd_noti.sink_status.power_list[i].max_voltage;
					*taMaxCur = pd_noti.sink_status.power_list[i].max_current;
					break;
				}
			}
			if (*pdo_pos)
				break;
		}

		if (*pdo_pos == 0) {
			pr_info("mv (%d) and ma (%d) out of range of APDO\n",
				*taMaxVol, *taMaxCur);
			ret = -EINVAL;
		}
	} else {
		/* If we already have pdo object position, we don't need to search max current */
		ret = -ENOTSUPP;
	}

	pr_info("%s : *pdo_pos(%d), *taMaxVol(%d), *maxCur(%d), *maxPwr(%d)\n",
		__func__, *pdo_pos, *taMaxVol, *taMaxCur, *taMaxPwr);

	return ret;
}

void pdc_select_pdo(int batt_idx)
{
	struct chg_alg_device *alg  = get_chg_alg_by_name("pd");
	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);
	int idx = batt_idx - 1; // Offset by 1
#if IS_ENABLED(CONFIG_SEC_FACTORY) && !IS_ENABLED(CONFIG_DIRECT_CHARGING)
	int newvbus, newcur, newidx;
#endif

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	if (tcpm_inquire_pd_charging_policy(pd->tcpc_dev) == DPM_CHARGING_POLICY_PPS) {
		pr_info("%s: Ending PPS\n", __func__);
		pd_hal_set_adapter_cap_type(alg, MTK_PD_APDO_END, 5000, 2000);
	}
#endif
	pr_info("%s: selecting idx:%d(sec_batt:%d)\n", __func__, idx, batt_idx);

	__mtk_pdc_setup(alg, idx);

	pd_err("[%s]idx:%d:%d:%d:%d vbus:%d cur:%d\n", __func__,
		pd->pd_idx, idx, pd->pd_boost_idx, pd->pd_buck_idx,
		pd->cap.max_mv[idx], pd->cap.ma[idx]);

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	pd_noti.sink_status.selected_pdo_num = batt_idx;
	pd_noti.sink_status.current_pdo_num = batt_idx;
#endif
#if IS_ENABLED(CONFIG_SEC_FACTORY) && !IS_ENABLED(CONFIG_DIRECT_CHARGING)
	pr_info("%s: checking capabilities (current_pdo_num:%d)\n", __func__, pd_noti.sink_status.current_pdo_num);
	__mtk_pdc_get_setting(alg, &newvbus, &newcur, &newidx);
#endif
}

int pdc_select_pps(int num, int ppsVol, int ppsCur)
{
	int ret = -100, idx = num-1;
	struct chg_alg_device *alg  = get_chg_alg_by_name("pd");
	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);
	int newvbus, newcur, newidx;

	pr_info("%s : num(%d), ppsVol(%d), ppsCur(%d)\n",
		__func__, num, ppsVol, ppsCur);

	if (pd->pd_idx != idx) {
		pr_info("%s : start\n", __func__);
		pd_hal_set_adapter_cap_type(alg, MTK_PD_APDO_START, ppsVol, ppsCur);
	}
	ret = pd_hal_set_adapter_cap_type(alg, MTK_PD_APDO, ppsVol, ppsCur);
	if (ret != MTK_ADAPTER_OK)
		pr_info("%s : not OK(%d)\n", __func__, ret);

	if (pd->pd_idx != idx) {
		pr_info("%s: checking capabilities (pd->pd_idx:%d =>  idx:%d)\n", __func__, pd->pd_idx, idx);
		__mtk_pdc_get_setting(alg, &newvbus, &newcur, &newidx);
	}

	pd->pd_idx = idx;
	return ret;
}

void pdc_usbpd_forced_change_srccap(int max_cur)
{
	struct tcpm_power_cap cap;
	struct tcpc_device *tcpc;
	uint32_t pdo_cur = 0;

	tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (!tcpc) {
		pr_err("%s get tcpc dev fail\n", __func__);
		return;
	}

	tcpm_inquire_pd_local_source_cap(tcpc, &cap); // save the previous src_cap

	/*
	 * B19...10 : Voltage in 50mV units
	 * B9...0 : Maximum Current in 10mA units
	 */
	/* 5V */
	pdo_cur = 0x00019000;
	max_cur /= 10;
	pdo_cur = pdo_cur | (uint32_t)max_cur;
	pr_info("%s : Force set PDO current: %08x(%d)\n",
		__func__, pdo_cur, (int)((pdo_cur & 0x00003FF) * 10));
	cap.pdos[0] = pdo_cur;

	tcpm_set_pd_local_source_cap(tcpc, &cap);  // set the new src_cap
	tcpm_dpm_pd_source_cap(tcpc, NULL);  // send out src_cap PD msg
}

int pdc_clear(void)
{
	struct chg_alg_device *alg  = get_chg_alg_by_name("pd");
	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	PD_NOTI_TYPEDEF pdic_noti;
#endif
	pr_err("%s: clear selected pdo\n", __func__);
	pd->fpdo_num = 0;
	pd->apdo_num = 0;
	pd->unknown_num = 0;
	pd->ps_rdy = 0;
	pd->prev_available_pdo = -1;
	pd->pd_idx = -1;
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	pdic_noti.src = PDIC_NOTIFY_DEV_PDIC;
	pdic_noti.dest = PDIC_NOTIFY_DEV_BATT;
	pdic_noti.id = PDIC_NOTIFY_ID_POWER_STATUS;
	pdic_noti.sub1 = 0;
	pdic_noti.sub2 = 0;
	pdic_noti.sub3 = 0;
	pdic_noti.pd = NULL;
	pd_noti.sink_status.current_pdo_num = 0;
	pd_noti.sink_status.selected_pdo_num = 0;
	if (pd_noti.event != PDIC_NOTIFY_EVENT_DETACH) {
		pd_noti.event = PDIC_NOTIFY_EVENT_DETACH;
		pdic_notifier_notify((PD_NOTI_TYPEDEF *)&pdic_noti, &pd_noti, 0);
	}
	if (pd_noti.sink_status.has_apdo) {
		pd_hal_set_adapter_cap_type(alg, MTK_PD_APDO_END, 5000, 2000);
		pd_noti.sink_status.has_apdo = false;
	}
	pd_noti.sink_status.pps_voltage = 0;
	pd_noti.sink_status.pps_current = 0;
#endif
	pd->is_srccap_changed = 0;
	return 0;
}
EXPORT_SYMBOL(pdc_clear);

int pdc_hard_rst(void)
{
	struct chg_alg_device *alg  = get_chg_alg_by_name("pd");
	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);

	pr_err("%s: hard reset\n", __func__);
	pd->was_hard_rst = 1;
	pdc_clear();

	return 0;
}
EXPORT_SYMBOL(pdc_hard_rst);

int pdc_send_pd_noti(void)
{
	struct chg_alg_device *alg  = get_chg_alg_by_name("pd");
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	int i;
	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);
	struct pd_power_cap *cap;
	bool do_power_nego = false;
	u8 temp = 0x00;
	PD_NOTI_TYPEDEF pdic_noti;
	int selected_idx;
#endif
	union power_supply_propval value = {0, };

	cap = &pd->cap;
	pd->fpdo_num = 0;
	pd->apdo_num = 0;
	pd->unknown_num = 0;
	for (i = 1; i <= cap->nr; i++) {
		if (cap->type[i] == MTK_PD_APDO)
			pd->apdo_num++;
		else if (cap->type[i] == MTK_PD)
			pd->fpdo_num++;
		else
			pd->unknown_num++;
	}
	if (cap->nr <= 0) {
		pr_info("%s : PDO list is empty!!\n", __func__);
		return 0;
	} else {
		pr_info("%s: [START] total num_pd_list: %d, num_fpdo: %d, num_apdo: %d\n",
			__func__, cap->nr, pd->fpdo_num, pd->apdo_num);
	}
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	temp = pd_noti.sink_status.available_pdo_num = cap->nr;

	pd_noti.sink_status.has_apdo = false;
	pd_noti.sink_status.has_vpdo = false;

	for (i = 0; i < temp; i++) {
		if (!(do_power_nego) &&
			(pd_noti.sink_status.power_list[i + 1].max_current != cap->ma[i] ||
			pd_noti.sink_status.power_list[i + 1].max_voltage != cap->max_mv[i]))
			do_power_nego = true;

		pd_noti.sink_status.power_list[i + 1].max_current = cap->ma[i];
		pd_noti.sink_status.power_list[i + 1].max_voltage = cap->max_mv[i];
		pd_noti.sink_status.power_list[i + 1].min_voltage = cap->min_mv[i];
		pd_noti.sink_status.power_list[i + 1].comm_capable = pd_hal_is_src_usb_communication_capable(alg);
		pd_noti.sink_status.power_list[i + 1].suspend = pd_hal_is_src_usb_suspend_support(alg);

			pd_noti.sink_status.power_list[i + 1].accept = true;
		if (cap->type[i] == MTK_PD_APDO) {
			pd_noti.sink_status.power_list[i + 1].apdo = true;
			pd_noti.sink_status.power_list[i + 1].pdo_type = APDO_TYPE;
			pd_noti.sink_status.has_apdo = true;
		} else if (cap->type[i] == MTK_PD_VPDO) {
			pd_noti.sink_status.power_list[i + 1].pdo_type = VPDO_TYPE;
			pd_noti.sink_status.has_vpdo = true;
			pd_noti.sink_status.has_apdo = false;
		} else if (cap->type[i] == MTK_PD) {
			pd_noti.sink_status.power_list[i + 1].apdo = false;
			pd_noti.sink_status.power_list[i + 1].pdo_type = FPDO_TYPE;
			pd_noti.sink_status.has_apdo = false;
		}
		pr_info("%s : PDO_Num[%d,%s,%s] MAX_CURR(%d) MAX_VOLT(%d), AVAILABLE_PDO_Num(%d), comm(%d), suspend(%d)\n", __func__,
				i, pd_noti.sink_status.power_list[i + 1].apdo ? "APDO" :
				(pd_noti.sink_status.power_list[i + 1].pdo_type == VPDO_TYPE ? "VPDO":"FIXED"),
				pd_noti.sink_status.power_list[i + 1].accept ? "O" : "X",
				pd_noti.sink_status.power_list[i + 1].max_current,
				pd_noti.sink_status.power_list[i + 1].max_voltage,
				pd_noti.sink_status.available_pdo_num,
				pd_noti.sink_status.power_list[i + 1].comm_capable,
				pd_noti.sink_status.power_list[i + 1].suspend);
	}

	selected_idx = cap->selected_cap_idx;
	if (selected_idx < 0 || selected_idx >= PD_CAP_MAX_NR) {
		pr_info("[%s] selected_cap_idx:%d\n", __func__, selected_idx);
		selected_idx = 0;
	}

	pd_noti.sink_status.current_pdo_num = selected_idx + 1;
	if (pd_noti.sink_status.current_pdo_num != pd_noti.sink_status.selected_pdo_num) {
		if (pd_noti.sink_status.selected_pdo_num == 0) {
			pr_info("%s : PDO is not selected, default PDO used\n",
				 __func__);
			pd_noti.sink_status.selected_pdo_num = 1;
		}
	}
	pdic_noti.src = PDIC_NOTIFY_DEV_PDIC;
	pdic_noti.dest = PDIC_NOTIFY_DEV_BATT;
	pdic_noti.id = PDIC_NOTIFY_ID_POWER_STATUS;

	if ((pd->ps_rdy == 1 &&
		pd->prev_available_pdo !=
			pd_noti.sink_status.available_pdo_num) ||
			(pd->was_hard_rst) ||
			(do_power_nego && pd->is_srccap_changed)) {
		pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK_CAP;

		if (do_power_nego && pd->is_srccap_changed) {
			value.intval = 1;
			psy_do_property("battery", set,
				POWER_SUPPLY_EXT_PROP_SRCCAP, value);
			pd->is_srccap_changed = 0;
			pr_info("%s : do_power_nego(%d) is_srccap_changed(%d)\n", __func__, do_power_nego, pd->is_srccap_changed);
		}			
		if (pd->was_hard_rst)
			pd->was_hard_rst = 0;
	} else
		pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK;
	pd->ps_rdy = 1;
	pd->prev_available_pdo = pd_noti.sink_status.available_pdo_num;

	pr_info("%s : [END] pd_noti.event = %d, current_pdo_num(%d)\n", __func__, pd_noti.event, pd_noti.sink_status.current_pdo_num);
	pdic_noti.sub1 = 1;
	pdic_noti.sub2 = 0;
	pdic_noti.sub3 = 0;
	pdic_noti.pd = NULL;
	pdic_notifier_notify((PD_NOTI_TYPEDEF *)&pdic_noti, &pd_noti, 0);
#endif

	return 0;
}

#endif

int __mtk_pdc_setup(struct chg_alg_device *alg, int idx)
{
	int ret = -100;
	unsigned int mivr;
	unsigned int oldmivr = 4600000;
#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	unsigned int oldmA = 3000000;
#endif
	bool force_update = false;
	int chg_cnt, is_chip_enabled, i;

	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);

	if (pd->pd_idx == idx) {
		pd_hal_get_mivr(alg, CHG1, &oldmivr);

		if (pd->cap.max_mv[idx] - oldmivr / 1000 >
			PD_VBUS_IR_DROP_THRESHOLD)
			force_update = true;

		chg_cnt = pd_hal_get_charger_cnt(alg);
		if (chg_cnt > 1 && alg->config == DUAL_CHARGERS_IN_SERIES) {
			for (i = CHG2; i < CHG_MAX; i++) {
				is_chip_enabled =
						pd_hal_is_chip_enable(alg, i);
				if (is_chip_enabled) {
					pd_hal_get_mivr(alg, CHG2, &oldmivr);

					if (pd->cap.max_mv[idx] - oldmivr / 1000
						> PD_VBUS_IR_DROP_THRESHOLD -
						pd->slave_mivr_diff / 1000)
						force_update = true;
				}
			}
		}
	}

	if (pd->pd_idx != idx || force_update) {
		if (pd->cap.max_mv[idx] > 5000)
			pd_hal_enable_vbus_ovp(alg, false);
		else
			pd_hal_enable_vbus_ovp(alg, true);

		pd_hal_get_mivr(alg, CHG1, &oldmivr);
		mivr = pd->min_charger_voltage / 1000;
		pd_hal_set_mivr(alg, CHG1, pd->min_charger_voltage);

#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		pd_hal_get_input_current(alg, CHG1, &oldmA);
		oldmA = oldmA / 1000;

#ifdef FIXME
		if (info->data.parallel_vbus && (oldmA * 2 > pd->cap.ma[idx])) {
			charger_dev_set_input_current(info->chg1_dev,
					pd->cap.ma[idx] * 1000 / 2);
			charger_dev_set_input_current(info->chg2_dev,
					pd->cap.ma[idx] * 1000 / 2);
		} else if (info->data.parallel_vbus == false &&
			(oldmA > pd->cap.ma[idx]))
			charger_dev_set_input_current(info->chg1_dev,
						pd->cap.ma[idx] * 1000);
#endif
		if (oldmA > pd->cap.ma[idx])
			pd_hal_set_input_current(alg, CHG1,
				pd->cap.ma[idx] * 1000);
#endif

		ret = pd_hal_set_adapter_cap(alg, pd->cap.max_mv[idx],
			pd->cap.ma[idx]);

		/* when VPDO, mivr set as min_charger_voltage : 4400mv */
		if (pd_noti.sink_status.power_list[idx+1].pdo_type != VPDO_TYPE) {
			if (ret == 0) {
				pr_info("%s: got PSRDY.\n", __func__);
#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#ifdef FIXME
				if (info->data.parallel_vbus &&
					(oldmA * 2 < pd->cap.ma[idx])) {
					charger_dev_set_input_current(info->chg1_dev,
							pd->cap.ma[idx] * 1000 / 2);
					charger_dev_set_input_current(info->chg2_dev,
							pd->cap.ma[idx] * 1000 / 2);
				} else if (info->data.parallel_vbus == false &&
					(oldmA < pd->cap.ma[idx]))
					charger_dev_set_input_current(info->chg1_dev,
							pd->cap.ma[idx] * 1000);
#endif

				if (oldmA < pd->cap.ma[idx])
					pd_hal_set_input_current(alg, CHG1,
						pd->cap.ma[idx] * 1000);
#endif
				if ((pd->cap.max_mv[idx] - PD_VBUS_IR_DROP_THRESHOLD)
					> mivr)
					mivr = pd->cap.max_mv[idx] -
						PD_VBUS_IR_DROP_THRESHOLD;

				pd_hal_set_mivr(alg, CHG1, mivr * 1000);
			} else {
#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#ifdef FIXME
				if (info->data.parallel_vbus &&
					(oldmA * 2 > pd->cap.ma[idx])) {
					charger_dev_set_input_current(info->chg1_dev,
							oldmA * 1000 / 2);
					charger_dev_set_input_current(info->chg2_dev,
							oldmA * 1000 / 2);
				} else if (info->data.parallel_vbus == false &&
					(oldmA > pd->cap.ma[idx]))
					charger_dev_set_input_current(info->chg1_dev,
						oldmA * 1000);
#endif
				if (oldmA > pd->cap.ma[idx])
					pd_hal_set_input_current(alg, CHG1,
						oldmA * 1000);

#endif
				pd_hal_set_mivr(alg, CHG1, oldmivr);
			}
		}

		__mtk_pdc_get_idx(alg, idx,
			&pd->pd_boost_idx, &pd->pd_buck_idx);
	}

	pd_dbg("[%s]idx:%d:%d:%d:%d vbus:%d cur:%d ret:%d\n", __func__,
		pd->pd_idx, idx, pd->pd_boost_idx, pd->pd_buck_idx,
		pd->cap.max_mv[idx], pd->cap.ma[idx], ret);

	pd->pd_idx = idx;

	return ret;
}


void mtk_pdc_reset(struct chg_alg_device *alg)
{
	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);

	pd_dbg("%s: reset to default profile\n", __func__);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pdc_clear();
#endif
	__mtk_pdc_init_table(alg);
	__mtk_pdc_get_reset_idx(alg);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pdc_select_pdo(pd->pd_reset_idx + 1);
#else
	__mtk_pdc_setup(alg, pd->pd_reset_idx);
#endif
	pd_hal_vbat_mon_en(alg, CHG1, false);
	pd->old_cv = 0;
}


int __mtk_pdc_get_setting(struct chg_alg_device *alg, int *newvbus, int *newcur,
			int *newidx)
{
	int ret = 0;
	int idx, selected_idx;
#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	unsigned int pd_max_watt, pd_min_watt, now_max_watt;
#endif
	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);
	int ibus = 0, vbus;
#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	int chg2_watt = 0;
	bool boost = false, buck = false;
#endif
	struct pd_power_cap *cap;
	unsigned int mivr1 = 0;
	unsigned int mivr2 = 0;
	bool chg1_mivr = false;
	bool chg2_mivr = false;
	int chg_cnt, i, is_chip_enabled;
	int pd_role = PD_ROLE_SINK;

	__mtk_pdc_init_table(alg);
	__mtk_pdc_get_reset_idx(alg);
	__mtk_pdc_get_cap_max_watt(alg);

	cap = &pd->cap;

	if (cap->nr == 0)
		return -1;

	ret = pd_hal_get_ibus(alg, &ibus);
	if (ret < 0) {
		pd_err("[%s] get ibus fail, keep default voltage\n", __func__);
		return -1;
	}
#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#ifdef FIXME
	if (info->data.parallel_vbus) {
		ret = charger_dev_get_ibat(info->chg1_dev, &chg1_ibat);
		if (ret < 0)
			pd_dbg("[%s] get ibat fail\n", __func__);

		ret = charger_dev_get_ibat(info->chg2_dev, &chg2_ibat);
		if (ret < 0) {
			ibat = battery_get_bat_current();
			chg2_ibat = ibat * 100 - chg1_ibat;
		}

		if (ibat < 0 || chg2_ibat < 0)
			chg2_watt = 0;
		else
			chg2_watt = chg2_ibat / 1000 * battery_get_bat_voltage()
					/ info->data.chg2_eff * 100;

		pd_dbg("[%s] chg2_watt:%d ibat2:%d ibat1:%d ibat:%d\n",
			__func__, chg2_watt, chg2_ibat, chg1_ibat, ibat * 100);
	}
#endif
#endif

	pd_hal_get_mivr_state(alg, CHG1, &chg1_mivr);
	pd_hal_get_mivr(alg, CHG1, &mivr1);

	chg_cnt = pd_hal_get_charger_cnt(alg);
	if (chg_cnt > 1 && alg->config == DUAL_CHARGERS_IN_SERIES) {
		for (i = CHG2; i < CHG_MAX; i++) {
			is_chip_enabled =
					pd_hal_is_chip_enable(alg, i);
			if (is_chip_enabled) {
				pd_hal_get_mivr_state(alg, CHG2, &chg2_mivr);
				pd_hal_get_mivr(alg, CHG2, &mivr2);
			}

		}
	}

	vbus = pd_hal_get_vbus(alg);
	ibus = ibus / 1000;
	if (ibus == 0)
		ibus = 1000;

	if ((chg1_mivr && (vbus < mivr1 / 1000 - 500)) ||
	    (chg2_mivr && (vbus < mivr2 / 1000 - 500))) {
		pr_info("[%s] vbus:%d ibus:%d, chg1_mivr:%d,%d, mivr:%d,%d\n",
			__func__, vbus, ibus, chg1_mivr, chg2_mivr, mivr1, mivr2);
#if !IS_ENABLED(CONFIG_SEC_FACTORY)
		goto reset;
#endif
	}

	selected_idx = cap->selected_cap_idx;
	idx = selected_idx;

	if (idx < 0 || idx >= PD_CAP_MAX_NR)
		idx = selected_idx = 0;

#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pd_dbg("idx:%d %d %d %d %d %d\n", idx,
		cap->max_mv[idx],
		cap->ma[idx],
		cap->maxwatt[idx],
		pd->ibus_err,
		ibus);

	pd_max_watt = cap->max_mv[idx] * (cap->ma[idx]
			/ 100 * (100 - pd->ibus_err) - 100);

	pd_dbg("pd_max_watt:%d %d %d %d %d\n", idx,
		cap->max_mv[idx],
		cap->ma[idx],
		pd->ibus_err,
		pd_max_watt);


	now_max_watt = cap->max_mv[idx] * ibus + chg2_watt;
	pd_dbg("now_max_watt:%d %d %d %d %d\n", idx,
		cap->max_mv[idx],
		ibus,
		chg2_watt,
		now_max_watt);

	pd_min_watt = cap->max_mv[pd->pd_buck_idx] * cap->ma[pd->pd_buck_idx]
			/ 100 * (100 - pd->ibus_err)
			- pd->vsys_watt;

	pd_dbg("pd_min_watt:%d %d %d %d %d\n", pd->pd_buck_idx,
		cap->max_mv[pd->pd_buck_idx],
		cap->ma[pd->pd_buck_idx],
		pd->ibus_err,
		pd->vsys_watt);

	if (pd_min_watt <= 5000000)
		pd_min_watt = 5000000;

	if ((now_max_watt >= pd_max_watt) || chg1_mivr || chg2_mivr) {
		*newidx = pd->pd_boost_idx;
		boost = true;
	} else if (now_max_watt <= pd_min_watt) {
		*newidx = pd->pd_buck_idx;
		buck = true;
	} else {
		*newidx = selected_idx;
		boost = false;
		buck = false;
	}

	*newvbus = cap->max_mv[*newidx];
	*newcur = cap->ma[*newidx];

	pd_dbg("[%s]watt:%d,%d,%d up:%d,%d vbus:%d ibus:%d, mivr:%d,%d\n",
		__func__,
		pd_max_watt, now_max_watt, pd_min_watt,
		boost, buck,
		vbus, ibus, chg1_mivr, chg2_mivr);
#endif
	pr_info("[%s]vbus:%d:%d:%d current:%d idx:%d default_idx:%d\n",
		__func__, pd->vbus_h, pd->vbus_l, *newvbus,
		*newcur, *newidx, selected_idx);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pd_role = tcpm_inquire_pd_power_role(pd->tcpc_dev);
	pr_info("[%s] pd_role=%s\n", __func__, pd_role ? "SOURCE" : "SINK");
	if (pd_role != PD_ROLE_SOURCE )
		pdc_send_pd_noti(); // Don't Send Notification if the role has changed to SOURCE
#endif
	return 0;

#if !IS_ENABLED(CONFIG_SEC_FACTORY)
reset:
	mtk_pdc_reset(alg);
#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	*newidx = pd->pd_reset_idx;
	*newvbus = cap->max_mv[*newidx];
	*newcur = cap->ma[*newidx];
#endif
	return 0;
#endif
}

static int pd_sc_set_charger(struct chg_alg_device *alg)
{
	struct mtk_pd *pd;
	int ichg1_min = -1, aicr1_min = -1;
	int ret;

	pd = dev_get_drvdata(&alg->dev);

	if (pd->input_current_limit1 == 0 ||
		pd->charging_current_limit1 == 0) {
		pr_notice("input/charging current is 0, end Pd\n");
		return -1;
	}

	mutex_lock(&pd->data_lock);
	if (pd->charging_current_limit1 != -1) {
		if (pd->charging_current_limit1 <
			pd->sc_charger_current)
			pd->charging_current1 =
				pd->charging_current_limit1;
		ret = pd_hal_get_min_charging_current(alg, CHG1, &ichg1_min);
		if (ret != -EOPNOTSUPP &&
			pd->charging_current_limit1 < ichg1_min)
			pd->charging_current1 = 0;
	} else
		pd->charging_current1 = pd->sc_charger_current;

	if (pd->input_current_limit1 != -1 &&
		pd->input_current_limit1 <
		pd->sc_input_current) {
		pd->input_current1 = pd->input_current_limit1;
		ret = pd_hal_get_min_input_current(alg, CHG1, &aicr1_min);
		if (ret != -EOPNOTSUPP &&
			pd->input_current_limit1 < aicr1_min)
			pd->input_current1 = 0;
	} else
		pd->input_current1 = pd->sc_input_current;
	mutex_unlock(&pd->data_lock);


	if (pd->input_current1 == 0 ||
		pd->charging_current1 == 0) {
		pd_err("current is zero %d %d\n",
			pd->input_current1,
			pd->charging_current1);
		return -1;
	}

	pd_hal_set_charging_current(alg,
		CHG1, pd->charging_current1);
	pd_hal_set_input_current(alg,
		CHG1, pd->input_current1);

	if (pd->old_cv == 0 || (pd->old_cv != pd->cv) || pd->pd_6pin_en == 0) {
		pd_hal_vbat_mon_en(alg, CHG1, false);
		pd_hal_set_cv(alg, CHG1, pd->cv);
		if (pd->pd_6pin_en && pd->stop_6pin_re_en != 1)
			pd_hal_vbat_mon_en(alg, CHG1, true);

		pd->old_cv = pd->cv;
	} else {
		if (pd->pd_6pin_en && pd->stop_6pin_re_en != 1) {
			pd->stop_6pin_re_en = 1;
			pd_hal_vbat_mon_en(alg, CHG1, true);
		}
	}

	pd_dbg("%s old_cv=%d, new_cv=%d, pd_6pin_en=%d 6pin_re_en=%d\n", __func__,
		pd->old_cv, pd->cv, pd->pd_6pin_en, pd->stop_6pin_re_en);

	return 0;
}

static int pd_dcs_set_charger(struct chg_alg_device *alg)
{
	struct mtk_pd *pd;
	bool chg2_enable = true;
	bool chg2_chip_enabled = false;
	int ret;
	int ichg1_min = -1, ichg2_min = -1;
	int aicr1_min = -1;

	pd = dev_get_drvdata(&alg->dev);

	if (pd->input_current_limit1 == 0 ||
		pd->charging_current_limit1 == 0 ||
		pd->charging_current_limit2 == 0) {
		pr_notice("input/charging current is 0, end PD\n");
		return -1;
	}

	mutex_lock(&pd->data_lock);
	if (pd->input_current_limit1 != -1 &&
		pd->input_current_limit1 <
		pd->dcs_input_current) {
		pd->input_current1 = pd->input_current_limit1;
		ret = pd_hal_get_min_input_current(alg, CHG1, &aicr1_min);
		if (ret != -EOPNOTSUPP &&
			pd->input_current_limit1 < aicr1_min)
			pd->input_current1 = 0;
	} else
		pd->input_current1 = pd->dcs_input_current;

	if (pd->charging_current_limit1 != -1 &&
		pd->charging_current_limit1 <
		pd->dcs_chg1_charger_current) {
		pd->charging_current1 = pd->charging_current_limit1;
		ret = pd_hal_get_min_charging_current(alg, CHG1, &ichg1_min);
		if (ret != -EOPNOTSUPP &&
			pd->charging_current_limit1 < ichg1_min)
			pd->charging_current1 = 0;
	} else
		pd->charging_current1 = pd->dcs_chg1_charger_current;

	if (pd->state == PD_RUN)
		pd->charging_current2 = pd->dcs_chg2_charger_current;

	if (pd->charging_current_limit2 != -1 &&
		pd->charging_current_limit2 <
		pd->charging_current2) {
		pd->charging_current2 = pd->charging_current_limit2;
		ret = pd_hal_get_min_charging_current(alg, CHG2, &ichg2_min);
		if (ret != -EOPNOTSUPP &&
			pd->charging_current_limit2 < ichg2_min)
			pd->charging_current2 = 0;
	}
	mutex_unlock(&pd->data_lock);

	if (pd->input_current1 == 0 ||
		pd->charging_current1 == 0 ||
		pd->charging_current2 == 0) {
		pd_err("current is zero %d %d %d\n",
			pd->input_current1,
			pd->charging_current1,
			pd->charging_current2);
		pd_hal_enable_charger(alg, CHG2, false);
		pd_hal_charger_enable_chip(alg, CHG2, false);
		return -1;
	}

	chg2_chip_enabled = pd_hal_is_chip_enable(alg, CHG2);
	pd_dbg("chg2_en:%d %d %d\n",
		chg2_enable, chg2_chip_enabled, pd->state);
	if (pd->state == PD_RUN) {
		if (!chg2_chip_enabled)
			pd_hal_charger_enable_chip(alg, CHG2, true);
		pd_hal_enable_charger(alg, CHG2, true);
		pd_hal_set_input_current(alg,
			CHG2, pd->charging_current2);
		pd_hal_set_charging_current(alg,
			CHG2, pd->charging_current2);

		pd_hal_set_eoc_current(alg, CHG1,
			pd->dual_polling_ieoc);
		pd_hal_enable_termination(alg, CHG1, false);
		pd_hal_safety_check(alg, pd->dual_polling_ieoc);
	} else if (pd->state == PD_TUNING) {
		if (!chg2_chip_enabled)
			pd_hal_charger_enable_chip(alg, CHG2, true);
		pd_hal_enable_charger(alg, CHG2, true);
		pd_hal_set_eoc_current(alg, CHG1, pd->dual_polling_ieoc);
		pd_hal_enable_termination(alg, CHG1, false);
		pd_hal_safety_check(alg, pd->dual_polling_ieoc);
	} else if (pd->state == PD_POSTCC) {
		pd_hal_set_eoc_current(alg, CHG1, 150000);
		pd_hal_enable_termination(alg, CHG1, true);
	} else {
		pd_err("%s state error!", __func__);
		return -1;
	}

	pd_hal_set_charging_current(alg,
		CHG1, pd->charging_current1);
	pd_hal_set_input_current(alg,
		CHG1, pd->input_current1);
	pd_hal_set_cv(alg,
		CHG1, pd->cv);

	pd_dbg("%s m:%d s:%d cv:%d chg1:%d,%d chg2:%d,%d chg2en:%d min:%d,%d,%d\n",
		__func__,
		alg->config,
		pd->state,
		pd->cv,
		pd->input_current1,
		pd->charging_current1,
		pd->input_current2,
		pd->charging_current2,
		chg2_enable,
		ichg1_min,
		ichg2_min,
		aicr1_min);

	return 0;
}

static int __pd_run(struct chg_alg_device *alg)
{
	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);
	int vbus = 0, cur = 0, idx = 0, ret = 0, ret_value = ALG_RUNNING;

	pr_info("[%s] ret:%d\n", __func__, ret);
	ret = __mtk_pdc_get_setting(alg, &vbus, &cur, &idx);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	ret_value = ALG_DONE;
	return ret_value;
#endif


	if (ret != -1 && idx != -1) {
		if ((pd->input_current_limit1 != -1 &&
			pd->input_current_limit1 < cur * 1000) == false)
			pd->input_current_limit1 = cur * 1000;
		__mtk_pdc_setup(alg, idx);
	} else {
		pd->input_current_limit1 =
			PD_FAIL_CURRENT;
		pd->charging_current_limit1 =
			PD_FAIL_CURRENT;
	}

	if (alg->config == DUAL_CHARGERS_IN_SERIES) {
		if (pd_dcs_set_charger(alg) != 0) {
			ret_value = ALG_DONE;
			//goto out;
		}
	} else {
		if (pd_sc_set_charger(alg) != 0) {
			ret_value = ALG_DONE;
			//goto out;
		}
	}

	return ret_value;
}

static int _pd_start_algo(struct chg_alg_device *alg)
{
	int ret_value = 0;
	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);
	bool again = false;
#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	int uisoc;
#endif

	mutex_lock(&pd->access_lock);

	if (algo_waiver_test) {
		ret_value = ALG_WAIVER;
		goto skip;
	}

	do {
		pd_info("%s state:%d %s %d\n", __func__,
			pd->state,
			pd_state_to_str(pd->state),
			again);
		again = false;

		switch (pd->state) {
		case PD_HW_UNINIT:
		case PD_HW_FAIL:
			ret_value = ALG_INIT_FAIL;
			break;
		case PD_HW_READY:
			ret_value = pd_hal_is_pd_adapter_ready(alg);
			if (ret_value == ALG_TA_NOT_SUPPORT)
				pd->state = PD_TA_NOT_SUPPORT;
			else if (ret_value == ALG_READY) {
#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
				uisoc = pd_hal_get_uisoc(alg);
				if (pd->input_current_limit1 != -1 ||
					pd->charging_current_limit1 != -1 ||
					pd->input_current_limit2 != -1 ||
					pd->charging_current_limit2 != -1)
					ret_value = ALG_NOT_READY;
				else if (uisoc >= pd->pd_stop_battery_soc ||
					(uisoc == -1 && pd->ref_vbat > pd->vbat_threshold))
					ret_value = ALG_WAIVER;
				else {
#else
				{
#endif
					pd->state = PD_RUN;
					again = true;
				}
			}
			break;
		case PD_TA_NOT_SUPPORT:
			ret_value = ALG_TA_NOT_SUPPORT;
			break;
		case PD_RUN:
		case PD_TUNING:
		case PD_POSTCC:
			ret_value = __pd_run(alg);
			break;
		default:
			pd_err("PD unknown state:%d\n", pd->state);
			ret_value = ALG_INIT_FAIL;
			break;
		}
	} while (again == true);
skip:
	mutex_unlock(&pd->access_lock);

	return ret_value;
}


static bool _pd_is_algo_running(struct chg_alg_device *alg)
{
	struct mtk_pd *pd;

	pd_dbg("%s\n", __func__);
	pd = dev_get_drvdata(&alg->dev);

	if (pd->state == PD_RUN || pd->state == PD_TUNING
		|| pd->state == PD_POSTCC)
		return true;

	return false;
}

static int _pd_stop_algo(struct chg_alg_device *alg)
{
	int ret_value = 0;
	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);

	mutex_lock(&pd->access_lock);

	pd_info("%s state:%d %s\n", __func__,
		pd->state,
		pd_state_to_str(pd->state));

	switch (pd->state) {
	case PD_HW_UNINIT:
	case PD_HW_FAIL:
	case PD_HW_READY:
	case PD_TA_NOT_SUPPORT:
		break;
	case PD_TUNING:
	case PD_POSTCC:
	case PD_RUN:
		mtk_pdc_reset(alg);
		pd_hal_set_charging_current(alg,
			CHG1, PD_FAIL_CURRENT);
		pd_hal_set_input_current(alg,
			CHG1, PD_FAIL_CURRENT);
		pd_hal_set_cv(alg,
			CHG1, pd->cv);
		pd->state = PD_HW_READY;
		if (alg->config == DUAL_CHARGERS_IN_SERIES) {
			pd_hal_enable_charger(alg, CHG2, false);
			pd_hal_charger_enable_chip(alg,
			CHG2, false);
		}
		break;
	default:
		pd_err("PD unknown state:%d\n", pd->state);
		ret_value = ALG_INIT_FAIL;
		break;
	}
	mutex_unlock(&pd->access_lock);

	return ret_value;
}

static int pd_full_evt(struct chg_alg_device *alg)
{
	struct mtk_pd *pd;
	int ret = 0;
	bool chg_en = true, chg2_enabled = false;
	int ichg2 = 0, ichg2_min = 100000;
	int ret_value = 0;

	pd = dev_get_drvdata(&alg->dev);
	switch (pd->state) {
	case PD_HW_UNINIT:
	case PD_HW_FAIL:
	case PD_HW_READY:
	case PD_TA_NOT_SUPPORT:
		break;
	case PD_TUNING:
	case PD_POSTCC:
	case PD_RUN:
		if (alg->config == DUAL_CHARGERS_IN_SERIES) {
			pd_hal_is_charger_enable(
				alg, CHG2, &chg_en);
			chg2_enabled = pd_hal_is_chip_enable(alg, CHG2);

			if (!chg_en || !chg2_enabled) {
				/* notify eoc , fix me */
				pd->state = PD_HW_READY;
				pd_err("%s: charging done:%d %d\n",
					__func__, chg_en, chg2_enabled);
				if (alg->is_polling_mode == false)
					ret_value = 1;
			} else {
				pd_hal_get_charging_current(alg, CHG2, &ichg2);
				ret = pd_hal_get_min_charging_current(
					alg, CHG2, &ichg2_min);
				if (ret == -EOPNOTSUPP)
					ichg2_min = 100000;

				pd_err("ichg2:%d, ichg2_min:%d state:%d\n",
					ichg2, ichg2_min, pd->state);
				if (ichg2 - 500000 <= ichg2_min) {
					pd->state = PD_POSTCC;
					pd_hal_enable_charger(alg,
						CHG2, false);
					pd_hal_set_eoc_current(alg,
						CHG1, 150000);
					pd_hal_enable_termination(alg,
						CHG1, true);
				} else {
					pd->state = PD_TUNING;
					mutex_lock(&pd->data_lock);
					if (pd->charging_current2 >= 500000)
						pd->charging_current2 =
							ichg2 - 500000;
					pd_hal_set_charging_current(alg,
						CHG2, pd->charging_current2);
					mutex_unlock(&pd->data_lock);
				}
				ret_value = 1;
			}

		} else {
			if (pd->state == PD_RUN) {
				pd_err("%s evt full\n",  __func__);
				pd->state = PD_HW_READY;
			}
		}
		break;
	default:
		pd_err("PD unknown state:%d\n", pd->state);
		ret_value = ALG_INIT_FAIL;
		break;
	}
	return ret_value;
}

static int pd_plugout_reset(struct chg_alg_device *alg)
{
	struct mtk_pd *pd;

	pd = dev_get_drvdata(&alg->dev);
	switch (pd->state) {
	case PD_HW_UNINIT:
	case PD_HW_FAIL:
	case PD_HW_READY:
		break;
	case PD_TA_NOT_SUPPORT:
		pd->state = PD_HW_READY;
		break;
	case PD_TUNING:
	case PD_POSTCC:
	case PD_RUN:
		pd->state = PD_HW_READY;
		if (alg->config == DUAL_CHARGERS_IN_SERIES) {
			pd_hal_enable_charger(alg, CHG2, false);
			pd_hal_charger_enable_chip(alg,
			CHG2, false);
		}
		pd->pd_cap_max_watt = -1;
		pd->pd_idx = -1;
		pd->pd_reset_idx = -1;
		pd->pd_boost_idx = 0;
		pd->pd_buck_idx = 0;
		break;
	default:
		pd_err("PD unknown state:%d\n", pd->state);
		break;
	}
	return 0;
}

static int _pd_notifier_call(struct chg_alg_device *alg,
			 struct chg_alg_notify *notify)
{
	struct mtk_pd *pd;
	int ret_value = 0;

	pd = dev_get_drvdata(&alg->dev);
	pd_dbg("%s evt:%d state:%s\n", __func__, notify->evt,
		pd_state_to_str(pd->state));

	switch (notify->evt) {
	case EVT_PLUG_OUT:
		pd->stop_6pin_re_en = 0;
		ret_value = pd_plugout_reset(alg);
		break;
	case EVT_FULL:
		pd->stop_6pin_re_en = 1;
		ret_value = pd_full_evt(alg);
		break;
	case EVT_BATPRO_DONE:
		pd->pd_6pin_en = 0;
		ret_value = 0;
		break;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	case EVT_HARDRESET:
		ret_value = pdc_hard_rst();
		break;
	case EVT_DETACH:
		ret_value = pdc_clear();
		break;
#endif
	default:
		ret_value = -EINVAL;
	}

	return ret_value;
}

static int pdc_tcp_notifier_call(struct notifier_block *pnb,
						unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct mtk_pd *pd;
	int ret = 0;
	union power_supply_propval value = {0, };

	pd = container_of(pnb, struct mtk_pd, pd_nb);

	pr_notice("%s: PDC charger event:%d %d\n", __func__, (int)event,
			(int)noti->pd_state.connected);

	switch (event) {
	case TCP_NOTIFY_PD_STATE:
		switch (noti->pd_state.connected) {
		case PD_CONNECT_NEW_SRC_CAP:
			pd->is_srccap_changed = 1;
			value.intval = 0;
			psy_do_property("battery", set, POWER_SUPPLY_EXT_PROP_SRCCAP, value);
			pr_info("%s : PD_CONNECT_NEW_SRC_CAP is_srccap_changed(%d)\n", __func__, pd->is_srccap_changed);
			break;
		}
	}

	return ret;
}

static void mtk_pd_parse_dt(struct mtk_pd *pd,
				struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 val;

	if (of_property_read_u32(np, "min_charger_voltage", &val) >= 0)
		pd->min_charger_voltage = val;
	else {
		pd_err("use default V_CHARGER_MIN:%d\n", V_CHARGER_MIN);
		pd->min_charger_voltage = V_CHARGER_MIN;
	}

	/* PD */
	if (of_property_read_u32(np, "pd_vbus_upper_bound", &val) >= 0) {
		pd->vbus_h = val / 1000;
	} else {
		pd_err("use default pd_vbus_upper_bound:%d\n",
			PD_VBUS_UPPER_BOUND);
		pd->vbus_h = PD_VBUS_UPPER_BOUND / 1000;
	}

	if (of_property_read_u32(np, "pd_vbus_low_bound", &val) >= 0) {
		pd->vbus_l = val / 1000;
	} else {
		pd_err("use default pd_vbus_low_bound:%d\n",
			PD_VBUS_LOW_BOUND);
		pd->vbus_l = PD_VBUS_LOW_BOUND / 1000;
	}

	if (of_property_read_u32(np, "vsys_watt", &val) >= 0) {
		pd->vsys_watt = val;
	} else {
		pd_err("use default vsys_watt:%d\n",
			VSYS_WATT);
		pd->vsys_watt = VSYS_WATT;
	}

	if (of_property_read_u32(np, "ibus_err", &val) >= 0) {
		pd->ibus_err = val;
	} else {
		pd_err("use default ibus_err:%d\n",
			IBUS_ERR);
		pd->ibus_err = IBUS_ERR;
	}

	if (of_property_read_u32(np, "pd_stop_battery_soc", &val) >= 0)
		pd->pd_stop_battery_soc = val;
	else {
		pd_err("use default pd_stop_battery_soc:%d\n",
			PD_STOP_BATTERY_SOC);
		pd->pd_stop_battery_soc = PD_STOP_BATTERY_SOC;
	}

	/* single charger */
	if (of_property_read_u32(np, "sc_input_current", &val) >= 0) {
		pd->sc_input_current = val;
	} else {
		pd_err("use default sc_input_current:%d\n",
			PD_SC_INPUT_CURRENT);
		pd->sc_input_current = PD_SC_INPUT_CURRENT;
	}

	if (of_property_read_u32(np, "sc_charger_current", &val) >= 0) {
		pd->sc_charger_current = val;
	} else {
		pd_err("use default sc_charger_current:%d\n",
			PD_SC_CHARGER_CURRENT);
		pd->sc_charger_current = PD_SC_CHARGER_CURRENT;
	}

	/* dual charger in series*/
	if (of_property_read_u32(np, "dcs_input_current", &val) >= 0) {
		pd->dcs_input_current = val;
	} else {
		pd_err("use default dcs_input_current:%d\n",
			PD_DCS_INPUT_CURRENT);
		pd->dcs_input_current = PD_DCS_INPUT_CURRENT;
	}

	if (of_property_read_u32(np, "dcs_chg1_charger_current", &val) >= 0) {
		pd->dcs_chg1_charger_current = val;
	} else {
		pd_err("use default dcs_chg1_charger_current:%d\n",
			PD_DCS_CHG1_CHARGER_CURRENT);
		pd->dcs_chg1_charger_current = PD_DCS_CHG1_CHARGER_CURRENT;
	}

	if (of_property_read_u32(np, "dcs_chg2_charger_current", &val) >= 0) {
		pd->dcs_chg2_charger_current = val;
	} else {
		pd_err("use default dcs_chg2_charger_current:%d\n",
			PD_DCS_CHG2_CHARGER_CURRENT);
		pd->dcs_chg2_charger_current = PD_DCS_CHG2_CHARGER_CURRENT;
	}

	/* dual charger */
	if (of_property_read_u32(np, "slave_mivr_diff", &val) >= 0)
		pd->slave_mivr_diff = val;
	else {
		pd_err("use default SLAVE_MIVR_DIFF:%d\n", SLAVE_MIVR_DIFF);
		pd->slave_mivr_diff = SLAVE_MIVR_DIFF;
	}

	if (of_property_read_u32(np, "dual_polling_ieoc", &val) >= 0)
		pd->dual_polling_ieoc = val;
	else {
		pd_err("use default dual_polling_ieoc :%d\n", 750000);
		pd->dual_polling_ieoc = 750000;
	}

	if (of_property_read_u32(np, "vbat_threshold", &val) >= 0)
		pd->vbat_threshold = val;
	else {
		pr_notice("turn off vbat_threshold checking:%d\n",
			DISABLE_VBAT_THRESHOLD);
		pd->vbat_threshold = DISABLE_VBAT_THRESHOLD;
	}

}

int _pd_get_prop(struct chg_alg_device *alg,
		enum chg_alg_props s, int *value)
{

	pr_notice("%s\n", __func__);
	if (s == ALG_MAX_VBUS)
		*value = 10000;
	else
		pr_notice("%s does not support prop:%d\n", __func__, s);
	return 0;
}

int _pd_set_setting(struct chg_alg_device *alg_dev,
	struct chg_limit_setting *setting)
{
	struct mtk_pd *pd;

	pd_dbg("%s cv:%d icl:%d,%d cc:%d,%d\n",
		__func__,
		setting->cv,
		setting->input_current_limit1,
		setting->input_current_limit2,
		setting->charging_current_limit1,
		setting->charging_current_limit2);
	pd = dev_get_drvdata(&alg_dev->dev);

	mutex_lock(&pd->access_lock);
	pd->cv = setting->cv;
	pd->pd_6pin_en = setting->vbat_mon_en;
	pd->input_current_limit1 = setting->input_current_limit1;
	pd->charging_current_limit1 = setting->charging_current_limit1;
	pd->input_current_limit2 = setting->input_current_limit2;
	pd->charging_current_limit2 = setting->charging_current_limit2;
	mutex_unlock(&pd->access_lock);

	return 0;
}

int _pd_set_prop(struct chg_alg_device *alg,
		enum chg_alg_props s, int value)
{
	struct mtk_pd *pd;

	pr_notice("%s %d %d\n", __func__, s, value);

	pd = dev_get_drvdata(&alg->dev);

	switch (s) {
	case ALG_LOG_LEVEL:
		pd_dbg_level = value;
		break;
	case ALG_REF_VBAT:
		pd->ref_vbat = value;
		break;
	default:
		break;
	}

	return 0;
}


static struct chg_alg_ops pd_alg_ops = {
	.init_algo = _pd_init_algo,
	.is_algo_ready = _pd_is_algo_ready,
	.start_algo = _pd_start_algo,
	.is_algo_running = _pd_is_algo_running,
	.stop_algo = _pd_stop_algo,
	.notifier_call = _pd_notifier_call,
	.get_prop = _pd_get_prop,
	.set_prop = _pd_set_prop,
	.set_current_limit = _pd_set_setting,
};

static int mtk_pd_probe(struct platform_device *pdev)
{
	struct mtk_pd *pd = NULL;
	int ret = 0;

	pr_notice("%s: starts\n", __func__);

	pd = devm_kzalloc(&pdev->dev, sizeof(*pd), GFP_KERNEL);
	if (!pd)
		return -ENOMEM;
	platform_set_drvdata(pdev, pd);
	pd->pdev = pdev;
	mutex_init(&pd->access_lock);
	mutex_init(&pd->data_lock);
	mtk_pd_parse_dt(pd, &pdev->dev);
	pd->bat_psy = devm_power_supply_get_by_phandle(&pdev->dev, "gauge");
	if (IS_ERR_OR_NULL(pd->bat_psy))
		pd_err("%s: devm power fail to get bat_psy\n", __func__);

	pd->alg = chg_alg_device_register("pd", &pdev->dev,
					pd, &pd_alg_ops, NULL);

	pd->is_srccap_changed = 0;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pd->fpdo_num = 0;
	pd->apdo_num = 0;
	pd->unknown_num = 0;
	pd->ps_rdy = 0;
	pd->prev_available_pdo = -1;
	pd->was_hard_rst = 0;
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	pd_noti.sink_status.current_pdo_num = 0;
	pd_noti.sink_status.selected_pdo_num = 0;
	pd_noti.sink_status.fp_sec_pd_select_pdo = pdc_select_pdo;
	pd_noti.sink_status.fp_sec_pd_select_pps = pdc_select_pps;
	pd_noti.sink_status.fp_sec_pd_change_src = pdc_usbpd_forced_change_srccap;
#endif
	pd->tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
	if (!pd->tcpc_dev) {
		pr_err("%s get tcpc dev fail\n", __func__);
		return -ENODEV;
	}
	
	pd->pd_nb.notifier_call = pdc_tcp_notifier_call;
	
	ret = register_tcp_dev_notifier(pd->tcpc_dev, &pd->pd_nb,
			TCP_NOTIFY_TYPE_ALL);
	if (ret < 0) {
		pr_notice("%s: register tcpc notifer fail\n", __func__);
		return -EINVAL;	
	}
	
#endif
	return 0;
}

static int mtk_pd_remove(struct platform_device *dev)
{
	return 0;
}

static void mtk_pd_shutdown(struct platform_device *dev)
{

}

static const struct of_device_id mtk_pd_of_match[] = {
	{.compatible = "mediatek,charger,pd",},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_pd_of_match);

struct platform_device pd_device = {
	.name = "pd",
	.id = -1,
};

static struct platform_driver pd_driver = {
	.probe = mtk_pd_probe,
	.remove = mtk_pd_remove,
	.shutdown = mtk_pd_shutdown,
	.driver = {
		   .name = "pd",
		   .of_match_table = mtk_pd_of_match,
	},
};

static int __init mtk_pd_init(void)
{
	return platform_driver_register(&pd_driver);
}
module_init(mtk_pd_init);

static void __exit mtk_pd_exit(void)
{
	platform_driver_unregister(&pd_driver);
}
module_exit(mtk_pd_exit);


MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK PD algorithm Driver");
MODULE_LICENSE("GPL");
