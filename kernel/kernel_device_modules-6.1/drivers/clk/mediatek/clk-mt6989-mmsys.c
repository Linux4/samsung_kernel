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

static const struct mtk_gate_regs mm10_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mm10_hwv_regs = {
	.set_ofs = 0x0020,
	.clr_ofs = 0x0024,
	.sta_ofs = 0x1C10,
};

static const struct mtk_gate_regs mm11_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

#define GATE_MM10(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm10_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_MM10(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &mm10_cg_regs,			\
		.hwv_regs = &mm10_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER | CLK_EN_MM_INFRA_PWR,	\
	}

#define GATE_MM11(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm11_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate mm1_clks[] = {
	/* MM10 */
	GATE_MM10(CLK_MM1_DISPSYS1_CONFIG, "mm1_dispsys1_config",
			"disp_ck"/* parent */, 0),
	GATE_MM10(CLK_MM1_DISP_MUTEX0, "mm1_disp_mutex0",
			"disp_ck"/* parent */, 1),
	GATE_MM10(CLK_MM1_DISP_DLI_ASYNC0, "mm1_disp_dli_async0",
			"disp_ck"/* parent */, 2),
	GATE_MM10(CLK_MM1_DISP_DLI_ASYNC1, "mm1_disp_dli_async1",
			"disp_ck"/* parent */, 3),
	GATE_MM10(CLK_MM1_DISP_DLI_ASYNC2, "mm1_disp_dli_async2",
			"disp_ck"/* parent */, 4),
	GATE_MM10(CLK_MM1_MDP_RDMA0, "mm1_mdp_rdma0",
			"disp_ck"/* parent */, 5),
	GATE_MM10(CLK_MM1_DISP_R2Y0, "mm1_disp_r2y0",
			"disp_ck"/* parent */, 6),
	GATE_MM10(CLK_MM1_DISP_SPLITTER0, "mm1_disp_splitter0",
			"disp_ck"/* parent */, 7),
	GATE_MM10(CLK_MM1_DISP_SPLITTER1, "mm1_disp_splitter1",
			"disp_ck"/* parent */, 8),
	GATE_MM10(CLK_MM1_DISP_VDCM0, "mm1_disp_vdcm0",
			"disp_ck"/* parent */, 9),
	GATE_MM10(CLK_MM1_DISP_DSC_WRAP0, "mm1_disp_dsc_wrap0",
			"disp_ck"/* parent */, 10),
	GATE_MM10(CLK_MM1_DISP_DSC_WRAP1, "mm1_disp_dsc_wrap1",
			"disp_ck"/* parent */, 11),
	GATE_MM10(CLK_MM1_DISP_DSC_WRAP2, "mm1_disp_dsc_wrap2",
			"disp_ck"/* parent */, 12),
	GATE_MM10(CLK_MM1_DISP_DP_INTF0, "mm1_DP_CLK",
			"disp_ck"/* parent */, 13),
	GATE_MM10(CLK_MM1_DISP_DSI0, "mm1_CLK0",
			"disp_ck"/* parent */, 14),
	GATE_MM10(CLK_MM1_DISP_DSI1, "mm1_CLK1",
			"disp_ck"/* parent */, 15),
	GATE_MM10(CLK_MM1_DISP_DSI2, "mm1_CLK2",
			"disp_ck"/* parent */, 16),
	GATE_MM10(CLK_MM1_DISP_MERGE0, "mm1_disp_merge0",
			"disp_ck"/* parent */, 17),
	GATE_MM10(CLK_MM1_DISP_WDMA0, "mm1_disp_wdma0",
			"disp_ck"/* parent */, 18),
	GATE_HWV_MM10(CLK_MM1_SMI_SUB_COMM0, "mm1_ssc",
			"disp_ck"/* parent */, 19),
	GATE_MM10(CLK_MM1_DISP_WDMA1, "mm1_disp_wdma1",
			"disp_ck"/* parent */, 20),
	GATE_MM10(CLK_MM1_DISP_WDMA2, "mm1_disp_wdma2",
			"disp_ck"/* parent */, 21),
	GATE_MM10(CLK_MM1_DISP_GDMA0, "mm1_disp_gdma0",
			"disp_ck"/* parent */, 22),
	GATE_MM10(CLK_MM1_DISP_DLI_ASYNC3, "mm1_disp_dli_async3",
			"disp_ck"/* parent */, 23),
	GATE_MM10(CLK_MM1_DISP_DLI_ASYNC4, "mm1_disp_dli_async4",
			"disp_ck"/* parent */, 24),
	/* MM11 */
	GATE_MM11(CLK_MM1_MOD1, "mm1_mod1",
			"disp_ck"/* parent */, 0),
	GATE_MM11(CLK_MM1_MOD2, "mm1_mod2",
			"disp_ck"/* parent */, 1),
	GATE_MM11(CLK_MM1_MOD3, "mm1_mod3",
			"disp_ck"/* parent */, 2),
	GATE_MM11(CLK_MM1_MOD4, "mm1_mod4",
			"disp_ck"/* parent */, 3),
	GATE_MM11(CLK_MM1_MOD5, "mm1_mod5",
			"disp_ck"/* parent */, 4),
	GATE_MM11(CLK_MM1_MOD6, "mm1_mod6",
			"disp_ck"/* parent */, 5),
	GATE_MM11(CLK_MM1_MOD7, "mm1_mod7",
			"disp_ck"/* parent */, 6),
	GATE_MM11(CLK_MM1_SUBSYS, "mm1_subsys_ck",
			"disp_ck"/* parent */, 7),
	GATE_MM11(CLK_MM1_DSI0, "mm1_dsi0_ck",
			"disp_ck"/* parent */, 8),
	GATE_MM11(CLK_MM1_DSI1, "mm1_dsi1_ck",
			"disp_ck"/* parent */, 9),
	GATE_MM11(CLK_MM1_DSI2, "mm1_dsi2_ck",
			"disp_ck"/* parent */, 10),
	GATE_MM11(CLK_MM1_DP, "mm1_dp_ck",
			"disp_ck"/* parent */, 11),
	GATE_MM11(CLK_MM1_F26M, "mm1_f26m_ck",
			"disp_ck"/* parent */, 12),
};

