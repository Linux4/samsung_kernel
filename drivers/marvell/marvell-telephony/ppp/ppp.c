/*
 * ppp.c -- ppp main module
 *
 * (C) Copyright [2006-2008] Marvell International Ltd.
 * All Rights Reserved
 *
 */


#include <linux/module.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/udp.h>

#include "common_datastub.h"
#include "ppp.h"
#include "lcp.h"
#include "pap.h"
#include "chap.h"
#include "ipcp.h"
#include "ppp_log.h"
#include "modem_if.h"

PppControlS PppControl;
static U_INT PppInitCount = 0;

const U_SHORT fcstab[256] = {
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
	0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
	0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
	0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
	0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
	0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
	0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
	0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
	0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
	0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
	0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
	0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
	0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
	0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
	0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
	0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
	0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
	0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
	0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
	0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
	0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
	0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
	0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
	0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
	0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
	0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
	0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
	0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
	0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
	0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
	0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
	0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

static int __init ppp_module_init(void)
{
	dataCbFuncs DataCbFuncs = { PppInit, PppDeinit, PppReset, PppSetCid, PppMessageReq, PppUpdateIpParams };
	modem_register_data_service(DataCbFuncs);
	return 0;
}

static void __exit ppp_module_exit(void)
{
	modem_unregister_data_service();
}

module_init(ppp_module_init);
module_exit(ppp_module_exit);

#define DRIVER_VERSION  "10-Sept-2010"
#define DRIVER_DESC "PPP Services for ACM Driver"

MODULE_VERSION(DRIVER_VERSION);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("Marvell International Ltd");
MODULE_LICENSE("GPL");

extern void sendPSDData(int cid, struct sk_buff *skb);
static void pppSendDataReqtoPSDChannel(unsigned char cid, char *buf, int len)
{
	struct sk_buff *skb;

	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb) {
		PPP_PRINTF(KERN_ERR "[PPP] %s: malloc error\n", __func__);
		return;
	}
	PPP_MEMCPY(skb_put(skb, len), buf, len);
	sendPSDData(cid, skb);
}

static void pppStartTimer(PppControlS *pppControl, U_INT timeout, PppTimeoutCallbackFunc toCb)
{
	init_timer(&pppControl->PppTimer);
	pppControl->PppTimer.expires = jiffies + msecs_to_jiffies(timeout);
	pppControl->PppTimer.function = toCb;
	pppControl->PppTimer.data = (unsigned long)pppControl;
	add_timer(&pppControl->PppTimer);
}

static MessageBufS* PppAllocateMessage(U_INT size)
{
	MessageBufS *p;

	p = PPP_MALLOC(sizeof(MessageBufS));

	if (p) {
		p->Buf = (U_CHAR *)PPP_MALLOC(size);
		if (p->Buf == NULL) {
			PPP_FREE(p);
			return NULL;
		}
		p->Length = 0;
	}

	return p;
}

static void PppFreeMessage(MessageBufS *p)
{
	PPP_FREE(p->Buf);
	PPP_FREE(p);
}

static void pppHandleTerminateTimoeut(unsigned long val)
{
	UNUSEDPARAM(val);	//PppControlS *pppControl = (PppControlS *)val;
	PppTerminate();
}

int pppRelayMessageFromComm(const U_CHAR *buf, U_INT count, U_CHAR cid)
{
	MessageBufS *messageFromComm;
	QueueMessageS qMsg;

	if(cid != PppControl.Cid)
		return 0;
	if (PppControl.PppState == PPP_STATE_CONNECTED) {
		PPP_TRACE_BUF("pppRelayMessageFromComm ", (U_CHAR *)buf, count);

		messageFromComm = PppAllocateMessage(PPP_FRAME_SIZE);
		if (messageFromComm != NULL) {

			PPP_MEMCPY(messageFromComm->Buf, buf, count);
			messageFromComm->Length = count;

			qMsg.Ptr = (U_CHAR *) messageFromComm;
			qMsg.Type = PPP_MESSAGE_FROM_COMM;

			PppControl.QueuedDataInPacketsCount--; // One less available slot in queue
			PPP_SEND_Q_MSG(PppControl.MsgQRef, &qMsg, sizeof(QueueMessageS));
		} else {
			PppControl.CPOOMDrops++;
			PPP_PRINTF("[PPP] %s: Dropped %d CP packets\n", __func__, (int)PppControl.CPOOMDrops);
		}

	}
	return 1;
}

