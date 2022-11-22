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

#include "vl53l5_platform_algo_limits.h"
#include "dci_luts.h"
#include "dci_ui_union_structs.h"
#include "dci_patch_luts.h"
#include "dci_patch_union_structs.h"
#include "vl53l5_d16_defs.h"
#include "vl53l5_d16_luts.h"
#include "vl53l5_d16_size.h"
#include "vl53l5_depth16_block_funcs.h"

void vl53l5_d16_bh_list_update(
	struct  vl53l5_range_common_data_t    *pdata,
	struct  vl53l5_d16__arrayed__cfg_t    *parrayed_cfg)
{

	uint32_t max_targets_per_zone =
		(uint32_t)pdata->rng__max_targets_per_zone;
	uint32_t no_of_zones =
		(uint32_t)pdata->rng__no_of_zones;
	uint32_t acc__no_of_zones =
		(uint32_t)pdata->rng__acc__no_of_zones;

	uint32_t bh_count = 0U;
	uint32_t b_size__p_rep = 0U;

	union  dci_union__block_header_u   bh = {0};

	bh_count = 0U;
	while (parrayed_cfg->bh_list[bh_count] > 0 &&
		   bh_count < VL53L5_D16_DEF__MAX_BH_LIST_SIZE) {
		bh.bytes = parrayed_cfg->bh_list[bh_count];

		switch (bh.b_idx__p_idx) {
		case DCI_D16_BH_PARM_ID__AMB_RATE_KCPS_PER_SPAD:
			b_size__p_rep = no_of_zones;
			break;
		case DCI_D16_BH_PARM_ID__PEAK_RATE_KCPS_PER_SPAD:
		case DCI_D16_BH_PARM_ID__DEPTH16:
		case DCI_D16_BH_PARM_ID__TARGET_STATUS:
			b_size__p_rep = no_of_zones * max_targets_per_zone;
			break;
		case DCI_D16_BH_PARM_ID__SZ__AMB_RATE_KCPS_PER_SPAD:
			b_size__p_rep = acc__no_of_zones;
			break;
		case DCI_D16_BH_PARM_ID__SZ__PEAK_RATE_KCPS_PER_SPAD:
		case DCI_D16_BH_PARM_ID__SZ__DEPTH16:
		case DCI_D16_BH_PARM_ID__SZ__TARGET_STATUS:
			b_size__p_rep = acc__no_of_zones * max_targets_per_zone;
			break;
		default:
			b_size__p_rep = bh.b_size__p_rep;
			break;
		}

		bh.b_size__p_rep = 0x0FFFU & b_size__p_rep;
		parrayed_cfg->bh_list[bh_count] = bh.bytes;
		bh_count++;
	}
}

uint32_t vl53l5_d16_block_calc_size(
	uint32_t                                   bh_value)
{
	uint32_t  block_sz_bytes = 0U;
	uint32_t  block_sz_words = 0U;

	union  dci_union__block_header_u   bh = {0};

	bh.bytes = bh_value;

	if (bh.p_type == 0U)
		block_sz_bytes = (uint32_t)bh.b_size__p_rep;
	else if (bh.p_type <= 8U)
		block_sz_bytes = (uint32_t)bh.p_type * (uint32_t)bh.b_size__p_rep;

	block_sz_words = ((block_sz_bytes + 3U) >> 2U);

	return block_sz_words;
}

void vl53l5_d16_block_meta_init(
	uint32_t                             bh_value,
	uint32_t                             bh_idx,
	struct   dci_grp__d16__blk__meta_t  *pblk_meta)
{
	pblk_meta->header.bytes = bh_value;
	pblk_meta->header_idx = bh_idx;
	pblk_meta->data_idx = 0;
	pblk_meta->size = vl53l5_d16_block_calc_size(bh_value);
}

