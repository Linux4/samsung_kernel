/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/sched/types.h>

#include "vertex-log.h"
#include "vertex-device.h"
#include "vertex-graphmgr.h"

static void __vertex_taskdesc_set_free(struct vertex_graphmgr *gmgr,
		struct vertex_taskdesc *taskdesc)
{
	vertex_enter();
	taskdesc->state = VERTEX_TASKDESC_STATE_FREE;
	list_add_tail(&taskdesc->list, &gmgr->tdfre_list);
	gmgr->tdfre_cnt++;
	vertex_leave();
}

static struct vertex_taskdesc *__vertex_taskdesc_get_first_free(
		struct vertex_graphmgr *gmgr)
{
	struct vertex_taskdesc *taskdesc = NULL;

	vertex_enter();
	if (gmgr->tdfre_cnt)
		taskdesc = list_first_entry(&gmgr->tdfre_list,
				struct vertex_taskdesc, list);
	vertex_leave();
	return taskdesc;
}

static void __vertex_taskdesc_set_ready(struct vertex_graphmgr *gmgr,
		struct vertex_taskdesc *taskdesc)
{
	vertex_enter();
	taskdesc->state = VERTEX_TASKDESC_STATE_READY;
	list_add_tail(&taskdesc->list, &gmgr->tdrdy_list);
	gmgr->tdrdy_cnt++;
	vertex_leave();
}

static struct vertex_taskdesc *__vertex_taskdesc_pop_ready(
		struct vertex_graphmgr *gmgr)
{
	struct vertex_taskdesc *taskdesc = NULL;

	vertex_enter();
	if (gmgr->tdrdy_cnt) {
		taskdesc = container_of(gmgr->tdrdy_list.next,
				struct vertex_taskdesc, list);

		list_del(&taskdesc->list);
		gmgr->tdrdy_cnt--;
	}
	vertex_leave();
	return taskdesc;
}

static void __vertex_taskdesc_set_request(struct vertex_graphmgr *gmgr,
		struct vertex_taskdesc *taskdesc)
{
	vertex_enter();
	taskdesc->state = VERTEX_TASKDESC_STATE_REQUEST;
	list_add_tail(&taskdesc->list, &gmgr->tdreq_list);
	gmgr->tdreq_cnt++;
	vertex_leave();
}

static void __vertex_taskdesc_set_request_sched(struct vertex_graphmgr *gmgr,
		struct vertex_taskdesc *taskdesc, struct vertex_taskdesc *next)
{
	vertex_enter();
	taskdesc->state = VERTEX_TASKDESC_STATE_REQUEST;
	list_add_tail(&taskdesc->list, &next->list);
	gmgr->tdreq_cnt++;
	vertex_leave();
}

static void __vertex_taskdesc_set_process(struct vertex_graphmgr *gmgr,
		struct vertex_taskdesc *taskdesc)
{
	vertex_enter();
	taskdesc->state = VERTEX_TASKDESC_STATE_PROCESS;
	list_add_tail(&taskdesc->list, &gmgr->tdpro_list);
	gmgr->tdpro_cnt++;
	vertex_leave();
}

static void __vertex_taskdesc_set_complete(struct vertex_graphmgr *gmgr,
		struct vertex_taskdesc *taskdesc)
{
	vertex_enter();
	taskdesc->state = VERTEX_TASKDESC_STATE_COMPLETE;
	list_add_tail(&taskdesc->list, &gmgr->tdcom_list);
	gmgr->tdcom_cnt++;
	vertex_leave();
}

static void __vertex_taskdesc_trans_fre_to_rdy(struct vertex_graphmgr *gmgr,
		struct vertex_taskdesc *taskdesc)
{
	vertex_enter();
	if (!gmgr->tdfre_cnt) {
		vertex_warn("tdfre_cnt is zero\n");
		return;
	}

	if (taskdesc->state != VERTEX_TASKDESC_STATE_FREE) {
		vertex_warn("state(%x) is invlid\n", taskdesc->state);
		return;
	}

	list_del(&taskdesc->list);
	gmgr->tdfre_cnt--;
	__vertex_taskdesc_set_ready(gmgr, taskdesc);
	vertex_leave();
}

