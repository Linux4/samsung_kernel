// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * ALSA SoC - Samsung Abox Component driver
 *
 * Copyright (c) 2021 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <sound/soc.h>

#include "abox.h"
#include "abox_cmpnt.h"
#include "abox_dma.h"
#include "abox_spus.h"

static const unsigned short SND_SOC_DAPM_PRE_PMU_POST_PMD = SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD;

struct abox_spus_data {
	struct abox_data *abox;
	struct abox_dma_data *rdma;
};

static inline struct device *abox_dma_get_dev(struct abox_dma_data *dma)
{
	return dma->dev;
}

static inline int abox_dma_get_id(struct abox_dma_data *dma)
{
	return dma->id;
}

static struct device *spus_get_dev(struct abox_spus_data *spus)
{
	return abox_dma_get_dev(spus->rdma);
}

static int spus_get_id(struct abox_spus_data *spus)
{
	return abox_dma_get_id(spus->rdma);
}

static int get_dapm_widget_id(struct snd_soc_dapm_widget *widget)
{
	return widget->id;
}

static int get_dapm_kcontrol_id(struct snd_kcontrol *kcontrol)
{
	return get_dapm_widget_id(snd_soc_dapm_kcontrol_widget(kcontrol));
}

static enum abox_dai spus_sink[COUNT_SPUS];
static int write_spus_sink(unsigned int id, enum abox_dai dai)
{
	if (id >= ARRAY_SIZE(spus_sink))
		return -EINVAL;

	spus_sink[id] = dai;
	return 0;
}
static int read_spus_sink(int id, enum abox_dai *dai)
{
	if (id >= ARRAY_SIZE(spus_sink))
		return -EINVAL;

	*dai = spus_sink[id];
	return 0;
}

static int spus_in_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e, int id)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return abox_cmpnt_notify_event(data, ABOX_WIDGET_SPUS_IN0 + id, e);
}

static int spus_in0_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_in_event(w, k, e, 0);
}

static int spus_in1_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_in_event(w, k, e, 1);
}

static int spus_in2_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_in_event(w, k, e, 2);
}

static int spus_in3_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_in_event(w, k, e, 3);
}

static int spus_in4_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_in_event(w, k, e, 4);
}

static int spus_in5_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_in_event(w, k, e, 5);
}

static int spus_in6_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_in_event(w, k, e, 6);
}

static int spus_in7_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_in_event(w, k, e, 7);
}

static int spus_in8_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_in_event(w, k, e, 8);
}

static int spus_in9_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_in_event(w, k, e, 9);
}

static int spus_in10_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_in_event(w, k, e, 10);
}

static int spus_in11_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_in_event(w, k, e, 11);
}

static int spus_asrc_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e, int id)
{
	return abox_cmpnt_asrc_event(w, e, SNDRV_PCM_STREAM_PLAYBACK, id);
}

static int spus_asrc0_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_asrc_event(w, k, e, 0);
}

static int spus_asrc1_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_asrc_event(w, k, e, 1);
}

static int spus_asrc2_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_asrc_event(w, k, e, 2);
}

static int spus_asrc3_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_asrc_event(w, k, e, 3);
}

static int spus_asrc4_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_asrc_event(w, k, e, 4);
}

static int spus_asrc5_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_asrc_event(w, k, e, 5);
}

static int spus_asrc6_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_asrc_event(w, k, e, 6);
}

static int spus_asrc7_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_asrc_event(w, k, e, 7);
}

static int spus_asrc8_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_asrc_event(w, k, e, 8);
}

static int spus_asrc9_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_asrc_event(w, k, e, 9);
}

static int spus_asrc10_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_asrc_event(w, k, e, 10);
}

static int spus_asrc11_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int e)
{
	return spus_asrc_event(w, k, e, 11);
}

static const char *const spus_inx_texts[] = {
	"RDMA", "SIFSM", "RESERVED", "SIFST",
};
static SOC_ENUM_SINGLE_DECL(spus_in0_enum, ABOX_SPUS_CTRL_FC_SRC(0),
		ABOX_FUNC_CHAIN_SRC_IN_L(0), spus_inx_texts);
static SOC_ENUM_SINGLE_DECL(spus_in1_enum, ABOX_SPUS_CTRL_FC_SRC(1),
		ABOX_FUNC_CHAIN_SRC_IN_L(1), spus_inx_texts);
static SOC_ENUM_SINGLE_DECL(spus_in2_enum, ABOX_SPUS_CTRL_FC_SRC(2),
		ABOX_FUNC_CHAIN_SRC_IN_L(2), spus_inx_texts);
static SOC_ENUM_SINGLE_DECL(spus_in3_enum, ABOX_SPUS_CTRL_FC_SRC(3),
		ABOX_FUNC_CHAIN_SRC_IN_L(3), spus_inx_texts);
static SOC_ENUM_SINGLE_DECL(spus_in4_enum, ABOX_SPUS_CTRL_FC_SRC(4),
		ABOX_FUNC_CHAIN_SRC_IN_L(4), spus_inx_texts);
static SOC_ENUM_SINGLE_DECL(spus_in5_enum, ABOX_SPUS_CTRL_FC_SRC(5),
		ABOX_FUNC_CHAIN_SRC_IN_L(5), spus_inx_texts);
static SOC_ENUM_SINGLE_DECL(spus_in6_enum, ABOX_SPUS_CTRL_FC_SRC(6),
		ABOX_FUNC_CHAIN_SRC_IN_L(6), spus_inx_texts);
static SOC_ENUM_SINGLE_DECL(spus_in7_enum, ABOX_SPUS_CTRL_FC_SRC(7),
		ABOX_FUNC_CHAIN_SRC_IN_L(7), spus_inx_texts);
static SOC_ENUM_SINGLE_DECL(spus_in8_enum, ABOX_SPUS_CTRL_FC_SRC(8),
		ABOX_FUNC_CHAIN_SRC_IN_L(8), spus_inx_texts);
static SOC_ENUM_SINGLE_DECL(spus_in9_enum, ABOX_SPUS_CTRL_FC_SRC(9),
		ABOX_FUNC_CHAIN_SRC_IN_L(9), spus_inx_texts);
