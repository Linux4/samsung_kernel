/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos pablo video node functions
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
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

#define MAX_OUT_LOGICAL_NODE	(1)
#define MAX_CAP_LOGICAL_NODE	(4)

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
	/* stripe width for sub-frame */
	u32 				stripe_w;
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

struct is_sub_frame {
	/* Target address for all input node
	 * 0: invalid address, DMA should be disabled
	 * any value: valid address
	 */
	u32			id;
	u32			num_planes;
	u32			hw_pixeltype;
	ulong			kva[IS_MAX_PLANES];
	dma_addr_t		dva[IS_MAX_PLANES];
	dma_addr_t		backup_dva[IS_MAX_PLANES];

	struct is_stripe_info	stripe_info;
};

struct is_sub_node {
	u32			hw_id;
	struct is_sub_frame	sframe[CAPTURE_NODE_MAX];
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

	/* dva for ICPU */
	dma_addr_t		shot_dva;

	/* stream use */
	struct camera2_stream	*stream;

	/* common use */
	u32			planes; /* total planes include multi-buffers */
	struct is_priv_buf	*pb_output;
	struct is_priv_buf	*pb_capture[CAPTURE_NODE_MAX];
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

	dma_addr_t txcTargetAddress[IS_MAX_PLANES];		/* 3AA capture WDMA */
	dma_addr_t txpTargetAddress[IS_MAX_PLANES];		/* 3AA preview WDMA */
	dma_addr_t mrgTargetAddress[IS_MAX_PLANES];		/* 3AA merge WDMA */
	dma_addr_t efdTargetAddress[IS_MAX_PLANES];		/* 3AA FDPIG WDMA */
	dma_addr_t txdgrTargetAddress[IS_MAX_PLANES];		/* 3AA DRC GRID WDMA */
	dma_addr_t txodsTargetAddress[IS_MAX_PLANES];		/* 3AA ORB DS WDMA */
	dma_addr_t txldsTargetAddress[IS_MAX_PLANES];		/* 3AA LME DS WDMA */
	dma_addr_t txhfTargetAddress[IS_MAX_PLANES];		/* 3AA HF WDMA */
	dma_addr_t lmesTargetAddress[IS_MAX_PLANES];       	/* LME prev IN DMA */
	u64 lmesKTargetAddress[IS_MAX_PLANES];			/* LME prev IN KVA */
	dma_addr_t lmecTargetAddress[IS_MAX_PLANES];       	/* LME OUT DMA */
	u64 lmecKTargetAddress[IS_MAX_PLANES];			/* LME OUT KVA */
	u64 lmemKTargetAddress[IS_MAX_PLANES];			/* LME SW OUT KVA */
	dma_addr_t ixcTargetAddress[IS_MAX_PLANES];		/* DNS YUV out WDMA */
	dma_addr_t ixpTargetAddress[IS_MAX_PLANES];
	dma_addr_t ixdgrTargetAddress[IS_MAX_PLANES];		/* DNS DRC GRID RDMA */
	dma_addr_t ixrrgbTargetAddress[IS_MAX_PLANES];		/* DNS REP RGB RDMA */
	dma_addr_t ixnoirTargetAddress[IS_MAX_PLANES];		/* DNS REP NOISE RDMA */
	dma_addr_t ixscmapTargetAddress[IS_MAX_PLANES];		/* DNS SC MAP RDMA */
	dma_addr_t ixnrdsTargetAddress[IS_MAX_PLANES];		/* DNS NR DS WDMA */
	dma_addr_t ixnoiTargetAddress[IS_MAX_PLANES];		/* DNS NOISE WDMA */
	dma_addr_t ixdgaTargetAddress[IS_MAX_PLANES];		/* DNS DRC GAIN WDMA */
	dma_addr_t ixsvhistTargetAddress[IS_MAX_PLANES];	/* DNS SV HIST WDMA */
	dma_addr_t ixhfTargetAddress[IS_MAX_PLANES];		/* DNS HF WDMA */
	dma_addr_t ixwrgbTargetAddress[IS_MAX_PLANES];		/* DNS REP RGB WDMA */
	dma_addr_t ixnoirwTargetAddress[IS_MAX_PLANES];		/* DNS REP NOISE WDMA */
	dma_addr_t ixbnrTargetAddress[IS_MAX_PLANES];		/* DNS BNR WDMA */
	dma_addr_t ixvTargetAddress[IS_MAX_PLANES];		/* TNR PREV WDMA */
	dma_addr_t ixwTargetAddress[IS_MAX_PLANES];		/* TNR PREV weight WDMA */
	dma_addr_t ixtTargetAddress[IS_MAX_PLANES];		/* TNR PREV RDMA */
	dma_addr_t ixgTargetAddress[IS_MAX_PLANES];		/* TNR PREV weight RDMA */
	dma_addr_t ixmTargetAddress[IS_MAX_PLANES];		/* TNR MOTION RDMA */
	u64 ixmKTargetAddress[IS_MAX_PLANES];			/* TNR MOTION KVA */
	u64 mexcTargetAddress[IS_MAX_PLANES];			/* ME or MCH out DMA */
	u64 orbxcKTargetAddress[IS_MAX_PLANES];			/* ME or MCH out DMA */
	dma_addr_t ypnrdsTargetAddress[IS_MAX_PLANES];		/* YPP Y/UV L2 RDMA */
	dma_addr_t ypnoiTargetAddress[IS_MAX_PLANES];		/* YPP NOISE RDMA */
	dma_addr_t ypdgaTargetAddress[IS_MAX_PLANES];		/* YPP DRC GAIN RDMA */
	dma_addr_t ypsvhistTargetAddress[IS_MAX_PLANES];	/* YPP SV HIST RDMA */
	dma_addr_t sc0TargetAddress[IS_MAX_PLANES];
	dma_addr_t sc1TargetAddress[IS_MAX_PLANES];
	dma_addr_t sc2TargetAddress[IS_MAX_PLANES];
	dma_addr_t sc3TargetAddress[IS_MAX_PLANES];
	dma_addr_t sc4TargetAddress[IS_MAX_PLANES];
	dma_addr_t sc5TargetAddress[IS_MAX_PLANES];
	dma_addr_t clxsTargetAddress[IS_MAX_PLANES];		/* CLAHE IN DMA */
	dma_addr_t clxcTargetAddress[IS_MAX_PLANES];		/* CLAHE OUT DMA */

	/* Logical node information */
	struct is_sub_node	out_node;
	struct is_sub_node	cap_node;

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
