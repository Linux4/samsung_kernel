/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_O3_DSP_HW_O3_PM_H__
#define __HW_O3_DSP_HW_O3_PM_H__

#include "hardware/dsp-pm.h"

enum dsp_o3_devfreq_id {
	DSP_O3_DEVFREQ_NPUS,
	DSP_O3_DEVFREQ_VPD,
	DSP_O3_DEVFREQ_COUNT,
};

enum dsp_o3_extra_devfreq_id {
	DSP_O3_EXTRA_DEVFREQ_CL2,
	DSP_O3_EXTRA_DEVFREQ_CL1,
	DSP_O3_EXTRA_DEVFREQ_CL0,
	DSP_O3_EXTRA_DEVFREQ_INT,
	DSP_O3_EXTRA_DEVFREQ_MIF,
	DSP_O3_EXTRA_DEVFREQ_COUNT,
};

int dsp_hw_o3_pm_init(void);

#endif
