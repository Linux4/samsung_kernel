/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#ifndef __UT_UTIL_H_
#define __UT_UTIL_H_

#include <linux/printk.h>

#define MOD ("uarthub_ut")
#define UTLOG(format, arg...) pr_info("[%s]: " format "\n", MOD, ## arg)

#define UTLOG_VEB(format, arg...) \
	do { \
		if (g_uh_ut_verbose) \
			UTLOG(format, ## arg); \
	} while (0)

#define ASSERT(expr, fmt, ...) \
	_do_assert(expr, __func__, __FILE__, __LINE__, \
		"Assertion '" #expr "' failed: " #fmt, ## __VA_ARGS__, NULL)
#define EXPECT_INT(X, O, Y, exit_, msg, arg...) \
	do { int x = (X); int y = (Y); int r = 0;\
		r = ASSERT((X O Y), #X == 0x%x; #Y == 0x%x, x, y); \
		if (!r) { \
			UTLOG(msg, ##arg); \
			goto exit_; \
		} \
	} while (0)
#define EXPECT_INT_EQ(X, Y, exit_, msg) EXPECT_INT(X, ==, Y, exit_, msg)
#define EXPECT_INT_NE(X, Y, exit_, msg) EXPECT_INT(X, !=, Y, exit_, msg)
#define EXPECT_INT_LT(X, Y, exit_, msg) EXPECT_INT(X, <,  Y, exit_, msg)
#define EXPECT_INT_LE(X, Y, exit_, msg) EXPECT_INT(X, <=, Y, exit_, msg)
#define EXPECT_INT_GT(X, Y, exit_, msg) EXPECT_INT(X, >,  Y, exit_, msg)
#define EXPECT_INT_GE(X, Y, exit_, msg) EXPECT_INT(X, >=, Y, exit_, msg)


int _do_assert(int result, const char *func, const char *file, int line, const char *errmsg_fmt, ...);

#endif //__UT_UTIL_H_
