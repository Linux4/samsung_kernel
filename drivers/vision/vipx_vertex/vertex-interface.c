/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/interrupt.h>

#include "vertex-log.h"
#include "vertex-device.h"
#include "vertex-binary.h"
#include "vertex-queue.h"
#include "vertex-graphmgr.h"
#include "vertex-memory.h"
#include "vertex-mailbox.h"
#include "vertex-message.h"
#include "vertex-system.h"
#include "vertex-interface.h"

#define enter_request_barrier(task)	mutex_lock(task->lock)
#define exit_request_barrier(task)	mutex_unlock(task->lock)
#define enter_process_barrier(itf)	spin_lock_irq(&itf->process_barrier)
#define exit_process_barrier(itf)	spin_unlock_irq(&itf->process_barrier)

/* TODO check */
//#define NON_BLOCK_INVOKE

static inline void __vertex_interface_set_reply(struct vertex_interface *itf,
		unsigned int gidx)
{
	vertex_enter();
	itf->reply[gidx].valid = 1;
	wake_up(&itf->reply_wq);
	vertex_leave();
}

static inline void __vertex_interface_clear_reply(struct vertex_interface *itf,
		unsigned int gidx)
{
	vertex_enter();
	itf->reply[gidx].valid = 0;
	vertex_leave();
}

static int __vertex_interface_wait_reply(struct vertex_interface *itf,
		unsigned int gidx)
{
	int ret;
	unsigned int cmd;
	unsigned long type;
	long wait_time, timeout;

	vertex_enter();
	cmd = itf->request[gidx]->message;
	type = itf->request[gidx]->param3;

#ifdef NON_BLOCK_INVOKE
	wait_time = VERTEX_COMMAND_TIMEOUT;
#else
	wait_time = VERTEX_COMMAND_TIMEOUT * 2;
#endif
	timeout = wait_event_timeout(itf->reply_wq, itf->reply[gidx].valid,
			wait_time);
	if (!timeout) {
		vertex_debug_write_log_binary();
		vertex_debug_dump_debug_regs();
		ret = -ETIMEDOUT;
		vertex_err("wait time(%u ms) is expired (%u/%u/%lu)\n",
				jiffies_to_msecs(wait_time), gidx, cmd, type);
		goto p_err;
	}

	vertex_leave();
	return 0;
p_err:
	return ret;
}

static void __vertex_interface_send_interrupt(struct vertex_interface *itf)
{
#ifndef VERTEX_MBOX_EMULATOR
	unsigned long type;
	unsigned int offset;
	int try_count = 100;
	unsigned int val;

	vertex_enter();
	type = itf->process->param3;
	offset = (type == VERTEX_MTYPE_H2F_NORMAL) ? 0x10 : 0x0C;

	/* Check interrupt clear */
	while (try_count) {
		val = readl(itf->regs + offset);
		if (!val)
			break;

		vertex_warn("interrupt(0x%x) is not yet clear (%u)\n",
				val, try_count);
		udelay(1);
	}

	/* Raise interrupt */
	if (!val)
		writel(0x100, itf->regs + offset);
	else
		vertex_err("failed to send interrupt(0x%x)\n", val);

	vertex_leave();
#endif
}

static int __vertex_interface_send_command(struct vertex_interface *itf,
	struct vertex_task *itask)
{
	int ret;
	struct vertex_taskmgr *itaskmgr;
	struct vertex_mailbox_ctrl *mctrl;
	unsigned int cmd;
	void *msg;
	unsigned long gidx, size, type;
	unsigned int cid;
	unsigned long flags;

	vertex_enter();
	itaskmgr = &itf->taskmgr;
	mctrl = itf->mctrl;

	cmd = itask->message;
	msg = (void *)itask->param0;
	gidx = itask->param1;
	size = itask->param2;
	type = itask->param3;
	cid = itask->index;

	enter_request_barrier(itask);
	itf->request[gidx] = itask;

	enter_process_barrier(itf);
	itf->process = itask;

	ret = vertex_mbox_check_full(mctrl, type, VERTEX_MAILBOX_WAIT);
	if (ret) {
		vertex_err("mbox is full (%lu/%u/%u)\n", gidx, cmd, cid);
		goto p_err_process;
	}

	taskmgr_e_barrier_irqs(itaskmgr, 0, flags);
	vertex_task_trans_req_to_pro(itaskmgr, itask);
	taskmgr_x_barrier_irqr(itaskmgr, 0, flags);

