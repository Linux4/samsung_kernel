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

#include "vl53l5_range_rotation_funcs.h"
#include "vl53l5_range_rotation_main.h"
#include "dci_defs.h"
#include "dci_luts.h"
#include "vl53l5_rr_version_defs.h"
#include "vl53l5_rr_defs.h"
#include "vl53l5_rr_luts.h"

void vl53l5_rr_get_version(
	struct  common_grp__version_t  *pversion)
{

	pversion->version__major = VL53L5_RR_DEF__VERSION__MAJOR;
	pversion->version__minor = VL53L5_RR_DEF__VERSION__MINOR;
	pversion->version__build = VL53L5_RR_DEF__VERSION__BUILD;
	pversion->version__revision = VL53L5_RR_DEF__VERSION__REVISION;
}

void vl53l5_range_rotation_main(
	struct   vl53l5_range_results_t                      *pip_results,
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
	struct  vl53l5_sharpener_target_data_t   *pip_sharp,
#endif
	struct   vl53l5_rr_cfg_dev_t               *pcfg_dev,
	struct   vl53l5_rr_int_dev_t               *pint_dev,
	struct   vl53l5_range_results_t                      *pop_results,
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
	struct  vl53l5_sharpener_target_data_t   *pop_sharp,
#endif
	struct   vl53l5_rr_err_dev_t               *perr_dev)
{

	int32_t   iz  = 0;

	int32_t   oz  = 0;

	struct   common_grp__status_t *perror = &(perr_dev->rr_error_status);
	struct   common_grp__status_t *pwarning = &(perr_dev->rr_warning_status);

	struct   vl53l5_range_results_t                      *pin_rng = pip_results;
	struct   vl53l5_range_results_t                      *pou_rng = pop_results;

#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
	struct   vl53l5_sharpener_target_data_t  *pin_sha = pip_sharp;
	struct   vl53l5_sharpener_target_data_t  *pou_sha = pop_sharp;
#endif

	int32_t   no_of_zones =
		(int32_t)pip_results->common_data.rng__no_of_zones;

	vl53l5_rr_clear_data(
		perr_dev);

	switch (pcfg_dev->rr_general_cfg.rr__mode) {
	case VL53L5_RR_MODE__DISABLED:
		pwarning->status__type = VL53L5_RR_WARNING_DISABLED;
		pin_rng       = pip_results;
		pou_rng       = pop_results;
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
		pin_sha       = pip_sharp;
		pou_sha       = pop_sharp;
#endif
		break;
	case VL53L5_RR_MODE__ENABLED:
		pwarning->status__type = VL53L5_RR_WARNING_NONE;
		pin_rng       = pip_results;
		pou_rng       = pop_results;
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
		pin_sha       = pip_sharp;
		pou_sha       = pop_sharp;
#endif
		break;
	case VL53L5_RR_MODE__ENABLED_INPLACE:
		pwarning->status__type = VL53L5_RR_WARNING_NONE;
		pin_rng       = pop_results;
		pou_rng       = pip_results;
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
		pin_sha       = pop_sharp;
		pou_sha       = pip_sharp;
#endif
		break;
	default:
		perror->status__type = VL53L5_RR_ERROR_INVALID_PARAMS;
		break;
	}

	if (perror->status__type == VL53L5_RR_ERROR_NONE) {
		memcpy(
			pop_results,
			pip_results,
			sizeof(struct vl53l5_range_results_t));
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
		memcpy(
			pop_sharp,
			pip_sharp,
			sizeof(struct vl53l5_sharpener_target_data_t));
#endif
	}

	if (perror->status__type == VL53L5_RR_ERROR_NONE &&
		pwarning->status__type != VL53L5_RR_WARNING_DISABLED) {
		vl53l5_rr_init_zone_order_coeffs(
			no_of_zones,
			(int32_t)pcfg_dev->rr_general_cfg.rr__rotation_sel,
			&(pint_dev->rr_zone_order_cfg),
			perror);
	}

	for (oz = 0 ; oz < no_of_zones; oz++) {
		if (perror->status__type == VL53L5_RR_ERROR_NONE &&
			pwarning->status__type != VL53L5_RR_WARNING_DISABLED) {
			iz =
				vl53l5_rr_new_sequence_idx(
					oz,
					&(pint_dev->rr_zone_order_cfg),
					perror);
		}

		if (perror->status__type == VL53L5_RR_ERROR_NONE &&
			pwarning->status__type != VL53L5_RR_WARNING_DISABLED) {
			vl53l5_rr_rotate_per_zone_data(
				iz,
				oz,
				pin_rng,
				pou_rng);

			vl53l5_rr_rotate_per_target_data(
				iz,
				oz,
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
				pin_sha,
				pou_sha,
#endif
				pin_rng,
				pou_rng);
		}
	}
}
