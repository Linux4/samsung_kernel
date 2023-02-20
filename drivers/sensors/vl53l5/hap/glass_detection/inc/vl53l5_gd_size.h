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

#ifndef __VL53L5_GD_SIZE_H__
#define __VL53L5_GD_SIZE_H__

#include "vl53l5_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VL53L5_VL53L5_GD_GENERAL_CFG_SZ \
	((uint16_t) 36)

#define VL53L5_VL53L5_GD_TGT_STATUS_CFG_SZ \
	((uint16_t) 8)

#define VL53L5_VGTSC_GD_TGT_STATUS_SZ \
	((uint16_t) 8)

#define VL53L5_VL53L5_GD_ZONE_META_SZ \
	((uint16_t) 4)

#define VL53L5_VL53L5_GD_ZONE_DATA_SZ \
	((uint16_t) 912)

#define VL53L5_VGZD_GD_PEAK_RATE_KCPS_PER_SPAD_SZ \
	((uint16_t) 256)

#define VL53L5_VGZD_GD_X_POSITON_MM_SZ \
	((uint16_t) 128)

#define VL53L5_VGZD_GD_Y_POSITON_MM_SZ \
	((uint16_t) 128)

#define VL53L5_VGZD_GD_Z_POSITON_MM_SZ \
	((uint16_t) 128)

#define VL53L5_VGZD_GD_DIST_FROM_PLANE_MM_SZ \
	((uint16_t) 128)

#define VL53L5_VGZD_GD_IS_VALID_SZ \
	((uint16_t) 64)

#define VL53L5_VGZD_GD_TARGET_STATUS_SZ \
	((uint16_t) 64)

#define VL53L5_VGZD_GD_TARGET_STATUS_COUNTS_SZ \
	((uint16_t) 16)

#define VL53L5_VL53L5_GD_TRIG_COEFFS_SZ \
	((uint16_t) 16)

#define VL53L5_VGTC_GD_TRIG_COEFFS_SZ \
	((uint16_t) 16)

#define VL53L5_VL53L5_GD_PLANE_POINTS_SZ \
	((uint16_t) 64)

#define VL53L5_VGPP_GD_PLANE_POINT_X_SZ \
	((uint16_t) 16)

#define VL53L5_VGPP_GD_PLANE_POINT_Y_SZ \
	((uint16_t) 16)

#define VL53L5_VGPP_GD_PLANE_POINT_Z_SZ \
	((uint16_t) 16)

#define VL53L5_VGPP_GD_PLANE_POINT_ZONE_SZ \
	((uint16_t) 16)

#define VL53L5_VL53L5_GD_PLANE_COEFFS_SZ \
	((uint16_t) 16)

#define VL53L5_VL53L5_GD_PLANE_STATUS_SZ \
	((uint16_t) 8)

#ifdef __cplusplus
}
#endif

#endif
