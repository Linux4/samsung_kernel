/*
 * linux/sound/sprd-audio-hook.h
 *
 * SPRD ASoC Audio Hook -- Customer implement.
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
#ifndef __LINUX_SND_SOC_SPRD_AUDIO_HOOK_H
#define __LINUX_SND_SOC_SPRD_AUDIO_HOOK_H

enum {
	/* hook function implemented, and execute normally */
	HOOK_OK = 0x1,
	/* hook function implemented, but not hook this parameter */
	HOOK_BPY = 0x2,
	/* not implement the hook functions */
	NO_HOOK = HOOK_BPY,
};

enum {
	SPRD_AUDIO_ID_NONE = 0,
	SPRD_AUDIO_ID_SPEAKER = 0x00000001,
	SPRD_AUDIO_ID_SPEAKER2 = 0x00000002,
	SPRD_AUDIO_ID_HEADPHONE = 0x00000004,
	SPRD_AUDIO_ID_EARPIECE = 0x00000008,
	SPRD_AUDIO_ID_DIG_FM = 0x00000010,

	SPRD_AUDIO_ID_MAIN_MIC = 0x80010000,
	SPRD_AUDIO_ID_SUB_MIC = 0x80020000,
	SPRD_AUDIO_ID_HEAD_MIC = 0x80040000,
	SPRD_AUDIO_ID_DIG0_MIC = 0x80080000,
	SPRD_AUDIO_ID_DIG1_MIC = 0x80100000,
	SPRD_AUDIO_ID_LINE_IN = 0x80200000,
};

int sprd_ext_speaker_ctrl(int id, int on);
int sprd_ext_headphone_ctrl(int id, int on);
int sprd_ext_earpiece_ctrl(int id, int on);
int sprd_ext_mic_ctrl(int id, int on);
int sprd_ext_fm_ctrl(int id, int on);

typedef int (*sprd_audio_hook_func)(int id, int on);

struct sprd_audio_ext_hook {
	sprd_audio_hook_func ext_speaker_ctrl;
	sprd_audio_hook_func ext_headphone_ctrl;
	sprd_audio_hook_func ext_earpiece_ctrl;
	sprd_audio_hook_func ext_mic_ctrl;
	sprd_audio_hook_func ext_fm_ctrl;
};

int sprd_ext_hook_register(struct sprd_audio_ext_hook *hook);
int sprd_ext_hook_unregister(struct sprd_audio_ext_hook *hook);

#endif /* __LINUX_SND_SOC_SPRD_AUDIO_HOOK_H */
