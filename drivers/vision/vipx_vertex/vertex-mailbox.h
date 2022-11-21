/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VERTEX_MAILBOX_H__
#define __VERTEX_MAILBOX_H__

#include "vertex-message.h"

#define MAX_MESSAGE_CNT		(8)
#define VERTEX_MAILBOX_WAIT	(true)
#define VERTEX_MAILBOX_NOWAIT	(false)

enum vertex_mailbox_h2f_type {
	VERTEX_MTYPE_H2F_NORMAL,
	VERTEX_MTYPE_H2F_URGENT,
};

enum vertex_mailbox_f2h_type {
	VERTEX_MTYPE_F2H_NORMAL,
	VERTEX_MTYPE_F2H_URGENT,
};

struct vertex_mailbox_h2f {
	/* Host permission of wmsg is RW */
	unsigned int			wmsg_idx;
	/* Host permission of rmsg is R_ONLY */
	unsigned int			rmsg_idx;
	struct vertex_message		msg[MAX_MESSAGE_CNT];
};

struct vertex_mailbox_f2h {
	/* Host permission of wmsg is RW */
	unsigned int			wmsg_idx;
	/* Host permission of rmsg is R_ONLY */
	unsigned int			rmsg_idx;
	struct vertex_message		msg[MAX_MESSAGE_CNT];
};

struct vertex_mailbox_stack {
	struct vertex_mailbox_h2f	h2f;
	struct vertex_mailbox_f2h	f2h;
};

struct vertex_mailbox_ctrl {
	struct vertex_mailbox_stack	stack;
	struct vertex_mailbox_stack	urgent_stack;
};

int vertex_mbox_check_full(struct vertex_mailbox_ctrl *mctrl, unsigned int type,
		int wait);
int vertex_mbox_write(struct vertex_mailbox_ctrl *mctrl, unsigned int type,
		void *msg, size_t size);

int vertex_mbox_check_reply(struct vertex_mailbox_ctrl *mctrl,
		unsigned int type, int wait);
int vertex_mbox_read(struct vertex_mailbox_ctrl *mctrl, unsigned int type,
		void *msg);

#endif
