/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2020 MediaTek Inc.
 */

/*
 * Log: gl_sys.h
 *
 *
 *
 */

#ifndef _GL_SYS_H
#define _GL_SYS_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */


/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define FS_VERSION						4
#define FS_VERSION_SIZE					2

#define FS_SOLUTION_PROVIDER_SIZE		3

/* HW Feature Set */
#define FS_HW_FEATURE_SIZE				2
#define FS_HW_FEATURE_LEN				4

#define FS_HW_STANDARD_WIFI4			0
#define FS_HW_STANDARD_WIFI5			1
#define FS_HW_STANDARD_WIFI6			2
#define FS_HW_STANDARD_WIFI6E			3
#define FS_HW_STANDARD_WIFI7			4

#define FS_HW_LP_RX_CORE_OFFSET			4
#define FS_HW_LP_RX_CORE				1

#define FS_HW_NUM_CORES_OFFSET			5
#define FS_HW_NUM_CORES_ONE				1
#define FS_HW_NUM_CORES_TWO				2

#define FS_HW_CONCURRENCY_MODE_OFFSET	8
#define FS_HW_CONCURRENCY_MODE_ONE		1
#define FS_HW_CONCURRENCY_MODE_TWO		2

#define FS_HW_NUM_ANT_OFFSET			11
#define FS_HW_NUM_ANT_SISO				1
#define FS_HW_NUM_ANT_MIMO				2

/* SW Feature Set */
#define FS_SW_FEATURE_NUM				14
#define FS_SW_FEATURE_LEN				108
#define FS_SW_MAX_DATA_LEN				8

/* 1 PNO */
#define FS_SW_PNO_ID					1
#define FS_SW_PNO_LEN					1
#define FS_SW_PNO_SUPPORT				BIT(0)
#define FS_SW_PNO_UNASSOC				BIT(1)
#define FS_SW_PNO_ASSOC					BIT(2)

/* 2 TWT  */
#define FS_SW_TWT_ID					2
#define FS_SW_TWT_LEN					2
#define FS_SW_TWT_SUPPORT				BIT(0)
#define FS_SW_TWT_REQUESTER				BIT(1)
#define FS_SW_TWT_BROADCAST				BIT(2)
#define FS_SW_TWT_FLEXIBLE				BIT(3)
/*
 * B4~B5: Min Service Period (0: 4ms, 1:8ms, 2:12ms, 3:16ms)
 * B6~B7: Min Sleep Period   (0: 4ms, 1:8ms, 2:12ms, 3:16ms)
 * 2nd byte : Reserved
 */

/* 3 Wi-Fi Optimizer */
#define FS_SW_OPTI_ID					3
#define FS_SW_OPTI_LEN					1
#define FS_SW_OPTI_SUPPORT				BIT(0)
/* B1: Dynamic scan dwell control */
#define FS_SW_OPTI_B1					BIT(1)
/* B2: Enhanced passive scan */
#define FS_SW_OPTI_B2					BIT(2)

/* 4 Scheduled PM */
#define FS_SW_SCHE_PM_ID				4
#define FS_SW_SCHE_PM_LEN				1
#define FS_SW_SCHE_PM_SUPPORT			BIT(0)

/* 5 Delayed Wakeup */
#define FS_SW_D_WAKEUP_ID				5
#define FS_SW_D_WAKEUP_LEN				1
#define FS_SW_D_WAKEUP_SUPPORT			BIT(0)

/* 6 RFC8325 */
#define FS_SW_RFC8325_ID				6
#define FS_SW_RFC8325_LEN				1
#define FS_SW_RFC8325_SUPPORT			BIT(0)

/* 7 MHS */
/* 1st byte */
#define FS_SW_MHS_GET_VALID_CH			BIT(0)
#define FS_SW_MHS_11AX					BIT(1)
#define FS_SW_MHS_WPA3					BIT(2)

/* 2nd byte */
#define FS_SW_MHS_ID					7
#define FS_SW_MHS_LEN					2
#define FS_SW_MHS_DUAL_INF				BIT(0) /* B0 */
#define FS_SW_MHS_5G					BIT(1)
#define FS_SW_MHS_6G					BIT(2)
#define FS_SW_MHS_MAX_CLIENT			15 /* B3~B6 MAX Clients */
#define FS_SW_MHS_MAX_CLIENT_OFFSET		3
#define FS_SW_MHS_CC_HAL				BIT(7)

/* 8 Roaming */
#define FS_SW_ROAM_ID					8
#define FS_SW_ROAM_LEN					4
#define FS_SW_ROAM_VER_MAJOR			3 /* 1st byte */
#define FS_SW_ROAM_VER_MINOR			0 /* 2nd byte */

/* 3rd byte */
/* B0: High Channel Utilization Trigger */
#define FS_SW_ROAM_3B_B0				BIT(0)
/* B1: Emergence Roaming Trigger */
#define FS_SW_ROAM_3B_B1				BIT(1)
/* B2: BTM Roaming Trigger */
#define FS_SW_ROAM_3B_B2				BIT(2)
/* B3: Idle Roaming Trigger */
#define FS_SW_ROAM_3B_B3				BIT(3)
/* B4: WTC Roaming Trigger */
#define FS_SW_ROAM_3B_B4				BIT(4)
/* B5: BT Coex Roaming Trigger */
#define FS_SW_ROAM_3B_B5				BIT(5)
/* B6: Roaming between WPA and WPA2 Roaming */
#define FS_SW_ROAM_3B_B6				BIT(6)
/* B7: Manage Channel List API */
#define FS_SW_ROAM_3B_B7				BIT(7)

