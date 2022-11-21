/*
 * ipcp.h -- IPCP header
 *
 * (C) Copyright [2006-2008] Marvell International Ltd.
 * All Rights Reserved
 *
 */
#ifndef _IPCP_H_INC_
#define _IPCP_H_INC_

#if defined __cplusplus
extern "C" {
#endif				//__cplusplus

#include "ppp_types.h"

void IpcpHandleIncomingFrame(PppControlS * pppControl);
void IpcpSendConfigReq(PppControlS * pppControl);
void IpcpUpdateIpParams(PppControlS * pppControl, IpcpConnectionParamsS * ipcpConnectionParams);

#if defined __cplusplus
}
#endif				//__cplusplus
#endif				// _IPCP_H_INC_
