// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung DMA register setting mode
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <drm/drm_atomic.h>
#include <drm/drm_fourcc.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/irq.h>
#include <linux/dma-buf.h>
#include <soc/samsung/exynos-smc.h>
#include <linux/dma-heap.h>
#include <linux/iommu.h>
#include <linux/dma-iommu.h>
#include <linux/pm_runtime.h>

#include <exynos_drm_sfr_dma.h>
#include <cal_common/dpp_cal.h>
#include <soc/samsung/exynos/debug-snapshot.h>

struct dpuf_dma {
	u32 id;
	int irq;
	bool initialized;
	bool enabled;
	bool dma_switch;
	atomic_t req_cnt;
	u16 req_flag;
	u64 *data;
	dma_addr_t addr;
};

struct dpuf_dma dma_private[MAX_DPUF_CNT];

static bool __is_enabled(int id)
{
	return (dma_private[id].initialized && dma_private[id].enabled);
}

static struct dpuf_dma *get_dpuf_data(int id)
{
	return __is_enabled(id) ? &dma_private[id] : NULL;
}

static void exynos_drm_sfr_dma_reset_config(struct dpuf_dma *pdma)
{
	atomic_set(&pdma->req_cnt, 0);
}

void exynos_drm_sfr_dma_mode_switch(bool en)
{
	struct dpuf_dma *pdma = &dma_private[0];

	pdma->dma_switch = en;
}

bool exynos_drm_sfr_dma_is_enabled(int id)
{
	int dpuf_id = id / DPP_PER_DPUF;

	return dma_private[dpuf_id].enabled;
}

static void exynos_drm_sfr_dma_enable_sync(void)
{
	int dpuf;
	struct dpuf_dma *pdma;
	bool dma_switch = dma_private[0].dma_switch;

	for (dpuf = 0; dpuf < MAX_DPUF_CNT; ++dpuf) {
		pdma = &dma_private[dpuf];
		pdma->enabled = dma_switch;
	}
}

static void __dump_dataspace(void)
{
	struct dpuf_dma *pdma = NULL;
	int dpuf, req_cnt, idx;

	for (dpuf = 0; dpuf < MAX_DPUF_CNT; ++dpuf) {
		u16 sfr = sfr_dma_reg_get_sfr_bitmask();

		pdma = get_dpuf_data(dpuf);
		if (!pdma || !pdma->req_flag)
			continue;

		sfr &= pdma->req_flag;
		if (!sfr)
			continue;

		req_cnt = atomic_read(&pdma->req_cnt);
		if (req_cnt == 0)
			continue;

		pr_err("====== dpuf%d : requested %d =====\n", dpuf, req_cnt);
		for (idx = 0; idx < req_cnt; ++idx) {
			pr_err("[%04d] 0x%16llx\n", idx, *(pdma->data + idx));
		}
	}
}

/* dummy func */
__weak void sfr_dma_reg_start_update(int id, dma_addr_t addr, int cnt, u32 en)
{
	pr_debug("not supported\n");
}

__weak void sfr_dma_reg_set_irq_mask_all(u32 id, bool en)
{
	pr_debug("not supported\n");
}

__weak void sfr_dma_reg_set_irq_enable(u32 id, bool en)
{
	pr_debug("not supported\n");
}

__weak u32 sfr_dma_get_irq_and_clear(int id)
{
	return 0;
}

__weak int sfr_dma_reg_arrange_data_format(u32 val, enum dpp_regs_type type, u64 *data)
{
	return -ESRCH;
}

__weak u16 sfr_dma_reg_get_sfr_bitmask(void)
{
	return 0;
}

__weak u16 sfr_dma_reg_get_layer_base_addr(enum dpp_regs_type type, int layer)
{
	return 0;
}

__weak int sfr_dma_reg_wait_framedone(int dpuf, u32 wait_ms)
{
	return 0;
}

int exynos_drm_sfr_dma_update(void)
{
	struct dpuf_dma *pdma = NULL;
	int dpuf, req_cnt = 0;
	int ret;

	for (dpuf = 0; dpuf < MAX_DPUF_CNT; ++dpuf) {
		u16 sfr = sfr_dma_reg_get_sfr_bitmask();

		pdma = get_dpuf_data(dpuf);
		if (!pdma || !pdma->req_flag)
			continue;

		sfr &= pdma->req_flag;
		if (!sfr)
			continue;

		req_cnt = atomic_read(&pdma->req_cnt);
		if (req_cnt == 0)
			continue;

		exynos_drm_sfr_dma_irq_enable(dpuf, true);
		/*
		 * HDD said that dataspace needs to be sorted before updating.
		 * However, sorting complexity was bigger than increase in hw tick.
		 */
		sfr_dma_reg_start_update(dpuf, pdma->addr, req_cnt, true);
		ret = sfr_dma_reg_wait_framedone(dpuf, 10);
		if (ret < 0) {
			pr_err("%s: dpuf%d: SFR update timedout\n", __func__, dpuf);
			__dump_dataspace();
			dbg_snapshot_expire_watchdog();
			return -EPIPE;
		}
		exynos_drm_sfr_dma_irq_enable(dpuf, false);

		exynos_drm_sfr_dma_reset_config(pdma);
	}
	exynos_drm_sfr_dma_enable_sync();

	return 0;
}

