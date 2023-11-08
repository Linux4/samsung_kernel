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
struct kunit_case i2c_sensor_rear_test_cases[] = {
	KUNIT_CASE(hw_bigdata_i2c_sensor_rear_test),
	{},
};

struct kunit_case i2c_sensor_rear2_test_cases[] = {
	KUNIT_CASE(hw_bigdata_i2c_sensor_rear2_test),
	{},
};

struct kunit_case i2c_sensor_rear3_test_cases[] = {
	KUNIT_CASE(hw_bigdata_i2c_sensor_rear3_test),
	{},
};

struct kunit_case i2c_sensor_rear4_test_cases[] = {
	KUNIT_CASE(hw_bigdata_i2c_sensor_rear4_test),
	{},
};

struct kunit_case i2c_sensor_front_test_cases[] = {
	KUNIT_CASE(hw_bigdata_i2c_sensor_front_test),
	{},
};

struct kunit_case i2c_sensor_front_top_test_cases[] = {
	KUNIT_CASE(hw_bigdata_i2c_sensor_front_top_test),
	{},
};

struct kunit_case i2c_af_rear_test_cases[] = {
	KUNIT_CASE(hw_bigdata_i2c_af_rear_test),
	{},
};

struct kunit_case i2c_af_rear2_test_cases[] = {
	KUNIT_CASE(hw_bigdata_i2c_af_rear2_test),
	{},
};

struct kunit_case i2c_af_rear3_test_cases[] = {
	KUNIT_CASE(hw_bigdata_i2c_af_rear3_test),
	{},
};

struct kunit_case i2c_af_rear4_test_cases[] = {
	KUNIT_CASE(hw_bigdata_i2c_af_rear4_test),
	{},
};

struct kunit_case i2c_af_front_test_cases[] = {
	KUNIT_CASE(hw_bigdata_i2c_af_front_test),
	{},
};

struct kunit_case i2c_af_front_top_test_cases[] = {
	KUNIT_CASE(hw_bigdata_i2c_af_front_top_test),
	{},
};

struct kunit_case i2c_ois_rear_test_cases[] = {
	KUNIT_CASE(hw_bigdata_i2c_ois_rear_test),
	{},
};

struct kunit_case i2c_ois_rear3_test_cases[] = {
	KUNIT_CASE(hw_bigdata_i2c_ois_rear3_test),
	{},
};

struct kunit_case i2c_ois_rear4_test_cases[] = {
	KUNIT_CASE(hw_bigdata_i2c_ois_rear4_test),
	{},
};

struct kunit_case mipi_sensor_rear_test_cases[] = {
	KUNIT_CASE(hw_bigdata_mipi_sensor_rear_test),
	{},
};

struct kunit_case mipi_sensor_rear2_test_cases[] = {
	KUNIT_CASE(hw_bigdata_mipi_sensor_rear2_test),
	{},
};

struct kunit_case mipi_sensor_rear3_test_cases[] = {
	KUNIT_CASE(hw_bigdata_mipi_sensor_rear3_test),
	{},
};

struct kunit_case mipi_sensor_rear4_test_cases[] = {
	KUNIT_CASE(hw_bigdata_mipi_sensor_rear4_test),
	{},
};

struct kunit_case mipi_sensor_front_test_cases[] = {
	KUNIT_CASE(hw_bigdata_mipi_sensor_front_test),
	{},
};

struct kunit_case mipi_sensor_front_top_test_cases[] = {
	KUNIT_CASE(hw_bigdata_mipi_sensor_front_top_test),
	{},
};

struct kunit_case mipi_init_param_test_cases[] = {
	KUNIT_CASE(hw_bigdata_mipi_init_param_test),
	{},
};

struct kunit_case file_test_cases[] = {
	KUNIT_CASE(hw_bigdata_file_test),
	{},
};

struct kunit_case update_rear_module_info_test_cases[] = {
	KUNIT_CASE(eeprom_update_rear_module_info_test),
	{},
};

struct kunit_case update_rear2_module_info_test_cases[] = {
	KUNIT_CASE(eeprom_update_rear2_module_info_test),
	{},
};

