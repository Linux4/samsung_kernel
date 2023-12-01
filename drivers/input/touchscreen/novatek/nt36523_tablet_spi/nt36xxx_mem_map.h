/*
 * Copyright (C) 2010 - 2022 Novatek, Inc.
 *
 * $Revision: 110851 $
 * $Date: 2022-12-27 17:56:41 +0800 (週二, 27 十二月 2022) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */


struct nvt_ts_mem_map {
	uint32_t EVENT_BUF_ADDR;
	uint32_t RAW_PIPE0_ADDR;
	uint32_t RAW_PIPE1_ADDR;
	uint32_t BASELINE_ADDR;
	uint32_t BASELINE_BTN_ADDR;
	uint32_t DIFF_PIPE0_ADDR;
	uint32_t DIFF_PIPE1_ADDR;
	uint32_t RAW_BTN_PIPE0_ADDR;
	uint32_t RAW_BTN_PIPE1_ADDR;
	uint32_t DIFF_BTN_PIPE0_ADDR;
	uint32_t DIFF_BTN_PIPE1_ADDR;
	uint32_t PEN_2D_BL_TIP_X_ADDR;
	uint32_t PEN_2D_BL_TIP_Y_ADDR;
	uint32_t PEN_2D_BL_RING_X_ADDR;
	uint32_t PEN_2D_BL_RING_Y_ADDR;
	uint32_t PEN_2D_DIFF_TIP_X_ADDR;
	uint32_t PEN_2D_DIFF_TIP_Y_ADDR;
	uint32_t PEN_2D_DIFF_RING_X_ADDR;
	uint32_t PEN_2D_DIFF_RING_Y_ADDR;
	uint32_t PEN_2D_RAW_TIP_X_ADDR;
	uint32_t PEN_2D_RAW_TIP_Y_ADDR;
	uint32_t PEN_2D_RAW_RING_X_ADDR;
	uint32_t PEN_2D_RAW_RING_Y_ADDR;
	uint32_t PEN_1D_DIFF_TIP_X_ADDR;
	uint32_t PEN_1D_DIFF_TIP_Y_ADDR;
	uint32_t PEN_1D_DIFF_RING_X_ADDR;
	uint32_t PEN_1D_DIFF_RING_Y_ADDR;
	/* FDM selftest */
	uint32_t DOZE_GM_S1D_SCAN_RAW_ADDR;
	/* FW History */
	uint32_t MMAP_HISTORY_EVENT0;
	uint32_t MMAP_HISTORY_EVENT1;
	/* Phase 2 Host Download */
	uint32_t BOOT_RDY_ADDR;
	uint32_t POR_CD_ADDR;
	uint32_t TX_AUTO_COPY_EN;
	uint32_t SPI_DMA_TX_INFO;
	/* BLD CRC */
	uint32_t BLD_LENGTH_ADDR;
	uint32_t ILM_LENGTH_ADDR;
	uint32_t DLM_LENGTH_ADDR;
	uint32_t BLD_DES_ADDR;
	uint32_t ILM_DES_ADDR;
	uint32_t DLM_DES_ADDR;
	uint32_t G_ILM_CHECKSUM_ADDR;
	uint32_t G_DLM_CHECKSUM_ADDR;
	uint32_t R_ILM_CHECKSUM_ADDR;
	uint32_t R_DLM_CHECKSUM_ADDR;
	uint32_t DMA_CRC_EN_ADDR;
	uint32_t BLD_ILM_DLM_CRC_ADDR;
	uint32_t DMA_CRC_FLAG_ADDR;
	uint32_t SPI_DMA_VAL_ADDR;
	//Cascade SPI DMA
	uint32_t SPI_DMA_LENGTH;
	uint32_t SPI_DMA_REM_ADDR;
	uint32_t SPI_DMA_LOC_ADDR;
	uint32_t SPI_TX_READ_TRIGGER;
	uint32_t SPI_TX_WRITE_TRIGGER;
	uint32_t TX_DMA_2X_EN;
	uint32_t SPI_DMA_TX_RESET;
	uint32_t SPI_DMA_RX_RESET;
	uint32_t RX_DMA_2X_EN;
	uint32_t LOC_RW_BUF_ADDR;
	/* SRAM Test */
	uint32_t MBT_MUX_CTL0;
	uint32_t MBT_MUX_CTL1;
	uint32_t MBT_MUX_CTL2;
	uint32_t MBT_DB;
	uint32_t MBT_MODE;
	uint32_t MBT_EN;
	uint32_t MBT_OP;
	uint32_t MBT_STATUS;
	//LPWG DUMP
	uint32_t LPWG_DUMP_ADDR;
};

