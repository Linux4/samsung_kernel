/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/delay.h>

#include "vertex-log.h"
#include "vertex-core.h"
#include "vertex-queue.h"
#include "vertex-context.h"
#include "vertex-graphmgr.h"
#include "vertex-graph.h"

#define VERTEX_GRAPH_MAX_PRIORITY		(20)
#define VERTEX_GRAPH_TIMEOUT		(3 * HZ)

static int __vertex_graph_start(struct vertex_graph *graph)
{
	int ret;

	vertex_enter();
	if (test_bit(VERTEX_GRAPH_STATE_START, &graph->state))
		return 0;

	ret = vertex_graphmgr_grp_start(graph->owner, graph);
	if (ret)
		goto p_err;

	set_bit(VERTEX_GRAPH_STATE_START, &graph->state);

	vertex_leave();
	return 0;
p_err:
	return ret;
}

static int __vertex_graph_stop(struct vertex_graph *graph)
{
	unsigned int timeout, retry;
	struct vertex_taskmgr *tmgr;
	struct vertex_task *control;

	vertex_enter();
	if (!test_bit(VERTEX_GRAPH_STATE_START, &graph->state))
		return 0;

	tmgr = &graph->taskmgr;
	if (tmgr->req_cnt + tmgr->pre_cnt) {
		control = &graph->control;
		control->message = VERTEX_CTRL_STOP;
		vertex_graphmgr_queue(graph->owner, control);
		timeout = wait_event_timeout(graph->control_wq,
			control->message == VERTEX_CTRL_STOP_DONE,
			VERTEX_GRAPH_TIMEOUT);
		if (!timeout)
			vertex_err("wait time(%u ms) is expired (%u)\n",
					jiffies_to_msecs(VERTEX_GRAPH_TIMEOUT),
					graph->idx);
	}

	retry = VERTEX_STOP_WAIT_COUNT;
	while (tmgr->req_cnt) {
		if (!retry)
			break;

		vertex_warn("Waiting request count(%u) to be zero (%u/%u)\n",
				tmgr->req_cnt, retry, graph->idx);
		udelay(10);
	}

	if (tmgr->req_cnt)
		vertex_err("request count(%u) is remained (%u)\n",
				tmgr->req_cnt, graph->idx);

	retry = VERTEX_STOP_WAIT_COUNT;
	while (tmgr->pre_cnt) {
		if (!retry)
			break;

		vertex_warn("Waiting prepare count(%u) to be zero (%u/%u)\n",
				tmgr->pre_cnt, retry, graph->idx);
		udelay(10);
	}

	if (tmgr->pre_cnt)
		vertex_err("prepare count(%u) is remained (%u)\n",
				tmgr->pre_cnt, graph->idx);

	retry = VERTEX_STOP_WAIT_COUNT;
	while (tmgr->pro_cnt) {
		if (!retry)
			break;

		vertex_warn("Waiting process count(%u) to be zero (%u/%u)\n",
				tmgr->pro_cnt, retry, graph->idx);
		udelay(10);
	}

	if (tmgr->pro_cnt)
		vertex_err("process count(%u) is remained (%u)\n",
				tmgr->pro_cnt, graph->idx);

	vertex_graphmgr_grp_stop(graph->owner, graph);
	vertex_task_flush(tmgr);
	clear_bit(VERTEX_GRAPH_STATE_START, &graph->state);

	vertex_leave();
	return 0;
}

static int vertex_graph_set_graph(struct vertex_graph *graph,
		struct vs4l_graph *ginfo)
{
	int ret;

	vertex_enter();
	if (test_bit(VERTEX_GRAPH_STATE_CONFIG, &graph->state)) {
		ret = -EINVAL;
		vertex_err("graph(%u) is already configured (%lu)\n",
				graph->idx, graph->state);
		goto p_err;
	}

	if (ginfo->priority > VERTEX_GRAPH_MAX_PRIORITY) {
		vertex_warn("graph(%u) priority is over (%u/%u)\n",
				graph->idx, ginfo->priority,
				VERTEX_GRAPH_MAX_PRIORITY);
		ginfo->priority = VERTEX_GRAPH_MAX_PRIORITY;
	}

	graph->uid = ginfo->id;
	graph->flags = ginfo->flags;
	graph->priority = ginfo->priority;

	set_bit(VERTEX_GRAPH_STATE_CONFIG, &graph->state);

