/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VERTEX_GRAPHMGR_H___
#define __VERTEX_GRAPHMGR_H___

#include <linux/kthread.h>
#include <linux/types.h>

#include "vertex-config.h"
#include "vertex-taskmgr.h"
#include "vertex-graph.h"
#include "vertex-interface.h"

struct vertex_system;

enum vertex_taskdesc_state {
	VERTEX_TASKDESC_STATE_FREE,
	VERTEX_TASKDESC_STATE_READY,
	VERTEX_TASKDESC_STATE_REQUEST,
	VERTEX_TASKDESC_STATE_ALLOC,
	VERTEX_TASKDESC_STATE_PROCESS,
	VERTEX_TASKDESC_STATE_COMPLETE
};

struct vertex_taskdesc {
	struct list_head	list;
	unsigned int		index;
	unsigned int		priority;
	struct vertex_graph	*graph;
	struct vertex_task	*task;
	unsigned int		state;
};

enum vertex_graphmgr_state {
	VERTEX_GRAPHMGR_OPEN,
	VERTEX_GRAPHMGR_ENUM
};

enum vertex_graphmgr_client {
	VERTEX_GRAPHMGR_CLIENT_GRAPH = 1,
	VERTEX_GRAPHMGR_CLIENT_INTERFACE
};

struct vertex_graphmgr {
	struct vertex_graph	*graph[VERTEX_MAX_GRAPH];
	atomic_t		active_cnt;
	unsigned long		state;
	struct mutex		mlock;

	unsigned int		tick_cnt;
	unsigned int		tick_pos;
	unsigned int		sched_cnt;
	unsigned int		sched_pos;
	struct kthread_worker	worker;
	struct task_struct	*graph_task;

	struct vertex_interface	*interface;

	struct mutex		tdlock;
	struct vertex_taskdesc	taskdesc[VERTEX_MAX_TASKDESC];
	struct list_head	tdfre_list;
	struct list_head	tdrdy_list;
	struct list_head	tdreq_list;
	struct list_head	tdalc_list;
	struct list_head	tdpro_list;
	struct list_head	tdcom_list;
	unsigned int		tdfre_cnt;
	unsigned int		tdrdy_cnt;
	unsigned int		tdreq_cnt;
	unsigned int		tdalc_cnt;
	unsigned int		tdpro_cnt;
	unsigned int		tdcom_cnt;

	struct vertex_graph_model	*current_model;
};

void vertex_taskdesc_print(struct vertex_graphmgr *gmgr);
int vertex_graphmgr_grp_register(struct vertex_graphmgr *gmgr,
		struct vertex_graph *graph);
int vertex_graphmgr_grp_unregister(struct vertex_graphmgr *gmgr,
		struct vertex_graph *graph);
int vertex_graphmgr_grp_start(struct vertex_graphmgr *gmgr,
		struct vertex_graph *graph);
int vertex_graphmgr_grp_stop(struct vertex_graphmgr *gmgr,
		struct vertex_graph *graph);
void vertex_graphmgr_queue(struct vertex_graphmgr *gmgr,
		struct vertex_task *task);

int vertex_graphmgr_open(struct vertex_graphmgr *gmgr);
int vertex_graphmgr_close(struct vertex_graphmgr *gmgr);

int vertex_graphmgr_probe(struct vertex_system *sys);
void vertex_graphmgr_remove(struct vertex_graphmgr *gmgr);

#endif
