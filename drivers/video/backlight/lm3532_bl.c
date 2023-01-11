 /*
  * Copyright (C) 2008-2009 Motorola, Inc.
  * Author: Alina Yakovleva <qvdh43@motorola.com>
  *
  * This program is free software; you can redistribute it and/or
  * modify it under the terms of the GNU General Public License
  * as published by the Free Software Foundation; either version 2
  * of the License, or (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/sysfs.h>
#include <linux/hwmon-sysfs.h>
#include <linux/gpio_event.h>
#include <linux/leds.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/lm3532_bl.h>
#include <linux/backlight.h>

#define LM3532_INIT_REQUESTS 8
#define LM3532_BL_MAX_BRIGHTNESS 0xff

static unsigned trace_suspend = 1;
module_param(trace_suspend, uint, 0664);
#define printk_suspend(fmt, args...)                 \
({                                                   \
	if (trace_suspend)                           \
		pr_info(fmt, ##args);                \
})

#ifdef LM3532_DEBUG
static unsigned trace_brightness;
module_param(trace_brightness, uint, 0664);
#define printk_br(fmt, args...)                     \
({                                                  \
	if (trace_brightness)                       \
		pr_info(fmt, ##args);               \
})

static unsigned trace_write;
module_param(trace_write, uint, 0664);
#define printk_write(fmt, args...)               \
({                                               \
	if (trace_write)                         \
		pr_info(fmt, ##args);            \
})
#else
#define printk_br(fmt, args...)
#define printk_write(fmt, args...)
#endif

struct lm3532_data {
	uint16_t addr;
	struct i2c_client *client;
	struct input_dev *idev;
	struct lm3532_platform_data *pdata;
	struct backlight_device *bl;
	unsigned initialized;
	unsigned in_webtop_mode;
	uint8_t revision;
	uint8_t enable_reg;
	atomic_t suspended;
	atomic_t in_suspend;	/* Whether the driver is in TCMD SUSPEND mode */
	unsigned bvalue;	/* Current brightness register value */
	unsigned saved_bvalue;	/* Brightness before TCMD SUSPEND */
	struct work_struct work;
};

static DEFINE_MUTEX(lm3532_mutex);

struct lm3532_reg {
	const char *name;
	uint8_t reg;
} lm3532_r0_regs[] = {
	{
	"OUTPUT_CFG_REG", LM3532_OUTPUT_CFG_REG}, {
	"START_UP_RAMP_REG", LM3532_START_UP_RAMP_REG}, {
	"RUN_TIME_RAMP_REG", LM3532_RUN_TIME_RAMP_REG}, {
	"CTRL_A_PWM_REG", LM3532_CTRL_A_PWM_REG}, {
	"CTRL_B_PWM_REG", LM3532_CTRL_B_PWM_REG}, {
	"CTRL_C_PWM_REG", LM3532_CTRL_C_PWM_REG}, {
	"CTRL_A_BR_CFG_REG", LM3532_CTRL_A_BR_CFG_REG}, {
	"CTRL_B_BR_CFG_REG", LM3532_R0_CTRL_B_BR_CFG_REG}, {
	"CTRL_C_BR_CFG_REG", LM3532_R0_CTRL_C_BR_CFG_REG}, {
	"ENABLE_REG", LM3532_R0_ENABLE_REG}, {
	"ALS1_RES_SEL_REG", LM3532_ALS1_RES_SEL_REG}, {
	"ALS2_RES_SEL_REG", LM3532_ALS2_RES_SEL_REG}, {
	"ALS_CFG_REG", LM3532_R0_ALS_CFG_REG}, {
	"ALS_ZONE_REG", LM3532_R0_ALS_ZONE_REG}, {
	"ALS_BR_ZONE_REG", LM3532_R0_ALS_BR_ZONE_REG}, {
	"ALS_UP_ZONE_REG", LM3532_R0_ALS_UP_ZONE_REG}, {
	"IND_BLINK1_REG", LM3532_R0_IND_BLINK1_REG}, {
	"IND_BLINK2_REG", LM3532_R0_IND_BLINK2_REG}, {
	"ZB1_REG", LM3532_ZB1_REG}, {
	"ZB2_REG", LM3532_ZB2_REG}, {
	"ZB3_REG", LM3532_ZB3_REG}, {
	"ZB4_REG", LM3532_ZB4_REG}, {
	"CTRL_A_ZT1_REG", LM3532_CTRL_A_ZT1_REG}, {
	"CTRL_A_ZT2_REG", LM3532_CTRL_A_ZT2_REG}, {
	"CTRL_A_ZT3_REG", LM3532_CTRL_A_ZT3_REG}, {
	"CTRL_A_ZT4_REG", LM3532_CTRL_A_ZT4_REG}, {
	"CTRL_A_ZT5_REG", LM3532_CTRL_A_ZT5_REG}, {
	"CTRL_B_ZT1_REG", LM3532_CTRL_B_ZT1_REG}, {
	"CTRL_B_ZT2_REG", LM3532_CTRL_B_ZT2_REG}, {
	"CTRL_B_ZT3_REG", LM3532_CTRL_B_ZT3_REG}, {
	"CTRL_B_ZT4_REG", LM3532_CTRL_B_ZT4_REG}, {
	"CTRL_B_ZT5_REG", LM3532_CTRL_B_ZT5_REG}, {
	"CTRL_C_ZT1_REG", LM3532_CTRL_C_ZT1_REG}, {
	"CTRL_C_ZT2_REG", LM3532_CTRL_C_ZT2_REG}, {
	"CTRL_C_ZT3_REG", LM3532_CTRL_C_ZT3_REG}, {
	"CTRL_C_ZT4_REG", LM3532_CTRL_C_ZT4_REG}, {
	"CTRL_C_ZT5_REG", LM3532_CTRL_C_ZT5_REG}, {
"VERSION_REG", LM3532_VERSION_REG},};