static void pppRelayMessageToComm(PppControlS * pppControl)
{
	U_CHAR *buf = (U_CHAR *) pppControl->InMessage;
	//Adjust length to send to COMM to include 2 bytes that were "dropped" in pppDecodeFrame
	U_INT length = pppControl->InFrameContainer->Length - 1;

	PPP_LOG_ENTRY(MDB_pppRelayMessageToComm);

	PPP_LOG_1(length);

	buf++;

	PPP_TRACE_BUF("Send To Comm: ", buf, length);

	if (pppControl->Cid != 0xFF)
		pppSendDataReqtoPSDChannel(pppControl->Cid, buf, length);

	PPP_LOG_EXIT();

}

static void pppSendCommMessageInd(PppControlS * pppControl, MessageBufS * ptr)
{
	U_CHAR *buf = pppControl->OutMessageContainer.Buf;
	U_CHAR *inbuf = (U_CHAR *) ptr->Buf;
	U_INT inLen = ptr->Length;
	U_INT index = 0;
	U_SHORT fcs = PPP_INITFCS;

	PPP_LOG_ENTRY(MDB_pppSendCommMessageInd);
	UNUSEDPARAM(fcs);

	if (!pppControl->lcpXmitParams.isAcfcEnabled) {
		PPP_LOG();
		PPP_APPEND(buf, index, PPP_ALLSTATIONS, fcs);
		PPP_APPEND(buf, index, PPP_UI, fcs);
	}

	PPP_APPEND(buf, index, PPP_IP, fcs);
	PPP_MEMCPY(&buf[index], inbuf, inLen);
	index += inLen;

	pppControl->OutMessageContainer.Length = index;

	PppCreateMessageFrameIp();

	PppSendToCallback(pppControl, pppControl->OutFrameContainer.Buf, pppControl->OutFrameContainer.Length);

	PPP_LOG_EXIT();

}

static void pppHandleIncomingFrame(PppControlS * pppControl)
{
	PppFrameS *pppFrame;
	U_CHAR *p;
	U_SHORT proto;

	PPP_LOG_ENTRY(MDB_pppHandleIncomingFrame);

	p = pppControl->InFrameContainer->Buf;

	// PPP frame contains address and control fields
	if ((p[0] == PPP_ALLSTATIONS) && (p[1] == PPP_UI)) {
		pppFrame = (PppFrameS *) p;
		pppControl->InMessage = (PppMessageS *) & pppFrame->Message;
		pppControl->InFrameContainer->Length -= 2;	// decrease header size

		PPP_LOG_1(pppControl->InFrameContainer->Length);
	}
	//no address-control. is it allowed?
	else if (pppControl->lcpRecvParams.isAcfcEnabled) {
		pppControl->InMessage = (PppMessageS *) p;
		PPP_LOG();
	}
	//drop bad frame
	else {
		PPP_LOG();
		return;
	}

	GETSHORT_NOINC(proto, &pppControl->InMessage->Protocol);

	PPP_LOG_1(proto);

	switch (proto) {
	case PPP_LCP:
		LcpHandleIncomingFrame(pppControl);
		break;

	case PPP_PAP:
		PapHandleIncomingFrame(pppControl);
		break;

	case PPP_CHAP:
		ChapHandleIncomingFrame(pppControl);
		break;

	case PPP_CCP:
		CcpHandleIncomingFrame(pppControl);
		break;

	case PPP_IPCP:
		IpcpHandleIncomingFrame(pppControl);
		break;

	case PPP_IPv6CP:
		Ipv6cpHandleIncomingFrame(pppControl);
		break;

	default:
		if (proto < 0x4000)
			pppRelayMessageToComm(pppControl);
		break;
	}

	PPP_LOG_EXIT();
}

