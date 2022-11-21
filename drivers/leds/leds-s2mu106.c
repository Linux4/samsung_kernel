/*
 * leds-s2mu106.c - LED class driver for S2MU106 FLASH LEDs.
 *
 * Copyright (C) 2020 Samsung Electronics
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
#include <linux/platform_device.h>
#include <linux/sec_batt.h>
#include <linux/power_supply.h>
#include <linux/leds-s2mu106.h>
#include <linux/mfd/samsung/s2mu106.h>

/* FLED operating mode enable */
enum operating_mode {
	AUTO_MODE = 0,
	BOOST_MODE,
	TA_MODE,
	SYS_MODE,
};

enum cam_flash_mode{
	CAMERA_FLASH_OP_INVALID,
	CAMERA_FLASH_OP_OFF,
	CAMERA_FLASH_OP_FIRELOW,
	CAMERA_FLASH_OP_FIREHIGH,
	CAMERA_FLASH_OP_FIRERECORD,
	CAMERA_FLASH_OP_MAX,
};

int s2mu106_fled_set_mode_ctrl(int chan, enum cam_flash_mode cam_mode);
int s2mu106_fled_set_curr(int chan, enum cam_flash_mode cam_mode, int curr);
int s2mu106_fled_get_curr(int chan, enum cam_flash_mode cam_mode);

#define CONTROL_I2C	0
#define CONTROL_GPIO	1

extern struct class *camera_class;
extern struct device *cam_dev_flash;
extern int factory_mode;
static struct s2mu106_fled_data *g_fled_data = NULL;

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

/*Channel values range fron 1 to 3 so 0 value is never obtained check function s2mu106_fled_set_torch_curr*/
static int s2mu106_fled_torch_curr_max[] = {
	-1, 320, 320, 320
};

/*Channel values range fron 1 to 3 so 0 value is never obtained check function s2mu106_fled_set_flash_curr*/
static int s2mu106_fled_flash_curr_max[] = {
	-1, 1600, 1600, 500
};

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
	sprintf(str+strlen(str), "0x63:0x%02x, ", data);

	s2mu106_read_reg(i2c, 0x66, &data);
	sprintf(str+strlen(str), "0x66:0x%02x, ", data);

	s2mu106_read_reg(i2c, 0x67, &data);
	pr_err("%s: %s0x67:0x%02x\n", __func__, str, data);

}

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

static void s2mu106_fled_operating_mode(struct s2mu106_fled_data *fled, int mode)
{
	u8 value;

	if (fled->set_on_factory) {
		pr_info("%s Factory Status, Return\n", __func__);
		return;
	}

	if (mode < 0 || mode > 3) {
		pr_info ("%s, wrong mode\n", __func__);
		mode = AUTO_MODE;
	}

	pr_info ("%s = %s\n", __func__, s2mu106_fled_operating_mode_string[mode]);

	value = mode << 6;
	s2mu106_update_reg(fled->i2c, S2MU106_FLED_CTRL0, value, 0xC0);

	/*P200827-05556 - change PMIC Boost switching freq from 1.4Mhz (101) to 1.75Mhz (111)*/
	s2mu106_update_reg(fled->i2c, 0x99, 0x07, 0x0E);
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
			if (fled->control_mode == CONTROL_I2C)
				bit = S2MU106_FLED_EN << 3;
			else
				bit = S2MU106_FLED_GPIO_EN1 << 3;

			break;
		case S2MU106_FLED_MODE_TORCH:
			s2mu106_fled_operating_mode(fled, SYS_MODE);
			mask = S2MU106_CHX_TORCH_FLED_EN;
			if (fled->control_mode == CONTROL_I2C)
				bit = S2MU106_FLED_EN;
			else
				bit = S2MU106_FLED_GPIO_EN2; //It should matching with schematic

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
		s2mu106_fled_operating_mode(fled, AUTO_MODE);

	return 0;
}

