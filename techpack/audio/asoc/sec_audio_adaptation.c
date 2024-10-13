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
#include <asoc/sec_audio_adaptation.h>
#include "msm-pcm-routing-v2.h"

struct afe_ctl {
	void *apr;
	atomic_t state;
	atomic_t status;
	wait_queue_head_t wait;
};

struct afe_port {
	unsigned int device_tx_port;
	unsigned int spk_rx_port;
	unsigned int usb_rx_port;
	unsigned int bt_rx_port;
	unsigned int headset_rx_port;
	unsigned int volume_monitor_port;
};

static struct afe_ctl this_afe;
static struct audio_session *session;
static struct afe_port afe_port;
static struct mutex asm_lock;
static uint32_t upscaler_val;

int q6asm_set_sound_alive(struct audio_client *ac, long *param)
{
	struct asm_stream_cmd_set_pp_params_sa cmd;
	struct param_hdr_v3 param_info;
	int rc = 0;
	int i = 0;

	if (ac == NULL) {
		pr_err("%s: Audio client is NULL\n", __func__);
		return -EINVAL;
	}

	if (param == NULL) {
		pr_err("%s: param is NULL\n", __func__);
		return -EINVAL;
	}

	memset(&param_info, 0, sizeof(param_info));
	memset(&cmd, 0, sizeof(cmd));
	param_info.module_id = MODULE_ID_PP_SA;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = PARAM_ID_PP_SA_PARAMS;
	param_info.param_size = sizeof(cmd);

	/* SA paramerters */
	cmd.OutDevice = param[0];
	cmd.Preset = param[1];
	for (i = 0; i < 9; i++)
		cmd.EqLev[i] = param[i+2];
	cmd.m3Dlevel = param[11];
	cmd.BElevel = param[12];
	cmd.CHlevel = param[13];
	cmd.CHRoomSize = param[14];
	cmd.Clalevel = param[15];
	cmd.volume = param[16];
	cmd.Sqrow = param[17];
	cmd.Sqcol = param[18];
	cmd.TabInfo = param[19];
	cmd.NewUI = param[20];
	cmd.m3DPositionOn = param[21];
	cmd.m3DPositionAngle[0] = param[22];
	cmd.m3DPositionAngle[1] = param[23];
	cmd.m3DPositionGain[0] = param[24];
	cmd.m3DPositionGain[1] = param[25];
	cmd.AHDRonoff = param[26];
	pr_info("%s: %d %d %d%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
		__func__,
		cmd.OutDevice, cmd.Preset, cmd.EqLev[0],
		cmd.EqLev[1], cmd.EqLev[2], cmd.EqLev[3],
		cmd.EqLev[4], cmd.EqLev[5], cmd.EqLev[6],
		cmd.EqLev[7], cmd.EqLev[8],
		cmd.m3Dlevel, cmd.BElevel, cmd.CHlevel,
		cmd.CHRoomSize, cmd.Clalevel, cmd.volume,
		cmd.Sqrow, cmd.Sqcol, cmd.TabInfo,
		cmd.NewUI, cmd.m3DPositionOn,
		cmd.m3DPositionAngle[0], cmd.m3DPositionAngle[1],
		cmd.m3DPositionGain[0], cmd.m3DPositionGain[1],
		cmd.AHDRonoff);

	rc = q6asm_pack_and_set_pp_param_in_band(ac, param_info,
						 (u8 *) &cmd);
	if (rc)
		pr_err("%s: set-params send failed paramid[0x%x] rc %d\n",
			   __func__, param_info.param_id, rc);
	return rc;
}

int q6asm_set_sa_listenback(struct audio_client *ac, long *param)
{
	struct asm_stream_cmd_set_pp_params_sa cmd = {0, };
	struct param_hdr_v3 param_info;
	int rc = 0;

	if (ac == NULL) {
		pr_err("%s: Audio client is NULL\n", __func__);
		return -EINVAL;
	}

	if (param == NULL) {
		pr_err("%s: param is NULL\n", __func__);
		return -EINVAL;
	}

	memset(&param_info, 0, sizeof(param_info));
	memset(&cmd, 0, sizeof(cmd));
	param_info.module_id = MODULE_ID_PP_SA;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = PARAM_ID_PP_SA_PARAMS;
	param_info.param_size = sizeof(cmd);

	/* SA paramerters */
	cmd.OutDevice = 1; /* EAR */
	cmd.Preset = param[0];
	cmd.BElevel = param[1];
	cmd.Clalevel = param[2];
	cmd.TabInfo = 1;
	cmd.NewUI = 1;
	pr_info("%s: %d %d %d\n", __func__, cmd.Preset, cmd.BElevel, cmd.Clalevel);

	rc = q6asm_pack_and_set_pp_param_in_band(ac, param_info,
						 (u8 *) &cmd);
	if (rc)
		pr_err("%s: set-params send failed paramid[0x%x] rc %d\n",
			   __func__, param_info.param_id, rc);
	return rc;
}

int q6asm_set_play_speed(struct audio_client *ac, long *param)
{
	struct asm_stream_cmd_set_pp_params_vsp cmd;
	struct param_hdr_v3 param_info;
	int rc = 0;

	if (ac == NULL) {
		pr_err("%s: Audio client is NULL\n", __func__);
		return -EINVAL;
	}

	if (param == NULL) {
		pr_err("%s: param is NULL\n", __func__);
		return -EINVAL;
	}

	memset(&param_info, 0, sizeof(param_info));
	memset(&cmd, 0, sizeof(cmd));
	param_info.module_id = MODULE_ID_PP_SA_VSP;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = PARAM_ID_PP_SA_VSP_PARAMS;
	param_info.param_size = sizeof(cmd);

	/* play speed paramerters */
	cmd.speed_int = param[0];
	pr_info("%s: %d\n", __func__, cmd.speed_int);

	rc = q6asm_pack_and_set_pp_param_in_band(ac, param_info,
						 (u8 *) &cmd);
	if (rc)
		pr_err("%s: set-params send failed paramid[0x%x] rc %d\n",
			   __func__, param_info.param_id, rc);
	return rc;
}

