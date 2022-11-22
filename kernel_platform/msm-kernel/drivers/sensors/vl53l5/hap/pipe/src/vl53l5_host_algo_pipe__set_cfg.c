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

#include "vl53l5_host_algo_pipe__set_cfg.h"
#include "vl53l5_host_algo_pipe_funcs.h"
#ifdef VL53L5_ALGO_LONG_TAIL_FILTER_ON
#include "vl53l5_ltf_err_map.h"
#include "vl53l5_long_tail_filter__set_cfg_main.h"
#endif
#ifdef VL53L5_ALGO_GLASS_DETECTION_ON
#include "vl53l5_gd_err_map.h"
#include "vl53l5_glass_detection__set_cfg_main.h"
#endif
#ifdef VL53L5_ALGO_RANGE_ROTATION_ON
#include "vl53l5_rr_err_map.h"
#include "vl53l5_range_rotation__set_cfg_main.h"
#endif
#ifdef VL53L5_ALGO_ANTI_FLICKER_FILTER_ON
#include "vl53l5_aff_err_map.h"
#include "vl53l5_anti_flicker_filter__set_cfg_main.h"
#endif
#ifdef VL53L5_ALGO_OUTPUT_TARGET_SORT_ON
#include "vl53l5_ots_err_map.h"
#include "vl53l5_output_target_sort__set_cfg_main.h"
#endif
#include "vl53l5_otf_err_map.h"
#include "vl53l5_output_target_filter__set_cfg_main.h"
#if defined(VL53L5_ALGO_DEPTH16_ON) || defined(VL53L5_ALGO_DEPTH16_BUFFER_ON)
#include "dci_structs.h"
#include "vl53l5_d16_err_map.h"
#include "vl53l5_depth16__set_cfg_main.h"
#endif

void vl53l5_host_algo_pipe__set_general_cfg(
	uint8_t                                 hap__mode,
	uint8_t                                 aff_processing_mode,
	struct vl53l5_hap__general__cfg_t      *pgen_cfg,
	struct vl53l5_hap_err_dev_t            *perr_dev)
{
	struct  common_grp__status_t  *perr_status =
		&(perr_dev->hap_error_status);

	perr_status->status__group    = VL53L5_HOST_ALGO_PIPE_GROUP_ID;
	perr_status->status__stage_id = VL53L5_HAP_STAGE_ID__SET_GENERAL_CFG;
	perr_status->status__type     = VL53L5_HAP_ERROR_NONE;
	perr_status->status__debug_0 = 0U;
	perr_status->status__debug_1 = 0U;
	perr_status->status__debug_2 = 0U;

	memset(pgen_cfg, 0, sizeof(struct vl53l5_hap__general__cfg_t));

	switch (hap__mode) {
	case VL53L5_HAP_MODE__DISABLED:
	case VL53L5_HAP_MODE__ENABLED:
		pgen_cfg->hap__mode = hap__mode;
		break;
	default:
		perr_status->status__type = VL53L5_HAP_ERROR_INVALID_PARAMS;
		break;
	}

	switch (aff_processing_mode) {
	case VL53L5_AFF_PROCESSING_MODE__DISABLED:
	case VL53L5_AFF_PROCESSING_MODE__NORMAL_ONLY:
	case VL53L5_AFF_PROCESSING_MODE__ASZ_ONLY:
	case VL53L5_AFF_PROCESSING_MODE__BOTH:
		pgen_cfg->aff_processing_mode = aff_processing_mode;
		break;
	default:
		perr_status->status__type = VL53L5_HAP_ERROR_INVALID_PARAMS;
		break;
	}
}

int32_t vl53l5_host_algo_pipe__set_cfg(
	struct vl53l5_hap_tuning_t         *ptuning,
	struct vl53l5_hap_cfg_dev_t        *pcfg_dev,
	struct vl53l5_hap_err_dev_t        *perr_dev)
{

	int32_t  status = 0;
	struct  common_grp__status_t *perr_status =
		&(perr_dev->hap_error_status);

