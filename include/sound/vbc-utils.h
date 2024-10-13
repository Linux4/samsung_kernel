/*
 * include/sound/vbc-utils.h
 *
 * SPRD SoC VBC -- SpreadTrum SOC utils function for VBC DAI.
 *
 * Copyright (C) 2013 SpreadTrum Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY ork FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __VBC_UTILS_H
#define __VBC_UTILS_H

#include <sound/audio-smsg.h>
#include <linux/device.h>
#include <../../drivers/mcdt/mcdt_phy_v0.h>

#define SPRD_VBC_VERSION	"vbc.r3p0"
/* store vbc pathset configuration for dsp */
#define VBC_PATHSET_CONFIG_MAX 30
/*mcdt channel 5 6 7 has been used by modem*/

/*voip*/
#define MCDT_CHAN_VOIP MCDT_CHAN1
#define MCDT_AP_DMA_CHAN_LOOP MCDT_AP_DMA_CH1

#define MCDT_FULL_WMK_VOIP (160)
#define MCDT_EMPTY_WMK_VOIP (160)

/*voice capture*/
#define MCDT_CHAN_VOICE_CAPTURE MCDT_CHAN2
#define MCDT_AP_DMA_CHAN_VOICE_CAPTURE MCDT_AP_DMA_CH2

#define MCDT_FULL_WMK_VOICE_CAPTURE 80
#define MCDT_FIFO_SIZE_VOICE_CAPTURE 512

/*loop back (mcdt)*/
#define MCDT_CHAN_LOOP MCDT_CHAN3
#define MCDT_AP_DMA_CHAN_LOOP MCDT_AP_DMA_CH3

#define MCDT_FULL_WMK_LOOP 122
#define MCDT_EMPTY_WMK_LOOP 122

/*fast play*/
#define MCDT_CHAN_FAST_PLAY MCDT_CHAN4
#define MCDT_AP_DMA_CHAN_FAST_PLAY MCDT_AP_DMA_CH4

#define MCDT_FULL_WMK_FAST_PLAY 272
#define MCDT_EMPTY_WMK_FAST_PLAY 240






typedef enum {
	AUD_AP_SRC_MODE_48000,
	AUD_AP_SRC_MODE_44100,
	AUD_AP_SRC_MODE_32000,
	AUD_AP_SRC_MODE_24000,
	AUD_AP_SRC_MODE_22050,
	AUD_AP_SRC_MODE_16000,
	AUD_AP_SRC_MODE_12000,
	AUD_AP_SRC_MODE_11025,
	AUD_AP_SRC_MODE_NA,
	AUD_AP_SRC_MODE_8000
}AUD_AP_SRC_MODE_E;

typedef enum {
	VBC_DA0,
	VBC_DA1
} VBC_DA_ID_E;

typedef enum {
	VBC_AD0,
	VBC_AD1,
	VBC_AD2,
	VBC_AD3
} VBC_AD_ID_E;

typedef enum {//this is dai id, comunicate with dsp, negotiate with dsp.
	VBC_DAI_ID_NORMAL_OUTDSP = 0,
	VBC_DAI_ID_NORMAL_WITHDSP,
	VBC_DAI_ID_FAST_P,
	VBC_DAI_ID_OFFLOAD,
	VBC_DAI_ID_VOICE,	//default ad2 + da1
	VBC_DAI_ID_VOIP,	//default ad2 + da1
	VBC_DAI_ID_FM,		//default ad3 + outdsp
	VBC_DAI_ID_FM_C_WITHDSP,
	VBC_DAI_ID_VOICE_CAPTURE,
	VBC_DAI_ID_LOOP_RECORD,
	VBC_DAI_ID_LOOP_PLAY,
	VBC_DAI_ID_VOIP_RECORD,
	VBC_DAI_ID_VOIP_PLAY,
	VBC_DAI_ID_FM_CAPTURE,
//	VBC_DAI_ID_BT_CALL,
	VBC_DAI_ID_MAX
}VBC_DAI_ID_E;

