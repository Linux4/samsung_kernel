/*
 * sound/soc/sprd/sprd-asoc-card.c
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
#define pr_fmt(fmt) pr_sprd_fmt("BOARD") fmt

#include <linux/module.h>
#include <linux/of.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <sound/sprd-audio-hook.h>

#include <mach/sprd-audio.h>
#include "sprd-asoc-common.h"
#include "dai/vbc/dfm.h"

#define FUN_REG(f) ((unsigned short)(-(f+1)))

#define SWITCH_FUN_ON    1
#define SWITCH_FUN_OFF   0

enum {
	BOARD_FUNC_SPKL = 0,
	BOARD_FUNC_SPKR,
	BOARD_FUNC_EAR,
	BOARD_FUNC_HP,
	BOARD_FUNC_MUTE_MAX,
	BOARD_FUNC_LINE = BOARD_FUNC_MUTE_MAX,
	BOARD_FUNC_MIC,
	BOARD_FUNC_AUXMIC,
	BOARD_FUNC_HP_MIC,
	BOARD_FUNC_DMIC,
	BOARD_FUNC_DMIC1,
	BOARD_FUNC_DFM,
	BOARD_FUNC_MAX
};

struct board_mute {
	int need_mute;
	int is_on;
	int (*mute_func) (struct snd_soc_card *, int);
};

static struct board_priv {
	int func[BOARD_FUNC_MAX];
	struct board_mute m[BOARD_FUNC_MUTE_MAX];
	int pa_type[BOARD_FUNC_MUTE_MAX];
} board;

#define BOARD_EXT_SPK "Ext Spk"
#define BOARD_EXT_SPK2 "Ext Spk2"
#define BOARD_EXT_EAR "Ext Ear"
#define BOARD_HEADPHONE_JACK "HeadPhone Jack"
#define BOARD_LINE_JACK "Line Jack"
#define BOARD_MIC_JACK "Mic Jack"
#define BOARD_AUX_MIC_JACK "Aux Mic Jack"
#define BOARD_HP_MIC_JACK "HP Mic Jack"
#define BOARD_DMIC_JACK "DMic Jack"
#define BOARD_DMIC1_JACK "DMic1 Jack"
#define BOARD_DIG_FM_JACK "Dig FM Jack"
#define BOARD_INTER_HP_PA "inter HP PA"
#define BOARD_INTER_SPK_PA "inter Spk PA"
#define BOARD_INTER_SPK2_PA "inter Spk2 PA"

static const char *func_name[BOARD_FUNC_MAX] = {
	BOARD_EXT_SPK,
	BOARD_EXT_SPK2,
	BOARD_EXT_EAR,
	BOARD_HEADPHONE_JACK,
	BOARD_LINE_JACK,
	BOARD_MIC_JACK,
	BOARD_AUX_MIC_JACK,
	BOARD_HP_MIC_JACK,
	BOARD_DMIC_JACK,
	BOARD_DMIC1_JACK,
	BOARD_DIG_FM_JACK,
};

static void board_ext_control(struct snd_soc_dapm_context *dapm, int s, int e)
{
	int i;
	BUG_ON(e > BOARD_FUNC_MAX);
	for (i = s; i < e; i++) {
		if (board.func[i] == SWITCH_FUN_ON)
			snd_soc_dapm_enable_pin(dapm, func_name[i]);
		else
			snd_soc_dapm_disable_pin(dapm, func_name[i]);
	}

	/* signal a DAPM event */
	snd_soc_dapm_sync(dapm);
}

static inline void local_cpu_pa_control(struct snd_soc_card *card,
					const char *id, bool enable, bool sync)
{
	if (enable) {
		snd_soc_dapm_enable_pin(&card->dapm, id);
	} else {
		snd_soc_dapm_disable_pin(&card->dapm, id);
	}

	if (sync) {
		/* signal a DAPM event */
		snd_soc_dapm_sync(&card->dapm);
	}
}

