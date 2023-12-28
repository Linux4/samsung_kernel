// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef _MSM_COMMON_H_
#define _MSM_COMMON_H_

#include <sound/soc.h>
#include <sound/pcm.h>

#ifdef CONFIG_COMMON_AMP_CIRRUS
#define CLK_SRC_SCLK 0
#define CLK_SRC_LRCLK 1
#define CLK_SRC_PDM 2
#define CLK_SRC_SELF 3
#define CLK_SRC_MCLK 4
#define CLK_SRC_SWIRE 5
#define CLK_SRC_DAI 0
#define CLK_SRC_CODEC 1

#define CS35L43_MONO   0x31
#define CS35L43_STEREO 0x32
#define CS35L43_QUAD    0x34

#define CS35L45_MONO   0x51
#define CS35L45_STEREO 0x52
#define CS35L45_QUAD    0x54
#endif

enum {
	MI2S = 0,
	TDM,
	AUXPCM,
};

enum {
	PRI_MI2S_TDM_AUXPCM = 0,
	SEC_MI2S_TDM_AUXPCM,
	TER_MI2S_TDM_AUXPCM,
	QUAT_MI2S_TDM_AUXPCM,
	QUIN_MI2S_TDM_AUXPCM,
	SEN_MI2S_TDM_AUXPCM,
	SEP_MI2S_TDM_AUXPCM,
	MI2S_TDM_AUXPCM_MAX,
};

typedef enum snd_card_status_t {
	SND_CARD_STATUS_OFFLINE = 0,
	SND_CARD_STATUS_ONLINE  = 1,
} snd_card_status_t;

struct msm_common_pdata {
	uint8_t *aud_dev_state;
	struct kobject aud_dev_kobj;
	struct mutex aud_dev_lock;
	uint32_t num_aud_devs;
	struct device_node *mi2s_gpio_p[MI2S_TDM_AUXPCM_MAX];
	struct mutex lock[MI2S_TDM_AUXPCM_MAX];
	u32 tdm_max_slots; /* Max TDM slots used */
	atomic_t mi2s_gpio_ref_cnt[MI2S_TDM_AUXPCM_MAX];
	atomic_t lpass_intf_clk_ref_cnt[MI2S_TDM_AUXPCM_MAX];
	atomic_t lpass_audio_hw_vote_ref_cnt;
	struct clk *lpass_audio_hw_vote;
	uint32_t is_audio_hw_vote_required[MI2S_TDM_AUXPCM_MAX];
	uint32_t tdm_clk_attribute[MI2S_TDM_AUXPCM_MAX];
	uint32_t mi2s_clk_attribute[MI2S_TDM_AUXPCM_MAX];
};

int snd_card_notify_user(snd_card_status_t card_status);
int snd_card_set_card_status(snd_card_status_t card_status);
struct msm_common_pdata *msm_common_get_pdata(struct snd_soc_card *card);

void msm_common_set_pdata(struct snd_soc_card *card,
			  struct msm_common_pdata *pdata);

int snd_card_sysfs_init(void);

int msm_common_snd_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params);

int msm_common_snd_startup(struct snd_pcm_substream *substream);

void msm_common_snd_shutdown(struct snd_pcm_substream *substream);

int msm_common_snd_init(struct platform_device *pdev,
			struct snd_soc_card *card);

void msm_common_snd_deinit(struct msm_common_pdata *pdata);

int msm_common_dai_link_init(struct snd_soc_pcm_runtime *rtd);
#endif
