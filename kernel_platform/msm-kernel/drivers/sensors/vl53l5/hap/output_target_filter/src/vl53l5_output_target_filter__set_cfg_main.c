/*******************************************************************************
* Copyright (c) 2021, STMicroelectronics - All Rights Reserved
*
* This file is part of VL53L5 Kernel Driver and is dual licensed,
* either 'STMicroelectronics Proprietary license'
* or 'BSD 3-clause "New" or "Revised" License' , at your option.
*
********************************************************************************
*
* 'STMicroelectronics Proprietary license'
*
********************************************************************************
*
* License terms: STMicroelectronics Proprietary in accordance with licensing
* terms at www.st.com/sla0081
*
* STMicroelectronics confidential
* Reproduction and Communication of this document is strictly prohibited unless
* specifically authorized in writing by STMicroelectronics.
*
*
********************************************************************************
*
* Alternatively, VL53L5 Kernel Driver may be distributed under the terms of
* 'BSD 3-clause "New" or "Revised" License', in which case the following
* provisions apply instead of the ones mentioned above :
*
********************************************************************************
*
* License terms: BSD 3-clause "New" or "Revised" License.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice, this
* list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its contributors
* may be used to endorse or promote products derived from this software
* without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*
*******************************************************************************/

#include "dci_defs.h"
#include "dci_luts.h"
#include "vl53l5_otf_defs.h"
#include "vl53l5_otf_luts.h"
#include "vl53l5_output_target_filter__set_cfg_main.h"

void vl53l5_output_target_filter__set_cfg__default(
	uint8_t                                 otf_mode,
	uint32_t                                otf_range_clip_en,
	uint32_t                                otf_max_targets_per_zone,
	struct vl53l5_otf__general__cfg_t      *pgen_cfg,
	struct vl53l5_otf__tgt_status__cfg_t   *ptgt_status_list_valid,
	struct vl53l5_otf__tgt_status__cfg_t   *ptgt_status_list_blurred)
{

	uint32_t i = 0U;

	pgen_cfg->otf__mode                 = otf_mode;
	pgen_cfg->otf__range_clip_en        = (uint8_t)otf_range_clip_en;
	pgen_cfg->otf__max_targets_per_zone = (uint8_t)otf_max_targets_per_zone;
	pgen_cfg->otf__gen_cfg__spare_0     = 0U;

	for (i = 0; i < VL53L5_OTF_DEF__MAX_TGT_STATUS_LIST_SIZE; i++) {
		ptgt_status_list_valid->otf__tgt_status[i]   = 0;
		ptgt_status_list_blurred->otf__tgt_status[i] = 0;
	}
}

void vl53l5_output_target_filter__set_cfg__list_aff(
	struct vl53l5_otf__tgt_status__cfg_t   *ptgt_status_list)
{

	int32_t  n = 0;

	ptgt_status_list->otf__tgt_status[n] =
		DCI_TARGET_STATUS__PHASECONSISTENCY;
	n++;
	ptgt_status_list->otf__tgt_status[n] =
		DCI_TARGET_STATUS__RANGECOMPLETE;
	n++;
	ptgt_status_list->otf__tgt_status[n] =
		DCI_TARGET_STATUS__RANGECOMPLETE_NO_WRAP_CHECK;
	n++;
	ptgt_status_list->otf__tgt_status[n] =
		DCI_TARGET_STATUS__RANGECOMPLETE_MERGED_PULSE;
	n++;
	ptgt_status_list->otf__tgt_status[n] =
		DCI_TARGET_STATUS__PREV_RANGE_NO_TARGETS;
}

void vl53l5_output_target_filter__set_cfg__list_valid(
	struct vl53l5_otf__tgt_status__cfg_t   *ptgt_status_list)
{

	int32_t  n = 0;

	ptgt_status_list->otf__tgt_status[n] =
		DCI_TARGET_STATUS__RANGECOMPLETE;
	n++;
	ptgt_status_list->otf__tgt_status[n] =
		DCI_TARGET_STATUS__RANGECOMPLETE_NO_WRAP_CHECK;
	n++;
	ptgt_status_list->otf__tgt_status[n] =
		DCI_TARGET_STATUS__RANGECOMPLETE_MERGED_PULSE;
}

void vl53l5_output_target_filter__set_cfg__list_blurred(
	struct vl53l5_otf__tgt_status__cfg_t   *ptgt_status_list)
{

	int32_t  n = 0;

	ptgt_status_list->otf__tgt_status[n] =
		DCI_TARGET_STATUS__TARGETDUETOBLUR;
	n++;
	ptgt_status_list->otf__tgt_status[n] =
		DCI_TARGET_STATUS__TARGETDUETOBLUR_NO_WRAP_CHECK;
	n++;
	ptgt_status_list->otf__tgt_status[n] =
		DCI_TARGET_STATUS__TARGETDUETOBLUR_MERGED_PULSE;
}