	vertex_leave();
	return 0;
p_err:
	return ret;
}

static int vertex_graph_set_format(struct vertex_graph *graph,
		struct vs4l_format_list *flist)
{
	int ret;
	struct vertex_format_list *in_flist;
	struct vertex_format_list *ot_flist;
	unsigned int cnt;

	vertex_enter();
	in_flist = &graph->inflist;
	ot_flist = &graph->otflist;

	if (flist->direction == VS4L_DIRECTION_IN) {
		if (in_flist->count != flist->count) {
			kfree(in_flist->formats);

			in_flist->count = flist->count;
			in_flist->formats = kcalloc(in_flist->count,
					sizeof(*in_flist->formats), GFP_KERNEL);
			if (!in_flist->formats) {
				ret = -ENOMEM;
				vertex_err("Failed to alloc in_flist formats\n");
				goto p_err;
			}

			for (cnt = 0; cnt < in_flist->count; ++cnt) {
				in_flist->formats[cnt].format =
					flist->formats[cnt].format;
				in_flist->formats[cnt].plane =
					flist->formats[cnt].plane;
				in_flist->formats[cnt].width =
					flist->formats[cnt].width;
				in_flist->formats[cnt].height =
					flist->formats[cnt].height;
			}
		}
	} else if (flist->direction == VS4L_DIRECTION_OT) {
		if (ot_flist->count != flist->count) {
			kfree(ot_flist->formats);

			ot_flist->count = flist->count;
			ot_flist->formats = kcalloc(ot_flist->count,
					sizeof(*ot_flist->formats), GFP_KERNEL);
			if (!ot_flist->formats) {
				ret = -ENOMEM;
				vertex_err("Failed to alloc ot_flist formats\n");
				goto p_err;
			}

			for (cnt = 0; cnt < ot_flist->count; ++cnt) {
				ot_flist->formats[cnt].format =
					flist->formats[cnt].format;
				ot_flist->formats[cnt].plane =
					flist->formats[cnt].plane;
				ot_flist->formats[cnt].width =
					flist->formats[cnt].width;
				ot_flist->formats[cnt].height =
					flist->formats[cnt].height;
			}
		}
	} else {
		ret = -EINVAL;
		vertex_err("invalid direction (%d)\n", flist->direction);
		goto p_err;
	}

	vertex_leave();
	return 0;
p_err:
	kfree(in_flist->formats);
	in_flist->formats = NULL;
	in_flist->count = 0;

	kfree(ot_flist->formats);
	ot_flist->formats = NULL;
	ot_flist->count = 0;
	return ret;
}

static int vertex_graph_set_param(struct vertex_graph *graph,
		struct vs4l_param_list *plist)
{
	vertex_enter();
	set_bit(VERTEX_GRAPH_FLAG_UPDATE_PARAM, &graph->flags);
	vertex_leave();
	return 0;
}

static int vertex_graph_set_ctrl(struct vertex_graph *graph,
		struct vs4l_ctrl *ctrl)
{
	vertex_enter();
	vertex_graph_print(graph);
	vertex_leave();
	return 0;
}

static int vertex_graph_queue(struct vertex_graph *graph,
		struct vertex_container_list *incl,
		struct vertex_container_list *otcl)
{
	int ret;
	struct vertex_taskmgr *tmgr;
	struct vertex_task *task;
	unsigned long flags;

	vertex_enter();
	tmgr = &graph->taskmgr;

	if (!test_bit(VERTEX_GRAPH_STATE_START, &graph->state)) {
		ret = -EINVAL;
		vertex_err("graph(%u) is not started (%lu)\n",
				graph->idx, graph->state);
		goto p_err;
	}

	if (incl->id != otcl->id) {
		vertex_warn("buffer id is incoincidence (%u/%u)\n",
				incl->id, otcl->id);
		otcl->id = incl->id;
	}

	taskmgr_e_barrier_irqs(tmgr, 0, flags);
	task = vertex_task_pick_fre_to_req(tmgr);
	taskmgr_x_barrier_irqr(tmgr, 0, flags);
	if (!task) {
		ret = -ENOMEM;
		vertex_err("free task is not remained (%u)\n", graph->idx);
		vertex_task_print_all(tmgr);
		goto p_err;
	}

	graph->inhash[incl->index] = task->index;
	graph->othash[otcl->index] = task->index;
	graph->input_cnt++;

