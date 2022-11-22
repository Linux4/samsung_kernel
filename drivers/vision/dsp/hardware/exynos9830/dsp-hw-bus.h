/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_HW_BUS_H__
#define __DSP_HW_BUS_H__

struct dsp_system;

enum dsp_bus_scenario_id {
	DSP_MO_MAX,
	DSP_MO_SCENARIO_COUNT,
};

struct dsp_bus {
	struct dsp_bus_scenario	*scen;
	struct dsp_system	*sys;
};

#endif
