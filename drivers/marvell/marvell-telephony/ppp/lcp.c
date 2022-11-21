/*
 * lcp.c - lcp functions
 *
 * (C) Copyright [2006-2008] Marvell International Ltd.
 * All Rights Reserved
 *
 */

#include "ppp.h"
#include "lcp.h"
#include "chap.h"
#include "ppp_log.h"

static void lcpStartEchoReqTimer(PppControlS *pppControl, U_INT timeout, PppTimeoutCallbackFunc toCb)
{
	PPP_LOG_ENTRY(MDB_lcpStartEchoReqTimer);
	PPP_LOG_1(timeout);
	init_timer(&pppControl->LcpEchoReqTimeoutTimer);

	pppControl->LcpEchoReqTimeoutTimer.expires = jiffies + msecs_to_jiffies(timeout);
	pppControl->LcpEchoReqTimeoutTimer.function = toCb;
	pppControl->LcpEchoReqTimeoutTimer.data = (unsigned long)pppControl;

	add_timer(&pppControl->LcpEchoReqTimeoutTimer);

	PPP_LOG_EXIT();

}

static void lcpHandleEchoRequest(PppControlS *pppControl)
{
	U_CHAR *buf = pppControl->OutMessageContainer.Buf;
	LcpMessageS *inLcpMsg = (LcpMessageS *)pppControl->InMessage->Data;
	U_CHAR *inbuf = (U_CHAR *)&inLcpMsg->Id;	//pppControl->InMessage->Data; // point to LcpMessage.Id, skipping Type
	U_SHORT inLen;		// length of LCP message without message type, which is modified.

	U_INT index = 0;
	U_SHORT fcs = PPP_INITFCS;

	PPP_LOG_ENTRY(MDB_lcpHandleEchoRequest);
	UNUSEDPARAM(fcs);

	PPP_LOG_2(inLcpMsg->Id, inLcpMsg->Length);
	GETSHORT_NOINC(inLen, &inLcpMsg->Length);
	inLen--;

	PPP_LOG_1(inLen);

	if (!pppControl->lcpXmitParams.isAcfcEnabled) {
		PPP_LOG();
		PPP_APPEND(buf, index, PPP_ALLSTATIONS, fcs);
		PPP_APPEND(buf, index, PPP_UI, fcs);
	}

	PPP_APPEND_SHORT(buf, index, PPP_LCP, fcs);

	PPP_APPEND(buf, index, ECHO_REPLY, fcs);

	PPP_LOG_1(inLen);

	while (inLen-- > 0) {
		PPP_APPEND(buf, index, *inbuf, fcs);
		inbuf++;
	}

	PPP_APPEND_FCS(buf, index, fcs);

	pppControl->OutMessageContainer.Length = index;

	PppCreateMessageFrame();

	PppSendToCallback(pppControl, pppControl->OutFrameContainer.Buf, pppControl->OutFrameContainer.Length);

	PPP_LOG_EXIT();
}

static void lcpSendNextEchoReq(unsigned long arg)
{
	PppControlS *pppControl=(PppControlS *)arg;
	QueueMessageS qMsg;

	PPP_LOG_ENTRY(MDB_lcpSendNextEchoReq);

	if (pppControl->isLcpEchoRequestEnabled) {
		PPP_LOG();
		qMsg.Type = PPP_SEND_ECHO_REQ;

		PPP_SEND_Q_MSG(pppControl->MsgQRef, &qMsg, sizeof(qMsg));
	}

	PPP_LOG_EXIT();
}

static void lcpHandleEchoReqAck(PppControlS *pppControl)
{
	PPP_LOG_ENTRY(MDB_lcpHandleEchoReqAck);
	del_timer(&pppControl->LcpEchoReqTimeoutTimer);

	PPP_LOG_1(pppControl->lcpEchoReqTimeoutCount);
	pppControl->lcpEchoReqTimeoutCount = 0;

	lcpStartEchoReqTimer(pppControl, LCP_ECHO_REQ_INTERVAL_TIME, lcpSendNextEchoReq);

	PPP_LOG_EXIT();
}

