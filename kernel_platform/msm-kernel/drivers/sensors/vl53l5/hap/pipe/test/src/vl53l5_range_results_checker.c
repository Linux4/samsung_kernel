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

#include "vl53l5_utils_csv_luts.h"
#include "vl53l5_range_results_checker.h"
#include "vl53l5_utils_chk_common.h"
#include "vl53l5_utils_chk_structs_results.h"

int32_t vl53l5_range_dev_check_values_zone(
    int32_t                                         idx,
    struct  vl53l5_range_per_zone_results_t           *pactual,
    struct  vl53l5_range_per_zone_results_t           *pexpected,
    struct  vl53l5_chk_tol_rng_op_dev_t            *ptol)
{
    int32_t  error_count = 0;

    #ifdef VL53L5_AMB_RATE_KCPS_PER_SPAD_ON
    error_count +=
        vl53l5_utils_check_within_tol(
            "rng_per_zone_data.",
            "amb_rate_kcps_per_spad",
            idx,
            (int32_t)pactual->amb_rate_kcps_per_spad[idx],
            (int32_t)pexpected->amb_rate_kcps_per_spad[idx],
            &(ptol->tol__amb_rate_kcps_per_spad));
    #endif

    #ifdef VL53L5_EFFECTIVE_SPAD_COUNT_ON
    error_count +=
        vl53l5_utils_check_within_tol(
            "rng_per_zone_data.",
            "rng__effective_spad_count",
            idx,
            (int32_t)pactual->rng__effective_spad_count[idx],
            (int32_t)pexpected->rng__effective_spad_count[idx],
            &(ptol->tol__rng__effective_spad_count));
    #endif

    #ifdef VL53L5_AMB_DMAX_MM_ON
    error_count +=
        vl53l5_utils_check_within_tol(
            "rng_per_zone_data.",
            "amb_dmax_mm",
            idx,
            (int32_t)pactual->amb_dmax_mm[idx],
            (int32_t)pexpected->amb_dmax_mm[idx],
            &(ptol->tol__amb_dmax_mm));
    #endif

    #ifdef VL53L5_SILICON_TEMP_DEGC_START_ON
    error_count +=
        vl53l5_utils_check_within_tol(
            "rng_per_zone_data.",
            "silicon_temp_degc__start",
            idx,
            (int32_t)pactual->silicon_temp_degc__start[idx],
            (int32_t)pexpected->silicon_temp_degc__start[idx],
            &(ptol->tol__silicon_temp_degc__start));
    #endif

    #ifdef VL53L5_SILICON_TEMP_DEGC_END_ON
    error_count +=
        vl53l5_utils_check_within_tol(
            "rng_per_zone_data.",
            "silicon_temp_degc__end",
            idx,
            (int32_t)pactual->silicon_temp_degc__end[idx],
            (int32_t)pexpected->silicon_temp_degc__end[idx],
            &(ptol->tol__silicon_temp_degc__end));
    #endif

    #ifdef VL53L5_NO_OF_TARGETS_ON
    error_count +=
        vl53l5_utils_check_within_tol(
            "rng_per_zone_data.",
            "rng__no_of_targets",
            idx,
            (int32_t)pactual->rng__no_of_targets[idx],
            (int32_t)pexpected->rng__no_of_targets[idx],
            &(ptol->tol__rng__no_of_targets));
    #endif

    #ifdef VL53L5_ZONE_ID_ON
    error_count +=
        vl53l5_utils_check_within_tol(
            "rng_per_zone_data.",
            "rng__zone_id",
            idx,
            (int32_t)pactual->rng__zone_id[idx],
            (int32_t)pexpected->rng__zone_id[idx],
            &(ptol->tol__rng__zone_id));
    #endif

    #ifdef VL53L5_SEQUENCE_IDX_ON
    error_count +=
        vl53l5_utils_check_within_tol(
            "rng_per_zone_data.",
            "rng__sequence_idx",
            idx,
            (int32_t)pactual->rng__sequence_idx[idx],
            (int32_t)pexpected->rng__sequence_idx[idx],
            &(ptol->tol__rng__sequence_idx));
    #endif

