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

#ifndef __VL53L5_HAP_LUTS_H__
#define __VL53L5_HAP_LUTS_H__

#include "vl53l5_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VL53L5_HAP_MODE__INVALID \
	((uint8_t) 0U)

#define VL53L5_HAP_MODE__DISABLED \
	((uint8_t) 1U)

#define VL53L5_HAP_MODE__ENABLED \
	((uint8_t) 2U)

#define VL53L5_AFF_PROCESSING_MODE__INVALID \
	((uint8_t) 0U)

#define VL53L5_AFF_PROCESSING_MODE__DISABLED \
	((uint8_t) 1U)

#define VL53L5_AFF_PROCESSING_MODE__NORMAL_ONLY \
	((uint8_t) 2U)

#define VL53L5_AFF_PROCESSING_MODE__ASZ_ONLY \
	((uint8_t) 3U)

#define VL53L5_AFF_PROCESSING_MODE__BOTH \
	((uint8_t) 4U)

#define VL53L5_HAP_STAGE_ID__INVALID \
	((int16_t) 0)

#define VL53L5_HAP_STAGE_ID__SET_CFG \
	((int16_t) 1)

#define VL53L5_HAP_STAGE_ID__SET_GENERAL_CFG \
	((int16_t) 2)

#define VL53L5_HAP_STAGE_ID__UPDATE_AFF_CFG \
	((int16_t) 3)

#define VL53L5_HAP_STAGE_ID__MAIN \
	((int16_t) 4)

#define VL53L5_HAP_ERROR_NONE \
	((int16_t) 0)

#define VL53L5_HAP_ERROR_INVALID_PARAMS \
	((int16_t) 1)

#define VL53L5_HAP_ERROR_DIVISION_BY_ZERO \
	((int16_t) 2)

#define VL53L5_HAP_ERROR_NOT_IMPLEMENTED \
	((int16_t) 3)

#define VL53L5_HAP_ERROR_UNDEFINED \
	((int16_t) 4)

#define VL53L5_HAP_WARNING_NONE \
	((int16_t) 0)

#define VL53L5_HAP_WARNING_DISABLED \
	((int16_t) 1)

#ifdef __cplusplus
}
#endif

#endif
