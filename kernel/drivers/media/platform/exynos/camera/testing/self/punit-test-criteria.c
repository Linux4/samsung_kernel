// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "punit-test-criteria.h"
#include "punit-group-test.h"
#include "punit-test-checker.h"
#include "punit-dvfs-test.h"

#define PUNIT_CRITERIA_MAX 10

struct punit_point_criteria {
	u32 count;
	struct punit_test_criteria criteria[PUNIT_CRITERIA_MAX];
};

#define MAX_RES_QSIZE (PUNIT_CHECK_POINT_MAX * PUNIT_CRITERIA_MAX)
#define MAX_RES_MSIZE 256

static const struct punit_checker_ops *(*type_checker_ops[PUNIT_CHECK_TYPE_MAX])(void) = {
	[PUNIT_DVFS_TEST] = punit_get_dvfs_checker_ops,
};

#define CALL_CHECKER(type, ops, args...)                                                           \
	((type_checker_ops[type]) && (type_checker_ops[type]()->ops) ?                             \
			(type_checker_ops[type]()->ops)(args) : PGT_FAIL)

#define CALL_START_CHECKER(crit, args...) CALL_CHECKER(crit->check_type, start_checker, crit, args)
#define CALL_DQUE_CHECKER(crit, args...) CALL_CHECKER(crit->check_type, dque_checker, crit, args)
#define CALL_STOP_CHECKER(crit, args...) CALL_CHECKER(crit->check_type, stop_checker, crit, args)
#define HAS_CRIT_FORMATTER(type)                                                                   \
	((type_checker_ops[type]) && (type_checker_ops[type]()->result_formatter) ? 1 : 0)
#define CALL_CRIT_FORMATTER(crit, args...)                                                         \
	CALL_CHECKER(crit->check_type, result_formatter, crit, args)

#define ALLOW_DROP_FCOUNT 5
#define CHECK_FCOUNT(check, cur) ((cur) - (check) <= ALLOW_DROP_FCOUNT)
#define CHECK_DQUE_COND(crit, head, frame)                                                         \
	(CHECK_EQ(crit->group_id, head->slot) && CHECK_FCOUNT(crit->fcount, frame->fcount))

static struct punit_point_criteria *point_criteria[IS_STREAM_COUNT][PUNIT_CHECK_POINT_MAX];
static struct {
	char data[MAX_RES_QSIZE][MAX_RES_MSIZE];
	int front;
	int rear;
} result_queue;

static u32 criteria_total_count;

static int punit_clear_criteria(void)
{
	int i, j;

	for (i = 0; i < IS_STREAM_COUNT; ++i) {
		for (j = 0; j < PUNIT_CHECK_POINT_MAX; ++j) {
			kvfree(point_criteria[i][j]);
			point_criteria[i][j] = NULL;
		}
	}

	return 0;
}

void punit_init_criteria(void)
{
	criteria_total_count = 0;
	result_queue.front = result_queue.rear = 0;
	punit_clear_criteria();
}

u32 punit_get_criteria_count(void)
{
	return criteria_total_count;
}

static int punit_add_criteria(u32 instance, u32 check_point, u32 group_id, u32 fcount,
	u32 check_type, u32 check_variable, u32 check_operator, u32 expect_value)
{
	u32 index = 0;
	struct punit_test_criteria *criteria;

	if (ZERO_OR_NULL_PTR(point_criteria[instance][check_point])) {
		point_criteria[instance][check_point] =
			kvzalloc(sizeof(struct punit_point_criteria), GFP_KERNEL);
		if (ZERO_OR_NULL_PTR(point_criteria[instance][check_point])) {
			pr_err("failed to alloc criteria\n");
			return -ENOMEM;
		}
	} else {
		if (point_criteria[instance][check_point]->count == PUNIT_CRITERIA_MAX) {
			pr_err("exceeded the number of available criteria\n");
			return -ENOMEM;
		}
	}

	index = point_criteria[instance][check_point]->count;
	point_criteria[instance][check_point]->count++;

	criteria = &point_criteria[instance][check_point]->criteria[index];
	criteria->instance = instance;
	criteria->check_point = check_point;
	criteria->group_id = group_id;
	criteria->fcount = fcount;
	criteria->check_type = check_type;
	criteria->check_variable = check_variable;
	criteria->check_operator = check_operator;
	criteria->expect_value = expect_value;
	criteria->actual_value = -1;
	criteria->check_result = -1;

	criteria_total_count++;

	return 0;
}

