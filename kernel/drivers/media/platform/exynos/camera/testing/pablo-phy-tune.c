// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <videodev2_exynos_camera.h>

#include "is-core.h"
#include "is-device-sensor.h"
#include "pablo-phy-tune.h"

/*
 * @phy self test(use it for auto tune)
 */
#define PHY_LANE_OFFSET		0x100
#define PHY_ADDR_SD_DESKEW_CON0	0x40
#define PHY_ADDR_SD_DESKEW_CON2	0x48
#define PHY_ADDR_SD_CRC_CON0	0x60

#define SKEW_CAL_CLK_START_POS	0
#define SKEW_CAL_DATA_START_POS	8

static u32 open_window;
static u32 total_err;
static u32 first_open;

static int pablo_test_get_phy_tune(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_phy_tune = {
	.set = pablo_test_set_phy_tune,
	.get = pablo_test_get_phy_tune,
};

static int pablo_test_set_rt_phy_tune(const char *val, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_rt_phy_tune = {
	.set = pablo_test_set_rt_phy_tune,
};

module_param_cb(test_phy_tune, &pablo_param_ops_phy_tune, NULL, 0644);
module_param_cb(test_rt_phy_tune, &pablo_param_ops_rt_phy_tune, NULL, 0644);

extern struct is_device_sensor *sensor;

static int sensor_stream_on(struct v4l2_subdev *sd_csi, struct v4l2_subdev *sd_mod)
{
	int ret;

	/* link */
	ret = v4l2_subdev_call(sd_csi, video, s_stream, IS_ENABLE_LINK_ONLY);
	if (ret) {
		err("fail CSIS stream on(%d)", ret);
		return ret;
	}

	/* sensor */
	ret = v4l2_subdev_call(sd_mod, video, s_stream, true);
	if (ret) {
		err("fail sensor stream on(%d)", ret);
		return ret;
	}

	return 0;
}

static int sensor_stream_off(struct v4l2_subdev *sd_csi, struct v4l2_subdev *sd_mod)
{
	int ret;

	/* sensor */
	ret = v4l2_subdev_call(sd_mod, video, s_stream, false);
	if (ret) {
		err("fail sensor stream off(%d)", ret);
		return ret;
	}

	/* link */
	ret = v4l2_subdev_call(sd_csi, video, s_stream, IS_DISABLE_STREAM);
	if (ret) {
		err("fail CSIS stream off(%d)", ret);
		return ret;
	}

	return 0;
}

static void __skew_code_sweep(u32 num_skew_code, u32 skew_test_time, u32 max_lane,
			struct is_device_csi *csi, struct v4l2_subdev *sd_csi,
			struct v4l2_subdev *sd_mod, void __iomem *regs,
			u32 start)
{
	struct phy_setfile sf_info;
	u32 i, lane;
	u32 tmp_open = 0, tmp_open_window = 0;
	u32 cont_err = 0, cont_end = 11; /* check continuous error */

	sf_info.addr = PHY_ADDR_SD_DESKEW_CON2;
	sf_info.start = start;
	sf_info.width = 5;

	for (i = 0; i < num_skew_code; i++) {
		sf_info.val = i;

		for (lane = 0; lane < max_lane; lane++)
			update_bits(regs + (lane * PHY_LANE_OFFSET) + sf_info.addr,
					sf_info.start, sf_info.width, sf_info.val);

		sensor_stream_on(sd_csi, sd_mod);
		fsleep(skew_test_time * 1000);
		sensor_stream_off(sd_csi, sd_mod);

		info("%s[SKEW_CAL_%s_CODE: %d] s:%d, e:%d, err:%d\n", __func__,
			start ? "DATA" : "CLK", i, csi->state_cnt.str,
			csi->state_cnt.end, csi->state_cnt.err);

		if (csi->state_cnt.err) {
			total_err += csi->state_cnt.err;
			cont_err++;
			tmp_open = 0;
		} else {
			cont_end = 2;
			if (csi->state_cnt.str && csi->state_cnt.end) {
				if (++tmp_open > tmp_open_window)
					tmp_open_window = tmp_open;

				cont_err = 0;
			} else {
				err("invalid state");
			}
		}

		if (cont_err >= cont_end)
			break;
	}

	if (start)
		first_open = tmp_open_window;

	open_window += tmp_open_window;

	sf_info.val = 0;
	for (lane = 0; lane < max_lane; lane++)
		update_bits(regs + (lane * PHY_LANE_OFFSET) + sf_info.addr,
				sf_info.start, sf_info.width, sf_info.val);
}

static void skew_code_sweep(u32 num_skew_code, u32 skew_test_time, u32 max_lane,
			struct is_device_csi *csi, struct v4l2_subdev *sd_csi,
			struct v4l2_subdev *sd_mod, void __iomem *regs)
{
	struct phy_setfile sf_info;
	u32 lane;

	/* Enable SKEW_CAL */
	sf_info.addr = PHY_ADDR_SD_DESKEW_CON0;
	sf_info.start = 0;
	sf_info.width = 1;
	sf_info.val = 0;

	for (lane = 0; lane < max_lane; lane++)
		update_bits(regs + (lane * PHY_LANE_OFFSET) + sf_info.addr,
				sf_info.start, sf_info.width, sf_info.val);

	/* Sweep SKEW_CAL_DATA_CODE */
	__skew_code_sweep(num_skew_code, skew_test_time, max_lane, csi, sd_csi,
			sd_mod, regs, SKEW_CAL_DATA_START_POS);

	/* Sweep SKEW_CAL_CLK_CODE */
	__skew_code_sweep(num_skew_code, skew_test_time, max_lane, csi, sd_csi,
			sd_mod, regs, SKEW_CAL_CLK_START_POS);

	sf_info.val = 1;
	for (lane = 0; lane < max_lane; lane++)
		update_bits(regs + (lane * PHY_LANE_OFFSET) + sf_info.addr,
				sf_info.start, sf_info.width, sf_info.val);
}

static void crc_code_sweep(u32 num_crc_code, u32 crc_test_time, u32 max_lane,
			struct is_device_csi *csi, struct v4l2_subdev *sd_csi,
			struct v4l2_subdev *sd_mod, void __iomem *regs)
{
	struct phy_setfile sf_info;
	u32 i, lane;
	u32 tmp_open = 0, tmp_first;
	u32 cont_err = 0, cont_end = 21; /* check continuous error */

	/* Sweep CRC_FORCE_CODE */
	sf_info.addr = PHY_ADDR_SD_CRC_CON0;
	sf_info.start = 0;
	sf_info.width = 10;
	for (i = 0; i < num_crc_code; i++) {
		sf_info.val = i << 4 | 0x1;

		for (lane = 0; lane < max_lane; lane++)
			update_bits(regs + (lane * PHY_LANE_OFFSET) + sf_info.addr,
					sf_info.start, sf_info.width, sf_info.val);

		sensor_stream_on(sd_csi, sd_mod);
		msleep(crc_test_time);
		sensor_stream_off(sd_csi, sd_mod);

		info("%s[CRC_FORCE_CODE: %d] s:%d, e:%d, err:%d\n", __func__,
			i, csi->state_cnt.str, csi->state_cnt.end, csi->state_cnt.err);

		if (csi->state_cnt.err) {
			total_err += csi->state_cnt.err;
			cont_err++;
			tmp_open = 0;
		} else {
			cont_end = 5;
			if (csi->state_cnt.str && csi->state_cnt.end) {
				if (!tmp_open)
					tmp_first = i;

				if (++tmp_open > open_window) {
					open_window = tmp_open;
					first_open = tmp_first;
				}

				cont_err = 0;
			} else {
				err("invalid state\n");
			}
		}

		if (cont_err >= cont_end)
			break;
	}

	sf_info.val = 0;
	for (lane = 0; lane < max_lane; lane++)
		update_bits(regs + (lane * PHY_LANE_OFFSET) + sf_info.addr,
				sf_info.start, sf_info.width, sf_info.val);
}

int pablo_test_set_phy_tune(const char *val, const struct kernel_param *kp)
{
	int ret, argc;
	char **argv;
	struct phy_setfile sf_info;
	struct mipi_phy_desc *phy_desc;
	void __iomem *regs;
	struct is_device_csi *csi;
	struct v4l2_subdev *sd_csi, *sd_mod;
	u32 debug_phy_tune;
	u32 size, pre_arg_num;
	u32 num_test_code, test_time;
	u32 arg_idx = 0, tune_idx, i, lane, data_lane;
	u32 prev, next;
	const u32 elem_sz = sizeof(struct phy_setfile) / sizeof(u32);
	void (*run_code_sweep)(u32, u32, u32, struct is_device_csi*, struct v4l2_subdev*,
			struct v4l2_subdev*, void __iomem*);

	if (!sensor) {
		err("sensor is NULL");
		return -EACCES;
	}

	debug_phy_tune = is_get_debug_param(IS_DEBUG_PARAM_PHY_TUNE);
	if (!debug_phy_tune) {
		err("Turn on debug_phy_tune! # echo 1 > debug_phy_tune");
		return -EINVAL;
	}

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		err("No argument!");
		return -EINVAL;
	}

	pre_arg_num = argc % elem_sz;

	/* default value */
	if (is_get_debug_param(IS_DEBUG_PARAM_PHY_TUNE) == PABLO_PHY_TUNE_DPHY) {
		run_code_sweep = skew_code_sweep;
		num_test_code = 26;
	} else {
		run_code_sweep = crc_code_sweep;
		num_test_code = 48;
	}

	test_time = 100; /* msecs */

	if (pre_arg_num == 3) {
		ret = kstrtouint(argv[arg_idx++], 0, &num_test_code);
		if (ret) {
			err("Invalid num_test_code %d ret %d", num_test_code, ret);
			goto func_exit;
		}

		ret = kstrtouint(argv[arg_idx++], 0, &test_time);
		if (ret) {
			err("Invalid test_time %d ret %d", test_time, ret);
			goto func_exit;
		}
	} else if (pre_arg_num != 1) {
		err("invalid num of parameters. %d", argc);
		ret = -EINVAL;
		goto func_exit;
	}

	ret = kstrtouint(argv[arg_idx++], 0, &size);
	if (ret) {
		err("Invalid size %d ret %d", size, ret);
		goto func_exit;
	}

	if (argc < (size * elem_sz + arg_idx)) {
		err("Not enough parameters. %d < %d", argc, size * elem_sz + 1);
		goto func_exit;
	}

	sd_csi = sensor->subdev_csi;
	sd_mod = sensor->subdev_module;

	csi = v4l2_get_subdevdata(sensor->subdev_csi);
	phy_desc = phy_get_drvdata(csi->phy);

	sensor_stream_off(sd_csi, sd_mod);

	data_lane = csi->sensor_cfg->lanes + 1;

	/* Set phy tune value */
	for (i = 0; i < size; i++) {
		tune_idx = i * elem_sz + arg_idx;
		ret = kstrtouint(argv[tune_idx++], 0, &sf_info.addr);
		ret |= kstrtouint(argv[tune_idx++], 0, &sf_info.start);
		ret |= kstrtouint(argv[tune_idx++], 0, &sf_info.width);
		ret |= kstrtouint(argv[tune_idx++], 0, &sf_info.val);
		ret |= kstrtouint(argv[tune_idx++], 0, &sf_info.index);
		ret |= kstrtouint(argv[tune_idx++], 0, &sf_info.max_lane);
		if (ret) {
			err("Invalid parameters(ret %d)", ret);
			goto invalid_param;
		}

		if (sf_info.index == IDX_CLK_VAL) {
			regs = phy_desc->regs;
		} else {
			regs = phy_desc->regs_lane;
			data_lane = sf_info.max_lane;
		}

		/* check parameter validation */
		if ((sf_info.addr >= PHY_LANE_OFFSET * 2)
		    || (sf_info.start + sf_info.width >= BITS_PER_TYPE(u32))
		    || (sf_info.max_lane > CSI_DATA_LANES_MAX)) {
			err("%s: invalid param(addr(0x%X),start(%d),width(%d),max_lane(%d)",
				__func__, sf_info.addr, sf_info.start,
				sf_info.width, sf_info.max_lane);
			goto invalid_param;
		}

		prev = readl(regs + sf_info.addr);

		for (lane = 0; lane < sf_info.max_lane; lane++)
			update_bits(regs + (lane * PHY_LANE_OFFSET) + sf_info.addr,
					sf_info.start, sf_info.width, sf_info.val);

		next = readl(regs + sf_info.addr);

		info("%s (0x%04X: 0x%08X -> 0x%08X)\n", __func__,
			sf_info.addr, prev, next);
	}

	open_window = 0;
	total_err = 0;
	first_open = 0;

	if (!num_test_code) {
		sensor_stream_on(sd_csi, sd_mod);
		msleep(test_time);
		sensor_stream_off(sd_csi, sd_mod);

		info("%s s:%d, e:%d, err:%d\n", __func__,
			csi->state_cnt.str, csi->state_cnt.end, csi->state_cnt.err);

		if (csi->state_cnt.err) {
			open_window = 0;
		} else {
			if (csi->state_cnt.str && csi->state_cnt.end)
				open_window = 1;
			else
				err("invalid state\n");
		}
	} else {
		run_code_sweep(num_test_code, test_time, data_lane, csi, sd_csi, sd_mod, phy_desc->regs_lane);
	}

invalid_param:
func_exit:
	argv_free(argv);
	return ret;
}

static int pablo_test_get_phy_tune(char *buffer, const struct kernel_param *kp)
{
	int ret;

	ret = sprintf(buffer, "%d %d %d\n", open_window, total_err, first_open);

	return ret;
}

u32 pablo_test_get_open_window(void)
{
	return open_window;
}

u32 pablo_test_get_total_err(void)
{
	return total_err;
}

u32 pablo_test_get_first_open(void)
{
	return first_open;
}

static int pablo_test_set_rt_phy_tune(const char *val, const struct kernel_param *kp)
{
	int ret, argc;
	char **argv;
	struct phy_setfile sf_info;
	struct mipi_phy_desc *phy_desc;
	void __iomem *regs;
	struct is_device_csi *csi;
	struct v4l2_subdev *sd_csi, *sd_mod;
	u32 sensor_id, size;
	u32 arg_idx = 0, tune_idx, i, lane;
	u32 prev, next;
	const u32 elem_sz = sizeof(struct phy_setfile) / sizeof(u32);
	struct is_device_sensor *ids;
	struct is_core *core;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		err("No argument!");
		return -EINVAL;
	}

	ret = kstrtouint(argv[arg_idx++], 0, &sensor_id);
	if (ret) {
		err("Invalid sensor_id %d ret %d", sensor_id, ret);
		goto func_exit;
	}

	if (sensor_id >= IS_SENSOR_COUNT) {
		err("Invalid sensor_id %d", sensor_id);
		goto func_exit;
	}

	core = is_get_is_core();

	ids = &core->sensor[sensor_id];
	if (!test_bit(IS_SENSOR_FRONT_START, &ids->state)) {
		err("sensor is not in stream on state");
		goto func_exit;
	}

	ret = kstrtouint(argv[arg_idx++], 0, &size);
	if (ret) {
		err("Invalid size %d ret %d", size, ret);
		goto func_exit;
	}

	if (argc < (size * elem_sz + arg_idx)) {
		err("Not enough parameters. %d < %d", argc, size * elem_sz + 1);
		goto func_exit;
	}

	sd_csi = ids->subdev_csi;
	sd_mod = ids->subdev_module;

	csi = v4l2_get_subdevdata(ids->subdev_csi);
	phy_desc = phy_get_drvdata(csi->phy);

	/* Set phy tune value */
	for (i = 0; i < size; i++) {
		tune_idx = i * elem_sz + arg_idx;
		ret = kstrtouint(argv[tune_idx++], 0, &sf_info.addr);
		ret |= kstrtouint(argv[tune_idx++], 0, &sf_info.start);
		ret |= kstrtouint(argv[tune_idx++], 0, &sf_info.width);
		ret |= kstrtouint(argv[tune_idx++], 0, &sf_info.val);
		ret |= kstrtouint(argv[tune_idx++], 0, &sf_info.index);
		ret |= kstrtouint(argv[tune_idx++], 0, &sf_info.max_lane);
		if (ret) {
			err("Invalid parameters(ret %d)", ret);
			goto func_exit;
		}

		if (sf_info.index == IDX_CLK_VAL)
			regs = phy_desc->regs;
		else
			regs = phy_desc->regs_lane;

		/* check parameter validation */
		if ((sf_info.addr >= PHY_LANE_OFFSET * 2)
		    || (sf_info.start + sf_info.width >= BITS_PER_TYPE(u32))
		    || (sf_info.max_lane > CSI_DATA_LANES_MAX)) {
			err("%s: invalid param(addr(0x%X),start(%d),width(%d),max_lane(%d)",
				__func__, sf_info.addr, sf_info.start,
				sf_info.width, sf_info.max_lane);
			goto func_exit;
		}

		prev = readl(regs + sf_info.addr);

		for (lane = 0; lane < sf_info.max_lane; lane++)
			update_bits(regs + (lane * PHY_LANE_OFFSET) + sf_info.addr,
					sf_info.start, sf_info.width, sf_info.val);

		next = readl(regs + sf_info.addr);

		info("%s (0x%04X: 0x%08X -> 0x%08X)\n", __func__,
			sf_info.addr, prev, next);
	}

func_exit:
	argv_free(argv);
	return ret;
}
