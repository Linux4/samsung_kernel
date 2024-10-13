/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef __SEC_AUDIO_ADAPTATION_H
#define __SEC_AUDIO_ADAPTATION_H

#include <dsp/q6afe-v2.h>
#include <dsp/q6asm-v2.h>
#include <dsp/q6adm-v2.h>

/* Samsung Record */
#define MODULE_ID_PP_SS_REC             0x10001050
#define PARAM_ID_PP_SS_REC_GETPARAMS    0x10001052
#define PARAM_ID_PP_SS_REC_SETPARAMS    0x1000BA30

/* Sound Alive */
#define MODULE_ID_PP_SA                 0x10001fa0
#define PARAM_ID_PP_SA_PARAMS           0x10001fa1
#define MODULE_ID_PP_SA_VSP             0x10001fb0
#define PARAM_ID_PP_SA_VSP_PARAMS       0x10001fb1
#define MODULE_ID_PP_ADAPTATION_SOUND		 0x10001fc0
#define PARAM_ID_PP_ADAPTATION_SOUND_PARAMS  0x10001fc1
#define MODULE_ID_PP_LRSM               0x10001fe0
#define PARAM_ID_PP_LRSM_PARAMS         0x10001fe1
#define MODULE_ID_PP_SA_MSP             0x10001ff0
#define MODULE_ID_PP_SA_MSP_PARAM       0x10001ff1

/* Sound Booster*/
#define MODULE_ID_PP_SB                 0x10001f01
#define PARAM_ID_PP_SB_PARAM            0x10001f04
#define PARAM_ID_PP_SB_ROTATION_PARAM	0x10001f02
#define PARAM_ID_PP_SB_FLATMOTION_PARAM    0x10001f05
#define PARAM_ID_PP_SB_PARAMS_VOLUME     0x10001f00

/* Upscaler */
#define MODULE_ID_PP_SA_UPSCALER_COLOR            0x10001f20
#define PARAM_ID_PP_SA_UPSCALER_COLOR_PARAMS      0x10001f21

/* Dolby */
#define MODULE_ID_PP_DOLBY_DAP 0x10001fd0
#define PARAM_ID_PP_DOLBY_DAP_PARAMS 0x10001fd1

/* Volume Monitor */
#define VOLUME_MONITOR_GET_PAYLOAD_SIZE 61
#define VOLUME_MONITOR_SET_PAYLOAD_SIZE 4
#define AFE_MODULE_ID_VOLUME_MONITOR 0x10001f41
#define AFE_MODULE_PARAM_ID_GET_VOLUME_MONITOR 0x10001f42
#define AFE_MODULE_PARAM_ID_SET_VOLUME_MONITOR 0x10001f43

/* Listenback */
#define MODULE_ID_PP_LISTENBACK		 0x10001f51
#define PARAM_ID_PP_LISTENBACK_SET_PARAMS  0x10001f52

/* Remote MIC */
#define AFE_MODULE_LVVEFQ_TX				0x1000B501
#define DIAMONDVOICE_REMOTEVOL_PARAM		0x10001012

enum {
	DEEP_OFFLOAD_MODE = 0,
	LOW_LATENCY_MODE,
	RINGTONE_MODE,
};

enum sb_type {
	SB_DISABLE,
	SB_ENABLE,
	SB_RINGTONE,
	SB_REARLEFT,
	SB_REARRIGHT,
	SB_FRONTLEFT,
	SB_FRONTRIGHT,
	SB_ROTATION,
	SB_ROTATION_LL,
	SB_ROTATION_RINGTONE,
	SB_FLATMOTION,
	SB_VOLUME,
	SB_MAX,
};

struct asm_stream_cmd_set_pp_params_sa {
	int16_t OutDevice;
	int16_t Preset;
	int32_t EqLev[9];
	int16_t m3Dlevel;
	int16_t BElevel;
	int16_t CHlevel;
	int16_t CHRoomSize;
	int16_t Clalevel;
	int16_t volume;
	int16_t Sqrow;
	int16_t Sqcol;
	int16_t TabInfo;
	int16_t NewUI;
	int16_t m3DPositionOn;
	int16_t reserved;
	int32_t m3DPositionAngle[2];
	int32_t m3DPositionGain[2];
	int32_t AHDRonoff;
} __packed;

struct asm_stream_cmd_set_pp_params_vsp {
	uint32_t speed_int;
} __packed;

struct asm_stream_cmd_set_pp_params_adaptation_sound {
	int32_t enable;
	int16_t gain[2][6];
	int16_t device;
} __packed;

struct asm_stream_cmd_set_pp_params_lrsm {
	int16_t sm;
	int16_t lr;
} __packed;

struct asm_stream_cmd_set_pp_params_msp {
	uint32_t msp_int;
} __packed;

struct adm_param_soundbooster_t {
	uint32_t sb_enable;
} __packed;

struct asm_stream_cmd_set_pp_params_upscaler {
	uint32_t upscaler_enable;
} __packed;

struct adm_param_sb_rotation {
	uint32_t sb_rotation;
} __packed;

struct adm_param_sb_flatmotion {
	uint32_t sb_flatmotion;
} __packed;

struct adm_param_sb_volume {
	uint32_t sb_volume;
} __packed;

struct asm_stream_cmd_set_pp_params_dolby_atmos {
	uint32_t enable;
	int16_t device;
	int16_t dolby_profile;
	int16_t swap_param;
} __packed;

struct adm_param_interview_operating_mode {
	uint32_t onoff;
} __packed;

struct afe_volume_monitor_set_params_t {
	uint32_t  payload[VOLUME_MONITOR_SET_PAYLOAD_SIZE];
} __packed;

struct afe_volume_monitor_get_params_t {
	uint32_t  payload[VOLUME_MONITOR_GET_PAYLOAD_SIZE];
} __packed;

struct afe_remote_mic_params_t {
	uint32_t  payload;
} __packed;

struct afe_listen_enable_params_t {
	uint32_t  payload;
} __packed;

int afe_get_volume_monitor(int port_id, int *volume_monitor_value);
int afe_set_volume_monitor(int port_id, int enable,  int volume_level, int avc_support, int db_atten);
int afe_set_sa_listenback(int port_id, int enable);
int afe_set_remote_mic_vol(int port_id, int vol_index);
#endif /* __SEC_AUDIO_ADAPTATION_H */