int q6asm_set_adaptation_sound(struct audio_client *ac, long *param)
{
	struct asm_stream_cmd_set_pp_params_adaptation_sound cmd;
	struct param_hdr_v3 param_info;
	int rc = 0;
	int i = 0;

	if (ac == NULL) {
		pr_err("%s: Audio client is NULL\n", __func__);
		return -EINVAL;
	}

	if (param == NULL) {
		pr_err("%s: param is NULL\n", __func__);
		return -EINVAL;
	}

	memset(&param_info, 0, sizeof(param_info));
	memset(&cmd, 0, sizeof(cmd));
	param_info.module_id = MODULE_ID_PP_ADAPTATION_SOUND;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = PARAM_ID_PP_ADAPTATION_SOUND_PARAMS;
	param_info.param_size = sizeof(cmd);

	/* adapt sound paramerters */
	cmd.enable = param[0];
	for (i = 0; i < 12; i++)
		cmd.gain[i/6][i%6] = param[i+1];
	cmd.device = param[13];
	pr_info("%s: %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
		__func__,
		cmd.enable, cmd.gain[0][0], cmd.gain[0][1], cmd.gain[0][2],
		cmd.gain[0][3], cmd.gain[0][4], cmd.gain[0][5], cmd.gain[1][0],
		cmd.gain[1][1], cmd.gain[1][2], cmd.gain[1][3], cmd.gain[1][4],
		cmd.gain[1][5], cmd.device);

	rc = q6asm_pack_and_set_pp_param_in_band(ac, param_info,
						 (u8 *) &cmd);
	if (rc)
		pr_err("%s: set-params send failed paramid[0x%x] rc %d\n",
			   __func__, param_info.param_id, rc);
	return rc;
}

int q6asm_set_sound_balance(struct audio_client *ac, long *param)
{
	struct asm_stream_cmd_set_pp_params_lrsm cmd;
	struct param_hdr_v3 param_info;
	int rc = 0;

	if (ac == NULL) {
		pr_err("%s: Audio client is NULL\n", __func__);
		return -EINVAL;
	}

	if (param == NULL) {
		pr_err("%s: param is NULL\n", __func__);
		return -EINVAL;
	}

	memset(&param_info, 0, sizeof(param_info));
	memset(&cmd, 0, sizeof(cmd));
	param_info.module_id = MODULE_ID_PP_LRSM;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = PARAM_ID_PP_LRSM_PARAMS;
	param_info.param_size = sizeof(cmd);

	/* sound balance paramerters */
	cmd.sm = param[0];
	cmd.lr = param[1];
	pr_info("%s: %d %d\n", __func__, cmd.sm, cmd.lr);

	rc = q6asm_pack_and_set_pp_param_in_band(ac, param_info,
						 (u8 *) &cmd);
	if (rc)
		pr_err("%s: set-params send failed paramid[0x%x] rc %d\n",
			   __func__, param_info.param_id, rc);
	return rc;
}

int q6asm_set_myspace(struct audio_client *ac, long *param)
{
	struct asm_stream_cmd_set_pp_params_msp cmd;
	struct param_hdr_v3 param_info;
	int rc = 0;

	if (ac == NULL) {
		pr_err("%s: Audio client is NULL\n", __func__);
		return -EINVAL;
	}

	if (param == NULL) {
		pr_err("%s: param is NULL\n", __func__);
		return -EINVAL;
	}

	memset(&param_info, 0, sizeof(param_info));
	memset(&cmd, 0, sizeof(cmd));
	param_info.module_id = MODULE_ID_PP_SA_MSP;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = MODULE_ID_PP_SA_MSP_PARAM;
	param_info.param_size = sizeof(cmd);

	/* myspace paramerters */
	cmd.msp_int = param[0];
	pr_info("%s: %d\n", __func__, cmd.msp_int);

	rc = q6asm_pack_and_set_pp_param_in_band(ac, param_info,
						 (u8 *) &cmd);
	if (rc)
		pr_err("%s: set-params send failed paramid[0x%x] rc %d\n",
			   __func__, param_info.param_id, rc);
	return rc;
}

int q6asm_set_upscaler(struct audio_client *ac, long *param)
{
	struct asm_stream_cmd_set_pp_params_upscaler cmd;
	struct param_hdr_v3 param_info;
	int rc = 0;

	if (ac == NULL) {
		pr_err("%s: Audio client is NULL\n", __func__);
		return -EINVAL;
	}

	if (param == NULL) {
		pr_err("%s: param is NULL\n", __func__);
		return -EINVAL;
	}

	memset(&param_info, 0, sizeof(param_info));
	memset(&cmd, 0, sizeof(cmd));
	param_info.module_id = MODULE_ID_PP_SA_UPSCALER_COLOR;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = PARAM_ID_PP_SA_UPSCALER_COLOR_PARAMS;
	param_info.param_size = sizeof(cmd);

	/* upscaler paramerters */
	cmd.upscaler_enable = param[0];
	pr_info("%s: %d\n", __func__, cmd.upscaler_enable);

	rc = q6asm_pack_and_set_pp_param_in_band(ac, param_info,
						 (u8 *) &cmd);
	if (rc)
		pr_err("%s: set-params send failed paramid[0x%x] rc %d\n",
			   __func__, param_info.param_id, rc);

	upscaler_val = cmd.upscaler_enable;

	return rc;
}

