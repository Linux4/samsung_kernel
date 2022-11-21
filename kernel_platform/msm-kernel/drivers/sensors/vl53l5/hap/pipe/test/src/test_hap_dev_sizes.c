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

#include "vl53l5_algo_build_config.h"
#include "vl53l5_utils_chk_luts.h"
#include "vl53l5_utils_chk_common.h"
#include "dci_structs.h"
#include "vl53l5_results_map.h"
#include "dci_patch_op_map.h"
#include "vl53l5_hap_ver_map.h"
#include "vl53l5_hap_cfg_map.h"
#include "vl53l5_hap_int_map.h"
#include "vl53l5_hap_op_map.h"
#ifdef VL53L5_ALGO_LONG_TAIL_FILTER_ON
#include "vl53l5_ltf_err_map.h"
#endif
#ifdef VL53L5_ALGO_GLASS_DETECTION_ON
#include "vl53l5_gd_err_map.h"
#endif
#ifdef VL53L5_ALGO_RANGE_ROTATION_ON
#include "vl53l5_rr_err_map.h"
#endif
#ifdef VL53L5_ALGO_ANTI_FLICKER_FILTER_ON
#include "vl53l5_aff_err_map.h"
#endif
#ifdef VL53L5_ALGO_OUTPUT_TARGET_SORT_ON
#include "vl53l5_ots_err_map.h"
#endif
#include "vl53l5_otf_err_map.h"
#if defined(VL53L5_ALGO_DEPTH16_ON) || defined(VL53L5_ALGO_DEPTH16_BUFFER_ON)
#include "vl53l5_d16_err_map.h"
#endif
#include "vl53l5_hap_err_map.h"
#include "test_map.h"
#include "test_hap_dev_sizes.h"

#include "vl53l5_test_logging_setup.h"

int32_t test_hap_dev_sizes(
    const char                     *plog_txt_file)
{

    int32_t   error_count   = 0;
    #ifndef __KERNEL__
    int32_t   status        = 0;
    #endif

    char    func_root[] = "hap_dev_sizes";

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

    test_print("%-32s = %6d\n", "dci_op_dev_t",                      sizeof(struct vl53l5_range_results_t));
    test_print("%-32s = %6d\n", "dci_patch_op_dev_t",                sizeof(struct dci_patch_op_dev_t));
    #ifdef VL53L5_ALGO_DEPTH16_BUFFER_ON
    test_print("%-32s = %6d\n", "dci_d16_buf_dev_t",                 sizeof(struct dci_d16_buf_dev_t));
    #endif
    test_print("%-32s = %6d\n", "dci_grp__sharpener_target_data_t",  sizeof(struct vl53l5_sharpener_target_data_t));

    #ifdef VL53L5_ALGO_LONG_TAIL_FILTER_ON
    test_print("%-32s = %6d\n", "vl53l5_ltf_cfg_dev_t", sizeof(struct vl53l5_ltf_cfg_dev_t));
    test_print("%-32s = %6d\n", "vl53l5_ltf_err_dev_t", sizeof(struct vl53l5_ltf_err_dev_t));
    #endif

    #ifdef VL53L5_ALGO_GLASS_DETECTION_ON
    test_print("%-32s = %6d\n", "vl53l5_gd_cfg_dev_t",  sizeof(struct vl53l5_gd_cfg_dev_t));
    test_print("%-32s = %6d\n", "vl53l5_gd_int_dev_t",  sizeof(struct vl53l5_gd_int_dev_t));
    test_print("%-32s = %6d\n", "vl53l5_gd_err_dev_t",  sizeof(struct vl53l5_gd_err_dev_t));
    #endif

    #ifdef VL53L5_ALGO_RANGE_ROTATION_ON
    test_print("%-32s = %6d\n", "vl53l5_rr_cfg_dev_t",  sizeof(struct vl53l5_rr_cfg_dev_t));
    test_print("%-32s = %6d\n", "vl53l5_rr_int_dev_t",  sizeof(struct vl53l5_rr_int_dev_t));
    test_print("%-32s = %6d\n", "vl53l5_rr_err_dev_t",  sizeof(struct vl53l5_rr_err_dev_t));
    #endif

    #ifdef VL53L5_ALGO_ANTI_FLICKER_FILTER_ON
    test_print("%-32s = %6d\n", "vl53l5_aff_cfg_dev_t", sizeof(struct vl53l5_aff_cfg_dev_t));
    test_print("%-32s = %6d\n", "vl53l5_aff_int_dev_t", sizeof(struct vl53l5_aff_int_dev_t));
    test_print("%-32s = %6d\n", "vl53l5_aff_err_dev_t", sizeof(struct vl53l5_aff_err_dev_t));
    #endif

    #ifdef VL53L5_ALGO_OUTPUT_TARGET_SORT_ON
    test_print("%-32s = %6d\n", "vl53l5_ots_cfg_dev_t", sizeof(struct vl53l5_ots_cfg_dev_t));
    test_print("%-32s = %6d\n", "vl53l5_ots_err_dev_t", sizeof(struct vl53l5_ots_err_dev_t));
    #endif

    test_print("%-32s = %6d\n", "vl53l5_otf_cfg_dev_t", sizeof(struct vl53l5_otf_cfg_dev_t));
    test_print("%-32s = %6d\n", "vl53l5_otf_int_dev_t", sizeof(struct vl53l5_otf_int_dev_t));
    test_print("%-32s = %6d\n", "vl53l5_otf_err_dev_t", sizeof(struct vl53l5_otf_err_dev_t));

    #if defined(VL53L5_ALGO_DEPTH16_ON) || defined(VL53L5_ALGO_DEPTH16_BUFFER_ON)
    test_print("%-32s = %6d\n", "vl53l5_d16_cfg_dev_t", sizeof(struct vl53l5_d16_cfg_dev_t));
    test_print("%-32s = %6d\n", "vl53l5_d16_err_dev_t", sizeof(struct vl53l5_d16_err_dev_t));
    #endif

    test_print("%-32s = %6d\n", "vl53l5_hap_ver_dev_t", sizeof(struct vl53l5_hap_ver_dev_t));
    test_print("%-32s = %6d\n", "vl53l5_hap_cfg_dev_t", sizeof(struct vl53l5_hap_cfg_dev_t));
    test_print("%-32s = %6d\n", "vl53l5_hap_int_dev_t", sizeof(struct vl53l5_hap_int_dev_t));
    test_print("%-32s = %6d\n", "vl53l5_hap_op_dev_t",  sizeof(struct vl53l5_hap_op_dev_t));
    test_print("%-32s = %6d\n", "vl53l5_hap_err_dev_t", sizeof(struct vl53l5_hap_err_dev_t));

    test_print("%-32s = %6d\n", "test_dev_t",           sizeof(struct test_dev_t));

    test_print("test_%s() - error_count = %d\n", func_root, error_count);

    #ifndef __KERNEL__
    vl53l5_test_close_log_file();

exit:
    #endif

    test_info("test_%s() - error_count = %d\n", func_root, error_count);
    return error_count;
}
