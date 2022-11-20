/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * Richtek TypeC Port Control Interface Core Driver
 *
 * Author: TH <tsunghan_tsai@richtek.com>
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
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include <linux/usb/typec/etek/et7303/tcpci.h>
#include <linux/usb/typec/etek/et7303/tcpci_typec.h>
#include <linux/usb/typec/etek/et7303/tcpm_block.h>

#ifdef CONFIG_USB_POWER_DELIVERY
#include "pd_dpm_prv.h"
#include <linux/usb/typec/etek/et7303/tcpm.h>
#include <linux/usb/typec/etek/et7303/pd_dpm_core.h>
#endif /* CONFIG_USB_POWER_DELIVERY */

#if defined(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_core.h>
#include <linux/usb/typec/common/pdic_sysfs.h>
#include <linux/usb/typec/common/pdic_notifier.h>

static enum pdic_sysfs_property tcpc_sysfs_properties[] = {
	PDIC_SYSFS_PROP_CHIP_NAME,
	PDIC_SYSFS_PROP_CC_PIN_STATUS,
	PDIC_SYSFS_PROP_NOVBUS_RP22K,
};
#endif /* CONFIG_PDIC_NOTIFIER */

#define TCPC_CORE_VERSION		"1.2.1_G"

static ssize_t tcpc_show_property(struct device *dev,
				  struct device_attribute *attr, char *buf);
static ssize_t tcpc_store_property(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count);