int q6asm_set_dolby_atmos(struct audio_client *ac, long *param)
{
	struct asm_stream_cmd_set_pp_params_dolby_atmos cmd;
	struct param_hdr_v3 param_info;
	int rc = 0;

	if (ac == NULL) {
		pr_err("%s: Audio client is NULL\n", __func__);
		return -EINVAL;
	}

	if (param == NULL) {
		pr_err("%s: param is NULL\n", __func__);
		return -EINVAL;
	}

	memset(&param_info, 0, sizeof(param_info));
	memset(&cmd, 0, sizeof(cmd));
	param_info.module_id = MODULE_ID_PP_DOLBY_DAP;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = PARAM_ID_PP_DOLBY_DAP_PARAMS;
	param_info.param_size = sizeof(cmd);

	/* rotation paramerters */
	cmd.enable = (uint32_t)param[0];
	cmd.device = (uint16_t)param[1];
	cmd.dolby_profile = (uint16_t)param[2];
	cmd.swap_param = (uint16_t)param[3];
	pr_info("%s: %d %d %d %d\n", __func__,
		cmd.enable, cmd.device, cmd.dolby_profile, cmd.swap_param);

	rc = q6asm_pack_and_set_pp_param_in_band(ac, param_info,
						 (u8 *) &cmd);
	if (rc)
		pr_err("%s: set-params send failed paramid[0x%x] rc %d\n",
			   __func__, param_info.param_id, rc);

	return rc;
}

int adm_set_sound_booster(int port_id, int copp_idx,
			long *param)
{
	struct adm_param_soundbooster_t sb_param;
	struct param_hdr_v3 param_hdr;
	int ret  = 0;

	if (param == NULL) {
		pr_err("%s: param is NULL\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: Enter\n", __func__);

	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = MODULE_ID_PP_SB;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = PARAM_ID_PP_SB_PARAM;
	param_hdr.param_size = sizeof(sb_param);
	/* soundbooster paramerters */
	sb_param.sb_enable = (uint32_t)param[0];

	pr_info("%s: Enter, port_id(0x%x), copp_idx(%d), enable(%d)\n",
		  __func__, port_id, copp_idx, sb_param.sb_enable);

	ret = adm_pack_and_set_one_pp_param(port_id, copp_idx, param_hdr,
					    (uint8_t *) &sb_param);
	if (ret)
		pr_err("%s: Failed to set sound booster params, err %d\n",
		       __func__, ret);

	pr_debug("%s: Exit, ret=%d\n", __func__, ret);

	return ret;
}

int adm_set_sb_rotation(int port_id, int copp_idx,
			uint32_t rotation)
{
	struct adm_param_sb_rotation cmd;
	struct param_hdr_v3 param_hdr;
	int ret  = 0;

	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = MODULE_ID_PP_SB;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = PARAM_ID_PP_SB_ROTATION_PARAM;
	param_hdr.param_size = sizeof(cmd);
	/* rotation paramerters */
	cmd.sb_rotation = rotation;

	pr_info("%s: Enter, port_id(0x%x), copp_idx(%d), enable(%d)\n",
		  __func__, port_id, copp_idx, cmd.sb_rotation);

	ret = adm_pack_and_set_one_pp_param(port_id, copp_idx, param_hdr,
					    (uint8_t *) &cmd);
	if (ret)
		pr_err("%s: Failed to set sb rotation params, err %d\n",
		       __func__, ret);

	pr_debug("%s: Exit, ret=%d\n", __func__, ret);

	return ret;
}

int adm_set_sb_flatmotion(int port_id, int copp_idx,
			long *param)
{
	struct adm_param_sb_flatmotion cmd;
	struct param_hdr_v3 param_hdr;
	int ret  = 0;

	if (param == NULL) {
		pr_err("%s: param is NULL\n", __func__);
		return -EINVAL;
	}

	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = MODULE_ID_PP_SB;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = PARAM_ID_PP_SB_FLATMOTION_PARAM;
	param_hdr.param_size = sizeof(cmd);
	/* flatmotion paramerters */
	cmd.sb_flatmotion = (unsigned int)param[0];

	pr_info("%s: Enter, port_id(0x%x), copp_idx(%d), enable(%d)\n",
		  __func__, port_id, copp_idx, cmd.sb_flatmotion);

	ret = adm_pack_and_set_one_pp_param(port_id, copp_idx, param_hdr,
					    (uint8_t *) &cmd);
	if (ret)
		pr_err("%s: Failed to set sb flatmotion params, err %d\n",
		       __func__, ret);

	pr_debug("%s: Exit, ret=%d\n", __func__, ret);

	return ret;
}

int adm_set_sb_volume(int port_id, int copp_idx,
			long *param)
{
	struct adm_param_sb_volume cmd;
	struct param_hdr_v3 param_hdr;
	int ret  = 0;

	if (param == NULL) {
		pr_err("%s: param is NULL\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: Enter\n", __func__);

	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = MODULE_ID_PP_SB;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = PARAM_ID_PP_SB_PARAMS_VOLUME;
	param_hdr.param_size = sizeof(cmd);
	/* soundbooster paramerters */
	cmd.sb_volume = (uint32_t)param[0];

	pr_info("%s: Enter, port_id(0x%x), copp_idx(%d), volume(%d)\n",
		  __func__, port_id, copp_idx, cmd.sb_volume);

	ret = adm_pack_and_set_one_pp_param(port_id, copp_idx, param_hdr,
					    (uint8_t *) &cmd);
	if (ret)
		pr_err("%s: Failed to set sound booster volume, err %d\n",
		       __func__, ret);

	pr_debug("%s: Exit, ret=%d\n", __func__, ret);

	return ret;
}

int adm_set_interview_operating_mode(int port_id, int copp_idx,
			long *param)
{
	struct adm_param_interview_operating_mode cmd;
	struct param_hdr_v3 param_hdr;
	int ret  = 0;

	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = MODULE_ID_PP_SS_REC;
	param_hdr.instance_id = 0x8000;
	param_hdr.param_id = PARAM_ID_PP_SS_REC_SETPARAMS;
	param_hdr.param_size = sizeof(cmd);
	/* Interview solution bypass paramerters */
	cmd.onoff = (uint32_t)param[0];

	pr_info("%s: Enter, port_id(0x%x), copp_idx(%d), enable(%d)\n",
		  __func__, port_id, copp_idx, cmd.onoff);

	ret = adm_pack_and_set_one_pp_param(port_id, copp_idx, param_hdr,
					    (uint8_t *) &cmd);
	if (ret)
		pr_err("%s: Failed to set interview mode params, err %d\n",
		       __func__, ret);

	pr_debug("%s: Exit, ret=%d\n", __func__, ret);

	return ret;
}

static int sec_audio_sound_alive_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_sa_listenback_rx_vol_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_sb_rx_vol_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_sb_fm_rx_vol_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_sa_listenback_rx_data_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_play_speed_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_adaptation_sound_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_sound_balance_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_voice_tracking_info_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;
	int be_idx = 0;
	char *param_value;
	int *update_param_value;
	uint32_t param_size = (RMS_PAYLOAD_LEN + 1) * sizeof(uint32_t);
	struct msm_pcm_routing_bdai_data msm_bedai;
	struct param_hdr_v3 param_hdr;

	param_value = kzalloc(param_size, GFP_KERNEL);
	if (!param_value)
		return -ENOMEM;

	for (be_idx = 0; be_idx < MSM_BACKEND_DAI_MAX; be_idx++) {
		msm_pcm_routing_get_bedai_info(be_idx, &msm_bedai);
		if (msm_bedai.port_id == afe_port.device_tx_port)
			break;
	}
	if ((be_idx < MSM_BACKEND_DAI_MAX) && msm_bedai.active) {
		memset(&param_hdr, 0, sizeof(param_hdr));
		param_hdr.module_id = MODULE_ID_PP_SS_REC;
		param_hdr.instance_id = 0x8000;
		param_hdr.param_id = PARAM_ID_PP_SS_REC_GETPARAMS;
		param_hdr.param_size = param_size;
		rc = adm_get_pp_params(afe_port.device_tx_port, 0,
				ADM_CLIENT_ID_DEFAULT, NULL, &param_hdr, param_value);
		if (rc) {
			pr_err("%s: get parameters failed:%d\n", __func__, rc);
			kfree(param_value);
			return -EINVAL;
		}

		if (param_value == NULL) {
			pr_err("%s: param_value is NULL\n", __func__);
			return -EINVAL;
		}

		update_param_value = (int *)param_value;
		ucontrol->value.integer.value[0] = update_param_value[0];

		pr_debug("%s: FROM DSP value[0] 0x%x\n",
			  __func__, update_param_value[0]);
	}
	kfree(param_value);

	return 0;
}

static int sec_audio_myspace_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_sound_boost_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_upscaler_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = upscaler_val;
	return 0;
}

