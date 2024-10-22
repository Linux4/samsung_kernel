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

static const struct mtk_gate_regs camsys_ipe_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs camsys_ipe_hwv_regs = {
	.set_ofs = 0x0000,
	.clr_ofs = 0x0004,
	.sta_ofs = 0x1C00,
};

#define GATE_CAMSYS_IPE(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &camsys_ipe_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_CAMSYS_IPE(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &camsys_ipe_cg_regs,			\
		.hwv_regs = &camsys_ipe_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER | CLK_EN_MM_INFRA_PWR,	\
	}

static const struct mtk_gate camsys_ipe_clks[] = {
	GATE_HWV_CAMSYS_IPE(CLK_CAMSYS_IPE_LARB19, "camsys_ipe_larb19",
			"ipe_ck"/* parent */, 0),
	GATE_CAMSYS_IPE(CLK_CAMSYS_IPE_DPE, "camsys_ipe_dpe",
			"ipe_ck"/* parent */, 1),
	GATE_CAMSYS_IPE(CLK_CAMSYS_IPE_FUS, "camsys_ipe_fus",
			"ipe_ck"/* parent */, 2),
	GATE_CAMSYS_IPE(CLK_CAMSYS_IPE_DHZE, "camsys_ipe_dhze",
			"ipe_ck"/* parent */, 3),
	GATE_HWV_CAMSYS_IPE(CLK_CAMSYS_IPE_GALS, "camsys_ipe_gals",
			"ipe_ck"/* parent */, 4),
};

static const struct mtk_clk_desc camsys_ipe_mcd = {
	.clks = camsys_ipe_clks,
	.num_clks = CLK_CAMSYS_IPE_NR_CLK,
};

static const struct mtk_gate_regs cam_m0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs cam_m0_hwv_regs = {
	.set_ofs = 0x0008,
	.clr_ofs = 0x000C,
	.sta_ofs = 0x1C04,
};

static const struct mtk_gate_regs cam_m1_cg_regs = {
	.set_ofs = 0x50,
	.clr_ofs = 0x54,
	.sta_ofs = 0x4C,
};

#define GATE_CAM_M0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_m0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_CAM_M0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &cam_m0_cg_regs,			\
		.hwv_regs = &cam_m0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER | CLK_EN_MM_INFRA_PWR,	\
	}

