/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * Filename : wapi.h
 *
 * Authors      :
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SPRDWL_WAPI_H__
#define __SPRDWL_WAPI_H__

#include "sprdwl.h"
#include "cmdevt.h"

#define ETH_PKT_TYPE_OFFSET       12
#define WAPI_TYPE                 0x88B4
#define IPV6_TYPE                 0x86DD
#define IP_TYPE                   0x0800
#define ARP_TYPE                  0x0806
#define ONE_X_TYPE                0x888E
#define VLAN_TYPE                 0x8100
#define LLTD_TYPE                 0x88D9
#define UDP_TYPE                  0x11
#define TCP_TYPE                  0x06
#define SNAP_HDR_LEN		  8
#define ETHERNET_HDR_LEN          14
#define IP_HDR_OFFSET             ETHERNET_HDR_LEN
#define IP_HDR_LEN                20
#define IP_PROT_OFFSET            23
#define UDP_HDR_OFFSET            (IP_HDR_LEN + IP_HDR_OFFSET)
#define UDP_HDR_LEN               8
#define UDP_DATA_OFFSET           (UDP_HDR_OFFSET + UDP_HDR_LEN)
#define UDP_SRC_PORT_OFFSET       UDP_HDR_OFFSET
#define UDP_DST_PORT_OFFSET       (UDP_HDR_OFFSET + 2)
#define VLAN_HDR_LEN              18
#define TOS_FIELD_OFFSET          15
#define VLAN_TID_FIELD_OFFSET     14
#define MAC_UDP_DATA_LEN          1472
#define MAX_UDP_IP_PKT_LEN        (MAC_UDP_DATA_LEN + UDP_DATA_OFFSET)
#define SPRDWL_WAPI_ATTACH_LEN    18

static inline int sprdwl_is_wapi(struct sprdwl_vif *vif, unsigned char *data)
{
	return (vif->prwise_crypto == SPRDWL_CIPHER_WAPI &&
		vif->key_len[SPRDWL_PAIRWISE]
			    [vif->key_index[SPRDWL_PAIRWISE]] != 0 &&
		(*(u16 *)(data + ETH_PKT_TYPE_OFFSET) != 0xb488));
}

unsigned short sprdwl_wapi_enc(struct sprdwl_vif *vif,
			       unsigned char *data,
			       unsigned short data_len,
			       unsigned char *output_buf);

unsigned short sprdwl_wapi_dec(struct sprdwl_vif *vif,
			       unsigned char *input_ptk,
			       unsigned short header_len,
			       unsigned short data_len,
			       unsigned char *output_buf);

/* This function compares the address with the (last bit on air) BIT24 to    */
/* determine if the address is a group address.                              */
/* Returns true if the input address has the group bit set.                  */
static inline bool is_group(unsigned char *addr)
{
	if ((addr[0] & BIT(0)) != 0)
		return true;

	return false;
}

static inline unsigned char *inc_wapi_pairwise_key_txrsc(struct sprdwl_vif *vif)
{
	int i;

	vif->key_txrsc[1][15] += 2;

	if (vif->key_txrsc[1][15] == 0x00) {
		for (i = 14; i >= 0; i--) {
			vif->key_txrsc[1][i] += 1;
			if ((vif->key_txrsc[1][i]) != 0x00)
				break;
		}
	}

	return vif->key_txrsc[1];
}

static inline unsigned char *mget_wapi_group_pkt_key(struct sprdwl_vif *vif,
						     int index)
{
	return (index >= 3) ? NULL : vif->key[0][index];
}

static inline unsigned char *mget_wapi_pairwise_pkt_key(struct sprdwl_vif *vif,
							int index)
{
	return (index >= 3) ? NULL : vif->key[1][index];
}

static inline unsigned char *mget_wapi_group_mic_key(struct sprdwl_vif *vif,
						     int index)
{
	return (index >= 3) ? NULL : ((u8 *)vif->key[0][index] + 16);
}

static inline unsigned char *mget_wapi_pairwise_mic_key(struct sprdwl_vif *vif,
							int index)
{
	return (index >= 3) ? NULL : ((u8 *)vif->key[1][index] + 16);
}

#endif /* __WAPI_H__ */