static void pppProcessQMsg(QueueMessageS * qMsg)
{
	IpcpConnectionParamsS *ipParams;
	PPP_LOG_ENTRY(MDB_pppProcessQMsg);

	switch (qMsg->Type) {
	case PPP_MSG_REQ:
		PppControl.InFrameContainer = (MessageBufS *) qMsg->Ptr;
		pppHandleIncomingFrame(&PppControl);
		PppFreeMessage((MessageBufS *)qMsg->Ptr);
		break;

	case PPP_MSG_SEND_REQ:
		LcpSendConfigReq(&PppControl);
		break;
	case PPP_MSG_SEND_CNF:
		PPP_LOG_1(qMsg->Size);
		PppSendToCallback(&PppControl, qMsg->Ptr, qMsg->Size);
		break;

	case PPP_IPCP_SEND_REQ:
		ipParams = (IpcpConnectionParamsS *) qMsg->Ptr;
		PPP_LOG_3(ipParams->IpAddress, ipParams->PrimaryDns, ipParams->SecondaryDns);
		IpcpUpdateIpParams(&PppControl, ipParams);
		if (TRUE == PppControl.ipcpRecvAck) {
			IpcpSendConfigReq(&PppControl);
		}

		PPP_FREE(qMsg->Ptr);
		break;

	case PPP_MESSAGE_FROM_COMM:
		pppSendCommMessageInd(&PppControl, (MessageBufS *)qMsg->Ptr);
		PppFreeMessage((MessageBufS *)qMsg->Ptr);
		break;

	case PPP_SEND_TERMINATE_REQ:
		pppStartTimer(&PppControl, PPP_TERMINATE_TIMEOUT, pppHandleTerminateTimoeut);
		LcpSendTerminateReq(&PppControl);
		break;

	case PPP_LCP_ECHO_REQ_TIMEOUT:
		PPP_LOG();

		// If we reached max echo reuest timeouts, send terminate request (and print some debug info)
		if (PppControl.lcpEchoReqTimeoutCount >= LCP_ECHO_REQ_NUM_TRIES) {
			PPP_PRINTF("[PPP] %s: %d echo requests failed on timeout, send terminate request\n",
			       __func__, PppControl.lcpEchoReqTimeoutCount);
			LcpSendTerminateReq(&PppControl);
		} else {
			PppControl.lcpEchoReqTimeoutCount++;
			PPP_LOG_1(PppControl.lcpEchoReqTimeoutCount);
			LcpSendEchoReq(&PppControl);
		}
		break;

	case PPP_KICKOFF_ECHO_REQ:
		//Start sending echo requests
		LcpKickoffEchoRequest(&PppControl);
		break;

	case PPP_SEND_ECHO_REQ:
		LcpSendEchoReq(&PppControl);
		break;
	case PPP_IPCP_GET_IP_REQ:
		PPP_GETIP(PppControl.GetIpCallbackFunc, PppControl.Cid);
		break;
	default:
		break;
	}

	PPP_LOG_EXIT();
}

// PPP main task
static void pppHandler(void)
{
	QueueMessageS qMsg;
	PPP_STATUS status;

	PPP_LOG_ENTRY(MDB_pppHandler);

	while (1) {
		PPP_LOG();
		status = PPP_RECV_Q_MSG(PppControl.MsgQRef, &qMsg, sizeof(QueueMessageS));
		if(status != PPP_SUCCESS)
		{
			PPP_PRINTF(KERN_ERR "[PPP] %s: receive msg from queue error\n", __func__);
			continue;
		}

		PPP_LOG_2(qMsg.Type, qMsg.Size);

#if defined (PPP_UNIT_TEST)
		qMsg.Type = (U_CHAR) PppControl.Event;
#endif
		if(qMsg.Type == PPP_MSG_DEINIT)
		{
			complete(&PppControl.task_exit);
			break;
		}

		pppProcessQMsg(&qMsg);
	}

	PPP_LOG_EXIT();
}