static void __vertex_taskdesc_trans_rdy_to_fre(struct vertex_graphmgr *gmgr,
		struct vertex_taskdesc *taskdesc)
{
	vertex_enter();
	if (!gmgr->tdrdy_cnt) {
		vertex_warn("tdrdy_cnt is zero\n");
		return;
	}

	if (taskdesc->state != VERTEX_TASKDESC_STATE_READY) {
		vertex_warn("state(%x) is invlid\n", taskdesc->state);
		return;
	}

	list_del(&taskdesc->list);
	gmgr->tdrdy_cnt--;
	__vertex_taskdesc_set_free(gmgr, taskdesc);
	vertex_leave();
}

static void __vertex_taskdesc_trans_req_to_pro(struct vertex_graphmgr *gmgr,
		struct vertex_taskdesc *taskdesc)
{
	vertex_enter();
	if (!gmgr->tdreq_cnt) {
		vertex_warn("tdreq_cnt is zero\n");
		return;
	}

	if (taskdesc->state != VERTEX_TASKDESC_STATE_REQUEST) {
		vertex_warn("state(%x) is invlid\n", taskdesc->state);
		return;
	}

	list_del(&taskdesc->list);
	gmgr->tdreq_cnt--;
	__vertex_taskdesc_set_process(gmgr, taskdesc);
	vertex_leave();
}

static void __vertex_taskdesc_trans_req_to_fre(struct vertex_graphmgr *gmgr,
		struct vertex_taskdesc *taskdesc)
{
	vertex_enter();
	if (!gmgr->tdreq_cnt) {
		vertex_warn("tdreq_cnt is zero\n");
		return;
	}

	if (taskdesc->state != VERTEX_TASKDESC_STATE_REQUEST) {
		vertex_warn("state(%x) is invlid\n", taskdesc->state);
		return;
	}

	list_del(&taskdesc->list);
	gmgr->tdreq_cnt--;
	__vertex_taskdesc_set_free(gmgr, taskdesc);
	vertex_leave();
}

static void __vertex_taskdesc_trans_alc_to_pro(struct vertex_graphmgr *gmgr,
		struct vertex_taskdesc *taskdesc)
{
	vertex_enter();
	if (!gmgr->tdalc_cnt) {
		vertex_warn("tdalc_cnt is zero\n");
		return;
	}

	if (taskdesc->state != VERTEX_TASKDESC_STATE_ALLOC) {
		vertex_warn("state(%x) is invlid\n", taskdesc->state);
		return;
	}

	list_del(&taskdesc->list);
	gmgr->tdalc_cnt--;
	__vertex_taskdesc_set_process(gmgr, taskdesc);
	vertex_leave();
}

static void __vertex_taskdesc_trans_alc_to_fre(struct vertex_graphmgr *gmgr,
		struct vertex_taskdesc *taskdesc)
{
	vertex_enter();
	if (!gmgr->tdalc_cnt) {
		vertex_warn("tdalc_cnt is zero\n");
		return;
	}

	if (taskdesc->state != VERTEX_TASKDESC_STATE_ALLOC) {
		vertex_warn("state(%x) is invlid\n", taskdesc->state);
		return;
	}

	list_del(&taskdesc->list);
	gmgr->tdalc_cnt--;
	__vertex_taskdesc_set_free(gmgr, taskdesc);
	vertex_leave();
}

static void __vertex_taskdesc_trans_pro_to_com(struct vertex_graphmgr *gmgr,
		struct vertex_taskdesc *taskdesc)
{
	vertex_enter();
	if (!gmgr->tdpro_cnt) {
		vertex_warn("tdpro_cnt is zero\n");
		return;
	}

	if (taskdesc->state != VERTEX_TASKDESC_STATE_PROCESS) {
		vertex_warn("state(%x) is invlid\n", taskdesc->state);
		return;
	}

	list_del(&taskdesc->list);
	gmgr->tdpro_cnt--;
	__vertex_taskdesc_set_complete(gmgr, taskdesc);
	vertex_leave();
}

static void __vertex_taskdesc_trans_pro_to_fre(struct vertex_graphmgr *gmgr,
		struct vertex_taskdesc *taskdesc)
{
	vertex_enter();
	if (!gmgr->tdpro_cnt) {
		vertex_warn("tdpro_cnt is zero\n");
		return;
	}