static void lcpHandleEchoReqTimeout(unsigned long arg)
{
	PppControlS *pppControl = (PppControlS *)arg;
	QueueMessageS qMsg;

	PPP_LOG_ENTRY(MDB_lcpHandleEchoReqTimeout);

	qMsg.Type = PPP_LCP_ECHO_REQ_TIMEOUT;
	qMsg.Ptr = NULL;
	qMsg.Size = 0;

	PPP_SEND_Q_MSG(pppControl->MsgQRef, &qMsg, sizeof(qMsg));

	PPP_LOG_EXIT();
}

static void lcpHandleTerminateReq(PppControlS *pppControl)
{
	U_CHAR *buf = pppControl->OutMessageContainer.Buf;
	U_INT index = 0;
	U_SHORT fcs = PPP_INITFCS;

	PPP_LOG_ENTRY(MDB_lcpHandleTerminateReq);
	UNUSEDPARAM(fcs);

	del_timer(&pppControl->PppTimer);
	del_timer(&pppControl->LcpEchoReqTimeoutTimer);

	PPP_LOG();
	PPP_APPEND(buf, index, PPP_ALLSTATIONS, fcs);
	PPP_APPEND(buf, index, PPP_UI, fcs);

	PPP_APPEND_SHORT(buf, index, PPP_LCP, fcs);

	PPP_APPEND(buf, index, TERMINATE_ACK, fcs);
	PPP_APPEND(buf, index, pppControl->LastRecvId, fcs);
	PPP_APPEND_SHORT(buf, index, 4, fcs);

	pppControl->OutMessageContainer.Length = index;

	PppCreateMessageFrame();

	PppSendToCallback(pppControl, pppControl->OutFrameContainer.Buf, pppControl->OutFrameContainer.Length);

	PppTerminate();

	PPP_LOG_EXIT();

}

static void lcpSendReject(PppControlS *pppControl, LcpOptionS *lcpOption)
{
	U_CHAR *buf;
	U_INT index;
	U_SHORT fcs = PPP_INITFCS;

	PPP_LOG_ENTRY(MDB_lcpSendReject);
	UNUSEDPARAM(fcs);

	buf = pppControl->OutMessageContainer.Buf;

	index = 0;

	if (!pppControl->lcpXmitParams.isAcfcEnabled) {
		PPP_LOG_1(fcs);
		PPP_APPEND(buf, index, PPP_ALLSTATIONS, fcs);
		PPP_LOG_1(fcs);
		PPP_APPEND(buf, index, PPP_UI, fcs);
		PPP_LOG_1(fcs);
	}

	PPP_APPEND_SHORT(buf, index, PPP_LCP, fcs);
	PPP_LOG_1(fcs);

	PPP_APPEND(buf, index, CONFIG_REJ, fcs);
	PPP_LOG_1(fcs);
	PPP_APPEND(buf, index, pppControl->LastRecvId, fcs);
	PPP_LOG_1(fcs);
	PPP_APPEND_SHORT(buf, index, 4 + lcpOption->Length, fcs);
	PPP_LOG_1(fcs);
	PPP_LOG_1(4 + lcpOption->Length);

	{
		U_INT l = lcpOption->Length;
		U_CHAR *p = (U_CHAR *)lcpOption;

		PPP_LOG_1(l);
		while (l-- > 0) {
			PPP_APPEND(buf, index, *p, fcs);
			PPP_LOG_1(fcs);
			PPP_LOG_1(*p);
			p++;
		}

	}

	PPP_APPEND_FCS(buf, index, fcs);
	pppControl->OutMessageContainer.Length = index;

	PppCreateMessageFrame();

	PppSendToCallback(pppControl, pppControl->OutFrameContainer.Buf, pppControl->OutFrameContainer.Length);

	PPP_LOG_EXIT();
}