static void pppSetLcpParams(PppControlS * pppControl)
{
	PPP_LOG_ENTRY(MDB_pppSetLcpParams);

	PPP_LCP_INIT_MRU(PppControl.lcpXmitParams.Mru);
	PPP_LOG_1(PppControl.lcpXmitParams.Mru.Mru);

	PPP_LCP_INIT_ACCM(pppControl->lcpXmitParams.Accm);
	PPP_LOG_1(PppControl.lcpXmitParams.Accm.Accm);

	PPP_LCP_INIT_AUTH_PROTO(pppControl->lcpXmitParams.AuthProtocol);
	PPP_LOG_1(PppControl.lcpXmitParams.AuthProtocol.Proto);

	PPP_LCP_INIT_MAGIC_NUM(pppControl->lcpXmitParams.MagicNumber);
	PPP_LOG_1(PppControl.lcpXmitParams.MagicNumber.Number);

	PPP_LCP_SET_MRU(pppControl->lcpXmitParams, PPP_LCP_MRU_SIZE);
	PPP_LOG_1(PppControl.lcpXmitParams.Mru.Mru);

	PPP_LCP_SET_MRU(pppControl->lcpRecvParams, PPP_LCP_RECV_MRU_SIZE);
	PPP_LOG_1(PppControl.lcpRecvParams.Mru.Mru);

	PPP_LCP_SET_ACCM(pppControl->lcpXmitParams, 0);
	PPP_LOG_1(PppControl.lcpXmitParams.Accm.Accm);

	PPP_LCP_SET_AUTH_PROTO(pppControl->lcpXmitParams, PPP_PAP);
	PPP_LOG_1(PppControl.lcpXmitParams.AuthProtocol.Proto);

	PPP_LCP_SET_MAGIC_NUM(pppControl->lcpXmitParams);
	PPP_LOG_1(PppControl.lcpXmitParams.MagicNumber.Number);

	PPP_LCP_SET_PFC(pppControl->lcpXmitParams, FALSE);
	// we support this feature - but we will not enable it until configured
    PPP_SET_BIT(pppControl->lcpXmitParams.supportedParams,LCP_PFC);
	PPP_LOG_2(PppControl.lcpXmitParams.isPfcEnabled, PppControl.lcpXmitParams.supportedParams);

	PPP_LCP_SET_ACFC(pppControl->lcpXmitParams, FALSE);
	// we support this feature - but we will not enable it until configured
    PPP_SET_BIT(pppControl->lcpXmitParams.supportedParams,LCP_ACFC);
	PPP_LOG_2(PppControl.lcpXmitParams.isAcfcEnabled, PppControl.lcpXmitParams.supportedParams);

	PPP_LOG_EXIT();
}

/************************************************************************/
/* API's                                                                */
/************************************************************************/
void PppSendToCallback(PppControlS *pppControl, U_CHAR *buf, U_INT length)
{
	PPP_TRACE_BUF("PppSendToCallback (Encoded): ", buf, length);
	if (pppControl->RxCallbackFunc) {
		pppControl->RxCallbackFunc(buf, length, PppControl.Cid);
	}

}

void PppReset(void)
{
	PppControl.isConfigReqSent = FALSE;
	PppControl.IsRxFragmented = FALSE;
	PppControl.IsEscapeFound = FALSE;
	PppControl.FlagState = HDLC_FLAG_NONE;
	pppSetLcpParams(&PppControl);
	PppControl.LastXmitId = 0;
	PppControl.ipcpRecvAck = FALSE;
	PppControl.PppState = PPP_STATE_NONE;
	PppControl.isConnectIndSent = FALSE;
	PppControl.Cid = 0xFF;
	PppControl.lcpEchoReqTimeoutCount = 0;
	PppControl.ipcpXmitParams.IpAddress = 0;

	PppControl.CPOOMDrops = 0;
	PppControl.APOOMDrops = 0;

	del_timer(&PppControl.PppTimer);
	del_timer(&PppControl.LcpEchoReqTimeoutTimer);
	registerRxCallBack(PppControl.ServiceType, NULL);

}

void PppDeinit(void)
{
	PPP_LOG_ENTRY(MDB_PppDeinit);
	if (--PppInitCount == 0) {
		QueueMessageS qMsg;
		qMsg.Type = PPP_MSG_DEINIT;
		PPP_SEND_Q_MSG(PppControl.MsgQRef, &qMsg, sizeof(qMsg));
		if(wait_for_completion_timeout(&PppControl.task_exit, 60 * HZ) == 0)
		{
			PPP_PRINTF(KERN_ERR "%s: wait for task exit fail\n", __func__);
		}

		PPP_DELETE_Q_MSG(PppControl.MsgQRef);

		PPP_FREE(PppControl.OutFrameContainer.Buf);
		PPP_FREE(PppControl.OutMessageContainer.Buf);

		PPP_PRINTF("[PPP] %s: PppDeinit: Ok\n", __func__);

	}

	PPP_LOG_EXIT();
}

