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
#include "dci_defs.h"
#include "dci_luts.h"
#include "vl53l5_otf_defs.h"
#include "vl53l5_otf_luts.h"
#include "vl53l5_output_target_filter_funcs.h"

void vl53l5_otf_clear_data(
	struct dci_patch_op_dev_t           *pop_dev,
	struct vl53l5_otf_err_dev_t         *perr_dev)
{

	struct common_grp__status_t    *perror   =
		&(perr_dev->otf_error_status);
	struct common_grp__status_t    *pwarning =
		&(perr_dev->otf_warning_status);

	if (pop_dev != NULL) {
		memset(
			&(pop_dev->sz_zone_data),
			0,
			sizeof(struct dci_grp__sz__zone_data_t));
		memset(
			&(pop_dev->sz_target_data),
			0,
			sizeof(struct dci_grp__sz__target_data_t));
	}

	perror->status__group = VL53L5_ALGO_OUTPUT_TARGET_FILTER_GROUP_ID;
	perror->status__stage_id = VL53L5_OTF_STAGE_ID__MAIN;
	perror->status__type = VL53L5_OTF_ERROR_NONE;
	perror->status__debug_0 = 0U;
	perror->status__debug_1 = 0U;
	perror->status__debug_2 = 0U;

	pwarning->status__group = VL53L5_ALGO_OUTPUT_TARGET_FILTER_GROUP_ID;
	pwarning->status__stage_id = VL53L5_OTF_STAGE_ID__MAIN;
	pwarning->status__type = VL53L5_OTF_WARNING_NONE;
	pwarning->status__debug_0 = 0U;
	pwarning->status__debug_1 = 0U;
	pwarning->status__debug_2 = 0U;
}

int32_t vl53l5_otf_is_target_valid(
	uint8_t                                  target_status,
	struct   vl53l5_otf__tgt_status__cfg_t  *ptgt_status_list)
{

	int32_t   target_valid = 0U;
	uint8_t  *ptgt_status = &(ptgt_status_list->otf__tgt_status[0]);

	while (*ptgt_status != 0U && *ptgt_status != 255U) {
		if (target_status == *ptgt_status)
			target_valid = 1U;
		ptgt_status++;
	}

	return target_valid;
}

void vl53l5_otf_init_tgt_idx_list(
	uint32_t                              init_value,
	struct  vl53l5_otf__tgt__idx_data_t  *ptgt_idx_data)
{

	int32_t   i = 0;

	for (i = 0; i < (int32_t)VL53L5_OTF_DEF__MAX_TARGETS_PER_ZONE; i++)
		ptgt_idx_data->otf__tgt_idx[i] = init_value;
}

void vl53l5_otf_filter_targets(
	int32_t                                  zone_idx,
	struct   vl53l5_otf__tgt_status__cfg_t  *ptgt_status_list_valid,
	struct   vl53l5_otf__tgt_status__cfg_t  *ptgt_status_list_blurred,
	struct   vl53l5_otf_int_dev_t           *pint_dev,
	struct   vl53l5_range_results_t                   *presults,
	int32_t                                  *pop_no_of_targets)
{

	int32_t   it = 0;
	int32_t   t = 0;
	int32_t   i = 0;
	int32_t   is_valid = 0;
	int32_t   is_blurred = 0;
	int32_t   found_valid = 0;

	int32_t  max_targets_per_zone =
		(int32_t)presults->common_data.rng__max_targets_per_zone;
	int32_t  ip_no_of_targets =
		(int32_t)presults->per_zone_results.rng__no_of_targets[zone_idx];

	t = 0;
	vl53l5_otf_init_tgt_idx_list(
		255,
		&(pint_dev->otf_tgt_idx_data));

	for (i = 0 ; i < ip_no_of_targets; i++) {
		it = (zone_idx * max_targets_per_zone) + i;

		is_valid =
			vl53l5_otf_is_target_valid(
				presults->per_tgt_results.target_status[it],
				ptgt_status_list_valid);

		is_blurred =
			vl53l5_otf_is_target_valid(
				presults->per_tgt_results.target_status[it],
				ptgt_status_list_blurred);

		if (is_valid > 0 && (t < (int32_t)VL53L5_OTF_DEF__MAX_TARGETS_PER_ZONE)) {

			if (found_valid == 0) {
				t = 0;
				vl53l5_otf_init_tgt_idx_list(
					255,
					&(pint_dev->otf_tgt_idx_data));
			}

			found_valid = 1;
			pint_dev->otf_tgt_idx_data.otf__tgt_idx[t] = (uint32_t)i;
			t++;
		} else if (is_blurred > 0
				   && (t < (int32_t)VL53L5_OTF_DEF__MAX_TARGETS_PER_ZONE)) {
			pint_dev->otf_tgt_idx_data.otf__tgt_idx[t] = (uint32_t)i;
			t++;
		}
	}

	*pop_no_of_targets = t;
}

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
	struct   vl53l5_range_results_t                      *presults)
{

