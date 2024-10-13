// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include "is-interface-ddk.h"
#include "is-metadata-vendor.h"

static u32 object[1];

static int mock_chain_create(u32 chain_id, ulong addr, u32 offset,
				const struct lib_callback_func *cb)
{
	return 0;
}

static int mock_object_create(void **pobject, u32 obj_info, void *hw)
{
	*pobject = object;

	return 0;
}

static int mock_chain_destroy(u32 chain_id)
{
	return 0;
}

static int mock_object_destroy(void *object, u32 sensor_id)
{
	return 0;
}

static int mock_stop(void *object, u32 instance_id, u32 immediate)
{
	return 0;
}

static int mock_recovery(u32 instance_id)
{
	return 0;
}

static int mock_set_param(void *object, void *param_set)
{
	return 0;
}

static int mock_set_ctrl(void *object, u32 instance_id, u32 frame_number,
			struct camera2_shot *shot)
{
	return 0;
}

static int mock_shot(void *object, void *param_set, struct camera2_shot *shot, u32 num_buffers)
{
	return 0;
}

static int mock_get_meta(void *object, u32 instance_id, u32 frame_number,
			struct camera2_shot *shot)
{
	return 0;
}

#if IS_ENABLED(USE_DDK_INTF_CAP_META)
static int mock_get_cap_meta(void *object, u32 instance_id, u32 frame_number,
			u32 size, ulong addr)
{
	return 0;
}
#endif

static int mock_create_tune_set(void *isp_object, u32 instance_id, struct lib_tune_set *set)
{
	return 0;
}

static int mock_apply_tune_set(void *isp_object, u32 instance_id, u32 index, u32 scenario_idx)
{
	return 0;
}

static int mock_delete_tune_set(void *isp_object, u32 instance_id, u32 index)
{
	return 0;
}

static int mock_set_line_buffer_offset(u32 index, u32 num_set, u32 *offset)
{
	return 0;
}

static int mock_change_chain(void *isp_object, u32 instance_id, u32 chain_id)
{
	return 0;
}

static int mock_load_cal_data(void *isp_object, u32 instance_id, ulong kvaddr)
{
	return 0;
}

static int mock_get_cal_data(void *isp_object, u32 instance_id,
			struct cal_info *data, int type)
{
	return 0;
}

static int mock_sensor_info_mode_chg(void *isp_object, u32 instance_id,
		struct camera2_shot *shot)
{
	return 0;
}

static int mock_sensor_update_ctl(void *isp_object, u32 instance_id,
				u32 frame_count, struct camera2_shot *shot)
{
	return 0;
}

static int mock_set_system_config(struct lib_system_config *config)
{
	return 0;
}

static int mock_set_line_buffer_trigger(u32 trigger_value, u32 trigger_event)
{
	return 0;
}

static int mock_event_notifier(int hw_id, int instance_id, u32 fcount,
			int event_id, u32 strip_index, void *data)
{
	return 0;
}

static struct lib_interface_func lib_itf_func = {
	.chain_create = mock_chain_create,
	.object_create = mock_object_create,
	.chain_destroy = mock_chain_destroy,
	.object_destroy = mock_object_destroy,
	.stop = mock_stop,
	.recovery = mock_recovery,
	.set_param = mock_set_param,
	.set_ctrl = mock_set_ctrl,
	.shot = mock_shot,
	.get_meta = mock_get_meta,
#if IS_ENABLED(USE_DDK_INTF_CAP_META)
	.get_cap_meta = mock_get_cap_meta,
#endif
	.create_tune_set = mock_create_tune_set,
	.apply_tune_set = mock_apply_tune_set,
	.delete_tune_set = mock_delete_tune_set,
	.set_line_buffer_offset = mock_set_line_buffer_offset,
	.change_chain = mock_change_chain,
	.load_cal_data = mock_load_cal_data,
	.get_cal_data = mock_get_cal_data,
	.sensor_info_mode_chg = mock_sensor_info_mode_chg,
	.sensor_update_ctl = mock_sensor_update_ctl,
	.set_system_config = mock_set_system_config,
	.set_line_buffer_trigger = mock_set_line_buffer_trigger,
	.event_notifier = mock_event_notifier,
};

void pablo_kunit_make_mock_interface_ddk(struct is_lib_isp *lib_isp)
{
	lib_isp->func = &lib_itf_func;
	lib_isp->object = object;
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_make_mock_interface_ddk);

u32 get_lib_func_mock(u32 type, void **lib_func)
{
	*lib_func = &lib_itf_func;

	return 0;
}

MODULE_LICENSE("GPL v2");
