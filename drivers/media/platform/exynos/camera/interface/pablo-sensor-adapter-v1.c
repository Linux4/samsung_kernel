/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo IS driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-sensor-adapter.h"
#include "is-interface-sensor.h"
#include "pablo-crta-interface.h"
#include <linux/kthread.h>

#define PABLO_SENSOR_MAX_WORK	20

struct is_sensor_interface;

enum pablo_sensor_adt_state {
	IS_SENSOR_ADT_OPEN,
	IS_SENSOR_ADT_START,
};

typedef int (*sensor_control_fn)(struct is_sensor_interface *sensor_itf, u32 instance,
	u32 *ctrl_param, u32 *param_idx);

struct constrol_sensor_fn_table {
	enum pablo_sensor_control_id	control_id;
	sensor_control_fn		control_fn;
};

struct pablo_sensor_work {
	struct kthread_work			work;
	struct pablo_sensor_adt_v1		*sensor_adt;
	struct is_sensor_interface		*sensor_itf;
	struct pablo_sensor_control_info	*psci;
	dma_addr_t				psci_dva;
	u32					psci_idx;
};

struct pablo_sensor_task {
	struct kthread_worker		*worker;
	spinlock_t			work_lock;
	u32				work_idx;
	struct pablo_sensor_work	work[PABLO_SENSOR_MAX_WORK];
};

struct pablo_sensor_adt_v1 {
	u32						instance;
	struct is_sensor_interface			*sensor_itf;
	struct pablo_sensor_task			sensor_task;

	void						*caller;
	pablo_sensor_adt_control_sensor_callback	callback;

	struct work_struct				actuator_info_work;
	struct pablo_crta_sensor_info			latest_pcsi;
	spinlock_t					latest_pcsi_slock;

	unsigned long					state;
};

static int __pablo_sensor_adt_request_exposure(struct is_sensor_interface *sensor_itf, u32 instance,
	u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	enum is_exposure_gain_count num_data;
	u32 *exposure;
	u32 ctrl_end;

	num_data = ctrl_param[(*param_idx)++];

	if (num_data >= EXPOSURE_GAIN_COUNT_END) {
		merr_adt("invalid num_data (%d)", instance, num_data);
		return -EINVAL;
	}

	exposure = &ctrl_param[*param_idx];
	*param_idx += num_data;
	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	ret = sensor_itf->cis_itf_ops.request_exposure(sensor_itf, num_data, exposure);

	return ret;
}

static int __pablo_sensor_adt_request_gain(struct is_sensor_interface *sensor_itf, u32 instance,
	u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	enum is_exposure_gain_count num_data;
	u32 *tgain, *again, *dgain;
	u32 ctrl_end;

	num_data = ctrl_param[(*param_idx)++];

	if (num_data >= EXPOSURE_GAIN_COUNT_END) {
		merr_adt("invalid num_data (%d)", instance, num_data);
		return -EINVAL;
	}

	tgain = &ctrl_param[*param_idx];
	*param_idx += num_data;
	again = &ctrl_param[*param_idx];
	*param_idx += num_data;
	dgain = &ctrl_param[*param_idx];
	*param_idx += num_data;
	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	ret = sensor_itf->cis_itf_ops.request_gain(sensor_itf, num_data,
		tgain, again, dgain);

	return ret;
}

static int __pablo_sensor_adt_set_alg_reset_flag(struct is_sensor_interface *sensor_itf, u32 instance,
	u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	bool executed;
	u32 ctrl_end;

	executed = ctrl_param[(*param_idx)++];
	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	ret = sensor_itf->cis_itf_ops.set_alg_reset_flag(sensor_itf, executed);

	return ret;
}

static int __pablo_sensor_adt_set_num_of_frame_per_one_3aa(struct is_sensor_interface *sensor_itf,
	u32 instance, u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	u32 num_of_frame;
	u32 ctrl_end;

	num_of_frame = ctrl_param[(*param_idx)++];
	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	ret = sensor_itf->cis_itf_ops.set_num_of_frame_per_one_3aa(sensor_itf, &num_of_frame);

	return ret;
}

static int __pablo_sensor_adt_apply_sensor_setting(struct is_sensor_interface *sensor_itf,
	u32 instance, u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	u32 ctrl_end;

	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	ret = sensor_itf->cis_itf_ops.apply_sensor_setting(sensor_itf);
	sensor_itf->cis_evt_ops.end_of_frame(sensor_itf);
	sensor_itf->cis_evt_ops.start_of_frame(sensor_itf);

	return ret;
}

static int __pablo_sensor_adt_request_reset_expo_gain(struct is_sensor_interface *sensor_itf,
	u32 instance, u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	enum is_exposure_gain_count num_data;
	u32 *expo, *tgain, *again, *dgain;
	u32 ctrl_end;

	num_data = ctrl_param[(*param_idx)++];

	if (num_data >= EXPOSURE_GAIN_COUNT_END) {
		merr_adt("invalid num_data (%d)", instance, num_data);
		return -EINVAL;
	}

	expo = &ctrl_param[*param_idx];
	*param_idx += num_data;
	tgain = &ctrl_param[*param_idx];
	*param_idx += num_data;
	again = &ctrl_param[*param_idx];
	*param_idx += num_data;
	dgain = &ctrl_param[*param_idx];
	*param_idx += num_data;
	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	ret = sensor_itf->cis_itf_ops.request_reset_expo_gain(sensor_itf, num_data,
		expo, tgain, again, dgain);

	return ret;
}

