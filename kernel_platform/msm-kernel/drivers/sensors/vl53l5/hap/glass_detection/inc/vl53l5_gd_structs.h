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

#ifndef __VL53L5_GD_STRUCTS_H__
#define __VL53L5_GD_STRUCTS_H__

#include "vl53l5_gd_defs.h"
#include "vl53l5_gd_luts.h"
#include "vl53l5_gd_structs.h"
#include "vl53l5_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct vl53l5_gd__general__cfg_t {

	uint32_t gd__min_rate_ratio;

	int16_t gd__max_dist_from_plane_mm;

	int16_t gd__min_tgt_behind_glass_dist_mm;

	int16_t gd__max_dist_from_plane_mirror_mm;

	uint16_t gd__rate_ratio_lower_confidence_glass;

	uint16_t gd__rate_ratio_upper_confidence_glass;

	uint16_t gd__rate_ratio_lower_confidence_tgt_merged;

	uint16_t gd__rate_ratio_upper_confidence_tgt_merged;

	uint16_t gd__rate_ratio_lower_confidence_mirror;

	uint16_t gd__rate_ratio_upper_confidence_mirror;

	uint8_t gd__mode;

	uint8_t gd__min_valid_zones;

	uint8_t gd__min_tgt_behind_glass_merged_zones;

	uint8_t gd__min_tgt2_behind_mirror_zones;

	uint8_t gd__min_points_on_plane_pc;

	uint8_t gd__exclude_outer__glass_merged;

	uint8_t gd__exclude_outer__glass_only;

	uint8_t gd__exclude_outer__mirror;

	uint8_t gd__glass_merged__rate_subtraction_pc;

	uint8_t gd__merged_target_min_radius;

	uint8_t gd__mirror_min_radius;

	uint8_t gd__cfg_spare_0;

	uint8_t gd__cfg_spare_1;

	uint8_t gd__cfg_spare_2;
};

struct vl53l5_gd__tgt_status__cfg_t {

	uint8_t gd__tgt_status[VL53L5_GD_DEF__MAX_TGT_STATUS_LIST_SIZE];
};

struct vl53l5_gd__zone__meta_t {

	uint8_t gd__max_zones;

	uint8_t gd__no_of_zones;

	uint8_t gd__grid__size;

	uint8_t gd__grid__pitch_spad;
};

struct vl53l5_gd__zone__data_t {

	uint32_t gd__peak_rate_kcps_per_spad[VL53L5_GD_DEF__MAX_ZONES];

	int16_t gd__x_positon_mm[VL53L5_GD_DEF__MAX_ZONES];

	int16_t gd__y_positon_mm[VL53L5_GD_DEF__MAX_ZONES];

	int16_t gd__z_positon_mm[VL53L5_GD_DEF__MAX_ZONES];

	int16_t gd__dist_from_plane_mm[VL53L5_GD_DEF__MAX_ZONES];

	uint8_t gd__is_valid[VL53L5_GD_DEF__MAX_ZONES];

	uint8_t gd__target_status[VL53L5_GD_DEF__MAX_ZONES];

	uint8_t gd__target_status_counts[VL53L5_GD_DEF__MAX_COUNTS];
};

struct vl53l5_gd__trig__coeffs_t {

	int16_t gd__trig_coeffs[VL53L5_GD_DEF__MAX_COEFFS];
};

struct vl53l5_gd__plane__points_t {

	int32_t gd__plane_point_x[VL53L5_GD_DEF__MAX_POINTS];

	int32_t gd__plane_point_y[VL53L5_GD_DEF__MAX_POINTS];

	int32_t gd__plane_point_z[VL53L5_GD_DEF__MAX_POINTS];

	int32_t gd__plane_point_zone[VL53L5_GD_DEF__MAX_POINTS];
};

struct vl53l5_gd__plane__coeffs_t {

	int32_t gd__plane_coeff_a;

	int32_t gd__plane_coeff_b;

	int32_t gd__plane_coeff_c;

	int32_t gd__plane_coeff_d;
};

struct vl53l5_gd__plane__status_t {

	int16_t gd__dist_max_from_plane_mm;

	int16_t gd__dist_max_from_point_not_on_plane_mm;

	uint8_t gd__points_on_plane;

	uint8_t gd__points_not_on_of_plane;

	uint8_t gd__total_valid_points;

	uint8_t gd__spare_0;
};

#ifdef __cplusplus
}
#endif

#endif