/************************************************************************/
/* lcpSendAck                                                           */
/* Send ack to other peer                                               */
/* Use same frame buffer, just contol code and FCS changed              */
/************************************************************************/
static void lcpSendAck(PppControlS *pppControl)
{
	U_CHAR *buf = pppControl->OutMessageContainer.Buf;
	LcpMessageS *inLcpMsg = (LcpMessageS *)pppControl->InMessage->Data;
	U_CHAR *inbuf = (U_CHAR *)&inLcpMsg->Id;	//pppControl->InMessage->Data; // point to LcpMessage.Id, skipping Type
	U_SHORT inLen;		// length of LCP message without message type, which is modified.
	U_INT index = 0;
	U_SHORT fcs = PPP_INITFCS;

	PPP_LOG_ENTRY(MDB_lcpSendAck);
	UNUSEDPARAM(fcs);

	PPP_LOG_2(inLcpMsg->Id, inLcpMsg->Length);
	GETSHORT_NOINC(inLen, &inLcpMsg->Length);
	inLen--;

	PPP_LOG_1(inLen);

	if (!pppControl->lcpXmitParams.isAcfcEnabled) {
		PPP_LOG();
		PPP_APPEND(buf, index, PPP_ALLSTATIONS, fcs);
		PPP_APPEND(buf, index, PPP_UI, fcs);
	}

	PPP_APPEND_SHORT(buf, index, PPP_LCP, fcs);

	PPP_APPEND(buf, index, CONFIG_ACK, fcs);

	PPP_LOG_1(inLen);

	while (inLen-- > 0) {
		PPP_APPEND(buf, index, *inbuf, fcs);
		inbuf++;
	}

	PPP_APPEND_FCS(buf, index, fcs);

	pppControl->OutMessageContainer.Length = index;

	PppCreateMessageFrame();

	PppSendToCallback(pppControl, pppControl->OutFrameContainer.Buf, pppControl->OutFrameContainer.Length);

	PPP_LOG_EXIT();
}

static void lcpHandleConfigNack(PppControlS *pppControl)
{
	if (pppControl->lcpXmitParams.AuthProtocol.Proto == PPP_PAP) {
		PPP_LCP_SET_AUTH_PROTO(pppControl->lcpXmitParams, PPP_CHAP);
		LcpSendConfigReq(pppControl);
	} else {
		del_timer(&pppControl->PppTimer);
		LcpSendTerminateReq(pppControl);
	}

}

static void lcpHandleConfigAck(PppControlS *pppControl)
{
	PPP_LOG_ENTRY(MDB_lcpHandleConfigAck);

	if (PPP_IS_BIT_SET(pppControl->lcpXmitParams.supportedParams, LCP_PFC))
	{
		PPP_LCP_SET_PFC(pppControl->lcpXmitParams, TRUE);
	}
	else
	{
		PPP_LCP_SET_PFC(pppControl->lcpXmitParams,FALSE);
	}

	if (PPP_IS_BIT_SET(pppControl->lcpXmitParams.supportedParams, LCP_ACFC))
	{
		PPP_LCP_SET_ACFC(pppControl->lcpXmitParams, TRUE);
	}
	else
	{
		PPP_LCP_SET_ACFC(pppControl->lcpXmitParams,FALSE);
	}

	if (pppControl->lcpXmitParams.AuthProtocol.Proto == PPP_CHAP)
	{
		ChapSendAuthChallenge(pppControl);
	}

	PPP_LOG_EXIT();

}

static void lcpHandleConfigRej(PppControlS *pppControl)
{
	LcpMessageS *lcpMessage = (LcpMessageS *)pppControl->InMessage->Data;
	U_CHAR *pData = lcpMessage->Data;
	U_SHORT len;
	LcpOptionS *lcpOption;
	QueueMessageS qMsg;

	PPP_LOG_ENTRY(MDB_lcpHandleConfigRej);

	GETSHORT_NOINC(len, &lcpMessage->Length);

	PPP_LOG_1(len);

	len -= 4;		//decrease header

	PPP_LOG_1(pppControl->lcpXmitParams.supportedParams);
	while (len > 0) {
		lcpOption = (LcpOptionS *) pData;

		// clear option from supported parameters bitfield
		PPP_CLEAR_BIT(pppControl->lcpXmitParams.supportedParams, lcpOption->Type);

		len -= lcpOption->Length;
	}

	PPP_LOG_1(pppControl->lcpXmitParams.supportedParams);

	qMsg.Type = PPP_MSG_SEND_REQ;

	PPP_SEND_Q_MSG(pppControl->MsgQRef, &qMsg, sizeof(qMsg));

	PPP_LOG_EXIT();

}

