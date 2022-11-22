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

#include "test_hap_update_aff_cfg.h"
#include "vl53l5_utils_chk_luts.h"
#include "vl53l5_utils_chk_structs.h"
#include "vl53l5_utils_chk_common.h"
#include "vl53l5_aff_defs.h"
#include "vl53l5_host_algo_pipe_funcs.h"
#include "test_hap_update_aff_cfg_data.h"
#include "test_utils.h"

#include "vl53l5_test_logging_setup.h"

#ifdef VL53L5_ALGO_ANTI_FLICKER_FILTER_ON

static const int32_t test_set_cfg[] = TEST_HAP_UPDATE_AFF_CFG_DATA;

int32_t test_hap_update_aff_cfg(
    const char                      *plog_txt_file,
    struct   test_dev_t             *pdev,
    struct   test_dev_t             *pexpected)
{

    int32_t  error_count = 0;
    #ifndef __KERNEL__
    int32_t   status        = 0;
    #endif
    int32_t  t       = 0;
    int32_t  o       = 0;
    int32_t  i       = 0;

    char    func_root[] = "vl53l5_hap_update_aff_cfg";

    struct  vl53l5_range_results_t              *prng_dev = &(pdev->rng_dev);
    struct  vl53l5_chk__tolerance_t    chk_tol  = {0};
    struct  vl53l5_chk__tolerance_t  *pchk_tol  = &chk_tol;

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

    for (t = 0 ; t < TEST_HAP_UPDATE_AFF_CFG_ROWS ; t++)
    {
        o = t * TEST_HAP_UPDATE_AFF_CFG_COLS;

        pdev->cfg_dev.hap_general_cfg.aff_processing_mode = (uint8_t)test_set_cfg[o + 1];
        prng_dev->common_data.rng__no_of_zones = (uint8_t)test_set_cfg[o + 2];
        prng_dev->common_data.rng__acc__no_of_zones = (uint8_t)test_set_cfg[o + 3];

        pexpected->cfg_dev.aff_cfg_dev.aff_cfg.aff__nb_zones_to_filter = (uint8_t)test_set_cfg[o + 4];

        for (i = 0 ; i < (int32_t)VL53L5_AFF_DEF__MAX_ZONE_SEL; i++)
        {
            pexpected->cfg_dev.aff_cfg_dev.aff_arrayed_cfg.aff__zone_sel[i] =
                (uint8_t)test_set_cfg[o + i + 5];
        }

        vl53l5_hap_update_aff_cfg(
            &(pdev->cfg_dev.hap_general_cfg),
            &(pdev->rng_dev),
            &(pdev->cfg_dev.aff_cfg_dev),
            &(pdev->err_dev));

        error_count +=
            vl53l5_utils_check_equal_to(
                "aff_cfg.",
                "aff__nb_zones_to_filter",
                -1,
                (int32_t)pdev->cfg_dev.aff_cfg_dev.aff_cfg.aff__nb_zones_to_filter,
                (int32_t)pexpected->cfg_dev.aff_cfg_dev.aff_cfg.aff__nb_zones_to_filter);

        error_count +=
            vl53l5_utils_check_array_uint8(
                "aff_arrayed_cfg.",
                "aff__zone_sel",
                VL53L5_AFF_DEF__MAX_ZONE_SEL,
                &(pdev->cfg_dev.aff_cfg_dev.aff_arrayed_cfg.aff__zone_sel[0]),
                &(pexpected->cfg_dev.aff_cfg_dev.aff_arrayed_cfg.aff__zone_sel[0]),
                pchk_tol);
    }

    test_print("test_%s() - error_count = %d\n", func_root, error_count);

    #ifndef __KERNEL__
    vl53l5_test_close_log_file();

exit:
    #endif

    test_info("test_%s() - error_count = %d\n", func_root, error_count);
    return error_count;
}

#endif