static SOC_ENUM_SINGLE_DECL(spus_in10_enum, ABOX_SPUS_CTRL_FC_SRC(10),
		ABOX_FUNC_CHAIN_SRC_IN_L(10), spus_inx_texts);
static SOC_ENUM_SINGLE_DECL(spus_in11_enum, ABOX_SPUS_CTRL_FC_SRC(11),
		ABOX_FUNC_CHAIN_SRC_IN_L(11), spus_inx_texts);

static const struct snd_kcontrol_new spus_in_controls[] = {
	SOC_DAPM_ENUM("MUX", spus_in0_enum),
	SOC_DAPM_ENUM("MUX", spus_in1_enum),
	SOC_DAPM_ENUM("MUX", spus_in2_enum),
	SOC_DAPM_ENUM("MUX", spus_in3_enum),
	SOC_DAPM_ENUM("MUX", spus_in4_enum),
	SOC_DAPM_ENUM("MUX", spus_in5_enum),
	SOC_DAPM_ENUM("MUX", spus_in6_enum),
	SOC_DAPM_ENUM("MUX", spus_in7_enum),
	SOC_DAPM_ENUM("MUX", spus_in8_enum),
	SOC_DAPM_ENUM("MUX", spus_in9_enum),
	SOC_DAPM_ENUM("MUX", spus_in10_enum),
	SOC_DAPM_ENUM("MUX", spus_in11_enum),
};

static const char *const spus_outx_texts[] = {
	"RESERVED", "SIFS0", "SIFS1", "SIFS2", "SIFS3",
	"SIFS4", "SIFS5", "SIFS6",
};
#define SPUS_OUT(x)	((x) ? ((x) << 1) : 0x1)
static const unsigned int spus_outx_values[] = {
	0x0, SPUS_OUT(0), SPUS_OUT(1), SPUS_OUT(2), SPUS_OUT(3),
	SPUS_OUT(4), SPUS_OUT(5), SPUS_OUT(6),
};
static const unsigned int spus_outx_mask = ABOX_FUNC_CHAIN_SRC_OUT_MASK(0) >>
		ABOX_FUNC_CHAIN_SRC_OUT_L(0);

static int spus_out_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol, int id)
{
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;
	enum abox_dai dai;
	int ret;

	if (item[0] >= e->items)
		return -EINVAL;

	abox_dev(dapm->dev, "%s(%d, %u)\n", __func__, id, item[0]);

	dai = (item[0] > 0) ? (ABOX_SIFS0 + (item[0] - 1)) : ABOX_NONE;
	ret = write_spus_sink(id, dai);
	if (ret < 0)
		return ret;

	return snd_soc_dapm_put_enum_double(kcontrol, ucontrol);
}

static int spus_out0_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_out_put(kcontrol, ucontrol, 0);
}

static int spus_out1_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_out_put(kcontrol, ucontrol, 1);
}

static int spus_out2_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_out_put(kcontrol, ucontrol, 2);
}

static int spus_out3_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_out_put(kcontrol, ucontrol, 3);
}

static int spus_out4_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_out_put(kcontrol, ucontrol, 4);
}

static int spus_out5_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_out_put(kcontrol, ucontrol, 5);
}

static int spus_out6_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_out_put(kcontrol, ucontrol, 6);
}

static int spus_out7_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_out_put(kcontrol, ucontrol, 7);
}

static int spus_out8_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_out_put(kcontrol, ucontrol, 8);
}

static int spus_out9_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_out_put(kcontrol, ucontrol, 9);
}

static int spus_out10_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_out_put(kcontrol, ucontrol, 10);
}

static int spus_out11_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_out_put(kcontrol, ucontrol, 11);
}

static SOC_VALUE_ENUM_SINGLE_AUTODISABLE_DECL(spus_out0_enum,
		ABOX_SPUS_CTRL_FC_SRC(0), ABOX_FUNC_CHAIN_SRC_OUT_L(0),
		spus_outx_mask, spus_outx_texts, spus_outx_values);
static SOC_VALUE_ENUM_SINGLE_AUTODISABLE_DECL(spus_out1_enum,
		ABOX_SPUS_CTRL_FC_SRC(1), ABOX_FUNC_CHAIN_SRC_OUT_L(1),
		spus_outx_mask, spus_outx_texts, spus_outx_values);
static SOC_VALUE_ENUM_SINGLE_AUTODISABLE_DECL(spus_out2_enum,
		ABOX_SPUS_CTRL_FC_SRC(2), ABOX_FUNC_CHAIN_SRC_OUT_L(2),
		spus_outx_mask, spus_outx_texts, spus_outx_values);
static SOC_VALUE_ENUM_SINGLE_AUTODISABLE_DECL(spus_out3_enum,
		ABOX_SPUS_CTRL_FC_SRC(3), ABOX_FUNC_CHAIN_SRC_OUT_L(3),
		spus_outx_mask, spus_outx_texts, spus_outx_values);
static SOC_VALUE_ENUM_SINGLE_AUTODISABLE_DECL(spus_out4_enum,
		ABOX_SPUS_CTRL_FC_SRC(4), ABOX_FUNC_CHAIN_SRC_OUT_L(4),
		spus_outx_mask, spus_outx_texts, spus_outx_values);
static SOC_VALUE_ENUM_SINGLE_AUTODISABLE_DECL(spus_out5_enum,
		ABOX_SPUS_CTRL_FC_SRC(5), ABOX_FUNC_CHAIN_SRC_OUT_L(5),
		spus_outx_mask, spus_outx_texts, spus_outx_values);
static SOC_VALUE_ENUM_SINGLE_AUTODISABLE_DECL(spus_out6_enum,
		ABOX_SPUS_CTRL_FC_SRC(6), ABOX_FUNC_CHAIN_SRC_OUT_L(6),
		spus_outx_mask, spus_outx_texts, spus_outx_values);