int s2mu106_led_mode_ctrl(int state)
{
	struct s2mu106_fled_data *fled = g_fled_data;
	union power_supply_propval value;
	int gpio_torch = fled->torch_gpio;
	int gpio_flash = fled->flash_gpio;

	pr_info("%s : state = %d gpio=(%d %d) control_mode=%d\n", __func__, state, gpio_torch, gpio_flash, g_fled_data->control_mode);

	if (g_fled_data->control_mode == CONTROL_GPIO) {
		gpio_request(gpio_torch, "s2mu106_gpio_torch");
		gpio_request(gpio_flash, "s2mu106_gpio_flash");
	}

	switch(state) {
	case S2MU106_FLED_MODE_OFF:
		
/*		Temporary commented - Kernel panic when boot(pdo_ctrl_by_flash) */
/* 		pdo_ctrl_by_flash(0);
		muic_afc_set_voltage(9);
		pr_info("[%s]%d Down Volatge set Clear \n" ,__func__,__LINE__);*/

		if (g_fled_data->control_mode == CONTROL_I2C) {
			s2mu106_fled_set_mode(g_fled_data, 1, state);
		} else { 
			gpio_direction_output(gpio_torch, 0);
			gpio_direction_output(gpio_flash, 0);
			if (fled->is_en_flash) {
				if (!fled->psy_chg)
					fled->psy_chg = power_supply_get_by_name("s2mu106-charger");
				fled->is_en_flash = value.intval = false;
				power_supply_set_property(fled->psy_chg,
					POWER_SUPPLY_PROP_ENERGY_AVG, &value);
			}
			s2mu106_fled_operating_mode(fled, SYS_MODE);
		}
		break;
	case S2MU106_FLED_MODE_TORCH:
/*		muic_afc_set_voltage(5);
		pdo_ctrl_by_flash(1);
		pr_info("[%s]%d Down Volatge set On \n" ,__func__,__LINE__);*/

		if (g_fled_data->control_mode == CONTROL_I2C) {
			s2mu106_fled_set_mode(g_fled_data, 1, state);
		} else { 
			/* chgange SYS MODE when turn on torch */
			s2mu106_fled_operating_mode(fled, SYS_MODE);
			gpio_direction_output(gpio_torch, 1);
		}
		break;
	case S2MU106_FLED_MODE_FLASH:
		if (!fled->psy_chg)
			fled->psy_chg = power_supply_get_by_name("s2mu106-charger");

		fled->is_en_flash = value.intval = true;
		power_supply_set_property(fled->psy_chg,
				POWER_SUPPLY_PROP_ENERGY_AVG, &value);

		s2mu106_fled_operating_mode(fled, AUTO_MODE);

		if (g_fled_data->control_mode == CONTROL_I2C) {
			s2mu106_fled_set_mode(g_fled_data, 1, state);
		} else { 
			gpio_direction_output(gpio_flash, 1);
		}
		break;
	case S2MU106_FLED_MODE_MOVIE:
		if (g_fled_data->control_mode == CONTROL_I2C) {
			s2mu106_fled_set_mode(g_fled_data, 1, state);
		} else { 

			s2mu106_fled_operating_mode(fled, SYS_MODE);
			s2mu106_fled_set_torch_curr(fled, 1, fled->movie_current);
			gpio_direction_output(gpio_torch, 1);
		}
		break;
	case S2MU106_FLED_MODE_FACTORY:
		if (g_fled_data->control_mode == CONTROL_I2C) {
			s2mu106_fled_set_mode(g_fled_data, 1, state);
		} else { 

			/* chgange SYS MODE when turn on torch */
			s2mu106_fled_operating_mode(fled, SYS_MODE);
			gpio_direction_output(gpio_torch, 1);
		}
		break;
	default:
		break;
	}

	if (g_fled_data->control_mode == CONTROL_GPIO) {
		gpio_free(gpio_torch);
		gpio_free(gpio_flash);
	}

	return 0;
}

int s2mu106_mode_change_cam_to_leds(enum cam_flash_mode cam_mode)
{
	int mode = -1;

	switch (cam_mode) {
	case CAMERA_FLASH_OP_OFF:
		mode = S2MU106_FLED_MODE_OFF;
		break;
	case CAMERA_FLASH_OP_FIREHIGH:
		mode = S2MU106_FLED_MODE_FLASH;
		break;
	case CAMERA_FLASH_OP_FIRELOW:
		mode = S2MU106_FLED_MODE_TORCH;
		break;
	case CAMERA_FLASH_OP_FIRERECORD:
		mode = S2MU106_FLED_MODE_MOVIE;
		break;
	default:
		mode = S2MU106_FLED_MODE_OFF;
		break;
	}

	return mode;
}

