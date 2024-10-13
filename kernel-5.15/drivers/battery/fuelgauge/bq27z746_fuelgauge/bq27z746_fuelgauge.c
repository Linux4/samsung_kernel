// SPDX-License-Identifier: GPL-2.0

/*
 * BQ27xxx battery driver
 *
 * Copyright (C) 2008 Rodolfo Giometti <giometti@linux.it>
 * Copyright (C) 2008 Eurotech S.p.A. <info@eurotech.it>
 * Copyright (C) 2010-2011 Lars-Peter Clausen <lars@metafoo.de>
 * Copyright (C) 2011 Pali Rohár <pali@kernel.org>
 * Copyright (C) 2015 Texas Instruments Incorporated - https://www.ti.com/
 *	Andrew F. Davis <afd@ti.com>
 * Copyright (C) 2017 Liam Breck <kernel@networkimprov.net>
 *
 * Based on a previous work by Copyright (C) 2008 Texas Instruments, Inc.
 *
 * Datasheets:
 * https://www.ti.com/product/bq27z746
 */

#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <asm/unaligned.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/rtc.h>
#include<linux/string.h>

#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

#if (KERNEL_VERSION(5, 0, 0) <= LINUX_VERSION_CODE)
#include <linux/time64.h>
#define SEC_TIMESPEC timespec64
#define SEC_GETTIMEOFDAY ktime_get_real_ts64
#define SEC_RTC_TIME_TO_TM rtc_time64_to_tm
#else
#include <linux/time.h>
#define SEC_TIMESPEC timeval
#define SEC_GETTIMEOFDAY do_gettimeofday
#define SEC_RTC_TIME_TO_TM rtc_time_to_tm
#endif

#include "bq27z746_fuelgauge.h"

#define BQ27XXX_MANUFACTURER	"Texas Instruments"

/* BQ27XXX Flags */
#define BQ27XXX_FLAG_DSC	BIT(0)
#define BQ27XXX_FLAG_SOCF	BIT(1) /* State-of-Charge threshold final */
#define BQ27XXX_FLAG_SOC1	BIT(2) /* State-of-Charge threshold 1 */
#define BQ27XXX_FLAG_CFGUP	BIT(4)
#define BQ27XXX_FLAG_FC		BIT(9)
#define BQ27XXX_FLAG_OTD	BIT(14)
#define BQ27XXX_FLAG_OTC	BIT(15)
#define BQ27XXX_FLAG_UT		BIT(14)
#define BQ27XXX_FLAG_OT		BIT(15)

/* BQ27000 has different layout for Flags register */
#define BQ27000_FLAG_EDVF	BIT(0) /* Final End-of-Discharge-Voltage flag */
#define BQ27000_FLAG_EDV1	BIT(1) /* First End-of-Discharge-Voltage flag */
#define BQ27000_FLAG_CI		BIT(4) /* Capacity Inaccurate flag */
#define BQ27000_FLAG_FC		BIT(5)
#define BQ27000_FLAG_CHGS	BIT(7) /* Charge state flag */

/* BQ27Z746 has different layout for Flags register */
#define BQ27Z746_FLAG_FDC	BIT(4) /* Battery fully discharged */
#define BQ27Z746_FLAG_FC	BIT(5) /* Battery fully charged */
#define BQ27Z746_FLAG_DIS_CH	BIT(6) /* Battery is discharging */

/* control register params */
#define BQ27XXX_SEALED			0x0030
#define BQ27XXX_SET_CFGUPDATE		0x13
#define BQ27XXX_SOFT_RESET		0x42
#define BQ27XXX_RESET			0x41

#define BQ27XXX_RS			(20) /* Resistor sense mOhm */
#define BQ27XXX_POWER_CONSTANT		(29200) /* 29.2 µV^2 * 1000 */
#define BQ27XXX_CURRENT_CONSTANT	(3570) /* 3.57 µV * 1000 */

#define INVALID_REG_ADDR	0xff

#define FAKE_FG_SOC_OB_MODE		88
#define FAKE_FG_VOLT_OB_MODE	4000
#define RETRY_COUNT		3

#define DEBUG	0
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
static char __read_mostly *f_mode;
module_param(f_mode, charp, 0444);
#endif

static int __read_mostly factory_mode;
module_param(factory_mode, int, 0444);

/*
 * bq27xxx_reg_index - Register names
 *
 * These are indexes into a device's register mapping array.
 */

/*
 *battery_present will be true in normal booting case with battery present and will be false when booting with 523k jig+ no battery case
 */
static int battery_present = 1;

#if IS_ENABLED(CONFIG_BQ27Z746_I2C_ERR_SW_WA)
/* to maintain prev value so as to use at i2c failure*/
#define INVALID_TIME 0
static unsigned long i2c_fail_time = INVALID_TIME;
static bool i2c_recovery = false;
static int prev_volt = -1;
static int prev_temp = -1;
static int prev_soc = -1;
static int prev_soh = -1;
static int prev_ai = -1;
static int prev_current = -1;
static int prev_cyc = -1;
static int prev_intTemp = -1;
#endif
#if IS_ENABLED(CONFIG_BQ27Z746_LOW_TEMP_COMP)
/* for Low temperature Compensation*/
static bool temperatureComp = false;
#endif

enum bq27xxx_reg_index {
	BQ27XXX_REG_CTRL = 0,	/* Control */
	BQ27XXX_REG_TEMP,	/* Temperature */
	BQ27XXX_REG_INT_TEMP,	/* Internal Temperature */
	BQ27XXX_REG_VOLT,	/* Voltage */
	BQ27XXX_REG_AI,		/* Average Current */
	BQ27XXX_REG_FLAGS,	/* Flags */
	BQ27XXX_REG_TTE,	/* Time-to-Empty */
	BQ27XXX_REG_TTF,	/* Time-to-Full */
	BQ27XXX_REG_TTES,	/* Time-to-Empty Standby */
	BQ27XXX_REG_TTECP,	/* Time-to-Empty at Constant Power */
	BQ27XXX_REG_NAC,	/* Nominal Available Capacity */
	BQ27XXX_REG_RC,		/* Remaining Capacity */
	BQ27XXX_REG_FCC,	/* Full Charge Capacity */
	BQ27XXX_REG_CYCT,	/* Cycle Count */
	BQ27XXX_REG_AE,		/* Available Energy */
	BQ27XXX_REG_SOC,	/* State-of-Charge */
	BQ27XXX_REG_DCAP,	/* Design Capacity */
	BQ27XXX_REG_AP,		/* Average Power */
	BQ27XXX_DM_CTRL,	/* Block Data Control */
	BQ27XXX_DM_CLASS,	/* Data Class */
	BQ27XXX_DM_BLOCK,	/* Data Block */
	BQ27XXX_DM_DATA,	/* Block Data */
	BQ27XXX_DM_CKSUM,	/* Block Data Checksum */
	BQ27XXX_REG_INSTCURR,	/* Instant Current */
	BQ27XXX_REG_MLI,	/* Maximum Load Current */
	BQ27XXX_REG_MLTTE,	/* Maximum Load Time To Empty */
	BQ27XXX_REG_SOH,	/*State Of Health*/
	BQ27XXX_REG_CV,	/*Charging Voltage*/
	BQ27XXX_REG_CC,	/*Charging Current*/
	BQ27XXX_REG_QMAX,	/*Qmax Cycle Count*/
	BQ27XXX_REG_TV, /* Term Voltage*/
	BQ27XXX_REG_MAX,	/* sentinel */
};

#define BQ27XXX_DM_REG_ROWS \
	[BQ27XXX_DM_CTRL] = 0x61,  \
	[BQ27XXX_DM_CLASS] = 0x3e, \
	[BQ27XXX_DM_BLOCK] = 0x3f, \
	[BQ27XXX_DM_DATA] = 0x40,  \
	[BQ27XXX_DM_CKSUM] = 0x60

/* Register mappings */
static u8
	bq27z746_regs[BQ27XXX_REG_MAX] = {
		[BQ27XXX_REG_CTRL] = 0x3E,
		[BQ27XXX_REG_TEMP] = 0x06,
		[BQ27XXX_REG_INT_TEMP] = 0x28,
		[BQ27XXX_REG_VOLT] = 0x08,
		[BQ27XXX_REG_AI] = 0x14,
		[BQ27XXX_REG_FLAGS] = 0x0a,
		[BQ27XXX_REG_TTE] = 0x16,
		[BQ27XXX_REG_TTF] = 0x18,
		[BQ27XXX_REG_TTES] = INVALID_REG_ADDR,
		[BQ27XXX_REG_TTECP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_NAC] = INVALID_REG_ADDR,
		[BQ27XXX_REG_RC] = 0x10,
		[BQ27XXX_REG_FCC] = 0x12,
		[BQ27XXX_REG_CYCT] = 0x2a,
		[BQ27XXX_REG_SOC] = 0x2c,
		[BQ27XXX_REG_DCAP] = 0x3c,
		[BQ27XXX_REG_AP] = 0x22,
		[BQ27XXX_REG_INSTCURR] = 0x0C,
		[BQ27XXX_REG_MLI] = 0x1E,
		[BQ27XXX_REG_MLTTE] = 0x20,
		[BQ27XXX_REG_SOH] = 0x2E,
		[BQ27XXX_REG_CV] = 0x30,
		[BQ27XXX_REG_CC] = 0x32,
		[BQ27XXX_REG_QMAX] = 0x3A,
		[BQ27XXX_REG_TV] = 0x34,
		BQ27XXX_DM_REG_ROWS,
	};

static enum power_supply_property bq27z746_props[] = {
	POWER_SUPPLY_PROP_STATUS,
};

struct bq27xxx_dm_reg {
	u8 subclass_id;
	u8 offset;
	u8 bytes;
	u16 min, max;
};

enum bq27xxx_dm_reg_id {
	BQ27XXX_DM_DESIGN_CAPACITY = 0,
	BQ27XXX_DM_DESIGN_ENERGY,
	BQ27XXX_DM_TERMINATE_VOLTAGE,
};

/* todo create data memory definitions from datasheets and test on chips */
//#define bq27z746_dm_regs NULL
static struct bq27xxx_dm_reg bq27z746_dm_regs[] = {
	[BQ27XXX_DM_DESIGN_CAPACITY]   = { 82, 12, 2,    0, 32767 },
	[BQ27XXX_DM_DESIGN_ENERGY]     = { 82, 14, 2,    0, 32767 },
	[BQ27XXX_DM_TERMINATE_VOLTAGE] = { 82, 18, 2, 2800,  3700 },
};
#define BQ27XXX_O_ZERO		BIT(0)
#define BQ27XXX_O_OTDC		BIT(1) /* has OTC/OTD overtemperature flags */
#define BQ27XXX_O_UTOT		BIT(2) /* has OT overtemperature flag */
#define BQ27XXX_O_CFGUP		BIT(3)
#define BQ27XXX_O_RAM		BIT(4)
#define BQ27Z746_O_BITS		BIT(5)
#define BQ27XXX_O_SOC_SI	BIT(6) /* SoC is single register */
#define BQ27XXX_O_HAS_CI	BIT(7) /* has Capacity Inaccurate flag */
#define BQ27XXX_O_MUL_CHEM	BIT(8) /* multiple chemistries supported */