struct kunit_case update_rear3_module_info_test_cases[] = {
	KUNIT_CASE(eeprom_update_rear3_module_info_test),
	{},
};

struct kunit_case update_rear4_module_info_test_cases[] = {
	KUNIT_CASE(eeprom_update_rear4_module_info_test),
	{},
};

struct kunit_case update_front_module_info_test_cases[] = {
	KUNIT_CASE(eeprom_update_front_module_info_test),
	{},
};

struct kunit_case rear_match_crc_test_cases[] = {
	KUNIT_CASE(eeprom_rear_match_crc_test),
	{},
};

struct kunit_case rear2_match_crc_test_cases[] = {
	KUNIT_CASE(eeprom_rear2_match_crc_test),
	{},
};

struct kunit_case rear3_match_crc_test_cases[] = {
	KUNIT_CASE(eeprom_rear3_match_crc_test),
	{},
};

struct kunit_case rear4_match_crc_test_cases[] = {
	KUNIT_CASE(eeprom_rear4_match_crc_test),
	{},
};

struct kunit_case front_match_crc_test_cases[] = {
	KUNIT_CASE(eeprom_front_match_crc_test),
	{},
};

struct kunit_case calc_calmap_size_test_cases[] = {
	KUNIT_CASE(eeprom_calc_calmap_size_test),
	{},
};

struct kunit_case get_custom_info_test_cases[] = {
	KUNIT_CASE(eeprom_get_custom_info_test),
	{},
};

struct kunit_case fill_config_info_test_cases[] = {
	KUNIT_CASE(eeprom_fill_config_info_test),
	{},
};

struct kunit_case link_module_info_rear_test_cases[] = {
	KUNIT_CASE(eeprom_link_module_info_rear_test),
	{},
};

struct kunit_case link_module_info_rear2_test_cases[] = {
	KUNIT_CASE(eeprom_link_module_info_rear2_test),
	{},
};

struct kunit_case link_module_info_rear3_test_cases[] = {
	KUNIT_CASE(eeprom_link_module_info_rear3_test),
	{},
};

struct kunit_case link_module_info_rear4_test_cases[] = {
	KUNIT_CASE(eeprom_link_module_info_rear4_test),
	{},
};

struct kunit_case link_module_info_front_test_cases[] = {
	KUNIT_CASE(eeprom_link_module_info_front_test),
	{},
};

struct kunit_case link_module_info_front2_test_cases[] = {
	KUNIT_CASE(eeprom_link_module_info_front2_test),
	{},
};

struct kunit_case link_module_info_front3_test_cases[] = {
	KUNIT_CASE(eeprom_link_module_info_front3_test),
	{},
};

struct kunit_case apply_cdr_value_test_cases[] = {
	KUNIT_CASE(cam_clock_data_recovery_apply_value_test),
	{},
};

struct kunit_case apply_result_value_test_cases[] = {
	KUNIT_CASE(cam_clock_data_recovery_apply_result_test),
	{},
};

struct kunit_case cdr_write_normal_test_cases[] = {
	KUNIT_CASE(cam_clock_data_recovery_write_normal_test),
	{},
};

struct kunit_case cdr_write_overflow1_test_cases[] = {
	KUNIT_CASE(cam_clock_data_recovery_write_overflow1_test),
	{},
};

struct kunit_case cdr_write_overflow2_test_cases[] = {
	KUNIT_CASE(cam_clock_data_recovery_write_overflow2_test),
	{},
};

struct kunit_case cdr_write_invalid_test_cases[] = {
	KUNIT_CASE(cam_clock_data_recovery_write_invalid_test),
	{},
};

struct kunit_case mipi_register_ril_notifier_test_cases[] = {
	KUNIT_CASE(cam_mipi_register_ril_notifier_test),
	{},
};

struct kunit_case mipi_pick_rf_channel_rear_test_cases[] = {
	KUNIT_CASE(cam_mipi_pick_rf_channel_rear_test),
	{},
};

struct kunit_case mipi_pick_rf_channel_rear2_test_cases[] = {
	KUNIT_CASE(cam_mipi_pick_rf_channel_rear2_test),
	{},
};

