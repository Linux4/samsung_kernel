/* drivers/muic/muic-core.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/gpio.h>

/* switch device header */
//#ifdef CONFIG_SWITCH
#include <linux/switch.h>
//#endif /* CONFIG_SWITCH */

#if defined(CONFIG_USB_HW_PARAM)
#include <linux/usb_notify.h>
#endif

#include <linux/muic/common/muic.h>

#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/common/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#if IS_ENABLED(CONFIG_USE_SECOND_MUIC)
struct muic_platform_data muic_pdata2;
#endif

#if IS_ENABLED(CONFIG_ANDROID_SWITCH) || IS_ENABLED(CONFIG_SWITCH)
static struct switch_dev switch_dock = {
	.name = "dock",
};

struct switch_dev switch_uart3 = {
	.name = "uart3", /* sys/class/switch/uart3/state */
};

#ifdef CONFIG_SEC_FACTORY
struct switch_dev switch_attached_muic_cable = {
	.name = "attached_muic_cable", /* sys/class/switch/attached_muic_cable/state */
};
#endif
#endif /* CONFIG_ANDROID_SWITCH || CONFIG_SWITCH */

#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
static struct notifier_block dock_notifier_block;
static struct notifier_block cable_data_notifier_block;

void muic_send_dock_intent(int type)
{
	pr_info("%s: MUIC dock type(%d)\n", __func__, type);
#if IS_ENABLED(CONFIG_ANDROID_SWITCH) || IS_ENABLED(CONFIG_SWITCH)
	switch_set_state(&switch_dock, type);
#endif
}
EXPORT_SYMBOL(muic_send_dock_intent);

#ifdef CONFIG_SEC_FACTORY
void muic_send_attached_muic_cable_intent(int type)
{
	pr_info("%s: MUIC attached_muic_cable type(%d)\n", __func__, type);
#if IS_ENABLED(CONFIG_ANDROID_SWITCH) || IS_ENABLED(CONFIG_SWITCH)
	switch_set_state(&switch_attached_muic_cable, type);
#endif
}
#endif

static int muic_dock_attach_notify(int type, const char *name)
{
	pr_info("%s: %s\n", __func__, name);
	muic_send_dock_intent(type);

	return NOTIFY_OK;
}

static int muic_dock_detach_notify(void)
{
	pr_info("%s\n", __func__);
	muic_send_dock_intent(MUIC_DOCK_DETACHED);

	return NOTIFY_OK;
}

static int muic_handle_dock_notification(struct notifier_block *nb,
			unsigned long action, void *data)
{
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER) && IS_ENABLED(CONFIG_MUIC_SUPPORT_PDIC)
	PD_NOTI_ATTACH_TYPEDEF *pnoti = (PD_NOTI_ATTACH_TYPEDEF *)data;
	muic_attached_dev_t attached_dev = pnoti->cable_type;
#else
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;
#endif
	int type = MUIC_DOCK_DETACHED;
	const char *name;

	switch (attached_dev) {
	case ATTACHED_DEV_DESKDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_VB_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) {
			type = MUIC_DOCK_DESKDOCK;
			name = "Desk Dock Attach";
			return muic_dock_attach_notify(type, name);
		}
		else if (action == MUIC_NOTIFY_CMD_DETACH)
			return muic_dock_detach_notify();
		break;
	case ATTACHED_DEV_CARDOCK_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) {
			type = MUIC_DOCK_CARDOCK;
			name = "Car Dock Attach";
			return muic_dock_attach_notify(type, name);
		}
		else if (action == MUIC_NOTIFY_CMD_DETACH)
			return muic_dock_detach_notify();
		break;
	case ATTACHED_DEV_SMARTDOCK_MUIC:
	case ATTACHED_DEV_SMARTDOCK_VB_MUIC:
	case ATTACHED_DEV_SMARTDOCK_TA_MUIC:
	case ATTACHED_DEV_SMARTDOCK_USB_MUIC:
		if (action == MUIC_NOTIFY_CMD_LOGICALLY_ATTACH) {
			type = MUIC_DOCK_SMARTDOCK;
			name = "Smart Dock Attach";
			return muic_dock_attach_notify(type, name);
		}
		else if (action == MUIC_NOTIFY_CMD_LOGICALLY_DETACH)
			return muic_dock_detach_notify();
		break;
	case ATTACHED_DEV_UNIVERSAL_MMDOCK_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) {
			type = MUIC_DOCK_SMARTDOCK;
			name = "Universal MMDock Attach";
			return muic_dock_attach_notify(type, name);
		}
		else if (action == MUIC_NOTIFY_CMD_DETACH)
			return muic_dock_detach_notify();
		break;
	case ATTACHED_DEV_AUDIODOCK_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) {
			type = MUIC_DOCK_AUDIODOCK;
			name = "Audio Dock Attach";
			return muic_dock_attach_notify(type, name);
		}
		else if (action == MUIC_NOTIFY_CMD_DETACH)
			return muic_dock_detach_notify();
		break;
	case ATTACHED_DEV_HMT_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) {
			type = MUIC_DOCK_HMT;
			name = "HMT Attach";
			return muic_dock_attach_notify(type, name);
		}
		else if (action == MUIC_NOTIFY_CMD_DETACH)
			return muic_dock_detach_notify();
		break;
	case ATTACHED_DEV_GAMEPAD_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) {
			type = MUIC_DOCK_GAMEPAD;
			name = "Gamepad Attach";
			return muic_dock_attach_notify(type, name);
		} else if (action == MUIC_NOTIFY_CMD_DETACH)
			return muic_dock_detach_notify();
		break;
	default:
		break;
	}

	pr_info("%s: ignore(%d)\n", __func__, attached_dev);
	return NOTIFY_DONE;
}