#define BQ27XXX_DATA(ref, key, opt) {		\
	.opts = (opt),				\
	.unseal_key = key,			\
	.regs  = ref##_regs,			\
	.dm_regs = ref##_dm_regs,		\
	.props = ref##_props,			\
	.props_size = ARRAY_SIZE(ref##_props) }

/* Block size */
#define BQ27XXX_DM_SZ	34
static DEFINE_IDR(battery_id);
static DEFINE_MUTEX(battery_mutex);

static const struct i2c_device_id bq27xxx_i2c_id_table[] = {
	{ "bq27z746", BQ27Z746 },
	{},
};
MODULE_DEVICE_TABLE(i2c, bq27xxx_i2c_id_table);

#ifdef CONFIG_OF
static const struct of_device_id bq27xxx_battery_i2c_of_match_table[] = {
	{ .compatible = "ti,bq27z746" },
	{},
};
MODULE_DEVICE_TABLE(of, bq27xxx_battery_i2c_of_match_table);
#endif

static struct {
	u32 opts;
	u32 unseal_key;
	u8 *regs;
	struct bq27xxx_dm_reg *dm_regs;
	enum power_supply_property *props;
	size_t props_size;
} bq27xxx_chip_data[] = {
	[BQ27Z746]  = BQ27XXX_DATA(bq27z746,   0x04143672, BQ27Z746_O_BITS),
};

static DEFINE_MUTEX(bq27xxx_list_lock);
static LIST_HEAD(bq27xxx_battery_devices);

#define BQ27XXX_MSLEEP(i) usleep_range((i)*1000, (i)*1000+500)


/**
 * struct bq27xxx_dm_buf - chip data memory buffer
 * @class: data memory subclass_id
 * @block: data memory block number
 * @data: data from/for the block
 * @has_data: true if data has been filled by read
 * @dirty: true if data has changed since last read/write
 *
 * Encapsulates info required to manage chip data memory blocks.
 */
struct bq27xxx_dm_buf {
	u8 class;
	u8 block;
	u8 data[BQ27XXX_DM_SZ];
	bool has_data, dirty;
};

#define BQ27XXX_DM_BUF(di, i) { \
	.class = (di)->dm_regs[i].subclass_id, \
	.block = (di)->dm_regs[i].offset / BQ27XXX_DM_SZ, \
}

static inline __be16 *bq27xxx_dm_reg_ptr(struct bq27xxx_dm_buf *buf,
				      struct bq27xxx_dm_reg *reg)
{
	if (buf->class == reg->subclass_id &&
	    buf->block == reg->offset / BQ27XXX_DM_SZ)
		return (__be16 *) (buf->data + reg->offset % BQ27XXX_DM_SZ);

	return NULL;
}

static const char * const bq27xxx_dm_reg_name[] = {
	[BQ27XXX_DM_DESIGN_CAPACITY] = "design-capacity",
	[BQ27XXX_DM_DESIGN_ENERGY] = "design-energy",
	[BQ27XXX_DM_TERMINATE_VOLTAGE] = "terminate-voltage",
};

static bool bq27xxx_dt_to_nvm = true;
module_param_named(dt_monitored_battery_updates_nvm, bq27xxx_dt_to_nvm, bool, 0444);
MODULE_PARM_DESC(dt_monitored_battery_updates_nvm,
	"Devicetree monitored-battery config updates data memory on NVM/flash chips.\n"
	"Users must set this =0 when installing a different type of battery!\n"
	"Default is =1."
#ifndef CONFIG_BATTERY_BQ27XXX_DT_UPDATES_NVM
	"\nSetting this affects future kernel updates, not the current configuration."
#endif
);

static int poll_interval_param_set(const char *val, const struct kernel_param *kp)
{
	struct bq27xxx_device_info *di;
	unsigned int prev_val = *(unsigned int *) kp->arg;
	int ret;

	ret = param_set_uint(val, kp);
	if (ret < 0 || prev_val == *(unsigned int *) kp->arg)
		return ret;

	mutex_lock(&bq27xxx_list_lock);
	list_for_each_entry(di, &bq27xxx_battery_devices, list) {
		cancel_delayed_work_sync(&di->work);
		schedule_delayed_work(&di->work, 0);
	}
	mutex_unlock(&bq27xxx_list_lock);

	return ret;
}

static const struct kernel_param_ops param_ops_poll_interval = {
	.get = param_get_uint,
	.set = poll_interval_param_set,
};

static unsigned int poll_interval = 360;
module_param_cb(poll_interval, &param_ops_poll_interval, &poll_interval, 0644);
MODULE_PARM_DESC(poll_interval,
		 "battery poll interval in seconds - 0 disables polling");

/*
 * Common code for BQ27xxx devices
 */
#if IS_ENABLED(CONFIG_BQ27Z746_I2C_ERR_SW_WA)
/* Get current time in seconds*/
static unsigned long bq27xxx_get_currenttime()
{
	struct SEC_TIMESPEC tv;
    /* Format the Log time R#: [hr:min:sec.microsec] */
	SEC_GETTIMEOFDAY(&tv);
    /* Convert rtc to local time */
	pr_info("%s: %d\n", __func__, (u32)tv.tv_sec);
	return (u32)tv.tv_sec;
}

/* return prev value if i2c fail because of environmental issue*/
static int bq27xxx_prevValue(int reg_index)
{
	int ret = -1;
	switch(reg_index){
			case BQ27XXX_REG_SOC:
			{
				ret = prev_soc;
				break;
			}
			case BQ27XXX_REG_TEMP:
			{
				ret = prev_temp;
				break;
			}
			case BQ27XXX_REG_VOLT:
			{
				ret = prev_volt;
				break;
			}
			case BQ27XXX_REG_INSTCURR:
			{
				ret = prev_current;
				break;
			}
			case BQ27XXX_REG_SOH:
			{
				ret = prev_soh;
				break;
			}
			case BQ27XXX_REG_AI:
			{
				ret = prev_ai;
				break;
			}
			case BQ27XXX_REG_CYCT:
			{
				ret = prev_cyc;
				break;
			}
			case BQ27XXX_REG_INT_TEMP:
			{
				ret = prev_intTemp;
				break;
			}
			default:
			{
				ret = -EINVAL;
				break;
			}
		}
	return ret;
}
#endif

static inline int bq27xxx_read(struct bq27xxx_device_info *di, int reg_index,
			       bool single)
{
	int ret;
#if IS_ENABLED(CONFIG_BQ27Z746_I2C_ERR_SW_WA)
	unsigned long time_now;
#endif
	
	if (!di || di->regs[reg_index] == INVALID_REG_ADDR)
		return -EINVAL;
#if IS_ENABLED(CONFIG_BQ27Z746_I2C_ERR_SW_WA)
	time_now = bq27xxx_get_currenttime();
	if (i2c_recovery && i2c_fail_time != INVALID_TIME) {
		if (time_now - i2c_fail_time < 10) {
				pr_info("%s: skip i2c read, i2c recovery time, %ld elapsed\n", __func__, (time_now - i2c_fail_time));
				return bq27xxx_prevValue(reg_index);
		} else {
			pr_info("%s: i2c recovery time complete (%ld)\n", __func__, (time_now - i2c_fail_time));
			i2c_recovery = false;
		}
	}
#endif
	ret = di->bus.read(di, di->regs[reg_index], single);
#if IS_ENABLED(CONFIG_BQ27Z746_I2C_ERR_SW_WA)
	if (i2c_recovery && i2c_fail_time != INVALID_TIME) {
		return bq27xxx_prevValue(reg_index);
	}
#endif
	if (ret < 0)
		dev_dbg(di->dev, "failed to read register 0x%02x (index %d)\n",
			di->regs[reg_index], reg_index);

	return ret;
}

static inline int bq27xxx_write(struct bq27xxx_device_info *di, int reg_index,
				u16 value, bool single)
{
	int ret;

	if (!di || di->regs[reg_index] == INVALID_REG_ADDR)
		return -EINVAL;

	if (!di->bus.write)
		return -EPERM;

	ret = di->bus.write(di, di->regs[reg_index], value, single);
	if (ret < 0)
		dev_dbg(di->dev, "failed to write register 0x%02x (index %d)\n",
			di->regs[reg_index], reg_index);

	return ret;
}

static inline int bq27xxx_read_block(struct bq27xxx_device_info *di, int reg_index,
				     u8 *data, int len)
{
	int ret;

	if (!di || di->regs[reg_index] == INVALID_REG_ADDR)
		return -EINVAL;

	if (!di->bus.read_bulk)
		return -EPERM;

	ret = di->bus.read_bulk(di, di->regs[reg_index], data, len);
	if (ret < 0)
		dev_dbg(di->dev, "failed to read_bulk register 0x%02x (index %d)\n",
			di->regs[reg_index], reg_index);

	return ret;
}

static inline int bq27xxx_write_block(struct bq27xxx_device_info *di, int reg_index,
				      u8 *data, int len)
{
	int ret;

	if (!di || di->regs[reg_index] == INVALID_REG_ADDR)
		return -EINVAL;

	if (!di->bus.write_bulk)
		return -EPERM;

	ret = di->bus.write_bulk(di, di->regs[reg_index], data, len);
	if (ret < 0)
		dev_dbg(di->dev, "failed to write_bulk register 0x%02x (index %d)\n",
			di->regs[reg_index], reg_index);

	return ret;
}

static int bq27xxx_battery_seal(struct bq27xxx_device_info *di)
{
	int ret;

	ret = bq27xxx_write(di, BQ27XXX_REG_CTRL, BQ27XXX_SEALED, false);
	if (ret < 0) {
		dev_err(di->dev, "bus error on seal: %d\n", ret);
		return ret;
	}
	dev_info(di->dev, "%s Seal successful : ret(%d)\n", __func__, ret);
	return 0;
}

static int bq27xxx_battery_unseal(struct bq27xxx_device_info *di)
{
	int ret;

	if (di->unseal_key == 0) {
		dev_err(di->dev, "unseal failed due to missing key\n");
		return -EINVAL;
	}
	mutex_lock(&bq27xxx_list_lock);
	ret = bq27xxx_write(di, BQ27XXX_REG_CTRL, (u16)(di->unseal_key >> 16), false);
	if (ret < 0)
		goto out;

	ret = bq27xxx_write(di, BQ27XXX_REG_CTRL, (u16)di->unseal_key, false);
	if (ret < 0)
		goto out;
	mutex_unlock(&bq27xxx_list_lock);
	return 0;

out:
	dev_err(di->dev, "bus error on unseal: %d\n", ret);
	mutex_unlock(&bq27xxx_list_lock);
	return ret;
}

static u8 bq27xxx_battery_checksum_dm_block(struct bq27xxx_dm_buf *buf)
{
	u16 sum = 0;
	int i;

	for (i = 0; i < BQ27XXX_DM_SZ; i++)
		sum += buf->data[i];
	sum &= 0xff;

	return 0xff - sum;
}

static int bq27xxx_battery_read_dm_block(struct bq27xxx_device_info *di,
					 struct bq27xxx_dm_buf *buf)
{
	int ret;

