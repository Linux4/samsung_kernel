/*
 * leds-s2mu107.c - LED class driver for S2MU107 FLASH LEDs.
 *
 * Copyright (C) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/mfd/samsung/s2mu107.h>
#include <linux/leds-s2mu107.h>
#include <linux/platform_device.h>
#include <linux/sec_batt.h>

#define CONTROL_I2C	0
#define CONTROL_GPIO	1

static struct s2mu107_fled_data *g_fled_data = NULL;

extern struct class *camera_class;
static struct device *flash_dev;
extern struct device *cam_dev_flash;

static char *s2mu107_fled_mode_string[] = {
	"OFF",
	"TORCH",
	"FLASH",
};

static char *s2mu107_fled_operating_mode_string[] = {
	"AUTO",
	"BOOST",
	"TA",
	"SYS",
};

/*
 * Channel values range fron 1 to 3 so 0 value is never obtained
 * check function s2mu107_fled_set_torch_curr
 */
static int s2mu107_fled_torch_curr_max[] = {
	-1, 320
};

/*
 * Channel values range fron 1 to 3 so 0 value is never obtained
 * check function s2mu107_fled_set_flash_curr
 */
static int s2mu107_fled_flash_curr_max[] = {
	-1, 1500
};

static void s2mu107_fled_test_read(struct s2mu107_fled_data *fled)
{
	static u8 reg_list[] = {
		0x0A, 0x0B, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
		0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
	};
	u8 data = 0;
	char str[1016] = {0,};
	int i = 0, reg_list_size = 0;

	reg_list_size = ARRAY_SIZE(reg_list);
	for (i = 0; i < reg_list_size; i++) {
		s2mu107_read_reg(fled->i2c, reg_list[i], &data);
		sprintf(str + strlen(str),
			"0x%02x:0x%02x, ", reg_list[i], data);
	}

	/* print buffer */
	pr_info("[FLED]%s: %s\n", __func__, str);
}

static int s2mu107_fled_get_flash_curr(struct s2mu107_fled_data *fled, int chan)
{
	int curr = -1;
	u8 data, dest;

	if ((chan < 1) || (chan > S2MU107_CH_MAX)) {
		pr_info("%s: Wrong channel!!\n", __func__);
		return -1;
	}

	switch (chan) {
	case 1:
		dest = S2MU107_FLED_CH1_CTRL0;
		break;
	default:
		pr_info("%s: CH%02d does not exist!!\n", __func__, chan);
		return curr;
	}

	s2mu107_read_reg(fled->i2c, dest, &data);

	data = data & S2MU107_CHX_FLASH_IOUT;
	curr = (data * 50) + 50;

	pr_info("%s: CH%02d flash curr. = %dmA\n", __func__,
		chan, curr);

	return curr;
}

static int s2mu107_fled_set_flash_curr(struct s2mu107_fled_data *fled,
				       int chan, int curr)
{
	int ret = -1, curr_set;
	u8 data, dest;

	if ((chan < 1) || (chan > S2MU107_CH_MAX)) {
		pr_info("%s: Wrong channel!!\n", __func__);
		return -1;
	}

	switch (chan) {
	case 1:
		dest = S2MU107_FLED_CH1_CTRL0;
		break;
	default:
		pr_info("%s: CH%02d does not exist!!\n", __func__, chan);
		return ret;
	}

	if (curr < 50)
		curr = 50;
	else if (curr > s2mu107_fled_flash_curr_max[chan])
		curr = s2mu107_fled_flash_curr_max[chan];

	data = (curr - 50) / 50;

	s2mu107_update_reg(fled->i2c, dest, data, S2MU107_CHX_FLASH_IOUT);

	curr_set = s2mu107_fled_get_flash_curr(fled, chan);

	pr_info("%s: curr: %d, curr_set: %d\n", __func__, curr, curr_set);

	return ret;
}

static int s2mu107_fled_get_torch_curr(struct s2mu107_fled_data *fled, int chan)
{
	int curr = -1;
	u8 data, dest;

	if ((chan < 1) || (chan > S2MU107_CH_MAX)) {
		pr_info("%s: Wrong channel!!\n", __func__);
		return -1;
	}

	switch (chan) {
	case 1:
		dest = S2MU107_FLED_CH1_CTRL1;
		break;
	default:
		pr_info("%s: CH%02d does not exist!!\n", __func__, chan);
		return curr;
	}

	s2mu107_read_reg(fled->i2c, dest, &data);

	data = data & S2MU107_CHX_TORCH_IOUT;
	curr = data * 10 + 10;

	pr_info("%s: CH%02d torch curr. = %dmA\n", __func__,
		chan, curr);

	return curr;
}

static int s2mu107_fled_set_torch_curr(struct s2mu107_fled_data *fled,
				       int chan, int curr)
{
	int ret = -1, curr_set;
	u8 data, dest;

	if ((chan < 1) || (chan > S2MU107_CH_MAX)) {
		pr_info("%s: Wrong channel!!\n", __func__);
		return -1;
	}

	switch (chan) {
	case 1:
		dest = S2MU107_FLED_CH1_CTRL1;
		break;
	default:
		pr_info("%s: CH%02d does not exist!!\n", __func__, chan);
		return ret;
	}

	if (curr < 10)
		curr = 10;
	else if (curr > s2mu107_fled_torch_curr_max[chan])
		curr = s2mu107_fled_torch_curr_max[chan];

	data = (curr - 10)/10;

	s2mu107_update_reg(fled->i2c, dest, data, S2MU107_CHX_TORCH_IOUT);

	curr_set = s2mu107_fled_get_torch_curr(fled, chan);

	pr_info("%s: curr: %d, curr_set: %d\n", __func__, curr, curr_set);

	ret = 0;

	return ret;

}

static void s2mu107_fled_operating_mode(struct s2mu107_fled_data *fled,
					int mode)
{
	if (fled->set_on_factory) {
		pr_info("%s: Factory Status, Return\n", __func__);
		return;
	}

	if (mode < AUTO_MODE || mode > SYS_MODE) {
		pr_info ("%s: wrong mode\n", __func__);
		mode = AUTO_MODE;
	}

	pr_info("%s: %s\n", __func__,
		s2mu107_fled_operating_mode_string[mode]);
	/* set FLED_MODE */
	s2mu107_update_reg(fled->i2c, S2MU107_FLED_CTRL0,
			   mode << S2MU107_FLED_MODE_SHIFT, S2MU107_FLED_MODE);
}

