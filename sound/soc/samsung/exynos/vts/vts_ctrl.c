// SPDX-License-Identifier: GPL-2.0-or-later
/* sound/soc/samsung/vts/vts_ctrl.c
 *
 * ALSA SoC - Samsung VTS driver
 *
 * Copyright (c) 2022 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <sound/tlv.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <linux/pm_runtime.h>
#include "vts_ctrl.h"
#include "vts_dbg.h"
#include "vts_res.h"

static const char * const vts_hpf_sel_texts[] = {"120Hz", "40Hz"};
static SOC_ENUM_SINGLE_DECL(vts_hpf_sel, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_HPF_SEL_OFFSET, vts_hpf_sel_texts);

static const char * const vts_cps_sel_texts[] = {"normal", "absolute"};
static SOC_ENUM_SINGLE_DECL(vts_cps_sel, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_CPS_SEL_OFFSET, vts_cps_sel_texts);

static const DECLARE_TLV_DB_SCALE(vts_gain_tlv_array, 0, 6, 0);

static const char * const vts_sys_sel_texts[] = {"512kHz", "768kHz",
	"384kHz", "2048kHz", "3072kHz_48kHz", "3072kHz_96kHz"};
static SOC_ENUM_SINGLE_DECL(vts_sys_sel, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_SYS_SEL_OFFSET, vts_sys_sel_texts);

static int vts_ctrl_sys_sel_put_enum(
	struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct device *dev = component->dev;
	unsigned int *item = ucontrol->value.enumerated.item;
	struct vts_data *data = vts_get_vts_data();

	vts_dev_dbg(dev, "%s(%u)\n", __func__, item[0]);

	data->syssel_rate = item[0];

	return snd_soc_put_enum_double(kcontrol, ucontrol);
}

static const char * const vts_polarity_clk_texts[] = {"rising edge of clock",
	"falling edge of clock"};
static SOC_ENUM_SINGLE_DECL(vts_polarity_clk, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_POLARITY_CLK_OFFSET, vts_polarity_clk_texts);

static const char * const vts_polarity_output_texts[] = {"right first",
	"left first"};
static SOC_ENUM_SINGLE_DECL(vts_polarity_output, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_POLARITY_OUTPUT_OFFSET, vts_polarity_output_texts);

static const char * const vts_polarity_input_texts[] = {"left PDM on CLK high",
	"left PDM on CLK low"};
static SOC_ENUM_SINGLE_DECL(vts_polarity_input, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_POLARITY_INPUT_OFFSET, vts_polarity_input_texts);

static const char * const vts_ovfw_ctrl_texts[] = {"limit", "reset"};
static SOC_ENUM_SINGLE_DECL(vts_ovfw_ctrl, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_OVFW_CTRL_OFFSET, vts_ovfw_ctrl_texts);

static const char * const vts_cic_sel_texts[] = {"Off", "On"};
static SOC_ENUM_SINGLE_DECL(vts_cic_sel, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_CIC_SEL_OFFSET, vts_cic_sel_texts);

static const char * const vtsvcrecog_mode_text[] = {
	"OFF", "ON"
};

static const char * const vtsactive_phrase_text[] = {
	"NONE", "SoundModel_ID_1", "SoundModel_ID_2", "SoundModel_ID_3"
};
static const char * const vtsforce_reset_text[] = {
	"NONE", "RESET"
};
static SOC_ENUM_SINGLE_EXT_DECL(vtsvcrecog_mode_enum, vtsvcrecog_mode_text);
static SOC_ENUM_SINGLE_EXT_DECL(vtsactive_phrase_enum, vtsactive_phrase_text);
static SOC_ENUM_SINGLE_EXT_DECL(vtsforce_reset_enum, vtsforce_reset_text);

static int vts_ctrl_load_sound_model(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);

	if (data->sm_loaded) {
		/*
		 * load sound model data before starting recognition
		 */

		data->sm_info.actual_sz = __ALIGN_UP(data->sm_info.actual_sz, 8);

		memcpy(data->sram_base +
			data->sm_info.offset,
			data->sm_data,
			data->sm_info.actual_sz);
		vts_dev_info(dev, "Binary uploaded size=%zu\n",
			data->sm_info.actual_sz);
		data->sm_loaded = false;
	} else {
		vts_dev_err(dev, "%s: Model Binary File not Loaded\n",
			__func__);
		return -EINVAL;
	}

	return 0;
}

