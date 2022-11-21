/*
 * es705-routes.h  --  Audience eS705 ALSA SoC Audio driver
 *
 * Copyright 2013 Audience, Inc.
 *
 * Author: Greg Clemson <gclemson@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ES705_ROUTES_H
#define _ES705_ROUTES_H

struct esxxx_route_config {
	const u32 *route;
	const u32 *nb;
	const u32 *wb;
	const u32 *swb;
	const u32 *fb;
};

enum {
	ROUTE_OFF,
	ROUTE_CS_VOICE_1MIC_CT,
	ROUTE_CS_VOICE_2MIC_CT,
	ROUTE_CS_VOICE_3MIC_CT,
	ROUTE_CS_VOICE_1MIC_FT,
	ROUTE_CS_VOICE_2MIC_FT,
	ROUTE_CS_VOICE_3MIC_FT,
	ROUTE_CS_VOICE_HEADSET,
	ROUTE_CS_VOICE_1MIC_HEADPHONE,
	ROUTE_VOIP_1MIC_CT,
	ROUTE_VOIP_2MIC_CT,
	ROUTE_VOIP_3MIC_CT,
	ROUTE_VOIP_1MIC_FT,
	ROUTE_VOIP_2MIC_FT,
	ROUTE_VOIP_3MIC_FT,
	ROUTE_VOIP_HEADSET,
	ROUTE_VOIP_1MIC_HEADPHONE,
	ROUTE_VOICE_ASR_1MIC,
	ROUTE_VOICE_ASR_2MIC,
	ROUTE_VOICE_ASR_3MIC,
	ROUTE_VOICESENSE_SBUSRX4,
	ROUTE_VOICESENSE_SBUSRX0,
	ROUTE_VOICESENSE_PDM,
	ROUTE_1CHAN_PLAYBACK,
	ROUTE_2CHAN_PLAYBACK,
	ROUTE_1CHAN_CAPTURE,
	ROUTE_2CHAN_CAPTURE,
	ROUTE_AUDIOZOOM_2MIC,
	ROUTE_AUDIOZOOM_3MIC,
	ROUTE_MAX
};

enum {
	RATE_NB,
	RATE_WB,
	RATE_SWB,
	RATE_FB,
	RATE_MAX
};

static const u32 route_off[] = {
	0xffffffff,
};

static const u32 route_cs_voice_1mic_ct[] = {
	0x903103e9,
	0xffffffff	/* terminate */
};
static const u32 nb_cs_voice_1mic_ct[] = {
	0x90310227,
	0xffffffff	/* terminate */
};
static const u32 wb_cs_voice_1mic_ct[] = {
	0x9031024f,
	0xffffffff	/* terminate */
};
static const u32 swb_cs_voice_1mic_ct[] = {
	0x90310250,
	0xffffffff	/* terminate */
};

static const u32 route_cs_voice_2mic_ct[] = {
	0x9031041b,	/* 1051, without PDM */
/*	0x90310577,	 1399 with PDM*/
	0xffffffff	/* terminate */
};
static const u32 nb_cs_voice_2mic_ct[] = {
	0x9031022a,
	0xffffffff	/* terminate */
};
static const u32 wb_cs_voice_2mic_ct[] = {
	0x9031022b,
	0xffffffff	/* terminate */
};
static const u32 swb_cs_voice_2mic_ct[] = {
	0x9031022c,
	0xffffffff	/* terminate */
};

static const u32 route_cs_voice_3mic_ct[] = {
	0x9031044d,	/* 1101, 3 mic, without PDM */
/*	0x903157d,	 1405, 3 mic, 1PDM-2Analog */
	0xffffffff	/* terminate */
};
static const u32 nb_cs_voice_3mic_ct[] = {
	0x9031022d,
	0xffffffff	/* terminate */
};
static const u32 wb_cs_voice_3mic_ct[] = {
	0x9031022e,
	0xffffffff	/* terminate */
};
static const u32 swb_cs_voice_3mic_ct[] = {
	0xffffffff	/* terminate */
};

