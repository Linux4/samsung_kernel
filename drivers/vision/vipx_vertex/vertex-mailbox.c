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

#include "vertex-log.h"
#include "vertex-io.h"
#include "vertex-mailbox.h"

static inline ssize_t __vertex_mbox_get_remain_size(
		struct vertex_mailbox_h2f *mbox)
{
	vertex_check();
	return MAX_MESSAGE_CNT - (mbox->wmsg_idx - mbox->rmsg_idx);
}

int vertex_mbox_check_full(struct vertex_mailbox_ctrl *mctrl, unsigned int type,
		int wait)
{
	int ret;
	struct vertex_mailbox_h2f *mbox;
	int try_count = 1000;
	ssize_t remain_size;

	vertex_enter();
	if (type == VERTEX_MTYPE_H2F_NORMAL) {
		mbox = &mctrl->stack.h2f;
	} else if (type == VERTEX_MTYPE_H2F_URGENT) {
		mbox = &mctrl->urgent_stack.h2f;
	} else {
		ret = -EINVAL;
		vertex_err("invalid mbox type(%u)\n", type);
		goto p_err;
	}

	while (try_count) {
		remain_size = __vertex_mbox_get_remain_size(mbox);
		if (remain_size > 0) {
			break;
		} else if (remain_size < 0) {
			ret = -EFAULT;
			vertex_err("index of mbox(%u) is invalid (%u/%u)\n",
					type, mbox->wmsg_idx, mbox->rmsg_idx);
			goto p_err;
		} else if (wait) {
			vertex_warn("mbox(%u) has no remain space (%u/%u/%d)\n",
					type, mbox->wmsg_idx, mbox->rmsg_idx,
					try_count);
			udelay(10);
			try_count--;
		} else {
			break;
		}
	}

	if (!remain_size) {
		ret = -EBUSY;
		vertex_err("mbox(%u) has no remain space (%u/%u)\n",
				type, mbox->wmsg_idx, mbox->rmsg_idx);
		goto p_err;
	}

	vertex_leave();
	return 0;
p_err:
	return ret;
}

int vertex_mbox_write(struct vertex_mailbox_ctrl *mctrl, unsigned int type,
		void *msg, size_t size)
{
	int ret;
	struct vertex_mailbox_h2f *mbox;
	void *wptr;
	struct vertex_message *debug;

	vertex_enter();
	if (size > sizeof(struct vertex_message)) {
		ret = -EINVAL;
		vertex_err("message size(%zu) for mbox(%u) is invalid\n",
				size, type);
		goto p_err;
	}

	if (type == VERTEX_MTYPE_H2F_NORMAL) {
		mbox = &mctrl->stack.h2f;
	} else if (type == VERTEX_MTYPE_H2F_URGENT) {
		mbox = &mctrl->urgent_stack.h2f;
	} else {
		ret = -EINVAL;
		vertex_err("invalid mbox type(%u)\n", type);
		goto p_err;
	}

	wptr = &mbox->msg[mbox->wmsg_idx % MAX_MESSAGE_CNT];
	vertex_io_copy_mem2io(wptr, msg, size);
	mbox->wmsg_idx++;

	debug = msg;
	vertex_dbg("[MBOX-W] cmd:%u/type:%u/wmsg_idx:%u/rmsg_idx:%u\n",
			debug->type, type, mbox->wmsg_idx, mbox->rmsg_idx);
	vertex_leave();
	return 0;
p_err:
	return ret;
}

static inline int __vertex_mbox_is_empty(struct vertex_mailbox_f2h *mbox)
{
	vertex_check();
	return mbox->wmsg_idx == mbox->rmsg_idx;
}

int vertex_mbox_check_reply(struct vertex_mailbox_ctrl *mctrl,
		unsigned int type, int wait)
{
	int ret;
	struct vertex_mailbox_f2h *mbox;
	int try_count = 1000;

	vertex_enter();
	if (type == VERTEX_MTYPE_F2H_NORMAL) {
		mbox = &mctrl->stack.f2h;
	} else if (type == VERTEX_MTYPE_F2H_URGENT) {
		mbox = &mctrl->urgent_stack.f2h;
	} else {
		ret = -EINVAL;
		vertex_err("invalid mbox type (%u)\n", type);
		goto p_err;
	}

	while (try_count) {
		ret = __vertex_mbox_is_empty(mbox);
		if (ret) {
			if (!wait)
				break;

			udelay(10);
			try_count--;
		} else {
			break;
		}
	}

	if (ret) {
		ret = -EFAULT;
		goto p_err;
	}

	vertex_leave();
	return 0;
p_err:
	return ret;
}

