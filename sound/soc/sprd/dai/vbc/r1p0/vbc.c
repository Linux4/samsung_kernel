/*
 * sound/soc/sprd/dai/vbc/r1p0/vbc.c
 *
 * SPRD SoC VBC -- SpreadTrum SOC for VBC DAI function.
 *
 * Copyright (C) 2013 SpreadTrum Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include "sprd-asoc-debug.h"
#define pr_fmt(fmt) pr_sprd_fmt(" VBC ") fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>

#include "sprd-asoc-common.h"
#include "sprd-pcm.h"
#include "vbc.h"
#include "dfm.h"

typedef int (*vbc_dma_set) (int enable);

struct sprd_vbc_buffer_info {
	int reg;
	int shift;
	int mask;
	int max;
};

struct sprd_vbc_priv {
	vbc_dma_set dma_set[2];
	int (*arch_enable) (int chan);
	int (*arch_disable) (int chan);
	int used_chan_count;
	struct sprd_vbc_buffer_info buf_info;
	int rate;
};

struct sprd_dfm_priv dfm = { 0, 0 };

EXPORT_SYMBOL_GPL(dfm);

static struct sprd_pcm_dma_params vbc_pcm_stereo_out = {
	.name = "VBC PCM Stereo out",
	.irq_type = BLK_DONE,
	.desc = {
		 .datawidth = SHORT_WIDTH,
		 .fragmens_len = VBC_FIFO_FRAME_NUM * 2,
		 .src_step = 2,
		 .des_step = 0,
		 },
	.dev_paddr = {PHYS_VBDA0, PHYS_VBDA1},
};

static struct sprd_pcm_dma_params vbc_pcm_stereo_in = {
	.name = "VBC PCM Stereo in",
	.irq_type = BLK_DONE,
	.desc = {
		 .datawidth = SHORT_WIDTH,
		 .fragmens_len = VBC_FIFO_FRAME_NUM * 2,
		 .src_step = 0,
		 .des_step = 2,
		 },
	.dev_paddr = {PHYS_VBAD0, PHYS_VBAD1},
};

static struct sprd_vbc_priv vbc[VBC_IDX_MAX];

static int vbc_iis_high_for_da1(int enable)
{
	vbc_reg_update(VBIISSEL, (enable ? 1 : 0) << VBIISSEL_DA_LRCK,
		       1 << VBIISSEL_DA_LRCK);
	return 0;
}

#if 0
static int vbc_iis_high_for_ad1(int enable)
{
	vbc_reg_update(VBIISSEL, (enable ? 1 : 0) << VBIISSEL_AD_LRCK,
		       1 << VBIISSEL_AD_LRCK);
	return 0;
}
#endif

static int vbc_set_buffer_size(int vbc_idx, int buffer_size)
{
	int val = vbc_reg_read(vbc[vbc_idx].buf_info.reg);
	WARN_ON(buffer_size > vbc[vbc_idx].buf_info.max);
	if ((buffer_size > 0)
	    && (buffer_size <= vbc[vbc_idx].buf_info.max)) {
		val &= ~(vbc[vbc_idx].buf_info.mask);
		val |= (((buffer_size - 1) << vbc[vbc_idx].buf_info.shift)
			& vbc[vbc_idx].buf_info.mask);
		vbc_reg_update(vbc[vbc_idx].buf_info.reg, val,
			       vbc[vbc_idx].buf_info.mask);
	} else {
		pr_err("ERR:buffer_size error! %d\n", buffer_size);
		return -EINVAL;
	}
	return 0;
}

static int fm_set_vbc_buffer_size(void)
{
	int i;
	for (i = SPRD_VBC_PLAYBACK_COUNT; i < VBC_IDX_MAX; i++) {
		vbc_set_buffer_size(i, VBC_FIFO_FRAME_NUM);
	}
	return 0;
}

static inline int vbc_sw_write_buffer(int enable)
{
	/* Software access ping-pong buffer enable when VBENABE bit low */
	vbc_reg_update(VBDABUFFDTA, ((enable ? 1 : 0) << RAMSW_EN),
		       (1 << RAMSW_EN));
	return 0;
}

static inline int vbc_ad0_dma_set(int enable)
{
	vbc_reg_update(VBDABUFFDTA, ((enable ? 1 : 0) << VBAD0DMA_EN),
		       (1 << VBAD0DMA_EN));
	return 0;
}

static inline int vbc_ad1_dma_set(int enable)
{
	vbc_reg_update(VBDABUFFDTA, ((enable ? 1 : 0) << VBAD1DMA_EN),
		       (1 << VBAD1DMA_EN));
	return 0;
}

static inline int vbc_da0_dma_set(int enable)
{
	vbc_reg_update(VBDABUFFDTA, ((enable ? 1 : 0) << VBDA0DMA_EN),
		       (1 << VBDA0DMA_EN));
	return 0;
}