static void board_try_inter_pa_control(struct snd_soc_card *card, int id,
				       bool sync)
{
	char *pa_name;

	if (id >= BOARD_FUNC_MUTE_MAX)
		return;

	switch (id) {
	case BOARD_FUNC_SPKL:
		pa_name = BOARD_INTER_SPK_PA;
		break;
	case BOARD_FUNC_SPKR:
		pa_name = BOARD_INTER_SPK2_PA;
		break;
	case BOARD_FUNC_HP:
		pa_name = BOARD_INTER_HP_PA;
		break;
	default:
		return;
	}

	if (board.pa_type[id] & HOOK_BPY) {
		int enable = 1;
		if (board.func[id] == SWITCH_FUN_ON) {
			if (board.m[id].need_mute) {
				enable = 0;
			}
		} else {
			enable = 0;
		}
		local_cpu_pa_control(card, pa_name, enable, sync);
	}
}

static int audio_speaker_enable_inter(struct snd_soc_card *card, int enable)
{
	int ret;
	if (enable && board.m[BOARD_FUNC_SPKL].need_mute) {
		enable = 0;
	}
	ret = sprd_ext_speaker_ctrl(SPRD_AUDIO_ID_SPEAKER, enable);
	if (ret < 0) {
		pr_err("ERR:Call external speaker control failed %d!\n", ret);
		return ret;
	}
	return ret;
}

static void audio_speaker_enable(struct snd_soc_card *card, int enable)
{
	board.m[BOARD_FUNC_SPKL].is_on = enable;
	board.m[BOARD_FUNC_SPKL].mute_func(card, enable);
}

static int audio_speaker2_enable_inter(struct snd_soc_card *card, int enable)
{
	int ret;
	if (enable && board.m[BOARD_FUNC_SPKR].need_mute) {
		enable = 0;
	}
	ret = sprd_ext_speaker_ctrl(SPRD_AUDIO_ID_SPEAKER2, enable);
	if (ret < 0) {
		pr_err("ERR:Call external speaker2 control failed %d!\n", ret);
		return ret;
	}
	return ret;
}

static void audio_speaker2_enable(struct snd_soc_card *card, int enable)
{
	board.m[BOARD_FUNC_SPKR].is_on = enable;
	board.m[BOARD_FUNC_SPKR].mute_func(card, enable);
}

static int audio_headphone_enable_inter(struct snd_soc_card *card, int enable)
{
	int ret;
	if (enable && board.m[BOARD_FUNC_HP].need_mute) {
		enable = 0;
	}
	ret = sprd_ext_headphone_ctrl(SPRD_AUDIO_ID_HEADPHONE, enable);
	if (ret < 0) {
		pr_err("ERR:Call external headphone control failed %d!\n", ret);
		return ret;
	}
	return ret;
}

static void audio_headphone_enable(struct snd_soc_card *card, int enable)
{
	board.m[BOARD_FUNC_HP].is_on = enable;
	board.m[BOARD_FUNC_HP].mute_func(card, enable);
}

static int audio_earpiece_enable_inter(struct snd_soc_card *card, int enable)
{
	int ret;
	if (enable && board.m[BOARD_FUNC_EAR].need_mute) {
		enable = 0;
	}
	ret = sprd_ext_earpiece_ctrl(SPRD_AUDIO_ID_EARPIECE, enable);
	if (ret < 0) {
		pr_err("ERR:Call external earpiece control failed %d!\n", ret);
	}
	return ret;
}

static void audio_earpiece_enable(struct snd_soc_card *card, int enable)
{
	board.m[BOARD_FUNC_EAR].is_on = enable;
	board.m[BOARD_FUNC_EAR].mute_func(card, enable);
}

static int board_headphone_event(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *k, int event)
{
	int on = ! !SND_SOC_DAPM_EVENT_ON(event);
	sp_asoc_pr_dbg("Headphone Switch %s\n", STR_ON_OFF(on));
	audio_headphone_enable(w->dapm->card, on);
	return 0;
}

