/*
 * Copyright 2013 Marvell Tech. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef __DTS_PXA1928_CONCORD_PINFUNC_H
#define __DTS_PXA1928_CONCORD_PINFUNC_H

#include "pxa1928-pinfunc.h"

#ifndef PXA1928_DISCRETE

/* UART 0 */
#define	UART0_RXD			P0 0x6
#define UART0_TXD			P1 0x6
#define UART0_CTS			P2 0x6
#define UART0_RTS			P3 0x6

/* UART 3 */
#define UART3_TXD			P47 0x2
#define UART3_RXD			P48 0x2
#define UART3_CTS			P49 0x2
#define UART3_RTS			P50 0x2

/* Camera */
#define CAM_MCLK			P14 0x3

/* Keyboard */
#define KP_DKIN0			P16 0x1
#define KP_DKIN1			P17 0x1

/* SSPA1 */
#define I2S_SYSCLK			P20 0x1
#define I2S_BITCLK			P21 0x1
#define I2S_SYNC			P22 0x1
#define I2S_DATA_OUT			P23 0x1
#define I2S_SDATA_IN			P24 0x1
#define I2S_SYNC_2			P25 0x6
#define I2S_BITCLK_2			P26 0x6
#define I2S_DATA_OUT_2			P27 0x6
#define I2S_SDATA_IN_2			P28 0x6

/* GSSP from CP */
#define GSSP_SFRM			P25 0x1
#define GSSP_SCLK			P26 0x1
#define GSSP_TXD			P27 0x1
#define GSSP_RXD			P28 0x1

/* I2C */
#define TWSI4_SCL			P29 0x7
#define TWSI4_SDA			P30 0x7
#define TWSI2_SCL			P18 0x4
#define TWSI2_SDA			P19 0x4
#define TWSI3_SCL_ISP			P31 0x7
#define TWSI3_SDA_ISP			P32 0x7
#define TWSI1_SCL			P43 0x6
#define TWSI1_SDA			P44 0x6
#define TWSI3_SDA			P45 0x6
#define TWSI3_SCL			P46 0x6

/* PWM2 */
#define PWM2				P51 0x2

/* MMC1 */
#define MMC1_CD_N			P100 0x7

/* GPIO */
#define TP_RESET			2
#define NFC_IRQ				3
#define FLASH_EN			5
#define BACKLIGHT_EN			6
#define PRESSURE_DRDY1			7
#define PRESSURE_DRDY2			8

#define P_5V_EN				10
#define GSEN_DRDY			11
#define CAM_MAIN_PWDN_N			12
#define CAM_SEC_PWDN_N			13

#define DKIN0				15
#define DKIN1				17

#define TWSI2_SCL_GPIO			18
#define TWSI2_SDA_GPIO			19
#define TWSI4_SCL_GPIO			29
#define TWSI4_SDA_GPIO			30
#define TWSI3_SCL_ISP_GPIO		31
#define TWSI3_SDA_ISP_GPIO		32
#define TWSI1_SCL_GPIO			43
#define TWSI1_SDA_GPIO			44
#define TWSI3_SDA_GPIO			45
#define TWSI3_SCL_GPIO			46


#define TP_INT				52
#define GPS_ON_OFF			53

#ifdef PXA1928_FF
#define VIBRATOR_TRIGGER		112
#define TORCH_EN			133
#else
#define TORCH_EN			4
#endif

#define CAMERA_DVDD_SUPPLY		buck3

#else	/* PXA1928_DISCRETE */

/* UART 1 */
#define	UART0_RXD			P145 0x6
#define UART0_TXD			P146 0x6
#define UART0_CTS			P147 0x6
#define UART0_RTS			P148 0x6

/* UART 4 */
#define UART3_TXD			P182 0x2
#define UART3_RXD			P183 0x2
#define UART3_CTS			P184 0x2
#define UART3_RTS			P185 0x2

/* Camera */
#define CAM_MCLK			P159 0x3

/* Keyboard */
#define KP_DKIN0			P161 0x1
#define KP_DKIN1			P162 0x1

/* SSPA1 */
#define I2S_SYSCLK			P165 0x1
#define I2S_BITCLK			P166 0x1
#define I2S_SYNC			P167 0x1
#define I2S_DATA_OUT			P168 0x1
#define I2S_SDATA_IN			P169 0x1
#define I2S_SYNC_2			P170 0x6
#define I2S_BITCLK_2			P171 0x6
#define I2S_DATA_OUT_2			P172 0x6
#define I2S_SDATA_IN_2			P173 0x6

/* GSSP from CP */
#define GSSP_SFRM			P170 0x1
#define GSSP_SCLK			P171 0x1
#define GSSP_TXD			P172 0x1
#define GSSP_RXD			P173 0x1

