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
#include "vl53l5_gd_version_defs.h"
#include "vl53l5_gd_defs.h"
#include "vl53l5_gd_luts.h"
#include "vl53l5_glass_detection_funcs.h"
#include "vl53l5_glass_detection_main.h"

void vl53l5_gd_get_version(
	struct  common_grp__version_t  *pversion)
{

	pversion->version__major = VL53L5_GD_DEF__VERSION__MAJOR;
	pversion->version__minor = VL53L5_GD_DEF__VERSION__MINOR;
	pversion->version__build = VL53L5_GD_DEF__VERSION__BUILD;
	pversion->version__revision = VL53L5_GD_DEF__VERSION__REVISION;
}

void vl53l5_glass_detection_main(
	struct vl53l5_range_results_t          *presults,
	struct vl53l5_gd_cfg_dev_t   *pcfg_dev,
	struct vl53l5_gd_int_dev_t   *pint_dev,
	struct dci_patch_op_dev_t    *pop_dev,
	struct vl53l5_gd_err_dev_t   *perr_dev)
{

	struct   common_grp__status_t *perror = &(perr_dev->gd_error_status);
	struct   common_grp__status_t *pwarning = &(perr_dev->gd_warning_status);

	vl53l5_gd_clear_data(
		pint_dev,
		pop_dev,
		perr_dev);

	switch (pcfg_dev->gd_general_cfg.gd__mode) {
	case VL53L5_GD_MODE__DISABLED:
		pwarning->status__type = VL53L5_GD_ERROR_INVALID_PARAMS;
		break;
	case VL53L5_GD_MODE__ENABLED:
		pwarning->status__type = VL53L5_GD_WARNING_NONE;
		break;
	default:
		perror->status__type = VL53L5_GD_ERROR_INVALID_PARAMS;
		break;
	}

	if (perror->status__type == VL53L5_GD_ERROR_NONE &&
		pwarning->status__type == VL53L5_GD_WARNING_NONE) {
		vl53l5_gd_init_zone_meta_data(
			&(presults->common_data),
			&(pint_dev->gd_zone_meta),
			perror);
	}

	if (perror->status__type == VL53L5_GD_ERROR_NONE &&
		pwarning->status__type == VL53L5_GD_WARNING_NONE) {
		vl53l5_gd_init_trig_coeffs(
			&(pint_dev->gd_zone_meta),
			&(pint_dev->gd_trig_data),
			perror);
	}

	if (perror->status__type == VL53L5_GD_ERROR_NONE) {
		vl53l5_gd_filter_results(
			presults,
			pcfg_dev,
			pint_dev,
			pop_dev,
			perror);
	}

	if (perror->status__type == VL53L5_GD_ERROR_NONE &&
		pwarning->status__type == VL53L5_GD_WARNING_NONE) {
		vl53l5_gd_find_min_max_rates_valid(
			&(pint_dev->gd_zone_meta),
			&(pint_dev->gd_zone_data),
			&(pop_dev->gd_op_data),
			perror);
	}

	if (pop_dev->gd_op_data.gd__valid_zone_count <
		pcfg_dev->gd_general_cfg.gd__min_valid_zones) {
		pwarning->status__stage_id = VL53L5_GD_STAGE_ID__FILTER_RESULTS;
		pwarning->status__type = VL53L5_GD_WARNING_INSUFFICENT_VALID_ZONES;
	}

	if (pop_dev->gd_op_data.gd__max__valid_rate_kcps_spad == 0U) {
		pwarning->status__stage_id = VL53L5_GD_STAGE_ID__FIND_MIN_MAX_RATE__VALID;
		pwarning->status__type = VL53L5_GD_WARNING_MAX_VALID_RATE_ZERO;
	}

	if (perror->status__type == VL53L5_GD_ERROR_NONE &&
		pwarning->status__type == VL53L5_GD_WARNING_NONE) {
		vl53l5_gd_find_points(
			pcfg_dev,
			pint_dev,
			pop_dev,
			perror,
			pwarning);
	}

	if (perror->status__type == VL53L5_GD_ERROR_NONE &&
		pwarning->status__type == VL53L5_GD_WARNING_NONE) {
		vl53l5_gd_plane_fit(
			&(pint_dev->gd_plane_points),
			&(pint_dev->gd_plane_coeffs),
			perror,
			pwarning);
	}

	if (perror->status__type == VL53L5_GD_ERROR_NONE &&
		pwarning->status__type == VL53L5_GD_WARNING_NONE) {
		vl53l5_gd_calc_distance_from_plane(
			pcfg_dev,
			pint_dev,
			perror);
	}

	if (perror->status__type == VL53L5_GD_ERROR_NONE &&
		pwarning->status__type == VL53L5_GD_WARNING_NONE) {
		vl53l5_gd_find_min_max_rates_plane(
			pint_dev,
			pop_dev,
			perror);
	}

	if (perror->status__type == VL53L5_GD_ERROR_NONE &&
		pwarning->status__type == VL53L5_GD_WARNING_NONE) {
		vl53l5_gd_calc_rate_ratio(
			&(pcfg_dev->gd_general_cfg),
			&(pop_dev->gd_op_data),
			&(pop_dev->gd_op_status),
			perror);
	}

	if (perror->status__type == VL53L5_GD_ERROR_NONE &&
		pwarning->status__type == VL53L5_GD_WARNING_NONE) {
		vl53l5_gd_calc_points_on_plane_pc(
			&(pop_dev->gd_op_data),
			perror);
	}

	if (perror->status__type == VL53L5_GD_ERROR_NONE &&
		pwarning->status__type == VL53L5_GD_WARNING_NONE) {
		vl53l5_gd_update_output_status(
			pcfg_dev,
			pint_dev,
			pop_dev,
			perror);
	}
}