static const u32 route_cs_voice_1mic_ft[] = {
	0x903103e9,
	0xffffffff	/* terminate */
};
static const u32 nb_cs_voice_1mic_ft[] = {
	0x90310230,
	0xffffffff	/* terminate */
};
static const u32 wb_cs_voice_1mic_ft[] = {
	0x9031024f,
	0xffffffff	/* terminate */
};
static const u32 swb_cs_voice_1mic_ft[] = {
	0x90310250,
	0xffffffff	/* terminate */
};

static const u32 route_cs_voice_2mic_ft[] = {
	0x9031041b,	/* 1051, without PDM  */
/*	0x90310577, 	1399, 2mic, PDM mic */
	0xffffffff	/* terminate */
};
static const u32 nb_cs_voice_2mic_ft[] = {
	0x9031023c,
	0xffffffff	/* terminate */
};
static const u32 wb_cs_voice_2mic_ft[] = {
	0x9031023a,
	0xffffffff	/* terminate */
};
static const u32 swb_cs_voice_2mic_ft[] = {
	0x9031023b,
	0xffffffff	/* terminate */
};

static const u32 route_cs_voice_3mic_ft[] = {
	0x9031044d,	/* 1101, without PDM */
	0xffffffff	/* terminate */
};
static const u32 nb_cs_voice_3mic_ft[] = {
	0x90310242,
	0xffffffff	/* terminate */
};
static const u32 wb_cs_voice_3mic_ft[] = {
	0x90310243,
	0xffffffff	/* terminate */
};
static const u32 swb_cs_voice_3mic_ft[] = {
	0xffffffff	/* terminate */
};

static const u32 route_cs_voice_headset[] = {
	0x903103ed,
	0xffffffff	/* terminate */
};
static const u32 nb_cs_voice_headset[] = {
	0x9031024e,
	0xffffffff	/* terminate */
};
static const u32 wb_cs_voice_headset[] = {
	0x9031024f,
	0xffffffff	/* terminate */
};
static const u32 swb_cs_voice_headset[] = {
	0xffffffff	/* terminate */
};

static const u32 route_cs_voice_1mic_headphone[] = {
	0x903103ed,
	0xffffffff	/* terminate */
};
static const u32 nb_cs_voice_1mic_headphone[] = {
	0xffffffff	/* terminate */
};
static const u32 wb_cs_voice_1mic_headphone[] = {
	0xffffffff	/* terminate */
};
static const u32 swb_cs_voice_1mic_headphone[] = {
	0xffffffff	/* terminate */
};

static const u32 route_voip_1mic_ct[] = {
	0x903103e9,
	0xffffffff	/* terminate */
};
static const u32 nb_voip_1mic_ct[] = {
	0x9031024e,
	0xffffffff	/* terminate */
};
static const u32 wb_voip_1mic_ct[] = {
	0x9031024f,
	0xffffffff	/* terminate */
};
static const u32 swb_voip_1mic_ct[] = {
	0x90310250,
	0xffffffff	/* terminate */
};

static const u32 route_voip_2mic_ct[] = {
	/*	0x9031041b,     without PDM  */
	0x90310577,	/* 1399 with PDM*/
	0xffffffff	/* terminate */
};
static const u32 nb_voip_2mic_ct[] = {
	0x9031022a,
	0xffffffff	/* terminate */
};
static const u32 wb_voip_2mic_ct[] = {
	0x9031022b,
	0xffffffff	/* terminate */
};
static const u32 swb_voip_2mic_ct[] = {
	0x9031022c,
	0xffffffff	/* terminate */
};
static const u32 fb_voip_2mic_ct[] = {
	0x90310272, /* 626 */
	0xffffffff	/* terminate */
};

static const u32 route_voip_3mic_ct[] = {
	0x9031057d,	/* 1405, 3 mic, 1PDM-2Analog */
	0xffffffff	/* terminate */
};
static const u32 nb_voip_3mic_ct[] = {
	0x9031022d, /* 557 */
	0xffffffff	/* terminate */
};
static const u32 wb_voip_3mic_ct[] = {
	0x9031022e, /* 558 */
	0xffffffff	/* terminate */
};
static const u32 swb_voip_3mic_ct[] = {
	0x9031022f, /* 559 */
	0xffffffff	/* terminate */
};
static const u32 fb_voip_3mic_ct[] = {
	0x90310273, /* 627 */
	0xffffffff	/* terminate */
};

