/*
 * drivers/leds/leds-lm3648.c
 * General device driver for TI LM3648, FLASH LED Driver
 *
 * Copyright (C) 2014 Texas Instruments
 *
 * Contact: Daniel Jeong <daniel.jeong@ti.com>
 *			Ldd-Mlp <ldd-mlp@list.ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
 
 /*
 cd /sys/class/leds/torch/
 echo enable > enable
 echo on > onoff
 echo off > onoff
 echo 120 > brightness(0~127, using Decimal,not Hexadecimal)
 echo disable > enable
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/regmap.h>
#include <linux/workqueue.h>
#include <linux/platform_data/leds-lm3648.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>

/* registers */
#define REG_ENABLE		0x01
#define REG_FLASH_BR	0x03
#define REG_TORCH_BR	0x05
#define REG_FLASH_TOUT	0x08
#define REG_FLAG0		0x0a
#define REG_FLAG1		0x0b

enum lm3648_devid {
	ID_FLASH = 0x0,
	ID_TORCH,
	ID_MAX
};

enum lm3648_mode {
	MODE_STDBY = 0x0,
	MODE_IR,
	MODE_TORCH,
	MODE_FLASH,
	MODE_MAX
};

enum lm3648_devfile {
	DFILE_FLASH_ENABLE = 0,
	DFILE_FLASH_ONOFF,
	DFILE_FLASH_SOURCE,
	DFILE_FLASH_TIMEOUT,
	DFILE_TORCH_ENABLE,
	DFILE_TORCH_ONOFF,
	DFILE_TORCH_SOURCE,
	DFILE_MAX
};

#define to_lm3648(_ctrl, _no) container_of(_ctrl, struct lm3648, cdev[_no])

struct lm3648 {
	struct device *dev;

	u8 brightness[ID_MAX];
	struct work_struct work[ID_MAX];
	struct led_classdev cdev[ID_MAX];

	struct lm3648_platform_data *pdata;
	struct regmap *regmap;
	struct mutex lock;
};


struct lm3648_devices {
	struct led_classdev cdev;
	work_func_t func;
};

/* flag read */
static void lm3648_read_flag(struct lm3648 *pchip){

	int rval;
	unsigned int flag0, flag1;

	rval = regmap_read(pchip->regmap,REG_FLAG0, &flag0);
	rval |= regmap_read(pchip->regmap,REG_FLAG1, &flag1);

	if(rval < 0){
		dev_err(pchip->dev, "i2c access fail.\n");
		return;
	}

	dev_info(pchip->dev, "[flag1] 0x%x, [flag0] 0x%x\n", flag1 & 0x1f, flag0);
}

/* torch brightness control */
static void lm3648_deferred_torch_brightness_set(struct work_struct *work)
{
	struct lm3648 *pchip = container_of(work,
							struct lm3648, work[ID_TORCH]);

	if(regmap_update_bits(pchip->regmap, 
								REG_TORCH_BR, 0xff,
								pchip->brightness[ID_TORCH] | 0x80))
		dev_err(pchip->dev, "i2c access fail.\n");
	lm3648_read_flag(pchip);
}

static void lm3648_torch_brightness_set(struct led_classdev *cdev,
					enum led_brightness brightness)
{
	struct lm3648 *pchip = container_of(cdev, struct lm3648, cdev[ID_TORCH]);

	pchip->brightness[ID_TORCH] = brightness;
	schedule_work(&pchip->work[ID_TORCH]);
}

/* flash brightness control */
static void lm3648_deferred_flash_brightness_set(struct work_struct *work)
{
	struct lm3648 *pchip = container_of(work,
							struct lm3648, work[ID_FLASH]);

	if(regmap_update_bits(pchip->regmap, 
								REG_FLASH_BR, 0xff,
								pchip->brightness[ID_FLASH] | 0x80))
		dev_err(pchip->dev, "i2c access fail.\n");
	lm3648_read_flag(pchip);
}

static void lm3648_flash_brightness_set(struct led_classdev *cdev,
					enum led_brightness brightness)
{
	struct lm3648 *pchip = container_of(cdev, struct lm3648, cdev[ID_FLASH]);

	pchip->brightness[ID_FLASH] = brightness;
	schedule_work(&pchip->work[ID_FLASH]);
}

