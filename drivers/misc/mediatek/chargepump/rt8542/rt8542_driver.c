/*
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2011 Bosch Sensortec GmbH
 * All Rights Reserved
 */


/* file rt8542.c
   brief This file contains all function implementations for the rt8542 in linux
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
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <mach/gpio_const.h>
#include <mt_gpio.h>

#include <linux/platform_device.h>

#include <linux/leds.h>

#define LCD_LED_MAX 0x7F
#define LCD_LED_MIN 0

#define DEFAULT_BRIGHTNESS 0x73	/* for 20mA */
#define RT8542_MIN_VALUE_SETTINGS 10	/* value leds_brightness_set */
#define RT8542_MAX_VALUE_SETTINGS 255	/* value leds_brightness_set */
#define MIN_MAX_SCALE(x) (((x) < RT8542_MIN_VALUE_SETTINGS) ? RT8542_MIN_VALUE_SETTINGS :\
(((x) > RT8542_MAX_VALUE_SETTINGS) ? RT8542_MAX_VALUE_SETTINGS:(x)))

#define BACKLIHGT_NAME "charge-pump"

#define RT8542_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)

#define RT8542_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))

#define RT8542_DEV_NAME "rt8542"

#define CPD_TAG                  "[ChargePump] "
#define CPD_FUN(f)               pr_debug(CPD_TAG"%s\n", __func__)
#define CPD_ERR(fmt, args...)    pr_err(CPD_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define CPD_LOG(fmt, args...)    pr_debug(CPD_TAG fmt, ##args)

/* I2C variable */
static struct i2c_client *new_client;
static const struct i2c_device_id rt8542_i2c_id[] = { {RT8542_DEV_NAME, 0}, {} };
static struct i2c_board_info i2c_rt8542 __initdata = { I2C_BOARD_INFO(RT8542_DEV_NAME, 0x39) };

static int rt8542_driver_probe(struct i2c_client *client, const struct i2c_device_id *id);

#ifdef CONFIG_OF
static const struct of_device_id rt8542_of_match[] = {
	{.compatible = "mediatek,BACKLIGHT",},
	{},
};

MODULE_DEVICE_TABLE(of, rt8542_of_match);
#endif

static struct i2c_driver rt8542_driver = {
	.driver = {
		   .name = "rt8542",
#ifdef CONFIG_OF
		   .of_match_table = rt8542_of_match,
#endif
		   },
	.probe = rt8542_driver_probe,
	.id_table = rt8542_i2c_id,
};

/* Flash control */
unsigned char strobe_ctrl;
unsigned char flash_ctrl;
unsigned char flash_status;
/* #define GPIO_LCD_BL_EN GPIO_LCM_BL_EN */
/* #define GPIO_LCD_BL_EN_M_GPIO GPIO_LCM_BL_EN_M_GPIO */

/* #define USING_LCM_BL_EN */

#if 1
#define GPIO_LCD_BL_EN         (GPIO63 | 0x80000000)
#define GPIO_LCD_BL_EN_M_GPIO   GPIO_MODE_00
#endif

/* Gamma 2.2 Table */
unsigned char bright_arr[] = {
	10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12,
	13, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 19, 19, 20, 21, 21, 22, 23, 24,
	24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 38, 39, 40, 41, 43, 44, 45,
	47, 48, 49, 51, 52, 54, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 73, 74, 76, 78,
	80, 82, 84, 86, 89, 91, 93, 95, 97, 100, 102, 104, 106, 109, 111, 114, 116, 119, 121, 124,
	    127
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static unsigned char current_brightness;
#endif
static unsigned char is_suspend;

struct semaphore rt8542_lock;

/* generic */
#define RT8542_MAX_RETRY_I2C_XFER (100)
#define RT8542_I2C_WRITE_DELAY_TIME 1

/* i2c read routine for API*/
static char rt8542_i2c_read(struct i2c_client *client, u8 reg_addr, u8 *data, u8 len)
{
#if !defined BMA_USE_BASIC_I2C_FUNC
	s32 dummy;

	if (NULL == client)
		return -1;

	while (0 != len--) {
#ifdef BMA_SMBUS
		dummy = i2c_smbus_read_byte_data(client, reg_addr);
		if (dummy < 0) {
			CPD_ERR("i2c bus read error");
			return -1;
		}
		*data = (u8) (dummy & 0xff);
#else
		dummy = i2c_master_send(client, (char *)&reg_addr, 1);
		if (dummy < 0) {
			CPD_ERR("send dummy is %d", dummy);
			return -1;
		}

		dummy = i2c_master_recv(client, (char *)data, 1);
		if (dummy < 0) {
			CPD_ERR("recv dummy is %d", dummy);
			return -1;
		}
#endif
		reg_addr++;
		data++;
	}
	return 0;
#else
	int retry;

	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = &reg_addr,
		 },

		{
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		 .len = len,
		 .buf = data,
		 },
	};

	for (retry = 0; retry < RT8542_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		mdelay(RT8542_I2C_WRITE_DELAY_TIME);
	}

	if (RT8542_MAX_RETRY_I2C_XFER <= retry) {
		CPD_ERR("I2C xfer error");
		return -EIO;
	}

	return 0;
