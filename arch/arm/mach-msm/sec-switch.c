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
#include <linux/power_supply.h>
#include <linux/usb/gadget.h>
#include <linux/switch.h>
#include <linux/sec_class.h>
#include <linux/pm_runtime.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/host_notify.h>
#endif
#if defined(CONFIG_SM5502_MUIC)
#include <linux/i2c/sm5502.h>
#define DEBUG_STATUS	1
#endif
#if defined(CONFIG_SM5504_MUIC)
#include <linux/i2c/sm5504.h>
#define DEBUG_STATUS    1
#endif
#define BATT_SEARCH_CNT_MAX	10
#if defined (CONFIG_VIDEO_MHL_V2)
static int MHL_Connected;
#endif
#if defined(CONFIG_USB_SWITCH_TSU6721) || defined(CONFIG_USB_SWITCH_RT8973)
#include <linux/i2c/tsu6721.h>
#define DEBUG_STATUS			1
#endif
#if defined(CONFIG_USB_SWITCH_RT8973)
#include <linux/platform_data/rt8973.h>
#endif

#ifdef CONFIG_TOUCHSCREEN_MMS300
#include <linux/i2c/mms300.h>
#endif

#ifdef CONFIG_TOUCHSCREEN_ZINITIX_BT541
#include <linux/i2c/zinitix_i2c.h>
#endif

#ifdef CONFIG_TOUCHSCREEN_CYTTSP4
#include <linux/i2c/cyttsp4_core.h>
#endif

#if defined(CONFIG_USB_SWITCH_TSU6721) || defined(CONFIG_USB_SWITCH_RT8973)
#include <linux/switch.h>

int current_cable_type = POWER_SUPPLY_TYPE_BATTERY;

struct switch_dev switch_dock = {
      .name = "dock",
};

struct device *switch_dev;
EXPORT_SYMBOL(switch_dev);
EXPORT_SYMBOL(switch_dock);

extern void sec_otg_set_vbus_state(int);

static enum cable_type_t set_cable_status;
int msm8930_get_cable_status(void) {return (int)set_cable_status; }

/* support for LPM charging */
//#ifdef CONFIG_SAMSUNG_LPM_MODE
#if defined(CONFIG_BATTERY_SAMSUNG) || defined(CONFIG_QPNP_SEC_CHARGER)
bool sec_bat_is_lpm(void)
{
	return (bool)poweroff_charging; 
}
#endif

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
		value.intval = POWER_SUPPLY_TYPE_MISC;
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

void tsu6721_oxp_callback(int state)
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
static int status_count;
#endif
void tsu6721_callback(enum cable_type_t cable_type, int attached)
{
	union power_supply_propval value;
	int i, ret = 0;
	//Initialize power supply to battery, till power supply decided.
	struct power_supply *psy = power_supply_get_by_name("battery");
	static enum cable_type_t previous_cable_type = CABLE_TYPE_NONE;

	pr_info("%s, called : cable_type :%d \n",__func__, cable_type);
#if defined(CONFIG_TOUCHSCREEN_MXTS) ||defined(CONFIG_TOUCHSCREEN_MXT224E)
        if (charger_callbacks && charger_callbacks->inform_charger)
                charger_callbacks->inform_charger(charger_callbacks,
                attached);
#endif

	set_cable_status = attached ? cable_type : CABLE_TYPE_NONE;

	switch (cable_type) {
	case CABLE_TYPE_USB:

#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s USB Cable status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s USB Cable Status detached (%d) \n", __func__,status_count);
               }
#endif
		sec_otg_set_vbus_state(attached);
		break;
	case CABLE_TYPE_AC:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Charger status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s charger status detached (%d) \n", __func__,status_count);
               }
