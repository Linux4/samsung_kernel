/*
 * leds-s2mu106.c - LED class driver for S2MU106 FLASH LEDs.
 *
 * Copyright (C) 2018 Samsung Electronics
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
#include <linux/mfd/samsung/s2mu106.h>
#include <linux/leds-s2mu106.h>
#include <linux/platform_device.h>

struct s2mu106_fled_data *f_flash_data;

#define CONTROL_I2C	0
#define CONTROL_GPIO	1

static char *s2mu106_fled_mode_string[] = {
	"OFF",
	"TORCH",
	"FLASH",
};

static char *s2mu106_fled_operating_mode_string[] = {
	"AUTO",
	"BOOST",
	"TA",
	"SYS",
};

/* IC current limit */
static int s2mu106_fled_torch_curr_max[] = {
	0, 320, 320, 320
};

/* IC current limit */
static int s2mu106_fled_flash_curr_max[] = {
	0, 1600, 1600, 500
};

#if DEBUG_TEST_READ
static void s2mu106_fled_test_read(struct s2mu106_fled_data *fled)
{
    u8 data;
    char str[1016] = {0,};
    int i;
	struct i2c_client *i2c = fled->i2c;

    for (i = 0x0B; i <= 0x0C; i++) {
        s2mu106_read_reg(i2c, i, &data);
        sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
    }
	for (i = 0x14; i <= 0x15; i++) {
		s2mu106_read_reg(i2c, i, &data);
		sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}

	for (i = 0x53; i <= 0x5A; i++) {
		s2mu106_read_reg(i2c, i, &data);
		sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}

    s2mu106_read_reg(i2c, 0x5B, &data);
    pr_err("%s: %s0x5B:0x%02x\n", __func__, str, data);

	memset(str,0,strlen(str));

	for (i = 0x5C; i <= 0x62; i++) {
		s2mu106_read_reg(i2c, i, &data);
		sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}

    s2mu106_read_reg(i2c, 0x63, &data);
    pr_err("%s: %s0x63:0x%02x\n", __func__, str, data);
}
#endif

static int s2mu106_fled_get_flash_curr(struct s2mu106_fled_data *fled, int chan)
{
	int curr = -1;
	u8 data;
	u8 dest;

	if ((chan < 1) || (chan > S2MU106_CH_MAX)) {
		pr_info("%s: Wrong channel!!\n", __func__);
		return -1;
	}

	switch(chan) {
		case 1:
			dest = S2MU106_FLED_CH1_CTRL0;
			break;
		case 2:
			dest = S2MU106_FLED_CH2_CTRL0;
			break;
		case 3:
			dest = S2MU106_FLED_CH3_CTRL0;
			break;
		default:
			return curr;
			break;
	}

	s2mu106_read_reg(fled->i2c, dest, &data);

	data = data & S2MU106_CHX_FLASH_IOUT;
	curr = (data * 50) + 50;

	pr_info("%s: CH%02d flash curr. = %dmA\n", __func__,
		chan, curr);

	return curr;
}

static int s2mu106_fled_set_flash_curr(struct s2mu106_fled_data *fled,
	int chan, int curr)
{
	int ret = -1;
	u8 data;
	u8 dest;
	int curr_set;

	if ((chan < 1) || (chan > S2MU106_CH_MAX)) {
		pr_info("%s: Wrong channel!!\n", __func__);
		return -1;
	}

	switch(chan) {
		case 1:
			dest = S2MU106_FLED_CH1_CTRL0;
			break;
		case 2:
			dest = S2MU106_FLED_CH2_CTRL0;
			break;
		case 3:
			dest = S2MU106_FLED_CH3_CTRL0;
			break;
		default:
			return ret;
			break;
	}

	if (curr < 50)
		curr = 50;
	else if (curr > s2mu106_fled_flash_curr_max[chan])
		curr = s2mu106_fled_flash_curr_max[chan];

	data = (curr - 50)/50;

	s2mu106_update_reg(fled->i2c, dest, data, S2MU106_CHX_FLASH_IOUT);

	curr_set = s2mu106_fled_get_flash_curr(fled, chan);

	pr_info("%s: curr: %d, curr_set: %d\n", __func__,
		curr, curr_set);

	return ret;
}