void PppInit(PppInitParamsS * PppInitParams)
{
	PPP_LOG_ENTRY(MDB_PppInit);
	if (PppInitCount == 0) {
		PPP_MEMSET(&PppControl, 0, sizeof(PppControlS));

		PppLogInit();
		PppControl.RxCallbackFunc = PppInitParams->RxCallbackFunc;
		PppControl.TerminateCallbackFunc = PppInitParams->TerminateCallbackFunc;
		PppControl.ConnectCallbackFunc = PppInitParams->ConnectCallbackFunc;
		PppControl.AuthenticateCallbackFunc = PppInitParams->AuthenticateCallbackFunc;
		PppControl.GetIpCallbackFunc = PppInitParams->GetIpCallbackFunc;
		PppControl.ServiceType = PppInitParams->ServiceType;
		init_completion(&PppControl.task_exit);
		PppControl.OutFrameContainer.Buf = PPP_ZALLOC(PPP_FRAME_SIZE);
		PppControl.OutMessageContainer.Buf = PPP_ZALLOC(PPP_FRAME_SIZE);

		if (PppControl.OutFrameContainer.Buf == NULL || PppControl.OutMessageContainer.Buf == NULL) {
			PPP_PRINTF(KERN_ERR "[PPP] %s: malloc error\n", __func__);
			goto err1;
		}

		//Create message queue
		PppControl.MsgQRef = PPP_CREATE_Q_MSG(PPP_QUEUE_NUM_MESSAGES, sizeof(QueueMessageS));
		if(PppControl.MsgQRef == NULL)
		{
			PPP_PRINTF(KERN_ERR "[PPP] %s: create msg queue error\n", __func__);
			goto err1;
		}

		//Create Task
		PppControl.TaskRef = PPP_CREATE_TASK(pppHandler);
		if(PppControl.TaskRef == NULL)
		{
			PPP_PRINTF(KERN_ERR "[PPP] %s: create task error\n", __func__);
			goto err2;
		}

		PppControl.isLcpEchoRequestEnabled = TRUE;

		PppReset();

		PPP_PRINTF("[PPP] %s PppInit: Ok\n", __func__);
	}
	PppInitCount++;
	PPP_LOG_EXIT();
	return;
err2:
	PPP_DELETE_Q_MSG(PppControl.MsgQRef);
err1:
	PPP_FREE(PppControl.OutFrameContainer.Buf);
	PPP_FREE(PppControl.OutMessageContainer.Buf);
}

static BOOL pppIsWriteThrough(MessageBufS * newMessage)
{
	PppFrameS *pppFrame;
	U_CHAR *p;
	U_SHORT proto;
	PppMessageS *inMessage;
	BOOL rc = FALSE;

	PPP_LOG_ENTRY(MDB_pppIsWriteThrough);

	p = newMessage->Buf;

	// PPP frame contains address and control fields
	if ((p[0] == PPP_ALLSTATIONS) && (p[1] == PPP_UI)) {
		pppFrame = (PppFrameS *) p;
		inMessage = (PppMessageS *) & pppFrame->Message;
		newMessage->Length -= 2;	// decrease header size

		PPP_LOG_1(newMessage->Length);
	}
	//no address-control. is it allowed?
	else if (PppControl.lcpRecvParams.isAcfcEnabled) {
		inMessage = (PppMessageS *) p;
		PPP_LOG();
	} else {
		PPP_LOG_EXIT();
		return rc;
	}

	GETSHORT_NOINC(proto, &inMessage->Protocol);

	PPP_LOG_1(proto);

	if (proto < 0x4000)
		rc = TRUE;

	PPP_LOG_1(rc);

	PPP_LOG_EXIT();

	return rc;

}

static void pppRelayMessageToCommDirect(MessageBufS * newMessage)
{
	U_CHAR *buf = (U_CHAR *)newMessage->Buf;

	//Adjust length to send to COMM to include 2 bytes that were "dropped" in pppDecodeFrame
	U_INT length = newMessage->Length - 1;

	PPP_LOG_ENTRY(MDB_pppRelayMessageToCommDirect);

	PPP_LOG_1(length);

	buf++;

	PPP_TRACE_BUF("Send To Comm: ", buf, length);

	if (PppControl.Cid != 0xFF)
		pppSendDataReqtoPSDChannel(PppControl.Cid, buf, length);

	PPP_LOG_EXIT();

}

static void pppResetRecvState(PppControlS * pppControl)
{
	pppControl->IsRxFragmented = FALSE;
	pppControl->IsEscapeFound = FALSE;
	pppControl->FlagState = HDLC_FLAG_FIRST;
}