#endif
		break;
	case CABLE_TYPE_UARTOFF:
	case CABLE_TYPE_JIG_UART_OFF_VB:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s UART Status attached (%d), VBUS: %s\n",__func__, 
					status_count,((cable_type == CABLE_TYPE_UARTOFF ? "No": "Yes")));
               } else {
                       status_count = status_count-1;
                       pr_err("%s UART status detached (%d), VBUS: %s\n", __func__,
					status_count,((cable_type == CABLE_TYPE_UARTOFF ? "No": "Yes")));
               }
#endif
		break;
	case CABLE_TYPE_JIG:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s JIG cable status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s JIG cable status detached (%d) \n", __func__,status_count);
               }
#endif
		return;
	case CABLE_TYPE_CDP:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s CDP status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s CDP Status detached (%d) \n", __func__,status_count);
               }
#endif
		sec_otg_set_vbus_state(attached);
		break;
	case CABLE_TYPE_OTG:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s OTG status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s OTG status detached (%d) \n", __func__,status_count);
               }
#endif
#if defined(CONFIG_USB_HOST_NOTIFY)
		sec_otg_notify(attached ? HNOTIFY_ID : HNOTIFY_ID_PULL);
#endif

	       return;
	case CABLE_TYPE_AUDIO_DOCK:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Audiodock status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s Audiodock status detached (%d) \n", __func__,status_count);
               }
#endif
		return;
	case CABLE_TYPE_CARDOCK:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Cardock status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s Cardock status detached (%d) \n", __func__,status_count);
               }
#endif
		switch_set_state(&switch_dock, attached ? 2 : 0);
		break;
	case CABLE_TYPE_DESK_DOCK:
	case CABLE_TYPE_DESK_DOCK_NO_VB:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Deskdock %s status attached (%d) \n",__func__,
				((cable_type == CABLE_TYPE_DESK_DOCK)? "VBUS" : "NO Vbus"),status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s Deskdock %s status detached (%d) \n", __func__,
				((cable_type == CABLE_TYPE_DESK_DOCK)? "VBUS" : "NO Vbus"),status_count);
               }
#endif
		//skip set state for Just TA detach-with target docked.
		if (cable_type != CABLE_TYPE_DESK_DOCK_NO_VB)
			switch_set_state(&switch_dock, attached);
		break;
	case CABLE_TYPE_INCOMPATIBLE:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Incompatible Charger status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s Incomabtible Charger status detached (%d) \n", __func__,status_count);
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
	case CABLE_TYPE_CARDOCK:
		value.intval = POWER_SUPPLY_TYPE_CARDOCK;
		break;
	case CABLE_TYPE_CDP:
		value.intval = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case CABLE_TYPE_INCOMPATIBLE:
		value.intval = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	case CABLE_TYPE_DESK_DOCK:
		value.intval = POWER_SUPPLY_TYPE_MISC;
		break;
        case CABLE_TYPE_JIG_UART_OFF_VB:
                value.intval = POWER_SUPPLY_TYPE_UARTOFF;
                break;
	case CABLE_TYPE_JIG:
	case CABLE_TYPE_DESK_DOCK_NO_VB:
	case CABLE_TYPE_UARTOFF:
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
	pr_info("%s setting cable type(%d)\n",__func__, value.intval);

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

//int current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
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

#ifdef MUIC_SUPPORT_CARDOCK_FUNCTION
void rt8973_jig_callback(jig_type_t type, uint8_t attached)
{
	//Check JIG cable type and whether dock finished the initialization or not (rt8973_dock_init)
	if (type == JIG_UART_BOOT_ON && switch_dock.dev != NULL) {
		if (attached)
			pr_err("%s Cardock status attached \n",__func__);
		else
			pr_err("%s Cardock status detached \n", __func__);

		switch_set_state(&switch_dock, attached ? 2 : 0);
	}
	return;
}
#endif

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
	case MUIC_RT8973_CABLE_TYPE_L200K_SPEC_USB:
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

		break;
	case MUIC_RT8973_CABLE_TYPE_JIG_USB_ON:
	case MUIC_RT8973_CABLE_TYPE_JIG_USB_OFF:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
        set_cable_status = CABLE_TYPE_USB;
		break;
	case MUIC_RT8973_CABLE_TYPE_0x1A:
	case MUIC_RT8973_CABLE_TYPE_TYPE1_CHARGER:
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
        set_cable_status = CABLE_TYPE_AC;
		break;
	case MUIC_RT8973_CABLE_TYPE_0x15:
		current_cable_type = POWER_SUPPLY_TYPE_MISC;
        set_cable_status = CABLE_TYPE_AC;
		break;
	case MUIC_RT8973_CABLE_TYPE_ATT_TA:
		current_cable_type = POWER_SUPPLY_TYPE_MISC;
        set_cable_status = CABLE_TYPE_AC;
		break;
	case MUIC_RT8973_CABLE_TYPE_JIG_UART_OFF_WITH_VBUS:
		current_cable_type = POWER_SUPPLY_TYPE_UARTOFF;
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

