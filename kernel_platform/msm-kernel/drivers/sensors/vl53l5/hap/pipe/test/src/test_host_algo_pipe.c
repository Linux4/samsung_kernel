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
#include "vl53l5_utils_chk_luts.h"
#include "vl53l5_utils_chk_common.h"
#include "vl53l5_utils_chk_structs_results.h"
#include "vl53l5_range_results_checker.h"

#include "vl53l5_host_algo_pipe__set_cfg.h"
#include "vl53l5_host_algo_pipe_funcs.h"
#include "vl53l5_host_algo_pipe_init.h"
#include "vl53l5_host_algo_pipe.h"

#ifdef  VL53L5_ALGO_GLASS_DETECTION_ON
#include "vl53l5_gd_luts.h"
#endif
#ifdef  VL53L5_ALGO_DEPTH16_ON
#include "vl53l5_d16_luts.h"
#endif

#include "common_datatype_structs.h"
#include "test_utils.h"

#ifndef VL53L5_HOST_ALGO_PIPE_PROFILING
#include "test_hap_main_4x4_no_aff_tuning_data.h"
#include "test_hap_main_4x4_no_aff_input_data.h"
#include "test_hap_main_4x4_no_aff_expected_data.h"
#include "test_hap_main_8x8_no_aff_tuning_data.h"

#ifdef VL53L5_ALGO_ANTI_FLICKER_FILTER_ON
#include "test_hap_tuning_aff_only_data.h"
#include "test_hap_main_4x4_aff_only_input_data.h"
#include "test_hap_main_4x4_aff_only_expected_data.h"
#include "test_hap_main_4x4_aff_only_neg_input_data.h"
#include "test_hap_main_4x4_aff_only_neg_expected_data.h"
#endif
#else
#include "test_hap_tuning_default_data.h"
#endif

#include "test_hap_main_8x8_no_aff_input_data.h"
#include "test_hap_main_8x8_no_aff_expected_data.h"
#include "test_host_algo_pipe.h"

#include "vl53l5_test_logging_setup.h"

#ifndef VL53L5_HOST_ALGO_PIPE_PROFILING
static  int32_t  test_data_4x4_tuning[]   = TEST_HAP_MAIN_4X4_NO_AFF_TUNING_DATA;
static  int32_t  test_data_4x4_input[]    = TEST_HAP_MAIN_4X4_NO_AFF_INPUT_DATA;
static  int32_t  test_data_4x4_expected[] = TEST_HAP_MAIN_4X4_NO_AFF_EXPECTED_DATA;
static  int32_t  test_data_8x8_tuning[]   = TEST_HAP_MAIN_8X8_NO_AFF_TUNING_DATA;

#ifdef VL53L5_ALGO_ANTI_FLICKER_FILTER_ON
static  int32_t  hap_tuning_aff_only[]   = TEST_HAP_TUNING_AFF_ONLY_DATA;
static  int32_t  test_data_4x4_aff_only_input[]    = TEST_HAP_MAIN_4X4_AFF_ONLY_INPUT_DATA;
static  int32_t  test_data_4x4_aff_only_expected[] = TEST_HAP_MAIN_4X4_AFF_ONLY_EXPECTED_DATA;
static  int32_t  test_data_4x4_aff_only_neg_input[]    = TEST_HAP_MAIN_4X4_AFF_ONLY_NEG_INPUT_DATA;
static  int32_t  test_data_4x4_aff_only_neg_expected[] = TEST_HAP_MAIN_4X4_AFF_ONLY_NEG_EXPECTED_DATA;
#endif
#else
static  int32_t  hap_tuning_default[]   = TEST_HAP_TUNING_DEFAULT_DATA;
#endif

static  int32_t  test_data_8x8_input[]    = TEST_HAP_MAIN_8X8_NO_AFF_INPUT_DATA;
static  int32_t  test_data_8x8_expected[] = TEST_HAP_MAIN_8X8_NO_AFF_EXPECTED_DATA;

