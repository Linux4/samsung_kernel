/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Filename : npi.h
 * Abstract : This file is a general definition for NPI cmd
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

#ifndef __NPI_H__
#define __NPI_H__

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
	/*for samsung hardware test interface */
	NPI_CMD_PREAMBLE_TYPE,
	NPI_CMD_SHORTGI,
	NPI_CMD_SET_BURST_INTERVAL,
	NPI_CMD_PAYLOAD,

	NPI_CMD_BAND,
	NPI_CMD_BANDWIDTH,
	NPI_CMD_MACFILTER,

	NPI_CMD_PKTLEN,

	NPI_CMD_MAX
};

/**
 * enum nlnpi_commands - supported nlnpi commands
 *
 * @NLNPI_CMD_UNSPEC: unspecified command to catch errors
 *
 * @NLNPI_CMD_START: Start NPI operation, no parameters.
 * @NLNPI_CMD_STOP: Stop NPI operation.
 *
 * @NLNPI_CMD_MAX: highest used command number
 * @__NLNPI_CMD_AFTER_LAST: internal use
 */
enum nlnpi_commands {
/* don't change the order or add anything between, this is ABI! */
	NLNPI_CMD_UNSPEC,

	NLNPI_CMD_START,
	NLNPI_CMD_STOP,
	NLNPI_CMD_SET_MAC,
	NLNPI_CMD_GET_MAC,
	NLNPI_CMD_SET_CHANNEL,
	NLNPI_CMD_GET_CHANNEL,
	NLNPI_CMD_GET_RSSI,
	NLNPI_CMD_SET_TX_MODE,
	NLNPI_CMD_SET_RATE,
	NLNPI_CMD_GET_RATE,
	NLNPI_CMD_TX_START,
	NLNPI_CMD_TX_STOP,
	NLNPI_CMD_RX_START,
	NLNPI_CMD_RX_STOP,
	NLNPI_CMD_SET_TX_POWER,
	NLNPI_CMD_GET_TX_POWER,
	NLNPI_CMD_SET_TX_COUNT,
	NLNPI_CMD_GET_RX_OK_COUNT,
	NLNPI_CMD_GET_RX_ERR_COUNT,
	NLNPI_CMD_CLEAR_RX_COUNT,
	NLNPI_CMD_GET_REG,
	NLNPI_CMD_SET_REG,
	NLNPI_CMD_SET_DEBUG,
	NLNPI_CMD_GET_DEBUG,
	NLNPI_CMD_SET_SBLOCK,
	NLNPI_CMD_GET_SBLOCK,
	/* add new commands above here */
	/* herry.he add tx sin wave */
	NLNPI_CMD_SIN_WAVE,
	NLNPI_CMD_LNA_ON,
	NLNPI_CMD_LNA_OFF,
	NLNPI_CMD_GET_LNA_STATUS,
	NLNPI_CMD_SPEED_UP,
	NLNPI_CMD_SPEED_DOWN,
	/*for samsung hardware test interface */
	NLNPI_CMD_SET_LONG_PREAMBLE,
	NLNPI_CMD_SET_SHORT_GUARD_INTERVAL,
	NLNPI_CMD_SET_BURST_INTERVAL,
	NLNPI_CMD_SET_PAYLOAD,
	NLNPI_CMD_GET_PAYLOAD,

	NLNPI_CMD_SET_PREAMBLE_TYPE,
	NLNPI_CMD_GET_PREAMBLE_TYPE,

	NLNPI_CMD_GET_TX_MODE,

	/* set_band */
	NLNPI_CMD_SET_BAND,
	/* set_band */
	NLNPI_CMD_GET_BAND,

	/* set_bandwidth */
	NLNPI_CMD_SET_BANDWIDTH,
	/* get_bandwidth */
	NLNPI_CMD_GET_BANDWIDTH,

	/* set_guard_interval */
	NLNPI_CMD_SET_SHORTGI,
	/* get_guard_interval */
	NLNPI_CMD_GET_SHORTGI,

	/* set_macfilter */
	NLNPI_CMD_SET_MACFILTER,
	/* get_macfilter */
	NLNPI_CMD_GET_MACFILTER,

	NLNPI_CMD_SET_PKTLEN,
	NLNPI_CMD_GET_PKTLEN,

