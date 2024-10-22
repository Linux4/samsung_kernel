/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */
#ifndef _MT6681_PRIVATE_H_
#define _MT6681_PRIVATE_H_
#define MT6681_NLE_GAIN_STAGE 8
#include <linux/i2c.h>
enum {
	MT6681_MTKAIF_PROTOCOL_1 = 0,
	MT6681_MTKAIF_PROTOCOL_2,
	MT6681_MTKAIF_PROTOCOL_2_CLK_P2,
};

enum {
	MT6681_AIF_1 = 0,	/* dl: hp, rcv, hp+lo */
	MT6681_AIF_2,		/* dl: lo only */
	MT6681_AIF_3,		/* dl: lo only */
	MT6681_AIF_VOW,
	MT6681_AIF_NUM,
};

enum {
	AUDIO_ANALOG_VOLUME_HSOUTL,
	AUDIO_ANALOG_VOLUME_HSOUTR,
	AUDIO_ANALOG_VOLUME_HPOUTL,
	AUDIO_ANALOG_VOLUME_HPOUTR,
	AUDIO_ANALOG_VOLUME_LINEOUTL,
	AUDIO_ANALOG_VOLUME_LINEOUTR,
	AUDIO_ANALOG_VOLUME_MICAMP1,
	AUDIO_ANALOG_VOLUME_MICAMP2,
	AUDIO_ANALOG_VOLUME_MICAMP3,
	AUDIO_ANALOG_VOLUME_MICAMP4,
	AUDIO_ANALOG_VOLUME_MICAMP5,
	AUDIO_ANALOG_VOLUME_MICAMP6,
	AUDIO_ANALOG_NEG_VOLUME_MICAMP1,
	AUDIO_ANALOG_NEG_VOLUME_MICAMP2,
	AUDIO_ANALOG_NEG_VOLUME_MICAMP3,
	AUDIO_ANALOG_NEG_VOLUME_MICAMP4,
	AUDIO_ANALOG_NEG_VOLUME_MICAMP5,
	AUDIO_ANALOG_NEG_VOLUME_MICAMP6,
	AUDIO_ANALOG_VOLUME_TYPE_MAX
};

enum {
	MUX_MIC_TYPE_0,		/* ain0, micbias 0 */
	MUX_MIC_TYPE_1,		/* ain1, micbias 1 */
	MUX_MIC_TYPE_2,		/* ain2/3, micbias 2 */
	MUX_MIC_TYPE_3,		/* ain3/4, micbias 3 */
	MUX_PGA_L,
	MUX_PGA_R,
	MUX_PGA_3,
	MUX_PGA_4,
	MUX_PGA_5,
	MUX_PGA_6,
	MUX_HP_L,
	MUX_HP_R,
	MUX_NUM,
};

enum { DEVICE_HP, DEVICE_LO, DEVICE_RCV, DEVICE_MIC1, DEVICE_MIC2, DEVICE_NUM };

enum {
	HP_GAIN_CTL_ZCD = 0,
	HP_GAIN_CTL_NLE,
	HP_GAIN_CTL_NUM,
};