	if (taskdesc->state != VERTEX_TASKDESC_STATE_PROCESS) {
		vertex_warn("state(%x) is invlid\n", taskdesc->state);
		return;
	}

	list_del(&taskdesc->list);
	gmgr->tdpro_cnt--;
	__vertex_taskdesc_set_free(gmgr, taskdesc);
	vertex_leave();
}

static void __vertex_taskdesc_trans_com_to_fre(struct vertex_graphmgr *gmgr,
		struct vertex_taskdesc *taskdesc)
{
	vertex_enter();
	if (!gmgr->tdcom_cnt) {
		vertex_warn("tdcom_cnt is zero\n");
		return;
	}

	if (taskdesc->state != VERTEX_TASKDESC_STATE_COMPLETE) {
		vertex_warn("state(%x) is invlid\n", taskdesc->state);
		return;
	}

	list_del(&taskdesc->list);
	gmgr->tdcom_cnt--;
	__vertex_taskdesc_set_free(gmgr, taskdesc);
	vertex_leave();
}

static int __vertex_graphmgr_itf_init(struct vertex_interface *itf,
		struct vertex_graph *graph)
{
	int ret;
	unsigned long flags;
	struct vertex_taskmgr *itaskmgr;
	struct vertex_task *itask;

	vertex_enter();
	itaskmgr = &itf->taskmgr;

	taskmgr_e_barrier_irqs(itaskmgr, 0, flags);
	itask = vertex_task_pick_fre_to_req(itaskmgr);
	taskmgr_x_barrier_irqr(itaskmgr, 0, flags);
	if (!itask) {
		ret = -ENOMEM;
		vertex_err("itask is NULL\n");
		goto p_err_pick;
	}

	itask->id = 0;
	itask->lock = &graph->local_lock;
	itask->findex = VERTEX_MAX_TASK;
	itask->tdindex = VERTEX_MAX_TASKDESC;
	itask->message = VERTEX_TASK_INIT;
	itask->param0 = graph->uid;
	itask->param1 = graph->idx;
	itask->param2 = 0;
	itask->param3 = 0;

	ret = vertex_hw_init(itf, itask);
	if (ret)
		goto p_err_init;

	vertex_leave();
	return 0;
p_err_init:
p_err_pick:
	return ret;
}

static int __vertex_graphmgr_itf_deinit(struct vertex_interface *itf,
		struct vertex_graph *graph)
{
	int ret;
	unsigned long flags;
	struct vertex_taskmgr *itaskmgr;
	struct vertex_task *itask;

	vertex_enter();
	itaskmgr = &itf->taskmgr;

	taskmgr_e_barrier_irqs(itaskmgr, 0, flags);
	itask = vertex_task_pick_fre_to_req(itaskmgr);
	taskmgr_x_barrier_irqr(itaskmgr, 0, flags);
	if (!itask) {
		ret = -ENOMEM;
		vertex_err("itask is NULL\n");
		goto p_err_pick;
	}

	itask->id = 0;
	itask->lock = &graph->local_lock;
	itask->findex = VERTEX_MAX_TASK;
	itask->tdindex = VERTEX_MAX_TASKDESC;
	itask->message = VERTEX_TASK_DEINIT;
	itask->param0 = graph->uid;
	itask->param1 = graph->idx;
	itask->param2 = 0;
	itask->param3 = 0;

	ret = vertex_hw_deinit(itf, itask);
	if (ret)
		goto p_err_deinit;

	vertex_leave();
	return 0;
p_err_deinit:
p_err_pick:
	return ret;
}

static int __vertex_graphmgr_itf_create(struct vertex_interface *itf,
		struct vertex_graph *graph)
{
	int ret;
	unsigned long flags;
	struct vertex_taskmgr *itaskmgr;
	struct vertex_task *itask;

	/* TODO check */
	return 0;
	vertex_enter();
	itaskmgr = &itf->taskmgr;

	taskmgr_e_barrier_irqs(itaskmgr, 0, flags);
	itask = vertex_task_pick_fre_to_req(itaskmgr);
	taskmgr_x_barrier_irqr(itaskmgr, 0, flags);
	if (!itask) {
		ret = -ENOMEM;
		vertex_err("itask is NULL\n");
		goto p_err;
	}