void rt8973_usb_cb(uint8_t attached) {
	pr_info("%s USB Cable is %s\n",
		__func__, attached ? "attached" : "detached");
	sec_otg_set_vbus_state(attached);
}
#ifdef CONFIG_USB_HOST_NOTIFY
void rt8973_otg_cb(uint8_t attached)
{
	pr_info("rt8973_otg_cb attached %d\n", attached);
	current_cable_type = POWER_SUPPLY_TYPE_OTG;

	if (attached) {
		pr_info("%s USB Host attached", __func__);
		sec_otg_notify(HNOTIFY_ID);
	} else {
		pr_info("%s USB Host detached", __func__);
		sec_otg_notify(HNOTIFY_ID_PULL);
	}
}
#endif
struct rt8973_platform_data  rt8973_pdata = {
#ifdef CONFIG_MACH_KANAS3G_CTC
    .irq_gpio = 83,
#else
    .irq_gpio = 82,
#endif
    .cable_chg_callback = NULL,
    .ocp_callback = NULL,
    .otp_callback = NULL,
    .ovp_callback = NULL,
    .usb_callback = rt8973_usb_cb,
    .uart_callback = NULL,
#ifdef CONFIG_USB_HOST_NOTIFY
    .otg_callback = rt8973_otg_cb,
#else
    .otg_callback = NULL,
#endif
    .dock_init = rt8973_dock_init,
#ifdef MUIC_SUPPORT_CARDOCK_FUNCTION
    .jig_callback = rt8973_jig_callback,
#else
    .jig_callback = NULL,
#endif
};

/*static struct i2c_board_info rtmuic_i2c_boardinfo[] __initdata = {

    {
        I2C_BOARD_INFO("rt8973", 0x28>>1),
        .platform_data = &rt8973_pdata,
    },

};*/

//////////////////////////////////////////////////////////////////////////////////////////////////
#endif

#if defined(CONFIG_SM5502_MUIC) || defined(CONFIG_SM5504_MUIC)
#ifndef CONFIG_USB_HOST_NOTIFY
/* Dummy callback Function to handle the OTG Test case*/
int sec_get_notification(int mode)
{
	return 1;
}
#endif
extern void sec_otg_set_vbus_state(int);

static enum cable_type_t set_cable_status;

int current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
static struct switch_dev switch_dock = {
		.name = "dock",
		};
struct device *switch_dev;
EXPORT_SYMBOL(switch_dev);

/* support for the LPM Charging*/
#if defined(CONFIG_BATTERY_SAMSUNG)
bool sec_bat_is_lpm(void)
{
	return (bool)poweroff_charging; 
}
#endif
#endif
#if defined(CONFIG_SM5502_MUIC)
/* callbacks & Handlers for the SM5502 MUIC*/
#if defined(CONFIG_QPNP_BMS)
extern int check_sm5502_jig_state(void);
#endif

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
static int status_count;
#endif