static int __pablo_sensor_adt_set_long_term_expo_mode(struct is_sensor_interface *sensor_itf,
	u32 instance, u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	u32 i, expo[2], again[2], dgain[2], ctrl_end;
	struct is_long_term_expo_mode long_term_expo_mode = {0,};

	expo[0] = ctrl_param[(*param_idx)++];
	expo[1] = ctrl_param[(*param_idx)++];
	again[0] = ctrl_param[(*param_idx)++];
	again[1] = ctrl_param[(*param_idx)++];
	dgain[0] = ctrl_param[(*param_idx)++];
	dgain[1] = ctrl_param[(*param_idx)++];
	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	for (i = 0; i < 2; i++) {
		long_term_expo_mode.expo[i] = expo[i];
		long_term_expo_mode.again[i] = again[i];
		long_term_expo_mode.dgain[i] = dgain[i];
	}
	long_term_expo_mode.frm_num_strm_off_on_interval = 0;

	ret = sensor_itf->cis_ext2_itf_ops.set_long_term_expo_mode(sensor_itf, &long_term_expo_mode);

	return ret;
}

static int __pablo_sensor_adt_set_sensor_info_mode_change(struct is_sensor_interface *sensor_itf,
	u32 instance, u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	enum is_exposure_gain_count num_data;
	u32 *expo, *again, *dgain;
	u32 ctrl_end;

	num_data = ctrl_param[(*param_idx)++];

	if (num_data >= EXPOSURE_GAIN_COUNT_END) {
		merr_adt("invalid num_data (%d)", instance, num_data);
		return -EINVAL;
	}

	expo = &ctrl_param[*param_idx];
	*param_idx += num_data;
	again = &ctrl_param[*param_idx];
	*param_idx += num_data;
	dgain = &ctrl_param[*param_idx];
	*param_idx += num_data;
	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	ret = sensor_itf->cis_itf_ops.set_sensor_info_mode_change(sensor_itf, num_data,
		expo, again, dgain);

	return ret;
}

static int __pablo_sensor_adt_set_sensor_info_mfhdr_mode_change(struct is_sensor_interface *sensor_itf,
	u32 instance, u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	u32 count, *expo, *again, *dgain, *sensitivity, ctrl_end;

	count = ctrl_param[(*param_idx)++];
	expo = &ctrl_param[*param_idx];
	*param_idx += count;
	again = &ctrl_param[*param_idx];
	*param_idx += count;
	dgain = &ctrl_param[*param_idx];
	*param_idx += count;
	sensitivity = &ctrl_param[*param_idx];
	*param_idx += count;
	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	ret = sensor_itf->cis_ext2_itf_ops.set_sensor_info_mfhdr_mode_change(sensor_itf,
		count, expo, again, dgain, expo, again, dgain, sensitivity);

	return ret;
}

static int __pablo_sensor_adt_set_adjust_sync(struct is_sensor_interface *sensor_itf,
	u32 instance, u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	u32 setsync;
	u32 ctrl_end;

	setsync = ctrl_param[(*param_idx)++];
	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	ret = sensor_itf->cis_ext_itf_ops.set_adjust_sync(sensor_itf, setsync);

	return ret;
}

static int __pablo_sensor_adt_request_frame_length_line(struct is_sensor_interface *sensor_itf,
	u32 instance, u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	u32 framelengthline;
	u32 ctrl_end;

	framelengthline = ctrl_param[(*param_idx)++];
	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	ret = sensor_itf->cis_ext_itf_ops.request_frame_length_line(sensor_itf, framelengthline);

	return ret;
}

static int __pablo_sensor_adt_request_sensitivity(struct is_sensor_interface *sensor_itf,
	u32 instance, u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	u32 sensitivity;
	u32 ctrl_end;

	sensitivity = ctrl_param[(*param_idx)++];
	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	ret = sensor_itf->cis_ext_itf_ops.request_sensitivity(sensor_itf, sensitivity);

	return ret;
}

static int __pablo_sensor_adt_set_sensor_12bit_state(struct is_sensor_interface *sensor_itf,
	u32 instance, u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	enum is_sensor_12bit_state state;
	u32 ctrl_end;

	state = ctrl_param[(*param_idx)++];
	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	ret = sensor_itf->cis_ext_itf_ops.set_sensor_12bit_state(sensor_itf, state);

	return ret;
}

static int __pablo_sensor_adt_set_low_noise_mode(struct is_sensor_interface *sensor_itf,
	u32 instance, u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	u32 mode;
	u32 ctrl_end;

	mode = ctrl_param[(*param_idx)++];
	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	ret = sensor_itf->cis_ext2_itf_ops.set_low_noise_mode(sensor_itf, mode);

	return ret;
}

