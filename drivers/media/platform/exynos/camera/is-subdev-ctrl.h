// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_SUBDEV_H
#define IS_SUBDEV_H

#include "is-video.h"
#include "pablo-work.h"

#define SUBDEV_INTERNAL_BUF_MAX		(8)

struct is_device_sensor;
struct is_device_ischain;
struct is_subdev;
struct is_frame;

enum is_subdev_device_type {
	IS_SENSOR_SUBDEV,
	IS_ISCHAIN_SUBDEV,
};

enum is_subdev_state {
	IS_SUBDEV_OPEN,
	IS_SUBDEV_START,
	IS_SUBDEV_RUN,
	IS_SUBDEV_FORCE_SET,
	IS_SUBDEV_EXTERNAL_USE,
	IS_SUBDEV_INTERNAL_USE,
	IS_SUBDEV_INTERNAL_S_FMT,
	IS_SUBDEV_VOTF_USE,
};

enum pablo_subdev_get_type {
	PSGT_REGION_NUM,
};

struct is_subdev_path {
	u32		width;
	u32		height;
	struct is_crop	canv;
	struct is_crop	crop;
};

/* Caution: Do not exceed 64 */
enum is_subdev_id {
	ENTRY_SENSOR,
	ENTRY_SSVC0,
	ENTRY_SSVC1,
	ENTRY_SSVC2,
	ENTRY_SSVC3,
	ENTRY_SSVC4,
	ENTRY_SSVC5,
	ENTRY_SSVC6,
	ENTRY_SSVC7,
	ENTRY_3AA,
	ENTRY_ISP,
	ENTRY_MCS,
	ENTRY_M0P,
	ENTRY_M1P,
	ENTRY_M2P,
	ENTRY_M3P,
	ENTRY_M4P,
	ENTRY_M5P,
	ENTRY_PAF,	/* PDP(PATSTAT) Bayer RDMA */
	ENTRY_PDAF,	/* PDP(PATSTAT) AF RDMA */
	ENTRY_PDST,	/* PDP(PATSTAT) PD STAT WDMA */
	ENTRY_ORB,	/* ORBMCH */
	ENTRY_ORBXC,/* ORB */
	ENTRY_YPP,	/* YUVPP */
	ENTRY_SHRP,	/* SHRP */
	ENTRY_LME,
	ENTRY_LMES,
	ENTRY_LMEC,

	ENTRY_BYRP,
	ENTRY_RGBP,
	ENTRY_RGBP_HF,
	ENTRY_MCFP,

	ENTRY_INTERNAL,

	ENTRY_END,
};

#define ENTRY_SS_VC0		ENTRY_SSVC0
#define ENTRY_SS_VC1		ENTRY_SSVC1
#define ENTRY_SS_VC2		ENTRY_SSVC2
#define ENTRY_SS_VC3		ENTRY_SSVC3
#define ENTRY_SS_VC4		ENTRY_3AA
#define ENTRY_SS_VC5		ENTRY_3AC
#define ENTRY_SS_VC6		ENTRY_3AP
#define ENTRY_SS_VC7		ENTRY_3AF
#define ENTRY_SS_VC8		ENTRY_3AG
#define ENTRY_SS_VC9		ENTRY_3AO
#define ENTRY_SS_MCB0		ENTRY_3AL
#define ENTRY_SS_MCB1		ENTRY_ISP
#define ENTRY_SS_MCB2		ENTRY_IXC
#define ENTRY_SS_MCB3		ENTRY_IXP
#define ENTRY_SS_BNS		ENTRY_IXT
#define ENTRY_CSTAT		ENTRY_IXG
#define ENTRY_CSTAT_LME_DS0	ENTRY_IXV
#define ENTRY_CSTAT_LME_DS1	ENTRY_IXW
#define ETNRY_CSTAT_FDPIG	ENTRY_MEXC
#define ENTRY_CSTAT_RGBHIST	ENTRY_CLH
#define ENTRY_CSTAT_SVHIST	ENTRY_CLHC
#define ENTRY_CSTAT_DRC		ENTRY_ORBXC
#define ENTRY_YUVP		ENTRY_YPP
#define ENTRY_MCSC		ENTRY_MCS
#define ENTRY_MCSC_P0		ENTRY_M0P
#define ENTRY_MCSC_P1		ENTRY_M1P
#define ENTRY_MCSC_P2		ENTRY_M2P
#define ENTRY_MCSC_P3		ENTRY_M3P
#define ENTRY_MCSC_P4		ENTRY_M4P
#define ENTRY_MCSC_P5		ENTRY_M5P
#define ENTRY_LME_PREV		ENTRY_LMES
#define ENTRY_LME_PURE		ENTRY_LMEC

struct is_subdev_ops {
	int (*bypass)(struct is_subdev *subdev,
		void *device_data,
		struct is_frame *frame,
		bool bypass);
	int (*cfg)(struct is_subdev *subdev,
		void *device_data,
		struct is_frame *frame,
		struct is_crop *incrop,
		struct is_crop *otcrop,
		IS_DECLARE_PMAP(pmap));
	int (*tag)(struct is_subdev *subdev,
		void *device_data,
		struct is_frame *frame,
		struct camera2_node *node);
	int (*get)(struct is_subdev *subdev,
		   struct is_device_ischain *idi,
		   struct is_frame *frame,
		   enum pablo_subdev_get_type type,
		   void *result);
};

