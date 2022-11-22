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

#include "is-vender-test-sensor.h"
#include "is-vender-caminfo.h"

#ifdef USE_SENSOR_TEST_SETTING
#define SENSOR_ID_MAX 400
u32 *globals[SENSOR_ID_MAX];
u32 global_sizes[SENSOR_ID_MAX];
u32 **setfiles[SENSOR_ID_MAX];
u32 *setfile_sizes[SENSOR_ID_MAX];
struct sensor_pll_info_compact_test **pllinfos[SENSOR_ID_MAX];
bool sensor_test_flags[SENSOR_ID_MAX];
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
u32 *global_retentions[SENSOR_ID_MAX];
u32 global_retention_sizes[SENSOR_ID_MAX];
u32 **retentions[SENSOR_ID_MAX];
u32 *retention_sizes[SENSOR_ID_MAX];
u32 **load_srams[SENSOR_ID_MAX];
u32 *load_sram_sizes[SENSOR_ID_MAX];
#endif

const u32 *default_globals[SENSOR_ID_MAX];
u32 default_global_sizes[SENSOR_ID_MAX];
const u32 **default_setfiles[SENSOR_ID_MAX];
const u32 *default_setfile_sizes[SENSOR_ID_MAX];
const struct sensor_pll_info_compact_test **default_pllinfos[SENSOR_ID_MAX];
u32 default_max_setfile_nums[SENSOR_ID_MAX];
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
const u32 *default_global_retentions[SENSOR_ID_MAX];
u32 default_global_retention_sizes[SENSOR_ID_MAX];
const u32 **default_retentions[SENSOR_ID_MAX];
const u32 *default_retention_sizes[SENSOR_ID_MAX];
u32 default_max_retention_nums[SENSOR_ID_MAX];
const u32 **default_load_srams[SENSOR_ID_MAX];
const u32 *default_load_sram_sizes[SENSOR_ID_MAX];
u32 default_max_load_sram_nums[SENSOR_ID_MAX];
#endif

int is_search_sensor_module_with_sensorid(struct is_device_sensor *device,
	u32 sensor_id, struct is_module_enum **module)
{
	int ret = 0;
	u32 mindex, mmax;
	struct is_module_enum *module_enum;
	struct is_resourcemgr *resourcemgr;

	resourcemgr = device->resourcemgr;
	module_enum = device->module_enum;
	*module = NULL;

	if (resourcemgr == NULL) {
		mwarn("resourcemgr is NULL", device);
		return -EINVAL;
	}

	mmax = atomic_read(&device->module_count);
	for (mindex = 0; mindex < mmax; mindex++) {
		*module = &module_enum[mindex];
		if (!(*module)) {
			merr("module is not probed, mindex = %d", device, mindex);
			ret = -EINVAL;
			goto p_err;
		}

		if (module_enum[mindex].sensor_id == sensor_id)
			break;
	}

	if (mindex >= mmax) {
		*module = NULL;
		merr("module(%d) is not found", device, sensor_id);
		ret = -EINVAL;
	}

p_err:
	return ret;
}

bool is_vener_ts_get_sensor_test_flag(u32 sensor_id)
{
	bool ret = false;

	if (sensor_id == -1)
		return ret;

	ret = sensor_test_flags[sensor_id];

	return ret;
}
EXPORT_SYMBOL_GPL(is_vener_ts_get_sensor_test_flag);

int is_vender_ts_set_default_setting_for_test(u32 sensor_id,
	const u32 *d_global,
	u32 d_global_size,
	const u32 **d_setfiles,
	const u32 *d_setfile_sizes,
	void *d_pllinfos,
	u32 d_max_setfile_num)
{
	int ret = 0;
	const struct sensor_pll_info_compact_test **temp_pllinfos;

	if (sensor_id == -1)
		return ret;

	temp_pllinfos = d_pllinfos;
	default_globals[sensor_id] = d_global;
	default_global_sizes[sensor_id] = d_global_size;
	default_setfiles[sensor_id] = d_setfiles;
	default_setfile_sizes[sensor_id] = d_setfile_sizes;
	default_pllinfos[sensor_id] = temp_pllinfos;
	default_max_setfile_nums[sensor_id] = d_max_setfile_num;

	return ret;
}
EXPORT_SYMBOL_GPL(is_vender_ts_set_default_setting_for_test);