static int s2mu107_fled_get_mode(struct s2mu107_fled_data *fled, int chan)
{
	int ret = -1;
	u8 status;

	s2mu107_read_reg(fled->i2c, S2MU107_FLED_STATUS1, &status);

	pr_info("%s: S2MU107_FLED_STATUS1: 0x%02x\n", __func__, status);

	if ((chan < 1) || (chan > S2MU107_CH_MAX)) {
		pr_info("%s: Wrong channel!!\n", __func__);
		return -1;
	}

	switch (chan) {
	case 1:
		if (status & S2MU107_CH1_FLASH_ON)
			ret = S2MU107_FLED_MODE_FLASH;
		else if (status & S2MU107_CH1_TORCH_ON)
			ret = S2MU107_FLED_MODE_TORCH;
		else
			ret = S2MU107_FLED_MODE_OFF;
		break;
	default:
		pr_info("%s: CH%02d does not exist!!\n", __func__, chan);
		break;
	}
	return ret;
}

static void s2mu107_pre_enable(struct s2mu107_fled_data *fled, int mode)
{
	u8 status = 0;
	switch (mode) {
	case S2MU107_FLED_MODE_OFF:
		/* If FLED is off, clear EN_FLED_PRE */
		s2mu107_read_reg(fled->i2c, S2MU107_FLED_STATUS1, &status);
		if (!(status & S2MU107_FLED_ON_CHECK))
			s2mu107_update_reg(fled->i2c, S2MU107_FLED_CTRL0,
					   0, S2MU107_EN_FLED_PRE);
		break;
	case S2MU107_FLED_MODE_TORCH:
	case S2MU107_FLED_MODE_FLASH:
		/* Need to set EN_FLED_PRE bit before mode change */
		s2mu107_update_reg(fled->i2c, S2MU107_FLED_CTRL0,
				   S2MU107_EN_FLED_PRE, S2MU107_EN_FLED_PRE);
		break;
	default:
		pr_info("%s: MODE%02d does not exist!!\n", __func__, mode);
		break;
	}
}

static int s2mu107_fled_set_mode(struct s2mu107_fled_data *fled,
				 int chan, int mode)
{
	u8 dest = 0, bit = 0, mask = 0;

	if ((chan <= 0) || (chan > S2MU107_CH_MAX) ||
	    (mode < S2MU107_FLED_MODE_OFF) || (mode >= S2MU107_FLED_MODE_MAX)) {
		pr_err("%s: Wrong channel or mode.\n", __func__);
		return -EFAULT;
	}

	pr_err("%s: CH(%d), mode(%d)\n", __func__, chan, mode);

	/* 0b00: OFF, 0b11: i2c bit control(on) */
	switch (mode) {
	case S2MU107_FLED_MODE_OFF:
		mask = (S2MU107_CHX_FLASH_FLED_EN | S2MU107_CHX_TORCH_FLED_EN);
		bit = S2MU107_FLED_OFF;
		break;
	case S2MU107_FLED_MODE_FLASH:
		mask = S2MU107_CHX_FLASH_FLED_EN;
		if (fled->control_mode == CONTROL_I2C)
			bit = (S2MU107_FLED_ON << S2MU107_FLASH_EN_SHIFT);
		else
			bit = (S2MU107_FLASH_EN_HIGH << S2MU107_FLASH_EN_SHIFT);
		break;
	case S2MU107_FLED_MODE_TORCH:
		s2mu107_fled_operating_mode(fled, SYS_MODE);
		mask = S2MU107_CHX_TORCH_FLED_EN;
		if (fled->control_mode == CONTROL_I2C)
			bit = S2MU107_FLED_ON;
		else
			bit = S2MU107_TORCH_EN_HIGH;
		break;
	default:
		pr_info("%s: MODE%02d does not exist!!\n", __func__, mode);
		return -EFAULT;
	}

	switch (chan) {
	case 1:
		dest = S2MU107_FLED_CTRL1;
		break;
	default:
		pr_info("%s: CH%02d does not exist!!\n", __func__, chan);
		return -EFAULT;
	}

	/* set EN_FLED_PRE bit */
	s2mu107_pre_enable(fled, mode);

	s2mu107_update_reg(fled->i2c, dest, bit, mask);

	if (mode == S2MU107_FLED_MODE_OFF)
		s2mu107_fled_operating_mode(fled, AUTO_MODE);

	return 0;
}

static int ta_notification(struct notifier_block *nb,
			   unsigned long action, void *data)
{
	struct s2mu107_fled_data *fled = container_of(nb,
						     struct s2mu107_fled_data,
						     batt_nb);

	switch (action) {
	case MUIC_NOTIFY_CMD_DETACH:
	case MUIC_NOTIFY_CMD_LOGICALLY_DETACH:
		fled->attach_ta = 0;
		pr_info("%s: FLED detach (attach_ta = %d)\n",
			__func__, fled->attach_ta);
		break;
	case MUIC_NOTIFY_CMD_ATTACH:
	case MUIC_NOTIFY_CMD_LOGICALLY_ATTACH:
		fled->attach_ta = 1;
		pr_info("%s: FLED attach (attach_ta = %d)\n",
			__func__, fled->attach_ta);
		break;
	default:
		break;
	}
	return 0;
}
int s2mu107_fled_show_flash_en(void)
{
	return g_fled_data->flash_en;
}
EXPORT_SYMBOL_GPL(s2mu107_fled_show_flash_en);

static void s2mu107_fled_call_sc_set_property(struct s2mu107_fled_data *fled,
					int onoff)
{
	union power_supply_propval value;

	value.intval = 0;

	switch (onoff) {
	case S2MU107_FLASH_BOOST_ON:
		//power_supply_set_property(fled->sc_psy, POWER_SUPPLY_EXT_PROP_FLED_BOOST_ON, &value);
		psy_do_property("s2mu107-switching-charger", set,
			POWER_SUPPLY_EXT_PROP_FLED_BOOST_ON, value);
		break;
	case S2MU107_FLASH_BOOST_OFF:
		//power_supply_set_property(fled->sc_psy, POWER_SUPPLY_EXT_PROP_FLED_BOOST_OFF, &value);
		psy_do_property("s2mu107-switching-charger", set,
			POWER_SUPPLY_EXT_PROP_FLED_BOOST_OFF, value);
		break;
	default:
		break;
	}
}

