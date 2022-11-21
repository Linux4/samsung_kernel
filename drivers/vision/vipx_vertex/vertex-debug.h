/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VERTEX_DEBUG_H__
#define __VERTEX_DEBUG_H__

#include <linux/timer.h>

#include "vertex-config.h"
#include "vertex-system.h"

#ifdef DEBUG_LOG_CONCATE_ENABLE
#define DLOG_INIT()		struct vertex_debug_log dlog = \
					{ .dsentence_pos = 0 }
#define DLOG(fmt, ...)		\
	vertex_debug_log_concate(&dlog, fmt, ##__VA_ARGS__)
#define DLOG_OUT()		vertex_debug_log_print(&dlog)
#else
#define DLOG_INIT()
#define DLOG(fmt, ...)
#define DLOG_OUT()		"FORBIDDEN HISTORY"
#endif
#define DEBUG_SENTENCE_MAX		(300)

struct vertex_device;

struct vertex_debug_imgdump {
	struct dentry			*file;
	unsigned int			target_graph;
	unsigned int			target_chain;
	unsigned int			target_pu;
	unsigned int			target_index;
	void				*kvaddr;
	void				*cookie;
	size_t				length;
	size_t				offset;
};

struct vertex_debug_monitor {
	unsigned int			time_cnt;
	unsigned int			tick_cnt;
	unsigned int			sched_cnt;
	unsigned int			done_cnt;
	struct timer_list		timer;
};

enum vertex_debug_state {
	VERTEX_DEBUG_STATE_START,
};

struct vertex_debug {
	unsigned long			state;
	struct vertex_system		*system;

	struct dentry			*root;
	struct dentry			*logfile;
	struct dentry			*grpfile;
	struct dentry			*buffile;

	struct vertex_debug_imgdump	imgdump;
	struct vertex_debug_monitor	monitor;
};

struct vertex_debug_log {
	size_t				dsentence_pos;
	char				dsentence[DEBUG_SENTENCE_MAX];
};

int vertex_debug_write_log_binary(void);
int vertex_debug_dump_debug_regs(void);

int vertex_debug_atoi(const char *psz_buf);
int vertex_debug_bitmap_scnprintf(char *buf, unsigned int buflen,
		const unsigned long *maskp, int nmaskbits);

void vertex_debug_log_concate(struct vertex_debug_log *log,
		const char *fmt, ...);
char *vertex_debug_log_print(struct vertex_debug_log *log);

void vertex_debug_memdump(const char *prefix, void *start, size_t size);

int vertex_debug_start(struct vertex_debug *debug);
int vertex_debug_stop(struct vertex_debug *debug);
int vertex_debug_open(struct vertex_debug *debug);
int vertex_debug_close(struct vertex_debug *debug);

int vertex_debug_probe(struct vertex_device *device);
void vertex_debug_remove(struct vertex_debug *debug);

#endif
