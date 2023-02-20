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

#ifndef VL53L5_ANTI_FLICKER_FILTER_FUNCS_H_
#define VL53L5_ANTI_FLICKER_FILTER_FUNCS_H_

#include "vl53l5_types.h"
#include "vl53l5_results_config.h"
#include "common_datatype_structs.h"
#include "vl53l5_results_map.h"
#include "vl53l5_aff_cfg_map.h"
#include "vl53l5_aff_int_map.h"
#include "vl53l5_aff_err_map.h"
#include "vl53l5_aff_defs.h"
#include "vl53l5_aff_luts.h"
#include "vl53l5_aff_structs.h"
#include "vl53l5_anti_flicker_filter_defs.h"
#include "vl53l5_anti_flicker_filter_structs.h"

void vl53l5_aff_clear_data(
	struct vl53l5_aff_int_dev_t          *pint_dev,
	struct vl53l5_aff_err_dev_t          *perr_dev);

void vl53l5_aff_init_mem(
	struct vl53l5_aff_int_dev_t          *pint_dev,
	struct vl53l5_aff_cfg_dev_t          *pcfg_dev,
	struct vl53l5_aff_err_dev_t          *perr_dev);

struct vl53l5_aff_tgt_data_t *vl53l5_aff_cur_tgt_arr(
	struct vl53l5_aff_int_dev_t          *pint_dev);

struct vl53l5_aff_tgt_out_t *vl53l5_aff_out_cand_arr(
	struct vl53l5_aff_int_dev_t          *pint_dev);

struct vl53l5_aff_tgt_hist_t *vl53l5_aff_filled_tgt_hist_arr(
	struct vl53l5_aff_int_dev_t          *pint_dev);

int32_t *vl53l5_aff_affinity_matrix(
	struct vl53l5_aff_int_dev_t          *pint_dev);

int32_t *vl53l5_aff_cur_hist_asso(
	struct vl53l5_aff_int_dev_t          *pint_dev);

int32_t *vl53l5_aff_hist_cur_asso(
	struct vl53l5_aff_int_dev_t          *pint_dev);

struct vl53l5_aff_tgt_data_t *vl53l5_aff_passed_data(
	struct vl53l5_aff_int_dev_t   *pint_dev,
	struct vl53l5_aff_tgt_hist_t         *phist,
	int32_t                       time,
	int32_t                       iir_enabled
);

void vl53l5_aff_get_tgt_hist(
	struct vl53l5_aff_int_dev_t          *pint_dev,
	int32_t                              tgt_hist_id,
	int32_t                              zone_id,
	struct vl53l5_aff_tgt_hist_t                *ptgt_hist,
	struct vl53l5_aff_err_dev_t          *perr_dev
);

int32_t vl53l5_aff_dci_to_cur_tgt(
	struct vl53l5_range_results_t                  *pdci_dev,
	int32_t                              zone_id,
	struct vl53l5_aff_int_dev_t          *pint_dev,
	struct vl53l5_aff_err_dev_t          *perr_dev
);

void vl53l5_aff_match_targets(
	int32_t                        nb_cur_tgts,
	int32_t                        nb_hist_tgts,
	int32_t                        *pcur_hist_asso,
	int32_t                        *phist_cur_asso,
	struct vl53l5_aff_int_dev_t    *pint_dev,
	struct vl53l5_aff_err_dev_t    *perr_dev
);

void vl53l5_aff_comp_aff_matrix(
	int32_t                        nb_current_targets,
	int32_t                        nb_history_targets,
	int32_t                        iir_enabled,
	struct vl53l5_aff_cfg_dev_t    *pcfg_dev,
	struct vl53l5_aff_int_dev_t    *pint_dev,
	struct vl53l5_aff_err_dev_t    *perr_dev
);

int32_t vl53l5_aff_lzc_uint64(uint64_t x);

int32_t vl53l5_aff_lzc_int64(int64_t x);