struct lm3532_reg lm3532_regs[] = {
	{"OUTPUT_CFG_REG", LM3532_OUTPUT_CFG_REG},
	{"START_UP_RAMP_REG", LM3532_START_UP_RAMP_REG},
	{"RUN_TIME_RAMP_REG", LM3532_RUN_TIME_RAMP_REG},
	{"CTRL_A_PWM_REG", LM3532_CTRL_A_PWM_REG},
	{"CTRL_B_PWM_REG", LM3532_CTRL_B_PWM_REG},
	{"CTRL_C_PWM_REG", LM3532_CTRL_C_PWM_REG},
	{"CTRL_A_BR_CFG_REG", LM3532_CTRL_A_BR_CFG_REG},
	{"CTRL_A_FS_CURR_REG", LM3532_CTRL_A_FS_CURR_REG},
	{"CTRL_B_BR_CFG_REG", LM3532_CTRL_B_BR_CFG_REG},
	{"CTRL_B_FS_CURR_REG", LM3532_CTRL_B_FS_CURR_REG},
	{"CTRL_C_BR_CFG_REG", LM3532_CTRL_C_BR_CFG_REG},
	{"CTRL_C_FS_CURR_REG", LM3532_CTRL_C_FS_CURR_REG},
	{"ENABLE_REG", LM3532_ENABLE_REG},
	{"FEEDBACK_ENABLE_REG", LM3532_FEEDBACK_ENABLE_REG},
	{"ALS1_RES_SEL_REG", LM3532_ALS1_RES_SEL_REG},
	{"ALS2_RES_SEL_REG", LM3532_ALS2_RES_SEL_REG},
	{"ALS_CFG_REG", LM3532_ALS_CFG_REG},
	{"ALS_ZONE_REG", LM3532_ALS_ZONE_REG},
	{"ALS_BR_ZONE_REG", LM3532_ALS_BR_ZONE_REG},
	{"ALS_UP_ZONE_REG", LM3532_ALS_UP_ZONE_REG},
	{"ZB1_REG", LM3532_ZB1_REG},
	{"ZB2_REG", LM3532_ZB2_REG},
	{"ZB3_REG", LM3532_ZB3_REG},
	{"ZB4_REG", LM3532_ZB4_REG},
	{"CTRL_A_ZT1_REG", LM3532_CTRL_A_ZT1_REG},
	{"CTRL_A_ZT2_REG", LM3532_CTRL_A_ZT2_REG},
	{"CTRL_A_ZT3_REG", LM3532_CTRL_A_ZT3_REG},
	{"CTRL_A_ZT4_REG", LM3532_CTRL_A_ZT4_REG},
	{"CTRL_A_ZT5_REG", LM3532_CTRL_A_ZT5_REG},
	{"CTRL_B_ZT1_REG", LM3532_CTRL_B_ZT1_REG},
	{"CTRL_B_ZT2_REG", LM3532_CTRL_B_ZT2_REG},
	{"CTRL_B_ZT3_REG", LM3532_CTRL_B_ZT3_REG},
	{"CTRL_B_ZT4_REG", LM3532_CTRL_B_ZT4_REG},
	{"CTRL_B_ZT5_REG", LM3532_CTRL_B_ZT5_REG},
	{"CTRL_C_ZT1_REG", LM3532_CTRL_C_ZT1_REG},
	{"CTRL_C_ZT2_REG", LM3532_CTRL_C_ZT2_REG},
	{"CTRL_C_ZT3_REG", LM3532_CTRL_C_ZT3_REG},
	{"CTRL_C_ZT4_REG", LM3532_CTRL_C_ZT4_REG},
	{"CTRL_C_ZT5_REG", LM3532_CTRL_C_ZT5_REG},
	{"VERSION_REG", LM3532_VERSION_REG},
};

