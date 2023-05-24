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
#include "vl53l5_anti_flicker_filter_funcs.h"

#include "dci_luts.h"

#ifdef AFF_FUNCTIONS_DEBUG
#include "aff_utils.h"
#endif

#include "vl53l5_aff_defs.h"
#include "vl53l5_aff_luts.h"
#include "vl53l5_aff_cfg_map.h"
#include "vl53l5_aff_int_map.h"
#include "vl53l5_aff_err_map.h"

void vl53l5_aff_clear_data(
	struct vl53l5_aff_int_dev_t         *pint_dev,
	struct vl53l5_aff_err_dev_t         *perr_dev)
{

	struct common_grp__status_t    *perror    = &(perr_dev->aff_error_status);
	struct common_grp__status_t    *pwarning  = &(perr_dev->aff_warning_status);

	memset(
		&(pint_dev->aff_persistent_container),
		0,
		sizeof(struct vl53l5_aff__persistent_container_t));
	memset(
		&(pint_dev->aff_workspace_container),
		0,
		sizeof(struct vl53l5_aff__workspace_container_t));

	perror->status__group       = VL53L5_ALGO_ANTI_FLICKER_FILTER_GROUP_ID;
	perror->status__stage_id    = VL53L5_AFF_STAGE_ID__MAIN;
	perror->status__type        = VL53L5_AFF_ERROR__NONE;
	perror->status__debug_0     = 0U;
	perror->status__debug_1     = 0U;
	perror->status__debug_2     = 0U;
	pwarning->status__group     = VL53L5_ALGO_ANTI_FLICKER_FILTER_GROUP_ID;
	pwarning->status__stage_id  = VL53L5_AFF_STAGE_ID__MAIN;
	pwarning->status__type      = VL53L5_AFF_WARNING__NONE;
	pwarning->status__debug_0   = 0U;
	pwarning->status__debug_1   = 0U;
	pwarning->status__debug_2   = 0U;

}

void vl53l5_aff_init_mem(
	struct vl53l5_aff_int_dev_t          *pint_dev,
	struct vl53l5_aff_cfg_dev_t          *pcfg_dev,
	struct vl53l5_aff_err_dev_t          *perr_dev)
{

	int32_t                        max_tgt;
	int32_t                        nb_zones;
	int32_t                        nb_past;
	int32_t                        tgt_hist_size;
	int32_t                        zone_size;
	int32_t                        required;
	int32_t                        T          = (int32_t)(DCI_MAX_TARGET_PER_ZONE);
	int32_t                        N          =
		(pcfg_dev->aff_cfg).aff__max_nb_of_targets_to_keep_per_zone;
	struct common_grp__status_t    *perror    = &(perr_dev->aff_error_status);

	if ((VL53L5_AFF_DEF__PERSISTENT_MEM_SIZE <= AFF_PM_MAX_TGT_IDX)
		|| (VL53L5_AFF_DEF__PERSISTENT_MEM_SIZE <= AFF_PM_NB_PAST_IDX)
		|| (VL53L5_AFF_DEF__PERSISTENT_MEM_SIZE <= AFF_PM_NB_ZONE_IDX)
		|| (VL53L5_AFF_DEF__PERSISTENT_MEM_SIZE <= AFF_PM_TEMP_IND_IDX)
	   ) {
		perror->status__stage_id    = VL53L5_AFF_STAGE_ID__INIT_MEM;
		perror->status__type        = VL53L5_AFF_ERROR__PERSISTENT_MEM_TOO_SMALL;
		goto exit;
	}

	memset(
		&(pint_dev->aff_persistent_container),
		0,
		sizeof(struct vl53l5_aff__persistent_container_t));

	(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_MAX_TGT_IDX]
		= (pcfg_dev->aff_cfg).aff__max_nb_of_targets_to_keep_per_zone;
	(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_NB_PAST_IDX]
		= (pcfg_dev->aff_cfg).aff__nb_of_past_instants;
	(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_TEMP_IND_IDX]
		= 0;

	(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_NB_ZONE_IDX]
		= 0;

	max_tgt        =
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_MAX_TGT_IDX];
	nb_zones       = (pcfg_dev->aff_cfg).aff__nb_zones_to_filter;
	nb_past        =
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_NB_PAST_IDX];
	tgt_hist_size  = (4 + nb_past * (int32_t)(sizeof(struct
					  vl53l5_aff_tgt_data_t)));

	zone_size      = max_tgt * tgt_hist_size;

	required       = 4 * (AFF_PM_HISTORY_IDX) + zone_size *
					 nb_zones;

	if (required > 4 * (int32_t)VL53L5_AFF_DEF__PERSISTENT_MEM_SIZE) {
		perror->status__stage_id    = VL53L5_AFF_STAGE_ID__INIT_MEM;
		perror->status__type        = VL53L5_AFF_ERROR__PERSISTENT_MEM_TOO_SMALL;
		goto exit;
	}

	required = 4 * AFF_WS_CUR_TGT_ARR_IDX
			   + T * (int32_t)(sizeof(struct vl53l5_aff_tgt_data_t))

			   + (T + N) * (int32_t)(sizeof(struct vl53l5_aff_tgt_out_t))

			   + N * (int32_t)(sizeof(struct vl53l5_aff_tgt_hist_t))

			   + T * N * 4

			   + T * 4

			   + N * 4

			   + 2 * T * N * 4

			   + T * 4;

	if (required > 4 * (int32_t)VL53L5_AFF_DEF__WORKSPACE_SIZE) {
		perror->status__stage_id    = VL53L5_AFF_STAGE_ID__INIT_MEM;
		perror->status__type        = VL53L5_AFF_ERROR__WORKSPACE_MEM_TOO_SMALL;
		goto exit;
	}

	(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_NB_ZONE_IDX]
		= nb_zones;

exit:

	return;

}

struct vl53l5_aff_tgt_data_t *vl53l5_aff_cur_tgt_arr(
	struct vl53l5_aff_int_dev_t          *pint_dev)
{

	return (struct vl53l5_aff_tgt_data_t *)(&((
			pint_dev->aff_workspace_container).aff__workspace_array[AFF_WS_CUR_TGT_ARR_IDX]));

}

struct vl53l5_aff_tgt_out_t *vl53l5_aff_out_cand_arr(
	struct vl53l5_aff_int_dev_t          *pint_dev)
{

	return (struct vl53l5_aff_tgt_out_t *)(&((
			pint_dev->aff_workspace_container).aff__workspace_array[AFF_WS_OUT_CAND_ARR_IDX]));

}

struct vl53l5_aff_tgt_hist_t *vl53l5_aff_filled_tgt_hist_arr(
	struct vl53l5_aff_int_dev_t          *pint_dev)
{

	int32_t max_tgt  =
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_MAX_TGT_IDX];

	return (struct vl53l5_aff_tgt_hist_t *)(&((
			pint_dev->aff_workspace_container).aff__workspace_array[(int32_t)(
					AFF_WS_OUT_CAND_ARR_IDX)
											+ ((int32_t)(max_tgt) + (int32_t)(DCI_MAX_TARGET_PER_ZONE)) * ((int32_t)(sizeof(
													struct vl53l5_aff_tgt_out_t) / 4))]));

}

int32_t *vl53l5_aff_affinity_matrix(
	struct vl53l5_aff_int_dev_t          *pint_dev)
{

	int32_t max_tgt  =
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_MAX_TGT_IDX];

	return (int32_t *)(&((
							 pint_dev->aff_workspace_container).aff__workspace_array[(uint32_t)(
										 AFF_WS_OUT_CAND_ARR_IDX)
									 + ((uint32_t)(max_tgt) + (uint32_t)(DCI_MAX_TARGET_PER_ZONE)) * ((uint32_t)(
											 sizeof(struct vl53l5_aff_tgt_out_t) / 4))
									 + ((uint32_t)(max_tgt) * ((uint32_t)(sizeof(struct vl53l5_aff_tgt_hist_t) /
											 4)))]));

}

int32_t *vl53l5_aff_hist_cur_asso(
	struct vl53l5_aff_int_dev_t          *pint_dev
)
{

	int32_t max_tgt  =
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_MAX_TGT_IDX];

	return (int32_t *)(vl53l5_aff_affinity_matrix(pint_dev) + (max_tgt * (int32_t)(
						   DCI_MAX_TARGET_PER_ZONE)));

}

int32_t *vl53l5_aff_cur_hist_asso(
	struct vl53l5_aff_int_dev_t          *pint_dev
)
{

	return (int32_t *)(vl53l5_aff_hist_cur_asso(pint_dev) +
					   (DCI_MAX_TARGET_PER_ZONE));

}

int32_t *vl53l5_aff_get_cur_pref(
	struct vl53l5_aff_int_dev_t          *pint_dev)
{

	int32_t max_tgt  =
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_MAX_TGT_IDX];

	return (int32_t *)(vl53l5_aff_cur_hist_asso(pint_dev) + (max_tgt));

}

int32_t *vl53l5_aff_get_hist_pref(
	struct vl53l5_aff_int_dev_t          *pint_dev)
{

	int32_t max_tgt  =
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_MAX_TGT_IDX];

	return (int32_t *)(vl53l5_aff_get_cur_pref(pint_dev) + (max_tgt * (int32_t)(
						   DCI_MAX_TARGET_PER_ZONE)));

}

int32_t *vl53l5_aff_get_ind_to_be_asso(
	struct vl53l5_aff_int_dev_t          *pint_dev)
{

	int32_t max_tgt  =
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_MAX_TGT_IDX];

	return (int32_t *)(vl53l5_aff_get_hist_pref(pint_dev) + (max_tgt * (int32_t)(
						   DCI_MAX_TARGET_PER_ZONE)));

}

struct vl53l5_aff_tgt_data_t *vl53l5_aff_passed_data(
	struct vl53l5_aff_int_dev_t   *pint_dev,
	struct vl53l5_aff_tgt_hist_t  *phist,
	int32_t                       time,
	int32_t                       iir_enabled
)
{

	int32_t nb_past  =
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_NB_PAST_IDX];
	int32_t index    =
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_TEMP_IND_IDX];

	if (iir_enabled == 1)
		nb_past = nb_past - 1;

	return (struct vl53l5_aff_tgt_data_t *)(&((phist->pdata_arr)[(index + time +
											nb_past) % nb_past]));

}

void vl53l5_aff_get_tgt_hist(
	struct vl53l5_aff_int_dev_t          *pint_dev,
	int32_t                              tgt_hist_id,
	int32_t                              zone_id,
	struct vl53l5_aff_tgt_hist_t         *ptgt_hist,
	struct vl53l5_aff_err_dev_t          *perr_dev
)
{

	int32_t                      *p;
	int32_t                      max_tgt        =
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_MAX_TGT_IDX];
	int32_t                      nb_past        =
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_NB_PAST_IDX];
	int32_t                      tgt_hist_size  = (4 + nb_past * (int32_t)(sizeof(
				struct vl53l5_aff_tgt_data_t))) / 4;
	int32_t                      zone_size      = max_tgt * tgt_hist_size;
	struct common_grp__status_t  *perror        = &(perr_dev->aff_error_status);

	p                      = (int32_t *)(&((
			pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_HISTORY_IDX]));
	p                      = p + zone_id * zone_size;
	p                      = p + tgt_hist_id * tgt_hist_size;

	if (p + tgt_hist_size > (int32_t *)(&((
			pint_dev->aff_persistent_container).aff__persistent_array[0])) + (int32_t)(
			VL53L5_AFF_DEF__PERSISTENT_MEM_SIZE)) {
		ptgt_hist->pfilled        = (int32_t *)(NULL);
		ptgt_hist->pdata_arr      = (struct vl53l5_aff_tgt_data_t *)(
										NULL);

		perror->status__stage_id  = VL53L5_AFF_STAGE_ID__GET_TGT_HIST;
		perror->status__type      = VL53L5_AFF_ERROR__INVALID_PARAM;
	} else {
		ptgt_hist->pfilled     = (int32_t *)p;
		ptgt_hist->pdata_arr   = (struct vl53l5_aff_tgt_data_t *)((int32_t *)p + 1);
	}
	ptgt_hist->avg_zscore = 0;

}

