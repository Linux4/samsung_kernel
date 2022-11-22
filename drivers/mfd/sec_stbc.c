/*
 *  sec_stbc.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG

#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/mfd/stbcfg01.h>
#include <linux/mfd/stbcfg01-private.h>

static struct mfd_cell stbc_devs[] = {
	{ .name = "sec-charger",},
	{ .name = "sec-fuelgauge",},
};

static int __devinit sec_stbc_probe(struct i2c_client *client,
				       const struct i2c_device_id *id)
{
	struct stbcfg01_dev *stbc;
	int ret;
	unsigned char values, values2;

	pr_info("[%s] START\n", __func__);

	stbc = kzalloc(sizeof(struct stbcfg01_dev), GFP_KERNEL);
	if (stbc == NULL)
		return -ENOMEM;

	i2c_set_clientdata(client, stbc);

	stbc->i2c = client;
	stbc->dev = &client->dev;
	stbc->pdata = client->dev.platform_data;

	ret = mfd_add_devices(stbc->dev, -1,
			      stbc_devs,
			      ARRAY_SIZE(stbc_devs), NULL,
			      0);

	if (ret) {
		dev_err(&client->dev, "%s : failed to add devices\n", __func__);
		goto err_mfd;
	}

	pr_info("[%s] END\n", __func__);

	return ret;

err_mfd:
	mfd_remove_devices(stbc->dev);
	return ret;
}

static int __devexit sec_stbc_remove(struct i2c_client *client)
{
	struct stbcfg01_dev *stbc = i2c_get_clientdata(client);

	mfd_remove_devices(stbc->dev);
	return 0;
}

static const struct i2c_device_id sec_stbc_id[] = {
	{"sec-stbc", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, sec_stbc_id);

static struct i2c_driver sec_stbc_driver = {
	.driver = {
		.name	= "sec-stbc",
		.owner = THIS_MODULE,
	},
	.probe	= sec_stbc_probe,
	.remove	= __devexit_p(sec_stbc_remove),
	.id_table	= sec_stbc_id,
};

static int __init sec_stbc_init(void)
{
	pr_info("[%s] start \n", __func__);
	return i2c_add_driver(&sec_stbc_driver);
}

static void __exit sec_stbc_exit(void)
{
	i2c_del_driver(&sec_stbc_driver);
}

module_init(sec_stbc_init);
module_exit(sec_stbc_exit);

MODULE_DESCRIPTION("Samsung STBC Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
