/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VERTEX_GRAPH_H__
#define __VERTEX_GRAPH_H__

#include <linux/mutex.h>
#include <linux/wait.h>

#include "vertex-config.h"
#include "vs4l.h"
#include "vertex-taskmgr.h"

struct vips_context;
struct vertex_graph;

enum vertex_graph_state {
	VERTEX_GRAPH_STATE_CONFIG,
	VERTEX_GRAPH_STATE_HENROLL,
	VERTEX_GRAPH_STATE_HMAPPED,
	VERTEX_GRAPH_STATE_MMAPPED,
	VERTEX_GRAPH_STATE_START,
};

enum vertex_graph_flag {
	VERTEX_GRAPH_FLAG_UPDATE_PARAM = VS4L_GRAPH_FLAG_END
};

struct vertex_format {
	unsigned int			format;
	unsigned int			plane;
	unsigned int			width;
	unsigned int			height;
};

struct vertex_format_list {
	unsigned int			count;
	struct vertex_format		*formats;
};

struct vertex_graph_ops {
	int (*control)(struct vertex_graph *graph, struct vertex_task *task);
	int (*request)(struct vertex_graph *graph, struct vertex_task *task);
	int (*process)(struct vertex_graph *graph, struct vertex_task *task);
	int (*cancel)(struct vertex_graph *graph, struct vertex_task *task);
	int (*done)(struct vertex_graph *graph, struct vertex_task *task);
	int (*update_param)(struct vertex_graph *graph,
			struct vertex_task *task);
};

struct vertex_graph {
	unsigned int			idx;
	unsigned long			state;
	struct mutex			*global_lock;

	void				*owner;
	const struct vertex_graph_ops	*gops;
	struct mutex			local_lock;
	struct vertex_task		control;
	wait_queue_head_t		control_wq;
	struct vertex_taskmgr		taskmgr;

	unsigned int			uid;
	unsigned long			flags;
	unsigned int			priority;

	struct vertex_format_list		inflist;
	struct vertex_format_list		otflist;

	unsigned int			inhash[VERTEX_MAX_TASK];
	unsigned int			othash[VERTEX_MAX_TASK];

	struct list_head		gmodel_list;
	unsigned int			gmodel_count;

	/* for debugging */
	unsigned int			input_cnt;
	unsigned int			cancel_cnt;
	unsigned int			done_cnt;
	unsigned int			recent;

	struct vertex_context		*vctx;
};

extern const struct vertex_queue_gops vertex_queue_gops;
extern const struct vertex_vctx_gops vertex_vctx_gops;

void vertex_graph_print(struct vertex_graph *graph);

struct vertex_graph *vertex_graph_create(struct vertex_context *vctx,
		void *graphmgr);
int vertex_graph_destroy(struct vertex_graph *graph);

#endif
