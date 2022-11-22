/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Filename : wapi.h
 *
 * Authors      :
 * Wenjie.Zhang <Wenjie.Zhang@spreadtrum.com>
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

#ifndef __WAPI_H__
#define __WAPI_H__

#include "sprdwl.h"

#ifndef false
#define false 0
#endif
#ifndef true
#define true 1
#endif
#ifndef NULL
#define NULL 0
#endif

/* Bit Values */
#define BIT31                   ((unsigned int)(1 << 31))
#define BIT30                   ((unsigned int)(1 << 30))
#define BIT29                   ((unsigned int)(1 << 29))
#define BIT28                   ((unsigned int)(1 << 28))
#define BIT27                   ((unsigned int)(1 << 27))
#define BIT26                   ((unsigned int)(1 << 26))
#define BIT25                   ((unsigned int)(1 << 25))
#define BIT24                   ((unsigned int)(1 << 24))
#define BIT23                   ((unsigned int)(1 << 23))
#define BIT22                   ((unsigned int)(1 << 22))
#define BIT21                   ((unsigned int)(1 << 21))
#define BIT20                   ((unsigned int)(1 << 20))
#define BIT19                   ((unsigned int)(1 << 19))
#define BIT18                   ((unsigned int)(1 << 18))
#define BIT17                   ((unsigned int)(1 << 17))
#define BIT16                   ((unsigned int)(1 << 16))
#define BIT15                   ((unsigned int)(1 << 15))
#define BIT14                   ((unsigned int)(1 << 14))
#define BIT13                   ((unsigned int)(1 << 13))
#define BIT12                   ((unsigned int)(1 << 12))
#define BIT11                   ((unsigned int)(1 << 11))
#define BIT10                   ((unsigned int)(1 << 10))
#define BIT9                    ((unsigned int)(1 << 9))
#define BIT8                    ((unsigned int)(1 << 8))
#define BIT7                    ((unsigned int)(1 << 7))
#define BIT6                    ((unsigned int)(1 << 6))
#define BIT5                    ((unsigned int)(1 << 5))
#define BIT4                    ((unsigned int)(1 << 4))
#define BIT3                    ((unsigned int)(1 << 3))
#define BIT2                    ((unsigned int)(1 << 2))
#define BIT1                    ((unsigned int)(1 << 1))
#define BIT0                    ((unsigned int)(1 << 0))
#define ALL                     0xFFFF

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

extern unsigned short wlan_tx_wapi_encryption(struct sprdwl_priv *priv,
					      unsigned char *data,
					      unsigned short data_len,
					      unsigned char *ouput_buf);

extern unsigned short wlan_rx_wapi_decryption(struct sprdwl_priv *priv,
					      unsigned char *input_ptk,
					      unsigned short header_len,
					      unsigned short data_len,
					      unsigned char *output_buf);

/* This function compares the address with the (last bit on air) BIT24 to    */
/* determine if the address is a group address.                              */
/* Returns true if the input address has the group bit set.                  */
static inline bool is_group(unsigned char *addr)
{
	if ((addr[0] & BIT0) != 0)
		return true;

	return false;
}

static inline unsigned char *inc_wapi_pairwise_key_txrsc(struct sprdwl_priv
							 *priv)
{
	int i;

	priv->key_txrsc[1][15] += 2;

	if (priv->key_txrsc[1][15] == 0x00) {
		for (i = 14; i >= 0; i--) {
			priv->key_txrsc[1][i] += 1;
			if ((priv->key_txrsc[1][i]) != 0x00)
				break;
		}
	}

	return priv->key_txrsc[1];
}

static inline unsigned char *mget_wapi_group_pkt_key(struct sprdwl_priv *priv,
						     int index)
{
	return (index >= 3) ? NULL : priv->key[0][index];
}

static inline unsigned char *mget_wapi_pairwise_pkt_key(struct sprdwl_priv
							*priv, int index)
{
	return (index >= 3) ? NULL : priv->key[1][index];
}

static inline unsigned char *mget_wapi_group_mic_key(struct sprdwl_priv *priv,
						     int index)
{
	return (index >= 3) ? NULL : ((u8 *)priv->key[0][index] + 16);
}

static inline unsigned char *mget_wapi_pairwise_mic_key(struct sprdwl_priv
							*priv, int index)
{
	return (index >= 3) ? NULL : ((u8 *)priv->key[1][index] + 16);
}

#endif /* __WAPI_H__ */
