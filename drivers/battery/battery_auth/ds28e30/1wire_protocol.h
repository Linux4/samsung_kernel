/*******************************************************************************
 * Copyright (C) 2011 Maxim Integrated Products, Inc., All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY,  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of Maxim Integrated shall
 * not be used except as stated in the Maxim Integrated Products, Inc. Branding Policy.
 *
 * The mere transfer of this software does not imply any licenses
 * of trade secrets, proprietary technology, copyrights, patents,
 * trademarks, maskwork rights, or any other form of intellectual
 * property whatsoever. Maxim Integrated Products, Inc. retains all ownership rights.
 *******************************************************************************
 *
 *  1wire_Protocol.h - Include file for 1wire_ds9481.c
 */

#ifndef __1WIRE_PROTOCOL_H__
#define __1WIRE_PROTOCOL_H__

#include <linux/types.h>
#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include "ds28e30.h"
#include "sec_auth_ds28e30.h"

// definitions
#ifndef TRUE
#define TRUE    1
#endif
#ifndef FALSE
#define FALSE   0
#endif

#define WRITE_FUNCTION                 0
#define READ_FUNCTION                  1

#define MODE_STANDARD                  0x00
#define MODE_OVERDRIVE                 0x01


// mode bit flags
#define MODE_NORMAL                    0x00
#define MODE_OVERDRIVE                 0x01
#define MODE_STRONG5                   0x02
#define MODE_PROGRAM                   0x04
#define MODE_BREAK                     0x08


#define GPIO_DIRECTION_IN (0u)  /*!< From the host perspective, GPIO In direction */
#define GPIO_DIRECTION_OUT (1u) /*!< From the host perspective, GPIO Out direction */
#define GPIO_INPUT	0
#define GPIO_OUTPUT	1
#define GPIO_LEVEL_HIGH (1u) /*!< GPIO logic high */
#define GPIO_LEVEL_LOW (0u)  /*!< GPIO logic low */

void GPIO_init(void);
uint8_t GPIO_read(void);
void GPIO_write(uint8_t level);
void GPIO_set_dir(uint8_t dir);

#if defined(CONFIG_ENG_BATTERY_CONCEPT)
void GPIO_toggle(void);
#endif

// Basic 1-Wire functions
int  ow_reset(void);
void write_byte(unsigned char byte_value);
void write_bit(unsigned char bit_value);
unsigned char read_bit(void);
unsigned char read_byte(void);
//int OWBlock(unsigned char *tran_buf, int tran_len);
int OWReadROM(void);
int OWSkipROM(void);
void set_1wire_ow_gpio(struct sec_auth_ds28e30_platform_data *pdata);

extern int Active_Device;
#endif