int is_vender_ts_get_setting_for_test(u32 sensor_id,
	const u32 **s_global,
	u32 *s_global_size,
	const u32 ***s_setfiles,
	const u32 **s_setfile_sizes,
	void *s_pllinfos)
{
	int ret = 0;
	const struct sensor_pll_info_compact_test ***temp_pllinfos;

	if (sensor_id == -1)
		return ret;

	*s_global = (const u32 *)globals[sensor_id];
	*s_global_size = global_sizes[sensor_id];
	*s_setfiles = (const u32 **)setfiles[sensor_id];
	*s_setfile_sizes = (const u32 *)setfile_sizes[sensor_id];
	temp_pllinfos = (const struct sensor_pll_info_compact_test ***)s_pllinfos;
	*temp_pllinfos = (const struct sensor_pll_info_compact_test **)pllinfos[sensor_id];

	return ret;
}
EXPORT_SYMBOL_GPL(is_vender_ts_get_setting_for_test);

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
int is_vender_ts_set_default_retention_setting_for_test(u32 sensor_id,
	const u32 *d_global,
	u32 d_global_size,
	const u32 **d_retentions,
	const u32 *d_retention_sizes,
	u32 d_max_retention_num,
	const u32 **d_load_srams,
	const u32 *d_load_sram_sizes,
	u32 d_max_load_sram_num)
{
	int ret = 0;

	if (sensor_id == -1)
		return ret;

	default_global_retentions[sensor_id] = d_global;
	default_global_retention_sizes[sensor_id] = d_global_size;
	default_retentions[sensor_id] = d_retentions;
	default_retention_sizes[sensor_id] = d_retention_sizes;
	default_max_retention_nums[sensor_id] = d_max_retention_num;
	default_load_srams[sensor_id] = d_load_srams;
	default_load_sram_sizes[sensor_id] = d_load_sram_sizes;
	default_max_load_sram_nums[sensor_id] = d_max_load_sram_num;

	return ret;
}
EXPORT_SYMBOL_GPL(is_vender_ts_set_default_retention_setting_for_test);

int is_vender_test_sensor_get_retention_setting_for_test(u32 sensor_id,
	const u32 **s_global,
	u32 *s_global_size,
	const u32 ***s_retentions,
	const u32 **s_retention_sizes,
	const u32 ***s_load_srams,
	const u32 **s_load_sram_sizes)
{
	int ret = 0;

	if (sensor_id == -1)
		return ret;

	*s_global = (const u32 *)global_retentions[sensor_id];
	*s_global_size = global_retention_sizes[sensor_id];
	*s_retentions = (const u32 **)retentions[sensor_id];
	*s_retention_sizes = (const u32 *)retention_sizes[sensor_id];
	*s_load_srams = (const u32 **)load_srams[sensor_id];
	*s_load_sram_sizes = (const u32 *)load_sram_sizes[sensor_id];

	return ret;
}
EXPORT_SYMBOL_GPL(is_vender_test_sensor_get_retention_setting_for_test);
#endif

int is_vender_test_sensor_cmd_set_sensor_global_test(void __user *user_data)
{
	int ret = 0;
	struct caminfo_global_test_data global_data;
	u32 sensor_id;
	u32 type;
	u32 **gls;
	u32 *gl_sizes;
	u32 *global = NULL;

	if (copy_from_user((void *)&global_data, user_data, sizeof(struct caminfo_global_test_data))) {
		err("%s : failed to copy data from user", __func__);
		ret = -EINVAL;
		goto EXIT;
	}

	sensor_id = global_data.sensor_id;
	type = global_data.type;

	switch (type) {
	case TEST_TYPE_NORMAL:
		gls = globals;
		gl_sizes = global_sizes;
		break;
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	case TEST_TYPE_RETENTION:
		gls = global_retentions;
		gl_sizes = global_retention_sizes;
		break;
#endif
	default:
		err("%s : type(%d) does not exist", __func__, type);
		ret = -EINVAL;
		goto EXIT;
	}

	global = vmalloc(sizeof(u32) * global_data.size);

	if (copy_from_user((void *)global, global_data.global, sizeof(u32) * global_data.size)) {
		err("%s : failed to copy data from user", __func__);
		ret = -EINVAL;
		goto EXIT_FREE;
	}

	if (gls[sensor_id] != NULL)
		vfree(gls[sensor_id]);

	gls[sensor_id] = global;
	gl_sizes[sensor_id] = global_data.size;
	ret = 0;

	goto EXIT;

EXIT_FREE:
	if (global)
		vfree(global);

EXIT:
	return ret;
}

