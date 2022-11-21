/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_N3_DSP_HW_N3_PM_H__
#define __HW_N3_DSP_HW_N3_PM_H__

#include "hardware/dsp-pm.h"

enum dsp_n3_devfreq_id {
	DSP_N3_DEVFREQ_DNC,
	DSP_N3_DEVFREQ_DSP,
	DSP_N3_DEVFREQ_COUNT,
};

enum dsp_n3_extra_devfreq_id {
	DSP_N3_EXTRA_DEVFREQ_CL1,
	DSP_N3_EXTRA_DEVFREQ_CL0,
	DSP_N3_EXTRA_DEVFREQ_INT,
	DSP_N3_EXTRA_DEVFREQ_MIF,
	DSP_N3_EXTRA_DEVFREQ_COUNT,
};

int dsp_hw_n3_pm_init(void);

#endif