static const u32 route_voip_1mic_ft[] = {
	0x903103e9,
	0xffffffff	/* terminate */
};
static const u32 nb_voip_1mic_ft[] = {
	0x9031024e,
	0xffffffff	/* terminate */
};
static const u32 wb_voip_1mic_ft[] = {
	0x9031024f,
	0xffffffff	/* terminate */
};
static const u32 swb_voip_1mic_ft[] = {
	0x90310250,
	0xffffffff	/* terminate */
};

static const u32 route_voip_2mic_ft[] = {
/*	0x9031041b, Analog Mic */
	0x90310577, /* 1399, 2mic, PDM mic */
	0xffffffff	/* terminate */
};
static const u32 nb_voip_2mic_ft[] = {
	0x90310239,
	0xffffffff	/* terminate */
};
static const u32 wb_voip_2mic_ft[] = {
	0x9031023a,
	0xffffffff	/* terminate */
};
static const u32 swb_voip_2mic_ft[] = {
	0x9031023b,
	0xffffffff	/* terminate */
};
static const u32 fb_voip_2mic_ft[] = {
	0x90310277, /* 631 */
	0xffffffff	/* terminate */
};

static const u32 route_voip_3mic_ft[] = {
	0x90310595,	/* 1429, 3 mic, 1PDM-2Analog, Ter is PDM Mic */
	0xffffffff	/* terminate */
};
static const u32 nb_voip_3mic_ft[] = {
	0x90310242, /* 578 */
	0xffffffff	/* terminate */
};
static const u32 wb_voip_3mic_ft[] = {
	0x90310243, /* 579 */
	0xffffffff	/* terminate */
};
static const u32 swb_voip_3mic_ft[] = {
	0x90310244, /* 580 */
	0xffffffff	/* terminate */
};
static const u32 fb_voip_3mic_ft[] = {
	0x9031027a, /* 634 */
	0xffffffff	/* terminate */
};

static const u32 route_voip_headset[] = {
	0x903103ed,
	0xffffffff	/* terminate */
};
static const u32 nb_voip_headset[] = {
	0x9031024e,
	0xffffffff	/* terminate */
};
static const u32 wb_voip_headset[] = {
	0x9031024f,
	0xffffffff	/* terminate */
};
static const u32 swb_voip_headset[] = {
	0xffffffff	/* terminate */
};

static const u32 route_voip_1mic_headphone[] = {
	0x903103ed,
	0xffffffff	/* terminate */
};
static const u32 nb_voip_1mic_headphone[] = {
	0xffffffff	/* terminate */
};
static const u32 wb_voip_1mic_headphone[] = {
	0xffffffff	/* terminate */
};
static const u32 swb_voip_1mic_headphone[] = {
	0xffffffff	/* terminate */
};

static const u32 route_voice_asr_1mic[] = {
	0x903104b1,
	0xffffffff	/* terminate */
};
static const u32 nb_voice_asr_1mic[] = {
	0xffffffff	/* terminate */
};
static const u32 wb_voice_asr_1mic[] = {
	0xffffffff	/* terminate */
};
static const u32 swb_voice_asr_1mic[] = {
	0xffffffff	/* terminate */
};

static const u32 route_voice_asr_2mic[] = {
	0x903104ca,
	0xffffffff	/* terminate */
};
static const u32 nb_voice_asr_2mic[] = {
	0xffffffff	/* terminate */
};
static const u32 wb_voice_asr_2mic[] = {
	0xffffffff	/* terminate */
};
static const u32 swb_voice_asr_2mic[] = {
	0xffffffff	/* terminate */
};

static const u32 route_voice_asr_3mic[] = {
	0xffffffff	/* terminate */
};
static const u32 nb_voice_asr_3mic[] = {
	0xffffffff	/* terminate */
};
static const u32 wb_voice_asr_3mic[] = {
	0xffffffff	/* terminate */
};
static const u32 swb_voice_asr_3mic[] = {
	0xffffffff	/* terminate */
};

