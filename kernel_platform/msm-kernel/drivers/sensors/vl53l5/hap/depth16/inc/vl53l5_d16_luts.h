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

#ifndef __VL53L5_D16_LUTS_H__
#define __VL53L5_D16_LUTS_H__

#include "vl53l5_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VL53L5_D16_MODE__INVALID \
	((uint32_t) 0U)

#define VL53L5_D16_MODE__DISABLED \
	((uint32_t) 1U)

#define VL53L5_D16_MODE__ENABLED \
	((uint32_t) 2U)

#define VL53L5_D16_BUF_MODE__INVALID \
	((uint32_t) 0U)

#define VL53L5_D16_BUF_MODE__DISABLED \
	((uint32_t) 1U)

#define VL53L5_D16_BUF_MODE__ENABLED \
	((uint32_t) 2U)

#define VL53L5_D16_CFG_PRESET__INVALID \
	((uint32_t) 0U)

#define VL53L5_D16_CFG_PRESET__NONE \
	((uint32_t) 1U)

#define VL53L5_D16_CFG_PRESET__TUNING_0 \
	((uint32_t) 2U)

#define VL53L5_D16_CFG_PRESET__TUNING_1 \
	((uint32_t) 3U)

#define VL53L5_D16_CFG_PRESET__TUNING_2 \
	((uint32_t) 4U)

#define VL53L5_D16_CFG_PRESET__TUNING_3 \
	((uint32_t) 5U)

#define VL53L5_D16_CFG_PRESET__TUNING_4 \
	((uint32_t) 6U)

#define VL53L5_D16_CFG_PRESET__TUNING_5 \
	((uint32_t) 7U)

#define VL53L5_D16_CFG_PRESET__TUNING_6 \
	((uint32_t) 8U)

#define VL53L5_D16_CFG_PRESET__TUNING_7 \
	((uint32_t) 9U)

#define VL53L5_D16_STAGE_ID__INVALID \
	((int16_t) 0)

#define VL53L5_D16_STAGE_ID__SET_CFG \
	((int16_t) 1)

#define VL53L5_D16_STAGE_ID__INIT \
	((int16_t) 2)

#define VL53L5_D16_STAGE_ID__MAIN \
	((int16_t) 3)

#define VL53L5_D16_STAGE_ID__PACKET_INIT \
	((int16_t) 4)

#define VL53L5_D16_STAGE_ID__GET_PARM_ID \
	((int16_t) 5)

#define VL53L5_D16_STAGE_ID__BLOCK_DECODE_WORD \
	((int16_t) 6)

#define VL53L5_D16_STAGE_ID__BLOCK_ENCODE_WORD \
	((int16_t) 7)

#define VL53L5_D16_STAGE_ID__BLOCK_DECODE \
	((int16_t) 8)

#define VL53L5_D16_STAGE_ID__BLOCK_ENCODE \
	((int16_t) 9)

#define VL53L5_D16_STAGE_ID__PACKET_DECODE_INIT \
	((int16_t) 10)

#define VL53L5_D16_STAGE_ID__PACKET_ENCODE_INIT \
	((int16_t) 11)

#define VL53L5_D16_STAGE_ID__PACKET_DECODE \
	((int16_t) 12)

#define VL53L5_D16_STAGE_ID__PACKET_ENCODE \
	((int16_t) 13)

#define VL53L5_D16_STAGE_ID__BUFFER_DECODE \
	((int16_t) 14)

#define VL53L5_D16_STAGE_ID__BUFFER_ENCODE \
	((int16_t) 15)

#define VL53L5_D16_ERROR_NONE \
	((int16_t) 0)

#define VL53L5_D16_ERROR_INVALID_PARAMS \
	((int16_t) 1)

#define VL53L5_D16_ERROR_DIVISION_BY_ZERO \
	((int16_t) 2)

#define VL53L5_D16_ERROR_NOT_IMPLEMENTED \
	((int16_t) 3)

#define VL53L5_D16_ERROR_UNDEFINED \
	((int16_t) 4)

#define VL53L5_D16_ERROR_UNSUPPORTED_NO_OF_ZONES \
	((int16_t) 5)

#define VL53L5_D16_ERROR_INVALID_MAX_TARGETS_PER_ZONE \
	((int16_t) 6)

#define VL53L5_D16_ERROR_INVALID_ZONE_ID \
	((int16_t) 7)

#define VL53L5_D16_ERROR_INVALID_SINGLE_ZONE_ID \
	((int16_t) 8)

#define VL53L5_D16_ERROR_INVALID_OP_MAX_TGTS_PER_ZONE \
	((int16_t) 9)

#define VL53L5_D16_ERROR_INVALID_PARM_ID \
	((int16_t) 10)

#define VL53L5_D16_ERROR_INVALID_WORD_IDX \
	((int16_t) 11)

#define VL53L5_D16_ERROR_INVALID_PACKET_IDX \
	((int16_t) 12)

#define VL53L5_D16_ERROR_INVALID_LAST_PACKET_FLAG \
	((int16_t) 13)

#define VL53L5_D16_ERROR_DEV_PTR_NULL \
	((int16_t) 14)

#define VL53L5_D16_ERROR_PACKET_PTR_NULL \
	((int16_t) 15)

#define VL53L5_D16_ERROR_PACKET_SIZE_TOO_SMALL \
	((int16_t) 16)

#define VL53L5_D16_ERROR_BUFFER_PTR_NULL \
	((int16_t) 17)

#define VL53L5_D16_ERROR_BUFFER_SIZE_TOO_SMALL \
	((int16_t) 18)

#define VL53L5_D16_WARNING_NONE \
	((int16_t) 0)

#define VL53L5_D16_WARNING_DISABLED \
	((int16_t) 1)

#define VL53L5_D16_WARNING_NO_GLASS_DETECT_DATA \
	((int16_t) 2)

#define VL53L5_D16_WARNING_NO_SPACE_FOR_SINGLE_ZONE \
	((int16_t) 3)

#ifdef __cplusplus
}
#endif

#endif
