// SPDX-License-Identifier: GPL
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/slab.h>

#include "pablo-icpu.h"
#include "pablo-icpu-mem-wrapper.h"
#include "pablo-mem.h"

static struct icpu_logger _log = {
	.level = LOGLEVEL_INFO,
	.prefix = "[ICPU-PMEM-WRAP]",
};

struct icpu_logger *get_icpu_pmem_log(void)
{
	return &_log;
}

static struct is_mem __is_mem;

static int __alloc(struct device *dev, struct icpu_buf *ibuf)
{
	struct is_priv_buf *buf;

	if (!ibuf || !dev) {
		ICPU_ERR("invalid parameters");
		return -EINVAL;
	}

	buf = CALL_PTR_MEMOP(&__is_mem, alloc, __is_mem.priv, ibuf->size, ibuf->heapname, 0);
	if (IS_ERR_OR_NULL(buf)) {
		ICPU_ERR("fail to alloc");
		return -ENOMEM;
	}

	ibuf->data = buf;
	ibuf->paddr = CALL_BUFOP(buf, phaddr, buf);
	ibuf->daddr = CALL_BUFOP(buf, dvaddr, buf);
	ibuf->vaddr = (void*)CALL_BUFOP(buf, kvaddr, buf);

	return 0;
}

static void __free(struct icpu_buf *buf)
{
	if (!buf || !buf->data)
		return;

	CALL_VOID_BUFOP((struct is_priv_buf *)buf->data, free, buf->data);
}

static void __sync_for_device(struct icpu_buf *buf)
{
	if (!buf || !buf->data)
		return;

	CALL_BUFOP((struct is_priv_buf *)buf->data, sync_for_device, buf->data,
			0, buf->size, DMA_TO_DEVICE);
}

struct icpu_buf_ops icpu_buf_ops_pmem = {
	.alloc = __alloc,
	.free = __free,
	.sync_for_device = __sync_for_device,
};

int pablo_icpu_pmem_init(void *pdev)
{
	int ret;

	ret = is_mem_init(&__is_mem, pdev);
	if (ret) {
		ICPU_ERR("is_mem_init fail");
		return ret;
	}

	return 0;
}

void pablo_icpu_pmem_deinit(void)
{
}
