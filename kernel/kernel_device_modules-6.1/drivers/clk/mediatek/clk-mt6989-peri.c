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

static const struct mtk_gate_regs impc_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate impc_clks[] = {
	GATE_IMPC(CLK_IMPC_I2C10, "impc_i2c10",
			"i2c_ck"/* parent */, 0),
	GATE_IMPC(CLK_IMPC_I2C11, "impc_i2c11",
			"i2c_ck"/* parent */, 1),
	GATE_IMPC(CLK_IMPC_I2C12, "impc_i2c12",
			"i2c_ck"/* parent */, 2),
	GATE_IMPC(CLK_IMPC_I2C13, "impc_i2c13",
			"i2c_ck"/* parent */, 3),
	GATE_IMPC(CLK_IMPC_SEC_EN, "impc_sec_en",
			"i2c_ck"/* parent */, 4),
};

static const struct mtk_clk_desc impc_mcd = {
	.clks = impc_clks,
	.num_clks = CLK_IMPC_NR_CLK,
};

static const struct mtk_gate_regs impes_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPES(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impes_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate impes_clks[] = {
	GATE_IMPES(CLK_IMPES_I2C2, "impes_i2c2",
			"i2c_ck"/* parent */, 0),
	GATE_IMPES(CLK_IMPES_I2C4, "impes_i2c4",
			"i2c_ck"/* parent */, 1),
	GATE_IMPES(CLK_IMPES_I2C7, "impes_i2c7",
			"i2c_ck"/* parent */, 2),
	GATE_IMPES(CLK_IMPES_I2C8, "impes_i2c8",
			"i2c_ck"/* parent */, 3),
	GATE_IMPES(CLK_IMPES_SEC_EN, "impes_sec_en",
			"i2c_ck"/* parent */, 4),
};

static const struct mtk_clk_desc impes_mcd = {
	.clks = impes_clks,
	.num_clks = CLK_IMPES_NR_CLK,
};

static const struct mtk_gate_regs impn_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPN(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impn_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate impn_clks[] = {
	GATE_IMPN(CLK_IMPN_I2C0, "impn_i2c0",
			"i2c_ck"/* parent */, 0),
	GATE_IMPN(CLK_IMPN_I2C6, "impn_i2c6",
			"i2c_ck"/* parent */, 1),
	GATE_IMPN(CLK_IMPN_SEC_EN, "impn_sec_en",
			"i2c_ck"/* parent */, 2),
};

static const struct mtk_clk_desc impn_mcd = {
	.clks = impn_clks,
	.num_clks = CLK_IMPN_NR_CLK,
};

static const struct mtk_gate_regs imps_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPS(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imps_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate imps_clks[] = {
	GATE_IMPS(CLK_IMPS_I2C1, "imps_i2c1",
			"i2c_ck"/* parent */, 0),
	GATE_IMPS(CLK_IMPS_I2C3, "imps_i2c3",
			"i2c_ck"/* parent */, 1),
	GATE_IMPS(CLK_IMPS_I2C5, "imps_i2c5",
			"i2c_ck"/* parent */, 2),
	GATE_IMPS(CLK_IMPS_I2C9, "imps_i2c9",
			"i2c_ck"/* parent */, 3),
	GATE_IMPS(CLK_IMPS_SEC_EN, "imps_sec_en",
			"i2c_ck"/* parent */, 4),
};

static const struct mtk_clk_desc imps_mcd = {
	.clks = imps_clks,
	.num_clks = CLK_IMPS_NR_CLK,
};

static const struct mtk_gate_regs perao0_cg_regs = {
	.set_ofs = 0x24,
	.clr_ofs = 0x28,
	.sta_ofs = 0x10,
};

static const struct mtk_gate_regs perao0_hwv_regs = {
	.set_ofs = 0x0020,
	.clr_ofs = 0x0024,
	.sta_ofs = 0x1C10,
};

static const struct mtk_gate_regs perao1_cg_regs = {
	.set_ofs = 0x2C,
	.clr_ofs = 0x30,
	.sta_ofs = 0x14,
};

static const struct mtk_gate_regs perao2_cg_regs = {
	.set_ofs = 0x34,
	.clr_ofs = 0x38,
	.sta_ofs = 0x18,
};

#define GATE_PERAO0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_PERAO0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "hw-voter-regmap",				\
		.regs = &perao0_cg_regs,			\
		.hwv_regs = &perao0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER,				\
	}

#define GATE_PERAO1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PERAO2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate perao_clks[] = {
	/* PERAO0 */
	GATE_PERAO0(CLK_PERAOP_UART0, "peraop_uart0",
			"uart_ck"/* parent */, 0),
	GATE_PERAO0(CLK_PERAOP_UART1, "peraop_uart1",
			"uart_ck"/* parent */, 1),
	GATE_PERAO0(CLK_PERAOP_UART2, "peraop_uart2",
			"uart_ck"/* parent */, 2),
	GATE_PERAO0(CLK_PERAOP_UART3, "peraop_uart3",
			"uart_ck"/* parent */, 3),
	GATE_PERAO0(CLK_PERAOP_PWM_H, "peraop_pwm_h",
			"peri_faxi_ck"/* parent */, 4),
	GATE_PERAO0(CLK_PERAOP_PWM_B, "peraop_pwm_b",
			"pwm_ck"/* parent */, 5),
	GATE_PERAO0(CLK_PERAOP_PWM_FB1, "peraop_pwm_fb1",
			"pwm_ck"/* parent */, 6),
	GATE_PERAO0(CLK_PERAOP_PWM_FB2, "peraop_pwm_fb2",
			"pwm_ck"/* parent */, 7),
	GATE_PERAO0(CLK_PERAOP_PWM_FB3, "peraop_pwm_fb3",
			"pwm_ck"/* parent */, 8),
	GATE_PERAO0(CLK_PERAOP_PWM_FB4, "peraop_pwm_fb4",
			"pwm_ck"/* parent */, 9),
	GATE_PERAO0(CLK_PERAOP_DISP_PWM0, "peraop_disp_pwm0",
			"disp_pwm_ck"/* parent */, 10),
	GATE_PERAO0(CLK_PERAOP_DISP_PWM1, "peraop_disp_pwm1",
			"disp_pwm_ck"/* parent */, 11),
	GATE_HWV_PERAO0(CLK_PERAOP_SPI0_B, "peraop_spi0_b",
			"spi0_b_ck"/* parent */, 12),
	GATE_HWV_PERAO0(CLK_PERAOP_SPI1_B, "peraop_spi1_b",
			"spi1_b_ck"/* parent */, 13),
	GATE_HWV_PERAO0(CLK_PERAOP_SPI2_B, "peraop_spi2_b",
			"spi2_b_ck"/* parent */, 14),
	GATE_HWV_PERAO0(CLK_PERAOP_SPI3_B, "peraop_spi3_b",
			"spi3_b_ck"/* parent */, 15),
	GATE_HWV_PERAO0(CLK_PERAOP_SPI4_B, "peraop_spi4_b",
			"spi4_b_ck"/* parent */, 16),
	GATE_HWV_PERAO0(CLK_PERAOP_SPI5_B, "peraop_spi5_b",
			"spi5_b_ck"/* parent */, 17),
	GATE_HWV_PERAO0(CLK_PERAOP_SPI6_B, "peraop_spi6_b",
			"spi6_b_ck"/* parent */, 18),
	GATE_HWV_PERAO0(CLK_PERAOP_SPI7_B, "peraop_spi7_b",
			"spi7_b_ck"/* parent */, 19),
	/* PERAO1 */
	GATE_PERAO1(CLK_PERAOP_DMA_B, "peraop_dma_b",
			"peri_faxi_ck"/* parent */, 1),
	GATE_PERAO1(CLK_PERAOP_SSUSB0_FRMCNT, "peraop_ssusb0_frmcnt",
			"ssusb_fmcnt_ck"/* parent */, 4),
	GATE_PERAO1(CLK_PERAOP_SSUSB1_FRMCNT, "peraop_ssusb1_frmcnt",
			"ssusb_fmcnt_p1_ck"/* parent */, 11),
	GATE_PERAO1(CLK_PERAOP_MSDC1, "peraop_msdc1",
			"msdc30_1_ck"/* parent */, 17),
	GATE_PERAO1(CLK_PERAOP_MSDC1_F, "peraop_msdc1_f",
			"peri_faxi_ck"/* parent */, 18),
	GATE_PERAO1(CLK_PERAOP_MSDC1_H, "peraop_msdc1_h",
			"peri_faxi_ck"/* parent */, 19),
	GATE_PERAO1(CLK_PERAOP_MSDC2, "peraop_msdc2",
			"msdc30_2_ck"/* parent */, 20),
	GATE_PERAO1(CLK_PERAOP_MSDC2_F, "peraop_msdc2_f",
			"peri_faxi_ck"/* parent */, 21),
	GATE_PERAO1(CLK_PERAOP_MSDC2_H, "peraop_msdc2_h",
			"peri_faxi_ck"/* parent */, 22),
	/* PERAO2 */
	GATE_PERAO2(CLK_PERAOP_AUDIO_SLV, "peraop_audio_slv",
			"peri_faxi_ck"/* parent */, 0),
	GATE_PERAO2(CLK_PERAOP_AUDIO_MST, "peraop_audio_mst",
			"peri_faxi_ck"/* parent */, 1),
	GATE_PERAO2(CLK_PERAOP_AUDIO_INTBUS, "peraop_audio_intbus",
			"aud_intbus_ck"/* parent */, 2),
	GATE_PERAO2(CLK_PERAOP_AUDIO_ENGEN1, "peraop_audio_engen1",
			"aud_engen1_ck"/* parent */, 3),
	GATE_PERAO2(CLK_PERAOP_AUDIO_ENGEN2, "peraop_audio_engen2",
			"aud_engen2_ck"/* parent */, 4),
	GATE_PERAO2(CLK_PERAOP_AUDIO_TDMOUT_B, "peraop_aud_tdmob",
			"aud_1_ck"/* parent */, 5),
	GATE_PERAO2(CLK_PERAOP_AUDIO_H_AUD, "peraop_audio_h_aud",
			"audio_h_ck"/* parent */, 6),
};

static const struct mtk_clk_desc perao_mcd = {
	.clks = perao_clks,
	.num_clks = CLK_PERAO_NR_CLK,
};

static const struct mtk_gate_regs pext_cg_regs = {
	.set_ofs = 0x18,
	.clr_ofs = 0x1C,
	.sta_ofs = 0x14,
};

#define GATE_PEXT(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &pext_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate pext_clks[] = {
	GATE_PEXT(CLK_PEXTP_P0_MAC_REF, "pextp_p0_mac_ref",
			"vlp_lp_ref_ck"/* parent */, 0),
	GATE_PEXT(CLK_PEXTP_P0_MAC_PL_P, "pextp_p0_mac_pl_p",
			"f26m_ck"/* parent */, 1),
	GATE_PEXT(CLK_PEXTP_P0_MAC_TL, "pextp_p0_mac_tl",
			"vlp_lp_ref_ck"/* parent */, 2),
	GATE_PEXT(CLK_PEXTP_P0_MAC_AXI, "pextp_p0_mac_axi",
			"pextp_fmem_sub_ck"/* parent */, 3),
	GATE_PEXT(CLK_PEXTP_P0_MAC_AHB_APB, "pextp_p0_mac_ahb_apb",
			"pextp_faxi_ck"/* parent */, 4),
	GATE_PEXT(CLK_PEXTP_P0_PHY_REF, "pextp_p0_phy_ref",
			"vlp_lp_ref_ck"/* parent */, 5),
	GATE_PEXT(CLK_PEXTP_P0_PHY_MCU_BUS, "pextp_p0_phy_mcu_bus",
			"f26m_ck"/* parent */, 6),
	GATE_PEXT(CLK_PEXTP_P1_MAC_REF, "pextp_p1_mac_ref",
			"vlp_lp_ref_ck"/* parent */, 8),
	GATE_PEXT(CLK_PEXTP_P1_MAC_PL_P, "pextp_p1_mac_pl_p",
			"f26m_ck"/* parent */, 9),
	GATE_PEXT(CLK_PEXTP_P1_MAC_TL, "pextp_p1_mac_tl",
			"vlp_lp_ref_ck"/* parent */, 10),
	GATE_PEXT(CLK_PEXTP_P1_MAC_AXI, "pextp_p1_mac_axi",
			"pextp_fmem_sub_ck"/* parent */, 11),
	GATE_PEXT(CLK_PEXTP_P1_MAC_AHB_APB, "pextp_p1_mac_ahb_apb",
			"pextp_faxi_ck"/* parent */, 12),
	GATE_PEXT(CLK_PEXTP_P1_PHY_REF, "pextp_p1_phy_ref",
			"vlp_lp_ref_ck"/* parent */, 13),
	GATE_PEXT(CLK_PEXTP_P1_PHY_MCU_BUS, "pextp_p1_phy_mcu_bus",
			"f26m_ck"/* parent */, 14),
};