	int32_t it = (zone_idx * ip_max_targets_per_zone) + ip_target_idx;
	int32_t ot = (zone_idx * op_max_targets_per_zone) + op_target_idx;

	struct  vl53l5_range_per_tgt_results_t  *ptargets =
		&(presults->per_tgt_results);

	if (it != ot) {
#ifdef VL53L5_PEAK_RATE_KCPS_PER_SPAD_ON
		ptargets->peak_rate_kcps_per_spad[ot] =
			ptargets->peak_rate_kcps_per_spad[it];
#endif
#ifdef VL53L5_MEDIAN_PHASE_ON
		ptargets->median_phase[ot] =
			ptargets->median_phase[it];
#endif
#ifdef VL53L5_RATE_SIGMA_KCPS_PER_SPAD_ON
		ptargets->rate_sigma_kcps_per_spad[ot] =
			ptargets->rate_sigma_kcps_per_spad[it];
#endif
#ifdef VL53L5_RANGE_SIGMA_MM_ON
		ptargets->range_sigma_mm[ot] =
			ptargets->range_sigma_mm[it];
#endif
#ifdef VL53L5_MEDIAN_RANGE_MM_ON
		ptargets->median_range_mm[ot] =
			ptargets->median_range_mm[it];
#endif
#ifdef VL53L5_MIN_RANGE_DELTA_MM_ON
		ptargets->min_range_delta_mm[ot] =
			ptargets->min_range_delta_mm[it];
#endif
#ifdef VL53L5_MAX_RANGE_DELTA_MM_ON
		ptargets->max_range_delta_mm[ot] =
			ptargets->max_range_delta_mm[it];
#endif
#ifdef VL53L5_TARGET_REFLECTANCE_EST_PC_ON
		ptargets->target_reflectance_est_pc[ot] =
			ptargets->target_reflectance_est_pc[it];
#endif
#ifdef VL53L5_TARGET_STATUS_ON
		ptargets->target_status[ot] =
			ptargets->target_status[it];
#endif

#ifdef VL53L5_TARGET_ZSCORE_ON
		ptargets->target_zscore[ot] =
			ptargets->target_zscore[it];
#endif
#ifdef VL53L5_START_RANGE_MM_ON
		ptargets->start_range_mm[ot] =
			ptargets->start_range_mm[it];
#endif
#ifdef VL53L5_END_RANGE_MM_ON
		ptargets->end_range_mm[ot] =
			ptargets->end_range_mm[it];
#endif

#ifdef VL53L5_SHARPENER_GROUP_INDEX_ON
		psharp_tgt_data->sharpener__group_index[ot] =
			psharp_tgt_data->sharpener__group_index[it];
#endif
#ifdef VL53L5_SHARPENER_CONFIDENCE_ON
		psharp_tgt_data->sharpener__confidence[ot] =
			psharp_tgt_data->sharpener__confidence[it];
#endif
	}

#ifdef VL53L5_MEDIAN_RANGE_MM_ON
	if (range_clip_en > 0 && ptargets->median_range_mm[ot] < 0)
		ptargets->median_range_mm[ot] = 0;
#endif
}

uint32_t vl53l5_otf_gen1_calc_system_dmax(
	uint16_t       wrap_dmax_mm,
	uint16_t       ambient_dmax_mm)
{

	uint32_t  system_dmax_mm = (uint32_t)wrap_dmax_mm;

	if (ambient_dmax_mm < wrap_dmax_mm)
		system_dmax_mm = (uint32_t)ambient_dmax_mm;

	if (system_dmax_mm > 0x1FFFU)
		system_dmax_mm = 0x1FFFU;

	return system_dmax_mm;
}

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
	struct   common_grp__status_t              *perror)
{

	int32_t   t   = 0;

	int32_t   it  = 0;

	int32_t   ot  = 0;

	int32_t   no_of_targets = 0;
	uint32_t  system_dmax_mm = 0U;

	perror->status__stage_id = VL53L5_OTF_STAGE_ID__TARGET_FILTER;
	perror->status__type = VL53L5_OTF_ERROR_NONE;

	system_dmax_mm =
		vl53l5_otf_gen1_calc_system_dmax(
			presults->common_data.wrap_dmax_mm,
			presults->per_zone_results.amb_dmax_mm[zone_idx]);

	vl53l5_otf_filter_targets(
		zone_idx,
		ptgt_status_list_valid,
		ptgt_status_list_blurred,
		pint_dev,
		presults,
		&no_of_targets);

	for (ot = 0; ot < no_of_targets && ot < op_max_targets_per_zone; ot++) {
		it = (int32_t)pint_dev->otf_tgt_idx_data.otf__tgt_idx[ot];

		vl53l5_otf_copy_target(
			zone_idx,
			it,
			ot,
			ip_max_targets_per_zone,
			op_max_targets_per_zone,
			range_clip_en,
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
			psharp_tgt_data,
#endif
			presults);
	}
	no_of_targets = ot;