enum {
	MT6681_AFE_ETDM_NONE = -1,
	MT6681_AFE_ETDM_8000HZ = 0,
	MT6681_AFE_ETDM_11025HZ = 1,
	MT6681_AFE_ETDM_12000HZ = 2,
	MT6681_AFE_ETDM_384000HZ = 3,
	MT6681_AFE_ETDM_16000HZ = 4,
	MT6681_AFE_ETDM_22050HZ = 5,
	MT6681_AFE_ETDM_24000HZ = 6,
	MT6681_AFE_ETDM_32000HZ = 8,
	MT6681_AFE_ETDM_44100HZ = 9,
	MT6681_AFE_ETDM_48000HZ = 10,
	MT6681_AFE_ETDM_88200HZ = 11,
	MT6681_AFE_ETDM_96000HZ = 12,
	MT6681_AFE_ETDM_176400HZ = 13,
	MT6681_AFE_ETDM_192000HZ = 14,
};
enum {
	MT6681_ADDA_NONE = -1,
	MT6681_ADDA_8000HZ = 0x0,
	MT6681_ADDA_12000HZ = 0x1,
	MT6681_ADDA_16000HZ = 0x2,
	MT6681_ADDA_24000HZ = 0x3,
	MT6681_ADDA_32000HZ = 0x4,
	MT6681_ADDA_48000HZ = 0x5,
	MT6681_ADDA_64000HZ = 0x6,
	MT6681_ADDA_96000HZ = 0x7,
	MT6681_ADDA_128000HZ = 0x8,
	MT6681_ADDA_192000HZ = 0x9,
	MT6681_ADDA_25600HZ = 0xa,
	MT6681_ADDA_384000HZ = 0xb,
	MT6681_ADDA_11000HZ = 0x10,
	MT6681_ADDA_22000HZ = 0x11,
	MT6681_ADDA_44000HZ = 0x12,
	MT6681_ADDA_88000HZ = 0x13,
	MT6681_ADDA_176000HZ = 0x14,
	MT6681_ADDA_35200HZ = 0x15,
};

enum {
	MT6681_DLSRC_NONE = -1,
	MT6681_DLSRC_8000HZ = 0x0,
	MT6681_DLSRC_11025HZ = 0x1,
	MT6681_DLSRC_12000HZ = 0x2,
	MT6681_DLSRC_16000HZ = 0x3,
	MT6681_DLSRC_22050HZ = 0x4,
	MT6681_DLSRC_24000HZ = 0x5,
	MT6681_DLSRC_32000HZ = 0x6,
	MT6681_DLSRC_44100HZ = 0x7,
	MT6681_DLSRC_48000HZ = 0x8,
	MT6681_DLSRC_96000HZ = 0x9,
	MT6681_DLSRC_192000HZ = 0xa,
	MT6681_DLSRC_384000HZ = 0xb,
};

enum {
	MT6681_VOICE_NONE = -1,
	MT6681_VOICE_8000HZ = 0x0,
	MT6681_VOICE_16000HZ = 0x1,
	MT6681_VOICE_32000HZ = 0x2,
	MT6681_VOICE_48000HZ = 0x3,
	MT6681_VOICE_96000HZ = 0x4,
	MT6681_VOICE_192000HZ = 0x5,
};

/* Supply widget subseq */
enum {
	/* common */
	SUPPLY_SEQ_SCP_REQ,
	SUPPLY_SEQ_CLK_BUF,
	SUPPLY_SEQ_CKTST,
	SUPPLY_SEQ_LDO_VAUD18,
	SUPPLY_SEQ_ADC_INIT,
	SUPPLY_SEQ_AUD_GLB,
	SUPPLY_SEQ_AUD_GLB_VOW,
	SUPPLY_SEQ_PLL_208M,
	SUPPLY_SEQ_DL_GPIO,
	SUPPLY_SEQ_UL_GPIO,
	SUPPLY_SEQ_HP_PULL_DOWN,
	SUPPLY_SEQ_CLKSQ,
	SUPPLY_SEQ_ADC_CLKGEN,
	SUPPLY_SEQ_TOP_CK,
	SUPPLY_SEQ_TOP_SRAM,
	SUPPLY_SEQ_TOP_CK_LAST,
	SUPPLY_SEQ_MIC_BIAS,
	SUPPLY_SEQ_DCC_CLK,
	SUPPLY_SEQ_DMIC,
	SUPPLY_SEQ_AUD_TOP,
	SUPPLY_SEQ_AUD_TOP_LAST,
	SUPPLY_SEQ_DL_SDM_FIFO_CLK,
	SUPPLY_SEQ_DL_SDM,
	SUPPLY_SEQ_DL_NLE,
	SUPPLY_SEQ_DL_NCP,
	SUPPLY_SEQ_DL_CLH,
	SUPPLY_SEQ_DL_HWGAIN,
	SUPPLY_SEQ_AFE,
	/* playback */
	SUPPLY_SEQ_DL_SRC,
	SUPPLY_SEQ_DL_ZCD,
	SUPPLY_SEQ_DL_ESD_RESIST,
	SUPPLY_SEQ_HP_DAMPING_OFF_RESET_CMFB,
	SUPPLY_SEQ_HP_MUTE,
	SUPPLY_SEQ_DL_LDO_REMOTE_SENSE,
	SUPPLY_SEQ_DL_LDO,
	SUPPLY_SEQ_DL_CLH_POST,
	SUPPLY_SEQ_DL_NVREG,
	SUPPLY_SEQ_DL_NV,
	SUPPLY_SEQ_HP_ANA_TRIM,
	SUPPLY_SEQ_DL_IBIST,
	/* capture */
	SUPPLY_SEQ_UL_PGA,
	SUPPLY_SEQ_UL_ADC,
	SUPPLY_SEQ_UL_MTKAIF,
	SUPPLY_SEQ_UL_SRC_DMIC,
	SUPPLY_SEQ_UL_SRC,
#if IS_ENABLED(CONFIG_MTK_VOW_SUPPORT)
	SUPPLY_SEQ_VOW_DIG_CFG,
	SUPPLY_SEQ_VOW_CIC_CFG,
#endif
};

