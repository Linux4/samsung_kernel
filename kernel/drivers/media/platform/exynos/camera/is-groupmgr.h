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

#ifndef IS_GROUP_MGR_H
#define IS_GROUP_MGR_H

#include "is-config.h"
#include "is-time.h"
#include "is-subdev-ctrl.h"
#include "is-video.h"
#include "is_groupmgr_config.h"
#include "pablo-fpsimd.h"

enum is_group_state {
	IS_GROUP_OPEN,
	IS_GROUP_INIT,
	IS_GROUP_START,
	IS_GROUP_SHOT,
	IS_GROUP_REQUEST_FSTOP,
	IS_GROUP_FORCE_STOP,
	IS_GROUP_OTF_INPUT,
	IS_GROUP_OTF_OUTPUT,
	IS_GROUP_VOTF_INPUT,
	IS_GROUP_VOTF_OUTPUT,
	IS_GROUP_STANDBY,
	IS_GROUP_VOTF_CONN_LINK,
	IS_GROUP_USE_MULTI_CH,
	IS_GROUP_STRGEN_INPUT,
	IS_GROUP_REPROCESSING,
};

enum is_group_input_type {
	GROUP_INPUT_MEMORY,
	GROUP_INPUT_OTF,
	GROUP_INPUT_PIPE,
	GROUP_INPUT_SEMI_PIPE,
	GROUP_INPUT_VOTF,
	GROUP_INPUT_STRGEN,
	GROUP_INPUT_MAX,
};

struct is_frame;
struct is_device_ischain;
struct is_group;
struct is_groupmgr;

typedef int (*is_shot_callback)(struct is_device_ischain *device,
	struct is_group *group,
	struct is_frame *frame);

#define for_each_group_child(pos, grp)	\
	for (pos = (grp); pos; pos = pos->child)
#define for_each_group_child_double_path(pos, tmp, grp)	\
	for (pos = (grp),				\
		tmp = (grp) ? (grp)->pnext : NULL;	\
	     pos;					\
	     pos = pos->child ? pos->child : tmp,	\
		tmp = (pos == tmp) ? NULL : tmp)