static int board_earpiece_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *k, int event)
{
	int on = ! !SND_SOC_DAPM_EVENT_ON(event);
	sp_asoc_pr_dbg("Earpiece Switch %s\n", STR_ON_OFF(on));
	audio_earpiece_enable(w->dapm->card, on);
	return 0;
}

static int board_speaker_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *k, int event)
{
	int on = ! !SND_SOC_DAPM_EVENT_ON(event);
	sp_asoc_pr_dbg("Speaker Switch %s\n", STR_ON_OFF(on));
	audio_speaker_enable(w->dapm->card, on);
	return 0;
}

static int board_speaker2_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *k, int event)
{
	int on = ! !SND_SOC_DAPM_EVENT_ON(event);
	sp_asoc_pr_dbg("Speaker2 Switch %s\n", STR_ON_OFF(on));
	audio_speaker2_enable(w->dapm->card, on);
	return 0;
}

static int board_main_mic_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *k, int event)
{
	int on = ! !SND_SOC_DAPM_EVENT_ON(event);
	sp_asoc_pr_dbg("Main MIC Switch %s\n", STR_ON_OFF(on));
	sprd_ext_mic_ctrl(SPRD_AUDIO_ID_MAIN_MIC, on);
	return 0;
}

static int board_sub_mic_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *k, int event)
{
	int on = ! !SND_SOC_DAPM_EVENT_ON(event);
	sp_asoc_pr_dbg("Sub MIC Switch %s\n", STR_ON_OFF(on));
	sprd_ext_mic_ctrl(SPRD_AUDIO_ID_SUB_MIC, on);
	return 0;
}

static int board_head_mic_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *k, int event)
{
	int on = ! !SND_SOC_DAPM_EVENT_ON(event);
	sp_asoc_pr_dbg("Head MIC Switch %s\n", STR_ON_OFF(on));
	sprd_ext_mic_ctrl(SPRD_AUDIO_ID_HEAD_MIC, on);
	return 0;
}

static int board_dig0_mic_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *k, int event)
{
	int on = ! !SND_SOC_DAPM_EVENT_ON(event);
	sp_asoc_pr_dbg("Digtial0 MIC Switch %s\n", STR_ON_OFF(on));
	sprd_ext_mic_ctrl(SPRD_AUDIO_ID_DIG0_MIC, on);
	return 0;
}

static int board_dig1_mic_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *k, int event)
{
	int on = ! !SND_SOC_DAPM_EVENT_ON(event);
	sp_asoc_pr_dbg("Digtial1 MIC Switch %s\n", STR_ON_OFF(on));
	sprd_ext_mic_ctrl(SPRD_AUDIO_ID_DIG1_MIC, on);
	return 0;
}

static int board_line_in_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *k, int event)
{
	int on = ! !SND_SOC_DAPM_EVENT_ON(event);
	sp_asoc_pr_dbg("LINE IN Switch %s\n", STR_ON_OFF(on));
	sprd_ext_mic_ctrl(SPRD_AUDIO_ID_LINE_IN, on);
	return 0;
}

static int board_dig_fm_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *k, int event)
{
	int on = ! !SND_SOC_DAPM_EVENT_ON(event);
	sp_asoc_pr_dbg("Digtial FM Switch %s\n", STR_ON_OFF(on));
	sprd_ext_fm_ctrl(SPRD_AUDIO_ID_DIG_FM, on);
	return 0;
}

