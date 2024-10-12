/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef __Q6VOICE_ADAPTATION_H
#define __Q6VOICE_ADAPTATION_H

/* NXP */
#define VOICE_MODULE_LVVEFQ_TX              0x1000B500
#define VPM_TX_SM_LVVEFQ_COPP_TOPOLOGY      0x1000BFF0
#define VPM_TX_DM_LVVEFQ_COPP_TOPOLOGY      0x1000BFF1
#define VPM_TX_QM_LVVEFQ_COPP_TOPOLOGY      0x1000BFF3
#define VPM_TX_SM_LVSAFQ_COPP_TOPOLOGY      0x1000B200
#define VPM_TX_DM_LVSAFQ_COPP_TOPOLOGY      0x1000B201

/* Fortemeia */
#define VOICE_FVSAM_MODULE					0x10001041
#define VOICE_WISEVOICE_MODULE				0x10001031
#define VOICE_TX_DIAMONDVOICE_FVSAM_SM      0x1000110B
#define VOICE_TX_DIAMONDVOICE_FVSAM_DM      0x1000110A
#define VOICE_TX_DIAMONDVOICE_FRSAM_DM      0x1000110C
#define VOICE_TX_DIAMONDVOICE_FVSAM_QM      0x1000110D

/* Solomon Voice */
#define TX_VOICE_SOLOMONVOICE               0x100010A0
#define VOICE_TX_SOLOMONVOICE_SM            0x100010AA
#define VOICE_TX_SOLOMONVOICE_DM            0x100010AB
#define VOICE_TX_SOLOMONVOICE_QM            0x100010AC

/* Loopback */
#define VOICEPROC_MODULE_VENC				0x00010F07
#define VOICE_PARAM_LOOPBACK_ENABLE			0x00010E18

#define VOICE_VOICEMODE_MODULE				0x10001001
#define VOICE_BT_RVC_VOL_PARAM				0x10001012
#define VOICE_ADAPTATION_SOUND_PARAM        0x10001022
#define VOICE_NBMODE_PARAM					0x10001023
#define VOICE_SPKMODE_PARAM					0x10001025
#define VOICE_RCVMODE_PARAM					0x10001027

#define VOICE_ECHO_REF_LCH_MUTE_PARAM       0x10001028
#define VOICE_NREC_MODE_DYNAMIC_PARAM       0x10001029
#define ENHANCED_VT_CALL_DYNAMIC_PARAM       0x1000102A

#define VOICE_MODULE_SET_DEVICE				0x10041000
#define VOICE_MODULE_SET_DEVICE_PARAM		0x10041001

#define VOICE_MODULE_ECHO_REF_MUTE			0x1000B500
#define VOICE_MODULE_ECHO_REF_MUTE_PARAM	0x1000105A

#define SEC_ADAPTATAION_LOOPBACK_SRC_PORT	2	/* CVS */
#define SEC_ADAPTATAION_VOICE_SRC_PORT		2	/* CVP */

enum {
	LOOPBACK_DISABLE = 0,
	LOOPBACK_ENABLE,
	LOOPBACK_NODELAY,
	LOOPBACK_ZERO_DELAY,
	LOOPBACK_MAX,
};

struct vss_icommon_cmd_set_ui_property_v2_t {
	uint32_t module_id;
	uint16_t instance_id;
	uint16_t reserved;
	uint32_t param_id;
	uint32_t param_size;
	uint16_t enable;
	uint16_t reserved_field;
};

struct vss_icommon_cmd_set_loopback_enable_t {
	uint32_t module_id;
	uint32_t param_id;
	uint16_t param_size;
	uint16_t reserved;
	uint16_t loopback_enable;
	uint16_t reserved_field;
};

struct vss_icommon_cmd_get_param_v3_t {
	uint32_t mem_handle;
	uint64_t mem_address;
	uint16_t mem_size;
	uint32_t module_id;
	uint16_t instance_id;
	uint16_t reserved;
	uint32_t param_id;
} __packed;

