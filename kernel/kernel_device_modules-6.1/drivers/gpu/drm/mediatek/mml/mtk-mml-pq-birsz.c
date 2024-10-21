// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chris-YC Chen <chris-yc.chen@mediatek.com>
 */

#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <mtk_drm_ddp_comp.h>

#include "mtk-mml-color.h"
#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-dle-adaptor.h"
#include "mtk-mml-pq-birsz-fw.h"

#define BIRSZ_EN		0x000
#define BIRSZ_SEETING		0x004
#define BIRSZ_SIZE_IN		0x008
#define BIRSZ_SIZE_OUT		0x00c
#define BIRSZ_VERT_STEP		0x010
#define BIRSZ_HORI_STEP		0x014
#define BIRSZ_HORI_INT_OFST	0x018
#define BIRSZ_HORI_SUB_OFST	0x01c
#define BIRSZ_VERT_INT_OFST	0x020
#define BIRSZ_VERT_SUB_OFST	0x024
#define BIRSZ_CFG		0x028
#define BIRSZ_RESET		0x02c
#define BIRSZ_INTEN		0x030
#define BIRSZ_INTSTA		0x034
#define BIRSZ_STATUS		0x038
#define BIRSZ_INPUT_COUNT	0x03c
#define BIRSZ_OUTPUT_COUNT	0x040
#define BIRSZ_CHKSUM		0x044
#define BIRSZ_DUMMY_REG		0x048
#define BIRSZ_ATPG		0x04c
#define BIRSZ_SHADOW_CTRL	0x050
#define BIRSZ_DGB_SEL		0x054
#define BIRSZ_DBG_RDATA		0x058

struct birsz_data {
	u32 tile_width;
};

static const struct birsz_data mt6985_birsz_data = {
	.tile_width = 516,
};

struct mml_comp_birsz {
	struct mtk_ddp_comp ddp_comp;
	struct mml_comp comp;
	const struct birsz_data *data;
	bool ddp_bound;
};

/* meta data for each different frame config */
struct birsz_frame_data {
	struct birsz_fw_out fw_out;
};

static inline struct birsz_frame_data *birsz_frm_data(struct mml_comp_config *ccfg)
{
	return ccfg->data;
}

static inline struct mml_comp_birsz *comp_to_birsz(struct mml_comp *comp)
{
	return container_of(comp, struct mml_comp_birsz, comp);
}

static void birsz_first_6_taps(s32 out_start,
			       s32 out_end,
			       s32 coeff,
			       s32 precision,
			       s32 crop,
			       s32 crop_frac,
			       s32 in_max,
			       s32 *in_start,
			       s32 *in_end)
{
	s64 start, end;

	if (crop_frac < 0)
		crop_frac = -0xfffff;
	crop_frac = ((s64)crop_frac * precision) >> MML_SUBPIXEL_BITS;

	start = (s64)out_start * coeff + (s64)crop * precision + crop_frac;
	if (start <= (s64)3 * precision) {
		*in_start = 0;
	} else {
		start = start / precision - 3;
		if (!(start & 0x1))
			*in_start = (s32)start;
		else /* must be even */
			*in_start = (s32)start - 1;
	}

	end = (s64)out_end * coeff + (s64)crop * precision + crop_frac +
		(3 + 2) * precision;
	if (end > (s64)in_max * precision) {
		*in_end = in_max;
	} else {
		end = end / precision;
		if (end & 0x1)
			*in_end = (s32)end;
		else /* must be odd */
			*in_end = (s32)end + 1;
	}
}

static void birsz_second_6_taps(s32 in_start,
				s32 coeff,
				s32 precision,
				s32 crop,
				s32 crop_frac,
				s32 back_out_start,
				s32 *luma,
				s32 *luma_frac)
{
	s64 offset;

	if (crop_frac < 0)
		crop_frac = -0xfffff;
	crop_frac = ((s64)crop_frac * precision) >> MML_SUBPIXEL_BITS;

	/* cal offset & frac by fixed out_start */
	offset = (s64)back_out_start * coeff +
		(s64)crop * precision + crop_frac - (s64)in_start * precision;

	*luma = (s32)(offset / precision);
	*luma_frac = (s32)(offset - *luma * precision);

	if (*luma_frac < 0) {
		*luma -= 1;
		*luma_frac += precision;
	}
}

static void birsz_cal_tile(struct mml_tile_engine *ref_tile,
			   struct mml_tile_engine *tile,
			   struct birsz_frame_data *birsz_frm,
			   struct mml_frame_config *cfg)
{
	s32 in_x_left;
	s32 in_x_right;
	s32 out_x_left;
	s32 out_x_right;