static int __pablo_sensor_adt_request_wb_gain(struct is_sensor_interface *sensor_itf,
	u32 instance, u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	u32 gr_gain, r_gain, b_gain, gb_gain;
	u32 ctrl_end;

	gr_gain = ctrl_param[(*param_idx)++];
	r_gain = ctrl_param[(*param_idx)++];
	b_gain = ctrl_param[(*param_idx)++];
	gb_gain = ctrl_param[(*param_idx)++];
	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	ret = sensor_itf->cis_ext2_itf_ops.request_wb_gain(sensor_itf,
		gr_gain, r_gain, b_gain, gb_gain);

	return ret;
}

static int __pablo_sensor_adt_set_mainflash_duration(struct is_sensor_interface *sensor_itf,
	u32 instance, u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	u32 mainflash_duration;
	u32 ctrl_end;

	mainflash_duration = ctrl_param[(*param_idx)++];
	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	ret = sensor_itf->cis_ext2_itf_ops.set_mainflash_duration(sensor_itf, mainflash_duration);

	return ret;
}

static int __pablo_sensor_adt_request_direct_flash(struct is_sensor_interface *sensor_itf,
	u32 instance, u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	u32 mode, intensity, time;
	bool on;
	u32 ctrl_end;

	mode = ctrl_param[(*param_idx)++];
	on = ctrl_param[(*param_idx)++];
	intensity = ctrl_param[(*param_idx)++];
	time = ctrl_param[(*param_idx)++];
	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	ret = sensor_itf->cis_ext2_itf_ops.request_direct_flash(sensor_itf,
		mode, on, intensity, time);

	return ret;
}

static int __pablo_sensor_adt_set_hdr_mode(struct is_sensor_interface *sensor_itf,
	u32 instance, u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	u32 mode;
	u32 ctrl_end;

	mode = ctrl_param[(*param_idx)++];
	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	ret = sensor_itf->cis_ext2_itf_ops.set_hdr_mode(sensor_itf, mode);

	return ret;
}

static int __pablo_sensor_adt_set_remosaic_zoom_ratio(struct is_sensor_interface *sensor_itf,
	u32 instance, u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	u32 zoom_ratio;
	u32 ctrl_end;

	zoom_ratio = ctrl_param[(*param_idx)++];
	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	ret = sensor_itf->cis_ext2_itf_ops.set_remosaic_zoom_ratio(sensor_itf, zoom_ratio);

	return ret;
}

static int __pablo_sensor_adt_set_position(struct is_sensor_interface *sensor_itf,
	u32 instance, u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	u32 position;
	int domain_offset;
	u32 ctrl_end;

	position = ctrl_param[(*param_idx)++];
	domain_offset = ctrl_param[(*param_idx)++];
	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	ret = sensor_itf->actuator_itf_ops.set_position(sensor_itf, position, domain_offset);

	return ret;
}

static int __pablo_sensor_adt_set_soft_landing_config(struct is_sensor_interface *sensor_itf,
	u32 instance, u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	u32 step_delay, position_num, *position_table, ctrl_end;

	step_delay = ctrl_param[(*param_idx)++];
	position_num = ctrl_param[(*param_idx)++];
	position_table = &ctrl_param[*param_idx];
	*param_idx += position_num;
	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	ret = sensor_itf->actuator_itf_ops.set_soft_landing_config(sensor_itf,
		step_delay, position_num, position_table);

	return ret;
}

static int __pablo_sensor_adt_request_flash(struct is_sensor_interface *sensor_itf,
	u32 instance, u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	u32 mode, intensity, time;
	bool on;
	u32 ctrl_end;

	mode = ctrl_param[(*param_idx)++];
	on = ctrl_param[(*param_idx)++];
	intensity = ctrl_param[(*param_idx)++];
	time = ctrl_param[(*param_idx)++];
	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	ret = sensor_itf->flash_itf_ops.request_flash(sensor_itf,
		mode, on, intensity, time);

	return ret;
}

static int __pablo_sensor_adt_request_flash_expo_gain(struct is_sensor_interface *sensor_itf,
	u32 instance, u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	struct is_flash_expo_gain *flash_ae;
	u32 ctrl_end;

	flash_ae = (struct is_flash_expo_gain *)&ctrl_param[(*param_idx)];
	(*param_idx) += (sizeof(struct is_flash_expo_gain) / sizeof(u32));
	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	ret = sensor_itf->flash_itf_ops.request_flash_expo_gain(sensor_itf, flash_ae);

	return ret;
}

static int __pablo_sensor_adt_set_aperture_value(struct is_sensor_interface *sensor_itf,
	u32 instance, u32 *ctrl_param, u32 *param_idx)
{
	int ret;
	int value;
	u32 ctrl_end;

	value = ctrl_param[(*param_idx)++];
	ctrl_end = ctrl_param[(*param_idx)++];

	if (ctrl_end != SS_CTRL_END) {
		merr_adt("invalid sensor ctrl end (%d)", instance, ctrl_end);
		return -EINVAL;
	}

	ret = sensor_itf->aperture_itf_ops.set_aperture_value(sensor_itf, value);

	return ret;
}