static int vts_ctrl_start_recognization(struct device *dev, int start)
{
	struct vts_data *data = dev_get_drvdata(dev);
	int target_id = data->target_id;
	int result;
	u32 values[3];

	vts_dev_info(dev, "%s for %s start %d\n", __func__,
		vtsactive_phrase_text[target_id], start);

	start = !!start;
	if (start) {
		vts_dev_info(dev, "%s: for %s, sm_loaded: %d\n",
			__func__, vtsactive_phrase_text[target_id],
			data->sm_loaded);
		vts_dev_info(dev, "%s exec_mode %d active_trig :%d\n", __func__,
			data->exec_mode, target_id);

		result = vts_ctrl_load_sound_model(dev);
		if (result < 0) {
			vts_dev_err(dev, "%s: loading sound model failed\n",
				__func__);
			return result;
		}

		result = vts_set_dmicctrl(data->pdev, target_id, true);
		if (result < 0) {
			vts_dev_err(dev, "%s: MIC control failed\n",
						 __func__);
			return result;
		}

		values[0] = VTS_MIC_INPUT_CH;
		values[1] = data->mic_input_ch;
		values[2] = 0;
		result = vts_start_ipc_transaction(dev, data,
				VTS_IRQ_AP_COMMAND,
				&values, 0, 1);
		if (result < 0) {
			vts_dev_err(dev, "%s: vts ipc VTS_IRQ_AP_COMMAND failed: %d\n",
				__func__, result);
		} else {
			vts_dev_info(dev, "%s: sent mic_input_ch(%d)\n", __func__, data->mic_input_ch);
		}

		/* Send Start recognition IPC command to VTS */
		values[0] = target_id;
		values[1] = 0;
		values[2] = 0;
		result = vts_start_ipc_transaction(dev, data,
				VTS_IRQ_AP_START_RECOGNITION,
				&values, 0, 1);
		if (result < 0) {
			vts_dev_err(dev, "vts ipc VTS_IRQ_AP_START_RECOGNITION failed: %d\n",
				result);
			return result;
		}

		vts_intr_start_dump(dev, true);

		data->vts_state = VTS_STATE_RECOG_STARTED;
		data->poll_event_type = EVENT_NONE;
		vts_dev_info(dev, "%s start=%d, target_id=%d\n", __func__,
			start, target_id);
	} else if (!start) {
		if (data->vts_status == ABNORMAL) {
			// to avoid spending time for meaningless waiting
			vts_dev_err(dev, "%s: skip STOP_RECOGNITION ipc\n", __func__);
			goto skip_ipc;
		}

		values[0] = target_id;
		values[1] = 0;
		values[2] = 0;
		result = vts_start_ipc_transaction(dev, data,
				VTS_IRQ_AP_STOP_RECOGNITION,
				&values, 0, 1);

		if (result < 0) {
			vts_dev_err(dev, "vts ipc VTS_IRQ_AP_STOP_RECOGNITION failed: %d\n",
				result);
			/* HACK */
			/* return result; */
		}

		vts_intr_start_dump(dev, false);

skip_ipc:
		data->vts_state = VTS_STATE_RECOG_STOPPED;
		vts_dev_info(dev, "%s: start=%d, target_id=%d\n",
				   __func__, start, target_id);

		/* If clk_path is VTS_CLK_SRC_RCO or s5e8825, 9925 or 9935 sets below configuration.
		   So, don't need condition to set for now */
		result = vts_set_dmicctrl(data->pdev, target_id, false);
		if (result < 0) {
			vts_dev_err(dev, "%s: MIC control failed\n", __func__);
			return result;
		}
	}
	return 0;
}

