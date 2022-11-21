/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VERTEX_INTERFACE_H__
#define __VERTEX_INTERFACE_H__

#include <linux/platform_device.h>
#include <linux/timer.h>

#include "vertex-config.h"
#include "platform/vertex-ctrl.h"
#include "vertex-taskmgr.h"

#define VERTEX_WORK_MAX_COUNT		(20)
#define VERTEX_WORK_MAX_DATA		(24)
#define VERTEX_COMMAND_TIMEOUT		(3 * HZ)

struct vertex_system;

enum vertex_work_message {
	VERTEX_WORK_MTYPE_BLK,
	VERTEX_WORK_MTYPE_NBLK,
};

enum vertex_interface_state {
	VERTEX_ITF_STATE_OPEN,
	VERTEX_ITF_STATE_BOOTUP,
	VERTEX_ITF_STATE_START
};

struct vertex_work {
	struct list_head		list;
	unsigned int			valid;
	unsigned int			id;
	enum vertex_work_message		message;
	unsigned int			param0;
	unsigned int			param1;
	unsigned int			param2;
	unsigned int			param3;
	unsigned char			data[VERTEX_WORK_MAX_DATA];
};

struct vertex_work_list {
	struct vertex_work		work[VERTEX_WORK_MAX_COUNT];
	spinlock_t			slock;
	struct list_head		free_head;
	unsigned int			free_cnt;
	struct list_head		reply_head;
	unsigned int			reply_cnt;
};

struct vertex_interface {
	int				irq[VERTEX_REG_MAX];
	void __iomem			*regs;
	struct vertex_taskmgr		taskmgr;
	void				*cookie;
	unsigned long			state;

	wait_queue_head_t		reply_wq;
	spinlock_t			process_barrier;
	struct vertex_work_list		work_list;
	struct work_struct		work_queue;

	struct vertex_task		*request[VERTEX_MAX_GRAPH];
	struct vertex_task		*process;
	struct vertex_work		reply[VERTEX_MAX_GRAPH];
	unsigned int			done_cnt;

	struct vertex_mailbox_ctrl	*mctrl;
#ifdef VERTEX_MBOX_EMULATOR
	struct timer_list		timer;
#endif

	struct vertex_system		*system;
};

int vertex_hw_wait_bootup(struct vertex_interface *itf);

int vertex_hw_config(struct vertex_interface *itf, struct vertex_task *itask);
int vertex_hw_process(struct vertex_interface *itf, struct vertex_task *itask);
int vertex_hw_create(struct vertex_interface *itf, struct vertex_task *itask);
int vertex_hw_destroy(struct vertex_interface *itf, struct vertex_task *itask);
int vertex_hw_init(struct vertex_interface *itf, struct vertex_task *itask);
int vertex_hw_deinit(struct vertex_interface *itf, struct vertex_task *itask);

int vertex_interface_start(struct vertex_interface *itf);
int vertex_interface_stop(struct vertex_interface *itf);
int vertex_interface_open(struct vertex_interface *itf, void *mbox);
int vertex_interface_close(struct vertex_interface *itf);

int vertex_interface_probe(struct vertex_system *sys);
void vertex_interface_remove(struct vertex_interface *itf);

#endif