#define TCPC_DEVICE_ATTR(_name, _mode)					\
{									\
	.attr = { .name = #_name, .mode = _mode },			\
	.show = tcpc_show_property,					\
	.store = tcpc_store_property,					\
}

static struct class *tcpc_class;
EXPORT_SYMBOL_GPL(tcpc_class);

static struct device_type tcpc_dev_type;

static struct device_attribute tcpc_device_attributes[] = {
	TCPC_DEVICE_ATTR(role_def, S_IRUGO),
	TCPC_DEVICE_ATTR(rp_lvl, S_IRUGO),
	TCPC_DEVICE_ATTR(pd_test, S_IRUGO | S_IWUSR | S_IWGRP),
	TCPC_DEVICE_ATTR(info, S_IRUGO),
	TCPC_DEVICE_ATTR(timer, S_IRUGO | S_IWUSR | S_IWGRP),
	TCPC_DEVICE_ATTR(caps_info, S_IRUGO),
	TCPC_DEVICE_ATTR(pdo_sel, S_IRUGO | S_IWUSR | S_IWGRP),
	TCPC_DEVICE_ATTR(pdos_sel, S_IRUGO | S_IWUSR | S_IWGRP),
};

enum {
	TCPC_DESC_ROLE_DEF = 0,
	TCPC_DESC_RP_LEVEL,
	TCPC_DESC_PD_TEST,
	TCPC_DESC_INFO,
	TCPC_DESC_TIMER,
	TCPC_DESC_CAP_INFO,
	TCPC_DESC_PDO_SEL,
	TCPC_DESC_PDOS_SEL,
};

static struct attribute *__tcpc_attrs[ARRAY_SIZE(tcpc_device_attributes) + 1];
static struct attribute_group tcpc_attr_group = {
	.attrs = __tcpc_attrs,
};

static const struct attribute_group *tcpc_attr_groups[] = {
	&tcpc_attr_group,
	NULL,
};

static const char * const role_text[] = {
	"SNK Only",
	"SRC Only",
	"DRP",
	"Try.SRC",
	"Try.SNK",
};

static ssize_t tcpc_show_property(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct tcpc_device *tcpc = to_tcpc_device(dev);
	const ptrdiff_t offset = attr - tcpc_device_attributes;
	int i, flag;
#ifdef CONFIG_USB_POWER_DELIVERY
	struct tcpm_power_cap_val cap;
	struct tcpm_power_cap caps;
#endif	/* CONFIG_USB_POWER_DELIVERY */

	switch (offset) {
#ifdef CONFIG_USB_POWER_DELIVERY
	case TCPC_DESC_PDOS_SEL:
		if (tcpm_inquire_pd_contract(tcpc, &cap.max_mv, &cap.ma))
			return -ENODEV;

		snprintf(buf+strlen(buf), 256, "%s = %d (vol = %dmV, cur %dmA)\n",
			"remote_selected_cap",
			tcpc->pd_port.remote_selected_cap,
			cap.max_mv, cap.ma);
		break;
	case TCPC_DESC_PDO_SEL:
		if (tcpm_inquire_pd_contract(tcpc, &cap.max_mv, &cap.ma))
			return -ENODEV;

		snprintf(buf+strlen(buf), 256, "%s = %d (vol = %dmV, cur %dmA)\n",
			"remote_selected_cap",
			tcpc->pd_port.remote_selected_cap,
			cap.max_mv, cap.ma);
		break;
	case TCPC_DESC_CAP_INFO:
		flag = tcpm_inquire_pd_connected(tcpc);

		snprintf(buf+strlen(buf), 256, "%s = %d\n%s = %d\n",
			"local_selected_cap",
			flag ? tcpc->pd_port.local_selected_cap : 0,
			"remote_selected_cap",
			flag ? tcpc->pd_port.remote_selected_cap : 0);

		snprintf(buf+strlen(buf), 256, "%s\n",
				"local_src_cap(type, vmin, vmax, oper)");
		for (i = 0; i < tcpc->pd_port.local_src_cap.nr; i++) {
			tcpm_extract_power_cap_val(
				tcpc->pd_port.local_src_cap.pdos[i],
				&cap);
			snprintf(buf+strlen(buf), 256, "%d %d %d %d\n",
				cap.type, cap.min_mv, cap.max_mv, cap.ma);
		}

		snprintf(buf+strlen(buf), 256, "%s\n",
				"local_snk_cap(type, vmin, vmax, ioper)");
		for (i = 0; i < tcpc->pd_port.local_snk_cap.nr; i++) {
			tcpm_extract_power_cap_val(
				tcpc->pd_port.local_snk_cap.pdos[i],
				&cap);
			snprintf(buf+strlen(buf), 256, "%d %d %d %d\n",
				cap.type, cap.min_mv, cap.max_mv, cap.ma);
		}
		snprintf(buf+strlen(buf), 256, "%s\n",
				"remote_src_cap(type, vmin, vmax, ioper)");
		if (!tcpm_inquire_pd_source_cap(tcpc, &caps)) {
			for (i = 0; i < caps.cnt; i++) {
				tcpm_extract_power_cap_val(caps.pdos[i], &cap);
				snprintf(buf+strlen(buf), 256, "%d %d %d %d\n",
					cap.type, cap.min_mv, cap.max_mv, cap.ma);
			}
		}

		snprintf(buf+strlen(buf), 256, "%s\n",
				"remote_snk_cap(type, vmin, vmax, ioper)");
		if (!tcpm_inquire_pd_sink_cap(tcpc, &caps)) {
			for (i = 0; i < caps.cnt; i++) {
				tcpm_extract_power_cap_val(caps.pdos[i], &cap);
				snprintf(buf+strlen(buf), 256, "%d %d %d %d\n",
					cap.type, cap.min_mv, cap.max_mv, cap.ma);
			}
		}
		break;
#endif	/* CONFIG_USB_POWER_DELIVERY */
	case TCPC_DESC_ROLE_DEF:
		snprintf(buf, 256, "%s\n", role_text[tcpc->desc.role_def]);
		break;
	case TCPC_DESC_RP_LEVEL:
		if (tcpc->typec_local_rp_level == TYPEC_CC_RP_DFT)
			snprintf(buf, 256, "%s\n", "Default");
		else if (tcpc->typec_local_rp_level == TYPEC_CC_RP_1_5)
			snprintf(buf, 256, "%s\n", "1.5");
		else if (tcpc->typec_local_rp_level == TYPEC_CC_RP_3_0)
			snprintf(buf, 256, "%s\n", "3.0");
		break;
	case TCPC_DESC_PD_TEST:
		snprintf(buf, 256, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
			"1: pr_swap", "2: dr_swap", "3: vconn_swap",
			"4: soft reset", "5: hard reset",
			"6: get_src_cap", "7: get_sink_cap",
			"8: discover_id", "9: discover_cable");
		break;
	case TCPC_DESC_INFO:
		i += snprintf(buf + i,
			256, "|^|==( %s info )==|^|\n", tcpc->desc.name);
		i += snprintf(buf + i,
			256, "role = %s\n", role_text[tcpc->desc.role_def]);
		if (tcpc->typec_local_rp_level == TYPEC_CC_RP_DFT)
			i += snprintf(buf + i, 256, "rplvl = %s\n", "Default");
		else if (tcpc->typec_local_rp_level == TYPEC_CC_RP_1_5)
			i += snprintf(buf + i, 256, "rplvl = %s\n", "1.5");
		else if (tcpc->typec_local_rp_level == TYPEC_CC_RP_3_0)
			i += snprintf(buf + i, 256, "rplvl = %s\n", "3.0");
		break;
	default:
		break;
	}
	return strlen(buf);
}

static int get_parameters(char *buf, long int *param1, int num_of_par)
{
	char *token;
	int base, cnt;

	token = strsep(&buf, " ");

	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token != NULL) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;

			if (kstrtoul(token, base, &param1[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
			}
		else
			return -EINVAL;
	}
	return 0;
}

static ssize_t tcpc_store_property(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
#ifdef CONFIG_USB_POWER_DELIVERY
	uint8_t role;
#endif	/* CONFIG_USB_POWER_DELIVERY */

	struct tcpc_device *tcpc = to_tcpc_device(dev);
	const ptrdiff_t offset = attr - tcpc_device_attributes;
	int ret, i;
	long int val;
	long int tmp[2];
	struct tcpm_power_cap_val cap;
	struct tcpm_power_cap caps;

	switch (offset) {
	case TCPC_DESC_PDOS_SEL:
		ret = get_parameters((char *)buf, tmp, 1);
		if (ret < 0) {
			dev_err(dev, "get parameters fail\n");
			return -EINVAL;
		}
		pr_info("TCPC_DESC_PDOS_SEL\n");
		if (tcpm_inquire_pd_source_cap(tcpc, &caps)) {
			dev_err(dev, "tcpm_inquire_pd_source_cap fail\n");
			return -EINVAL;
		}

		for (i = 0; i < caps.cnt; i++) {
			tcpm_extract_power_cap_val(caps.pdos[i], &cap);
			pr_info("%d %d %d %d\n", cap.type, cap.min_mv, cap.max_mv, cap.ma);
		}
		if (tmp[0] >= caps.cnt) {
			dev_err(dev, "get index large than fail\n");
			return -EINVAL;
		}

		if (!tcpm_extract_power_cap_val(caps.pdos[tmp[0]], &cap)) {
			dev_err(dev, "tcpm_extract_power_cap_val fail\n");
			return -EINVAL;
		}
		if (cap.type != 0) {
			dev_err(dev, "donot support this type\n");
			return -EINVAL;
		}

		if (tcpm_dpm_pd_request_bk(tcpc, cap.max_mv, cap.ma)) {
			dev_err(dev, "Can't set vbus request\n");
			return -EINVAL;
		};
		break;
	case TCPC_DESC_PDO_SEL:
		ret = get_parameters((char *)buf, tmp, 2);
		if (ret < 0) {
			dev_err(dev, "get parameters fail\n");
			return -EINVAL;
		}

		if (tcpm_dpm_pd_request_bk(tcpc, tmp[0], tmp[1])) {
			dev_err(dev, "Can't set vbus request\n");
			return -EINVAL;
		};

		break;
	case TCPC_DESC_ROLE_DEF:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_err(dev, "get parameters fail\n");
			return -EINVAL;
		}

		tcpm_typec_change_role(tcpc, val);
		break;
	case TCPC_DESC_TIMER:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_err(dev, "get parameters fail\n");
			return -EINVAL;
		}
		#ifdef CONFIG_USB_POWER_DELIVERY
		if (val > 0 && val <= PD_PE_TIMER_END_ID)
			pd_enable_timer(&tcpc->pd_port, val);
		else if (val > PD_PE_TIMER_END_ID && val < PD_TIMER_NR)
			tcpc_enable_timer(tcpc, val);
		#else
		if (val > 0 && val < PD_TIMER_NR)
			tcpc_enable_timer(tcpc, val);
		#endif /* CONFIG_USB_POWER_DELIVERY */
		break;
	#ifdef CONFIG_USB_POWER_DELIVERY
	case TCPC_DESC_PD_TEST:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_err(dev, "get parameters fail\n");
			return -EINVAL;
		}
		switch (val) {
		case 1: /* Power Role Swap */
			role = tcpm_inquire_pd_power_role(tcpc);
			if (role == PD_ROLE_SINK)
				role = PD_ROLE_SOURCE;
			else
				role = PD_ROLE_SINK;
			tcpm_dpm_pd_power_swap(tcpc, role, NULL);
			break;
		case 2: /* Data Role Swap */
			role = tcpm_inquire_pd_data_role(tcpc);
			if (role == PD_ROLE_UFP)
				role = PD_ROLE_DFP;
			else
				role = PD_ROLE_UFP;
			tcpm_dpm_pd_data_swap(tcpc, role, NULL);
			break;
		case 3: /* Vconn Swap */
			role = tcpm_inquire_pd_vconn_role(tcpc);
			if (role == PD_ROLE_VCONN_OFF)
				role = PD_ROLE_VCONN_ON;
			else
				role = PD_ROLE_VCONN_OFF;
			tcpm_dpm_pd_vconn_swap(tcpc, role, NULL);
			break;
		case 4: /* Software Reset */
			tcpm_dpm_pd_soft_reset(tcpc, NULL);
			break;
		case 5: /* Hardware Reset */
			tcpm_dpm_pd_hard_reset(tcpc);
			break;
		case 6:
			tcpm_dpm_pd_get_source_cap(tcpc, NULL);
			break;
		case 7:
			tcpm_dpm_pd_get_sink_cap(tcpc, NULL);
			break;
		case 8:
			tcpm_dpm_vdm_discover_id(tcpc, NULL);
			break;
		case 9:
			tcpm_dpm_vdm_discover_cable(tcpc, NULL);
			break;
		default:
			break;
		}
		break;
	#endif /* CONFIG_USB_POWER_DELIVERY */
	default:
		break;
	}
	return count;
}

