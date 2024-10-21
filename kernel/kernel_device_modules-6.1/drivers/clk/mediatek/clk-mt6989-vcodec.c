// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6989-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs vde20_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vde21_cg_regs = {
	.set_ofs = 0x200,
	.clr_ofs = 0x204,
	.sta_ofs = 0x200,
};

#define GATE_VDE20(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde20_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDE21(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde21_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

static const struct mtk_gate vde2_clks[] = {
	/* VDE20 */
	GATE_VDE20(CLK_VDE2_VDEC_CKEN, "vde2_vdec_cken",
			"vdec_ck"/* parent */, 0),
	GATE_VDE20(CLK_VDE2_VDEC_ACTIVE, "vde2_vdec_active",
			"vdec_ck"/* parent */, 4),
	GATE_VDE20(CLK_VDE2_VDEC_CKEN_ENG, "vde2_vdec_cken_eng",
			"vdec_ck"/* parent */, 8),
	/* VDE21 */
	GATE_VDE21(CLK_VDE2_LAT_CKEN, "vde2_lat_cken",
			"vdec_ck"/* parent */, 0),
	GATE_VDE21(CLK_VDE2_LAT_ACTIVE, "vde2_lat_active",
			"vdec_ck"/* parent */, 4),
	GATE_VDE21(CLK_VDE2_LAT_CKEN_ENG, "vde2_lat_cken_eng",
			"vdec_ck"/* parent */, 8),
};

static const struct mtk_clk_desc vde2_mcd = {
	.clks = vde2_clks,
	.num_clks = CLK_VDE2_NR_CLK,
};

static const struct mtk_gate_regs vde10_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vde11_cg_regs = {
	.set_ofs = 0x1E0,
	.clr_ofs = 0x1E0,
	.sta_ofs = 0x1E0,
};

static const struct mtk_gate_regs vde12_cg_regs = {
	.set_ofs = 0x1EC,
	.clr_ofs = 0x1EC,
	.sta_ofs = 0x1EC,
};

static const struct mtk_gate_regs vde13_cg_regs = {
	.set_ofs = 0x200,
	.clr_ofs = 0x204,
	.sta_ofs = 0x200,
};

#define GATE_VDE10(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde10_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDE11(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde11_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_VDE12(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde12_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_VDE13(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde13_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

static const struct mtk_gate vde1_clks[] = {
	/* VDE10 */
	GATE_VDE10(CLK_VDE1_VDEC_CKEN, "vde1_vdec_cken",
			"vdec_ck"/* parent */, 0),
	GATE_VDE10(CLK_VDE1_VDEC_ACTIVE, "vde1_vdec_active",
			"vdec_ck"/* parent */, 4),
	GATE_VDE10(CLK_VDE1_VDEC_CKEN_ENG, "vde1_vdec_cken_eng",
			"vdec_ck"/* parent */, 8),
	/* VDE11 */
	GATE_VDE11(CLK_VDE1_VDEC_SOC_IPS_EN, "vde1_vdec_soc_ips_en",
			"vdec_ck"/* parent */, 0),
	/* VDE12 */
	GATE_VDE12(CLK_VDE1_VDEC_SOC_APTV_EN, "vde1_aptv_en",
			"avs_vdec_ck"/* parent */, 0),
	GATE_VDE12(CLK_VDE1_VDEC_SOC_APTV_TOP_EN, "vde1_aptv_topen",
			"avs_vdec_ck"/* parent */, 1),
	/* VDE13 */
	GATE_VDE13(CLK_VDE1_LAT_CKEN, "vde1_lat_cken",
			"vdec_ck"/* parent */, 0),
	GATE_VDE13(CLK_VDE1_LAT_ACTIVE, "vde1_lat_active",
			"vdec_ck"/* parent */, 4),
	GATE_VDE13(CLK_VDE1_LAT_CKEN_ENG, "vde1_lat_cken_eng",
			"vdec_ck"/* parent */, 8),
};

static const struct mtk_clk_desc vde1_mcd = {
	.clks = vde1_clks,
	.num_clks = CLK_VDE1_NR_CLK,
};

static const struct mtk_gate_regs ven1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs ven1_hwv_regs = {
	.set_ofs = 0x0088,
	.clr_ofs = 0x008C,
	.sta_ofs = 0x1C44,
};

#define GATE_VEN1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ven1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_HWV_VEN1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ven1_cg_regs,			\
		.hwv_regs = &ven1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = CLK_USE_HW_VOTER | CLK_EN_MM_INFRA_PWR,	\
	}

