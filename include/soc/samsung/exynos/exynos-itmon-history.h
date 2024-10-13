/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS IPs Traffic Monitor History for Samsung EXYNOS SoC
 * By Myungsu Cha (myung-su.cha@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef EXYNOS_ITMON_HISTORY__H
#define EXYNOS_ITMON_HISTORY__H

#define ITMON_HISTROY_STRING_SIZE 50

struct itmon_history {
	unsigned int magic;
	int idx;
	int nr_entry;
	char data[][50];
} __attribute__((packed));

#endif

