/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mt6989-afe-common.h  --  Mediatek 6989 audio driver definitions
 *
 * Copyright (c) 2023 MediaTek Inc.
 *  Author: Tina Tsai <tina.tsai@mediatek.com>
 */

#ifndef _MT_6989_AFE_COMMON_H_
#define _MT_6989_AFE_COMMON_H_
#include <sound/soc.h>
#include <linux/list.h>
#include <linux/regmap.h>
#include <mt-plat/aee.h>
#include "mt6989-reg.h"
#include "../common/mtk-base-afe.h"
#include <linux/regulator/consumer.h>

// #define SKIP_SB
#ifdef SKIP_SB
/* define SKIP_SB to skip all feature */
#define SKIP_SB_DSP
#define SKIP_SB_BTCVSD
#define SKIP_SB_OFFLOAD
#define SKIP_SB_VOW
#define SKIP_SB_ULTRA
#define SKIP_SB_AUDIO
#define SKIP_SMCC_SB
#else
/* delete define below if your feature don't want to skip */
#define SKIP_SB_BTCVSD
#endif

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define AUDIO_AEE(message) \
	(aee_kernel_exception_api(__FILE__, \
				  __LINE__, \
				  DB_OPT_FTRACE, message, \
				  "audio assert"))
#else
#define AUDIO_AEE(message) WARN_ON(true)
#endif

enum {
	MT6989_MEMIF_DL0,
	MT6989_MEMIF_DL1,
	MT6989_MEMIF_DL2,
	MT6989_MEMIF_DL3,
	MT6989_MEMIF_DL4,
	MT6989_MEMIF_DL5,
	MT6989_MEMIF_DL6,
	MT6989_MEMIF_DL7,
	MT6989_MEMIF_DL8,
	MT6989_MEMIF_DL23,
	MT6989_MEMIF_DL24,
	MT6989_MEMIF_DL25,
	MT6989_MEMIF_DL_24CH,
	MT6989_MEMIF_VUL0,
	MT6989_MEMIF_VUL1,
	MT6989_MEMIF_VUL2,
	MT6989_MEMIF_VUL3,
	MT6989_MEMIF_VUL4,
	MT6989_MEMIF_VUL5,
	MT6989_MEMIF_VUL6,
	MT6989_MEMIF_VUL7,
	MT6989_MEMIF_VUL8,
	MT6989_MEMIF_VUL9,
	MT6989_MEMIF_VUL10,
	MT6989_MEMIF_VUL24,
	MT6989_MEMIF_VUL25,
	MT6989_MEMIF_VUL_CM0,
	MT6989_MEMIF_VUL_CM1,
	MT6989_MEMIF_ETDM_IN0,
	MT6989_MEMIF_ETDM_IN1,
	MT6989_MEMIF_ETDM_IN2,
	MT6989_MEMIF_ETDM_IN4,
	MT6989_MEMIF_ETDM_IN6,
	MT6989_MEMIF_HDMI,
	MT6989_MEMIF_NUM,
	MT6989_DAI_ADDA = MT6989_MEMIF_NUM,
	MT6989_DAI_ADDA_CH34,
	MT6989_DAI_ADDA_CH56,
	MT6989_DAI_AP_DMIC,
	MT6989_DAI_AP_DMIC_CH34,
	MT6989_DAI_AP_DMIC_CH56,
	MT6989_DAI_VOW,
	MT6989_DAI_VOW_SCP_DMIC,
	MT6989_DAI_CONNSYS_I2S,
	MT6989_DAI_I2S_IN0,
	MT6989_DAI_I2S_IN1,
	MT6989_DAI_I2S_IN2,
	MT6989_DAI_I2S_IN4,
	MT6989_DAI_I2S_IN6,
	MT6989_DAI_I2S_OUT0,
	MT6989_DAI_I2S_OUT1,
	MT6989_DAI_I2S_OUT2,
	MT6989_DAI_I2S_OUT4,
	MT6989_DAI_I2S_OUT6,
	MT6989_DAI_FM_I2S_MASTER,
	MT6989_DAI_HW_GAIN_0,
	MT6989_DAI_HW_GAIN_1,
	MT6989_DAI_HW_GAIN_2,
	MT6989_DAI_HW_GAIN_3,
	MT6989_DAI_SRC_0,
	MT6989_DAI_SRC_1,
	MT6989_DAI_SRC_2,
	MT6989_DAI_SRC_3,
	MT6989_DAI_PCM_0,
	MT6989_DAI_PCM_1,
	MT6989_DAI_TDM,
	MT6989_DAI_TDM_DPTX,
	MT6989_DAI_HOSTLESS_LPBK,
	MT6989_DAI_HOSTLESS_FM,
	MT6989_DAI_HOSTLESS_HW_GAIN_AAUDIO,
	MT6989_DAI_HOSTLESS_SRC_AAUDIO,
	MT6989_DAI_HOSTLESS_SPEECH,
	MT6989_DAI_HOSTLESS_BT,
	MT6989_DAI_HOSTLESS_SPH_ECHO_REF,
	MT6989_DAI_HOSTLESS_SPK_INIT,
	MT6989_DAI_HOSTLESS_IMPEDANCE,
	MT6989_DAI_HOSTLESS_SRC_0,
	MT6989_DAI_HOSTLESS_SRC_1,
	MT6989_DAI_HOSTLESS_SRC_2,
	MT6989_DAI_HOSTLESS_HW_SRC_0_OUT,
	MT6989_DAI_HOSTLESS_HW_SRC_0_IN,
	MT6989_DAI_HOSTLESS_HW_SRC_1_OUT,
	MT6989_DAI_HOSTLESS_HW_SRC_1_IN,
	MT6989_DAI_HOSTLESS_HW_SRC_2_OUT,
	MT6989_DAI_HOSTLESS_HW_SRC_2_IN,
	MT6989_DAI_HOSTLESS_SRC_BARGEIN,
	MT6989_DAI_HOSTLESS_UL1,
	MT6989_DAI_HOSTLESS_UL2,
	MT6989_DAI_HOSTLESS_UL3,
	MT6989_DAI_HOSTLESS_UL4,
	MT6989_DAI_HOSTLESS_DSP_DL,
	MT6989_DAI_NUM,
	/* use for mtkaif calibration specail setting */
	MT6989_DAI_MTKAIF = MT6989_DAI_NUM,
	MT6989_DAI_MISO_ONLY,
};