struct kunit_case mipi_pick_rf_channel_rear3_test_cases[] = {
	KUNIT_CASE(cam_mipi_pick_rf_channel_rear3_test),
	{},
};

struct kunit_case mipi_pick_rf_channel_front_test_cases[] = {
	KUNIT_CASE(cam_mipi_pick_rf_channel_front_test),
	{},
};

struct kunit_case mipi_pick_rf_channel_front_top_test_cases[] = {
	KUNIT_CASE(cam_mipi_pick_rf_channel_front_top_test),
	{},
};

struct kunit_case mipi_get_rfinfo_test_cases[] = {
	KUNIT_CASE(cam_mipi_get_rfinfo_test),
	{},
};

struct kunit_case mipi_get_clock_string_normal_test_cases[] = {
	KUNIT_CASE(cam_mipi_get_clock_string_normal_test),
	{},
};

struct kunit_case mipi_get_clock_string_invalid_test_cases[] = {
	KUNIT_CASE(cam_mipi_get_clock_string_invalid_test),
	{},
};

struct kunit_case cam_sysfs_check_avail_cam_test_cases[] = {
	KUNIT_CASE(cam_sysfs_check_avail_cam_test),
	{},
};

struct kunit_case cam_sysfs_hw_bigdata_node_test_cases[] = {
	KUNIT_CASE(cam_sysfs_hw_bigdata_node_test),
	{},
};

struct kunit_case cam_sysfs_valid_module_test_cases[] = {
	KUNIT_CASE(cam_sysfs_valid_module_test),
	{},
};

struct kunit_case cam_sysfs_invalid_module_test_cases[] = {
	KUNIT_CASE(cam_sysfs_invalid_module_test),
	{},
};

struct kunit_case cam_sysfs_null_module_type1_test_cases[] = {
	KUNIT_CASE(cam_sysfs_null_module_type1_test),
	{},
};

struct kunit_case cam_sysfs_null_module_type2_test_cases[] = {
	KUNIT_CASE(cam_sysfs_null_module_type2_test),
	{},
};

struct kunit_suite cam_kunit_i2c_sensor_rear = {
	.name = "cam_kunit_i2c_sensor_rear_test",
	.init = hw_bigdata_test_init,
	.exit = hw_bigdata_test_exit,
	.test_cases = i2c_sensor_rear_test_cases,
};

struct kunit_suite cam_kunit_i2c_sensor_rear2 = {
	.name = "cam_kunit_i2c_sensor_rear2_test",
	.init = hw_bigdata_test_init,
	.exit = hw_bigdata_test_exit,
	.test_cases = i2c_sensor_rear2_test_cases,
};

struct kunit_suite cam_kunit_i2c_sensor_rear3 = {
	.name = "cam_kunit_i2c_sensor_rear3_test",
	.init = hw_bigdata_test_init,
	.exit = hw_bigdata_test_exit,
	.test_cases = i2c_sensor_rear3_test_cases,
};

struct kunit_suite cam_kunit_i2c_sensor_rear4 = {
	.name = "cam_kunit_i2c_sensor_rear4_test",
	.init = hw_bigdata_test_init,
	.exit = hw_bigdata_test_exit,
	.test_cases = i2c_sensor_rear4_test_cases,
};

struct kunit_suite cam_kunit_i2c_sensor_front = {
	.name = "cam_kunit_i2c_sensor_front_test",
	.init = hw_bigdata_test_init,
	.exit = hw_bigdata_test_exit,
	.test_cases = i2c_sensor_front_test_cases,
};

struct kunit_suite cam_kunit_i2c_sensor_front_top = {
	.name = "cam_kunit_i2c_sensor_front_top_test",
	.init = hw_bigdata_test_init,
	.exit = hw_bigdata_test_exit,
	.test_cases = i2c_sensor_front_top_test_cases,
};

struct kunit_suite cam_kunit_i2c_af_rear = {
	.name = "cam_kunit_i2c_af_rear_test",
	.init = hw_bigdata_test_init,
	.exit = hw_bigdata_test_exit,
	.test_cases = i2c_af_rear_test_cases,
};