	itask->id = 0;
	itask->lock = &graph->local_lock;
	itask->findex = VERTEX_MAX_TASK;
	itask->tdindex = VERTEX_MAX_TASKDESC;
	itask->message = VERTEX_TASK_CREATE;
	itask->param0 = graph->uid;
	itask->param1 = graph->idx;
	itask->param2 = 0;
	itask->param3 = 0;

	ret = vertex_hw_create(itf, itask);
	if (ret)
		goto p_err;

	vertex_leave();
	return 0;
p_err:
	return ret;
}

static int __vertex_graphmgr_itf_destroy(struct vertex_interface *itf,
		struct vertex_graph *graph)
{
	int ret;
	unsigned long flags;
	struct vertex_taskmgr *itaskmgr;
	struct vertex_task *itask;

	vertex_enter();
	itaskmgr = &itf->taskmgr;

	taskmgr_e_barrier_irqs(itaskmgr, 0, flags);
	itask = vertex_task_pick_fre_to_req(itaskmgr);
	taskmgr_x_barrier_irqr(itaskmgr, 0, flags);
	if (!itask) {
		ret = -ENOMEM;
		vertex_err("itask is NULL\n");
		goto p_err;
	}

	itask->id = 0;
	itask->lock = &graph->local_lock;
	itask->findex = VERTEX_MAX_TASK;
	itask->tdindex = VERTEX_MAX_TASKDESC;
	itask->message = VERTEX_TASK_DESTROY;
	itask->param0 = graph->uid;
	itask->param1 = graph->idx;
	itask->param2 = 0;
	itask->param3 = 0;

	ret = vertex_hw_destroy(itf, itask);
	if (ret)
		goto p_err;

	vertex_leave();
	return 0;
p_err:
	return ret;
}

static int __vertex_graphmgr_itf_config(struct vertex_interface *itf,
		struct vertex_graph *graph)
{
	int ret;
	unsigned long flags;
	struct vertex_taskmgr *itaskmgr;
	struct vertex_task *itask;

	vertex_enter();
	itaskmgr = &itf->taskmgr;

	taskmgr_e_barrier_irqs(itaskmgr, 0, flags);
	itask = vertex_task_pick_fre_to_req(itaskmgr);
	taskmgr_x_barrier_irqr(itaskmgr, 0, flags);
	if (!itask) {
		ret = -ENOMEM;
		vertex_err("itask is NULL\n");
		goto p_err;
	}

	itask->id = 0;
	itask->lock = &graph->local_lock;
	itask->findex = VERTEX_MAX_TASK;
	itask->tdindex = VERTEX_MAX_TASKDESC;
	itask->message = VERTEX_TASK_ALLOCATE;
	itask->param0 = graph->uid;
	itask->param1 = graph->idx;
	itask->param2 = (unsigned long)&graph->inflist;
	itask->param3 = (unsigned long)&graph->otflist;

	ret = vertex_hw_config(itf, itask);
	if (ret)
		goto p_err;

	vertex_leave();
	return 0;
p_err:
	return ret;
}

static int __vertex_graphmgr_itf_process(struct vertex_interface *itf,
		struct vertex_graph *graph, struct vertex_task *task)
{
	int ret;
	unsigned long flags;
	struct vertex_taskmgr *itaskmgr;
	struct vertex_task *itask;

	vertex_enter();
	itaskmgr = &itf->taskmgr;

	taskmgr_e_barrier_irqs(itaskmgr, 0, flags);
	itask = vertex_task_pick_fre_to_req(itaskmgr);
	taskmgr_x_barrier_irqr(itaskmgr, 0, flags);
	if (!itask) {
		ret = -ENOMEM;
		vertex_err("itask is NULL\n");
		goto p_err_pick;
	}

	if (test_bit(VERTEX_GRAPH_FLAG_UPDATE_PARAM, &graph->flags)) {
		ret = graph->gops->update_param(graph, task);
		if (ret)
			goto p_err_update_param;

		clear_bit(VERTEX_GRAPH_FLAG_UPDATE_PARAM, &graph->flags);
	}