#if defined(CONFIG_PDIC_NOTIFIER)
static int tcpc_sysfs_get_prop(struct _pdic_data_t *ppdic_data,
					enum pdic_sysfs_property prop,
					char *buf)
{
	struct tcpc_device *tcpc =
			(struct tcpc_device *)ppdic_data->drv_data;
	int retval = -ENODEV;
	int cc1, cc2;
	bool rp22 = false;

	if (!tcpc) {
		printk("%s : chip is null\n", __func__);
		return retval;
	}

	switch (prop) {
	case PDIC_SYSFS_PROP_CC_PIN_STATUS:
		if (tcpc->ops->get_cc(tcpc, &cc1, &cc2) == 0) {
			pr_info("%s : cc1=%d, cc2=%d\n", __func__, cc1, cc2);
			if (!(cc1 == TYPEC_CC_VOLT_OPEN || cc1 == TYPEC_CC_DRP_TOGGLING))
				retval = sprintf(buf, "1\n"); //CC1_ACTIVE
			else if (!(cc2 == TYPEC_CC_VOLT_OPEN || cc2 ==TYPEC_CC_DRP_TOGGLING))
				retval = sprintf(buf, "2\n"); //CC2_ACTVIE
			else
				retval = sprintf(buf, "0\n"); //NO_DETERMINATION
		}
		pr_info("%s : PDIC_SYSFS_PROP_CC_PIN_STATUS : %s\n", __func__, buf);
		break;
	case PDIC_SYSFS_PROP_NOVBUS_RP22K:
		if (tcpc->ops->get_cc(tcpc, &cc1, &cc2) == 0) {
			pr_info("%s : cc1=%d, cc2=%d vbus=%d\n", __func__,
					cc1, cc2, tcpc->vbus_level);

			if (cc1 == TYPEC_CC_VOLT_SNK_1_5 &&
					cc2 == TYPEC_CC_VOLT_OPEN)
				rp22 = true;
			else if (cc2 == TYPEC_CC_VOLT_SNK_1_5 &&
					cc1 == TYPEC_CC_VOLT_OPEN)
				rp22 = true;

			if (rp22 && tcpc->vbus_level == TCPC_VBUS_SAFE0V)
				retval = sprintf(buf, "1\n");
			else
				retval = sprintf(buf, "0\n");
		}
		pr_info("%s : PDIC_SYSFS_PROP_NOVBUS_RP22K : %s\n", __func__,
				buf);
		break;
	default:
		pr_info("%s : prop read not supported prop (%d)\n",
				__func__, prop);
		retval = -ENODATA;
		break;
	}

	return retval;
}

