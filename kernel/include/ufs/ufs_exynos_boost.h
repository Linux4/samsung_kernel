/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * UFS Boost mode
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef _UFS_EXYNOS_BOOST_H
#define _UFS_EXYNOS_BOOST_H

enum ufs_perf_request_mode {
	REQUEST_BOOST = 0,
	REQUEST_NORMAL,
};

struct ufs_boost_request {
	struct list_head list;
	char *func;
	unsigned int line;
	u64 s_time, e_time;
};

#define ufs_perf_add_boost_mode_request(arg...) do {			\
	__ufs_perf_add_boost_mode_request((char *)__func__, __LINE__, ##arg); \
} while (0)

extern void __ufs_perf_add_boost_mode_request(char *func, unsigned int line,
		struct ufs_boost_request *rq);
extern int ufs_perf_request_boost_mode(struct ufs_boost_request *rq,
		enum ufs_perf_request_mode mode);

#endif