	/* used to define NLNPI_CMD_MAX below */
	__NLNPI_CMD_AFTER_LAST,
	NLNPI_CMD_MAX = __NLNPI_CMD_AFTER_LAST - 1
};

/**
 * enum nlnpi_attrs - nlnpi netlink attributes
 *
 * @NLNPI_ATTR_UNSPEC: unspecified attribute to catch errors
 *
 * @NLNPI_ATTR_MAX: highest attribute number currently defined
 * @__NLNPI_ATTR_AFTER_LAST: internal use
 */
enum nlnpi_attrs {
/* don't change the order or add anything between, this is ABI! */
	NLNPI_ATTR_UNSPEC,

	NLNPI_ATTR_IFINDEX,

	NLNPI_ATTR_REPLY_STATUS,
	NLNPI_ATTR_REPLY_DATA,
	NLNPI_ATTR_GET_NO_ARG,

	NLNPI_ATTR_START,
	NLNPI_ATTR_STOP,
	NLNPI_ATTR_TX_START,
	NLNPI_ATTR_TX_STOP,
	NLNPI_ATTR_RX_START,
	NLNPI_ATTR_RX_STOP,
	NLNPI_ATTR_SET_TX_MODE,
	NLNPI_ATTR_TX_COUNT,
	NLNPI_ATTR_RSSI,
	NLNPI_ATTR_GET_RX_COUNT,
	NLNPI_ATTR_SET_RX_COUNT,
	NLNPI_ATTR_GET_TX_POWER,
	NLNPI_ATTR_SET_TX_POWER,
	NLNPI_ATTR_GET_CHANNEL,
	NLNPI_ATTR_SET_CHANNEL,
	NLNPI_ATTR_SET_RATE,
	NLNPI_ATTR_GET_RATE,
	NLNPI_ATTR_SET_MAC,
	/* all get attr is arg len */
	NLNPI_ATTR_GET_MAC,
	NLNPI_ATTR_SET_REG,
	NLNPI_ATTR_GET_REG,
	NLNPI_ATTR_GET_REG_ARG,
	NLNPI_ATTR_SET_DEBUG,
	NLNPI_ATTR_GET_DEBUG,
	NLNPI_ATTR_GET_DEBUG_ARG,
	NLNPI_ATTR_SBLOCK_ARG,

	/* add attributes here, update the policy in sprdwl_nlnpi.c */
	/* herry.he add tx sin wave */
	NLNPI_ATTR_SIN_WAVE,
	NLNPI_ATTR_LNA_ON,
	NLNPI_ATTR_LNA_OFF,
	NLNPI_ATTR_GET_LNA_STATUS,
	NLNPI_ATTR_SPEED_UP,
	NLNPI_ATTR_SPEED_DOWN,
	/*for samsung hardware test interface */
	NLNPI_ATTR_SET_LONG_PREAMBLE,
	NLNPI_ATTR_SET_SHORT_GUARD_INTERVAL,
	NLNPI_ATTR_SET_BURST_INTERVAL,

	/* payload */
	NLNPI_ATTR_SET_PAYLOAD,
	NLNPI_ATTR_GET_PAYLOAD,

	NLNPI_ATTR_SET_PREAMBLE_TYPE,
	NLNPI_ATTR_GET_PREAMBLE_TYPE,

	NLNPI_ATTR_GET_TX_MODE,

	/* set_band */
	NLNPI_ATTR_SET_BAND,
	/* get_band */
	NLNPI_ATTR_GET_BAND,

	/* bandwidth */
	NLNPI_ATTR_SET_BANDWIDTH,
	NLNPI_ATTR_GET_BANDWIDTH,

	/* gi */
	NLNPI_ATTR_SET_SHORTGI,
	NLNPI_ATTR_GET_SHORTGI,

	/* macfilter */
	NLNPI_ATTR_SET_MACFILTER,
	NLNPI_ATTR_GET_MACFILTER,

	/* pktlen */
	NLNPI_ATTR_SET_PKTLEN,
	NLNPI_ATTR_GET_PKTLEN,

	__NLNPI_ATTR_AFTER_LAST,
	NLNPI_ATTR_MAX = __NLNPI_ATTR_AFTER_LAST - 1
};

/* wlan_sipc add key value struct*/
struct wlan_sipc_npi {
	u8 id;
	u16 len;
	u8 value[0];
} __packed;

extern int npi_init_netlink(void);
extern void npi_exit_netlink(void);
#endif /*__NPI_H__*/