static int vts_ctrl_get_vtsvoicerecognize_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct vts_data *data = vts_get_vts_data();

	ucontrol->value.integer.value[0] = data->exec_mode;

	vts_dev_dbg(component->dev, "GET VTS Execution mode: %d\n",
			 data->exec_mode);

	return 0;
}

static bool vts_ctrl_check_is_active_id(struct device *dev, int target_id)
{
	struct vts_data *data = dev_get_drvdata(dev);
	bool is_active = data->current_active_ids & (0x1 << target_id);

	vts_dev_info(dev, "%s: current: 0x%x, target_id %d is %s",
			__func__, data->current_active_ids, target_id,
			is_active ? "active" : "inactive");

	return is_active;
}

static void vts_ctrl_add_active_id(struct device *dev, int target_id)
{
	struct vts_data *data = dev_get_drvdata(dev);

	data->current_active_ids |= (0x1 << data->target_id);
}

static void vts_ctrl_remove_active_id(struct device *dev, int target_id)
{
	struct vts_data *data = dev_get_drvdata(dev);

	data->current_active_ids &= ~(0x1 << data->target_id);
}

static int vts_ctrl_set_voicerecognize_mode(struct device *dev, int vcrecognize_mode)
{
	struct vts_data *data = dev_get_drvdata(dev);
	int result = 0;
	int vcrecognize_start = 0;

	pm_runtime_barrier(dev);

	vts_dev_info(dev, "%s: requested id %d %s, current active recognition: 0x%x\n",
			 __func__, data->target_id,
			 vtsvcrecog_mode_text[vcrecognize_mode],
			 data->current_active_ids);

	if (vcrecognize_mode == VTS_RECOGNIZE_START) {
		if (vts_ctrl_check_is_active_id(dev, data->target_id)) {
			vts_dev_warn(dev, "%s: requested trigger id %d is already completed",
					__func__, data->target_id);
			return 0;
		}

		pm_runtime_get_sync(dev);
		vts_start_runtime_resume(dev, 0);
		if (data->syssel_rate == 1) {
			vts_clk_set_rate(dev, data->tri_clk_path);
		} else {
			vts_clk_set_rate(dev, data->aud_clk_path);
		}
		vcrecognize_start = true;
	} else {
		if (!vts_ctrl_check_is_active_id(dev, data->target_id)) {
			vts_dev_warn(dev, "%s: requested trigger id %d is already off",
					__func__, data->target_id);
			return 0;
		}
		vcrecognize_start = false;
	}

	if (!pm_runtime_active(dev)) {
		vts_dev_warn(dev, "%s wrong state %d req: %d\n",
				__func__, data->exec_mode,
				vcrecognize_mode);
		return 0;
	}

	data->exec_mode = vcrecognize_mode;

	/* Start/stop the request Voice recognization mode */
	result = vts_ctrl_start_recognization(dev,
					vcrecognize_start);
	if (result < 0) {
		vts_dev_err(dev, "Start Recognization Failed: %d\n",
			 result);
		goto err_ipcmode;
	}

	if (vcrecognize_start)
		vts_ctrl_add_active_id(dev, data->target_id);
	else
		vts_ctrl_remove_active_id(dev, data->target_id);

	vts_dev_info(dev, "%s Configured: [%d] %s started\n",
		 __func__, data->exec_mode,
		 vtsvcrecog_mode_text[vcrecognize_mode]);

	if (!vcrecognize_start && pm_runtime_active(dev))
		pm_runtime_put_sync(dev);

	return  0;

err_ipcmode:
	if (pm_runtime_active(dev) && vcrecognize_start)
		pm_runtime_put_sync(dev);

	if (!vcrecognize_start && pm_runtime_active(dev)) {
		pm_runtime_put_sync(dev);
		vts_ctrl_remove_active_id(dev, data->target_id);
	}

	return result;
}