static inline int vbc_da1_dma_set(int enable)
{
	vbc_reg_update(VBDABUFFDTA, ((enable ? 1 : 0) << VBDA1DMA_EN),
		       (1 << VBDA1DMA_EN));
	return 0;
}

static void vbc_da_buffer_clear(int id)
{
	int i;
	vbc_reg_update(VBDABUFFDTA, ((id ? 1 : 0) << RAMSW_NUMB),
		       (1 << RAMSW_NUMB));
	for (i = 0; i < VBC_FIFO_FRAME_NUM; i++) {
		vbc_reg_raw_write(VBDA0, 0);
		vbc_reg_raw_write(VBDA1, 0);
	}
}

static void vbc_da_buffer_clear_all(int vbc_idx)
{
	int i;

	for (i = 0; i < 2; i++) {
		vbc[vbc_idx].arch_enable(i);
	}

	vbc_sw_write_buffer(true);
	vbc_set_buffer_size(vbc_idx, VBC_FIFO_FRAME_NUM);
	vbc_da_buffer_clear(1);	/* clear data buffer 1 */
	vbc_da_buffer_clear(0);	/* clear data buffer 0 */
	vbc_sw_write_buffer(false);

	for (i = 0; i < 2; i++) {
		vbc[vbc_idx].arch_disable(i);
	}
}

static int vbc_da_arch_enable(int chan)
{
	return vbc_chan_enable(1, VBC_PLAYBACK, chan);
}

static int vbc_da_arch_disable(int chan)
{
	return vbc_chan_enable(0, VBC_PLAYBACK, chan);
}

static int vbc_ad_arch_enable(int chan)
{
	return vbc_chan_enable(1, VBC_CAPTRUE, chan);
}

static int vbc_ad_arch_disable(int chan)
{
	return vbc_chan_enable(0, VBC_CAPTRUE, chan);
}

static struct sprd_vbc_priv vbc[VBC_IDX_MAX] = {
	/* All Playback */
	{			/*PlayBack */
	 .dma_set = {vbc_da0_dma_set, vbc_da1_dma_set},
	 .arch_enable = vbc_da_arch_enable,
	 .arch_disable = vbc_da_arch_disable,
	 .buf_info = {VBBUFFSIZE, VBDABUFFERSIZE_SHIFT, VBDABUFFERSIZE_MASK,
		      VBC_FIFO_FRAME_NUM},
	 },

	/* All Capture */
	{			/*Capture for ad01 */
	 .dma_set = {vbc_ad0_dma_set, vbc_ad1_dma_set},
	 .arch_enable = vbc_ad_arch_enable,
	 .arch_disable = vbc_ad_arch_disable,
	 .buf_info = {VBBUFFSIZE, VBADBUFFERSIZE_SHIFT, VBADBUFFERSIZE_MASK,
		      VBC_FIFO_FRAME_NUM},
	 },
};

/* NOTE:
   this index need use for the [struct sprd_vbc_priv] vbc[2] index
   default MUST return 0.
 */
static inline int vbc_str_2_index(int stream, int id)
{
	if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		return id + SPRD_VBC_PLAYBACK_COUNT;
	}
	return id;
}

static int vbc_startup(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	int vbc_idx;
	int ret;

	vbc_idx = vbc_str_2_index(substream->stream, dai->id);
	sp_asoc_pr_dbg("%s VBC(%s)\n", __func__, vbc_get_name(vbc_idx));
	sp_asoc_pr_dbg("dfm.hw_rate:%d, dfm.sample_rate:%d", dfm.hw_rate,
		       dfm.sample_rate);

	if (dfm.sample_rate != 0) {
		if ((vbc_idx == VBC_PLAYBACK) || (vbc_idx == VBC_CAPTRUE)) {
			ret = snd_pcm_hw_constraint_minmax(substream->runtime,
							   SNDRV_PCM_HW_PARAM_RATE,
							   dfm.sample_rate,
							   dfm.sample_rate);
			if (ret < 0) {
				pr_err("constraint error");
				return ret;
			}
		}
	}

	vbc_power(1);

	vbc_set_buffer_size(vbc_idx, VBC_FIFO_FRAME_NUM);
	vbc_codec_startup(vbc_idx, dai);

	WARN_ON(!vbc[vbc_idx].arch_enable);
	WARN_ON(!vbc[vbc_idx].arch_disable);
	WARN_ON(!vbc[vbc_idx].dma_set[0]);
	WARN_ON(!vbc[vbc_idx].dma_set[1]);

	return 0;
}

static void vbc_shutdown(struct snd_pcm_substream *substream,
			 struct snd_soc_dai *dai)
{
	int vbc_idx;

