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
#include "dci_luts.h"
#include "vl53l5_gd_defs.h"
#include "vl53l5_gd_luts.h"
#include "vl53l5_glass_detection_funcs.h"

void vl53l5_gd_clear_data(
	struct vl53l5_gd_int_dev_t         *pint_dev,
	struct dci_patch_op_dev_t          *pop_dev,
	struct vl53l5_gd_err_dev_t         *perr_dev)
{
	struct dci_grp__gd__op__data_t   *pop_data = &(pop_dev->gd_op_data);
	struct dci_grp__gd__op__status_t *pop_status = &(pop_dev->gd_op_status);
	struct common_grp__status_t    *perror   = &(perr_dev->gd_error_status);
	struct common_grp__status_t    *pwarning = &(perr_dev->gd_warning_status);

	memset(
		&(pint_dev->gd_zone_data),
		0,
		sizeof(struct vl53l5_gd__zone__data_t));
	memset(
		&(pint_dev->gd_plane_coeffs),
		0,
		sizeof(struct vl53l5_gd__plane__coeffs_t));
	memset(
		&(pint_dev->gd_plane_status),
		0,
		sizeof(struct vl53l5_gd__plane__status_t));

	pop_data->gd__max__valid_rate_kcps_spad  = 0U;
	pop_data->gd__min__valid_rate_kcps_spad  = 0U;
	pop_data->gd__max__plane_rate_kcps_spad = 0U;
	pop_data->gd__min__plane_rate_kcps_spad = 0U;

	pop_data->gd__glass_display_case_dist_mm = 0;
	pop_data->gd__glass_merged_dist_mm = 0;

	pop_data->gd__max_valid_rate_zone = 0U;
	pop_data->gd__max_valid_rate_zone_clipped = 0U;
	pop_data->gd__valid_zone_count = 0U;
	pop_data->gd__merged_zone_count = 0U;
	pop_data->gd__blurred_zone_count = 0U;
	pop_data->gd__plane_zone_count = 0U;
	pop_data->gd__points_on_plane_pc = 0U;

	pop_status->gd__rate_ratio = 0U;
	pop_status->gd__confidence = 0U;
	pop_status->gd__plane_detected = 0U;
	pop_status->gd__ratio_detected = 0U;
	pop_status->gd__glass_detected = 0U;
	pop_status->gd__tgt_behind_glass_detected = 0U;
	pop_status->gd__mirror_detected = 0U;
	pop_status->gd__op_spare_0 = 0U;
	pop_status->gd__op_spare_1 = 0U;

	perror->status__group = VL53L5_ALGO_GLASS_DETECTION_GROUP_ID;
	perror->status__stage_id = VL53L5_GD_STAGE_ID__INVALID;
	perror->status__type = VL53L5_GD_ERROR_NONE;
	perror->status__debug_0 = 0U;
	perror->status__debug_1 = 0U;
	perror->status__debug_2 = 0U;

	pwarning->status__group = VL53L5_ALGO_GLASS_DETECTION_GROUP_ID;
	pwarning->status__stage_id = VL53L5_GD_STAGE_ID__INVALID;
	pwarning->status__type = VL53L5_GD_WARNING_NONE;
	pwarning->status__debug_0 = 0U;
	pwarning->status__debug_1 = 0U;
	pwarning->status__debug_2 = 0U;
}

void vl53l5_gd_init_zone_meta_data(
	struct vl53l5_range_common_data_t   *prng_common_data,
	struct vl53l5_gd__zone__meta_t      *pzone_meta,
	struct common_grp__status_t         *pstatus)
{

	pstatus->status__stage_id = VL53L5_GD_STAGE_ID__INIT_ZONE_META_DATA;
	pstatus->status__type = VL53L5_GD_ERROR_NONE;

	pzone_meta->gd__max_zones         = VL53L5_GD_DEF__MAX_ZONES;
	pzone_meta->gd__no_of_zones       = prng_common_data->rng__no_of_zones;
	pzone_meta->gd__grid__size        = 0U;
	pzone_meta->gd__grid__pitch_spad  = 0U;

	switch (pzone_meta->gd__no_of_zones) {
	case 16:
		pzone_meta->gd__grid__size = 4U;
		pzone_meta->gd__grid__pitch_spad = 8U;
		break;
	case 64:
		pzone_meta->gd__grid__size = 8U;
		pzone_meta->gd__grid__pitch_spad = 4U;
		break;
	default:
		pstatus->status__type =
			VL53L5_GD_ERROR_INVALID_PARAMS;
		break;
	}

}

void vl53l5_gd_init_trig_coeffs(
	struct vl53l5_gd__zone__meta_t      *pzone_meta,
	struct vl53l5_gd__trig__coeffs_t    *ptrig_coeffs,
	struct common_grp__status_t         *pstatus)
{

	int32_t  z             = 0U;
	int32_t  grid_size =
		(int32_t)pzone_meta->gd__grid__size;
	int32_t zone_centre_um = 0;
	int32_t trig_coeff = 0;

	pstatus->status__stage_id = VL53L5_GD_STAGE_ID__INIT_TRIG_COEFFS;
	pstatus->status__type = VL53L5_GD_ERROR_NONE;

	for (z = 0 ; z < (int32_t)VL53L5_GD_DEF__MAX_COEFFS ; z++)
		ptrig_coeffs->gd__trig_coeffs[z] = 4096;

	for (z = 0 ; z < grid_size ; z++) {

		zone_centre_um = (z * 2) - grid_size + 1;
		zone_centre_um *= (int32_t)pzone_meta->gd__grid__pitch_spad;
		zone_centre_um *= (int32_t)VL53L5_GD_DEF__SPAD_PITCH_UM;
		zone_centre_um /= 2;

		trig_coeff = zone_centre_um * 4096;
		if (trig_coeff < 0)
			trig_coeff -= ((int32_t)VL53L5_GD_DEF__EFFECTIVE_FOCAL_LENGTH_UM / 2);
		else
			trig_coeff += ((int32_t)VL53L5_GD_DEF__EFFECTIVE_FOCAL_LENGTH_UM / 2);
		trig_coeff /= (int32_t)VL53L5_GD_DEF__EFFECTIVE_FOCAL_LENGTH_UM;

		ptrig_coeffs->gd__trig_coeffs[z] = (int16_t)trig_coeff;

	}
}

int16_t vl53l5_gd_scale_range(
	int16_t  median_range_mm,
	int16_t  scale_factor)
{
	int32_t tmp = (int32_t)median_range_mm;
	int16_t value = 0;

	tmp *= (int32_t)scale_factor;

	if (tmp < 0)
		tmp -= 2048;
	else
		tmp += 2048;

	tmp /= 4096;

	if (tmp < (int32_t)VL53L5_INT16_MIN)
		tmp = (int32_t)VL53L5_INT16_MIN;
	if (tmp > (int32_t)VL53L5_INT16_MAX)
		tmp = (int32_t)VL53L5_INT16_MAX;

	value = (int16_t)tmp;

	return value;
}