uint32_t vl53l5_d16_block_division(
	uint32_t        value0,
	uint32_t        value1)
{

	uint32_t op_value = value0;

	if (value1 > 0U) {
		if ((VL53L5_UINT32_MAX - value0) > (value1 >> 1U))
			op_value += (value1 >> 1U);
		else
			op_value = VL53L5_UINT32_MAX;
		op_value /= value1;
	}

	return op_value;
}

uint32_t vl53l5_d16_block_multiply(
	uint32_t        value0,
	uint32_t        value1)
{

	uint32_t op_value = 0;

	if (value0 > 0U && value1 > 0U) {
		if ((VL53L5_UINT32_MAX / value0) > value1)
			op_value = value0 * value1;
		else
			op_value = VL53L5_UINT32_MAX;
	}

	return op_value;
}

uint32_t vl53l5_d16_block_encode_glass_detect(
	struct  dci_grp__gd__op__status_t    *pdata)
{

	union dci_union__d16__buf__glass_detect_u glass_detect = {0};
	uint32_t  tmp = 0U;

	tmp =
		vl53l5_d16_block_division(
			pdata->gd__rate_ratio,
			256U);

	glass_detect.bytes = 0U;
	glass_detect.gd__rate_ratio =
		0xFFFFU & (uint16_t)tmp;
	glass_detect.gd__confidence =
		0x00FFU & (uint32_t)pdata->gd__confidence;

	glass_detect.gd__plane_detected =
		0x0001U & (uint32_t)pdata->gd__plane_detected;
	glass_detect.gd__ratio_detected =
		0x0001U & (uint32_t)pdata->gd__ratio_detected;
	glass_detect.gd__glass_detected =
		0x0001U & (uint32_t)pdata->gd__glass_detected;
	glass_detect.gd__tgt_behind_glass_detected =
		0x0001U & (uint32_t)pdata->gd__tgt_behind_glass_detected;
	glass_detect.gd__mirror_detected =
		0x0001U & (uint32_t)pdata->gd__mirror_detected;

	glass_detect.reserved_0 =
		0x0007U & (uint32_t)pdata->gd__op_spare_0;

	return (uint32_t)glass_detect.bytes;
}

void vl53l5_d16_block_decode_glass_detect(
	uint32_t                              encoded_word,
	struct  dci_grp__gd__op__status_t    *pdata)
{

	union dci_union__d16__buf__glass_detect_u glass_detect = {0};
	uint32_t  tmp = 0U;

	glass_detect.bytes = encoded_word;
	tmp = (uint32_t)glass_detect.gd__rate_ratio;
	pdata->gd__confidence =
		(uint8_t)glass_detect.gd__confidence;

	pdata->gd__plane_detected =
		(uint8_t)glass_detect.gd__plane_detected;
	pdata->gd__ratio_detected =
		(uint8_t)glass_detect.gd__ratio_detected;
	pdata->gd__glass_detected =
		(uint8_t)glass_detect.gd__glass_detected;
	pdata->gd__tgt_behind_glass_detected =
		(uint8_t)glass_detect.gd__tgt_behind_glass_detected;
	pdata->gd__mirror_detected =
		(uint8_t)glass_detect.gd__mirror_detected;

	pdata->gd__op_spare_0 =
		(uint8_t)glass_detect.reserved_0;

	pdata->gd__rate_ratio =
		vl53l5_d16_block_multiply(
			tmp,
			256U);
}

uint32_t vl53l5_d16_block_get_data_idx(
	struct  vl53l5_range_common_data_t *pcommon,
	struct  dci_grp__d16__blk__meta_t  *pblk_meta)
{
	uint32_t  data_idx = 0U;
	uint32_t rng__no_of_zones =
		(uint32_t)pcommon->rng__no_of_zones;
	uint32_t rng__max_targets_per_zone =
		(uint32_t)pcommon->rng__max_targets_per_zone;

