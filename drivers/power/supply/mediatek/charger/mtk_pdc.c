/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/slab.h>
#if defined(CONFIG_BATTERY_SAMSUNG)
#if defined(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_notifier.h>
#endif
#if defined(CONFIG_BATTERY_NOTIFIER)
#include <linux/battery/battery_notifier.h>
#else
#include <linux/battery/sec_pd.h>
#endif
#endif
#include "mtk_intf.h"
#include <tcpm.h>


#define PD_MIN_WATT 5000000
#define PD_VBUS_IR_DROP_THRESHOLD 1200

static struct pdc *pd;

bool pdc_is_ready(void)
{
	return adapter_is_support_pd();
}

void pdc_init_table(void)
{
	pd->cap.nr = 0;
	pd->cap.selected_cap_idx = -1;

	if (pdc_is_ready())
		adapter_get_cap(&pd->cap);
	else
		chr_err("mtk_is_pdc_ready is fail\n");

	chr_err("[%s] nr:%d default:%d\n", __func__, pd->cap.nr,
	pd->cap.selected_cap_idx);
}

void pdc_get_reset_idx(void)
{
	struct pd_cap *cap;
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
		chr_err("[%s]reset idx:%d vbus:%d %d\n", __func__,
			idx, cap->min_mv[idx], cap->max_mv[idx]);
	}
}

int pdc_set_mivr(int uV)
{
	int ret = 0;

	ret = charger_set_mivr(uV);
	if (ret < 0)
		chr_err("%s: failed, ret = %d\n", __func__, ret);

	return ret;
}