static SOC_VALUE_ENUM_SINGLE_AUTODISABLE_DECL(spus_out7_enum,
		ABOX_SPUS_CTRL_FC_SRC(7), ABOX_FUNC_CHAIN_SRC_OUT_L(7),
		spus_outx_mask, spus_outx_texts, spus_outx_values);
static SOC_VALUE_ENUM_SINGLE_AUTODISABLE_DECL(spus_out8_enum,
		ABOX_SPUS_CTRL_FC_SRC(8), ABOX_FUNC_CHAIN_SRC_OUT_L(8),
		spus_outx_mask, spus_outx_texts, spus_outx_values);
static SOC_VALUE_ENUM_SINGLE_AUTODISABLE_DECL(spus_out9_enum,
		ABOX_SPUS_CTRL_FC_SRC(9), ABOX_FUNC_CHAIN_SRC_OUT_L(9),
		spus_outx_mask, spus_outx_texts, spus_outx_values);
static SOC_VALUE_ENUM_SINGLE_AUTODISABLE_DECL(spus_out10_enum,
		ABOX_SPUS_CTRL_FC_SRC(10), ABOX_FUNC_CHAIN_SRC_OUT_L(10),
		spus_outx_mask, spus_outx_texts, spus_outx_values);
static SOC_VALUE_ENUM_SINGLE_AUTODISABLE_DECL(spus_out11_enum,
		ABOX_SPUS_CTRL_FC_SRC(11), ABOX_FUNC_CHAIN_SRC_OUT_L(11),
		spus_outx_mask, spus_outx_texts, spus_outx_values);

static const struct snd_kcontrol_new spus_out_controls[] = {
	SOC_DAPM_ENUM_EXT("DEMUX", spus_out0_enum,
			snd_soc_dapm_get_enum_double, spus_out0_put),
	SOC_DAPM_ENUM_EXT("DEMUX", spus_out1_enum,
			snd_soc_dapm_get_enum_double, spus_out1_put),
	SOC_DAPM_ENUM_EXT("DEMUX", spus_out2_enum,
			snd_soc_dapm_get_enum_double, spus_out2_put),
	SOC_DAPM_ENUM_EXT("DEMUX", spus_out3_enum,
			snd_soc_dapm_get_enum_double, spus_out3_put),
	SOC_DAPM_ENUM_EXT("DEMUX", spus_out4_enum,
			snd_soc_dapm_get_enum_double, spus_out4_put),
	SOC_DAPM_ENUM_EXT("DEMUX", spus_out5_enum,
			snd_soc_dapm_get_enum_double, spus_out5_put),
	SOC_DAPM_ENUM_EXT("DEMUX", spus_out6_enum,
			snd_soc_dapm_get_enum_double, spus_out6_put),
	SOC_DAPM_ENUM_EXT("DEMUX", spus_out7_enum,
			snd_soc_dapm_get_enum_double, spus_out7_put),
	SOC_DAPM_ENUM_EXT("DEMUX", spus_out8_enum,
			snd_soc_dapm_get_enum_double, spus_out8_put),
	SOC_DAPM_ENUM_EXT("DEMUX", spus_out9_enum,
			snd_soc_dapm_get_enum_double, spus_out9_put),
	SOC_DAPM_ENUM_EXT("DEMUX", spus_out10_enum,
			snd_soc_dapm_get_enum_double, spus_out10_put),
	SOC_DAPM_ENUM_EXT("DEMUX", spus_out11_enum,
			snd_soc_dapm_get_enum_double, spus_out11_put),
};

