/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 * Author: Andrzej Hajda <a.hajda@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Device Tree binding constants for emulator clock controller.
 */

#ifndef _DT_BINDINGS_CLOCK_EMULATOR_H
#define _DT_BINDINGS_CLOCK_EMULATOR_H

#define CLK_FIN_PLL     1
#define CLK_UART_BAUD0  2
#define CLK_GATE_PCLK0  3
#define CLK_GATE_PCLK1  4
#define CLK_GATE_PCLK2  5
#define CLK_GATE_PCLK3  6
#define CLK_GATE_PCLK4  7
#define CLK_GATE_PCLK5  8
#define CLK_GATE_UART0  9
#define CLK_GATE_UART1  10
#define CLK_GATE_UART2  11
#define CLK_GATE_UART3  12
#define CLK_GATE_UART4  13
#define CLK_GATE_UART5  14
#define CLK_UART0       15
#define CLK_UART1       16
#define CLK_UART2       17
#define CLK_UART3       18
#define CLK_UART4       19
#define CLK_UART5       20
#define CLK_MCT         21

/* must be greater than maximal clock id */
#define CLK_NR_CLKS     22

#endif
