/*
 * Copyright (C) 2023 Samsung Electronics
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/of.h>
#include <linux/workqueue.h>
#include <linux/list.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/leds-aw36518.h>

/* define mutex and work queue */
static DEFINE_MUTEX(aw36518_mutex);

/* define i2c */
static struct i2c_client *aw36518_i2c_client;
struct aw36518_chip_data *g_chip_data;

/******************************************************************************
 * aw36518 operations
 *****************************************************************************/
static const unsigned char aw36518_torch_level[AW36518_LEVEL_NUM] = {
	0x06, 0x0F, 0x17, 0x1F, 0x27, 0x2F, 0x37, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const unsigned char aw36518_flash_level[AW36518_LEVEL_NUM] = {
	0x01, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x10, 0x14,
	0x19, 0x1D, 0x21, 0x25, 0x2A, 0x2E, 0x32, 0x37, 0x3B,
	0x3F, 0x43, 0x48, 0x4C, 0x50, 0x54, 0x59, 0x5D
};

static const unsigned int aw36518_flash_time_out_map[] = {
	40, 80, 120, 160, 200, 240,	280, 320,
	360, 400, 600, 800, 1000, 1200, 1400, 1600,
};

static volatile unsigned char aw36518_reg_enable;

/* i2c wrapper function */
static int aw36518_i2c_write(struct i2c_client *client, unsigned char reg,
				unsigned char val)
{
	int ret;
	unsigned char cnt = 0;

	pr_debug("%s, 0x%x, 0x%x\n", __func__, reg, val);

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_write_byte_data(client, reg, val);
		if (ret < 0) {
			pr_info("%s: i2c_write addr=0x%02X, data=0x%02X, cnt=%d, error=%d\n",
				__func__, reg, val, cnt, ret);
		} else {
			break;
		}
		cnt++;
		msleep(AW_I2C_RETRY_DELAY);
	}

	return ret;
}

static int aw36518_i2c_read(struct i2c_client *client, unsigned char reg,
				unsigned char *val)
{
	int ret;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(client, reg);
		if (ret < 0) {
			pr_info("%s: i2c_read addr=0x%02X, cnt=%d, error=%d\n",
				__func__, reg, cnt, ret);
		} else {
			*val = ret;
			break;
		}
		cnt++;
		msleep(AW_I2C_RETRY_DELAY);
	}

	return ret;
}

static void aw36518_soft_reset(void)
{
	unsigned char reg_val;

	aw36518_i2c_read(aw36518_i2c_client, AW36518_REG_BOOST_CONFIG,
			 &reg_val);
	reg_val &= AW36518_BIT_SOFT_RST_MASK;
	reg_val |= AW36518_BIT_SOFT_RST_ENABLE;
	aw36518_i2c_write(aw36518_i2c_client, AW36518_REG_BOOST_CONFIG,
			 reg_val);
	usleep_range(5, 10);
}

static void aw36518_pre_enable_cfg_by_vendor(void)
{
	struct aw36518_chip_data *pdata = aw36518_i2c_client->dev.driver_data;

	if (pdata->chip_version == AW36518_CHIP_B) {
		aw36518_i2c_write(aw36518_i2c_client, AW36518_REG_CTRL2, 0x02);
		aw36518_i2c_write(aw36518_i2c_client, AW36518_REG_CTRL1, 0x0C);
	}
}

/* set flash duration , unit ms */
static void aw36518_set_flash_time_out_duration(int duration)
{
	unsigned char val;
	unsigned char read_val;

	if (duration < 0 || duration > 1600) {
		pr_err("%s Parameter not available, cur = %d\n", __func__,
			 duration);
		duration = 0;
	}

	/* duration to reg val */
	if (duration < 40)
		val = 0;
	else if (duration <= 400)
		val = duration / 40 - 1;
	else
		val = (duration - 400) / 200 + 0x09;

	aw36518_i2c_read(aw36518_i2c_client, AW36518_REG_TIMING_CONF,
			 &read_val);

	/* only set [bit0 - bit3] */
	read_val &= 0xf0;
	val |= read_val;

	pr_info("%s: duration = %dms, reg_val = %x\n", __func__, duration, val);
	/* set flash duration */
	aw36518_i2c_write(aw36518_i2c_client, AW36518_REG_TIMING_CONF, val);
}