static const struct snd_soc_dapm_widget spus_in_widgets[] = {
	SND_SOC_DAPM_MUX_E("SPUS IN0", SND_SOC_NOPM, 0, 0, &spus_in_controls[0],
			spus_in0_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPUS IN1", SND_SOC_NOPM, 1, 0, &spus_in_controls[1],
			spus_in1_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPUS IN2", SND_SOC_NOPM, 2, 0, &spus_in_controls[2],
			spus_in2_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPUS IN3", SND_SOC_NOPM, 3, 0, &spus_in_controls[3],
			spus_in3_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPUS IN4", SND_SOC_NOPM, 4, 0, &spus_in_controls[4],
			spus_in4_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPUS IN5", SND_SOC_NOPM, 5, 0, &spus_in_controls[5],
			spus_in5_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPUS IN6", SND_SOC_NOPM, 6, 0, &spus_in_controls[6],
			spus_in6_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPUS IN7", SND_SOC_NOPM, 7, 0, &spus_in_controls[7],
			spus_in7_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPUS IN8", SND_SOC_NOPM, 8, 0, &spus_in_controls[8],
			spus_in8_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPUS IN9", SND_SOC_NOPM, 9, 0, &spus_in_controls[9],
			spus_in9_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPUS IN10", SND_SOC_NOPM, 10, 0, &spus_in_controls[10],
			spus_in10_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPUS IN11", SND_SOC_NOPM, 11, 0, &spus_in_controls[11],
			spus_in11_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
};

static const struct snd_soc_dapm_widget spus_pga_widgets[] = {
	SND_SOC_DAPM_PGA("SPUS PGA0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS PGA1", SND_SOC_NOPM, 1, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS PGA2", SND_SOC_NOPM, 2, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS PGA3", SND_SOC_NOPM, 3, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS PGA4", SND_SOC_NOPM, 4, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS PGA5", SND_SOC_NOPM, 5, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS PGA6", SND_SOC_NOPM, 6, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS PGA7", SND_SOC_NOPM, 7, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS PGA8", SND_SOC_NOPM, 8, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS PGA9", SND_SOC_NOPM, 9, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS PGA10", SND_SOC_NOPM, 10, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS PGA11", SND_SOC_NOPM, 11, 0, NULL, 0),
};

static const struct snd_soc_dapm_widget spus_asrc_widgets[] = {
	SND_SOC_DAPM_PGA_E("SPUS ASRC0", SND_SOC_NOPM, 0, 0, NULL, 0,
			spus_asrc0_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUS ASRC1", SND_SOC_NOPM, 1, 0, NULL, 0,
			spus_asrc1_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUS ASRC2", SND_SOC_NOPM, 2, 0, NULL, 0,
			spus_asrc2_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUS ASRC3", SND_SOC_NOPM, 3, 0, NULL, 0,
			spus_asrc3_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUS ASRC4", SND_SOC_NOPM, 4, 0, NULL, 0,
			spus_asrc4_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUS ASRC5", SND_SOC_NOPM, 5, 0, NULL, 0,
			spus_asrc5_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUS ASRC6", SND_SOC_NOPM, 6, 0, NULL, 0,
			spus_asrc6_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUS ASRC7", SND_SOC_NOPM, 7, 0, NULL, 0,
			spus_asrc7_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUS ASRC8", SND_SOC_NOPM, 8, 0, NULL, 0,
			spus_asrc8_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUS ASRC9", SND_SOC_NOPM, 9, 0, NULL, 0,
			spus_asrc9_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUS ASRC10", SND_SOC_NOPM, 10, 0, NULL, 0,
			spus_asrc10_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUS ASRC11", SND_SOC_NOPM, 11, 0, NULL, 0,
			spus_asrc11_event, SND_SOC_DAPM_PRE_PMU_POST_PMD),
};

static const struct snd_soc_dapm_widget spus_out_widgets[] = {
	SND_SOC_DAPM_DEMUX("SPUS OUT0", SND_SOC_NOPM, 0, 0, &spus_out_controls[0]),
	SND_SOC_DAPM_DEMUX("SPUS OUT1", SND_SOC_NOPM, 1, 0, &spus_out_controls[1]),
	SND_SOC_DAPM_DEMUX("SPUS OUT2", SND_SOC_NOPM, 2, 0, &spus_out_controls[2]),
	SND_SOC_DAPM_DEMUX("SPUS OUT3", SND_SOC_NOPM, 3, 0, &spus_out_controls[3]),
	SND_SOC_DAPM_DEMUX("SPUS OUT4", SND_SOC_NOPM, 4, 0, &spus_out_controls[4]),
	SND_SOC_DAPM_DEMUX("SPUS OUT5", SND_SOC_NOPM, 5, 0, &spus_out_controls[5]),
	SND_SOC_DAPM_DEMUX("SPUS OUT6", SND_SOC_NOPM, 6, 0, &spus_out_controls[6]),
	SND_SOC_DAPM_DEMUX("SPUS OUT7", SND_SOC_NOPM, 7, 0, &spus_out_controls[7]),
	SND_SOC_DAPM_DEMUX("SPUS OUT8", SND_SOC_NOPM, 8, 0, &spus_out_controls[8]),
	SND_SOC_DAPM_DEMUX("SPUS OUT9", SND_SOC_NOPM, 9, 0, &spus_out_controls[9]),
	SND_SOC_DAPM_DEMUX("SPUS OUT10", SND_SOC_NOPM, 10, 0, &spus_out_controls[10]),
	SND_SOC_DAPM_DEMUX("SPUS OUT11", SND_SOC_NOPM, 11, 0, &spus_out_controls[11]),
};

static const struct snd_soc_dapm_widget *spus_widgets[] = {
	spus_in_widgets,
	spus_pga_widgets,
	spus_asrc_widgets,
	spus_out_widgets,
};

static const struct snd_kcontrol_new spus_asrc_dummy_contols[] = {
	SOC_SINGLE("SPUS ASRC0 Dummy Start",
			ABOX_SPUS_CTRL_RDMA_ASRC_DUMMY_START,
			ABOX_RDMA_ASRC_DUMMY_START_L(0), 1, 0),
	SOC_SINGLE("SPUS ASRC1 Dummy Start",
			ABOX_SPUS_CTRL_RDMA_ASRC_DUMMY_START,
			ABOX_RDMA_ASRC_DUMMY_START_L(1), 1, 0),
	SOC_SINGLE("SPUS ASRC2 Dummy Start",
			ABOX_SPUS_CTRL_RDMA_ASRC_DUMMY_START,
			ABOX_RDMA_ASRC_DUMMY_START_L(2), 1, 0),
	SOC_SINGLE("SPUS ASRC3 Dummy Start",
			ABOX_SPUS_CTRL_RDMA_ASRC_DUMMY_START,
			ABOX_RDMA_ASRC_DUMMY_START_L(3), 1, 0),
	SOC_SINGLE("SPUS ASRC4 Dummy Start",
			ABOX_SPUS_CTRL_RDMA_ASRC_DUMMY_START,
			ABOX_RDMA_ASRC_DUMMY_START_L(4), 1, 0),
	SOC_SINGLE("SPUS ASRC5 Dummy Start",
			ABOX_SPUS_CTRL_RDMA_ASRC_DUMMY_START,
			ABOX_RDMA_ASRC_DUMMY_START_L(5), 1, 0),
	SOC_SINGLE("SPUS ASRC6 Dummy Start",
			ABOX_SPUS_CTRL_RDMA_ASRC_DUMMY_START,
			ABOX_RDMA_ASRC_DUMMY_START_L(6), 1, 0),
	SOC_SINGLE("SPUS ASRC7 Dummy Start",
			ABOX_SPUS_CTRL_RDMA_ASRC_DUMMY_START,
			ABOX_RDMA_ASRC_DUMMY_START_L(7), 1, 0),
	SOC_SINGLE("SPUS ASRC8 Dummy Start",
			ABOX_SPUS_CTRL_RDMA_ASRC_DUMMY_START,
			ABOX_RDMA_ASRC_DUMMY_START_L(8), 1, 0),
	SOC_SINGLE("SPUS ASRC9 Dummy Start",
			ABOX_SPUS_CTRL_RDMA_ASRC_DUMMY_START,
			ABOX_RDMA_ASRC_DUMMY_START_L(9), 1, 0),
	SOC_SINGLE("SPUS ASRC10 Dummy Start",
			ABOX_SPUS_CTRL_RDMA_ASRC_DUMMY_START,
			ABOX_RDMA_ASRC_DUMMY_START_L(10), 1, 0),
	SOC_SINGLE("SPUS ASRC11 Dummy Start",
			ABOX_SPUS_CTRL_RDMA_ASRC_DUMMY_START,
			ABOX_RDMA_ASRC_DUMMY_START_L(11), 1, 0),
};

static const struct snd_kcontrol_new spus_asrc_start_num_contols[] = {
	SOC_SINGLE("SPUS ASRC0 Start Num",
			ABOX_SPUS_CTRL_RDMA_START_ASRC_NUM(0),
			ABOX_RDMA_START_ASRC_NUM_L(0), 32, 0),
	SOC_SINGLE("SPUS ASRC1 Start Num",
			ABOX_SPUS_CTRL_RDMA_START_ASRC_NUM(0),
			ABOX_RDMA_START_ASRC_NUM_L(1), 32, 0),
	SOC_SINGLE("SPUS ASRC2 Start Num",
			ABOX_SPUS_CTRL_RDMA_START_ASRC_NUM(0),
			ABOX_RDMA_START_ASRC_NUM_L(2), 32, 0),
	SOC_SINGLE("SPUS ASRC3 Start Num",
			ABOX_SPUS_CTRL_RDMA_START_ASRC_NUM(3),
			ABOX_RDMA_START_ASRC_NUM_L(3), 32, 0),
	SOC_SINGLE("SPUS ASRC4 Start Num",
			ABOX_SPUS_CTRL_RDMA_START_ASRC_NUM(4),
			ABOX_RDMA_START_ASRC_NUM_L(4), 32, 0),
	SOC_SINGLE("SPUS ASRC5 Start Num",
			ABOX_SPUS_CTRL_RDMA_START_ASRC_NUM(5),
			ABOX_RDMA_START_ASRC_NUM_L(5), 32, 0),
	SOC_SINGLE("SPUS ASRC6 Start Num",
			ABOX_SPUS_CTRL_RDMA_START_ASRC_NUM(6),
			ABOX_RDMA_START_ASRC_NUM_L(6), 32, 0),
	SOC_SINGLE("SPUS ASRC7 Start Num",
			ABOX_SPUS_CTRL_RDMA_START_ASRC_NUM(7),
			ABOX_RDMA_START_ASRC_NUM_L(7), 32, 0),
	SOC_SINGLE("SPUS ASRC8 Start Num",
			ABOX_SPUS_CTRL_RDMA_START_ASRC_NUM(8),
			ABOX_RDMA_START_ASRC_NUM_L(8), 32, 0),
	SOC_SINGLE("SPUS ASRC9 Start Num",
			ABOX_SPUS_CTRL_RDMA_START_ASRC_NUM(9),
			ABOX_RDMA_START_ASRC_NUM_L(9), 32, 0),
	SOC_SINGLE("SPUS ASRC10 Start Num",
			ABOX_SPUS_CTRL_RDMA_START_ASRC_NUM(10),
			ABOX_RDMA_START_ASRC_NUM_L(10), 32, 0),
	SOC_SINGLE("SPUS ASRC11 Start Num",
			ABOX_SPUS_CTRL_RDMA_START_ASRC_NUM(11),
			ABOX_RDMA_START_ASRC_NUM_L(11), 32, 0),
};

static bool spus_asrc_force_enable[] = {
	false, false, false, false,
	false, false, false, false,
	false, false, false, false
};

static int spus_asrc_enable_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol, int idx)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	long val = ucontrol->value.integer.value[0];
	int ret;

	abox_info(dev, "%s(%ld, %d)\n", __func__, val, idx);

	spus_asrc_force_enable[idx] = !!val;

	return 0;
}