int s2mu107_led_mode_ctrl(int state)
{
	struct s2mu107_fled_data *fled = g_fled_data;
	int gpio_torch = fled->torch_gpio;
	int gpio_flash = fled->flash_gpio;

	pr_info("%s: state = %d gpio=(%d %d)\n", __func__, state, gpio_torch, gpio_flash);

	gpio_request(gpio_torch, "s2mu107_gpio_torch");
	gpio_request(gpio_flash, "s2mu107_gpio_flash");

	switch (state) {
	case S2MU107_FLED_MODE_OFF:
		if (fled->torch_en == S2MU107_TORCH_ENABLE) {
			gpio_direction_output(gpio_torch, 0);
			/* PSK auto on */
			s2mu107_update_reg(fled->chg, 0x3C, 0x01, 0x03);
		} else
			gpio_direction_output(gpio_torch, 0);

		if (fled->flash_en == S2MU107_FLASH_ENABLE) {
			gpio_direction_output(gpio_flash, 0);

			usleep_range(10000, 11000);

			s2mu107_fled_call_sc_set_property(fled,
				S2MU107_FLASH_BOOST_OFF);
		} else
			gpio_direction_output(gpio_flash, 0);

		fled->flash_en = S2MU107_FLASH_DISABLE;
		fled->torch_en = S2MU107_TORCH_DISABLE;
		s2mu107_fled_operating_mode(fled, AUTO_MODE);
		break;
	case S2MU107_FLED_MODE_TORCH:
		/* force PSK to stay on */
		s2mu107_update_reg(fled->chg, 0x3C, 0x03, 0x03);
		/* chgange SYS MODE when turn on torch */
		s2mu107_fled_operating_mode(fled, SYS_MODE);
		gpio_direction_output(gpio_torch, 1);
		fled->torch_en = S2MU107_TORCH_ENABLE;
		break;
	case S2MU107_FLED_MODE_FLASH:
		s2mu107_fled_call_sc_set_property(fled,
			S2MU107_FLASH_BOOST_ON);
		s2mu107_fled_operating_mode(fled, AUTO_MODE);
		/* 0x8C[7:4] set to 1111 */
		s2mu107_update_reg(fled->chg, 0x8C, 0xF0, 0xF0);
		gpio_direction_output(gpio_flash, 1);
		/* 0x8C[7:4] set to 1010 */
		s2mu107_update_reg(fled->chg, 0x8C, 0xA0, 0xF0);
		fled->flash_en = S2MU107_FLASH_ENABLE;
		break;
	default:
		pr_info("%s: MODE%02d does not exist!!\n", __func__, state);
		break;
	}

	gpio_free(gpio_torch);
	gpio_free(gpio_flash);

	return 0;
}

int s2mu107_mode_change_cam_to_leds(enum cam_flash_mode cam_mode)
{
	int mode = -1;

	switch (cam_mode) {
	case CAMERA_FLASH_OP_OFF:
		mode = S2MU107_FLED_MODE_OFF;
		break;
	case CAMERA_FLASH_OP_FIREHIGH:
		mode = S2MU107_FLED_MODE_FLASH;
		break;
	case CAMERA_FLASH_OP_FIRELOW:
	case CAMERA_FLASH_OP_FIRERECORD:
		mode = S2MU107_FLED_MODE_TORCH;
		break;
	default:
		mode = S2MU107_FLED_MODE_OFF;
		break;
	}

	return mode;
}

int s2mu107_fled_set_mode_ctrl(int chan, enum cam_flash_mode cam_mode)
{
	struct s2mu107_fled_data *fled = g_fled_data;
	int mode = -1;

	mode = s2mu107_mode_change_cam_to_leds(cam_mode);

	if ((chan <= 0) || (chan > S2MU107_CH_MAX) ||
	     (mode < 0) || (mode >= S2MU107_FLED_MODE_MAX)) {
		pr_err("%s: channel: %d, mode: %d\n", __func__, chan, mode);
		pr_err("%s: Wrong channel or mode.\n", __func__);
		return -1;
	}

	s2mu107_fled_set_mode(fled, chan, mode);
	s2mu107_fled_test_read(fled);

	return 0;
}

int s2mu107_fled_set_curr(int chan, enum cam_flash_mode cam_mode, int curr)
{
	struct s2mu107_fled_data *fled = g_fled_data;
	int mode = -1;

	mode = s2mu107_mode_change_cam_to_leds(cam_mode);

	/* Check channel */
	if ((chan <= 0) || (chan > S2MU107_CH_MAX)) {
		pr_err("%s: Wrong channel.\n", __func__);
		return -EFAULT;
	}

	switch (mode) {
	case S2MU107_FLED_MODE_TORCH:
		/* Set curr. */
		s2mu107_fled_set_torch_curr(fled, chan, curr);
		break;
	case S2MU107_FLED_MODE_FLASH:
		/* Set curr. */
		s2mu107_fled_set_flash_curr(fled, chan, curr);
		break;
	default:
		pr_info("%s: MODE(%02d) does not exist!!\n", __func__, mode);
		return -1;
	}
	/* Test read */
	s2mu107_fled_test_read(fled);

	return 0;
}

int s2mu107_fled_get_curr(int chan, enum cam_flash_mode cam_mode)
{
	struct s2mu107_fled_data *fled = g_fled_data;
	int mode = -1, curr = 0;

	mode = s2mu107_mode_change_cam_to_leds(cam_mode);

	/* Check channel */
	if ((chan <= 0) || (chan > S2MU107_CH_MAX)) {
		pr_err("%s: Wrong channel.\n", __func__);
		return -EFAULT;
	}

	switch (mode) {
		case S2MU107_FLED_MODE_TORCH:
			curr = s2mu107_fled_get_torch_curr(fled, chan);
			break;
		case S2MU107_FLED_MODE_FLASH:
			curr = s2mu107_fled_get_flash_curr(fled, chan);
			break;
		default:
			return -1;
	}
	/* Test read */
	s2mu107_fled_test_read(fled);

	return curr;
}