void test_tolerance_init_equal_to(
    struct  vl53l5_chk_tol_sharp_op_dev_t      *pchk_tol_sharp_op,
    struct  vl53l5_chk_tol_rng_op_dev_t        *pchk_tol_rng_op,
    struct  vl53l5_chk_tol_rng_patch_op_dev_t  *pchk_tol_rng_patch_op)
{

    memset(
        pchk_tol_sharp_op,
        0,
        sizeof(struct vl53l5_chk_tol_sharp_op_dev_t));
    memset(
        pchk_tol_rng_op,
        0,
        sizeof(struct vl53l5_chk_tol_rng_op_dev_t));
    memset(
        pchk_tol_rng_patch_op,
        0,
        sizeof(struct vl53l5_chk_tol_rng_patch_op_dev_t));
}

void test_tolerance_init_aff_only(
    struct  vl53l5_chk_tol_sharp_op_dev_t      *pchk_tol_sharp_op,
    struct  vl53l5_chk_tol_rng_op_dev_t        *pchk_tol_rng_op,
    struct  vl53l5_chk_tol_rng_patch_op_dev_t  *pchk_tol_rng_patch_op)
{

    test_tolerance_init_equal_to(
        pchk_tol_sharp_op,
        pchk_tol_rng_op,
        pchk_tol_rng_patch_op);

    pchk_tol_rng_op->tol__rng__no_of_targets.disable = 0;
    pchk_tol_rng_op->tol__rng__no_of_targets.abs_tol = 0;
    pchk_tol_rng_op->tol__rng__no_of_targets.rel_tol = 0;

    pchk_tol_rng_op->tol__peak_rate_kcps_per_spad.disable = 0;
    pchk_tol_rng_op->tol__peak_rate_kcps_per_spad.abs_tol = 0;
    pchk_tol_rng_op->tol__peak_rate_kcps_per_spad.rel_tol = 0;

    pchk_tol_rng_op->tol__rate_sigma_kcps_per_spad.disable = 0;
    pchk_tol_rng_op->tol__rate_sigma_kcps_per_spad.abs_tol = 1;
    pchk_tol_rng_op->tol__rate_sigma_kcps_per_spad.rel_tol = 0;

    pchk_tol_rng_op->tol__target_zscore.disable = 0;
    pchk_tol_rng_op->tol__target_zscore.abs_tol = 0;
    pchk_tol_rng_op->tol__target_zscore.rel_tol = 0;

    pchk_tol_rng_op->tol__range_sigma_mm.disable = 0;
    pchk_tol_rng_op->tol__range_sigma_mm.abs_tol = 1;
    pchk_tol_rng_op->tol__range_sigma_mm.rel_tol = 0;

    pchk_tol_rng_op->tol__median_range_mm.disable = 0;
    pchk_tol_rng_op->tol__median_range_mm.abs_tol = 0;
    pchk_tol_rng_op->tol__median_range_mm.rel_tol = 0;

    pchk_tol_rng_op->tol__target_status.disable = 0;
    pchk_tol_rng_op->tol__target_status.abs_tol = 0;
    pchk_tol_rng_op->tol__target_status.rel_tol = 0;
}

int32_t check_host_algo_pipe(
    char                      *ptest_label,
    int32_t                    tuning_cols,
    int32_t                    tuning_rows,
    int32_t                   *ptuning_data,
    int32_t                    input_cols,
    int32_t                    input_rows,
    int32_t                   *pinput_data,
    int32_t                    expected_cols,
    int32_t                    expected_rows,
    int32_t                   *pexpected_data,
    struct test_dev_t         *pdev,
    struct test_dev_t         *pexpected)
{

    int32_t   status       = 0;
    int32_t   error_count  = 0;
    int32_t   last_target  = 0;
    int32_t   range_count  = 0;
    int32_t   sample_no    = 0;
    int32_t   measurement_count = 0;

