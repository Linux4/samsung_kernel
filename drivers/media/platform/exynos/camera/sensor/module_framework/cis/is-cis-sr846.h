/*
 * Samsung Exynos5 SoC series Sensor driver
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_SR846D_H
#define IS_CIS_SR846D_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_SR846_MAX_WIDTH      (3264 + 0)
#define SENSOR_SR846_MAX_HEIGHT     (2448 + 0)

#define SENSOR_SR846_FINE_INTEGRATION_TIME_MIN                0x0
#define SENSOR_SR846_FINE_INTEGRATION_TIME_MAX                0x0
#define SENSOR_SR846_COARSE_INTEGRATION_TIME_MIN              0x06
#define SENSOR_SR846_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x06

#define SENSOR_SR846_FRAME_LENGTH_LINE_ADDR      (0x0006)
#define SENSOR_SR846_LINE_LENGTH_PCK_ADDR        (0x0008)
#define SENSOR_SR846_MODEL_ID_ADDR               (0x0F16)
#define SENSOR_SR846_SENSOR_ID_ADDR              (0x0F14)
#define SENSOR_SR846_MODE_SEL_ADDR               (0x0A20)
#define SENSOR_SR846_GROUP_PARAM_HOLD_ADDR       (0x0046)
#define SENSOR_SR846_COARSE_INTEG_TIME_ADDR      (0x0074)
#define SENSOR_SR846_ANALOG_GAIN_ADDR            (0x0077)
#define SENSOR_SR846_DIGITAL_GAIN_ADDR           (0x0078)
#define SENSOR_SR846_ISP_PLL_ENABLE_ADDR         (0x0F02)
#define SENSOR_SR846_ISP_EN_ADDR                 (0x0A04)
#define SENSOR_SR846_WINDOW_ACTIVE_X_START_ADDR  (0x0120)
#define SENSOR_SR846_WINDOW_ACTIVE_Y_START_ADDR  (0x0026)
#define SENSOR_SR846_WINDOW_ACTIVE_X_END_ADDR    (0x0122)
#define SENSOR_SR846_WINDOW_ACTIVE_Y_END_ADDR    (0x002C)
#define SENSOR_SR846_FMT_COLUMN_OUT_SIZE_ADDR    (0x0A12)
#define SENSOR_SR846_FMT_ROW_OUT_SIZE_ADDR       (0x0A14)
#define SENSOR_SR846_OTP_RDATA_ADDR              (0x0708)

#define SENSOR_SR846_MIN_ANALOG_GAIN_SET_VALUE   (0x0)
#define SENSOR_SR846_MAX_ANALOG_GAIN_SET_VALUE   (0xF0)
#define SENSOR_SR846_MIN_DIGITAL_GAIN_SET_VALUE  (0x200)
#define SENSOR_SR846_MAX_DIGITAL_GAIN_SET_VALUE  (0x1FFB)

#define SENSOR_SR846_DEBUG_INFO (1)
#define USE_GROUP_PARAM_HOLD (0)
#define SENSOR_SR846_OTP_READ (0)

#endif

