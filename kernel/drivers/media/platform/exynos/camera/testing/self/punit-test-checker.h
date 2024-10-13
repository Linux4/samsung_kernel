/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PUNIT_TEST_CHECKER_H
#define PUNIT_TEST_CHECKER_H

#include "punit-test-criteria.h"

#define CHECK_LT(x, y) ((x) < (y))
#define CHECK_GT(x, y) ((x) > (y))
#define CHECK_LE(x, y) ((x) <= (y))
#define CHECK_GE(x, y) ((x) >= (y))
#define CHECK_EQ(x, y) ((x) == (y))
#define CHECK_NE(x, y) ((x) != (y))

#define CHECK_EQ_NULL(x) (!(x))
#define CHECK_NE_NULL(x) ((x) ? 1 : 0)
#define CHECK_EQ_STR(x, y) (!strncmp((x), (y), strlen(y)))

#define PUNIT_EXPECT(op, x, y)									\
	((op == EQ)	? CHECK_EQ(x, y) :							\
	(op == NE)	? CHECK_NE(x, y) :							\
	(op == LT)	? CHECK_LT(x, y) :							\
	(op == GT)	? CHECK_GT(x, y) :							\
	(op == LE)	? CHECK_LE(x, y) :							\
	(op == GE)	? CHECK_GE(x, y) :							\
	(op == NOTNULL) ? CHECK_NE_NULL(x) :							\
	(op == ISNULL)	? CHECK_EQ_NULL(x) :							\
			  CHECK_EQ(x, y))

#define FNAME(name) (#name)
#define CHECK_RES(res) (res > 0) ? "Pass" : "Fail"

typedef int (*punit_stream_start_checker_t)(struct punit_test_criteria *criteria, int instance);
typedef int (*punit_dque_checker_t)(
	struct punit_test_criteria *criteria, struct is_group *head, struct is_frame *frame);
typedef int (*punit_stream_stop_checker_t)(struct punit_test_criteria *criteria, int instance);
typedef int (*punit_checker_result_formatter_t)(struct punit_test_criteria *criteria, char *buffer);

struct punit_checker_ops {
	punit_stream_start_checker_t start_checker;
	punit_dque_checker_t dque_checker;
	punit_stream_stop_checker_t stop_checker;
	punit_checker_result_formatter_t result_formatter;
};

#endif /* PUNIT_TEST_CHECKER_H */
