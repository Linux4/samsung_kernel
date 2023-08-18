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

#include "dci_luts.h"
#include "dci_ui_union_structs.h"
#include "dci_patch_union_structs.h"
#include "vl53l5_d16_defs.h"
#include "vl53l5_d16_luts.h"
#include "vl53l5_d16_size.h"
#include "vl53l5_depth16_block_funcs.h"
#include "vl53l5_depth16_packet_funcs.h"

void vl53l5_d16_packet_clear_data(
	int16_t                        stage_id,
	struct vl53l5_d16_err_dev_t   *perr_dev)
{
	struct common_grp__status_t    *perror   = &(perr_dev->d16_error_status);
	struct common_grp__status_t    *pwarning = &(perr_dev->d16_warning_status);

	perror->status__group = VL53L5_ALGO_DEPTH16_GROUP_ID;
	perror->status__stage_id = stage_id;
	perror->status__type = VL53L5_D16_ERROR_NONE;
	perror->status__debug_0 = 0U;
	perror->status__debug_1 = 0U;
	perror->status__debug_2 = 0U;

	pwarning->status__group = VL53L5_ALGO_DEPTH16_GROUP_ID;
	pwarning->status__stage_id = stage_id;
	pwarning->status__type = VL53L5_D16_WARNING_NONE;
	pwarning->status__debug_0 = 0U;
	pwarning->status__debug_1 = 0U;
	pwarning->status__debug_2 = 0U;
}

uint32_t vl53l5_d16_packet_decode_header(
	uint32_t                                encoded_header,
	struct  vl53l5_range_common_data_t     *pcommon)
{

	uint32_t no_of_zones = 0;
	uint32_t last_packet  = 0;

	union dci_union__d16__buf__packet_header_u packet_header = {0};

	packet_header.bytes = encoded_header;

	pcommon->rng__max_targets_per_zone =
		(uint8_t)packet_header.max_targets_per_zone;
	no_of_zones  = packet_header.array_width;
	no_of_zones *= packet_header.array_height;
	pcommon->rng__no_of_zones = (uint8_t)no_of_zones;

	last_packet = packet_header.last_packet;

	return last_packet;
}

uint32_t vl53l5_d16_packet_encode_header(
	struct  vl53l5_range_common_data_t     *pcommon,
	uint32_t                                last_packet)
{

	uint32_t grid_size = 0;

	union dci_union__d16__buf__packet_header_u packet_header = {0};

	packet_header.version = 1U;

	packet_header.max_targets_per_zone =
		0x0003U & (uint32_t)pcommon->rng__max_targets_per_zone;

	grid_size = 8;
	if (pcommon->rng__no_of_zones == 16)
		grid_size = 4;
	packet_header.array_width = 0x003F & grid_size;
	packet_header.array_height = 0x003F & grid_size;

	packet_header.last_packet = 0x0001U & last_packet;

	return packet_header.bytes;
}

void vl53l5_d16_packet_meta_init(
	uint32_t                                  packet_size,
	struct  vl53l5_d16__arrayed__cfg_t       *parrayed_cfg,
	struct  vl53l5_range_common_data_t       *pcommon,
	struct  dci_grp__d16__pkt__meta_t        *ppkt_meta)
{
	uint32_t i         = 0U;
	uint32_t word_count = 0U;
	uint32_t last_packet = 0U;

	i = 0U;
	while (parrayed_cfg->bh_list[i] > 0 &&
		   i < VL53L5_D16_DEF__MAX_BH_LIST_SIZE) {
		word_count +=
			vl53l5_d16_block_calc_size(
				parrayed_cfg->bh_list[i]);
		i++;
	}

	ppkt_meta->size = packet_size;
	if (packet_size == 0)
		ppkt_meta->size = word_count + 1;

	ppkt_meta->count =
		(word_count + (ppkt_meta->size - 2U)) / (ppkt_meta->size - 1U);
	ppkt_meta->idx = 0U;

	last_packet = (ppkt_meta->count == 1U) ? 1 : 0;

	ppkt_meta->header.bytes =
		vl53l5_d16_packet_encode_header(
			pcommon,
			last_packet);
}
