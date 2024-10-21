// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Dennis YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */

#include <linux/component.h>
#include <linux/dma-mapping.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/math64.h>
#include <soc/mediatek/smi.h>

#include "mtk-mml-driver.h"
#include "mtk-mml-tile.h"
#include "mtk-mml-sys.h"
#include "mtk-mml-mmp.h"
#include "mtk-mml-dle-adaptor.h"
#include "tile_driver.h"
#include "tile_mdp_func.h"

#define mrg_msg(fmt, args...) \
do { \
	if (mtk_mml_msg || mml_rrot_msg) { \
		_mml_log("[merge]" fmt, ##args); \
	} \
} while (0)

/* MERGE register offset */
#define VPP_MERGE_ENABLE		0x000
#define VPP_MERGE_SHADOW_CTRL		0x008
#define VPP_MERGE_CFG_0			0x010
#define VPP_MERGE_CFG_1			0x014
#define VPP_MERGE_CFG_4			0x020
#define VPP_MERGE_CFG_12		0x040
#define VPP_MERGE_CFG_24		0x070
#define VPP_MERGE_CFG_25		0x074
#define VPP_MERGE_CFG_26		0x078
#define VPP_MERGE_CFG_27		0x07c
#define VPP_MERGE_MON_0			0x100
#define VPP_MERGE_MON_1			0x104
#define VPP_MERGE_MON_2			0x108
#define VPP_MERGE_MON_4			0x110
#define VPP_MERGE_MON_5			0x114
#define VPP_MERGE_MON_6			0x118
#define VPP_MERGE_MON_8			0x120
#define VPP_MERGE_MON_9			0x124
#define VPP_MERGE_MON_10		0x128

#define MERGE_LABEL_TOTAL		0

struct merge_data {
};

static const struct merge_data mt6989_merge_data = {
};

struct mml_comp_merge {
	struct mml_comp comp;
	const struct merge_data *data;
};

struct merge_frame_data {
};

static inline struct mml_comp_merge *comp_to_merge(struct mml_comp *comp)
{
	return container_of(comp, struct mml_comp_merge, comp);
}

static inline struct merge_frame_data *merge_frm_data(struct mml_comp_config *ccfg)
{
	return ccfg->data;
}

static s32 merge_prepare(struct mml_comp *comp, struct mml_task *task,
			 struct mml_comp_config *ccfg)
{
	ccfg->data = kzalloc(sizeof(struct merge_frame_data), GFP_KERNEL);
	if (!ccfg->data)
		return -ENOMEM;

	return 0;
}

s32 merge_tile_prepare(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg,
	struct tile_func_block *func,
	union mml_tile_data *data)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_size *frame_in = &cfg->frame_in;
	struct mml_crop *crop = &cfg->frame_in_crop[0];

	func->enable_flag = true;

	if (cfg->info.dest_cnt == 1 &&
	     (crop->r.width != frame_in->width ||
	    crop->r.height != frame_in->height)) {
		u32 in_crop_w, in_crop_h;

		in_crop_w = crop->r.width;
		in_crop_h = crop->r.height;
		if (in_crop_w + crop->r.left > frame_in->width)
			in_crop_w = frame_in->width - crop->r.left;
		if (in_crop_h + crop->r.top > frame_in->height)
			in_crop_h = frame_in->height - crop->r.top;
		func->full_size_x_in = in_crop_w;
		func->full_size_y_in = in_crop_h;
	} else {
		func->full_size_x_in = frame_in->width;
		func->full_size_y_in = frame_in->height;
	}
	func->full_size_x_out = func->full_size_x_in;
	func->full_size_y_out = func->full_size_y_in;

	return 0;

}

static const struct mml_comp_tile_ops merge_tile_ops = {
	.prepare = merge_tile_prepare,
};

static u32 merge_get_label_count(struct mml_comp *comp, struct mml_task *task,
				 struct mml_comp_config *ccfg)
{
	return MERGE_LABEL_TOTAL;
}

static s32 merge_config_frame(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;

	cmdq_pkt_write(pkt, NULL, base_pa + VPP_MERGE_ENABLE, 0x1, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + VPP_MERGE_SHADOW_CTRL,
		((cfg->shadow ? 0 : BIT(1)) << 1) | 0x1, U32_MAX);

	if (cfg->rrot_dual) {
		/* bit[7:0] 8'd24:  CFG_2PI_2PI_2PO_0PO_MERGE */
		cmdq_pkt_write(pkt, NULL, base_pa + VPP_MERGE_CFG_12, 24, U32_MAX);
	} else {
		/* bit[7:0] 8'd08 : CFG_2PI_0PI_2PO_0PO_BUF_MODE */
		cmdq_pkt_write(pkt, NULL, base_pa + VPP_MERGE_CFG_12, 8, U32_MAX);
	}

	return 0;
}