int vertex_mbox_read(struct vertex_mailbox_ctrl *mctrl, unsigned int type,
		void *msg)
{
	int ret;
	struct vertex_mailbox_f2h *mbox;
	void *rptr;
	struct vertex_message *debug;

	vertex_enter();
	if (type == VERTEX_MTYPE_F2H_NORMAL) {
		mbox = &mctrl->stack.f2h;
	} else if (type == VERTEX_MTYPE_F2H_URGENT) {
		mbox = &mctrl->urgent_stack.f2h;
	} else {
		ret = -EINVAL;
		vertex_err("invalid mbox type(%u)\n", type);
		goto p_err;
	}

	rptr = &mbox->msg[mbox->rmsg_idx % MAX_MESSAGE_CNT];
	vertex_io_copy_io2mem(msg, rptr, sizeof(struct vertex_message));
	mbox->rmsg_idx++;

	debug = rptr;
	vertex_dbg("[MBOX-R] cmd:%u/type:%u/wmsg_idx:%u/rmsg_idx:%u\n",
			debug->type, type, mbox->wmsg_idx, mbox->rmsg_idx);
	vertex_leave();
	return 0;
p_err:
	return ret;
}

#ifdef VERTEX_MBOX_EMULATOR
static inline ssize_t __vertex_mbox_emul_get_remain_size(
		struct vertex_mailbox_f2h *mbox)
{
	vertex_check();
	return MAX_MESSAGE_CNT - (mbox->wmsg_idx - mbox->rmsg_idx);
}

static int __vertex_mbox_emul_check_full(struct vertex_mailbox_ctrl *mctrl,
		unsigned int type, int wait)
{
	int ret;
	struct vertex_mailbox_f2h *mbox;
	int try_count = 1000;
	ssize_t remain_size;

	vertex_enter();
	if (type == VERTEX_MTYPE_F2H_NORMAL) {
		mbox = &mctrl->stack.f2h;
	} else if (type == VERTEX_MTYPE_F2H_URGENT) {
		mbox = &mctrl->urgent_stack.f2h;
	} else {
		ret = -EINVAL;
		vertex_err("invalid mbox type(%u)\n", type);
		goto p_err;
	}

	while (try_count) {
		remain_size = __vertex_mbox_emul_get_remain_size(mbox);
		if (remain_size > 0) {
			break;
		} else if (remain_size < 0) {
			ret = -EFAULT;
			vertex_err("index of mbox(%u) is invalid (%u/%u)\n",
					type, mbox->wmsg_idx, mbox->rmsg_idx);
			goto p_err;
		} else {
			vertex_warn("mbox(%u) has no remain space (%u/%u/%d)\n",
					type, mbox->wmsg_idx, mbox->rmsg_idx,
					try_count);
			udelay(10);
			try_count--;
		}
	}

	if (!remain_size) {
		ret = -EBUSY;
		vertex_err("mbox(%u) has no remain space (%u/%u)\n",
				type, mbox->wmsg_idx, mbox->rmsg_idx);
		goto p_err;
	}

	vertex_leave();
	return 0;
p_err:
	return ret;
}

static int __vertex_mbox_emul_write(struct vertex_mailbox_ctrl *mctrl,
		unsigned int type, void *msg, size_t size)
{
	int ret;
	struct vertex_mailbox_f2h *mbox;
	void *wptr;

	vertex_enter();
	if (size > sizeof(struct vertex_message)) {
		ret = -EINVAL;
		vertex_err("message size(%zu) for mbox(%u) is invalid\n",
				size, type);
		goto p_err;
	}

	if (type == VERTEX_MTYPE_F2H_NORMAL) {
		mbox = &mctrl->stack.f2h;
	} else if (type == VERTEX_MTYPE_F2H_URGENT) {
		mbox = &mctrl->urgent_stack.f2h;
	} else {
		ret = -EINVAL;
		vertex_err("invalid mbox type(%u)\n", type);
		goto p_err;
	}

	wptr = &mbox->msg[mbox->wmsg_idx % MAX_MESSAGE_CNT];
	vertex_io_copy_mem2io(wptr, msg, size);
	mbox->wmsg_idx++;

	vertex_dbg("[MBOX-W] type:%u/wmsg_idx:%u/rmsg_idx:%u\n",
			type, mbox->wmsg_idx, mbox->rmsg_idx);
	vertex_leave();
	return 0;
p_err:
	return ret;
}

