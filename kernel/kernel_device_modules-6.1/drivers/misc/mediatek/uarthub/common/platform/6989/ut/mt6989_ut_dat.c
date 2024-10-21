// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/stddef.h>
#include "mt6989_ut_dat.h"

/*
 * Unit Test packet declaration
 *
 * Naming format: PKT_@NN_@A@B
 * @NN: Type Num : 01 ~ 16 (HCI CMD, HCI EVENT, ...)
 * @A:  Is CRC embedded
 * @B:  Completeness
 */
const unsigned char PKT_02_01[] = {0x04, 0xE4, 0x07, 0x02, 0x03, 0x03, 0x00, 0x00, 0x03, 0x01};
const unsigned char PKT_02_11[] = {0x04, 0xE4, 0x07, 0x02, 0x03, 0x03, 0x00, 0x00, 0x03, 0x01, 0xCF, 0x55 };
const unsigned char PKT_03_01[] = {0x02, 0x00, 0x00, 0x03, 0x00, 0x66, 0x67, 0x68};
const unsigned char PKT_03_11[] = {0x02, 0x00, 0x00, 0x03, 0x00, 0x66, 0x67, 0x68, 0x92, 0x06};
const unsigned char PKT_08_01[] = {0x86, 0x00, 0x00, 0x03, 0x00, 0x66, 0x67, 0x68};
const unsigned char PKT_08_11[] = {0x86, 0x00, 0x00, 0x03, 0x00, 0x66, 0x67, 0x68, 0x7e, 0xf4};
const unsigned char PKT_12_01[] = {0x82, 0x00, 0x00, 0x03, 0x00, 0x66, 0x67, 0x68};
const unsigned char PKT_12_11[] = {0x82, 0x00, 0x00, 0x03, 0x00, 0x66, 0x67, 0x68, 0x13, 0xfb};

const unsigned char PKT_F0[] = {0xF0};
const unsigned char PKT_F1[] = {0xF1};
const unsigned char PKT_F2[] = {0xF2};
const unsigned char PKT_FF[] = {0xFF};

const unsigned char CRC_03_11[] = {0x92, 0x06};
const unsigned char CRC_08_11[] = {0x7e, 0xf4};
const unsigned char CRC_12_11[] = {0x13, 0xfb};

const int SZ_02_01 = sizeof(PKT_02_01);
const int SZ_02_11 = sizeof(PKT_02_11);
const int SZ_03_01 = sizeof(PKT_03_01);
const int SZ_03_11 = sizeof(PKT_03_11);
const int SZ_08_01 = sizeof(PKT_08_01);
const int SZ_08_11 = sizeof(PKT_08_11);
const int SZ_12_01 = sizeof(PKT_12_01);
const int SZ_12_11 = sizeof(PKT_12_11);
const int SZ_ESC = sizeof(PKT_FF);
const int SZ_CRC = 2;

/* cmd and event actions sequence - 01*/
#define ITEM_CNT_01		11
const unsigned char *pCmds_01[ITEM_CNT_01] = {
		PKT_03_01, PKT_03_11, PKT_08_01, PKT_08_11, PKT_12_01, PKT_12_11,
		PKT_F0,    PKT_F1,    PKT_F2,    PKT_FF,    PKT_F0
};
const int szCmds_01[ITEM_CNT_01] = {
	SZ_03_01, SZ_03_11, SZ_08_01, SZ_08_11, SZ_12_01, SZ_12_11,
	SZ_ESC,   SZ_ESC,   SZ_ESC,   SZ_ESC,   SZ_ESC
};
const int tx_dev_ids_01[ITEM_CNT_01] = {
	DEV_0,   DEV_CMM,  DEV_1,   DEV_CMM, DEV_2,  DEV_CMM,
	DEV_CMM, DEV_CMM,  DEV_CMM, DEV_CMM, DEV_CMM
};
const unsigned char *pEvts_01[ITEM_CNT_01] = {
	PKT_03_11, PKT_03_01, PKT_08_11, PKT_08_01, PKT_12_11, PKT_12_01,
	PKT_FF,    PKT_FF,    PKT_FF,    NULL,    PKT_FF
};
const int szEvts_01[ITEM_CNT_01] = {
	SZ_03_11, SZ_03_01, SZ_08_11, SZ_08_01, SZ_12_11, SZ_12_01,
	SZ_ESC,   SZ_ESC,   SZ_ESC,   0,        SZ_ESC
};
const int rx_dev_ids_01[ITEM_CNT_01] = {
	DEV_CMM, DEV_0,  DEV_CMM, DEV_1,   DEV_CMM, DEV_2,
	DEV_0,   DEV_1,  DEV_2,   DEV_OFF, DEV_0
};
