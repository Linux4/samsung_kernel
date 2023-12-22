#ifndef _CAM_SYSFS_HW_BIGDATA_TEST_H_
#define _CAM_SYSFS_HW_BIGDATA_TEST_H_

#include "cam_sysfs_hw_bigdata.h"
#include "cam_hw_bigdata.h"

void cam_sysfs_check_avail_cam_test(struct kunit *test);
void cam_sysfs_hw_bigdata_node_test(struct kunit *test);
void cam_sysfs_valid_module_test(struct kunit *test);
void cam_sysfs_invalid_module_test(struct kunit *test);
void cam_sysfs_null_module_type1_test(struct kunit *test);
void cam_sysfs_null_module_type2_test(struct kunit *test);

#endif /* _CAM_SYSFS_HW_BIGDATA_TEST_H_ */