/* flashlight enable function */
static void aw36518_enable_flash(bool is_enable, int duration)
{
	unsigned char reg, val;
	struct aw36518_chip_data *pdata = aw36518_i2c_client->dev.driver_data;

	reg = AW36518_REG_ENABLE;
	aw36518_pre_enable_cfg_by_vendor();
	if (is_enable) {
		/* flash mode */
		aw36518_set_flash_time_out_duration(duration);
		if (pdata->gpio_request < 0) {
			aw36518_reg_enable |= AW36518_ENABLE_LED1_FLASH;
			val = aw36518_reg_enable;
			aw36518_i2c_write(aw36518_i2c_client, reg, val);
			pdata->used_gpio_ctrl = false;
		} else {
			val = 0x23;
			aw36518_i2c_write(aw36518_i2c_client, reg, val);
			gpio_direction_output(pdata->fen_pin, 0);
			gpio_set_value(pdata->fen_pin, 1);
			pdata->used_gpio_ctrl = true;
		}
	} else {
		/* standby mode */
		val = aw36518_reg_enable;
		aw36518_i2c_write(aw36518_i2c_client, reg, val);
		aw36518_reg_enable &= AW36518_LED_STANDBY_MODE_MASK;
	}

	pr_info("%s reg = 0x%x;val = 0x%x;\n", __func__, reg, val);
	pr_info("%s pdata->used_gpio_ctrl = %d\n", __func__,
		pdata->used_gpio_ctrl);
}

/* torch enable function */
static void aw36518_enable_torch(bool is_enable)
{
	unsigned char reg, val;
	struct aw36518_chip_data *pdata = aw36518_i2c_client->dev.driver_data;

	reg = AW36518_REG_ENABLE;
	aw36518_pre_enable_cfg_by_vendor();
	if (is_enable) {
		/* torch mode */
		if (pdata->gpio_request < 0) {
			aw36518_reg_enable |= AW36518_ENABLE_LED1_TORCH;
			val = aw36518_reg_enable;
			aw36518_i2c_write(aw36518_i2c_client, reg, val);
			pdata->used_gpio_ctrl = false;
		} else {
			aw36518_reg_enable |= AW36518_ENABLE_LED1_TORCH;
			val = aw36518_reg_enable;
			aw36518_i2c_write(aw36518_i2c_client, reg, val);
			gpio_direction_output(pdata->fen_pin, 0);
			gpio_set_value(pdata->fen_pin, 1);
			pdata->used_gpio_ctrl = true;
		}
	} else {
		/* standby mode */
		aw36518_reg_enable &= AW36518_LED_STANDBY_MODE_MASK;
		val = aw36518_reg_enable;
		aw36518_i2c_write(aw36518_i2c_client, reg, val);
	}

	pr_info("%s reg = 0x%x;val = 0x%x;\n", __func__, reg, val);
	pr_info("%s pdata->used_gpio_ctrl = %d\n", __func__,
		pdata->used_gpio_ctrl);
}

/* disabel chip output */
static void aw36518_disable_led(void)
{
	struct aw36518_chip_data *pdata = aw36518_i2c_client->dev.driver_data;

	pr_info("%s start.\n", __func__);
	aw36518_pre_enable_cfg_by_vendor();
	if (pdata->used_gpio_ctrl == true) {
		gpio_direction_output(pdata->fen_pin, 0);
		gpio_set_value(pdata->fen_pin, 0);
		pdata->used_gpio_ctrl = false;
		aw36518_i2c_write(aw36518_i2c_client, AW36518_REG_ENABLE, 0x00);
	} else {
		aw36518_reg_enable &= 0xf0;
		aw36518_i2c_write(aw36518_i2c_client, AW36518_REG_ENABLE,
				 aw36518_reg_enable);
	}
}

/* set torch bright cur , unit mA range 0 - 386 */
static void aw36518_set_torch_cur(int cur)
{
	unsigned char val;
	struct aw36518_chip_data *pdata = aw36518_i2c_client->dev.driver_data;

	pr_info("%s, set cur %d\n", __func__, cur);
	if (cur < AW36518_MIN_TORCH_CUR || cur > pdata->max_torch_cur) {
		pr_err("%s Parameter not available, cur = %d\n", __func__, cur);
		cur = 0;
	}

	/* cur to reg val */
	if (cur < 1) {
		val = 0;
	} else {
		if (pdata->chip_version == AW36518_CHIP_B) {
			pr_info("aw36518 chip vendor is B\n");
			val = ((cur * 200) - 135) / 270;
		} else {
			val = ((cur * 100) - 75) / 151;
		}
	}

	pr_info("%s: cur = %dmA, reg_val = 0x%x\n", __func__, cur, val);

	/* set torch brightness level */
	aw36518_i2c_write(aw36518_i2c_client, AW36518_REG_TORCH_LEVEL_LED1,
			 val);
}

