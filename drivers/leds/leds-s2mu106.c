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
#include <linux/leds-s2m-common.h>
#include <linux/platform_device.h>
#include <linux/sec_batt.h>
#include <linux/power_supply.h>
#include <linux/leds-s2mu106.h>
#include <linux/mfd/slsi/s2mu106/s2mu106.h>
/* MUIC header file */
#include <linux/muic/common/muic.h>
#include <linux/muic/slsi/s2mu106/s2mu106-muic.h>
#include <linux/muic/slsi/s2mu106/s2mu106-muic-hv.h>
#include <linux/usb/typec/slsi/common/usbpd_ext.h>

#define CONTROL_I2C	0
#define CONTROL_GPIO	1

extern int factory_mode;

static struct s2mu106_fled_data *g_fled_data;

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

/**
 * To export Flash current in Torch Mode support
 * Created to separate Movie Mode Current and PreFlash Mode Current
 */
void s2mu106_set_torch_current(enum s2mu106_fled_mode state)
{
	pr_info("%s : State(%d)\n", __func__, state);
	if (state == S2MU106_FLED_MODE_TORCH) {
		s2mu106_fled_set_torch_curr(g_fled_data, g_fled_data->camera_fled_channel,
										g_fled_data->preflash_current);
	}
	else if (state == S2MU106_FLED_MODE_MOVIE) {
		s2mu106_fled_set_torch_curr(g_fled_data, g_fled_data->camera_fled_channel,
										g_fled_data->movie_current);
	}
	else {
		pr_err("%s : Invalid State\n", __func__);
	}
}

EXPORT_SYMBOL_GPL(s2mu106_set_torch_current);

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

/*
 * s2mu106_fled_set_mode associates a FGPIO pin (1 ~ 4) with a FLED channel 
 * in some mode (Torch or Flash) for GPIO control mode. Please check datasheet
 * for more information.
 */
