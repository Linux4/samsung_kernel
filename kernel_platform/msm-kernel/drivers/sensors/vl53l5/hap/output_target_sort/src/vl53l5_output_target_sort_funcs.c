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

#include "vl53l5_output_target_sort_funcs.h"

uint16_t vl53l5_ots_first_tgt_idx_for_zone(
	uint8_t  z,
	uint8_t  rng__max_targets_per_zone)
{
	return (uint16_t)((uint16_t)z * (uint16_t)rng__max_targets_per_zone);
}

void vl53l5_ots_clear_data(struct vl53l5_ots_err_dev_t  *perr_dev)
{

	struct common_grp__status_t    *perror   = &(perr_dev->ots_error_status);
	struct common_grp__status_t    *pwarning = &(perr_dev->ots_warning_status);

	perror->status__group = VL53L5_ALGO_OUTPUT_TARGET_SORT_GROUP_ID;
	perror->status__stage_id = VL53L5_OTS_STAGE_ID__MAIN;
	perror->status__type = VL53L5_OTS_ERROR_NONE;
	perror->status__debug_0 = 0U;
	perror->status__debug_1 = 0U;
	perror->status__debug_2 = 0U;

	pwarning->status__group = VL53L5_ALGO_OUTPUT_TARGET_SORT_GROUP_ID;
	pwarning->status__stage_id = VL53L5_OTS_STAGE_ID__MAIN;
	pwarning->status__type = VL53L5_OTS_WARNING_NONE;
	pwarning->status__debug_0 = 0U;
	pwarning->status__debug_1 = 0U;
	pwarning->status__debug_2 = 0U;
}

void vl53l5_ots_swap_targets(
	struct vl53l5_range_per_tgt_results_t    *prng_tgt_data,
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
	struct vl53l5_sharpener_target_data_t  *psharp_tgt_data,
#endif
	uint16_t                                   first_idx,
	uint16_t                                   second_idx)
{
	uint8_t  temp_u8;
	int16_t  temp_i16;
	uint16_t temp_u16;
	uint32_t temp_u32;

#ifdef VL53L5_PEAK_RATE_KCPS_PER_SPAD_ON
	temp_u32 = prng_tgt_data->peak_rate_kcps_per_spad[first_idx];
	prng_tgt_data->peak_rate_kcps_per_spad[first_idx] =
		prng_tgt_data->peak_rate_kcps_per_spad[second_idx];
	prng_tgt_data->peak_rate_kcps_per_spad[second_idx] = temp_u32;
#endif
#ifdef VL53L5_MEDIAN_PHASE_ON
	temp_u32 = prng_tgt_data->median_phase[first_idx];
	prng_tgt_data->median_phase[first_idx] =
		prng_tgt_data->median_phase[second_idx];
	prng_tgt_data->median_phase[second_idx] = temp_u32;
#endif
#ifdef VL53L5_RATE_SIGMA_KCPS_PER_SPAD_ON
	temp_u32 = prng_tgt_data->rate_sigma_kcps_per_spad[first_idx];
	prng_tgt_data->rate_sigma_kcps_per_spad[first_idx] =
		prng_tgt_data->rate_sigma_kcps_per_spad[second_idx];
	prng_tgt_data->rate_sigma_kcps_per_spad[second_idx] = temp_u32;
#endif
#ifdef VL53L5_RANGE_SIGMA_MM_ON
	temp_u16 = prng_tgt_data->range_sigma_mm[first_idx];
	prng_tgt_data->range_sigma_mm[first_idx] =
		prng_tgt_data->range_sigma_mm[second_idx];
	prng_tgt_data->range_sigma_mm[second_idx] = temp_u16;
#endif
#ifdef VL53L5_MEDIAN_RANGE_MM_ON
	temp_i16 = prng_tgt_data->median_range_mm[first_idx];
	prng_tgt_data->median_range_mm[first_idx] =
		prng_tgt_data->median_range_mm[second_idx];
	prng_tgt_data->median_range_mm[second_idx] = temp_i16;
#endif
#ifdef VL53L5_MIN_RANGE_DELTA_MM_ON
	temp_u8 = prng_tgt_data->min_range_delta_mm[first_idx];
	prng_tgt_data->min_range_delta_mm[first_idx] =
		prng_tgt_data->min_range_delta_mm[second_idx];
	prng_tgt_data->min_range_delta_mm[second_idx] = temp_u8;
#endif
#ifdef VL53L5_MAX_RANGE_DELTA_MM_ON
	temp_u8 = prng_tgt_data->max_range_delta_mm[first_idx];
	prng_tgt_data->max_range_delta_mm[first_idx] =
		prng_tgt_data->max_range_delta_mm[second_idx];
	prng_tgt_data->max_range_delta_mm[second_idx] = temp_u8;
#endif
#ifdef VL53L5_TARGET_REFLECTANCE_EST_PC_ON
	temp_u8 = prng_tgt_data->target_reflectance_est_pc[first_idx];
	prng_tgt_data->target_reflectance_est_pc[first_idx] =
		prng_tgt_data->target_reflectance_est_pc[second_idx];
	prng_tgt_data->target_reflectance_est_pc[second_idx] = temp_u8;
#endif
#ifdef VL53L5_TARGET_STATUS_ON
	temp_u8 = prng_tgt_data->target_status[first_idx];
	prng_tgt_data->target_status[first_idx] =
		prng_tgt_data->target_status[second_idx];
	prng_tgt_data->target_status[second_idx] = temp_u8;
#endif

#ifdef VL53L5_TARGET_ZSCORE_ON
	temp_u16 = prng_tgt_data->target_zscore[first_idx];
	prng_tgt_data->target_zscore[first_idx] =
		prng_tgt_data->target_zscore[second_idx];
	prng_tgt_data->target_zscore[second_idx] = temp_u16;
#endif
#ifdef VL53L5_START_RANGE_MM_ON
	temp_i16 = prng_tgt_data->start_range_mm[first_idx];
	prng_tgt_data->start_range_mm[first_idx] =
		prng_tgt_data->start_range_mm[second_idx];
	prng_tgt_data->start_range_mm[second_idx] = temp_i16;
#endif
#ifdef VL53L5_END_RANGE_MM_ON
	temp_i16 = prng_tgt_data->end_range_mm[first_idx];
	prng_tgt_data->end_range_mm[first_idx] =
		prng_tgt_data->end_range_mm[second_idx];
	prng_tgt_data->end_range_mm[second_idx] = temp_i16;
#endif

#ifdef VL53L5_SHARPENER_GROUP_INDEX_ON
	temp_u8 = psharp_tgt_data->sharpener__group_index[first_idx];
	psharp_tgt_data->sharpener__group_index[first_idx] =
		psharp_tgt_data->sharpener__group_index[second_idx];
	psharp_tgt_data->sharpener__group_index[second_idx] = temp_u8;
#endif
#ifdef VL53L5_SHARPENER_CONFIDENCE_ON
	temp_u8 = psharp_tgt_data->sharpener__confidence[first_idx];
	psharp_tgt_data->sharpener__confidence[first_idx] =
		psharp_tgt_data->sharpener__confidence[second_idx];
	psharp_tgt_data->sharpener__confidence[second_idx] = temp_u8;
#endif
}