	itask->id = task->id;
	itask->lock = &graph->local_lock;
	itask->findex = task->index;
	itask->tdindex = task->tdindex;
	itask->message = VERTEX_TASK_PROCESS;
	itask->param0 = graph->uid;
	itask->param1 = graph->idx;
	itask->param2 = 0; /* return : DONE or NDONE */
	itask->param3 = 0; /* return : error code if param2 is NDONE */
	itask->flags = task->flags;
	itask->incl = task->incl;
	itask->otcl = task->otcl;

	ret = graph->gops->process(graph, task);
	if (ret)
		goto p_err_process;

	ret = vertex_hw_process(itf, itask);
	if (ret)
		goto p_err_hw_process;

	vertex_leave();
	return 0;
p_err_hw_process:
p_err_process:
p_err_update_param:
p_err_pick:
	return ret;
}

static void __vertex_graphmgr_sched(struct vertex_graphmgr *gmgr)
{
	int ret;
	struct vertex_taskdesc *ready_td, *list_td, *temp;
	struct vertex_graph *graph;
	struct vertex_task *task;

	vertex_enter();

	mutex_lock(&gmgr->tdlock);

	while (1) {
		ready_td = __vertex_taskdesc_pop_ready(gmgr);
		if (!ready_td)
			break;

		if (!gmgr->tdreq_cnt) {
			__vertex_taskdesc_set_request(gmgr, ready_td);
			continue;
		}

		list_for_each_entry_safe(list_td, temp, &gmgr->tdreq_list,
				list) {
			if (ready_td->priority > list_td->priority) {
				__vertex_taskdesc_set_request_sched(gmgr,
						ready_td, list_td);
				break;
			}
		}

		if (ready_td->state == VERTEX_TASKDESC_STATE_READY)
			__vertex_taskdesc_set_request(gmgr, ready_td);
	}

	list_for_each_entry_safe(list_td, temp, &gmgr->tdreq_list, list) {
		__vertex_taskdesc_trans_req_to_pro(gmgr, list_td);
	}

	mutex_unlock(&gmgr->tdlock);

	list_for_each_entry_safe(list_td, temp, &gmgr->tdpro_list, list) {
		graph = list_td->graph;
		task = list_td->task;

		ret = __vertex_graphmgr_itf_process(gmgr->interface,
				graph, task);
		if (ret) {
			ret = graph->gops->cancel(graph, task);
			if (ret)
				return;

			list_td->graph = NULL;
			list_td->task = NULL;
			__vertex_taskdesc_trans_pro_to_fre(gmgr, list_td);
			continue;
		}

		__vertex_taskdesc_trans_pro_to_com(gmgr, list_td);
	}

	gmgr->sched_cnt++;
	vertex_leave();
}

static void vertex_graph_thread(struct kthread_work *work)
{
	int ret;
	struct vertex_task *task;
	struct vertex_graph *graph;
	struct vertex_graphmgr *gmgr;
	struct vertex_taskdesc *taskdesc, *temp;

	vertex_enter();
	task = container_of(work, struct vertex_task, work);
	graph = task->owner;
	gmgr = graph->owner;

	switch (task->message) {
	case VERTEX_TASK_REQUEST:
		ret = graph->gops->request(graph, task);
		if (ret)
			return;

		taskdesc = __vertex_taskdesc_get_first_free(gmgr);
		if (!taskdesc) {
			vertex_err("taskdesc is NULL\n");
			return;
		}

		task->tdindex = taskdesc->index;
		taskdesc->graph = graph;
		taskdesc->task = task;
		taskdesc->priority = graph->priority;
		__vertex_taskdesc_trans_fre_to_rdy(gmgr, taskdesc);
		break;
	case VERTEX_CTRL_STOP:
		list_for_each_entry_safe(taskdesc, temp,
				&gmgr->tdrdy_list, list) {
			if (taskdesc->graph->idx != graph->idx)
				continue;

			ret = graph->gops->cancel(graph, taskdesc->task);
			if (ret)
				return;

			taskdesc->graph = NULL;
			taskdesc->task = NULL;
			__vertex_taskdesc_trans_rdy_to_fre(gmgr, taskdesc);
		}

		mutex_lock(&gmgr->tdlock);

		list_for_each_entry_safe(taskdesc, temp,
				&gmgr->tdreq_list, list) {
			if (taskdesc->graph->idx != graph->idx)
				continue;

			ret = graph->gops->cancel(graph, taskdesc->task);
			if (ret)
				return;

			taskdesc->graph = NULL;
			taskdesc->task = NULL;
			__vertex_taskdesc_trans_req_to_fre(gmgr, taskdesc);
		}

		mutex_unlock(&gmgr->tdlock);

		ret = graph->gops->control(graph, task);
		if (ret)
			return;
		/* TODO check return/break */
		return;
	default:
		vertex_err("message of task is invalid (%d)\n", task->message);
		return;
	}

	__vertex_graphmgr_sched(gmgr);
	vertex_leave();
}