int32_t vl53l5_aff_dci_to_cur_tgt(
	struct vl53l5_range_results_t                  *pdci,
	int32_t                              zone_id_index,
	struct vl53l5_aff_int_dev_t          *pint_dev,
	struct vl53l5_aff_err_dev_t          *perr_dev
)
{

	int32_t                       t;
	int32_t                       nb_of_targets =
		(pdci->per_zone_results).rng__no_of_targets[zone_id_index];
	struct vl53l5_aff_tgt_data_t  *pcur         = vl53l5_aff_cur_tgt_arr(pint_dev);
	struct common_grp__status_t   *perror       = &(perr_dev->aff_error_status);
	int32_t                       offset;

	if (zone_id_index > (int32_t)(VL53L5_MAX_ZONES)) {
		perror->status__stage_id  = VL53L5_AFF_STAGE_ID__DCI_TO_CUR_TGT;
		perror->status__type      = VL53L5_AFF_ERROR__INVALID_PARAM;
		return 0;
	}

	offset = ((int32_t)((pdci->common_data).rng__max_targets_per_zone) *
			  zone_id_index);

	for (t = 0; t < nb_of_targets; t++) {

		pcur[t].peak_rate_kcps_per_spad =
			((pdci->per_tgt_results).peak_rate_kcps_per_spad)[offset + t];
#ifdef VL53L5_MEDIAN_PHASE_ON
		pcur[t].median_phase =
			((pdci->per_tgt_results).median_phase)[offset + t];
#endif
#ifdef VL53L5_RATE_SIGMA_KCPS_PER_SPAD_ON
		pcur[t].rate_sigma_kcps_per_spad =
			((pdci->per_tgt_results).rate_sigma_kcps_per_spad)[offset + t];
#endif
		pcur[t].target_zscore =
			((pdci->per_tgt_results).target_zscore)[offset + t];
		pcur[t].range_sigma_mm =
			((pdci->per_tgt_results).range_sigma_mm)[offset + t];
		pcur[t].median_range_mm =
			((pdci->per_tgt_results).median_range_mm)[offset + t];
#ifdef VL53L5_START_RANGE_MM_ON
		pcur[t].start_range_mm =
			((pdci->per_tgt_results).start_range_mm)[offset + t];
#endif
#ifdef VL53L5_END_RANGE_MM_ON
		pcur[t].end_range_mm =
			((pdci->per_tgt_results).end_range_mm)[offset + t];
#endif
#ifdef VL53L5_MIN_RANGE_DELTA_MM_ON
		pcur[t].min_range_delta_mm =
			((pdci->per_tgt_results).min_range_delta_mm)[offset + t];
#endif
#ifdef VL53L5_MAX_RANGE_DELTA_MM_ON
		pcur[t].max_range_delta_mm =
			((pdci->per_tgt_results).max_range_delta_mm)[offset + t];
#endif
#ifdef VL53L5_TARGET_REFLECTANCE_EST_PC_ON
		pcur[t].target_reflectance_est_pc =
			((pdci->per_tgt_results).target_reflectance_est_pc)[offset + t];
#endif
		pcur[t].target_status =
			((pdci->per_tgt_results).target_status)[offset + t];
	}

	return nb_of_targets;

}

void vl53l5_aff_comp_aff_matrix(
	int32_t                        nb_current_targets,
	int32_t                        nb_history_targets,
	int32_t                        iir_enabled,
	struct vl53l5_aff_cfg_dev_t    *pcfg_dev,
	struct vl53l5_aff_int_dev_t    *pint_dev,
	struct vl53l5_aff_err_dev_t    *perr_dev
)
{

	int32_t                      i;
	int32_t                      k;
	struct vl53l5_aff_tgt_out_t  avg_tgt;
	int32_t                      aff_dist;
	int32_t                      *pmat;
	struct vl53l5_aff_tgt_data_t *pcur;
	struct vl53l5_aff_tgt_data_t *previous;
	struct vl53l5_aff_tgt_hist_t *phist_tgts;
	int16_t                      median_range_mm;
	int32_t                      nb_past  =
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_NB_PAST_IDX];

	if ((nb_current_targets == 0) && (nb_history_targets == 0))
		return;

	pcur        = vl53l5_aff_cur_tgt_arr(pint_dev);
	pmat        = vl53l5_aff_affinity_matrix(pint_dev);
	phist_tgts  = vl53l5_aff_filled_tgt_hist_arr(pint_dev);

	for (k = 0; k < nb_history_targets; k++) {

		if (iir_enabled == 0) {
			vl53l5_aff_avg_tgt_hist(
				NULL,
				&(phist_tgts[k]),
				pcfg_dev,
				pint_dev,
				&avg_tgt,
				perr_dev
			);
			median_range_mm = avg_tgt.median_range_mm;
		} else {
			previous         = &((phist_tgts[k].pdata_arr)[nb_past - 1]);
			median_range_mm  = previous->median_range_mm;
		}

#if defined(AFF_FUNCTIONS_DEBUG)
		printf("    Compute affinity matrix hist(%d).\n", k);
		printf("          previous range:      %20d  %f\n", (median_range_mm),
			   (double)(median_range_mm) / pow(2, 2));
#endif

		for (i = 0; i < nb_current_targets; i++) {
			aff_dist = pcur[i].median_range_mm - median_range_mm;
#if defined(AFF_FUNCTIONS_DEBUG)
			printf("          cur range:      %20d  %f\n", (pcur[i].median_range_mm),
				   (double)(pcur[i].median_range_mm) / pow(2, 2));
#endif
			if (aff_dist < 0) {

				aff_dist = -aff_dist;
			}

			if (aff_dist > (pcfg_dev->aff_cfg).aff__affinity_param0) {

				aff_dist = 0x7FFFFFFF;
			} else if ((pcfg_dev->aff_cfg).aff__affinity_param1 == 2) {
				aff_dist = aff_dist * aff_dist;

			}

			pmat[i * nb_history_targets + k] = aff_dist;
		}
	}

}

int32_t vl53l5_aff_lzc_uint64(uint64_t x)
{
	if ((x >> 32) == 0)
		return (32 + LEADING_ZERO_COUNT((uint32_t)(x)));
	else
		return LEADING_ZERO_COUNT((uint32_t)(x >> 32));
}

int32_t vl53l5_aff_lzc_int64(int64_t x)
{
	if (x < 0)
		x = -x;
	return (vl53l5_aff_lzc_uint64((uint64_t)(x)) - 1);
}

void vl53l5_aff_mult_int64(
	int64_t   a,
	int32_t   frac_a,
	int64_t   b,
	int32_t   frac_b,
	int64_t   *pres,
	int32_t   *pfrac_res
)
{

	uint64_t a0;
	uint64_t b0;
	uint64_t tp64u1;
	uint64_t tp64u2;
	uint64_t res;
	int32_t  lz;
	int32_t  frac_res;

	a0   = a < 0 ? (uint64_t)(-a) : (uint64_t)(a);
	b0   = b < 0 ? (uint64_t)(-b) : (uint64_t)(b);

	tp64u1 = MUL_32X32_64((uint32_t)(a0 & 0xFFFFffff), (uint32_t)(b0 & 0xFFFFffff));
	tp64u2 = MUL_32X32_64((uint32_t)(a0 >> 32), (uint32_t)(b0 & 0xFFFFffff))
			 + MUL_32X32_64((uint32_t)(b0 >> 32), (uint32_t)(a0 & 0xFFFFffff))
			 + (tp64u1 >> 32);
	tp64u1 = (tp64u1 & 0xFFFFffff) | (tp64u2 << 32);
	tp64u2 = MUL_32X32_64((uint32_t)(a0 >> 32),
						  (uint32_t)(b0 >> 32)) + (tp64u2 >> 32);

	if (tp64u2 > 0) {
		lz          = vl53l5_aff_lzc_int64((int64_t)(tp64u2));
		res         = (tp64u2 << lz) | (tp64u1 >> (64 - lz));
		frac_res    = (frac_a + frac_b) - (64 - lz);
	}

	else {

		if ((int64_t)(tp64u1) >= 0) {
			res       = tp64u1;
			frac_res  = frac_a + frac_b;
		}

		else {
			res       = (tp64u1 >> 1);
			frac_res  = (frac_a + frac_b) - 1;
		}
	}

	if (frac_res < 0) {
		res  = 0x7FFFffffFFFFffff;
		frac_res = 0;
	}

	if (frac_res > 62) {
		res      = ((res) >> (frac_res - 62));
		frac_res = 62;
	}

	if ((int64_t)((uint64_t)(a) ^ (uint64_t)(b)) >= 0)
		*pres       = (int64_t)(res);

	else
		*pres       = -(int64_t)(res);
	*pfrac_res = frac_res;

}

void vl53l5_aff_add_int64(
	int64_t x,
	int32_t xn,
	int64_t y,
	int32_t yn,
	int64_t *pr,
	int32_t *prn)
{

	uint64_t ux;
	uint64_t res;
	int64_t  tp64s;
	int32_t  tp32s;
	int32_t  r;

	if (xn > yn) {
		tp64s = y;
		y     = x;
		x     = tp64s;
		tp32s = yn;
		yn    = xn;
		xn    = tp32s;
	}

	res = (uint64_t)(x) + ((((uint64_t)(y)) >> (yn - xn)) | (((int64_t)(((uint64_t)(
							   y))) < 0) ? ~(((uint64_t)(0xFFFFffffFFFFffff)) >> (yn - xn)) : 0));

	if ((int64_t)((((uint64_t)(x)) ^ res) & (((uint64_t)(y)) ^ res)) >= 0) {

		ux = (uint64_t)(res);
		if ((int64_t)(res) < 0)
			ux = -res;
		ux  = (ux >> xn) | 0x1;
		r   = vl53l5_aff_lzc_uint64(ux) - 1 - xn;

		res = res << r;

		res = res + (((((uint64_t)(y) << (63 - (yn - xn))) & 0x7FFFffffFFFFffff) >>
					  (63 - r)));

		xn  += r;

	}

	else {

		if (xn > 0) {
			res = (res >> 1) | ((uint64_t)(x) & 0x8000000000000000);
			xn--;
		}

		else
			res = (((uint64_t)(x)) >> 63) + (0x7FFFFFFFffffFFFF);
	}

	*pr   = (int64_t)(res);
	*prn  = xn;
	return;

}

uint32_t vl53l5_aff_avg_sigma(
	int64_t  num,
	int32_t  num_frac,
	int64_t  den,
	int32_t  out_frac,
	uint32_t sat
)
{

	int32_t                 temp32s_1;

	temp32s_1  = vl53l5_aff_lzc_int64(num >> num_frac) - num_frac;
	num        = num << temp32s_1;

	den        = den * den;

	den        = den >> 10;

	if (den > 0) {

		num  = (DIVS_64D64_64(num, den));
		num_frac = num_frac + temp32s_1 - 8;

		if (num_frac < 0)
			return (uint32_t)(sat);

		temp32s_1  = vl53l5_aff_lzc_int64(num >> num_frac) - num_frac;
		if (temp32s_1 < 32) {
			num = (num >> (32 - temp32s_1));
			num_frac = num_frac - ((32 - temp32s_1));
		}
		if (num_frac & 0x1) {
			num = (num >> 1);
			num_frac = num_frac - 1;
		}
		num      = INTEGER_SQRT((uint32_t)(num));
		num_frac = num_frac / 2;
		if (num_frac  > out_frac)
			num = (num >> (num_frac  - out_frac));
		else
			num = (num << (out_frac - (num_frac)));
	} else
		num = (uint32_t)(sat);
	return (uint32_t)(num);

}