static ssize_t tcpc_sysfs_set_prop(struct _pdic_data_t *ppdic_data,
				enum pdic_sysfs_property prop,
				const char *buf, size_t size)
{
	ssize_t retval = size;
	struct tcpc_device *tcpc =
			(struct tcpc_device *)ppdic_data->drv_data;

	if (!tcpc) {
		printk("%s : chip is null : request prop = %d\n",
				__func__, prop);
		return -ENODEV;
	}

	switch (prop) {
	default:
		printk("%s : prop write not supported prop (%d)\n",
				__func__, prop);
		retval = -ENODATA;
		return retval;
	}
	return size;
}

static int tcpc_sysfs_is_writeable(struct _pdic_data_t *ppdic_data,
				enum pdic_sysfs_property prop)
{
	switch (prop) {
	default:
		return 0;
	}
}
#endif /* CONFIG_PDIC_NOTIFIER */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0))
static int tcpc_match_device_by_name(struct device *dev, const void *data)
#else
static int tcpc_match_device_by_name(struct device *dev, void *data)
#endif
{
	const char *name = data;
	struct tcpc_device *tcpc = dev_get_drvdata(dev);

	return strcmp(tcpc->desc.name, name) == 0;
}

struct tcpc_device *tcpc_dev_get_by_name(const char *name)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0))
	struct device *dev = class_find_device(tcpc_class,
			NULL, (const void *)name, tcpc_match_device_by_name);