uint32_t  vl53l5_gd_is_target_valid(
	uint32_t    target_idx,
	uint32_t    rng__no_of_targets,
	uint32_t    target_status,
	uint8_t    *ptgt_status_list)
{

	uint32_t  target_idx_valid = 0U;
	uint32_t  target_status_valid = 0U;
	uint32_t  target_valid = 0U;
	uint8_t  *ptgt_status = ptgt_status_list;

	if (target_idx < rng__no_of_targets)
		target_idx_valid = 1U;

	while (*ptgt_status != 0U && *ptgt_status != 255U) {
		if (target_status == *ptgt_status)
			target_status_valid = 1U;
		ptgt_status++;
	}

	target_valid = target_idx_valid * target_status_valid;

	return target_valid;
}

void vl53l5_gd_filter_results(
	struct vl53l5_range_results_t                 *presults,
	struct vl53l5_gd_cfg_dev_t          *pcfg_dev,
	struct vl53l5_gd_int_dev_t          *pint_dev,
	struct dci_patch_op_dev_t           *pop_dev,
	struct common_grp__status_t         *pstatus)
{

	uint32_t  z                = 0U;
	uint32_t  c                = 0U;
	uint32_t  r                = 0U;
	uint32_t  t                = 0U;
	uint32_t  target_valid     = 0U;
	int16_t   median_range_mm  = 0;
	uint8_t   target_status    = 0U;

	uint32_t  max_targets_per_zone =
		(uint32_t)presults->common_data.rng__max_targets_per_zone;
	uint32_t  no_of_zones =
		(uint32_t)pint_dev->gd_zone_meta.gd__no_of_zones;

	struct vl53l5_gd__zone__meta_t   *pzone_meta = &(pint_dev->gd_zone_meta);
	struct vl53l5_gd__zone__data_t   *pzone_data = &(pint_dev->gd_zone_data);
	struct vl53l5_gd__trig__coeffs_t *ptrig_coeffs = &(pint_dev->gd_trig_data);
	struct dci_grp__gd__op__data_t     *pop_data = &(pop_dev->gd_op_data);

	pstatus->status__stage_id = VL53L5_GD_STAGE_ID__FILTER_RESULTS;
	pstatus->status__type = VL53L5_GD_ERROR_NONE;

	for (z = 0 ; z < VL53L5_GD_DEF__MAX_ZONES ; z++) {
		pzone_data->gd__peak_rate_kcps_per_spad[z] = 0U;
		pzone_data->gd__x_positon_mm[z] = 0;
		pzone_data->gd__y_positon_mm[z] = 0;
		pzone_data->gd__z_positon_mm[z] = 0;
		pzone_data->gd__target_status[z] = 0U;
		pzone_data->gd__is_valid[z] = 0U;
	}

	for (z = 0 ; z < VL53L5_GD_DEF__MAX_COUNTS ; z++)
		pzone_data->gd__target_status_counts[z] = 0U;

	for (z = 0 ; z < no_of_zones ; z++) {

		c = z % (uint32_t)pzone_meta->gd__grid__size;
		r = z / (uint32_t)pzone_meta->gd__grid__size;

		t = z * max_targets_per_zone;

		target_valid =
			vl53l5_gd_is_target_valid(
				0U,
				(uint32_t)presults->per_zone_results.rng__no_of_targets[z],
				(uint32_t)presults->per_tgt_results.target_status[t],
				&(pcfg_dev->gd_tgt_status_list_0.gd__tgt_status[0]));

		if (target_valid > 0U) {

			pzone_data->gd__peak_rate_kcps_per_spad[z] =
				presults->per_tgt_results.peak_rate_kcps_per_spad[t];

			median_range_mm =
				presults->per_tgt_results.median_range_mm[t];

			if (median_range_mm < 0)
				median_range_mm -= 2;
			else
				median_range_mm += 2;
			median_range_mm /= 4;

			pzone_data->gd__x_positon_mm[z] =
				vl53l5_gd_scale_range(
					median_range_mm,
					ptrig_coeffs->gd__trig_coeffs[c]);

			pzone_data->gd__y_positon_mm[z] =
				vl53l5_gd_scale_range(
					median_range_mm,
					ptrig_coeffs->gd__trig_coeffs[r]);

			pzone_data->gd__z_positon_mm[z] = median_range_mm;

			pzone_data->gd__is_valid[z] = 1U;
			target_status = presults->per_tgt_results.target_status[t];

			pzone_data->gd__target_status[z] = target_status;
			pzone_data->gd__target_status_counts[target_status]++;

		}
	}

	pop_data->gd__merged_zone_count =
		pzone_data->gd__target_status_counts[DCI_TARGET_STATUS__RANGECOMPLETE_MERGED_PULSE];
	pop_data->gd__blurred_zone_count =
		pzone_data->gd__target_status_counts[DCI_TARGET_STATUS__TARGETDUETOBLUR];

}

void  vl53l5_gd_update_is_valid_list(
	struct vl53l5_gd__zone__meta_t   *pzone_meta,
	struct vl53l5_gd__zone__data_t   *pzone_data,
	uint8_t                          *ptgt_status_list)
{

	int32_t   z  = 0;
	uint8_t   target_status = 0U;
	uint8_t  *ptgt_status = ptgt_status_list;

	for (z = 0; z < (int32_t)pzone_meta->gd__no_of_zones; z++) {
		target_status = pzone_data->gd__target_status[z];
		pzone_data->gd__is_valid[z] = 0U;

		ptgt_status = ptgt_status_list;
		while (*ptgt_status != 0U && *ptgt_status != 255U) {
			if (target_status == *ptgt_status)
				pzone_data->gd__is_valid[z] = 1U;
			ptgt_status++;
		}
	}
}

void vl53l5_gd_find_min_max_rates_valid(
	struct vl53l5_gd__zone__meta_t      *pzone_meta,
	struct vl53l5_gd__zone__data_t      *pzone_data,
	struct dci_grp__gd__op__data_t        *pop_data,
	struct common_grp__status_t         *pstatus)
{

	uint32_t  z             = 0U;

	pstatus->status__stage_id = VL53L5_GD_STAGE_ID__FIND_MIN_MAX_RATE__VALID;
	pstatus->status__type = VL53L5_GD_ERROR_NONE;

	pop_data->gd__min__valid_rate_kcps_spad = 500000U << 11U;
	pop_data->gd__max__valid_rate_kcps_spad = 0U;
	pop_data->gd__max_valid_rate_zone = 0U;
	pop_data->gd__valid_zone_count = 0U;

	for (z = 0 ; z < (uint32_t)pzone_meta->gd__no_of_zones ; z++) {

		if (pzone_data->gd__target_status[z] == 0)
			continue;

		if (pzone_data->gd__peak_rate_kcps_per_spad[z] >
			pop_data->gd__max__valid_rate_kcps_spad) {
			pop_data->gd__max__valid_rate_kcps_spad =
				pzone_data->gd__peak_rate_kcps_per_spad[z];
			pop_data->gd__max_valid_rate_zone = (uint8_t)z;
		}

		if (pzone_data->gd__peak_rate_kcps_per_spad[z] <
			pop_data->gd__min__valid_rate_kcps_spad) {
			pop_data->gd__min__valid_rate_kcps_spad =
				pzone_data->gd__peak_rate_kcps_per_spad[z];
		}

		pop_data->gd__valid_zone_count++;
	}
}

