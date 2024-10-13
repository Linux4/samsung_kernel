/*
 * sound/soc/sprd/i2s-null-codec.c
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
#define pr_fmt(fmt) pr_sprd_fmt("I2SNC") fmt

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <sound/soc.h>
#include "sprd-asoc-common.h"
#include <mach/i2s.h>

#define NAME_SIZE	32

static struct snd_soc_dai_link all_i2s_dai[] = {
	{
	 .name = "all-i2s",
	 .stream_name = "i2s",

	 .codec_name = "null-codec",
	 .platform_name = "sprd-pcm-audio",
	 .cpu_dai_name = "i2s_bt_sco0",
	 .codec_dai_name = "null-codec-dai",
	 },
};

static struct snd_soc_card all_i2s_card = {
	.name = "all-i2s",
	.dai_link = all_i2s_dai,
	.num_links = ARRAY_SIZE(all_i2s_dai),
	.owner = THIS_MODULE,
};

static int sprd_asoc_i2s_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &all_i2s_card;
	struct device_node *node = pdev->dev.of_node;
	card->dev = &pdev->dev;
	if (node) {
		int i;
		int dai_count;
		struct device_node *pcm_node;
		struct device_node *codec_node;

		if (snd_soc_of_parse_card_name(card, "sprd,model")) {
			pr_err("ERR:Card name is not provided\n");
			return -ENODEV;
		}

		pcm_node = of_parse_phandle(node, "sprd,pcm", 0);
		if (!pcm_node) {
			pr_err("ERR:PCM node is not provided\n");
			return -EINVAL;
		}

		codec_node = of_parse_phandle(node, "sprd,codec", 0);
		if (!codec_node) {
			pr_err("ERR:CODEC node is not provided\n");
			of_node_put(pcm_node);
			return -EINVAL;
		}

		if (!of_get_property(node, "sprd,i2s", &dai_count))
			return -ENOENT;
		dai_count /= sizeof(((struct property *) 0)->length);

		sp_asoc_pr_dbg("Register I2S from DTS count is %d\n",
			       dai_count);

		card->dai_link =
		    devm_kzalloc(&pdev->dev,
				 dai_count * sizeof(struct snd_soc_dai_link),
				 GFP_KERNEL);
		card->num_links = dai_count;

		for (i = 0; i < card->num_links; i++) {
			char uni_name[NAME_SIZE] = { 0 };
			char uni_sname[NAME_SIZE] = { 0 };
			struct device_node *dai_node;
			dai_node = of_parse_phandle(node, "sprd,i2s", i);
			if (dai_node)
				sp_asoc_pr_dbg("Register I2S dai node is %s\n",
				       dai_node->full_name);
			else
				pr_err("ERR:I2S dai node is not provided\n");
			card->dai_link[i] = all_i2s_dai[0];
			snprintf(uni_name, NAME_SIZE, "%s.%d",
				 all_i2s_dai[0].name, i);
			snprintf(uni_sname, NAME_SIZE, "%s.%d",
				 all_i2s_dai[0].stream_name, i);
			card->dai_link[i].name = kstrdup(uni_name, GFP_KERNEL);
			card->dai_link[i].stream_name =
			    kstrdup(uni_sname, GFP_KERNEL);
			card->dai_link[i].cpu_dai_name = NULL;
			card->dai_link[i].cpu_of_node = dai_node;
			card->dai_link[i].platform_name = NULL;
			card->dai_link[i].platform_of_node = pcm_node;
			card->dai_link[i].codec_name = NULL;
			card->dai_link[i].codec_of_node = codec_node;
			of_node_put(dai_node);
		}
		of_node_put(pcm_node);
		of_node_put(codec_node);
	}
	return snd_soc_register_card(card);
}

static int sprd_asoc_i2s_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	int i = 0;
	if (all_i2s_dai != card->dai_link) {
		for (i = 0; i < card->num_links; i++) {
			kfree(card->dai_link[i].name);
			kfree(card->dai_link[i].stream_name);
		}
	}
	snd_soc_unregister_card(card);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id i2s_null_codec_of_match[] = {
	{.compatible = "sprd,i2s-null-codec",},
	{},
};

MODULE_DEVICE_TABLE(of, i2s_null_codec_of_match);
#endif

static struct platform_driver sprd_asoc_i2s_driver = {
	.driver = {
		   .name = "i2s-null-codec",
		   .owner = THIS_MODULE,
		   .pm = &snd_soc_pm_ops,
		   .of_match_table = of_match_ptr(i2s_null_codec_of_match),
		   },
	.probe = sprd_asoc_i2s_probe,
	.remove = sprd_asoc_i2s_remove,
};

module_platform_driver(sprd_asoc_i2s_driver);

MODULE_DESCRIPTION("ALSA SoC SpreadTrum I2S");
MODULE_AUTHOR("Ken Kuang <ken.kuang@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("machine:i2s");