/* set flash bright cur , unit mA unit mA range 0 - 1500 */
static int aw36518_set_flash_cur(int cur)
{
	int ret;
	unsigned char val;

	if (cur < AW36518_MIN_FLASH_CUR || cur > AW36518_MAX_FLASH_CUR) {
		pr_err("%s Parameter not available, cur = %d\n", __func__, cur);
		cur = 0;
	}

	/* cur to reg val */
	if (cur < 1)
		val = 0;
	else
		val = ((cur * 100) - 294) / 587;

	pr_info("%s: cur = %dmA, reg_val = %x\n", __func__, cur, val);
	/* set flash brightness level */
	ret = aw36518_i2c_write(aw36518_i2c_client,
				AW36518_REG_FLASH_LEVEL_LED1, val);

	return ret;
}

/* Flicker mode */
void aw36518_enable_flicker(int curr, bool is_enable)
{
	unsigned char reg = AW36518_REG_ENABLE, val = 0;
	if (is_enable) {
		/* torch mode */
		val = 0x13;
		aw36518_set_torch_cur(curr);
	} else {
		/* standby mode */
		aw36518_reg_enable &= AW36518_LED_STANDBY_MODE_MASK;
		val = aw36518_reg_enable;
	}
	aw36518_pre_enable_cfg_by_vendor();

	aw36518_i2c_write(aw36518_i2c_client, reg, val);

	pr_info("%s reg = 0x%x;val = 0x%x;\n", __func__, reg, val);
}
EXPORT_SYMBOL(aw36518_enable_flicker);

/* set flash bright cur , unit mA unit mA range 0 - 1500 */
static int aw36518_get_flash_time_out(void)
{
	unsigned char reg_val;

	aw36518_i2c_read(aw36518_i2c_client, AW36518_REG_TIMING_CONF, &reg_val);
	reg_val &= 0x0f;
	pr_info("%s flash time out = %d", __func__,
		aw36518_flash_time_out_map[reg_val]);

	return aw36518_flash_time_out_map[reg_val];
}

static void aw36518_clear_flags(void)
{
	unsigned char reg_val_flag1 = 0;
	unsigned char reg_val_flag2 = 0;

	pr_info("%s enter\n", __func__);
	aw36518_i2c_read(aw36518_i2c_client, AW36518_REG_FLAG1, &reg_val_flag1);
	aw36518_i2c_read(aw36518_i2c_client, AW36518_REG_FLAG2, &reg_val_flag2);
	pr_info("%s reg FLAG1=0x%02X", __func__, reg_val_flag1);
	pr_info("%s reg FLAG2=0x%02X", __func__, reg_val_flag2);

	if (reg_val_flag1 & AW36518_BIT_UVLO_FAULT)
		pr_err("%s: UVLO FAULT", __func__);
	if (reg_val_flag1 & AW36518_BIT_TSD_FAULT)
		pr_err("%s: TSD FAULT", __func__);
	if (reg_val_flag1 & AW36518_BIT_LED_SHORT_FAULT_1)
		pr_err("%s: LED SHORT1", __func__);
	if (reg_val_flag1 & AW36518_BIT_LED_SHORT_FAULT_2)
		pr_err("%s: LED SHORT2", __func__);
	if (reg_val_flag1 & AW36518_BIT_VOUT_SHORT_FAULT)
		pr_err("%s VOUT FAULT", __func__);

	if (reg_val_flag2 & AW36518_BIT_OVP_FAULT)
		pr_err("%s: OVP FAULT", __func__);
}

static void aw36518_delay_work_routine(struct work_struct *work)
{
	pr_info("%s enter\n", __func__);

	/* read state and clear err flag */
	aw36518_clear_flags();
}

extern int32_t aw36518_fled_mode_ctrl(int state, uint32_t brightness);

