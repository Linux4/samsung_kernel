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
#ifndef FIMC_IS_FRAME_MGR_H
#define FIMC_IS_FRAME_MGR_H

#include <linux/kthread.h>
#include "fimc-is-time.h"

#define FIMC_IS_MAX_BUFS	VIDEO_MAX_FRAME
#define FIMC_IS_MAX_PLANES	VIDEO_MAX_PLANES

#define FRAMEMGR_ID_INVALID	0x000000
#define FRAMEMGR_ID_SSX 	0x000100
#define FRAMEMGR_ID_3XS 	0x000200
#define FRAMEMGR_ID_3XC 	0x000400
#define FRAMEMGR_ID_3XP 	0x000800
#define FRAMEMGR_ID_IXS 	0x001000
#define FRAMEMGR_ID_IXC 	0x002000
#define FRAMEMGR_ID_IXP 	0x004000
#define FRAMEMGR_ID_DIS 	0x008000
#define FRAMEMGR_ID_SCC 	0x010000
#define FRAMEMGR_ID_SCP 	0x020000
#define FRAMEMGR_ID_HW		0x100000
#define FRAMEMGR_ID_SHOT	(FRAMEMGR_ID_SSX | FRAMEMGR_ID_3XS | \
				 FRAMEMGR_ID_IXS | FRAMEMGR_ID_DIS)
#define FRAMEMGR_ID_STREAM	(FRAMEMGR_ID_3XC | FRAMEMGR_ID_3XP | \
				 FRAMEMGR_ID_SCC | FRAMEMGR_ID_DIS | \
				 FRAMEMGR_ID_SCP)
/* #define TRACE_FRAME */
#define TRACE_ID		(FRAMEMGR_ID_SHOT | FRAMEMGR_ID_STREAM | FRAMEMGR_ID_HW)

#define FRAMEMGR_MAX_REQUEST	VIDEO_MAX_FRAME

#define FMGR_IDX_0		(1 << 0 )
#define FMGR_IDX_1		(1 << 1 )
#define FMGR_IDX_2		(1 << 2 )
#define FMGR_IDX_3		(1 << 3 )
#define FMGR_IDX_4		(1 << 4 )
#define FMGR_IDX_5		(1 << 5 )
#define FMGR_IDX_6		(1 << 6 )
#define FMGR_IDX_7		(1 << 7 )
#define FMGR_IDX_8		(1 << 8 )
#define FMGR_IDX_9		(1 << 9 )
#define FMGR_IDX_10		(1 << 10)
#define FMGR_IDX_11		(1 << 11)
#define FMGR_IDX_12		(1 << 12)
#define FMGR_IDX_13		(1 << 13)
#define FMGR_IDX_14		(1 << 14)
#define FMGR_IDX_15		(1 << 15)
#define FMGR_IDX_16		(1 << 16)
#define FMGR_IDX_17		(1 << 17)
#define FMGR_IDX_18		(1 << 18)
#define FMGR_IDX_19		(1 << 19)
#define FMGR_IDX_20		(1 << 20)
#define FMGR_IDX_21		(1 << 21)
#define FMGR_IDX_22		(1 << 22)
#define FMGR_IDX_23		(1 << 23)
#define FMGR_IDX_24		(1 << 24)
#define FMGR_IDX_25		(1 << 25)
#define FMGR_IDX_26		(1 << 26)
#define FMGR_IDX_27		(1 << 27)
#define FMGR_IDX_28		(1 << 28)
#define FMGR_IDX_29		(1 << 29)
#define FMGR_IDX_30		(1 << 30)
#define FMGR_IDX_31		(1 << 31)

#define framemgr_e_barrier_irqs(this, index, flag) \
	this->sindex |= index; spin_lock_irqsave(&this->slock, flag)
#define framemgr_x_barrier_irqr(this, index, flag) \
	spin_unlock_irqrestore(&this->slock, flag); this->sindex &= ~index
#define framemgr_e_barrier_irq(this, index) \
	this->sindex |= index; spin_lock_irq(&this->slock)
#define framemgr_x_barrier_irq(this, index) \
	spin_unlock_irq(&this->slock); this->sindex &= ~index
#define framemgr_e_barrier(this, index) \
	this->sindex |= index; spin_lock(&this->slock)
#define framemgr_x_barrier(this, index) \
	spin_unlock(&this->slock); this->sindex &= ~index

