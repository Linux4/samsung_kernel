/*
 * Samsung Exynos5 SoC series Actuator driver
 *
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_DEVICE_AK737X_H
#define IS_DEVICE_AK737X_H

#define ACTUATOR_NAME		"AK737X"

#define AK737X_REG_CONT1				0x02

#define AK737X_MODE_SLEEP				0x20
#define AK737X_MODE_STANDBY				0x40
#define AK737X_MODE_ACTIVE				0x00

#define AK737X_PRODUCT_ID_AK7371		0x09
#define AK737X_PRODUCT_ID_AK7372		0x0C
#define AK737X_PRODUCT_ID_AK7374		0x0E
#define AK737X_PRODUCT_ID_AK7377		0x13

#define AK737X_POS_SIZE_BIT		ACTUATOR_POS_SIZE_9BIT
#define AK737X_POS_MAX_SIZE		((1 << AK737X_POS_SIZE_BIT) - 1)
#define AK737X_POS_DIRECTION		ACTUATOR_RANGE_INF_TO_MAC
#define AK737X_REG_POS_HIGH		0x00
#define AK737X_REG_POS_LOW		0x01

#define AK737X_REG_SETTING_MODE_ON		0xAE
#define AK737X_REG_CHANGE_GAMMA_PARAMETER		0x15 // gamma
#define AK737X_REG_CHANGE_GAIN1_PARAMETER		0x11 // gain1
#define AK737X_REG_CHANGE_GAIN_PARAMETER		0x25 // gain2

#define AK737X_MAX_PRODUCT_LIST		6

#endif