struct nvt_ts_hw_info {
	uint8_t hw_crc;
	uint8_t auto_copy;
	bool use_gcm;
	const struct nvt_ts_hw_reg_addr_info *hw_regs;
};

typedef enum {
	HWCRC_NOSUPPORT  = 0x00,
	HWCRC_LEN_2Bytes = 0x01,
	HWCRC_LEN_3Bytes = 0x02,
} HWCRCBankConfig;

typedef enum {
	AUTOCOPY_NOSUPPORT    = 0x00,
	CHECK_SPI_DMA_TX_INFO = 0x01,
	CHECK_TX_AUTO_COPY_EN = 0x02,
} AUTOCOPYCheck;

/* hw info reg*/
struct nvt_ts_hw_reg_addr_info {
	uint32_t chip_ver_trim_addr;
	uint32_t swrst_sif_addr;
	uint32_t crc_err_flag_addr;
};

static const struct nvt_ts_hw_reg_addr_info hw_reg_addr_info = {
	.chip_ver_trim_addr = 0x1FB104,
	.swrst_sif_addr = 0x1FB43E,
	.crc_err_flag_addr = 0x1FB535,
};

static const struct nvt_ts_hw_reg_addr_info hw_reg_addr_info_old_w_isp = {
	.chip_ver_trim_addr = 0x3F004,
	.swrst_sif_addr = 0x3F0FE,
	.crc_err_flag_addr = 0x3F135,
};

static const struct nvt_ts_hw_reg_addr_info hw_reg_addr_info_legacy_w_isp = {
	.chip_ver_trim_addr = 0x1F64E,
	.swrst_sif_addr = 0x1F01A,
	.crc_err_flag_addr = 0,
};