typedef enum {
	SND_PCM_PLAYBACK = 0,
	SND_PCM_CAPTURE,
	SND_PCM_DIRECTION_MAX
}SND_PCM_DIRECTION_E;

typedef enum {
	SND_PCM_TRIGGER_CMD_STOP = 0,
	SND_PCM_TRIGGER_CMD_START,
	SND_PCM_TRIGGER_CMD_PAUSE_PUSH = 3,
	SND_PCM_TRIGGER_CMD_PAUSE_RELEASE,
	SND_PCM_TRIGGER_CMD_SUSPEND,
	SND_PCM_TRIGGER_CMD_RESUME,
	SND_PCM_TRIGGER_CMD_END
}SND_PCM_TRIGGER_CMD_E;

/* smsg command definition for dai driver according to ASOC */
enum {
	SND_VBC_DSP_FUNC_BEGIN = SMSG_CMD_NR,/*3*/
	SND_VBC_DSP_FUNC_STARTUP,/*4*/
	SND_VBC_DSP_FUNC_SHUTDOWN,/*5*/
	SND_VBC_DSP_FUNC_HW_PARAMS,/*6*/
	SND_VBC_DSP_FUNC_HW_FREE,/*7*/
	SND_VBC_DSP_FUNC_HW_PREPARE,/*8*/
	SND_VBC_DSP_FUNC_HW_TRIGGER,/*9*/
	SND_VBC_DSP_FUNC_END,/*10*/
};

/* smsg command definition for IO command */
enum {
	SND_VBC_DSP_IO_KCTL_GET = SND_VBC_DSP_FUNC_END,/*10*/
	SND_VBC_DSP_IO_KCTL_SET,/*11*/
	SND_VBC_DSP_IO_SHAREMEM_GET,/*12*/
	SND_VBC_DSP_IO_SHAREMEM_SET,/*13*/
	SND_VBC_DSP_IO_CMD_END,/*14*/
};

/*src*/
typedef enum {
	AUD_SRC_MODE_48000 = 0,
	AUD_SRC_MODE_44100,
	AUD_SRC_MODE_32000,
	AUD_SRC_MODE_24000,
	AUD_SRC_MODE_22050,
	AUD_SRC_MODE_16000,
	AUD_SRC_MODE_12000,
	AUD_SRC_MODE_11025,
	AUD_SRC_MODE_NA,
	AUD_SRC_MODE_8000,
	AUD_SRC_MODE_MAX,
}VBC_SRC_MODE_E;

typedef enum {
	VBC_LOOPBACK_ADDA,
	VBC_LOOPBACK_AD_ULDL_DA_PROCESS,//echo cancellation, noise cancellation etc
	VBC_LOOPBACK_AD_UL_ENCODE_DECODE_DL_DA_PROCESS,
	VBC_LOOPBACK_TYPE_MAX,
}VBC_LOOPBACK_TYPE_E;

enum {
	SND_KCTL_TYPE_REG = 0,
	SND_KCTL_TYPE_MDG,
	SND_KCTL_TYPE_SRC,
	SND_KCTL_TYPE_DG,
	SND_KCTL_TYPE_SMTHDG,
	SND_KCTL_TYPE_SMTHDG_STEP,
	SND_KCTL_TYPE_MIXERDG_MAIN,
	SND_KCTL_TYPE_MIXERDG_MIX,
	SND_KCTL_TYPE_MIXERDG_STEP,
	SND_KCTL_TYPE_MIXER,
	SND_KCTL_TYPE_MUX,
    SND_KCTL_TYPE_BT_CALL,
    SND_KCTL_TYPE_VOLUME = SND_KCTL_TYPE_BT_CALL,
	SND_KCTL_TYPE_ADDER,
	SND_KCTL_TYPE_LOOPBACK_TYPE,
	SND_KCTL_TYPE_DATAPATH,
	SND_KCTL_TYPE_END,
};

/* share memory cmd use type begin */
/* audio profile type fix */
typedef enum {
	SND_VBC_PROFILE_START = 0,
	SND_VBC_PROFILE_AUDIO_STRUCTURE =SND_VBC_PROFILE_START,
	SND_VBC_PROFILE_DSP,
	SND_VBC_PROFILE_NXP,
	SND_VBC_PROFILE_MAX
} SND_VBC_PROFILE_USE_E;

