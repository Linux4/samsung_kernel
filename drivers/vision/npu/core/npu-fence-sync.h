/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_SYNC_H_
#define _NPU_SYNC_H_

#include <linux/dma-fence.h>

struct npu_sync_file {
	struct file *file;
	/**
	 * @user_name:
	 *
	 * Name of the sync file provided by userspace, for merged fences.
	 * Otherwise generated through driver callbacks (in which case the
	 * entire array is 0).
	 */
	char user_name[32];
#ifdef CONFIG_DEBUG_FS
	struct list_head sync_file_list;
#endif

	wait_queue_head_t wq;
	unsigned long flags;

	struct dma_fence *fence;
	struct dma_fence_cb cb;
};

#define NPU_POLL_ENABLED 0

struct npu_sync_file *npu_sync_file_create(struct dma_fence *fence);
struct npu_sync_file *npu_sync_file_fdget(int fd);
struct dma_fence *npu_sync_file_get_fence(int fd);

#define SYNC_IOC_UPDATE_PT			_IOW('V', 0, struct npu_sync_data)

#endif // _NPU_SYNC_H_