static int sec_audio_sb_rotation_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_sb_flatmotion_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_dolby_atmos_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

int32_t volume_monitor_value[VOLUME_MONITOR_GET_PAYLOAD_SIZE] = {0};

static int sec_audio_volume_monitor_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	int be_idx = 0;
	int i;

	struct msm_pcm_routing_bdai_data msm_bedai;

	for (be_idx = 0; be_idx < MSM_BACKEND_DAI_MAX; be_idx++) {
		msm_pcm_routing_get_bedai_info(be_idx, &msm_bedai);

		if (msm_bedai.active) {
			if (msm_bedai.port_id == afe_port.headset_rx_port)
				break;
			if (msm_bedai.port_id == afe_port.bt_rx_port)
				break;
			if (msm_bedai.port_id == afe_port.usb_rx_port)
				break;
		}
	}
	if (be_idx == MSM_BACKEND_DAI_MAX) {
		pr_info("%s: no active backend port\n", __func__);
		goto done;
	}

	afe_port.volume_monitor_port = msm_bedai.port_id;

	ret = afe_get_volume_monitor(afe_port.volume_monitor_port, volume_monitor_value);

	if (ret) {
		pr_err("%s: fail afe_get_volume_monitor error = %d\n", __func__, ret);
		ret = -EINVAL;
		goto done;
	}

	if (volume_monitor_value == NULL) {
		pr_err("%s: volume_monitor_value is NULL\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < VOLUME_MONITOR_GET_PAYLOAD_SIZE; i++)
		ucontrol->value.integer.value[i] = volume_monitor_value[i];

done:
	return ret;
}

static int sec_audio_volume_monitor_data_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_sa_listenback_enable_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_interview_mode_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_sound_alive_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct msm_pcm_routing_fdai_data fe_dai_map;
	struct audio_client *ac;

	mutex_lock(&asm_lock);
	msm_pcm_routing_get_fedai_info(MSM_FRONTEND_DAI_MULTIMEDIA4,
			SESSION_TYPE_RX, &fe_dai_map);

	if ((fe_dai_map.strm_id <= 0) ||
	    (fe_dai_map.strm_id > ASM_ACTIVE_STREAMS_ALLOWED))
		goto done;

	ac = q6asm_get_audio_client(fe_dai_map.strm_id);
	ret = q6asm_set_sound_alive(ac,
		(long *)ucontrol->value.integer.value);

	if (ret < 0)
		pr_err("%s: sound alive cmd failed ret=%d\n", __func__, ret);

done:
	mutex_unlock(&asm_lock);
	return ret;
}

static int sec_audio_sa_listenback_rx_vol_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	uint32_t gain_list[8];
	int ret = 0;
	struct audio_client *ac;
	struct msm_pcm_routing_fdai_data fe_dai_map;

	gain_list[0] = (uint32_t)ucontrol->value.integer.value[0];
	gain_list[1] = (uint32_t)ucontrol->value.integer.value[0];
	gain_list[2] = (uint32_t)ucontrol->value.integer.value[0];

	mutex_lock(&asm_lock);
	msm_pcm_routing_get_fedai_info(MSM_FRONTEND_DAI_MULTIMEDIA6,
					SESSION_TYPE_RX, &fe_dai_map);

	if ((fe_dai_map.strm_id <= 0) ||
	    (fe_dai_map.strm_id > ASM_ACTIVE_STREAMS_ALLOWED)) {
		pr_info("%s: audio session is not active : %d\n",
			__func__, fe_dai_map.strm_id);
		ret = -EINVAL;
		goto done;
	}
	pr_info("%s: stream id %d, vol = %d\n",
			__func__, fe_dai_map.strm_id, gain_list[0]);

	ac = q6asm_get_audio_client(fe_dai_map.strm_id);
	ret = q6asm_set_multich_gain(ac, 3/*num_channels*/, gain_list, NULL, true);
	if (ret < 0)
		pr_err("%s: listenback rx vol cmd failed ret=%d\n", __func__, ret);