#ifdef VL53L5_MEDIAN_RANGE_MM_ON
uint8_t vl53l5_ots_swap_reqd_by_range(
	struct vl53l5_range_per_tgt_results_t  *prng_tgt_data,
	uint16_t first_idx,
	uint16_t second_idx)
{

	if (prng_tgt_data->median_range_mm[first_idx] >
		prng_tgt_data->median_range_mm[second_idx])
		return 1;
	else
		return 0;
}
#endif

#ifdef VL53L5_PEAK_RATE_KCPS_PER_SPAD_ON
uint8_t vl53l5_ots_swap_reqd_by_rate(
	struct vl53l5_range_per_tgt_results_t  *prng_tgt_data,
	uint16_t first_idx,
	uint16_t second_idx)
{

	if (prng_tgt_data->peak_rate_kcps_per_spad[first_idx] <
		prng_tgt_data->peak_rate_kcps_per_spad[second_idx])
		return 1;
	else
		return 0;
}
#endif

#ifdef VL53L5_TARGET_ZSCORE_ON
uint8_t vl53l5_ots_swap_reqd_by_zscore(
	struct vl53l5_range_per_tgt_results_t  *prng_tgt_data,
	uint16_t first_idx,
	uint16_t second_idx)
{

	if (prng_tgt_data->target_zscore[first_idx] <
		prng_tgt_data->target_zscore[second_idx])
		return 1;
	else
		return 0;
}
#endif

void vl53l5_ots_sort_targets(
	struct vl53l5_range_per_tgt_results_t    *prng_tgt_data,
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
	struct vl53l5_sharpener_target_data_t  *psharp_tgt_data,
#endif
	uint16_t                                   first_tgt_idx_for_zone,
	uint16_t                                   rng__no_of_targets,
	uint8_t (*swap_func_ptr)(struct vl53l5_range_per_tgt_results_t *, uint16_t,
							 uint16_t))
{
	uint16_t i, j;
	uint8_t  swapped = 0;

	for (i = 0; i < rng__no_of_targets - 1; ++i) {
		swapped = 0;

		for (j = first_tgt_idx_for_zone;
			 j < ((first_tgt_idx_for_zone + rng__no_of_targets) - i) - 1; ++j) {

			if (swap_func_ptr(prng_tgt_data, j, (uint16_t)(j + 1))) {
				vl53l5_ots_swap_targets(
					prng_tgt_data,
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
					psharp_tgt_data,
#endif
					j,
					(uint16_t)(j + 1));
				swapped = 1;
			}
		}

		if (swapped == 0)
			break;
	}
}