#else
	struct device *dev = class_find_device(tcpc_class,
			NULL, (void *)name, tcpc_match_device_by_name);
#endif
	return dev ? dev_get_drvdata(dev) : NULL;
}

static void tcpc_device_release(struct device *dev)
{
	struct tcpc_device *tcpc_dev = to_tcpc_device(dev);

	pr_info("%s : %s device release\n", __func__, dev_name(dev));
	PD_BUG_ON(tcpc_dev == NULL);
	/* Un-init pe thread */
#ifdef CONFIG_USB_POWER_DELIVERY
	tcpci_event_deinit(tcpc_dev);
#endif /* CONFIG_USB_POWER_DELIVERY */
	/* Un-init timer thread */
	tcpci_timer_deinit(tcpc_dev);
	/* Un-init Mutex */
	/* Do initialization */
	devm_kfree(dev, tcpc_dev);
}

#if IS_ENABLED(CONFIG_PDIC_POLICY)
static int tcpc_get_vbus_dischg_gpio(struct tcpc_device *tcpc,
		struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret = 0;

	if (!np)
		return -EINVAL;

	pr_info("%s\n", __func__);
	ret = of_get_named_gpio(np, "tcpc,vbus_discharge", 0);
	if (gpio_is_valid(ret))
		gpio_direction_output(ret, 0);

	return ret;
}
#endif /* CONFIG_PDIC_POLICY */

