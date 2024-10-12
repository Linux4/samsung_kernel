/*
 *  sec_wcd_sysfs_cb.c
 *
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
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/input.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <sound/samsung/sec_audio_sysfs.h>
#include "sec_wcd_sysfs_cb.h"
#include "codecs/wcd-mbhc-v2.h"
#include "codecs/wcd934x/wcd934x.h"
#include "codecs/wcd934x/wcd934x-mbhc.h"

static struct snd_soc_codec *wcd_codec;
static int mad_mic_bias;

#ifndef CONFIG_SEC_SND_USB_HEADSET_ONLY
static int get_jack_status(void)
{
	struct snd_soc_codec *codec = wcd_codec;
	struct wcd934x_mbhc *wcd934x_mbhc = tavil_soc_get_mbhc(codec);
	struct wcd_mbhc *mbhc = &wcd934x_mbhc->wcd_mbhc;
	int value = 0;

	if ((mbhc->hph_status == SND_JACK_HEADSET) ||
	    (mbhc->hph_status == SND_JACK_HEADPHONE))
		value = 1;

	dev_info(codec->dev, "%s: %d\n", __func__, value);

	return value;
}

static int get_key_status(void)
{
	struct snd_soc_codec *codec = wcd_codec;
	struct wcd934x_mbhc *wcd934x_mbhc = tavil_soc_get_mbhc(codec);
	struct wcd_mbhc *mbhc = &wcd934x_mbhc->wcd_mbhc;
	int value = -1;

	if (mbhc->buttons_pressed)
		value = mbhc->mbhc_cb->map_btn_code_to_num(mbhc->codec);

	dev_info(codec->dev, "%s: %d\n", __func__, value);

	return value;
}
#endif

static int get_wdsp_status(void)
{
	struct snd_soc_codec *codec = wcd_codec;

	int value = -1;

	value = snd_soc_read(codec, 0x021F);

	dev_info(codec->dev, "%s: %d\n", __func__, value);

	return value;
}

static void set_mad_mic_bias_status(int onoff)
{
	struct snd_soc_codec *codec = wcd_codec;

	/*
	It is WCD934x codec's MAD mic register.
	it is need to change when if it use another wcd codec
	WCD934X_ANA_MAD_SETUP                              0x060d
	*/
	if(onoff == 0) {
		mad_mic_bias = snd_soc_read(codec, 0x060D);
		snd_soc_update_bits(codec, 0x060D,
					    0x07, 0x0);
	} else {
		snd_soc_update_bits(codec, 0x060D,
					    0x07, mad_mic_bias);
	}
	
	dev_info(codec->dev, "%s: onoff %d mad_mic_bias 0x%x\n", __func__, onoff, mad_mic_bias);
}

void register_mbhc_jack_cb(struct snd_soc_codec *codec)
{
	wcd_codec = codec;
#ifndef CONFIG_SEC_SND_USB_HEADSET_ONLY
	audio_register_jack_state_cb(get_jack_status);
	audio_register_key_state_cb(get_key_status);
#ifndef CONFIG_SND_SOC_WCD_MBHC
	audio_register_mic_adc_cb(get_key_status);
#endif
#endif
	audio_register_wdsp_state_cb(get_wdsp_status);
	audio_register_mad_mic_state_cb(set_mad_mic_bias_status);
}
EXPORT_SYMBOL_GPL(register_mbhc_jack_cb);