static ssize_t fled_flash_curr_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct s2mu107_fled_data *fled = container_of(led_cdev,
						      struct s2mu107_fled_data,
						      cdev);
	int cnt = 0, curr = 0, i;
	char str[1016] = {0,};

	/* Read curr. */
	for (i = 1; i <= S2MU107_CH_MAX; i++) {
		curr = s2mu107_fled_get_flash_curr(fled, i);
		pr_info("%s: CH(%d), Curr(%dmA)\n", __func__, i, curr);
		if (curr >= 0)
			cnt += sprintf(str + strlen(str),
				       "CH%02d: %dmA, ", i, curr);
	}

	cnt += sprintf(str + strlen(str), "\n");
	strcpy(buf, str);

	return cnt;
}

static ssize_t fled_flash_curr_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct s2mu107_fled_data *fled = container_of(led_cdev,
						      struct s2mu107_fled_data,
						      cdev);
	int chan = -1, curr = -1;

	sscanf(buf, "%d %d", &chan, &curr);

	/* Check channel */
	if ((chan <= 0) || (chan > S2MU107_CH_MAX)) {
		pr_err("%s: Wrong channel.\n", __func__);
		return -EFAULT;
	}

	/* Set curr. */
	s2mu107_fled_set_flash_curr(fled, chan, curr);

	/* Test read */
	s2mu107_fled_test_read(fled);

	return size;
}


static ssize_t fled_torch_curr_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct s2mu107_fled_data *fled = container_of(led_cdev,
						      struct s2mu107_fled_data,
						      cdev);
	int cnt = 0, curr = 0, i;
	char str[128] = {0,};

	/* Read curr. */
	for (i = 1; i <= S2MU107_CH_MAX; i++) {
		curr = s2mu107_fled_get_torch_curr(fled, i);
		pr_info("%s: CH(%d), Curr(%dmA)\n", __func__, i, curr);
		if (curr >= 0)
			cnt += sprintf(str + strlen(str),
				       "CH%02d: %dmA, ", i, curr);
	}

	cnt += sprintf(str + strlen(str), "\n");
	strcpy(buf, str);
	pr_info("%s: str=%s cnt=%d\n", __func__, str, cnt);

	return cnt;
}

static ssize_t fled_torch_curr_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct s2mu107_fled_data *fled = container_of(led_cdev,
						      struct s2mu107_fled_data,
						      cdev);
	int chan = -1, curr = -1;

	sscanf(buf, "%d %d", &chan, &curr);

	/* Check channel */
	if ((chan <= 0) || (chan > S2MU107_CH_MAX)) {
		pr_err("%s: Wrong channel.\n", __func__);
		return -EFAULT;
	}

	/* Set curr. */
	s2mu107_fled_set_torch_curr(fled, chan, curr);

	/* Test read */
	s2mu107_fled_test_read(fled);

	return size;
}

static ssize_t fled_mode_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct s2mu107_fled_data *fled = container_of(led_cdev,
						      struct s2mu107_fled_data,
						      cdev);
	int cnt = 0, mode = 0, i;
	char str[512] = {0,};

	s2mu107_fled_test_read(fled);

	for (i = 1; i <= S2MU107_CH_MAX; i++) {
		mode = s2mu107_fled_get_mode(fled, i);
		if (mode >= 0)
			cnt += sprintf(str + strlen(str), "CH%02d: %s, ", i,
				       s2mu107_fled_mode_string[mode]);
	}

	cnt += sprintf(str + strlen(str), "\n");

	strcpy(buf, str);
	pr_err("%s: str=%s cnt=%d\n", __func__, str, cnt);

    return cnt;
}

static ssize_t fled_mode_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct s2mu107_fled_data *fled = container_of(led_cdev,
						      struct s2mu107_fled_data,
						      cdev);
	int chan = -1, mode = -1;

	sscanf(buf, "%d %d", &chan, &mode);

	if ((chan <= 0) || (chan > S2MU107_CH_MAX) ||
	     (mode < 0) || (mode >= S2MU107_FLED_MODE_MAX)) {
		pr_err("%s: channel: %d, mode: %d\n", __func__, chan, mode);
		pr_err("%s: Wrong channel or mode.\n", __func__);
		return -EFAULT;
	}
	/* I2C Control */
	if (fled->control_mode == CONTROL_I2C) {
		s2mu107_fled_set_mode(fled, chan, mode);
		return size;
	}
	/* GPIO Control */
	switch (mode) {
	case S2MU107_FLED_MODE_OFF:
		s2mu107_led_mode_ctrl(S2MU107_FLED_MODE_OFF);
		break;
	case S2MU107_FLED_MODE_TORCH:
		s2mu107_led_mode_ctrl(S2MU107_FLED_MODE_TORCH);
		break;
	case S2MU107_FLED_MODE_FLASH:
		s2mu107_led_mode_ctrl(S2MU107_FLED_MODE_FLASH);
		break;
	default:
		break;
	}
	s2mu107_fled_test_read(fled);

	return size;
}

static DEVICE_ATTR(fled_mode, 0644, fled_mode_show, fled_mode_store);
static DEVICE_ATTR(fled_flash_curr, 0644,
		   fled_flash_curr_show, fled_flash_curr_store);
static DEVICE_ATTR(fled_torch_curr, 0644,
		   fled_torch_curr_show, fled_torch_curr_store);

static struct attribute *s2mu107_fled_attrs[] = {
	&dev_attr_fled_mode.attr,
	&dev_attr_fled_flash_curr.attr,
	&dev_attr_fled_torch_curr.attr,
	NULL
};
ATTRIBUTE_GROUPS(s2mu107_fled);

void s2mu107_fled_set_operation_mode(int mode)
{
	if (mode) {
		s2mu107_fled_operating_mode(g_fled_data, TA_MODE);
		g_fled_data->set_on_factory = 1;
		pr_info("%s: TA only mode set\n", __func__);
	}
	else {
		g_fled_data->set_on_factory = 0;
		s2mu107_fled_operating_mode(g_fled_data, AUTO_MODE);
		pr_info("%s: Auto control mode set\n", __func__);
	}
}

