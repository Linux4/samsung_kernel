/*
 *  sb_mfc.c
 *  Samsung Mobile SEC Battery MFC Driver
 *
 *  Copyright (C) 2021 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/of_gpio.h>
#include <linux/pm_wakeup.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>

#include <linux/battery/sb_sysfs.h>
#include <linux/battery/sb_notify.h>
#include <linux/battery/sb_mfc.h>

#include <linux/battery/sec_battery_common.h>

#if defined(CONFIG_SEC_KUNIT)
#include <kunit/mock.h>
#else
#define __visible_for_testing static
#endif

#define mfc_log(str, ...) pr_info("[SB-MFC]:%s: "str, __func__, ##__VA_ARGS__)

struct sb_mfc {
	/* dt data */
	int wpc_det_gpio;
	int wpc_en_gpio;

	char *chg_name;
};

static unsigned int __read_mostly wireless_ic;
module_param(wireless_ic, uint, 0444);


int sb_mfc_check_chip_id(int chip_id)
{
	if (WRL_PARAM_GET_CHIP_ID(wireless_ic) == chip_id)
		return CHIP_ID_MATCHED;

	return CHIP_ID_NOT_SET;
}
EXPORT_SYMBOL(sb_mfc_check_chip_id);

static int i2c_read(struct i2c_client *i2c, u16 reg, u8 *val)
{
	int ret;
	struct i2c_msg msg[2];
	u8 wbuf[2];
	u8 rbuf[2];

	msg[0].addr = i2c->addr;
	msg[0].flags = i2c->flags & I2C_M_TEN;
	msg[0].len = 2;
	msg[0].buf = wbuf;

	wbuf[0] = (reg & 0xFF00) >> 8;
	wbuf[1] = (reg & 0xFF);

	msg[1].addr = i2c->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = rbuf;

	ret = i2c_transfer(i2c->adapter, msg, 2);
	if (ret < 0) {
		mfc_log("i2c read error, reg: 0x%x, ret: %d (called by %ps)\n",
			reg, ret, __builtin_return_address(0));
		return ret;
	}

	*val = rbuf[0];
	return ret;
}

static int set_uno(char *chg_name, bool uno)
{
	static bool otg_used;

	union power_supply_propval value = { uno, };
	int ret = 0;

	if (!otg_used) {
		ret = psy_do_property(chg_name, set,
			POWER_SUPPLY_EXT_PROP_CHARGE_UNO_CONTROL, value);\
		if (!ret)
			return 0;

		otg_used = true;
		value.intval = uno;
	}

    return psy_do_property("otg", set,
		POWER_SUPPLY_EXT_PROP_CHARGE_UNO_CONTROL, value);\
}

static int get_chip_id(struct i2c_client *i2c)
{
	int ret = 0, i;
	u8 temp;

	for (i = 0; i < 3; i++) {
		ret = i2c_read(i2c, 0x00, &temp);
		if (ret >= 0) {
			mfc_log("chip_id = 0x%x\n", temp);
			return temp;
		}
	}

	return 0;
}

static int parse_dt(struct sb_mfc *mfc, struct device *dev)
{
#if defined(CONFIG_OF)
	struct device_node *np;

	if (!dev)
		return -EINVAL;

	np = dev->of_node;

	mfc->wpc_det_gpio = of_get_named_gpio(np, "wpc_det_gpio", 0);
	if (mfc->wpc_det_gpio < 0)
		mfc_log("failed to get wpc-det gpio(%d)\n", mfc->wpc_det_gpio);

	mfc->wpc_en_gpio = of_get_named_gpio(np, "wpc_en_gpio", 0);
	if (mfc->wpc_en_gpio < 0)
		mfc_log("failed to get wpc-en gpio(%d)\n", mfc->wpc_en_gpio);

	sb_of_parse_str(np, mfc, chg_name);
#endif
	return 0;
}

static bool check_chip_id(int chip_id)
{
	static int chip_id_list[2] = { 0x20, 0x04 };
	int i;

	if (chip_id == 0)
		return false;

	for (i = 0; i < ARRAY_SIZE(chip_id_list); i++) {
		if (chip_id == chip_id_list[i])
			return true;
	}

	return false;
}

