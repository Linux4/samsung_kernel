/*
 * sm5714-charger.c - SM5714 Charger operation mode control module.
 *
 * Copyright (C) 2017 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mfd/sm/sm5714/sm5714.h>
#include <linux/mfd/sm/sm5714/sm5714-private.h>
#include <linux/usb/typec/common/pdic_notifier.h>
// #include "../../common/sec_charging_common.h"

enum {
    OP_MODE_SUSPEND     = 0x0,
    OP_MODE_CHG_ON_VBUS = 0x5,
    OP_MODE_USB_OTG     = 0x7,
    OP_MODE_FLASH_BOOST = 0x8,
};

enum {
	BSTOUT_4400mV   = 0x0,
	BSTOUT_4500mV   = 0x1,
	BSTOUT_4600mV   = 0x2,
	BSTOUT_4700mV   = 0x3,
	BSTOUT_4800mV   = 0x4,
	BSTOUT_5000mV   = 0x5,
	BSTOUT_5100mV   = 0x6,
	BSTOUT_5200mV   = 0x7,

	BSTOUT_5300mV   = 0x9,
	BSTOUT_5400mV   = 0xA,
	BSTOUT_5500mV   = 0xB,
};

enum {
	OTG_CURRENT_500mA   = 0x0,
	OTG_CURRENT_900mA   = 0x1,
	OTG_CURRENT_1200mA  = 0x2,
	OTG_CURRENT_1500mA  = 0x3,
};

#define make_OP_STATUS(vbus, otg, pwr_shar, flash, torch, suspend)  (((vbus & 0x1)      << SM5714_CHARGER_OP_EVENT_VBUSIN)      | \
												((otg & 0x1)       << SM5714_CHARGER_OP_EVENT_USB_OTG)     | \
												((pwr_shar & 0x1)  << SM5714_CHARGER_OP_EVENT_PWR_SHAR)    | \
												((flash & 0x1)     << SM5714_CHARGER_OP_EVENT_FLASH)       | \
												((torch & 0x1)     << SM5714_CHARGER_OP_EVENT_TORCH)       | \
												((suspend & 0x1)   << SM5714_CHARGER_OP_EVENT_SUSPEND))

struct sm5714_charger_oper_table_info {
	unsigned short status;
	unsigned char oper_mode;
	unsigned char BST_OUT;
	unsigned char OTG_CURRENT;
};

struct sm5714_charger_oper_info {
	struct i2c_client *i2c;
	struct mutex op_mutex;
	int max_table_num;
	int table_index;
	struct sm5714_charger_oper_table_info current_table;
	/* for Factory mode control */
	unsigned char factory_RID;
};
static struct sm5714_charger_oper_info *oper_info;

/**
 *  (VBUS in/out) (USB-OTG in/out) (PWR-SHAR in/out)
 *  (Flash on/off) (Torch on/off) (Suspend mode on/off)
 **/
static struct sm5714_charger_oper_table_info sm5714_charger_op_mode_table[] = {
	/* Charger=ON Mode in a valid Input */
	{ make_OP_STATUS(0, 0, 0, 0, 0, 0), OP_MODE_CHG_ON_VBUS, BSTOUT_4500mV, OTG_CURRENT_500mA},
	{ make_OP_STATUS(1, 0, 0, 0, 0, 0), OP_MODE_CHG_ON_VBUS, BSTOUT_4500mV, OTG_CURRENT_500mA},
	{ make_OP_STATUS(1, 1, 0, 0, 0, 0), OP_MODE_CHG_ON_VBUS, BSTOUT_4500mV, OTG_CURRENT_500mA},      /* Prevent : VBUS + OTG timing sync */
	{ make_OP_STATUS(1, 0, 0, 0, 1, 0), OP_MODE_CHG_ON_VBUS, BSTOUT_4500mV, OTG_CURRENT_500mA},
	/* Flash Boost Mode */
	{ make_OP_STATUS(0, 0, 0, 1, 0, 0), OP_MODE_FLASH_BOOST, BSTOUT_4500mV, OTG_CURRENT_500mA},
	{ make_OP_STATUS(1, 0, 0, 1, 0, 0), OP_MODE_FLASH_BOOST, BSTOUT_4500mV, OTG_CURRENT_500mA},

