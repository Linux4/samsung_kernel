/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_CONTROL_H__
#define __DSP_CONTROL_H__

#define SCENARIO_NAME_MAX		(32)

enum dsp_control_id {
	DSP_CONTROL_ENABLE_DVFS,
	DSP_CONTROL_DISABLE_DVFS,
	DSP_CONTROL_ENABLE_BOOST,
	DSP_CONTROL_DISABLE_BOOST,
	DSP_CONTROL_REQUEST_MO,
	DSP_CONTROL_RELEASE_MO,
};

struct dsp_control_dvfs {
	unsigned int			pm_qos;
	unsigned int			reserved[3];
};

struct dsp_control_boost {
	unsigned int			reserved[4];
};

struct dsp_control_mo {
	unsigned char			scenario_name[SCENARIO_NAME_MAX];
	unsigned int			reserved[2];
};

union dsp_control {
	struct dsp_control_dvfs		dvfs;
	struct dsp_control_boost	boost;
	struct dsp_control_mo		mo;
};

#endif
