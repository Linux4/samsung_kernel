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

#ifndef __VL53L5_GD_DEFS_H__
#define __VL53L5_GD_DEFS_H__

#include "vl53l5_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VL53L5_ALGO_GLASS_DETECTION_GROUP_ID \
	((int16_t) 16)

#define VL53L5_GD_DEF__MAX_STRING_LENGTH \
	((uint32_t) 256U)

#define VL53L5_GD_DEF__MAX_TGT_STATUS_LIST_SIZE \
	((uint32_t) 8U)

#define VL53L5_GD_DEF__MAX_ZONES \
	((uint32_t) 64U)

#define VL53L5_GD_DEF__MAX_COUNTS \
	((uint32_t) 16U)

#define VL53L5_GD_DEF__MAX_POINTS \
	((uint32_t) 4U)

#define VL53L5_GD_DEF__MAX_COEFFS \
	((uint32_t) 8U)

#define VL53L5_GD_DEF__DISTANCE_MM_INTEGER_BITS \
	((uint32_t) 14U)

#define VL53L5_GD_DEF__DISTANCE_MM_FRAC_BITS \
	((uint32_t) 0U)

#define VL53L5_GD_DEF__DISTANCE_MM_MAX_VALUE \
	((int32_t) 8191)

#define VL53L5_GD_DEF__DISTANCE_MM_MIN_VALUE \
	((int32_t) -8192)

#define VL53L5_GD_DEF__DISTANCE_MM_UNITY_VALUE \
	((int32_t) 1)

#define VL53L5_GD_DEF__PLANE_COEFF_INTEGER_BITS \
	((uint32_t) 22U)

#define VL53L5_GD_DEF__PLANE_COEFF_FRAC_BITS \
	((uint32_t) 10U)

#define VL53L5_GD_DEF__PLANE_COEFF_UNITY_VALUE \
	((int32_t) 1024)

#define VL53L5_GD_DEF__RATE_RATIO_INTEGER_BITS \
	((uint32_t) 12U)

#define VL53L5_GD_DEF__RATE_RATIO_FRAC_BITS \
	((uint32_t) 8U)

#define VL53L5_GD_DEF__RATE_RATIO_MAX_VALUE \
	((uint32_t) 1048575U)

#define VL53L5_GD_DEF__RATE_RATIO_MIN_VALUE \
	((uint32_t) 0U)

#define VL53L5_GD_DEF__RATE_RATIO_UNITY_VALUE \
	((uint32_t) 256U)

#define VL53L5_GD_DEF__PERCENT_INTEGER_BITS \
	((uint32_t) 7U)

#define VL53L5_GD_DEF__PERCENT_FRAC_BITS \
	((uint32_t) 0U)

#define VL53L5_GD_DEF__PERCENT_MAX_VALUE \
	((uint32_t) 100U)

#define VL53L5_GD_DEF__PERCENT_MIN_VALUE \
	((uint32_t) 0U)

#define VL53L5_GD_DEF__PERCENT_UNITY_VALUE \
	((uint32_t) 1U)

#define VL53L5_GD_DEF__EFFECTIVE_FOCAL_LENGTH_UM \
	((uint32_t) 587601U)

#define VL53L5_GD_DEF__SPAD_PITCH_UM \
	((uint32_t) 15790U)

#define VL53L5_GD_DEF__NO_OF_SEARCH_DIRECTIONS_MERGED_TGT \
	((uint8_t) 4U)

#define VL53L5_GD_DEF__NO_OF_SEARCH_DIRECTIONS_MIRROR \
	((uint8_t) 4U)

#ifdef __cplusplus
}
#endif

#endif
