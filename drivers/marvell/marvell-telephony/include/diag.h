/*

 *(C) Copyright 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 */

#ifndef _DIAG_H_
#define _DIAG_H_

#include "linux_driver_types.h"

#define DIAG_CORE_ID  1

#define MAX_DIAG_DATA_RXMSG_LEN 2048
#define DIAGDRV_MAGIC 249
#define DIAGSTUB_USBADD _IOW(DIAGDRV_MAGIC, 1, int)
#define DIAGSTUB_USBREMOVE _IOW(DIAGDRV_MAGIC, 2, int)
#define DIAGSTUB_TESTSTART _IOW(DIAGDRV_MAGIC, 3, int)

#define DIAGSTUB_DUMPCOUNT _IOW(DIAGDRV_MAGIC, 3, int)

#define DIAGSTUB_USBSTART _IOW(DIAGDRV_MAGIC, 5, int)
#define DIAGSTUB_USBINIT  _IOW(DIAGDRV_MAGIC, 6, int)


#define DiagDataRequestStartProcId 1
#define DiagDataConfirmStartProcId 2
//#define DiagDataIndProcId 3
//#define DiagDataReqProcId 4
#define DiagExtIfConnectedProcId 5
#define DiagExtIfDisconnectedProcId 6
#define DiagExtIfConnectedAckProcId 7
#define DiagExtIfDisconnectedAckProcId 8

#define DiagDataRequestStartMsg 0x6d727631
#define DiagDataConfirmStartMsg  0x6d727632
#define DiagExtIfConnectedMsg 0x6d727633
#define DiagExtIfConnectedAckMsg 0x6d727634
#define DiagExtIfDisconnectedMsg 0x6d727635
#define DiagExtIfDisconnectedAckMsg  0x6d727636
#define SinglePktMaxLen 2024
#define IO_FLUSH_PENDING_BUFFERS _IOW('T', 99, int) /* flush pending buffer */

typedef int (*DiagRxCallbackFunc)(char* packet, int len);
typedef struct _diagheader
{
	UINT16 seqNo;
	UINT16 packetlen;
	UINT32 msgType;
}DIAGHDR;       //don't change the definition. variable definition order sensitive.

typedef struct _diagmsgheader
{
	DIAGHDR diagHeader;
	int procId;
}DiagMsgHeader;

void diagTxRawData(unsigned char *msg, int msglen);
void diagSendUsbData(unsigned char*msg, int msglen);

void InitDiagChannel(void);
void InitUSBChannel(void);
void DeInitDiagChannel(void);
void diagTxMsg(int procId, unsigned char *msg, int msglen);
int diagRegisterRxCallBack(DiagRxCallbackFunc callback);
int diagUnregisterRxCallBack(void);
void diagUsbConnectNotify(void);
void diagUsbDisConnectNotify(void);




#endif
