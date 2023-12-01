/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Device Tree binding constants for Exynos991 devfreq
 */

#ifndef _DT_BINDINGS_EXYNOS_9935_DEVFREQ_H
#define _DT_BINDINGS_EXYNOS_9935_DEVFREQ_H

/* DEVFREQ TYPE LIST */
#define DEVFREQ_MIF			0
#define DEVFREQ_INT			1
#define DEVFREQ_INTCAM			2
#define DEVFREQ_DISP			3
#define DEVFREQ_CAM			4
#define DEVFREQ_NPU0			5
#define DEVFREQ_NPU1			6
#define DEVFREQ_AUD			7
#define DEVFREQ_DNC			8
#define DEVFREQ_MFC			9
#define DEVFREQ_CSIS			10
#define DEVFREQ_ISP			11
#define DEVFREQ_MFD			12
#define DEVFREQ_DSP			13
#define DEVFREQ_ALIVE			14
#define DEVFREQ_CHUB			15
#define DEVFREQ_VTS			16
#define DEVFREQ_HSI0			17
#define DEVFREQ_UFD			18
#define DEVFREQ_TYPE_END		19

/* ESS FLAG LIST */

/* DEVFREQ GOV TYPE */
#define SIMPLE_INTERACTIVE	0

#define ESS_FLAG_CPU_CL0	0
#define ESS_FLAG_CPU_CL1	1
#define ESS_FLAG_CPU_CL2	2
#define ESS_FLAG_DSU		12
#endif
