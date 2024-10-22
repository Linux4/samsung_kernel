/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _NAN_EXT_ASCC_H_
#define _NAN_EXT_ASCC_H_

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */
#if CFG_SUPPORT_NAN
/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
enum NAN_ASCC_PACKET_TYPE {
	NAN_ASCC_PACKET_REQUEST = 0,
	NAN_ASCC_PACKET_RESPONSE = 1,
	NAN_ASCC_PACKET_CONFIRM = 2,
	/* 3: RESERVED */
};

enum NAN_SCHEDULE_METHOD {
	NAN_SCHEDULE_METHOD_FRAMEWORK_FORCED,
	NAN_SCHEDULE_METHOD_FRAMEWORK_SUGGESTED_SDF,
	NAN_SCHEDULE_METHOD_FRAMEWORK_SUGGESTED_FW,
	NAN_SCHEDULE_METHOD_FW_MANAGED,
};

enum NAN_SCHEDULE_USAGE {
	NAN_SCHEDULE_USAGE_GENERAL,	/* 0 */
	NAN_SCHEDULE_USAGE_CCM,		/* 1 */
	NAN_SCHEDULE_USAGE_FC,		/* 2 */
	NAN_SCHEDULE_USAGE_FR,		/* 3 */
	NAN_SCHEDULE_USAGE_APNAN,	/* 4 */
	NAN_SCHEDULE_USAGE_P2P_DISCOVERY,	/* 5 */
	NAN_SCHEDULE_USAGE_ASCC_PS,	/* 6 */
	NAN_SCHEDULE_USAGE_ANS,		/* 7, TODO: only up to 7 for event? */
	NAN_SCHEDULE_USAGE_APNAN_PS,	/* 8 */
	NAN_SCHEDULE_USAGE_ASCC_PS_FR,	/* 9 */
	NAN_SCHEDULE_USAGE_APNAN_PS_FR,	/* 10 */
	NAN_SCHEDULE_USAGE_CHECK_SCHEDULE,	/* 11 */
	NAN_SCHEDULE_USAGE_SD_USD,	/* 12 */
	NAN_SCHEDULE_USAGE_NDP_IDLE_TIMER,	/* 13 */
};

/**
 * 5 GHz preference: FW will try to use full 5G slots instead of using 6G
 * 6 GHz preference: if this sets 6 GHz preference, FW will try to use
 *	1) for 6G peers: half 6G and half 5G
 *	2) for 5G peers: half 5G and half 2G
 */
enum NAN_SCHEDULE_BAND_PREF {
	NAN_SCHEDULE_BAND_PREF_2G,
	NAN_SCHEDULE_BAND_PREF_5G,
	NAN_SCHEDULE_BAND_PREF_6G,
};

enum NAN_SCHEDULE_CATEGORY {
	CATEGORY_DW_2G,  /* 0 */		/* IE_NAN_ASCC_DW */
	CATEGORY_FC_2G,  /* 1 */		/* IE_NAN_ASCC_FAW */
	CATEGORY_NDP_FAW_2G,   /* 2 */		/* IE_NAN_ASCC_FAW */
	CATEGORY_DW_5G,  /* 3 */		/* IE_NAN_ASCC_DW */
	CATEGORY_FC_5G,  /* 4 */		/* IE_NAN_ASCC_FAW */
	CATEGORY_NDP_FAW_5G,  /* 5 */		/* IE_NAN_ASCC_FAW */
	CATEGORY_DW_6G,  /* 6 */		/* IE_NAN_ASCC_DW */
	CATEGORY_FC_6G,  /* 7 */		/* IE_NAN_ASCC_FAW */
	CATEGORY_NDP_FAW_6G,  /* 8 */		/* IE_NAN_ASCC_FAW */
	CATEGORY_AP_2G,  /* 9 */		/* IE_NAN_ASCC_FAW */
	CATEGORY_AP_5G,  /* 10 */		/* IE_NAN_ASCC_FAW */
	CATEGORY_AP_6G,  /* 11 */		/* IE_NAN_ASCC_FAW */
	CATEGORY_USD_2G, /* 12 */		/* IE_NAN_ASCC_USD */
	CATEGORY_USD_5G, /* 13 */		/* IE_NAN_ASCC_USD */
	CATEGORY_SD_2G,  /* 14 */		/* IE_NAN_ASCC_SD */
	CATEGORY_SD_5G,  /* 15 */		/* IE_NAN_ASCC_SD */
	CATEGORY_PASSIVE_SCAN,  /* 16 */	/* IE_NAN_ASCC_ULW */
	CATEGORY_FW_TRIGGERED_SCAN,  /* 17 */	/* IE_NAN_ASCC_FAW */
	CATEGORY_ULW_2G, /* 18 */		/* IE_NAN_ASCC_ULW */
	CATEGORY_ULW_5G, /* 19 */		/* IE_NAN_ASCC_ULW */
	CATEGORY_ULW_6G, /* 20 */		/* IE_NAN_ASCC_ULW */
	CATEGORY_DISCOVERY_2G, /* 21 */  /* IE_NAN_ASCC_DISCOVERY */
	CATEGORY_DISCOVERY_5G, /* 22 */  /* IE_NAN_ASCC_DISCOVERY */
	CATEGORY_DISCOVERY_6G, /* 23 */  /* IE_NAN_ASCC_DISCOVERY */
};

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/**
 * Common header of various schedule entry types,
 * dispatch to corresponding handler according to the schedule_category field
 */
