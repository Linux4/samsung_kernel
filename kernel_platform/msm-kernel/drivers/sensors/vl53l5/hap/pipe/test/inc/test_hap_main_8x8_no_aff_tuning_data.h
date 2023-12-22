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

#ifndef TEST_HAP_MAIN_8X8_NO_AFF_TUNING_DATA_H_
#define TEST_HAP_MAIN_8X8_NO_AFF_TUNING_DATA_H_

#ifdef __cplusplus
extern "C" {
#endif

#define TEST_HAP_MAIN_8X8_NO_AFF_TUNING_ROWS 10
#define TEST_HAP_MAIN_8X8_NO_AFF_TUNING_COLS 16
#define TEST_HAP_MAIN_8X8_NO_AFF_TUNING_SZ 160

#define TEST_HAP_MAIN_8X8_NO_AFF_TUNING_DATA { \
    0, 2, 3, 3, 3, 0, 1, 4, 2, 2, 3, 0, 0, 4, 2, 4, \
    1, 2, 3, 3, 3, 1, 1, 4, 2, 2, 3, 0, 0, 4, 2, 4, \
    2, 2, 3, 3, 3, 2, 1, 4, 2, 2, 3, 0, 0, 4, 2, 4, \
    3, 2, 3, 3, 3, 3, 1, 4, 2, 2, 3, 0, 0, 4, 2, 4, \
    4, 2, 3, 3, 3, 4, 1, 4, 2, 2, 3, 0, 0, 4, 2, 4, \
    5, 2, 3, 3, 3, 5, 1, 4, 2, 2, 3, 0, 0, 4, 2, 4, \
    6, 2, 3, 3, 3, 0, 1, 4, 2, 2, 3, 0, 0, 4, 2, 4, \
    7, 2, 3, 3, 3, 0, 1, 4, 2, 2, 3, 0, 0, 4, 2, 4, \
    8, 2, 3, 3, 3, 0, 1, 4, 2, 2, 3, 0, 0, 4, 2, 4, \
    9, 2, 3, 3, 3, 0, 1, 4, 2, 2, 3, 1, 1, 4, 2, 4}

#ifdef __cplusplus
}
#endif

#endif
