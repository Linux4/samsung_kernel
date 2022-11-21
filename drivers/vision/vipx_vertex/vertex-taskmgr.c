/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "vertex-log.h"
#include "vertex-taskmgr.h"

static void __vertex_task_s_free(struct vertex_taskmgr *tmgr,
		struct vertex_task *task)
{
	vertex_enter();
	task->state = VERTEX_TASK_STATE_FREE;
	list_add_tail(&task->list, &tmgr->fre_list);
	tmgr->fre_cnt++;
	vertex_leave();
}

static struct vertex_task *__vertex_task_get_first_free(
		struct vertex_taskmgr *tmgr)
{
	struct vertex_task *task;

	vertex_enter();
	if (tmgr->fre_cnt)
		task = list_first_entry(&tmgr->fre_list,
				struct vertex_task, list);
	else
		task = NULL;
	vertex_leave();
	return task;
}

static void __vertex_task_s_request(struct vertex_taskmgr *tmgr,
		struct vertex_task *task)
{
	vertex_enter();
	task->state = VERTEX_TASK_STATE_REQUEST;
	list_add_tail(&task->list, &tmgr->req_list);
	tmgr->req_cnt++;
	vertex_leave();
}

static void __vertex_task_s_prepare(struct vertex_taskmgr *tmgr,
		struct vertex_task *task)
{
	vertex_enter();
	task->state = VERTEX_TASK_STATE_PREPARE;
	list_add_tail(&task->list, &tmgr->pre_list);
	tmgr->pre_cnt++;
	vertex_leave();
}

static void __vertex_task_s_process(struct vertex_taskmgr *tmgr,
		struct vertex_task *task)
{
	vertex_enter();
	task->state = VERTEX_TASK_STATE_PROCESS;
	list_add_tail(&task->list, &tmgr->pro_list);
	tmgr->pro_cnt++;
	vertex_leave();
}

static void __vertex_task_s_complete(struct vertex_taskmgr *tmgr,
		struct vertex_task *task)
{
	vertex_enter();
	task->state = VERTEX_TASK_STATE_COMPLETE;
	list_add_tail(&task->list, &tmgr->com_list);
	tmgr->com_cnt++;
	vertex_leave();
}

void vertex_task_print_free_list(struct vertex_taskmgr *tmgr)
{
	struct vertex_task *task, *temp;
	int idx = 0;

	vertex_enter();
	vertex_dbg("fre(%d, %d)\n", tmgr->id, tmgr->fre_cnt);
	list_for_each_entry_safe(task, temp, &tmgr->fre_list, list) {
		vertex_dbg("fre[%d] %d\n", idx++, task->index);
	}
	vertex_leave();
}


void vertex_task_print_request_list(struct vertex_taskmgr *tmgr)
{
	struct vertex_task *task, *temp;
	int idx = 0;

	vertex_enter();
	vertex_dbg("req(%d, %d)\n", tmgr->id, tmgr->req_cnt);
	list_for_each_entry_safe(task, temp, &tmgr->req_list, list) {
		vertex_dbg("req[%d] %d\n", idx++, task->index);
	}
	vertex_leave();
}

void vertex_task_print_prepare_list(struct vertex_taskmgr *tmgr)
{
	struct vertex_task *task, *temp;
	int idx = 0;

	vertex_enter();
	vertex_dbg("pre(%d, %d)\n", tmgr->id, tmgr->pre_cnt);
	list_for_each_entry_safe(task, temp, &tmgr->pre_list, list) {
		vertex_dbg("pre[%d] %d\n", idx++, task->index);
	}
	vertex_leave();
}

void vertex_task_print_process_list(struct vertex_taskmgr *tmgr)
{
	struct vertex_task *task, *temp;
	int idx = 0;

	vertex_enter();
	vertex_dbg("pro(%d, %d)\n", tmgr->id, tmgr->pro_cnt);
	list_for_each_entry_safe(task, temp, &tmgr->pro_list, list) {
		vertex_dbg("pro[%d] %d\n", idx++, task->index);
	}
	vertex_leave();
}

