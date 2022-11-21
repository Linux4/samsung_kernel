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
#include "inc/tcpci.h"
#include "inc/tcpci_typec.h"
#include <linux/usb/typec.h>
#if defined(CONFIG_USB_HW_PARAM)
#include <linux/usb_notify.h>
#endif /* CONFIG_USB_HW_PARAM */

static int tcpc_typec_class_dr_set(const struct typec_capability *cap, enum typec_data_role val)
{
	struct  tcpc_device *tcpc = container_of(cap, struct tcpc_device, typec_cap);
	int ret = 0;
	uint8_t role;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();

	if (o_notify)
		inc_hw_param(o_notify, USB_CCIC_DR_SWAP_COUNT);
#endif /* CONFIG_USB_HW_PARAM */

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

	if (val == tcpc->typec_data_role) {
		pr_info("%s wrong role (%d->%d)\n",
			__func__, tcpc->typec_data_role, val);
		return 0;
	}

	ret = tcpm_dpm_pd_data_swap(tcpc, role, NULL);
	pr_info("%s data role swap (%d->%d): %d\n",
		__func__, tcpc->typec_data_role, val, ret);

	return ret;
}

static int tcpc_typec_class_pr_set(const struct typec_capability *cap, enum typec_role val)
{
	struct  tcpc_device *tcpc = container_of(cap, struct tcpc_device, typec_cap);
	int ret = 0;
	uint8_t role;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();

	if (o_notify)
		inc_hw_param(o_notify, USB_CCIC_PR_SWAP_COUNT);
#endif /* CONFIG_USB_HW_PARAM */

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

	if (val == tcpc->typec_power_role) {
		pr_info("%s wrong role (%d->%d)\n",
			__func__, tcpc->typec_power_role, val);
		return 0;
	}

	ret = tcpm_dpm_pd_power_swap(tcpc, role, NULL);
	pr_info("%s power role swap (%d->%d): %d\n",
		__func__, tcpc->typec_power_role, val, ret);

	if (ret == TCPM_ERROR_NO_PD_CONNECTED) {
		ret = tcpm_typec_role_swap(tcpc);
		pr_info("%s typec role swap (%d->%d): %d\n",
			__func__, tcpc->typec_power_role, val, ret);
	}

	return ret;
}

static int tcpc_typec_class_port_type_set(const struct typec_capability *cap, enum typec_port_type port_type)
{
	struct  tcpc_device *tcpc = container_of(cap, struct tcpc_device, typec_cap);
	int ret = 0;

	ret = tcpm_typec_role_swap(tcpc);
	if (ret) {
		pr_err("%s: %d\n", __func__, ret);
		if (ret == TCPM_ERROR_UNATTACHED)
			ret = 0;
	}

	return ret;
}

int tcpc_typec_class_init(struct tcpc_device *tcpc)
{
	tcpc->typec_cap.revision = USB_TYPEC_REV_1_2;
	tcpc->typec_cap.pd_revision = 0x300;
	tcpc->typec_cap.prefer_role = TYPEC_NO_PREFERRED_ROLE;
	tcpc->typec_cap.pr_set = tcpc_typec_class_pr_set;
	tcpc->typec_cap.dr_set = tcpc_typec_class_dr_set;
	tcpc->typec_cap.port_type_set = tcpc_typec_class_port_type_set;
	tcpc->typec_cap.type = TYPEC_PORT_DRP;

	tcpc->typec_power_role = TYPEC_SINK;
	tcpc->typec_data_role = TYPEC_DEVICE;
	tcpc->pwr_opmode = TYPEC_PWR_MODE_USB;

	tcpc->port = typec_register_port(&tcpc->dev, &tcpc->typec_cap);
	if (IS_ERR(tcpc->port)) {
		dev_err(&tcpc->dev, "tcpc fail to register typec usb\n");
		return -EINVAL;
	}

	return 0;
}
