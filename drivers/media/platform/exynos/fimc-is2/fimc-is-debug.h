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

#ifndef FIMC_IS_DEBUG_H
#define FIMC_IS_DEBUG_H

#include "fimc-is-video.h"

#define DEBUG_SENTENCE_MAX		300

#ifdef DBG_DRAW_DIGIT
#define DBG_DIGIT_CNT		20	/* Max count of total digit */
#define DBG_DIGIT_W		16
#define DBG_DIGIT_H		32
#define DBG_DIGIT_MARGIN_W_INIT	128
#define DBG_DIGIT_MARGIN_H_INIT	64
#define DBG_DIGIT_MARGIN_W	8
#define DBG_DIGIT_MARGIN_H	2
#define DBG_DIGIT_TAG(row, col, queue, frame, digit)	\
	do {		\
		ulong addr;	\
		u32 width, height, pixelformat, bitwidth;		\
		addr = queue->buf_kva[frame->index][0];			\
		width = (frame->width) ? frame->width : queue->framecfg.width;	\
		height = (frame->height) ? frame->height : queue->framecfg.height;	\
		pixelformat = queue->framecfg.format->pixelformat;	\
		bitwidth = queue->framecfg.format->bitwidth;		\
		fimc_is_draw_digit(addr, width, height, pixelformat, bitwidth,	\
				row, col, digit);			\
	} while(0)
#else
#define DBG_DIGIT_TAG(row, col, queue, frame, digit)
#endif

#define MAX_EVENT_LOG_NUM 32
#define FIMC_IS_EVENT_MAX_NUM SZ_4K
#define FIMC_IS_EVENT_PRINT_MAX 512

enum fimc_is_debug_state {
	FIMC_IS_DEBUG_OPEN
};

enum dbg_dma_dump_type {
	DBG_DMA_DUMP_IMAGE,
	DBG_DMA_DUMP_META,
};

typedef enum fimc_is_event_type {
	/* INTERUPPT  */
	FIMC_IS_EVENT_INT_START		= 0x0,
	FIMC_IS_EVENT_INT_END,

	/* SHOT */
	FIMC_IS_EVENT_SHOT_START	= 0x10,
	FIMC_IS_EVENT_SHOT_DONE,

	/* DDK MEM ALLOC */
	FIMC_IS_EVENT_LIB_MEM_ALLOC	= 0x300,
	FIMC_IS_EVENT_LIB_MEM_FREE,

	FIMC_IS_EVENT_SIZE		= 0x1000,

	FIMC_IS_EVENT_MAX,
} fimc_is_event_type_t;

struct fimc_is_debug_event_size {
	unsigned int id;
	unsigned int total_width;
	unsigned int total_height;
	unsigned int width;
	unsigned int height;
	unsigned int position_x;
	unsigned int position_y;
};

struct fimc_is_debug_event_msg {
	char text[MAX_EVENT_LOG_NUM];
};

struct fimc_is_debug_event_lib_memory {
	char		comm[TASK_COMM_LEN];
	u32		size;
	ulong		kvaddr;
	dma_addr_t	dvaddr;
};

struct fimc_is_debug_event {
	unsigned int num;
	ktime_t time;
	fimc_is_event_type_t type;
	union {
		struct fimc_is_debug_event_size size;
		struct fimc_is_debug_event_msg msg;
		struct fimc_is_debug_event_lib_memory lib_mem;
	} event_data;
};

struct fimc_is_debug {
	struct dentry		*root;
	struct dentry		*logfile;
	struct dentry		*imgfile;
	struct dentry		*event;

	/* log dump */
	size_t			read_vptr;
	struct fimc_is_minfo	*minfo;

	/* debug message */
	size_t			dsentence_pos;
	char			dsentence[DEBUG_SENTENCE_MAX];

	unsigned long		state;

	/* event message */
	struct fimc_is_debug_event event_log[FIMC_IS_EVENT_MAX_NUM];
	atomic_t event_index;

	/* test */
	char build_date[MAX_EVENT_LOG_NUM];
	unsigned int iknownothing;
};

extern struct fimc_is_debug fimc_is_debug;

int fimc_is_debug_probe(void);
int fimc_is_debug_open(struct fimc_is_minfo *minfo);
int fimc_is_debug_close(void);

void fimc_is_dmsg_init(void);
void fimc_is_dmsg_concate(const char *fmt, ...);
char *fimc_is_dmsg_print(void);
void fimc_is_print_buffer(char *buffer, size_t len);
int fimc_is_debug_dma_dump(struct fimc_is_queue *queue, u32 index, u32 vid, u32 type);
#ifdef DBG_DRAW_DIGIT
void fimc_is_draw_digit(ulong addr, int width, int height, u32 pixelformat,
		u32 bitwidth, int row_index, int col_index, u64 digit);
#endif

int imgdump_request(ulong cookie, ulong kvaddr, size_t size);

#ifdef ENABLE_DBG_EVENT
void fimc_is_debug_event_add(struct fimc_is_debug *info,
		struct fimc_is_debug_event *event);
int fimc_is_debug_info_dump(struct seq_file *s, struct fimc_is_debug *info);
void fimc_is_event_test(struct fimc_is_debug *info);
void fimc_is_event_add_lib_mem(	bool type, char* comm, u32 size, ulong kvaddr,
		dma_addr_t dvaddr);
#endif
#endif
