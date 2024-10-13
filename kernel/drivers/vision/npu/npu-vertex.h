/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_VERTEX_H_
#define _NPU_VERTEX_H_

#include "npu-queue.h"
#include "npu-syscall.h"
#include "npu-profile-v2.h"

struct npu_vertex;
bool is_kpi_mode_enabled(bool strict);

enum npu_vertex_state {
	NPU_VERTEX_OPEN,
	NPU_VERTEX_POWER,
	NPU_VERTEX_GRAPH,
	NPU_VERTEX_FORMAT,
	NPU_VERTEX_STREAMOFF,
	NPU_VERTEX_CLOSE
};

struct npu_vertex_refcount {
	atomic_t			refcount;
	struct npu_vertex		*vertex;
	int				(*first)(struct npu_vertex *vertex);
	int				(*final)(struct npu_vertex *vertex);
};

struct npu_vertex {
	struct mutex lock;
	struct vision_device vd;
	struct npu_vertex_refcount open_cnt;
	struct npu_vertex_refcount start_cnt;
};

struct npu_vertex_ctx {
	u32 state;
	u32 id;
	struct mutex lock;
	struct npu_queue queue;
	struct npu_vertex *vertex;
};

enum npu_vertex_state check_done_state(u32 state);
int npu_vertex_probe(struct npu_vertex *vertex, struct device *parent);
int npu_vertex_s_graph(struct file *file, struct vs4l_graph *sinfo);
int npu_vertex_s_format(struct file *file, struct vs4l_format_bundle *fbundle);
int npu_vertex_s_param(struct file *file, struct vs4l_param_list *plist);
int npu_vertex_s_ctrl(struct file *file, struct vs4l_ctrl *ctrl);
int npu_streamon(struct npu_vertex_ctx *vctx);
int npu_vertex_bootup(struct file *file, struct vs4l_ctrl *ctrl);
int npu_vertex_streamoff(struct file *file);
int npu_vertex_qbuf(struct file *file, struct vs4l_container_bundle *cbundle);
int npu_vertex_dqbuf(struct file *file, struct vs4l_container_bundle *cbundle);
int npu_vertex_prepare(struct file *file, struct vs4l_container_bundle *cbundle);
int npu_vertex_unprepare(struct file *file, struct vs4l_container_bundle *cbundle);
int npu_vertex_sched_param(struct file *file, struct vs4l_sched_param *param);
int npu_vertex_g_max_freq(struct file *file, struct vs4l_freq_param *param);
int npu_vertex_profileon(struct file *file, struct vs4l_profiler *phead);
int npu_vertex_profileoff(struct file *file, struct vs4l_profiler *phead);
int npu_vertex_profile_ready(struct file *file, struct vs4l_profile_ready *pread);
int npu_vertex_version(struct file *file, struct vs4l_version *version);
int npu_vertex_sync(struct file *file, struct vs4l_ctrl *ctrl);
int npu_vertex_open(struct file *file);
int npu_vertex_close(struct file *file);
int npu_vertex_flush(struct file *file);
int npu_vertex_qbuf_cancel(struct file *file, struct vs4l_container_bundle *cbundle);
unsigned int npu_vertex_poll(struct file *file, poll_table *poll);
int npu_vertex_mmap(struct file *file, struct vm_area_struct *vm_area);

#endif