#ifdef LM3532_DEBUG
static const char *lm3532_reg_name(struct lm3532_data *driver_data, int reg)
{
	unsigned reg_count;
	int i;

	if (driver_data->revision == 0xF6) {
		reg_count = sizeof(lm3532_r0_regs) / sizeof(lm3532_r0_regs[0]);
		for (i = 0; i < reg_count; i++)
			if (reg == lm3532_r0_regs[i].reg)
				return lm3532_r0_regs[i].name;
	} else {
		reg_count = sizeof(lm3532_regs) / sizeof(lm3532_regs[0]);
		for (i = 0; i < reg_count; i++)
			if (reg == lm3532_regs[i].reg)
				return lm3532_regs[i].name;
	}
	return "UNKNOWN";
}
#endif

static int lm3532_read_reg(struct i2c_client *client,
			   unsigned reg, uint8_t *value)
{
	uint8_t buf[1];
	int ret = 0;

	if (!value)
		return -EINVAL;
	buf[0] = reg;
	ret = i2c_master_send(client, buf, 1);
	if (ret > 0) {
		msleep_interruptible(1);
		ret = i2c_master_recv(client, buf, 1);
		if (ret > 0)
			*value = buf[0];
	}
	return ret;
}

static int lm3532_write_reg(struct i2c_client *client,
			    unsigned reg, uint8_t value, const char *caller)
{
	uint8_t buf[2] = { reg, value };
	int ret = 0;
#ifdef LM3532_DEBUG
	struct lm3532_data *driver_data = i2c_get_clientdata(client);

	printk_write("%s: reg 0x%X (%s) = 0x%x\n", caller, buf[0],
		     lm3532_reg_name(driver_data, reg), buf[1]);
#endif
	ret = i2c_master_send(client, buf, 2);
	if (ret < 0)
		pr_err("%s: i2c_master_send error %d\n", caller, ret);
	return ret;
}

/*
 * This function calculates ramp step time so that total ramp time is
 * equal to ramp_time defined currently at 200ms
 */
static int lm3532_set_ramp(struct lm3532_data *driver_data,
			   unsigned int on, unsigned int nsteps,
			   unsigned int *rtime)
{
	int ret, i = 0;
	uint8_t value = 0;
	unsigned int total_time = 0;
	/* Ramp times in microseconds */
	unsigned int lm3532_ramp[] = { 8, 1000, 2000, 4000, 8000, 16000, 32000,
		64000
	};
	int nramp = sizeof(lm3532_ramp) / sizeof(lm3532_ramp[0]);
	unsigned ramp_time = driver_data->pdata->ramp_time * 1000;

	if (on) {
		/* Calculate the closest possible ramp time */
		for (i = 0; i < nramp; i++) {
			total_time = nsteps * lm3532_ramp[i];
			if (total_time >= ramp_time)
				break;
		}
		if (i > 0 && total_time > ramp_time) {
			i--;
			total_time = nsteps * lm3532_ramp[i];
		}
		value = i | (i << 3);
	} else
		value = 0;

	if (rtime)
		*rtime = total_time;
	ret = lm3532_write_reg(driver_data->client, LM3532_RUN_TIME_RAMP_REG,
			       value, __func__);
	ret = lm3532_write_reg(driver_data->client, LM3532_START_UP_RAMP_REG,
			       value, __func__);
	return ret;
}