struct IE_NAN_ASCC_SCHEDULE {
	uint8_t schedule_category:5, /* enum NAN_SCHEDULE_CATEGORY */
		operation:3; /* enum NAN_SCHEDULE_ENTRY_OPERATION */
} __KAL_ATTRIB_PACKED__;

/**
 * [v1.8.1] Table 5. AScC data
 *
 * @ucNanOui: Vendor specific OUI type, 0x3B = 59
 *
 * @u2Length: Length of this OUI data, counted after this field
 *
 * @ucMajorVersion: Version of STD+
 *
 * @ucMinorVersion: Version of STD+
 *
 * @ucRequestId: ID for distinguishing requested data
 *
 * @type: AScC packet type
 *   Ref: enum NAN_ASCC_PACKET_TYPE
 *
 * @scheduling_method: Scheduling control entity and methods
 *   Ref: enum NAN_SCHEDULE_METHOD
 *
 * @usage: Indicate specific use case of this AScC data
 *   Ref: enum NAN_SCHEDULE_USAGE
 *
 * @aucSrcNMIAddr: Specify the NMI of source of this private command
 *
 * @aucDestNMIAddr: Intended Peer NMI used for AP-NAN or Fast recovery.
 *		    It can be set as ‘broadcast’
 *		    If this is set to 0x516F9A010000 or 0xFFFFFFFFFFFF, the SDF
 *		    or NAF will be unicasted to all the discovered peer.
 *
 * @schedule_confirm_handshake_required: Devices can transmit data as soon as
 *					 they wakeup if this field is set 0
 *   BITS(7): Schedule confirm handshake required
 *	0: Not required (Assume wakeup schedule is optimal and consent)
 *	1: Required (Default)
 *
 * @data_transmission_start_timing: Explicit data transmission enable timing
 *   BITS(4, 6): Data transmission start timing
 *	000: Start immediately
 *	001: After a DW
 *	010: After 2 DWs
 *
 * @band_preference: Specify band preference when FW manages scheduling
 *   Ref: enum NAN_SCHEDULE_BAND_PREF
 *
 * @ndp_idle_timer_paused: Set NDP idle timer
 *   BIT(0): Set NDP idle timer
 *	0b0: Enable NDP idle timer (default)
 *	0b1: Disable NDP idle timer (Paused NDP idle timer, no disconnection)
 *   The default is 0, where there is NDP disconnection timer (usually 1 min).
 *   If this is set 1, the devices shall not disconnect each peer even the NDP
 *   is idle.
 *
 * @u4Category: Specify category of the corresponding schedule.
 *		If usage is 1011 (Schedule info request, this field means
 *		chedule category required)
 *   Ref: enum NAN_SCHEDULE_CATEGORY
 *
 * @aucEntry: For each schedule category, schedule entry shall be provided.
 */
struct IE_NAN_ASCC_CMD {
	uint8_t ucNanOui; /* NAN_ASCC_CMD_OUI */
	uint16_t u2Length;
	uint8_t ucMajorVersion;
	uint8_t ucMinorVersion;
	uint8_t ucRequestId;

	uint8_t type:2, /* enum NAN_ASCC_PACKET_TYPE */
		scheduling_method:2, /* enum NAN_SCHEDULE_METHOD */
		usage:4; /* enum NAN_SCHEDULE_USAGE */

	uint8_t aucSrcNMIAddr[MAC_ADDR_LEN];
	uint8_t aucDestNMIAddr[MAC_ADDR_LEN];

