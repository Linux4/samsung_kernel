/*
 *  jack_rt5665_sysfs_cb.c
 *  Copyright (c) Samsung Electronics
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/input.h>
#include <sound/soc.h>
#include <sound/samsung/sec_audio_sysfs.h>
#include "jack_rt5665_sysfs_cb.h"
#include "../codecs/rt5665.h"

static struct snd_soc_codec *rt5665_codec;

static int get_jack_status(void)
{
	struct snd_soc_codec *codec = rt5665_codec;
	struct rt5665_priv *rt5665 = snd_soc_codec_get_drvdata(codec);

	int status = rt5665->jack_type;
	int report = 0;

	if (status) {
	    report = 1;
	}

	dev_info(codec->dev, "%s: %d\n", __func__, report);

	return report;
}

static int get_key_status(void)
{
	struct snd_soc_codec *codec = rt5665_codec;
	struct rt5665_priv *rt5665 = snd_soc_codec_get_drvdata(codec);
	int report = 0;

	report = rt5665->btn_det;

	dev_info(codec->dev, "%s: %d\n", __func__, report);

	return report;
}

static int get_mic_adc(void)
{
	struct snd_soc_codec *codec = rt5665_codec;
	struct rt5665_priv *rt5665 = snd_soc_codec_get_drvdata(codec);
	int adc = 0;

	adc = rt5665->adc_val;

	dev_info(codec->dev, "%s: %d\n", __func__, adc);

	return adc;
}

void register_rt5665_jack_cb(struct snd_soc_codec *codec)
{
	dev_info(codec->dev, "%s\n", __func__);

	rt5665_codec = codec;

	audio_register_jack_state_cb(get_jack_status);
	audio_register_key_state_cb(get_key_status);
	audio_register_mic_adc_cb(get_mic_adc);
}
EXPORT_SYMBOL_GPL(register_rt5665_jack_cb);