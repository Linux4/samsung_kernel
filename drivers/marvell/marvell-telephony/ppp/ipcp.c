/*
 * ipcp.c -- ipcp module
 *
 * (C) Copyright [2006-2008] Marvell International Ltd.
 * All Rights Reserved
 *
 */
#include "ppp.h"
#include "ipcp.h"
#include "ppp_log.h"

void IpcpUpdateIpParams(PppControlS *pppControl, IpcpConnectionParamsS *ipcpConnectionParams)
{
	PPP_LOG_ENTRY(MDB_IpcpUpdateIpParams);

	pppControl->ipcpXmitParams.IpAddress = ipcpConnectionParams->IpAddress;
	pppControl->ipcpXmitParams.PrimaryDns = ipcpConnectionParams->PrimaryDns;
	pppControl->ipcpXmitParams.SecondaryDns = ipcpConnectionParams->SecondaryDns;

	PPP_LOG_3(pppControl->ipcpXmitParams.IpAddress, pppControl->ipcpXmitParams.PrimaryDns,
		  pppControl->ipcpXmitParams.SecondaryDns);
	PPP_LOG_EXIT();
}

void IpcpSendConfigReq(PppControlS *pppControl)
{
	U_CHAR *buf;
	U_SHORT index, lenIndex, msgLen;
	U_SHORT fcs = PPP_INITFCS;

	PPP_LOG_ENTRY(MDB_ipcpSendConfigReq);
	UNUSEDPARAM(fcs);
	buf = pppControl->OutMessageContainer.Buf;

	index = 0;
	msgLen = 0;

	if (!pppControl->lcpXmitParams.isAcfcEnabled) {
		PPP_LOG();
		PPP_APPEND(buf, index, PPP_ALLSTATIONS, fcs);
		PPP_APPEND(buf, index, PPP_UI, fcs);
	}

	PPP_APPEND_SHORT(buf, index, (U_SHORT)PPP_IPCP, fcs);
	PPP_APPEND(buf, index, CONFIG_REQ, fcs);
	msgLen++;
	PPP_APPEND(buf, index, pppControl->LastXmitId++, fcs);
	msgLen++;

	lenIndex = index;
	index += 2;
	msgLen += 2;

	//IP
	PPP_APPEND(buf, index, IPCP_OPT_IP_ADDRESS, fcs);
	PPP_APPEND(buf, index, 6, fcs);
	PPP_APPEND_LONG(buf, index, pppControl->ipcpXmitParams.IpAddress, fcs);
	msgLen += 6;
	PPP_LOG_1(pppControl->ipcpXmitParams.IpAddress);

	PPP_APPEND_SHORT(buf, lenIndex, msgLen, fcs);

	PPP_APPEND_FCS(buf, index, fcs);

	pppControl->OutMessageContainer.Length = index;

	PppCreateMessageFrame();

	PppSendToCallback(pppControl, pppControl->OutFrameContainer.Buf, pppControl->OutFrameContainer.Length);

	PPP_LOG_EXIT();
}

