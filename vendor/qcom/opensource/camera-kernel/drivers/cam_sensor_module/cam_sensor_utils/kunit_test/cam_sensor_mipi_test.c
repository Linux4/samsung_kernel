// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <linux/module.h>
#include "camera_kunit_main.h"
#include "cam_sensor_mipi_test.h"

#define SENSOR_REAR 0x08E3
#define SENSOR_REAR2 0x0564
#define SENSOR_REAR3 0x30B1
#define SENSOR_FRONT 0x34CB
#define SENSOR_FRONT_TOP 0x0471

struct cam_sensor_ctrl_t *mipi_ctrl;
const struct cam_mipi_sensor_mode *cur_mipi_sensor_mode;
extern char mipi_string[20];

int cam_mipi_test_init(struct kunit *test)
{
	mipi_ctrl = kmalloc(sizeof(struct cam_sensor_ctrl_t), GFP_KERNEL);
	mipi_ctrl->sensordata = kmalloc(sizeof(struct cam_sensor_board_info), GFP_KERNEL);
	mipi_ctrl->sensor_mode = 0;

	return 0;
}

void cam_mipi_test_exit(struct kunit *test)
{
	if (mipi_ctrl->sensordata) {
		kfree(mipi_ctrl->sensordata);
		mipi_ctrl->sensordata = NULL;
	}

	if (mipi_ctrl) {
		kfree(mipi_ctrl);
		mipi_ctrl = NULL;
	}
}

void cam_mipi_configure_setting(uint32_t sensor_id)
{
	mipi_ctrl->sensordata->slave_info.sensor_id = sensor_id;
	cam_mipi_init_setting(mipi_ctrl);
	cam_mipi_update_info(mipi_ctrl);

	cur_mipi_sensor_mode = &(mipi_ctrl->mipi_info[0]);

	CAM_INFO(CAM_SENSOR, "[AM_DBG] cur rat : %d", cur_mipi_sensor_mode->mipi_channel->rat_band);
}


void cam_mipi_register_ril_notifier_test(struct kunit *test)
{
	cam_mipi_register_ril_notifier();

	KUNIT_EXPECT_EQ(test, TRUE, TRUE);
}

void cam_mipi_pick_rf_channel_rear_test(struct kunit *test)
{
	if (mipi_ctrl != NULL) {
		cam_mipi_configure_setting(SENSOR_REAR);
		KUNIT_EXPECT_EQ(test, (cur_mipi_sensor_mode->mipi_channel->rat_band > 0), TRUE);
	}
}

void cam_mipi_pick_rf_channel_rear2_test(struct kunit *test)
{
	if (mipi_ctrl != NULL) {
		cam_mipi_configure_setting(SENSOR_REAR2);
		KUNIT_EXPECT_EQ(test, (cur_mipi_sensor_mode->mipi_channel->rat_band > 0), TRUE);
	}
}

void cam_mipi_pick_rf_channel_rear3_test(struct kunit *test)
{
	cam_mipi_configure_setting(SENSOR_REAR3);
	KUNIT_EXPECT_EQ(test, (cur_mipi_sensor_mode->mipi_channel->rat_band > 0), TRUE);
}

void cam_mipi_pick_rf_channel_front_test(struct kunit *test)
{
	if (mipi_ctrl != NULL) {
		cam_mipi_configure_setting(SENSOR_FRONT);
		KUNIT_EXPECT_EQ(test, (cur_mipi_sensor_mode->mipi_channel->rat_band > 0), TRUE);
	}
}

void cam_mipi_pick_rf_channel_front_top_test(struct kunit *test)
{
	if (mipi_ctrl != NULL) {
		cam_mipi_configure_setting(SENSOR_FRONT_TOP);
		KUNIT_EXPECT_EQ(test, (cur_mipi_sensor_mode->mipi_channel->rat_band > 0), TRUE);
	}
}

void cam_mipi_get_rfinfo_test(struct kunit *test)
{
	struct cam_cp_noti_info rf_info;

	get_rf_info(&rf_info);
	CAM_INFO(CAM_SENSOR,
		"[AM_DBG] rat : %d, band : %d, channel : %d",
		rf_info.rat, rf_info.band, rf_info.channel);

	KUNIT_EXPECT_EQ(test, (rf_info.rat > 0), TRUE);
	KUNIT_EXPECT_EQ(test, (rf_info.band > 0), TRUE);
	KUNIT_EXPECT_EQ(test, (rf_info.channel > 0), TRUE);
}

void cam_mipi_get_clock_string_normal_test(struct kunit *test)
{
	if (mipi_ctrl != NULL) {
		cam_mipi_configure_setting(SENSOR_REAR);
		cam_mipi_get_clock_string(mipi_ctrl);

		CAM_INFO(CAM_SENSOR, "[AM_DBG] cam_mipi_get_clock_string : %d", mipi_ctrl->mipi_clock_index_new);
		CAM_INFO(CAM_SENSOR, "[AM_DBG] mipi_string : %s", mipi_string);

		KUNIT_EXPECT_EQ(test, (strlen(mipi_string) > 0), TRUE);
	}
}

void cam_mipi_get_clock_string_invalid_test(struct kunit *test)
{
	if (mipi_ctrl != NULL) {
		cam_mipi_configure_setting(SENSOR_FRONT_TOP);
		cam_mipi_get_clock_string(mipi_ctrl);

		CAM_INFO(CAM_SENSOR, "[AM_DBG] cam_mipi_get_clock_string : %d", mipi_ctrl->mipi_clock_index_new);
		CAM_INFO(CAM_SENSOR, "[AM_DBG] mipi_string : %s", mipi_string);

		KUNIT_EXPECT_EQ(test, (!strncmp(mipi_string, "DUMMY", 5)), TRUE);
	}
}

MODULE_LICENSE("GPL v2");