#define CALL_GROUP_OPS(group, op, args...)	\
	(((group)->ops && (group)->ops->op) ? ((group)->ops->op(group, ##args)) : 0)

struct is_group_ops {
	int (*open)(struct is_group *group, u32 id, struct is_video_ctx *vctx);
	int (*close)(struct is_group *group);
	int (*init)(struct is_group *group, u32 input_type, u32 stream_leader, u32 stream_type);
	int (*start)(struct is_group *group);
	int (*stop)(struct is_group *group);
	int (*buffer_init)(struct is_group *group, u32 index);
	int (*buffer_queue)(struct is_group *group, struct is_queue *queue, u32 index);
	int (*buffer_finish)(struct is_group *group, u32 index);
	int (*shot)(struct is_group *group, struct is_frame *frame);
	int (*done)(struct is_group *group, struct is_frame *frame, u32 done_state);
	int (*change_chain)(struct is_group *group, u32 next_id);
	int (*lock)(struct is_group *group, enum is_device_type device_type, bool leader_lock,
		unsigned long *flags);
	void (*unlock)(struct is_group *group, unsigned long flags, enum is_device_type device_type,
		       bool leader_lock);
	void (*subdev_cancel)(struct is_group *group, struct is_frame *ldr_frame,
			      enum is_device_type device_type, enum is_frame_state frame_state,
			      bool flush);
};

struct is_group {
	char				*stm;
	u32				instance;	/* logical stream id */
	u32				logical_id;	/* logical group id */
	u32				id;		/* physical group id */
	u32				slot;		/* physical group slot */

	const struct is_hw_group_ops	*hw_grp_ops;
	u32				hw_slot_id[HW_SLOT_MAX];

	struct is_group			*next;
	struct is_group			*prev;
	struct is_group			*gnext;
	struct is_group			*gprev;
	struct is_group			*parent;
	struct is_group			*child;
	struct is_group			*head;
	struct is_group			*tail;
	struct is_group			*pnext;
	struct is_group			*ptail;

	struct is_subdev		leader;
	struct is_subdev		*junction;
	struct is_subdev		*subdev[ENTRY_END];
	struct is_framemgr		*locked_sub_framemgr[ENTRY_END];

	struct list_head		subdev_list;

	/* for otf interface */
	atomic_t			sensor_fcount;
	atomic_t			backup_fcount;
	struct semaphore		smp_trigger;
	atomic_t			smp_shot_count;
	u32				init_shots;
	u32				asyn_shots;
	u32				sync_shots;
	u32				skip_shots;

	struct camera2_aa_ctl		intent_ctl;
	struct camera2_lens_ctl		lens_ctl;

	u32				pcount; /* program count */
	u32				fcount; /* frame count */
	atomic_t			scount; /* shot count */
	atomic_t			rcount; /* request count */
	unsigned long			state;
	u32				input_type;

	is_shot_callback		shot_callback;
	struct is_device_ischain	*device;
	struct is_device_sensor		*sensor;
	enum is_device_type		device_type;

#ifdef DEBUG_AA
#ifdef DEBUG_FLASH
	enum aa_ae_flashmode		flashmode;
	struct camera2_flash_dm		flash;
#endif
#endif

#ifdef MEASURE_TIME
#ifdef MONITOR_TIME
	struct is_time		time;
#endif
#endif
	u32				aeflashMode; /* Flash Mode Control */
	u32				remainIntentCount;
	u32				remainCaptureIntentCount;
	u32				remainRemosaicCropIntentCount;
	u32				junction_vid;
	struct mutex			mlock_votf;
	spinlock_t			slock_s_ctrl;
	u32				thrott_tick;

	const struct is_group_ops	*ops;
};

enum is_group_task_state {
	IS_GTASK_OPEN,
	IS_GTASK_START,
	IS_GTASK_REQUEST_STOP
};

struct is_group_task {
	u32				id;
	struct task_struct		*task;
	struct kthread_worker		worker;
	struct semaphore		smp_resource;
	unsigned long			state;
	atomic_t			refcount;

#ifdef VH_FPSIMD_API
	struct is_kernel_fpsimd_state	fp_state;
#endif
	spinlock_t			gtask_slock;

	/* additional feature */
	struct pablo_sync_repro		*sync_repro;
};

struct is_groupmgr {
	struct is_group			*leader[IS_STREAM_COUNT];
	struct is_group			*group[IS_STREAM_COUNT][GROUP_SLOT_MAX];
	struct is_group_task		gtask[GROUP_ID_MAX];
};

struct is_groupmgr *is_get_groupmgr(void);
struct is_group *get_ischain_leader_group(u32 instance);
struct is_group *get_leader_group(u32 instance);
struct is_group_task *get_group_task(u32 gid);

int is_groupmgr_probe(void);
int is_groupmgr_init(struct is_device_ischain *device);
int is_groupmgr_start(struct is_device_ischain *device);
void is_groupmgr_stop(struct is_device_ischain *device);
void is_group_release(struct is_device_ischain *device, u32 slot);
int is_group_probe(struct is_device_sensor *sensor, struct is_device_ischain *device,
	is_shot_callback shot_callback, u32 slot);

void is_group_subdev_cancel(struct is_group *group,
		struct is_frame *ldr_frame,
		enum is_device_type device_type,
		enum is_frame_state frame_state,
		bool flush);

/* get head group's framemgr */
#define GET_HEAD_GROUP_FRAMEMGR(group) \
	(((group) && ((group)->head) && ((group)->head->leader.vctx)) ? (&((group)->head->leader).vctx->queue->framemgr) : NULL)

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct pablo_kunit_groupmgr_func {
	int (*group_lock)(struct is_group *group, enum is_device_type device_type, bool leader_lock,
		unsigned long *flags);
	void (*group_unlock)(struct is_group *, unsigned long, enum is_device_type, bool);
	void (*group_subdev_cancel)(struct is_group *, struct is_frame *, enum is_device_type,
				    enum is_frame_state, bool);
	void (*group_cancel)(struct is_group *, struct is_frame *);
	int (*group_task_open)(struct is_group_task *gtask, int slot);
	int (*group_task_close)(struct is_group_task *gtask);
	bool (*wait_req_frame_before_stop)(struct is_group *, struct is_group_task *,
					   struct is_framemgr *);
	bool (*wait_shot_thread)(struct is_group *);
	bool (*wait_pro_frame)(struct is_group *, struct is_framemgr *);
	bool (*wait_req_frame_after_stop)(struct is_group *, struct is_framemgr *);
	bool (*check_remain_req_count)(struct is_group *, struct is_group_task *);
	void (*group_override_meta)(struct is_group *, struct is_frame *, struct is_resourcemgr *,
				    struct is_device_sensor *);
	void (*group_override_sensor_req)(struct is_framemgr *, struct is_frame *);
	bool (*group_has_child)(struct is_group *, u32);
	bool (*group_throttling_tick_check)(struct is_device_ischain *, struct is_group *,
					    struct is_frame *);
	void (*group_shot_recovery)(struct is_groupmgr *, struct is_group *, struct is_group_task *,
				    bool, bool, bool[]);
	void (*group_release)(struct is_device_ischain *, u32);
	int (*group_probe)(struct is_device_sensor *, struct is_device_ischain *, is_shot_callback,
			   u32);
};

struct pablo_kunit_groupmgr_func *pablo_kunit_get_groupmgr_func(void);
#endif
#endif