int s2mu106_fled_set_mode_ctrl(int chan, enum cam_flash_mode cam_mode)
{
	struct s2mu106_fled_data *fled = g_fled_data;
	int mode = -1;

	mode = s2mu106_mode_change_cam_to_leds(cam_mode);

	if ((chan <= 0) || (chan > S2MU106_CH_MAX) ||
		(mode < 0) || (mode >= S2MU106_FLED_MODE_MAX)) {
			pr_err("%s: channel: %d, mode: %d\n", __func__, chan, mode);
			pr_err("%s: Wrong channel or mode.\n", __func__);
			return -1;
	}

	s2mu106_fled_set_mode(fled, chan, mode);
	s2mu106_fled_test_read(fled);

	return 0;
}

int s2mu106_fled_set_curr(int chan, enum cam_flash_mode cam_mode, int curr)
{
	struct s2mu106_fled_data *fled = g_fled_data;
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
	s2mu106_fled_test_read(fled);

	return 0;
}

int s2mu106_fled_get_curr(int chan, enum cam_flash_mode cam_mode)
{
	struct s2mu106_fled_data *fled = g_fled_data;
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
	s2mu106_fled_test_read(fled);

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
	s2mu106_fled_test_read(fled);

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
	s2mu106_fled_test_read(fled);

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

	s2mu106_fled_test_read(fled);

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

	if (fled->control_mode == CONTROL_I2C)
		s2mu106_fled_set_mode(fled, chan, mode);
	else {
		if (mode == 1)
			s2mu106_led_mode_ctrl(S2MU106_FLED_MODE_TORCH);
		else if (mode == 2)
			s2mu106_led_mode_ctrl(S2MU106_FLED_MODE_FLASH);
		else
			s2mu106_led_mode_ctrl(S2MU106_FLED_MODE_OFF);
	}

	s2mu106_fled_test_read(fled);

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

void s2mu106_fled_set_operation_mode(int mode)
{
	if(mode) {
		s2mu106_fled_operating_mode(g_fled_data, TA_MODE);
		g_fled_data->set_on_factory = 1;
		pr_info("%s: TA only mode set\n", __func__);
	}
	else {
		g_fled_data->set_on_factory = 0;
		s2mu106_fled_operating_mode(g_fled_data, AUTO_MODE);
		pr_info("%s: Auto control mode set\n", __func__);
	}
}

static void s2mu106_fled_init(struct s2mu106_fled_data *fled)
{
	int i;

	pr_info("%s: s2mu106_fled init start\n", __func__);

	fled->is_en_flash = false;
	if (gpio_is_valid(fled->pdata->flash_gpio) &&
			gpio_is_valid(fled->pdata->torch_gpio)) {
		pr_info("%s: s2mu106_fled gpio mode\n", __func__);
		fled->control_mode = CONTROL_GPIO;
		gpio_request_one(fled->flash_gpio, GPIOF_OUT_INIT_LOW, "LED_GPIO_OUTPUT_LOW");
		gpio_request_one(fled->torch_gpio, GPIOF_OUT_INIT_LOW, "LED_GPIO_OUTPUT_LOW");
		gpio_free(fled->flash_gpio);
		gpio_free(fled->torch_gpio);
		/* CAM_FLASH_EN -> FLASH gpio mode */
		s2mu106_fled_set_mode(fled, 1, S2MU106_FLED_MODE_FLASH);
		/* CAM_TORCH_EN -> TORCH gpio mode */
		s2mu106_fled_set_mode(fled, 1, S2MU106_FLED_MODE_TORCH);

		s2mu106_led_mode_ctrl(S2MU106_FLED_MODE_OFF);
	} else {
		fled->control_mode = CONTROL_I2C;
	}

	/* FLED driver operating mode set, TA only mode*/
	fled->set_on_factory = 0;
#if !defined(CONFIG_SEC_FACTORY)
	if(factory_mode) {
		s2mu106_fled_operating_mode(fled, TA_MODE);
		fled->set_on_factory = 1;
	}
#endif
	/* for Flash Auto boost */
	s2mu106_update_reg(fled->i2c, S2MU106_FLED_TEST3, 0x20, 0x20);
	s2mu106_update_reg(fled->i2c, S2MU106_FLED_TEST4, 0x40, 0x40);

	for (i = 1; i <= S2MU106_CH_MAX; i++) {
		s2mu106_fled_set_flash_curr(fled, i, fled->default_current);
		s2mu106_fled_set_torch_curr(fled, i, fled->default_current);
	}
	/* flash current set */
	s2mu106_fled_set_flash_curr(fled, 1, fled->flash_current);

	/* torch current set */
	s2mu106_fled_set_torch_curr(fled, 1, fled->torch_current);

	/* w/a: prevent SMPL event in case of flash operation */
	s2mu106_update_reg(fled->i2c, 0x21, 0x4, 0x7);
	s2mu106_update_reg(fled->i2c, 0x89, 0x0, 0x3);

	fled->psy_chg = power_supply_get_by_name("s2mu106-charger");

	s2mu106_fled_test_read(fled);
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

	ret = of_property_read_u32(np, "preflash_current",
			&pdata->preflash_current);
	if (ret < 0)
		pr_err("%s : could not find preflash_current\n", __func__);

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
			pdata->flashlight_current, S2MU106_FLASH_LIGHT_MAX);
	if (ret < 0) {
		pr_err("%s : could not find flashlight_current\n", __func__);

		//default setting
		pdata->flashlight_current[0] = 45;
		pdata->flashlight_current[1] = 75;
		pdata->flashlight_current[2] = 125;
		pdata->flashlight_current[3] = 195;
		pdata->flashlight_current[4] = 270;
	}

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

		pr_info("%s: temp = %d, index = %d\n", __func__, temp, index);

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
	return 0;

dt_err:
	pr_err("%s: DT parsing finish. ret = %d\n", __func__, ret);
	return ret;
}
#endif /* CONFIG_OF */

