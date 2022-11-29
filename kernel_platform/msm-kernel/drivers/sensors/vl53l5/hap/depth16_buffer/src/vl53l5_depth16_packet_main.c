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

#include "vl53l5_depth16_packet_main.h"
#include "dci_structs.h"
#include "dci_patch_union_structs.h"
#include "vl53l5_d16_defs.h"
#include "vl53l5_d16_luts.h"
#include "vl53l5_depth16_block_funcs.h"
#include "vl53l5_depth16_packet_funcs.h"

uint32_t vl53l5_depth16_packet_decode_init(
	uint32_t                                 *ppacket,
	struct  vl53l5_d16_cfg_dev_t             *pcfg_dev,
	struct  vl53l5_range_common_data_t       *pcommon,
	struct  dci_grp__d16__pkt__meta_t        *ppkt_meta,
	struct  dci_grp__d16__blk__meta_t        *pblk_meta,
	struct  vl53l5_d16_err_dev_t             *perr_dev)
{

	uint32_t last_packet  = 0;

	vl53l5_d16_packet_clear_data(
		VL53L5_D16_STAGE_ID__PACKET_DECODE_INIT,
		perr_dev);

	last_packet =
		vl53l5_d16_packet_decode_header(
			*ppacket,
			pcommon);

	vl53l5_d16_bh_list_update(
		pcommon,
		&(pcfg_dev->d16_arrayed_cfg));

	vl53l5_d16_packet_meta_init(
		(uint32_t)pcfg_dev->d16_general_cfg.d16__packet_size,
		&(pcfg_dev->d16_arrayed_cfg),
		pcommon,
		ppkt_meta);

	vl53l5_d16_block_meta_init(
		pcfg_dev->d16_arrayed_cfg.bh_list[0],
		0U,
		pblk_meta);

	return last_packet;
}

uint32_t vl53l5_depth16_packet_encode_init(
	struct  vl53l5_d16_cfg_dev_t             *pcfg_dev,
	struct  vl53l5_range_common_data_t       *pcommon,
	struct  dci_grp__d16__pkt__meta_t        *ppkt_meta,
	struct  dci_grp__d16__blk__meta_t        *pblk_meta,
	struct  vl53l5_d16_err_dev_t             *perr_dev)
{

	struct  common_grp__status_t *perror = &(perr_dev->d16_error_status);

	vl53l5_d16_packet_clear_data(
		VL53L5_D16_STAGE_ID__PACKET_ENCODE_INIT,
		perr_dev);

	if (pcommon->rng__max_targets_per_zone > VL53L5_D16_DEF__MAX_TARGETS_PER_ZONE) {
		perror->status__type =
			VL53L5_D16_ERROR_INVALID_MAX_TARGETS_PER_ZONE;
	}

	if (perror->status__type == VL53L5_D16_ERROR_NONE) {

		vl53l5_d16_bh_list_update(
			pcommon,
			&(pcfg_dev->d16_arrayed_cfg));

		vl53l5_d16_packet_meta_init(
			(uint32_t)pcfg_dev->d16_general_cfg.d16__packet_size,
			&(pcfg_dev->d16_arrayed_cfg),
			pcommon,
			ppkt_meta);

		vl53l5_d16_block_meta_init(
			pcfg_dev->d16_arrayed_cfg.bh_list[0],
			0U,
			pblk_meta);
	}

	return (uint32_t)ppkt_meta->header.last_packet;
}

uint32_t vl53l5_depth16_packet_decode_main(
	uint32_t                                 *ppacket,
	struct  vl53l5_d16_cfg_dev_t             *pcfg_dev,
	struct  dci_grp__d16__pkt__meta_t        *ppkt_meta,
	struct  dci_grp__d16__blk__meta_t        *pblk_meta,
	struct  vl53l5_range_results_t                     *prng_dev,
	struct  dci_patch_op_dev_t               *pop_dev,
	struct  vl53l5_d16_err_dev_t             *perr_dev)
{

	uint32_t  word_idx        = 0U;
	uint32_t  z               = 0U;

