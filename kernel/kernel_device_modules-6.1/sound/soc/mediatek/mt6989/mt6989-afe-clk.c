// SPDX-License-Identifier: GPL-2.0
/*
 *  mt6989-afe-clk.c  --  Mediatek 6989 afe clock ctrl
 *
 *  Copyright (c) 2023 MediaTek Inc.
 *  Author: Tina Tsai <tina.tsai@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/arm-smccc.h> /* for Kernel Native SMC API */
#include <linux/soc/mediatek/mtk_sip_svc.h> /* for SMC ID table */

#include "mt6989-afe-common.h"
#include "mt6989-afe-clk.h"

#if defined(CONFIG_FPGA_EARLY_PORTING)
int mt6989_init_clock(struct mtk_base_afe *afe) { return 0; }
int mt6989_afe_enable_clock(struct mtk_base_afe *afe) { return 0; }
void mt6989_afe_disable_clock(struct mtk_base_afe *afe) {}
int mt6989_afe_sram_request(struct mtk_base_afe *afe) { return 0; }
void mt6989_afe_sram_release(struct mtk_base_afe *afe) {}
int mt6989_afe_dram_request(struct device *dev) { return 0; }
int mt6989_afe_dram_release(struct device *dev) { return 0; }
int mt6989_apll1_enable(struct mtk_base_afe *afe) { return 0; }
void mt6989_apll1_disable(struct mtk_base_afe *afe) {}
int mt6989_apll2_enable(struct mtk_base_afe *afe) { return 0; }
void mt6989_apll2_disable(struct mtk_base_afe *afe) {}
int mt6989_get_apll_rate(struct mtk_base_afe *afe, int apll) { return 0; }
int mt6989_get_apll_by_rate(struct mtk_base_afe *afe, int rate) { return 0; }
int mt6989_get_apll_by_name(struct mtk_base_afe *afe, const char *name) { return 0; }
int mt6989_mck_enable(struct mtk_base_afe *afe, int mck_id, int rate) { return 0; }
int mt6989_mck_disable(struct mtk_base_afe *afe, int mck_id) { return 0; }
int mt6989_set_audio_int_bus_parent(struct mtk_base_afe *afe, int clk_id) { return 0; }
#else
static DEFINE_MUTEX(mutex_request_dram);

static const char *aud_clks[CLK_NUM] = {
	[CLK_HOPPING] = "aud_hopping_clk",
	[CLK_F26M] = "aud_f26m_clk",
	[CLK_UL0_ADC_CLK] = "aud_ul0_adc_clk",
	[CLK_UL0_ADC_HIRES_CLK] = "aud_ul0_adc_hires_clk",
	[CLK_UL1_ADC_CLK] = "aud_ul1_adc_clk",
	[CLK_UL1_ADC_HIRES_CLK] = "aud_ul1_adc_hires_clk",
	[CLK_UL2_ADC_CLK] = "aud_ul2_adc_clk",
	[CLK_UL2_ADC_HIRES_CLK] = "aud_ul2_adc_hires_clk",
	[CLK_APLL1] = "aud_apll1_clk",
	[CLK_APLL2] = "aud_apll2_clk",
	[CLK_APLL1_TUNER] = "aud_apll_tuner1_clk",
	[CLK_APLL2_TUNER] = "aud_apll_tuner2_clk",
	[CLK_MUX_AUDIOINTBUS] = "top_mux_audio_int",
	[CLK_TOP_MAINPLL_D4_D4] = "top_mainpll_d4_d4",
	[CLK_TOP_MUX_AUD_1] = "top_mux_aud_1",
	[CLK_TOP_APLL1_CK] = "top_apll1_ck",
	[CLK_TOP_MUX_AUD_2] = "top_mux_aud_2",
	[CLK_TOP_APLL2_CK] = "top_apll2_ck",
	[CLK_TOP_MUX_AUD_ENG1] = "top_mux_aud_eng1",
	[CLK_TOP_APLL1_D4] = "top_apll1_d4",
	[CLK_TOP_MUX_AUD_ENG2] = "top_mux_aud_eng2",
	[CLK_TOP_APLL2_D4] = "top_apll2_d4",
	[CLK_TOP_MUX_AUDIO_H] = "top_mux_audio_h",
	[CLK_TOP_I2SIN0_M_SEL] = "top_i2sin0_m_sel",
	[CLK_TOP_I2SIN1_M_SEL] = "top_i2sin1_m_sel",
	[CLK_TOP_FMI2S_M_SEL] = "top_fmi2s_m_sel",
	[CLK_TOP_TDMOUT_M_SEL] = "top_tdmout_m_sel",
	[CLK_TOP_APLL12_DIV_I2SIN0] = "top_apll12_div_i2sin0",
	[CLK_TOP_APLL12_DIV_I2SIN1] = "top_apll12_div_i2sin1",
	[CLK_TOP_APLL12_DIV_FMI2S] = "top_apll12_div_fmi2s",
	[CLK_TOP_APLL12_DIV_TDMOUT_M] = "top_apll12_div_tdmout_m",
	[CLK_TOP_APLL12_DIV_TDMOUT_B] = "top_apll12_div_tdmout_b",
	[CLK_CLK26M] = "top_clk26m_clk",
	[CLK_PERAO_AUDIO_SLV_CK_PERI] = "aud_slv_ck_peri",
	[CLK_PERAO_AUDIO_MST_CK_PERI] = "aud_mst_ck_peri",
	[CLK_PERAO_INTBUS_CK_PERI] = "aud_intbus_ck_peri",
	[CLK_PERAO_ENGEN1_CK] = "aud_engen1_ck_peri",
	[CLK_PERAO_ENGEN2_CK] = "aud_engen2_ck_peri",
	[CLK_PERAO_TDMOUT_B_CK] = "aud_tdmout_b_ck_peri",
	[CLK_PERAO_H_AUD_CK] = "aud_h_peri",
};

