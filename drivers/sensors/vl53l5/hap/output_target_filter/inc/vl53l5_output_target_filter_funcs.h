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

#ifndef VL53L5_OUTPUT_TARGET_FILTER_FUNCS_H_
#define VL53L5_OUTPUT_TARGET_FILTER_FUNCS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "vl53l5_types.h"
#include "vl53l5_results_config.h"

#include "common_datatype_structs.h"
#include "dci_structs.h"
#include "vl53l5_results_map.h"
#include "dci_patch_op_map.h"

#include "vl53l5_otf_cfg_map.h"
#include "vl53l5_otf_int_map.h"
#include "vl53l5_otf_err_map.h"

void vl53l5_otf_clear_data(
	struct dci_patch_op_dev_t           *pop_dev,
	struct vl53l5_otf_err_dev_t          *perr_dev);

int32_t vl53l5_otf_is_target_valid(
	uint8_t                                  target_status,
	struct   vl53l5_otf__tgt_status__cfg_t  *ptgt_status_list);

void vl53l5_otf_init_tgt_idx_list(
	uint32_t                              init_value,
	struct  vl53l5_otf__tgt__idx_data_t  *ptgt_idx_data);

void vl53l5_otf_filter_targets(
	int32_t                                   zone_idx,
	struct   vl53l5_otf__tgt_status__cfg_t   *ptgt_status_list_valid,
	struct   vl53l5_otf__tgt_status__cfg_t   *ptgt_status_list_blurred,
	struct   vl53l5_otf_int_dev_t            *pint_dev,
	struct   vl53l5_range_results_t                    *presults,
	int32_t                                  *pop_no_of_targets);

void vl53l5_otf_copy_target(
	int32_t                                     zone_idx,
	int32_t                                     ip_target_idx,
	int32_t                                     op_target_idx,
	int32_t                                     ip_max_targets_per_zone,
	int32_t                                     op_max_targets_per_zone,
	uint8_t                                     range_clip_en,
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
	struct   vl53l5_sharpener_target_data_t  *psharp_tgt_data,
#endif
	struct   vl53l5_range_results_t                      *presults);

uint32_t vl53l5_otf_gen1_calc_system_dmax(
	uint16_t       wrap_dmax_mm,
	uint16_t       ambient_dmax_mm);

void vl53l5_otf_target_filter(
	int32_t                                     zone_idx,
	int32_t                                     ip_max_targets_per_zone,
	int32_t                                     op_max_targets_per_zone,
	uint8_t                                     range_clip_en,
	struct   vl53l5_otf__tgt_status__cfg_t     *ptgt_status_list_valid,
	struct   vl53l5_otf__tgt_status__cfg_t     *ptgt_status_list_blurred,
	struct   vl53l5_otf_int_dev_t              *pint_dev,
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
	struct   vl53l5_sharpener_target_data_t  *psharp_tgt_data,
#endif
	struct   vl53l5_range_results_t                      *presults,
	struct   common_grp__status_t              *perror);

void  vl53l5_otf_copy_single_zone_data(
	int32_t                              ip_z,
	int32_t                              op_z,
	uint8_t                              range_clip_en,
	struct     vl53l5_range_results_t             *presults,
	struct     dci_patch_op_dev_t       *pop_dev,
	struct     common_grp__status_t     *perror);

#ifdef __cplusplus
}
#endif

#endif