static ssize_t rear_flash_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int mode = -1;
	int value = 0;
	int flash_current = 0;
	int torch_current = 0;

	pr_info("%s: rear_flash_store start\n", __func__);
	if ((buf == NULL) || kstrtouint(buf, 10, &value)) {
		return -1;
	}

	if ((value < 0)) {
		pr_err("%s: value: %d\n", __func__, value);
		pr_err("%s: Wrong mode.\n", __func__);
		return -EFAULT;
	}
	pr_info("%s: %d: rear_flash_store:\n", __func__,value );
	if(NULL == g_fled_data) {
		pr_err("%s: s2mu106 flash device is NULL, return\n", __func__);
		return -1;
	}

	g_fled_data->sysfs_input_data = value;

	flash_current = g_fled_data->flash_current;
	torch_current = g_fled_data->torch_current;

	if (value <= 0) {
		mode = S2MU106_FLED_MODE_OFF;
	} else if (value == 1) {
		mode = S2MU106_FLED_MODE_TORCH;
		torch_current = g_fled_data->flashlight_current[0];
	} else if (value == 100) {
		/* Factory Torch*/
		pr_info("%s: factory torch current [%d]\n", __func__, g_fled_data->factory_current);
		torch_current = g_fled_data->factory_current;
		mode = S2MU106_FLED_MODE_TORCH;
	} else if (value == 200) {
		/* Factory Flash */
		pr_info("%s: factory flash current [%d]\n", __func__, g_fled_data->factory_current);
		flash_current = g_fled_data->factory_current;
		mode = S2MU106_FLED_MODE_FLASH;
	} else if (value <= 1010 && value >= 1001) {
		mode = S2MU106_FLED_MODE_TORCH;
		/* (value) 1001, 1002, 1004, 1006, 1009 */
		if (value <= 1001)
			torch_current = g_fled_data->flashlight_current[0];
		else if (value <= 1002)
			torch_current = g_fled_data->flashlight_current[1];
		else if (value <= 1004)
			torch_current = g_fled_data->flashlight_current[2];
		else if (value <= 1006)
			torch_current = g_fled_data->flashlight_current[3];
		else if (value <= 1009)
			torch_current = g_fled_data->flashlight_current[4];
		else
			torch_current = g_fled_data->torch_current;
		g_fled_data->sysfs_input_data = 1;
	} else if (value == 2) {
		mode = S2MU106_FLED_MODE_FLASH;
	}

	mutex_lock(&g_fled_data->lock);
	if (g_fled_data->control_mode == CONTROL_I2C) {
		s2mu106_fled_set_mode(g_fled_data, 1, mode);
	} else {
		if (mode == S2MU106_FLED_MODE_TORCH) {
			pr_info("%s: %d: S2MU106_FLED_MODE_FACTORY - %dmA\n", __func__, value, torch_current );
			/* torch current set */
			s2mu106_fled_set_torch_curr(g_fled_data, 1, torch_current);
			s2mu106_led_mode_ctrl(S2MU106_FLED_MODE_FACTORY);
		} else if (mode == S2MU106_FLED_MODE_FLASH) {
			pr_info("%s: %d: S2MU106_FLED_MODE_FLASH - %dmA\n", __func__, value, flash_current );
			/* flash current set */
			s2mu106_fled_set_flash_curr(g_fled_data, 1, flash_current);
			s2mu106_led_mode_ctrl(S2MU106_FLED_MODE_FLASH);
		} else {
			pr_info("%s: %d: S2MU106_FLED_MODE_OFF\n", __func__,value );

			/* flase, torch current set for initial current */
			flash_current = g_fled_data->flash_current;
			torch_current = g_fled_data->torch_current;

			/* flash current set */
			s2mu106_fled_set_flash_curr(g_fled_data, 1, flash_current);
			/* torch current set */
			s2mu106_fled_set_torch_curr(g_fled_data, 1, torch_current);
			s2mu106_led_mode_ctrl(S2MU106_FLED_MODE_OFF);
		}
	}

	mutex_unlock(&g_fled_data->lock);
	s2mu106_fled_test_read(g_fled_data);
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

