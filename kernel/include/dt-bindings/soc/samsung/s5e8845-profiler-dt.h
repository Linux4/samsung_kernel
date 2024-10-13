/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __EXYNOS_PROFILER_DT_H
#define __EXYNOS_PROFILER_DT_H

#define	PROFILER_CL0	0
#define	PROFILER_CL1	1
#define	PROFILER_GPU	2
#define	PROFILER_MIF	3
#define	NUM_OF_DOMAIN	4
#define NUM_OF_CPU_DOMAIN	(PROFILER_CL1 + 1)

#define MAXNUM_OF_CPU		8
#define MAXNUM_OF_DVFS		30

#define BASE_CORE_LIT	0	// 0-3
#define BASE_CORE_BIG	4	// 4-7

#define PROFILER_GET_CPU_BASE_ID(id) ({ \
	int core;                           \
	if (id >= PROFILER_CL1)             \
		core = BASE_CORE_BIG;           \
	else                                \
		core = BASE_CORE_LIT;           \
	core;                               \
})

#define PROFILER_GET_CL_ID(core) ({ \
	int id;                         \
	if (core >= BASE_CORE_BIG)      \
		id = PROFILER_CL1;          \
	else                            \
		id = PROFILER_CL0;          \
	id;                             \
})

#endif /* End __EXYNOS_PROFILER_DT_H */
