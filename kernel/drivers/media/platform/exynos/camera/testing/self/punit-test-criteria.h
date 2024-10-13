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

#ifndef PUNIT_TEST_CRITERIA_H
#define PUNIT_TEST_CRITERIA_H

#include <linux/types.h>
#include "is-groupmgr.h"

enum punit_criteria_check_point {
	PUNIT_STREAM_START,
	PUNIT_DQBUF,
	PUNIT_STREAM_STOP,
	PUNIT_CHECK_POINT_MAX,
};

enum punit_criteria_check_type {
	PUNIT_DVFS_TEST,
	PUNIT_CHECK_TYPE_MAX,
};

enum punit_criteria_operator {
	EQ,
	NE,
	LT,
	GT,
	LE,
	GE,
	ISNULL,
	NOTNULL,
};

enum punit_criteria_dvfs_variable {
	PUNIT_DVFS_VAR_STATIC,
	PUNIT_DVFS_VAR_DYNAMIC,
	PUNIT_DVFS_VAR_MAX,
};

struct punit_test_criteria {
	u32 check_point;
	u32 instance;
	u32 group_id;
	u32 fcount;
	u32 check_type;
	u32 check_variable;
	u32 check_operator;
	u32 expect_value;
	int actual_value;
	int check_result;
	char desc[256];
};

int punit_set_test_criteria_info(int argc, char **argv);
void punit_init_criteria(void);
u32 punit_get_criteria_count(void);
bool punit_finish_criteria(void);

void punit_check_start_criteria(int instance);
void punit_check_stop_criteria(int instance);
void punit_check_dq_criteria(struct is_group *head, struct is_frame *frame);

bool punit_has_remained_criteria_result_msg(void);
int punit_get_remained_criteria_result_msg(char *buffer);

#endif /* PUNIT_TEST_CRITERIA_H */
