/*
 * Copyright (c) 2015, SPREADTRUM Corporation.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * haidian, Beijing, CHINA, 3/20/2015.
 */

#ifndef __CRYPTODEV_SPRD_AES_H
#define __CRYPTODEV_SPRD_AES_H

/* config register address offset from base address. */
#define SPRD_AES_DMA_STATUS		0x000
#define SPRD_AES_STATUS			0x004
#define SPRD_AES_CLK_EN			0x018
#define SPRD_AES_INT_EN			0x01c
#define SPRD_AES_INT_STATUS		0x020
#define SPRD_AES_INT_CLEAR		0x024
#define SPRD_AES_START			0x028
#define SPRD_AES_CLEAR			0x02c
#define SPRD_AES_MODE			0x030
#define SPRD_AES_CFG			0x044
#define SPRD_AES_LIST_LEN		0x058
#define SPRD_AES_LIST_PTR		0x05c
#define SPRD_AES_KEY_ADDR_LEN		0x068
#define SPRD_AES_KEY_ADDR		0x06c

/* the common value of configure register. */
#define CMD_DISABLE			0x00
#define CMD_MAIN_CLK_EN			0x01
#define CMD_AES_CLK_EN			0x02
#define CMD_AES_LIST_DONE_INT		0x04
#define CMD_AES_LIST_END_INT		0x08
#define CMD_AES_EN			0x01
#define CMD_AES_ENC_DEC_SEL		0x10
#define CMD_AES_ECB_MODE		0x00
#define CMD_AES_CBC_MODE		0x01
#define CMD_AES_CTR_MODE		0x02
#define CMD_AES_XTS_MODE		0x03
#define CMD_AES_CE_START		0x01
#define CMD_AES_CE_BUSY			0x10
#define CMD_AES_KEY128			0x00
#define CMD_AES_KEY192			0x10
#define CMD_AES_KEY256			0x30
#define CMD_AES_LINK_MODE		0x01
#define CMD_AES_KEY_IN_DDR		0x01
#define CMD_AES_GEN_INT_EACH		0x20
#define CMD_AES_RESET_CE		0x01
#define CMD_AES_UPD_IV_SEC_CNT		0x04
#define CMD_AES_LIST_END		0x02
#define CMD_AES_LIST_END_FLAG		0x01
#define CMD_AES_LEN_ERR_INT		0x10
#define CMD_AES_WAIT_BDONE		0x10
#define CMD_AES_DST_BYTE_SWITCH		0x10
#define CMD_AES_SRC_BYTE_SWITCH		0x20
#define CMD_AES_AXI_CLK			0x01

/* assistant mask and cmd */
#define NODE_DATA_LEN_MASK	0x00ffffff
#define NEXT_LIST_LEN_MASK	0x0000ffff
#define AES_ERR_INT_MASK	0x0000001f

/*AES process control flags*/
#define FLAGS_MODE_MASK			0x03f
#define FLAGS_OPS_MODE_MASK		0x03c
#define FLAGS_ENCRYPT			BIT(0)
#define FLAGS_DECRYPT			BIT(1)
#define FLAGS_ECB			BIT(2)
#define FLAGS_CBC			BIT(3)
#define FLAGS_CTR			BIT(4)
#define FLAGS_XTS			BIT(5)
#define FLAGS_NEW_KEY			BIT(6)
#define FLAGS_BUSY			BIT(7)
#define FLAGS_FIRST_LIST		BIT(8)

/* command bit shifts */
enum {
	CMD_BIT8_SHIFT = 8,
	CMD_BIT16_SHIFT = 16,
	CMD_BIT24_SHIFT = 24,
	CMD_BIT32_SHIFT = 32,
};

#endif