int32_t vl53l5_aff_tweak_mirroring_calc(
	int32_t                        candidate_zscore,
	struct vl53l5_aff_tgt_hist_t          *ptgt_hist,
	struct vl53l5_aff_cfg_dev_t    *pcfg_dev,
	struct vl53l5_aff_int_dev_t    *pint_dev,
	struct vl53l5_aff_err_dev_t    *perr_dev
);

void vl53l5_aff_avg_tgt_hist(
	struct vl53l5_aff_tgt_data_t          *pcur_tgt,
	struct vl53l5_aff_tgt_hist_t          *ptgt_hist,
	struct vl53l5_aff_cfg_dev_t    *pcfg_dev,
	struct vl53l5_aff_int_dev_t    *pint_dev,
	struct vl53l5_aff_tgt_out_t           *pavg_tgt,
	struct vl53l5_aff_err_dev_t    *perr_dev
);

void vl53l5_aff_temporal_shift(
	struct vl53l5_aff_tgt_data_t          *pcur_tgt,
	struct vl53l5_aff_tgt_out_t           *pout_tgt,
	int32_t                        iir_enabled,
	struct vl53l5_aff_tgt_hist_t          *ptgt_hist,
	struct vl53l5_aff_cfg_dev_t    *pcfg_dev,
	struct vl53l5_aff_int_dev_t    *pint_dev,
	struct vl53l5_aff_err_dev_t    *perr_dev __attribute__((unused))
);

void vl53l5_aff_copy_data(
	struct vl53l5_aff_tgt_data_t          *psrc,
	struct vl53l5_aff_tgt_data_t          *pdest,
	struct vl53l5_aff_err_dev_t    *perr_dev __attribute__((unused))
);

struct vl53l5_aff_tgt_hist_t   *vl53l5_aff_tgt_to_mem(
	struct vl53l5_aff_tgt_data_t          *ptgt,
	struct vl53l5_aff_tgt_out_t           *pout_tgt,
	int32_t                        zscore,
	int32_t                        iir_enabled,
	struct vl53l5_aff_int_dev_t    *pint_dev,
	struct vl53l5_aff_err_dev_t    *perr_dev
);

void vl53l5_aff_cand_to_dci(
	int32_t                       zone_id,
	int32_t                       nb_cand,
	struct vl53l5_aff_cfg_dev_t   *pcfg_dev,
	struct vl53l5_aff_int_dev_t   *pint_dev,
	struct vl53l5_range_results_t           *pdci_dev,
	struct vl53l5_aff_err_dev_t   *perr_dev __attribute__((unused))
);

void vl53l5_aff_update_temp_idx(
	struct vl53l5_aff_int_dev_t   *pint_dev,
	int32_t                       iir_enabled
);

int32_t *vl53l5_aff_get_cur_pref(
	struct vl53l5_aff_int_dev_t          *pint_dev);

int32_t *vl53l5_aff_get_hist_pref(
	struct vl53l5_aff_int_dev_t          *pint_dev);

void vl53l5_aff_comp_cur_pref(
	int32_t                        nb_cur_tgts,
	int32_t                        nb_hist_tgts,
	struct vl53l5_aff_int_dev_t    *pint_dev,
	struct vl53l5_aff_err_dev_t    *perr_dev __attribute__((unused)));

void vl53l5_aff_comp_hist_pref(
	int32_t                        nb_cur_tgts,
	int32_t                        nb_hist_tgts,
	struct vl53l5_aff_int_dev_t    *pint_dev,
	struct vl53l5_aff_err_dev_t    *perr_dev  __attribute__((unused)));

int32_t *vl53l5_aff_get_ind_to_be_asso(
	struct vl53l5_aff_int_dev_t          *pint_dev);

void vl53l5_aff_out_to_data(
	struct vl53l5_aff_tgt_out_t           *pout_src,
	struct vl53l5_aff_tgt_data_t          *pdat_dst,
	struct vl53l5_aff_err_dev_t    *perr_dev __attribute__((unused))
);

#endif