#if defined(CONFIG_MUIC_SM5502_SUPPORT_LANHUB_TA)
void sm5502_lanhub_callback(enum cable_type_t cable_type, int attached, bool lanhub_ta)
{
	union power_supply_propval value;
	struct power_supply *psy;
	int i, ret = 0;

	pr_info("SM5502 Lanhub Callback called, cable %d, attached %d, TA: %s \n",
				cable_type,attached,(lanhub_ta ? "Yes":"No"));
	if(lanhub_ta)
		set_cable_status = attached ? CABLE_TYPE_LANHUB : POWER_SUPPLY_TYPE_OTG;
	else
		set_cable_status = attached ? CABLE_TYPE_LANHUB : CABLE_TYPE_NONE;

	pr_info("%s:sm5502 cable type : %d", __func__, set_cable_status);

	for (i = 0; i < 10; i++) {
		psy = power_supply_get_by_name("battery");
		if (psy)
			break;
	}

	if (i == 10) {
		pr_err("%s: fail to get battery ps\n", __func__);
		return;
	}

	if (!psy || !psy->set_property)
                pr_err("%s: fail to set battery psy\n", __func__);
        else {
		switch (set_cable_status) {
		case CABLE_TYPE_LANHUB:
			value.intval = POWER_SUPPLY_TYPE_LAN_HUB;
			break;
	        case POWER_SUPPLY_TYPE_OTG:
	        case CABLE_TYPE_NONE:
			value.intval = lanhub_ta ? POWER_SUPPLY_TYPE_OTG : POWER_SUPPLY_TYPE_BATTERY;
			break;
		default:
	                pr_err("%s invalid status:%d\n", __func__,attached);
	                return;
		}

	        ret = psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
		if (ret) {
			pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
				__func__, ret);
	        }

	}

#ifdef CONFIG_USB_HOST_NOTIFY
	if (attached){
		if(lanhub_ta) {
			pr_info("USB Host HNOTIFY LANHUB_N_TA_ON\n");
		}
		else{
			pr_info("USB Host HNOTIFY_LANHUB_ON\n");
			sec_otg_notify(HNOTIFY_LANHUB_ON);
		}
	}else{
		if(lanhub_ta) {
                        pr_info("USB Host HNOTIFY LANHUB ON\n");
                }
                else{
			pr_info("USB Host HNOTIFY_LANHUB_OFF");
			sec_otg_notify(HNOTIFY_LANHUB_OFF);
		}
	}
#endif

}
#endif

#if defined(CONFIG_TOUCHSCREEN_ZINITIX_BT541) || defined(USE_TSP_TA_CALLBACKS)
struct tsp_callbacks *charger_callbacks;
#endif

void sm5502_callback(enum cable_type_t cable_type, int attached)
{
	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");
	static enum cable_type_t previous_cable_type = CABLE_TYPE_NONE;
	pr_info("%s, called : cable_type :%d \n",__func__, cable_type);

	set_cable_status = attached ? cable_type : CABLE_TYPE_NONE;

	switch (cable_type) {
	case CABLE_TYPE_USB:
#if defined(CONFIG_TOUCHSCREEN_ZINITIX_BT541) || defined(USE_TSP_TA_CALLBACKS)
		if (charger_callbacks && charger_callbacks->inform_charger)
			charger_callbacks->inform_charger(charger_callbacks,
			attached);
#endif
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s USB Cable status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s USB Cable Status detached (%d) \n", __func__,status_count);
               }
#endif
		sec_otg_set_vbus_state(attached);
		break;
	case CABLE_TYPE_AC:
#if defined(CONFIG_TOUCHSCREEN_ZINITIX_BT541) || defined(USE_TSP_TA_CALLBACKS)
		if (charger_callbacks && charger_callbacks->inform_charger)
			charger_callbacks->inform_charger(charger_callbacks,
			attached);
#endif
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Charger status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s charger status detached (%d) \n", __func__,status_count);
               }
#endif
		break;
	case CABLE_TYPE_UARTOFF:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s UART Status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s UART status detached (%d) \n", __func__,status_count);
               }