	{ make_OP_STATUS(0, 0, 1, 1, 0, 0), OP_MODE_FLASH_BOOST, BSTOUT_4500mV, OTG_CURRENT_900mA},
	{ make_OP_STATUS(0, 0, 0, 1, 1, 0), OP_MODE_FLASH_BOOST, BSTOUT_4500mV, OTG_CURRENT_900mA},
	{ make_OP_STATUS(0, 0, 0, 0, 1, 0), OP_MODE_FLASH_BOOST, BSTOUT_4500mV, OTG_CURRENT_900mA},
	/* USB OTG Mode */
	{ make_OP_STATUS(0, 1, 0, 0, 0, 0), OP_MODE_USB_OTG,     BSTOUT_5100mV, OTG_CURRENT_900mA},
	{ make_OP_STATUS(0, 0, 1, 0, 0, 0), OP_MODE_USB_OTG,     BSTOUT_5100mV, OTG_CURRENT_900mA},
	{ make_OP_STATUS(0, 1, 0, 1, 0, 0), OP_MODE_USB_OTG, 	 BSTOUT_5100mV, OTG_CURRENT_900mA},
	{ make_OP_STATUS(0, 1, 0, 0, 1, 0), OP_MODE_USB_OTG,     BSTOUT_5100mV, OTG_CURRENT_900mA},
	{ make_OP_STATUS(0, 0, 1, 0, 1, 0), OP_MODE_USB_OTG,     BSTOUT_5100mV, OTG_CURRENT_900mA},
	/* Suspend Mode */
	{ make_OP_STATUS(0, 0, 0, 0, 0, 1), OP_MODE_SUSPEND,     BSTOUT_5100mV, OTG_CURRENT_900mA},      /* Reserved position of SUSPEND mode table */
};

static int set_OP_MODE(struct i2c_client *i2c, u8 mode)
{
	return sm5714_update_reg(i2c, SM5714_CHG_REG_CNTL2, (mode << 0), (0xF << 0));
}

static int set_BSTOUT(struct i2c_client *i2c, u8 bstout)
{
	return sm5714_update_reg(i2c, SM5714_CHG_REG_BSTCNTL1, (bstout << 0), (0xF << 0));
}

static int set_OTG_CURRENT(struct i2c_client *i2c, u8 otg_curr)
{
	return sm5714_update_reg(i2c, SM5714_CHG_REG_BSTCNTL1, (otg_curr << 6), (0x3 << 6));
}

static inline int change_op_table(unsigned char new_status)
{
	u8 reg_st1;
	int i = 0;

	pr_info("sm5714-charger: %s: Old table[%d] info (STATUS: 0x%x, MODE: %d, BST_OUT: 0x%x, OTG_CURRENT: 0x%x)\n",
			__func__, oper_info->table_index, oper_info->current_table.status, oper_info->current_table.oper_mode,
			oper_info->current_table.BST_OUT, oper_info->current_table.OTG_CURRENT);

	/* Check actvated Suspend Mode */
	if (new_status & (0x1 << SM5714_CHARGER_OP_EVENT_SUSPEND)) {
		i = oper_info->max_table_num - 1;    /* Reserved SUSPEND Mode Table index */
	} else {
		/* Search matched Table */
		for (i = 0; i < oper_info->max_table_num; ++i) {
			if (new_status == sm5714_charger_op_mode_table[i].status) {
				break;
			}
		}
	}
	if (i == oper_info->max_table_num) {
		pr_err("sm5714-charger: %s: can't find matched charger op_mode table (status = 0x%x)\n", __func__, new_status);
		return -EINVAL;
	}

    /* Update current table info */

	/* Safety logic : (VBUSPOK or TA) + Flash On - boost out 5V fix */
	sm5714_read_reg(oper_info->i2c, SM5714_CHG_REG_STATUS1, &reg_st1);
	if ((reg_st1&(0x1<<0)) && (sm5714_charger_op_mode_table[i].status == make_OP_STATUS(1, 0, 0, 1, 0, 0))) {
		set_BSTOUT(oper_info->i2c, BSTOUT_5000mV);
		oper_info->current_table.BST_OUT = BSTOUT_5000mV;
	} else {
		set_BSTOUT(oper_info->i2c, sm5714_charger_op_mode_table[i].BST_OUT);
		oper_info->current_table.BST_OUT = sm5714_charger_op_mode_table[i].BST_OUT;
	}

	set_OTG_CURRENT(oper_info->i2c, sm5714_charger_op_mode_table[i].OTG_CURRENT);
	oper_info->current_table.OTG_CURRENT = sm5714_charger_op_mode_table[i].OTG_CURRENT;

	/* Factory 523K-JIG Test : Torch Light - Prevent VBUS input source */
	if ((oper_info->factory_RID == RID_523K) && (sm5714_charger_op_mode_table[i].status == make_OP_STATUS(0, 0, 0, 0, 1, 0))) {
		pr_info("sm5714-charger: %s: skip Flash Boost mode for Factory JIG fled:torch test\n", __func__);
	/* Factory 523K-JIG Test : Flash Light - Prevent VBUS input source */
	} else if ((oper_info->factory_RID == RID_523K) && (sm5714_charger_op_mode_table[i].status == make_OP_STATUS(0, 0, 0, 1, 0, 0))) {
		pr_info("sm5714-charger: %s: skip Flash Boost mode for Factory JIG fled:flash test\n", __func__);
	} else {
		set_OP_MODE(oper_info->i2c, sm5714_charger_op_mode_table[i].oper_mode);
		oper_info->current_table.oper_mode = sm5714_charger_op_mode_table[i].oper_mode;
	}
	oper_info->current_table.status = new_status;
	oper_info->table_index = i;

	pr_info("sm5714-charger: %s: New table[%d] info (STATUS: 0x%x, MODE: %d, BST_OUT: 0x%x, OTG_CURRENT: 0x%x)\n",
			__func__, oper_info->table_index, oper_info->current_table.status, oper_info->current_table.oper_mode,
			oper_info->current_table.BST_OUT, oper_info->current_table.OTG_CURRENT);

	return 0;
}

