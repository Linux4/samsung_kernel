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
#include "vl53l5_gd_defs.h"
#include "vl53l5_gd_luts.h"
#include "vl53l5_glass_detection__set_cfg_main.h"

void vl53l5_glass_detection__set_cfg__default(
	uint8_t                       gd__mode,
	struct vl53l5_gd_cfg_dev_t   *pcfg_dev)
{

	uint32_t i = 0U;

	pcfg_dev->gd_general_cfg.gd__min_rate_ratio = 40U *
			VL53L5_GD_DEF__RATE_RATIO_UNITY_VALUE;

	pcfg_dev->gd_general_cfg.gd__max_dist_from_plane_mm = 100;
	pcfg_dev->gd_general_cfg.gd__min_tgt_behind_glass_dist_mm = 100;
	pcfg_dev->gd_general_cfg.gd__max_dist_from_plane_mirror_mm = 100;
	pcfg_dev->gd_general_cfg.gd__rate_ratio_lower_confidence_glass = 10;
	pcfg_dev->gd_general_cfg.gd__rate_ratio_upper_confidence_glass = 50;
	pcfg_dev->gd_general_cfg.gd__rate_ratio_lower_confidence_tgt_merged = 10;
	pcfg_dev->gd_general_cfg.gd__rate_ratio_upper_confidence_tgt_merged = 50;
	pcfg_dev->gd_general_cfg.gd__rate_ratio_lower_confidence_mirror = 10;
	pcfg_dev->gd_general_cfg.gd__rate_ratio_upper_confidence_mirror = 50;
	pcfg_dev->gd_general_cfg.gd__mode = gd__mode;
	pcfg_dev->gd_general_cfg.gd__min_valid_zones = 8U;

	pcfg_dev->gd_general_cfg.gd__min_tgt_behind_glass_merged_zones = 100U;
	pcfg_dev->gd_general_cfg.gd__min_tgt2_behind_mirror_zones = 7U;
	pcfg_dev->gd_general_cfg.gd__min_points_on_plane_pc = 65U;

	pcfg_dev->gd_general_cfg.gd__exclude_outer__glass_merged = 0U;
	pcfg_dev->gd_general_cfg.gd__exclude_outer__glass_only = 0U;
	pcfg_dev->gd_general_cfg.gd__exclude_outer__mirror = 0U;
	pcfg_dev->gd_general_cfg.gd__glass_merged__rate_subtraction_pc = 80U;
	pcfg_dev->gd_general_cfg.gd__merged_target_min_radius = 3U;
	pcfg_dev->gd_general_cfg.gd__mirror_min_radius = 2U;

	pcfg_dev->gd_general_cfg.gd__cfg_spare_0 = 0U;
	pcfg_dev->gd_general_cfg.gd__cfg_spare_1 = 0U;
	pcfg_dev->gd_general_cfg.gd__cfg_spare_2 = 0U;

	for (i = 0; i < VL53L5_GD_DEF__MAX_TGT_STATUS_LIST_SIZE; i++) {
		pcfg_dev->gd_tgt_status_list_0.gd__tgt_status[i] = 0;
		pcfg_dev->gd_tgt_status_list_1.gd__tgt_status[i] = 0;
		pcfg_dev->gd_tgt_status_list_2.gd__tgt_status[i] = 0;
	}

	pcfg_dev->gd_tgt_status_list_0.gd__tgt_status[0] =
		DCI_TARGET_STATUS__RANGECOMPLETE;
	pcfg_dev->gd_tgt_status_list_0.gd__tgt_status[1] =
		DCI_TARGET_STATUS__RANGECOMPLETE_NO_WRAP_CHECK;
	pcfg_dev->gd_tgt_status_list_0.gd__tgt_status[2] =
		DCI_TARGET_STATUS__RANGECOMPLETE_MERGED_PULSE;
	pcfg_dev->gd_tgt_status_list_0.gd__tgt_status[3] =
		DCI_TARGET_STATUS__TARGETDUETOBLUR;
	pcfg_dev->gd_tgt_status_list_0.gd__tgt_status[4] =
		DCI_TARGET_STATUS__TARGETDUETOBLUR_NO_WRAP_CHECK;
	pcfg_dev->gd_tgt_status_list_0.gd__tgt_status[5] =
		DCI_TARGET_STATUS__TARGETDUETOBLUR_MERGED_PULSE;

	pcfg_dev->gd_tgt_status_list_1.gd__tgt_status[0] =
		DCI_TARGET_STATUS__RANGECOMPLETE;
	pcfg_dev->gd_tgt_status_list_1.gd__tgt_status[1] =
		DCI_TARGET_STATUS__RANGECOMPLETE_NO_WRAP_CHECK;
	pcfg_dev->gd_tgt_status_list_1.gd__tgt_status[2] =
		DCI_TARGET_STATUS__RANGECOMPLETE_MERGED_PULSE;
	pcfg_dev->gd_tgt_status_list_1.gd__tgt_status[3] =
		DCI_TARGET_STATUS__TARGETDUETOBLUR;
	pcfg_dev->gd_tgt_status_list_1.gd__tgt_status[4] =
		DCI_TARGET_STATUS__TARGETDUETOBLUR_NO_WRAP_CHECK;
	pcfg_dev->gd_tgt_status_list_1.gd__tgt_status[5] =
		DCI_TARGET_STATUS__TARGETDUETOBLUR_MERGED_PULSE;

