/*
* Samsung debugging features for Samsung's SoC's.
*
* Copyright (c) 2014-2017 Samsung Electronics Co., Ltd.
*      http://www.samsung.com
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*/

#ifndef SEC_PERF_H
#define SEC_PERF_H

#include <linux/kernel.h>

#ifdef CONFIG_SEC_PERF_LATENCYCHECKER
extern void sec_perf_latencychecker_check_latency_other_cpu(void);
extern void sec_perf_latencychecker_stop(void);
extern int sec_perf_latencychecker_enable(unsigned int cpu);
extern int sec_perf_latencychecker_disable(unsigned int cpu);
#else
#define sec_perf_latencychecker_check_latency_other_cpu() do { } while (0)
#define sec_perf_latencychecker_stop() do { } while (0)
#define sec_perf_latencychecker_enable(a) do { } while (0)
#define sec_perf_latencychecker_disable(a) do { } while (0)
#endif
#endif
