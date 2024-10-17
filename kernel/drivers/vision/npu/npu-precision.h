/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_PRECISION_H_
#define _NPU_PRECISION_H_

#define PRECISION_LEN	256
#define PRECISION_REGISTERED	0xCAFE
#define PRECISION_RELEASE_FREQ	1300000

enum ncp_precision_featuremapdata_type {
	PRE_INT4    = 0x1,
	PRE_INT8    = 0x2,
	PRE_INT16   = 0x4,
	PRE_FP8     = 0x8,
	PRE_FP16    = 0x10,
	PRE_FP32    = 0x20,
};

struct npu_precision_model_info {
	bool active;
	u32 index;
	u32 type;
	char model_name[NCP_MODEL_NAME_LEN];
	u32 computational_workload;
	u32 io_workload;
	atomic_t ref;

	struct hlist_node hlist;
};

int npu_precision_hash_check(struct npu_session *session);
void npu_precision_hash_update(struct npu_session *session, u32 type);
void npu_precision_active_hash_delete(struct npu_session *session);
int npu_precision_open(struct npu_system *system);
int npu_precision_close(struct npu_system *system);
int npu_precision_probe(struct npu_device *device);
int npu_precision_release(struct npu_device *device);

#endif /* _NPU_PRECISION_H_ */
