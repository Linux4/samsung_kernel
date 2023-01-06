/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fab/s6e3fab_canvas_panel_poc.h
 *
 * Header file for POC Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FAB_CANVAS_PANEL_POC_H__
#define __S6E3FAB_CANVAS_PANEL_POC_H__

#include "../panel.h"
#include "../panel_poc.h"

#define CANVAS_BDIV	(BIT_RATE_DIV_32)
#define CANVAS_EXEC_USEC	(2)
#define CANVAS_QD_DONE_MDELAY		(30)
#define CANVAS_RD_DONE_UDELAY		(70)
#define CANVAS_WR_ENABLE_01_UDELAY		(20)
#define CANVAS_WR_ENABLE_02_MDELAY		(10)
#define CANVAS_WR_EXEC_UDELAY		(400)
#define CANVAS_WR_DONE_MDELAY		(1)
#ifdef CONFIG_SUPPORT_POC_FLASH
#define CANVAS_ER_4K_DONE_MDELAY		(400)
#define CANVAS_ER_32K_DONE_MDELAY		(800)
#define CANVAS_ER_64K_DONE_MDELAY		(1000)
#endif

#define CANVAS_POC_IMG_ADDR	(0)
#define CANVAS_POC_IMG_SIZE	(551186)

#ifdef CONFIG_SUPPORT_DIM_FLASH
#define CANVAS_POC_DIM_DATA_ADDR	(0xF0000)
#define CANVAS_POC_DIM_DATA_SIZE (S6E3FAB_DIM_FLASH_DATA_SIZE)
#define CANVAS_POC_DIM_CHECKSUM_ADDR	(CANVAS_POC_DIM_DATA_ADDR + S6E3FAB_DIM_FLASH_CHECKSUM_OFS)
#define CANVAS_POC_DIM_CHECKSUM_SIZE (S6E3FAB_DIM_FLASH_CHECKSUM_LEN)
#define CANVAS_POC_DIM_MAGICNUM_ADDR	(CANVAS_POC_DIM_DATA_ADDR + S6E3FAB_DIM_FLASH_MAGICNUM_OFS)
#define CANVAS_POC_DIM_MAGICNUM_SIZE (S6E3FAB_DIM_FLASH_MAGICNUM_LEN)
#define CANVAS_POC_DIM_TOTAL_SIZE (S6E3FAB_DIM_FLASH_TOTAL_SIZE)

#define CANVAS_POC_MTP_DATA_ADDR	(0xF2800)
#define CANVAS_POC_MTP_DATA_SIZE (S6E3FAB_DIM_FLASH_MTP_DATA_SIZE)
#define CANVAS_POC_MTP_CHECKSUM_ADDR	(CANVAS_POC_MTP_DATA_ADDR + S6E3FAB_DIM_FLASH_MTP_CHECKSUM_OFS)
#define CANVAS_POC_MTP_CHECKSUM_SIZE (S6E3FAB_DIM_FLASH_MTP_CHECKSUM_LEN)
#define CANVAS_POC_MTP_TOTAL_SIZE (S6E3FAB_DIM_FLASH_MTP_TOTAL_SIZE)

#define CANVAS_POC_DIM_120HZ_DATA_ADDR	(0xF3000)
#define CANVAS_POC_DIM_120HZ_DATA_SIZE (S6E3FAB_DIM_FLASH_120HZ_DATA_SIZE)
#define CANVAS_POC_DIM_120HZ_CHECKSUM_ADDR	(CANVAS_POC_DIM_120HZ_DATA_ADDR + S6E3FAB_DIM_FLASH_120HZ_CHECKSUM_OFS)
#define CANVAS_POC_DIM_120HZ_CHECKSUM_SIZE (S6E3FAB_DIM_FLASH_120HZ_CHECKSUM_LEN)
#define CANVAS_POC_DIM_120HZ_MAGICNUM_ADDR	(CANVAS_POC_DIM_120HZ_DATA_ADDR + S6E3FAB_DIM_FLASH_120HZ_MAGICNUM_OFS)
#define CANVAS_POC_DIM_120HZ_MAGICNUM_SIZE (S6E3FAB_DIM_FLASH_120HZ_MAGICNUM_LEN)
#define CANVAS_POC_DIM_120HZ_TOTAL_SIZE (S6E3FAB_DIM_FLASH_120HZ_TOTAL_SIZE)

#define CANVAS_POC_MTP_120HZ_DATA_ADDR	(0xF5800)
#define CANVAS_POC_MTP_120HZ_DATA_SIZE (S6E3FAB_DIM_FLASH_120HZ_MTP_DATA_SIZE)
#define CANVAS_POC_MTP_120HZ_CHECKSUM_ADDR	(CANVAS_POC_MTP_120HZ_DATA_ADDR + S6E3FAB_DIM_FLASH_120HZ_MTP_CHECKSUM_OFS)
#define CANVAS_POC_MTP_120HZ_CHECKSUM_SIZE (S6E3FAB_DIM_FLASH_120HZ_MTP_CHECKSUM_LEN)
#define CANVAS_POC_MTP_120HZ_TOTAL_SIZE (S6E3FAB_DIM_FLASH_120HZ_MTP_TOTAL_SIZE)

