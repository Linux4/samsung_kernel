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
#include "vl53l5_platform_algo_macros.h"
#include "vl53l5_utils_chk_luts.h"
#include "vl53l5_utils_chk_common.h"

#include "vl53l5_test_logging_setup.h"

static uint32_t _utils_chk_reporting_level = VL53L5_CHK_REPORTING_LEVEL__FULL;

void vl53l5_utils_chk_set_reporting_level(
    uint32_t  reporting_level)
{

    _utils_chk_reporting_level = reporting_level;
}

uint32_t vl53l5_utils_chk_get_reporting_level(void)
{

    return _utils_chk_reporting_level;
}

int32_t vl53l5_utils_check_equal_to(
    char       *pprefix,
    char       *pname,
    int32_t     idx,
    int32_t     actual_value,
    int32_t     expected_value)
{

    int32_t   error_count = 0;
    uint32_t  error_flag  = 0U;

    const char  *pstatus_strings[] = {"CHK::OK", "CHK::ERROR"};

    if (actual_value != expected_value)
    {
        error_count = 1;
        error_flag  = 1U;
    }

    if (((_utils_chk_reporting_level & error_flag) > 0) ||
            (_utils_chk_reporting_level == VL53L5_CHK_REPORTING_LEVEL__FULL))
    {
        if (idx < 0)
        {
            test_print(
                "%-10s  Actual = %12d, Expected = %12d :: %s%s\n",
                pstatus_strings[error_count],
                actual_value,
                expected_value,
                pprefix,
                pname);
        }
        else
        {
            test_print(
                "%-10s  Actual = %12d, Expected = %12d :: %s%s[%d]\n",
                pstatus_strings[error_count],
                actual_value,
                expected_value,
                pprefix,
                pname,
                idx);
        }
    }

    return error_count;
}

int32_t vl53l5_utils_check_within_tol(
    char                              *pprefix,
    char                              *pname,
    int32_t                            idx,
    int32_t                            actual_value,
    int32_t                            expected_value,
    struct   vl53l5_chk__tolerance_t  *ptol)
{

    int32_t   error_count = 0;
    uint32_t  error_flag  = 0U;

    const char   *pstatus_strings[] = {"CHK::OK", "CHK::ERROR"};

    uint32_t     delta        = 0;
    uint32_t     tolerance    = 0;
    uint64_t     tmp          = 0;

    if (ptol->disable > 0)
    {
        goto exit;
    }

    if (actual_value > expected_value)
    {
        delta = (uint32_t)(actual_value - expected_value);
    }
    else
    {
        delta = (uint32_t)(expected_value - actual_value);
    }

    if (expected_value < 0)
    {
        tmp = (uint64_t)(-expected_value);
    }
    else
    {
        tmp = (uint64_t)expected_value;
    }

    tmp = (tmp * (uint64_t)(ptol->rel_tol)) + 500;
    tmp = DIV_64D64_64(tmp, 1000);

    tolerance = (uint32_t)(ptol->abs_tol) + (uint32_t)tmp;

    if (delta > tolerance)
    {
        error_count = 1;
        error_flag  = 1U;
    }

    if (((_utils_chk_reporting_level & error_flag) > 0) ||
            (_utils_chk_reporting_level == VL53L5_CHK_REPORTING_LEVEL__FULL))
    {
        if (idx < 0)
        {
            test_print(
                "%-10s  Actual = %12d, Expected = %12d, Tol = %9u :: %s%s\n",
                pstatus_strings[error_count],
                actual_value,
                expected_value,
                tolerance,
                pprefix,
                pname);
        }
        else
        {
            test_print(
                "%-10s  Actual = %12d, Expected = %12d, Tol = %9u :: %s%s[%d]\n",
                pstatus_strings[error_count],
                actual_value,
                expected_value,
                tolerance,
                pprefix,
                pname,
                idx);
        }
    }

exit:
    return error_count;
}