int punit_set_test_criteria_info(int argc, char **argv)
{
	u32 instance;
	u32 check_point;
	u32 group_id;
	u32 fcount;
	u32 check_type;
	u32 check_variable;
	u32 check_operator;
	u32 expect_value;
	int i;
	int ret = 0;
	int arg_i = 1;

	if ((argc < 16) || (argc % 2 == 1)) {
		pr_err("%s: Not enough parameters. %d\n", __func__, argc);
		return -EINVAL;
	}

	ret = kstrtouint(argv[arg_i], 0, &instance);
	if (ret) {
		pr_err("%s: Invalid parameter[%s]\n", __func__, argv[arg_i]);
		return ret;
	}

	if (instance >= IS_STREAM_COUNT) {
		pr_err("%s: Invalid parameters. instance %d >= %d\n", __func__, instance,
			IS_STREAM_COUNT);
		return -EINVAL;
	}

	arg_i++;
	for (i = arg_i; i < argc; i += 2) {
		if (strcmp(argv[i], "-p") == 0) {
			ret = kstrtouint(argv[i + 1], 0, &check_point);
			if (ret) {
				pr_err("%s: Invalid parameter[%s]\n", __func__, argv[i + 1]);
				return ret;
			}

			if (check_point >= PUNIT_CHECK_POINT_MAX) {
				pr_err("%s: Invalid parameters. check_point %d >= %d\n", __func__,
					check_point, PUNIT_CHECK_POINT_MAX);
				return -EINVAL;
			}
		} else if (strcmp(argv[i], "-g") == 0) {
			ret = kstrtouint(argv[i + 1], 0, &group_id);
			if (ret) {
				pr_err("%s: Invalid parameter[%s]\n", __func__, argv[i + 1]);
				return ret;
			}
		} else if (strcmp(argv[i], "-f") == 0) {
			ret = kstrtouint(argv[i + 1], 0, &fcount);
			if (ret) {
				pr_err("%s: Invalid parameter[%s]\n", __func__, argv[i + 1]);
				return ret;
			}
		} else if (strcmp(argv[i], "-t") == 0) {
			ret = kstrtouint(argv[i + 1], 0, &check_type);
			if (ret) {
				pr_err("%s: Invalid parameter[%s]\n", __func__, argv[i + 1]);
				return ret;
			}
		} else if (strcmp(argv[i], "-v") == 0) {
			ret = kstrtouint(argv[i + 1], 0, &check_variable);
			if (ret) {
				pr_err("%s: Invalid parameter[%s]\n", __func__, argv[i + 1]);
				return ret;
			}
		} else if (strcmp(argv[i], "-o") == 0) {
			ret = kstrtouint(argv[i + 1], 0, &check_operator);
			if (ret) {
				pr_err("%s: Invalid parameter[%s]\n", __func__, argv[i + 1]);
				return ret;
			}
		} else if (strcmp(argv[i], "-e") == 0) {
			ret = kstrtouint(argv[i + 1], 0, &expect_value);
			if (ret) {
				pr_err("%s: Invalid parameter[%s]\n", __func__, argv[i + 1]);
				return ret;
			}
		} else {
			pr_err("%s: Invalid parameter[%s]\n", __func__, argv[i]);
			return -EINVAL;
		}
	}

	ret = punit_add_criteria(instance, check_point, group_id, fcount, check_type,
		check_variable, check_operator, expect_value);

	return ret;
}

static int punit_default_res_formatter(struct punit_test_criteria *criteria, char *buffer)
{
	int ret = 0;

	/* TODO : need to append criteria type string or tc name or criteria description etc. */
	ret = sprintf(buffer, "%s [I#%d][G#%d][F#%d] actual : %d / expect : %d",
		CHECK_RES(criteria->check_result), criteria->instance, criteria->group_id,
		criteria->fcount, criteria->actual_value, criteria->expect_value);
	return ret;
}

