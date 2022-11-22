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

#include "vl53l5_otf_version_defs.h"
#include "vl53l5_otf_defs.h"
#include "vl53l5_otf_luts.h"
#include "vl53l5_output_target_filter_funcs.h"
#include "vl53l5_output_target_filter_main.h"

void vl53l5_otf_get_version(
	struct  common_grp__version_t  *pversion)
{

	pversion->version__major = VL53L5_OTF_DEF__VERSION__MAJOR;
	pversion->version__minor = VL53L5_OTF_DEF__VERSION__MINOR;
	pversion->version__build = VL53L5_OTF_DEF__VERSION__BUILD;
	pversion->version__revision = VL53L5_OTF_DEF__VERSION__REVISION;
}

void vl53l5_output_target_filter_main(
	uint32_t                                     instance_sel,
	struct   vl53l5_range_results_t                       *presults,
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
	struct   vl53l5_sharpener_target_data_t   *psharp_tgt_data,
#endif
	struct   vl53l5_otf_cfg_dev_t               *pcfg_dev,
	struct   vl53l5_otf_int_dev_t               *pint_dev,
	struct   dci_patch_op_dev_t                 *pop_dev,
	struct   vl53l5_otf_err_dev_t               *perr_dev)
{

	int32_t   no_of_zones =
		(int32_t)presults->common_data.rng__no_of_zones;
	int32_t   acc_no_of_zones =
		(int32_t)presults->common_data.rng__acc__no_of_zones;
	int32_t   z  = 0;

	int32_t   ip_max_targets_per_zone =
		(int32_t)presults->common_data.rng__max_targets_per_zone;
	int32_t   op_max_targets_per_zone = 0;

	struct   vl53l5_otf__general__cfg_t     *pgen_cfg =
		&(pcfg_dev->otf_general_cfg_0);
	struct   vl53l5_otf__tgt_status__cfg_t  *ptgt_status_list_valid =
		&(pcfg_dev->otf_tgt_status_list_0v);
	struct   vl53l5_otf__tgt_status__cfg_t  *ptgt_status_list_blurred =
		&(pcfg_dev->otf_tgt_status_list_0b);

	struct   common_grp__status_t *perror = &(perr_dev->otf_error_status);
	struct   common_grp__status_t *pwarning = &(perr_dev->otf_warning_status);

	vl53l5_otf_clear_data(
		pop_dev,
		perr_dev);

	switch (instance_sel) {
	case VL53L5_OTF_INSTANCE__0:
		pgen_cfg = &(pcfg_dev->otf_general_cfg_0);
		ptgt_status_list_valid = &(pcfg_dev->otf_tgt_status_list_0v);
		ptgt_status_list_blurred = &(pcfg_dev->otf_tgt_status_list_0b);
		break;
	case VL53L5_OTF_INSTANCE__1:
		pgen_cfg = &(pcfg_dev->otf_general_cfg_1);
		ptgt_status_list_valid = &(pcfg_dev->otf_tgt_status_list_1v);
		ptgt_status_list_blurred = &(pcfg_dev->otf_tgt_status_list_1b);
		break;
	default:
		perror->status__type = VL53L5_OTF_ERROR_INVALID_PARAMS;
		break;
	}

	switch (pgen_cfg->otf__mode) {
	case VL53L5_OTF_MODE__DISABLED:
		pwarning->status__type = VL53L5_OTF_WARNING_DISABLED;
		break;
	case VL53L5_OTF_MODE__ENABLED:
		pwarning->status__type = VL53L5_OTF_WARNING_NONE;
		break;
	default:
		perror->status__type = VL53L5_OTF_ERROR_INVALID_PARAMS;
		break;
	}

	op_max_targets_per_zone =
		(int32_t)pgen_cfg->otf__max_targets_per_zone;

	if (op_max_targets_per_zone == 0)
		perror->status__type = VL53L5_OTF_ERROR_INVALID_OP_MAX_TGTS_PER_ZONE;

	if (op_max_targets_per_zone > ip_max_targets_per_zone)
		op_max_targets_per_zone = ip_max_targets_per_zone;

	if ((op_max_targets_per_zone * (no_of_zones + acc_no_of_zones)) >
		(int32_t)VL53L5_MAX_TARGETS)
		perror->status__type = VL53L5_OTF_ERROR_INVALID_OP_MAX_TGTS_PER_ZONE;

	for (z = 0 ; z < (no_of_zones + acc_no_of_zones); z++) {

		if (perror->status__type == VL53L5_OTF_ERROR_NONE &&
			pwarning->status__type != VL53L5_OTF_WARNING_DISABLED) {
			vl53l5_otf_target_filter(
				z,
				ip_max_targets_per_zone,
				op_max_targets_per_zone,
				pgen_cfg->otf__range_clip_en,
				ptgt_status_list_valid,
				ptgt_status_list_blurred,
				pint_dev,
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
				psharp_tgt_data,
#endif
				presults,
				perror);
		}
	}

	presults->common_data.rng__max_targets_per_zone =
		(uint8_t)op_max_targets_per_zone;

	for (z = 0 ; z < acc_no_of_zones; z++) {
		if (perror->status__type == VL53L5_OTF_ERROR_NONE &&
			pwarning->status__type != VL53L5_OTF_WARNING_DISABLED) {
			vl53l5_otf_copy_single_zone_data(
				no_of_zones + z,
				z,
				pgen_cfg->otf__range_clip_en,
				presults,
				pop_dev,
				perror);
		}
	}
}