enum fimc_is_frame_state {
	FS_FREE,
	FS_REQUEST,
	FS_PROCESS,
	FS_COMPLETE,
	FS_INVALID
};
#define NR_FRAME_STATE FS_INVALID

enum fimc_is_frame_request {
	/* SCC, SCP frame done,
	   ISP meta done */
	REQ_FRAME,
	/* 3AA shot done */
	REQ_3AA_SHOT,
	/* ISP shot done */
	REQ_ISP_SHOT,
	/* DIS shot done */
	REQ_DIS_SHOT
};

enum fimc_is_frame_mem_state {
	/* initialized memory */
	FRAME_MEM_INIT,
	/* mapped memory */
	FRAME_MEM_MAPPED
};

#ifndef ENABLE_IS_CORE
#define MAX_FRAME_INFO		(4)
enum fimc_is_frame_info_index {
	INFO_FRAME_START,
	INFO_CONFIG_LOCK,
	INFO_FRAME_END_PROC
};

struct fimc_is_frame_info {
	int			cpu;
	int			pid;
	unsigned long long	when;
};
#endif

struct fimc_is_frame {
	struct list_head	list;
	struct kthread_work	work;
	void			*groupmgr;
	void			*group;

	/* group leader use */
	struct camera2_shot	*shot;
	struct camera2_shot_ext	*shot_ext;
	ulong			kvaddr_shot;
	u32			dvaddr_shot;
	ulong			cookie_shot;
	size_t			shot_size;

	/* stream use */
	struct camera2_stream	*stream;
	u32			stream_size;

	/* common use */
	u32			planes;
	u32			kvaddr_buffer[FIMC_IS_MAX_PLANES];
	u32			dvaddr_buffer[FIMC_IS_MAX_PLANES];

	/* internal use */
	unsigned long		mem_state;
	u32			state;
	u32			fcount;
	u32			rcount;
	u32			index;
	u32			result;
	unsigned long		req_flag;
	unsigned long		out_flag;
	struct vb2_buffer	*vb;

	/* for overwriting framecount check */
	bool			has_fcount;

#ifndef ENABLE_IS_CORE
	struct fimc_is_frame_info frame_info[MAX_FRAME_INFO];
	u32			instance; /* device instance */
	u32			type;
	unsigned long		ndone_flag;

	/*
	 * core_flag is SCC & FD frame end flag.
	 * This flag is set and clear when shot or shot done
	 */
	unsigned long		core_flag;
	atomic_t		shot_done_flag;
#endif

	/* time measure externally */
	struct timeval	*tzone;
	/* time measure internally */
	struct fimc_is_monitor	mpoint[TMM_END];
};

struct fimc_is_framemgr {
	u32			id;
	spinlock_t		slock;
	ulong			sindex;

	u32			num_frames;
	struct fimc_is_frame	*frames;

	u32			queued_count[NR_FRAME_STATE];
	struct list_head	queued_list[NR_FRAME_STATE];
};

int put_frame(struct fimc_is_framemgr *this, struct fimc_is_frame *frame,
			enum fimc_is_frame_state state);
struct fimc_is_frame *get_frame(struct fimc_is_framemgr *this,
			enum fimc_is_frame_state state);
int trans_frame(struct fimc_is_framemgr *this, struct fimc_is_frame *frame,
			enum fimc_is_frame_state state);
struct fimc_is_frame *peek_frame(struct fimc_is_framemgr *this,
			enum fimc_is_frame_state state);
struct fimc_is_frame *find_frame(struct fimc_is_framemgr *this,
			enum fimc_is_frame_state state,
			int (*fn)(struct fimc_is_frame *, void *), void *data);
int swap_head_frame(struct fimc_is_framemgr *this,
			enum fimc_is_frame_state state);
void print_frame_queue(struct fimc_is_framemgr *this,
			enum fimc_is_frame_state state);
void print_frame_info_queue(struct fimc_is_framemgr *this,
			enum fimc_is_frame_state state);

int frame_manager_probe(struct fimc_is_framemgr *this, u32 id);
int frame_manager_open(struct fimc_is_framemgr *this, u32 buffers);
int frame_manager_close(struct fimc_is_framemgr *this);
int frame_manager_flush(struct fimc_is_framemgr *this);
void frame_manager_print_queues(struct fimc_is_framemgr *this);
void frame_manager_print_info_queues(struct fimc_is_framemgr *this);

#endif