static void lcpHandleConfigReq(PppControlS *pppControl)
{
	LcpMessageS *lcpMessage = (LcpMessageS *)pppControl->InMessage->Data;
	U_CHAR *pData = lcpMessage->Data;
	U_SHORT len;
	LcpOptionS *lcpOption = (LcpOptionS *)pData;
	BOOL isAckMessage = TRUE;

	PPP_LOG_ENTRY(MDB_lcpHandleConfigReq);
	GETSHORT_NOINC(len, &lcpMessage->Length);
	PPP_LOG_1(len);
	len -= 4;		//decrease header
	while (len > 0) {
		lcpOption = (LcpOptionS *)pData;
		PPP_LOG_1(lcpOption->Type);
		switch(lcpOption->Type) {
		case LCP_ACCM:
		{
			U_INT accm;
			U_CHAR *p = lcpOption->Data;
			GETLONG(accm, p);
			break;
		}
		case LCP_MAGIC:
		{
			U_CHAR *p = lcpOption->Data;
			GETLONG(pppControl->lcpRecvParams.MagicNumber.Number, p);
			break;
		}
		case LCP_PFC:
		{
			pppControl->lcpRecvParams.isPfcEnabled = TRUE;
			break;
		}
		case LCP_ACFC:
		{
			pppControl->lcpRecvParams.isAcfcEnabled = TRUE;
			break;
		}
		case LCP_AUTH:
		{
			break;
		}
		case LCP_MRU:
		{
			U_SHORT *p = (U_SHORT *)lcpOption->Data;

			GETLONG(pppControl->lcpRecvParams.Mru.Mru, p);
			PPP_LOG_1(*p);
			break;
		}
		default:
		{
			isAckMessage = FALSE;
			len = 0;	// exit loop and reject
			break;
		}
		}
		if (len) {
			len -= lcpOption->Length;
			pData += lcpOption->Length;
		}
	}
	if (isAckMessage) {
		QueueMessageS qMsg;
		lcpSendAck(pppControl);
		qMsg.Type = PPP_MSG_SEND_REQ;
		PPP_LOG_1(qMsg.Type);
		PPP_SEND_Q_MSG(pppControl->MsgQRef, &qMsg, sizeof(qMsg));
	} else {
		PPP_LOG();
		lcpSendReject(pppControl, lcpOption);
	}
	PPP_LOG_EXIT();
}

/************************************************************************/
// CcpHandleIncomingFrame
// CCP is rejected using LCP Protocol Reject message, so implementation's here
/************************************************************************/
void CcpHandleIncomingFrame(PppControlS *pppControl)
{
	U_CHAR *buf;
	U_INT index;
	U_SHORT fcs = PPP_INITFCS;
	LcpMessageS *lcpMessage = (LcpMessageS *)pppControl->InMessage->Data;
	U_SHORT inLen;

	PPP_LOG_ENTRY(MDB_CcpHandleIncomingFrame);
	UNUSEDPARAM(fcs);

	GETSHORT_NOINC(inLen, &lcpMessage->Length);

	PPP_LOG_1(inLen);

	buf = pppControl->OutMessageContainer.Buf;

	index = 0;

	PPP_APPEND(buf, index, PPP_ALLSTATIONS, fcs);
	PPP_APPEND(buf, index, PPP_UI, fcs);

	PPP_APPEND_SHORT(buf, index, PPP_LCP, fcs);

	PPP_APPEND(buf, index, PROTO_REJECT, fcs);
	PPP_APPEND(buf, index, 0, fcs);
	PPP_APPEND_SHORT(buf, index, 4 + 2 + inLen, fcs);	// Header + 2 bytes for 0x80FD + CCP message length

	//Append received message
	{
		U_CHAR *p = (U_CHAR *)pppControl->InMessage;

		inLen += 2;	//including CCP protocol number = 0x80FD

		while (inLen-- > 0) {
			PPP_APPEND(buf, index, *p, fcs);
			p++;
		}

	}

	PPP_APPEND_FCS(buf, index, fcs);
	pppControl->OutMessageContainer.Length = index;

	PppCreateMessageFrame();

	PppSendToCallback(pppControl, pppControl->OutFrameContainer.Buf, pppControl->OutFrameContainer.Length);

	PPP_LOG_EXIT();
}