static inline unsigned char update_status(int event_type, bool enable)
{
	if (event_type > SM5714_CHARGER_OP_EVENT_VBUSIN) {
		pr_debug("sm5714-charger: %s: invalid event type (type=0x%x)\n", __func__, event_type);
		return oper_info->current_table.status;
	}

	if (enable) {
		return (oper_info->current_table.status | (1 << event_type));
	} else {
		return (oper_info->current_table.status & ~(1 << event_type));
	}
}

int sm5714_charger_oper_push_event(int event_type, bool enable)
{
	unsigned char new_status;
    int ret = 0;

	if (oper_info == NULL) {
		pr_err("sm5714-charger: %s: required init op_mode table\n", __func__);
		return -ENOENT;
	}
	pr_info("sm5714-charger: %s: event_type=%d, enable=%d\n", __func__, event_type, enable);

	mutex_lock(&oper_info->op_mutex);

	new_status = update_status(event_type, enable);
    if (new_status == oper_info->current_table.status) {
		goto out;
	}
    ret = change_op_table(new_status);

out:
	mutex_unlock(&oper_info->op_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(sm5714_charger_oper_push_event);

static inline int detect_initial_table_index(struct i2c_client *i2c)
{
    return 0;
}
int sm5714_charger_oper_table_init(struct sm5714_dev *sm5714)
{
	struct i2c_client *i2c = sm5714->charger;
	int index;

	if (oper_info) {
		pr_info("sm5714-charger: %s: already initialized\n", __func__);
		return 0;
	}

	if (i2c == NULL) {
		pr_err("sm5714-charger: %s: invalid i2c client handler=n", __func__);
		return -EINVAL;
	}

	oper_info = kmalloc(sizeof(struct sm5714_charger_oper_info), GFP_KERNEL);
	if (oper_info == NULL) {
		pr_err("sm5714-charger: %s: failed to alloctae memory\n", __func__);
		return -ENOMEM;
	}
	oper_info->i2c = i2c;

	mutex_init(&oper_info->op_mutex);

	/* set default operation mode condition */
	oper_info->max_table_num = ARRAY_SIZE(sm5714_charger_op_mode_table);
	index = detect_initial_table_index(oper_info->i2c);
	oper_info->table_index = index;
	oper_info->current_table.status = sm5714_charger_op_mode_table[index].status;
	oper_info->current_table.oper_mode = sm5714_charger_op_mode_table[index].oper_mode;
	oper_info->current_table.BST_OUT = sm5714_charger_op_mode_table[index].BST_OUT;
	oper_info->current_table.OTG_CURRENT = sm5714_charger_op_mode_table[index].OTG_CURRENT;

	set_OP_MODE(oper_info->i2c, oper_info->current_table.oper_mode);
	set_BSTOUT(oper_info->i2c, oper_info->current_table.BST_OUT);
	set_OTG_CURRENT(oper_info->i2c, oper_info->current_table.OTG_CURRENT);

	oper_info->factory_RID = 0;

	pr_info("sm5714-charger: %s: current table[%d] info (STATUS: 0x%x, MODE: %d, BST_OUT: 0x%x, OTG_CURRENT: 0x%x)\n", \
			__func__, oper_info->table_index, oper_info->current_table.status, oper_info->current_table.oper_mode, \
			oper_info->current_table.BST_OUT, oper_info->current_table.OTG_CURRENT);

	return 0;
}
EXPORT_SYMBOL_GPL(sm5714_charger_oper_table_init);

int sm5714_charger_oper_get_current_status(void)
{
	if (oper_info == NULL) {
		return -EINVAL;
	}
	return oper_info->current_table.status;
}
EXPORT_SYMBOL_GPL(sm5714_charger_oper_get_current_status);

int sm5714_charger_oper_get_current_op_mode(void)
{
	if (oper_info == NULL) {
		return -EINVAL;
	}
	return oper_info->current_table.oper_mode;
}
EXPORT_SYMBOL_GPL(sm5714_charger_oper_get_current_op_mode);

int sm5714_charger_oper_en_factory_mode(int dev_type, int rid, bool enable)
{
#if 0 // Disable Factory Mode (GED)
	union power_supply_propval val = {0, };

	if (oper_info == NULL) {
		return -EINVAL;
	}

	if (enable) {
		switch (rid) {
		case RID_523K:
			sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_CNTL1, (0x0 << 6), (0x1 << 6));				/* AICLEN_VBUS = 0 (Disable) */
			sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_FACTORY1, (0x1 << 0), (0x1 << 0));				/* NOZX 	   = 1 */
			sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_VBUSCNTL, (0x7F << 0), (0x7F << 0));			/* VBUS_LIMIT  = MAX(3275mA) */
		break;
		case RID_301K:
		case RID_619K:
			sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_CNTL1, (0x0 << 6), (0x1 << 6));			/* AICLEN_VBUS 	= 0 */
			sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_FACTORY1, (0x0 << 0), (0x1 << 0));			/* NOZX 		= 0 */
			sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_VBUSCNTL, (0x44 << 0), (0x7F << 0));		/* VBUS_LIMIT	= 1800mA */
		break;
		}

		psy_do_property("sm5714-fuelgauge", set,
			POWER_SUPPLY_PROP_ENERGY_NOW, val);

		oper_info->factory_RID = rid;
		pr_info("sm5714-charger: %s enable factroy mode configuration\n", __func__);
	} else {
		sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_CNTL1, (0x1 << 6), (0x1 << 6));			/* AICLEN_VBUS = 1 */
		sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_FACTORY1, (0x0 << 0), (0x1 << 0));			/* NOZX = 0 */
		sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_VBUSCNTL, (0x10 << 0), (0x7F << 0));		/* VBUS_LIMIT=500mA*/

		oper_info->factory_RID = 0;
		pr_info("sm5714-charger: %s disable factroy mode configuration\n", __func__);
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(sm5714_charger_oper_en_factory_mode);

int sm5714_charger_oper_forced_vbus_limit_control(int mA)
{

	u8 old_offset, new_offset;
	int old_mA;
	int msec;

	sm5714_read_reg(oper_info->i2c, SM5714_CHG_REG_VBUSCNTL, &old_offset);
	old_mA = ((old_offset & 0x7F) * 25) + 100;

	new_offset = ((mA - 100) / 25);
	sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_VBUSCNTL, ((new_offset & 0x7F) << 0), (0x7F << 0));

	msec = (old_mA - mA) / 10;		// 10mA/ms
	if (msec < 0) {
		msec *= (-1);
	}

	msleep(msec);

	pr_info("sm5714-charger: %s VBUSLIMIT control 0x%X[%dmA] -> 0x%X[%dmA] (%d ms) \n", __func__, old_offset, old_mA, new_offset, mA, msec);

	return 0;
}
EXPORT_SYMBOL_GPL(sm5714_charger_oper_forced_vbus_limit_control);