/* I2C */
#define TWSI4_SCL			P174 0x7
#define TWSI4_SDA			P175 0x7
#define TWSI2_SCL			P163 0x4
#define TWSI2_SDA			P164 0x4
#define TWSI3_SCL_ISP			P176 0x7
#define TWSI3_SDA_ISP			P177 0x7
#define TWSI1_SCL			P178 0x6
#define TWSI1_SDA			P179 0x6
#define TWSI3_SDA			P180 0x6
#define TWSI3_SCL			P181 0x6

/* PWM2 */
#define PWM2				P186 0x2

/* MMC1 */
#define MMC1_CD_N			P100 0x7

/* GPIO */
#define TP_RESET			147
#define NFC_IRQ				148
#define TORCH_EN			149
#define FLASH_EN			150
#define BACKLIGHT_EN			151
#define PRESSURE_DRDY1			152
#define PRESSURE_DRDY2			153

#define P_5V_EN				155
#define GSEN_DRDY			156
#define CAM_MAIN_PWDN_N			157
#define CAM_SEC_PWDN_N			158

#define DKIN0				160
#define DKIN1				162
#define TWSI2_SCL_GPIO			163
#define TWSI2_SDA_GPIO			164
#define TWSI4_SCL_GPIO			174
#define TWSI4_SDA_GPIO			175
#define TWSI3_SCL_ISP_GPIO		176
#define TWSI3_SDA_ISP_GPIO		177

#define TWSI1_SCL_GPIO			178
#define TWSI1_SDA_GPIO			179
#define TWSI3_SDA_GPIO			180
#define TWSI3_SCL_GPIO			181

#define TP_INT				187
#define GPS_ON_OFF			188

#define CAMERA_DVDD_SUPPLY		ldo19

#endif	/* PXA1928_DISCRETE */

#define TWSI5_SCL			P35 0x5
#define TWSI5_SDA			P36 0x5

#define TWSI5_SCL_GPIO			35
#define TWSI5_SDA_GPIO			36
#define PWR_SCL_GPIO			67
#define PWR_SDA_GPIO			68
#define PWR_SCL				P67 0x0
#define PWR_SDA				P68 0x0

#define PWR_CP_SCL			P67 0x2
#define PWR_CP_SDA			P68 0x2

/* UART 2 */
#define UART2_RXD_GPIO			33
#define UART2_RXD			P33 0x7
#define UART2_TXD			P34 0x7

/* PWM3 */
#define PWM3				P76 0x3

/* SSP3 */
#define SSP3_CLK			P114 0x2
#define SSP3_FRM			P115 0x2
#define SSP3_TXD			P116 0x2
#define SSP3_RXD			P117 0x2

/* MISC */
#define SLAVE_RESET_OUT			P73 0x0
#define CLK_REQ				P74 0x0
#define SLEEP_IND			P76 0x0
#define VCXO_REQ			P77 0x0
#define VCXO_OUT			P78 0x0
#define MN_CLK_OUT			P79 0x0

/* DVC */
#define DVC00				P107 0x7
#define DVC01				P108 0x7
#define DVC02				P99 0x7
#define DVC03				P103 0x7

/* MMC1(SD) */
#define MMC1_DAT7			P55 0x0
#define MMC1_DAT6			P56 0x0
#define MMC1_DAT5			P57 0x0
#define MMC1_DAT4			P58 0x0
#define MMC1_DAT3			P59 0x0
#define MMC1_DAT2			P60 0x0
#define MMC1_DAT1			P61 0x0
#define MMC1_DAT0			P62 0x0
#define MMC1_CMD			P63 0x0
#define MMC1_CLK			P64 0x0
#define MMC1_WP				P66 0x0

/* MMC2(SDIO) */
#define MMC2_DAT3			P37 0x0
#define MMC2_DAT2			P38 0x0
#define MMC2_DAT1			P39 0x0
#define MMC2_DAT0			P40 0x0
#define MMC2_CMD			P41 0x0
#define MMC2_CLK			P42 0x0

/* MMC3(eMMC) */
#define MMC3_DAT7			P80 0x1
#define MMC3_DAT6			P81 0x1
#define MMC3_DAT5			P82 0x1
#define MMC3_DAT4			P83 0x1
#define MMC3_DAT3			P84 0x1
#define MMC3_DAT2			P85 0x1
#define MMC3_DAT1			P86 0x1
#define MMC3_DAT0			P87 0x1
#define MMC3_CLK			P88 0x1
#define MMC3_CMD			P89 0x1
#define MMC3_RST			P90 0x1

/* GPIO */
#define MMC2_DAT1_GPIO			39
#define BAT_IRQ				75

#define NFC_EN				112

#define WIFI_RST_N			118
#define WIFI_PD_N			119

#define CAM2_RST_N			111
#define CAM1_RST_N			113

#define LCD_RESET_N			121
#define GPS_RST_N			122
#define ALS_INT				132

#endif /* __DTS_PXA1928_CONCORD_PINFUNC_H */