struct kunit_suite cam_kunit_i2c_af_rear2 = {
	.name = "cam_kunit_i2c_af_rear2_test",
	.init = hw_bigdata_test_init,
	.exit = hw_bigdata_test_exit,
	.test_cases = i2c_af_rear2_test_cases,
};

struct kunit_suite cam_kunit_i2c_af_rear3 = {
	.name = "cam_kunit_i2c_af_rear3_test",
	.init = hw_bigdata_test_init,
	.exit = hw_bigdata_test_exit,
	.test_cases = i2c_af_rear3_test_cases,
};

struct kunit_suite cam_kunit_i2c_af_rear4 = {
	.name = "cam_kunit_i2c_af_rear4_test",
	.init = hw_bigdata_test_init,
	.exit = hw_bigdata_test_exit,
	.test_cases = i2c_af_rear4_test_cases,
};

struct kunit_suite cam_kunit_i2c_af_front = {
	.name = "cam_kunit_i2c_af_front_test",
	.init = hw_bigdata_test_init,
	.exit = hw_bigdata_test_exit,
	.test_cases = i2c_af_front_test_cases,
};

struct kunit_suite cam_kunit_i2c_af_front_top = {
	.name = "cam_kunit_i2c_af_front_top_test",
	.init = hw_bigdata_test_init,
	.exit = hw_bigdata_test_exit,
	.test_cases = i2c_af_front_top_test_cases,
};

struct kunit_suite cam_kunit_i2c_ois_rear = {
	.name = "cam_kunit_i2c_ois_rear_test",
	.init = hw_bigdata_test_init,
	.exit = hw_bigdata_test_exit,
	.test_cases = i2c_ois_rear_test_cases,
};

struct kunit_suite cam_kunit_i2c_ois_rear3 = {
	.name = "cam_kunit_i2c_ois_rear3_test",
	.init = hw_bigdata_test_init,
	.exit = hw_bigdata_test_exit,
	.test_cases = i2c_ois_rear3_test_cases,
};

struct kunit_suite cam_kunit_i2c_ois_rear4 = {
	.name = "cam_kunit_i2c_ois_rear4_test",
	.init = hw_bigdata_test_init,
	.exit = hw_bigdata_test_exit,
	.test_cases = i2c_ois_rear4_test_cases,
};

struct kunit_suite cam_kunit_mipi_sensor_rear = {
	.name = "cam_kunit_mipi_sensor_rear_test",
	.init = hw_bigdata_test_init,
	.exit = hw_bigdata_test_exit,
	.test_cases = mipi_sensor_rear_test_cases,
};

struct kunit_suite cam_kunit_mipi_sensor_rear2 = {
	.name = "cam_kunit_mipi_sensor_rear2_test",
	.init = hw_bigdata_test_init,
	.exit = hw_bigdata_test_exit,
	.test_cases = mipi_sensor_rear2_test_cases,
};

struct kunit_suite cam_kunit_mipi_sensor_rear3 = {
	.name = "cam_kunit_mipi_sensor_rear3_test",
	.init = hw_bigdata_test_init,
	.exit = hw_bigdata_test_exit,
	.test_cases = mipi_sensor_rear3_test_cases,
};

struct kunit_suite cam_kunit_mipi_sensor_rear4 = {
	.name = "cam_kunit_mipi_sensor_rear4_test",
	.init = hw_bigdata_test_init,
	.exit = hw_bigdata_test_exit,
	.test_cases = mipi_sensor_rear4_test_cases,
};

struct kunit_suite cam_kunit_mipi_sensor_front = {
	.name = "cam_kunit_mipi_sensor_front_test",
	.init = hw_bigdata_test_init,
	.exit = hw_bigdata_test_exit,
	.test_cases = mipi_sensor_front_test_cases,
};

struct kunit_suite cam_kunit_mipi_sensor_front_top = {
	.name = "cam_kunit_mipi_sensor_front_top_test",
	.init = hw_bigdata_test_init,
	.exit = hw_bigdata_test_exit,
	.test_cases = mipi_sensor_front_top_test_cases,
};

struct kunit_suite cam_kunit_mipi_init_param = {
	.name = "cam_kunit_mipi_init_param_test",
	.init = hw_bigdata_test_init,
	.exit = hw_bigdata_test_exit,
	.test_cases = mipi_init_param_test_cases,
};

