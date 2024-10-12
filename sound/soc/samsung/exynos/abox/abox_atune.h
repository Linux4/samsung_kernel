/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * ALSA SoC - Samsung Abox ASoC Audio Tuning Block driver
 *
 * Copyright (c) 2020 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_ABOX_ATUNE_H
#define __SND_SOC_ABOX_ATUNE_H

#include "abox_soc.h"

#define SPUS_BASE			0x0000
#define SPUM_BASE			0x1000

#define DSGAIN_BASE			0x0000
#define DSGAIN_ITV			0x40
#define DSGAIN_CTRL			0x0000
#define DSGAIN_VOL_CHANGE_FIN		0x0008
#define DSGAIN_VOL_CHANGE_FOUT		0x000c
#define DSGAIN_GAIN0			0x0010
#define DSGAIN_GAIN1			0x0014
#define DSGAIN_GAIN2			0x0018
#define DSGAIN_GAIN3			0x001c
#define DSGAIN_BIT_CTRL			0x0020
#define DSGAIN_FADEIN_CNT		0x0024
#define DSGAIN_FADEOUT_CNT		0x0028

#define USGAIN_BASE			0x0080
#define USGAIN_ITV			0x40
#define USGAIN_CTRL			0x0000
#define USGAIN_VOL_CHANGE_FIN		0x0008
#define USGAIN_VOL_CHANGE_FOUT		0x000c
#define USGAIN_GAIN0			0x0010
#define USGAIN_GAIN1			0x0014
#define USGAIN_GAIN2			0x0018
#define USGAIN_GAIN3			0x001c
#define USGAIN_FADEIN_CNT		0x0024
#define USGAIN_FADEOUT_CNT		0x0028

#define SPUS_DSGAIN_FADEIN_CNT(x)	ATUNE_SFR(SPUS_DSGAIN, DSGAIN_FADEIN_CNT, x)
#define SPUS_DSGAIN_FADEOUT_CNT(x)	ATUNE_SFR(SPUS_DSGAIN, DSGAIN_FADEOUT_CNT, x)
#define SPUS_USGAIN_FADEIN_CNT(x)	ATUNE_SFR(SPUS_USGAIN, USGAIN_FADEIN_CNT, x)
#define SPUS_USGAIN_FADEOUT_CNT(x)	ATUNE_SFR(SPUS_USGAIN, USGAIN_FADEOUT_CNT, x)
#define SPUM_DSGAIN_FADEIN_CNT(x)	ATUNE_SFR(SPUM_DSGAIN, DSGAIN_FADEIN_CNT, x)
#define SPUM_DSGAIN_FADEOUT_CNT(x)	ATUNE_SFR(SPUM_DSGAIN, DSGAIN_FADEOUT_CNT, x)
#define SPUM_USGAIN_FADEIN_CNT(x)	ATUNE_SFR(SPUM_USGAIN, USGAIN_FADEIN_CNT, x)
#define SPUM_USGAIN_FADEOUT_CNT(x)	ATUNE_SFR(SPUM_DSGAIN, USGAIN_FADEOUT_CNT, x)
#define FADEIN_CNT_SHIFT		0x0
#define FADEIN_CNT_MASK			0xffff
#define FADEOUT_CNT_SHIFT		0x0
#define FADEOUT_CNT_MASK		0xffff

#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_ABOX_ATUNE)
/**
 * Probe atune widget and controls
 * @param[in]	data	pointer to abox_data structure
 * @return	0 or error code
 */
extern int abox_atune_probe(struct abox_data *data);

/**
 * Test whether the posttune is connected
 * @param[in]	data	pointer to abox_data structure
 * @param[in]	id	id of spus
 * @return	if it's connected true, otherwise false.
 */
extern bool abox_atune_spus_posttune_connected(struct abox_data *data, int id);

/**
 * Test whether the pretune is connected
 * @param[in]	data	pointer to abox_data structure
 * @param[in]	id	id of spum
 * @return	if it's connected true, otherwise false.
 */
extern bool abox_atune_spum_pretune_connected(struct abox_data *data, int id);

/**
 * Synchronize format and connect atune with spus or spum
 * @param[in]	data	pointer to abox_data structure
 * @param[in]	e	dapm event flag
 * @param[in]	stream	SNDRV_PCM_STREAM_PLAYBACK or SNDRV_PCM_STREAM_CAPTURE
 * @param[in]	sid	spus or spum id
 * @return	0 or error code
 */
extern int abox_atune_dapm_event(struct abox_data *data, int e, int stream,
		int sid);
#else
static inline int abox_atune_probe(struct abox_data *data)
{
	return -ENODEV;
}
static inline bool abox_atune_spus_posttune_connected(struct abox_data *data, int id)
{
	return false;
}
static inline bool abox_atune_spum_pretune_connected(struct abox_data *data, int id)
{
	return false;
}
static inline int abox_atune_dapm_event(struct abox_data *data, int e, int stream,
		int sid)
{
	return 0;
}
#endif

#endif /* __SND_SOC_ABOX_ATUNE_H */
