/*
 *  sec-switch.c
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/switch.h>
#include "devices.h"
#include <linux/power_supply.h>

#if defined(CONFIG_USB_SWITCH_TSU6721) || defined(CONFIG_USB_SWITCH_RT8973) || defined(CONFIG_SM5502_MUIC)
#include <linux/i2c/tsu6721.h>
#define DEBUG_STATUS   			1
#endif
#if defined(CONFIG_USB_SWITCH_RT8973)
#include <linux/i2c/rt8973.h>
#endif

#if defined(CONFIG_SM5502_MUIC)
#include <linux/i2c/sm5502.h>
#endif

#define BATT_SEARCH_CNT_MAX       10
#ifdef CONFIG_MFD_MAX77803
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/gpio_keys.h>
#include <linux/gpio_event.h>

#include <linux/regulator/machine.h>
#include <linux/regulator/max8649.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/wm8994/pdata.h>
#include <linux/mfd/max77803.h>
#include <linux/mfd/max77803-private.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/gpio.h>

#include <linux/power_supply.h>

#include <linux/switch.h>

#ifdef CONFIG_MACH_STRETTO
#include <mach/stretto-gpio.h>
#endif

#include "devices.h"

#if !defined(CONFIG_USB_SWITCH_RT8973)
#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/host_notify.h>
#endif
#endif
#include <linux/pm_runtime.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>

#ifdef CONFIG_JACK_MON
#include <linux/jack.h>
#endif

#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI
#include <linux/i2c/synaptics_rmi.h>
#endif
#include <linux/sec_class.h>
static int MHL_Connected;
struct switch_dev switch_dock = {
	.name = "dock",
};

struct device *switch_dev;
EXPORT_SYMBOL(switch_dev);

#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI
struct synaptics_rmi_callbacks *charger_callbacks;
#endif

/* charger cable state */
bool is_cable_attached;
bool is_jig_attached;

/* LPM mode status */
extern int poweroff_charging;

static ssize_t midas_switch_show_vbus(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	int i;
	struct regulator *regulator;

	regulator = regulator_get(NULL, "safeout1");
	if (IS_ERR(regulator)) {
		pr_warn("%s: fail to get regulator\n", __func__);
		return sprintf(buf, "UNKNOWN\n");
	}
	if (regulator_is_enabled(regulator))
		i = sprintf(buf, "VBUS is enabled\n");
	else
		i = sprintf(buf, "VBUS is disabled\n");
	regulator_put(regulator);

	return i;
}
#if 0 //midas_switch_not enabled
static ssize_t midas_switch_store_vbus(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	int disable, ret, usb_mode;
	struct regulator *regulator;
	/* struct s3c_udc *udc = platform_get_drvdata(&s3c_device_usbgadget); */

	if (!strncmp(buf, "0", 1))
		disable = 0;
	else if (!strncmp(buf, "1", 1))
		disable = 1;
	else {
		pr_warn("%s: Wrong command\n", __func__);
		return count;
	}

	pr_info("%s: disable=%d\n", __func__, disable);
	usb_mode = disable ? USB_CABLE_DETACHED_WITHOUT_NOTI : USB_CABLE_ATTACHED;
	/* ret = udc->change_usb_mode(usb_mode); */
	ret = -1;
	if (ret < 0)
		pr_err("%s: fail to change mode!!!\n", __func__);

	regulator = regulator_get(NULL, "safeout1");
	if (IS_ERR(regulator)) {
		pr_warn("%s: fail to get regulator\n", __func__);
		return count;
	}

	if (disable) {
		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
		if (!regulator_is_enabled(regulator))
			regulator_enable(regulator);
	} else {
		if (!regulator_is_enabled(regulator))
			regulator_enable(regulator);
	}
	regulator_put(regulator);

	return count;
}

DEVICE_ATTR(disable_vbus, 0664, midas_switch_show_vbus,
	    midas_switch_store_vbus);
#endif

#ifdef CONFIG_TARGET_LOCALE_KOR
#include "../../../drivers/usb/gadget/s3c_udc.h"
/* usb access control for SEC DM */
struct device *usb_lock;
static int is_usb_locked;

static ssize_t midas_switch_show_usb_lock(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (is_usb_locked)
		return snprintf(buf, PAGE_SIZE, "USB_LOCK");
	else
		return snprintf(buf, PAGE_SIZE, "USB_UNLOCK");
}

static ssize_t midas_switch_store_usb_lock(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int lock;
	struct s3c_udc *udc =
		platform_get_drvdata(&msm8960_device_gadget_peripheral);

	if (!strncmp(buf, "0", 1))
		lock = 0;
	else if (!strncmp(buf, "1", 1))
		lock = 1;
	else {
		pr_warn("%s: Wrong command\n", __func__);
		return count;
	}

	if (IS_ERR_OR_NULL(udc))
		return count;

	pr_info("%s: lock=%d\n", __func__, lock);

	if (lock != is_usb_locked) {
		is_usb_locked = lock;

		if (lock) {
			if (udc->udc_enabled)
				usb_gadget_vbus_disconnect(&udc->gadget);
		}
	}

	return count;
}

