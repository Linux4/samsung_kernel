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

#include "dci_defs.h"
#include "dci_luts.h"
#include "vl53l5_ots_defs.h"
#include "vl53l5_ots_luts.h"
#include "vl53l5_output_target_sort__set_cfg_main.h"

void vl53l5_output_target_sort__set_cfg__default(
	uint8_t                                 ots__mode,
	struct vl53l5_ots__general__cfg_t      *pgen_cfg)
{

	pgen_cfg->ots__mode                 = ots__mode;
	pgen_cfg->ots__gen_cfg__spare_0     = 0U;
	pgen_cfg->ots__gen_cfg__spare_1     = 0U;
	pgen_cfg->ots__gen_cfg__spare_2     = 0U;

}

void vl53l5_output_target_sort__set_cfg_main(
	uint32_t                       ots_cfg_preset,
	struct vl53l5_ots_cfg_dev_t   *pcfg_dev,
	struct vl53l5_ots_err_dev_t   *perr_dev)
{

	struct  common_grp__status_t  *pstatus = &(perr_dev->ots_error_status);

	pstatus->status__group = VL53L5_ALGO_OUTPUT_TARGET_SORT_GROUP_ID;
	pstatus->status__stage_id = VL53L5_OTS_STAGE_ID__SET_CFG;
	pstatus->status__type = VL53L5_OTS_ERROR_NONE;
	pstatus->status__debug_0 = 0U;
	pstatus->status__debug_1 = 0U;
	pstatus->status__debug_2 = 0U;

	switch (ots_cfg_preset) {
	case VL53L5_OTS_CFG_PRESET__NONE:

		vl53l5_output_target_sort__set_cfg__default(
			VL53L5_OTS_MODE__DISABLED,
			&(pcfg_dev->ots_general_cfg));

		break;

	case VL53L5_OTS_CFG_PRESET__TUNING_0:

		vl53l5_output_target_sort__set_cfg__default(
			VL53L5_OTS_MODE__CLOSEST_FIRST,
			&(pcfg_dev->ots_general_cfg));

		break;

	case VL53L5_OTS_CFG_PRESET__TUNING_1:

		vl53l5_output_target_sort__set_cfg__default(
			VL53L5_OTS_MODE__STRONGEST_FIRST,
			&(pcfg_dev->ots_general_cfg));

		break;

	case VL53L5_OTS_CFG_PRESET__TUNING_2:

		vl53l5_output_target_sort__set_cfg__default(
			VL53L5_OTS_MODE__MOST_CONFIDENT_FIRST,
			&(pcfg_dev->ots_general_cfg));

		break;

	default:
		pstatus->status__type =
			VL53L5_OTS_ERROR_INVALID_PARAMS;
		break;
	}
}