static int s2mu106_fled_get_torch_curr(struct s2mu106_fled_data *fled,
	int chan)
{
	int curr = -1;
	u8 data;
	u8 dest;

	if ((chan < 1) || (chan > S2MU106_CH_MAX)) {
		pr_info("%s: Wrong channel!!\n", __func__);
		return -1;
	}

	switch(chan) {
		case 1:
			dest = S2MU106_FLED_CH1_CTRL1;
			break;
		case 2:
			dest = S2MU106_FLED_CH2_CTRL1;
			break;
		case 3:
			dest = S2MU106_FLED_CH3_CTRL1;
			break;
		default:
			return curr;
			break;
	}

	s2mu106_read_reg(fled->i2c, dest, &data);

	data = data & S2MU106_CHX_TORCH_IOUT;
	curr = data * 10 + 10;

	pr_info("%s: CH%02d torch curr. = %dmA\n", __func__,
		chan, curr);

	return curr;
}

static int s2mu106_fled_set_torch_curr(struct s2mu106_fled_data *fled,
	int chan, int curr)
{
	int ret = -1;
	u8 data;
	u8 dest;
	int curr_set;

	if ((chan < 1) || (chan > S2MU106_CH_MAX)) {
		pr_info("%s: Wrong channel!!\n", __func__);
		return -1;
	}

	switch(chan) {
		case 1:
			dest = S2MU106_FLED_CH1_CTRL1;
			break;
		case 2:
			dest = S2MU106_FLED_CH2_CTRL1;
			break;
		case 3:
			dest = S2MU106_FLED_CH3_CTRL1;
			break;
		default:
			return ret;
			break;
	}

	if (curr < 10)
		curr = 10;
	else if (curr > s2mu106_fled_torch_curr_max[chan])
		curr = s2mu106_fled_torch_curr_max[chan];

	data = (curr - 10)/10;

	s2mu106_update_reg(fled->i2c, dest, data, S2MU106_CHX_TORCH_IOUT);

	curr_set = s2mu106_fled_get_torch_curr(fled, chan);

	pr_info("%s: curr: %d, curr_set: %d\n", __func__,
		curr, curr_set);

	ret = 0;

	return ret;

}

static void s2mu106_fled_operating_mode(struct s2mu106_fled_data *fled,
					int mode)
{
	u8 value;

	if (mode < 0 || mode > 3) {
		pr_info ("%s, wrong mode\n", __func__);
		mode = AUTO_MODE_LED;
	}

	pr_info ("%s = %s\n", __func__,
		 s2mu106_fled_operating_mode_string[mode]);

	value = mode << 6;
	s2mu106_update_reg(fled->i2c, S2MU106_FLED_CTRL0, value, 0xC0);
}

static int s2mu106_fled_get_mode(struct s2mu106_fled_data *fled, int chan)
{
	u8 status;
	int ret = -1;

	s2mu106_read_reg(fled->i2c, S2MU106_FLED_STATUS1, &status);

	pr_info("%s: S2MU106_FLED_STATUS1: 0x%02x\n", __func__, status);

	if ((chan < 1) || (chan > S2MU106_CH_MAX)) {
		pr_info("%s: Wrong channel!!\n", __func__);
		return -1;
	}

	switch(chan) {
		case 1:
			if (status & S2MU106_CH1_FLASH_ON)
				ret = S2MU106_FLED_MODE_FLASH;
			else if (status & S2MU106_CH1_TORCH_ON)
				ret = S2MU106_FLED_MODE_TORCH;
			else
				ret = S2MU106_FLED_MODE_OFF;
			break;
		case 2:
			if (status & S2MU106_CH2_FLASH_ON)
				ret = S2MU106_FLED_MODE_FLASH;
			else if (status & S2MU106_CH2_TORCH_ON)
				ret = S2MU106_FLED_MODE_TORCH;
			else
				ret = S2MU106_FLED_MODE_OFF;
			break;
		case 3:
			if (status & S2MU106_CH3_FLASH_ON)
				ret = S2MU106_FLED_MODE_FLASH;
			else if (status & S2MU106_CH3_TORCH_ON)
				ret = S2MU106_FLED_MODE_TORCH;
			else
				ret = S2MU106_FLED_MODE_OFF;
			break;
		default:
			break;
	}
	return ret;
}