static const struct snd_soc_dapm_widget sprd_codec_dapm_widgets[] = {
	SND_SOC_DAPM_MIC(BOARD_MIC_JACK, board_main_mic_event),
	SND_SOC_DAPM_MIC(BOARD_AUX_MIC_JACK, board_sub_mic_event),
	SND_SOC_DAPM_MIC(BOARD_HP_MIC_JACK, board_head_mic_event),
	SND_SOC_DAPM_MIC(BOARD_DMIC_JACK, board_dig0_mic_event),
	SND_SOC_DAPM_MIC(BOARD_DMIC1_JACK, board_dig1_mic_event),
	/*digital fm input */
	SND_SOC_DAPM_LINE(BOARD_DIG_FM_JACK, board_dig_fm_event),

	SND_SOC_DAPM_SPK(BOARD_EXT_SPK, board_speaker_event),
	SND_SOC_DAPM_SPK(BOARD_EXT_SPK2, board_speaker2_event),
	SND_SOC_DAPM_SPK(BOARD_EXT_EAR, board_earpiece_event),
	SND_SOC_DAPM_LINE(BOARD_LINE_JACK, board_line_in_event),
	SND_SOC_DAPM_HP(BOARD_HEADPHONE_JACK, board_headphone_event),
	SND_SOC_DAPM_HP(BOARD_INTER_HP_PA, NULL),
	SND_SOC_DAPM_SPK(BOARD_INTER_SPK_PA, NULL),
	SND_SOC_DAPM_SPK(BOARD_INTER_SPK2_PA, NULL),
};

static int board_func_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);
	ucontrol->value.integer.value[0] = board.func[id];
	return 0;
}

static int board_func_set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	int id = FUN_REG(mc->reg);

	sp_asoc_pr_info("%s Switch %s\n", func_name[id],
			STR_ON_OFF(ucontrol->value.integer.value[0]));

	if (board.func[id] == ucontrol->value.integer.value[0])
		return 0;

	board.func[id] = ucontrol->value.integer.value[0];
	board_try_inter_pa_control(card, id, 1);
	board_ext_control(&card->dapm, id, id + 1);
	return 1;
}

static int board_mute_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);
	ucontrol->value.integer.value[0] = board.m[id].need_mute;
	return 0;
}

static int board_mute_set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	int id = FUN_REG(mc->reg);

	sp_asoc_pr_info("%s Switch %s\n", func_name[id],
			ucontrol->value.integer.value[0] ? "Mute" : "Unmute");

	if (board.m[id].need_mute == ucontrol->value.integer.value[0])
		return 0;

	board.m[id].need_mute = ucontrol->value.integer.value[0];
	board.m[id].mute_func(card, board.m[id].is_on);
	board_try_inter_pa_control(card, id, 1);
	return 1;
}

static void board_inter_pa_check(int func_id)
{
	switch (func_id) {
	case BOARD_FUNC_SPKL:
		board.pa_type[func_id] =
		    sprd_ext_speaker_ctrl(SPRD_AUDIO_ID_SPEAKER, 0);
		break;
	case BOARD_FUNC_SPKR:
		board.pa_type[func_id] =
		    sprd_ext_speaker_ctrl(SPRD_AUDIO_ID_SPEAKER2, 0);
		break;
	case BOARD_FUNC_EAR:
		board.pa_type[func_id] =
		    sprd_ext_earpiece_ctrl(SPRD_AUDIO_ID_EARPIECE, 0);
		break;
	case BOARD_FUNC_HP:
		board.pa_type[func_id] =
		    sprd_ext_headphone_ctrl(SPRD_AUDIO_ID_HEADPHONE, 0);
		break;
	default:
		return;
	}
}

static void board_inter_pa_init(void)
{
	int id;
	for (id = BOARD_FUNC_SPKL; id < BOARD_FUNC_MUTE_MAX; id++)
		board_inter_pa_check(id);
}

static void board_inter_pa_control(struct snd_soc_card *card)
{
	int id;
	for (id = BOARD_FUNC_SPKL; id < BOARD_FUNC_MUTE_MAX; id++)
		board_try_inter_pa_control(card, id, 0);
}