static const struct mtk_clk_desc mm1_mcd = {
	.clks = mm1_clks,
	.num_clks = CLK_MM1_NR_CLK,
};

static const struct mtk_gate_regs mm0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mm1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

static const struct mtk_gate_regs mm1_hwv_regs = {
	.set_ofs = 0x0028,
	.clr_ofs = 0x002C,
	.sta_ofs = 0x1C14,
};

#define GATE_MM0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MM1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_MM1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &mm1_cg_regs,			\
		.hwv_regs = &mm1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER | CLK_EN_MM_INFRA_PWR,	\
	}

static const struct mtk_gate mm_clks[] = {
	/* MM0 */
	GATE_MM0(CLK_MM_CONFIG, "mm_config",
			"disp_ck"/* parent */, 0),
	GATE_MM0(CLK_MM_DISP_MUTEX0, "mm_disp_mutex0",
			"disp_ck"/* parent */, 1),
	GATE_MM0(CLK_MM_DISP_AAL0, "mm_disp_aal0",
			"disp_ck"/* parent */, 2),
	GATE_MM0(CLK_MM_DISP_AAL1, "mm_disp_aal1",
			"disp_ck"/* parent */, 3),
	GATE_MM0(CLK_MM_DISP_C3D0, "mm_disp_c3d0",
			"disp_ck"/* parent */, 4),
	GATE_MM0(CLK_MM_DISP_C3D1, "mm_disp_c3d1",
			"disp_ck"/* parent */, 5),
	GATE_MM0(CLK_MM_DISP_CCORR0, "mm_disp_ccorr0",
			"disp_ck"/* parent */, 6),
	GATE_MM0(CLK_MM_DISP_CCORR1, "mm_disp_ccorr1",
			"disp_ck"/* parent */, 7),
	GATE_MM0(CLK_MM_DISP_CCORR2, "mm_disp_ccorr2",
			"disp_ck"/* parent */, 8),
	GATE_MM0(CLK_MM_DISP_CCORR3, "mm_disp_ccorr3",
			"disp_ck"/* parent */, 9),
	GATE_MM0(CLK_MM_DISP_CHIST0, "mm_disp_chist0",
			"disp_ck"/* parent */, 10),
	GATE_MM0(CLK_MM_DISP_CHIST1, "mm_disp_chist1",
			"disp_ck"/* parent */, 11),
	GATE_MM0(CLK_MM_DISP_COLOR0, "mm_disp_color0",
			"disp_ck"/* parent */, 12),
	GATE_MM0(CLK_MM_DISP_COLOR1, "mm_disp_color1",
			"disp_ck"/* parent */, 13),
	GATE_MM0(CLK_MM_DISP_DITHER0, "mm_disp_dither0",
			"disp_ck"/* parent */, 14),
	GATE_MM0(CLK_MM_DISP_DITHER1, "mm_disp_dither1",
			"disp_ck"/* parent */, 15),
	GATE_MM0(CLK_MM_DISP_DITHER2, "mm_disp_dither2",
			"disp_ck"/* parent */, 16),
	GATE_MM0(CLK_MM_DLI_ASYNC0, "mm_dli_async0",
			"disp_ck"/* parent */, 17),
	GATE_MM0(CLK_MM_DLI_ASYNC1, "mm_dli_async1",
			"disp_ck"/* parent */, 18),
	GATE_MM0(CLK_MM_DLI_ASYNC2, "mm_dli_async2",
			"disp_ck"/* parent */, 19),
	GATE_MM0(CLK_MM_DLI_ASYNC3, "mm_dli_async3",
			"disp_ck"/* parent */, 20),
	GATE_MM0(CLK_MM_DLI_ASYNC4, "mm_dli_async4",
			"disp_ck"/* parent */, 21),
	GATE_MM0(CLK_MM_DLI_ASYNC5, "mm_dli_async5",
			"disp_ck"/* parent */, 22),
	GATE_MM0(CLK_MM_DLI_ASYNC6, "mm_dli_async6",
			"disp_ck"/* parent */, 23),
	GATE_MM0(CLK_MM_DLI_ASYNC7, "mm_dli_async7",
			"disp_ck"/* parent */, 24),
	GATE_MM0(CLK_MM_DLO_ASYNC0, "mm_dlo_async0",
			"disp_ck"/* parent */, 25),
	GATE_MM0(CLK_MM_DLO_ASYNC1, "mm_dlo_async1",
			"disp_ck"/* parent */, 26),
	GATE_MM0(CLK_MM_DLO_ASYNC2, "mm_dlo_async2",
			"disp_ck"/* parent */, 27),
	GATE_MM0(CLK_MM_DLO_ASYNC3, "mm_dlo_async3",
			"disp_ck"/* parent */, 28),
	GATE_MM0(CLK_MM_DLO_ASYNC4, "mm_dlo_async4",
			"disp_ck"/* parent */, 29),
	GATE_MM0(CLK_MM_DISP_GAMMA0, "mm_disp_gamma0",
			"disp_ck"/* parent */, 30),
	GATE_MM0(CLK_MM_DISP_GAMMA1, "mm_disp_gamma1",
			"disp_ck"/* parent */, 31),
	/* MM1 */
	GATE_MM1(CLK_MM_MDP_AAL0, "mm_mdp_aal0",
			"disp_ck"/* parent */, 0),
	GATE_MM1(CLK_MM_MDP_RDMA0, "mm_mdp_rdma0",
			"disp_ck"/* parent */, 1),
	GATE_MM1(CLK_MM_DISP_ODDMR0, "mm_disp_oddmr0",
			"disp_ck"/* parent */, 2),
	GATE_MM1(CLK_MM_DISP_POSTALIGN0, "mm_disp_postalign0",
			"disp_ck"/* parent */, 3),
	GATE_MM1(CLK_MM_DISP_POSTMASK0, "mm_disp_postmask0",
			"disp_ck"/* parent */, 4),
	GATE_MM1(CLK_MM_DISP_POSTMASK1, "mm_disp_postmask1",
			"disp_ck"/* parent */, 5),
	GATE_MM1(CLK_MM_DISP_RSZ0, "mm_disp_rsz0",
			"disp_ck"/* parent */, 6),
	GATE_MM1(CLK_MM_DISP_RSZ1, "mm_disp_rsz1",
			"disp_ck"/* parent */, 7),
	GATE_MM1(CLK_MM_DISP_SPR0, "mm_disp_spr0",
			"disp_ck"/* parent */, 8),
	GATE_MM1(CLK_MM_DISP_TDSHP0, "mm_disp_tdshp0",
			"disp_ck"/* parent */, 9),
	GATE_MM1(CLK_MM_DISP_TDSHP1, "mm_disp_tdshp1",
			"disp_ck"/* parent */, 10),
	GATE_MM1(CLK_MM_DISP_WDMA1, "mm_disp_wdma1",
			"disp_ck"/* parent */, 11),
	GATE_MM1(CLK_MM_DISP_Y2R0, "mm_disp_y2r0",
			"disp_ck"/* parent */, 12),
	GATE_MM1(CLK_MM_MDP_AAL1, "mm_mdp_aal1",
			"disp_ck"/* parent */, 13),
	GATE_HWV_MM1(CLK_MM_SMI_SUB_COMM0, "mm_ssc",
			"disp_ck"/* parent */, 14),
};