/************************************************************************/
// Ipv6cpHandleIncomingFrame
// Ipv6CP is rejected using LCP Protocol Reject message, so implementation's here
/************************************************************************/
void Ipv6cpHandleIncomingFrame(PppControlS *pppControl)
{
	U_CHAR *buf;
	U_INT index;
	U_SHORT fcs = PPP_INITFCS;
	LcpMessageS *lcpMessage = (LcpMessageS *)pppControl->InMessage->Data;
	U_SHORT inLen;

	PPP_LOG_ENTRY(MDB_pv6cpHandleIncomingFrame);
	UNUSEDPARAM(fcs);

	GETSHORT_NOINC(inLen, &lcpMessage->Length);

	PPP_LOG_1(inLen);

	buf = pppControl->OutMessageContainer.Buf;

	index = 0;

	PPP_APPEND(buf, index, PPP_ALLSTATIONS, fcs);
	PPP_APPEND(buf, index, PPP_UI, fcs);

	PPP_APPEND_SHORT(buf, index, PPP_LCP, fcs);

	PPP_APPEND(buf, index, PROTO_REJECT, fcs);
	PPP_APPEND(buf, index, 0, fcs);
	PPP_APPEND_SHORT(buf, index, 4 + 2 + inLen, fcs);	// Header + 2 bytes for 0x8057 + IPv6CP message length

	//Append received message
	{
		U_CHAR *p = (U_CHAR *)pppControl->InMessage;

		inLen += 2;	//including IPv6CP protocol number = 0x8057

		while (inLen-- > 0) {
			PPP_APPEND(buf, index, *p, fcs);
			p++;
		}

	}

	PPP_APPEND_FCS(buf, index, fcs);
	pppControl->OutMessageContainer.Length = index;

	PppCreateMessageFrame();

	PppSendToCallback(pppControl, pppControl->OutFrameContainer.Buf, pppControl->OutFrameContainer.Length);

	PPP_LOG_EXIT();
}

void LcpHandleIncomingFrame(PppControlS *pppControl)
{
	LcpMessageS *lcpMessage = (LcpMessageS *)pppControl->InMessage->Data;
	PPP_LOG_ENTRY(MDB_LcpHandleIncomingFrame);

	pppControl->LastRecvId = lcpMessage->Id;

	PPP_LOG_1(pppControl->LastRecvId);
	PPP_LOG_1(lcpMessage->Type);

	switch (lcpMessage->Type) {
	case CONFIG_REQ:
		lcpHandleConfigReq(pppControl);
		break;

	case CONFIG_REJ:
		lcpHandleConfigRej(pppControl);
		break;

	case CONFIG_ACK:
		lcpHandleConfigAck(pppControl);
		break;

	case CONFIG_NAK:
		lcpHandleConfigNack(pppControl);
		break;

	case TERMINATE_REQ:
		lcpHandleTerminateReq(pppControl);
		break;

	case TERMINATE_ACK:
		del_timer(&pppControl->PppTimer);
		PppTerminate();
		break;

	case ECHO_REQUEST:
		lcpHandleEchoRequest(pppControl);
		break;

	case ECHO_REPLY:
		lcpHandleEchoReqAck(pppControl);
		break;

		//Drop unsupported message
	default:
		break;

	}

	PPP_LOG_EXIT();
}

