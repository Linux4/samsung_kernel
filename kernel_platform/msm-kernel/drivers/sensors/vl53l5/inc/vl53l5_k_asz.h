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

#ifndef VL53L5_K_ASZ_H
#define VL53L5_K_ASZ_H

#ifdef __cplusplus
extern "C" {
#endif

#define ASZ_0_BH 0xdf900041U
#define ASZ_1_BH 0xdfd40041U
#define ASZ_2_BH 0xe0180041U
#define ASZ_3_BH 0xe05c0041U

#define byte1(bh) (0xff & bh)
#define byte2(bh) (0xff & (bh >> 8))
#define byte3(bh) (0xff & (bh >> 16))
#define byte4(bh) (0xff & (bh >> 24))

// To adjust the ASZs for ROIs have to chage below configuration 

// # 1 ASZ, Zones ID 
#ifdef STM_VL53L5_SUPPORT_LEGACY_CODE
#define ASZ_8X8_0_A  27U
#define ASZ_8X8_0_B  28U
#define ASZ_8X8_0_C  35U
#define ASZ_8X8_0_D  36U

#define ASZ_8X8_1_A  0U
#define ASZ_8X8_1_B  1U
#define ASZ_8X8_1_C  8U
#define ASZ_8X8_1_D  9U

#define ASZ_8X8_2_A  6U
#define ASZ_8X8_2_B  7U
#define ASZ_8X8_2_C  14U
#define ASZ_8X8_2_D  15U

#define ASZ_8X8_3_A  48U
#define ASZ_8X8_3_B  49U
#define ASZ_8X8_3_C  56U
#define ASZ_8X8_3_D  57U
#endif
#ifdef STM_VL53L5_SUPPORT_SEC_CODE
#define ASZ_8X8_0_A  20U
#define ASZ_8X8_0_B  21U
#define ASZ_8X8_0_C  28U
#define ASZ_8X8_0_D  29U

#define ASZ_8X8_1_A  13U
#define ASZ_8X8_1_B  14U
#define ASZ_8X8_1_C  21U
#define ASZ_8X8_1_D  22U

#define ASZ_8X8_2_A  29U
#define ASZ_8X8_2_B  30U
#define ASZ_8X8_2_C  37U
#define ASZ_8X8_2_D  38U

#define ASZ_8X8_3_A  29U
#define ASZ_8X8_3_B  30U
#define ASZ_8X8_3_C  29U
#define ASZ_8X8_3_D  30U
#endif
#define ASZ_BUFFER_8X8 { \
	byte1(ASZ_0_BH), byte2(ASZ_0_BH), byte3(ASZ_0_BH), byte4(ASZ_0_BH), \
	ASZ_8X8_0_A, ASZ_8X8_0_B, ASZ_8X8_0_C, ASZ_8X8_0_D, \
	byte1(ASZ_1_BH), byte2(ASZ_1_BH), byte3(ASZ_1_BH), byte4(ASZ_1_BH), \
	ASZ_8X8_1_A, ASZ_8X8_1_B, ASZ_8X8_1_C, ASZ_8X8_1_D, \
	byte1(ASZ_2_BH), byte2(ASZ_2_BH), byte3(ASZ_2_BH), byte4(ASZ_2_BH), \
	ASZ_8X8_2_A, ASZ_8X8_2_B, ASZ_8X8_2_C, ASZ_8X8_2_D, \
	byte1(ASZ_3_BH), byte2(ASZ_3_BH), byte3(ASZ_3_BH), byte4(ASZ_3_BH), \
	ASZ_8X8_3_A, ASZ_8X8_3_B, ASZ_8X8_3_C, ASZ_8X8_3_D, \
	}

#ifdef STM_VL53L5_SUPPORT_LEGACY_CODE

#define ASZ_4X4_0_A  0U

#define ASZ_4X4_0_B  1U

#define ASZ_4X4_0_C  4U

#define ASZ_4X4_0_D  5U

#define ASZ_4X4_1_A  2U

#define ASZ_4X4_1_B  3U

#define ASZ_4X4_1_C  6U

#define ASZ_4X4_1_D  7U

#define ASZ_4X4_2_A  5U

#define ASZ_4X4_2_B  6U

#define ASZ_4X4_2_C  9U

#define ASZ_4X4_2_D  10U

#define ASZ_4X4_3_A  8U

#define ASZ_4X4_3_B  9U

#define ASZ_4X4_3_C  12U

#define ASZ_4X4_3_D  13U

#define ASZ_BUFFER_4X4 { \
	byte1(ASZ_0_BH), byte2(ASZ_0_BH), byte3(ASZ_0_BH), byte4(ASZ_0_BH), \
	ASZ_4X4_0_A, ASZ_4X4_0_B, ASZ_4X4_0_C, ASZ_4X4_0_D, \
	byte1(ASZ_1_BH), byte2(ASZ_1_BH), byte3(ASZ_1_BH), byte4(ASZ_1_BH), \
	ASZ_4X4_1_A, ASZ_4X4_1_B, ASZ_4X4_1_C, ASZ_4X4_1_D, \
	byte1(ASZ_2_BH), byte2(ASZ_2_BH), byte3(ASZ_2_BH), byte4(ASZ_2_BH), \
	ASZ_4X4_2_A, ASZ_4X4_2_B, ASZ_4X4_2_C, ASZ_4X4_2_D, \
	byte1(ASZ_3_BH), byte2(ASZ_3_BH), byte3(ASZ_3_BH), byte4(ASZ_3_BH), \
	ASZ_4X4_3_A, ASZ_4X4_3_B, ASZ_4X4_3_C, ASZ_4X4_3_D, \
	}
#endif

#ifdef __cplusplus
}
#endif

#endif