#define CANVAS_POC_DIM_60HZ_HS_DATA_ADDR	(0xF6000)
#define CANVAS_POC_DIM_60HZ_HS_DATA_SIZE (S6E3FAB_DIM_FLASH_60HZ_HS_DATA_SIZE)
#define CANVAS_POC_DIM_60HZ_HS_CHECKSUM_ADDR	(CANVAS_POC_DIM_60HZ_HS_DATA_ADDR + S6E3FAB_DIM_FLASH_60HZ_HS_CHECKSUM_OFS)
#define CANVAS_POC_DIM_60HZ_HS_CHECKSUM_SIZE (S6E3FAB_DIM_FLASH_60HZ_HS_CHECKSUM_LEN)
#define CANVAS_POC_DIM_60HZ_HS_MAGICNUM_ADDR	(CANVAS_POC_DIM_60HZ_HS_DATA_ADDR + S6E3FAB_DIM_FLASH_60HZ_HS_MAGICNUM_OFS)
#define CANVAS_POC_DIM_60HZ_HS_MAGICNUM_SIZE (S6E3FAB_DIM_FLASH_60HZ_HS_MAGICNUM_LEN)
#define CANVAS_POC_DIM_60HZ_HS_TOTAL_SIZE (S6E3FAB_DIM_FLASH_60HZ_HS_TOTAL_SIZE)

#define CANVAS_POC_MTP_60HZ_HS_DATA_ADDR	(0xF8800)
#define CANVAS_POC_MTP_60HZ_HS_DATA_SIZE (S6E3FAB_DIM_FLASH_60HZ_HS_MTP_DATA_SIZE)
#define CANVAS_POC_MTP_60HZ_HS_CHECKSUM_ADDR	(CANVAS_POC_MTP_60HZ_HS_DATA_ADDR + S6E3FAB_DIM_FLASH_60HZ_HS_MTP_CHECKSUM_OFS)
#define CANVAS_POC_MTP_60HZ_HS_CHECKSUM_SIZE (S6E3FAB_DIM_FLASH_60HZ_HS_MTP_CHECKSUM_LEN)
#define CANVAS_POC_MTP_60HZ_HS_TOTAL_SIZE (S6E3FAB_DIM_FLASH_60HZ_HS_MTP_TOTAL_SIZE)
#endif

#define CANVAS_POC_MCD_ADDR	(0xB8000)
#define CANVAS_POC_MCD_SIZE (S6E3FAB_FLASH_MCD_LEN)

#ifdef CONFIG_SUPPORT_POC_SPI
#define CANVAS_SPI_ER_DONE_MDELAY		(35)
#define CANVAS_SPI_STATUS_WR_DONE_MDELAY		(15)

#ifdef PANEL_POC_SPI_BUSY_WAIT
#define CANVAS_SPI_WR_DONE_UDELAY		(50)
#else
#define CANVAS_SPI_WR_DONE_UDELAY		(400)
#endif
#endif

/* ===================================================================================== */
/* ============================== [S6E3FAB MAPPING TABLE] ============================== */
/* ===================================================================================== */
static struct maptbl canvas_poc_maptbl[] = {
	[POC_WR_ADDR_MAPTBL] = DEFINE_0D_MAPTBL(canvas_poc_wr_addr_table, init_common_table, NULL, copy_poc_wr_addr_maptbl),
	[POC_RD_ADDR_MAPTBL] = DEFINE_0D_MAPTBL(canvas_poc_rd_addr_table, init_common_table, NULL, copy_poc_rd_addr_maptbl),
	[POC_WR_DATA_MAPTBL] = DEFINE_0D_MAPTBL(canvas_poc_wr_data_table, init_common_table, NULL, copy_poc_wr_data_maptbl),
#ifdef CONFIG_SUPPORT_POC_FLASH
	[POC_ER_ADDR_MAPTBL] = DEFINE_0D_MAPTBL(canvas_poc_er_addr_table, init_common_table, NULL, copy_poc_er_addr_maptbl),
#endif
#ifdef CONFIG_SUPPORT_POC_SPI
	[POC_SPI_READ_ADDR_MAPTBL] = DEFINE_0D_MAPTBL(canvas_poc_spi_read_table, init_common_table, NULL, copy_poc_rd_addr_maptbl),
	[POC_SPI_WRITE_ADDR_MAPTBL] = DEFINE_0D_MAPTBL(canvas_poc_spi_write_addr_table, init_common_table, NULL, copy_poc_wr_addr_maptbl),
	[POC_SPI_WRITE_DATA_MAPTBL] = DEFINE_0D_MAPTBL(canvas_poc_spi_write_data_table, init_common_table, NULL, copy_poc_wr_data_maptbl),
	[POC_SPI_ERASE_ADDR_MAPTBL] = DEFINE_0D_MAPTBL(canvas_poc_spi_erase_addr_table, init_common_table, NULL, copy_poc_er_addr_maptbl),
#endif
};

