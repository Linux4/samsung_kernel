#ifndef ECC_H_
#define ECC_H_
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
 * @file   ecc.h
 * @date   January, 2021
 * @brief  Implementation of ECC routine
 */
#include "../authon_api.h"

#define  MAC_BYTE_LEN 			(10)

#define ODC_BYTE_LEN			(50)
#define HASH_BYTE_LEN			(32)
#define PUBLICKEY_BYTE_LEN		(22)
#define ECC_CHALLENGE_LEN		(21)
#define ECC_RESPONSE_LEN		(44)

#define ECC_FIXED_WAIT          (120)
#define ECC_RETRY_COUNT         (100)
#define MAC_RETRY_COUNT         (100)

#define INTERRUPT_START_DELAY    (5)
#define MAC_RETRY_DELAY          (2)

uint16_t Send_SWI_ChallengeAndGetResponse( uint8_t *gf2n_Challenge, uint8_t *ecc_Response,  uint8_t ecc_key_set, EVENT_HANDLE event_handle);

#endif /* ECC_H_ */
