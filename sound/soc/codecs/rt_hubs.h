/*
 * rt_hubs.h  --  RT55xx codec common code
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _RT_HUBS_H
#define _RT_HUBS_H

#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <sound/control.h>

struct snd_soc_codec;

/* This *must* be the first element of the codec->private_data struct */
struct rt_hubs_data {
	bool mic_diff[3];

	void (*eq_enable)(struct snd_soc_codec *, bool en);
	void (*drc_dac_enable)(struct snd_soc_codec *, bool en);
};

extern int rt_hubs_add_analogue_controls(struct snd_soc_codec *);
extern int rt_hubs_add_analogue_routes(struct snd_soc_codec *);
extern int rt_hubs_handle_analogue_pdata(struct snd_soc_codec *,
	int mic1_diff, int mic2_diff, int mic3_diff,
	int micbias1_lvl, int micbias2_lvl);

extern void rt_hubs_vmid_ena(struct snd_soc_codec *codec);
extern void rt_hubs_set_bias_level(struct snd_soc_codec *codec,
				   enum snd_soc_bias_level level);

#endif