#define GATE_CAM_M1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_m1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate cam_m_clks[] = {
	/* CAM_M0 */
	GATE_HWV_CAM_M0(CLK_CAM_MAIN_LARB13, "cam_m_larb13",
			"cam_ck"/* parent */, 0),
	GATE_HWV_CAM_M0(CLK_CAM_MAIN_LARB14, "cam_m_larb14",
			"cam_ck"/* parent */, 1),
	GATE_HWV_CAM_M0(CLK_CAM_MAIN_LARB27, "cam_m_larb27",
			"cam_ck"/* parent */, 2),
	GATE_HWV_CAM_M0(CLK_CAM_MAIN_LARB29, "cam_m_larb29",
			"cam_ck"/* parent */, 3),
	GATE_HWV_CAM_M0(CLK_CAM_MAIN_CAM, "cam_m_cam",
			"cam_ck"/* parent */, 4),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM_SUBA, "cam_m_cam_suba",
			"cam_ck"/* parent */, 5),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM_SUBB, "cam_m_cam_subb",
			"cam_ck"/* parent */, 6),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM_SUBC, "cam_m_cam_subc",
			"cam_ck"/* parent */, 7),
	GATE_HWV_CAM_M0(CLK_CAM_MAIN_CAM_MRAW, "cam_m_cam_mraw",
			"cam_ck"/* parent */, 8),
	GATE_HWV_CAM_M0(CLK_CAM_MAIN_CAMTG, "cam_m_camtg",
			"camtm_ck"/* parent */, 9),
	GATE_HWV_CAM_M0(CLK_CAM_MAIN_SENINF, "cam_m_seninf",
			"cam_ck"/* parent */, 10),
	GATE_CAM_M0(CLK_CAM_MAIN_CAMSV_TOP, "cam_m_camsv",
			"cam_ck"/* parent */, 11),
	GATE_CAM_M0(CLK_CAM_MAIN_ADLRD, "cam_m_adlrd",
			"cam_ck"/* parent */, 12),
	GATE_CAM_M0(CLK_CAM_MAIN_ADLWR, "cam_m_adlwr",
			"cam_ck"/* parent */, 13),
	GATE_CAM_M0(CLK_CAM_MAIN_UISP, "cam_m_uisp",
			"cam_ck"/* parent */, 14),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM2MM0_GALS, "cam_m_cam2mm0_GCON_0",
			"cam_ck"/* parent */, 16),
	GATE_HWV_CAM_M0(CLK_CAM_MAIN_CAM2MM1_GALS, "cam_m_cam2mm1_GCON_0",
			"cam_ck"/* parent */, 17),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM2SYS_GALS, "cam_m_cam2sys_GCON_0",
			"cam_ck"/* parent */, 18),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM2MM2_GALS, "cam_m_cam2mm2_GCON_0",
			"cam_ck"/* parent */, 19),
	GATE_CAM_M0(CLK_CAM_MAIN_CCUSYS, "cam_m_ccusys",
			"ccusys_ck"/* parent */, 20),
	GATE_HWV_CAM_M0(CLK_CAM_MAIN_CAM_DPE, "cam_m_cam_dpe",
			"ipe_ck"/* parent */, 26),
	GATE_HWV_CAM_M0(CLK_CAM_MAIN_CAM_ASG, "cam_m_cam_asg",
			"cam_ck"/* parent */, 27),
	/* CAM_M1 */
	GATE_CAM_M1(CLK_CAM_MAIN_CAMSV_A_CON_1, "cam_m_camsv_a_con_1",
			"cam_ck"/* parent */, 0),
	GATE_CAM_M1(CLK_CAM_MAIN_CAMSV_B_CON_1, "cam_m_camsv_b_con_1",
			"cam_ck"/* parent */, 1),
	GATE_CAM_M1(CLK_CAM_MAIN_CAMSV_C_CON_1, "cam_m_camsv_c_con_1",
			"cam_ck"/* parent */, 2),
	GATE_CAM_M1(CLK_CAM_MAIN_CAMSV_D_CON_1, "cam_m_camsv_d_con_1",
			"cam_ck"/* parent */, 3),
	GATE_CAM_M1(CLK_CAM_MAIN_CAMSV_E_CON_1, "cam_m_camsv_e_con_1",
			"cam_ck"/* parent */, 4),
	GATE_CAM_M1(CLK_CAM_MAIN_CAMSV_CON_1, "cam_m_camsv_con_1",
			"cam_ck"/* parent */, 5),
};

static const struct mtk_clk_desc cam_m_mcd = {
	.clks = cam_m_clks,
	.num_clks = CLK_CAM_M_NR_CLK,
};

static const struct mtk_gate_regs cam_mr_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs cam_mr_hwv_regs = {
	.set_ofs = 0x0010,
	.clr_ofs = 0x0014,
	.sta_ofs = 0x1C08,
};

#define GATE_CAM_MR(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_mr_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_CAM_MR(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &cam_mr_cg_regs,			\
		.hwv_regs = &cam_mr_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER | CLK_EN_MM_INFRA_PWR,	\
	}

static const struct mtk_gate cam_mr_clks[] = {
	GATE_HWV_CAM_MR(CLK_CAM_MR_LARBX, "cam_mr_larbx",
			"cam_ck"/* parent */, 0),
	GATE_HWV_CAM_MR(CLK_CAM_MR_GALS, "cam_mr_gals",
			"cam_ck"/* parent */, 1),
	GATE_CAM_MR(CLK_CAM_MR_CAMTG, "cam_mr_camtg",
			"camtm_ck"/* parent */, 2),
	GATE_CAM_MR(CLK_CAM_MR_MRAW0, "cam_mr_mraw0",
			"cam_ck"/* parent */, 3),
	GATE_CAM_MR(CLK_CAM_MR_MRAW1, "cam_mr_mraw1",
			"cam_ck"/* parent */, 4),
	GATE_CAM_MR(CLK_CAM_MR_MRAW2, "cam_mr_mraw2",
			"cam_ck"/* parent */, 5),
	GATE_CAM_MR(CLK_CAM_MR_MRAW3, "cam_mr_mraw3",
			"cam_ck"/* parent */, 6),
	GATE_CAM_MR(CLK_CAM_MR_PDA0, "cam_mr_pda0",
			"cam_ck"/* parent */, 7),
	GATE_CAM_MR(CLK_CAM_MR_PDA1, "cam_mr_pda1",
			"cam_ck"/* parent */, 8),
};