void vertex_task_print_complete_list(struct vertex_taskmgr *tmgr)
{
	struct vertex_task *task, *temp;
	int idx = 0;

	vertex_enter();
	vertex_dbg("com(%d, %d)\n", tmgr->id, tmgr->com_cnt);
	list_for_each_entry_safe(task, temp, &tmgr->com_list, list) {
		vertex_dbg("com[%d] %d\n", idx++, task->index);
	}
	vertex_leave();
}

void vertex_task_print_all(struct vertex_taskmgr *tmgr)
{
	vertex_enter();
	vertex_task_print_free_list(tmgr);
	vertex_task_print_request_list(tmgr);
	vertex_task_print_prepare_list(tmgr);
	vertex_task_print_process_list(tmgr);
	vertex_task_print_complete_list(tmgr);
	vertex_leave();
}

static int __vertex_task_check_free(struct vertex_taskmgr *tmgr,
		struct vertex_task *task)
{
	if (!tmgr->fre_cnt) {
		vertex_warn("fre_cnt is zero\n");
		return -EINVAL;
	}

	if (task->state != VERTEX_TASK_STATE_FREE) {
		vertex_warn("state(%x) is invlid\n", task->state);
		return -EINVAL;
	}
	return 0;
}

static int __vertex_task_check_request(struct vertex_taskmgr *tmgr,
		struct vertex_task *task)
{
	if (!tmgr->req_cnt) {
		vertex_warn("req_cnt is zero\n");
		return -EINVAL;
	}

	if (task->state != VERTEX_TASK_STATE_REQUEST) {
		vertex_warn("state(%x) is invlid\n", task->state);
		return -EINVAL;
	}
	return 0;
}

static int __vertex_task_check_prepare(struct vertex_taskmgr *tmgr,
		struct vertex_task *task)
{
	if (!tmgr->pre_cnt) {
		vertex_warn("pre_cnt is zero\n");
		return -EINVAL;
	}

	if (task->state != VERTEX_TASK_STATE_PREPARE) {
		vertex_warn("state(%x) is invlid\n", task->state);
		return -EINVAL;
	}
	return 0;
}

static int __vertex_task_check_process(struct vertex_taskmgr *tmgr,
		struct vertex_task *task)
{
	if (!tmgr->pro_cnt) {
		vertex_warn("pro_cnt is zero\n");
		return -EINVAL;
	}

	if (task->state != VERTEX_TASK_STATE_PROCESS) {
		vertex_warn("state(%x) is invlid\n", task->state);
		return -EINVAL;
	}
	return 0;
}

static int __vertex_task_check_complete(struct vertex_taskmgr *tmgr,
		struct vertex_task *task)
{
	if (!tmgr->com_cnt) {
		vertex_warn("com_cnt is zero\n");
		return -EINVAL;
	}

	if (task->state != VERTEX_TASK_STATE_COMPLETE) {
		vertex_warn("state(%x) is invlid\n", task->state);
		return -EINVAL;
	}
	return 0;
}

void vertex_task_trans_fre_to_req(struct vertex_taskmgr *tmgr,
		struct vertex_task *task)
{
	vertex_enter();
	if (__vertex_task_check_free(tmgr, task))
		return;

	list_del(&task->list);
	tmgr->fre_cnt--;
	__vertex_task_s_request(tmgr, task);
	vertex_leave();
}

void vertex_task_trans_req_to_pre(struct vertex_taskmgr *tmgr,
		struct vertex_task *task)
{
	vertex_enter();
	if (__vertex_task_check_request(tmgr, task))
		return;

	list_del(&task->list);
	tmgr->req_cnt--;
	__vertex_task_s_prepare(tmgr, task);
	vertex_leave();
}

void vertex_task_trans_req_to_pro(struct vertex_taskmgr *tmgr,
		struct vertex_task *task)
{
	vertex_enter();
	if (__vertex_task_check_request(tmgr, task))
		return;

