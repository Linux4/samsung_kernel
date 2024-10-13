/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_MAILBOX_H__
#define __DSP_MAILBOX_H__

#include "dsp-util.h"
#include "dsp-task.h"
#include "dsp-time.h"
#include "dsp-hw-mailbox.h"

struct dsp_system;

struct dsp_mailbox_pool_manager {
	spinlock_t			slock;
	struct list_head		pool_list;
	unsigned int			pool_count;
	struct dsp_util_bitmap		pool_map;

	size_t				size;
	dma_addr_t			iova;
	void				*kva;

	unsigned int			block_size;
	unsigned int			block_count;
	unsigned int			used_count;
};

struct dsp_mailbox_pool {
	unsigned int			block_count;
	int				block_start;
	size_t				size;
	size_t				used_size;
	dma_addr_t			iova;
	void				*kva;
	struct list_head		list;

	struct dsp_mailbox_pool_manager	*owner;
	struct dsp_time			time;
};

struct dsp_mailbox_pool *dsp_mailbox_alloc_pool(struct dsp_mailbox *mbox,
		unsigned int size);
void dsp_mailbox_free_pool(struct dsp_mailbox_pool *pool);
void dsp_mailbox_dump_pool(struct dsp_mailbox_pool *pool);

int dsp_mailbox_send_task(struct dsp_mailbox *mbox, struct dsp_task *task);
int dsp_mailbox_receive_task(struct dsp_mailbox *mbox);

int dsp_mailbox_send_message(struct dsp_mailbox *mbox, unsigned int message_id);

int dsp_mailbox_start(struct dsp_mailbox *mbox);
int dsp_mailbox_stop(struct dsp_mailbox *mbox);

int dsp_mailbox_open(struct dsp_mailbox *mbox);
int dsp_mailbox_close(struct dsp_mailbox *mbox);

int dsp_mailbox_probe(struct dsp_system *sys);
void dsp_mailbox_remove(struct dsp_mailbox *mbox);

#endif
