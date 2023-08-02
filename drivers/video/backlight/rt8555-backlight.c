/*
 * rt8555-backlight.c - Platform data for RT8555 backlight driver
 *
 * Copyright (C) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/kernel.h>
#include <asm/unaligned.h>
#include <mach/board.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/regulator/consumer.h>
#include <asm/mach-types.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/earlysuspend.h>

/*#define CONFIG_BLIC_TUNING 1*/

struct tuning_table {
	u8* table;
	u8 table_length;
};
struct backlight_platform_data {
	unsigned	 int gpio_backlight_en;
	unsigned	 int gpio_backlight_pwm;
	u32 en_gpio_flags;

	int gpio_sda;
	u32 sda_gpio_flags;

	int gpio_scl;
	u32 scl_gpio_flags;
	struct early_suspend early_suspend_desc;
};

struct backlight_info {
	struct i2c_client			*client;
	struct backlight_platform_data	*pdata;
	struct tuning_table bl_ic_settings;
	struct tuning_table bl_control;
};

struct backlight_info *bl_info;
static const char *bl_ic_name;

static int backlight_i2c_write(struct i2c_client *client,
		u8 reg,  u8 val, unsigned int len)
{
	int err = 0;
	int retry = 3;
	u8 temp_val = val;

	while (retry--) {
		err = i2c_smbus_write_i2c_block_data(client,
				reg, len, &temp_val);
		if (err >= 0)
			return err;

		dev_info(&client->dev, "%s: i2c transfer error. %d\n", __func__, err);
	}
	return err;
}

int backlight_i2c_read_sys(u8 reg, u8* val)
{
	int err = 0;
	int retry = 3;
	struct backlight_info *info = bl_info;
	struct i2c_client *client=info->client;
	while (retry--) {
		err = i2c_smbus_read_i2c_block_data(client,
				reg, 1, val);
		if (err >= 0)
			return err;
		printk("[BACKLIGHT] %s:i2c transfer error.\n", __func__);
	}
	return err;
}
EXPORT_SYMBOL(backlight_i2c_read_sys);

int backlight_i2c_write_sys(u8 reg,  u8 val)
{
	int err = 0;
	int retry = 3;
	u8 temp_val = val;
	struct backlight_info *info = bl_info;
	struct i2c_client *client=info->client;
	while (retry--) {
		err = i2c_smbus_write_i2c_block_data(client,
				reg, 1, &temp_val);
		if (err >= 0)
			return err;
		printk("[BACKLIGHT] %s: i2c transfer error. %d\n", __func__, err);
	}
	return err;
}
EXPORT_SYMBOL(backlight_i2c_write_sys);


#if defined(CONFIG_BLIC_TUNING)
static int atoi(const char *name, int *l)
{
	int val = 0;
	if(*name == '\0' || *name == '\n')
		return -1;
	for (;; name++) {
		(*l)++;
		switch (*name) {
			case '0' ... '9':
					val = 16*val+(*name-'0');
					break;
			case 'A' ... 'F':
				val = 16*val+(*name-'A') + 10;
				break;
			case 'a' ... 'f':
				val = 10*val+(*name-'a') + 10;
				break;
			case ' ':
				pr_debug("%s: val is: %d\n", __func__, val);
				return val;
				break;
			case '\n':
				pr_debug("%s: val is: %d\n", __func__, val);
				return val;
				break;
			default:
				return -1;
		}
	}
}

int backlight_i2c_read_sys(u8 reg, u8* val)
{
	int err = 0;
	int retry = 3;
	struct backlight_info *info = bl_info;
	struct i2c_client *client=info->client;
	while (retry--) {
		err = i2c_smbus_read_i2c_block_data(client,
				reg, 1, val);
		if (err >= 0)
			return err;
		printk("[BACKLIGHT] %s:i2c transfer error.\n", __func__);
	}
	return err;
}
EXPORT_SYMBOL(backlight_i2c_read_sys);