static const struct nvt_ts_mem_map NT36523N_memory_map = {
	.EVENT_BUF_ADDR           = 0x2FD00,
	.RAW_PIPE0_ADDR           = 0x30FA0,
	.RAW_PIPE1_ADDR           = 0x30FA0,
	.BASELINE_ADDR            = 0x36510,
	.BASELINE_BTN_ADDR        = 0,
	.DIFF_PIPE0_ADDR          = 0x373E8,
	.DIFF_PIPE1_ADDR          = 0x38068,
	.RAW_BTN_PIPE0_ADDR       = 0,
	.RAW_BTN_PIPE1_ADDR       = 0,
	.DIFF_BTN_PIPE0_ADDR      = 0,
	.DIFF_BTN_PIPE1_ADDR      = 0,
	.PEN_2D_BL_TIP_X_ADDR     = 0x2988A,
	.PEN_2D_BL_TIP_Y_ADDR     = 0x29A1A,
	.PEN_2D_BL_RING_X_ADDR    = 0x29BAA,
	.PEN_2D_BL_RING_Y_ADDR    = 0x29D3A,
	.PEN_2D_DIFF_TIP_X_ADDR   = 0x29ECA,
	.PEN_2D_DIFF_TIP_Y_ADDR   = 0x2A05A,
	.PEN_2D_DIFF_RING_X_ADDR  = 0x2A1EA,
	.PEN_2D_DIFF_RING_Y_ADDR  = 0x2A37A,
	.PEN_2D_RAW_TIP_X_ADDR    = 0x2A50A,
	.PEN_2D_RAW_TIP_Y_ADDR    = 0x2A69A,
	.PEN_2D_RAW_RING_X_ADDR   = 0x2A82A,
	.PEN_2D_RAW_RING_Y_ADDR   = 0x2A9BA,
	.PEN_1D_DIFF_TIP_X_ADDR   = 0x2AB4A,
	.PEN_1D_DIFF_TIP_Y_ADDR   = 0x2ABAE,
	.PEN_1D_DIFF_RING_X_ADDR  = 0x2AC12,
	.PEN_1D_DIFF_RING_Y_ADDR  = 0x2AC76,
	/* FW History */
	.MMAP_HISTORY_EVENT0      = 0x38D54,
	.MMAP_HISTORY_EVENT1      = 0x38D94,
	/* Phase 2 Host Download */
	.BOOT_RDY_ADDR            = 0x3F10D,
	.TX_AUTO_COPY_EN          = 0x3F7E8,
	.SPI_DMA_TX_INFO          = 0x3F7F1,
	/* BLD CRC */
	.BLD_LENGTH_ADDR          = 0x3F138,	//0x3F138 ~ 0x3F13A	(3 bytes)
	.ILM_LENGTH_ADDR          = 0x3F118,	//0x3F118 ~ 0x3F11A	(3 bytes)
	.DLM_LENGTH_ADDR          = 0x3F130,	//0x3F130 ~ 0x3F132	(3 bytes)
	.BLD_DES_ADDR             = 0x3F114,	//0x3F114 ~ 0x3F116	(3 bytes)
	.ILM_DES_ADDR             = 0x3F128,	//0x3F128 ~ 0x3F12A	(3 bytes)
	.DLM_DES_ADDR             = 0x3F12C,	//0x3F12C ~ 0x3F12E	(3 bytes)
	.G_ILM_CHECKSUM_ADDR      = 0x3F100,	//0x3F100 ~ 0x3F103	(4 bytes)
	.G_DLM_CHECKSUM_ADDR      = 0x3F104,	//0x3F104 ~ 0x3F107	(4 bytes)
	.R_ILM_CHECKSUM_ADDR      = 0x3F120,	//0x3F120 ~ 0x3F123 (4 bytes)
	.R_DLM_CHECKSUM_ADDR      = 0x3F124,	//0x3F124 ~ 0x3F127 (4 bytes)
	.DMA_CRC_EN_ADDR          = 0x3F136,
	.BLD_ILM_DLM_CRC_ADDR     = 0x3F133,
	.DMA_CRC_FLAG_ADDR        = 0x3F134,
	.SPI_DMA_VAL_ADDR         = 0x3F7D0,
	//Cascade SPI DMA
	.SPI_DMA_LENGTH 		  = 0x3F7C0,
	.SPI_DMA_REM_ADDR		  = 0x3F7C3,
	.SPI_DMA_LOC_ADDR		  = 0x3F7C6,
	.SPI_TX_READ_TRIGGER	  = 0x3F7CB,
	.SPI_TX_WRITE_TRIGGER	  = 0x3F7CC,
	.TX_DMA_2X_EN			  = 0x3F7CE,
	.SPI_DMA_TX_RESET		  = 0x3F7DD,
	.SPI_DMA_RX_RESET		  = 0x3F7DE,
	.RX_DMA_2X_EN			  = 0x3F7E7,
	.LOC_RW_BUF_ADDR		  = 0x20000,
	//SRAM Test
	.MBT_MUX_CTL0             = 0x3F2C0,
	.MBT_MUX_CTL1             = 0x3F2C1,
	.MBT_MUX_CTL2             = 0x3F2C2,
	.MBT_DB                   = 0x3F2C3,
	.MBT_MODE                 = 0x3F2C7,
	.MBT_EN                   = 0x3F2C8,
	.MBT_OP                   = 0x3F2C9,
	.MBT_STATUS               = 0x3F2CE,
	//LPWG DUMP
	.LPWG_DUMP_ADDR           = 0x32710,
};