static int s2mu106_fled_set_mode(struct s2mu106_fled_data *fled,
								int chan, int mode)
{
	u8 dest = 0, bit = 0, mask = 0, status = 0;

	if ((chan <= 0) || (chan > S2MU106_CH_MAX) ||
		(mode < 0) || (mode > S2MU106_FLED_MODE_MAX)) {
			pr_err("%s: Wrong channel or mode.\n", __func__);
			return -EFAULT;
	}

	pr_err("%s: channel: %d, mode: %d\n", __func__, chan, mode);

	/* 0b000: OFF, 0b101: i2c bit control(on) */
	switch(mode) {
		case S2MU106_FLED_MODE_OFF:
			mask = S2MU106_CHX_FLASH_FLED_EN |
					S2MU106_CHX_TORCH_FLED_EN;
			bit = 0;
			break;
		case S2MU106_FLED_MODE_FLASH:
			mask = S2MU106_CHX_FLASH_FLED_EN;
			bit = S2MU106_FLED_EN << 3;
			break;
		case S2MU106_FLED_MODE_TORCH:
			s2mu106_fled_operating_mode(fled, SYS_MODE_LED);
			mask = S2MU106_CHX_TORCH_FLED_EN;
			bit = S2MU106_FLED_EN;
			break;
		default:
			return -EFAULT;
			break;
	}

	switch(chan) {
		case 1:
			dest = S2MU106_FLED_CTRL1;
			break;
		case 2:
			dest = S2MU106_FLED_CTRL2;
			break;
		case 3:
			dest = S2MU106_FLED_CTRL3;
			break;
		default:
			return -EFAULT;
			break;
	}

	/* Need to set EN_FLED_PRE bit before mode change */
	if (mode != S2MU106_FLED_MODE_OFF)
		s2mu106_update_reg(fled->i2c, S2MU106_FLED_CTRL0,
			S2MU106_EN_FLED_PRE, S2MU106_EN_FLED_PRE);
	else {
		/* If no LED is on, clear EN_FLED_PRE */
		s2mu106_read_reg(fled->i2c, S2MU106_FLED_STATUS1, &status);
		if (!(status & S2MU106_FLED_ON_CHECK))
			s2mu106_update_reg(fled->i2c, S2MU106_FLED_CTRL0,
					0, S2MU106_EN_FLED_PRE);
	}
	s2mu106_update_reg(fled->i2c, dest, bit, mask);

	if (mode == S2MU106_FLED_MODE_OFF)
		s2mu106_fled_operating_mode(fled, AUTO_MODE_LED);

	return 0;
}

int s2mu106_mode_change_cam_to_leds(enum cam_flashtorch_mode cam_mode)
{
	int mode = -1;

	switch(cam_mode) {
		case CAM_FLASH_MODE_OFF:
			mode = S2MU106_FLED_MODE_OFF;
			break;
		case CAM_FLASH_MODE_SINGLE:
		case CAMERA_FLASH_OPP_TORCHRECORD:	
			mode = S2MU106_FLED_MODE_TORCH;
			break;
		case CAM_FLASH_MODE_TORCH:
			mode = S2MU106_FLED_MODE_FLASH;
			break;
		default:
			mode = S2MU106_FLED_MODE_OFF;
			break;
	}

	return mode;
}

int s2mu106_fled_set_mode_ctrl(int chan, enum cam_flashtorch_mode cam_mode)
{
	struct s2mu106_fled_data *fled = f_flash_data;
	int mode = -1;
	pr_err("%s: channel: %d, mode: %d\n", __func__, chan, mode);

	mode = s2mu106_mode_change_cam_to_leds(cam_mode);

	if ((chan <= 0) || (chan > S2MU106_CH_MAX) ||
		(mode < 0) || (mode >= S2MU106_FLED_MODE_MAX)) {
			pr_err("%s: channel: %d, mode: %d\n", __func__, chan, mode);
			pr_err("%s: Wrong channel or mode.\n", __func__);
			return -1;
	}

	s2mu106_fled_set_mode(fled, chan, mode);
#if DEBUG_TEST_READ
	s2mu106_fled_test_read(fled);
#endif

	return 0;
}

int s2mu106_fled_set_curr(int chan, enum cam_flashtorch_mode cam_mode, int curr)
{
	struct s2mu106_fled_data *fled = f_flash_data;
	int mode = -1;

	mode = s2mu106_mode_change_cam_to_leds(cam_mode);

	/* Check channel */
	if ((chan <= 0) || (chan > S2MU106_CH_MAX)) {
			pr_err("%s: Wrong channel.\n", __func__);
			return -EFAULT;
	}

	switch (mode){
		case S2MU106_FLED_MODE_TORCH:
			/* Set curr. */
			s2mu106_fled_set_torch_curr(fled, chan, curr);
			break;
		case S2MU106_FLED_MODE_FLASH:
			/* Set curr. */
			s2mu106_fled_set_flash_curr(fled, chan, curr);
			break;
		default:
			return -1;
	}
	/* Test read */
#if DEBUG_TEST_READ
	s2mu106_fled_test_read(fled);
#endif

	return 0;
}