static const u32 route_voicesense_sbusrx4[] = {
	0xb05a0a40,	/* SBUS.Rx4 -> RxChMgr0*/
	0xb05b0000,	/* Set PathId  RxChMgr0 PATH_ID_PRI */
	0xb0640038,	/* connect RxChMgr0.o0 */
	0xb0640190,	/* connect senseVoice.i0 */
	0xb0630003,	/* set rate RxChMgr0 8k*/
	0xb0630019,	/* set rate senseVoice 8k*/
	0xb0680300,	/* set group RxChMgr0 0*/
	0xb0681900,	/* set group sensevoice 0*/
	0xb00c0900,	/* setDeviceParamID set Multichannel Link Bits*/
	0x900d0000,	/* setDeviceParam Slimbus Linked*/
	0xffffffff	/* terminate */
};

static const u32 route_voicesense_pdm[] = {
	0x90310566,
	0xffffffff      /* terminate */
};


static const u32 route_voicesense_sbusrx0[] = {
	0xb05a0a00,	/* SBUS.Rx0 -> RxChMgr0*/
	0xb05b0000,	/* Set PathId  RxChMgr0 PATH_ID_PRI */
	0xb0640038,	/* connect RxChMgr0.o0 */
	0xb0640190,	/* connect senseVoice.i0 */
	0xb0630003,	/* set rate RxChMgr0 8k*/
	0xb0630019,	/* set rate senseVoice 8k*/
	0xb0680300,	/* set group RxChMgr0 0*/
	0xb0681900,	/* set group sensevoice 0*/
	0xb00c0900,	/* setDeviceParamID set Multichannel Link Bits*/
	0x900d0000,	/* setDeviceParam Slimbus Linked*/
	0xffffffff	/* terminate */
};

static const u32 route_1chan_playback[] = {
	0x9031047f,
	0xffffffff	/* terminate */
};

static const u32 route_2chan_playback[] = {
	0x90310483,
	0xffffffff	/* terminate */
};

static const u32 route_1chan_capture[] = {
	0x90310480,
	0xffffffff	/* terminate */
};

static const u32 route_2chan_capture[] = {
	0x90310484,
	0xffffffff	/* terminate */
};

static const u32 route_audiozoom_2mic[] = {
	0x9031054b,
	0xffffffff	/* terminate */
};

static const u32 route_audiozoom_3mic[] = {
	0x9031054d,
	0xffffffff	/* terminate */
};