/* brightness is led current . unit is mA */
int32_t aw36518_fled_mode_ctrl(int state, uint32_t brightness)
{
	int ret = 0;
	u8 iq_cur = 0;

	struct aw36518_chip_data *pdata = g_chip_data;

	if (pdata == NULL) {
		pr_err("aw36518_fled: %s: g_aw36518_fled is not initialized.\n",
			 __func__);
		return -EFAULT;
	}

	/* brightness to irq */
	iq_cur = brightness;

	pr_info("aw36518-fled: %s: iq_cur=0x%x brightness=%u\n", __func__,
		iq_cur, brightness);

	mutex_lock(&pdata->lock);
	aw36518_clear_flags();

	switch (state) {
	case AW36518_FLED_MODE_OFF:
		/* FlashLight Mode OFF */
		aw36518_disable_led();
		pr_info("aw36518-fled: %s: AW36518_FLED_MODE_OFF(%d) done\n",
			__func__, state);
		break;

	case AW36518_FLED_MODE_MAIN_FLASH:
		ret = aw36518_set_flash_cur(pdata->main_flash_cur);
		aw36518_enable_flash(true, AW36518_DEFAULT_FLASH_DURATION);
		schedule_delayed_work(
			&pdata->delay_work,
			msecs_to_jiffies(pdata->flash_time_out_ms));
		pr_info("aw36518-fled: %s: AW36518_FLED_MODE_MAIN_FLASH(%d) done\n",
			__func__, state);
		break;

	case AW36518_FLED_MODE_TORCH_FLASH: /* TORCH FLASH */
		aw36518_set_torch_cur(iq_cur);
		aw36518_enable_torch(true);
		pr_info("aw36518-fled: %s: AW36518_FLED_MODE_TORCH_FLASH(%d) done\n",
			__func__, state);
		break;

	case AW36518_FLED_MODE_PRE_FLASH: /* TORCH FLASH */
		aw36518_set_torch_cur(pdata->pre_flash_cur);
		aw36518_enable_torch(true);
		pr_info("aw36518-fled: %s: AW36518_FLED_MODE_PRE_FLASH(%d) done\n",
			__func__, state);
		break;

	case AW36518_FLED_MODE_PREPARE_FLASH:
		ret = aw36518_set_flash_cur(iq_cur);
		if (ret < 0)
			pr_err("aw36518-fled: %s: AW36518_FLED_MODE_PREPARE_FLASH(%d) failed\n",
				 __func__, state);
		else
			pr_info("aw36518-fled: %s: AW36518_FLED_MODE_PREPARE_FLASH(%d) done\n",
				__func__, state);
		break;

	case AW36518_FLED_MODE_CLOSE_FLASH:
		aw36518_enable_flash(false, 0);
		aw36518_disable_led();
		pr_info("aw36518-fled: %s: AW36518_FLED_MODE_CLOSE_FLASH(%d) done\n",
			__func__, state);
		break;
	default:
		aw36518_disable_led();
		/* FlashLight Mode OFF */
		pr_info("aw36518-fled: %s: FLED_MODE_OFF(%d) done\n", __func__,
			state);
		break;
	}
	mutex_unlock(&pdata->lock);
	return ret;
}
EXPORT_SYMBOL_GPL(aw36518_fled_mode_ctrl);

/* flashlight init */
int aw36518_init(void)
{
	int ret;
	unsigned char reg, val;
	struct aw36518_chip_data *pdata = aw36518_i2c_client->dev.driver_data;

	aw36518_soft_reset();
	usleep_range(2000, 2500);

	aw36518_i2c_read(aw36518_i2c_client, AW36518_REG_CHIP_VENDOR_ID, &val);
	pr_info("aw36518 reg[0x25] = 0x%2x\n", val);
	if ((val & 0x04) == AW36518_CHIP_VENDOR_ID) {
		pr_info("aw36518 chip vendor is B\n");
		pdata->chip_version = AW36518_CHIP_B;
		pdata->max_torch_cur = AW36518_VENDOR_B_MAX_TORCH_CUR;
	} else {
		pdata->chip_version = AW36518_CHIP_A;
		pdata->max_torch_cur = AW36518_VENDOR_A_MAX_TORCH_CUR;
		pr_info("aw36518 chip vendor is A\n");
	}
	/* clear enable register */
	reg = AW36518_REG_ENABLE;
	val = AW36518_DISABLE;
	aw36518_pre_enable_cfg_by_vendor();
	ret = aw36518_i2c_write(aw36518_i2c_client, reg, val);

	aw36518_reg_enable = val;

	/* set default torch current ramp time and flash timeout */
	reg = AW36518_REG_TIMING_CONF;
	val = AW36518_TORCH_RAMP_TIME | AW36518_FLASH_TIMEOUT;
	ret = aw36518_i2c_write(aw36518_i2c_client, reg, val);

	/* set default flash duration 600 ms*/
	aw36518_set_flash_time_out_duration(AW36518_DEFAULT_FLASH_DURATION);

	/* set default flash curren */
	ret = aw36518_set_flash_cur(AW36518_DEFAULT_FLASH_CUR);

	return ret;
}