	buf->has_data = false;

	ret = bq27xxx_write(di, BQ27XXX_DM_CLASS, buf->class, true);
	if (ret < 0)
		goto out;

	ret = bq27xxx_write(di, BQ27XXX_DM_BLOCK, buf->block, true);
	if (ret < 0)
		goto out;

	BQ27XXX_MSLEEP(1);

	ret = bq27xxx_read_block(di, BQ27XXX_DM_DATA, buf->data, BQ27XXX_DM_SZ);
	if (ret < 0)
		goto out;

	ret = bq27xxx_read(di, BQ27XXX_DM_CKSUM, true);
	if (ret < 0)
		goto out;

	if ((u8)ret != bq27xxx_battery_checksum_dm_block(buf)) {
		ret = -EINVAL;
		goto out;
	}

	buf->has_data = true;
	buf->dirty = false;

	return 0;

out:
	dev_err(di->dev, "bus error reading chip memory: %d\n", ret);
	return ret;
}

static void bq27xxx_battery_update_dm_block(struct bq27xxx_device_info *di,
					    struct bq27xxx_dm_buf *buf,
					    enum bq27xxx_dm_reg_id reg_id,
					    unsigned int val)
{
	struct bq27xxx_dm_reg *reg = &di->dm_regs[reg_id];
	const char *str = bq27xxx_dm_reg_name[reg_id];
	__be16 *prev = bq27xxx_dm_reg_ptr(buf, reg);

	if (prev == NULL) {
		dev_warn(di->dev, "buffer does not match %s dm spec\n", str);
		return;
	}

	if (reg->bytes != 2) {
		dev_warn(di->dev, "%s dm spec has unsupported byte size\n", str);
		return;
	}

	if (!buf->has_data)
		return;

	if (be16_to_cpup(prev) == val) {
		dev_info(di->dev, "%s has %u\n", str, val);
		return;
	}

#ifdef CONFIG_BATTERY_BQ27XXX_DT_UPDATES_NVM
	if (!(di->opts & BQ27XXX_O_RAM) && !bq27xxx_dt_to_nvm) {
#else
	if (!(di->opts & BQ27XXX_O_RAM)) {
#endif
		/* devicetree and NVM differ; defer to NVM */
		dev_warn(di->dev, "%s has %u; update to %u disallowed "
#ifdef CONFIG_BATTERY_BQ27XXX_DT_UPDATES_NVM
			 "by dt_monitored_battery_updates_nvm=0"
#else
			 "for flash/NVM data memory"
#endif
			 "\n", str, be16_to_cpup(prev), val);
		return;
	}

	dev_info(di->dev, "update %s to %u\n", str, val);

	*prev = cpu_to_be16(val);
	buf->dirty = true;
}

static int bq27xxx_battery_cfgupdate_priv(struct bq27xxx_device_info *di, bool active)
{
	const int limit = 100;
	u16 cmd = active ? BQ27XXX_SET_CFGUPDATE : BQ27XXX_SOFT_RESET;
	int ret, try = limit;

	ret = bq27xxx_write(di, BQ27XXX_REG_CTRL, cmd, false);
	if (ret < 0)
		return ret;

	do {
		BQ27XXX_MSLEEP(25);
		ret = bq27xxx_read(di, BQ27XXX_REG_FLAGS, false);
		if (ret < 0)
			return ret;
	} while (!!(ret & BQ27XXX_FLAG_CFGUP) != active && --try);

	if (!try && di->chip != BQ27425) { // 425 has a bug
		dev_err(di->dev, "timed out waiting for cfgupdate flag %d\n", active);
		return -EINVAL;
	}

	if (limit - try > 3)
		dev_warn(di->dev, "cfgupdate %d, retries %d\n", active, limit - try);

	return 0;
}

static inline int bq27xxx_battery_set_cfgupdate(struct bq27xxx_device_info *di)
{
	int ret = bq27xxx_battery_cfgupdate_priv(di, true);

	if (ret < 0 && ret != -EINVAL)
		dev_err(di->dev, "bus error on set_cfgupdate: %d\n", ret);

	return ret;
}

static inline int bq27xxx_battery_soft_reset(struct bq27xxx_device_info *di)
{
	int ret = bq27xxx_battery_cfgupdate_priv(di, false);

	if (ret < 0 && ret != -EINVAL)
		dev_err(di->dev, "bus error on soft_reset: %d\n", ret);

	return ret;
}

static int bq27xxx_read_manfucture_block(struct bq27xxx_device_info *di, u8 *read_cmd, u8 *block_buf)
{
	int ret = -1;

	/* send unseal command */
	pr_debug("%s: start unseal\n", __func__);
	ret = bq27xxx_battery_unseal(di);
	if (ret < 0)
		return ret;
	pr_debug("%s: unseal complete\n", __func__);

	/* send read command */
	ret = bq27xxx_write_block(di, BQ27XXX_DM_CLASS, read_cmd, 2);
	if (ret < 0) {
		dev_err(di->dev, "%s: send read command failed(%d)\n", __func__, ret);
		return ret;
	}

	ret = bq27xxx_read_block(di, BQ27XXX_DM_CLASS, block_buf, BQ27XXX_DM_SZ);
	if (ret < 0) {
		dev_err(di->dev, "%s: read block failed(%d)\n", __func__, ret);
		if (bq27xxx_battery_seal(di) < 0)
			dev_err(di->dev, "bus error on seal\n");
		return ret;
	}

	/* send seal command */
	ret = bq27xxx_battery_seal(di);
	if (ret < 0)
		dev_err(di->dev, "bus error on seal ret: %d\n", ret);

	return ret;
}

static int bq27xxx_find_checksum(u8 *data, int buf_size)
{
	int sum = 0;
	int i = 0;

	for (i = 0; i < buf_size; i++)
		sum += data[i];

	sum = ~sum;
	pr_debug("%s: Checksum value : %d\n", __func__, sum);
	return sum;
}

static void bq27xxx_write_manfucture_block(struct bq27xxx_device_info *di, u8 *block_buf, int buf_size)
{
	u8 cksum[2];
	int csum = 0;
	int ret = -1;
	/* send unseal command */
	pr_debug("%s: start unseal\n", __func__);
	if (bq27xxx_battery_unseal(di) < 0)
		return;
	pr_debug("%s: unseal complete\n", __func__);

	/*Write block data*/
	pr_debug("%s: data write start\n", __func__);
	ret = bq27xxx_write_block(di, BQ27XXX_DM_CLASS, block_buf, buf_size);
	pr_debug("%s: data write complete, ret = %d\n", __func__, ret);

	/*Calculate checksum*/
	csum = bq27xxx_find_checksum(block_buf, buf_size);
	cksum[0] = csum;
	cksum[1] = buf_size + 2;

	/*Write Checksum Data*/
	pr_debug("%s: data checksum write start\n", __func__);
	ret = bq27xxx_write_block(di, BQ27XXX_DM_CKSUM, cksum, 2);
	pr_debug("%s: data checksum write complete, ret = %d\n", __func__, ret);

	/*Send Seal Command */
	if (bq27xxx_battery_seal(di) < 0)
		return;
	pr_debug("%s: seal complete\n", __func__);

}

static int bq27xxx_read_qr_code(struct bq27xxx_device_info *di, char *buf)
{
	u8 block_buf[BQ27XXX_DM_SZ];
	u8 read_cmd[2];

	int ret = -1;
	/* Read Address for qr_code*/
	read_cmd[0] = 0x70;
	read_cmd[1] = 0x0;

	/* Read Block*/
	ret = bq27xxx_read_manfucture_block(di, read_cmd, block_buf);
	if(ret < 0) {
		dev_err(di->dev, "error read block ret : %d", ret);
		return ret;
	}
	memcpy(buf, block_buf + 2, 28);

	pr_info("%s: qrcode read succesfully", __func__);
	return ret;
}

static int bq27xxx_read_manufacture_date(struct bq27xxx_device_info *di, char *buf)
{
	u8 block_buf[BQ27XXX_DM_SZ];
	u8 read_cmd[2];

	int ret = -1;
	int byteData16 = 0;
	int day, month, year;

	/* Read Address for Manufacture date*/
	read_cmd[0] = 0x4D;
	read_cmd[1] = 0x0;

	/* Read Block*/
	ret = bq27xxx_read_manfucture_block(di, read_cmd, block_buf);

	/* Calculate Day , Month , Year from Fetched data */
	byteData16 = (block_buf[3] << 8) + block_buf[2];
	day = byteData16 & 0x001F;
	month = (byteData16 >> 5) & 0x000F;
	year = (byteData16 >> 9) + 1980;
	snprintf(buf, 9, "%04d%02d%02d", year, month, day);

	pr_info("%s: date: %s", __func__, buf);
	return ret;
}

#if defined(CONFIG_ENG_BATTERY_CONCEPT)
static void bq27xxx_reset_mem(struct bq27xxx_device_info *di)
{
	u8 reset_buf[BQ27XXX_DM_SZ] = {0x7A, 0x0, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
								0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E,
								0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
								0x77, 0x7A, 0x78, 0x79, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35};

	/* Write default data to Manufacture Block B */
	bq27xxx_write_manfucture_block(di, reset_buf, BQ27XXX_DM_SZ);
	pr_debug("%s: reset mem done\n", __func__);

#if DEBUG
	/* READ data */
	{
		u8 block_buf[BQ27XXX_DM_SZ] = {0, };
		u8 read_cmd[2] = {0x7A, 0x0};
		int i = 0;
		int ret = -1;

		memset(block_buf, 0xFF, sizeof(block_buf)); //Temporary memset

		/* Read Block*/
		ret = bq27xxx_read_manfucture_block(di, read_cmd, block_buf);
		pr_info("%s: block_buf data after read_cmd ret = %d ", __func__, ret);
		/* Display Read data from block */
		for (i = 0; i < BQ27XXX_DM_SZ; i++)
			pr_info("%x", block_buf[i]);
	}
#endif
}
#endif

static int bq27xxx_get_firstUseDate(struct bq27xxx_device_info *di, char *buf)
{
	u8 block_buf[BQ27XXX_DM_SZ];
	u8 read_cmd[2];
	int ret = -1;

	/*read manufacture block*/
	read_cmd[0] = 0x7A;
	read_cmd[1] = 0x0;
	ret = bq27xxx_read_manfucture_block(di, read_cmd, block_buf);
	if (ret < 0)
		dev_err(di->dev, "error read block ret : %d", ret);

	snprintf(buf, 9, "%s", block_buf + 2);

	return ret;
}

static void bq27xxx_set_firstUseDate(struct bq27xxx_device_info *di, char *buf)
{
	u8 block_buf[BQ27XXX_DM_SZ];
	u8 read_cmd[2];
	int ret = -1;

	/* Read Block */
	read_cmd[0] = 0x7A;
	read_cmd[1] = 0x0;
	ret = bq27xxx_read_manfucture_block(di, read_cmd, block_buf);
	pr_debug("%s: Complete read", __func__);
	if (ret < 0)
		dev_err(di->dev, "error read block : %d", ret);
	block_buf[0] = 0x7A;
	block_buf[1] = 0x0;
	memcpy(block_buf + 2, buf, 8);

	/* Write First use date on block */
	bq27xxx_write_manfucture_block(di, block_buf, BQ27XXX_DM_SZ);
	pr_debug("%s: Complete write", __func__);
}


static int bq27xxx_battery_write_dm_block(struct bq27xxx_device_info *di,
					  struct bq27xxx_dm_buf *buf)
{
	bool cfgup = di->opts & BQ27XXX_O_CFGUP;
	int ret;

	if (!buf->dirty)
		return 0;

	if (cfgup) {
		ret = bq27xxx_battery_set_cfgupdate(di);
		if (ret < 0)
			return ret;
	}

	ret = bq27xxx_write(di, BQ27XXX_DM_CTRL, 0, true);
	if (ret < 0)
		goto out;

	ret = bq27xxx_write(di, BQ27XXX_DM_CLASS, buf->class, true);
	if (ret < 0)
		goto out;

	ret = bq27xxx_write(di, BQ27XXX_DM_BLOCK, buf->block, true);
	if (ret < 0)
		goto out;

	BQ27XXX_MSLEEP(1);

	ret = bq27xxx_write_block(di, BQ27XXX_DM_DATA, buf->data, BQ27XXX_DM_SZ);
	if (ret < 0)
		goto out;

	ret = bq27xxx_write(di, BQ27XXX_DM_CKSUM,
			    bq27xxx_battery_checksum_dm_block(buf), true);
	if (ret < 0)
		goto out;

	/* DO NOT read BQ27XXX_DM_CKSUM here to verify it! That may cause NVM
	 * corruption on the '425 chip (and perhaps others), which can damage
	 * the chip.
	 */

	if (cfgup) {
		BQ27XXX_MSLEEP(1);
		ret = bq27xxx_battery_soft_reset(di);
		if (ret < 0)
			return ret;
	} else {
		BQ27XXX_MSLEEP(100); /* flash DM updates in <100ms */
	}

	buf->dirty = false;

	return 0;

out:
	if (cfgup)
		bq27xxx_battery_soft_reset(di);

	dev_err(di->dev, "bus error writing chip memory: %d\n", ret);
	return ret;
}

static void bq27xxx_battery_set_config(struct bq27xxx_device_info *di,
				       struct power_supply_battery_info *info)
{
	struct bq27xxx_dm_buf bd = BQ27XXX_DM_BUF(di, BQ27XXX_DM_DESIGN_CAPACITY);
	struct bq27xxx_dm_buf bt = BQ27XXX_DM_BUF(di, BQ27XXX_DM_TERMINATE_VOLTAGE);
	bool updated;

	if (bq27xxx_battery_unseal(di) < 0)
		return;

	if (info->charge_full_design_uah != -EINVAL &&
	    info->energy_full_design_uwh != -EINVAL) {
		bq27xxx_battery_read_dm_block(di, &bd);
		/* assume design energy & capacity are in same block */
		bq27xxx_battery_update_dm_block(di, &bd,
					BQ27XXX_DM_DESIGN_CAPACITY,
					info->charge_full_design_uah / 1000);
		bq27xxx_battery_update_dm_block(di, &bd,
					BQ27XXX_DM_DESIGN_ENERGY,
					info->energy_full_design_uwh / 1000);
	}

	if (info->voltage_min_design_uv != -EINVAL) {
		bool same = bd.class == bt.class && bd.block == bt.block;

		if (!same)
			bq27xxx_battery_read_dm_block(di, &bt);
		bq27xxx_battery_update_dm_block(di, same ? &bd : &bt,
					BQ27XXX_DM_TERMINATE_VOLTAGE,
					info->voltage_min_design_uv / 1000);
	}

	updated = bd.dirty || bt.dirty;

	bq27xxx_battery_write_dm_block(di, &bd);
	bq27xxx_battery_write_dm_block(di, &bt);

	bq27xxx_battery_seal(di);

	if (updated && !(di->opts & BQ27XXX_O_CFGUP)) {
		bq27xxx_write(di, BQ27XXX_REG_CTRL, BQ27XXX_RESET, false);
		BQ27XXX_MSLEEP(300); /* reset time is not documented */
	}
	/* assume bq27xxx_battery_update() is called hereafter */
}

static void bq27xxx_battery_settings(struct bq27xxx_device_info *di)
{
	struct power_supply_battery_info info = {};
	unsigned int min, max;

	if (power_supply_get_battery_info(di->bat, &info) < 0)
		return;

	if (!di->dm_regs) {
		dev_warn(di->dev, "data memory update not supported for chip\n");
		return;
	}

	if (info.energy_full_design_uwh != info.charge_full_design_uah) {
		if (info.energy_full_design_uwh == -EINVAL)
			dev_warn(di->dev, "missing battery:energy-full-design-microwatt-hours\n");
		else if (info.charge_full_design_uah == -EINVAL)
			dev_warn(di->dev, "missing battery:charge-full-design-microamp-hours\n");
	}

	/* assume min == 0 */
	max = di->dm_regs[BQ27XXX_DM_DESIGN_ENERGY].max;
	if (info.energy_full_design_uwh > max * 1000) {
		dev_err(di->dev, "invalid battery:energy-full-design-microwatt-hours %d\n",
			info.energy_full_design_uwh);
		info.energy_full_design_uwh = -EINVAL;
	}

	/* assume min == 0 */
	max = di->dm_regs[BQ27XXX_DM_DESIGN_CAPACITY].max;
	if (info.charge_full_design_uah > max * 1000) {
		dev_err(di->dev, "invalid battery:charge-full-design-microamp-hours %d\n",
			info.charge_full_design_uah);
		info.charge_full_design_uah = -EINVAL;
	}

	min = di->dm_regs[BQ27XXX_DM_TERMINATE_VOLTAGE].min;
	max = di->dm_regs[BQ27XXX_DM_TERMINATE_VOLTAGE].max;
	if ((info.voltage_min_design_uv < min * 1000 ||
	     info.voltage_min_design_uv > max * 1000) &&
	     info.voltage_min_design_uv != -EINVAL) {
		dev_err(di->dev, "invalid battery:voltage-min-design-microvolt %d\n",
			info.voltage_min_design_uv);
		info.voltage_min_design_uv = -EINVAL;
	}

	if ((info.energy_full_design_uwh != -EINVAL &&
	     info.charge_full_design_uah != -EINVAL) ||
	     info.voltage_min_design_uv  != -EINVAL)
		bq27xxx_battery_set_config(di, &info);
}
static int bq27xxx_fg_get_atomic_capacity(struct bq27xxx_device_info *di, unsigned int soc)
{
	pr_info("%s : NOW(%d), OLD(%d)\n", __func__, soc, di->capacity_old);

	if (di->pdata->capacity_calculation_type &
	    SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC) {
		if (di->capacity_old < soc)
			soc = di->capacity_old + 1;
		else if (di->capacity_old > soc)
			soc = di->capacity_old - 1;
	}

	/* updated old capacity */
	di->capacity_old = soc;

	return soc;
}
static int bq27xxx_get_ui_soc(struct bq27xxx_device_info *di, unsigned int soc)
{
	/* (Only for atomic capacity)
	 * In initial time, capacity_old is 0.
	 * and in resume from sleep,
	 * capacity_old is too different from actual soc.
	 * should update capacity_old
	 * by val->intval in booting or resume.
	 */
	if (di->initial_update_of_soc
#if IS_ENABLED(CONFIG_BQ27Z746_I2C_ERR_SW_WA)
		&& !i2c_recovery
#endif
	) {
		di->initial_update_of_soc = false;
		/* updated old capacity */
		di->capacity_old = soc;
		return soc;
	}


	if (di->sleep_initial_update_of_soc) {
		/* updated old capacity in case of resume */
		if (di->is_charging ||
		((!di->is_charging) && (di->capacity_old >= soc))) {
			di->capacity_old = soc;
			di->sleep_initial_update_of_soc = false;
			return soc;
		}
	}

	if (di->pdata->capacity_calculation_type &
		(SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC)) {
		soc = bq27xxx_fg_get_atomic_capacity(di, soc);
	}

	return soc;
}
/*
 * Return the battery State-of-Charge
 * Or < 0 if something fails.
 */
static int bq27xxx_battery_read_soc(struct bq27xxx_device_info *di)
{
	int soc;

	soc = bq27xxx_read(di, BQ27XXX_REG_SOC, false);

	if (soc < 0) {
			dev_err(di->dev, "%s : error reading State-of-Charge\n", __func__);
			return soc;
	}
	dev_info(di->dev, "%s: %d\n", __func__, soc);
#if IS_ENABLED(CONFIG_BQ27Z746_I2C_ERR_SW_WA)
	prev_soc = soc;
#endif
	return soc;
}

static int bq27xxx_get_raw_soc(struct bq27xxx_device_info *di)
{
	int soc = bq27xxx_battery_read_soc(di);

	/* capacity should be between 0% and 100% */
	if (soc > 100)
		soc = 100;
	if (soc < 0)
		soc = 0;

	dev_info(di->dev, "%s: %d\n", __func__, soc);

	return soc;
}

/*
 * Return a battery charge value in µAh
 * Or < 0 if something fails.
 */
static int bq27xxx_battery_read_charge(struct bq27xxx_device_info *di, u8 reg)
{
	int charge;

	charge = bq27xxx_read(di, reg, false);
	if (charge < 0) {
		dev_dbg(di->dev, "%s : error reading charge register %02x: %d\n",
			__func__, reg, charge);
		return charge;
	}

	if (reg == BQ27XXX_REG_RC)
		dev_info(di->dev, "%s : bq27xxx_battery_read_RemainingCapacity: %d\n", __func__, charge);
	else if (reg == BQ27XXX_REG_FCC)
		dev_info(di->dev, "%s : bq27xxx_battery_read_FullCapacity: %d\n", __func__, charge);

	return charge;
}

/*
 * Return the battery Nominal available capacity in µAh
 * Or < 0 if something fails.
 */
static inline int bq27xxx_battery_read_nac(struct bq27xxx_device_info *di)
{
	return bq27xxx_battery_read_charge(di, BQ27XXX_REG_NAC);
}

/*
 * Return the battery Remaining Capacity in mAh
 * Or < 0 if something fails.
 */
static inline int bq27xxx_battery_read_rc(struct bq27xxx_device_info *di)
{
	return bq27xxx_battery_read_charge(di, BQ27XXX_REG_RC);
}

/*
 * Return the battery Full Charge Capacity in µAh
 * Or < 0 if something fails.
 */
static inline int bq27xxx_battery_read_fcc(struct bq27xxx_device_info *di)
{
	return bq27xxx_battery_read_charge(di, BQ27XXX_REG_FCC);
}

/*
 * Return the Design Capacity in µAh
 * Or < 0 if something fails.
 */
static int bq27xxx_battery_read_dcap(struct bq27xxx_device_info *di)
{
	int dcap;

	dcap = bq27xxx_read(di, BQ27XXX_REG_DCAP, false);

	if (dcap < 0) {
		dev_dbg(di->dev, "%s : error reading initial last measured discharge\n", __func__);
		return dcap;
	}

	dev_info(di->dev, "%s : bq27xxx_battery_design_capacity: %d\n", __func__, dcap);
	return dcap;
}

/*
 * Return the battery Available energy in µWh
 * Or < 0 if something fails.
 */
/*
 * static int bq27xxx_battery_read_energy(struct bq27xxx_device_info *di)
 * {
 * int ae;
 * ae = bq27xxx_read(di, BQ27XXX_REG_AE, false);
 * if (ae < 0) {
 * dev_dbg(di->dev, "error reading available energy\n");
 * return ae;
 * }
 * if (di->opts & BQ27XXX_O_ZERO)
 * ae *= BQ27XXX_POWER_CONSTANT / BQ27XXX_RS;
 * else
 * ae *= 1000;
 * return ae;
 * }
 */
#if IS_ENABLED(CONFIG_BQ27Z746_LOW_TEMP_COMP)
static int bq27xxx_read_term_voltage(struct bq27xxx_device_info *di)
{
	int ret;

	ret = bq27xxx_read(di, BQ27XXX_REG_TV, false);
	if (ret < 0) {
		dev_err(di->dev, "%s : error reading term voltage %d\n", __func__, ret);
		return ret;
	}

	pr_debug("%s: vterm(%d)\n", __func__, ret);
	return ret;
}
static int bq27xxx_update_term_voltage(struct bq27xxx_device_info *di, int volt)
{
	int ret;

	/* send unseal command */
	pr_debug("%s: start unseal\n", __func__);
	ret = bq27xxx_battery_unseal(di);
	if (ret < 0)
		return ret;
	pr_debug("%s: unseal complete\n", __func__);

	ret = bq27xxx_write(di, BQ27XXX_REG_TV, volt, false);
	pr_info("%s: Term voltage write complete %d\n", __func__, ret);

	if (ret < 0) {
		dev_err(di->dev, "bus error on write: %d\n", ret);
		return ret;
	}
	temperatureComp = !temperatureComp;
		/*Send Seal Command */
	ret = bq27xxx_battery_seal(di);
	if (ret < 0)
		return ret;
	pr_debug("%s: seal complete %d\n", __func__, ret);
	
	return ret;
} 
#endif
/*
 * Return the battery temperature in degree Celsius
 * Or < 0 if something fails.
 */
static int bq27xxx_battery_read_temperature(struct bq27xxx_device_info *di)
{
	int temp;
	temp = bq27xxx_read(di, BQ27XXX_REG_TEMP, false);
	if (temp < 0) {
			dev_err(di->dev, "%s : error reading temperature\n", __func__);
			return temp;
	}

/*
 * if (di->opts & BQ27XXX_O_ZERO)
 * temp = 5 * temp / 2;
 */
	temp = temp * 10; /* in Kelvin */
	temp = temp - 27315; /* in Celsius */
	temp = temp / 10;
	dev_info(di->dev, "%s:temp(%03d)\n", __func__, temp);
#if IS_ENABLED(CONFIG_BQ27Z746_I2C_ERR_SW_WA)
	prev_temp = temp;
#endif
	return temp;
}
/*
 * Return internal temperature of the device
 */
static int bq27xxx_battery_read_internal_temperature(struct bq27xxx_device_info *di)
{
	int temp;

	temp = bq27xxx_read(di, BQ27XXX_REG_INT_TEMP, false);
	if (temp < 0) {
		dev_err(di->dev, "%s : error reading temperature\n", __func__);
		return temp;
	}

/*
 * if (di->opts & BQ27XXX_O_ZERO)
 * temp = 5 * temp / 2;
 */
	temp = temp * 10; /* in Kelvin */
	temp = temp - 27315; /* in Celsius */
	temp = temp / 10;
	dev_info(di->dev, "%s : bq27xxx_battery_read_InternalTemperature: %03d\n", __func__, temp);
#if IS_ENABLED(CONFIG_BQ27Z746_I2C_ERR_SW_WA)
	prev_intTemp = temp;
#endif
	return temp;
}

/*
 * Return the battery Cycle count total
 * Or < 0 if something fails.
 */
static int bq27xxx_battery_read_cyct(struct bq27xxx_device_info *di)
{
	int cyct;

	cyct = bq27xxx_read(di, BQ27XXX_REG_CYCT, false);
	if (cyct < 0) {
		dev_err(di->dev, "%s : error reading cycle count total\n", __func__);
		return cyct;
	}
	dev_info(di->dev, "%s : bq27xxx_battery_read_CycleCount: %d\n", __func__, cyct);
#if IS_ENABLED(CONFIG_BQ27Z746_I2C_ERR_SW_WA)
	prev_cyc = cyct;
#endif
	return cyct;
}

/*
 * Read a time register.
 * Return < 0 if something fails.
 */
static int bq27xxx_battery_read_time(struct bq27xxx_device_info *di, u8 reg)
{
	int tval;

	tval = bq27xxx_read(di, reg, false);
	if (tval < 0 || tval > 65535) {
		dev_dbg(di->dev, "%s : error reading time register %02x: %d\n",
			__func__, reg, tval);
		return tval;
	}
	if (reg == BQ27XXX_REG_TTE)
		dev_info(di->dev, "%s : battery_AverageTimeToEmpty %d min\n", __func__, tval);
	else if (reg == BQ27XXX_REG_TTF)
		dev_info(di->dev, "%s : battery_AverageTimeToFull %d min\n", __func__, tval);

	return tval;
}
#if IS_ENABLED(CONFIG_SEC_FACTORY)
static void bq27xxx_reset_cycleCount(struct bq27xxx_device_info *di)
{
	u8 block_buf[3];

	memset(block_buf, 0x00, sizeof(block_buf));
	block_buf[0] = 0xC0;
	block_buf[1] = 0x41;

	bq27xxx_write_manfucture_block(di, block_buf, sizeof(block_buf));
	pr_info("%s: Write complete\n", __func__);
}
#endif
/*
 * Returns true if a battery over temperature condition is detected
 */
static bool bq27xxx_battery_overtemp(struct bq27xxx_device_info *di, u16 flags)
{
	if (di->opts & BQ27XXX_O_OTDC)
		return flags & (BQ27XXX_FLAG_OTC | BQ27XXX_FLAG_OTD);
	if (di->opts & BQ27XXX_O_UTOT)
		return flags & BQ27XXX_FLAG_OT;

	return false;
}

/*
 * Returns true if a battery under temperature condition is detected
 */
static bool bq27xxx_battery_undertemp(struct bq27xxx_device_info *di, u16 flags)
{
	if (di->opts & BQ27XXX_O_UTOT)
		return flags & BQ27XXX_FLAG_UT;

	return false;
}

/*
 * Returns true if a low state of charge condition is detected
 */
static bool bq27xxx_battery_dead(struct bq27xxx_device_info *di, u16 flags)
{
	if (di->opts & BQ27XXX_O_ZERO)
		return flags & (BQ27000_FLAG_EDV1 | BQ27000_FLAG_EDVF);
	else if (di->opts & BQ27Z746_O_BITS)
		return flags & BQ27Z746_FLAG_FDC;
	else
		return flags & (BQ27XXX_FLAG_SOC1 | BQ27XXX_FLAG_SOCF);
}

/*
 * Returns true if reported battery capacity is inaccurate
 */
static bool bq27xxx_battery_capacity_inaccurate(struct bq27xxx_device_info *di,
						 u16 flags)
{
	if (di->opts & BQ27XXX_O_HAS_CI)
		return (flags & BQ27000_FLAG_CI);
	else
		return false;
}

static int bq27xxx_battery_read_health(struct bq27xxx_device_info *di)
{
	/* Unlikely but important to return first */
	if (unlikely(bq27xxx_battery_overtemp(di, di->cache.flags)))
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	if (unlikely(bq27xxx_battery_undertemp(di, di->cache.flags)))
		return POWER_SUPPLY_HEALTH_COLD;
	if (unlikely(bq27xxx_battery_dead(di, di->cache.flags)))
		return POWER_SUPPLY_HEALTH_DEAD;
	if (unlikely(bq27xxx_battery_capacity_inaccurate(di, di->cache.flags)))
		return POWER_SUPPLY_HEALTH_CALIBRATION_REQUIRED;

	return POWER_SUPPLY_HEALTH_GOOD;
}

void bq27xxx_battery_update(struct bq27xxx_device_info *di)
{
	struct bq27xxx_reg_cache cache = {0, };
	bool has_singe_flag = di->opts & BQ27XXX_O_ZERO;

	cache.flags = bq27xxx_read(di, BQ27XXX_REG_FLAGS, has_singe_flag);
	if ((cache.flags & 0xff) == 0xff)
		cache.flags = -1; /* read error */
	if (cache.flags >= 0) {
		cache.temperature = bq27xxx_battery_read_temperature(di);
		if (di->regs[BQ27XXX_REG_TTE] != INVALID_REG_ADDR)
			cache.time_to_empty = bq27xxx_battery_read_time(di, BQ27XXX_REG_TTE);
		if (di->regs[BQ27XXX_REG_TTF] != INVALID_REG_ADDR)
			cache.time_to_full = bq27xxx_battery_read_time(di, BQ27XXX_REG_TTF);

		cache.charge_full = bq27xxx_battery_read_fcc(di);
		cache.capacity = bq27xxx_battery_read_soc(di);
		di->cache.flags = cache.flags;
		cache.health = bq27xxx_battery_read_health(di);
		if (di->regs[BQ27XXX_REG_CYCT] != INVALID_REG_ADDR)
			cache.cycle_count = bq27xxx_battery_read_cyct(di);

		/* We only have to read charge design full once */
		if (di->charge_design_full <= 0)
			di->charge_design_full = bq27xxx_battery_read_dcap(di);
	}

	if ((di->cache.capacity != cache.capacity) ||
	    (di->cache.flags != cache.flags))
		power_supply_changed(di->bat);

	if (memcmp(&di->cache, &cache, sizeof(cache)) != 0)
		di->cache = cache;

	di->last_update = jiffies;
}
EXPORT_SYMBOL_GPL(bq27xxx_battery_update);

static void bq27xxx_battery_poll(struct work_struct *work)
{
	struct bq27xxx_device_info *di =
			container_of(work, struct bq27xxx_device_info,
				     work.work);

	bq27xxx_battery_update(di);

	if (poll_interval > 0)
		schedule_delayed_work(&di->work, poll_interval * HZ);
}

/*
static bool bq27xxx_battery_is_full(struct bq27xxx_device_info *di, int flags)
{
	if (di->opts & BQ27XXX_O_ZERO)
		return (flags & BQ27000_FLAG_FC);
	else if (di->opts & BQ27Z746_O_BITS)
		return (flags & BQ27Z746_FLAG_FC);
	else
		return (flags & BQ27XXX_FLAG_FC);
}
*/
static int bq27xxx_battery_Qmaxcycle(struct bq27xxx_device_info *di)
{
	int cycle;

	cycle = bq27xxx_read(di, BQ27XXX_REG_QMAX, false);
	if (cycle < 0) {
		dev_err(di->dev, "%s : error reading cycle count total\n", __func__);
		return cycle;
	}
	dev_info(di->dev, "%s : battery_qmax_cycle %d\n", __func__, cycle);
	return cycle;
}

// Return instantaneous current flow through the sense resistor
static int bq27xxx_battery_instant_current(struct bq27xxx_device_info *di)
{
	int curr;

	curr = bq27xxx_read(di, BQ27XXX_REG_INSTCURR, false);
	if (curr < -32768 || curr > 32767) {
		dev_err(di->dev, "%s : error reading current\n", __func__);
		return curr;
	}
	dev_info(di->dev, "%s : battery_instant_current %d\n", __func__, curr);
#if IS_ENABLED(CONFIG_BQ27Z746_I2C_ERR_SW_WA)
	prev_current = curr;
#endif
	return curr;
}

static int bq27xxx_battery_average_current(struct bq27xxx_device_info *di)
{
	int curr;

	curr = bq27xxx_read(di, BQ27XXX_REG_AI, false);
	if (curr < -32767 || curr > 32768) {
		dev_err(di->dev, "%s : error reading current\n", __func__);
		return curr;
	}
	dev_info(di->dev, "%s battery_average_current %d\n", __func__, curr);
#if IS_ENABLED(CONFIG_BQ27Z746_I2C_ERR_SW_WA)
	prev_ai = curr;
#endif
	return curr;
}

static int bq27xxx_battery_max_load_current(struct bq27xxx_device_info *di)
{
	int curr;

	curr = bq27xxx_read(di, BQ27XXX_REG_MLI, false);
	dev_info(di->dev, "%s : battery_max_load_current %d\n", __func__, curr);
	return curr;
}

static int bq27xxx_battery_max_load_time_empty(struct bq27xxx_device_info *di)
{
	int tval;

	tval = bq27xxx_read(di, BQ27XXX_REG_MLTTE, false);
	if (tval < 0 || tval > 65535) {
		dev_err(di->dev, "%s : error reading time\n", __func__);
		return tval;
	}
	dev_info(di->dev, "%s : battery_max_load_time_empty %d\n", __func__, tval);
	return tval;
}
static int bq27xxx_battery_state_of_health(struct bq27xxx_device_info *di)
{
	int soh;

	soh = bq27xxx_read(di, BQ27XXX_REG_SOH, false);
	if (soh < 0 || soh > 100) {
		dev_err(di->dev, "%s : error reading soh\n", __func__);
		return soh;
	}
	dev_info(di->dev, "%s : battery_state_of_health %d\n", __func__, soh);
#if IS_ENABLED(CONFIG_BQ27Z746_I2C_ERR_SW_WA)
	prev_soh = soh;
#endif
	return soh;
}
static int bq27xxx_battery_charging_voltage(struct bq27xxx_device_info *di)
{
	int cv;

	cv = bq27xxx_read(di, BQ27XXX_REG_CV, false);
	if (cv < 0 || cv > 32767) {
		dev_err(di->dev, "%s : error reading cv\n", __func__);
		return cv;
	}
	dev_info(di->dev, "%s : battery_charging_voltage %d\n", __func__, cv);
	return cv;
}

static int bq27xxx_battery_charging_current(struct bq27xxx_device_info *di)
{
	int cc;

	cc = bq27xxx_read(di, BQ27XXX_REG_CC, false);
	if (cc < 0 || cc > 32767) {
		dev_err(di->dev, "%s : error reading cc\n", __func__);
		return cc;
	}
	dev_info(di->dev, "%s : battery_charging_current %d\n", __func__, cc);
	return cc;
}

/*
 * Return the battery average current in µA and the status
 * Note that current can be negative signed as well
 * Or 0 if something fails.
 */
/*
static int bq27xxx_battery_current_and_status(
	struct bq27xxx_device_info *di,
	union power_supply_propval *val_curr,
	union power_supply_propval *val_status)
{
	bool single_flags = (di->opts & BQ27XXX_O_ZERO);
	int curr;
	int flags;

	curr = bq27xxx_read(di, BQ27XXX_REG_AI, false);
	if (curr < 0) {
		dev_err(di->dev, "error reading current\n");
		return curr;
	}

	flags = bq27xxx_read(di, BQ27XXX_REG_FLAGS, single_flags);
	if (flags < 0) {
		dev_err(di->dev, "error reading flags\n");
		return flags;
	}

	if (di->opts & BQ27XXX_O_ZERO) {
		if (!(flags & BQ27000_FLAG_CHGS)) {
			dev_dbg(di->dev, "negative current!\n");
			curr = -curr;
		}

		curr = curr * BQ27XXX_CURRENT_CONSTANT / BQ27XXX_RS;
	} else {
		curr = (int)((s16)curr) * 1000;
	}

	if (val_curr)
		val_curr->intval = curr;

	if (val_status) {
		if (curr > 0) {
			val_status->intval = POWER_SUPPLY_STATUS_CHARGING;
		} else if (curr < 0) {
			val_status->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		} else {
			if (bq27xxx_battery_is_full(di, flags))
				val_status->intval = POWER_SUPPLY_STATUS_FULL;
			else
				val_status->intval =
					POWER_SUPPLY_STATUS_NOT_CHARGING;
		}
	}

	return 0;
}
*/

/*
 * Get the average power in mW
 * Return < 0 if something fails.
 */
static int bq27xxx_battery_pwr_avg(struct bq27xxx_device_info *di)
{
	int power;

	power = bq27xxx_read(di, BQ27XXX_REG_AP, false);
	if (power < -32768 || power > 32767) {
		dev_err(di->dev,
			"%s : error reading average power register %02x: %d\n",
			__func__, BQ27XXX_REG_AP, power);
		return power;
	}
	dev_info(di->dev, "%s : battery_average_power %d\n", __func__, power);
	return power;
}

/*
static int bq27xxx_battery_capacity_level(struct bq27xxx_device_info *di,
					  union power_supply_propval *val)
{
	int level;

	if (di->opts & BQ27XXX_O_ZERO) {
		if (di->cache.flags & BQ27000_FLAG_FC)
			level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		else if (di->cache.flags & BQ27000_FLAG_EDV1)
			level = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
		else if (di->cache.flags & BQ27000_FLAG_EDVF)
			level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
		else
			level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	} else if (di->opts & BQ27Z746_O_BITS) {
		if (di->cache.flags & BQ27Z746_FLAG_FC)
			level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		else if (di->cache.flags & BQ27Z746_FLAG_FDC)
			level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
		else
			level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	} else {
		if (di->cache.flags & BQ27XXX_FLAG_FC)
			level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		else if (di->cache.flags & BQ27XXX_FLAG_SOC1)
			level = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
		else if (di->cache.flags & BQ27XXX_FLAG_SOCF)
			level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
		else
			level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	}

	val->intval = level;

	return 0;
}
*/

/*
 * Return the battery Voltage in millivolts
 * Or < 0 if something fails.
 */
static int bq27xxx_battery_voltage(struct bq27xxx_device_info *di)
{
	int volt;

	volt = bq27xxx_read(di, BQ27XXX_REG_VOLT, false);
	if (volt < 0) {
			dev_err(di->dev, "%s : error reading voltage\n", __func__);
			return volt;
	}
	dev_info(di->dev, "%s : reading voltage %d\n", __func__, volt);
#if IS_ENABLED(CONFIG_BQ27Z746_I2C_ERR_SW_WA)
	prev_volt = volt;
#endif
	return volt;
}

static struct device_attribute bq27xxx_attrs[] = {
	BQ27XXX_ATTR(soc),
	BQ27XXX_ATTR(cycle),
	BQ27XXX_ATTR(q_max_cycles),
	BQ27XXX_ATTR(instant_current),
	BQ27XXX_ATTR(average_current),
	BQ27XXX_ATTR(charging_current),
	BQ27XXX_ATTR(max_load_current),
	BQ27XXX_ATTR(charging_voltage),
	BQ27XXX_ATTR(battery_voltage),
	BQ27XXX_ATTR(average_power),
	BQ27XXX_ATTR(design_capacity),
	BQ27XXX_ATTR(remaining_capacity),
	BQ27XXX_ATTR(full_charge_capacity),
	BQ27XXX_ATTR(battery_state_of_health),
	BQ27XXX_ATTR(average_time_to_empty),
	BQ27XXX_ATTR(average_time_to_full),
	BQ27XXX_ATTR(max_load_time_to_empty),
	BQ27XXX_ATTR(internal_temperature),
	BQ27XXX_ATTR(battery_temperature),
	BQ27XXX_ATTR(manufacture_date),
	BQ27XXX_ATTR(first_use_date),
	BQ27XXX_ATTR(qr_code),
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	BQ27XXX_ATTR(reset_mem),
#endif
};

static int bq27xxx_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < (int)ARRAY_SIZE(bq27xxx_attrs); i++) {
		rc = device_create_file(dev, &bq27xxx_attrs[i]);
		if (rc)
			break;
	}

	if (rc) {
		dev_err(dev, "%s: failed (%d)\n", __func__, rc);
		while (i--)
			device_remove_file(dev, &bq27xxx_attrs[i]);
	}
	return rc;
}