static const struct esxxx_route_config es705_route_config[ROUTE_MAX] = {
	[ROUTE_OFF] = {
		.route = route_off,
	},
	[ROUTE_CS_VOICE_1MIC_CT] = {
		.route = route_cs_voice_1mic_ct,
		.nb = nb_cs_voice_1mic_ct,
		.wb = wb_cs_voice_1mic_ct,
		.swb = swb_cs_voice_1mic_ct,
	},
	[ROUTE_CS_VOICE_2MIC_CT] = {
		.route = route_cs_voice_2mic_ct,
		.nb = nb_cs_voice_2mic_ct,
		.wb = wb_cs_voice_2mic_ct,
		.swb = swb_cs_voice_2mic_ct,
	},
	[ROUTE_CS_VOICE_3MIC_CT] = {
		.route = route_cs_voice_3mic_ct,
		.nb = nb_cs_voice_3mic_ct,
		.wb = wb_cs_voice_3mic_ct,
		.swb = swb_cs_voice_3mic_ct,
	},
	[ROUTE_CS_VOICE_1MIC_FT] = {
		.route = route_cs_voice_1mic_ft,
		.nb = nb_cs_voice_1mic_ft,
		.wb = wb_cs_voice_1mic_ft,
		.swb = swb_cs_voice_1mic_ft,
	},
	[ROUTE_CS_VOICE_2MIC_FT] = {
		.route = route_cs_voice_2mic_ft,
		.nb = nb_cs_voice_2mic_ft,
		.wb = wb_cs_voice_2mic_ft,
		.swb = swb_cs_voice_2mic_ft,
	},
	[ROUTE_CS_VOICE_3MIC_FT] = {
		.route = route_cs_voice_3mic_ft,
		.nb = nb_cs_voice_3mic_ft,
		.wb = wb_cs_voice_3mic_ft,
		.swb = swb_cs_voice_3mic_ft,
	},
	[ROUTE_CS_VOICE_HEADSET] = {
		.route = route_cs_voice_headset,
		.nb = nb_cs_voice_headset,
		.wb = wb_cs_voice_headset,
		.swb = swb_cs_voice_headset,
	},
	[ROUTE_CS_VOICE_1MIC_HEADPHONE] = {
		.route = route_cs_voice_1mic_headphone,
		.nb = nb_cs_voice_1mic_headphone,
		.wb = wb_cs_voice_1mic_headphone,
		.swb = swb_cs_voice_1mic_headphone,
	},
	[ROUTE_VOIP_1MIC_CT] = {
		.route = route_voip_1mic_ct,
		.nb = nb_voip_1mic_ct,
		.wb = wb_voip_1mic_ct,
		.swb = swb_voip_1mic_ct,
	},
	[ROUTE_VOIP_2MIC_CT] = {
		.route = route_voip_2mic_ct,
		.nb = nb_voip_2mic_ct,
		.wb = wb_voip_2mic_ct,
		.swb = swb_voip_2mic_ct,
		.fb = fb_voip_2mic_ct,
	},
	[ROUTE_VOIP_3MIC_CT] = {
		.route = route_voip_3mic_ct,
		.nb = nb_voip_3mic_ct,
		.wb = wb_voip_3mic_ct,
		.swb = swb_voip_3mic_ct,
		.fb = fb_voip_3mic_ct,
	},
	[ROUTE_VOIP_1MIC_FT] = {
		.route = route_voip_1mic_ft,
		.nb = nb_voip_1mic_ft,
		.wb = wb_voip_1mic_ft,
		.swb = swb_voip_1mic_ft,
	},
	[ROUTE_VOIP_2MIC_FT] = {
		.route = route_voip_2mic_ft,
		.nb = nb_voip_2mic_ft,
		.wb = wb_voip_2mic_ft,
		.swb = swb_voip_2mic_ft,
		.fb = fb_voip_2mic_ft,
	},
	[ROUTE_VOIP_3MIC_FT] = {
		.route = route_voip_3mic_ft,
		.nb = nb_voip_3mic_ft,
		.wb = wb_voip_3mic_ft,
		.swb = swb_voip_3mic_ft,
		.fb = fb_voip_3mic_ft,
	},
	[ROUTE_VOIP_HEADSET] = {
		.route = route_voip_headset,
		.nb = nb_voip_headset,
		.wb = wb_voip_headset,
		.swb = swb_voip_headset,
	},
	[ROUTE_VOIP_1MIC_HEADPHONE] = {
		.route = route_voip_1mic_headphone,
		.nb = nb_voip_1mic_headphone,
		.wb = wb_voip_1mic_headphone,
		.swb = swb_voip_1mic_headphone,
	},
	[ROUTE_VOICE_ASR_1MIC] = {
		.route = route_voice_asr_1mic,
		.nb = nb_voice_asr_1mic,
		.wb = wb_voice_asr_1mic,
		.swb = swb_voice_asr_1mic,
	},
	[ROUTE_VOICE_ASR_2MIC] = {
		.route = route_voice_asr_2mic,
		.nb = nb_voice_asr_2mic,
		.wb = wb_voice_asr_2mic,
		.swb = swb_voice_asr_2mic,
	},
	[ROUTE_VOICE_ASR_3MIC] = {
		.route = route_voice_asr_3mic,
		.nb = nb_voice_asr_3mic,
		.wb = wb_voice_asr_3mic,
		.swb = swb_voice_asr_3mic,
	},
	[ROUTE_VOICESENSE_SBUSRX4] = {
		.route = route_voicesense_sbusrx4,
	},
	[ROUTE_VOICESENSE_SBUSRX0] = {
		.route = route_voicesense_sbusrx0,
	},
	[ROUTE_VOICESENSE_PDM] = {
		.route = route_voicesense_pdm,
	},
	[ROUTE_1CHAN_PLAYBACK] = {
		.route = route_1chan_playback,
	},
	[ROUTE_2CHAN_PLAYBACK] = {
		.route = route_2chan_playback,
	},
	[ROUTE_1CHAN_CAPTURE] = {
		.route = route_1chan_capture,
	},
	[ROUTE_2CHAN_CAPTURE] = {
		.route = route_2chan_capture,
	},
	[ROUTE_AUDIOZOOM_2MIC] = {
		.route = route_audiozoom_2mic,
	},
	[ROUTE_AUDIOZOOM_3MIC] = {
		.route = route_audiozoom_3mic,
	},
};