int mt6989_set_audio_int_bus_parent(struct mtk_base_afe *afe,
				    int clk_id)
{
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	if (clk_id < 0 || clk_id > CLK_CLK26M) {
		dev_info(afe->dev, "%s(), invalid clk_id %d\n", __func__, clk_id);
		return -EINVAL;
	}

	ret = clk_set_parent(afe_priv->clk[CLK_MUX_AUDIOINTBUS],
			     afe_priv->clk[clk_id]);
	if (ret) {
		dev_info(afe->dev, "%s() clk_set_parent %s-%s fail %d\n",
			__func__, aud_clks[CLK_MUX_AUDIOINTBUS],
			aud_clks[clk_id], ret);
	}

	return ret;
}
int mt6989_set_audio_h_parent(struct mtk_base_afe *afe,
				    int clk_id)
{
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	if (clk_id < 0 || clk_id > CLK_CLK26M) {
		dev_info(afe->dev, "%s(), invalid clk_id %d\n", __func__, clk_id);
		return -EINVAL;
	}

	ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUDIO_H],
			     afe_priv->clk[clk_id]);
	if (ret) {
		dev_info(afe->dev, "%s() clk_set_parent %s-%s fail %d\n",
			__func__, aud_clks[CLK_TOP_MUX_AUDIO_H],
			aud_clks[clk_id], ret);
	}
	return ret;
}
static int apll1_mux_setting(struct mtk_base_afe *afe, bool enable)
{
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	if (enable) {
		ret = clk_prepare_enable(afe_priv->clk[CLK_PERAO_ENGEN1_CK]);
		if (ret) {
			dev_info(afe->dev, "%s() clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_PERAO_ENGEN1_CK], ret);
			goto CLK_PERAO_ENGEN1_CK_ERR;
		}

		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_1]);
		if (ret) {
			dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_1], ret);
			goto EXIT;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_1],
				     afe_priv->clk[CLK_TOP_APLL1_CK]);
		if (ret) {
			dev_info(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_1],
				aud_clks[CLK_TOP_APLL1_CK], ret);
			goto EXIT;
		}

		/* 180.6336 / 4 = 45.1584MHz */
		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1]);
		if (ret) {
			dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG1], ret);
			goto EXIT;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1],
				     afe_priv->clk[CLK_TOP_APLL1_D4]);
		if (ret) {
			dev_info(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG1],
				aud_clks[CLK_TOP_APLL1_D4], ret);
			goto EXIT;
		}
		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUDIO_H]);
		if (ret) {
			dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				 __func__, aud_clks[CLK_TOP_MUX_AUDIO_H], ret);
			goto EXIT;
		}

		mt6989_set_audio_h_parent(afe, CLK_TOP_APLL1_CK);
	} else {
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1],
				     afe_priv->clk[CLK_CLK26M]);
		if (ret) {
			dev_info(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG1],
				aud_clks[CLK_CLK26M], ret);
			goto EXIT;
		}
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1]);

		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_1],
				     afe_priv->clk[CLK_CLK26M]);
		if (ret) {
			dev_info(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_1],
				aud_clks[CLK_CLK26M], ret);
			goto EXIT;
		}
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_1]);

		mt6989_set_audio_h_parent(afe, CLK_CLK26M);
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUDIO_H]);

		clk_disable_unprepare(afe_priv->clk[CLK_PERAO_ENGEN1_CK]);
	}