ssize_t bq27xxx_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct bq27xxx_device_info *di = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - bq27xxx_attrs;
	union power_supply_propval value = {0, };
	int i = 0;

	switch (offset) {
	case SOC:
		value.intval = bq27xxx_battery_read_soc(di);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case CYCLE:
		value.intval = bq27xxx_battery_read_cyct(di);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case Q_MAX_CYCLES:
		value.intval = bq27xxx_battery_Qmaxcycle(di);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case INSTANT_CURRENT:
		value.intval = bq27xxx_battery_instant_current(di);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case AVERAGE_CURRENT:
		value.intval = bq27xxx_battery_average_current(di);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case CHARGING_CURRENT:
		value.intval = bq27xxx_battery_charging_current(di);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case MAX_LOAD_CURRENT:
		value.intval = bq27xxx_battery_max_load_current(di);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case CHARGING_VOLTAGE:
		value.intval = bq27xxx_battery_charging_voltage(di);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case BATTERY_VOLTAGE:
		value.intval = bq27xxx_battery_voltage(di);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case AVERAGE_POWER:
		value.intval = bq27xxx_battery_pwr_avg(di);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case DESIGN_CAPACITY:
		value.intval = bq27xxx_battery_read_dcap(di);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case REMAINING_CAPACITY:
		value.intval = bq27xxx_battery_read_charge(di, BQ27XXX_REG_RC);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case FULL_CHARGE_CAPACITY:
		value.intval = bq27xxx_battery_read_charge(di, BQ27XXX_REG_FCC);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case BATTERY_STATE_OF_HEALTH:
		value.intval = bq27xxx_battery_state_of_health(di);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case AVERAGE_TIME_TO_EMPTY:
		value.intval = bq27xxx_battery_read_time(di, BQ27XXX_REG_TTE);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case AVERAGE_TIME_TO_FULL:
		value.intval = bq27xxx_battery_read_time(di, BQ27XXX_REG_TTF);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case MAX_LOAD_TIME_TO_EMPTY:
		value.intval = bq27xxx_battery_max_load_time_empty(di);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case INTERNAL_TEMPERATURE:
		value.intval = bq27xxx_battery_read_internal_temperature(di);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case BATTERY_TEMPERATURE:
		value.intval = bq27xxx_battery_read_temperature(di);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case MANUFACTURE_DATE:
	{
		char temp_buf[9] = {0, };

		value.intval = bq27xxx_read_manufacture_date(di, temp_buf);
		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
				temp_buf);

		break;
	}
	case QR_CODE:
	{
		char temp_buf[29] = {0, };

		value.intval = bq27xxx_read_qr_code(di, temp_buf);
		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
				temp_buf);
		break;
	}
	case FIRST_USE_DATE:
	{
		char temp_buf[9] = {0, };

		value.intval = bq27xxx_get_firstUseDate(di, temp_buf);
		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
				temp_buf);

		break;
	}
	default:
		i = -EINVAL;
		break;
	}
	return i;
}

