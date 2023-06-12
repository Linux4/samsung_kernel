/*
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2011 Bosch Sensortec GmbH
 * All Rights Reserved
 */


/* file sm5306.c
   brief This file contains all function implementations for the sm5306 in linux
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


/* jongmahn.kim E0 bring up

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#include <mach/mt_pm_ldo.h>
#include <cust_acc.h>

#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <linux/hwmsen_helper.h>

*/

#include <linux/platform_device.h>
#include <linux/leds.h>

#define LCD_LED_MAX 0x7F
#define LCD_LED_MIN 0

#define DEFAULT_BRIGHTNESS 0x73	/* for 20mA */
#define sm5306_MIN_VALUE_SETTINGS 10	/* value leds_brightness_set */
#define sm5306_MAX_VALUE_SETTINGS 255	/* value leds_brightness_set */
#define MIN_MAX_SCALE(x)\
(((x) < sm5306_MIN_VALUE_SETTINGS) ? \
sm5306_MIN_VALUE_SETTINGS : (((x) > sm5306_MAX_VALUE_SETTINGS) ?\
sm5306_MAX_VALUE_SETTINGS:(x)))

#define BACKLIHGT_NAME "chargepump"

#define sm5306_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)

#define sm5306_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))

#define CPD_TAG                  "[ChargePump] "
#define CPD_FUN(f)               pr_debug(CPD_TAG"%s\n", __func__)
#define CPD_ERR(fmt, args...)    pr_err(CPD_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define CPD_LOG(fmt, args...)    pr_debug(CPD_TAG fmt, ##args)

/* I2C variable */
static struct i2c_client *new_client;

#define REG_CTRL         0x00
#define REG_CONFIG       0x01
#define REG_BRT_A        0x03
#define REG_BRT_B        0x04
#define REG_INT_STATUS   0x09
#define REG_INT_EN       0x0A
#define REG_FAULT        0x0B
#define REG_PWM_OUTLOW   0x12
#define REG_PWM_OUTHIGH  0x13
#define REG_MAX          0x1F

/* modified jongmahn.kim M-OS  */
/* #define GPIO_LCD_BL_EN        GPIO_LCM_BL_EN */
/* #define GPIO_LCD_BL_EN_M_GPIO GPIO_LCM_BL_EN_M_GPIO */
#ifndef GPIO_LCD_BL_EN
#define GPIO_LCD_BL_EN         (GPIO65 | 0x80000000)
#define GPIO_LCD_BL_EN_M_GPIO  GPIO_MODE_00
#endif

/* Gamma 2.2 Table */
unsigned char bright_arr[] = {
	10, 10, 10, 50, 50, 60, 80, 90, 90, 90, 100, 100, 110, 110, 110, 120, 120, 120, 130, 130,
	130, 140, 140, 140, 140, 150, 150, 150, 160, 160, 160, 165, 165, 165, 170, 170, 170, 175,
	    175, 175,
	180, 180, 180, 180, 180, 180, 185, 185, 185, 185, 185, 185, 190, 190, 190, 190, 195, 195,
	    195, 195,
	200, 200, 200, 200, 205, 205, 205, 210, 210, 210, 210, 215, 215, 215, 220, 220, 220, 225,
	    225, 225,
	230, 230, 230, 230, 240, 240, 240, 245, 245, 245, 250, 250, 250, 250, 250, 250, 250, 250,
	    255, 255, 255
};



#ifdef CONFIG_HAS_EARLYSUSPEND
static unsigned char current_brightness;
#endif
static unsigned char is_suspend;

struct semaphore sm5306_lock;

/* generic */
#define sm5306_MAX_RETRY_I2C_XFER (100)
#define sm5306_I2C_WRITE_DELAY_TIME 1

/* i2c read routine for API*/
static char sm5306_i2c_read(struct i2c_client *client, u8 reg_addr, u8 *data, u8 len)
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

	for (retry = 0; retry < sm5306_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;

		mdelay(sm5306_I2C_WRITE_DELAY_TIME);
	}

	if (sm5306_MAX_RETRY_I2C_XFER <= retry) {
		CPD_ERR("I2C xfer error");
		return -EIO;
	}

	return 0;
#endif
}

/* i2c write routine for */
static char sm5306_i2c_write(struct i2c_client *client, u8 reg_addr, u8 *data, u8 len)
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
		for (retry = 0; retry < sm5306_MAX_RETRY_I2C_XFER; retry++) {
			if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
				break;
			mdelay(sm5306_I2C_WRITE_DELAY_TIME);
		}
		if (sm5306_MAX_RETRY_I2C_XFER <= retry)
			return -EIO;

		reg_addr++;
		data++;
	}
#endif
	return 0;
}

static int sm5306_smbus_read_byte(struct i2c_client *client,
				  unsigned char reg_addr, unsigned char *data)
{
	return sm5306_i2c_read(client, reg_addr, data, 1);
}

static int sm5306_smbus_write_byte(struct i2c_client *client,
				   unsigned char reg_addr, unsigned char *data)
{
	int ret_val = 0;
	int i = 0;

	ret_val = sm5306_i2c_write(client, reg_addr, data, 1);

	for (i = 0; i < 5; i++) {
		if (ret_val != 0)
			sm5306_i2c_write(client, reg_addr, data, 1);
		else
			return ret_val;
	}
	return ret_val;
}

