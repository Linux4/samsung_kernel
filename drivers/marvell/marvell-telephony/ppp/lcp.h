/*
 * lcp.h -- lcp header
 *
 * (C) Copyright [2006-2008] Marvell International Ltd.
 * All Rights Reserved
 *
 */

#ifndef _LCP_H_
#define _LCP_H_

#if defined __cplusplus
extern "C" {
#endif				//__cplusplus

#include "ppp_types.h"

//APIs
void LcpHandleIncomingFrame(PppControlS * pppControl);
void LcpSendConfigReq(PppControlS * pppControl);
void LcpSendTerminateReq(PppControlS * pppControl);
void LcpSendEchoReq(PppControlS * pppControl);
void LcpKickoffEchoRequest(PppControlS * pppControl);
void CcpHandleIncomingFrame(PppControlS * pppControl);
void Ipv6cpHandleIncomingFrame(PppControlS * pppControl);

#if defined __cplusplus
}
#endif				//__cplusplus
#endif