done:
	mutex_unlock(&asm_lock);
	return ret;
}

static int sec_audio_sb_rx_vol_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	int port_id, copp_idx;

	port_id = afe_port.spk_rx_port;
	ret = q6audio_get_copp_idx_from_port_id(port_id, SB_VOLUME, &copp_idx);
	if (ret) {
		pr_err("%s: Could not get copp idx for port_id=%d\n",
			__func__, port_id);

		ret = -EINVAL;
		goto done;
	}

	ret = adm_set_sb_volume(port_id, copp_idx,
		(long *)ucontrol->value.integer.value);
	if (ret) {
		pr_err("%s: Error setting sound booster volume, err=%d\n",
			  __func__, ret);

		ret = -EINVAL;
		goto done;
	}

done:
	return ret;
}

static int sec_audio_sb_fm_rx_vol_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	uint32_t gain_list[8];
	int ret = 0;
	struct audio_client *ac;
	struct msm_pcm_routing_fdai_data fe_dai_map;

	gain_list[0] = (uint32_t)ucontrol->value.integer.value[0];
	gain_list[1] = (uint32_t)ucontrol->value.integer.value[0];
	gain_list[2] = (uint32_t)ucontrol->value.integer.value[0];

	mutex_lock(&asm_lock);
	msm_pcm_routing_get_fedai_info(MSM_FRONTEND_DAI_MULTIMEDIA6,
					SESSION_TYPE_RX, &fe_dai_map);

	if ((fe_dai_map.strm_id <= 0) ||
	    (fe_dai_map.strm_id > ASM_ACTIVE_STREAMS_ALLOWED)) {
		pr_info("%s: audio session is not active : %d\n",
			__func__, fe_dai_map.strm_id);
		ret = -EINVAL;
		goto done;
	}
	pr_info("%s: stream id %d, vol = %d\n",
			__func__, fe_dai_map.strm_id, gain_list[0]);

	ac = q6asm_get_audio_client(fe_dai_map.strm_id);
	ret = q6asm_set_multich_gain(ac, 3/*num_channels*/, gain_list, NULL, true);
	if (ret < 0)
		pr_err("%s: fm rx vol cmd failed ret=%d\n", __func__, ret);

done:
	mutex_unlock(&asm_lock);
	return ret;
}

static int sec_audio_sa_listenback_rx_data_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct msm_pcm_routing_fdai_data fe_dai_map;
	struct audio_client *ac;

	mutex_lock(&asm_lock);
	msm_pcm_routing_get_fedai_info(MSM_FRONTEND_DAI_MULTIMEDIA6,
				SESSION_TYPE_RX, &fe_dai_map);

	if ((fe_dai_map.strm_id <= 0) ||
	    (fe_dai_map.strm_id > ASM_ACTIVE_STREAMS_ALLOWED))
		goto done;

	pr_info("%s: stream id %d\n", __func__, fe_dai_map.strm_id);

	ac = q6asm_get_audio_client(fe_dai_map.strm_id);
	ret = q6asm_set_sa_listenback(ac, (long *)ucontrol->value.integer.value);
	if (ret < 0)
		pr_err("%s: listenback rx data cmd failed ret=%d\n", __func__, ret);

done:
	mutex_unlock(&asm_lock);
	return ret;
}

static int sec_audio_play_speed_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct msm_pcm_routing_fdai_data fe_dai_map;
	struct audio_client *ac;

	mutex_lock(&asm_lock);
	msm_pcm_routing_get_fedai_info(MSM_FRONTEND_DAI_MULTIMEDIA4,
			SESSION_TYPE_RX, &fe_dai_map);

	if ((fe_dai_map.strm_id <= 0) ||
	    (fe_dai_map.strm_id > ASM_ACTIVE_STREAMS_ALLOWED))
		goto done;

	ac = q6asm_get_audio_client(fe_dai_map.strm_id);
	ret = q6asm_set_play_speed(ac,
		(long *)ucontrol->value.integer.value);

	if (ret < 0)
		pr_err("%s: play speed cmd failed ret=%d\n", __func__, ret);

done:
	mutex_unlock(&asm_lock);
	return ret;
}

static int sec_audio_adaptation_sound_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct msm_pcm_routing_fdai_data fe_dai_map;
	struct audio_client *ac;

	mutex_lock(&asm_lock);
	msm_pcm_routing_get_fedai_info(MSM_FRONTEND_DAI_MULTIMEDIA4,
			SESSION_TYPE_RX, &fe_dai_map);

	if ((fe_dai_map.strm_id <= 0) ||
	    (fe_dai_map.strm_id > ASM_ACTIVE_STREAMS_ALLOWED))
		goto done;

	ac = q6asm_get_audio_client(fe_dai_map.strm_id);
	ret = q6asm_set_adaptation_sound(ac,
		(long *)ucontrol->value.integer.value);

	if (ret < 0)
		pr_err("%s: adaptation sound cmd failed ret=%d\n", __func__, ret);

done:
	mutex_unlock(&asm_lock);
	return ret;
}

static int sec_audio_sound_balance_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct msm_pcm_routing_fdai_data fe_dai_map;
	struct audio_client *ac;

	mutex_lock(&asm_lock);
	msm_pcm_routing_get_fedai_info(MSM_FRONTEND_DAI_MULTIMEDIA4,
			SESSION_TYPE_RX, &fe_dai_map);

	if ((fe_dai_map.strm_id <= 0) ||
	    (fe_dai_map.strm_id > ASM_ACTIVE_STREAMS_ALLOWED))
		goto done;

	ac = q6asm_get_audio_client(fe_dai_map.strm_id);
	ret = q6asm_set_sound_balance(ac,
		(long *)ucontrol->value.integer.value);

	if (ret < 0)
		pr_err("%s: sound balance cmd failed ret=%d\n", __func__, ret);

done:
	mutex_unlock(&asm_lock);
	return ret;
}

static int sec_audio_voice_tracking_info_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	return ret;
}