	uint8_t schedule_confirm_handshake_required:1,
		data_transmission_start_timing:3,
		band_preference:3, /* Ref: enum NAN_SCHEDULE_BAND_PREF */
		ndp_idle_timer_paused:1;

	uint32_t u4Category;
	uint8_t aucEntry[];
} __KAL_ATTRIB_PACKED__;

#define ASCC_CMD_HDR_LEN OFFSET_OF(struct IE_NAN_ASCC_CMD, ucMajorVersion)
#define ASCC_CMD_LEN(fp) (((struct IE_NAN_ASCC_CMD *)(fp))->u2Length)
#define ASCC_CMD_SIZE(fp) (ASCC_CMD_HDR_LEN + ASCC_CMD_LEN(fp))
#define ASCC_CMD_BODY_SIZE(fp)	\
	(ASCC_CMD_SIZE(fp) - sizeof(struct IE_NAN_ASCC_CMD))

enum NAN_SCHEDULE_ENTRY_OPERATION {
	NAN_SCHEDULE_ENTRY_ADD = 0,
	NAN_SCHEDULE_ENTRY_REMOVE = 1,
	NAN_SCHEDULE_ENTRY_OVERWRITE = 2,
	NAN_SCHEDULE_ENTRY_RESET = 3,
	NAN_SCHEDULE_ENTRY_NOT_CHANGE = 4,
	/* 5~6: Reserved */
	NAN_SCHEDULE_ENTRY_FAIL_TO_CHANGE = 7,
};

/**
 * [v1.8.1] Table 6. Schedule entry for DW (2.4 GHz DW, 5 GHz DW, 6 GHz DW)
 *
 * @schedule_category: Value of corresponding AScC schedule category field
 *   Ref: enum NAN_SCHEDULE_CATEGORY
 *   BITS(3, 7):
 *	0b00000(0d0): 2.4 GHz DW
 *	0b00011(0d3): 5 GHz DW
 *	0b00110(0d6): 6 GHz DW
 *
 * @operation: Ref enum NAN_SCHEDULE_ENTRY_OPERATION
 *
 * @start_offset: Start offset of this DW schedule with reference to DW 0 in
 *		  unit of 16 TU
 *   BITS(3, 7):
 *	0b00000: DW 0 (0 TU) - 2.4 GHz Default
 *	0b00001: 16 TUs
 *	0b00010: 32 TUs
 *	...
 *	0b01000: 128 TUs - 5 GHz Default
 *	...
 *	0b11111: 496 TUs - The last slot between DWST
 *
 * @reserved_1: This is for byte alignment
 *
 * @ucOpChannel: Indicate the corresponding DW channel number.
 *	0d001~0d014: Used for 2.4, 5, and 6 GHz channel number
 *	0d015~0d196: Used for 5 and 6 GHz channel number
 *	0d197~0d233: Used for 6 GHz channel number
 *	0d234~0d255: Reserved
 *	0d000: Not used
 *
 * @dw_wakeup_interval: DW wakeup interval
 *   BITS(4, 7):
 *	0b0000: = repeat every (512*2^(0-1) = 256 TU)
 *	0b0001: = repeat every (512*2^(1-1) = 512 TU)
 *	0b0010: = repeat every (512*2^(2-1) = 1024 TU)
 *	0b0011: = repeat every (512*2^(3-1) = 2048 TU)
 *	0b0100: = repeat every (512*2^(4-1) = 4096 TU)
 *	0b0101: = repeat every (512*2^(5-1) = 8192 TU)
 *	0b0110~0b1110: Reserved
 *	0b1111: Do not wake up at DW
 *
 * @discovery_beacon_enabled: Discovery beacon transmission enabled
 *   BIT(3):
 *	0b0: Do not transmit discovery beacon
 *	0b1: Transmit discovery beacon
 *
 * @reserved_2: This is for byte alignment
 */
struct IE_NAN_ASCC_DW {
	uint8_t schedule_category:5, /* enum NAN_SCHEDULE_CATEGORY */
		operation:3; /* enum NAN_SCHEDULE_ENTRY_OPERATION */

	uint8_t start_offset:5,
		reserved_1:3;

	uint8_t ucOpChannel;

	uint8_t dw_wakeup_interval:4,
		discovery_beacon_enabled:1,
		reserved_2:3;
} __KAL_ATTRIB_PACKED__;

