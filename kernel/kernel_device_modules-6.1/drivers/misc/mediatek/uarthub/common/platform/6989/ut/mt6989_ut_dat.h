/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef _MT6989_UT_DAT_H_
#define _MT6989_UT_DAT_H_

#define _RESULT_MSG(s) ((s)? "FAIL" : "PASS")

#define DEBUG_TX_MOINTOR_Array_Mask_GET_intfhub_debug_tx_monitor_Array(buf, mask) \
	do {\
		*(int *)(buf)    = (DEBUG_TX_MOINTOR_0_GET_intfhub_debug_tx_monitor0(DEBUG_TX_MOINTOR_0_ADDR) & mask); \
		*(int *)(buf+4)  = (DEBUG_TX_MOINTOR_1_GET_intfhub_debug_tx_monitor1(DEBUG_TX_MOINTOR_1_ADDR) & mask); \
		*(int *)(buf+8)  = (DEBUG_TX_MOINTOR_2_GET_intfhub_debug_tx_monitor2(DEBUG_TX_MOINTOR_2_ADDR) & mask); \
		*(int *)(buf+12) = (DEBUG_TX_MOINTOR_3_GET_intfhub_debug_tx_monitor3(DEBUG_TX_MOINTOR_3_ADDR) & mask); \
	} while (0)
#define DEBUG_TX_MOINTOR_Array_GET_intfhub_debug_tx_monitor_Array(buf) \
	DEBUG_TX_MOINTOR_Array_Mask_GET_intfhub_debug_tx_monitor_Array(buf, 0xFFFFFFFF)

#define DEBUG_RX_MOINTOR_Array_Mask_GET_intfhub_debug_rx_monitor_Array(buf, mask) \
	do {\
		*(int *)(buf)    = (DEBUG_RX_MOINTOR_0_GET_intfhub_debug_rx_monitor0(DEBUG_RX_MOINTOR_0_ADDR) & mask); \
		*(int *)(buf+4)  = (DEBUG_RX_MOINTOR_1_GET_intfhub_debug_rx_monitor1(DEBUG_RX_MOINTOR_1_ADDR) & mask); \
		*(int *)(buf+8)  = (DEBUG_RX_MOINTOR_2_GET_intfhub_debug_rx_monitor2(DEBUG_RX_MOINTOR_2_ADDR) & mask); \
		*(int *)(buf+12) = (DEBUG_RX_MOINTOR_3_GET_intfhub_debug_rx_monitor3(DEBUG_RX_MOINTOR_3_ADDR) & mask); \
	} while (0)
#define DEBUG_RX_MOINTOR_Array_GET_intfhub_debug_rx_monitor_Array(buf) \
	DEBUG_RX_MOINTOR_Array_Mask_GET_intfhub_debug_rx_monitor_Array(buf, 0xFFFFFFFF)

#define DEV_OFF		(-1)
#define DEV_0		(0)
#define DEV_1		(1)
#define DEV_2		(2)
#define DEV_CMM		(3)

extern int g_uh_ut_verbose;

extern const unsigned char PKT_02_01[];
extern const unsigned char PKT_02_11[];
extern const unsigned char PKT_03_01[];
extern const unsigned char PKT_03_11[];
extern const unsigned char PKT_08_01[];
extern const unsigned char PKT_08_11[];
extern const unsigned char PKT_12_01[];
extern const unsigned char PKT_12_11[];
extern const unsigned char PKT_F0[];
extern const unsigned char PKT_F1[];
extern const unsigned char PKT_F2[];
extern const unsigned char PKT_FF[];

extern const unsigned char CRC_03_11[];
extern const unsigned char CRC_08_11[];
extern const unsigned char CRC_12_11[];

extern const int SZ_02_01;
extern const int SZ_02_11;
extern const int SZ_03_01;
extern const int SZ_03_11;
extern const int SZ_08_01;
extern const int SZ_08_11;
extern const int SZ_12_01;
extern const int SZ_12_11;
extern const int SZ_ESC;
extern const int SZ_CRC;

/* cmd and event actions sequence - 01*/
#define ITEM_CNT_01		11
extern const unsigned char *pCmds_01[ITEM_CNT_01];
extern const int szCmds_01[ITEM_CNT_01];
extern const int tx_dev_ids_01[ITEM_CNT_01];
extern const unsigned char *pEvts_01[ITEM_CNT_01];
extern const int szEvts_01[ITEM_CNT_01];
extern const int rx_dev_ids_01[ITEM_CNT_01];

#endif //_MT6989_UT_DAT_H_