static int lm3532_bl_brightness_set(struct backlight_device *bl)
{
	int nsteps = 0;
	unsigned int total_time = 0;
	unsigned do_ramp = 1;
	struct lm3532_data *driver_data;
	struct i2c_client *client;
	enum led_brightness value = bl->props.brightness;

	driver_data = bl_get_data(bl);
	client = driver_data->client;

	if (!driver_data->initialized) {
		pr_err("%s: not initialized\n", __func__);
		return -1;
	}

	if (driver_data->pdata->ramp_time == 0)
		do_ramp = 0;

	mutex_lock(&lm3532_mutex);

	/* Calculate number of steps for ramping */
	nsteps = value - driver_data->bvalue;
	if (nsteps < 0)
		nsteps = -nsteps;

	if (do_ramp) {
		lm3532_set_ramp(driver_data, do_ramp, nsteps,
				&total_time);
		printk_br("%s: 0x%x => 0x%x, %d steps, RT=%dus\n",
			  __func__, driver_data->bvalue,
			  value, nsteps, total_time);
	}

	lm3532_write_reg(client, LM3532_CTRL_A_ZT1_REG, value, __func__);
	driver_data->bvalue = value;

	mutex_unlock(&lm3532_mutex);

	return 0;
}

#ifdef CONFIG_OF
static void lm3532_power(struct i2c_client *client, int on)
{
	static struct regulator *bl_avdd;
	struct device_node *np = client->dev.of_node;
	int rst_en, ret = 0;
	const char *avdd_en = NULL;

	rst_en = of_get_named_gpio(np, "rst_gpio", 0);
	if (rst_en < 0) {
		pr_err("%s: of_get_named_gpio failed\n", __func__);
		return;
	}

	if (gpio_request(rst_en, "backlight reset gpio")) {
		pr_err("gpio %d request failed\n", rst_en);
		return;
	}

	avdd_en = of_get_property(np, "avdd-supply", 0);
	if (avdd_en && !bl_avdd) {
		bl_avdd = regulator_get(&client->dev, "avdd");
		if (IS_ERR(bl_avdd)) {
			pr_err("%s regulator get error!\n", __func__);
			gpio_free(rst_en);
			return;
		}
	}

	if (on) {
		if (avdd_en) {
			regulator_set_voltage(bl_avdd, 2800000, 2800000);
			ret = regulator_enable(bl_avdd);
		}
		gpio_direction_output(rst_en, 1);
		if (ret < 0)
			bl_avdd = NULL;
	} else {
		gpio_direction_output(rst_en, 0);
		if (avdd_en)
			regulator_disable(bl_avdd);
	}

	gpio_free(rst_en);
}
#else
static void lm3532_power(struct i2c_client *client, int on)
{
}
#endif

static int lm3532_bl_get_brightness(struct backlight_device *bl)
{
	uint8_t value;
	struct lm3532_data *driver_data;
	struct i2c_client *client;

	driver_data = container_of(&bl, struct lm3532_data, bl);
	client = driver_data->client;

	lm3532_read_reg(client, LM3532_CTRL_A_ZT1_REG, &value);

	return value;
}

static irqreturn_t lm3532_irq_handler(int irq, void *dev_id)
{
	struct lm3532_data *driver_data = dev_id;

	pr_debug("%s: got an interrupt %d\n", __func__, irq);

	disable_irq(irq);
	schedule_work(&driver_data->work);

	return IRQ_HANDLED;
}

static void lm3532_work_func(struct work_struct *work)
{
	struct lm3532_data *driver_data =
	    container_of(work, struct lm3532_data, work);

	enable_irq(driver_data->client->irq);
}

static int lm3532_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct lm3532_data *driver_data = i2c_get_clientdata(client);
	printk_suspend("%s: called with pm message %d\n", __func__, mesg.event);

	atomic_set(&driver_data->suspended, 1);
	if (driver_data->pdata->power_off)
		driver_data->pdata->power_off();
	lm3532_power(client, 0);

	return 0;
}