/**
 * [v1.8.1] Table 7. Schedule entry for FAW (2.4/5/6 GHz NDP and FC)
 *
 * @schedule_category: Value of corresponding AScC schedule category field
 *   Ref: enum NAN_SCHEDULE_CATEGORY
 *   BITS(3, 7):
 *	0d1: 2.4 GHz Fast connect
 *	0d2: 2.4 GHz NDP
 *	0d4: 5 GHz FC
 *	0d5: 5 GHz NDP
 *	0d7: 6 GHz FC
 *	0d8: 6 GHz NDP
 *	0d9: 2.4 GHz AP
 *	0d10: 5 GHz AP
 *	0d11: 6 GHz AP
 *	0d17: FW triggered scan
 *
 * @operation: Ref enum NAN_SCHEDULE_ENTRY_OPERATION
 *
 * @u4Bitmap: Specify Schedule time bitmap allocation
 *	      0x00000000 ~ 0xFFFFFFFF, where each bit means presence of timing.
 *	      Notation will follow Wireshark based ordering, where it is LSB
 *	      order.
 *
 * @ucOpChannel: Indicate the corresponding DW channel number.
 *	0d001~0d014: Used for 2.4, 5, and 6 GHz channel number
 *	0d015~0d196: Used for 5 and 6 GHz channel number
 *	0d197~0d233: Used for 6 GHz channel number
 */
struct IE_NAN_ASCC_FAW {
	uint8_t schedule_category:5, /* enum NAN_SCHEDULE_CATEGORY */
		operation:3; /* enum NAN_SCHEDULE_ENTRY_OPERATION */

	union {
		uint32_t u4Bitmap;
		uint8_t byte[4];
	};

	uint8_t ucOpChannel;
} __KAL_ATTRIB_PACKED__;

/**
 * [v1.8.1] Table 8. Schedule entry for USD (2.4/5 GHz)
 *
 * @schedule_category: Value of corresponding AScC schedule category field
 *   Ref: enum NAN_SCHEDULE_CATEGORY
 *   BITS(3, 7):
 *	0d12: 2.4 GHz USD
 *	0d13: 5 GHz USD
 *
 * @operation: Ref enum NAN_SCHEDULE_ENTRY_OPERATION
 *
 * @u4Bitmap: Specify Schedule time bitmap allocation
 *	      0x00000000 ~ 0xFFFFFFFF, where each bit means presence of timing.
 *	      Notation will follow Wireshark based ordering, where it is LSB
 *	      order.
 *
 * @ucOpChannel: Indicate the corresponding DW channel number.
 *	0d001~0d014: Used for 2.4, 5, and 6 GHz channel number
 *	0d015~0d196: Used for 5 and 6 GHz channel number
 *	0d197~0d233: Used for 6 GHz channel number
 *
 * @ucUsdTimeout: Indicate timeout of SD in second
 *	0d0: Terminate USD
 *	0d1~254: USD timer in second
 *	0d255: Keep USD on continuously (No timeout and not proceed to SD)
 *
 * @number_unsoliciated_sdf: number of unsolicited SDF messages during
 *			     USD period (at an interval)
 *   BITS(4, 7):
 *	0: Do not send message
 *	1 (= 1): Send a single message
 *	1111 (= 15): Send 15 sequential messages
 *
 * @sd_interval: At a SD interval, NAN device shall repeat SD period scheduled
 *		 through schedule entry of SD.
 *   BITS(0, 3):
 *	0b0000: = repeat every (512*2^(0-1) = 256 TU)
 *	0b0001: = repeat every (512*2^(1-1) = 512 TU)
 *	0b0010: = repeat every (512*2^(2-1) = 1024 TU)
 *	0b0011: = repeat every (512*2^(3-1) = 2048 TU)
 *	0b0100: = repeat every (512*2^(4-1) = 4096 TU)
 *	0b0101: = repeat every (512*2^(5-1) = 8192 TU)
 *	0b0110~0b1110: Reserved
 *	0b1111: Do not wake up at DW
 */
struct IE_NAN_ASCC_USD {
	uint8_t schedule_category:5, /* enum NAN_SCHEDULE_CATEGORY */
		operation:3; /* enum NAN_SCHEDULE_ENTRY_OPERATION */

	union {
		uint32_t u4Bitmap;
		uint8_t byte[4];
	};

	uint8_t ucOpChannel;

	uint8_t ucUsdTimeout;

	uint8_t number_unsoliciated_sdf:4,
		usd_interval:4;
} __KAL_ATTRIB_PACKED__;