static void s2mu107_set_interface(struct s2mu107_fled_data *fled)
{
	int flash_gpio = fled->pdata->flash_gpio;
	int torch_gpio = fled->pdata->torch_gpio;

	if (gpio_is_valid(flash_gpio) && gpio_is_valid(torch_gpio)) {
		pr_info("%s: s2mu107_fled gpio mode\n", __func__);

		/* Set GPIO */
		fled->control_mode = CONTROL_GPIO;

		/* Request GPIO */
		gpio_request_one(fled->flash_gpio,
				 GPIOF_OUT_INIT_LOW, "LED_GPIO_OUTPUT_LOW");
		gpio_request_one(fled->torch_gpio,
				 GPIOF_OUT_INIT_LOW, "LED_GPIO_OUTPUT_LOW");

		/* Free GPIO */
		gpio_free(fled->flash_gpio);
		gpio_free(fled->torch_gpio);

		/* CAM_FLASH_EN -> FLASH gpio mode */
		s2mu107_fled_set_mode(fled, 1, S2MU107_FLED_MODE_FLASH);

		/* CAM_TORCH_EN -> TORCH gpio mode */
		s2mu107_fled_set_mode(fled, 1, S2MU107_FLED_MODE_TORCH);

		/* FLED off */
		s2mu107_led_mode_ctrl(S2MU107_FLED_MODE_OFF);
	} else
		fled->control_mode = CONTROL_I2C;
}

static void s2mu107_fled_init(struct s2mu107_fled_data *fled)
{
	int ret;
	pr_info("%s: s2mu107_fled init start\n", __func__);

	/* 0x8C[7:4] set to 1010 */
	ret = s2mu107_update_reg(fled->chg, 0x8C, 0xA0, 0xF0);
	if (ret < 0)
		pr_info("%s: fail to update reg\n", __func__);

	/* 0x25[2:1] set to 11 */
	ret = s2mu107_update_reg(fled->i2c, 0x25, 0x06, 0x06);
	if (ret < 0)
		pr_info("%s: fail to update reg\n", __func__);

	/* FLED interface(I2C or GPIO) setting */
	s2mu107_set_interface(fled);

	/* FLED driver operating mode set, TA only mode*/
	fled->set_on_factory = 0;
	if (factory_mode) {
		s2mu107_fled_operating_mode(fled, TA_MODE);
		fled->set_on_factory = 1;
	}

	/* for Flash Auto boost */
	s2mu107_update_reg(fled->i2c, S2MU107_FLED_TEST2, 0x60, 0x60);

	/* flash current set */
	s2mu107_fled_set_flash_curr(fled, 1, fled->pdata->flash_current);

	/* torch current set */
	s2mu107_fled_set_torch_curr(fled, 1, fled->pdata->torch_current);

	s2mu107_fled_test_read(fled);
}

#if defined(CONFIG_OF)
static int s2mu107_led_dt_parse_pdata(struct device *dev,
				      struct s2mu107_fled_platform_data *pdata)
{
	struct device_node *led_np, *np;
	int ret;

#if 0
	struct device_node *c_np;
	u32 temp, index;
#endif
	led_np = dev->parent->of_node;
	pr_info("%s: DT parsing\n", __func__);

	if (!led_np) {
		pr_err("<%s> could not find led sub-node led_np\n", __func__);
		return -ENODEV;
	}

	np = of_find_node_by_name(led_np, "flash_led");
	if (!np) {
		pr_err("%s : could not find led sub-node np\n", __func__);
		return -EINVAL;
	}

	ret = pdata->flash_gpio = of_get_named_gpio(np, "flash-gpio", 0);
	if (ret < 0) {
		pr_err("%s : can't get flash-gpio\n", __func__);
		return ret;
	}

	ret = pdata->torch_gpio = of_get_named_gpio(np, "torch-gpio", 0);
	if (ret < 0) {
		pr_err("%s : can't get torch-gpio\n", __func__);
		return ret;
	}

	ret = of_property_read_u32(np, "flash_current",
				   &pdata->flash_current);
	if (ret < 0)
		pr_err("%s : could not find flash_current\n", __func__);

	ret = of_property_read_u32(np, "torch_current",
				   &pdata->torch_current);
	if (ret < 0)
		pr_err("%s : could not find torch_current\n", __func__);

	ret = of_property_read_u32(np, "movie_current",
				   &pdata->movie_current);
	if (ret < 0)
		pr_err("%s : could not find movie_current\n", __func__);

	ret = of_property_read_u32(np, "factory_current",
				   &pdata->factory_current);
	if (ret < 0)
		pr_err("%s : could not find factory_current\n", __func__);

	ret = of_property_read_u32_array(np, "flashlight_current",
					 pdata->flashlight_current,
					 S2MU107_FLASH_LIGHT_MAX);
	if (ret < 0) {
		pr_err("%s : could not find flashlight_current\n", __func__);

		//default setting
		pdata->flashlight_current[0] = 45;
		pdata->flashlight_current[1] = 75;
		pdata->flashlight_current[2] = 125;
		pdata->flashlight_current[3] = 195;
		pdata->flashlight_current[4] = 270;
	}
	pr_err("%s : fled current config in dtsi: %d %d %d %d\n",
		__func__, pdata->flash_current, pdata->torch_current, pdata->movie_current, pdata->factory_current);

	if (pdata->sc_name == NULL)
		pdata->sc_name = "s2mu107-switching-charger";

	return 0;
}

#endif /* CONFIG_OF */

#define CONFIG_S2MU107_FLASH_LED_SYSFS 1