static const struct mtk_clk_desc mm_mcd = {
	.clks = mm_clks,
	.num_clks = CLK_MM_NR_CLK,
};

static const struct mtk_gate_regs mminfra_ao_config0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mminfra_ao_config0_hwv_regs = {
	.set_ofs = 0x0050,
	.clr_ofs = 0x0054,
	.sta_ofs = 0x1C28,
};

static const struct mtk_gate_regs mminfra_ao_config1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

static const struct mtk_gate_regs mminfra_ao_config1_hwv_regs = {
	.set_ofs = 0x0058,
	.clr_ofs = 0x005C,
	.sta_ofs = 0x1C2C,
};

#define GATE_MMINFRA_AO_CONFIG0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mminfra_ao_config0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_MMINFRA_AO_CONFIG0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &mminfra_ao_config0_cg_regs,			\
		.hwv_regs = &mminfra_ao_config0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER | CLK_EN_MM_INFRA_PWR,	\
	}

#define GATE_MMINFRA_AO_CONFIG1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mminfra_ao_config1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_MMINFRA_AO_CONFIG1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &mminfra_ao_config1_cg_regs,			\
		.hwv_regs = &mminfra_ao_config1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER | CLK_EN_MM_INFRA_PWR,	\
	}

#define GATE_MMINFRA_AO_CONFIG0_DUMMY(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &mminfra_ao_config0_hwv_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_dummy,			\
	}

