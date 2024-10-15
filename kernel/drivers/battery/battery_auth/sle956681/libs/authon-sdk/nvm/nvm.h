#ifndef NVM_NVM_H_
#define NVM_NVM_H_
/**
 * \copyright
 * MIT License
 *
 * Copyright (c) 2024 Infineon Technologies AG
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE
 *
 * \endcopyright
 */
/**
 * @file   nvm.h
 * @date   January, 2021
 * @brief  Implementation of NVM definition
 */
#include "../../authon-sdk/authon_api.h"
#include "../../authon-sdk/authon_config.h"

#define NVM_ENDURANCE                    500000  /*!  NVM Endurance write cycle */
#define NVM_PROGRAMMING_TIME             (7)     /*!Max. NVM programming time */
#define MACCR_PROGRAMMING_TIME           (43)     /*!Max. MACCR programming time */
#define MACK_PROGRAMMING_TIME            (20)     /*!Max. MACK programming time */

#define PAGE_SIZE_BYTE                   (4u)    /*!Each page is 4 bytes */
#define ON_USR_NVM_PAGE_SIZE			 (64u)   /*!  64 pages of User NVM for 2Kbits NVM */

#if (ON_USR_NVM_PAGE_SIZE!=64)
#pragma message("Error: Invalid NVM size")
#endif

#endif /* NVM_NVM_H_ */
