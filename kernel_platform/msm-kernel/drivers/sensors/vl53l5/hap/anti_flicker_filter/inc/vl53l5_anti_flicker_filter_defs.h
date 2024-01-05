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

#ifndef VL53L5_ANTI_FLICKER_FILTER_DEFS_H_
#define VL53L5_ANTI_FLICKER_FILTER_DEFS_H_

#include "vl53l5_types.h"
#include "vl53l5_results_config.h"
#include "vl53l5_anti_flicker_filter_structs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AFF_PM_MAX_TGT_IDX        0
#define AFF_PM_NB_PAST_IDX        1
#define AFF_PM_NB_ZONE_IDX        2
#define AFF_PM_TEMP_IND_IDX       3
#define AFF_PM_HISTORY_IDX        4
#define AFF_WS_CUR_TGT_ARR_IDX    0
#define AFF_WS_OUT_CAND_ARR_IDX   (AFF_WS_CUR_TGT_ARR_IDX + (DCI_MAX_TARGET_PER_ZONE*sizeof(struct vl53l5_aff_tgt_data_t)/4))

#define aff__iir_alpha                          aff__spare0
#define aff__avg_option                         aff__missing_cur_zscore_mode
#define AFF__IIR_ENABLED                        (0)
#define AFF__MIRRORING_TYPE                     (1)
#define AFF__IIR_AVG_ON_RNG_DATA                (2)
#define AFF__KEEP_PREV_OUT_ON_EMPTY_TGT_HIST    (3)
#define AFF__USE_PREV_OUT_IN_STATUS_DERIVATION  (4)
#define AFF__TWEAK_MIRRORING                    (5)

#define SET_BIT_UINT8(x, bit_nb)    ((x) = (x) | (uint8_t)(1 << bit_nb))
#define CLEAR_BIT_UINT8(x, bit_nb)  ((x) = (x) & (uint8_t)(~(1 << bit_nb)))
#define IS_BIT_SET_UINT8(x, bit_nb) ((((uint8_t)(x)) >> bit_nb) & (0x1))

#ifdef __cplusplus
}
#endif

#endif