static const struct nvt_ts_mem_map NT36528_memory_map = {
	.EVENT_BUF_ADDR           = 0x22D00,
	.RAW_PIPE0_ADDR           = 0x24000,
	.RAW_PIPE1_ADDR           = 0x24000,
	.BASELINE_ADDR            = 0x21880,
	.BASELINE_BTN_ADDR        = 0,
	.DIFF_PIPE0_ADDR          = 0x20B00,
	.DIFF_PIPE1_ADDR          = 0x24B00,
	.RAW_BTN_PIPE0_ADDR       = 0,
	.RAW_BTN_PIPE1_ADDR       = 0,
	.DIFF_BTN_PIPE0_ADDR      = 0,
	.DIFF_BTN_PIPE1_ADDR      = 0,
	/* FW History */
	.MMAP_HISTORY_EVENT0      = 0x23768,
	.MMAP_HISTORY_EVENT1      = 0x237A8,
	/* Phase 2 Host Download */
	.BOOT_RDY_ADDR            = 0x3F10D,
	/* BLD CRC */
	.BLD_LENGTH_ADDR          = 0x3F138,	//0x3F138 ~ 0x3F13A	(3 bytes)
	.ILM_LENGTH_ADDR          = 0x3F118,	//0x3F118 ~ 0x3F11A	(3 bytes)
	.DLM_LENGTH_ADDR          = 0x3F130,	//0x3F130 ~ 0x3F132	(3 bytes)
	.BLD_DES_ADDR             = 0x3F114,	//0x3F114 ~ 0x3F116	(3 bytes)
	.ILM_DES_ADDR             = 0x3F128,	//0x3F128 ~ 0x3F12A	(3 bytes)
	.DLM_DES_ADDR             = 0x3F12C,	//0x3F12C ~ 0x3F12E	(3 bytes)
	.G_ILM_CHECKSUM_ADDR      = 0x3F100,	//0x3F100 ~ 0x3F103	(4 bytes)
	.G_DLM_CHECKSUM_ADDR      = 0x3F104,	//0x3F104 ~ 0x3F107	(4 bytes)
	.R_ILM_CHECKSUM_ADDR      = 0x3F120,	//0x3F120 ~ 0x3F123 (4 bytes)
	.R_DLM_CHECKSUM_ADDR      = 0x3F124,	//0x3F124 ~ 0x3F127 (4 bytes)
	.DMA_CRC_EN_ADDR          = 0x3F136,
	.BLD_ILM_DLM_CRC_ADDR     = 0x3F133,
	.DMA_CRC_FLAG_ADDR        = 0x3F134,
	//SRAM Test
	.MBT_MUX_CTL0             = 0x3F2C0,
	.MBT_MUX_CTL1             = 0x3F2C1,
	.MBT_MUX_CTL2             = 0x3F2C2,
	.MBT_DB                   = 0x3F2C3,
	.MBT_MODE                 = 0x3F2C7,
	.MBT_EN                   = 0x3F2C8,
	.MBT_OP                   = 0x3F2C9,
	.MBT_STATUS               = 0x3F2CE,
};

static const struct nvt_ts_mem_map NT36526_memory_map = {
	.EVENT_BUF_ADDR           = 0x22D00,
	.RAW_PIPE0_ADDR           = 0x24000,
	.RAW_PIPE1_ADDR           = 0x24000,
	.BASELINE_ADDR            = 0x21758,
	.BASELINE_BTN_ADDR        = 0,
	.DIFF_PIPE0_ADDR          = 0x20AB0,
	.DIFF_PIPE1_ADDR          = 0x24AB0,
	.RAW_BTN_PIPE0_ADDR       = 0,
	.RAW_BTN_PIPE1_ADDR       = 0,
	.DIFF_BTN_PIPE0_ADDR      = 0,
	.DIFF_BTN_PIPE1_ADDR      = 0,
	/* FW History */
	.MMAP_HISTORY_EVENT0      = 0x23C38,
	.MMAP_HISTORY_EVENT1      = 0x23C78,
	/* Phase 2 Host Download */
	.BOOT_RDY_ADDR            = 0x3F10D,
	/* BLD CRC */
	.BLD_LENGTH_ADDR          = 0x3F138,	//0x3F138 ~ 0x3F13A	(3 bytes)
	.ILM_LENGTH_ADDR          = 0x3F118,	//0x3F118 ~ 0x3F11A	(3 bytes)
	.DLM_LENGTH_ADDR          = 0x3F130,	//0x3F130 ~ 0x3F132	(3 bytes)
	.BLD_DES_ADDR             = 0x3F114,	//0x3F114 ~ 0x3F116	(3 bytes)
	.ILM_DES_ADDR             = 0x3F128,	//0x3F128 ~ 0x3F12A	(3 bytes)
	.DLM_DES_ADDR             = 0x3F12C,	//0x3F12C ~ 0x3F12E	(3 bytes)
	.G_ILM_CHECKSUM_ADDR      = 0x3F100,	//0x3F100 ~ 0x3F103	(4 bytes)
	.G_DLM_CHECKSUM_ADDR      = 0x3F104,	//0x3F104 ~ 0x3F107	(4 bytes)
	.R_ILM_CHECKSUM_ADDR      = 0x3F120,	//0x3F120 ~ 0x3F123 (4 bytes)
	.R_DLM_CHECKSUM_ADDR      = 0x3F124,	//0x3F124 ~ 0x3F127 (4 bytes)
	.DMA_CRC_EN_ADDR          = 0x3F136,
	.BLD_ILM_DLM_CRC_ADDR     = 0x3F133,
	.DMA_CRC_FLAG_ADDR        = 0x3F134,
	//SRAM Test
	.MBT_MUX_CTL0             = 0x3F2C0,
	.MBT_MUX_CTL1             = 0x3F2C1,
	.MBT_MUX_CTL2             = 0x3F2C2,
	.MBT_DB                   = 0x3F2C3,
	.MBT_MODE                 = 0x3F2C7,
	.MBT_EN                   = 0x3F2C8,
	.MBT_OP                   = 0x3F2C9,
	.MBT_STATUS               = 0x3F2CE,
};