/**
 * [v1.8.1] Table 9. Schedule entry for SD (2.4/5 GHz)
 *
 * @schedule_category: Value of corresponding AScC schedule category field
 *   Ref: enum NAN_SCHEDULE_CATEGORY
 *   BITS(3, 7):
 *	0d14: 2.4 GHz SD
 *	0d15: 5 GHz SD
 *
 * @operation: Ref enum NAN_SCHEDULE_ENTRY_OPERATION
 *
 * @u4Bitmap: Specify Schedule time bitmap allocationSpecify Schedule time
 *	      bitmap allocation
 *	      0x00000000 ~ 0xFFFFFFFF, where each bit means presence of timing.
 *	      Notation will follow Wireshark based ordering, where it is LSB
 *	      order.
 *
 * @ucOpChannel: Indicate the corresponding DW channel number.
 *	0d001~0d014: Used for 2.4, 5, and 6 GHz channel number
 *	0d015~0d196: Used for 5 and 6 GHz channel number
 *	0d197~0d233: Used for 6 GHz channel number
 *	0d234~0d255: Reserved
 *	0d000: Not used
 *
 * @number_unsoliciated_sdf: number of unsolicited SDF messages during
 *			     USD period (at an interval)
 *   BITS(4, 7):
 *	0: Do not send message
 *	1 (= 1): Send a single message
 *	1111 (= 15): Send 15 sequential messages
 *
 * @sd_interval: At a SD interval, NAN device shall repeat SD period scheduled
 *		 through schedule entry of SD.
 *   BITS(0, 3):
 *	0b0000: = repeat every (512*2^(0-1) = 256 TU)
 *	0b0001: = repeat every (512*2^(1-1) = 512 TU)
 *	0b0010: = repeat every (512*2^(2-1) = 1024 TU)
 *	0b0011: = repeat every (512*2^(3-1) = 2048 TU)
 *	0b0100: = repeat every (512*2^(4-1) = 4096 TU)
 *	0b0101: = repeat every (512*2^(5-1) = 8192 TU)
 *	0b0110~0b1110: Reserved
 *	0b1111: Do not wake up at DW
 *
 * @ucSdTimeout: Indicate timeout of SD in second
 *	0d0: Terminate SD
 *	0d1~254: SD timer in second
 *	0d255: Keep SD on continuously (No timeout)
 *	If SD timeout is not present in private command, the default timeout is
 *	254 seconds.
 */
struct IE_NAN_ASCC_SD {
	uint8_t schedule_category:5, /* enum NAN_SCHEDULE_CATEGORY */
		operation:3; /* enum NAN_SCHEDULE_ENTRY_OPERATION */

	union {
		uint32_t u4Bitmap;
		uint8_t byte[4];
	};

	uint8_t ucOpChannel;

	uint8_t number_unsoliciated_sdf:4,
		sd_interval:4;

	uint8_t ucSdTimeout;
} __KAL_ATTRIB_PACKED__;

/**
 * [v1.8.1] Table 10. Schedule entry for ULW (2.4/5/6 GHz ULW and Passive scan)
 *
 * @schedule_category: Value of corresponding AScC schedule category field
 *   Ref: enum NAN_SCHEDULE_CATEGORY
 *   BITS(3, 7):
 *	0d16: Passive scan
 *	0d18: 2.4 GHz ULW
 *	0d19: 5 GHz ULW
 *	0d20: 6 GHz ULW
 *
 * @operation: Ref enum NAN_SCHEDULE_ENTRY_OPERATION
 *
 * @u4StartingTime: Starting time of this ULW schedule with reference to TSF.
 *
 * @u4Duration: Duration of ULW in unit of micro-second
 *
 * @u4Period: Interval of ULW in unit of micro-second
 *
 * @u4Countdown: Count of ULW
 *
 * @ucOpChannel:
 *	0d001~0d014: Used for 2.4, 5, and 6 GHz channel number
 *	0d015~0d196: Used for 5 and 6 GHz channel number
 *	0d197~0d233: Used for 6 GHz channel number
 *	0d234~0d255: Reserved
 *	0d000: Not used
 *
 * @ucNanPresent: indicate presence of NAN
 */
struct IE_NAN_ASCC_ULW {
	uint8_t schedule_category:5, /* enum NAN_SCHEDULE_CATEGORY  */
		operation:3; /* enum NAN_SCHEDULE_ENTRY_OPERATION */

	uint32_t u4StartingTime;
	uint32_t u4Duration;
	uint32_t u4Period;
	uint32_t u4Countdown;
	uint8_t ucOpChannel;
	uint16_t u2NanPresent :1,
		 u2Reserved :15;
} __KAL_ATTRIB_PACKED__;

