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

#include "test_utils.h"
#include "vl53l5_algo_build_config.h"

#include "vl53l5_test_logging_setup.h"

void test_common_version_write(
    char                          *plabel,
    struct common_grp__version_t  *pdata)
{
    test_print(
        "%-32s: R_%d.%d.%d.%d\n",
        plabel,
        pdata->version__major,
        pdata->version__minor,
        pdata->version__build,
        pdata->version__revision);
}

void test_pipe_and_algo_versions_write(
    struct vl53l5_hap_ver_dev_t   *pdev)
{

    test_common_version_write("hap_version",     &(pdev->pipe_version));

    #ifdef VL53L5_ALGO_LONG_TAIL_FILTER_ON
    test_common_version_write("ltf_version",     &(pdev->ltf_version));
    #endif
    #ifdef VL53L5_ALGO_GLASS_DETECTION_ON
    test_common_version_write("gd_version",      &(pdev->gd_version));
    #endif
    #ifdef VL53L5_ALGO_RANGE_ROTATION_ON
    test_common_version_write("rr_version",      &(pdev->rr_version));
    #endif
    #ifdef VL53L5_ALGO_ANTI_FLICKER_FILTER_ON
    test_common_version_write("aff_version",     &(pdev->aff_version));
    #endif
    #ifdef VL53L5_ALGO_OUTPUT_TARGET_SORT_ON
    test_common_version_write("ots_version",     &(pdev->ots_version));
    #endif
    test_common_version_write("otf_version",     &(pdev->otf_version));
    #ifdef VL53L5_ALGO_DEPTH16_ON
    test_common_version_write("d16_version",     &(pdev->d16_version));
    #endif
    #ifdef VL53L5_ALGO_DEPTH16_BUFFER_ON
    test_common_version_write("d16_buf_version", &(pdev->d16_buf_version));
    #endif

}

void test_common_status_write_json(
    struct common_grp__status_t  *pdata)
{
    test_print("{\n");
    test_print("\t\"status__group\":    %d,\n", pdata->status__group);
    test_print("\t\"status__type\":     %d,\n", pdata->status__type);
    test_print("\t\"status__stage_id\": %d,\n", pdata->status__stage_id);
    test_print("\t\"status__debug_0\":  %u,\n", pdata->status__debug_0);
    test_print("\t\"status__debug_1\":  %u,\n", pdata->status__debug_1);
    test_print("\t\"status__debug_2\":  %u\n",  pdata->status__debug_2);
    test_print("}");
}

void test_common_status_print_json(
    struct common_grp__status_t  *pdata)
{
    test_info("{\n");
    test_info("\t\"status__group\":    %d,\n", pdata->status__group);
    test_info("\t\"status__type\":     %d,\n", pdata->status__type);
    test_info("\t\"status__stage_id\": %d,\n", pdata->status__stage_id);
    test_info("\t\"status__debug_0\":  %u,\n", pdata->status__debug_0);
    test_info("\t\"status__debug_1\":  %u,\n", pdata->status__debug_1);
    test_info("\t\"status__debug_2\":  %u\n",  pdata->status__debug_2);
    test_info("}");
}

int32_t test_hap_tuning_read_fields(
    int32_t                       *pfields,
    int32_t                        first_col,
    struct   vl53l5_hap_tuning_t  *pdata)
{
    int32_t next_col = first_col;

    pdata->hap__mode = (uint32_t) * (pfields + next_col);
    next_col++;
    pdata->ltf_cfg_preset = (uint32_t) * (pfields + next_col);
    next_col++;
    pdata->gd_cfg_preset = (uint32_t) * (pfields + next_col);
    next_col++;
    pdata->rr_cfg_preset = (uint32_t) * (pfields + next_col);
    next_col++;
    pdata->rr_rotation = (uint32_t) * (pfields + next_col);
    next_col++;
    pdata->aff_cfg_preset = (uint32_t) * (pfields + next_col);
    next_col++;
    pdata->aff_processing_mode = (uint32_t) * (pfields + next_col);
    next_col++;
    pdata->ots_cfg_preset = (uint32_t) * (pfields + next_col);
    next_col++;
    pdata->otf_cfg_preset_0 = (uint32_t) * (pfields + next_col);
    next_col++;
    pdata->otf_cfg_preset_1 = (uint32_t) * (pfields + next_col);
    next_col++;
    pdata->otf_range_clip_en_0 = (uint32_t) * (pfields + next_col);
    next_col++;
    pdata->otf_range_clip_en_1 = (uint32_t) * (pfields + next_col);
    next_col++;
    pdata->otf_max_targets_per_zone_0 = (uint32_t) * (pfields + next_col);
    next_col++;
    pdata->otf_max_targets_per_zone_1 = (uint32_t) * (pfields + next_col);
    next_col++;
    pdata->d16_cfg_preset = (uint32_t) * (pfields + next_col);
    next_col++;

    return next_col;
}