/* flashlight uninit */
int aw36518_uninit(void)
{
	aw36518_disable_led();
	return 0;
}

int aw36518_get_dts_info(struct i2c_client *client,
			 struct aw36518_chip_data *pdata)
{
	int ret = 0;
	int i = 0;
	unsigned int temp = 100;
	struct device_node *node = NULL;

	if (!pdata) {
		pr_err("%s Driver data does not exist\n", __func__);
		return -EINVAL;
	}
	node = client->dev.of_node;
	if (node == NULL) {
		pr_err("%s: Mode is NULL", __func__);
		return -EINVAL;
	}

	/* get torch level 1 cur */
	ret = of_property_read_u32(node, "torch_level_1_cur", &temp);
	if (ret < 0) {
		dev_err(&client->dev, "%s:dts parse torch_level_1_cur failed\n",
			__func__);
		return -EINVAL;
	}
	pdata->torch_level[0] = temp;

	/* get torch level 2 cur */
	ret = of_property_read_u32(node, "torch_level_2_cur", &temp);
	if (ret < 0) {
		dev_err(&client->dev, "%s:dts parse torch_level_2_cur failed\n",
			__func__);
		return -EINVAL;
	}
	pdata->torch_level[1] = temp;

	/* get torch level 3 cur */
	ret = of_property_read_u32(node, "torch_level_3_cur", &temp);
	if (ret < 0) {
		dev_err(&client->dev, "%s:dts parse torch_level_3_cur failed\n",
			__func__);
		return -EINVAL;
	}
	pdata->torch_level[2] = temp;

	/* get torch level 4 cur */
	ret = of_property_read_u32(node, "torch_level_4_cur", &temp);
	if (ret < 0) {
		dev_err(&client->dev, "%s:dts parse torch_level_4_cur failed\n",
			__func__);
		return -EINVAL;
	}
	pdata->torch_level[3] = temp;

	/* get torch level 5 cur */
	ret = of_property_read_u32(node, "torch_level_5_cur", &temp);
	if (ret < 0) {
		dev_err(&client->dev, "%s:dts parse torch_level_5_cur failed\n",
			__func__);
		return -EINVAL;
	}
	pdata->torch_level[4] = temp;

	/* debug info */
	for (i = 0; i < 5; i++) {
		pr_info("%s: torch_level_%d_cur = %d", __func__, i,
			pdata->torch_level[i]);
	}

	pdata->gpio_request = -1;
	pdata->fen_pin = -1;
	ret = pdata->fen_pin = of_get_named_gpio(node, "flash-gpio", 0);
	pr_info("pdata->fen_pin = %d", pdata->fen_pin);
	if (ret < 0) {
		pr_err("%s : can't get flash-en-gpio\n", __func__);
	} else {
		pdata->gpio_request =
			gpio_request(pdata->fen_pin, "aw36518_fled");
		if (pdata->gpio_request < 0) {
			pr_info("%s: failed request fen_pin(%d) done\n",
				__func__, pdata->gpio_request);
		}
	}
	pr_info("%s stop.\n", __func__);

	ret = of_property_read_u32(node, "pre_flash_cur", &temp);
	if (ret < 0) {
		dev_err(&client->dev, "%s:dts parse pre_flash_cur failed\n",
			__func__);
		return -EINVAL;
	}
	pdata->pre_flash_cur = temp;

	ret = of_property_read_u32(node, "main_flash_cur", &temp);
	if (ret < 0) {
		dev_err(&client->dev, "%s:dts parse main_flash_cur failed\n",
			__func__);
		return -EINVAL;
	}
	pdata->main_flash_cur = temp;