static const struct constrol_sensor_fn_table fn_table[SS_CTRL_END] = {
	{SS_CTRL_CIS_REQUEST_EXPOSURE, __pablo_sensor_adt_request_exposure},
	{SS_CTRL_CIS_REQUEST_GAIN, __pablo_sensor_adt_request_gain},
	{SS_CTRL_CIS_SET_ALG_RESET_FLAG, __pablo_sensor_adt_set_alg_reset_flag},
	{SS_CTRL_CIS_SET_NUM_OF_FRAME_PER_ONE_RTA, __pablo_sensor_adt_set_num_of_frame_per_one_3aa},
	{SS_CTRL_CIS_APPLY_SENSOR_SETTING, __pablo_sensor_adt_apply_sensor_setting},
	{SS_CTRL_CIS_REQUEST_RESET_EXPO_GAIN, __pablo_sensor_adt_request_reset_expo_gain},
	{SS_CTRL_CIS_SET_STREAM_OFF_ON_MODE, __pablo_sensor_adt_set_long_term_expo_mode},
	{SS_CTRL_CIS_SET_SENSOR_INFO_MODE_CHANGE, __pablo_sensor_adt_set_sensor_info_mode_change},
	{SS_CTRL_CIS_REQUEST_SENSOR_INFO_MFHDR_MODE_CHANGE, __pablo_sensor_adt_set_sensor_info_mfhdr_mode_change},
	{SS_CTRL_CIS_ADJUST_SYNC, __pablo_sensor_adt_set_adjust_sync},
	{SS_CTRL_CIS_REQUEST_FRAME_LENGTH_LINE, __pablo_sensor_adt_request_frame_length_line},
	{SS_CTRL_CIS_REQUEST_SENSITIVITY, __pablo_sensor_adt_request_sensitivity},
	{SS_CTRL_CIS_SET_12BIT_STATE, __pablo_sensor_adt_set_sensor_12bit_state},
	{SS_CTRL_CIS_SET_NOISE_MODE, __pablo_sensor_adt_set_low_noise_mode},
	{SS_CTRL_CIS_REQUEST_WB_GAIN, __pablo_sensor_adt_request_wb_gain},
	{SS_CTRL_CIS_SET_MAINFLASH_DURATION, __pablo_sensor_adt_set_mainflash_duration},
	{SS_CTRL_CIS_REQUEST_DIRECT_FLASH, __pablo_sensor_adt_request_direct_flash},
	{SS_CTRL_CIS_SET_HDR_MODE, __pablo_sensor_adt_set_hdr_mode},
	{SS_CTRL_CIS_SET_REMOSAIC_ZOOM_RATIO, __pablo_sensor_adt_set_remosaic_zoom_ratio},
	/* actuator */
	{SS_CTRL_ACTUATOR_SET_POSITION, __pablo_sensor_adt_set_position},
	{SS_CTRL_CIS_SET_SOFT_LANDING_CONFIG, __pablo_sensor_adt_set_soft_landing_config},
	/* flash */
	{SS_CTRL_FLASH_REQUEST_FLASH, __pablo_sensor_adt_request_flash},
	{SS_CTRL_FLASH_REQUEST_EXPO_GAIN, __pablo_sensor_adt_request_flash_expo_gain},
	/* aperture */
	{SS_CTRL_APERTURE_SET_APERTURE_VALUE, __pablo_sensor_adt_set_aperture_value},
};

static void __pablo_sensor_adt_handle_kthread_work(struct kthread_work *work)
{
	int ret = 0;
	u32 instance, ctrl_idx, param_idx, *ctrl;
	enum pablo_sensor_control_id control_id;
	struct pablo_sensor_adt_v1 *sensor_adt;
	struct pablo_sensor_work *sensor_work;
	struct is_sensor_interface *sensor_itf;
	struct pablo_sensor_control_info *psci;

	sensor_work = container_of(work, struct pablo_sensor_work, work);
	sensor_adt = sensor_work->sensor_adt;
	instance = sensor_adt->instance;
	sensor_itf = sensor_work->sensor_itf;
	psci = sensor_work->psci;
	ctrl = psci->sensor_control;

	mdbg_adt(2, "%s\n", instance, __func__);

	if (!test_bit(IS_SENSOR_ADT_START, &sensor_adt->state)) {
		merr_adt("sensor adt is already stopped", instance);
		ret = -EINVAL;
		goto callback;
	}

	param_idx = 0;
	if (ctrl[param_idx] != PABLO_CRTA_MAGIC_NUMBER) {
		merr_adt("invalid magic number: 0x%08X, 0x%08X", instance,
			 psci->sensor_control[0], PABLO_CRTA_MAGIC_NUMBER);
		ret = -EINVAL;
		goto callback;
	}

	param_idx = 1;
	for (ctrl_idx = 0; ctrl_idx < psci->sensor_control_size; ctrl_idx++) {
		control_id = ctrl[param_idx++];
		if (control_id < SS_CTRL_END) {
			ret = fn_table[control_id].control_fn(sensor_itf, instance,
							      ctrl, &param_idx);
			if (ret) {
				merr_adt("invalid control param", instance);
				goto callback;
			}
		} else {
			merr_adt("invalid control id", instance);
			ret = -EINVAL;
			goto callback;
		}
	}

callback:
	if (sensor_adt->callback)
		sensor_adt->callback(sensor_adt->caller, instance, ret,
				     sensor_work->psci_dva, sensor_work->psci_idx);
}

