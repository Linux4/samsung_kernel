/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_HW_CLK_H__
#define __DSP_HW_CLK_H__

#include <linux/device.h>

struct dsp_clk_foramt;

struct dsp_clk {
	struct device		*dev;
	struct dsp_clk_format	*array;
};

#endif

