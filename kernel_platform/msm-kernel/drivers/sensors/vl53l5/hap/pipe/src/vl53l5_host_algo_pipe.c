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

#include "vl53l5_hap_luts.h"
#include "vl53l5_host_algo_pipe.h"
#include "vl53l5_host_algo_pipe_funcs.h"
#include "common_datatype_structs.h"
#ifdef VL53L5_ALGO_LONG_TAIL_FILTER_ON
#include "vl53l5_ltf_err_map.h"
#include "vl53l5_long_tail_filter_main.h"
#endif
#ifdef VL53L5_ALGO_GLASS_DETECTION_ON
#include "vl53l5_gd_err_map.h"
#include "vl53l5_glass_detection_main.h"
#endif
#ifdef VL53L5_ALGO_RANGE_ROTATION_ON
#include "vl53l5_rr_err_map.h"
#include "vl53l5_range_rotation_main.h"
#endif
#ifdef VL53L5_ALGO_ANTI_FLICKER_FILTER_ON
#include "vl53l5_aff_luts.h"
#include "vl53l5_aff_err_map.h"
#include "vl53l5_anti_flicker_filter_main.h"
#endif
#ifdef VL53L5_ALGO_OUTPUT_TARGET_SORT_ON
#include "vl53l5_ots_err_map.h"
#include "vl53l5_output_target_sort_main.h"
#endif
#include "vl53l5_otf_luts.h"
#include "vl53l5_otf_err_map.h"
#include "vl53l5_output_target_filter_main.h"
#ifdef VL53L5_ALGO_DEPTH16_ON
#include "vl53l5_d16_err_map.h"
#include "vl53l5_depth16_main.h"
#endif
#ifdef VL53L5_ALGO_DEPTH16_BUFFER_ON
#include "vl53l5_d16_luts.h"
#include "vl53l5_depth16_buffer_main.h"
#endif

int32_t vl53l5_host_algo_pipe(
	struct vl53l5_range_results_t                      *prng_dev,
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
	struct vl53l5_sharpener_target_data_t  *psharp_tgt_data,
#endif
	struct vl53l5_hap_cfg_dev_t              *pcfg_dev,
	struct vl53l5_hap_int_dev_t              *pint_dev,
	struct vl53l5_hap_op_dev_t               *pop_dev,
	struct vl53l5_hap_err_dev_t              *perr_dev)
{

	int32_t  status = 0;
	int32_t  pipe_disabled = 0;
	struct   common_grp__status_t *perr_status = &(perr_dev->hap_error_status);
	struct   common_grp__status_t *pwarn_status = &(perr_dev->hap_warning_status);

	vl53l5_hap_clear_data(
		perr_dev);

	switch (pcfg_dev->hap_general_cfg.hap__mode) {
	case VL53L5_HAP_MODE__DISABLED:
		pipe_disabled = 1;
		break;
	case VL53L5_HAP_MODE__ENABLED:
		pipe_disabled = 0;
		break;
	default:
		pipe_disabled = 0;
		perr_status->status__type = VL53L5_HAP_ERROR_INVALID_PARAMS;
		break;
	}

#ifdef VL53L5_ALGO_ANTI_FLICKER_FILTER_ON
	if (pop_dev->hap_state.initialised == 0U &&
		perr_status->status__type == 0 &&
		pipe_disabled == 0) {

		vl53l5_hap_update_aff_cfg(
			&(pcfg_dev->hap_general_cfg),
			prng_dev,
			&(pcfg_dev->aff_cfg_dev),
			perr_dev);
	}
#endif

#ifdef VL53L5_ALGO_ANTI_FLICKER_FILTER_ON
	if (pop_dev->hap_state.initialised == 0U &&
		perr_status->status__type == 0 &&
		pipe_disabled == 0) {

		vl53l5_anti_flicker_filter_init_start_rng(
			&(pcfg_dev->aff_cfg_dev),
			&(pint_dev->aff_int_dev),
			(struct vl53l5_aff_err_dev_t *)perr_dev);
	}
#endif

	if (perr_status->status__type == 0 &&
		pipe_disabled == 0)
		pop_dev->hap_state.initialised = 1U;

#ifdef VL53L5_ALGO_LONG_TAIL_FILTER_ON

	if (perr_status->status__type == 0 && pipe_disabled == 0) {
		vl53l5_long_tail_filter_main(
			&(pcfg_dev->ltf_cfg_dev),
			prng_dev,
			(struct vl53l5_ltf_err_dev_t *)perr_dev);
	}
#endif

#ifdef VL53L5_ALGO_GLASS_DETECTION_ON

