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

#include "vl53l5_host_algo_pipe_funcs.h"
#include "vl53l5_hap_version_defs.h"

#ifdef VL53L5_ALGO_LONG_TAIL_FILTER_ON
#include "vl53l5_long_tail_filter_main.h"
#endif
#ifdef VL53L5_ALGO_GLASS_DETECTION_ON
#include "vl53l5_glass_detection_main.h"
#endif
#ifdef VL53L5_ALGO_RANGE_ROTATION_ON
#include "vl53l5_range_rotation_main.h"
#endif
#ifdef VL53L5_ALGO_ANTI_FLICKER_FILTER_ON
#include "vl53l5_aff_defs.h"
#include "vl53l5_anti_flicker_filter_main.h"
#endif
#ifdef VL53L5_ALGO_OUTPUT_TARGET_SORT_ON
#include "vl53l5_output_target_sort_main.h"
#endif
#include "vl53l5_output_target_filter_main.h"
#ifdef VL53L5_ALGO_DEPTH16_ON
#include "vl53l5_depth16_main.h"
#endif
#ifdef VL53L5_ALGO_DEPTH16_BUFFER_ON
#include "vl53l5_depth16_buffer_main.h"
#endif

int32_t vl53l5_hap_encode_status_code(
	struct  common_grp__status_t  *p_status)
{
	int32_t  status = 0;
	uint32_t tmp = 0;
	uint16_t *p_reinterpret = NULL;

	if (p_status->status__type != 0) {
		status = (int32_t)VL53L5_HAP_DEF__BASE_FW_ERROR;

		p_reinterpret = (uint16_t *)&p_status->status__group;
		tmp |= ((uint32_t)(0xff & *p_reinterpret) << 24);

		p_reinterpret = (uint16_t *)&p_status->status__type;
		tmp |= ((uint32_t)(0xff & *p_reinterpret) << 16);

		p_reinterpret = (uint16_t *)&p_status->status__stage_id;
		tmp |= ((uint32_t)(0xff & *p_reinterpret) << 8);

		p_reinterpret = (uint16_t *)&p_status->status__debug_2;
		tmp |= (uint32_t)(0xff & *p_reinterpret);

		status += (int32_t)tmp;
	}

	return status;
}

void vl53l5_hap_decode_status_code(
	int32_t                        encoded_status,
	struct  common_grp__status_t  *p_status)
{
	p_status->status__group =
		(int16_t)((encoded_status >> 24) & 0x001F);
	p_status->status__type =
		(int16_t)((encoded_status >> 16) & 0x001F);
	p_status->status__stage_id =
		(int16_t)((encoded_status >> 8) & 0x00FF);
	p_status->status__debug_2 =
		(uint16_t)(encoded_status & 0x00FF);
}

void vl53l5_hap_get_version(
	struct  vl53l5_hap_ver_dev_t  *pver_dev)
{

	pver_dev->pipe_version.version__major =
		VL53L5_HAP_DEF__VERSION__MAJOR;
	pver_dev->pipe_version.version__minor =
		VL53L5_HAP_DEF__VERSION__MINOR;
	pver_dev->pipe_version.version__build =
		VL53L5_HAP_DEF__VERSION__BUILD;
	pver_dev->pipe_version.version__revision =
		VL53L5_HAP_DEF__VERSION__REVISION;

#ifdef VL53L5_ALGO_LONG_TAIL_FILTER_ON
	vl53l5_ltf_get_version(&(pver_dev->ltf_version));
#endif
#ifdef VL53L5_ALGO_GLASS_DETECTION_ON
	vl53l5_gd_get_version(&(pver_dev->gd_version));
#endif
#ifdef VL53L5_ALGO_RANGE_ROTATION_ON
	vl53l5_rr_get_version(&(pver_dev->rr_version));
#endif
#ifdef VL53L5_ALGO_ANTI_FLICKER_FILTER_ON
	vl53l5_aff_get_version(&(pver_dev->aff_version));
#endif
#ifdef VL53L5_ALGO_OUTPUT_TARGET_SORT_ON
	vl53l5_ots_get_version(&(pver_dev->ots_version));
#endif
	vl53l5_otf_get_version(&(pver_dev->otf_version));
#ifdef VL53L5_ALGO_DEPTH16_ON
	vl53l5_d16_get_version(&(pver_dev->d16_version));
#endif
#ifdef VL53L5_ALGO_DEPTH16_BUFFER_ON
	vl53l5_d16_buf_get_version(&(pver_dev->d16_buf_version));
#endif
}