int32_t vl53l5_gd_clip_point(
	int32_t                ip_zone,
	int32_t                grid__size,
	int32_t                exclude_outer)
{

	int32_t  col  = ip_zone % grid__size;
	int32_t  row  = ip_zone / grid__size;
	int32_t  min = exclude_outer;
	int32_t  max = grid__size - exclude_outer - 1;
	int32_t  zone = 0;

	if (col < min)
		col = min;
	if (col > max)
		col = max;

	if (row < min)
		row = min;
	if (row > max)
		row = max;

	zone = row * grid__size + col;

	return zone;
}

void vl53l5_gd_find_point(
	int32_t                       ip_zone,
	int32_t                       grid__size,
	uint32_t                      search_dir,
	int32_t                       exclude_outer,
	uint8_t                       *pis_valid_list,
	int32_t                       *pop_zone,
	struct common_grp__status_t   *perror,
	struct common_grp__status_t   *pwarning)
{

	int32_t    thresh     = 0;
	int32_t  *pthresh     = &thresh;
	int32_t    loop_start = 0;
	int32_t  *ploop_start = &loop_start;
	int32_t    loop_end   = 0;
	int32_t  *ploop_end   = &loop_end;
	int32_t    zone       = 0;
	int32_t  *pzone       = &zone;
	int32_t    zone_inc   = 0;
	int32_t  *pzone_inc   = &zone_inc;
	int32_t  zone_idxs[2] = { -1};
	int32_t  zone_dist[2] = { -1};
	int32_t  i = 0;

	perror->status__stage_id = VL53L5_GD_STAGE_ID__FIND_POINT;
	pwarning->status__stage_id = VL53L5_GD_STAGE_ID__FIND_POINT;
	pwarning->status__type = VL53L5_GD_WARNING_PLANE_POINT_NOT_FOUND;

	vl53l5_gd_search_params(
		ip_zone,
		grid__size,
		search_dir,
		exclude_outer,
		pzone,
		pzone_inc,
		pthresh,
		ploop_start,
		ploop_end);

	for (i = loop_start; i < loop_end; i++) {

		if ((*(pis_valid_list + zone)) > 0U) {

			if (zone_idxs[0] < 0) {
				zone_idxs[0] = zone;
				zone_dist[0] = thresh - i;
				if (zone_dist[0] < 0)
					zone_dist[0] = -zone_dist[0];
			}

			zone_idxs[1] = zone;
			zone_dist[1] = i - thresh;
			if (zone_dist[1] < 0)
				zone_dist[1] = -zone_dist[1];

		}
		zone += zone_inc;

	}

	if ((zone_idxs[0] > -1 && zone_idxs[1] > -1) &&
		(zone_dist[0] > 0 || zone_dist[1] > 0)) {

		*pop_zone = zone_idxs[0];
		if (zone_dist[1] > zone_dist[0])
			*pop_zone = zone_idxs[1];
		pwarning->status__type =
			VL53L5_GD_WARNING_NONE;
	}
}

void vl53l5_gd_find_possible_merged_points(
	struct vl53l5_gd_cfg_dev_t           *pcfg_dev,
	struct vl53l5_gd__zone__meta_t       *pzone_meta,
	struct vl53l5_gd__zone__data_t       *pzone_data,
	struct dci_grp__gd__op__data_t         *pop_data,
	struct vl53l5_gd__plane__points_t    *ppoints,
	uint8_t                              *ppossible_merged_zone_count,
	struct common_grp__status_t          *perror,
	struct common_grp__status_t          *pwarning)
{
	int32_t  grid__size    = (int32_t)pzone_meta->gd__grid__size;
	int32_t  exclude_outer = (int32_t)
							 pcfg_dev->gd_general_cfg.gd__exclude_outer__glass_merged;
	int32_t  search_results_zone_idxs[2] = { -1, -1};
	uint32_t  search_direction = 0;
	uint32_t  search_directions[VL53L5_GD_DEF__NO_OF_SEARCH_DIRECTIONS_MERGED_TGT];
	uint8_t  i;

	search_directions[0] = VL53L5_GD_POINT_SEARCH_DIR__HORIZONTAL;
	search_directions[1] = VL53L5_GD_POINT_SEARCH_DIR__VERTICAL;
	search_directions[2] =
		VL53L5_GD_POINT_SEARCH_DIR__DIAG_BTM_LEFT_TO_TOP_RIGHT_THRU_IP_ZONE;
	search_directions[3] =
		VL53L5_GD_POINT_SEARCH_DIR__DIAG_TOP_LEFT_TO_BTM_RIGHT_THRU_IP_ZONE;

	perror->status__stage_id = VL53L5_GD_STAGE_ID__FIND_PLANE_POINTS__TGT_MERGED;
	pwarning->status__stage_id = VL53L5_GD_STAGE_ID__FIND_PLANE_POINTS__TGT_MERGED;
	pwarning->status__type = VL53L5_GD_ERROR_NONE;

	(*ppossible_merged_zone_count) = 0;

	pop_data->gd__max_valid_rate_zone_clipped =
		pop_data->gd__max_valid_rate_zone;

	if (exclude_outer > 0) {
		pop_data->gd__max_valid_rate_zone_clipped =
			(uint8_t)vl53l5_gd_clip_point(
				(int32_t)pop_data->gd__max_valid_rate_zone,
				grid__size,
				exclude_outer);
	}

	for (i = 0; i < VL53L5_GD_DEF__NO_OF_SEARCH_DIRECTIONS_MERGED_TGT; ++i) {
		search_direction = search_directions[i];

		vl53l5_gd_find_possible_merged_point(
			(int32_t)pop_data->gd__max_valid_rate_zone_clipped,
			grid__size,
			search_direction,
			exclude_outer,
			&(pzone_data->gd__z_positon_mm[0]),
			pcfg_dev->gd_general_cfg.gd__min_tgt_behind_glass_dist_mm,
			pcfg_dev->gd_general_cfg.gd__merged_target_min_radius,
			&(pzone_data->gd__is_valid[0]),
			search_results_zone_idxs,
			perror,
			pwarning);

		if (search_results_zone_idxs[0] > -1) {
			if ((*ppossible_merged_zone_count) < VL53L5_GD_DEF__MAX_POINTS)
				ppoints->gd__plane_point_zone[(*ppossible_merged_zone_count)] =
					search_results_zone_idxs[0];
			++(*ppossible_merged_zone_count);
		}
		if (search_results_zone_idxs[1] > -1) {
			if ((*ppossible_merged_zone_count) < VL53L5_GD_DEF__MAX_POINTS)
				ppoints->gd__plane_point_zone[(*ppossible_merged_zone_count)] =
					search_results_zone_idxs[1];
			++(*ppossible_merged_zone_count);
		}
	}
	if ((*ppossible_merged_zone_count) < 3)
		pwarning->status__type = VL53L5_GD_WARNING_PLANE_POINT_NOT_FOUND;
}

void vl53l5_gd_find_possible_merged_point(
	int32_t                         ip_zone,
	int32_t                         grid__size,
	uint32_t                        search_dir,
	int32_t                         exclude_outer,
	int16_t                       *pz_positons_mm,
	int16_t                         min_tgt_behind_glass_dist_mm,
	uint8_t                         merged_target_min_radius,
	uint8_t                       *pis_valid_list,
	int32_t                       *pzone_idxs,
	struct common_grp__status_t   *perror,
	struct common_grp__status_t   *pwarning)
{