int32_t vl53l5_utils_check_array_uint8(
    char                              *pprefix,
    char                              *pname,
    int32_t                            array_size,
    uint8_t                           *pactual,
    uint8_t                           *pexpected,
    struct   vl53l5_chk__tolerance_t  *ptol)
{
    int32_t    error_count  = 0;
    int32_t    i = 0;

    for (i = 0 ; i < array_size ; i++)
    {
        error_count +=
            vl53l5_utils_check_within_tol(
                pprefix,
                pname,
                i,
                (int32_t)(*(pactual + i)),
                (int32_t)(*(pexpected + i)),
                ptol);
    }

    return error_count;
}

int32_t vl53l5_utils_check_array_int8(
    char                              *pprefix,
    char                              *pname,
    int32_t                            array_size,
    int8_t                            *pactual,
    int8_t                            *pexpected,
    struct   vl53l5_chk__tolerance_t  *ptol)
{
    int32_t    error_count = 0;
    int32_t    i = 0;

    for (i = 0 ; i < array_size ; i++)
    {
        error_count +=
            vl53l5_utils_check_within_tol(
                pprefix,
                pname,
                i,
                (int32_t)(*(pactual + i)),
                (int32_t)(*(pexpected + i)),
                ptol);
    }

    return error_count;
}

int32_t vl53l5_utils_check_array_uint16(
    char                              *pprefix,
    char                              *pname,
    int32_t                            array_size,
    uint16_t                          *pactual,
    uint16_t                          *pexpected,
    struct   vl53l5_chk__tolerance_t  *ptol)
{
    int32_t    error_count = 0;
    int32_t    i = 0;

    for (i = 0 ; i < array_size ; i++)
    {
        error_count +=
            vl53l5_utils_check_within_tol(
                pprefix,
                pname,
                i,
                (int32_t)(*(pactual + i)),
                (int32_t)(*(pexpected + i)),
                ptol);
    }

    return error_count;
}

int32_t vl53l5_utils_check_array_int16(
    char                              *pprefix,
    char                              *pname,
    int32_t                            array_size,
    int16_t                           *pactual,
    int16_t                           *pexpected,
    struct   vl53l5_chk__tolerance_t  *ptol)
{
    int32_t    error_count = 0;
    int32_t    i = 0;

    for (i = 0 ; i < array_size ; i++)
    {
        error_count +=
            vl53l5_utils_check_within_tol(
                pprefix,
                pname,
                i,
                (int32_t)(*(pactual + i)),
                (int32_t)(*(pexpected + i)),
                ptol);
    }

    return error_count;
}

int32_t vl53l5_utils_check_array_uint32(
    char                              *pprefix,
    char                              *pname,
    int32_t                            array_size,
    uint32_t                          *pactual,
    uint32_t                          *pexpected,
    struct   vl53l5_chk__tolerance_t  *ptol)
{
    int32_t    error_count = 0;
    int32_t    i = 0;

    for (i = 0 ; i < array_size ; i++)
    {
        error_count +=
            vl53l5_utils_check_within_tol(
                pprefix,
                pname,
                i,
                (int32_t)(*(pactual + i)),
                (int32_t)(*(pexpected + i)),
                ptol);
    }

    return error_count;
}

int32_t vl53l5_utils_check_array_int32(
    char                              *pprefix,
    char                              *pname,
    int32_t                            array_size,
    int32_t                           *pactual,
    int32_t                           *pexpected,
    struct   vl53l5_chk__tolerance_t  *ptol)
{
    int32_t    error_count = 0;
    int32_t    i = 0;

    for (i = 0 ; i < array_size ; i++)
    {
        error_count +=
            vl53l5_utils_check_within_tol(
                pprefix,
                pname,
                i,
                (int32_t)(*(pactual + i)),
                (int32_t)(*(pexpected + i)),
                ptol);
    }

    return error_count;
}