static int create_flash_sysfs(struct s2mu106_fled_data *fled_data)
{
	int err = -ENODEV;
	struct device *flash_dev = fled_data->flash_dev;

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

static int s2mu106_led_probe(struct platform_device *pdev)
{
	int ret = 0;
	int cnt = 0;
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
	g_fled_data = fled_data;

	snprintf(name, sizeof(name), "fled-s2mu106");
	fled_data->cdev.name = name;
	fled_data->cdev.groups = s2mu106_fled_groups;

	ret = devm_led_classdev_register(&pdev->dev, &fled_data->cdev);
	if (ret < 0) {
		pr_err("%s: unable to register LED class dev\n", __func__);
		return ret;
	}

	g_fled_data->flash_gpio			= fled_data->pdata->flash_gpio;
	g_fled_data->torch_gpio			= fled_data->pdata->torch_gpio;
	g_fled_data->default_current	= fled_data->pdata->default_current;
	g_fled_data->flash_current 		= fled_data->pdata->flash_current;
	g_fled_data->torch_current 		= fled_data->pdata->torch_current;
	g_fled_data->preflash_current 	= fled_data->pdata->preflash_current;
	g_fled_data->movie_current 		= fled_data->pdata->movie_current;
	g_fled_data->factory_current 	= fled_data->pdata->factory_current;

	for (cnt = 0; cnt < S2MU106_FLASH_LIGHT_MAX; cnt++) {
		g_fled_data->flashlight_current[cnt] = fled_data->pdata->flashlight_current[cnt];
	}

	s2mu106_fled_init(g_fled_data);
	mutex_init(&fled_data->lock);

	//create sysfs for camera.
	create_flash_sysfs(fled_data);

	pr_info("%s: s2mu106_fled loaded\n", __func__);
	return 0;
}

static int s2mu106_led_suspend(struct device *dev)
{

	struct pinctrl *pinctrl_i2c = NULL;
	pr_info("%s(%s)\n", __func__,dev_driver_string(dev));

	pinctrl_i2c = devm_pinctrl_get_select(dev->parent, "flash_suspend");
	if (IS_ERR_OR_NULL(pinctrl_i2c)) {
		printk(KERN_ERR "%s: Failed to configure i2c pin\n", __func__);
	} else {
		devm_pinctrl_put(pinctrl_i2c);
	}

	return 0;
}

static int s2mu106_led_resume(struct device *dev)
{
	pr_info("%s\n", __func__);

	return 0;
}

static const struct dev_pm_ops s2mu106_led_pm_ops = {
	.suspend                = s2mu106_led_suspend,
	.resume                 = s2mu106_led_resume,
};

static int s2mu106_led_remove(struct platform_device *pdev)
{
	struct s2mu106_fled_data *fled_data = platform_get_drvdata(pdev);

	device_remove_file(fled_data->flash_dev, &dev_attr_rear_flash);
	device_remove_file(fled_data->flash_dev, &dev_attr_rear_torch_flash);
	device_destroy(camera_class, 0);
	class_destroy(camera_class);
	mutex_destroy(&fled_data->lock);
	return 0;
}

static const struct platform_device_id s2mu106_leds_id[] = {
	{"leds-s2mu106", 0},
	{},
};

static struct platform_driver s2mu106_led_driver = {
	.driver = {
		.name  = "leds-s2mu106",
		.owner = THIS_MODULE,
		.pm = &s2mu106_led_pm_ops,
		},
	.probe  = s2mu106_led_probe,
	.remove = s2mu106_led_remove,
	.id_table = s2mu106_leds_id,
};

static int s2mu106_led_pinctrl_init(
	struct s2mu106_pinctrl_info *s2mu106_pctrl, struct device *dev)
{

	s2mu106_pctrl->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(s2mu106_pctrl->pinctrl)) {
		pr_err("%s: Getting pinctrl handle failed", __func__);
		return -EINVAL;
	}
	s2mu106_pctrl->gpio_state_active =
		pinctrl_lookup_state(s2mu106_pctrl->pinctrl,
				"fled_default");
	if (IS_ERR_OR_NULL(s2mu106_pctrl->gpio_state_active)) {
		pr_err("%s: Failed to get the active state pinctrl handle", __func__);
		return -EINVAL;
	}
	s2mu106_pctrl->gpio_state_suspend
		= pinctrl_lookup_state(s2mu106_pctrl->pinctrl,
				"fled_suspend");
	if (IS_ERR_OR_NULL(s2mu106_pctrl->gpio_state_suspend)) {
		pr_err("%s Failed to get the suspend state pinctrl handle", __func__);
		return -EINVAL;
	}

	return 0;
}

