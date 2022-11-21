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

#ifndef VL53L5_ANTI_FLICKER_FILTER_STRUCTS_H_
#define VL53L5_ANTI_FLICKER_FILTER_STRUCTS_H_

#include "vl53l5_types.h"
#include "vl53l5_results_config.h"

#ifdef __cplusplus
extern "C" {
#endif

struct vl53l5_aff_tgt_data_t {

	uint32_t peak_rate_kcps_per_spad;

	uint32_t median_phase;

	uint32_t rate_sigma_kcps_per_spad;

	uint16_t target_zscore;

	uint16_t range_sigma_mm;

	int16_t median_range_mm;

	int16_t start_range_mm;

	int16_t end_range_mm;

	uint8_t min_range_delta_mm;

	uint8_t max_range_delta_mm;

	uint8_t target_reflectance_est_pc;

	uint8_t target_status;

	int16_t filled;

};

struct vl53l5_aff_tgt_out_t {

	uint32_t peak_rate_kcps_per_spad;

	uint32_t median_phase;

	uint32_t rate_sigma_kcps_per_spad;

	int32_t target_zscore;

	uint16_t range_sigma_mm;

	int16_t median_range_mm;

	int16_t start_range_mm;

	int16_t end_range_mm;

	uint8_t min_range_delta_mm;

	uint8_t max_range_delta_mm;

	uint8_t target_reflectance_est_pc;

	uint8_t target_status;

	int16_t filled;

	int16_t spare;

	struct vl53l5_aff_tgt_hist_t          *ptgt_hist;

};

struct vl53l5_aff_tgt_hist_t {

	int32_t *pfilled;

	struct vl53l5_aff_tgt_data_t *pdata_arr;

	int32_t avg_zscore;

};

#ifdef __cplusplus
}
#endif

#endif
