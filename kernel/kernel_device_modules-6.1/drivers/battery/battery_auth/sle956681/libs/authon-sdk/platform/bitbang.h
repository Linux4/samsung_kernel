#ifndef PLATFORM_BITBANG_H_
#define PLATFORM_BITBANG_H_
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
 * @file   bitbang.h
 * @date   January, 2021
 * @brief  Implementation of SWI bitbang API
 */
#include "../authon_config.h"

#define GPIO_DIRECTION_IN (0u)  /*!< From the host perspective, GPIO In direction */
#define GPIO_DIRECTION_OUT (1u) /*!< From the host perspective, GPIO Out direction */
#define GPIO_INPUT	0
#define GPIO_OUTPUT	1

void GPIO_init(void);
uint8_t GPIO_read(void);
void GPIO_write(uint8_t level);
void GPIO_set_dir(uint8_t dir, uint8_t output_state);

void Set_SWILowBaud(uint32_t value);
void Set_SWIHighBaud(uint32_t value);
void Set_SWIStopBaud(uint32_t value);
uint32_t Get_SWILowBaud(void);
uint32_t Get_SWIHighBaud(void);
uint32_t Get_SWIStopBaud(void);

void Set_SWITimeoutBaud(uint32_t value);
uint32_t Get_SWITimeoutBaud(void);

void Set_WakeupTime(uint32_t value);
void Set_PowerdownTime(uint32_t value);
void Set_PowerupTime(uint32_t value);
void Set_ResetTime(uint32_t value);
void Set_NVMTimeOutCount(uint32_t value);
void Set_EccRetryCount(uint32_t value);
uint32_t Get_EccRetryCount(void);

uint32_t Get_WakeupTime(void);
uint32_t Get_WakeupLowTime(void);
uint32_t Get_PowerdownTime(void);
uint32_t Get_PowerupTime(void);

uint32_t Get_NVMTimeOutCount(void);
uint32_t Get_ResetTime(void);

#endif /* PLATFORM_BITBANG_H_ */