static DEVICE_ATTR(enable, 0664,
		   midas_switch_show_usb_lock, midas_switch_store_usb_lock);
#endif

static int __init midas_sec_switch_init(void)
{
	int ret;
	switch_dev = device_create(sec_class, NULL, 0, NULL, "switch");

	if (IS_ERR(switch_dev))
		pr_err("Failed to create device(switch)!\n");

	ret = device_create_file(switch_dev, &dev_attr_disable_vbus);
	if (ret)
		pr_err("Failed to create device file(disable_vbus)!\n");

#ifdef CONFIG_TARGET_LOCALE_KOR
	usb_lock = device_create(sec_class, switch_dev,
				MKDEV(0, 0), NULL, ".usb_lock");

	if (IS_ERR(usb_lock))
		pr_err("Failed to create device (usb_lock)!\n");

	if (device_create_file(usb_lock, &dev_attr_enable) < 0)
		pr_err("Failed to create device file(.usblock/enable)!\n");
#endif

	return 0;
};

int current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
extern int poweroff_charging;
int max77803_muic_charger_cb(enum cable_type_muic cable_type)
{
#ifdef CONFIG_CHARGER_MAX77803
	struct power_supply *psy = power_supply_get_by_name("battery");
	union power_supply_propval value;
	static enum cable_type_muic previous_cable_type = CABLE_TYPE_NONE_MUIC;
#endif
	pr_info("%s: cable type : %d\n", __func__, cable_type);

#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI

	if (charger_callbacks && charger_callbacks->inform_charger)
		charger_callbacks->inform_charger(charger_callbacks,
		cable_type);

#endif

#ifdef CONFIG_JACK_MON
	switch (cable_type) {
	case CABLE_TYPE_OTG_MUIC:
	case CABLE_TYPE_NONE_MUIC:
	case CABLE_TYPE_JIG_UART_OFF_MUIC:
	case CABLE_TYPE_MHL_MUIC:
		is_cable_attached = false;
		break;
	case CABLE_TYPE_USB_MUIC:
#ifdef CONFIG_CHARGER_MAX77803
		value.intval = POWER_SUPPLY_TYPE_USB;
#endif
	case CABLE_TYPE_JIG_USB_OFF_MUIC:
	case CABLE_TYPE_JIG_USB_ON_MUIC:
	case CABLE_TYPE_SMARTDOCK_USB_MUIC:
		is_cable_attached = true;
		break;
	case CABLE_TYPE_MHL_VB_MUIC:
		is_cable_attached = true;
		break;
	case CABLE_TYPE_AUDIODOCK_MUIC:
	case CABLE_TYPE_TA_MUIC:
	case CABLE_TYPE_CARDOCK_MUIC:
	case CABLE_TYPE_DESKDOCK_MUIC:
	case CABLE_TYPE_SMARTDOCK_MUIC:
	case CABLE_TYPE_SMARTDOCK_TA_MUIC:
	case CABLE_TYPE_JIG_UART_OFF_VB_MUIC:
	case CABLE_TYPE_INCOMPATIBLE_MUIC:
		is_cable_attached = true;
		break;
	default:
		pr_err("%s: invalid type:%d\n", __func__, cable_type);
		return -EINVAL;
	}
#endif

#ifdef CONFIG_CHARGER_MAX77803
	/*  charger setting */
	if (previous_cable_type == cable_type) {
		pr_info("%s: SKIP cable setting\n", __func__);
		goto skip;
	}

	switch (cable_type) {
	case CABLE_TYPE_NONE_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;

	case CABLE_TYPE_MHL_VB_MUIC:
		if(poweroff_charging)
			current_cable_type = POWER_SUPPLY_TYPE_USB;
		else
			goto skip;
		break;
	case CABLE_TYPE_MHL_MUIC:
		if(poweroff_charging)
			current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		else
			goto skip;
		break;
	case CABLE_TYPE_USB_MUIC:
	case CABLE_TYPE_JIG_USB_OFF_MUIC:
	case CABLE_TYPE_JIG_USB_ON_MUIC:
	case CABLE_TYPE_SMARTDOCK_USB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case CABLE_TYPE_JIG_UART_OFF_VB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_UARTOFF;
		break;
	case CABLE_TYPE_TA_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;
	case CABLE_TYPE_AUDIODOCK_MUIC:
	case CABLE_TYPE_CARDOCK_MUIC:
	case CABLE_TYPE_DESKDOCK_MUIC:
	case CABLE_TYPE_SMARTDOCK_MUIC:
	case CABLE_TYPE_SMARTDOCK_TA_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_MISC;
		break;
	case CABLE_TYPE_OTG_MUIC:
		goto skip;
	case CABLE_TYPE_JIG_UART_OFF_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case CABLE_TYPE_INCOMPATIBLE_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	default:
		pr_err("%s: invalid type for charger:%d\n",
				__func__, cable_type);
		goto skip;
	}
#if defined(CONFIG_SEC_J_PROJECT) && defined(CONFIG_USB_HOST_NOTIFY)
	sec_otg_wake_unlock(cable_type,previous_cable_type);
#endif
	previous_cable_type = cable_type;

	if (!psy || !psy->set_property)
		pr_err("%s: fail to get battery psy\n", __func__);
	else {
		value.intval = current_cable_type<<ONLINE_TYPE_MAIN_SHIFT;
		psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	}
skip:
#endif
#ifdef CONFIG_JACK_MON
	jack_event_handler("charger", is_cable_attached);
#endif

	return 0;
}

