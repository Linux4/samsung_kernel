/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_UTILL_COMMON_H_
#define _NPU_UTILL_COMMON_H_

#include <linux/ktime.h>
#include <linux/bitmap.h>

#define NPU_BITMAP_NAME_LEN		(16)

#define PWM_COUNTER_DIVIDER         (200)
#define TIMER_MEGA_CLOCK_FREQ        (2449)
#define PWM_COUNTER_TOTAL_USEC(ntick) \
	            (u32)((PWM_COUNTER_DIVIDER * (ntick)) / TIMER_MEGA_CLOCK_FREQ)

struct npu_util_bitmap {
	char				name[NPU_BITMAP_NAME_LEN];
	unsigned long			*bitmap;
	unsigned int			bitmap_size;
	unsigned int			used_size;
	unsigned int			base_bit;
};

s64 npu_get_time_ns(void);
s64 npu_get_time_us(void);
void npu_util_bitmap_dump(struct npu_util_bitmap *map);
int npu_util_bitmap_set_region(struct npu_util_bitmap *map, unsigned int size);
void npu_util_bitmap_clear_region(struct npu_util_bitmap *map,
		unsigned int start, unsigned int size);
int npu_util_bitmap_init(struct npu_util_bitmap *map, const char *name,
		unsigned int size);
void npu_util_bitmap_zero(struct npu_util_bitmap *map);
void npu_util_bitmap_deinit(struct npu_util_bitmap *map);

#endif	/* _NPU_UTILL_COMMON_H_ */