	struct   common_grp__status_t *perr_status =
		&(perr_dev->d16_error_status);
	struct   vl53l5_range_common_data_t *pcommon =
		&(prng_dev->common_data);
	uint32_t no_of_zones = 0U;

	vl53l5_d16_packet_clear_data(
		VL53L5_D16_STAGE_ID__PACKET_DECODE,
		perr_dev);

	ppkt_meta->header.bytes = *ppacket;

	word_idx = 1U;
	while ((pcfg_dev->d16_arrayed_cfg.bh_list[pblk_meta->header_idx] > 0U) &&
		   (perr_status->status__type == VL53L5_D16_ERROR_NONE) &&
		   (word_idx < ppkt_meta->size)) {

		word_idx =
			vl53l5_d16_block_decode(
				word_idx,
				ppkt_meta->size,
				ppacket,
				pblk_meta,
				prng_dev,
				pop_dev,
				perr_status);

		if (pblk_meta->data_idx >= pblk_meta->size) {
			vl53l5_d16_block_meta_init(
				pcfg_dev->d16_arrayed_cfg.bh_list[(pblk_meta->header_idx) + 1],
				(pblk_meta->header_idx) + 1,
				pblk_meta);
		}
	}

#ifdef VL53L5_ZONE_ID_ON
	if (perr_status->status__type == VL53L5_D16_ERROR_NONE) {
		no_of_zones =
			(uint32_t)pcommon->rng__no_of_zones +
			(uint32_t)pcommon->rng__acc__no_of_zones;

		for (z = 0; z < no_of_zones; z++) {
			if (z < (uint32_t)pcommon->rng__no_of_zones)
				prng_dev->per_zone_results.rng__zone_id[z] = (uint8_t)z;
			else {
				prng_dev->per_zone_results.rng__zone_id[z] =
					(uint8_t)(z  + (uint32_t)pcommon->rng__acc__zone_id - (uint32_t)
							  pcommon->rng__no_of_zones);
			}
		}
	}
#endif

	(ppkt_meta->idx)++;

	return (uint32_t)ppkt_meta->header.last_packet;
}

uint32_t vl53l5_depth16_packet_encode_main(
	struct  vl53l5_range_results_t                     *prng_dev,
	struct  dci_patch_op_dev_t               *pop_dev,
	struct  vl53l5_d16_cfg_dev_t             *pcfg_dev,
	struct  dci_grp__d16__pkt__meta_t        *ppkt_meta,
	struct  dci_grp__d16__blk__meta_t        *pblk_meta,
	uint32_t                                 *ppacket,
	struct  vl53l5_d16_err_dev_t             *perr_dev)
{

	uint32_t  word_idx      = 0U;
	struct   common_grp__status_t *perr_status =
		&(perr_dev->d16_error_status);

	vl53l5_d16_packet_clear_data(
		VL53L5_D16_STAGE_ID__PACKET_ENCODE,
		perr_dev);

	ppkt_meta->header.last_packet = 0U;
	if (ppkt_meta->idx >= (ppkt_meta->count - 1U))
		ppkt_meta->header.last_packet  = 1U;

	if (perr_status->status__type == VL53L5_D16_ERROR_NONE)
		*ppacket = ppkt_meta->header.bytes;

	word_idx = 1U;
	while ((pcfg_dev->d16_arrayed_cfg.bh_list[pblk_meta->header_idx] > 0U) &&
		   (perr_status->status__type == VL53L5_D16_ERROR_NONE) &&
		   (word_idx < ppkt_meta->size)) {

		word_idx =
			vl53l5_d16_block_encode(
				prng_dev,
				pop_dev,
				pblk_meta,
				word_idx,
				ppkt_meta->size,
				ppacket,
				perr_status);

		if (pblk_meta->data_idx >= pblk_meta->size) {
			vl53l5_d16_block_meta_init(
				pcfg_dev->d16_arrayed_cfg.bh_list[(pblk_meta->header_idx) + 1],
				(pblk_meta->header_idx) + 1,
				pblk_meta);
		}
	}

	(ppkt_meta->idx)++;

	return (uint32_t)ppkt_meta->header.last_packet;
}