int max77803_get_jig_state(void)
{
	pr_info("%s: %d\n", __func__, is_jig_attached);
	return is_jig_attached;
}
EXPORT_SYMBOL(max77803_get_jig_state);

void max77803_set_jig_state(int jig_state)
{
	pr_info("%s: %d\n", __func__, jig_state);
	is_jig_attached = jig_state;
}

//extern void sec_otg_set_vbus_state(int);
/* usb cable call back function */
void max77803_muic_usb_cb(u8 usb_mode)
{
#ifdef CONFIG_TARGET_LOCALE_KOR
	if (is_usb_locked) {
		pr_info("%s: usb locked by mdm\n", __func__);
		return;
	}
#endif

	pr_info("%s: MUIC attached: %d\n", __func__, usb_mode);
	if (usb_mode == USB_CABLE_DETACHED
		|| usb_mode == USB_CABLE_ATTACHED) {
		pr_info("msm_otg_set_vbus_state(%d)", usb_mode);
		sec_otg_set_vbus_state(usb_mode);
#if !defined(CONFIG_USB_SWITCH_RT8973)
#ifdef CONFIG_USB_HOST_NOTIFY
	} else if (usb_mode == USB_OTGHOST_DETACHED
		|| usb_mode == USB_OTGHOST_ATTACHED) {

		if (usb_mode == USB_OTGHOST_DETACHED) {
			pr_info("sec_otg_set_id_state(1)");
			sec_otg_set_id_state(1);
		} else {
			pr_info("sec_otg_set_id_state(0)");
			sec_otg_set_id_state(0);
		}

	} else if (usb_mode == USB_POWERED_HOST_DETACHED
		|| usb_mode == USB_POWERED_HOST_ATTACHED) {
		if (usb_mode == USB_POWERED_HOST_DETACHED){
			pr_info("msm_otg_set_smartdock_state(1)");
			msm_otg_set_smartdock_state(1);
		}else{
			pr_info("msm_otg_set_smartdock_state(0)");
			msm_otg_set_smartdock_state(0);
		}
#endif
#endif
	}

#ifdef CONFIG_JACK_MON
	if (usb_mode == USB_OTGHOST_ATTACHED
	|| usb_mode == USB_POWERED_HOST_ATTACHED)
		jack_event_handler("host", USB_CABLE_ATTACHED);
	else if (usb_mode == USB_OTGHOST_DETACHED
	|| usb_mode == USB_POWERED_HOST_DETACHED)
		jack_event_handler("host", USB_CABLE_DETACHED);
	else if ((usb_mode == USB_CABLE_ATTACHED)
		|| (usb_mode == USB_CABLE_DETACHED))
		jack_event_handler("usb", usb_mode);
#endif

}

#ifdef CONFIG_VIDEO_MHL_V2
static BLOCKING_NOTIFIER_HEAD(acc_mhl_notifier);
int acc_register_notifier(struct notifier_block *nb)
{
	int ret;
	ret = blocking_notifier_chain_register(&acc_mhl_notifier, nb);
	return ret;
}

int acc_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&acc_mhl_notifier, nb);
}

static int acc_notify(int event)
{
	int ret;
	ret = blocking_notifier_call_chain(&acc_mhl_notifier, event, NULL);
	return ret;
}
#endif

