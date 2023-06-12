/*
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2011 Bosch Sensortec GmbH
 * All Rights Reserved
 */


/* file sm5107.c
   brief This file contains all function implementations for the sm5107 in linux
   this source file refer to MT6572 platform
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <mach/gpio_const.h>
#include <mt_gpio.h>

#include <linux/platform_device.h>

#include <linux/leds.h>

#define SM5107_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)

#define SM5107_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))

#define DSV_NAME "dsv"

#define SM5107_DEV_NAME "sm5107"

#define CPD_TAG                  "[ChargePump] "
#define CPD_FUN(f)               pr_debug(CPD_TAG"%s\n", __func__)
#define CPD_ERR(fmt, args...)    pr_err(CPD_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define CPD_LOG(fmt, args...)    pr_debug(CPD_TAG fmt, ##args)

/* I2C variable */
static struct i2c_client *new_client;
static const struct i2c_device_id sm5107_i2c_id[] = { {SM5107_DEV_NAME, 0}, {} };
static struct i2c_board_info i2c_sm5107 __initdata = { I2C_BOARD_INFO(SM5107_DEV_NAME, 0x3E) };

static int sm5107_driver_probe(struct i2c_client *client, const struct i2c_device_id *id);

#ifdef CONFIG_OF
static const struct of_device_id sm5107_of_match[] = {
	{.compatible = "mediatek,DSV",},
	{},
};

MODULE_DEVICE_TABLE(of, sm5107_of_match);
#endif

static struct i2c_driver sm5107_driver = {
	.driver = {
		   .name = "sm5107",
#ifdef CONFIG_OF
		   .of_match_table = sm5107_of_match,
#endif
		   },
	.probe = sm5107_driver_probe,
	.id_table = sm5107_i2c_id,
};

#ifndef GPIO_DSV_ENN_EN
#define GPIO_DSV_ENN_EN (GPIO60 | 0x80000000)
#define GPIO_DSV_ENN_EN_M_GPIO GPIO_MODE_00
#endif

struct semaphore sm5107_lock;

/* generic */
#define SM5107_MAX_RETRY_I2C_XFER (100)
#define SM5107_I2C_WRITE_DELAY_TIME 1

/* i2c write routine for */
static char sm5107_i2c_write(struct i2c_client *client, u8 reg_addr, u8 *data, u8 len)
{
#if !defined BMA_USE_BASIC_I2C_FUNC
	s32 dummy;
#ifndef BMA_SMBUS
	/* u8 buffer[2]; */
#endif

	if (NULL == client)
		return -1;

	while (0 != len--) {
#if 1
		dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);
#else
		buffer[0] = reg_addr;
		buffer[1] = *data;
		dummy = i2c_master_send(client, (char *)buffer, 2);
#endif

		reg_addr++;
		data++;
		if (dummy < 0)
			return -1;
	}

#else
	u8 buffer[2];
	int retry;
	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 2,
		 .buf = buffer,
		 },
	};

	while (0 != len--) {
		buffer[0] = reg_addr;
		buffer[1] = *data;
		for (retry = 0; retry < SM5107_MAX_RETRY_I2C_XFER; retry++) {
			if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
				break;
			mdelay(SM5107_I2C_WRITE_DELAY_TIME);
		}
		if (SM5107_MAX_RETRY_I2C_XFER <= retry)
			return -EIO;
		reg_addr++;
		data++;
	}
#endif
	return 0;
}

#if 0
static int sm5107_smbus_read_byte(struct i2c_client *client,
				  unsigned char reg_addr, unsigned char *data)
{
	return sm5107_i2c_read(client, reg_addr, data, 1);
}
#endif

static int sm5107_smbus_write_byte(struct i2c_client *client,
				   unsigned char reg_addr, unsigned char *data)
{
	int ret_val = 0;
	int i = 0;

	ret_val = sm5107_i2c_write(client, reg_addr, data, 1);

	for (i = 0; i < 5; i++) {
		if (ret_val != 0)
			sm5107_i2c_write(client, reg_addr, data, 1);
		else
			return ret_val;
	}
	return ret_val;
}