void PppMessageReq(const U_CHAR *buf, U_INT length)
{
	MessageBufS *newMessage = NULL;
	BOOL isEnd = FALSE;
	U_CHAR *s, *d;		//s = running on source buffer, received from PC  d = runs on destination buffer
	QueueMessageS qMsg;
	PPP_LOG_ENTRY(MDB_PppMessageReq);
	PPP_LOG_1(length);

	PPP_TRACE_BUF("PppMessageReq: ", (U_CHAR *)buf, length);

	s = (U_CHAR *) buf;

	do {
		PPP_LOG_1(length);
		if (PppControl.IsRxFragmented) {
			PPP_LOG();
			newMessage = PppControl.InFramePlaceholder;

			if (newMessage == NULL) {
				PppControl.IsRxFragmented = FALSE;
				PppControl.IsEscapeFound = FALSE;
				PppControl.IncomingFcs = PPP_INITFCS;

				PppControl.APOOMDrops++;
				PPP_PRINTF("[PPP] %s: Fragmented Flag Error - Dropped %d AP packets\n",
					   __func__, (int)PppControl.APOOMDrops);
				PPP_LOG_EXIT();
				return;
			}

			d = &newMessage->Buf[newMessage->Length];
		} else {
			PPP_LOG();
			newMessage = PppAllocateMessage(PPP_FRAME_SIZE);
			PppControl.IsRxFragmented = FALSE;
			PppControl.IsEscapeFound = FALSE;
			PppControl.IncomingFcs = PPP_INITFCS;
			if (newMessage == NULL) {
				PppControl.APOOMDrops++;
				PPP_PRINTF("[PPP] %s: Dropped %d AP packets\n", __func__, (int)PppControl.APOOMDrops);
				PPP_LOG_EXIT();
				return;
			}

			d = newMessage->Buf;
		}

		while ((length > 0) && (!isEnd)) {
			if (*s == PPP_FLAG) {
				switch (PppControl.FlagState) {
				case HDLC_FLAG_NONE:
					PppControl.FlagState = HDLC_FLAG_FIRST;
					PPP_LOG();
					break;

				case HDLC_FLAG_FIRST:
					PPP_LOG();
					if (newMessage->Length != 0) {
						PppControl.FlagState = HDLC_FLAG_LAST;
						isEnd = TRUE;
						PPP_LOG();
					}
					break;
				default:
					break;
				}
			} else if (*s == PPP_ESCAPE) {
				PppControl.IsEscapeFound = TRUE;
			} else if (PppControl.IsEscapeFound) {
				PppControl.IsEscapeFound = FALSE;

				if (newMessage->Length < PppControl.lcpRecvParams.Mru.Mru + 3)	//protocol+FCS is also copied to buffer so add 3 bytes to MRU limit
				{
					PPP_ESCAPE_C(*d, *s);
					PPP_FCS(PppControl.IncomingFcs, *d);
					d++;
					newMessage->Length++;
				} else {
					newMessage->Length = 0xFFFF;
					break;
				}
			} else {
				if (newMessage->Length < PppControl.lcpRecvParams.Mru.Mru + 3)	//protocol+FCS is also copied to buffer so add 3 bytes to MRU limit
				{
					*d = *s;
					PPP_FCS(PppControl.IncomingFcs, *d);
					d++;
					newMessage->Length++;
				} else {
					newMessage->Length = 0xFFFF;
					break;
				}
			}
			s++;
			length--;
		}

		PPP_LOG_1(newMessage->Length);
		PPP_LOG_1(PppControl.lcpRecvParams.Mru.Mru);

		if (newMessage->Length > PppControl.lcpRecvParams.Mru.Mru + 3) {
			//Message too long - drop it, free memory and report
			PPP_PRINTF("[PPP] %s: Message too long, dropping...length=%d mru=%d\n", __func__, (int)newMessage->Length,
			       (int)PppControl.lcpRecvParams.Mru.Mru);
			PppFreeMessage(newMessage);
			newMessage = NULL;
			pppResetRecvState(&PppControl);
			isEnd = FALSE;
			while (length > 0 && *s != PPP_FLAG) {
				s++;
				length--;
			}
		}

		else if (!isEnd) {
			PPP_LOG_1(PppControl.FlagState);
			if (PppControl.FlagState == HDLC_FLAG_FIRST) {
				PPP_TRACE("IsFrarmented = TRUE");

				PppControl.IsRxFragmented = TRUE;

				PppControl.InFramePlaceholder = newMessage;

				newMessage = NULL;
			}
		} else {
			PPP_LOG_1(PppControl.IncomingFcs);

			if (PPP_GOODFCS == PppControl.IncomingFcs) {
				// Message is OK. newMessage.Length includes FCS, so decrease it by 2
				newMessage->Length -= 2;
				PPP_LOG_3((U_INT) newMessage, (U_INT) newMessage->Buf, newMessage->Length);
				PPP_TRACE_BUF("In-Decoded: ", newMessage->Buf, newMessage->Length);

				if (pppIsWriteThrough(newMessage) == TRUE) {
					if (PppControl.PppState == PPP_STATE_CONNECTED) {
						pppRelayMessageToCommDirect(newMessage);
					}
					PppFreeMessage(newMessage);
					newMessage = NULL;
				} else {
					qMsg.Type = PPP_MSG_REQ;
					qMsg.Ptr = (U_CHAR *)newMessage;
					PPP_LOG_1((U_INT)qMsg.Ptr);
					PPP_SEND_Q_MSG(PppControl.MsgQRef, &qMsg, sizeof(qMsg));
					newMessage = NULL;
				}
			} else {
				PppFreeMessage(newMessage);
				newMessage = NULL;
				PPP_PRINTF("[PPP] %s: Wrong checksum, dropping message:%x\n", __func__, PppControl.IncomingFcs);
			}

			pppResetRecvState(&PppControl);
			isEnd = FALSE;
		}

	} while (length > 0);

	if (newMessage != NULL) {
		PppFreeMessage(newMessage);
	}

	PPP_LOG_EXIT();
}