static void ipcpSendConfigNak(PppControlS *pppControl)
{
	U_CHAR *buf;
	U_SHORT index;
	U_SHORT fcs = PPP_INITFCS;
	U_SHORT lenIndex;

	PPP_LOG_ENTRY(MDB_ipcpSendConfigNak);
	UNUSEDPARAM(fcs);
	buf = pppControl->OutMessageContainer.Buf;

	index = 0;

	if (!pppControl->lcpXmitParams.isAcfcEnabled) {
		PPP_LOG();
		PPP_APPEND(buf, index, PPP_ALLSTATIONS, fcs);
		PPP_APPEND(buf, index, PPP_UI, fcs);
	}

	PPP_APPEND_SHORT(buf, index, (U_SHORT)PPP_IPCP, fcs);
	PPP_APPEND(buf, index, CONFIG_NAK, fcs);
	PPP_APPEND(buf, index, pppControl->LastRecvId, fcs);

	lenIndex = index;
	index += 2;

	//IP
	PPP_APPEND(buf, index, IPCP_OPT_IP_ADDRESS, fcs);
	PPP_APPEND(buf, index, 6, fcs);
	PPP_APPEND_LONG(buf, index, pppControl->ipcpXmitParams.IpAddress, fcs);
	PPP_LOG_1(pppControl->ipcpXmitParams.IpAddress);

	//Prim DNS
	PPP_APPEND(buf, index, IPCP_OPT_PRIMARY_DNS_ADDRESS, fcs);
	PPP_APPEND(buf, index, 6, fcs);
	PPP_APPEND_LONG(buf, index, pppControl->ipcpXmitParams.PrimaryDns, fcs);
	PPP_LOG_1(pppControl->ipcpXmitParams.PrimaryDns);

	//Second DNS
	PPP_APPEND(buf, index, IPCP_OPT_SECONDARY_DNS_ADDRESS, fcs);
	PPP_APPEND(buf, index, 6, fcs);
	PPP_APPEND_LONG(buf, index, pppControl->ipcpXmitParams.SecondaryDns, fcs);
	PPP_LOG_1(pppControl->ipcpXmitParams.SecondaryDns);

	PPP_LOG_1(index);
	PPP_APPEND_SHORT(buf, lenIndex, index - 2, fcs);

	PPP_APPEND_FCS(buf, index, fcs);

	pppControl->OutMessageContainer.Length = index;

	PppCreateMessageFrame();

	PppSendToCallback(pppControl, pppControl->OutFrameContainer.Buf, pppControl->OutFrameContainer.Length);

	PPP_LOG_EXIT();

}

static void ipcpSendConfigAck(PppControlS *pppControl)
{
	PppMessageHeaderS *ipcpMessageHeader = (PppMessageHeaderS *)pppControl->InMessage->Data;
	U_CHAR *buf, *p;
	U_SHORT index;
	U_SHORT fcs = PPP_INITFCS;
	U_SHORT len;

	PPP_LOG_ENTRY(MDB_ipcpSendConfigAck);
	UNUSEDPARAM(fcs);

	buf = pppControl->OutMessageContainer.Buf;

	index = 0;

	GETSHORT_NOINC(len, &ipcpMessageHeader->Length);

	if (!pppControl->lcpXmitParams.isAcfcEnabled) {
		PPP_LOG();
		PPP_APPEND(buf, index, PPP_ALLSTATIONS, fcs);
		PPP_APPEND(buf, index, PPP_UI, fcs);
	}

	PPP_APPEND_SHORT(buf, index, PPP_IPCP, fcs);
	PPP_APPEND(buf, index, CONFIG_ACK, fcs);
	PPP_APPEND(buf, index, pppControl->LastRecvId, fcs);
	PPP_APPEND_SHORT(buf, index, len, fcs);

	PPP_LOG_1(len);
	len -= PPP_HDRLEN;
	PPP_LOG_1(len);

	p = ipcpMessageHeader->Data;

	while (len-- > 0) {
		PPP_APPEND(buf, index, *p, fcs);
		p++;
	}

	PPP_APPEND_FCS(buf, index, fcs);

	pppControl->OutMessageContainer.Length = index;

	PppCreateMessageFrame();

	// signal ourself that we ack'ed other peer IPCP Configure Request
	pppControl->ipcpXmitAck = TRUE;

	PppSendToCallback(pppControl, pppControl->OutFrameContainer.Buf, pppControl->OutFrameContainer.Length);

	PPP_LOG_EXIT();
}