static const struct nvt_ts_mem_map NT36675_memory_map = {
	.EVENT_BUF_ADDR           = 0x22D00,
	.RAW_PIPE0_ADDR           = 0x24000,
	.RAW_PIPE1_ADDR           = 0x24000,
	.BASELINE_ADDR            = 0x21B90,
	.BASELINE_BTN_ADDR        = 0,
	.DIFF_PIPE0_ADDR          = 0x20C60,
	.DIFF_PIPE1_ADDR          = 0x24C60,
	.RAW_BTN_PIPE0_ADDR       = 0,
	.RAW_BTN_PIPE1_ADDR       = 0,
	.DIFF_BTN_PIPE0_ADDR      = 0,
	.DIFF_BTN_PIPE1_ADDR      = 0,
	/* FDM selftest */
	.DOZE_GM_S1D_SCAN_RAW_ADDR = 0x22900,
	/* FW History */
	.MMAP_HISTORY_EVENT0      = 0x23D10,
	.MMAP_HISTORY_EVENT1      = 0x23D50,
	/* Phase 2 Host Download */
	.BOOT_RDY_ADDR            = 0x3F10D,
	/* BLD CRC */
	.BLD_LENGTH_ADDR          = 0x3F138,	//0x3F138 ~ 0x3F13A	(3 bytes)
	.ILM_LENGTH_ADDR          = 0x3F118,	//0x3F118 ~ 0x3F11A	(3 bytes)
	.DLM_LENGTH_ADDR          = 0x3F130,	//0x3F130 ~ 0x3F132	(3 bytes)
	.BLD_DES_ADDR             = 0x3F114,	//0x3F114 ~ 0x3F116	(3 bytes)
	.ILM_DES_ADDR             = 0x3F128,	//0x3F128 ~ 0x3F12A	(3 bytes)
	.DLM_DES_ADDR             = 0x3F12C,	//0x3F12C ~ 0x3F12E	(3 bytes)
	.G_ILM_CHECKSUM_ADDR      = 0x3F100,	//0x3F100 ~ 0x3F103	(4 bytes)
	.G_DLM_CHECKSUM_ADDR      = 0x3F104,	//0x3F104 ~ 0x3F107	(4 bytes)
	.R_ILM_CHECKSUM_ADDR      = 0x3F120,	//0x3F120 ~ 0x3F123 (4 bytes)
	.R_DLM_CHECKSUM_ADDR      = 0x3F124,	//0x3F124 ~ 0x3F127 (4 bytes)
	.DMA_CRC_EN_ADDR          = 0x3F136,
	.BLD_ILM_DLM_CRC_ADDR     = 0x3F133,
	.DMA_CRC_FLAG_ADDR        = 0x3F134,
	//SRAM Test
	.MBT_MUX_CTL0             = 0x3F2C0,
	.MBT_MUX_CTL1             = 0x3F2C1,
	.MBT_MUX_CTL2             = 0x3F2C2,
	.MBT_DB                   = 0x3F2C3,
	.MBT_MODE                 = 0x3F2C7,
	.MBT_EN                   = 0x3F2C8,
	.MBT_OP                   = 0x3F2C9,
	.MBT_STATUS               = 0x3F2CE,
	//LPWG DUMP
	.LPWG_DUMP_ADDR           = 0x26AA0,
};