#define MT6989_DAI_I2S_MAX_NUM 11 //depends each platform's max i2s num
#define MT6989_RECORD_MEMIF MT6989_MEMIF_VUL9
#define MT6989_ECHO_REF_MEMIF MT6989_MEMIF_VUL8
#define MT6989_PRIMARY_MEMIF MT6989_MEMIF_DL0
#define MT6989_FAST_MEMIF MT6989_MEMIF_DL2
#define MT6989_DEEP_MEMIF MT6989_MEMIF_DL3
#define MT6989_VOIP_MEMIF MT6989_MEMIF_DL1
#define MT6989_MMAP_DL_MEMIF MT6989_MEMIF_DL4
#define MT6989_MMAP_UL_MEMIF MT6989_MEMIF_VUL2
#define MT6989_BARGE_IN_MEMIF MT6989_MEMIF_VUL_CM0

// adsp define
#define MT6989_DSP_PRIMARY_MEMIF MT6989_MEMIF_DL0
#define MT6989_DSP_DEEPBUFFER_MEMIF MT6989_MEMIF_DL3
#define MT6989_DSP_VOIP_MEMIF MT6989_MEMIF_DL1
#define MT6989_DSP_PLAYBACKDL_MEMIF MT6989_MEMIF_DL4
#define MT6989_DSP_PLAYBACKUL_MEMIF MT6989_MEMIF_VUL4

/* update irq ID (= enum) from AFE_IRQ_MCU_STATUS */
enum {
	MT6989_IRQ_0,
	MT6989_IRQ_1,
	MT6989_IRQ_2,
	MT6989_IRQ_3,
	MT6989_IRQ_4,
	MT6989_IRQ_5,
	MT6989_IRQ_6,
	MT6989_IRQ_7,
	MT6989_IRQ_8,
	MT6989_IRQ_9,
	MT6989_IRQ_10,
	MT6989_IRQ_11,
	MT6989_IRQ_12,
	MT6989_IRQ_13,
	MT6989_IRQ_14,
	MT6989_IRQ_15,
	MT6989_IRQ_16,
	MT6989_IRQ_17,
	MT6989_IRQ_18,
	MT6989_IRQ_19,
	MT6989_IRQ_20,
	MT6989_IRQ_21,
	MT6989_IRQ_22,
	MT6989_IRQ_23,
	MT6989_IRQ_24,
	MT6989_IRQ_25,
	MT6989_IRQ_26,
	MT6989_IRQ_31,  /* used only for TDM */
	MT6989_IRQ_NUM,
};
/* update irq ID (= enum) from AFE_IRQ_MCU_STATUS */
enum {
	MT6989_CUS_IRQ_TDM = 0,  /* used only for TDM */
	MT6989_CUS_IRQ_NUM,
};