int s2mu106_fled_get_curr(int chan, enum cam_flashtorch_mode cam_mode)
{
	struct s2mu106_fled_data *fled = f_flash_data;
	int mode = -1;
	int curr = 0;

	mode = s2mu106_mode_change_cam_to_leds(cam_mode);

	/* Check channel */
	if ((chan <= 0) || (chan > S2MU106_CH_MAX)) {
			pr_err("%s: Wrong channel.\n", __func__);
			return -EFAULT;
	}

	switch (mode){
		case S2MU106_FLED_MODE_TORCH:
			curr = s2mu106_fled_get_torch_curr(fled, chan);
			break;
		case S2MU106_FLED_MODE_FLASH:
			curr = s2mu106_fled_get_flash_curr(fled, chan);
			break;
		default:
			return -1;
	}
	/* Test read */
#if DEBUG_TEST_READ
	s2mu106_fled_test_read(fled);
#endif

	return curr;
}

static ssize_t fled_flash_curr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct s2mu106_fled_data *fled =
		container_of(led_cdev, struct s2mu106_fled_data, cdev);
	int cnt = 0;
	int curr = 0;
	int i;
    char str[1016] = {0,};

	/* Read curr. */
	for (i = 1; i <= S2MU106_CH_MAX; i++) {
		curr = s2mu106_fled_get_flash_curr(fled, i);
		pr_info("%s: channel: %d, curr: %dmA\n", __func__, i, curr);
		if (curr >= 0)
			cnt += sprintf(str+strlen(str), "CH%02d: %dmA, ", i, curr);
	}

	cnt += sprintf(str+strlen(str), "\n");

	strcpy(buf, str);

    return cnt;
}

static ssize_t fled_flash_curr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct s2mu106_fled_data *fled =
		container_of(led_cdev, struct s2mu106_fled_data, cdev);
	int chan = -1;
	int curr = -1;

	sscanf(buf, "%d %d", &chan, &curr);

	/* Check channel */
	if ((chan <= 0) || (chan > S2MU106_CH_MAX)) {
			pr_err("%s: Wrong channel.\n", __func__);
			return -EFAULT;
	}

	/* Set curr. */
	s2mu106_fled_set_flash_curr(fled, chan, curr);

	/* Test read */
#if DEBUG_TEST_READ
	s2mu106_fled_test_read(fled);
#endif

	return size;
}


static ssize_t fled_torch_curr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct s2mu106_fled_data *fled =
		container_of(led_cdev, struct s2mu106_fled_data, cdev);
	int cnt = 0;
	int curr = 0;
	int i;
    char str[1016] = {0,};

	/* Read curr. */
	for (i = 1; i <= S2MU106_CH_MAX; i++) {
		curr = s2mu106_fled_get_torch_curr(fled, i);
		pr_info("%s: channel: %d, curr: %dmA\n", __func__, i, curr);
		if (curr >= 0)
			cnt += sprintf(str+strlen(str), "CH%02d: %dmA, ", i, curr);
	}

	cnt += sprintf(str+strlen(str), "\n");

	strcpy(buf, str);

    return cnt;
}

static ssize_t fled_torch_curr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct s2mu106_fled_data *fled =
		container_of(led_cdev, struct s2mu106_fled_data, cdev);
	int chan = -1;
	int curr = -1;

	sscanf(buf, "%d %d", &chan, &curr);

	/* Check channel */
	if ((chan <= 0) || (chan > S2MU106_CH_MAX)) {
			pr_err("%s: Wrong channel.\n", __func__);
			return -EFAULT;
	}

	/* Set curr. */
	s2mu106_fled_set_torch_curr(fled, chan, curr);

	/* Test read */
#if DEBUG_TEST_READ
	s2mu106_fled_test_read(fled);
#endif

	return size;
}

static ssize_t fled_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct s2mu106_fled_data *fled =
		container_of(led_cdev, struct s2mu106_fled_data, cdev);
	int cnt = 0;
	int mode = 0;
	int i;
    char str[1016] = {0,};

#if DEBUG_TEST_READ
	s2mu106_fled_test_read(fled);
#endif

	for (i = 1; i <= S2MU106_CH_MAX; i++) {
		mode = s2mu106_fled_get_mode(fled, i);
		if (mode >= 0)
			cnt += sprintf(str+strlen(str), "CH%02d: %s, ", i,
				s2mu106_fled_mode_string[mode]);
	}

	cnt += sprintf(str+strlen(str), "\n");

	strcpy(buf, str);

    return cnt;
}