static void tcpc_init_work(struct work_struct *work);

struct tcpc_device *tcpc_device_register(struct device *parent,
	struct tcpc_desc *tcpc_desc, struct tcpc_ops *ops, void *drv_data)
{
	struct tcpc_device *tcpc;
	int ret = 0;
#if defined(CONFIG_PDIC_NOTIFIER)
	ppdic_data_t ppdic_data;
	ppdic_sysfs_property_t ppdic_sysfs_prop;
#endif /* CONFIG_PDIC_NOTIFIER */

	pr_info("%s register tcpc device (%s)\n", __func__, tcpc_desc->name);
	tcpc = devm_kzalloc(parent, sizeof(*tcpc), GFP_KERNEL);
	if (!tcpc) {
		pr_err("%s : allocate tcpc memeory failed\n", __func__);
		return NULL;
	}

#if defined(CONFIG_PDIC_NOTIFIER)
	get_pdic_device();
	ppdic_data = devm_kzalloc(parent, sizeof(pdic_data_t), GFP_KERNEL);

	if (!ppdic_data)
		return NULL;

	ppdic_sysfs_prop = devm_kzalloc(parent, sizeof(pdic_sysfs_property_t), GFP_KERNEL);

	if (!ppdic_sysfs_prop)
		return NULL;
#endif

	tcpc->dev.class = tcpc_class;
	tcpc->dev.type = &tcpc_dev_type;
	tcpc->dev.parent = parent;
	tcpc->dev.release = tcpc_device_release;
	dev_set_drvdata(&tcpc->dev, tcpc);
	tcpc->drv_data = drv_data;
	dev_set_name(&tcpc->dev, "%s", tcpc_desc->name);
	tcpc->desc = *tcpc_desc;
	tcpc->ops = ops;
	tcpc->typec_local_rp_level = tcpc_desc->rp_lvl;

#ifdef CONFIG_TCPC_VCONN_SUPPLY_MODE
	tcpc->tcpc_vconn_supply = tcpc_desc->vconn_supply;
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */

	ret = device_register(&tcpc->dev);
	if (ret) {
		devm_kfree(parent, tcpc);
		return ERR_PTR(ret);
	}

	srcu_init_notifier_head(&tcpc->evt_nh);
	INIT_DELAYED_WORK(&tcpc->init_work, tcpc_init_work);

	mutex_init(&tcpc->access_lock);
	mutex_init(&tcpc->typec_lock);
	mutex_init(&tcpc->timer_lock);
	sema_init(&tcpc->timer_enable_mask_lock, 1);
	spin_lock_init(&tcpc->timer_tick_lock);

	/* If system support "WAKE_LOCK_IDLE",
	 * please use it instead of "WAKE_LOCK_SUSPEND"
	 */
	wake_lock_init(&tcpc->attach_wake_lock, WAKE_LOCK_SUSPEND,
		"tcpc_attach_wakelock");
	wake_lock_init(&tcpc->dettach_temp_wake_lock, WAKE_LOCK_SUSPEND,
		"tcpc_detach_wakelock");