static int __pablo_sensor_adt_create_ktherad(struct pablo_sensor_adt_v1 *sensor_adt)
{
	ulong flag;
	u32 instance, work_idx;
	struct pablo_sensor_task *sensor_task;

	instance = sensor_adt->instance;
	/* create kthread worker */
	sensor_task = &sensor_adt->sensor_task;
	sensor_task->worker = kthread_create_worker(0, "pablo_ss_adt_%d", instance);
	if (IS_ERR(sensor_task->worker)) {
		merr_adt("failed to create ss_adt kthread: %ld", instance,
			PTR_ERR(sensor_task->worker));
		sensor_task->worker = NULL;
		return PTR_ERR(sensor_task->worker);
	}

	/* init spin lock */
	spin_lock_init(&sensor_task->work_lock);

	/* init kthread work */
	spin_lock_irqsave(&sensor_task->work_lock, flag);
	sensor_task->work_idx = 0;
	for (work_idx = 0; work_idx < PABLO_SENSOR_MAX_WORK; work_idx++) {
		sensor_task->work[work_idx].sensor_adt = sensor_adt;
		sensor_task->work[work_idx].sensor_itf = sensor_adt->sensor_itf;
		sensor_task->work[work_idx].psci = NULL;
		sensor_task->work[work_idx].psci_dva = 0;
		sensor_task->work[work_idx].psci_idx = 0;
		kthread_init_work(&sensor_task->work[work_idx].work,
				__pablo_sensor_adt_handle_kthread_work);
	}
	spin_unlock_irqrestore(&sensor_task->work_lock, flag);

	return 0;
}

static void __pablo_sensor_adt_update_actuator_info_work(struct work_struct *data)
{

	int ret, temperature, voltage;
	u32 instance, status, temperature_valid, voltage_valid;
	unsigned long flags;
	struct pablo_sensor_adt_v1 *sensor_adt;
	struct is_sensor_interface *sensor_itf;
	struct pablo_crta_sensor_info *pcsi;

	sensor_adt = container_of(data, struct pablo_sensor_adt_v1, actuator_info_work);

	instance = sensor_adt->instance;
	sensor_itf = sensor_adt->sensor_itf;

	mdbg_adt(2, "%s\n", instance, __func__);

	if (sensor_itf->cis_itf_ops.is_actuator_available(sensor_itf))
		ret = sensor_itf->actuator_itf_ops.get_status(sensor_itf, &status,
			&temperature_valid,
			&temperature,
			&voltage_valid,
			&voltage);
	else
		ret = -ENODEV;

	spin_lock_irqsave(&sensor_adt->latest_pcsi_slock, flags);
	pcsi = &sensor_adt->latest_pcsi;
	if (!ret) {
		pcsi->temperature_valid = temperature_valid;
		pcsi->temperature = temperature;
		pcsi->voltage_valid = voltage_valid;
		pcsi->voltage = voltage;
	} else {
		pcsi->temperature_valid = false;
		pcsi->voltage_valid = false;
	}
	spin_unlock_irqrestore(&sensor_adt->latest_pcsi_slock, flags);
}

static int pablo_sensor_adt_open(struct pablo_sensor_adt *adt, u32 instance,
	struct is_sensor_interface *sensor_itf)
{
	int ret;
	struct pablo_sensor_adt_v1 *sensor_adt;

	mdbg_adt(1, "%s\n", instance, __func__);

	if (!sensor_itf) {
		merr_adt("sensor_itf is null", instance);
		return -EINVAL;
	}

	if (atomic_inc_return(&adt->rsccount) == 1) {
		/* alloc priv */
		sensor_adt = vzalloc(sizeof(struct pablo_sensor_adt_v1));
		if (!sensor_adt) {
			merr_adt("failed to alloc pablo_sensor_adt_v1", instance);
			ret = -ENOMEM;
			goto err_alloc;
		}
		adt->priv = (void *)sensor_adt;

		sensor_adt->instance = instance;
		sensor_adt->sensor_itf = sensor_itf;

		ret = __pablo_sensor_adt_create_ktherad(sensor_adt);
		if (ret)
			goto err_kthread;

		INIT_WORK(&sensor_adt->actuator_info_work,
			__pablo_sensor_adt_update_actuator_info_work);

		spin_lock_init(&sensor_adt->latest_pcsi_slock);

		set_bit(IS_SENSOR_ADT_OPEN, &sensor_adt->state);
	} else {
		sensor_adt = (struct pablo_sensor_adt_v1 *)adt->priv;
	}

	return 0;

err_kthread:
	vfree(adt->priv);
	adt->priv = NULL;
err_alloc:
	atomic_dec(&adt->rsccount);
	return ret;
}

