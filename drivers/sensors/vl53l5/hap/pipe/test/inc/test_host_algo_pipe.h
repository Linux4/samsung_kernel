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

#ifndef TEST_HOST_ALGO_PIPE_H_
#define TEST_HOST_ALGO_PIPE_H_

#include "vl53l5_types.h"
#include "vl53l5_results_config.h"
#include "vl53l5_algo_build_config.h"
#include "vl53l5_hap_structs.h"
#include "test_map.h"

void test_tolerance_init_equal_to(
    struct  vl53l5_chk_tol_sharp_op_dev_t      *pchk_tol_sharp_op,
    struct  vl53l5_chk_tol_rng_op_dev_t        *pchk_tol_rng_op,
    struct  vl53l5_chk_tol_rng_patch_op_dev_t  *pchk_tol_rng_patch_op);

void test_tolerance_init_aff_only(
    struct  vl53l5_chk_tol_sharp_op_dev_t      *pchk_tol_sharp_op,
    struct  vl53l5_chk_tol_rng_op_dev_t        *pchk_tol_rng_op,
    struct  vl53l5_chk_tol_rng_patch_op_dev_t  *pchk_tol_rng_patch_op);

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
    struct test_dev_t         *pexpected);

int32_t test_host_algo_pipe(
    const char                *plog_txt_file,
    struct test_dev_t         *pdev,
    struct test_dev_t         *pexpected);

#endif
