/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VERTEX_QUEUE_H__
#define __VERTEX_QUEUE_H__

#include <linux/poll.h>
#include <linux/types.h>
#include <linux/mutex.h>

#include "vertex-config.h"
#include "vs4l.h"
#include "vertex-memory.h"
#include "vertex-graph.h"

struct vertex_context;
struct vertex_queue_list;

struct vertex_format_type {
	char				*name;
	unsigned int			colorspace;
	unsigned int			planes;
	unsigned int			bitsperpixel[VERTEX_MAX_PLANES];
};

struct vertex_buffer_format {
	unsigned int			target;
	struct vertex_format_type		*fmt;
	unsigned int			colorspace;
	unsigned int			plane;
	unsigned int			width;
	unsigned int			height;
	unsigned int			size[VERTEX_MAX_PLANES];
};

struct vertex_buffer_format_list {
	unsigned int			count;
	struct vertex_buffer_format	*formats;
};

struct vertex_container {
	unsigned int			type;
	unsigned int			target;
	unsigned int			memory;
	unsigned int			reserved[4];
	unsigned int			count;
	struct vertex_buffer		*buffers;
	struct vertex_buffer_format	*format;
};

struct vertex_container_list {
	unsigned int			direction;
	unsigned int			id;
	unsigned int			index;
	unsigned long			flags;
	struct timeval			timestamp[6];
	unsigned int			count;
	unsigned int			user_params[MAX_NUM_OF_USER_PARAMS];
	struct vertex_container		*containers;
};

enum vertex_bundle_state {
	VERTEX_BUNDLE_STATE_DEQUEUED,
	VERTEX_BUNDLE_STATE_QUEUED,
	VERTEX_BUNDLE_STATE_PROCESS,
	VERTEX_BUNDLE_STATE_DONE,
};

struct vertex_bundle {
	unsigned long			flags;
	unsigned int			state;
	struct list_head		queued_entry;
	struct list_head		process_entry;
	struct list_head		done_entry;

	struct vertex_container_list	clist;
};

struct vertex_queue {
	unsigned int			direction;
	unsigned int			streaming;
	struct mutex			*lock;

	struct list_head		queued_list;
	atomic_t			queued_count;
	struct list_head		process_list;
	atomic_t			process_count;
	struct list_head		done_list;
	atomic_t			done_count;

	spinlock_t			done_lock;
	wait_queue_head_t		done_wq;

	struct vertex_buffer_format_list	flist;
	struct vertex_bundle		*bufs[VERTEX_MAX_BUFFER];
	unsigned int			num_buffers;

	struct vertex_queue_list		*qlist;
};

struct vertex_queue_gops {
	int (*set_graph)(struct vertex_graph *graph, struct vs4l_graph *ginfo);
	int (*set_format)(struct vertex_graph *graph,
			struct vs4l_format_list *flist);
	int (*set_param)(struct vertex_graph *graph,
			struct vs4l_param_list *plist);
	int (*set_ctrl)(struct vertex_graph *graph, struct vs4l_ctrl *ctrl);
	int (*queue)(struct vertex_graph *graph,
			struct vertex_container_list *incl,
			struct vertex_container_list *otcl);
	int (*deque)(struct vertex_graph *graph,
			struct vertex_container_list *clist);
	int (*start)(struct vertex_graph *graph);
	int (*stop)(struct vertex_graph *graph);
};

struct vertex_queue_list {
	struct vertex_queue		in_queue;
	struct vertex_queue		out_queue;
	const struct vertex_queue_gops	*gops;
	struct vertex_context		*vctx;
};

void vertex_queue_done(struct vertex_queue_list *qlist,
		struct vertex_container_list *inclist,
		struct vertex_container_list *otclist,
		unsigned long flags);

int vertex_queue_init(struct vertex_context *vctx);

#endif