enum {
	CH_L = 0,
	CH_R,
	NUM_CH,
};

enum {
	DRBIAS_4UA = 0,
	DRBIAS_5UA,
	DRBIAS_6UA,
	DRBIAS_7UA,
	DRBIAS_8UA,
	DRBIAS_9UA,
	DRBIAS_10UA,
	DRBIAS_11UA,
};

enum {
	IBIAS_4UA = 0,
	IBIAS_5UA,
	IBIAS_6UA,
	IBIAS_7UA,
};

enum {
	IBIAS_ZCD_3UA = 0,
	IBIAS_ZCD_4UA,
	IBIAS_ZCD_5UA,
	IBIAS_ZCD_6UA,
};

enum {
	MIC_BIAS_1P7 = 0,
	MIC_BIAS_1P8,
	MIC_BIAS_1P9,
	MIC_BIAS_2P0,
	MIC_BIAS_2P1,
	MIC_BIAS_2P5,
	MIC_BIAS_2P6,
	MIC_BIAS_2P7,
};

enum {
	DL_GAIN_8DB = 0,
	DL_GAIN_0DB = 8,
	DL_GAIN_N_1DB = 9,
	DL_GAIN_N_10DB = 18,
	DL_GAIN_N_22DB = 30,
	DL_GAIN_N_40DB = 0x1f,
};

enum {
	HP_GAIN_9DB = 0,
	HP_GAIN_6DB = 1,
	HP_GAIN_3DB = 2,
	HP_GAIN_0DB = 3,
};

enum {
	MIC_TYPE_MUX_IDLE = 0,
	MIC_TYPE_MUX_ACC,
	MIC_TYPE_MUX_DMIC,
	MIC_TYPE_MUX_DCC,
	MIC_TYPE_MUX_DCC_ECM_DIFF,
	MIC_TYPE_MUX_DCC_ECM_SINGLE,
};

enum {
	MIC_INDEX_IDLE = 0,
	MIC_INDEX_MAIN,
	MIC_INDEX_REF,
	MIC_INDEX_THIRD,
	MIC_INDEX_HEADSET,
};

enum {
	LO_MUX_OPEN = 0,
	LO_MUX_L_DAC,
	LO_MUX_3RD_DAC,
	LO_MUX_TEST_MODE,
	LO_MUX_MASK = 0x3,
};

enum {
	HP_MUX_OPEN = 0,
	HP_MUX_HPSPK,
	HP_MUX_HP,
	HP_MUX_TEST_MODE,
	HP_MUX_HP_IMPEDANCE,
	HP_MUX_MASK = 0x7,
};

/* Test mode */
enum {
	HP_MUX_TEST = 0,
	HP_MUX_HS,
	HP_MUX_LOL = 0x2,
};

