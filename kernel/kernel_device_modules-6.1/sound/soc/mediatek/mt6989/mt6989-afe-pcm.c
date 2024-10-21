// SPDX-License-Identifier: GPL-2.0
/*
 *  Mediatek ALSA SoC AFE platform driver for 6989
 *
 *  Copyright (c) 2023 MediaTek Inc.
 *  Author: Tina Tsai <tina.tsai@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <sound/soc.h>
#include <linux/regmap.h>
#include <linux/of_device.h>
#include <linux/arm-smccc.h> /* for Kernel Native SMC API */
#include <linux/soc/mediatek/mtk_sip_svc.h> /* for SMC ID table */

#include "../common/mtk-afe-debug.h"
#include "../common/mtk-afe-platform-driver.h"
#include "../common/mtk-afe-fe-dai.h"
#include "../common/mtk-sp-pcm-ops.h"
#include "../common/mtk-sram-manager.h"
#include "../common/mtk-mmap-ion.h"
#include "../common/mtk-usb-offload-ops.h"

#include "mt6989-afe-cm.h"
#include "mt6989-afe-common.h"
#include "mt6989-afe-clk.h"
#include "mt6989-afe-gpio.h"
#include "mt6989-interconnection.h"
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
#include "../audio_dsp/mtk-dsp-common.h"
#endif
#if IS_ENABLED(CONFIG_MTK_ULTRASND_PROXIMITY) && !defined(SKIP_SB_ULTRA)
#include "../ultrasound/ultra_scp/mtk-scp-ultra-common.h"
#endif
/* FORCE_FPGA_ENABLE_IRQ use irq in fpga */
#define FORCE_FPGA_ENABLE_IRQ


#define AFE_SYS_DEBUG_SIZE (1024 * 64) // 64K
#define MAX_DEBUG_WRITE_INPUT 256

static ssize_t mt6989_debug_read_reg(char *buffer, int size, struct mtk_base_afe *afe);

static const struct snd_pcm_hardware mt6989_afe_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
		 SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_MMAP_VALID),
	.formats = (SNDRV_PCM_FMTBIT_S16_LE |
		    SNDRV_PCM_FMTBIT_S24_LE |
		    SNDRV_PCM_FMTBIT_S32_LE),
	.period_bytes_min = 96,
	.period_bytes_max = 4 * 48 * 1024,
	.periods_min = 2,
	.periods_max = 256,
	.buffer_bytes_max = 256 * 1024,
	.fifo_size = 0,
};

static int mt6989_fe_startup(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int memif_num = cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	const struct snd_pcm_hardware *mtk_afe_hardware = afe->mtk_afe_hardware;
	int ret;

	memif->substream = substream;

	snd_pcm_hw_constraint_step(substream->runtime, 0,
				   SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 16);

	snd_soc_set_runtime_hwparams(substream, mtk_afe_hardware);

	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		dev_info(afe->dev, "snd_pcm_hw_constraint_integer failed\n");

	/* dynamic allocate irq to memif */
	if (memif->irq_usage < 0) {
		int irq_id = mtk_dynamic_irq_acquire(afe);

		if (irq_id != afe->irqs_size) {
			/* link */
			memif->irq_usage = irq_id;
		} else {
			dev_info(afe->dev, "%s() error: no more asys irq\n",
				__func__);
			ret = -EBUSY;
		}
	}

	return ret;
}

void mt6989_fe_shutdown(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int memif_num = cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;

	if (memif->err_close_order) {
		dump_stack();
		memif->err_close_order = false;
	}

	memif->substream = NULL;
	afe_priv->irq_cnt[memif_num] = 0;
	afe_priv->xrun_assert[memif_num] = 0;

	if (!memif->const_irq) {
		mtk_dynamic_irq_release(afe, irq_id);
		memif->irq_usage = -1;
		memif->substream = NULL;
	}
}

int mt6989_fe_trigger(struct snd_pcm_substream *substream, int cmd,
		      struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *const runtime = substream->runtime;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	int irq_id = memif->irq_usage;
	struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;
	unsigned int counter = runtime->period_size;
	unsigned int rate = runtime->rate;
	int fs;
	int ret = 0;

	if (!in_interrupt())
		dev_info(afe->dev,
			 "%s(), %s cmd %d, irq_id %d, is_afe_need_triggered %d, no_period_wakeup %d\n",
			 __func__, memif->data->name, cmd, irq_id,
			 is_afe_need_triggered(memif),
			 runtime->no_period_wakeup);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		/* add delay for bt memif to avoid dl noise */
		if (id == MT6989_MEMIF_DL23)
			mtk_memif_set_pbuf_size(afe, id, MT6989_MEMIF_PBUF_SIZE_32_BYTES);

		if (is_afe_need_triggered(memif)) {
			ret = mtk_memif_set_enable(afe, id);
			if (ret) {
				dev_info(afe->dev,
					"%s(), error, id %d, memif enable, ret %d\n",
					__func__, id, ret);
				return ret;
			}
			if (!strcmp(memif->data->name, "VUL8")) {
				if (mt6989_is_need_enable_cm(afe, CM0))
					mt6989_enable_cm(afe, CM0, 1);
			}
			if (!strcmp(memif->data->name, "VUL9")) {
				if (mt6989_is_need_enable_cm(afe, CM1))
					mt6989_enable_cm(afe, CM1, 1);
			}
		}

		/*
		 * for small latency record
		 * ul memif need read some data before irq enable
		 */
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			if ((runtime->period_size * 1000) / rate <= 10)
				mt6989_aud_delay(300);
		}

		/* set irq counter */
		if (afe_priv->irq_cnt[id] > 0)
			counter = afe_priv->irq_cnt[id];

		mtk_regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				   irq_data->irq_cnt_maskbit,
				   counter, irq_data->irq_cnt_shift);

		/* set irq fs */
		fs = afe->irq_fs(substream, runtime->rate);
		if (fs < 0)
			return -EINVAL;

		mtk_regmap_update_bits(afe->regmap, irq_data->irq_fs_reg,
				   irq_data->irq_fs_maskbit,
				   fs, irq_data->irq_fs_shift);

		if (!runtime->no_period_wakeup)
			mtk_irq_set_enable(afe, irq_data, id);

		return 0;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		if (afe_priv->xrun_assert[id] > 0) {
			if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
				int avail = snd_pcm_capture_avail(runtime);

				if (avail >= runtime->buffer_size) {
					dev_info(afe->dev, "%s(), id %d, xrun assert\n",
						 __func__, id);
					AUDIO_AEE("xrun assert");
				}
			}
		}

		if (is_afe_need_triggered(memif)) {
			ret = mtk_memif_set_disable(afe, id);
			if (ret) {
				dev_info(afe->dev,
					"%s(), error, id %d, memif enable, ret %d\n",
					__func__, id, ret);
			}
			if (!strcmp(memif->data->name, "VUL8"))
				mt6989_enable_cm(afe, CM0, 0);
			if (!strcmp(memif->data->name, "VUL9"))
				mt6989_enable_cm(afe, CM1, 0);
		}

		if (!runtime->no_period_wakeup) {
			/* disable interrupt */
			mtk_irq_set_disable(afe, irq_data, id);

			/* clear pending IRQ */
			regmap_write(afe->regmap, irq_data->irq_clr_reg,
				     1 << irq_data->irq_clr_shift);
		}

		return ret;
	default:
		return -EINVAL;
	}
}

static int mt6989_memif_fs(struct snd_pcm_substream *substream,
			   unsigned int rate)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = NULL;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	unsigned int rate_reg = 0;
	int cm = 0;

	if (!component)
		return -EINVAL;

	afe = snd_soc_component_get_drvdata(component);

	if (!afe)
		return -EINVAL;

	rate_reg = mt6989_rate_transform(afe->dev, rate, id);

	switch (id) {
	case MT6989_MEMIF_VUL8:
	case MT6989_MEMIF_VUL_CM0:
		cm = 0x0;
		break;
	case MT6989_MEMIF_VUL9:
	case MT6989_MEMIF_VUL_CM1:
		cm = 0x1;
		break;
	default:
		cm = 0x0;
		break;
	}

	mt6989_set_cm_rate(cm, rate_reg);

	return rate_reg;
}

static int mt6989_get_dai_fs(struct mtk_base_afe *afe,
			     int dai_id, unsigned int rate)
{
	return mt6989_rate_transform(afe->dev, rate, dai_id);
}

static int mt6989_irq_fs(struct snd_pcm_substream *substream, unsigned int rate)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = NULL;

	if (!component)
		return -EINVAL;
	afe = snd_soc_component_get_drvdata(component);
	return mt6989_general_rate_transform(afe->dev, rate);
}

int mt6989_get_memif_pbuf_size(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	if ((runtime->period_size * 1000) / runtime->rate > 10)
		return MT6989_MEMIF_PBUF_SIZE_256_BYTES;
	else
		return MT6989_MEMIF_PBUF_SIZE_32_BYTES;
}

/* FE DAIs */
static const struct snd_soc_dai_ops mt6989_memif_dai_ops = {
	.startup        = mt6989_fe_startup,
	.shutdown       = mt6989_fe_shutdown,
	.hw_params      = mtk_afe_fe_hw_params,
	.hw_free        = mtk_afe_fe_hw_free,
	.prepare        = mtk_afe_fe_prepare,
	.trigger        = mt6989_fe_trigger,
};

#define MTK_PCM_RATES (SNDRV_PCM_RATE_8000_48000 |\
		       SNDRV_PCM_RATE_88200 |\
		       SNDRV_PCM_RATE_96000 |\
		       SNDRV_PCM_RATE_176400 |\
		       SNDRV_PCM_RATE_192000)

#define MTK_PCM_DAI_RATES (SNDRV_PCM_RATE_8000 |\
			   SNDRV_PCM_RATE_16000 |\
			   SNDRV_PCM_RATE_32000 |\
			   SNDRV_PCM_RATE_48000)

#define MTK_PCM_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			 SNDRV_PCM_FMTBIT_S24_LE |\
			 SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver mt6989_memif_dai_driver[] = {
	/* FE DAIs: memory intefaces to CPU */
	{
		.name = "DL0",
		.id = MT6989_MEMIF_DL0,
		.playback = {
			.stream_name = "DL0",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "DL1",
		.id = MT6989_MEMIF_DL1,
		.playback = {
			.stream_name = "DL1",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "DL2",
		.id = MT6989_MEMIF_DL2,
		.playback = {
			.stream_name = "DL2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "DL3",
		.id = MT6989_MEMIF_DL3,
		.playback = {
			.stream_name = "DL3",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "DL4",
		.id = MT6989_MEMIF_DL4,
		.playback = {
			.stream_name = "DL4",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "DL5",
		.id = MT6989_MEMIF_DL5,
		.playback = {
			.stream_name = "DL5",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "DL6",
		.id = MT6989_MEMIF_DL6,
		.playback = {
			.stream_name = "DL6",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "DL7",
		.id = MT6989_MEMIF_DL7,
		.playback = {
			.stream_name = "DL7",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "DL8",
		.id = MT6989_MEMIF_DL8,
		.playback = {
			.stream_name = "DL8",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "DL23",
		.id = MT6989_MEMIF_DL23,
		.playback = {
			.stream_name = "DL23",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "DL24",
		.id = MT6989_MEMIF_DL24,
		.playback = {
			.stream_name = "DL24",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "DL25",
		.id = MT6989_MEMIF_DL25,
		.playback = {
			.stream_name = "DL25",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "DL_24CH",
		.id = MT6989_MEMIF_DL_24CH,
		.playback = {
			.stream_name = "DL_24CH",
			.channels_min = 1,
			.channels_max = 8,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "UL0",
		.id = MT6989_MEMIF_VUL0,
		.capture = {
			.stream_name = "UL0",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "UL1",
		.id = MT6989_MEMIF_VUL1,
		.capture = {
			.stream_name = "UL1",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "UL2",
		.id = MT6989_MEMIF_VUL2,
		.capture = {
			.stream_name = "UL2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "UL3",
		.id = MT6989_MEMIF_VUL3,
		.capture = {
			.stream_name = "UL3",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "UL4",
		.id = MT6989_MEMIF_VUL4,
		.capture = {
			.stream_name = "UL4",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "UL5",
		.id = MT6989_MEMIF_VUL5,
		.capture = {
			.stream_name = "UL5",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "UL6",
		.id = MT6989_MEMIF_VUL6,
		.capture = {
			.stream_name = "UL6",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "UL7",
		.id = MT6989_MEMIF_VUL7,
		.capture = {
			.stream_name = "UL7",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "UL8",
		.id = MT6989_MEMIF_VUL8,
		.capture = {
			.stream_name = "UL8",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "UL9",
		.id = MT6989_MEMIF_VUL9,
		.capture = {
			.stream_name = "UL9",
			.channels_min = 1,
			.channels_max = 16,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "UL10",
		.id = MT6989_MEMIF_VUL10,
		.capture = {
			.stream_name = "UL10",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "UL24",
		.id = MT6989_MEMIF_VUL24,
		.capture = {
			.stream_name = "UL24",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "UL25",
		.id = MT6989_MEMIF_VUL25,
		.capture = {
			.stream_name = "UL25",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "UL_CM0",
		.id = MT6989_MEMIF_VUL_CM0,
		.capture = {
			.stream_name = "UL_CM0",
			.channels_min = 1,
			.channels_max = 8,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "UL_CM1",
		.id = MT6989_MEMIF_VUL_CM1,
		.capture = {
			.stream_name = "UL_CM1",
			.channels_min = 1,
			.channels_max = 16,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "UL_ETDM_IN0",
		.id = MT6989_MEMIF_ETDM_IN0,
		.capture = {
			.stream_name = "UL_ETDM_IN0",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "UL_ETDM_IN1",
		.id = MT6989_MEMIF_ETDM_IN1,
		.capture = {
			.stream_name = "UL_ETDM_IN1",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "UL_ETDM_IN2",
		.id = MT6989_MEMIF_ETDM_IN2,
		.capture = {
			.stream_name = "UL_ETDM_IN2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "UL_ETDM_IN4",
		.id = MT6989_MEMIF_ETDM_IN4,
		.capture = {
			.stream_name = "UL_ETDM_IN4",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "UL_ETDM_IN6",
		.id = MT6989_MEMIF_ETDM_IN6,
		.capture = {
			.stream_name = "UL_ETDM_IN6",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
	{
		.name = "HDMI",
		.id = MT6989_MEMIF_HDMI,
		.playback = {
			.stream_name = "HDMI",
			.channels_min = 2,
			.channels_max = 8,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6989_memif_dai_ops,
	},
};

/* kcontrol */
static int mt6989_irq_cnt1_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] =
		afe_priv->irq_cnt[MT6989_PRIMARY_MEMIF];
	return 0;
}

static int mt6989_irq_cnt1_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6989_PRIMARY_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;
	int irq_cnt = afe_priv->irq_cnt[memif_num];

	dev_info(afe->dev, "%s(), irq_id %d, irq_cnt = %d, value = %ld\n",
		 __func__,
		 irq_id, irq_cnt,
		 ucontrol->value.integer.value[0]);

	if (irq_cnt == ucontrol->value.integer.value[0])
		return 0;

	irq_cnt = ucontrol->value.integer.value[0];
	afe_priv->irq_cnt[memif_num] = irq_cnt;

	if (pm_runtime_status_suspended(afe->dev) || irq_id < 0) {
		dev_info(afe->dev, "%s(), suspended || irq_id %d, not set\n",
			 __func__, irq_id);
	} else {
		struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
		const struct mtk_base_irq_data *irq_data = irqs->irq_data;

		regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				   irq_data->irq_cnt_maskbit
				   << irq_data->irq_cnt_shift,
				   irq_cnt << irq_data->irq_cnt_shift);
	}

	return 0;
}

static int mt6989_irq_cnt2_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] =
		afe_priv->irq_cnt[MT6989_RECORD_MEMIF];
	return 0;
}

static int mt6989_irq_cnt2_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6989_RECORD_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;
	int irq_cnt = afe_priv->irq_cnt[memif_num];

	dev_info(afe->dev, "%s(), irq_id %d, irq_cnt = %d, value = %ld\n",
		 __func__,
		 irq_id, irq_cnt,
		 ucontrol->value.integer.value[0]);

	if (irq_cnt == ucontrol->value.integer.value[0])
		return 0;

	irq_cnt = ucontrol->value.integer.value[0];
	afe_priv->irq_cnt[memif_num] = irq_cnt;

	if (pm_runtime_status_suspended(afe->dev) || irq_id < 0) {
		dev_info(afe->dev, "%s(), suspended || irq_id %d, not set\n",
			 __func__, irq_id);
	} else {
		struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
		const struct mtk_base_irq_data *irq_data = irqs->irq_data;

		regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				   irq_data->irq_cnt_maskbit
				   << irq_data->irq_cnt_shift,
				   irq_cnt << irq_data->irq_cnt_shift);
	}

	return 0;
}

static int mt6989_deep_irq_cnt_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->irq_cnt[MT6989_DEEP_MEMIF];
	return 0;
}

static int mt6989_deep_irq_cnt_set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6989_DEEP_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;
	int irq_cnt = afe_priv->irq_cnt[memif_num];

	dev_info(afe->dev, "%s(), irq_id %d, irq_cnt = %d, value = %ld\n",
		 __func__,
		 irq_id, irq_cnt,
		 ucontrol->value.integer.value[0]);

	if (irq_cnt == ucontrol->value.integer.value[0])
		return 0;

	irq_cnt = ucontrol->value.integer.value[0];
	afe_priv->irq_cnt[memif_num] = irq_cnt;

	if (pm_runtime_status_suspended(afe->dev) || irq_id < 0) {
		dev_info(afe->dev, "%s(), suspended || irq_id %d, not set\n",
			 __func__, irq_id);
	} else {
		struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
		const struct mtk_base_irq_data *irq_data = irqs->irq_data;

		regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				   irq_data->irq_cnt_maskbit
				   << irq_data->irq_cnt_shift,
				   irq_cnt << irq_data->irq_cnt_shift);
	}

	return 0;
}

static int mt6989_voip_rx_irq_cnt_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->irq_cnt[MT6989_VOIP_MEMIF];
	return 0;
}

static int mt6989_voip_rx_irq_cnt_set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6989_VOIP_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;
	int irq_cnt = afe_priv->irq_cnt[memif_num];

	dev_info(afe->dev, "%s(), irq_id %d, irq_cnt = %d, value = %ld\n",
		 __func__,
		 irq_id, irq_cnt,
		 ucontrol->value.integer.value[0]);

	if (irq_cnt == ucontrol->value.integer.value[0])
		return 0;

	irq_cnt = ucontrol->value.integer.value[0];
	afe_priv->irq_cnt[memif_num] = irq_cnt;

	if (pm_runtime_status_suspended(afe->dev) || irq_id < 0) {
		dev_info(afe->dev, "%s(), suspended || irq_id %d, not set\n",
			 __func__, irq_id);
	} else {
		struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
		const struct mtk_base_irq_data *irq_data = irqs->irq_data;

		regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				   irq_data->irq_cnt_maskbit
				   << irq_data->irq_cnt_shift,
				   irq_cnt << irq_data->irq_cnt_shift);
	}

	return 0;
}

static int mt6989_deep_scene_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->deep_playback_state;
	return 0;
}

static int mt6989_deep_scene_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6989_DEEP_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->deep_playback_state = ucontrol->value.integer.value[0];

	if (afe_priv->deep_playback_state == 1)
		memif->ack_enable = true;
	else
		memif->ack_enable = false;

	return 0;
}

static int mt6989_fast_scene_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->fast_playback_state;
	return 0;
}

static int mt6989_fast_scene_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6989_FAST_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->fast_playback_state = ucontrol->value.integer.value[0];

	if (afe_priv->fast_playback_state == 1)
		memif->use_dram_only = 1;
	else
		memif->use_dram_only = 0;

	return 0;
}

static int mt6989_primary_scene_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->primary_playback_state;
	return 0;
}

static int mt6989_primary_scene_set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6989_PRIMARY_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->primary_playback_state = ucontrol->value.integer.value[0];

	if (afe_priv->primary_playback_state == 1)
		memif->use_dram_only = 1;
	else
		memif->use_dram_only = 0;

	return 0;
}

static int mt6989_voip_scene_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->voip_rx_state;
	return 0;
}

static int mt6989_voip_scene_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6989_VOIP_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->voip_rx_state = ucontrol->value.integer.value[0];

	if (afe_priv->voip_rx_state == 1)
		memif->use_dram_only = 1;
	else
		memif->use_dram_only = 0;

	return 0;
}

static int mt6989_record_xrun_assert_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int xrun_assert = afe_priv->xrun_assert[MT6989_RECORD_MEMIF];

	ucontrol->value.integer.value[0] = xrun_assert;
	return 0;
}

static int mt6989_record_xrun_assert_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int xrun_assert = ucontrol->value.integer.value[0];

	if (xrun_assert != 0)
		dev_info(afe->dev, "%s(), xrun_assert %d\n", __func__, xrun_assert);
	afe_priv->xrun_assert[MT6989_RECORD_MEMIF] = xrun_assert;
	return 0;
}

static int mt6989_echo_ref_xrun_assert_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int xrun_assert = afe_priv->xrun_assert[MT6989_ECHO_REF_MEMIF];

	ucontrol->value.integer.value[0] = xrun_assert;
	return 0;
}

static int mt6989_echo_ref_xrun_assert_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int xrun_assert = ucontrol->value.integer.value[0];

	dev_info(afe->dev, "%s(), xrun_assert %d\n", __func__, xrun_assert);
	afe_priv->xrun_assert[MT6989_ECHO_REF_MEMIF] = xrun_assert;
	return 0;
}

static int mt6989_sram_size_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_audio_sram *sram = afe->sram;

	ucontrol->value.integer.value[0] =
		mtk_audio_sram_get_size(sram, sram->prefer_mode);

	return 0;
}

static int mt6989_vow_barge_in_irq_id_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int memif_num = MT6989_BARGE_IN_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;

	ucontrol->value.integer.value[0] = irq_id;
	return 0;
}


#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP) && !defined(SKIP_SB_DSP)
static int mt6989_adsp_ref_mem_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_base_afe_memif *memif;
	int memif_num = get_dsp_task_attr(AUDIO_TASK_CAPTURE_UL1_ID,
					  ADSP_TASK_ATTR_MEMREF);
	if (memif_num < 0)
		return 0;

	memif = &afe->memif[memif_num];
	ucontrol->value.integer.value[0] = memif->use_adsp_share_mem;

	return 0;
}

static int mt6989_adsp_ref_mem_set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_base_afe_memif *memif;
	int memif_num = get_dsp_task_attr(AUDIO_TASK_CAPTURE_UL1_ID,
					  ADSP_TASK_ATTR_MEMREF);
	if (memif_num < 0)
		return 0;

	memif = &afe->memif[memif_num];
	memif->use_adsp_share_mem = ucontrol->value.integer.value[0];

	return 0;
}

static int mt6989_adsp_mem_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_base_afe_memif *memif;
	int memif_num = -1;
	int task_id = get_dsp_task_id_from_str(kcontrol->id.name);

	switch (task_id) {
	case AUDIO_TASK_PRIMARY_ID:
	case AUDIO_TASK_DEEPBUFFER_ID:
	case AUDIO_TASK_FAST_ID:
	case AUDIO_TASK_OFFLOAD_ID:
	case AUDIO_TASK_PLAYBACK_ID:
	case AUDIO_TASK_CALL_FINAL_ID:
	case AUDIO_TASK_KTV_ID:
	case AUDIO_TASK_VOIP_ID:
	case AUDIO_TASK_BTDL_ID:
	case AUDIO_TASK_ECHO_REF_DL_ID:
	case AUDIO_TASK_USBDL_ID:
	case AUDIO_TASK_MDUL_ID:
		memif_num = get_dsp_task_attr(task_id,
					      ADSP_TASK_ATTR_MEMDL);
		break;
	case AUDIO_TASK_CAPTURE_UL1_ID:
	case AUDIO_TASK_FM_ADSP_ID:
	case AUDIO_TASK_BTUL_ID:
	case AUDIO_TASK_ECHO_REF_ID:
	case AUDIO_TASK_USBUL_ID:
	case AUDIO_TASK_MDDL_ID:
		memif_num = get_dsp_task_attr(task_id,
					      ADSP_TASK_ATTR_MEMUL);
		break;
	default:
		pr_info("%s(), task_id %d do not use shared mem\n",
			__func__, task_id);
		break;
	};

	if (memif_num < 0)
		return 0;

	memif = &afe->memif[memif_num];
	ucontrol->value.integer.value[0] = memif->use_adsp_share_mem;

	return 0;
}

static int mt6989_adsp_mem_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_base_afe_memif *memif;
	int dl_memif_num = -1;
	int ul_memif_num = -1;
	int ref_memif_num = -1;
	int task_id = get_dsp_task_id_from_str(kcontrol->id.name);
	int old_value;

	switch (task_id) {
	case AUDIO_TASK_PRIMARY_ID:
	case AUDIO_TASK_DEEPBUFFER_ID:
	case AUDIO_TASK_FAST_ID:
	case AUDIO_TASK_OFFLOAD_ID:
	case AUDIO_TASK_VOIP_ID:
	case AUDIO_TASK_BTDL_ID:
	case AUDIO_TASK_ECHO_REF_DL_ID:
	case AUDIO_TASK_USBDL_ID:
	case AUDIO_TASK_MDUL_ID:
		dl_memif_num = get_dsp_task_attr(task_id,
						 ADSP_TASK_ATTR_MEMDL);
		break;
	case AUDIO_TASK_CAPTURE_UL1_ID:
	case AUDIO_TASK_FM_ADSP_ID:
	case AUDIO_TASK_ECHO_REF_ID:
	case AUDIO_TASK_USBUL_ID:
	case AUDIO_TASK_MDDL_ID:
	case AUDIO_TASK_BTUL_ID:
		ul_memif_num = get_dsp_task_attr(task_id,
						 ADSP_TASK_ATTR_MEMUL);
		break;
	case AUDIO_TASK_CALL_FINAL_ID:
	case AUDIO_TASK_PLAYBACK_ID:
	case AUDIO_TASK_KTV_ID:
		dl_memif_num = get_dsp_task_attr(task_id,
						 ADSP_TASK_ATTR_MEMDL);
		ul_memif_num = get_dsp_task_attr(task_id,
						 ADSP_TASK_ATTR_MEMUL);
		ref_memif_num = get_dsp_task_attr(task_id,
						  ADSP_TASK_ATTR_MEMREF);
		break;
	default:
		pr_info("%s(), task_id %d do not use shared mem\n",
			__func__, task_id);
		break;
	};

	if (dl_memif_num >= 0) {
		memif = &afe->memif[dl_memif_num];
		old_value = memif->use_adsp_share_mem;
		memif->use_adsp_share_mem = ucontrol->value.integer.value[0];

		pr_info("%s(), dl:%s, use_adsp_share_mem %d->%d\n",
			__func__, memif->data->name, old_value, memif->use_adsp_share_mem);

		if (memif->substream && memif->use_adsp_share_mem == 0 && old_value) {
			memif->err_close_order = true;
			AUDIO_AEE("disable adsp memory used before substream shutdown");
		}
	}

	if (ul_memif_num >= 0) {
		memif = &afe->memif[ul_memif_num];
		old_value = memif->use_adsp_share_mem;
		memif->use_adsp_share_mem = ucontrol->value.integer.value[0];

		pr_info("%s(), ul:%s, use_adsp_share_mem %d->%d\n",
			__func__, memif->data->name, old_value, memif->use_adsp_share_mem);

		if (memif->substream && memif->use_adsp_share_mem == 0 && old_value) {
			memif->err_close_order = true;
			AUDIO_AEE("disable adsp memory used before substream shutdown");
		}
	}

	if (ref_memif_num >= 0) {
		memif = &afe->memif[ref_memif_num];
		old_value = memif->use_adsp_share_mem;
		memif->use_adsp_share_mem = ucontrol->value.integer.value[0];

		pr_info("%s(), ref:%s, use_adsp_share_mem %d->%d\n",
			__func__, memif->data->name, old_value, memif->use_adsp_share_mem);

		if (memif->substream && memif->use_adsp_share_mem == 0 && old_value) {
			memif->err_close_order = true;
			AUDIO_AEE("disable adsp memory used before substream shutdown");
		}
	}

	return 0;
}
#endif

static int mt6989_mmap_dl_scene_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->mmap_playback_state;
	return 0;
}

static int mt6989_mmap_dl_scene_set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6989_MMAP_DL_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->mmap_playback_state = ucontrol->value.integer.value[0];

	if (afe_priv->mmap_playback_state == 1) {
		unsigned long phy_addr;
		void *vir_addr;

		mtk_get_mmap_dl_buffer(&phy_addr, &vir_addr);

		if (phy_addr != 0x0 && vir_addr)
			memif->use_mmap_share_mem = 1;
	} else {
		memif->use_mmap_share_mem = 0;
	}

	dev_info(afe->dev, "%s(), state %d, mem %d\n", __func__,
		 afe_priv->mmap_playback_state, memif->use_mmap_share_mem);
	return 0;
}

static int mt6989_mmap_ul_scene_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->mmap_record_state;
	return 0;
}