#endif
		break;
	case CABLE_TYPE_JIG_UART_OFF_VB:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s UART OFF VBUS Status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s UART OFF VBUS status detached (%d) \n", __func__,status_count);
               }
#endif
		break;
	case CABLE_TYPE_JIG:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s JIG cable status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s JIG cable status detached (%d) \n", __func__,status_count);
               }
#endif
		return;
	case CABLE_TYPE_CDP:
#if defined(CONFIG_TOUCHSCREEN_ZINITIX_BT541) || defined(USE_TSP_TA_CALLBACKS)
		if (charger_callbacks && charger_callbacks->inform_charger)
			charger_callbacks->inform_charger(charger_callbacks,
			attached);
#endif
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s CDP status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s CDP Status detached (%d) \n", __func__,status_count);
               }
#endif
		sec_otg_set_vbus_state(attached);
		break;
	case CABLE_TYPE_OTG:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s OTG status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s OTG status detached (%d) \n", __func__,status_count);
               }
#endif
#if defined(CONFIG_USB_HOST_NOTIFY)
		sec_otg_notify(attached ? HNOTIFY_ID : HNOTIFY_ID_PULL);
#endif
	       return;
	case CABLE_TYPE_AUDIO_DOCK:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Audiodock status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s Audiodock status detached (%d) \n", __func__,status_count);
               }
#endif
		return;
	case CABLE_TYPE_CARDOCK:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Cardock status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s Cardock status detached (%d) \n", __func__,status_count);
               }
#endif
		switch_set_state(&switch_dock, attached ? 2 : 0);
		break;
	case CABLE_TYPE_DESK_DOCK:
	case CABLE_TYPE_DESK_DOCK_NO_VB:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Deskdock %s status attached (%d) \n",__func__,
				((cable_type == CABLE_TYPE_DESK_DOCK)? "VBUS" : "NOVBUS"),status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s Deskdock %s status detached (%d) \n", __func__,
				((cable_type == CABLE_TYPE_DESK_DOCK)? "VBUS" : "NOVBUS"),status_count);
               }
#endif
		switch_set_state(&switch_dock, attached);
		break;
	case CABLE_TYPE_219KUSB:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s 219K USB status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s 219K USB status detached (%d) \n", __func__,status_count);
               }
#endif
		sec_otg_set_vbus_state(attached);
		break;
	case CABLE_TYPE_INCOMPATIBLE:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Incompatible Charger status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s Incomabtible Charger status detached (%d) \n", __func__,status_count);
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

#if defined(CONFIG_FUELGAUGE_MAX17050)
	if(check_sm5502_jig_state())
	{
		struct power_supply *fuel_psy = power_supply_get_by_name("sec-fuelgauge");
		if (!fuel_psy || !fuel_psy->set_property)
			pr_err("%s: fail to get sec-fuelgauge psy\n", __func__);
		else {
			fuel_psy->set_property(fuel_psy, POWER_SUPPLY_PROP_CHARGE_TYPE, &value);
		}
	}
#endif
#if defined(CONFIG_QPNP_BMS)
	if(check_sm5502_jig_state())
	{
	  struct power_supply *fuel_psy = power_supply_get_by_name("bms");
	  if (!fuel_psy || !fuel_psy->set_property)
		pr_err("%s: fail to get BMS psy\n", __func__);
		else {
			fuel_psy->set_property(fuel_psy, POWER_SUPPLY_PROP_CHARGE_TYPE, &value);
		}
	}
