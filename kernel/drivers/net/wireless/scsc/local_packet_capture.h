/****************************************************************************
 *
 * Copyright (c) 2023 - 2023 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SLSI_LPC_H__
#define __SLSI_LPC_H__

#include "dev.h"
#include "debug.h"
#include <linux/skbuff.h>
#include <net/genetlink.h>
#include <net/ieee80211_radiotap.h>

#define LPC_TAG_NUM_MAX (0xFFFF)

#define LPC_STORAGE_DESCRIPTOR_MAGIC_NUMBER (0x5AB1EB39)
#define LPC_STORAGE_DESCRIPTOR_VERSION_MAJOR (1)
#define LPC_STORAGE_DESCRIPTOR_VERSION_MINOR (2)
#define LPC_TLV_CONTAINER_MAGIC_NUMBER (0x25DE5A92)

#define LPC_TYPE_AMPDU_TX (0x00c9)
#define LPC_TYPE_MPDU_TX (0x00ca)
#define LPC_TYPE_AMPDU_RX (0x00cb)
#define LPC_TYPE_MPDU_RX (0x00cc)
#define LPC_TYPE_PHY_DOLLOP (0x00cd)
#define LPC_TYPE_PADDING (0xffff)

#define LPC_RADIOTAP_MAX_BIT_NUMBER (30)

struct log_descriptor {
	u32  magic_number;
	u8   version_major; /* Major version 1*/
	u8   version_minor; /* Minor version 2*/
	u16  tlv_container_cnt;
	u32  fw_sable_addr;
	u32  reserved;
	u32  buffer_size;
	u32  alloc_offset;
} __packed;

struct tlv_container {
	u32  magic_number;
	u16  id;
	u16  reserved;
	u32  flag;
	u32  w_index;
	u32  r_index;
	u32  of_tlvs;
	u32  buffer_size;
} __packed;

struct tlv_hdr {
	u16  type;
	u16  len;
} __packed;

#define LPC_TX_FLAG_RTS_REQUESTED (BIT(0))
#define LPC_TX_FLAG_PS_BIT_SET (BIT(1))
#define LPC_TX_FLAG_AMSDU_SUBFRAME (BIT(2))

struct lpc_type_mpdu_tx {
	u32 systime;
	u32 tx_mactime;
	u16 tx_spent;
	u8 mac_instance;
	u8 tx_state;
	u16 flag16;
	u8 rts_retried;
	u8 content_len;
	u16 mpdu_len;
	u32 tx_req_rate;
	u32 tx_cfm_rate;
	u16 tx_attempts;
	u16 delay_ms;
	u32 lpc_tag;
	u16 tx_frames_in_vif;
	u8 content[64];
	u16 primary_freq;
} __packed;

#define LPC_NUM_MPDU_MAX_IN_AMPDU_TX (64)

struct lpc_type_ampdu_tx {
	u32 systime;
	u32 tx_mactime;
	u16 tx_spent;
	u8 mac_instance;
	u8 tx_state;
	u16 flag16;
	u32 rts_rate;
	u32 tx_rate;
	u16 delay_ms;
	u32 tx_lpc_tag_start;
	u16 tx_frames_in_vif;
	u16 fc;
	u16 ra[3];
	u16 ta[3];
	u16 start_seq;
	u8 tid;
	u8 reserved;
	u64 req_bitmap;
	u64 ack_bitmap;
	u64 retry_bitmap;
	u16 mpdu_len[LPC_NUM_MPDU_MAX_IN_AMPDU_TX];
	u8 fw_agg_tags[LPC_NUM_MPDU_MAX_IN_AMPDU_TX];
	u32 tx_tag_list[LPC_NUM_MPDU_MAX_IN_AMPDU_TX];
	u16 primary_freq;
} __packed;

struct lpc_type_mpdu_rx {
	u32 systime;
	u8 sync_counter;
	u32 lpc_tag;
	u8 mac_instance;
	u16 mpdu_len;
	u8 content_len;
	u8 reserved;
	u16 mbulks_dpif_large;
	u16 mbulks_in_host;
	u8 content[64];
} __packed;

#define LPC_NUM_MPDU_MAX_IN_AMPDU_RX (64)
#define LPC_MAC_HDR_QOS_SIZE (26)