/* ===================================================================================== */
/* ============================== [S6E3FAB COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 CANVAS_KEY2_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static u8 CANVAS_KEY2_DISABLE[] = { 0xF0, 0xA5, 0xA5 };
static u8 CANVAS_POC_KEY_ENABLE[] = { 0xF1, 0xF1, 0xA2 };
static u8 CANVAS_POC_KEY_DISABLE[] = { 0xF1, 0xA5, 0xA5 };
static u8 CANVAS_POC_PGM_ENABLE[] = { 0xC0, 0x02 };
static u8 CANVAS_POC_PGM_DISABLE[] = { 0xC0, 0x00 };
static u8 CANVAS_POC_EXEC[] = { 0xC0, 0x03 };
#ifdef CONFIG_SUPPORT_POC_FLASH
static u8 CANVAS_POC_ER_CTRL_01[] = { 0xC1, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04 };
static u8 CANVAS_POC_ER_CTRL_02[] = { 0xC1, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x10, 0x04 };
static u8 CANVAS_POC_ER_4K_STT[] = { 0xC1, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x04 };
static u8 CANVAS_POC_ER_32K_STT[] = { 0xC1, 0x00, 0x00, 0x00, 0x52, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x04 };
static u8 CANVAS_POC_ER_64K_STT[] = { 0xC1, 0x00, 0x00, 0x00, 0xD8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x04 };
#endif
static u8 CANVAS_POC_RD_ENABLE[] = { 0xC1, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, CANVAS_BDIV };
static u8 CANVAS_POC_QD_ENABLE[] = { 0xC1, 0x00, 0x00, 0x00, 0x01, 0x5E, 0x02, 0x00, 0x00, 0x00, 0x00, 0x10, 0x04 };
static u8 CANVAS_POC_RD_STT[] = { 0xC1, 0x00, 0x00, 0x00, 0x6B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, CANVAS_BDIV, 0x01 };

static u8 CANVAS_POC_WR_ENABLE_01[] = { 0xC1, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04 };
static u8 CANVAS_POC_WR_ENABLE_02[] = { 0xC1, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x10, 0x04 };
static u8 CANVAS_POC_WR_END[] = { 0xC1, 0x00, 0x00, 0xFF, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x04, 0x01 };
static u8 CANVAS_POC_WR_EXIT[] = { 0xC1, 0x00, 0x00, 0x00, 0x01, 0x5E, 0x02, 0x00, 0x00, 0x00, 0x00, 0x10, 0x04 };
static u8 CANVAS_POC_WR_STT_DAT[] = { 0xC1, 0x00, 0x00, 0xFF, 0x32, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, //addr
	0x40, 0x04, 0x01,
	//data
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};


static DEFINE_STATIC_PACKET(canvas_level2_key_enable, DSI_PKT_TYPE_WR, CANVAS_KEY2_ENABLE, 0);
static DEFINE_STATIC_PACKET(canvas_level2_key_disable, DSI_PKT_TYPE_WR, CANVAS_KEY2_DISABLE, 0);
static DEFINE_STATIC_PACKET(canvas_poc_key_enable, DSI_PKT_TYPE_WR, CANVAS_POC_KEY_ENABLE, 0);
static DEFINE_STATIC_PACKET(canvas_poc_key_disable, DSI_PKT_TYPE_WR, CANVAS_POC_KEY_DISABLE, 0);
static DEFINE_STATIC_PACKET(canvas_poc_pgm_enable, DSI_PKT_TYPE_WR, CANVAS_POC_PGM_ENABLE, 0);
static DEFINE_STATIC_PACKET(canvas_poc_pgm_disable, DSI_PKT_TYPE_WR, CANVAS_POC_PGM_DISABLE, 0);
#ifdef CONFIG_SUPPORT_POC_FLASH
static DEFINE_STATIC_PACKET(canvas_poc_er_ctrl_01, DSI_PKT_TYPE_WR, CANVAS_POC_ER_CTRL_01, 0);
static DEFINE_STATIC_PACKET(canvas_poc_er_ctrl_02, DSI_PKT_TYPE_WR, CANVAS_POC_ER_CTRL_02, 0);
static DEFINE_PKTUI(canvas_poc_er_4k_stt, &canvas_poc_maptbl[POC_ER_ADDR_MAPTBL], 5);
static DEFINE_VARIABLE_PACKET(canvas_poc_er_4k_stt, DSI_PKT_TYPE_WR, CANVAS_POC_ER_4K_STT, 0);
static DEFINE_PANEL_MDELAY(canvas_poc_wait_er_4k_done, CANVAS_ER_4K_DONE_MDELAY);
static DEFINE_PKTUI(canvas_poc_er_32k_stt, &canvas_poc_maptbl[POC_ER_ADDR_MAPTBL], 5);
static DEFINE_VARIABLE_PACKET(canvas_poc_er_32k_stt, DSI_PKT_TYPE_WR, CANVAS_POC_ER_32K_STT, 0);
static DEFINE_PANEL_MDELAY(canvas_poc_wait_er_32k_done, CANVAS_ER_32K_DONE_MDELAY);
static DEFINE_PKTUI(canvas_poc_er_64k_stt, &canvas_poc_maptbl[POC_ER_ADDR_MAPTBL], 5);
static DEFINE_VARIABLE_PACKET(canvas_poc_er_64k_stt, DSI_PKT_TYPE_WR, CANVAS_POC_ER_64K_STT, 0);
static DEFINE_PANEL_MDELAY(canvas_poc_wait_er_64k_done, CANVAS_ER_64K_DONE_MDELAY);
#endif
static DEFINE_STATIC_PACKET(canvas_poc_exec, DSI_PKT_TYPE_WR, CANVAS_POC_EXEC, 0);
static DEFINE_STATIC_PACKET(canvas_poc_rd_enable, DSI_PKT_TYPE_WR, CANVAS_POC_RD_ENABLE, 0);
static DEFINE_STATIC_PACKET(canvas_poc_qd_enable, DSI_PKT_TYPE_WR, CANVAS_POC_QD_ENABLE, 0);
static DEFINE_STATIC_PACKET(canvas_poc_wr_enable_01, DSI_PKT_TYPE_WR, CANVAS_POC_WR_ENABLE_01, 0);
static DEFINE_STATIC_PACKET(canvas_poc_wr_enable_02, DSI_PKT_TYPE_WR, CANVAS_POC_WR_ENABLE_02, 0);
static DEFINE_STATIC_PACKET(canvas_poc_wr_end, DSI_PKT_TYPE_WR, CANVAS_POC_WR_END, 0);
static DEFINE_STATIC_PACKET(canvas_poc_wr_exit, DSI_PKT_TYPE_WR, CANVAS_POC_WR_EXIT, 0);
static DEFINE_PKTUI(canvas_poc_rd_stt, &canvas_poc_maptbl[POC_RD_ADDR_MAPTBL], 8);
static DEFINE_VARIABLE_PACKET(canvas_poc_rd_stt, DSI_PKT_TYPE_WR, CANVAS_POC_RD_STT, 0);
static DECLARE_PKTUI(canvas_poc_wr_stt_dat) = {
	{ .offset = 8, .maptbl = &canvas_poc_maptbl[POC_WR_ADDR_MAPTBL] },
	{ .offset = 14, .maptbl = &canvas_poc_maptbl[POC_WR_DATA_MAPTBL] },
};
static DEFINE_VARIABLE_PACKET(canvas_poc_wr_stt_dat, DSI_PKT_TYPE_WR, CANVAS_POC_WR_STT_DAT, 0);

static DEFINE_PANEL_UDELAY_NO_SLEEP(canvas_poc_wait_exec, CANVAS_EXEC_USEC);
static DEFINE_PANEL_UDELAY_NO_SLEEP(canvas_poc_wait_rd_done, CANVAS_RD_DONE_UDELAY);
static DEFINE_PANEL_UDELAY_NO_SLEEP(canvas_poc_wait_wr_exec, CANVAS_WR_EXEC_UDELAY);
static DEFINE_PANEL_MDELAY(canvas_poc_wait_wr_done, CANVAS_WR_DONE_MDELAY);
static DEFINE_PANEL_UDELAY_NO_SLEEP(canvas_poc_wait_wr_enable_01, CANVAS_WR_ENABLE_01_UDELAY);
static DEFINE_PANEL_MDELAY(canvas_poc_wait_wr_enable_02, CANVAS_WR_ENABLE_02_MDELAY);
static DEFINE_PANEL_MDELAY(canvas_poc_wait_qd_status, CANVAS_QD_DONE_MDELAY);

#ifdef CONFIG_SUPPORT_POC_SPI
static u8 CANVAS_POC_SPI_SET_STATUS1_INIT[] = { 0x01, 0x00 };
static u8 CANVAS_POC_SPI_SET_STATUS2_INIT[] = { 0x31, 0x00 };
static u8 CANVAS_POC_SPI_SET_STATUS1_EXIT[] = { 0x01, 0x5C };
static u8 CANVAS_POC_SPI_SET_STATUS2_EXIT[] = { 0x31, 0x02 };
static DEFINE_STATIC_PACKET(canvas_poc_spi_set_status1_init, SPI_PKT_TYPE_WR, CANVAS_POC_SPI_SET_STATUS1_INIT, 0);
static DEFINE_STATIC_PACKET(canvas_poc_spi_set_status2_init, SPI_PKT_TYPE_WR, CANVAS_POC_SPI_SET_STATUS2_INIT, 0);
static DEFINE_STATIC_PACKET(canvas_poc_spi_set_status1_exit, SPI_PKT_TYPE_WR, CANVAS_POC_SPI_SET_STATUS1_EXIT, 0);
static DEFINE_STATIC_PACKET(canvas_poc_spi_set_status2_exit, SPI_PKT_TYPE_WR, CANVAS_POC_SPI_SET_STATUS2_EXIT, 0);

static u8 CANVAS_POC_SPI_STATUS1[] = { 0x05 };
static u8 CANVAS_POC_SPI_STATUS2[] = { 0x35 };
static u8 CANVAS_POC_SPI_READ[] = { 0x03, 0x00, 0x00, 0x00 };
static u8 CANVAS_POC_SPI_ERASE_4K[] = { 0x20, 0x00, 0x00, 0x00 };
static u8 CANVAS_POC_SPI_ERASE_32K[] = { 0x52, 0x00, 0x00, 0x00 };
static u8 CANVAS_POC_SPI_ERASE_64K[] = { 0xD8, 0x00, 0x00, 0x00 };
static u8 CANVAS_POC_SPI_WRITE_ENABLE[] = { 0x06 };
static u8 CANVAS_POC_SPI_WRITE[260] = { 0x02, 0x00, 0x00, 0x00, };
static DEFINE_STATIC_PACKET(canvas_poc_spi_status1, SPI_PKT_TYPE_SETPARAM, CANVAS_POC_SPI_STATUS1, 0);
static DEFINE_STATIC_PACKET(canvas_poc_spi_status2, SPI_PKT_TYPE_SETPARAM, CANVAS_POC_SPI_STATUS2, 0);
static DEFINE_STATIC_PACKET(canvas_poc_spi_write_enable, SPI_PKT_TYPE_WR, CANVAS_POC_SPI_WRITE_ENABLE, 0);
static DEFINE_PKTUI(canvas_poc_spi_erase_4k, &canvas_poc_maptbl[POC_SPI_ERASE_ADDR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas_poc_spi_erase_4k, SPI_PKT_TYPE_WR, CANVAS_POC_SPI_ERASE_4K, 0);
static DEFINE_PKTUI(canvas_poc_spi_erase_32k, &canvas_poc_maptbl[POC_SPI_ERASE_ADDR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas_poc_spi_erase_32k, SPI_PKT_TYPE_WR, CANVAS_POC_SPI_ERASE_32K, 0);
static DEFINE_PKTUI(canvas_poc_spi_erase_64k, &canvas_poc_maptbl[POC_SPI_ERASE_ADDR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas_poc_spi_erase_64k, SPI_PKT_TYPE_WR, CANVAS_POC_SPI_ERASE_64K, 0);

static DECLARE_PKTUI(canvas_poc_spi_write) = {
	{ .offset = 1, .maptbl = &canvas_poc_maptbl[POC_SPI_WRITE_ADDR_MAPTBL] },
	{ .offset = 4, .maptbl = &canvas_poc_maptbl[POC_SPI_WRITE_DATA_MAPTBL] },
};
static DEFINE_VARIABLE_PACKET(canvas_poc_spi_write, SPI_PKT_TYPE_WR, CANVAS_POC_SPI_WRITE, 0);
static DEFINE_PKTUI(canvas_poc_spi_rd_addr, &canvas_poc_maptbl[POC_SPI_READ_ADDR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas_poc_spi_rd_addr, SPI_PKT_TYPE_SETPARAM, CANVAS_POC_SPI_READ, 0);
static DEFINE_PANEL_UDELAY_NO_SLEEP(canvas_poc_spi_wait_write, CANVAS_SPI_WR_DONE_UDELAY);
static DEFINE_PANEL_MDELAY(canvas_poc_spi_wait_erase, CANVAS_SPI_ER_DONE_MDELAY);
static DEFINE_PANEL_MDELAY(canvas_poc_spi_wait_status, CANVAS_SPI_STATUS_WR_DONE_MDELAY);
#endif

#ifdef CONFIG_SUPPORT_POC_FLASH
static void *canvas_poc_erase_enter_cmdtbl[] = {
	&PKTINFO(canvas_level2_key_enable),
	&PKTINFO(canvas_poc_key_enable),
	&PKTINFO(canvas_poc_pgm_enable),
	&PKTINFO(canvas_poc_er_ctrl_01),
	&PKTINFO(canvas_poc_exec),
	&DLYINFO(canvas_poc_wait_wr_enable_01),
	&PKTINFO(canvas_poc_er_ctrl_02),
	&PKTINFO(canvas_poc_exec),
	&DLYINFO(canvas_poc_wait_wr_enable_02),
};

static void *canvas_poc_erase_4k_cmdtbl[] = {
	&PKTINFO(canvas_poc_er_ctrl_01),
	&PKTINFO(canvas_poc_exec),
	&PKTINFO(canvas_poc_er_4k_stt),
	&PKTINFO(canvas_poc_exec),
	&DLYINFO(canvas_poc_wait_er_4k_done),
};

static void *canvas_poc_erase_32k_cmdtbl[] = {
	&PKTINFO(canvas_poc_er_ctrl_01),
	&PKTINFO(canvas_poc_exec),
	&PKTINFO(canvas_poc_er_32k_stt),
	&PKTINFO(canvas_poc_exec),
	&DLYINFO(canvas_poc_wait_er_32k_done),
};

static void *canvas_poc_erase_64k_cmdtbl[] = {
	&PKTINFO(canvas_poc_er_ctrl_01),
	&PKTINFO(canvas_poc_exec),
	&PKTINFO(canvas_poc_er_64k_stt),
	&PKTINFO(canvas_poc_exec),
	&DLYINFO(canvas_poc_wait_er_64k_done),
};

static void *canvas_poc_erase_exit_cmdtbl[] = {
	&PKTINFO(canvas_poc_pgm_disable),
	&PKTINFO(canvas_poc_key_disable),
	&PKTINFO(canvas_level2_key_disable),
};
#endif

static void *canvas_poc_wr_enter_cmdtbl[] = {
	&PKTINFO(canvas_level2_key_enable),
	&PKTINFO(canvas_poc_key_enable),
	&PKTINFO(canvas_poc_pgm_enable),
	&PKTINFO(canvas_poc_wr_enable_01),
	&PKTINFO(canvas_poc_exec),
	&DLYINFO(canvas_poc_wait_wr_enable_01),
	&PKTINFO(canvas_poc_wr_enable_02),
	&PKTINFO(canvas_poc_exec),
	&DLYINFO(canvas_poc_wait_wr_enable_02),
};

static void *canvas_poc_wr_dat_cmdtbl[] = {
	&PKTINFO(canvas_poc_wr_enable_01),
	&PKTINFO(canvas_poc_exec),
	&DLYINFO(canvas_poc_wait_wr_enable_01),
	&PKTINFO(canvas_poc_wr_stt_dat),
	&PKTINFO(canvas_poc_exec),
	&DLYINFO(canvas_poc_wait_wr_exec),
	&PKTINFO(canvas_poc_wr_end),
	&DLYINFO(canvas_poc_wait_wr_done),
};
static void *canvas_poc_wr_exit_cmdtbl[] = {
	&PKTINFO(canvas_poc_pgm_disable),
	&PKTINFO(canvas_poc_key_disable),
	&PKTINFO(canvas_level2_key_disable),
};

static void *canvas_poc_rd_enter_cmdtbl[] = {
	&PKTINFO(canvas_level2_key_enable),
	&PKTINFO(canvas_poc_key_enable),
	&PKTINFO(canvas_poc_pgm_enable),
	&PKTINFO(canvas_poc_rd_enable),
	&PKTINFO(canvas_poc_exec),
	&DLYINFO(canvas_poc_wait_exec),
	&PKTINFO(canvas_poc_qd_enable),
	&PKTINFO(canvas_poc_exec),
	&DLYINFO(canvas_poc_wait_qd_status),
};

static void *canvas_poc_rd_dat_cmdtbl[] = {
	&PKTINFO(canvas_poc_rd_stt),
	&PKTINFO(canvas_poc_exec),
	&DLYINFO(canvas_poc_wait_rd_done),
	&s6e3fab_restbl[RES_POC_DATA],
};

static void *canvas_poc_rd_exit_cmdtbl[] = {
	&PKTINFO(canvas_poc_pgm_disable),
	&PKTINFO(canvas_poc_pgm_enable),
	&PKTINFO(canvas_poc_wr_enable_01),
	&PKTINFO(canvas_poc_exec),
	&PKTINFO(canvas_poc_wr_exit),
	&PKTINFO(canvas_poc_exec),
	&PKTINFO(canvas_poc_pgm_disable),
	&PKTINFO(canvas_poc_key_disable),
	&PKTINFO(canvas_level2_key_disable),
};
#ifdef CONFIG_SUPPORT_POC_SPI
static void *canvas_poc_spi_init_cmdtbl[] = {
	&PKTINFO(canvas_poc_spi_write_enable),
	&PKTINFO(canvas_poc_spi_set_status1_init),
	&DLYINFO(canvas_poc_spi_wait_status),
	&PKTINFO(canvas_poc_spi_write_enable),
	&PKTINFO(canvas_poc_spi_set_status2_init),
	&DLYINFO(canvas_poc_spi_wait_status),
};

static void *canvas_poc_spi_exit_cmdtbl[] = {
	&PKTINFO(canvas_poc_spi_write_enable),
	&PKTINFO(canvas_poc_spi_set_status1_exit),
	&DLYINFO(canvas_poc_spi_wait_status),
	&PKTINFO(canvas_poc_spi_write_enable),
	&PKTINFO(canvas_poc_spi_set_status2_exit),
	&DLYINFO(canvas_poc_spi_wait_status),
};

static void *canvas_poc_spi_read_cmdtbl[] = {
	&PKTINFO(canvas_poc_spi_rd_addr),
	&s6e3fab_restbl[RES_POC_SPI_READ],
};

static void *canvas_poc_spi_erase_4k_cmdtbl[] = {
	&PKTINFO(canvas_poc_spi_write_enable),
	&PKTINFO(canvas_poc_spi_erase_4k),
};

static void *canvas_poc_spi_erase_32k_cmdtbl[] = {
	&PKTINFO(canvas_poc_spi_write_enable),
	&PKTINFO(canvas_poc_spi_erase_32k),
};

static void *canvas_poc_spi_erase_64k_cmdtbl[] = {
	&PKTINFO(canvas_poc_spi_write_enable),
	&PKTINFO(canvas_poc_spi_erase_64k),
};

static void *canvas_poc_spi_write_cmdtbl[] = {
	&PKTINFO(canvas_poc_spi_write_enable),
	&PKTINFO(canvas_poc_spi_write),
#ifndef PANEL_POC_SPI_BUSY_WAIT
	&DLYINFO(canvas_poc_spi_wait_write),
#endif
};

static void *canvas_poc_spi_status_cmdtbl[] = {
	&PKTINFO(canvas_poc_spi_status1),
	&s6e3fab_restbl[RES_POC_SPI_STATUS1],
	&PKTINFO(canvas_poc_spi_status2),
	&s6e3fab_restbl[RES_POC_SPI_STATUS2],
};

static void *canvas_poc_spi_wait_write_cmdtbl[] = {
	&DLYINFO(canvas_poc_spi_wait_write),
};

static void *canvas_poc_spi_wait_erase_cmdtbl[] = {
	&DLYINFO(canvas_poc_spi_wait_erase),
};
#endif

static struct seqinfo canvas_poc_seqtbl[MAX_POC_SEQ] = {
#ifdef CONFIG_SUPPORT_POC_FLASH
	/* poc erase */
	[POC_ERASE_ENTER_SEQ] = SEQINFO_INIT("poc-erase-enter-seq", canvas_poc_erase_enter_cmdtbl),
	[POC_ERASE_4K_SEQ] = SEQINFO_INIT("poc-erase-4k-seq", canvas_poc_erase_4k_cmdtbl),
	[POC_ERASE_32K_SEQ] = SEQINFO_INIT("poc-erase-32k-seq", canvas_poc_erase_32k_cmdtbl),
	[POC_ERASE_64K_SEQ] = SEQINFO_INIT("poc-erase-64k-seq", canvas_poc_erase_64k_cmdtbl),
	[POC_ERASE_EXIT_SEQ] = SEQINFO_INIT("poc-erase-exit-seq", canvas_poc_erase_exit_cmdtbl),
