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

int pablo_interface_irta_dma_buf_attach(struct pablo_interface_irta *pii,
				enum interface_irta_buf_type type, int fd)
{
	int ret;

	if (fd < 0) {
		err("fd is invalid (type: %d, fd: %d)", type, fd);
		return -EINVAL;
	}

	pii->dma_buf[type] = dma_buf_get(fd);
	if (IS_ERR(pii->dma_buf[type])) {
		err("Failed to get dmabuf. fd %d ret %ld", fd, PTR_ERR(pii->dma_buf[type]));
		return -EINVAL;
	}

	pii->attachment[type] = dma_buf_attach(pii->dma_buf[type], pablo_itf_get_icpu_dev());
	if (IS_ERR(pii->attachment[type])) {
		ret = -ENOMEM;
		goto err_attach;
	}

	pii->sgt[type] = dma_buf_map_attachment(pii->attachment[type], DMA_BIDIRECTIONAL);
	if (IS_ERR(pii->sgt[type])) {
		ret = -ENOMEM;
		goto err_map_attachment;
	}

	pii->dva[type] = sg_dma_address(pii->sgt[type]->sgl);
	if (!pii->dva[type]) {
		ret = -ENOMEM;
		goto err_address;
	}

	pii->dma_buf_fd[type] = fd;

	return 0;

err_address:
	dma_buf_unmap_attachment(pii->attachment[type], pii->sgt[type], DMA_BIDIRECTIONAL);

err_map_attachment:
	dma_buf_detach(pii->dma_buf[type], pii->attachment[type]);

err_attach:
	dma_buf_put(pii->dma_buf[type]);

	pii->dva[type] = 0;
	pii->sgt[type] = NULL;
	pii->attachment[type] = NULL;
	pii->dma_buf[type] = NULL;
	pii->dma_buf_fd[type] = 0;

	return ret;
}
KUNIT_EXPORT_SYMBOL(pablo_interface_irta_dma_buf_attach);

int pablo_interface_irta_dma_buf_detach(struct pablo_interface_irta *pii, enum interface_irta_buf_type type)
{
	if(!pii->dma_buf[type]) {
		err("dma_buf is NULL(type: %d)", type);
		return -EINVAL;
	}

	dma_buf_unmap_attachment(pii->attachment[type], pii->sgt[type], DMA_BIDIRECTIONAL);
	dma_buf_detach(pii->dma_buf[type], pii->attachment[type]);
	dma_buf_put(pii->dma_buf[type]);

	pii->dva[type] = 0;
	pii->sgt[type] = NULL;
	pii->attachment[type] = NULL;
	pii->dma_buf[type] = NULL;
	pii->dma_buf_fd[type] = 0;

	return 0;
}
KUNIT_EXPORT_SYMBOL(pablo_interface_irta_dma_buf_detach);

int pablo_interface_irta_kva_map(struct pablo_interface_irta *pii, enum interface_irta_buf_type type)
{
	pii->kva[type] = pkv_dma_buf_vmap(pii->dma_buf[type]);
	if (!pii->kva[type]) {
		err("kva is invalid(type: %d, kva: %p)", type, pii->kva[type]);
		return -ENOMEM;
	}

	return 0;
}
KUNIT_EXPORT_SYMBOL(pablo_interface_irta_kva_map);

int pablo_interface_irta_kva_unmap(struct pablo_interface_irta *pii, enum interface_irta_buf_type type)
{
	if(!pii->dma_buf[type]) {
		err("dma_buf is NULL(type: %d)", type);
		return -EINVAL;
	}

	if(!pii->kva[type])
		return 0;

	pkv_dma_buf_vunmap(pii->dma_buf[type], pii->kva[type]);

	pii->kva[type] = NULL;

	return 0;
}
KUNIT_EXPORT_SYMBOL(pablo_interface_irta_kva_unmap);

static void check_valid_fd(struct pablo_interface_irta *pii,
			enum interface_irta_buf_type type, int fd)
{
	int prev_fd = pii->dma_buf_fd[type];

	if (pii->dma_buf[type]) {
		if (prev_fd == fd)
			warn("fd is same(type: %d)", type);
		else
			warn("fd is not empty(type: %d)", type);

		if (pii->kva[type])
			pablo_interface_irta_kva_unmap(pii, type);

		pablo_interface_irta_dma_buf_detach(pii, type);
	}
}

int pablo_interface_irta_result_buf_set(struct pablo_interface_irta *pii, int fd)
{
	int ret;
	enum interface_irta_buf_type type = ITF_IRTA_BUF_TYPE_RESULT;
	struct pablo_crta_buf_info buf_info = { 0, };

	check_valid_fd(pii, type, fd);

	ret = pablo_interface_irta_dma_buf_attach(pii, type, fd);
	if (ret)
		return ret;

	ret = pablo_interface_irta_kva_map(pii, type);
	if (ret)
		goto err_save_kva;

	buf_info.id = 0;
	buf_info.dva = pii->dva[type];
	buf_info.kva = pii->kva[type];
	buf_info.size = pii->dma_buf[type]->size;
	CALL_ADT_MSG_OPS(pii->icpu_adt, send_msg_put_buf, pii->instance,
			PABLO_BUFFER_RTA, &buf_info);

	return 0;

err_save_kva:
	pablo_interface_irta_dma_buf_detach(pii, type);

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
	enum interface_irta_buf_type type;

	for (type = 0; type < ITF_IRTA_BUF_TYPE_MAX; type++) {
		if (pii->kva[type])
			pablo_interface_irta_kva_unmap(pii, type);

		if (pii->dma_buf[type])
			pablo_interface_irta_dma_buf_detach(pii, type);
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
