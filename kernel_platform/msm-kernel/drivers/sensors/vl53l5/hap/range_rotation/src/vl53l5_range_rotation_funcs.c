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

#include "vl53l5_platform_algo_macros.h"
#include "vl53l5_platform_algo_limits.h"
#include "vl53l5_range_rotation_funcs.h"
#include "dci_defs.h"
#include "dci_luts.h"
#include "vl53l5_rr_defs.h"
#include "vl53l5_rr_luts.h"

void vl53l5_rr_clear_data(
	struct vl53l5_rr_err_dev_t          *perr_dev)
{
	struct common_grp__status_t    *perror   = &(perr_dev->rr_error_status);
	struct common_grp__status_t    *pwarning = &(perr_dev->rr_warning_status);

	perror->status__group = VL53L5_ALGO_RANGE_ROTATION_GROUP_ID;
	perror->status__stage_id = VL53L5_RR_STAGE_ID__MAIN;
	perror->status__type = VL53L5_RR_ERROR_NONE;
	perror->status__debug_0 = 0U;
	perror->status__debug_1 = 0U;
	perror->status__debug_2 = 0U;

	pwarning->status__group = VL53L5_ALGO_RANGE_ROTATION_GROUP_ID;
	pwarning->status__stage_id = VL53L5_RR_STAGE_ID__MAIN;
	pwarning->status__type = VL53L5_RR_WARNING_NONE;
	pwarning->status__debug_0 = 0U;
	pwarning->status__debug_1 = 0U;
	pwarning->status__debug_2 = 0U;
}

void vl53l5_rr_init_zone_order_coeffs(
	int32_t    no_of_zones,
	int32_t    rotation_sel,
	struct     vl53l5_rr__zone_order__cfg_t  *pdata,
	struct     common_grp__status_t          *perror)
{

	int16_t grid_size = 0;

	perror->status__stage_id = VL53L5_RR_STAGE_ID__INIT_ZONE_ORDER_COEFFS;
	perror->status__type = VL53L5_RR_ERROR_NONE;

	switch (no_of_zones) {
	case 64:
		grid_size = 8;
		break;
	case 16:
		grid_size = 4;
		break;
	default:
		perror->status__type = VL53L5_RR_ERROR_UNSUPPORTED_NO_OF_ZONES;
		break;
	}

	pdata->rr__no_of_zones = (int16_t)no_of_zones;
	pdata->rr__grid_size = grid_size;

	switch (rotation_sel) {
	case VL53L5_RR_ROTATION_SEL__NONE:
		pdata->rr__col_m =  0;
		pdata->rr__col_x =  1;
		pdata->rr__row_m =  0;
		pdata->rr__row_x =  grid_size;
		break;
	case VL53L5_RR_ROTATION_SEL__CW_90:
		pdata->rr__col_m = (grid_size - 1) * grid_size;
		pdata->rr__col_x =  -grid_size;
		pdata->rr__row_m =  0;
		pdata->rr__row_x =  1;
		break;
	case VL53L5_RR_ROTATION_SEL__CW_180:
		pdata->rr__col_m =  0;
		pdata->rr__col_x = -1;
		pdata->rr__row_m = (grid_size * grid_size) - 1;
		pdata->rr__row_x =  -grid_size;
		break;
	case VL53L5_RR_ROTATION_SEL__CW_270:
		pdata->rr__col_m =  0;
		pdata->rr__col_x =  grid_size;
		pdata->rr__row_m =  grid_size - 1;
		pdata->rr__row_x =  -1;
		break;
	case VL53L5_RR_ROTATION_SEL__MIRROR_X:
		pdata->rr__col_m =  grid_size - 1;
		pdata->rr__col_x = -1;
		pdata->rr__row_m =  0;
		pdata->rr__row_x =  grid_size;
		break;
	case VL53L5_RR_ROTATION_SEL__MIRROR_Y:
		pdata->rr__col_m =  0;
		pdata->rr__col_x =  1;
		pdata->rr__row_m = (grid_size - 1) * grid_size;
		pdata->rr__row_x =  -grid_size;
		break;
	default:
		perror->status__type = VL53L5_RR_ERROR_INVALID_ROTATION_SEL;
		break;
	}
}

int32_t vl53l5_rr_new_sequence_idx(
	int32_t    zone_id,
	struct     vl53l5_rr__zone_order__cfg_t    *pdata,
	struct     common_grp__status_t            *perror)
{

	int32_t  r = 0U;
	int32_t  c = 0U;
	int32_t  sequence_idx = 0U;

	perror->status__stage_id = VL53L5_RR_STAGE_ID__NEW_SEQUENCE_IDX;
	perror->status__type = VL53L5_RR_ERROR_NONE;

	if (zone_id < 0 || zone_id > (int32_t)pdata->rr__no_of_zones) {
		sequence_idx = -1;
		perror->status__type = VL53L5_RR_ERROR_INVALID_ZONE_ID;
	} else {
		r = zone_id / pdata->rr__grid_size;
		c = zone_id % pdata->rr__grid_size;

		sequence_idx = ((r * pdata->rr__row_x) + pdata->rr__row_m);
		sequence_idx += ((c * pdata->rr__col_x) + pdata->rr__col_m);
	}

	return sequence_idx;
}