#endif

	switch (set_cable_status) {
	case CABLE_TYPE_MISC:
		value.intval = POWER_SUPPLY_TYPE_MISC;
		break;
	case CABLE_TYPE_USB:
		value.intval = POWER_SUPPLY_TYPE_USB;
		break;
	case CABLE_TYPE_219KUSB:
	case CABLE_TYPE_AC:
	case CABLE_TYPE_AUDIO_DOCK:
		value.intval = POWER_SUPPLY_TYPE_MAINS;
		break;
	case CABLE_TYPE_CARDOCK:
		value.intval = POWER_SUPPLY_TYPE_CARDOCK;
		break;
	case CABLE_TYPE_CDP:
		value.intval = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case CABLE_TYPE_INCOMPATIBLE:
		value.intval = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	case CABLE_TYPE_DESK_DOCK:
		value.intval = POWER_SUPPLY_TYPE_MAINS;
		break;
        case CABLE_TYPE_JIG_UART_OFF_VB:
                value.intval = POWER_SUPPLY_TYPE_UARTOFF;
                break;
	case CABLE_TYPE_DESK_DOCK_NO_VB:
	case CABLE_TYPE_UARTOFF:
	case CABLE_TYPE_NONE:
		value.intval = POWER_SUPPLY_TYPE_BATTERY;
		break;
	default:
		pr_err("%s: invalid cable :%d\n", __func__, set_cable_status);
		return;
	}
	current_cable_type = value.intval;
	pr_info("%s:MUIC setting the cable type as (%d)\n",__func__,value.intval);
	if (!psy || !psy->set_property)
		pr_err("%s: fail to get battery psy\n", __func__);
	else {
		psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	}
}

struct sm5502_platform_data sm5502_pdata = {
	.callback = sm5502_callback,
#if defined(CONFIG_MUIC_SM5502_SUPPORT_LANHUB_TA)
	.lanhub_cb = sm5502_lanhub_callback,
#endif
	.dock_init = sm5502_dock_init,
	.oxp_callback = sm5502_oxp_callback,
	.mhl_sel = NULL,

};
#endif /*End of SM5502 MUIC Callbacks*/

#if defined(CONFIG_SM5504_MUIC)
/* callbacks & Handlers for the SM5504 MUIC*/
/* support for the LPM Charging*/
#if defined(CONFIG_QPNP_BMS)
extern int check_sm5504_jig_state(void);
#endif

void sm5504_oxp_callback(int state)
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

int sm5504_dock_init(void)
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
static int status_count;
#endif

#if defined(CONFIG_MUIC_SM5504_SUPPORT_LANHUB_TA)
void sm5504_lanhub_callback(enum cable_type_t cable_type, int attached, bool lanhub_ta)
{
	union power_supply_propval value;
	struct power_supply *psy;
	int i, ret = 0;

	pr_info("SM5504 Lanhub Callback called, cable %d, attached %d, TA: %s \n",
				cable_type,attached,(lanhub_ta ? "Yes":"No"));
	if(lanhub_ta)
		set_cable_status = attached ? CABLE_TYPE_LANHUB : POWER_SUPPLY_TYPE_OTG;
	else
		set_cable_status = attached ? CABLE_TYPE_LANHUB : CABLE_TYPE_NONE;

	pr_info("%s:sm5504 cable type : %d", __func__, set_cable_status);

	for (i = 0; i < 10; i++) {
		psy = power_supply_get_by_name("battery");
		if (psy)
			break;
	}

	if (i == 10) {
		pr_err("%s: fail to get battery ps\n", __func__);
		return;
	}

	if (!psy || !psy->set_property)
                pr_err("%s: fail to set battery psy\n", __func__);
        else {
		switch (set_cable_status) {
		case CABLE_TYPE_LANHUB:
			value.intval = POWER_SUPPLY_TYPE_LAN_HUB;
			break;
	        case POWER_SUPPLY_TYPE_OTG:
	        case CABLE_TYPE_NONE:
			value.intval = lanhub_ta ? POWER_SUPPLY_TYPE_OTG : POWER_SUPPLY_TYPE_BATTERY;
			break;
		default:
	                pr_err("%s invalid status:%d\n", __func__,attached);
	                return;
		}

	        ret = psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
		if (ret) {
			pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
				__func__, ret);
	        }

	}

