#ifndef _CAM_SENSOR_MIPI_TEST_H_
#define _CAM_SENSOR_MIPI_TEST_H_

#include "cam_sensor_mipi.h"

int cam_mipi_test_init(struct kunit *test);
void cam_mipi_test_exit(struct kunit *test);

void cam_mipi_register_ril_notifier_test(struct kunit *test);
void cam_mipi_pick_rf_channel_rear_test(struct kunit *test);
void cam_mipi_pick_rf_channel_rear2_test(struct kunit *test);
void cam_mipi_pick_rf_channel_rear3_test(struct kunit *test);
void cam_mipi_pick_rf_channel_front_test(struct kunit *test);
void cam_mipi_pick_rf_channel_front_top_test(struct kunit *test);
void cam_mipi_get_rfinfo_test(struct kunit *test);
void cam_mipi_get_clock_string_normal_test(struct kunit *test);
void cam_mipi_get_clock_string_invalid_test(struct kunit *test);

#endif /* _CAM_SENSOR_MIPI_TEST_H_ */