/*extern void MHL_On(bool on);*/
void max77803_muic_mhl_cb(int attached)
{
	pr_info("%s: MUIC attached: %d\n", __func__, attached);
	if (attached == MAX77803_MUIC_ATTACHED) {
		/*MHL_On(1);*/ /* GPIO_LEVEL_HIGH */
		pr_info("MHL Attached !!\n");
#ifdef CONFIG_VIDEO_MHL_V2
		if (!MHL_Connected) {
			acc_notify(1);
			MHL_Connected = 1;
		} else {
			pr_info("MHL Attached but ignored!!\n");
		}
#endif
	} else {
		/*MHL_On(0);*/ /* GPIO_LEVEL_LOW */
		pr_info("MHL Detached !!\n");
#ifdef CONFIG_VIDEO_MHL_V2
		acc_notify(0);
		MHL_Connected = 0;
#endif
	}
}

bool max77803_muic_is_mhl_attached(void)
{
	return 0;
}

void max77803_muic_dock_cb(int type)
{
	pr_info("%s: MUIC attached: %d\n", __func__, type);

#ifdef CONFIG_JACK_MON
	jack_event_handler("cradle", type);
#endif
	switch_set_state(&switch_dock, type);
}

void max77803_muic_init_cb(void)
{
	int ret;

	/* for CarDock, DeskDock */
	ret = switch_dev_register(&switch_dock);

	pr_info("%s: MUIC ret=%d\n", __func__, ret);

	if (ret < 0)
		pr_err("Failed to register dock switch. %d\n", ret);
}

#if !defined(CONFIG_USB_SWITCH_RT8973)
#ifdef CONFIG_USB_HOST_NOTIFY
int max77803_muic_host_notify_cb(int enable)
{
	return sec_otg_notify_cb(enable);
}
#endif
#endif
int max77803_muic_set_safeout(int path)
{
	struct regulator *regulator;

	pr_info("%s: MUIC safeout path=%d\n", __func__, path);

	if (path == CP_USB_MODE) {
		regulator = regulator_get(NULL, "safeout1");
		if (IS_ERR(regulator))
			return -ENODEV;
		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
		regulator_put(regulator);

		regulator = regulator_get(NULL, "safeout2");
		if (IS_ERR(regulator))
			return -ENODEV;
		if (!regulator_is_enabled(regulator))
			regulator_enable(regulator);
		regulator_put(regulator);
	} else {
		/* AP_USB_MODE || AUDIO_MODE */
		regulator = regulator_get(NULL, "safeout1");
		if (IS_ERR(regulator))
			return -ENODEV;
		if (!regulator_is_enabled(regulator))
			regulator_enable(regulator);
		regulator_put(regulator);

		regulator = regulator_get(NULL, "safeout2");
		if (IS_ERR(regulator))
			return -ENODEV;
		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
		regulator_put(regulator);
	}

	return 0;
}

struct max77803_muic_data max77803_muic = {
	.usb_cb = max77803_muic_usb_cb,
	.charger_cb = max77803_muic_charger_cb,
	.mhl_cb = max77803_muic_mhl_cb,
	.is_mhl_attached = max77803_muic_is_mhl_attached,
	.set_safeout = max77803_muic_set_safeout,
	.init_cb = max77803_muic_init_cb,
	.dock_cb = max77803_muic_dock_cb,
#if !defined(CONFIG_USB_SWITCH_RT8973)
#ifdef CONFIG_USB_HOST_NOTIFY
	.host_notify_cb = max77803_muic_host_notify_cb,
#else
	.host_notify_cb = NULL,
#endif
#else
	.host_notify_cb = NULL,
#endif	
	.gpio_usb_sel = -1,
	.jig_state = max77803_set_jig_state,
};

device_initcall(midas_sec_switch_init);
#endif


#if defined(CONFIG_USB_SWITCH_TSU6721) || defined(CONFIG_USB_SWITCH_RT8973) || defined(CONFIG_SM5502_MUIC)
#include <linux/switch.h>

static struct switch_dev switch_dock = {
      .name = "dock",
};

struct device *switch_dev;
EXPORT_SYMBOL(switch_dev);

extern void sec_otg_set_vbus_state(int);

static enum cable_type_t set_cable_status;
int msm8930_get_cable_status(void) {return (int)set_cable_status; }

/* support for LPM charging */
//#ifdef CONFIG_SAMSUNG_LPM_MODE
bool sec_bat_is_lpm(void)
{return (bool)poweroff_charging; }

