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

#include "vl53l5_utils_chk_luts.h"
#include "vl53l5_utils_chk_common.h"
#include "common_datatype_structs.h"
#include "vl53l5_hap_ver_map.h"
#include "vl53l5_host_algo_pipe_funcs.h"
#include "test_hap_get_version_data.h"
#include "test_hap_get_version.h"
#include "test_utils.h"

#include "vl53l5_test_logging_setup.h"

static const int32_t test_version[] = TEST_HAP_GET_VERSION_DATA;

int32_t test_hap_get_version(
    const char                *plog_txt_file)
{

    int32_t   error_count   = 0;
    #ifndef __KERNEL__
    int32_t   status        = 0;
    #endif

    struct    vl53l5_hap_ver_dev_t   ver_dev;
    struct    vl53l5_hap_ver_dev_t *pver_dev = &ver_dev;

    struct    common_grp__version_t  expected = {0};
    struct    common_grp__version_t *pexpected = &expected;

    char    func_root[] = "vl53l5_hap_get_version";

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
        VL53L5_CHK_REPORTING_LEVEL__FULL);

    pexpected->version__revision = (uint16_t)test_version[0];
    pexpected->version__build = (uint16_t)test_version[1];
    pexpected->version__minor = (uint16_t)test_version[2];
    pexpected->version__major = (uint16_t)test_version[3];

    vl53l5_hap_get_version(pver_dev);

    test_pipe_and_algo_versions_write(pver_dev);

    error_count +=
        vl53l5_utils_check_equal_to(
            "pipe_version.",
            "version__revision",
            -1,
            (int32_t)pver_dev->pipe_version.version__revision,
            pexpected->version__revision);

    error_count +=
        vl53l5_utils_check_equal_to(
            "pipe_version.",
            "version__build",
            -1,
            (int32_t)pver_dev->pipe_version.version__build,
            pexpected->version__build);

    error_count +=
        vl53l5_utils_check_equal_to(
            "pipe_version.",
            "version__minor",
            -1,
            (int32_t)pver_dev->pipe_version.version__minor,
            pexpected->version__minor);

    error_count +=
        vl53l5_utils_check_equal_to(
            "pipe_version.",
            "version__major",
            -1,
            (int32_t)pver_dev->pipe_version.version__major,
            pexpected->version__major);

    test_print("test_%s() - error_count = %d\n", func_root, error_count);

    #ifndef __KERNEL__
    vl53l5_test_close_log_file();

exit:
    #endif

    test_info("test_%s() - error_count = %d\n", func_root, error_count);
    return error_count;
}