/************************************************************************/
// PppCreateMessageFrame
// 1. Takes the message in PppControl.OutMessageContainer and encodes it into
// PppControl.OutFrameContainer
// 2. Calculates FCS
/************************************************************************/
void PppCreateMessageFrame(void)
{
	U_INT msgLen, frameLen;
	U_SHORT fcs = PPP_INITFCS;
	U_CHAR *msgPtr = PppControl.OutMessageContainer.Buf;
	U_CHAR *framePtr = PppControl.OutFrameContainer.Buf;

	PPP_LOG_ENTRY(MDB_PppCreateMessageFrame);
	PPP_LOG_1(PppControl.OutMessageContainer.Length);

	PUTCHAR(PPP_FLAG, framePtr);
	frameLen = 1;

	for (msgLen = PppControl.OutMessageContainer.Length; msgLen > 0; msgLen--) {
		if ((*msgPtr < PPP_TRANS) || (*msgPtr == PPP_FLAG) || (*msgPtr == PPP_ESCAPE)) {
			frameLen++;
			PUTCHAR(PPP_ESCAPE, framePtr);
			PUTCHAR((*msgPtr) ^ PPP_TRANS, framePtr);
		} else {
			PUTCHAR(*msgPtr, framePtr);
		}
		PPP_FCS(fcs, *msgPtr);
		msgPtr++;
		frameLen++;
	}

	// Append FCS to Out Message, and update its length
	PPP_LOG_1(fcs);
	fcs ^= 0xffff;
	PPP_LOG_1(fcs);
	PUTCHAR_NOINC(fcs & 0xFF, msgPtr);
	PPP_LOG_1(*msgPtr);
	PUTCHAR_NOINC((fcs >> 8) & 0xFF, (msgPtr + 1));
	PPP_LOG_1(*(msgPtr + 1));
	PppControl.OutMessageContainer.Length += 2;

	// Add FCS to PPP encoded frame
	for (msgLen = 2; msgLen > 0; msgLen--) {
		if ((*msgPtr < PPP_TRANS) || (*msgPtr == PPP_FLAG) || (*msgPtr == PPP_ESCAPE)) {
			frameLen++;
			PUTCHAR(PPP_ESCAPE, framePtr);
			PUTCHAR((*msgPtr) ^ PPP_TRANS, framePtr);
		} else {
			PUTCHAR(*msgPtr, framePtr);
		}
		msgPtr++;
		frameLen++;

	}
	PUTCHAR(PPP_FLAG, framePtr);
	frameLen++;

	PPP_LOG_1(frameLen);

	PPP_TRACE_BUF("CreateMessageFrame: ", PppControl.OutFrameContainer.Buf, frameLen);

	PppControl.OutFrameContainer.Length = frameLen;

	PPP_LOG_EXIT();

}