static int probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct sb_mfc *mfc = NULL;
	int ret = 0, wpc_det = 0, chip_id = 0;

	/* check param */
	mfc_log("Loading...wireless_ic = 0x%x\n", wireless_ic);
	chip_id = WRL_PARAM_GET_CHIP_ID(wireless_ic);
	if (check_chip_id(chip_id)) {
		ret = -ENODEV;
		goto skip_probe;
	}

	ret = i2c_check_functionality(i2c->adapter, I2C_FUNC_SMBUS_BYTE_DATA |
		I2C_FUNC_SMBUS_WORD_DATA | I2C_FUNC_SMBUS_I2C_BLOCK);
	if (!ret) {
		ret = i2c_get_functionality(i2c->adapter);
		mfc_log("I2C functionality is not supported.\n");
		ret = -ENOSYS;
		goto skip_probe;
	}

	mfc = kzalloc(sizeof(struct sb_mfc), GFP_KERNEL);
	if (!mfc) {
		mfc_log("failed to alloc sb_mfc\n");
		ret = -ENOMEM;
		goto skip_probe;
	}

	ret = parse_dt(mfc, &i2c->dev);
	if (ret < 0) {
		mfc_log("failed to parse dt (ret = %d)\n", ret);
		goto failed_dt;
	}

	/* check chip id */
	wpc_det = gpio_get_value(mfc->wpc_det_gpio);
	if (!wpc_det) {
		sec_chg_check_dev_modprobe(SC_DEV_MAIN_CHG);
		set_uno(mfc->chg_name, true);
		msleep(200);
	}

	chip_id = get_chip_id(i2c);
	wireless_ic = wireless_ic & ~(WRL_PARAM_CHIP_ID_MASK);
	wireless_ic = wireless_ic | (chip_id << WRL_PARAM_CHIP_ID_SHIFT);

	if (!wpc_det)
		set_uno(mfc->chg_name, false);

	ret = -ENODEV;
	mfc_log("Loaded...wireless_ic = 0x%x\n", wireless_ic);

	sec_chg_set_dev_init(SC_DEV_SB_MFC);

	kfree(mfc);
	return ret;

failed_dt:
	kfree(mfc);
skip_probe:
	sec_chg_set_dev_init(SC_DEV_SB_MFC);
	return ret;
}

static int remove(struct i2c_client *client)
{
	mfc_log("\n");
	return 0;
}

#if defined(CONFIG_PM)
static int suspend(struct device *dev)
{
	return 0;
}

static int resume(struct device *dev)
{
	return 0;
}
#else
#define suspend NULL
#define resume NULL
#endif

static void shutdown(struct i2c_client *client)
{
}

static const struct i2c_device_id id_table[] = {
	{ MFC_MODULE_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, id_table);

#ifdef CONFIG_OF
static const struct of_device_id match_table[] = {
	{ .compatible = MFC_MODULE_NAME, },
	{},
};
#else
#define match_table NULL
#endif

static const struct dev_pm_ops pm = {
	SET_SYSTEM_SLEEP_PM_OPS(suspend, resume)
};

static struct i2c_driver i2c = {
	.driver = {
		.name	= MFC_MODULE_NAME,
		.owner	= THIS_MODULE,
#if defined(CONFIG_PM)
		.pm = &pm,
#endif /* CONFIG_PM */
		.of_match_table = match_table,
	},
	.shutdown	= shutdown,
	.probe	= probe,
	.remove	= remove,
	.id_table = id_table,
};

static int __init sb_mfc_init(void)
{
	mfc_log("\n");
	return i2c_add_driver(&i2c);
}
module_init(sb_mfc_init);

static void __exit sb_mfc_exit(void)
{
	i2c_del_driver(&i2c);
}
module_exit(sb_mfc_exit);

MODULE_SOFTDEP("pre: max77705_charger");
MODULE_DESCRIPTION("Samsung Battery MFC");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
