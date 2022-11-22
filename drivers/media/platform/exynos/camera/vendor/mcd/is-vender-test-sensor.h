/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_VENDER_TEST_SENSOR_H
#define IS_VENDER_TEST_SENSOR_H

#ifdef USE_SENSOR_TEST_SETTING

int is_vender_test_sensor_cmd_set_sensor_global_test(void __user *user_data);
int is_vender_test_sensor_cmd_get_sensor_global_test(void __user *user_data);
int is_vender_test_sensor_cmd_set_sensor_setfile_test(void __user *user_data);
int is_vender_test_sensor_cmd_get_sensor_setfile_test(void __user *user_data);
int is_vender_test_sensor_cmd_set_sensor_pllinfo_test(void __user *user_data);
int is_vender_test_sensor_cmd_get_sensor_pllinfo_test(void __user *user_data);
int is_vender_test_sensor_cmd_enable_sensor_test(void __user *user_data);

bool is_vener_ts_get_sensor_test_flag(u32 sensor_id);
int is_vender_ts_set_default_setting_for_test(u32 sensor_id,
	const u32 *d_global,
	u32 d_global_size,
	const u32 **d_setfiles,
	const u32 *d_setfile_sizes,
	void *d_pllinfos,
	u32 d_max_setfile_num);
int is_vender_ts_get_setting_for_test(u32 sensor_id,
	const u32 **s_global,
	u32 *s_global_size,
	const u32 ***s_setfiles,
	const u32 **s_setfile_sizes,
	void *s_pllinfos);
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
int is_vender_ts_set_default_retention_setting_for_test(u32 sensor_id,
	const u32 *d_global,
	u32 d_global_size,
	const u32 **d_retentions,
	const u32 *d_retention_sizes,
	u32 d_max_retention_num,
	const u32 **d_load_srams,
	const u32 *d_load_sram_sizes,
	u32 d_max_load_sram_num);
int is_vender_test_sensor_get_retention_setting_for_test(u32 sensor_id,
	const u32 **s_global,
	u32 *s_global_size,
	const u32 ***s_retentions,
	const u32 **s_retention_sizes,
	const u32 ***s_load_srams,
	const u32 **s_load_sram_sizes);
#endif
#endif

#endif /* IS_VENDER_TEST_SENSOR_H */