	list_del(&task->list);
	tmgr->req_cnt--;
	__vertex_task_s_process(tmgr, task);
	vertex_leave();
}

void vertex_task_trans_req_to_com(struct vertex_taskmgr *tmgr,
		struct vertex_task *task)
{
	vertex_enter();
	if (__vertex_task_check_request(tmgr, task))
		return;

	list_del(&task->list);
	tmgr->req_cnt--;
	__vertex_task_s_complete(tmgr, task);
	vertex_leave();
}

void vertex_task_trans_req_to_fre(struct vertex_taskmgr *tmgr,
		struct vertex_task *task)
{
	vertex_enter();
	if (__vertex_task_check_request(tmgr, task))
		return;

	list_del(&task->list);
	tmgr->req_cnt--;
	__vertex_task_s_free(tmgr, task);
	vertex_leave();
}

void vertex_task_trans_pre_to_pro(struct vertex_taskmgr *tmgr,
		struct vertex_task *task)
{
	vertex_enter();
	if (__vertex_task_check_prepare(tmgr, task))
		return;

	list_del(&task->list);
	tmgr->pre_cnt--;
	__vertex_task_s_process(tmgr, task);
	vertex_leave();
}

void vertex_task_trans_pre_to_com(struct vertex_taskmgr *tmgr,
		struct vertex_task *task)
{
	vertex_enter();
	if (__vertex_task_check_prepare(tmgr, task))
		return;

	list_del(&task->list);
	tmgr->pre_cnt--;
	__vertex_task_s_complete(tmgr, task);
	vertex_leave();
}

void vertex_task_trans_pre_to_fre(struct vertex_taskmgr *tmgr,
		struct vertex_task *task)
{
	vertex_enter();
	if (__vertex_task_check_prepare(tmgr, task))
		return;

	list_del(&task->list);
	tmgr->pre_cnt--;
	__vertex_task_s_free(tmgr, task);
	vertex_leave();
}

void vertex_task_trans_pro_to_com(struct vertex_taskmgr *tmgr,
		struct vertex_task *task)
{
	vertex_enter();
	if (__vertex_task_check_process(tmgr, task))
		return;

	list_del(&task->list);
	tmgr->pro_cnt--;
	__vertex_task_s_complete(tmgr, task);
	vertex_leave();
}

void vertex_task_trans_pro_to_fre(struct vertex_taskmgr *tmgr,
		struct vertex_task *task)
{
	vertex_enter();
	if (__vertex_task_check_process(tmgr, task))
		return;

	list_del(&task->list);
	tmgr->pro_cnt--;
	__vertex_task_s_free(tmgr, task);
	vertex_leave();
}

void vertex_task_trans_com_to_fre(struct vertex_taskmgr *tmgr,
		struct vertex_task *task)
{
	vertex_enter();
	if (__vertex_task_check_complete(tmgr, task))
		return;

	list_del(&task->list);
	tmgr->com_cnt--;
	__vertex_task_s_free(tmgr, task);
	vertex_leave();
}

void vertex_task_trans_any_to_fre(struct vertex_taskmgr *tmgr,
		struct vertex_task *task)
{
	vertex_enter();

	list_del(&task->list);
	switch (task->state) {
	case VERTEX_TASK_STATE_REQUEST:
		tmgr->req_cnt--;
		break;
	case VERTEX_TASK_STATE_PREPARE:
		tmgr->pre_cnt--;
		break;
	case VERTEX_TASK_STATE_PROCESS:
		tmgr->pro_cnt--;
		break;
	case VERTEX_TASK_STATE_COMPLETE:
		tmgr->com_cnt--;
		break;
	default:
		vertex_err("state(%d) of task is invalid\n", task->state);
		return;
	}

	__vertex_task_s_free(tmgr, task);
	vertex_leave();
}

struct vertex_task *vertex_task_pick_fre_to_req(struct vertex_taskmgr *tmgr)
{
	struct vertex_task *task;