	int32_t    thresh     = 0;
	int32_t  *pthresh     = &thresh;
	int32_t    loop_start = 0;
	int32_t  *ploop_start = &loop_start;
	int32_t    loop_end   = 0;
	int32_t  *ploop_end   = &loop_end;
	int32_t    zone       = 0;
	int32_t  *pzone       = &zone;
	int32_t    zone_inc   = 0;
	int32_t  *pzone_inc   = &zone_inc;
	int32_t  zone_dist    = 0;
	int32_t  i = 0;

	perror->status__stage_id = VL53L5_GD_STAGE_ID__FIND_POINT__TGT_MERGED;
	pwarning->status__stage_id = VL53L5_GD_STAGE_ID__FIND_POINT__TGT_MERGED;

	pzone_idxs[0] = -1;
	pzone_idxs[1] = -1;

	vl53l5_gd_search_params(
		ip_zone,
		grid__size,
		search_dir,
		exclude_outer,
		pzone,
		pzone_inc,
		pthresh,
		ploop_start,
		ploop_end);

	for (i = loop_start; i < loop_end; i++) {

		if ((*(pis_valid_list + zone)) > 0U &&
			(pz_positons_mm[zone] - pz_positons_mm[ip_zone]) >
			min_tgt_behind_glass_dist_mm) {

			zone_dist = i - thresh;
			if (pzone_idxs[0] < 0 && zone_dist <= -(int32_t)merged_target_min_radius)
				pzone_idxs[0] = zone;

			if (zone_dist >= (int32_t)merged_target_min_radius)
				pzone_idxs[1] = zone;

		}
		zone += zone_inc;

	}

	if (pzone_idxs[0] == pzone_idxs[1])
		pzone_idxs[1] = -1;
}

void vl53l5_gd_search_params(
	int32_t                         ip_zone,
	int32_t                         grid__size,
	uint32_t                        search_dir,
	int32_t                         exclude_outer,
	int32_t                       *pstart_zone,
	int32_t                       *pzone_inc,
	int32_t                       *pthresh,
	int32_t                       *ploop_start,
	int32_t                       *ploop_end)
{

	int32_t  ip_col   = ip_zone % grid__size;
	int32_t  ip_row   = ip_zone / grid__size;
	int32_t start_col = exclude_outer;
	int32_t start_row = ip_row;
	int32_t end_col   = (grid__size - 1) - exclude_outer;
	int32_t end_row   = ip_row;
	(*pthresh)        = ip_col;
	(*pzone_inc)      = 1;
	(*ploop_start)    = start_col;
	(*ploop_end)      = end_col + 1;

	if (search_dir == VL53L5_GD_POINT_SEARCH_DIR__VERTICAL) {
		start_col     = ip_col;
		start_row     = exclude_outer;
		end_col       = ip_col;
		end_row       = (grid__size - 1) - exclude_outer;
		(*pthresh)    = ip_row;
		(*pzone_inc)  = grid__size;
		(*ploop_start)    = start_row;
		(*ploop_end)      = end_row + 1;
	} else if (search_dir == VL53L5_GD_POINT_SEARCH_DIR__DIAGONAL) {
		(*pthresh) = ip_col;
		if (((ip_col < (grid__size / 2)) && (ip_row < (grid__size / 2))) ||
			((ip_col >= (grid__size / 2)) && (ip_row >= (grid__size / 2)))) {
			start_col     = exclude_outer;
			start_row     = exclude_outer;
			end_col       = (grid__size - 1) - exclude_outer;
			end_row       = (grid__size - 1) - exclude_outer;
			(*pzone_inc)  = grid__size + 1;
		} else {
			start_col     = exclude_outer;
			start_row     = (grid__size - 1) - exclude_outer;
			end_col       = (grid__size - 1) - exclude_outer;
			end_row       = exclude_outer;
			(*pzone_inc)  = 1 - grid__size;
		}
		(*ploop_start)    = start_col;
		(*ploop_end)      = end_col + 1;
	} else if (search_dir ==
			   VL53L5_GD_POINT_SEARCH_DIR__DIAG_BTM_LEFT_TO_TOP_RIGHT_THRU_IP_ZONE) {
		if (ip_col >= ip_row) {

			start_col      = ip_col - (ip_row - exclude_outer);
			start_row      = exclude_outer;
			end_col        = (grid__size - 1) - exclude_outer;
			end_row        = ip_row + (((grid__size - 1) - exclude_outer) - ip_col);
		} else {

			start_col      = exclude_outer;
			start_row      = ip_row - (ip_col - exclude_outer);
			end_col        = ip_col + (((grid__size - 1) - exclude_outer) - ip_row);
			end_row        = (grid__size - 1) - exclude_outer;
		}
		(*pthresh) = ip_col;
		(*pzone_inc)   = grid__size + 1;
		(*ploop_start)    = start_col;
		(*ploop_end)      = end_col + 1;
	} else if (search_dir ==
			   VL53L5_GD_POINT_SEARCH_DIR__DIAG_TOP_LEFT_TO_BTM_RIGHT_THRU_IP_ZONE) {
		if (ip_row > ((grid__size - 1) - ip_col)) {

			start_col       = ip_col - (((grid__size - 1) - exclude_outer) - ip_row);
			start_row       = (grid__size - 1) - exclude_outer;
			end_col         = (grid__size - 1) - exclude_outer;
			end_row         = ip_row - (((grid__size - 1) - exclude_outer) - ip_col);
		} else {

			start_col       = exclude_outer;
			start_row       = ip_row + (ip_col - exclude_outer);
			end_col         = ip_col + (ip_row - exclude_outer);
			end_row         = exclude_outer;
		}
		(*pthresh) = ip_col;
		(*pzone_inc)   = 1 - grid__size;
		(*ploop_start)    = start_col;
		(*ploop_end)      = end_col + 1;
	}

	(*pstart_zone)    = (start_row * grid__size) + start_col;
}

void vl53l5_gd_find_points_glass_and_target_merged(
	int32_t                               exclude_outer,
	struct vl53l5_gd__zone__meta_t       *pzone_meta,
	struct vl53l5_gd__zone__data_t       *pzone_data,
	struct dci_grp__gd__op__data_t         *pop_data,
	struct vl53l5_gd__plane__points_t    *ppoints,
	struct common_grp__status_t          *perror,
	struct common_grp__status_t          *pwarning)
{

	int32_t  grid__size = (int32_t)pzone_meta->gd__grid__size;

	perror->status__stage_id = VL53L5_GD_STAGE_ID__FIND_PLANE_POINTS__TGT_MERGED;
	pwarning->status__stage_id = VL53L5_GD_STAGE_ID__FIND_PLANE_POINTS__TGT_MERGED;
	pwarning->status__type = VL53L5_GD_ERROR_NONE;

	pop_data->gd__max_valid_rate_zone_clipped =
		pop_data->gd__max_valid_rate_zone;

	if (exclude_outer > 0) {
		pop_data->gd__max_valid_rate_zone_clipped =
			(uint8_t)vl53l5_gd_clip_point(
				(int32_t)pop_data->gd__max_valid_rate_zone,
				grid__size,
				exclude_outer);
	}

	vl53l5_gd_find_point(
		(int32_t)pop_data->gd__max_valid_rate_zone_clipped,
		grid__size,
		VL53L5_GD_POINT_SEARCH_DIR__HORIZONTAL,
		exclude_outer,
		&(pzone_data->gd__is_valid[0]),
		&(ppoints->gd__plane_point_zone[0]),
		perror,
		pwarning);

