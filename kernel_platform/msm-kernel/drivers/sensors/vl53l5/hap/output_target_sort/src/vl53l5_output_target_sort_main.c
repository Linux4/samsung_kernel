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

#include "vl53l5_output_target_sort_funcs.h"
#include "vl53l5_output_target_sort_main.h"

void vl53l5_ots_get_version(
	struct  common_grp__version_t  *pversion)
{

	pversion->version__major = VL53L5_OTS_DEF__VERSION__MAJOR;
	pversion->version__minor = VL53L5_OTS_DEF__VERSION__MINOR;
	pversion->version__build = VL53L5_OTS_DEF__VERSION__BUILD;
	pversion->version__revision = VL53L5_OTS_DEF__VERSION__REVISION;
}

void vl53l5_output_target_sort_main(
	struct   vl53l5_range_results_t             *presults,
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
	struct vl53l5_sharpener_target_data_t  *psharp_tgt_data,
#endif
	struct   vl53l5_ots_cfg_dev_t     *pcfg_dev,
	struct   vl53l5_ots_err_dev_t     *perr_dev)
{

	uint8_t   total_no_of_zones = 0;
	uint8_t   z  = 0;
	uint16_t  first_tgt_idx_for_zone = 0;

	uint8_t (*swap_func_ptr)(struct vl53l5_range_per_tgt_results_t *, uint16_t,
							 uint16_t) = vl53l5_ots_swap_reqd_by_range;

	struct   common_grp__status_t *perror   = &(perr_dev->ots_error_status);
	struct   common_grp__status_t *pwarning = &(perr_dev->ots_warning_status);

	total_no_of_zones = (uint8_t)(presults->common_data.rng__no_of_zones +
								  presults->common_data.rng__acc__no_of_zones);

	vl53l5_ots_clear_data(perr_dev);

	switch (pcfg_dev->ots_general_cfg.ots__mode) {
#ifdef VL53L5_MEDIAN_RANGE_MM_ON
	case VL53L5_OTS_MODE__CLOSEST_FIRST:
		swap_func_ptr = vl53l5_ots_swap_reqd_by_range;
		pwarning->status__type = VL53L5_OTS_WARNING_NONE;
		break;
#endif
#ifdef VL53L5_PEAK_RATE_KCPS_PER_SPAD_ON
	case VL53L5_OTS_MODE__STRONGEST_FIRST:
		swap_func_ptr = vl53l5_ots_swap_reqd_by_rate;
		pwarning->status__type = VL53L5_OTS_WARNING_NONE;
		break;
#endif
#ifdef VL53L5_TARGET_ZSCORE_ON
	case VL53L5_OTS_MODE__MOST_CONFIDENT_FIRST:
		swap_func_ptr = vl53l5_ots_swap_reqd_by_zscore;
		pwarning->status__type = VL53L5_OTS_WARNING_NONE;
		break;
#endif
	case VL53L5_OTS_MODE__DISABLED:
		pwarning->status__type = VL53L5_OTS_WARNING_DISABLED;
		break;
	default:
		perror->status__type = VL53L5_OTS_ERROR_INVALID_PARAMS;
		break;
	}

	for (z = 0 ; z < total_no_of_zones; ++z) {

		if (perror->status__type == VL53L5_OTS_ERROR_NONE &&
			pwarning->status__type == VL53L5_OTS_WARNING_NONE) {
			first_tgt_idx_for_zone = vl53l5_ots_first_tgt_idx_for_zone(
										 z,
										 presults->common_data.rng__max_targets_per_zone);

			if (presults->per_zone_results.rng__no_of_targets[z] >
				DCI_MAX_TARGET_PER_ZONE) {

				perror->status__type = VL53L5_OTS_ERROR_INVALID_PARAMS;
			} else {
				vl53l5_ots_sort_targets(
					&(presults->per_tgt_results),
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
					psharp_tgt_data,
#endif
					first_tgt_idx_for_zone,
					(uint16_t)presults->per_zone_results.rng__no_of_targets[z],
					swap_func_ptr);
			}
		}
	}
}
