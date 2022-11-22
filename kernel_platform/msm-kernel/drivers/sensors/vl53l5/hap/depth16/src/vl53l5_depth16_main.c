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
#include "vl53l5_d16_version_defs.h"
#include "vl53l5_d16_defs.h"
#include "vl53l5_d16_luts.h"
#include "vl53l5_depth16_funcs.h"
#include "vl53l5_depth16_main.h"

void vl53l5_d16_get_version(
	struct  common_grp__version_t  *pversion)
{

	pversion->version__major = VL53L5_D16_DEF__VERSION__MAJOR;
	pversion->version__minor = VL53L5_D16_DEF__VERSION__MINOR;
	pversion->version__build = VL53L5_D16_DEF__VERSION__BUILD;
	pversion->version__revision = VL53L5_D16_DEF__VERSION__REVISION;
}

void vl53l5_depth16_main(
	struct   vl53l5_range_results_t           *presults,
	struct   vl53l5_d16_cfg_dev_t   *pcfg_dev,
	struct   dci_patch_op_dev_t     *pop_dev,
	struct   vl53l5_d16_err_dev_t   *perr_dev)
{

	int32_t   z  = 0;
	int32_t   t = 0;
	int32_t   i = 0;

	int32_t  no_of_zones =
		(int32_t)presults->common_data.rng__no_of_zones;
	int32_t  acc__no_of_zones =
		(int32_t)presults->common_data.rng__acc__no_of_zones;
	int32_t  max_targets_per_zone =
		(int32_t)presults->common_data.rng__max_targets_per_zone;

	int32_t  no_of_targets = 0;

	uint32_t system_dmax_mm = 0U;
	uint16_t d16_range = 0U;
	uint16_t d16_confidence = 0U;
	uint8_t  d16_target_status = 0U;

	struct   common_grp__status_t *perror = &(perr_dev->d16_error_status);
	struct   common_grp__status_t *pwarning = &(perr_dev->d16_warning_status);

	vl53l5_d16_clear_data(
		pop_dev,
		perr_dev);

	switch (pcfg_dev->d16_general_cfg.d16__cfg__mode) {
	case VL53L5_D16_MODE__DISABLED:
		pwarning->status__type = VL53L5_D16_WARNING_DISABLED;
		break;
	case VL53L5_D16_MODE__ENABLED:
		pwarning->status__type = VL53L5_D16_WARNING_NONE;
		break;
	default:
		perror->status__type = VL53L5_D16_ERROR_INVALID_PARAMS;
		break;
	}

	for (z = 0;
		 (z < (no_of_zones + acc__no_of_zones) &&
		  perror->status__type == VL53L5_D16_ERROR_NONE &&
		  pwarning->status__type != VL53L5_D16_WARNING_DISABLED);
		 z++) {

		system_dmax_mm =
			vl53l5_d16_gen1_calc_system_dmax(
				presults->per_zone_results.amb_dmax_mm[z],
				presults->common_data.wrap_dmax_mm);

		for (t = 0 ; t < max_targets_per_zone ; t++) {
			i = (z * max_targets_per_zone) + t;
			pop_dev->d16_per_target_data.depth16[i] =
				(uint16_t)(1U << 13U);
		}

		no_of_targets =
			(int32_t)presults->per_zone_results.rng__no_of_targets[z];

		for (t = 0 ; t < no_of_targets; t++) {
			i = (z * max_targets_per_zone) + t;

			pop_dev->d16_per_target_data.depth16[i] =
				vl53l5_d16_gen1_calc(
					t,
					presults->per_tgt_results.range_sigma_mm[i],
					presults->per_tgt_results.median_range_mm[i],
					presults->per_tgt_results.target_status[i],
					&d16_range,
					&d16_confidence,
					&d16_target_status);

		}

		if (no_of_targets == 0) {
			i = (z * max_targets_per_zone);
			pop_dev->d16_per_target_data.depth16[i] =
				(uint16_t)(system_dmax_mm + (2U << 13U));
		}

		for (t = 0 ; t < max_targets_per_zone ; t++) {
			i = (z * max_targets_per_zone) + t;
			d16_confidence =
				(pop_dev->d16_per_target_data.depth16[i] >> 13U);

			if (d16_confidence == 1U || t >= no_of_targets)
				presults->per_tgt_results.peak_rate_kcps_per_spad[i] = 0U;
		}
	}
}