	if (perror->status__type == VL53L5_GD_ERROR_NONE &&
		pwarning->status__type == VL53L5_GD_WARNING_NONE) {
		vl53l5_gd_find_point(
			(int32_t)pop_data->gd__max_valid_rate_zone_clipped,
			grid__size,
			VL53L5_GD_POINT_SEARCH_DIR__VERTICAL,
			exclude_outer,
			&(pzone_data->gd__is_valid[0]),
			&(ppoints->gd__plane_point_zone[1]),
			perror,
			pwarning);
	}

	if (perror->status__type == VL53L5_GD_ERROR_NONE &&
		pwarning->status__type == VL53L5_GD_WARNING_NONE) {
		vl53l5_gd_find_point(
			(int32_t)pop_data->gd__max_valid_rate_zone_clipped,
			grid__size,
			VL53L5_GD_POINT_SEARCH_DIR__DIAGONAL,
			exclude_outer,
			&(pzone_data->gd__is_valid[0]),
			&(ppoints->gd__plane_point_zone[2]),
			perror,
			pwarning);
	}
}

void vl53l5_gd_find_points_glass_only(
	int32_t                               exclude_outer,
	struct vl53l5_gd__zone__meta_t       *pzone_meta,
	struct vl53l5_gd__zone__data_t       *pzone_data,
	struct dci_grp__gd__op__data_t         *pop_data,
	struct vl53l5_gd__plane__points_t    *ppoints,
	struct common_grp__status_t          *perror,
	struct common_grp__status_t          *pwarning)
{

	int32_t  grid__size = (int32_t)pzone_meta->gd__grid__size;

	perror->status__stage_id = VL53L5_GD_STAGE_ID__FIND_PLANE_POINTS__GLASS_ONLY;
	pwarning->status__stage_id = VL53L5_GD_STAGE_ID__FIND_PLANE_POINTS__GLASS_ONLY;
	pwarning->status__type = VL53L5_GD_WARNING_NONE;

	pop_data->gd__max_valid_rate_zone_clipped =
		pop_data->gd__max_valid_rate_zone;

	if (pop_data->gd__max__valid_rate_kcps_spad > 0) {
		pop_data->gd__max_valid_rate_zone_clipped =
			(uint8_t)vl53l5_gd_clip_point(
				(int32_t)pop_data->gd__max_valid_rate_zone,
				grid__size,
				exclude_outer);
	} else {
		pwarning->status__type =
			VL53L5_GD_WARNING_MAX_VALID_RATE_ZERO;
	}

	ppoints->gd__plane_point_zone[0] =
		(int32_t)pop_data->gd__max_valid_rate_zone_clipped;

	if (perror->status__type == VL53L5_GD_ERROR_NONE &&
		pwarning->status__type == VL53L5_GD_WARNING_NONE) {
		vl53l5_gd_find_point(
			ppoints->gd__plane_point_zone[0],
			grid__size,
			VL53L5_GD_POINT_SEARCH_DIR__HORIZONTAL,
			exclude_outer,
			&(pzone_data->gd__is_valid[0]),
			&(ppoints->gd__plane_point_zone[1]),
			perror,
			pwarning);

	}

	if (perror->status__type == VL53L5_GD_ERROR_NONE &&
		pwarning->status__type == VL53L5_GD_WARNING_NONE) {
		vl53l5_gd_find_point(
			ppoints->gd__plane_point_zone[0],
			grid__size,
			VL53L5_GD_POINT_SEARCH_DIR__VERTICAL,
			exclude_outer,
			&(pzone_data->gd__is_valid[0]),
			&(ppoints->gd__plane_point_zone[2]),
			perror,
			pwarning);
	}
}

void vl53l5_gd_find_points(
	struct vl53l5_gd_cfg_dev_t           *pcfg_dev,
	struct vl53l5_gd_int_dev_t           *pint_dev,
	struct dci_patch_op_dev_t            *pop_dev,
	struct common_grp__status_t          *perror,
	struct common_grp__status_t          *pwarning)
{

	uint32_t i = 0;
	int32_t  z = 0;
	int32_t  exclude_outer = 0;

	pop_dev->gd_op_data.gd__possible_merged_zone_count = 0;
	pop_dev->gd_op_data.gd__glass_display_case_dist_mm = 0;

	for (i = 0; i < VL53L5_GD_DEF__MAX_POINTS; i++)
		pint_dev->gd_plane_points.gd__plane_point_zone[i] = -1;

	vl53l5_gd_update_is_valid_list(
		&(pint_dev->gd_zone_meta),
		&(pint_dev->gd_zone_data),
		pcfg_dev->gd_tgt_status_list_2.gd__tgt_status);

	vl53l5_gd_find_possible_merged_points(
		pcfg_dev,
		&(pint_dev->gd_zone_meta),
		&(pint_dev->gd_zone_data),
		&(pop_dev->gd_op_data),
		&(pint_dev->gd_plane_points),
		&(pop_dev->gd_op_data.gd__possible_merged_zone_count),
		perror,
		pwarning);

	if (pop_dev->gd_op_data.gd__possible_merged_zone_count >=
		pcfg_dev->gd_general_cfg.gd__min_tgt_behind_glass_merged_zones &&
		pwarning->status__type != VL53L5_GD_WARNING_PLANE_POINT_NOT_FOUND) {

		pop_dev->gd_op_data.gd__glass_display_case_dist_mm =
			pint_dev->gd_zone_data.gd__z_positon_mm[pop_dev->gd_op_data.gd__max_valid_rate_zone];
	} else {

		if (pcfg_dev->gd_general_cfg.gd__exclude_outer__glass_only > 0U) {
			exclude_outer =
				(int32_t)pcfg_dev->gd_general_cfg.gd__exclude_outer__glass_only;
		}

		vl53l5_gd_update_is_valid_list(
			&(pint_dev->gd_zone_meta),
			&(pint_dev->gd_zone_data),
			pcfg_dev->gd_tgt_status_list_1.gd__tgt_status);

		vl53l5_gd_find_points_glass_only(
			exclude_outer,
			&(pint_dev->gd_zone_meta),
			&(pint_dev->gd_zone_data),
			&(pop_dev->gd_op_data),
			&(pint_dev->gd_plane_points),
			perror,
			pwarning);
	}

	if (perror->status__type == VL53L5_GD_ERROR_NONE &&
		pwarning->status__type == VL53L5_GD_WARNING_NONE) {
		for (i = 0; i < 3; i++) {
			z = pint_dev->gd_plane_points.gd__plane_point_zone[i];
			pint_dev->gd_plane_points.gd__plane_point_x[i] =
				pint_dev->gd_zone_data.gd__x_positon_mm[z];
			pint_dev->gd_plane_points.gd__plane_point_y[i] =
				pint_dev->gd_zone_data.gd__y_positon_mm[z];
			pint_dev->gd_plane_points.gd__plane_point_z[i] =
				pint_dev->gd_zone_data.gd__z_positon_mm[z];
		}
	}
}

void vl53l5_gd_normalise_plane_coeff(
	int64_t                               ip_coeff,
	int32_t                               norm_factor,
	int32_t                              *pop_coeff,
	struct common_grp__status_t          *perror,
	struct common_grp__status_t          *pwarning)
{

	int64_t  tmp = ip_coeff * VL53L5_GD_DEF__PLANE_COEFF_UNITY_VALUE;