#if defined(CONFIG_S2MU107_FLASH_LED_SYSFS)
static ssize_t rear_flash_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	int flash_current = 0, torch_current = 0;
	int mode = -1, value = 0;

	pr_info("%s: rear_flash_store start\n", __func__);
	if ((buf == NULL) || kstrtouint(buf, 10, &value)) {
		return -1;
	}

	if (value < 0) {
		pr_err("%s: value: %d, Wrong mode.\n", __func__, value);
		return -EFAULT;
	}
	pr_info("%s: %d: rear_flash_store:\n", __func__, value);

	if(NULL == g_fled_data) {
		pr_err("%s: s2mu107 flash device is NULL, return\n", __func__);
		return -1;
	}
	g_fled_data->sysfs_input_data = value;

	flash_current = g_fled_data->pdata->flash_current;
	torch_current = g_fled_data->pdata->torch_current;

	if (value <= 0) {
		mode = S2MU107_FLED_MODE_OFF;
	} else if (value == 1) {
		mode = S2MU107_FLED_MODE_TORCH;
		torch_current = g_fled_data->pdata->flashlight_current[0];
	} else if (value == 100) {
		/* Factory Torch*/
		pr_info("%s: factory torch current [%d]\n",
			__func__, g_fled_data->pdata->factory_current);
		torch_current = g_fled_data->pdata->factory_current;
		mode = S2MU107_FLED_MODE_TORCH;
	} else if (value == 200) {
		/* Factory Flash */
		pr_info("%s: factory flash current [%d]\n",
			__func__, g_fled_data->pdata->factory_current);
		flash_current = g_fled_data->pdata->factory_current;
		mode = S2MU107_FLED_MODE_FLASH;
	} else if (value <= 1010 && value >= 1001) {
		mode = S2MU107_FLED_MODE_TORCH;
		/* (value) 1001, 1002, 1004, 1006, 1009 */
		if (value <= 1001)
			torch_current = g_fled_data->pdata->flashlight_current[0];
		else if (value <= 1002)
			torch_current = g_fled_data->pdata->flashlight_current[1];
		else if (value <= 1004)
			torch_current = g_fled_data->pdata->flashlight_current[2];
		else if (value <= 1006)
			torch_current = g_fled_data->pdata->flashlight_current[3];
		else if (value <= 1009)
			torch_current = g_fled_data->pdata->flashlight_current[4];
		else
			torch_current = g_fled_data->pdata->torch_current;
		g_fled_data->sysfs_input_data = 1;
	} else if (value == 2) {
		mode = S2MU107_FLED_MODE_FLASH;
	}

	mutex_lock(&g_fled_data->lock);
	if (g_fled_data->control_mode == CONTROL_I2C) {
		s2mu107_fled_set_mode(g_fled_data, 1, mode);
	} else {
		if (mode == S2MU107_FLED_MODE_TORCH) {
			pr_info("%s: %d: S2MU107_FLED_MODE_TORCH - %dmA\n",
				__func__, value, torch_current);
			/* torch current set */
			s2mu107_fled_set_torch_curr(g_fled_data, 1,
						    torch_current);
			s2mu107_led_mode_ctrl(S2MU107_FLED_MODE_TORCH);
		} else if (mode == S2MU107_FLED_MODE_FLASH) {
			pr_info("%s: %d: S2MU107_FLED_MODE_FLASH - %dmA\n",
				__func__, value, flash_current);
			/* flash current set */
			s2mu107_fled_set_flash_curr(g_fled_data, 1,
						    flash_current);
			s2mu107_led_mode_ctrl(S2MU107_FLED_MODE_FLASH);
		} else {
			pr_info("%s: %d: S2MU107_FLED_MODE_OFF\n",
				__func__, value);

			/* flash, torch current set for initial current */
			flash_current = g_fled_data->pdata->flash_current;
			torch_current = g_fled_data->pdata->torch_current;

			/* flash current set */
			s2mu107_fled_set_flash_curr(g_fled_data, 1,
						    flash_current);
			/* torch current set */
			s2mu107_fled_set_torch_curr(g_fled_data, 1,
						    torch_current);
			s2mu107_led_mode_ctrl(S2MU107_FLED_MODE_OFF);
		}
	}

	mutex_unlock(&g_fled_data->lock);
	s2mu107_fled_test_read(g_fled_data);
	pr_info("%s: rear_flash_store END\n", __func__);
	return size;
}

static ssize_t rear_flash_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", g_fled_data->sysfs_input_data);
}


static DEVICE_ATTR(rear_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
		   rear_flash_show, rear_flash_store);
static DEVICE_ATTR(rear_torch_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
		   rear_flash_show, rear_flash_store);

int create_flash_sysfs(void)
{
	int err = -ENODEV;

	if (IS_ERR_OR_NULL(camera_class)) {
		pr_err("flash_sysfs: error, camera class not exist");
		return -ENODEV;
	}
	if (!IS_ERR_OR_NULL(cam_dev_flash)) {
		device_remove_file(cam_dev_flash, &dev_attr_rear_flash);
		device_remove_file(cam_dev_flash, &dev_attr_rear_torch_flash);
		flash_dev = cam_dev_flash;
	} else {
		flash_dev = device_create(camera_class, NULL, 0, NULL, "flash");
		if (IS_ERR(flash_dev)) {
			pr_err("flash_sysfs: failed to create device(flash)\n");
			return -ENODEV;
		}
	}

	err = device_create_file(flash_dev, &dev_attr_rear_flash);
	if (unlikely(err < 0)) {
		pr_err("flash_sysfs: failed to create device file, %s\n",
			dev_attr_rear_flash.attr.name);
	}

	err = device_create_file(flash_dev, &dev_attr_rear_torch_flash);
	if (unlikely(err < 0)) {
		pr_err("flash_sysfs: failed to create device file, %s\n",
			dev_attr_rear_torch_flash.attr.name);
	}

	return 0;
}
#endif

static int s2mu107_led_pinctrl_init(
	struct s2mu107_pinctrl_info *s2mu107_pctrl, struct device *dev)
{

	s2mu107_pctrl->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(s2mu107_pctrl->pinctrl)) {
		pr_err("%s: Getting pinctrl handle failed", __func__);
		return -EINVAL;
	}
	s2mu107_pctrl->gpio_state_active =
		pinctrl_lookup_state(s2mu107_pctrl->pinctrl,
				"fled_default");
	if (IS_ERR_OR_NULL(s2mu107_pctrl->gpio_state_active)) {
		pr_err("%s: Failed to get the active state pinctrl handle", __func__);
		return -EINVAL;
	}
	s2mu107_pctrl->gpio_state_suspend
		= pinctrl_lookup_state(s2mu107_pctrl->pinctrl,
				"fled_suspend");
	if (IS_ERR_OR_NULL(s2mu107_pctrl->gpio_state_suspend)) {
		pr_err("%s Failed to get the suspend state pinctrl handle", __func__);
		return -EINVAL;
	}

	return 0;
}