/* device info for led class register */
static struct lm3648_devices lm3648_leds[ID_MAX] = {
	[ID_FLASH] = {
		.cdev.name = "flash",
		.cdev.brightness = 0x1f,
		.cdev.max_brightness = 0x3f,
		.cdev.brightness_set = lm3648_flash_brightness_set,
		.cdev.default_trigger = "flash",
		.func = lm3648_deferred_flash_brightness_set
		},
	[ID_TORCH] = {
		.cdev.name = "torch",
		.cdev.brightness = 0x70,
		.cdev.max_brightness = 0x7f,
		.cdev.brightness_set = lm3648_torch_brightness_set,
		.cdev.default_trigger = "torch",
		.func = lm3648_deferred_torch_brightness_set
		},
};


/* led class register and unregister */
static void lm3648_led_unregister(struct lm3648 *pchip, enum lm3648_devid id)
{
	int icnt;

	for(icnt = id; icnt > 0 ; icnt--)
		led_classdev_unregister(&pchip->cdev[icnt-1]);
}

static int lm3648_led_register(struct lm3648 *pchip)
{
	int icnt, rval; 

	for(icnt = 0 ; icnt < ID_MAX; icnt++){
		INIT_WORK(&pchip->work[icnt], lm3648_leds[icnt].func);
		pchip->cdev[icnt].name = lm3648_leds[icnt].cdev.name;
		pchip->cdev[icnt].max_brightness = lm3648_leds[icnt].cdev.max_brightness;
		pchip->cdev[icnt].brightness = lm3648_leds[icnt].cdev.brightness;
		pchip->cdev[icnt].brightness_set = lm3648_leds[icnt].cdev.brightness_set;
		pchip->cdev[icnt].default_trigger = lm3648_leds[icnt].cdev.default_trigger;
		rval = led_classdev_register((struct device *)
				    pchip->dev, &pchip->cdev[icnt]);
		if(rval < 0){
			lm3648_led_unregister(pchip, icnt);
			return rval;
		}	
	}
	return 0;
}

/* control commands */
struct lm3648_commands {
	char *str;
	int size;
};

enum lm3648_cmd_id {
	CMD_ENABLE = 0,
	CMD_DISABLE,
	CMD_ON,
	CMD_OFF,
	CMD_IRMODE,
	CMD_MAX
};

static struct lm3648_commands lm3648_cmds[CMD_MAX] = {
	[CMD_ENABLE] = {"enable", 6},
	[CMD_DISABLE] = {"disable", 7},
	[CMD_ON] = {"on", 2},
	[CMD_OFF] = {"off", 3},
	[CMD_IRMODE] = {"irmode", 6},
};

struct lm3648_files {
	enum lm3648_devid id;
	struct device_attribute attr;
};
int en_gpio_number;
static size_t lm3648_ctrl(struct device *dev,
					   const char *buf, enum lm3648_devid id,
					   enum lm3648_devfile dfid, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3648 *pchip = to_lm3648(led_cdev, id);
	enum lm3648_cmd_id icnt;
	int tout, rval;

	mutex_lock(&pchip->lock);
	for(icnt=0; icnt < CMD_MAX ; icnt++){
		if (strncmp(buf, lm3648_cmds[icnt].str, lm3648_cmds[icnt].size) == 0)
			break;
	}

	switch(dfid){
	/* led enable */
	case DFILE_FLASH_ENABLE:
	case DFILE_TORCH_ENABLE:
		if(icnt == CMD_ENABLE){
			gpio_set_value(en_gpio_number, 1);
			udelay(100);
			rval = regmap_update_bits(pchip->regmap, REG_ENABLE, 0x3, 0x3);
		}
		else if(icnt == CMD_DISABLE){
				rval = regmap_update_bits(pchip->regmap, REG_ENABLE, 0x3, 0x0);
				gpio_set_value(en_gpio_number, 0);
			}
	break;

	/* mode control flash/ir */
	case DFILE_FLASH_ONOFF:
		if(icnt == CMD_ON)
			rval = regmap_update_bits(pchip->regmap, REG_ENABLE, 0xc, 0xc);
		else if(icnt == CMD_OFF)
			rval = regmap_update_bits(pchip->regmap, REG_ENABLE, 0xc, 0x0);
		else if(icnt == CMD_IRMODE)
			rval = regmap_update_bits(pchip->regmap, REG_ENABLE, 0xc, 0x4);
	break;

	/* mode control torch */	
	case DFILE_TORCH_ONOFF:
		if(icnt == CMD_ON)
			rval = regmap_update_bits(pchip->regmap, REG_ENABLE, 0xc, 0x8);
		else if(icnt == CMD_OFF)
			rval = regmap_update_bits(pchip->regmap, REG_ENABLE, 0xc, 0x0);
	break;

	/* strobe pin control */
	case DFILE_FLASH_SOURCE:
		if(icnt == CMD_ON)
			rval = regmap_update_bits(pchip->regmap, REG_ENABLE, 0x20, 0x20);
		else if(icnt == CMD_OFF)
			rval = regmap_update_bits(pchip->regmap, REG_ENABLE, 0x20, 0x0);
	break;

	case DFILE_TORCH_SOURCE:
		if(icnt == CMD_ON)
			rval = regmap_update_bits(pchip->regmap, REG_ENABLE, 0x10, 0x10);
		else if(icnt == CMD_OFF)
			rval = regmap_update_bits(pchip->regmap, REG_ENABLE, 0x10, 0x0);
	break;

	/* flash time out */
	case DFILE_FLASH_TIMEOUT:
		rval = kstrtouint((const char *)buf, 10, &tout);
		if(rval < 0)
			break;
		rval = regmap_update_bits(pchip->regmap,
									REG_FLASH_TOUT, 0x0f, tout);
	break;

	default:
		dev_err(pchip->dev, "error : undefined dev file\n");
	break;
	}
	lm3648_read_flag(pchip);
	mutex_unlock(&pchip->lock);
	return size;
}