static int punit_add_criteria_result(struct punit_test_criteria *criteria)
{
	int res;

	if ((result_queue.rear + 1) % MAX_RES_QSIZE == result_queue.front) {
		pr_err("%s: criteria result msg queue is full\n", __func__);
		return -ENOMEM;
	}

	if (HAS_CRIT_FORMATTER(criteria->check_type))
		res = CALL_CRIT_FORMATTER(criteria, result_queue.data[result_queue.rear]);
	else
		res = punit_default_res_formatter(criteria, result_queue.data[result_queue.rear]);

	if (res < 0)
		return -ENOMEM;

	result_queue.rear = (result_queue.rear + 1) % MAX_RES_QSIZE;

	return 0;
}

bool punit_has_remained_criteria_result_msg(void)
{
	if (result_queue.front == result_queue.rear)
		return false;

	return true;
}

int punit_get_remained_criteria_result_msg(char *buffer)
{
	int msg_len;
	int crit_res_size;

	if (!punit_has_remained_criteria_result_msg())
		return 0;

	crit_res_size = (result_queue.rear - result_queue.front + MAX_RES_QSIZE) % MAX_RES_QSIZE;
	msg_len = sprintf(buffer, "...[CRIT#%d] ", (criteria_total_count - crit_res_size));
	msg_len = sprintf(buffer + msg_len, "%s", result_queue.data[result_queue.front]);

	result_queue.front = (result_queue.front + 1) % MAX_RES_QSIZE;
	return msg_len;
}

bool punit_finish_criteria(void)
{
	int i, j, k, num;
	struct punit_test_criteria *criteria;
	bool res = PGT_PASS;

	if (criteria_total_count == 0)
		return PGT_PASS;

	/* fill result message queue and make final result */
	for (i = 0; i < IS_STREAM_COUNT; ++i) {
		for (j = 0; j < PUNIT_CHECK_POINT_MAX; ++j) {
			if (point_criteria[i][j]) {
				num = point_criteria[i][j]->count;
				for (k = 0; k < num; k++) {
					criteria = &point_criteria[i][j]->criteria[k];
					punit_add_criteria_result(criteria);
					res &= (criteria->check_result > 0) ? PGT_PASS : PGT_FAIL;
				}
			}
		}
	}

	punit_clear_criteria();
	return res;
}

static void punit_check_criteria(enum punit_criteria_check_point point, int instance,
	struct is_group *head, struct is_frame *frame)
{
	int i, num;
	struct punit_point_criteria *point_crit;
	struct punit_test_criteria *criteria;

	point_crit = point_criteria[instance][point];
	if (ZERO_OR_NULL_PTR(point_crit))
		return;

	num = point_crit->count;
	for (i = 0; i < num; i++) {
		criteria = &point_crit->criteria[i];
		if (criteria->check_result != PGT_UNCHECKED)
			continue;

		switch (point) {
		case PUNIT_STREAM_START:
			criteria->check_result = CALL_START_CHECKER(criteria, instance);
			break;
		case PUNIT_STREAM_STOP:
			criteria->check_result = CALL_STOP_CHECKER(criteria, instance);
			break;
		case PUNIT_DQBUF:
			if (CHECK_DQUE_COND(criteria, head, frame))
				criteria->check_result = CALL_DQUE_CHECKER(criteria, head, frame);
			break;
		default:
			criteria->check_result = PGT_FAIL;
		}
	}
}

void punit_check_start_criteria(int instance)
{
	punit_check_criteria(PUNIT_STREAM_START, instance, /* head */ NULL, /* frame */ NULL);
}

void punit_check_stop_criteria(int instance)
{
	punit_check_criteria(PUNIT_STREAM_STOP, instance, /* head */ NULL, /* frame */ NULL);
}

void punit_check_dq_criteria(struct is_group *head, struct is_frame *frame)
{
	if (ZERO_OR_NULL_PTR(head) || ZERO_OR_NULL_PTR(frame))
		return;

	punit_check_criteria(PUNIT_DQBUF, head->instance, head, frame);
}
