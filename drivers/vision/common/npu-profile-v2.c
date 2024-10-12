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

#include "npu-profile-v2.h"
#include "npu-log.h"

void npu_profile_print(struct npu_profile *profiler)
{
	int iter = 0;
	if (!profiler || !profiler[PROFILE_ON].flag)
		return;

	iter = profiler[PROFILE_QBUF].iter / 2;

	npu_info("-------------- NPU/DSP system call Profile --------------\n");
	npu_info("%s iter - %d \n", profile_type_str[PROFILE_QBUF], profiler[PROFILE_QBUF].iter);
	npu_info("%s iter - %d \n", profile_type_str[PROFILE_DQBUF], profiler[PROFILE_DQBUF].iter);

	npu_info("%s - [%d]us \n", profile_type_str[PROFILE_BOOTUP], profiler[PROFILE_BOOTUP].duration);
	npu_info("%s - [%d]us \n", profile_type_str[PROFILE_S_PARAM], profiler[PROFILE_S_PARAM].duration);
	npu_info("%s - [%d]us \n", profile_type_str[PROFILE_S_GRAPH], profiler[PROFILE_S_GRAPH].duration);
	npu_info("%s - [%d]us \n", profile_type_str[PROFILE_S_FORMAT], profiler[PROFILE_S_FORMAT].duration);
	npu_info("%s - [%d]us \n", profile_type_str[PROFILE_SCHED_PARAM], profiler[PROFILE_SCHED_PARAM].duration);
	npu_info("%s - [%d]us \n", profile_type_str[PROFILE_STREAM_ON], profiler[PROFILE_STREAM_ON].duration);
	npu_info("%s - [%d]us \n", profile_type_str[PROFILE_QBUF], profiler[PROFILE_QBUF].duration / iter);
	npu_info("%s - [%d]us \n", profile_type_str[PROFILE_DQBUF], (profiler[PROFILE_DQBUF].duration - profiler[PROFILE_DD_FIRMWARE].duration) / iter);
	npu_info("%s - [%d]us \n", profile_type_str[PROFILE_DD_FIRMWARE], profiler[PROFILE_DD_FIRMWARE].duration / iter);
	npu_info("%s - [%d]us \n", profile_type_str[PROFILE_DD_NPU_HARDWARE], profiler[PROFILE_DD_NPU_HARDWARE].duration / iter);
	npu_info("%s - [%d]us \n", profile_type_str[PROFILE_DD_DSP_HARDWARE], profiler[PROFILE_DD_DSP_HARDWARE].duration / iter);
	npu_info("---------------------------------------------------------\n");
}

int npu_profile_unprepare(struct npu_profile *profiler)
{
	int ret = 0;

	if (!profiler)
		return -EFAULT;

	if (profiler[PROFILE_QBUF].node)
		kfree(profiler[PROFILE_QBUF].node);

	return ret;
}

int npu_profile_prepare(struct npu_profile *profiler)
{
	int ret = 0;

	if (!profiler)
		return -EFAULT;

	if (profiler[PROFILE_QBUF].node == NULL) {
		profiler[PROFILE_QBUF].node = kcalloc(3, sizeof(struct npu_profile_node), GFP_KERNEL);
		if (!profiler[PROFILE_QBUF].node)
			ret = -ENOMEM;
	}

	// profiler[PROFILE_QBUF].duration = 0;
	// profiler[PROFILE_DQBUF].duration = 0;

	return ret;
}

int npu_profile_init(struct npu_profile *profiler)
{
	int i = 0;
	int ret = 0;

	if (!profiler)
		return -EFAULT;

	for (i = 0; i < PROFILE_USED_COUNT; i++) {
			profiler[i].flag = 0;
			profiler[i].type = 0;
			profiler[i].duration = 0;
			profiler[i].node = NULL;
	}

	return ret;
}
