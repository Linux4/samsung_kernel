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

#ifndef IS_DEBUG_H
#define IS_DEBUG_H

#include "is-video.h"

#define DEBUG_SENTENCE_MAX		300
#define LOG_INTERVAL_OF_WARN		30

#ifdef DBG_DRAW_DIGIT
#define DBG_DIGIT_CNT		20	/* Max count of total digit */
#define DBG_DIGIT_W		16
#define DBG_DIGIT_H		32
#define DBG_DIGIT_MARGIN_W_INIT	128
#define DBG_DIGIT_MARGIN_H_INIT	64
#define DBG_DIGIT_MARGIN_W	8
#define DBG_DIGIT_MARGIN_H	2
#define DBG_DIGIT_TAG(row, col, queue, frame, digit, increase_unit)	\
	do {			\
		int i, j;   \
		for (i = 0, j = digit; i < frame->num_buffers; i++, j += increase_unit) {   \
			ulong addr;	\
			u32 width, height, pixelformat, bitwidth;		\
			addr = queue->buf_kva[frame->index][i];			\
			width = (frame->width) ? frame->width : queue->framecfg.width;	\
			height = (frame->height) ? frame->height : queue->framecfg.height;	\
			pixelformat = queue->framecfg.format->pixelformat;	\
			bitwidth = queue->framecfg.format->hw_bitwidth;		\
			is_draw_digit(addr, width, height, pixelformat, bitwidth,	\
					row, col, j);			\
		}   \
	} while(0)
#else
#define DBG_DIGIT_TAG(row, col, queue, frame, digit, increase_unit)
#endif

enum is_debug_state {
	IS_DEBUG_OPEN
};

enum dbg_dma_dump_type {
	DBG_DMA_DUMP_IMAGE,
	DBG_DMA_DUMP_META,
};

struct is_debug {
	struct dentry		*root;
	struct dentry		*imgfile;
	struct dentry		*event_dir;
	struct dentry		*logfile;

	/* log dump */
	size_t			read_vptr;
	struct is_minfo	*minfo;

	/* debug message */
	size_t			dsentence_pos;
	char			dsentence[DEBUG_SENTENCE_MAX];

	unsigned long		state;

};

extern struct is_debug is_debug;

int is_debug_probe(void);
int is_debug_open(struct is_minfo *minfo);
int is_debug_close(void);

void is_dmsg_init(void);
void is_dmsg_concate(const char *fmt, ...);
char *is_dmsg_print(void);
void is_print_buffer(char *buffer, size_t len);
int is_debug_dma_dump(struct is_queue *queue, u32 index, u32 vid, u32 type);
int is_debug_dma_dump_by_frame(struct is_frame *queue, u32 vid, u32 type);
#ifdef DBG_DRAW_DIGIT
void is_draw_digit(ulong addr, int width, int height, u32 pixelformat,
		u32 bitwidth, int row_index, int col_index, u64 digit);
#endif

int imgdump_request(ulong cookie, ulong kvaddr, size_t size);

#define IS_EVENT_MAX_NUM	SZ_4K
#define EVENT_STR_MAX		SZ_128

typedef enum is_event_store_type {
	/* normal log - full then replace first normal log buffer */
	IS_EVENT_NORMAL = 0x1,
	/* critical log - full then stop to add log to critical log buffer*/
	IS_EVENT_CRITICAL = 0x2,
	IS_EVENT_OVERFLOW_CSI = 0x3,
	IS_EVENT_OVERFLOW_3AA = 0x4,
	IS_EVENT_ALL,
} is_event_store_type_t;

struct is_debug_event_log {
	unsigned int log_num;
	ktime_t time;
	is_event_store_type_t event_store_type;
	char dbg_msg[EVENT_STR_MAX];
	void (*callfunc)(void *);

	/* This parameter should be used in non-atomic context */
	void *ptrdata;
	int cpu;
};

struct is_debug_event {
	struct dentry			*log;

	struct dentry			*logfilter;
	u32				log_filter;

	struct dentry			*logenable;
	u32				log_enable;

#ifdef ENABLE_DBG_EVENT_PRINT
	atomic_t			event_index;
	atomic_t			critical_log_tail;
	atomic_t			normal_log_tail;
	struct is_debug_event_log	event_log_critical[IS_EVENT_MAX_NUM];
	struct is_debug_event_log	event_log_normal[IS_EVENT_MAX_NUM];
#endif

	atomic_t			overflow_csi;
	atomic_t			overflow_3aa;
};

#ifdef ENABLE_DBG_EVENT_PRINT
void is_debug_event_print(u32 event_type, void (*callfunc)(void *), void *ptrdata, size_t datasize, const char *fmt, ...);
#else
#define is_debug_event_print(...)	do { } while(0)
#endif
void is_debug_event_count(u32 event_type);
void is_dbg_print(char *fmt, ...);
int is_debug_info_dump(struct seq_file *s, struct is_debug_event *debug_event);

extern int debug_s2d;
void is_debug_s2d(bool en_s2d, const char *fmt, ...);
#endif
