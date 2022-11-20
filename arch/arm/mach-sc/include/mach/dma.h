/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MACH_DMA_H
#define __MACH_DMA_H


#if defined(CONFIG_ARCH_SC8825) || defined(CONFIG_ARCH_SC7710)
#define DMA_VER_R1P0
#include <mach/dma_r1p0.h>

#define DMA_UID_SOFTWARE                0
#define DMA_UART0_TX                    1
#define DMA_UART0_RX                    2
#define DMA_UART1_TX                    3
#define DMA_UART1_RX                    4
#define DMA_UART2_TX                    5
#define DMA_UART2_RX                    6
#define DMA_IIS_TX                      7
#define DMA_IIS_RX                      8
#define DMA_EPT_RX                      9
#define DMA_EPT_TX                      10
#define DMA_VB_DA0                      11
#define DMA_VB_DA1                      12
#define DMA_VB_AD0                      13
#define DMA_VB_AD1                      14
#define DMA_SIM0_TX                     15
#define DMA_SIM0_RX                     16
#define DMA_SIM1_TX                     17
#define DMA_SIM1_RX                     18
#define DMA_SPI0_TX                     19
#define DMA_SPI0_RX                     20
#define DMA_ROT                         21
#define DMA_SPI1_TX                     22
#define DMA_SPI1_RX                     23
#define DMA_IIS1_TX                     24
#define DMA_IIS1_RX                     25
#define DMA_NFC_TX                      26
#define DMA_NFC_RX                      27
#define DMA_DRM_RAW                     29
#define DMA_DRM_CPT                     30

#define DMA_UID_MIN                     0
#define DMA_UID_MAX                     32

#endif


#ifdef CONFIG_ARCH_SCX35
#define DMA_VER_R4P0
#include <mach/dma_r4p0.h>
/*DMA Request ID def*/
#define DMA_SIM_RX		1
#define DMA_SIM_TX		2
#define DMA_IIS0_RX		3
#define DMA_IIS0_TX		4
#define DMA_IIS1_RX		5
#define DMA_IIS1_TX		6
#define DMA_IIS2_RX             7
#define DMA_IIS2_TX             8
#define DMA_IIS3_RX             9
#define DMA_IIS3_TX             10
#define DMA_SPI0_RX             11
#define DMA_SPI0_TX             12
#define DMA_SPI1_RX             13
#define DMA_SPI1_TX             14
#define DMA_SPI2_RX		15
#define DMA_SPI2_TX             16
#define DMA_UART0_RX		17
#define DMA_UART0_TX		18
#define DMA_UART1_RX		19
#define DMA_UART1_TX		20
#define DMA_UART2_RX		21
#define DMA_UART2_TX		22
#define DMA_UART3_RX		23
#define DMA_UART3_TX		24
#define DMA_UART4_RX		25
#define DMA_UART4_TX		26
#define DMA_DRM_CPT		27
#define DMA_DRM_RAW		28
#define DMA_VB_DA0		29
#define DMA_VB_DA1		30
#define DMA_VB_AD0		31
#define DMA_VB_AD1		32
#define DMA_VB_AD2		33
#define DMA_VB_AD3		34
#define DMA_GPS			35

#ifdef CONFIG_ARCH_SCX30G
#define DMA_SDIO0_RD		36
#define DMA_SDIO0_WR		37
#define DMA_SDIO1_RD		38
#define DMA_SDIO1_WR		39
#define DMA_SDIO2_RD		40
#define DMA_SDIO2_WR		41
#define DMA_EMMC_RD		42
#define DMA_EMMC_WR		43
#endif

#define DMA_IIS_RX	DMA_IIS0_RX
#define DMA_IIS_TX	DMA_IIS0_TX

#endif

#endif