void vl53l5_hap_clear_data(
	struct vl53l5_hap_err_dev_t          *perr_dev)
{
	struct common_grp__status_t    *perror   = &(perr_dev->hap_error_status);
	struct common_grp__status_t    *pwarning = &(perr_dev->hap_warning_status);

	perror->status__group = VL53L5_HOST_ALGO_PIPE_GROUP_ID;
	perror->status__stage_id = VL53L5_HAP_STAGE_ID__MAIN;
	perror->status__type = VL53L5_HAP_ERROR_NONE;
	perror->status__debug_0 = 0U;
	perror->status__debug_1 = 0U;
	perror->status__debug_2 = 0U;

	pwarning->status__group = VL53L5_HOST_ALGO_PIPE_GROUP_ID;
	pwarning->status__stage_id = VL53L5_HAP_STAGE_ID__MAIN;
	pwarning->status__type = VL53L5_HAP_WARNING_NONE;
	pwarning->status__debug_0 = 0U;
	pwarning->status__debug_1 = 0U;
	pwarning->status__debug_2 = 0U;
}

#ifdef VL53L5_ALGO_ANTI_FLICKER_FILTER_ON
void vl53l5_hap_update_aff_cfg(
	struct  vl53l5_hap__general__cfg_t   *phap_gen_cfg,
	struct  vl53l5_range_results_t                 *prng_dev,
	struct  vl53l5_aff_cfg_dev_t         *paff_cfg,
	struct  vl53l5_hap_err_dev_t         *perr_dev)
{

	uint32_t  i = 0U;
	uint32_t  nb_zones_to_filter = 0U;
	uint32_t  initial_zone_index = 0U;

	uint32_t  no_of_zones =
		(uint32_t)prng_dev->common_data.rng__no_of_zones;
	uint32_t  acc__no_of_zones =
		(uint32_t)prng_dev->common_data.rng__acc__no_of_zones;

	struct  common_grp__status_t  *perr_status =
		&(perr_dev->hap_error_status);

	perr_status->status__group = VL53L5_HOST_ALGO_PIPE_GROUP_ID;
	perr_status->status__stage_id = VL53L5_HAP_STAGE_ID__UPDATE_AFF_CFG;
	perr_status->status__type = VL53L5_HAP_ERROR_NONE;

	switch (phap_gen_cfg->aff_processing_mode) {
	case VL53L5_AFF_PROCESSING_MODE__DISABLED:
		nb_zones_to_filter = 0;
		initial_zone_index = 0;
		break;
	case VL53L5_AFF_PROCESSING_MODE__NORMAL_ONLY:
		nb_zones_to_filter = no_of_zones;
		initial_zone_index = 0;
		break;
	case VL53L5_AFF_PROCESSING_MODE__ASZ_ONLY:
		nb_zones_to_filter = acc__no_of_zones;
		initial_zone_index = no_of_zones;
		break;
	case VL53L5_AFF_PROCESSING_MODE__BOTH:
		nb_zones_to_filter = no_of_zones + acc__no_of_zones;
		initial_zone_index = 0;
		break;
	default:
		perr_status->status__type = VL53L5_HAP_ERROR_INVALID_PARAMS;
		break;
	}

	if (perr_status->status__type == VL53L5_HAP_ERROR_NONE) {
		paff_cfg->aff_cfg.aff__nb_zones_to_filter =
			(uint8_t)(nb_zones_to_filter);
		for (i = 0U; i < VL53L5_AFF_DEF__MAX_ZONE_SEL; i++) {
			paff_cfg->aff_arrayed_cfg.aff__zone_sel[i] = 0U;
			if (i < nb_zones_to_filter) {
				paff_cfg->aff_arrayed_cfg.aff__zone_sel[i] =
					(uint8_t)(initial_zone_index + i);
			}
		}
	}
}
#endif
