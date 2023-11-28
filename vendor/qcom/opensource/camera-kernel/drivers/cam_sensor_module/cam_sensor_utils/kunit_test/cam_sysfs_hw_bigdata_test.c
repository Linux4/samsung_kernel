// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <linux/module.h>
#include "camera_kunit_main.h"
#include "cam_sysfs_hw_bigdata_test.h"

#define SYSFS_MODULE_ID_INVALID 0
#define SYSFS_MODULE_ID_VALID 1
#define SYSFS_MODULE_ID_ERR_CHAR -1
#define SYSFS_MODULE_ID_ERR_CNT_MAX -2
#define SYSFS_FROM_MODULE_ID_SIZE 10

extern uint8_t module_id[MAX_EEP_CAMID][FROM_MODULE_ID_SIZE + 1];

void cam_sysfs_check_avail_cam_test(struct kunit *test)
{
	struct cam_hw_param *hw_param = NULL;
	
	camera_hw_param_check_avail_cam();

	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_REAR);
	CAM_INFO(CAM_SENSOR, "Rear CAM Support : %s", (hw_param->cam_available == 1) ? "YES" : "NO");
	KUNIT_EXPECT_EQ(test, (hw_param->cam_available == 1), TRUE);

	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_REAR2);
	CAM_INFO(CAM_SENSOR, "Rear2 CAM Support : %s", (hw_param->cam_available == 1) ? "YES" : "NO");
	KUNIT_EXPECT_EQ(test, (hw_param->cam_available == 1), TRUE);

	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_REAR3);
	CAM_INFO(CAM_SENSOR, "Rear3 CAM Support : %s", (hw_param->cam_available == 1) ? "YES" : "NO");
	KUNIT_EXPECT_EQ(test, (hw_param->cam_available == 1), TRUE);

	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_REAR4);
	CAM_INFO(CAM_SENSOR, "Rear4 CAM Support : %s", (hw_param->cam_available == 1) ? "YES" : "NO");
	KUNIT_EXPECT_EQ(test, (hw_param->cam_available == 1), TRUE);

	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_FRONT);
	CAM_INFO(CAM_SENSOR, "Front CAM Support : %s", (hw_param->cam_available == 1) ? "YES" : "NO");
	KUNIT_EXPECT_EQ(test, (hw_param->cam_available == 1), TRUE);

	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_FRONT2);
	CAM_INFO(CAM_SENSOR, "Front2 CAM Support : %s", (hw_param->cam_available == 1) ? "YES" : "NO");	
	KUNIT_EXPECT_EQ(test, (hw_param->cam_available == 0), TRUE);
}

void cam_sysfs_hw_bigdata_node_test(struct kunit *test)
{
	struct cam_hw_param *hw_param = NULL;
	ssize_t ret;
	char *buf = NULL;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);

	if (buf != NULL) {
		hw_bigdata_get_hw_param(&hw_param, HW_PARAM_REAR);
		ret = fill_hw_bigdata_sysfs_node(buf, hw_param, module_id[EEP_REAR], HW_PARAM_REAR);

		CAM_INFO(CAM_SENSOR, "rear_hwparam : %d", ret);
		KUNIT_EXPECT_EQ(test, (ret > 0), TRUE);

#if defined(CONFIG_SAMSUNG_REAR_DUAL)
		hw_bigdata_get_hw_param(&hw_param, HW_PARAM_REAR2);
		ret = fill_hw_bigdata_sysfs_node(buf, hw_param, module_id[EEP_REAR2], HW_PARAM_REAR2);

		CAM_INFO(CAM_SENSOR, "rear2_hwparam : %d", ret);
		KUNIT_EXPECT_EQ(test, (ret > 0), TRUE);
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
		hw_bigdata_get_hw_param(&hw_param, HW_PARAM_REAR3);
		ret = fill_hw_bigdata_sysfs_node(buf, hw_param, module_id[EEP_REAR3], HW_PARAM_REAR3);

		CAM_INFO(CAM_SENSOR, "rear3_hwparam : %d", ret);
		KUNIT_EXPECT_EQ(test, (ret > 0), TRUE);
#endif
#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
		hw_bigdata_get_hw_param(&hw_param, HW_PARAM_REAR4);
		ret = fill_hw_bigdata_sysfs_node(buf, hw_param, module_id[EEP_REAR4], HW_PARAM_REAR4);

		CAM_INFO(CAM_SENSOR, "rear4_hwparam : %d", ret);
		KUNIT_EXPECT_EQ(test, (ret > 0), TRUE);
#endif
#endif
		hw_bigdata_get_hw_param(&hw_param, HW_PARAM_FRONT);
		ret = fill_hw_bigdata_sysfs_node(buf, hw_param, module_id[EEP_FRONT], HW_PARAM_FRONT);

		CAM_INFO(CAM_SENSOR, "front_hwparam : %d", ret);
		KUNIT_EXPECT_EQ(test, (ret > 0), TRUE);

#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
		hw_bigdata_get_hw_param(&hw_param, HW_PARAM_FRONT2);
		ret = fill_hw_bigdata_sysfs_node(buf, hw_param, module_id[EEP_FRONT2], HW_PARAM_FRONT2);

		CAM_INFO(CAM_SENSOR, "front2_hwparam : %d", ret);
		KUNIT_EXPECT_EQ(test, (ret > 0), TRUE);
#endif
		kfree(buf);
		buf = NULL;
	}
}

void cam_sysfs_valid_module_test(struct kunit *test)
{
	int rc = 0;
	char moduleid[SYSFS_FROM_MODULE_ID_SIZE] = "AB012";
	rc = is_hw_param_valid_module_id(moduleid);

	KUNIT_EXPECT_EQ(test, (rc == SYSFS_MODULE_ID_VALID), TRUE);
}

void cam_sysfs_invalid_module_test(struct kunit *test)
{
	int rc = 0;
	char moduleid[SYSFS_FROM_MODULE_ID_SIZE] = "ab012";
	rc = is_hw_param_valid_module_id(moduleid);

	KUNIT_EXPECT_EQ(test, (rc == SYSFS_MODULE_ID_ERR_CHAR), TRUE);
}

void cam_sysfs_null_module_type1_test(struct kunit *test)
{
	int rc = 0;
	char moduleid[SYSFS_FROM_MODULE_ID_SIZE] = "";
	rc = is_hw_param_valid_module_id(moduleid);

	KUNIT_EXPECT_EQ(test, (rc == SYSFS_MODULE_ID_ERR_CNT_MAX), TRUE);
}

void cam_sysfs_null_module_type2_test(struct kunit *test)
{
	int rc = 0;
	char *moduleid = NULL;
	rc = is_hw_param_valid_module_id(moduleid);

	KUNIT_EXPECT_EQ(test, (rc == SYSFS_MODULE_ID_INVALID), TRUE);
}

MODULE_LICENSE("GPL v2");