static int lm3532_configure(struct lm3532_data *driver_data)
{
	int ret = 0;
	static uint8_t r0_fs_current[] = {
		LM3532_R0_5mA_FS_CURRENT,	/* 0x00000 */
		LM3532_R0_5mA_FS_CURRENT,	/* 0x00001 */
		LM3532_R0_8mA_FS_CURRENT,	/* 0x00010 */
		LM3532_R0_8mA_FS_CURRENT,	/* 0x00011 */
		LM3532_R0_8mA_FS_CURRENT,	/* 0x00100 */
		LM3532_R0_8mA_FS_CURRENT,	/* 0x00101 */
		LM3532_R0_8mA_FS_CURRENT,	/* 0x00110 */
		LM3532_R0_12mA_FS_CURRENT,	/* 0x00111 */
		LM3532_R0_12mA_FS_CURRENT,	/* 0x01000 */
		LM3532_R0_12mA_FS_CURRENT,	/* 0x01001 */
		LM3532_R0_12mA_FS_CURRENT,	/* 0x01010 */
		LM3532_R0_15mA_FS_CURRENT,	/* 0x01011 */
		LM3532_R0_15mA_FS_CURRENT,	/* 0x01100 */
		LM3532_R0_15mA_FS_CURRENT,	/* 0x01101 */
		LM3532_R0_15mA_FS_CURRENT,	/* 0x01110 */
		LM3532_R0_19mA_FS_CURRENT,	/* 0x01111 */
		LM3532_R0_19mA_FS_CURRENT,	/* 0x10000 */
		LM3532_R0_19mA_FS_CURRENT,	/* 0x10001 */
		LM3532_R0_19mA_FS_CURRENT,	/* 0x10010 */
		LM3532_R0_19mA_FS_CURRENT,	/* 0x10011 */
		LM3532_R0_22mA_FS_CURRENT,	/* 0x10100 */
		LM3532_R0_22mA_FS_CURRENT,	/* 0x10101 */
		LM3532_R0_22mA_FS_CURRENT,	/* 0x10110 */
		LM3532_R0_22mA_FS_CURRENT,	/* 0x10111 */
		LM3532_R0_22mA_FS_CURRENT,	/* 0x11000 */
		LM3532_R0_26mA_FS_CURRENT,	/* 0x11001 */
		LM3532_R0_26mA_FS_CURRENT,	/* 0x11010 */
		LM3532_R0_26mA_FS_CURRENT,	/* 0x11011 */
		LM3532_R0_26mA_FS_CURRENT,	/* 0x11100 */
		LM3532_R0_29mA_FS_CURRENT,	/* 0x11101 */
		LM3532_R0_29mA_FS_CURRENT,	/* 0x11110 */
		LM3532_R0_29mA_FS_CURRENT,	/* 0x11111 */
	};
	uint8_t ctrl_a_fs_current;

	if (driver_data->revision == 0xF6) {
		/* Revision 0 */
		ctrl_a_fs_current =
		    r0_fs_current[driver_data->pdata->ctrl_a_fs_current];
		/* Map ILED1 to CTRL A, ILED2 to CTRL B, ILED3 to CTRL C */
		lm3532_write_reg(driver_data->client,
				 LM3532_OUTPUT_CFG_REG, 0x34, __func__);
		lm3532_write_reg(driver_data->client,
				 LM3532_CTRL_A_BR_CFG_REG,
				 LM3532_I2C_CONTROL | ctrl_a_fs_current |
				 driver_data->pdata->ctrl_a_mapping_mode,
				 __func__);

		ret = lm3532_write_reg(driver_data->client,
				       LM3532_CTRL_A_ZT2_REG, 0, __func__);
	} else {
		/* Revision 1 and above */
		lm3532_write_reg(driver_data->client,
				 LM3532_FEEDBACK_ENABLE_REG,
				 driver_data->pdata->feedback_en_val,
				 __func__);
		lm3532_write_reg(driver_data->client,
				 LM3532_CTRL_A_PWM_REG,
				 driver_data->pdata->ctrl_a_pwm, __func__);
		lm3532_write_reg(driver_data->client,
				 LM3532_OUTPUT_CFG_REG,
				 driver_data->pdata->output_cfg_val, __func__);
		lm3532_write_reg(driver_data->client,
				LM3532_CTRL_A_BR_CFG_REG,
				driver_data->pdata->ctrl_a_current_ctrl |
				driver_data->pdata->ctrl_a_mapping_mode,
				 __func__);

		lm3532_write_reg(driver_data->client,
				 LM3532_CTRL_A_FS_CURR_REG,
				 driver_data->pdata->ctrl_a_fs_current,
				 __func__);

		ret = lm3532_write_reg(driver_data->client,
				LM3532_CTRL_A_ZT1_REG,
				LM3532_BL_MAX_BRIGHTNESS, __func__);
	}
	if (driver_data->pdata->ramp_time == 0) {
		lm3532_write_reg(driver_data->client,
				 LM3532_START_UP_RAMP_REG, 0x0, __func__);
		lm3532_write_reg(driver_data->client,
				 LM3532_RUN_TIME_RAMP_REG, 0x0, __func__);
	}
	if (driver_data->pdata->flags & LM3532_CONFIG_MAP_ALL_CTRL_A)
		lm3532_write_reg(driver_data->client,
			LM3532_OUTPUT_CFG_REG, 0x00, __func__);

	return ret;
}

