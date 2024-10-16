// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/dma-buf.h>

#include "pablo-interface-irta.h"
#include "pablo-debug.h"
#include "is-config.h"
#include "is-hw.h"
#include "pablo-icpu-itf.h"
#include "pablo-icpu-adapter.h"
#include "is-interface-wrap.h"

static struct pablo_interface_irta *itf_irta;

int pablo_interface_irta_dma_buf_attach(struct pablo_interface_irta *pii, u32 idx, int fd)
{
	int ret;

	if (fd < 0) {
		err("fd is invalid (idx: %d, fd: %d)", idx, fd);
		return -EINVAL;
	}

	pii->dma_buf[idx] = dma_buf_get(fd);
	if (IS_ERR(pii->dma_buf[idx])) {
		err("Failed to get dmabuf. fd %d ret %ld", fd, PTR_ERR(pii->dma_buf[idx]));
		return -EINVAL;
	}

	pii->attachment[idx] = dma_buf_attach(pii->dma_buf[idx], pablo_itf_get_icpu_dev());
	if (IS_ERR(pii->attachment[idx])) {
		ret = -ENOMEM;
		goto err_attach;
	}

	pii->sgt[idx] = dma_buf_map_attachment(pii->attachment[idx], DMA_BIDIRECTIONAL);
	if (IS_ERR(pii->sgt[idx])) {
		ret = -ENOMEM;
		goto err_map_attachment;
	}

	pii->dva[idx] = sg_dma_address(pii->sgt[idx]->sgl);
	if (!pii->dva[idx]) {
		ret = -ENOMEM;
		goto err_address;
	}

	pii->dma_buf_fd[idx] = fd;

	return 0;

err_address:
	dma_buf_unmap_attachment(pii->attachment[idx], pii->sgt[idx], DMA_BIDIRECTIONAL);

err_map_attachment:
	dma_buf_detach(pii->dma_buf[idx], pii->attachment[idx]);

err_attach:
	dma_buf_put(pii->dma_buf[idx]);

	pii->dva[idx] = 0;
	pii->sgt[idx] = NULL;
	pii->attachment[idx] = NULL;
	pii->dma_buf[idx] = NULL;
	pii->dma_buf_fd[idx] = 0;

	return ret;
}
KUNIT_EXPORT_SYMBOL(pablo_interface_irta_dma_buf_attach);

int pablo_interface_irta_dma_buf_detach(struct pablo_interface_irta *pii, u32 idx)
{
	if (!pii->dma_buf[idx]) {
		err("dma_buf is NULL(idx: %d)", idx);
		return -EINVAL;
	}

	dma_buf_unmap_attachment(pii->attachment[idx], pii->sgt[idx], DMA_BIDIRECTIONAL);
	dma_buf_detach(pii->dma_buf[idx], pii->attachment[idx]);
	dma_buf_put(pii->dma_buf[idx]);

	pii->dva[idx] = 0;
	pii->sgt[idx] = NULL;
	pii->attachment[idx] = NULL;
	pii->dma_buf[idx] = NULL;
	pii->dma_buf_fd[idx] = 0;
	
	return 0;
}
KUNIT_EXPORT_SYMBOL(pablo_interface_irta_dma_buf_detach);

int pablo_interface_irta_kva_map(struct pablo_interface_irta *pii, u32 idx)
{
	pii->kva[idx] = pkv_dma_buf_vmap(pii->dma_buf[idx]);
	if (!pii->kva[idx]) {
		err("kva is invalid(idx: %d, kva: %p)", idx, pii->kva[idx]);
		return -ENOMEM;
	}

	return 0;
}
KUNIT_EXPORT_SYMBOL(pablo_interface_irta_kva_map);

int pablo_interface_irta_kva_unmap(struct pablo_interface_irta *pii, u32 idx)
{
	if (!pii->dma_buf[idx]) {
		err("dma_buf is NULL(idx: %d)", idx);
		return -EINVAL;
	}

	if (!pii->kva[idx])
		return 0;

	pkv_dma_buf_vunmap(pii->dma_buf[idx], pii->kva[idx]);

	pii->kva[idx] = NULL;

	return 0;
}
KUNIT_EXPORT_SYMBOL(pablo_interface_irta_kva_unmap);

