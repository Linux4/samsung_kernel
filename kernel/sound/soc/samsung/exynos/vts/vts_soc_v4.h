/* SPDX-License-Identifier: GPL-2.0-or-later
 * sound/soc/samsung/vts/vts_soc.h
 *
 * ALSA SoC - Samsung VTS driver
 *
 * Copyright (c) 2022 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_VTS_DEP_SOC_V4_H
#define __SND_SOC_VTS_DEP_SOC_V4_H

/* SYSREG_VTS */
#define	USER_REG0						0x0000
#define	USER_REG1						0x0004
#define	USER_REG2						0x0008
#define	USER_REG3						0x000c
#define	USER_REG4						0x0010
#define	SFR_APB							0x0100
#define	BUS_COMPONENT_DRCG_EN			0x0104
#define	MEMCLK							0x0108
#define	VTS_MIF_REQ_OUT					0x0200
#define	VTS_MIF_REQ_ACK_IN				0x0204
#define	TCXO_REQ_OUT					0x0210
#define	TCXO_REQ_ACK_IN					0x0214
#define	CMGP_REQ_OUT					0x0220
#define	CMGP_REQ_ACK_IN					0x0224
#define	ARCACHE_M5						0x0228
#define	ARCACHE_M5_SEL					0x022c
#define	UNPU_REQ_OUT					0x0230
#define	UNPU_REQ_ACK_IN					0x0234
#define	LPP_MCS_LVT						0x0300
#define	LPP_MCSW_LVT					0x0304
#define	LPP_MCSRD_LVT					0x0308
#define	LPP_MCSWR_LVT					0x030c
#define	LPP_KCS_LVT						0x0310
#define	LPP_ADME_LVT					0x0314
#define	LPP_WRME_LVT					0x0318
#define	EMA_STATUS						0x031c
#define	LPP_MCS_RVT						0x0320
#define	LPP_MCSW_RVT					0x0324
#define	LPP_ADME_RVT					0x0334
#define	LPP_WRME_RVT					0x0338
#define	DMIC_CLK_DEBUG					0x0430
#define	DMIC_CLK_CON					0x0434
#define	DMIC1_CLK_CON					0x0500
#define	DMIC1_SFR_CLK_SEL				0x0504
#define	DMIC2_CLK_CON					0x0508
#define	DMIC2_SFR_CLK_SEL				0x0520
#define	DMIC_CLK_DEBUG_DMIC1			0x0524
#define	DMIC_CLK_DEBUG_DMIC2			0x0528
#define	APB_SEMA_DMAILBOX				0x0530
#define	APB_SEMA_DMAILBOX_1				0x0534
#define	APB_SEMA_DMAILBOX_2				0x0538
#define	APB_SEMA_DMAILBOX_3				0x053c
#define	APB_SEMA_DMAILBOX_4				0x0540
#define	PSTATUS							0x0558
#define	DMIC_SFR_CLK_SEL				0x0600
#define	YAMIN_FAULT0					0x0604
#define	YAMIN_FAULT1					0x0608
#define	YAMIN_CACHE						0x0620
#define	YAMIN_MPU						0x0624
#define	YAMIN_WIC						0x0628
#define	YAMIN_INTNUM					0x0644
#define	YAMIN_SLEEP						0x0648
#define	VTS_SCALABLE_CONTROL0			0x0650
#define	VTS_SCALABLE_CONTROL1			0x0654
#define	FG_MEMP							0x0700
#define	SRAM_CON_CODE					0x0800
#define	DEBUG_IN_CODE					0x0804
#define	SRAM_CON_DATA0					0x0808
#define	DEBUG_IN_DATA0					0x080c
#define	SRAM_CON_PCM					0x0810
#define	DEBUG_IN_PCM					0x0814
#define	SRAM_CON_DATA1					0x0818
#define	DEBUG_IN_DATA1					0x081c
#define	SERIAL_LIF_RMASTER_USE			0x0820
#define	YAMIN_MCU_VTS_CFG				0x0828
#define	YAMIN_MCU_VTS_CALIB				0x0840
#define	YAMIN_FP						0x0920
#define	YAMIN_LOCKUP					0x0924
#define	HWACG_CM55_CLKREQ				0x0928
#define	HWACG_CM55_CLKREQ_1				0x092c
#define	HWACG_CM55_CLKREQ_DAP			0x0930
#define	SEL_PDM							0x0940
#define	SEL_PDM_REQUEST					0x0944
#define	YAMIN_DUMP						0x0948
#define	ENABLE_DMIC_IF					0x1000
#define	EXT_INT							0x100c
#define	AWCACHE_M5						0x1040
#define	AWCACHE_M5_SEL					0x1044
#define	AXIQOS							0x1100
#define	ERR_RSP_EN						0x1800

