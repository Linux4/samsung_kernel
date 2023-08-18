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

#ifndef __VL53L5_AFF_STRUCTS_H__
#define __VL53L5_AFF_STRUCTS_H__

#include "vl53l5_types.h"
#include "vl53l5_aff_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct vl53l5_aff__cfg_t {

	int16_t aff__affinity_param0;

	int16_t aff__affinity_param1;

	int16_t aff__non_matched_history_tweak_param;

	int16_t aff__missing_cur_zscore_param;

	int16_t aff__out_sort_param;

	uint16_t aff__zscore_missing_tgt;

	uint16_t aff__min_zscore_for_in;

	uint16_t aff__min_zscore_for_out;

	int16_t aff__discard_threshold;

	uint8_t aff__nb_of_past_instants;

	uint8_t aff__max_nb_of_targets_to_keep_per_zone;

	uint8_t aff__mode;

	uint8_t aff__discard_close_tgt;

	uint8_t aff__nb_zones_to_filter;

	uint8_t aff__affinity_mode;

	uint8_t aff__non_matched_history_tweak_mode;

	uint8_t aff__missing_cur_zscore_mode;

	uint8_t aff__out_sort_mode;

	uint8_t aff__spare0;
};

struct vl53l5_aff__arrayed_cfg_t {

	uint8_t aff__temporal_weights[
		VL53L5_AFF_DEF__MAX_NB_OF_TEMPORAL_WEIGHTS];

	uint8_t aff__zone_sel[VL53L5_AFF_DEF__MAX_ZONE_SEL];
};

struct vl53l5_aff__persistent_container_t {

	int32_t aff__persistent_array[VL53L5_AFF_DEF__PERSISTENT_MEM_SIZE];
};

struct vl53l5_aff__workspace_container_t {

	int32_t aff__workspace_array[VL53L5_AFF_DEF__WORKSPACE_SIZE];
};

#ifdef __cplusplus
}
#endif

#endif
