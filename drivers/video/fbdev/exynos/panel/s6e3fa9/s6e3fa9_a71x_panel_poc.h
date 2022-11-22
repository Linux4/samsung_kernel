/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fa9/s6e3fa9_a71x_panel_poc.h
 *
 * Header file for POC Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FA9_A71X_PANEL_POC_H__
#define __S6E3FA9_A71X_PANEL_POC_H__

#include "../panel.h"
#include "../panel_poc.h"

#define A71X_POC_IMG_ADDR	(0)
#define A71X_POC_IMG_SIZE	(327246)

#define A71x_POC_WRITE_MAX_SIZE	(256)
/* ===================================================================================== */
/* ============================== [S6E3FA9 MAPPING TABLE] ============================== */
/* ===================================================================================== */
static struct maptbl a71x_poc_maptbl[] = {
	[POC_WR_ADDR_MAPTBL] = DEFINE_0D_MAPTBL(a71x_poc_wr_addr_table, init_common_table, NULL, copy_poc_wr_addr_maptbl),
	[POC_RD_ADDR_MAPTBL] = DEFINE_0D_MAPTBL(a71x_poc_rd_addr_table, init_common_table, NULL, copy_poc_rd_addr_maptbl),
	[POC_WR_DATA_MAPTBL] = DEFINE_0D_MAPTBL(a71x_poc_wr_data_table, init_common_table, NULL, copy_poc_wr_data_maptbl),
	[POC_ER_ADDR_MAPTBL] = DEFINE_0D_MAPTBL(a71x_poc_er_addr_table, init_common_table, NULL, copy_poc_er_addr_maptbl),
};

/* ===================================================================================== */
/* ============================== [S6E3FA9 COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 A71X_POC_KEY2_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static u8 A71X_POC_KEY2_DISABLE[] = { 0xF0, 0xA5, 0xA5 };
static u8 A71X_POC_KEY_ENABLE[] = { 0xF1, 0xF1, 0xA2 };
static u8 A71X_POC_KEY_DISABLE[] = { 0xF1, 0xA5, 0xA5 };
static u8 A71X_POC_PGM_ENABLE[] = { 0xC0, 0x02 };
static u8 A71X_POC_PGM_DISABLE[] = { 0xC0, 0x00 };
static u8 A71X_POC_FLASH_CONTROL_1[] = { 0xC1, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static u8 A71X_POC_FLASH_CONTROL_2[] = { 0xC1, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x80, 0x00, 0x00 };
static u8 A71X_POC_ACCESS_ENABLE[] = { 0x71, 0x03, 0x00, 0x00 };

static u8 A71X_POC_EXEC[] = { 0xC0, 0x03 };

static u8 A71X_POC_RD_STT[] = { 0xC1, 0x00, 0x00, 0x6B, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x80};

static u8 A71X_POC_WR_STT[] = { 0xC1, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0xC2, 0x00 };
static u8 A71X_POC_WR_DAT[A71x_POC_WRITE_MAX_SIZE + 1] = { 0x6C, 0x00 };

static u8 A71X_POC_ER_4K_DAT[] = { 0xC1, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00,};
static u8 A71X_POC_ER_32K_DAT[] = { 0xC1, 0x00, 0x00, 0x52, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00,};
static u8 A71X_POC_ER_64K_DAT[] = { 0xC1, 0x00, 0x00, 0xD8, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00,};

/*********** COMMON ***********/
static DEFINE_STATIC_PACKET(a71x_poc_level2_key_enable, DSI_PKT_TYPE_WR, A71X_POC_KEY2_ENABLE, 0);
static DEFINE_STATIC_PACKET(a71x_poc_level2_key_disable, DSI_PKT_TYPE_WR, A71X_POC_KEY2_DISABLE, 0);
static DEFINE_STATIC_PACKET(a71x_poc_key_enable, DSI_PKT_TYPE_WR, A71X_POC_KEY_ENABLE, 0);
static DEFINE_STATIC_PACKET(a71x_poc_key_disable, DSI_PKT_TYPE_WR, A71X_POC_KEY_DISABLE, 0);
static DEFINE_STATIC_PACKET(a71x_poc_pgm_enable, DSI_PKT_TYPE_WR, A71X_POC_PGM_ENABLE, 0);
static DEFINE_STATIC_PACKET(a71x_poc_pgm_disable, DSI_PKT_TYPE_WR, A71X_POC_PGM_DISABLE, 0);
static DEFINE_STATIC_PACKET(a71x_poc_flash_confrol_1, DSI_PKT_TYPE_WR, A71X_POC_FLASH_CONTROL_1, 0);
static DEFINE_STATIC_PACKET(a71x_poc_flash_confrol_2, DSI_PKT_TYPE_WR, A71X_POC_FLASH_CONTROL_2, 0);
static DEFINE_STATIC_PACKET(a71x_poc_access_enable, DSI_PKT_TYPE_WR, A71X_POC_ACCESS_ENABLE, 0);
static DEFINE_STATIC_PACKET(a71x_poc_exec, DSI_PKT_TYPE_WR, A71X_POC_EXEC, 0);