typedef enum {
	SND_VBC_SHM_START = SND_VBC_PROFILE_MAX,
	SND_VBC_SHM_VBC_REG = SND_VBC_SHM_START,
	SND_VBC_SHM_MAX
} SND_VBC_SHM_USE_E;
/* share memory cmd use type end */

enum {
	VBC_LEFT = 0,
	VBC_RIGHT = 1,
	VBC_ALL_CHAN = 2,
	VBC_CHAN_MAX
};

typedef enum {
	VBC_DAT_H24 = 0,
	VBC_DAT_L24,
	VBC_DAT_H16,
	VBC_DAT_L16
}VBC_DAT_FORMAT;

typedef enum {
	VBC_MDG_START = SND_VBC_PROFILE_MAX,
	VBC_MDG_DAC0_DSP = VBC_MDG_START,
	VBC_MDG_DAC1_DSP,
	VBC_MDG_DAC0_AUD,
	VBC_MDG_DAC1_AUD,	//not realized, reserved for feature
	VBC_MDG_MAX
}VBC_MDG_ID_E;

typedef enum {
	VBC_SRC_DAC0 = 0,
	VBC_SRC_DAC1,	//not open
	VBC_SRC_ADC0,
	VBC_SRC_ADC1,
	VBC_SRC_ADC2,
	VBC_SRC_ADC3,
	VBC_SRC_BT,
	VBC_SRC_FM,
	VBC_SRC_AUDPLY,
	VBC_SRC_AUDRCD,
	VBC_SRC_MAX
}VBC_SRC_ID_E;

typedef enum {
	VBC_DG_START = VBC_MDG_MAX,
	VBC_DG_DAC0 = VBC_DG_START,
	VBC_DG_DAC1,
	VBC_DG_ADC0,
	VBC_DG_ADC1,
	VBC_DG_ADC2,
	VBC_DG_ADC3,
	VBC_DG_FM,
	VBC_DG_ST,
	OFFLOAD_DG,
	VBC_DG_MAX
}VBC_DG_ID_E;

typedef enum {
	VBC_SMTHDG_START = VBC_DG_MAX,
	VBC_SMTHDG_DAC0 = VBC_SMTHDG_START,
	VBC_SMTHDG_DAC1,
	VBC_SMTHDG_MAX
}VBC_SMTHDG_ID_E;

typedef enum {
	VBC_MIXERDG_START = VBC_SMTHDG_MAX,
	VBC_MIXERDG_DAC0 = VBC_MIXERDG_START,	//dac0(audio && voice)
	VBC_MIXERDG_DAC1,		//dac1(voice && audio)
	VBC_MIXERDG_MAX
} VBC_MIXERDG_ID_E;

typedef enum {
	VBC_MIXER0_DAC0 = 0,
	VBC_MIXER1_DAC0,
	VBC_MIXER2_DAC0,
	VBC_MIXER0_DAC1,
	VBC_MIXER1_DAC1,
	VBC_MIXER2_DAC1,
	VBC_MIXER_ST,
	VBC_MIXER_FM,
	VBC_MIXER_MAX
}VBC_MIXER_INDEX_E;