static int vts_ctrl_set_vtsvoicerecognize_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct device *dev = component->dev;
	struct vts_data *data = dev_get_drvdata(dev);
	int vcrecognize_mode = 0;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;
	int ret = 0;

	vcrecognize_mode = ucontrol->value.integer.value[0];
	vts_dev_info(dev, "%s state %d %d (%d)\n", __func__,
			data->vts_state,
			data->clk_path,
			data->micclk_init_cnt);

	if (item[0] >= e->items) {
		vts_dev_err(dev,
			"%s try to set %d but should be under %d\n",
			__func__, item[0], e->items);

		return -EINVAL;
	}

	/* check dmic_if clk */
	if (data->clk_path == VTS_CLK_SRC_AUD0) {
		data->syssel_rate = 4;
		vts_dev_info(dev, "changed as 3072000Hz mode\n");
	}
	ret = vts_ctrl_set_voicerecognize_mode(dev, vcrecognize_mode);

	return ret;
}

static int vts_ctrl_get_vtsactive_phrase(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct vts_data *data = vts_get_vts_data();

	ucontrol->value.integer.value[0] = data->target_id;

	vts_dev_dbg(component->dev, "GET VTS Active Phrase: %s\n",
			vtsactive_phrase_text[data->target_id]);

	return 0;
}

static int vts_ctrl_set_vtsactive_phrase(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct vts_data *data = vts_get_vts_data();
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;
	int vtsactive_phrase;

	pm_runtime_barrier(component->dev);

	if (item[0] >= e->items) {
		vts_dev_err(component->dev,
			"%s try to set %d but shuld be under %d\n",
			__func__, item[0], e->items);

		return -EINVAL;
	}

	vtsactive_phrase = ucontrol->value.integer.value[0];

	if (vtsactive_phrase < 0) {
		vts_dev_err(component->dev,
		"Invalid VTS Trigger Key phrase =%d", vtsactive_phrase);
		return 0;
	}

	data->target_id = vtsactive_phrase;
	vts_dev_info(component->dev, "VTS Active phrase: %s\n",
		vtsactive_phrase_text[vtsactive_phrase]);

	return  0;
}

static int vts_ctrl_get_voicetrigger_value(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct vts_data *data = vts_get_vts_data();

	ucontrol->value.integer.value[0] = data->target_size;

	vts_dev_info(component->dev, "GET Voice Trigger Value: %d\n",
			data->target_size);

	return 0;
}

static int vts_ctrl_set_voicetrigger_value(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct vts_data *data = vts_get_vts_data();
	int target_id = data->target_id;
	u32 values[3];
	int result = 0;
	int trig_ms;

	pm_runtime_barrier(component->dev);

	trig_ms = ucontrol->value.integer.value[0];

	if (trig_ms > 2000 || trig_ms < 0) {
		vts_dev_err(component->dev,
		"Invalid Voice Trigger Value = %d (valid range 0~2000ms)",
			trig_ms);
		return 0;
	}

	/* Configure VTS target size */
	/* 1ms requires (16KHz,16bit,Mono) = 16samples * 2 bytes = 32 bytes*/
	values[0] = trig_ms * 32;
	values[1] = target_id;
	values[2] = 0;
	result = vts_start_ipc_transaction(component->dev, data,
		VTS_IRQ_AP_TARGET_SIZE, &values, 0, 1);
	if (result < 0) {
		vts_dev_err(component->dev, "Voice Trigger Value setting IPC Transaction Failed: %d\n",
			result);
		return result;
	}

	data->target_size = trig_ms;
	vts_dev_info(component->dev, "SET Voice Trigger Value: %dms\n",
		data->target_size);

	return 0;
}