void  vl53l5_rr_rotate_per_zone_data(
	int32_t                            iz,
	int32_t                            oz,
	struct   vl53l5_range_results_t             *pinput,
	struct   vl53l5_range_results_t             *poutput)
{

#ifdef VL53L5_AMB_RATE_KCPS_PER_SPAD_ON
	poutput->per_zone_results.amb_rate_kcps_per_spad[oz] =
		pinput->per_zone_results.amb_rate_kcps_per_spad[iz];
#endif
#ifdef VL53L5_EFFECTIVE_SPAD_COUNT_ON
	poutput->per_zone_results.rng__effective_spad_count[oz] =
		pinput->per_zone_results.rng__effective_spad_count[iz];
#endif
#ifdef VL53L5_AMB_DMAX_MM_ON
	poutput->per_zone_results.amb_dmax_mm[oz] =
		pinput->per_zone_results.amb_dmax_mm[iz];
#endif
#ifdef VL53L5_SILICON_TEMP_DEGC_START_ON
	poutput->per_zone_results.silicon_temp_degc__start[oz] =
		pinput->per_zone_results.silicon_temp_degc__start[iz];
#endif
#ifdef VL53L5_SILICON_TEMP_DEGC_END_ON
	poutput->per_zone_results.silicon_temp_degc__end[oz] =
		pinput->per_zone_results.silicon_temp_degc__end[iz];
#endif
	poutput->per_zone_results.rng__no_of_targets[oz] =
		pinput->per_zone_results.rng__no_of_targets[iz];
#ifdef VL53L5_ZONE_ID_ON
	poutput->per_zone_results.rng__zone_id[oz] =
		pinput->per_zone_results.rng__zone_id[iz];
#endif
#ifdef VL53L5_SEQUENCE_IDX_ON
	poutput->per_zone_results.rng__sequence_idx[oz] =
		pinput->per_zone_results.rng__sequence_idx[iz];
#endif
}

void vl53l5_rr_rotate_per_target_data(
	int32_t                                    iz,
	int32_t                                    oz,
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
	struct   vl53l5_sharpener_target_data_t  *pip_sharp,
	struct   vl53l5_sharpener_target_data_t  *pop_sharp,
#endif
	struct   vl53l5_range_results_t                      *pip_results,
	struct   vl53l5_range_results_t                      *pop_results)
{

	int32_t   t   = 0;

	int32_t   it  = 0;

	int32_t   ot  = 0;

	int32_t   max_targets_per_zone =
		(int32_t)pip_results->common_data.rng__max_targets_per_zone;
	int32_t   no_of_targets =
		(int32_t)pip_results->per_zone_results.rng__no_of_targets[iz];

	for (t = 0 ; t < no_of_targets; t++) {
		it = (iz * max_targets_per_zone) + t;
		ot = (oz * max_targets_per_zone) + t;

#ifdef VL53L5_PEAK_RATE_KCPS_PER_SPAD_ON
		pop_results->per_tgt_results.peak_rate_kcps_per_spad[ot] =
			pip_results->per_tgt_results.peak_rate_kcps_per_spad[it];
#endif
#ifdef VL53L5_MEDIAN_PHASE_ON
		pop_results->per_tgt_results.median_phase[ot] =
			pip_results->per_tgt_results.median_phase[it];
#endif
#ifdef VL53L5_RATE_SIGMA_KCPS_PER_SPAD_ON
		pop_results->per_tgt_results.rate_sigma_kcps_per_spad[ot] =
			pip_results->per_tgt_results.rate_sigma_kcps_per_spad[it];
#endif
#ifdef VL53L5_RANGE_SIGMA_MM_ON
		pop_results->per_tgt_results.range_sigma_mm[ot] =
			pip_results->per_tgt_results.range_sigma_mm[it];
#endif
#ifdef VL53L5_MEDIAN_RANGE_MM_ON
		pop_results->per_tgt_results.median_range_mm[ot] =
			pip_results->per_tgt_results.median_range_mm[it];
#endif
#ifdef VL53L5_MIN_RANGE_DELTA_MM_ON
		pop_results->per_tgt_results.min_range_delta_mm[ot] =
			pip_results->per_tgt_results.min_range_delta_mm[it];
#endif
#ifdef VL53L5_MAX_RANGE_DELTA_MM_ON
		pop_results->per_tgt_results.max_range_delta_mm[ot] =
			pip_results->per_tgt_results.max_range_delta_mm[it];
#endif
#ifdef VL53L5_TARGET_REFLECTANCE_EST_PC_ON
		pop_results->per_tgt_results.target_reflectance_est_pc[ot] =
			pip_results->per_tgt_results.target_reflectance_est_pc[it];
#endif
#ifdef VL53L5_TARGET_STATUS_ON
		pop_results->per_tgt_results.target_status[ot] =
			pip_results->per_tgt_results.target_status[it];
#endif

#ifdef VL53L5_TARGET_ZSCORE_ON
		pop_results->per_tgt_results.target_zscore[ot] =
			pip_results->per_tgt_results.target_zscore[it];
#endif
#ifdef VL53L5_START_RANGE_MM_ON
		pop_results->per_tgt_results.start_range_mm[ot] =
			pip_results->per_tgt_results.start_range_mm[it];
#endif
#ifdef VL53L5_END_RANGE_MM_ON
		pop_results->per_tgt_results.end_range_mm[ot] =
			pip_results->per_tgt_results.end_range_mm[it];
#endif

#ifdef VL53L5_SHARPENER_GROUP_INDEX_ON
		pop_sharp->sharpener__group_index[ot] =
			pip_sharp->sharpener__group_index[it];
#endif
#ifdef VL53L5_SHARPENER_CONFIDENCE_ON
		pop_sharp->sharpener__confidence[ot] =
			pip_sharp->sharpener__confidence[it];
#endif

	}
}