struct kunit_suite cam_kunit_file = {
	.name = "cam_kunit_file_test",
	.init = hw_bigdata_test_init,
	.exit = hw_bigdata_test_exit,
	.test_cases = file_test_cases,
};

struct kunit_suite cam_kunit_update_rear_module_info = {
	.name = "cam_kunit_update_rear_module_info_test",
	.init = eeprom_test_init,
	.exit = eeprom_test_exit,
	.test_cases = update_rear_module_info_test_cases,
};

struct kunit_suite cam_kunit_update_rear2_module_info = {
	.name = "cam_kunit_update_rear2_module_info_test",
	.init = eeprom_test_init,
	.exit = eeprom_test_exit,
	.test_cases = update_rear2_module_info_test_cases,
};

struct kunit_suite cam_kunit_update_rear3_module_info = {
	.name = "cam_kunit_update_rear3_module_info_test",
	.init = eeprom_test_init,
	.exit = eeprom_test_exit,
	.test_cases = update_rear3_module_info_test_cases,
};

struct kunit_suite cam_kunit_update_rear4_module_info = {
	.name = "cam_kunit_update_rear4_module_info_test",
	.init = eeprom_test_init,
	.exit = eeprom_test_exit,
	.test_cases = update_rear4_module_info_test_cases,
};

struct kunit_suite cam_kunit_update_front_module_info = {
	.name = "cam_kunit_update_front_module_info_test",
	.init = eeprom_test_init,
	.exit = eeprom_test_exit,
	.test_cases = update_front_module_info_test_cases,
};

struct kunit_suite cam_kunit_rear_match_crc = {
	.name = "cam_kunit_rear_match_crc_test",
	.init = eeprom_test_init,
	.exit = eeprom_test_exit,
	.test_cases = rear_match_crc_test_cases,
};

struct kunit_suite cam_kunit_rear2_match_crc = {
	.name = "cam_kunit_rear2_match_crc_test",
	.init = eeprom_test_init,
	.exit = eeprom_test_exit,
	.test_cases = rear2_match_crc_test_cases,
};

struct kunit_suite cam_kunit_rear3_match_crc = {
	.name = "cam_kunit_rear3_match_crc_test",
	.init = eeprom_test_init,
	.exit = eeprom_test_exit,
	.test_cases = rear3_match_crc_test_cases,
};

struct kunit_suite cam_kunit_rear4_match_crc = {
	.name = "cam_kunit_rear4_match_crc_test",
	.init = eeprom_test_init,
	.exit = eeprom_test_exit,
	.test_cases = rear4_match_crc_test_cases,
};

struct kunit_suite cam_kunit_front_match_crc = {
	.name = "cam_kunit_front_match_crc_test",
	.init = eeprom_test_init,
	.exit = eeprom_test_exit,
	.test_cases = front_match_crc_test_cases,
};

struct kunit_suite cam_kunit_calc_calmap_size = {
	.name = "cam_kunit_calc_calmap_size_test",
	.init = eeprom_test_init,
	.exit = eeprom_test_exit,
	.test_cases = calc_calmap_size_test_cases,
};

struct kunit_suite cam_kunit_get_custom_info = {
	.name = "cam_kunit_get_custom_info_test",
	.init = eeprom_test_init,
	.exit = eeprom_test_exit,
	.test_cases = get_custom_info_test_cases,
};

struct kunit_suite cam_kunit_fill_config_info = {
	.name = "cam_kunit_fill_config_info_test",
	.init = eeprom_test_init,
	.exit = eeprom_test_exit,
	.test_cases = fill_config_info_test_cases,
};

struct kunit_suite cam_kunit_link_module_info_rear = {
	.name = "cam_kunit_link_module_info_rear_test",
	.init = eeprom_test_init,
	.exit = eeprom_test_exit,
	.test_cases = link_module_info_rear_test_cases,
};

struct kunit_suite cam_kunit_link_module_info_rear2 = {
	.name = "cam_kunit_link_module_info_rear2_test",
	.init = eeprom_test_init,
	.exit = eeprom_test_exit,
	.test_cases = link_module_info_rear2_test_cases,
};