static int spus_asrc0_enable_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_enable_put(kcontrol, ucontrol, 0);
}

static int spus_asrc1_enable_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_enable_put(kcontrol, ucontrol, 1);
}

static int spus_asrc2_enable_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_enable_put(kcontrol, ucontrol, 2);
}

static int spus_asrc3_enable_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_enable_put(kcontrol, ucontrol, 3);
}

static int spus_asrc4_enable_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_enable_put(kcontrol, ucontrol, 4);
}

static int spus_asrc5_enable_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_enable_put(kcontrol, ucontrol, 5);
}

static int spus_asrc6_enable_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_enable_put(kcontrol, ucontrol, 6);
}

static int spus_asrc7_enable_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_enable_put(kcontrol, ucontrol, 7);
}

static int spus_asrc8_enable_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_enable_put(kcontrol, ucontrol, 8);
}

static int spus_asrc9_enable_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_enable_put(kcontrol, ucontrol, 9);
}

static int spus_asrc10_enable_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_enable_put(kcontrol, ucontrol, 10);
}

static int spus_asrc11_enable_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_enable_put(kcontrol, ucontrol, 11);
}


static const struct snd_kcontrol_new spus_asrc_controls[] = {
	SOC_SINGLE_EXT("SPUS ASRC0", ABOX_SPUS_CTRL_FC_SRC(0),
			ABOX_FUNC_CHAIN_SRC_ASRC_L(0), 1, 0,
			snd_soc_get_volsw, spus_asrc0_enable_put),
	SOC_SINGLE_EXT("SPUS ASRC1", ABOX_SPUS_CTRL_FC_SRC(1),
			ABOX_FUNC_CHAIN_SRC_ASRC_L(1), 1, 0,
			snd_soc_get_volsw, spus_asrc1_enable_put),
	SOC_SINGLE_EXT("SPUS ASRC2", ABOX_SPUS_CTRL_FC_SRC(2),
			ABOX_FUNC_CHAIN_SRC_ASRC_L(2), 1, 0,
			snd_soc_get_volsw, spus_asrc2_enable_put),
	SOC_SINGLE_EXT("SPUS ASRC3", ABOX_SPUS_CTRL_FC_SRC(3),
			ABOX_FUNC_CHAIN_SRC_ASRC_L(3), 1, 0,
			snd_soc_get_volsw, spus_asrc3_enable_put),
	SOC_SINGLE_EXT("SPUS ASRC4", ABOX_SPUS_CTRL_FC_SRC(4),
			ABOX_FUNC_CHAIN_SRC_ASRC_L(4), 1, 0,
			snd_soc_get_volsw, spus_asrc4_enable_put),
	SOC_SINGLE_EXT("SPUS ASRC5", ABOX_SPUS_CTRL_FC_SRC(5),
			ABOX_FUNC_CHAIN_SRC_ASRC_L(5), 1, 0,
			snd_soc_get_volsw, spus_asrc5_enable_put),
	SOC_SINGLE_EXT("SPUS ASRC6", ABOX_SPUS_CTRL_FC_SRC(6),
			ABOX_FUNC_CHAIN_SRC_ASRC_L(6), 1, 0,
			snd_soc_get_volsw, spus_asrc6_enable_put),
	SOC_SINGLE_EXT("SPUS ASRC7", ABOX_SPUS_CTRL_FC_SRC(7),
			ABOX_FUNC_CHAIN_SRC_ASRC_L(7), 1, 0,
			snd_soc_get_volsw, spus_asrc7_enable_put),
	SOC_SINGLE_EXT("SPUS ASRC8", ABOX_SPUS_CTRL_FC_SRC(8),
			ABOX_FUNC_CHAIN_SRC_ASRC_L(8), 1, 0,
			snd_soc_get_volsw, spus_asrc8_enable_put),
	SOC_SINGLE_EXT("SPUS ASRC9", ABOX_SPUS_CTRL_FC_SRC(9),
			ABOX_FUNC_CHAIN_SRC_ASRC_L(9), 1, 0,
			snd_soc_get_volsw, spus_asrc9_enable_put),
	SOC_SINGLE_EXT("SPUS ASRC10", ABOX_SPUS_CTRL_FC_SRC(10),
			ABOX_FUNC_CHAIN_SRC_ASRC_L(10), 1, 0,
			snd_soc_get_volsw, spus_asrc10_enable_put),
	SOC_SINGLE_EXT("SPUS ASRC11", ABOX_SPUS_CTRL_FC_SRC(11),
			ABOX_FUNC_CHAIN_SRC_ASRC_L(11), 1, 0,
			snd_soc_get_volsw, spus_asrc11_enable_put),
};