/**
 * [v2.0] Table 11. Schedule entry for Common Discovery (2.4/5/6 GHz)
 *
 * @schedule_category: Value of corresponding AScC schedule category field
 *   Ref: enum NAN_SCHEDULE_CATEGORY
 *   BITS(3, 7):
 *	0d21: 2.4 GHz Common Discovery Schedule
 *	0d22: 5 GHz Common Discovery Schedule
 *	0d23: 6 GHz Common Discovery Schedule
 *
 * @operation: Ref enum NAN_SCHEDULE_ENTRY_OPERATION
 *   0: cancel MSCP immediately
 *   1~: interval duration in units of 1024 TUs
 *
 * @ucMscpInterval: Interval of MSCP
 *
 *
 * @u4Reserved: Reserved for future use or byte alignment
 */
struct IE_NAN_ASCC_DISCOVERY {
	uint8_t schedule_category:5, /* enum NAN_SCHEDULE_CATEGORY  */
		operation:3; /* enum NAN_SCHEDULE_ENTRY_OPERATION */

	uint8_t ucMscpInterval;
	uint8_t u4Reserved[10];
} __KAL_ATTRIB_PACKED__;

struct IE_NAN_ASCC_PENDING_CMD {
	struct LINK_ENTRY rLinkEntry;
	uint8_t cmd[]; /* IE_NAN_ASCC_CMD, copy content of requested command */
};


enum NAN_ASCC_STATUS {
	NAN_ASCC_STATUS_SUCCESS = 0,
	NAN_ASCC_STATUS_SUCCESS_BUT_CHANGED = 1,
	NAN_ASCC_STATUS_FAIL = 2,
	NAN_ASCC_STATUS_ASYNC_EVENT = 3,
};

enum NAN_ASCC_REASON {
	/* Type == 0b01 (Response), Status = 0b00~10 (not Asynchronous event) */
	NAN_ASCC_REASON_SUCCESS = 0,
	NAN_ASCC_REASON_OVERWRITE_FAILED = 1,
	NAN_ASCC_REASON_MODIFY_PARTLY = 2,
	NAN_ASCC_REASON_ASCC_PS_FAILED = 3,
	NAN_ASCC_REASON_APNAN_PS_FAILED = 4,
	/* 5~12 Reserved */
	NAN_ASCC_REASON_COMMAND_PARSING_ERROR = 14,
	NAN_ASCC_REASON_UNDEFINED_FAILURE = 15,

	/* Type == 0b01 (Response), Status = 0b11 (Asynchronous event) */
	NAN_ASCC_REASON_FIRMWARE_SCAN_STARTED = 0,    /* ASYNC */
	NAN_ASCC_REASON_FRAMEWORK_SCAN_STARTED = 1,   /* ASYNC */
	NAN_ASCC_REASON_NAN_PASSIVE_SCAN_STARTED = 2, /* ASYNC */
	NAN_ASCC_REASON_SCHED_NEGO_COMPLETED_NDP = 3,   /* SCHED ENTRY */
	NAN_ASCC_REASON_SCHED_NEGO_COMPLETED_OTHER = 4, /* SCHED ENTRY */
	NAN_ASCC_REASON_SCHED_CHANGED_WITHOUT_NEGO = 5, /* SCHED ENTRY */
	NAN_ASCC_REASON_SCHED_CHANGED_LEGACY_NAN = 6,   /* SCHED ENTRY */

	/* Type == 0b10 (Confirm) */
	NAN_ASCC_REASON_PEER_SUCCESS_ASCC_PS = 1,
	NAN_ASCC_REASON_PEER_SUCCESS_APNAN_PS = 2,
	NAN_ASCC_REASON_PEER_SUCCESS_FR_GIVEN_SCHEDULE = 3,
	NAN_ASCC_REASON_PEER_SUCCESS_FR_DIFFERENT_SCHEDULE = 4,
	/* 5~15 Reserved */
};

/**
 * [v2.1] Table 15. Asynchronous event field for AScC, in uint8_t.
 */
enum NAN_ASCC_ASYNC_EVENT_TYPES {
	ASYNC_FIRMWARE_TRIGGERED_SCAN_STARTED = 0,
	ASYNC_FRAMEWORK_TRIGGERED_SCAN_STARTED = 1,
	ASYNC_NAN_PASSIVE_SCAN_STARTED = 2,
	ASYNC_SCHEDULE_CHANGED = 3, /* 3~6, TODO: the difference? */
	ASYNC_FW_TRIGGERED_SCAN_FINISHED = 7,
};