static int board_late_probe(struct snd_soc_card *card)
{
	int i;
	sprd_audio_debug_init(card->snd_card);

	board_inter_pa_control(card);
	board_ext_control(&card->dapm, 0, BOARD_FUNC_MAX);

	for (i = 0; i < BOARD_FUNC_MAX; i++) {
		snd_soc_dapm_ignore_suspend(&card->dapm, func_name[i]);
	}

	snd_soc_dapm_ignore_suspend(&card->dapm, BOARD_INTER_HP_PA);
	snd_soc_dapm_ignore_suspend(&card->dapm, BOARD_INTER_SPK_PA);
	snd_soc_dapm_ignore_suspend(&card->dapm, BOARD_INTER_SPK2_PA);
	snd_soc_dapm_ignore_suspend(&card->rtd->codec->dapm, "Playback-Vaudio");
	snd_soc_dapm_ignore_suspend(&card->rtd->codec->dapm,
				    "Main-Capture-Vaudio");
	snd_soc_dapm_ignore_suspend(&card->rtd->codec->dapm,
				    "Ext-Capture-Vaudio");
	snd_soc_dapm_ignore_suspend(&card->rtd->codec->dapm, "DFM-Output");
	return 0;
}

static void board_mute_init(void)
{
	board.m[BOARD_FUNC_SPKL].mute_func = audio_speaker_enable_inter;
	board.m[BOARD_FUNC_SPKR].mute_func = audio_speaker2_enable_inter;
	board.m[BOARD_FUNC_HP].mute_func = audio_headphone_enable_inter;
	board.m[BOARD_FUNC_EAR].mute_func = audio_earpiece_enable_inter;
}

static int sprd_asoc_probe(struct platform_device *pdev,
			   struct snd_soc_card *card)
{
	struct device_node *node = pdev->dev.of_node;
	card->dev = &pdev->dev;
	if (node) {
		int i;
		struct device_node *pcm_node;
		struct device_node *codec_node;

		if (snd_soc_of_parse_card_name(card, "sprd,model")) {
			pr_err("ERR:Card name is not provided\n");
			return -ENODEV;
		}
#if 0
		ret = snd_soc_of_parse_audio_routing(card,
						     "sprd,audio-routing");
		if (ret) {
			pr_err("ERR:Error while parsing DAPM routing\n");
			return ret;
		}
#endif

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

		for (i = 0; i < card->num_links; i++) {
			card->dai_link[i].platform_name = NULL;
			card->dai_link[i].platform_of_node = pcm_node;
			card->dai_link[i].codec_name = NULL;
			card->dai_link[i].codec_of_node = codec_node;
		}
		of_node_put(pcm_node);
		of_node_put(codec_node);
	}

	board_mute_init();
	board_inter_pa_init();
	return snd_soc_register_card(card);
}

static int sprd_asoc_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	snd_soc_unregister_card(card);
	return 0;
}

static void sprd_asoc_shutdown(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	memset(&board.func, 0, sizeof(board.func));
	board_inter_pa_control(card);
	board_ext_control(&card->dapm, 0, BOARD_FUNC_MAX);
}

#define BOARD_CODEC_FUNC(xname, xreg) \
	SOC_SINGLE_EXT(xname, FUN_REG(xreg), 0, 1, 0, board_func_get, board_func_set)

#define BOARD_CODEC_MUTE(xname, xreg) \
	SOC_SINGLE_EXT(xname, FUN_REG(xreg), 0, 1, 0, board_mute_get, board_mute_set)

#if defined(CONFIG_SND_SOC_SPRD_VBC_R2P0_SPRD_CODEC_V4)
#include "vbc-r2p0-sprd-codec-v4.h"
#else
#include "vbc-r1p0-sprd-codec-v1.h"
#include "vbc-r2p0-sprd-codec-v3.h"
#endif

MODULE_DESCRIPTION("ALSA ASoC SpreadTrum VBC SPRD-CODEC");
MODULE_AUTHOR("Zhenfang Wang <zhenfang.wang@spreadtrum.com>");
MODULE_AUTHOR("Ken Kuang <ken.kuang@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("machine:vbc-sprd-codec");
