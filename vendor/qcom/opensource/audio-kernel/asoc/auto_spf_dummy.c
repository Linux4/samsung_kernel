/* Copyright (c) 2014-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/of_device.h>
#include <linux/pm_qos.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/info.h>
#include <dsp/audio_notifier.h>
#include <dsp/audio_prm.h>
#include "msm_dailink.h"
#include "msm_common.h"


#define DRV_NAME "spf-asoc-snd"
#define DRV_PINCTRL_NAME "audio-pcm-pinctrl"

#define __CHIPSET__ "SA8xx5 "
#define MSM_DAILINK_NAME(name) (__CHIPSET__#name)

enum pinctrl_pin_state {
	STATE_SLEEP = 0, /* All pins are in sleep state */
	STATE_ACTIVE,  /* TDM = active */
};

struct msm_pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *sleep;
	struct pinctrl_state *active;
	enum pinctrl_pin_state curr_state;
};

static const char *const pin_states[] = {"sleep", "active"};

static const char *const tdm_gpio_phandle[] = {"qcom,pri-tdm-gpios",
						"qcom,sec-tdm-gpios",
						"qcom,tert-tdm-gpios",
						"qcom,quat-tdm-gpios",
						"qcom,quin-tdm-gpios",
						"qcom,sen-tdm-gpios",
						"qcom,sep-tdm-gpios",
						"qcom,hsif0-tdm-gpios",
						"qcom,hsif1-tdm-gpios",
						"qcom,hsif2-tdm-gpios",
						"qcom,hsif3-tdm-gpios",
						"qcom,hsif4-tdm-gpios",
						};

static const char *const mclk_gpio_phandle[] = { "qcom,internal-mclk1-gpios" };

enum {
	TDM_PRI = 0,
	TDM_SEC,
	TDM_TERT,
	TDM_QUAT,
	TDM_QUIN,
	TDM_SEN,
	TDM_SEP,
	TDM_HSIF0,
	TDM_HSIF1,
	TDM_HSIF2,
	TDM_HSIF3,
	TDM_HSIF4,
	TDM_INTERFACE_MAX,
};

enum {
	IDX_PRIMARY_TDM_RX_0,
	IDX_PRIMARY_TDM_TX_0,
	IDX_SECONDARY_TDM_RX_0,
	IDX_SECONDARY_TDM_TX_0,
	IDX_TERTIARY_TDM_RX_0,
	IDX_TERTIARY_TDM_TX_0,
	IDX_QUATERNARY_TDM_RX_0,
	IDX_QUATERNARY_TDM_TX_0,
	IDX_QUINARY_TDM_RX_0,
	IDX_QUINARY_TDM_TX_0,
	IDX_SENARY_TDM_RX_0,
	IDX_SENARY_TDM_TX_0,
	IDX_SEPTENARY_TDM_RX_0,
	IDX_SEPTENARY_TDM_TX_0,
	IDX_HSIF0_TDM_RX_0,
	IDX_HSIF0_TDM_TX_0,
	IDX_HSIF1_TDM_RX_0,
	IDX_HSIF1_TDM_TX_0,
	IDX_HSIF2_TDM_RX_0,
	IDX_HSIF2_TDM_TX_0,
	IDX_HSIF3_TDM_RX_0,
	IDX_HSIF3_TDM_TX_0,
	IDX_HSIF4_TDM_RX_0,
	IDX_HSIF4_TDM_TX_0,
	IDX_GROUP_TDM_MAX,
};

enum {
	MCLK1 = 0,
	MCLK_MAX,
};

struct tdm_conf {
	struct mutex lock;
	u32 ref_cnt;
};

struct msm_asoc_mach_data {
	struct snd_info_entry *codec_root;
	struct msm_common_pdata *common_pdata;
	struct device_node *us_euro_gpio_p; /* used by pinctrl API */
	struct device_node *hph_en1_gpio_p; /* used by pinctrl API */
	struct device_node *hph_en0_gpio_p; /* used by pinctrl API */
	struct device_node *fsa_handle;
	struct snd_soc_codec *codec;
	struct work_struct adsp_power_up_work;
	struct tdm_conf tdm_intf_conf[TDM_INTERFACE_MAX];
	struct msm_pinctrl_info pinctrl_info[TDM_INTERFACE_MAX];
	struct msm_pinctrl_info mclk_pinctrl_info[MCLK_MAX];
};

static struct platform_device *spdev;

static bool codec_reg_done;

static struct clk_cfg internal_mclk[MCLK_MAX] = {
	{
		CLOCK_ID_MCLK_1,
		IBIT_CLOCK_12_P288_MHZ,
		CLOCK_ATTRIBUTE_COUPLE_NO,
		CLOCK_ROOT_DEFAULT,
	}
};