/* DMIC_CLK_CON */
#define VTS_ENABLE_CLK_GEN_OFFSET       (0)
#define VTS_ENABLE_CLK_GEN_SIZE         (1)
#define VTS_SEL_EXT_DMIC_CLK_OFFSET     (1)
#define VTS_SEL_EXT_DMIC_CLK_SIZE       (1)
#define VTS_ENABLE_CLK_CLK_GEN_OFFSET   (14)
#define VTS_ENABLE_CLK_CLK_GEN_SIZE     (1)

/* DMIC_IF */
#define VTS_DMIC_ENABLE_DMIC_IF		(0x0000)
#define VTS_DMIC_CONTROL_DMIC_IF	(0x0004)
/* VTS_DMIC_ENABLE_DMIC_IF */
#define VTS_DMIC_ENABLE_DMIC_IF_OFFSET	(31)
#define VTS_DMIC_ENABLE_DMIC_IF_SIZE	(1)
#define VTS_DMIC_PERIOD_DATA2REQ_OFFSET	(16)
#define VTS_DMIC_PERIOD_DATA2REQ_SIZE	(2)
/* VTS_DMIC_CONTROL_DMIC_IF */
#define VTS_DMIC_HPF_EN_OFFSET		(31)
#define VTS_DMIC_HPF_EN_SIZE		(1)
#define VTS_DMIC_LPFEXT_EN_OFFSET	(30)
#define VTS_DMIC_LPFEXT_EN_SIZE 	(1)
#define VTS_DMIC_HPF_SEL_OFFSET		(28)
#define VTS_DMIC_HPF_SEL_SIZE		(1)
#define VTS_DMIC_CPS_SEL_OFFSET		(27)
#define VTS_DMIC_CPS_SEL_SIZE		(1)
#define VTS_DMIC_GAIN_OFFSET		(24)
#define VTS_DMIC_1DB_GAIN_OFFSET	(21)
#define VTS_DMIC_GAIN_SIZE		(3)
#define VTS_DMIC_CPSEXT_EN_OFFSET	(20)
#define VTS_DMIC_CPSEXT_EN_SIZE 	(1)
#define VTS_DMIC_DMIC_SEL_OFFSET	(18)
#define VTS_DMIC_DMIC_SEL_SIZE		(1)
#define VTS_DMIC_RCH_EN_OFFSET		(17)
#define VTS_DMIC_RCH_EN_SIZE		(1)
#define VTS_DMIC_LCH_EN_OFFSET		(16)
#define VTS_DMIC_LCH_EN_SIZE		(1)
#define VTS_DMIC_SYS_SEL_OFFSET		(12)
#define VTS_DMIC_SYS_SEL_SIZE		(2)
#define VTS_DMIC_POLARITY_CLK_OFFSET	(10)
#define VTS_DMIC_POLARITY_CLK_SIZE	(1)
#define VTS_DMIC_POLARITY_OUTPUT_OFFSET	(9)
#define VTS_DMIC_POLARITY_OUTPUT_SIZE	(1)
#define VTS_DMIC_POLARITY_INPUT_OFFSET	(8)
#define VTS_DMIC_POLARITY_INPUT_SIZE	(1)
#define VTS_DMIC_OVFW_CTRL_OFFSET	(4)
#define VTS_DMIC_OVFW_CTRL_SIZE		(1)
#define VTS_DMIC_CIC_SEL_OFFSET		(0)
#define VTS_DMIC_CIC_SEL_SIZE		(1)