/* MCLK */
enum {
	MT6989_I2SIN0_MCK = 0,
	MT6989_I2SIN1_MCK,
	MT6989_FMI2S_MCK,
	MT6989_TDMOUT_MCK,
	MT6989_TDMOUT_BCK,
	MT6989_MCK_NUM,
};

/* hw gain */
static const uint32_t kHWGainMap[] = {
	0x00000, //   0, -64.0 dB (mute)
	0x0015E, //   1, -63.5 dB
	0x00173, //   2, -63.0 dB
	0x00189, //   3, -62.5 dB
	0x001A0, //   4, -62.0 dB
	0x001B9, //   5, -61.5 dB
	0x001D3, //   6, -61.0 dB
	0x001EE, //   7, -60.5 dB
	0x0020C, //   8, -60.0 dB
	0x0022B, //   9, -59.5 dB
	0x0024C, //  10, -59.0 dB
	0x0026F, //  11, -58.5 dB
	0x00294, //  12, -58.0 dB
	0x002BB, //  13, -57.5 dB
	0x002E4, //  14, -57.0 dB
	0x00310, //  15, -56.5 dB
	0x0033E, //  16, -56.0 dB
	0x00370, //  17, -55.5 dB
	0x003A4, //  18, -55.0 dB
	0x003DB, //  19, -54.5 dB
	0x00416, //  20, -54.0 dB
	0x00454, //  21, -53.5 dB
	0x00495, //  22, -53.0 dB
	0x004DB, //  23, -52.5 dB
	0x00524, //  24, -52.0 dB
	0x00572, //  25, -51.5 dB
	0x005C5, //  26, -51.0 dB
	0x0061D, //  27, -50.5 dB
	0x00679, //  28, -50.0 dB
	0x006DC, //  29, -49.5 dB
	0x00744, //  30, -49.0 dB
	0x007B2, //  31, -48.5 dB
	0x00827, //  32, -48.0 dB
	0x008A2, //  33, -47.5 dB
	0x00925, //  34, -47.0 dB
	0x009B0, //  35, -46.5 dB
	0x00A43, //  36, -46.0 dB
	0x00ADF, //  37, -45.5 dB
	0x00B84, //  38, -45.0 dB
	0x00C32, //  39, -44.5 dB
	0x00CEC, //  40, -44.0 dB
	0x00DB0, //  41, -43.5 dB
	0x00E7F, //  42, -43.0 dB
	0x00F5B, //  43, -42.5 dB
	0x01044, //  44, -42.0 dB
	0x0113B, //  45, -41.5 dB
	0x01240, //  46, -41.0 dB
	0x01355, //  47, -40.5 dB
	0x0147A, //  48, -40.0 dB
	0x015B1, //  49, -39.5 dB
	0x016FA, //  50, -39.0 dB
	0x01857, //  51, -38.5 dB
	0x019C8, //  52, -38.0 dB
	0x01B4F, //  53, -37.5 dB
	0x01CED, //  54, -37.0 dB
	0x01EA4, //  55, -36.5 dB
	0x02075, //  56, -36.0 dB
	0x02261, //  57, -35.5 dB
	0x0246B, //  58, -35.0 dB
	0x02693, //  59, -34.5 dB
	0x028DC, //  60, -34.0 dB
	0x02B48, //  61, -33.5 dB
	0x02DD9, //  62, -33.0 dB
	0x03090, //  63, -32.5 dB
	0x03371, //  64, -32.0 dB
	0x0367D, //  65, -31.5 dB
	0x039B8, //  66, -31.0 dB
	0x03D24, //  67, -30.5 dB
	0x040C3, //  68, -30.0 dB
	0x04499, //  69, -29.5 dB
	0x048AA, //  70, -29.0 dB
	0x04CF8, //  71, -28.5 dB
	0x05188, //  72, -28.0 dB
	0x0565D, //  73, -27.5 dB
	0x05B7B, //  74, -27.0 dB
	0x060E6, //  75, -26.5 dB
	0x066A4, //  76, -26.0 dB
	0x06CB9, //  77, -25.5 dB
	0x0732A, //  78, -25.0 dB
	0x079FD, //  79, -24.5 dB
	0x08138, //  80, -24.0 dB
	0x088E0, //  81, -23.5 dB
	0x090FC, //  82, -23.0 dB
	0x09994, //  83, -22.5 dB
	0x0A2AD, //  84, -22.0 dB
	0x0AC51, //  85, -21.5 dB
	0x0B687, //  86, -21.0 dB
	0x0C157, //  87, -20.5 dB
	0x0CCCC, //  88, -20.0 dB
	0x0D8EF, //  89, -19.5 dB
	0x0E5CA, //  90, -19.0 dB
	0x0F367, //  91, -18.5 dB
	0x101D3, //  92, -18.0 dB
	0x1111A, //  93, -17.5 dB
	0x12149, //  94, -17.0 dB
	0x1326D, //  95, -16.5 dB
	0x14496, //  96, -16.0 dB
	0x157D1, //  97, -15.5 dB
	0x16C31, //  98, -15.0 dB
	0x181C5, //  99, -14.5 dB
	0x198A1, // 100, -14.0 dB
	0x1B0D7, // 101, -13.5 dB
	0x1CA7D, // 102, -13.0 dB
	0x1E5A8, // 103, -12.5 dB
	0x2026F, // 104, -12.0 dB
	0x220EA, // 105, -11.5 dB
	0x24134, // 106, -11.0 dB
	0x26368, // 107, -10.5 dB
	0x287A2, // 108, -10.0 dB
	0x2AE02, // 109,  -9.5 dB
	0x2D6A8, // 110,  -9.0 dB
	0x301B7, // 111,  -8.5 dB
	0x32F52, // 112,  -8.0 dB
	0x35FA2, // 113,  -7.5 dB
	0x392CE, // 114,  -7.0 dB
	0x3C903, // 115,  -6.5 dB
	0x4026E, // 116,  -6.0 dB
	0x43F40, // 117,  -5.5 dB
	0x47FAC, // 118,  -5.0 dB
	0x4C3EA, // 119,  -4.5 dB
	0x50C33, // 120,  -4.0 dB
	0x558C4, // 121,  -3.5 dB
	0x5A9DF, // 122,  -3.0 dB
	0x5FFC8, // 123,  -2.5 dB
	0x65AC8, // 124,  -2.0 dB
	0x6BB2D, // 125,  -1.5 dB
	0x72148, // 126,  -1.0 dB
	0x78D6F, // 127,  -0.5 dB
	0x80000, // 128,   0.0 dB
	0x8795A, // 0.5
	0x8F9E4, // 1
	0x9820D, // 1.5
	0xA1248, // 2
	0xAAB0D, // 2.5
	0xB4CE4, // 3
	0xBF84A, // 3.5
	0xCADDC, // 4
	0xD6E30, // 4.5
	0xE39EA, // 5
	0xF11B6, // 5.5
	0xFF64A, // 6
	0x10E86C, // 6.5
	0x11E8E2, // 7
	0x12F892, // 7.5
	0x141856, // 8
	0x15492A, // 8.5
	0x168C08, // 9
	0x17E210, // 9.5
	0x194C54, // 10
	0x1ACC17, // 10.5
	0x1C6290, // 11
	0x1E1126, // 11.5
	0x1FD93E, // 12
	0x21BC58, // 12.5
	0x23BC16, // 13
	0x25DA23, // 13.5
	0x28184C, // 14
	0x2A7883, // 14.5
	0x2CFCC2, // 15
	0x2FA729, // 15.5
	0x3279FE, // 16
	0x3577AE, // 16.5
	0x38A2B6, // 17
	0x3BFDD5, // 17.5
	0x3F8BDA, // 18
	0x434FC5, // 18.5
	0x474CD0, // 19
	0x4B865D, // 19.5
	0x500000, // 20
	0x54BD84, // 20.5
	0x59C2F2, // 21
	0x5F1486, // 21.5
	0x64B6C6, // 22
	0x6AAE84, // 22.5
	0x7100C0, // 23
	0x77B2E7, // 23.5
	0x7ECA98, // 24
	0x864DE8, // 24.5
	0x8E432E, // 25
};

