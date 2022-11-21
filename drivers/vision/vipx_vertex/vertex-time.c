/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/ktime.h>

#include "vertex-log.h"
#include "vertex-time.h"

void vertex_get_timestamp(struct vertex_time *time)
{
	do_gettimeofday(&time->time);
}
