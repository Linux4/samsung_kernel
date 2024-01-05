/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Filename : sipc_types.h
 * Abstract : This file is a general definition for sipc data struct
 *
 * Authors	:
 * Leon Liu <leon.liu@spreadtrum.com>
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

#ifndef __ITM_SIPC_TYPES_H__
#define __ITM_SIPC_TYPES_H__

#include <linux/ieee80211.h>
#include "WIFI_nvm_data.h"

/*MSG TAG*/
#define MSG_TYPE_MASK		0x0000FFFF
#define MSG_LEN_MASK		0xFFFF0000
#define MSG_TYPE_OFFSET		0
#define MSG_LEN_OFFSET		16
/*MSG TYPES*/
#define ITM_WLAN_MSG_CMD		0x0
#define ITM_WLAN_MSG_CMD_RSP		0x1
#define ITM_WLAN_MSG_EVENT		0x2
#define ITM_WLAN_MSG_MAX		(ITM_WLAN_MSG_EVENT + 1)
#define ITM_WLAN_MAX_MSG_LEN		65535
/*help function*/
#define ITM_WLAN_MSG(type, len)		((type << MSG_TYPE_OFFSET)	\
					| (len << MSG_LEN_OFFSET))
#define ITM_WLAN_MSG_GET_TYPE(msg)	(((msg) & MSG_TYPE_MASK)	\
					>> MSG_TYPE_OFFSET)
#define ITM_WLAN_MSG_GET_LEN(msg)	(((msg) & MSG_LEN_MASK)	\
					>> MSG_LEN_OFFSET)
#define ITM_WLAN_MSG_HDR_SIZE		4

/*CMD TAG*/
#define CMD_TAG_TYPE_MASK		0xF000
#define CMD_TAG_DIR_MASK		0x800
#define CMD_TAG_LEVEL_MASK		0x700
#define CMD_TAG_ID_MASK			0xFF
#define CMD_TAG_TYPE_OFFSET		12
#define CMD_TAG_DIR_OFFSET		11
#define CMD_TAG_LEVEL_OFFSET		8
#define CMD_TAG_ID_OFFSET		0
 /*CMD TYPES */
#define CMD_TYPE_GET			0x0
#define CMD_TYPE_SET			0x1
#define CMD_TYPE_ADD			0x2
#define CMD_TYPE_DEL			0x3
#define CMD_TYPE_FLUSH			0x4
#define CMD_TYPE_MAX			(CMD_TYPE_FLUSH + 1)
/*CMD DIRECTION*/
#define CMD_DIR_AP2CP			0
#define CMD_DIR_CP2AP			1
/*CMD LEVEL*/
#define CMD_LEVEL_DEBUG			0x0
#define CMD_LEVEL_NORMAL		0x1
#define CMD_LEVEL_MAX			(CMD_LEVEL_NORMAL + 1)
/*help function*/
#define ITM_WLAN_CMD_TAG(type, dir, level, id)((type << CMD_TAG_TYPE_OFFSET)\
					| (dir << CMD_TAG_DIR_OFFSET)	 \
					| (level << CMD_TAG_LEVEL_OFFSET)\
					| (id << CMD_TAG_ID_OFFSET))
#define ITM_WLAN_CMD_TAG_GET_TYPE(cmd)	(((cmd) & CMD_TAG_TYPE_MASK) \
					>> CMD_TAG_TYPE_OFFSET)
#define ITM_WLAN_CMD_TAG_GET_DIR(cmd)	(((cmd) & CMD_TAG_DIR_MASK)	 \
					>> CMD_TAG_DIR_OFFSET)
#define ITM_WLAN_CMD_TAG_GET_LEVEL(cmd)	(((cmd) & CMD_TAG_LEVEL_MASK)\
					>> CMD_TAG_LEVEL_OFFSET)
#define ITM_WLAN_CMD_TAG_GET_ID(cmd)	(((cmd) & CMD_TAG_ID_MASK)	 \
					>> CMD_TAG_ID_OFFSET)
#define	ITM_WLAN_CMD_HDR_SIZE			(ITM_WLAN_MSG_HDR_SIZE + 4)
#define	ITM_WLAN_CMD_RESP_HDR_SIZE		(ITM_WLAN_MSG_HDR_SIZE + 8)

/*EVENT TAG*/
#define EVENT_TAG_TYPE_MASK			0xF000
#define EVENT_TAG_ID_MASK			0xFFF
#define EVENT_TAG_TYPE_OFFSET			12
#define EVENT_TAG_ID_OFFSET			0
/*EVENT TYPES*/
#define EVENT_TYPE_NORMAL			0x0
#define EVENT_TYPE_SYSERR			0x1
#define EVENT_TYPE_MAX				(EVENT_TYPE_SYSERR + 1)
/*help function*/
#define ITM_WLAN_EVENT_TAG(type, id)	((type << EVENT_TAG_TYPE_OFFSET) \
					| (id << EVENT_TAG_ID_OFFSET))