static int lm3532_enable(struct lm3532_data *driver_data)
{
	lm3532_write_reg(driver_data->client,
		driver_data->enable_reg, LM3532_CTRLA_ENABLE, __func__);
	return 0;
}

static int lm3532_resume(struct i2c_client *client)
{
	struct lm3532_data *driver_data = i2c_get_clientdata(client);

	printk_suspend("%s: resuming\n", __func__);
	if (driver_data->pdata->power_on)
		driver_data->pdata->power_on();
	lm3532_power(client, 1);

	mutex_lock(&lm3532_mutex);
	lm3532_configure(driver_data);
	lm3532_enable(driver_data);
	mutex_unlock(&lm3532_mutex);
	atomic_set(&driver_data->suspended, 0);

	printk_suspend("%s: driver resumed\n", __func__);

	return 0;
}

static int lm3532_setup(struct lm3532_data *driver_data)
{
	int ret;
	uint8_t value;

	/* Read revision number, need to write first to get correct value */
	lm3532_write_reg(driver_data->client, LM3532_VERSION_REG, 0, __func__);
	ret = lm3532_read_reg(driver_data->client, LM3532_VERSION_REG, &value);
	if (ret < 0) {
		pr_err("%s: unable to read from chip: %d\n", __func__, ret);
		return ret;
	}
	driver_data->revision = value;
	pr_info("%s: revision 0x%X\n", __func__, driver_data->revision);
	if (value == 0xF6)
		driver_data->enable_reg = LM3532_R0_ENABLE_REG;
	else
		driver_data->enable_reg = LM3532_ENABLE_REG;

	ret = lm3532_configure(driver_data);
	driver_data->bvalue = 255;

	return ret;
}

static const struct backlight_ops lm3532_bl_ops = {
	.update_status	= lm3532_bl_brightness_set,
	.get_brightness	= lm3532_bl_get_brightness,
};