static int pablo_sensor_adt_start(struct pablo_sensor_adt *adt)
{
	struct pablo_sensor_adt_v1 *sensor_adt = (struct pablo_sensor_adt_v1 *)adt->priv;

	mdbg_adt(1, "%s\n", sensor_adt->instance, __func__);

	set_bit(IS_SENSOR_ADT_START, &sensor_adt->state);

	return 0;
}

static int pablo_sensor_adt_stop(struct pablo_sensor_adt *adt)
{
	struct pablo_sensor_adt_v1 *sensor_adt = (struct pablo_sensor_adt_v1 *)adt->priv;

	mdbg_adt(1, "%s\n", sensor_adt->instance, __func__);

	clear_bit(IS_SENSOR_ADT_START, &sensor_adt->state);

	if (sensor_adt->sensor_task.worker)
		kthread_flush_worker(sensor_adt->sensor_task.worker);

	return 0;
}

static int pablo_sensor_adt_close(struct pablo_sensor_adt *adt)
{
	struct pablo_sensor_adt_v1 *sensor_adt = (struct pablo_sensor_adt_v1 *)adt->priv;

	mdbg_adt(1, "%s\n", sensor_adt->instance, __func__);

	if (atomic_dec_return(&adt->rsccount) == 0) {
		flush_work(&sensor_adt->actuator_info_work);

		if (sensor_adt->sensor_task.worker)
			kthread_destroy_worker(sensor_adt->sensor_task.worker);

		clear_bit(IS_SENSOR_ADT_OPEN, &sensor_adt->state);

		vfree(adt->priv);
		adt->priv = NULL;
	}

	return 0;
}

static int pablo_sensor_adt_update_actuator_info(struct pablo_sensor_adt *adt, bool block)
{
	struct pablo_sensor_adt_v1 *sensor_adt;

	if (!atomic_read(&adt->rsccount)) {
		err("sensor_adt is not open");
		return -ENODEV;
	}

	sensor_adt = adt->priv;

	if (block)
		__pablo_sensor_adt_update_actuator_info_work(&sensor_adt->actuator_info_work);
	else
		schedule_work(&sensor_adt->actuator_info_work);

	return 0;
}

static int __pablo_sensor_adt_get_vc_dma_pdaf_buf_info(struct is_sensor_interface *itf,
		struct pablo_pdaf_info *buf_info)
{
	int ret = 0, ch;
	struct is_module_enum *module;
	struct v4l2_subdev *subdev_module;
	struct is_device_sensor *sensor;
	u32 stat_vc;

	memset(buf_info, 0, sizeof(struct pablo_pdaf_info));
	buf_info->sensor_mode = VC_SENSOR_MODE_INVALID;

	module = get_subdev_module_enum(itf);
	if (!module) {
		err("failed to get sensor_peri's module");
		return -ENODEV;
	}

	subdev_module = module->subdev;
	if (!subdev_module) {
		err("module's subdev was not probed");
		return -ENODEV;
	}

	sensor = v4l2_get_subdev_hostdata(subdev_module);
	if (!sensor) {
		err("failed to get sensor device");
		return -ENODEV;
	}

	stat_vc = module->stat_vc;
	buf_info->sensor_mode = module->vc_extra_info[VC_BUF_DATA_TYPE_GENERAL_STAT1].sensor_mode;
	buf_info->hpd_width = sensor->cfg->input[stat_vc].width + sensor->cfg->dummy_pixel[stat_vc];
	buf_info->hpd_height = sensor->cfg->input[stat_vc].height;

	for (ch = CSI_VIRTUAL_CH_1; ch < CSI_VIRTUAL_CH_MAX; ch++) {
		if (sensor->cfg->output[ch].type == VC_VPDAF) {
			buf_info->sensor_mode = module->vpd_sensor_mode;
			buf_info->vpd_width = sensor->cfg->input[ch].width + sensor->cfg->dummy_pixel[ch];
			buf_info->vpd_height = sensor->cfg->input[ch].height;
		}
	}

	if (sensor->cfg->stat_sensor_mode != VC_SENSOR_MODE_INVALID)
		buf_info->sensor_mode = sensor->cfg->stat_sensor_mode;

	dbg_sensor(2, "%s stat_vc %d mode %d hpd size %dx%d vpd size %dx%d\n",
			__func__,
			stat_vc,
			buf_info->sensor_mode,
			buf_info->hpd_width, buf_info->hpd_height,
			buf_info->vpd_width, buf_info->vpd_height);

	return ret;
}