#ifdef CONFIG_USB_HOST_NOTIFY
	if (attached){
		if(lanhub_ta) {
			pr_info("USB Host HNOTIFY LANHUB_N_TA_ON\n");
		}
		else{
			pr_info("USB Host HNOTIFY_LANHUB_ON\n");
			sec_otg_notify(HNOTIFY_LANHUB_ON);
		}
	}else{
		if(lanhub_ta) {
                        pr_info("USB Host HNOTIFY LANHUB ON\n");
                }
                else{
			pr_info("USB Host HNOTIFY_LANHUB_OFF");
			sec_otg_notify(HNOTIFY_LANHUB_OFF);
		}
	}
#endif

}
#endif

#if defined(CONFIG_TOUCHSCREEN_ZINITIX_BT541) || defined(USE_TSP_TA_CALLBACKS)
struct tsp_callbacks *charger_callbacks;
#endif

void sm5504_callback(enum cable_type_t cable_type, int attached)
{
	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");
	static enum cable_type_t previous_cable_type = CABLE_TYPE_NONE;
	pr_info("%s, called : cable_type :%d \n",__func__, cable_type);

	set_cable_status = attached ? cable_type : CABLE_TYPE_NONE;

	switch (cable_type) {
	case CABLE_TYPE_USB:
#if defined(CONFIG_TOUCHSCREEN_ZINITIX_BT541) || defined(USE_TSP_TA_CALLBACKS)
		if (charger_callbacks && charger_callbacks->inform_charger)
			charger_callbacks->inform_charger(charger_callbacks,
			attached);
#endif
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s USB Cable status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s USB Cable Status detached (%d) \n", __func__,status_count);
               }
#endif
		sec_otg_set_vbus_state(attached);
		break;
	case CABLE_TYPE_AC:
#if defined(CONFIG_TOUCHSCREEN_ZINITIX_BT541) || defined(USE_TSP_TA_CALLBACKS)
		if (charger_callbacks && charger_callbacks->inform_charger)
			charger_callbacks->inform_charger(charger_callbacks,
			attached);
#endif
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Charger status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s charger status detached (%d) \n", __func__,status_count);
               }
#endif
		break;
	case CABLE_TYPE_UARTOFF:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s UART Status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s UART status detached (%d) \n", __func__,status_count);
               }
#endif
		break;
	case CABLE_TYPE_JIG_UART_OFF_VB:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s UART OFF VBUS Status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s UART OFF VBUS status detached (%d) \n", __func__,status_count);
               }
#endif
		break;
	case CABLE_TYPE_JIG:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s JIG cable status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s JIG cable status detached (%d) \n", __func__,status_count);
               }
#endif
		return;
	case CABLE_TYPE_CDP:
#if defined(CONFIG_TOUCHSCREEN_ZINITIX_BT541) || defined(USE_TSP_TA_CALLBACKS)
		if (charger_callbacks && charger_callbacks->inform_charger)
			charger_callbacks->inform_charger(charger_callbacks,
			attached);
#endif
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s CDP status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s CDP Status detached (%d) \n", __func__,status_count);
               }
#endif
		sec_otg_set_vbus_state(attached);
		break;
	case CABLE_TYPE_OTG:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s OTG status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s OTG status detached (%d) \n", __func__,status_count);
               }
#endif
#if defined(CONFIG_USB_HOST_NOTIFY)
		sec_otg_notify(attached ? HNOTIFY_ID : HNOTIFY_ID_PULL);
#endif
	       return;
	case CABLE_TYPE_AUDIO_DOCK:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Audiodock status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s Audiodock status detached (%d) \n", __func__,status_count);
               }
#endif
		return;
	case CABLE_TYPE_CARDOCK:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Cardock status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s Cardock status detached (%d) \n", __func__,status_count);
               }
#endif
		switch_set_state(&switch_dock, attached ? 2 : 0);
		break;
	case CABLE_TYPE_DESK_DOCK:
	case CABLE_TYPE_DESK_DOCK_NO_VB:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Deskdock %s status attached (%d) \n",__func__,
				((cable_type == CABLE_TYPE_DESK_DOCK)? "VBUS" : "NOVBUS"),status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s Deskdock %s status detached (%d) \n", __func__,
				((cable_type == CABLE_TYPE_DESK_DOCK)? "VBUS" : "NOVBUS"),status_count);
               }
