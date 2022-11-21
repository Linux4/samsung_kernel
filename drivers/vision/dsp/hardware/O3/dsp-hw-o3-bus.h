/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_O3_DSP_HW_O3_BUS_H__
#define __HW_O3_DSP_HW_O3_BUS_H__

#include "hardware/dsp-bus.h"

enum dsp_o3_bus_scenario_id {
	DSP_O3_MO_MAX,
	DSP_O3_MO_NPU_NORMAL,
	DSP_O3_MO_DSP_CAMERA_CAPTURE,
	DSP_O3_MO_SCENARIO_COUNT,
};

int dsp_hw_o3_bus_init(void);

#endif