static int pablo_sensor_adt_get_sensor_info(struct pablo_sensor_adt *adt, struct pablo_crta_sensor_info *pcsi)
{
	u32 instance;
	int i;
	unsigned long flags;
	struct pablo_sensor_adt_v1 *sensor_adt;
	struct is_sensor_interface *sensor_itf;
	enum is_sensor_stat_control stat_control;
	enum itf_laser_af_type af_type;
	union itf_laser_af_data af_data;
	struct is_sensor_mode_info seamless_mode_info[SEAMLESS_MODE_MAX];

	if (!atomic_read(&adt->rsccount)) {
		err("sensor_adt is not open");
		return -ENODEV;
	}

	sensor_adt = adt->priv;
	instance = sensor_adt->instance;
	sensor_itf = sensor_adt->sensor_itf;

	if (!pcsi) {
		merr_adt("pcsi is null", instance);
		return -EINVAL;
	}

	mdbg_adt(1, "%s\n", instance, __func__);

	/* sensor properties */
	sensor_itf->cis_itf_ops.get_module_position(sensor_itf, &pcsi->position);
	pcsi->type = sensor_itf->cis_itf_ops.get_sensor_type(sensor_itf);
	sensor_itf->cis_itf_ops.get_bayer_order(sensor_itf, &pcsi->pixel_order);
	pcsi->min_expo = sensor_itf->cis_itf_ops.get_min_exposure_time(sensor_itf);
	pcsi->max_expo = sensor_itf->cis_itf_ops.get_max_exposure_time(sensor_itf);
	pcsi->min_again = sensor_itf->cis_itf_ops.get_min_analog_gain(sensor_itf);
	pcsi->max_again = sensor_itf->cis_itf_ops.get_max_analog_gain(sensor_itf);
	pcsi->min_dgain = sensor_itf->cis_itf_ops.get_min_digital_gain(sensor_itf);
	pcsi->max_dgain = sensor_itf->cis_itf_ops.get_max_digital_gain(sensor_itf);
	sensor_itf->cis_itf_ops.get_sensor_frame_timing(sensor_itf,
		&pcsi->vvalid_time, &pcsi->vblank_time);
	sensor_itf->cis_itf_ops.get_sensor_max_fps(sensor_itf, &pcsi->max_fps);

	sensor_itf->cis_ext2_itf_ops.get_sensor_seamless_mode_info(sensor_itf,
		seamless_mode_info, &pcsi->seamless_mode_cnt);

	if (pcsi->seamless_mode_cnt > 0) {
		for (i = 0; i < pcsi->seamless_mode_cnt; i++) {
			pcsi->seamless_mode_info[i].mode = seamless_mode_info[i].mode;
			pcsi->seamless_mode_info[i].min_expo = seamless_mode_info[i].min_expo;
			pcsi->seamless_mode_info[i].max_expo = seamless_mode_info[i].max_expo;
			pcsi->seamless_mode_info[i].min_again = seamless_mode_info[i].min_again;
			pcsi->seamless_mode_info[i].max_again = seamless_mode_info[i].max_again;
			pcsi->seamless_mode_info[i].min_dgain = seamless_mode_info[i].min_dgain;
			pcsi->seamless_mode_info[i].max_dgain = seamless_mode_info[i].max_dgain;
			pcsi->seamless_mode_info[i].vvalid_time = seamless_mode_info[i].vvalid_time;
			pcsi->seamless_mode_info[i].vblank_time = seamless_mode_info[i].vblank_time;
			pcsi->seamless_mode_info[i].max_fps = seamless_mode_info[i].max_fps;
		}
	}

	pcsi->mono_en = is_sensor_get_mono_mode(sensor_itf); /* 0: normal, 1: mono */
	pcsi->bayer_pattern = 0; /* 0: normal, 1: tetra */
	pcsi->hdr_type = is_sensor_get_frame_type(instance);

	/* additional sensor properties */
	sensor_itf->cis_ext_itf_ops.get_sensor_flag(sensor_itf, &stat_control, &pcsi->hdr_mode,
		&pcsi->sensor_12bit_mode, &pcsi->expo_count);
	sensor_itf->cis_ext_itf_ops.get_sensor_12bit_state(sensor_itf, &pcsi->sensor_12bit_state);
	sensor_itf->cis_itf_ops.get_sensor_initial_aperture(sensor_itf, &pcsi->initial_aperture);

	/* peri info */
	pcsi->actuator_available = sensor_itf->cis_itf_ops.is_actuator_available(sensor_itf);
	pcsi->flash_available = sensor_itf->cis_itf_ops.is_flash_available(sensor_itf);
	pcsi->laser_af_available = sensor_itf->cis_itf_ops.is_laser_af_available(sensor_itf);
	pcsi->tof_af_available = sensor_itf->cis_itf_ops.is_tof_af_available(sensor_itf);

	/* ae info */
	sensor_itf->cis_itf_ops.get_frame_timing(sensor_itf,
		EXPOSURE_NUM_MAX, pcsi->expo, pcsi->frame_period, pcsi->line_period);
	sensor_itf->cis_itf_ops.get_analog_gain(sensor_itf, EXPOSURE_NUM_MAX, pcsi->again);
	sensor_itf->cis_itf_ops.get_digital_gain(sensor_itf, EXPOSURE_NUM_MAX, pcsi->dgain);
	sensor_itf->cis_itf_ops.get_next_frame_timing(sensor_itf,
		EXPOSURE_NUM_MAX, pcsi->next_exposure, pcsi->next_frame_period, pcsi->next_line_period);
	sensor_itf->cis_itf_ops.get_next_analog_gain(sensor_itf, EXPOSURE_NUM_MAX, pcsi->next_again);
	sensor_itf->cis_itf_ops.get_next_digital_gain(sensor_itf, EXPOSURE_NUM_MAX, pcsi->next_dgain);
	sensor_itf->cis_itf_ops.get_sensor_cur_fps(sensor_itf, &pcsi->current_fps);
	sensor_itf->cis_ext2_itf_ops.get_open_close_hint(&pcsi->opening_hint, &pcsi->closing_hint);
	sensor_itf->cis_ext2_itf_ops.get_sensor_min_frame_duration(sensor_itf, &pcsi->min_frame_duration);

	/* af info */
	if (pcsi->actuator_available) {
		sensor_itf->actuator_itf_ops.get_cur_frame_position(sensor_itf, &pcsi->actuator_position);
		spin_lock_irqsave(&sensor_adt->latest_pcsi_slock, flags);
		pcsi->temperature_valid = sensor_adt->latest_pcsi.temperature_valid;
		pcsi->temperature = sensor_adt->latest_pcsi.temperature;
		pcsi->voltage_valid = sensor_adt->latest_pcsi.voltage_valid;
		pcsi->voltage = sensor_adt->latest_pcsi.voltage;
		spin_unlock_irqrestore(&sensor_adt->latest_pcsi_slock, flags);
	}

	/* laser af info */
	if (pcsi->laser_af_available) {
		sensor_itf->laser_af_itf_ops.get_distance(sensor_itf, &af_type, &af_data);
		if (af_type == ITF_LASER_AF_TYPE_VL53L1) {
			pcsi->laser_af_distance = af_data.vl53l1.distance_mm;
			pcsi->laser_af_confiden = af_data.vl53l1.confidence;
			pcsi->laser_af_info.type = ITF_LASER_AF_TYPE_VL53L1;
			pcsi->laser_af_info.dataSize = sizeof(struct itf_laser_af_data_VL53L5);
		} else if (af_type == ITF_LASER_AF_TYPE_VL53L8) {
			pcsi->laser_af_info.type = ITF_LASER_AF_TYPE_VL53L8;
			pcsi->laser_af_info.dataSize = sizeof(struct itf_laser_af_data_VL53L8);
		}
	}

	/* pdaf info */
	__pablo_sensor_adt_get_vc_dma_pdaf_buf_info(sensor_itf, &pcsi->pdaf_info);

	return 0;
}

