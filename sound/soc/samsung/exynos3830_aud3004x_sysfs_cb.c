/*
 *  exynos3830_aud3004x_sysfs_cb.c
 *  Copyright (c) Samsung Electronics
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 */

#include <linux/input.h>
#include <sound/soc.h>
#include <sound/samsung/sec_audio_sysfs.h>

#include "exynos3830_aud3004x_sysfs_cb.h"
#include "../codecs/aud3004x.h"


#if IS_ENABLED(CONFIG_SND_SOC_AUD3004X_5PIN)
#include "../codecs/aud3004x-5pin.h"
#endif /* CONFIG_SND_SOC_AUD3004X_5PIN */
#if IS_ENABLED(CONFIG_SND_SOC_AUD3004X_6PIN)
#include "../codecs/aud3004x-6pin.h"
#endif /* CONFIG_SND_SOC_AUD3004X_6PIN */


static struct snd_soc_component *aud3004x_component;


static int get_jack_status(void)
{
	struct snd_soc_component *component = aud3004x_component;
	struct aud3004x_priv *aud3004x = dev_get_drvdata(component->dev);
	struct aud3004x_jack *jackdet = aud3004x->p_jackdet;
	struct earjack_state *jack_state = &jackdet->jack_state;
	int report = 0;

	if (jack_state->cur_jack_state & JACK_IN)
		report = 1;

	dev_info(component->dev, "%s: %d\n", __func__, report);

	return report;
}

static int get_key_status(void)
{
	struct snd_soc_component *component = aud3004x_component;
	struct aud3004x_priv *aud3004x = dev_get_drvdata(component->dev);
	struct aud3004x_jack *jackdet = aud3004x->p_jackdet;
	struct earjack_state *jack_state = &jackdet->jack_state;
	int report = 0;

	if (jack_state->cur_btn_state == BUTTON_PRESS)
		report = 1;

	dev_info(component->dev, "%s: %d\n", __func__, report);

	return report;
}

static int get_mic_adc(void)
{
	struct snd_soc_component *component = aud3004x_component;
	struct aud3004x_priv *aud3004x = dev_get_drvdata(component->dev);
	struct aud3004x_jack *jackdet = aud3004x->p_jackdet;
	struct earjack_state *jack_state = &jackdet->jack_state;
	int report = 0;

	report = jack_state->btn_adc;

	dev_info(component->dev, "%s: %d\n", __func__, report);

	return report;
}

void register_exynos3830_aud3004x_sysfs_cb(struct snd_soc_component *component)
{
	aud3004x_component = component;

	audio_register_jack_state_cb(get_jack_status);
	audio_register_key_state_cb(get_key_status);
	audio_register_mic_adc_cb(get_mic_adc);

}
EXPORT_SYMBOL_GPL(register_exynos3830_aud3004x_sysfs_cb);
