/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Filename : npi.h
 * Abstract : This file is a general definition for npi cmd
 *
 * Authors	:
 * Wenjie.Zhang <Wenjie.Zhang@spreadtrum.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ITM_NPI_CMD_H__
#define __ITM_NPI_CMD_H__

#define NL_GENERAL_NPI_ID 1022

enum SIPC_NPI_CMD_ID {
	NPI_CMD_MAC = 0,
	NPI_CMD_TX_MODE,
	NPI_CMD_TX_START,
	NPI_CMD_TX_STOP,
	NPI_CMD_RX_START,
	NPI_CMD_RX_STOP,
	NPI_CMD_RATE,
	NPI_CMD_CHANNEL,
	NPI_CMD_TX_POWER,
	NPI_CMD_RX_RIGHT,
	NPI_CMD_RX_ERROR,
	NPI_CMD_RX_COUNT,
	NPI_CMD_RSSI,
	NPI_CMD_TX_COUNT,
	NPI_CMD_REG,
	NPI_CMD_DEBUG,
	NPI_CMD_GET_SBLOCK,
	NPI_CMD_SIN_WAVE,
	NPI_CMD_LNA_ON,
	NPI_CMD_LNA_OFF,
	NPI_CMD_GET_LNA_STATUS,
	NPI_CMD_SPEED_UP,
	NPI_CMD_SPEED_DOWN,
	NPI_CMD_MAX
};

/* wlan_sipc add key value struct*/
struct wlan_sipc_npi {
	u8 id;
	u16 len;
	u8 value[0];
} __packed;

extern int npi_init_netlink(void);
extern void npi_exit_netlink(void);
#endif
