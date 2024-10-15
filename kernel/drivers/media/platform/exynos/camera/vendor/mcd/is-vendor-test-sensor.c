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
#include "is-vendor-test-sensor.h"
#include "is-vendor-mipi.h"

#ifdef USE_SENSOR_DEBUG
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

static struct cam_cp_noti_cell_infos g_test_cell_infos;

u32 cell_info_offsets[] = {
	offsetof(struct cam_cp_cell_info, rat),
	offsetof(struct cam_cp_cell_info, band),
	offsetof(struct cam_cp_cell_info, channel),
	offsetof(struct cam_cp_cell_info, connection_status),
	offsetof(struct cam_cp_cell_info, bandwidth),
	offsetof(struct cam_cp_cell_info, sinr),
	offsetof(struct cam_cp_cell_info, rsrp),
	offsetof(struct cam_cp_cell_info, rsrq),
	offsetof(struct cam_cp_cell_info, cqi),
	offsetof(struct cam_cp_cell_info, dl_mcs),
	offsetof(struct cam_cp_cell_info, pusch_power),
};

u32 cell_info_sizes[] = {
	sizeof(((struct cam_cp_cell_info *)0)->rat),
	sizeof(((struct cam_cp_cell_info *)0)->band),
	sizeof(((struct cam_cp_cell_info *)0)->channel),
	sizeof(((struct cam_cp_cell_info *)0)->connection_status),
	sizeof(((struct cam_cp_cell_info *)0)->bandwidth),
	sizeof(((struct cam_cp_cell_info *)0)->sinr),
	sizeof(((struct cam_cp_cell_info *)0)->rsrp),
	sizeof(((struct cam_cp_cell_info *)0)->rsrq),
	sizeof(((struct cam_cp_cell_info *)0)->cqi),
	sizeof(((struct cam_cp_cell_info *)0)->dl_mcs),
	sizeof(((struct cam_cp_cell_info *)0)->pusch_power),
};

/* CP Test with ADB Commend */
static int test_set_manual_mipi_cell_infos(const char *val, const struct kernel_param *kp)
{
	int ret = 0;
	int argc;
	char **argv;
	char *command;
	int i;
	int pos;
	int32_t i32tmp;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		err("No argument!");
		return -EINVAL;
	}

	command = argv[0];

	info("[%s] command : %s ", __func__, command);

	if (!strcmp(command, "set")) {
		pos = g_test_cell_infos.num_cell;
		for (i = 1; i < argc; i++) {
			ret |= kstrtoint(argv[i], 0, &i32tmp);
			memcpy((char *)&(g_test_cell_infos.cell_list[pos]) + cell_info_offsets[i-1],
					&i32tmp,
					cell_info_sizes[i - 1]);
		}

		g_test_cell_infos.num_cell++;
	} else if (!strcmp(command, "clear")) {
		memset(&g_test_cell_infos, 0, sizeof(struct cam_cp_noti_cell_infos));
		is_vendor_set_cp_test_cell(false);
	} else if (!strcmp(command, "execute")) {
		is_vendor_set_cp_test_cell_infos(&g_test_cell_infos);
		is_vendor_set_cp_test_cell(true);
	} else {
		err("[%s] command error %s", __func__, command);
		ret = -EINVAL;
	}

	argv_free(argv);
	return ret;
}

static const struct kernel_param_ops param_ops_test_manual_mipi_cell_infos = {
	.set = test_set_manual_mipi_cell_infos,
	.get = NULL,
};

/**
 * Command : adb shell "echo set 1 1 0 1 10000 > /sys/module/fimc_is/parameters/test_manual_mipi_cell_infos"
 * Command : adb shell "echo clear > /sys/module/fimc_is/parameters/test_manual_mipi_cell_infos"
 * Command : adb shell "echo execute > /sys/module/fimc_is/parameters/test_manual_mipi_cell_infos"
 * @param 0 Command to execute (set, clear, execute)
 * @param 1 rat
 * @param 2 band
 * @param 3 channel
 * @param 4 connection_status
 * @param 5 bandwidth
 * @param 6 sinr
 * @param 7 rsrp
 * @param 8 rsrq
 * @param 9 cqi
 * @param 10 dl_mcs
 * @param 11 pusch_power
 */
module_param_cb(test_manual_mipi_cell_infos, &param_ops_test_manual_mipi_cell_infos, NULL, 0644);

