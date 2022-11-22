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

#include "vl53l5_platform_algo_limits.h"
#include "vl53l5_utils_chk_structs_results.h"
#include "vl53l5_utils_csv_luts.h"
#include "vl53l5_utils_chk_common.h"

int32_t vl53l5_dci_buf_meta_data_check_values(
    char                                          *pprefix,
    struct  vl53l5_range_meta_data_t              *pactual,
    struct  vl53l5_range_meta_data_t              *pexpected,
    struct  vl53l5_chk__tolerance_t               *ptol)
{
    int32_t  error_count = 0;

    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "time_stamp",
            -1,
            (int32_t)pactual->time_stamp,
            (int32_t)pexpected->time_stamp,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "device_status",
            -1,
            (int32_t)pactual->device_status,
            (int32_t)pexpected->device_status,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "transaction_id",
            -1,
            (int32_t)pactual->transaction_id,
            (int32_t)pexpected->transaction_id,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "buffer_id",
            -1,
            (int32_t)pactual->buffer_id,
            (int32_t)pexpected->buffer_id,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "stream_count",
            -1,
            (int32_t)pactual->stream_count,
            (int32_t)pexpected->stream_count,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "silicon_temp_degc",
            -1,
            (int32_t)pactual->silicon_temp_degc,
            (int32_t)pexpected->silicon_temp_degc,
            ptol);

    return  error_count;
}

int32_t vl53l5_dci_rng_common_data_check_values(
    int32_t                                        csv_fmt_select,
    char                                          *pprefix,
    struct  vl53l5_range_common_data_t            *pactual,
    struct  vl53l5_range_common_data_t            *pexpected,
    struct  vl53l5_chk__tolerance_t               *ptol)
{
    int32_t  error_count = 0;

    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "wrap_dmax_mm",
            -1,
            (int32_t)pactual->wrap_dmax_mm,
            (int32_t)pexpected->wrap_dmax_mm,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "rng__no_of_zones",
            -1,
            (int32_t)pactual->rng__no_of_zones,
            (int32_t)pexpected->rng__no_of_zones,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "rng__max_targets_per_zone",
            -1,
            (int32_t)pactual->rng__max_targets_per_zone,
            (int32_t)pexpected->rng__max_targets_per_zone,
            ptol);
    if (csv_fmt_select >= VL53L5_CSV_FMT_SELECT__EXT30)
    {
        error_count +=
            vl53l5_utils_check_within_tol(
                pprefix,
                "rng__acc__no_of_zones",
                -1,
                (int32_t)pactual->rng__acc__no_of_zones,
                (int32_t)pexpected->rng__acc__no_of_zones,
                ptol);
        error_count +=
            vl53l5_utils_check_within_tol(
                pprefix,
                "rng__acc__zone_id",
                -1,
                (int32_t)pactual->rng__acc__zone_id,
                (int32_t)pexpected->rng__acc__zone_id,
                ptol);
        error_count +=
            vl53l5_utils_check_within_tol(
                pprefix,
                "rng__common__spare_0",
                -1,
                (int32_t)pactual->rng__common__spare_0,
                (int32_t)pexpected->rng__common__spare_0,
                ptol);
        error_count +=
            vl53l5_utils_check_within_tol(
                pprefix,
                "rng__common__spare_1",
                -1,
                (int32_t)pactual->rng__common__spare_1,
                (int32_t)pexpected->rng__common__spare_1,
                ptol);
    }

    return  error_count;
}

int32_t vl53l5_dci_rng_timing_data_check_values(
    char                                          *pprefix,
    struct  vl53l5_range_timing_data_t            *pactual,
    struct  vl53l5_range_timing_data_t            *pexpected,
    struct  vl53l5_chk__tolerance_t               *ptol)
{
    int32_t  error_count = 0;

    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "rng__integration_time_us",
            -1,
            (int32_t)pactual->rng__integration_time_us,
            (int32_t)pexpected->rng__integration_time_us,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "rng__total_periods_elapsed",
            -1,
            (int32_t)pactual->rng__total_periods_elapsed,
            (int32_t)pexpected->rng__total_periods_elapsed,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "rng__blanking_time_us",
            -1,
            (int32_t)pactual->rng__blanking_time_us,
            (int32_t)pexpected->rng__blanking_time_us,
            ptol);

    return  error_count;
}