static int vts_ctrl_get_vtsforce_reset(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct vts_data *data = vts_get_vts_data();

	ucontrol->value.integer.value[0] = data->running;

	vts_dev_dbg(component->dev, "GET VTS Force Reset: %s\n",
			(vts_is_running() ? "VTS Running" :
			"VTS Not Running"));

	return 0;
}

static int vts_ctrl_set_vtsforce_reset(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct device *dev = component->dev;

	vts_dev_info(dev, "%s: VTS RESET\n", __func__);

	while (vts_is_running() && pm_runtime_active(dev)) {
		vts_dev_warn(dev, "%s: clear active models\n", __func__);
		pm_runtime_put_sync(dev);
	}

	return  0;
}

static int vts_ctrl_get_force_trigger(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	/* Nothing To Do */

	return 0;
}

static int vts_ctrl_set_force_trigger(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component
		= snd_soc_kcontrol_component(kcontrol);
	struct device *dev = component->dev;
	struct vts_data *data = vts_get_vts_data();
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;
	u32 values[3];
	int result = 0;
	int val = ucontrol->value.integer.value[0];

	if (item[0] >= e->items) {
		vts_dev_err(dev,
			"%s try to set %d but should be under %d\n",
			__func__, item[0], e->items);
		return -EINVAL;
	}

	values[0] = VTS_FORCE_TRIGGER;
	values[1] = val;
	values[2] = 0;
	result = vts_start_ipc_transaction(dev, data,
					VTS_IRQ_AP_COMMAND,
					&values, 0, 0);
	if (result < 0) {
		vts_dev_err(dev, "Force Trigger setting IPC Transaction Failed: %d\n",
			result);
		return result;
	}

	vts_dev_info(dev, "Force Trigger Command, command num : %d\n", val);

	return  0;
}

static int vts_ctrl_get_supported_mic_num(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct vts_data *data = vts_get_vts_data();

	ucontrol->value.integer.value[0] = data->supported_mic_num;

	return 0;
}

static int vts_ctrl_set_supported_mic_num(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = component->dev;
	struct vts_data *data = vts_get_vts_data();
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;

	if (item[0] >= e->items) {
		vts_dev_err(dev,
			"%s try to set %d but should be under %d\n",
			__func__, item[0], e->items);
		return -EINVAL;
	}

	data->supported_mic_num = (unsigned int)ucontrol->value.integer.value[0];
	vts_dev_info(dev, "%s: %d\n", __func__, data->supported_mic_num);

	return 0;
}

static int vts_ctrl_get_force_error(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	/* Nothing To Do */

	return 0;
}

static int vts_ctrl_set_force_error(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = component->dev;
	struct vts_data *data = vts_get_vts_data();
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;
	u32 values[3];
	int result = 0;
	int val = ucontrol->value.integer.value[0];

	if (item[0] >= e->items) {
		vts_dev_err(dev,
			"%s try to set %d but should be under %d\n",
			__func__, item[0], e->items);
		return -EINVAL;
	}

	values[0] = VTS_FORCE_ERROR;
	values[1] = val;
	values[2] = 0;
	result = vts_start_ipc_transaction(dev, data,
					VTS_IRQ_AP_COMMAND,
					&values, 0, 0);
	if (result < 0) {
		vts_dev_err(dev, "Force Error setting IPC Transaction Failed: %d\n",
			result);
		return result;
	}

	vts_dev_info(dev, "%s: force err num : %d\n", __func__, val);

	return  0;
}

static int vts_ctrl_get_sysclk_div(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct vts_data *data = vts_get_vts_data();

	ucontrol->value.integer.value[0] = data->sysclk_div;

	return 0;
}