ssize_t bq27xxx_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{

	struct power_supply *psy = dev_get_drvdata(dev);
	struct bq27xxx_device_info *di = power_supply_get_drvdata(psy);

	const ptrdiff_t offset = attr - bq27xxx_attrs;
	int ret = -EINVAL;

	switch (offset) {
	case SOC:
		break;
	case CYCLE:
	{
#if IS_ENABLED(CONFIG_SEC_FACTORY)
		bq27xxx_reset_cycleCount(di);
		ret = count;
#endif
		break;
	}
	case Q_MAX_CYCLES:
		break;
	case INSTANT_CURRENT:
		break;
	case AVERAGE_CURRENT:
		break;
	case CHARGING_CURRENT:
		break;
	case MAX_LOAD_CURRENT:
		break;
	case CHARGING_VOLTAGE:
		break;
	case BATTERY_VOLTAGE:
		break;
	case AVERAGE_POWER:
		break;
	case DESIGN_CAPACITY:
		break;
	case REMAINING_CAPACITY:
		break;
	case FULL_CHARGE_CAPACITY:
		break;
	case BATTERY_STATE_OF_HEALTH:
		break;
	case AVERAGE_TIME_TO_EMPTY:
		break;
	case AVERAGE_TIME_TO_FULL:
		break;
	case MAX_LOAD_TIME_TO_EMPTY:
		break;
	case INTERNAL_TEMPERATURE:
		break;
	case BATTERY_TEMPERATURE:
		break;
	case FIRST_USE_DATE:
	{
		char temp_buf[8] = {0, };
		if(sscanf(buf,"%s\n",temp_buf) == 1)
		{
			bq27xxx_set_firstUseDate(di, temp_buf);
		}
		ret = count;
		break;
	}
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	case RESET_MEM:
	{
		pr_debug("%s: Reset mem start\n", __func__);
		bq27xxx_reset_mem(di);
		pr_debug("%s: Reset mem end\n", __func__);
		ret = count;
		break;
	}
#endif
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

/*
static int bq27xxx_simple_value(int value,
				union power_supply_propval *val)
{
	if (value < 0)
		return value;

	val->intval = value;

	return 0;
}
*/

static int bq27xxx_battery_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct bq27xxx_device_info *di = power_supply_get_drvdata(psy);

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (!battery_present) {
			val->intval = FAKE_FG_VOLT_OB_MODE;
			pr_info("%s: VNOW(%d): no battery\n", __func__, val->intval);
		} else {
			val->intval = bq27xxx_battery_voltage(di);
		}
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
		if (di->f_mode == OB_MODE) {
			val->intval = FAKE_FG_VOLT_OB_MODE;
			pr_info("%s: Voltage changed to (%d) due to OB mode\n", __func__, val->intval);
		}
#else
		if (factory_mode) {
			val->intval = FAKE_FG_VOLT_OB_MODE;
			pr_info("%s: Voltage changed to (%d) due to factory mode\n", __func__, val->intval);
		}
#endif
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		if (val->intval == SEC_BATTERY_CURRENT_UA)
			val->intval = bq27xxx_battery_average_current(di) * 1000;
		else
			val->intval = bq27xxx_battery_average_current(di);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (val->intval == SEC_BATTERY_CURRENT_UA)
			val->intval = bq27xxx_battery_instant_current(di) * 1000;
		else
			val->intval = bq27xxx_battery_instant_current(di);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (!battery_present) {
			val->intval = FAKE_FG_SOC_OB_MODE;
			pr_info("%s: SOC(%d): no battery\n", __func__, val->intval);
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
		} else if (di->f_mode == OB_MODE) {
			val->intval = FAKE_FG_SOC_OB_MODE;
			pr_info("%s: SOC set to (%d) due to OB mode\n", __func__, val->intval);
#else
		} else if (factory_mode) {
			val->intval = FAKE_FG_SOC_OB_MODE;
			pr_info("%s: SOC set to (%d) due to factory mode\n", __func__, val->intval);
#endif
		} else {
			if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RAW ||
				val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE) {
				/* Raw SOC must be 0.1% degree */
				val->intval = bq27xxx_get_raw_soc(di) * 10;
			} else {
				/* Get UI SOC */
				val->intval = bq27xxx_get_raw_soc(di);
				val->intval = bq27xxx_get_ui_soc(di, val->intval);
				pr_info("%s: UI SOC(%d)\n", __func__, val->intval);
			}
		}
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = bq27xxx_battery_read_temperature(di);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		val->intval = bq27xxx_battery_read_time(di, BQ27XXX_REG_TTE);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		val->intval = bq27xxx_battery_read_time(di, BQ27XXX_REG_TTF);
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		val->intval = bq27xxx_battery_read_charge(di, BQ27XXX_REG_RC);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = bq27xxx_battery_read_charge(di, BQ27XXX_REG_FCC);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = bq27xxx_battery_read_dcap(di);
		break;
	case POWER_SUPPLY_PROP_POWER_AVG:
		val->intval = bq27xxx_battery_pwr_avg(di);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = bq27xxx_battery_state_of_health(di);
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = BQ27XXX_MANUFACTURER;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = bq27xxx_battery_read_cyct(di);
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = bq27xxx_battery_read_charge(di, BQ27XXX_REG_RC)*1000;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}


