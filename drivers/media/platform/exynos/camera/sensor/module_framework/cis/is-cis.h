/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_H
#define IS_CIS_H

#include <exynos-is-module.h>

#include "is-metadata.h"

/* if you need color-bar, change a below macro to 2 */
#define CIS_TEST_PATTERN_MODE 0
#define CIS_STREAM_OFF_WAIT_TIME 2000	/* 2ms */
#define CIS_STREAM_ON_WAIT_TIME 2000	/* 2ms */

#define SENSOR_CIS_240FPS_FRAME_DURATION_US	(4166)

struct sensor_pll_info_compact {
	u32 ext_clk;
	u64 mipi_datarate;
	u64 pclk;
	u32 frame_length_lines;
	u32 line_length_pck;
};

struct sensor_pll_info {
	u32 ext_clk;
	u32 vt_pix_clk_div;
	u32 vt_sys_clk_div;
	u32 pre_pll_clk_div;
	u32 pll_multiplier;
	u32 op_pix_clk_div;
	u32 op_sys_clk_div;

	u32 secnd_pre_pll_clk_div;
	u32 secnd_pll_multiplier;
	u32 frame_length_lines;
	u32 line_length_pck;
};

struct sensor_reg_addr {
	u16 fll;
	u16 fll_aeb_long;
	u16 fll_aeb_short;
	u16 fll_shifter;
	u16 cit;
	u16 cit_aeb_long;
	u16 cit_aeb_short;
	u16 cit_shifter;
	u16 again;
	u16 again_aeb_long;
	u16 again_aeb_short;
	u16 dgain;
	u16 dgain_aeb_long;
	u16 dgain_aeb_short;
	u16 group_param_hold;
};

struct sensor_cis_info {
	const char *name;
	const char *version;
	u16 max_width;
	u16 max_height;
	u16 min_analog_gain;
	u16 min_digital_gain;
	u16 fine_integration_time;
	u16 cit_compensation_threshold;
	u8 image_orientation;
	bool use_group_param_hold;
	const struct sensor_cis_mode_info **mode_infos;
	u32 mode_count;
	const struct cam_mipi_sensor_mode *mipi_mode;
	u32 mipi_mode_count;
	void *priv; /* sensor private data */
};

#define SENSOR_REGS(val) {val, ARRAY_SIZE(val)}

struct sensor_regs {
	const u16 *regs;
	const u32 size;
};

struct sensor_cis_mode_info {
	u16 setfile_index;
	const struct sensor_regs setfile;
	u16 setfile_fcm_index;
	const struct sensor_regs setfile_fcm;

	u64 pclk;
	u16 frame_length_lines;
	u16 line_length_pck;
	u16 min_analog_gain;
	u16 max_analog_gain;
	u16 max_digital_gain;
	u16 min_cit;
	u16 max_cit_margin;
	u16 align_cit;
	u16 align_offset_cit;
	u16 align_fll;
	u16 fine_integration_time;

	bool use_mode_analog_gain;
	bool wb_gain_support;
	bool remosaic_mode;
	enum is_sensor_12bit_state state_12bit;
};

struct sensor_crop_xy_info {
	u32 crop_x;
	u32 crop_y;
};

/* FIXME: make it dynamic parsing */
#define I2C_READ  4
#define I2C_WRITE 3
#define I2C_BYTE  2
#define I2C_DATA  1
#define I2C_ADDR  0

enum i2c_write {
	I2C_WRITE_ADDR8_DATA8 = 0x0,
	I2C_WRITE_ADDR16_DATA8,
	I2C_WRITE_ADDR16_DATA16
};

struct is_cis_log {
	unsigned int	type; /* read / write */
	unsigned int	size; /* 8bit, 16bit */
	unsigned int	addr;
	unsigned int	data;
	char		*name;
};

int sensor_cis_set_registers(struct v4l2_subdev *subdev, const u32 *regs, const u32 size);
int sensor_cis_set_registers_addr8(struct v4l2_subdev *subdev, const u32 *regs, const u32 size);
int sensor_cis_write_registers(struct v4l2_subdev *subdev, const struct sensor_regs regset);
int sensor_cis_write_registers_locked(struct v4l2_subdev *subdev, const struct sensor_regs regset);