#define ITM_WLAN_EVENT_TAG_GET_TYPE(event)(((event) & EVENT_TAG_TYPE_MASK) \
					>> EVENT_TAG_TYPE_OFFSET)
#define ITM_WLAN_EVENT_TAG_GET_ID(event)(((event) & EVENT_TAG_ID_MASK) \
					>> EVENT_TAG_ID_OFFSET)
#define ITM_WLAN_EVENT_HDR_SIZE			(ITM_WLAN_MSG_HDR_SIZE + 6)

/*device mode*/
#define DEVICE_MODE_STA				0
#define DEVICE_MODE_Access_Point		1
#define DEVICE_MODE_P2P_Client			2
#define DEVICE_MODE_P2P_GO			3

/* auth type */
#define ITM_AUTH_OPEN	0
#define ITM_AUTH_SHARED	1

/*cipher type*/
#define NONE		0
#define WEP40		1
#define WEP104		2
#define TKIP		3
#define CCMP		4
#define AP_TKIP		5
#define AP_CCMP		6
#define WAPI		7

/*AKM suite*/
#define AKM_SUITE_PSK	1
#define AKM_SUITE_8021X	2
#define AKM_SUITE_WAPI	3

/*PMKID max len*/
#define ITM_PMKID_LEN		16

/* WPS type */
#define WPS_REQ_IE	1
#define WPS_ASSOC_IE	2

#define ETH_ALEN		6

enum wlan_sipc_status {
	WLAN_SIPC_STATUS_OK = 0,
	WLAN_SIPC_STATUS_ERR = -1,
};

/* The reason code is defined by CP2 */
enum wlan_sipc_disconnect_reason {
	AP_LEAVING = 0xc1,
        AP_DEAUTH = 0xc4,
};

/* ITM_WLAN SIPC DATA*/
struct wlan_sipc_data {
	u32 msg_hdr;
	union {
		struct {
			u16 cmd_tag;
			u16 seq_no;
			/* possibly followed by cmd data */
			u8 variable[0];
		} __packed cmd;
		struct {
			u16 cmd_tag;
			u16 seq_no;
			u16 status_code;
			u16 resv;
			/* possibly followed by cmd resp data */
			u8 variable[0];
		} __packed cmd_resp;
		struct {
			u16 event_tag;
			u16 seq_no;
			u16 resv;
			/* possibly followed by event text */
			u8 variable[0];
		} __packed event;
	} u;
} __packed;

/* wlan_sipc add key value struct*/
struct wlan_sipc_add_key {
	u8 mac[6];
	u8 keyseq[8];
	u8 pairwise;
	u8 cypher_type;
	u8 key_index;
	u8 key_len;
	u8 value[0];
} __packed;

/*wlan_sipc del key value struct */
struct wlan_sipc_del_key {
	u8 key_index;
	u8 pairwise;		/* unicase or group */
	u8 mac[6];
} __packed;

/* wlan_sipc pmkid set/del/flush struct */
struct wlan_sipc_pmkid {
	u8 bssid[ETH_ALEN];
	u8 pmkid[ITM_PMKID_LEN];
} __packed;

/* wlan_sipc beacon struct */
struct wlan_sipc_beacon {
	u8 len;
	u8 value[0];
} __packed;

/* wlan_sipc mac open struct */
struct wlan_sipc_mac_open {
	u8 mode;	/* AP or STATION mode */
	u8 mac[6];
	WIFI_nvm_data nvm_data;
} __packed;

/* wlan_sipc wps ie struct */
struct wlan_sipc_wps_ie {
	u8 type;	/* probe req ie or assoc req ie */
	u8 len;		/* max ie len is 255 */
	u8 value[0];
} __packed;

/* combo scan data */
struct wlan_sipc_scan_ssid {
	u8 len;
	u8 ssid[0];
} __packed;

/* sblock data */
struct wlan_sblock_recv_data {
	u8 is_encrypted;
	union {
		u8 resv[13];
		struct {
			u8 resv[13];
		} __packed nomal;
		struct {
			u16 header_len;
			/* followed by reseved data */
			u8 resv[11];
		} __packed encrypt;
	} u1;
	union {
		struct {
			u8 eth_header[14];
			/* followed by nomal data */
			u8 variable[0];
		} __packed nomal;
		struct {
			struct ieee80211_hdr_3addr mac_header;
			/* followed by nomal data */
			u8 variable[0];
		} __packed encrypt;
	} u2;
} __packed;

struct wlan_softap_event {
	u8 connected;
	u8 mac[6];
} __packed;

#endif/*__ITM_SIPC_TYPES_H__*/