CLK_PERAO_ENGEN1_CK_ERR:
EXIT:
	return 0;
}

static int apll2_mux_setting(struct mtk_base_afe *afe, bool enable)
{
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	if (enable) {

		ret = clk_prepare_enable(afe_priv->clk[CLK_PERAO_ENGEN2_CK]);
		if (ret) {
			dev_info(afe->dev, "%s() clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_PERAO_ENGEN2_CK], ret);
			goto CLK_PERAO_ENGEN2_CK_ERR;
		}

		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_2]);
		if (ret) {
			dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_2], ret);
			goto EXIT;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_2],
				     afe_priv->clk[CLK_TOP_APLL2_CK]);
		if (ret) {
			dev_info(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_2],
				aud_clks[CLK_TOP_APLL2_CK], ret);
			goto EXIT;
		}

		/* 196.608 / 4 = 49.152MHz */
		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_ENG2]);
		if (ret) {
			dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG2], ret);
			goto EXIT;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_ENG2],
				     afe_priv->clk[CLK_TOP_APLL2_D4]);
		if (ret) {
			dev_info(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG2],
				aud_clks[CLK_TOP_APLL2_D4], ret);
			goto EXIT;
		}
		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUDIO_H]);
		if (ret) {
			dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				 __func__, aud_clks[CLK_TOP_MUX_AUDIO_H], ret);
			goto EXIT;
		}

		mt6989_set_audio_h_parent(afe, CLK_TOP_APLL2_CK);
	} else {
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_ENG2],
				     afe_priv->clk[CLK_CLK26M]);
		if (ret) {
			dev_info(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG2],
				aud_clks[CLK_CLK26M], ret);
			goto EXIT;
		}
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_ENG2]);

		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_2],
				     afe_priv->clk[CLK_CLK26M]);
		if (ret) {
			dev_info(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_2],
				aud_clks[CLK_CLK26M], ret);
			goto EXIT;
		}
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_2]);

		mt6989_set_audio_h_parent(afe, CLK_CLK26M);
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUDIO_H]);

		clk_disable_unprepare(afe_priv->clk[CLK_PERAO_ENGEN2_CK]);
	}
CLK_PERAO_ENGEN2_CK_ERR:
EXIT:
	return 0;
}
int mt6989_afe_disable_apll(struct mtk_base_afe *afe)
{
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	dev_dbg(afe->dev, "%s() successfully start\n", __func__);

	ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUDIO_H]);
	if (ret) {
		dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			 __func__, aud_clks[CLK_TOP_MUX_AUDIO_H], ret);
		goto EXIT;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_1]);
	if (ret) {
		dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_TOP_MUX_AUD_1], ret);
		goto EXIT;
	}

	ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_1],
			     afe_priv->clk[CLK_CLK26M]);
	if (ret) {
		dev_info(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
			__func__, aud_clks[CLK_TOP_MUX_AUD_1],
			aud_clks[CLK_CLK26M], ret);
		goto EXIT;
	}
	ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_2]);
	if (ret) {
		dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_TOP_MUX_AUD_2], ret);
		goto EXIT;
	}

	ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_2],
			     afe_priv->clk[CLK_CLK26M]);
	if (ret) {
		dev_info(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
			__func__, aud_clks[CLK_TOP_MUX_AUD_2],
			aud_clks[CLK_CLK26M], ret);
		goto EXIT;
	}

	clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_1]);
	clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_2]);
	mt6989_set_audio_h_parent(afe, CLK_CLK26M);
	clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUDIO_H]);

	return 0;
EXIT:
	return ret;

}