static const u32 route_id_00[] = {
	0x903103ed,	/* #1005 */
	0x9031024e,	/* #590 */
	0xffffffff		/* terminate */
};
#ifdef CONFIG_SND_SOC_3MIC_SUPPORT
static const u32 route_id_01[] = {
	0x903105A2,	/* #1442 */
	0x9031039D,	/* #925 */
	0xffffffff		/* terminate */
};

static const u32 route_id_02[] = {
	0x9031044D,	/* #1101 */
	0x9031022D,	/* #557 */
	0xffffffff		/* terminate */
};

static const u32 route_id_03[] = {
	0x903105A2,	/* #1442 */
	0x9031039D,	/* #925 */
	0x903103E6,	/* #998 */
	0xffffffff		/* terminate */
};

static const u32 route_id_04[] = {
	0x9031044D,	/* #1101 */
	0x9031022D,	/* #557 */
	0x903103BD,	/* #957 */
	0xffffffff		/* terminate */
};
#else
static const u32 route_id_01[] = {
	0x9031041b,	/* #1051 */
	0x90310391,	/* #913 */
	0xffffffff		/* terminate */
};

static const u32 route_id_02[] = {
	0x9031041b,	/* #1051 */
	0x9031022a,	/* #554 */
	0xffffffff		/* terminate */
};

static const u32 route_id_03[] = {
	0x9031041b,	/* #1051 */
	0x9031038d,	/* #909 */
	0x903103E6,	/* #998 */
	0xffffffff		/* terminate */
};

static const u32 route_id_04[] = {
	0x9031041b,	/* #1051 */
	0x90310227,	/* #551 */
	0x903103BD,	/* #957 */
	0xffffffff		/* terminate */
};
#endif
static const u32 route_id_05[] = {
	0x9031047f,	/* #1151 */
	0x90310357,	/* #855 */
	0xffffffff		/* terminate */
};

static const u32 route_id_06[] = {
	0x90310483,	/* #1155 */
	0x90310358,	/* #856 */
	0xffffffff		/* terminate */
};
#ifdef CONFIG_SND_SOC_3MIC_SUPPORT
static const u32 route_id_07[] = {
	0x9031054D,	/* #1357 */
	0x90310308,	/* #776 */
	0xffffffff		/* terminate */
};

static const u32 route_id_08[] = {
	0x9031054D,	/* #1357 */
	0x90310309,	/* #777 */
	0xffffffff		/* terminate */
};
#else
static const u32 route_id_07[] = {
	0x9031054b,	/* #1355 */
	0x903102f0,	/* #752 */
	0xffffffff		/* terminate */
};

static const u32 route_id_08[] = {
	0x9031054b,	/* #1355 */
	0x903102f1,	/* #753 */
	0xffffffff		/* terminate */
};
#endif

static const u32 route_id_09[] = {
	0xffffffff		/* terminate */
};

static const u32 route_id_10[] = {
	0xffffffff		/* terminate */
};

static const u32 route_id_11[] = {
	0x903103ed,	/* #1005 */
	0x9031024e,	/* #590 */
	0x9031026d,	/* #621 */
	0xffffffff		/* terminate */
};

static const u32 route_id_12[] = {
	0x903103e9,	/* #1001 */
	0x9031024e,	/* #590 */
	0x9031026d,	/* #621 */
	0xffffffff		/* terminate */
};
#ifdef CONFIG_SND_SOC_3MIC_SUPPORT