int ext_pmic_cam_flash_ctrl(int cam_mode, int curr)
{
	int flash_current = 0, torch_current = 0;
	int mode = -1;

	if(NULL == g_fled_data) {
		pr_err("%s: s2mu107 flash device is NULL, return\n", __func__);
		return 0;
	}
	mutex_lock(&g_fled_data->lock);
	mode = s2mu107_mode_change_cam_to_leds(cam_mode);
	pr_info("%s: ext_pmic_cam_flash_ctrl cam_mode=%d curr=%d mode=%d\n", __func__, cam_mode, curr, mode);

	if(curr > 0) {
		flash_current = curr;
		torch_current = curr;
	} else {
		flash_current = g_fled_data->pdata->flash_current;
		torch_current = g_fled_data->pdata->torch_current;
	}

	if (g_fled_data->control_mode == CONTROL_I2C) {
		s2mu107_fled_set_mode(g_fled_data, 1, mode);
	} else {
		if (mode == S2MU107_FLED_MODE_TORCH) {
			pr_info("%s: S2MU107_FLED_MODE_TORCH - %dmA\n",
				__func__, torch_current);
			/* torch current set */
			s2mu107_fled_set_torch_curr(g_fled_data, 1,
						    torch_current);
			s2mu107_led_mode_ctrl(S2MU107_FLED_MODE_TORCH);
		} else if (mode == S2MU107_FLED_MODE_FLASH) {
			pr_info("%s: S2MU107_FLED_MODE_FLASH - %dmA\n",
				__func__, flash_current);
			/* flash current set */
			s2mu107_fled_set_flash_curr(g_fled_data, 1,
						    flash_current);
			s2mu107_led_mode_ctrl(S2MU107_FLED_MODE_FLASH);
		} else {
			pr_info("%s: S2MU107_FLED_MODE_OFF\n",
				__func__);

			/* flash, torch current set for initial current */
			flash_current = 0;
			torch_current = 0;

			/* flash current set */
			s2mu107_fled_set_flash_curr(g_fled_data, 1,
						    flash_current);
			/* torch current set */
			s2mu107_fled_set_torch_curr(g_fled_data, 1,
						    torch_current);
			s2mu107_led_mode_ctrl(S2MU107_FLED_MODE_OFF);
		}
	}

	mutex_unlock(&g_fled_data->lock);
	s2mu107_fled_test_read(g_fled_data);
	pr_info("%s: ext_pmic_cam_flash_ctrl END\n", __func__);
	return 0;
}

static int s2mu107_led_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct s2mu107_dev *s2mu107 = dev_get_drvdata(pdev->dev.parent);
	struct s2mu107_fled_data *fled_data;
	char name[20];

	pr_info("%s: s2mu107_fled start\n", __func__);

	if (!s2mu107) {
		dev_err(&pdev->dev, "drvdata->dev.parent not supplied\n");
		return -ENODEV;
	}

	fled_data = devm_kzalloc(&pdev->dev,
				 sizeof(struct s2mu107_fled_data), GFP_KERNEL);
	if (!fled_data) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		return -ENOMEM;
	}

	fled_data->dev = &pdev->dev;
	fled_data->i2c = s2mu107->i2c;
	fled_data->chg = s2mu107->chg;
	fled_data->pdata = devm_kzalloc(&pdev->dev, sizeof(*(fled_data->pdata)),
					GFP_KERNEL);
	if (!fled_data->pdata) {
		pr_err("%s: failed to allocate platform data\n", __func__);
		return -ENOMEM;
	}

	if (s2mu107->dev->of_node) {
		ret = s2mu107_led_dt_parse_pdata(&pdev->dev, fled_data->pdata);
		if (ret < 0) {
			pr_err("%s: not found leds dt! ret=%d\n",
				__func__, ret);
			return ret;
		}
	}

	platform_set_drvdata(pdev, fled_data);

	/* Store fled_data for EXPORT_SYMBOL */
	g_fled_data = fled_data;

	snprintf(name, sizeof(name), "fled-s2mu107");
	fled_data->cdev.name = name;
	fled_data->cdev.groups = s2mu107_fled_groups;

	ret = devm_led_classdev_register(&pdev->dev, &fled_data->cdev);
	if (ret < 0) {
		pr_err("%s: unable to register LED class dev\n", __func__);
		return ret;
	}

	g_fled_data->flash_gpio		= fled_data->pdata->flash_gpio;
	g_fled_data->torch_gpio		= fled_data->pdata->torch_gpio;

	s2mu107_fled_init(g_fled_data);
	mutex_init(&fled_data->lock);
	fled_data->sc_psy = power_supply_get_by_name(fled_data->pdata->sc_name);

#if defined(CONFIG_S2MU107_FLASH_LED_SYSFS)
	/* create sysfs for camera */
	create_flash_sysfs();
#endif

	muic_notifier_register(&fled_data->batt_nb, ta_notification,
			       MUIC_NOTIFY_DEV_CHARGER);
#if 0
	/* init pinctrl */
	ret = s2mu107_led_pinctrl_init(&fled_data->flash_pctrl, &pdev->dev);
	if (ret >= 0) {
		// make pin state to suspend
		ret = pinctrl_select_state(fled_data->flash_pctrl.pinctrl, fled_data->flash_pctrl.gpio_state_suspend);
		if (ret < 0) {
			pr_info("%s: Cannot set pin to suspend state", __func__);
			return ret;
		}
	}
#endif
	pr_info("%s: s2mu107_fled loaded\n", __func__);
	return 0;
}

static int s2mu107_led_remove(struct platform_device *pdev)
{
#if defined(CONFIG_S2MU107_FLASH_LED_SYSFS)
	device_remove_file(flash_dev, &dev_attr_rear_flash);
	device_remove_file(flash_dev, &dev_attr_rear_torch_flash);
	device_destroy(camera_class, 0);
	class_destroy(camera_class);
#endif
	mutex_destroy(&g_fled_data->lock);
	return 0;
}