static s32 merge_config_tile(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;
	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);
	u32 width = tile->in.xe - tile->in.xs + 1;
	u32 height = tile->in.ye - tile->in.ys + 1;
	u32 input0, input1;

	input0 = (cfg->rrot_out[0].height << 16) | cfg->rrot_out[0].width;
	input1 = (cfg->rrot_out[1].height << 16) | cfg->rrot_out[1].width;

	/* vpp_merge_i_0_fwidth, vpp_merge_i_0_fheight */
	cmdq_pkt_write(pkt, NULL, base_pa + VPP_MERGE_CFG_0, input0, U32_MAX);
	/* vpp_merge_i_1_fwidth, vpp_merge_i_1_fheight */
	cmdq_pkt_write(pkt, NULL, base_pa + VPP_MERGE_CFG_1, input1, U32_MAX);
	/* vpp_merge_o_0_fwidth, vpp_merge_o_0_fheight */
	cmdq_pkt_write(pkt, NULL, base_pa + VPP_MERGE_CFG_4, (height << 16) | width, U32_MAX);

	/* vpp_merge_sram_0_fwidth, vpp_merge_sram_0_fheight */
	cmdq_pkt_write(pkt, NULL, base_pa + VPP_MERGE_CFG_24, input0, U32_MAX);
	/* vpp_merge_sram_1_fwidth, vpp_merge_sram_1_fheight */
	if (cfg->rrot_dual) {
		/* config sram as input 1 size */
		cmdq_pkt_write(pkt, NULL, base_pa + VPP_MERGE_CFG_25, input1, U32_MAX);
	} else {
		/* config sram as input 0 size, since buf mode use both sram */
		cmdq_pkt_write(pkt, NULL, base_pa + VPP_MERGE_CFG_25, input0, U32_MAX);
	}

	/* vpp_merge_merge_0_fwidth, vpp_merge_merge_0_fheight */
	cmdq_pkt_write(pkt, NULL, base_pa + VPP_MERGE_CFG_26, input0, U32_MAX);
	/* vpp_merge_merge_1_fwidth, vpp_merge_merge_1_fheight */
	cmdq_pkt_write(pkt, NULL, base_pa + VPP_MERGE_CFG_27, input1, U32_MAX);

	/* qos accumulate tile pixel */
	cache_max_sz(cache, width, height);

	if (cfg->rrot_out[0].width + cfg->rrot_out[1].width == width &&
		cfg->rrot_out[0].height == height &&
		((cfg->rrot_dual && cfg->rrot_out[1].height == height) || !cfg->rrot_dual))
		mrg_msg("[merge]in0 %u %u in1 %u %u out %u %u full %u %u",
			cfg->rrot_out[0].width, cfg->rrot_out[0].height,
			cfg->rrot_out[1].width, cfg->rrot_out[1].height,
			width, height,
			cfg->frame_in.width, cfg->frame_in.height);
	else
		mml_err("[merge]in0 %u %u in1 %u %u out %u %u full %u %u size not match!",
			cfg->rrot_out[0].width, cfg->rrot_out[0].height,
			cfg->rrot_out[1].width, cfg->rrot_out[1].height,
			width, height,
			cfg->frame_in.width, cfg->frame_in.height);

	return 0;
}

static const struct mml_comp_config_ops merge_cfg_ops = {
	.prepare = merge_prepare,
	.get_label_count = merge_get_label_count,
	.frame = merge_config_frame,
	.tile = merge_config_tile,
};