	if (perr_status->status__type == 0 && pipe_disabled == 0) {
		vl53l5_glass_detection_main(
			prng_dev,
			&(pcfg_dev->gd_cfg_dev),
			&(pint_dev->gd_int_dev),
			&(pop_dev->rng_patch_dev),
			(struct vl53l5_gd_err_dev_t *)perr_dev);

		pop_dev->rng_patch_dev.gd_op_status.gd__op_spare_1 =
			(uint8_t)perr_dev->hap_warning_status.status__type;
	}
#else
	memset(&(pop_dev->rng_patch_dev.gd_op_status),
		   0,
		   sizeof(struct dci_grp__gd__op__status_t));
#endif

#ifdef VL53L5_ALGO_RANGE_ROTATION_ON

	if (perr_status->status__type == 0 && pipe_disabled == 0) {
		vl53l5_range_rotation_main(
			prng_dev,
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
			psharp_tgt_data,
#endif
			(&(pcfg_dev->rr_cfg_dev)),
			&(pint_dev->rr_int_dev),
			&(pint_dev->presults_tmp),
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
			(&(pint_dev->psharp_tmp)),
#endif
			(struct vl53l5_rr_err_dev_t *)perr_dev);
	}
#endif

#ifdef VL53L5_ALGO_ANTI_FLICKER_FILTER_ON

	if (perr_status->status__type == 0 && pipe_disabled == 0) {
		vl53l5_output_target_filter_main(
			VL53L5_OTF_INSTANCE__0,
			prng_dev,
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
			psharp_tgt_data,
#endif
			(&(pcfg_dev->otf_cfg_dev)),
			&(pint_dev->otf_int_dev),
			&(pop_dev->rng_patch_dev),
			(struct vl53l5_otf_err_dev_t *)perr_dev);
	}

	if (perr_status->status__type == 0 &&
		pipe_disabled == 0) {
		vl53l5_anti_flicker_filter_main(
			prng_dev,
			prng_dev,
			&(pcfg_dev->aff_cfg_dev),
			&(pint_dev->aff_int_dev),
			(struct vl53l5_aff_err_dev_t *)perr_dev);
	}
#endif

#ifdef VL53L5_ALGO_OUTPUT_TARGET_SORT_ON

	if (perr_status->status__type == 0 && pipe_disabled == 0) {
		vl53l5_output_target_sort_main(
			prng_dev,
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
			psharp_tgt_data,
#endif
			(&(pcfg_dev->ots_cfg_dev)),
			(struct vl53l5_ots_err_dev_t *)perr_dev);
	}
#endif

	if (perr_status->status__type == 0 && pipe_disabled == 0) {
		vl53l5_output_target_filter_main(
			VL53L5_OTF_INSTANCE__1,
			prng_dev,
#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
			psharp_tgt_data,
#endif
			(&(pcfg_dev->otf_cfg_dev)),
			&(pint_dev->otf_int_dev),
			&(pop_dev->rng_patch_dev),
			(struct vl53l5_otf_err_dev_t *)perr_dev);
	}

#ifdef VL53L5_ALGO_DEPTH16_ON

	if (perr_status->status__type == 0 &&
		pcfg_dev->d16_cfg_dev.d16_general_cfg.d16__cfg__mode !=
		VL53L5_D16_MODE__DISABLED &&
		pipe_disabled == 0) {
		vl53l5_depth16_main(
			prng_dev,
			&(pcfg_dev->d16_cfg_dev),
			&(pop_dev->rng_patch_dev),
			(struct vl53l5_d16_err_dev_t *)perr_dev);
	}
#endif

#ifdef VL53L5_ALGO_DEPTH16_BUFFER_ON

	if (perr_status->status__type == 0 &&
		pcfg_dev->d16_cfg_dev.d16_general_cfg.d16__buf__mode !=
		VL53L5_D16_BUF_MODE__DISABLED &&
		pipe_disabled == 0) {
		vl53l5_depth16_buffer_encode_main(
			&(pcfg_dev->d16_cfg_dev),
			prng_dev,
			&(pop_dev->rng_patch_dev),
			&(pop_dev->d16_buf_dev),
			(struct vl53l5_d16_err_dev_t *)perr_dev);
	}
#endif

	if (pipe_disabled == 0)
		pop_dev->hap_state.range_count += 1U;
	else
		pwarn_status->status__type = VL53L5_HAP_WARNING_DISABLED;

	status =
		vl53l5_hap_encode_status_code(
			perr_status);

	return status;
}