/* SLIF VT OFFSET */
#define SLIF_INT_EN_CLR		(0x40)
#define SLIF_INT_PEND		(0x48)
#define SLIF_CTRL		(0x100)
#define SLIF_CONFIG_DONE	(0x10C)

/* GPR_DUMP */
#define VTS_CM_R(x)			(0x0010 + (x * 0x4))
#define VTS_CM_PC			(0x0004)
#define GPR_DUMP_CNT			(23) /* guided from manual */

/* Interrupt number VTS -> AP */
#define VTS_IRQ_VTS_ERROR               (0)
#define VTS_IRQ_VTS_BOOT_COMPLETED      (1)
#define VTS_IRQ_VTS_IPC_RECEIVED        (2)
#define VTS_IRQ_VTS_VOICE_TRIGGERED     (3)
#define VTS_IRQ_VTS_PERIOD_ELAPSED      (4)
#define VTS_IRQ_VTS_REC_PERIOD_ELAPSED  (5)
#define VTS_IRQ_VTS_DBGLOG_BUFZERO	 (6)
#define VTS_IRQ_VTS_INTERNAL_REC_ELAPSED (7)
#define VTS_IRQ_VTS_AUDIO_DUMP		 (8)
#define VTS_IRQ_VTS_LOG_DUMP            (9)
#define VTS_IRQ_VTS_STATUS		(10)
#define VTS_IRQ_VTS_SLIF_DUMP           (11)
#define VTS_IRQ_VTS_CP_WAKEUP           (15)

/* Interrupt number AP -> VTS */
#define VTS_IRQ_AP_IPC_RECEIVED         (16)
#define VTS_IRQ_AP_SET_DRAM_BUFFER      (17)
#define VTS_IRQ_AP_START_RECOGNITION    (18)
#define VTS_IRQ_AP_STOP_RECOGNITION     (19)
#define VTS_IRQ_AP_START_COPY           (20)
#define VTS_IRQ_AP_STOP_COPY            (21)
#define VTS_IRQ_AP_SET_MODE             (22)
#define VTS_IRQ_AP_POWER_DOWN           (23)
#define VTS_IRQ_AP_TARGET_SIZE          (24)
#define VTS_IRQ_AP_SET_REC_BUFFER       (25)
#define VTS_IRQ_AP_START_REC            (26)
#define VTS_IRQ_AP_STOP_REC             (27)
#define VTS_IRQ_AP_GET_VERSION		(28)
#define VTS_IRQ_AP_RESTART_RECOGNITION  (29)
#define VTS_IRQ_AP_COMMAND		(31)

#define VTS_DMIC_IF_ENABLE_DMIC_AUD0		(0x6)
#define VTS_DMIC_IF_ENABLE_DMIC_AUD1		(0x7)
#define VTS_DMIC_IF_ENABLE_DMIC_AUD2		(0x8)

#define VTS_IRQ_COUNT (11)
#define VTS_IRQ_LIMIT (32)

#define VTS_BAAW_BASE		   (0x60000000)
#define VTS_BAAW_SRC_START_ADDRESS (0x10000)
#define VTS_BAAW_SRC_END_ADDRESS   (0x10004)
#define VTS_BAAW_REMAPPED_ADDRESS  (0x10008)
#define VTS_BAAW_INIT_DONE	   (0x1000C)
/* VTS_BAAW_BASE / VTS_BAAW_DRAM_DIV = Config value in baaw guide doc */
#define VTS_BAAW_DRAM_DIV	   (0x10)

#define SICD_SOC_DOWN_OFFSET	(0x18C)
#define SICD_MIF_DOWN_OFFSET	(0x19C)

