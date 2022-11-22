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
#include "dci_size.h"
#include "dci_ui_union_structs.h"
#include "dci_patch_defs.h"
#include "dci_patch_luts.h"
#include "vl53l5_d16_defs.h"
#include "vl53l5_d16_luts.h"
#include "vl53l5_d16_size.h"
#include "vl53l5_depth16__set_cfg_main.h"

void vl53l5_depth16__set_cfg__default(
	uint32_t                             cfg_preset,
	uint32_t                             rng__no_of_zones,
	uint32_t                             rng__max_targets_per_zone,
	uint32_t                             rng__acc__no_of_zones,
	uint8_t                              d16_cfg_mode,
	uint8_t                              d16_buf_mode,
	struct  vl53l5_d16_cfg_dev_t        *pcfg_dev)
{

	uint32_t i = 0U;
	uint32_t o = 0U;
	union  dci_union__block_header_u  bh;

	uint32_t  bh_data[] = {
		2, rng__no_of_zones      * rng__max_targets_per_zone,   DCI_D16_BH_PARM_ID__DEPTH16,
		4, rng__no_of_zones,                                    DCI_D16_BH_PARM_ID__AMB_RATE_KCPS_PER_SPAD,
		0, 4,                                                   DCI_D16_BH_PARM_ID__GLASS_DETECT,
		4, rng__no_of_zones      * rng__max_targets_per_zone,   DCI_D16_BH_PARM_ID__PEAK_RATE_KCPS_PER_SPAD,
		2, rng__acc__no_of_zones * rng__max_targets_per_zone,   DCI_D16_BH_PARM_ID__SZ__DEPTH16,
		4, rng__acc__no_of_zones,                               DCI_D16_BH_PARM_ID__SZ__AMB_RATE_KCPS_PER_SPAD,
		4, rng__acc__no_of_zones * rng__max_targets_per_zone,   DCI_D16_BH_PARM_ID__SZ__PEAK_RATE_KCPS_PER_SPAD,
	};
	uint32_t bh_count = sizeof(bh_data) / (3 * sizeof(uint32_t));

	pcfg_dev->d16_general_cfg.d16__max_buffer_size =
		DCI_D16_MAX_BUFFER_SIZE;
	pcfg_dev->d16_general_cfg.d16__packet_size =
		DCI_D16_PACKET_SIZE;
	pcfg_dev->d16_general_cfg.d16__cfg__mode = d16_cfg_mode;
	pcfg_dev->d16_general_cfg.d16__buf__mode = d16_buf_mode;
	pcfg_dev->d16_general_cfg.d16__gen_cfg__spare_0 = 0U;
	pcfg_dev->d16_general_cfg.d16__gen_cfg__spare_1 = 0U;

	for (i = 0; i < VL53L5_D16_DEF__MAX_BH_LIST_SIZE; i++)
		pcfg_dev->d16_arrayed_cfg.bh_list[i] = 0U;

	for (i = 0; (i < bh_count) && (i < cfg_preset); i++) {
		o = i * 3U;
		bh.bytes = 0U;
		bh.p_type = 0x000FU & bh_data[o];
		bh.b_size__p_rep = 0x0FFFU & bh_data[o + 1U];
		bh.b_idx__p_idx = 0x0000FFFFU & bh_data[o + 2U];
		pcfg_dev->d16_arrayed_cfg.bh_list[i] = bh.bytes;
	}
}

void vl53l5_depth16__set_cfg_main(
	uint32_t                             cfg_preset,
	uint32_t                             rng__no_of_zones,
	uint32_t                             rng__max_targets_per_zone,
	uint32_t                             rng__acc__no_of_zones,
	struct  vl53l5_d16_cfg_dev_t        *pcfg_dev,
	struct  vl53l5_d16_err_dev_t        *perr_dev)
{

	struct  common_grp__status_t  *pstatus = &(perr_dev->d16_error_status);

	pstatus->status__group = VL53L5_ALGO_DEPTH16_GROUP_ID;
	pstatus->status__stage_id = VL53L5_D16_STAGE_ID__SET_CFG;
	pstatus->status__type = VL53L5_D16_ERROR_NONE;
	pstatus->status__debug_0 = 0U;
	pstatus->status__debug_1 = 0U;
	pstatus->status__debug_2 = 0U;

	switch (cfg_preset) {
	case VL53L5_D16_CFG_PRESET__NONE:
		vl53l5_depth16__set_cfg__default(
			cfg_preset,
			rng__no_of_zones,
			rng__max_targets_per_zone,
			rng__acc__no_of_zones,
			VL53L5_D16_MODE__DISABLED,
			VL53L5_D16_BUF_MODE__DISABLED,
			pcfg_dev);
		break;
	case VL53L5_D16_CFG_PRESET__TUNING_0:
	case VL53L5_D16_CFG_PRESET__TUNING_1:
	case VL53L5_D16_CFG_PRESET__TUNING_2:
	case VL53L5_D16_CFG_PRESET__TUNING_3:
	case VL53L5_D16_CFG_PRESET__TUNING_4:
	case VL53L5_D16_CFG_PRESET__TUNING_5:
		vl53l5_depth16__set_cfg__default(
			cfg_preset,
			rng__no_of_zones,
			rng__max_targets_per_zone,
			rng__acc__no_of_zones,
			VL53L5_D16_MODE__ENABLED,
			VL53L5_D16_BUF_MODE__ENABLED,
			pcfg_dev);
		break;
	default:
		pstatus->status__type =
			VL53L5_D16_ERROR_INVALID_PARAMS;
		break;
	}
}