/* CP Test with ADB Commend */
static int test_set_auto_mipi_cell_infos(const char *val, const struct kernel_param *kp)
{
	int ret = 0;
	int argc;
	char **argv;
	int i;
	int pos;
	int test_idx;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		err("No argument!");
		return -EINVAL;
	}

	memset(&g_test_cell_infos, 0, sizeof(struct cam_cp_noti_cell_infos));

	for (i = 0; i < argc; i++) {
		ret |= kstrtoint(argv[i], 0, &test_idx);

		info("[%s] apply test cell info index : %d", __func__, test_idx);
		test_idx--; //start with 0
		pos = g_test_cell_infos.num_cell;
		memcpy(&g_test_cell_infos.cell_list[pos],
				&test_cp_cell_infos[test_idx],
				sizeof(struct cam_cp_cell_info));
		g_test_cell_infos.num_cell++;
	}

	is_vendor_set_cp_test_cell_infos(&g_test_cell_infos);
	is_vendor_set_cp_test_cell(true);

	argv_free(argv);
	return ret;
}

static const struct kernel_param_ops param_ops_test_auto_mipi_cell_infos = {
	.set = test_set_auto_mipi_cell_infos,
	.get = NULL,
};

/**
 * Command : adb shell "echo 1 3 6 > /sys/module/fimc_is/parameters/test_auto_mipi_cell_infos"
 * @param 0 test cp cell info index starting with 1
 */
module_param_cb(test_auto_mipi_cell_infos, &param_ops_test_auto_mipi_cell_infos, NULL, 0644);

static int test_set_seamless_mode(const char *val, const struct kernel_param *kp)
{
	int ret, position, argc, hdr, ln, sensor_12bit_state, zoom_ratio;
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

	ret = kstrtouint(argv[1], 0, &hdr);
	info("[%s] hdr mode mode %d", __func__, hdr);

	ret = kstrtouint(argv[2], 0, &ln);
	info("[%s] LN mode %d", __func__, ln);

	ret = kstrtouint(argv[3], 0, &sensor_12bit_state);
	info("[%s] 12bit state %d", __func__, sensor_12bit_state);

	ret = kstrtouint(argv[4], 0, &zoom_ratio);
	info("[%s] zoom_ratio %d", __func__, zoom_ratio);

	is_vendor_get_module_from_position(position, &module);
	WARN_ON(!module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	WARN_ON(!sensor_peri);

	cis = &sensor_peri->cis;
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis->cis_data->cur_hdr_mode = hdr;
	cis->cis_data->cur_lownoise_mode = ln;
	cis->cis_data->cur_12bit_mode = sensor_12bit_state;
	cis->cis_data->cur_remosaic_zoom_ratio = zoom_ratio;

	ret = CALL_CISOPS(cis, cis_update_seamless_mode, cis->subdev);

	argv_free(argv);
	return ret;
}

static int test_get_seamless_mode(char *buffer, const struct kernel_param *kp)
{
	int position, hdr, ln, sensor_12bit_state, zoom_ratio, pos;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_module_enum *module;
	struct is_cis *cis;
	int len = 0;

	for (pos = 0; pos < SENSOR_POSITION_MAX; pos++) {
		is_vendor_get_module_from_position(pos, &module);
		if(!module) continue;

		sensor_peri = (struct is_device_sensor_peri *)module->private_data;
		WARN_ON(!sensor_peri);

		cis = &sensor_peri->cis;
		WARN_ON(!cis);
		WARN_ON(!cis->cis_data);

		if (cis->cis_data->stream_on) {
			position = pos;
			hdr = cis->cis_data->cur_hdr_mode;
			ln = cis->cis_data->cur_lownoise_mode;
			sensor_12bit_state = cis->cis_data->cur_12bit_mode;
			zoom_ratio = cis->cis_data->cur_remosaic_zoom_ratio;

			len += sprintf(buffer + len, "pos(%d) aeb(%d) ln(%d) dt(%d) zr(%d)\n",
				position, hdr, ln, sensor_12bit_state, zoom_ratio);
		}
	}

	return len;
}

static const struct kernel_param_ops param_ops_test_seamless_mode = {
	.set = test_set_seamless_mode,
	.get = test_get_seamless_mode,
};

/**
 * Command : adb shell "echo 0 1 2 3 4 > /sys/module/fimc_is/parameters/test_seamless_mode"
 * @param 0 Select sensor position
 * @param 1 Select hdr mode for AEB
 * @param 2 Select ln mode
 * @param 3 Select sensor 12bit state
 * @param 4 Select cropped remosaic mode(zoom ratio)
 */
module_param_cb(test_seamless_mode, &param_ops_test_seamless_mode, NULL, 0644);
#endif