static ssize_t fled_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct s2mu106_fled_data *fled =
		container_of(led_cdev, struct s2mu106_fled_data, cdev);
	int chan = -1;
	int mode = -1;

	sscanf(buf, "%d %d", &chan, &mode);

	if ((chan <= 0) || (chan > S2MU106_CH_MAX) ||
		(mode < 0) || (mode >= S2MU106_FLED_MODE_MAX)) {
			pr_err("%s: channel: %d, mode: %d\n", __func__, chan, mode);
			pr_err("%s: Wrong channel or mode.\n", __func__);
			return -EFAULT;
	}

	s2mu106_fled_set_mode(fled, chan, mode);
#if DEBUG_TEST_READ
	s2mu106_fled_test_read(fled);
#endif

	return size;
}

static DEVICE_ATTR(fled_mode, 0644, fled_mode_show, fled_mode_store);
static DEVICE_ATTR(fled_flash_curr, 0644, fled_flash_curr_show, fled_flash_curr_store);
static DEVICE_ATTR(fled_torch_curr, 0644, fled_torch_curr_show, fled_torch_curr_store);

static struct attribute *s2mu106_fled_attrs[] = {
	&dev_attr_fled_mode.attr,
	&dev_attr_fled_flash_curr.attr,
	&dev_attr_fled_torch_curr.attr,
    NULL
};
ATTRIBUTE_GROUPS(s2mu106_fled);

static void s2mu106_set_interface(struct s2mu106_fled_data *fled)
{
	int flash_gpio = fled->pdata->flash_gpio;
	int torch_gpio = fled->pdata->torch_gpio;

	if (gpio_is_valid(flash_gpio) && gpio_is_valid(torch_gpio)) {
		pr_info("%s: s2mu106_fled gpio mode\n", __func__);

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

		/* FLED off */
		s2mu106_fled_set_mode(fled, 1, S2MU106_FLED_MODE_OFF);

		/* w/a: prevent SMPL event in case of flash operation */
		s2mu106_update_reg(fled->i2c, 0x21, 0x4, 0x7);
		s2mu106_update_reg(fled->i2c, 0x89, 0x0, 0x3);
	} else
		fled->control_mode = CONTROL_I2C;
}

#if defined(CONFIG_OF)
static int s2mu106_led_dt_parse_pdata(struct device *dev,
				struct s2mu106_fled_platform_data *pdata)
{
	struct device_node *led_np, *np, *c_np;
	int ret;
	u32 temp;
	u32 index;

	led_np = dev->parent->of_node;

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

	ret = of_property_read_u32(np, "default_current",
			&pdata->default_current);
	if (ret < 0)
		pr_err("%s : could not find default_current\n", __func__);

	ret = of_property_read_u32(np, "max_current",
			&pdata->max_current);
	if (ret < 0)
		pr_err("%s : could not find max_current\n", __func__);

	ret = of_property_read_u32(np, "default_timer",
			&pdata->default_timer);
	if (ret < 0)
		pr_err("%s : could not find default_timer\n", __func__);


#if FLED_EN
	ret = pdata->fled-en1-pin = of_get_named_gpio(np, "fled-en1-gpio", 0);
	if (ret < 0) {
		pr_err("%s : can't get fled-en1-pin\n", __func__);
		return ret;
	}

	ret = pdata->fled-en2-pin = of_get_named_gpio(np, "fled-en2-gpio", 0);
	if (ret < 0) {
		pr_err("%s : can't get fled-en2-pin\n", __func__);
		return ret;
	}

	ret = pdata->fled-en3-pin = of_get_named_gpio(np, "fled-en3-gpio", 0);
	if (ret < 0) {
		pr_err("%s : can't get fled-en3-pin\n", __func__);
		return ret;
	}

	ret = pdata->fled-en4-pin = of_get_named_gpio(np, "fled-en4-gpio", 0);
	if (ret < 0) {
		pr_err("%s : can't get fled-en4-pin\n", __func__);
		return ret;
	}
#endif

	pdata->chan_num = of_get_child_count(np);

	if (pdata->chan_num > S2MU106_CH_MAX)
		pdata->chan_num = S2MU106_CH_MAX;

	pdata->channel = devm_kzalloc(dev,
		sizeof(struct s2mu106_fled_chan) * pdata->chan_num, GFP_KERNEL);