#endif
		switch_set_state(&switch_dock, attached);
		break;
	case CABLE_TYPE_219KUSB:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s 219K USB status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s 219K USB status detached (%d) \n", __func__,status_count);
               }
#endif
		sec_otg_set_vbus_state(attached);
		break;
	case CABLE_TYPE_INCOMPATIBLE:
#if defined(DEBUG_STATUS)
               if (attached)
               {
                       status_count = status_count+1;
                       pr_err("%s Incompatible Charger status attached (%d) \n",__func__, status_count);
               } else {
                       status_count = status_count-1;
                       pr_err("%s Incomabtible Charger status detached (%d) \n", __func__,status_count);
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

#if defined(CONFIG_FUELGAUGE_MAX17050)
	if(check_sm5504_jig_state())
	{
		struct power_supply *fuel_psy = power_supply_get_by_name("sec-fuelgauge");
		if (!fuel_psy || !fuel_psy->set_property)
			pr_err("%s: fail to get sec-fuelgauge psy\n", __func__);
		else {
			fuel_psy->set_property(fuel_psy, POWER_SUPPLY_PROP_CHARGE_TYPE, &value);
		}
	}
#endif
#if defined(CONFIG_QPNP_BMS)
	if(check_sm5504_jig_state())
	{
	  struct power_supply *fuel_psy = power_supply_get_by_name("bms");
	  if (!fuel_psy || !fuel_psy->set_property)
		pr_err("%s: fail to get BMS psy\n", __func__);
		else {
			fuel_psy->set_property(fuel_psy, POWER_SUPPLY_PROP_CHARGE_TYPE, &value);
		}
	}
#endif

	switch (set_cable_status) {
	case CABLE_TYPE_MISC:
		value.intval = POWER_SUPPLY_TYPE_MISC;
		break;
	case CABLE_TYPE_USB:
		value.intval = POWER_SUPPLY_TYPE_USB;
		break;
	case CABLE_TYPE_219KUSB:
	case CABLE_TYPE_AC:
	case CABLE_TYPE_AUDIO_DOCK:
		value.intval = POWER_SUPPLY_TYPE_MAINS;
		break;
	case CABLE_TYPE_CARDOCK:
		value.intval = POWER_SUPPLY_TYPE_CARDOCK;
		break;
	case CABLE_TYPE_CDP:
		value.intval = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case CABLE_TYPE_INCOMPATIBLE:
		value.intval = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	case CABLE_TYPE_DESK_DOCK:
		value.intval = POWER_SUPPLY_TYPE_MAINS;
		break;
        case CABLE_TYPE_JIG_UART_OFF_VB:
                value.intval = POWER_SUPPLY_TYPE_UARTOFF;
                break;
	case CABLE_TYPE_DESK_DOCK_NO_VB:
	case CABLE_TYPE_UARTOFF:
	case CABLE_TYPE_NONE:
		value.intval = POWER_SUPPLY_TYPE_BATTERY;
		break;
	default:
		pr_err("%s: invalid cable :%d\n", __func__, set_cable_status);
		return;
	}
	current_cable_type = value.intval;
	pr_info("%s:MUIC setting the cable type as (%d)\n",__func__,value.intval);
	if (!psy || !psy->set_property)
		pr_err("%s: fail to get battery psy\n", __func__);
	else {
		psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	}
}

struct sm5504_platform_data sm5504_pdata = {
	.callback = sm5504_callback,
#if defined(CONFIG_MUIC_SM5504_SUPPORT_LANHUB_TA)
	.lanhub_cb = sm5504_lanhub_callback,
#endif
	.dock_init = sm5504_dock_init,
	.oxp_callback = sm5504_oxp_callback,
	.mhl_sel = NULL,

};
#endif /*End of SM5504 MUIC Callbacks*/
