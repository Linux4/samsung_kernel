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
	/* x offset position for sub-frame */
	u32	offset_x;
	/* crop x position for sub-frame */
	u32	crop_x;
	/* stripe width for sub-frame */
	u32	crop_width;
	/* left margin for sub-frame */
	u32	left_margin;
	/* right margin for sub-frame */
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
	/* Horizontal pixel count which stripe processing is done for. */
	u32				h_pix_num[MAX_STRIPE_REGION_NUM];
	/* stripe start position for sub-frame */
	u32				start_pos_x;
	/* stripe width for sub-frame */
	u32				stripe_width;
	/* sub-frame size for incrop/otcrop */
	struct is_stripe_size		in;
	struct is_stripe_size		out;
#ifdef USE_KERNEL_VFS_READ_WRITE
	/* For image dump */
	ulong                           kva[MAX_STRIPE_REGION_NUM][IS_MAX_PLANES];
	size_t                          size[MAX_STRIPE_REGION_NUM][IS_MAX_PLANES];
#endif
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

	struct is_vb2_buf 	*vbuf;

	/* dva for ICPU */
	dma_addr_t		shot_dva;

	/* stream use */
	struct camera2_stream	*stream;

	/* common use */
	u32			planes; /* total planes include multi-buffers */
	struct is_priv_buf	*pb_output;
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

	u32 cstat_ctx;
	dma_addr_t txpTargetAddress[CSTAT_CTX_NUM][IS_MAX_PLANES];	/* CSTAT CDS WDMA */
	dma_addr_t efdTargetAddress[CSTAT_CTX_NUM][IS_MAX_PLANES];	/* CSTAT FDPIG WDMA */
	dma_addr_t txdgrTargetAddress[CSTAT_CTX_NUM][IS_MAX_PLANES];	/* CSTAT DRC GRID WDMA */
	dma_addr_t txldsTargetAddress[CSTAT_CTX_NUM][IS_MAX_PLANES];	/* CSTAT LMEDS0 WDMA */
	dma_addr_t dva_cstat_lmeds1[CSTAT_CTX_NUM][IS_MAX_PLANES];	/* CSTAT LMEDS1 WDMA */

	dma_addr_t lmesTargetAddress[IS_MAX_PLANES];		/* LME prev IN DMA */
	dma_addr_t lmecTargetAddress[IS_MAX_PLANES];		/* LME MV OUT DMA */
	u64 lmecKTargetAddress[IS_MAX_PLANES];			/* LME MV OUT KVA */
	dma_addr_t lmesadTargetAddress[IS_MAX_PLANES];		/* LME SAD OUT DMA */
	u64 lmemKTargetAddress[IS_MAX_PLANES];			/* LME MV SW OUT KVA */
	dma_addr_t ixscmapTargetAddress[IS_MAX_PLANES];		/* DNS SC MAP RDMA */
	dma_addr_t ypnrdsTargetAddress[IS_MAX_PLANES];		/* YPP Y/UV L2 RDMA */
	dma_addr_t ypdgaTargetAddress[IS_MAX_PLANES];		/* YPP DRC GAIN RDMA */
	dma_addr_t ypclaheTargetAddress[IS_MAX_PLANES];		/* YPP CLAHE RDMA */
	dma_addr_t ypsvhistTargetAddress[IS_MAX_PLANES];	/* YPP SV HIST WDMA */
	dma_addr_t sc0TargetAddress[IS_MAX_PLANES];
	dma_addr_t sc1TargetAddress[IS_MAX_PLANES];
	dma_addr_t sc2TargetAddress[IS_MAX_PLANES];
	dma_addr_t sc3TargetAddress[IS_MAX_PLANES];
	dma_addr_t sc4TargetAddress[IS_MAX_PLANES];
	dma_addr_t sc5TargetAddress[IS_MAX_PLANES];
	dma_addr_t dva_mcfp_prev_yuv[IS_MAX_PLANES];		/* MCFP PREV YUV RDMA */
	dma_addr_t dva_mcfp_prev_wgt[IS_MAX_PLANES];		/* MCFP PREV WGT RDMA */
	dma_addr_t dva_mcfp_cur_wgt[IS_MAX_PLANES];		/* MCFP CUR WGT RDMA */
	dma_addr_t dva_mcfp_cur_drc[IS_MAX_PLANES];		/* MCFP CUR DRC RDMA */
	dma_addr_t dva_mcfp_prev_drc[IS_MAX_PLANES];		/* MCFP PREV DRC RDMA */
	dma_addr_t dva_mcfp_motion[IS_MAX_PLANES];		/* MCFP MOTION RDMA */
	u64 kva_mcfp_motion[IS_MAX_PLANES];			/* MCFP MOTION RDMA */
	dma_addr_t dva_mcfp_mv_mixer[IS_MAX_PLANES];		/* MCFP MVMIXER RDMA */
	dma_addr_t dva_mcfp_sat_flag[IS_MAX_PLANES];		/* MCFP SAT FLAG RDMA */
	dma_addr_t dva_mcfp_wgt[IS_MAX_PLANES];			/* MCFP WGT WDMA */
	dma_addr_t dva_mcfp_yuv[IS_MAX_PLANES];			/* MCFP YUV WDMA */
	dma_addr_t dva_rgbp_hf[IS_MAX_PLANES];			/* RGBP HF WDMA*/
	dma_addr_t dva_rgbp_sf[IS_MAX_PLANES];			/* RGBP SF WDMA */
	dma_addr_t dva_rgbp_yuv[IS_MAX_PLANES];			/* RGBP YUV WDMA*/
	dma_addr_t dva_rgbp_rgb[IS_MAX_PLANES];			/* RGBP RGB WDMA */
	dma_addr_t dva_byrp_hdr[IS_MAX_PLANES];			/* BYRP HDR RDMA */
	dma_addr_t dva_byrp_byr[IS_MAX_PLANES];			/* BYRP WDMA Before WBG (DNG) */
	dma_addr_t dva_byrp_byr_processed[IS_MAX_PLANES];	/* BYRP WDMA After WBG (Super night)*/

	u64 kva_byrp_rta_info[IS_MAX_PLANES];
	u64 kva_rgbp_rta_info[IS_MAX_PLANES];
	u64 kva_mcfp_rta_info[IS_MAX_PLANES];
	u64 kva_yuvp_rta_info[IS_MAX_PLANES];
	u64 kva_lme_rta_info[IS_MAX_PLANES];

	/* Logical node information */
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