int sec_bat_get_cable_status(void)
{
	int rc;
	struct power_supply *psy;
	union power_supply_propval value;

	psy = power_supply_get_by_name("battery");
	switch (set_cable_status) {
	case CABLE_TYPE_MISC:
		value.intval = POWER_SUPPLY_TYPE_MISC;
		break;
	case CABLE_TYPE_USB:
		value.intval = POWER_SUPPLY_TYPE_USB;
		break;
	case CABLE_TYPE_AC:
	case CABLE_TYPE_AUDIO_DOCK:
		value.intval = POWER_SUPPLY_TYPE_MAINS;
		break;
	case CABLE_TYPE_UARTOFF:
		value.intval = POWER_SUPPLY_TYPE_UARTOFF;
		break;
	case CABLE_TYPE_CARDOCK:
		value.intval = POWER_SUPPLY_TYPE_CARDOCK;
		break;
	case CABLE_TYPE_CDP:
		value.intval = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case CABLE_TYPE_INCOMPATIBLE:
		value.intval = POWER_SUPPLY_TYPE_MAINS;
		break;
	case CABLE_TYPE_DESK_DOCK:
		value.intval = POWER_SUPPLY_TYPE_DESK_DOCK;
		break;
	case CABLE_TYPE_NONE:
		value.intval = POWER_SUPPLY_TYPE_BATTERY;
	if (set_cable_status == CABLE_TYPE_UARTOFF)
	value.intval = POWER_SUPPLY_TYPE_UARTOFF;
		break;
	default:
                pr_err("%s: LPM boot with invalid cable :%d\n", __func__,set_cable_status);
	}

	rc = psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE,&value);

	if (rc) {
		pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
			__func__, rc);
        }

	return (int)set_cable_status;
}
//#endif
bool ovp_state;
EXPORT_SYMBOL(ovp_state);

void tsu6721_oxp_callback(int state)
{
//ovp stub-implemented on completion
	if (state == 1) {
		ovp_state = true;
		/*ENABLE*/
	} else if (state == 0) {
		ovp_state = false;
		/*DISABLE*/
	}
}

int tsu6721_dock_init(void)
{
	int ret;
	/* for CarDock, DeskDock */
	ret = switch_dev_register(&switch_dock);
	if (ret < 0) {
		pr_err("Failed to register dock switch. %d\n", ret);
		return ret;
	}
	return 0;
}
#if defined(DEBUG_STATUS)
static unsigned status_count;
#endif
void tsu6721_callback(enum cable_type_t cable_type, int attached)
{
	union power_supply_propval value;
	int i, ret = 0;
	struct power_supply *psy;
	static enum cable_type_t previous_cable_type = CABLE_TYPE_NONE;

	printk("%s, called \n",__func__);
#if defined(CONFIG_TOUCHSCREEN_MXTS) ||defined(CONFIG_TOUCHSCREEN_MXT224E)
        if (charger_callbacks && charger_callbacks->inform_charger)
                charger_callbacks->inform_charger(charger_callbacks,
                attached);
#endif

	if (cable_type == CABLE_TYPE_INCOMPATIBLE)
		cable_type = CABLE_TYPE_AC;
	set_cable_status = attached ? cable_type : CABLE_TYPE_NONE;

	switch (cable_type) {
	case CABLE_TYPE_USB:
		
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s USB Cable status attached (%u) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s USB Cable Status detached (%u) \n", __func__,status_count);
               }
#endif
		sec_otg_set_vbus_state(attached);
		break;
	case CABLE_TYPE_AC:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Charger status attached (%u) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s charger status detached (%u) \n", __func__,status_count);
               }
#endif
		break;
	case CABLE_TYPE_UARTOFF:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s UART Status attached (%u) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s UART status detached (%u) \n", __func__,status_count);
               }
#endif
		break;
	case CABLE_TYPE_JIG:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s JIG cable status attached (%u) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s JIG cable status detached (%u) \n", __func__,status_count);
               }
#endif
		return;
	case CABLE_TYPE_CDP:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s CDP status attached (%u) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s CDP Status detached (%u) \n", __func__,status_count);
               }
#endif
		break;
	case CABLE_TYPE_OTG:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s OTG status attached (%u) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s OTG status detached (%u) \n", __func__,status_count);
               }
#endif
	       return;
	case CABLE_TYPE_AUDIO_DOCK:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Audiodock status attached (%u) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s Audiodock status detached (%u) \n", __func__,status_count);
               }
#endif
		return;
	case CABLE_TYPE_CARDOCK:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Cardock status attached (%u) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s Cardock status detached (%u) \n", __func__,status_count);
               }
#endif
#ifndef CONFIG_MACH_MS01_LTE_KOR
		switch_set_state(&switch_dock, attached ? 2 : 0);
#endif
		break;
	case CABLE_TYPE_DESK_DOCK:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Deskdock status attached (%u) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s Deskdock status detached (%u) \n", __func__,status_count);
               }
#endif
		switch_set_state(&switch_dock, attached);
		break;
	case CABLE_TYPE_INCOMPATIBLE:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Incompatible Charger status attached (%u) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s Incomabtible Charger status detached (%u) \n", __func__,status_count);
               }