void test_hap_tuning_write_json(
    struct   vl53l5_hap_tuning_t  *pdata)
{
    test_print("{\n");
    test_print("\t\"hap__mode\":                  %u,\n", pdata->hap__mode);
    test_print("\t\"ltf_cfg_preset\":             %u,\n", pdata->ltf_cfg_preset);
    test_print("\t\"gd_cfg_preset\":              %u,\n", pdata->gd_cfg_preset);
    test_print("\t\"rr_cfg_preset\":              %u,\n", pdata->rr_cfg_preset);
    test_print("\t\"rr_rotation\":                %u,\n", pdata->rr_rotation);
    test_print("\t\"aff_cfg_preset\":             %u,\n", pdata->aff_cfg_preset);
    test_print("\t\"aff_processing_mode\":        %u,\n", pdata->aff_processing_mode);
    test_print("\t\"ots_cfg_preset\":             %u,\n", pdata->ots_cfg_preset);
    test_print("\t\"otf_cfg_preset_0\":           %u,\n", pdata->otf_cfg_preset_0);
    test_print("\t\"otf_cfg_preset_1\":           %u,\n", pdata->otf_cfg_preset_1);
    test_print("\t\"otf_range_clip_en_0\":        %u,\n", pdata->otf_range_clip_en_0);
    test_print("\t\"otf_range_clip_en_1\":        %u,\n", pdata->otf_range_clip_en_1);
    test_print("\t\"otf_max_targets_per_zone_0\": %u,\n", pdata->otf_max_targets_per_zone_0);
    test_print("\t\"otf_max_targets_per_zone_1\": %u,\n", pdata->otf_max_targets_per_zone_1);
    test_print("\t\"d16_cfg_preset\":             %u\n",  pdata->d16_cfg_preset);
    test_print("}");
}

void test_hap_tuning_print_json(
    struct   vl53l5_hap_tuning_t  *pdata)
{
    test_info("{\n");
    test_info("\t\"hap__mode\":                  %u,\n", pdata->hap__mode);
    test_info("\t\"ltf_cfg_preset\":             %u,\n", pdata->ltf_cfg_preset);
    test_info("\t\"gd_cfg_preset\":              %u,\n", pdata->gd_cfg_preset);
    test_info("\t\"rr_cfg_preset\":              %u,\n", pdata->rr_cfg_preset);
    test_info("\t\"rr_rotation\":                %u,\n", pdata->rr_rotation);
    test_info("\t\"aff_cfg_preset\":             %u,\n", pdata->aff_cfg_preset);
    test_info("\t\"aff_processing_mode\":        %u,\n", pdata->aff_processing_mode);
    test_info("\t\"ots_cfg_preset\":             %u,\n", pdata->ots_cfg_preset);
    test_info("\t\"otf_cfg_preset_0\":           %u,\n", pdata->otf_cfg_preset_0);
    test_info("\t\"otf_cfg_preset_1\":           %u,\n", pdata->otf_cfg_preset_1);
    test_info("\t\"otf_range_clip_en_0\":        %u,\n", pdata->otf_range_clip_en_0);
    test_info("\t\"otf_range_clip_en_1\":        %u,\n", pdata->otf_range_clip_en_1);
    test_info("\t\"otf_max_targets_per_zone_0\": %u,\n", pdata->otf_max_targets_per_zone_0);
    test_info("\t\"otf_max_targets_per_zone_1\": %u,\n", pdata->otf_max_targets_per_zone_1);
    test_info("\t\"d16_cfg_preset\":             %u\n",  pdata->d16_cfg_preset);
    test_info("}");
}

int32_t test_data_read_fields(
    int32_t                                 *pfields,
    int32_t                                 *psample_no,
    struct  vl53l5_range_results_t                    *prng_dev,
    struct  dci_patch_op_dev_t              *prng_patch_dev)
{
    int32_t  last_target          = *pfields;
    int32_t  max_targets_per_zone = *(pfields + 6);

    int32_t  z = *(pfields + 1);
    int32_t  t = *(pfields + 22);
    int32_t  i = 0;

    i = (z * max_targets_per_zone) + t;

    *psample_no = (int32_t) * (pfields + 2);
    #ifdef VL53L5_COMMON_DATA_ON
    prng_dev->meta_data.stream_count = (uint8_t) * (pfields + 3);
    #endif

    #ifdef VL53L5_COMMON_DATA_ON
    prng_dev->common_data.wrap_dmax_mm              = (uint16_t) * (pfields + 4);
    prng_dev->common_data.rng__no_of_zones          = (uint8_t) * (pfields + 5);
    prng_dev->common_data.rng__max_targets_per_zone = (uint8_t) * (pfields + 6);
    prng_dev->common_data.rng__acc__no_of_zones     = (uint8_t) * (pfields + 7);
    prng_dev->common_data.rng__acc__zone_id         = (uint8_t) * (pfields + 8);
    prng_dev->common_data.rng__common__spare_0      = (uint8_t) * (pfields + 9);
    prng_dev->common_data.rng__common__spare_1      = (uint8_t) * (pfields + 10);
    #endif

