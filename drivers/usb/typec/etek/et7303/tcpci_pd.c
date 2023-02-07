/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * TCPC Interface for dual role
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/usb/typec/etek/et7303/pd_dpm_pdo_select.h>
#include <linux/usb/typec/etek/et7303/tcpm.h>
#include <linux/usb/typec/etek/et7303/tcpm_block.h>
#if IS_ENABLED(CONFIG_PDIC_POLICY)
#include <linux/usb/typec/common/pdic_policy.h>
#endif /* CONFIG_PDIC_POLICY */

struct tcpc_device *g_tcpc_dev;

static void tcpc_pd_select_pdo(int num)
{
	struct tcpc_device *tcpc = g_tcpc_dev;
	struct pdic_notifier_struct *pd_noti = NULL;	
	struct tcpm_power_cap_val cap;
	struct tcpm_power_cap caps;
	int i = 0, pos = num - 1;

	if (!tcpc) {
		pr_err("%s: tcpc is NULL\n", __func__);
		return;
	}

	pd_noti = &tcpc->pd_noti;
	
	if (pd_noti->sink_status.selected_pdo_num == num) {
#if IS_ENABLED(CONFIG_PDIC_POLICY)
		if (tcpc->pd_port.pe_ready) {
			pd_noti->event = PDIC_NOTIFY_EVENT_PD_SINK;
			pdic_policy_send_msg(tcpc->pp_data,
				MSG_PD_POWER_STATUS, 1, 0);
		}
#endif /* CONFIG_PDIC_POLICY */
		return;
	} else if (num > pd_noti->sink_status.available_pdo_num) {
		pd_noti->sink_status.selected_pdo_num =
			pd_noti->sink_status.available_pdo_num;
	} else if (num < 1) {
		pd_noti->sink_status.selected_pdo_num = 1;
	} else {
		pd_noti->sink_status.selected_pdo_num = num;
	}

	pr_info("%s : PDO(%d) is selected to change\n",
		__func__, pd_noti->sink_status.selected_pdo_num);

	if (tcpm_inquire_pd_source_cap(tcpc, &caps)) {
		pr_err("%s: tcpm_inquire_pd_source_cap fail\n", __func__);
		return;
	}

	for (i = 0; i < caps.cnt; i++) {
		tcpm_extract_power_cap_val(caps.pdos[i], &cap);
		pr_debug("%s: %d %d %d %d\n", __func__, cap.type, cap.min_mv, cap.max_mv, cap.ma);
	}

	if (pos >= caps.cnt) {
		pr_err("%s: get index large than fail\n", __func__);
		return;
	}

	if (!tcpm_extract_power_cap_val(caps.pdos[pos], &cap)) {
		pr_err("%s: tcpm_extract_power_cap_val fail\n", __func__);
		return;
	}

	if (cap.type != 0) {
		pr_err("%s: donot support this type\n", __func__);
		return;
	}

	if (tcpm_dpm_pd_request_bk(tcpc, cap.max_mv, cap.ma)) {
		pr_err("%s: Can't set vbus request\n", __func__);
		return;
	};
}

void tcpc_pd_update_src_cap(struct tcpc_device *tcpc, struct pd_port_power_capabilities *src_cap)
{
	struct dpm_pdo_info_t source;
	struct pdic_notifier_struct *pd_noti = &tcpc->pd_noti;
	int i, available_pdo_num = 0;
	POWER_LIST* pPower_list;	

	for (i = 0; i < src_cap->nr; i++) {
		DPM_INFO("SrcCap%d: 0x%08x\r\n", i + 1, src_cap->pdos[i]);
		dpm_extract_pdo_info(src_cap->pdos[i], &source);

		pPower_list = &pd_noti->sink_status.power_list[available_pdo_num + 1];

		switch (source.type) {
		case DPM_PDO_TYPE_FIXED:		
			pPower_list->apdo = false;
			pPower_list->pdo_type = FPDO_TYPE;
			pPower_list->max_voltage = source.vmax;
			pPower_list->min_voltage = source.vmin;
			pPower_list->max_current = source.ma;
			if (pPower_list->max_voltage > AVAILABLE_VOLTAGE)
				pPower_list->accept = false;
			else
				pPower_list->accept = true;
			available_pdo_num++;
			pr_info("%s: [%d] FIXED max_voltage(%d)mV, max_current(%d)\n",
				__func__, i+1, source.vmax, source.ma);
			break;
		case DPM_PDO_TYPE_VAR:			
			pr_info("%s: [%d] VARIABLE min_volt(%d)mV, max_volt(%d)mV, max_current(%d)mA\n",
				__func__, i+1, source.vmin, source.vmax, source.ma);
			break;
		case DPM_PDO_TYPE_BAT:
			pr_info("%s: [%d] BATTERY min_volt(%d)mV, max_volt(%d)mV, max_power(%d)mW\n",
				__func__, i+1, source.vmin, source.vmax, source.uw / 1000);
			break;
#ifdef CONFIG_USB_PD_REV30_PPS_SINK
		case DPM_PDO_TYPE_APDO:
#endif
		default:
			break;
		};		
	}

	pd_noti->sink_status.available_pdo_num = available_pdo_num;
}

int tcpc_pd_init(struct tcpc_device *tcpc)
{
	if (!tcpc)
		return -EEXIST;

	g_tcpc_dev = tcpc;
	tcpc->ic_data.pd_noti = &tcpc->pd_noti;
	tcpc->pd_noti.sink_status.available_pdo_num = 0;
	tcpc->pd_noti.sink_status.selected_pdo_num = 0;
	tcpc->pd_noti.sink_status.current_pdo_num = 0;
	tcpc->pd_noti.sink_status.pps_voltage = 0;
	tcpc->pd_noti.sink_status.pps_current = 0;
	tcpc->pd_noti.sink_status.has_apdo = false;
	tcpc->pd_noti.sink_status.fp_sec_pd_select_pdo = tcpc_pd_select_pdo;

	return 0;
}