	tcpci_timer_init(tcpc);
#ifdef CONFIG_USB_POWER_DELIVERY
	tcpci_event_init(tcpc);
	pd_core_init(tcpc);
#endif /* CONFIG_USB_POWER_DELIVERY */

#if IS_ENABLED(CONFIG_PDIC_POLICY)
	tcpc->ic_data.dev = parent;
	tcpc->ic_data.support_pd = 1;
	tcpc->ic_data.drv_data = tcpc;
	ret = tcpc_typec_class_init(tcpc);
	if (ret < 0)
		dev_err(&tcpc->dev, "typec class init fail\n");
	ret = tcpc_get_vbus_dischg_gpio(tcpc, parent);
	if (ret < 0)
		dev_info(&tcpc->dev, "vbus_dischg_gpio is not used\n");
	tcpc->ic_data.vbus_dischar_gpio = ret;
#if IS_ENABLED(CONFIG_SEC_PD)
	ret = tcpc_pd_init(tcpc);
#endif

	tcpc->pp_data = pdic_policy_init(&tcpc->ic_data);
#endif /* CONFIG_PDIC_POLICY */

#if defined(CONFIG_PDIC_NOTIFIER)
	ppdic_sysfs_prop->get_property = tcpc_sysfs_get_prop;
	ppdic_sysfs_prop->set_property = tcpc_sysfs_set_prop;
	ppdic_sysfs_prop->property_is_writeable = tcpc_sysfs_is_writeable;
	ppdic_sysfs_prop->properties = tcpc_sysfs_properties;
	ppdic_sysfs_prop->num_properties = ARRAY_SIZE(tcpc_sysfs_properties);
	ppdic_data->pdic_sysfs_prop = ppdic_sysfs_prop;
	ppdic_data->drv_data = tcpc;
	ppdic_data->name = "et7303";
	ret = pdic_core_register_chip(ppdic_data);

	if (ret) {
		devm_kfree(parent, tcpc);
		devm_kfree(parent, ppdic_data);
		devm_kfree(parent, ppdic_sysfs_prop);
		return ERR_PTR(ret);
	}

	pdic_misc_init(ppdic_data);
	ppdic_data->misc_dev->uvdm_read = sec_dfp_uvdm_in_request_message;
	ppdic_data->misc_dev->uvdm_write = sec_dfp_uvdm_out_request_message;
	ppdic_data->misc_dev->uvdm_ready = sec_dfp_uvdm_ready;
	ppdic_data->misc_dev->uvdm_close = sec_dfp_uvdm_close;
#endif /* CONFIG_PDIC_NOTIFIER */

	return tcpc;
}
EXPORT_SYMBOL(tcpc_device_register);

static int tcpc_device_irq_enable(struct tcpc_device *tcpc)
{
	int ret;

	if (!tcpc->ops->init) {
		pr_err("%s Please implment tcpc ops init function\n",
		__func__);
		return -EINVAL;
	}

	ret = tcpci_init(tcpc, false);
	if (ret < 0) {
		pr_err("%s tcpc init fail\n", __func__);
		return ret;
	}

	tcpci_lock_typec(tcpc);
	ret = tcpc_typec_init(tcpc, tcpc->desc.role_def + 1);
	tcpci_unlock_typec(tcpc);

	if (ret < 0) {
		pr_err("%s : tcpc typec init fail\n", __func__);
		return ret;
	}

	pr_info("%s : tcpc irq enable OK!\n", __func__);
	return 0;
}

static void tcpc_init_work(struct work_struct *work)
{
	struct tcpc_device *tcpc = container_of(
		work, struct tcpc_device, init_work.work);

	if (tcpc->desc.notifier_supply_num == 0)
		return;

	pr_info("%s force start\n", __func__);

	tcpc->desc.notifier_supply_num = 0;
	tcpc_device_irq_enable(tcpc);
}

int tcpc_schedule_init_work(struct tcpc_device *tcpc)
{
	if (tcpc->desc.notifier_supply_num == 0)
		return tcpc_device_irq_enable(tcpc);

	pr_info("%s wait %d num\n", __func__, tcpc->desc.notifier_supply_num);

	schedule_delayed_work(
		&tcpc->init_work, msecs_to_jiffies(30*1000));
	return 0;
}

