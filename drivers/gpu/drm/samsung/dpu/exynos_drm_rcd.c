// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_rcd.c
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/component.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/dma-iommu.h>
#include <linux/iommu.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include <drm/drm_rect.h>
#include <decon_cal.h>
#include <exynos_drm_decon.h>
#include <exynos_drm_rcd.h>
#include <rcd_cal.h>
#include <regs-rcd.h>

#define RCD_BUF_SIZE_DEFAULT 1440 * 3200
struct rcd_device *rcd_drvdata[MAX_RCD_CNT];

static int rcd_log_level = 6;
module_param_named(dpu_rcd_log_level, rcd_log_level, int, 0600);
MODULE_PARM_DESC(dpu_rcd_log_level, "log level for rcd [default : 6]");

static int rcd_user_disable = 0;
module_param_named(dpu_rcd_disable, rcd_user_disable, int, 0600);
MODULE_PARM_DESC(dpu_rcd_disable, "disable RCD feature");

#define RCD_NAME "exynos-rcd"
#define rcd_info(fmt, ...) \
	dpu_pr_info(RCD_NAME, 0, rcd_log_level, fmt, ##__VA_ARGS__)

#define rcd_warn(fmt, ...) \
	dpu_pr_warn(RCD_NAME, 0, rcd_log_level, fmt, ##__VA_ARGS__)

#define rcd_err(fmt, ...) \
	dpu_pr_err(RCD_NAME, 0, rcd_log_level, fmt, ##__VA_ARGS__)

#define rcd_debug(fmt, ...) \
	dpu_pr_debug(RCD_NAME, 0, rcd_log_level, fmt, ##__VA_ARGS__)


static void rcd_update_image(struct rcd_device *rcd)
{
	struct rcd_params_info *p;
	u32 id;
	int i, j;

	rcd_debug("+\n");

	if (!rcd || !rcd->initialized)
		return;

	p = &rcd->param;
	id = rcd->id;

	memset((u8 *)rcd->buf_vaddr, 0xff, rcd->buf_size);

	for (i = 0; i < p->num_win; i++) {
		u32 *pos = p->win_pos[i];
		u8 *cursor = (u8 *)(rcd->buf_vaddr + pos[0] + pos[1] * p->total_width);
		u32 size = p->imgs[i].size;
		u8* buf = p->imgs[i].buf;
		for (j = 0; j <size; j += 2) {
			u8 val, len;
			val = *(buf + j);
			len = *(buf + j + 1);
			memset(cursor, val, len);
			cursor += len;
		}
	}

	if (rcd->state == RCD_STATE_OFF) {
		rcd_warn("%s, rcd state is off\n", __func__);
		return;
	}

	rcd_reg_set_config(id);

	rcd_debug("-\n");
}

static u32 rcd_rect_x2(struct rcd_rect *rect)
{
	return rect->x + rect->w;
}

static u32 rcd_rect_y2(struct rcd_rect *rect)
{
	return rect->y + rect->h;
}

void rcd_set_partial(struct rcd_device *rcd, const struct drm_rect *r)
{
	struct rcd_rect rect;
	struct rcd_params_info *param = &rcd->param;

	if (!rcd || !rcd->initialized) {
		pr_err("%s rcd is not ready\n", __func__);
		return;
	}

	if (rcd->state == RCD_STATE_OFF)
		return;

	rect.x = r->x1;
	rect.y = r->y1;
	rect.w = drm_rect_width(r);
	rect.h = drm_rect_height(r);

	rcd_reg_set_partial(rcd->id, rect);

	if (param->block_en) {
		struct drm_rect block;

		block.x1 = param->block_rect.x;
		block.y1 = param->block_rect.y;
		block.x2 = rcd_rect_x2(&param->block_rect);
		block.y2 = rcd_rect_y2(&param->block_rect);

		if (drm_rect_intersect(&block, r)) {
			int y = max(0, block.y1 - r->y1);
			int h = min(drm_rect_height(&block), block.y2 - r->y1);
			rcd_reg_set_block_mode(rcd->id, true,
				param->block_rect.x, y,
				param->block_rect.w, h);
			rcd_debug("update block, y:%d, h:%d\n", y, h);
		} else
			rcd_reg_set_block_mode(rcd->id, false, 0, 0, 0, 0);
	}

}

static void rcd_prepare(struct rcd_device *rcd)
{
	struct decon_device *decon;
	struct decon_config *cfg;

	if (!rcd || !rcd->initialized)
		return;

	if (rcd->state == RCD_STATE_ON)
		return;

	if (rcd_user_disable)
		return;

	decon = rcd->decon;
	if (!decon) {
		rcd_err("%s, decon is null\n", __func__);
		return;
	}
	cfg = &decon->config;

	rcd_debug("+\n");

	mutex_lock(&rcd->lock);
	rcd_reg_init(rcd->id);
	rcd_reg_set_config(rcd->id);
	enable_irq(rcd->irq);

	decon_reg_set_rcd_enable(decon->id, true);
	cfg->rcd_en = true;
	rcd->state = RCD_STATE_ON;
	mutex_unlock(&rcd->lock);

	rcd_debug("-\n");
}

static void rcd_unprepare(struct rcd_device *rcd)
{
	struct decon_device *decon;
	struct decon_config *cfg;

	if (!rcd || !rcd->initialized)
		return;

	if (rcd->state == RCD_STATE_OFF)
		return;

	decon = rcd->decon;
	if (!decon) {
		rcd_err("decon is NULL\n");
		return;
	}
	cfg = &decon->config;

	rcd_debug("%s +\n", __func__);

	mutex_lock(&rcd->lock);
	rcd_reg_deinit(rcd->id, 1);
	disable_irq(rcd->irq);
	rcd_reg_get_irq_and_clear(rcd->id);
	rcd->state = RCD_STATE_OFF;
	cfg->rcd_en = false;
	mutex_unlock(&rcd->lock);
}

struct exynos_rcd_funcs rcd_funcs = {
	.prepare = rcd_prepare,
	.update = rcd_update_image,
	.set_partial = rcd_set_partial,
	.unprepare = rcd_unprepare,
};

static unsigned int rcd_map_handle(struct rcd_device *rcd, struct device *dev)
{
	rcd_debug("+\n");

	rcd->attachment = dma_buf_attach(rcd->buf, dev);
	if (IS_ERR_OR_NULL(rcd->attachment)) {
		rcd_err("dma_buf_attach() failed: %ld\n",
				PTR_ERR(rcd->attachment));
		goto err_buf_map_attach;
	}

	rcd->sg_table = dma_buf_map_attachment(rcd->attachment, DMA_TO_DEVICE);
	if (IS_ERR_OR_NULL(rcd->sg_table)) {
		rcd_err("dma_buf_map_attachment() failed: %ld\n",
				PTR_ERR(rcd->sg_table));
		goto err_buf_map_attachment;
	}

	rcd->dma_addr = sg_dma_address(rcd->sg_table->sgl);
	if (IS_ERR_VALUE(rcd->dma_addr)) {
		rcd_err("sg_dma_address() failed: %pa\n", &rcd->dma_addr);
		goto err_iovmm_map;
	}

	rcd_debug("-\n");

	return rcd->buf->size;

err_iovmm_map:
err_buf_map_attachment:
err_buf_map_attach:
	return 0;
}

static irqreturn_t rcd_irq_handler(int irq, void *priv)
{
	struct rcd_device *rcd = priv;
	u32 irqs;

	spin_lock(&rcd->slock);

	if (rcd->state == RCD_STATE_OFF)
		goto irq_end;

	irqs = rcd_reg_get_irq_and_clear(rcd->id);

	if (irqs & DMA_RCD_CONFIG_ERR_MASK)
		rcd_err("rcd: config error irq occurs\n");

	if (irqs & DMA_RCD_READ_SLAVE_ERR_MASK)
		rcd_err("rcd: read slave error irq occurs\n");

	if (irqs & DMA_RCD_DEADLOCK_MASK)
		rcd_err("rcd: deadlock irq occurs\n");


irq_end:
	spin_unlock(&rcd->slock);
	return IRQ_HANDLED;
}

static int exynos_alloc_rcd_memory(struct rcd_device *rcd, struct device *dev)
{
	struct dma_heap *dma_heap;
	struct dma_buf_map map = DMA_BUF_MAP_INIT_VADDR(NULL);
	unsigned int ret;
	u32 size;

	rcd_debug("+\n");

	size = PAGE_ALIGN(rcd->buf_size);

	dma_heap = dma_heap_find("system-uncached");
	if (dma_heap) {
		rcd->buf = dma_heap_buffer_alloc(dma_heap, (size_t)size, 0, 0);
		dma_heap_put(dma_heap);
	} else {
		rcd_err("dma_heap_find() failed\n");
		goto err_share_dma_buf;
	}
	if (IS_ERR(rcd->buf)) {
		rcd_err("ion_alloc() failed\n");
		goto err_share_dma_buf;
	}

	ret = dma_buf_vmap(rcd->buf, &map);
	if (ret) {
		rcd_err("failed to vmap buffer [%x]\n", ret);
		goto err_share_dma_buf;
	}
	rcd->buf_vaddr = map.vaddr;
	memset((u8 *)rcd->buf_vaddr, 0xff, size);

	ret = rcd_map_handle(rcd, dev);
	if (!ret)
		goto err_map;

	rcd_info("alloated memory for rcd, size:0x%x\n", size);
	rcd_info("rcd start addr = 0x%x\n", (u32)rcd->dma_addr);

	rcd_debug("-\n");

	return 0;

err_map:
	dma_buf_put(rcd->buf);
err_share_dma_buf:
	return -ENOMEM;
}

static int rcd_init_resources(struct rcd_device *rcd, struct platform_device *pdev)
{
	struct resource *res;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		rcd_err("failed to get mem resource\n");
		return -ENOENT;
	}

	rcd_debug("res: start(0x%x), end(0x%x)\n", (u32)res->start, (u32)res->end);

	rcd->reg_base = devm_ioremap_resource(rcd->dev, res);
	if (IS_ERR_OR_NULL(rcd->reg_base)) {
		rcd_err("failed to remap rcd sfr region\n");
		return -EINVAL;
	}
	rcd_regs_desc_init(rcd->reg_base, "rcd", 0);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		rcd_err("failed to get rcd irq resource\n");
		return -ENOENT;
	}
	rcd_debug("rcd irq no = %lld\n", res->start);

	rcd->irq = res->start;
	ret = devm_request_irq(rcd->dev, res->start, rcd_irq_handler, 0, pdev->name, rcd);
	if (ret) {
		rcd_err("failed to install rcd irq\n");
		return -EINVAL;
	}
	disable_irq(rcd->irq);

	return 0;
}

void exynos_rcd_enable(struct rcd_device *rcd)
{
	const struct exynos_rcd_funcs *funcs;

	if (!rcd || !rcd->initialized)
		return;

	rcd_debug("+\n");

	funcs = rcd->funcs;
	if (funcs)
		funcs->prepare(rcd);

	rcd_debug("-\n");
}

void exynos_rcd_disable(struct rcd_device *rcd)
{
	const struct exynos_rcd_funcs *funcs;

	if (!rcd || !rcd->initialized)
		return;

	rcd_debug("+\n");

	funcs = rcd->funcs;
	if (funcs)
		funcs->unprepare(rcd);

	rcd_debug("-\n");
}

void rcd_dump(struct rcd_device *rcd)
{
	if (rcd->state == RCD_STATE_OFF)
		return;

	rcd_dma_dump_regs(rcd->id, rcd->reg_base);
}

struct rcd_device *exynos_rcd_register(struct decon_device *decon)
{
	struct device_node *np;
	struct rcd_device *rcd = rcd_drvdata[0];
	struct device *dev;
	int ret;
	u32 val = 1;

	rcd_info("+\n");

	if (!rcd) {
		rcd_err("rcd is NULL\n");
		return NULL;
	}

	if (!decon) {
		rcd_err("decon is NULL\n");
		return NULL;
	}
	dev = decon->dev;

	if (decon->id > 0 || !(decon->config.out_type & DECON_OUT_DSI)) {
		rcd_err("RCD is only supported for DSI interface\n");
		return NULL;
	}
	np = dev->of_node;

	if (decon->emul_mode) {
		rcd_info("emul-mode not supported\n");
		return NULL;
	}

	ret = of_property_read_u32(np, "rcd-overlay-enable", &val);

	if (ret == -EINVAL || (ret == 0 && val == 0)) {
		rcd_err("RCD is not enabled\n");
		return NULL;
	}

	if (of_property_read_u32(np, "rcd-buf-size", &rcd->buf_size)) {
		rcd_warn("failed to parse rcd buffer size. use default value\n");
		rcd->buf_size = RCD_BUF_SIZE_DEFAULT;
	}

	ret = exynos_alloc_rcd_memory(rcd, dev);
	if (ret) {
		rcd_err("failed to alloc memory\n");
		return NULL;
	}

	rcd->decon = decon;
	rcd->id = decon->id;
	rcd->initialized = true;

	rcd_info("Rounded Corner Display registered successfully\n");

	return rcd;
}

static int rcd_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rcd_device *rcd;
	int ret = 0;

	pr_info("%s+\n", __func__);

	rcd = devm_kzalloc(dev, sizeof(struct rcd_device), GFP_KERNEL);
	if (!rcd)
		return -ENOMEM;

	rcd->dev = dev;

	ret = rcd_init_resources(rcd, pdev);
	if (ret) {
		rcd_err("failed to init resources regs\n");
		goto err;
	}

	rcd->funcs = &rcd_funcs;
	rcd_drvdata[0] = rcd;
	spin_lock_init(&rcd->slock);
	mutex_init(&rcd->lock);

	platform_set_drvdata(pdev, rcd);
	rcd->state = RCD_STATE_OFF;

	pr_info("%s-\n", __func__);
	rcd_info("rcd has been probed\n");

	return 0;

err:
	kfree(rcd);

	return ret;
}

static int rcd_remove(struct platform_device *pdev)
{
	rcd_info("driver unloaded\n");
	return 0;
}

static const struct of_device_id rcd_of_match[] = {
	{.compatible = "samsung,exynos-rcd"},
	{},
};
MODULE_DEVICE_TABLE(of, rcd_of_match);

struct platform_driver rcd_driver = {
	.probe = rcd_probe,
	.remove = rcd_remove,
	.driver = {
		.name = "exynos-rcd",
		.owner = THIS_MODULE,
		.of_match_table = rcd_of_match,
	},
};

MODULE_DESCRIPTION("Samsung EXYNOS RCD driver");
MODULE_LICENSE("GPL");