	vbc_idx = vbc_str_2_index(substream->stream, dai->id);
	sp_asoc_pr_dbg("%s VBC(%s)\n", __func__, vbc_get_name(vbc_idx));

	/* vbc da close MUST clear da buffer */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		vbc_da_buffer_clear_all(vbc_idx);
	}

	vbc_power(0);
}

static int vbc_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *params,
			 struct snd_soc_dai *dai)
{
	int vbc_idx;
	struct sprd_pcm_dma_params *dma_data[VBC_IDX_MAX] = {
		&vbc_pcm_stereo_out,
		&vbc_pcm_stereo_in,
	};

	vbc_idx = vbc_str_2_index(substream->stream, dai->id);
	sp_asoc_pr_dbg("%s VBC(%s)\n", __func__, vbc_get_name(vbc_idx));

	snd_soc_dai_set_dma_data(dai, substream, dma_data[vbc_idx]);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	default:
		pr_err("ERR:VBC Only Supports Format S16_LE\n");
		break;
	}

	vbc[vbc_idx].used_chan_count = params_channels(params);
	if (vbc[vbc_idx].used_chan_count > 2) {
		pr_err("ERR:VBC Can NOT Supports Grate 2 Channels\n");
	}

	vbc_iis_high_for_da1(1);
#ifdef CONFIG_SND_SOC_SPRD_VBC_LR_INVERT
	if (vbc[vbc_idx].used_chan_count == 2) {
		vbc_iis_high_for_da1(0);
	}
#endif
	vbc[vbc_idx].rate = params_rate(params);
	return 0;
}

static int vbc_hw_free(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	int vbc_idx;

	vbc_idx = vbc_str_2_index(substream->stream, dai->id);
	sp_asoc_pr_dbg("%s VBC(%s)\n", __func__, vbc_get_name(vbc_idx));

	vbc[vbc_idx].rate = 0;

	return 0;
}

static int vbc_trigger(struct snd_pcm_substream *substream, int cmd,
		       struct snd_soc_dai *dai)
{
	int vbc_idx;
	int ret = 0;
	int i;

#if 0
	sp_asoc_pr_dbg("%s\n", __func__);
#endif

	vbc_idx = vbc_str_2_index(substream->stream, dai->id);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		for (i = 0; i < vbc[vbc_idx].used_chan_count; i++) {
			vbc[vbc_idx].arch_enable(i);
			vbc[vbc_idx].dma_set[i] (1);
		}
		vbc_enable(1);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		for (i = 0; i < vbc[vbc_idx].used_chan_count; i++) {
			vbc[vbc_idx].arch_disable(i);
			vbc[vbc_idx].dma_set[i] (0);
		}
		vbc_enable(0);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static struct snd_soc_dai_ops vbc_dai_ops = {
	.startup = vbc_startup,
	.shutdown = vbc_shutdown,
	.hw_params = vbc_hw_params,
	.trigger = vbc_trigger,
	.hw_free = vbc_hw_free,
};

static int dfm_startup(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	static const unsigned int dfm_all_rates[] = { 32000, 44100, 48000 };
	static const struct snd_pcm_hw_constraint_list dfm_rates_constraint = {
		.count = ARRAY_SIZE(dfm_all_rates),
		.list = dfm_all_rates,
	};
	int ret;
	int vbc_idx;
	struct snd_soc_card *card = dai->card;
	int i;

	sp_asoc_pr_dbg("%s\n, vbc[vbc_idx].rate:da:%d, ad:%d, ad23:%d",
		       __func__, vbc[0].rate, vbc[1].rate, vbc[2].rate);

	ret = snd_pcm_hw_constraint_list(substream->runtime, 0,
					 SNDRV_PCM_HW_PARAM_RATE,
					 &dfm_rates_constraint);
	if (ret < 0)
		return ret;

	for (vbc_idx = VBC_PLAYBACK; vbc_idx < VBC_CAPTRUE1; vbc_idx++) {
		if (vbc[vbc_idx].rate != 0) {
			ret = snd_pcm_hw_constraint_minmax(substream->runtime,
							   SNDRV_PCM_HW_PARAM_RATE,
							   vbc[vbc_idx].rate,
							   vbc[vbc_idx].rate);
			if (ret < 0) {
				pr_err("constraint error");
				return ret;
			}
		}
	}

	kfree(snd_soc_dai_get_dma_data(dai, substream));
	snd_soc_dai_set_dma_data(dai, substream, NULL);

	snd_soc_dapm_enable_pin(&dai->codec->dapm, "DFM");
	snd_soc_dapm_sync(&dai->codec->dapm);
	for (i = 0; i < card->num_rtd; i++) {
		card->rtd[i].dai_link->ignore_suspend = 1;
	}
	snd_soc_dapm_ignore_suspend(&dai->codec->dapm, "DFM");

	return 0;
}

static void dfm_shutdown(struct snd_pcm_substream *substream,
			 struct snd_soc_dai *dai)
{
	struct snd_soc_card *card = dai->card;
	int i;
	sp_asoc_pr_dbg("%s\n", __func__);