static int mt6989_mmap_ul_scene_set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6989_MMAP_UL_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->mmap_record_state = ucontrol->value.integer.value[0];

	if (afe_priv->mmap_record_state == 1) {
		unsigned long phy_addr;
		void *vir_addr;

		mtk_get_mmap_ul_buffer(&phy_addr, &vir_addr);

		if (phy_addr != 0x0 && vir_addr)
			memif->use_mmap_share_mem = 2;
	} else {
		memif->use_mmap_share_mem = 0;
	}

	dev_info(afe->dev, "%s(), state %d, mem %d\n", __func__,
		 afe_priv->mmap_record_state, memif->use_mmap_share_mem);
	return 0;
}

static int mt6989_mmap_ion_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int mt6989_mmap_ion_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);

	dev_info(afe->dev, "%s() successfully start\n", __func__);
	mtk_exporter_init(afe->dev);
	return 0;
}

static int mt6989_dl_mmap_fd_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int memif_num = MT6989_MMAP_DL_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	ucontrol->value.integer.value[0] = (memif->use_mmap_share_mem == 1) ?
					    mtk_get_mmap_dl_fd() : 0;
	dev_info(afe->dev, "%s, fd %ld\n", __func__,
		 ucontrol->value.integer.value[0]);
	return 0;
}

static int mt6989_dl_mmap_fd_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int mt6989_ul_mmap_fd_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int memif_num = MT6989_MMAP_UL_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	ucontrol->value.integer.value[0] = (memif->use_mmap_share_mem == 2) ?
					    mtk_get_mmap_ul_fd() : 0;
	dev_info(afe->dev, "%s, fd %ld\n", __func__,
		 ucontrol->value.integer.value[0]);
	return 0;
}

static int mt6989_ul_mmap_fd_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}
static int record_miso1_en_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	pr_info("%s(), audio_r_miso1_enable = %d\n", __func__, afe_priv->audio_r_miso1_enable);

	ucontrol->value.integer.value[0] = afe_priv->audio_r_miso1_enable;
	return 0;
}

static int record_miso1_en_set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	afe_priv->audio_r_miso1_enable = ucontrol->value.integer.value[0];
	pr_info("%s(), audio_r_miso1_enable = %d\n", __func__, afe_priv->audio_r_miso1_enable);

	return 0;
}


static const char *const off_on_function[] = {"Off", "On"};

static const struct soc_enum mt6989_pcm_type_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(off_on_function),
			    off_on_function),
};

static const struct snd_kcontrol_new mt6989_pcm_kcontrols[] = {
	SOC_SINGLE_EXT("Audio IRQ1 CNT", SND_SOC_NOPM, 0, 0x3ffff, 0,
		       mt6989_irq_cnt1_get, mt6989_irq_cnt1_set),
	SOC_SINGLE_EXT("Audio IRQ2 CNT", SND_SOC_NOPM, 0, 0x3ffff, 0,
		       mt6989_irq_cnt2_get, mt6989_irq_cnt2_set),
	SOC_SINGLE_EXT("deep_buffer_irq_cnt", SND_SOC_NOPM, 0, 0x3ffff, 0,
		       mt6989_deep_irq_cnt_get, mt6989_deep_irq_cnt_set),
	SOC_SINGLE_EXT("voip_rx_irq_cnt", SND_SOC_NOPM, 0, 0x3ffff, 0,
		       mt6989_voip_rx_irq_cnt_get, mt6989_voip_rx_irq_cnt_set),
	SOC_SINGLE_EXT("deep_buffer_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6989_deep_scene_get, mt6989_deep_scene_set),
	SOC_SINGLE_EXT("record_xrun_assert", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6989_record_xrun_assert_get,
		       mt6989_record_xrun_assert_set),
	SOC_SINGLE_EXT("echo_ref_xrun_assert", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6989_echo_ref_xrun_assert_get,
		       mt6989_echo_ref_xrun_assert_set),
	SOC_SINGLE_EXT("fast_play_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6989_fast_scene_get, mt6989_fast_scene_set),
	SOC_SINGLE_EXT("primary_play_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6989_primary_scene_get, mt6989_primary_scene_set),
	SOC_SINGLE_EXT("voip_rx_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6989_voip_scene_get, mt6989_voip_scene_set),
	SOC_SINGLE_EXT("sram_size", SND_SOC_NOPM, 0, 0xffffffff, 0,
		       mt6989_sram_size_get, NULL),
	SOC_SINGLE_EXT("vow_barge_in_irq_id", SND_SOC_NOPM, 0, 0x3ffff, 0,
		       mt6989_vow_barge_in_irq_id_get, NULL),
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP) && !defined(SKIP_SB_DSP)
	SOC_SINGLE_EXT("adsp_primary_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6989_adsp_mem_get,
		       mt6989_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_deepbuffer_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6989_adsp_mem_get,
		       mt6989_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_voip_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6989_adsp_mem_get,
		       mt6989_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_playback_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6989_adsp_mem_get,
		       mt6989_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_call_final_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6989_adsp_mem_get,
		       mt6989_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_ktv_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6989_adsp_mem_get,
		       mt6989_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_fm_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6989_adsp_mem_get,
		       mt6989_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_offload_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6989_adsp_mem_get,
		       mt6989_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_capture_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6989_adsp_mem_get,
		       mt6989_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_ref_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6989_adsp_ref_mem_get,
		       mt6989_adsp_ref_mem_set),
	SOC_SINGLE_EXT("adsp_fast_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6989_adsp_mem_get,
		       mt6989_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_spatializer_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6989_adsp_mem_get,
		       mt6989_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_btdl_sharemem_scenario",
			   SND_SOC_NOPM, 0, 0x1, 0,
			   mt6989_adsp_mem_get,
			   mt6989_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_btul_sharemem_scenario",
			   SND_SOC_NOPM, 0, 0x1, 0,
			   mt6989_adsp_mem_get,
			   mt6989_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_echoref_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6989_adsp_mem_get,
		       mt6989_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_echodl_sharemem_scenario",
			   SND_SOC_NOPM, 0, 0x1, 0,
			   mt6989_adsp_mem_get,
			   mt6989_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_usbdl_sharemem_scenario",
			   SND_SOC_NOPM, 0, 0x1, 0,
			   mt6989_adsp_mem_get,
			   mt6989_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_usbul_sharemem_scenario",
			   SND_SOC_NOPM, 0, 0x1, 0,
			   mt6989_adsp_mem_get,
			   mt6989_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_mddl_sharemem_scenario",
			   SND_SOC_NOPM, 0, 0x1, 0,
			   mt6989_adsp_mem_get,
			   mt6989_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_mdul_sharemem_scenario",
			   SND_SOC_NOPM, 0, 0x1, 0,
			   mt6989_adsp_mem_get,
			   mt6989_adsp_mem_set),
#endif
	SOC_SINGLE_EXT("mmap_play_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6989_mmap_dl_scene_get, mt6989_mmap_dl_scene_set),
	SOC_SINGLE_EXT("mmap_record_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6989_mmap_ul_scene_get, mt6989_mmap_ul_scene_set),
	SOC_SINGLE_EXT("aaudio_ion",
		       SND_SOC_NOPM, 0, 0xffffffff, 0,
		       mt6989_mmap_ion_get,
		       mt6989_mmap_ion_set),
	SOC_SINGLE_EXT("aaudio_dl_mmap_fd",
		       SND_SOC_NOPM, 0, 0xffffffff, 0,
		       mt6989_dl_mmap_fd_get,
		       mt6989_dl_mmap_fd_set),
	SOC_SINGLE_EXT("aaudio_ul_mmap_fd",
		       SND_SOC_NOPM, 0, 0xffffffff, 0,
		       mt6989_ul_mmap_fd_get,
		       mt6989_ul_mmap_fd_set),
	SOC_ENUM_EXT("MTK_RECORD_MISO1", mt6989_pcm_type_enum[0],
		     record_miso1_en_get, record_miso1_en_set),
};

enum {
	CM0_MUX_VUL8_2CH,
	CM0_MUX_VUL8_8CH,
	CM0_MUX_MASK,
};
enum {
	CM1_MUX_VUL9_2CH,
	CM1_MUX_VUL9_16CH,
	CM1_MUX_MASK,
};

static int ul_cm0_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol,
			int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	unsigned int channels = 0;

	dev_dbg(afe->dev, "%s(), event 0x%x, name %s\n",
		 __func__, event, w->name);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		channels = mtk_get_channel_value();
		mt6989_enable_cm_bypass(afe, CM0, 0x0);
		mt6989_set_cm(afe, CM0, 0x1, false, channels);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		mt6989_enable_cm_bypass(afe, CM0, 0x1);
		break;
	default:
		break;
	}
	return 0;
}

static int ul_cm1_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol,
			int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	unsigned int channels = 0;

	dev_dbg(afe->dev, "%s(), event 0x%x, name %s\n",
		 __func__, event, w->name);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		channels = mtk_get_channel_value();
		mt6989_enable_cm_bypass(afe, CM1, 0x0);
		mt6989_set_cm(afe, CM1, 0x1, false, channels);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* remove so we fix in normal mode */
		/* mt6989_enable_cm_bypass(afe, CM1, 0x1); */
		break;
	default:
		break;
	}
	return 0;
}

/* dma widget & routes*/
static const struct snd_kcontrol_new memif_ul0_ch1_mix[] = {
	/* Normal record */
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN018_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN018_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN018_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN018_0,
	I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN018_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN018_0,
				    I_ADDA_UL_CH6, 1, 0),
	/* FM */
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH1", AFE_CONN018_0,
				    I_CONNSYS_I2S_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN018_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN018_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN018_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN018_1,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN018_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN018_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH1", AFE_CONN018_1,
				    I_DL7_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL23_CH1", AFE_CONN018_2,
				    I_DL23_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN018_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN018_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN018_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH1", AFE_CONN018_4,
				    I_I2SIN0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH1", AFE_CONN018_4,
				    I_I2SIN1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH1", AFE_CONN018_6,
				    I_SRC_0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH1", AFE_CONN018_6,
				    I_SRC_2_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul0_ch2_mix[] = {
	/* Normal record */
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN019_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN019_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN019_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN019_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN019_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN019_0,
				    I_ADDA_UL_CH6, 1, 0),
	/* FM */
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH2", AFE_CONN019_0,
				    I_CONNSYS_I2S_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN019_1,
				    I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN019_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN019_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN019_1,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN019_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN019_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH2", AFE_CONN019_1,
				    I_DL7_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL23_CH2", AFE_CONN018_2,
				    I_DL23_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN019_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN019_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH2", AFE_CONN019_4,
				    I_PCM_0_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN019_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH2", AFE_CONN019_4,
				    I_PCM_1_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH2", AFE_CONN019_4,
				    I_I2SIN0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH2", AFE_CONN019_4,
				    I_I2SIN1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH2", AFE_CONN019_6,
				    I_SRC_0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH2", AFE_CONN019_6,
				    I_SRC_2_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul1_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN020_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH1", AFE_CONN020_0,
				    I_CONNSYS_I2S_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN020_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN020_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN020_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN020_1,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN020_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN020_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH1", AFE_CONN020_1,
				    I_DL7_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL23_CH1", AFE_CONN020_2,
				    I_DL23_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN020_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN020_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN020_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH1", AFE_CONN020_4,
				    I_I2SIN0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH1", AFE_CONN020_4,
				    I_I2SIN1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH1", AFE_CONN020_4,
				    I_I2SIN4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH1", AFE_CONN020_6,
				    I_SRC_0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH1", AFE_CONN020_6,
				    I_SRC_2_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul1_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN021_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH2", AFE_CONN021_0,
				    I_CONNSYS_I2S_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN021_1,
				    I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN021_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN021_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN021_1,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN021_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN021_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH2", AFE_CONN021_1,
				    I_DL7_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL23_CH2", AFE_CONN021_2,
				    I_DL23_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN021_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN021_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH2", AFE_CONN021_4,
				    I_PCM_0_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN021_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH2", AFE_CONN021_4,
				    I_PCM_1_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH2", AFE_CONN021_4,
				    I_I2SIN0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH2", AFE_CONN021_4,
				    I_I2SIN1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH2", AFE_CONN021_4,
				    I_I2SIN4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH2", AFE_CONN021_6,
				    I_SRC_0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH2", AFE_CONN021_6,
				    I_SRC_2_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul2_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN022_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN022_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN022_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN022_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN1_OUT_CH1", AFE_CONN022_0,
				    I_GAIN1_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH1", AFE_CONN022_6,
				    I_SRC_1_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul2_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN023_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN023_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN023_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN023_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN1_OUT_CH2", AFE_CONN023_0,
				    I_GAIN1_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH2", AFE_CONN023_6,
				    I_SRC_1_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul3_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN024_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH1", AFE_CONN024_4,
				    I_I2SIN0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH1", AFE_CONN024_4,
				    I_I2SIN1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH1", AFE_CONN024_4,
				    I_I2SIN4_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul3_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN025_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH2", AFE_CONN025_4,
				    I_I2SIN0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH2", AFE_CONN025_4,
				    I_I2SIN1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH2", AFE_CONN025_4,
				    I_I2SIN4_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul4_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN026_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN026_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN026_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN026_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN026_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN026_1,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN026_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN026_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN026_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH1", AFE_CONN026_4,
					I_I2SIN0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN0_OUT_CH1", AFE_CONN026_0,
				    I_GAIN0_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul4_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN027_0,
					I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN027_1,
				    I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN027_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN027_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN027_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN027_1,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN027_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN027_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH2", AFE_CONN027_4,
				    I_PCM_0_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN027_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH2", AFE_CONN027_4,
				    I_PCM_1_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH2", AFE_CONN027_4,
					I_I2SIN0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN0_OUT_CH2", AFE_CONN027_0,
				    I_GAIN0_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul5_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN028_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN028_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN028_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN028_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN028_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN028_1,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN028_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN028_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN028_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("GAIN0_OUT_CH1", AFE_CONN028_0,
				    I_GAIN0_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul5_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN029_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN029_1,
				    I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN029_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN029_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN029_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN029_1,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN029_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN029_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH2", AFE_CONN029_4,
				    I_PCM_0_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN029_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH2", AFE_CONN029_4,
				    I_PCM_1_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("GAIN0_OUT_CH2", AFE_CONN029_0,
				    I_GAIN0_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul6_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN030_0,
					I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH1", AFE_CONN030_0,
				    I_CONNSYS_I2S_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN030_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN030_1,
				    I_DL2_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul6_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN031_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH2", AFE_CONN031_0,
				    I_CONNSYS_I2S_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN031_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN031_1,
				    I_DL2_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul7_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN032_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN032_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN032_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN032_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH1", AFE_CONN032_0,
				    I_CONNSYS_I2S_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN032_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN032_1,
				    I_DL2_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul7_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN033_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN033_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN033_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN033_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH2", AFE_CONN033_0,
				    I_CONNSYS_I2S_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN033_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN033_1,
				    I_DL2_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul8_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN034_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN034_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN034_4,
				    I_PCM_1_CAP_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul8_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN035_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN035_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH2", AFE_CONN035_4,
				    I_PCM_0_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN035_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH2", AFE_CONN035_4,
				    I_PCM_1_CAP_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul9_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN036_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN036_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN036_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN036_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN036_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN036_0,
				    I_ADDA_UL_CH6, 1, 0),
};

static const struct snd_kcontrol_new memif_ul9_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN037_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN037_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN037_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN037_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN037_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN037_0,
				    I_ADDA_UL_CH6, 1, 0),
};

static const struct snd_kcontrol_new memif_ul24_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN066_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH1", AFE_CONN066_5,
					I_I2SIN6_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul24_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN067_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH2", AFE_CONN067_5,
					I_I2SIN6_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul25_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH1", AFE_CONN068_5,
					I_I2SIN6_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul25_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH2", AFE_CONN069_5,
					I_I2SIN6_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_dsp_dl_playback_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL0", SND_SOC_NOPM, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL2", SND_SOC_NOPM, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL1", SND_SOC_NOPM, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL6", SND_SOC_NOPM, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL3", SND_SOC_NOPM, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL_24CH", SND_SOC_NOPM, 0, 1, 0),
};

static const struct snd_kcontrol_new memif_ul_cm0_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN040_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN040_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN040_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN040_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN040_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN040_0,
				    I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN1_OUT_CH1", AFE_CONN040_0,
				    I_GAIN1_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH1", AFE_CONN040_6,
				    I_SRC_0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH1", AFE_CONN040_6,
				    I_SRC_1_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH1", AFE_CONN040_4,
				    I_I2SIN4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH2", AFE_CONN040_4,
				    I_I2SIN4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH3", AFE_CONN040_4,
				    I_I2SIN4_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH4", AFE_CONN040_4,
				    I_I2SIN4_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm0_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN041_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN041_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN041_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN041_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN041_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN041_0,
				    I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN1_OUT_CH2", AFE_CONN041_0,
				    I_GAIN1_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH2", AFE_CONN041_6,
				    I_SRC_0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH2", AFE_CONN041_6,
				    I_SRC_1_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH1", AFE_CONN041_4,
				    I_I2SIN4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH2", AFE_CONN041_4,
				    I_I2SIN4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH3", AFE_CONN041_4,
				    I_I2SIN4_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH4", AFE_CONN041_4,
				    I_I2SIN4_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm0_ch3_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN042_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN042_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN042_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN042_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH1", AFE_CONN042_4,
				    I_I2SIN4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH2", AFE_CONN042_4,
				    I_I2SIN4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH3", AFE_CONN042_4,
				    I_I2SIN4_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH4", AFE_CONN042_4,
				    I_I2SIN4_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm0_ch4_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN043_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN043_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN043_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN043_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH1", AFE_CONN043_4,
				    I_I2SIN4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH2", AFE_CONN043_4,
				    I_I2SIN4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH3", AFE_CONN043_4,
				    I_I2SIN4_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH4", AFE_CONN043_4,
				    I_I2SIN4_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm0_ch5_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN044_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN044_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN044_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN044_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm0_ch6_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN045_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN045_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN045_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN045_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm0_ch7_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN046_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN046_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN046_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN046_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm0_ch8_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN047_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN047_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN047_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN047_0,
				    I_ADDA_UL_CH4, 1, 0),
};

static const struct snd_kcontrol_new memif_ul_cm1_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN048_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN048_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN048_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN048_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN048_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN048_0,
				    I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH1", AFE_CONN048_6,
				    I_SRC_0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH1", AFE_CONN048_4,
				    I_I2SIN4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH2", AFE_CONN048_4,
				    I_I2SIN4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH3", AFE_CONN048_4,
				    I_I2SIN4_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH4", AFE_CONN048_4,
				    I_I2SIN4_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN049_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN049_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN049_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN049_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN049_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN049_0,
				    I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH2", AFE_CONN049_6,
				    I_SRC_0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH1", AFE_CONN049_4,
				    I_I2SIN4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH2", AFE_CONN049_4,
				    I_I2SIN4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH3", AFE_CONN049_4,
				    I_I2SIN4_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH4", AFE_CONN049_4,
				    I_I2SIN4_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch3_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN050_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN050_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN050_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN050_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN050_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN050_0,
				    I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH1", AFE_CONN050_4,
				    I_I2SIN4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH2", AFE_CONN050_4,
				    I_I2SIN4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH3", AFE_CONN050_4,
				    I_I2SIN4_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH4", AFE_CONN050_4,
				    I_I2SIN4_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch4_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN051_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN051_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN051_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN051_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN051_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN051_0,
				    I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH1", AFE_CONN051_4,
				    I_I2SIN4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH2", AFE_CONN051_4,
				    I_I2SIN4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH3", AFE_CONN051_4,
				    I_I2SIN4_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH4", AFE_CONN051_4,
				    I_I2SIN4_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch5_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN052_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN052_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN052_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN052_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN052_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN052_0,
				    I_ADDA_UL_CH6, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch6_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN053_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN053_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN053_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN053_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN053_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN053_0,
				    I_ADDA_UL_CH6, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch7_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN054_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN054_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN054_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN054_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch8_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN055_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN055_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN055_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN055_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch9_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN056_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN056_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN056_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN056_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch10_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN057_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN057_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN057_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN057_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch11_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN058_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN058_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN058_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN058_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch12_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN059_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN059_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN059_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN059_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch13_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN060_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN060_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN060_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN060_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch14_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN061_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN061_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN061_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN061_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch15_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN062_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN062_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN062_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN062_0,
				    I_ADDA_UL_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch16_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN063_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN063_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN063_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN063_0,
				    I_ADDA_UL_CH4, 1, 0),
};

static const char * const cm0_mux_map[] = {
	"CM0_8CH_PATH",
	"CM0_2CH_PATH",
};
static const char * const cm1_mux_map[] = {
	"CM1_16CH_PATH",
	"CM1_2CH_PATH",
};

static int cm0_mux_map_value[] = {
	CM0_MUX_VUL8_8CH,
	CM0_MUX_VUL8_2CH,
};

static int cm1_mux_map_value[] = {
	CM1_MUX_VUL9_16CH,
	CM1_MUX_VUL9_2CH,
};

static SOC_VALUE_ENUM_SINGLE_DECL(ul_cm0_mux_map_enum,
				  AFE_CM0_CON0,
				  AFE_CM0_OUTPUT_MUX_SFT,
				  AFE_CM0_OUTPUT_MUX_MASK,
				  cm0_mux_map,
				  cm0_mux_map_value);
static SOC_VALUE_ENUM_SINGLE_DECL(ul_cm1_mux_map_enum,
				  AFE_CM1_CON0,
				  AFE_CM1_OUTPUT_MUX_SFT,
				  AFE_CM1_OUTPUT_MUX_MASK,
				  cm1_mux_map,
				  cm1_mux_map_value);

static const struct snd_kcontrol_new ul_cm0_mux_control =
	SOC_DAPM_ENUM("CM0_UL_MUX Select", ul_cm0_mux_map_enum);
static const struct snd_kcontrol_new ul_cm1_mux_control =
	SOC_DAPM_ENUM("CM1_UL_MUX Select", ul_cm1_mux_map_enum);