#define GATE_MMINFRA_AO_CONFIG1_DUMMY(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &mminfra_ao_config0_hwv_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_dummy,			\
	}

static const struct mtk_gate mminfra_ao_config_clks[] = {
	/* MMINFRA_AO_CONFIG0 */
	GATE_HWV_MMINFRA_AO_CONFIG0(CLK_MMINFRA_AO_GCE_D, "mminfra_ao_gce_d",
			"mminfra_ck"/* parent */, 0),
	GATE_HWV_MMINFRA_AO_CONFIG0(CLK_MMINFRA_AO_GCE_M, "mminfra_ao_gce_m",
			"mminfra_ck"/* parent */, 1),
	/* MMINFRA_AO_CONFIG1 */
	GATE_HWV_MMINFRA_AO_CONFIG1(CLK_MMINFRA_AO_GCE_26M, "mminfra_ao_gce_26m",
			"mminfra_ck"/* parent */, 17),
};

static const struct mtk_clk_desc mminfra_ao_config_mcd = {
	.clks = mminfra_ao_config_clks,
	.num_clks = CLK_MMINFRA_AO_CONFIG_NR_CLK,
};

static const struct mtk_gate_regs mminfra_config_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mminfra_config_hwv_regs = {
	.set_ofs = 0x0060,
	.clr_ofs = 0x0064,
	.sta_ofs = 0x1C30,
};

#define GATE_MMINFRA_CONFIG(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mminfra_config_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}