int register_tcp_dev_notifier(struct tcpc_device *tcp_dev,
			      struct notifier_block *nb)
{
	int ret;

	ret = srcu_notifier_chain_register(&tcp_dev->evt_nh, nb);
	if (ret != 0)
		return ret;

	if (tcp_dev->desc.notifier_supply_num == 0) {
		pr_info("%s already started\n", __func__);
		return 0;
	}

	tcp_dev->desc.notifier_supply_num--;
	pr_info("%s supply_num = %d\n", __func__,
		tcp_dev->desc.notifier_supply_num);

	if (tcp_dev->desc.notifier_supply_num == 0) {
		cancel_delayed_work(&tcp_dev->init_work);
		tcpc_device_irq_enable(tcp_dev);
	}

	return ret;
}
EXPORT_SYMBOL(register_tcp_dev_notifier);

int unregister_tcp_dev_notifier(struct tcpc_device *tcp_dev,
				struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&tcp_dev->evt_nh, nb);
}
EXPORT_SYMBOL(unregister_tcp_dev_notifier);


void tcpc_device_unregister(struct device *dev, struct tcpc_device *tcpc)
{
	if (!tcpc)
		return;

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	pdic_misc_exit();
#endif /* CONFIG_PDIC_NOTIFIER */
#if IS_ENABLED(CONFIG_PDIC_POLICY)
	pdic_policy_deinit(&tcpc->ic_data);
#endif /* CONFIG_PDIC_POLICY */

	tcpc_typec_deinit(tcpc);

	wake_lock_destroy(&tcpc->dettach_temp_wake_lock);
	wake_lock_destroy(&tcpc->attach_wake_lock);

	device_unregister(&tcpc->dev);

}
EXPORT_SYMBOL(tcpc_device_unregister);

void *tcpc_get_dev_data(struct tcpc_device *tcpc)
{
	return tcpc->drv_data;
}
EXPORT_SYMBOL(tcpc_get_dev_data);

void tcpci_lock_typec(struct tcpc_device *tcpc)
{
	mutex_lock(&tcpc->typec_lock);
}

void tcpci_unlock_typec(struct tcpc_device *tcpc)
{
	mutex_unlock(&tcpc->typec_lock);
}

static void tcpc_init_attrs(struct device_type *dev_type)
{
	int i;

	dev_type->groups = tcpc_attr_groups;
	for (i = 0; i < ARRAY_SIZE(tcpc_device_attributes); i++)
		__tcpc_attrs[i] = &tcpc_device_attributes[i].attr;
}

static int __init tcpc_class_init(void)
{
	pr_info("%s (%s)\n", __func__, TCPC_CORE_VERSION);

#ifdef CONFIG_USB_POWER_DELIVERY
	dpm_check_supported_modes();
#endif /* CONFIG_USB_POWER_DELIVERY */

	tcpc_class = class_create(THIS_MODULE, "tcpc");
	if (IS_ERR(tcpc_class)) {
		pr_info("Unable to create tcpc class; errno = %ld\n",
		       PTR_ERR(tcpc_class));
		return PTR_ERR(tcpc_class);
	}
	tcpc_init_attrs(&tcpc_dev_type);

/* add_sec */
//	tcpc_class->suspend = NULL;
//	tcpc_class->resume = NULL;
/**********/

	pr_info("TCPC class init OK\n");
	return 0;
}

static void __exit tcpc_class_exit(void)
{
	class_destroy(tcpc_class);
	pr_info("TCPC class un-init OK\n");
}

subsys_initcall(tcpc_class_init);
module_exit(tcpc_class_exit);

MODULE_DESCRIPTION("Richtek TypeC Port Control Core");
MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_VERSION(TCPC_CORE_VERSION);
MODULE_LICENSE("GPL");