int mt6989_afe_enable_ao_clock(struct mtk_base_afe *afe)
{
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	dev_dbg(afe->dev, "%s() successfully start\n", __func__);
	/* Peri clock AO enable */
	ret = clk_prepare_enable(afe_priv->clk[CLK_PERAO_INTBUS_CK_PERI]);
	if (ret) {
		dev_info(afe->dev, "%s() clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_PERAO_INTBUS_CK_PERI], ret);
		goto CLK_PERAO_INTBUS_CK_PERI_ERR;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_PERAO_AUDIO_SLV_CK_PERI]);
	if (ret) {
		dev_info(afe->dev, "%s() clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_PERAO_AUDIO_SLV_CK_PERI], ret);
		goto CLK_PERAO_AUDIO_SLV_CK_PERI_ERR;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_PERAO_AUDIO_MST_CK_PERI]);
	if (ret) {
		dev_info(afe->dev, "%s() clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_PERAO_AUDIO_MST_CK_PERI], ret);
		goto CLK_PERAO_AUDIO_MST_CK_PERI_ERR;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_PERAO_H_AUD_CK]);
	if (ret) {
		dev_info(afe->dev, "%s() clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_PERAO_H_AUD_CK], ret);
		goto CLK_PERAO_H_AUD_CK_ERR;
	}

	return 0;

CLK_PERAO_H_AUD_CK_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_PERAO_H_AUD_CK]);
CLK_PERAO_AUDIO_MST_CK_PERI_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_PERAO_AUDIO_MST_CK_PERI]);
CLK_PERAO_AUDIO_SLV_CK_PERI_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_PERAO_AUDIO_SLV_CK_PERI]);
CLK_PERAO_INTBUS_CK_PERI_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_PERAO_INTBUS_CK_PERI]);

	return ret;
}

int mt6989_afe_apll_init(struct mtk_base_afe *afe)
{
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	/* APLL1_CON2 = 0x6f28bd4c
	 * APLL2_CON2 = 0x78FD5264
	 * APLL1_TUNER_CON0 = 0x6f28bd4d
	 * APLL2_TUNER_CON0 = 0x78fd5264
	 */
	regmap_write(afe_priv->apmixed, APLL2_TUNER_CON0, 0x78fd5265);

	return 0;
}


int mt6989_afe_enable_clock(struct mtk_base_afe *afe)
{
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;
#if !defined(SKIP_SMCC_SB)
	struct arm_smccc_res res;
#endif
	/* IPM2.0: USE HOPPING & 26M */
	ret = clk_prepare_enable(afe_priv->clk[CLK_HOPPING]);
	if (ret) {
		dev_info(afe->dev, "%s() clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_HOPPING], ret);
		goto CLK_AFE_ERR;
	}
	ret = clk_prepare_enable(afe_priv->clk[CLK_F26M]);
	if (ret) {
		dev_info(afe->dev, "%s() clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_F26M], ret);
		goto CLK_AFE_ERR;
	}

	/* IPM2.0: Remove CLK_MUX_AUDIO */

	ret = clk_prepare_enable(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);
	if (ret) {
		dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			 __func__, aud_clks[CLK_MUX_AUDIOINTBUS], ret);
		goto CLK_MUX_AUDIO_INTBUS_ERR;
	}
	ret = mt6989_set_audio_int_bus_parent(afe, CLK_CLK26M);

#if !defined(SKIP_SMCC_SB)
	/* use arm_smccc_smc to notify SPM */
	arm_smccc_smc(MTK_SIP_AUDIO_CONTROL,
		MTK_AUDIO_SMC_OP_DOMAIN_SIDEBANDS,
		0, 0, 0, 0, 0, 0, &res);
#endif
	return 0;

CLK_AFE_ERR:
	/* IPM2.0: Use HOPPING & 26M */
	clk_disable_unprepare(afe_priv->clk[CLK_HOPPING]);
	clk_disable_unprepare(afe_priv->clk[CLK_F26M]);
CLK_MUX_AUDIO_INTBUS_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);

	return ret;
}

void mt6989_afe_disable_clock(struct mtk_base_afe *afe)
{
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	dev_dbg(afe->dev, "%s() successfully start\n", __func__);

	mt6989_set_audio_int_bus_parent(afe, CLK_CLK26M);
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);
	/* IPM2.0: Use HOPPING & 26M */
	clk_disable_unprepare(afe_priv->clk[CLK_HOPPING]);
	clk_disable_unprepare(afe_priv->clk[CLK_F26M]);

}

int mt6989_afe_dram_request(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
#if !defined(SKIP_SMCC_SB)
	struct arm_smccc_res res;
#endif

	dev_dbg(dev, "%s(), dram_resource_counter %d\n",
		 __func__, afe_priv->dram_resource_counter);

	mutex_lock(&mutex_request_dram);

	/* use arm_smccc_smc to notify SPM */
	if (afe_priv->dram_resource_counter == 0)
#if !defined(SKIP_SMCC_SB)
		arm_smccc_smc(MTK_SIP_AUDIO_CONTROL,
			      MTK_AUDIO_SMC_OP_DRAM_REQUEST,
			      0, 0, 0, 0, 0, 0, &res);
#endif

	afe_priv->dram_resource_counter++;
	mutex_unlock(&mutex_request_dram);
	return 0;
}