    int32_t   t_tuning     = 0;
    int32_t   t_input      = 0;
    int32_t   t_expected   = 0;
    int32_t   o            = 0;
    int32_t   i            = 0;

    int32_t   max_zones    = 0;
    int32_t   max_targets  = 0;

    struct  common_grp__status_t       decoded_status = {0};
    struct  common_grp__status_t      *pdecoded_status = &decoded_status;
    struct  vl53l5_range_results_t              *prng_dev = &(pdev->rng_dev);

    char    func_root[] = "vl53l5_host_algo_pipe";

    vl53l5_hap_get_version(
        &(pdev->ver_dev));

    vl53l5_host_algo_pipe_init(&(pdev->op_dev));

    range_count = 0;
    measurement_count = 0;

    for (t_input = 0; t_input < input_rows; t_input++)
    {

        o = t_input * input_cols;

        last_target =
            test_data_read_fields(
                pinput_data + o,
                &sample_no,
                &(pdev->rng_dev),
                &(pdev->op_dev.rng_patch_dev));

        if (last_target == 1)
        {

            if (t_tuning < tuning_rows)
            {
                o = t_tuning * tuning_cols;

                (void)test_hap_tuning_read_fields(
                    ptuning_data + o,
                    1,
                    &(pdev->tuning));

                status =
                    vl53l5_host_algo_pipe__set_cfg(
                        &(pdev->tuning),
                        &(pdev->cfg_dev),
                        &(pdev->err_dev));

                if (status != 0)
                {
                    goto pipe_error;
                }

                t_tuning++;
            }

            test_print("%s::%06d::%06d::Processing Range\n",
                       ptest_label,
                       range_count,
                       sample_no);

            if (sample_no == 0)
            {
                vl53l5_host_algo_pipe_clear_initialised(&(pdev->op_dev));
                measurement_count += 1;
                test_print("%s::%06d::Clearing HAP Initialised Flag\n",
                           ptest_label,
                           range_count);
            }

            status =
                vl53l5_host_algo_pipe(
                    &(pdev->rng_dev),
                    #ifdef VL53L5_SHARPENER_TARGET_DATA_ON
                    (&(pdev->sharp_tgt_data)),
                    #endif
                    (&(pdev->cfg_dev)),
                    &(pdev->int_dev),
                    &(pdev->op_dev),
                    &(pdev->err_dev));
            if (status != 0)
            {
                goto pipe_error;
            }

            max_zones =
                (int32_t)prng_dev->common_data.rng__no_of_zones;
            max_targets =
                max_zones * (int32_t)prng_dev->common_data.rng__max_targets_per_zone;

            for (i = 0; i < max_targets; i++)
            {
                pexpected->op_dev.rng_patch_dev.d16_per_target_data.depth16[i] = (uint16_t)(1U << 13U);
            }

            last_target = 0;
            while (t_expected < expected_rows && last_target == 0)
            {
                o = t_expected * expected_cols;

                last_target =
                    test_data_read_fields(
                        pexpected_data + o,
                        &sample_no,
                        &(pexpected->rng_dev),
                        &(pexpected->op_dev.rng_patch_dev));

                t_expected++;
            }

            error_count +=
                vl53l5_range_dev_check_values(
                    VL53L5_CSV_FMT_SELECT__EXT32_HAP_GD,
                    &(pdev->rng_dev),
                    &(pexpected->rng_dev),
                    &(pdev->chk_tol_rng_op));

            #ifdef VL53L5_ALGO_GLASS_DETECTION_ON
            if (pdev->cfg_dev.gd_cfg_dev.gd_general_cfg.gd__mode != VL53L5_GD_MODE__DISABLED)
            {
                error_count +=
                    vl53l5_dci_gd_op_status_check_values(
                        "gd_op_status.",
                        &(pdev->op_dev.rng_patch_dev.gd_op_status),
                        &(pexpected->op_dev.rng_patch_dev.gd_op_status),
                        &(pdev->chk_tol_rng_patch_op.tol__gd_op_status));
            }
            #endif

            #ifdef VL53L5_ALGO_DEPTH16_ON
            if (pdev->cfg_dev.d16_cfg_dev.d16_general_cfg.d16__cfg__mode != VL53L5_D16_MODE__DISABLED)
            {
                error_count +=
                    vl53l5_utils_check_array_uint16(
                        "d16_per_target_data.",
                        "depth16",
                        max_targets,
                        &(pdev->op_dev.rng_patch_dev.d16_per_target_data.depth16[0]),
                        &(pexpected->op_dev.rng_patch_dev.d16_per_target_data.depth16[0]),
                        &(pdev->chk_tol_rng_patch_op.tol__depth16));
            }
            #endif

            range_count++;
        }
    }

pipe_error:

    if (status != 0)
    {
        test_info("test_%s()::%s::PIPE ERROR! status = 0x%08u\n",
                  func_root, ptest_label, status);
        test_print("test_%s()::%s::PIPE ERROR! status = 0x%08u\n",
                   func_root, ptest_label, status);

        error_count += 1001;

        vl53l5_hap_decode_status_code(
            status,
            pdecoded_status);

        test_common_status_print_json(
            pdecoded_status);

        test_common_status_write_json(
            pdecoded_status);
    }

    test_info(
        "test_%s()::%s::measurement_count = %6d, range_count = %6d, error_count = %d\n",
        func_root,
        ptest_label,
        measurement_count,
        pdev->op_dev.hap_state.range_count,
        error_count);
    test_print(
        "test_%s()::%s::measurement_count = %6d, range_count = %6d, error_count = %d\n",
        func_root,
        ptest_label,
        measurement_count,
        pdev->op_dev.hap_state.range_count,
        error_count);

    return error_count;
}

int32_t test_host_algo_pipe(
    const char                *plog_txt_file,
    struct test_dev_t         *pdev,
    struct test_dev_t         *pexpected)
{

    int32_t    error_count   = 0;
    #ifndef __KERNEL__
    int32_t   status        = 0;
    #endif

    char    func_root[] = "vl53l5_host_algo_pipe";

    #ifndef __KERNEL__
    if (plog_txt_file != NULL)
    {
        status = vl53l5_test_open_log_file(plog_txt_file);
        if (status != 0)
        {
            error_count = status;
            goto exit;
        }
    }
    #else
    (void)plog_txt_file;
    #endif

    vl53l5_utils_chk_set_reporting_level(
        VL53L5_CHK_REPORTING_LEVEL__ERROR_ONLY);

    vl53l5_hap_get_version(&(pdev->ver_dev));
    test_pipe_and_algo_versions_write(&(pdev->ver_dev));

    test_tolerance_init_equal_to(
        &(pdev->chk_tol_sharp_op),
        &(pdev->chk_tol_rng_op),
        &(pdev->chk_tol_rng_patch_op));

    #ifndef VL53L5_HOST_ALGO_PIPE_PROFILING

    error_count +=
        check_host_algo_pipe(
            "4x4_no_aff",
            TEST_HAP_MAIN_4X4_NO_AFF_TUNING_COLS,
            TEST_HAP_MAIN_4X4_NO_AFF_TUNING_ROWS,
            &(test_data_4x4_tuning[0]),
            TEST_HAP_MAIN_4X4_NO_AFF_INPUT_COLS,
            TEST_HAP_MAIN_4X4_NO_AFF_INPUT_ROWS,
            &(test_data_4x4_input[0]),
            TEST_HAP_MAIN_4X4_NO_AFF_EXPECTED_COLS,
            TEST_HAP_MAIN_4X4_NO_AFF_EXPECTED_ROWS,
            &(test_data_4x4_expected[0]),
            pdev,
            pexpected);

