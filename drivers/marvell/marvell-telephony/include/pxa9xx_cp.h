/*
 * PXA9xx CP related
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2007 Marvell International Ltd.
 * All Rights Reserved
 */

#ifndef _PXA9XX_CP_H
#define _PXA9XX_CP_H

#include "linux_driver_types.h"

#define LOAD_TABLE_OFFSET       0x1C0
#define LT_APP2COM_DATA_LEN     48
#define LOAD_TABLE_SIGNATURE    "LOAD_TABLE_SIGN"
#define OFFSET_IN_ARBEL_IMAGE   4
/* As agreed with CP side, reserve 0x800 byte for each key section.  */
#define SHM_KEY_SECTION_LEN	0x800
#define SMALL_CODE_SIGNARTURE   0x534c4344

#define NETWORK_MODE_TD ('_TD_')
#define NETWORK_MODE_WB ('_WB_')
struct load_table_dynamic_part
{
	UINT32 b2init;
	UINT32 init;
	UINT32 addrLoadTableRWcopy;
	UINT32 ee_postmortem;
	UINT32 numOfLife;

	UINT32 diag_p_diagIntIfQPtrData;
	UINT32 diag_p_diagIntIfReportsList;
	UINT32 spare_CCCC;

	UINT32 ACIPCBegin;
	UINT32 ACIPCEnd;
	UINT32 LTEUpBegin;
	UINT32 LTEUpEnd;
	UINT32 LTEDownBegin;
	UINT32 LTEDownEnd;
	UINT32 apps2commDataLen;
	UINT8  apps2commDataBuf[LT_APP2COM_DATA_LEN];

	// CP load addresses for AP
	UINT32 CPLoadAddr;
	UINT32 MSALoadAddrStart;
	UINT32 MSALoadAddrEnd;

	UINT32 ACIPCPSDownlinkBegin;
	UINT32 ACIPCPSDownlinkEnd;
	UINT32 ACIPCPSUplinkBegin;
	UINT32 ACIPCPSUplinkEnd;
	UINT32 ACIPCOtherPortDlBegin;
	UINT32 ACIPCOtherPortDlEnd;
	UINT32 ACIPCOtherPortUlBegin;
	UINT32 ACIPCOtherPortUlEnd;

	UINT32 ACIPCDIAGPortDlBegin;
	UINT32 ACIPCDIAGPortDlEnd;
	UINT32 ACIPCDIAGPortUlBegin;
	UINT32 ACIPCDIAGPortUlEnd;

	UINT32 ACIPCPSKeySectionBegin;
	UINT32 ACIPCOtherPortKeySectionBegin;
	UINT32 ACIPCDIAGKeySectionBegin;

	UINT32 MemLogBegin;
	UINT32 MemLogEnd;
	/* network mode, '_TD_' for TD mode, '_WB_' for WB mode */
	UINT32 NetworkModeIndication;

	UINT32 RFBinLoadAddrStart;
	UINT32 RFBinLoadAddrEnd;

	/* link type, '_DL_' for dual link, '_SL_' for single link */
	UINT32 LinkType;
};

struct cp_load_table_head
{
	union{
		struct load_table_dynamic_part dp;
		UINT8  filler[LOAD_TABLE_OFFSET-OFFSET_IN_ARBEL_IMAGE-4];
	};
	UINT32 smallCodeSign;
	UINT32 imageBegin;
	UINT32 imageEnd;
	char   Signature[16];
	UINT32 sharedFreeBegin;
	UINT32 sharedFreeEnd;
	UINT32 ramRWbegin;
	UINT32 ramRWend;
	UINT32 spare_EEEE;
	UINT32 ReliableDataBegin;
	UINT32 ReliableDataEnd;

	char   OBM_VerString[8];
	UINT32 OBM_Data32bits;
};

#endif // _PXA9XX_CP_H