	perror->status__stage_id = VL53L5_GD_STAGE_ID__NORMALISE_PLANE_COEFF;
	perror->status__type = VL53L5_GD_ERROR_NONE;

	pwarning->status__stage_id = VL53L5_GD_STAGE_ID__NORMALISE_PLANE_COEFF;
	pwarning->status__type = VL53L5_GD_WARNING_PLANE_COEFF_C_ZERO;

	if (norm_factor != 0) {
		pwarning->status__type = VL53L5_GD_WARNING_NONE;

		if (tmp < 0)
			tmp -= (int64_t)(norm_factor / 2);
		else
			tmp += (int64_t)(norm_factor / 2);

		tmp = DIVS_64D64_64(tmp, (int64_t)norm_factor);

		if (tmp > (int64_t)VL53L5_INT32_MAX)
			tmp = (int64_t)VL53L5_INT32_MAX;
		if (tmp < (int64_t)VL53L5_INT32_MIN)
			tmp = (int64_t)VL53L5_INT32_MIN;

		*pop_coeff = (int32_t)tmp;
	}

}

void vl53l5_gd_plane_fit(
	struct vl53l5_gd__plane__points_t    *ppoints,
	struct vl53l5_gd__plane__coeffs_t    *pcoeffs,
	struct common_grp__status_t          *perror,
	struct common_grp__status_t          *pwarning)
{

	int32_t  plane_a = 0;
	int32_t  plane_b = 0;
	int32_t  plane_c = 0;
	int64_t  plane_d = 0;

	plane_a  = ((ppoints->gd__plane_point_y[1] - ppoints->gd__plane_point_y[0]) *
				(ppoints->gd__plane_point_z[2] - ppoints->gd__plane_point_z[0]));
	plane_a -= ((ppoints->gd__plane_point_y[2] - ppoints->gd__plane_point_y[0]) *
				(ppoints->gd__plane_point_z[1] - ppoints->gd__plane_point_z[0]));

	plane_b  = ((ppoints->gd__plane_point_z[1] - ppoints->gd__plane_point_z[0]) *
				(ppoints->gd__plane_point_x[2] - ppoints->gd__plane_point_x[0]));
	plane_b -= ((ppoints->gd__plane_point_z[2] - ppoints->gd__plane_point_z[0]) *
				(ppoints->gd__plane_point_x[1] - ppoints->gd__plane_point_x[0]));

	plane_c  = ((ppoints->gd__plane_point_x[1] - ppoints->gd__plane_point_x[0]) *
				(ppoints->gd__plane_point_y[2] - ppoints->gd__plane_point_y[0]));
	plane_c -= ((ppoints->gd__plane_point_x[2] - ppoints->gd__plane_point_x[0]) *
				(ppoints->gd__plane_point_y[1] - ppoints->gd__plane_point_y[0]));

	plane_d  = 0;
	plane_d -= MULS_32X32_64(plane_a, ppoints->gd__plane_point_x[0]);
	plane_d -= MULS_32X32_64(plane_b, ppoints->gd__plane_point_y[0]);
	plane_d -= MULS_32X32_64(plane_c, ppoints->gd__plane_point_z[0]);

	if (perror->status__type == VL53L5_GD_ERROR_NONE &&
		pwarning->status__type == VL53L5_GD_WARNING_NONE) {
		vl53l5_gd_normalise_plane_coeff(
			(int64_t)plane_a,
			plane_c,
			&(pcoeffs->gd__plane_coeff_a),
			perror,
			pwarning);
	}

	if (perror->status__type == VL53L5_GD_ERROR_NONE &&
		pwarning->status__type == VL53L5_GD_WARNING_NONE) {
		vl53l5_gd_normalise_plane_coeff(
			(int64_t)plane_b,
			plane_c,
			&(pcoeffs->gd__plane_coeff_b),
			perror,
			pwarning);
	}

	if (perror->status__type == VL53L5_GD_ERROR_NONE &&
		pwarning->status__type == VL53L5_GD_WARNING_NONE) {
		vl53l5_gd_normalise_plane_coeff(
			plane_d,
			plane_c,
			&(pcoeffs->gd__plane_coeff_d),
			perror,
			pwarning);
	}

	if (perror->status__type == VL53L5_GD_ERROR_NONE &&
		pwarning->status__type == VL53L5_GD_WARNING_NONE)
		pcoeffs->gd__plane_coeff_c = VL53L5_GD_DEF__PLANE_COEFF_UNITY_VALUE;
}

int16_t vl53l5_gd_calc_plane_point(
	int16_t                                    x,
	int16_t                                    y,
	struct vl53l5_gd__plane__coeffs_t    *pcoeffs)
{

	int64_t  z = 0;

	z -= MULS_32X32_64(x, pcoeffs->gd__plane_coeff_a);
	z -= MULS_32X32_64(y, pcoeffs->gd__plane_coeff_b);
	z -= (int64_t)pcoeffs->gd__plane_coeff_d;

	if (z < 0)
		z -= (int64_t)(pcoeffs->gd__plane_coeff_c / 2);
	else
		z += (int64_t)(pcoeffs->gd__plane_coeff_c / 2);
	z = DIVS_64D64_64(z, pcoeffs->gd__plane_coeff_c);

	if (z > (int64_t)VL53L5_GD_DEF__DISTANCE_MM_MAX_VALUE)
		z = (int64_t)VL53L5_GD_DEF__DISTANCE_MM_MAX_VALUE;

	if (z < (int64_t)VL53L5_GD_DEF__DISTANCE_MM_MIN_VALUE)
		z = (int64_t)VL53L5_GD_DEF__DISTANCE_MM_MIN_VALUE;

	return (int16_t)z;
}

void vl53l5_gd_calc_distance_from_plane(
	struct vl53l5_gd_cfg_dev_t           *pcfg_dev,
	struct vl53l5_gd_int_dev_t           *pint_dev,
	struct common_grp__status_t          *pstatus)
{

	struct vl53l5_gd__zone__data_t    *pdata   = &(pint_dev->gd_zone_data);
	struct vl53l5_gd__plane__coeffs_t *pcoeffs = &(pint_dev->gd_plane_coeffs);
	struct vl53l5_gd__plane__status_t *pplane  = &(pint_dev->gd_plane_status);

	int32_t  z = 0;
	int64_t  tmp64 = 0;
	uint32_t numerator = 0U;
	uint32_t denominator = 0U;
	uint32_t dist_mm = 0U;

	pstatus->status__stage_id = VL53L5_GD_STAGE_ID__CALC_DISTANCE_FROM_PLANE;
	pstatus->status__type = VL53L5_GD_ERROR_NONE;

	tmp64  = MULS_32X32_64(pcoeffs->gd__plane_coeff_a, pcoeffs->gd__plane_coeff_a);
	if (tmp64 >= (int64_t)VL53L5_UINT32_MAX)
		tmp64 = (int64_t)VL53L5_UINT32_MAX;
	else {

		tmp64 += MULS_32X32_64(pcoeffs->gd__plane_coeff_b, pcoeffs->gd__plane_coeff_b);
		if (tmp64 >= (int64_t)VL53L5_UINT32_MAX)
			tmp64 = (int64_t)VL53L5_UINT32_MAX;
		else {
			tmp64 += MULS_32X32_64(pcoeffs->gd__plane_coeff_c, pcoeffs->gd__plane_coeff_c);
			if (tmp64 > (int64_t)VL53L5_UINT32_MAX)
				tmp64 = (int64_t)VL53L5_UINT32_MAX;
		}
	}