	for_each_child_of_node(np, c_np) {
		ret = of_property_read_u32(c_np, "id", &temp);
		if (ret < 0)
			goto dt_err;
		index = temp;

		pr_info("%s: temp = %d, index = %d\n",
			__func__, temp, index);

		if (index < S2MU106_CH_MAX) {
			pdata->channel[index].id = index;

			ret = of_property_read_u32_index(np, "current", index,
					&pdata->channel[index].curr);
			if (ret < 0) {
				pr_err("%s : could not find current for channel%d\n",
					__func__, pdata->channel[index].id);
				pdata->channel[index].curr = pdata->default_current;
			}

			ret = of_property_read_u32_index(np, "timer", index,
					&pdata->channel[index].timer);
			if (ret < 0) {
				pr_err("%s : could not find timer for channel%d\n",
					__func__, pdata->channel[index].id);
				pdata->channel[index].timer = pdata->default_timer;
			}
		}
	}

	ret = of_property_read_u32(np, "factory_current",
				   &pdata->factory_current);
	if (ret < 0)
		pr_err("%s : could not find factory_current\n", __func__);

	ret = of_property_read_u32_array(np, "flashlight_current",
					 pdata->flashlight_current, 5);
	if (ret < 0) {
		pr_err("%s : could not find flashlight_current\n", __func__);

		//default setting
		pdata->flashlight_current[0] = 45;
		pdata->flashlight_current[1] = 75;
		pdata->flashlight_current[2] = 125;
		pdata->flashlight_current[3] = 195;
		pdata->flashlight_current[4] = 270;
	}
	
	return 0;
dt_err:
	pr_err("%s: DT parsing finish. ret = %d\n", __func__, ret);
	return ret;
}
#endif /* CONFIG_OF */

#define CONFIG_S2MU106_FLASH_LED_SYSFS 1
#if defined(CONFIG_S2MU106_FLASH_LED_SYSFS)
extern struct class *camera_class;
struct device *fled_dev;
extern struct device *cam_dev_flash;

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

	if(NULL == f_flash_data) {
		pr_err("%s: s2mu106 flash device is NULL, return\n", __func__);
		return -1;
	}
	f_flash_data->sysfs_input_data = value;

	flash_current = f_flash_data->pdata->flash_current;
	torch_current = f_flash_data->pdata->torch_current;

	if (value <= 0) {
		mode = S2MU106_FLED_MODE_OFF;
	} else if (value == 1) {
		mode = S2MU106_FLED_MODE_TORCH;
	} else if (value == 100) {
		/* Factory Torch*/
		pr_info("%s: factory torch current [%d]\n",
			__func__, f_flash_data->pdata->factory_current);
		torch_current = f_flash_data->pdata->factory_current;
		mode = S2MU106_FLED_MODE_TORCH;
	} else if (value == 200) {
		/* Factory Flash */
		pr_info("%s: factory flash current [%d]\n",
			__func__, f_flash_data->pdata->factory_current);
		flash_current = f_flash_data->pdata->factory_current;
		mode = S2MU106_FLED_MODE_FLASH;
	} else if (value <= 1010 && value >= 1001) {
		mode = S2MU106_FLED_MODE_TORCH;
		/* (value) 1001, 1002, 1004, 1006, 1009 */
		if (value <= 1001)
			torch_current = f_flash_data->pdata->flashlight_current[0];
		else if (value <= 1002)
			torch_current = f_flash_data->pdata->flashlight_current[1];
		else if (value <= 1004)
			torch_current = f_flash_data->pdata->flashlight_current[2];
		else if (value <= 1006)
			torch_current = f_flash_data->pdata->flashlight_current[3];
		else if (value <= 1009)
			torch_current = f_flash_data->pdata->flashlight_current[4];
		else
			torch_current = f_flash_data->pdata->torch_current;
		f_flash_data->sysfs_input_data = 1;
	} else if (value == 2) {
		mode = S2MU106_FLED_MODE_FLASH;
	}

	mutex_lock(&f_flash_data->lock);
	if (f_flash_data->control_mode == CONTROL_I2C) {
		s2mu106_fled_set_mode(f_flash_data, 1, mode);
	} else {
		if (mode == S2MU106_FLED_MODE_TORCH) {
			pr_info("%s: %d: S2MU106_FLED_MODE_TORCH - %dmA\n",
				__func__, value, torch_current);
			/* torch current set */
			s2mu106_fled_set_torch_curr(f_flash_data, 1,
						    torch_current);
			s2mu106_fled_set_mode(f_flash_data, 1, S2MU106_FLED_MODE_TORCH);
		} else if (mode == S2MU106_FLED_MODE_FLASH) {
			pr_info("%s: %d: S2MU107_FLED_MODE_FLASH - %dmA\n",
				__func__, value, flash_current);
			/* flash current set */
			s2mu106_fled_set_flash_curr(f_flash_data, 1,
						    flash_current);
			s2mu106_fled_set_mode(f_flash_data, 1, S2MU106_FLED_MODE_FLASH);
		} else {
			pr_info("%s: %d: S2MU106_FLED_MODE_OFF\n",
				__func__, value);

			/* flash, torch current set for initial current */
			flash_current = f_flash_data->pdata->flash_current;
			torch_current = f_flash_data->pdata->torch_current;

			/* flash current set */
			s2mu106_fled_set_flash_curr(f_flash_data, 1,
						    flash_current);
			/* torch current set */
			s2mu106_fled_set_torch_curr(f_flash_data, 1,
						    torch_current);
			s2mu106_fled_set_mode(f_flash_data, 1, S2MU106_FLED_MODE_OFF);
		}
	}

	mutex_unlock(&f_flash_data->lock);
	//s2mu106_fled_test_read(f_flash_data);
	pr_info("%s: rear_flash_store END\n", __func__);
	return size;
}

