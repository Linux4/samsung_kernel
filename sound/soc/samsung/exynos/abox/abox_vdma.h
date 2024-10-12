/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * ALSA SoC - Samsung Abox Virtual DMA driver
 *
 * Copyright (c) 2017 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_ABOX_VDMA_H
#define __SND_SOC_ABOX_VDMA_H

/**
 * Register abox virtual component
 * @param[in]	dev		pointer to abox device
 * @param[in]	id		unique buffer id
 * @param[in]	name		name of the component
 * @param[in]	aaudio		whether the pcm supports aaudio
 * @param[in]	playback	playback capability
 * @param[in]	capture		capture capability
 * @return	device containing asoc component
 */
extern struct device *abox_vdma_register_component(struct device *dev,
		int id, const char *name, bool aaudio,
		struct snd_pcm_hardware *playback,
		struct snd_pcm_hardware *capture);

/**
 * register sound card for abox vdma
 * @param[in]	dev_abox	pointer to abox device
 */
extern void abox_vdma_register_card(struct device *dev_abox);

/**
 * Initialize abox vdma
 * @param[in]	dev_abox	pointer to abox device
 */
extern void abox_vdma_init(struct device *dev_abox);

#endif /* __SND_SOC_ABOX_VDMA_H */
