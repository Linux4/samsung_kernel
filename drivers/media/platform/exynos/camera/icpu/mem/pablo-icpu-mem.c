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

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "pablo-icpu.h"
#include "pablo-icpu-mem.h"
#include "pablo-icpu-mem-wrapper.h"

static struct icpu_logger _log = {
	.level = LOGLEVEL_INFO,
	.prefix = "[ICPU-MEM]",
};

struct icpu_logger *get_icpu_mem_log(void)
{
	return &_log;
}

enum icpu_mem_state {
	ICPU_MEM_STATE_INVALID = 0,
	ICPU_MEM_STATE_INIT = 1,
};

static struct icpu_mem {
	enum icpu_mem_type type;
	enum icpu_mem_state state;
	void *priv;
} _icpu_mem;

extern struct icpu_buf_ops icpu_buf_ops_pmem;
extern struct icpu_buf_ops icpu_buf_ops_cma;

static struct icpu_buf_ops *__icpu_buf_ops[] = {
	&icpu_buf_ops_pmem,
	&icpu_buf_ops_cma,
};

static inline bool __check_state(enum icpu_mem_state state)
{
	return _icpu_mem.state == state ? true : false;
}

static inline size_t __mem_size(struct icpu_buf *buf)
{
	return buf->size;
}

static inline dma_addr_t __mem_dvaddr(struct icpu_buf *buf)
{
	return buf->daddr;
}

static inline ulong __mem_kvaddr(struct icpu_buf *buf)
{
	return (ulong)buf->vaddr;
}

static inline phys_addr_t __mem_phaddr(struct icpu_buf *buf)
{
	return buf->paddr;
}

static inline u32 __get_type(struct icpu_buf *buf)
{
	return buf->type;
}

void *pablo_icpu_mem_alloc(u32 type, size_t size, const char *heapname, unsigned long align)
{
	int ret;
	struct icpu_buf *ibuf;

	if (!__check_state(ICPU_MEM_STATE_INIT)) {
		ICPU_ERR("icpu mem state is invalid");
		return NULL;
	}

	ibuf = kzalloc(sizeof(struct icpu_buf), GFP_KERNEL);
	if (!ibuf) {
		ICPU_ERR("ibuf alloc fail");
		return NULL;
	}

	ibuf->type = type;
	ibuf->size = size;
	ibuf->align = align;
	ibuf->heapname = heapname;

	ret = __icpu_buf_ops[type]->alloc(_icpu_mem.priv, ibuf);
	if (ret) {
		ICPU_ERR("alloc fail, ret(%d)", ret);
		kfree(ibuf);
		ibuf = NULL;
	}

	return ibuf;
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_mem_alloc);

void pablo_icpu_mem_free(void *buf)
{
	if (!buf)
		return;

	__icpu_buf_ops[__get_type(buf)]->free(buf);

	kfree(buf);
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_mem_free);

struct pablo_icpu_buf_info pablo_icpu_mem_get_buf_info(void *buf)
{
	struct pablo_icpu_buf_info info = {0, };

	if (!buf)
		return info;

	info.size = __mem_size(buf);
	info.kva = __mem_kvaddr(buf);
	info.dva = __mem_dvaddr(buf);

	/* TODO: only reserved mem valid pa */
	info.pa = __mem_phaddr(buf);

	return info;
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_mem_get_buf_info);

void pablo_icpu_mem_sync_for_device(void *buf)
{
	if (!buf)
		return;

	__icpu_buf_ops[__get_type(buf)]->sync_for_device(buf);
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_mem_sync_for_device);

int pablo_icpu_mem_probe(void *pdev)
{
	int ret;

	_icpu_mem.priv = &((struct platform_device *)pdev)->dev;

	ret = pablo_icpu_pmem_init(pdev);
	if (ret) {
		ICPU_ERR("pmem_init fail");
		goto probe_fail;
	}

	ret = pablo_icpu_cma_init(pdev);
	if (ret) {
		ICPU_ERR("cma_init fail");
		goto probe_fail;
	}

	_icpu_mem.state = ICPU_MEM_STATE_INIT;

probe_fail:
	return ret;
}

void pablo_icpu_mem_remove(void)
{
	pablo_icpu_pmem_deinit();
	pablo_icpu_cma_deinit();

	memset(&_icpu_mem, 0, sizeof(_icpu_mem));
}
