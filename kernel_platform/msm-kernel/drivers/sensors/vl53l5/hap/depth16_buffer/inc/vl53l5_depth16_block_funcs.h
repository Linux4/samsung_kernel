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

#ifndef VL53L5_DEPTH16_BUFFER_FUNCS_H_
#define VL53L5_DEPTH16_BUFFER_FUNCS_H_

#include "vl53l5_types.h"
#include "vl53l5_results_config.h"
#include "common_datatype_structs.h"
#include "vl53l5_results_map.h"
#include "dci_patch_structs.h"
#include "dci_patch_op_map.h"
#include "vl53l5_d16_structs.h"
#include "vl53l5_d16_err_map.h"

void vl53l5_d16_bh_list_update(
	struct  vl53l5_range_common_data_t    *pdata,
	struct  vl53l5_d16__arrayed__cfg_t    *parrayed_cfg);

uint32_t vl53l5_d16_block_calc_size(
	uint32_t                                   bh_value);

void vl53l5_d16_block_meta_init(
	uint32_t                             bh_value,
	uint32_t                             bh_idx,
	struct   dci_grp__d16__blk__meta_t  *pblk_meta);

uint32_t vl53l5_d16_block_encode_glass_detect(
	struct  dci_grp__gd__op__status_t    *pdata);

void vl53l5_d16_block_decode_glass_detect(
	uint32_t                              encoded_word,
	struct  dci_grp__gd__op__status_t    *pdata);

uint32_t vl53l5_d16_block_division(
	uint32_t        value0,
	uint32_t        value1);

uint32_t vl53l5_d16_block_multiply(
	uint32_t        value0,
	uint32_t        value1);

uint32_t vl53l5_d16_block_get_data_idx(
	struct  vl53l5_range_common_data_t *pcommon,
	struct  dci_grp__d16__blk__meta_t  *pblk_meta);

uint32_t vl53l5_d16_block_encode_word(
	struct  vl53l5_range_results_t                *prng_dev,
	struct  dci_patch_op_dev_t          *pop_dev,
	struct  dci_grp__d16__blk__meta_t   *pblk_meta,
	struct  common_grp__status_t        *perr_status);

void vl53l5_d16_block_decode_word(
	uint32_t                             encoded_word,
	struct  dci_grp__d16__blk__meta_t   *pblk_meta,
	struct  vl53l5_range_results_t                *prng_dev,
	struct  dci_patch_op_dev_t          *pop_dev,
	struct  common_grp__status_t        *perr_status);

uint32_t vl53l5_d16_block_encode(
	struct  vl53l5_range_results_t                *prng_dev,
	struct  dci_patch_op_dev_t          *pop_dev,
	struct  dci_grp__d16__blk__meta_t   *pblk_meta,
	uint32_t                             packet_offset,
	uint32_t                             packet_size,
	uint32_t                            *ppacket,
	struct  common_grp__status_t        *perr_status);

uint32_t vl53l5_d16_block_decode(
	uint32_t                             packet_offset,
	uint32_t                             packet_size,
	uint32_t                            *ppacket,
	struct  dci_grp__d16__blk__meta_t   *pblk_meta,
	struct  vl53l5_range_results_t                *prng_dev,
	struct  dci_patch_op_dev_t          *pop_dev,
	struct  common_grp__status_t        *perr_status);

#endif