struct lpc_type_ampdu_rx {
	u32 systime;
	u8 sync_counter;
	u32 lpc_tag;
	u8 mac_instance;
	u8 num_mpdu;
	u8 has_phy_dollop;
	u16 mbulks_dpif_large;
	u16 mbulks_in_host;
	u16 seq_frag_list[LPC_NUM_MPDU_MAX_IN_AMPDU_RX];
	u16 mpdu_len_list[LPC_NUM_MPDU_MAX_IN_AMPDU_RX];
	u64 retry_bitmap;
	u8 mac_header[LPC_MAC_HDR_QOS_SIZE];
} __packed;

struct lpc_type_phy_dollop {
	u32 systime;
	u32 rx_start_mactime;
	u32 rx_rate;
	u32 pdu_len;
	u16 flag;
	u8 group_id;
	u8 res;
	u8 sync_counter;
	u8 mac_instance;
	short rssi;
	short snr;
	int freq_offset;
	u16 primary_freq;
} __packed;

struct rx_radiotap_hdr {
	struct ieee80211_radiotap_header hdr;
	__le64 rt_tsft;
	u8 rt_rate;
	short rssi;
	short snr;
} __packed;

struct tx_radiotap_hdr_mpdu {
	struct ieee80211_radiotap_header hdr;
	__le64 rt_tsft;
} __packed;

struct tx_radiotap_hdr_ampdu {
	struct ieee80211_radiotap_header hdr;
	__le64 rt_tsft;
	u32 ampdu_reference;
	u16 ampdu_flags;
	u16 ampdu_pad;
} __packed;

/* FLAG BITMAP */
#define FRAME_COMPLETED (BIT(0)) /* MSDU frame has been successfully completed in FW. Can be removed in skb_clone hashmap */
#define FRAME_DROPPED (BIT(1)) /* MSDU frame has been dropped in FW. Can be removed in skb_clone hashmap */
#define FRAME_FIRST (BIT(2)) /* First MPDU frame of a A-MPDU */
#define FRAME_LAST (BIT(3)) /* Last MPDU frame of a A-MPDU */
#define FRAME_PAYLOAD_INLINE (BIT(4)) /* Payload in-line */
#define FRAME_DROPPED_SUBF_HEADER (BIT(5)) /* Subframe header dropped (when one single MSDU is sent) */
#define FRAME_IS_MSDU (BIT(5)) /* Next 4 octets refers to MSDU dma_addr_t */

/* attributes */
enum slsi_lpc_attr {
	SLSI_LPC_ATTR_PKT_A_UNSPEC,
	SLSI_LPC_ATTR_PKT_LOG_TYPE, /* u8 */
	SLSI_LPC_ATTR_PKT_PAYLOAD, /* binary */
	__SLSI_LPC_ATTR_MAX,
	SLSI_LPC_ATTR_MAX = __SLSI_LPC_ATTR_MAX - 1
};

/* commands */
enum {
	SLSI_LPC_C_UNSPEC,
	SLSI_LPC_C_TEST,
	__SLSI_LPC_C_MAX,
	SLSI_LPC_C_MAX = __SLSI_LPC_C_MAX - 1
};

enum slsi_lpc_test_code {
	SLSI_LOCAL_PACKET_CAPTURE_DATA,
	SLSI_LOCAL_PACKET_CAPTURE_START,
	SLSI_LOCAL_PACKET_CAPTURE_STOP,
};

enum slsi_lpc_data_type {
	SLSI_LPC_DATA_TYPE_RX,
	SLSI_LPC_DATA_TYPE_TX,
	SLSI_LPC_DATA_TYPE_AMPDU_RX_INFO,
};

int slsi_lpc_get_packets_info(struct slsi_dev *sdev);
int slsi_lpc_add_packet_data(u32 lpc_tag, struct sk_buff *skb, int data_type);
int slsi_lpc_send_ampdu_rx_later(u32 lpc_tag, struct sk_buff *skb);
int slsi_lpc_init(void);
int slsi_lpc_deinit(struct slsi_dev *sdev);
int slsi_lpc_start(struct slsi_dev *sdev, int type);
int slsi_lpc_stop(struct slsi_dev *sdev);
int slsi_lpc_is_lpc_enabled(void);
#endif
