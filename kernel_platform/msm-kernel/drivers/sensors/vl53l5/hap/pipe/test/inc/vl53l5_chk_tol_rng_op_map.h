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

#ifndef __VL53L5_CHK_TOL_RNG_OP_MAP_H__
#define __VL53L5_CHK_TOL_RNG_OP_MAP_H__

#include "vl53l5_utils_chk_structs.h"
#include "vl53l5_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct vl53l5_chk_tol_rng_op_dev_t {

	struct vl53l5_chk__tolerance_t tol__rng_meta_data;

	struct vl53l5_chk__tolerance_t tol__rng_common_data;

	struct vl53l5_chk__tolerance_t tol__rng_timing_data;

	struct vl53l5_chk__tolerance_t tol__amb_rate_kcps_per_spad;

	struct vl53l5_chk__tolerance_t tol__rng__effective_spad_count;

	struct vl53l5_chk__tolerance_t tol__amb_dmax_mm;

	struct vl53l5_chk__tolerance_t tol__silicon_temp_degc__start;

	struct vl53l5_chk__tolerance_t tol__silicon_temp_degc__end;

	struct vl53l5_chk__tolerance_t tol__rng__no_of_targets;

	struct vl53l5_chk__tolerance_t tol__rng__zone_id;

	struct vl53l5_chk__tolerance_t tol__rng__sequence_idx;

	struct vl53l5_chk__tolerance_t tol__peak_rate_kcps_per_spad;

	struct vl53l5_chk__tolerance_t tol__median_phase;

	struct vl53l5_chk__tolerance_t tol__rate_sigma_kcps_per_spad;

	struct vl53l5_chk__tolerance_t tol__target_zscore;

	struct vl53l5_chk__tolerance_t tol__range_sigma_mm;

	struct vl53l5_chk__tolerance_t tol__median_range_mm;

	struct vl53l5_chk__tolerance_t tol__start_range_mm;

	struct vl53l5_chk__tolerance_t tol__end_range_mm;

	struct vl53l5_chk__tolerance_t tol__min_range_delta_mm;

	struct vl53l5_chk__tolerance_t tol__max_range_delta_mm;

	struct vl53l5_chk__tolerance_t tol__target_reflectance_est_pc;

	struct vl53l5_chk__tolerance_t tol__target_status;

	struct vl53l5_chk__tolerance_t tol__ref_timing_data;

	struct vl53l5_chk__tolerance_t tol__ref_channel_data;

	struct vl53l5_chk__tolerance_t tol__ref_target_data;

};

#ifdef __cplusplus
}
#endif

#endif