static void check_valid_fd(struct pablo_interface_irta *pii, u32 idx, int fd)
{
	int prev_fd = pii->dma_buf_fd[idx];

	if (pii->dma_buf[idx]) {
		if (prev_fd == fd)
			warn("fd is same(idx: %d)", idx);
		else
			warn("fd is not empty(idx: %d)", idx);

		if (pii->kva[idx])
			pablo_interface_irta_kva_unmap(pii, idx);

		pablo_interface_irta_dma_buf_detach(pii, idx);
	}
}

int pablo_interface_irta_result_buf_set(struct pablo_interface_irta *pii, int fd, u32 idx)
{
	int ret;
	struct pablo_crta_buf_info buf_info = { 0, };

	if (idx >= ITF_IRTA_BUF_TYPE_MAX) {
		err("idx is invalid (idx: %d, fd: %d)", idx, fd);
		return -EINVAL;
	}

	check_valid_fd(pii, idx, fd);

	ret = pablo_interface_irta_dma_buf_attach(pii, idx, fd);
	if (ret)
		return ret;

	ret = pablo_interface_irta_kva_map(pii, idx);
	if (ret)
		goto err_save_kva;

	buf_info.id = idx;
	buf_info.dva = pii->dva[idx];
	buf_info.kva = pii->kva[idx];
	buf_info.size = pii->dma_buf[idx]->size;
	CALL_ADT_MSG_OPS(pii->icpu_adt, send_msg_put_buf, pii->instance,
			PABLO_BUFFER_RTA, &buf_info);

	return 0;

err_save_kva:
	pablo_interface_irta_dma_buf_detach(pii, idx);

	return ret;
}
KUNIT_EXPORT_SYMBOL(pablo_interface_irta_result_buf_set);

int pablo_interface_irta_result_fcount_set(struct pablo_interface_irta *pii, u32 fcount)
{
	CALL_ADT_MSG_OPS(pii->icpu_adt, send_msg_set_shared_buf_idx, pii->instance,
			PABLO_BUFFER_RTA, fcount);

	return 0;
}
KUNIT_EXPORT_SYMBOL(pablo_interface_irta_result_fcount_set);

int pablo_interface_irta_start(struct pablo_interface_irta *pii)
{
	return is_itf_icpu_start_wrap(pii->idi);
}
KUNIT_EXPORT_SYMBOL(pablo_interface_irta_start);

static void flush_all_buf_type(struct pablo_interface_irta *pii)
{
	u32 idx;

	for (idx = 0; idx < ITF_IRTA_BUF_TYPE_MAX; idx++) {
		if (pii->kva[idx])
			pablo_interface_irta_kva_unmap(pii, idx);

		if (pii->dma_buf[idx])
			pablo_interface_irta_dma_buf_detach(pii, idx);
	}
}

int pablo_interface_irta_open(struct pablo_interface_irta *pii, struct is_device_ischain *idi)
{
	flush_all_buf_type(pii);
	pii->icpu_adt = pablo_get_icpu_adt();
	pii->idi = idi;

	return 0;
}
KUNIT_EXPORT_SYMBOL(pablo_interface_irta_open);

int pablo_interface_irta_close(struct pablo_interface_irta *pii)
{
	flush_all_buf_type(pii);

	return 0;
}
KUNIT_EXPORT_SYMBOL(pablo_interface_irta_close);

struct pablo_interface_irta *pablo_interface_irta_get(u32 instance)
{
	if (!itf_irta)
		return NULL;

	return &itf_irta[instance];
}
KUNIT_EXPORT_SYMBOL(pablo_interface_irta_get);

void pablo_interface_irta_set(struct pablo_interface_irta *pii)
{
	itf_irta = pii;
}
KUNIT_EXPORT_SYMBOL(pablo_interface_irta_set);

int pablo_interface_irta_probe(void)
{
	int i;

	itf_irta = kvzalloc(array_size(sizeof(struct pablo_interface_irta), IS_STREAM_COUNT),
				GFP_KERNEL);
	if (!itf_irta) {
		err("failed to allocate itf_irta\n");
		return -ENOMEM;
	}

	for (i = 0; i < IS_STREAM_COUNT; i++)
		itf_irta[i].instance = i;

	return 0;
}
KUNIT_EXPORT_SYMBOL(pablo_interface_irta_probe);

int pablo_interface_irta_remove(void)
{
	kvfree(itf_irta);
	itf_irta = NULL;

	return 0;
}
KUNIT_EXPORT_SYMBOL(pablo_interface_irta_remove);