	ret = vertex_mbox_write(mctrl, type, msg, size);
	if (ret) {
		vertex_err("writing to mbox is failed (%lu/%u/%u)\n",
				gidx, cmd, cid);
		goto p_err_process;
	}

	__vertex_interface_send_interrupt(itf);
	itf->process = NULL;
	exit_process_barrier(itf);

	ret = __vertex_interface_wait_reply(itf, gidx);
	if (ret) {
		vertex_err("No reply (%lu/%u/%u)\n", gidx, cmd, cid);
		goto p_err_request;
	}

	__vertex_interface_clear_reply(itf, gidx);

	if (itf->reply[gidx].param1) {
		ret = itf->reply[gidx].param1;
		vertex_err("Reply(%d) is error (%lu/%u/%u)\n",
				ret, gidx, cmd, cid);
		goto p_err_request;
	}

	itf->request[gidx] = NULL;
	exit_request_barrier(itask);

	vertex_leave();
	return 0;
p_err_process:
	itf->process = NULL;
	exit_process_barrier(itf);
p_err_request:
	itf->request[gidx] = NULL;
	exit_request_barrier(itask);
	return ret;
}

#ifdef NON_BLOCK_INVOKE
static int __vertex_interface_send_command_nonblocking(
		struct vertex_interface *itf, struct vertex_task *itask)
{
	int ret;
	struct vertex_taskmgr *itaskmgr;
	struct vertex_mailbox_ctrl *mctrl;
	unsigned int cmd;
	void *msg;
	unsigned long gidx, size, type;
	unsigned int cid;
	unsigned long flags;

	vertex_enter();
	itaskmgr = &itf->taskmgr;
	mctrl = itf->mctrl;

	cmd = itask->message;
	msg = (void *)itask->param0;
	gidx = itask->param1;
	size = itask->param2;
	type = itask->param3;
	cid = itask->index;

	enter_process_barrier(itf);
	itf->process = itask;

	ret = vertex_mbox_check_full(mctrl, type, VERTEX_MAILBOX_WAIT);
	if (ret) {
		vertex_err("mbox is full (%lu/%u/%u)\n", gidx, cmd, cid);
		goto p_err_process;
	}

	taskmgr_e_barrier_irqs(itaskmgr, 0, flags);
	vertex_task_trans_req_to_pro(itaskmgr, itask);
	taskmgr_x_barrier_irqr(itaskmgr, 0, flags);

	ret = vertex_mbox_write(mctrl, type, msg, size);
	if (ret) {
		vertex_err("writing to mbox is failed (%lu/%u/%u)\n",
				gidx, cmd, cid);
		goto p_err_process;
	}

	__vertex_interface_send_interrupt(itf);
	itf->process = NULL;
	exit_process_barrier(itf);

	vertex_leave();
	return 0;
p_err_process:
	itf->process = NULL;
	exit_process_barrier(itf);
	return ret;
}
#endif

int vertex_hw_wait_bootup(struct vertex_interface *itf)
{
	int ret;
	int try_cnt = 1000;

	vertex_enter();
	while (!test_bit(VERTEX_ITF_STATE_BOOTUP, &itf->state)) {
		if (!try_cnt)
			break;

		udelay(100);
		try_cnt--;
	}

	if (!test_bit(VERTEX_ITF_STATE_BOOTUP, &itf->state)) {
		vertex_debug_write_log_binary();
		vertex_debug_dump_debug_regs();
		ret = -ETIMEDOUT;
		vertex_err("Failed to boot CM7 (%d)\n", ret);
		goto p_err;
	}

	vertex_leave();
	return 0;
p_err:
	return ret;
}