int mt6989_afe_dram_release(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
#if !defined(SKIP_SMCC_SB)
	struct arm_smccc_res res;
#endif

	dev_dbg(dev, "%s(), dram_resource_counter %d\n",
		 __func__, afe_priv->dram_resource_counter);

	mutex_lock(&mutex_request_dram);
	afe_priv->dram_resource_counter--;

	/* use arm_smccc_smc to notify SPM */
#if !defined(SKIP_SMCC_SB)
	if (afe_priv->dram_resource_counter == 0)
		arm_smccc_smc(MTK_SIP_AUDIO_CONTROL,
			      MTK_AUDIO_SMC_OP_DRAM_RELEASE,
			      0, 0, 0, 0, 0, 0, &res);
#endif

	if (afe_priv->dram_resource_counter < 0) {
		dev_info(dev, "%s(), dram_resource_counter %d\n",
			 __func__, afe_priv->dram_resource_counter);
		afe_priv->dram_resource_counter = 0;
	}
	mutex_unlock(&mutex_request_dram);
	return 0;
}

int mt6989_afe_sram_request(struct mtk_base_afe *afe)
{
	struct arm_smccc_res res;

	/* use arm_smccc_smc to notify SPM */
	arm_smccc_smc(MTK_SIP_AUDIO_CONTROL,
				MTK_AUDIO_SMC_OP_SRAM_REQUEST,
				0, 0, 0, 0, 0, 0, &res);

	return 0;
}

void mt6989_afe_sram_release(struct mtk_base_afe *afe)
{
#if !defined(SKIP_SMCC_SB)
	struct arm_smccc_res res;

	/* use arm_smccc_smc to notify SPM */
	arm_smccc_smc(MTK_SIP_AUDIO_CONTROL,
				MTK_AUDIO_SMC_OP_SRAM_RELEASE,
				0, 0, 0, 0, 0, 0, &res);
#endif
}

int mt6989_apll1_enable(struct mtk_base_afe *afe)
{
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int ret;

	/* setting for APLL */
	apll1_mux_setting(afe, true);

	ret = clk_prepare_enable(afe_priv->clk[CLK_APLL1]);
	if (ret) {
		dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_APLL1], ret);
		goto ERR_CLK_APLL1;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_APLL1_TUNER]);
	if (ret) {
		dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_APLL1_TUNER], ret);
		goto ERR_CLK_APLL1_TUNER;
	}

	regmap_update_bits(afe->regmap, AFE_APLL1_TUNER_CFG,
			   0x0000FFF7, 0x00000372);
	regmap_update_bits(afe->regmap, AFE_APLL1_TUNER_CFG, 0x1, 0x1);

	regmap_update_bits(afe->regmap, AUDIO_ENGEN_CON0,
			   AUDIO_APLL1_EN_ON_MASK_SFT,
			   0x1 << AUDIO_APLL1_EN_ON_SFT);

	return 0;

ERR_CLK_APLL1_TUNER:
	clk_disable_unprepare(afe_priv->clk[CLK_APLL1_TUNER]);
ERR_CLK_APLL1:
	clk_disable_unprepare(afe_priv->clk[CLK_APLL1]);

	return ret;
}

void mt6989_apll1_disable(struct mtk_base_afe *afe)
{
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	regmap_update_bits(afe->regmap, AUDIO_ENGEN_CON0,
			   AUDIO_APLL1_EN_ON_MASK_SFT,
			   0x0 << AUDIO_APLL1_EN_ON_SFT);

	regmap_update_bits(afe->regmap, AFE_APLL1_TUNER_CFG, 0x1, 0x0);

	clk_disable_unprepare(afe_priv->clk[CLK_APLL1_TUNER]);
	clk_disable_unprepare(afe_priv->clk[CLK_APLL1]);

	apll1_mux_setting(afe, false);
}

int mt6989_apll2_enable(struct mtk_base_afe *afe)
{
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int ret;

	/* setting for APLL */
	apll2_mux_setting(afe, true);

	ret = clk_prepare_enable(afe_priv->clk[CLK_APLL2]);
	if (ret) {
		dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_APLL2], ret);
		goto ERR_CLK_APLL2;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_APLL2_TUNER]);
	if (ret) {
		dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_APLL2_TUNER], ret);
		goto ERR_CLK_APLL2_TUNER;
	}

	regmap_update_bits(afe->regmap, AFE_APLL2_TUNER_CFG,
			   0x0000FFF7, 0x00000374);
	regmap_update_bits(afe->regmap, AFE_APLL2_TUNER_CFG, 0x1, 0x1);

	regmap_update_bits(afe->regmap, AUDIO_ENGEN_CON0,
			   AUDIO_APLL2_EN_ON_MASK_SFT,
			   0x1 << AUDIO_APLL2_EN_ON_SFT);

	return 0;