static void vertex_interface_thread(struct kthread_work *work)
{
	int ret;
	struct vertex_task *task, *itask;
	struct vertex_interface *itf;
	struct vertex_graphmgr *gmgr;
	unsigned int taskdesc_index;
	struct vertex_taskmgr *itaskmgr;
	struct vertex_graph *graph;
	struct vertex_taskdesc *taskdesc;
	unsigned long flags;

	vertex_enter();
	itask = container_of(work, struct vertex_task, work);
	itf = itask->owner;
	itaskmgr = &itf->taskmgr;
	gmgr = itf->cookie;

	if (itask->findex >= VERTEX_MAX_TASK) {
		vertex_err("task index(%u) is invalid (%u)\n",
				itask->findex, VERTEX_MAX_TASK);
		return;
	}

	switch (itask->message) {
	case VERTEX_TASK_ALLOCATE:
		taskdesc_index = itask->tdindex;
		if (taskdesc_index >= VERTEX_MAX_TASKDESC) {
			vertex_err("taskdesc index(%u) is invalid (%u)\n",
					taskdesc_index, VERTEX_MAX_TASKDESC);
			return;
		}

		taskdesc = &gmgr->taskdesc[taskdesc_index];
		if (taskdesc->state != VERTEX_TASKDESC_STATE_ALLOC) {
			vertex_err("taskdesc state(%u) is not ALLOC (%u)\n",
					taskdesc->state, taskdesc_index);
			return;
		}

		graph = taskdesc->graph;
		if (!graph) {
			vertex_err("graph is NULL (%u)\n", taskdesc_index);
			return;
		}

		task = taskdesc->task;
		if (!task) {
			vertex_err("task is NULL (%u)\n", taskdesc_index);
			return;
		}

		task->message = itask->message;
		task->param0 = itask->param2;
		task->param1 = itask->param3;

		/* return status check */
		if (task->param0) {
			vertex_err("task allocation failed (%lu, %lu)\n",
					task->param0, task->param1);

			ret = graph->gops->cancel(graph, task);
			if (ret)
				return;

			/* taskdesc cleanup */
			mutex_lock(&gmgr->tdlock);
			taskdesc->graph = NULL;
			taskdesc->task = NULL;
			__vertex_taskdesc_trans_alc_to_fre(gmgr, taskdesc);
			mutex_unlock(&gmgr->tdlock);
		} else {
			/* taskdesc transition */
			mutex_lock(&gmgr->tdlock);
			__vertex_taskdesc_trans_alc_to_pro(gmgr, taskdesc);
			mutex_unlock(&gmgr->tdlock);
		}

		/* itask cleanup */
		taskmgr_e_barrier_irqs(itaskmgr, 0, flags);
		vertex_task_trans_com_to_fre(itaskmgr, itask);
		taskmgr_x_barrier_irqr(itaskmgr, 0, flags);
		break;
	case VERTEX_TASK_PROCESS:
		taskdesc_index = itask->tdindex;
		if (taskdesc_index >= VERTEX_MAX_TASKDESC) {
			vertex_err("taskdesc index(%u) is invalid (%u)\n",
					taskdesc_index, VERTEX_MAX_TASKDESC);
			return;
		}

		taskdesc = &gmgr->taskdesc[taskdesc_index];
		if (taskdesc->state == VERTEX_TASKDESC_STATE_FREE) {
			vertex_err("taskdesc(%u) state is FREE\n",
					taskdesc_index);

			/* itask cleanup */
			taskmgr_e_barrier_irqs(itaskmgr, 0, flags);
			vertex_task_trans_com_to_fre(itaskmgr, itask);
			taskmgr_x_barrier_irqr(itaskmgr, 0, flags);
			break;
		}

		if (taskdesc->state != VERTEX_TASKDESC_STATE_COMPLETE) {
			vertex_err("taskdesc state is not COMPLETE (%u)\n",
					taskdesc->state);
			return;
		}

		graph = taskdesc->graph;
		if (!graph) {
			vertex_err("graph is NULL (%u)\n", taskdesc_index);
			return;
		}

		task = taskdesc->task;
		if (!task) {
			vertex_err("task is NULL (%u)\n", taskdesc_index);
			return;
		}

		task->message = itask->message;
		task->tdindex = VERTEX_MAX_TASKDESC;
		task->param0 = itask->param2;
		task->param1 = itask->param3;

		ret = graph->gops->done(graph, task);
		if (ret)
			return;

		/* taskdesc cleanup */
		taskdesc->graph = NULL;
		taskdesc->task = NULL;
		__vertex_taskdesc_trans_com_to_fre(gmgr, taskdesc);

		/* itask cleanup */
		taskmgr_e_barrier_irqs(itaskmgr, 0, flags);
		vertex_task_trans_com_to_fre(itaskmgr, itask);
		taskmgr_x_barrier_irqr(itaskmgr, 0, flags);
		break;
	default:
		vertex_err("message of task is invalid (%d)\n", itask->message);
		return;
	}

	__vertex_graphmgr_sched(gmgr);
	vertex_leave();
}

