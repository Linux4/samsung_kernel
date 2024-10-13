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

#ifndef _NPU_PROFILE_V2_H_
#define _NPU_PROFILE_V2_H_

enum profile_type
{
	PROFILE_S_GRAPH,
	PROFILE_S_FORMAT,
	PROFILE_S_PARAM,
	PROFILE_S_CTRL,
	PROFILE_STREAM_ON,
	PROFILE_STREAM_OFF,
	PROFILE_QBUF,
	PROFILE_DQBUF,
	PROFILE_PREPARE,
	PROFILE_UNPREPARE,
	PROFILE_SCHED_PARAM,
	PROFILE_ON,
	PROFILE_OFF,
	PROFILE_BOOTUP,
	PROFILE_SYNC,
	PROFILE_VERSION,
	PROFILE_TEMP,
	PROFILE_DD_FIRMWARE,
	PROFILE_DD_NPU_HARDWARE,
	PROFILE_DD_DSP_HARDWARE,
	PROFILE_USED_COUNT,
};

static const char *profile_type_str[] = {
	"VS4L_VERTEXIOC_S_GRAPH",
	"VS4L_VERTEXIOC_S_FORMAT",
	"VS4L_VERTEXIOC_S_PARAM",
	"VS4L_VERTEXIOC_S_CTRL",
	"VS4L_VERTEXIOC_STREAM_ON",
	"VS4L_VERTEXIOC_STREAM_OFF",
	"VS4L_VERTEXIOC_QBUF",
	"VS4L_VERTEXIOC_DQBUF",
	"VS4L_VERTEXIOC_PREPARE",
	"VS4L_VERTEXIOC_UNPREPARE",
	"VS4L_VERTEXIOC_SCHED_PARAM",
	"VS4L_VERTEXIOC_PROFILE_ON",
	"VS4L_VERTEXIOC_PROFILE_OFF",
	"VS4L_VERTEXIOC_BOOTUP",
	"VS4L_VERTEXIOC_SYNC",
	"VS4L_VERTEXIOC_VERSION",
	"VS4L_VERTEXIOC_TEMP",
	"VS4L_VERTEXIOC_DD_FIRMWARE",
	"VS4L_VERTEXIOC_DD_NPU_HARDWARE",
	"VS4L_VERTEXIOC_DD_DSP_HARDWARE",
};

struct npu_profile_node {
	char *name;
	unsigned int duration;
};

struct npu_profile {
	unsigned int flag;
	unsigned int type;
	unsigned int iter;
	unsigned int duration;
	struct npu_profile_node *node;
};

void npu_profile_print(struct npu_profile *profiler);
int npu_profile_unprepare(struct npu_profile *profiler);
int npu_profile_prepare(struct npu_profile *profiler);
int npu_profile_init(struct npu_profile *profiler);

#endif	/* _NPU_PROFILE_H_ */