/*vbc mux setting*/
typedef enum {
	VBC_MUX_START = VBC_MIXERDG_MAX,
	VBC_MUX_ADC0_SOURCE = VBC_MUX_START,	//adc0 mux: iis0 or vbc_if
	VBC_MUX_ADC0_INSEL,
	VBC_MUX_ADC1_SOURCE,		//adc1 mux
	VBC_MUX_ADC1_INSEL,
	VBC_MUX_ADC2_SOURCE,		//adc2 mux
	VBC_MUX_ADC2_INSEL,
	VBC_MUX_ADC3_SOURCE,		//adc3 mux
	VBC_MUX_ADC3_INSEL,
	VBC_MUX_FM_INSEL,			//fm mux
	VBC_MUX_ST_INSEL,			//st mux
	VBC_MUX_DAC0_ADC_INSEL,		//loopback mux
	VBC_MUX_DAC1_ADC_INSEL,
	VBC_MUX_DAC0_DAC1_INSEL,
	VBC_MUX_AUDRCD_INSEL1,		//record without dsp
	//VBC_MUX_AUDRCD_INSEL2,
	VBC_MUX_DAC0_OUT_SEL,		//0:iis; 1:vbc_if;
	VBC_MUX_DAC1_OUT_SEL,		//0:iis; 1:vbc_if;
	VBC_MUX_DAC0_IIS_SEL,		//0:iis0; 1:iis1; 2:iis2; 3:iis3;
	VBC_MUX_DAC1_IIS_SEL,		//0:iis0; 1:iis1; 2:iis2; 3:iis3;
	VBC_MUX_ADC0_IIS_SEL,		//0:iis0; 1:iis1; 2:iis2; 3:iis3;
	VBC_MUX_ADC1_IIS_SEL,		//0:iis0; 1:iis1; 2:iis2; 3:iis3;
	VBC_MUX_ADC2_IIS_SEL,		//0:iis0; 1:iis1; 2:iis2; 3:iis3;
	VBC_MUX_ADC3_IIS_SEL,		//0:iis0; 1:iis1; 2:iis2; 3:iis3;
	VBC_MUX_MAX
}VBC_MUX_ID_E;

typedef enum {
	VBC_ADDER_START = VBC_MUX_MAX,
	VBC_ADDER_AUDPLY_DAC0 = VBC_ADDER_START,
	VBC_ADDER_FM_DAC0,
	VBC_ADDER_SMTHDG_VOICE_DAC0,
	VBC_ADDER_MBDRC_VOICE_DAC0,
	VBC_ADDER_ST_DAC0,
	VBC_ADDER_AUDPLY_DAC1,	//reserved for future
	VBC_ADDER_FM_DAC1,
	VBC_ADDER_SMTHDG_VOICE_DAC1,
	VBC_ADDER_MBDRC_VOICE_DAC1,
	VBC_ADDER_ST_DAC1,
	VBC_ADDER_MAX
} VBC_ADDER_ID_E;

typedef enum {
	VBC_ADDER_MODE_IGNORE = 0,
	VBC_ADDER_MODE_ADD,
	VBC_ADDER_MODE_MINUS,
	VBC_ADDER_MODE_MAX,
} VBC_ADDER_MODE_E;

typedef enum {
    VBC_DP_EN_START = 0,
	VBC_DAC0_DP_EN = VBC_DP_EN_START,
    VBC_DAC1_DP_EN,
    VBC_ADC0_DP_EN,
    VBC_ADC1_DP_EN,
    VBC_ADC2_DP_EN,
    VBC_ADC3_DP_EN,
    VBC_ofld_DP_EN,
    VBC_fm_DP_EN,
    VBC_st_DP_EN,
    VBC_DP_EN_MAX,
} VBC_DATAPATH_ID_E;

#if 0
typedef enum {
    VBC_BAK_REG_START = 0,
    /*[0]=0 auply data to dac0 work,[0]=1 auply data only to dsp ,
        *so when fm use dac0, it should be configured with 1*/
    VBC_AUDPLY_DATA_DAC0_DSP = VBC_BAK_REG_START,
    /*[1]=1, when dac bt src enable it be configured with 1*/
    VBC_DAC_MIX_FIX_CTL,
    VBC_BAK_REG_MAX,
} VBC_BAK_REG_ID_E;
#endif

struct snd_pcm_hw_paras {
	unsigned int channels;
	unsigned int rate;
	unsigned int format;
};

struct snd_pcm_statup_paras {
	VBC_DA_ID_E dac_id;
	VBC_AD_ID_E adc_id;
	int dac_out_sel;	//iis or vbc_if
	int adc_source_sel;		//iis or vbc_if
	int dac_iis_port;	//iis0-iis3
	int adc_iis_port;	//iis0-iis3
};

