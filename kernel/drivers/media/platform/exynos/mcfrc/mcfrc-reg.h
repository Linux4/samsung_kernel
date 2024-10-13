/* linux/drivers/media/platform/exynos/mcfrc/mcfrc-regs.h
 *
 * Register definition file for Samsung JPEG Squeezer driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Authors: Jungik Seo <jungik.seo@samsung.com>
 *          Igor Kovliga <i.kovliga@samsung.com>
 *          Sergei Ilichev <s.ilichev@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef MCFRC_REGS_H_
#define MCFRC_REGS_H_

#ifndef __DEBUG_WANT_ONLY_MACROSES_
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/module.h>
#include "mcfrc-core.h"
#endif

// MC ver3.0

#define CAVY_GET_MASK(hi, low)                      ( (((u32)0xffffffff)>>(31-hi)) & (((u32)0xffffffff)<<(low)) )
#define CAVY_SHIFT_CLIP_VAL(var, hi, low)           ( ((u32)(var)<<low) & CAVY_GET_MASK(hi,low) )
#define CAVY_INSERT_VAL(var, val, hi, low)          ( var = (var & (~CAVY_GET_MASK(hi, low))) | CAVY_SHIFT_CLIP_VAL(val,hi,low) )
#define CAVY_PICK_VAL(var, val, hi, low)            ( var = (val & CAVY_GET_MASK(hi,low)) >> low)

//===============================================================================================================

////    0x0000  WR      FRC_MC_OP_MODE_SEL
////            [0]   FRC_MC_OP_MODE_SEL
#define REG_TOP_0x0000_FRC_MC_OP_MODE_SEL           (0x0000)
#define SET_FRC_MC_OP_MODE_SEL_REG(base_addr, val)  writel(val, base_addr+REG_TOP_0x0000_FRC_MC_OP_MODE_SEL)
#define GET_FRC_MC_OP_MODE_SEL_REG(base_addr, val)  (val = readl(base_addr+REG_TOP_0x0000_FRC_MC_OP_MODE_SEL))

////    0x0004  WR      FRC_MC_CLKREQ_CONTROL
////            [0] FRC_MC_CLKREQ_CONTROL
#define REG_TOP_0x0004_FRC_MC_CLKREQ_CONTROL        (0x0004)
#define SET_MC_CLKREQ_CONTROL_REG(base_addr, val)   writel(val, base_addr+REG_TOP_0x0004_FRC_MC_CLKREQ_CONTROL)
#define GET_MC_CLKREQ_CONTROL_REG(base_addr, val)   (val = readl(base_addr+REG_TOP_0x0004_FRC_MC_CLKREQ_CONTROL))

////	0x0008	WR		FRC_MC_APB_PPROT
////			[0]	FRC_MC_APB_PPROT
#define REG_TOP_0x0008_FRC_MC_APB_PPROT             (0x0008)
#define SET_MC_APB_PPROT_REG(base_addr, val)        writel(val, base_addr+REG_TOP_0x0008_FRC_MC_APB_PPROT)
#define GET_MC_APB_PPROT_REG(base_addr, val)        (val = readl(base_addr+REG_TOP_0x0008_FRC_MC_APB_PPROT))

//===============================================================================================================

////    0x0200	WC		FRC_MC_START
////            WC[0]	FRC_MC_START
#define REG_TOP_0x0200_MC_START                     (0x0200)
#define SET_FRC_MC_START_REG(base_addr, val)        writel(val, base_addr+REG_TOP_0x0200_MC_START)

////    0x0204	WC		FRC_APB_UPDATE
////            WC[0]	FRC_APB_UPDATE
#define REG_TOP_0x0204_MC_APB_UPDATE                (0x0204)
#define SET_FRC_APB_UPDATE_REG(base_addr, val)      writel(val, base_addr+REG_TOP_0x0204_MC_APB_UPDATE)

////    0x0208	RW		SW Reset
////            RW[0]	MC_SW_RESET
#define REG_TOP_0x0208_MC_SW_RESET                  (0x0208)
#define SET_MC_SW_RESET_REG(base_addr, val)         writel(val, base_addr+REG_TOP_0x0208_MC_SW_RESET)
#define GET_MC_SW_RESET_REG(base_addr, val)         (val = readl(base_addr+REG_TOP_0x0208_MC_SW_RESET))

////    0x020C	RW		DMA_SW_RESET
////            RW[0]	DMA_SW_RESET
#define REG_TOP_0x020C_DMA_SW_RESET                 (0x020C)
#define SET_DMA_SW_RESET_REG(base_addr, val)        writel(val, base_addr+REG_TOP_0x020C_DMA_SW_RESET)
#define GET_DMA_SW_RESET_REG(base_addr, val)        (val = readl(base_addr+REG_TOP_0x020C_DMA_SW_RESET))

////    0x0210  WR      FRC_MC_CLKREQn_CONTROL
////            [0] FRC_MC_CLKREQn_CONTROL
#define REG_TOP_0x0210_MC_CLKREQn_CONTROL           (0x0210)
#define SET_MC_CLKREQn_CONTROL_REG(base_addr, val)   writel(val, base_addr+REG_TOP_0x0004_FRC_MC_CLKREQ_CONTROL)
#define GET_MC_CLKREQn_CONTROL_REG(base_addr, val)   (val = readl(base_addr+REG_TOP_0x0004_FRC_MC_CLKREQ_CONTROL))

////    0x0214	RW		MC_FRAME_ID
////            [31:0]	MC_FRAME_ID
#define REG_TOP_0x0214_MC_FRAME_ID                  (0x0214)
#define SET_MC_FRAME_ID_REG(base_addr, val)         writel(val, base_addr+REG_TOP_0x0214_MC_FRAME_ID)
#define GET_MC_FRAME_ID_REG(base_addr, val)         (val = readl(base_addr+REG_TOP_0x0214_MC_FRAME_ID))

////    0x0218	RW		MC_EARLY_START_EN
////            [0]	MC_EARLY_START_EN
#define REG_TOP_0x0218_MC_EARLY_START_EN            (0x0218)
#define SET_MC_EARLY_START_EN_REG(base_addr, val)   writel(val, base_addr+REG_TOP_0x0218_MC_EARLY_START_EN)

////    0x021C	RW		MC_FRAME_SIZE
////            [31:16]	MC_FRAME_WIDTH
////            [15:0]	MC_FRAME_HEIGHT
#define REG_TOP_0x021C_MC_FRAME_SIZE                (0x021C)
#define INSERT_MC_FRAME_WIDTH(var, val)             CAVY_INSERT_VAL(var, val, 31, 16)
#define INSERT_MC_FRAME_HEIGHT(var, val)            CAVY_INSERT_VAL(var, val, 15, 0)
#define SET_MC_FRAME_SIZE_REG(base_addr, val)       writel(val, base_addr+REG_TOP_0x021C_MC_FRAME_SIZE)
#define GET_MC_FRAME_SIZE_REG(base_addr, val)       (val = readl(base_addr+REG_TOP_0x021C_MC_FRAME_SIZE))

////    0x0220	RW		MC_VERSION_OP_MODE_SEL
////            [28]    MC_VERSION
////            [24]    OPERATION_MODE
////            [23]    FRAME 2 BYPASS_TYPE
////            [21:16] FRAME 2 PHASE
////            [15]    FRAME 1 BYPASS_TYPE
////            [13:8]  FRAME 1 PHASE
////            [7]     FRAME 0 BYPASS_TYPE
////            [5:0]   FRAME 0 PHASE
#define REG_TOP_0x0220_MC_VERSION_OP_MODE_SEL           (0x0220)
#define INSERT_OPERATION_MODE(var, val)                 CAVY_INSERT_VAL(var, val, 24, 24)
#define INSERT_FRAME_2_BYPASS_TYPE(var, val)            CAVY_INSERT_VAL(var, val, 23, 23)
#define INSERT_FRAME_2_PHASE(var, val)                  CAVY_INSERT_VAL(var, val, 21, 16)
#define INSERT_FRAME_1_BYPASS_TYPE(var, val)            CAVY_INSERT_VAL(var, val, 15, 15)
#define INSERT_FRAME_1_PHASE(var, val)                  CAVY_INSERT_VAL(var, val, 13, 8 )
#define INSERT_FRAME_0_BYPASS_TYPE(var, val)            CAVY_INSERT_VAL(var, val, 7 , 7 )
#define INSERT_FRAME_0_PHASE(var, val)                  CAVY_INSERT_VAL(var, val, 5 , 0 )
#define SET_MC_VERSION_OP_MODE_SEL_REG(base_addr, val)  writel(val, base_addr+REG_TOP_0x0220_MC_VERSION_OP_MODE_SEL)
#define GET_MC_VERSION_OP_MODE_SEL_REG(base_addr, val)  (val = readl(base_addr+REG_TOP_0x0220_MC_VERSION_OP_MODE_SEL))

////    0x0224	RW		MC_BATCH_MODE
////            [20]	MC_BATCH_PHASE_INFORM	"0 : SAME PHASE 1 : Different Phase per FRAME"
////            [16]	MC_BATCH_EN
////            [15:0]	MC_BATCH_NUMBER
#define REG_TOP_0x0224_MC_BATCH_MODE                    (0x0224)
#define INSERT_MC_BATCH_EN(var, val)                    CAVY_INSERT_VAL(var, val, 16, 16)
#define INSERT_MC_BATCH_NUMBER(var, val)                CAVY_INSERT_VAL(var, val, 15, 0)
#define SET_MC_BATCH_MODE_REG(base_addr, val)           writel(val, base_addr+REG_TOP_0x0224_MC_BATCH_MODE)
#define GET_MC_BATCH_MODE_REG(base_addr, val)           (val = readl(base_addr+REG_TOP_0x0224_MC_BATCH_MODE))

////    0x0228	RW		MC_BYPASS_PHASE
////            [9:8]	MC_BYPASS_TYPE
////            [5:0]	MC_FRAME_PHASE
#define REG_TOP_0x0228_MC_BYPASS_PHASE                  (0x0228)
#define INSERT_MC_BYPASS_TYPE(var, val)                 CAVY_INSERT_VAL(var, val, 9, 8)
#define INSERT_MC_FRAME_PHASE(var, val)                 CAVY_INSERT_VAL(var, val, 5, 0)
#define SET_MC_BYPASS_PHASE_REG(base_addr, val)         writel(val, base_addr+REG_TOP_0x0228_MC_BYPASS_PHASE)
#define GET_MC_BYPASS_PHASE_REG(base_addr, val)         (val = readl(base_addr+REG_TOP_0x0228_MC_BYPASS_PHASE))

////    0x022C	RW		MC_TIMEOUT
////            [31]	MC_TIMEOUT_EN
////            [23:8]	MC_TIMEOUT_THRESHOLD
////            [7:0]	MC_OUT_TILE_THRESHOLD
#define REG_TOP_0x022C_MC_TIMEOUT                       (0x022C)
#define INSERT_MC_TIMEOUT_EN(var, val)                  CAVY_INSERT_VAL(var, val, 31, 31)
#define INSERT_MC_TIMEOUT_THRESHOLD(var, val)           CAVY_INSERT_VAL(var, val, 23, 8)
#define INSERT_MC_OUT_TILE_THRESHOLD(var, val)          CAVY_INSERT_VAL(var, val, 7, 0)
#define SET_MC_TIMEOUT_REG(base_addr, val)              writel(val, base_addr+REG_TOP_0x022C_MC_TIMEOUT)
#define GET_MC_TIMEOUT_REG(base_addr, val)              (val = readl(base_addr+REG_TOP_0x022C_MC_TIMEOUT))

////    0x0230	RW		MC_FRAME_DONE_CHECK
////            RW[31]	MC_FRAME_DONE_CHECK_EN
////            RW[30:0]	MC_FRAME_DONE_COUNT_TH
#define REG_TOP_0x0230_MC_FRAME_DONE_CHECK              (0x0230)
#define INSERT_MC_FRAME_DONE_CHECK_EN(var, val)         CAVY_INSERT_VAL(var, val, 31, 31)
#define INSERT_MC_FRAME_DONE_COUNT_TH(var, val)         CAVY_INSERT_VAL(var, val, 30, 0)
#define SET_MC_FRAME_DONE_CHECK_REG(base_addr, val)     writel(val, base_addr+REG_TOP_0x0230_MC_FRAME_DONE_CHECK)
#define GET_MC_FRAME_DONE_CHECK_REG(base_addr, val)     (val = readl(base_addr+REG_TOP_0x0230_MC_FRAME_DONE_CHECK))

//===============================================================================================================

////    0x0250	WC		FRC_DBL_START
////            WC[0]	FRC_DBL_START
#define REG_TOP_0x0250_FRC_DBL_START                (0x0250)
#define SET_FRC_DBL_START_REG(base_addr, val)       writel(val, base_addr+REG_TOP_0x0250_FRC_DBL_START)

////    0x0254	RW		FRC_DBL_EN
////            [0]	FRC_DBL_EN
#define REG_TOP_0x0254_FRC_DBL_EN                   (0x0254)
#define SET_FRC_DBL_EN_REG(base_addr, val)          writel(val, base_addr+REG_TOP_0x0254_FRC_DBL_EN)
#define GET_FRC_DBL_EN_REG(base_addr, val)          (val = readl(base_addr+REG_TOP_0x0254_FRC_DBL_EN))

////    0x0258	RW		FRC_DBL_MODE
////            [1:0]	FRC_DBL_MODE
#define REG_TOP_0x0258_FRC_DBL_MODE                 (0x0258)
#define FRC_DBL_MODE_PARALLEL                       0x0
#define FRC_DBL_MODE_SERIAL                         0x2
#define FRC_DBL_MODE_MANUAL                         0x3
#define SET_FRC_DBL_MODE_REG(base_addr, val)        writel(val, base_addr+REG_TOP_0x0258_FRC_DBL_MODE)
#define GET_FRC_DBL_MODE_REG(base_addr, val)        (val = readl(base_addr+REG_TOP_0x0258_FRC_DBL_MODE))

////    0x0270	RW		MC_AXI_USER
////            RW[7:4]	MC_AXI_AWUSER
////            RW[3:0]	MC_AXI_ARUSER
#define REG_TOP_0x0270_MC_AXI_USER                  (0x0270)
#define INSERT_MC_AXI_AWUSER(var, val)              CAVY_INSERT_VAL(var, val, 7, 4)
#define INSERT_MC_AXI_ARUSER(var, val)              CAVY_INSERT_VAL(var, val, 3, 0)
#define SET_MC_AXI_USER_REG(base_addr, val)         writel(val, base_addr+REG_TOP_0x0270_MC_AXI_USER)
#define GET_MC_AXI_USER_REG(base_addr, val)         (val = readl(base_addr+REG_TOP_0x0270_MC_AXI_USER))

////    0x0274	RW		DMA_SBI_RD_CONFIG
////            RW[16]	SBI_RD_STOP_REQ
////            RW[15:0]	SBI_RD_CONFIG
#define REG_TOP_0x0274_DMA_SBI_RD_CONFIG            (0x0274)
#define INSERT_SBI_RD_STOP_REQ(var, val)            CAVY_INSERT_VAL(var, val, 16, 16)
#define INSERT_SBI_RD_CONFIG(var, val)              CAVY_INSERT_VAL(var, val, 15, 0)
#define SET_DMA_SBI_RD_CONFIG_REG(base_addr, val)   writel(val, base_addr+REG_TOP_0x0274_DMA_SBI_RD_CONFIG)
#define GET_DMA_SBI_RD_CONFIG_REG(base_addr, val)   (val = readl(base_addr+REG_TOP_0x0274_DMA_SBI_RD_CONFIG))

////    0x0278	RO		DMA_SBI_RD_STATUS
////            RO[16]	SBI_RD_STOP_ACK
////            RO[15:0]	SBI_RD_STATUS
#define REG_TOP_0x0278_DMA_SBI_RD_STATUS            (0x0278)
#define PICK_SBI_RD_STOP_ACK(var, val)              CAVY_PICK_VAL(var, val, 16, 16)
#define PICK_SBI_RD_STATUS(var, val)                CAVY_PICK_VAL(var, val, 15, 0)
#define GET_DMA_SBI_RD_STATUS_REG(base_addr, val)   (val = readl(base_addr+REG_TOP_0x0278_DMA_SBI_RD_STATUS))

////    0x027C	RW		DMA_SBI_WR_CONFIG
////            RW[16]	SBI_WR_STOP_REQ
////            RW[15:0]	SBI_WR_CONFIG
#define REG_TOP_0x027C_DMA_SBI_WR_CONFIG            (0x027C)
#define INSERT_SBI_WR_STOP_REQ(var, val)            CAVY_INSERT_VAL(var, val, 16, 16)
#define INSERT_SBI_WR_CONFIG(var, val)              CAVY_INSERT_VAL(var, val, 15, 0)
#define SET_DMA_SBI_WR_CONFIG_REG(base_addr, val)   writel(val, base_addr+REG_TOP_0x027C_DMA_SBI_WR_CONFIG)
#define GET_DMA_SBI_WR_CONFIG_REG(base_addr, val)   (val = readl(base_addr+REG_TOP_0x027C_DMA_SBI_WR_CONFIG))

////    0x0280	RO		DMA_SBI_WR_STATUS
////            RO[16]	SBI_WR_STOP_ACK
////            RO[15:0]	SBI_WR_STATUS
#define REG_TOP_0x0280_DMA_SBI_WR_STATUS            (0x0280)
#define PICK_SBI_WR_STOP_ACK(var, val)              CAVY_PICK_VAL(var, val, 16, 16)
#define PICK_SBI_WR_STATUS(var, val)                CAVY_PICK_VAL(var, val, 15, 0)
#define GET_DMA_SBI_WR_STATUS_REG(base_addr, val)   (val = readl(base_addr+REG_TOP_0x0280_DMA_SBI_WR_STATUS))

////    0x0290	RW		MC_INTR_MASKING
////            [16]	o_FRC_MC_DONE_IRQ' Mask
////            [12]	o_FRC_MC_DBL_ERR_IRQ' Fuction All Mask
////            [8]	└ SRST_DONE_IRQ Mask
////            [4]	└ FRC_DBL_M_DONE_IRQ Mask
////            [0]	└ FRC_MC_ERROR_IRQ Mask
#define REG_TOP_0x0290_MC_INTR_MASKING              (0x0290)
#define INSERT_FRC_MC_DONE_IRQ(var, val)            CAVY_INSERT_VAL(var, val, 16, 16)
#define INSERT_FRC_MC_DBL_ERR_IRQ(var, val)         CAVY_INSERT_VAL(var, val, 12, 12)
#define INSERT_SRST_DONE_IRQ(var, val)              CAVY_INSERT_VAL(var, val, 8 , 8 )
#define INSERT_FRC_DBL_M_DONE_IRQ(var, val)         CAVY_INSERT_VAL(var, val, 4 , 4 )
#define INSERT_FRC_MC_ERROR_IRQ(var, val)           CAVY_INSERT_VAL(var, val, 0 , 0 )
#define SET_MC_INTR_MASKING_REG(base_addr, val)     writel(val, base_addr+REG_TOP_0x0290_MC_INTR_MASKING)
#define GET_MC_INTR_MASKING_REG(base_addr, val)     (val = readl(base_addr+REG_TOP_0x0290_MC_INTR_MASKING))

////    0x0294	RO		INTR_MASK_DBG_STATUS
////            [24]	FRC_MC_FR_STARTED_IRQ
////            [20]	FRC_MC_SRST_DONE_IRQ
////            [16]	FRC_DBL_M_DONE_IRQ
////            [12]	FRC_MC_ERROR_IRQ
////            [8]	FRC_MC_DONE_FB_IRQ
////            [4]	FRC_MC_DONE_TIME_IRQ
////            [0]	FRC_MC_DONE_NORM_IRQ
#define REG_TOP_0x0294_INTR_MASK_DBG_STATUS             (0x0294)
#define PICK_DBG_FRC_MC_FR_STARTED_IRQ(var, val)        CAVY_PICK_VAL(var, val, 24, 24)
#define PICK_DBG_FRC_MC_SRST_DONE_IRQ(var, val)         CAVY_PICK_VAL(var, val, 20, 20)
#define PICK_DBG_FRC_DBL_M_DONE_IRQ(var, val)           CAVY_PICK_VAL(var, val, 16, 16)
#define PICK_DBG_FRC_MC_ERROR_IRQ(var, val)             CAVY_PICK_VAL(var, val, 12, 12)
#define PICK_DBG_FRC_MC_DONE_FB_IRQ(var, val)           CAVY_PICK_VAL(var, val, 8 , 8 )
#define PICK_DBG_FRC_MC_DONE_TIME_IRQ(var, val)         CAVY_PICK_VAL(var, val, 4 , 4 )
#define PICK_DBG_FRC_MC_DONE_NORM_IRQ(var, val)         CAVY_PICK_VAL(var, val, 0 , 0 )
#define GET_INTR_MASK_DBG_STATUS_REG(base_addr, val)    (val = readl(base_addr+REG_TOP_0x0294_INTR_MASK_DBG_STATUS))

////    0x0298	WC		FRC_MC_DONE_INTR_CLEAR
////            WC[0]	FRC_MC_DONE_INTR_CLEAR
#define REG_TOP_0x0298_FRC_MC_DONE_INTR_CLEAR           (0x0298)
#define SET_FRC_MC_DONE_INTR_CLEAR_REG(base_addr, val)  writel(val, base_addr+REG_TOP_0x0298_FRC_MC_DONE_INTR_CLEAR)

////    0x029C	RO		FRC_MC_DONE_IRQ_STATUS
////            [8]	FRC_MC_DONE_FB_IRQ
////            [4]	FRC_MC_DONE_TIME_IRQ
////            [0]	FRC_MC_DONE_NORM_IRQ
#define REG_TOP_0x029C_FRC_MC_DONE_IRQ_STATUS           (0x029C)
#define IRQ_MC_DONE_FB                                  (0x1<<8)
#define IRQ_MC_DONE_TIME                                (0x1<<4)
#define IRQ_MC_DONE_NORM                                (0x1<<0)
#define PICK_FRC_MC_DONE_FB_IRQ(var, val)               CAVY_PICK_VAL(var, val, 8 , 8 )
#define PICK_FRC_MC_DONE_TIME_IRQ(var, val)             CAVY_PICK_VAL(var, val, 4 , 4 )
#define PICK_FRC_MC_DONE_NORM_IRQ(var, val)             CAVY_PICK_VAL(var, val, 0 , 0 )
#define GET_FRC_MC_DONE_IRQ_STATUS_REG(base_addr, val)  (val = readl(base_addr+REG_TOP_0x029C_FRC_MC_DONE_IRQ_STATUS))

////    0x02A0  WC		FRC_MC_DBL_ERR_INTR_CLEAR
////            [12]	FRC_MC_FR_STARTED_CLEAR
////            [0] 	FRC_DBL_M_DONE_INTR_CLEAR
#define REG_TOP_0x02A0_FRC_MC_DBL_ERR_INTR_CLEAR            (0x02A0)
#define INSERT_FRC_MC_FR_STARTED_CLEAR(var, val)            CAVY_INSERT_VAL(var, val, 12, 12)
#define INSERT_FRC_DBL_M_DONE_INTR_CLEAR(var, val)          CAVY_INSERT_VAL(var, val, 0, 0)
#define SET_FRC_MC_DBL_ERR_INTR_CLEAR_REG(base_addr, val)   writel(val, base_addr+REG_TOP_0x02A0_FRC_MC_DBL_ERR_INTR_CLEAR)

////    0x02A4	RO		FRC_MC_DBL_ERR_IRQ_STATUS
////            [8]	FRC_MC_SRST_DONE_IRQ
////            [4]	FRC_DBL_M_DONE_IRQ
////            [0]	FRC_MC_ERROR_IRQ
#define REG_TOP_0x02A4_FRC_MC_DBL_ERR_IRQ_STATUS            (0x02A4)
#define IRQ_ERRRST_STATE_RST                                (0x1<<8)
#define IRQ_ERRRST_STATE_DMAN                               (0x1<<4)
#define IRQ_ERRRST_STATE_TERR                               (0x1<<0)
#define PICK_FRC_MC_SRST_DONE_IRQ(var, val)                 CAVY_PICK_VAL(var, val, 8 , 8 )
#define PICK_FRC_DBL_M_DONE_IRQ(var, val)                   CAVY_PICK_VAL(var, val, 4 , 4 )
#define PICK_FRC_MC_ERROR_IRQ(var, val)                     CAVY_PICK_VAL(var, val, 0 , 0 )
#define GET_FRC_MC_DBL_ERR_IRQ_STATUS_REG(base_addr, val)   (val = readl(base_addr+REG_TOP_0x02A4_FRC_MC_DBL_ERR_IRQ_STATUS))

////    0x02A8	RW		MC_ERROR_DEBUG_ENABLE
////            RW[12]	DEBUG_MONITORING_EN
////            RW[8]	DEBUG_APB_READ_SEL
////            RW[3:0]	MC_ERROR_ENABLE
#define REG_TOP_0x02A8_MC_ERROR_DEBUG_ENABLE                (0x02A8)
#define INSERT_DEBUG_MONITORING_EN(var, val)                CAVY_INSERT_VAL(var, val, 12, 12)
#define INSERT_DEBUG_APB_READ_SEL(var, val)                 CAVY_INSERT_VAL(var, val, 8, 8)
#define INSERT_MC_ERROR_ENABLE(var, val)                    CAVY_INSERT_VAL(var, val, 3, 0)
#define SET_MC_ERROR_DEBUG_ENABLE_REG(base_addr, val)       writel(val, base_addr+REG_TOP_0x02A8_MC_ERROR_DEBUG_ENABLE)
#define GET_MC_ERROR_DEBUG_ENABLE_REG(base_addr, val)       (val = readl(base_addr+REG_TOP_0x02A8_MC_ERROR_DEBUG_ENABLE))

////    0x02AC	WC		MC_ERROR_CLEAR
////            [29:24]	RDMA_ERROR_CLEAR
////            [19:16]	WDMA_ERROR_CLEAR
////            [12]	REG_SETTING_ERROR_CLEAR
////            [8]	DBL_CORE_ERROR_CLEAR
////            [4]	MC_CORE_ERROR_CLEAR
////            [1]	TOP_ERROR_CLEAR
////            [0]	TIMEOUT_STATUS_CLEAR
#define REG_TOP_0x02AC_MC_ERROR_CLEAR                   (0x02AC)
#define INSERT_RDMA_ERROR_CLEAR(var, val)               CAVY_INSERT_VAL(var, val, 29, 24)
#define INSERT_WDMA_ERROR_CLEAR(var, val)               CAVY_INSERT_VAL(var, val, 19, 16)
#define INSERT_REG_SETTING_ERROR_CLEAR(var, val)        CAVY_INSERT_VAL(var, val, 12,12)
#define INSERT_DBL_CORE_ERROR_CLEARR(var, val)          CAVY_INSERT_VAL(var, val, 8, 8)
#define INSERT_MC_CORE_ERROR_CLEAR(var, val)            CAVY_INSERT_VAL(var, val, 4, 4)
#define INSERT_TOP_ERROR_CLEAR(var, val)                CAVY_INSERT_VAL(var, val, 1, 1)
#define INSERT_TIMEOUT_STATUS_CLEAR(var, val)           CAVY_INSERT_VAL(var, val, 0, 0)
#define SET_MC_ERROR_CLEAR_REG(base_addr, val)          writel(val, base_addr+REG_TOP_0x02AC_MC_ERROR_CLEAR)

////    0x02B0	RO		MC_ERROR_STATUS
////            [31:16]	DMA_ERROR_STATUS
////            [15:12]	REG_SETTING_ERROR_STATUS
////            [11:8]	DBL_CORE_ERROR_STATUS
////            [7:4]	MC_CORE_ERROR_STATUS
////            [1:0]	TOP_ERROR_STATUS
#define REG_TOP_0x02B0_MC_ERROR_STATUS                  (0x02B0)
#define PICK_DMA_ERROR_STATUS(var, val)                 CAVY_PICK_VAL(var, val, 31, 16)
#define PICK_REG_SETTING_ERROR_STATUS(var, val)         CAVY_PICK_VAL(var, val, 15, 12)
#define PICK_DBL_CORE_ERROR_STATUS(var, val)            CAVY_PICK_VAL(var, val, 11, 8 )
#define PICK_MC_CORE_ERROR_STATUS(var, val)             CAVY_PICK_VAL(var, val, 7 , 4 )
#define PICK_TOP_ERROR_STATUS(var, val)                 CAVY_PICK_VAL(var, val, 1 , 0 )
#define GET_MC_ERROR_STATUS_REG(base_addr, val)         (val = readl(base_addr+REG_TOP_0x02B0_MC_ERROR_STATUS))

////    0x02D0	RO		MC_PROCESSING_TIME_STAUTS
////            [31:0]	MC_PROCESSING_TIME_CYCLES
#define REG_TOP_0x02D0_MC_PROCESSING_TIME_STATUS            (0x02D0)
#define GET_MC_PROCESSING_TIME_STATUS_REG(base_addr, val)   (val = readl(base_addr+REG_TOP_0x02D0_MC_PROCESSING_TIME_STATUS))

////    0x02D4	RO		MC_CORE_STATUS_0
////            [31:0]	MC_CORE_TOTAL_READ_REQ
#define REG_TOP_0x02D4_MC_CORE_STATUS_0                 (0x02D4)
#define GET_MC_CORE_STATUS_0_REG(base_addr, val)        (val = readl(base_addr + REG_TOP_0x02D4_MC_CORE_STATUS_0))

////    0x02D8	RO		MC_CORE_STATUS_1
////            RO[31:20]	MC_CORE_MAX_READ_REQUEST
////            RO[19:0]	MC_PROCESSED_TILE_NUM
#define REG_TOP_0x02D8_MC_CORE_STATUS_1                 (0x02D8)
#define PICK_MC_CORE_MAX_READ_REQUEST(var, val)         CAVY_PICK_VAL(var, val, 31, 20 )
#define PICK_MC_PROCESSED_TILE_NUM(var, val)            CAVY_PICK_VAL(var, val, 19, 0 )
#define GET_MC_CORE_STATUS_1_REG(base_addr, val)        (val = readl(base_addr+REG_TOP_0x02D8_MC_CORE_STATUS_1))

////    0x02DC	RO		MC_CORE_STATUS_2
////            [29:28]	MC_INFORM
////            [27:16]	MC_TILE_STATUS
////            [15:6]	MC_DXI_READ_STATUS
////            [5:0]	MC_DXI_WRITE_STATUS
#define REG_TOP_0x02DC_MC_CORE_STATUS_2                 (0x02DC)
#define PICK_MC_INFORM(var, val)                        CAVY_PICK_VAL(var, val, 29, 28)
#define PICK_MC_TILE_STATUS(var, val)                   CAVY_PICK_VAL(var, val, 27, 16)
#define PICK_MC_DXI_READ_STATUS(var, val)               CAVY_PICK_VAL(var, val, 15, 6 )
#define PICK_MC_DXI_WRITE_STATUS(var, val)              CAVY_PICK_VAL(var, val, 5 , 0 )
#define GET_MC_CORE_STATUS_2_REG(base_addr, val)        (val = readl(base_addr+REG_TOP_0x02DC_MC_CORE_STATUS_2))

////    0x02E0	RO		MC_CORE_STATUS_3
////            [27:16]	MC_SRAM_TILE_STATUS
#define REG_TOP_0x02E0_MC_CORE_STATUS_3                 (0x02E0)
#define GET_MC_CORE_STATUS_3_REG(base_addr, val)        (val = readl(base_addr+REG_TOP_0x02E0_MC_CORE_STATUS_3))

////    0x02E4	RO		MC_BATCH_STATUS_0
////            [31:30]	FRC_MC_BATCH00_DONE_STATUS
////            [01:00]	FRC_MC_BATCH15_DONE_STATUS
#define REG_TOP_0x02E4_MC_BATCH_STATUS_0                (0x02E4)
#define PICK_FRC_MC_BATCH00_DONE_STATUS(var, val)       CAVY_PICK_VAL(var, val, 31, 30 )
#define PICK_FRC_MC_BATCH15_DONE_STATUS(var, val)       CAVY_PICK_VAL(var, val, 1, 0 )
#define GET_MC_BATCH_STATUS_0_REG(base_addr, val)       (val = readl(base_addr+REG_TOP_0x02E4_MC_BATCH_STATUS_0))

////    0x02E8	RO		DBG_DMA_MC
////            [27:16]	DMA_MC_TILE_IF_STATUS
////            [15:0]	DMA_MC_DXI_STATUS
#define REG_TOP_0x02E8_DBG_DMA_MC                       (0x02E8)
#define GET_DBG_DMA_MC_REG(base_addr, val)              (val = readl(base_addr+REG_TOP_0x02E8_DBG_DMA_MC))

////    0x02EC	RO		DBG_DMA_DBL
////            [15:8]	DMA_DBL_TILE_IF_STATUS
////            [5:0]	DMA_DBL_DXI_STATUS
#define REG_TOP_0x02EC_DBG_DMA_DBL                      (0x02EC)
#define GET_DBG_DMA_DBL_REG(base_addr, val)             (val = readl(base_addr+REG_TOP_0x02EC_DBG_DMA_DBL))

////    0x02F0	RO		DBL_CORE_STATUS
////            [15:8]	DBL_TILE_IF_STATUS
////            [5:0]	DBL_DXI_STATUS
#define REG_TOP_0x02F0_DBL_CORE_STATUS                  (0x02F0)
#define GET_DBL_CORE_STATUS_REG(base_addr, val)         (val = readl(base_addr+REG_TOP_0x02F0_DBL_CORE_STATUS))

//=========================================================================================

#define REG_RDMA_CH_BASE(ch)                                (0x0400 + 0x80*ch)
//#define REG_RDMA_CH_BASE(1)                               (0x0480)
//#define REG_RDMA_CH_BASE(2)                               (0x0500)
//#define REG_RDMA_CH_BASE(3)                               (0x0580)
//#define REG_RDMA_CH_BASE(4)                               (0x0600)
//#define REG_RDMA_CH_BASE(5)                               (0x0680)

////0x0400	WC		RDMA0_CH0_FRAME_START
////        WC[0]	RDMA0_CH0_FRAME_START
#define RDMA_FRAME_START                                    (0x00)
#define SET_RDMA_FRAME_START_REG(base_addr, ch, val)        writel(val, base_addr+REG_RDMA_CH_BASE(ch)+RDMA_FRAME_START)

////0x0404	RW		RDMA0_CH0_APB_READ_SEL
////        RW[0]	RDMA0_CH0_APB_READ_SEL
#define RDMA_APB_READ_SEL                                   (0x04)
#define RDMA_APB_READ_SEL_FLAG_SHADOW                       (0)
#define RDMA_APB_READ_SEL_FLAG_CORE                         (1)
#define SET_RDMA_APB_READ_SEL_REG(base_addr, ch, val)       writel(val, base_addr+REG_RDMA_CH_BASE(ch)+RDMA_APB_READ_SEL)
#define GET_RDMA_APB_READ_SEL_REG(base_addr, ch, val)       (val = readl(base_addr+REG_RDMA_CH_BASE(ch)+RDMA_APB_READ_SEL))

////0x0428	RO		RDMA0_CH0_STATUS
////        RO[31:16]	RDMA0_CH0_DBG_PIXEL
////        RO[15:0]	RDMA0_CH0_DBG_LINE
#define RDMA_STATUS                                         (0x28)
#define PICK_RDMA_CHn_DBG_PIXEL(var, val)                   CAVY_PICK_VAL(var, val, 31, 16)
#define PICK_RDMA_CHn_DBG_LINE(var, val)                    CAVY_PICK_VAL(var, val, 15, 0)
#define GET_RDMA_STATUS_REG(base_addr, ch, val)             (val = readl(base_addr+REG_RDMA_CH_BASE(ch)+RDMA_STATUS))

////0x042C	RO		RDMA0_CH0_IDLE
////        RO[16]	RDMA0_CH0_DBG_IDLE
#define RDMA_IDLE                                           (0x2C)
#define PICK_RDMA_CHn_IDLE(var, val)                        CAVY_PICK_VAL(var, val, 16, 16)
#define GET_RDMA_IDLE_REG(base_addr, ch, val)               (val = readl(base_addr+REG_RDMA_CH_BASE(ch)+RDMA_IDLE))

////0x0430	RO		RDMA0_CH0_DBG_STATE
////        RO[18:16]	RDMA0_CH0_DBG_STATE
////        RO[9:0]	RDMA0_CH0_FIFO_LEVEL
#define RDMA_DBG_STATE                                      (0x30)
#define PICK_RDMA_CHn_DBG_STATE(var, val)                   CAVY_PICK_VAL(var, val, 18, 16)
#define PICK_RDMA_CHn_FIFO_LEVEL(var, val)                  CAVY_PICK_VAL(var, val, 9, 0)
#define GET_RDMA_DBG_STATE_REG(base_addr, ch, val)          (val = readl(base_addr+REG_RDMA_CH_BASE(ch)+RDMA_DBG_STATE))

////0x0440	RW		RDMA0_CH0_BASEADDR
////        RW[31:0]	RDMA0_CH0_BASEADDR
#define RDMA_BASEADDR                                       (0x40)
#define SET_RDMA_BASEADDR_REG(base_addr, ch, val)           writel(val, base_addr+REG_RDMA_CH_BASE(ch)+RDMA_BASEADDR)
#define GET_RDMA_BASEADDR_REG(base_addr, ch, val)           (val = readl(base_addr+REG_RDMA_CH_BASE(ch)+RDMA_BASEADDR))

////0x0444	RW		RDMA0_CH0_STRIDE
////        RW[28]	RDMA0_CH0_STRIDE_NEG
////        RW[18:0]	RDMA0_CH0_STRIDE
#define RDMA_STRIDE                                         (0x44)
#define INSERT_RDMA_CHn_STRIDE_NEG(var, val)                CAVY_INSERT_VAL(var, val, 28, 28)
#define INSERT_RDMA_CHn_STRIDE(var, val)                    CAVY_INSERT_VAL(var, val, 18, 0)
#define SET_RDMA_STRIDE_REG(base_addr, ch, val)             writel(val, base_addr+REG_RDMA_CH_BASE(ch)+RDMA_STRIDE)
#define GET_RDMA_STRIDE_REG(base_addr, ch, val)             (val = readl(base_addr+REG_RDMA_CH_BASE(ch)+RDMA_STRIDE))

////0x044C	RW		RDMA0_CH0_CONTROL
////        RW[25:16]	RDMA0_CH0_CTRL_FIFO_SIZE
////        RW[9:0]	RDMA0_CH0_CTRL_FIFO_ADDR
#define RDMA_CONTROL                                        (0x4C)
#define INSERT_RDMA_CHn_CTRL_FIFO_SIZE(var, val)            CAVY_INSERT_VAL(var, val, 25, 16)
#define INSERT_RDMA_CHn_CTRL_FIFO_ADDR(var, val)            CAVY_INSERT_VAL(var, val, 9, 0)
#define SET_RDMA_CONTROL_REG(base_addr, ch, val)            writel(val, base_addr+REG_RDMA_CH_BASE(ch)+RDMA_CONTROL)
#define GET_RDMA_CONTROL_REG(base_addr, ch, val)            (val = readl(base_addr+REG_RDMA_CH_BASE(ch)+RDMA_CONTROL))

////0x0450	RW		RDMA0_CH0_ERROR_ENABLE
////        RW[15:0]	RDMA0_CH0_ERROR_ENABLE
#define RDMA_ERROR_ENABLE                                   (0x50)
#define RDMA_ERROR_FLAG_DMA_OTF_ERROR                       (1<<0 )
#define RDMA_ERROR_FLAG_FIFO_OVERFLOW                       (1<<1 )
#define RDMA_ERROR_FLAG_FIFO_UNDERFLOW                      (1<<2 )
#define RDMA_ERROR_FLAG_SBI_ERROR                           (1<<3 )
#define RDMA_ERROR_FLAG_BASE_ADDRESS_STRIDE_ALIGN_ERROR     (1<<7 )
#define RDMA_ERROR_FLAG_FORMAT_ERROR                        (1<<8 )
#define RDMA_ERROR_FLAG_DMA_SETTING_VALUE_CHANGE_ERROR      (1<<12)
#define RDMA_ERROR_FLAG_DMA_START_ERROR                     (1<<13)
#define SET_RDMA_ERROR_ENABLE_REG(base_addr, ch, val)       writel(val, base_addr+REG_RDMA_CH_BASE(ch)+RDMA_ERROR_ENABLE)
#define GET_RDMA_ERROR_ENABLE_REG(base_addr, ch, val)       (val = readl(base_addr+REG_RDMA_CH_BASE(ch)+RDMA_ERROR_ENABLE))

////0x0454	WC		RDMA0_CH0_ERROR_CLEAR
////        WC[15:0]	RDMA0_CH0_ERROR_CLEAR
#define RDMA_ERROR_CLEAR                                    (0x54)
#define SET_RDMA_ERROR_CLEAR_REG(base_addr, ch, val)        writel(val, base_addr+REG_RDMA_CH_BASE(ch)+RDMA_ERROR_CLEAR)

////0x0458	RO		RDMA0_CH0_ERROR_STATUS
////        RO[15:0]	RDMA0_CH0_ERROR_STATUS
#define RDMA_ERROR_STATUS                                   (0x58)
#define GET_RDMA_ERROR_STATUS_REG(base_addr, ch, val)       (val = readl(base_addr+REG_RDMA_CH_BASE(ch)+RDMA_ERROR_STATUS))

////0x045C	RW		RDMA0_CH0_OFFSET_ADDR
////        RW[31:0]	RDMA0_CH0_OFFSET_ADDR
#define RDMA_OFFSET_ADDR                                    (0x5C)

#define SET_RDMA_OFFSET_ADDR_REG(base_addr, ch, val)        writel(val, base_addr+REG_RDMA_CH_BASE(ch)+RDMA_OFFSET_ADDR)
#define GET_RDMA_OFFSET_ADDR_REG(base_addr, ch, val)        (val = readl(base_addr+REG_RDMA_CH_BASE(ch)+RDMA_OFFSET_ADDR))

//==============================================================================================

// special names for CH5_0 and CH5_1

////0x06C0	RW		RDMA0_CH5_BASEADDR_0
////        RW[31:0]	RDMA0_CH5_BASEADDR_0
#define RDMA_CH5_BASEADDR_0                                 (0x40)
#define SET_RDMA_CH5_BASEADDR_0_REG(base_addr, val)         writel(val, base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_BASEADDR_0)
#define GET_RDMA_CH5_BASEADDR_0_REG(base_addr, val)         (val = readl(base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_BASEADDR_0))

////0x06C4	RW		RDMA0_CH5_STRIDE_0
////        RW[28]	RDMA0_CH5_STRIDE_0_NEG
////        RW[18:0]	RDMA0_CH5_STRIDE_0
#define RDMA_CH5_STRIDE_0                                   (0x44)
#define SET_RDMA_CH5_STRIDE_0_REG(base_addr, val)           writel(val, base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_STRIDE_0)
#define GET_RDMA_CH5_STRIDE_0_REG(base_addr, val)           (val = readl(base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_STRIDE_0))

////0x06C8	RW		RDMA0_CH5_BASEADDR_1
////        RW[31:0]	RDMA0_CH5_BASEADDR_1
#define RDMA_CH5_BASEADDR_1                                 (0x48)
#define SET_RDMA_CH5_BASEADDR_1_REG(base_addr, val)         writel(val, base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_BASEADDR_1)
#define GET_RDMA_CH5_BASEADDR_1_REG(base_addr, val)         (val = readl(base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_BASEADDR_1))

////0x06CC	RW		RDMA0_CH5_STRIDE_1
////        RW[28]	RDMA0_CH5_STRIDE_1_NEG
////        RW[18:0]	RDMA0_CH5_STRIDE_1
#define RDMA_CH5_STRIDE_1                                   (0x4C)
#define SET_RDMA_CH5_STRIDE_1_REG(base_addr, val)           writel(val, base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_STRIDE_1)
#define GET_RDMA_CH5_STRIDE_1_REG(base_addr, val)           (val = readl(base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_STRIDE_1))

////0x06D0	RW		RDMA0_CH5_CONTROL
////        RW[25:16]	RDMA0_CH5_CTRL_FIFO_SIZE
////        RW[9:0]	RDMA0_CH5_CTRL_FIFO_ADDR
#define RDMA_CH5_CONTROL_01                                 (0x50)
#define SET_RDMA_CH5_CONTROL_01_REG(base_addr, val)         writel(val, base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_CONTROL_01)
#define GET_RDMA_CH5_CONTROL_01_REG(base_addr, val)         (val = readl(base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_CONTROL_01))

////0x06D4	RW		RDMA0_CH5_ERROR_ENABLE
////        RW[15:0]	RDMA0_CH5_ERROR_ENABLE
#define RDMA_CH5_ERROR_ENABLE_01                            (0x54)
#define SET_RDMA_CH5_ERROR_ENABLE_01_REG(base_addr, val)    writel(val, base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_ERROR_ENABLE_01)
#define GET_RDMA_CH5_ERROR_ENABLE_01_REG(base_addr, val)    (val = readl(base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_ERROR_ENABLE_01))

////0x06D8	WC		RDMA0_CH5_ERROR_CLEAR
////        WC[15:0]	RDMA0_CH5_ERROR_CLEAR
#define RDMA_CH5_ERROR_CLEAR_01                             (0x58)
#define SET_RDMA0_CH5_ERROR_CLEAR_REG(base_addr, val)       writel(val, base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_ERROR_CLEAR_01)

////0x06DC	RO		RDMA0_CH5_ERROR_STATUS
////        RO[15:0]	RDMA0_CH5_ERROR_STATUS
#define RDMA_CH5_ERROR_STATUS_01                            (0x5C)
#define GET_RDMA_CH5_ERROR_STATUS_01_REG(base_addr, val)    (val = readl(base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_ERROR_STATUS_01))

////0x06E0	RW		RDMA0_CH5_OFFSET_ADDR_0
////        RW[31:0]	RDMA0_CH5_OFFSET_ADDR_0
#define RDMA_CH5_OFFSET_ADDR_0                              (0x60)
#define SET_RDMA_CH5_OFFSET_ADDR_0(base_addr, val)          writel(val, base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_OFFSET_ADDR_0)
#define GET_RDMA_CH5_OFFSET_ADDR_0(base_addr, val)          (val = readl(base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_OFFSET_ADDR_0))

////0x06E4	RW		RDMA0_CH5_OFFSET_ADDR_1
////        RW[31:0]	RDMA0_CH5_OFFSET_ADDR_1
#define RDMA_CH5_OFFSET_ADDR_1                              (0x64)
#define SET_RDMA_CH5_OFFSET_ADDR_1(base_addr, val)          writel(val, base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_OFFSET_ADDR_1)
#define GET_RDMA_CH5_OFFSET_ADDR_1(base_addr, val)          (val = readl(base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_OFFSET_ADDR_1))

////0x06E8	RW		RDMA0_CH5_BASEADDR_0_F1
////		RW	[31:0]	RDMA0_CH5_BASEADDR_0_F1
#define RDMA_CH5_BASEADDR_0_F1                              (0x68)
#define SET_RDMA_CH5_BASEADDR_0_F1(base_addr, val)          writel(val, base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_BASEADDR_0_F1)
#define GET_RDMA_CH5_BASEADDR_0_F1(base_addr, val)          (val = readl(base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_BASEADDR_0_F1))

////0x06EC	RW		RDMA0_CH5_BASEADDR_1_F1
////		RW	[31:0]	RDMA0_CH5_BASEADDR_1_F1
#define RDMA_CH5_BASEADDR_1_F1                              (0x6C)
#define SET_RDMA_CH5_BASEADDR_1_F1(base_addr, val)          writel(val, base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_BASEADDR_1_F1)
#define GET_RDMA_CH5_BASEADDR_1_F1(base_addr, val)          (val = readl(base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_BASEADDR_1_F1))

////0x06F0	RW		RDMA0_CH5_BASEADDR_0_F2
////		RW	[31:0]	RDMA0_CH5_BASEADDR_0_F2
#define RDMA_CH5_BASEADDR_0_F2                              (0x70)
#define SET_RDMA_CH5_BASEADDR_0_F2(base_addr, val)          writel(val, base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_BASEADDR_0_F2)
#define GET_RDMA_CH5_BASEADDR_0_F2(base_addr, val)          (val = readl(base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_BASEADDR_0_F2))

////0x06F4	RW		RDMA0_CH5_BASEADDR_1_F2
////		RW	[31:0]	RDMA0_CH5_BASEADDR_1_F2
#define RDMA_CH5_BASEADDR_1_F2                              (0x74)
#define SET_RDMA_CH5_BASEADDR_1_F2(base_addr, val)          writel(val, base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_BASEADDR_1_F2)
#define GET_RDMA_CH5_BASEADDR_1_F2(base_addr, val)          (val = readl(base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_BASEADDR_1_F2))

//===============================================================================================

// WDMA registers

#define REG_WDMA_CH_BASE(ch)              (0x0800 + 0x80*ch)
//#define REG_RDMA_CH_BASE(1)              (0x0880)
//#define REG_RDMA_CH_BASE(2)              (0x0900)
//#define REG_RDMA_CH_BASE(3)              (0x0980)

////0x0800	WC		WDMA0_CH0_FRAME_START
////        WC[0]	WDMA0_CH0_FRAME_START
#define WDMA_FRAME_START                                        (0x00)
#define SET_WDMA_FRAME_START_REG(base_addr, ch, val)            writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_FRAME_START)

////0x0804	RW		WDMA0_CH0_TILE_CTRL_FRAME_SIZE
////        RW[31:16]	WDMA0_CH0_FRAME_SIZE_H
////        RW[15:0]	WDMA0_CH0_FRAME_SIZE_V
#define WDMA_TILE_CTRL_FRAME_SIZE                               (0x04)
#define INSERT_WDMA_CHn_FRAME_SIZE_H(var, val)                  CAVY_INSERT_VAL(var, val, 31, 16)
#define INSERT_WDMA_CHn_FRAME_SIZE_V(var, val)                  CAVY_INSERT_VAL(var, val, 15, 0)
#define SET_WDMA_TILE_CTRL_FRAME_SIZE_REG(base_addr, ch, val)   writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_TILE_CTRL_FRAME_SIZE)
#define GET_WDMA_TILE_CTRL_FRAME_SIZE_REG(base_addr, ch, val)   (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_TILE_CTRL_FRAME_SIZE))

////0x0808	RW		WDMA0_CH0_TILE_CTRL_FRAME_START
////        RW[31:16]	WDMA0_CH0_FRAME_START_X
////        RW[15:0]	WDMA0_CH0_FRAME_START_Y
#define WDMA_TILE_CTRL_FRAME_START                              (0x08)
#define INSERT_WDMA_CHn_FRAME_START_X(var, val)                 CAVY_INSERT_VAL(var, val, 31, 16)
#define INSERT_WDMA_CHn_FRAME_START_Y(var, val)                 CAVY_INSERT_VAL(var, val, 15, 0)
#define SET_WDMA_TILE_CTRL_FRAME_START_REG(base_addr, ch, val)  writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_TILE_CTRL_FRAME_START)
#define GET_WDMA_TILE_CTRL_FRAME_START_REG(base_addr, ch, val)  (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_TILE_CTRL_FRAME_START))

////0x080C	RW		WDMA0_CH0_TILE_CTRL_TILE_SIZE
////        RW[31:16]	WDMA0_CH0_TILE_SIZE_H
////        RW[15:0]	WDMA0_CH0_TILE_SIZE_V
#define WDMA_TILE_CTRL_TILE_SIZE                                (0x0C)
#define INSERT_WDMA_CHn_TILE_SIZE_H(var, val)                   CAVY_INSERT_VAL(var, val, 31, 16)
#define INSERT_WDMA_CHn_TILE_SIZE_V(var, val)                   CAVY_INSERT_VAL(var, val, 15, 0)
#define SET_WDMA_TILE_CTRL_TILE_SIZE_REG(base_addr, ch, val)    writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_TILE_CTRL_TILE_SIZE)
#define GET_WDMA_TILE_CTRL_TILE_SIZE_REG(base_addr, ch, val)    (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_TILE_CTRL_TILE_SIZE))

////0x0810	RW		WDMA0_CH0_TILE_CTRL_TILE_NUM
////        RW[25:16]	WDMA0_CH0_TILE_NUM_H
////        RW[9:0]	WDMA0_CH0_TILE_NUM_V
#define WDMA_TILE_CTRL_TILE_NUM                                 (0x10)
#define INSERT_WDMA_CHn_TILE_NUM_H(var, val)                    CAVY_INSERT_VAL(var, val, 25, 16)
#define INSERT_WDMA_CHn_TILE_NUM_V(var, val)                    CAVY_INSERT_VAL(var, val, 9, 0)
#define SET_WDMA_TILE_CTRL_TILE_NUM_REG(base_addr, ch, val)     writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_TILE_CTRL_TILE_NUM)
#define GET_WDMA_TILE_CTRL_TILE_NUM_REG(base_addr, ch, val)     (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_TILE_CTRL_TILE_NUM))

////0x0820	RW		WDMA0_CH0_APB_READ_SEL
////        RW[0]	WDMA0_CH0_APB_READ_SEL
#define WDMA_APB_READ_SEL                                       (0x20)
#define WDMA_APB_READ_SEL_FLAG_SHADOW                           (0)
#define WDMA_APB_READ_SEL_FLAG_CORE                             (1)
#define SET_WDMA_APB_READ_SEL_REG(base_addr, ch, val)           writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_APB_READ_SEL)
#define GET_WDMA_APB_READ_SEL_REG(base_addr, ch, val)           (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_APB_READ_SEL))

////0x0828	RO		WDMA0_CH0_TILE_CTRL_DBG
////        RO[31:16]	WDMA0_CH0_DBG_PIXEL
////        RO[15:0]	WDMA0_CH0_DBG_LINE
#define WDMA_TILE_CTRL_DBG                                      (0x28)
#define PICK_WDMA_CHn_DBG_PIXEL(var, val)                       CAVY_PICK_VAL(var, val, 31, 16)
#define PICK_WDMA_CHn_DBG_LINE(var, val)                        CAVY_PICK_VAL(var, val, 15, 0)
#define GET_WDMA_TILE_CTRL_DBG_REG(base_addr, ch, val)          (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_TILE_CTRL_DBG))

////0x082C	RO		WDMA0_CH0_TILE_CTRL_DBG_TILE
////        RO[20]	    WDMA0_CH0_DBG_IDLE
////        RO[19:10]	WDMA0_CH0_DBG_TILE_H
////        RO[9:0]	    WDMA0_CH0_DBG_TILE_V
#define WDMA_TILE_CTRL_DBG_TILE                                 (0x2C)
#define PICK_WDMA_CHn_DBG_IDLE(var, val)                        CAVY_PICK_VAL(var, val, 20, 20)
#define PICK_WDMA_CHn_DBG_TILE_H(var, val)                      CAVY_PICK_VAL(var, val, 19, 10)
#define PICK_WDMA_CHn_DBG_TILE_V(var, val)                      CAVY_PICK_VAL(var, val, 9, 0)
#define GET_WDMA_TILE_CTRL_DBG_TILE_REG(base_addr, ch, val)     (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_TILE_CTRL_DBG_TILE))

////0x0830	RO		WDMA0_CH0_DBG_STATE
////        RO[18:16]	WDMA0_CH0_DBG_STATE
////        RO[9:0]	    WDMA0_CH0_DBG_FIFO_LEVEL
#define WDMA_DBG_STATE                                          (0x30)
#define PICK_WDMA_CHn_DBG_STATE(var, val)                       CAVY_PICK_VAL(var, val, 18, 16)
#define PICK_WDMA_CHn_DBG_FIFO_LEVEL(var, val)                  CAVY_PICK_VAL(var, val, 9, 0)
#define GET_WDMA_DBG_STATE_REG(base_addr, ch, val)              (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_DBG_STATE))

////0x0840	RW		WDMA0_CH0_BASEADDR
////        RW[31:0]	WDMA0_CH0_BASEADDR
#define WDMA_BASEADDR                                           (0x40)
#define SET_WDMA_BASEADDR_REG(base_addr, ch, val)               writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_BASEADDR)
#define GET_WDMA_BASEADDR_REG(base_addr, ch, val)               (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_BASEADDR))

////0x0844	RW		WDMA0_CH0_STRIDE
////        RW[28]	    WDMA0_CH0_STRIDE_NEG
////        RW[18:0]	WDMA0_CH0_STRIDE
#define WDMA_STRIDE                                             (0x44)
#define INSERT_WDMA_CHn_STRIDE_NEG(var, val)                    CAVY_INSERT_VAL(var, val, 28, 28)
#define INSERT_WDMA_CHn_STRIDE(var, val)                        CAVY_INSERT_VAL(var, val, 18, 0)
#define SET_WDMA_STRIDE_REG(base_addr, ch, val)                 writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_STRIDE)
#define GET_WDMA_STRIDE_REG(base_addr, ch, val)                 (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_STRIDE))

////0x0848	RW		WDMA0_CH0_FORMAT
////        RW[9:8]	WDMA0_CH0_FMT_PPC
////        RW[6:0]	WDMA0_CH0_FMT_BPP
#define WDMA_FORMAT                                             (0x48)
#define WDMA_FORMAT_FLAG_1PPC                                   (0)
#define WDMA_FORMAT_FLAG_2PPC                                   (1)
#define WDMA_FORMAT_FLAG_4PPC                                   (2)
#define INSERT_WDMA0_CH_FMT_PPC(var, val)                       CAVY_INSERT_VAL(var, val, 9, 8)
#define INSERT_WDMA0_CH_FMT_BPP(var, val)                       CAVY_INSERT_VAL(var, val, 6, 0)
#define SET_WDMA_FORMAT_REG(base_addr, ch, val)                 writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_FORMAT)
#define GET_WDMA_FORMAT_REG(base_addr, ch, val)                 (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_FORMAT))

////0x084C	RW		WDMA0_CH0_CONTROL
////        RW[25:16]	WDMA0_CH0_CTRL_FIFO_SIZE
////        RW[9:0]	    WDMA0_CH0_CTRL_FIFO_ADDR
#define WDMA_CONTROL                                            (0x4C)
#define INSERT_WDMA_CHn_CTRL_FIFO_SIZE(var, val)                CAVY_INSERT_VAL(var, val, 25, 16)
#define INSERT_WDMA_CHn_CTRL_FIFO_ADDR(var, val)                CAVY_INSERT_VAL(var, val, 9, 0)
#define SET_WDMA_CONTROL_REG(base_addr, ch, val)                writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_CONTROL)
#define GET_WDMA_CONTROL_REG(base_addr, ch, val)                (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_CONTROL))

////0x0850	RW		WDMA0_CH0_ERROR_ENABLE
////        RW[15:0]	WDMA0_CH0_ERROR_ENABLE
#define WDMA_ERROR_ENABLE                                       (0x50)
#define WDMA_ERROR_FLAG_DMA_OTF_ERROR                           (1<<0 )
#define WDMA_ERROR_FLAG_FIFO_OVERFLOW                           (1<<1 )
#define WDMA_ERROR_FLAG_FIFO_UNDERFLOW                          (1<<2 )
#define WDMA_ERROR_FLAG_SBI_ERROR                               (1<<3 )
#define WDMA_ERROR_FLAG_DXI_EOL ERROR                           (1<<4 )
#define WDMA_ERROR_FLAG_DXI_EOT ERROR                           (1<<5 )
#define WDMA_ERROR_FLAG_2PPC_4PPC_SETTING_VALUE_ERROR           (1<<6 )
#define WDMA_ERROR_FLAG_BASE_ADDRESS_STRIDE_ALIGN_ERROR         (1<<7 )
#define WDMA_ERROR_FLAG_FORMAT_ERROR                            (1<<8 )
#define WDMA_ERROR_FLAG_YC420_LINE_ERROR                        (1<<9 )
#define WDMA_ERROR_FLAG_WRITE_BIT_OFFSET_ERROR                  (1<<11)
#define WDMA_ERROR_FLAG_DDMA_SETTING_VALUE_CHANGE_ERROR         (1<<12)
#define WDMA_ERROR_FLAG_DDMA_START_ERROR                        (1<<13)
#define SET_WDMA_ERROR_ENABLE_REG(base_addr, ch, val)           writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_ERROR_ENABLE)
#define GET_WDMA_ERROR_ENABLE_REG(base_addr, ch, val)           (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_ERROR_ENABLE))

////0x0854	WC		WDMA0_CH0_ERROR_CLEAR
////        WC[15:0]	WDMA0_CH0_ERROR_CLEAR
#define WDMA_ERROR_CLEAR                                        (0x54)
#define SET_WDMA_ERROR_CLEAR_REG(base_addr, ch, val)            writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_ERROR_CLEAR)

////0x0858	RO		WDMA0_CH0_ERROR_STATUS
////        RO[15:0]	WDMA0_CH0_ERROR_STATUS
#define WDMA_ERROR_STATUS                                       (0x58)
#define GET_WDMA_ERROR_STATUS_REG(base_addr, ch, val)           (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_ERROR_STATUS))

////0x085C	RW		WDMA0_CH0_OFFSET_ADDR
////        RW[31:0]	WDMA0_CH0_OFFSET_ADDR
#define WDMA_OFFSET_ADDR                                        (0x5C)
#define SET_WDMA_OFFSET_ADDR_REG(base_addr, ch, val)            writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_OFFSET_ADDR)
#define GET_WDMA_OFFSET_ADDR_REG(base_addr, ch, val)            (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_OFFSET_ADDR))

////0x0860	RW		WDMA0_CH0_BASEADDR_F1
////		RW	[31:0]	WDMA0_CH0_BASEADDR_F1
#define WDMA_BASEADDR_F1                                        (0x60)
#define SET_WDMA_BASEADDR_F1_REG(base_addr, ch, val)            writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_BASEADDR_F1)
#define GET_WDMA_BASEADDR_F1_REG(base_addr, ch, val)            (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_BASEADDR_F1))

////0x0864	RW		WDMA0_CH0_BASEADDR_F2
////		RW	[31:0]	WDMA0_CH0_BASEADDR_F2
#define WDMA_BASEADDR_F2                                        (0x64)
#define SET_WDMA_BASEADDR_F2_REG(base_addr, ch, val)            writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_BASEADDR_F2)
#define GET_WDMA_BASEADDR_F2_REG(base_addr, ch, val)            (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_BASEADDR_F2))


#ifdef DEBUG
static inline void mcfrc_print_all_regs(struct mcfrc_dev *mcfrc, void __iomem *base_addr)
{
	unsigned int val, ch;
	dev_dbg(mcfrc->dev, "%s: BASE ADDR is %llx\n", __func__, (unsigned long long)base_addr);
	// TOP
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0000_FRC_MC_OP_MODE_SEL, GET_FRC_MC_OP_MODE_SEL_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0004_FRC_MC_CLKREQ_CONTROL, GET_MC_CLKREQ_CONTROL_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0008_FRC_MC_APB_PPROT, GET_MC_APB_PPROT_REG(base_addr, val));

	// MC TOP
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0208_MC_SW_RESET, GET_MC_SW_RESET_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x020C_DMA_SW_RESET, GET_DMA_SW_RESET_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0210_MC_CLKREQn_CONTROL, GET_MC_CLKREQn_CONTROL_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0214_MC_FRAME_ID, GET_MC_FRAME_ID_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x021C_MC_FRAME_SIZE, GET_MC_FRAME_SIZE_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0220_MC_VERSION_OP_MODE_SEL, GET_MC_VERSION_OP_MODE_SEL_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0224_MC_BATCH_MODE, GET_MC_BATCH_MODE_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0228_MC_BYPASS_PHASE, GET_MC_BYPASS_PHASE_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x022C_MC_TIMEOUT, GET_MC_TIMEOUT_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0230_MC_FRAME_DONE_CHECK, GET_MC_FRAME_DONE_CHECK_REG(base_addr, val));

	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0254_FRC_DBL_EN, GET_FRC_DBL_EN_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0258_FRC_DBL_MODE, GET_FRC_DBL_MODE_REG(base_addr, val));

	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0270_MC_AXI_USER, GET_MC_AXI_USER_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0274_DMA_SBI_RD_CONFIG, GET_DMA_SBI_RD_CONFIG_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0278_DMA_SBI_RD_STATUS, GET_DMA_SBI_RD_STATUS_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x027C_DMA_SBI_WR_CONFIG, GET_DMA_SBI_WR_CONFIG_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0280_DMA_SBI_WR_STATUS, GET_DMA_SBI_WR_STATUS_REG(base_addr, val));

	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0290_MC_INTR_MASKING, GET_MC_INTR_MASKING_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0294_INTR_MASK_DBG_STATUS, GET_INTR_MASK_DBG_STATUS_REG(base_addr, val));

	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x029C_FRC_MC_DONE_IRQ_STATUS, GET_FRC_MC_DONE_IRQ_STATUS_REG(base_addr, val));

	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x02A4_FRC_MC_DBL_ERR_IRQ_STATUS, GET_FRC_MC_DBL_ERR_IRQ_STATUS_REG(base_addr, val));

	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x02A8_MC_ERROR_DEBUG_ENABLE, GET_MC_ERROR_DEBUG_ENABLE_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x02B0_MC_ERROR_STATUS, GET_MC_ERROR_STATUS_REG(base_addr, val));

	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x02D0_MC_PROCESSING_TIME_STATUS, GET_MC_PROCESSING_TIME_STATUS_REG(base_addr, val));

	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x02D4_MC_CORE_STATUS_0, GET_MC_CORE_STATUS_0_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x02D8_MC_CORE_STATUS_1, GET_MC_CORE_STATUS_1_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x02DC_MC_CORE_STATUS_2, GET_MC_CORE_STATUS_2_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x02E0_MC_CORE_STATUS_3, GET_MC_CORE_STATUS_3_REG(base_addr, val));

	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x02E4_MC_BATCH_STATUS_0, GET_MC_BATCH_STATUS_0_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x02E8_DBG_DMA_MC, GET_DBG_DMA_MC_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x02EC_DBG_DMA_DBL, GET_MC_BATCH_STATUS_0_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x02F0_DBL_CORE_STATUS, GET_DBL_CORE_STATUS_REG(base_addr, val));

	// RDMA
	for (ch = 0; ch <= 5; ch++) {
		dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(ch) + RDMA_APB_READ_SEL, GET_RDMA_APB_READ_SEL_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(ch) + RDMA_STATUS, GET_RDMA_STATUS_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(ch) + RDMA_IDLE, GET_RDMA_IDLE_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(ch) + RDMA_DBG_STATE, GET_RDMA_DBG_STATE_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(ch) + RDMA_BASEADDR, GET_RDMA_BASEADDR_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(ch) + RDMA_STRIDE, GET_RDMA_STRIDE_REG(base_addr, ch, val));
		if (5 != ch) {
			dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(ch) + RDMA_CONTROL, GET_RDMA_CONTROL_REG(base_addr, ch, val));
			dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(ch) + RDMA_ERROR_ENABLE, GET_RDMA_ERROR_ENABLE_REG(base_addr, ch, val));
			dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(ch) + RDMA_ERROR_STATUS, GET_RDMA_ERROR_STATUS_REG(base_addr, ch, val));
			dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(ch) + RDMA_OFFSET_ADDR, GET_RDMA_OFFSET_ADDR_REG(base_addr, ch, val));
		}
	}
	ch = 5;
	dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(5) + RDMA_CH5_BASEADDR_1, GET_RDMA_CH5_BASEADDR_1_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(5) + RDMA_CH5_STRIDE_1, GET_RDMA_CH5_STRIDE_1_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(5) + RDMA_CH5_CONTROL_01, GET_RDMA_CH5_CONTROL_01_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(5) + RDMA_CH5_ERROR_ENABLE_01, GET_RDMA_CH5_ERROR_ENABLE_01_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(5) + RDMA_CH5_ERROR_STATUS_01, GET_RDMA_CH5_ERROR_STATUS_01_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(5) + RDMA_CH5_OFFSET_ADDR_0, GET_RDMA_CH5_OFFSET_ADDR_0(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(5) + RDMA_CH5_OFFSET_ADDR_1, GET_RDMA_CH5_OFFSET_ADDR_1(base_addr, val));

	// WDMA
	for (ch = 0; ch <= 3; ch++) {
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_TILE_CTRL_FRAME_SIZE, GET_WDMA_TILE_CTRL_FRAME_SIZE_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_TILE_CTRL_FRAME_START, GET_WDMA_TILE_CTRL_FRAME_START_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_TILE_CTRL_TILE_SIZE, GET_WDMA_TILE_CTRL_TILE_SIZE_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_TILE_CTRL_TILE_NUM, GET_WDMA_TILE_CTRL_TILE_NUM_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_APB_READ_SEL, GET_WDMA_APB_READ_SEL_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_TILE_CTRL_DBG, GET_WDMA_TILE_CTRL_DBG_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_TILE_CTRL_DBG_TILE, GET_WDMA_TILE_CTRL_DBG_TILE_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_DBG_STATE, GET_WDMA_DBG_STATE_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_BASEADDR, GET_WDMA_BASEADDR_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_STRIDE, GET_WDMA_STRIDE_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_FORMAT, GET_WDMA_FORMAT_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_CONTROL, GET_WDMA_CONTROL_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_ERROR_ENABLE, GET_WDMA_ERROR_ENABLE_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_ERROR_STATUS, GET_WDMA_ERROR_STATUS_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_OFFSET_ADDR, GET_WDMA_OFFSET_ADDR_REG(base_addr, ch, val));
	}
}
#endif

#endif /* MCFRC_REGS_H_ */