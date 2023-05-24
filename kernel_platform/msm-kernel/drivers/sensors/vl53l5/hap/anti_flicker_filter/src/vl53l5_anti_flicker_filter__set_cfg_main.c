/*******************************************************************************
* Copyright (c) 2020, STMicroelectronics - All Rights Reserved
*
* This file is part of VL53L5 Algo Pipe and is dual licensed,
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
* Alternatively, VL53L5 Algo Pipe may be distributed under the terms of
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

#include "vl53l5_platform_algo_macros.h"
#include "vl53l5_platform_algo_limits.h"
#include "vl53l5_results_build_config.h"
#include "vl53l5_aff_defs.h"
#include "vl53l5_aff_luts.h"
#include "vl53l5_anti_flicker_filter__set_cfg_main.h"

void vl53l5_anti_flicker_filter__set_cfg__default(
	uint8_t                       aff__mode,
	struct vl53l5_aff_cfg_dev_t  *pdev)
{

	uint32_t i;

	(pdev->aff_cfg).aff__affinity_param0                     = (int16_t)(1500);
	(pdev->aff_cfg).aff__affinity_param1                     = (int16_t)(1);
	(pdev->aff_cfg).aff__non_matched_history_tweak_param     = (int16_t)(256);
	(pdev->aff_cfg).aff__missing_cur_zscore_param            = (int16_t)(0);
	(pdev->aff_cfg).aff__out_sort_param                      = (int16_t)(6554);
	(pdev->aff_cfg).aff__zscore_missing_tgt                  = (uint16_t)(163);
	(pdev->aff_cfg).aff__min_zscore_for_in                   = (uint16_t)(195);
	(pdev->aff_cfg).aff__min_zscore_for_out                  = (uint16_t)(195);
	(pdev->aff_cfg).aff__discard_threshold                   = (uint16_t)(400);
	(pdev->aff_cfg).aff__nb_of_past_instants                 = (uint8_t)(4);
	(pdev->aff_cfg).aff__max_nb_of_targets_to_keep_per_zone  = (uint8_t)(4);
	(pdev->aff_cfg).aff__mode                                = aff__mode;
	(pdev->aff_cfg).aff__discard_close_tgt                   = (uint8_t)(1);
	(pdev->aff_cfg).aff__nb_zones_to_filter                  =
		(uint8_t)VL53L5_AFF_DEF__MAX_ZONE_SEL;
	(pdev->aff_cfg).aff__affinity_mode                       =
		(uint8_t)VL53L5_AFF_AFFINITY_OPTION__RANGE_ONLY;
	(pdev->aff_cfg).aff__non_matched_history_tweak_mode      = (uint8_t)(0);

	(pdev->aff_cfg).aff__missing_cur_zscore_mode             = (uint8_t)(59);
	(pdev->aff_cfg).aff__out_sort_mode                       =
		(uint8_t)VL53L5_AFF_OUT_SORT_OPTION__ZSCORE_AND_PREV_BEST;
	(pdev->aff_cfg).aff__spare0                              = (uint8_t)(205);

	for (i = 0; i < VL53L5_AFF_DEF__MAX_NB_OF_TEMPORAL_WEIGHTS; i++)
		(pdev->aff_arrayed_cfg).aff__temporal_weights[i]     = (uint8_t)(0);

	(pdev->aff_arrayed_cfg).aff__temporal_weights[0]         = (uint8_t)(5 << 4);
	(pdev->aff_arrayed_cfg).aff__temporal_weights[1]         = (uint8_t)(4 << 4);
	(pdev->aff_arrayed_cfg).aff__temporal_weights[2]         = (uint8_t)(4 << 4);
	(pdev->aff_arrayed_cfg).aff__temporal_weights[3]         = (uint8_t)(3 << 4);

	for (i = 0; i < VL53L5_AFF_DEF__MAX_ZONE_SEL; i++)
		(pdev->aff_arrayed_cfg).aff__zone_sel[i]             = (uint8_t)(i);
}

void vl53l5_anti_flicker_filter__set_cfg__tuning_0(
	uint8_t                       aff__mode,
	struct vl53l5_aff_cfg_dev_t  *pdev)
{

	vl53l5_anti_flicker_filter__set_cfg__default(
		aff__mode,
		pdev);
}

void vl53l5_anti_flicker_filter__set_cfg__tuning_1(
	uint8_t                       aff__mode,
	struct vl53l5_aff_cfg_dev_t  *pdev)
{

	vl53l5_anti_flicker_filter__set_cfg__default(
		aff__mode,
		pdev);

	(pdev->aff_cfg).aff__missing_cur_zscore_mode = (uint8_t)(63);
}

void vl53l5_anti_flicker_filter__set_cfg__tuning_2(
	uint8_t                       aff__mode,
	struct vl53l5_aff_cfg_dev_t  *pdev)
{

	vl53l5_anti_flicker_filter__set_cfg__default(
		aff__mode,
		pdev);

	(pdev->aff_cfg).aff__affinity_param0 = (int16_t)(800);
}

void vl53l5_anti_flicker_filter__set_cfg__tuning_3(
	uint8_t                       aff__mode,
	struct vl53l5_aff_cfg_dev_t  *pdev)
{

	vl53l5_anti_flicker_filter__set_cfg__default(
		aff__mode,
		pdev);

	(pdev->aff_cfg).aff__affinity_param0         = (int16_t)(800);
	(pdev->aff_cfg).aff__missing_cur_zscore_mode = (uint8_t)(63);
}

void vl53l5_anti_flicker_filter__set_cfg__tuning_4(
	uint8_t                       aff__mode,
	struct vl53l5_aff_cfg_dev_t  *pdev)
{

	vl53l5_anti_flicker_filter__set_cfg__default(
		aff__mode,
		pdev);

	(pdev->aff_cfg).aff__spare0 = (uint8_t)(128);
}

void vl53l5_anti_flicker_filter__set_cfg__tuning_5(
	uint8_t                       aff__mode,
	struct vl53l5_aff_cfg_dev_t  *pdev)
{

	vl53l5_anti_flicker_filter__set_cfg__default(
		aff__mode,
		pdev);

	(pdev->aff_cfg).aff__missing_cur_zscore_mode = (uint8_t)(63);
	(pdev->aff_cfg).aff__spare0                  = (uint8_t)(128);
}

void vl53l5_anti_flicker_filter__set_cfg__tuning_6(
	uint8_t                       aff__mode,
	struct vl53l5_aff_cfg_dev_t  *pdev)
{

	vl53l5_anti_flicker_filter__set_cfg__default(
		aff__mode,
		pdev);

	(pdev->aff_cfg).aff__affinity_param0 = (int16_t)(800);
	(pdev->aff_cfg).aff__spare0          = (uint8_t)(128);
}

void vl53l5_anti_flicker_filter__set_cfg__tuning_7(
	uint8_t                       aff__mode,
	struct vl53l5_aff_cfg_dev_t  *pdev)
{

	vl53l5_anti_flicker_filter__set_cfg__default(
		aff__mode,
		pdev);

	(pdev->aff_cfg).aff__affinity_param0         = (int16_t)(800);
	(pdev->aff_cfg).aff__missing_cur_zscore_mode = (uint8_t)(63);
	(pdev->aff_cfg).aff__spare0                  = (uint8_t)(128);
}

void vl53l5_anti_flicker_filter__set_cfg_main(
	uint32_t                      aff_cfg_preset,
	struct vl53l5_aff_cfg_dev_t  *pcfg_dev,
	struct vl53l5_aff_err_dev_t  *perr_dev)
{

	struct  common_grp__status_t  *pstatus = &(perr_dev->aff_error_status);

	pstatus->status__group     = VL53L5_ALGO_ANTI_FLICKER_FILTER_GROUP_ID;
	pstatus->status__stage_id  = VL53L5_AFF_STAGE_ID__SET_CFG;
	pstatus->status__type      = VL53L5_AFF_ERROR__NONE;
	pstatus->status__debug_0   = 0U;
	pstatus->status__debug_1   = 0U;
	pstatus->status__debug_2   = 0U;

	switch (aff_cfg_preset) {
	case VL53L5_AFF_CFG_PRESET__NONE:
		vl53l5_anti_flicker_filter__set_cfg__default(
			VL53L5_AFF_MODE__DISABLED,
			pcfg_dev);
		break;
	case VL53L5_AFF_CFG_PRESET__TUNING_0:
		vl53l5_anti_flicker_filter__set_cfg__tuning_0(
			VL53L5_AFF_MODE__ENABLED,
			pcfg_dev);
		break;
	case VL53L5_AFF_CFG_PRESET__TUNING_1:
		vl53l5_anti_flicker_filter__set_cfg__tuning_1(
			VL53L5_AFF_MODE__ENABLED,
			pcfg_dev);
		break;
	case VL53L5_AFF_CFG_PRESET__TUNING_2:
		vl53l5_anti_flicker_filter__set_cfg__tuning_2(
			VL53L5_AFF_MODE__ENABLED,
			pcfg_dev);
		break;
	case VL53L5_AFF_CFG_PRESET__TUNING_3:
		vl53l5_anti_flicker_filter__set_cfg__tuning_3(
			VL53L5_AFF_MODE__ENABLED,
			pcfg_dev);
		break;
	case VL53L5_AFF_CFG_PRESET__TUNING_4:
		vl53l5_anti_flicker_filter__set_cfg__tuning_4(
			VL53L5_AFF_MODE__ENABLED,
			pcfg_dev);
		break;
	case VL53L5_AFF_CFG_PRESET__TUNING_5:
		vl53l5_anti_flicker_filter__set_cfg__tuning_5(
			VL53L5_AFF_MODE__ENABLED,
			pcfg_dev);
		break;
	case VL53L5_AFF_CFG_PRESET__TUNING_6:
		vl53l5_anti_flicker_filter__set_cfg__tuning_6(
			VL53L5_AFF_MODE__ENABLED,
			pcfg_dev);
		break;
	case VL53L5_AFF_CFG_PRESET__TUNING_7:
		vl53l5_anti_flicker_filter__set_cfg__tuning_7(
			VL53L5_AFF_MODE__ENABLED,
			pcfg_dev);
		break;
	default:
		pstatus->status__type =
			VL53L5_AFF_ERROR__INVALID_CFG_MODE;
		break;
	}
}