#ifdef CONFIG_OF
static int lm3532_probe_dt(struct i2c_client *client)
{
	struct lm3532_platform_data *pdata;
	struct device_node *np = client->dev.of_node;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (pdata == NULL) {
		dev_err(&client->dev, "Alloc GFP_KERNEL memory failed.");
		return -ENOMEM;
	}
	client->dev.platform_data = pdata;

	if (of_property_read_u32(np, "flags", &pdata->flags)) {
		dev_info(&client->dev,
		"failed to get flags property, set with default register value\n");
		pdata->flags = 0;
	}
	if (of_property_read_u32(np, "ramp_time", &pdata->ramp_time)) {
		dev_info(&client->dev, "failed to get ramp_time property, set as 0\n");
		pdata->ramp_time = 0;
	}
	if (of_property_read_u32(np, "ctrl_a_fs_current",
				 &pdata->ctrl_a_fs_current)) {
		dev_info(&client->dev,
			"failed to get ctrl_a_fs_current property, set as default\n");
		pdata->ctrl_a_fs_current =
			LM3532_CTRL_A_FS_CURR_REG_DEFAULT_20p2MA;
	}
	if (of_property_read_u32(np, "ctrl_a_mapping_mode",
				 &pdata->ctrl_a_mapping_mode)) {
		dev_info(&client->dev,
			"failed to get ctrl_a_mapping_mode property, set_as default\n");
		pdata->ctrl_a_mapping_mode =
				LM3532_CTRL_A_BR_CFG_REG_MAP_DEFAULT;
	}
	if (of_property_read_u32(np, "ctrl_a_pwm", &pdata->ctrl_a_pwm)) {
		dev_info(&client->dev,
			"failed to get ctrl_a_pwm property, set as default\n");
		pdata->ctrl_a_pwm = LM3532_CTRL_A_PWM_REG_DEFAULT;
	}
	if (of_property_read_u32(np, "feedback_en_val",
					&pdata->feedback_en_val)) {
		dev_info(&client->dev,
			"failed to get feedback_en_val property, set as default\n");
		pdata->feedback_en_val = LM3532_FEEDBACK_ENABLE_REG_DEFAULT;
	}
	if (of_property_read_u32(np, "output_cfg_val",
					&pdata->output_cfg_val)) {
		dev_info(&client->dev,
			"failed to get output_cfg_val property, set as default\n");
		pdata->output_cfg_val = LM3532_OUTPUT_CFG_REG_DEFAULT;
	}
	if (of_property_read_u32(np, "ctrl_a_current_ctrl",
					&pdata->ctrl_a_current_ctrl)) {
		dev_info(&client->dev,
			"failed to get ctrl_a_current_ctrl property, set as default\n");
		pdata->ctrl_a_current_ctrl = LM3532_I2C_CONTROL;
	}

	return 0;
}
#endif

/* This function is called by i2c_probe */
static int lm3532_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;
	struct lm3532_platform_data *pdata;
	struct lm3532_data *driver_data;
	struct backlight_properties props;

	pr_info("%s: enter, I2C address = 0x%x, flags = 0x%x\n",
		__func__, client->addr, client->flags);

#ifdef CONFIG_OF
	ret = lm3532_probe_dt(client);
	if (ret == -ENOMEM) {
		dev_err(&client->dev,
			"%s: Failed to alloc mem for platform_data\n",
			__func__);
		return ret;
	} else if (ret == -EINVAL) {
		kfree(client->dev.platform_data);
		dev_err(&client->dev,
			"%s: Probe device tree data failed\n", __func__);
		return ret;
	}
#endif
	pdata = client->dev.platform_data;
	if (pdata == NULL) {
		pr_err("%s: platform data required\n", __func__);
		return -EINVAL;
	}
	/* We should be able to read and write byte data */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: I2C_FUNC_I2C not supported\n", __func__);
		return -ENOTSUPP;
	}
	driver_data = kzalloc(sizeof(struct lm3532_data), GFP_KERNEL);
	if (driver_data == NULL) {
		pr_err("%s: kzalloc failed\n", __func__);
		return -ENOMEM;
	}
	memset(driver_data, 0, sizeof(*driver_data));

	driver_data->client = client;
	driver_data->pdata = pdata;
	if (driver_data->pdata->ctrl_a_fs_current > 0xFF)
		driver_data->pdata->ctrl_a_fs_current = 0xFF;
	if (driver_data->pdata->ctrl_b_fs_current > 0xFF)
		driver_data->pdata->ctrl_b_fs_current = 0xFF;

	i2c_set_clientdata(client, driver_data);

	/* Initialize chip */
	if (pdata->init)
		pdata->init();

	if (pdata->power_on)
		pdata->power_on();
	lm3532_power(client, 1);

	ret = lm3532_setup(driver_data);
	if (ret < 0)
		goto setup_failed;

	/* Register LED class */
	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = props.brightness = LM3532_BL_MAX_BRIGHTNESS;

	driver_data->bl = backlight_device_register(
				dev_driver_string(&client->dev),
				&client->dev,
				driver_data,
				&lm3532_bl_ops,
				&props);
	if (driver_data->bl == NULL) {
		dev_err(&client->dev, "failed to register backlight\n");
		ret = PTR_ERR(driver_data->bl);
		goto led_register_failed;
	}

	/* Initialize interrupts */
	if (driver_data->pdata->flags & LM3532_CONFIG_ALS) {
		unsigned long request_flags = IRQF_TRIGGER_LOW;

		INIT_WORK(&driver_data->work, lm3532_work_func);
		ret = request_irq(client->irq, lm3532_irq_handler,
				  request_flags, LM3532_NAME, driver_data);

		if (ret == 0)
			ret = irq_set_irq_wake(client->irq, 1);
		else {
			pr_err("request_irq %d for lm3532 failed: %d\n",
					       client->irq, ret);
			ret = -EINVAL;
			goto request_irq_failed;
		}
		driver_data->idev = input_allocate_device();
		if (driver_data->idev == NULL) {
			pr_err("%s: unable to allocate input device file for als\n",
						       __func__);
			goto input_allocate_device_failed;
		}
		driver_data->idev->name = "als";
		input_set_capability(driver_data->idev, EV_MSC, MSC_RAW);
		input_set_capability(driver_data->idev, EV_LED, LED_MISC);
		ret = input_register_device(driver_data->idev);
		if (ret) {
			pr_err("%s: unable to register input device file for als: %d\n",
						       __func__, ret);
			goto input_register_device_failed;
		}
	}

	lm3532_enable(driver_data);

	/*
	 * FIXME: Maybe should initialize brightness to whatever
	 * bootloader wrote into the register
	 */
	driver_data->bl->props.brightness = LM3532_BL_MAX_BRIGHTNESS;
	driver_data->initialized = 1;
	pm_runtime_enable(&driver_data->bl->dev);
	pm_runtime_forbid(&driver_data->bl->dev);

	return 0;