int is_vender_test_sensor_cmd_get_sensor_global_test(void __user *user_data)
{
	int ret = 0;
	struct caminfo_global_test_data global_data;
	u32 sensor_id = -1;
	u32 type;
	const u32 **gls;
	u32 *gl_sizes;
	u32 *global = NULL;

	if (copy_from_user((void *)&global_data, user_data, sizeof(struct caminfo_global_test_data))) {
		err("%s : failed to copy data from user", __func__);
		ret = -EINVAL;
		goto EXIT;
	}

	sensor_id = global_data.sensor_id;
	type = global_data.type;

	switch (type) {
	case TEST_TYPE_NORMAL:
		gls = default_globals;
		gl_sizes = default_global_sizes;
		break;
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	case TEST_TYPE_RETENTION:
		gls = default_global_retentions;
		gl_sizes = default_global_retention_sizes;
		break;
#endif
	default:
		err("%s : type(%d) does not exist", __func__, type);
		ret = -EINVAL;
		goto EXIT;
	}

	info("[%s] : sensor_id %d", __func__, sensor_id);
	info("[%s] : gls is null %d", __func__, gls == NULL);
	info("[%s] : gls[sensor_id] is null %d", __func__, gls[sensor_id] == NULL);

	if (gls[sensor_id] == NULL) {
		err("%s : failed to get default data", __func__);
		ret = -EINVAL;
		goto EXIT;
	}

	global_data.size = gl_sizes[sensor_id];
	global = vmalloc(sizeof(u32) * GLOBAL_TEST_MAX_SIZE);
	memset(global, 0, sizeof(u32) * GLOBAL_TEST_MAX_SIZE);
	memcpy(global, gls[sensor_id], sizeof(u32) * global_data.size);

	if (copy_to_user(global_data.global, global, sizeof(u32) * global_data.size)) {
		err("%s : failed to copy data to user", __func__);
		ret = -EINVAL;
		goto EXIT_FREE;
	}

	if (copy_to_user(user_data, &global_data, sizeof(struct caminfo_global_test_data))) {
		err("%s : failed to copy data to user", __func__);
		ret = -EINVAL;
		goto EXIT_FREE;
	}

EXIT_FREE:
	if (global)
		vfree(global);

EXIT:
	return ret;
}

int is_vender_test_sensor_cmd_set_sensor_setfile_test(void __user *user_data)
{
	int ret = 0;
	int i;
	struct caminfo_setfile_test_data setfile_data;
	u32 sensor_id;
	u32 type;
	u32 ***sfs;
	u32 **sf_sizes;
	u32 max_size;
	u32 *setfile = NULL;

	if (copy_from_user((void *)&setfile_data, user_data, sizeof(struct caminfo_setfile_test_data))) {
		err("%s : failed to copy data from user", __func__);
		ret = -EINVAL;
		goto EXIT;
	}

	sensor_id = setfile_data.sensor_id;
	type = setfile_data.type;

	switch (type) {
	case TEST_TYPE_NORMAL:
		sfs = setfiles;
		sf_sizes = setfile_sizes;
		max_size = default_max_setfile_nums[sensor_id];
		break;
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	case TEST_TYPE_RETENTION:
		sfs = retentions;
		sf_sizes = retention_sizes;
		max_size = default_max_retention_nums[sensor_id];
		break;
	case TEST_TYPE_LOAD_SRAM:
		sfs = load_srams;
		sf_sizes = load_sram_sizes;
		max_size = default_max_load_sram_nums[sensor_id];
		break;
#endif
	default:
		err("%s : type(%d) does not exist", __func__, type);
		ret = -EINVAL;
		goto EXIT;
	}

	setfile = vmalloc(sizeof(u32) * setfile_data.size);

	if (copy_from_user((void *)setfile, setfile_data.setfile, sizeof(u32) * setfile_data.size)) {
		err("%s : failed to copy data from user", __func__);
		ret = -EINVAL;
		goto EXIT_FREE;
	}

	if (setfile_data.idx == 0) {

		if (sfs[sensor_id] != NULL) {
			for (i = 0; i < max_size; i++)
				vfree(sfs[sensor_id][i]);

			vfree(sfs[sensor_id]);
		}
		sfs[sensor_id] = vmalloc(sizeof(u32 *) * max_size);

		if (sf_sizes[sensor_id] != NULL)
			vfree(sf_sizes[sensor_id]);

		sf_sizes[sensor_id] = vmalloc(sizeof(u32) * max_size);
	}

	sfs[sensor_id][setfile_data.idx] = setfile;
	sf_sizes[sensor_id][setfile_data.idx] = setfile_data.size;

	goto EXIT;
EXIT_FREE:

	if (setfile)
		vfree(setfile);

EXIT:
	return ret;
}

