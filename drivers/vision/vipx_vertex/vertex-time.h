/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VERTEX_TIME_H__
#define __VERTEX_TIME_H__

#include <linux/time.h>

enum vertex_time_measure_point {
	VERTEX_TIME_QUEUE,
	VERTEX_TIME_REQUEST,
	VERTEX_TIME_RESOURCE,
	VERTEX_TIME_PROCESS,
	VERTEX_TIME_DONE,
	VERTEX_TIME_COUNT
};

struct vertex_time {
	struct timeval		time;
};

void vertex_get_timestamp(struct vertex_time *time);

#define VERTEX_TIME_IN_US(v)	((v).time.tv_sec * 1000000 + (v).time.tv_usec)

#endif
