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
#ifdef CONFIG_TOUCHSCREEN_MMS300
#include <linux/i2c/mms300.h>
#endif
#define BATT_SEARCH_CNT_MAX	10
#if defined (CONFIG_VIDEO_MHL_V2)
static int MHL_Connected;
#endif

#if defined(CONFIG_SM5502_MUIC)
/* callbacks & Handlers for the SM5502 MUIC*/

#if 0 // by jjuny79.kim (USB team) , I defined the set_otg_set_vbus_state function explicitly.
/* Enable when the wrapper is ready
 * extern void sec_otg_set_vbus_state(int);
 * and remove the following dummy function.
 */
void sec_otg_set_vbus_state(int online)
{
	pr_info("%s:file, DUMMY sec_otg_set_vbus_state called\n",__FILE__);
}
#else
extern void sec_otg_set_vbus_state(int);
#endif

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

//Added TSP TA charger callbacks
#if defined(CONFIG_TOUCHSCREEN_ZINITIX_BT532)
struct zinitix_callbacks {
	void (*inform_charger)(int);
};
struct zinitix_callbacks *zinitix_charger_callbacks;
void zinitix_tsp_charger_infom(int cable_type){
	if (zinitix_charger_callbacks && zinitix_charger_callbacks->inform_charger)
		zinitix_charger_callbacks->inform_charger(cable_type);

	pr_info("%s: %s\n", __func__, cable_type ? "connected" : "disconnected");

}
void zinitix_tsp_register_callback(struct zinitix_callbacks *cb)
{
	zinitix_charger_callbacks = cb;
	pr_info("%s: [mxt] charger callback!\n", __func__);

}
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

#if defined (CONFIG_TOUCHSCREEN_MMS300)
struct tsp_callbacks *charger_callbacks;
#endif
void sm5502_callback(enum cable_type_t cable_type, int attached)
{
	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");
	static enum cable_type_t previous_cable_type = CABLE_TYPE_NONE;
	pr_info("%s, called : cable_type :%d \n",__func__, cable_type);
#if defined(CONFIG_TOUCHSCREEN_MMS252) || defined(CONFIG_TOUCHSCREEN_MMS300)
        if (charger_callbacks && charger_callbacks->inform_charger)
                charger_callbacks->inform_charger(charger_callbacks,
                attached);
#endif

#if defined(CONFIG_TOUCHSCREEN_ATMEL_MXT1188S)
	mxt_tsp_charger_infom(attached);
#endif
	set_cable_status = attached ? cable_type : CABLE_TYPE_NONE;

#if defined(CONFIG_TOUCHSCREEN_ZINITIX_BT532)
	zinitix_tsp_charger_infom(set_cable_status);
#endif

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