static void ipcpSendConfigReject(PppControlS *pppControl, PppOptionS *ipcpRejectsList[], U_CHAR ipcpRejectsListCount)
{
	U_CHAR *buf, *p;
	U_INT index, lenPos;
	PppOptionS *ipcpOption;
	U_SHORT fcs = PPP_INITFCS;
	U_SHORT ipcpMessageLength;
	U_CHAR ipcpOptLen;
	U_CHAR i;

	PPP_LOG_ENTRY(MDB_ipcpSendConfigReject);
	UNUSEDPARAM(fcs);

	buf = pppControl->OutMessageContainer.Buf;

	index = 0;

	if (!pppControl->lcpXmitParams.isAcfcEnabled) {
		PPP_LOG();
		PPP_APPEND(buf, index, PPP_ALLSTATIONS, fcs);
		PPP_APPEND(buf, index, PPP_UI, fcs);
	}

	PPP_APPEND_SHORT(buf, index, PPP_IPCP, fcs);

	PPP_APPEND(buf, index, CONFIG_REJ, fcs);
	PPP_APPEND(buf, index, pppControl->LastRecvId, fcs);

	PPP_LOG_1(pppControl->LastRecvId);

	// save length position
	lenPos = index;
	index += 2;
	ipcpMessageLength = PPP_HDRLEN;

	for (i = 0; i < ipcpRejectsListCount; i++) {
		ipcpOptLen = ipcpRejectsList[i]->Length - 2;
		ipcpMessageLength += ipcpRejectsList[i]->Length;
		ipcpOption = ipcpRejectsList[i];

		PPP_APPEND(buf, index, ipcpRejectsList[i]->Type, fcs);
		PPP_APPEND(buf, index, ipcpRejectsList[i]->Length, fcs);
		PPP_LOG_2(ipcpRejectsList[i]->Type, ipcpRejectsList[i]->Length);

		p = ipcpOption->Data;
		PPP_LOG_1(ipcpOptLen);
		while (ipcpOptLen > 0) {
			PPP_APPEND(buf, index, *p, fcs);
			p++;
			ipcpOptLen--;
		}
	}

	// put mesasge length where it belongs
	PPP_APPEND_SHORT(buf, lenPos, ipcpMessageLength, fcs);
	PPP_LOG_1(ipcpMessageLength);

	PPP_APPEND_FCS(buf, index, fcs);

	pppControl->OutMessageContainer.Length = index;

	PppCreateMessageFrame();

	PppSendToCallback(pppControl, pppControl->OutFrameContainer.Buf, pppControl->OutFrameContainer.Length);

	PPP_LOG_EXIT();
}

/*
 *	Other peer ack'ed our request. We're ready to do IP Tx/Rx
 */
static void ipcpHandleConfigAck(PppControlS *pppControl)
{
	QueueMessageS qMsg;

	PPP_LOG_ENTRY(MDB_ipcpHandleConfigAck);
	pppControl->ipcpRecvAck = TRUE;
	pppControl->PppState = PPP_STATE_CONNECTED;
	registerRxCallBack(pppControl->ServiceType, pppRelayMessageFromComm);

	if (pppControl->isLcpEchoRequestEnabled) {
		PPP_LOG();
		qMsg.Type = PPP_KICKOFF_ECHO_REQ;

		PPP_SEND_Q_MSG(pppControl->MsgQRef, &qMsg, sizeof(qMsg));
	}

	PPP_LOG_EXIT();
}