#define GATE_HWV_MMINFRA_CONFIG(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &mminfra_config_cg_regs,			\
		.hwv_regs = &mminfra_config_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER | CLK_EN_MM_INFRA_PWR,	\
	}

#define GATE_MMINFRA_CONFIG_DUMMY(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &mminfra_config_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_dummy,			\
	}

#define GATE_HWV_MMINFRA_CONFIG_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &mminfra_config_cg_regs,			\
		.hwv_regs = &mminfra_config_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_dummys,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER | CLK_EN_MM_INFRA_PWR,	\
	}

static const struct mtk_gate mminfra_config_clks[] = {
	GATE_HWV_MMINFRA_CONFIG_DUMMY(CLK_MMINFRA_SMI, "mminfra_smi",
			"clk26m"/* parent */, 2),
};

static const struct mtk_clk_desc mminfra_config_mcd = {
	.clks = mminfra_config_clks,
	.num_clks = CLK_MMINFRA_CONFIG_NR_CLK,
};

static const struct mtk_gate_regs ovl1_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs ovl1_hwv_regs = {
	.set_ofs = 0x0068,
	.clr_ofs = 0x006C,
	.sta_ofs = 0x1C34,
};

#define GATE_OVL1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ovl1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_OVL1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ovl1_cg_regs,			\
		.hwv_regs = &ovl1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER | CLK_EN_MM_INFRA_PWR,	\
	}

static const struct mtk_gate ovl1_clks[] = {
	GATE_OVL1(CLK_OVL1_OVLSYS_CONFIG, "ovl1_ovlsys_config",
			"disp_ck"/* parent */, 0),
	GATE_OVL1(CLK_OVL1_DISP_FAKE_ENG0, "ovl1_disp_fake_eng0",
			"disp_ck"/* parent */, 1),
	GATE_OVL1(CLK_OVL1_DISP_FAKE_ENG1, "ovl1_disp_fake_eng1",
			"disp_ck"/* parent */, 2),
	GATE_OVL1(CLK_OVL1_DISP_MUTEX0, "ovl1_disp_mutex0",
			"disp_ck"/* parent */, 3),
	GATE_OVL1(CLK_OVL1_DISP_OVL0_2L, "ovl1_disp_ovl0_2l",
			"disp_ck"/* parent */, 4),
	GATE_OVL1(CLK_OVL1_DISP_OVL1_2L, "ovl1_disp_ovl1_2l",
			"disp_ck"/* parent */, 5),
	GATE_OVL1(CLK_OVL1_DISP_OVL2_2L, "ovl1_disp_ovl2_2l",
			"disp_ck"/* parent */, 6),
	GATE_OVL1(CLK_OVL1_DISP_OVL3_2L, "ovl1_disp_ovl3_2l",
			"disp_ck"/* parent */, 7),
	GATE_OVL1(CLK_OVL1_DISP_RSZ1, "ovl1_disp_rsz1",
			"disp_ck"/* parent */, 8),
	GATE_OVL1(CLK_OVL1_MDP_RSZ0, "ovl1_mdp_rsz0",
			"disp_ck"/* parent */, 9),
	GATE_OVL1(CLK_OVL1_DISP_WDMA0, "ovl1_disp_wdma0",
			"disp_ck"/* parent */, 10),
	GATE_OVL1(CLK_OVL1_DISP_UFBC_WDMA0, "ovl1_disp_ufbc_wdma0",
			"disp_ck"/* parent */, 11),
	GATE_OVL1(CLK_OVL1_DISP_WDMA2, "ovl1_disp_wdma2",
			"disp_ck"/* parent */, 12),
	GATE_OVL1(CLK_OVL1_DISP_DLI_ASYNC0, "ovl1_disp_dli_async0",
			"disp_ck"/* parent */, 13),
	GATE_OVL1(CLK_OVL1_DISP_DLI_ASYNC1, "ovl1_disp_dli_async1",
			"disp_ck"/* parent */, 14),
	GATE_OVL1(CLK_OVL1_DISP_DLI_ASYNC2, "ovl1_disp_dli_async2",
			"disp_ck"/* parent */, 15),
	GATE_OVL1(CLK_OVL1_DISP_DL0_ASYNC0, "ovl1_disp_dl0_async0",
			"disp_ck"/* parent */, 16),
	GATE_OVL1(CLK_OVL1_DISP_DL0_ASYNC1, "ovl1_disp_dl0_async1",
			"disp_ck"/* parent */, 17),
	GATE_OVL1(CLK_OVL1_DISP_DL0_ASYNC2, "ovl1_disp_dl0_async2",
			"disp_ck"/* parent */, 18),
	GATE_OVL1(CLK_OVL1_DISP_DL0_ASYNC3, "ovl1_disp_dl0_async3",
			"disp_ck"/* parent */, 19),
	GATE_OVL1(CLK_OVL1_DISP_DL0_ASYNC4, "ovl1_disp_dl0_async4",
			"disp_ck"/* parent */, 20),
	GATE_OVL1(CLK_OVL1_DISP_DL0_ASYNC5, "ovl1_disp_dl0_async5",
			"disp_ck"/* parent */, 21),
	GATE_OVL1(CLK_OVL1_DISP_DL0_ASYNC6, "ovl1_disp_dl0_async6",
			"disp_ck"/* parent */, 22),
	GATE_OVL1(CLK_OVL1_INLINEROT0, "ovl1_inlinerot0",
			"disp_ck"/* parent */, 23),
	GATE_HWV_OVL1(CLK_OVL1_SMI_SUB_COMM0, "ovl1_ssc",
			"disp_ck"/* parent */, 24),
	GATE_OVL1(CLK_OVL1_DISP_Y2R0, "ovl1_disp_y2r0",
			"disp_ck"/* parent */, 25),
	GATE_OVL1(CLK_OVL1_DISP_Y2R1, "ovl1_disp_y2r1",
			"disp_ck"/* parent */, 26),
	GATE_OVL1(CLK_OVL1_DISP_OVL4_2L, "ovl1_disp_ovl4_2l",
			"disp_ck"/* parent */, 27),
};