void vertex_taskdesc_print(struct vertex_graphmgr *gmgr)
{
	vertex_enter();
	vertex_leave();
}

int vertex_graphmgr_grp_register(struct vertex_graphmgr *gmgr,
		struct vertex_graph *graph)
{
	int ret;
	unsigned int index;

	vertex_enter();
	mutex_lock(&gmgr->mlock);
	for (index = 0; index < VERTEX_MAX_GRAPH; ++index) {
		if (!gmgr->graph[index]) {
			gmgr->graph[index] = graph;
			graph->idx = index;
			break;
		}
	}
	mutex_unlock(&gmgr->mlock);

	if (index >= VERTEX_MAX_GRAPH) {
		ret = -EINVAL;
		vertex_err("graph slot is lack\n");
		goto p_err;
	}

	kthread_init_work(&graph->control.work, vertex_graph_thread);
	for (index = 0; index < VERTEX_MAX_TASK; ++index)
		kthread_init_work(&graph->taskmgr.task[index].work,
				vertex_graph_thread);

	graph->global_lock = &gmgr->mlock;
	atomic_inc(&gmgr->active_cnt);

	vertex_leave();
	return 0;
p_err:
	return ret;
}

int vertex_graphmgr_grp_unregister(struct vertex_graphmgr *gmgr,
		struct vertex_graph *graph)
{
	vertex_enter();
	mutex_lock(&gmgr->mlock);
	gmgr->graph[graph->idx] = NULL;
	mutex_unlock(&gmgr->mlock);
	atomic_dec(&gmgr->active_cnt);
	vertex_leave();
	return 0;
}

int vertex_graphmgr_grp_start(struct vertex_graphmgr *gmgr,
		struct vertex_graph *graph)
{
	int ret;

	vertex_enter();
	mutex_lock(&gmgr->mlock);
	if (!test_bit(VERTEX_GRAPHMGR_ENUM, &gmgr->state)) {
		ret = __vertex_graphmgr_itf_init(gmgr->interface, graph);
		if (ret) {
			mutex_unlock(&gmgr->mlock);
			goto p_err_init;
		}
		set_bit(VERTEX_GRAPHMGR_ENUM, &gmgr->state);
	}
	mutex_unlock(&gmgr->mlock);

	ret = __vertex_graphmgr_itf_create(gmgr->interface, graph);
	if (ret)
		goto p_err_create;

	ret = __vertex_graphmgr_itf_config(gmgr->interface, graph);
	if (ret)
		goto p_err_config;