	task->id = incl->id;
	task->incl = incl;
	task->otcl = otcl;
	task->message = VERTEX_TASK_REQUEST;
	task->param0 = 0;
	task->param1 = 0;
	task->param2 = 0;
	task->param3 = 0;
	clear_bit(VS4L_CL_FLAG_TIMESTAMP, &task->flags);

	if ((incl->flags & (1 << VS4L_CL_FLAG_TIMESTAMP)) ||
		(otcl->flags & (1 << VS4L_CL_FLAG_TIMESTAMP))) {
		set_bit(VS4L_CL_FLAG_TIMESTAMP, &task->flags);
		vertex_get_timestamp(&task->time[VERTEX_TIME_QUEUE]);
	}

	vertex_graphmgr_queue(graph->owner, task);

	vertex_leave();
	return 0;
p_err:
	return ret;
}

static int vertex_graph_deque(struct vertex_graph *graph,
		struct vertex_container_list *clist)
{
	int ret;
	struct vertex_taskmgr *tmgr;
	unsigned int tidx;
	struct vertex_task *task;
	unsigned long flags;

	vertex_enter();
	tmgr = &graph->taskmgr;

	if (!test_bit(VERTEX_GRAPH_STATE_START, &graph->state)) {
		ret = -EINVAL;
		vertex_err("graph(%u) is not started (%lu)\n",
				graph->idx, graph->state);
		goto p_err;
	}

	if (clist->direction == VS4L_DIRECTION_IN)
		tidx = graph->inhash[clist->index];
	else
		tidx = graph->othash[clist->index];

	if (tidx >= VERTEX_MAX_TASK) {
		ret = -EINVAL;
		vertex_err("task index(%u) invalid (%u)\n", tidx, graph->idx);
		goto p_err;
	}

	task = &tmgr->task[tidx];
	if (task->state != VERTEX_TASK_STATE_COMPLETE) {
		ret = -EINVAL;
		vertex_err("task(%u) state(%d) is invalid (%u)\n",
				tidx, task->state, graph->idx);
		goto p_err;
	}

	if (clist->direction == VS4L_DIRECTION_IN) {
		if (task->incl != clist) {
			ret = -EINVAL;
			vertex_err("incl ptr is invalid (%u)\n", graph->idx);
			goto p_err;
		}

		graph->inhash[clist->index] = VERTEX_MAX_TASK;
		task->incl = NULL;
	} else {
		if (task->otcl != clist) {
			ret = -EINVAL;
			vertex_err("otcl ptr is invalid (%u)\n", graph->idx);
			goto p_err;
		}

		graph->othash[clist->index] = VERTEX_MAX_TASK;
		task->otcl = NULL;
	}

	if (task->incl || task->otcl)
		return 0;

	taskmgr_e_barrier_irqs(tmgr, 0, flags);
	vertex_task_trans_com_to_fre(tmgr, task);
	taskmgr_x_barrier_irqr(tmgr, 0, flags);

	vertex_leave();
	return 0;
p_err:
	return ret;
}

static int vertex_graph_start(struct vertex_graph *graph)
{
	int ret;

	vertex_enter();
	ret = __vertex_graph_start(graph);
	if (ret)
		goto p_err;

	vertex_leave();
	return 0;
p_err:
	return ret;
}

static int vertex_graph_stop(struct vertex_graph *graph)
{
	vertex_enter();

	__vertex_graph_stop(graph);

	vertex_leave();
	return 0;
}

const struct vertex_queue_gops vertex_queue_gops = {
	.set_graph	= vertex_graph_set_graph,
	.set_format	= vertex_graph_set_format,
	.set_param	= vertex_graph_set_param,
	.set_ctrl	= vertex_graph_set_ctrl,
	.queue		= vertex_graph_queue,
	.deque		= vertex_graph_deque,
	.start		= vertex_graph_start,
	.stop		= vertex_graph_stop
};

static int vertex_graph_control(struct vertex_graph *graph,
		struct vertex_task *task)
{
	int ret;
	struct vertex_taskmgr *tmgr;

	vertex_enter();
	tmgr = &graph->taskmgr;

	switch (task->message) {
	case VERTEX_CTRL_STOP:
		graph->control.message = VERTEX_CTRL_STOP_DONE;
		wake_up(&graph->control_wq);
		break;
	default:
		ret = -EINVAL;
		vertex_err("invalid task message(%u) of graph(%u)\n",
				task->message, graph->idx);
		vertex_task_print_all(tmgr);
		goto p_err;
	}