#endif
}

/* i2c write routine for */
static char rt8542_i2c_write(struct i2c_client *client, u8 reg_addr, u8 *data, u8 len)
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
		for (retry = 0; retry < RT8542_MAX_RETRY_I2C_XFER; retry++) {
			if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
				break;
			mdelay(RT8542_I2C_WRITE_DELAY_TIME);
		}
		if (RT8542_MAX_RETRY_I2C_XFER <= retry)
			return -EIO;
		reg_addr++;
		data++;
	}
#endif
	return 0;
}

static int rt8542_smbus_read_byte(struct i2c_client *client,
				  unsigned char reg_addr, unsigned char *data)
{
	return rt8542_i2c_read(client, reg_addr, data, 1);
}

static int rt8542_smbus_write_byte(struct i2c_client *client,
				   unsigned char reg_addr, unsigned char *data)
{
	int ret_val = 0;
	int i = 0;

	ret_val = rt8542_i2c_write(client, reg_addr, data, 1);

	for (i = 0; i < 5; i++) {
		if (ret_val != 0)
			rt8542_i2c_write(client, reg_addr, data, 1);
		else
			return ret_val;
	}
	return ret_val;
}

#if 0				/* sangcheol.seo 150728 bring up */
static int rt8542_smbus_read_byte_block(struct i2c_client *client,
					unsigned char reg_addr, unsigned char *data,
					unsigned char len)
{
	return rt8542_i2c_read(client, reg_addr, data, len);
}
#endif

bool check_charger_pump_vendor(void)
{
	int err = 0;
	unsigned char data = 0;

	err = rt8542_smbus_read_byte(new_client, 0x01, &data);

	if (err < 0)
		CPD_ERR("read charge-pump vendor id fail\n");

/* CPD_ERR("vendor is 0x%x\n"); */

	if ((data & 0x03) == 0x03)	/* Richtek */
		return 0;
	else
		return 1;
}


int chargepump_set_backlight_level(unsigned int level)
{
	unsigned char data = 0;
	unsigned char data1 = 0;
	unsigned int bright_per = 0;
	unsigned int results = 0;	/* sangcheol.seo@lge.com */
	int ret = 0;

	if (level == 0) {
		results = down_interruptible(&rt8542_lock);
		data1 = 0x00;	/* backlight2 brightness 0 */
		rt8542_smbus_write_byte(new_client, 0x05, &data1);

		rt8542_smbus_read_byte(new_client, 0x0A, &data1);
		data1 &= 0x66;

		rt8542_smbus_write_byte(new_client, 0x0A, &data1);
		up(&rt8542_lock);
		if (flash_status == 0) {
#ifdef USING_LCM_BL_EN
			mt_set_gpio_out(GPIO_LCM_BL_EN, GPIO_OUT_ZERO);
#else
			mt_set_gpio_out(GPIO_LCD_BL_EN, GPIO_OUT_ZERO);
#endif
		}
		is_suspend = 1;
	} else {
		level = MIN_MAX_SCALE(level);

		/* Gamma 2.2 Table adapted */
		bright_per = (level - (unsigned int)10) * (unsigned int)100 / (unsigned int)245;
		data = bright_arr[bright_per];

/* data = 0x70;//force the backlight on if level > 0 <==Add by Minrui for backlight debug */

		if (is_suspend == 1) {
			is_suspend = 0;
#ifdef USING_LCM_BL_EN
			mt_set_gpio_out(GPIO_LCM_BL_EN, GPIO_OUT_ONE);
#else
			mt_set_gpio_out(GPIO_LCD_BL_EN, GPIO_OUT_ONE);
#endif
			mdelay(10);
			results = down_interruptible(&rt8542_lock);
			if (check_charger_pump_vendor() == 0) {
				data1 = 0x54;	/* 0x37; */
				rt8542_smbus_write_byte(new_client, 0x02, &data1);
				CPD_LOG("[ChargePump]-Richtek\n");
			} else {
				data1 = 0x54;	/* 0x57; */
				rt8542_smbus_write_byte(new_client, 0x02, &data1);
				CPD_LOG("[RT8542]-TI\n");
			}

			ret = rt8542_smbus_write_byte(new_client, 0x05, &data);
			CPD_LOG("[RT8542]-wake up I2C check = %d\n", ret);
			CPD_LOG("[RT8542]-backlight brightness Setting[reg0x05][value:0x%x]\n",
				data);

			rt8542_smbus_read_byte(new_client, 0x0A, &data1);
			data1 |= 0x19;

			rt8542_smbus_write_byte(new_client, 0x0A, &data1);
			up(&rt8542_lock);
		}

		results = down_interruptible(&rt8542_lock);
		{
			unsigned char read_data = 0;

			rt8542_smbus_read_byte(new_client, 0x02, &read_data);
			CPD_LOG("[RT8542]-OVP[0x%x]\n", read_data);
		}

		CPD_LOG("[RT8542]-backlight Seting[reg0x05][value:0x%x]\n", data);
		ret = rt8542_smbus_write_byte(new_client, 0x05, &data);
		CPD_LOG("[RT8542]-I2C check = %d\n", ret);
		up(&rt8542_lock);
	}
	return 0;
}