#endif
		break;
	default:
		break;
	}

	if (previous_cable_type == set_cable_status) {
		pr_info("%s: SKIP cable setting\n", __func__);
		return;
	}
	previous_cable_type = set_cable_status;

	for (i = 0; i < BATT_SEARCH_CNT_MAX; i++) {
		psy = power_supply_get_by_name("battery");
		if (psy)
			break;
	}
	if (i == BATT_SEARCH_CNT_MAX) {
		pr_err("%s: fail to get battery ps\n", __func__);
		return;
	}
	if (psy == NULL || psy->set_property == NULL) {
		pr_err("%s: battery ps doesn't support set_property()\n",
				__func__);
		return;
	}

	switch (set_cable_status) {
	case CABLE_TYPE_MISC:
		value.intval = POWER_SUPPLY_TYPE_MISC;
		break;
	case CABLE_TYPE_USB:
		value.intval = POWER_SUPPLY_TYPE_USB;
		break;
	case CABLE_TYPE_AC:
	case CABLE_TYPE_AUDIO_DOCK:
		value.intval = POWER_SUPPLY_TYPE_MAINS;
		break;
	case CABLE_TYPE_UARTOFF:
		value.intval = POWER_SUPPLY_TYPE_UARTOFF;
		break;
	case CABLE_TYPE_CARDOCK:
		value.intval = POWER_SUPPLY_TYPE_CARDOCK;
		break;
	case CABLE_TYPE_CDP:
		value.intval = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case CABLE_TYPE_INCOMPATIBLE:
		value.intval = POWER_SUPPLY_TYPE_MAINS;
		break;
	case CABLE_TYPE_DESK_DOCK:
		value.intval = POWER_SUPPLY_TYPE_DESK_DOCK;
		break;
	case CABLE_TYPE_NONE:
		value.intval = POWER_SUPPLY_TYPE_BATTERY;
		break;
	default:
		pr_err("%s: invalid cable :%d\n", __func__, set_cable_status);
		return;
	}
#if defined(CONFIG_MACH_CRATERTD_CHN_3G)
	current_cable_type = value.intval;
#endif

	ret = psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	if (ret) {
                pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
                        __func__, ret);
        }
}

struct tsu6721_platform_data tsu6721_pdata = {
	.callback = tsu6721_callback,
	.dock_init = tsu6721_dock_init,
	.oxp_callback = tsu6721_oxp_callback,
	.mhl_sel = NULL,

};