    return error_count;
}

int32_t vl53l5_range_dev_check_values_target(
    int32_t                                         idx,
    int32_t                                         csv_fmt_select,
    struct  vl53l5_range_per_tgt_results_t         *pactual,
    struct  vl53l5_range_per_tgt_results_t         *pexpected,
    struct  vl53l5_chk_tol_rng_op_dev_t            *ptol)
{

    int32_t  error_count = 0;

    #ifdef VL53L5_PEAK_RATE_KCPS_PER_SPAD_ON
    error_count +=
        vl53l5_utils_check_within_tol(
            "rng_per_target_data.",
            "peak_rate_kcps_per_spad",
            idx,
            (int32_t)pactual->peak_rate_kcps_per_spad[idx],
            (int32_t)pexpected->peak_rate_kcps_per_spad[idx],
            &(ptol->tol__peak_rate_kcps_per_spad));
    #endif

    #ifdef VL53L5_MEDIAN_PHASE_ON
    error_count +=
        vl53l5_utils_check_within_tol(
            "rng_per_target_data.",
            "median_phase",
            idx,
            (int32_t)pactual->median_phase[idx],
            (int32_t)pexpected->median_phase[idx],
            &(ptol->tol__median_phase));
    #endif

    #ifdef VL53L5_RATE_SIGMA_KCPS_PER_SPAD_ON
    error_count +=
        vl53l5_utils_check_within_tol(
            "rng_per_target_data.",
            "rate_sigma_kcps_per_spad",
            idx,
            (int32_t)pactual->rate_sigma_kcps_per_spad[idx],
            (int32_t)pexpected->rate_sigma_kcps_per_spad[idx],
            &(ptol->tol__rate_sigma_kcps_per_spad));
    #endif

    #ifdef VL53L5_TARGET_ZSCORE_ON
    if (csv_fmt_select >= VL53L5_CSV_FMT_SELECT__EXT30)
    {
        error_count +=
            vl53l5_utils_check_within_tol(
                "rng_per_target_data.",
                "target_zscore",
                idx,
                (int32_t)pactual->target_zscore[idx],
                (int32_t)pexpected->target_zscore[idx],
                &(ptol->tol__target_zscore));
    }
    #endif

    #ifdef VL53L5_RANGE_SIGMA_MM_ON
    error_count +=
        vl53l5_utils_check_within_tol(
            "rng_per_target_data.",
            "range_sigma_mm",
            idx,
            (int32_t)pactual->range_sigma_mm[idx],
            (int32_t)pexpected->range_sigma_mm[idx],
            &(ptol->tol__range_sigma_mm));
    #endif

    #ifdef VL53L5_MEDIAN_RANGE_MM_ON
    error_count +=
        vl53l5_utils_check_within_tol(
            "rng_per_target_data.",
            "median_range_mm",
            idx,
            (int32_t)pactual->median_range_mm[idx],
            (int32_t)pexpected->median_range_mm[idx],
            &(ptol->tol__median_range_mm));
    #endif

    #ifdef VL53L5_START_RANGE_MM_ON
    if (csv_fmt_select >= VL53L5_CSV_FMT_SELECT__EXT30)
    {
        error_count +=
            vl53l5_utils_check_within_tol(
                "rng_per_target_data.",
                "start_range_mm",
                idx,
                (int32_t)pactual->start_range_mm[idx],
                (int32_t)pexpected->start_range_mm[idx],
                &(ptol->tol__start_range_mm));
    }
    #endif

    #ifdef VL53L5_END_RANGE_MM_ON
    if (csv_fmt_select >= VL53L5_CSV_FMT_SELECT__EXT30)
    {
        error_count +=
            vl53l5_utils_check_within_tol(
                "rng_per_target_data.",
                "end_range_mm",
                idx,
                (int32_t)pactual->end_range_mm[idx],
                (int32_t)pexpected->end_range_mm[idx],
                &(ptol->tol__end_range_mm));
    }
    #endif
    #ifdef VL53L5_MIN_RANGE_DELTA_MM_ON
    error_count +=
        vl53l5_utils_check_within_tol(
            "rng_per_target_data.",
            "min_range_delta_mm",
            idx,
            (int32_t)pactual->min_range_delta_mm[idx],
            (int32_t)pexpected->min_range_delta_mm[idx],
            &(ptol->tol__min_range_delta_mm));
    #endif