static int vts_ctrl_set_sysclk_div(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = component->dev;
	struct vts_data *data = vts_get_vts_data();
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;

	if (item[0] >= e->items) {
		vts_dev_err(dev,
			"%s try to set %d but should be under %d\n",
			__func__, item[0], e->items);
		return -EINVAL;
	}

	data->sysclk_div = (unsigned int)ucontrol->value.integer.value[0];
	data->target_sysclk = VTS_SYS_CLOCK_MAX / data->sysclk_div;

	vts_dev_info(dev, "%s: sysclk div(%d) target_sysclk(%d)\n",
			   __func__, data->sysclk_div, data->target_sysclk);

	return 0;
}

static int vts_ctrl_get_reset_support(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct vts_data *data = vts_get_vts_data();

	ucontrol->value.integer.value[0] = data->silent_reset_support;

	return 0;
}

static int vts_ctrl_set_reset_support(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = component->dev;
	struct vts_data *data = vts_get_vts_data();
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;

	if (item[0] >= e->items) {
		vts_dev_err(dev,
			"%s try to set %d but should be under %d\n",
			__func__, item[0], e->items);
		return -EINVAL;
	}

	data->silent_reset_support = (unsigned int)ucontrol->value.integer.value[0];

	vts_dev_info(dev, "%s: silent_reset_support(%d)\n", __func__, data->silent_reset_support);

	return 0;
}

static const struct snd_kcontrol_new vts_ctrl_controls[] = {
	SOC_SINGLE("PERIOD DATA2REQ", VTS_DMIC_ENABLE_DMIC_IF,
		VTS_DMIC_PERIOD_DATA2REQ_OFFSET, 3, 0),
	SOC_SINGLE("HPF EN", VTS_DMIC_CONTROL_DMIC_IF,
		VTS_DMIC_HPF_EN_OFFSET, 1, 0),
	SOC_ENUM("HPF SEL", vts_hpf_sel),
	SOC_ENUM("CPS SEL", vts_cps_sel),
	SOC_SINGLE_TLV("GAIN", VTS_DMIC_CONTROL_DMIC_IF,
		VTS_DMIC_GAIN_OFFSET, 4, 0, vts_gain_tlv_array),
	SOC_SINGLE("1DB GAIN", VTS_DMIC_CONTROL_DMIC_IF,
		VTS_DMIC_1DB_GAIN_OFFSET, 5, 0),
	SOC_ENUM_EXT("SYS SEL", vts_sys_sel, snd_soc_get_enum_double,
		vts_ctrl_sys_sel_put_enum),
	SOC_ENUM("POLARITY CLK", vts_polarity_clk),
	SOC_ENUM("POLARITY OUTPUT", vts_polarity_output),
	SOC_ENUM("POLARITY INPUT", vts_polarity_input),
	SOC_ENUM("OVFW CTRL", vts_ovfw_ctrl),
	SOC_ENUM("CIC SEL", vts_cic_sel),
	SOC_ENUM_EXT("VoiceRecognization Mode",
		vtsvcrecog_mode_enum,
		vts_ctrl_get_vtsvoicerecognize_mode,
		vts_ctrl_set_vtsvoicerecognize_mode),
	SOC_ENUM_EXT("Active Keyphrase",
		vtsactive_phrase_enum,
		vts_ctrl_get_vtsactive_phrase,
		vts_ctrl_set_vtsactive_phrase),
	SOC_SINGLE_EXT("VoiceTrigger Value",
		SND_SOC_NOPM,
		0, 2000, 0,
		vts_ctrl_get_voicetrigger_value,
		vts_ctrl_set_voicetrigger_value),
	SOC_ENUM_EXT("Force Reset",
		vtsforce_reset_enum,
		vts_ctrl_get_vtsforce_reset,
		vts_ctrl_set_vtsforce_reset),
	SOC_SINGLE_EXT("Force Trigger",
		SND_SOC_NOPM,
		0, 1000, 0,
		vts_ctrl_get_force_trigger,
		vts_ctrl_set_force_trigger),
	SOC_SINGLE_EXT("Supported Mic Num",
		SND_SOC_NOPM,
		0, 3, 0,
		vts_ctrl_get_supported_mic_num,
		vts_ctrl_set_supported_mic_num),
	SOC_SINGLE_EXT("Force Error",
		SND_SOC_NOPM,
		0, 100, 0,
		vts_ctrl_get_force_error,
		vts_ctrl_set_force_error),
	SOC_SINGLE_EXT("SYSCLK DIV",
		SND_SOC_NOPM,
		0, 10, 0,
		vts_ctrl_get_sysclk_div,
		vts_ctrl_set_sysclk_div),
	SOC_SINGLE_EXT("RESET SUPPORT",
		SND_SOC_NOPM,
		0, 10, 0,
		vts_ctrl_get_reset_support,
		vts_ctrl_set_reset_support),
};