int32_t vl53l5_dci_ref_channel_data_check_values(
    char                                          *pprefix,
    struct  vl53l5_ref_channel_data_t           *pactual,
    struct  vl53l5_ref_channel_data_t           *pexpected,
    struct  vl53l5_chk__tolerance_t               *ptol)
{
    int32_t  error_count = 0;

    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "amb_rate_kcps_per_spad",
            -1,
            (int32_t)pactual->amb_rate_kcps_per_spad,
            (int32_t)pexpected->amb_rate_kcps_per_spad,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "rng__effective_spad_count",
            -1,
            (int32_t)pactual->rng__effective_spad_count,
            (int32_t)pexpected->rng__effective_spad_count,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "amb_dmax_mm",
            -1,
            (int32_t)pactual->amb_dmax_mm,
            (int32_t)pexpected->amb_dmax_mm,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "silicon_temp_degc__start",
            -1,
            (int32_t)pactual->silicon_temp_degc__start,
            (int32_t)pexpected->silicon_temp_degc__start,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "silicon_temp_degc__end",
            -1,
            (int32_t)pactual->silicon_temp_degc__end,
            (int32_t)pexpected->silicon_temp_degc__end,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "rng__no_of_targets",
            -1,
            (int32_t)pactual->rng__no_of_targets,
            (int32_t)pexpected->rng__no_of_targets,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "rng__zone_id",
            -1,
            (int32_t)pactual->rng__zone_id,
            (int32_t)pexpected->rng__zone_id,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "rng__sequence_idx",
            -1,
            (int32_t)pactual->rng__sequence_idx,
            (int32_t)pexpected->rng__sequence_idx,
            ptol);

    return  error_count;
}

int32_t vl53l5_dci_ref_target_data_check_values(
    int32_t                                        csv_fmt_select,
    char                                          *pprefix,
    struct  vl53l5_ref_target_data_t            *pactual,
    struct  vl53l5_ref_target_data_t            *pexpected,
    struct  vl53l5_chk__tolerance_t               *ptol)
{
    int32_t  error_count = 0;

    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "peak_rate_kcps_per_spad",
            -1,
            (int32_t)pactual->peak_rate_kcps_per_spad,
            (int32_t)pexpected->peak_rate_kcps_per_spad,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "median_phase",
            -1,
            (int32_t)pactual->median_phase,
            (int32_t)pexpected->median_phase,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "rate_sigma_kcps_per_spad",
            -1,
            (int32_t)pactual->rate_sigma_kcps_per_spad,
            (int32_t)pexpected->rate_sigma_kcps_per_spad,
            ptol);
    if (csv_fmt_select >= VL53L5_CSV_FMT_SELECT__EXT30)
    {
        error_count +=
            vl53l5_utils_check_within_tol(
                pprefix,
                "target_zscore",
                -1,
                (int32_t)pactual->target_zscore,
                (int32_t)pexpected->target_zscore,
                ptol);
    }
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "range_sigma_mm",
            -1,
            (int32_t)pactual->range_sigma_mm,
            (int32_t)pexpected->range_sigma_mm,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "median_range_mm",
            -1,
            (int32_t)pactual->median_range_mm,
            (int32_t)pexpected->median_range_mm,
            ptol);
    if (csv_fmt_select >= VL53L5_CSV_FMT_SELECT__EXT30)
    {
        error_count +=
            vl53l5_utils_check_within_tol(
                pprefix,
                "start_range_mm",
                -1,
                (int32_t)pactual->start_range_mm,
                (int32_t)pexpected->start_range_mm,
                ptol);
        error_count +=
            vl53l5_utils_check_within_tol(
                pprefix,
                "end_range_mm",
                -1,
                (int32_t)pactual->end_range_mm,
                (int32_t)pexpected->end_range_mm,
                ptol);
    }
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "min_range_delta_mm",
            -1,
            (int32_t)pactual->min_range_delta_mm,
            (int32_t)pexpected->min_range_delta_mm,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "max_range_delta_mm",
            -1,
            (int32_t)pactual->max_range_delta_mm,
            (int32_t)pexpected->max_range_delta_mm,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "target_reflectance_est_pc",
            -1,
            (int32_t)pactual->target_reflectance_est_pc,
            (int32_t)pexpected->target_reflectance_est_pc,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "target_status",
            -1,
            (int32_t)pactual->target_status,
            (int32_t)pexpected->target_status,
            ptol);

    return  error_count;
}

int32_t vl53l5_dci_d16_ref_target_data_check_values(
    char                                          *pprefix,
    struct  dci_grp__d16__ref_target_data_t       *pactual,
    struct  dci_grp__d16__ref_target_data_t       *pexpected,
    struct  vl53l5_chk__tolerance_t               *ptol)
{
    int32_t  error_count = 0;

    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "depth16",
            -1,
            (int32_t)pactual->depth16,
            (int32_t)pexpected->depth16,
            ptol);

    return  error_count;
}

