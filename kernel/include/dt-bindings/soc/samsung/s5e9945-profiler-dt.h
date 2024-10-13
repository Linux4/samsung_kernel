/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
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
#define	PROFILER_CL1L	1
#define	PROFILER_CL1H	2
#define	PROFILER_CL2	3
#define	PROFILER_GPU	4
#define	PROFILER_MIF	5
#define	NUM_OF_DOMAIN	6
#define NUM_OF_CPU_DOMAIN	(PROFILER_CL2 + 1)

#define BASE_CORE_LIT	0	// 0-3
#define BASE_CORE_MIDL	4	// 4-6
#define BASE_CORE_MIDH	7	// 7-8
#define BASE_CORE_BIG	9	// 9

#define MAXNUM_OF_CPU		10
#define MAXNUM_OF_DVFS		30

#define PROFILER_GET_CPU_BASE_ID(id) ({ \
	int core;                      \
	if (id >= PROFILER_CL2)        \
		core = BASE_CORE_BIG;      \
	else if (id == PROFILER_CL1H)  \
		core = BASE_CORE_MIDH;     \
	else if (id == PROFILER_CL1L)  \
		core = BASE_CORE_MIDL;     \
	else                           \
		core = BASE_CORE_LIT;      \
	core;                          \
})

#define PROFILER_GET_CL_ID(core) ({     \
	int id;                             \
	if (core >= BASE_CORE_BIG)          \
		id = PROFILER_CL2;              \
	else if (core >= BASE_CORE_MIDH)    \
		id = PROFILER_CL1H;             \
	else if (core >= BASE_CORE_MIDL)    \
		id = PROFILER_CL1L;             \
	else                                \
		id = PROFILER_CL0;              \
	id;                                 \
})

#endif /* End __EXYNOS_PROFILER_DT_H */
