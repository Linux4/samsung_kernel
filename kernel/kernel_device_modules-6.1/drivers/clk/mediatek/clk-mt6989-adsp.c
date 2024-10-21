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

static const struct mtk_gate_regs afe0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs afe1_cg_regs = {
	.set_ofs = 0x10,
	.clr_ofs = 0x10,
	.sta_ofs = 0x10,
};

static const struct mtk_gate_regs afe2_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x4,
	.sta_ofs = 0x4,
};

static const struct mtk_gate_regs afe3_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0x8,
	.sta_ofs = 0x8,
};

static const struct mtk_gate_regs afe4_cg_regs = {
	.set_ofs = 0xC,
	.clr_ofs = 0xC,
	.sta_ofs = 0xC,
};

#define GATE_AFE0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE3(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe3_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE4(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe4_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate afe_clks[] = {
	/* AFE0 */
	GATE_AFE0(CLK_AFE_MEMIF_INTBUS_CLK, "afe_memif_intbus_clk",
			"aud_intbus_ck"/* parent */, 0),
	GATE_AFE0(CLK_AFE_MEMIF_HOP_CLK, "afe_memif_hop_clk",
			"f26m_ck"/* parent */, 1),
	GATE_AFE0(CLK_AFE_PCM1, "afe_pcm1",
			"f26m_ck"/* parent */, 13),
	GATE_AFE0(CLK_AFE_PCM0, "afe_pcm0",
			"f26m_ck"/* parent */, 14),
	GATE_AFE0(CLK_AFE_SOUNDWIRE, "afe_soundwire",
			"f26m_ck"/* parent */, 15),
	GATE_AFE0(CLK_AFE_CM1, "afe_cm1",
			"f26m_ck"/* parent */, 17),
	GATE_AFE0(CLK_AFE_CM0, "afe_cm0",
			"f26m_ck"/* parent */, 18),
	GATE_AFE0(CLK_AFE_STF, "afe_stf",
			"f26m_ck"/* parent */, 19),
	GATE_AFE0(CLK_AFE_HW_GAIN23, "afe_hw_gain23",
			"f26m_ck"/* parent */, 20),
	GATE_AFE0(CLK_AFE_HW_GAIN01, "afe_hw_gain01",
			"f26m_ck"/* parent */, 21),
	GATE_AFE0(CLK_AFE_DAIBT, "afe_daibt",
			"f26m_ck"/* parent */, 22),
	GATE_AFE0(CLK_AFE_MERGE_IF, "afe_merge_if",
			"f26m_ck"/* parent */, 23),
	GATE_AFE0(CLK_AFE_FM_I2S, "afe_fm_i2s",
			"f26m_ck"/* parent */, 24),
	/* AFE1 */
	GATE_AFE1(CLK_AFE_AUDIO_HOPPING, "afe_audio_hopping_ck",
			"f26m_ck"/* parent */, 0),
	GATE_AFE1(CLK_AFE_AUDIO_F26M, "afe_audio_f26m_ck",
			"f26m_ck"/* parent */, 1),
	GATE_AFE1(CLK_AFE_APLL1, "afe_apll1_ck",
			"aud_1_ck"/* parent */, 2),
	GATE_AFE1(CLK_AFE_APLL2, "afe_apll2_ck",
			"aud_2_ck"/* parent */, 3),
	GATE_AFE1(CLK_AFE_H208M, "afe_h208m_ck",
			"audio_h_ck"/* parent */, 4),
	GATE_AFE1(CLK_AFE_APLL_TUNER2, "afe_apll_tuner2",
			"aud_engen2_ck"/* parent */, 12),
	GATE_AFE1(CLK_AFE_APLL_TUNER1, "afe_apll_tuner1",
			"aud_engen1_ck"/* parent */, 13),
	/* AFE2 */
	GATE_AFE2(CLK_AFE_UL2_ADC_HIRES_TML, "afe_ul2_aht",
			"audio_h_ck"/* parent */, 12),
	GATE_AFE2(CLK_AFE_UL2_ADC_HIRES, "afe_ul2_adc_hires",
			"audio_h_ck"/* parent */, 13),
	GATE_AFE2(CLK_AFE_UL2_TML, "afe_ul2_tml",
			"f26m_ck"/* parent */, 14),
	GATE_AFE2(CLK_AFE_UL2_ADC, "afe_ul2_adc",
			"f26m_ck"/* parent */, 15),
	GATE_AFE2(CLK_AFE_UL1_ADC_HIRES_TML, "afe_ul1_aht",
			"audio_h_ck"/* parent */, 16),
	GATE_AFE2(CLK_AFE_UL1_ADC_HIRES, "afe_ul1_adc_hires",
			"audio_h_ck"/* parent */, 17),
	GATE_AFE2(CLK_AFE_UL1_TML, "afe_ul1_tml",
			"f26m_ck"/* parent */, 18),
	GATE_AFE2(CLK_AFE_UL1_ADC, "afe_ul1_adc",
			"f26m_ck"/* parent */, 19),
	GATE_AFE2(CLK_AFE_UL0_ADC_HIRES_TML, "afe_ul0_aht",
			"audio_h_ck"/* parent */, 20),
	GATE_AFE2(CLK_AFE_UL0_ADC_HIRES, "afe_ul0_adc_hires",
			"audio_h_ck"/* parent */, 21),
	GATE_AFE2(CLK_AFE_UL0_TML, "afe_ul0_tml",
			"f26m_ck"/* parent */, 22),
	GATE_AFE2(CLK_AFE_UL0_ADC, "afe_ul0_adc",
			"f26m_ck"/* parent */, 23),
	/* AFE3 */
	GATE_AFE3(CLK_AFE_DPRX, "afe_dprx_ck",
			"aud_1_ck"/* parent */, 4),
	GATE_AFE3(CLK_AFE_DPTX, "afe_dptx_ck",
			"aud_1_ck"/* parent */, 5),
	GATE_AFE3(CLK_AFE_ETDM_IN6, "afe_etdm_in6",
			"aud_engen1_ck"/* parent */, 6),
	GATE_AFE3(CLK_AFE_ETDM_IN5, "afe_etdm_in5",
			"aud_engen1_ck"/* parent */, 7),
	GATE_AFE3(CLK_AFE_ETDM_IN4, "afe_etdm_in4",
			"aud_engen1_ck"/* parent */, 8),
	GATE_AFE3(CLK_AFE_ETDM_IN3, "afe_etdm_in3",
			"aud_engen1_ck"/* parent */, 9),
	GATE_AFE3(CLK_AFE_ETDM_IN2, "afe_etdm_in2",
			"aud_engen1_ck"/* parent */, 10),
	GATE_AFE3(CLK_AFE_ETDM_IN1, "afe_etdm_in1",
			"aud_engen1_ck"/* parent */, 11),
	GATE_AFE3(CLK_AFE_ETDM_IN0, "afe_etdm_in0",
			"aud_engen1_ck"/* parent */, 12),
	GATE_AFE3(CLK_AFE_ETDM_OUT6, "afe_etdm_out6",
			"aud_engen1_ck"/* parent */, 13),
	GATE_AFE3(CLK_AFE_ETDM_OUT5, "afe_etdm_out5",
			"aud_engen1_ck"/* parent */, 14),
	GATE_AFE3(CLK_AFE_ETDM_OUT4, "afe_etdm_out4",
			"aud_engen1_ck"/* parent */, 15),
	GATE_AFE3(CLK_AFE_ETDM_OUT3, "afe_etdm_out3",
			"aud_engen1_ck"/* parent */, 16),
	GATE_AFE3(CLK_AFE_ETDM_OUT2, "afe_etdm_out2",
			"aud_engen1_ck"/* parent */, 17),
	GATE_AFE3(CLK_AFE_ETDM_OUT1, "afe_etdm_out1",
			"aud_engen1_ck"/* parent */, 18),
	GATE_AFE3(CLK_AFE_ETDM_OUT0, "afe_etdm_out0",
			"aud_engen1_ck"/* parent */, 19),
	/* AFE4 */
	GATE_AFE4(CLK_AFE_GENERAL24_ASRC, "afe_general24_asrc",
			"f26m_ck"/* parent */, 0),
	GATE_AFE4(CLK_AFE_GENERAL23_ASRC, "afe_general23_asrc",
			"f26m_ck"/* parent */, 1),
	GATE_AFE4(CLK_AFE_GENERAL22_ASRC, "afe_general22_asrc",
			"f26m_ck"/* parent */, 2),
	GATE_AFE4(CLK_AFE_GENERAL21_ASRC, "afe_general21_asrc",
			"f26m_ck"/* parent */, 3),
	GATE_AFE4(CLK_AFE_GENERAL20_ASRC, "afe_general20_asrc",
			"f26m_ck"/* parent */, 4),
	GATE_AFE4(CLK_AFE_GENERAL19_ASRC, "afe_general19_asrc",
			"f26m_ck"/* parent */, 5),
	GATE_AFE4(CLK_AFE_GENERAL18_ASRC, "afe_general18_asrc",
			"f26m_ck"/* parent */, 6),
	GATE_AFE4(CLK_AFE_GENERAL17_ASRC, "afe_general17_asrc",
			"f26m_ck"/* parent */, 7),
	GATE_AFE4(CLK_AFE_GENERAL16_ASRC, "afe_general16_asrc",
			"f26m_ck"/* parent */, 8),
	GATE_AFE4(CLK_AFE_GENERAL15_ASRC, "afe_general15_asrc",
			"f26m_ck"/* parent */, 9),
	GATE_AFE4(CLK_AFE_GENERAL14_ASRC, "afe_general14_asrc",
			"f26m_ck"/* parent */, 10),
	GATE_AFE4(CLK_AFE_GENERAL13_ASRC, "afe_general13_asrc",
			"f26m_ck"/* parent */, 11),
	GATE_AFE4(CLK_AFE_GENERAL12_ASRC, "afe_general12_asrc",
			"f26m_ck"/* parent */, 12),
	GATE_AFE4(CLK_AFE_GENERAL11_ASRC, "afe_general11_asrc",
			"f26m_ck"/* parent */, 13),
	GATE_AFE4(CLK_AFE_GENERAL10_ASRC, "afe_general10_asrc",
			"f26m_ck"/* parent */, 14),
	GATE_AFE4(CLK_AFE_GENERAL9_ASRC, "afe_general9_asrc",
			"f26m_ck"/* parent */, 15),
	GATE_AFE4(CLK_AFE_GENERAL8_ASRC, "afe_general8_asrc",
			"f26m_ck"/* parent */, 16),
	GATE_AFE4(CLK_AFE_GENERAL7_ASRC, "afe_general7_asrc",
			"f26m_ck"/* parent */, 17),
	GATE_AFE4(CLK_AFE_GENERAL6_ASRC, "afe_general6_asrc",
			"f26m_ck"/* parent */, 18),
	GATE_AFE4(CLK_AFE_GENERAL5_ASRC, "afe_general5_asrc",
			"f26m_ck"/* parent */, 19),
	GATE_AFE4(CLK_AFE_GENERAL4_ASRC, "afe_general4_asrc",
			"f26m_ck"/* parent */, 20),
	GATE_AFE4(CLK_AFE_GENERAL3_ASRC, "afe_general3_asrc",
			"f26m_ck"/* parent */, 21),
	GATE_AFE4(CLK_AFE_GENERAL2_ASRC, "afe_general2_asrc",
			"f26m_ck"/* parent */, 22),
	GATE_AFE4(CLK_AFE_GENERAL1_ASRC, "afe_general1_asrc",
			"f26m_ck"/* parent */, 23),
	GATE_AFE4(CLK_AFE_GENERAL0_ASRC, "afe_general0_asrc",
			"f26m_ck"/* parent */, 24),
	GATE_AFE4(CLK_AFE_CONNSYS_I2S_ASRC, "afe_connsys_i2s_asrc",
			"f26m_ck"/* parent */, 25),
};

static const struct mtk_clk_desc afe_mcd = {
	.clks = afe_clks,
	.num_clks = CLK_AFE_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6989_adsp[] = {
	{
		.compatible = "mediatek,mt6989-audiosys",
		.data = &afe_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6989_adsp_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6989_adsp_drv = {
	.probe = clk_mt6989_adsp_grp_probe,
	.driver = {
		.name = "clk-mt6989-adsp",
		.of_match_table = of_match_clk_mt6989_adsp,
	},
};

module_platform_driver(clk_mt6989_adsp_drv);
MODULE_LICENSE("GPL");