static int bq27xxx_battery_set_property(struct power_supply *psy,
					enum power_supply_property psp,
					const union power_supply_propval *val)
{
	struct bq27xxx_device_info *di = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp =
					(enum power_supply_ext_property)psp;
#if IS_ENABLED(CONFIG_BQ27Z746_LOW_TEMP_COMP)
	int ret, vterm;
#endif
	switch ((int)psp) {
#if IS_ENABLED(CONFIG_BQ27Z746_LOW_TEMP_COMP)
	case POWER_SUPPLY_PROP_TEMP:
		vterm = bq27xxx_read_term_voltage(di);
		if (vterm < 0)
			break;
		if (vterm == 2900)
			temperatureComp = true;
		else if (vterm == 3400)
			temperatureComp = false;
		pr_info("%s: temp(%d), vterm(%d), temperatureComp(%d)\n",
					__func__, val->intval, vterm, temperatureComp);
		if (val->intval <= 0 && !temperatureComp) {
			ret = bq27xxx_update_term_voltage(di, 2900);
			if (ret < 0)
				dev_err(di->dev, "%s : error updating term voltage ,ret = %d", __func__, ret);
		} else if (val->intval >= 20 && temperatureComp) {
			ret = bq27xxx_update_term_voltage(di, 3400);
			if (ret < 0)
				dev_err(di->dev, "%s : error updating term voltage ,ret = %d", __func__, ret);
		}
	break;
#endif
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
		case POWER_SUPPLY_EXT_PROP_BATT_F_MODE:
			di->f_mode = val->intval;
			pr_info("%s : f_mode set from set_prop  = %d\n", __func__, di->f_mode);
			break;
#endif
#if IS_ENABLED(CONFIG_SBP_FG)
		case POWER_SUPPLY_EXT_PROP_FAKE_SOC:
			battery_present = 0;
			pr_info("%s : battery present(%d)\n", __func__, battery_present);
			break;
#endif
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			switch (val->intval) {
			case SEC_BAT_CHG_MODE_CHARGING_OFF:
				di->is_charging = false;
				break;
			case SEC_BAT_CHG_MODE_CHARGING:
			case SEC_BAT_CHG_MODE_PASS_THROUGH:
				di->is_charging = true;
				break;
			};
		break;
		default:
			return -EINVAL;
		}
	break;
	default:
		return -EINVAL;
	}
	pr_debug("%s : set prop called for %s\n", __func__, di->name);
	return 0;
}