#endif

	/* poc write */
	[POC_WRITE_ENTER_SEQ] = SEQINFO_INIT("poc-wr-enter-seq", canvas_poc_wr_enter_cmdtbl),
	[POC_WRITE_DAT_SEQ] = SEQINFO_INIT("poc-wr-dat-seq", canvas_poc_wr_dat_cmdtbl),
	[POC_WRITE_EXIT_SEQ] = SEQINFO_INIT("poc-wr-exit-seq", canvas_poc_wr_exit_cmdtbl),

	/* poc read */
	[POC_READ_ENTER_SEQ] = SEQINFO_INIT("poc-rd-enter-seq", canvas_poc_rd_enter_cmdtbl),
	[POC_READ_DAT_SEQ] = SEQINFO_INIT("poc-rd-dat-seq", canvas_poc_rd_dat_cmdtbl),
	[POC_READ_EXIT_SEQ] = SEQINFO_INIT("poc-rd-exit-seq", canvas_poc_rd_exit_cmdtbl),
#ifdef CONFIG_SUPPORT_POC_SPI
	[POC_SPI_INIT_SEQ] = SEQINFO_INIT("poc-spi-init-seq", canvas_poc_spi_init_cmdtbl),
	[POC_SPI_EXIT_SEQ] = SEQINFO_INIT("poc-spi-exit-seq", canvas_poc_spi_exit_cmdtbl),
	[POC_SPI_READ_SEQ] = SEQINFO_INIT("poc-spi-read-seq", canvas_poc_spi_read_cmdtbl),
	[POC_SPI_ERASE_4K_SEQ] = SEQINFO_INIT("poc-spi-erase-4k-seq", canvas_poc_spi_erase_4k_cmdtbl),
	[POC_SPI_ERASE_32K_SEQ] = SEQINFO_INIT("poc-spi-erase-4k-seq", canvas_poc_spi_erase_32k_cmdtbl),
	[POC_SPI_ERASE_64K_SEQ] = SEQINFO_INIT("poc-spi-erase-32k-seq", canvas_poc_spi_erase_64k_cmdtbl),
	[POC_SPI_WRITE_SEQ] = SEQINFO_INIT("poc-spi-write-seq", canvas_poc_spi_write_cmdtbl),
	[POC_SPI_STATUS_SEQ] = SEQINFO_INIT("poc-spi-status-seq", canvas_poc_spi_status_cmdtbl),
	[POC_SPI_WAIT_WRITE_SEQ] = SEQINFO_INIT("poc-spi-wait-write-seq", canvas_poc_spi_wait_write_cmdtbl),
	[POC_SPI_WAIT_ERASE_SEQ] = SEQINFO_INIT("poc-spi-wait-erase-seq", canvas_poc_spi_wait_erase_cmdtbl),