/* Need to check after bring up */
unsigned char get_rt8542_backlight_level(void)
{
	unsigned char rt8542_msb = 0;
	unsigned char rt8542_lsb = 0;
	unsigned char rt8542_level = 0;

	rt8542_smbus_read_byte(new_client, 0x04, &rt8542_msb);
	rt8542_smbus_read_byte(new_client, 0x05, &rt8542_lsb);

	rt8542_level |= ((rt8542_msb & 0x3) << 6);
	rt8542_level |= ((rt8542_lsb & 0xFC) >> 2);

	return rt8542_level;

}

void set_rt8542_backlight_level(unsigned char level)
{
	unsigned char rt8542_msb = 0;
	unsigned char rt8542_lsb = 0;
	unsigned char data = 0;

	if (level == 0)
		chargepump_set_backlight_level(level);
	else {
		if (is_suspend == 1) {
			is_suspend = 0;

			data = 0x70;	/* 0x57; */
			rt8542_smbus_write_byte(new_client, 0x02, &data);
			data = 0x00;	/* 11bit / */
			rt8542_smbus_write_byte(new_client, 0x03, &data);

			rt8542_msb = (level >> 6) | 0x04;
			rt8542_lsb = (level << 2) | 0x03;

			rt8542_smbus_write_byte(new_client, 0x04, &rt8542_msb);
			rt8542_smbus_write_byte(new_client, 0x05, &rt8542_lsb);

			rt8542_smbus_read_byte(new_client, 0x0A, &data);
			data |= 0x19;

			rt8542_smbus_write_byte(new_client, 0x0A, &data);
		} else {
			rt8542_msb = (level >> 6) | 0x04;
			rt8542_lsb = (level << 2) | 0x03;

			rt8542_smbus_write_byte(new_client, 0x04, &rt8542_msb);
			rt8542_smbus_write_byte(new_client, 0x05, &rt8542_lsb);
		}
	}
}

/* Need to check after bring up */
static int rt8542_driver_probe(struct i2c_client *client, const struct i2c_device_id *id)
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

static int rt8542_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	new_client = client;

	CPD_FUN();
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CPD_LOG("i2c_check_functionality error\n");
		return -1;
	}
/* sema_init(&rt8542_lock, 1); */

	if (client == NULL)
		CPD_ERR("client is NULL\n");
	else
		CPD_LOG("%p %x %x\n", client->adapter, client->addr, client->flags);
	return 0;
}


static int rt8542_i2c_remove(struct i2c_client *client)
{
	CPD_FUN();
	new_client = NULL;
	return 0;
}


static int
	__attribute__ ((unused)) rt8542_detect(struct i2c_client *client, int kind,
					   struct i2c_board_info *info)
{
	CPD_FUN();
	return 0;
}