int is_vender_test_sensor_cmd_get_sensor_setfile_test(void __user *user_data)
{
	int ret = 0;
	struct caminfo_setfile_test_data setfile_data;
	u32 sensor_id = -1;
	u32 type;
	const u32 ***sfs;
	const u32 **sf_sizes;
	u32 max_size;
	u32 *setfile = NULL;

	if (copy_from_user((void *)&setfile_data, user_data, sizeof(struct caminfo_setfile_test_data))) {
		err("%s : failed to copy data from user", __func__);
		ret = -EINVAL;
		goto EXIT;
	}

	sensor_id = setfile_data.sensor_id;
	type = setfile_data.type;

	switch (type) {
	case TEST_TYPE_NORMAL:
		sfs = default_setfiles;
		sf_sizes = default_setfile_sizes;
		max_size = default_max_setfile_nums[sensor_id];
		break;
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	case TEST_TYPE_RETENTION:
		sfs = default_retentions;
		sf_sizes = default_retention_sizes;
		max_size = default_max_retention_nums[sensor_id];
		break;
	case TEST_TYPE_LOAD_SRAM:
		sfs = default_load_srams;
		sf_sizes = default_load_sram_sizes;
		max_size = default_max_load_sram_nums[sensor_id];
		break;
#endif
	default:
		err("%s : type(%d) does not exist", __func__, type);
		ret = -EINVAL;
		goto EXIT;
	}

	if (setfile_data.idx >= max_size) {
		ret = 3;//idx out of range
		goto EXIT;
	}

	if (sf_sizes[sensor_id] == NULL
		|| sfs[sensor_id] == NULL
		|| sfs[sensor_id][setfile_data.idx] == NULL) {
		err("%s : failed to get default data", __func__);
		ret = -EINVAL;
		goto EXIT;
	}

	setfile_data.size = sf_sizes[sensor_id][setfile_data.idx];
	setfile = vmalloc(sizeof(u32) * SETFILE_TEST_MAX_SIZE);
	memset(setfile, 0, sizeof(u32) * SETFILE_TEST_MAX_SIZE);
	memcpy(setfile, sfs[sensor_id][setfile_data.idx], sizeof(u32) * setfile_data.size);

	if (copy_to_user(setfile_data.setfile, setfile, sizeof(u32) * setfile_data.size)) {
		err("%s : failed to copy data to user", __func__);
		ret = -EINVAL;
		goto EXIT_FREE;
	}

	if (copy_to_user(user_data, &setfile_data, sizeof(struct caminfo_setfile_test_data))) {
		err("%s : failed to copy data to user", __func__);
		ret = -EINVAL;
		goto EXIT_FREE;
	}

EXIT_FREE:
	if (setfile)
		vfree(setfile);

EXIT:
	return ret;
}