ERR_CLK_APLL2_TUNER:
	clk_disable_unprepare(afe_priv->clk[CLK_APLL2_TUNER]);
ERR_CLK_APLL2:
	clk_disable_unprepare(afe_priv->clk[CLK_APLL2]);

	return ret;

	return 0;
}

void mt6989_apll2_disable(struct mtk_base_afe *afe)
{
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	regmap_update_bits(afe->regmap, AUDIO_ENGEN_CON0,
			   AUDIO_APLL2_EN_ON_MASK_SFT,
			   0x0 << AUDIO_APLL2_EN_ON_SFT);

	regmap_update_bits(afe->regmap, AFE_APLL2_TUNER_CFG, 0x1, 0x0);

	clk_disable_unprepare(afe_priv->clk[CLK_APLL2_TUNER]);
	clk_disable_unprepare(afe_priv->clk[CLK_APLL2]);

	apll2_mux_setting(afe, false);
}

int mt6989_get_apll_rate(struct mtk_base_afe *afe, int apll)
{
	return (apll == MT6989_APLL1) ? 180633600 : 196608000;
}

int mt6989_get_apll_by_rate(struct mtk_base_afe *afe, int rate)
{
	return ((rate % 8000) == 0) ? MT6989_APLL2 : MT6989_APLL1;
}

int mt6989_get_apll_by_name(struct mtk_base_afe *afe, const char *name)
{
	if (strcmp(name, APLL1_W_NAME) == 0)
		return MT6989_APLL1;
	else
		return MT6989_APLL2;
}

/* mck */
struct mt6989_mck_div {
	int m_sel_id;
	int div_clk_id;
	/* below will be deprecated */
	int div_pdn_reg;
	int div_pdn_mask_sft;
	int div_reg;
	int div_mask_sft;
	int div_mask;
	int div_sft;
	int div_msb_clk_id;
	int div_msb_reg;
	int div_msb_mask_sft;
	int div_msb_mask;
	int div_msb_sft;
	int div_apll_sel_reg;
	int div_apll_sel_mask_sft;
	int div_apll_sel_sft;
	int div_inv_reg;
	int div_inv_mask_sft;
};


