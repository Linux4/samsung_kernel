/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fac/s6e3fac_rainbow_panel_poc.h
 *
 * Header file for POC Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FAC_RAINBOW_PANEL_POC_H__
#define __S6E3FAC_RAINBOW_PANEL_POC_H__

#include "../panel.h"
#include "../panel_poc.h"

#define RAINBOW_GM2_GAMMA_ADDR	(0x78000)
#define RAINBOW_GM2_GAMMA_SIZE	(0x40D)
#define RAINBOW_GM2_GAMMA_DATA_ADDR	(RAINBOW_GM2_GAMMA_ADDR)
#define RAINBOW_GM2_GAMMA_DATA_SIZE	(0x2B0)


#define RAINBOW_GM2_GAMMA_CHECKSUM_ADDR	(RAINBOW_GM2_GAMMA_ADDR + RAINBOW_GM2_GAMMA_DATA_SIZE)
#define RAINBOW_GM2_GAMMA_CHECKSUM_SIZE	(2)
#define RAINBOW_GM2_GAMMA_MAGICNUM_ADDR	(0x7840C)
#define RAINBOW_GM2_GAMMA_MAGICNUM_SIZE	(1)

#ifdef CONFIG_SUPPORT_POC_SPI
#define RAINBOW_SPI_ER_DONE_MDELAY		(35)
#define RAINBOW_SPI_STATUS_WR_DONE_MDELAY		(15)

#ifdef PANEL_POC_SPI_BUSY_WAIT
#define RAINBOW_SPI_WR_DONE_UDELAY		(50)
#else
#define RAINBOW_SPI_WR_DONE_UDELAY		(400)
#endif
#endif

/* ===================================================================================== */
/* ============================== [S6E3FAC MAPPING TABLE] ============================== */
/* ===================================================================================== */
static struct maptbl rainbow_poc_maptbl[] = {
#if 0
#ifdef CONFIG_SUPPORT_POC_SPI
	[POC_SPI_READ_ADDR_MAPTBL] = DEFINE_0D_MAPTBL(rainbow_poc_spi_read_table, init_common_table, NULL, copy_poc_rd_addr_maptbl),
	[POC_SPI_WRITE_ADDR_MAPTBL] = DEFINE_0D_MAPTBL(rainbow_poc_spi_write_addr_table, init_common_table, NULL, copy_poc_wr_addr_maptbl),
	[POC_SPI_WRITE_DATA_MAPTBL] = DEFINE_0D_MAPTBL(rainbow_poc_spi_write_data_table, init_common_table, NULL, copy_poc_wr_data_maptbl),
	[POC_SPI_ERASE_ADDR_MAPTBL] = DEFINE_0D_MAPTBL(rainbow_poc_spi_erase_addr_table, init_common_table, NULL, copy_poc_er_addr_maptbl),
#endif
#endif
};

#ifdef CONFIG_SUPPORT_POC_SPI
static u8 RAINBOW_POC_SPI_SET_STATUS1_INIT[] = { 0x01, 0x00 };
static u8 RAINBOW_POC_SPI_SET_STATUS2_INIT[] = { 0x31, 0x00 };
static u8 RAINBOW_POC_SPI_SET_STATUS1_EXIT[] = { 0x01, 0x5C };
static u8 RAINBOW_POC_SPI_SET_STATUS2_EXIT[] = { 0x31, 0x02 };
static DEFINE_STATIC_PACKET(rainbow_poc_spi_set_status1_init, SPI_PKT_TYPE_WR, RAINBOW_POC_SPI_SET_STATUS1_INIT, 0);
static DEFINE_STATIC_PACKET(rainbow_poc_spi_set_status2_init, SPI_PKT_TYPE_WR, RAINBOW_POC_SPI_SET_STATUS2_INIT, 0);
static DEFINE_STATIC_PACKET(rainbow_poc_spi_set_status1_exit, SPI_PKT_TYPE_WR, RAINBOW_POC_SPI_SET_STATUS1_EXIT, 0);
static DEFINE_STATIC_PACKET(rainbow_poc_spi_set_status2_exit, SPI_PKT_TYPE_WR, RAINBOW_POC_SPI_SET_STATUS2_EXIT, 0);

