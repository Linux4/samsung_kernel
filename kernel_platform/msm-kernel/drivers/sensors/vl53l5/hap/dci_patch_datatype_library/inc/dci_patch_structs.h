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

#ifndef __DCI_PATCH_STRUCTS_H__
#define __DCI_PATCH_STRUCTS_H__

#include "vl53l5_results_config.h"
#include "dci_defs.h"
#include "dci_luts.h"
#include "dci_ui_union_structs.h"
#include "dci_patch_defs.h"
#include "dci_patch_luts.h"
#include "dci_patch_structs.h"
#include "dci_patch_union_structs.h"
#include "vl53l5_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dci_grp__gd__op__data_t {

	uint32_t gd__max__valid_rate_kcps_spad;

	uint32_t gd__min__valid_rate_kcps_spad;

	uint32_t gd__max__plane_rate_kcps_spad;

	uint32_t gd__min__plane_rate_kcps_spad;

	int16_t gd__glass_display_case_dist_mm;

	int16_t gd__glass_merged_dist_mm;

	int16_t gd__mirror_dist_mm;

	int16_t gd__target_behind_mirror_dist_mm;

	int16_t gd__target_behind_mirror_min_dist_mm;

	int16_t gd__target_behind_mirror_max_dist_mm;

	int16_t gd__plane_xy_angle;

	int16_t gd__plane_yz_angle;

	uint8_t gd__max_valid_rate_zone;

	uint8_t gd__max_valid_rate_zone_clipped;

	uint8_t gd__valid_zone_count;

	uint8_t gd__merged_zone_count;

	uint8_t gd__possible_merged_zone_count;

	uint8_t gd__possible_tgt2_behind_mirror_zone_count;

	uint8_t gd__blurred_zone_count;

	uint8_t gd__plane_zone_count;

	uint8_t gd__points_on_plane_pc;

	uint8_t gd__tgt2_behind_mirror_plane_zone_count;

	uint8_t gd__tgt2_behind_mirror_points_on_plane_pc;

	uint8_t gd__op_data_spare_0;
};

struct dci_grp__gd__op__status_t {

	uint32_t gd__rate_ratio;

	uint8_t gd__confidence;

	uint8_t gd__plane_detected;

	uint8_t gd__ratio_detected;

	uint8_t gd__glass_detected;

	uint8_t gd__tgt_behind_glass_detected;

	uint8_t gd__mirror_detected;

	uint8_t gd__op_spare_0;

	uint8_t gd__op_spare_1;
};

struct dci_grp__sz__common_data_t {

	uint8_t sz__no_of_zones;

	uint8_t sz__max_targets_per_zone;

	uint8_t sz__common__spare_0;

	uint8_t sz__common__spare_1;
};

struct dci_grp__sz__zone_data_t {

	uint32_t amb_rate_kcps_per_spad[DCI_SZ_MAX_ZONES];

	uint32_t rng__effective_spad_count[DCI_SZ_MAX_ZONES];

	uint16_t amb_dmax_mm[DCI_SZ_MAX_ZONES];

	uint8_t rng__no_of_targets[DCI_SZ_MAX_ZONES];

	uint8_t rng__zone_id[DCI_SZ_MAX_ZONES];
};

struct dci_grp__sz__target_data_t {

	uint32_t peak_rate_kcps_per_spad[DCI_SZ_MAX_TARGETS];

	uint16_t range_sigma_mm[DCI_SZ_MAX_TARGETS];

	int16_t median_range_mm[DCI_SZ_MAX_TARGETS];

	uint8_t target_status[DCI_SZ_MAX_TARGETS];
};

struct dci_grp__depth16_value_t {

	union dci_union__depth16_value_u depth16;

	uint16_t d16_confidence_pc;
};

struct dci_grp__d16__per_target_data_t {

	uint16_t depth16[VL53L5_MAX_TARGETS];
};

struct dci_grp__d16__ref_target_data_t {

	uint16_t depth16;

	uint16_t d16__ref_target__pad_0;
};

struct dci_grp__d16__buf__packet_header_t {

	union dci_union__d16__buf__packet_header_u packet_header;
};

struct dci_grp__d16__buf__glass_detect_t {

	union dci_union__d16__buf__glass_detect_u glass_detect;
};

struct dci_grp__d16__blk__meta_t {

	union dci_union__block_header_u header;

	uint32_t size;

	uint32_t data_idx;

	uint32_t header_idx;
};

struct dci_grp__d16__buf__meta_t {

	uint32_t max_size;

	uint32_t size;
};

struct dci_grp__d16__pkt__meta_t {

	uint32_t size;

	union dci_union__d16__buf__packet_header_u header;

	uint32_t count;

	uint32_t idx;
};

struct dci_grp__d16__buf__format_t {

	uint32_t bh_list[DCI_D16_MAX_BH_LIST_SIZE];
};

struct dci_grp__d16__buf__data_t {

	uint32_t data[DCI_D16_MAX_BUFFER_SIZE];
};

#ifdef __cplusplus
}
#endif

#endif