static int __vertex_interface_get_size_format(struct vertex_format *format,
		unsigned int plane, unsigned int *width, unsigned int *height)
{
	int ret;

	vertex_enter();
	switch (format->format) {
	case VS4L_DF_IMAGE_U8:
	case VS4L_DF_IMAGE_U16:
	case VS4L_DF_IMAGE_RGB:
	case VS4L_DF_IMAGE_U32:
		*width = format->width;
		*height = format->height;
		break;
	case VS4L_DF_IMAGE_NV21:
		if (plane == 0) {
			*width = format->width;
			*height = format->height;
		} else if (plane == 1) {
			*width = format->width;
			*height = format->height / 2;
		} else {
			vertex_err("Invalid plane(%d) for NV21\n", plane);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case VS4L_DF_IMAGE_YV12:
	case VS4L_DF_IMAGE_I420:
		if (plane == 0) {
			*width = format->width;
			*height = format->height;
		} else if ((plane == 1) || (plane == 2)) {
			*width = format->width / 2;
			*height = format->height / 2;
		} else {
			vertex_err("Invalid plane(%d) for YV12/I420\n", plane);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case VS4L_DF_IMAGE_I422:
		if (plane == 0) {
			*width = format->width;
			*height = format->height;
		} else if ((plane == 1) || (plane == 2)) {
			*width = format->width / 2;
			*height = format->height;
		} else {
			vertex_err("Invalid plane(%d) for I422\n", plane);
			ret = -EINVAL;
		}
		break;
	default:
		vertex_err("invalid format (%u)\n", format->format);
		ret = -EINVAL;
		goto p_err;
	}

	vertex_leave();
	return 0;
p_err:
	return ret;
}

int vertex_hw_config(struct vertex_interface *itf, struct vertex_task *itask)
{
	int ret;
	struct vertex_taskmgr *itaskmgr;
	struct vertex_format_list *incl;
	struct vertex_format_list *otcl;
	struct vertex_message msg;
	struct vertex_set_graph_req *req;
	unsigned int lcnt, fcnt;
	unsigned int width, height;
	int num_inputs, num_outputs;
	size_t size;
	dma_addr_t heap;
	unsigned long flags;

	vertex_enter();
	itaskmgr = &itf->taskmgr;
	incl = (struct vertex_format_list *)itask->param2;
	otcl = (struct vertex_format_list *)itask->param3;

	msg.trans_id = itask->index;
	msg.type = SET_GRAPH_REQ;

	req = &msg.payload.set_graph_req;
	req->graph_id = itask->param0;

	/* Allocate and assign heap */
	if (req->graph_id < SCENARIO_DE_CAPTURE) {
		size = VERTEX_CM7_HEAP_SIZE_PREVIEW;
	} else if (req->graph_id == SCENARIO_ENF ||
			req->graph_id == SCENARIO_ENF_UV ||
			req->graph_id == SCENARIO_ENF_YUV) {
		size = VERTEX_CM7_HEAP_SIZE_ENF;
	} else {
		size = VERTEX_CM7_HEAP_SIZE_CAPTURE;
	}
	heap = vertex_memory_allocate_heap(&itf->system->memory, req->graph_id,
			size);
	if (IS_ERR_VALUE(heap)) {
		ret = (int)heap;
		goto p_err_heap;
	}

	req->p_temp = heap;
	req->sz_temp = size;

	/* TODO : How to update depth field */
	for (lcnt = 0, num_inputs = 0; lcnt < incl->count; ++lcnt) {
		for (fcnt = 0; fcnt < incl->formats[lcnt].plane; ++fcnt) {
			ret = __vertex_interface_get_size_format(
					&incl->formats[lcnt], fcnt,
					&width, &height);
			if (ret)
				goto p_err_incl;

			req->input_width[num_inputs] = width;
			req->input_height[num_inputs] = height;
			req->input_depth[num_inputs] = 0;
			num_inputs++;
		}
	}
	req->num_inputs = num_inputs;

	for (lcnt = 0, num_outputs = 0; lcnt < otcl->count; ++lcnt) {
		for (fcnt = 0; fcnt < otcl->formats[lcnt].plane; ++fcnt) {
			ret = __vertex_interface_get_size_format(
					&otcl->formats[lcnt], fcnt,
					&width, &height);
			if (ret)
				goto p_err_otcl;

			req->output_width[num_outputs] = width;
			req->output_height[num_outputs] = height;
			req->output_depth[num_outputs] = 0;
			num_outputs++;
		}
	}
	req->num_outputs = num_outputs;

	itask->param0 = (unsigned long)&msg;
	itask->param2 = sizeof(msg);
	itask->param3 = VERTEX_MTYPE_H2F_NORMAL;

	ret = __vertex_interface_send_command(itf, itask);
	if (ret) {
		vertex_err("Failed to send command (%d)\n", ret);
		goto p_err_send;
	}

	taskmgr_e_barrier_irqs(itaskmgr, 0, flags);
	vertex_task_trans_pro_to_fre(itaskmgr, itask);
	taskmgr_x_barrier_irqr(itaskmgr, 0, flags);

	vertex_leave();
	return 0;
p_err_send:
p_err_otcl:
p_err_incl:
	vertex_memory_free_heap(&itf->system->memory);
p_err_heap:
	taskmgr_e_barrier_irqs(itaskmgr, 0, flags);
	vertex_task_trans_any_to_fre(itaskmgr, itask);
	taskmgr_x_barrier_irqr(itaskmgr, 0, flags);
	return ret;
}

int vertex_hw_process(struct vertex_interface *itf, struct vertex_task *itask)
{
	int ret;
	struct vertex_taskmgr *itaskmgr;
	struct vertex_container_list *incl;
	struct vertex_container_list *otcl;
	struct vertex_message msg;
	struct vertex_invoke_graph_req *req;
	unsigned int lcnt, fcnt, count;
	int num_inputs, num_outputs;
	unsigned long flags;

	vertex_enter();
	itaskmgr = &itf->taskmgr;
	incl = itask->incl;
	otcl = itask->otcl;

	msg.trans_id = itask->index;
	msg.type = INVOKE_GRAPH_REQ;

	req = &msg.payload.invoke_graph_req;
	req->graph_id = itask->param0;

	/* TODO : We need to consider to support multi plain buffer */
	for (lcnt = 0, num_inputs = 0; lcnt < incl->count; ++lcnt) {
		for (fcnt = 0; fcnt < incl->containers[lcnt].count; ++fcnt) {
			req->p_input[num_inputs] =
				incl->containers[lcnt].buffers[fcnt].dvaddr;
			num_inputs++;
		}
	}
	req->num_inputs = num_inputs;

	for (lcnt = 0, num_outputs = 0; lcnt < otcl->count; ++lcnt) {
		for (fcnt = 0; fcnt < otcl->containers[lcnt].count; ++fcnt) {
			req->p_output[num_outputs] =
				otcl->containers[lcnt].buffers[fcnt].dvaddr;
			num_outputs++;
		}
	}
	req->num_outputs = num_outputs;

	for (count = 0; count < MAX_NUM_OF_USER_PARAMS; ++count)
		req->user_params[count] = incl->user_params[count];

	itask->param0 = (unsigned long)&msg;
	itask->param2 = sizeof(msg);
	itask->param3 = VERTEX_MTYPE_H2F_NORMAL;

#ifdef NON_BLOCK_INVOKE
	ret = __vertex_interface_send_command_nonblocking(itf, itask);
#else
	ret = __vertex_interface_send_command(itf, itask);
#endif
	if (ret)
		goto p_err;

	vertex_leave();
	return 0;
p_err:
	taskmgr_e_barrier_irqs(itaskmgr, 0, flags);
	vertex_task_trans_any_to_fre(itaskmgr, itask);
	taskmgr_x_barrier_irqr(itaskmgr, 0, flags);
	return ret;
}

int vertex_hw_create(struct vertex_interface *itf, struct vertex_task *itask)
{
	int ret;
	struct vertex_taskmgr *itaskmgr;
	struct vertex_message msg;
	struct vertex_create_graph_req *req;
	unsigned long flags;

	vertex_enter();
	itaskmgr = &itf->taskmgr;

	msg.trans_id = itask->index;
	msg.type = CREATE_GRAPH_REQ;

	req = &msg.payload.create_graph_req;
	req->p_graph = 0;
	req->sz_graph = 0;

	itask->param0 = (unsigned long)&msg;
	itask->param2 = sizeof(msg);
	itask->param3 = VERTEX_MTYPE_H2F_NORMAL;

	ret = __vertex_interface_send_command(itf, itask);
	if (ret)
		goto p_err;

	taskmgr_e_barrier_irqs(itaskmgr, 0, flags);
	vertex_task_trans_pro_to_fre(itaskmgr, itask);
	taskmgr_x_barrier_irqr(itaskmgr, 0, flags);

	vertex_leave();
	return 0;
p_err:
	taskmgr_e_barrier_irqs(itaskmgr, 0, flags);
	vertex_task_trans_any_to_fre(itaskmgr, itask);
	taskmgr_x_barrier_irqr(itaskmgr, 0, flags);
	return ret;
}

int vertex_hw_destroy(struct vertex_interface *itf, struct vertex_task *itask)
{
	int ret;
	struct vertex_taskmgr *itaskmgr;
	struct vertex_message msg;
	struct vertex_destroy_graph_req *req;
	unsigned long flags;

	vertex_enter();
	itaskmgr = &itf->taskmgr;

	msg.trans_id = itask->index;
	msg.type = DESTROY_GRAPH_REQ;

	req = &msg.payload.destroy_graph_req;
	req->graph_id = itask->param0;

	itask->param0 = (unsigned long)&msg;
	itask->param2 = sizeof(msg);
	itask->param3 = VERTEX_MTYPE_H2F_NORMAL;

	ret = __vertex_interface_send_command(itf, itask);
	if (ret)
		goto p_err;

	taskmgr_e_barrier_irqs(itaskmgr, 0, flags);
	vertex_task_trans_pro_to_fre(itaskmgr, itask);
	taskmgr_x_barrier_irqr(itaskmgr, 0, flags);

	vertex_memory_free_heap(&itf->system->memory);

	vertex_leave();
	return 0;
p_err:
	taskmgr_e_barrier_irqs(itaskmgr, 0, flags);
	vertex_task_trans_any_to_fre(itaskmgr, itask);
	taskmgr_x_barrier_irqr(itaskmgr, 0, flags);

	vertex_memory_free_heap(&itf->system->memory);
	return ret;
}

int vertex_hw_init(struct vertex_interface *itf, struct vertex_task *itask)
{
	int ret;
	struct vertex_taskmgr *itaskmgr;
	struct vertex_message msg;
	struct vertex_init_req *req;
	unsigned long flags;

	vertex_enter();
	itaskmgr = &itf->taskmgr;

	msg.trans_id = itask->index;
	msg.type = INIT_REQ;

	req = &msg.payload.init_req;
	req->p_cc_heap = 0;
	req->sz_cc_heap = 0;

	itask->param0 = (unsigned long)&msg;
	itask->param2 = sizeof(msg);
	itask->param3 = VERTEX_MTYPE_H2F_URGENT;

	ret = __vertex_interface_send_command(itf, itask);
	if (ret)
		goto p_err;

	taskmgr_e_barrier_irqs(itaskmgr, 0, flags);
	vertex_task_trans_pro_to_fre(itaskmgr, itask);
	taskmgr_x_barrier_irqr(itaskmgr, 0, flags);

	vertex_leave();
	return 0;
p_err:
	taskmgr_e_barrier_irqs(itaskmgr, 0, flags);
	vertex_task_trans_any_to_fre(itaskmgr, itask);
	taskmgr_x_barrier_irqr(itaskmgr, 0, flags);
	return ret;
}

int vertex_hw_deinit(struct vertex_interface *itf, struct vertex_task *itask)
{
	int ret;
	struct vertex_taskmgr *itaskmgr;
	struct vertex_message msg;
	struct vertex_powerdown_req *req;
	unsigned long flags;

	vertex_enter();
	itaskmgr = &itf->taskmgr;

	msg.trans_id = itask->index;
	msg.type = POWER_DOWN_REQ;

	req = &msg.payload.powerdown_req;
	req->valid = 1;

	itask->param0 = (unsigned long)&msg;
	itask->param2 = sizeof(msg);
	itask->param3 = VERTEX_MTYPE_H2F_URGENT;

	ret = __vertex_interface_send_command(itf, itask);
	if (ret)
		goto p_err;

	taskmgr_e_barrier_irqs(itaskmgr, 0, flags);
	vertex_task_trans_pro_to_fre(itaskmgr, itask);
	taskmgr_x_barrier_irqr(itaskmgr, 0, flags);

	vertex_leave();
	return 0;
p_err:
	taskmgr_e_barrier_irqs(itaskmgr, 0, flags);
	vertex_task_trans_any_to_fre(itaskmgr, itask);
	taskmgr_x_barrier_irqr(itaskmgr, 0, flags);
	return ret;
}

static void __vertex_interface_work_list_queue_free(
		struct vertex_work_list *work_list, struct vertex_work *work)
{
	unsigned long flags;

	vertex_enter();
	spin_lock_irqsave(&work_list->slock, flags);
	list_add_tail(&work->list, &work_list->free_head);
	work_list->free_cnt++;
	spin_unlock_irqrestore(&work_list->slock, flags);
	vertex_leave();
}

static struct vertex_work *__vertex_interface_work_list_dequeue_free(
		struct vertex_work_list *work_list)
{
	unsigned long flags;
	struct vertex_work *work;

	vertex_enter();
	spin_lock_irqsave(&work_list->slock, flags);
	if (work_list->free_cnt) {
		work = list_first_entry(&work_list->free_head,
				struct vertex_work, list);
		list_del(&work->list);
		work_list->free_cnt--;
	} else {
		work = NULL;
		vertex_err("free work list empty\n");
	}
	spin_unlock_irqrestore(&work_list->slock, flags);

	vertex_leave();
	return work;
}

static void __vertex_interface_work_list_queue_reply(
		struct vertex_work_list *work_list, struct vertex_work *work)
{
	unsigned long flags;

	vertex_enter();
	spin_lock_irqsave(&work_list->slock, flags);
	list_add_tail(&work->list, &work_list->reply_head);
	work_list->reply_cnt++;
	spin_unlock_irqrestore(&work_list->slock, flags);
	vertex_leave();
}

static struct vertex_work *__vertex_interface_work_list_dequeue_reply(
		struct vertex_work_list *work_list)
{
	unsigned long flags;
	struct vertex_work *work;

	vertex_enter();
	spin_lock_irqsave(&work_list->slock, flags);
	if (work_list->reply_cnt) {
		work = list_first_entry(&work_list->reply_head,
				struct vertex_work, list);
		list_del(&work->list);
		work_list->reply_cnt--;
	} else {
		work = NULL;
	}
	spin_unlock_irqrestore(&work_list->slock, flags);

	vertex_leave();
	return work;
}

static void __vertex_interface_work_list_init(
		struct vertex_work_list *work_list, unsigned int count)
{
	unsigned int idx;

	vertex_enter();
	spin_lock_init(&work_list->slock);
	INIT_LIST_HEAD(&work_list->free_head);
	work_list->free_cnt = 0;
	INIT_LIST_HEAD(&work_list->reply_head);
	work_list->reply_cnt = 0;

	for (idx = 0; idx < count; ++idx)
		__vertex_interface_work_list_queue_free(work_list,
				&work_list->work[idx]);

	vertex_leave();
}

static void __interface_isr(void *data)
{
	struct vertex_interface *itf;
	struct vertex_mailbox_ctrl *mctrl;
	struct vertex_work_list *work_list;
	struct work_struct *work_queue;
	struct vertex_work *work;
	struct vertex_message rsp;

	vertex_enter();
	itf = (struct vertex_interface *)data;
	mctrl = itf->mctrl;
	work_list = &itf->work_list;
	work_queue = &itf->work_queue;

	while (!vertex_mbox_check_reply(mctrl, VERTEX_MTYPE_F2H_URGENT, 0)) {
		vertex_mbox_read(mctrl, VERTEX_MTYPE_F2H_URGENT, &rsp);

		if (rsp.type == BOOTUP_RSP) {
			if (rsp.payload.bootup_rsp.error == 0) {
				vertex_dbg("CM7 bootup complete (%u)\n",
						rsp.trans_id);
				set_bit(VERTEX_ITF_STATE_BOOTUP, &itf->state);
			} else {
				/* TODO process boot fail */
				vertex_err("CM7 bootup failed (%u,%d)\n",
						rsp.trans_id,
						rsp.payload.bootup_rsp.error);
			}
			break;
		}

		work = __vertex_interface_work_list_dequeue_free(work_list);
		if (work) {
			/* TODO */
			work->id = rsp.trans_id;
			work->message = 0;
			work->param0 = 0;
			work->param1 = 0;
			work->param2 = 0;
			work->param3 = 0;

			__vertex_interface_work_list_queue_reply(work_list,
					work);

			if (!work_pending(work_queue))
				schedule_work(work_queue);
		}
	}

	while (!vertex_mbox_check_reply(mctrl, VERTEX_MTYPE_F2H_NORMAL, 0)) {
		vertex_mbox_read(mctrl, VERTEX_MTYPE_F2H_NORMAL, &rsp);

		if (rsp.type == TEST_RSP) {
			enter_process_barrier(itf);
			if (!itf->process) {
				vertex_err("process task is empty\n");
				exit_process_barrier(itf);
				continue;
			}

			vertex_dbg("test response %x\n",
					rsp.payload.test_rsp.error);
			itf->process->message = VERTEX_TASK_DONE;
			wake_up(&itf->reply_wq);
			exit_process_barrier(itf);
			continue;
		}

		work = __vertex_interface_work_list_dequeue_free(work_list);
		if (work) {
			/* TODO */
			work->id = rsp.trans_id;
			work->message = 0;
			work->param0 = 0;
			work->param1 = 0;
			work->param2 = 0;
			work->param3 = 0;

			__vertex_interface_work_list_queue_reply(work_list,
					work);

			if (!work_pending(work_queue))
				schedule_work(work_queue);
		}
	}

	vertex_leave();
}

irqreturn_t vertex_interface_isr0(int irq, void *data)
{
	vertex_enter();
	__interface_isr(data);
	vertex_leave();
	return IRQ_HANDLED;
}

irqreturn_t vertex_interface_isr1(int irq, void *data)
{
	vertex_enter();
	__interface_isr(data);
	vertex_leave();
	return IRQ_HANDLED;
}

#ifdef VERTEX_MBOX_EMULATOR
#define MBOX_TURN_AROUND_TIME		(HZ / 20)

extern int vertex_mbox_emul_handler(struct vertex_mailbox_ctrl *mctrl);

static void __mbox_timer(unsigned long data)
{
	struct vertex_interface *itf;
	struct vertex_mailbox_ctrl *mctrl;
	int ret;
	unsigned int random;

	vertex_enter();
	itf = (struct vertex_interface *)data;
	mctrl = itf->private_data;

	if (!test_bit(VERTEX_ITF_STATE_BOOTUP, &itf->state))
		goto exit;

	ret = vertex_emul_mbox_handler(mctrl);
	if (ret)
		__interface_isr(itf);

exit:
	get_random_bytes(&random, sizeof(random));
	random = random % MBOX_TURN_AROUND_TIME;
	mod_timer(&itf->timer, jiffies + random);
	vertex_leave();
}
#endif

int vertex_interface_start(struct vertex_interface *itf)
{
	vertex_enter();
#ifdef VERTEX_MBOX_EMULATOR
	init_timer(&itf->timer);
	itf->timer.expires = jiffies + MBOX_TURN_AROUND_TIME;
	itf->timer.data = (unsigned long)itf;
	itf->timer.function = __mbox_timer;
	add_timer(&itf->timer);
#endif
	set_bit(VERTEX_ITF_STATE_START, &itf->state);

	vertex_leave();
	return 0;
}

static void __vertex_interface_cleanup(struct vertex_interface *itf)
{
	struct vertex_taskmgr *taskmgr;
	int task_count;

	unsigned long flags;
	unsigned int idx;
	struct vertex_task *task;

	vertex_enter();
	taskmgr = &itf->taskmgr;

	task_count = taskmgr->req_cnt + taskmgr->pro_cnt + taskmgr->com_cnt;
	if (task_count) {
		vertex_warn("count of task manager is not zero (%u/%u/%u)\n",
				taskmgr->req_cnt, taskmgr->pro_cnt,
				taskmgr->com_cnt);
		/* TODO remove debug log */
		vertex_task_print_all(&itf->taskmgr);

		taskmgr_e_barrier_irqs(taskmgr, 0, flags);

		for (idx = 1; idx < taskmgr->tot_cnt; ++idx) {
			task = &taskmgr->task[idx];
			if (task->state == VERTEX_TASK_STATE_FREE)
				continue;

			vertex_task_trans_any_to_fre(taskmgr, task);
			task_count--;
		}

		taskmgr_x_barrier_irqr(taskmgr, 0, flags);
	}
}

int vertex_interface_stop(struct vertex_interface *itf)
{
	vertex_enter();
	__vertex_interface_cleanup(itf);
#ifdef VERTEX_MBOX_EMULATOR
	del_timer_sync(&itf->timer);
#endif
	clear_bit(VERTEX_ITF_STATE_BOOTUP, &itf->state);
	clear_bit(VERTEX_ITF_STATE_START, &itf->state);

	vertex_leave();
	return 0;
}

int vertex_interface_open(struct vertex_interface *itf, void *mbox)
{
	int idx;

	vertex_enter();
	itf->mctrl = mbox;

	itf->process = NULL;
	for (idx = 0; idx < VERTEX_MAX_GRAPH; ++idx) {
		itf->request[idx] = NULL;
		itf->reply[idx].valid = 0;
	}

	itf->done_cnt = 0;
	itf->state = 0;
	set_bit(VERTEX_ITF_STATE_OPEN, &itf->state);

	vertex_leave();
	return 0;
}

int vertex_interface_close(struct vertex_interface *itf)
{
	vertex_enter();
	itf->mctrl = NULL;
	clear_bit(VERTEX_ITF_STATE_OPEN, &itf->state);

	vertex_leave();
	return 0;
}

static void vertex_interface_work_reply_func(struct work_struct *data)
{
	struct vertex_interface *itf;
	struct vertex_taskmgr *itaskmgr;
	struct vertex_work_list *work_list;
	struct vertex_work *work;
	struct vertex_task *itask;
	unsigned int gidx;
	unsigned long flags;

	vertex_enter();
	itf = container_of(data, struct vertex_interface, work_queue);
	itaskmgr = &itf->taskmgr;
	work_list = &itf->work_list;

	while (true) {
		work = __vertex_interface_work_list_dequeue_reply(work_list);
		if (!work)
			break;

		if (work->id >= VERTEX_MAX_TASK) {
			vertex_err("work id is invalid (%d)\n", work->id);
			goto p_end;
		}

		itask = &itaskmgr->task[work->id];

		if (itask->state != VERTEX_TASK_STATE_PROCESS) {
			vertex_err("task(%u/%u/%lu) state(%u) is invalid\n",
					itask->message, itask->index,
					itask->param1, itask->state);
			vertex_err("work(%u/%u/%u)\n", work->id,
					work->param0, work->param1);
			vertex_task_print_all(&itf->taskmgr);
			goto p_end;
		}

		switch (itask->message) {
		case VERTEX_TASK_INIT:
		case VERTEX_TASK_DEINIT:
		case VERTEX_TASK_CREATE:
		case VERTEX_TASK_DESTROY:
		case VERTEX_TASK_ALLOCATE:
		case VERTEX_TASK_REQUEST:
			gidx = itask->param1;
			itf->reply[gidx] = *work;
			__vertex_interface_set_reply(itf, gidx);
			break;
		case VERTEX_TASK_PROCESS:
#ifndef NON_BLOCK_INVOKE
			gidx = itask->param1;
			itf->reply[gidx] = *work;
			__vertex_interface_set_reply(itf, gidx);
#endif
			taskmgr_e_barrier_irqs(itaskmgr, 0, flags);
			vertex_task_trans_pro_to_com(itaskmgr, itask);
			taskmgr_x_barrier_irqr(itaskmgr, 0, flags);

			itask->param2 = 0;
			itask->param3 = 0;
			vertex_graphmgr_queue(itf->cookie, itask);
			itf->done_cnt++;
			break;
		default:
			vertex_err("unresolved task message(%d) have arrived\n",
					work->message);
			break;
		}
p_end:
		__vertex_interface_work_list_queue_free(work_list, work);
	};
	vertex_leave();
}

/* TODO temp code */
void *vertex_interface_data;

int vertex_interface_probe(struct vertex_system *sys)
{
	int ret;
	struct platform_device *pdev;
	struct vertex_interface *itf;
	struct device *dev;
	struct vertex_taskmgr *taskmgr;

	vertex_enter();
	pdev = to_platform_device(sys->dev);
	itf = &sys->interface;
	itf->system = sys;
	dev = &pdev->dev;

	vertex_interface_data = itf;
	itf->regs = sys->reg_ss[VERTEX_REG_SS1];

	taskmgr = &itf->taskmgr;
	spin_lock_init(&taskmgr->slock);
	ret = vertex_task_init(taskmgr, VERTEX_MAX_GRAPH, itf);
	if (ret)
		goto p_err_taskmgr;

	itf->cookie = &sys->graphmgr;
	itf->state = 0;

	init_waitqueue_head(&itf->reply_wq);
	spin_lock_init(&itf->process_barrier);

	INIT_WORK(&itf->work_queue, vertex_interface_work_reply_func);
	__vertex_interface_work_list_init(&itf->work_list,
			VERTEX_WORK_MAX_COUNT);

	vertex_leave();
	return 0;
p_err_taskmgr:
	return ret;
}

void vertex_interface_remove(struct vertex_interface *itf)
{
	vertex_task_deinit(&itf->taskmgr);
}