static ssize_t rear_flash_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", f_flash_data->sysfs_input_data);
}


static DEVICE_ATTR(rear_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
		   rear_flash_show, rear_flash_store);
static DEVICE_ATTR(rear_torch_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
		   rear_flash_show, rear_flash_store);

int create_flash_sys(void)
{
	int err = -ENODEV;

	if (IS_ERR_OR_NULL(camera_class)) {
		pr_err("flash_sysfs: error, camera class not exist");
		return -ENODEV;
	}
	if (!IS_ERR_OR_NULL(cam_dev_flash)) {
		device_remove_file(cam_dev_flash, &dev_attr_rear_flash);
		device_remove_file(cam_dev_flash, &dev_attr_rear_torch_flash);
		fled_dev = cam_dev_flash;
	} else {
		fled_dev = device_create(camera_class, NULL, 0, NULL, "flash");
		if (IS_ERR(fled_dev)) {
			pr_err("flash_sysfs: failed to create device(flash)\n");
			return -ENODEV;
		}
	}

	err = device_create_file(fled_dev, &dev_attr_rear_flash);
	if (unlikely(err < 0)) {
		pr_err("flash_sysfs: failed to create device file, %s\n",
			dev_attr_rear_flash.attr.name);
	}

	err = device_create_file(fled_dev, &dev_attr_rear_torch_flash);
	if (unlikely(err < 0)) {
		pr_err("flash_sysfs: failed to create device file, %s\n",
			dev_attr_rear_torch_flash.attr.name);
	}

	return 0;
}
#endif