static const struct mtk_clk_desc cam_mr_mcd = {
	.clks = cam_mr_clks,
	.num_clks = CLK_CAM_MR_NR_CLK,
};

static const struct mtk_gate_regs cam_ra_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_RA(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_ra_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_RA_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_ra_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_dummy,	\
	}

static const struct mtk_gate cam_ra_clks[] = {
	GATE_CAM_RA_DUMMY(CLK_CAM_RA_LARBX, "cam_ra_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_RA(CLK_CAM_RA_LARBX_PDCK, "cam_ra_larbx_pdck",
			"cam_ck"/* parent */, 0),
	GATE_CAM_RA(CLK_CAM_RA_CAM, "cam_ra_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_RA(CLK_CAM_RA_CAMTG, "cam_ra_camtg",
			"camtm_ck"/* parent */, 2),
};

static const struct mtk_clk_desc cam_ra_mcd = {
	.clks = cam_ra_clks,
	.num_clks = CLK_CAM_RA_NR_CLK,
};

static const struct mtk_gate_regs cam_rb_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_RB(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_rb_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_RB_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_rb_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_dummy,	\
	}

static const struct mtk_gate cam_rb_clks[] = {
	GATE_CAM_RB_DUMMY(CLK_CAM_RB_LARBX, "cam_rb_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_RB(CLK_CAM_RB_LARBX_PDCK, "cam_rb_larbx_pdck",
			"cam_ck"/* parent */, 0),
	GATE_CAM_RB(CLK_CAM_RB_CAM, "cam_rb_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_RB(CLK_CAM_RB_CAMTG, "cam_rb_camtg",
			"camtm_ck"/* parent */, 2),
};

static const struct mtk_clk_desc cam_rb_mcd = {
	.clks = cam_rb_clks,
	.num_clks = CLK_CAM_RB_NR_CLK,
};

static const struct mtk_gate_regs cam_rc_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_RC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_rc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_RC_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_rc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_dummy,	\
	}

static const struct mtk_gate cam_rc_clks[] = {
	GATE_CAM_RC_DUMMY(CLK_CAM_RC_LARBX, "cam_rc_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_RC(CLK_CAM_RC_LARBX_PDCK, "cam_rc_larbx_pdck",
			"cam_ck"/* parent */, 0),
	GATE_CAM_RC(CLK_CAM_RC_CAM, "cam_rc_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_RC(CLK_CAM_RC_CAMTG, "cam_rc_camtg",
			"camtm_ck"/* parent */, 2),
};

static const struct mtk_clk_desc cam_rc_mcd = {
	.clks = cam_rc_clks,
	.num_clks = CLK_CAM_RC_NR_CLK,
};