	ret = of_property_read_u32(node, "video_torch_cur", &temp);
	if (ret < 0) {
		dev_err(&client->dev, "%s:dts parse video_torch_cur failed\n",
			__func__);
		return -EINVAL;
	}
	pdata->video_torch_cur = temp;

	return 0;
}

#if 0
/***************************************************************************/
/*AW36518 Debug file */
/***************************************************************************/
static ssize_t aw36518_get_reg(struct device *cd, struct device_attribute *attr,
				 char *buf)
{
	unsigned char reg_val;
	unsigned char i;
	ssize_t len = 0;

	for (i = 0; i < 0x0E; i++) {
		aw36518_i2c_read(aw36518_i2c_client, i, &reg_val);
		len += snprintf(buf + len, PAGE_SIZE - len, "reg0x%x = 0x%x\n",
				i, reg_val);
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\r\n");
	return len;
}

static ssize_t aw36518_set_reg(struct device *cd, struct device_attribute *attr,
				 const char *buf, size_t len)
{
	unsigned int databuf[2];

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2)
		aw36518_i2c_write(aw36518_i2c_client, databuf[0], databuf[1]);
	return len;
}

static DEVICE_ATTR(reg, 0660, aw36518_get_reg, aw36518_set_reg);
#endif

ssize_t aw36518_show(char *buf)
{
	struct aw36518_chip_data *pdata = g_chip_data;

	if (pdata == NULL) {
		pr_err("aw36518-fled: %s: g_aw36518_fled is NULL\n", __func__);
		return -ENODEV;
	}

	return sprintf(buf, "%s\n", "success");
}
EXPORT_SYMBOL_GPL(aw36518_show);

ssize_t aw36518_store(const char *buf)
{
	u32 store_value;

	struct aw36518_chip_data *pdata = g_chip_data;

	if (pdata == NULL) {
		pr_err("aw36518-fled: %s: pdata is NULL\n", __func__);
		return -ENODEV;
	}

	if ((buf == NULL) || kstrtouint(buf, 10, &store_value))
		return -ENXIO;

	mutex_lock(&pdata->lock);

	pr_info("%s : store_value = %d\n", __func__, store_value);

	switch (store_value) {
	case 0: /* Torch or Flash OFF */
		aw36518_disable_led();
		break;
	case 1: /* 1 set default brightness of torch and turn on torch */
		aw36518_i2c_write(aw36518_i2c_client,
				 AW36518_REG_TORCH_LEVEL_LED1,
				 AW36518_TORCH_DEFAULT_CUR_REG_VAL);
		aw36518_enable_torch(true);
		break;
	case 100: /* 100 : set higtest brightness of torch and turn on torch */
		aw36518_i2c_write(aw36518_i2c_client,
				 AW36518_REG_TORCH_LEVEL_LED1,
				 AW36518_TORCH_HIGTEST_CUR_REG_VAL);
		aw36518_enable_torch(true);
		break;
	case 200: /* 200 : factory flash test */
		aw36518_i2c_write(aw36518_i2c_client,
				 AW36518_REG_FLASH_LEVEL_LED1,
				 AW36518_FLASH_FACTORY_CUR_REG_VAL);
		aw36518_enable_flash(true, AW36518_MAX_FLASH_DURATION);
		schedule_delayed_work(
			&pdata->delay_work,
			msecs_to_jiffies(pdata->flash_time_out_ms));
		break;
	case 1001: /* set level 1 brightness and turn on torch */
		aw36518_set_torch_cur(pdata->torch_level[0]);
		aw36518_enable_torch(true);
		break;
	case 1002: /* set level 2 brightness and turn on torch */
		aw36518_set_torch_cur(pdata->torch_level[1]);
		aw36518_enable_torch(true);
		break;

	case 1003: /* set level 3 brightness and turn on torch */
	case 1004:
		aw36518_set_torch_cur(pdata->torch_level[2]);
		aw36518_enable_torch(true);
		break;

	case 1005: /* set level 4 brightness and turn on torch */
	case 1006:
		aw36518_set_torch_cur(pdata->torch_level[3]);
		aw36518_enable_torch(true);
		break;

	case 1007: /* set level 5 brightness and turn on torch */
	case 1008:
	case 1009:
		aw36518_set_torch_cur(pdata->torch_level[4]);
		aw36518_enable_torch(true);
		break;
	default: /* off */
		aw36518_disable_led();
		pr_err("%s: unsupport params. value=%d\n", __func__,
			 store_value);
		break;
	}
	mutex_unlock(&pdata->lock);
	return 0;
}
EXPORT_SYMBOL_GPL(aw36518_store);