	presults->per_zone_results.rng__no_of_targets[zone_idx] =
		(uint8_t)no_of_targets;

	for (t = no_of_targets; t < op_max_targets_per_zone; t++) {
		ot = (zone_idx * op_max_targets_per_zone) + t;
		if (t == 0) {
			presults->per_tgt_results.median_range_mm[ot] =
				(int16_t)(system_dmax_mm << 2U);
		} else
			presults->per_tgt_results.median_range_mm[ot] = 0U;
#ifdef VL53L5_PEAK_RATE_KCPS_PER_SPAD_ON
		presults->per_tgt_results.peak_rate_kcps_per_spad[ot] = 0U;
#endif
		presults->per_tgt_results.target_status[ot] =
			DCI_TARGET_STATUS__NOUPDATE;
	}
}

void  vl53l5_otf_copy_single_zone_data(
	int32_t                              ip_z,
	int32_t                              op_z,
	uint8_t                              range_clip_en,
	struct     vl53l5_range_results_t             *presults,
	struct     dci_patch_op_dev_t       *pop_dev,
	struct     common_grp__status_t     *perror)
{

	int32_t   t = 0;
	int32_t   ip_i = 0;
	int32_t   op_i = 0;
	int32_t   max_targets =
		(int32_t)presults->common_data.rng__max_targets_per_zone;
	int32_t   no_of_targets =
		(int32_t)presults->per_zone_results.rng__no_of_targets[ip_z];
	int16_t   range_mm = 0;

	perror->status__stage_id = VL53L5_OTF_STAGE_ID__INIT_SINGLE_ZONE_DATA;
	perror->status__type = VL53L5_OTF_ERROR_INVALID_SINGLE_ZONE_ID;

	if (max_targets > (int32_t)VL53L5_OTF_DEF__MAX_TARGETS_PER_ZONE)
		max_targets = (int32_t)VL53L5_OTF_DEF__MAX_TARGETS_PER_ZONE;

	if (no_of_targets > max_targets)
		no_of_targets = max_targets;

	pop_dev->sz_common_data.sz__max_targets_per_zone =
		(uint8_t)max_targets;
	pop_dev->sz_common_data.sz__no_of_zones =
		presults->common_data.rng__acc__no_of_zones;
	pop_dev->sz_common_data.sz__common__spare_0 = 0U;
	pop_dev->sz_common_data.sz__common__spare_1 = 0U;

	if (ip_z < (int32_t)VL53L5_MAX_ZONES) {
		perror->status__type = VL53L5_OTF_ERROR_NONE;

#ifdef VL53L5_AMB_RATE_KCPS_PER_SPAD_ON
		pop_dev->sz_zone_data.amb_rate_kcps_per_spad[op_z] =
			presults->per_zone_results.amb_rate_kcps_per_spad[ip_z];
#endif
#ifdef VL53L5_EFFECTIVE_SPAD_COUNT_ON
		pop_dev->sz_zone_data.rng__effective_spad_count[op_z] =
			presults->per_zone_results.rng__effective_spad_count[ip_z];
#endif
#ifdef VL53L5_AMB_DMAX_MM_ON
		pop_dev->sz_zone_data.amb_dmax_mm[op_z] =
			presults->per_zone_results.amb_dmax_mm[ip_z];
#endif
		pop_dev->sz_zone_data.rng__no_of_targets[op_z] =
			(uint8_t)no_of_targets;
#ifdef VL53L5_ZONE_ID_ON
		pop_dev->sz_zone_data.rng__zone_id[op_z] =
			presults->per_zone_results.rng__zone_id[ip_z];
#endif

		for (t = 0; t < no_of_targets; t++) {
			ip_i = (ip_z * max_targets) + t;
			op_i = (op_z * max_targets) + t;
			if (ip_i < VL53L5_MAX_TARGETS && op_i < DCI_SZ_MAX_TARGETS) {
#ifdef VL53L5_PEAK_RATE_KCPS_PER_SPAD_ON
				pop_dev->sz_target_data.peak_rate_kcps_per_spad[op_i] =
					presults->per_tgt_results.peak_rate_kcps_per_spad[ip_i];
#endif
#ifdef VL53L5_RANGE_SIGMA_MM_ON
				pop_dev->sz_target_data.range_sigma_mm[op_i] =
					presults->per_tgt_results.range_sigma_mm[ip_i];
#endif

				range_mm = presults->per_tgt_results.median_range_mm[ip_i];
				if (range_clip_en > 0 && range_mm < 0)
					range_mm = 0;
				pop_dev->sz_target_data.median_range_mm[op_i] = range_mm;

				pop_dev->sz_target_data.target_status[op_i] =
					presults->per_tgt_results.target_status[ip_i];
			}
		}
	}
}