static void bq27xxx_external_power_changed(struct power_supply *psy)
{
	struct bq27xxx_device_info *di = power_supply_get_drvdata(psy);

	cancel_delayed_work_sync(&di->work);
	schedule_delayed_work(&di->work, 0);
}
static int bq27xxx_fuelgauge_parse_dt(struct bq27xxx_device_info *di)
{
	struct device_node *np = of_find_node_by_name(NULL, "bq27z746-battery");
	bq27xxx_fuelgauge_platform_data_t *pdata = di->pdata;
	int ret;

	if (np == NULL)
		return -EINVAL;

	ret = of_property_read_u32(np, "fuelgauge,capacity_calculation_type",
					&pdata->capacity_calculation_type);
	if (ret < 0)
		pr_err("%s: error reading capacity_calculation_type %d\n",
			__func__, ret);
	pr_debug("%s: capcity calculation type: %d\n", __func__, pdata->capacity_calculation_type);
	return 0;
}

int bq27xxx_battery_setup(struct bq27xxx_device_info *di)
{
	struct power_supply_desc *psy_desc;
	struct power_supply_config psy_cfg = {
		.of_node = di->dev->of_node,
		.drv_data = di,
	};

	INIT_DELAYED_WORK(&di->work, bq27xxx_battery_poll);
	mutex_init(&di->lock);

	di->regs       = bq27xxx_chip_data[di->chip].regs;
	di->unseal_key = bq27xxx_chip_data[di->chip].unseal_key;
	di->dm_regs    = bq27xxx_chip_data[di->chip].dm_regs;
	di->opts       = bq27xxx_chip_data[di->chip].opts;

	pr_info("%s : unsealKey  = %d , opts = %d\n", __func__, di->unseal_key, di->opts);
	psy_desc = devm_kzalloc(di->dev, sizeof(*psy_desc), GFP_KERNEL);
	if (!psy_desc)
		goto err_mem;

	psy_desc->name = "sbp-fg";
	psy_desc->type = POWER_SUPPLY_TYPE_UNKNOWN;
	psy_desc->properties = bq27xxx_chip_data[di->chip].props;
	psy_desc->num_properties = bq27xxx_chip_data[di->chip].props_size;
	psy_desc->get_property = bq27xxx_battery_get_property;
	psy_desc->set_property = bq27xxx_battery_set_property;
	psy_desc->external_power_changed = bq27xxx_external_power_changed;

	di->bat = power_supply_register(di->dev, psy_desc, &psy_cfg);
	if (IS_ERR(di->bat))
		return dev_err_probe(di->dev, PTR_ERR(di->bat),
				     "failed to register battery\n");

	bq27xxx_create_attrs(&di->bat->dev);
	bq27xxx_battery_settings(di);
	bq27xxx_battery_update(di);

	mutex_lock(&bq27xxx_list_lock);
	list_add(&di->list, &bq27xxx_battery_devices);
	mutex_unlock(&bq27xxx_list_lock);

	return 0;

err_mem:
	mutex_destroy(&di->lock);
	return -ENOMEM;
}
EXPORT_SYMBOL_GPL(bq27xxx_battery_setup);