static const struct mt6989_mck_div mck_div[MT6989_MCK_NUM] = {
	[MT6989_I2SIN0_MCK] = {
		.m_sel_id = CLK_TOP_I2SIN0_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV_I2SIN0,
		.div_pdn_reg = CLK_AUDDIV_0,
		.div_pdn_mask_sft = APLL12_DIV_I2SIN0_PDN_MASK_SFT,
		.div_reg = CLK_AUDDIV_2,
		.div_mask_sft = APLL12_CK_DIV_I2SIN0_MASK_SFT,
		.div_mask = APLL12_CK_DIV_I2SIN0_MASK,
		.div_sft = APLL12_CK_DIV_I2SIN0_SFT,
		.div_apll_sel_reg = CLK_AUDDIV_0,
		.div_apll_sel_mask_sft = APLL_I2SIN0_MCK_SEL_MASK_SFT,
		.div_apll_sel_sft = APLL_I2SIN0_MCK_SEL_SFT,
		.div_inv_reg = CLK_AUDDIV_1,
		.div_inv_mask_sft = APLL12_DIV_I2SIN0_INV_SFT,
	},
	[MT6989_I2SIN1_MCK] = {
		.m_sel_id = CLK_TOP_I2SIN1_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV_I2SIN1,
		.div_pdn_reg = CLK_AUDDIV_0,
		.div_pdn_mask_sft = APLL12_DIV_I2SIN1_PDN_MASK_SFT,
		.div_reg = CLK_AUDDIV_2,
		.div_mask_sft = APLL12_CK_DIV_I2SIN1_MASK_SFT,
		.div_mask = APLL12_CK_DIV_I2SIN1_MASK,
		.div_sft = APLL12_CK_DIV_I2SIN1_SFT,
		.div_apll_sel_reg = CLK_AUDDIV_0,
		.div_apll_sel_mask_sft = APLL_I2SIN1_MCK_SEL_MASK_SFT,
		.div_apll_sel_sft = APLL_I2SIN1_MCK_SEL_SFT,
		.div_inv_reg = CLK_AUDDIV_1,
		.div_inv_mask_sft = APLL12_DIV_I2SIN1_INV_SFT,
	},
	[MT6989_FMI2S_MCK] = {
		.m_sel_id = CLK_TOP_FMI2S_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV_FMI2S,
		.div_pdn_reg = CLK_AUDDIV_0,
		.div_pdn_mask_sft = APLL12_DIV_FMI2S_PDN_MASK_SFT,
		.div_reg = CLK_AUDDIV_5,
		.div_mask_sft = APLL12_CK_DIV_FMI2S_MASK_SFT,
		.div_mask = APLL12_CK_DIV_FMI2S_MASK,
		.div_sft = APLL12_CK_DIV_FMI2S_SFT,
		.div_apll_sel_reg = CLK_AUDDIV_0,
		.div_apll_sel_mask_sft = APLL_FMI2S_MCK_SEL_MASK_SFT,
		.div_apll_sel_sft = APLL_FMI2S_MCK_SEL_SFT,
		.div_inv_reg = CLK_AUDDIV_1,
		.div_inv_mask_sft = APLL12_DIV_FMI2S_INV_SFT,
	},
	[MT6989_TDMOUT_MCK] = {
		.m_sel_id = CLK_TOP_TDMOUT_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV_TDMOUT_M,
		.div_pdn_reg = CLK_AUDDIV_0,
		.div_pdn_mask_sft = APLL12_DIV_TDMOUT_M_PDN_MASK_SFT,
		.div_reg = CLK_AUDDIV_5,
		.div_mask_sft = APLL12_CK_DIV_TDMOUT_M_MASK_SFT,
		.div_mask = APLL12_CK_DIV_TDMOUT_M_MASK,
		.div_sft = APLL12_CK_DIV_TDMOUT_M_SFT,
		.div_apll_sel_reg = CLK_AUDDIV_0,
		.div_apll_sel_mask_sft = APLL_TDMOUT_MCK_SEL_MASK_SFT,
		.div_apll_sel_sft = APLL_TDMOUT_MCK_SEL_SFT,
		.div_inv_reg = CLK_AUDDIV_1,
		.div_inv_mask_sft = APLL12_DIV_TDMOUT_M_INV_SFT,
	},
	[MT6989_TDMOUT_BCK] = {
		.m_sel_id = -1,
		.div_clk_id = CLK_TOP_APLL12_DIV_TDMOUT_B,
		.div_pdn_reg = CLK_AUDDIV_0,
		.div_pdn_mask_sft = APLL12_DIV_TDMOUT_B_PDN_MASK_SFT,
		.div_reg = CLK_AUDDIV_5,
		.div_mask_sft = APLL12_CK_DIV_TDMOUT_B_MASK_SFT,
		.div_mask = APLL12_CK_DIV_TDMOUT_B_MASK,
		.div_sft = APLL12_CK_DIV_TDMOUT_B_SFT,
		.div_inv_reg = CLK_AUDDIV_1,
		.div_inv_mask_sft = APLL12_DIV_TDMOUT_B_INV_SFT,
	},
};