static int spus_asrc_id_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	/* ignore asrc id change */
	return 0;
}

static const struct snd_kcontrol_new spus_asrc_id_controls[] = {
	SOC_SINGLE_EXT("SPUS ASRC0 ID", ABOX_SPUS_CTRL_SRC_ASRC_ID(0),
			ABOX_SRC_ASRC_ID_L(0), 15, 0,
			snd_soc_get_volsw, spus_asrc_id_put),
	SOC_SINGLE_EXT("SPUS ASRC1 ID", ABOX_SPUS_CTRL_SRC_ASRC_ID(1),
			ABOX_SRC_ASRC_ID_L(1), 15, 0,
			snd_soc_get_volsw, spus_asrc_id_put),
	SOC_SINGLE_EXT("SPUS ASRC2 ID", ABOX_SPUS_CTRL_SRC_ASRC_ID(2),
			ABOX_SRC_ASRC_ID_L(2), 15, 0,
			snd_soc_get_volsw, spus_asrc_id_put),
	SOC_SINGLE_EXT("SPUS ASRC3 ID", ABOX_SPUS_CTRL_SRC_ASRC_ID(3),
			ABOX_SRC_ASRC_ID_L(3), 15, 0,
			snd_soc_get_volsw, spus_asrc_id_put),
	SOC_SINGLE_EXT("SPUS ASRC4 ID", ABOX_SPUS_CTRL_SRC_ASRC_ID(4),
			ABOX_SRC_ASRC_ID_L(4), 15, 0,
			snd_soc_get_volsw, spus_asrc_id_put),
	SOC_SINGLE_EXT("SPUS ASRC5 ID", ABOX_SPUS_CTRL_SRC_ASRC_ID(5),
			ABOX_SRC_ASRC_ID_L(5), 15, 0,
			snd_soc_get_volsw, spus_asrc_id_put),
	SOC_SINGLE_EXT("SPUS ASRC6 ID", ABOX_SPUS_CTRL_SRC_ASRC_ID(6),
			ABOX_SRC_ASRC_ID_L(6), 15, 0,
			snd_soc_get_volsw, spus_asrc_id_put),
	SOC_SINGLE_EXT("SPUS ASRC7 ID", ABOX_SPUS_CTRL_SRC_ASRC_ID(7),
			ABOX_SRC_ASRC_ID_L(7), 15, 0,
			snd_soc_get_volsw, spus_asrc_id_put),
	SOC_SINGLE_EXT("SPUS ASRC8 ID", ABOX_SPUS_CTRL_SRC_ASRC_ID(8),
			ABOX_SRC_ASRC_ID_L(8), 15, 0,
			snd_soc_get_volsw, spus_asrc_id_put),
	SOC_SINGLE_EXT("SPUS ASRC9 ID", ABOX_SPUS_CTRL_SRC_ASRC_ID(9),
			ABOX_SRC_ASRC_ID_L(9), 15, 0,
			snd_soc_get_volsw, spus_asrc_id_put),
	SOC_SINGLE_EXT("SPUS ASRC10 ID", ABOX_SPUS_CTRL_SRC_ASRC_ID(10),
			ABOX_SRC_ASRC_ID_L(10), 15, 0,
			snd_soc_get_volsw, spus_asrc_id_put),
	SOC_SINGLE_EXT("SPUS ASRC11 ID", ABOX_SPUS_CTRL_SRC_ASRC_ID(11),
			ABOX_SRC_ASRC_ID_L(11), 15, 0,
			snd_soc_get_volsw, spus_asrc_id_put),
};

static int spus_get_apf_coef(struct abox_data *data, int idx)
{
	return data->apf_coef[SNDRV_PCM_STREAM_PLAYBACK][idx];
}

static void spus_set_apf_coef(struct abox_data *data, int idx, int coef)
{
	data->apf_coef[SNDRV_PCM_STREAM_PLAYBACK][idx] = coef;
}

static int spus_asrc_apf_coef_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol, int idx)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	abox_dbg(dev, "%s(%d, %d)\n", __func__, idx);

	ucontrol->value.integer.value[0] = spus_get_apf_coef(data, idx);

	return 0;
}