static int32_t s2mu107_led_i2c_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{
	struct s2mu107_dev *s2mu107;
	struct s2mu107_fled_platform_data *pdata = i2c->dev.platform_data;
	struct s2mu107_fled_data *fled_data = NULL;

	int ret = 0;

	pr_info("%s i2c addr = 0x%x g_fled_data=%p\n", __func__, i2c->addr, g_fled_data);

	if (NULL == g_fled_data) {
		s2mu107 = devm_kzalloc(&i2c->dev, sizeof(struct s2mu107_dev),
				       GFP_KERNEL);
		if (!s2mu107) {
			dev_err(&i2c->dev, "%s: Failed to alloc mem for s2mu107\n",
					__func__);
			return -ENOMEM;
		}

		fled_data = devm_kzalloc(&i2c->dev,
					     sizeof(struct s2mu107_fled_data),
					     GFP_KERNEL);
		if (!fled_data) {
			pr_err("%s: failed to allocate driver data\n", __func__);
			return -ENOMEM;
		}

		if (i2c->dev.of_node) {
			pdata = devm_kzalloc(&i2c->dev,
					     sizeof(struct s2mu107_fled_platform_data),
					     GFP_KERNEL);
			if (!pdata) {
				dev_err(&i2c->dev, "Failed to allocate memory\n");
				ret = -ENOMEM;
				goto err;
			}

			ret = s2mu107_led_dt_parse_pdata(&i2c->dev, pdata);
			if (ret < 0) {
				dev_err(&i2c->dev, "Failed to get device of_node\n");
				goto err;
			}

			fled_data->pdata = i2c->dev.platform_data = pdata;
		} else {
			pr_err("%s: there is no of_node data\n", __func__);
			fled_data->pdata = pdata = i2c->dev.platform_data;
		}
		s2mu107->dev = &i2c->dev;
		s2mu107->i2c = i2c;
		s2mu107->irq = i2c->irq;
		mutex_init(&s2mu107->i2c_lock);

		pr_info("%s: i2c_set_clientdata for s2mu107=%p pdata=%p\n", __func__, s2mu107, fled_data->pdata);
		i2c_set_clientdata(i2c, s2mu107);

		/* Store fled_data for EXPORT_SYMBOL */
		g_fled_data = fled_data;

		s2mu107_fled_init(g_fled_data);
		mutex_init(&fled_data->lock);
		muic_notifier_register(&fled_data->batt_nb, ta_notification,
			       	       MUIC_NOTIFY_DEV_CHARGER);
	} else {
		fled_data = g_fled_data;
	}

	ret = s2mu107_led_pinctrl_init(&fled_data->flash_pctrl, &i2c->dev);
	if (ret >= 0) {
		// make pin state to suspend
		ret = pinctrl_select_state(fled_data->flash_pctrl.pinctrl, fled_data->flash_pctrl.gpio_state_suspend);
		if (ret < 0) {
			pr_info("%s: Cannot set pin to suspend state", __func__);
			return ret;
		}
		else
			pr_info("%s: qfh success to set pin to suspend state", __func__);
	}
	pr_info("%s: i2c probe done fled_data=%p\n", __func__, fled_data);

	return ret;

err:
	kfree(s2mu107);

	return ret;
}

static int s2mu107_led_i2c_remove(struct i2c_client *client)
{
	struct s2mu107_dev *s2mu107 = i2c_get_clientdata(client);
	pr_info("%s: remove i2c device s2mu107 = %p\n", __func__, s2mu107);
	i2c_unregister_device(s2mu107->i2c);
	i2c_set_clientdata(client, NULL);

	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id cam_flash_dt_match[] = {
	{.compatible = "qcom,s2mu107-fled", .data = NULL},
	{}
};
MODULE_DEVICE_TABLE(of, cam_flash_dt_match);
#endif

static const struct platform_device_id s2mu107_leds_id[] = {
	{"leds-s2mu107", 0},
	{},
};

static struct platform_driver s2mu107_led_driver = {
	.driver = {
		.name  = "leds-s2mu107",
		.owner = THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table = cam_flash_dt_match,
#endif
		},
	.probe  = s2mu107_led_probe,
	.remove = s2mu107_led_remove,
	.id_table = s2mu107_leds_id,
};

#define FLED_DRIVER_I2C "i2c_fled"

static const struct i2c_device_id i2c_id[] = {
	{FLED_DRIVER_I2C, (kernel_ulong_t)NULL},
	{ }
};

static struct i2c_driver s2mu107_led_driver_i2c = {
	.id_table = i2c_id,
	.probe = s2mu107_led_i2c_probe,
	.remove = s2mu107_led_i2c_remove,
	.driver = {
		.name = FLED_DRIVER_I2C,
		.owner = THIS_MODULE,
		.of_match_table = cam_flash_dt_match,
		.suppress_bind_attrs = true,	
	},
};

static int __init s2mu107_led_driver_init(void)
{
	int rc = 0;
	pr_info("%s: platform_driver_register start...\n", __func__);

	rc = platform_driver_register(&s2mu107_led_driver);
	if (rc < 0) {
		pr_info("%s: platform_driver_register Failed: rc = %d",
			__func__, rc);
		return rc;
	}

	pr_info("%s: s2mu107_fled i2c_add_driver start...\n", __func__);

	rc = i2c_add_driver(&s2mu107_led_driver_i2c);
	if (rc)
		pr_info("%s: s2mu107_fled i2c_add_driver rc=%d\n", __func__, rc);

	return rc;
}

module_init(s2mu107_led_driver_init);

static void __exit s2mu107_led_driver_exit(void)
{
	pr_info("%s: s2mu107_led_driver_exit\n", __func__);
	platform_driver_unregister(&s2mu107_led_driver);
	i2c_del_driver(&s2mu107_led_driver_i2c);
}
module_exit(s2mu107_led_driver_exit);

MODULE_AUTHOR("Keunho Hwang <keunho.hwang@samsung.com>");
MODULE_DESCRIPTION("SAMSUNG s2mu107 flash LED Driver");
MODULE_LICENSE("GPL");
