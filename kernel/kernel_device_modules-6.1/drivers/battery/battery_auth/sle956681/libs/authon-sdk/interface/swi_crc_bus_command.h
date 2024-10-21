#ifndef SWI_CRC_COMMAND_H_
#define SWI_CRC_COMMAND_H_
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
 * @file   swi_crc_bus_command.h
 * @date   January, 2021
 * @brief  Implementation of SWI bus command
 *
 */

#include "../authon_config.h"

void Send_EINT(uint8_t *ubIsIntReceived);
void Send_RBL(uint8_t ub_rbl_numb);
void Send_DISS(void);
void Send_DIP0(uint8_t *ubIsIntReceived);
void Send_DIP1(uint8_t *ubIsIntReceived);
void Send_DIE0(void);
void Send_DIE1(void);
void Send_DI00(uint8_t *ubIsIntReceived);
void Send_DI01(uint8_t *ubIsIntReceived);
void Send_DI10(uint8_t *ubIsIntReceived);
void Send_DI11(uint8_t *ubIsIntReceived);
void Send_EDA(uint8_t ubAddr);
void Send_SDA(uint8_t ubAddr);
void Send_ERA(uint8_t ubAddr);
void Send_WRA(uint8_t ubAddr);
uint16_t Send_RRA(uint8_t ubAddr, uint16_t rd_len, uint8_t *ub_data);
uint16_t Send_WD(uint16_t data_len, uint8_t *ub_data);
uint16_t Send_BRES(void);
uint16_t Send_PDWN(void);
uint16_t Send_HRREQ(void);
uint16_t Send_HRRES(uint8_t *ub_data);
uint16_t Send_WDA(uint16_t ubAddr);
uint16_t Send_MACS(uint16_t uw_mac_addr);
uint16_t Send_MACR(uint8_t *ub_data);
uint16_t Send_MACK(uint8_t *ub_data);
uint16_t Send_MACCR1(uint8_t *ub_data);
uint16_t Send_MACCR2(uint8_t *ub_data);
uint16_t Send_MACCR3(uint8_t *ub_data);
uint16_t Send_MACCR4(uint8_t *ub_data);
uint16_t Send_MACCR5(uint8_t *ub_data);
uint16_t Send_ECCS1(void);
uint16_t Send_ECCS2(void);
uint16_t Send_ECCC(uint8_t *ub_data);
uint16_t Send_ECCR(uint8_t *ub_data);


uint16_t Send_ECCM(uint8_t *ub_data);

//#if (HOST_AUTHENTICATION_FEATURE_SUPPORTED==1)
uint16_t Send_DRRES(uint8_t *ub_data);
uint16_t Send_HREQ1(uint8_t *ub_data);
uint16_t Send_DRES1(uint8_t *ub_data);
//#endif
uint16_t Get_error_code(uint8_t* error_code);

#endif /* SWI_CRC_COMMAND_H_ */