enum {
	RCV_MUX_OPEN = 0,
	RCV_MUX_MUTE,
	RCV_MUX_VOICE_PLAYBACK,
	RCV_MUX_TEST_MODE,
	RCV_MUX_MASK = 0x3,
};

enum {
	PGA_MUX_AIN0 = 0,
	PGA_MUX_AIN1,
	PGA_MUX_AIN2,
	PGA_MUX_NONE,
};

enum {
	PGA_3_MUX_AIN0,
	PGA_3_MUX_AIN2,
	PGA_3_MUX_AIN3,
	PGA_3_MUX_AIN5,
};

enum {
	PGA_4_MUX_AIN2,
	PGA_4_MUX_AIN3,
	PGA_4_MUX_AIN4,
	PGA_4_MUX_AIN6,
};

enum {
	PGA_5_MUX_AIN0,
	PGA_5_MUX_AIN2,
	PGA_5_MUX_AIN5,
	PGA_5_MUX_AIN6,
};

enum {
	PGA_6_MUX_AIN1,
	PGA_6_MUX_AIN2,
	PGA_6_MUX_AIN5,
	PGA_6_MUX_AIN6,
};

enum {
	UL_SRC_MUX_AMIC = 0,
	UL_SRC_MUX_DMIC,
};

enum {
	MISO_MUX_UL1_CH1 = 0,
	MISO_MUX_UL1_CH2,
	MISO_MUX_UL2_CH1,
	MISO_MUX_UL2_CH2,
	MISO_MUX_UL3_CH1,
	MISO_MUX_UL3_CH2,
};

enum {
	VOW_AMIC_MUX_ADC_DATA_0 = 0,
	VOW_AMIC_MUX_ADC_DATA_1,
	VOW_AMIC_MUX_ADC_DATA_2,
	VOW_AMIC_MUX_ADC_DATA_3,
	VOW_AMIC_MUX_ADC_DATA_4,
	VOW_AMIC_MUX_ADC_DATA_5
};

enum {
	VOW_DMIC_MUX_ADC_DATA_0 = 0,
	VOW_DMIC_MUX_ADC_DATA_1,
	VOW_DMIC_MUX_ADC_DATA_2,
	VOW_DMIC_MUX_ADC_DATA_3,
	VOW_DMIC_MUX_ADC_DATA_4,
	VOW_DMIC_MUX_ADC_DATA_5
};

enum {
	DMIC_MUX_DMIC_DATA0 = 0,
	DMIC_MUX_DMIC_DATA1,
	DMIC_MUX_DMIC_DATA2,
	DMIC_MUX_DMIC_DATA3,
};

enum {
	VOW_PBUF_MUX_CH_0 = 0,
	VOW_PBUF_MUX_CH_1,
	VOW_PBUF_MUX_CH_2,
	VOW_PBUF_MUX_CH_3
};

enum {
	ADC_MUX_IDLE = 0,
	ADC_MUX_AIN0,
	ADC_MUX_PREAMPLIFIER,
	ADC_MUX_IDLE1,
};

enum {
	TRIM_BUF_MUX_OPEN = 0,
	TRIM_BUF_MUX_HPL,
	TRIM_BUF_MUX_HPR,
	TRIM_BUF_MUX_HSP,
	TRIM_BUF_MUX_HSN,
	TRIM_BUF_MUX_LOLP,
	TRIM_BUF_MUX_LOLN,
	TRIM_BUF_MUX_AU_REFN,
	TRIM_BUF_MUX_AVSS32,
	TRIM_BUF_MUX_UNUSED,
};

enum {
	TRIM_BUF_GAIN_0DB = 0,
	TRIM_BUF_GAIN_6DB,
	TRIM_BUF_GAIN_12DB,
	TRIM_BUF_GAIN_18DB,
};

enum {
	TRIM_STEP0 = 0,
	TRIM_STEP1,
	TRIM_STEP2,
	TRIM_STEP3,
	TRIM_STEP_NUM,
};