	s32 in_y_top;
	s32 in_y_bottom;
	s32 luma;
	s32 luma_sub;

	out_x_left = ref_tile->in.xs;
	out_x_right = ref_tile->in.xe;

	mml_msg("%s out_x_right %d, out_x_left %d", __func__, out_x_right, out_x_left);

	birsz_first_6_taps(out_x_left,
		out_x_right,
		birsz_frm->fw_out.hori_step,
		birsz_frm->fw_out.precision,
		birsz_frm->fw_out.hori_int_ofst,
		birsz_frm->fw_out.hori_sub_ofst,
		cfg->info.seg_map.width - 1,
		&in_x_left,
		&in_x_right);

	if (in_x_left < 0)
		in_x_left = 0;
	if (in_x_right + 1 > cfg->info.seg_map.width)
		in_x_right = cfg->info.seg_map.width - 1;

	mml_msg("in_x_right %d, in_x_left %d, seg_map.width %d",
		in_x_right, in_x_left, cfg->info.seg_map.width);

	birsz_second_6_taps(in_x_left,
		birsz_frm->fw_out.hori_step,
		birsz_frm->fw_out.precision,
		birsz_frm->fw_out.hori_int_ofst,
		birsz_frm->fw_out.hori_sub_ofst,
		out_x_left,
		&luma,
		&luma_sub);

	tile->luma.x = (luma > 0) ? (u32)luma : 0x1ffff;
	tile->luma.x_sub = luma_sub;

	mml_msg("luma.x %#010x, luma.x_sub %#010x",
		tile->luma.x, tile->luma.x_sub);

	mml_msg("%s in.ye %d, in.ys %d", __func__, ref_tile->in.ye, ref_tile->in.ys);
	birsz_first_6_taps(ref_tile->in.ys,
		ref_tile->in.ye,
		birsz_frm->fw_out.vert_step,
		birsz_frm->fw_out.precision,
		birsz_frm->fw_out.vert_int_ofst,
		birsz_frm->fw_out.vert_sub_ofst,
		cfg->info.seg_map.height - 1,
		&in_y_top,
		&in_y_bottom);

	mml_msg("in_y_bottom %d, in_y_top %d", in_y_bottom, in_y_top);

	birsz_second_6_taps(in_y_top,
		birsz_frm->fw_out.vert_step,
		birsz_frm->fw_out.precision,
		birsz_frm->fw_out.vert_int_ofst,
		birsz_frm->fw_out.vert_sub_ofst,
		ref_tile->in.ys,
		&luma,
		&luma_sub);

	tile->luma.y = (luma > 0) ? (u32)luma : 0x1ffff;
	tile->luma.y_sub = luma_sub;
	mml_msg("luma.y %#010x, luma.y_sub %#010x", tile->luma.y, tile->luma.y_sub);

	tile->in.xs = in_x_left;
	tile->in.xe = in_x_right;
	tile->in.ys = in_y_top;
	tile->in.ye = in_y_bottom;
	tile->out.xs = out_x_left;
	tile->out.xe = out_x_right;
	tile->out.ys = ref_tile->in.ys;
	tile->out.ye = ref_tile->in.ye;
}

static s32 birsz_region_pq_bw(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg,
			      struct mml_tile_engine *ref_tile,
			      struct mml_tile_engine *tile)
{
	struct birsz_frame_data *birsz_frm = birsz_frm_data(ccfg);
	struct mml_frame_config *cfg = task->config;

	birsz_cal_tile(ref_tile, tile, birsz_frm, cfg);
	return 0;
}

static const struct mml_comp_tile_ops birsz_tile_ops = {
	.region_pq_bw = birsz_region_pq_bw,
};

static s32 birsz_prepare(struct mml_comp *comp, struct mml_task *task,
				   struct mml_comp_config *ccfg)
{
	struct birsz_frame_data *birsz_frm = NULL;
	const struct mml_frame_config *cfg = task->config;
	const struct mml_frame_data *src = &cfg->info.seg_map;
	const struct mml_frame_size *frame_out = &cfg->frame_out[0];
	struct birsz_fw_in fw_in;

	birsz_frm = kzalloc(sizeof(*birsz_frm), GFP_KERNEL);
	if (!birsz_frm)
		return -ENOMEM;
	ccfg->data = birsz_frm;

	fw_in.in_width = src->width;
	fw_in.in_height = src->height;
	fw_in.out_width = frame_out->width;
	fw_in.out_height = frame_out->height;