	perr_status->status__group = VL53L5_HOST_ALGO_PIPE_GROUP_ID;
	perr_status->status__stage_id = VL53L5_HAP_STAGE_ID__SET_CFG;
	perr_status->status__type = VL53L5_HAP_ERROR_NONE;
	perr_status->status__debug_0 = 0U;
	perr_status->status__debug_1 = 0U;
	perr_status->status__debug_2 = 0U;

#ifndef VL53L5_ALGO_ANTI_FLICKER_FILTER_ON
	ptuning->aff_processing_mode =
		VL53L5_AFF_PROCESSING_MODE__DISABLED;
#endif

	if (perr_status->status__type == 0) {
		vl53l5_host_algo_pipe__set_general_cfg(
			(uint8_t)ptuning->hap__mode,
			(uint8_t)ptuning->aff_processing_mode,
			(&(pcfg_dev->hap_general_cfg)),
			perr_dev);
	}

#ifdef VL53L5_ALGO_LONG_TAIL_FILTER_ON
	if (perr_status->status__type == 0) {
		vl53l5_long_tail_filter__set_cfg_main(
			ptuning->ltf_cfg_preset,
			&(pcfg_dev->ltf_cfg_dev),
			(struct vl53l5_ltf_err_dev_t *)perr_dev);
	}
#endif

#ifdef VL53L5_ALGO_GLASS_DETECTION_ON
	if (perr_status->status__type == 0) {
		vl53l5_glass_detection__set_cfg_main(
			ptuning->gd_cfg_preset,
			&(pcfg_dev->gd_cfg_dev),
			(struct vl53l5_gd_err_dev_t *)perr_dev);
	}
#endif

#ifdef VL53L5_ALGO_RANGE_ROTATION_ON
	if (perr_status->status__type == 0) {
		vl53l5_range_rotation__set_cfg_main(
			ptuning->rr_cfg_preset,
			(uint8_t)ptuning->rr_rotation,
			&(pcfg_dev->rr_cfg_dev),
			(struct vl53l5_rr_err_dev_t *)perr_dev);
	}
#endif

#ifdef VL53L5_ALGO_ANTI_FLICKER_FILTER_ON
	if (perr_status->status__type == 0) {
		vl53l5_anti_flicker_filter__set_cfg_main(
			ptuning->aff_cfg_preset,
			&(pcfg_dev->aff_cfg_dev),
			(struct vl53l5_aff_err_dev_t *)perr_dev);
	}
#endif

#ifdef VL53L5_ALGO_OUTPUT_TARGET_SORT_ON
	if (perr_status->status__type == 0) {
		vl53l5_output_target_sort__set_cfg_main(
			ptuning->ots_cfg_preset,
			&(pcfg_dev->ots_cfg_dev),
			(struct vl53l5_ots_err_dev_t *)perr_dev);
	}
#endif

	if (perr_status->status__type == 0) {
		vl53l5_output_target_filter__set_cfg_main(
			ptuning->otf_cfg_preset_0,
			ptuning->otf_cfg_preset_1,
			ptuning->otf_range_clip_en_0,
			ptuning->otf_range_clip_en_1,
			ptuning->otf_max_targets_per_zone_0,
			ptuning->otf_max_targets_per_zone_1,
			&(pcfg_dev->otf_cfg_dev),
			(struct vl53l5_otf_err_dev_t *)perr_dev);
	}

#if defined(VL53L5_ALGO_DEPTH16_ON) || defined(VL53L5_ALGO_DEPTH16_BUFFER_ON)
	if (perr_status->status__type == 0) {
		vl53l5_depth16__set_cfg_main(
			ptuning->d16_cfg_preset,
			64U,
			(uint32_t)ptuning->otf_max_targets_per_zone_1,
			4U,
			&(pcfg_dev->d16_cfg_dev),
			(struct vl53l5_d16_err_dev_t *)perr_dev);
	}
#endif

	status =
		vl53l5_hap_encode_status_code(
			perr_status);

	return status;
}
