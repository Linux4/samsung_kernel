/*
 *  sm5720_charger.h
 *  Samsung SM5720 Charger Driver header file
 *
 *  Copyright (C) 2016 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SM5720_CHARGER_H
#define __SM5720_CHARGER_H __FILE__

#include <linux/mfd/core.h>
#include <linux/mfd/sm5720.h>
#include <linux/mfd/sm5720-private.h>
#include <linux/regulator/machine.h>

//#define SM5720_WATCHDOG_RESET_ACTIVATE
//#define SM5720_SBPS_ACTIVATE

enum {
	CHIP_ID = 0,
	DATA,
};

ssize_t sm5720_chg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t sm5720_chg_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

#define SM5720_CHARGER_ATTR(_name)				\
{							                    \
	.attr = {.name = #_name, .mode = 0664},	    \
	.show = sm5720_chg_show_attrs,			    \
	.store = sm5720_chg_store_attrs,			\
}

#define INPUT_CURRENT_TA		                1000

#define REDUCE_CURRENT_STEP						100
#define MINIMUM_INPUT_CURRENT					300
#define SLOW_CHARGING_CURRENT_STANDARD          400

struct sm5720_charger_data {
	struct device *dev;
	struct i2c_client *i2c;
	struct mutex charger_mutex;

	struct sm5720_platform_data *sm5720_pdata;

	struct power_supply	psy_chg;
	struct power_supply	psy_otg;
	int status;

	/* for IRQ-service handling */
	int irq_aicl;
	int irq_vbus_pok;
	int irq_wpcin_pok;
	int irq_topoff;
	int irq_done;

	/* for Workqueue & wake-lock, mutex process */
	struct workqueue_struct *wqueue;
	struct delayed_work wpc_work;
	struct delayed_work afc_work;
	struct delayed_work aicl_work;
	struct delayed_work topoff_work;

	struct wake_lock wpc_wake_lock;
	struct wake_lock afc_wake_lock;
	struct wake_lock aicl_wake_lock;

	/* for charging operation handling */
	int charge_mode;
	int is_charging;
	int cable_type;
	int input_current;
	int charging_current;

	int irq_wpcin_state;
	int siop_level;
	int float_voltage;

	bool slow_late_chg_mode;

	sec_charging_current_t *charging_current_t;
};

/* For Charger Operation mode control module */
enum {
    SM5720_CHARGER_OP_MODE_SUSPEND              = 0x0,
    SM5720_CHARGER_OP_MODE_TX_MODE_NOVBUS       = 0x1,
    SM5720_CHARGER_OP_MODE_TX_MODE_VBUS         = 0x2,
    SM5720_CHARGER_OP_MODE_WPC_OTG_CHG_ON       = 0x3,
    SM5720_CHARGER_OP_MODE_CHG_ON_WPCIN         = 0x4,
    SM5720_CHARGER_OP_MODE_CHG_ON_VBUS          = 0x5,
    SM5720_CHARGER_OP_MODE_USB_OTG_TX_MODE      = 0x6,
    SM5720_CHARGER_OP_MODE_USB_OTG              = 0x7,
};

enum {
    SM5720_CHARGER_OP_EVENT_VBUSIN              = 0x6,
    SM5720_CHARGER_OP_EVENT_WPCIN               = 0x5,
    SM5720_CHARGER_OP_EVENT_USB_OTG             = 0x4,
    SM5720_CHARGER_OP_EVENT_PWR_SHAR            = 0x3,
    SM5720_CHARGER_OP_EVENT_5V_TX               = 0x2,
    SM5720_CHARGER_OP_EVENT_9V_TX               = 0x1,
    SM5720_CHARGER_OP_EVENT_SUSPEND             = 0x0,
};

int sm5720_charger_oper_push_event(int event_type, bool enable);
int sm5720_charger_oper_table_init(struct i2c_client *i2c);

int sm5720_charger_oper_get_current_status(void);
int sm5720_charger_oper_get_current_op_mode(void);

#endif /* __SM5720_CHARGER_H */