#endif
};

/* partition consists of DATA, CHECKSUM and MAGICNUM */
static struct poc_partition canvas_poc_partition[] = {
	[POC_IMG_PARTITION] = {
		.name = "image",
		.addr = CANVAS_POC_IMG_ADDR,
		.size = CANVAS_POC_IMG_SIZE,
		.need_preload = false
	},
#ifdef CONFIG_SUPPORT_DIM_FLASH
	[POC_DIM_PARTITION] = {
		.name = "dimming",
		.addr = CANVAS_POC_DIM_DATA_ADDR,
		.size = CANVAS_POC_DIM_TOTAL_SIZE,
		.data_addr = CANVAS_POC_DIM_DATA_ADDR,
		.data_size = CANVAS_POC_DIM_DATA_SIZE,
		.checksum_addr = CANVAS_POC_DIM_CHECKSUM_ADDR,
		.checksum_size = CANVAS_POC_DIM_CHECKSUM_SIZE,
		.magicnum_addr = CANVAS_POC_DIM_MAGICNUM_ADDR,
		.magicnum_size = CANVAS_POC_DIM_MAGICNUM_SIZE,
		.need_preload = false,
		.magicnum = 1,
	},
	[POC_MTP_PARTITION] = {
		.name = "mtp",
		.addr = CANVAS_POC_MTP_DATA_ADDR,
		.size = CANVAS_POC_MTP_TOTAL_SIZE,
		.data_addr = CANVAS_POC_MTP_DATA_ADDR,
		.data_size = CANVAS_POC_MTP_DATA_SIZE,
		.checksum_addr = CANVAS_POC_MTP_CHECKSUM_ADDR,
		.checksum_size = CANVAS_POC_MTP_CHECKSUM_SIZE,
		.magicnum_addr = 0,
		.magicnum_size = 0,
		.need_preload = false,
	},
	[POC_DIM_PARTITION_1] = {
		.name = "dimming",
		.addr = CANVAS_POC_DIM_120HZ_DATA_ADDR,
		.size = CANVAS_POC_DIM_120HZ_TOTAL_SIZE,
		.data_addr = CANVAS_POC_DIM_120HZ_DATA_ADDR,
		.data_size = CANVAS_POC_DIM_120HZ_DATA_SIZE,
		.checksum_addr = CANVAS_POC_DIM_120HZ_CHECKSUM_ADDR,
		.checksum_size = CANVAS_POC_DIM_120HZ_CHECKSUM_SIZE,
		.magicnum_addr = CANVAS_POC_DIM_120HZ_MAGICNUM_ADDR,
		.magicnum_size = CANVAS_POC_DIM_120HZ_MAGICNUM_SIZE,
		.need_preload = false,
		.magicnum = 1,
	},
	[POC_MTP_PARTITION_1] = {
		.name = "mtp",
		.addr = CANVAS_POC_MTP_120HZ_DATA_ADDR,
		.size = CANVAS_POC_MTP_120HZ_TOTAL_SIZE,
		.data_addr = CANVAS_POC_MTP_120HZ_DATA_ADDR,
		.data_size = CANVAS_POC_MTP_120HZ_DATA_SIZE,
		.checksum_addr = CANVAS_POC_MTP_120HZ_CHECKSUM_ADDR,
		.checksum_size = CANVAS_POC_MTP_120HZ_CHECKSUM_SIZE,
		.magicnum_addr = 0,
		.magicnum_size = 0,
		.need_preload = false,
	},
	[POC_DIM_PARTITION_2] = {
		.name = "dimming",
		.addr = CANVAS_POC_DIM_60HZ_HS_DATA_ADDR,
		.size = CANVAS_POC_DIM_60HZ_HS_TOTAL_SIZE,
		.data_addr = CANVAS_POC_DIM_60HZ_HS_DATA_ADDR,
		.data_size = CANVAS_POC_DIM_60HZ_HS_DATA_SIZE,
		.checksum_addr = CANVAS_POC_DIM_60HZ_HS_CHECKSUM_ADDR,
		.checksum_size = CANVAS_POC_DIM_60HZ_HS_CHECKSUM_SIZE,
		.magicnum_addr = CANVAS_POC_DIM_60HZ_HS_MAGICNUM_ADDR,
		.magicnum_size = CANVAS_POC_DIM_60HZ_HS_MAGICNUM_SIZE,
		.need_preload = false,
		.magicnum = 1,
	},
	[POC_MTP_PARTITION_2] = {
		.name = "mtp",
		.addr = CANVAS_POC_MTP_60HZ_HS_DATA_ADDR,
		.size = CANVAS_POC_MTP_60HZ_HS_TOTAL_SIZE,
		.data_addr = CANVAS_POC_MTP_60HZ_HS_DATA_ADDR,
		.data_size = CANVAS_POC_MTP_60HZ_HS_DATA_SIZE,
		.checksum_addr = CANVAS_POC_MTP_60HZ_HS_CHECKSUM_ADDR,
		.checksum_size = CANVAS_POC_MTP_60HZ_HS_CHECKSUM_SIZE,
		.magicnum_addr = 0,
		.magicnum_size = 0,
		.need_preload = false,
	},
	[POC_MCD_PARTITION] = {
		.name = "mcd",
		.addr = CANVAS_POC_MCD_ADDR,
		.size = CANVAS_POC_MCD_SIZE,
		.need_preload = false
	},
#endif
};

static struct panel_poc_data s6e3fab_canvas_poc_data = {
	.version = 2,
	.seqtbl = canvas_poc_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(canvas_poc_seqtbl),
	.maptbl = canvas_poc_maptbl,
	.nr_maptbl = ARRAY_SIZE(canvas_poc_maptbl),
	.partition = canvas_poc_partition,
	.nr_partition = ARRAY_SIZE(canvas_poc_partition),
	.wdata_len = 256,
#ifdef CONFIG_SUPPORT_POC_SPI
	.spi_wdata_len = 256,
	.conn_src = POC_CONN_SRC_SPI,
	.state_mask = 0x025C,
	.state_init = 0x0000,
	.state_uninit = 0x025C,
	.busy_mask = 0x0001,
#endif
};
#endif /* __S6E3FAB_CANVAS_PANEL_POC_H__ */