static u8 RAINBOW_POC_SPI_STATUS1[] = { 0x05 };
static u8 RAINBOW_POC_SPI_STATUS2[] = { 0x35 };
static u8 RAINBOW_POC_SPI_READ[] = { 0x03, 0x00, 0x00, 0x00 };
static u8 RAINBOW_POC_SPI_ERASE_4K[] = { 0x20, 0x00, 0x00, 0x00 };
static u8 RAINBOW_POC_SPI_ERASE_32K[] = { 0x52, 0x00, 0x00, 0x00 };
static u8 RAINBOW_POC_SPI_ERASE_64K[] = { 0xD8, 0x00, 0x00, 0x00 };
static u8 RAINBOW_POC_SPI_WRITE_ENABLE[] = { 0x06 };
static u8 RAINBOW_POC_SPI_WRITE[260] = { 0x02, 0x00, 0x00, 0x00, };
static DEFINE_STATIC_PACKET(rainbow_poc_spi_status1, SPI_PKT_TYPE_SETPARAM, RAINBOW_POC_SPI_STATUS1, 0);
static DEFINE_STATIC_PACKET(rainbow_poc_spi_status2, SPI_PKT_TYPE_SETPARAM, RAINBOW_POC_SPI_STATUS2, 0);
static DEFINE_STATIC_PACKET(rainbow_poc_spi_write_enable, SPI_PKT_TYPE_WR, RAINBOW_POC_SPI_WRITE_ENABLE, 0);
static DEFINE_PKTUI(rainbow_poc_spi_erase_4k, &rainbow_poc_maptbl[POC_SPI_ERASE_ADDR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(rainbow_poc_spi_erase_4k, SPI_PKT_TYPE_WR, RAINBOW_POC_SPI_ERASE_4K, 0);
static DEFINE_PKTUI(rainbow_poc_spi_erase_32k, &rainbow_poc_maptbl[POC_SPI_ERASE_ADDR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(rainbow_poc_spi_erase_32k, SPI_PKT_TYPE_WR, RAINBOW_POC_SPI_ERASE_32K, 0);
static DEFINE_PKTUI(rainbow_poc_spi_erase_64k, &rainbow_poc_maptbl[POC_SPI_ERASE_ADDR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(rainbow_poc_spi_erase_64k, SPI_PKT_TYPE_WR, RAINBOW_POC_SPI_ERASE_64K, 0);

static DECLARE_PKTUI(rainbow_poc_spi_write) = {
	{ .offset = 1, .maptbl = &rainbow_poc_maptbl[POC_SPI_WRITE_ADDR_MAPTBL] },
	{ .offset = 4, .maptbl = &rainbow_poc_maptbl[POC_SPI_WRITE_DATA_MAPTBL] },
};
static DEFINE_VARIABLE_PACKET(rainbow_poc_spi_write, SPI_PKT_TYPE_WR, RAINBOW_POC_SPI_WRITE, 0);
static DEFINE_PKTUI(rainbow_poc_spi_rd_addr, &rainbow_poc_maptbl[POC_SPI_READ_ADDR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(rainbow_poc_spi_rd_addr, SPI_PKT_TYPE_SETPARAM, RAINBOW_POC_SPI_READ, 0);
static DEFINE_PANEL_UDELAY_NO_SLEEP(rainbow_poc_spi_wait_write, RAINBOW_SPI_WR_DONE_UDELAY);
static DEFINE_PANEL_MDELAY(rainbow_poc_spi_wait_erase, RAINBOW_SPI_ER_DONE_MDELAY);
static DEFINE_PANEL_MDELAY(rainbow_poc_spi_wait_status, RAINBOW_SPI_STATUS_WR_DONE_MDELAY);
#endif

#ifdef CONFIG_SUPPORT_POC_SPI
static void *rainbow_poc_spi_init_cmdtbl[] = {
	&PKTINFO(rainbow_poc_spi_write_enable),
	&PKTINFO(rainbow_poc_spi_set_status1_init),
	&DLYINFO(rainbow_poc_spi_wait_status),
	&PKTINFO(rainbow_poc_spi_write_enable),
	&PKTINFO(rainbow_poc_spi_set_status2_init),
	&DLYINFO(rainbow_poc_spi_wait_status),
};

static void *rainbow_poc_spi_exit_cmdtbl[] = {
	&PKTINFO(rainbow_poc_spi_write_enable),
	&PKTINFO(rainbow_poc_spi_set_status1_exit),
	&DLYINFO(rainbow_poc_spi_wait_status),
	&PKTINFO(rainbow_poc_spi_write_enable),
	&PKTINFO(rainbow_poc_spi_set_status2_exit),
	&DLYINFO(rainbow_poc_spi_wait_status),
};

static void *rainbow_poc_spi_read_cmdtbl[] = {
	&PKTINFO(rainbow_poc_spi_rd_addr),
	&s6e3fac_restbl[RES_POC_SPI_READ],
};

static void *rainbow_poc_spi_erase_4k_cmdtbl[] = {
	&PKTINFO(rainbow_poc_spi_write_enable),
	&PKTINFO(rainbow_poc_spi_erase_4k),
};

static void *rainbow_poc_spi_erase_32k_cmdtbl[] = {
	&PKTINFO(rainbow_poc_spi_write_enable),
	&PKTINFO(rainbow_poc_spi_erase_32k),
};

static void *rainbow_poc_spi_erase_64k_cmdtbl[] = {
	&PKTINFO(rainbow_poc_spi_write_enable),
	&PKTINFO(rainbow_poc_spi_erase_64k),
};

static void *rainbow_poc_spi_write_cmdtbl[] = {
	&PKTINFO(rainbow_poc_spi_write_enable),
	&PKTINFO(rainbow_poc_spi_write),
#ifndef PANEL_POC_SPI_BUSY_WAIT
	&DLYINFO(rainbow_poc_spi_wait_write),
#endif
};

static void *rainbow_poc_spi_status_cmdtbl[] = {
	&PKTINFO(rainbow_poc_spi_status1),
	&s6e3fac_restbl[RES_POC_SPI_STATUS1],
	&PKTINFO(rainbow_poc_spi_status2),
	&s6e3fac_restbl[RES_POC_SPI_STATUS2],
};

static void *rainbow_poc_spi_wait_write_cmdtbl[] = {
	&DLYINFO(rainbow_poc_spi_wait_write),
};

static void *rainbow_poc_spi_wait_erase_cmdtbl[] = {
	&DLYINFO(rainbow_poc_spi_wait_erase),
};
#endif

static struct seqinfo rainbow_poc_seqtbl[MAX_POC_SEQ] = {
#ifdef CONFIG_SUPPORT_POC_SPI
	[POC_SPI_INIT_SEQ] = SEQINFO_INIT("poc-spi-init-seq", rainbow_poc_spi_init_cmdtbl),
	[POC_SPI_EXIT_SEQ] = SEQINFO_INIT("poc-spi-exit-seq", rainbow_poc_spi_exit_cmdtbl),
	[POC_SPI_READ_SEQ] = SEQINFO_INIT("poc-spi-read-seq", rainbow_poc_spi_read_cmdtbl),
	[POC_SPI_ERASE_4K_SEQ] = SEQINFO_INIT("poc-spi-erase-4k-seq", rainbow_poc_spi_erase_4k_cmdtbl),
	[POC_SPI_ERASE_32K_SEQ] = SEQINFO_INIT("poc-spi-erase-4k-seq", rainbow_poc_spi_erase_32k_cmdtbl),
	[POC_SPI_ERASE_64K_SEQ] = SEQINFO_INIT("poc-spi-erase-32k-seq", rainbow_poc_spi_erase_64k_cmdtbl),
	[POC_SPI_WRITE_SEQ] = SEQINFO_INIT("poc-spi-write-seq", rainbow_poc_spi_write_cmdtbl),
	[POC_SPI_STATUS_SEQ] = SEQINFO_INIT("poc-spi-status-seq", rainbow_poc_spi_status_cmdtbl),
	[POC_SPI_WAIT_WRITE_SEQ] = SEQINFO_INIT("poc-spi-wait-write-seq", rainbow_poc_spi_wait_write_cmdtbl),
	[POC_SPI_WAIT_ERASE_SEQ] = SEQINFO_INIT("poc-spi-wait-erase-seq", rainbow_poc_spi_wait_erase_cmdtbl),
#endif
};

/* partition consists of DATA, CHECKSUM and MAGICNUM */
static struct poc_partition rainbow_poc_partition[] = {
	[POC_GM2_GAMMA_PARTITION] = {
		.name = "gm2_gamma",
		.addr = RAINBOW_GM2_GAMMA_ADDR,
		.size = RAINBOW_GM2_GAMMA_SIZE,
		.data = {
			{
				.data_addr = RAINBOW_GM2_GAMMA_DATA_ADDR,
				.data_size = RAINBOW_GM2_GAMMA_DATA_SIZE,
			},
		},
		.checksum_addr = RAINBOW_GM2_GAMMA_CHECKSUM_ADDR,
		.checksum_size = RAINBOW_GM2_GAMMA_CHECKSUM_SIZE,
		.magicnum_addr = RAINBOW_GM2_GAMMA_MAGICNUM_ADDR,
		.magicnum_size = RAINBOW_GM2_GAMMA_MAGICNUM_SIZE,
		.magicnum = 0x01,
		.need_preload = true
	},
};

static struct panel_poc_data s6e3fac_rainbow_poc_data = {
	.version = 3,
	.seqtbl = rainbow_poc_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(rainbow_poc_seqtbl),
	.maptbl = rainbow_poc_maptbl,
	.nr_maptbl = ARRAY_SIZE(rainbow_poc_maptbl),
	.partition = rainbow_poc_partition,
	.nr_partition = ARRAY_SIZE(rainbow_poc_partition),
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
#endif /* __S6E3FAC_RAINBOW_PANEL_POC_H__ */