void vl53l5_output_target_filter__set_cfg_instance(
	uint32_t                                otf_cfg_preset,
	uint32_t                                otf_range_clip_en,
	uint32_t                                otf_max_targets_per_zone,
	struct vl53l5_otf__general__cfg_t      *pgen_cfg,
	struct vl53l5_otf__tgt_status__cfg_t   *ptgt_status_list_valid,
	struct vl53l5_otf__tgt_status__cfg_t   *ptgt_status_list_blurred,
	struct vl53l5_otf_err_dev_t            *perr_dev)
{

	struct  common_grp__status_t  *pstatus = &(perr_dev->otf_error_status);

	pstatus->status__group = VL53L5_ALGO_OUTPUT_TARGET_FILTER_GROUP_ID;
	pstatus->status__stage_id = VL53L5_OTF_STAGE_ID__SET_CFG;
	pstatus->status__type = VL53L5_OTF_ERROR_NONE;
	pstatus->status__debug_0 = 0U;
	pstatus->status__debug_1 = 0U;
	pstatus->status__debug_2 = 0U;

	switch (otf_cfg_preset) {
	case VL53L5_OTF_CFG_PRESET__NONE:

		vl53l5_output_target_filter__set_cfg__default(
			VL53L5_OTF_MODE__DISABLED,
			otf_range_clip_en,
			otf_max_targets_per_zone,
			pgen_cfg,
			ptgt_status_list_valid,
			ptgt_status_list_blurred);
		vl53l5_output_target_filter__set_cfg__list_valid(
			ptgt_status_list_valid);
		break;

	case VL53L5_OTF_CFG_PRESET__TUNING_0:

		vl53l5_output_target_filter__set_cfg__default(
			VL53L5_OTF_MODE__ENABLED,
			otf_range_clip_en,
			otf_max_targets_per_zone,
			pgen_cfg,
			ptgt_status_list_valid,
			ptgt_status_list_blurred);
		vl53l5_output_target_filter__set_cfg__list_aff(
			ptgt_status_list_valid);
		break;

	case VL53L5_OTF_CFG_PRESET__TUNING_1:

		vl53l5_output_target_filter__set_cfg__default(
			VL53L5_OTF_MODE__ENABLED,
			otf_range_clip_en,
			otf_max_targets_per_zone,
			pgen_cfg,
			ptgt_status_list_valid,
			ptgt_status_list_blurred);
		vl53l5_output_target_filter__set_cfg__list_valid(
			ptgt_status_list_valid);
		break;

	case VL53L5_OTF_CFG_PRESET__TUNING_2:

		vl53l5_output_target_filter__set_cfg__default(
			VL53L5_OTF_MODE__ENABLED,
			otf_range_clip_en,
			otf_max_targets_per_zone,
			pgen_cfg,
			ptgt_status_list_valid,
			ptgt_status_list_blurred);
		vl53l5_output_target_filter__set_cfg__list_valid(
			ptgt_status_list_valid);
		vl53l5_output_target_filter__set_cfg__list_blurred(
			ptgt_status_list_blurred);
		break;

	default:
		pstatus->status__type =
			VL53L5_OTF_ERROR_INVALID_PARAMS;
		break;
	}
}

void vl53l5_output_target_filter__set_cfg_main(
	uint32_t                       otf_cfg_preset_0,
	uint32_t                       otf_cfg_preset_1,
	uint32_t                       otf_range_clip_en_0,
	uint32_t                       otf_range_clip_en_1,
	uint32_t                       otf_max_targets_per_zone_0,
	uint32_t                       otf_max_targets_per_zone_1,
	struct vl53l5_otf_cfg_dev_t   *pcfg_dev,
	struct vl53l5_otf_err_dev_t   *perr_dev)
{

	struct  common_grp__status_t  *pstatus = &(perr_dev->otf_error_status);

	pstatus->status__group = VL53L5_ALGO_OUTPUT_TARGET_FILTER_GROUP_ID;
	pstatus->status__stage_id = VL53L5_OTF_STAGE_ID__SET_CFG;
	pstatus->status__type = VL53L5_OTF_ERROR_NONE;
	pstatus->status__debug_0 = 0U;
	pstatus->status__debug_1 = 0U;
	pstatus->status__debug_2 = 0U;

	if (pstatus->status__type == 0) {
		vl53l5_output_target_filter__set_cfg_instance(
			otf_cfg_preset_0,
			otf_range_clip_en_0,
			otf_max_targets_per_zone_0,
			&(pcfg_dev->otf_general_cfg_0),
			&(pcfg_dev->otf_tgt_status_list_0v),
			&(pcfg_dev->otf_tgt_status_list_0b),
			perr_dev);
	}

	if (pstatus->status__type == 0) {
		vl53l5_output_target_filter__set_cfg_instance(
			otf_cfg_preset_1,
			otf_range_clip_en_1,
			otf_max_targets_per_zone_1,
			&(pcfg_dev->otf_general_cfg_1),
			&(pcfg_dev->otf_tgt_status_list_1v),
			&(pcfg_dev->otf_tgt_status_list_1b),
			perr_dev);
	}
}