static const struct mtk_gate_regs camsys_rmsa_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAMSYS_RMSA(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &camsys_rmsa_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate camsys_rmsa_clks[] = {
	GATE_CAMSYS_RMSA(CLK_CAMSYS_RMSA_LARBX, "camsys_rmsa_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAMSYS_RMSA(CLK_CAMSYS_RMSA_CAM, "camsys_rmsa_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAMSYS_RMSA(CLK_CAMSYS_RMSA_CAMTG, "camsys_rmsa_camtg",
			"camtm_ck"/* parent */, 2),
};

static const struct mtk_clk_desc camsys_rmsa_mcd = {
	.clks = camsys_rmsa_clks,
	.num_clks = CLK_CAMSYS_RMSA_NR_CLK,
};

static const struct mtk_gate_regs camsys_rmsb_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAMSYS_RMSB(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &camsys_rmsb_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate camsys_rmsb_clks[] = {
	GATE_CAMSYS_RMSB(CLK_CAMSYS_RMSB_LARBX, "camsys_rmsb_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAMSYS_RMSB(CLK_CAMSYS_RMSB_CAM, "camsys_rmsb_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAMSYS_RMSB(CLK_CAMSYS_RMSB_CAMTG, "camsys_rmsb_camtg",
			"camtm_ck"/* parent */, 2),
};

static const struct mtk_clk_desc camsys_rmsb_mcd = {
	.clks = camsys_rmsb_clks,
	.num_clks = CLK_CAMSYS_RMSB_NR_CLK,
};

static const struct mtk_gate_regs camsys_rmsc_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAMSYS_RMSC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &camsys_rmsc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate camsys_rmsc_clks[] = {
	GATE_CAMSYS_RMSC(CLK_CAMSYS_RMSC_LARBX, "camsys_rmsc_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAMSYS_RMSC(CLK_CAMSYS_RMSC_CAM, "camsys_rmsc_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAMSYS_RMSC(CLK_CAMSYS_RMSC_CAMTG, "camsys_rmsc_camtg",
			"camtm_ck"/* parent */, 2),
};

static const struct mtk_clk_desc camsys_rmsc_mcd = {
	.clks = camsys_rmsc_clks,
	.num_clks = CLK_CAMSYS_RMSC_NR_CLK,
};

static const struct mtk_gate_regs cam_ya_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_YA(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_ya_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_YA_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_ya_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_dummy,	\
	}

static const struct mtk_gate cam_ya_clks[] = {
	GATE_CAM_YA_DUMMY(CLK_CAM_YA_LARBX, "cam_ya_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_YA(CLK_CAM_YA_LARBX_PDCK, "cam_ya_larbx_pdck",
			"cam_ck"/* parent */, 0),
	GATE_CAM_YA(CLK_CAM_YA_CAM, "cam_ya_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_YA(CLK_CAM_YA_CAMTG, "cam_ya_camtg",
			"camtm_ck"/* parent */, 2),
};

static const struct mtk_clk_desc cam_ya_mcd = {
	.clks = cam_ya_clks,
	.num_clks = CLK_CAM_YA_NR_CLK,
};

static const struct mtk_gate_regs cam_yb_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_YB(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_yb_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_YB_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_yb_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_dummy,	\
	}

static const struct mtk_gate cam_yb_clks[] = {
	GATE_CAM_YB_DUMMY(CLK_CAM_YB_LARBX, "cam_yb_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_YB(CLK_CAM_YB_LARBX_PDCK, "cam_yb_larbx_pdck",
			"cam_ck"/* parent */, 0),
	GATE_CAM_YB(CLK_CAM_YB_CAM, "cam_yb_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_YB(CLK_CAM_YB_CAMTG, "cam_yb_camtg",
			"camtm_ck"/* parent */, 2),
};

static const struct mtk_clk_desc cam_yb_mcd = {
	.clks = cam_yb_clks,
	.num_clks = CLK_CAM_YB_NR_CLK,
};

static const struct mtk_gate_regs cam_yc_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_YC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_yc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_YC_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_yc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_dummy,	\
	}

static const struct mtk_gate cam_yc_clks[] = {
	GATE_CAM_YC_DUMMY(CLK_CAM_YC_LARBX, "cam_yc_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_YC(CLK_CAM_YC_LARBX_PDCK, "cam_yc_larbx_pdck",
			"cam_ck"/* parent */, 0),
	GATE_CAM_YC(CLK_CAM_YC_CAM, "cam_yc_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_YC(CLK_CAM_YC_CAMTG, "cam_yc_camtg",
			"camtm_ck"/* parent */, 2),
};

static const struct mtk_clk_desc cam_yc_mcd = {
	.clks = cam_yc_clks,
	.num_clks = CLK_CAM_YC_NR_CLK,
};

static const struct mtk_gate_regs cam_vcore_cg_regs = {
	.set_ofs = 0x2C,
	.clr_ofs = 0x2C,
	.sta_ofs = 0x2C,
};

#define GATE_CAM_VCORE(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_vcore_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

static const struct mtk_gate cam_vcore_clks[] = {
	GATE_CAM_VCORE(CLK_CAM_V_MM0_SUBCOMM, "CAM_V_MM0_SUBCOMM",
			"mminfra_ck"/* parent */, 1),
};

static const struct mtk_clk_desc cam_vcore_mcd = {
	.clks = cam_vcore_clks,
	.num_clks = CLK_CAM_VCORE_NR_CLK,
};

static const struct mtk_gate_regs ccu_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs ccu_hwv_regs = {
	.set_ofs = 0x0018,
	.clr_ofs = 0x001C,
	.sta_ofs = 0x1C0C,
};

#define GATE_CCU(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ccu_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_CCU(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ccu_cg_regs,			\
		.hwv_regs = &ccu_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER | CLK_EN_MM_INFRA_PWR,	\
	}

static const struct mtk_gate ccu_clks[] = {
	GATE_HWV_CCU(CLK_CCU_LARB30_CON, "ccu_larb30_con",
			"ccusys_ck"/* parent */, 0),
	GATE_HWV_CCU(CLK_CCU_AHB_CON, "ccu_ahb_con",
			"ccu_ahb_ck"/* parent */, 1),
	GATE_CCU(CLK_CCUSYS_CCU0_CON, "ccusys_ccu0_con",
			"ccusys_ck"/* parent */, 2),
	GATE_CCU(CLK_CCUSYS_CCU1_CON, "ccusys_ccu1_con",
			"ccusys_ck"/* parent */, 3),
	GATE_HWV_CCU(CLK_CCU2MM0_GALS_CON, "ccu2mm0_GCON",
			"ccusys_ck"/* parent */, 4),
};

static const struct mtk_clk_desc ccu_mcd = {
	.clks = ccu_clks,
	.num_clks = CLK_CCU_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6989_cam[] = {
	{
		.compatible = "mediatek,mt6989-camsys_ipe",
		.data = &camsys_ipe_mcd,
	}, {
		.compatible = "mediatek,mt6989-camsys_main",
		.data = &cam_m_mcd,
	}, {
		.compatible = "mediatek,mt6989-camsys_mraw",
		.data = &cam_mr_mcd,
	}, {
		.compatible = "mediatek,mt6989-camsys_rawa",
		.data = &cam_ra_mcd,
	}, {
		.compatible = "mediatek,mt6989-camsys_rawb",
		.data = &cam_rb_mcd,
	}, {
		.compatible = "mediatek,mt6989-camsys_rawc",
		.data = &cam_rc_mcd,
	}, {
		.compatible = "mediatek,mt6989-camsys_rmsa",
		.data = &camsys_rmsa_mcd,
	}, {
		.compatible = "mediatek,mt6989-camsys_rmsb",
		.data = &camsys_rmsb_mcd,
	}, {
		.compatible = "mediatek,mt6989-camsys_rmsc",
		.data = &camsys_rmsc_mcd,
	}, {
		.compatible = "mediatek,mt6989-camsys_yuva",
		.data = &cam_ya_mcd,
	}, {
		.compatible = "mediatek,mt6989-camsys_yuvb",
		.data = &cam_yb_mcd,
	}, {
		.compatible = "mediatek,mt6989-camsys_yuvc",
		.data = &cam_yc_mcd,
	}, {
		.compatible = "mediatek,mt6989-cam_vcore",
		.data = &cam_vcore_mcd,
	}, {
		.compatible = "mediatek,mt6989-ccu",
		.data = &ccu_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6989_cam_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6989_cam_drv = {
	.probe = clk_mt6989_cam_grp_probe,
	.driver = {
		.name = "clk-mt6989-cam",
		.of_match_table = of_match_clk_mt6989_cam,
	},
};

module_platform_driver(clk_mt6989_cam_drv);
MODULE_LICENSE("GPL");