int32_t vl53l5_dci_dyn_xtalk_persistent_data_check_values(
    char                                          *pprefix,
    struct  vl53l5_dyn_xtalk_persistent_data_t  *pactual,
    struct  vl53l5_dyn_xtalk_persistent_data_t  *pexpected,
    struct  vl53l5_chk__tolerance_t               *ptol)
{
    int32_t  error_count = 0;

    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "dyn_xt__dyn_xtalk_grid_maximum_kcps_per_spad",
            -1,
            (int32_t)pactual->dyn_xt__dyn_xtalk_grid_maximum_kcps_per_spad,
            (int32_t)pexpected->dyn_xt__dyn_xtalk_grid_maximum_kcps_per_spad,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "dyn_xt__dyn_xtalk_grid_max_sigma_kcps_per_spad",
            -1,
            (int32_t)pactual->dyn_xt__dyn_xtalk_grid_max_sigma_kcps_per_spad,
            (int32_t)pexpected->dyn_xt__dyn_xtalk_grid_max_sigma_kcps_per_spad,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "dyn_xt__new_max_xtalk_sigma_kcps_per_spad",
            -1,
            (int32_t)pactual->dyn_xt__new_max_xtalk_sigma_kcps_per_spad,
            (int32_t)pexpected->dyn_xt__new_max_xtalk_sigma_kcps_per_spad,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "dyn_xt__calibration_gain",
            -1,
            (int32_t)pactual->dyn_xt__calibration_gain,
            (int32_t)pexpected->dyn_xt__calibration_gain,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "temp_comp__temp_gain",
            -1,
            (int32_t)pactual->temp_comp__temp_gain,
            (int32_t)pexpected->temp_comp__temp_gain,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "dyn_xt__nb_samples",
            -1,
            (int32_t)pactual->dyn_xt__nb_samples,
            (int32_t)pexpected->dyn_xt__nb_samples,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "dyn_xt__desired_samples",
            -1,
            (int32_t)pactual->dyn_xt__desired_samples,
            (int32_t)pexpected->dyn_xt__desired_samples,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "dyn_xt__accumulating",
            -1,
            (int32_t)pactual->dyn_xt__accumulating,
            (int32_t)pexpected->dyn_xt__accumulating,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "dyn_xt__grid_ready",
            -1,
            (int32_t)pactual->dyn_xt__grid_ready,
            (int32_t)pexpected->dyn_xt__grid_ready,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "dyn_xt__grid_ready_internal",
            -1,
            (int32_t)pactual->dyn_xt__grid_ready_internal,
            (int32_t)pexpected->dyn_xt__grid_ready_internal,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "dyn_xt__persist_spare_2",
            -1,
            (int32_t)pactual->dyn_xt__persist_spare_2,
            (int32_t)pexpected->dyn_xt__persist_spare_2,
            ptol);

    return  error_count;
}