static const u32 route_id_13[] = {
#if 0
	0x903104E3,	/* #1251 */
	0x903103BA,	/* #954 */
#else /* AUDIENCE_CVQ */
	0x90310480, /* 1152 1ch recording */
	0x90310357, /* 855 1ch recording */
#endif
	0xffffffff		/* terminate */
};
#else
static const u32 route_id_13[] = {
	0x903104ca,	/* #1226 */
	0x903103ae,	/* #942 */
	0xffffffff		/* terminate */
};
#endif

static const u32 route_id_14[] = {
	0xffffffff		/* terminate */
};

static const u32 route_id_15[] = {
	0xffffffff		/* terminate */
};

static const u32 route_id_16[] = {
	0xffffffff		/* terminate */
};

static const u32 route_id_17[] = {
	0x903103e9,	/* #1001 */
	0x90310228,	/* #552 */
	0x9031026d,	/* #621 */
	0xffffffff		/* terminate */
};

static const u32 route_id_18[] = {
	0x903103e9,	/* #1001 */
	0x90310237,	/* #567 */
	0x9031026d,	/* #621 */
	0xffffffff		/* terminate */
};

static const u32 route_id_19[] = {
	0x903103ed,	/* #1001 */
	0x9031024f,	/* #591 */
	0x9031026d,	/* #621 */
	0xffffffff		/* terminate */
};

static const u32 route_id_20[] = {
	0xffffffff		/* terminate */
};

static const u32 route_id_21[] = {
	0x903103ed,	/* #1005 */
	0x9031024f,	/* #591 */
	0xffffffff		/* terminate */
};
#ifdef CONFIG_SND_SOC_3MIC_SUPPORT

static const u32 route_id_22[] = {
	0x903105A2,	/* #1442 */
	0x9031039E,	/* #926 */
	0xffffffff		/* terminate */
};

static const u32 route_id_23[] = {
	0x9031044D,	/* #1101 */
	0x9031022E,	/* #558 */
	0xffffffff		/* terminate */
};

static const u32 route_id_24[] = {
	0x903105A2,	/* #1442 */
	0x9031039E,	/* #926 */
	0x903103E7,	/* #999 */
	0xffffffff		/* terminate */
};

static const u32 route_id_25[] = {
	0x9031044D,	/* #1051 */
	0x9031022E,	/* #552 */
	0x903103BE,	/* #958 */
	0xffffffff		/* terminate */
};
#else
static const u32 route_id_22[] = {
	0x9031041b,	/* #1051 */
	0x90310392,	/* #914 */
	0xffffffff		/* terminate */
};

static const u32 route_id_23[] = {
	0x9031041b,	/* #1051 */
	0x9031022b,	/* #555 */
	0xffffffff		/* terminate */
};

static const u32 route_id_24[] = {
	0x9031041b,	/* #1051 */
	0x9031038e,	/* #910 */
	0x903103E7,	/* #999 */
	0xffffffff		/* terminate */
};

static const u32 route_id_25[] = {
	0x9031041b,	/* #1051 */
	0x90310228,	/* #552 */
	0x903103BE,	/* #958 */
	0xffffffff		/* terminate */
};
#endif

static const u32 route_id_26[] = {
	0xffffffff		/* terminate */
};

static const u32 route_sensory_pdm[] = {
	0xb05a0c00,
	0xb05b0000,
	0xb0640038,
	0xb0640190,
	0xb0630103,
	0xb0630119,
	0xb0680300,
	0xb0681900,
	0xb0690000,
	0xb015000b,
	0x9017e02b, /* Sensory input gain reg = 0xe02b */
	0x90181000, /* Sensory input gain value = 0x1000 */
	0xffffffff,

};
static const u32 route_none[] = {
	0xffffffff	
};

static const u32 route_id_28[] = {
	0x90310DAF,	/* #3503 */
	0x9031024E,	/* #590 */
	0xffffffff		/* terminate */
};

static const u32 route_id_29[] = {
	0x90310DAF,	/* #3503 */
	0x9031024F,	/* #591 */
	0xffffffff		/* terminate */
};

static const u32 route_id_30[] = {
	0x90310DAD,	/* #3501 */
	0x90310228,	/* #552 */
	0x9031026D,	/* #621 */
	0xffffffff		/* terminate */
};

static const u32 route_id_31[] = {
	0x90310DAD,	/* #3501 */
	0x90310237,	/* #567 */
	0x9031026D,	/* #621 */
	0xffffffff		/* terminate */
};