	switch (pblk_meta->header.b_idx__p_idx) {
	case DCI_D16_BH_PARM_ID__SZ__AMB_RATE_KCPS_PER_SPAD:
		data_idx =
			pblk_meta->data_idx + rng__no_of_zones;
		break;
	case DCI_D16_BH_PARM_ID__SZ__DEPTH16:
		data_idx =
			pblk_meta->data_idx +
			((rng__no_of_zones * rng__max_targets_per_zone) >> 1U);
		break;
	case DCI_D16_BH_PARM_ID__SZ__PEAK_RATE_KCPS_PER_SPAD:
		data_idx =
			pblk_meta->data_idx +
			(rng__no_of_zones * rng__max_targets_per_zone);
		break;
	default:
		data_idx = pblk_meta->data_idx;
	}

	return data_idx;
}

uint32_t vl53l5_d16_block_encode_word(
	struct  vl53l5_range_results_t               *prng_dev,
	struct  dci_patch_op_dev_t         *pop_dev,
	struct  dci_grp__d16__blk__meta_t  *pblk_meta,
	struct  common_grp__status_t       *perr_status)
{

	uint32_t encoded_word = 0U;
	uint32_t data_idx = 0;

	perr_status->status__stage_id =
		VL53L5_D16_STAGE_ID__BLOCK_ENCODE_WORD;
	perr_status->status__type =
		VL53L5_D16_ERROR_NONE;

	data_idx =
		vl53l5_d16_block_get_data_idx(
			&(prng_dev->common_data),
			pblk_meta);

	switch (pblk_meta->header.b_idx__p_idx) {
	case DCI_D16_BH_PARM_ID__DEPTH16:
	case DCI_D16_BH_PARM_ID__SZ__DEPTH16:
		encoded_word =
			(uint32_t)pop_dev->d16_per_target_data.depth16[(data_idx * 2)];
		encoded_word |=
			((uint32_t)pop_dev->d16_per_target_data.depth16[(data_idx * 2) + 1] << 16U);
		break;
	case DCI_D16_BH_PARM_ID__AMB_RATE_KCPS_PER_SPAD:
	case DCI_D16_BH_PARM_ID__SZ__AMB_RATE_KCPS_PER_SPAD:
		encoded_word =
			vl53l5_d16_block_division(
				prng_dev->per_zone_results.amb_rate_kcps_per_spad[data_idx],
				2048U);
		break;
	case DCI_D16_BH_PARM_ID__GLASS_DETECT:
		encoded_word =
			vl53l5_d16_block_encode_glass_detect(
				&(pop_dev->gd_op_status));
		break;
	case DCI_D16_BH_PARM_ID__PEAK_RATE_KCPS_PER_SPAD:
	case DCI_D16_BH_PARM_ID__SZ__PEAK_RATE_KCPS_PER_SPAD:
		encoded_word =
			vl53l5_d16_block_division(
				prng_dev->per_tgt_results.peak_rate_kcps_per_spad[data_idx],
				2048U);
		break;
	case DCI_D16_BH_PARM_ID__EMPTY:
	case DCI_D16_BH_PARM_ID__END_OF_DATA:
		encoded_word = 0U;
		break;
	default:
		perr_status->status__type = VL53L5_D16_ERROR_INVALID_PARM_ID;
		encoded_word = 0U;
		break;
	}

	return encoded_word;
}

void vl53l5_d16_block_decode_word(
	uint32_t                            encoded_word,
	struct  dci_grp__d16__blk__meta_t  *pblk_meta,
	struct  vl53l5_range_results_t               *prng_dev,
	struct  dci_patch_op_dev_t         *pop_dev,
	struct  common_grp__status_t       *perr_status)
{

	uint32_t data_idx = 0;

	perr_status->status__stage_id =
		VL53L5_D16_STAGE_ID__BLOCK_DECODE_WORD;
	perr_status->status__type =
		VL53L5_D16_ERROR_NONE;

	data_idx =
		vl53l5_d16_block_get_data_idx(
			&(prng_dev->common_data),
			pblk_meta);