struct kunit_suite cam_kunit_link_module_info_rear3 = {
	.name = "cam_kunit_link_module_info_rear3_test",
	.init = eeprom_test_init,
	.exit = eeprom_test_exit,
	.test_cases = link_module_info_rear3_test_cases,
};

struct kunit_suite cam_kunit_link_module_info_rear4 = {
	.name = "cam_kunit_link_module_info_rear4_test",
	.init = eeprom_test_init,
	.exit = eeprom_test_exit,
	.test_cases = link_module_info_rear4_test_cases,
};

struct kunit_suite cam_kunit_link_module_info_front = {
	.name = "cam_kunit_link_module_info_front_test",
	.init = eeprom_test_init,
	.exit = eeprom_test_exit,
	.test_cases = link_module_info_front_test_cases,
};

struct kunit_suite cam_kunit_link_module_info_front2 = {
	.name = "cam_kunit_link_module_info_front2_test",
	.init = eeprom_test_init,
	.exit = eeprom_test_exit,
	.test_cases = link_module_info_front2_test_cases,
};

struct kunit_suite cam_kunit_link_module_info_front3 = {
	.name = "cam_kunit_link_module_info_front3_test",
	.init = eeprom_test_init,
	.exit = eeprom_test_exit,
	.test_cases = link_module_info_front3_test_cases,
};

struct kunit_suite cam_kunit_apply_cdr_value = {
	.name = "cam_kunit_apply_cdr_value_test",
	.test_cases = apply_cdr_value_test_cases,
};

struct kunit_suite cam_kunit_apply_result_value = {
	.name = "cam_kunit_apply_result_value_test",
	.test_cases = apply_result_value_test_cases,
};

struct kunit_suite cam_kunit_cdr_write_normal = {
	.name = "cam_kunit_cdr_write_normal_test",
	.test_cases = cdr_write_normal_test_cases,
};

struct kunit_suite cam_kunit_cdr_write_overflow1 = {
	.name = "cam_kunit_cdr_write_overflow1_test",
	.test_cases = cdr_write_overflow1_test_cases,
};

struct kunit_suite cam_kunit_cdr_write_overflow2 = {
	.name = "cam_kunit_cdr_write_overflow2_test",
	.test_cases = cdr_write_overflow2_test_cases,
};

struct kunit_suite cam_kunit_cdr_write_invalid = {
	.name = "cam_kunit_cdr_write_invalid_test",
	.test_cases = cdr_write_invalid_test_cases,
};

struct kunit_suite cam_kunit_mipi_register_ril_notifier = {
	.name = "cam_kunit_mipi_register_ril_notifier_test",
	.init = cam_mipi_test_init,
	.exit = cam_mipi_test_exit,
	.test_cases = mipi_register_ril_notifier_test_cases,
};

struct kunit_suite cam_kunit_mipi_pick_rf_channel_rear = {
	.name = "cam_kunit_mipi_pick_rf_channel_rear_test",
	.init = cam_mipi_test_init,
	.exit = cam_mipi_test_exit,
	.test_cases = mipi_pick_rf_channel_rear_test_cases,
};

struct kunit_suite cam_kunit_mipi_pick_rf_channel_rear2 = {
	.name = "cam_kunit_mipi_pick_rf_channel_rear2_test",
	.init = cam_mipi_test_init,
	.exit = cam_mipi_test_exit,
	.test_cases = mipi_pick_rf_channel_rear2_test_cases,
};

struct kunit_suite cam_kunit_mipi_pick_rf_channel_rear3 = {
	.name = "cam_kunit_mipi_pick_rf_channel_rear3_test",
	.init = cam_mipi_test_init,
	.exit = cam_mipi_test_exit,
	.test_cases = mipi_pick_rf_channel_rear3_test_cases,
};

struct kunit_suite cam_kunit_mipi_pick_rf_channel_front = {
	.name = "cam_kunit_mipi_pick_rf_channel_front_test",
	.init = cam_mipi_test_init,
	.exit = cam_mipi_test_exit,
	.test_cases = mipi_pick_rf_channel_front_test_cases,
};

