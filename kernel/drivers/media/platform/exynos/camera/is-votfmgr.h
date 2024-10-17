/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_VOTF_MGR_H
#define IS_VOTF_MGR_H

#include "votf/pablo-votf.h"
#include "is-groupmgr.h"

#define NUM_OF_VOTF_BUF		(1)

#if IS_ENABLED(CONFIG_VIDEO_EXYNOS_PABLO_LIB_VOTF) || IS_ENABLED(CONFIG_VIDEO_EXYNOS_CAMERA_POSTPROCESS_VOTF)
int is_votf_check_wait_con(struct is_group *group);
int is_votf_check_invalid_state(struct is_group *group);
int is_votf_create_link(struct is_group *group, u32 width, u32 height);
int is_votf_destroy_link(struct is_group *group);
int is_votf_check_link(struct is_group *group);

struct is_framemgr *is_votf_get_framemgr(struct is_group *group, enum votf_service type,
	unsigned long id);
struct is_frame *is_votf_get_frame(struct is_group *group, enum votf_service type,
	unsigned long id, u32 fcount);
int is_votf_register_framemgr(struct is_group *group, enum votf_service type,
	void *data, const struct votf_ops *ops, unsigned long subdev_id);
int is_votf_link_set_service_cfg(struct votf_info *src_info, struct votf_info *dst_info,
	u32 width, u32 height, u32 change);
int __is_votf_force_flush(struct votf_info *src_info, struct votf_info *dst_info);
#else
static inline int is_votf_check_wait_con(struct is_group *group) {return 0;}
static inline int is_votf_check_invalid_state(struct is_group *group) {return 0;}
static inline int is_votf_create_link(struct is_group *group, u32 width, u32 height) {return 0;}
static inline int is_votf_destroy_link(struct is_group *group) {return 0;}
static inline int is_votf_check_link(struct is_group *group) {return 0;}

static inline struct is_framemgr *is_votf_get_framemgr(struct is_group *group, enum votf_service type,
	unsigned long id) {return 0;}
static inline struct is_frame *is_votf_get_frame(struct is_group *group, enum votf_service type,
	unsigned long id, u32 fcount) {return 0;}
static inline int is_votf_register_framemgr(struct is_group *group, enum votf_service type,
	void *data, const struct votf_ops *ops, unsigned long subdev_id) {return 0;}
static inline int is_votf_link_set_service_cfg(struct votf_info *src_info, struct votf_info *dst_info,
	u32 width, u32 height, u32 change) {return 0;}
static inline int __is_votf_force_flush(struct votf_info *src_info, struct votf_info *dst_info) {return 0;}
#endif
#define votf_fmgr_call(mgr, o, f, args...)				\
	({								\
		int __result = 0;					\
		if (!(mgr))						\
			__result = -ENODEV;				\
		else if (!((mgr)->o.ops) || !((mgr)->o.ops->f))		\
			__result = -ENOIOCTLCMD;			\
		else							\
			(mgr)->o.ops->f((mgr)->o.data, mgr->o.id, ##args);	\
		__result;						\
	})
struct pablo_votfitf_ops {
	int (*subdev_call)(struct v4l2_subdev *device, int *dma_ch);
	struct is_module_enum *(*subdev_data)(struct v4l2_subdev *device);
	int (*check_wait_con)(struct votf_info *s_vinfo, struct votf_info *d_vinfo);
	int (*check_invalid_state)(struct votf_info *s_vinfo, struct votf_info *d_vinfo);
	int (*create_link)(int src_ip, int dst_ip);
	int (*create_ring)(void);
	int (*set_flush)(struct votf_info *vinfo);
	struct is_subdev *(*get_slave_leader_subdev)(struct is_group *group, enum votf_service type);
};
struct pablo_votfitf_ops *pablo_get_votfitf(void);
#endif
