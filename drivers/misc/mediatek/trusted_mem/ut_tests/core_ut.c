/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define TMEM_UT_TEST_FMT
#define PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS
#include "private/tmem_pr_fmt.h" PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/slab.h>

#include "private/tmem_error.h"
#include "private/tmem_device.h"
#include "private/tmem_utils.h"
#include "private/ut_entry.h"
#include "private/ut_macros.h"
#include "private/ut_common.h"

#include "private/ut_tests.h"
#include "private/ut_cmd.h"
DEFINE_UT_SUPPORT(tmem_core);

static enum UT_RET_STATE
multiple_region_multiple_thread_alloc(struct ut_params *params)
{
	BEGIN_UT_TEST;

	ASSERT_EQ(0, mem_multi_type_alloc_multithread_test(),
		  "multi mem type multithread alloc test");

	END_UT_TEST;
}

BEGIN_TEST_SUITE(TMEM_UT_CORE_BASE, TMEM_UT_CORE_MAX, tmem_core_ut_run, NULL)
DEFINE_TEST_CASE(TMEM_UT_CORE_MULTIPLE_REGION_MULTIPLE_THREAD_ALLOC,
		 multiple_region_multiple_thread_alloc)
END_TEST_SUITE(NULL)
REGISTER_TEST_SUITE(TMEM_UT_CORE_BASE, TMEM_UT_CORE_MAX, tmem_core_ut_run)