    #ifdef VL53L5_RNG_TIMING_DATA_ON
    prng_dev->rng_timing_data.rng__integration_time_us   = (uint32_t) * (pfields + 11);
    prng_dev->rng_timing_data.rng__total_periods_elapsed = (uint32_t) * (pfields + 12);
    prng_dev->rng_timing_data.rng__blanking_time_us      = (uint32_t) * (pfields + 13);
    #endif

    #ifdef VL53L5_AMB_RATE_KCPS_PER_SPAD_ON
    prng_dev->per_zone_results.amb_rate_kcps_per_spad[z]    = (uint32_t) * (pfields + 14);
    #endif
    #ifdef VL53L5_EFFECTIVE_SPAD_COUNT_ON
    prng_dev->per_zone_results.rng__effective_spad_count[z] = (uint32_t) * (pfields + 15);
    #endif
    #ifdef VL53L5_AMB_DMAX_MM_ON
    prng_dev->per_zone_results.amb_dmax_mm[z]               = (uint16_t) * (pfields + 16);
    #endif
    #ifdef VL53L5_SILICON_TEMP_DEGC_START_ON
    prng_dev->per_zone_results.silicon_temp_degc__start[z]  = (int8_t) * (pfields + 17);
    #endif
    #ifdef VL53L5_SILICON_TEMP_DEGC_END_ON
    prng_dev->per_zone_results.silicon_temp_degc__end[z]    = (int8_t) * (pfields + 18);
    #endif
    #ifdef VL53L5_NO_OF_TARGETS_ON
    prng_dev->per_zone_results.rng__no_of_targets[z]        = (uint8_t) * (pfields + 19);
    #endif
    #ifdef VL53L5_ZONE_ID_ON
    prng_dev->per_zone_results.rng__zone_id[z]              = (uint8_t) * (pfields + 20);
    #endif
    #ifdef VL53L5_SEQUENCE_IDX_ON
    prng_dev->per_zone_results.rng__sequence_idx[z]         = (uint8_t) * (pfields + 21);
    #endif

    #ifdef VL53L5_PEAK_RATE_KCPS_PER_SPAD_ON
    prng_dev->per_tgt_results.peak_rate_kcps_per_spad[i]   = (uint32_t) * (pfields + 23);
    #endif
    #ifdef VL53L5_MEDIAN_PHASE_ON
    prng_dev->per_tgt_results.median_phase[i]              = (uint32_t) * (pfields + 24);
    #endif
    #ifdef VL53L5_RATE_SIGMA_KCPS_PER_SPAD_ON
    prng_dev->per_tgt_results.rate_sigma_kcps_per_spad[i]  = (uint32_t) * (pfields + 25);
    #endif
    #ifdef VL53L5_TARGET_ZSCORE_ON
    prng_dev->per_tgt_results.target_zscore[i]             = (uint16_t) * (pfields + 26);
    #endif
    #ifdef VL53L5_RANGE_SIGMA_MM_ON
    prng_dev->per_tgt_results.range_sigma_mm[i]            = (uint16_t) * (pfields + 27);
    #endif
    #ifdef VL53L5_MEDIAN_RANGE_MM_ON
    prng_dev->per_tgt_results.median_range_mm[i]           = (int16_t) * (pfields + 28);
    #endif
    #ifdef VL53L5_START_RANGE_MM_ON
    prng_dev->per_tgt_results.start_range_mm[i]            = (int16_t) * (pfields + 29);
    #endif
    #ifdef VL53L5_END_RANGE_MM_ON
    prng_dev->per_tgt_results.end_range_mm[i]              = (int16_t) * (pfields + 30);
    #endif
    #ifdef VL53L5_MIN_RANGE_DELTA_MM_ON
    prng_dev->per_tgt_results.min_range_delta_mm[i]        = (uint8_t) * (pfields + 31);
    #endif
    #ifdef VL53L5_MAX_RANGE_DELTA_MM_ON
    prng_dev->per_tgt_results.max_range_delta_mm[i]        = (uint8_t) * (pfields + 32);
    #endif
    #ifdef VL53L5_TARGET_REFLECTANCE_EST_PC_ON
    prng_dev->per_tgt_results.target_reflectance_est_pc[i] = (uint8_t) * (pfields + 33);
    #endif
    #ifdef VL53L5_TARGET_STATUS_ON
    prng_dev->per_tgt_results.target_status[i]            = (uint8_t) * (pfields + 34);
    #endif

    #ifdef VL53L5_ALGO_DEPTH16_ON
    prng_patch_dev->d16_per_target_data.depth16[i]            = (uint16_t) * (pfields + 35);
    #endif

    #ifdef VL53L5_ALGO_GLASS_DETECTION_ON
    prng_patch_dev->gd_op_status.gd__rate_ratio        = (uint32_t) * (pfields + 36);
    prng_patch_dev->gd_op_status.gd__confidence        = (uint8_t) * (pfields + 37);
    prng_patch_dev->gd_op_status.gd__plane_detected    = (uint8_t) * (pfields + 38);
    prng_patch_dev->gd_op_status.gd__ratio_detected    = (uint8_t) * (pfields + 39);
    prng_patch_dev->gd_op_status.gd__glass_detected    = (uint8_t) * (pfields + 40);
    #endif

    return last_target;
}