	birsz_fw(&fw_in, &birsz_frm->fw_out);
	birsz_frm->fw_out.hori_sub_ofst =
		((s64)birsz_frm->fw_out.hori_sub_ofst << MML_SUBPIXEL_BITS) /
		birsz_frm->fw_out.precision;
	birsz_frm->fw_out.vert_sub_ofst =
		((s64)birsz_frm->fw_out.vert_sub_ofst << MML_SUBPIXEL_BITS) /
		birsz_frm->fw_out.precision;
	return 0;
}

static void birsz_init(struct cmdq_pkt *pkt, const phys_addr_t base_pa)
{
	cmdq_pkt_write(pkt, NULL, base_pa + BIRSZ_EN, 0x1, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + BIRSZ_CFG, 0x2, U32_MAX);
	/* Enable shadow */
	cmdq_pkt_write(pkt, NULL, base_pa + BIRSZ_SHADOW_CTRL, 0x2, U32_MAX);
}

static s32 birsz_config_init(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	birsz_init(task->pkts[ccfg->pipe], comp->base_pa);
	return 0;
}

static s32 birsz_config_frame(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg)
{
	struct birsz_frame_data *birsz_frm = birsz_frm_data(ccfg);
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;

	mml_msg("%s is called", __func__);

	mml_msg("vert_step %#010x, hori_step %#010x",
		birsz_frm->fw_out.vert_step, birsz_frm->fw_out.hori_step);

	cmdq_pkt_write(pkt, NULL, base_pa + BIRSZ_VERT_STEP,
		       birsz_frm->fw_out.vert_step, 0x7ffff);
	cmdq_pkt_write(pkt, NULL, base_pa + BIRSZ_HORI_STEP,
		       birsz_frm->fw_out.hori_step, 0x7ffff);

	mml_msg("%s is end", __func__);
	return 0;
}

static s32 birsz_config_tile(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;

	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);

	u32 birsz_input_w = 0;
	u32 birsz_input_h = 0;
	u32 birsz_output_w = 0;
	u32 birsz_output_h = 0;

	mml_msg("%s idx[%d]", __func__, idx);

	birsz_input_w = tile->in.xe - tile->in.xs + 1;
	birsz_input_h = tile->in.ye - tile->in.ys + 1;
	birsz_output_w = tile->out.xe - tile->out.xs + 1;
	birsz_output_h = tile->out.ye - tile->out.ys + 1;

	mml_msg("%s in.xe %d, in.xs %d, in.ye %d, in.ys %d",
		__func__, tile->in.xe, tile->in.xs, tile->in.ye, tile->in.ys);
	mml_msg("%s out.xe %d, out.xs %d, out.ye %d, out.ys %d",
		__func__, tile->out.xe, tile->out.xs, tile->out.ye, tile->out.ys);
	mml_msg("%s luma.x %#010x, luma.x_sub %#010x, luma.y %#010x, luma.y_sub %#010x",
		__func__, tile->luma.x, tile->luma.x_sub, tile->luma.y, tile->luma.y_sub);

	cmdq_pkt_write(pkt, NULL, base_pa + BIRSZ_SIZE_IN,
		       (birsz_input_h << 16) + birsz_input_w, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + BIRSZ_SIZE_OUT,
		       (birsz_output_h << 16) + birsz_output_w, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + BIRSZ_HORI_INT_OFST,
		       tile->luma.x, 0x1ffff);
	cmdq_pkt_write(pkt, NULL, base_pa + BIRSZ_HORI_SUB_OFST,
		       tile->luma.x_sub, 0x7fff);
	cmdq_pkt_write(pkt, NULL, base_pa + BIRSZ_VERT_INT_OFST,
		       tile->luma.y, 0x1ffff);
	cmdq_pkt_write(pkt, NULL, base_pa + BIRSZ_VERT_SUB_OFST,
		       tile->luma.y_sub, 0x7fff);
	return 0;
}

static const struct mml_comp_config_ops birsz_cfg_ops = {
	.prepare = birsz_prepare,
	.init = birsz_config_init,
	.frame = birsz_config_frame,
	.tile = birsz_config_tile,
};

static const struct mml_comp_hw_ops birsz_hw_ops = {
	.clk_enable = &mml_comp_clk_enable,
	.clk_disable = &mml_comp_clk_disable,
};

