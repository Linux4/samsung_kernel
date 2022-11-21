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

#ifndef VL53L5_GLASS_DETECTION_FUNCS_H_
#define VL53L5_GLASS_DETECTION_FUNCS_H_

#include "vl53l5_types.h"
#include "vl53l5_results_config.h"
#include "common_datatype_structs.h"
#include "vl53l5_results_map.h"
#include "dci_patch_op_map.h"
#include "vl53l5_gd_cfg_map.h"
#include "vl53l5_gd_int_map.h"
#include "vl53l5_gd_err_map.h"

void vl53l5_gd_clear_data(
	struct vl53l5_gd_int_dev_t          *pint_dev,
	struct dci_patch_op_dev_t           *pop_dev,
	struct vl53l5_gd_err_dev_t          *perr_dev);

void vl53l5_gd_init_zone_meta_data(
	struct vl53l5_range_common_data_t   *prng_common_data,
	struct vl53l5_gd__zone__meta_t      *pzone_meta,
	struct common_grp__status_t         *pstatus);

void vl53l5_gd_init_trig_coeffs(
	struct vl53l5_gd__zone__meta_t      *pzone_meta,
	struct vl53l5_gd__trig__coeffs_t    *ptrig_coeffs,
	struct common_grp__status_t         *pstatus);

int16_t vl53l5_gd_scale_range(
	int16_t  median_range_mm,
	int16_t  scale_factor);

uint32_t  vl53l5_gd_is_target_valid(
	uint32_t    target_idx,
	uint32_t    rng__no_of_targets,
	uint32_t    target_status,
	uint8_t    *ptgt_status_list);

void vl53l5_gd_filter_results(
	struct vl53l5_range_results_t                 *presults,
	struct vl53l5_gd_cfg_dev_t          *pcfg_dev,
	struct vl53l5_gd_int_dev_t          *pint_dev,
	struct dci_patch_op_dev_t           *pop_dev,
	struct common_grp__status_t         *pstatus);

void  vl53l5_gd_update_is_valid_list(
	struct vl53l5_gd__zone__meta_t   *pzone_meta,
	struct vl53l5_gd__zone__data_t   *pzone_data,
	uint8_t                          *ptgt_status_list);

void vl53l5_gd_find_min_max_rates_valid(
	struct vl53l5_gd__zone__meta_t      *pzone_meta,
	struct vl53l5_gd__zone__data_t      *pzone_data,
	struct dci_grp__gd__op__data_t        *pop_data,
	struct common_grp__status_t         *pstatus);

int32_t vl53l5_gd_clip_point(
	int32_t                ip_zone,
	int32_t                grid__size,
	int32_t                exclude_outer);

void vl53l5_gd_find_point(
	int32_t                       ip_zone,
	int32_t                       grid__size,
	uint32_t                      search_dir,
	int32_t                       exclude_outer,
	uint8_t                       *pis_valid_list,
	int32_t                       *pop_zone,
	struct common_grp__status_t   *perror,
	struct common_grp__status_t   *pwarning);

void vl53l5_gd_find_possible_merged_points(
	struct vl53l5_gd_cfg_dev_t           *pcfg_dev,
	struct vl53l5_gd__zone__meta_t       *pzone_meta,
	struct vl53l5_gd__zone__data_t       *pzone_data,
	struct dci_grp__gd__op__data_t         *pop_data,
	struct vl53l5_gd__plane__points_t    *ppoints,
	uint8_t                              *ppossible_merged_zone_count,
	struct common_grp__status_t          *perror,
	struct common_grp__status_t          *pwarning);

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
	struct common_grp__status_t   *pwarning);

void vl53l5_gd_search_params(
	int32_t                         ip_zone,
	int32_t                         grid__size,
	uint32_t                        search_dir,
	int32_t                         exclude_outer,
	int32_t                       *pstart_zone,
	int32_t                       *pzone_inc,
	int32_t                       *pthresh,
	int32_t                       *ploop_start,
	int32_t                       *ploop_end);

void vl53l5_gd_find_points_glass_and_target_merged(
	int32_t                               exclude_outer,
	struct vl53l5_gd__zone__meta_t       *pzone_meta,
	struct vl53l5_gd__zone__data_t       *pzone_data,
	struct dci_grp__gd__op__data_t         *pop_data,
	struct vl53l5_gd__plane__points_t    *ppoints,
	struct common_grp__status_t          *perror,
	struct common_grp__status_t          *pwarning);

void vl53l5_gd_find_points_glass_only(
	int32_t                               exclude_outer,
	struct vl53l5_gd__zone__meta_t       *pzone_meta,
	struct vl53l5_gd__zone__data_t       *pzone_data,
	struct dci_grp__gd__op__data_t         *pop_data,
	struct vl53l5_gd__plane__points_t    *ppoints,
	struct common_grp__status_t          *perror,
	struct common_grp__status_t          *pwarning);

void vl53l5_gd_find_points(
	struct vl53l5_gd_cfg_dev_t           *pcfg_dev,
	struct vl53l5_gd_int_dev_t           *pint_dev,
	struct dci_patch_op_dev_t            *pop_dev,
	struct common_grp__status_t          *perror,
	struct common_grp__status_t          *pwarning);

void vl53l5_gd_normalise_plane_coeff(
	int64_t                               ip_coeff,
	int32_t                               norm_factor,
	int32_t                              *pop_coeff,
	struct common_grp__status_t          *perror,
	struct common_grp__status_t          *pwarning);

void vl53l5_gd_plane_fit(
	struct vl53l5_gd__plane__points_t    *ppoints,
	struct vl53l5_gd__plane__coeffs_t    *pcoeffs,
	struct common_grp__status_t          *perror,
	struct common_grp__status_t          *pwarning);

int16_t vl53l5_gd_calc_plane_point(
	int16_t                                    x,
	int16_t                                    y,
	struct vl53l5_gd__plane__coeffs_t    *pcoeffs);

void vl53l5_gd_calc_distance_from_plane(
	struct vl53l5_gd_cfg_dev_t           *pcfg_dev,
	struct vl53l5_gd_int_dev_t           *pint_dev,
	struct common_grp__status_t          *pstatus);

void vl53l5_gd_find_min_max_rates_plane(
	struct vl53l5_gd_int_dev_t           *pint_dev,
	struct dci_patch_op_dev_t            *pop_dev,
	struct common_grp__status_t          *pstatus);

void vl53l5_gd_calc_rate_ratio(
	struct vl53l5_gd__general__cfg_t    *pcfg,
	struct dci_grp__gd__op__data_t      *pgd_data,
	struct dci_grp__gd__op__status_t    *pgd_status,
	struct common_grp__status_t         *pstatus);

uint8_t  vl53l5_gd_confidence_factor(
	uint16_t    rate_ratio_lower,
	uint16_t    rate_ratio_upper,
	uint32_t    rate_ratio);

void vl53l5_gd_calc_points_on_plane_pc(
	struct dci_grp__gd__op__data_t      *pdata,
	struct common_grp__status_t         *pstatus);

void vl53l5_gd_update_output_status(
	struct vl53l5_gd_cfg_dev_t           *pcfg_dev,
	struct vl53l5_gd_int_dev_t           *pint_dev,
	struct dci_patch_op_dev_t            *pop_dev,
	struct common_grp__status_t          *pstatus);

#endif