struct snd_soc_card sa8155_snd_soc_card_auto_msm = {
        .name = "sa8155-adp-star-snd-card",
};

struct snd_soc_card sa8295_snd_soc_card_auto_msm = {
	.name = "sa8295-adp-star-snd-card",
};

struct snd_soc_card sa8255_snd_soc_card_auto_msm = {
	.name = "sa8255-adp-star-snd-card",
};

static int msm_tdm_get_intf_idx(u16 id)
{
	switch (id) {
		case IDX_PRIMARY_TDM_RX_0:
		case IDX_PRIMARY_TDM_TX_0:
			return TDM_PRI;
		case IDX_SECONDARY_TDM_RX_0:
		case IDX_SECONDARY_TDM_TX_0:
			return TDM_SEC;
		case IDX_TERTIARY_TDM_RX_0:
		case IDX_TERTIARY_TDM_TX_0:
			return TDM_TERT;
		case IDX_QUATERNARY_TDM_RX_0:
		case IDX_QUATERNARY_TDM_TX_0:
			return TDM_QUAT;
		case IDX_QUINARY_TDM_RX_0:
		case IDX_QUINARY_TDM_TX_0:
			return TDM_QUIN;
		case IDX_SENARY_TDM_RX_0:
		case IDX_SENARY_TDM_TX_0:
			return TDM_SEN;
		case IDX_SEPTENARY_TDM_RX_0:
		case IDX_SEPTENARY_TDM_TX_0:
			return TDM_SEP;
		case IDX_HSIF0_TDM_RX_0:
		case IDX_HSIF0_TDM_TX_0:
			return TDM_HSIF0;
		case IDX_HSIF1_TDM_RX_0:
		case IDX_HSIF1_TDM_TX_0:
			return TDM_HSIF1;
		case IDX_HSIF2_TDM_RX_0:
		case IDX_HSIF2_TDM_TX_0:
                        return TDM_HSIF2;
		case IDX_HSIF3_TDM_RX_0:
		case IDX_HSIF3_TDM_TX_0:
                        return TDM_HSIF3;
		case IDX_HSIF4_TDM_RX_0:
		case IDX_HSIF4_TDM_TX_0:
                        return TDM_HSIF4;

		default: return -EINVAL;
	}
}