int backlight_i2c_write_sys(u8 reg,  u8 val)
{
	int err = 0;
	int retry = 3;
	u8 temp_val = val;
	struct backlight_info *info = bl_info;
	struct i2c_client *client=info->client;
	while (retry--) {
		err = i2c_smbus_write_i2c_block_data(client,
				reg, 1, &temp_val);
		if (err >= 0)
			return err;
		printk("[BACKLIGHT] %s: i2c transfer error. %d\n", __func__, err);
	}
	return err;
}
EXPORT_SYMBOL(backlight_i2c_write_sys);
static ssize_t backlight_i2c_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	u8 arr[6];
	u32 i, cnt=0;
	u8 val=0;
	for(i=0; i<3; i++)
		arr[i] = atoi(buf+cnt, &cnt);
	for(i=0; i<3; i++) {
		if(arr[i] == -1)
			break;
	}
	if(i<3) {
		pr_err("%s: Invalid value entered\n", __func__);
		return size;
	}
	backlight_i2c_read_sys(arr[0],&val);
	pr_info("%s: Before REG 0x%02X  = 0x%02X\n", __func__,arr[0],val);
	backlight_i2c_write_sys(arr[0],arr[1]);
	pr_info("%s: After REG 0x%02Xh =  0x%02X\n", __func__,arr[0],arr[1]);
	return size;
}
static ssize_t backlight_i2c_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	static int string_size = 400;
	char temp[string_size];
	char temp1[40];
	int i;
	u8 val=0;
	snprintf(temp, 34, "BL IC Current register settings\n");
	for(i=0; i<0x20; i++) {
		backlight_i2c_read_sys(i,&val);
		snprintf(temp1, 22, "REG 0x%02Xh = 0x%02X\n", i,val);
		strncat(temp, temp1, 22);
	}
	strlcat(buf, temp, string_size);
	return strnlen(buf, string_size);
}
static ssize_t backlight_ic_name_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	if(bl_ic_name)
		snprintf(buf, 50, "BACKLIGHT IC NAME : %s\n",bl_ic_name);

	return strnlen(buf, 50);
}
static DEVICE_ATTR(bl_ic_tune, 0666,backlight_i2c_show , backlight_i2c_store);
static DEVICE_ATTR(bl_ic_name, S_IRUGO,backlight_ic_name_show , NULL);
#endif

static int backlight_parse_dt(struct device *dev,
			struct backlight_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	pdata->gpio_scl = of_get_named_gpio_flags(np, "backlight,scl-gpio",
				0, &pdata->scl_gpio_flags);
	pdata->gpio_sda = of_get_named_gpio_flags(np, "backlight,sda-gpio",
				0, &pdata->sda_gpio_flags);
	pdata->gpio_backlight_en = of_get_named_gpio_flags(np, "backlight-en-gpio",
				0, &pdata->en_gpio_flags);

	bl_ic_name = of_get_property(np, "backlight-ic-name", NULL);
	if (!bl_ic_name) {
		printk("[BACKLIGHT] %s:%d, backlight IC name not specified\n",__func__, __LINE__);
	} else {
		printk("[BACKLIGHT] %s: Backlight IC Name = %s\n", __func__,bl_ic_name);
	}

	pr_err("[BACKLIGHT] %s gpio_scl : %d , gpio_sda : %d en : %d \n", __func__, pdata->gpio_scl, pdata->gpio_sda, pdata->gpio_backlight_en);
	return 0;
}

static void rt8555_register_init(void)
{
	u8 Current_Setting_Reg=0x02;
	u8 Current_Setting_Value=0xB6;
	u8 LED_Driver_Headroom_Reg=0x08;
	u8 LED_Driver_Headroom_Value=0x04;
	int err = 0;

	err = backlight_i2c_write_sys(Current_Setting_Reg,Current_Setting_Value);
	if(err<0)
		printk("[BACKLIGHT] rt8555 Current_Setting_Reg set error!!!\n");
	err = backlight_i2c_write_sys(LED_Driver_Headroom_Reg,LED_Driver_Headroom_Value);
	if(err<0)
		printk("[BACKLIGHT] rt8555 LED_Driver_Headroom_Reg set error!!!\n");
	err = backlight_i2c_write_sys(0x50,0x05);
	if(err<0)
		printk("[BACKLIGHT] rt8555 0x50 set error!!!\n");
	err = backlight_i2c_write_sys(0x51,0x08);
	if(err<0)
		printk("[BACKLIGHT] rt8555 0x51 set error!!!\n");
}

