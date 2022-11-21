/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VERTEX_CONTEXT_H__
#define __VERTEX_CONTEXT_H__

#include <linux/mutex.h>

#include "vertex-queue.h"
#include "vertex-graph.h"
#include "vertex-ioctl.h"
#include "vertex-core.h"

enum vertex_context_state {
	VERTEX_CONTEXT_OPEN,
	VERTEX_CONTEXT_GRAPH,
	VERTEX_CONTEXT_FORMAT,
	VERTEX_CONTEXT_START,
	VERTEX_CONTEXT_STOP
};

struct vertex_context;

struct vertex_context_qops {
	int (*poll)(struct vertex_queue_list *qlist,
			struct file *file, struct poll_table_struct *poll);
	int (*set_graph)(struct vertex_queue_list *qlist,
			struct vs4l_graph *ginfo);
	int (*set_format)(struct vertex_queue_list *qlist,
			struct vs4l_format_list *flist);
	int (*set_param)(struct vertex_queue_list *qlist,
			struct vs4l_param_list *plist);
	int (*set_ctrl)(struct vertex_queue_list *qlist,
			struct vs4l_ctrl *ctrl);
	int (*qbuf)(struct vertex_queue_list *qlist,
			struct vs4l_container_list *clist);
	int (*dqbuf)(struct vertex_queue_list *qlist,
			struct vs4l_container_list *clist);
	int (*streamon)(struct vertex_queue_list *qlist);
	int (*streamoff)(struct vertex_queue_list *qlist);
};

struct vertex_context {
	unsigned int			state;
	unsigned int			idx;
	struct list_head		list;
	struct mutex			lock;

	const struct vertex_context_gops	*graph_ops;

	const struct vertex_context_qops	*queue_ops;
	struct vertex_queue_list		queue_list;

	struct vertex_core		*core;
	struct vertex_graph		*graph;
};

struct vertex_context *vertex_context_create(struct vertex_core *core);
void vertex_context_destroy(struct vertex_context *vctx);

#endif