static int muic_handle_cable_data_notification(struct notifier_block *nb,
			unsigned long action, void *data)
{
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER) && IS_ENABLED(CONFIG_MUIC_SUPPORT_PDIC)
	PD_NOTI_ATTACH_TYPEDEF *pnoti = (PD_NOTI_ATTACH_TYPEDEF *)data;
	muic_attached_dev_t attached_dev = pnoti->cable_type;
#else
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;
#endif
	int jig_state = 0;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	switch (attached_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:		/* VBUS enabled */
	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:	/* for otg test */
	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:	/* for fg test */
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:		/* VBUS enabled */
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH)
			jig_state = 1;
		break;
#if defined(CONFIG_USB_HW_PARAM)
	case ATTACHED_DEV_TIMEOUT_OPEN_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH && o_notify)
			inc_hw_param(o_notify, USB_MUIC_DCD_TIMEOUT_COUNT);
		break;
#endif
	default:
		jig_state = 0;
		break;
	}

	pr_info("%s: MUIC uart type(%d)\n", __func__, jig_state);
#if IS_ENABLED(CONFIG_ANDROID_SWITCH) || IS_ENABLED(CONFIG_SWITCH)
	switch_set_state(&switch_uart3, jig_state);
#endif
	return NOTIFY_DONE;
}
#endif /* CONFIG_MUIC_NOTIFIER */

static void muic_init_switch_dev_cb(void)
{
#if IS_ENABLED(CONFIG_ANDROID_SWITCH) || IS_ENABLED(CONFIG_SWITCH)
	int ret;

	/* for DockObserver */
	ret = switch_dev_register(&switch_dock);
	if (ret < 0) {
		pr_err("%s: Failed to register dock switch(%d)\n",
				__func__, ret);
		return;
	}

	/* for UART event */
	ret = switch_dev_register(&switch_uart3);
	if (ret < 0) {
		pr_err("%s: Failed to register uart3 switch(%d)\n",
				__func__, ret);
		return;
	}

#ifdef CONFIG_SEC_FACTORY
	/* for cable type event */
	ret = switch_dev_register(&switch_attached_muic_cable);
	if (ret < 0) {
		pr_err("%s: Failed to register attached_muic_cable switch(%d)\n",
				__func__, ret);
		return;
	}
#endif
#endif /* CONFIG_ANDROID_SWITCH || CONFIG_SWITCH */

#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
	muic_notifier_register(&dock_notifier_block,
			muic_handle_dock_notification, MUIC_NOTIFY_DEV_DOCK);
	muic_notifier_register(&cable_data_notifier_block,
			muic_handle_cable_data_notification, MUIC_NOTIFY_DEV_CABLE_DATA);
#endif /* CONFIG_MUIC_NOTIFIER */

	pr_info("%s: done\n", __func__);
}