static int spus_asrc0_apf_coef_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_get(kcontrol, ucontrol, 0);
}

static int spus_asrc1_apf_coef_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_get(kcontrol, ucontrol, 1);
}

static int spus_asrc2_apf_coef_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_get(kcontrol, ucontrol, 2);
}

static int spus_asrc3_apf_coef_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_get(kcontrol, ucontrol, 3);
}

static int spus_asrc4_apf_coef_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_get(kcontrol, ucontrol, 4);
}

static int spus_asrc5_apf_coef_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_get(kcontrol, ucontrol, 5);
}

static int spus_asrc6_apf_coef_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_get(kcontrol, ucontrol, 6);
}

static int spus_asrc7_apf_coef_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_get(kcontrol, ucontrol, 7);
}

static int spus_asrc8_apf_coef_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_get(kcontrol, ucontrol, 8);
}

static int spus_asrc9_apf_coef_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_get(kcontrol, ucontrol, 9);
}

static int spus_asrc10_apf_coef_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_get(kcontrol, ucontrol, 10);
}

static int spus_asrc11_apf_coef_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_get(kcontrol, ucontrol, 11);
}

static int spus_asrc_apf_coef_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol, int idx)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	long val = ucontrol->value.integer.value[0];

	abox_dbg(dev, "%s(%d, %ld)\n", __func__, idx, val);

	spus_set_apf_coef(data, idx, val);

	return 0;
}

static int spus_asrc0_apf_coef_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_put(kcontrol, ucontrol, 0);
}

static int spus_asrc1_apf_coef_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_put(kcontrol, ucontrol, 1);
}

static int spus_asrc2_apf_coef_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_put(kcontrol, ucontrol, 2);
}

static int spus_asrc3_apf_coef_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_put(kcontrol, ucontrol, 3);
}

static int spus_asrc4_apf_coef_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_put(kcontrol, ucontrol, 4);
}

static int spus_asrc5_apf_coef_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_put(kcontrol, ucontrol, 5);
}

static int spus_asrc6_apf_coef_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_put(kcontrol, ucontrol, 6);
}

static int spus_asrc7_apf_coef_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_put(kcontrol, ucontrol, 7);
}

static int spus_asrc8_apf_coef_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_put(kcontrol, ucontrol, 8);
}

static int spus_asrc9_apf_coef_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_put(kcontrol, ucontrol, 9);
}

static int spus_asrc10_apf_coef_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_put(kcontrol, ucontrol, 10);
}

static int spus_asrc11_apf_coef_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_apf_coef_put(kcontrol, ucontrol, 11);
}

static const struct snd_kcontrol_new spus_asrc_apf_coef_controls[] = {
	SOC_SINGLE_EXT("SPUS ASRC0 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			0, 1, 0, spus_asrc0_apf_coef_get, spus_asrc0_apf_coef_put),
	SOC_SINGLE_EXT("SPUS ASRC1 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			1, 1, 0, spus_asrc1_apf_coef_get, spus_asrc1_apf_coef_put),
	SOC_SINGLE_EXT("SPUS ASRC2 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			2, 1, 0, spus_asrc2_apf_coef_get, spus_asrc2_apf_coef_put),
	SOC_SINGLE_EXT("SPUS ASRC3 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			3, 1, 0, spus_asrc3_apf_coef_get, spus_asrc3_apf_coef_put),
	SOC_SINGLE_EXT("SPUS ASRC4 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			4, 1, 0, spus_asrc4_apf_coef_get, spus_asrc4_apf_coef_put),
	SOC_SINGLE_EXT("SPUS ASRC5 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			5, 1, 0, spus_asrc5_apf_coef_get, spus_asrc5_apf_coef_put),
	SOC_SINGLE_EXT("SPUS ASRC6 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			6, 1, 0, spus_asrc6_apf_coef_get, spus_asrc6_apf_coef_put),
	SOC_SINGLE_EXT("SPUS ASRC7 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			7, 1, 0, spus_asrc7_apf_coef_get, spus_asrc7_apf_coef_put),
	SOC_SINGLE_EXT("SPUS ASRC8 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			8, 1, 0, spus_asrc8_apf_coef_get, spus_asrc8_apf_coef_put),
	SOC_SINGLE_EXT("SPUS ASRC9 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			9, 1, 0, spus_asrc9_apf_coef_get, spus_asrc9_apf_coef_put),
	SOC_SINGLE_EXT("SPUS ASRC10 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			10, 1, 0, spus_asrc10_apf_coef_get, spus_asrc10_apf_coef_put),
	SOC_SINGLE_EXT("SPUS ASRC11 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			11, 1, 0, spus_asrc11_apf_coef_get, spus_asrc11_apf_coef_put),
};

static const char *const asrc_source_enum_texts[] = {
	"UAIF0",
	"UAIF1",
	"UAIF2",
	"UAIF3",
	"UAIF4",
	"UAIF5",
	"UAIF6",
	"UAIF7",
	"USB",
	"Ext CP",
	"Ext BCLK_CP",
	"CP",
	"PCM_CNTR",
	"BCLK_SPDY",
	"ABOX",
};

static const unsigned int asrc_source_enum_values[] = {
	TICK_UAIF0,
	TICK_UAIF1,
	TICK_UAIF2,
	TICK_UAIF3,
	TICK_UAIF4,
	TICK_UAIF5,
	TICK_UAIF6,
	TICK_UAIF7,
	TICK_USB,
	TICK_CP_EXT,
	TICK_BCLK_CP_EXT,
	TICK_CP,
	TICK_PCM_CNTR,
	TICK_BCLK_SPDY,
	TICK_SYNC,
};

static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc0_os_enum, 0, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc1_os_enum, 1, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc2_os_enum, 2, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc3_os_enum, 3, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc4_os_enum, 4, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc5_os_enum, 5, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc6_os_enum, 6, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc7_os_enum, 7, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc8_os_enum, 8, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc9_os_enum, 9, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc10_os_enum, 10, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc11_os_enum, 11, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);

static enum asrc_tick spus_asrc_os[] = {
	TICK_SYNC, TICK_SYNC, TICK_SYNC, TICK_SYNC,
	TICK_SYNC, TICK_SYNC, TICK_SYNC, TICK_SYNC,
	TICK_SYNC, TICK_SYNC, TICK_SYNC, TICK_SYNC,
};
static enum asrc_tick spus_asrc_is[] = {
	TICK_SYNC, TICK_SYNC, TICK_SYNC, TICK_SYNC,
	TICK_SYNC, TICK_SYNC, TICK_SYNC, TICK_SYNC,
	TICK_SYNC, TICK_SYNC, TICK_SYNC, TICK_SYNC
};

static int spus_asrc_os_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol, int idx)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;
	enum asrc_tick tick = spus_asrc_os[idx];

	abox_dbg(dev, "%s(%d): %d\n", __func__, idx, tick);

	item[0] = snd_soc_enum_val_to_item(e, tick);

	return 0;
}

static int spus_asrc0_os_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_os_get(kcontrol, ucontrol, 0);
}

static int spus_asrc0_os_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_os_get(kcontrol, ucontrol, 1);
}

static int spus_asrc0_os_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_os_get(kcontrol, ucontrol, 2);
}

static int spus_asrc0_os_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_os_get(kcontrol, ucontrol, 3);
}

static int spus_asrc0_os_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_os_get(kcontrol, ucontrol, 4);
}

static int spus_asrc0_os_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_os_get(kcontrol, ucontrol, 5);
}

static int spus_asrc0_os_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_os_get(kcontrol, ucontrol, 6);
}

static int spus_asrc0_os_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_os_get(kcontrol, ucontrol, 7);
}

static int spus_asrc0_os_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_os_get(kcontrol, ucontrol, 8);
}

