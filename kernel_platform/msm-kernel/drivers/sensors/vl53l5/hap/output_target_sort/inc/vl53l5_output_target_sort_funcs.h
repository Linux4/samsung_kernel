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

#ifndef VL53L5_OUTPUT_TARGET_SORT_FUNCS_H
#define VL53L5_OUTPUT_TARGET_SORT_FUNCS_H

#include "vl53l5_platform_algo_macros.h"
#include "vl53l5_platform_algo_limits.h"
#include "dci_defs.h"
#include "dci_luts.h"
#include "vl53l5_ots_defs.h"
#include "vl53l5_ots_luts.h"
#include <stddef.h>

#include "vl53l5_types.h"
#include "vl53l5_results_config.h"
#include "common_datatype_structs.h"
#include "vl53l5_results_map.h"
#include "dci_structs.h"
#include "vl53l5_ots_cfg_map.h"
#include "vl53l5_ots_err_map.h"

uint16_t vl53l5_ots_first_tgt_idx_for_zone(
	uint8_t  z,
	uint8_t  rng__max_targets_per_zone);

void vl53l5_ots_clear_data(struct vl53l5_ots_err_dev_t  *perr_dev);

void vl53l5_ots_swap_targets(
	struct vl53l5_range_per_tgt_results_t    *prng_tgt_data,
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
	struct vl53l5_sharpener_target_data_t  *psharp_tgt_data,
#endif
	uint16_t                                   first_idx,
	uint16_t                                   second_idx);

#ifdef VL53L5_MEDIAN_RANGE_MM_ON
uint8_t vl53l5_ots_swap_reqd_by_range(
	struct vl53l5_range_per_tgt_results_t  *prng_tgt_data,
	uint16_t first_idx,
	uint16_t second_idx);
#endif

#ifdef VL53L5_PEAK_RATE_KCPS_PER_SPAD_ON
uint8_t vl53l5_ots_swap_reqd_by_rate(
	struct vl53l5_range_per_tgt_results_t  *prng_tgt_data,
	uint16_t first_idx,
	uint16_t second_idx);
#endif

#ifdef VL53L5_TARGET_ZSCORE_ON
uint8_t vl53l5_ots_swap_reqd_by_zscore(
	struct vl53l5_range_per_tgt_results_t  *prng_tgt_data,
	uint16_t first_idx,
	uint16_t second_idx);
#endif

void vl53l5_ots_sort_targets(
	struct vl53l5_range_per_tgt_results_t    *prng_tgt_data,
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
	struct vl53l5_sharpener_target_data_t  *psharp_tgt_data,
#endif
	uint16_t                                   first_tgt_idx_for_zone,
	uint16_t                                   rng__no_of_targets,
	uint8_t (*swap_func_ptr)(struct vl53l5_range_per_tgt_results_t *, uint16_t,
							 uint16_t));

#endif