/* IPM hw remap */
static const uint32_t kHWGainMap_IPM2P[] = {
	0x0, // 0x52C(-64dB) change to mute
	0x57A,
	0x5CD,
	0x625,
	0x682,
	0x6E5,
	0x74E,
	0x7BC,
	0x832,
	0x8AE,
	0x932,
	0x9BD,
	0xA51,
	0xAED,
	0xB93,
	0xC42,
	0xCFC,
	0xDC1,
	0xE92,
	0xF6F,
	0x1059,
	0x1151,
	0x1257,
	0x136E,
	0x1494,
	0x15CC,
	0x1717,
	0x1875,
	0x19E8,
	0x1B71,
	0x1D11,
	0x1ECA,
	0x209D,
	0x228C,
	0x2498,
	0x26C3,
	0x290F,
	0x2B7E,
	0x2E12,
	0x30CC,
	0x33B1,
	0x36C1,
	0x39FF,
	0x3D6F,
	0x4113,
	0x44EE,
	0x4903,
	0x4D57,
	0x51EC,
	0x56C7,
	0x5BEB,
	0x615D,
	0x6722,
	0x6D3E,
	0x73B8,
	0x7A93,
	0x81D6,
	0x8988,
	0x91AE,
	0x9A4F,
	0xA374,
	0xAD24,
	0xB766,
	0xC244,
	0xCDC7,
	0xD9F8,
	0xE6E2,
	0xF491,
	0x1030E,
	0x11268,
	0x122AA,
	0x133E3,
	0x14622,
	0x15975,
	0x16DED,
	0x1839C,
	0x19A93,
	0x1B2E7,
	0x1CCAC,
	0x1E7F8,
	0x204e2,
	0x22382,
	0x243F3,
	0x26651,
	0x28AB7,
	0x2B146,
	0x2DA1D,
	0x30560,
	0x33334,
	0x363BE,
	0x39729,
	0x3CD9F,
	0x40750,
	0x4446C,
	0x48527,
	0x4C9B8,
	0x51259,
	0x55F47,
	0x5B0C5,
	0x60716,
	0x66285,
	0x6C35F,
	0x729F6,
	0x796A2,
	0x809BD,
	0x883AB,
	0x904D2,
	0x98DA1,
	0xA1E8A,
	0xAB80A,
	0xB5AA2,
	0xC06DD,
	0xCBD4C,
	0xD7E8A,
	0xE4B3C,
	0xF240F,
	0x1009BA,
	0x10FD02,
	0x110000,
	0x130FAB,
	0x1430CE,
	0x156313,
	0x16A77E,
	0x17FF23,
	0x196B24,
	0x1AECB6,
	0x1C8521,
	0x1E35C0,
	0x200000, // 0.0 dB
	0x21E568, // 0.5 dB
	0x23E793, // 1.0 dB
	0x260835, // 1.5 dB
	0x28491D, // 2.0 dB
	0x2AAC35, // 2.5 dB
	0x2D3381, // 3.0 dB
	0x2FE129, // 3.5 dB
	0x32B771, // 4.0 dB
	0x35B8C3, // 4.5 dB
	0x38E7AA, // 5.0 dB
	0x3C46DA, // 5.5 dB
	0x3FD930, // 6.0 dB
	0x43A1B3, // 6.5 dB
	0x47A39A, // 7.0 dB
	0x4BE24B, // 7.5 dB
	0x50615F, // 8.0 dB
	0x5524A8, // 8.5 dB
	0x5A3031, // 9.0 dB
	0x5F8841, // 9.5 dB
	0x653160, // 10.0 dB
	0x6B305E, // 10.5 dB
	0x718A50, // 11.0 dB
	0x784499, // 11.5 dB
	0x7F64F0, // 12.0 dB
	0x86F160, // 12.5 dB
	0x8EF051, // 13.0 dB
	0x97688D, // 13.5 dB
	0xA06142, // 14.0 dB
	0xA9E20D, // 14.5 dB
	0xB3F300, // 15.0 dB
	0xBE9CA4, // 15.5 dB
	0xC9E806, // 16.0 dB
	0xD5DEBB, // 16.5 dB
	0xE28AEB, // 17.0 dB
	0xEFF755, // 17.5 dB
	0xFE2F5E, // 18.0 dB
	0x10D3F17, // 18.5 dB
	0x11D3346, // 19.0 dB
	0x12E1977, // 19.5 dB
	0x1400000, // 20.0 dB
	0x152F610, // 20.5 dB
	0x1670BC0, // 21.0 dB
	0x17C521A, // 21.5 dB
	0x192DB2B, // 22.0 dB
	0x1AABA13, // 22.5 dB
	0x1C40313, // 23.0 dB
	0x1DECB9F, // 23.5 dB
	0x1FB2A73, // 24.0 dB
	0x21937A0, // 24.5 dB
	0x2390CA6, // 25.0 dB
};
struct snd_pcm_substream;
struct mtk_base_irq_data;
struct clk;
struct mt6989_compress_info {
	int card;
	int device;
	int dir;
	char id[64];
};
struct mt6989_afe_private {
	struct clk **clk;
	struct regmap *topckgen;
	struct regmap *apmixed;
	struct regmap *infracfg;
	struct regmap *pmic_regmap;
	int irq_cnt[MT6989_MEMIF_NUM];
	int stf_positive_gain_db;
	int dram_resource_counter;
	int sgen_sel;
	int sgen_mode;
	int sgen_rate;
	int sgen_amplitude;
	/* usb call */
	int usb_call_echo_ref_enable;
	int usb_call_echo_ref_size;
	bool usb_call_echo_ref_reallocate;
	/* deep buffer playback */
	int deep_playback_state;
	/* fast playback */
	int fast_playback_state;
	/* mmap playback */
	int mmap_playback_state;
	/* mmap record */
	int mmap_record_state;
	/* primary playback */
	int primary_playback_state;
	/* voip rx */
	int voip_rx_state;
	/* xrun assert */
	int xrun_assert[MT6989_MEMIF_NUM];