static const struct nvt_ts_mem_map NT36672A_memory_map = {
	.EVENT_BUF_ADDR           = 0x21C00,
	.RAW_PIPE0_ADDR           = 0x20000,
	.RAW_PIPE1_ADDR           = 0x23000,
	.BASELINE_ADDR            = 0x20BFC,
	.BASELINE_BTN_ADDR        = 0x23BFC,
	.DIFF_PIPE0_ADDR          = 0x206DC,
	.DIFF_PIPE1_ADDR          = 0x236DC,
	.RAW_BTN_PIPE0_ADDR       = 0x20510,
	.RAW_BTN_PIPE1_ADDR       = 0x23510,
	.DIFF_BTN_PIPE0_ADDR      = 0x20BF0,
	.DIFF_BTN_PIPE1_ADDR      = 0x23BF0,
	/* FDM selftest */
	.DOZE_GM_S1D_SCAN_RAW_ADDR = 0x23C1C,
	/* FW History */
	.MMAP_HISTORY_EVENT0      = 0x218D6,
	.MMAP_HISTORY_EVENT1      = 0x24B2D,
	/* Phase 2 Host Download */
	.BOOT_RDY_ADDR            = 0x3F10D,
	/* BLD CRC */
	.BLD_LENGTH_ADDR          = 0x3F10E,	//0x3F10E ~ 0x3F10F	(2 bytes)
	.ILM_LENGTH_ADDR          = 0x3F118,	//0x3F118 ~ 0x3F119	(2 bytes)
	.DLM_LENGTH_ADDR          = 0x3F130,	//0x3F130 ~ 0x3F131	(2 bytes)
	.BLD_DES_ADDR             = 0x3F114,	//0x3F114 ~ 0x3F116	(3 bytes)
	.ILM_DES_ADDR             = 0x3F128,	//0x3F128 ~ 0x3F12A	(3 bytes)
	.DLM_DES_ADDR             = 0x3F12C,	//0x3F12C ~ 0x3F12E	(3 bytes)
	.G_ILM_CHECKSUM_ADDR      = 0x3F100,	//0x3F100 ~ 0x3F103	(4 bytes)
	.G_DLM_CHECKSUM_ADDR      = 0x3F104,	//0x3F104 ~ 0x3F107	(4 bytes)
	.R_ILM_CHECKSUM_ADDR      = 0x3F120,	//0x3F120 ~ 0x3F123 (4 bytes)
	.R_DLM_CHECKSUM_ADDR      = 0x3F124,	//0x3F124 ~ 0x3F127 (4 bytes)
	.DMA_CRC_EN_ADDR          = 0x3F132,
	.BLD_ILM_DLM_CRC_ADDR     = 0x3F133,
	.DMA_CRC_FLAG_ADDR        = 0x3F134,
	/* SRAM Test */
	//SRAM Test
	.MBT_MUX_CTL0             = 0x3F2C0,
	.MBT_MUX_CTL1             = 0x3F2C1,
	.MBT_MUX_CTL2             = 0x3F2C2,
	.MBT_DB                   = 0x3F2C3,
	.MBT_MODE                 = 0x3F2C7,
	.MBT_EN                   = 0x3F2C8,
	.MBT_OP                   = 0x3F2C9,
	.MBT_STATUS               = 0x3F2CE,
	//LPWG DUMP
	.LPWG_DUMP_ADDR           = 0x24698,
};

static const struct nvt_ts_mem_map NT36772_memory_map = {
	.EVENT_BUF_ADDR           = 0x11E00,
	.RAW_PIPE0_ADDR           = 0x10000,
	.RAW_PIPE1_ADDR           = 0x12000,
	.BASELINE_ADDR            = 0x10E70,
	.BASELINE_BTN_ADDR        = 0x12E70,
	.DIFF_PIPE0_ADDR          = 0x10830,
	.DIFF_PIPE1_ADDR          = 0x12830,
	.RAW_BTN_PIPE0_ADDR       = 0x10E60,
	.RAW_BTN_PIPE1_ADDR       = 0x12E60,
	.DIFF_BTN_PIPE0_ADDR      = 0x10E68,
	.DIFF_BTN_PIPE1_ADDR      = 0x12E68,
	/* FDM selftest */
	.DOZE_GM_S1D_SCAN_RAW_ADDR = 0x143D0,
	/* FW History */
	.MMAP_HISTORY_EVENT0      = 0,
	.MMAP_HISTORY_EVENT1      = 0,
	/* Phase 2 Host Download */
	.BOOT_RDY_ADDR            = 0x1F141,
	.POR_CD_ADDR              = 0x1F61C,
	/* BLD CRC */
	.R_ILM_CHECKSUM_ADDR      = 0x1BF00,
};