void LcpSendTerminateReq(PppControlS *pppControl)
{
	U_CHAR *buf = pppControl->OutMessageContainer.Buf;
	U_INT index = 0;
	U_SHORT len = PPP_HDRLEN;
	U_SHORT fcs = PPP_INITFCS;

	PPP_LOG_ENTRY(MDB_LcpSendTerminateReq);
	UNUSEDPARAM(fcs);

	del_timer(&pppControl->LcpEchoReqTimeoutTimer);

	if (!pppControl->lcpXmitParams.isAcfcEnabled) {
		PPP_LOG();
		PPP_APPEND(buf, index, PPP_ALLSTATIONS, fcs);
		PPP_APPEND(buf, index, PPP_UI, fcs);
	}

	PPP_APPEND_SHORT(buf, index, PPP_LCP, fcs);

	PPP_APPEND(buf, index, TERMINATE_REQ, fcs);
	PPP_LOG_1(pppControl->LastXmitId);
	PPP_APPEND(buf, index, pppControl->LastXmitId++, fcs);

	PPP_APPEND_SHORT(buf, index, len, fcs);

	PPP_APPEND_FCS(buf, index, fcs);

	PPP_LOG_1(fcs);

	pppControl->OutMessageContainer.Length = index;
	PPP_LOG_1(index);

	PppCreateMessageFrame();

	PppSendToCallback(pppControl, pppControl->OutFrameContainer.Buf, pppControl->OutFrameContainer.Length);

	PPP_LOG_EXIT();

}

void LcpSendConfigReq(PppControlS *pppControl)
{
	U_CHAR *buf = pppControl->OutMessageContainer.Buf;
	U_INT index = 0;
	U_INT lenIndex;
	U_SHORT len = PPP_HDRLEN;
	U_SHORT fcs = PPP_INITFCS;
	U_CHAR i;
	U_INT magic;

	PPP_LOG_ENTRY(MDB_LcpSendConfigReq);
	UNUSEDPARAM(fcs);

	if (!pppControl->lcpXmitParams.isAcfcEnabled) {
		PPP_LOG();
		PPP_APPEND(buf, index, PPP_ALLSTATIONS, fcs);
		PPP_APPEND(buf, index, PPP_UI, fcs);
	}

	PPP_APPEND_SHORT(buf, index, PPP_LCP, fcs);

	PPP_APPEND(buf, index, CONFIG_REQ, fcs);
	PPP_LOG_1(pppControl->LastXmitId);
	PPP_APPEND(buf, index, pppControl->LastXmitId++, fcs);

	lenIndex = index;

	index += 2;

	for (i = 0; i < LCP_MAX_OPTIONS; i++) {
		if (PPP_IS_BIT_SET(pppControl->lcpXmitParams.supportedParams, i)) {
			switch (i) {
			case LCP_MRU:
				PPP_APPEND(buf, index, pppControl->lcpXmitParams.Mru.Header.Type, fcs);
				PPP_APPEND(buf, index, pppControl->lcpXmitParams.Mru.Header.Length, fcs);
				PPP_APPEND_SHORT(buf, index, pppControl->lcpXmitParams.Mru.Mru, fcs);
				PPP_LOG_1(pppControl->lcpXmitParams.Mru.Mru);
				len += LCP_MRU_LEN;
				break;

			case LCP_ACCM:
				PPP_APPEND(buf, index, pppControl->lcpXmitParams.Accm.Header.Type, fcs);
				PPP_APPEND(buf, index, pppControl->lcpXmitParams.Accm.Header.Length, fcs);
				PPP_APPEND_LONG(buf, index, pppControl->lcpXmitParams.Accm.Accm, fcs);
				PPP_LOG_1(pppControl->lcpXmitParams.Accm.Accm);
				len += LCP_ACCM_LEN;
				break;

			case LCP_AUTH:
				if (pppControl->lcpXmitParams.AuthProtocol.Proto == PPP_PAP) {
					PPP_APPEND(buf, index, pppControl->lcpXmitParams.AuthProtocol.Header.Type, fcs);
					PPP_APPEND(buf, index, pppControl->lcpXmitParams.AuthProtocol.Header.Length,
						   fcs);
					PPP_APPEND_SHORT(buf, index, pppControl->lcpXmitParams.AuthProtocol.Proto, fcs);
					PPP_LOG_1(pppControl->lcpXmitParams.AuthProtocol.Proto);
					len += LCP_AUTH_LEN;

				} else if (pppControl->lcpXmitParams.AuthProtocol.Proto == PPP_CHAP) {
					PPP_APPEND(buf, index, pppControl->lcpXmitParams.AuthProtocol.Header.Type, fcs);
					PPP_APPEND(buf, index, pppControl->lcpXmitParams.AuthProtocol.Header.Length + 1,
						   fcs);
					PPP_APPEND_SHORT(buf, index, pppControl->lcpXmitParams.AuthProtocol.Proto, fcs);
					PPP_APPEND(buf, index, CHAP_MD5, fcs);
					PPP_LOG_1(pppControl->lcpXmitParams.AuthProtocol.Proto);
					len += (LCP_AUTH_LEN + 1);
				} else {
					PPP_LOG_1(pppControl->lcpXmitParams.AuthProtocol.Proto);
				}
				break;

			case LCP_MAGIC:
				PPP_APPEND(buf, index, pppControl->lcpXmitParams.MagicNumber.Header.Type, fcs);
				PPP_APPEND(buf, index, pppControl->lcpXmitParams.MagicNumber.Header.Length, fcs);
				magic = pppControl->lcpRecvParams.MagicNumber.Number;
				PPP_LOG_1(magic);
				magic ^= 0x7EA5A57E;
				PPP_LOG_1(magic);
				pppControl->lcpXmitParams.MagicNumber.Number = magic;
				PPP_APPEND_LONG(buf, index, magic, fcs);
				PPP_LOG_1(pppControl->lcpXmitParams.MagicNumber.Number);
				len += LCP_MAGIC_LEN;
				break;

			case LCP_PFC:
				PPP_APPEND(buf, index, LCP_PFC, fcs);
				PPP_APPEND(buf, index, 2, fcs);
				PPP_LOG();
				len += LCP_PFC_LEN;
				break;

			case LCP_ACFC:
				PPP_APPEND(buf, index, LCP_ACFC, fcs);
				PPP_APPEND(buf, index, 2, fcs);
				PPP_LOG();
				len += LCP_ACFC_LEN;
				break;
			}
		}

	}

	// Put length in right place
	PPP_APPEND_SHORT(buf, lenIndex, len, fcs);

	PPP_APPEND_FCS(buf, index, fcs);

	PPP_LOG_1(fcs);

	pppControl->OutMessageContainer.Length = index;
	PPP_LOG_1(index);

	PppCreateMessageFrame();

	PppSendToCallback(pppControl, pppControl->OutFrameContainer.Buf, pppControl->OutFrameContainer.Length);

	PPP_LOG_EXIT();
}