/* flash enable control */
static ssize_t lm3648_flash_enable_store(struct device *dev,
					   struct device_attribute *devAttr,
					   const char *buf, size_t size)
{
	return lm3648_ctrl(dev, buf, ID_FLASH, DFILE_FLASH_ENABLE, size);
}

/* flash onoff control */
static ssize_t lm3648_flash_onoff_store(struct device *dev,
					   struct device_attribute *devAttr,
					   const char *buf, size_t size)
{
	return lm3648_ctrl(dev, buf, ID_FLASH, DFILE_FLASH_ONOFF, size);
}

/* flash timeout control */
static ssize_t lm3648_flash_timeout_store(struct device *dev,
					   struct device_attribute *devAttr,
					   const char *buf, size_t size)
{
	return lm3648_ctrl(dev, buf, ID_FLASH, DFILE_FLASH_TIMEOUT, size);
}

/* flash source control */
static ssize_t lm3648_flash_source_store(struct device *dev,
					   struct device_attribute *devAttr,
					   const char *buf, size_t size)
{
	return lm3648_ctrl(dev, buf, ID_FLASH, DFILE_FLASH_SOURCE, size);
}

/* torch enable control */
static ssize_t lm3648_torch_enable_store(struct device *dev,
					   struct device_attribute *devAttr,
					   const char *buf, size_t size)
{
	return lm3648_ctrl(dev, buf, ID_TORCH, DFILE_TORCH_ENABLE, size);
}

/* torch onoff control */
static ssize_t lm3648_torch_onoff_store(struct device *dev,
					   struct device_attribute *devAttr,
					   const char *buf, size_t size)
{
	return lm3648_ctrl(dev, buf, ID_TORCH, DFILE_TORCH_ONOFF, size);
}

/* torch source control */
static ssize_t lm3648_torch_source_store(struct device *dev,
					   struct device_attribute *devAttr,
					   const char *buf, size_t size)
{
	return lm3648_ctrl(dev, buf, ID_TORCH, DFILE_TORCH_SOURCE, size);
}

/* device files */
#define lm3648_attr(_name, _show, _store)\
{\
	.attr = {\
		.name = _name,\
		.mode = 0644,\
	},\
	.show = _show,\
	.store = _store,\
}

static struct lm3648_files lm3648_devfiles[DFILE_MAX] = {
	[DFILE_FLASH_ENABLE] = {
		.id = ID_FLASH,
		.attr = lm3648_attr("enable", NULL, lm3648_flash_enable_store),
	},
	[DFILE_FLASH_ONOFF] = {
		.id = ID_FLASH,
		.attr = lm3648_attr("onoff", NULL, lm3648_flash_onoff_store),
	},
	[DFILE_FLASH_SOURCE] = {
		.id = ID_FLASH,
		.attr = lm3648_attr("source", NULL, lm3648_flash_source_store),
	},
	[DFILE_FLASH_TIMEOUT] = {
		.id = ID_FLASH,
		.attr = lm3648_attr("timeout", NULL, lm3648_flash_timeout_store),
	},
	[DFILE_TORCH_ENABLE] = {
		.id = ID_TORCH,
		.attr = lm3648_attr("enable", NULL, lm3648_torch_enable_store),
	},
	[DFILE_TORCH_ONOFF] = {
		.id = ID_TORCH,
		.attr = lm3648_attr("onoff", NULL, lm3648_torch_onoff_store),
	},
	[DFILE_TORCH_SOURCE] = {
		.id = ID_TORCH,
		.attr = lm3648_attr("source", NULL, lm3648_torch_source_store),
	},
};

