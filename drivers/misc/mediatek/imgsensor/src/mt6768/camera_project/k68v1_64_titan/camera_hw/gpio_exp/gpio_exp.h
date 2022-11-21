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

#ifndef __IMGSENSOR_HW_GPIO_EXP_H__
#define __IMGSENSOR_HW_GPIO_EXP_H__

#include "imgsensor_common.h"
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_gpio.h>

#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>
#include "imgsensor_hw.h"
#include "imgsensor_platform.h"

struct GPIO_EXP {
	int gpionum_mipi_switch_en;
	int gpionum_mipi_switch_sel;
	int gpionum[IMGSENSOR_SENSOR_IDX_MAX_NUM][IMGSENSOR_HW_PIN_MAX_NUM];
};

enum IMGSENSOR_RETURN
imgsensor_hw_gpio_exp_open(struct IMGSENSOR_HW_DEVICE **pdevice);

extern struct platform_device *gpimgsensor_hw_platform_device;

#endif

