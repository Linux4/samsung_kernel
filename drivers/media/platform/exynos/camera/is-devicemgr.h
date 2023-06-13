/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef IS_DEVICE_MGR_H
#define IS_DEVICE_MGR_H

#include "is-config.h"
#include "is-device-ischain.h"
#include "is-device-sensor.h"

#define GET_HEAD_GROUP_IN_DEVICE(type, group)			\
	({ struct is_group *head;				\
	 head = group ? (group)->head : NULL;			\
	 while (head) {						\
		if (head->device_type == type)			\
			break;					\
		else if (head->instance != (group)->instance)	\
			head = head->pnext;			\
		else						\
			head = head->child;			\
	 }; head;})

#define GET_OUT_FLAG_IN_DEVICE(device_type, out_flag)				\
	({unsigned long tmp_out_flag;						\
	if (device_type == IS_DEVICE_ISCHAIN)				\
		tmp_out_flag = ((out_flag) & (~((1 << ENTRY_3AA) - 1)));	\
	else									\
		tmp_out_flag = ((out_flag) & ((1 << ENTRY_3AA) - 1));		\
	tmp_out_flag;})

struct devicemgr_sensor_tag_data {
	struct is_devicemgr	*devicemgr;
	struct is_group		*group;
	u32			fcount;
	u32			stream;
};

struct is_devicemgr {
	struct is_device_sensor		*sensor[IS_STREAM_COUNT];
	struct is_device_ischain		*ischain[IS_STREAM_COUNT];
	struct tasklet_struct			tasklet[IS_STREAM_COUNT];
	struct devicemgr_sensor_tag_data	tag_data[IS_STREAM_COUNT];
};

struct is_group *get_ischain_leader_group(struct is_device_ischain *device);
int is_devicemgr_open(void *device, enum is_device_type type);
int is_devicemgr_binding(struct is_device_sensor *sensor,
		struct is_device_ischain *ischain,
		enum is_device_type type);
int is_devicemgr_start(void *device, enum is_device_type type);
int is_devicemgr_stop(void *device, enum is_device_type type);
int is_devicemgr_close(void *device, enum is_device_type type);
int is_devicemgr_shot_callback(struct is_group *group,
		struct is_frame *frame,
		u32 fcount,
		enum is_device_type type);
int is_devicemgr_shot_done(struct is_group *group,
		struct is_frame *ldr_frame,
		u32 status);
void is_devicemgr_late_shot_handle(struct is_group *group,
		struct is_frame *frame, u32 status);
#endif
