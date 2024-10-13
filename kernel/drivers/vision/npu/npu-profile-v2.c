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

#include "npu-profile-v2.h"
#include "npu-log.h"
#include "npu-sessionmgr.h"

static struct npu_profile npu_profiles[NPU_MAX_SESSION];

void npu_profile_iter_reupdate(unsigned int clist_idx, int uid)
{
	int i = 0;
	unsigned long *profiled_data;
	unsigned long *profiled_data_temp;
	unsigned long *profiled_data_fw;
	unsigned long fw_latency;
	unsigned long dd_latency;
	struct npu_profile *npu_profile = &npu_profiles[uid];

	if (npu_profile->iter * sizeof(u64) * END_DEVICE_PROFILE_LABEL >= npu_profile->size || npu_profile->profile_mem_buf == NULL)
		return;

	for (i = 0; i < npu_profile->iter; i++) {
		profiled_data = npu_profile->profile_mem_buf->vaddr + 8;
		profiled_data_fw = npu_profile->profile_mem_buf->vaddr + npu_profile->offset;
		if (npu_profile->mode == NPU_PERF_MODE_NPU_BOOST_PRUNE) {
			if (npu_profile->detail == PROFILE_MODE_DETAIL) {
				profiled_data +=  (i * MAX_DD_LABEL);
				profiled_data_fw += (i * MAX_FW_LABEL);
				fw_latency = *profiled_data_fw >> 32;
				dd_latency = *profiled_data >> 32;
				dd_latency += fw_latency;
				*profiled_data = (dd_latency  << 32) | (clist_idx << 16) | PROFILE_DD_TOP;
				profiled_data_temp = profiled_data + 3;
				*profiled_data_temp = (fw_latency  << 32) | (clist_idx << 16) | PROFILE_DD_FW_IF;
			} else {
				profiled_data +=  i;
				profiled_data_fw += (i * 2);
				fw_latency = *profiled_data_fw >> 32;
				dd_latency = *profiled_data >> 32;
				dd_latency += fw_latency;
				*profiled_data = (dd_latency  << 32) | (clist_idx << 16) | PROFILE_DD_TOP;
			}
		}
	}
}

void npu_profile_iter_update(unsigned int clist_idx, int uid)
{
	int i = 0;
	unsigned int updated_iter;
	struct npu_profile *npu_profile = &npu_profiles[uid];
	u64 *profiled_data;
	u64 *profiled_iter;

	if (npu_profile->iter * sizeof(u64) * END_DEVICE_PROFILE_LABEL >= npu_profile->size || npu_profile->profile_mem_buf == NULL)
		return;

	profiled_iter = npu_profile->profile_mem_buf->vaddr;
	profiled_data = npu_profile->profile_mem_buf->vaddr + 8;

	if (npu_profile->detail == PROFILE_MODE_DETAIL) {
		updated_iter = npu_profile->iter * MAX_DD_LABEL;
		profiled_data += updated_iter;
		*profiled_data = ((npu_profile->duration[clist_idx][PROFILE_DD_TOP - 1])  << 32) | (clist_idx << 16) | PROFILE_DD_TOP;
		profiled_data++;
		for (i = 1; i < MAX_DD_LABEL; i++) {
			if (npu_profile->duration[clist_idx][i] == 0) {
				*profiled_data = ((u64)0xFFFFFFFF << 32) | (clist_idx << 16) | (i + 1);
			} else {
				*profiled_data = ((npu_profile->duration[clist_idx][i]) << 32) | (clist_idx << 16) | (i + 1);
			}
			profiled_data++;
		}
	} else {
		profiled_data +=  npu_profile->iter;
		*profiled_data = ((npu_profile->duration[clist_idx][PROFILE_DD_TOP - 1])  << 32) | (clist_idx << 16) | PROFILE_DD_TOP;
	}

	npu_profile->iter++;
	*profiled_iter = npu_profile->iter;
}

void npu_profile_record_end(unsigned int label, unsigned int clist_idx, u64 ktime, unsigned int mode, int uid)
{
	struct npu_profile *npu_profile = &npu_profiles[uid];

	if (npu_profile->mode != NPU_PERF_MODE_NORMAL) {
		if ((npu_profile->mode & mode) == 0)
			return;
	}

	if (npu_profile->iter >= npu_profile->max_iter)
		return;

	if (!npu_profile->detail && label > 1)
		return;

	npu_profile->duration[clist_idx][label - 1] = ktime - npu_profile->duration[clist_idx][label - 1];
}

void npu_profile_record_start(unsigned int label, unsigned int clist_idx, u64 ktime, unsigned int mode, int uid)
{
	struct npu_profile *npu_profile = &npu_profiles[uid];

	if (npu_profile->mode != NPU_PERF_MODE_NORMAL) {
		if ((npu_profile->mode & mode) == 0)
			return;
	}

	if (npu_profile->iter >= npu_profile->max_iter)
		return;

	if (!npu_profile->detail && label > 1)
		return;

	npu_profile->duration[clist_idx][label - 1] = ktime;
}

void npu_profile_deinit(struct npu_memory *memory)
{
	struct npu_memory_buffer *ion_mem_buf;
	struct npu_profile *npu_profile;
	int i = 0;

	for (i = 0; i < NPU_MAX_SESSION; i++) {
		npu_profile = &npu_profiles[i];

		ion_mem_buf = npu_profile->profile_mem_buf;
		npu_profile->profile_mem_buf = NULL;
		if (ion_mem_buf) {
			npu_memory_unmap(memory, ion_mem_buf);
			kfree(ion_mem_buf);
		}

		ion_mem_buf = npu_profile->profile_mem_sub_buf;
		npu_profile->profile_mem_sub_buf = NULL;
		if (ion_mem_buf) {
			npu_memory_free(memory, ion_mem_buf);
			kfree(ion_mem_buf);
		}
	}
}

void npu_profile_init(struct npu_memory_buffer *profile_mem_buf, struct npu_memory_buffer *profile_mem_sub_buf,
		struct vs4l_profile_ready *pread, unsigned int mode, int uid)
{
	int i = 0, j = 0;
	struct npu_profile *npu_profile = &npu_profiles[uid];

	npu_profile->profile_mem_buf = profile_mem_buf;
	npu_profile->profile_mem_sub_buf = profile_mem_sub_buf;
	npu_profile->fd = pread->fd;
	npu_profile->detail = pread->mode;
	npu_profile->mode = mode;
	npu_profile->size = pread->size;
	npu_profile->offset = pread->offset;
	npu_profile->iter = 0;
	npu_profile->max_iter = pread->max_iter;

	for (i = 0; i < NPU_FRAME_MAX_CONTAINERLIST; i++) {
		for (j = 0; j < MAX_DD_LABEL; j++) {
			npu_profile->duration[i][j] = 0;
		}
	}
}