	vertex_enter();
	task = __vertex_task_get_first_free(tmgr);
	if (task)
		vertex_task_trans_fre_to_req(tmgr, task);
	vertex_leave();
	return task;
}

int vertex_task_init(struct vertex_taskmgr *tmgr, unsigned int id, void *owner)
{
	unsigned long flags;
	unsigned int index;

	vertex_enter();
	spin_lock_irqsave(&tmgr->slock, flags);
	tmgr->id = id;
	tmgr->sindex = 0;

	tmgr->tot_cnt = VERTEX_MAX_TASK;
	tmgr->fre_cnt = 0;
	tmgr->req_cnt = 0;
	tmgr->pre_cnt = 0;
	tmgr->pro_cnt = 0;
	tmgr->com_cnt = 0;

	INIT_LIST_HEAD(&tmgr->fre_list);
	INIT_LIST_HEAD(&tmgr->req_list);
	INIT_LIST_HEAD(&tmgr->pre_list);
	INIT_LIST_HEAD(&tmgr->pro_list);
	INIT_LIST_HEAD(&tmgr->com_list);

	/*
	 * task index 0 means invalid
	 * because firmware can't accept 0 invocation id
	 */
	for (index = 1; index < tmgr->tot_cnt; ++index) {
		tmgr->task[index].index = index;
		tmgr->task[index].owner = owner;
		tmgr->task[index].findex = VERTEX_MAX_TASK;
		tmgr->task[index].tdindex = VERTEX_MAX_TASKDESC;
		__vertex_task_s_free(tmgr, &tmgr->task[index]);
	}

	spin_unlock_irqrestore(&tmgr->slock, flags);

	vertex_leave();
	return 0;
}

int vertex_task_deinit(struct vertex_taskmgr *tmgr)
{
	unsigned long flags;
	unsigned int index;

	vertex_enter();
	spin_lock_irqsave(&tmgr->slock, flags);

	for (index = 0; index < tmgr->tot_cnt; ++index)
		__vertex_task_s_free(tmgr, &tmgr->task[index]);

	spin_unlock_irqrestore(&tmgr->slock, flags);

	vertex_leave();
	return 0;
}

void vertex_task_flush(struct vertex_taskmgr *tmgr)
{
	unsigned long flags;
	struct vertex_task *task, *temp;

	vertex_enter();
	spin_lock_irqsave(&tmgr->slock, flags);

	list_for_each_entry_safe(task, temp, &tmgr->req_list, list) {
		vertex_task_trans_req_to_fre(tmgr, task);
		vertex_warn("req task(%d) is flushed(count:%d)",
				task->id, tmgr->req_cnt);
	}

	list_for_each_entry_safe(task, temp, &tmgr->pre_list, list) {
		vertex_task_trans_pre_to_fre(tmgr, task);
		vertex_warn("pre task(%d) is flushed(count:%d)",
				task->id, tmgr->pre_cnt);
	}

	list_for_each_entry_safe(task, temp, &tmgr->pro_list, list) {
		vertex_task_trans_pro_to_fre(tmgr, task);
		vertex_warn("pro task(%d) is flushed(count:%d)",
				task->id, tmgr->pro_cnt);
	}

	list_for_each_entry_safe(task, temp, &tmgr->com_list, list) {
		vertex_task_trans_com_to_fre(tmgr, task);
		vertex_warn("com task(%d) is flushed(count:%d)",
				task->id, tmgr->com_cnt);
	}

	spin_unlock_irqrestore(&tmgr->slock, flags);

	if (tmgr->fre_cnt != (tmgr->tot_cnt - 1))
		vertex_warn("free count leaked (%d/%d/%d/%d/%d/%d)\n",
				tmgr->fre_cnt, tmgr->req_cnt,
				tmgr->pre_cnt, tmgr->pro_cnt,
				tmgr->com_cnt, tmgr->tot_cnt);
	vertex_leave();
}
