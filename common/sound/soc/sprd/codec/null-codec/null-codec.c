/*
 * sound/soc/sprd/codec/null-codec/null-codec.c
 *
 * NULL-CODEC -- SpreadTrum just for codec code.
 *
 * Copyright (C) 2012 SpreadTrum Ltd.
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
#define pr_fmt(fmt) pr_sprd_fmt("NULCD") fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/of.h>

#include <sound/core.h>
#include <sound/soc.h>

#include "sprd-asoc-common.h"
#include "null-codec.h"

/* PCM Playing and Recording default in full duplex mode */
struct snd_soc_dai_driver null_codec_dai[] = {
	{
	 .name = "null-codec-dai",
	 .playback = {
		      .stream_name = "Playback",
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_48000,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .capture = {
		     .stream_name = "Capture",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_8000_48000,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 },
};

static struct snd_soc_codec_driver soc_codec_dev_null_codec = { 0 };

static int null_codec_codec_probe(struct platform_device *pdev)
{
	int ret;

	sp_asoc_pr_dbg("%s\n", __func__);

	ret = snd_soc_register_codec(&pdev->dev,
				     &soc_codec_dev_null_codec, null_codec_dai,
				     ARRAY_SIZE(null_codec_dai));
	if (ret != 0) {
		pr_err("ERR:Failed to register NULL CODEC: %d\n", ret);
		return ret;
	}

	return 0;

}

static int null_codec_codec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id codec_of_match[] = {
	{.compatible = "sprd,null-codec",},
	{},
};

MODULE_DEVICE_TABLE(of, codec_of_match);
#endif

static struct platform_driver null_codec_codec_driver = {
	.driver = {
		   .name = "null-codec",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(codec_of_match),
		   },
	.probe = null_codec_codec_probe,
	.remove = null_codec_codec_remove,
};

module_platform_driver(null_codec_codec_driver);

MODULE_DESCRIPTION("NULL-CODEC ALSA SoC codec driver");
MODULE_AUTHOR("Ken Kuang <ken.kuang@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("codec:null-codec");
