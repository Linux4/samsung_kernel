// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2023 MediaTek Inc.

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>

#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/irqflags.h>
#include <linux/timekeeping.h>

#include "mtk_ccu_common.h"

static inline unsigned int mtk_ccu_mstojiffies(unsigned int Ms)
{
	return ((Ms * HZ + 512) >> 10);
}

void mtk_ccu_memclr(void *dst, int len)
{
	int i = 0;
	uint32_t *dstPtr = (uint32_t *)dst;

	for (i = 0; i < len/4; i++)
		writel(0, dstPtr + i);
}

void mtk_ccu_memcpy(void *dst, const void *src, uint32_t len)
{
	int i, copy_len;
	uint32_t data = 0;
	uint32_t align_data = 0;

	for (i = 0; i < len/4; ++i)
		writel(*((uint32_t *)src+i), (uint32_t *)dst+i);

	if ((len % 4) != 0) {
		copy_len = len & ~(0x3);
		for (i = 0; i < 4; ++i) {
			if (i < (len%4)) {
				data = *((char *)src + copy_len + i);
				align_data += data << (8 * i);
			}
		}
		writel(align_data, (uint32_t *)dst + len/4);
	}
}

struct mtk_ccu_mem_info *mtk_ccu_get_meminfo(struct mtk_ccu *ccu,
	enum mtk_ccu_buffer_type type)
{
	if (type >= MTK_CCU_BUF_MAX)
		return NULL;
	else
		return &ccu->buffer_handle[type].meminfo;
}

MODULE_DESCRIPTION("MTK CCU Rproc Driver");
MODULE_LICENSE("GPL");