static const u32 route_id_32[] = {
	0x90310FC9,	/* #4041 */
	0x9031039D,	/* #925 */
	0xffffffff		/* terminate */
};

static const u32 route_id_33[] = {
	0x90310DBF,	/* #3519 */
	0x9031022D,	/* #557 */
	0xffffffff		/* terminate */
};

static const u32 route_id_34[] = {
	0x90310FC9,	/* #4041 */
	0x9031039D,	/* #925 */
	0x903103E6,	/* #998 */
	0xffffffff		/* terminate */
};

static const u32 route_id_35[] = {
	0x90310DBF,	/* #3519 */
	0x9031022D,	/* #557 */
	0x903103BD,	/* #957 */
	0xffffffff		/* terminate */
};

static const u32 route_id_36[] = {
	0x90310FC9,	/* #4041 */
	0x9031039E,	/* #926 */
	0xffffffff		/* terminate */
};

static const u32 route_id_37[] = {
	0x90310DBF,	/* #3519 */
	0x9031022E,	/* #558 */
	0xffffffff		/* terminate */
};

static const u32 route_id_38[] = {
	0x90310FC9,	/* #4041 */
	0x9031039E,	/* #926 */
	0x903103E7,	/* #999 */
	0xffffffff		/* terminate */
};

static const u32 route_id_39[] = {
	0x90310DBF,	/* #3519 */
	0x9031022E,	/* #558 */
	0x903103BE,	/* #958 */
	0xffffffff		/* terminate */
};

static const u32 *es705_route_configs[] = {
	route_id_00, /* Wired Headset, NB */
	route_id_01, /* 2Mic SPK, NB, FO, NS On */
	route_id_02, /* 2Mic HS, NB, NS On */
	route_id_03, /* 2Mic SPK, NB, FO,NS Off */
	route_id_04, /* 2Mic HS, NB, NS Off */
	route_id_05, /* 1ch PB */
	route_id_06, /* 2ch PB */
	route_id_07, /* 2Mic AudioZoom Interview */
	route_id_08, /* 2Mic AudioZoom Conversation */
	route_id_09, /* dummy */
	route_id_10, /* dummy */
	route_id_11, /* TTY VCO  */
	route_id_12, /* TTY HCO */
	route_id_13, /* 2Mic ASR AO */
	route_id_14, /* dummy */
	route_id_15, /* dummy */
	route_id_16, /* dummy */
	route_id_17, /* 1Mic HS, Loopback */
	route_id_18, /* 1Mic SPK, Loopback */
	route_id_19, /* 1Mic Wired Headset, Loopback */
	route_id_20, /* dummy */
	route_id_21, /* 1Mic Wired Headset, WB, NS Off */
	route_id_22, /* 2Mic SPK, WB, FO, NS On */
	route_id_23, /* 2Mic HS, WB, NS On */
	route_id_24, /* 2Mic SPK, WB, FO, NS Off */
	route_id_25, /* 2Mic HS, WB, NS Off */
	route_id_26, /* dummy */
	route_sensory_pdm,
	route_id_28, /* 1Mic WHS, NB, SW Bypass, CP2 */
	route_id_29, /* 1Mic WHS, WB, SW Bypass, CP2 */
	route_id_30, /* 1Mic HS, LB, SW Bypass, CP2 */
	route_id_31, /* 1Mic SPK, LB, SW Bypass, CP2 */
	route_id_32, /* 3Mic SPK, NB, NS On, CP2 */
	route_id_33, /* 3Mic HS, NB, NS On, CP2 */
	route_id_34, /* 3Mic SPK, NB, NS Off, CP2 */
	route_id_35, /* 3Mic HS, NB, NS Off, CP2 */
	route_id_36, /* 3Mic SPK, WB, NS On, CP2 */
	route_id_37, /* 3Mic HS, WB, NS On, CP2 */
	route_id_38, /* 3Mic SPK, WB, NS Off, CP2 */
	route_id_39, /* 3Mic HS, WB, NS Off, CP2 */
	route_none,	 /* 40 : do nothing */
};
#endif