static int s2mu106_fled_set_mode(struct s2mu106_fled_data *fled,
								int chan, int mode, int gpio)
{
	u8 dest = 0, bit = 0, mask = 0, status = 0;

	if ((chan <= 0) || (chan > S2MU106_CH_MAX) ||
		(mode < 0) || (mode > S2MU106_FLED_MODE_MAX) ||
		((fled->control_mode != CONTROL_I2C) &&
			((gpio < S2MU106_FLED_GPIO_EN1) || (gpio > S2MU106_FLED_GPIO_MAX)))) {
			pr_err("%s: Wrong channel or mode or gpio\n", __func__);
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
				bit = gpio << 3;
			break;
		case S2MU106_FLED_MODE_TORCH:
			s2mu106_fled_operating_mode(fled, SYS_MODE);
			mask = S2MU106_CHX_TORCH_FLED_EN;
			if (fled->control_mode == CONTROL_I2C)
				bit = S2MU106_FLED_EN;
			else
				bit = gpio; //It should matching with schematic
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
	int gpio_flash_set = fled->flash_set_gpio;

	pr_info("%s : state = %d\n", __func__, state);

	if (g_fled_data->control_mode == CONTROL_GPIO) {
		gpio_request(gpio_torch, "s2mu106_gpio_torch");
		gpio_request(gpio_flash, "s2mu106_gpio_flash");
		gpio_request(gpio_flash_set, "s2mu106_gpio_flash_set");
	}

	switch(state) {
	case S2MU106_FLED_MODE_OFF:
		
/*		Temporary commented - Kernel panic when boot(pdo_ctrl_by_flash) */
/* 		pdo_ctrl_by_flash(0);
		muic_afc_set_voltage(9);
		pr_info("[%s]%d Down Volatge set Clear \n" ,__func__,__LINE__);*/

		if (g_fled_data->control_mode == CONTROL_I2C) {
			s2mu106_fled_set_mode(g_fled_data, S2MU106_FLED_CH1, state, S2MU106_FLED_GPIO_NONE);
			s2mu106_fled_set_mode(g_fled_data, S2MU106_FLED_CH2, state, S2MU106_FLED_GPIO_NONE);
		} else { 
			gpio_direction_output(gpio_torch, 0);
			gpio_direction_output(gpio_flash, 0);
			gpio_direction_output(gpio_flash_set, 0);
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
			s2mu106_fled_set_mode(g_fled_data, S2MU106_FLED_CH2, state, S2MU106_FLED_GPIO_NONE);
		} else { 
			/* change SYS MODE when turn on torch */
			s2mu106_fled_operating_mode(fled, SYS_MODE);
			gpio_direction_output(gpio_flash_set, 1);
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
			s2mu106_fled_set_mode(g_fled_data, S2MU106_FLED_CH2, state, S2MU106_FLED_GPIO_NONE);
		} else { 
			gpio_direction_output(gpio_flash, 1);
		}
		break;
	case S2MU106_FLED_MODE_MOVIE:
		if (g_fled_data->control_mode == CONTROL_I2C) {
			s2mu106_fled_set_mode(g_fled_data, S2MU106_FLED_CH2, state, S2MU106_FLED_GPIO_NONE);
		} else { 

			s2mu106_fled_operating_mode(fled, SYS_MODE);
			s2mu106_fled_set_torch_curr(fled, fled->camera_fled_channel, fled->movie_current);
			gpio_direction_output(gpio_flash_set, 1);
		}
		break;
	case S2MU106_FLED_MODE_FACTORY:
		if (g_fled_data->control_mode == CONTROL_I2C) {
			s2mu106_fled_set_mode(g_fled_data, S2MU106_FLED_CH1, state, S2MU106_FLED_GPIO_NONE);
		} else { 

			/* change SYS MODE when turn on torch */
			s2mu106_fled_set_mode(fled, fled->flashlight_channel, S2MU106_FLED_MODE_TORCH, fled->flashlight_en_fgpio);
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
		gpio_free(gpio_flash_set);
	}

	return 0;
}

int s2mu106_mode_change_cam_to_leds(enum cam_flash_mode cam_mode)
{
	int mode = -1;

	switch(cam_mode) {
		case CAM_FLASH_MODE_OFF:
			mode = S2MU106_FLED_MODE_OFF;
			break;
		case CAM_FLASH_MODE_SINGLE:
			mode = S2MU106_FLED_MODE_FLASH;
			break;
		case CAM_FLASH_MODE_TORCH:
			mode = S2MU106_FLED_MODE_TORCH;
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

	s2mu106_fled_set_mode(fled, chan, mode, S2MU106_FLED_GPIO_NONE);
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
		s2mu106_fled_set_mode(fled, chan, mode, S2MU106_FLED_GPIO_NONE);
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
	if(!g_fled_data) {
		pr_err("%s: g_fled_data is NULL, s2mu106 probe is not called.\n", __func__);
		return;
	}

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
		if (gpio_is_valid(fled->pdata->flash_set_gpio)) {
			gpio_request_one(fled->flash_set_gpio, GPIOF_OUT_INIT_LOW, "LED_GPIO_OUTPUT_LOW");
		}
		gpio_request_one(fled->torch_gpio, GPIOF_OUT_INIT_LOW, "LED_GPIO_OUTPUT_LOW");

		gpio_free(fled->flash_gpio);
		if (gpio_is_valid(fled->pdata->flash_set_gpio)) {
			gpio_free(fled->flash_set_gpio);
		}
		gpio_free(fled->torch_gpio);

		/* CAM_FLASH_EN -> FLASH CAPTURE gpio mode */
		s2mu106_fled_set_mode(fled, fled->camera_fled_channel, S2MU106_FLED_MODE_FLASH, fled->camera_flash_en_fgpio);
		/* FLASH_SET -> VIDEO FLASH, PRE FLASH gpio mode */
		s2mu106_fled_set_mode(fled, fled->camera_fled_channel, S2MU106_FLED_MODE_TORCH, fled->camera_torch_en_fgpio);
		/*CAM_TORCH_EN -> FLASHLIGHT gpio mode*/
		s2mu106_fled_set_mode(fled, fled->flashlight_channel, S2MU106_FLED_MODE_TORCH, fled->flashlight_en_fgpio);

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
	s2mu106_fled_set_flash_curr(fled, 2, fled->flash_current);

	/* torch current set */
	s2mu106_fled_set_torch_curr(fled, 1, fled->torch_current);
	s2mu106_fled_set_torch_curr(fled, 2, fled->torch_current);

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

	ret = pdata->flash_set_gpio = of_get_named_gpio(np, "flash-set-gpio", 0);
	if (ret < 0) {
		pr_err("%s : can't get flash-set-gpio. torch-gpio set as flash-set-gpio\n", __func__);

		/* For models having one LED, same GPIO is used for camera-torch mode and Flashlight mode */	
		pdata->flash_set_gpio = pdata->torch_gpio;
	}

	ret = of_property_read_u32(np, "camera_fled_channel",
			&pdata->camera_fled_channel);
	if (ret < 0) {
		pr_err("%s : camera_flash_channel not available. Setting default value - CH1\n", __func__);
		pdata->camera_fled_channel = S2MU106_FLED_CH1;
	}

	ret = of_property_read_u32(np, "flashlight_channel",
			&pdata->flashlight_channel);
	if (ret < 0) {
		pr_err("%s : flashlight_channel not available. Setting default value - CH1\n", __func__);
		pdata->flashlight_channel = S2MU106_FLED_CH1;
	}

	ret = of_property_read_u32(np, "camera_flash_en_fgpio",
			&pdata->camera_flash_en_fgpio);
	if (ret < 0) {
		pr_err("%s : camera_flash_en_fgpio not available. Setting default value\n", __func__);
		pdata->camera_flash_en_fgpio = S2MU106_FLED_GPIO_EN1;
	}

	ret = of_property_read_u32(np, "camera_torch_en_fgpio",
			&pdata->camera_torch_en_fgpio);
	if (ret < 0) {
		pr_err("%s : camera_torch_en_fgpio not available. Setting default value\n", __func__);
		pdata->camera_torch_en_fgpio = S2MU106_FLED_GPIO_EN2;
	}

	ret = of_property_read_u32(np, "flashlight_en_fgpio",
			&pdata->flashlight_en_fgpio);
	if (ret < 0) {
		pr_err("%s : flashlight_en_fgpio not available. Setting default value\n", __func__);
		pdata->flashlight_en_fgpio = S2MU106_FLED_GPIO_EN2;
	}

	ret = of_property_read_u32(np, "flash_current",
			&pdata->flash_current);
	if (ret < 0)
		pr_err("%s : could not find flash_current\n", __func__);

	ret = of_property_read_u32(np, "preflash_current",
			&pdata->preflash_current);
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

	ret = of_property_read_u32(np, "factory_torch_current",
			&pdata->factory_torch_current);
	if (ret < 0)
		pr_err("%s : could not find factory_torch_current\n", __func__);

	ret = of_property_read_u32(np, "factory_flash_current",
			&pdata->factory_flash_current);
	if (ret < 0)
		pr_err("%s : could not find factory_flash_current\n", __func__);

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
	pr_info("%s: DT parsing finished successfully \n", __func__, ret);
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
	int fled_channel = g_fled_data->flashlight_channel;

	pr_info("%s: rear_flash_store start\n", __func__);
	if ((buf == NULL) || kstrtouint(buf, 10, &value)) {
		return -1;
	}

	if (strcmp(attr->attr.name, "rear_flash") == 0) {
		pr_info("%s : Using FLED CH1 \n",__func__);
		fled_channel = g_fled_data->flashlight_channel;
	} else if (strcmp(attr->attr.name, "rear_flash2") == 0) {
		pr_info("%s : Using FLED CH2 \n",__func__);
		fled_channel = g_fled_data->camera_fled_channel; 
	} else {
		pr_err("%s : Wrong sysfs file \n",__func__);
	}


	if ((value < 0)) {
		pr_err("%s: value: %d\n", __func__, value);
		pr_err("%s: Wrong mode.\n", __func__);
		return -EFAULT;
	}
	pr_info("%s: %d: rear_flash_store:\n", __func__,value );
	g_fled_data->sysfs_input_data = value;

	flash_current = g_fled_data->flash_current;
	torch_current = g_fled_data->torch_current;

	if (value <= 0) {
		mode = S2MU106_FLED_MODE_OFF;
	} else if (value == 1) {
		mode = S2MU106_FLED_MODE_TORCH;
	} else if (value == 100) {
		/* Factory Torch*/
		torch_current = g_fled_data->factory_torch_current;
		mode = S2MU106_FLED_MODE_TORCH;
		pr_info("%s: CH%d factory torch current [%d]\n", __func__, fled_channel, torch_current);
	} else if (value == 200) {
		/* Factory Flash */
		fled_channel = g_fled_data->camera_fled_channel;
		flash_current = g_fled_data->factory_flash_current;
		mode = S2MU106_FLED_MODE_FLASH;
		pr_info("%s: CH%d factory flash current [%d]\n", __func__, fled_channel, flash_current);
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
		s2mu106_fled_set_mode(g_fled_data, 1, mode, S2MU106_FLED_GPIO_NONE);
	} else {
		if (mode == S2MU106_FLED_MODE_TORCH) {
			/* torch current set */
			s2mu106_fled_set_torch_curr(g_fled_data, fled_channel, torch_current);
			if (fled_channel == g_fled_data->flashlight_channel) {
				s2mu106_led_mode_ctrl(S2MU106_FLED_MODE_FACTORY);
				pr_info("%s: CH%d- %d: LED MODE CTRL FACTORY - %dmA\n", __func__, fled_channel, value, torch_current ); //led2
			} else {
				s2mu106_led_mode_ctrl(S2MU106_FLED_MODE_TORCH);
				pr_info("%s: CH%d- %d: LED MODE CTRL TORCH - %dmA\n", __func__, fled_channel, value, torch_current ); //led1
			}
		} else if (mode == S2MU106_FLED_MODE_FLASH) {
			pr_info("%s: CH%d- %d: S2MU106_FLED_MODE_FLASH - %dmA\n", __func__, fled_channel, value, flash_current );
			/* flash current set */
			s2mu106_fled_set_flash_curr(g_fled_data, fled_channel, flash_current);
			s2mu106_led_mode_ctrl(S2MU106_FLED_MODE_FLASH);
		} else {
			pr_info("%s: CH%d- %d: S2MU106_FLED_MODE_OFF\n", __func__, fled_channel, value );

			/* flash, torch current reset to initial values */
			flash_current = g_fled_data->flash_current;
			torch_current = g_fled_data->torch_current;

			/* flash current set */
			s2mu106_fled_set_flash_curr(g_fled_data, g_fled_data->camera_fled_channel, flash_current);
			s2mu106_fled_set_flash_curr(g_fled_data, g_fled_data->flashlight_channel, flash_current);
			/* torch current set */
			s2mu106_fled_set_torch_curr(g_fled_data, g_fled_data->camera_fled_channel, torch_current);
			s2mu106_fled_set_torch_curr(g_fled_data, g_fled_data->flashlight_channel, torch_current);
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
static DEVICE_ATTR(rear_flash2, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
	rear_flash_show, rear_flash_store);

static int create_flash_sysfs(struct s2mu106_fled_data *fled_data)
{
	int err = -ENODEV;
	struct device *flash_dev = fled_data->flash_dev;

	if (IS_ERR_OR_NULL(camera_class)) {
		pr_err("flash_sysfs: error, camera class not exist");
		return -ENODEV;
	}

	flash_dev = device_create(camera_class, NULL, 0, NULL, "flash");
	if (IS_ERR(flash_dev)) {
		pr_err("flash_sysfs: failed to create device(flash)\n");
		return -ENODEV;
	}
	err = device_create_file(flash_dev, &dev_attr_rear_flash);
	if (unlikely(err < 0)) {
		pr_err("flash_sysfs: failed to create device file, %s\n",
				dev_attr_rear_flash.attr.name);
	}
	err = device_create_file(flash_dev, &dev_attr_rear_flash2);
	if (unlikely(err < 0)) {
		pr_err("flash_sysfs: failed to create device file, %s\n",
				dev_attr_rear_flash2.attr.name);
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

	g_fled_data->flash_gpio				= fled_data->pdata->flash_gpio;
	g_fled_data->flash_set_gpio			= fled_data->pdata->flash_set_gpio;
	g_fled_data->torch_gpio				= fled_data->pdata->torch_gpio;
	g_fled_data->camera_fled_channel	= fled_data->pdata->camera_fled_channel;
	g_fled_data->flashlight_channel		= fled_data->pdata->flashlight_channel;
	g_fled_data->camera_flash_en_fgpio	= fled_data->pdata->camera_flash_en_fgpio;
	g_fled_data->camera_torch_en_fgpio	= fled_data->pdata->camera_torch_en_fgpio;
	g_fled_data->flashlight_en_fgpio	= fled_data->pdata->flashlight_en_fgpio;
	g_fled_data->default_current		= fled_data->pdata->default_current;
	g_fled_data->flash_current 			= fled_data->pdata->flash_current;
	g_fled_data->torch_current 			= fled_data->pdata->torch_current;
	g_fled_data->preflash_current 		= fled_data->pdata->preflash_current;
	g_fled_data->movie_current 			= fled_data->pdata->movie_current;
	g_fled_data->factory_torch_current 	= fled_data->pdata->factory_torch_current;
	g_fled_data->factory_flash_current 	= fled_data->pdata->factory_flash_current;

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
	device_remove_file(fled_data->flash_dev, &dev_attr_rear_flash2);
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

MODULE_AUTHOR("Keunho Hwang <keunho.hwang@samsung.com>");
MODULE_DESCRIPTION("SAMSUNG s2mu106 flash LED Driver");
MODULE_LICENSE("GPL");