static int sec_audio_myspace_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct msm_pcm_routing_fdai_data fe_dai_map;
	struct audio_client *ac;

	mutex_lock(&asm_lock);
	msm_pcm_routing_get_fedai_info(MSM_FRONTEND_DAI_MULTIMEDIA4,
			SESSION_TYPE_RX, &fe_dai_map);

	if ((fe_dai_map.strm_id <= 0) ||
	    (fe_dai_map.strm_id > ASM_ACTIVE_STREAMS_ALLOWED))
		goto done;

	ac = q6asm_get_audio_client(fe_dai_map.strm_id);
	ret = q6asm_set_myspace(ac, (long *)ucontrol->value.integer.value);

	if (ret < 0)
		pr_err("%s: sound myspace cmd failed ret=%d\n", __func__, ret);

done:
	mutex_unlock(&asm_lock);
	return ret;
}

static int sec_audio_sound_boost_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	int port_id, copp_idx;
	enum sb_type func_type = (uint32_t)ucontrol->value.integer.value[0];

	port_id = afe_port.spk_rx_port;
	ret = q6audio_get_copp_idx_from_port_id(port_id, func_type, &copp_idx);
	if (ret) {
		pr_err("%s: Could not get copp idx for port_id=%d\n",
			__func__, port_id);

		ret = -EINVAL;
		goto done;
	}

	ret = adm_set_sound_booster(port_id, copp_idx,
		(long *)ucontrol->value.integer.value);
	if (ret) {
		pr_err("%s: Error setting Sound Focus Params, err=%d\n",
			  __func__, ret);

		ret = -EINVAL;
		goto done;
	}

done:
	return ret;
}

static int sec_audio_upscaler_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct msm_pcm_routing_fdai_data fe_dai_map;
	struct audio_client *ac;

	mutex_lock(&asm_lock);
	msm_pcm_routing_get_fedai_info(MSM_FRONTEND_DAI_MULTIMEDIA4,
			SESSION_TYPE_RX, &fe_dai_map);

	if ((fe_dai_map.strm_id <= 0) ||
	    (fe_dai_map.strm_id > ASM_ACTIVE_STREAMS_ALLOWED))
		goto done;

	ac = q6asm_get_audio_client(fe_dai_map.strm_id);
	ret = q6asm_set_upscaler(ac, (long *)ucontrol->value.integer.value);

	if (ret < 0)
		pr_err("%s: upscaler cmd failed ret=%d\n", __func__, ret);

done:
	mutex_unlock(&asm_lock);
	return ret;
}

/*
 * Stream Information
 * 0 : Deep/Offload
 * 1 : Low Latency
 *
 * Rotation Information
 * 0 : Top up
 * 1 : Left up
 * 2 : Bottom up
 * 3 : Right up
 */
static int sec_audio_sb_rotation_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	int port_id, copp_idx;
	uint32_t stream = 0;
	uint32_t rotation = 0;
	enum sb_type func_type = SB_MAX;

	stream = (uint32_t)ucontrol->value.integer.value[0];
	rotation = (uint32_t)ucontrol->value.integer.value[1];

	switch (stream) {
	case DEEP_OFFLOAD_MODE:
		func_type = SB_ROTATION;
		break;
	case LOW_LATENCY_MODE:
		func_type = SB_ROTATION_LL;
		break;
	case RINGTONE_MODE:
		func_type = SB_ROTATION_RINGTONE;
		break;
	default:
		pr_info("%s: unknown stream type\n", __func__);
		break;
	}

	port_id = afe_port.spk_rx_port;
	ret = q6audio_get_copp_idx_from_port_id(port_id, func_type, &copp_idx);
	if (ret) {
		pr_err("%s: Could not get copp idx for port_id=%d\n",
			__func__, port_id);

		ret = -EINVAL;
		goto done;
	}

	ret = adm_set_sb_rotation(port_id, copp_idx, rotation);
	if (ret) {
		pr_err("%s: Error setting Sound boost rotation, err=%d\n",
			  __func__, ret);

		ret = -EINVAL;
		goto done;
	}

done:
	return ret;
}

static int sec_audio_sb_flatmotion_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	int port_id, copp_idx;

	port_id = afe_port.spk_rx_port;
	ret = q6audio_get_copp_idx_from_port_id(port_id, SB_FLATMOTION, &copp_idx);
	if (ret) {
		pr_err("%s: Could not get copp idx for port_id=%d\n",
			__func__, port_id);

		ret = -EINVAL;
		goto done;
	}

	ret = adm_set_sb_flatmotion(port_id, copp_idx,
		(long *)ucontrol->value.integer.value);
	if (ret) {
		pr_err("%s: Error setting Sound boost flatmotion, err=%d\n",
			  __func__, ret);

		ret = -EINVAL;
		goto done;
	}

done:
	return ret;
}

static int sec_audio_dolby_atmos_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct msm_pcm_routing_fdai_data fe_dai_map;
	struct audio_client *ac;

	mutex_lock(&asm_lock);
	msm_pcm_routing_get_fedai_info(MSM_FRONTEND_DAI_MULTIMEDIA4,
			SESSION_TYPE_RX, &fe_dai_map);

	if ((fe_dai_map.strm_id <= 0) ||
	    (fe_dai_map.strm_id > ASM_ACTIVE_STREAMS_ALLOWED))
		goto done;

	ac = q6asm_get_audio_client(fe_dai_map.strm_id);
	ret = q6asm_set_dolby_atmos(ac, (long *)ucontrol->value.integer.value);

	if (ret < 0)
		pr_err("%s: dolby atmos cmd failed ret=%d\n", __func__, ret);

done:
	mutex_unlock(&asm_lock);
	return ret;
}

static int sec_audio_interview_mode_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	int port_id, copp_idx;

	port_id = afe_port.device_tx_port;
	copp_idx = sec_get_copp_idx(port_id, SESSION_TYPE_TX);
	if (copp_idx) {
		pr_err("%s: Could not get copp idx for port_id=%d\n",
			__func__, port_id);

		ret = -EINVAL;
		goto done;
	}

	ret = adm_set_interview_operating_mode(port_id, copp_idx,
		(long *)ucontrol->value.integer.value);
	if (ret) {
		pr_err("%s: Error setting interview mode, err=%d\n",
			  __func__, ret);

		ret = -EINVAL;
		goto done;
	}