static const struct mtk_clk_desc ovl1_mcd = {
	.clks = ovl1_clks,
	.num_clks = CLK_OVL1_NR_CLK,
};

static const struct mtk_gate_regs ovl_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs ovl_hwv_regs = {
	.set_ofs = 0x0070,
	.clr_ofs = 0x0074,
	.sta_ofs = 0x1C38,
};

#define GATE_OVL(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ovl_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_OVL(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ovl_cg_regs,			\
		.hwv_regs = &ovl_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER | CLK_EN_MM_INFRA_PWR,	\
	}

static const struct mtk_gate ovl_clks[] = {
	GATE_OVL(CLK_OVLSYS_CONFIG, "ovlsys_config",
			"disp_ck"/* parent */, 0),
	GATE_OVL(CLK_OVL_DISP_FAKE_ENG0, "ovl_disp_fake_eng0",
			"disp_ck"/* parent */, 1),
	GATE_OVL(CLK_OVL_DISP_FAKE_ENG1, "ovl_disp_fake_eng1",
			"disp_ck"/* parent */, 2),
	GATE_OVL(CLK_OVL_DISP_MUTEX0, "ovl_disp_mutex0",
			"disp_ck"/* parent */, 3),
	GATE_OVL(CLK_OVL_DISP_OVL0_2L, "ovl_disp_ovl0_2l",
			"disp_ck"/* parent */, 4),
	GATE_OVL(CLK_OVL_DISP_OVL1_2L, "ovl_disp_ovl1_2l",
			"disp_ck"/* parent */, 5),
	GATE_OVL(CLK_OVL_DISP_OVL2_2L, "ovl_disp_ovl2_2l",
			"disp_ck"/* parent */, 6),
	GATE_OVL(CLK_OVL_DISP_OVL3_2L, "ovl_disp_ovl3_2l",
			"disp_ck"/* parent */, 7),
	GATE_OVL(CLK_OVL_DISP_RSZ1, "ovl_disp_rsz1",
			"disp_ck"/* parent */, 8),
	GATE_OVL(CLK_OVL_MDP_RSZ0, "ovl_mdp_rsz0",
			"disp_ck"/* parent */, 9),
	GATE_OVL(CLK_OVL_DISP_WDMA0, "ovl_disp_wdma0",
			"disp_ck"/* parent */, 10),
	GATE_OVL(CLK_OVL_DISP_UFBC_WDMA0, "ovl_disp_ufbc_wdma0",
			"disp_ck"/* parent */, 11),
	GATE_OVL(CLK_OVL_DISP_WDMA2, "ovl_disp_wdma2",
			"disp_ck"/* parent */, 12),
	GATE_OVL(CLK_OVL_DISP_DLI_ASYNC0, "ovl_disp_dli_async0",
			"disp_ck"/* parent */, 13),
	GATE_OVL(CLK_OVL_DISP_DLI_ASYNC1, "ovl_disp_dli_async1",
			"disp_ck"/* parent */, 14),
	GATE_OVL(CLK_OVL_DISP_DLI_ASYNC2, "ovl_disp_dli_async2",
			"disp_ck"/* parent */, 15),
	GATE_OVL(CLK_OVL_DISP_DL0_ASYNC0, "ovl_disp_dl0_async0",
			"disp_ck"/* parent */, 16),
	GATE_OVL(CLK_OVL_DISP_DL0_ASYNC1, "ovl_disp_dl0_async1",
			"disp_ck"/* parent */, 17),
	GATE_OVL(CLK_OVL_DISP_DL0_ASYNC2, "ovl_disp_dl0_async2",
			"disp_ck"/* parent */, 18),
	GATE_OVL(CLK_OVL_DISP_DL0_ASYNC3, "ovl_disp_dl0_async3",
			"disp_ck"/* parent */, 19),
	GATE_OVL(CLK_OVL_DISP_DL0_ASYNC4, "ovl_disp_dl0_async4",
			"disp_ck"/* parent */, 20),
	GATE_OVL(CLK_OVL_DISP_DL0_ASYNC5, "ovl_disp_dl0_async5",
			"disp_ck"/* parent */, 21),
	GATE_OVL(CLK_OVL_DISP_DL0_ASYNC6, "ovl_disp_dl0_async6",
			"disp_ck"/* parent */, 22),
	GATE_OVL(CLK_OVL_INLINEROT0, "ovl_inlinerot0",
			"disp_ck"/* parent */, 23),
	GATE_HWV_OVL(CLK_OVL_SMI_SUB_COMM0, "ovl_ssc",
			"disp_ck"/* parent */, 24),
	GATE_OVL(CLK_OVL_DISP_Y2R0, "ovl_disp_y2r0",
			"disp_ck"/* parent */, 25),
	GATE_OVL(CLK_OVL_DISP_Y2R1, "ovl_disp_y2r1",
			"disp_ck"/* parent */, 26),
	GATE_OVL(CLK_OVL_DISP_OVL4_2L, "ovl_disp_ovl4_2l",
			"disp_ck"/* parent */, 27),
};

static const struct mtk_clk_desc ovl_mcd = {
	.clks = ovl_clks,
	.num_clks = CLK_OVL_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6989_mmsys[] = {
	{
		.compatible = "mediatek,mt6989-mmsys1",
		.data = &mm1_mcd,
	}, {
		.compatible = "mediatek,mt6989-mmsys0",
		.data = &mm_mcd,
	}, {
		.compatible = "mediatek,mt6989-mminfra_ao_config",
		.data = &mminfra_ao_config_mcd,
	}, {
		.compatible = "mediatek,mt6989-mminfra_config",
		.data = &mminfra_config_mcd,
	}, {
		.compatible = "mediatek,mt6989-ovlsys1_config",
		.data = &ovl1_mcd,
	}, {
		.compatible = "mediatek,mt6989-ovlsys_config",
		.data = &ovl_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6989_mmsys_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6989_mmsys_drv = {
	.probe = clk_mt6989_mmsys_grp_probe,
	.driver = {
		.name = "clk-mt6989-mmsys",
		.of_match_table = of_match_clk_mt6989_mmsys,
	},
};

module_platform_driver(clk_mt6989_mmsys_drv);
MODULE_LICENSE("GPL");
