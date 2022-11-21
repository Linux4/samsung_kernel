/*
 * ppp.h -- ppp.c header
 *
 * (C) Copyright [2006-2008] Marvell International Ltd.
 * All Rights Reserved
 *
 */

#ifndef _PPP_H_
#define _PPP_H_

#if defined __cplusplus
extern "C" {
#endif				//__cplusplus

#include "ppp_types.h"
#include "common_datastub.h"

#define PPP_QUEUE_NUM_MESSAGES	32
#define MAX_QUEUED_INCOMING_DATA_PACKETS PPP_QUEUE_NUM_MESSAGES

typedef int (*DataRxCallbackFunc) (const unsigned char *packet, unsigned int len, unsigned char cid);
extern int registerRxCallBack(SVCTYPE pdpTye, DataRxCallbackFunc callback);


/************************************************************************/
/* APIs                                                                 */
/************************************************************************/
void PppInit(PppInitParamsS * PppInitParams);	//  PppRxCallbackFunc fPtr , PppTerminateCallbackFunc fTermCbPtr);
void PppDeinit(void);
void PppMessageReq(const U_CHAR * buf, U_INT length);
void PppCreateMessageFrameIp(void);
void PppCreateMessageFrame(void);
void PppSendToCallback(PppControlS * pppControl, U_CHAR * buf, U_INT length);
void PppUpdateIpParams(IpcpConnectionParamsS * ipParams);
void PppSendTerminateReq(void);
void PppTerminate(void);
void PppReset(void);
void PppSetCid(U_INT cid);
void PppEnableEchoRequest(BOOL isEnabled);
void modem_ppp_get_ip(unsigned char cid);
int pppRelayMessageFromComm(const U_CHAR *buf, U_INT count, U_CHAR cid);
#if defined __cplusplus
}
#endif				//__cplusplus
#endif				// _PPP_H_