static void muic_cleanup_switch_dev_cb(void)
{
#if IS_ENABLED(CONFIG_ANDROID_SWITCH) || IS_ENABLED(CONFIG_SWITCH)
#ifdef CONFIG_SEC_FACTORY
	/* for cable type event */
	switch_dev_unregister(&switch_attached_muic_cable);
#endif
	/* for UART event */
	switch_dev_unregister(&switch_uart3);
	/* for DockObserver */
	switch_dev_unregister(&switch_dock);
#endif /* CONFIG_ANDROID_SWITCH || CONFIG_SWITCH */
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
	muic_notifier_unregister(&dock_notifier_block);
	muic_notifier_unregister(&cable_data_notifier_block);
#endif /* CONFIG_MUIC_NOTIFIER */

	pr_info("%s: done\n", __func__);
}

extern struct muic_platform_data muic_pdata;
#if IS_MODULE(CONFIG_MUIC_NOTIFIER)
extern pmic_info;
/*
 * switch_sel value get from bootloader command line
 * switch_sel data consist 8 bits (xxxxyyyyzzzz)
 * first 4bits(zzzz) mean path information.
 * next 4bits(yyyy) mean if pmic version info
 * next 4bits(xxxx) mean afc disable info
 */
int get_switch_sel(void)
{
	muic_pdata.switch_sel = pmic_info;
	muic_pdata.switch_sel = (muic_pdata.switch_sel) & 0xfff;
	pr_info("%s: switch_sel: 0x%03x\n", __func__,
			muic_pdata.switch_sel);

	return muic_pdata.switch_sel;
}
EXPORT_SYMBOL(get_switch_sel);

static int afc_mode = 0;
extern charging_mode;
/* afc_mode:
 *   0x31 : Disabled
 *   0x30 : Enabled
 */
int get_afc_mode(void)
{
	afc_mode = (charging_mode & 0x0000FF00) >> 8;
	pr_info("%s: afc_mode is 0x%02x\n", __func__, afc_mode);

	return afc_mode;
}
EXPORT_SYMBOL(get_afc_mode);

#if defined(CONFIG_USB_ARCH_EXYNOS)
extern ccic_info;
/*
 * __ccic_info :
 * b'0: 1 if an active ccic is present,
 *        0 when muic works without ccic chip or
 *              no ccic Noti. registration is needed
 *              even though a ccic chip is present.
 */
int get_pdic_info(void)
{
	pr_info("%s: ccic_info: 0x%04x\n", __func__, ccic_info);

	return ccic_info;
}
EXPORT_SYMBOL(get_pdic_info);
#endif
#else
/* func : set_switch_sel for LSI boot command
 * switch_sel value get from bootloader command line
 * switch_sel data consist 8 bits (xxxxyyyyzzzz)
 * first 4bits(zzzz) mean path information.
 * next 4bits(yyyy) mean if pmic version info
 * next 4bits(xxxx) mean afc disable info
 */
static int switch_sel = -1;
static int set_switch_sel(char *str)
{
	get_option(&str, &switch_sel);
	switch_sel = switch_sel & 0xfff;
	pr_info("%s: switch_sel: 0x%03x\n", __func__, switch_sel);

	return switch_sel;
}
__setup("pmic_info=", set_switch_sel);

int get_switch_sel(void)
{
	return switch_sel;
}

/* func : set_uart_sel for QC boot command
 * uart_sel value get from bootloader command line
 */
static int uart_sel = -1;
static int __init set_uart_sel(char *str)
{
	get_option(&str, &uart_sel);
	pr_info("%s: uart_sel is 0x%02x\n", __func__, uart_sel);

	return 0;
}
early_param("uart_sel", set_uart_sel);

int get_uart_sel(void)
{
	return uart_sel;
}

/* afc_mode:
 *   0x31 : Disabled
 *   0x30 : Enabled
 */
static int afc_mode = 0;
#if defined(CONFIG_SEC_MPARAM)
extern charging_mode;
#endif

/* for LSI boot command */
static int __init set_charging_mode(char *str)
{
	int mode;
	get_option(&str, &mode);
	afc_mode = (mode & 0x0000FF00) >> 8;
	pr_info("%s: afc_mode is 0x%02x\n", __func__, afc_mode);

	return 0;
}
early_param("charging_mode", set_charging_mode);

/* for QC boot command */
static int __init set_afc_disable(char *str)
{
	get_option(&str, &afc_mode);
	pr_info("%s: afc_mode is 0x%02x\n", __func__, afc_mode);

	return 0;
}
early_param("afc_disable", set_afc_disable);