int pdc_get_idx(int selected_idx,
	int *boost_idx, int *buck_idx)
{
	struct pd_cap *cap;
	int i = 0;
	int idx = 0;

	cap = &pd->cap;
	idx = selected_idx;

	if (idx < 0) {
		chr_err("[%s] invalid idx:%d\n", __func__, idx);
		*boost_idx = 0;
		*buck_idx = 0;
		return -1;
	}

	/* get boost_idx */
	for (i = 0; i < cap->nr; i++) {

		if (cap->min_mv[i] < pd->vbus_l ||
			cap->max_mv[i] < pd->vbus_l) {
			chr_err("min_mv error:%d %d %d\n",
					cap->min_mv[i],
					cap->max_mv[i],
					pd->vbus_l);
			continue;
		}

		if (cap->min_mv[i] > pd->vbus_h ||
			cap->max_mv[i] > pd->vbus_h) {
			chr_err("max_mv error:%d %d %d\n",
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
			chr_err("min_mv error:%d %d %d\n",
					cap->min_mv[i],
					cap->max_mv[i],
					pd->vbus_l);
			continue;
		}

		if (cap->min_mv[i] > pd->vbus_h ||
			cap->max_mv[i] > pd->vbus_h) {
			chr_err("max_mv error:%d %d %d\n",
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

#if defined(CONFIG_BATTERY_SAMSUNG)
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

void pdc_select_pdo(int idx)
{
	int ret = -100;
	unsigned int mivr;
	unsigned int oldmivr = 4600000;
	bool force_update = false;

	ret = tcpm_reset_pd_charging_policy(pd->tcpc, NULL);
	if (ret != TCP_DPM_RET_SUCCESS)
		chr_err("[%s] tcpm_reset_pd_charging_policy() : ret(%d)\n", __func__, ret);

	idx = idx - 1;
	pr_info("%s: selecting idx:%d(sec_batt:%d)\n", __func__, idx, idx + 1);

	if (pd->pd_idx == idx) {
		charger_get_mivr(&oldmivr);

		if (pd->cap.max_mv[idx] - oldmivr / 1000 >
			PD_VBUS_IR_DROP_THRESHOLD)
			force_update = true;
	}

	if (pd->pd_idx != idx || force_update) {
		if (pd->cap.max_mv[idx] > 5000)
			enable_vbus_ovp(false);
		else
			enable_vbus_ovp(true);

		charger_get_mivr(&oldmivr);
		mivr = pd->data.min_charger_voltage / 1000;
		pdc_set_mivr(pd->data.min_charger_voltage);

		ret = adapter_set_cap(pd->cap.max_mv[idx], pd->cap.ma[idx]);

		if (ret == ADAPTER_OK) {
			pr_info("%s: got PSRDY.\n", __func__);

			if ((pd->cap.max_mv[idx] - PD_VBUS_IR_DROP_THRESHOLD)
				> mivr)
				mivr = pd->cap.max_mv[idx] -
					PD_VBUS_IR_DROP_THRESHOLD;

			pdc_set_mivr(mivr * 1000);
		} else {
			pdc_set_mivr(oldmivr);
		}

		pdc_get_idx(idx, &pd->pd_boost_idx, &pd->pd_buck_idx);
	}

	pd->pd_idx = idx;

	chr_err("[%s]idx:%d:%d:%d:%d vbus:%d cur:%d ret:%d\n", __func__,
		pd->pd_idx, idx, pd->pd_boost_idx, pd->pd_buck_idx,
		pd->cap.max_mv[idx], pd->cap.ma[idx], ret);

#if defined(CONFIG_BATTERY_SAMSUNG) && defined(CONFIG_PDIC_NOTIFIER)
	pd_noti.sink_status.selected_pdo_num = idx + 1;
	pd_noti.sink_status.current_pdo_num = idx + 1;
#endif

	pr_info("%s: checking capabilities\n", __func__);
	pdc_get_setting();
}

int pdc_select_pps(int num, int ppsVol, int ppsCur)
{
	int ret = -100, idx = num-1;

	pr_info("%s : num(%d), ppsVol(%d), ppsCur(%d)\n",
		__func__, num, ppsVol, ppsCur);

	if (pd->pd_idx != idx) {
		pr_info("%s : start\n", __func__);
		adapter_set_cap_start(ppsVol, ppsCur);
	}

	ret = adapter_set_cap(ppsVol, ppsCur);
	if (ret != ADAPTER_OK)
		pr_info("%s : not OK(%d)\n", __func__, ret);

	if (pd->pd_idx != idx) {
		pr_info("%s: checking capabilities\n", __func__);
		pdc_get_setting();
	}

	pd->pd_idx = idx;
	return ret;
}
#else
int pdc_setup(int idx)
{
	int ret = -100;
	unsigned int mivr;
	unsigned int oldmivr = 4600000;
	unsigned int oldmA = 3000000;
	bool force_update = false;

	if (pd->pd_idx == idx) {
		charger_get_mivr(&oldmivr);

		if (pd->cap.max_mv[idx] - oldmivr / 1000 >
			PD_VBUS_IR_DROP_THRESHOLD)
			force_update = true;
	}

	if (pd->pd_idx != idx || force_update) {
		if (pd->cap.max_mv[idx] > 5000)
			enable_vbus_ovp(false);
		else
			enable_vbus_ovp(true);

		charger_get_mivr(&oldmivr);
		mivr = pd->data.min_charger_voltage / 1000;
		pdc_set_mivr(pd->data.min_charger_voltage);

		charger_get_input_current(&oldmA);
		oldmA = oldmA / 1000;

		if (oldmA > pd->cap.ma[idx])
			charger_set_input_current(pd->cap.ma[idx] * 1000);

		ret = adapter_set_cap(pd->cap.max_mv[idx], pd->cap.ma[idx]);

		if (ret == ADAPTER_OK) {
			if (oldmA < pd->cap.ma[idx])
				charger_set_input_current(pd->cap.ma[idx]
								* 1000);

			if ((pd->cap.max_mv[idx] - PD_VBUS_IR_DROP_THRESHOLD)
				> mivr)
				mivr = pd->cap.max_mv[idx] -
					PD_VBUS_IR_DROP_THRESHOLD;

			pdc_set_mivr(mivr * 1000);
		} else {
			if (oldmA > pd->cap.ma[idx])
				charger_set_input_current(oldmA * 1000);

			pdc_set_mivr(oldmivr);
		}

		pdc_get_idx(idx, &pd->pd_boost_idx, &pd->pd_buck_idx);
	}

	chr_err("[%s]idx:%d:%d:%d:%d vbus:%d cur:%d ret:%d\n", __func__,
		pd->pd_idx, idx, pd->pd_boost_idx, pd->pd_buck_idx,
		pd->cap.max_mv[idx], pd->cap.ma[idx], ret);

	pd->pd_idx = idx;

	return ret;
}
#endif

void pdc_get_cap_max_watt(void)
{
	struct pd_cap *cap;
	int i = 0;
	int idx = 0;

	cap = &pd->cap;

	if (pd->pd_cap_max_watt == -1) {
		for (i = 0; i < cap->nr; i++) {
			if (cap->min_mv[i] <= pd->vbus_h ||
				cap->max_mv[i] <= pd->vbus_h) {

				if (cap->maxwatt[i] > pd->pd_cap_max_watt) {
					pd->pd_cap_max_watt = cap->maxwatt[i];
					idx = i;
				}
				continue;
			}
		}
		chr_err("[%s]idx:%d vbus:%d %d maxwatt:%d\n", __func__,
			idx, cap->min_mv[idx], cap->max_mv[idx],
			pd->pd_cap_max_watt);
	}
}

#if defined(CONFIG_BATTERY_SAMSUNG)
int pdc_clear(void)
{
#if defined(CONFIG_BATTERY_SAMSUNG) && defined(CONFIG_PDIC_NOTIFIER)
	PD_NOTI_TYPEDEF pdic_noti;
#endif
	chr_err("%s: clear selected pdo\n", __func__);
	pd->data.fpdo_num = 0;
	pd->data.apdo_num = 0;
	pd->data.unknown_num = 0;
	pd->data.ps_rdy = 0;
	pd->data.prev_available_pdo = -1;
#if defined(CONFIG_BATTERY_SAMSUNG) && defined(CONFIG_PDIC_NOTIFIER)
	pdic_noti.src = PDIC_NOTIFY_DEV_PDIC;
	pdic_noti.dest = PDIC_NOTIFY_DEV_BATT;
	pdic_noti.id = PDIC_NOTIFY_ID_POWER_STATUS;
	pdic_noti.sub1 = 0;
	pdic_noti.sub2 = 0;
	pdic_noti.sub3 = 0;
	pd_noti.sink_status.current_pdo_num = 0;
	pd_noti.sink_status.selected_pdo_num = 0;
	if (pd_noti.event != PDIC_NOTIFY_EVENT_DETACH) {
		pd_noti.event = PDIC_NOTIFY_EVENT_DETACH;
		pdic_notifier_notify((PD_NOTI_TYPEDEF *)&pdic_noti, &pd_noti, 0);
	}
	if (pd_noti.sink_status.has_apdo) {
		adapter_set_cap_end(5000, 2000);
		pd_noti.sink_status.has_apdo = false;
	}
	pd_noti.sink_status.pps_voltage = 0;
	pd_noti.sink_status.pps_current = 0;
#endif
	return 0;
}

int pdc_hard_rst(void)
{
	chr_err("%s: hard reset\n", __func__);
	pd->data.was_hard_rst = 1;
	pdc_clear();

	return 0;
}
#endif

int pdc_reset(void)
{
	if (pd == NULL || !pdc_is_ready())
		return -1;

	chr_err("%s: reset to default profile\n", __func__);
#if defined(CONFIG_BATTERY_SAMSUNG)
	pdc_clear();
#endif
	pdc_init_table();
	pdc_get_reset_idx();
#if defined(CONFIG_BATTERY_SAMSUNG)
	pdc_select_pdo(pd->pd_reset_idx + 1);
#else
	pdc_setup(pd->pd_reset_idx);
#endif

	return 0;
}

int pdc_stop(void)
{
	pdc_reset();

	return 0;
}

#if defined(CONFIG_BATTERY_SAMSUNG)
int pdc_get_setting(void)
{
	int ret = 0;
	int idx, selected_idx;
	int ibus = 0, vbus;
	struct pd_cap *cap = NULL;
	unsigned int mivr1 = 0;
	bool chg1_mivr = false;
	int i;
#if defined(CONFIG_BATTERY_SAMSUNG) && defined(CONFIG_PDIC_NOTIFIER)
	bool do_power_nego = false;
	u8 temp = 0x00;
	PD_NOTI_TYPEDEF pdic_noti;
#endif
	pdc_init_table();
	pdc_get_reset_idx();
	pdc_get_cap_max_watt();

	cap = &pd->cap;

	if (cap->nr == 0 || !pdc_is_ready() || !mt_charger_plugin())
		return -1;

	ret = charger_get_ibus(&ibus);
	if (ret < 0) {
		chr_err("[%s] get ibus fail, keep default voltage\n", __func__);
		return -1;
	}
	charger_get_mivr_state(&chg1_mivr);
	charger_get_mivr(&mivr1);

	vbus = battery_get_vbus();
	ibus = ibus / 1000;

	if ((chg1_mivr && (vbus < mivr1 / 1000 - 500))) {
		chr_err("[%s] vbus:%d ibus:%d, mivr:%d\n",
				__func__, vbus, ibus, chg1_mivr);
#if !defined(CONFIG_SEC_FACTORY)
		goto reset;
#endif
	}
	selected_idx = cap->selected_cap_idx;
	idx = selected_idx;

	if (idx < 0 || idx >= ADAPTER_CAP_MAX_NR)
		idx = selected_idx = 0;

	pd->data.fpdo_num = 0;
	pd->data.apdo_num = 0;
	pd->data.unknown_num = 0;
	for (i = 1; i <= cap->nr; i++) {
		if (cap->type[i] == MTK_PD_APDO)
			pd->data.apdo_num++;
		else if (cap->type[i] == MTK_PD)
			pd->data.fpdo_num++;
		else
			pd->data.unknown_num++;
	}
	if (cap->nr <= 0) {
		pr_info("%s : PDO list is empty!!\n", __func__);
		return 0;
	} else {
		pr_info("%s: total num_pd_list: %d, num_fpdo: %d, num_apdo: %d\n",
			__func__, cap->nr, pd->data.fpdo_num, pd->data.apdo_num);
	}
#if defined(CONFIG_BATTERY_SAMSUNG) && defined(CONFIG_PDIC_NOTIFIER)
	temp = pd_noti.sink_status.available_pdo_num = cap->nr;

	pd_noti.sink_status.has_apdo = false;

	for (i = 0; i < temp; i++) {
		if (!(do_power_nego) &&
			(pd_noti.sink_status.power_list[i + 1].max_current != cap->ma[i] ||
			pd_noti.sink_status.power_list[i + 1].max_voltage != cap->max_mv[i]))
			do_power_nego = true;

		pd_noti.sink_status.power_list[i + 1].max_current = cap->ma[i];
		pd_noti.sink_status.power_list[i + 1].max_voltage = cap->max_mv[i];
		pd_noti.sink_status.power_list[i + 1].min_voltage = cap->min_mv[i];
		pd_noti.sink_status.power_list[i + 1].comm_capable = adapter_is_src_usb_communication_capable();
		pd_noti.sink_status.power_list[i + 1].suspend = adapter_is_src_usb_suspend_support();

		pd_noti.sink_status.power_list[i + 1].accept = true;
		if (cap->type[i] == MTK_PD_APDO) {
			pd_noti.sink_status.power_list[i + 1].apdo = true;
			pd_noti.sink_status.power_list[i + 1].pdo_type = APDO_TYPE;
			pd_noti.sink_status.has_apdo = true;
		} else if (cap->type[i] == MTK_PD) {
			pd_noti.sink_status.power_list[i + 1].apdo = false;
			pd_noti.sink_status.power_list[i + 1].pdo_type = FPDO_TYPE;
			pd_noti.sink_status.has_apdo = false;
		}
		pr_info("%s : PDO_Num[%d,%s,%s] MAX_CURR(%d) MAX_VOLT(%d), AVAILABLE_PDO_Num(%d), comm(%d), suspend(%d)\n", __func__,
				i, pd_noti.sink_status.power_list[i + 1].apdo ? "APDO" : "FIXED", pd_noti.sink_status.power_list[i + 1].accept ? "O" : "X", 
				pd_noti.sink_status.power_list[i + 1].max_current,
				pd_noti.sink_status.power_list[i + 1].max_voltage,
				pd_noti.sink_status.available_pdo_num,
				pd_noti.sink_status.power_list[i + 1].comm_capable,
				pd_noti.sink_status.power_list[i + 1].suspend);
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

	if ((pd->data.ps_rdy == 1 &&
		pd->data.prev_available_pdo !=
			pd_noti.sink_status.available_pdo_num) ||
			(pd->data.was_hard_rst)) {
		pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK_CAP;
		if (pd->data.was_hard_rst)
			pd->data.was_hard_rst = 0;
	} else
		pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK;
	pd->data.ps_rdy = 1;
	pd->data.prev_available_pdo = pd_noti.sink_status.available_pdo_num;

	pr_info("%s : pd_noti.event = %d\n", __func__, pd_noti.event);
	pdic_noti.sub1 = 0;
	pdic_noti.sub2 = 0;
	pdic_noti.sub3 = 0;
	pdic_notifier_notify((PD_NOTI_TYPEDEF *)&pdic_noti, &pd_noti, 0);
#endif
	chr_err("[%s] vbus:%d ibus:%d, mivr:%d\n",
		__func__, vbus, ibus, chg1_mivr);
	chr_err("[%s]vbus:%d:%d\n", __func__, pd->vbus_h, pd->vbus_l);

	return 0;
#if !defined(CONFIG_SEC_FACTORY)
reset:
	pdc_reset();

	return 0;
#endif
}
#else
int pdc_get_setting(int *newvbus, int *newcur,
			int *newidx)
{
	int ret = 0;
	int idx, selected_idx;
	unsigned int pd_max_watt, pd_min_watt, now_max_watt;
	int ibus = 0, vbus;
	bool boost = false, buck = false;
	struct pd_cap *cap = NULL;
	unsigned int mivr1 = 0;
	bool chg1_mivr = false;

	pdc_init_table();
	pdc_get_reset_idx();
	pdc_get_cap_max_watt();

	cap = &pd->cap;

	if (cap->nr == 0)
		return -1;

	ret = charger_get_ibus(&ibus);
	if (ret < 0) {
		chr_err("[%s] get ibus fail, keep default voltage\n", __func__);
		return -1;
	}

	charger_get_mivr_state(&chg1_mivr);
	charger_get_mivr(&mivr1);

	vbus = battery_get_vbus();
	ibus = ibus / 1000;

	if ((chg1_mivr && (vbus < mivr1 / 1000 - 500)))
		goto reset;

	selected_idx = cap->selected_cap_idx;
	idx = selected_idx;

	if (idx < 0 || idx >= ADAPTER_CAP_MAX_NR)
		idx = selected_idx = 0;

	pd_max_watt = cap->max_mv[idx] * (cap->ma[idx]
			/ 100 * (100 - pd->data.ibus_err) - 100);
	now_max_watt = cap->max_mv[idx] * ibus;
	pd_min_watt = cap->max_mv[pd->pd_buck_idx] * cap->ma[pd->pd_buck_idx]
			/ 100 * (100 - pd->data.ibus_err)
			- pd->data.vsys_watt;

	if (pd_min_watt <= 5000000)
		pd_min_watt = 5000000;

	if ((now_max_watt >= pd_max_watt) || chg1_mivr) {
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

	chr_err("[%s]watt:%d,%d,%d up:%d,%d vbus:%d ibus:%d, mivr:%d\n",
		__func__,
		pd_max_watt, now_max_watt, pd_min_watt,
		boost, buck,
		vbus, ibus, chg1_mivr);

	chr_err("[%s]vbus:%d:%d:%d current:%d idx:%d default_idx:%d\n",
		__func__, pd->vbus_h, pd->vbus_l, *newvbus,
		*newcur, *newidx, selected_idx);

	return 0;

reset:
	pdc_reset();
	*newidx = pd->pd_reset_idx;
	*newvbus = cap->max_mv[*newidx];
	*newcur = cap->ma[*newidx];

	return 0;
}
#endif

int pdc_check_leave(void)
{
	struct pd_cap *cap;
	int ibus = 0, vbus = 0;
#if defined(CONFIG_BATTERY_SAMSUNG)
	int  input_current = 0;
#endif
	unsigned int mivr1 = 0;
	bool mivr_state = false;
	int max_mv = 0;

	cap = &pd->cap;
#if defined(CONFIG_BATTERY_SAMSUNG) && defined(CONFIG_PDIC_NOTIFIER)
	if (pd_noti.sink_status.selected_pdo_num != 0)
		max_mv = cap->max_mv[pd_noti.sink_status.selected_pdo_num - 1];
	else
		max_mv = cap->max_mv[0];
#else
	max_mv = cap->max_mv[pd->pd_idx];
#endif

#if defined(CONFIG_BATTERY_SAMSUNG)
	charger_get_input_current(&input_current);
#endif
	charger_get_ibus(&ibus);
	ibus = ibus / 1000;
	vbus = battery_get_vbus();
	charger_get_mivr_state(&mivr_state);
	charger_get_mivr(&mivr1);

	chr_err("[%s]mv:%d, vbus:%d, ibus:%d, idx:%d, min_watt:%d, mivr:%d, mivr_state:%d\n",
		__func__, max_mv, vbus, ibus, pd->pd_idx,
		PD_MIN_WATT, mivr1 / 1000, mivr_state);

#if defined(CONFIG_BATTERY_SAMSUNG)
	if (vbus == 0)
		goto leave;
#else
	if (max_mv * ibus <= PD_MIN_WATT) {
		if (mivr_state)
			chr_err("[%s] MIVR occurred, ibus can't draw much higher current",
				__func__);
		goto leave;
	}
#endif

	return 0;

leave:
	pdc_stop();
	return 2;
}

int pdc_init(void)
{
	struct pdc *pdc = NULL;

	if (pd == NULL) {
		pdc = kzalloc(sizeof(struct pdc), GFP_KERNEL);
		if (pdc == NULL)
			return -ENOMEM;

		pd = pdc;

		pd->data.input_current_limit = 3000000;
		pd->data.charging_current_limit = 3000000;
		pd->data.battery_cv = 4350000;

		pd->data.min_charger_voltage = 4600000;
		pd->data.pd_vbus_low_bound = 5000000;
		pd->data.pd_vbus_upper_bound = 5000000;
		pd->data.ibus_err = 14;
		pd->data.vsys_watt = 5000000;
#if defined(CONFIG_BATTERY_SAMSUNG)
		pd->data.fpdo_num = 0;
		pd->data.apdo_num = 0;
		pd->data.unknown_num = 0;
		pd->data.ps_rdy = 0;
		pd->data.prev_available_pdo = -1;
#endif

		pd->pdc_input_current_limit_setting = -1;
		pd->pdc_max_watt_setting = -1;
		pd->pd_cap_max_watt = -1;
		pd->pd_idx = -1;
		pd->pd_reset_idx = -1;
		pd->pd_boost_idx = 0;
		pd->pd_buck_idx = 0;
		pd->vbus_l = 5000;
		pd->vbus_h = 5000;
#ifdef CONFIG_BATTERY_SAMSUNG
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
		pd_noti.sink_status.current_pdo_num = 0;
		pd_noti.sink_status.selected_pdo_num = 0;
#if defined(CONFIG_BATTERY_NOTIFIER)
		fp_select_pdo = pdc_select_pdo;
		fp_sec_pd_select_pps = pdc_select_pps;
		fp_sec_pd_get_apdo_max_power = pdc_get_apdo_max_power;
#else
		pd_noti.sink_status.fp_sec_pd_select_pdo = pdc_select_pdo;
		pd_noti.sink_status.fp_sec_pd_select_pps = pdc_select_pps;
#endif
#endif
		pd->tcpc = tcpc_dev_get_by_name("type_c_port0");
		if (!pd->tcpc) {
			chr_err("%s get tcpc dev fail\n", __func__);
			return -ENODEV;
		}

#endif
		return 0;
	}

	return 1;
}

struct pdc_data *pdc_get_data(void)
{
	return &pd->data;
}

int pdc_set_data(struct pdc_data data)
{
	pd->data.input_current_limit = data.input_current_limit;
	pd->data.charging_current_limit = data.charging_current_limit;
	pd->data.battery_cv = data.battery_cv;
	pd->data.min_charger_voltage = data.min_charger_voltage;
	pd->data.pd_vbus_low_bound = data.pd_vbus_low_bound;
	pd->data.pd_vbus_upper_bound = data.pd_vbus_upper_bound;
	pd->data.ibus_err = data.ibus_err;
	pd->data.vsys_watt = data.vsys_watt;

	chr_err("[%s]%d %d %d %d %d %d %d %d\n", __func__,
		pd->data.input_current_limit,
		pd->data.charging_current_limit,
		pd->data.battery_cv,
		pd->data.min_charger_voltage,
		pd->data.pd_vbus_low_bound,
		pd->data.pd_vbus_upper_bound,
		pd->data.ibus_err,
		pd->data.vsys_watt);

	pd->vbus_l = pd->data.pd_vbus_low_bound / 1000;
	pd->vbus_h = pd->data.pd_vbus_upper_bound / 1000;

	return 0;
}

#if defined(CONFIG_BATTERY_SAMSUNG)
int pdc_run(void)
{
	int ret = 0;

	pd->vbus_l = pd->data.pd_vbus_low_bound / 1000;
	pd->vbus_h = pd->data.pd_vbus_upper_bound / 1000;
	ret = pdc_get_setting();
	ret = pdc_check_leave();

	if (ret == 2)
		pdc_clear();

	chr_err("[%s] ret:%d\n", __func__, ret);

	return ret;
}
#else
int pdc_set_current(void)
{
	if (pd->pdc_input_current_limit_setting != -1 &&
	    pd->pdc_input_current_limit_setting <
	    pd->data.input_current_limit)
		pd->data.input_current_limit =
			pd->pdc_input_current_limit_setting;

	charger_set_input_current(pd->data.input_current_limit);
	charger_set_charging_current(pd->data.charging_current_limit);

	return 0;
}

int pdc_set_cv(void)
{
	charger_set_constant_voltage(pd->data.battery_cv);

	return 0;
}

int pdc_run(void)
{
	int ret = 0;
	int vbus = 0, cur = 0, idx = 0;

	pd->vbus_l = pd->data.pd_vbus_low_bound / 1000;
	pd->vbus_h = pd->data.pd_vbus_upper_bound / 1000;

	pdc_set_cv();

	ret = pdc_get_setting(&vbus, &cur, &idx);

	if (ret != -1 && idx != -1) {
		pd->pdc_input_current_limit_setting =  cur * 1000;
		pdc_set_current();
		pdc_setup(idx);
	}

	ret = pdc_check_leave();

	chr_err("[%s]vbus:%d input_cur:%d idx:%d current:%d ret:%d\n",
			__func__, vbus, cur, idx,
			pd->data.input_current_limit, ret);

	return ret;
}
#endif
