/*

 *(C) Copyright 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 */

#ifndef _SHM_SHARE_H_
#define _SHM_SHARE_H_

#define CiStubSvc_Id 1
#define NVMSvc_Id 2
#define CiDataStubSvc_Id 3
#define DiagSvc_Id 4
#define AudioStubSvc_Id 5
#define CiCsdStubSvc_Id 6
#define TestPortSvc_Id 8
#define CiImsStubSvc_Id 9

#define CISTUB_PORT 1
#define NVMSRV_PORT 2
#define CIDATASTUB_PORT 3	/* ttc */
#define DIAG_PORT 4
#define AUDIOSTUB_PORT 5
#define CICSDSTUB_PORT 6
#define TEST_PORT 8
#define CIIMSSTUB_PORT 9

#ifdef CONFIG_SSIPC_SUPPORT
#define RAW_AT_PORT 7
#define RAW_AT_DUN_PORT 10
#define RAW_AT_PROD_PORT 11
#define RAW_AT_SIMAL_PORT 12
#define RAW_AT_CLIENT_SOL_PORT 13
#define RAW_AT_CLIENT_UNSOL_PORT 14
#define RAW_AT_RESERVERED_PORT 15
#define RAW_AT_GPS_PORT 16
#define RAW_AT_RESERVERED2_PORT 17

#define RAW_AT_PORT2 20
#define RAW_AT_DUN_PORT2 21
#define RAW_AT_PROD_PORT2 22
#define RAW_AT_SIMAL_PORT2 23
#define RAW_AT_CLIENT_SOL_PORT2 24
#define RAW_AT_CLIENT_UNSOL_PORT2 25
#define RAW_AT_RESERVERED_PORT2 26
#define RAW_AT_RESERVERED2_PORT2 27
#define RAW_AT_GPS_PORT2 28
#endif

#define MsocketLinkdownProcId   0xFFFE	/* For linkdown indication */
#define MsocketLinkupProcId             0xFFFD	/* For linkup indication */

/* The linkup messages will be sent to the ports by this order. */
#define BroadCastSequence	{0, NVMSRV_PORT, DIAG_PORT, CISTUB_PORT, \
		CIDATASTUB_PORT, CICSDSTUB_PORT, TEST_PORT, AUDIOSTUB_PORT}
typedef struct _ShmApiMsg {
	int svcId;
	int procId;
	int msglen;
} ShmApiMsg;

#define SHM_HEADER_SIZE sizeof(ShmApiMsg)

#endif