void PppCreateMessageFrameIp(void)
{
	U_INT msgLen, frameLen;
	U_SHORT fcs = PPP_INITFCS;
	U_CHAR *msgPtr = PppControl.OutMessageContainer.Buf;
	U_CHAR *framePtr = PppControl.OutFrameContainer.Buf;

	PPP_LOG_ENTRY(MDB_PppCreateMessageFrame);
	PPP_LOG_1(PppControl.OutMessageContainer.Length);

	PUTCHAR(PPP_FLAG, framePtr);
	frameLen = 1;

	for (msgLen = PppControl.OutMessageContainer.Length; msgLen > 0; msgLen--) {
		if ((*msgPtr == PPP_FLAG) || (*msgPtr == PPP_ESCAPE)) {
			frameLen++;
			PUTCHAR(PPP_ESCAPE, framePtr);
			PUTCHAR((*msgPtr) ^ PPP_TRANS, framePtr);
		} else {
			PUTCHAR(*msgPtr, framePtr);
		}
		PPP_FCS(fcs, *msgPtr);
		msgPtr++;
		frameLen++;
	}

	// Append FCS to Out Message, and update its length
	PPP_LOG_1(fcs);
	fcs ^= 0xffff;
	PPP_LOG_1(fcs);
	PUTCHAR_NOINC(fcs & 0xFF, msgPtr);
	PPP_LOG_1(*msgPtr);
	PUTCHAR_NOINC((fcs >> 8) & 0xFF, (msgPtr + 1));
	PPP_LOG_1(*(msgPtr + 1));
	PppControl.OutMessageContainer.Length += 2;

	// Add FCS to PPP encoded frame
	for (msgLen = 2; msgLen > 0; msgLen--) {
		if ((*msgPtr == PPP_FLAG) || (*msgPtr == PPP_ESCAPE)) {
			frameLen++;
			PUTCHAR(PPP_ESCAPE, framePtr);
			PUTCHAR((*msgPtr) ^ PPP_TRANS, framePtr);
		} else {
			PUTCHAR(*msgPtr, framePtr);
		}
		msgPtr++;
		frameLen++;

	}
	PUTCHAR(PPP_FLAG, framePtr);
	frameLen++;

	PPP_LOG_1(frameLen);

	PPP_TRACE_BUF("CreateMessageFrame: ", PppControl.OutFrameContainer.Buf, frameLen);

	PppControl.OutFrameContainer.Length = frameLen;

	PPP_LOG_EXIT();

}

void PppUpdateIpParams(IpcpConnectionParamsS * inIpParams)
{
	QueueMessageS qMsg;

	IpcpConnectionParamsS *ipParams;
	PPP_LOG_ENTRY(MDB_PppUpdateIpParams);

	PPP_LOG_3(inIpParams->IpAddress, inIpParams->PrimaryDns, inIpParams->SecondaryDns);

	ipParams = (IpcpConnectionParamsS *) PPP_MALLOC(sizeof(IpcpConnectionParamsS));

	if (ipParams == NULL) {
		//ASSERT(FALSE);
	}

	ipParams->IpAddress = inIpParams->IpAddress;
	ipParams->PrimaryDns = inIpParams->PrimaryDns;
	ipParams->SecondaryDns = inIpParams->SecondaryDns;

	PPP_LOG_3(ipParams->IpAddress, ipParams->PrimaryDns, ipParams->SecondaryDns);

	qMsg.Type = PPP_IPCP_SEND_REQ;
	qMsg.Ptr = (U_CHAR *) ipParams;
	qMsg.Size = sizeof(IpcpConnectionParamsS);

	PPP_SEND_Q_MSG(PppControl.MsgQRef, &qMsg, sizeof(qMsg));

	PPP_LOG_EXIT();

}

void PppSendTerminateReq(void)
{
	QueueMessageS qMsg;

	qMsg.Type = PPP_SEND_TERMINATE_REQ;
	qMsg.Ptr = NULL;
	qMsg.Size = 0;

	PPP_SEND_Q_MSG(PppControl.MsgQRef, &qMsg, sizeof(qMsg));

}

void PppTerminate(void)
{
	PPP_TERMINATE(PppControl.TerminateCallbackFunc);

	PppReset();
}

void PppSetCid(U_INT cid)
{
	PppControl.Cid = cid;
}

void PppEnableEchoRequest(BOOL isEnabled)
{
	PppControl.isLcpEchoRequestEnabled = isEnabled;
}

#if defined (PPP_UNIT_TEST)
void PppSetEvent(U_INT event)
{
	PppControl.Event = event;
}
#endif