struct NAN_ASCC_ASYNC_EVENT {
	uint8_t ucEvent; /* enum NAN_ASCC_ASYNC_EVENT_TYPES */
} __KAL_ATTRIB_PACKED__;

/**
 * [v1.8.1] Table 13. AScC event data format
 *
 * @ucNanOui: Vendor specific OUI type, 0x3B = 59
 * @u2Length: Length of this OUI data, counted after this field
 * @ucMajorVersion: Version of STD+
 * @ucMinorVersion: Version of STD+
 * @ucRequestId: ID for distinguishing requested data
 *	0x00 ~ 0xFE
 *	FF: Asynchronous event
 *
 * @type: AScC packet type
 *   Ref: enum NAN_ASCC_PACKET_TYPE
 *
 * @status: Success or Fail status, NAN device decrypts the following data field
 *	    according to this subtype value.
 *   REF enum NAN_ASCC_STATUS
 *
 * @reason_code: indicate result as a reason code
 *   REF enum NAN_ASCC_REASON
 *
 * @scheduling_method: Scheduling control entity and methods
 *   Ref: enum NAN_SCHEDULE_METHOD
 *
 * @usage: Indicate specific use case of this AScC data
 *   Ref: enum NAN_SCHEDULE_USAGE
 *
 * @reserved: This is for byte alignment
 *
 * @aucPeerNMIAddr: Intended Peer NMI related with this schedule change
 *	0x000000000000~0xFFFFFFFFFFFE: Peer NMI
 *	0x516F9A010000 or
 *	0xFFFFFFFFFFFF: Unknown or General
 *
 * @u4Category: Specify category of the corresponding schedule.
 *		If usage is 1011 (Schedule info request, this field means
 *		chedule category required)
 *   Ref: enum NAN_SCHEDULE_CATEGORY
 *
 * Followed by various structures, determine by the common fields in
 * at the starting of the header, and casting to defined structure
 * accordingly.
 * 1. schedule entries (Table 6~10), and/or
 *    for status = 1, adopted (modified) schedule,
 *        status = 3, reason code 3, 4, 5, 6
 * 2. additional information.
 *    Refer to Table 13. Representative AScC use case
 *
 * @aucData: Specify category of the corresponding schedule.
 *   Status == 01 (Success but changed), for each adopted (modified) schedule
 *   Status == 11 (Asynchronous event ), reason code 3, 4, 5, and 6
 *
 * May followed by additional information for asynchronous event defined in
 * Table 13
 *
 * Table 13:
 *  case 1. Entering PS mode without fast recovery
 *	16 DW interval sync bacon exchange based power-saving
 *	Condition:
 *	1. Explicit NAN data transmission ends
 *	2. Pairs are not connected to the same AP
 *  case 2. Entering AP-NAN without fast recovery
 *	16 DW interval AP TSF beacon listening based power saving
 *	1. Explicit NAN data transmission ends
 *	2. Pairs are connected to the same AP
 *  case 3. Entering AP_NAN with fast recovery
 *      Same with above case, but usage field is set to 1010
 *      (10, NAN_SCHEDULE_USAGE_APNAN_PS_FR).
 *      1. Framework will manage NDP and corresponding Service ID
 *  case 4. Terminating PS mode with fast recovery (full 5GHz NDP)
 *      Terminate PS and restore full 5GHz NDP, Usage field is set to 0011
 *      (3, NAN_SCHEDULE_USAGE_FR).
 *      Condition:
 *      1. DUT can exploit 32 149 5GHz NDP slots without negotiation
 *  case 5. Terminating PS mode with fast recovery (4 149 5G slots, 28 6G slots)
 *      Terminate PS and restore 4 5GHz NDP & 28 half AP or 6GHz.
 *      Condition:
 *      1. DUT can exploit 4 149 5 GHz NDP slots and 28 6 GHz NDP or scsn slots
 *         without negotiation
 *  case 6. Indicating USD & SD Termination
 *      Indicate for peers to terminate USD related with DUT. Usage field is set
 *      to 1100 (12, NAN_SCHEDULE_USAGE_SD_USD).
 *      Condition:
 *      1. Framework will be noteiced USD terminate event from application
 *  case 7. Check current schedule
 *      Firmware returns current schedule as a AScC format usage field is set to
 *      1011(11, NAN_SCHEDULE_USAGE_CHECK_SCHEDULE).
 *      Condition:
 *      No condition required.
 *  case 8. NDP timer stops
 *      FW can stop NDP idle timer between two devices.
 *      Condition:
 *      Refer to AScC private command with usage 1101
 *      (13, NAN_SCHEDULE_USAGE_NDP_IDLE_TIMER) and NDP timer enabled bit
 */