#if 0				/* jongmahn.kim@lge.com */
static int sm5306_smbus_read_byte_block(struct i2c_client *client,
					unsigned char reg_addr, unsigned char *data,
					unsigned char len)
{
	return sm5306_i2c_read(client, reg_addr, data, len);
}
#endif

bool check_charger_pump_vendor(void)
{
	int err = 0;
	unsigned char data = 0;

	err = sm5306_smbus_read_byte(new_client, 0x01, &data);

	if (err < 0)
		CPD_ERR("read charge-pump vendor id fail\n");

/* CPD_ERR("vendor is 0x%x\n"); */

	if ((data & 0x03) == 0x03)	/* Richtek */
		return 0;
	else
		return 1;
}


/****    sm5306 : jongmahn.kim@lge.com       ****/
int chargepump_set_backlight_level(unsigned int level)
{
	unsigned int results = 0;	/* add jongmahn.kim@lge.com */
	unsigned char bright_data = 0;
	unsigned char ctrl_data = 0;
	unsigned int bright_per = 0;

	if (level == 0) {
		results = down_interruptible(&sm5306_lock);
		ctrl_data = 0x00;	/* backlight brightness 0 */
		sm5306_smbus_write_byte(new_client, REG_BRT_A, &ctrl_data);	/* brightness MSB control */
		sm5306_smbus_write_byte(new_client, REG_BRT_B, &ctrl_data);	/* brightness LSB control */

		sm5306_smbus_read_byte(new_client, REG_CTRL, &ctrl_data);
		ctrl_data &= 0xF9;	/* LED_EN 1,2 disable */
		sm5306_smbus_write_byte(new_client, REG_CTRL, &ctrl_data);


		sm5306_smbus_read_byte(new_client, REG_CTRL, &ctrl_data);
		ctrl_data |= 0x80;	/* sleep cmd = 1 */
		sm5306_smbus_write_byte(new_client, REG_CTRL, &ctrl_data);


		mt_set_gpio_out(GPIO_LCD_BL_EN, GPIO_OUT_ZERO);
		is_suspend = 1;

		up(&sm5306_lock);

	} else {
		level = MIN_MAX_SCALE(level);
		/* Gamma 2.2 Table adapted */
		bright_per = (level - (unsigned int)10) * (unsigned int)100 / (unsigned int)245;
		bright_data = bright_arr[bright_per];

		if (is_suspend == 1) {
			is_suspend = 0;
			mt_set_gpio_out(GPIO_LCD_BL_EN, GPIO_OUT_ONE);
			mdelay(10);
			results = down_interruptible(&sm5306_lock);

			CPD_LOG("[ChargePump] - is_suspend == 1\n");

			sm5306_smbus_read_byte(new_client, REG_CTRL, &ctrl_data);
			ctrl_data &= 0x7F;	/* sleep cmd =0 */
			ctrl_data |= 0x06;	/* bled1,2 en | backlight en */
			sm5306_smbus_write_byte(new_client, REG_CTRL, &ctrl_data);

			ctrl_data = 0xFF;

			sm5306_smbus_write_byte(new_client, REG_BRT_A, &bright_data);	/* brighhtness control */

			CPD_LOG("[sm5306]-backlight brightness Setting[reg0x05][value:0x%x]\n",
				bright_data);

			up(&sm5306_lock);
		}

		results = down_interruptible(&sm5306_lock);

		CPD_LOG("[sm5306]-backlight Seting[reg0x05][value:0x%x]\n", bright_data);

		sm5306_smbus_write_byte(new_client, REG_BRT_A, &bright_data);
		up(&sm5306_lock);
	}
	return 0;
}

static int sm5306_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;

	CPD_FUN();

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CPD_LOG("i2c_check_functionality error\n");
		err = -ENODEV;
		goto exit;
	}

	new_client = client;

	return 0;

exit:
	return err;
}

static int
__attribute__ ((unused)) sm5306_detect(struct i2c_client *client, int kind,
					   struct i2c_board_info *info)
{
	CPD_FUN();
	return 0;
}

static const struct i2c_device_id sm5306_i2c_id[] = {
	{"sm5306", 0},
	{},
};

#ifdef CONFIG_OF
static const struct of_device_id sm5306_of_match[] = {
	{.compatible = "mediatek,sm5306",},
	{},
};

MODULE_DEVICE_TABLE(of, sm5306_of_match);
#endif

static struct i2c_driver sm5306_driver = {
	.driver = {
		   .name = "sm5306",
#ifdef CONFIG_OF
		   .of_match_table = sm5306_of_match,
#endif
		   },
	.probe = sm5306_probe,
	.id_table = sm5306_i2c_id,
};

static int __init sm5306_init(void)
{
	CPD_FUN();
	sema_init(&sm5306_lock, 1);

	if (i2c_add_driver(&sm5306_driver) != 0)
		CPD_ERR("Failed to register sm5306 driver");

	return 0;
}

static void __exit sm5306_exit(void)
{
	i2c_del_driver(&sm5306_driver);
}

MODULE_DESCRIPTION("sm5306 driver");
MODULE_LICENSE("GPL");

module_init(sm5306_init);
module_exit(sm5306_exit);