int ext_pmic_cam_fled_ctrl(int cam_mode, int curr)
{
	int flash_current = 0, torch_current = 0;
	int mode = -1;

	if(NULL == f_flash_data) {
		pr_err("%s: s2mu106 flash device is NULL, return\n", __func__);
		return 0;
	}
	mutex_lock(&f_flash_data->lock);
	mode = s2mu106_mode_change_cam_to_leds(cam_mode);
	pr_info("%s: ext_pmic_cam_fled_ctrl cam_mode=%d curr=%d mode=%d\n", __func__, cam_mode, curr, mode);

	if(curr > 0) {
		flash_current = curr;
		torch_current = curr;
	} else {
		flash_current = f_flash_data->pdata->flash_current;
		torch_current = f_flash_data->pdata->torch_current;
	}
	pr_info("%s: S2MU106_FLED control_mode - %d \n", __func__, f_flash_data->control_mode);
	if (f_flash_data->control_mode == CONTROL_I2C) {
		s2mu106_fled_set_mode(f_flash_data, 1, mode);
	} else {
		if (mode == S2MU106_FLED_MODE_TORCH) {
			pr_info("%s: S2MU106_FLED_MODE_TORCH - %dmA\n",
				__func__, torch_current);
			/* torch current set */
			s2mu106_fled_set_torch_curr(f_flash_data, 1,
						    torch_current);
			s2mu106_fled_set_mode(f_flash_data, 1, S2MU106_FLED_MODE_TORCH);
		} else if (mode == S2MU106_FLED_MODE_FLASH) {
			pr_info("%s: S2MU106_FLED_MODE_FLASH - %dmA\n",
				__func__, flash_current);
			/* flash current set */
			s2mu106_fled_set_flash_curr(f_flash_data, 1,
						    flash_current);
			s2mu106_fled_set_mode(f_flash_data, 1, S2MU106_FLED_MODE_FLASH);
		} else {
			pr_info("%s: S2MU106_FLED_MODE_OFF\n",
				__func__);

			/* flash, torch current set for initial current */
			flash_current = 0;
			torch_current = 0;

			/* flash current set */
			s2mu106_fled_set_flash_curr(f_flash_data, 1,
						    flash_current);
			/* torch current set */
			s2mu106_fled_set_torch_curr(f_flash_data, 1,
						    torch_current);
			s2mu106_fled_set_mode(f_flash_data, 1, S2MU106_FLED_MODE_OFF);
		}
	}

	mutex_unlock(&f_flash_data->lock);
	//s2mu106_fled_test_read(f_flash_data);
	pr_info("%s: ext_pmic_cam_fled_ctrl END\n", __func__);
	return 0;
}
static int s2mu106_led_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct s2mu106_dev *s2mu106 = dev_get_drvdata(pdev->dev.parent);
	struct s2mu106_fled_data *fled_data;
	char name[20];

	pr_info("%s: s2mu106_fled start\n", __func__);

	if (!s2mu106) {
		dev_err(&pdev->dev, "drvdata->dev.parent not supplied\n");
		return -ENODEV;
	}

	fled_data = devm_kzalloc(&pdev->dev,
		sizeof(struct s2mu106_fled_data), GFP_KERNEL);
	if (!fled_data) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		return -ENOMEM;
	}

	fled_data->dev = &pdev->dev;
	fled_data->i2c = s2mu106->i2c;
	fled_data->pdata = devm_kzalloc(&pdev->dev,
		sizeof(*(fled_data->pdata)), GFP_KERNEL);
	if (!fled_data->pdata) {
		pr_err("%s: failed to allocate platform data\n", __func__);
		return -ENOMEM;
	}

	if (s2mu106->dev->of_node) {
		ret = s2mu106_led_dt_parse_pdata(&pdev->dev, fled_data->pdata);
		if (ret < 0) {
			pr_err("%s: not found leds dt! ret=%d\n",
				__func__, ret);
			return -1;
		}
	}

	platform_set_drvdata(pdev, fled_data);

	/* Store fled_data for EXPORT_SYMBOL */
	f_flash_data = fled_data;

	snprintf(name, sizeof(name), "fled-s2mu106");
	fled_data->cdev.name = name;
	fled_data->cdev.groups = s2mu106_fled_groups;

	f_flash_data->flash_gpio		= fled_data->pdata->flash_gpio;
	f_flash_data->torch_gpio		= fled_data->pdata->torch_gpio;
	s2mu106_set_interface(fled_data);

	ret = devm_led_classdev_register(&pdev->dev, &fled_data->cdev);
	if (ret < 0) {
		pr_err("%s: unable to register LED class dev\n", __func__);
		return ret;
	}

#if defined(CONFIG_S2MU106_FLASH_LED_SYSFS)
	/* create sysfs for camera */
	create_flash_sys();
#endif

	pr_info("%s: s2mu106_fled loaded\n", __func__);
	return 0;
}

static int s2mu106_led_remove(struct platform_device *pdev)
{
	return 0;
}

static void s2mu106_led_shutdown(struct platform_device *pdev)
{
	struct s2mu106_fled_data *fled_data =
		platform_get_drvdata(pdev);
	int chan;

	if (!fled_data->i2c) {
		pr_err("%s: no i2c client\n", __func__);
		return;
	}

	/* Turn off all leds when power off */
	pr_info("%s: turn off all leds\n", __func__);
	for (chan = 1; chan <= S2MU106_CH_MAX; chan++)
		s2mu106_fled_set_mode(fled_data, chan, S2MU106_FLED_MODE_OFF);
}

#if defined(CONFIG_OF)
static const struct of_device_id cam_flash_dt_match[] = {
	{.compatible = "qcom,s2mu106-fled", .data = NULL},
	{}
};
MODULE_DEVICE_TABLE(of, cam_flash_dt_match);
#endif

static struct platform_driver s2mu106_led_driver = {
	.driver = {
		.name  = "leds-s2mu106",
		.owner = THIS_MODULE,
		},
	.probe  = s2mu106_led_probe,
	.remove = s2mu106_led_remove,
	.shutdown = s2mu106_led_shutdown,
};

static int __init s2mu106_led_driver_init(void)
{
	return platform_driver_register(&s2mu106_led_driver);
}
module_init(s2mu106_led_driver_init);

static void __exit s2mu106_led_driver_exit(void)
{
	platform_driver_unregister(&s2mu106_led_driver);
}
module_exit(s2mu106_led_driver_exit);

MODULE_AUTHOR("Li Guanhai <guanhai.li@samsung.com>");
MODULE_DESCRIPTION("SAMSUNG s2mu106 flash LED Driver");
MODULE_LICENSE("GPL");