static const struct mtk_gate ven1_clks[] = {
	GATE_HWV_VEN1(CLK_VEN1_CKE0_LARB, "ven1_larb",
			"venc_ck"/* parent */, 0),
	GATE_HWV_VEN1(CLK_VEN1_CKE1_VENC, "ven1_venc",
			"venc_ck"/* parent */, 4),
	GATE_VEN1(CLK_VEN1_CKE2_JPGENC, "ven1_jpgenc",
			"venc_ck"/* parent */, 8),
	GATE_VEN1(CLK_VEN1_CKE3_JPGDEC, "ven1_jpgdec",
			"venc_ck"/* parent */, 12),
	GATE_VEN1(CLK_VEN1_CKE4_JPGDEC_C1, "ven1_jpgdec_c1",
			"venc_ck"/* parent */, 16),
	GATE_VEN1(CLK_VEN1_CKE5_GALS, "ven1_gals",
			"venc_ck"/* parent */, 28),
	GATE_VEN1(CLK_VEN1_CKE6_GALS_SRAM, "ven1_gals_sram",
			"venc_ck"/* parent */, 31),
};

static const struct mtk_clk_desc ven1_mcd = {
	.clks = ven1_clks,
	.num_clks = CLK_VEN1_NR_CLK,
};

static const struct mtk_gate_regs ven2_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs ven2_hwv_regs = {
	.set_ofs = 0x0090,
	.clr_ofs = 0x0094,
	.sta_ofs = 0x1C48,
};

#define GATE_VEN2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ven2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_HWV_VEN2(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ven2_cg_regs,			\
		.hwv_regs = &ven2_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = CLK_USE_HW_VOTER | CLK_EN_MM_INFRA_PWR,	\
	}

static const struct mtk_gate ven2_clks[] = {
	GATE_HWV_VEN2(CLK_VEN2_CKE0_LARB, "ven2_larb",
			"venc_ck"/* parent */, 0),
	GATE_HWV_VEN2(CLK_VEN2_CKE1_VENC, "ven2_venc",
			"venc_ck"/* parent */, 4),
	GATE_VEN2(CLK_VEN2_CKE2_JPGENC, "ven2_jpgenc",
			"venc_ck"/* parent */, 8),
	GATE_VEN2(CLK_VEN2_CKE3_JPGDEC, "ven2_jpgdec",
			"venc_ck"/* parent */, 12),
	GATE_VEN2(CLK_VEN2_CKE5_GALS, "ven2_gals",
			"venc_ck"/* parent */, 28),
	GATE_VEN2(CLK_VEN2_CKE6_GALS_SRAM, "ven2_gals_sram",
			"venc_ck"/* parent */, 31),
};

static const struct mtk_clk_desc ven2_mcd = {
	.clks = ven2_clks,
	.num_clks = CLK_VEN2_NR_CLK,
};

static const struct mtk_gate_regs ven_c2_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs ven_c2_hwv_regs = {
	.set_ofs = 0x0098,
	.clr_ofs = 0x009C,
	.sta_ofs = 0x1C4C,
};

#define GATE_VEN_C2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ven_c2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_HWV_VEN_C2(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ven_c2_cg_regs,			\
		.hwv_regs = &ven_c2_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = CLK_USE_HW_VOTER | CLK_EN_MM_INFRA_PWR,	\
	}

static const struct mtk_gate ven_c2_clks[] = {
	GATE_HWV_VEN_C2(CLK_VEN_C2_CKE0_LARB, "ven_c2_larb",
			"venc_ck"/* parent */, 0),
	GATE_HWV_VEN_C2(CLK_VEN_C2_CKE1_VENC, "ven_c2_venc",
			"venc_ck"/* parent */, 4),
	GATE_VEN_C2(CLK_VEN_C2_CKE2_JPGENC, "ven_c2_jpgenc",
			"venc_ck"/* parent */, 8),
	GATE_VEN_C2(CLK_VEN_C2_CKE5_GALS, "ven_c2_gals",
			"venc_ck"/* parent */, 28),
	GATE_VEN_C2(CLK_VEN_C2_CKE6_GALS_SRAM, "ven_c2_gals_sram",
			"venc_ck"/* parent */, 31),
};

static const struct mtk_clk_desc ven_c2_mcd = {
	.clks = ven_c2_clks,
	.num_clks = CLK_VEN_C2_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6989_vcodec[] = {
	{
		.compatible = "mediatek,mt6989-vdecsys",
		.data = &vde2_mcd,
	}, {
		.compatible = "mediatek,mt6989-vdecsys_soc",
		.data = &vde1_mcd,
	}, {
		.compatible = "mediatek,mt6989-vencsys",
		.data = &ven1_mcd,
	}, {
		.compatible = "mediatek,mt6989-vencsys_c1",
		.data = &ven2_mcd,
	}, {
		.compatible = "mediatek,mt6989-vencsys_c2",
		.data = &ven_c2_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6989_vcodec_grp_probe(struct platform_device *pdev)
{
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init begin\n", __func__, pdev->name);
#endif

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init end\n", __func__, pdev->name);
#endif

	return r;
}

static struct platform_driver clk_mt6989_vcodec_drv = {
	.probe = clk_mt6989_vcodec_grp_probe,
	.driver = {
		.name = "clk-mt6989-vcodec",
		.of_match_table = of_match_clk_mt6989_vcodec,
	},
};

module_platform_driver(clk_mt6989_vcodec_drv);
MODULE_LICENSE("GPL");