void vl53l5_aff_int64_to_32(
	int64_t   a,
	int32_t   frac_a,
	int32_t   *pres,
	int32_t   *pfrac_res
)
{

	uint64_t a0;
	int32_t  lz;

	a0  = a < 0 ? (uint64_t)(-a) : (uint64_t)(a);

	lz  = vl53l5_aff_lzc_uint64((a0 >> frac_a) | 0x1) - 1 - frac_a;

	a0  = (a0 << lz);

	*pfrac_res  = frac_a + lz - 32;

	if (*pfrac_res < 0) {
		*pres       = (a < 0) + (0x7FFFffff);
		*pfrac_res  = 0;
		return;
	}

	*pres      = a < 0 ? (-(int32_t)(a0 >> 32)) : ((int32_t)(a0 >> 32));

}

int64_t vl53l5_aff_div(int64_t x, int64_t y)
{
	if (x < 0) {

		if (((int64_t)(0x8000000000000000) - x) < -(y / 2))
			x = x - y / 2;
		else
			x = (int64_t)(0x8000000000000000);
	} else {

		if (((int64_t)(0x7FFFffffFFFFffff) - x) > (y / 2))
			x = x + y / 2;
		else
			x = (int64_t)(0x7FFFffffFFFFffff);
	}

	return (int64_t)(DIVS_64D64_64(x, y));
}

int32_t vl53l5_aff_tweak_mirroring_calc(
	int32_t                        candidate_zscore,
	struct vl53l5_aff_tgt_hist_t          *ptgt_hist,
	struct vl53l5_aff_cfg_dev_t    *pcfg_dev,
	struct vl53l5_aff_int_dev_t    *pint_dev,
	struct vl53l5_aff_err_dev_t    *perr_dev
)
{

	int32_t                       k;
	int32_t                       temp32s_1;
	int32_t                       iir_enabled;
	int64_t                       zmissing;
	int64_t                       zscore;
	int64_t                       sum64z;
	struct vl53l5_aff_tgt_data_t  *pdat;
	int32_t                       ret_zscore = candidate_zscore;
	int32_t                       nb_past    =
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_NB_PAST_IDX];

	(void)(perr_dev);

	if (IS_BIT_SET_UINT8((pcfg_dev->aff_cfg).aff__avg_option, AFF__IIR_ENABLED)) {
		nb_past       = nb_past - 1;

		iir_enabled   = 1;
	} else
		iir_enabled   = 0;

	if (ptgt_hist != NULL) {
		temp32s_1 = 0;
		for (k = 0; k < nb_past ; k = k + 1) {
			pdat = vl53l5_aff_passed_data(pint_dev, ptgt_hist, k, iir_enabled);
			if (pdat->filled == 1)
				temp32s_1 = temp32s_1 + 1;
		}

		if ((temp32s_1 < nb_past) && (temp32s_1 > 0)) {
			zmissing = 0;
			sum64z   = 0;
			zscore   = 0;
			for (k = 0; k < nb_past ; k = k + 1) {
				pdat = vl53l5_aff_passed_data(pint_dev, ptgt_hist, k, iir_enabled);
				if (pdat->filled == 1) {
					zmissing   = zmissing + ((int64_t)(pdat->target_zscore) << 15) * ((
									 pcfg_dev->aff_arrayed_cfg).aff__temporal_weights[k + 1]);
					sum64z     = sum64z + (pcfg_dev->aff_arrayed_cfg).aff__temporal_weights[k + 1];
				}
			}

			if (sum64z != 0) {
				zmissing   = vl53l5_aff_div(zmissing, sum64z);

				if (zmissing > ((int64_t)((pcfg_dev->aff_cfg).aff__min_zscore_for_in) << 15))
					zmissing   = (2 * ((int64_t)((pcfg_dev->aff_cfg).aff__min_zscore_for_in) << 15))
								 - zmissing;
				else
					zmissing = (int64_t)((pcfg_dev->aff_cfg).aff__min_zscore_for_in) << 15;

				zmissing   = zmissing + ((int64_t)((
													   pcfg_dev->aff_cfg).aff__non_matched_history_tweak_param) << 15);
			}

			zscore     = (zmissing) * ((
										   pcfg_dev->aff_arrayed_cfg).aff__temporal_weights[0]);
			sum64z     = (pcfg_dev->aff_arrayed_cfg).aff__temporal_weights[0];

			for (k = 0; k < nb_past ; k = k + 1) {
				pdat = vl53l5_aff_passed_data(pint_dev, ptgt_hist, k, iir_enabled);
				if (pdat->filled == 1)
					zscore   = zscore + ((int64_t)(pdat->target_zscore) << 15) * ((
								   pcfg_dev->aff_arrayed_cfg).aff__temporal_weights[k + 1]);
				else
					zscore   = zscore + (zmissing) * ((
														  pcfg_dev->aff_arrayed_cfg).aff__temporal_weights[k + 1]);
				sum64z   = sum64z + (pcfg_dev->aff_arrayed_cfg).aff__temporal_weights[k + 1];
			}

			ret_zscore             = (int32_t)(vl53l5_aff_div(zscore, sum64z));
			if (ret_zscore >= ((pcfg_dev->aff_cfg).aff__min_zscore_for_out << 15))
				ret_zscore = candidate_zscore;
			else {
				for (k = 0; k < nb_past ; k = k + 1) {
					pdat = vl53l5_aff_passed_data(pint_dev, ptgt_hist, k, iir_enabled);
					pdat->filled = 0;
				}
			}
		}
	}

	return ret_zscore;
}

int32_t vl53l5_aff_iir(int32_t x, int32_t y, int32_t coeff, int32_t max,
					   int32_t min)
{

	int32_t temp32s_1;

	temp32s_1 = (coeff * x + (256 - coeff) * y);
	if (temp32s_1 >= 0)
		temp32s_1 = (temp32s_1 + 128) / 256;
	else
		temp32s_1 = (temp32s_1 - 128) / 256;

	if (temp32s_1 > max)
		temp32s_1 = max;
	if (temp32s_1 < min)
		temp32s_1 = min;

	return temp32s_1;

}

void vl53l5_aff_avg_tgt_hist(
	struct vl53l5_aff_tgt_data_t   *pcur_tgt,
	struct vl53l5_aff_tgt_hist_t   *ptgt_hist,
	struct vl53l5_aff_cfg_dev_t    *pcfg_dev,
	struct vl53l5_aff_int_dev_t    *pint_dev,
	struct vl53l5_aff_tgt_out_t    *pavg_tgt,
	struct vl53l5_aff_err_dev_t    *perr_dev
)
{

	int32_t                      k;
	uint32_t                     temp32u_1;
	int32_t                      temp32s_1;
	int32_t                      temp32s_2;
	int32_t                      nb_merged;
	int32_t                      nb_valid;
	int32_t                      first_status_idx;
	int32_t                      first_status_val;
#ifdef VL53L5_RATE_SIGMA_KCPS_PER_SPAD_ON
	int64_t                      rate_sig;
	int32_t                      rate_sig_frac;
#endif
	int64_t                      range_sigma_mm;
	int32_t                      range_sigma_mm_frac;
	int64_t                      double_weight;
	int64_t                      peak_rate;
	int64_t                      zscore;
	int64_t                      zmissing;
#ifdef VL53L5_MEDIAN_PHASE_ON
	int64_t                      median_phase;
#endif
	int64_t                      median_range_mm;
#ifdef VL53L5_START_RANGE_MM_ON
	int64_t                      start_range_mm;
#endif
#ifdef VL53L5_END_RANGE_MM_ON
	int64_t                      end_range_mm;
#endif
#ifdef VL53L5_MIN_RANGE_DELTA_MM_ON
	int64_t                      min_range_delta_mm;
#endif
#ifdef VL53L5_MAX_RANGE_DELTA_MM_ON
	int64_t                      max_range_delta_mm;
#endif
#ifdef VL53L5_TARGET_REFLECTANCE_EST_PC_ON
	int64_t                      refl;
#endif
	int64_t                      temp64s_1;
	int64_t                      temp64s_2;
	int64_t                      temp64s_3;
	int64_t                      temp64s_5;
	int64_t                      sum64w;
	int64_t                      sum64z;
	int64_t                      sum64wsq;
	int32_t                      iir_enabled;
	int32_t                      past_empty;
	int32_t                      coeff;
	struct vl53l5_aff_tgt_data_t *previous_out;
	struct vl53l5_aff_tgt_data_t *pdat;
	int32_t                      nb_past  =
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_NB_PAST_IDX];
	struct common_grp__status_t  *perror  = &(perr_dev->aff_error_status);

#if defined(AFF_FUNCTIONS_DEBUG)
	printf("        vl53l5_aff_avg_tgt_hist.\n");
#endif

	if ((pcur_tgt == NULL) && (ptgt_hist == NULL)) {
		perror->status__stage_id    = VL53L5_AFF_STAGE_ID__AVG_TGT_HIST;
		perror->status__type        = VL53L5_AFF_ERROR__INVALID_PARAM;
		goto exit;
	}

	previous_out  = NULL;
	if (IS_BIT_SET_UINT8((pcfg_dev->aff_cfg).aff__avg_option, AFF__IIR_ENABLED)) {
		coeff         = (pcfg_dev->aff_cfg).aff__iir_alpha;

		nb_past       = nb_past - 1;

		iir_enabled   = 1;
		if (ptgt_hist != NULL)
			previous_out  = &((ptgt_hist->pdata_arr)[nb_past]);

	} else {
		iir_enabled   = 0;
		coeff         = 0;

	}
	peak_rate               = 0;
#ifdef VL53L5_MEDIAN_PHASE_ON
	median_phase            = 0;
#endif
	median_range_mm         = 0;
#ifdef VL53L5_START_RANGE_MM_ON
	start_range_mm          = 0;
#endif
#ifdef VL53L5_END_RANGE_MM_ON
	end_range_mm            = 0;
#endif
#ifdef VL53L5_MIN_RANGE_DELTA_MM_ON
	min_range_delta_mm      = 0;
#endif
#ifdef VL53L5_MAX_RANGE_DELTA_MM_ON
	max_range_delta_mm      = 0;
#endif
#ifdef VL53L5_TARGET_REFLECTANCE_EST_PC_ON
	refl                    = 0;
#endif
	range_sigma_mm          = 0;
	range_sigma_mm_frac     = 0;
	temp64s_5               = 0;
	sum64z                  = 0;
	sum64w                  = 0;
	sum64wsq                = 0;
#ifdef VL53L5_RATE_SIGMA_KCPS_PER_SPAD_ON
	rate_sig                = 0;
	rate_sig_frac           = 0;