static const struct nvt_ts_mem_map NT36525_memory_map = {
	.EVENT_BUF_ADDR           = 0x11A00,
	.RAW_PIPE0_ADDR           = 0x10000,
	.RAW_PIPE1_ADDR           = 0x12000,
	.BASELINE_ADDR            = 0x10B08,
	.BASELINE_BTN_ADDR        = 0x12B08,
	.DIFF_PIPE0_ADDR          = 0x1064C,
	.DIFF_PIPE1_ADDR          = 0x1264C,
	.RAW_BTN_PIPE0_ADDR       = 0x10634,
	.RAW_BTN_PIPE1_ADDR       = 0x12634,
	.DIFF_BTN_PIPE0_ADDR      = 0x10AFC,
	.DIFF_BTN_PIPE1_ADDR      = 0x12AFC,
	/* FDM selftest */
	.DOZE_GM_S1D_SCAN_RAW_ADDR = 0x12B28,
	/* FW History */
	.MMAP_HISTORY_EVENT0      = 0,
	.MMAP_HISTORY_EVENT1      = 0,
	/* Phase 2 Host Download */
	.BOOT_RDY_ADDR            = 0x1F141,
	.POR_CD_ADDR              = 0x1F61C,
	/* BLD CRC */
	.R_ILM_CHECKSUM_ADDR      = 0x1BF00,
};

static const struct nvt_ts_mem_map NT36676F_memory_map = {
	.EVENT_BUF_ADDR           = 0x11A00,
	.RAW_PIPE0_ADDR           = 0x10000,
	.RAW_PIPE1_ADDR           = 0x12000,
	.BASELINE_ADDR            = 0x10B08,
	.BASELINE_BTN_ADDR        = 0x12B08,
	.DIFF_PIPE0_ADDR          = 0x1064C,
	.DIFF_PIPE1_ADDR          = 0x1264C,
	.RAW_BTN_PIPE0_ADDR       = 0x10634,
	.RAW_BTN_PIPE1_ADDR       = 0x12634,
	.DIFF_BTN_PIPE0_ADDR      = 0x10AFC,
	.DIFF_BTN_PIPE1_ADDR      = 0x12AFC,
	/* FDM selftest */
	.DOZE_GM_S1D_SCAN_RAW_ADDR = 0x12B28,
	/* FW History */
	.MMAP_HISTORY_EVENT0      = 0,
	.MMAP_HISTORY_EVENT1      = 0,
	/* Phase 2 Host Download */
	.BOOT_RDY_ADDR            = 0x1F141,
	.POR_CD_ADDR              = 0x1F61C,
	/* BLD CRC */
	.R_ILM_CHECKSUM_ADDR      = 0x1BF00,
};

static struct nvt_ts_hw_info NT36528_hw_info = {
	.hw_crc    = HWCRC_LEN_3Bytes,
	.auto_copy = AUTOCOPY_NOSUPPORT,
	.hw_regs   = &hw_reg_addr_info_old_w_isp,
};

static struct nvt_ts_hw_info NT36523_hw_info = {
	.hw_crc    = HWCRC_LEN_3Bytes,
	.auto_copy = CHECK_SPI_DMA_TX_INFO,
	.hw_regs   = &hw_reg_addr_info_old_w_isp,
};

static struct nvt_ts_hw_info NT36526_hw_info = {
	.hw_crc    = HWCRC_LEN_3Bytes,
	.auto_copy = AUTOCOPY_NOSUPPORT,
	.hw_regs   = &hw_reg_addr_info_old_w_isp,
};

static struct nvt_ts_hw_info NT36675_hw_info = {
	.hw_crc    = HWCRC_LEN_3Bytes,
	.auto_copy = AUTOCOPY_NOSUPPORT,
	.hw_regs   = &hw_reg_addr_info_old_w_isp,
};

static struct nvt_ts_hw_info NT36672A_hw_info = {
	.hw_crc    = HWCRC_LEN_2Bytes,
	.auto_copy = AUTOCOPY_NOSUPPORT,
	.hw_regs   = &hw_reg_addr_info_old_w_isp,
};

static struct nvt_ts_hw_info NT36772_hw_info = {
	.hw_crc    = HWCRC_NOSUPPORT,
	.auto_copy = AUTOCOPY_NOSUPPORT,
	.hw_regs   = &hw_reg_addr_info_legacy_w_isp,
};

static struct nvt_ts_hw_info NT36525_hw_info = {
	.hw_crc    = HWCRC_NOSUPPORT,
	.auto_copy = AUTOCOPY_NOSUPPORT,
	.hw_regs   = &hw_reg_addr_info_legacy_w_isp,
};

static struct nvt_ts_hw_info NT36676F_hw_info = {
	.hw_crc    = HWCRC_NOSUPPORT,
	.auto_copy = AUTOCOPY_NOSUPPORT,
	.hw_regs   = &hw_reg_addr_info_legacy_w_isp,
};