	denominator = INTEGER_SQRT((uint32_t)tmp64);

	if (denominator == 0) {
		pstatus->status__type = VL53L5_GD_ERROR_DIVISION_BY_ZERO;
		return;
	}

	pplane->gd__dist_max_from_plane_mm = 0U;
	pplane->gd__dist_max_from_point_not_on_plane_mm = 0U;
	pplane->gd__points_on_plane = 0U;
	pplane->gd__points_not_on_of_plane = 0U;
	pplane->gd__total_valid_points = 0U;

	for (z = 0; z < (int32_t)pint_dev->gd_zone_meta.gd__no_of_zones; z++) {

		if (pdata->gd__target_status[z] == 0)
			continue;

		tmp64 =
			MULS_32X32_64(
				(int32_t)pdata->gd__x_positon_mm[z],
				pcoeffs->gd__plane_coeff_a);
		tmp64 +=
			MULS_32X32_64(
				(int32_t)pdata->gd__y_positon_mm[z],
				pcoeffs->gd__plane_coeff_b);
		tmp64 +=
			MULS_32X32_64(
				(int32_t)pdata->gd__z_positon_mm[z],
				pcoeffs->gd__plane_coeff_c);
		tmp64 +=
			(int64_t)pcoeffs->gd__plane_coeff_d;

		if (tmp64 < 0)
			tmp64 = -tmp64;
		if (tmp64 > (int64_t)VL53L5_UINT32_MAX)
			tmp64 = (int64_t)VL53L5_UINT32_MAX;

		numerator = (uint32_t)tmp64;

		dist_mm = numerator;

		if ((VL53L5_UINT32_MAX - dist_mm) > (denominator >> 1U))
			dist_mm += (denominator >> 1U);
		else
			dist_mm = VL53L5_UINT32_MAX;

		dist_mm /= denominator;

		if (dist_mm > VL53L5_INT16_MAX)
			dist_mm = VL53L5_INT16_MAX;

		pdata->gd__dist_from_plane_mm[z] = (int16_t)dist_mm;

		if (dist_mm < (uint32_t)pcfg_dev->gd_general_cfg.gd__max_dist_from_plane_mm) {
			pdata->gd__is_valid[z] = 1U;
			pplane->gd__points_on_plane++;

			if (dist_mm > (uint32_t)pplane->gd__dist_max_from_plane_mm)
				pplane->gd__dist_max_from_plane_mm = (int16_t)dist_mm;
		} else {
			pdata->gd__is_valid[z] = 0U;
			pplane->gd__points_not_on_of_plane++;

			if (dist_mm > (uint32_t)pplane->gd__dist_max_from_point_not_on_plane_mm)
				pplane->gd__dist_max_from_point_not_on_plane_mm = (int16_t)dist_mm;
		}

		pplane->gd__total_valid_points++;
	}

}

void vl53l5_gd_find_min_max_rates_plane(
	struct vl53l5_gd_int_dev_t           *pint_dev,
	struct dci_patch_op_dev_t            *pop_dev,
	struct common_grp__status_t          *pstatus)
{

	uint32_t  z             = 0U;

	struct  vl53l5_gd__zone__data_t *pip = &(pint_dev->gd_zone_data);
	struct  dci_grp__gd__op__data_t   *pop = &(pop_dev->gd_op_data);

	pstatus->status__stage_id = VL53L5_GD_STAGE_ID__FIND_MIN_MAX_RATES__PLANE;
	pstatus->status__type = VL53L5_GD_ERROR_NONE;

	pop->gd__min__plane_rate_kcps_spad = 500000U << 11U;
	pop->gd__max__plane_rate_kcps_spad = 0U;
	pop->gd__plane_zone_count = 0U;

	for (z = 0 ; z < (uint32_t)pint_dev->gd_zone_meta.gd__no_of_zones ; z++) {
		if (pip->gd__is_valid[z] == 0)
			continue;

		if (pip->gd__peak_rate_kcps_per_spad[z] >
			pop->gd__max__plane_rate_kcps_spad) {
			pop->gd__max__plane_rate_kcps_spad =
				pip->gd__peak_rate_kcps_per_spad[z];
		}

		if (pip->gd__peak_rate_kcps_per_spad[z] <
			pop->gd__min__plane_rate_kcps_spad) {
			pop->gd__min__plane_rate_kcps_spad =
				pip->gd__peak_rate_kcps_per_spad[z];
		}

		pop->gd__plane_zone_count++;
	}
}

void vl53l5_gd_calc_rate_ratio(
	struct vl53l5_gd__general__cfg_t    *pcfg,
	struct dci_grp__gd__op__data_t      *pgd_data,
	struct dci_grp__gd__op__status_t    *pgd_status,
	struct common_grp__status_t         *pstatus)
{

	uint64_t ratio = 0U;
	uint64_t max_rate_kcps_per_spad =
		(uint64_t)pgd_data->gd__max__plane_rate_kcps_spad;
	uint64_t min_rate_kcps_per_spad =
		(uint64_t)pgd_data->gd__min__plane_rate_kcps_spad;

	pstatus->status__stage_id = VL53L5_GD_STAGE_ID__CALC_RATE_RATIO;
	pstatus->status__type = VL53L5_GD_ERROR_NONE;

	if (pgd_data->gd__possible_merged_zone_count >=
		pcfg->gd__min_tgt_behind_glass_merged_zones) {

		max_rate_kcps_per_spad = (uint64_t)pgd_data->gd__max__valid_rate_kcps_spad;
		min_rate_kcps_per_spad = (uint64_t)pgd_data->gd__min__valid_rate_kcps_spad;

		max_rate_kcps_per_spad = max_rate_kcps_per_spad * 100U;
		max_rate_kcps_per_spad -=
			(min_rate_kcps_per_spad * (uint64_t)
			 pcfg->gd__glass_merged__rate_subtraction_pc);
		max_rate_kcps_per_spad += 50U;
		max_rate_kcps_per_spad = DIV_64D64_64(max_rate_kcps_per_spad, 100U);

		min_rate_kcps_per_spad =
			(100U - (uint64_t)pcfg->gd__glass_merged__rate_subtraction_pc) *
			min_rate_kcps_per_spad;
		min_rate_kcps_per_spad += 50U;
		min_rate_kcps_per_spad = DIV_64D64_64(min_rate_kcps_per_spad, 100U);

	}

	if (pgd_data->gd__plane_zone_count > 2U && min_rate_kcps_per_spad > 0U) {
		ratio  = max_rate_kcps_per_spad << (uint64_t)
				 VL53L5_GD_DEF__RATE_RATIO_FRAC_BITS;
		ratio += (min_rate_kcps_per_spad >> 1U);
		ratio  = DIV_64D64_64(ratio, min_rate_kcps_per_spad);

		if (ratio > (uint64_t)VL53L5_GD_DEF__RATE_RATIO_MAX_VALUE)
			ratio = (uint64_t)VL53L5_GD_DEF__RATE_RATIO_MAX_VALUE;
	}

	pgd_status->gd__rate_ratio = (uint32_t)ratio;
}