static void birsz_debug_dump(struct mml_comp *comp)
{
	void __iomem *base = comp->base;
	u32 value[20];
	u32 debug[8];
	u32 shadow_ctrl;

	mml_err("birsz component %u dump:", comp->id);

	/* Enable shadow read working */
	shadow_ctrl = readl(base + BIRSZ_SHADOW_CTRL);
	shadow_ctrl |= 0x4;
	writel(shadow_ctrl, base + BIRSZ_SHADOW_CTRL);

	value[0] = readl(base + BIRSZ_EN);
	value[1] = readl(base + BIRSZ_SEETING);
	value[2] = readl(base + BIRSZ_SIZE_IN);
	value[3] = readl(base + BIRSZ_SIZE_OUT);
	value[4] = readl(base + BIRSZ_VERT_STEP);
	value[5] = readl(base + BIRSZ_HORI_STEP);
	value[6] = readl(base + BIRSZ_HORI_INT_OFST);
	value[7] = readl(base + BIRSZ_HORI_SUB_OFST);
	value[8] = readl(base + BIRSZ_VERT_INT_OFST);
	value[9] = readl(base + BIRSZ_VERT_SUB_OFST);
	value[10] = readl(base + BIRSZ_CFG);
	value[11] = readl(base + BIRSZ_RESET);
	value[12] = readl(base + BIRSZ_INTEN);
	value[13] = readl(base + BIRSZ_INTSTA);
	value[14] = readl(base + BIRSZ_STATUS);
	value[15] = readl(base + BIRSZ_INPUT_COUNT);
	value[16] = readl(base + BIRSZ_OUTPUT_COUNT);
	value[17] = readl(base + BIRSZ_CHKSUM);
	value[18] = readl(base + BIRSZ_DUMMY_REG);
	value[19] = readl(base + BIRSZ_ATPG);

	writel(0x1, base + BIRSZ_DGB_SEL);
	debug[0] = readl(base + BIRSZ_DBG_RDATA);
	writel(0x2, base + BIRSZ_DGB_SEL);
	debug[1] = readl(base + BIRSZ_DBG_RDATA);
	writel(0x3, base + BIRSZ_DGB_SEL);
	debug[2] = readl(base + BIRSZ_DBG_RDATA);
	writel(0x9, base + BIRSZ_DGB_SEL);
	debug[3] = readl(base + BIRSZ_DBG_RDATA);
	writel(0xa, base + BIRSZ_DGB_SEL);
	debug[4] = readl(base + BIRSZ_DBG_RDATA);
	writel(0xb, base + BIRSZ_DGB_SEL);
	debug[5] = readl(base + BIRSZ_DBG_RDATA);
	writel(0xd, base + BIRSZ_DGB_SEL);
	debug[6] = readl(base + BIRSZ_DBG_RDATA);
	writel(0xe, base + BIRSZ_DGB_SEL);
	debug[7] = readl(base + BIRSZ_DBG_RDATA);

	mml_err("BIRSZ_EN %#010x BIRSZ_SEETING %#010x",
		value[0], value[1]);
	mml_err("BIRSZ_SIZE_IN %#010x BIRSZ_SIZE_OUT %#010x",
		value[2], value[3]);
	mml_err("BIRSZ_VERT_STEP %#010x BIRSZ_HORI_STEP %#010x",
		value[4], value[5]);
	mml_err("BIRSZ_HORI_INT_OFST %#010x BIRSZ_HORI_SUB_OFST %#010x",
		value[6], value[7]);
	mml_err("BIRSZ_VERT_INT_OFST %#010x BIRSZ_VERT_SUB_OFST %#010x",
		value[8], value[9]);
	mml_err("BIRSZ_CFG %#010x BIRSZ_RESET %#010x",
		value[10], value[11]);
	mml_err("BIRSZ_INTEN %#010x BIRSZ_INTSTA %#010x BIRSZ_STATUS %#010x",
		value[12], value[13], value[14]);
	mml_err("BIRSZ_INPUT_COUNT %#010x BIRSZ_OUTPUT_COUNT %#010x",
		value[15], value[16]);
	mml_err("BIRSZ_CHKSUM %#010x BIRSZ_DUMMY_REG %#010x BIRSZ_ATPG %#010x",
		value[17], value[18], value[19]);
	mml_err("BIRSZ_DEBUG_1 %#010x BIRSZ_DEBUG_2 %#010x BIRSZ_DEBUG_3 %#010x",
		debug[0], debug[1], debug[2]);
	mml_err("BIRSZ_DEBUG_9 %#010x BIRSZ_DEBUG_10 %#010x BIRSZ_DEBUG_11 %#010x",
		debug[3], debug[4], debug[5]);
	mml_err("BIRSZ_DEBUG_13 %#010x BIRSZ_DEBUG_14 %#010x",
		debug[6], debug[7]);
}