int exynos_drm_sfr_dma_request(int id, u32 offset, u32 val, enum dpp_regs_type type)
{
	int dpuf_id = id / DPP_PER_DPUF;
	struct dpuf_dma *pdma = NULL;
	u64 data;
	int idx;

	pdma = get_dpuf_data(dpuf_id);
	if (!pdma || type == REGS_VOTF || type >= REGS_DPP_TYPE_MAX)
		goto exit;

	offset += sfr_dma_reg_get_layer_base_addr(type, id);
	data = (u64)offset;
	idx = atomic_read(&pdma->req_cnt);
	pdma->req_flag |= sfr_dma_reg_get_sfr_bitmask();

	if (sfr_dma_reg_arrange_data_format(val, type, &data) < 0)
		goto exit;

	pdma->data[idx] = data;
	atomic_inc(&pdma->req_cnt);

	pr_debug("[%d] dpuf:%d, ch:%d, offset=0x%08x, val=0x%08x, data=0x%16llx\n",
			idx, dpuf_id, id, offset, val, *(pdma->data + idx));

	return 0;
exit:
	return -EINVAL;
}

static int drm_dma_alloc_map_buf(struct device *dev, struct dpuf_dma *dma, size_t size)
{
	struct platform_device *pdev;
	struct dma_buf *buf;
	struct dma_heap *dma_heap;
	struct dma_buf_map map = DMA_BUF_MAP_INIT_VADDR(NULL);
	struct dma_buf_attachment *attachment;
	struct sg_table *sg_table;
	dma_addr_t dma_addr;
	int ret;

	pr_debug("%s +\n", __func__);

	size = ALIGN(size, DMA_DATA_SIZE);

	dma_heap = dma_heap_find("system-uncached");
	if (IS_ERR_OR_NULL(dma_heap)) {
		pr_err("dma_heap_find() failed [%x]\n", dma_heap);
		return -ENODEV;
	}

	buf = dma_heap_buffer_alloc(dma_heap, (size_t)size, 0, 0);
	dma_heap_put(dma_heap);
	if (IS_ERR_OR_NULL(buf)) {
		pr_err("dma_buf allocation is failed [%x]\n", buf);
		return PTR_ERR(buf);
	}

	ret = dma_buf_vmap(buf, &map);
	if (ret) {
		pr_err("failed to vmap buffer [%x]\n", ret);
		return -EINVAL;
	}

	attachment = dma_buf_attach(buf, dev);
	if (IS_ERR_OR_NULL(attachment)) {
		pr_err("failed to attach dma_buf [%x]\n", attachment);
		return -EINVAL;
	}

	sg_table = dma_buf_map_attachment(attachment, DMA_TO_DEVICE);
	if (IS_ERR_OR_NULL(sg_table)) {
		pr_err("failed to map attachment [%x]\n", sg_table);
		return -EINVAL;
	}

	dma_addr = sg_dma_address(sg_table->sgl);
	if (IS_ERR_VALUE(dma_addr)) {
		pr_err("failed to map iovmm [%x]\n", dma_addr);
		return -EINVAL;
	}

	pdev = container_of(dev, struct platform_device, dev);
	dma->data = (u64 *)map.vaddr;
	dma->addr = dma_addr;

	pr_info("DMA(dev:%s) vaddr:%p paddr:0x%llx\n", pdev->name, map.vaddr, dma_addr);

	return 0;
}

static irqreturn_t dpuf_irq_handler(int irq, void *priv)
{
	struct dpuf_dma *pdma = priv;
	u32 irqs;

	irqs = sfr_dma_get_irq_and_clear(pdma->id);

	return IRQ_HANDLED;
}

int exynos_drm_sfr_dma_irq_enable(u32 dpuf, bool en)
{
	struct dpuf_dma *pdma = NULL;

	pdma = get_dpuf_data(dpuf);
	if (!pdma)
		return -EINVAL;

	if (en) {
		sfr_dma_reg_set_irq_mask_all(pdma->id, en);
		sfr_dma_reg_set_irq_enable(pdma->id, en);
	} else
		sfr_dma_reg_set_irq_enable(pdma->id, en);

	return 0;
}

int exynos_drm_sfr_dma_initialize(struct device *dev, size_t size, bool en)
{
	struct device_node *np = dev->of_node;
	struct dpuf_dma *pdma;
	int ret, dpuf;

	of_property_read_u32(np, "dpuf,id", &dpuf);
	pdma = &dma_private[dpuf];

	ret = drm_dma_alloc_map_buf(dev, pdma, size);
	if (ret < 0) {
		pdma->initialized = false;
		pdma->enabled = false;
		goto out;
	} else {
		pdma->id = dpuf;
		pdma->initialized = true;
		exynos_drm_sfr_dma_reset_config(pdma);
	}

	pdma->enabled = en;
	pdma->dma_switch = en;

	pdma->irq = of_irq_get_byname(np, "sfr_dma");

	if (pdma->enabled) {
		if (dpuf == 0)
			ret = devm_request_irq(dev, pdma->irq,
					dpuf_irq_handler, 0, "CGCTRL0", pdma);
		else
			ret = devm_request_irq(dev, pdma->irq,
					dpuf_irq_handler, 0, "CGCTRL1", pdma);
		if (ret) {
			pr_err("failed to install SFR_DMA irq\n");
			goto out;
		}
		disable_irq(pdma->irq);
	} else
		ret = devm_request_irq(dev, pdma->irq, NULL, 0, "CGCTRLSFR", pdma);

	pr_info("%s:DMA vaddr:%p paddr:0x%llx irq:%d\n", __func__,
				pdma->data, pdma->addr,	pdma->irq);

out:
	return ret;
}