#if 0
static DEVICE_ATTR(rear_flash, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH,
			aw36518_rear_flash_show, aw36518_rear_flash_store);

static int aw36518_create_sysfs(struct i2c_client *client)
{
	int err;
	struct device *dev = &(client->dev);
	struct device *flash_dev;

	/* creat reg attr */
	err = device_create_file(dev, &dev_attr_reg);
	if (err < 0)
		return err;

	if (IS_ERR_OR_NULL(camera_class)) {
		pr_err("%s: can't find camera_class sysfs object, didn't use rear_flash attribute\n",
			 __func__);
		return err;
	}

	flash_dev = device_create(camera_class, NULL, 0, NULL, "flash");

	if (IS_ERR(flash_dev)) {
		pr_err("%s failed create device for rear_flash\n", __func__);
		return err;
	}

	err = device_create_file(flash_dev, &dev_attr_rear_flash);
	if (err < 0) {
		pr_err("%s failed create device file for rear_flash\n",
			 __func__);
		return err;
	}

	return err;
}
#endif

static int aw36518_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct aw36518_chip_data *chip;
	int err;

	pr_err("%s Probe start.\n", __func__);

	/* check i2c */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("Failed to check i2c functionality.\n");
		err = -ENODEV;
		goto err_out;
	}

	/* init chip private data */
	chip = kzalloc(sizeof(struct aw36518_chip_data), GFP_KERNEL);
	if (!chip) {
		err = -ENOMEM;
		goto err_out;
	}
	chip->client = client;

	i2c_set_clientdata(client, chip);
	aw36518_i2c_client = client;

	/* get dts info */
	err = aw36518_get_dts_info(client, chip);
	if (err < 0) {
		pr_err("Failed to aw36518_get_dts_info.\n");
		goto err_free;
	}
	g_chip_data = chip;

	/* init mutex and spinlock */
	mutex_init(&chip->lock);

	/*aw36518_create_sysfs(client);*/

	err = aw36518_init();
	if (err < 0)
		goto err_free;

	chip->flash_time_out_ms = aw36518_get_flash_time_out();

	INIT_DELAYED_WORK(&chip->delay_work, aw36518_delay_work_routine);

	pr_info("%s Probe done.\n", __func__);
	pr_info("%s init success.\n", __func__);

	return 0;

err_free:
	i2c_set_clientdata(client, NULL);
	kfree(chip);
err_out:
	return err;
}

static int aw36518_i2c_remove(struct i2c_client *client)
{
	struct aw36518_chip_data *chip = i2c_get_clientdata(client);

	pr_info("Remove start.\n");

	kfree(chip);
	if (g_chip_data != NULL)
		kfree(g_chip_data);

	pr_info("Remove done.\n");

	return 0;
}

static void aw36518_i2c_shutdown(struct i2c_client *client)
{
	pr_info("aw36518 shutdown start.\n");

	aw36518_pre_enable_cfg_by_vendor();
	aw36518_i2c_write(aw36518_i2c_client, AW36518_REG_ENABLE,
			 AW36518_CHIP_STANDBY);

	pr_info("aw36518 shutdown done.\n");
}

static const struct i2c_device_id aw36518_i2c_id[] = { { AW36518_NAME, 0 },
							 {} };

#ifdef CONFIG_OF
static const struct of_device_id aw36518_i2c_of_match[] = {
	{ .compatible = "aw36518" },
	{},
};
#endif
MODULE_DEVICE_TABLE(of, aw36518_i2c_of_match);

static struct i2c_driver aw36518_i2c_driver = {
	.driver = {
			.name = AW36518_NAME,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = aw36518_i2c_of_match,
#endif
	},
	.probe = aw36518_i2c_probe,
	.remove = aw36518_i2c_remove,
	.shutdown = aw36518_i2c_shutdown,
	.id_table = aw36518_i2c_id,
};

module_i2c_driver(aw36518_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("awinic");
MODULE_DESCRIPTION("AW Flashlight AW36518 Driver");