static int32_t s2mu106_led_i2c_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{
	struct s2mu106_dev *s2mu106;
	struct s2mu106_fled_platform_data *pdata = i2c->dev.platform_data;
	struct s2mu106_fled_data *fled_data = NULL;

	int ret = 0;

	pr_info("%s i2c addr = 0x%x g_fled_data=%p\n", __func__, i2c->addr, g_fled_data);

	if (NULL == g_fled_data) {
		s2mu106 = devm_kzalloc(&i2c->dev, sizeof(struct s2mu106_dev),
				       GFP_KERNEL);
		if (!s2mu106) {
			dev_err(&i2c->dev, "%s: Failed to alloc mem for s2mu106\n",
					__func__);
			return -ENOMEM;
		}

		fled_data = devm_kzalloc(&i2c->dev,
					     sizeof(struct s2mu106_fled_data),
					     GFP_KERNEL);
		if (!fled_data) {
			pr_err("%s: failed to allocate driver data\n", __func__);
			return -ENOMEM;
		}

		if (i2c->dev.of_node) {
			pdata = devm_kzalloc(&i2c->dev,
					     sizeof(struct s2mu106_fled_platform_data),
					     GFP_KERNEL);
			if (!pdata) {
				dev_err(&i2c->dev, "Failed to allocate memory\n");
				ret = -ENOMEM;
				goto err;
			}

			ret = s2mu106_led_dt_parse_pdata(&i2c->dev, pdata);
			if (ret < 0) {
				dev_err(&i2c->dev, "Failed to get device of_node\n");
				goto err;
			}

			fled_data->pdata = i2c->dev.platform_data = pdata;
		} else {
			pr_err("%s: there is no of_node data\n", __func__);
			fled_data->pdata = pdata = i2c->dev.platform_data;
		}
		s2mu106->dev = &i2c->dev;
		s2mu106->i2c = i2c;
		s2mu106->irq = i2c->irq;
		mutex_init(&s2mu106->i2c_lock);

		pr_info("%s: i2c_set_clientdata for s2mu106=%p pdata=%p\n", __func__, s2mu106, fled_data->pdata);
		i2c_set_clientdata(i2c, s2mu106);

		/* Store fled_data for EXPORT_SYMBOL */
		g_fled_data = fled_data;

		s2mu106_fled_init(g_fled_data);
		mutex_init(&fled_data->lock);
		/*muic_notifier_register(&fled_data->batt_nb, ta_notification,
			       	       MUIC_NOTIFY_DEV_CHARGER);*/
	} else {
		fled_data = g_fled_data;
	}

	ret = s2mu106_led_pinctrl_init(&fled_data->flash_pctrl, &i2c->dev);
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
	kfree(s2mu106);

	return ret;
}

