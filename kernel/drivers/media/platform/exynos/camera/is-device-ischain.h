/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_DEVICE_ISCHAIN_H
#define IS_DEVICE_ISCHAIN_H

#include "is-subdev-ctrl.h"
#include "is-groupmgr.h"
#include "is-resourcemgr.h"
#include "is-cmd.h"

#define IS_SETFILE_MASK		0x0000FFFF
#define IS_SCENARIO_MASK		0xFFFF0000
#define IS_SCENARIO_SHIFT		16

/*global state*/
enum is_ischain_state {
	IS_ISCHAIN_OPENING,
	IS_ISCHAIN_CLOSING,
	IS_ISCHAIN_OPEN,
	IS_ISCHAIN_INITING,
	IS_ISCHAIN_INIT,
	IS_ISCHAIN_START,
	IS_ISCHAIN_LOADED,
	IS_ISCHAIN_POWER_ON,
	IS_ISCHAIN_OPEN_STREAM,
	IS_ISCHAIN_REPROCESSING,
	IS_ISCHAIN_MULTI_CH,
};

enum is_stream_type {
	IS_PREVIEW_STREAM = 0,
	IS_REPROCESSING_STREAM,
};

enum is_fast_ctl_state {
	IS_FAST_CTL_FREE,
	IS_FAST_CTL_REQUEST,
	IS_FAST_CTL_STATE,
};

struct is_fast_ctl {
	struct list_head	list;
	u32			state;

	bool			lens_pos_flag;
	uint32_t		lens_pos;
};

#define MAX_NUM_FAST_CTL	5
struct fast_control_mgr {
	u32			fast_capture_count;

	spinlock_t		slock;
	struct is_fast_ctl	fast_ctl[MAX_NUM_FAST_CTL];
	u32			queued_count[IS_FAST_CTL_STATE];
	struct list_head	queued_list[IS_FAST_CTL_STATE];
};

struct pablo_device_ischain_ops {
	int (*power)(struct is_device_ischain *, int);
};

struct is_device_ischain {
	char					*stm;
	u32					instance; /* logical stream id */

	struct platform_device			*pdev;

	struct is_resourcemgr			*resourcemgr;

	struct is_region			*is_region;

	unsigned long				state;
	atomic_t				group_open_cnt;
	atomic_t				open_cnt;
	atomic_t				init_cnt;

	u32					setfile;
	bool					llc_mode; /* 0: LLC off, 1: LLC on */

	struct is_group				*group[GROUP_SLOT_MAX];

	struct is_device_sensor			*sensor;

	/* Async metadata control to reduce frame delay */
	struct fast_control_mgr			fastctlmgr;

	ulong					ginstance_map;

	struct pablo_interface_irta		*pii;

	u32 yuv_max_width;
	u32 yuv_max_height;
	const struct pablo_device_ischain_ops *ops;
};

#define CALL_DEVICE_ISCHAIN_OPS(device, op, args...)                                               \
	(((device)->ops && (device)->ops->op) ? ((device)->ops->op(device, args)) : 0)

/*global function*/
int is_ischain_probe(struct is_device_ischain *device,
	struct is_resourcemgr *resourcemgr,
	struct platform_device *pdev,
	u32 instance);
void is_ischain_remove(struct is_device_ischain *device);
int is_ischain_g_ddk_capture_meta_update(struct is_group *group, struct is_device_ischain *device,
	u32 fcount, u32 size, ulong addr);
int is_ischain_g_ddk_setfile_version(struct is_device_ischain *device,
	void *user_ptr);
int is_ischain_g_capability(struct is_device_ischain *this,
	ulong user_ptr);

int is_ischain_open_wrap(struct is_device_ischain *device, bool EOS);
int is_ischain_close_wrap(struct is_device_ischain *device);
int is_ischain_start_wrap(struct is_device_ischain *device,
	struct is_group *group);
int is_ischain_stop_wrap(struct is_device_ischain *device,
	struct is_group *group);

int is_ischain_group_open(struct is_device_ischain *device,
			struct is_video_ctx *vctx, u32 group_id);
int is_ischain_group_close(struct is_device_ischain *device, struct is_group *group);
int is_ischain_group_s_input(struct is_device_ischain *device,
	struct is_group *group,
	u32 stream_type,
	u32 position,
	u32 input_type,
	u32 stream_leader);

int is_itf_stream_on(struct is_device_ischain *this);
int is_itf_stream_off(struct is_device_ischain *this);
int is_itf_process_start(struct is_device_ischain *device, struct is_group *head);
int is_itf_process_stop(struct is_device_ischain *device, struct is_group *head, u32 fstop);
int is_itf_grp_shot(struct is_device_ischain *device,
	struct is_group *group,
	struct is_frame *frame);

int is_itf_s_param(struct is_device_ischain *device,
	struct is_frame *frame,
	IS_DECLARE_PMAP(pmap));
void * is_itf_g_param(struct is_device_ischain *device,
	struct is_frame *frame,
	u32 index);
int is_itf_preload_setfile(void);
int is_ischain_update_sensor_mode(struct is_device_ischain *device,
	struct is_frame *frame);

struct is_queue_ops *is_get_ischain_device_qops(void);

int is_ischain_power(struct is_device_ischain *this, int on);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct pablo_kunit_device_ischain_func {
	int (*itf_open)(struct is_device_ischain *, u32);
	int (*itf_close)(struct is_device_ischain *);
	int (*itf_setfile)(struct is_device_ischain *);
	int (*g_ddk_capture_meta_update)(struct is_group *, struct is_device_ischain *, u32, u32, ulong);
	int (*g_ddk_setfile_version)(struct is_device_ischain *, void *);
	int (*open)(struct is_device_ischain *);
	int (*close)(struct is_device_ischain *);
	int (*change_vctx_group)(struct is_device_ischain *, struct is_video_ctx *);
	int (*group_open)(struct is_device_ischain *, struct is_video_ctx *, u32);
	int (*group_close)(struct is_device_ischain *, struct is_group *);
	int (*group_s_input)(struct is_device_ischain *, struct is_group *, u32, u32, u32, u32);
	struct is_queue_ops *(*get_device_qops)(void);
};

struct pablo_kunit_device_ischain_func *pablo_kunit_get_device_ischain_func(void);
#endif
#endif