#endif
	zscore                  = 0;
	nb_merged               = 0;
	nb_valid                = 0;
	first_status_idx        = -1;
	first_status_val        = 0;
	pavg_tgt->target_zscore = 0;

	past_empty              = 1;
	if (ptgt_hist != NULL) {
		for (k = 0; k < nb_past ; k = k + 1) {
			pdat = vl53l5_aff_passed_data(pint_dev, ptgt_hist, k, iir_enabled);
			if (pdat->filled == 1) {
				past_empty          = 0;
				break;
			}
		}
	}

	if (pcur_tgt == NULL) {
		if (past_empty == 0) {
			if (IS_BIT_SET_UINT8((pcfg_dev->aff_cfg).aff__avg_option,
								 AFF__MIRRORING_TYPE)) {
				if (previous_out != NULL)
					temp64s_1 = (int64_t)((int32_t)((uint32_t)((uint16_t)(
													previous_out->target_zscore))
												+ (((uint32_t)((uint16_t)(previous_out->filled))) << 16)));
				else {
					perror->status__stage_id    = VL53L5_AFF_STAGE_ID__AVG_TGT_HIST;
					perror->status__type        = VL53L5_AFF_ERROR__INVALID_PARAM;
					goto exit;
				}

#if defined(AFF_FUNCTIONS_DEBUG)
				printf("            prev out zscore:        %f\n", (double)(temp64s_1) / pow(2,
						20));
				printf("            aff__min_zscore_for_in: %f\n",
					   (double)((pcfg_dev->aff_cfg).aff__min_zscore_for_in) / pow(2, 5));
#endif

				if (temp64s_1 >= ((int64_t)((pcfg_dev->aff_cfg).aff__min_zscore_for_in) <<
								  15)) {
					zmissing   = (int64_t)(2 * ((int64_t)((
							pcfg_dev->aff_cfg).aff__min_zscore_for_in) << 15) - temp64s_1);
#if defined(AFF_FUNCTIONS_DEBUG)
					printf("            zmissing 1:            %f\n", (double)(zmissing) / pow(2,
							20));
#endif

				} else {
					zmissing      = (int64_t)((pcfg_dev->aff_cfg).aff__zscore_missing_tgt) << 15;
#if defined(AFF_FUNCTIONS_DEBUG)
					printf("            zmissing 2:            %f\n", (double)(zmissing) / pow(2,
							20));
#endif

				}

			} else {
				zmissing = 0;
				for (k = 0; k < nb_past ; k = k + 1) {
					pdat = vl53l5_aff_passed_data(pint_dev, ptgt_hist, k, iir_enabled);
					if (pdat->filled == 1) {
						zmissing   = zmissing + ((int64_t)(pdat->target_zscore) << 15) * ((
										 pcfg_dev->aff_arrayed_cfg).aff__temporal_weights[k + 1]);
						sum64z     = sum64z + (pcfg_dev->aff_arrayed_cfg).aff__temporal_weights[k + 1];
#if defined(AFF_FUNCTIONS_DEBUG)
						printf("            pdat->target_zscore:    %f\n",
							   (double)(((int64_t)(pdat->target_zscore) << 15)) / pow(2, 20));
#endif
					}
				}
				if (sum64z != 0) {
					zmissing   = vl53l5_aff_div(zmissing, sum64z);
					if (zmissing > ((int64_t)((pcfg_dev->aff_cfg).aff__min_zscore_for_in) << 15))
						zmissing = (2 * ((int64_t)((pcfg_dev->aff_cfg).aff__min_zscore_for_in) << 15)) -
								   zmissing;
					else
						zmissing = (int64_t)((pcfg_dev->aff_cfg).aff__min_zscore_for_in) << 15;
				}
#if defined(AFF_FUNCTIONS_DEBUG)
				printf("            zmissing 3:                %f\n",
					   (double)(zmissing) / pow(2, 20));
#endif

			}

		} else {

			if (iir_enabled == 0) {
				perror->status__stage_id    = VL53L5_AFF_STAGE_ID__AVG_TGT_HIST;
				perror->status__type        = VL53L5_AFF_ERROR__INVALID_PARAM;
				goto exit;
			}

			if (IS_BIT_SET_UINT8((pcfg_dev->aff_cfg).aff__avg_option,
								 AFF__MIRRORING_TYPE)) {

				if (previous_out != NULL)
					temp64s_1 = (int64_t)((int32_t)((uint32_t)((uint16_t)(previous_out->target_zscore))
							+ (((uint32_t)((uint16_t)(previous_out->filled))) << 16)));
				else {
					perror->status__stage_id    = VL53L5_AFF_STAGE_ID__AVG_TGT_HIST;
					perror->status__type        = VL53L5_AFF_ERROR__INVALID_PARAM;
					goto exit;
				}
				if (temp64s_1 > ((int64_t)((pcfg_dev->aff_cfg).aff__min_zscore_for_in) << 15))
					zmissing   = (int64_t)(2 * ((int64_t)((
							pcfg_dev->aff_cfg).aff__min_zscore_for_in) << 15) - temp64s_1);
				else {
					zmissing   = temp64s_1;
#if defined(AFF_FUNCTIONS_DEBUG)
					printf("            zmissing 4:                %f\n",
						   (double)(zmissing) / pow(2, 20));
#endif
				}
			} else {
				zmissing       = (int64_t)(((int64_t)((
						pcfg_dev->aff_cfg).aff__zscore_missing_tgt) << 15));
#if defined(AFF_FUNCTIONS_DEBUG)
				printf("            zmissing 5:                %f\n",
					   (double)(zmissing) / pow(2, 20));
#endif

			}

		}

		zscore     = (zmissing) * ((
									   pcfg_dev->aff_arrayed_cfg).aff__temporal_weights[0]);
		sum64z     = (pcfg_dev->aff_arrayed_cfg).aff__temporal_weights[0];
	} else {

#if defined(AFF_FUNCTIONS_DEBUG)
		printf("            past_empty:                %d\n", past_empty);
#endif
		if (ptgt_hist == NULL) {
			zmissing      = (int64_t)((pcfg_dev->aff_cfg).aff__zscore_missing_tgt) << 15;
			zmissing      = (int64_t)(((int64_t)((
					pcfg_dev->aff_cfg).aff__zscore_missing_tgt) << 15));
#if defined(AFF_FUNCTIONS_DEBUG)
			printf("            zmissing 6:                %f\n",
				   (double)(zmissing) / pow(2, 20));
#endif

		} else {
			if (IS_BIT_SET_UINT8((pcfg_dev->aff_cfg).aff__avg_option,
								 AFF__MIRRORING_TYPE)) {

				if (((pcfg_dev->aff_cfg).aff__min_zscore_for_out >
					 (pcfg_dev->aff_cfg).aff__min_zscore_for_in)
					|| (coeff == 0))
					zmissing      = (int64_t)((pcfg_dev->aff_cfg).aff__zscore_missing_tgt) << 15;
				else {
					if (previous_out != NULL)
						zmissing     = (int64_t)((int32_t)((uint32_t)((uint16_t)(
															previous_out->target_zscore))
														+ (((uint32_t)((uint16_t)(previous_out->filled))) << 16)));
					else {
						perror->status__stage_id    = VL53L5_AFF_STAGE_ID__AVG_TGT_HIST;
						perror->status__type        = VL53L5_AFF_ERROR__INVALID_PARAM;
						goto exit;
					}

#if defined(AFF_FUNCTIONS_DEBUG)
					printf("            zmissing 7 (prev out):         %20d    %f\n", zmissing,
						   (double)(zmissing) / pow(2, 20));
#endif
				}
			} else {
				zmissing      = (int64_t)(((int64_t)((
						pcfg_dev->aff_cfg).aff__zscore_missing_tgt) << 15));
#if defined(AFF_FUNCTIONS_DEBUG)
				printf("            zmissing 8:                %f\n",
					   (double)(zmissing) / pow(2, 20));
#endif
			}
		}

		zscore     = ((int64_t)(pcur_tgt->target_zscore) << 15) * ((
						 pcfg_dev->aff_arrayed_cfg).aff__temporal_weights[0]);
		sum64z     = (pcfg_dev->aff_arrayed_cfg).aff__temporal_weights[0];
#if defined(AFF_FUNCTIONS_DEBUG)
		printf("            curr zscore:             %f\n",
			   (double)(((int64_t)(pcur_tgt->target_zscore) << 15)) / pow(2, 20));
		printf("            zmissing:                %f\n", (double)(zmissing) / pow(2,
				20));
#endif
	}

	if (ptgt_hist == NULL) {
		if ((iir_enabled == 0) || (pcur_tgt != NULL)) {
			for (k = 0; k < nb_past ; k = k + 1) {
				zscore   = zscore + (zmissing) * ((
													  pcfg_dev->aff_arrayed_cfg).aff__temporal_weights[k + 1]);
				sum64z   = sum64z + (pcfg_dev->aff_arrayed_cfg).aff__temporal_weights[k + 1];
			}
		}
	} else {
		for (k = 0; k < nb_past ; k = k + 1) {
			pdat = vl53l5_aff_passed_data(pint_dev, ptgt_hist, k, iir_enabled);
			if (pdat->filled == 1)
				zscore   = zscore + ((int64_t)(pdat->target_zscore) << 15) * ((
							   pcfg_dev->aff_arrayed_cfg).aff__temporal_weights[k + 1]);
			else
				zscore   = zscore + (zmissing) * ((
													  pcfg_dev->aff_arrayed_cfg).aff__temporal_weights[k + 1]);
			sum64z   = sum64z + (pcfg_dev->aff_arrayed_cfg).aff__temporal_weights[k + 1];
		}
	}

	pavg_tgt->target_zscore  = (int32_t)(vl53l5_aff_div(zscore, sum64z));

#if defined(AFF_FUNCTIONS_DEBUG)
	printf("            pavg_tgt->target_zscore: %20d %f\n",
		   pavg_tgt->target_zscore, (double)(pavg_tgt->target_zscore) / pow(2, 20));
