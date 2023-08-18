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

#ifndef VL53L5_HOST_ALGO_PIPE_FUNCS_H_
#define VL53L5_HOST_ALGO_PIPE_FUNCS_H_

#include "vl53l5_types.h"
#include "vl53l5_results_config.h"
#include "vl53l5_algo_build_config.h"
#include "common_datatype_structs.h"
#include "vl53l5_results_map.h"
#include "vl53l5_hap_structs.h"
#include "vl53l5_hap_ver_map.h"
#include "vl53l5_hap_err_map.h"
#include "vl53l5_aff_cfg_map.h"

#define  VL53L5_HAP_DEF__BASE_FW_ERROR  0xE0000000U

int32_t vl53l5_hap_encode_status_code(
	struct  common_grp__status_t  *p_status);

void vl53l5_hap_decode_status_code(
	int32_t                        encoded_status,
	struct  common_grp__status_t  *p_status);

void vl53l5_hap_get_version(
	struct  vl53l5_hap_ver_dev_t  *pver_dev);

void vl53l5_hap_clear_data(
	struct vl53l5_hap_err_dev_t           *perr_dev);

#ifdef VL53L5_ALGO_ANTI_FLICKER_FILTER_ON

void vl53l5_hap_update_aff_cfg(
	struct  vl53l5_hap__general__cfg_t   *phap_gen_cfg,
	struct  vl53l5_range_results_t                 *prng_dev,
	struct  vl53l5_aff_cfg_dev_t         *paff_cfg,
	struct  vl53l5_hap_err_dev_t         *perr_dev);
#endif

#endif
