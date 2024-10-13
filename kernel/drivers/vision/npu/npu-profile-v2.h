/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_PROFILE_V2_H_
#define _NPU_PROFILE_V2_H_

#include "npu-common.h"
#include "npu-memory.h"

#define PROFILE_MODE_OVERVIEW	(1)
#define PROFILE_MODE_DETAIL	(2)
#define MAX_DD_LABEL	(4)
#define MAX_FW_LABEL	(9)

enum npu_profile_label {
	PROFILE_DD_TOP = 1,
	PROFILE_DD_QUEUE,
	PROFILE_DD_SESSION,
	PROFILE_DD_FW_IF,
	PROFILE_FW_INTERNAL,
	PROFILE_HW,
	PROFILE_HW_NPU0,
	PROFILE_HW_NPU1,
	PROFILE_HW_DSP,
	PROFILE_HW_NPU0_NPU1,
	PROFILE_HW_NPU0_DSP,
	PROFILE_HW_NPU1_DSP,
	PROFILE_HW_NPU0_NPU1_DSP,
	END_DEVICE_PROFILE_LABEL,
};

struct npu_profile {
	unsigned int fd;
	unsigned int detail;
	unsigned int mode;
	unsigned long max_size;
	unsigned long size;
	unsigned long offset;
	unsigned long max_iter;
	unsigned long iter;
	unsigned long duration[NPU_FRAME_MAX_CONTAINERLIST][MAX_DD_LABEL];
	struct npu_memory_buffer *profile_mem_sub_buf;
	struct npu_memory_buffer *profile_mem_buf;
};

void npu_profile_iter_update(unsigned int clist_idx, int uid);
void npu_profile_iter_reupdate(unsigned int clist_idx, int uid);
void npu_profile_record_start(unsigned int label, unsigned int clist_idx, u64 ktime, unsigned int mode, int uid);
void npu_profile_record_end(unsigned int label, unsigned int clist_idx, u64 ktime, unsigned int mode, int uid);
void npu_profile_init(struct npu_memory_buffer *profile_mem_buf, struct npu_memory_buffer *profile_mem_sub_buf,
		struct vs4l_profile_ready *pread, unsigned int mode, int uid);
void npu_profile_deinit(struct npu_memory *memory);

#endif	/* _NPU_PROFILE_H_ */
