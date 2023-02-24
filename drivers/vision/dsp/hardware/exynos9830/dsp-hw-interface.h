/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_HW_INTERFACE_H__
#define __DSP_HW_INTERFACE_H__

struct dsp_system;

struct dsp_interface {
	void __iomem			*sfr;

	int				irq0;
	int				irq1;
	int				irq2;
	int				irq3;
	int				irq4;

	struct dsp_system		*sys;
};

#endif