done:
	return ret;
}

static int sec_audio_volume_monitor_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_volume_monitor_data_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	int port_id;
	int be_idx = 0;
	int enable = ucontrol->value.integer.value[0];
	int volume_level = ucontrol->value.integer.value[1];
	int avc_support = ucontrol->value.integer.value[2];
	int db_atten = ucontrol->value.integer.value[3];

	struct msm_pcm_routing_bdai_data msm_bedai;

	for (be_idx = 0; be_idx < MSM_BACKEND_DAI_MAX; be_idx++) {
		msm_pcm_routing_get_bedai_info(be_idx, &msm_bedai);

		if (msm_bedai.active) {
			if (msm_bedai.port_id == afe_port.headset_rx_port)
				break;
			if (msm_bedai.port_id == afe_port.bt_rx_port)
				break;
			if (msm_bedai.port_id == afe_port.usb_rx_port)
				break;
		}
	}

	if (be_idx == MSM_BACKEND_DAI_MAX) {
		pr_info("%s: no active backend port\n", __func__);
		goto done;
	}

	port_id = msm_bedai.port_id;

	pr_info("%s: port_id : %x , enable : %d volume_level : %d avc_support : %d db_atten : %d\n",
			__func__, port_id, enable, volume_level, avc_support, db_atten);

	ret = afe_set_volume_monitor(port_id, enable, volume_level, avc_support, db_atten);

	if (ret) {
		pr_err("%s: Error setting volume monitor, err=%d\n",
			  __func__, ret);

		ret = -EINVAL;
		goto done;
	}

done:
	return ret;
}

static int sec_audio_sa_listenback_enable_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	int port_id;
	int be_idx = 0;
	int enable = ucontrol->value.integer.value[0];

	struct msm_pcm_routing_bdai_data msm_bedai;

	for (be_idx = 0; be_idx < MSM_BACKEND_DAI_MAX; be_idx++) {
		msm_pcm_routing_get_bedai_info(be_idx, &msm_bedai);
		if (msm_bedai.active) {
			if (msm_bedai.port_id == afe_port.usb_rx_port)
				break;
			if (msm_bedai.port_id == afe_port.headset_rx_port)
				break;
		}
	}

	if (be_idx == MSM_BACKEND_DAI_MAX) {
		pr_info("%s: no active backend port\n", __func__);
		goto done;
	}

	port_id = msm_bedai.port_id;

	pr_info("%s: port_id : %x , enable : %d\n",
			__func__, port_id, enable);

	ret = afe_set_sa_listenback(port_id, enable);

	if (ret) {
		pr_err("%s: Error setting set listenback, err=%d\n",
			  __func__, ret);

		ret = -EINVAL;
		goto done;
	}

done:
	return ret;
}

static int sec_afe_remote_mic_vol(int index)
{
	int ret = 0;
	int port_id;
	int be_idx = 0;
	struct msm_pcm_routing_bdai_data msm_bedai;

	for (be_idx = 0; be_idx < MSM_BACKEND_DAI_MAX; be_idx++) {
		msm_pcm_routing_get_bedai_info(be_idx, &msm_bedai);
		if (msm_bedai.active) {
			if (msm_bedai.port_id == afe_port.device_tx_port)
				break;
		}
	}

	if (be_idx == MSM_BACKEND_DAI_MAX) {
		pr_info("%s: no active backend port\n", __func__);
		goto done;
	}

	port_id = msm_bedai.port_id;

	pr_info("%s: port_id : %x , index : %d\n",
			__func__, port_id, index);

	ret = afe_set_remote_mic_vol(port_id, index);

	if (ret) {
		pr_err("%s: Error setting set listenback, err=%d\n",
			  __func__, ret);

		ret = -EINVAL;
		goto done;
	}

done:
	return ret;
}

static int sec_remote_mic_vol_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_remote_mic_vol_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int volumeindex = ucontrol->value.integer.value[0];

	if ((volumeindex < 0) || (volumeindex > 15)) {
		pr_err("%s: volumeindex=%d is wrong value\n", __func__, volumeindex);
		return -EINVAL;
	}

	pr_debug("%s: volumeindex=%d\n", __func__, volumeindex);

	return sec_afe_remote_mic_vol(volumeindex);
}