static int auto_adsp_notifier_service_cb(struct notifier_block *this,
					 unsigned long opcode, void *ptr)
{
	pr_debug("%s: Service opcode 0x%lx\n", __func__, opcode);

	switch (opcode) {
	case AUDIO_NOTIFIER_SERVICE_DOWN:
		snd_card_notify_user(SND_CARD_STATUS_OFFLINE);
		break;
	case AUDIO_NOTIFIER_SERVICE_UP:
		snd_card_notify_user(SND_CARD_STATUS_ONLINE);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block service_nb = {
	.notifier_call  = auto_adsp_notifier_service_cb,
	.priority = -INT_MAX,
};

static int msm_set_pinctrl(struct msm_pinctrl_info *pinctrl_info,
                                enum pinctrl_pin_state new_state)
{
	int ret = 0;
	int curr_state = 0;

	if (pinctrl_info == NULL) {
		pr_err("%s: pinctrl info is NULL\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	if (pinctrl_info->pinctrl == NULL) {
		pr_err("%s: pinctrl handle is NULL\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	curr_state = pinctrl_info->curr_state;
	pinctrl_info->curr_state = new_state;
	pr_debug("%s: curr_state = %s new_state = %s\n", __func__,
                 pin_states[curr_state], pin_states[pinctrl_info->curr_state]);

	if (curr_state == pinctrl_info->curr_state) {
		pr_err("%s: pin already in same state\n", __func__);
		goto err;
	}

	if (curr_state != STATE_SLEEP &&
		pinctrl_info->curr_state != STATE_SLEEP) {
		pr_err("%s: pin state is already active, cannot switch\n", __func__);
		ret = -EIO;
		goto err;
	}
	switch (pinctrl_info->curr_state) {
	case STATE_ACTIVE:
		ret = pinctrl_select_state(pinctrl_info->pinctrl,
                                        pinctrl_info->active);
		if (ret) {
			pr_err("%s: state select to active failed with %d\n",
                                __func__, ret);
			ret = -EIO;
			goto err;
		}
		break;
	case STATE_SLEEP:
		ret = pinctrl_select_state(pinctrl_info->pinctrl,
                                        pinctrl_info->sleep);
		if (ret) {
			pr_err("%s: state select to sleep failed with %d\n",
                                __func__, ret);
			ret = -EIO;
			goto err;
		}
		break;
	default:
		pr_err("%s: pin state is invalid\n", __func__);
		return -EINVAL;
	}

err:
	return ret;
}

static int tdm_snd_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct tdm_conf *intf_conf = NULL;
	struct msm_pinctrl_info *pinctrl_info = NULL;
	int ret_pinctrl = 0;
	int index;

	index = msm_tdm_get_intf_idx(dai_link->id);
	if (index < 0) {
		ret = -EINVAL;
		pr_err("%s: DAI link id (%d) out of range\n",
			__func__, dai_link->id);
		goto err;
	}

        /*
         * Mutex protection in case the same TDM
         * interface using for both TX and RX so
         * that the same clock won't be enable twice.
         */
	intf_conf = &pdata->tdm_intf_conf[index];
	mutex_lock(&intf_conf->lock);
	if (++intf_conf->ref_cnt == 1) {
		pinctrl_info = &pdata->pinctrl_info[index];
		if (pinctrl_info->pinctrl) {
			ret_pinctrl = msm_set_pinctrl(pinctrl_info,
                                                      STATE_ACTIVE);
			if (ret_pinctrl)
				pr_err("%s: TDM TLMM pinctrl set failed with %d\n",
                                        __func__, ret_pinctrl);
		}
	}
	mutex_unlock(&intf_conf->lock);

err:
	return ret;
}

static void tdm_snd_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_pinctrl_info *pinctrl_info = NULL;
	struct tdm_conf *intf_conf = NULL;
	int ret_pinctrl = 0;
	int index;

	pr_debug("%s: substream = %s, stream = %d\n", __func__,
                 substream->name, substream->stream);

	index = msm_tdm_get_intf_idx(dai_link->id);
	if (index < 0) {
		pr_err("%s: DAI link id (%d) out of range\n",
                        __func__, dai_link->id);
		return;
	}

	intf_conf = &pdata->tdm_intf_conf[index];
	mutex_lock(&intf_conf->lock);
	if (--intf_conf->ref_cnt == 0) {
		pinctrl_info = &pdata->pinctrl_info[index];
		if (pinctrl_info->pinctrl) {
			ret_pinctrl = msm_set_pinctrl(pinctrl_info,
                                                      STATE_SLEEP);
			if (ret_pinctrl)
                                pr_err("%s: TDM TLMM pinctrl set failed with %d\n",
                                        __func__, ret_pinctrl);
		}
	}
	mutex_unlock(&intf_conf->lock);
}

static struct snd_soc_ops tdm_be_ops = {
	.startup = tdm_snd_startup,
	.shutdown = tdm_snd_shutdown
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link msm_common_dai_links[] = {
/* BackEnd DAI Links */
{
	.name = "LPASS_BE_AUXPCM_RX_DUMMY",
	.stream_name = "AUXPCM-LPAIF-RX-PRIMARY",
	.dpcm_playback = 1,
	.ops = &tdm_be_ops,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	.id = IDX_PRIMARY_TDM_RX_0,
	SND_SOC_DAILINK_REG(lpass_be_auxpcm_rx_dummy),
},
{
	.name = "LPASS_BE_AUXPCM_TX_DUMMY",
	.stream_name = "AUXPCM-LPAIF-TX-PRIMARY",
	.dpcm_capture = 1,
	.ops = &tdm_be_ops,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	.id = IDX_PRIMARY_TDM_TX_0,
	SND_SOC_DAILINK_REG(lpass_be_auxpcm_tx_dummy),
},
{
	.name = "SEC_TDM_RX_0",
	.stream_name = "TDM-LPAIF-RX-SECONDARY",
	.dpcm_playback = 1,
        .ops = &tdm_be_ops,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	.id = IDX_SECONDARY_TDM_RX_0,
	SND_SOC_DAILINK_REG(sec_tdm_rx_0),
},
{
	.name = "SEC_TDM_TX_0",
	.stream_name = "TDM-LPAIF-TX-SECONDARY",
	.dpcm_capture = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
        .ops = &tdm_be_ops,
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
        .id = IDX_SECONDARY_TDM_TX_0,
	SND_SOC_DAILINK_REG(sec_tdm_tx_0),
},
{
	.name = "TERT_TDM_RX_0",
	.stream_name = "TDM-LPAIF-RX-TERTIARY",
	.dpcm_playback = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
	.ops = &tdm_be_ops,
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	.id = IDX_TERTIARY_TDM_RX_0,
	SND_SOC_DAILINK_REG(tert_tdm_rx_0),
},
{
	.name = "TERT_TDM_TX_0",
	.stream_name = "TDM-LPAIF-TX-TERTIARY",
	.dpcm_capture = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
	.ops = &tdm_be_ops,
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	.id = IDX_TERTIARY_TDM_TX_0,
	SND_SOC_DAILINK_REG(tert_tdm_tx_0),
},
{
	.name = "QUAT_TDM_RX_0",
	.stream_name = "TDM-LPAIF_RXTX-RX-PRIMARY",
	.dpcm_playback = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
	.ops = &tdm_be_ops,
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	.id = IDX_QUATERNARY_TDM_RX_0,
	SND_SOC_DAILINK_REG(quat_tdm_rx_0),
},
{
	.name = "QUAT_TDM_TX_0",
	.stream_name = "TDM-LPAIF_RXTX-TX-PRIMARY",
	.dpcm_capture = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
	.ops = &tdm_be_ops,
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	.id = IDX_QUATERNARY_TDM_TX_0,
	SND_SOC_DAILINK_REG(quat_tdm_tx_0),
},
{
	.name = "QUIN_TDM_RX_0",
	.stream_name = "TDM-LPAIF_VA-RX-PRIMARY",
	.dpcm_playback = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
	.ops = &tdm_be_ops,
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	.id = IDX_QUINARY_TDM_RX_0,
	SND_SOC_DAILINK_REG(quin_tdm_rx_0),
},
{
	.name = "QUIN_TDM_TX_0",
	.stream_name = "TDM-LPAIF_VA-TX-PRIMARY",
	.dpcm_capture = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
	.ops = &tdm_be_ops,
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	.id = IDX_QUINARY_TDM_TX_0,
	SND_SOC_DAILINK_REG(quin_tdm_tx_0),
},
{
	.name = "SEN_TDM_RX_0",
	.stream_name = "TDM-LPAIF_WSA-RX-PRIMARY",
	.dpcm_playback = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	SND_SOC_DAILINK_REG(sen_tdm_rx_0),
},
{
	.name = "SEN_TDM_TX_0",
	.stream_name = "TDM-LPAIF_WSA-TX-PRIMARY",
	.dpcm_capture = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	SND_SOC_DAILINK_REG(sen_tdm_tx_0),
},
{
	.name = "SEP_TDM_RX_0",
	.stream_name = "TDM-LPAIF_AUD-RX-PRIMARY",
	.dpcm_playback = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	SND_SOC_DAILINK_REG(sep_tdm_rx_0),
},
{
	.name = "SEP_TDM_TX_0",
	.stream_name = "TDM-LPAIF_AUD-TX-PRIMARY",
	.dpcm_capture = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	SND_SOC_DAILINK_REG(sep_tdm_tx_0),
},
{
	.name = "OCT_TDM_RX_0",
	.stream_name = "TDM-LPAIF_WSA2-RX-PRIMARY",
	.dpcm_playback = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	SND_SOC_DAILINK_REG(oct_tdm_rx_0),
},
{
	.name = "OCT_TDM_TX_0",
	.stream_name = "TDM-LPAIF_WSA2-TX-PRIMARY",
	.dpcm_capture = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	SND_SOC_DAILINK_REG(oct_tdm_tx_0),
},
{
	.name = "HS_IF0_TDM_RX_0",
	.stream_name = "TDM-LPAIF_SDR-RX-PRIMARY",
	.dpcm_playback = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
        .ops = &tdm_be_ops,
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
        .id = IDX_HSIF0_TDM_RX_0,
	SND_SOC_DAILINK_REG(hs_if0_tdm_rx_0),
},
{
	.name = "HS_IF0_TDM_TX_0",
	.stream_name = "TDM-LPAIF_SDR-TX-PRIMARY",
	.dpcm_capture = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
        .ops = &tdm_be_ops,
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
        .id = IDX_HSIF0_TDM_TX_0,
	SND_SOC_DAILINK_REG(hs_if0_tdm_tx_0),
},
{
	.name = "HS_IF1_TDM_RX_0",
	.stream_name = "TDM-LPAIF_SDR-RX-SECONDARY",
	.dpcm_playback = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
        .ops = &tdm_be_ops,
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
        .id = IDX_HSIF1_TDM_RX_0,
	SND_SOC_DAILINK_REG(hs_if1_tdm_rx_0),
},
{
	.name = "HS_IF1_TDM_TX_0",
	.stream_name = "TDM-LPAIF_SDR-TX-SECONDARY",
	.dpcm_capture = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
        .ops = &tdm_be_ops,
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
        .id = IDX_HSIF1_TDM_TX_0,
	SND_SOC_DAILINK_REG(hs_if1_tdm_tx_0),
},
{
	.name = "HS_IF2_TDM_RX_0",
	.stream_name = "TDM-LPAIF_SDR-RX-TERTIARY",
	.dpcm_playback = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
        .ops = &tdm_be_ops,
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
        .id = IDX_HSIF2_TDM_RX_0,
	SND_SOC_DAILINK_REG(hs_if2_tdm_rx_0),
},
{
	.name = "HS_IF2_TDM_TX_0",
	.stream_name = "TDM-LPAIF_SDR-TX-TERTIARY",
	.dpcm_capture = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
        .ops = &tdm_be_ops,
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
        .id = IDX_HSIF2_TDM_TX_0,
	SND_SOC_DAILINK_REG(hs_if2_tdm_tx_0),
},
{
	.name = "HS_IF3_TDM_RX_0",
	.stream_name = "TDM-LPAIF_SDR-RX-QUATERNARY",
	.dpcm_playback = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
        .ops = &tdm_be_ops,
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
        .id = IDX_HSIF3_TDM_RX_0,
	SND_SOC_DAILINK_REG(hs_if3_tdm_rx_0),
},
{
	.name = "HS_IF3_TDM_TX_0",
	.stream_name = "TDM-LPAIF_SDR-TX-QUATERNARY",
	.dpcm_capture = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
        .ops = &tdm_be_ops,
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
        .id = IDX_HSIF3_TDM_TX_0,
	SND_SOC_DAILINK_REG(hs_if3_tdm_tx_0),
},
{
	.name = "HS_IF4_TDM_RX_0",
	.stream_name = "TDM-LPAIF_SDR-RX-QUINARY",
	.dpcm_playback = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
        .ops = &tdm_be_ops,
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
        .id = IDX_HSIF4_TDM_RX_0,
	SND_SOC_DAILINK_REG(hs_if4_tdm_rx_0),
},
{
	.name = "HS_IF4_TDM_TX_0",
	.stream_name = "TDM-LPAIF_SDR-TX-QUINARY",
	.dpcm_capture = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
        .ops = &tdm_be_ops,
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
        .id = IDX_HSIF4_TDM_TX_0,
	SND_SOC_DAILINK_REG(hs_if4_tdm_tx_0),
},
{
	.name = "QUAT_TDM_RX_0_DUMMY",
	.stream_name = "TDM-LPAIF-RX-QUATERNARY",
	.dpcm_playback = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	SND_SOC_DAILINK_REG(quat_tdm_rx_0_dummy),
},
{
	.name = "QUAT_TDM_TX_0_DUMMY",
	.stream_name = "TDM-LPAIF-TX-QUATERNARY",
	.dpcm_capture = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	SND_SOC_DAILINK_REG(quat_tdm_tx_0_dummy),
},
{
	.name = "QUIN_TDM_RX_0_DUMMY",
	.stream_name = "TDM-LPAIF-RX-QUINARY",
	.dpcm_playback = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	SND_SOC_DAILINK_REG(quin_tdm_rx_0_dummy),
},
{
	.name = "QUIN_TDM_TX_0_DUMMY",
	.stream_name = "TDM-LPAIF-TX-QUINARY",
	.dpcm_capture = 1,
	.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	SND_SOC_DAILINK_REG(quin_tdm_tx_0_dummy),
},
};


static int msm_populate_dai_link_component_of_node(
					struct snd_soc_card *card)
{
	int i, j, index, ret = 0;
	struct device *cdev = card->dev;
	struct snd_soc_dai_link *dai_link = card->dai_link;
	struct device_node *np;

	if (!cdev) {
		pr_err("%s: Sound card device memory NULL\n", __func__);
		return -ENODEV;
	}

	for (i = 0; i < card->num_links; i++) {
		if (dai_link[i].platforms->of_node && dai_link[i].cpus->of_node)
			continue;

	/* populate platform_of_node for snd card dai links Skip this part for dummy snd card*/
	if (0) {
		if (dai_link[i].platforms->name &&
			!dai_link[i].platforms->of_node) {
			index = of_property_match_string(cdev->of_node,
						"asoc-platform-names",
						dai_link[i].platforms->name);
			if (index < 0) {
				pr_err("%s: No match found for platform name: %s\n index: %i cdev_of_node: %s",
					__func__, dai_link[i].platforms->name, index, cdev->of_node);
				ret = index;
				goto err;
			}
			np = of_parse_phandle(cdev->of_node, "asoc-platform",
						index);
			if (!np) {
				pr_err("%s: retrieving phandle for platform %s, index %d failed\n",
					__func__, dai_link[i].platforms->name,
					index);
				ret = -ENODEV;
				goto err;
			}
			dai_link[i].platforms->of_node = np;
			dai_link[i].platforms->name = NULL;
		}
	}


		/* populate cpu_of_node for snd card dai links */
		if (dai_link[i].cpus->dai_name && !dai_link[i].cpus->of_node) {
			index = of_property_match_string(cdev->of_node,
						 "asoc-cpu-names",
						 dai_link[i].cpus->dai_name);
			pr_err("%s: retrieving cpu_of_node for %s\n",
						__func__,
						dai_link[i].cpus->dai_name);
			if (index >= 0) {
				np = of_parse_phandle(cdev->of_node, "asoc-cpu",
						index);
				if (!np) {
					pr_err("%s: retrieving phandle for cpu dai %s failed\n",
						__func__,
						dai_link[i].cpus->dai_name);
					ret = -ENODEV;
					goto err;
				}
				dai_link[i].cpus->of_node = np;
				dai_link[i].cpus->dai_name = NULL;
			}
		}

		/* populate codec_of_node for snd card dai links */
		if (dai_link[i].num_codecs > 0) {
			for (j = 0; j < dai_link[i].num_codecs; j++) {
				if (dai_link[i].codecs[j].of_node ||
						!dai_link[i].codecs[j].name)
					continue;

				index = of_property_match_string(cdev->of_node,
						"asoc-codec-names",
						dai_link[i].codecs[j].name);
				if (index < 0)
					continue;
				np = of_parse_phandle(cdev->of_node,
						      "asoc-codec",
						      index);
				if (!np) {
					pr_err("%s: retrieving phandle for codec %s failed\n",
							__func__, dai_link[i].codecs[j].name);
					ret = -ENODEV;
					goto err;
				}
				dai_link[i].codecs[j].of_node = np;
				dai_link[i].codecs[j].name = NULL;
			}
		}
	}

err:
	return ret;
}

static const struct of_device_id asoc_machine_of_match[]  = {
	{ .compatible = "qcom,sa8295-asoc-snd-adp-star",
	  .data = "adp_star_codec"},
	{ .compatible = "qcom,sa8155-asoc-snd-adp-star",
	  .data = "adp_star_codec"},
	{ .compatible = "qcom,sa8255-asoc-snd-adp-star",
	  .data = "adp_star_codec"},
	{},
};

static const struct of_device_id audio_pinctrl_dummy_match[] = {
	{ .compatible = "qcom,msm-pcm-pinctrl" },
	{ },
};

static struct snd_soc_dai_link msm_auto_dai_links[
			 ARRAY_SIZE(msm_common_dai_links)];

static struct snd_soc_card *populate_snd_card_dailinks(struct device *dev)
{
	struct snd_soc_card *card = NULL;
	struct snd_soc_dai_link *dailink;
	int total_links;
	const struct of_device_id *match;

	match = of_match_node(asoc_machine_of_match, dev->of_node);
	if (!match) {
		dev_err(dev, "%s: No DT match found for sound card\n",
			__func__);
		return NULL;
	}

	if (!strcmp(match->compatible, "qcom,sa8155-asoc-snd-adp-star")) {
		card = &sa8155_snd_soc_card_auto_msm;
	} else if (!strcmp(match->compatible, "qcom,sa8295-asoc-snd-adp-star")) {
		card = &sa8295_snd_soc_card_auto_msm;
	} else if (!strcmp(match->compatible, "qcom,sa8255-asoc-snd-adp-star")) {
		card = &sa8255_snd_soc_card_auto_msm;
	}
	total_links = ARRAY_SIZE(msm_common_dai_links);
	memcpy(msm_auto_dai_links,
		msm_common_dai_links,
		sizeof(msm_common_dai_links));
	dailink = msm_auto_dai_links;

	if (card) {
		card->dai_link = dailink;
		card->num_links = total_links;
	}

	return card;
}

struct msm_common_pdata *msm_common_get_pdata(struct snd_soc_card *card)
{
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	if (!pdata)
		return NULL;

	return pdata->common_pdata;
}

void msm_common_set_pdata(struct snd_soc_card *card,
			  struct msm_common_pdata *common_pdata)
{
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	if (!pdata)
		return;

	pdata->common_pdata = common_pdata;
}

/*****************************************************************************
 * pinctrl
 *****************************************************************************/
static void msm_release_pinctrl(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_pinctrl_info *pinctrl_info = NULL;
	int i;

	for (i = TDM_PRI; i < TDM_INTERFACE_MAX; i++) {
		pinctrl_info = &pdata->pinctrl_info[i];
		if (pinctrl_info == NULL)
			continue;
		if (pinctrl_info->pinctrl) {
			devm_pinctrl_put(pinctrl_info->pinctrl);
			pinctrl_info->pinctrl = NULL;
		}
	}
}

static int msm_get_pinctrl(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_pinctrl_info *pinctrl_info = NULL;
	struct pinctrl *pinctrl = NULL;
	int i, j;
	struct device_node *np = NULL;
	struct platform_device *pdev_np = NULL;
	int ret = 0;

	for (i = TDM_PRI; i < TDM_INTERFACE_MAX; i++) {
		np = of_parse_phandle(pdev->dev.of_node,
					tdm_gpio_phandle[i], 0);
		if (!np) {
			pr_debug("%s: device node %s is null\n",
					__func__, tdm_gpio_phandle[i]);
			continue;
		}

		pdev_np = of_find_device_by_node(np);
		if (!pdev_np) {
			pr_err("%s: platform device not found\n", __func__);
			continue;
		}

		pinctrl_info = &pdata->pinctrl_info[i];
		if (pinctrl_info == NULL) {
			pr_err("%s: pinctrl info is null\n", __func__);
			continue;
		}

		pinctrl = devm_pinctrl_get(&pdev_np->dev);
		if (IS_ERR_OR_NULL(pinctrl)) {
			pr_err("%s: fail to get pinctrl handle\n", __func__);
			goto err;
		}
		pinctrl_info->pinctrl = pinctrl;

		/* get all the states handles from Device Tree */
		pinctrl_info->sleep = pinctrl_lookup_state(pinctrl,
							"default");
		if (IS_ERR(pinctrl_info->sleep)) {
			pr_err("%s: could not get sleep pin state\n", __func__);
			goto err;
		}
		pinctrl_info->active = pinctrl_lookup_state(pinctrl,
							"active");
		if (IS_ERR(pinctrl_info->active)) {
			pr_err("%s: could not get active pin state\n",
				__func__);
			goto err;
		}

		/* Reset the TLMM pins to a sleep state */
		ret = pinctrl_select_state(pinctrl_info->pinctrl,
						pinctrl_info->sleep);
		if (ret != 0) {
			pr_err("%s: set pin state to sleep failed with %d\n",
				__func__, ret);
			ret = -EIO;
			goto err;
		}
		pinctrl_info->curr_state = STATE_SLEEP;
	}
	return 0;

err:
	for (j = i; j >= 0; j--) {
		pinctrl_info = &pdata->pinctrl_info[j];
		if (pinctrl_info == NULL)
			continue;
		if (pinctrl_info->pinctrl) {
			devm_pinctrl_put(pinctrl_info->pinctrl);
			pinctrl_info->pinctrl = NULL;
		}
	}
	return -EINVAL;
}

static int msm_pinctrl_mclk_enable(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_pinctrl_info *pinctrl_info = NULL;
	struct pinctrl *pinctrl = NULL;
	int pinctrl_num;
	int i, j;
	struct device_node *np = NULL;
	struct platform_device *pdev_np = NULL;
	int ret = 0;

	pinctrl_num = MCLK_MAX;
	for (i = 0; i < pinctrl_num; i++) {

		np = of_parse_phandle(pdev->dev.of_node, mclk_gpio_phandle[i], 0);
		if (!np) {
			pr_err("%s: device node %s is null\n", __func__, mclk_gpio_phandle[i]);
			continue;
		}
		pdev_np = of_find_device_by_node(np);
		if (!pdev_np) {
			pr_err("%s: platform device not found\n", __func__);
			continue;
		}

		pinctrl_info = &pdata->mclk_pinctrl_info[i];
		if (pinctrl_info == NULL) {
			pr_err("%s: pinctrl info is null\n", __func__);
			continue;
		}

		pinctrl = devm_pinctrl_get(&pdev_np->dev);
		if (IS_ERR_OR_NULL(pinctrl)) {
			pr_err("%s: fail to get pinctrl handle\n", __func__);
			goto err;
		}
		pinctrl_info->pinctrl = pinctrl;
		/* get all the states handles from Device Tree */
		pinctrl_info->sleep = pinctrl_lookup_state(pinctrl,
			"sleep");
		if (IS_ERR(pinctrl_info->sleep)) {
			pr_err("%s: could not get sleep pin state\n", __func__);
			goto err;
		}
		pinctrl_info->active = pinctrl_lookup_state(pinctrl,
			"default");
		if (IS_ERR(pinctrl_info->active)) {
			pr_err("%s: could not get active pin state\n", __func__);
			goto err;
		}

		/* Reset the mclk pins to a active state */
		ret = audio_prm_set_lpass_clk_cfg(&internal_mclk[i], 1);
		if (ret < 0) {
			pr_err("%s: audio_prm_set_lpass_clk_cfg failed to enable clock, err:%d\n",
				__func__, ret);
		}

		ret = pinctrl_select_state(pinctrl_info->pinctrl, pinctrl_info->active);
		if (ret != 0) {
			pr_err("%s: set pin state to active failed with %d\n",
				__func__, ret);
			ret = -EIO;
			goto err;
		}
		pinctrl_info->curr_state = STATE_ACTIVE;
	}
	return 0;

err:
	for (j = i; j >= 0; j--) {
		pinctrl_info = &pdata->mclk_pinctrl_info[i];
		if (pinctrl_info == NULL)
			continue;
		if (pinctrl_info->pinctrl) {
			devm_pinctrl_put(pinctrl_info->pinctrl);
			pinctrl_info->pinctrl = NULL;
		}
	}
	return -EINVAL;
}


static int msm_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	struct msm_asoc_mach_data *pdata;
	int ret;
	const struct of_device_id *match;

	dev_err(&pdev->dev, "%s: audio_reach\n", __func__);

	match = of_match_node(asoc_machine_of_match, pdev->dev.of_node);
	if (!match) {
		dev_err(&pdev->dev, "%s: No DT match found for sound card\n", __func__);
		return -EINVAL;
	}

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No platform supplied from device tree\n");
		return -EINVAL;
	}

	dev_err(&pdev->dev, "%s", __func__);

	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct msm_asoc_mach_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	card = populate_snd_card_dailinks(&pdev->dev);
	if (!card) {
		dev_err(&pdev->dev, "%s: Card uninitialized\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, pdata);

	ret = snd_soc_of_parse_card_name(card, "qcom,model");
	if (ret) {
		dev_err(&pdev->dev, "parse card name failed, err:%d\n",
			ret);
		pr_err("parse card name failed, err:%d\n", __func__, ret);
		goto err;
	}

	ret = msm_populate_dai_link_component_of_node(card);
	if (ret) {
		ret = -EPROBE_DEFER;
		goto err;
	}

	ret = devm_snd_soc_register_card(&pdev->dev, card);

	if (ret == -EPROBE_DEFER) {
		if (codec_reg_done)
			ret = -EINVAL;
		goto err;
	} else if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		pr_err("snd_soc_register_card failed (%d)\n",
			ret);
		goto err;
	}

	msm_common_snd_init(pdev, card);

	/* Parse pinctrl info from devicetree */
	ret = msm_get_pinctrl(pdev);
	if (!ret) {
		pr_err("%s: pinctrl parsing successful\n", __func__);
	} else {
		dev_dbg(&pdev->dev,
			"%s: pinctrl parsing failed with %d\n",
			__func__, ret);
		ret = 0;
	}

	/* enable mclk pinctrl info from devicetree */
    match = of_match_node(asoc_machine_of_match, pdev->dev.of_node);
	if (!match) {
		dev_err(&pdev->dev, "%s: No DT match found for sound card\n", __func__);
		return -EINVAL;
	}
	if (strstr(match->compatible, "sa8295") || strstr(match->compatible, "sa8255")) {
		/* enable mclk pinctrl info from devicetree */
		ret = msm_pinctrl_mclk_enable(pdev);
		if (!ret) {
			pr_debug("%s: pinctrl mclk parsing successful\n", __func__);
		} else {
			dev_err(&pdev->dev,
				"%s: pinctrl mclk parsing failed with %d\n",
				__func__, ret);
			ret = 0;
		}
	}

	dev_info(&pdev->dev, "Sound card %s registered\n", card->name);
	pr_err("Sound card %s registered\n", card->name);
	spdev = pdev;

	snd_card_set_card_status(SND_CARD_STATUS_ONLINE);
	ret = audio_notifier_register("auto_spf", AUDIO_NOTIFIER_ADSP_DOMAIN,
				      &service_nb);

	return 0;
err:
	msm_release_pinctrl(pdev);
	devm_kfree(&pdev->dev, pdata);
	return ret;
}

static int msm_asoc_machine_remove(struct platform_device *pdev)
{
	msm_release_pinctrl(pdev);
	return 0;
}

static int audio_pinctrl_dummy_probe(struct platform_device *pdev)
{
	pr_err("%s\n", __func__);
	return 0;
}

static int audio_pinctrl_dummy_remove(struct platform_device *pdev)
{
	pr_err("%s\n", __func__);
	return 0;
}

static struct platform_driver audio_pinctrl_dummy_driver = {
	.driver = {
		.name = DRV_PINCTRL_NAME,
		.owner = THIS_MODULE,
		.of_match_table = audio_pinctrl_dummy_match,
		.suppress_bind_attrs = true,
	},
	.probe = audio_pinctrl_dummy_probe,
	.remove = audio_pinctrl_dummy_remove,
};

static struct platform_driver asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = asoc_machine_of_match,
		.suppress_bind_attrs = true,
	},
	.probe = msm_asoc_machine_probe,
	.remove = msm_asoc_machine_remove,
};

int __init auto_spf_init(void)
{
	pr_err("%s\n", __func__);
	snd_card_sysfs_init();
	platform_driver_register(&audio_pinctrl_dummy_driver);
	return platform_driver_register(&asoc_machine_driver);
}

void auto_spf_exit(void)
{
	pr_err("%s\n", __func__);
	platform_driver_unregister(&audio_pinctrl_dummy_driver);
	platform_driver_unregister(&asoc_machine_driver);
}

module_init(auto_spf_init);
module_exit(auto_spf_exit);

MODULE_DESCRIPTION("ALSA SoC Machine Driver for SPF");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_ALIAS("platform:" DRV_PINCTRL_NAME);
MODULE_DEVICE_TABLE(of, asoc_machine_of_match);
MODULE_DEVICE_TABLE(of, audio_pinctrl_dummy_match);