struct kunit_suite cam_kunit_mipi_pick_rf_channel_front_top = {
	.name = "cam_kunit_mipi_pick_rf_channel_front_top_test",
	.init = cam_mipi_test_init,
	.exit = cam_mipi_test_exit,
	.test_cases = mipi_pick_rf_channel_front_top_test_cases,
};

struct kunit_suite cam_kunit_mipi_get_rfinfo = {
	.name = "cam_kunit_mipi_get_rfinfo_test",
	.init = cam_mipi_test_init,
	.exit = cam_mipi_test_exit,
	.test_cases = mipi_get_rfinfo_test_cases,
};

struct kunit_suite cam_kunit_mipi_get_clock_string_normal = {
	.name = "cam_kunit_mipi_get_clock_string_normal_test",
	.init = cam_mipi_test_init,
	.exit = cam_mipi_test_exit,
	.test_cases = mipi_get_clock_string_normal_test_cases,
};

struct kunit_suite cam_kunit_mipi_get_clock_string_invalid = {
	.name = "cam_kunit_mipi_get_clock_string_invalid_test",
	.init = cam_mipi_test_init,
	.exit = cam_mipi_test_exit,
	.test_cases = mipi_get_clock_string_invalid_test_cases,
};

struct kunit_suite cam_kunit_sysfs_check_avail_cam = {
	.name = "cam_kunit_sysfs_check_avail_cam_test",
	.test_cases = cam_sysfs_check_avail_cam_test_cases,
};

struct kunit_suite cam_kunit_sysfs_hw_bigdata_node = {
	.name = "cam_kunit_sysfs_hw_bigdata_node_test",
	.test_cases = cam_sysfs_hw_bigdata_node_test_cases,
};

struct kunit_suite cam_kunit_sysfs_valid_module = {
	.name = "cam_kunit_sysfs_valid_module_test",
	.test_cases = cam_sysfs_valid_module_test_cases,
};

struct kunit_suite cam_kunit_sysfs_invalid_module = {
	.name = "cam_kunit_sysfs_invalid_module_test",
	.test_cases = cam_sysfs_invalid_module_test_cases,
};

struct kunit_suite cam_kunit_sysfs_null_module_type1 = {
	.name = "cam_kunit_sysfs_null_module_type1_test",
	.test_cases = cam_sysfs_null_module_type1_test_cases,
};

struct kunit_suite cam_kunit_sysfs_null_module_type2 = {
	.name = "cam_kunit_sysfs_null_module_type2_test",
	.test_cases = cam_sysfs_null_module_type2_test_cases,
};

int cam_kunit_hw_bigdata_test(void)
{
	CAM_INFO(CAM_UTIL, "Start");
	
	kunit_run_tests(&cam_kunit_i2c_sensor_rear);
	kunit_run_tests(&cam_kunit_i2c_sensor_rear2);
	kunit_run_tests(&cam_kunit_i2c_sensor_rear3);
	kunit_run_tests(&cam_kunit_i2c_sensor_rear4);
	kunit_run_tests(&cam_kunit_i2c_sensor_front);
	kunit_run_tests(&cam_kunit_i2c_sensor_front_top);

	kunit_run_tests(&cam_kunit_i2c_af_rear);
	kunit_run_tests(&cam_kunit_i2c_af_rear2);
	kunit_run_tests(&cam_kunit_i2c_af_rear3);
	kunit_run_tests(&cam_kunit_i2c_af_rear4);
	kunit_run_tests(&cam_kunit_i2c_af_front);
	kunit_run_tests(&cam_kunit_i2c_af_front_top);

	kunit_run_tests(&cam_kunit_i2c_ois_rear);
	kunit_run_tests(&cam_kunit_i2c_ois_rear3);
	kunit_run_tests(&cam_kunit_i2c_ois_rear4);

	kunit_run_tests(&cam_kunit_mipi_sensor_rear);
	kunit_run_tests(&cam_kunit_mipi_sensor_rear2);
	kunit_run_tests(&cam_kunit_mipi_sensor_rear3);
	kunit_run_tests(&cam_kunit_mipi_sensor_rear4);
	kunit_run_tests(&cam_kunit_mipi_sensor_front);
	kunit_run_tests(&cam_kunit_mipi_sensor_front_top);

	kunit_run_tests(&cam_kunit_mipi_init_param);
	kunit_run_tests(&cam_kunit_file);

	CAM_INFO(CAM_UTIL, "End");

	return 0;
}


