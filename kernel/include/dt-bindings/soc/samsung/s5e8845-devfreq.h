
/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Device Tree binding constants for Exynos991 devfreq
 */
#ifndef _DT_BINDINGS_EXYNOS_8845_DEVFREQ_H
#define _DT_BINDINGS_EXYNOS_8845_DEVFREQ_H
/* DEVFREQ TYPE LIST */
#define DEVFREQ_MIF			0
#define DEVFREQ_INT			1
#define DEVFREQ_NPU			2
#define DEVFREQ_DNC			3
#define DEVFREQ_AUD			4
#define DEVFREQ_DISP			5
#define DEVFREQ_INTCAM			6
#define DEVFREQ_CAM			7
#define DEVFREQ_ISP			8
#define DEVFREQ_MFC			9
#define DEVFREQ_CSIS			10
#define DEVFREQ_ICPU			11
#define DEVFREQ_TYPE_END		12
/* ESS FLAG LIST */
#define ESS_FLAG_CPU_CL0	0
#define ESS_FLAG_CPU_CL1	1
#define ESS_FLAG_CPU_CL2	2
#define ESS_FLAG_DSU		12
#endif