#endif

	if ((iir_enabled == 1) && (previous_out != NULL)) {
		temp64s_1 = (int64_t)((int32_t)((uint32_t)((uint16_t)(
											previous_out->target_zscore))
										+ (((uint32_t)((uint16_t)(previous_out->filled))) << 16)));
		temp64s_2 = (int64_t)(pavg_tgt->target_zscore);

		temp64s_3 = ((int64_t)(coeff) * temp64s_1 + (int64_t)(256 - coeff) * temp64s_2);

		if (temp64s_3 >= 0)
			temp64s_3 = (temp64s_3 + 128) / 256;
		else
			temp64s_3 = (temp64s_3 - 128) / 256;

		pavg_tgt->target_zscore = (int32_t)temp64s_3;

#if defined(AFF_FUNCTIONS_DEBUG)
		printf("            pavg_tgt->target_zscore (after iir): %20d %f\n",
			   pavg_tgt->target_zscore, (double)(pavg_tgt->target_zscore) / pow(2, 20));
		printf("            coeff:                               %20d %f\n", coeff,
			   (double)(coeff) / pow(2, 8));
#endif

	}

	sum64w = 0;
	if (pcur_tgt != NULL) {
		double_weight       = ((pcfg_dev->aff_arrayed_cfg).aff__temporal_weights[0]) *
							  (pcur_tgt->target_zscore);

		peak_rate           = (int64_t)(pcur_tgt->peak_rate_kcps_per_spad) *
							  double_weight;

#ifdef VL53L5_MEDIAN_PHASE_ON
		median_phase        = (int64_t)(pcur_tgt->median_phase) * double_weight;
#endif
		median_range_mm     = (int64_t)(pcur_tgt->median_range_mm) *
							  double_weight;

#ifdef VL53L5_START_RANGE_MM_ON
		start_range_mm      = (int64_t)(pcur_tgt->start_range_mm) * double_weight;
#endif
#ifdef VL53L5_END_RANGE_MM_ON
		end_range_mm        = (int64_t)(pcur_tgt->end_range_mm) * double_weight;
#endif
#ifdef VL53L5_MIN_RANGE_DELTA_MM_ON
		min_range_delta_mm  = (int64_t)(pcur_tgt->min_range_delta_mm) * double_weight;
#endif
#ifdef VL53L5_MAX_RANGE_DELTA_MM_ON
		max_range_delta_mm  = (int64_t)(pcur_tgt->max_range_delta_mm) * double_weight;
#endif
#ifdef VL53L5_TARGET_REFLECTANCE_EST_PC_ON
		refl                = (int64_t)(pcur_tgt->target_reflectance_est_pc) *
							  double_weight;
#endif

#ifdef VL53L5_RATE_SIGMA_KCPS_PER_SPAD_ON
		temp64s_5           = (int64_t)(pcur_tgt->rate_sigma_kcps_per_spad) *
							  double_weight;

		vl53l5_aff_mult_int64(temp64s_5, 20, temp64s_5, 20, &temp64s_5,
							  &temp32s_1);

		vl53l5_aff_add_int64(rate_sig, rate_sig_frac, temp64s_5, temp32s_1, &rate_sig,
							 &rate_sig_frac);

#endif

		temp64s_5           = (int64_t)(pcur_tgt->range_sigma_mm) *
							  double_weight;

		vl53l5_aff_mult_int64(temp64s_5, 16, temp64s_5, 16, &temp64s_5,
							  &temp32s_1);

		vl53l5_aff_add_int64(range_sigma_mm, range_sigma_mm_frac, temp64s_5, temp32s_1,
							 &range_sigma_mm,
							 &range_sigma_mm_frac);

		sum64w              = sum64w + double_weight;
		sum64wsq            = sum64wsq + (double_weight * double_weight);
		if (first_status_idx == -1) {
			first_status_idx = 0;
			first_status_val = pcur_tgt->target_status;
		}
		if (pcur_tgt->target_status == DCI_TARGET_STATUS__RANGECOMPLETE_MERGED_PULSE)
			nb_merged = nb_merged + 1;
		if (pcur_tgt->target_status == DCI_TARGET_STATUS__RANGECOMPLETE)
			nb_valid = nb_valid + 1;

	}
	if (ptgt_hist != NULL) {

		for (k = 0; k < nb_past ; k = k + 1) {
			pdat = vl53l5_aff_passed_data(pint_dev, ptgt_hist, k, iir_enabled);
			if (pdat->filled == 1) {
				past_empty          = 0;
				double_weight       = ((pcfg_dev->aff_arrayed_cfg).aff__temporal_weights[k + 1])
									  * (pdat->target_zscore);
				peak_rate           = peak_rate + (int64_t)(pdat->peak_rate_kcps_per_spad) *
									  double_weight;
#ifdef VL53L5_MEDIAN_PHASE_ON
				median_phase        = median_phase + (int64_t)(pdat->median_phase) *
									  double_weight;
#endif
				median_range_mm     = median_range_mm + (int64_t)(pdat->median_range_mm) *
									  double_weight;
#ifdef VL53L5_START_RANGE_MM_ON
				start_range_mm      = start_range_mm + (int64_t)(pdat->start_range_mm) *
									  double_weight;
#endif
#ifdef VL53L5_END_RANGE_MM_ON
				end_range_mm        = end_range_mm + (int64_t)(pdat->end_range_mm) *
									  double_weight;
#endif
#ifdef VL53L5_MIN_RANGE_DELTA_MM_ON
				min_range_delta_mm  = min_range_delta_mm + (int64_t)(pdat->min_range_delta_mm) *
									  double_weight;
#endif
#ifdef VL53L5_MAX_RANGE_DELTA_MM_ON
				max_range_delta_mm  = max_range_delta_mm + (int64_t)(pdat->max_range_delta_mm) *
									  double_weight;
#endif
#ifdef VL53L5_TARGET_REFLECTANCE_EST_PC_ON
				refl                = refl + (int64_t)(pdat->target_reflectance_est_pc) *
									  double_weight;
#endif

#ifdef VL53L5_RATE_SIGMA_KCPS_PER_SPAD_ON
				temp64s_5           = (int64_t)(pdat->rate_sigma_kcps_per_spad) *
									  double_weight;

				vl53l5_aff_mult_int64(temp64s_5, 20, temp64s_5, 20, &temp64s_5,
									  &temp32s_1);

				vl53l5_aff_add_int64(rate_sig, rate_sig_frac, temp64s_5, temp32s_1, &rate_sig,
									 &rate_sig_frac);

#endif

				temp64s_5           = (int64_t)(pdat->range_sigma_mm) *
									  double_weight;

				vl53l5_aff_mult_int64(temp64s_5, 16, temp64s_5, 16, &temp64s_5,
									  &temp32s_1);

				vl53l5_aff_add_int64(range_sigma_mm, range_sigma_mm_frac, temp64s_5, temp32s_1,
									 &range_sigma_mm,
									 &range_sigma_mm_frac);

				sum64w              = sum64w + double_weight;
				sum64wsq            = sum64wsq + (double_weight * double_weight);
				if (first_status_idx == -1) {
					first_status_idx = 1;
					first_status_val = pdat->target_status;
				}
				if (pdat->target_status == DCI_TARGET_STATUS__RANGECOMPLETE_MERGED_PULSE)
					nb_merged = nb_merged + 1;
				if (pdat->target_status == DCI_TARGET_STATUS__RANGECOMPLETE)
					nb_valid = nb_valid + 1;
			}
		}

		if ((previous_out != NULL)
			&& (IS_BIT_SET_UINT8((pcfg_dev->aff_cfg).aff__avg_option,
								 AFF__USE_PREV_OUT_IN_STATUS_DERIVATION))
			&& (iir_enabled == 1)) {
			if (((pcur_tgt == NULL) && (past_empty == 0)) || (pcur_tgt != NULL)) {
				if (first_status_idx == -1) {
					first_status_idx = 0;
					first_status_val = previous_out->target_status;
				}
				if (previous_out->target_status ==
					DCI_TARGET_STATUS__RANGECOMPLETE_MERGED_PULSE)
					nb_merged = nb_merged + 1;
				if (previous_out->target_status == DCI_TARGET_STATUS__RANGECOMPLETE)
					nb_valid = nb_valid + 1;
			}

		}
	}
	if (sum64w != 0 && sum64wsq != 0) {
		peak_rate = vl53l5_aff_div(peak_rate, sum64w);

#ifdef VL53L5_MEDIAN_PHASE_ON
		median_phase = vl53l5_aff_div(median_phase, sum64w);
#endif

		median_range_mm = vl53l5_aff_div(median_range_mm, sum64w);

#ifdef VL53L5_START_RANGE_MM_ON
		start_range_mm = vl53l5_aff_div(start_range_mm, sum64w);
#endif

#ifdef VL53L5_END_RANGE_MM_ON
		end_range_mm = vl53l5_aff_div(end_range_mm, sum64w);
#endif

#ifdef VL53L5_MIN_RANGE_DELTA_MM_ON
		min_range_delta_mm = vl53l5_aff_div(min_range_delta_mm, sum64w);
#endif

#ifdef VL53L5_MAX_RANGE_DELTA_MM_ON
		max_range_delta_mm = vl53l5_aff_div(max_range_delta_mm, sum64w);
#endif

#ifdef VL53L5_TARGET_REFLECTANCE_EST_PC_ON
		refl = vl53l5_aff_div(refl, sum64w);
#endif

#ifdef VL53L5_RATE_SIGMA_KCPS_PER_SPAD_ON
		rate_sig                            = vl53l5_aff_avg_sigma(rate_sig,
											  rate_sig_frac, sum64w, 11, 0x3FFFFFFF);
		pavg_tgt->rate_sigma_kcps_per_spad  = (uint32_t)(rate_sig);
#endif

		range_sigma_mm                      = vl53l5_aff_avg_sigma(range_sigma_mm,
											  range_sigma_mm_frac, sum64w, 7, 0x7FFF);
		pavg_tgt->range_sigma_mm            = (uint16_t)(range_sigma_mm);
	}

	pavg_tgt->peak_rate_kcps_per_spad    = (uint32_t)(peak_rate);
#ifdef VL53L5_MEDIAN_PHASE_ON
	pavg_tgt->median_phase               = (uint32_t)(median_phase);
#endif
	pavg_tgt->median_range_mm            = (int16_t)(median_range_mm);
#ifdef VL53L5_START_RANGE_MM_ON
	pavg_tgt->start_range_mm             = (int16_t)(start_range_mm);
#endif
#ifdef VL53L5_END_RANGE_MM_ON
	pavg_tgt->end_range_mm               = (int16_t)(end_range_mm);
#endif
#ifdef VL53L5_MIN_RANGE_DELTA_MM_ON
	pavg_tgt->min_range_delta_mm         = (uint8_t)(min_range_delta_mm);
#endif
#ifdef VL53L5_MAX_RANGE_DELTA_MM_ON
	pavg_tgt->max_range_delta_mm         = (uint8_t)(max_range_delta_mm);
#endif
#ifdef VL53L5_TARGET_REFLECTANCE_EST_PC_ON
	pavg_tgt->target_reflectance_est_pc  = (uint8_t)(refl);
#endif
#ifdef VL53L5_RATE_SIGMA_KCPS_PER_SPAD_ON
	pavg_tgt->rate_sigma_kcps_per_spad   = (uint32_t)(rate_sig);
