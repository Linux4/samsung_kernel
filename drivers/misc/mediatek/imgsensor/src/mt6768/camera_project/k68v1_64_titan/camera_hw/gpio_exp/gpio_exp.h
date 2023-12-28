/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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