	vertex_leave();
	return 0;
p_err:
	return ret;
}

static int vertex_graph_request(struct vertex_graph *graph,
		struct vertex_task *task)
{
	int ret;
	struct vertex_taskmgr *tmgr;
	unsigned long flags;

	vertex_enter();
	tmgr = &graph->taskmgr;
	if (task->state != VERTEX_TASK_STATE_REQUEST) {
		ret = -EINVAL;
		vertex_err("task state(%u) is not REQUEST (graph:%u)\n",
				task->state, graph->idx);
		goto p_err;
	}

	taskmgr_e_barrier_irqs(tmgr, 0, flags);
	vertex_task_trans_req_to_pre(tmgr, task);
	taskmgr_x_barrier_irqr(tmgr, 0, flags);

	if (test_bit(VS4L_CL_FLAG_TIMESTAMP, &task->flags))
		vertex_get_timestamp(&task->time[VERTEX_TIME_REQUEST]);

	vertex_leave();
	return 0;
p_err:
	return ret;
}

static int vertex_graph_process(struct vertex_graph *graph,
		struct vertex_task *task)
{
	int ret;
	struct vertex_taskmgr *tmgr;
	unsigned long flags;

	vertex_enter();
	tmgr = &graph->taskmgr;
	if (task->state != VERTEX_TASK_STATE_PREPARE) {
		ret = -EINVAL;
		vertex_err("task state(%u) is not PREPARE (graph:%u)\n",
				task->state, graph->idx);
		goto p_err;
	}

	taskmgr_e_barrier_irqs(tmgr, TASKMGR_IDX_0, flags);
	vertex_task_trans_pre_to_pro(tmgr, task);
	taskmgr_x_barrier_irqr(tmgr, TASKMGR_IDX_0, flags);

	if (test_bit(VS4L_CL_FLAG_TIMESTAMP, &task->flags))
		vertex_get_timestamp(&task->time[VERTEX_TIME_PROCESS]);

	vertex_leave();
	return 0;
p_err:
	return ret;
}

static int vertex_graph_cancel(struct vertex_graph *graph,
		struct vertex_task *task)
{
	int ret;
	struct vertex_taskmgr *tmgr;
	struct vertex_queue_list *qlist;
	struct vertex_container_list *incl, *otcl;
	unsigned long result;
	unsigned long flags;

	vertex_enter();
	tmgr = &graph->taskmgr;
	qlist = &graph->vctx->queue_list;
	incl = task->incl;
	otcl = task->otcl;

	if (task->state != VERTEX_TASK_STATE_PROCESS) {
		ret = -EINVAL;
		vertex_err("task state(%u) is not PROCESS (graph:%u)\n",
				task->state, graph->idx);
		goto p_err;
	}

	if (test_bit(VS4L_CL_FLAG_TIMESTAMP, &task->flags)) {
		vertex_get_timestamp(&task->time[VERTEX_TIME_DONE]);

		if (incl->flags & (1 << VS4L_CL_FLAG_TIMESTAMP))
			memcpy(incl->timestamp, task->time, sizeof(task->time));

		if (otcl->flags & (1 << VS4L_CL_FLAG_TIMESTAMP))
			memcpy(otcl->timestamp, task->time, sizeof(task->time));
	}

	taskmgr_e_barrier_irqs(tmgr, TASKMGR_IDX_0, flags);
	vertex_task_trans_pro_to_com(tmgr, task);
	taskmgr_x_barrier_irqr(tmgr, TASKMGR_IDX_0, flags);

	graph->recent = task->id;
	graph->done_cnt++;

	result = 0;
	set_bit(VS4L_CL_FLAG_DONE, &result);
	set_bit(VS4L_CL_FLAG_INVALID, &result);

	vertex_queue_done(qlist, incl, otcl, result);

	vertex_leave();
	return 0;
p_err:
	return ret;
}

static int vertex_graph_done(struct vertex_graph *graph,
		struct vertex_task *task)
{
	int ret;
	struct vertex_taskmgr *tmgr;
	struct vertex_queue_list *qlist;
	struct vertex_container_list *incl, *otcl;
	unsigned long result;
	unsigned long flags;