static const struct snd_soc_dapm_widget mt6989_memif_widgets[] = {
	/* inter-connections */
	SND_SOC_DAPM_MIXER("UL0_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul0_ch1_mix, ARRAY_SIZE(memif_ul0_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL0_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul0_ch2_mix, ARRAY_SIZE(memif_ul0_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL1_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul1_ch1_mix, ARRAY_SIZE(memif_ul1_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL1_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul1_ch2_mix, ARRAY_SIZE(memif_ul1_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL2_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul2_ch1_mix, ARRAY_SIZE(memif_ul2_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL2_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul2_ch2_mix, ARRAY_SIZE(memif_ul2_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL3_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul3_ch1_mix, ARRAY_SIZE(memif_ul3_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL3_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul3_ch2_mix, ARRAY_SIZE(memif_ul3_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL4_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul4_ch1_mix, ARRAY_SIZE(memif_ul4_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL4_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul4_ch2_mix, ARRAY_SIZE(memif_ul4_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL5_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul5_ch1_mix, ARRAY_SIZE(memif_ul5_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL5_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul5_ch2_mix, ARRAY_SIZE(memif_ul5_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL6_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul6_ch1_mix, ARRAY_SIZE(memif_ul6_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL6_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul6_ch2_mix, ARRAY_SIZE(memif_ul6_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL7_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul7_ch1_mix, ARRAY_SIZE(memif_ul7_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL7_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul7_ch2_mix, ARRAY_SIZE(memif_ul7_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL8_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul8_ch1_mix, ARRAY_SIZE(memif_ul8_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL8_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul8_ch2_mix, ARRAY_SIZE(memif_ul8_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL9_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul9_ch1_mix, ARRAY_SIZE(memif_ul9_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL9_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul9_ch2_mix, ARRAY_SIZE(memif_ul9_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL24_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul24_ch1_mix, ARRAY_SIZE(memif_ul24_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL24_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul24_ch2_mix, ARRAY_SIZE(memif_ul24_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL25_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul25_ch1_mix, ARRAY_SIZE(memif_ul25_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL25_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul25_ch2_mix, ARRAY_SIZE(memif_ul25_ch2_mix)),
	SND_SOC_DAPM_MIXER("DSP_DL", SND_SOC_NOPM, 0, 0,
			   mtk_dsp_dl_playback_mix,
			   ARRAY_SIZE(mtk_dsp_dl_playback_mix)),

	SND_SOC_DAPM_MIXER("UL_CM0_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch1_mix, ARRAY_SIZE(memif_ul_cm0_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL_CM0_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch2_mix, ARRAY_SIZE(memif_ul_cm0_ch2_mix)),
	SND_SOC_DAPM_MIXER("UL_CM0_CH3", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch3_mix, ARRAY_SIZE(memif_ul_cm0_ch3_mix)),
	SND_SOC_DAPM_MIXER("UL_CM0_CH4", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch4_mix, ARRAY_SIZE(memif_ul_cm0_ch4_mix)),
	SND_SOC_DAPM_MIXER("UL_CM0_CH5", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch5_mix, ARRAY_SIZE(memif_ul_cm0_ch5_mix)),
	SND_SOC_DAPM_MIXER("UL_CM0_CH6", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch6_mix, ARRAY_SIZE(memif_ul_cm0_ch6_mix)),
	SND_SOC_DAPM_MIXER("UL_CM0_CH7", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch7_mix, ARRAY_SIZE(memif_ul_cm0_ch7_mix)),
	SND_SOC_DAPM_MIXER("UL_CM0_CH8", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch8_mix, ARRAY_SIZE(memif_ul_cm0_ch8_mix)),
	SND_SOC_DAPM_MUX_E("CM0_UL_MUX", SND_SOC_NOPM, 0, 0,
			   &ul_cm0_mux_control,
			   ul_cm0_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_MIXER("UL_CM1_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch1_mix, ARRAY_SIZE(memif_ul_cm1_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch2_mix, ARRAY_SIZE(memif_ul_cm1_ch2_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH3", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch3_mix, ARRAY_SIZE(memif_ul_cm1_ch3_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH4", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch4_mix, ARRAY_SIZE(memif_ul_cm1_ch4_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH5", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch5_mix, ARRAY_SIZE(memif_ul_cm1_ch5_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH6", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch6_mix, ARRAY_SIZE(memif_ul_cm1_ch6_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH7", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch7_mix, ARRAY_SIZE(memif_ul_cm1_ch7_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH8", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch8_mix, ARRAY_SIZE(memif_ul_cm1_ch8_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH9", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch9_mix, ARRAY_SIZE(memif_ul_cm1_ch9_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH10", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch10_mix, ARRAY_SIZE(memif_ul_cm1_ch10_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH11", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch11_mix, ARRAY_SIZE(memif_ul_cm1_ch11_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH12", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch12_mix, ARRAY_SIZE(memif_ul_cm1_ch12_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH13", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch13_mix, ARRAY_SIZE(memif_ul_cm1_ch13_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH14", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch14_mix, ARRAY_SIZE(memif_ul_cm1_ch14_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH15", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch15_mix, ARRAY_SIZE(memif_ul_cm1_ch15_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH16", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch16_mix, ARRAY_SIZE(memif_ul_cm1_ch16_mix)),
	SND_SOC_DAPM_MUX("CM1_UL_MUX", SND_SOC_NOPM, 0, 0,
			   &ul_cm1_mux_control),
	SND_SOC_DAPM_SUPPLY("CM0_Enable",
			AFE_CM0_CON0, AFE_CM0_ON_SFT, 0,
			ul_cm0_event,
			SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY("CM1_Enable",
			SND_SOC_NOPM, 0, 0,
			ul_cm1_event,
			SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_INPUT("UL1_VIRTUAL_INPUT"),
	SND_SOC_DAPM_INPUT("UL2_VIRTUAL_INPUT"),
	SND_SOC_DAPM_INPUT("UL4_VIRTUAL_INPUT"),

	SND_SOC_DAPM_OUTPUT("DL_TO_DSP"),
	SND_SOC_DAPM_OUTPUT("DL6_VIRTUAL_OUTPUT"),
};

static const struct snd_soc_dapm_route mt6989_memif_routes[] = {
	{"UL0", NULL, "UL0_CH1"},
	{"UL0", NULL, "UL0_CH2"},
	/* Normal record */
	{"UL0_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL0_CH1", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL0_CH1", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL0_CH1", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL0_CH1", "ADDA_UL_CH5", "ADDA_CH56_UL_Mux"},
	{"UL0_CH1", "ADDA_UL_CH6", "ADDA_CH56_UL_Mux"},
	{"UL0_CH2", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL0_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL0_CH2", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL0_CH2", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL0_CH2", "ADDA_UL_CH5", "ADDA_CH56_UL_Mux"},
	{"UL0_CH2", "ADDA_UL_CH6", "ADDA_CH56_UL_Mux"},
	/* FM */
	{"UL0_CH1", "CONNSYS_I2S_CH1", "Connsys I2S"},
	{"UL0_CH2", "CONNSYS_I2S_CH2", "Connsys I2S"},

	{"UL0_CH1", "I2SIN0_CH1", "I2SIN0"},
	{"UL0_CH2", "I2SIN0_CH2", "I2SIN0"},
	{"UL0_CH1", "I2SIN1_CH1", "I2SIN1"},
	{"UL0_CH2", "I2SIN1_CH2", "I2SIN1"},

	{"UL0_CH1", "PCM_0_CAP_CH1", "PCM 0 Capture"},
	{"UL0_CH2", "PCM_0_CAP_CH1", "PCM 0 Capture"},
	{"UL0_CH1", "PCM_1_CAP_CH1", "PCM 1 Capture"},
	{"UL0_CH2", "PCM_1_CAP_CH1", "PCM 1 Capture"},


	{"UL0_CH1", "HW_SRC_0_OUT_CH1", "HW_SRC_0_Out"},
	{"UL0_CH2", "HW_SRC_0_OUT_CH2", "HW_SRC_0_Out"},
	{"UL0_CH1", "HW_SRC_2_OUT_CH1", "HW_SRC_2_Out"},
	{"UL0_CH2", "HW_SRC_2_OUT_CH2", "HW_SRC_2_Out"},

	{"UL1", NULL, "UL1_CH1"},
	{"UL1", NULL, "UL1_CH2"},
	/* cannot connect FE to FE directly */
	{"UL1_CH1", "DL0_CH1", "Hostless_UL1 UL"},
	{"UL1_CH2", "DL0_CH2", "Hostless_UL1 UL"},
	{"UL1_CH1", "DL1_CH1", "Hostless_UL1 UL"},
	{"UL1_CH2", "DL1_CH2", "Hostless_UL1 UL"},
	{"UL1_CH1", "DL6_CH1", "Hostless_UL1 UL"},
	{"UL1_CH2", "DL6_CH2", "Hostless_UL1 UL"},
	{"UL1_CH1", "DL2_CH1", "Hostless_UL1 UL"},
	{"UL1_CH2", "DL2_CH2", "Hostless_UL1 UL"},
	{"UL1_CH1", "DL3_CH1", "Hostless_UL1 UL"},
	{"UL1_CH2", "DL3_CH2", "Hostless_UL1 UL"},
	{"UL1_CH1", "DL4_CH1", "Hostless_UL1 UL"},
	{"UL1_CH2", "DL4_CH2", "Hostless_UL1 UL"},
	{"UL1_CH1", "DL7_CH1", "Hostless_UL1 UL"},
	{"UL1_CH2", "DL7_CH2", "Hostless_UL1 UL"},
	{"UL1_CH1", "DL23_CH1", "Hostless_UL1 UL"},
	{"UL1_CH2", "DL23_CH2", "Hostless_UL1 UL"},
	{"UL1_CH1", "DL_24CH_CH1", "Hostless_UL1 UL"},
	{"UL1_CH2", "DL_24CH_CH2", "Hostless_UL1 UL"},

	{"Hostless_UL1 UL", NULL, "UL1_VIRTUAL_INPUT"},
	{"UL1_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL1_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},

	{"UL1_CH1", "I2SIN0_CH1", "I2SIN0"},
	{"UL1_CH2", "I2SIN0_CH2", "I2SIN0"},
	{"UL1_CH1", "I2SIN1_CH1", "I2SIN1"},
	{"UL1_CH2", "I2SIN1_CH2", "I2SIN1"},
	{"UL1_CH1", "I2SIN4_CH1", "I2SIN4"},
	{"UL1_CH2", "I2SIN4_CH2", "I2SIN4"},

	{"UL1_CH1", "PCM_0_CAP_CH1", "PCM 0 Capture"},
	{"UL1_CH2", "PCM_0_CAP_CH1", "PCM 0 Capture"},
	{"UL1_CH1", "PCM_1_CAP_CH1", "PCM 1 Capture"},
	{"UL1_CH2", "PCM_1_CAP_CH1", "PCM 1 Capture"},

	{"UL1_CH1", "CONNSYS_I2S_CH1", "Connsys I2S"},
	{"UL1_CH2", "CONNSYS_I2S_CH2", "Connsys I2S"},

	{"UL1_CH1", "HW_SRC_0_OUT_CH1", "HW_SRC_0_Out"},
	{"UL1_CH2", "HW_SRC_0_OUT_CH2", "HW_SRC_0_Out"},
	{"UL1_CH1", "HW_SRC_2_OUT_CH1", "HW_SRC_2_Out"},
	{"UL1_CH2", "HW_SRC_2_OUT_CH2", "HW_SRC_2_Out"},

	{"UL2", NULL, "UL2_CH1"},
	{"UL2", NULL, "UL2_CH2"},
	{"UL2_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL2_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL2_CH1", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL2_CH2", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL2_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL2_CH2", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL2_CH2", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL2_CH1", "HW_GAIN1_OUT_CH1", "HW Gain 1 Out"},
	{"UL2_CH2", "HW_GAIN1_OUT_CH2", "HW Gain 1 Out"},
	{"UL2_CH1", "HW_SRC_1_OUT_CH1", "HW_SRC_1_Out"},
	{"UL2_CH2", "HW_SRC_1_OUT_CH2", "HW_SRC_1_Out"},

	{"UL3", NULL, "UL3_CH1"},
	{"UL3", NULL, "UL3_CH2"},

	{"UL3_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL3_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL3_CH1", "I2SIN0_CH1", "I2SIN0"},
	{"UL3_CH2", "I2SIN0_CH2", "I2SIN0"},
	{"UL3_CH1", "I2SIN1_CH1", "I2SIN1"},
	{"UL3_CH2", "I2SIN1_CH2", "I2SIN1"},
	{"UL3_CH1", "I2SIN4_CH1", "I2SIN4"},
	{"UL3_CH2", "I2SIN4_CH2", "I2SIN4"},

	{"UL4", NULL, "UL4_CH1"},
	{"UL4", NULL, "UL4_CH2"},
	{"UL4_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL4_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL4_CH1", "I2SIN0_CH1", "I2SIN0"},
	{"UL4_CH2", "I2SIN0_CH2", "I2SIN0"},

	{"UL4_CH1", "DL0_CH1", "Hostless_UL4 UL"},
	{"UL4_CH2", "DL0_CH2", "Hostless_UL4 UL"},
	{"UL4_CH1", "DL2_CH1", "Hostless_UL4 UL"},
	{"UL4_CH2", "DL2_CH2", "Hostless_UL4 UL"},
	{"UL4_CH1", "DL1_CH1", "Hostless_UL4 UL"},
	{"UL4_CH2", "DL1_CH2", "Hostless_UL4 UL"},
	{"UL4_CH1", "DL6_CH1", "Hostless_UL4 UL"},
	{"UL4_CH2", "DL6_CH2", "Hostless_UL4 UL"},
	{"UL4_CH1", "DL3_CH1", "Hostless_UL4 UL"},
	{"UL4_CH2", "DL3_CH2", "Hostless_UL4 UL"},
	{"UL4_CH1", "DL_24CH_CH1", "Hostless_UL4 UL"},
	{"UL4_CH2", "DL_24CH_CH2", "Hostless_UL4 UL"},
	{"Hostless_UL4 UL", NULL, "UL4_VIRTUAL_INPUT"},

	{"UL4_CH1", "PCM_0_CAP_CH1", "PCM 0 Capture"},
	{"UL4_CH2", "PCM_0_CAP_CH1", "PCM 0 Capture"},
	{"UL4_CH1", "PCM_1_CAP_CH1", "PCM 1 Capture"},
	{"UL4_CH2", "PCM_1_CAP_CH1", "PCM 1 Capture"},
	{"UL4_CH1", "HW_GAIN0_OUT_CH1", "HW Gain 0 Out"},
	{"UL4_CH2", "HW_GAIN0_OUT_CH2", "HW Gain 0 Out"},

	{"UL5", NULL, "UL5_CH1"},
	{"UL5", NULL, "UL5_CH2"},

	{"UL5_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL5_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL5_CH1", "PCM_0_CAP_CH1", "PCM 0 Capture"},
	{"UL5_CH2", "PCM_0_CAP_CH1", "PCM 0 Capture"},
	{"UL5_CH1", "PCM_1_CAP_CH1", "PCM 1 Capture"},
	{"UL5_CH2", "PCM_1_CAP_CH1", "PCM 1 Capture"},
	{"UL5_CH1", "GAIN0_OUT_CH1", "HW Gain 0 Out"},
	{"UL5_CH2", "GAIN0_OUT_CH2", "HW Gain 0 Out"},

	{"UL6", NULL, "UL6_CH1"},
	{"UL6", NULL, "UL6_CH2"},
	{"UL6_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL6_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL6_CH1", "CONNSYS_I2S_CH1", "Connsys I2S"},
	{"UL6_CH2", "CONNSYS_I2S_CH2", "Connsys I2S"},

	{"UL7", NULL, "UL7_CH1"},
	{"UL7", NULL, "UL7_CH2"},
	{"UL7_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL7_CH1", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL7_CH1", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL7_CH2", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL7_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL7_CH2", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL7_CH2", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL7_CH1", "CONNSYS_I2S_CH1", "Connsys I2S"},
	{"UL7_CH2", "CONNSYS_I2S_CH2", "Connsys I2S"},

	{"UL8", NULL, "CM0_UL_MUX"},
	{"CM0_UL_MUX", "CM0_2CH_PATH", "UL8_CH1"},
	{"CM0_UL_MUX", "CM0_2CH_PATH", "UL8_CH2"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH1"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH2"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH3"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH4"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH5"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH6"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH7"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH8"},
	{"UL_CM0_CH1", NULL, "CM0_Enable"},
	{"UL_CM0_CH2", NULL, "CM0_Enable"},
	{"UL_CM0_CH3", NULL, "CM0_Enable"},
	{"UL_CM0_CH4", NULL, "CM0_Enable"},
	{"UL_CM0_CH5", NULL, "CM0_Enable"},
	{"UL_CM0_CH6", NULL, "CM0_Enable"},
	{"UL_CM0_CH7", NULL, "CM0_Enable"},
	{"UL_CM0_CH8", NULL, "CM0_Enable"},

	/* UL8 o34o35 <- ADDA */
	{"UL8_CH1", "PCM_0_CAP_CH1", "PCM 0 Capture"},
	{"UL8_CH1", "PCM_1_CAP_CH1", "PCM 1 Capture"},
	{"UL8_CH2", "PCM_0_CAP_CH2", "PCM 0 Capture"},
	{"UL8_CH2", "PCM_1_CAP_CH2", "PCM 1 Capture"},

	{"UL9", NULL, "CM1_UL_MUX"},
	{"CM1_UL_MUX", "CM1_2CH_PATH", "UL9_CH1"},
	{"CM1_UL_MUX", "CM1_2CH_PATH", "UL9_CH2"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH1"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH2"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH3"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH4"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH5"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH6"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH7"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH8"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH9"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH10"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH11"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH12"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH13"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH14"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH15"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH16"},

	{"UL_CM1_CH1", NULL, "CM1_Enable"},
	{"UL_CM1_CH2", NULL, "CM1_Enable"},
	{"UL_CM1_CH3", NULL, "CM1_Enable"},
	{"UL_CM1_CH4", NULL, "CM1_Enable"},
	{"UL_CM1_CH5", NULL, "CM1_Enable"},
	{"UL_CM1_CH6", NULL, "CM1_Enable"},
	{"UL_CM1_CH7", NULL, "CM1_Enable"},
	{"UL_CM1_CH8", NULL, "CM1_Enable"},
	{"UL_CM1_CH9", NULL, "CM1_Enable"},
	{"UL_CM1_CH10", NULL, "CM1_Enable"},
	{"UL_CM1_CH11", NULL, "CM1_Enable"},
	{"UL_CM1_CH12", NULL, "CM1_Enable"},
	{"UL_CM1_CH13", NULL, "CM1_Enable"},
	{"UL_CM1_CH14", NULL, "CM1_Enable"},
	{"UL_CM1_CH15", NULL, "CM1_Enable"},
	{"UL_CM1_CH16", NULL, "CM1_Enable"},

	/* UL9 o36o37 <- ADDA */
	{"UL9_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL9_CH1", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL9_CH1", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL9_CH1", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL9_CH1", "ADDA_UL_CH5", "ADDA_CH56_UL_Mux"},
	{"UL9_CH1", "ADDA_UL_CH6", "ADDA_CH56_UL_Mux"},
	{"UL9_CH2", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL9_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL9_CH2", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL9_CH2", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL9_CH2", "ADDA_UL_CH5", "ADDA_CH56_UL_Mux"},
	{"UL9_CH2", "ADDA_UL_CH6", "ADDA_CH56_UL_Mux"},

	{"UL24", NULL, "UL24_CH1"},
	{"UL24", NULL, "UL24_CH2"},
	{"UL24_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL24_CH1", "I2SIN6_CH1", "I2SIN6"},
	{"UL24_CH2", "I2SIN6_CH2", "I2SIN6"},

	{"UL25", NULL, "UL25_CH1"},
	{"UL25", NULL, "UL25_CH2"},
	{"UL25_CH1", "I2SIN6_CH1", "I2SIN6"},
	{"UL25_CH2", "I2SIN6_CH2", "I2SIN6"},

	{"DL_TO_DSP", NULL, "Hostless_DSP_DL DL"},
	{"Hostless_DSP_DL DL", NULL, "DSP_DL"},

	{"DSP_DL", "DSP_DL0", "DL0"},
	{"DSP_DL", "DSP_DL2", "DL2"},
	{"DSP_DL", "DSP_DL1", "DL1"},
	{"DSP_DL", "DSP_DL6", "DL6"},
	{"DSP_DL", "DSP_DL3", "DL3"},
	{"DSP_DL", "DSP_DL_24CH", "DL_24CH"},

	{"HW_GAIN1_IN_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"HW_GAIN1_IN_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},

	{"UL_CM0", NULL, "UL_CM0_CH1"},
	{"UL_CM0", NULL, "UL_CM0_CH2"},
	{"UL_CM0", NULL, "UL_CM0_CH3"},
	{"UL_CM0", NULL, "UL_CM0_CH4"},
	{"UL_CM0", NULL, "UL_CM0_CH5"},
	{"UL_CM0", NULL, "UL_CM0_CH6"},
	{"UL_CM0", NULL, "UL_CM0_CH7"},
	{"UL_CM0", NULL, "UL_CM0_CH8"},
	{"UL_CM0_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM0_CH1", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM0_CH1", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM0_CH1", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL_CM0_CH1", "ADDA_UL_CH5", "ADDA_CH56_UL_Mux"},
	{"UL_CM0_CH1", "ADDA_UL_CH6", "ADDA_CH56_UL_Mux"},
	{"UL_CM0_CH2", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM0_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM0_CH2", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM0_CH2", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL_CM0_CH2", "ADDA_UL_CH5", "ADDA_CH56_UL_Mux"},
	{"UL_CM0_CH2", "ADDA_UL_CH6", "ADDA_CH56_UL_Mux"},
	{"UL_CM0_CH3", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM0_CH3", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM0_CH3", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM0_CH3", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL_CM0_CH4", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM0_CH4", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM0_CH4", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM0_CH4", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL_CM0_CH1", "HW_GAIN1_OUT_CH1", "HW Gain 1 Out"},
	{"UL_CM0_CH2", "HW_GAIN1_OUT_CH2", "HW Gain 1 Out"},
	{"UL_CM0_CH1", "HW_SRC_0_OUT_CH1", "HW_SRC_0_Out"},
	{"UL_CM0_CH2", "HW_SRC_0_OUT_CH2", "HW_SRC_0_Out"},
	{"UL_CM0_CH1", "HW_SRC_1_OUT_CH1", "HW_SRC_1_Out"},
	{"UL_CM0_CH2", "HW_SRC_1_OUT_CH2", "HW_SRC_1_Out"},

	{"UL_CM0_CH1", "I2SIN4_CH1", "I2SIN4"},
	{"UL_CM0_CH2", "I2SIN4_CH2", "I2SIN4"},
	{"UL_CM0_CH3", "I2SIN4_CH3", "I2SIN4"},
	{"UL_CM0_CH4", "I2SIN4_CH4", "I2SIN4"},

	{"UL_CM1", NULL, "UL_CM1_CH1"},
	{"UL_CM1", NULL, "UL_CM1_CH2"},
	{"UL_CM1", NULL, "UL_CM1_CH3"},
	{"UL_CM1", NULL, "UL_CM1_CH4"},
	{"UL_CM1", NULL, "UL_CM1_CH5"},
	{"UL_CM1", NULL, "UL_CM1_CH6"},
	{"UL_CM1", NULL, "UL_CM1_CH7"},
	{"UL_CM1", NULL, "UL_CM1_CH8"},
	{"UL_CM1", NULL, "UL_CM1_CH9"},
	{"UL_CM1", NULL, "UL_CM1_CH10"},
	{"UL_CM1", NULL, "UL_CM1_CH11"},
	{"UL_CM1", NULL, "UL_CM1_CH12"},
	{"UL_CM1", NULL, "UL_CM1_CH13"},
	{"UL_CM1", NULL, "UL_CM1_CH14"},
	{"UL_CM1", NULL, "UL_CM1_CH15"},
	{"UL_CM1", NULL, "UL_CM1_CH16"},
	{"UL_CM1_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM1_CH1", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM1_CH1", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH1", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH1", "ADDA_UL_CH5", "ADDA_CH56_UL_Mux"},
	{"UL_CM1_CH1", "ADDA_UL_CH6", "ADDA_CH56_UL_Mux"},
	{"UL_CM1_CH2", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM1_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM1_CH2", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH2", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH2", "ADDA_UL_CH5", "ADDA_CH56_UL_Mux"},
	{"UL_CM1_CH2", "ADDA_UL_CH6", "ADDA_CH56_UL_Mux"},
	{"UL_CM1_CH3", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM1_CH3", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM1_CH3", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH3", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH3", "ADDA_UL_CH5", "ADDA_CH56_UL_Mux"},
	{"UL_CM1_CH3", "ADDA_UL_CH6", "ADDA_CH56_UL_Mux"},
	{"UL_CM1_CH4", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM1_CH4", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM1_CH4", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH4", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH4", "ADDA_UL_CH5", "ADDA_CH56_UL_Mux"},
	{"UL_CM1_CH4", "ADDA_UL_CH6", "ADDA_CH56_UL_Mux"},
	{"UL_CM1_CH5", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM1_CH5", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM1_CH5", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH5", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH5", "ADDA_UL_CH5", "ADDA_CH56_UL_Mux"},
	{"UL_CM1_CH5", "ADDA_UL_CH6", "ADDA_CH56_UL_Mux"},
	{"UL_CM1_CH6", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM1_CH6", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM1_CH6", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH6", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH6", "ADDA_UL_CH5", "ADDA_CH56_UL_Mux"},
	{"UL_CM1_CH6", "ADDA_UL_CH6", "ADDA_CH56_UL_Mux"},
	{"UL_CM1_CH1", "HW_SRC_0_OUT_CH1", "HW_SRC_0_Out"},
	{"UL_CM1_CH2", "HW_SRC_0_OUT_CH2", "HW_SRC_0_Out"},

	{"UL_CM1_CH1", "I2SIN4_CH1", "I2SIN4"},
	{"UL_CM1_CH2", "I2SIN4_CH2", "I2SIN4"},
	{"UL_CM1_CH3", "I2SIN4_CH3", "I2SIN4"},
	{"UL_CM1_CH4", "I2SIN4_CH4", "I2SIN4"},

	{"DL6_VIRTUAL_OUTPUT", NULL, "Hostless_UL1 DL"},
	{"Hostless_UL1 DL", NULL, "DL6"},
};

static const struct mtk_base_memif_data memif_data[MT6989_MEMIF_NUM] = {
	[MT6989_MEMIF_DL0] = {
		.name = "DL0",
		.id = MT6989_MEMIF_DL0,
		.reg_ofs_base = AFE_DL0_BASE,
		.reg_ofs_cur = AFE_DL0_CUR,
		.reg_ofs_end = AFE_DL0_END,
		.reg_ofs_base_msb = AFE_DL0_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL0_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL0_END_MSB,
		.fs_reg = AFE_DL0_CON0,
		.fs_shift = DL0_SEL_FS_SFT,
		.fs_maskbit = DL0_SEL_FS_MASK,
		.mono_reg = AFE_DL0_CON0,
		.mono_shift = DL0_MONO_SFT,
		.enable_reg = AFE_DL0_CON0,
		.enable_shift = DL0_ON_SFT,
		.hd_reg = AFE_DL0_CON0,
		.hd_mask = DL0_HD_MODE_MASK,
		.hd_shift = DL0_HD_MODE_SFT,
		.hd_align_reg = AFE_DL0_CON0,
		.hd_align_mshift = DL0_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL0_CON0,
		.pbuf_mask = DL0_PBUF_SIZE_MASK,
		.pbuf_shift = DL0_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL0_CON0,
		.minlen_mask = DL0_MINLEN_MASK,
		.minlen_shift = DL0_MINLEN_SFT,
	},
	[MT6989_MEMIF_DL1] = {
		.name = "DL1",
		.id = MT6989_MEMIF_DL1,
		.reg_ofs_base = AFE_DL1_BASE,
		.reg_ofs_cur = AFE_DL1_CUR,
		.reg_ofs_end = AFE_DL1_END,
		.reg_ofs_base_msb = AFE_DL1_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL1_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL1_END_MSB,
		.fs_reg = AFE_DL1_CON0,
		.fs_shift = DL1_SEL_FS_SFT,
		.fs_maskbit = DL1_SEL_FS_MASK,
		.mono_reg = AFE_DL1_CON0,
		.mono_shift = DL1_MONO_SFT,
		.enable_reg = AFE_DL1_CON0,
		.enable_shift = DL1_ON_SFT,
		.hd_reg = AFE_DL1_CON0,
		.hd_mask = DL1_HD_MODE_MASK,
		.hd_shift = DL1_HD_MODE_SFT,
		.hd_align_reg = AFE_DL1_CON0,
		.hd_align_mshift = DL1_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL1_CON0,
		.pbuf_mask = DL1_PBUF_SIZE_MASK,
		.pbuf_shift = DL1_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL1_CON0,
		.minlen_mask = DL1_MINLEN_MASK,
		.minlen_shift = DL1_MINLEN_SFT,
	},
	[MT6989_MEMIF_DL2] = {
		.name = "DL2",
		.id = MT6989_MEMIF_DL2,
		.reg_ofs_base = AFE_DL2_BASE,
		.reg_ofs_cur = AFE_DL2_CUR,
		.reg_ofs_end = AFE_DL2_END,
		.reg_ofs_base_msb = AFE_DL2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL2_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL2_END_MSB,
		.fs_reg = AFE_DL2_CON0,
		.fs_shift = DL2_SEL_FS_SFT,
		.fs_maskbit = DL2_SEL_FS_MASK,
		.mono_reg = AFE_DL2_CON0,
		.mono_shift = DL2_MONO_SFT,
		.enable_reg = AFE_DL2_CON0,
		.enable_shift = DL2_ON_SFT,
		.hd_reg = AFE_DL2_CON0,
		.hd_mask = DL2_HD_MODE_MASK,
		.hd_shift = DL2_HD_MODE_SFT,
		.hd_align_reg = AFE_DL2_CON0,
		.hd_align_mshift = DL2_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL2_CON0,
		.pbuf_mask = DL2_PBUF_SIZE_MASK,
		.pbuf_shift = DL2_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL2_CON0,
		.minlen_mask = DL2_MINLEN_MASK,
		.minlen_shift = DL2_MINLEN_SFT,
	},
	[MT6989_MEMIF_DL3] = {
		.name = "DL3",
		.id = MT6989_MEMIF_DL3,
		.reg_ofs_base = AFE_DL3_BASE,
		.reg_ofs_cur = AFE_DL3_CUR,
		.reg_ofs_end = AFE_DL3_END,
		.reg_ofs_base_msb = AFE_DL3_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL3_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL3_END_MSB,
		.fs_reg = AFE_DL3_CON0,
		.fs_shift = DL3_SEL_FS_SFT,
		.fs_maskbit = DL3_SEL_FS_MASK,
		.mono_reg = AFE_DL3_CON0,
		.mono_shift = DL3_MONO_SFT,
		.enable_reg = AFE_DL3_CON0,
		.enable_shift = DL3_ON_SFT,
		.hd_reg = AFE_DL3_CON0,
		.hd_mask = DL3_HD_MODE_MASK,
		.hd_shift = DL3_HD_MODE_SFT,
		.hd_align_reg = AFE_DL3_CON0,
		.hd_align_mshift = DL3_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL3_CON0,
		.pbuf_mask = DL3_PBUF_SIZE_MASK,
		.pbuf_shift = DL3_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL3_CON0,
		.minlen_mask = DL3_MINLEN_MASK,
		.minlen_shift = DL3_MINLEN_SFT,
	},
	[MT6989_MEMIF_DL4] = {
		.name = "DL4",
		.id = MT6989_MEMIF_DL4,
		.reg_ofs_base = AFE_DL4_BASE,
		.reg_ofs_cur = AFE_DL4_CUR,
		.reg_ofs_end = AFE_DL4_END,
		.reg_ofs_base_msb = AFE_DL4_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL4_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL4_END_MSB,
		.fs_reg = AFE_DL4_CON0,
		.fs_shift = DL4_SEL_FS_SFT,
		.fs_maskbit = DL4_SEL_FS_MASK,
		.mono_reg = AFE_DL4_CON0,
		.mono_shift = DL4_MONO_SFT,
		.enable_reg = AFE_DL4_CON0,
		.enable_shift = DL4_ON_SFT,
		.hd_reg = AFE_DL4_CON0,
		.hd_mask = DL4_HD_MODE_MASK,
		.hd_shift = DL4_HD_MODE_SFT,
		.hd_align_reg = AFE_DL4_CON0,
		.hd_align_mshift = DL4_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL4_CON0,
		.pbuf_mask = DL4_PBUF_SIZE_MASK,
		.pbuf_shift = DL4_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL4_CON0,
		.minlen_mask = DL4_MINLEN_MASK,
		.minlen_shift = DL4_MINLEN_SFT,
	},
	[MT6989_MEMIF_DL5] = {
		.name = "DL5",
		.id = MT6989_MEMIF_DL5,
		.reg_ofs_base = AFE_DL5_BASE,
		.reg_ofs_cur = AFE_DL5_CUR,
		.reg_ofs_end = AFE_DL5_END,
		.reg_ofs_base_msb = AFE_DL5_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL5_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL5_END_MSB,
		.fs_reg = AFE_DL5_CON0,
		.fs_shift = DL5_SEL_FS_SFT,
		.fs_maskbit = DL5_SEL_FS_MASK,
		.mono_reg = AFE_DL5_CON0,
		.mono_shift = DL5_MONO_SFT,
		.enable_reg = AFE_DL5_CON0,
		.enable_shift = DL5_ON_SFT,
		.hd_reg = AFE_DL5_CON0,
		.hd_mask = DL5_HD_MODE_MASK,
		.hd_shift = DL5_HD_MODE_SFT,
		.hd_align_reg = AFE_DL5_CON0,
		.hd_align_mshift = DL5_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL5_CON0,
		.pbuf_mask = DL5_PBUF_SIZE_MASK,
		.pbuf_shift = DL5_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL5_CON0,
		.minlen_mask = DL5_MINLEN_MASK,
		.minlen_shift = DL5_MINLEN_SFT,
	},
	[MT6989_MEMIF_DL6] = {
		.name = "DL6",
		.id = MT6989_MEMIF_DL6,
		.reg_ofs_base = AFE_DL6_BASE,
		.reg_ofs_cur = AFE_DL6_CUR,
		.reg_ofs_end = AFE_DL6_END,
		.reg_ofs_base_msb = AFE_DL6_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL6_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL6_END_MSB,
		.fs_reg = AFE_DL6_CON0,
		.fs_shift = DL6_SEL_FS_SFT,
		.fs_maskbit = DL6_SEL_FS_MASK,
		.mono_reg = AFE_DL6_CON0,
		.mono_shift = DL6_MONO_SFT,
		.enable_reg = AFE_DL6_CON0,
		.enable_shift = DL6_ON_SFT,
		.hd_reg = AFE_DL6_CON0,
		.hd_mask = DL6_HD_MODE_MASK,
		.hd_shift = DL6_HD_MODE_SFT,
		.hd_align_reg = AFE_DL6_CON0,
		.hd_align_mshift = DL6_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL6_CON0,
		.pbuf_mask = DL6_PBUF_SIZE_MASK,
		.pbuf_shift = DL6_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL6_CON0,
		.minlen_mask = DL6_MINLEN_MASK,
		.minlen_shift = DL6_MINLEN_SFT,
	},
	[MT6989_MEMIF_DL7] = {
		.name = "DL7",
		.id = MT6989_MEMIF_DL7,
		.reg_ofs_base = AFE_DL7_BASE,
		.reg_ofs_cur = AFE_DL7_CUR,
		.reg_ofs_end = AFE_DL7_END,
		.reg_ofs_base_msb = AFE_DL7_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL7_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL7_END_MSB,
		.fs_reg = AFE_DL7_CON0,
		.fs_shift = DL7_SEL_FS_SFT,
		.fs_maskbit = DL7_SEL_FS_MASK,
		.mono_reg = AFE_DL7_CON0,
		.mono_shift = DL7_MONO_SFT,
		.enable_reg = AFE_DL7_CON0,
		.enable_shift = DL7_ON_SFT,
		.hd_reg = AFE_DL7_CON0,
		.hd_mask = DL7_HD_MODE_MASK,
		.hd_shift = DL7_HD_MODE_SFT,
		.hd_align_reg = AFE_DL7_CON0,
		.hd_align_mshift = DL7_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL7_CON0,
		.pbuf_mask = DL7_PBUF_SIZE_MASK,
		.pbuf_shift = DL7_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL7_CON0,
		.minlen_mask = DL7_MINLEN_MASK,
		.minlen_shift = DL7_MINLEN_SFT,
	},
	[MT6989_MEMIF_DL8] = {
		.name = "DL8",
		.id = MT6989_MEMIF_DL8,
		.reg_ofs_base = AFE_DL8_BASE,
		.reg_ofs_cur = AFE_DL8_CUR,
		.reg_ofs_end = AFE_DL8_END,
		.reg_ofs_base_msb = AFE_DL8_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL8_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL8_END_MSB,
		.fs_reg = AFE_DL8_CON0,
		.fs_shift = DL8_SEL_FS_SFT,
		.fs_maskbit = DL8_SEL_FS_MASK,
		.mono_reg = AFE_DL8_CON0,
		.mono_shift = DL8_MONO_SFT,
		.enable_reg = AFE_DL8_CON0,
		.enable_shift = DL8_ON_SFT,
		.hd_reg = AFE_DL8_CON0,
		.hd_mask = DL8_HD_MODE_MASK,
		.hd_shift = DL8_HD_MODE_SFT,
		.hd_align_reg = AFE_DL8_CON0,
		.hd_align_mshift = DL8_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL8_CON0,
		.pbuf_mask = DL8_PBUF_SIZE_MASK,
		.pbuf_shift = DL8_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL8_CON0,
		.minlen_mask = DL8_MINLEN_MASK,
		.minlen_shift = DL8_MINLEN_SFT,
	},
	[MT6989_MEMIF_DL23] = {
		.name = "DL23",
		.id = MT6989_MEMIF_DL23,
		.reg_ofs_base = AFE_DL23_BASE,
		.reg_ofs_cur = AFE_DL23_CUR,
		.reg_ofs_end = AFE_DL23_END,
		.reg_ofs_base_msb = AFE_DL23_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL23_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL23_END_MSB,
		.fs_reg = AFE_DL23_CON0,
		.fs_shift = DL23_SEL_FS_SFT,
		.fs_maskbit = DL23_SEL_FS_MASK,
		.mono_reg = AFE_DL23_CON0,
		.mono_shift = DL23_MONO_SFT,
		.enable_reg = AFE_DL23_CON0,
		.enable_shift = DL23_ON_SFT,
		.hd_reg = AFE_DL23_CON0,
		.hd_mask = DL23_HD_MODE_MASK,
		.hd_shift = DL23_HD_MODE_SFT,
		.hd_align_reg = AFE_DL23_CON0,
		.hd_align_mshift = DL23_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL23_CON0,
		.pbuf_mask = DL23_PBUF_SIZE_MASK,
		.pbuf_shift = DL23_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL23_CON0,
		.minlen_mask = DL23_MINLEN_MASK,
		.minlen_shift = DL23_MINLEN_SFT,
	},
	[MT6989_MEMIF_DL24] = {
		.name = "DL24",
		.id = MT6989_MEMIF_DL24,
		.reg_ofs_base = AFE_DL24_BASE,
		.reg_ofs_cur = AFE_DL24_CUR,
		.reg_ofs_end = AFE_DL24_END,
		.reg_ofs_base_msb = AFE_DL24_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL24_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL24_END_MSB,
		.fs_reg = AFE_DL24_CON0,
		.fs_shift = DL24_SEL_FS_SFT,
		.fs_maskbit = DL24_SEL_FS_MASK,
		.mono_reg = AFE_DL24_CON0,
		.mono_shift = DL24_MONO_SFT,
		.enable_reg = AFE_DL24_CON0,
		.enable_shift = DL24_ON_SFT,
		.hd_reg = AFE_DL24_CON0,
		.hd_mask = DL24_HD_MODE_MASK,
		.hd_shift = DL24_HD_MODE_SFT,
		.hd_align_reg = AFE_DL24_CON0,
		.hd_align_mshift = DL24_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL24_CON0,
		.pbuf_mask = DL24_PBUF_SIZE_MASK,
		.pbuf_shift = DL24_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL24_CON0,
		.minlen_mask = DL24_MINLEN_MASK,
		.minlen_shift = DL24_MINLEN_SFT,
	},
	[MT6989_MEMIF_DL25] = {
		.name = "DL25",
		.id = MT6989_MEMIF_DL25,
		.reg_ofs_base = AFE_DL25_BASE,
		.reg_ofs_cur = AFE_DL25_CUR,
		.reg_ofs_end = AFE_DL25_END,
		.reg_ofs_base_msb = AFE_DL25_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL25_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL25_END_MSB,
		.fs_reg = AFE_DL25_CON0,
		.fs_shift = DL25_SEL_FS_SFT,
		.fs_maskbit = DL25_SEL_FS_MASK,
		.mono_reg = AFE_DL25_CON0,
		.mono_shift = DL25_MONO_SFT,
		.enable_reg = AFE_DL25_CON0,
		.enable_shift = DL25_ON_SFT,
		.hd_reg = AFE_DL25_CON0,
		.hd_mask = DL25_HD_MODE_MASK,
		.hd_shift = DL25_HD_MODE_SFT,
		.hd_align_reg = AFE_DL25_CON0,
		.hd_align_mshift = DL25_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL25_CON0,
		.pbuf_mask = DL25_PBUF_SIZE_MASK,
		.pbuf_shift = DL25_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL25_CON0,
		.minlen_mask = DL25_MINLEN_MASK,
		.minlen_shift = DL25_MINLEN_SFT,
	},
	[MT6989_MEMIF_DL_24CH] = {
		.name = "DL_24CH",
		.id = MT6989_MEMIF_DL_24CH,
		.reg_ofs_base = AFE_DL_24CH_BASE,
		.reg_ofs_cur = AFE_DL_24CH_CUR,
		.reg_ofs_end = AFE_DL_24CH_END,
		.reg_ofs_base_msb = AFE_DL_24CH_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL_24CH_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL_24CH_END_MSB,
		.fs_reg = AFE_DL_24CH_CON0,
		.fs_shift = DL_24CH_SEL_FS_SFT,
		.fs_maskbit = DL_24CH_SEL_FS_MASK,
		.mono_reg = -1,
		.mono_shift = -1,
		.enable_reg = AFE_DL_24CH_CON0,
		.enable_shift = DL_24CH_ON_SFT,
		.hd_reg = AFE_DL_24CH_CON0,
		.hd_mask = DL_24CH_HD_AUDIO_ON_MASK,
		.hd_shift = DL_24CH_HD_AUDIO_ON_SFT,
		.hd_align_reg = AFE_DL_24CH_CON1,
		.hd_align_mshift = DL_24CH_HALIGN_SFT,
		.hd_msb_shift = DL_24CH_HD_MSB_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL_24CH_CON0,
		.pbuf_mask = DL_24CH_PBUF_SIZE_MASK,
		.pbuf_shift = DL_24CH_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL_24CH_CON0,
		.minlen_mask = DL_24CH_MINLEN_MASK,
		.minlen_shift = DL_24CH_MINLEN_SFT,
		.ch_num_reg = AFE_DL_24CH_CON0,
		.ch_num_maskbit = DL_24CH_NUM_MASK,
		.ch_num_shift = DL_24CH_NUM_SFT,
	},
	[MT6989_MEMIF_VUL0] = {
		.name = "VUL0",
		.id = MT6989_MEMIF_VUL0,
		.reg_ofs_base = AFE_VUL0_BASE,
		.reg_ofs_cur = AFE_VUL0_CUR,
		.reg_ofs_end = AFE_VUL0_END,
		.reg_ofs_base_msb = AFE_VUL0_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL0_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL0_END_MSB,
		.fs_reg = AFE_VUL0_CON0,
		.fs_shift = VUL0_SEL_FS_SFT,
		.fs_maskbit = VUL0_SEL_FS_MASK,
		.mono_reg = AFE_VUL0_CON0,
		.mono_shift = VUL0_MONO_SFT,
		.enable_reg = AFE_VUL0_CON0,
		.enable_shift = VUL0_ON_SFT,
		.hd_reg = AFE_VUL0_CON0,
		.hd_mask = VUL0_HD_MODE_MASK,
		.hd_shift = VUL0_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL0_CON0,
		.hd_align_mshift = VUL0_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6989_MEMIF_VUL1] = {
		.name = "VUL1",
		.id = MT6989_MEMIF_VUL1,
		.reg_ofs_base = AFE_VUL1_BASE,
		.reg_ofs_cur = AFE_VUL1_CUR,
		.reg_ofs_end = AFE_VUL1_END,
		.reg_ofs_base_msb = AFE_VUL1_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL1_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL1_END_MSB,
		.fs_reg = AFE_VUL1_CON0,
		.fs_shift = VUL1_SEL_FS_SFT,
		.fs_maskbit = VUL1_SEL_FS_MASK,
		.mono_reg = AFE_VUL1_CON0,
		.mono_shift = VUL1_MONO_SFT,
		.enable_reg = AFE_VUL1_CON0,
		.enable_shift = VUL1_ON_SFT,
		.hd_reg = AFE_VUL1_CON0,
		.hd_mask = VUL1_HD_MODE_MASK,
		.hd_shift = VUL1_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL1_CON0,
		.hd_align_mshift = VUL1_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6989_MEMIF_VUL2] = {
		.name = "VUL2",
		.id = MT6989_MEMIF_VUL2,
		.reg_ofs_base = AFE_VUL2_BASE,
		.reg_ofs_cur = AFE_VUL2_CUR,
		.reg_ofs_end = AFE_VUL2_END,
		.reg_ofs_base_msb = AFE_VUL2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL2_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL2_END_MSB,
		.fs_reg = AFE_VUL2_CON0,
		.fs_shift = VUL2_SEL_FS_SFT,
		.fs_maskbit = VUL2_SEL_FS_MASK,
		.mono_reg = AFE_VUL2_CON0,
		.mono_shift = VUL2_MONO_SFT,
		.enable_reg = AFE_VUL2_CON0,
		.enable_shift = VUL2_ON_SFT,
		.hd_reg = AFE_VUL2_CON0,
		.hd_mask = VUL2_HD_MODE_MASK,
		.hd_shift = VUL2_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL2_CON0,
		.hd_align_mshift = VUL2_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6989_MEMIF_VUL3] = {
		.name = "VUL3",
		.id = MT6989_MEMIF_VUL3,
		.reg_ofs_base = AFE_VUL3_BASE,
		.reg_ofs_cur = AFE_VUL3_CUR,
		.reg_ofs_end = AFE_VUL3_END,
		.reg_ofs_base_msb = AFE_VUL3_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL3_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL3_END_MSB,
		.fs_reg = AFE_VUL3_CON0,
		.fs_shift = VUL3_SEL_FS_SFT,
		.fs_maskbit = VUL3_SEL_FS_MASK,
		.mono_reg = AFE_VUL3_CON0,
		.mono_shift = VUL3_MONO_SFT,
		.enable_reg = AFE_VUL3_CON0,
		.enable_shift = VUL3_ON_SFT,
		.hd_reg = AFE_VUL3_CON0,
		.hd_mask = VUL3_HD_MODE_MASK,
		.hd_shift = VUL3_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL3_CON0,
		.hd_align_mshift = VUL3_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6989_MEMIF_VUL4] = {
		.name = "VUL4",
		.id = MT6989_MEMIF_VUL4,
		.reg_ofs_base = AFE_VUL4_BASE,
		.reg_ofs_cur = AFE_VUL4_CUR,
		.reg_ofs_end = AFE_VUL4_END,
		.reg_ofs_base_msb = AFE_VUL4_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL4_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL4_END_MSB,
		.fs_reg = AFE_VUL4_CON0,
		.fs_shift = VUL4_SEL_FS_SFT,
		.fs_maskbit = VUL4_SEL_FS_MASK,
		.mono_reg = AFE_VUL4_CON0,
		.mono_shift = VUL4_MONO_SFT,
		.enable_reg = AFE_VUL4_CON0,
		.enable_shift = VUL4_ON_SFT,
		.hd_reg = AFE_VUL4_CON0,
		.hd_mask = VUL4_HD_MODE_MASK,
		.hd_shift = VUL4_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL4_CON0,
		.hd_align_mshift = VUL4_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6989_MEMIF_VUL5] = {
		.name = "VUL5",
		.id = MT6989_MEMIF_VUL5,
		.reg_ofs_base = AFE_VUL5_BASE,
		.reg_ofs_cur = AFE_VUL5_CUR,
		.reg_ofs_end = AFE_VUL5_END,
		.reg_ofs_base_msb = AFE_VUL5_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL5_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL5_END_MSB,
		.fs_reg = AFE_VUL5_CON0,
		.fs_shift = VUL5_SEL_FS_SFT,
		.fs_maskbit = VUL5_SEL_FS_MASK,
		.mono_reg = AFE_VUL5_CON0,
		.mono_shift = VUL5_MONO_SFT,
		.enable_reg = AFE_VUL5_CON0,
		.enable_shift = VUL5_ON_SFT,
		.hd_reg = AFE_VUL5_CON0,
		.hd_mask = VUL5_HD_MODE_MASK,
		.hd_shift = VUL5_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL5_CON0,
		.hd_align_mshift = VUL5_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6989_MEMIF_VUL6] = {
		.name = "VUL6",
		.id = MT6989_MEMIF_VUL6,
		.reg_ofs_base = AFE_VUL6_BASE,
		.reg_ofs_cur = AFE_VUL6_CUR,
		.reg_ofs_end = AFE_VUL6_END,
		.reg_ofs_base_msb = AFE_VUL6_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL6_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL6_END_MSB,
		.fs_reg = AFE_VUL6_CON0,
		.fs_shift = VUL6_SEL_FS_SFT,
		.fs_maskbit = VUL6_SEL_FS_MASK,
		.mono_reg = AFE_VUL6_CON0,
		.mono_shift = VUL6_MONO_SFT,
		.enable_reg = AFE_VUL6_CON0,
		.enable_shift = VUL6_ON_SFT,
		.hd_reg = AFE_VUL6_CON0,
		.hd_mask = VUL6_HD_MODE_MASK,
		.hd_shift = VUL6_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL6_CON0,
		.hd_align_mshift = VUL6_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6989_MEMIF_VUL7] = {
		.name = "VUL7",
		.id = MT6989_MEMIF_VUL7,
		.reg_ofs_base = AFE_VUL7_BASE,
		.reg_ofs_cur = AFE_VUL7_CUR,
		.reg_ofs_end = AFE_VUL7_END,
		.reg_ofs_base_msb = AFE_VUL7_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL7_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL7_END_MSB,
		.fs_reg = AFE_VUL7_CON0,
		.fs_shift = VUL7_SEL_FS_SFT,
		.fs_maskbit = VUL7_SEL_FS_MASK,
		.mono_reg = AFE_VUL7_CON0,
		.mono_shift = VUL7_MONO_SFT,
		.enable_reg = AFE_VUL7_CON0,
		.enable_shift = VUL7_ON_SFT,
		.hd_reg = AFE_VUL7_CON0,
		.hd_mask = VUL7_HD_MODE_MASK,
		.hd_shift = VUL7_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL7_CON0,
		.hd_align_mshift = VUL7_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6989_MEMIF_VUL8] = {
		.name = "VUL8",
		.id = MT6989_MEMIF_VUL8,
		.reg_ofs_base = AFE_VUL8_BASE,
		.reg_ofs_cur = AFE_VUL8_CUR,
		.reg_ofs_end = AFE_VUL8_END,
		.reg_ofs_base_msb = AFE_VUL8_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL8_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL8_END_MSB,
		.fs_reg = AFE_VUL8_CON0,
		.fs_shift = VUL8_SEL_FS_SFT,
		.fs_maskbit = VUL8_SEL_FS_MASK,
		.mono_reg = AFE_VUL8_CON0,
		.mono_shift = VUL8_MONO_SFT,
		.enable_reg = AFE_VUL8_CON0,
		.enable_shift = VUL8_ON_SFT,
		.hd_reg = AFE_VUL8_CON0,
		.hd_mask = VUL8_HD_MODE_MASK,
		.hd_shift = VUL8_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL8_CON0,
		.hd_align_mshift = VUL8_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6989_MEMIF_VUL9] = {
		.name = "VUL9",
		.id = MT6989_MEMIF_VUL9,
		.reg_ofs_base = AFE_VUL9_BASE,
		.reg_ofs_cur = AFE_VUL9_CUR,
		.reg_ofs_end = AFE_VUL9_END,
		.reg_ofs_base_msb = AFE_VUL9_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL9_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL9_END_MSB,
		.fs_reg = AFE_VUL9_CON0,
		.fs_shift = VUL9_SEL_FS_SFT,
		.fs_maskbit = VUL9_SEL_FS_MASK,
		.mono_reg = AFE_VUL9_CON0,
		.mono_shift = VUL9_MONO_SFT,
		.enable_reg = AFE_VUL9_CON0,
		.enable_shift = VUL9_ON_SFT,
		.hd_reg = AFE_VUL9_CON0,
		.hd_mask = VUL9_HD_MODE_MASK,
		.hd_shift = VUL9_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL9_CON0,
		.hd_align_mshift = VUL9_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6989_MEMIF_VUL10] = {
		.name = "VUL10",
		.id = MT6989_MEMIF_VUL10,
		.reg_ofs_base = AFE_VUL10_BASE,
		.reg_ofs_cur = AFE_VUL10_CUR,
		.reg_ofs_end = AFE_VUL10_END,
		.reg_ofs_base_msb = AFE_VUL10_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL10_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL10_END_MSB,
		.fs_reg = AFE_VUL10_CON0,
		.fs_shift = VUL10_SEL_FS_SFT,
		.fs_maskbit = VUL10_SEL_FS_MASK,
		.mono_reg = AFE_VUL10_CON0,
		.mono_shift = VUL10_MONO_SFT,
		.enable_reg = AFE_VUL10_CON0,
		.enable_shift = VUL10_ON_SFT,
		.hd_reg = AFE_VUL10_CON0,
		.hd_mask = VUL10_HD_MODE_MASK,
		.hd_shift = VUL10_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL10_CON0,
		.hd_align_mshift = VUL10_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6989_MEMIF_VUL24] = {
		.name = "VUL24",
		.id = MT6989_MEMIF_VUL24,
		.reg_ofs_base = AFE_VUL24_BASE,
		.reg_ofs_cur = AFE_VUL24_CUR,
		.reg_ofs_end = AFE_VUL24_END,
		.reg_ofs_base_msb = AFE_VUL24_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL24_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL24_END_MSB,
		.fs_reg = AFE_VUL24_CON0,
		.fs_shift = VUL24_SEL_FS_SFT,
		.fs_maskbit = VUL24_SEL_FS_MASK,
		.mono_reg = AFE_VUL24_CON0,
		.mono_shift = VUL24_MONO_SFT,
		.enable_reg = AFE_VUL24_CON0,
		.enable_shift = VUL24_ON_SFT,
		.hd_reg = AFE_VUL24_CON0,
		.hd_mask = VUL24_HD_MODE_MASK,
		.hd_shift = VUL24_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL24_CON0,
		.hd_align_mshift = VUL24_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6989_MEMIF_VUL25] = {
		.name = "VUL25",
		.id = MT6989_MEMIF_VUL25,
		.reg_ofs_base = AFE_VUL25_BASE,
		.reg_ofs_cur = AFE_VUL25_CUR,
		.reg_ofs_end = AFE_VUL25_END,
		.reg_ofs_base_msb = AFE_VUL25_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL25_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL25_END_MSB,
		.fs_reg = AFE_VUL25_CON0,
		.fs_shift = VUL25_SEL_FS_SFT,
		.fs_maskbit = VUL25_SEL_FS_MASK,
		.mono_reg = AFE_VUL25_CON0,
		.mono_shift = VUL25_MONO_SFT,
		.enable_reg = AFE_VUL25_CON0,
		.enable_shift = VUL25_ON_SFT,
		.hd_reg = AFE_VUL25_CON0,
		.hd_mask = VUL25_HD_MODE_MASK,
		.hd_shift = VUL25_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL25_CON0,
		.hd_align_mshift = VUL25_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6989_MEMIF_VUL_CM0] = {
		.name = "VUL_CM0",
		.id = MT6989_MEMIF_VUL_CM0,
		.reg_ofs_base = AFE_VUL_CM0_BASE,
		.reg_ofs_cur = AFE_VUL_CM0_CUR,
		.reg_ofs_end = AFE_VUL_CM0_END,
		.reg_ofs_base_msb = AFE_VUL_CM0_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL_CM0_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL_CM0_END_MSB,
		//.fs_reg = AFE_CM0_CON0,
		//.fs_shift = AFE_CM0_1X_EN_SEL_FS_SFT,
		//.fs_maskbit = AFE_CM0_1X_EN_SEL_FS_MASK,
		//.mono_reg = AFE_VUL_CM0_CON0,
		//.mono_shift = VUL_CM0_MONO_SFT,
		.enable_reg = AFE_VUL_CM0_CON0,
		.enable_shift = VUL_CM0_ON_SFT,
		.hd_reg = AFE_VUL_CM0_CON0,
		.hd_mask = VUL_CM0_HD_MODE_MASK,
		.hd_shift = VUL_CM0_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL_CM0_CON0,
		.hd_align_mshift = VUL_CM0_HALIGN_SFT,
		.hd_msb_shift = VUL_CM0_HD_MSB_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6989_MEMIF_VUL_CM1] = {
		.name = "VUL_CM1",
		.id = MT6989_MEMIF_VUL_CM1,
		.reg_ofs_base = AFE_VUL_CM1_BASE,
		.reg_ofs_cur = AFE_VUL_CM1_CUR,
		.reg_ofs_end = AFE_VUL_CM1_END,
		.reg_ofs_base_msb = AFE_VUL_CM1_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL_CM1_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL_CM1_END_MSB,
		//.fs_reg = AFE_CM1_CON0,
		//.fs_shift = AFE_CM1_1X_EN_SEL_FS_SFT,
		//.fs_maskbit = AFE_CM1_1X_EN_SEL_FS_MASK,
		//.mono_reg = AFE_VUL_CM1_CON0,
		//.mono_shift = VUL_CM1_MONO_SFT,
		.enable_reg = AFE_VUL_CM1_CON0,
		.enable_shift = VUL_CM1_ON_SFT,
		.hd_reg = AFE_VUL_CM1_CON0,
		.hd_mask = VUL_CM1_HD_MODE_MASK,
		.hd_shift = VUL_CM1_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL_CM1_CON0,
		.hd_align_mshift = VUL_CM1_HALIGN_SFT,
		.hd_msb_shift = VUL_CM1_HD_MSB_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6989_MEMIF_ETDM_IN0] = {
		.name = "ETDM_IN0",
		.id = MT6989_MEMIF_ETDM_IN0,
		.reg_ofs_base = AFE_ETDM_IN0_BASE,
		.reg_ofs_cur = AFE_ETDM_IN0_CUR,
		.reg_ofs_end = AFE_ETDM_IN0_END,
		.reg_ofs_base_msb = AFE_ETDM_IN0_BASE_MSB,
		.reg_ofs_cur_msb = AFE_ETDM_IN0_CUR_MSB,
		.reg_ofs_end_msb = AFE_ETDM_IN0_END_MSB,
		.fs_reg = ETDM_IN0_CON3,
		.fs_shift = REG_FS_TIMING_SEL_SFT,
		.fs_maskbit = REG_FS_TIMING_SEL_MASK,
		//.mono_reg = ,
		//.mono_shift = ,
		.enable_reg = AFE_ETDM_IN0_CON0,
		.enable_shift = ETDM_IN0_ON_SFT,
		.hd_reg = AFE_ETDM_IN0_CON0,
		.hd_mask = ETDM_IN0_HD_MODE_MASK,
		.hd_shift = ETDM_IN0_HD_MODE_SFT,
		.hd_align_reg = AFE_ETDM_IN0_CON0,
		.hd_align_mshift = ETDM_IN0_HALIGN_SFT,
		.hd_msb_shift = ETDM_IN0_HD_MSB_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6989_MEMIF_ETDM_IN1] = {
		.name = "ETDM_IN1",
		.id = MT6989_MEMIF_ETDM_IN1,
		.reg_ofs_base = AFE_ETDM_IN1_BASE,
		.reg_ofs_cur = AFE_ETDM_IN1_CUR,
		.reg_ofs_end = AFE_ETDM_IN1_END,
		.reg_ofs_base_msb = AFE_ETDM_IN1_BASE_MSB,
		.reg_ofs_cur_msb = AFE_ETDM_IN1_CUR_MSB,
		.reg_ofs_end_msb = AFE_ETDM_IN1_END_MSB,
		.fs_reg = ETDM_IN1_CON3,
		.fs_shift = REG_FS_TIMING_SEL_SFT,
		.fs_maskbit = REG_FS_TIMING_SEL_MASK,
		//.mono_reg = ,
		//.mono_shift = ,
		.enable_reg = AFE_ETDM_IN1_CON0,
		.enable_shift = ETDM_IN1_ON_SFT,
		.hd_reg = AFE_ETDM_IN1_CON0,
		.hd_mask = ETDM_IN1_HD_MODE_MASK,
		.hd_shift = ETDM_IN1_HD_MODE_SFT,
		.hd_align_reg = AFE_ETDM_IN1_CON0,
		.hd_align_mshift = ETDM_IN1_HALIGN_SFT,
		.hd_msb_shift = ETDM_IN1_HD_MSB_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6989_MEMIF_ETDM_IN2] = {
		.name = "ETDM_IN2",
		.id = MT6989_MEMIF_ETDM_IN2,
		.reg_ofs_base = AFE_ETDM_IN2_BASE,
		.reg_ofs_cur = AFE_ETDM_IN2_CUR,
		.reg_ofs_end = AFE_ETDM_IN2_END,
		.reg_ofs_base_msb = AFE_ETDM_IN2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_ETDM_IN2_CUR_MSB,
		.reg_ofs_end_msb = AFE_ETDM_IN2_END_MSB,
		.fs_reg = ETDM_IN2_CON3,
		.fs_shift = REG_FS_TIMING_SEL_SFT,
		.fs_maskbit = REG_FS_TIMING_SEL_MASK,
		//.mono_reg = ,
		//.mono_shift = ,
		.enable_reg = AFE_ETDM_IN2_CON0,
		.enable_shift = ETDM_IN2_ON_SFT,
		.hd_reg = AFE_ETDM_IN2_CON0,
		.hd_mask = ETDM_IN2_HD_MODE_MASK,
		.hd_shift = ETDM_IN2_HD_MODE_SFT,
		.hd_align_reg = AFE_ETDM_IN2_CON0,
		.hd_align_mshift = ETDM_IN2_HALIGN_SFT,
		.hd_msb_shift = ETDM_IN2_HD_MSB_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6989_MEMIF_ETDM_IN4] = {
		.name = "ETDM_IN4",
		.id = MT6989_MEMIF_ETDM_IN4,
		.reg_ofs_base = AFE_ETDM_IN4_BASE,
		.reg_ofs_cur = AFE_ETDM_IN4_CUR,
		.reg_ofs_end = AFE_ETDM_IN4_END,
		.reg_ofs_base_msb = AFE_ETDM_IN4_BASE_MSB,
		.reg_ofs_cur_msb = AFE_ETDM_IN4_CUR_MSB,
		.reg_ofs_end_msb = AFE_ETDM_IN4_END_MSB,
		.fs_reg = ETDM_IN4_CON3,
		.fs_shift = REG_FS_TIMING_SEL_SFT,
		.fs_maskbit = REG_FS_TIMING_SEL_MASK,
		//.mono_reg = ,
		//.mono_shift = ,
		.enable_reg = AFE_ETDM_IN4_CON0,
		.enable_shift = ETDM_IN4_ON_SFT,
		.hd_reg = AFE_ETDM_IN4_CON0,
		.hd_mask = ETDM_IN4_HD_MODE_MASK,
		.hd_shift = ETDM_IN4_HD_MODE_SFT,
		.hd_align_reg = AFE_ETDM_IN4_CON0,
		.hd_align_mshift = ETDM_IN4_HALIGN_SFT,
		.hd_msb_shift = ETDM_IN4_HD_MSB_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6989_MEMIF_ETDM_IN6] = {
		.name = "ETDM_IN6",
		.id = MT6989_MEMIF_ETDM_IN6,
		.reg_ofs_base = AFE_ETDM_IN6_BASE,
		.reg_ofs_cur = AFE_ETDM_IN6_CUR,
		.reg_ofs_end = AFE_ETDM_IN6_END,
		.reg_ofs_base_msb = AFE_ETDM_IN6_BASE_MSB,
		.reg_ofs_cur_msb = AFE_ETDM_IN6_CUR_MSB,
		.reg_ofs_end_msb = AFE_ETDM_IN6_END_MSB,
		.fs_reg = ETDM_IN6_CON3,
		.fs_shift = REG_FS_TIMING_SEL_SFT,
		.fs_maskbit = REG_FS_TIMING_SEL_MASK,
		//.mono_reg = ,
		//.mono_shift = ,
		.enable_reg = AFE_ETDM_IN6_CON0,
		.enable_shift = ETDM_IN6_ON_SFT,
		.hd_reg = AFE_ETDM_IN6_CON0,
		.hd_mask = ETDM_IN6_HD_MODE_MASK,
		.hd_shift = ETDM_IN6_HD_MODE_SFT,
		.hd_align_reg = AFE_ETDM_IN6_CON0,
		.hd_align_mshift = ETDM_IN6_HALIGN_SFT,
		.hd_msb_shift = ETDM_IN6_HD_MSB_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6989_MEMIF_HDMI] = {
		.name = "HDMI",
		.id = MT6989_MEMIF_HDMI,
		.reg_ofs_base = AFE_HDMI_OUT_BASE,
		.reg_ofs_cur = AFE_HDMI_OUT_CUR,
		.reg_ofs_end = AFE_HDMI_OUT_END,
		.reg_ofs_base_msb = AFE_HDMI_OUT_BASE_MSB,
		.reg_ofs_cur_msb = AFE_HDMI_OUT_CUR_MSB,
		.reg_ofs_end_msb = AFE_HDMI_OUT_END_MSB,
		.fs_reg = -1,
		.fs_shift = -1,
		.fs_maskbit = -1,
		.mono_reg = -1,
		.mono_shift = -1,
		.enable_reg = AFE_HDMI_OUT_CON0,
		.enable_shift = HDMI_OUT_ON_SFT,
		.hd_reg = AFE_HDMI_OUT_CON0,
		.hd_mask = HDMI_OUT_HD_MODE_MASK,
		.hd_shift = HDMI_OUT_HD_MODE_SFT,
		.hd_align_reg = AFE_HDMI_OUT_CON0,
		.hd_align_mshift = HDMI_OUT_HALIGN_SFT,
		.hd_msb_shift = -1,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_HDMI_OUT_CON0,
		.pbuf_mask = HDMI_OUT_PBUF_SIZE_MASK,
		.pbuf_shift = HDMI_OUT_PBUF_SIZE_SFT,
		.minlen_reg = AFE_HDMI_OUT_CON0,
		.minlen_mask = HDMI_OUT_MINLEN_MASK,
		.minlen_shift = HDMI_OUT_MINLEN_SFT,
	},
};

static const struct mtk_base_irq_data irq_data[MT6989_IRQ_NUM] = {
	[MT6989_IRQ_0] = {
		.id = MT6989_IRQ_0,
		.irq_cnt_reg = AFE_IRQ0_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ0_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ0_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ0_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ0_MCU_CFG0,
		.irq_en_shift = AFE_IRQ0_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ0_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ0_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_1] = {
		.id = MT6989_IRQ_1,
		.irq_cnt_reg = AFE_IRQ1_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ1_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ1_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ1_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ1_MCU_CFG0,
		.irq_en_shift = AFE_IRQ1_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ1_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ1_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_2] = {
		.id = MT6989_IRQ_2,
		.irq_cnt_reg = AFE_IRQ2_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ2_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ2_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ2_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ2_MCU_CFG0,
		.irq_en_shift = AFE_IRQ2_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ2_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ2_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_3] = {
		.id = MT6989_IRQ_3,
		.irq_cnt_reg = AFE_IRQ3_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ3_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ3_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ3_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ3_MCU_CFG0,
		.irq_en_shift = AFE_IRQ3_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ3_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ3_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_4] = {
		.id = MT6989_IRQ_4,
		.irq_cnt_reg = AFE_IRQ4_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ4_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ4_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ4_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ4_MCU_CFG0,
		.irq_en_shift = AFE_IRQ4_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ4_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ4_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_5] = {
		.id = MT6989_IRQ_5,
		.irq_cnt_reg = AFE_IRQ5_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ5_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ5_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ5_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ5_MCU_CFG0,
		.irq_en_shift = AFE_IRQ5_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ5_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ5_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_6] = {
		.id = MT6989_IRQ_6,
		.irq_cnt_reg = AFE_IRQ6_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ6_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ6_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ6_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ6_MCU_CFG0,
		.irq_en_shift = AFE_IRQ6_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ6_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ6_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_7] = {
		.id = MT6989_IRQ_7,
		.irq_cnt_reg = AFE_IRQ7_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ7_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ7_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ7_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ7_MCU_CFG0,
		.irq_en_shift = AFE_IRQ7_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ7_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ7_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_8] = {
		.id = MT6989_IRQ_8,
		.irq_cnt_reg = AFE_IRQ8_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ8_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ8_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ8_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ8_MCU_CFG0,
		.irq_en_shift = AFE_IRQ8_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ8_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ8_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_9] = {
		.id = MT6989_IRQ_9,
		.irq_cnt_reg = AFE_IRQ9_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ9_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ9_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ9_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ9_MCU_CFG0,
		.irq_en_shift = AFE_IRQ9_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ9_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ9_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_10] = {
		.id = MT6989_IRQ_10,
		.irq_cnt_reg = AFE_IRQ10_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ10_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ10_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ10_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ10_MCU_CFG0,
		.irq_en_shift = AFE_IRQ10_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ10_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ10_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_11] = {
		.id = MT6989_IRQ_11,
		.irq_cnt_reg = AFE_IRQ11_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ11_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ11_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ11_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ11_MCU_CFG0,
		.irq_en_shift = AFE_IRQ11_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ11_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ11_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_12] = {
		.id = MT6989_IRQ_12,
		.irq_cnt_reg = AFE_IRQ12_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ12_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ12_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ12_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ12_MCU_CFG0,
		.irq_en_shift = AFE_IRQ12_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ12_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ12_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_13] = {
		.id = MT6989_IRQ_13,
		.irq_cnt_reg = AFE_IRQ13_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ13_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ13_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ13_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ13_MCU_CFG0,
		.irq_en_shift = AFE_IRQ13_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ13_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ13_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_14] = {
		.id = MT6989_IRQ_14,
		.irq_cnt_reg = AFE_IRQ14_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ14_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ14_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ14_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ14_MCU_CFG0,
		.irq_en_shift = AFE_IRQ14_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ14_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ14_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_15] = {
		.id = MT6989_IRQ_15,
		.irq_cnt_reg = AFE_IRQ15_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ15_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ15_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ15_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ15_MCU_CFG0,
		.irq_en_shift = AFE_IRQ15_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ15_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ15_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_16] = {
		.id = MT6989_IRQ_16,
		.irq_cnt_reg = AFE_IRQ16_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ16_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ16_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ16_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ16_MCU_CFG0,
		.irq_en_shift = AFE_IRQ16_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ16_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ16_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_17] = {
		.id = MT6989_IRQ_17,
		.irq_cnt_reg = AFE_IRQ17_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ17_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ17_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ17_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ17_MCU_CFG0,
		.irq_en_shift = AFE_IRQ17_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ17_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ17_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_18] = {
		.id = MT6989_IRQ_18,
		.irq_cnt_reg = AFE_IRQ18_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ18_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ18_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ18_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ18_MCU_CFG0,
		.irq_en_shift = AFE_IRQ18_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ18_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ18_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_19] = {
		.id = MT6989_IRQ_19,
		.irq_cnt_reg = AFE_IRQ19_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ19_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ19_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ19_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ19_MCU_CFG0,
		.irq_en_shift = AFE_IRQ19_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ19_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ19_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_20] = {
		.id = MT6989_IRQ_20,
		.irq_cnt_reg = AFE_IRQ20_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ20_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ20_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ20_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ20_MCU_CFG0,
		.irq_en_shift = AFE_IRQ20_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ20_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ20_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_21] = {
		.id = MT6989_IRQ_21,
		.irq_cnt_reg = AFE_IRQ21_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ21_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ21_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ21_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ21_MCU_CFG0,
		.irq_en_shift = AFE_IRQ21_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ21_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ21_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_22] = {
		.id = MT6989_IRQ_22,
		.irq_cnt_reg = AFE_IRQ22_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ22_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ22_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ22_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ22_MCU_CFG0,
		.irq_en_shift = AFE_IRQ22_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ22_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ22_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_23] = {
		.id = MT6989_IRQ_23,
		.irq_cnt_reg = AFE_IRQ23_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ23_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ23_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ23_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ23_MCU_CFG0,
		.irq_en_shift = AFE_IRQ23_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ23_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ23_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_24] = {
		.id = MT6989_IRQ_24,
		.irq_cnt_reg = AFE_IRQ24_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ24_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ24_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ24_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ24_MCU_CFG0,
		.irq_en_shift = AFE_IRQ24_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ24_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ24_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_25] = {
		.id = MT6989_IRQ_25,
		.irq_cnt_reg = AFE_IRQ25_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ25_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ25_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ25_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ25_MCU_CFG0,
		.irq_en_shift = AFE_IRQ25_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ25_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ25_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_26] = {
		.id = MT6989_IRQ_26,
		.irq_cnt_reg = AFE_IRQ26_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ26_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ26_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ26_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ26_MCU_CFG0,
		.irq_en_shift = AFE_IRQ26_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ26_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ26_MCU_SCP_EN_SFT,
	},
	[MT6989_IRQ_31] = {
		.id = MT6989_CUS_IRQ_TDM,
		.irq_cnt_reg = AFE_CUSTOM_IRQ0_MCU_CNT,
		.irq_cnt_shift = AFE_CUSTOM_IRQ0_MCU_CNT_SFT,
		.irq_cnt_maskbit = AFE_CUSTOM_IRQ0_MCU_CNT_MASK,
		.irq_fs_reg = -1,
		.irq_fs_shift = -1,
		.irq_fs_maskbit = -1,
		.irq_en_reg = AFE_CUSTOM_IRQ_MCU_CON_CFG0,
		.irq_en_shift = AFE_CUSTOM_IRQ0_MCU_ON_SFT,
		.irq_clr_reg = AFE_CUSTOM_IRQ_MCU_CLR,
		.irq_clr_shift = CUSTOM_IRQ0_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_CUSTOM_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_CUSTOM_IRQ_MCU_SCP_EN,
	},
};

static const int memif_irq_usage[MT6989_MEMIF_NUM] = {
	/* TODO: verify each memif & irq */
	[MT6989_MEMIF_DL0] = MT6989_IRQ_0,
	[MT6989_MEMIF_DL1] = MT6989_IRQ_1,
	[MT6989_MEMIF_DL2] = MT6989_IRQ_2,
	[MT6989_MEMIF_DL3] = MT6989_IRQ_3,
	[MT6989_MEMIF_DL4] = MT6989_IRQ_4,
	[MT6989_MEMIF_DL5] = MT6989_IRQ_5,
	[MT6989_MEMIF_DL6] = MT6989_IRQ_6,
	[MT6989_MEMIF_DL7] = MT6989_IRQ_7,
	[MT6989_MEMIF_DL8] = MT6989_IRQ_8,
	[MT6989_MEMIF_DL23] = MT6989_IRQ_9,
	[MT6989_MEMIF_DL24] = MT6989_IRQ_10,
	[MT6989_MEMIF_DL25] = MT6989_IRQ_11,
	[MT6989_MEMIF_DL_24CH] = MT6989_IRQ_12,
	[MT6989_MEMIF_VUL0] = MT6989_IRQ_13,
	[MT6989_MEMIF_VUL1] = MT6989_IRQ_14,
	[MT6989_MEMIF_VUL2] = MT6989_IRQ_15,
	[MT6989_MEMIF_VUL3] = MT6989_IRQ_16,
	[MT6989_MEMIF_VUL4] = MT6989_IRQ_17,
	[MT6989_MEMIF_VUL5] = MT6989_IRQ_18,
	[MT6989_MEMIF_VUL6] = MT6989_IRQ_19,
	[MT6989_MEMIF_VUL7] = MT6989_IRQ_20,
	[MT6989_MEMIF_VUL8] = MT6989_IRQ_21,
	[MT6989_MEMIF_VUL9] = MT6989_IRQ_22,
	[MT6989_MEMIF_VUL10] = MT6989_IRQ_23,
	[MT6989_MEMIF_VUL24] = MT6989_IRQ_24,
	[MT6989_MEMIF_VUL25] = MT6989_IRQ_25,
	[MT6989_MEMIF_VUL_CM0] = MT6989_IRQ_26,
	[MT6989_MEMIF_VUL_CM1] = MT6989_IRQ_0,
	[MT6989_MEMIF_ETDM_IN0] = MT6989_IRQ_0,
	[MT6989_MEMIF_ETDM_IN1] = MT6989_IRQ_0,
	[MT6989_MEMIF_ETDM_IN2] = MT6989_IRQ_0,
	[MT6989_MEMIF_ETDM_IN4] = MT6989_IRQ_0,
	[MT6989_MEMIF_ETDM_IN6] = MT6989_IRQ_0,
	[MT6989_MEMIF_HDMI] = MT6989_IRQ_31,
};

static bool mt6989_is_volatile_reg(struct device *dev, unsigned int reg)
{
	/* these auto-gen reg has read-only bit, so put it as volatile */
	/* volatile reg cannot be cached, so cannot be set when power off */
	switch (reg) {
	case AUDIO_TOP_CON0:    /* reg bit controlled by CCF */
	case AUDIO_TOP_CON1:    /* reg bit controlled by CCF */
	case AUDIO_TOP_CON2:
	case AUDIO_TOP_CON3:
	case AUDIO_TOP_CON4:
	case AFE_APLL1_TUNER_MON0:
	case AFE_APLL2_TUNER_MON0:
	case AFE_SPM_CONTROL_ACK:
	case AUDIO_TOP_IP_VERSION:
	case AUDIO_ENGEN_CON0_MON:
	case AFE_CONNSYS_I2S_IPM_VER_MON:
	case AFE_CONNSYS_I2S_MON:
	case AFE_PCM_INTF_MON:
	case AFE_PCM_TOP_IP_VERSION:
	case AFE_IRQ_MCU_STATUS:
	case AFE_CUSTOM_IRQ_MCU_STATUS:
	case AFE_IRQ_MCU_MON0:
	case AFE_IRQ_MCU_MON1:
	case AFE_IRQ_MCU_MON2:
	case AFE_IRQ0_CNT_MON:
	case AFE_IRQ1_CNT_MON:
	case AFE_IRQ2_CNT_MON:
	case AFE_IRQ3_CNT_MON:
	case AFE_IRQ4_CNT_MON:
	case AFE_IRQ5_CNT_MON:
	case AFE_IRQ6_CNT_MON:
	case AFE_IRQ7_CNT_MON:
	case AFE_IRQ8_CNT_MON:
	case AFE_IRQ9_CNT_MON:
	case AFE_IRQ10_CNT_MON:
	case AFE_IRQ11_CNT_MON:
	case AFE_IRQ12_CNT_MON:
	case AFE_IRQ13_CNT_MON:
	case AFE_IRQ14_CNT_MON:
	case AFE_IRQ15_CNT_MON:
	case AFE_IRQ16_CNT_MON:
	case AFE_IRQ17_CNT_MON:
	case AFE_IRQ18_CNT_MON:
	case AFE_IRQ19_CNT_MON:
	case AFE_IRQ20_CNT_MON:
	case AFE_IRQ21_CNT_MON:
	case AFE_IRQ22_CNT_MON:
	case AFE_IRQ23_CNT_MON:
	case AFE_IRQ24_CNT_MON:
	case AFE_IRQ25_CNT_MON:
	case AFE_IRQ26_CNT_MON:
	case AFE_CUSTOM_IRQ0_CNT_MON:
	case AFE_STF_MON:
	case AFE_STF_IP_VERSION:
	case AFE_CM0_MON:
	case AFE_CM0_IP_VERSION:
	case AFE_CM1_MON:
	case AFE_CM1_IP_VERSION:
	case AFE_ADDA_UL0_SRC_DEBUG_MON0:
	case AFE_ADDA_UL0_SRC_MON0:
	case AFE_ADDA_UL0_SRC_MON1:
	case AFE_ADDA_UL0_IP_VERSION:
	case AFE_ADDA_UL1_SRC_DEBUG_MON0:
	case AFE_ADDA_UL1_SRC_MON0:
	case AFE_ADDA_UL1_SRC_MON1:
	case AFE_ADDA_UL1_IP_VERSION:
	case AFE_MTKAIF_IPM_VER_MON:
	case AFE_MTKAIF_MON:
	case AFE_AUD_PAD_TOP_MON:
	case AFE_ADDA_MTKAIFV4_MON0:
	case AFE_ADDA_MTKAIFV4_MON1:
	case AFE_ADDA6_MTKAIFV4_MON0:
	case ETDM_IN0_MON:
	case ETDM_IN1_MON:
	case ETDM_IN2_MON:
	case ETDM_IN4_MON:
	case ETDM_IN6_MON:
	case ETDM_OUT0_MON:
	case ETDM_OUT1_MON:
	case ETDM_OUT2_MON:
	case ETDM_OUT4_MON:
	case ETDM_OUT6_MON:
	case AFE_DPTX_MON:
	case AFE_TDM_TOP_IP_VERSION:
	case AFE_CONN_MON0:
	case AFE_CONN_MON1:
	case AFE_CONN_MON2:
	case AFE_CONN_MON3:
	case AFE_CONN_MON4:
	case AFE_CONN_MON5:
	case AFE_CBIP_SLV_DECODER_MON0:
	case AFE_CBIP_SLV_DECODER_MON1:
	case AFE_CBIP_SLV_MUX_MON0:
	case AFE_CBIP_SLV_MUX_MON1:
	case AFE_DL0_CUR_MSB:
	case AFE_DL0_CUR:
	case AFE_DL0_RCH_MON:
	case AFE_DL0_LCH_MON:
	case AFE_DL1_CUR_MSB:
	case AFE_DL1_CUR:
	case AFE_DL1_RCH_MON:
	case AFE_DL1_LCH_MON:
	case AFE_DL2_CUR_MSB:
	case AFE_DL2_CUR:
	case AFE_DL2_RCH_MON:
	case AFE_DL2_LCH_MON:
	case AFE_DL3_CUR_MSB:
	case AFE_DL3_CUR:
	case AFE_DL3_RCH_MON:
	case AFE_DL3_LCH_MON:
	case AFE_DL4_CUR_MSB:
	case AFE_DL4_CUR:
	case AFE_DL4_RCH_MON:
	case AFE_DL4_LCH_MON:
	case AFE_DL5_CUR_MSB:
	case AFE_DL5_CUR:
	case AFE_DL5_RCH_MON:
	case AFE_DL5_LCH_MON:
	case AFE_DL6_CUR_MSB:
	case AFE_DL6_CUR:
	case AFE_DL6_RCH_MON:
	case AFE_DL6_LCH_MON:
	case AFE_DL7_CUR_MSB:
	case AFE_DL7_CUR:
	case AFE_DL7_RCH_MON:
	case AFE_DL7_LCH_MON:
	case AFE_DL8_CUR_MSB:
	case AFE_DL8_CUR:
	case AFE_DL8_RCH_MON:
	case AFE_DL8_LCH_MON:
	case AFE_DL_24CH_CUR_MSB:
	case AFE_DL_24CH_CUR:
	case AFE_DL23_CUR_MSB:
	case AFE_DL23_CUR:
	case AFE_DL23_RCH_MON:
	case AFE_DL23_LCH_MON:
	case AFE_DL24_CUR_MSB:
	case AFE_DL24_CUR:
	case AFE_DL24_RCH_MON:
	case AFE_DL24_LCH_MON:
	case AFE_DL25_CUR_MSB:
	case AFE_DL25_CUR:
	case AFE_DL25_RCH_MON:
	case AFE_DL25_LCH_MON:
	case AFE_VUL0_CUR_MSB:
	case AFE_VUL0_CUR:
	case AFE_VUL1_CUR_MSB:
	case AFE_VUL1_CUR:
	case AFE_VUL2_CUR_MSB:
	case AFE_VUL2_CUR:
	case AFE_VUL3_CUR_MSB:
	case AFE_VUL3_CUR:
	case AFE_VUL4_CUR_MSB:
	case AFE_VUL4_CUR:
	case AFE_VUL5_CUR_MSB:
	case AFE_VUL5_CUR:
	case AFE_VUL6_CUR_MSB:
	case AFE_VUL6_CUR:
	case AFE_VUL7_CUR_MSB:
	case AFE_VUL7_CUR:
	case AFE_VUL8_CUR_MSB:
	case AFE_VUL8_CUR:
	case AFE_VUL9_CUR_MSB:
	case AFE_VUL9_CUR:
	case AFE_VUL10_CUR_MSB:
	case AFE_VUL10_CUR:
	case AFE_VUL24_CUR_MSB:
	case AFE_VUL24_CUR:
	case AFE_VUL25_CUR_MSB:
	case AFE_VUL25_CUR:
	case AFE_VUL_CM0_CUR_MSB:
	case AFE_VUL_CM0_CUR:
	case AFE_VUL_CM1_CUR_MSB:
	case AFE_VUL_CM1_CUR:
	case AFE_ETDM_IN0_CUR_MSB:
	case AFE_ETDM_IN0_CUR:
	case AFE_ETDM_IN1_CUR_MSB:
	case AFE_ETDM_IN1_CUR:
	case AFE_ETDM_IN2_CUR_MSB:
	case AFE_ETDM_IN2_CUR:
	case AFE_ETDM_IN4_CUR_MSB:
	case AFE_ETDM_IN4_CUR:
	case AFE_ETDM_IN6_CUR_MSB:
	case AFE_ETDM_IN6_CUR:
	case AFE_HDMI_OUT_CUR_MSB:
	case AFE_HDMI_OUT_CUR:
	case AFE_HDMI_OUT_END:
	case AFE_PROT_SIDEBAND0_MON:
	case AFE_PROT_SIDEBAND1_MON:
	case AFE_PROT_SIDEBAND2_MON:
	case AFE_PROT_SIDEBAND3_MON:
	case AFE_DOMAIN_SIDEBAND0_MON:
	case AFE_DOMAIN_SIDEBAND1_MON:
	case AFE_DOMAIN_SIDEBAND2_MON:
	case AFE_DOMAIN_SIDEBAND3_MON:
	case AFE_DOMAIN_SIDEBAND4_MON:
	case AFE_DOMAIN_SIDEBAND5_MON:
	case AFE_DOMAIN_SIDEBAND6_MON:
	case AFE_DOMAIN_SIDEBAND7_MON:
	case AFE_DOMAIN_SIDEBAND8_MON:
	case AFE_DOMAIN_SIDEBAND9_MON:
	case AFE_PCM0_INTF_CON1_MASK_MON:
	case AFE_PCM0_INTF_CON0_MASK_MON:
	case AFE_CONNSYS_I2S_CON_MASK_MON:
	case AFE_TDM_CON2_MASK_MON:
	case AFE_MTKAIF0_CFG0_MASK_MON:
	case AFE_MTKAIF1_CFG0_MASK_MON:
	case AFE_ADDA_UL0_SRC_CON0_MASK_MON:
	case AFE_ADDA_UL1_SRC_CON0_MASK_MON:
	case AFE_ASRC_NEW_CON0:
	case AFE_ASRC_NEW_CON6:
	case AFE_ASRC_NEW_CON8:
	case AFE_ASRC_NEW_CON9:
	case AFE_ASRC_NEW_CON12:
	case AFE_ASRC_NEW_IP_VERSION:
	case AFE_GASRC0_NEW_CON0:
	case AFE_GASRC0_NEW_CON6:
	case AFE_GASRC0_NEW_CON8:
	case AFE_GASRC0_NEW_CON9:
	case AFE_GASRC0_NEW_CON10:
	case AFE_GASRC0_NEW_CON11:
	case AFE_GASRC0_NEW_CON12:
	case AFE_GASRC0_NEW_IP_VERSION:
	case AFE_GASRC1_NEW_CON0:
	case AFE_GASRC1_NEW_CON6:
	case AFE_GASRC1_NEW_CON8:
	case AFE_GASRC1_NEW_CON9:
	case AFE_GASRC1_NEW_CON12:
	case AFE_GASRC1_NEW_IP_VERSION:
	case AFE_GASRC2_NEW_CON0:
	case AFE_GASRC2_NEW_CON6:
	case AFE_GASRC2_NEW_CON8:
	case AFE_GASRC2_NEW_CON9:
	case AFE_GASRC2_NEW_CON12:
	case AFE_GASRC2_NEW_IP_VERSION:
	case AFE_GASRC3_NEW_CON0:
	case AFE_GASRC3_NEW_CON6:
	case AFE_GASRC3_NEW_CON8:
	case AFE_GASRC3_NEW_CON9:
	case AFE_GASRC3_NEW_CON12:
	case AFE_GASRC3_NEW_IP_VERSION:
	case AFE_GAIN0_CUR_L:
	case AFE_GAIN0_CUR_R:
	case AFE_GAIN1_CUR_L:
	case AFE_GAIN1_CUR_R:
	case AFE_GAIN2_CUR_L:
	case AFE_GAIN2_CUR_R:
	case AFE_GAIN3_CUR_L:
	case AFE_GAIN3_CUR_R:
	/* these reg would change in adsp */
	case AFE_IRQ_MCU_EN:
	case AFE_IRQ_MCU_DSP_EN:
	case AFE_IRQ_MCU_DSP2_EN:
	case AFE_DL5_CON0:
	case AFE_DL6_CON0:
	case AFE_DL23_CON0:
	case AFE_DL_24CH_CON0:
	case AFE_DL_24CH_CON1:
	case AFE_VUL1_CON0:
	case AFE_VUL3_CON0:
	case AFE_VUL4_CON0:
	case AFE_VUL5_CON0:
	case AFE_VUL9_CON0:
	case AFE_VUL25_CON0:
	case AFE_IRQ0_MCU_CFG0:
	case AFE_IRQ1_MCU_CFG0:
	case AFE_IRQ2_MCU_CFG0:
	case AFE_IRQ3_MCU_CFG0:
	case AFE_IRQ4_MCU_CFG0:
	case AFE_IRQ5_MCU_CFG0:
	case AFE_IRQ6_MCU_CFG0:
	case AFE_IRQ7_MCU_CFG0:
	case AFE_IRQ8_MCU_CFG0:
	case AFE_IRQ9_MCU_CFG0:
	case AFE_IRQ10_MCU_CFG0:
	case AFE_IRQ11_MCU_CFG0:
	case AFE_IRQ12_MCU_CFG0:
	case AFE_IRQ13_MCU_CFG0:
	case AFE_IRQ14_MCU_CFG0:
	case AFE_IRQ15_MCU_CFG0:
	case AFE_IRQ16_MCU_CFG0:
	case AFE_IRQ17_MCU_CFG0:
	case AFE_IRQ18_MCU_CFG0:
	case AFE_IRQ19_MCU_CFG0:
	case AFE_IRQ20_MCU_CFG0:
	case AFE_IRQ21_MCU_CFG0:
	case AFE_IRQ22_MCU_CFG0:
	case AFE_IRQ23_MCU_CFG0:
	case AFE_IRQ24_MCU_CFG0:
	case AFE_IRQ25_MCU_CFG0:
	case AFE_IRQ26_MCU_CFG0:
	case AFE_IRQ0_MCU_CFG1:
	case AFE_IRQ1_MCU_CFG1:
	case AFE_IRQ2_MCU_CFG1:
	case AFE_IRQ3_MCU_CFG1:
	case AFE_IRQ4_MCU_CFG1:
	case AFE_IRQ5_MCU_CFG1:
	case AFE_IRQ6_MCU_CFG1:
	case AFE_IRQ7_MCU_CFG1:
	case AFE_IRQ8_MCU_CFG1:
	case AFE_IRQ9_MCU_CFG1:
	case AFE_IRQ10_MCU_CFG1:
	case AFE_IRQ11_MCU_CFG1:
	case AFE_IRQ12_MCU_CFG1:
	case AFE_IRQ13_MCU_CFG1:
	case AFE_IRQ14_MCU_CFG1:
	case AFE_IRQ15_MCU_CFG1:
	case AFE_IRQ16_MCU_CFG1:
	case AFE_IRQ17_MCU_CFG1:
	case AFE_IRQ18_MCU_CFG1:
	case AFE_IRQ19_MCU_CFG1:
	case AFE_IRQ20_MCU_CFG1:
	case AFE_IRQ21_MCU_CFG1:
	case AFE_IRQ22_MCU_CFG1:
	case AFE_IRQ23_MCU_CFG1:
	case AFE_IRQ24_MCU_CFG1:
	case AFE_IRQ25_MCU_CFG1:
	case AFE_IRQ26_MCU_CFG1:
	/* for vow using */
	case AFE_IRQ_MCU_SCP_EN:
	case AFE_VUL_CM0_BASE_MSB:
	case AFE_VUL_CM0_BASE:
	case AFE_VUL_CM0_END_MSB:
	case AFE_VUL_CM0_END:
	case AFE_VUL_CM0_CON0:
	/* case AFE_CM0_CON0: */
	/* case AFE_CM1_CON0: */
		return true;
	default:
		return false;
	};
}

static const struct regmap_config mt6989_afe_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,

	.volatile_reg = mt6989_is_volatile_reg,

	.max_register = AFE_MAX_REGISTER,
	.num_reg_defaults_raw = AFE_MAX_REGISTER,

	.cache_type = REGCACHE_FLAT,
};

#if !defined(CONFIG_FPGA_EARLY_PORTING) || defined(FORCE_FPGA_ENABLE_IRQ)
static irqreturn_t mt6989_afe_irq_handler(int irq_id, void *dev)
{
	struct mtk_base_afe *afe = dev;
	struct mtk_base_afe_irq *irq;
	unsigned int status = 0;
	unsigned int status_mcu;
	unsigned int mcu_en = 0;
	unsigned int cus_status = 0;
	unsigned int cus_status_mcu;
	unsigned int cus_mcu_en = 0;
	int ret, cus_ret;
	int i;
	struct timespec64 ts64;
	unsigned long long t1, t2;
	/* one interrupt period = 5ms */
	unsigned long long timeout_limit = 5000000;

	/* get irq that is sent to MCU */
	regmap_read(afe->regmap, AFE_IRQ_MCU_EN, &mcu_en);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_EN, &cus_mcu_en);

	ret = regmap_read(afe->regmap, AFE_IRQ_MCU_STATUS, &status);
	cus_ret = regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_STATUS, &cus_status);
	/* only care IRQ which is sent to MCU */
	status_mcu = status & mcu_en & AFE_IRQ_STATUS_BITS;
	cus_status_mcu = cus_status & cus_mcu_en & AFE_IRQ_STATUS_BITS;
	if ((ret || (status_mcu == 0)) &&
	    (cus_ret || (cus_status_mcu == 0))) {
		dev_info(afe->dev, "%s(), irq status err, ret %d, status 0x%x, mcu_en 0x%x\n",
			 __func__, ret, status, mcu_en);
		dev_info(afe->dev, "%s(), irq status err, ret %d, cus_status_mcu 0x%x, cus_mcu_en 0x%x\n",
			 __func__, ret, cus_status_mcu, cus_mcu_en);

		goto err_irq;
	}

	ktime_get_ts64(&ts64);
	t1 = timespec64_to_ns(&ts64);

	for (i = 0; i < MT6989_MEMIF_NUM; i++) {
		struct mtk_base_afe_memif *memif = &afe->memif[i];

		if (!memif->substream)
			continue;

		if (memif->irq_usage < 0)
			continue;
		irq = &afe->irqs[memif->irq_usage];

		if (i == MT6989_MEMIF_HDMI) {
			if (cus_status_mcu & (0x1 << irq->irq_data->id))
				snd_pcm_period_elapsed(memif->substream);
		} else {
			if (status_mcu & (0x1 << irq->irq_data->id))
				snd_pcm_period_elapsed(memif->substream);
		}
	}

	ktime_get_ts64(&ts64);
	t2 = timespec64_to_ns(&ts64);
	t2 = t2 - t1; /* in ns (10^9) */

	if (t2 > timeout_limit) {
		dev_info(afe->dev, "%s(), mcu_en 0x%x, cus_mcu_en 0x%x, timeout %llu, limit %llu, ret %d\n",
			__func__, mcu_en, cus_mcu_en,
			t2, timeout_limit, ret);
	}

err_irq:
	/* clear irq */
	regmap_write(afe->regmap,
		     AFE_IRQ_MCU_CLR,
		     status_mcu);
	regmap_write(afe->regmap,
		     AFE_CUSTOM_IRQ_MCU_CLR,
		     cus_status_mcu);

	return IRQ_HANDLED;
}
#endif

static int mt6989_afe_runtime_suspend(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	unsigned int value = 0;
	int ret = 0;

	if (!afe->regmap) {
		dev_info(afe->dev, "%s() skip regmap\n", __func__);
		goto skip_regmap;
	}

	/* Add to be off for free run*/
	/* disable AFE */
	regmap_update_bits(afe->regmap, AUDIO_ENGEN_CON0, 0x1, 0x0);

	ret = regmap_read_poll_timeout(afe->regmap,
				       AUDIO_ENGEN_CON0_MON,
				       value,
				       (value & AUDIO_ENGEN_MON_SFT) == 0,
				       20,
				       1 * 1000 * 1000);
	dev_info(afe->dev, "%s() read_poll ret %d\n", __func__, ret);
	if (ret)
		dev_info(afe->dev, "%s(), ret %d\n", __func__, ret);

	/* make sure all irq status are cleared */
	regmap_write(afe->regmap, AFE_IRQ_MCU_CLR, 0xffffffff);
	regmap_write(afe->regmap, AFE_IRQ_MCU_CLR, 0xffffffff);
	regmap_write(afe->regmap, AFE_CUSTOM_IRQ_MCU_CLR, 0xffffffff);
	regmap_write(afe->regmap, AFE_CUSTOM_IRQ_MCU_CLR, 0xffffffff);

	/* reset sgen */
	regmap_write(afe->regmap, AFE_SINEGEN_CON0, 0x0);
	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
			   SINE_DOMAIN_MASK_SFT,
			   0x0 << SINE_DOMAIN_SFT);
	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
			   SINE_MODE_MASK_SFT,
			   0x0 << SINE_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
			   INNER_LOOP_BACKI_SEL_MASK_SFT,
			   0x0 << INNER_LOOP_BACKI_SEL_SFT);
	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
			   INNER_LOOP_BACK_MODE_MASK_SFT,
			   0xff << INNER_LOOP_BACK_MODE_SFT);

	regmap_write(afe->regmap, AUDIO_TOP_CON4, 0x3fff);

	/* cache only */
	regcache_cache_only(afe->regmap, true);
	regcache_mark_dirty(afe->regmap);

skip_regmap:
	mt6989_afe_sram_release(afe);
	mt6989_afe_disable_clock(afe);
	return 0;
}

static int mt6989_afe_runtime_resume(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	int ret = 0;

	ret = mt6989_afe_enable_clock(afe);
	dev_info(afe->dev, "%s(), enable_clock ret %d\n", __func__, ret);

	if (ret)
		return ret;
	mt6989_afe_sram_request(afe);

	if (!afe->regmap) {
		dev_info(afe->dev, "%s() skip regmap\n", __func__);
		goto skip_regmap;
	}

	regcache_cache_only(afe->regmap, false);
	regcache_sync(afe->regmap);
	/* IPM2.0: Clear AUDIO_TOP_CON4 for enabling AP side module clk */
	regmap_write(afe->regmap, AUDIO_TOP_CON4, 0x0);

	/* Add to be on for free run */
	regmap_write(afe->regmap, AUDIO_TOP_CON0, 0x0);
	regmap_write(afe->regmap, AUDIO_TOP_CON1, 0x0);
	regmap_write(afe->regmap, AUDIO_TOP_CON2, 0x0);
	regmap_write(afe->regmap, AUDIO_TOP_CON3, 0x0);
	/* Can't set AUDIO_TOP_CON3 to be 0x0, it will hang in FPGA env */

	regmap_write(afe->regmap, AUDIO_TOP_CON3, 0x0);

	regmap_update_bits(afe->regmap, AFE_CBIP_CFG0, 0x1, 0x1);

	/* force cpu use 8_24 format when writing 32bit data */
	regmap_update_bits(afe->regmap, AFE_MEMIF_CON0,
			   CPU_HD_ALIGN_MASK_SFT, 0 << CPU_HD_ALIGN_SFT);

	/* set all output port to 24bit */
	regmap_write(afe->regmap, AFE_CONN_24BIT_0, 0xffffffff);
	regmap_write(afe->regmap, AFE_CONN_24BIT_1, 0xffffffff);
	regmap_write(afe->regmap, AFE_CONN_24BIT_2, 0xffffffff);
	regmap_write(afe->regmap, AFE_CONN_24BIT_3, 0xffffffff);
	regmap_write(afe->regmap, AFE_CONN_24BIT_4, 0xffffffff);
	regmap_write(afe->regmap, AFE_CONN_24BIT_5, 0xffffffff);

	/* enable AFE */
	regmap_update_bits(afe->regmap, AUDIO_ENGEN_CON0, 0x1, 0x1);

skip_regmap:
	return 0;
}

static int mt6989_afe_pcm_copy(struct snd_pcm_substream *substream,
			       int channel, unsigned long hwoff,
			       void *buf, unsigned long bytes,
			       mtk_sp_copy_f sp_copy)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = NULL;
	int ret = 0;

	if (!component)
		return -EINVAL;
	afe = snd_soc_component_get_drvdata(component);
	/* mt6989_set_audio_int_bus_parent(afe, CLK_TOP_MAINPLL_D4_D4); */

	ret = sp_copy(substream, channel, hwoff, buf, bytes);

	/* mt6989_set_audio_int_bus_parent(afe, CLK_CLK26M); */

	return ret;
}

static int mt6989_set_memif_sram_mode(struct device *dev,
				      enum mtk_audio_sram_mode sram_mode)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	int reg_bit = sram_mode == MTK_AUDIO_SRAM_NORMAL_MODE ? 1 : 0;

	regmap_update_bits(afe->regmap, AFE_DL0_CON0,
			   DL0_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL0_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL1_CON0,
			   DL1_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL1_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL2_CON0,
			   DL2_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL2_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL3_CON0,
			   DL3_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL3_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL4_CON0,
			   DL4_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL4_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL5_CON0,
			   DL5_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL5_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL6_CON0,
			   DL6_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL6_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL7_CON0,
			   DL7_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL7_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL8_CON0,
			   DL8_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL8_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL23_CON0,
			   DL23_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL23_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL24_CON0,
			   DL24_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL24_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL25_CON0,
			   DL25_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL25_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL_24CH_CON0,
			   DL_24CH_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL_24CH_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL0_CON0,
			   VUL0_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL0_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL1_CON0,
			   VUL1_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL1_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL2_CON0,
			   VUL2_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL2_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL3_CON0,
			   VUL3_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL3_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL4_CON0,
			   VUL4_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL4_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL5_CON0,
			   VUL5_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL5_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL6_CON0,
			   VUL6_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL6_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL7_CON0,
			   VUL7_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL7_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL8_CON0,
			   VUL8_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL8_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL9_CON0,
			   VUL9_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL9_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL10_CON0,
			   VUL10_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL10_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL24_CON0,
			   VUL24_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL24_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL25_CON0,
			   VUL25_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL25_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL_CM0_CON0,
			   VUL_CM0_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL_CM0_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL_CM1_CON0,
			   VUL_CM1_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL_CM1_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_ETDM_IN0_CON0,
			   ETDM_IN0_NORMAL_MODE_MASK_SFT,
			   reg_bit << ETDM_IN0_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_ETDM_IN1_CON0,
			   ETDM_IN1_NORMAL_MODE_MASK_SFT,
			   reg_bit << ETDM_IN1_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_ETDM_IN2_CON0,
			   ETDM_IN2_NORMAL_MODE_MASK_SFT,
			   reg_bit << ETDM_IN2_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_ETDM_IN4_CON0,
			   ETDM_IN4_NORMAL_MODE_MASK_SFT,
			   reg_bit << ETDM_IN4_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_ETDM_IN6_CON0,
			   ETDM_IN6_NORMAL_MODE_MASK_SFT,
			   reg_bit << ETDM_IN6_NORMAL_MODE_SFT);
	return 0;
}

static int mt6989_set_sram_mode(struct device *dev,
				enum mtk_audio_sram_mode sram_mode)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);

	/* set memif sram mode */
	mt6989_set_memif_sram_mode(dev, sram_mode);

	if (sram_mode == MTK_AUDIO_SRAM_COMPACT_MODE)
		/* cpu use compact mode when access sram data */
		regmap_update_bits(afe->regmap, AFE_MEMIF_CON0,
				   CPU_COMPACT_MODE_MASK_SFT,
				   0x1 << CPU_COMPACT_MODE_SFT);
	else
		/* cpu use normal mode when access sram data */
		regmap_update_bits(afe->regmap, AFE_MEMIF_CON0,
				   CPU_COMPACT_MODE_MASK_SFT,
				   0x0 << CPU_COMPACT_MODE_SFT);

	return 0;
}

static const struct mtk_audio_sram_ops mt6989_sram_ops = {
	.set_sram_mode = mt6989_set_sram_mode,
};

static u32 copy_from_buffer_request(void *dest, size_t destsize, const void *src,
				    size_t srcsize, u32 offset, size_t request)
{
	/* if request == -1, offset == 0, copy full srcsize */
	if (offset + request > srcsize)
		request = srcsize - offset;

	/* if destsize == -1, don't check the request size */
	if (!dest || destsize < request) {
		pr_info("%s, buffer null or not enough space", __func__);
		return 0;
	}

	memcpy(dest, src + offset, request);
	return request;
}

/*
 * sysfs bin_attribute node
 */

static ssize_t afe_sysfs_debug_read(struct file *filep, struct kobject *kobj,
				    struct bin_attribute *attr,
				    char *buf, loff_t offset, size_t size)
{
	size_t read_size, ceil_size, page_mask;
	ssize_t ret;
	struct mtk_base_afe *afe = (struct mtk_base_afe *)attr->private;
	char *buffer = NULL; /* for reduce kernel stack */

	buffer = kmalloc(AFE_SYS_DEBUG_SIZE, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	// sys fs op align with page size
	read_size = mt6989_debug_read_reg(buffer, AFE_SYS_DEBUG_SIZE, afe);
	page_mask = ~(PAGE_SIZE-1);
	ceil_size = (read_size&page_mask) + PAGE_SIZE;

	ret = copy_from_buffer_request(buf, -1, buffer, ceil_size, offset, size);
	kfree(buffer);

	return ret;
}

/*
 * sysfs bin_attribute node
 */
static ssize_t afe_sysfs_debug_write(struct file *filep, struct kobject *kobj,
				     struct bin_attribute *attr,
				     char *buf, loff_t offset, size_t size)
{
	struct mtk_base_afe *afe = (struct mtk_base_afe *)attr->private;

	char input[MAX_DEBUG_WRITE_INPUT];
	char *temp, *command, *str_begin;
	char delim[] = " ,";

	if (!size) {
		dev_info(afe->dev, "%s(), count is 0, return directly\n",
			 __func__);
		goto exit;
	}

	if (size >= MAX_DEBUG_WRITE_INPUT)
		size = MAX_DEBUG_WRITE_INPUT - 1;

	memset((void *)input, 0, MAX_DEBUG_WRITE_INPUT);
	memcpy(input, buf, size);
	input[(int)size] = '\0';

	str_begin = kstrndup(input, MAX_DEBUG_WRITE_INPUT - 1,
			     GFP_KERNEL);

	if (!str_begin) {
		dev_info(afe->dev, "%s(), kstrdup fail\n", __func__);
		goto exit;
	}
	temp = str_begin;

	command = strsep(&temp, delim);

	if (strcmp("write_reg", command) == 0)
		mtk_afe_write_reg(afe, (void *)temp);
exit:

	return size;
}

struct bin_attribute bin_attr_afe_dump = {
	.attr = {
		.name = "mtk_afe_node",
		.mode = 0444,
	},
	.size = AFE_SYS_DEBUG_SIZE,
	.read = afe_sysfs_debug_read,
	.write = afe_sysfs_debug_write,
};

static struct bin_attribute *afe_bin_attrs[] = {
	&bin_attr_afe_dump,
	NULL,
};

struct attribute_group afe_bin_attr_group = {
	.name = "mtk_afe_attrs",
	.bin_attrs = afe_bin_attrs,
};


static int mt6989_afe_component_probe(struct snd_soc_component *component)
{
	struct mtk_base_afe *afe = NULL;
	struct snd_soc_card *sndcard = NULL;
	struct snd_card *card = NULL;
	int ret = 0;

	if (component) {
		afe = snd_soc_component_get_drvdata(component);
		sndcard = component->card;
		card = sndcard->snd_card;

		mtk_afe_add_sub_dai_control(component);
		mt6989_add_misc_control(component);

		bin_attr_afe_dump.private = (void *)afe;
		ret = snd_card_add_dev_attr(card, &afe_bin_attr_group);
		if (ret)
			pr_info("snd_card_add_dev_attr fail\n");
	}

	return 0;
}

static const struct snd_soc_component_driver mt6989_afe_component = {
	.name = AFE_PCM_NAME,
	.probe = mt6989_afe_component_probe,
	.pcm_construct = mtk_afe_pcm_new,
	.pcm_destruct = mtk_afe_pcm_free,
	.open = mtk_afe_pcm_open,
	.pointer = mtk_afe_pcm_pointer,
	.copy_user = mtk_afe_pcm_copy_user,
};

static ssize_t mt6989_debug_read_reg(char *buffer, int size, struct mtk_base_afe *afe)
{
	int n = 0, i = 0;
	unsigned int value;
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	if (!buffer)
		return -ENOMEM;

	n += scnprintf(buffer + n, size - n,
		       "mtkaif calibration phase %d, %d, %d, %d\n",
		       afe_priv->mtkaif_chosen_phase[0],
		       afe_priv->mtkaif_chosen_phase[1],
		       afe_priv->mtkaif_chosen_phase[2],
		       afe_priv->mtkaif_chosen_phase[3]);

	n += scnprintf(buffer + n, size - n,
		       "mtkaif calibration cycle %d, %d, %d, %d\n",
		       afe_priv->mtkaif_phase_cycle[0],
		       afe_priv->mtkaif_phase_cycle[1],
		       afe_priv->mtkaif_phase_cycle[2],
		       afe_priv->mtkaif_phase_cycle[3]);

	for (i = 0; i < afe->memif_size; i++) {
		n += scnprintf(buffer + n, size - n,
			       "memif[%d], irq_usage %d\n",
			       i, afe->memif[i].irq_usage);
	}
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	regmap_read(afe_priv->topckgen, CLK_CFG_6, &value);
	n += scnprintf(buffer + n, size - n,
		       "CLK_CFG_6 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_CFG_10, &value);
	n += scnprintf(buffer + n, size - n,
		       "CLK_CFG_10 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_CFG_11, &value);
	n += scnprintf(buffer + n, size - n,
		       "CLK_CFG_11 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_CFG_13, &value);
	n += scnprintf(buffer + n, size - n,
		       "CLK_CFG_13 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_CFG_UPDATE, &value);
	n += scnprintf(buffer + n, size - n,
		       "CLK_CFG_UPDATE = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_CFG_UPDATE1, &value);
	n += scnprintf(buffer + n, size - n,
		       "CLK_CFG_UPDATE1 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_AUDDIV_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "CLK_AUDDIV_0 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_AUDDIV_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "CLK_AUDDIV_1 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_AUDDIV_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "CLK_AUDDIV_2 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_AUDDIV_3, &value);
	n += scnprintf(buffer + n, size - n,
		       "CLK_AUDDIV_3 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_AUDDIV_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "CLK_AUDDIV_4 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_AUDDIV_5, &value);
	n += scnprintf(buffer + n, size - n,
		       "CLK_AUDDIV_5 = 0x%x\n", value);

	regmap_read(afe_priv->apmixed, AP_PLL_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "AP_PLL_CON3 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "APLL1_CON0 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL1_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "APLL1_CON1 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL1_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "APLL1_CON2 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL1_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "APLL1_CON4 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "APLL2_CON0 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL2_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "APLL2_CON1 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL2_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "APLL2_CON2 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL2_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "APLL2_CON4 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL1_TUNER_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "APLL1_TUNER_CON0 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL2_TUNER_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "APLL2_TUNER_CON0 = 0x%x\n", value);
#endif
	regmap_read(afe->regmap, AUDIO_TOP_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_ENGEN_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_ENGEN_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_ENGEN_CON0_USER1, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_ENGEN_CON0_USER1 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_ENGEN_CON0_USER2, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_ENGEN_CON0_USER2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SINEGEN_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SINEGEN_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SINEGEN_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SINEGEN_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SINEGEN_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SINEGEN_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SINEGEN_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SINEGEN_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_APLL1_TUNER_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_APLL1_TUNER_CFG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_APLL1_TUNER_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_APLL1_TUNER_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_APLL2_TUNER_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_APLL2_TUNER_CFG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_APLL2_TUNER_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_APLL2_TUNER_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_RG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_RG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_RG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_RG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_RG2, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_RG2 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_RG3, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_RG3 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_RG4, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_RG4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SPM_CONTROL_REQ, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SPM_CONTROL_REQ = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SPM_CONTROL_ACK, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SPM_CONTROL_ACK = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_ENGEN_CON0_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_ENGEN_CON0_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_USE_DEFAULT_DELSEL0, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_USE_DEFAULT_DELSEL0 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_USE_DEFAULT_DELSEL1, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_USE_DEFAULT_DELSEL1 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_USE_DEFAULT_DELSEL2, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_USE_DEFAULT_DELSEL2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONNSYS_I2S_IPM_VER_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONNSYS_I2S_IPM_VER_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONNSYS_I2S_MON_SEL, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONNSYS_I2S_MON_SEL = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONNSYS_I2S_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONNSYS_I2S_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONNSYS_I2S_CON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONNSYS_I2S_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_PCM0_INTF_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_PCM0_INTF_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_PCM0_INTF_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_PCM0_INTF_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_PCM_INTF_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_PCM_INTF_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_PCM1_INTF_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_PCM1_INTF_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_PCM1_INTF_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_PCM1_INTF_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_PCM_TOP_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_PCM_TOP_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_DSP_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_DSP_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_DSP2_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_DSP2_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_SCP_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_SCP_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ_MCU_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_DSP_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ_MCU_DSP_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_DSP2_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ_MCU_DSP2_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_SCP_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ_MCU_SCP_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_STATUS, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_STATUS = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_STATUS, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ_MCU_STATUS = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CLR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_CLR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_MISS_CLR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_MISS_CLR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_CLR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ_MCU_CLR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_MISS_CLR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ_MCU_MISS_CLR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ0_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ0_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ0_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ0_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ1_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ1_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ1_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ1_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ2_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ2_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ2_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ2_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ3_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ3_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ3_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ3_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ4_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ4_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ4_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ4_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ5_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ5_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ5_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ5_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ6_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ6_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ6_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ6_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ7_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ7_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ7_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ7_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ8_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ8_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ8_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ8_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ9_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ9_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ9_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ9_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ10_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ10_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ10_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ10_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ11_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ11_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ11_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ11_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ12_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ12_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ12_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ12_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ13_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ13_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ13_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ13_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ14_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ14_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ14_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ14_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ15_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ15_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ15_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ15_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ16_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ16_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ16_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ16_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ17_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ17_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ17_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ17_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ18_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ18_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ18_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ18_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ19_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ19_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ19_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ19_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ20_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ20_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ20_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ20_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ21_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ21_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ21_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ21_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ22_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ22_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ22_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ22_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ23_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ23_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ23_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ23_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ24_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ24_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ24_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ24_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ25_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ25_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ25_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ25_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ26_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ26_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ26_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ26_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_CON_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ_MCU_CON_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ0_MCU_CNT, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ0_MCU_CNT = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_MON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_MON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ0_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ0_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ1_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ1_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ2_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ2_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ3_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ3_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ4_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ4_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ5_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ5_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ6_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ6_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ7_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ7_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ8_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ8_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ9_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ9_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ10_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ10_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ11_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ11_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ12_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ12_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ13_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ13_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ14_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ14_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ15_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ15_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ16_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ16_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ17_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ17_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ18_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ18_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ19_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ19_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ20_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ20_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ21_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ21_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ22_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ22_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ23_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ23_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ24_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ24_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ25_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ25_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ26_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ26_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ0_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ0_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN0_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN0_CON1_R, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN0_CON1_R = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN0_CON1_L, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN0_CON1_L = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN0_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN0_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN0_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN0_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN0_CUR_R, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN0_CUR_R = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN0_CUR_L, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN0_CUR_L = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN1_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CON1_R, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN1_CON1_R = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CON1_L, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN1_CON1_L = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN1_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN1_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CUR_R, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN1_CUR_R = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CUR_L, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN1_CUR_L = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CON1_R, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN2_CON1_R = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CON1_L, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN2_CON1_L = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN2_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN2_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CUR_R, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN2_CUR_R = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CUR_L, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN2_CUR_L = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN3_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN3_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN3_CON1_R, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN3_CON1_R = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN3_CON1_L, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN3_CON1_L = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN3_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN3_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN3_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN3_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN3_CUR_R, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN3_CUR_R = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN3_CUR_L, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN3_CUR_L = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_STF_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_STF_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_STF_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_STF_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_STF_COEFF, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_STF_COEFF = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_STF_GAIN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_STF_GAIN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_STF_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_STF_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_STF_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_STF_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CM0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CM0_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CM0_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CM0_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CM0_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CM0_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CM1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CM1_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CM1_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CM1_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CM1_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CM1_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_SRC_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_SRC_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_SRC_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_DEBUG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_SRC_DEBUG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_DEBUG_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_SRC_DEBUG_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_SRC_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_SRC_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IIR_COEF_02_01, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_IIR_COEF_02_01 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IIR_COEF_04_03, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_IIR_COEF_04_03 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IIR_COEF_06_05, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_IIR_COEF_06_05 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IIR_COEF_08_07, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_IIR_COEF_08_07 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IIR_COEF_10_09, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_IIR_COEF_10_09 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_02_01, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_02_01 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_04_03, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_04_03 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_06_05, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_06_05 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_08_07, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_08_07 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_10_09, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_10_09 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_12_11, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_12_11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_14_13, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_14_13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_16_15, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_16_15 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_18_17, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_18_17 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_20_19, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_20_19 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_22_21, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_22_21 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_24_23, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_24_23 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_26_25, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_26_25 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_28_27, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_28_27 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_30_29, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_30_29 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_32_31, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_32_31 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_SRC_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_SRC_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_SRC_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_SRC_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_SRC_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_SRC_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_SRC_DEBUG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_SRC_DEBUG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_SRC_DEBUG_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_SRC_DEBUG_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_SRC_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_SRC_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_SRC_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_SRC_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_IIR_COEF_02_01, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_IIR_COEF_02_01 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_IIR_COEF_04_03, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_IIR_COEF_04_03 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_IIR_COEF_06_05, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_IIR_COEF_06_05 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_IIR_COEF_08_07, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_IIR_COEF_08_07 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_IIR_COEF_10_09, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_IIR_COEF_10_09 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_02_01, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_02_01 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_04_03, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_04_03 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_06_05, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_06_05 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_08_07, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_08_07 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_10_09, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_10_09 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_12_11, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_12_11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_14_13, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_14_13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_16_15, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_16_15 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_18_17, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_18_17 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_20_19, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_20_19 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_22_21, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_22_21 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_24_23, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_24_23 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_26_25, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_26_25 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_28_27, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_28_27 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_30_29, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_30_29 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_32_31, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_32_31 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IIR_COEF_02_01, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_IIR_COEF_02_01 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IIR_COEF_04_03, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_IIR_COEF_04_03 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IIR_COEF_06_05, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_IIR_COEF_06_05 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IIR_COEF_08_07, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_IIR_COEF_08_07 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IIR_COEF_10_09, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_IIR_COEF_10_09 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_02_01, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_02_01 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_04_03, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_04_03 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_06_05, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_06_05 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_08_07, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_08_07 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_10_09, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_10_09 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_12_11, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_12_11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_14_13, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_14_13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_16_15, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_16_15 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_18_17, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_18_17 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_20_19, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_20_19 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_22_21, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_22_21 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_24_23, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_24_23 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_26_25, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_26_25 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_28_27, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_28_27 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_30_29, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_30_29 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_32_31, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_32_31 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_SRC_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_SRC_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_SRC_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_SRC_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_SRC_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_SRC_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_SRC_DEBUG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_SRC_DEBUG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_SRC_DEBUG_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_SRC_DEBUG_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_SRC_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_SRC_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_SRC_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_SRC_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_IIR_COEF_02_01, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_IIR_COEF_02_01 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_IIR_COEF_04_03, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_IIR_COEF_04_03 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_IIR_COEF_06_05, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_IIR_COEF_06_05 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_IIR_COEF_08_07, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_IIR_COEF_08_07 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_IIR_COEF_10_09, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_IIR_COEF_10_09 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_02_01, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_02_01 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_04_03, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_04_03 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_06_05, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_06_05 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_08_07, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_08_07 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_10_09, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_10_09 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_12_11, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_12_11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_14_13, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_14_13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_16_15, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_16_15 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_18_17, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_18_17 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_20_19, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_20_19 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_22_21, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_22_21 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_24_23, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_24_23 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_26_25, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_26_25 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_28_27, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_28_27 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_30_29, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_30_29 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_32_31, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_32_31 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_PROXIMITY_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_PROXIMITY_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF_IPM_VER_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF_IPM_VER_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF_MON_SEL, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF_MON_SEL = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF0_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF0_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF0_TX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF0_TX_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF0_RX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF0_RX_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF0_RX_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF0_RX_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF0_RX_CFG2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF0_RX_CFG2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF1_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF1_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF1_TX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF1_TX_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF1_RX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF1_RX_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF1_RX_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF1_RX_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF1_RX_CFG2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF1_RX_CFG2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AUD_PAD_TOP_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_AUD_PAD_TOP_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AUD_PAD_TOP_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_AUD_PAD_TOP_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIFV4_TX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_MTKAIFV4_TX_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_MTKAIFV4_TX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_MTKAIFV4_TX_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIFV4_RX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_MTKAIFV4_RX_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIFV4_RX_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_MTKAIFV4_RX_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_MTKAIFV4_RX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_MTKAIFV4_RX_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_MTKAIFV4_RX_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_MTKAIFV4_RX_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIFV4_TX_SYNCWORD_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_MTKAIFV4_TX_SYNCWORD_CFG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIFV4_RX_SYNCWORD_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_MTKAIFV4_RX_SYNCWORD_CFG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIFV4_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_MTKAIFV4_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIFV4_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_MTKAIFV4_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_MTKAIFV4_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_MTKAIFV4_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN0_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN0_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN0_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN0_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN0_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN0_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN0_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN0_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN0_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN0_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN0_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN0_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN0_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN0_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN0_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN0_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN0_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN0_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN0_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN0_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN0_MON = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN1_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN1_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN1_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN1_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN1_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN1_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN1_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN1_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN1_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN1_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN1_MON = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN2_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN2_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN2_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN2_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN2_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN2_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN2_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN2_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN2_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN2_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN2_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN2_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN2_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN2_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN2_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN2_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN2_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN2_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN2_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN2_MON = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN4_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN4_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN4_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN4_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN4_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN4_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN4_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN4_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN4_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN4_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN4_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN4_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN4_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN4_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN4_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN4_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN4_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN4_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN4_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN4_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN4_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN4_MON = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN6_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN6_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN6_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN6_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN6_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN6_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN6_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN6_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN6_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN6_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN6_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN6_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN6_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN6_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN6_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN6_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN6_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN6_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN6_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN6_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN6_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN6_MON = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT0_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT0_MON = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT1_MON = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT2_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT2_MON = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT4_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT4_MON = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT6_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT6_MON = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT0_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT0_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT0_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT0_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT0_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT0_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT0_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT0_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT0_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT0_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT0_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT0_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT0_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT0_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT0_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT0_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT0_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT1_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT1_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT1_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT1_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT1_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT1_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT1_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT1_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT1_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT2_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT2_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT2_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT2_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT2_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT2_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT2_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT2_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT2_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT2_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT2_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT2_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT2_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT2_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT2_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT2_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT4_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT4_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT4_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT4_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT4_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT4_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT4_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT4_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT4_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT4_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT4_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT4_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT4_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT4_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT4_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT4_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT4_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT4_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT6_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT6_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT6_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT6_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT6_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT6_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT6_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT6_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT6_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT6_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT6_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT6_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT6_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT6_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT6_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT6_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT6_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT6_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_0_3_COWORK_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_0_3_COWORK_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_0_3_COWORK_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_0_3_COWORK_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_0_3_COWORK_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_0_3_COWORK_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_0_3_COWORK_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_0_3_COWORK_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_4_7_COWORK_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_4_7_COWORK_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_4_7_COWORK_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_4_7_COWORK_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_4_7_COWORK_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_4_7_COWORK_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_4_7_COWORK_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_4_7_COWORK_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DPTX_CON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DPTX_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DPTX_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DPTX_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_TDM_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_TDM_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_TDM_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_TDM_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_TDM_TOP_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_TDM_TOP_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_HDMI_CONN0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_HDMI_CONN0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN004_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN004_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN004_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN004_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN004_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN004_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN004_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN004_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN004_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN004_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN004_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN004_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN005_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN005_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN005_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN005_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN005_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN005_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN005_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN005_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN005_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN005_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN005_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN005_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN006_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN006_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN006_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN006_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN006_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN006_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN006_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN006_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN006_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN006_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN006_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN006_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN007_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN007_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN007_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN007_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN007_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN007_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN007_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN007_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN007_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN007_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN007_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN007_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN008_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN008_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN008_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN008_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN008_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN008_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN008_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN008_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN008_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN008_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN008_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN008_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN009_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN009_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN009_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN009_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN009_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN009_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN009_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN009_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN009_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN009_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN009_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN009_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN010_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN010_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN010_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN010_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN010_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN010_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN010_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN010_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN010_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN010_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN010_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN010_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN011_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN011_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN011_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN011_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN011_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN011_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN011_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN011_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN011_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN011_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN011_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN011_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN012_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN012_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN012_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN012_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN012_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN012_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN012_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN012_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN012_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN012_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN012_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN012_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN014_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN014_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN014_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN014_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN014_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN014_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN014_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN014_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN014_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN014_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN014_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN014_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN015_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN015_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN015_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN015_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN015_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN015_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN015_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN015_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN015_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN015_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN015_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN015_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN016_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN016_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN016_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN016_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN016_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN016_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN016_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN016_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN016_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN016_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN016_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN016_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN017_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN017_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN017_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN017_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN017_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN017_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN017_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN017_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN017_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN017_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN017_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN017_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN018_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN018_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN018_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN018_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN018_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN018_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN018_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN018_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN018_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN018_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN018_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN018_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN019_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN019_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN019_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN019_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN019_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN019_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN019_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN019_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN019_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN019_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN019_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN019_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN020_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN020_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN020_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN020_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN020_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN020_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN020_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN020_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN020_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN020_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN020_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN020_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN021_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN021_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN021_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN021_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN021_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN021_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN021_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN021_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN021_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN021_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN021_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN021_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN022_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN022_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN022_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN022_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN022_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN022_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN022_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN022_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN022_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN022_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN022_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN022_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN023_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN023_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN023_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN023_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN023_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN023_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN023_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN023_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN023_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN023_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN023_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN023_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN024_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN024_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN024_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN024_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN024_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN024_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN024_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN024_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN024_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN024_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN024_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN024_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN025_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN025_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN025_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN025_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN025_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN025_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN025_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN025_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN025_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN025_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN025_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN025_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN026_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN026_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN026_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN026_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN026_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN026_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN026_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN026_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN026_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN026_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN026_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN026_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN027_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN027_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN027_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN027_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN027_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN027_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN027_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN027_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN027_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN027_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN027_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN027_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN028_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN028_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN028_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN028_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN028_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN028_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN028_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN028_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN028_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN028_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN028_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN028_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN029_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN029_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN029_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN029_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN029_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN029_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN029_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN029_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN029_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN029_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN029_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN029_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN030_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN030_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN030_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN030_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN030_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN030_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN030_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN030_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN030_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN030_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN030_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN030_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN031_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN031_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN031_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN031_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN031_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN031_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN031_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN031_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN031_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN031_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN031_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN031_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN032_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN032_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN032_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN032_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN032_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN032_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN032_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN032_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN032_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN032_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN032_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN032_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN033_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN033_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN033_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN033_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN033_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN033_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN033_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN033_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN033_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN033_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN033_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN033_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN034_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN034_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN034_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN034_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN034_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN034_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN034_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN034_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN034_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN034_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN034_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN034_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN035_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN035_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN035_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN035_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN035_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN035_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN035_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN035_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN035_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN035_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN035_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN035_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN036_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN036_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN036_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN036_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN036_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN036_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN036_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN036_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN036_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN036_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN036_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN036_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN037_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN037_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN037_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN037_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN037_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN037_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN037_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN037_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN037_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN037_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN037_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN037_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN038_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN038_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN038_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN038_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN038_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN038_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN038_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN038_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN038_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN038_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN038_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN038_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN039_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN039_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN039_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN039_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN039_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN039_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN039_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN039_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN039_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN039_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN039_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN039_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN040_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN040_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN040_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN040_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN040_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN040_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN040_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN040_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN040_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN040_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN040_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN040_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN041_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN041_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN041_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN041_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN041_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN041_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN041_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN041_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN041_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN041_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN041_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN041_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN042_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN042_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN042_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN042_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN042_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN042_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN042_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN042_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN042_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN042_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN042_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN042_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN043_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN043_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN043_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN043_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN043_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN043_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN043_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN043_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN043_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN043_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN043_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN043_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN044_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN044_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN044_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN044_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN044_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN044_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN044_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN044_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN044_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN044_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN044_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN044_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN045_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN045_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN045_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN045_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN045_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN045_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN045_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN045_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN045_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN045_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN045_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN045_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN046_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN046_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN046_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN046_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN046_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN046_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN046_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN046_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN046_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN046_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN046_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN046_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN047_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN047_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN047_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN047_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN047_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN047_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN047_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN047_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN047_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN047_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN047_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN047_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN048_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN048_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN048_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN048_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN048_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN048_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN048_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN048_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN048_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN048_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN048_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN048_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN049_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN049_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN049_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN049_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN049_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN049_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN049_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN049_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN049_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN049_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN049_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN049_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN050_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN050_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN050_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN050_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN050_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN050_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN050_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN050_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN050_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN050_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN050_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN050_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN051_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN051_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN051_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN051_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN051_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN051_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN051_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN051_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN051_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN051_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN051_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN051_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN052_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN052_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN052_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN052_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN052_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN052_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN052_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN052_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN052_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN052_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN052_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN052_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN053_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN053_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN053_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN053_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN053_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN053_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN053_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN053_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN053_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN053_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN053_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN053_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN054_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN054_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN054_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN054_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN054_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN054_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN054_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN054_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN054_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN054_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN054_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN054_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN055_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN055_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN055_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN055_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN055_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN055_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN055_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN055_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN055_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN055_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN055_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN055_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN056_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN056_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN056_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN056_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN056_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN056_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN056_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN056_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN056_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN056_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN056_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN056_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN057_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN057_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN057_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN057_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN057_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN057_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN057_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN057_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN057_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN057_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN057_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN057_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN058_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN058_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN058_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN058_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN058_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN058_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN058_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN058_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN058_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN058_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN058_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN058_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN059_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN059_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN059_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN059_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN059_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN059_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN059_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN059_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN059_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN059_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN059_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN059_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN060_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN060_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN060_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN060_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN060_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN060_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN060_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN060_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN060_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN060_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN060_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN060_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN061_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN061_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN061_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN061_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN061_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN061_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN061_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN061_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN061_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN061_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN061_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN061_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN062_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN062_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN062_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN062_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN062_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN062_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN062_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN062_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN062_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN062_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN062_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN062_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN063_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN063_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN063_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN063_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN063_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN063_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN063_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN063_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN063_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN063_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN063_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN063_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN066_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN066_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN066_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN066_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN066_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN066_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN066_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN066_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN066_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN066_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN066_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN066_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN067_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN067_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN067_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN067_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN067_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN067_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN067_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN067_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN067_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN067_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN067_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN067_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN068_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN068_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN068_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN068_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN068_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN068_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN068_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN068_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN068_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN068_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN068_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN068_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN069_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN069_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN069_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN069_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN069_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN069_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN069_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN069_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN069_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN069_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN069_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN069_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN096_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN096_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN096_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN096_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN096_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN096_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN096_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN096_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN096_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN096_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN096_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN096_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN097_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN097_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN097_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN097_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN097_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN097_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN097_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN097_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN097_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN097_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN097_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN097_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN098_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN098_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN098_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN098_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN098_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN098_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN098_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN098_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN098_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN098_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN098_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN098_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN099_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN099_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN099_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN099_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN099_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN099_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN099_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN099_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN099_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN099_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN099_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN099_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN100_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN100_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN100_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN100_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN100_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN100_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN100_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN100_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN100_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN100_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN100_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN100_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN102_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN102_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN102_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN102_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN102_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN102_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN102_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN102_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN102_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN102_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN102_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN102_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN103_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN103_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN103_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN103_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN103_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN103_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN103_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN103_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN103_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN103_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN103_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN103_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN104_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN104_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN104_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN104_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN104_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN104_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN104_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN104_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN104_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN104_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN104_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN104_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN105_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN105_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN105_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN105_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN105_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN105_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN105_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN105_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN105_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN105_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN105_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN105_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN106_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN106_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN106_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN106_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN106_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN106_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN106_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN106_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN106_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN106_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN106_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN106_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN108_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN108_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN108_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN108_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN108_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN108_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN108_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN108_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN108_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN108_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN108_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN108_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN109_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN109_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN109_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN109_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN109_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN109_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN109_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN109_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN109_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN109_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN109_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN109_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN110_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN110_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN110_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN110_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN110_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN110_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN110_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN110_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN110_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN110_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN110_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN110_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN111_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN111_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN111_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN111_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN111_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN111_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN111_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN111_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN111_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN111_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN111_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN111_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN112_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN112_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN112_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN112_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN112_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN112_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN112_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN112_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN112_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN112_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN112_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN112_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN113_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN113_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN113_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN113_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN113_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN113_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN113_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN113_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN113_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN113_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN113_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN113_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN116_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN116_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN116_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN116_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN116_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN116_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN116_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN116_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN116_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN116_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN116_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN116_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN117_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN117_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN117_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN117_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN117_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN117_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN117_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN117_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN117_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN117_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN117_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN117_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN118_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN118_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN118_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN118_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN118_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN118_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN118_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN118_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN118_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN118_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN118_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN118_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN119_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN119_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN119_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN119_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN119_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN119_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN119_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN119_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN119_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN119_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN119_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN119_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN120_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN120_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN120_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN120_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN120_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN120_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN120_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN120_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN120_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN120_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN120_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN120_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN121_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN121_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN121_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN121_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN121_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN121_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN121_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN121_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN121_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN121_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN121_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN121_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN122_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN122_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN122_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN122_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN122_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN122_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN122_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN122_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN122_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN122_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN122_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN122_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN123_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN123_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN123_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN123_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN123_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN123_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN123_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN123_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN123_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN123_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN123_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN123_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN148_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN148_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN148_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN148_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN148_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN148_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN148_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN148_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN148_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN148_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN148_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN148_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN149_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN149_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN149_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN149_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN149_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN149_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN149_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN149_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN149_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN149_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN149_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN149_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN180_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN180_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN180_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN180_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN180_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN180_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN180_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN180_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN180_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN180_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN180_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN180_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN181_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN181_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN181_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN181_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN181_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN181_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN181_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN181_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN181_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN181_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN181_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN181_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN182_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN182_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN182_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN182_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN182_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN182_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN182_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN182_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN182_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN182_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN182_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN182_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN183_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN183_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN183_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN183_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN183_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN183_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN183_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN183_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN183_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN183_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN183_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN183_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN184_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN184_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN184_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN184_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN184_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN184_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN184_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN184_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN184_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN184_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN184_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN184_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN185_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN185_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN185_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN185_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN185_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN185_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN185_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN185_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN185_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN185_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN185_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN185_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN186_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN186_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN186_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN186_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN186_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN186_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN186_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN186_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN186_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN186_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN186_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN186_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN187_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN187_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN187_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN187_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN187_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN187_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN187_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN187_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN187_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN187_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN187_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN187_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_MON_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_MON_CFG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_MON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_MON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_MON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_MON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_MON4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_MON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_MON5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_MON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_RS_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_RS_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_RS_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_RS_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_RS_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_RS_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_RS_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_RS_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_RS_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_RS_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_RS_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_RS_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_DI_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_DI_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_DI_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_DI_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_DI_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_DI_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_DI_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_DI_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_DI_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_DI_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_DI_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_DI_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_16BIT_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_16BIT_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_16BIT_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_16BIT_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_16BIT_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_16BIT_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_16BIT_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_16BIT_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_16BIT_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_16BIT_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_16BIT_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_16BIT_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_24BIT_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_24BIT_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_24BIT_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_24BIT_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_24BIT_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_24BIT_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_24BIT_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_24BIT_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_24BIT_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_24BIT_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_24BIT_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_24BIT_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CBIP_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_SLV_DECODER_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CBIP_SLV_DECODER_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_SLV_DECODER_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CBIP_SLV_DECODER_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_SLV_MUX_MON_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CBIP_SLV_MUX_MON_CFG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_SLV_MUX_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CBIP_SLV_MUX_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_SLV_MUX_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CBIP_SLV_MUX_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MEMIF_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_ONE_HEART, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MEMIF_ONE_HEART = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL0_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL0_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL0_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL0_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL0_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL0_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL0_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL0_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL0_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL0_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL0_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL0_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL0_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL0_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL0_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL0_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL0_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL1_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL1_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL1_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL1_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL1_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL1_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL1_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL1_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL1_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL2_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL2_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL2_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL2_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL2_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL2_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL2_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL2_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL3_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL3_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL3_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL3_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL3_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL3_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL3_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL3_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL3_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL4_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL4_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL4_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL4_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL4_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL4_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL4_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL4_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL4_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL5_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL5_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL5_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL5_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL5_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL5_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL5_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL5_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL5_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL6_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL6_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL6_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL6_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL6_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL6_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL6_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL6_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL6_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL7_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL7_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL7_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL7_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL7_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL7_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL7_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL7_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL7_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL8_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL8_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL8_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL8_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL8_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL8_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL8_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL8_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL8_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL23_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL23_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL23_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL23_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL23_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL23_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL23_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL23_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL23_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL23_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL23_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL23_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL23_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL23_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL23_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL23_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL23_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL23_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL24_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL24_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL24_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL24_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL24_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL24_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL24_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL24_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL24_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL24_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL24_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL24_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL24_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL24_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL24_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL24_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL24_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL24_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL25_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL25_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL25_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL25_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL25_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL25_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL25_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL25_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL25_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL25_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL25_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL25_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL25_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL25_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL25_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL25_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL25_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL25_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL0_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL0_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL0_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL0_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL0_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL0_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL0_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL0_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL0_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL0_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL0_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL0_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL0_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL1_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL1_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL1_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL1_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL1_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL1_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL1_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL1_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL1_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL1_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL1_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL1_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL1_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL2_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL2_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL2_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL2_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL2_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL2_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL3_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL3_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL3_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL3_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL3_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL3_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL3_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL4_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL4_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL4_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL4_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL4_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL4_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL4_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL5_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL5_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL5_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL5_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL5_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL5_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL5_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL6_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL6_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL6_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL6_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL6_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL6_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL6_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL7_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL7_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL7_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL7_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL7_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL7_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL7_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL7_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL7_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL7_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL7_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL7_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL7_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL7_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL8_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL8_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL8_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL8_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL8_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL8_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL8_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL8_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL8_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL8_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL8_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL8_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL8_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL8_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL9_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL9_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL9_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL9_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL9_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL9_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL9_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL9_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL9_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL9_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL9_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL9_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL9_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL9_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL10_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL10_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL10_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL10_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL10_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL10_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL10_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL10_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL10_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL10_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL10_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL10_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL10_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL10_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL24_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL24_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL24_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL24_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL24_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL24_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL24_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL24_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL24_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL24_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL24_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL24_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL24_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL24_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL25_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL25_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL25_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL25_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL25_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL25_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL25_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL25_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL25_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL25_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL25_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL25_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL25_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL25_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM0_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM0_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM0_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM0_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM0_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM0_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM0_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM0_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM0_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM0_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM0_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM0_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM0_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM1_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM1_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM1_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM1_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM1_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM1_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM1_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM1_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM1_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM1_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM1_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM1_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM1_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN0_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN0_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN0_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN0_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN0_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN0_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN0_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN1_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN1_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN1_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN1_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN1_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN1_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN1_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN2_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN2_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN2_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN2_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN2_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN2_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN2_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN2_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN2_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN2_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN2_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN2_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN4_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN4_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN4_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN4_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN4_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN4_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN4_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN4_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN4_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN4_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN4_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN4_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN4_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN4_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN6_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN6_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN6_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN6_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN6_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN6_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN6_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN6_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN6_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN6_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN6_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN6_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN6_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN6_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_HDMI_OUT_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_HDMI_OUT_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_HDMI_OUT_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_HDMI_OUT_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_HDMI_OUT_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_HDMI_OUT_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_HDMI_OUT_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_HDMI_OUT_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_HDMI_OUT_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_HDMI_OUT_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_HDMI_OUT_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_HDMI_OUT_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_HDMI_OUT_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_HDMI_OUT_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SRAM_BOUND, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SRAM_BOUND = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SECURE_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SECURE_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_SECURE_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_SECURE_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_SECURE_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_SECURE_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_SECURE_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_SECURE_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_SECURE_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_SECURE_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_PROT_SIDEBAND0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_PROT_SIDEBAND0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_PROT_SIDEBAND1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_PROT_SIDEBAND1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_PROT_SIDEBAND2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_PROT_SIDEBAND2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_PROT_SIDEBAND3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_PROT_SIDEBAND3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_DOMAIN_SIDEBAND0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_DOMAIN_SIDEBAND1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_DOMAIN_SIDEBAND2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_DOMAIN_SIDEBAND3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_DOMAIN_SIDEBAND4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_DOMAIN_SIDEBAND5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_DOMAIN_SIDEBAND6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND7, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_DOMAIN_SIDEBAND7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND8, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_DOMAIN_SIDEBAND8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND9, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_DOMAIN_SIDEBAND9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_PROT_SIDEBAND0_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_PROT_SIDEBAND0_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_PROT_SIDEBAND1_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_PROT_SIDEBAND1_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_PROT_SIDEBAND2_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_PROT_SIDEBAND2_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_PROT_SIDEBAND3_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_PROT_SIDEBAND3_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND0_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DOMAIN_SIDEBAND0_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND1_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DOMAIN_SIDEBAND1_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND2_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DOMAIN_SIDEBAND2_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND3_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DOMAIN_SIDEBAND3_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND4_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DOMAIN_SIDEBAND4_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND5_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DOMAIN_SIDEBAND5_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND6_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DOMAIN_SIDEBAND6_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND7_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DOMAIN_SIDEBAND7_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND8_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DOMAIN_SIDEBAND8_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DOMAIN_SIDEBAND9_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DOMAIN_SIDEBAND9_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_CONN0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SECURE_CONN0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_CONN_ETDM0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SECURE_CONN_ETDM0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_CONN_ETDM1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SECURE_CONN_ETDM1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_CONN_ETDM2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SECURE_CONN_ETDM2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_SRAM_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SECURE_SRAM_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SECURE_SRAM_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SECURE_SRAM_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_CONN_INPUT_MASK0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_CONN_INPUT_MASK0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_CONN_INPUT_MASK1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_CONN_INPUT_MASK1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_CONN_INPUT_MASK2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_CONN_INPUT_MASK2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_CONN_INPUT_MASK3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_CONN_INPUT_MASK3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_CONN_INPUT_MASK4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_CONN_INPUT_MASK4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_CONN_INPUT_MASK5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_CONN_INPUT_MASK5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_CONN_INPUT_MASK6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_CONN_INPUT_MASK6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_CONN_INPUT_MASK7, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_CONN_INPUT_MASK7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_NON_SE_CONN_INPUT_MASK0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_NON_SE_CONN_INPUT_MASK0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_NON_SE_CONN_INPUT_MASK1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_NON_SE_CONN_INPUT_MASK1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_NON_SE_CONN_INPUT_MASK2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_NON_SE_CONN_INPUT_MASK2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_NON_SE_CONN_INPUT_MASK3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_NON_SE_CONN_INPUT_MASK3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_NON_SE_CONN_INPUT_MASK4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_NON_SE_CONN_INPUT_MASK4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_NON_SE_CONN_INPUT_MASK5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_NON_SE_CONN_INPUT_MASK5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_NON_SE_CONN_INPUT_MASK6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_NON_SE_CONN_INPUT_MASK6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_NON_SE_CONN_INPUT_MASK7, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_NON_SE_CONN_INPUT_MASK7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_CONN_OUTPUT_SEL0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_CONN_OUTPUT_SEL0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_CONN_OUTPUT_SEL1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_CONN_OUTPUT_SEL1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_CONN_OUTPUT_SEL2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_CONN_OUTPUT_SEL2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_CONN_OUTPUT_SEL3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_CONN_OUTPUT_SEL3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_CONN_OUTPUT_SEL4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_CONN_OUTPUT_SEL4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_CONN_OUTPUT_SEL5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_CONN_OUTPUT_SEL5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_CONN_OUTPUT_SEL6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_CONN_OUTPUT_SEL6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_CONN_OUTPUT_SEL7, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_CONN_OUTPUT_SEL7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_PCM0_INTF_CON1_MASK_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_PCM0_INTF_CON1_MASK_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_PCM0_INTF_CON0_MASK_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_PCM0_INTF_CON0_MASK_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONNSYS_I2S_CON_MASK_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONNSYS_I2S_CON_MASK_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_TDM_CON2_MASK_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_TDM_CON2_MASK_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF0_CFG0_MASK_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF0_CFG0_MASK_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF1_CFG0_MASK_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF1_CFG0_MASK_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_CON0_MASK_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_SRC_CON0_MASK_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_SRC_CON0_MASK_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_SRC_CON0_MASK_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON11, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON12, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON13, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON14, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON11, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON12, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON13, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON14, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON11, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON12, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON13, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON14, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON11, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON12, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON13, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON14, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON11, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON12, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON13, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON14, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_IP_VERSION = 0x%x\n", value);

	return n;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
static ssize_t mt6989_debugfs_read(struct file *file, char __user *buf,
				   size_t count, loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	const int size = AFE_SYS_DEBUG_SIZE;
	char *buffer = NULL; /* for reduce kernel stack */
	int n = 0, ret = 0;

	buffer = kmalloc(size, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	n = mt6989_debug_read_reg(buffer, size, afe);

	ret = simple_read_from_buffer(buf, count, pos, buffer, n);
	kfree(buffer);
	return ret;
}

static const struct mtk_afe_debug_cmd mt6989_debug_cmds[] = {
	MTK_AFE_DBG_CMD("write_reg", mtk_afe_debug_write_reg),
	{}
};

static const struct file_operations mt6989_debugfs_ops = {
	.open = mtk_afe_debugfs_open,
	.write = mtk_afe_debugfs_write,
	.read = mt6989_debugfs_read,
};
#endif

static int mt6989_dai_memif_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mt6989_memif_dai_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mt6989_memif_dai_driver);

	dai->controls = mt6989_pcm_kcontrols;
	dai->num_controls = ARRAY_SIZE(mt6989_pcm_kcontrols);
	dai->dapm_widgets = mt6989_memif_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mt6989_memif_widgets);
	dai->dapm_routes = mt6989_memif_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mt6989_memif_routes);
	return 0;
}

typedef int (*dai_register_cb)(struct mtk_base_afe *);
static const dai_register_cb dai_register_cbs[] = {
	mt6989_dai_adda_register,
	mt6989_dai_i2s_register,
	mt6989_dai_hw_gain_register,
	mt6989_dai_src_register,
	mt6989_dai_pcm_register,
	mt6989_dai_tdm_register,
	mt6989_dai_hostless_register,
	mt6989_dai_memif_register,
};

static int mt6989_afe_pcm_dev_probe(struct platform_device *pdev)
{
	int ret, i;
	unsigned int tmp_reg = 0;
#if !defined(CONFIG_FPGA_EARLY_PORTING) || defined(FORCE_FPGA_ENABLE_IRQ)
	int irq_id;
#endif
	struct mtk_base_afe *afe;
	struct mt6989_afe_private *afe_priv;
	struct resource *res;
	struct device *dev;
#if !defined(SKIP_SMCC_SB)
	struct arm_smccc_res smccc_res;
#endif
	struct device_node *np;
	struct platform_device *pmic_pdev = NULL;
	struct regmap *map;

	pr_info("+%s()\n", __func__);

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));
	if (ret)
		return ret;

	afe = devm_kzalloc(&pdev->dev, sizeof(*afe), GFP_KERNEL);
	if (!afe)
		return -ENOMEM;

	platform_set_drvdata(pdev, afe);
	mt6989_set_local_afe(afe);

	afe->platform_priv = devm_kzalloc(&pdev->dev, sizeof(*afe_priv),
					  GFP_KERNEL);
	if (!afe->platform_priv)
		return -ENOMEM;

	afe_priv = afe->platform_priv;

	afe->dev = &pdev->dev;
	dev = afe->dev;

	/* init audio related clock */
	ret = mt6989_init_clock(afe);
	if (ret) {
		dev_info(dev, "init clock error: %d\n", ret);
		return ret;
	}

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev))
		goto err_pm_disable;

	/* Audio device is part of genpd.
	 * Set audio as syscore device to prevent
	 * genpd automatically power off audio
	 * device when suspend
	 */
	dev_pm_syscore_device(&pdev->dev, true);

	/* regmap init */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	afe->base_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(afe->base_addr))
		return PTR_ERR(afe->base_addr);

	/* enable clock for regcache get default value from hw */
	pm_runtime_get_sync(&pdev->dev);

	afe->regmap = devm_regmap_init_mmio(&pdev->dev, afe->base_addr,
					    &mt6989_afe_regmap_config);
	if (IS_ERR(afe->regmap))
		return PTR_ERR(afe->regmap);

	/* IPM2.0 clock flow, need debug */

	regmap_read(afe->regmap, AFE_IRQ_MCU_EN, &tmp_reg);
	regmap_write(afe->regmap, AFE_IRQ_MCU_EN, 0xffffffff);
	regmap_read(afe->regmap, AFE_IRQ_MCU_EN, &tmp_reg);
	/* IPM2.0 clock flow, need debug */

	pm_runtime_put_sync(&pdev->dev);

	regcache_cache_only(afe->regmap, true);
	regcache_mark_dirty(afe->regmap);

	/* init gpio */
	ret = mt6989_afe_gpio_init(afe);
	if (ret)
		dev_info(dev, "init gpio error\n");

	/* init sram */
	afe->sram = devm_kzalloc(&pdev->dev, sizeof(struct mtk_audio_sram),
				 GFP_KERNEL);
	if (!afe->sram)
		return -ENOMEM;

	ret = mtk_audio_sram_init(dev, afe->sram, &mt6989_sram_ops);
	if (ret)
		return ret;

	/* init memif */
	/* IPM2.0 no need banding */
	afe->is_memif_bit_banding = 0;
	afe->memif_32bit_supported = 1;
	afe->memif_size = MT6989_MEMIF_NUM;
	afe->memif = devm_kcalloc(dev, afe->memif_size, sizeof(*afe->memif),
				  GFP_KERNEL);

	if (!afe->memif)
		return -ENOMEM;

	for (i = 0; i < afe->memif_size; i++) {
		afe->memif[i].data = &memif_data[i];
		afe->memif[i].irq_usage = memif_irq_usage[i];
		afe->memif[i].const_irq = 1;
	}
	afe->memif[MT6989_DEEP_MEMIF].ack = mtk_sp_clean_written_buffer_ack;
	afe->memif[MT6989_FAST_MEMIF].fast_palyback = 1;

	mutex_init(&afe->irq_alloc_lock);       /* needed when dynamic irq */

	/* init irq */
	afe->irqs_size = MT6989_IRQ_NUM;
	afe->irqs = devm_kcalloc(dev, afe->irqs_size, sizeof(*afe->irqs),
				 GFP_KERNEL);

	if (!afe->irqs)
		return -ENOMEM;

	for (i = 0; i < afe->irqs_size; i++)
		afe->irqs[i].irq_data = &irq_data[i];

#if !defined(CONFIG_FPGA_EARLY_PORTING) || defined(FORCE_FPGA_ENABLE_IRQ)
	/* request irq */
	irq_id = platform_get_irq(pdev, 0);
	if (irq_id <= 0) {
		dev_info(dev, "%pOFn no irq found\n", dev->of_node);
		return irq_id < 0 ? irq_id : -ENXIO;
	}
	ret = devm_request_irq(dev, irq_id, mt6989_afe_irq_handler,
			       IRQF_TRIGGER_NONE,
			       "Afe_ISR_Handle", (void *)afe);
	if (ret) {
		dev_info(dev, "could not request_irq for Afe_ISR_Handle\n");
		return ret;
	}
	ret = enable_irq_wake(irq_id);
	if (ret < 0)
		dev_info(dev, "enable_irq_wake %d err: %d\n", irq_id, ret);
#endif

#if !defined(SKIP_SMCC_SB)
	/* init arm_smccc_smc call */
	arm_smccc_smc(MTK_SIP_AUDIO_CONTROL, MTK_AUDIO_SMC_OP_INIT,
		      0, 0, 0, 0, 0, 0, &smccc_res);
#endif

	/* init sub_dais */
	INIT_LIST_HEAD(&afe->sub_dais);

	for (i = 0; i < ARRAY_SIZE(dai_register_cbs); i++) {
		ret = dai_register_cbs[i](afe);
		if (ret) {
			dev_info(afe->dev, "dai register i %d fail, ret %d\n",
				 i, ret);
			goto err_pm_disable;
		}
	}

	/* init dai_driver and component_driver */
	ret = mtk_afe_combine_sub_dai(afe);
	if (ret) {
		dev_info(afe->dev, "mtk_afe_combine_sub_dai fail, ret %d\n",
			 ret);
		goto err_pm_disable;
	}

	/* others */
	afe->mtk_afe_hardware = &mt6989_afe_hardware;
	afe->memif_fs = mt6989_memif_fs;
	afe->irq_fs = mt6989_irq_fs;
	afe->get_dai_fs = mt6989_get_dai_fs;
	afe->get_memif_pbuf_size = mt6989_get_memif_pbuf_size;

	afe->runtime_resume = mt6989_afe_runtime_resume;
	afe->runtime_suspend = mt6989_afe_runtime_suspend;

	afe->request_dram_resource = mt6989_afe_dram_request;
	afe->release_dram_resource = mt6989_afe_dram_release;

	afe->copy = mt6989_afe_pcm_copy;

	/* IPM2.0: No need */
	/* afe->is_scp_sema_support = 1; */

#if IS_ENABLED(CONFIG_DEBUG_FS)
	/* debugfs */
	afe->debug_cmds = mt6989_debug_cmds;
	afe->debugfs = debugfs_create_file("mtksocaudio",
					   S_IFREG | 0444, NULL,
					   afe, &mt6989_debugfs_ops);
#endif
	/* usb audio offload hook */
	ret = mtk_audio_usb_offload_afe_hook(afe);
	if (ret)
		dev_info(dev, "Hook usb offload interface err: %d\n", ret);

	/* register component */
	ret = devm_snd_soc_register_component(&pdev->dev,
					      &mt6989_afe_component,
					      afe->dai_drivers,
					      afe->num_dai_drivers);
	if (ret) {
		dev_info(dev, "afe component err: %d\n", ret);
		goto err_pm_disable;
	}
	/* get pmic6363 regmap */
	np = of_find_node_by_name(NULL, "pmic");
	if (!np) {
		dev_info(&pdev->dev, "pmic node not found\n");
		goto err_find_pmic;
	}

	pmic_pdev = of_find_device_by_node(np->child);
	if (!pmic_pdev) {
		dev_info(&pdev->dev, "pmic child device not found\n");
		goto err_find_pmic;
	}

	map = dev_get_regmap(pmic_pdev->dev.parent, NULL);
	afe_priv->pmic_regmap = map;
err_find_pmic:
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
	audio_set_dsp_afe(afe);
#endif
#if IS_ENABLED(CONFIG_MTK_ULTRASND_PROXIMITY) && !defined(SKIP_SB_ULTRA)
	ultra_set_dsp_afe(afe);
#endif
	return 0;

err_pm_disable:
	pm_runtime_disable(&pdev->dev);

	return ret;
}

static int mt6989_afe_pcm_dev_remove(struct platform_device *pdev)
{
	struct mtk_base_afe *afe = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		mt6989_afe_runtime_suspend(&pdev->dev);

	/* usb audio offload unhook */
	mtk_audio_usb_offload_afe_hook(NULL);

	/* disable afe clock */
	mt6989_afe_disable_clock(afe);
	return 0;
}

static const struct of_device_id mt6989_afe_pcm_dt_match[] = {
	{ .compatible = "mediatek,mt6989-sound", },
	{},
};
MODULE_DEVICE_TABLE(of, mt6989_afe_pcm_dt_match);

static const struct dev_pm_ops mt6989_afe_pm_ops = {
	SET_RUNTIME_PM_OPS(mt6989_afe_runtime_suspend,
			   mt6989_afe_runtime_resume, NULL)
};

static struct platform_driver mt6989_afe_pcm_driver = {
	.driver = {
		.name = "mt6989-audio",
		.of_match_table = mt6989_afe_pcm_dt_match,
#if IS_ENABLED(CONFIG_PM)
		.pm = &mt6989_afe_pm_ops,
#endif
	},
	.probe = mt6989_afe_pcm_dev_probe,
	.remove = mt6989_afe_pcm_dev_remove,
};

module_platform_driver(mt6989_afe_pcm_driver);

MODULE_DESCRIPTION("Mediatek ALSA SoC AFE platform driver for 6989");
MODULE_AUTHOR("Shane Chien <shane.chien@mediatek.com>");
MODULE_LICENSE("GPL");