static struct i2c_driver rt8542_i2c_driver = {
	.driver.name = RT8542_DEV_NAME,
	.probe = rt8542_i2c_probe,
	.remove = rt8542_i2c_remove,
	.id_table = rt8542_i2c_id,
};

static int rt8542_pd_probe(struct platform_device *pdev)
{
	CPD_FUN();

	/* i2c number 1(0~2) control */
	i2c_register_board_info(2, &i2c_rt8542, 1);

#ifdef USING_LCM_BL_EN
	mt_set_gpio_mode(GPIO_LCM_BL_EN, GPIO_LCM_BL_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_LCM_BL_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_LCM_BL_EN, GPIO_DIR_OUT);
#else
	mt_set_gpio_mode(GPIO_LCD_BL_EN, GPIO_LCD_BL_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_LCD_BL_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_LCD_BL_EN, GPIO_DIR_OUT);
#endif
/* i2c_add_driver(&rt8542_i2c_driver); */
	if (i2c_add_driver(&rt8542_driver) != 0)
		CPD_ERR("Failed to register rt8542 driver");
	return 0;
}

static int __attribute__ ((unused)) rt8542_pd_remove(struct platform_device *pdev)
{
	CPD_FUN();
	i2c_del_driver(&rt8542_i2c_driver);
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void rt8542_early_suspend(struct early_suspend *h)
{
	int err = 0;
	unsigned char data;

	CPD_FUN();

	down_interruptible(&rt8542_lock);
	data = 0x00;		/* backlight2 brightness 0 */
	err = rt8542_smbus_write_byte(new_client, 0x05, &data);

	err = rt8542_smbus_read_byte(new_client, 0x0A, &data);
	data &= 0x66;

	err = rt8542_smbus_write_byte(new_client, 0x0A, &data);
	up(&rt8542_lock);
	CPD_LOG("[RT8542] rt8542_early_suspend  [%d]\n", data);
#ifdef USING_LCM_BL_EN
	mt_set_gpio_out(GPIO_LCM_BL_EN, GPIO_OUT_ZERO);
#else
	mt_set_gpio_out(GPIO_LCD_BL_EN, GPIO_OUT_ZERO);
#endif
}

void rt8542_flash_strobe_prepare(char OnOff, char ActiveHigh)
{
	int err = 0;

	CPD_FUN();

	down_interruptible(&rt8542_lock);

	err = rt8542_smbus_read_byte(new_client, 0x09, &strobe_ctrl);

	err = rt8542_smbus_read_byte(new_client, 0x0A, &flash_ctrl);

	strobe_ctrl &= 0xF3;

	if (ActiveHigh)
		strobe_ctrl |= 0x20;
	else
		strobe_ctrl &= 0xDF;

	if (OnOff == 1) {
		CPD_LOG("Strobe mode On\n");
		strobe_ctrl |= 0x10;
		flash_ctrl |= 0x66;
		flash_status = 1;
		CPD_LOG("[RT8542][Strobe] flash_status = %d\n", flash_status);
	} else if (OnOff == 2) {
		CPD_LOG("Torch mode On\n");
		strobe_ctrl |= 0x10;
		flash_ctrl |= 0x62;
		flash_status = 1;
		CPD_LOG("[RT8542][Torch] flash_status = %d\n", flash_status);
	} else {
		CPD_LOG("Flash Off\n");
		strobe_ctrl &= 0xEF;
		flash_ctrl &= 0x99;
		flash_status = 0;
		CPD_LOG("[RT8542][off] flash_status = %d\n", flash_status);
	}

	err = rt8542_smbus_write_byte(new_client, 0x09, &strobe_ctrl);

	up(&rt8542_lock);
}
EXPORT_SYMBOL(rt8542_flash_strobe_prepare);

/* strobe enable */
void rt8542_flash_strobe_en(void)
{
	int err = 0;

	CPD_FUN();
	down_interruptible(&rt8542_lock);
	err = rt8542_smbus_write_byte(new_client, 0x0A, &flash_ctrl);
	up(&rt8542_lock);
}
EXPORT_SYMBOL(rt8542_flash_strobe_en);


/* strobe level */
void rt8542_flash_strobe_level(char level)
{
	int err = 0;
	unsigned char data1 = 0;
	unsigned char data2 = 0;
	unsigned char torch_level;
	unsigned char strobe_timeout = 0x1F;

	CPD_FUN();

	down_interruptible(&rt8542_lock);
	torch_level = 0x50;

	err = rt8542_smbus_read_byte(new_client, 0x06, &data1);

	strobe_timeout = 0x0F;
	if (level < 0)
		data1 = torch_level;
	else if (level == 1)
		data1 = torch_level | 0x03;
	else if (level == 2)
		data1 = torch_level | 0x05;
	else if (level == 3)
		data1 = torch_level | 0x08;
	else if (level == 4)
		data1 = torch_level | 0x0A;
	else
		data1 = torch_level | level;

	CPD_LOG("Flash Level =0x%x\n", data1);
	err = rt8542_smbus_write_byte(new_client, 0x06, &data1);

	data2 = 0x40 | strobe_timeout;
	CPD_LOG("Storbe Timeout =0x%x\n", data2);
	err |= rt8542_smbus_write_byte(new_client, 0x07, &data2);
	up(&rt8542_lock);
}
EXPORT_SYMBOL(rt8542_flash_strobe_level);

static void rt8542_late_resume(struct early_suspend *h)
{
	int err = 0;
	unsigned char data1;

	CPD_FUN();

#ifdef USING_LCM_BL_EN
	mt_set_gpio_out(GPIO_LCM_BL_EN, GPIO_OUT_ONE);
#else
	mt_set_gpio_out(GPIO_LCD_BL_EN, GPIO_OUT_ONE);
#endif
	mdelay(50);
	down_interruptible(&rt8542_lock);
	err = rt8542_smbus_write_byte(new_client, 0x05, &current_brightness);

	err = rt8542_smbus_read_byte(new_client, 0x0A, &data1);
	data1 |= 0x19;		/* backlight enable */

	err = rt8542_smbus_write_byte(new_client, 0x0A, &data1);
	up(&rt8542_lock);
	CPD_LOG("[RT8542] rt8542_late_resume  [%d]\n", data1);
}

static struct early_suspend __attribute__ ((unused)) rt8542_early_suspend_desc = {
.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN, .suspend = rt8542_early_suspend, .resume =
	    rt8542_late_resume,};
#endif

static struct platform_driver rt8542_backlight_driver = {
	.remove = rt8542_pd_remove,
	.probe = rt8542_pd_probe,
	.driver = {
		   .name = BACKLIHGT_NAME,
		   .owner = THIS_MODULE,
		   },
};

#if 0
#ifdef CONFIG_OF
static struct platform_device mtk_backlight_dev = {
	.name = BACKLIHGT_NAME,
	.id = -1,
};
#endif
#endif
static int __init rt8542_init(void)
{
	CPD_FUN();
	sema_init(&rt8542_lock, 1);

#if 0
#ifdef CONFIG_OF
	if (platform_device_register(&mtk_backlight_dev)) {
		CPD_ERR("failed to register device");
		return -1;
	}
#endif

#ifndef	CONFIG_MTK_LEDS
	register_early_suspend(&rt8542_early_suspend_desc);
#endif

	if (platform_driver_register(&rt8542_backlight_driver)) {
		CPD_ERR("failed to register driver");
		return -1;
	}
#else

	/* i2c number 1(0~2) control */
	i2c_register_board_info(2, &i2c_rt8542, 1);

	/*if(platform_driver_register(&rt8542_backlight_driver))
	   {
	   CPD_ERR("failed to register driver");
	   return -1;
	   } */
#ifdef USING_LCM_BL_EN
	mt_set_gpio_mode(GPIO_LCM_BL_EN, GPIO_LCM_BL_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_LCM_BL_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_LCM_BL_EN, GPIO_DIR_OUT);
#else
	mt_set_gpio_mode(GPIO_LCD_BL_EN, GPIO_LCD_BL_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_LCD_BL_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_LCD_BL_EN, GPIO_DIR_OUT);
#endif

/* i2c_add_driver(&rt8542_i2c_driver); */
	if (i2c_add_driver(&rt8542_driver) != 0)
		CPD_ERR("Failed to register rt8542 driver");
#endif

	return 0;
}

static void __exit rt8542_exit(void)
{
	platform_driver_unregister(&rt8542_backlight_driver);
}

MODULE_AUTHOR("Albert Zhang <xu.zhang@bosch-sensortec.com>");
MODULE_DESCRIPTION("rt8542 driver");
MODULE_LICENSE("GPL");

late_initcall(rt8542_init);
module_exit(rt8542_exit);