struct cvs_set_loopback_enable_cmd {
	struct apr_hdr hdr;
	uint32_t mem_handle;
	uint32_t mem_address_lsw;
	uint32_t mem_address_msw;
	uint32_t mem_size;
	struct vss_icommon_cmd_set_loopback_enable_t vss_set_loopback;
} __packed;

struct cvp_adaptation_sound_parm_send_t {
	uint32_t module_id;
	/* Unique ID of the module. */
	uint32_t param_id;
	/* Unique ID of the parameter. */
	uint16_t param_size;
	/* Size of the parameter in bytes: MOD_ENABLE_PARAM_LEN */
	uint16_t reserved;
	/* Reserved; set to 0. */
	uint16_t eq_mode;
	uint16_t select;
	int16_t param[12];
} __packed;

struct cvp_adaptation_sound_parm_send_cmd {
	struct apr_hdr hdr;
	uint32_t mem_handle;
	uint32_t mem_address_lsw;
	uint32_t mem_address_msw;
	uint32_t mem_size;
	struct cvp_adaptation_sound_parm_send_t adaptation_sound_data;
} __packed;

struct cvp_set_nbmode_enable_cmd {
	struct apr_hdr hdr;
	struct vss_icommon_cmd_set_ui_property_v2_t cvp_set_nbmode;
} __packed;

struct cvp_set_rcvmode_enable_cmd {
	struct apr_hdr hdr;
	struct vss_icommon_cmd_set_ui_property_v2_t cvp_set_rcvmode;
} __packed;

struct cvp_set_spkmode_enable_cmd {
	struct apr_hdr hdr;
	struct vss_icommon_cmd_set_ui_property_v2_t cvp_set_spkmode;
} __packed;

struct cvp_set_device_info_cmd {
	struct apr_hdr hdr;
	struct vss_icommon_cmd_set_ui_property_v2_t cvp_set_device_info;
} __packed;

struct cvp_get_echo_ref_mute_cmd {
	struct apr_hdr hdr;
	struct vss_icommon_cmd_get_param_v3_t cvp_get_echo_ref_mute;
} __packed;

struct cvp_set_ref_lch_mute_enable_cmd {
	struct apr_hdr hdr;
	struct vss_icommon_cmd_set_ui_property_v2_t cvp_set_ref_lch_mute;
} __packed;

struct cvp_set_aec_effect_cmd {
	struct apr_hdr hdr;
	struct vss_icommon_cmd_set_ui_property_v2_t cvp_set_aec_effect;
} __packed;

struct cvp_set_voice_remote_mic_cmd {
	struct apr_hdr hdr;
	struct vss_icommon_cmd_set_ui_property_v2_t cvp_set_voice_remote_mic;
} __packed;

struct cvp_set_voice_isolation_cmd {
	struct apr_hdr hdr;
	struct vss_icommon_cmd_set_ui_property_v2_t cvp_set_voice_isolation;
} __packed;

struct cvp_set_bt_rvc_vol_cmd {
	struct apr_hdr hdr;
	struct vss_icommon_cmd_set_ui_property_v2_t cvp_set_bt_rvc_vol;
} __packed;

int sec_voice_set_adaptation_sound(uint16_t mode,
	uint16_t select, int16_t *parameters);
int sec_voice_set_nb_mode(short enable);
int sec_voice_set_rcv_mode(short enable);
int sec_voice_set_spk_mode(short enable);
int sec_voice_set_device_info(short device);
int sec_voice_ref_lch_mute(short enable);
int sec_voice_request_echo_ref_mute(void);
int sec_voice_get_echo_ref_mute(void);
int sec_voice_aec_effect(short enable);
int sec_voice_set_bt_rvc_vol(short vol);
int sec_voice_get_loopback_enable(void);
void sec_voice_set_loopback_enable(int mode);
void voice_sec_loopback_start_cmd(u32 session_id);
void voice_sec_loopback_end_cmd(u32 session_id);
int sec_voice_isolation_mode(short mode);
#endif /* __Q6VOICE_ADAPTATION_H */