input_register_device_failed:
	input_free_device(driver_data->idev);
input_allocate_device_failed:
	free_irq(client->irq, driver_data);
request_irq_failed:
	backlight_device_unregister(driver_data->bl);
led_register_failed:
setup_failed:
	if (pdata->exit)
		pdata->exit();
#ifdef CONFIG_OF
	kfree(driver_data->pdata);
#endif
	kfree(driver_data);
	return ret;
}

static int lm3532_remove(struct i2c_client *client)
{
	struct lm3532_data *driver_data = i2c_get_clientdata(client);

	pm_runtime_disable(&client->dev);

	if (driver_data->pdata->flags & LM3532_CONFIG_ALS) {
		input_unregister_device(driver_data->idev);
		input_free_device(driver_data->idev);
		free_irq(client->irq, driver_data);
	}
#ifdef CONFIG_OF
	kfree(driver_data->pdata);
#endif
	kfree(driver_data);
	return 0;
}

static const struct i2c_device_id lm3532_id[] = {
	{LM3532_NAME, 0},
	{}
};

#ifdef CONFIG_PM_RUNTIME
static int lm3532_runtime_suspend(struct device *dev)
{
	struct lm3532_data *driver_data = dev_get_drvdata(dev);

	return lm3532_suspend(driver_data->client, PMSG_SUSPEND);
}

static int lm3532_runtime_resume(struct device *dev)
{
	struct lm3532_data *driver_data = dev_get_drvdata(dev);

	return lm3532_resume(driver_data->client);
}
#endif /* CONFIG_PM_RUNTIME */

static UNIVERSAL_DEV_PM_OPS(lm3532_pm_ops, lm3532_runtime_suspend,
			    lm3532_runtime_resume, NULL);

static struct of_device_id lm3532_dt_ids[] = {
	{.compatible = "ti,lm3532",},
	{}
};

/* This is the I2C driver that will be inserted */
static struct i2c_driver lm3532_driver = {
	.driver = {
		   .name = LM3532_NAME,
		   .pm = &lm3532_pm_ops,
		   .of_match_table = of_match_ptr(lm3532_dt_ids),
		   },
	.id_table = lm3532_id,
	.probe = lm3532_probe,
	.remove = lm3532_remove,
};

static int __init lm3532_init(void)
{
	int ret;

	pr_info("%s: enter\n", __func__);
	ret = i2c_add_driver(&lm3532_driver);
	if (ret)
		pr_err("%s: i2c_add_driver failed, error %d\n", __func__, ret);

	return ret;
}

static void __exit lm3532_exit(void)
{
	i2c_del_driver(&lm3532_driver);
}

late_initcall(lm3532_init);
module_exit(lm3532_exit);

MODULE_DESCRIPTION("LM3532 DISPLAY BACKLIGHT DRIVER");
MODULE_AUTHOR("Alina Yakovleva, Motorola, qvdh43@motorola.com");
MODULE_LICENSE("GPL v2");