uint8_t  vl53l5_gd_confidence_factor(
	uint16_t    rate_ratio_lower,
	uint16_t    rate_ratio_upper,
	uint32_t    rate_ratio)
{
	uint32_t  num  = 0;
	uint32_t  den  = 1;
	uint8_t   conf = 0;

	if (rate_ratio <= ((uint32_t)rate_ratio_lower << 8))
		conf = 0;
	else if (rate_ratio >= ((uint32_t)rate_ratio_upper << 8))
		conf = 100;
	else {

		num = 100U * (rate_ratio - ((uint32_t)rate_ratio_lower << 8));

		if (rate_ratio_upper != rate_ratio_lower) {
			den = ((uint32_t)rate_ratio_upper << 8) - ((uint32_t)rate_ratio_lower <<
					8);

		} else {

			num = 0;
			den = 1;
		}
		conf = (uint8_t)((num + (den >> 1)) / den);

	}
	return conf;
}

void vl53l5_gd_calc_points_on_plane_pc(
	struct dci_grp__gd__op__data_t      *pdata,
	struct common_grp__status_t         *pstatus)
{

	uint32_t percent = 0U;

	pstatus->status__stage_id = VL53L5_GD_STAGE_ID__CALC_POINTS_ON_PLANE_PC;
	pstatus->status__type = VL53L5_GD_ERROR_NONE;

	pdata->gd__points_on_plane_pc = 0U;

	if (pdata->gd__valid_zone_count > 0U) {
		percent  = 100U * (uint32_t)pdata->gd__plane_zone_count;
		percent += (uint32_t)(pdata->gd__valid_zone_count >> 1U);
		percent /= (uint32_t)pdata->gd__valid_zone_count;

		if (percent > (uint32_t)VL53L5_GD_DEF__PERCENT_MAX_VALUE)
			percent = (uint32_t)VL53L5_GD_DEF__PERCENT_MAX_VALUE;
		pdata->gd__points_on_plane_pc = (uint8_t)percent;
	}
}

void vl53l5_gd_update_output_status(
	struct vl53l5_gd_cfg_dev_t          *pcfg_dev,
	struct vl53l5_gd_int_dev_t          *pint_dev,
	struct dci_patch_op_dev_t           *pop_dev,
	struct common_grp__status_t         *pstatus)

{

	int32_t  dist_mm = 0;
	int32_t  tmp32   = 0;

	struct  vl53l5_gd__general__cfg_t  *pcfg = &(pcfg_dev->gd_general_cfg);
	struct  dci_grp__gd__op__data_t    *pdata = &(pop_dev->gd_op_data);
	struct  dci_grp__gd__op__status_t  *pflags = &(pop_dev->gd_op_status);

	pstatus->status__stage_id = VL53L5_GD_STAGE_ID__UPDATE_OUTPUT_STATUS;
	pstatus->status__type = VL53L5_GD_ERROR_NONE;

	if (pint_dev->gd_plane_coeffs.gd__plane_coeff_d > VL53L5_INT32_MIN)
		dist_mm = -pint_dev->gd_plane_coeffs.gd__plane_coeff_d;
	else
		dist_mm = VL53L5_INT32_MAX;

	if (dist_mm < 0) {

		if ((VL53L5_INT32_MIN - dist_mm) < -(VL53L5_GD_DEF__PLANE_COEFF_UNITY_VALUE /
											 2))
			dist_mm -= (VL53L5_GD_DEF__PLANE_COEFF_UNITY_VALUE / 2);
		else
			dist_mm = VL53L5_INT32_MIN;
	} else {

		if ((VL53L5_INT32_MAX - dist_mm) > (VL53L5_GD_DEF__PLANE_COEFF_UNITY_VALUE / 2))
			dist_mm += (VL53L5_GD_DEF__PLANE_COEFF_UNITY_VALUE / 2);
		else
			dist_mm = VL53L5_INT32_MAX;
	}
	dist_mm /= VL53L5_GD_DEF__PLANE_COEFF_UNITY_VALUE;

	tmp32 = dist_mm - pdata->gd__glass_display_case_dist_mm;
	if (tmp32 > (int32_t)VL53L5_INT16_MAX)
		tmp32 = (int32_t)VL53L5_INT16_MAX;
	if (tmp32 < (int32_t)VL53L5_INT16_MIN)
		tmp32 = (int32_t)VL53L5_INT16_MIN;
	pdata->gd__glass_merged_dist_mm = (int16_t)tmp32;

	pflags->gd__plane_detected = 0U;
	if (pdata->gd__points_on_plane_pc >= pcfg->gd__min_points_on_plane_pc)
		pflags->gd__plane_detected = 1U;

	pflags->gd__ratio_detected = 0U;
	if (pflags->gd__rate_ratio > pcfg->gd__min_rate_ratio)
		pflags->gd__ratio_detected = 1U;

	pflags->gd__tgt_behind_glass_detected = 0U;
	if (pdata->gd__possible_merged_zone_count >=
		pcfg->gd__min_tgt_behind_glass_merged_zones &&
		pflags->gd__plane_detected == 1U &&
		pflags->gd__ratio_detected == 1U &&
		pdata->gd__glass_merged_dist_mm > pcfg->gd__max_dist_from_plane_mm)
		pflags->gd__tgt_behind_glass_detected = 1U;

	pflags->gd__glass_detected = 0U;
	if (pflags->gd__plane_detected == 1U &&
		pflags->gd__ratio_detected == 1U) {
		if (pflags->gd__tgt_behind_glass_detected == 0U)
			pflags->gd__glass_detected = 1U;
	} else
		pflags->gd__glass_detected = 0U;

	pflags->gd__mirror_detected = 0U;
	pflags->gd__op_spare_0 = 0U;
	pflags->gd__op_spare_1 = 0U;

	pdata->gd__mirror_dist_mm = 0;
	pdata->gd__target_behind_mirror_dist_mm = 0;
	pdata->gd__target_behind_mirror_min_dist_mm = 0;
	pdata->gd__target_behind_mirror_max_dist_mm = 0;
	pdata->gd__possible_tgt2_behind_mirror_zone_count = 0U;
	pdata->gd__op_data_spare_0 = 0U;
	pdata->gd__tgt2_behind_mirror_plane_zone_count = 0U;
	pdata->gd__tgt2_behind_mirror_points_on_plane_pc = 0U;

	pflags->gd__confidence = 0U;

	if (pflags->gd__plane_detected == 1U) {
		pflags->gd__confidence = vl53l5_gd_confidence_factor(
									 pcfg->gd__rate_ratio_lower_confidence_glass,
									 pcfg->gd__rate_ratio_upper_confidence_glass,
									 pflags->gd__rate_ratio);
	}

	if (pflags->gd__tgt_behind_glass_detected == 1U) {
		pflags->gd__confidence = vl53l5_gd_confidence_factor(
									 pcfg->gd__rate_ratio_lower_confidence_tgt_merged,
									 pcfg->gd__rate_ratio_upper_confidence_tgt_merged,
									 pflags->gd__rate_ratio);
	} else if (pflags->gd__mirror_detected == 1U) {
		pflags->gd__confidence = vl53l5_gd_confidence_factor(
									 pcfg->gd__rate_ratio_lower_confidence_mirror,
									 pcfg->gd__rate_ratio_upper_confidence_mirror,
									 pflags->gd__rate_ratio);
	}
}
