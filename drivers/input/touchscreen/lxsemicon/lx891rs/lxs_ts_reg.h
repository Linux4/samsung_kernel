/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * lxs_ts_reg.h
 *
 * LXS touch register
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LXS_TS_REG_H_
#define _LXS_TS_REG_H_

#define CHIP_ID				"9112"
#define CHIP_DEVICE_NAME	"SW89112"

#define CHIP_VCHIP			16
#define CHIP_VPROTO			4

#define INFO_PTR					(0xFFFF)

#define SPR_CHK						(0x022)

#define SPR_CHIP_TEST				(0x0A4)

#define SPR_CHIP_ID					(0x100)

#define SPR_RST_CTL					(0x082)

#define SPR_BOOT_CTL				(0x004)

#define SPR_SRAM_CTL				(0x005)

#define SPR_SUBDISP_ST				(0x110)

#define SERIAL_CODE_OFFSET			(0x020)
#define CODE_BASE_ADDR				(0xFD0)

#define SERIAL_DATA_OFFSET			(0x025)
#define DATA_BASE_ADDR				(0xFD1)

#define IC_STATUS					(0x200)
#define TC_STATUS					(0x201)
#define TC_DBG_REPORT				(0x23E)

#define TC_MODE_SIG					(0x250)
#define MODE_SIG_NP					(0x3355584C)
#define MODE_SIG_LP					(0x3055584C)
#define MODE_SIG_LP_PROX1			(0x3150584C)
#define MODE_SIG_LP_PROX3			(0x3350584C)

#define TC_VERSION					(0x282)
#define TC_DEV_CODE					(0x283)
#define TC_PRODUCT_ID1				(0x284)
#define TC_PRODUCT_ID2				(0x285)

#define INFO_CHIP_VERSION			(0x001)

#define TC_DEVICE_CTL				(0xC00)
#define TC_DRIVE_OPT1				(0xC01)
#define TC_DRIVE_OPT2				(0xC02)
#define TC_DRIVE_CTL				(0xC03)

#define KNOCKON_ENABLE				(0xD10)
#define SWIPE_ENABLE				(0xD11)

#define DEAD_ZONE_MODE				(0xD12)	//GRAB_MODE
#define SMARTVIEW_MODE				(0xD13)
#define DEX_MODE					(0xD14)
#define COVER_MODE					(0xD15)
#define GLOVE_MODE					(0xD16)
#define GAME_MODE					(0xD17)
#define NOTE_MODE					(0xD18)
#define PROX_MODE					(0xD19)	//Call info
#define PROX_LP_SCAN				(0xD1A)

#define CHARGER_INFO				(0xD20)
#define DISPLAY_INFO				(0xD21)
#define SIP_MODE					(0xD22)	//IME
#define RES_INFO					(0xD23)
#define CHANNEL_INFO				(0xD24)
#define FRAME_INFO					(0xD25)

#define TC_TSP_TEST_CTL				(0xC04)
#define TC_TSP_TEST_STS				(0x26C)
#define TC_TSP_TEST_RESULT			(0x26D)

/* TBD */
#define INFO_LCM_TYPE				(0x29C)
#define INFO_LOT_NUM				(0x29D)
#define INFO_FPC_TYPE				(0x29E)
#define INFO_DATE					(0x29F)
#define INFO_TIME					(0x2A0)

/* TBD */
#define PRD_IC_START_REG			(0xD0C)
#define PRD_IC_READYSTATUS			(0xD04)

#define SENSITIVITY_GET				(0xD54)
#define SENSITIVITY_DATA			(0xD55)

#define SIZEOF_FW					(84<<10)

/* TBD */
#define FW_BOOT_LOADER_INIT			(0x74696E69)	//"init"
#define FW_BOOT_LOADER_CODE			(0x544F4F42)	//"BOOT"

#define BOOT_CODE_ADDR				(0x0BD)

#define GDMA_SADDR					(0x091)
#define GDMA_DADDR					(0x090)
#define GDMA_CTRL					(0x093)

#define GDMA_START					(0x0C3)
#define GDMA_CLR					(0x0C4)
#define GDMA_SR						(0x118)

#define GDMA_CTRL_CON				((SIZEOF_FW>>2) | BIT(16) | BIT(19) | BIT(20))

#define SRAM_CRC_RESULT				(0x130)
#define SRAM_CRC_PASS				(0x131)

#define CRC_FIXED_VALUE				(0x800D800D)

#define LP_DUMP_BUF					(0x23CD)	//(0x2306)
#define LP_DUMP_CHK					(0xD2E)

#endif /* _LXS_TS_REG_H_ */