	/* dai */
	bool dai_on[MT6989_DAI_NUM];
	void *dai_priv[MT6989_DAI_NUM];

	/* adda */
	int mtkaif_protocol;
	int mtkaif_chosen_phase[4];
	int mtkaif_phase_cycle[4];
	int mtkaif_calibration_num_phase;
	int mtkaif_dmic;
	int mtkaif_dmic_ch34;
	int mtkaif_adda6_only;
	/* support ap_dmic */
	int ap_dmic;
	unsigned int audio_r_miso1_enable;
	unsigned int miso_only;
	/* add for vs1 voter */
	/* adda dl/ul is on */
	bool is_adda_dl_on;
	bool is_adda_ul_on;
	/* vow is on */
	bool is_vow_enable;
	/* adda dl vol idx is at maximum */
	bool is_adda_dl_max_vol;
	/* current vote status of vs1 */
	bool is_mt6363_vote;

	/* mck */
	int mck_rate[MT6989_MCK_NUM];

	/* speech mixctrl instead property usage */
	int speech_a2m_msg_id;
	int speech_md_status;
	int speech_adsp_status;
	int speech_mic_mute;
	int speech_dl_mute;
	int speech_ul_mute;
	int speech_phone1_md_idx;
	int speech_phone2_md_idx;
	int speech_phone_id;
	int speech_md_epof;
	int speech_bt_sco_wb;
	int speech_shm_init;
	int speech_shm_usip;
	int speech_shm_widx;
	int speech_md_headversion;
	int speech_md_version;
	int speech_cust_param_init;