struct IE_NAN_ASCC_EVENT {
	uint8_t ucNanOui; /* NAN_ASCC_EVT_OUI */
	uint16_t u2Length;
	uint8_t ucMajorVersion;
	uint8_t ucMinorVersion;
	uint8_t ucRequestId;

	uint8_t type:2, /* enum NAN_ASCC_PACKET_TYPE */
		status:2, /* enum NAN_ASCC_STATUS */
		reason_code:4; /* enum NAN_ASCC_REASON */

	uint8_t scheduling_method:2, /* enum NAN_SCHEDULE_METHOD */
		usage:4, /* enum NAN_SCHEDULE_USAGE */
		reserved:2;

	uint8_t aucPeerNMIAddr[MAC_ADDR_LEN];

	uint32_t u4Category; /* enum NAN_SCHEDULE_CATEGORY */

	/* 1) Schedule entries for status 01 (success but changed) or
	 *                      status 11 (asynchronous event)
	 * 2) additional information for asynchronous event (Table 13)
	 */
	uint8_t aucData[];
} __KAL_ATTRIB_PACKED__;

#define ASCC_EVENT_HDR_LEN OFFSET_OF(struct IE_NAN_ASCC_EVENT, ucMajorVersion)
#define ASCC_EVENT_LEN(fp) (((struct IE_NAN_ASCC_EVENT *)(fp))->u2Length)
#define ASCC_EVENT_SIZE(fp) (ASCC_EVENT_HDR_LEN + ASCC_EVENT_LEN(fp))
#define ASCC_EVENT_BODY_SIZE(fp)	\
	(ASCC_EVENT_SIZE(fp) - sizeof(struct IE_NAN_ASCC_EVENT))

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/* Internal structure storing data specified in Table 6 */
struct DW_ScheduleSettings {
	uint8_t band; /* 2.4G, 5G, 6G */
	uint8_t channel;
	uint8_t offset;
	uint8_t interval;
	uint8_t discovery_beacon_enabled;
};

/* Internal structure Storing data specified in Table 7 */
struct FAW_ScheduleSettings {
	uint8_t type; /* FC, NDP, AP, FW trigger scan */
	uint8_t band; /* 2.4G, 5G, 6G (FC, NDP, AP) */
	uint8_t channel;
	uint32_t bitmap;
};

/* Internal structure storing data specified in Table 8 */
struct USD_ScheduleSettings {
	uint8_t band; /* 2.4G, 5G */
	uint8_t channel;
	uint32_t bitmap;
	uint8_t sdf_num;
	uint8_t interval;
	uint8_t timeout;
};

/* Internal structure storing data specified in Table 9 */
struct SD_ScheduleSettings {
	uint8_t band; /* 2.4G, 5G */
	uint8_t channel;
	uint32_t bitmap;
	uint8_t sdf_num;
	uint8_t interval;
	uint8_t timeout;
};

/* Internal structure storing data specified in Table 10 */
struct ULW_ScheduleSettings {
	uint8_t type; /* Passive scan, ULW */
	uint8_t band; /* 2.4G, 5G, 6G (ULW) */
	uint8_t channel;
	uint32_t stating_time;
	uint32_t duration;
	uint32_t period;
	uint32_t countdown;
};


/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */


/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

uint32_t nanProcessASCC(struct ADAPTER *prAdapter, const uint8_t *buf,
			size_t u4Size);
uint32_t nanParseScheduleEntry(struct ADAPTER *prAdapter,
			       void *cmd,
			       const uint8_t *buf, uint32_t size,
			       u_int8_t update);
const char *schedule_category_included_str(uint32_t c,
					   char *buf, uint32_t size);

const char *ascc_type_str(uint8_t i);
const char *ascc_status_str(uint8_t i);
const char *scheduling_method_str(uint8_t i);
const char *operation_str(uint8_t i);

void nanAsccNanOnHandler(struct ADAPTER *prAdapter);
void nanAsccNanOffHandler(struct ADAPTER *prAdapter);

void nanReconfigureCustFaw(struct ADAPTER *prAdapter);

#endif /* CFG_SUPPORT_NAN */
#endif /* _NAN_EXT_ASCC_H_ */