int get_afc_mode(void)
{
#if defined(CONFIG_SEC_MPARAM)
	afc_mode = (charging_mode & 0x0000FF00) >> 8;
	pr_info("%s: afc_mode is 0x%02x\n", __func__, afc_mode);
#endif

	return afc_mode;
}

static int __pdic_info;
/*
 * __pdic_info :
 * b'0: 1 if an active pdic is present,
 *        0 when muic works without pdic chip or
 *              no pdic Noti. registration is needed
 *              even though a pdic chip is present.
 */
static int set_pdic_info(char *str)
{
	get_option(&str, &__pdic_info);

	pr_info("%s: pdic_info: 0x%04x\n", __func__, __pdic_info);

	return __pdic_info;
}
__setup("ccic_info=", set_pdic_info);

int get_pdic_info(void)
{
	return __pdic_info;
}
EXPORT_SYMBOL_GPL(get_pdic_info);
#endif

bool is_muic_usb_path_ap_usb(void)
{
	if (MUIC_PATH_USB_AP == muic_pdata.usb_path) {
		pr_info("%s: [%d]\n", __func__, muic_pdata.usb_path);
		return true;
	}

	return false;
}

bool is_muic_usb_path_cp_usb(void)
{
	if (MUIC_PATH_USB_CP == muic_pdata.usb_path) {
		pr_info("%s: [%d]\n", __func__, muic_pdata.usb_path);
		return true;
	}

	return false;
}

#if IS_MODULE(CONFIG_MUIC_NOTIFIER)
static int muic_init_gpio_cb(int switch_sel)
{
	struct muic_platform_data *pdata = &muic_pdata;
	const char *usb_mode;
	const char *uart_mode;
	int ret = 0;

	pr_info("%s (%d)\n", __func__, switch_sel);

	if (switch_sel & SWITCH_SEL_USB_MASK) {
		pdata->usb_path = MUIC_PATH_USB_AP;
		usb_mode = "PDA";
	} else {
		pdata->usb_path = MUIC_PATH_USB_CP;
		usb_mode = "MODEM";
	}

	if (pdata->set_gpio_usb_sel)
		ret = pdata->set_gpio_usb_sel(pdata->drv_data, pdata->usb_path);

	if (switch_sel & SWITCH_SEL_UART_MASK) {
		pdata->uart_path = MUIC_PATH_UART_AP;
		uart_mode = "AP";
	} else {
		pdata->uart_path = MUIC_PATH_UART_CP;
		uart_mode = "CP";
	}

	/* These flags MUST be updated again from probe function */
	pdata->rustproof_on = false;

#if !defined(CONFIG_SEC_FACTORY) && defined(CONFIG_MUIC_SUPPORT_TYPEB)
	if (!(switch_sel & SWITCH_SEL_RUSTPROOF_MASK))
		pdata->rustproof_on = true;
#endif

	pdata->afc_disable = false;

	if (pdata->set_gpio_uart_sel)
		ret = pdata->set_gpio_uart_sel(pdata->drv_data, pdata->uart_path);

	pr_info("%s: usb_path(%s), uart_path(%s)\n", __func__,
			usb_mode, uart_mode);

	return ret;
}
#else
static int muic_init_gpio_cb(void)
{
	struct muic_platform_data *pdata = &muic_pdata;
	const char *usb_mode;
	const char *uart_mode;
	int ret = 0;

	pr_info("%s: switch_sel(%d) uart_sel(%d)\n", __func__, switch_sel, uart_sel);

	pdata->usb_path = MUIC_PATH_USB_AP;
	usb_mode = "PDA";

	pdata->uart_path = MUIC_PATH_UART_AP;
	uart_mode = "AP";

	if (switch_sel != -1) {
		if (switch_sel & SWITCH_SEL_USB_MASK) {
			pdata->usb_path = MUIC_PATH_USB_AP;
			usb_mode = "PDA";
		} else {
			pdata->usb_path = MUIC_PATH_USB_CP;
			usb_mode = "MODEM";
		}
		if (switch_sel & SWITCH_SEL_UART_MASK) {
			pdata->uart_path = MUIC_PATH_UART_AP;
			uart_mode = "AP";
		} else {
			pdata->uart_path = MUIC_PATH_UART_CP;
			uart_mode = "CP";
		}
#if IS_ENABLED(CONFIG_MUIC_UART_SWITCH)
		if (switch_sel & SWITCH_SEL_UART_MASK2) {
			pdata->uart_path = MUIC_PATH_UART_CP2;
			uart_mode = "CP2";
		}
#endif
	}

#if defined(CONFIG_MUIC_SUPPORT_UART_SEL)
	if (get_uart_sel() == MUIC_PATH_UART_CP) {
		pdata->uart_path = MUIC_PATH_UART_CP;
		uart_mode = "CP";
	}
#endif

	/* These flags MUST be updated again from probe function */
	pdata->rustproof_on = false;

	pdata->afc_disable = false;

	if (pdata->set_gpio_usb_sel)
		ret = pdata->set_gpio_usb_sel(pdata->drv_data, pdata->usb_path);

	if (pdata->set_gpio_uart_sel)
		ret = pdata->set_gpio_uart_sel(pdata->drv_data, pdata->uart_path);

	pr_info("%s: usb_path(%s), uart_path(%s)\n", __func__,
			usb_mode, uart_mode);

	return ret;
}
#endif