static const struct mml_comp_debug_ops birsz_debug_ops = {
	.dump = &birsz_debug_dump,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_birsz *birsz = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	s32 ret;

	if (!drm_dev) {
		ret = mml_register_comp(master, &birsz->comp);
		if (ret)
			dev_err(dev, "Failed to register mml component %s: %d\n",
				dev->of_node->full_name, ret);
	} else {
		ret = mml_ddp_comp_register(drm_dev, &birsz->ddp_comp);
		if (ret)
			dev_err(dev, "Failed to register ddp component %s: %d\n",
				dev->of_node->full_name, ret);
		else
			birsz->ddp_bound = true;
	}
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_birsz *birsz = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	if (!drm_dev) {
		mml_unregister_comp(master, &birsz->comp);
	} else {
		mml_ddp_comp_unregister(drm_dev, &birsz->ddp_comp);
		birsz->ddp_bound = false;
	}
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static const struct mtk_ddp_comp_funcs ddp_comp_funcs = {
};

static struct mml_comp_birsz *dbg_probed_components[4];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_birsz *priv;
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
	priv->comp.tile_ops = &birsz_tile_ops;
	priv->comp.config_ops = &birsz_cfg_ops;
	priv->comp.hw_ops = &birsz_hw_ops;
	priv->comp.debug_ops = &birsz_debug_ops;

	dbg_probed_components[dbg_probed_count++] = priv;

	ret = component_add(dev, &mml_comp_ops);
	if (ret)
		dev_err(dev, "Failed to add component: %d\n", ret);

	return ret;
}

static int remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mml_comp_ops);
	component_del(&pdev->dev, &mml_comp_ops);
	return 0;
}

const struct of_device_id mml_pq_birsz_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6985-mml_birsz",
		.data = &mt6985_birsz_data,
	},
	{
		.compatible = "mediatek,mt6897-mml_birsz",
		.data = &mt6985_birsz_data,
	},
	{
		.compatible = "mediatek,mt6989-mml_birsz",
		.data = &mt6985_birsz_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_pq_birsz_driver_dt_match);

struct platform_driver mml_pq_birsz_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-pq-birsz",
		.owner = THIS_MODULE,
		.of_match_table = mml_pq_birsz_driver_dt_match,
	},
};

//module_platform_driver(mml_pq_birsz_driver);

static s32 dbg_case;
static s32 dbg_set(const char *val, const struct kernel_param *kp)
{
	s32 result;

	result = kstrtos32(val, 0, &dbg_case);
	mml_log("%s: debug_case=%d", __func__, dbg_case);

	switch (dbg_case) {
	case 0:
		mml_log("use read to dump component status");
		break;
	default:
		mml_err("invalid debug_case: %d", dbg_case);
		break;
	}
	return result;
}

static s32 dbg_get(char *buf, const struct kernel_param *kp)
{
	s32 length = 0;
	u32 i;

	switch (dbg_case) {
	case 0:
		length += snprintf(buf + length, PAGE_SIZE - length,
			"[%d] probed count: %d\n", dbg_case, dbg_probed_count);
		for (i = 0; i < dbg_probed_count; i++) {
			struct mml_comp *comp = &dbg_probed_components[i]->comp;

			length += snprintf(buf + length, PAGE_SIZE - length,
				"  - [%d] mml comp_id: %d.%d @%pa name: %s bound: %d\n", i,
				comp->id, comp->sub_idx, &comp->base_pa,
				comp->name ? comp->name : "(null)", comp->bound);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -         larb_port: %d @%llx pw: %d clk: %d\n",
				comp->larb_port, comp->larb_base,
				comp->pw_cnt, comp->clk_cnt);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -     ddp comp_id: %d bound: %d\n",
				dbg_probed_components[i]->ddp_comp.id,
				dbg_probed_components[i]->ddp_bound);
		}
		break;
	default:
		mml_err("not support read for debug_case: %d", dbg_case);
		break;
	}
	buf[length] = '\0';

	return length;
}

static const struct kernel_param_ops dbg_param_ops = {
	.set = dbg_set,
	.get = dbg_get,
};
module_param_cb(pq_birsz_debug, &dbg_param_ops, NULL, 0644);
MODULE_PARM_DESC(pq_birsz_debug, "mml birsz debug case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML BIRSZ driver");
MODULE_LICENSE("GPL v2");