static const struct mtk_clk_desc pext_mcd = {
	.clks = pext_clks,
	.num_clks = CLK_PEXT_NR_CLK,
};

static const struct mtk_gate_regs ufsao_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x4,
};

#define GATE_UFSAO(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufsao_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate ufsao_clks[] = {
	GATE_UFSAO(CLK_UFSAO_UNIPRO_TX_SYM, "ufsao_unipro_tx_sym",
			"clk_null"/* parent */, 0),
	GATE_UFSAO(CLK_UFSAO_UNIPRO_RX_SYM0, "ufsao_unipro_rx_sym0",
			"clk_null"/* parent */, 1),
	GATE_UFSAO(CLK_UFSAO_UNIPRO_RX_SYM1, "ufsao_unipro_rx_sym1",
			"clk_null"/* parent */, 2),
	GATE_UFSAO(CLK_UFSAO_UNIPRO_SYS, "ufsao_unipro_sys",
			"ufs_ck"/* parent */, 3),
	GATE_UFSAO(CLK_UFSAO_UNIPRO_PHY_SAP, "ufsao_unipro_phy_sap",
			"f26m_ck"/* parent */, 8),
	GATE_UFSAO(CLK_UFSAO_PERI2UFS_BRIDGE_H, "ufsao_p2ufs_bridge_h",
			"ufs_faxi_ck"/* parent */, 9),
};