	pcfg_dev->gd_tgt_status_list_2.gd__tgt_status[0] =
		DCI_TARGET_STATUS__RANGECOMPLETE;
	pcfg_dev->gd_tgt_status_list_2.gd__tgt_status[1] =
		DCI_TARGET_STATUS__RANGECOMPLETE_NO_WRAP_CHECK;
	pcfg_dev->gd_tgt_status_list_2.gd__tgt_status[2] =
		DCI_TARGET_STATUS__RANGECOMPLETE_MERGED_PULSE;
	pcfg_dev->gd_tgt_status_list_2.gd__tgt_status[3] =
		DCI_TARGET_STATUS__TARGETDUETOBLUR;
	pcfg_dev->gd_tgt_status_list_2.gd__tgt_status[4] =
		DCI_TARGET_STATUS__TARGETDUETOBLUR_NO_WRAP_CHECK;
	pcfg_dev->gd_tgt_status_list_2.gd__tgt_status[5] =
		DCI_TARGET_STATUS__TARGETDUETOBLUR_MERGED_PULSE;
}

void vl53l5_glass_detection__set_cfg__tuning_1(
	uint8_t                       gd__mode,
	struct vl53l5_gd_cfg_dev_t   *pcfg_dev)
{

	vl53l5_glass_detection__set_cfg__default(
		gd__mode,
		pcfg_dev);

	pcfg_dev->gd_general_cfg.gd__min_rate_ratio = 100U *
			VL53L5_GD_DEF__RATE_RATIO_UNITY_VALUE;

	pcfg_dev->gd_general_cfg.gd__rate_ratio_upper_confidence_glass = 120;
	pcfg_dev->gd_general_cfg.gd__rate_ratio_upper_confidence_tgt_merged = 120;
	pcfg_dev->gd_general_cfg.gd__rate_ratio_upper_confidence_mirror = 120;

	pcfg_dev->gd_general_cfg.gd__min_valid_zones = 10U;
	pcfg_dev->gd_general_cfg.gd__min_tgt_behind_glass_merged_zones = 4U;
	pcfg_dev->gd_general_cfg.gd__exclude_outer__glass_only = 1U;

}

void vl53l5_glass_detection__set_cfg__tuning_2(
	uint8_t                       gd__mode,
	struct vl53l5_gd_cfg_dev_t   *pcfg_dev)
{

	vl53l5_glass_detection__set_cfg__tuning_1(
		gd__mode,
		pcfg_dev);

	pcfg_dev->gd_general_cfg.gd__min_tgt_behind_glass_merged_zones = 100U;
}

void vl53l5_glass_detection__set_cfg_main(
	uint32_t                      gd_cfg_preset,
	struct vl53l5_gd_cfg_dev_t   *pcfg_dev,
	struct vl53l5_gd_err_dev_t   *perr_dev)
{

	struct  common_grp__status_t  *pstatus = &(perr_dev->gd_error_status);

	pstatus->status__group = VL53L5_ALGO_GLASS_DETECTION_GROUP_ID;
	pstatus->status__stage_id = VL53L5_GD_STAGE_ID__SET_CFG;
	pstatus->status__type = VL53L5_GD_ERROR_NONE;
	pstatus->status__debug_0 = 0U;
	pstatus->status__debug_1 = 0U;
	pstatus->status__debug_2 = 0U;

	switch (gd_cfg_preset) {
	case VL53L5_GD_CFG_PRESET__NONE:
		vl53l5_glass_detection__set_cfg__default(
			VL53L5_GD_MODE__DISABLED,
			pcfg_dev);
		break;
	case VL53L5_GD_CFG_PRESET__TUNING_0:
		vl53l5_glass_detection__set_cfg__default(
			VL53L5_GD_MODE__ENABLED,
			pcfg_dev);
		break;
	case VL53L5_GD_CFG_PRESET__TUNING_1:
		vl53l5_glass_detection__set_cfg__tuning_1(
			VL53L5_GD_MODE__ENABLED,
			pcfg_dev);
		break;
	case VL53L5_GD_CFG_PRESET__TUNING_2:
		vl53l5_glass_detection__set_cfg__tuning_2(
			VL53L5_GD_MODE__ENABLED,
			pcfg_dev);
		break;
	default:
		pstatus->status__type =
			VL53L5_GD_ERROR_INVALID_PARAMS;
		break;
	}
}