void bq27xxx_battery_teardown(struct bq27xxx_device_info *di)
{
	/*
	 * power_supply_unregister call bq27xxx_battery_get_property which
	 * call bq27xxx_battery_poll.
	 * Make sure that bq27xxx_battery_poll will not call
	 * schedule_delayed_work again after unregister (which cause OOPS).
	 */
	poll_interval = 0;

	cancel_delayed_work_sync(&di->work);

	power_supply_unregister(di->bat);

	mutex_lock(&bq27xxx_list_lock);
	list_del(&di->list);
	mutex_unlock(&bq27xxx_list_lock);

	mutex_destroy(&di->lock);
}
EXPORT_SYMBOL_GPL(bq27xxx_battery_teardown);

static irqreturn_t bq27xxx_battery_irq_handler_thread(int irq, void *data)
{
	struct bq27xxx_device_info *di = data;

	bq27xxx_battery_update(di);

	return IRQ_HANDLED;
}

static int bq27xxx_battery_i2c_read(struct bq27xxx_device_info *di, u8 reg,
				    bool single)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	struct i2c_msg msg[2];
	u8 data[2];
	short int val;
	signed char val_single;
	int ret;
	int retry_count = RETRY_COUNT;

	if (!client->adapter)
		return -ENODEV;

	dev_dbg(di->dev, "%s: Client Address: %hu\n", __func__, client->addr);
	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].buf = &reg;
	msg[0].len = sizeof(reg);
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = data;
	if (single)
		msg[1].len = 1;
	else
		msg[1].len = 2;
	while (retry_count) {
		ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
		if (ret > 0)
			break;
		pr_err("%s: i2c_transfer retry_count=%d, ret= %d\n", __func__, retry_count, ret);
		retry_count--;
	}
	if (ret < 0) {
#if IS_ENABLED(CONFIG_BQ27Z746_I2C_ERR_SW_WA)
		if(!i2c_recovery && (i2c_fail_time == INVALID_TIME)) {
			
			i2c_recovery = true;
			i2c_fail_time=bq27xxx_get_currenttime();
			pr_info("%s: i2c failed, recovery started\n", __func__);
		}
#endif
#if IS_ENABLED(CONFIG_SEC_ABC)
	sec_abc_send_event("MODULE=battery@WARN=fg_i2c_fail");
#endif
		return ret;
	}
#if IS_ENABLED(CONFIG_BQ27Z746_I2C_ERR_SW_WA)
	/* mark fail_time invalid when i2c read is success*/
	i2c_fail_time = INVALID_TIME;
#endif

	if (!single) {
		val = get_unaligned_le16(data);
		return (int)val;
	}
	val_single = data[0];
	return (int)val_single;

}

static int bq27xxx_battery_i2c_write(struct bq27xxx_device_info *di, u8 reg,
				     int value, bool single)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	struct i2c_msg msg;
	u8 data[4];
	int ret;
	int retry_count = RETRY_COUNT;

	if (!client->adapter)
		return -ENODEV;

	data[0] = reg;
	if (single) {
		data[1] = (u8) value;
		msg.len = 2;
	} else {
		put_unaligned_le16(value, &data[1]);
		msg.len = 3;
	}

	msg.buf = data;
	msg.addr = client->addr;
	msg.flags = 0;

	while (retry_count) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret > 0)
			break;
		pr_err("%s: i2c_transfer retry_count=%d, ret= %d\n", __func__, retry_count, ret);
		retry_count--;
	}
	if (ret < 0)
		return ret;
	if (ret != 1)
		return -EINVAL;
	return 0;
}

static int bq27xxx_battery_i2c_bulk_read(struct bq27xxx_device_info *di, u8 reg,
					 u8 *data, int len)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	struct i2c_msg msg[2];
	int ret;
	int retry_count = RETRY_COUNT;

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].buf = &reg;
	msg[0].len = sizeof(reg);
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = data;
	msg[1].len = len;
	while (retry_count) {
		ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
		if (ret > 0)
			break;
		pr_err("%s: i2c_transfer retry_count=%d, ret= %d\n", __func__, retry_count, ret);
		retry_count--;
	}
	if (ret < 0)
		return ret;
	return 0;
}

static int bq27xxx_battery_i2c_bulk_write(struct bq27xxx_device_info *di,
					  u8 reg, u8 *data, int len)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	struct i2c_msg msg;
	/* 1 byte : REG, BQ27XXX_DM_SZ: Block data(32 bytes) + 2 Bytes CMD*/
	u8 buf[1 + BQ27XXX_DM_SZ];
	int ret;
	int retry_count = RETRY_COUNT;

	if (!client->adapter)
		return -ENODEV;

	buf[0] = reg;
	memcpy(&buf[1], data, len);

	msg.buf = buf;
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = len + 1;

	while (retry_count) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret > 0)
			break;
		pr_err("%s: i2c_transfer retry_count=%d, ret= %d\n", __func__, retry_count, ret);
		retry_count--;
	}
	if (ret < 0)
		return ret;
	if (ret != 1)
		return -EINVAL;
	return 0;
}

static void bq27z746_parse_param_value(struct bq27xxx_device_info *di)
{
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	pr_info("%s: command line param f_mode: %s\n", __func__, f_mode);

	if (!f_mode)
		di->f_mode = NO_MODE;
	else if ((strncmp(f_mode, "OB", 2) == 0) || (strncmp(f_mode, "DL", 2) == 0))
		di->f_mode = OB_MODE;
	else if (strncmp(f_mode, "IB", 2) == 0)
		di->f_mode = IB_MODE;
	else
		di->f_mode = NO_MODE;

	pr_info("[Bq27z746] %s: f_mode: %s\n", __func__, BOOT_MODE_STRING[di->f_mode]);
#endif
}

static int bq27xxx_battery_i2c_probe(struct i2c_client *client,
				     const struct i2c_device_id *id)
{
	struct bq27xxx_device_info *di;
	int ret;
	char *name;
	int num;

	pr_info("%s: Started\n", __func__);
	/* Get new ID for the new battery device */
	mutex_lock(&battery_mutex);
	num = idr_alloc(&battery_id, client, 0, 0, GFP_KERNEL);
	mutex_unlock(&battery_mutex);
	if (num < 0)
		return num;

	di = devm_kzalloc(&client->dev, sizeof(*di), GFP_KERNEL);
	di->pdata = devm_kzalloc(&client->dev, sizeof(bq27xxx_fuelgauge_platform_data_t), GFP_KERNEL);

	name = devm_kasprintf(&client->dev, GFP_KERNEL, "%s-%d", id->name, num);

	if (!name)
		goto err_mem;

	if (!di)
		goto err_mem;

	if (!di->pdata)
		goto err_mem;

	di->id = num;
	di->dev = &client->dev;
	di->chip = id->driver_data;
	di->name = name;

	pr_info("%s: line di_chip = %d\n", __func__, di->chip);

	di->bus.read = bq27xxx_battery_i2c_read;
	di->bus.write = bq27xxx_battery_i2c_write;
	di->bus.read_bulk = bq27xxx_battery_i2c_bulk_read;
	di->bus.write_bulk = bq27xxx_battery_i2c_bulk_write;

	ret = bq27xxx_battery_setup(di);
	if (ret)
		goto err_failed;

	bq27z746_parse_param_value(di);

	/* Schedule a polling after about 1 min */
	schedule_delayed_work(&di->work, 60 * HZ);

	i2c_set_clientdata(client, di);
	dev_info(di->dev, "%s: I2C Probe line 1527\n", __func__);

	if (client->irq) {
		ret = devm_request_threaded_irq(&client->dev, client->irq,
				NULL, bq27xxx_battery_irq_handler_thread,
				IRQF_ONESHOT,
				di->name, di);
		if (ret) {
			dev_err(&client->dev,
				"Unable to register IRQ %d error %d\n",
				client->irq, ret);
			bq27xxx_battery_teardown(di);
			goto err_failed;
		}
	}
	dev_info(di->dev, "%s: I2C Probe Completed\n", __func__);
	ret = bq27xxx_fuelgauge_parse_dt(di);
	if (ret < 0)
		pr_err("%s not found fg dt! ret[%d]\n", __func__, ret);

	/* SOC */
	ret = bq27xxx_battery_read_soc(di);
	pr_info("%s: SOC ret = %d\n", __func__, ret);
	/* VCELL */
	ret = bq27xxx_battery_voltage(di);
	pr_info("%s: volt ret = %d\n", __func__, ret);

	di->sleep_initial_update_of_soc = false;
	di->initial_update_of_soc = true;

	sec_chg_set_dev_init(SC_DEV_FG);
	pr_info("%s: bq27z746 Fuelgauge Driver Loaded\n", __func__);

	return 0;

err_mem:
	ret = -ENOMEM;

err_failed:
	mutex_lock(&battery_mutex);
	idr_remove(&battery_id, num);
	mutex_unlock(&battery_mutex);

	return ret;
}

static int bq27xxx_battery_i2c_remove(struct i2c_client *client)
{
	struct bq27xxx_device_info *di = i2c_get_clientdata(client);

	bq27xxx_battery_teardown(di);

	mutex_lock(&battery_mutex);
	idr_remove(&battery_id, di->id);
	mutex_unlock(&battery_mutex);
	return 0;
}

static int bq27xxx_fuelgauge_suspend(struct device *dev)
{
	return 0;
}

static int bq27xxx_fuelgauge_resume(struct device *dev)
{
	struct bq27xxx_device_info *di = dev_get_drvdata(dev);

	di->sleep_initial_update_of_soc = true;
	return 0;
}

static SIMPLE_DEV_PM_OPS(bq27xxx_fuelgauge_pm_ops, bq27xxx_fuelgauge_suspend,
							bq27xxx_fuelgauge_resume);

static struct i2c_driver bq27xxx_battery_i2c_driver = {
	.driver = {
		.name = "bq27xxx-battery",
		.of_match_table = of_match_ptr(bq27xxx_battery_i2c_of_match_table),
#ifdef CONFIG_PM
		.pm = &bq27xxx_fuelgauge_pm_ops,
#endif
	},
	.probe = bq27xxx_battery_i2c_probe,
	.remove = bq27xxx_battery_i2c_remove,
	.id_table = bq27xxx_i2c_id_table,
};

static int __init bq27xxx_fuelgauge_init(void)
{
	pr_info("%s: BQ27XXX Fuelgauge Init\n", __func__);
	return i2c_add_driver(&bq27xxx_battery_i2c_driver);
}

static void __exit bq27xxx_fuelgauge_exit(void)
{
	i2c_del_driver(&bq27xxx_battery_i2c_driver);
}
module_init(bq27xxx_fuelgauge_init);
module_exit(bq27xxx_fuelgauge_exit);

MODULE_DESCRIPTION("Samsung BQ27XXXX Fuel Gauge Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