static int pablo_sensor_adt_control_sensor(struct pablo_sensor_adt *adt,
	struct pablo_sensor_control_info *psci, dma_addr_t dva, u32 idx)
{
	ulong flag;
	u32 instance, work_idx;
	struct pablo_sensor_adt_v1 *sensor_adt;
	struct is_sensor_interface *sensor_itf;
	struct pablo_sensor_task *sensor_task;

	if (!atomic_read(&adt->rsccount)) {
		err("sensor_adt is not open");
		return -ENODEV;
	}

	sensor_adt = adt->priv;
	instance = sensor_adt->instance;
	sensor_itf = sensor_adt->sensor_itf;

	if (!psci) {
		merr_adt("psci is null", instance);
		return -EINVAL;
	}

	mdbg_adt(1, "%s psci(%p)\n", instance, __func__, psci);

	sensor_task = &sensor_adt->sensor_task;
	spin_lock_irqsave(&sensor_task->work_lock, flag);
	work_idx = sensor_task->work_idx % PABLO_SENSOR_MAX_WORK;
	sensor_task->work[work_idx].psci = psci;
	sensor_task->work[work_idx].psci_dva = dva;
	sensor_task->work[work_idx].psci_idx = idx;
	sensor_task->work_idx++;
	spin_unlock_irqrestore(&sensor_task->work_lock, flag);

	kthread_queue_work(sensor_task->worker, &sensor_task->work[work_idx].work);

	return 0;
}

static int pablo_sensor_adt_register_callback(struct pablo_sensor_adt *adt, void *caller,
		pablo_sensor_adt_control_sensor_callback callback)
{
	struct pablo_sensor_adt_v1 *sensor_adt;

	if (!atomic_read(&adt->rsccount)) {
		err("sensor_adt is not open");
		return -ENODEV;
	}

	sensor_adt = adt->priv;

	sensor_adt->caller = caller;
	sensor_adt->callback = callback;

	return 0;
}

const static struct pablo_sensor_adt_ops sensor_adt_ops = {
	.open					= pablo_sensor_adt_open,
	.close					= pablo_sensor_adt_close,
	.start					= pablo_sensor_adt_start,
	.stop					= pablo_sensor_adt_stop,
	.update_actuator_info			= pablo_sensor_adt_update_actuator_info,
	.get_sensor_info			= pablo_sensor_adt_get_sensor_info,
	.control_sensor				= pablo_sensor_adt_control_sensor,
	.register_control_sensor_callback	= pablo_sensor_adt_register_callback,
};

int pablo_sensor_adt_probe(struct pablo_sensor_adt *adt)
{
	probe_info("%s", __func__);

	atomic_set(&adt->rsccount, 0);
	adt->ops = &sensor_adt_ops;

	return 0;
}
KUNIT_EXPORT_SYMBOL(pablo_sensor_adt_probe);
