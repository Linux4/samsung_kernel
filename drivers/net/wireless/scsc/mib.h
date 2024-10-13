/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/* Note: this is an auto-generated file. */

#ifndef SLSI_MIB_H__
#define SLSI_MIB_H__

#ifdef __cplusplus
extern "C" {
#endif

struct slsi_mib_data {
	u32 dataLength;
	u8  *data;
};

#define SLSI_MIB_MAX_INDEXES 2U

#define SLSI_MIB_TYPE_BOOL    0
#define SLSI_MIB_TYPE_UINT    1
#define SLSI_MIB_TYPE_INT     2
#define SLSI_MIB_TYPE_OCTET   3U
#define SLSI_MIB_TYPE_NONE    4

struct slsi_mib_value {
	u8 type;
	union {
		bool                 boolValue;
		s32                  intValue;
		u32                  uintValue;
		struct slsi_mib_data octetValue;
	} u;
};

struct slsi_mib_entry {
	u16                   psid;
	u16                   index[SLSI_MIB_MAX_INDEXES]; /* 0 = no Index */
	struct slsi_mib_value value;
};

struct slsi_mib_get_entry {
	u16 psid;
	u16 index[SLSI_MIB_MAX_INDEXES]; /* 0 = no Index */
};

#define SLSI_MIB_STATUS_SUCCESS                     0x0000
#define SLSI_MIB_STATUS_UNKNOWN_PSID                0x0001
#define SLSI_MIB_STATUS_INVALID_INDEX               0x0002
#define SLSI_MIB_STATUS_OUT_OF_RANGE                0x0003
#define SLSI_MIB_STATUS_WRITE_ONLY                  0x0004
#define SLSI_MIB_STATUS_READ_ONLY                   0x0005
#define SLSI_MIB_STATUS_UNKNOWN_INTERFACE_TAG       0x0006
#define SLSI_MIB_STATUS_INVALID_NUMBER_OF_INDICES   0x0007
#define SLSI_MIB_STATUS_ERROR                       0x0008
#define SLSI_MIB_STATUS_UNSUPPORTED_ON_INTERFACE    0x0009
#define SLSI_MIB_STATUS_UNAVAILABLE                 0x000A
#define SLSI_MIB_STATUS_NOT_FOUND                   0x000B
#define SLSI_MIB_STATUS_INCOMPATIBLE                0x000C
#define SLSI_MIB_STATUS_OUT_OF_MEMORY               0x000D
#define SLSI_MIB_STATUS_TO_MANY_REQUESTED_VARIABLES 0x000E
#define SLSI_MIB_STATUS_NOT_TRIED                   0x000F
#define SLSI_MIB_STATUS_FAILURE                     0xFFFF

/*******************************************************************************
 *
 * NAME
 *  slsi_mib_encode_get Functions
 *
 * DESCRIPTION
 *  For use when getting data from the Wifi Stack.
 *  These functions append the encoded data to the "buffer".
 *
 *  index == 0 where there is no index required
 *
 * EXAMPLE
 *  {
 *      static const struct slsi_mib_get_entry getValues[] = {
 *          { PSID1, { 0, 0 } },
 *          { PSID2, { 3, 0 } },
 *      };
 *      struct slsi_mib_data buffer;
 *      slsi_mib_encode_get_list(&buffer,
 *                              sizeof(getValues) / sizeof(struct slsi_mib_get_entry),
 *                              getValues);
 *  }
 *  or
 *  {
 *      struct slsi_mib_data buffer = {0, NULL};
 *      slsi_mib_encode_get(&buffer, PSID1, 0);
 *      slsi_mib_encode_get(&buffer, PSID2, 3);
 *  }
 * RETURN
 *  SlsiResult: See SLSI_MIB_STATUS_*
 *
 *******************************************************************************/
void slsi_mib_encode_get(struct slsi_mib_data *buffer, u16 psid, u16 index);
int slsi_mib_encode_get_list(struct slsi_mib_data *buffer, u16 psidsLength, const struct slsi_mib_get_entry *psids);

/*******************************************************************************
 *
 * NAME
 *  SlsiWifiMibdEncode Functions
 *
 * DESCRIPTION
 *  For use when getting data from the Wifi Stack.
 *
 *  index == 0 where there is no index required
 *
 * EXAMPLE
 *  {
 *      static const struct slsi_mib_get_entry getValues[] = {
 *          { PSID1, { 0, 0 } },
 *          { PSID2, { 3, 0 } },
 *      };
 *      struct slsi_mib_data buffer = rxMibData; # Buffer with encoded Mib Data
 *
 *      getValues = slsi_mib_decode_get_list(&buffer,
 *                                      sizeof(getValues) / sizeof(struct slsi_mib_get_entry),
 *                                      getValues);
 *
 *      print("PSID1 = %d\n", getValues[0].u.uintValue);
 *      print("PSID2.3 = %s\n", getValues[1].u.boolValue?"TRUE":"FALSE");
 *
 *      kfree(getValues);
 *
 *  }
 *  or
 *  {
 *      u8* buffer = rxMibData; # Buffer with encoded Mib Data
 *      size_t offset=0;
 *      struct slsi_mib_entry value;
 *
 *      offset += slsi_mib_decode(&buffer[offset], &value);
 *      print("PSID1 = %d\n", value.u.uintValue);
 *
 *      offset += slsi_mib_decode(&buffer[offset], &value);
 *      print("PSID2.3 = %s\n", value.u.boolValue?"TRUE":"FALSE");
 *
 *  }
 *
 *******************************************************************************/
size_t slsi_mib_decode(struct slsi_mib_data *buffer, struct slsi_mib_entry *value);
struct slsi_mib_value *slsi_mib_decode_get_list(struct slsi_mib_data *buffer, u16 psidsLength, const struct slsi_mib_get_entry *psids);

/*******************************************************************************
 *
 * NAME
 *  slsi_mib_encode Functions
 *
 * DESCRIPTION
 *  For use when setting data in the Wifi Stack.
 *  These functions append the encoded data to the "buffer".
 *
 *  index == 0 where there is no index required
 *
 * EXAMPLE
 *  {
 *      u8 octets[2] = {0x00, 0x01};
 *      struct slsi_mib_data buffer = {0, NULL};
 *      slsi_mib_encode_bool(&buffer, PSID1, TRUE, 0);                     # Boolean set with no index
 *      slsi_mib_encode_int(&buffer, PSID2, -1234, 1);                     # Signed Integer set with on index 1
 *      slsi_mib_encode_uint(&buffer, PSID2, 1234, 3);                     # Unsigned Integer set with on index 3
 *      slsi_mib_encode_octet(&buffer, PSID3, sizeof(octets), octets, 0);  # Octet set with no index
 *  }
 *  or
 *  {
 # Unsigned Integer set with on index 3
 #      struct slsi_mib_data buffer = {0, NULL};
 #      struct slsi_mib_entry value;
 #      value.psid = psid;
 #      value.index[0] = 3;
 #      value.index[1] = 0;
 #      value.value.type = SLSI_MIB_TYPE_UINT;
 #      value.value.u.uintValue = 1234;
 #      slsi_mib_encode(buffer, &value);
 #  }
 # RETURN
 #  See SLSI_MIB_STATUS_*
 #
 *******************************************************************************/
u16 slsi_mib_encode(struct slsi_mib_data *buffer, struct slsi_mib_entry *value);
u16 slsi_mib_encode_bool(struct slsi_mib_data *buffer, u16 psid, bool value, u16 index);
u16 slsi_mib_encode_int(struct slsi_mib_data *buffer, u16 psid, s32 value, u16 index);
u16 slsi_mib_encode_uint(struct slsi_mib_data *buffer, u16 psid, u32 value, u16 index);
u16 slsi_mib_encode_octet(struct slsi_mib_data *buffer, u16 psid, size_t dataLength, const u8 *data, u16 index);

/*******************************************************************************
 *
 * NAME
 *  SlsiWifiMib Low level Encode/Decode functions
 *
 *******************************************************************************/
size_t slsi_mib_encode_uint32(u8 *buffer, u32 value);
size_t slsi_mib_encode_int32(u8 *buffer, s32 signedValue);
size_t slsi_mib_encode_octet_str(u8 *buffer, struct slsi_mib_data *octetValue);

size_t slsi_mib_decodeUint32(u8 *buffer, u32 *value);
size_t slsi_mib_decodeInt32(u8 *buffer, s32 *value);
size_t slsi_mib_decode_octet_str(u8 *buffer, struct slsi_mib_data *octetValue);

/*******************************************************************************
 *
 * NAME
 *  SlsiWifiMib Helper Functions
 *
 *******************************************************************************/

/* Find a the offset to psid data in an encoded buffer
 * {
 *      struct slsi_mib_data buffer = rxMibData;                 # Buffer with encoded Mib Data
 *      struct slsi_mib_get_entry value = {PSID1, {0x01, 0x00}};   # Find value for PSID1.1
 *      u8* mibdata = slsi_mib_find(&buffer, &value);
 *      if(mibdata) {print("Mib Data for PSID1.1 Found\n");
 *  }
 */
u8 *slsi_mib_find(struct slsi_mib_data *buffer, const struct slsi_mib_get_entry *entry);

/* Append data to a Buffer */
void slsi_mib_buf_append(struct slsi_mib_data *dst, size_t bufferLength, u8 *buffer);

/*******************************************************************************
 *
 * PSID Definitions
 *
 *******************************************************************************/

/*******************************************************************************
 * NAME          : Dot11TdlsPeerUapsdIndicationWindow
 * PSID          : 53 (0x0035)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : beacon intervals
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  The minimum time that needs to pass after the most recent TPU SP, before
 *  a RAME_TPU_SP indication can be sent to MLME
 *******************************************************************************/
#define SLSI_PSID_DOT11_TDLS_PEER_UAPSD_INDICATION_WINDOW 0x0035

/*******************************************************************************
 * NAME          : Dot11AssociationSaQueryMaximumTimeout
 * PSID          : 100 (0x0064)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 1000
 * DESCRIPTION   :
 *  This attribute specifies the number of time units (TUs) that an AP can
 *  wait, from the scheduling of the first SA Query Request to allow
 *  association process to be started without starting additional SA Query
 *  procedure if a successful SA Query Response is not received.
 *******************************************************************************/
#define SLSI_PSID_DOT11_ASSOCIATION_SA_QUERY_MAXIMUM_TIMEOUT 0x0064

/*******************************************************************************
 * NAME          : Dot11AssociationSaQueryRetryTimeout
 * PSID          : 101 (0x0065)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : TU
 * MIN           : 1
 * MAX           : 4294967295
 * DEFAULT       : 201
 * DESCRIPTION   :
 *  This attribute specifies the number of time units (TUs) that an AP waits
 *  between issuing two subsequent SA Query Request frames.
 *******************************************************************************/
#define SLSI_PSID_DOT11_ASSOCIATION_SA_QUERY_RETRY_TIMEOUT 0x0065

/*******************************************************************************
 * NAME          : Dot11PowerCapabilityMaxImplemented
 * PSID          : 112 (0x0070)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : qdBm
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       :
 * DESCRIPTION   :
 *  This attribute indicates the maximum transmit Power Capability of this
 *  station at the antenna port. Values are expressed in 0.25 dBm units.
 *******************************************************************************/
#define SLSI_PSID_DOT11_POWER_CAPABILITY_MAX_IMPLEMENTED 0x0070

/*******************************************************************************
 * NAME          : Dot11PowerCapabilityMinImplemented
 * PSID          : 113 (0x0071)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : qdBm
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       :
 * DESCRIPTION   :
 *  This attribute indicates the minimum transmit Power Capability of this
 *  station at the antenna port. Values are expressed in 0.25 dBm units.
 *******************************************************************************/
#define SLSI_PSID_DOT11_POWER_CAPABILITY_MIN_IMPLEMENTED 0x0071

/*******************************************************************************
 * NAME          : Dot11RtsThreshold
 * PSID          : 121 (0x0079)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : octet
 * MIN           : 0
 * MAX           : 65536
 * DEFAULT       : 9000
 * DESCRIPTION   :
 *  This attribute shall indicate the size of an MPDU, below which an RTS/CTS
 *  handshake shall not be performed, except as RTS/CTS is used as a cross
 *  modulation protection mechanism as defined in 9.10. An RTS/CTS handshake
 *  shall be performed at the beginning of any frame exchange sequence where
 *  the MPDU is of type Data or Management, the MPDU has an individual
 *  address in the Address1 field, and the length of the MPDU is greater than
 *  this threshold. (For additional details, refer to Table 21 in 9.7.)
 *  Setting this attribute to be larger than the maximum MSDU size shall have
 *  the effect of turning off the RTS/CTS handshake for frames of Data or
 *  Management type transmitted by this STA. Setting this attribute to zero
 *  shall have the effect of turning on the RTS/CTS handshake for all frames
 *  of Data or Management type transmitted by this STA.
 *******************************************************************************/
#define SLSI_PSID_DOT11_RTS_THRESHOLD 0x0079

/*******************************************************************************
 * NAME          : Dot11ShortRetryLimit
 * PSID          : 122 (0x007A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 255
 * DEFAULT       : 15
 * DESCRIPTION   :
 *  This attribute shall indicate the maximum number of transmission attempts
 *  of a frame, the length of which is less than or equal to
 *  dot11RTSThreshold, that shall be made before a failure condition is
 *  indicated.
 *******************************************************************************/
#define SLSI_PSID_DOT11_SHORT_RETRY_LIMIT 0x007A

/*******************************************************************************
 * NAME          : Dot11LongRetryLimit
 * PSID          : 123 (0x007B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 255
 * DEFAULT       : 4
 * DESCRIPTION   :
 *  This attribute shall indicate the maximum number of transmission attempts
 *  of a frame, the length of which is greater than dot11RTSThreshold, that
 *  shall be made before a failure condition is indicated.
 *******************************************************************************/
#define SLSI_PSID_DOT11_LONG_RETRY_LIMIT 0x007B

/*******************************************************************************
 * NAME          : Dot11FragmentationThreshold
 * PSID          : 124 (0x007C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 256
 * MAX           : 11500
 * DEFAULT       : 3000
 * DESCRIPTION   :
 *  This attribute specifies the current maximum size, in octets, of the MPDU
 *  that may be delivered to the security encapsulation. This maximum size
 *  does not apply when an MSDU is transmitted using an HT-immediate or
 *  HTdelayed Block Ack agreement, or when an MSDU or MMPDU is carried in an
 *  AMPDU that does not contain a VHT single MPDU. Fields added to the frame
 *  by security encapsulation are not counted against the limit specified by
 *  this attribute. Except as described above, an MSDU or MMPDU is fragmented
 *  when the resulting frame has an individual address in the Address1 field,
 *  and the length of the frame is larger than this threshold, excluding
 *  security encapsulation fields. The default value for this attribute is
 *  the lesser of 11500 or the aMPDUMaxLength or the aPSDUMaxLength of the
 *  attached PHY and the value never exceeds the lesser of 11500 or the
 *  aMPDUMaxLength or the aPSDUMaxLength of the attached PHY.
 *******************************************************************************/
#define SLSI_PSID_DOT11_FRAGMENTATION_THRESHOLD 0x007C

/*******************************************************************************
 * NAME          : Dot11RtsSuccessCount
 * PSID          : 146 (0x0092)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  This counter shall increment when a CTS is received in response to an
 *  RTS.
 *******************************************************************************/
#define SLSI_PSID_DOT11_RTS_SUCCESS_COUNT 0x0092

/*******************************************************************************
 * NAME          : Dot11MulticastReceivedFrameCount
 * PSID          : 150 (0x0096)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  This counter shall increment when a MSDU is received with the multicast
 *  bit set in the destination MAC address.
 *******************************************************************************/
#define SLSI_PSID_DOT11_MULTICAST_RECEIVED_FRAME_COUNT 0x0096

/*******************************************************************************
 * NAME          : Dot11FcsErrorCount
 * PSID          : 151 (0x0097)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : -9223372036854775808
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  This counter shall increment when an FCS error is detected in a received
 *  MPDU.
 *******************************************************************************/
#define SLSI_PSID_DOT11_FCS_ERROR_COUNT 0x0097

/*******************************************************************************
 * NAME          : Dot11WepUndecryptableCount
 * PSID          : 153 (0x0099)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  This counter shall increment when a frame is received with the WEP
 *  subfield of the Frame Control field set to one and the WEPOn value for
 *  the key mapped to the transmitter&apos;s MAC address indicates that the
 *  frame should not have been encrypted or that frame is discarded due to
 *  the receiving STA not implementing the privacy option.
 *******************************************************************************/
#define SLSI_PSID_DOT11_WEP_UNDECRYPTABLE_COUNT 0x0099

/*******************************************************************************
 * NAME          : Dot11ManufacturerProductVersion
 * PSID          : 183 (0x00B7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 400
 * DEFAULT       :
 * DESCRIPTION   :
 *  Printable string used to identify the manufacturer&apos;s product version
 *  of the resource.
 *******************************************************************************/
#define SLSI_PSID_DOT11_MANUFACTURER_PRODUCT_VERSION 0x00B7

/*******************************************************************************
 * NAME          : Dot11RsnaStatsStaAddress
 * PSID          : 430 (0x01AE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  The MAC address of the STA to which the statistics in this conceptual row
 *  belong.
 *******************************************************************************/
#define SLSI_PSID_DOT11_RSNA_STATS_STA_ADDRESS 0x01AE

/*******************************************************************************
 * NAME          : Dot11RsnaStatsTkipicvErrors
 * PSID          : 433 (0x01B1)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Counts the number of TKIP ICV errors encountered when decrypting packets
 *  for the STA.
 *******************************************************************************/
#define SLSI_PSID_DOT11_RSNA_STATS_TKIPICV_ERRORS 0x01B1

/*******************************************************************************
 * NAME          : Dot11RsnaStatsTkipLocalMicFailures
 * PSID          : 434 (0x01B2)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Counts the number of MIC failures encountered when checking the integrity
 *  of packets received from the STA at this entity.
 *******************************************************************************/
#define SLSI_PSID_DOT11_RSNA_STATS_TKIP_LOCAL_MIC_FAILURES 0x01B2

/*******************************************************************************
 * NAME          : Dot11RsnaStatsTkipRemoteMicFailures
 * PSID          : 435 (0x01B3)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Counts the number of MIC failures encountered by the STA identified by
 *  dot11RSNAStatsSTAAddress and reported back to this entity.
 *******************************************************************************/
#define SLSI_PSID_DOT11_RSNA_STATS_TKIP_REMOTE_MIC_FAILURES 0x01B3

/*******************************************************************************
 * NAME          : Dot11RsnaStatsCcmpReplays
 * PSID          : 436 (0x01B4)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  The number of received CCMP MPDUs discarded by the replay mechanism.
 *******************************************************************************/
#define SLSI_PSID_DOT11_RSNA_STATS_CCMP_REPLAYS 0x01B4

/*******************************************************************************
 * NAME          : Dot11RsnaStatsCcmpDecryptErrors
 * PSID          : 437 (0x01B5)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  The number of received MPDUs discarded by the CCMP decryption algorithm.
 *******************************************************************************/
#define SLSI_PSID_DOT11_RSNA_STATS_CCMP_DECRYPT_ERRORS 0x01B5

/*******************************************************************************
 * NAME          : Dot11RsnaStatsTkipReplays
 * PSID          : 438 (0x01B6)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Counts the number of TKIP replay errors detected.
 *******************************************************************************/
#define SLSI_PSID_DOT11_RSNA_STATS_TKIP_REPLAYS 0x01B6

/*******************************************************************************
 * NAME          : Dot11RsnaStatsRobustMgmtCcmpReplays
 * PSID          : 441 (0x01B9)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  The number of received Robust Management frame MPDUs discarded due to
 *  CCMP replay errors
 *******************************************************************************/
#define SLSI_PSID_DOT11_RSNA_STATS_ROBUST_MGMT_CCMP_REPLAYS 0x01B9

/*******************************************************************************
 * NAME          : UnifiMlmeConnectionTimeout
 * PSID          : 2000 (0x07D0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : milliseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 3000
 * DESCRIPTION   :
 *  Firmware waits for unifiMLMEConnectionTimeOut of no successful Tx/Rx
 *  (including beacon) to/from AP before it disconnects from AP.For STA case
 *  - Setting it to less than 3 seconds may result in frequent disconnection
 *  with the AP
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_CONNECTION_TIMEOUT 0x07D0

/*******************************************************************************
 * NAME          : UnifiMlmeScanChannelMaxScanTime
 * PSID          : 2001 (0x07D1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 14
 * MAX           : 14
 * DEFAULT       : { 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00 }
 * DESCRIPTION   :
 *  For testing: overrides max_scan_time. 0 indicates not used.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_CHANNEL_MAX_SCAN_TIME 0x07D1

/*******************************************************************************
 * NAME          : UnifiMlmeScanChannelProbeInterval
 * PSID          : 2002 (0x07D2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 14
 * MAX           : 14
 * DEFAULT       : { 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00 }
 * DESCRIPTION   :
 *  For testing: overrides probe interval. 0 indicates not used.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_CHANNEL_PROBE_INTERVAL 0x07D2

/*******************************************************************************
 * NAME          : UnifiMlmeDataReferenceTimeout
 * PSID          : 2005 (0x07D5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 65534
 * DEFAULT       :
 * DESCRIPTION   :
 *  This attribute specifies the maximum time allowed for the data in data
 *  references corresponding to MLME primitives to be made available to the
 *  firmware. The special value 0 specifies an infinite timeout.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_DATA_REFERENCE_TIMEOUT 0x07D5

/*******************************************************************************
 * NAME          : UnifiMlmeScanProbeInterval
 * PSID          : 2007 (0x07D7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 70
 * DESCRIPTION   :
 *  This attribute specifies the time between transmissions of broadcast
 *  probe requests on a given channel when performing an active scan.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_PROBE_INTERVAL 0x07D7

/*******************************************************************************
 * NAME          : UnifiMlmeScanHighRssiThreshold
 * PSID          : 2008 (0x07D8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : dBm
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       : -100
 * DESCRIPTION   :
 *  This attribute specifies the minimum RSSI necessary for a new station to
 *  enter the coverage area of scan.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_HIGH_RSSI_THRESHOLD 0x07D8

/*******************************************************************************
 * NAME          : UnifiMlmeScanDeltaRssiThreshold
 * PSID          : 2010 (0x07DA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : dB
 * MIN           : 1
 * MAX           : 255
 * DEFAULT       : 20
 * DESCRIPTION   :
 *  This attribute specifies the magnitude of the change in RSSI for which a
 *  scan result will be issued
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_DELTA_RSSI_THRESHOLD 0x07DA

/*******************************************************************************
 * NAME          : UnifiMlmeScanHighSnrThreshold
 * PSID          : 2011 (0x07DB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : dB
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       : -100
 * DESCRIPTION   :
 *  This attribute specifies the minimum SNR necessary for a new station to
 *  enter the coverage area of scan.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_HIGH_SNR_THRESHOLD 0x07DB

/*******************************************************************************
 * NAME          : UnifiMlmeScanDeltaSnrThreshold
 * PSID          : 2013 (0x07DD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : dB
 * MIN           : 1
 * MAX           : 255
 * DEFAULT       : 255
 * DESCRIPTION   :
 *  This attribute specifies the magnitude of the change in SNR for a station
 *  in scan for which a scan result will be issued
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_DELTA_SNR_THRESHOLD 0x07DD

/*******************************************************************************
 * NAME          : UnifiMlmeScanMaximumAge
 * PSID          : 2014 (0x07DE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : s
 * MIN           : 1
 * MAX           : 2147
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  Not supported
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_MAXIMUM_AGE 0x07DE

/*******************************************************************************
 * NAME          : UnifiMlmeScanMaximumResults
 * PSID          : 2015 (0x07DF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 40
 * DESCRIPTION   :
 *  This attribute specifies the maximum number of scan results (for all
 *  scans) which will be stored before the oldest result is discarded,
 *  irrespective of its age. The value 0 specifies no maximum.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_MAXIMUM_RESULTS 0x07DF

/*******************************************************************************
 * NAME          : UnifiMlmeAutonomousScanNoisy
 * PSID          : 2016 (0x07E0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Not supported
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_AUTONOMOUS_SCAN_NOISY 0x07E0

/*******************************************************************************
 * NAME          : UnifiFirmwareBuildId
 * PSID          : 2021 (0x07E5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Numeric build identifier for this firmware build. This should normally be
 *  displayed in decimal. The textual build identifier is available via the
 *  standard dot11manufacturerProductVersion MIB attribute.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FIRMWARE_BUILD_ID 0x07E5

/*******************************************************************************
 * NAME          : UnifiChipVersion
 * PSID          : 2022 (0x07E6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Numeric identifier for the UniFi silicon revision (as returned by the
 *  GBL_CHIP_VERSION hardware register). Other than being different for each
 *  design variant (but not for alternative packaging options), the
 *  particular values returned do not have any significance.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CHIP_VERSION 0x07E6

/*******************************************************************************
 * NAME          : UnifiFirmwarePatchBuildId
 * PSID          : 2023 (0x07E7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Numeric build identifier for the patch set that has been applied to this
 *  firmware image. This should normally be displayed in decimal. For a
 *  patched ROM build there will be two build identifiers, the first will
 *  correspond to the base ROM image, the second will correspond to the patch
 *  set that has been applied.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FIRMWARE_PATCH_BUILD_ID 0x07E7

/*******************************************************************************
 * NAME          : UnifiBasicCapabilities
 * PSID          : 2030 (0x07EE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0X0730
 * DESCRIPTION   :
 *  This MIB variable indicates basis capabilities of the chip. The 16-bit
 *  field follows the coding of IEEE 802.11 Capability Information.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BASIC_CAPABILITIES 0x07EE

/*******************************************************************************
 * NAME          : UnifiExtendedCapabilities
 * PSID          : 2031 (0x07EF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 9
 * MAX           : 9
 * DEFAULT       : { 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00 }
 * DESCRIPTION   :
 *  This MIB variable indicates extended capabilities of the chip. Bit field
 *  definition and coding follows IEEE 802.11 Extended Capability Information
 *  Element, with spare subfields for capabilities that are independent from
 *  chip/firmware implementation.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_EXTENDED_CAPABILITIES 0x07EF

/*******************************************************************************
 * NAME          : UnifiHtCapabilities
 * PSID          : 2032 (0x07F0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 21
 * MAX           : 21
 * DEFAULT       : { 0X7F, 0X09, 0X17, 0XFF, 0X00, 0X00, 0X00, 0X01, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00 }
 * DESCRIPTION   :
 *  This MIB variable indicates the HT capabilities of the chip. See
 *  SC-503520-SP for further details.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_HT_CAPABILITIES 0x07F0

/*******************************************************************************
 * NAME          : UnifiRsnCapabilities
 * PSID          : 2034 (0x07F2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0X248C
 * DESCRIPTION   :
 *  This MIB variable encodes the RSN Capabilities field of IEEE 802.11 RSN
 *  Information Element.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RSN_CAPABILITIES 0x07F2

/*******************************************************************************
 * NAME          : Unifi24G40MhzChannels
 * PSID          : 2035 (0x07F3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  This attribute, when TRUE, enables 40Mz wide channels in the 2.4G band
 *******************************************************************************/
#define SLSI_PSID_UNIFI24_G40_MHZ_CHANNELS 0x07F3

/*******************************************************************************
 * NAME          : UnifiSupportedDataRates
 * PSID          : 2041 (0x07F9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * UNITS         : 500 kbps
 * MIN           : 2
 * MAX           : 16
 * DEFAULT       : { 0X02, 0X04, 0X0B, 0X0C, 0X12, 0X16, 0X18, 0X24, 0X30, 0X48, 0X60, 0X6C }
 * DESCRIPTION   :
 *  Defines the supported non-HT data rates. It is encoded as N+1 octets
 *  where the first octet is N and the subsequent octets each describe a
 *  single supported rate.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SUPPORTED_DATA_RATES 0x07F9

/*******************************************************************************
 * NAME          : UnifiRadioMeasurementActivated
 * PSID          : 2043 (0x07FB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  When TRUE Radio Measurements are supported. The capability is disabled
 *  otherwise.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_MEASUREMENT_ACTIVATED 0x07FB

/*******************************************************************************
 * NAME          : UnifiRadioMeasurementCapabilities
 * PSID          : 2044 (0x07FC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 5
 * MAX           : 5
 * DEFAULT       : { 0X71, 0X00, 0X00, 0X00, 0X00 }
 * DESCRIPTION   :
 *  This MIB variable indicates the RM Enabled capabilities of the chip. See
 *  SC-503520-SP for further details.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_MEASUREMENT_CAPABILITIES 0x07FC

/*******************************************************************************
 * NAME          : UnifiVhtActivated
 * PSID          : 2045 (0x07FD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  This attribute, when TRUE, indicates that use VHT mode. The capability is
 *  disabled otherwise.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_VHT_ACTIVATED 0x07FD

/*******************************************************************************
 * NAME          : UnifiHtActivated
 * PSID          : 2046 (0x07FE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  This attribute, when TRUE, indicates that use HT mode. The capability is
 *  disabled otherwise.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_HT_ACTIVATED 0x07FE

/*******************************************************************************
 * NAME          : UnifiRoamingEnabled
 * PSID          : 2049 (0x0801)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  The Enable MIB location for the Roaming functionality.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAMING_ENABLED 0x0801

/*******************************************************************************
 * NAME          : UnifiRssiRoamScanTrigger
 * PSID          : 2050 (0x0802)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : dBm
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       : -75
 * DESCRIPTION   :
 *  The current RSSI value below which roaming scan shall start
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RSSI_ROAM_SCAN_TRIGGER 0x0802

/*******************************************************************************
 * NAME          : UnifiRssiRoamDeltaTrigger
 * PSID          : 2051 (0x0803)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : dBm
 * MIN           : 1
 * MAX           : 255
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  The RSSI on the target AP must be greater than the current AP RSSI by
 *  that value to be oaming candidate
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RSSI_ROAM_DELTA_TRIGGER 0x0803

/*******************************************************************************
 * NAME          : UnifiCachedChannelScanPeriod
 * PSID          : 2052 (0x0804)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 1
 * MAX           : 4294967295
 * DEFAULT       : 20000000
 * DESCRIPTION   :
 *  The scan period for cached channels background roaming (microseconds)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CACHED_CHANNEL_SCAN_PERIOD 0x0804

/*******************************************************************************
 * NAME          : UnifiFullRoamScanPeriod
 * PSID          : 2053 (0x0805)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 1
 * MAX           : 4294967295
 * DEFAULT       : 30000000
 * DESCRIPTION   :
 *  DO NOT REMOVE. Although not used in the code, required to pass OKC test
 *  2.7 and 2.8.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FULL_ROAM_SCAN_PERIOD 0x0805

/*******************************************************************************
 * NAME          : UnifiRoamScanBand
 * PSID          : 2055 (0x0807)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 2
 * DEFAULT       : 2
 * DESCRIPTION   :
 *  Indicates whether only intra-band or all-band should be used for roaming
 *  scan. 2 - Roaming across band 1 - Roaming within band
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_SCAN_BAND 0x0807

/*******************************************************************************
 * NAME          : UnifiRoamScanMaxActiveChannelTime
 * PSID          : 2057 (0x0809)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 65535
 * DEFAULT       : 120
 * DESCRIPTION   :
 *  NCHO channel time. Name confusion for Host compatibility.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_SCAN_MAX_ACTIVE_CHANNEL_TIME 0x0809

/*******************************************************************************
 * NAME          : UnifiRoamMode
 * PSID          : 2060 (0x080C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 2
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  Enable/Disable host resume when roaming. 0: Wake up the host all the
 *  time. 1: Only wakeup the host if the AP is not white-listed. 2: Don't
 *  wake up the host.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_MODE 0x080C

/*******************************************************************************
 * NAME          : UnifiRoamOkcEnable
 * PSID          : 2061 (0x080D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  If set to false, On-Chip OKC is disabled.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_OKC_ENABLE 0x080D

/*******************************************************************************
 * NAME          : UnifiRoamScanBackgroundPeriod
 * PSID          : 2063 (0x080F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 1
 * MAX           : 4294967295
 * DEFAULT       : 30000000
 * DESCRIPTION   :
 *  Deprecated by unifiCachedChannelScanPeriod.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_SCAN_BACKGROUND_PERIOD 0x080F

/*******************************************************************************
 * NAME          : UnifiRssiRoamScanNoCandidateDeltaTrigger
 * PSID          : 2064 (0x0810)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : dBm
 * MIN           : 1
 * MAX           : 255
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  The value by which unifiRssiRoamScanTrigger is lowered when no roaming
 *  candidates are found
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RSSI_ROAM_SCAN_NO_CANDIDATE_DELTA_TRIGGER 0x0810

/*******************************************************************************
 * NAME          : UnifiRoamEapTimeout
 * PSID          : 2065 (0x0811)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : ms
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 200
 * DESCRIPTION   :
 *  Timeout for receiving the first EAP/EAPOL frame from the AP during
 *  roaming
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_EAP_TIMEOUT 0x0811

/*******************************************************************************
 * NAME          : UnifiRoamScanControl
 * PSID          : 2067 (0x0813)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  NCHO Roam Scan Control.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_SCAN_CONTROL 0x0813

/*******************************************************************************
 * NAME          : UnifiDfsScanMode
 * PSID          : 2068 (0x0814)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 2
 * DEFAULT       :
 * DESCRIPTION   :
 *  Scan DFS Mode. 0: DFS scan disabled 0: DFS roaming scan disabled. 1: DFS
 *  scan enabled. (passive scanning on DFS channels) 1: DFS roaming scan
 *  enabled. Normal mode. i.e. passive scanning on DFS channels (Default) 2:
 *  DFS scan enabled. (passive scanning on DFS channels) 2: DFS roaming scan
 *  enabled with active scanning on channel list supplied with
 *  MLME-SET-CACHED-CHANNELS.request
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DFS_SCAN_MODE 0x0814

/*******************************************************************************
 * NAME          : UnifiRoamScanHomeTime
 * PSID          : 2069 (0x0815)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 45
 * DESCRIPTION   :
 *  The maximum time to spend scanning before pausing for the
 *  unifiRoamScanHomeAwayTime, default of 0 mean has no specific requirement
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_SCAN_HOME_TIME 0x0815

/*******************************************************************************
 * NAME          : UnifiRoamScanHomeAwayTime
 * PSID          : 2070 (0x0816)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 100
 * DESCRIPTION   :
 *  The time to spend NOT scanning after scanning for
 *  unifiRoamScanHomeTime,default of 0 mean has no specific requirement
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_SCAN_HOME_AWAY_TIME 0x0816

/*******************************************************************************
 * NAME          : UnifiRoamScanNProbe
 * PSID          : 2072 (0x0818)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 2
 * DESCRIPTION   :
 *  The Number of ProbeReq per channel for the Roaming Scan.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAM_SCAN_NPROBE 0x0818

/*******************************************************************************
 * NAME          : UnifiVS2RoamingCount
 * PSID          : 2073 (0x0819)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Number of Roams since Connect or last set to 0. (CCX Voice Services:
 *  Roaming Count)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_VS2_ROAMING_COUNT 0x0819

/*******************************************************************************
 * NAME          : UnifiVS2RoamingDelay
 * PSID          : 2074 (0x081A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : ms
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  milliseconds taken for last roam (CCX Voice Services: Roaming Delay)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_VS2_ROAMING_DELAY 0x081A

/*******************************************************************************
 * NAME          : UnifiApOlbcDuration
 * PSID          : 2076 (0x081C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : milliseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 300
 * DESCRIPTION   :
 *  How long the AP enables reception of BEACON frames to perform Overlapping
 *  Legacy BSS Condition(OLBC). If set to 0 then OLBC is disabled.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_AP_OLBC_DURATION 0x081C

/*******************************************************************************
 * NAME          : UnifiApOlbcInterval
 * PSID          : 2077 (0x081D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : milliseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 2000
 * DESCRIPTION   :
 *  How long between periods of receiving BEACON frames to perform
 *  Overlapping Legacy BSS Condition(OLBC). This value MUST exceed the OBLC
 *  duration MIB unifiApOlbcDuration. If set to 0 then OLBC is disabled.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_AP_OLBC_INTERVAL 0x081D

/*******************************************************************************
 * NAME          : UnifiFrameResponseTimeout
 * PSID          : 2080 (0x0820)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 500
 * DEFAULT       : 200
 * DESCRIPTION   :
 *  How long to wait for a frame (Auth, Assoc, ReAssoc) after Rame replies to
 *  a send frame request
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FRAME_RESPONSE_TIMEOUT 0x0820

/*******************************************************************************
 * NAME          : UnifiConnectionFailureTimeout
 * PSID          : 2081 (0x0821)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 4000
 * DEFAULT       : 1500
 * DESCRIPTION   :
 *  How long the complete connection procedure has before the MLME times out
 *  and issues a Connect Indication (fail).
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CONNECTION_FAILURE_TIMEOUT 0x0821

/*******************************************************************************
 * NAME          : UnifiConnectingProbeTimeout
 * PSID          : 2082 (0x0822)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  How long to wait for a ProbeRsp when syncronising before resending a
 *  ProbeReq
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CONNECTING_PROBE_TIMEOUT 0x0822

/*******************************************************************************
 * NAME          : UnifiDisconnectTimeout
 * PSID          : 2083 (0x0823)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : seconds
 * MIN           : 0
 * MAX           : 3
 * DEFAULT       : 2
 * DESCRIPTION   :
 *  How long the firmware attempts to perform a disconnect (triggered by
 *  MLME_DISCONNECT-REQ) before responding with MLME-DISCONNECT-IND and
 *  aborting the disconnection attempt. This is particulary important when a
 *  SoftAP is attempting to disconnect associated stations which might have
 *  "silently" left the ESS.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DISCONNECT_TIMEOUT 0x0823

/*******************************************************************************
 * NAME          : UnifiFrameResponseCfmTxLifetimeTimeout
 * PSID          : 2084 (0x0824)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  How long to wait to retry a frame (Auth, Assoc, ReAssoc) after TX Cfm
 *  trasnmission_status = TxLifetime.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FRAME_RESPONSE_CFM_TX_LIFETIME_TIMEOUT 0x0824

/*******************************************************************************
 * NAME          : UnifiFrameResponseCfmFailureTimeout
 * PSID          : 2085 (0x0825)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 40
 * DESCRIPTION   :
 *  How long to wait to retry a frame (Auth, Assoc, ReAssoc) after TX Cfm
 *  trasnmission_status != Successful | TxLifetime.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FRAME_RESPONSE_CFM_FAILURE_TIMEOUT 0x0825

/*******************************************************************************
 * NAME          : UnifiDisconnectAllTimeout
 * PSID          : 2086 (0x0826)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : milliseconds
 * MIN           : 0
 * MAX           : 2000
 * DEFAULT       : 500
 * DESCRIPTION   :
 *  How long the firmware attempts to perform a disconnect all STAs
 *  (triggered by MLME_DISCONNECT-REQ 00:00:00:00:00:00) before responding
 *  with MLME-DISCONNECT-IND and aborting the disconnection attempt. This is
 *  particulary important when a SoftAP is attempting to disconnect all
 *  associated stations which might have "silently" left the ESS.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DISCONNECT_ALL_TIMEOUT 0x0826

/*******************************************************************************
 * NAME          : UnifiMlmeScanMaxNumberOfProbeSets
 * PSID          : 2087 (0x0827)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 2
 * DESCRIPTION   :
 *  Max number of Probe Request sets that the scan engine will send on a
 *  single channel.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_MAX_NUMBER_OF_PROBE_SETS 0x0827

/*******************************************************************************
 * NAME          : UnifiMlmeScanStopIfLessThanXFrames
 * PSID          : 2088 (0x0828)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 4
 * DESCRIPTION   :
 *  Stop scanning on a channel if less than X Beacons or Probe Responses are
 *  received.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MLME_SCAN_STOP_IF_LESS_THAN_XFRAMES 0x0828

/*******************************************************************************
 * NAME          : UnifiRoamingTriggerTime
 * PSID          : 2090 (0x082A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : us
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Timestamp of last roam tigger. Timestamp of any trigger for roaming.
 *  Caused by Link loss, Rssi, mlme_roam_req etc
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAMING_TRIGGER_TIME 0x082A

/*******************************************************************************
 * NAME          : UnifiRoamingStartTime
 * PSID          : 2091 (0x082B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : us
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Timestamp of last roam start. Start of a connection attempt to an AP
 *  (Starts at Dataplane Pause)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAMING_START_TIME 0x082B

/*******************************************************************************
 * NAME          : UnifiRoamingOnchipEndTime
 * PSID          : 2092 (0x082C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : us
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Timestamp of last roam end for the onchip portion of the roam.
 *  mlme_roamed_ind to Host
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAMING_ONCHIP_END_TIME 0x082C

/*******************************************************************************
 * NAME          : UnifiRoamingEndTime
 * PSID          : 2093 (0x082D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : us
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Timestamp of last roam end. Keys installed and Dataplane unpaused.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ROAMING_END_TIME 0x082D

/*******************************************************************************
 * NAME          : UnifiPeerAverageTxDataRate
 * PSID          : 2096 (0x0830)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  The average tx rate that are used for transmissions since this entry was
 *  last read;
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PEER_AVERAGE_TX_DATA_RATE 0x0830

/*******************************************************************************
 * NAME          : UnifiPeerRssi
 * PSID          : 2097 (0x0831)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       :
 * DESCRIPTION   :
 *  This attribute provides a running average of the Received Signal Strength
 *  Indication (RSSI) for packets received from the peer. The value is only
 *  an indication of the signal strength; it is not an accurate measurement.
 *  The table will be reset when UniFi joins or starts a BSS or is reset. An
 *  entry is reset when the corresponding peer station record is deleted.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PEER_RSSI 0x0831

/*******************************************************************************
 * NAME          : UnifiUartConfigure
 * PSID          : 2110 (0x083E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  An MLME-SET.request of this attribute causes the UART to be configured
 *  using the values of the other unifiUart* attributes. The value supplied
 *  for this attribute is ignored.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_UART_CONFIGURE 0x083E

/*******************************************************************************
 * NAME          : UnifiUartPios
 * PSID          : 2111 (0x083F)
 * PER INTERFACE?: NO
 * TYPE          : unifiUartPios
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Specification of which PIOs should be connected to the UART. Currently
 *  defined values are: 1 - UART not used; all PIOs are available for other
 *  uses. 2 - Data transmit and receive connected to PIO[12] and PIO[14]
 *  respectively. No hardware handshaking lines. 3 - Data and handshaking
 *  lines connected to PIO[12:15].
 *******************************************************************************/
#define SLSI_PSID_UNIFI_UART_PIOS 0x083F

/*******************************************************************************
 * NAME          : UnifiClockFrequency
 * PSID          : 2140 (0x085C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : kHz
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  This attribute can be used to query the nominal frequency of the external
 *  clock source or crystal oscillator used by UniFi. The clock frequency is
 *  a system parameter and can not be modified by this MIB key.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CLOCK_FREQUENCY 0x085C

/*******************************************************************************
 * NAME          : UnifiCrystalFrequencyTrim
 * PSID          : 2141 (0x085D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 63
 * DEFAULT       : 31
 * DESCRIPTION   :
 *  The IEEE 802.11 standard requires a frequency accuracy of either +/- 20
 *  ppm or +/- 25 ppm depending on the physical layer being used. If
 *  UniFi&apos;s frequency reference is a crystal then this attribute should
 *  be used to tweak the oscillating frequency to compensate for design- or
 *  device-specific variations. Each step change trims the frequency by
 *  approximately 2 ppm.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CRYSTAL_FREQUENCY_TRIM 0x085D

/*******************************************************************************
 * NAME          : UnifiEnableDorm
 * PSID          : 2142 (0x085E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Dorm/deep sleep can be permanently disallowed by setting the value to
 *  FALSE. When the value is FALSE, WLAN will not switch the radio power
 *  domain on/off *and* it will always veto deep sleep. Setting the value to
 *  TRUE means dorm functionality will behave normally. The intention is
 * not* for this value to be changed at runtime.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ENABLE_DORM 0x085E

/*******************************************************************************
 * NAME          : UnifiDisableDormWhenBtOn
 * PSID          : 2143 (0x085F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Dorm/deep sleep would be dynamically disabled when BT is turned ON if the
 *  value is TRUE, even though unifiEnableDorm is TRUE. For more details,
 *  take a look at SSB-17864.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DISABLE_DORM_WHEN_BT_ON 0x085F

/*******************************************************************************
 * NAME          : UnifiExternalClockDetect
 * PSID          : 2146 (0x0862)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  If UniFi is running with an external fast clock source, i.e.
 *  unifiExternalFastClockRequest is set, it is common for this clock to be
 *  shared with other devices. Setting this attribute to true causes UniFi to
 *  detect when the clock is present (presumably in response to a request
 *  from another device), and to perform any pending activities at that time
 *  rather than requesting the clock again some time later. This is likely to
 *  reduce overall system power consumption by reducing the total time that
 *  the clock needs to be active.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_EXTERNAL_CLOCK_DETECT 0x0862

/*******************************************************************************
 * NAME          : UnifiAnaIoSettingEnum
 * PSID          : 2148 (0x0864)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  This counter shall increment when an ACK is not received when expected.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ANA_IO_SETTING_ENUM 0x0864

/*******************************************************************************
 * NAME          : UnifiExternalFastClockRequest
 * PSID          : 2149 (0x0865)
 * PER INTERFACE?: NO
 * TYPE          : unifiExternalFastClockRequest
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  It is possible to supply UniFi with an external fast reference clock, as
 *  an alternative to using a crystal. If such a clock is used then it is
 *  only required when UniFi is active. A signal can be output on PIO[2] or
 *  if the version of UniFi in use is the UF602x or later, any PIO may be
 *  used (see unifiExternalFastClockRequestPIO) to indicate when UniFi
 *  requires a fast clock. Setting this attribute makes this signal become
 *  active and determines the type of signal output. 0 - No clock request. 1
 *  - Non inverted, totem pole. 2 - Inverted, totem pole. 3 - Open drain. 4 -
 *  Open source.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_EXTERNAL_FAST_CLOCK_REQUEST 0x0865

/*******************************************************************************
 * NAME          : UnifiWatchdogTimeout
 * PSID          : 2152 (0x0868)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : ms
 * MIN           : 1
 * MAX           : 65535
 * DEFAULT       : 1500
 * DESCRIPTION   :
 *  This attribute specifies the maximum time the background may be busy or
 *  locked out for. If this time is exceeded, UniFi will reset. If this key
 *  is set to 65535 then the watchdog will be disabled.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_WATCHDOG_TIMEOUT 0x0868

/*******************************************************************************
 * NAME          : UnifiScanParameters
 * PSID          : 2154 (0x086A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 18
 * MAX           : 18
 * DEFAULT       :
 * DESCRIPTION   :
 *  Scan parameters. Each row of the table contains 2 entries for a scan:
 *  first entry when there is 0 registered VIFs, second - when there is 1 or
 *  more registered VIFs. Entry has the following structure: octet 0 - Scan
 *  priority (uint8_t) octet 1 - Enable Early Channel Exit (uint8_t as bool)
 *  octet 2 ~ 3 - Probe Interval in Time Units (uint16_t) octet 4 ~ 5 - Max
 *  Active Channel Time in Time Units (uint16_t) octet 6 ~ 7 - Max Passive
 *  Channel Time in Time Units (uint16_t) octet 8 ~ 9 - Scan Policy
 *  (uint16_t) Size of each entry is 10 octets, row size is 20 octets. A Time
 *  Units value specifies a time interval as a multiple of TU (1024 us).
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SCAN_PARAMETERS 0x086A

/*******************************************************************************
 * NAME          : UnifiExternalFastClockRequestPio
 * PSID          : 2158 (0x086E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 15
 * DEFAULT       : 9
 * DESCRIPTION   :
 *  If an external fast reference clock is being supplied to UniFi as an
 *  alternative to a crystal (see unifiExternalFastClockRequest) and the
 *  version of UniFi in use is the UF602x or later, any PIO may be used as
 *  the external fast clock request output from UniFi. This MIB key
 *  determines the PIO to use.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_EXTERNAL_FAST_CLOCK_REQUEST_PIO 0x086E

/*******************************************************************************
 * NAME          : UnifiRssi
 * PSID          : 2200 (0x0898)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : dBm
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       :
 * DESCRIPTION   :
 *  This attribute provides a running average of the Received Signal Strength
 *  Indication (RSSI) for packets received by UniFi&apos;s radio. The value
 *  should only be treated as an indication of the signal strength; it is not
 *  an accurate measurement. The result is only meaningful if the
 *  unifiRxExternalGain attribute is set to the correct calibration value. If
 *  UniFi is part of a BSS, only frames originating from devices in the BSS
 *  are reported (so far as this can be determined). The average is reset
 *  when UniFi joins or starts a BSS or is reset.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RSSI 0x0898

/*******************************************************************************
 * NAME          : UnifiSnr
 * PSID          : 2202 (0x089A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : dB
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       :
 * DESCRIPTION   :
 *  This attribute provides a running average of the Signal to Noise Ratio
 *  (SNR) for packets received by UniFi&apos;s radio.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SNR 0x089A

/*******************************************************************************
 * NAME          : UnifiSwTxTimeout
 * PSID          : 2204 (0x089C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : seconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  This MIB sets the maximum time in seconds for a frame to be queued in
 *  firmware, ready to be sent, but not yet actually pumped to hardware.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SW_TX_TIMEOUT 0x089C

/*******************************************************************************
 * NAME          : UnifiHwTxTimeout
 * PSID          : 2205 (0x089D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : milliseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 512
 * DESCRIPTION   :
 *  This MIB sets the maximum time in milliseconds for a frame to be queued
 *  in the hardware/DPIF.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_HW_TX_TIMEOUT 0x089D

/*******************************************************************************
 * NAME          : UnifiRateStatsRxSuccessCount
 * PSID          : 2206 (0x089E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  The number of successful receptions of complete management and data
 *  frames at the rate indexed by unifiRateStatsIndex.This number will wrap
 *  to zero after the range is exceeded.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RATE_STATS_RX_SUCCESS_COUNT 0x089E

/*******************************************************************************
 * NAME          : UnifiRateStatsTxSuccessCount
 * PSID          : 2207 (0x089F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  The number of successful (acknowledged) unicast transmissions of complete
 *  data or management frames the rate indexed by unifiRateStatsIndex. This
 *  number will wrap to zero after the range is exceeded.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RATE_STATS_TX_SUCCESS_COUNT 0x089F

/*******************************************************************************
 * NAME          : UnifiTxDataRate
 * PSID          : 2208 (0x08A0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  The bit rate currently in use for transmissions of unicast data frames;
 *  On an infrastructure BSS, this is the data rate used in communicating
 *  with the associated access point, if there is none, an error is returned
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_DATA_RATE 0x08A0

/*******************************************************************************
 * NAME          : UnifiSnrExtraOffsetCck
 * PSID          : 2209 (0x08A1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : dB
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       : 8
 * DESCRIPTION   :
 *  This offset is added to SNR values received at 802.11b data rates. This
 *  accounts for differences in the RF pathway between 802.11b and 802.11g
 *  demodulators. The offset applies to values of unifiSNR as well as SNR
 *  values in scan indications. This attribute is not used in 5GHz mode.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SNR_EXTRA_OFFSET_CCK 0x08A1

/*******************************************************************************
 * NAME          : UnifiRssiMaxAveragingPeriod
 * PSID          : 2210 (0x08A2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 1
 * MAX           : 65535
 * DEFAULT       : 1024
 * DESCRIPTION   :
 *  This attribute limits the period over which the value of unifiRSSI is
 *  averaged. If no more than unifiRSSIMinReceivedFrames frames have been
 *  received in the period, then the value of unifiRSSI is reset to the value
 *  of the next measurement and the rolling average is restarted. This
 *  ensures that the value is timely (although possibly poorly averaged) when
 *  little data is being received.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RSSI_MAX_AVERAGING_PERIOD 0x08A2

/*******************************************************************************
 * NAME          : UnifiRssiMinReceivedFrames
 * PSID          : 2211 (0x08A3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 65535
 * DEFAULT       : 2
 * DESCRIPTION   :
 *  See the description of unifiRSSIMaxAveragingPeriod for how the
 *  combination of attributes is used.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RSSI_MIN_RECEIVED_FRAMES 0x08A3

/*******************************************************************************
 * NAME          : UnifiRateStatsRate
 * PSID          : 2212 (0x08A4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : 500 kbps
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  The rate corresponding to the current table entry. The value is rounded
 *  to the nearest number of units where necessary. Most rates do not require
 *  rounding, but when short guard interval is in effect the rates are no
 *  longer multiples of the base unit. Note that there may be two occurrences
 *  of the value 130: the first corresponds to MCS index 7, and the second,
 *  if present, to MCS index 6 with short guard interval.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RATE_STATS_RATE 0x08A4

/*******************************************************************************
 * NAME          : UnifiDiscardedFrameCount
 * PSID          : 2214 (0x08A6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  This is a counter that indicates the number of data and management frames
 *  that have been processed by the UniFi hardware but were discarded before
 *  being processed by the firmware. It does not include frames not processed
 *  by the hardware because they were not addressed to the local device, nor
 *  does it include frames discarded by the firmware in the course of normal
 *  MAC processing (which include, for example, frames in an appropriate
 *  encryption state and multicast frames not requested by the host).
 *  Typically this counter indicates lost data frames for which there was no
 *  buffer space; however, other cases may cause the counter to increment,
 *  such as receiving a retransmitted frame that was already successfully
 *  processed. Hence this counter should not be treated as a reliable guide
 *  to lost frames. The counter wraps to 0 after 65535.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DISCARDED_FRAME_COUNT 0x08A6

/*******************************************************************************
 * NAME          : UnifiIbssBeaconRateStart
 * PSID          : 2215 (0x08A7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 0X0005000A
 * DESCRIPTION   :
 *  In Oxygen it is required that in the first X seconds after joining an
 *  IBSS at least Y beacons must be transmitted. With this MIB it is possible
 *  to set the number of seconds X and the number of beacons Y where the most
 *  significant 16 bits is the number of seconds and the least significant 16
 *  bits is the number of beacons. If seconds or beacons is zero the feature
 *  is disabled.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_IBSS_BEACON_RATE_START 0x08A7

/*******************************************************************************
 * NAME          : UnifiIbssBeaconRateOnGoing
 * PSID          : 2216 (0x08A8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 0X000A0001
 * DESCRIPTION   :
 *  In Oxygen it is required that after the first N seconds which can be set
 *  by unifiIBSSBeaconRateStart at least Y beacons must be transmitted for
 *  every X seconds. With this MIB it is possible to set the number of
 *  seconds X and the number of beacons Y where the most significant 16 bits
 *  is the number of seconds and the least significant 16 bits is the number
 *  of beacons. If seconds or beacons is zero the feature is disabled.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_IBSS_BEACON_RATE_ON_GOING 0x08A8

/*******************************************************************************
 * NAME          : UnifiReceiverLeaderTimeout
 * PSID          : 2217 (0x08A9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 8
 * DESCRIPTION   :
 *  This attribute shall indicate the maximum number of seconds during which
 *  the leader has not received any action frames from the multicast
 *  transmitter before leader think the multicaster has cancelled its leader
 *  relation. After timer triggers TIMEOUT, FW shall cancel and unconfigure
 *  the HW
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RECEIVER_LEADER_TIMEOUT 0x08A9

/*******************************************************************************
 * NAME          : UnifiCurrentTsfTime
 * PSID          : 2218 (0x08AA)
 * PER INTERFACE?: NO
 * TYPE          : INT64
 * MIN           : -9223372036854775808
 * MAX           : 9223372036854775807
 * DEFAULT       :
 * DESCRIPTION   :
 *  Get TSF time (last 32 bits) for the specified VIF. VIF index can't be 0
 *  as that is treated as global VIF For station VIF - Correct BSS TSF wil
 *  only be reported after MLME-CONNECT.indication(success) indication to
 *  host. Note that if MAC Hardware is switched off then TSF returned is
 *  estimated value
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CURRENT_TSF_TIME 0x08AA

/*******************************************************************************
 * NAME          : UnifiTxFailureThreshold
 * PSID          : 2219 (0x08AB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  For Oxygen Fast TX Failure Event Notification. This value is the number
 *  of consecutive transmission failures for a peer device before the
 *  notification event is sent to host. value set to 0 disables this
 *  functionality
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_FAILURE_THRESHOLD 0x08AB

/*******************************************************************************
 * NAME          : UnifiRmcActionPeriod
 * PSID          : 2220 (0x08AC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : ms
 * MIN           : 0
 * MAX           : 1000
 * DEFAULT       : 300
 * DESCRIPTION   :
 *  This variable specifies the repetition period at which the Leader Select
 *  Action frame shall be transmitted.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RMC_ACTION_PERIOD 0x08AC

/*******************************************************************************
 * NAME          : UnifiRmcLeaderReselectPeriod
 * PSID          : 2221 (0x08AD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : s
 * MIN           : 0
 * MAX           : 360
 * DEFAULT       : 30
 * DESCRIPTION   :
 *  This variable specifies the rate at which the F/W will determine whether
 *  or not the current Receiver Leader should change based on link quality.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RMC_LEADER_RESELECT_PERIOD 0x08AD

/*******************************************************************************
 * NAME          : UnifiRmcFailureThreshold
 * PSID          : 2222 (0x08AE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 3
 * DESCRIPTION   :
 *  This value is the number of consecutive RMC transmission failures
 *  triggering the reselection of the RMC Receiver Leader. value set to 0
 *  disables this functionality.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RMC_FAILURE_THRESHOLD 0x08AE

/*******************************************************************************
 * NAME          : UnifiIbssShortRetryLimit
 * PSID          : 2223 (0x08AF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 255
 * DEFAULT       : 7
 * DESCRIPTION   :
 *  This attribute indicates the maximum number of transmission attempts of a
 *  frame whose length is less than or equal to dot11RTSThreshold, that shall
 *  be made in an IBSS before a failure condition is indicated.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_IBSS_SHORT_RETRY_LIMIT 0x08AF

/*******************************************************************************
 * NAME          : UnifiIbssLongRetryLimit
 * PSID          : 2224 (0x08B0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 255
 * DEFAULT       : 4
 * DESCRIPTION   :
 *  This attribute indicates the maximum number of transmission attempts of a
 *  frame whose length is greater than dot11RTSThreshold, that shall be made
 *  in an IBSS before a failure condition is indicated.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_IBSS_LONG_RETRY_LIMIT 0x08B0

/*******************************************************************************
 * NAME          : UnifiBaConfig
 * PSID          : 2225 (0x08B1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 0X3FFF00
 * DESCRIPTION   :
 *  Block Ack Configuration. It is composed of A-MSDU supported, TX MPDU per
 *  A-MPDU, RX Buffer size, TX Buffer size and Block Ack Timeout. see
 *  init_mlme_ba() for more detail
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BA_CONFIG 0x08B1

/*******************************************************************************
 * NAME          : UnifiBeaconReceived
 * PSID          : 2228 (0x08B4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Access point beacon received count from connected AP
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BEACON_RECEIVED 0x08B4

/*******************************************************************************
 * NAME          : UnifiAcRetries
 * PSID          : 2229 (0x08B5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  It represents the number of retransmitted frames under each ac priority
 *  (indexed by unifiAccessClassIndex). This number will wrap to zero after
 *  the range is exceeded.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_AC_RETRIES 0x08B5

/*******************************************************************************
 * NAME          : UnifiRadioOnTime
 * PSID          : 2230 (0x08B6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  msecs the radio is awake (32 bits number accruing over time)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_ON_TIME 0x08B6

/*******************************************************************************
 * NAME          : UnifiRadioTxTime
 * PSID          : 2231 (0x08B7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  msecs the radio is transmitting (32 bits number accruing over time)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_TX_TIME 0x08B7

/*******************************************************************************
 * NAME          : UnifiRadioRxTime
 * PSID          : 2232 (0x08B8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  msecs the radio is in active receive (32 bits number accruing over time)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_RX_TIME 0x08B8

/*******************************************************************************
 * NAME          : UnifiRadioScanTime
 * PSID          : 2233 (0x08B9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  msecs the radio is awake due to all scan (32 bits number accruing over
 *  time)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_SCAN_TIME 0x08B9

/*******************************************************************************
 * NAME          : UnifiPsLeakyAp
 * PSID          : 2234 (0x08BA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  indicate that this AP typically leaks packets beyond the guard time
 *  (5msecs).
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PS_LEAKY_AP 0x08BA

/*******************************************************************************
 * NAME          : UnifiTqamActivated
 * PSID          : 2235 (0x08BB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  True indicates that, use Vendor VHT IE for 256-QAM mode on 2.4GHz. The
 *  capability is disabled otherwise.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TQAM_ACTIVATED 0x08BB

/*******************************************************************************
 * NAME          : UnifiNoAckActivationCount
 * PSID          : 2240 (0x08C0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  The number of frames that are discarded due to HW No-ack activated during
 *  test. This number will wrap to zero after the range is exceeded.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NO_ACK_ACTIVATION_COUNT 0x08C0

/*******************************************************************************
 * NAME          : UnifiRxFcsErrorCount
 * PSID          : 2241 (0x08C1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  The number of received frames that are discarded due to bad FCS (CRC).
 *  This number will wrap to zero after the range is exceeded.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RX_FCS_ERROR_COUNT 0x08C1

/*******************************************************************************
 * NAME          : UnifiBeaconsReceivedPercentage
 * PSID          : 2245 (0x08C5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Percentage of beacons received, calculated as received / expected. The
 *  percentage is scaled to an integer value between 0 (0%) and 1000 (100%).
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BEACONS_RECEIVED_PERCENTAGE 0x08C5

/*******************************************************************************
 * NAME          : UnifiSwToHwQueueStats
 * PSID          : 2250 (0x08CA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  The timing statistics of packets being queued between SW-HW
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SW_TO_HW_QUEUE_STATS 0x08CA

/*******************************************************************************
 * NAME          : UnifiHostToSwQueueStats
 * PSID          : 2251 (0x08CB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  The timing statistics of packets being queued between HOST-SW
 *******************************************************************************/
#define SLSI_PSID_UNIFI_HOST_TO_SW_QUEUE_STATS 0x08CB

/*******************************************************************************
 * NAME          : UnifiQueueStatsEnable
 * PSID          : 2252 (0x08CC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Enables recording timing statistics of packets being queued between
 *  HOST-SW-HW
 *******************************************************************************/
#define SLSI_PSID_UNIFI_QUEUE_STATS_ENABLE 0x08CC

/*******************************************************************************
 * NAME          : UnifiTxDataConfirm
 * PSID          : 2253 (0x08CD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  This attribute allows to request on a per access class basis that an
 *  MA_UNITDATA.confirm be generated after each packet transfer. The default
 *  value is applied for all ACs.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_DATA_CONFIRM 0x08CD

/*******************************************************************************
 * NAME          : UnifiThroughputDebug
 * PSID          : 2254 (0x08CE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  This MIB is used to access throughput related counters that can help
 *  diagnose throughput problems. The index of the MIB will access different
 *  counters, as described in SC-506328-DD.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_THROUGHPUT_DEBUG 0x08CE

/*******************************************************************************
 * NAME          : UnifiLoadDpdLut
 * PSID          : 2255 (0x08CF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 144
 * MAX           : 144
 * DEFAULT       :
 * DESCRIPTION   :
 *  Write a DPD LUT entry
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LOAD_DPD_LUT 0x08CF

/*******************************************************************************
 * NAME          : UnifiDpdMasterSwitch
 * PSID          : 2256 (0x08D0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Enables Digital Pre-Distortion
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DPD_MASTER_SWITCH 0x08D0

/*******************************************************************************
 * NAME          : UnifiDpdPredistortGains
 * PSID          : 2257 (0x08D1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 98
 * MAX           : 98
 * DEFAULT       :
 * DESCRIPTION   :
 *  This attribute used to specify DPD pre-distort mode gains for each 2G
 *  channel. The format is [freq_msb, freq_lsb, OFDM0_gain, OFDM1_gain,
 *  CCK_gain, TR_gain]. The sequence is repeated for each 2G channel starting
 *  with lowest channel.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DPD_PREDISTORT_GAINS 0x08D1

/*******************************************************************************
 * NAME          : UnifiGoogleMaxNumberOfPeriodicScans
 * PSID          : 2260 (0x08D4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 6
 * DESCRIPTION   :
 *  Max number of periodic scans for Google scan functionality.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_GOOGLE_MAX_NUMBER_OF_PERIODIC_SCANS 0x08D4

/*******************************************************************************
 * NAME          : UnifiGoogleMaxRssiSampleSize
 * PSID          : 2261 (0x08D5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 8
 * DESCRIPTION   :
 *  Max number of RSSI samples used for averaging RSSI in Google scan
 *  functionality.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_GOOGLE_MAX_RSSI_SAMPLE_SIZE 0x08D5

/*******************************************************************************
 * NAME          : UnifiGoogleMaxHotlistAPs
 * PSID          : 2262 (0x08D6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 64
 * DESCRIPTION   :
 *  Max number of entries for hotlist APs in Google scan functionality.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_GOOGLE_MAX_HOTLIST_APS 0x08D6

/*******************************************************************************
 * NAME          : UnifiGoogleMaxSignificantWifiChangeAPs
 * PSID          : 2263 (0x08D7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 64
 * DESCRIPTION   :
 *  Max number of entries for significant WiFi change APs in Google scan
 *  functionality.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_GOOGLE_MAX_SIGNIFICANT_WIFI_CHANGE_APS 0x08D7

/*******************************************************************************
 * NAME          : UnifiGoogleMaxBssidHistoryEntries
 * PSID          : 2264 (0x08D8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Max number of BSSID/RSSI that the device can hold in Google scan
 *  functionality.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_GOOGLE_MAX_BSSID_HISTORY_ENTRIES 0x08D8

/*******************************************************************************
 * NAME          : UnifiMacBeaconTimeout
 * PSID          : 2270 (0x08DE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 128
 * DESCRIPTION   :
 *  The maximum time in microseconds we want to stall TX data when expecting
 *  a beacon at EBRT time as a station.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MAC_BEACON_TIMEOUT 0x08DE

/*******************************************************************************
 * NAME          : UnifiOverrideDefaultBetxopForHt
 * PSID          : 2364 (0x093C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  When set to non-zero value then this will override the BE TXOP for 11n
 *  and higher modulations (in 32 usec units) to the value specified here.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OVERRIDE_DEFAULT_BETXOP_FOR_HT 0x093C

/*******************************************************************************
 * NAME          : UnifiOverrideDefaultBetxop
 * PSID          : 2365 (0x093D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 78
 * DESCRIPTION   :
 *  When set to non-zero value then this will override the BE TXOP for 11g
 *  (in 32 usec units) to the value specified here.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OVERRIDE_DEFAULT_BETXOP 0x093D

/*******************************************************************************
 * NAME          : UnifiRxabbTrimSettings
 * PSID          : 2366 (0x093E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Various settings to change RX ABB filter trim behavior.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RXABB_TRIM_SETTINGS 0x093E

/*******************************************************************************
 * NAME          : UnifiRadioTrimsEnable
 * PSID          : 2367 (0x093F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  A bitmap for enabling/disabling trims at runtime | lsb | trim |
 * |-_-_-+-_-_-_-_-_| | 0 | RX ABB | | 1 | TX ABB | other trims might
 *  follow.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_TRIMS_ENABLE 0x093F

/*******************************************************************************
 * NAME          : UnifiRadioCcaThresholds
 * PSID          : 2368 (0x0940)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0X1C32
 * DESCRIPTION   :
 *  The CCA thresholds so that the CCA-ED triggers at the regulatory value of
 *  -62 dBm. Gain threshold in the lower octet and RSSI threshold in the
 *  higher octet.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_CCA_THRESHOLDS 0x0940

/*******************************************************************************
 * NAME          : UnifiHardwarePlatform
 * PSID          : 2369 (0x0941)
 * PER INTERFACE?: NO
 * TYPE          : unifiHardwarePlatform
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  This MIB specifies the hardware platform. This is necessary so we can
 *  apply tweaks to specific revisions, even though they might be running the
 *  same baseband and RF chip combination. Check unifiHardwarePlatform enum
 *  for description of the possible values.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_HARDWARE_PLATFORM 0x0941

/*******************************************************************************
 * NAME          : UnifiForceChannelBw
 * PSID          : 2370 (0x0942)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Test Mib to force channel bandwidth to specified value. This is used to
 *  allow emulator/silicon back to back connection to commnunicate at
 *  bandwidth other than default (20 MHz) Setting it to 0 uses the default
 *  bandwidth as selected by firmware channel_bw_20_mhz = 20,
 *  channel_bw_40_mhz = 40, channel_bw_80_mhz = 80, Note that it is live mib
 *  with default set to 0. Its default value is set in firmware and can't be
 *  changed by just changing the value in mib.xml file
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FORCE_CHANNEL_BW 0x0942

/*******************************************************************************
 * NAME          : UnifiDpdTrainingDuration
 * PSID          : 2371 (0x0943)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  This MIB specifies the duration of DPD training (in ms).
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DPD_TRAINING_DURATION 0x0943

/*******************************************************************************
 * NAME          : UnifiHighTemperatureCutOffThreshold
 * PSID          : 2446 (0x098E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : Celsius
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       : 255
 * DESCRIPTION   :
 *  This attribute specifies the temperature threshold at which wifi
 *  transmission will be paused. Normal values for this MIB is between +85 to
 * +125 degree Celsius. Setting the value to 255 will disable the Cut off
 *  mechanism. Deprecated - Condor onwards
 *******************************************************************************/
#define SLSI_PSID_UNIFI_HIGH_TEMPERATURE_CUT_OFF_THRESHOLD 0x098E

/*******************************************************************************
 * NAME          : UnifiLowTemperatureResumeThreshold
 * PSID          : 2447 (0x098F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : Celsius
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       : 105
 * DESCRIPTION   :
 *  This attribute specifies the temperature threshold below which wifi
 *  transmission will be resumed. Normal values for this MIB is between +85
 *  to +125 degree Celsius. Setting the value to 255 will disable the resume
 *  mechanism. Its value should always be less than
 *  unifiHighTemperatureCutOffThreshold value. Deprecated - Condor onwards
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LOW_TEMPERATURE_RESUME_THRESHOLD 0x098F

/*******************************************************************************
 * NAME          : UnifiFastPowerSaveTimeout
 * PSID          : 2500 (0x09C4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 2147483647
 * DEFAULT       : 400000
 * DESCRIPTION   :
 *  UniFi implements a proprietary power management mode called Fast Power
 *  Save that balances network performance against power consumption. In this
 *  mode UniFi delays entering power save mode until it detects that there
 *  has been no exchange of data for the duration specified in usec by this
 *  attribute.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FAST_POWER_SAVE_TIMEOUT 0x09C4

/*******************************************************************************
 * NAME          : UnifiIbssKeepAlivePeriod
 * PSID          : 2501 (0x09C5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : s
 * MIN           : 0
 * MAX           : 2147
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  AdHoc/IBSS/Oxygen Mode: This variable specifies the interval between
 *  sending keep-alive (Null frame) packets to associated station in an Ad
 *  Hoc network if there has been no unicast Rx/Tx activities. Setting it to
 *  0 Disables it. This MIB should be set before the VIF is created. If set
 *  to less than 10s (but not 0) it defaults to 10.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_IBSS_KEEP_ALIVE_PERIOD 0x09C5

/*******************************************************************************
 * NAME          : UnifiStaKeepAlivePeriod
 * PSID          : 2502 (0x09C6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : s
 * MIN           : 0
 * MAX           : 2147
 * DEFAULT       : 30
 * DESCRIPTION   :
 *  Station/P2P Client Mode: This variable specifies the interval between
 *  sending keep-alive (Null frame) packets while associated to an access
 *  point during periods of idleness (i.e. when there is no unicast transmit
 *  or receive activity).Setting it to 0 Disables it. This MIB should be set
 *  before the VIF is created. If set to less than 10s (but not 0) it
 *  defaults to 10.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_STA_KEEP_ALIVE_PERIOD 0x09C6

/*******************************************************************************
 * NAME          : UnifiApKeepAlivePeriod
 * PSID          : 2503 (0x09C7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : s
 * MIN           : 0
 * MAX           : 2147
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  Ap Mode:This variable specifies the interval between sending keep-alive
 *  (Null frame) packetsto associated stations if there has been no unicast
 *  Rx/Tx activities.Setting it to 0 Disables it. This MIB should be set
 *  before the VIF is created. Min value when different to 0 is 10s.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_AP_KEEP_ALIVE_PERIOD 0x09C7

/*******************************************************************************
 * NAME          : UnifiGoKeepAlivePeriod
 * PSID          : 2504 (0x09C8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : s
 * MIN           : 0
 * MAX           : 2147
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  P2P GO Mode:This variable specifies the interval between sending
 *  keep-alive (Null frame) packetsto associated P2P Clients if there has
 *  been no unicast Rx/Tx activities. Setting it to 0 Disables it.This MIB
 *  should be set before the VIF is created. min value when different to 0 is
 *  10s.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_GO_KEEP_ALIVE_PERIOD 0x09C8

/*******************************************************************************
 * NAME          : UnifiStaRouterAdvertisementMinimumIntervalToForward
 * PSID          : 2505 (0x09C9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : s
 * MIN           : 0
 * MAX           : 4294967285
 * DEFAULT       : 60
 * DESCRIPTION   :
 *  STA Mode: This variable specifies the minimum interval to forward Router
 *  Advertisement frame to Host. Minimum value = 60 secs.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_STA_ROUTER_ADVERTISEMENT_MINIMUM_INTERVAL_TO_FORWARD 0x09C9

/*******************************************************************************
 * NAME          : UnifiConnectionQualityCheckWaitAfterConnect
 * PSID          : 2506 (0x09CA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : ms
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 200
 * DESCRIPTION   :
 *  The amount of time a STA will wait after connection before starting to
 *  check the MLME-installed connection quality trigger thresholds
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CONNECTION_QUALITY_CHECK_WAIT_AFTER_CONNECT 0x09CA

/*******************************************************************************
 * NAME          : UnifiApBeaconMaxDrift
 * PSID          : 2507 (0x09CB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0XFFFF
 * DESCRIPTION   :
 *  The maximum drift in microseconds we will allow for each beacon sent when
 *  we're trying to move it to get a 50% duty cycle between GO and STA in
 *  multiple VIF scenario. We'll delay our TX beacon by a maximum of this
 *  value until we reach our target TBTT. We have 3 possible cases for this
 *  value: a) ap_beacon_max_drift = 0x0000 - Feature disabled b)
 *  ap_beacon_max_drift between 0x0001 and 0xFFFE - Each time we transmit the
 *  beacon we'll move it a little bit forward but never more than this. (Not
 *  implemented yet) c) ap_beacon_max_drift = 0xFFFF - Move the beacon to the
 *  desired position in one shot.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_AP_BEACON_MAX_DRIFT 0x09CB

/*******************************************************************************
 * NAME          : UnifiVifIdleMonitorTime
 * PSID          : 2509 (0x09CD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : s
 * MIN           : 0
 * MAX           : 1800
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  In Fast Power Save mode, the STA will decide whether it is idle based on
 *  monitoring its traffic class. If the traffic class is continuously
 *  "occasional" for equal or longer than this MIB value (specified in
 *  seconds), then the VIF is marked as idle. Traffic class monitoring is
 *  based on the interval specified in the "unifiExitPowerSavePeriod" MIB
 *******************************************************************************/
#define SLSI_PSID_UNIFI_VIF_IDLE_MONITOR_TIME 0x09CD

/*******************************************************************************
 * NAME          : UnifiDisableLegacyPowerSave
 * PSID          : 2510 (0x09CE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  This affects Station VIF power save behaviour. Setting it to 1 will
 *  disable legacy power save (i.e. we wil use fast power save to retrieve
 *  data) Note that this MIB actually disables full power save mode (i.e
 *  sending trigger to retrieve frames which will be PS-POLL for legacy and
 *  QOS-NULL for UAPSD)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DISABLE_LEGACY_POWER_SAVE 0x09CE

/*******************************************************************************
 * NAME          : UnifiForceActive
 * PSID          : 2511 (0x09CF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       :
 * DESCRIPTION   :
 *  This mib will always force station power save mode to be active (when
 *  scheduled). VIF scheduling, coex and other non-VIF specific reasons could
 *  still force power save on VIF. Applies to all VIFs of type station
 *  (includes P2P clieant). This mib is only provided for test purpose.
 *  Changes to the mib will only get applied after next host/mlme power
 *  management request.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FORCE_ACTIVE 0x09CF

/*******************************************************************************
 * NAME          : UnifiPowerManagementDelayTimeout
 * PSID          : 2514 (0x09D2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 2147483647
 * DEFAULT       : 30000
 * DESCRIPTION   :
 *  When UniFi enters power save mode it signals the new state by setting the
 *  power management bit in the frame control field of a NULL frame. It then
 *  remains active for the period since the previous unicast reception, or
 *  since the transmission of the NULL frame, whichever is later. This
 *  attribute controls the maximum time during which UniFi will continue to
 *  listen for data. This allows any buffered data on a remote device to be
 *  cleared. Note that this attribute specifies an upper limit on the
 *  timeout. UniFi internally implements a proprietary algorithm to adapt the
 *  timeout depending upon the situation.This is used by firmware when
 *  current station VIF is only station VIF which can be scheduled
 *******************************************************************************/
#define SLSI_PSID_UNIFI_POWER_MANAGEMENT_DELAY_TIMEOUT 0x09D2

/*******************************************************************************
 * NAME          : UnifiApsdServicePeriodTimeout
 * PSID          : 2515 (0x09D3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 20000
 * DESCRIPTION   :
 *  During Unscheduled Automated Power Save Delivery (U-APSD), UniFi may
 *  trigger a service period in order to fetch data from the access point.
 *  The service period is normally terminated by a frame from the access
 *  point with the EOSP (End Of Service Period) flag set, at which point
 *  UniFi returns to sleep. However, if the access point is temporarily
 *  inaccessible, UniFi would stay awake indefinitely. This attribute
 *  specifies a timeout starting from the point where the trigger frame has
 *  been sent. If the timeout expires and no data has been received from the
 *  access point, UniFi will behave as if the service period had been ended
 *  normally and return to sleep. This timeout takes precedence over
 *  unifiPowerSaveExtraListenTime if both would otherwise be applicable.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_APSD_SERVICE_PERIOD_TIMEOUT 0x09D3

/*******************************************************************************
 * NAME          : UnifiConcurrentPowerManagementDelayTimeout
 * PSID          : 2516 (0x09D4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 2147483647
 * DEFAULT       : 6000
 * DESCRIPTION   :
 *  When UniFi enters power save mode it signals the new state by setting the
 *  power management bit in the frame control field of a NULL frame. It then
 *  remains active for the period since the previous unicast reception, or
 *  since the transmission of the NULL frame, whichever is later. This
 *  attribute controls the maximum time during which UniFi will continue to
 *  listen for data. This allows any buffered data on a remote device to be
 *  cleared.This is same as unifiPowerManagementDelayTimeout but this value
 *  is considered only when we are doing multivif operations and other VIFs
 *  are waiting to be scheduled.Note that firmware automatically chooses one
 *  of unifiPowerManagementDelayTimeout and
 *  unifiConcurrentPowerManagementDelayTimeout depending upon the current
 *  situation.It is sensible to set unifiPowerManagementDelayTimeout to be
 *  always more thanunifiConcurrentPowerManagementDelayTimeout.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CONCURRENT_POWER_MANAGEMENT_DELAY_TIMEOUT 0x09D4

/*******************************************************************************
 * NAME          : UnifiStationQosInfo
 * PSID          : 2517 (0x09D5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  This MIB variable indicates the QoS capability for a non-AP Station, and
 *  is encoded as per IEEE 802.11 QoS Capability.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_STATION_QOS_INFO 0x09D5

/*******************************************************************************
 * NAME          : UnifiListenIntervalSkippingDtim
 * PSID          : 2518 (0x09D6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : DTIM intervals
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 0X000A89AA
 * DESCRIPTION   :
 *  This attribute specifies the listen interval of beacons when in
 *  single-vif power saving mode and receiving DTIMs is enabled. No DTIMs are
 *  skipped during MVIF operation. A maximum of the listen interval beacons
 *  are skipped, which may be less than the number of DTIMs that can be
 *  skipped. The value is a lookup table for DTIM counts. Each 4bits, in LSB
 *  order, represent DTIM1, DTIM2, DTIM3, DTIM4, DTIM5, (unused). This key is
 *  only used for STA VIF, connected to an AP. For P2P group client
 *  intervals, refer to unifiP2PListenIntervalSkippingDTIM, PSID=2523.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LISTEN_INTERVAL_SKIPPING_DTIM 0x09D6

/*******************************************************************************
 * NAME          : UnifiListenInterval
 * PSID          : 2519 (0x09D7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : beacon intervals
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  Defines the Beacon Listen Interval
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LISTEN_INTERVAL 0x09D7

/*******************************************************************************
 * NAME          : UnifiLegacyPsPollTimeout
 * PSID          : 2520 (0x09D8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 15000
 * DESCRIPTION   :
 *  Time we try to stay awake after sending a PS-POLL to receive data.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LEGACY_PS_POLL_TIMEOUT 0x09D8

/*******************************************************************************
 * NAME          : UnifiRadioCalibrationMode
 * PSID          : 2521 (0x09D9)
 * PER INTERFACE?: NO
 * TYPE          : unifiRadioCalibrationMode
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  This attribute is used to control the radio calibration operations
 *  performed by UniFi. If this key is left in the &apos;calibrate-auto&apos;
 *  state then the firmware will perform radio calibrations whenever they are
 *  required. In this mode a calibration is always performed after the first
 *  MLME-RESET - this calibration is required to enable the radio. This key
 *  can be set to the &apos;calibrate-now&apos; value to hint to the firmware
 *  that a calibration should be performed now. This can be useful if the
 *  host software knows that the radio has not been used for some time, but
 *  that it is about to be used. If this key is set to the
 *  &apos;no-calibrate&apos; setting then no radio calibrations will be
 *  performed by the firmware until it is commanded to (by setting the key to
 *  either &apos;calibrate-auto&apos; or &apos;calibrate-now&apos;. If this
 *  key is set to this state before the first MLME-RESET then the radio will
 *  not be able to be used. The &apos;no-calibrate&apos; mode will allow the
 *  fastest booting and will ensure that no RF power is emitted from the
 *  device. In this mode the radio will not work, even for receive.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RADIO_CALIBRATION_MODE 0x09D9

/*******************************************************************************
 * NAME          : UnifiTogglePowerDomain
 * PSID          : 2522 (0x09DA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Toggle WLAN power domain when entering dorm mode (deep sleep). When
 *  entering deep sleep and this value it true, then the WLAN power domain is
 *  disabled for the deep sleep duration. When false, the power domain is
 *  left turned on. This is to work around issues with WLAN rx, and is
 *  considered temporary until the root cause is found and fixed.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TOGGLE_POWER_DOMAIN 0x09DA

/*******************************************************************************
 * NAME          : UnifiP2PListenIntervalSkippingDtim
 * PSID          : 2523 (0x09DB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : DTIM intervals
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 0X00000002
 * DESCRIPTION   :
 *  This attribute specifies the listen interval of beacons when in
 *  single-vif, P2P client power saving mode and receiving DTIMs. No DTIMs
 *  are skipped during MVIF operation. A maximum of (listen interval - 1)
 *  beacons are skipped, which may be less than the number of DTIMs that can
 *  be skipped. The value is a lookup table for DTIM counts. Each 4bits, in
 *  LSB order, represent DTIM1, DTIM2, DTIM3, DTIM4, DTIM5, (unused). This
 *  key is only used for P2P group client. For STA connected to an AP, refer
 *  to unifiListenIntervalSkippingDTIM, PSID=2518.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_P2_PLISTEN_INTERVAL_SKIPPING_DTIM 0x09DB

/*******************************************************************************
 * NAME          : UnifiFragmentationDuration
 * PSID          : 2524 (0x09DC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  A limit on transmission time for a data frame. If the data payload would
 *  take longer than unifiFragmentationDuration to transmit, UniFi will
 *  attempt to fragment the frame to ensure that the data portion of each
 *  fragment is within the limit. The limit imposed by the fragmentation
 *  threshold is also respected, and no more than 16 fragments may be
 *  generated. If the value is zero no limit is imposed. The value may be
 *  changed dynamically during connections. Note that the limit is a
 *  guideline and may not always be respected. In particular, the data rate
 *  is finalised after fragmentation in order to ensure responsiveness to
 *  conditions, the calculation is not performed to high accuracy, and octets
 *  added during encryption are not included in the duration calculation.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FRAGMENTATION_DURATION 0x09DC

/*******************************************************************************
 * NAME          : UnifiDtimWaitTimeout
 * PSID          : 2529 (0x09E1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : microseconds
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 50000
 * DESCRIPTION   :
 *  If UniFi is in power save and receives a Traffic Indication Map from its
 *  associated access point with a DTIM indication, it will wait a maximum
 *  time given by this attribute for succeeding broadcast or multicast
 *  traffic, or until it receives such traffic with the &apos;more data&apos;
 *  flag clear. Any reception of broadcast or multicast traffic with the
 *  &apos;more data&apos; flag set, or any reception of unicast data, resets
 *  the timeout. The timeout can be turned off by setting the value to zero;
 *  in that case UniFi will remain awake indefinitely waiting for broadcast
 *  or multicast data. Otherwise, the value should be larger than that of
 *  unifiPowerSaveExtraListenTime.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DTIM_WAIT_TIMEOUT 0x09E1

/*******************************************************************************
 * NAME          : UnifiListenIntervalMaxTime
 * PSID          : 2530 (0x09E2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 65536
 * DEFAULT       : 1000
 * DESCRIPTION   :
 *  This attribute specifies the maximum number length of time, in Time Units
 *  (1TU = 1024us), that can be used as a beacon listen interval. This will
 *  limit how many beacons maybe skipped, and affects the DTIM beacon
 *  skipping count; DTIM skipping (if enabled) will be such that skipped
 *  count = (unifiListenIntervalMaxTime / DTIM_period).
 *******************************************************************************/
#define SLSI_PSID_UNIFI_LISTEN_INTERVAL_MAX_TIME 0x09E2

/*******************************************************************************
 * NAME          : UnifiScanMaxProbeTransmitLifetime
 * PSID          : 2531 (0x09E3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : TU
 * MIN           : 1
 * MAX           : 4294967295
 * DEFAULT       : 64
 * DESCRIPTION   :
 *  If the value of this attribute is non zero, it is used during active
 *  scans as the maximum lifetime for probe requests.It is the elapsed time
 *  after the initial transmissionat which further attempts to transmit the
 *  probe are terminated.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SCAN_MAX_PROBE_TRANSMIT_LIFETIME 0x09E3

/*******************************************************************************
 * NAME          : UnifiPowerSaveTransitionPacketThreshold
 * PSID          : 2532 (0x09E4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  If VIF has these many packet queued/transmitted/received in last
 *  unifiFastPowerSaveTransitionPeriod then firmware may decide to come out
 *  of aggressive power save mode. This is applicable to STA (CLI) and GO
 *  (VIF).Note that this is only a guideline. Firmware internal factors may
 *  override this MIB.Also see unifiExitPowerSavePeriod and
 *  unifiAggressivePowerSaveTransitionPeriod.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_POWER_SAVE_TRANSITION_PACKET_THRESHOLD 0x09E4

/*******************************************************************************
 * NAME          : UnifiProbeResponseLifetime
 * PSID          : 2533 (0x09E5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 500
 * DESCRIPTION   :
 *  This mib entry is used to indicate the lifetime of proberesponse frame in
 *  unit of ms.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PROBE_RESPONSE_LIFETIME 0x09E5

/*******************************************************************************
 * NAME          : UnifiProbeResponseMaxRetry
 * PSID          : 2534 (0x09E6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  This mib entry is used to indicate the number of retries of probe
 *  response frame.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PROBE_RESPONSE_MAX_RETRY 0x09E6

/*******************************************************************************
 * NAME          : UnifiExitPowerSavePeriod
 * PSID          : 2535 (0x09E7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 200
 * DESCRIPTION   :
 *  Period in TUs over which firmware counts number of packet
 *  transmitted/queued/received to decide to come out of aggressive power
 *  save mode.This is applicable to STA (CLI) and GO (AP) VIF. Note that this
 *  is only a guideline. Firmware internal factors may override this MIB.
 *  Also see unifiPowerSaveTransitionPacketThreshold and
 *  unifiAggressivePowerSaveTransitionPeriod
 *******************************************************************************/
#define SLSI_PSID_UNIFI_EXIT_POWER_SAVE_PERIOD 0x09E7

/*******************************************************************************
 * NAME          : UnifiAggressivePowerSaveTransitionPeriod
 * PSID          : 2536 (0x09E8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  Defines how many unifiExitPowerSavePeriod firmware should wait in which
 *  VIF had received/transmitted/queued less than
 *  unifiPowerSaveTransitionPacketThreshold packets - before entering
 *  aggressive power save mode (when not in aggressive power save mode) This
 *  is applicable to STA (CLI) and GO (AP) VIF. Note that this is only a
 *  guideline. Firmware internal factors may override this MIB. Also see
 *  unifiPowerSaveTransitionPacketThreshold and unifiExitPowerSavePeriod.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_AGGRESSIVE_POWER_SAVE_TRANSITION_PERIOD 0x09E8

/*******************************************************************************
 * NAME          : UnifiActiveTimeAfterMoreBit
 * PSID          : 2537 (0x09E9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 30
 * DESCRIPTION   :
 *  After seeing the "more" bit set in a message from the AP, the STA will
 *  goto active mode for this duration of time. After this time, traffic
 *  information is evaluated to determine whether the STA should stay active
 *  or go to powersave. Setting this value to 0 means that the described
 *  functionality is disabled.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_ACTIVE_TIME_AFTER_MORE_BIT 0x09E9

/*******************************************************************************
 * NAME          : UnifiForcedScheduleDuration
 * PSID          : 2538 (0x09EA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 50
 * DESCRIPTION   :
 *  Defines the time to keep a VIF scheduled after an outgoing packet is
 *  queued, if the "Immediate_Response_Expected" bit is set Tx control
 *  associated with a frame transmission request. The firmware may choose to
 *  override this value based on internal logic.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FORCED_SCHEDULE_DURATION 0x09EA

/*******************************************************************************
 * NAME          : UnifiVhtCapabilities
 * PSID          : 2540 (0x09EC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 12
 * MAX           : 12
 * DEFAULT       : { 0X31, 0X73, 0X80, 0X01, 0XFE, 0XFF, 0X00, 0X00, 0XFE, 0XFF, 0X00, 0X00 }
 * DESCRIPTION   :
 *  This MIB variable indicates the VHT capabilities of the chip. see
 *  SC-503520-SP
 *******************************************************************************/
#define SLSI_PSID_UNIFI_VHT_CAPABILITIES 0x09EC

/*******************************************************************************
 * NAME          : UnifiMaxVifScheduleDuration
 * PSID          : 2541 (0x09ED)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 20
 * DESCRIPTION   :
 *  Default time for which a non-scan VIF can be scheduled. Applies to
 *  multiVIF scenario. This is used as a guideline to firmware. Internal
 *  firmware logic or BSS state (e.g. NOA) may cut short the schedule..
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MAX_VIF_SCHEDULE_DURATION 0x09ED

/*******************************************************************************
 * NAME          : UnifiVifLongIntervalTime
 * PSID          : 2542 (0x09EE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 60
 * DESCRIPTION   :
 *  When the scheduler expects a VIF to schedule for time longer than this
 *  parameter (specified in TUs), then the VIF may come out of powersave.
 *  Only valid for STA VIFs.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_VIF_LONG_INTERVAL_TIME 0x09EE

/*******************************************************************************
 * NAME          : UnifiDisallowSchedRelinquish
 * PSID          : 2543 (0x09EF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  When this value is set to TRUE, the VIFs will not relinquish their
 *  assigned schedules when they have nothing left to do.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DISALLOW_SCHED_RELINQUISH 0x09EF

/*******************************************************************************
 * NAME          : UnifiRameDplaneOperationTimeout
 * PSID          : 2544 (0x09F0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : milliseconds
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 1000
 * DESCRIPTION   :
 *  Timeout for requests sent from MACRAME to Data Plane. Any value below
 *  1000ms will be capped at 1000ms.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RAME_DPLANE_OPERATION_TIMEOUT 0x09F0

/*******************************************************************************
 * NAME          : UnifiMaxClient
 * PSID          : 2550 (0x09F6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Restricts the maximum number of associated STAs for SoftAP.Defaulted to 0
 *  to allow the Soft AP to allow as many associated STAs as it can support.
 *  The Soft AP may restrict the number of associated STAs to less than this
 *  value (if non-zero), if the Soft AP is unable to support that many
 *  associated STAs. If non-zero the number of associated STAs will not
 *  exceed this value.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_MAX_CLIENT 0x09F6

/*******************************************************************************
 * NAME          : UnifiNoaDuration
 * PSID          : 2552 (0x09F8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  This MIB represents the absence period of P2P NoA in microsecond. If Host
 *  want to enable NoA, it needs to set both the unifiNoaDuration and the
 *  unifiNoaCount.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NOA_DURATION 0x09F8

/*******************************************************************************
 * NAME          : UnifiNoaCount
 * PSID          : 2553 (0x09F9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  This mib represents the count of P2P NoA. If the count is 255, then the
 *  P2P NoA is the periodic NoA(Infinite). If Host want to enable NoA, it
 *  needs to set both the unifiNoaDuration and the unifiNoaCount.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NOA_COUNT 0x09F9

/*******************************************************************************
 * NAME          : UnifiNoaInterval
 * PSID          : 2554 (0x09FA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 100
 * DESCRIPTION   :
 *  This MIB represents the interval of P2P NoA in Time Unit(TU).
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NOA_INTERVAL 0x09FA

/*******************************************************************************
 * NAME          : UnifiNoaStartOffset
 * PSID          : 2555 (0x09FB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 100
 * DESCRIPTION   :
 *  This MIB represents the start offset of P2P NoA in Time Unit(TU). Firware
 *  will calculate the actual start time using this value.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NOA_START_OFFSET 0x09FB

/*******************************************************************************
 * NAME          : UnifiTdlsInP2pActivated
 * PSID          : 2556 (0x09FC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  This attribute, when TRUE, indicates that use TDLS in P2P mode. The TDLS
 *  in P2P is disabled otherwise.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TDLS_IN_P2P_ACTIVATED 0x09FC

/*******************************************************************************
 * NAME          : UnifiCtWindow
 * PSID          : 2557 (0x09FD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  This value represents the CTWindow value.If this MIB is zero, the
 *  CTwindow in NoA Attribute will be deleted.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CT_WINDOW 0x09FD

/*******************************************************************************
 * NAME          : UnifiTdlsActivated
 * PSID          : 2558 (0x09FE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  This attribute, when TRUE, indicates that use TDLS mode. The TDLS is
 *  disabled otherwise.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TDLS_ACTIVATED 0x09FE

/*******************************************************************************
 * NAME          : UnifiTdlsTpThresholdPktSecs
 * PSID          : 2559 (0x09FF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 100
 * DESCRIPTION   :
 *  his MIB shall be used for the argument "throughput_threshold_pktsecs" of
 *  RAME-MLME-ENABLE-PEER-TRAFFIC-REPORTING.request signal defined in
 *  SC-505422-DD.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TDLS_TP_THRESHOLD_PKT_SECS 0x09FF

/*******************************************************************************
 * NAME          : UnifiTdlsRssiThreshold
 * PSID          : 2560 (0x0A00)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       : -75
 * DESCRIPTION   :
 *  This MIB shall be used for the FW initiated TDLS Discovery/Setup
 *  procedure. If the RSSI of a received TDLS Discovery Response frame is
 *  greater than this value, the TDLS FSM shall initiate the TDLS Setup
 *  procedure.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TDLS_RSSI_THRESHOLD 0x0A00

/*******************************************************************************
 * NAME          : UnifiTdlsMaximumRetry
 * PSID          : 2561 (0x0A01)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 3
 * DESCRIPTION   :
 *  Transmission of a TDLS Action frame or a TDLS Discovery Response Public
 *  Action frame shall be retried unifiTdlsMaximumRetry times until the frame
 *  is transmitted successfully.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TDLS_MAXIMUM_RETRY 0x0A01

/*******************************************************************************
 * NAME          : UnifiTdlsTpMonitorSecs
 * PSID          : 2562 (0x0A02)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  add description
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TDLS_TP_MONITOR_SECS 0x0A02

/*******************************************************************************
 * NAME          : UnifiTdlsBasicHtMcsSet
 * PSID          : 2563 (0x0A03)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 16
 * MAX           : 16
 * DEFAULT       : { 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00 }
 * DESCRIPTION   :
 *  The TDLS FSM shall monitor the number of transmitted packet count per a
 *  TDLS peer for unifiTdlsTPMonitorSecs seconds to decide to tear down the
 *  TDLS link (see 7.12.1).
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TDLS_BASIC_HT_MCS_SET 0x0A03

/*******************************************************************************
 * NAME          : UnifiTdlsBasicVhtMcsSet
 * PSID          : 2564 (0x0A04)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 2
 * MAX           : 2
 * DEFAULT       : { 0XFE, 0XFF }
 * DESCRIPTION   :
 *  This MIB shall be used to build the VHT Operation element in the TDLS
 *  Setup Confirm frame
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TDLS_BASIC_VHT_MCS_SET 0x0A04

/*******************************************************************************
 * NAME          : Dot11TdlsDiscoveryRequestWindow
 * PSID          : 2565 (0x0A05)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  The TDLS FSM shall not transmit the TDLS Discovery Request frame within
 *  dot11TDLSDiscoveryRequestWindow DTIM intervals after transmitting TDLS
 *  Discovery Request frame.
 *******************************************************************************/
#define SLSI_PSID_DOT11_TDLS_DISCOVERY_REQUEST_WINDOW 0x0A05

/*******************************************************************************
 * NAME          : Dot11TdlsResponseTimeout
 * PSID          : 2566 (0x0A06)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 5
 * DESCRIPTION   :
 *  If no TDLS Setup Response frame is received within
 *  dot11TDLSResponseTimeout, or if a TDLS Setup Response frame is received
 *  with a nonzero status code, the TDLS initiator STA shall terminate the
 *  setup procedure and discard the TDLS Setup Response frame.
 *******************************************************************************/
#define SLSI_PSID_DOT11_TDLS_RESPONSE_TIMEOUT 0x0A06

/*******************************************************************************
 * NAME          : Dot11TdlsChannelSwitchActivated
 * PSID          : 2567 (0x0A07)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  If dot11TDLSChannelSwitchActivated is TRUE, it need to send TDLS channel
 *  switch response packet as corresponeding
 *******************************************************************************/
#define SLSI_PSID_DOT11_TDLS_CHANNEL_SWITCH_ACTIVATED 0x0A07

/*******************************************************************************
 * NAME          : UnifiTdlsDesignForTestMode
 * PSID          : 2568 (0x0A08)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 0X00000000
 * DESCRIPTION   :
 *  This MIB shall be used to set TDLS design for test mode
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TDLS_DESIGN_FOR_TEST_MODE 0x0A08

/*******************************************************************************
 * NAME          : UnifiTdlsKeyLifeTimeInterval
 * PSID          : 2577 (0x0A11)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 0X000FFFFF
 * DESCRIPTION   :
 *  This MIB shall be used to build the Key Lifetime Interval in the TDLS
 *  Setup Request frame.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TDLS_KEY_LIFE_TIME_INTERVAL 0x0A11

/*******************************************************************************
 * NAME          : UnifiOxygenDesignForTestMode
 * PSID          : 2583 (0x0A17)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 0X00000000
 * DESCRIPTION   :
 *  This is only used for the test purpose that can verify the requirements
 *  of IBSS/OXYGEN feature
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OXYGEN_DESIGN_FOR_TEST_MODE 0x0A17

/*******************************************************************************
 * NAME          : UnifiChannelAnnouncementCount
 * PSID          : 2584 (0x0A18)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  This is the Channel switch announcement count which will be used in the
 *  Channel announcement IE
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CHANNEL_ANNOUNCEMENT_COUNT 0x0A18

/*******************************************************************************
 * NAME          : UnifiRaTestStoredSa
 * PSID          : 2585 (0x0A19)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0X00000000
 * DESCRIPTION   :
 *  It is source address of router assuming that is contained in virtural
 *  router advertisement packet This mib is only used for the test purpose,
 *  that is, specified in chapter '6.2 Forward Received RA frame to Host' in
 *  SC-506393-TE
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RA_TEST_STORED_SA 0x0A19

/*******************************************************************************
 * NAME          : UnifiRaTestStoreFrame
 * PSID          : 2586 (0x0A1A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0X00000000
 * DESCRIPTION   :
 *  It is virtual router advertisement packet This mib is only used for the
 *  test purpose, that is, specified in chapter '6.2 Forward Received RA
 *  frame to Host' in SC-506393-TE
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RA_TEST_STORE_FRAME 0x0A1A

/*******************************************************************************
 * NAME          : Dot11TdlsPeerUapsdBufferStaActivated
 * PSID          : 2587 (0x0A1B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  This attribute, when true, indicates that the STA implementation is
 *  capable of supporting TDLS peer U-APSD.
 *******************************************************************************/
#define SLSI_PSID_DOT11_TDLS_PEER_UAPSD_BUFFER_STA_ACTIVATED 0x0A1B

/*******************************************************************************
 * NAME          : UnifiPrivateBbbTxFilterConfig
 * PSID          : 4071 (0x0FE7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 0X17
 * DESCRIPTION   :
 *  This MIB entry is written directly to the BBB_TX_FILTER_CONFIG register.
 *  Only the lower eight bits of this register are implemented . Bits 0-3 are
 *  the &apos;Tx Gain&apos;, bits 6-8 are the &apos;Tx Delay&apos;. This
 *  register should only be changed by an expert.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PRIVATE_BBB_TX_FILTER_CONFIG 0x0FE7

/*******************************************************************************
 * NAME          : UnifiPrivateSwagcFrontEndGain
 * PSID          : 4075 (0x0FEB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       :
 * DESCRIPTION   :
 *  Gain of the path between chip and antenna when LNA is on.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PRIVATE_SWAGC_FRONT_END_GAIN 0x0FEB

/*******************************************************************************
 * NAME          : UnifiPrivateSwagcFrontEndLoss
 * PSID          : 4076 (0x0FEC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       :
 * DESCRIPTION   :
 *  Loss of the path between chip and antenna when LNA is off.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PRIVATE_SWAGC_FRONT_END_LOSS 0x0FEC

/*******************************************************************************
 * NAME          : UnifiPrivateSwagcExtThresh
 * PSID          : 4077 (0x0FED)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       : -25
 * DESCRIPTION   :
 *  Signal level at which external LNA will be used for AGC purposes.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PRIVATE_SWAGC_EXT_THRESH 0x0FED

/*******************************************************************************
 * NAME          : UnifiRxAgcControl
 * PSID          : 4079 (0x0FEF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 11
 * MAX           : 11
 * DEFAULT       :
 * DESCRIPTION   :
 *  This attribute is used to override the AGC by adjusting the Rx minimum
 *  and maximum gains of each stage. Set requests write the values to a
 *  static structure in mac/arch/maxwell/hal/halradio_agc.c. The saved values
 *  are written to the Jar register WLRF_RADIO_AGC_CONFIG2 and to the Night
 *  registers WL_RADIO_AGC_CONFIG2 and WL_RADIO_AGC_CONFIG3. The saved values
 *  are also used to configure the AGC whenever halradio_agc_setup() is
 *  called. Get requests read the values from the static structure in
 *  mac/arch/maxwell/hal/halradio_agc.c. AGC enables are not altered. Fixed
 *  gain may be tested by setting the minimums and maximums to the same
 *  value. Version. octet 0 - Version number for this mib. Gain values.
 *  Default in brackets. octet 1 - 5G LNA minimum gain (0). octet 2 - 5G LNA
 *  maximum gain (4). octet 3 - 2G LNA minimum gain (0). octet 4 - 2G LNA
 *  maximum gain (5). octet 5 - Mixer minimum gain (0). octet 6 - Mixer
 *  maximum gain (2). octet 7 - ABB minimum gain (0). octet 8 - ABB maximum
 *  gain (27). octet 9 - Digital minimum gain (0). octet 10 - Digital maximum
 *  gain (7).
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RX_AGC_CONTROL 0x0FEF

/*******************************************************************************
 * NAME          : UnifiRaaSpeculationInterval
 * PSID          : 4140 (0x102C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 100
 * DESCRIPTION   :
 *  For Rate Adaptation Algorithm. It defines the repeatable amount of time,
 *  in ms, that firmware will start to send speculation frames.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RAA_SPECULATION_INTERVAL 0x102C

/*******************************************************************************
 * NAME          : UnifiRaaSpeculationPeriod
 * PSID          : 4141 (0x102D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 10
 * DESCRIPTION   :
 *  For Rate Adaptation Algorithm. It defines the max amount of time, in ms,
 *  that firmware will use for sending speculation frames
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RAA_SPECULATION_PERIOD 0x102D

/*******************************************************************************
 * NAME          : UnifiRaaNumbSpeculationFrames
 * PSID          : 4142 (0x102E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 9
 * DESCRIPTION   :
 *  For Rate Adaptation Algorithm. It defines the max amount of speculation
 *  frames that firmware is allowed to send.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RAA_NUMB_SPECULATION_FRAMES 0x102E

/*******************************************************************************
 * NAME          : UnifiRaaTxSuccessesCount
 * PSID          : 4143 (0x102F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  For Rate Adaptation Algorithm. It represents the number of transmitted
 *  frames that were acked at a given rate.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RAA_TX_SUCCESSES_COUNT 0x102F

/*******************************************************************************
 * NAME          : UnifiRaaTxFailuresCount
 * PSID          : 4144 (0x1030)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  For Rate Adaptation Algorithm. It represents the number of transmitted
 *  frames that were NOT acked at a given rate.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RAA_TX_FAILURES_COUNT 0x1030

/*******************************************************************************
 * NAME          : UnifiRaaTxPer
 * PSID          : 4145 (0x1031)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  For Rate Adaptation Algorithm. It represents the Packet Error Rate for a
 *  given rate on the RAA rate stats.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RAA_TX_PER 0x1031

/*******************************************************************************
 * NAME          : UnifiRaaResetStats
 * PSID          : 4146 (0x1032)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  For Rate Adaptation Algorithm. It Resets the stats table used by the RAA.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RAA_RESET_STATS 0x1032

/*******************************************************************************
 * NAME          : UnifiRaaTxMtPer
 * PSID          : 4147 (0x1033)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  For Rate Adaptation Algorithm. It represents the Maximum Tolerable Packet
 *  Error Rate for each rate.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RAA_TX_MT_PER 0x1033

/*******************************************************************************
 * NAME          : UnifiRaaTxHostRate
 * PSID          : 4148 (0x1034)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 16385
 * DESCRIPTION   :
 *  This MIB is use for host to set a fixed TX rate. Ideally this should be
 *  done by the driver, but since there isn't support for it yet, the best
 *  solution is to set it through this MIB. Default is 0 so that the getter
 *  of this MIB nows that this means "host did not specified any rate".
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RAA_TX_HOST_RATE 0x1034

/*******************************************************************************
 * NAME          : UnifiFallbackShortFrameRetryDistribution
 * PSID          : 4149 (0x1035)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 5
 * MAX           : 5
 * DEFAULT       : {0X01, 0X01, 0X01, 0X01, 0X00}
 * DESCRIPTION   :
 *  Configure the retry distribution for fallback for short frames octet 0 -
 *  Number of retries for starting rate. octet 1 - Number of retries for next
 *  rate. octet 2 - Number of retries for next rate. octet 3 - Number of
 *  retries for next rate. octet 4 - Number of retries for last rate. If 0 is
 *  written to an entry then the retries for that rate will be the short
 *  retry limit minus the sum of the retries for each rate above that entry
 *  (e.g. 15 - 5). Therefore, this should always be the value for octet 4.
 *  Also, when the starting rate has short guard enabled, the number of
 *  retries in octet 1 will be used and for the next rate in the fallback
 *  table (same MCS value, but with sgi disabled) octet 0 number of retries
 *  will be used.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FALLBACK_SHORT_FRAME_RETRY_DISTRIBUTION 0x1035

/*******************************************************************************
 * NAME          : UnifiDebugModuleControl
 * PSID          : 5029 (0x13A5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  Debug Module levels for all modules. Module debug level is used to filter
 *  debug messages sent to the host. Only 6 levels of debug per module are
 *  available: a. -1 No debug created. b. 0 Debug if compiled in. Should not
 *  cause Buffer Full in normal testing. c. 1 - 3 Levels to allow sensible
 *  setting of the .hcf file while running specific tests or debugging d. 4
 *  Debug will harm normal execution due to excessive levels or processing
 *  time required. Only used in emergency debugging. Additional control for
 *  FSM transition and FSM signals logging is provided. Debug module level
 *  and 2 boolean flags are encoded within a uint16: Function | Is sending
 *  FSM signals | Is sending FSM transitions | Reseved | Module level (signed
 *  int)
 *  -_-_-_-_-_+-_-_-_-_-_-_-_-_-_-_-_-_-_+-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_+-_-_-_-_-_-+-_-_-_-_-_-_-_-_-_-_-_-_-_-_ Bits | 15 | 14 | 13 - 8 | 7 - 0 Note: 0x00FF disables any debug for a module 0xC004 enables all debug for a module
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DEBUG_MODULE_CONTROL 0x13A5

/*******************************************************************************
 * NAME          : UnifiTxUsingLdpcEnabled
 * PSID          : 5030 (0x13A6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  This attribute, when TRUE, indicates that LDPC will be used to code
 *  packets, for transmit only. If set to FALSE, chip will not send LDPC
 *  coded packets even if peer supports it. To advertise reception of LDPC
 *  coded packets,enable bit 0 of unifiHtCapabilities, and bit 4 of
 *  unifiVhtCapabilities.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_USING_LDPC_ENABLED 0x13A6

/*******************************************************************************
 * NAME          : UnifiTxSettings
 * PSID          : 5031 (0x13A7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Hardware specific transmitter settings
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_SETTINGS 0x13A7

/*******************************************************************************
 * NAME          : UnifiTxGainSettings
 * PSID          : 5032 (0x13A8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Hardware specific transmitter gain settings
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_GAIN_SETTINGS 0x13A8

/*******************************************************************************
 * NAME          : UnifiTxAntennaConnectionLossFrequency
 * PSID          : 5033 (0x13A9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 3940
 * MAX           : 12000
 * DEFAULT       :
 * DESCRIPTION   :
 *  The corresponding set of frequency values for
 *  TxAntennaConnectionLossTable
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_ANTENNA_CONNECTION_LOSS_FREQUENCY 0x13A9

/*******************************************************************************
 * NAME          : UnifiTxAntennaConnectionLoss
 * PSID          : 5034 (0x13AA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       :
 * DESCRIPTION   :
 *  The set of Antenna Connection Loss value, which is used for TPO/EIRP
 *  conversion
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_ANTENNA_CONNECTION_LOSS 0x13AA

/*******************************************************************************
 * NAME          : UnifiTxAntennaMaxGainFrequency
 * PSID          : 5035 (0x13AB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 3940
 * MAX           : 12000
 * DEFAULT       :
 * DESCRIPTION   :
 *  The corresponding set of frequency values for TxAntennaMaxGain
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_ANTENNA_MAX_GAIN_FREQUENCY 0x13AB

/*******************************************************************************
 * NAME          : UnifiTxAntennaMaxGain
 * PSID          : 5036 (0x13AC)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       :
 * DESCRIPTION   :
 *  The set of Antenna Max Gain value, which is used for TPO/EIRP conversion
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_ANTENNA_MAX_GAIN 0x13AC

/*******************************************************************************
 * NAME          : UnifiRxExternalGainFrequency
 * PSID          : 5037 (0x13AD)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 3940
 * MAX           : 12000
 * DEFAULT       :
 * DESCRIPTION   :
 *  The set of RSSI offset value
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RX_EXTERNAL_GAIN_FREQUENCY 0x13AD

/*******************************************************************************
 * NAME          : UnifiRxExternalGain
 * PSID          : 5038 (0x13AE)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       :
 * DESCRIPTION   :
 *  The table giving frequency-dependent RSSI offset value
 *******************************************************************************/
#define SLSI_PSID_UNIFI_RX_EXTERNAL_GAIN 0x13AE

/*******************************************************************************
 * NAME          : UnifiTxPowerAdjustFrequency
 * PSID          : 5049 (0x13B9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 4800
 * MAX           : 12000
 * DEFAULT       :
 * DESCRIPTION   :
 *  Frequency reference point for a row in unifiTxPowerAdjustTable, specified
 *  in 500 kHz units.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_POWER_ADJUST_FREQUENCY 0x13B9

/*******************************************************************************
 * NAME          : UnifiTxPowerAdjustTemperature
 * PSID          : 5050 (0x13BA)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       :
 * DESCRIPTION   :
 *  Temperature reference point for a row in unifiTxPowerAdjustTable,
 *  specified in degrees Celsius
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_POWER_ADJUST_TEMPERATURE 0x13BA

/*******************************************************************************
 * NAME          : UnifiTxPowerAdjustDelta
 * PSID          : 5051 (0x13BB)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -128
 * MAX           : 127
 * DEFAULT       :
 * DESCRIPTION   :
 *  Delta adjustment in quarter dB for a row in unifiTxPowerAdjustTable.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_POWER_ADJUST_DELTA 0x13BB

/*******************************************************************************
 * NAME          : UnifiTxPowerDetectorResponse
 * PSID          : 5055 (0x13BF)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Hardware specific transmitter detector response settings
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_POWER_DETECTOR_RESPONSE 0x13BF

/*******************************************************************************
 * NAME          : UnifiTxDetectorTemperatureCompensation
 * PSID          : 5056 (0x13C0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Hardware specific transmitter detector temperature compensation settings
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_DETECTOR_TEMPERATURE_COMPENSATION 0x13C0

/*******************************************************************************
 * NAME          : UnifiTxDetectorFrequencyCompensation
 * PSID          : 5057 (0x13C1)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Hardware specific transmitter detector frequency compensation settings
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_DETECTOR_FREQUENCY_COMPENSATION 0x13C1

/*******************************************************************************
 * NAME          : UnifiTxOpenLoopTemperatureCompensation
 * PSID          : 5058 (0x13C2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Hardware specific transmitter open-loop temperature compensation settings
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_OPEN_LOOP_TEMPERATURE_COMPENSATION 0x13C2

/*******************************************************************************
 * NAME          : UnifiTxOpenLoopFrequencyCompensation
 * PSID          : 5059 (0x13C3)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 255
 * DEFAULT       :
 * DESCRIPTION   :
 *  Hardware specific transmitter open-loop frequency compensation settings
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_OPEN_LOOP_FREQUENCY_COMPENSATION 0x13C3

/*******************************************************************************
 * NAME          : UnifiTxOfdmSelect
 * PSID          : 5060 (0x13C4)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 4
 * MAX           : 4
 * DEFAULT       :
 * DESCRIPTION   :
 *  Hardware specific transmitter OFDM selection settings.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_OFDM_SELECT 0x13C4

/*******************************************************************************
 * NAME          : UnifiTxDigGain
 * PSID          : 5061 (0x13C5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 16
 * MAX           : 16
 * DEFAULT       :
 * DESCRIPTION   :
 *  This attribute used to specify gain specific modulation power
 *  optimisation.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_DIG_GAIN 0x13C5

/*******************************************************************************
 * NAME          : UnifiChipTemperature
 * PSID          : 5062 (0x13C6)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : celcius
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       :
 * DESCRIPTION   :
 *  This attribute used to read the chip temperature as seen by WLAN radio
 *  firmware
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CHIP_TEMPERATURE 0x13C6

/*******************************************************************************
 * NAME          : UnifiBatteryVoltage
 * PSID          : 5063 (0x13C7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * UNITS         : millivolt
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  This attribute used to read the battery voltage
 *******************************************************************************/
#define SLSI_PSID_UNIFI_BATTERY_VOLTAGE 0x13C7

/*******************************************************************************
 * NAME          : UnifiTxOobConstraints
 * PSID          : 5064 (0x13C8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  OOB constraints table. | octects | description |
 * |-_-_-_-_-+-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-| | 0-1 | Hardware platform. Check unifiHardwarePlatform enum for possible values. | | 2 | DPD applicability bitmask: 0 = no DPD, 1 = dynamic DPD, 2 = static DPD, 3 = applies to both static and dynamic DPD | | 3-4 | Bitmask indicating which regulatory domains this rule applies to FCC=bit0, ETSI=bit1, JAPAN=bit2 | | 5-6 | Bitmask indicating which band edges this rule applies to RICE_BAND_EDGE_ISM_24G_LOWER = bit 0, RICE_BAND_EDGE_ISM_24G_UPPER = bit 1, RICE_BAND_EDGE_U_NII_1_LOWER = bit 2, RICE_BAND_EDGE_U_NII_1_UPPER = bit 3, RICE_BAND_EDGE_U_NII_2_LOWER = bit 4, RICE_BAND_EDGE_U_NII_2_UPPER = bit 5, RICE_BAND_EDGE_U_NII_2E_LOWER = bit 6, RICE_BAND_EDGE_U_NII_2E_UPPER = bit 7, RICE_BAND_EDGE_U_NII_3_LOWER = bit 8, RICE_BAND_EDGE_U_NII_3_UPPER = bit 9 | | 7 | Bitmask indicating which modulation types this rule applies to (LSB/b0=DSSS/CCK, b1= OFDM0 modulation group, b2= OFDM1 modulation group) | | 7 | Bitmask indicating which channel bandwidths this rule applies to (LSB/b0=20MHz, b1=40MHz, b2=80MHz) | | 8 | Minimum distance to nearest band edge in 500 kHz units for which this constraint becomes is applicable. | | 10 | Maximum power (EIRP) for this particular constraint - specified in units of quarter dBm. | | 11-12 | Spectral shaping configuration to be used for this particular constraint. The value is specific to the radio hardware and should only be altered under advice from the IC supplier. | |
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_OOB_CONSTRAINTS 0x13C8

/*******************************************************************************
 * NAME          : UnifiTxPowerTrimConfig
 * PSID          : 5072 (0x13D0)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 13
 * MAX           : 13
 * DEFAULT       :
 * DESCRIPTION   :
 *  This attribute used to specify configuration settings for the TX power
 *  trim. The format is [band, psat_v2i_gain, psat_mix_gain, psat_drv_gain,
 *  psat_pa_gain, psat_power_ref_mso, psat_power_ref_lso, psat_drv_bias,
 *  psat_pa_bias, max_adjust_up_mso, max_adjust_up_lso, max_adjust_down_mso,
 *  max_adjust_down_lso].
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TX_POWER_TRIM_CONFIG 0x13D0

/*******************************************************************************
 * NAME          : UnifiForceShortSlotTime
 * PSID          : 5080 (0x13D8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  If set to true, forces the UniFi chip to always use short slot times for
 *  all VIFs.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_FORCE_SHORT_SLOT_TIME 0x13D8

/*******************************************************************************
 * NAME          : UnifiCurrentTxpowerLevel
 * PSID          : 6020 (0x1784)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : qdBm
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       :
 * DESCRIPTION   :
 *  This attribute shall indicate the maximum air power for the VIF,
 *  currently used. Values are expressed in 0.25 dBm units.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CURRENT_TXPOWER_LEVEL 0x1784

/*******************************************************************************
 * NAME          : UnifiUserSetTxpowerLevel
 * PSID          : 6021 (0x1785)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * UNITS         : qdBm
 * MIN           : 0
 * MAX           : 127
 * DEFAULT       : 127
 * DESCRIPTION   :
 *  Maximum air power for the VIF, set by the user. Values are expressed in
 *  0.25 dBm units.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_USER_SET_TXPOWER_LEVEL 0x1785

/*******************************************************************************
 * NAME          : UnifiTpcMaxPowerRssiThreshold
 * PSID          : 6022 (0x1786)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       : -55
 * DESCRIPTION   :
 *  Below this RSSI(dBm) threshold, firmware will switch to max power allowed
 *  by current regulatory. If it has been previously reduced due to
 *  unifiTPCMinPowerRSSIThreshold. Applies to VIF of type STA (including P2P
 *  Client)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TPC_MAX_POWER_RSSI_THRESHOLD 0x1786

/*******************************************************************************
 * NAME          : UnifiTpcMinPowerRssiThreshold
 * PSID          : 6023 (0x1787)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       : -45
 * DESCRIPTION   :
 *  Above this RSSI(dBm) threshold, firmware will switch to minimum power
 *  that our hardware can support - provided it is lower than current
 *  regulatory limit. Setting it to zero disables the MIB and revert the
 *  power to default state (as specified by regulatory). Applies to VIF of
 *  type STA (including P2P Client)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TPC_MIN_POWER_RSSI_THRESHOLD 0x1787

/*******************************************************************************
 * NAME          : UnifiTpcMinPower2g
 * PSID          : 6024 (0x1788)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       : 52
 * DESCRIPTION   :
 *  Minimun power to use at 2.4GHz interface when RSSI is above
 *  unifiTPCMinPowerRSSIThreshold. Specified in quarter dbm. Its value should
 *  be higher than dot11PowerCapabilityMinImplemented. Applies to VIF of type
 *  STA (including P2P Client)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TPC_MIN_POWER2G 0x1788

/*******************************************************************************
 * NAME          : UnifiTpcMinPower5g
 * PSID          : 6025 (0x1789)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : -32768
 * MAX           : 32767
 * DEFAULT       : 40
 * DESCRIPTION   :
 *  Minimun power to use at 5 GHz interface when RSSI is above
 *  unifiTPCMinPowerRSSIThreshold. Specified in quarter dbm. Its value should
 *  be higher than dot11PowerCapabilityMinImplemented Applies to VIF of type
 *  STA (including P2P Client)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_TPC_MIN_POWER5G 0x1789

/*******************************************************************************
 * NAME          : UnifiSarBackoff
 * PSID          : 6026 (0x178A)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : 0
 * MAX           : 100
 * DEFAULT       :
 * DESCRIPTION   :
 *  SAR backoff values in 1/4 dBm.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SAR_BACKOFF 0x178A

/*******************************************************************************
 * NAME          : UnifiSarBackoffEnable
 * PSID          : 6027 (0x178B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       :
 * DESCRIPTION   :
 *  SAR Backoff feature hook.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SAR_BACKOFF_ENABLE 0x178B

/*******************************************************************************
 * NAME          : UnifiCcxSupportedVersion
 * PSID          : 6030 (0x178E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  This value enables/disables CCX to specified version. Initialised on
 *  system boot. Possible values: 0-_>No CCX supported 1-_>CCX v4 supported
 *  2-_>CCX v6 (Lite) version supported 3-_>CCXv4 and CCXv6 versions
 *  supported.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CCX_SUPPORTED_VERSION 0x178E

/*******************************************************************************
 * NAME          : UnifiCcxLiteFoundationSupportedServices
 * PSID          : 6031 (0x178F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 1
 * MAX           : 2
 * DEFAULT       : 2
 * DESCRIPTION   :
 *  This value should show which version of CCX Lite Foundation services is
 *  supported by the device. Only valid if CCX Lite supported. Possible
 *  values: 1-_>CCX Lite Foundation Services v1 supported 2-_>CCX Lite
 *  Foundation Services v2 supported (v2 includes v1 as well)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CCX_LITE_FOUNDATION_SUPPORTED_SERVICES 0x178F

/*******************************************************************************
 * NAME          : UnifiCcxLiteVoiceSupportedServices
 * PSID          : 6032 (0x1790)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 1
 * DESCRIPTION   :
 *  This value should show which version of of CCX Lite Voice services is
 *  supported by the device. Only valid if CCX Lite supported. Possible
 *  values: 0-_>CCX Lite Voice not supported 1-_>CCX Lite Voice Services v1
 *  supported 2-_>CCX Lite Voice Services v2 supported (v2 includes v1 as
 *  well)
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CCX_LITE_VOICE_SUPPORTED_SERVICES 0x1790

/*******************************************************************************
 * NAME          : UnifiCcxLiteLocationSupportedServices
 * PSID          : 6033 (0x1791)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       :
 * DESCRIPTION   :
 *  This value should show which version of of CCX Lite Location services is
 *  supported by the device. Only valid if CCX Lite supported. Possible
 *  values: 0-_>CCX Lite Location not supported 1-_>CCX Lite Location
 *  Services v1 supported
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CCX_LITE_LOCATION_SUPPORTED_SERVICES 0x1791

/*******************************************************************************
 * NAME          : UnifiCcxVoiceFailureThreshold
 * PSID          : 6034 (0x1792)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint16
 * MIN           : 0
 * MAX           : 65535
 * DEFAULT       : 3
 * DESCRIPTION   :
 *  If more than this number of CCX Voice frames fail consecutively then a
 *  failure will be reported to MLME, which may cause roaming.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_CCX_VOICE_FAILURE_THRESHOLD 0x1792

/*******************************************************************************
 * NAME          : UnifiPmfAssociationComebackTimeDelta
 * PSID          : 6050 (0x17A2)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * UNITS         : TU
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 1100
 * DESCRIPTION   :
 *  This MIB indicates a delta time for the assocication comeback time
 *  element in the SA Query request frame. The association comeback time in
 *  the SA Query request frame will be set to TSF +
 *  unifiPMFAssociationComebackTimeDelta.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_PMF_ASSOCIATION_COMEBACK_TIME_DELTA 0x17A2

/*******************************************************************************
 * NAME          : UnifiDebugInstantDelivery
 * PSID          : 6069 (0x17B5)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : FALSE
 * DESCRIPTION   :
 *  Debug to host state. Debug is either is sent to the host or it isn't.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DEBUG_INSTANT_DELIVERY 0x17B5

/*******************************************************************************
 * NAME          : UnifiDebugEnable
 * PSID          : 6071 (0x17B7)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Debug to host state. Debug is either is sent to the host or it isn't.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DEBUG_ENABLE 0x17B7

/*******************************************************************************
 * NAME          : UnifiDefaultGlobalDebugEnabled
 * PSID          : 6072 (0x17B8)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Needs to be removed!.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DEFAULT_GLOBAL_DEBUG_ENABLED 0x17B8

/*******************************************************************************
 * NAME          : UnifiDPlaneDebug
 * PSID          : 6073 (0x17B9)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       : 0X3
 * DESCRIPTION   :
 *  Bit mask for turning on individual debug entities in the data_plane that
 *  if enabled effect throughput. See DPLP_DEBUG_ENTITIES_T in
 *  dplane_dplp_debug.h for bits. Default of 0x3 means dplp and ampdu logs
 *  are enabled.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DPLANE_DEBUG 0x17B9

/*******************************************************************************
 * NAME          : UnifiRegulatoryParameters
 * PSID          : 8011 (0x1F4B)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 3
 * MAX           : 73
 * DEFAULT       :
 * DESCRIPTION   :
 *  Regulatory parameters. Each row of the table contains the regulatory
 *  rules for one country: octet 0 - first character of alpha2 code for
 *  country octet 1 - second character of alpha2 code for country octet 2 -
 *  regulatory domain for the country Followed by the rules for the country,
 *  numbered 0..n in this description octet 7n+3 - LSB start frequency octet
 *  7n+4 - MSB start frequency octet 7n+5 - LSB end frequency octet 7n+6 -
 *  MSB end frequency octet 7n+7 - maximum bandwidth octet 7n+8 - maximum
 *  power octet 7n+9 - rule flags
 *******************************************************************************/
#define SLSI_PSID_UNIFI_REGULATORY_PARAMETERS 0x1F4B

/*******************************************************************************
 * NAME          : UnifiSupportedChannels
 * PSID          : 8012 (0x1F4C)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 0
 * MAX           : 20
 * DEFAULT       :  {0X01,0X0E,0X24,0X04,0X34,0X04,0X64,0X0C,0X95,0X05}
 * DESCRIPTION   :
 *  Supported 20MHz channel centre frequency grouped in sub-bands. For each
 *  sub-band: starting channel number, followed by number of channels
 *******************************************************************************/
#define SLSI_PSID_UNIFI_SUPPORTED_CHANNELS 0x1F4C

/*******************************************************************************
 * NAME          : UnifiDefaultCountry
 * PSID          : 8013 (0x1F4D)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 3
 * MAX           : 3
 * DEFAULT       : {0X30, 0X30, 0X20}
 * DESCRIPTION   :
 *  Allows setting of default country code. Hosts sets the default country
 *  (index 1). Each VIF maintains current country code and updates it, but
 *  never reads. The host can access currently used country code of each VIF,
 *  but can't modify it.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_DEFAULT_COUNTRY 0x1F4D

/*******************************************************************************
 * NAME          : UnifiCountryList
 * PSID          : 8014 (0x1F4E)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 2
 * MAX           : 500
 * DEFAULT       : (Too Large to display)
 * DESCRIPTION   :
 *  Defines the ordered list of countries present in unifiRegulatoryTable.
 *  Each country is coded as 2 ASCII characters. If unifiRegulatoryTable is
 *  modified, such as a country is either added, deleted or its relative
 *  location is modified, this MIB has to be updated as well.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_COUNTRY_LIST 0x1F4E

/*******************************************************************************
 * NAME          : UnifiOperatingClassParamters
 * PSID          : 8015 (0x1F4F)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 6
 * MAX           : 73
 * DEFAULT       :
 * DESCRIPTION   :
 *  Operating Class parameters. Each row of the table contains the regulatory
 *  rules for one country: octet 0 - for Region Cone octet 1 - for Operating
 *  Class ID octet 2 ~ 3 - for Channel Starting Frequency octet 4 - for
 *  Channel Spacing octet 5 - for Number of Elements in Channel Set octet n -
 *  for Channel Set octet end - for Behavior Limits Set
 *******************************************************************************/
#define SLSI_PSID_UNIFI_OPERATING_CLASS_PARAMTERS 0x1F4F

/*******************************************************************************
 * NAME          : UnifiNoCellMaxPowerEna
 * PSID          : 8016 (0x1F50)
 * PER INTERFACE?: NO
 * TYPE          : SlsiBool
 * MIN           : 0
 * MAX           : 1
 * DEFAULT       : TRUE
 * DESCRIPTION   :
 *  Max power enable flag for selected channels indicated in
 *  unifiNoCellMaxPowerChannels.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NO_CELL_MAX_POWER_ENA 0x1F50

/*******************************************************************************
 * NAME          : UnifiNoCellMaxPower
 * PSID          : 8017 (0x1F51)
 * PER INTERFACE?: NO
 * TYPE          : SlsiInt16
 * MIN           : 5
 * MAX           : 20
 * DEFAULT       : 7
 * DESCRIPTION   :
 *  Max power values for channels indicated in unifiNoCellMaxPowerChannels.
 *  Power specified in dBm units.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NO_CELL_MAX_POWER 0x1F51

/*******************************************************************************
 * NAME          : UnifiNoCellMaxPowerChannels
 * PSID          : 8018 (0x1F52)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint8
 * MIN           : 8
 * MAX           : 8
 * DEFAULT       : { 0X00, 0X18, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00 }
 * DESCRIPTION   :
 *  Defines channels that are affected by the unifiNoCellMaxPowerEna control.
 *  These channels are defined using uint64 that is represented by the octet
 *  string. First byte of the octet string maps to LSB, where bit 0 maps to
 *  channel 1. Complete mapping coulbd be found in
 *  mlme_regulatory_freq_list[] array content in mlme_regulatory module.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_NO_CELL_MAX_POWER_CHANNELS 0x1F52

/*******************************************************************************
 * NAME          : UnifiReadReg
 * PSID          : 8051 (0x1F73)
 * PER INTERFACE?: NO
 * TYPE          : SlsiUint32
 * MIN           : 0
 * MAX           : 4294967295
 * DEFAULT       :
 * DESCRIPTION   :
 *  Read value from a register and return it.
 *******************************************************************************/
#define SLSI_PSID_UNIFI_READ_REG 0x1F73

#ifdef __cplusplus
}
#endif
#endif /* SLSI_MIB_H__ */