#endif
	pavg_tgt->range_sigma_mm             = (uint16_t)(range_sigma_mm);

	if ((iir_enabled == 1) && (previous_out != NULL)) {
		if ((past_empty == 1) && (pcur_tgt == NULL)) {
			pavg_tgt->peak_rate_kcps_per_spad    = previous_out->peak_rate_kcps_per_spad;
#ifdef VL53L5_MEDIAN_PHASE_ON
			pavg_tgt->median_phase               = previous_out->median_phase;
#endif
			pavg_tgt->median_range_mm            = previous_out->median_range_mm;
#ifdef VL53L5_START_RANGE_MM_ON
			pavg_tgt->start_range_mm             = previous_out->start_range_mm;
#endif
#ifdef VL53L5_END_RANGE_MM_ON
			pavg_tgt->end_range_mm               = previous_out->end_range_mm;
#endif
#ifdef VL53L5_MIN_RANGE_DELTA_MM_ON
			pavg_tgt->min_range_delta_mm         = previous_out->min_range_delta_mm;
#endif
#ifdef VL53L5_MAX_RANGE_DELTA_MM_ON
			pavg_tgt->max_range_delta_mm         = previous_out->max_range_delta_mm;
#endif
			pavg_tgt->rate_sigma_kcps_per_spad   = previous_out->rate_sigma_kcps_per_spad;
			pavg_tgt->range_sigma_mm             = previous_out->range_sigma_mm;
#ifdef VL53L5_TARGET_REFLECTANCE_EST_PC_ON
			pavg_tgt->target_reflectance_est_pc  = previous_out->target_reflectance_est_pc;
#endif
		} else if (IS_BIT_SET_UINT8((pcfg_dev->aff_cfg).aff__avg_option,
									AFF__IIR_AVG_ON_RNG_DATA)) {

			pavg_tgt->peak_rate_kcps_per_spad =
				(uint32_t)((((uint64_t)(coeff) * (uint64_t)(
								 previous_out->peak_rate_kcps_per_spad)
							 + (uint64_t)(256 - coeff) * ((uint64_t)(peak_rate))) + 128) >> 8);
			if (pavg_tgt->peak_rate_kcps_per_spad > 0x3FFFffff)
				pavg_tgt->peak_rate_kcps_per_spad = 0x3FFFffff;

#ifdef VL53L5_MEDIAN_PHASE_ON
			pavg_tgt->median_phase = (uint32_t)(vl53l5_aff_iir(
													(int32_t)(previous_out->median_phase),
													(int32_t)(median_phase),
													coeff, 0x7FFFF, 0));
#endif

			pavg_tgt->median_range_mm = (int16_t)(vl53l5_aff_iir(
					(int32_t)(previous_out->median_range_mm),
					(int32_t)(median_range_mm),
					coeff, VL53L5_INT16_MAX, VL53L5_INT16_MIN));

#ifdef VL53L5_START_RANGE_MM_ON
			pavg_tgt->start_range_mm = (int16_t)(vl53l5_aff_iir(
					(int32_t)(previous_out->start_range_mm),
					(int32_t)(start_range_mm),
					coeff, VL53L5_INT16_MAX, VL53L5_INT16_MIN));
#endif

#ifdef VL53L5_END_RANGE_MM_ON
			pavg_tgt->end_range_mm = (int16_t)(vl53l5_aff_iir(
												   (int32_t)(previous_out->end_range_mm),
												   (int32_t)(end_range_mm),
												   coeff, VL53L5_INT16_MAX, VL53L5_INT16_MIN));
#endif

#ifdef VL53L5_MIN_RANGE_DELTA_MM_ON
			pavg_tgt->min_range_delta_mm = (uint8_t)(vl53l5_aff_iir(
											   (int32_t)(previous_out->min_range_delta_mm),
											   (int32_t)(min_range_delta_mm),
											   coeff, VL53L5_UINT8_MAX, 0));
#endif

#ifdef VL53L5_MAX_RANGE_DELTA_MM_ON
			pavg_tgt->max_range_delta_mm = (uint8_t)(vl53l5_aff_iir(
											   (int32_t)(previous_out->max_range_delta_mm),
											   (int32_t)(max_range_delta_mm),
											   coeff, VL53L5_UINT8_MAX, 0));
#endif

#ifdef VL53L5_TARGET_REFLECTANCE_EST_PC_ON
			pavg_tgt->target_reflectance_est_pc = (uint8_t)(vl53l5_aff_iir(
					(int32_t)(previous_out->target_reflectance_est_pc),
					(int32_t)(refl),
					coeff, VL53L5_UINT8_MAX, 0));
#endif

#ifdef VL53L5_RATE_SIGMA_KCPS_PER_SPAD_ON

			temp64s_1 = (int64_t)(previous_out->rate_sigma_kcps_per_spad);

			temp64s_1 = temp64s_1 * temp64s_1;

			temp64s_2 = (int64_t)(coeff * coeff);

			vl53l5_aff_mult_int64(temp64s_1, 22, temp64s_2, 16, &temp64s_3, &temp32s_1);
			temp64s_1 = (int64_t)(rate_sig);

			temp64s_1 = temp64s_1 * temp64s_1;

			temp64s_2 = (int64_t)((256 - coeff) * (256 - coeff));

			vl53l5_aff_mult_int64(temp64s_1, 22, temp64s_2, 16, &temp64s_2, &temp32s_2);
			vl53l5_aff_add_int64(temp64s_3, temp32s_1, temp64s_2, temp32s_2, &temp64s_1,
								 &temp32s_1);
			vl53l5_aff_int64_to_32(temp64s_1, temp32s_1, &temp32s_1, &temp32s_2);
			if (temp32s_2 & 1) {

				temp32s_1 = (temp32s_1 <<
							 1);

				temp32s_2 = temp32s_2 + 1;
			}
			temp32u_1 = INTEGER_SQRT((uint32_t)(temp32s_1));
			temp32s_2 = (temp32s_2 >> 1);

			if ((temp32u_1 >> temp32s_2) > 0x7FFFF) {

				temp32u_1 = 0xFFFFffff;

			} else {
				if (temp32s_2 > 11) {

					temp32u_1 = temp32u_1 >> (temp32s_2 - 11);
				} else {

					temp32u_1 = temp32u_1 << (11 - temp32s_2);
				}
			}
			pavg_tgt->rate_sigma_kcps_per_spad  = (temp32u_1);
#endif

			temp64s_1 = (int64_t)(previous_out->range_sigma_mm);

			temp64s_1 = temp64s_1 * temp64s_1;

			temp64s_2 = (int64_t)(coeff * coeff);

			vl53l5_aff_mult_int64(temp64s_1, 14, temp64s_2, 16, &temp64s_3, &temp32s_1);
			temp64s_1 = (int64_t)(range_sigma_mm);

			temp64s_1 = temp64s_1 * temp64s_1;

			temp64s_2 = (int64_t)((256 - coeff) * (256 - coeff));

			vl53l5_aff_mult_int64(temp64s_1, 14, temp64s_2, 16, &temp64s_2, &temp32s_2);
			vl53l5_aff_add_int64(temp64s_3, temp32s_1, temp64s_2, temp32s_2, &temp64s_1,
								 &temp32s_1);
			vl53l5_aff_int64_to_32(temp64s_1, temp32s_1, &temp32s_1, &temp32s_2);
			if (temp32s_2 & 1) {

				temp32s_1 = (temp32s_1 <<
							 1);

				temp32s_2 = temp32s_2 + 1;
			}
			temp32u_1 = INTEGER_SQRT((uint32_t)(temp32s_1));
			temp32s_2 = (temp32s_2 >> 1);

			if ((temp32u_1 >> temp32s_2) > 0x1FF) {

				temp32u_1 = 0xFFFF;

			} else {
				if (temp32s_2 > 7) {

					temp32u_1 = temp32u_1 >> (temp32s_2 - 7);
				} else {

					temp32u_1 = temp32u_1 << (7 - temp32s_2);
				}
			}
			pavg_tgt->range_sigma_mm  = (uint16_t)(temp32u_1);
		}

	}

#if defined(AFF_FUNCTIONS_DEBUG)
	printf("            Status derivation. nb_valid: %d, nb_merged: %d, first_status_idx: %d, first_status_val: %d .\n",
		   nb_valid, nb_merged, first_status_idx, first_status_val);
#endif

	pavg_tgt->target_status = DCI_TARGET_STATUS__NOUPDATE;
	if (nb_valid + nb_merged > 0) {

		if (nb_valid >= nb_merged) {
			pavg_tgt->target_status = DCI_TARGET_STATUS__RANGECOMPLETE;
#if defined(AFF_FUNCTIONS_DEBUG)
			printf("            Set status to DCI_TARGET_STATUS__RANGECOMPLETE: %d\n",
				   DCI_TARGET_STATUS__RANGECOMPLETE);
#endif
		} else {
			pavg_tgt->target_status = DCI_TARGET_STATUS__RANGECOMPLETE_MERGED_PULSE;
#if defined(AFF_FUNCTIONS_DEBUG)
			printf("            Set status to DCI_TARGET_STATUS__RANGECOMPLETE_MERGED_PULSE: %d\n",
				   DCI_TARGET_STATUS__RANGECOMPLETE_MERGED_PULSE);
#endif
		}
	} else {

		if ((first_status_val == DCI_TARGET_STATUS__RANGECOMPLETE_NO_WRAP_CHECK)
			&& (first_status_idx > 0)) {

			pavg_tgt->target_status = DCI_TARGET_STATUS__PREV_RANGE_NO_TARGETS;
#if defined(AFF_FUNCTIONS_DEBUG)
			printf("            Set status to DCI_TARGET_STATUS__PREV_RANGE_NO_TARGETS: %d\n",
				   DCI_TARGET_STATUS__PREV_RANGE_NO_TARGETS);
#endif
		} else {
			pavg_tgt->target_status = (uint8_t)(first_status_val);
#if defined(AFF_FUNCTIONS_DEBUG)
			printf("            Set status to first_status_val: %d\n", first_status_val);
#endif
		}
	}

	if (ptgt_hist != NULL)
		ptgt_hist->avg_zscore = pavg_tgt->target_zscore;

	pavg_tgt->ptgt_hist = ptgt_hist;

exit:

	return;

}

struct vl53l5_aff_tgt_hist_t *vl53l5_aff_tgt_to_mem(
	struct vl53l5_aff_tgt_data_t          *ptgt,
	struct vl53l5_aff_tgt_out_t           *pout_tgt,
	int32_t                        zscore,
	int32_t                        iir_enabled,
	struct vl53l5_aff_int_dev_t    *pint_dev,
	struct vl53l5_aff_err_dev_t    *perr_dev
)
{

	int32_t                        max_tgt;
	int32_t                        k;
	struct vl53l5_aff_tgt_hist_t          *phist;
	struct vl53l5_aff_tgt_data_t          *pdat;
	int32_t                        found;
	int32_t                        zmin;
	int32_t                        nb_past;
	struct vl53l5_aff_tgt_hist_t          *phist_tgts;

#if defined(AFF_FUNCTIONS_DEBUG)
	printf("    Add target to memory.\n");
#endif

	max_tgt      =
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_MAX_TGT_IDX];
	found        = 0;
	zmin         = zscore;
	phist_tgts   = vl53l5_aff_filled_tgt_hist_arr(pint_dev);
	phist        = NULL;

	for (k = 0; k < max_tgt; k++) {
		if (((*((phist_tgts[k]).pfilled)) & 0x1) == 0) {
			phist   = &(phist_tgts[k]);
			found   = 1;
			break;
		}
	}

	if (found == 0) {
		for (k = max_tgt - 1; k >= 0;
			 k--) {

			if ((phist_tgts[k]).avg_zscore <= zmin) {
				zmin     = (phist_tgts[k]).avg_zscore;
				phist    = &(phist_tgts[k]);
				found    = 1;
			}
		}
	}

	if (found == 1) {
		nb_past        =
			(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_NB_PAST_IDX];
		if (iir_enabled == 1)
			nb_past = nb_past - 1;

		for (k = 0; k < nb_past; k++)
			((phist->pdata_arr)[k]).filled = 0;
		pdat     = vl53l5_aff_passed_data(pint_dev, phist, -1, iir_enabled);
		vl53l5_aff_copy_data(ptgt, pdat, perr_dev);
#if defined(AFF_FUNCTIONS_DEBUG)
		printf("        Copying target:\n");
		aff_utils_display_tgt_data(ptgt);
#endif

		pdat->filled       = 1;
		*(phist->pfilled)  = 0x1;

		phist->avg_zscore  = zscore;
		if (iir_enabled == 1) {
			vl53l5_aff_out_to_data(pout_tgt, &((phist->pdata_arr)[nb_past]), perr_dev);
#if defined(AFF_FUNCTIONS_DEBUG)
			printf("        Copying to previous out:\n");
			aff_utils_display_tgt_out(pout_tgt);
#endif

		}

	}

	return phist;

}

void vl53l5_aff_copy_data(
	struct vl53l5_aff_tgt_data_t          *psrc,
	struct vl53l5_aff_tgt_data_t          *pdest,
	struct vl53l5_aff_err_dev_t    *perr_dev __attribute__((unused)))
{

