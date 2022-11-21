/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_N1_DSP_N1_PM_H__
#define __HW_N1_DSP_N1_PM_H__

#include "hardware/dsp-pm.h"

enum dsp_n1_devfreq_id {
	DSP_N1_DEVFREQ_DNC,
	DSP_N1_DEVFREQ_DSP,
	DSP_N1_DEVFREQ_COUNT,
};

enum dsp_n1_extra_devfreq_id {
	DSP_N1_EXTRA_DEVFREQ_CL2,
	DSP_N1_EXTRA_DEVFREQ_CL1,
	DSP_N1_EXTRA_DEVFREQ_CL0,
	DSP_N1_EXTRA_DEVFREQ_INT,
	DSP_N1_EXTRA_DEVFREQ_MIF,
	DSP_N1_EXTRA_DEVFREQ_MIF_DSP,
	DSP_N1_EXTRA_DEVFREQ_COUNT,
};

int dsp_hw_n1_pm_init(void);

#endif