static void ipcpHandleConfigReq(PppControlS *pppControl)
{
	PppMessageHeaderS *ipcpMessageHeader = (LcpMessageS *) pppControl->InMessage->Data;
	PppOptionS *ipcpRejectsList[IPCP_NUM_AVAILABLE_OPTIONS];
	U_CHAR ipcpRejectsListCount = 0;
	U_CHAR *inPtr = ipcpMessageHeader->Data;
	PppOptionS *ipcpCurrentOption;
	U_SHORT ipcpMessageLength;

	PPP_LOG_ENTRY(MDB_ipcpHandleConfigReq);

	GETSHORT_NOINC(ipcpMessageLength, &ipcpMessageHeader->Length);
	PPP_LOG_1(ipcpMessageLength);

	ipcpMessageLength -= PPP_HDRLEN;	// don't need to count header

	PPP_LOG_1(ipcpMessageLength);

	while (ipcpMessageLength > 0) {
		ipcpCurrentOption = (PppOptionS *)inPtr;
		PPP_LOG_1(ipcpCurrentOption->Type);
		switch (ipcpCurrentOption->Type) {
		/*
		*      The following three options should contain the true IP and DNS addresses.
		*      If they don't, a Config-Nak should be sent with correct parameters obtains from network.
		*      These parameters must be fed to PPP before IPCP negotiation starts.
		*      Currently, PPP just accepts the parameters it get.
		*/
		case IPCP_OPT_IP_ADDRESS:
		{
			IpcpIpAddressS *ipcpIpAddress = (IpcpIpAddressS *)ipcpCurrentOption;
			pppControl->ipcpRecvParams.IpRemoteAddress.Length = ipcpIpAddress->Length - 2;
			GETLONG_NOINC(pppControl->ipcpRecvParams.IpRemoteAddress.Address,
					     &ipcpIpAddress->Address);
			pppControl->ipcpRecvParams.IpAddress =
				   pppControl->ipcpRecvParams.IpRemoteAddress.Address;
			PPP_LOG_1(pppControl->ipcpRecvParams.IpRemoteAddress.Length);
			PPP_LOG_1(pppControl->ipcpRecvParams.IpRemoteAddress.Address);
			break;
		}
		case IPCP_OPT_PRIMARY_DNS_ADDRESS:
		{
			IpcpPrimaryDns *ipcpDns = (IpcpPrimaryDns *)ipcpCurrentOption->Data;
			GETLONG_NOINC(pppControl->ipcpRecvParams.PrimaryDns, ipcpDns);
			PPP_LOG_1(pppControl->ipcpRecvParams.PrimaryDns);
			break;
		}
		case IPCP_OPT_SECONDARY_DNS_ADDRESS:
		{
			IpcpSecondaryDns *ipcpDns = (IpcpSecondaryDns *)ipcpCurrentOption->Data;
			GETLONG_NOINC(pppControl->ipcpRecvParams.SecondaryDns, ipcpDns);
			PPP_LOG_1(pppControl->ipcpRecvParams.SecondaryDns);
			break;
		}
		default:
		{
			ipcpRejectsList[ipcpRejectsListCount++] = ipcpCurrentOption;
			PPP_LOG();
			break;
		}
		}
		PPP_LOG_1(ipcpMessageLength);
		ipcpMessageLength -= ipcpCurrentOption->Length;
		PPP_LOG_1(ipcpMessageLength);

		PPP_LOG_1((UINT) inPtr);
		inPtr += ipcpCurrentOption->Length;
		PPP_LOG_1((UINT) inPtr);
	}

	PPP_LOG();

	if (ipcpRejectsListCount > 0) {
		PPP_LOG_1(ipcpRejectsListCount);
		ipcpSendConfigReject(pppControl, ipcpRejectsList, ipcpRejectsListCount);
	}
	else if (pppControl->ipcpXmitParams.IpAddress == 0) {
		// PPP_CONNECT Callback sends "AT+CGDATA=" to activate PDP and get then get IP/DNS address
		if (!pppControl->isConnectIndSent) {
			pppControl->isConnectIndSent = TRUE;
			if(PPP_CONNECT(pppControl->ConnectCallbackFunc, pppControl->Cid))
			{
				QueueMessageS qMsg;
				qMsg.Type = PPP_IPCP_GET_IP_REQ;

				PPP_SEND_Q_MSG(pppControl->MsgQRef, &qMsg, sizeof(qMsg));
			}
			else
			{
				PPP_PRINTF("[PPP] %s: ppp connect fail\n", __func__);
			}
		}
		ipcpSendConfigReject(pppControl, ipcpRejectsList, 0);
	} else if (pppControl->ipcpRecvParams.IpRemoteAddress.Address == 0) {
		PPP_LOG_1(pppControl->ipcpXmitParams.IpAddress);
		ipcpSendConfigNak(pppControl);
		PPP_LOG();
	}
	else {
		ipcpSendConfigAck(pppControl);
		PPP_LOG();
		IpcpSendConfigReq(pppControl);
	}
	PPP_LOG_EXIT();
}

/************************************************************************/
/* APIs                                                                 */
/************************************************************************/
void IpcpHandleIncomingFrame(PppControlS *pppControl)
{
	PppMessageHeaderS *ipcpMessageHeader = (LcpMessageS *)pppControl->InMessage->Data;

	PPP_LOG_ENTRY(MDB_IpcpHandleIncomingFrame);

	pppControl->LastRecvId = ipcpMessageHeader->Id;
	PPP_LOG_1(pppControl->LastRecvId);

	PPP_LOG_1(ipcpMessageHeader->Type);
	switch (ipcpMessageHeader->Type) {
	case CONFIG_REQ:
		ipcpHandleConfigReq(pppControl);
		break;

	case CONFIG_ACK:
		ipcpHandleConfigAck(pppControl);
		break;

	//Drop unsupported message
	default:
		break;
	}
	PPP_LOG_EXIT();
}