#define NVT_ID_BYTE_MAX 6
struct nvt_ts_trim_id_table {
	uint8_t id[NVT_ID_BYTE_MAX];
	uint8_t mask[NVT_ID_BYTE_MAX];
	const struct nvt_ts_mem_map *mmap;
	const struct nvt_ts_hw_info *hwinfo;
};

static const struct nvt_ts_trim_id_table trim_id_table[] = {
	{.id = {0x17, 0xFF, 0xFF, 0x23, 0x65, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36523N_memory_map,  .hwinfo = &NT36523_hw_info},
	{.id = {0xFF, 0xFF, 0xFF, 0x28, 0x65, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36528_memory_map,  .hwinfo = &NT36528_hw_info},
	{.id = {0x0C, 0xFF, 0xFF, 0x25, 0x65, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36672A_memory_map, .hwinfo = &NT36672A_hw_info},
	{.id = {0x1C, 0xFF, 0xFF, 0x25, 0x65, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36672A_memory_map, .hwinfo = &NT36672A_hw_info},
	{.id = {0x0C, 0xFF, 0xFF, 0x72, 0x66, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36675_memory_map,  .hwinfo = &NT36675_hw_info},
	{.id = {0xFF, 0xFF, 0xFF, 0x26, 0x65, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36526_memory_map,  .hwinfo = &NT36526_hw_info},
	{.id = {0xFF, 0xFF, 0xFF, 0x75, 0x66, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36675_memory_map,  .hwinfo = &NT36675_hw_info},
	{.id = {0x0B, 0xFF, 0xFF, 0x72, 0x66, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36672A_memory_map, .hwinfo = &NT36672A_hw_info},
	{.id = {0x0B, 0xFF, 0xFF, 0x82, 0x66, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36672A_memory_map, .hwinfo = &NT36672A_hw_info},
	{.id = {0x0B, 0xFF, 0xFF, 0x25, 0x65, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36672A_memory_map, .hwinfo = &NT36672A_hw_info},
	{.id = {0x0A, 0xFF, 0xFF, 0x72, 0x65, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36672A_memory_map, .hwinfo = &NT36672A_hw_info},
	{.id = {0x0A, 0xFF, 0xFF, 0x72, 0x66, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36672A_memory_map, .hwinfo = &NT36672A_hw_info},
	{.id = {0x0A, 0xFF, 0xFF, 0x82, 0x66, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36672A_memory_map, .hwinfo = &NT36672A_hw_info},
	{.id = {0x0A, 0xFF, 0xFF, 0x70, 0x66, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36672A_memory_map, .hwinfo = &NT36672A_hw_info},
	{.id = {0x0B, 0xFF, 0xFF, 0x70, 0x66, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36672A_memory_map, .hwinfo = &NT36672A_hw_info},
	{.id = {0x0A, 0xFF, 0xFF, 0x72, 0x67, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36672A_memory_map, .hwinfo = &NT36672A_hw_info},
	{.id = {0x55, 0x00, 0xFF, 0x00, 0x00, 0x00}, .mask = {1, 1, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map,  .hwinfo = &NT36772_hw_info},
	{.id = {0x55, 0x72, 0xFF, 0x00, 0x00, 0x00}, .mask = {1, 1, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map,  .hwinfo = &NT36772_hw_info},
	{.id = {0xAA, 0x00, 0xFF, 0x00, 0x00, 0x00}, .mask = {1, 1, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map,  .hwinfo = &NT36772_hw_info},
	{.id = {0xAA, 0x72, 0xFF, 0x00, 0x00, 0x00}, .mask = {1, 1, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map,  .hwinfo = &NT36772_hw_info},
	{.id = {0xFF, 0xFF, 0xFF, 0x72, 0x67, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map,  .hwinfo = &NT36772_hw_info},
	{.id = {0xFF, 0xFF, 0xFF, 0x70, 0x66, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map,  .hwinfo = &NT36772_hw_info},
	{.id = {0xFF, 0xFF, 0xFF, 0x70, 0x67, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map,  .hwinfo = &NT36772_hw_info},
	{.id = {0xFF, 0xFF, 0xFF, 0x72, 0x66, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map,  .hwinfo = &NT36772_hw_info},
	{.id = {0xFF, 0xFF, 0xFF, 0x25, 0x65, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36525_memory_map,  .hwinfo = &NT36525_hw_info},
	{.id = {0xFF, 0xFF, 0xFF, 0x76, 0x66, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36676F_memory_map, .hwinfo = &NT36676F_hw_info}
};