static const struct snd_kcontrol_new samsung_solution_mixer_controls[] = {
	SOC_SINGLE_MULTI_EXT("SA data", SND_SOC_NOPM, 0, 65535, 0, 27,
				sec_audio_sound_alive_get,
				sec_audio_sound_alive_put),
	SOC_SINGLE_EXT("SA LISTENBACK RX volume", SND_SOC_NOPM, 0, 65535, 0,
				sec_audio_sa_listenback_rx_vol_get,
				sec_audio_sa_listenback_rx_vol_put),
	SOC_SINGLE_MULTI_EXT("SA LISTENBACK RX data", SND_SOC_NOPM, 0, 65535, 0, 27,
				sec_audio_sa_listenback_rx_data_get,
				sec_audio_sa_listenback_rx_data_put),
	SOC_SINGLE_MULTI_EXT("VSP data", SND_SOC_NOPM, 0, 65535, 0, 1,
				sec_audio_play_speed_get,
				sec_audio_play_speed_put),
	SOC_SINGLE_MULTI_EXT("Audio DHA data", SND_SOC_NOPM, 0, 65535, 0, 14,
				sec_audio_adaptation_sound_get,
				sec_audio_adaptation_sound_put),
	SOC_SINGLE_MULTI_EXT("LRSM data", SND_SOC_NOPM, 0, 65535, 0, 2,
				sec_audio_sound_balance_get,
				sec_audio_sound_balance_put),
	SOC_SINGLE_EXT("VoiceTrackInfo", SND_SOC_NOPM, 0, 2147483647, 0,
				sec_audio_voice_tracking_info_get,
				sec_audio_voice_tracking_info_put),
	SOC_SINGLE_MULTI_EXT("MSP data", SND_SOC_NOPM, 0, 65535, 0, 1,
				sec_audio_myspace_get, sec_audio_myspace_put),
	SOC_SINGLE_MULTI_EXT("SB enable", SND_SOC_NOPM, 0, 65535, 0, 1,
				sec_audio_sound_boost_get,
				sec_audio_sound_boost_put),
	SOC_SINGLE_MULTI_EXT("UPSCALER", SND_SOC_NOPM, 0, 65535, 0, 1,
				sec_audio_upscaler_get, sec_audio_upscaler_put),
	SOC_SINGLE_MULTI_EXT("SB rotation", SND_SOC_NOPM, 0, 65535, 0, 2,
				sec_audio_sb_rotation_get, sec_audio_sb_rotation_put),
	SOC_SINGLE_MULTI_EXT("SB flatmotion", SND_SOC_NOPM, 0, 65535, 0, 1,
				sec_audio_sb_flatmotion_get, sec_audio_sb_flatmotion_put),
	SOC_SINGLE_MULTI_EXT("DA data", SND_SOC_NOPM, 0, 65535, 0, 4,
				sec_audio_dolby_atmos_get, sec_audio_dolby_atmos_put),
	SOC_SINGLE_EXT("SB RX Volume", SND_SOC_NOPM, 0, 65535, 0,
				sec_audio_sb_rx_vol_get,
				sec_audio_sb_rx_vol_put),
	SOC_SINGLE_EXT("SB FM RX Volume", SND_SOC_NOPM, 0, 65535, 0,
				sec_audio_sb_fm_rx_vol_get,
				sec_audio_sb_fm_rx_vol_put),
	SOC_SINGLE_EXT("Remote Mic Vol Index", SND_SOC_NOPM, 0, 65535, 0,
				sec_remote_mic_vol_get,
				sec_remote_mic_vol_put),
	SOC_SINGLE_EXT("Interview Mode", SND_SOC_NOPM, 0, 1, 0,
				sec_audio_interview_mode_get, sec_audio_interview_mode_put),
	SOC_SINGLE_MULTI_EXT("VM Energy", SND_SOC_NOPM, 0, 65535, 0, 61,
				sec_audio_volume_monitor_get, sec_audio_volume_monitor_put),
	SOC_SINGLE_MULTI_EXT("VM data", SND_SOC_NOPM, 0, 65535, 0, 4,
				sec_audio_volume_monitor_data_get, sec_audio_volume_monitor_data_put),
	SOC_SINGLE_EXT("Listenback Enable", SND_SOC_NOPM, 0, 65535, 0,
				sec_audio_sa_listenback_enable_get,
				sec_audio_sa_listenback_enable_put),
};

static int q6audio_adaptation_platform_probe(struct snd_soc_component *component)
{
	pr_info("%s: platform\n", __func__);

	session = q6asm_get_audio_session();

	snd_soc_add_component_controls(component,
				samsung_solution_mixer_controls,
			ARRAY_SIZE(samsung_solution_mixer_controls));

	return 0;
}

static const struct snd_soc_component_driver q6audio_adaptation = {
	.probe		= q6audio_adaptation_platform_probe,
};

static int samsung_q6audio_adaptation_probe(struct platform_device *pdev)
{
	int ret;

	pr_info("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

	atomic_set(&this_afe.state, 0);
	atomic_set(&this_afe.status, 0);
	this_afe.apr = NULL;
	init_waitqueue_head(&this_afe.wait);
	mutex_init(&asm_lock);

	ret = of_property_read_u32(pdev->dev.of_node,
			"adaptation,device-tx-port-id",
			&afe_port.device_tx_port);
	if (ret)
		pr_err("%s : Unable to read Tx BE\n", __func__);

	ret = of_property_read_u32(pdev->dev.of_node,
			"adaptation,spk-rx-port-id",
			&afe_port.spk_rx_port);
	if (ret)
		pr_debug("%s : Unable to find amp-rx-port\n", __func__);

	ret = of_property_read_u32(pdev->dev.of_node,
			"adaptation,usb-rx-port-id",
			&afe_port.usb_rx_port);
	if (ret)
		pr_debug("%s : Unable to find usb-rx-port\n", __func__);

	ret = of_property_read_u32(pdev->dev.of_node,
			"adaptation,bt-rx-port-id",
			&afe_port.bt_rx_port);
	if (ret)
		pr_debug("%s : Unable to find bt-rx-port\n", __func__);

	ret = of_property_read_u32(pdev->dev.of_node,
			"adaptation,headset-rx-port-id",
			&afe_port.headset_rx_port);
	if (ret)
		pr_debug("%s : Unable to find headset-rx-port\n", __func__);

	return snd_soc_register_component(&pdev->dev,
		&q6audio_adaptation, NULL, 0);
}

static int samsung_q6audio_adaptation_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

static const struct of_device_id samsung_q6audio_adaptation_dt_match[] = {
	{.compatible = "samsung,q6audio-adaptation"},
	{}
};
MODULE_DEVICE_TABLE(of, samsung_q6audio_adaptation_dt_match);

static struct platform_driver samsung_q6audio_adaptation_driver = {
	.driver = {
		.name = "samsung-q6audio-adaptation",
		.owner = THIS_MODULE,
		.of_match_table = samsung_q6audio_adaptation_dt_match,
	},
	.probe = samsung_q6audio_adaptation_probe,
	.remove = samsung_q6audio_adaptation_remove,
};

int __init sec_soc_audio_platform_init(void)
{
	pr_debug("%s\n", __func__);
	return platform_driver_register(&samsung_q6audio_adaptation_driver);
}

void sec_soc_audio_platform_exit(void)
{
	pr_debug("%s\n", __func__);
	platform_driver_unregister(&samsung_q6audio_adaptation_driver);
}

MODULE_DESCRIPTION("Samsung Audio Adaptation platform driver");
MODULE_LICENSE("GPL v2");