static void rt8555_backlight_earlysuspend(struct early_suspend *desc)
{
	struct backlight_platform_data *pdata = container_of(desc, struct backlight_platform_data,
				early_suspend_desc);

	gpio_direction_output(pdata->gpio_backlight_en, 0);

	printk("[BACKLIGHT] %s, pdata->gpio_backlight_en : %d\n", __func__, pdata->gpio_backlight_en);
}
static void rt8555_backlight_earlyresume(struct early_suspend *desc)
{
	struct backlight_platform_data *pdata = container_of(desc, struct backlight_platform_data,
				early_suspend_desc);

	//msleep(500);
	gpio_direction_output(pdata->gpio_backlight_en,1);
	msleep(10);
	rt8555_register_init();
}


static int rt8555_backlight_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct backlight_platform_data *pdata;
	struct backlight_info *info;
	int error = 0;

#if defined(CONFIG_BLIC_TUNING)
	struct class *backlight_class;
	struct device *backlight_dev;
#endif
	pr_info("%s", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)){
		pr_err("[BACKLIGHT] failed to check i2c functionality!\n");
		return -EIO;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct backlight_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_info(&client->dev, "[BACKLIGHT] Failed to allocate memory\n");
			return -ENOMEM;
		}

		error = backlight_parse_dt(&client->dev, pdata);
		if (error)
			return error;
	} else
		pdata = client->dev.platform_data;

	gpio_request(pdata->gpio_backlight_en,"BACKLIGHT_EN_GPIO");

	bl_info = info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_info(&client->dev, "[BACKLIGHT] %s: fail to memory allocation.\n", __func__);
		return -ENOMEM;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	pdata->early_suspend_desc.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING;
	pdata->early_suspend_desc.suspend = rt8555_backlight_earlysuspend;
	pdata->early_suspend_desc.resume = rt8555_backlight_earlyresume;
	register_early_suspend(&pdata->early_suspend_desc);
#endif

	info->client = client;
	info->pdata = pdata;

#if defined(CONFIG_BLIC_TUNING)
	backlight_class = class_create(THIS_MODULE, "bl-dbg");
	backlight_dev = device_create(backlight_class, NULL, 0, NULL,  "ic-tuning");
	if (IS_ERR(backlight_dev))
		pr_err("[BACKLIGHT] Failed to create device(backlight_dev)!\n");
	else {
		if (device_create_file(backlight_dev, &dev_attr_bl_ic_tune) < 0)
			pr_err("[BACKLIGHT] Failed to create device file(%s)!\n", dev_attr_bl_ic_tune.attr.name);
		if (device_create_file(backlight_dev, &dev_attr_bl_ic_name) < 0)
			pr_err("[BACKLIGHT] Failed to create device file(%s)!\n", dev_attr_bl_ic_name.attr.name);
	}
#endif
	i2c_set_clientdata(client, info);

	//rt8555_register_init();

	return error;
}

static int rt8555_backlight_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id rt8555_backlight_id[] = {
	{"RICHTEK-RT8555", 0},
	{}
};


static struct of_device_id rt8555_backlight_match_table[] = {
	{ .compatible = "Richtek, RT8555",},
	{ },
};



static struct i2c_driver rt8555_backlight_driver = {
	.driver = {
		.name = "RICHTEK-RT8555",
		.of_match_table = rt8555_backlight_match_table,
		   },
	.id_table = rt8555_backlight_id,
	.probe = rt8555_backlight_probe,
	.remove = rt8555_backlight_remove,
};

static int __init rt8555_backlight_init(void)
{

	int ret = 0;

	pr_info("%s", __func__);

	ret = i2c_add_driver(&rt8555_backlight_driver);
	if (ret) {
		printk(KERN_ERR "backlight_init registration failed. ret= %d\n",
			ret);
	}

	return ret;
}

static void __exit rt8555_backlight_exit(void)
{

	i2c_del_driver(&rt8555_backlight_driver);
}

module_init(rt8555_backlight_init);
module_exit(rt8555_backlight_exit);

MODULE_AUTHOR("rb.bankapur@samsung.com");
MODULE_DESCRIPTION("RT8555 backlight driver");
MODULE_LICENSE("GPL");