	/* regulator */
	struct regulator *reg_vib18;
};

struct audio_swpm_data {
	unsigned int afe_on;
	unsigned int user_case;
	unsigned int output_device;
	unsigned int input_device;
	unsigned int adda_mode;
	unsigned int sample_rate;
	unsigned int channel_num;
};

int mt6989_dai_adda_register(struct mtk_base_afe *afe);
int mt6989_dai_i2s_register(struct mtk_base_afe *afe);
int mt6989_dai_hw_gain_register(struct mtk_base_afe *afe);
int mt6989_dai_src_register(struct mtk_base_afe *afe);
int mt6989_dai_pcm_register(struct mtk_base_afe *afe);
int mt6989_dai_tdm_register(struct mtk_base_afe *afe);

int mt6989_dai_hostless_register(struct mtk_base_afe *afe);

int mt6989_add_misc_control(struct snd_soc_component *component);

int mt6989_set_local_afe(struct mtk_base_afe *afe);

unsigned int mt6989_general_rate_transform(struct device *dev,
		unsigned int rate);
unsigned int mt6989_rate_transform(struct device *dev,
				   unsigned int rate, int aud_blk);
int mt6989_dai_set_priv(struct mtk_base_afe *afe, int id,
			int priv_size, const void *priv_data);

int mt6989_enable_dc_compensation(bool enable);
int mt6989_set_lch_dc_compensation(int value);
int mt6989_set_rch_dc_compensation(int value);
int mt6989_adda_dl_gain_control(bool mute);

/* audio delay*/
void mt6989_aud_delay(unsigned long cycles);


#endif