void LcpSendEchoReq(PppControlS *pppControl)
{
	U_CHAR *buf = pppControl->OutMessageContainer.Buf;
	U_INT index = 0;
	U_INT lenIndex;
	U_SHORT len = PPP_HDRLEN;
	U_SHORT fcs = PPP_INITFCS;
	U_INT magic;

	PPP_LOG_ENTRY(MDB_LcpSendEchoReq);
	UNUSEDPARAM(fcs);

	if (!pppControl->lcpXmitParams.isAcfcEnabled) {
		PPP_LOG();
		PPP_APPEND(buf, index, PPP_ALLSTATIONS, fcs);
		PPP_APPEND(buf, index, PPP_UI, fcs);
	}

	PPP_APPEND_SHORT(buf, index, PPP_LCP, fcs);

	PPP_APPEND(buf, index, ECHO_REQUEST, fcs);
	PPP_APPEND(buf, index, pppControl->LastXmitId++, fcs);

	lenIndex = index;

	index += 2;

	magic = pppControl->lcpXmitParams.MagicNumber.Number;
	len += 4;
	PPP_LOG_1(magic);

	PPP_APPEND_LONG(buf, index, magic, fcs);

	// Put length in right place
	PPP_APPEND_SHORT(buf, lenIndex, len, fcs);

	PPP_APPEND_FCS(buf, index, fcs);

	pppControl->OutMessageContainer.Length = index;

	PppCreateMessageFrame();

	lcpStartEchoReqTimer(pppControl, LCP_ECHO_REQ_TIMEOUT, lcpHandleEchoReqTimeout);

	PppSendToCallback(pppControl, pppControl->OutFrameContainer.Buf, pppControl->OutFrameContainer.Length);

	PPP_LOG_EXIT();

}

void LcpKickoffEchoRequest(PppControlS *pppControl)
{
	lcpStartEchoReqTimer(pppControl, LCP_ECHO_REQ_INTERVAL_TIME, lcpSendNextEchoReq);
}