int32_t vl53l5_dci_gd_op_data_check_values(
    char                                          *pprefix,
    struct  dci_grp__gd__op__data_t               *pactual,
    struct  dci_grp__gd__op__data_t               *pexpected,
    struct  vl53l5_chk__tolerance_t               *ptol)
{
    int32_t  error_count = 0;

    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__max__valid_rate_kcps_spad",
            -1,
            (int32_t)pactual->gd__max__valid_rate_kcps_spad,
            (int32_t)pexpected->gd__max__valid_rate_kcps_spad,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__min__valid_rate_kcps_spad",
            -1,
            (int32_t)pactual->gd__min__valid_rate_kcps_spad,
            (int32_t)pexpected->gd__min__valid_rate_kcps_spad,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__max__plane_rate_kcps_spad",
            -1,
            (int32_t)pactual->gd__max__plane_rate_kcps_spad,
            (int32_t)pexpected->gd__max__plane_rate_kcps_spad,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__min__plane_rate_kcps_spad",
            -1,
            (int32_t)pactual->gd__min__plane_rate_kcps_spad,
            (int32_t)pexpected->gd__min__plane_rate_kcps_spad,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__glass_display_case_dist_mm",
            -1,
            (int32_t)pactual->gd__glass_display_case_dist_mm,
            (int32_t)pexpected->gd__glass_display_case_dist_mm,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__glass_merged_dist_mm",
            -1,
            (int32_t)pactual->gd__glass_merged_dist_mm,
            (int32_t)pexpected->gd__glass_merged_dist_mm,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__mirror_dist_mm",
            -1,
            (int32_t)pactual->gd__mirror_dist_mm,
            (int32_t)pexpected->gd__mirror_dist_mm,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__target_behind_mirror_dist_mm",
            -1,
            (int32_t)pactual->gd__target_behind_mirror_dist_mm,
            (int32_t)pexpected->gd__target_behind_mirror_dist_mm,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__target_behind_mirror_min_dist_mm",
            -1,
            (int32_t)pactual->gd__target_behind_mirror_min_dist_mm,
            (int32_t)pexpected->gd__target_behind_mirror_min_dist_mm,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__target_behind_mirror_max_dist_mm",
            -1,
            (int32_t)pactual->gd__target_behind_mirror_max_dist_mm,
            (int32_t)pexpected->gd__target_behind_mirror_max_dist_mm,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__plane_xy_angle",
            -1,
            (int32_t)pactual->gd__plane_xy_angle,
            (int32_t)pexpected->gd__plane_xy_angle,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__plane_yz_angle",
            -1,
            (int32_t)pactual->gd__plane_yz_angle,
            (int32_t)pexpected->gd__plane_yz_angle,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__max_valid_rate_zone",
            -1,
            (int32_t)pactual->gd__max_valid_rate_zone,
            (int32_t)pexpected->gd__max_valid_rate_zone,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__max_valid_rate_zone_clipped",
            -1,
            (int32_t)pactual->gd__max_valid_rate_zone_clipped,
            (int32_t)pexpected->gd__max_valid_rate_zone_clipped,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__valid_zone_count",
            -1,
            (int32_t)pactual->gd__valid_zone_count,
            (int32_t)pexpected->gd__valid_zone_count,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__merged_zone_count",
            -1,
            (int32_t)pactual->gd__merged_zone_count,
            (int32_t)pexpected->gd__merged_zone_count,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__possible_merged_zone_count",
            -1,
            (int32_t)pactual->gd__possible_merged_zone_count,
            (int32_t)pexpected->gd__possible_merged_zone_count,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__possible_tgt2_behind_mirror_zone_count",
            -1,
            (int32_t)pactual->gd__possible_tgt2_behind_mirror_zone_count,
            (int32_t)pexpected->gd__possible_tgt2_behind_mirror_zone_count,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__blurred_zone_count",
            -1,
            (int32_t)pactual->gd__blurred_zone_count,
            (int32_t)pexpected->gd__blurred_zone_count,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__plane_zone_count",
            -1,
            (int32_t)pactual->gd__plane_zone_count,
            (int32_t)pexpected->gd__plane_zone_count,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__points_on_plane_pc",
            -1,
            (int32_t)pactual->gd__points_on_plane_pc,
            (int32_t)pexpected->gd__points_on_plane_pc,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__tgt2_behind_mirror_plane_zone_count",
            -1,
            (int32_t)pactual->gd__tgt2_behind_mirror_plane_zone_count,
            (int32_t)pexpected->gd__tgt2_behind_mirror_plane_zone_count,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__tgt2_behind_mirror_points_on_plane_pc",
            -1,
            (int32_t)pactual->gd__tgt2_behind_mirror_points_on_plane_pc,
            (int32_t)pexpected->gd__tgt2_behind_mirror_points_on_plane_pc,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__op_data_spare_0",
            -1,
            (int32_t)pactual->gd__op_data_spare_0,
            (int32_t)pexpected->gd__op_data_spare_0,
            ptol);

    return  error_count;
}

int32_t vl53l5_dci_gd_op_status_check_values(
    char                                          *pprefix,
    struct  dci_grp__gd__op__status_t             *pactual,
    struct  dci_grp__gd__op__status_t             *pexpected,
    struct  vl53l5_chk__tolerance_t               *ptol)
{
    int32_t  error_count = 0;

    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__rate_ratio",
            -1,
            (int32_t)pactual->gd__rate_ratio,
            (int32_t)pexpected->gd__rate_ratio,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__confidence",
            -1,
            (int32_t)pactual->gd__confidence,
            (int32_t)pexpected->gd__confidence,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__plane_detected",
            -1,
            (int32_t)pactual->gd__plane_detected,
            (int32_t)pexpected->gd__plane_detected,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__ratio_detected",
            -1,
            (int32_t)pactual->gd__ratio_detected,
            (int32_t)pexpected->gd__ratio_detected,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__glass_detected",
            -1,
            (int32_t)pactual->gd__glass_detected,
            (int32_t)pexpected->gd__glass_detected,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__tgt_behind_glass_detected",
            -1,
            (int32_t)pactual->gd__tgt_behind_glass_detected,
            (int32_t)pexpected->gd__tgt_behind_glass_detected,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__mirror_detected",
            -1,
            (int32_t)pactual->gd__mirror_detected,
            (int32_t)pexpected->gd__mirror_detected,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__op_spare_0",
            -1,
            (int32_t)pactual->gd__op_spare_0,
            (int32_t)pexpected->gd__op_spare_0,
            ptol);
    error_count +=
        vl53l5_utils_check_within_tol(
            pprefix,
            "gd__op_spare_1",
            -1,
            (int32_t)pactual->gd__op_spare_1,
            (int32_t)pexpected->gd__op_spare_1,
            ptol);

    return  error_count;
}