int muic_afc_get_voltage(void)
{
	struct muic_platform_data *pdata = &muic_pdata;
	int vol = -ENODEV;

	pr_info("%s : %dV\n", __func__, vol);

	if (pdata && pdata->muic_afc_get_voltage_cb)
		vol = pdata->muic_afc_get_voltage_cb();

	return vol;
}
EXPORT_SYMBOL(muic_afc_get_voltage);

#if !defined(CONFIG_DISCRETE_CHARGER)
int muic_afc_set_voltage(int voltage)
{
	struct muic_platform_data *pdata = &muic_pdata;
#if defined(CONFIG_USE_SECOND_MUIC)
	struct muic_platform_data *pdata2 = &muic_pdata2;
#endif

	pr_info("%s : %dV\n", __func__, voltage);

#if defined(CONFIG_USE_SECOND_MUIC)
	if (pdata2->muic_afc_set_voltage_cb)
		pdata2->muic_afc_set_voltage_cb(voltage);
#endif

	if (pdata && pdata->muic_afc_set_voltage_cb)
		return pdata->muic_afc_set_voltage_cb(voltage);

	pr_err("%s: cannot supported\n", __func__);
	return -ENODEV;
}
EXPORT_SYMBOL(muic_afc_set_voltage);
#endif

int muic_hv_charger_disable(bool en)
{
	struct muic_platform_data *pdata = &muic_pdata;
#if defined(CONFIG_USE_SECOND_MUIC)
	struct muic_platform_data *pdata2 = &muic_pdata2;
#endif
	int ret = -ENODEV;

	pr_info("%s %sable\n", __func__, en ? "en" : "dis");

#if defined(CONFIG_USE_SECOND_MUIC)
	if (pdata2->muic_hv_charger_disable_cb)
		ret = pdata2->muic_hv_charger_disable_cb(en);
#endif

	if (pdata && pdata->muic_hv_charger_disable_cb)
		ret = pdata->muic_hv_charger_disable_cb(en);

	return ret;
}
EXPORT_SYMBOL(muic_hv_charger_disable);

int muic_hv_charger_init(void)
{
	struct muic_platform_data *pdata = &muic_pdata;

	if (pdata && pdata->muic_hv_charger_init_cb)
		return pdata->muic_hv_charger_init_cb();

	pr_err("%s: cannot supported\n", __func__);
	return -ENODEV;
}
EXPORT_SYMBOL(muic_hv_charger_init);

int muic_set_hiccup_mode(int on_off)
{
	struct muic_platform_data *pdata = &muic_pdata;

	if (pdata && pdata->muic_set_hiccup_mode_cb)
		return pdata->muic_set_hiccup_mode_cb(on_off);

	pr_err("%s: cannot supported\n", __func__);
	return -ENODEV;
}
EXPORT_SYMBOL_GPL(muic_set_hiccup_mode);

struct muic_platform_data muic_pdata = {
	.init_switch_dev_cb	= muic_init_switch_dev_cb,
	.cleanup_switch_dev_cb	= muic_cleanup_switch_dev_cb,
	.init_gpio_cb		= muic_init_gpio_cb,
};
EXPORT_SYMBOL(muic_pdata);