enum {
	AUXADC_AVG_1 = 0,
	AUXADC_AVG_4,
	AUXADC_AVG_8,
	AUXADC_AVG_16,
	AUXADC_AVG_32,
	AUXADC_AVG_64,
	AUXADC_AVG_128,
	AUXADC_AVG_256,
};

enum {
	MT6681_DL_GAIN_MUTE = 0,
	MT6681_DL_GAIN_NORMAL = 0xf74f,
	/* SA suggest apply -0.3db to audio/speech path */
};

enum {
	VOW_SCP_FIFO = 0,
	VOW_STANDALONE_CODEC = 1
};

enum {
	VOW_DIS_HDR_CONCURRENT = 0,
	VOW_EN_HDR_CONCURRENT
};

enum {
	VOW_LEGACY_CIC = 0,
	VOW_NEW_CIC
};

struct dc_trim_data {
	bool calibrated;
	int mic_vinp_mv;
};

struct hp_trim_data {
	unsigned int hp_trim_l;
	unsigned int hp_trim_r;
	unsigned int hp_fine_trim_l;
	unsigned int hp_fine_trim_r;
};

struct nle_trim_data {
	int L_LN[MT6681_NLE_GAIN_STAGE];
	int R_LN[MT6681_NLE_GAIN_STAGE];
	int L_GS[MT6681_NLE_GAIN_STAGE];
	int R_GS[MT6681_NLE_GAIN_STAGE];
};

struct mt6681_vow_periodic_on_off_data {
	unsigned long long pga_on;
	unsigned long long precg_on;
	unsigned long long adc_on;
	unsigned long long micbias0_on;
	unsigned long long micbias1_on;
	unsigned long long dcxo_on;
	unsigned long long audglb_on;
	unsigned long long vow_on;
	unsigned long long pga_off;
	unsigned long long precg_off;
	unsigned long long adc_off;
	unsigned long long micbias0_off;
	unsigned long long micbias1_off;
	unsigned long long dcxo_off;
	unsigned long long audglb_off;
	unsigned long long vow_off;
};

struct mt6681_codec_ops {
	int (*enable_dc_compensation)(bool enable);
	int (*set_lch_dc_compensation)(int value);
	int (*set_rch_dc_compensation)(int value);
	int (*adda_dl_gain_control)(bool mute);
};

struct mt6681_priv {
	struct device *dev;
	struct regmap *regmap;
	struct nvmem_device *efuse;
	struct i2c_client *i2c_client;
	unsigned int dl_rate[MT6681_AIF_NUM];
	unsigned int ul_rate[MT6681_AIF_NUM];
	int ana_gain[AUDIO_ANALOG_VOLUME_TYPE_MAX];
	unsigned int mux_select[MUX_NUM];
	int dev_counter[DEVICE_NUM];
	int hp_gain_ctl;
	int hp_hifi_mode;
	int hp_plugged;
	int mtkaif_protocol;
	int dmic_one_wire_mode;
	int mic_hifi_mode;
	int mic_ulcf_en;
	unsigned int vd105;
	unsigned int audio_r_miso1_enable;
	unsigned int miso_only;
	unsigned int hwgain_enable;
	unsigned int dl_hwgain;

	/* hw version */
	int hw_ver;
	unsigned long long hw_ecid;

	/* dc trim */
	struct dc_trim_data dc_trim;
	struct hp_trim_data hp_trim_3_pole;
	struct hp_trim_data hp_trim_4_pole;
	struct iio_channel *hpofs_cal_auxadc;

	/* headphone impedence */
	struct nvmem_device *hp_efuse;
	int hp_impedance;
	int hp_current_calibrate_val;
	struct mt6681_codec_ops ops;

	/* NLE */
	struct nle_trim_data nle_trim;

	/* debugfs */
	struct dentry *debugfs;