    #ifdef VL53L5_MAX_RANGE_DELTA_MM_ON
    error_count +=
        vl53l5_utils_check_within_tol(
            "rng_per_target_data.",
            "max_range_delta_mm",
            idx,
            (int32_t)pactual->max_range_delta_mm[idx],
            (int32_t)pexpected->max_range_delta_mm[idx],
            &(ptol->tol__max_range_delta_mm));
    #endif

    #ifdef VL53L5_TARGET_REFLECTANCE_EST_PC_ON
    error_count +=
        vl53l5_utils_check_within_tol(
            "rng_per_target_data.",
            "target_reflectance_est_pc",
            idx,
            (int32_t)pactual->target_reflectance_est_pc[idx],
            (int32_t)pexpected->target_reflectance_est_pc[idx],
            &(ptol->tol__target_reflectance_est_pc));
    #endif

    #ifdef VL53L5_TARGET_STATUS_ON
    error_count +=
        vl53l5_utils_check_within_tol(
            "rng_per_target_data.",
            "target_status",
            idx,
            (int32_t)pactual->target_status[idx],
            (int32_t)pexpected->target_status[idx],
            &(ptol->tol__target_status));
    #endif

    return error_count;

}

int32_t vl53l5_range_dev_check_values(
    int32_t                                csv_fmt_select,
    struct  vl53l5_range_results_t                  *pactual,
    struct  vl53l5_range_results_t                  *pexpected,
    struct  vl53l5_chk_tol_rng_op_dev_t   *ptol)
{
    int32_t  error_count = 0;
    int32_t  max_targets_per_zone =
        (int32_t)pexpected->common_data.rng__max_targets_per_zone;
    int32_t  max_zones =
        (int32_t)pexpected->common_data.rng__no_of_zones +
        (int32_t)pexpected->common_data.rng__acc__no_of_zones;
    int32_t  no_of_targets = 0;

    int32_t  z   = 0;
    int32_t  t   = 0;
    int32_t  i   = 0;

    if (max_zones > (int32_t)VL53L5_MAX_ZONES)
    {
        max_zones = (int32_t)VL53L5_MAX_ZONES;
    }

    #ifdef VL53L5_META_DATA_ON
    error_count +=
        vl53l5_dci_buf_meta_data_check_values(
            "rng_meta_data.",
            &(pactual->meta_data),
            &(pexpected->meta_data),
            &(ptol->tol__rng_meta_data));
    #endif

    #ifdef VL53L5_COMMON_DATA_ON
    error_count +=
        vl53l5_dci_rng_common_data_check_values(
            csv_fmt_select,
            "rng_common_data.",
            &(pactual->common_data),
            &(pexpected->common_data),
            &(ptol->tol__rng_common_data));
    #endif

    #ifdef VL53L5_RNG_TIMING_DATA_ON
    error_count +=
        vl53l5_dci_rng_timing_data_check_values(
            "rng_timing_data.",
            &(pactual->rng_timing_data),
            &(pexpected->rng_timing_data),
            &(ptol->tol__rng_timing_data));
    #endif

    for (z = 0 ; z < max_zones ; z++)
    {
        error_count +=
            vl53l5_range_dev_check_values_zone(
                z,
                &(pactual->per_zone_results),
                &(pexpected->per_zone_results),
                ptol);
    }

    for (z = 0 ; z < max_zones ; z++)
    {
        no_of_targets =
            (int32_t)pexpected->per_zone_results.rng__no_of_targets[z];

        for (t = 0 ; t < no_of_targets ; t++)
        {
            i = (z * max_targets_per_zone) + t;

            error_count +=
                vl53l5_range_dev_check_values_target(
                    i,
                    csv_fmt_select,
                    &(pactual->per_tgt_results),
                    &(pexpected->per_tgt_results),
                    ptol);
        }
    }