    error_count +=
        check_host_algo_pipe(
            "8x8_no_aff",
            TEST_HAP_MAIN_8X8_NO_AFF_TUNING_COLS,
            TEST_HAP_MAIN_8X8_NO_AFF_TUNING_ROWS,
            &(test_data_8x8_tuning[0]),
            TEST_HAP_MAIN_8X8_NO_AFF_INPUT_COLS,
            TEST_HAP_MAIN_8X8_NO_AFF_INPUT_ROWS,
            &(test_data_8x8_input[0]),
            TEST_HAP_MAIN_8X8_NO_AFF_EXPECTED_COLS,
            TEST_HAP_MAIN_8X8_NO_AFF_EXPECTED_ROWS,
            &(test_data_8x8_expected[0]),
            pdev,
            pexpected);

    #ifdef VL53L5_ALGO_ANTI_FLICKER_FILTER_ON

    test_tolerance_init_aff_only(
        &(pdev->chk_tol_sharp_op),
        &(pdev->chk_tol_rng_op),
        &(pdev->chk_tol_rng_patch_op));

    error_count +=
        check_host_algo_pipe(
            "4x4_aff_only",
            TEST_HAP_TUNING_AFF_ONLY_COLS,
            TEST_HAP_TUNING_AFF_ONLY_ROWS,
            &(hap_tuning_aff_only[0]),
            TEST_HAP_MAIN_4X4_AFF_ONLY_INPUT_COLS,
            TEST_HAP_MAIN_4X4_AFF_ONLY_INPUT_ROWS,
            &(test_data_4x4_aff_only_input[0]),
            TEST_HAP_MAIN_4X4_AFF_ONLY_EXPECTED_COLS,
            TEST_HAP_MAIN_4X4_AFF_ONLY_EXPECTED_ROWS,
            &(test_data_4x4_aff_only_expected[0]),
            pdev,
            pexpected);

    error_count +=
        check_host_algo_pipe(
            "4x4_aff_only_neg",
            TEST_HAP_TUNING_AFF_ONLY_COLS,
            TEST_HAP_TUNING_AFF_ONLY_ROWS,
            &(hap_tuning_aff_only[0]),
            TEST_HAP_MAIN_4X4_AFF_ONLY_NEG_INPUT_COLS,
            TEST_HAP_MAIN_4X4_AFF_ONLY_NEG_INPUT_ROWS,
            &(test_data_4x4_aff_only_neg_input[0]),
            TEST_HAP_MAIN_4X4_AFF_ONLY_NEG_EXPECTED_COLS,
            TEST_HAP_MAIN_4X4_AFF_ONLY_NEG_EXPECTED_ROWS,
            &(test_data_4x4_aff_only_neg_expected[0]),
            pdev,
            pexpected);
    #endif

    #else

    error_count +=
        check_host_algo_pipe(
            "8x8_default",
            TEST_HAP_TUNING_DEFAULT_COLS,
            TEST_HAP_TUNING_DEFAULT_ROWS,
            &(hap_tuning_default[0]),
            TEST_HAP_MAIN_8X8_NO_AFF_INPUT_COLS,
            TEST_HAP_MAIN_8X8_NO_AFF_INPUT_ROWS,
            &(test_data_8x8_input[0]),
            TEST_HAP_MAIN_8X8_NO_AFF_EXPECTED_COLS,
            TEST_HAP_MAIN_8X8_NO_AFF_EXPECTED_ROWS,
            &(test_data_8x8_expected[0]),
            pdev,
            pexpected);
    #endif

    test_print("test_%s() - error_count = %d\n", func_root, error_count);

    #ifndef __KERNEL__
    vl53l5_test_close_log_file();

exit:
    #endif

    test_info("test_%s() - error_count = %d\n", func_root, error_count);
    return error_count;
}