	snd_soc_dapm_disable_pin(&dai->codec->dapm, "DFM");
	snd_soc_dapm_sync(&dai->codec->dapm);
	for (i = 0; i < card->num_rtd; i++) {
		card->rtd[i].dai_link->ignore_suspend = 0;
	}
}

static int dfm_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *params,
			 struct snd_soc_dai *dai)
{
	sp_asoc_pr_dbg("%s \n", __func__);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	default:
		pr_err("ERR:VBC Only Supports Format S16_LE\n");
		break;
	}

	return 0;
}

static int dfm_hw_free(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	sp_asoc_pr_dbg("%s \n", __func__);
	dfm.sample_rate = 0;
	dfm.hw_rate = 0;
	return 0;
}

static struct snd_soc_dai_ops dfm_dai_ops = {
	.startup = dfm_startup,
	.shutdown = dfm_shutdown,
	.hw_params = dfm_hw_params,
	.hw_free = dfm_hw_free,
};

static struct snd_soc_dai_driver vbc_dai[] = {
	{
	 .name = "vbc-r1p0",
	 .id = 0,
	 .playback = {
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_CONTINUOUS,
		      .rate_max = 96000,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .capture = {
		     .channels_min = 1,
		     .channels_max = 2,	/* AD01 */
		     .rates = SNDRV_PCM_RATE_CONTINUOUS,
		     .rate_max = 96000,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &vbc_dai_ops,
	 },
	{
	 .name = "vbc-dfm",
	 .id = DFM_MAGIC_ID,
	 .playback = {
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_CONTINUOUS,
		      .rate_max = 48000,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .ops = &dfm_dai_ops,
	 }
};

static const struct snd_soc_component_driver sprd_vbc_component = {
	.name = "vbc",
};

static int sprd_vbc_codec_probe(struct platform_device *pdev);
static int vbc_drv_probe(struct platform_device *pdev)
{
	int i;
	int ret;
	struct clk *vbc_clk;

	sp_asoc_pr_dbg("%s\n", __func__);

	arch_audio_vbc_switch(AUDIO_TO_ARM_CTRL);
	ret = arch_audio_vbc_switch(AUDIO_NO_CHANGE);
	if (ret != AUDIO_TO_ARM_CTRL) {
		pr_err("Failed to Switch VBC to AP\n");
		return -1;
	}

	for (i = 0; i < 2; i++) {
		vbc_pcm_stereo_out.channels[i] = arch_audio_vbc_da_dma_info(i);
		vbc_pcm_stereo_in.channels[i] = arch_audio_vbc_ad_dma_info(i);
	}

	/* 1. probe CODEC */
	ret = sprd_vbc_codec_probe(pdev);

	if (ret < 0) {
		goto probe_err;
	}

	/* 2. probe DAIS */
	ret =
	    snd_soc_register_component(&pdev->dev, &sprd_vbc_component, vbc_dai,
				       ARRAY_SIZE(vbc_dai));

	if (ret < 0) {
		pr_err("ERR:Register VBC to DAIS Failed!\n");
		goto probe_err;
	}

	vbc_clk = clk_get(&pdev->dev, "clk_vbc");
	if (IS_ERR(vbc_clk)) {
		pr_err("Cannot request clk_vbc\n");
	} else {
		vbc_clk_set(vbc_clk);
	}

probe_err:
	sp_asoc_pr_dbg("return %i\n", ret);

	return ret;
}

static int sprd_vbc_codec_remove(struct platform_device *pdev);
static int vbc_drv_remove(struct platform_device *pdev)
{
	struct clk *vbc_clk = vbc_clk_get();

	if (vbc_clk) {
		clk_put(vbc_clk);
		vbc_clk_set(0);
	}

	snd_soc_unregister_component(&pdev->dev);

	sprd_vbc_codec_remove(pdev);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id vbc_of_match[] = {
	{.compatible = "sprd,vbc-r1p0",},
	{},
};

MODULE_DEVICE_TABLE(of, vbc_of_match);
#endif

static struct platform_driver vbc_driver = {
	.driver = {
		   .name = "vbc-r1p0",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(vbc_of_match),
		   },

	.probe = vbc_drv_probe,
	.remove = vbc_drv_remove,
};

module_platform_driver(vbc_driver);

/* include the other module of VBC */
#include "vbc-comm.c"
#include "vbc-codec.c"

MODULE_DESCRIPTION("SPRD ASoC VBC CUP-DAI driver");
MODULE_AUTHOR("Zhenfang Wang <zhenfang.wang@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("cpu-dai:vbc-r1p0");