/************ READ *************/
static DEFINE_PKTUI(a71x_poc_rd_stt, &a71x_poc_maptbl[POC_RD_ADDR_MAPTBL], 4);
static DEFINE_VARIABLE_PACKET(a71x_poc_rd_stt, DSI_PKT_TYPE_WR, A71X_POC_RD_STT, 0);


/************ WRITE *************/
static DEFINE_PKTUI(a71x_poc_wr_stt, &a71x_poc_maptbl[POC_WR_ADDR_MAPTBL], 4);
static DEFINE_VARIABLE_PACKET(a71x_poc_wr_stt, DSI_PKT_TYPE_WR, A71X_POC_WR_STT, 0);
static DEFINE_PKTUI(a71x_poc_wr_dat, &a71x_poc_maptbl[POC_WR_DATA_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a71x_poc_wr_dat, DSI_PKT_TYPE_WR, A71X_POC_WR_DAT, 0);


/************ ERASE *************/
static DEFINE_PKTUI(a71x_poc_er_4k_dat, &a71x_poc_maptbl[POC_ER_ADDR_MAPTBL], 4);
static DEFINE_VARIABLE_PACKET(a71x_poc_er_4k_dat, DSI_PKT_TYPE_WR, A71X_POC_ER_4K_DAT, 0);
static DEFINE_PKTUI(a71x_poc_er_32k_dat, &a71x_poc_maptbl[POC_ER_ADDR_MAPTBL], 4);
static DEFINE_VARIABLE_PACKET(a71x_poc_er_32k_dat, DSI_PKT_TYPE_WR, A71X_POC_ER_32K_DAT, 0);
static DEFINE_PKTUI(a71x_poc_er_64k_dat, &a71x_poc_maptbl[POC_ER_ADDR_MAPTBL], 4);
static DEFINE_VARIABLE_PACKET(a71x_poc_er_64k_dat, DSI_PKT_TYPE_WR, A71X_POC_ER_64K_DAT, 0);

static DEFINE_PANEL_MDELAY(a71x_poc_wait_1ms, 1);
static DEFINE_PANEL_MDELAY(a71x_poc_wait_4ms, 4);
static DEFINE_PANEL_MDELAY(a71x_poc_wait_10ms, 10);
static DEFINE_PANEL_MDELAY(a71x_poc_wait_400ms, 400);
static DEFINE_PANEL_MDELAY(a71x_poc_wait_1600ms, 1600);
static DEFINE_PANEL_MDELAY(a71x_poc_wait_2000ms, 2000);

#ifdef CONFIG_SUPPORT_POC_FLASH
static void *a71x_poc_common_enter_cmdtbl[] = {
	&PKTINFO(a71x_poc_level2_key_enable),
	&PKTINFO(a71x_poc_key_enable),
	&PKTINFO(a71x_poc_pgm_enable),
	&PKTINFO(a71x_poc_access_enable),
	&DLYINFO(a71x_poc_wait_1ms),
	&PKTINFO(a71x_poc_flash_confrol_1),
	&PKTINFO(a71x_poc_exec),
	&DLYINFO(a71x_poc_wait_1ms),
	&PKTINFO(a71x_poc_flash_confrol_2),
	&PKTINFO(a71x_poc_exec),
	&DLYINFO(a71x_poc_wait_10ms),
};

static void *a71x_poc_wr_dat_cmdtbl[] = {
	&PKTINFO(a71x_poc_wr_dat),
	&PKTINFO(a71x_poc_flash_confrol_1),
	&PKTINFO(a71x_poc_exec),
	&DLYINFO(a71x_poc_wait_1ms),
	&PKTINFO(a71x_poc_wr_stt),
	&PKTINFO(a71x_poc_exec),
	&DLYINFO(a71x_poc_wait_4ms),
};

static void *a71x_poc_rd_dat_cmdtbl[] = {
	&PKTINFO(a71x_poc_rd_stt),
	&PKTINFO(a71x_poc_exec),
	&DLYINFO(a71x_poc_wait_10ms),
	&s6e3fa9_restbl[RES_POC_DATA],
};

static void *a71x_poc_erase_4k_cmdtbl[] = {
	&PKTINFO(a71x_poc_flash_confrol_1),
	&PKTINFO(a71x_poc_exec),
	&DLYINFO(a71x_poc_wait_1ms),
	&PKTINFO(a71x_poc_er_4k_dat),
	&PKTINFO(a71x_poc_exec),
	&DLYINFO(a71x_poc_wait_400ms),
};

static void *a71x_poc_erase_32k_cmdtbl[] = {
	&PKTINFO(a71x_poc_flash_confrol_1),
	&PKTINFO(a71x_poc_exec),
	&DLYINFO(a71x_poc_wait_1ms),
	&PKTINFO(a71x_poc_er_32k_dat),
	&PKTINFO(a71x_poc_exec),
	&DLYINFO(a71x_poc_wait_1600ms),
};

static void *a71x_poc_erase_64k_cmdtbl[] = {
	&PKTINFO(a71x_poc_flash_confrol_1),
	&PKTINFO(a71x_poc_exec),
	&DLYINFO(a71x_poc_wait_1ms),
	&PKTINFO(a71x_poc_er_64k_dat),
	&PKTINFO(a71x_poc_exec),
	&DLYINFO(a71x_poc_wait_2000ms),
};

static void *a71x_poc_common_exit_cmdtbl[] = {
	&PKTINFO(a71x_poc_pgm_disable),
	&PKTINFO(a71x_poc_level2_key_disable),
	&PKTINFO(a71x_poc_key_disable),
};
#endif

static struct seqinfo a71x_poc_seqtbl[MAX_POC_SEQ] = {
#ifdef CONFIG_SUPPORT_POC_FLASH
	/* poc erase */
	[POC_ERASE_ENTER_SEQ] = SEQINFO_INIT("poc-erase-enter-seq", a71x_poc_common_enter_cmdtbl),
	[POC_ERASE_4K_SEQ] = SEQINFO_INIT("poc-erase-4k-seq", a71x_poc_erase_4k_cmdtbl),
	[POC_ERASE_32K_SEQ] = SEQINFO_INIT("poc-erase-32k-seq", a71x_poc_erase_32k_cmdtbl),
	[POC_ERASE_64K_SEQ] = SEQINFO_INIT("poc-erase-64k-seq", a71x_poc_erase_64k_cmdtbl),
	[POC_ERASE_EXIT_SEQ] = SEQINFO_INIT("poc-erase-exit-seq", a71x_poc_common_exit_cmdtbl),
	/* poc write */
	[POC_WRITE_ENTER_SEQ] = SEQINFO_INIT("poc-wr-enter-seq", a71x_poc_common_enter_cmdtbl),
	[POC_WRITE_DAT_SEQ] = SEQINFO_INIT("poc-wr-dat-seq", a71x_poc_wr_dat_cmdtbl),
	[POC_WRITE_EXIT_SEQ] = SEQINFO_INIT("poc-wr-exit-seq", a71x_poc_common_exit_cmdtbl),

	/* poc read */
	[POC_READ_ENTER_SEQ] = SEQINFO_INIT("poc-rd-enter-seq", a71x_poc_common_enter_cmdtbl),
	[POC_READ_DAT_SEQ] = SEQINFO_INIT("poc-rd-dat-seq", a71x_poc_rd_dat_cmdtbl),
	[POC_READ_EXIT_SEQ] = SEQINFO_INIT("poc-rd-exit-seq", a71x_poc_common_exit_cmdtbl),
#endif
};

/* partition consists of DATA, CHECKSUM and MAGICNUM */
static struct poc_partition a71x_poc_partition[] = {
	[POC_IMG_PARTITION] = {
		.name = "image",
		.addr = A71X_POC_IMG_ADDR,
		.size = A71X_POC_IMG_SIZE,
		.need_preload = false
	},
};

static struct panel_poc_data s6e3fa9_a71x_poc_data = {
	.version = 2,
	.seqtbl = a71x_poc_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(a71x_poc_seqtbl),
	.maptbl = a71x_poc_maptbl,
	.nr_maptbl = ARRAY_SIZE(a71x_poc_maptbl),
	.partition = a71x_poc_partition,
	.nr_partition = ARRAY_SIZE(a71x_poc_partition),
	.wdata_len = 256,
	.rdata_len = 128, /*Neus rx fifo size is 256byte (read max size:252 without pkt header, ecc,...)*/
};
#endif /* __S6E3FA9_A71X_PANEL_POC_H__ */
