/*
 * linux/sound/soc/pxa/mmp-pcm-hostless.c
 *
 * Copyright (C) 2014 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/dma-mapping.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/pcm.h>

#define STUB_RATES	SNDRV_PCM_RATE_8000_192000
#define STUB_FORMATS	(SNDRV_PCM_FMTBIT_S8 | \
			SNDRV_PCM_FMTBIT_U8 | \
			SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_U16_LE | \
			SNDRV_PCM_FMTBIT_S24_LE | \
			SNDRV_PCM_FMTBIT_U24_LE | \
			SNDRV_PCM_FMTBIT_S32_LE | \
			SNDRV_PCM_FMTBIT_U32_LE | \
			SNDRV_PCM_FMTBIT_IEC958_SUBFRAME_LE)

/*
 * ASoC no host IO hardware.
 * TODO: fine tune these values for all host less transfers.
 */
static const struct snd_pcm_hardware mmp_hostless_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_PAUSE |
				  SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
				  SNDRV_PCM_INFO_RESUME,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE |
				  SNDRV_PCM_FMTBIT_S32_LE,
	.period_bytes_min	= PAGE_SIZE >> 2,
	.period_bytes_max	= PAGE_SIZE >> 1,
	.periods_min		= 2,
	.periods_max		= 4,
	.buffer_bytes_max	= PAGE_SIZE,
};

#if defined(CONFIG_ARCH_PHYS_ADDR_T_64BIT)
static u64 mmp_hostless_mask = DMA_BIT_MASK(64);
#else
static u64 mmp_hostless_mask = DMA_BIT_MASK(32);
#endif

static int mmp_hostless_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	int ret;
	struct snd_card *card = substream->pcm->card;

	substream->dma_buffer.dev.type = SNDRV_DMA_TYPE_DEV;
	substream->dma_buffer.dev.dev = card->dev;
	substream->dma_buffer.dev.dev->coherent_dma_mask = mmp_hostless_mask;
	substream->dma_buffer.private_data = NULL;

	substream->hw_no_buffer = 1;

	ret = snd_pcm_lib_malloc_pages(substream, PAGE_SIZE);
	if (ret < 0) {
		dev_err(card->dev, "allocate page error");
		return ret;
	}

	return 0;
}

static int mmp_hostless_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_lib_free_pages(substream);

	return 0;
}

static int mmp_hostless_open(struct snd_pcm_substream *substream)
{
	snd_soc_set_runtime_hwparams(substream, &mmp_hostless_hardware);

	return 0;
}

static struct snd_soc_dai_driver mmp_dummy_dais[] = {
	{
		.name = "mmp-dummy-dai-0",
		.id = 0,
		.playback = {
			.channels_min	= 1,
			.channels_max	= 384,
			.rates		= STUB_RATES,
			.formats	= STUB_FORMATS,
		},
		.capture = {
			.channels_min	= 1,
			.channels_max	= 384,
			.rates = STUB_RATES,
			.formats = STUB_FORMATS,
		 },
	},
	{
		.name = "mmp-dummy-dai-1",
		.id = 1,
		.playback = {
			.channels_min	= 1,
			.channels_max	= 384,
			.rates		= STUB_RATES,
			.formats	= STUB_FORMATS,
		},
		.capture = {
			.channels_min	= 1,
			.channels_max	= 384,
			.rates = STUB_RATES,
			.formats = STUB_FORMATS,
		 },
	},
	{
		.name = "mmp-dummy-dai-2",
		.id = 2,
		.playback = {
			.channels_min	= 1,
			.channels_max	= 384,
			.rates		= STUB_RATES,
			.formats	= STUB_FORMATS,
		},
		.capture = {
			.channels_min	= 1,
			.channels_max	= 384,
			.rates = STUB_RATES,
			.formats = STUB_FORMATS,
		 },
	},
};

static const struct snd_soc_component_driver mmp_dummy_component = {
	.name		= "mmp-dummy",
};

static struct snd_pcm_ops mmp_hostless_ops = {
	.open		= mmp_hostless_open,
	.hw_params	= mmp_hostless_hw_params,
	.hw_free	= mmp_hostless_hw_free,
};

static struct snd_soc_platform_driver mmp_soc_hostless_platform = {
	.ops		= &mmp_hostless_ops,
};

static int mmp_pcm_hostless_probe(struct platform_device *pdev)
{
	int ret;

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	ret = snd_soc_register_component(&pdev->dev, &mmp_dummy_component,
					  mmp_dummy_dais, ARRAY_SIZE(mmp_dummy_dais));
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to register DAI\n");
		return ret;
	}

	ret = snd_soc_register_platform(&pdev->dev,
				   &mmp_soc_hostless_platform);

	if (ret < 0) {
		snd_soc_unregister_component(&pdev->dev);
		return ret;
	}

	return ret;
}

static int mmp_pcm_hostless_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mmp_pcm_hostless_of_match[] = {
	{ .compatible = "marvell,mmp-pcm-hostless"},
	{},
};
MODULE_DEVICE_TABLE(of, mmp_pcm_hostless_of_match);
#endif

static struct platform_driver mmp_pcm_hostless_driver = {
	.driver = {
		.name = "mmp-pcm-hostless",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(mmp_pcm_hostless_of_match),
#endif
	},
	.probe = mmp_pcm_hostless_probe,
	.remove = mmp_pcm_hostless_remove,
};

module_platform_driver(mmp_pcm_hostless_driver);

MODULE_DESCRIPTION("Hostless platform driver");
MODULE_LICENSE("GPL v2");