int is_vender_test_sensor_cmd_set_sensor_pllinfo_test(void __user *user_data)
{
	int ret = 0;
	int i;
	struct caminfo_pllinfo_test_data pllinfo_data;
	u32 sensor_id;
	u32 max_size;

	if (copy_from_user((void *)&pllinfo_data, user_data, sizeof(struct caminfo_pllinfo_test_data))) {
		err("%s : failed to copy data from user", __func__);
		ret = -EINVAL;
		goto EXIT;
	}

	sensor_id = pllinfo_data.sensor_id;
	max_size = default_max_setfile_nums[sensor_id];

	if (pllinfo_data.idx == 0) {
		if (pllinfos[sensor_id] != NULL) {
			for (i = 0; i < max_size; i++)
				vfree(pllinfos[sensor_id][i]);

			vfree(pllinfos[sensor_id]);
		}

		pllinfos[sensor_id] = vmalloc(sizeof(struct sensor_pll_info_compact_test *) * max_size);
	}

	if (pllinfos[sensor_id][pllinfo_data.idx] != NULL)
		vfree(pllinfos[sensor_id][pllinfo_data.idx]);

	pllinfos[sensor_id][pllinfo_data.idx] = vmalloc(sizeof(struct sensor_pll_info_compact_test));

	pllinfos[sensor_id][pllinfo_data.idx]->ext_clk = pllinfo_data.ext_clk;
	pllinfos[sensor_id][pllinfo_data.idx]->mipi_datarate = pllinfo_data.mipi_datarate;
	pllinfos[sensor_id][pllinfo_data.idx]->pclk = pllinfo_data.pclk;
	pllinfos[sensor_id][pllinfo_data.idx]->frame_length_lines = pllinfo_data.frame_length_lines;
	pllinfos[sensor_id][pllinfo_data.idx]->line_length_pck = pllinfo_data.line_length_pck;

EXIT:
	return ret;
}

int is_vender_test_sensor_cmd_get_sensor_pllinfo_test(void __user *user_data)
{
	int ret = 0;
	struct caminfo_pllinfo_test_data pllinfo_data;
	u32 sensor_id = -1;

	if (copy_from_user((void *)&pllinfo_data, user_data, sizeof(struct caminfo_pllinfo_test_data))) {
		err("%s : failed to copy data from user", __func__);
		ret = -EINVAL;
		goto EXIT;
	}

	sensor_id = pllinfo_data.sensor_id;
	if (pllinfo_data.idx >= default_max_setfile_nums[sensor_id]) {
		ret = 3;//idx out of range
		goto EXIT;
	}

	if (default_pllinfos[sensor_id] == NULL
		|| default_pllinfos[sensor_id][pllinfo_data.idx] == NULL) {
		err("%s : failed to get default data", __func__);
		ret = -EINVAL;
		goto EXIT;
	}

	pllinfo_data.ext_clk = default_pllinfos[sensor_id][pllinfo_data.idx]->ext_clk;
	pllinfo_data.mipi_datarate = default_pllinfos[sensor_id][pllinfo_data.idx]->mipi_datarate;
	pllinfo_data.pclk = default_pllinfos[sensor_id][pllinfo_data.idx]->pclk;
	pllinfo_data.frame_length_lines = default_pllinfos[sensor_id][pllinfo_data.idx]->frame_length_lines;
	pllinfo_data.line_length_pck = default_pllinfos[sensor_id][pllinfo_data.idx]->line_length_pck;

	if (copy_to_user(user_data, &pllinfo_data, sizeof(struct caminfo_pllinfo_test_data))) {
		err("%s : failed to copy data to user", __func__);
		ret = -EINVAL;
	}

EXIT:
	return ret;
}

int is_vender_test_sensor_cmd_enable_sensor_test(void __user *user_data)
{
	int ret = 0;
	struct caminfo_enable_sensor_test enable_data;
	u32 sensor_id = -1;

	if (copy_from_user((void *)&enable_data, user_data, sizeof(struct caminfo_enable_sensor_test))) {
		err("%s : failed to copy data from user", __func__);
		ret = -EINVAL;
		goto EXIT;
	}

	sensor_id = enable_data.sensor_id;
	sensor_test_flags[sensor_id] = enable_data.enable;

EXIT:
	return ret;
}
#endif