static int spus_asrc0_os_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_os_get(kcontrol, ucontrol, 9);
}

static int spus_asrc0_os_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return spus_asrc_os_get(kcontrol, ucontrol, 10);
}

static int spus_asrc_os_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol, int idx)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;
	enum asrc_tick tick;

	if (item[0] >= e->items)
		return -EINVAL;

	tick = snd_soc_enum_item_to_val(e, item[0]);
	abox_dbg(dev, "%s(%d, %d)\n", __func__, idx, tick);
	spus_asrc_os[idx] = tick;

	return 0;
}

static int spus_asrc_is_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol, int idx)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;
	enum asrc_tick tick = spus_asrc_is[idx];

	abox_dbg(dev, "%s(%d): %d\n", __func__, idx, tick);

	item[0] = snd_soc_enum_val_to_item(e, tick);

	return 0;
}

static int spus_asrc_is_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol, int idx)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;
	enum asrc_tick tick;

	if (item[0] >= e->items)
		return -EINVAL;

	tick = snd_soc_enum_item_to_val(e, item[0]);
	abox_dbg(dev, "%s(%d, %d)\n", __func__, idx, tick);
	spus_asrc_is[idx] = tick;

	return 0;
}

static const struct snd_kcontrol_new spus_asrc_os_controls[] = {
	SOC_VALUE_ENUM_EXT("SPUS ASRC0 OS", spus_asrc0_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC1 OS", spus_asrc1_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC2 OS", spus_asrc2_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC3 OS", spus_asrc3_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC4 OS", spus_asrc4_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC5 OS", spus_asrc5_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC6 OS", spus_asrc6_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC7 OS", spus_asrc7_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC8 OS", spus_asrc8_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC9 OS", spus_asrc9_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC10 OS", spus_asrc10_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC11 OS", spus_asrc11_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
};

static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc0_is_enum, 0, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc1_is_enum, 1, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc2_is_enum, 2, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc3_is_enum, 3, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc4_is_enum, 4, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc5_is_enum, 5, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc6_is_enum, 6, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc7_is_enum, 7, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc8_is_enum, 8, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc9_is_enum, 9, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc10_is_enum, 10, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc11_is_enum, 11, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);

static const struct snd_kcontrol_new spus_asrc_is_controls[] = {
	SOC_VALUE_ENUM_EXT("SPUS ASRC0 IS", spus_asrc0_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC1 IS", spus_asrc1_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC2 IS", spus_asrc2_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC3 IS", spus_asrc3_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC4 IS", spus_asrc4_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC5 IS", spus_asrc5_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC6 IS", spus_asrc6_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC7 IS", spus_asrc7_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC8 IS", spus_asrc8_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC9 IS", spus_asrc9_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC10 IS", spus_asrc10_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC11 IS", spus_asrc11_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
};

static const struct snd_kcontrol_new *spus_controls[] = {
	spus_asrc_dummy_contols,
	spus_asrc_start_num_contols,
	spus_asrc_controls,
	spus_asrc_id_controls,
	spus_asrc_apf_coef_controls,
	spus_asrc_os_controls,
	spus_asrc_is_controls,
};

static int abox_spus_register(struct abox_spus_data *spus)
{
	struct device *dev = spus_get_dev(spus);
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(spus->abox->cmpnt);
	struct snd_soc_dapm_widget *widget;
	int id = spus_get_id(spus);
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(spus_widgets); i++) {
		widget = &spus_widgets[i][id];
		if (!widget)
			abox_err(dev, "no dedicated widget: %d, %d\n", i, id);
		snd_soc_dapm_new_control(dapm, widget);
	}


	return ret;
}

static void abox_spus_unregister(struct abox_spus_data *spus)
{
}

int abox_spus_register_rdma(struct abox_data *abox, struct abox_dma_data *rdma)
{
	struct abox_spus_data *spus;

	spus = devm_kmalloc(rdma->dev, sizeof(*spus), GFP_KERNEL);
	if (!spus)
		return -ENOMEM;

	spus->abox = abox;
	spus->rdma = rdma;
	abox_spus_register(spus);
	abox_cmpnt_register_spus(abox, spus);
}

void abox_spus_unregister_rdma(struct abox_data *abox, struct abox_spus_data *spus)
{
	abox_cmpnt_unregister_spus(abox, spus);
	abox_spus_unregister(spus);
	devm_kfree(spus_get_dev(spus), spus);
}