void muic_callback(enum cable_type_t cable_type, int state)
{
	set_cable_status = state ? cable_type : CABLE_TYPE_NONE;
		switch (cable_type) {
	case CABLE_TYPE_USB:
		pr_info("%s USB Cable is %s\n",
			__func__, state ? "attached" : "detached");
		break;
	case CABLE_TYPE_AC:
		pr_info("%s Charger is %s\n",
			__func__, state ? "attached" : "detached");
		break;
	case CABLE_TYPE_UARTOFF:
		pr_info("%s Uart is %s\n",
			__func__, state ? "attached" : "detached");
		break;
	case CABLE_TYPE_JIG:
		pr_info("%s Jig is %s\n",
			__func__, state ? "attached" : "detached");
		return;
	case CABLE_TYPE_CDP:
		pr_info("%s USB CDP is %s\n",
			__func__, state ? "attached" : "detached");
		break;
	case CABLE_TYPE_OTG:
		pr_info("%s OTG is %s\n",
			__func__, state ? "attached" : "detached");
		return;
	case CABLE_TYPE_AUDIO_DOCK:
		pr_info("%s Audiodock is %s\n",
			__func__, state ? "attached" : "detached");
		return;
	case CABLE_TYPE_CARDOCK:
		pr_info("%s Cardock is %s\n",
			__func__, state ? "attached" : "detached");
		break;
	case CABLE_TYPE_INCOMPATIBLE:
		pr_info("%s Incompatible Charger is %s\n",
			__func__, state ? "attached" : "detached");
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(muic_callback);

#endif

#if defined(CONFIG_USB_SWITCH_RT8973)
//extern sec_battery_platform_data_t sec_battery_pdata;

//unsigned int lpcharge;
//EXPORT_SYMBOL(lpcharge);

int current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
//EXPORT_SYMBOL(current_cable_type);

u8 attached_cable;

int rt8973_dock_init(void)
{
	int ret;
	/* for CarDock, DeskDock */
	ret = switch_dev_register(&switch_dock);
	if (ret < 0) {
		pr_err("Failed to register dock switch. %d\n", ret);
		return ret;
	}
	return 0;
}

void sec_charger_cb(u8 cable_type)
{
	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");
    pr_info("%s: cable type (0x%02x)\n", __func__, cable_type);
	attached_cable = cable_type;

	switch (cable_type) {
	case MUIC_RT8973_CABLE_TYPE_NONE:
	case MUIC_RT8973_CABLE_TYPE_UNKNOWN:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		set_cable_status = CABLE_TYPE_NONE;
		break;
	case MUIC_RT8973_CABLE_TYPE_USB:
	case MUIC_RT8973_CABLE_TYPE_CDP:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		set_cable_status = CABLE_TYPE_USB;
		break;
	case MUIC_RT8973_CABLE_TYPE_REGULAR_TA:
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		set_cable_status = CABLE_TYPE_AC;
		break;
	case MUIC_RT8973_CABLE_TYPE_OTG:
		goto skip;
	case MUIC_RT8973_CABLE_TYPE_JIG_UART_OFF:
	/*
		if (!gpio_get_value(mfp_to_gpio(GPIO008_GPIO_8))) {
			pr_info("%s cable type POWER_SUPPLY_TYPE_UARTOFF\n", __func__);
			current_cable_type = POWER_SUPPLY_TYPE_UARTOFF;
		}
		else {
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		}*/
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		set_cable_status = CABLE_TYPE_NONE;
		break;
	case MUIC_RT8973_CABLE_TYPE_JIG_USB_ON:
	case MUIC_RT8973_CABLE_TYPE_JIG_USB_OFF:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		set_cable_status = CABLE_TYPE_USB;
		break;
	case MUIC_RT8973_CABLE_TYPE_0x1A:
	case MUIC_RT8973_CABLE_TYPE_TYPE1_CHARGER:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		set_cable_status = CABLE_TYPE_USB;
		break;
	case MUIC_RT8973_CABLE_TYPE_0x15:
		current_cable_type = POWER_SUPPLY_TYPE_MISC;
		set_cable_status = CABLE_TYPE_MISC;
		break;
	case MUIC_RT8973_CABLE_TYPE_ATT_TA:
		current_cable_type = POWER_SUPPLY_TYPE_MISC;
		set_cable_status = CABLE_TYPE_MISC;
		break;
	case MUIC_RT8973_CABLE_TYPE_JIG_UART_OFF_WITH_VBUS:
		current_cable_type = POWER_SUPPLY_TYPE_UARTOFF;
		set_cable_status = CABLE_TYPE_UARTOFF;
		break;
	default:
		pr_err("%s: invalid type for charger:%d\n",
			__func__, cable_type);
		current_cable_type = POWER_SUPPLY_TYPE_UNKNOWN;
		set_cable_status = CABLE_TYPE_NONE;
		goto skip;
	}

	if (!psy || !psy->set_property)
		pr_err("%s: fail to get battery psy\n", __func__);
	else {
		value.intval = current_cable_type;
		psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	}

skip:
	return;
}
EXPORT_SYMBOL(sec_charger_cb);

void rt8973_jig_callback(jig_type_t type, uint8_t attached)
{
	if (type == JIG_UART_BOOT_ON) {
		if (attached)
			pr_err("%s Cardock status attached \n",__func__);
		else
			pr_err("%s Cardock status detached \n", __func__);

		switch_set_state(&switch_dock, attached ? 2 : 0);			
	}
	return;
}

void rt8973_usb_cb(uint8_t attached) {
	pr_info("%s USB Cable is %s\n",
		__func__, attached ? "attached" : "detached");
	sec_otg_set_vbus_state(attached);
}

struct rt8973_platform_data  rt8973_pdata = {
    .irq_gpio = 82,
    .cable_chg_callback = NULL,
    .ocp_callback = NULL,
    .otp_callback = NULL,
    .ovp_callback = NULL,
    .usb_callback = rt8973_usb_cb,
    .uart_callback = NULL,
    .otg_callback = NULL,
    .jig_callback = rt8973_jig_callback,

};

/*static struct i2c_board_info rtmuic_i2c_boardinfo[] __initdata = {

    {
        I2C_BOARD_INFO("rt8973", 0x28>>1),
        .platform_data = &rt8973_pdata,
    },

};*/

//////////////////////////////////////////////////////////////////////////////////////////////////

#endif

#if defined(CONFIG_SM5502_MUIC)
void sm5502_oxp_callback(int state)
{
#if 0 //ovp stub-implemented on completion
	bool ovp_state;
	if (state == 1) {
		ovp_state = true;
		/*ENABLE*/
	} else if (state == 0) {
		ovp_state = false;
		/*DISABLE*/
	}
#endif
}

int sm5502_dock_init(void)
{
	int ret;
	/* for CarDock, DeskDock */
	ret = switch_dev_register(&switch_dock);
	if (ret < 0) {
		pr_err("Failed to register dock switch. %d\n", ret);
		return ret;
	}
	return 0;
}
#if defined(DEBUG_STATUS)
static unsigned status_count;
#endif
void sm5502_callback(enum cable_type_t cable_type, int attached)
{
	union power_supply_propval value;
	int i, ret = 0;
	struct power_supply *psy;
	static enum cable_type_t previous_cable_type = CABLE_TYPE_NONE;

	printk("%s, called \n",__func__);
#if defined(CONFIG_TOUCHSCREEN_MXTS) ||defined(CONFIG_TOUCHSCREEN_MXT224E)
        if (charger_callbacks && charger_callbacks->inform_charger)
                charger_callbacks->inform_charger(charger_callbacks,
                attached);
#endif

	if (cable_type == CABLE_TYPE_INCOMPATIBLE)
		cable_type = CABLE_TYPE_AC;
	set_cable_status = attached ? cable_type : CABLE_TYPE_NONE;

	switch (cable_type) {
	case CABLE_TYPE_USB:

#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s USB Cable status attached (%u) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s USB Cable Status detached (%u) \n", __func__,status_count);
               }
#endif
		sec_otg_set_vbus_state(attached);
		break;
	case CABLE_TYPE_AC:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Charger status attached (%u) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s charger status detached (%u) \n", __func__,status_count);
               }
#endif
		break;
	case CABLE_TYPE_UARTOFF:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s UART Status attached (%u) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s UART status detached (%u) \n", __func__,status_count);
               }
#endif
		break;
	case CABLE_TYPE_JIG:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s JIG cable status attached (%u) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s JIG cable status detached (%u) \n", __func__,status_count);
               }
