/*
 * Samsung Exynos SoC series FIMC-IS2 driver
 *
 * exynos fimc-is2 device interface functions
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_DEVICE_ITF_H
#define IS_DEVICE_ITF_H

#include "is-core.h"
#include "is-groupmgr.h"
#include "is-device-ischain.h"

int is_itf_s_param_wrap(struct is_device_ischain *device,
		IS_DECLARE_PMAP(pmap));
int is_itf_chg_setfile(struct is_device_ischain *device);
int is_itf_open_wrap(struct is_device_ischain *device, u32 flag);
int is_itf_close_wrap(struct is_device_ischain *device);
int is_itf_change_chain_wrap(struct is_group *group, u32 next_id);
int is_itf_setfile_wrap(struct is_device_ischain *device);
int is_itf_stream_on_wrap(struct is_device_ischain *device);
int is_itf_stream_off_wrap(struct is_device_ischain *device);
int is_itf_process_on_wrap(struct is_device_ischain *device, struct is_group *head);
int is_itf_process_off_wrap(struct is_device_ischain *device, struct is_group *head,
	u32 fstop);
int is_itf_icpu_start_wrap(struct is_device_ischain *device);
int is_itf_icpu_stop_wrap(struct is_device_ischain *device);
void is_itf_sudden_stop_wrap(struct is_device_ischain *device, u32 instance, struct is_group *group);
int is_itf_power_down_wrap(struct is_core *core);
int is_itf_sensor_mode_wrap(struct is_device_ischain *device,
	struct is_sensor_cfg *cfg);
int is_itf_shot_wrap(struct is_group *group, struct is_frame *frame);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct pablo_kunit_itf_wrap_ops {
	int (*change_chain)(struct is_group *, u32);
	void (*sudden_stop)(struct is_device_ischain *, u32, struct is_group *);
	int (*sensor_mode)(struct is_device_ischain *, struct is_sensor_cfg *);
	int (*shot)(struct is_group *group, struct is_frame *frame);
};
struct pablo_kunit_itf_wrap_ops *pablo_kunit_get_itf_wrap(void);
#endif
#endif
