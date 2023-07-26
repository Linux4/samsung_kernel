// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <ipc/apr_tal.h>
#include <dsp/apr_audio-v2.h>
#include <dsp/q6audio-v2.h>
#include <dsp/audio_cal_utils.h>
#include <dsp/q6voice.h>
#include <dsp/q6asm-v2.h>
#include <dsp/q6common.h>
#include <dsp/q6voice_adaptation.h>

static int sec_voice_adaptation_sound_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_voice_adaptation_sound_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int i = 0;

	int adaptation_sound_mode = ucontrol->value.integer.value[0];
	int adaptation_sound_select = ucontrol->value.integer.value[1];
	short adaptation_sound_param[12] = {0,};

	for (i = 0; i < 12; i++) {
		adaptation_sound_param[i] =
			(short)ucontrol->value.integer.value[2+i];
		pr_debug("%s: param - %d\n", __func__, adaptation_sound_param[i]);
	}

	return sec_voice_set_adaptation_sound(adaptation_sound_mode,
				adaptation_sound_select,
				adaptation_sound_param);
}

static int sec_voice_nb_mode_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_voice_nb_mode_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int enable = ucontrol->value.integer.value[0];

	pr_info("%s: enable=%d\n", __func__, enable);

	return sec_voice_set_nb_mode(enable);
}

static int sec_voice_rcv_mode_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_voice_rcv_mode_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int enable = ucontrol->value.integer.value[0];

	pr_info("%s: enable=%d\n", __func__, enable);

	return sec_voice_set_rcv_mode(enable);
}

static int sec_voice_spk_mode_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_voice_spk_mode_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int enable = ucontrol->value.integer.value[0];

	pr_debug("%s: enable=%d\n", __func__, enable);

	return sec_voice_set_spk_mode(enable);
}

static int sec_voice_bt_rvc_vol_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_voice_bt_rvc_vol_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int volume = ucontrol->value.integer.value[0];

	pr_debug("%s: volume=%d\n", __func__, volume);

	return sec_voice_set_bt_rvc_vol(volume);
}

static int sec_voice_loopback_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	int loopback_enable = ucontrol->value.integer.value[0];

	pr_info("%s: loopback enable=%d\n", __func__, loopback_enable);

	sec_voice_set_loopback_enable(loopback_enable);
	return 0;
}

static int sec_voice_loopback_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = sec_voice_get_loopback_enable();
	return 0;
}

static const char * const voice_device[] = {
	"ETC", "SPK", "EAR", "BT", "RCV"
};

static const struct soc_enum sec_voice_device_enum[] = {
	SOC_ENUM_SINGLE_EXT(5, voice_device),
};

static int sec_voice_dev_info_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_voice_dev_info_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int voc_dev = ucontrol->value.integer.value[0];

	pr_debug("%s: voice device=%d\n", __func__, voc_dev);

	return sec_voice_set_device_info(voc_dev);
}

static int sec_voice_ref_lch_mute_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_voice_ref_lch_mute_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int enable = ucontrol->value.integer.value[0];

	pr_info("%s: enable=%d\n", __func__, enable);

	return sec_voice_ref_lch_mute(enable);
}

static int sec_voice_echo_ref_mute_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int value = 0;
	int ret = 0;

	ret = sec_voice_request_echo_ref_mute();
	if (ret) {
		pr_err("%s: failed to get echo ref mute value %d\n",
			__func__, ret);
		return -EINVAL;
	}

	value = sec_voice_get_echo_ref_mute();
	ucontrol->value.integer.value[0] = value;

	pr_info("%s: value=%d\n", __func__, value);

	return 0;
}

static int sec_voice_echo_ref_mute_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static const char * const aec_switch[] = {
	"OFF", "ON"
};

static const struct soc_enum sec_aec_effect_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, aec_switch),
};

static int sec_voice_aec_effect_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_voice_aec_effect_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int enable = ucontrol->value.integer.value[0];

	pr_debug("%s: enable=%d\n", __func__, enable);

	return sec_voice_aec_effect(enable);
}

static const char * const voice_isolate[] = {
	"Standard", "VoiceFocus", "AllSound"
};

static const struct soc_enum sec_voice_isolation_enum[] = {
	SOC_ENUM_SINGLE_EXT(3, voice_isolate),
};

static int sec_voice_isolation_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_voice_isolation_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int mode = ucontrol->value.integer.value[0];

	pr_debug("%s: mode = %d\n", __func__, mode);

	return sec_voice_isolation_mode(mode);
}

static const struct snd_kcontrol_new samsung_voice_solution_mixer_controls[] = {
	SOC_SINGLE_MULTI_EXT("Sec Set DHA data", SND_SOC_NOPM, 0, 65535, 0, 14,
		sec_voice_adaptation_sound_get,
		sec_voice_adaptation_sound_put),
	SOC_SINGLE_EXT("NB Mode", SND_SOC_NOPM, 0, 1, 0,
		sec_voice_nb_mode_get,
		sec_voice_nb_mode_put),
	SOC_SINGLE_EXT("Receiver Sensor Mode", SND_SOC_NOPM, 0, 1, 0,
		sec_voice_rcv_mode_get,
		sec_voice_rcv_mode_put),
	SOC_SINGLE_EXT("Speaker Sensor Mode", SND_SOC_NOPM, 0, 1, 0,
		sec_voice_spk_mode_get,
		sec_voice_spk_mode_put),
	SOC_SINGLE_EXT("BT SCO Rx Volume", SND_SOC_NOPM, 0, 1, 0,
		sec_voice_bt_rvc_vol_get,
		sec_voice_bt_rvc_vol_put),
	SOC_SINGLE_EXT("Loopback Enable", SND_SOC_NOPM, 0, LOOPBACK_MAX, 0,
		sec_voice_loopback_get,
		sec_voice_loopback_put),
	SOC_ENUM_EXT("Voice Device Info", sec_voice_device_enum[0],
		sec_voice_dev_info_get,
		sec_voice_dev_info_put),
	SOC_SINGLE_EXT("Echo Ref Mute", SND_SOC_NOPM, 0, 2, 0,
		sec_voice_echo_ref_mute_get,
		sec_voice_echo_ref_mute_put),
	SOC_SINGLE_EXT("REF LCH MUTE", SND_SOC_NOPM, 0, 1, 0,
		sec_voice_ref_lch_mute_get,
		sec_voice_ref_lch_mute_put),
	SOC_ENUM_EXT("DSP AEC Effect", sec_aec_effect_enum[0],
		sec_voice_aec_effect_get,
		sec_voice_aec_effect_put),
	SOC_ENUM_EXT("Voice Isolation Mode", sec_voice_isolation_enum[0],
		sec_voice_isolation_get,
		sec_voice_isolation_put),
};

void sec_voice_adaptation_add_controls(struct snd_soc_component *component)
{
	pr_info("%s:\n", __func__);

	snd_soc_add_component_controls(component,
			samsung_voice_solution_mixer_controls,
			ARRAY_SIZE(samsung_voice_solution_mixer_controls));
}