static inline int __vertex_mbox_emul_is_empty(struct vertex_mailbox_h2f *mbox)
{
	vertex_check();
	return mbox->wmsg_idx == mbox->rmsg_idx;
}

static int __vertex_mbox_emul_check_reply(struct vertex_mailbox_ctrl *mctrl,
		unsigned int type, int wait)
{
	int ret;
	struct vertex_mailbox_h2f *mbox;
	int try_count = 1000;

	vertex_enter();
	if (type == VERTEX_MTYPE_H2F_NORMAL) {
		mbox = &mctrl->stack.h2f;
	} else if (type == VERTEX_MTYPE_H2F_URGENT) {
		mbox = &mctrl->urgent_stack.h2f;
	} else {
		ret = -EINVAL;
		vertex_err("invalid mbox type (%u)\n", type);
		goto p_err;
	}

	while (try_count) {
		ret = __vertex_mbox_emul_is_empty(mbox);
		if (ret) {
			if (!wait)
				break;

			udelay(10);
			try_count--;
		} else {
			break;
		}
	}

	if (ret) {
		ret = -EFAULT;
		goto p_err;
	}

	vertex_leave();
	return 0;
p_err:
	return ret;
}

static int __vertex_mbox_emul_read(struct vertex_mailbox_ctrl *mctrl,
		unsigned int type, void *msg)
{
	int ret;
	struct vertex_mailbox_h2f *mbox;
	void *rptr;

	vertex_enter();
	if (type == VERTEX_MTYPE_H2F_NORMAL) {
		mbox = &mctrl->stack.h2f;
	} else if (type == VERTEX_MTYPE_H2F_URGENT) {
		mbox = &mctrl->urgent_stack.h2f;
	} else {
		ret = -EINVAL;
		vertex_err("invalid mbox type(%u)\n", type);
		goto p_err;
	}

	rptr = &mbox->msg[mbox->rmsg_idx % MAX_MESSAGE_CNT];
	vertex_io_copy_io2mem(msg, rptr, sizeof(struct vertex_message));
	mbox->rmsg_idx++;

	vertex_dbg("[MBOX-R] type:%u/wmsg_idx:%u/rmsg_idx:%u\n",
			type, mbox->wmsg_idx, mbox->rmsg_idx);
	vertex_leave();
	return 0;
p_err:
	return ret;
}

int vertex_mbox_emul_handler(struct vertex_mailbox_ctrl *mctrl)
{
	int ret;
	struct vertex_message msg;
	int call_isr = 0;

	vertex_enter();
	ret = __vertex_mbox_emul_check_reply(mctrl, VERTEX_MTYPE_H2F_URGENT, 0);
	if (ret)
		goto p_normal;

	ret = __vertex_mbox_emul_read(mctrl, VERTEX_MTYPE_H2F_URGENT, &msg);
	if (ret)
		goto p_normal;

	ret = __vertex_mbox_emul_check_full(mctrl, VERTEX_MTYPE_F2H_URGENT, 0);
	if (ret)
		goto p_normal;

	ret = __vertex_mbox_emul_write(mctrl, VERTEX_MTYPE_F2H_URGENT, &msg,
			sizeof(msg));
	if (ret)
		goto p_normal;

	call_isr = 1;
p_normal:
	ret = __vertex_mbox_emul_check_reply(mctrl, VERTEX_MTYPE_H2F_NORMAL, 0);
	if (ret)
		goto p_err;

	ret = __vertex_mbox_emul_read(mctrl, VERTEX_MTYPE_H2F_NORMAL, &msg);
	if (ret)
		goto p_err;

	ret = __vertex_mbox_emul_check_full(mctrl, VERTEX_MTYPE_F2H_NORMAL, 0);
	if (ret)
		goto p_err;

	ret = __vertex_mbox_emul_write(mctrl, VERTEX_MTYPE_F2H_NORMAL, &msg,
			sizeof(msg));
	if (ret)
		goto p_err;

	call_isr = 1;
p_err:
	vertex_leave();
	return call_isr;
}

#endif
