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

#ifndef VL53L5_OUTPUT_TARGET_FILTER__SET_CFG_MAIN_H_
#define VL53L5_OUTPUT_TARGET_FILTER__SET_CFG_MAIN_H_

#include "vl53l5_types.h"
#include "vl53l5_results_config.h"
#include "vl53l5_otf_structs.h"
#include "vl53l5_otf_cfg_map.h"
#include "vl53l5_otf_err_map.h"

void vl53l5_output_target_filter__set_cfg__default(
	uint8_t                                 otf_mode,
	uint32_t                                otf_range_clip_en,
	uint32_t                                otf_max_targets_per_zone,
	struct vl53l5_otf__general__cfg_t      *pgen_cfg,
	struct vl53l5_otf__tgt_status__cfg_t   *ptgt_status_list_valid,
	struct vl53l5_otf__tgt_status__cfg_t   *ptgt_status_list_blurred);

void vl53l5_output_target_filter__set_cfg__list_aff(
	struct vl53l5_otf__tgt_status__cfg_t   *ptgt_status_list);

void vl53l5_output_target_filter__set_cfg__list_valid(
	struct vl53l5_otf__tgt_status__cfg_t   *ptgt_status_list);

void vl53l5_output_target_filter__set_cfg__list_blurred(
	struct vl53l5_otf__tgt_status__cfg_t   *ptgt_status_list);

void vl53l5_output_target_filter__set_cfg_instance(
	uint32_t                                otf_cfg_preset,
	uint32_t                                otf_range_clip_en,
	uint32_t                                otf_max_targets_per_zone,
	struct vl53l5_otf__general__cfg_t      *pgen_cfg,
	struct vl53l5_otf__tgt_status__cfg_t   *ptgt_status_list_valid,
	struct vl53l5_otf__tgt_status__cfg_t   *ptgt_status_list_blurred,
	struct vl53l5_otf_err_dev_t            *perr_dev);

void vl53l5_output_target_filter__set_cfg_main(
	uint32_t                       otf_cfg_preset_0,
	uint32_t                       otf_cfg_preset_1,
	uint32_t                       otf_range_clip_en_0,
	uint32_t                       otf_range_clip_en_1,
	uint32_t                       otf_max_targets_per_zone_0,
	uint32_t                       otf_max_targets_per_zone_1,
	struct vl53l5_otf_cfg_dev_t   *pcfg_dev,
	struct vl53l5_otf_err_dev_t   *perr_dev);

#endif
