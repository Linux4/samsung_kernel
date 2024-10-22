#ifndef PLATFORM_HELPER_H_
#define PLATFORM_HELPER_H_

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
 * @file   helper.h
 * @date   January, 2021
 * @brief  Definition of platform functions
 */

#include "../authon_config.h"

/**
* @brief check specific conditions at runtime and cause a breakpoint to facilitate a debug.
* @param expr expression is false will cause an assert.
*/
//todo System porting required: implements platform assert, if required.
#define ASSERT(expr)  WARN_ON(expr)


#define GPIO_LEVEL_HIGH (1u) /*!< GPIO logic high */
#define GPIO_LEVEL_LOW  (0u)  /*!< GPIO logic low */

typedef enum
{
	RED = 1,
	BLUE,
	GREEN,
	YELLOW,
	MAGENTA,
	CYAN
}PRINT_COLOR;

#define HIGH_BYTE_ADDR(Addr) ((Addr >> 8u) & 0xFFu)
#define LOW_BYTE_ADDR(Addr) (Addr & 0xFFu)

#define POLY				(0x8005)
#define CRC_BYTE_LEN		(2)

/* CRC functions */
uint16_t crc16_gen(uint8_t *data_p, uint16_t len);

void delay_ms(uint32_t uw_time_ms);

/* print functions */
void print_info(void);
void handle_error(void);
char *errorstring(uint16_t error_code);
void PRINT_C(PRINT_COLOR COLOR, char *string,...);
void PRINT(const char *string,...);
void PRINT_L(const char *string,...);
void cls_screen(void);
void print_row_UID_table(uint8_t *UID);
void print_HA_parameter(uint8_t *parameter);
void print_odc(uint32_t *ODC);
void print_puk(uint32_t *PUK);
void print_nvm_userpage(uint8_t *page, uint8_t count);
void print_nvm_TMpage(uint8_t *page, uint16_t count);
void print_nvm_page(uint8_t *page, uint8_t pagenumber);
void print_nvm_page_with_offset(uint8_t *data, uint8_t pagenumber, uint8_t offset);
void print_array(uint8_t* prgbBuf, uint16_t wLen);
void print_msg_digest(uint8_t* data, uint8_t len);
void PrintByteArrayHex(uint8_t *data,  uint16_t ArrayLen);

#if (ENABLE_SWI_TAU_CALIBRATION==1)
void swi_tau_calibration(void);
#endif

#endif /* PLATFORM_HELPER_H_ */
