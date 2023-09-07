/*
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2017 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include "../../../misc/mediatek/lcm/mtk_gen_panel/mtk_gen_common.h"
#define TPS_I2C_BUSNUM  1    /* for I2C channel 1 */
#define I2C_ID_NAME "isl98611"
#define ISL98611_SLAVE_ADDR_WRITE  0x29


/******************************************************************
 * GLobal Variable
 ******************************************************************/
static struct i2c_board_info isl98611_board_info __initdata = \
		{ I2C_BOARD_INFO(I2C_ID_NAME, ISL98611_SLAVE_ADDR_WRITE) };

static struct i2c_client *isl98611_i2c_client;

/******************************************************************
 * Data Structure
 ******************************************************************/
struct isl98611_dev {
	struct i2c_client *client;

};

static const struct i2c_device_id isl98611_id[] = {
	{I2C_ID_NAME, 0},
	{}
};

/******************************************************************
 * Function
 ******************************************************************/
static int isl98611_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0, cnt = 2;

	struct i2c_client *client = isl98611_i2c_client;
	char write_data[2] = { 0 };
	write_data[0] = addr;
	write_data[1] = value;

	do {
		ret = i2c_master_send(client, write_data, 2);
		if (ret < 0)
			pr_info("isl98611 write addr[0x%x] fail!!\n", addr);
		else 
			goto pass;
	} while (cnt--);

pass:
	return ret;
}

void ISL98611_reg_init(void)
{
	int ret = 0;
	pr_info("ISL98611_reg_init.\n");

	ret |= isl98611_write_bytes(0x04, 0x14);/*VSP 5P5*/
	ret |= isl98611_write_bytes(0x05, 0x14);/*VSN 5P5*/
	ret |= isl98611_write_bytes(0x13, 0xC7);/*PWMI * SWIRE(GND) */
	ret |= isl98611_write_bytes(0x14, 0x7D);/*8bit, 20915(20.915Khz)*/
	ret |= isl98611_write_bytes(0x12, 0xBF);/*0xBF-->25mA*/
	mdelay(3);
	if (ret < 0)
		pr_info("td4303-isl98611-cmd--i2c write error\n");
	else
		pr_info("td4303-isl98611-cmd--i2c write success\n");
}

static int isl98611_lcm_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{

	if (event != FB_LCM_EVENT_RESUME && event != FB_LCM_EVENT_SUSPEND)
		return 0;

	pr_info("mtk_lcm_notifier event=[%s]\n",\
		(event == FB_LCM_EVENT_RESUME) ? "resume" : "suspend");

	switch (event) {
	case FB_LCM_EVENT_RESUME:
		ISL98611_reg_init();
		break;
	case FB_LCM_EVENT_SUSPEND:
		break;
	}
	return 0;
}

static struct notifier_block blic_notify;

static int isl98611_bl_register_cb (void)
{
	memset(&blic_notify, 0, sizeof(blic_notify));
	blic_notify.notifier_call = isl98611_lcm_notifier_callback;

	return mtk_lcm_register_client(&blic_notify);
}

static void isl98611_bl_unregister_cb (void)
{
	mtk_lcm_unregister_client(&blic_notify);
}

static int isl98611_probe(struct i2c_client *client,\
					const struct i2c_device_id *id)
{
	int ret;

	pr_info("isl98611_iic_probe\n");
	pr_info("info==>name=%s addr=0x%x\n", client->name, client->addr);
	isl98611_i2c_client = client;

	ret = isl98611_bl_register_cb();

	if (ret)
		i2c_unregister_device(client);

	return 0;
}

static int isl98611_remove(struct i2c_client *client)
{
	pr_info("isl98611_remove\n");
	isl98611_i2c_client = NULL;
	i2c_unregister_device(client);

	return 0;
}

static struct i2c_driver isl98611_iic_driver = {
	.id_table = isl98611_id,
	.probe = isl98611_probe,
	.remove = isl98611_remove,
	/* .detect  = mt6605_detect, */
	.driver = {
		.owner = THIS_MODULE,
		.name = "isl98611",
	},
};

static int __init isl98611_iic_init(void)
{
	i2c_register_board_info(TPS_I2C_BUSNUM, &isl98611_board_info, 1);
	i2c_add_driver(&isl98611_iic_driver);
	pr_info("isl98611_iic_init success\n");

	return 0;
}

static void __exit isl98611_iic_exit(void)
{
	pr_info("isl98611_iic_exit\n");
	isl98611_bl_unregister_cb();
	i2c_del_driver(&isl98611_iic_driver);
}

module_init(isl98611_iic_init);
module_exit(isl98611_iic_exit);

MODULE_AUTHOR("jone.jung");
MODULE_DESCRIPTION("samsung isl98611 I2C Driver");
