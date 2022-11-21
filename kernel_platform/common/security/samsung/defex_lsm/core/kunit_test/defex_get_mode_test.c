/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "include/defex_internal.h"

/* Helper methods for testing */

int get_current_ped_features(void)
{
	int ped_features = 0;
#if defined(DEFEX_PED_ENABLE) && defined(DEFEX_PERMISSIVE_PED)
	if (global_privesc_status != 0)
		ped_features |= FEATURE_CHECK_CREDS;
	if (global_privesc_status == 2)
		ped_features |= FEATURE_CHECK_CREDS_SOFT;
#elif defined(DEFEX_PED_ENABLE)
	ped_features |= GLOBAL_PED_STATUS;
#endif /* DEFEX_PERMISSIVE_PED */
	return ped_features;
}

int get_current_safeplace_features(void)
{
	int safeplace_features = 0;
#if defined(DEFEX_SAFEPLACE_ENABLE) && defined(DEFEX_PERMISSIVE_SP)
	if (global_safeplace_status != 0)
		safeplace_features |= FEATURE_SAFEPLACE;
	if (global_safeplace_status == 2)
		safeplace_features |= FEATURE_SAFEPLACE_SOFT;
#elif defined(DEFEX_SAFEPLACE_ENABLE)
	safeplace_features |= GLOBAL_SAFEPLACE_STATUS;
#endif
	return safeplace_features;
}

int get_current_immutable_features(void)
{
	int immutable_features = 0;
#if defined(DEFEX_IMMUTABLE_ENABLE) && defined(DEFEX_PERMISSIVE_IM)
	if (global_immutable_status != 0)
		immutable_features |= FEATURE_IMMUTABLE;
	if (global_immutable_status == 2)
		immutable_features |= FEATURE_IMMUTABLE_SOFT;
#elif defined(DEFEX_IMMUTABLE_ENABLE)
	immutable_features |= GLOBAL_IMMUTABLE_STATUS;
#endif
	return immutable_features;
}

static void defex_get_mode_test(struct test *test)
{
	int expected_features;
#if defined(DEFEX_PED_ENABLE) && defined(DEFEX_PERMISSIVE_PED)
	unsigned int ped_status_backup;
#endif
#if defined DEFEX_SAFEPLACE_ENABLE && defined DEFEX_PERMISSIVE_SP
	unsigned int safeplace_status_backup;
#endif
#if defined DEFEX_IMMUTABLE_ENABLE && defined DEFEX_PERMISSIVE_IM
	unsigned int immutable_status_backup;
#endif

#ifdef DEFEX_PED_ENABLE
	expected_features = 0;
	expected_features |= get_current_safeplace_features();
	expected_features |= get_current_immutable_features();

#ifdef DEFEX_PERMISSIVE_PED
	ped_status_backup = global_privesc_status;

	global_privesc_status = 1;
	expected_features |= FEATURE_CHECK_CREDS;
	EXPECT_EQ(test, defex_get_features(), expected_features);

	global_privesc_status = 2;
	expected_features |= FEATURE_CHECK_CREDS_SOFT;
	EXPECT_EQ(test, defex_get_features(), expected_features);

	global_privesc_status = ped_status_backup;

#else
	expected_features |= GLOBAL_PED_STATUS;
	EXPECT_EQ(test, defex_get_features(), expected_features);

#endif /* DEFEX_PERMISSIVE_PED */
#endif /* DEFEX_PED_ENABLE */
/*-------------------------------------------------------------------*/
#ifdef DEFEX_SAFEPLACE_ENABLE
	expected_features = 0;
	expected_features |= get_current_ped_features();
	expected_features |= get_current_immutable_features();

#ifdef DEFEX_PERMISSIVE_SP
	safeplace_status_backup = global_safeplace_status;

	global_safeplace_status = 1;
	expected_features |= FEATURE_SAFEPLACE;
	EXPECT_EQ(test, defex_get_features(), expected_features);

	global_safeplace_status = 2;
	expected_features |= FEATURE_SAFEPLACE_SOFT;
	EXPECT_EQ(test, defex_get_features(), expected_features);

	global_safeplace_status = safeplace_status_backup;
#else
	expected_features |= GLOBAL_SAFEPLACE_STATUS;
	EXPECT_EQ(test, defex_get_features(), expected_features);
#endif /* DEFEX_PERMISSIVE_SP */
#endif /* DEFEX_SAFEPLACE_ENABLE */
/*-------------------------------------------------------------------*/
#ifdef DEFEX_IMMUTABLE_ENABLE
	expected_features = 0;
	expected_features |= get_current_ped_features();
	expected_features |= get_current_safeplace_features();

#ifdef DEFEX_PERMISSIVE_IM
	immutable_status_backup = global_immutable_status;

	global_immutable_status = 1;
	expected_features |= FEATURE_IMMUTABLE;
	EXPECT_EQ(test, defex_get_features(), expected_features);

	global_immutable_status = 2;
	expected_features |= FEATURE_IMMUTABLE_SOFT;
	EXPECT_EQ(test, defex_get_features(), expected_features);

	global_immutable_status = immutable_status_backup;
#else
	expected_features |= GLOBAL_IMMUTABLE_STATUS;
	EXPECT_EQ(test, defex_get_features(), expected_features);
#endif /* DEFEX_PERMISSIVE_IM */
#endif /* DEFEX_IMMUTABLE_ENABLE */
	SUCCEED(test);
}

static int defex_get_mode_test_init(struct test *test)
{
	return 0;
}

static void defex_get_mode_test_exit(struct test *test)
{
}

static struct test_case defex_get_mode_test_cases[] = {
	/* TEST FUNC DEFINES */
	TEST_CASE(defex_get_mode_test),
	{},
};

static struct test_module defex_get_mode_test_module = {
	.name = "defex_get_mode_test",
	.init = defex_get_mode_test_init,
	.exit = defex_get_mode_test_exit,
	.test_cases = defex_get_mode_test_cases,
};
module_test(defex_get_mode_test_module);

