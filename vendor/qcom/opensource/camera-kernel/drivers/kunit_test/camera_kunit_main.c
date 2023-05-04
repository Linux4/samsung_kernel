// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */
#include <linux/module.h>
#include "camera_kunit_main.h"
#include "cam_sensor_util.h"

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
struct kunit_case cam_kunit_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
#ifdef CONFIG_UML
	/* NOTE: UML TC */
	KUNIT_CASE(cam_ois_mcu_test_bar),
#else
	/* NOTE: Target running TC */
	KUNIT_CASE(cam_ois_mcu_test),
#endif
	{},
};

struct kunit_suite cam_kunit_test_module = {
	.name = "cam_kunit_test",
	.test_cases = cam_kunit_test_cases,
};
EXPORT_SYMBOL_KUNIT(cam_kunit_test_module);

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
//kunit_test_suites(&cam_kunit_test_module);

int cam_kunit_init(void)
{
	CAM_INFO(CAM_OIS, "cam_kunit_init");
	kunit_run_tests(&cam_kunit_test_module);
	return 0;
}

void cam_kunit_exit(void)
{
	CAM_INFO(CAM_OIS, "cam_kunit_exit");
}

MODULE_LICENSE("GPL v2");