int cam_kunit_eeprom_test(void)
{
	CAM_INFO(CAM_UTIL, "Start");
	
	kunit_run_tests(&cam_kunit_update_rear_module_info);
	kunit_run_tests(&cam_kunit_update_rear2_module_info);
	kunit_run_tests(&cam_kunit_update_rear3_module_info);
	kunit_run_tests(&cam_kunit_update_rear4_module_info);
	kunit_run_tests(&cam_kunit_update_front_module_info);

	kunit_run_tests(&cam_kunit_rear_match_crc);
	kunit_run_tests(&cam_kunit_rear2_match_crc);
	kunit_run_tests(&cam_kunit_rear3_match_crc);
	kunit_run_tests(&cam_kunit_rear4_match_crc);
	kunit_run_tests(&cam_kunit_front_match_crc);

	kunit_run_tests(&cam_kunit_calc_calmap_size);
	kunit_run_tests(&cam_kunit_get_custom_info);
	kunit_run_tests(&cam_kunit_fill_config_info);

	kunit_run_tests(&cam_kunit_link_module_info_rear);
	kunit_run_tests(&cam_kunit_link_module_info_rear2);
	kunit_run_tests(&cam_kunit_link_module_info_rear3);
	kunit_run_tests(&cam_kunit_link_module_info_rear4);
	kunit_run_tests(&cam_kunit_link_module_info_front);
	kunit_run_tests(&cam_kunit_link_module_info_front2);
	kunit_run_tests(&cam_kunit_link_module_info_front3);

	CAM_INFO(CAM_UTIL, "End");

	return 0;
}

int cam_kunit_clock_data_recovery_test(void)
{
	CAM_INFO(CAM_UTIL, "Start");
	
	kunit_run_tests(&cam_kunit_apply_cdr_value);
	kunit_run_tests(&cam_kunit_apply_result_value);

	kunit_run_tests(&cam_kunit_cdr_write_normal);
	kunit_run_tests(&cam_kunit_cdr_write_overflow1);
	kunit_run_tests(&cam_kunit_cdr_write_overflow2);
	kunit_run_tests(&cam_kunit_cdr_write_invalid);

	CAM_INFO(CAM_UTIL, "End");

	return 0;
}

int cam_kunit_adaptive_mipi_test(void)
{
	CAM_INFO(CAM_UTIL, "Start");

	kunit_run_tests(&cam_kunit_mipi_register_ril_notifier);
	kunit_run_tests(&cam_kunit_mipi_pick_rf_channel_rear);
	kunit_run_tests(&cam_kunit_mipi_pick_rf_channel_rear2);
	kunit_run_tests(&cam_kunit_mipi_pick_rf_channel_rear3);
	kunit_run_tests(&cam_kunit_mipi_pick_rf_channel_front);
	kunit_run_tests(&cam_kunit_mipi_pick_rf_channel_front_top);
	kunit_run_tests(&cam_kunit_mipi_get_rfinfo);
	kunit_run_tests(&cam_kunit_mipi_get_clock_string_normal);
	kunit_run_tests(&cam_kunit_mipi_get_clock_string_invalid);

	CAM_INFO(CAM_UTIL, "End");
	
	return 0;
}

int cam_kunit_sysfs_hw_bigdata_test(void)
{
	CAM_INFO(CAM_UTIL, "Start");

	kunit_run_tests(&cam_kunit_sysfs_check_avail_cam);
	kunit_run_tests(&cam_kunit_sysfs_hw_bigdata_node);

	kunit_run_tests(&cam_kunit_sysfs_valid_module);
	kunit_run_tests(&cam_kunit_sysfs_invalid_module);
	kunit_run_tests(&cam_kunit_sysfs_null_module_type1);
	kunit_run_tests(&cam_kunit_sysfs_null_module_type2);

	CAM_INFO(CAM_UTIL, "End");
	
	return 0;
}

MODULE_LICENSE("GPL v2");