    #ifdef VL53L5_REF_TIMING_DATA_ON
    error_count +=
        vl53l5_dci_rng_timing_data_check_values(
            "ref_timing_data.",
            &(pactual->ref_timing_data),
            &(pexpected->ref_timing_data),
            &(ptol->tol__ref_timing_data));
    #endif

    #ifdef VL53L5_REF_CHANNEL_DATA_ON
    error_count +=
        vl53l5_dci_ref_channel_data_check_values(
            "ref_channel_data.",
            &(pactual->ref_channel_data),
            &(pexpected->ref_channel_data),
            &(ptol->tol__ref_channel_data));
    #endif

    #ifdef VL53L5_REF_TARGET_DATA_ON
    error_count +=
        vl53l5_dci_ref_target_data_check_values(
            csv_fmt_select,
            "ref_target_data.",
            &(pactual->ref_target_data),
            &(pexpected->ref_target_data),
            &(ptol->tol__ref_target_data));
    #endif

    return  error_count;
}

int32_t vl53l5_patch_range_dev_check_values(
    int32_t                                     csv_fmt_select,
    struct  dci_patch_op_dev_t                 *pactual,
    struct  dci_patch_op_dev_t                 *pexpected,
    struct  vl53l5_chk_tol_rng_patch_op_dev_t  *ptol)
{
    int32_t  error_count = 0;

    if (csv_fmt_select >= VL53L5_CSV_FMT_SELECT__EXT32_HAP)
    {
        error_count +=
            vl53l5_dci_gd_op_data_check_values(
                "gd_op_data.",
                &(pactual->gd_op_data),
                &(pexpected->gd_op_data),
                &(ptol->tol__gd_op_data));

        error_count +=
            vl53l5_dci_gd_op_status_check_values(
                "gd_op_status.",
                &(pactual->gd_op_status),
                &(pexpected->gd_op_status),
                &(ptol->tol__gd_op_status));

        error_count +=
            vl53l5_utils_check_array_uint16(
                "d16_per_target_data.",
                "depth16",
                VL53L5_MAX_TARGETS,
                &(pactual->d16_per_target_data.depth16[0]),
                &(pexpected->d16_per_target_data.depth16[0]),
                &(ptol->tol__depth16));
    }

    return  error_count;
}

#ifdef VL53L5_SHARPENER_TARGET_DATA_ON
int32_t vl53l5_dci_sharpener_target_data_check_values(
    int32_t                                     csv_fmt_select,
    char                                       *pprefix,
    struct  vl53l5_sharpener_target_data_t   *pactual,
    struct  vl53l5_sharpener_target_data_t   *pexpected,
    struct  vl53l5_chk_tol_sharp_op_dev_t      *ptol)
{
    int32_t  error_count = 0;

    #ifdef VL53L5_SHARPENER_GROUP_INDEX_ON
    if (csv_fmt_select >= VL53L5_CSV_FMT_SELECT__EXT30)
    {
        error_count +=
            vl53l5_utils_check_array_uint8(
                pprefix,
                "sharpener__group_index",
                VL53L5_MAX_TARGETS,
                &(pactual->sharpener__group_index[0]),
                &(pexpected->sharpener__group_index[0]),
                &(ptol->tol__sharpener__group_index));
    }
    #endif

    #ifdef VL53L5_SHARPENER_CONFIDENCE_ON
    if (csv_fmt_select >= VL53L5_CSV_FMT_SELECT__EXT30)
    {
        error_count +=
            vl53l5_utils_check_array_uint8(
                pprefix,
                "sharpener__confidence",
                VL53L5_MAX_TARGETS,
                &(pactual->sharpener__confidence[0]),
                &(pexpected->sharpener__confidence[0]),
                &(ptol->tol__sharpener__confidence));
    }
    #endif

    return  error_count;
}
#endif