static void lm3648_df_remove(struct lm3648 *pchip, enum lm3648_devfile dfid)
{
	enum lm3648_devfile icnt;
	
	for( icnt = dfid; icnt > 0; icnt--)
		device_remove_file(pchip->cdev[lm3648_devfiles[icnt-1].id].dev,
									&lm3648_devfiles[icnt-1].attr);
}

static int lm3648_df_create(struct lm3648 *pchip)
{	
	enum lm3648_devfile icnt;
	int rval; 

	for(icnt = 0; icnt < DFILE_MAX; icnt++){
		rval = device_create_file(pchip->cdev[lm3648_devfiles[icnt].id].dev,
									&lm3648_devfiles[icnt].attr);
		if (rval < 0){
			lm3648_df_remove(pchip, icnt);
			return rval;
		}
	}
	return 0;
}

static const struct regmap_config lm3648_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
};

static int lm3648_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct lm3648 *pchip;
	int rval;
	struct device_node *np = client->dev.of_node;
#ifdef CONFIG_OF
        if (np){
		en_gpio_number = of_get_gpio(np, 0);
		if(en_gpio_number < 0){
			dev_err(&client->dev, "fail to get irq_gpio_number\n");
			return -1;
		}
		printk("sensor: gpio  GPIO_IR_EN %d.\n", en_gpio_number);
	
	}
#endif
       rval = gpio_request(en_gpio_number, NULL);
	if (rval) {
	//	tmp = 1;
		printk("sensor: gpio already request GPIO_IR_EN %d.\n", en_gpio_number);
	}
	gpio_direction_output(en_gpio_number, 0);
	gpio_set_value(en_gpio_number, 0);
	/* i2c check */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c functionality check fail.\n");
		return -EOPNOTSUPP;
	}

	pchip = devm_kzalloc(&client->dev, sizeof(struct lm3648), GFP_KERNEL);
	if (!pchip)
		return -ENOMEM;

	pchip->dev = &client->dev;
	pchip->regmap = devm_regmap_init_i2c(client, &lm3648_regmap);
	if (IS_ERR(pchip->regmap)) {
		rval = PTR_ERR(pchip->regmap);
		dev_err(&client->dev, "Failed to allocate register map: %d\n",
			rval);
		return rval;
	}
	mutex_init(&pchip->lock);
	i2c_set_clientdata(client, pchip);

	/* led class register */
	rval = lm3648_led_register(pchip);
	if (rval < 0)
		return rval;

	/* create dev files*/
	rval =  lm3648_df_create(pchip);
	if (rval < 0){
		lm3648_led_unregister(pchip, ID_MAX);
		return rval;
	}

	dev_info(pchip->dev, "lm3648 leds initialized\n");
	return 0;
}

static int lm3648_remove(struct i2c_client *client)
{
	struct lm3648 *pchip = i2c_get_clientdata(client);

	lm3648_df_remove(pchip, DFILE_MAX);
	lm3648_led_unregister(pchip, ID_MAX);

	return 0;
}

static struct of_device_id lm3648_match_table[] = {
	{ .compatible = "TI,lm3648", },
	{ },
};

static const struct i2c_device_id lm3648_id[] = {
	{LM3648_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, lm3648_id);

static struct i2c_driver lm3648_i2c_driver = {
	.driver = {
		   .name = LM3648_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF		
		   .of_match_table = lm3648_match_table,		
#endif
		   .pm = NULL,
		   },
	.probe = lm3648_probe,
	.remove = lm3648_remove,
	.id_table = lm3648_id,
};
static int __init lm3648_leds_init(void)
{
	return i2c_add_driver(&lm3648_i2c_driver);
}

static void __exit llm3648_leds_exit(void)
{
	i2c_del_driver(&lm3648_i2c_driver);
}

module_init(lm3648_leds_init);
module_exit(llm3648_leds_exit);

//module_i2c_driver(lm3648_i2c_driver);

MODULE_DESCRIPTION("Texas Instruments Flash Lighting driver for LM3648");
MODULE_AUTHOR("Daniel Jeong <daniel.jeong@ti.com>");
MODULE_LICENSE("GPL v2");
