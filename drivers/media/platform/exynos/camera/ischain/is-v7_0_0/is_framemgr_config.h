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

#define IS_MAX_BUFS	VIDEO_MAX_FRAME
#define IS_MAX_PLANES	17

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

struct is_frame {
	struct list_head	list;
	struct kthread_work	work;
	struct kthread_delayed_work dwork;
	void			*groupmgr;
	void			*group;
	void			*subdev; /* is_subdev */

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

	/*
	 * target address for capture node
	 * [0] invalid address, stop
	 * [others] valid address
	 */
	u32 txcTargetAddress[IS_MAX_PLANES]; /* 3AA capture DMA */
	u32 txpTargetAddress[IS_MAX_PLANES]; /* 3AA preview DMA */
	u32 mrgTargetAddress[IS_MAX_PLANES];
	u32 efdTargetAddress[IS_MAX_PLANES];
	u32 ixcTargetAddress[IS_MAX_PLANES];
	u32 ixpTargetAddress[IS_MAX_PLANES];
	u64 mexcTargetAddress[IS_MAX_PLANES]; /* ME out DMA */
	u32 sc0TargetAddress[IS_MAX_PLANES];
	u32 sc1TargetAddress[IS_MAX_PLANES];
	u32 sc2TargetAddress[IS_MAX_PLANES];
	u32 sc3TargetAddress[IS_MAX_PLANES];
	u32 sc4TargetAddress[IS_MAX_PLANES];
	u32 sc5TargetAddress[IS_MAX_PLANES];

	/* multi-buffer use */
	u32			num_buffers; /* total number of buffers per frame */
	u32			cur_buf_index; /* current processed buffer index */

	/* internal use */
	unsigned long		mem_state;
	u32			state;
	u32			fcount;
	u32			rcount;
	u32			index;
	u32			lindex;
	u32			hindex;
	u32			result;
	unsigned long		out_flag;
	unsigned long		bak_flag;

	struct is_frame_info frame_info[MAX_FRAME_INFO];
	u32			instance; /* device instance */
	u32			type;
	unsigned long		core_flag;
	atomic_t		shot_done_flag;

#ifdef ENABLE_SYNC_REPROCESSING
	struct list_head	sync_list;
#endif
	struct list_head	votf_list;

	/* for NI(noise index from DDK) use */
	u32			noise_idx; /* Noise index */

#ifdef MEASURE_TIME
	/* time measure externally */
	struct timeval	*tzone;
	/* time measure internally */
	struct is_monitor	mpoint[TMS_END];
#endif

#ifdef DBG_DRAW_DIGIT
	u32			width;
	u32			height;
#endif
};