int mt6989_mck_enable(struct mtk_base_afe *afe, int mck_id, int rate)
{
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int apll = mt6989_get_apll_by_rate(afe, rate);
	int apll_clk_id = apll == MT6989_APLL1 ?
			  CLK_TOP_MUX_AUD_1 : CLK_TOP_MUX_AUD_2;
	int m_sel_id = 0;
	int div_clk_id = 0;
	int ret = 0;

	if (mck_id < 0) {
		dev_info(afe->dev, "%s(), invalid mck_id %d\n", __func__, mck_id);
		return -EINVAL;
	}
	m_sel_id = mck_div[mck_id].m_sel_id;
	div_clk_id = mck_div[mck_id].div_clk_id;

	/* select apll */
	if (m_sel_id >= 0) {
		ret = clk_prepare_enable(afe_priv->clk[m_sel_id]);
		if (ret) {
			dev_info(afe->dev, "%s(), clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[m_sel_id], ret);
			return ret;
		}
		ret = clk_set_parent(afe_priv->clk[m_sel_id],
				     afe_priv->clk[apll_clk_id]);
		if (ret) {
			dev_info(afe->dev, "%s(), clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[m_sel_id],
				aud_clks[apll_clk_id], ret);
			return ret;
		}
	}

	/* enable div, set rate */
	if (div_clk_id < 0) {
		dev_info(afe->dev, "%s(), invalid div_clk_id %d\n", __func__, div_clk_id);
		return -EINVAL;
	}
	ret = clk_prepare_enable(afe_priv->clk[div_clk_id]);
	if (ret) {
		dev_info(afe->dev, "%s(), clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[div_clk_id], ret);
		return ret;
	}
	ret = clk_set_rate(afe_priv->clk[div_clk_id], rate);
	if (ret) {
		dev_info(afe->dev, "%s(), clk_set_rate %s, rate %d, fail %d\n",
			__func__, aud_clks[div_clk_id],
			rate, ret);
		return ret;
	}


	/* enable div, set rate */
	div_clk_id = mck_div[mck_id].div_msb_clk_id;

	if (div_clk_id < 0) {
		dev_info(afe->dev, "%s(), invalid div_clk_id %d\n", __func__, div_clk_id);
		return -EINVAL;
	}
	ret = clk_prepare_enable(afe_priv->clk[div_clk_id]);
	if (ret) {
		dev_info(afe->dev, "%s(), clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[div_clk_id], ret);
		return ret;
	}
	ret = clk_set_rate(afe_priv->clk[div_clk_id], rate);
	if (ret) {
		dev_info(afe->dev, "%s(), clk_set_rate %s, rate %d, fail %d\n",
			__func__, aud_clks[div_clk_id],
			rate, ret);
		return ret;
	}

	return 0;
}

int mt6989_mck_disable(struct mtk_base_afe *afe, int mck_id)
{
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int m_sel_id = 0;
	int div_clk_id = 0;

	if (mck_id < 0) {
		dev_info(afe->dev, "%s(), mck_id = %d < 0\n",
				__func__, mck_id);
		return -EINVAL;
	}

	m_sel_id = mck_div[mck_id].m_sel_id;
	div_clk_id = mck_div[mck_id].div_clk_id;

	if (div_clk_id < 0) {
		dev_info(afe->dev, "%s(), div_clk_id = %d < 0\n",
				__func__, div_clk_id);
		return -EINVAL;
	}
	clk_disable_unprepare(afe_priv->clk[div_clk_id]);


	if (m_sel_id >= 0)
		clk_disable_unprepare(afe_priv->clk[m_sel_id]);

	return 0;
}

int mt6989_init_clock(struct mtk_base_afe *afe)
{
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int i = 0;

	afe_priv->clk = devm_kcalloc(afe->dev, CLK_NUM, sizeof(*afe_priv->clk),
				     GFP_KERNEL);
	if (!afe_priv->clk)
		return -ENOMEM;

	for (i = 0; i < CLK_NUM; i++) {
		afe_priv->clk[i] = devm_clk_get(afe->dev, aud_clks[i]);
		if (IS_ERR(afe_priv->clk[i])) {
			dev_info(afe->dev, "%s devm_clk_get %s fail, ret %ld\n",
				 __func__,
				 aud_clks[i], PTR_ERR(afe_priv->clk[i]));
			/*return PTR_ERR(clks[i]);*/
			afe_priv->clk[i] = NULL;
		}
	}

	afe_priv->apmixed = syscon_regmap_lookup_by_phandle(afe->dev->of_node,
			    "apmixedsys");
	if (IS_ERR(afe_priv->apmixed)) {
		dev_info(afe->dev, "%s() Cannot find apmixedsys: %ld\n",
			__func__, PTR_ERR(afe_priv->apmixed));
		return PTR_ERR(afe_priv->apmixed);
	}

	afe_priv->topckgen = syscon_regmap_lookup_by_phandle(afe->dev->of_node,
			     "topckgen");
	if (IS_ERR(afe_priv->topckgen)) {
		dev_info(afe->dev, "%s() Cannot find topckgen controller: %ld\n",
			__func__, PTR_ERR(afe_priv->topckgen));
		return PTR_ERR(afe_priv->topckgen);
	}

	afe_priv->infracfg = syscon_regmap_lookup_by_phandle(
				     afe->dev->of_node,
				     "infracfg");
	if (IS_ERR(afe_priv->infracfg)) {
		dev_info(afe->dev, "%s() Cannot find infracfg: %ld\n",
			__func__, PTR_ERR(afe_priv->infracfg));
		return PTR_ERR(afe_priv->infracfg);
	}
	mt6989_afe_apll_init(afe);
	mt6989_afe_disable_apll(afe);
	mt6989_afe_enable_ao_clock(afe);
	return 0;
}
#endif