void sm5107_dsv_ctrl(int enable)
{
	unsigned char data = 0;

	mt_set_gpio_mode(GPIO_DSV_ENN_EN, GPIO_DSV_ENN_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_DSV_ENN_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_DSV_ENN_EN, GPIO_DIR_OUT);

	if (enable == 1) {
		mt_set_gpio_out(GPIO_DSV_ENN_EN, GPIO_OUT_ONE);
		mdelay(5);
		/* Set to Smart-phone mode, Active-Discharge Enable */
		data = 0x03;
		sm5107_smbus_write_byte(new_client, 0x03, &data);
		/* DSV output Enable, Set Normal mode */
		data = 0x40;
		sm5107_smbus_write_byte(new_client, 0xFF, &data);
	} else {
		/* Set to Smart-phone mode, Active-Discharge Enable */
		data = 0x03;
		sm5107_smbus_write_byte(new_client, 0x03, &data);
		/* DSV output Disable, Set Normal mode */
		data = 0x00;
		sm5107_smbus_write_byte(new_client, 0xFF, &data);

		mt_set_gpio_out(GPIO_DSV_ENN_EN, GPIO_OUT_ZERO);
		mdelay(5);
	}
}

void sm5107_dsv_toggle_ctrl(void)
{
	unsigned char data = 0;
	/* Set to Smart-phone mode, Active-Discharge Disable */
	data = 0x00;
	sm5107_smbus_write_byte(new_client, 0x03, &data);
	/* DSV output Disable, Set Knock-on 4 mode */
	data = 0x28;
	sm5107_smbus_write_byte(new_client, 0xFF, &data);
}

static int sm5107_driver_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;

	CPD_FUN();
	new_client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL);
	if (!new_client) {
		err = -ENOMEM;
		goto exit;
	}
	memset(new_client, 0, sizeof(struct i2c_client));

	new_client = client;

	return 0;

exit:
	return err;
}

static int sm5107_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	new_client = client;

	CPD_FUN();
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CPD_LOG("i2c_check_functionality error\n");
		return -1;
	}
/* sema_init(&sm5107_lock, 1); */

	if (client == NULL)
		CPD_ERR("client is NULL\n");
	else
		CPD_LOG("%p %x %x\n", client->adapter, client->addr, client->flags);
	return 0;
}


static int sm5107_i2c_remove(struct i2c_client *client)
{
	CPD_FUN();
	new_client = NULL;
	return 0;
}


static int
__attribute__ ((unused)) sm5107_detect(struct i2c_client *client, int kind,
					   struct i2c_board_info *info)
{
	CPD_FUN();
	return 0;
}

static struct i2c_driver sm5107_i2c_driver = {
	.driver.name = SM5107_DEV_NAME,
	.probe = sm5107_i2c_probe,
	.remove = sm5107_i2c_remove,
	.id_table = sm5107_i2c_id,
};

static int sm5107_pd_probe(struct platform_device *pdev)
{
	CPD_FUN();

	/* i2c number 1(0~2) control */
	i2c_register_board_info(2, &i2c_sm5107, 1);

/* i2c_add_driver(&sm5107_i2c_driver); */
	if (i2c_add_driver(&sm5107_driver) != 0)
		CPD_ERR("Failed to register sm5107 driver");
	return 0;
}

static int __attribute__ ((unused)) sm5107_pd_remove(struct platform_device *pdev)
{
	CPD_FUN();
	i2c_del_driver(&sm5107_i2c_driver);
	return 0;
}

static struct platform_driver sm5107_dsv_driver = {
	.remove = sm5107_pd_remove,
	.probe = sm5107_pd_probe,
	.driver = {
		   .name = DSV_NAME,
		   .owner = THIS_MODULE,
		   },
};

#if 0
#ifdef CONFIG_OF
static struct platform_device mtk_dsv_dev = {
	.name = DSV_NAME,
	.id = -1,
};
#endif
#endif
static int __init sm5107_init(void)
{
	CPD_FUN();
	sema_init(&sm5107_lock, 1);

#if 0
#ifdef CONFIG_OF
	if (platform_device_register(&mtk_dsv_dev)) {
		CPD_ERR("failed to register device");
		return -1;
	}
#endif

	if (platform_driver_register(&sm5107_dsv_driver)) {
		CPD_ERR("failed to register driver");
		return -1;
	}
#else

	i2c_register_board_info(2, &i2c_sm5107, 1);

/* i2c_add_driver(&sm5107_i2c_driver); */
	if (i2c_add_driver(&sm5107_driver) != 0)
		CPD_ERR("Failed to register sm5107 driver");
#endif

	return 0;
}

static void __exit sm5107_exit(void)
{
	platform_driver_unregister(&sm5107_dsv_driver);
}

MODULE_DESCRIPTION("sm5107 driver");
MODULE_LICENSE("GPL");

late_initcall(sm5107_init);
module_exit(sm5107_exit);