#define DMA_BUF_SIZE (0x50000)
#define DMA_BUF_TRI_OFFSET (0x0)
#define DMA_BUF_REC_OFFSET (DMA_BUF_SIZE*1)
#define DMA_BUF_INTERNAL_OFFSET (DMA_BUF_SIZE*2)
#define DMA_BUF_CNT (3)
#define DMA_BUF_BYTES_MAX (DMA_BUF_SIZE*DMA_BUF_CNT)
#define PERIOD_BYTES_MIN (SZ_4)
#define PERIOD_BYTES_MAX (DMA_BUF_SIZE)

#define SOUND_MODEL_SIZE_MAX (SZ_32K)
#define SOUND_MODEL_COUNT (3)

/* DRAM for copying VTS firmware logs */
#define LOG_BUFFER_BYTES_MAX	   (0x2000)
#define VTS_SRAMLOG_MSGS_OFFSET	   (0x00190000)
#define VTS_SRAMLOG_SIZE_MAX	   (SZ_4K)  /* SZ_1K : 0x400 */
#define VTS_SRAM_TIMELOG_SIZE_MAX  (SZ_1K)
#define VTS_SRAM_EVENTLOG_SIZE_MAX (VTS_SRAMLOG_SIZE_MAX - VTS_SRAM_TIMELOG_SIZE_MAX)

/* VTS firmware version information offset */
#define VTSFW_VERSION_OFFSET	(0xac)
#define DETLIB_VERSION_OFFSET	(0xa8)

/* PMU Registers */
#define PAD_RETENTION_VTS_OPTION  (0x25a0) // VTS_OUT : PAD__RTO
#define VTS_CPU_CONFIGURATION   (0x3580)
#define VTS_CPU_STATUS		(0x3584)
#define VTS_CPU_LOCAL_PWR_CFG			(0x00000001)
#define VTS_CPU_IN_SLEEPING_MASK		(0x00000001)
#define VTS_CPU_IN_SLEEPDEEP_MASK		(0x00000002)

#define LIMIT_IN_JIFFIES (msecs_to_jiffies(1000))
#define DMIC_CLK_RATE (768000)
#define VTS_TRIGGERED_TIMEOUT_MS (5000)

#define VTS_SYS_CLOCK_MAX	(393216000)
#define VTS_SYS_CLOCK		(196608000)	/* 200M */
#define DEF_VTS_PCM_DUMP

#define VTS_DMIC_CLK_MUX	(49152000)
#define SLIF_DMIC_CLK_MUX	(73728000)
#define DUAL_DMIC_CLK_MUX	(49152000)

#define VTS_ERR_HARD_FAULT	(0x1)
#define VTS_ERR_BUS_FAULT	(0x3)

#define MIC_IN_CH_NORMAL	(2)
#define MIC_IN_CH_WITH_ABOX_REC (4)

#define VTS_ITMON_NAME "CHUBVTS" /* refer to bootloader/platform/s5e9945/debug/itmon.c */

#define VTS_SRAM_SZ 0x200000

/* Internal memory  */
#define FG_MEMP_CON              (0x000c)
#define FG_MEMP_CON_EN_SIZE      (1)
#define FG_MEMP_CON_EN_OFFSET    (0)

/* cmu vts  */
#define MEMPG_CON_INTMEM_CODE_0  (0x3034)
#define MEMPG_CON_INTMEM_DATA0_3 (0x303C)
#define MEMPG_CON_INTMEM_DATA0_4 (0x3040)
#define MEMPG_CON_INTMEM_DATA0_5 (0x3044)
#define MEMPG_CON_INTMEM_DATA0_6 (0x3048)
#define MEMPG_CON_INTMEM_DATA0_7 (0x3050)
#define MEMPG_CON_INTMEM_PCM_1   (0x3058)

#define USE_PG_COUNTER_SIZE      (1)
#define USE_PG_COUNTER_OFFSET    (1)

#endif /* __SND_SOC_VTS_DEP_SOC_V4_H */