	/* vow control */
	int vow_enable;
	int vow_setup;
	int reg_afe_vow_vad_cfg0;
	int reg_afe_vow_vad_cfg1;
	int reg_afe_vow_vad_cfg2;
	int reg_afe_vow_vad_cfg3;
	int reg_afe_vow_vad_cfg4;
	int reg_afe_vow_vad_cfg5;
	int reg_afe_vow_periodic;
	unsigned int vow_channel;
	unsigned int vow_pbuf_active_bit;
	unsigned int vow_hdr_concurrent;
	unsigned int vow_cic_type;
	struct mt6681_vow_periodic_on_off_data vow_periodic_param;
	/* vow dmic low power mode, 1: enable, 0: disable */
	int vow_dmic_lp;
	int vow_single_mic_select;
	int bypass_hpdet_dump;
	int hdr_record;

	/* regulator */
	struct regulator *reg_vaud18;
};

#define MT_SOC_ENUM_EXT_ID(xname, xenum, xhandler_get, xhandler_put, id)       \
	{                                                                      \
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname,            \
		.device = id, .info = snd_soc_info_enum_double,                \
		.get = xhandler_get, .put = xhandler_put,                      \
		.private_value = (unsigned long)&xenum                         \
	}

/* dl bias */
#define IBIAS_MASK 0x3
#define IBIAS_HP_SFT (RG_AUDBIASADJ_1_VAUDP18_SFT + 0)
#define IBIAS_HP_MASK_SFT (IBIAS_MASK << IBIAS_HP_SFT)
#define IBIAS_HS_SFT (RG_AUDBIASADJ_1_VAUDP18_SFT + 2)
#define IBIAS_HS_MASK_SFT (IBIAS_MASK << IBIAS_HS_SFT)
#define IBIAS_LO_SFT (RG_AUDBIASADJ_1_VAUDP18_SFT + 4)
#define IBIAS_LO_MASK_SFT (IBIAS_MASK << IBIAS_LO_SFT)
#define IBIAS_ZCD_SFT (RG_AUDBIASADJ_1_VAUDP18_SFT + 6)
#define IBIAS_ZCD_MASK_SFT (IBIAS_MASK << IBIAS_ZCD_SFT)

/* dl pga gain */
#define DL_GAIN_N_10DB_REG (DL_GAIN_N_10DB)
#define DL_GAIN_N_22DB_REG (DL_GAIN_N_22DB)
#define DL_GAIN_N_40DB_REG (DL_GAIN_N_40DB)
#define DL_GAIN_REG_MASK 0x0f9f

/* mic type */

#define IS_DCC_BASE(x)                                                         \
	(x == MIC_TYPE_MUX_DCC || x == MIC_TYPE_MUX_DCC_ECM_DIFF               \
	 || x == MIC_TYPE_MUX_DCC_ECM_SINGLE)

#define IS_AMIC_BASE(x) (x == MIC_TYPE_MUX_ACC || IS_DCC_BASE(x))

/* reg idx for -40dB */
#define PGA_MINUS_40_DB_REG_VAL 0x1f
#define HP_PGA_MINUS_40_DB_REG_VAL 0x3f

/* dc trim */
#define TRIM_TIMES 26
#define TRIM_DISCARD_NUM 3
#define TRIM_USEFUL_NUM (TRIM_TIMES - (TRIM_DISCARD_NUM * 2))

/* headphone impedance detection */
#define PARALLEL_OHM 0

/* codec name */
#define CODEC_MT6681_NAME "mtk-codec-mt6681"
#define DEVICE_MT6681_NAME "mt6681-sound"

int mt6681_set_codec_ops(struct snd_soc_component *cmpnt, struct mt6681_codec_ops *ops);
int mt6681_set_mtkaif_protocol(struct snd_soc_component *cmpnt, int mtkaif_protocol);
void mt6681_mtkaif_calibration_enable(struct snd_soc_component *cmpnt);
void mt6681_mtkaif_calibration_disable(struct snd_soc_component *cmpnt);
void mt6681_mtkaif_loopback(struct snd_soc_component *cmpnt, bool enable);
void mt6681_set_mtkaif_calibration_phase(struct snd_soc_component *cmpnt,
					 int phase_1, int phase_2, int phase_3);
extern int scp_wake_request(struct i2c_adapter *);
extern int scp_wake_release(struct i2c_adapter *);

extern bool mt6681_probe_done;

#endif
