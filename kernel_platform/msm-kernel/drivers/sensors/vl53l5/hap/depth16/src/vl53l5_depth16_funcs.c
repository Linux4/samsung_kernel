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

#include "vl53l5_platform_algo_macros.h"
#include "vl53l5_platform_algo_limits.h"
#include "vl53l5_results_build_config.h"
#include "dci_defs.h"
#include "dci_luts.h"
#include "dci_patch_union_structs.h"
#include "vl53l5_d16_defs.h"
#include "vl53l5_d16_luts.h"
#include "vl53l5_depth16_funcs.h"

void vl53l5_d16_clear_data(
	struct dci_patch_op_dev_t           *pop_dev,
	struct vl53l5_d16_err_dev_t         *perr_dev)
{

	struct common_grp__status_t    *perror   = &(perr_dev->d16_error_status);
	struct common_grp__status_t    *pwarning = &(perr_dev->d16_warning_status);

	if (pop_dev != NULL) {
		memset(
			&(pop_dev->d16_per_target_data),
			0,
			sizeof(struct dci_grp__d16__per_target_data_t));
	}

	perror->status__group = VL53L5_ALGO_DEPTH16_GROUP_ID;
	perror->status__stage_id = VL53L5_D16_STAGE_ID__MAIN;
	perror->status__type = VL53L5_D16_ERROR_NONE;
	perror->status__debug_0 = 0U;
	perror->status__debug_1 = 0U;
	perror->status__debug_2 = 0U;

	pwarning->status__group = VL53L5_ALGO_DEPTH16_GROUP_ID;
	pwarning->status__stage_id = VL53L5_D16_STAGE_ID__MAIN;
	pwarning->status__type = VL53L5_D16_WARNING_NONE;
	pwarning->status__debug_0 = 0U;
	pwarning->status__debug_1 = 0U;
	pwarning->status__debug_2 = 0U;
}

uint32_t vl53l5_d16_gen1_calc_system_dmax(
	uint16_t       wrap_dmax_mm,
	uint16_t       ambient_dmax_mm)
{

	uint32_t  system_dmax_mm = (uint32_t)wrap_dmax_mm;

	if (ambient_dmax_mm < wrap_dmax_mm)
		system_dmax_mm = (uint32_t)ambient_dmax_mm;

	if (system_dmax_mm > 0x1FFFU)
		system_dmax_mm = 0x1FFFU;

	return system_dmax_mm;
}

uint32_t vl53l5_d16_gen1_calc_accuracy(
	uint16_t       range_sigma_mm,
	int16_t        median_range_mm)
{

	uint32_t  accuracy = 0U;

	if (median_range_mm > 0) {

		accuracy = 1000U * (uint32_t)range_sigma_mm;
		accuracy += ((uint32_t)median_range_mm / 2U);
		accuracy /= (uint32_t)median_range_mm;

		accuracy += (1U << 4U);
		accuracy >>= 5U;
	}

	return accuracy;
}

uint32_t vl53l5_d16_gen1_calc_rate(
	uint32_t        rate_kcps_per_spad)
{

	uint32_t rate = 0U;

	rate = rate_kcps_per_spad;
	rate += 1024U;
	rate /= 2048U;

	return rate;
}

uint32_t vl53l5_d16_gen1_calc_range(
	int16_t        median_range_mm)
{

	uint32_t range = 0U;

	if (median_range_mm > 0) {

		range = (uint32_t)median_range_mm;
		range += 2U;
		range /= 4U;

		if (range > 0x00001FFFU)
			range = 0x00001FFFU;
	}

	return range;
}

uint32_t vl53l5_d16_gen1_calc_confidence(
	uint16_t       range_sigma_mm,
	int16_t        median_range_mm,
	uint8_t        target_status)
{

	uint32_t accuracy = 0U;

	uint32_t confidence = 3U;

	if (median_range_mm > 0) {
		accuracy =
			vl53l5_d16_gen1_calc_accuracy(
				range_sigma_mm,
				median_range_mm);

		if (target_status == DCI_TARGET_STATUS__RANGECOMPLETE) {
			if (accuracy < 15U)
				confidence = 0U;
			else if (accuracy < 30U)
				confidence = 7U;
			else if (accuracy < 60U)
				confidence = 6U;
			else if (accuracy < 120U)
				confidence = 5U;
			else if (accuracy < 240U)
				confidence = 4U;
			else
				confidence = 3U;
		} else {
			if (accuracy < 15U)
				confidence = 5U;
			else if (accuracy < 30U)
				confidence = 4U;
			else
				confidence = 3U;
		}
	}

	return confidence;
}

uint16_t vl53l5_d16_gen1_calc(
	int32_t        target_idx,
	uint16_t       range_sigma_mm,
	int16_t        median_range_mm,
	uint8_t        target_status,
	uint16_t      *pdp16_range,
	uint16_t      *pdp16_confidence,
	uint8_t       *pdp16_target_status)
{

	uint32_t  dp16_confidence    = 1U;
	uint32_t  dp16_range         = 0U;
	uint8_t   dp16_target_status = DCI_TARGET_STATUS__NOUPDATE;

	union dci_union__depth16_value_u  dp16_value = {0};

	switch (target_status) {
	case DCI_TARGET_STATUS__TARGETDUETOBLUR:
	case DCI_TARGET_STATUS__TARGETDUETOBLUR_NO_WRAP_CHECK:
	case DCI_TARGET_STATUS__TARGETDUETOBLUR_MERGED_PULSE:
		if (target_idx == 0)
			dp16_confidence = 2U;
		else
			dp16_confidence = 1U;
		dp16_range =
			vl53l5_d16_gen1_calc_range(
				median_range_mm);
		dp16_target_status = target_status;
		break;

	case DCI_TARGET_STATUS__RANGECOMPLETE:
	case DCI_TARGET_STATUS__RANGECOMPLETE_NO_WRAP_CHECK:
	case DCI_TARGET_STATUS__RANGECOMPLETE_MERGED_PULSE:
		dp16_confidence =
			vl53l5_d16_gen1_calc_confidence(
				range_sigma_mm,
				median_range_mm,
				target_status);
		dp16_range =
			vl53l5_d16_gen1_calc_range(
				median_range_mm);
		dp16_target_status = target_status;
		break;

	default:
		dp16_confidence = 1U;
		dp16_range = 0U;
		dp16_target_status = DCI_TARGET_STATUS__NOUPDATE;
		break;
	}

	*pdp16_range = (uint16_t)dp16_range;
	*pdp16_confidence = (uint16_t)dp16_confidence;
	*pdp16_target_status = dp16_target_status;

	dp16_value.confidence = 0x0007U & (uint16_t)dp16_confidence;
	dp16_value.range_mm = 0x1FFFU & (uint16_t)dp16_range;

	return dp16_value.bytes;
}
