/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_N1_DSP_HW_N1_BUS_H__
#define __HW_N1_DSP_HW_N1_BUS_H__

#include "hardware/dsp-bus.h"

enum dsp_n1_bus_scenario_id {
	DSP_N1_MO_MAX,
	DSP_N1_MO_CAMERA,
	DSP_N1_MO_SCENARIO_COUNT,
};

int dsp_hw_n1_bus_init(void);

#endif