struct snd_pcm_stream_info {
	char name[32];
	int id;
	SND_PCM_DIRECTION_E stream;
};

struct sprd_vbc_stream_startup_shutdown {
	struct snd_pcm_stream_info stream_info;
	struct snd_pcm_statup_paras startup_para;
};

struct sprd_vbc_stream_hw_paras {
	struct snd_pcm_stream_info stream_info;
	struct snd_pcm_hw_paras hw_params_info;
};

struct sprd_vbc_stream_trigger {
	struct snd_pcm_stream_info stream_info;
	SND_PCM_TRIGGER_CMD_E pcm_trigger_cmd;
};

struct sprd_vbc_kcontrol {
	unsigned int reg;
	unsigned int mask;
	unsigned int value;
};

struct sprd_audio_sharemem {
	int id;
	int type;
	unsigned int phy_iram_addr;
	uint32_t size;
};

struct vbc_mute_dg_para {
	VBC_MDG_ID_E mdg_id;
	bool mdg_mute;
	int16_t mdg_step;
};

struct vbc_loopback_para {
	VBC_LOOPBACK_TYPE_E loopback_para;
	int voice_fmt;
	int amr_rate;
};


struct vbc_src_para {
	int enable;
	VBC_SRC_ID_E src_id;
	VBC_SRC_MODE_E src_mode;
};

struct vbc_bt_call_para {
	int enable;
	VBC_SRC_MODE_E src_mode;
};

struct vbc_dg_para {
	VBC_DG_ID_E dg_id;
	int16_t dg_left;
	int16_t dg_right;
};

struct vbc_smthdg_para {
	VBC_SMTHDG_ID_E smthdg_id;
	int16_t smthdg_left;
	int16_t smthdg_right;
};

struct vbc_smthdg_step_para {
	VBC_SMTHDG_ID_E smthdg_id;
	int16_t smthdg_step;
};

struct vbc_mixerdg_mainpath_para {
	VBC_MIXERDG_ID_E mixerdg_id;
	int16_t mixerdg_main_left;
	int16_t mixerdg_main_right;
};

struct vbc_mixerdg_mixpath_para {
	VBC_MIXERDG_ID_E mixerdg_id;
	int16_t mixerdg_mix_left;
	int16_t mixerdg_mix_right;
};

struct vbc_mixer_para {
	VBC_MIXER_INDEX_E mixer_id;
	int16_t mixer_mux_left;
	int16_t mixer_mux_right;
	int16_t mixer_out_left;
	int16_t mixer_out_right;
};

struct vbc_mux_para {
	VBC_MUX_ID_E mux_id;
	int16_t mux_sel;
};

struct vbc_adder_para {
	VBC_ADDER_ID_E adder_id;
	VBC_ADDER_MODE_E adder_mode_l;
	VBC_ADDER_MODE_E adder_mode_r;
};

struct sprd_vbc_priv {
	int adc_used_chan_count;
	int dac_used_chan_count;
	VBC_DA_ID_E dac_id;
	VBC_AD_ID_E adc_id;
	VBC_MUX_ID_E out_sel;
	VBC_MUX_ID_E adc_source_sel;
	VBC_MUX_ID_E dac_iis_port;
	VBC_MUX_ID_E adc_iis_port;
	struct sprd_pcm_dma_params *dma_params[SND_PCM_DIRECTION_MAX];
};

struct vbc_dp_en_para {
	int id;
	unsigned short enable;
};

const char *vbc_get_adder_name(int adder_id);
const char *vbc_get_mux_name(int mux_id);
const char *vbc_get_chan_name(int chan_id);
const char *vbc_get_mixerdg_name(int mixerdg_id);
const char *vbc_get_smthdg_name(int smthdg_id);
const char *vbc_get_dg_name(int dg_id);
const char *vbc_get_mdg_name(int mdg_id);
const char *vbc_get_profile_name(int profile_id);
const char *vbc_get_stream_name(int stream);
void __safe_mem_release(void **free);
#define vbc_safe_kfree(p) __safe_mem_release((void**)p)

#endif /* __VBC_UTILS_H */