static int vts_ctrl_vts_dmic_sel_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct vts_data *data = vts_get_vts_data();
	unsigned int dmic_sel;

	dmic_sel = ucontrol->value.enumerated.item[0];
	if (dmic_sel > 1)
		return -EINVAL;

	vts_info("[VTS]%s : VTS DMIC SEL: %d\n", __func__, dmic_sel);

	data->dmic_if = dmic_sel;

	return  0;
}

static const char * const dmic_sel_texts[] = {"DPDM", "APDM"};
static SOC_ENUM_SINGLE_EXT_DECL(dmic_sel_enum, dmic_sel_texts);

static const struct snd_kcontrol_new dmic_sel_controls[] = {
	SOC_DAPM_ENUM_EXT("MUX", dmic_sel_enum,
			snd_soc_dapm_get_enum_double,
			vts_ctrl_vts_dmic_sel_put),
};

static const struct snd_kcontrol_new dmic_if_controls[] = {
	SOC_DAPM_SINGLE("RCH EN", VTS_DMIC_CONTROL_DMIC_IF,
		VTS_DMIC_RCH_EN_OFFSET, 1, 0),
	SOC_DAPM_SINGLE("LCH EN", VTS_DMIC_CONTROL_DMIC_IF,
		VTS_DMIC_LCH_EN_OFFSET, 1, 0),
};

static const struct snd_soc_dapm_widget vts_ctrl_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("PAD APDM"),
	SND_SOC_DAPM_INPUT("PAD DPDM"),
	SND_SOC_DAPM_MUX("DMIC SEL", SND_SOC_NOPM, 0, 0, dmic_sel_controls),
	SOC_MIXER_ARRAY("DMIC IF", SND_SOC_NOPM, 0, 0, dmic_if_controls),
};

static const struct snd_soc_dapm_route vts_ctrl_dapm_routes[] = {
	// sink, control, source
	{"DMIC SEL", "APDM", "PAD APDM"},
	{"DMIC SEL", "DPDM", "PAD DPDM"},
	{"DMIC IF", "RCH EN", "DMIC SEL"},
	{"DMIC IF", "LCH EN", "DMIC SEL"},
	{"VTS Capture", NULL, "DMIC IF"},
};

static int vts_ctrl_component_probe(struct snd_soc_component *cmpnt)
{
	struct device *dev = cmpnt->dev;
	struct vts_data *data = dev_get_drvdata(dev);
	int ret = 0;

	vts_dev_info(dev, "%s\n", __func__);
	data->cmpnt = cmpnt;

	ret = vts_soc_cmpnt_probe(dev);
	if (ret) {
		vts_dev_err(dev, "vts_soc_cmpnt_probe err(%d)\n", ret);

		return ret;
	}

	vts_dev_info(dev, "%s(%d)\n", __func__, __LINE__);

	return ret;
}

static const struct snd_soc_component_driver vts_component = {
	.probe = vts_ctrl_component_probe,
	.controls = vts_ctrl_controls,
	.num_controls = ARRAY_SIZE(vts_ctrl_controls),
	.dapm_widgets = vts_ctrl_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(vts_ctrl_dapm_widgets),
	.dapm_routes = vts_ctrl_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(vts_ctrl_dapm_routes),
};