	pdest->peak_rate_kcps_per_spad    = psrc->peak_rate_kcps_per_spad;
#ifdef VL53L5_MEDIAN_PHASE_ON
	pdest->median_phase               = psrc->median_phase;
#endif
#ifdef VL53L5_RATE_SIGMA_KCPS_PER_SPAD_ON
	pdest->rate_sigma_kcps_per_spad   = psrc->rate_sigma_kcps_per_spad;
#endif
	pdest->peak_rate_kcps_per_spad    = psrc->peak_rate_kcps_per_spad;
	pdest->target_zscore              = psrc->target_zscore;
	pdest->range_sigma_mm             = psrc->range_sigma_mm;
	pdest->median_range_mm            = psrc->median_range_mm;
#ifdef VL53L5_START_RANGE_MM_ON
	pdest->start_range_mm             = psrc->start_range_mm;
#endif
#ifdef VL53L5_END_RANGE_MM_ON
	pdest->end_range_mm               = psrc->end_range_mm;
#endif
#ifdef VL53L5_MIN_RANGE_DELTA_MM_ON
	pdest->min_range_delta_mm         = psrc->min_range_delta_mm;
#endif
#ifdef VL53L5_MAX_RANGE_DELTA_MM_ON
	pdest->max_range_delta_mm         = psrc->max_range_delta_mm;
#endif
#ifdef VL53L5_TARGET_REFLECTANCE_EST_PC_ON
	pdest->target_reflectance_est_pc  = psrc->target_reflectance_est_pc;
#endif
	pdest->target_status              = psrc->target_status;
	pdest->filled                     = psrc->filled;
}

void vl53l5_aff_temporal_shift(
	struct vl53l5_aff_tgt_data_t          *pcur_tgt,
	struct vl53l5_aff_tgt_out_t           *pout_tgt,
	int32_t                        iir_enabled,
	struct vl53l5_aff_tgt_hist_t          *ptgt_hist,
	struct vl53l5_aff_cfg_dev_t    *pcfg_dev,
	struct vl53l5_aff_int_dev_t    *pint_dev,
	struct vl53l5_aff_err_dev_t    *perr_dev __attribute__((unused)))
{

	int32_t  nb_past;
	int32_t  temporal_idx;
	int32_t  any;
	int32_t  k;

#if defined(AFF_FUNCTIONS_DEBUG)

	printf("    Temporal shift.\n");
#endif

	temporal_idx   =
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_TEMP_IND_IDX];
	nb_past        =
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_NB_PAST_IDX];

	if (iir_enabled) {
		nb_past = nb_past - 1;
#if defined(AFF_FUNCTIONS_DEBUG)
		printf("        iir enabled, nb_past\n");
#endif
	}
#if defined(AFF_FUNCTIONS_DEBUG)
	printf("        temporal_idx: %d, nb_past: %d\n", temporal_idx, nb_past);
#endif

	if (pcur_tgt == NULL) {

		((ptgt_hist->pdata_arr)[(temporal_idx + nb_past - 1) % nb_past]).filled = 0;
#if defined(AFF_FUNCTIONS_DEBUG)
		printf("        filled set to 0 for index %d\n",
			   (temporal_idx + nb_past - 1) % nb_past);
#endif
	} else {
		vl53l5_aff_copy_data(
			pcur_tgt,
			&((ptgt_hist->pdata_arr)[(temporal_idx + nb_past - 1) % nb_past]),
			perr_dev);
		((ptgt_hist->pdata_arr)[(temporal_idx + nb_past - 1) % nb_past]).filled = 1;
#if defined(AFF_FUNCTIONS_DEBUG)
		printf("Copy current at index %d\n", (temporal_idx + nb_past - 1) % nb_past);
		printf("          zscore:     %20d  %f\n", (pcur_tgt->target_zscore),
			   (double)(pcur_tgt->target_zscore) / pow(2, 5));
		printf("          range:      %20d  %f\n", (pcur_tgt->median_range_mm),
			   (double)(pcur_tgt->median_range_mm) / pow(2, 2));

#endif
	}

	any = 0;
	for (k = 0; k < nb_past; k++) {
		if (((ptgt_hist->pdata_arr)[k]).filled == 1) {
			any = 1;

			break;
		}
	}

	if (iir_enabled == 1) {
		vl53l5_aff_out_to_data(pout_tgt, &((ptgt_hist->pdata_arr)[nb_past]), perr_dev);
#if defined(AFF_FUNCTIONS_DEBUG)
		printf("        Copy out at index index %d\n", nb_past);
		printf("        out is:\n");
		printf("          peak_rate:  %20d  %f\n", (pout_tgt->peak_rate_kcps_per_spad),
			   (double)(pout_tgt->peak_rate_kcps_per_spad) / pow(2, 11));
		printf("          zscore:     %20d  %f\n", (pout_tgt->target_zscore),
			   (double)(pout_tgt->target_zscore) / pow(2, 20));
		printf("          range:      %20d  %f\n", (pout_tgt->median_range_mm),
			   (double)(pout_tgt->median_range_mm) / pow(2, 2));
		printf("          status:     %20d\n", (pout_tgt->target_status));
#endif
	}

#if defined(AFF_FUNCTIONS_DEBUG)
	printf("        Set history to filled and let pervious best flag unchanged.\n");
#endif
	*(ptgt_hist->pfilled) = ((*(ptgt_hist->pfilled)) |
							 0x1);

	if (any == 0) {
#if defined(AFF_FUNCTIONS_DEBUG)
		printf("        Past empty.\n");
#endif
		if ((iir_enabled == 0) ||
			(iir_enabled == 1
			 && ((IS_BIT_SET_UINT8((pcfg_dev->aff_cfg).aff__avg_option,
								   AFF__KEEP_PREV_OUT_ON_EMPTY_TGT_HIST) == 0)
				 || (pout_tgt->target_status == 0)))) {

			*(ptgt_hist->pfilled) = ((*(ptgt_hist->pfilled)) &
									 (~0x3));

#if defined(AFF_FUNCTIONS_DEBUG)
			printf("        Reset filledvais : %d\n", *(ptgt_hist->pfilled));
#endif

		}
	}

}

void vl53l5_aff_cand_to_dci(
	int32_t                       zone_id,
	int32_t                       nb_cand,
	struct vl53l5_aff_cfg_dev_t   *pcfg_dev,
	struct vl53l5_aff_int_dev_t   *pint_dev,
	struct vl53l5_range_results_t           *pdci_dev,
	struct vl53l5_aff_err_dev_t   *perr_dev __attribute__((unused))
)

{

	int32_t                       k;
	int32_t                       q;
	int32_t                       max;
	uint32_t                      prmax;
	int32_t                       rgmax;
	int32_t                       kmax;
	struct vl53l5_aff_tgt_out_t   *pout;
	int32_t                       nb_dci_tgt;
	int32_t                       best_searched;
	int32_t                       offset;
	int32_t                       zscore;

#if defined(AFF_FUNCTIONS_DEBUG_FINE)
	printf("        nb_cand: %d.\n", nb_cand);
	printf("        aff__min_zscore_for_out: %d.\n",
		   (pcfg_dev->aff_cfg).aff__min_zscore_for_out);
#endif

	pout = vl53l5_aff_out_cand_arr(pint_dev);

	for (k = 0; k < nb_cand; k++) {
#if defined(AFF_FUNCTIONS_DEBUG_FINE)
		printf("        pout[%d] zscore:        %d.\n", k, pout[k].target_zscore);
		printf("        thresh:             %d.\n",
			   ((int32_t)((pcfg_dev->aff_cfg).aff__min_zscore_for_out) << 15));
		printf("        pout target_status: %d.\n", pout[k].target_status);
#endif

		if ((pout[k].target_zscore >= ((int32_t)((
										   pcfg_dev->aff_cfg).aff__min_zscore_for_out) << 15))
			&& (pout[k].target_status != DCI_TARGET_STATUS__NOUPDATE)) {
			pout[k].filled =
				1;

		} else
			pout[k].filled = 0;
	}
	nb_dci_tgt = 0;

	best_searched = 0;
	for (q = 0; q < (int32_t)((pdci_dev->common_data).rng__max_targets_per_zone);
		 q++) {
		max   = 0;
		prmax = 0;
		rgmax = 65535;
		kmax = -1;
		if ((pcfg_dev->aff_cfg).aff__out_sort_mode ==
			VL53L5_AFF_OUT_SORT_OPTION__ZSCORE_AND_PREV_BEST) {

			for (k = 0; k < nb_cand; k++) {
				if (pout[k].filled == 1) {
#if defined(AFF_FUNCTIONS_DEBUG_FINE)
					printf("pout[%d].target_zscore: %d\n", k, pout[k].target_zscore);
#endif
					if (pout[k].target_zscore > max) {
						max   = pout[k].target_zscore;
						kmax = k;
					}
				}
			}

#if defined(AFF_FUNCTIONS_DEBUG_FINE)
			printf("        q: %d, kmax: %d\n", q, kmax);
#endif

			if (best_searched == 0) {

				for (k = 0; k < nb_cand; k++) {
					if ((pout[k].filled == 1)
						&& ((uint64_t)(65536) * (uint64_t)((max - pout[k].target_zscore)) <=
							(uint64_t)(((uint64_t)((pcfg_dev->aff_cfg).aff__out_sort_param)) * (uint64_t)(
										   max)))
						&& (pout[k].ptgt_hist != NULL)
						&& (((*((*(pout[k].ptgt_hist)).pfilled)) & 0x2) == 0x2)
					   ) {
						kmax = k;
#if defined(AFF_FUNCTIONS_DEBUG_FINE)
						printf("Find previous best: %d\n", kmax);
#endif
						break;
					}
				}

				best_searched = 1;

				for (k = 0; k < nb_cand; k++) {
					if ((pout[k].ptgt_hist != NULL)
						&& (((*(pout[k].ptgt_hist)).pfilled) != NULL)) {
						(*((*(pout[k].ptgt_hist)).pfilled))
							= (*((*(pout[k].ptgt_hist)).pfilled)) &  ~(0x2);
					}
				}

				if ((kmax >= 0) && (pout[kmax].ptgt_hist != NULL)
					&& (((*(pout[kmax].ptgt_hist)).pfilled) != NULL)) {
					(*((*(pout[kmax].ptgt_hist)).pfilled))
						= (*((*(pout[kmax].ptgt_hist)).pfilled)) | (0x2);
#if defined(AFF_FUNCTIONS_DEBUG_FINE)
					printf("New previous best: %d\n", kmax);
#endif
				}
			} else {
				for (k = 0; k < nb_cand; k++) {
					if ((pout[k].filled == 1) && (pout[k].target_zscore > max)) {
						max   = pout[k].target_zscore;
						kmax = k;
					}
				}
			}

		} else {
			for (k = 0; k < nb_cand; k++) {
				if (pout[k].filled == 1) {

					if (pout[k].target_zscore > max) {
						max   = pout[k].target_zscore;
						prmax = pout[k].peak_rate_kcps_per_spad;
						rgmax = pout[k].median_range_mm;
						kmax = k;
					} else if (pout[k].target_zscore == max) {

						if (((pcfg_dev->aff_cfg).aff__out_sort_mode ==
							 VL53L5_AFF_OUT_SORT_OPTION__ZSCORE_THEN_CLOSEST)
							&& (pout[k].median_range_mm < rgmax)) {
							max   = pout[k].target_zscore;
							prmax = pout[k].peak_rate_kcps_per_spad;
							rgmax = pout[k].median_range_mm;
							kmax = k;
						}
						if (((pcfg_dev->aff_cfg).aff__out_sort_mode ==
							 VL53L5_AFF_OUT_SORT_OPTION__ZSCORE_THEN_STRONGEST)
							&& (pout[k].peak_rate_kcps_per_spad > prmax)) {
							max   = pout[k].target_zscore;
							prmax = pout[k].peak_rate_kcps_per_spad;
							rgmax = pout[k].median_range_mm;
							kmax = k;
						}

					}
				}

			}
		}

		if (kmax >= 0) {

			nb_dci_tgt = nb_dci_tgt + 1;
			(pout[kmax].filled = 0);

			offset = ((int32_t)((pdci_dev->common_data).rng__max_targets_per_zone) *
					  zone_id);

			((pdci_dev->per_tgt_results).peak_rate_kcps_per_spad)[offset + q]    =
				pout[kmax].peak_rate_kcps_per_spad;
#ifdef VL53L5_MEDIAN_PHASE_ON
			((pdci_dev->per_tgt_results).median_phase)[offset + q]               =
				pout[kmax].median_phase;
#endif
#ifdef VL53L5_RATE_SIGMA_KCPS_PER_SPAD_ON
			((pdci_dev->per_tgt_results).rate_sigma_kcps_per_spad)[offset + q]   =
				pout[kmax].rate_sigma_kcps_per_spad;
#endif
			zscore = pout[kmax].target_zscore;
			if (zscore < 0)
				((pdci_dev->per_tgt_results).target_zscore)[offset + q]          = (uint16_t)(
							0);

			else {
				zscore = (zscore + (1 << 14)) >> 15;
				if (zscore > 0xFFFF)
					((pdci_dev->per_tgt_results).target_zscore)[offset + q]          = (uint16_t)(
								0xFFFF);

				else
					((pdci_dev->per_tgt_results).target_zscore)[offset + q]          = (uint16_t)(
								zscore);
			}
			((pdci_dev->per_tgt_results).range_sigma_mm)[offset + q]             =
				pout[kmax].range_sigma_mm;
			((pdci_dev->per_tgt_results).median_range_mm)[offset + q]            =
				pout[kmax].median_range_mm;
#ifdef VL53L5_START_RANGE_MM_ON
			((pdci_dev->per_tgt_results).start_range_mm)[offset + q]             =
				pout[kmax].start_range_mm;
#endif
#ifdef VL53L5_END_RANGE_MM_ON
			((pdci_dev->per_tgt_results).end_range_mm)[offset + q]               =
				pout[kmax].end_range_mm;
#endif
#ifdef VL53L5_MIN_RANGE_DELTA_MM_ON
			((pdci_dev->per_tgt_results).min_range_delta_mm)[offset + q]         =
				pout[kmax].min_range_delta_mm;
#endif
#ifdef VL53L5_MAX_RANGE_DELTA_MM_ON
			((pdci_dev->per_tgt_results).max_range_delta_mm)[offset + q]         =
				pout[kmax].max_range_delta_mm;
#endif
#ifdef VL53L5_TARGET_REFLECTANCE_EST_PC_ON
			((pdci_dev->per_tgt_results).target_reflectance_est_pc)[offset + q]  =
				pout[kmax].target_reflectance_est_pc;
#endif
			((pdci_dev->per_tgt_results).target_status)[offset + q]              =
				pout[kmax].target_status;

#if defined(AFF_FUNCTIONS_DEBUG)
			printf("out[%d]:\n", kmax);
			printf("  peak_rate:       %20d  %f\n", (pout[kmax].peak_rate_kcps_per_spad),
				   (double)(pout[kmax].peak_rate_kcps_per_spad) / pow(2, 11));
			printf("  zscore:          %20d  %f\n", (pout[kmax].target_zscore),
				   (double)(pout[kmax].target_zscore) / pow(2, 5));
			printf("  range:           %20d  %f\n", (pout[kmax].median_range_mm),
				   (double)(pout[kmax].median_range_mm) / pow(2, 2));
			printf("  range_sigma_mm:  %20d  %f\n", (pout[kmax].range_sigma_mm),
				   (double)(pout[kmax].range_sigma_mm) / pow(2, 7));
			printf("  target_status:   %d.\n", pout[kmax].target_status);
#endif

		}

	}

	(pdci_dev->per_zone_results).rng__no_of_targets[zone_id] = (uint8_t)(
				nb_dci_tgt);

}

