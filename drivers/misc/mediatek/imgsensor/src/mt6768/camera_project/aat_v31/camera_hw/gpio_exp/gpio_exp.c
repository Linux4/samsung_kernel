/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "gpio_exp.h"

#define OF_NAME_MIPI_SWITCH_EN "mipi_switch_en"
#define OF_NAME_MIPI_SWITCH_SEL "mipi_switch_sel"

static const char *hw_pin_names[IMGSENSOR_HW_PIN_MAX_NUM] = {
	[IMGSENSOR_HW_PIN_PDN] = "pdn",
	[IMGSENSOR_HW_PIN_RST] = "rst",
	[IMGSENSOR_HW_PIN_AVDD] = "avdd",
	[IMGSENSOR_HW_PIN_DVDD] = "dvdd",
	[IMGSENSOR_HW_PIN_DOVDD] = "dovdd",
	[IMGSENSOR_HW_PIN_AFVDD] = "afvdd",
	[IMGSENSOR_HW_PIN_AVDD2] = "avdd2",
};

static struct GPIO_EXP gpio_exp_instance;

static int gpioexp_set(int gpio_num, int value)
{
	int ret = 0;

	ret = gpio_request(gpio_num, NULL);
	if (ret < 0) {
		pr_info("%s: fail to set %d as %d\n",
			__func__, gpio_num, value);
		return ret;
	}
	ret = gpio_direction_output(gpio_num, value);
	gpio_set_value(gpio_num, value);
	gpio_free(gpio_num);

	return ret;
}

static enum IMGSENSOR_RETURN gpio_exp_release(void *pinstance)
{
	int i, j;
	struct GPIO_EXP *pinst = pinstance;

	for (j = IMGSENSOR_SENSOR_IDX_MIN_NUM;
		j < IMGSENSOR_SENSOR_IDX_MAX_NUM; j++) {
		for (i = 0; i < IMGSENSOR_HW_PIN_MAX_NUM; i++) {
			if (pinst->gpionum[j][i] < 0)
				continue;
			gpioexp_set(pinst->gpionum[j][i], 0);
		}
	}

	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN gpio_exp_init(void *pinstance)
{
	int i, j;
	struct GPIO_EXP *pinst = pinstance;
	struct platform_device *pdev = gpimgsensor_hw_platform_device;
	char of_name[LENGTH_FOR_SNPRINTF];

	for (j = IMGSENSOR_SENSOR_IDX_MIN_NUM;
		j < IMGSENSOR_SENSOR_IDX_MAX_NUM; j++) {
		for (i = 0; i < IMGSENSOR_HW_PIN_MAX_NUM; i++) {
			if (!hw_pin_names[i]) {
				pinst->gpionum[j][i] = -1;
				continue;
			}
			snprintf(of_name, sizeof(of_name),
				"cam%d_%s", j, hw_pin_names[i]);
			pinst->gpionum[j][i] = of_get_named_gpio(
				pdev->dev.of_node, of_name, 0);
		}
	}

#ifdef MIPI_SWITCH
	pinst->gpionum_mipi_switch_en = of_get_named_gpio(
		pdev->dev.of_node, OF_NAME_MIPI_SWITCH_EN, 0);
	pinst->gpionum_mipi_switch_sel = of_get_named_gpio(
		pdev->dev.of_node, OF_NAME_MIPI_SWITCH_SEL, 0);
#else
	pinst->gpionum_mipi_switch_en = -1;
	pinst->gpionum_mipi_switch_sel = -1;
#endif

	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN gpio_exp_set(
	void *pinstance,
	enum IMGSENSOR_SENSOR_IDX   sensor_idx,
	enum IMGSENSOR_HW_PIN       pin,
	enum IMGSENSOR_HW_PIN_STATE pin_state)
{
	int ret, val;
	struct GPIO_EXP *pinst = pinstance;

	val = (pin_state > IMGSENSOR_HW_PIN_STATE_LEVEL_0) ? 1 : 0;

#ifdef MIPI_SWITCH
	if (pin == IMGSENSOR_HW_PIN_MIPI_SWITCH_EN) {
		if (pinst->gpionum_mipi_switch_en < 0)
			return IMGSENSOR_RETURN_ERROR;
		ret = gpioexp_set(pinst->gpionum_mipi_switch_en, val);
	}

	else if (pin == IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL) {
		if (pinst->gpionum_mipi_switch_sel < 0)
			return IMGSENSOR_RETURN_ERROR;
		ret = gpioexp_set(pinst->gpionum_mipi_switch_sel, val);
	}

	else
#endif

	{
		if (pinst->gpionum[sensor_idx][pin] < 0)
			return IMGSENSOR_RETURN_ERROR;
		ret = gpioexp_set(pinst->gpionum[sensor_idx][pin], val);
	}

	return (!ret) ? IMGSENSOR_RETURN_SUCCESS : IMGSENSOR_RETURN_ERROR;
}

static struct IMGSENSOR_HW_DEVICE device = {
	.pinstance = (void *)&gpio_exp_instance,
	.init      = gpio_exp_init,
	.set       = gpio_exp_set,
	.release   = gpio_exp_release,
	.id        = IMGSENSOR_HW_ID_GPIO_EXP
};

enum IMGSENSOR_RETURN imgsensor_hw_gpio_exp_open(
	struct IMGSENSOR_HW_DEVICE **pdevice)
{
	*pdevice = &device;
	return IMGSENSOR_RETURN_SUCCESS;
}