	vertex_leave();
	return 0;
p_err_config:
p_err_create:
p_err_init:
	return ret;
}

int vertex_graphmgr_grp_stop(struct vertex_graphmgr *gmgr,
		struct vertex_graph *graph)
{
	int ret;

	vertex_enter();
	ret = __vertex_graphmgr_itf_destroy(gmgr->interface, graph);
	if (ret)
		goto p_err_destroy;

	mutex_lock(&gmgr->mlock);
	if (test_bit(VERTEX_GRAPHMGR_ENUM, &gmgr->state) &&
			atomic_read(&gmgr->active_cnt) == 1) {
		ret = __vertex_graphmgr_itf_deinit(gmgr->interface, graph);
		if (ret) {
			mutex_unlock(&gmgr->mlock);
			goto p_err_deinit;
		}
		clear_bit(VERTEX_GRAPHMGR_ENUM, &gmgr->state);
	}
	mutex_unlock(&gmgr->mlock);

	vertex_leave();
	return 0;
p_err_deinit:
p_err_destroy:
	return ret;
}

void vertex_graphmgr_queue(struct vertex_graphmgr *gmgr,
		struct vertex_task *task)
{
	vertex_enter();
	kthread_queue_work(&gmgr->worker, &task->work);
	vertex_leave();
}

int vertex_graphmgr_open(struct vertex_graphmgr *gmgr)
{
	int ret;
	char name[30];
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	vertex_enter();
	kthread_init_worker(&gmgr->worker);
	snprintf(name, sizeof(name), "vertex_graph");
	gmgr->graph_task = kthread_run(kthread_worker_fn,
			&gmgr->worker, name);
	if (IS_ERR(gmgr->graph_task)) {
		ret = PTR_ERR(gmgr->graph_task);
		vertex_err("kthread_run is fail\n");
		goto p_err_run;
	}

	ret = sched_setscheduler_nocheck(gmgr->graph_task,
			SCHED_FIFO, &param);
	if (ret) {
		vertex_err("sched_setscheduler_nocheck is fail(%d)\n", ret);
		goto p_err_sched;
	}

	gmgr->current_model = NULL;
	set_bit(VERTEX_GRAPHMGR_OPEN, &gmgr->state);
	vertex_leave();
	return 0;
p_err_sched:
	kthread_stop(gmgr->graph_task);
p_err_run:
	return ret;
}

int vertex_graphmgr_close(struct vertex_graphmgr *gmgr)
{
	vertex_enter();
	kthread_stop(gmgr->graph_task);
	clear_bit(VERTEX_GRAPHMGR_OPEN, &gmgr->state);
	clear_bit(VERTEX_GRAPHMGR_ENUM, &gmgr->state);
	vertex_leave();
	return 0;
}

int vertex_graphmgr_probe(struct vertex_system *sys)
{
	struct vertex_graphmgr *gmgr;
	int index;

	vertex_enter();
	gmgr = &sys->graphmgr;
	gmgr->interface = &sys->interface;

	atomic_set(&gmgr->active_cnt, 0);
	mutex_init(&gmgr->mlock);
	mutex_init(&gmgr->tdlock);

	INIT_LIST_HEAD(&gmgr->tdfre_list);
	INIT_LIST_HEAD(&gmgr->tdrdy_list);
	INIT_LIST_HEAD(&gmgr->tdreq_list);
	INIT_LIST_HEAD(&gmgr->tdalc_list);
	INIT_LIST_HEAD(&gmgr->tdpro_list);
	INIT_LIST_HEAD(&gmgr->tdcom_list);

	for (index = 0; index < VERTEX_MAX_TASKDESC; ++index) {
		gmgr->taskdesc[index].index = index;
		__vertex_taskdesc_set_free(gmgr, &gmgr->taskdesc[index]);
	}

	for (index = 0; index < VERTEX_MAX_TASK; ++index)
		kthread_init_work(&gmgr->interface->taskmgr.task[index].work,
				vertex_interface_thread);

	return 0;
}

void vertex_graphmgr_remove(struct vertex_graphmgr *gmgr)
{
	mutex_destroy(&gmgr->tdlock);
	mutex_destroy(&gmgr->mlock);
}
