/*******************************************************************************
* Copyright (c) 2020, STMicroelectronics - All Rights Reserved
*
* This file is part of VL53L5 Algo Pipe and is dual licensed,
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
* Alternatively, VL53L5 Algo Pipe may be distributed under the terms of
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

#ifndef __VL53L5_AFF_LUTS_H__
#define __VL53L5_AFF_LUTS_H__

#include "vl53l5_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VL53L5_AFF_ERROR__NONE \
	((uint32_t) 0U)

#define VL53L5_AFF_ERROR__DIVISION_BY_ZERO \
	((uint32_t) 1U)

#define VL53L5_AFF_ERROR__PERSISTENT_MEM_TOO_SMALL \
	((uint32_t) 2U)

#define VL53L5_AFF_ERROR__WORKSPACE_MEM_TOO_SMALL \
	((uint32_t) 3U)

#define VL53L5_AFF_ERROR__INVALID_PARAM \
	((uint32_t) 4U)

#define VL53L5_AFF_ERROR__INVALID_CFG_MODE \
	((uint32_t) 5U)

#define VL53L5_AFF_ERROR__BAD_ZONE_ARR_IDX \
	((uint32_t) 6U)

#define VL53L5_AFF_WARNING__NONE \
	((uint32_t) 0U)

#define VL53L5_AFF_WARNING__ALGO_DISABLED \
	((uint32_t) 1U)

#define VL53L5_AFF_OUT_SORT_OPTION__ZSCORE_THEN_CLOSEST \
	((uint32_t) 0U)

#define VL53L5_AFF_OUT_SORT_OPTION__ZSCORE_THEN_STRONGEST \
	((uint32_t) 1U)

#define VL53L5_AFF_OUT_SORT_OPTION__ZSCORE_AND_PREV_BEST \
	((uint32_t) 2U)

#define VL53L5_AFF_AFFINITY_OPTION__RANGE_ONLY \
	((uint32_t) 0U)

#define VL53L5_AFF_AFFINITY_OPTION__RANGE_AND_SIGMA \
	((uint32_t) 1U)

#define VL53L5_AFF_AFFINITY_OPTION__PULSE_OVERLAP \
	((uint32_t) 2U)

#define VL53L5_AFF_MODE__INVALID \
	((uint32_t) 0U)

#define VL53L5_AFF_MODE__DISABLED \
	((uint32_t) 1U)

#define VL53L5_AFF_MODE__ENABLED \
	((uint32_t) 2U)

#define VL53L5_AFF_CFG_PRESET__INVALID \
	((uint32_t) 0U)

#define VL53L5_AFF_CFG_PRESET__NONE \
	((uint32_t) 1U)

#define VL53L5_AFF_CFG_PRESET__TUNING_0 \
	((uint32_t) 2U)

#define VL53L5_AFF_CFG_PRESET__TUNING_1 \
	((uint32_t) 3U)

#define VL53L5_AFF_CFG_PRESET__TUNING_2 \
	((uint32_t) 4U)

#define VL53L5_AFF_CFG_PRESET__TUNING_3 \
	((uint32_t) 5U)

#define VL53L5_AFF_CFG_PRESET__TUNING_4 \
	((uint32_t) 6U)

#define VL53L5_AFF_CFG_PRESET__TUNING_5 \
	((uint32_t) 7U)

#define VL53L5_AFF_CFG_PRESET__TUNING_6 \
	((uint32_t) 8U)

#define VL53L5_AFF_CFG_PRESET__TUNING_7 \
	((uint32_t) 9U)

#define VL53L5_AFF_STAGE_ID__INVALID \
	((int16_t) 0)

#define VL53L5_AFF_STAGE_ID__SET_CFG \
	((int16_t) 1)

#define VL53L5_AFF_STAGE_ID__MAIN \
	((int16_t) 2)

#define VL53L5_AFF_STAGE_ID__INIT_START_RNG \
	((int16_t) 3)

#define VL53L5_AFF_STAGE_ID__RESET_ZONE \
	((int16_t) 4)

#define VL53L5_AFF_STAGE_ID__GET_VERSION \
	((int16_t) 5)

#define VL53L5_AFF_STAGE_ID__CLEAR_DATA \
	((int16_t) 6)

#define VL53L5_AFF_STAGE_ID__INIT_MEM \
	((int16_t) 7)

#define VL53L5_AFF_STAGE_ID__CUR_TGT_ARR \
	((int16_t) 8)

#define VL53L5_AFF_STAGE_ID__OUT_CAND_ARR \
	((int16_t) 9)

#define VL53L5_AFF_STAGE_ID__FILLED_TGT_HIST_ARR \
	((int16_t) 10)

#define VL53L5_AFF_STAGE_ID__AFFINITY_MATRIX \
	((int16_t) 11)

#define VL53L5_AFF_STAGE_ID__HIST_CUR_ASSO \
	((int16_t) 12)

#define VL53L5_AFF_STAGE_ID__CUR_HIST_ASSO \
	((int16_t) 13)

#define VL53L5_AFF_STAGE_ID__GET_CUR_PREF \
	((int16_t) 14)

#define VL53L5_AFF_STAGE_ID__GET_HIST_PREF \
	((int16_t) 15)

#define VL53L5_AFF_STAGE_ID__GET_IND_TO_BE_ASSO \
	((int16_t) 16)

#define VL53L5_AFF_STAGE_ID__PASSED_DATA \
	((int16_t) 17)

#define VL53L5_AFF_STAGE_ID__GET_TGT_HIST \
	((int16_t) 18)

#define VL53L5_AFF_STAGE_ID__DCI_TO_CUR_TGT \
	((int16_t) 19)

#define VL53L5_AFF_STAGE_ID__COMP_AFF_MATRIX \
	((int16_t) 20)

#define VL53L5_AFF_STAGE_ID__AVG_TGT_HIST \
	((int16_t) 21)

#define VL53L5_AFF_STAGE_ID__TGT_TO_MEM \
	((int16_t) 22)

#define VL53L5_AFF_STAGE_ID__COPY_DATA \
	((int16_t) 23)

#define VL53L5_AFF_STAGE_ID__TEMPORAL_SHIFT \
	((int16_t) 24)

#define VL53L5_AFF_STAGE_ID__CAND_TO_DCI \
	((int16_t) 25)

#define VL53L5_AFF_STAGE_ID__UPDATE_TEMP_IDX \
	((int16_t) 26)

#define VL53L5_AFF_STAGE_ID__COMP_CUR_PREF \
	((int16_t) 27)

#define VL53L5_AFF_STAGE_ID__COMP_HIST_PREF \
	((int16_t) 28)

#define VL53L5_AFF_STAGE_ID__MATCH_TGT \
	((int16_t) 29)

#ifdef __cplusplus
}
#endif

#endif
