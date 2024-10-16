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

#include "is-device-sensor-peri.h"
#include "is-vender-test-sensor.h"
#include "is-vender-caminfo.h"
#include "../../sensor/module_framework/cis/is-cis.h"

#ifdef USE_SENSOR_DEBUG

int is_vender_caminfo_cmd_set_mipi_phy(void __user *user_data)
{
	int ret = 0, i;
	int sensor_position;
	int phy_num, phy_value, phy_index;
	char *token, *token_sub, *token_tmp;
	char *phy_setfile_string;
	struct phy_setfile *phy;
	struct is_cis *cis;
	struct is_device_csi *csi;
	struct is_module_enum *module;
	struct is_device_sensor *sensor;
	struct is_device_sensor_peri *sensor_peri;
	struct caminfo_mipi_phy_data mipi_phy_data;

	if (copy_from_user((void *)&mipi_phy_data, user_data, sizeof(struct caminfo_mipi_phy_data))) {
		err("%s : failed to copy data from user", __func__);
		return -EINVAL;
	}

	phy_setfile_string = vzalloc(MIPI_PHY_DATA_MAX_SIZE * sizeof(char));

	if (copy_from_user((void *)phy_setfile_string, mipi_phy_data.mipi_phy_buf,
			sizeof(uint8_t) * mipi_phy_data.mipi_phy_size)) {
		err("%s : failed to copy data from user", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	sensor_position = mipi_phy_data.position;

	if (is_vendor_get_module_from_position(sensor_position, &module) < 0) {
		err("%s : failed to get vendor module from position", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;

	if (!sensor_peri->subdev_cis) {
		err("%s : subdev_cis is NULL", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	sensor = (struct is_device_sensor *)v4l2_get_subdev_hostdata(sensor_peri->subdev_cis);
	csi = v4l2_get_subdevdata(sensor->subdev_csi);
	cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);

	phy_num = csi->phy_sf_tbl->sz_comm + csi->phy_sf_tbl->sz_lane;

	info("[%s] set mipi phy tune : sensor(%d:%s)", __func__, sensor_position, cis->sensor_info->name);

	token = phy_setfile_string;

	for (i = 0; i < phy_num; i++) {
		if (strstr(token, "comm ") != 0) {
			token_tmp = strstr(token, "comm ");
			token = token_tmp + strlen("comm ");
			token_sub = strsep(&token, " ");
			ret = kstrtoint(token_sub, 10, &phy_index);
			phy = &csi->phy_sf_tbl->sf_comm[phy_index];
		} else if (strstr(token, "lane ") != 0) {
			token_tmp = strstr(token, "lane ");
			token = token_tmp + strlen("lane ");
			token_sub = strsep(&token, " ");
			ret = kstrtoint(token_sub, 10, &phy_index);
			phy = &csi->phy_sf_tbl->sf_lane[phy_index];
		} else {
			break;
		}

		token_sub = strsep(&token, " "); // ignore "<"

		token_sub = strsep(&token, " ");
		ret = sscanf(token_sub, "%x", &phy_value);
		phy->addr = phy_value;

		token_sub = strsep(&token, " ");
		ret = kstrtoint(token_sub, 10, &phy_value);
		phy->start = phy_value;

		token_sub = strsep(&token, " ");
		ret = kstrtoint(token_sub, 10, &phy_value);
		phy->width = phy_value;

		token_sub = strsep(&token, " ");
		ret = sscanf(token_sub, "%x", &phy_value);
		phy->val = phy_value;

		token_sub = strsep(&token, " ");
		ret = kstrtoint(token_sub, 10, &phy_value);
		phy->index = phy_value;

		token_sub = strsep(&token, " ");
		ret = kstrtoint(token_sub, 10, &phy_value);
		phy->max_lane = phy_value;

		token_sub = strsep(&token, " "); // ignore ">"

		token_sub = strsep(&token, " "); // reg name

		info("[%s] set mipi phy tune : %#06x %d %d %#010x %d %d, %s",
			__func__, phy->addr, phy->start, phy->width, phy->val,
			phy->index, phy->max_lane, token_sub);
	}

p_err:
	vfree(phy_setfile_string);
	return ret;
}

int is_vender_caminfo_cmd_get_mipi_phy(void __user *user_data)
{
	int ret = 0, i, j;
	int sensor_position;
	int phy_num;
	char str_val[15];
	char *phy_setfile_string;
	struct phy_setfile *phy;
	struct is_cis *cis;
	struct is_device_csi *csi;
	struct is_module_enum *module;
	struct is_device_sensor *sensor;
	struct is_device_sensor_peri *sensor_peri;
	struct caminfo_mipi_phy_data mipi_phy_data;

	if (copy_from_user((void *)&mipi_phy_data, user_data, sizeof(struct caminfo_mipi_phy_data))) {
		err("%s : failed to copy data from user", __func__);
		return -EINVAL;
	}

	sensor_position = mipi_phy_data.position;

	if (is_vendor_get_module_from_position(sensor_position, &module) < 0)
		return -EINVAL;

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	if (!sensor_peri->subdev_cis)
		return -EINVAL;

	sensor = (struct is_device_sensor *)v4l2_get_subdev_hostdata(sensor_peri->subdev_cis);
	csi = v4l2_get_subdevdata(sensor->subdev_csi);
	cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);

	phy_num = csi->phy_sf_tbl->sz_comm + csi->phy_sf_tbl->sz_lane;

	phy_setfile_string = vzalloc(MIPI_PHY_DATA_MAX_SIZE * sizeof(char));

	strcpy(phy_setfile_string, cis->sensor_info->name);

	strcat(phy_setfile_string, " < addr, bit_start, bit_width, val, index, max_lane >\n");
	for (i = 0; i < phy_num; i++) {
		if (i < csi->phy_sf_tbl->sz_comm) {
			phy = &csi->phy_sf_tbl->sf_comm[i];
			sprintf(str_val, "comm %d", i);
		} else {
			phy = &csi->phy_sf_tbl->sf_lane[i - csi->phy_sf_tbl->sz_comm];
			sprintf(str_val, "lane %d", i - csi->phy_sf_tbl->sz_comm);
		}
		strcat(phy_setfile_string, str_val);

		sprintf(str_val, " < %#06x", phy->addr);
		strcat(phy_setfile_string, str_val);

		sprintf(str_val, " %d", phy->start);
		strcat(phy_setfile_string, str_val);

		sprintf(str_val, " %d", phy->width);
		strcat(phy_setfile_string, str_val);

		sprintf(str_val, " %#010x", phy->val);
		strcat(phy_setfile_string, str_val);

		sprintf(str_val, " %d", phy->index);
		strcat(phy_setfile_string, str_val);

		sprintf(str_val, " %d > ", phy->max_lane);
		strcat(phy_setfile_string, str_val);

		for (j = 0; j < MIPI_PHY_REG_MAX_SIZE; j++) {
			if (caminfo_mipi_phy_reg_list[j].reg == phy->addr) {
				strcat(phy_setfile_string, caminfo_mipi_phy_reg_list[j].reg_name);
				break;
			}
		}

		sprintf(str_val, " \n", phy->max_lane);
		strcat(phy_setfile_string, str_val);
	}

	mipi_phy_data.mipi_phy_size = strlen(phy_setfile_string);

	if (copy_to_user(user_data, &mipi_phy_data, sizeof(struct caminfo_mipi_phy_data))) {
		err("%s : failed to copy data to user", __func__);
		ret = -EINVAL;
	}

	if (copy_to_user(mipi_phy_data.mipi_phy_buf, phy_setfile_string,
			sizeof(char) * strlen(phy_setfile_string))) {
		err("%s : failed to copy data to user", __func__);
		ret  = -EINVAL;
	}

	vfree(phy_setfile_string);

	return ret;
}

/* Sensor Test with ADB Commend */
static int test_set_adaptive_mipi_mode(const char *val, const struct kernel_param *kp)
{
	int ret, position, argc, adaptive_mipi_mode;
	char **argv;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_module_enum *module;
	struct is_cis *cis;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		err("No argument!");
		return -EINVAL;
	}

	ret = kstrtouint(argv[0], 0, &position);
	ret = kstrtouint(argv[1], 0, &adaptive_mipi_mode);

	is_vendor_get_module_from_position(position, &module);
	WARN_ON(!module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	WARN_ON(!sensor_peri);

	cis = &sensor_peri->cis;
	WARN_ON(!cis);

	if (cis->mipi_sensor_mode_size == 0) {
		info("[%s][%d] adaptive mipi is not supported", __func__, position);
	} else {
		info("[%s][%d] adaptive_mipi is %s", __func__,
			position, (adaptive_mipi_mode == 1) ? "disabled" : "enabled");
		if (adaptive_mipi_mode == 1)
			cis->vendor_use_adaptive_mipi = false;
		else
			cis->vendor_use_adaptive_mipi = true;
	}

	argv_free(argv);
	return ret;
}

static const struct kernel_param_ops param_ops_test_mipi_mode = {
	.set = test_set_adaptive_mipi_mode,
	.get = NULL,
};

/**
 * Command : adb shell "echo 0 1 > /sys/module/fimc_is/parameters/test_mipi_mode"
 * @param 0 Select sensor position
 * @param 1 Select adaptive mipi mode : Disable[1]
 */
module_param_cb(test_mipi_mode, &param_ops_test_mipi_mode, NULL, 0644);

static int test_set_seamless_mode(const char *val, const struct kernel_param *kp)
{
	int ret, position, argc, zoom_ratio;
	char **argv;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_module_enum *module;
	struct is_cis *cis;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		err("No argument!");
		return -EINVAL;
	}

	ret = kstrtouint(argv[0], 0, &position);
	info("[%s] sensor position %d", __func__, position);

	ret = kstrtouint(argv[1], 0, &zoom_ratio);
	info("[%s] zoom_ratio %d", __func__, zoom_ratio);

	is_vendor_get_module_from_position(position, &module);
	WARN_ON(!module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	WARN_ON(!sensor_peri);

	cis = &sensor_peri->cis;
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis->cis_data->cur_remosaic_zoom_ratio = zoom_ratio;
	//cis->cis_data->cur_lownoise_mode = ln;

	ret = CALL_CISOPS(cis, cis_update_seamless_mode, cis->subdev);

	argv_free(argv);
	return ret;
}

static const struct kernel_param_ops param_ops_test_seamless_mode = {
	.set = test_set_seamless_mode,
	.get = NULL,
};

/**
 * Command : adb shell "echo 0 1 > /sys/module/fimc_is/parameters/test_seamless_mode"
 * @param 0 Select sensor position
 * @param 1 Select cropped remosaic mode(zoom ratio) : Croped_zoom[20]
 */
module_param_cb(test_seamless_mode, &param_ops_test_seamless_mode, NULL, 0644);
#endif