void vl53l5_aff_update_temp_idx(
	struct vl53l5_aff_int_dev_t   *pint_dev,
	int32_t                       iir_enabled
)
{

	int32_t  nb_past;
	int32_t  temporal_idx;

	temporal_idx   =
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_TEMP_IND_IDX];
	nb_past        =
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_NB_PAST_IDX];
	if (iir_enabled == 1)
		nb_past = nb_past - 1;
	temporal_idx   = (temporal_idx + nb_past - 1) % nb_past;
	(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_TEMP_IND_IDX]
		= temporal_idx;
}

void vl53l5_aff_comp_cur_pref(
	int32_t                        nb_cur_tgts,
	int32_t                        nb_hist_tgts,
	struct vl53l5_aff_int_dev_t    *pint_dev,
	struct vl53l5_aff_err_dev_t    *perr_dev __attribute__((unused)))
{

	int32_t   *pmat;
	int32_t   *pcurpref;
	int32_t   k;
	int32_t   j;
	int32_t   q;
	int32_t   x;

	pmat      = vl53l5_aff_affinity_matrix(pint_dev);
	pcurpref  = vl53l5_aff_get_cur_pref(pint_dev);

	for (k = 0; k < nb_cur_tgts; k++) {
		for (j = 0; j < nb_hist_tgts; j++)
			pcurpref[k * nb_hist_tgts + j] = j;
	}

	for (k = 0; k < nb_cur_tgts; k++) {
		for (j = 1; j < nb_hist_tgts; j++) {
			x = pcurpref[k * nb_hist_tgts + j];
			for (q = j; (q > 0)
				 && (pmat[k * nb_hist_tgts + pcurpref[k * nb_hist_tgts + q - 1]]
					 > pmat[k * nb_hist_tgts + x]) ; q--)
				pcurpref[k * nb_hist_tgts + q] = pcurpref[k * nb_hist_tgts + q - 1];
			pcurpref[k * nb_hist_tgts + q] = x;
		}
	}

}

void vl53l5_aff_comp_hist_pref(
	int32_t                        nb_cur_tgts,
	int32_t                        nb_hist_tgts,
	struct vl53l5_aff_int_dev_t    *pint_dev,
	struct vl53l5_aff_err_dev_t    *perr_dev __attribute__((unused)))
{

	int32_t   *pmat;
	int32_t   *phistpref;
	int32_t   k;
	int32_t   j;
	int32_t   q;
	int32_t   x;

	pmat       = vl53l5_aff_affinity_matrix(pint_dev);
	phistpref  = vl53l5_aff_get_hist_pref(pint_dev);

	for (j = 0; j < nb_hist_tgts; j++) {
		for (k = 0; k < nb_cur_tgts; k++)
			phistpref[k * nb_hist_tgts + j] = k;
	}

	for (j = 0; j < nb_hist_tgts; j++) {
		for (k = 1; k < nb_cur_tgts; k++) {
			x = phistpref[k * nb_hist_tgts + j];
			for (q = k; (q > 0)
				 && (pmat[phistpref[(q - 1) * nb_hist_tgts + j] * nb_hist_tgts + j]
					 > pmat[x * nb_hist_tgts + j]) ; q--)
				phistpref[q * nb_hist_tgts + j] = phistpref[(q - 1) * nb_hist_tgts + j];
			phistpref[q * nb_hist_tgts + j] = x;
		}
	}

}

void vl53l5_aff_match_targets(
	int32_t                        nb_cur_tgts,
	int32_t                        nb_hist_tgts,
	int32_t                        *pcur_hist_asso,
	int32_t                        *phist_cur_asso,
	struct vl53l5_aff_int_dev_t    *pint_dev,
	struct vl53l5_aff_err_dev_t    *perr_dev __attribute__((unused))
)
{

	int32_t *pmat;
	int32_t *pind;
	int32_t *pcurpref;
	int32_t *phistpref;
	int32_t k;
	int32_t q;
	int32_t nb_ind;
	int32_t cur_ind;
	int32_t hist_ind;
	int32_t prev_ind;
	int32_t loc_prev;
	int32_t loc_cur;

	pmat            = vl53l5_aff_affinity_matrix(pint_dev);
	pind            = vl53l5_aff_get_ind_to_be_asso(pint_dev);
	pcurpref        = vl53l5_aff_get_cur_pref(pint_dev);
	phistpref       = vl53l5_aff_get_hist_pref(pint_dev);

	vl53l5_aff_comp_cur_pref(
		nb_cur_tgts,
		nb_hist_tgts,
		pint_dev,
		perr_dev);

	vl53l5_aff_comp_hist_pref(
		nb_cur_tgts,
		nb_hist_tgts,
		pint_dev,
		perr_dev);

	for (k = 0; k < nb_hist_tgts; k++)
		pcur_hist_asso[k] = -1;

	for (k = 0; k < nb_cur_tgts; k++)
		phist_cur_asso[k] = -1;

	for (k = 0; k < nb_cur_tgts; k++)
		pind[k] = k;
	nb_ind = nb_cur_tgts;

	while (nb_ind != 0) {

		cur_ind = pind[0];
		for (k = 0; k < nb_hist_tgts; k++) {

			hist_ind = pcurpref[cur_ind * nb_hist_tgts + k];

			if (pmat[cur_ind * nb_hist_tgts + hist_ind] == 0x7FFFFFFF)
				break;

			if (pcur_hist_asso[hist_ind] == -1) {
				pcur_hist_asso[hist_ind] = cur_ind;
				phist_cur_asso[cur_ind]  = hist_ind;
				for (q = 0; q < nb_cur_tgts - 1; q++)
					pind[q] = pind[q + 1];
				nb_ind = nb_ind - 1;
				break;
			}

			prev_ind = pcur_hist_asso[hist_ind];
			loc_prev = 0;

			for (q = 0; q < nb_cur_tgts; q++) {
				if (phistpref[q * nb_hist_tgts + hist_ind] == prev_ind) {
					loc_prev = q;
					break;
				}
			}
			loc_cur = 0;

			for (q = 0; q < nb_cur_tgts; q++) {
				if (phistpref[q * nb_hist_tgts + hist_ind] == cur_ind) {
					loc_cur = q;
					break;
				}
			}

			if (loc_cur < loc_prev) {
				pcur_hist_asso[hist_ind] = cur_ind;
				phist_cur_asso[cur_ind]  = hist_ind;
				for (q = 0; q < nb_cur_tgts - 1; q++)
					pind[q] = pind[q + 1];
				pind[nb_ind - 1]          = prev_ind;
				phist_cur_asso[prev_ind]  = -1;
				break;
			}

		}
		if (phist_cur_asso[cur_ind] == -1) {
			for (q = 0; q < nb_cur_tgts - 1; q++)
				pind[q] = pind[q + 1];
			nb_ind = nb_ind - 1;
		}
	}

}

void vl53l5_aff_out_to_data(
	struct vl53l5_aff_tgt_out_t           *pout_src,
	struct vl53l5_aff_tgt_data_t          *pdat_dst,
	struct vl53l5_aff_err_dev_t    *perr_dev __attribute__((unused))
)
{

	pdat_dst->peak_rate_kcps_per_spad    = pout_src->peak_rate_kcps_per_spad;
	pdat_dst->median_phase               = pout_src->median_phase;
	pdat_dst->rate_sigma_kcps_per_spad   = pout_src->rate_sigma_kcps_per_spad;
	pdat_dst->range_sigma_mm             = pout_src->range_sigma_mm;
	pdat_dst->median_range_mm            = pout_src->median_range_mm;
	pdat_dst->start_range_mm             = pout_src->start_range_mm;
	pdat_dst->end_range_mm               = pout_src->end_range_mm;
	pdat_dst->min_range_delta_mm         = pout_src->min_range_delta_mm;
	pdat_dst->max_range_delta_mm         = pout_src->max_range_delta_mm;
	pdat_dst->target_reflectance_est_pc  = pout_src->target_reflectance_est_pc;
	pdat_dst->target_status              = pout_src->target_status;

	pdat_dst->target_zscore = (uint16_t)(pout_src->target_zscore);

	pdat_dst->filled        = (int16_t)(((uint32_t)(pout_src->target_zscore)) >>
										16);

}
