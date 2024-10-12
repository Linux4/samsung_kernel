/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos pablo video node functions
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_FRAME_MGR_CFG_H
#define IS_FRAME_MGR_CFG_H

#include "is-time-config.h"
#include "is-param.h"
#include "exynos-is-sensor.h"

#define MAX_FRAME_INFO		(4)

enum is_frame_info_index {
	INFO_FRAME_START,
	INFO_CONFIG_LOCK,
	INFO_FRAME_END_PROC
};

struct is_frame_info {
	int			cpu;
	int			pid;
	unsigned long long	when;
};

struct is_stripe_size {
	/* Horizontal pixel ratio how much stripe processing is done. */
	u32	h_pix_ratio;
	/* Horizontal pixel count which stripe processing is done for. */
	u32	h_pix_num[MAX_STRIPE_REGION_NUM];

	/* for backward compatibility */
	u32	offset_x;
	u32	crop_x;
	u32	crop_width;
	u32	left_margin;
	u32	right_margin;
};

enum pablo_repeat_scenario {
	PABLO_REPEAT_NO,
	PABLO_REPEAT_YUVP_CLAHE,
};

struct pablo_repeat_info {
	u32				num;
	u32				idx;
	enum pablo_repeat_scenario	scenario;
};

struct is_stripe_info {
	/* Region index. */
	u32				region_id;
	/* Total region num. */
	u32				region_num;
	/* Frame base address of Y, UV plane */
	u32				region_base_addr[2];
	/* Stripe size for incrop/otcrop */
	struct is_stripe_size	in;
	struct is_stripe_size	out;
	/* For image dump */
	ulong                           kva[MAX_STRIPE_REGION_NUM][IS_MAX_PLANES];
	size_t                          size[MAX_STRIPE_REGION_NUM][IS_MAX_PLANES];

	/* for backward compatibility */
	u32				h_pix_num[MAX_STRIPE_REGION_NUM];
	u32				start_pos_x;
	u32				stripe_width;
};

struct is_frame {
	struct list_head	list;
	struct kthread_work	work;
	struct kthread_delayed_work dwork;
	void			*groupmgr;
	void			*group;
	void			*subdev; /* is_subdev */
	u32			hw_slot_id[HW_SLOT_MAX];

	/* group leader use */
	struct camera2_shot_ext	*shot_ext;
	struct camera2_shot	*shot;
	size_t			shot_size;

	/* stream use */
	struct camera2_stream	*stream;

	/* common use */
	u32			planes; /* total planes include multi-buffers */
	dma_addr_t		dvaddr_buffer[IS_MAX_PLANES];
	ulong			kvaddr_buffer[IS_MAX_PLANES];
	u32			size[IS_MAX_PLANES];

	/* external plane use */
	u32			ext_planes;
	ulong			kvaddr_ext[IS_EXT_MAX_PLANES];

	/*
	 * target address for capture node
	 * [0] invalid address, stop
	 * [others] valid address
	 */
	dma_addr_t dva_ssvc[CSI_VIRTUAL_CH_MAX][IS_MAX_PLANES];	/* Sensor LVN */

	dma_addr_t txcTargetAddress[IS_MAX_PLANES];	/* 3AA capture DMA */
	dma_addr_t txpTargetAddress[IS_MAX_PLANES];	/* 3AA preview DMA */
	dma_addr_t mrgTargetAddress[IS_MAX_PLANES];	/* 3AA merge DMA */
	dma_addr_t efdTargetAddress[IS_MAX_PLANES];	/* 3AA FDPIG DMA */
	dma_addr_t ixcTargetAddress[IS_MAX_PLANES];	/* DNS YUV out DMA */
	dma_addr_t ixpTargetAddress[IS_MAX_PLANES];
	dma_addr_t ixvTargetAddress[IS_MAX_PLANES];	/* TNR PREV out DMA */
	dma_addr_t ixwTargetAddress[IS_MAX_PLANES];	/* TNR PREV weight out DMA */
	dma_addr_t ixtTargetAddress[IS_MAX_PLANES];	/* TNR PREV in DMA */
	dma_addr_t ixgTargetAddress[IS_MAX_PLANES];	/* TNR PREV weight in DMA */
	u64 mexcTargetAddress[IS_MAX_PLANES];		/* ME or MCH out DMA */
	u64 orbxcKTargetAddress[IS_MAX_PLANES];		/* ME or MCH out DMA */
	dma_addr_t sc0TargetAddress[IS_MAX_PLANES];
	dma_addr_t sc1TargetAddress[IS_MAX_PLANES];
	dma_addr_t sc2TargetAddress[IS_MAX_PLANES];
	dma_addr_t sc3TargetAddress[IS_MAX_PLANES];
	dma_addr_t sc4TargetAddress[IS_MAX_PLANES];
	dma_addr_t sc5TargetAddress[IS_MAX_PLANES];
	dma_addr_t clxsTargetAddress[IS_MAX_PLANES];	/* CLAHE IN DMA */
	dma_addr_t clxcTargetAddress[IS_MAX_PLANES];	/* CLAHE OUT DMA */

	/* multi-buffer use */
	/* total number of buffers per frame */
	u32			num_buffers;
	/* current processed buffer index */
	u32			cur_buf_index;

	/* internal use */
	unsigned long		mem_state;
	u32			state;
	u32			fcount;
	u32			rcount;
	u32			index;
	u32			result;
	unsigned long		out_flag;
	unsigned long		bak_flag;
	IS_DECLARE_PMAP(pmap);
	struct is_param_region	*parameter;

	struct is_frame_info frame_info[MAX_FRAME_INFO];
	u32			instance; /* device instance */
	u32			type;
	unsigned long		core_flag;

#ifdef ENABLE_SYNC_REPROCESSING
	struct list_head	sync_list;
	struct list_head	preview_list;
#endif
	struct list_head	votf_list;

	/* for NI(noise index from DDK) use */
	u32			noise_idx; /* Noise index */

#ifdef MEASURE_TIME
	/* time measure externally */
	struct timespec64	*tzone;
	/* time measure internally */
	struct is_monitor	mpoint[TMS_END];
#endif

	/* for draw digit */
	u32			width;
	u32			height;

	struct pablo_repeat_info	repeat_info;
	struct is_stripe_info		stripe_info;
};

#endif