	switch (pblk_meta->header.b_idx__p_idx) {
	case DCI_D16_BH_PARM_ID__DEPTH16:
	case DCI_D16_BH_PARM_ID__SZ__DEPTH16:
		pop_dev->d16_per_target_data.depth16[(data_idx * 2)] =
			(uint16_t)(encoded_word & 0x0000FFFF);
		pop_dev->d16_per_target_data.depth16[(data_idx * 2) + 1] =
			(uint16_t)(encoded_word >> 16U);
		break;
	case DCI_D16_BH_PARM_ID__AMB_RATE_KCPS_PER_SPAD:
	case DCI_D16_BH_PARM_ID__SZ__AMB_RATE_KCPS_PER_SPAD:
		prng_dev->per_zone_results.amb_rate_kcps_per_spad[data_idx] =
			vl53l5_d16_block_multiply(
				encoded_word,
				2048U);
		break;
	case DCI_D16_BH_PARM_ID__GLASS_DETECT:
		vl53l5_d16_block_decode_glass_detect(
			encoded_word,
			&(pop_dev->gd_op_status));
		break;
	case DCI_D16_BH_PARM_ID__PEAK_RATE_KCPS_PER_SPAD:
	case DCI_D16_BH_PARM_ID__SZ__PEAK_RATE_KCPS_PER_SPAD:
		prng_dev->per_tgt_results.peak_rate_kcps_per_spad[data_idx] =
			vl53l5_d16_block_multiply(
				encoded_word,
				2048U);
		break;
	case DCI_D16_BH_PARM_ID__EMPTY:
	case DCI_D16_BH_PARM_ID__END_OF_DATA:

		break;
	default:
		perr_status->status__type = VL53L5_D16_ERROR_INVALID_PARM_ID;
		break;
	}
}

uint32_t vl53l5_d16_block_encode(
	struct  vl53l5_range_results_t               *prng_dev,
	struct  dci_patch_op_dev_t         *pop_dev,
	struct  dci_grp__d16__blk__meta_t  *pblk_meta,
	uint32_t                            packet_offset,
	uint32_t                            packet_size,
	uint32_t                           *ppacket,
	struct  common_grp__status_t       *perr_status)
{

	uint32_t   word_idx = packet_offset;
	uint32_t   data_idx = pblk_meta->data_idx;
	uint32_t  *ppkt     = ppacket + packet_offset;

	perr_status->status__stage_id =
		VL53L5_D16_STAGE_ID__BLOCK_ENCODE;
	perr_status->status__type =
		VL53L5_D16_ERROR_NONE;

	while ((data_idx < pblk_meta->size) &&
		   (word_idx < packet_size) &&
		   (perr_status->status__type == VL53L5_D16_ERROR_NONE)) {
		*ppkt++ =
			vl53l5_d16_block_encode_word(
				prng_dev,
				pop_dev,
				pblk_meta,
				perr_status);

		word_idx++;
		data_idx++;
		pblk_meta->data_idx = data_idx;
	}

	return word_idx;
}

uint32_t vl53l5_d16_block_decode(
	uint32_t                            packet_offset,
	uint32_t                            packet_size,
	uint32_t                           *ppacket,
	struct  dci_grp__d16__blk__meta_t  *pblk_meta,
	struct  vl53l5_range_results_t               *prng_dev,
	struct  dci_patch_op_dev_t         *pop_dev,
	struct  common_grp__status_t       *perr_status)
{

	uint32_t   word_idx         = packet_offset;
	uint32_t   data_idx         = pblk_meta->data_idx;
	uint32_t  *ppkt             = ppacket + packet_offset;

	perr_status->status__stage_id =
		VL53L5_D16_STAGE_ID__BLOCK_DECODE;
	perr_status->status__type =
		VL53L5_D16_ERROR_NONE;

	while ((data_idx < pblk_meta->size) &&
		   (word_idx < packet_size) &&
		   (perr_status->status__type == VL53L5_D16_ERROR_NONE)) {
		vl53l5_d16_block_decode_word(
			*ppkt++,
			pblk_meta,
			prng_dev,
			pop_dev,
			perr_status);

		word_idx++;
		data_idx++;
		pblk_meta->data_idx = data_idx;
	}

	return word_idx;
}