struct is_cis *sensor_cis_get_cis(struct v4l2_subdev *subdev);
void sensor_cis_set_retention_mode(struct v4l2_subdev *subdev, u32 retention_mode);
u32 sensor_cis_get_retention_mode(struct v4l2_subdev *subdev);
bool sensor_cis_support_retention_mode(struct v4l2_subdev *subdev);

int sensor_cis_check_rev_on_init(struct v4l2_subdev *subdev);
int sensor_cis_check_rev(struct is_cis *cis);

u32 sensor_cis_calc_again_code(u32 permile);
u32 sensor_cis_calc_again_permile(u32 code);

u32 sensor_cis_calc_dgain_code(u32 permile);
u32 sensor_cis_calc_dgain_permile(u32 code);

int sensor_cis_set_exposure_time(struct v4l2_subdev *subdev, struct ae_param *target_exposure);
int sensor_cis_get_min_exposure_time(struct v4l2_subdev *subdev, u32 *min_expo);
int sensor_cis_get_max_exposure_time(struct v4l2_subdev *subdev, u32 *max_expo);
int sensor_cis_adjust_frame_duration(struct v4l2_subdev *subdev, u32 input_exposure_time, u32 *target_duration);
int sensor_cis_set_frame_duration(struct v4l2_subdev *subdev, u32 frame_duration);
int sensor_cis_set_frame_rate(struct v4l2_subdev *subdev, u32 min_fps);
int sensor_cis_adjust_analog_gain(struct v4l2_subdev *subdev, u32 input_again, u32 *target_permile);
int sensor_cis_set_analog_gain(struct v4l2_subdev *subdev, struct ae_param *again);
int sensor_cis_get_analog_gain(struct v4l2_subdev *subdev, u32 *again);
int sensor_cis_get_min_analog_gain(struct v4l2_subdev *subdev, u32 *min_again);
int sensor_cis_get_max_analog_gain(struct v4l2_subdev *subdev, u32 *max_again);
int sensor_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain);
int sensor_cis_get_digital_gain(struct v4l2_subdev *subdev, u32 *dgain);
int sensor_cis_get_min_digital_gain(struct v4l2_subdev *subdev, u32 *min_dgain);
int sensor_cis_get_max_digital_gain(struct v4l2_subdev *subdev, u32 *max_dgain);
int sensor_cis_compensate_gain_for_extremely_br(struct v4l2_subdev *subdev, u32 expo, u32 *again, u32 *dgain);
int sensor_cis_get_mode_info(struct v4l2_subdev *subdev, u32 mode, struct is_sensor_mode_info *mode_info);
int sensor_cis_recover_stream_on(struct v4l2_subdev *subdev);
int sensor_cis_recover_stream_off(struct v4l2_subdev *subdev);

void sensor_cis_data_calculation(struct v4l2_subdev *subdev, u32 mode);
u8 sensor_cis_get_duration_shifter(struct is_cis *cis, u32 input_duration);
int sensor_cis_long_term_exposure(struct v4l2_subdev *subdev);

int sensor_cis_dump_registers(struct v4l2_subdev *subdev, const u32 *regs, const u32 size);

u32 sensor_cis_do_div64(u64 num, u32 den);

int sensor_cis_parse_dt(struct device *dev, struct v4l2_subdev *subdev);

void sensor_cis_recover_i2c_fail(struct v4l2_subdev *subdev_cis);
int sensor_cis_wait_streamoff(struct v4l2_subdev *subdev);
int sensor_cis_wait_streamoff_mipi_end(struct v4l2_subdev *subdev);
int sensor_cis_wait_streamon(struct v4l2_subdev *subdev);

int sensor_cis_set_initial_exposure(struct v4l2_subdev *subdev);
int sensor_cis_active_test(struct v4l2_subdev *subdev);
int sensor_cis_get_bayer_order(struct v4l2_subdev *subdev, u32 *bayer_order);
void sensor_cis_log_status(struct is_cis *cis,
	const struct is_cis_log *log, unsigned int num, char *fn);
int sensor_cis_set_test_pattern(struct v4l2_subdev *subdev, camera2_sensor_ctl_t *sensor_ctl);
int sensor_cis_set_group_param_hold(struct v4l2_subdev *subdev, bool hold);

int sensor_cis_probe(void *client, struct device *client_dev,
		struct is_device_sensor_peri **sensor_peri, enum ixc_type type);
#endif