/* 4th byte */
/* B0: Adaptive 11r */
#define FS_SW_ROAM_4B_B0				BIT(0)
/* B1: Roaming Control API - 1.GET, 2.SET Roam Trigger */
#define FS_SW_ROAM_4B_B1				BIT(1)
/* B2: Roaming Control API - 3/4.Reassoc */
#define FS_SW_ROAM_4B_B2				BIT(2)
/* B3: Roaming Control API - 5.GET CU */
#define FS_SW_ROAM_4B_B3				BIT(3)

/* 9 NCHO */
#define FS_SW_NCHO_ID					9
#define FS_SW_NCHO_LEN					2
#define FS_SW_NCHO_VER_MAJOR			3
#define FS_SW_NCHO_VER_MINOR			0

/* 10 ASSURANCE */
#define FS_SW_ASSUR_ID					10
#define FS_SW_ASSUR_LEN					1
#define FS_SW_ASSUR_SUPPORT				BIT(0)

/* 11 Frame Pcap Logging */
#define FS_SW_FRAMEPCAP_ID				11
#define FS_SW_FRAMEPCAP_LEN				1
#define FS_SW_FRAMEPCAP_LOG				BIT(0)

/* 12 Security */
#define FS_SW_SEC_ID					12
#define FS_SW_SEC_LEN					2
#define FS_SW_SEC_WPA3_SAE_HASH			BIT(0)
#define FS_SW_SEC_WPA3_SAE_FT			BIT(1)
#define FS_SW_SEC_WPA3_ENT_S_B			BIT(2)
#define FS_SW_SEC_WPA3_ENT_S_B_192		BIT(3)
#define FS_SW_SEC_FILS_SHA256			BIT(4)
#define FS_SW_SEC_FILS_SHA384			BIT(5)
#define FS_SW_SEC_FILS_SHA256_FT		BIT(6)
#define FS_SW_SEC_FILS_SHA384_FT		BIT(7)

/* 2nd byte */
#define FS_SW_SEC_ENHANCED_OPEN			BIT(0)

/* 13 P2P */
#define FS_SW_P2P_ID					13
#define FS_SW_P2P_LEN					6

/* 1st byte */
#define FS_SW_P2P_NAN					BIT(0)
#define FS_SW_P2P_TDLS					BIT(1)
#define FS_SW_P2P_P2P6E					BIT(2)
#define FS_SW_P2P_LISTEN_OFFLOAD		BIT(3)
#define FS_SW_P2P_NOA_PS				BIT(4)
#define FS_SW_P2P_TDLS_OFF_CH			BIT(5)
#define FS_SW_P2P_TDLS_CAP_ENHANCE		BIT(6)
#define FS_SW_P2P_TDLS_PS				BIT(7)

/* 2nd byte
 * B0~B3 : Number of max TDLS connections
 * B4~B7 : Number of max NAN NDP
 */
#define FS_SW_P2P_TDLS_MAX_NUM			4

/* 3rd byte */
#define FS_SW_P2P_STA_P2P				BIT(0)
#define FS_SW_P2P_STA_SAP				BIT(1)
#define FS_SW_P2P_STA_NAN				BIT(2)
#define FS_SW_P2P_STA_TDLS				BIT(3)
#define FS_SW_P2P_STA_SAP_P2P			BIT(4)
#define FS_SW_P2P_STA_SAP_NAN			BIT(5)
#define FS_SW_P2P_STA_P2P_P2P			BIT(6)
#define FS_SW_P2P_STA_P2P_NAN			BIT(7)

/* 4th byte */
#define FS_SW_P2P_STA_P2P_TDLS			BIT(0)
#define FS_SW_P2P_STA_SAP_TDLS			BIT(1)
#define FS_SW_P2P_STA_NAN_TDLS			BIT(2)
#define FS_SW_P2P_STA_SAP_P2P_TDLS		BIT(3)
#define FS_SW_P2P_STA_SAP_NAN_TDLS		BIT(4)
#define FS_SW_P2P_STA_P2P_P2P_TDLS		BIT(5)
#define FS_SW_P2P_STA_P2P_NAN_TDLS		BIT(6)

/* 14 BIG DATA */
#define FS_SW_BIGD_ID					14
#define FS_SW_BIGD_LEN					1
#define FS_SW_BIGD_GETBSSINFO			BIT(0)
#define FS_SW_BIGD_GETASSOCREJECTINFO	BIT(1)
#define FS_SW_BIGD_GETSTAINFO			BIT(2)

#define MANIFEST_BUFFER_SIZE	256

extern uint8_t g_wifiVer[MANIFEST_BUFFER_SIZE];
extern uint32_t g_wifiVer_length;

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/* SW Feature Set ID/Len/Data */
struct FS_SW_ILD_T {
	u_int8_t u8ID;
	u_int8_t u8Len;
	u_int8_t aucData[FS_SW_MAX_DATA_LEN];
};



/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */


#endif /* _GL_SYS_H */
