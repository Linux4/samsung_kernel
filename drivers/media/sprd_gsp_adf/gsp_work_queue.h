/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __GSP_WORKQUEUE_H__
#define __GSP_WORKQUEUE_H__

#include <video/gsp_types_shark_adf.h>
#include <linux/slab.h>
#include "gsp_drv_common.h"
#include "gsp_kcfg.h"

#define GSP_EMPTY_MAX 8
#define GSP_FILL_MAX 8

struct gsp_work_queue {
	struct list_head fill_list_head;
	struct list_head empty_list_head;
	struct list_head separate_list_head;
	struct mutex fill_lock;
	struct mutex empty_lock;
	struct mutex sep_lock;
	int fill_cnt;
	int empty_cnt;
	int sep_cnt;
	wait_queue_head_t empty_wait;
	struct list_head *index[GSP_EMPTY_MAX];
	int init;
};

struct gsp_kcfg_info *gsp_work_queue_get_sep_kcfg(
		struct gsp_work_queue* wq);

struct gsp_kcfg_info *gsp_work_queue_get_empty_kcfg(
		struct gsp_work_queue *wq, struct gsp_context *ctx);

void gsp_work_queue_destroy(struct gsp_work_queue *wq);

struct gsp_kcfg_info *gsp_work_queue_pull_kcfg(struct gsp_work_queue *wq);

int gsp_work_queue_push_kcfg(struct gsp_work_queue *wq,
		struct gsp_kcfg_info *kcfg);

void gsp_work_queue_init(struct gsp_work_queue *wq);

void gsp_work_queue_put_fill_list(struct gsp_work_queue *wq);

void gsp_work_queue_put_sep_list(struct gsp_work_queue *wq);

void gsp_work_queue_put_back_kcfg(struct gsp_kcfg_info *kcfg,
		struct gsp_work_queue *wq);

int gsp_work_queue_has_fill_kcfg(struct gsp_work_queue *wq);

struct gsp_kcfg_info *gsp_work_queue_get_fill_kcfg(
		struct gsp_work_queue *wq);
#endif