enum subdev_ch_mode {
	SCM_WO_PAF_HW,
	SCM_W_PAF_HW,
	SCM_MAX,
};

struct is_subdev {
	u32				id;
	u32				vid; /* video id */
	u32				cid; /* capture node id */
	char				name[4];
	char				*stm;
	u32				instance;
	unsigned long			state;

	u32				constraints_width; /* spec in width */
	u32				constraints_height; /* spec in height */

	u32				param_otf_in;
	u32				param_dma_in;
	u32				param_otf_ot;
	u32				param_dma_ot;

	struct is_subdev_path		input;
	struct is_subdev_path		output;

	struct list_head		list;

	/* for internal use */
	struct is_framemgr		internal_framemgr;
	u32				batch_num;
	u32				buffer_num;
	u32				bits_per_pixel;
	u32				memory_bitwidth;
	u32				sbwc_type;
	u32				lossy_byte32num;
	struct is_priv_buf		*pb_subdev[SUBDEV_INTERNAL_BUF_MAX];
	char				data_type[15];

	struct is_video_ctx		*vctx;
	struct is_subdev		*leader;
	const struct is_subdev_ops	*ops;
};

int is_sensor_subdev_open(struct is_device_sensor *device,
	struct is_video_ctx *vctx);
int is_sensor_subdev_close(struct is_device_sensor *device,
	struct is_video_ctx *vctx);

int is_ischain_subdev_open(struct is_device_ischain *device,
	struct is_video_ctx *vctx);
int is_ischain_subdev_close(struct is_device_ischain *device,
	struct is_video_ctx *vctx);

/*common subdev*/
int is_subdev_probe(struct is_subdev *subdev,
	u32 instance,
	u32 id,
	const char *name,
	const struct is_subdev_ops *sops);
int is_subdev_open(struct is_subdev *subdev, struct is_video_ctx *vctx, u32 instance);
int is_subdev_close(struct is_subdev *subdev);
int pablo_subdev_buffer_init(struct is_subdev *is, struct vb2_buffer *vb);
int is_subdev_buffer_queue(struct is_subdev *subdev, struct vb2_buffer *vb);
int is_subdev_buffer_finish(struct is_subdev *subdev, struct vb2_buffer *vb);

struct is_queue_ops *is_get_sensor_subdev_qops(void);
struct is_queue_ops *is_get_ischain_subdev_qops(void);

void is_subdev_internal_get_sbwc_type(const struct is_subdev *subdev,
					u32 *sbwc_type, u32 *lossy_byte32num);
int is_subdev_internal_get_buffer_size(const struct is_subdev *subdev,
					u32 *width, u32 *height,
					u32 *sbwc_block_width, u32 *sbwc_block_height);
/* internal subdev use */
int is_subdev_internal_open(u32 instance, int vid,
				struct is_subdev *subdev);
int is_subdev_internal_close(struct is_subdev *subdev);
int is_subdev_internal_s_format(struct is_subdev *subdev,
				u32 width, u32 height, u32 bits_per_pixel,
				u32 sbwc_type, u32 lossy_byte32num,
				u32 buffer_num, const char *type_name);
struct is_sensor_cfg;
int is_subdev_internal_g_bpp(struct is_subdev *subdev,
	struct is_sensor_cfg *sensor_cfg);
int is_subdev_internal_start(struct is_subdev *subdev);
int is_subdev_internal_stop(struct is_subdev *subdev);

int __mcsc_dma_out_cfg(struct is_device_ischain *device,
	struct is_frame *ldr_frame,
	struct camera2_node *node,
	u32 pindex,
	IS_DECLARE_PMAP(pmap),
	int index);

#define GET_SUBDEV_FRAMEMGR(subdev) \
	({ struct is_framemgr *framemgr;						\
	if ((subdev) && (subdev)->vctx)							\
		framemgr = &(subdev)->vctx->queue.framemgr;				\
	else if ((subdev) && test_bit(IS_SUBDEV_INTERNAL_USE, &((subdev)->state)))	\
		framemgr = &(subdev)->internal_framemgr;				\
	else										\
		framemgr = NULL;							\
	framemgr;})

#define GET_SUBDEV_I_FRAMEMGR(subdev)				\
	({ struct is_framemgr *framemgr;			\
	if (subdev)						\
		framemgr = &(subdev)->internal_framemgr;	\
	else							\
		framemgr = NULL;				\
	framemgr; })

#define GET_SUBDEV_QUEUE(subdev) \
	(((subdev) && (subdev)->vctx) ? (&(subdev)->vctx->queue) : NULL)
#define CALL_SOPS(s, op, args...)	(((s) && (s)->ops && (s)->ops->op) ? ((s)->ops->op(s, args)) : 0)
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#endif