	vertex_enter();
	tmgr = &graph->taskmgr;
	qlist = &graph->vctx->queue_list;
	incl = task->incl;
	otcl = task->otcl;

	if (task->state != VERTEX_TASK_STATE_PROCESS) {
		ret = -EINVAL;
		vertex_err("task state(%u) is not PROCESS (graph:%u)\n",
				task->state, graph->idx);
		goto p_err;
	}

	if (test_bit(VS4L_CL_FLAG_TIMESTAMP, &task->flags)) {
		vertex_get_timestamp(&task->time[VERTEX_TIME_DONE]);

		if (incl->flags & (1 << VS4L_CL_FLAG_TIMESTAMP))
			memcpy(incl->timestamp, task->time, sizeof(task->time));

		if (otcl->flags & (1 << VS4L_CL_FLAG_TIMESTAMP))
			memcpy(otcl->timestamp, task->time, sizeof(task->time));
	}

	taskmgr_e_barrier_irqs(tmgr, TASKMGR_IDX_0, flags);
	vertex_task_trans_pro_to_com(tmgr, task);
	taskmgr_x_barrier_irqr(tmgr, TASKMGR_IDX_0, flags);

	graph->recent = task->id;
	graph->done_cnt++;

	result = 0;
	if (task->param0) {
		set_bit(VS4L_CL_FLAG_DONE, &result);
		set_bit(VS4L_CL_FLAG_INVALID, &result);
	} else {
		set_bit(VS4L_CL_FLAG_DONE, &result);
	}

	vertex_queue_done(qlist, incl, otcl, result);

	vertex_leave();
	return 0;
p_err:
	return ret;
}

static int vertex_graph_update_param(struct vertex_graph *graph,
		struct vertex_task *task)
{
	vertex_enter();
	vertex_leave();
	return 0;
}

static const struct vertex_graph_ops vertex_graph_ops = {
	.control	= vertex_graph_control,
	.request	= vertex_graph_request,
	.process	= vertex_graph_process,
	.cancel		= vertex_graph_cancel,
	.done		= vertex_graph_done,
	.update_param	= vertex_graph_update_param
};

void vertex_graph_print(struct vertex_graph *graph)
{
	vertex_enter();
	vertex_leave();
}

struct vertex_graph *vertex_graph_create(struct vertex_context *vctx,
		void *graphmgr)
{
	int ret;
	struct vertex_graph *graph;
	struct vertex_taskmgr *tmgr;
	unsigned int idx;

	vertex_enter();
	graph = kzalloc(sizeof(*graph), GFP_KERNEL);
	if (!graph) {
		ret = -ENOMEM;
		vertex_err("Failed to allocate graph\n");
		goto p_err_kzalloc;
	}

	ret = vertex_graphmgr_grp_register(graphmgr, graph);
	if (ret)
		goto p_err_grp_register;

	graph->owner = graphmgr;
	graph->gops = &vertex_graph_ops;
	mutex_init(&graph->local_lock);
	graph->control.owner = graph;
	graph->control.message = VERTEX_CTRL_NONE;
	init_waitqueue_head(&graph->control_wq);

	tmgr = &graph->taskmgr;
	spin_lock_init(&tmgr->slock);
	ret = vertex_task_init(tmgr, graph->idx, graph);
	if (ret)
		goto p_err_task_init;

	for (idx = 0; idx < VERTEX_MAX_TASK; ++idx) {
		graph->inhash[idx] = VERTEX_MAX_TASK;
		graph->othash[idx] = VERTEX_MAX_TASK;
	}

	INIT_LIST_HEAD(&graph->gmodel_list);
	graph->gmodel_count = 0;

	graph->vctx = vctx;

	vertex_leave();
	return graph;
p_err_task_init:
p_err_grp_register:
	kfree(graph);
p_err_kzalloc:
	return ERR_PTR(ret);
}

int vertex_graph_destroy(struct vertex_graph *graph)
{
	vertex_enter();
	__vertex_graph_stop(graph);
	vertex_graphmgr_grp_unregister(graph->owner, graph);

	kfree(graph->inflist.formats);
	graph->inflist.formats = NULL;
	graph->inflist.count = 0;

	kfree(graph->otflist.formats);
	graph->otflist.formats = NULL;
	graph->otflist.count = 0;

	kfree(graph);
	vertex_leave();
	return 0;
}