static void merge_debug_dump(struct mml_comp *comp)
{
	void __iomem *base = comp->base;
	u32 shadow_ctrl, value[4];

	mml_err("merge component %u %s dump:", comp->id, comp->name ? comp->name : "");

	/* Enable shadow read working */
	shadow_ctrl = readl(base + VPP_MERGE_SHADOW_CTRL);
	shadow_ctrl |= 0x4;
	writel(shadow_ctrl, base + VPP_MERGE_SHADOW_CTRL);

	value[0] = readl(base + VPP_MERGE_ENABLE);
	mml_err("VPP_MERGE_ENABLE %#x", value[0]);

	value[0] = readl(base + VPP_MERGE_CFG_0);
	value[1] = readl(base + VPP_MERGE_CFG_1);
	value[2] = readl(base + VPP_MERGE_CFG_4);
	value[3] = readl(base + VPP_MERGE_CFG_12);
	mml_err(
		"VPP_MERGE_CFG_0 %#010x VPP_MERGE_CFG_1 %#010x VPP_MERGE_CFG_4 %#010x VPP_MERGE_CFG_12 %#010x",
		value[0], value[1], value[2], value[3]);

	value[0] = readl(base + VPP_MERGE_CFG_24);
	value[1] = readl(base + VPP_MERGE_CFG_25);
	value[2] = readl(base + VPP_MERGE_CFG_26);
	value[3] = readl(base + VPP_MERGE_CFG_27);
	mml_err(
		"VPP_MERGE_CFG_24 %#010x VPP_MERGE_CFG_25 %#010x VPP_MERGE_CFG_26 %#010x VPP_MERGE_CFG_27 %#010x",
		value[0], value[1], value[2], value[3]);

	value[0] = readl(base + VPP_MERGE_MON_0);
	value[1] = readl(base + VPP_MERGE_MON_1);
	value[2] = readl(base + VPP_MERGE_MON_2);
	mml_err(
		"mon_i_0 frame done %u underflow %u overflow %u valid %u ready %u cout %#010x size %#010x",
		value[0] & 0x1, (value[0] >> 4) & 0x1, (value[0] >> 8) & 0x1,
		(value[0] >> 16) & 0x1, (value[0] >> 20) & 0x1, value[1], value[2]);

	value[0] = readl(base + VPP_MERGE_MON_4);
	value[1] = readl(base + VPP_MERGE_MON_5);
	value[2] = readl(base + VPP_MERGE_MON_6);
	mml_err(
		"mon_i_1 frame done %u underflow %u overflow %u valid %u ready %u cout %#010x size %#010x",
		value[0] & 0x1, (value[0] >> 4) & 0x1, (value[0] >> 8) & 0x1,
		(value[0] >> 16) & 0x1, (value[0] >> 20) & 0x1, value[1], value[2]);

	value[0] = readl(base + VPP_MERGE_MON_8);
	value[1] = readl(base + VPP_MERGE_MON_9);
	value[2] = readl(base + VPP_MERGE_MON_10);
	mml_err(
		"mon_o_0 frame done %u underflow %u overflow %u valid %u ready %u cout %#010x size %#010x",
		value[0] & 0x1, (value[0] >> 4) & 0x1, (value[0] >> 8) & 0x1,
		(value[0] >> 16) & 0x1, (value[0] >> 20) & 0x1, value[1], value[2]);
}

static void merge_reset(struct mml_comp *comp, struct mml_frame_config *cfg, u32 pipe)
{
}

static const struct mml_comp_debug_ops merge_debug_ops = {
	.dump = &merge_debug_dump,
	.reset = &merge_reset,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_merge *merge = dev_get_drvdata(dev);
	s32 ret;

	ret = mml_register_comp(master, &merge->comp);
	if (ret)
		dev_err(dev, "Failed to register mml component %s: %d\n",
			dev->of_node->full_name, ret);
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_merge *merge = dev_get_drvdata(dev);

	mml_unregister_comp(master, &merge->comp);
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static struct mml_comp_merge *dbg_probed_components[4];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_merge *priv;
	s32 ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = of_device_get_match_data(dev);

	ret = mml_comp_init(pdev, &priv->comp);
	if (ret) {
		dev_err(dev, "Failed to init mml component: %d\n", ret);
		return ret;
	}

	/* assign ops */
	priv->comp.tile_ops = &merge_tile_ops;
	priv->comp.config_ops = &merge_cfg_ops;
	priv->comp.debug_ops = &merge_debug_ops;

	dbg_probed_components[dbg_probed_count++] = priv;

	ret = component_add(dev, &mml_comp_ops);
	if (ret)
		dev_err(dev, "Failed to add component: %d\n", ret);

	return 0;
}

static int remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mml_comp_ops);
	return 0;
}

const struct of_device_id mml_merge_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6989-mml_merge",
		.data = &mt6989_merge_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_merge_driver_dt_match);

struct platform_driver mml_merge_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-merge",
		.owner = THIS_MODULE,
		.of_match_table = mml_merge_driver_dt_match,
	},
};

//module_platform_driver(mml_merge_driver);

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML MERGE driver");
MODULE_LICENSE("GPL");