#endif
		return;
	case CABLE_TYPE_CDP:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s CDP status attached (%u) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s CDP Status detached (%u) \n", __func__,status_count);
               }
#endif
		break;
	case CABLE_TYPE_OTG:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s OTG status attached (%u) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s OTG status detached (%u) \n", __func__,status_count);
               }
#endif
	       return;
	case CABLE_TYPE_AUDIO_DOCK:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Audiodock status attached (%u) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s Audiodock status detached (%u) \n", __func__,status_count);
               }
#endif
		return;
	case CABLE_TYPE_CARDOCK:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Cardock status attached (%u) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s Cardock status detached (%u) \n", __func__,status_count);
               }
#endif
		switch_set_state(&switch_dock, attached ? 2 : 0);
		break;
	case CABLE_TYPE_DESK_DOCK:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Deskdock status attached (%u) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s Deskdock status detached (%u) \n", __func__,status_count);
               }
#endif
		switch_set_state(&switch_dock, attached);
		break;
	case CABLE_TYPE_INCOMPATIBLE:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Incompatible Charger status attached (%u) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s Incomabtible Charger status detached (%u) \n", __func__,status_count);
               }
#endif
		break;
	default:
		break;
	}

	if (previous_cable_type == set_cable_status) {
		pr_info("%s: SKIP cable setting\n", __func__);
		return;
	}
	previous_cable_type = set_cable_status;

	for (i = 0; i < BATT_SEARCH_CNT_MAX; i++) {
		psy = power_supply_get_by_name("battery");
		if (psy)
			break;
	}
	if (i == BATT_SEARCH_CNT_MAX) {
		pr_err("%s: fail to get battery ps\n", __func__);
		return;
	}
	if (psy == NULL || psy->set_property == NULL) {
		pr_err("%s: battery ps doesn't support set_property()\n",
				__func__);
		return;
	}

	switch (set_cable_status) {
	case CABLE_TYPE_MISC:
		value.intval = POWER_SUPPLY_TYPE_MISC;
		break;
	case CABLE_TYPE_USB:
		value.intval = POWER_SUPPLY_TYPE_USB;
		break;
	case CABLE_TYPE_AC:
	case CABLE_TYPE_AUDIO_DOCK:
		value.intval = POWER_SUPPLY_TYPE_MAINS;
		break;
	case CABLE_TYPE_UARTOFF:
		value.intval = POWER_SUPPLY_TYPE_UARTOFF;
		break;
	case CABLE_TYPE_CARDOCK:
		value.intval = POWER_SUPPLY_TYPE_CARDOCK;
		break;
	case CABLE_TYPE_CDP:
		value.intval = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case CABLE_TYPE_INCOMPATIBLE:
		value.intval = POWER_SUPPLY_TYPE_MAINS;
		break;
	case CABLE_TYPE_DESK_DOCK:
		value.intval = POWER_SUPPLY_TYPE_DESK_DOCK;
		break;
	case CABLE_TYPE_NONE:
		value.intval = POWER_SUPPLY_TYPE_BATTERY;
		break;
	default:
		pr_err("%s: invalid cable :%d\n", __func__, set_cable_status);
		return;
	}
#if defined(CONFIG_MACH_CRATERTD_CHN_3G)
	current_cable_type = value.intval;
#endif

	ret = psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	if (ret) {
                pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
                        __func__, ret);
        }
}

struct sm5502_platform_data sm5502_pdata = {
	.callback = sm5502_callback,
	.dock_init = sm5502_dock_init,
	.oxp_callback = sm5502_oxp_callback,
	.mhl_sel = NULL,

};
#endif