static struct snd_soc_dai_driver vts_dai[] = {
	{
		.name = "vts-tri",
		.id = 0,
		.capture = {
			.stream_name = "VTS Trigger Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16,
			.sig_bits = 16,
		 },
	},
	{
		.name = "vts-rec",
		.id = 1,
		.capture = {
			.stream_name = "VTS Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16,
			.sig_bits = 16,
		 },
	},
	{
		.name = "vts-int",
		.id = 0,
		.capture = {
			.stream_name = "Internal Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16,
			.sig_bits = 16,
		 },
	},
};

int vts_ctrl_register(struct device *dev)
{
	int ret = 0;

	ret = snd_soc_register_component(dev,
		&vts_component, vts_dai, ARRAY_SIZE(vts_dai));
	if (ret < 0) {
		vts_dev_err(dev, "Failed to register ASoC component\n");
		return ret;
	}

	return ret;
}

static void hw_param_interval_set(struct snd_pcm_hw_params *params,
		snd_pcm_hw_param_t param, unsigned int val)
{
	struct snd_interval *interval = hw_param_interval(params, param);

	snd_interval_none(interval);
	if ((int)val >= 0) {
		interval->empty = 0;
		interval->min = interval->max = val;
		interval->openmin = interval->openmax = 0;
		interval->integer = 1;
	}
}

int vts_hw_params_fixup_helper(struct snd_soc_pcm_runtime *rtd, struct snd_pcm_hw_params *params)
{
	struct snd_soc_dai *dai = asoc_rtd_to_cpu(rtd, 0);
	struct device *dev = dai->dev;
	unsigned int rate, channels, width, pwidth, period_size, period_count, buffer_size;
	snd_pcm_format_t format;
	int ret = 0;

	vts_dev_dbg(dev, "%s(%s)\n", __func__, dai->name);

	/* ToDo: remove hard-coding if needed */
	rate = 16000;
	channels = 1;
	width = 16;
	period_size = 160;
	period_count = 1024;
	buffer_size = period_size * period_count;

	if (rate)
		hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE)->min = rate;

	if (channels)
		hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS)->min = channels;

	switch (width) {
	case 16:
		format = SNDRV_PCM_FORMAT_S16;
		break;
	case 24:
		format = SNDRV_PCM_FORMAT_S24;
		break;
	case 32:
		format = SNDRV_PCM_FORMAT_S32;
		break;
	default:
		format = 0;
		break;
	}
	if (format) {
		struct snd_mask *mask;

		mask = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);
		snd_mask_none(mask);
		snd_mask_set(mask, format);
	}

	if (period_size || period_count) {
		pwidth = snd_pcm_format_physical_width(format);
		hw_param_interval_set(params, SNDRV_PCM_HW_PARAM_PERIOD_TIME,
				USEC_PER_SEC * period_size / rate);
		hw_param_interval_set(params, SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
				period_size);
		hw_param_interval_set(params, SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
				period_size * (pwidth / 8) * channels);
		hw_param_interval_set(params, SNDRV_PCM_HW_PARAM_PERIODS,
				period_count);
		hw_param_interval_set(params, SNDRV_PCM_HW_PARAM_BUFFER_TIME,
				USEC_PER_SEC * buffer_size / rate);
		hw_param_interval_set(params, SNDRV_PCM_HW_PARAM_BUFFER_SIZE,
				buffer_size);
		hw_param_interval_set(params, SNDRV_PCM_HW_PARAM_BUFFER_BYTES,
				buffer_size * (pwidth / 8) * channels);
	}

	if (rate || channels || format || period_size || period_count)
		vts_dev_info(dev, "%s: %d bit, %u ch, %uHz, %uframes %uperiods\n", dai->name,
				width, channels, rate, period_size, period_count);

	return ret;
}
EXPORT_SYMBOL(vts_hw_params_fixup_helper);
