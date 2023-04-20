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
#include <linux/usb/typec.h>
#include <linux/usb/typec/etek/et7303/tcpci_core.h>
#include <linux/usb/typec/etek/et7303/tcpm.h>
#if IS_ENABLED(CONFIG_PDIC_POLICY)
#include <linux/usb/typec/common/pdic_policy.h>
#endif /* CONFIG_PDIC_POLICY */
#if defined(CONFIG_USB_HW_PARAM)
#include <linux/usb_notify.h>
#endif /* CONFIG_USB_HW_PARAM */

static int tcpc_typec_class_dr_set(void *data, enum typec_data_role val)
{
	struct tcpc_device *tcpc = data;
	int ret = 0;
	uint8_t role;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();

	if (o_notify)
		inc_hw_param(o_notify, USB_CCIC_DR_SWAP_COUNT);
#endif /* CONFIG_USB_HW_PARAM */

	if (!tcpc)
		return -EEXIST;

	switch (val) {
	case TYPEC_HOST:
		role = PD_ROLE_DFP;
		break;
	case TYPEC_DEVICE:
		role = PD_ROLE_UFP;
		break;
	default:
		return 0;
	}

	ret = tcpm_dpm_pd_data_swap(tcpc, role, NULL);
	pr_info("%s data role swap (%s): %d\n", __func__,
			(val == TYPEC_HOST ? "DFP" : "UFP"), ret);

	return ret;
}

static int tcpc_typec_class_pr_set(void *data, enum typec_role val)
{
	struct tcpc_device *tcpc = data;
	int ret = 0;
	uint8_t role;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();

	if (o_notify)
		inc_hw_param(o_notify, USB_CCIC_PR_SWAP_COUNT);
#endif /* CONFIG_USB_HW_PARAM */

	if (!tcpc)
		return -EEXIST;

	switch (val) {
	case TYPEC_SOURCE:
		role = PD_ROLE_SOURCE;
		break;
	case TYPEC_SINK:
		role = PD_ROLE_SINK;
		break;
	default:
		return 0;
	}

	ret = tcpm_dpm_pd_power_swap(tcpc, role, NULL);
	pr_info("%s power role swap (->%d): %d\n",
		__func__, val, ret);

	if (ret == TCPM_ERROR_NO_PD_CONNECTED) {
		ret = tcpm_typec_role_swap(tcpc);
		pr_info("%s typec role swap (%s): %d\n", __func__,
				(val == TYPEC_SOURCE ? "SRC" : "SNK"), ret);
	}

	return ret;
}

static int tcpc_typec_class_vconn_set(void *data, enum typec_role val)
{
	struct tcpc_device *tcpc = data;
	int ret;
	uint8_t role;

	if (!tcpc)
		return -EEXIST;

	switch (val) {
	case TYPEC_SINK:
		role = PD_ROLE_VCONN_OFF;
		break;
	case TYPEC_SOURCE:
		role = PD_ROLE_VCONN_ON;
		break;
	default:
		return 0;
	}

	ret = tcpm_dpm_pd_vconn_swap(tcpc, role, NULL);
	pr_info("%s vconn swap (%s): %d\n", __func__,
			(val == TYPEC_SINK ? "OFF" : "ON"), ret);

	return ret;
}

static int tcpc_typec_class_port_type_set(void *data,
		enum typec_port_type port_type)
{
	struct tcpc_device *tcpc = data;
	int ret = 0;

	if (!tcpc)
		return -EEXIST;

	if (port_type == TYPEC_PORT_DRP) {
		pr_info("%s: do not swap at DRP\n", __func__);
		return 0;
	}

	ret = tcpm_typec_role_swap(tcpc);
	if (ret) {
		pr_err("%s: %d\n", __func__, ret);
		if (ret == TCPM_ERROR_UNATTACHED)
			ret = 0;
	}

	return ret;
}

static const struct pdic_ops tcpc_ops = {
	.dr_set = tcpc_typec_class_dr_set,
	.pr_set = tcpc_typec_class_pr_set,
	.vconn_set = tcpc_typec_class_vconn_set,
	.port_type_set = tcpc_typec_class_port_type_set,
};

int tcpc_typec_class_init(struct tcpc_device *tcpc)
{
	if (!tcpc)
		return -EEXIST;

	tcpc->ic_data.p_ops = &tcpc_ops;

	return 0;
}