static int s2mu106_led_i2c_remove(struct i2c_client *client)
{
	struct s2mu106_dev *s2mu106 = i2c_get_clientdata(client);
	pr_info("%s: remove i2c device s2mu106 = %p\n", __func__, s2mu106);
	i2c_unregister_device(s2mu106->i2c);
	i2c_set_clientdata(client, NULL);

	return 0;
}

int ext_pmic_cam_fled_ctrl(int cam_mode, int curr)
{
	int mode = -1;

	if(NULL == g_fled_data) {
		pr_err("%s: s2mu106 flash device is NULL, return\n", __func__);
		return 0;
	}

	mutex_lock(&g_fled_data->lock);
	mode = s2mu106_mode_change_cam_to_leds(cam_mode);
	s2mu106_led_mode_ctrl(mode);

#ifdef DEBUG_TEST_READ
	pr_info("%s: cam_mode=%d curr=%d mode=%d default cur = %d %d\n",
		__func__, cam_mode, curr, mode, g_fled_data->pdata->flash_current, g_fled_data->pdata->torch_current);
	s2mu106_fled_test_read(g_fled_data);
	s2mu106_fled_get_flash_curr(g_fled_data, 1);
	s2mu106_fled_get_torch_curr(g_fled_data, 1);
#endif
	mutex_unlock(&g_fled_data->lock);

	pr_info("%s: ext_pmic_cam_fled_ctrl END\n", __func__);

	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id cam_flash_dt_match[] = {
	{.compatible = "qcom,s2mu106-fled", .data = NULL},
	{}
};
MODULE_DEVICE_TABLE(of, cam_flash_dt_match);
#endif

#define FLED_DRIVER_I2C "s2mu106_i2c_fled"

static const struct i2c_device_id i2c_id[] = {
	{FLED_DRIVER_I2C, (kernel_ulong_t)NULL},
	{ }
};

static struct i2c_driver s2mu106_led_driver_i2c = {
	.id_table = i2c_id,
	.probe = s2mu106_led_i2c_probe,
	.remove = s2mu106_led_i2c_remove,
	.driver = {
		.name = FLED_DRIVER_I2C,
		.owner = THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table = cam_flash_dt_match,
#endif
		.suppress_bind_attrs = true,	
	},
};
static int __init s2mu106_led_driver_init(void)
{
	int rc = 0;
	pr_info("%s: platform_driver_register start...\n", __func__);

	rc = platform_driver_register(&s2mu106_led_driver);
	if (rc < 0) {
		pr_info("%s: platform_driver_register Failed: rc = %d",
			__func__, rc);
		return rc;
	}

	pr_info("%s: s2mu106_fled i2c_add_driver start...\n", __func__);

	rc = i2c_add_driver(&s2mu106_led_driver_i2c);
	if (rc)
		pr_info("%s: s2mu106_fled i2c_add_driver rc=%d\n", __func__, rc);

	return rc;
}
module_init(s2mu106_led_driver_init);

static void __exit s2mu106_led_driver_exit(void)
{
	pr_info("%s: s2mu106_led_driver_exit\n", __func__);
	platform_driver_unregister(&s2mu106_led_driver);
	i2c_del_driver(&s2mu106_led_driver_i2c);
}
module_exit(s2mu106_led_driver_exit);

MODULE_AUTHOR("Keunho Hwang <keunho.hwang@samsung.com>");
MODULE_DESCRIPTION("SAMSUNG s2mu106 flash LED Driver");
MODULE_LICENSE("GPL");
