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
 * @file   authon_config.h
 * @date   January, 2021
 * @brief  SDK configuration
 *
 */
#ifndef AUTHON_CONFIG_H_
#define AUTHON_CONFIG_H_
//todo System porting required: implements platform header for assert, if required.
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/string.h>

/**
* @brief Authenticate On default device address
*/
#define ON_DEFAULT_ADDRESS          (0x30)

/**
* @brief Size of UID in bytes
*/
#define UID_SIZE_BYTE                (12)

/**
* @brief Number of expected devices on SWI bus
*/
#define	CONFIG_SWI_MAX_COUNT		 (4)     /*!  Configure the number of maximum number of devices that can be enumerated on the SWI bus. */
#if ((CONFIG_SWI_MAX_COUNT==0 ))
#pragma message("Error: Invalid device expected")
#endif

/* ECC Processing */
#define USE_ECC_INTERRUPT          (0)/*!  Set "1" to use ECC interrupt(for direct power only, cannot be used for indirect power mode). Set "0" to for fixed wait. */
/**
* @brief System configuration
*/
#define ENABLE_SWI_SEARCH           (0) /*!  Set "1" to enable SWI search algorithm for single or multiple devices on SWI bus. Set "0" to disable search on the bus(For direct single device only) */
#define ENABLE_OVERWRITE_BAUDRATE   (0) /*!  Set "1" to enable SWI baud rate to be overwritten outside of SDK. Set "0" to use default SDK baud rate. */
#define ENABLE_ASSERT               (0) /*!  Set "1" to enable debugging assert. Set "0" during production. */
#define ENABLE_DEBUG_PRINT          (1) /*! Set "1" to enable basic debug printing. Set "0" disable debug print. */
#define ENABLE_ECC_DEBUG_PRINT      (1) /*! Set "1" to enable debug ECC printing. Set "0" disable ECC debug print. */
#define ENABLE_HA_DEBUG_PRINT       (1) /*! Set "1" to enable debug HA printing. Set "0" disable HA debug print. */

/**
* @brief SWI Tau calibration
*/
//todo System porting required: implements platform SWI tau, if required.
#define ENABLE_SWI_TAU_CALIBRATION       (0) /*! Set "1" to enable SWI Tau calibration template function. Set "0" disable. */
#endif /* AUTHON_CONFIG_H_ */