static const struct mtk_clk_desc ufsao_mcd = {
	.clks = ufsao_clks,
	.num_clks = CLK_UFSAO_NR_CLK,
};

static const struct mtk_gate_regs ufspdn_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x4,
};

#define GATE_UFSPDN(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufspdn_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate ufspdn_clks[] = {
	GATE_UFSPDN(CLK_UFSPDN_UFSHCI_UFS, "ufspdn_ufshci_ufs",
			"ufs_ck"/* parent */, 0),
	GATE_UFSPDN(CLK_UFSPDN_UFSHCI_AES, "ufspdn_ufshci_aes",
			"aes_ufsfde_ck"/* parent */, 1),
	GATE_UFSPDN(CLK_UFSPDN_UFSHCI_U_AHB, "ufspdn_ufshci_u_ahb",
			"ufs_faxi_ck"/* parent */, 3),
	GATE_UFSPDN(CLK_UFSPDN_UFSHCI_U_AXI, "ufspdn_ufshci_u_axi",
			"ufs_fmem_sub_ck"/* parent */, 5),
};

static const struct mtk_clk_desc ufspdn_mcd = {
	.clks = ufspdn_clks,
	.num_clks = CLK_UFSPDN_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6989_peri[] = {
	{
		.compatible = "mediatek,mt6989-imp_iic_wrap_c",
		.data = &impc_mcd,
	}, {
		.compatible = "mediatek,mt6989-imp_iic_wrap_es",
		.data = &impes_mcd,
	}, {
		.compatible = "mediatek,mt6989-imp_iic_wrap_n",
		.data = &impn_mcd,
	}, {
		.compatible = "mediatek,mt6989-imp_iic_wrap_s",
		.data = &imps_mcd,
	}, {
		.compatible = "mediatek,mt6989-pericfg_ao",
		.data = &perao_mcd,
	}, {
		.compatible = "mediatek,mt6989-pextpcfg_ao",
		.data = &pext_mcd,
	}, {
		.compatible = "mediatek,mt6989-ufscfg_ao",
		.data = &ufsao_mcd,
	}, {
		.compatible = "mediatek,mt6989-ufscfg_pdn",
		.data = &ufspdn_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6989_peri_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6989_peri_drv = {
	.probe = clk_mt6989_peri_grp_probe,
	.driver = {
		.name = "clk-mt6989-peri",
		.of_match_table = of_match_clk_mt6989_peri,
	},
};

module_platform_driver(clk_mt6989_peri_drv);
MODULE_LICENSE("GPL");
