/*

 *(C) Copyright 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 */
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include "common_datastub.h"
#include "portqueue.h"
#include "shm_share.h"
#include "shm.h"
#include "ci_stub_ttc_macro.h"
#include "msocket.h"
#include "ci_opt_dat_types.h"
#include "data_channel_kernel.h"

#define SEARCH_CID_RETRIES 3
/* magic number that no handler found for data packages */
#define ERR_NO_HANDLER -123
#define ERR_NO_CALLBACK -124
#define CSD_NO_CALLBACK_MAX_TRY_NUM 25
#define CSD_MAX_TRY_NUM 5

static int csd_package_lose_counter;

static int cicsdstubsockfd = -1;
static int ciimsstubsockfd = -1;
static DataRxCallbackFunc dataRxCbFunc[SRV_MAX];

struct task_struct *ciCsdRcvTaskRef;
struct task_struct *ciCsdInitTaskRef;
struct task_struct *ciImsRcvTaskRef;
struct task_struct *ciImsInitTaskRef;

#if     0

#define DPRINT(fmt, args ...)	pr_info(fmt, ## args)
#define DBGMSG(fmt, args ...)	pr_debug("CIN: " fmt, ## args)
#define ENTER()			pr_debug("CIN: ENTER %s\n", __func__)
#define LEAVE()			pr_debug("CIN: LEAVE %s\n", __func__)
#define FUNC_EXIT()		pr_debug("CIN: EXIT %s\n", __func__)
#else

#define DPRINT(fmt, args ...)   pr_info("CI_DATA_STUB: " fmt, ## args)
#define DBGMSG(fmt, args ...)   pr_debug("CI_DATA_STUB: " fmt, ## args)
#define ENTER()         do {} while (0)
#define LEAVE()         do {} while (0)
#define FUNC_EXIT()     do {} while (0)
#endif
void ciSendSkbToServer(int Ciport, int procId, int svcID, struct sk_buff *skb,
		       int msglen)
{
	ShmApiMsg *pShmArgs;
	if (skb == NULL) {
		pr_err("CI:%s:skb buff is NULL!\n", __func__);
		return;
	}
	pShmArgs = (ShmApiMsg *) skb_push(skb, sizeof(*pShmArgs));
	pShmArgs->msglen = msglen;
	pShmArgs->procId = procId;
	pShmArgs->svcId = svcID;
	msendskb(Ciport, skb, msglen + SHM_HEADER_SIZE, MSOCKET_ATOMIC);
}

void ciCsdSendMsgToServer(int procId, unsigned char *msg, int msglen)
{
	ShmApiMsg *pShmArgs;

	pShmArgs = (ShmApiMsg *) msg;
	pShmArgs->svcId = CiCsdStubSvc_Id;
	pShmArgs->msglen = msglen;
	pShmArgs->procId = procId;

	msend(cicsdstubsockfd, msg, msglen + SHM_HEADER_SIZE, MSOCKET_ATOMIC);
}

int clientCiDataIndicateCallback(UINT8 *dataIndArgs)
{
	CiDatPrimRecvDataOptInd *pDataOptIndParam;
	DATAHANDLELIST *pNode, *plist;
	CiDatPdu *pRecvPdu;
	UINT8 *data = NULL;
	int index;
	UINT8 cid;
	int ret = 0;

	/* DBGMSG("%s\n", __func__); */

	pDataOptIndParam = (CiDatPrimRecvDataOptInd *) (dataIndArgs);
	pRecvPdu = &pDataOptIndParam->recvPdu;
	data = dataIndArgs + sizeof(CiDatPrimRecvDataOptInd);

	spin_lock_irq(&data_handle_list_lock);

	if (pDataOptIndParam->connInfo.type == CI_DAT_CONNTYPE_CS) {
		plist = hCsdataList;
	} else {
		spin_unlock_irq(&data_handle_list_lock);
		DPRINT
		    ("%s: No data list found for type %d\n",
		     __func__, pDataOptIndParam->connInfo.type);
		return ERR_NO_HANDLER;
	}

	cid = pDataOptIndParam->connInfo.id;
	pNode = search_handlelist_by_cid(plist, cid);

	if (!pNode) {
		spin_unlock_irq(&data_handle_list_lock);
		DBGMSG
		    ("%s: Not found handle node for cid %d\n",
		     __func__, cid);
		return ERR_NO_HANDLER;
	}

	if (pNode->handle.connectionType == ATCI_LOCAL)
		index = CSD_RAW;
	else {
		spin_unlock_irq(&data_handle_list_lock);
		DPRINT
		    ("%s: Not match callback for CSD cid %d\n",
		     __func__, cid);
		return -1;
	}

	spin_unlock_irq(&data_handle_list_lock);

	if (dataRxCbFunc[index]) {
		ret = dataRxCbFunc[index] (data, pRecvPdu->len, cid);
		if (ret < pRecvPdu->len)
			return -1;
		else
			return ret;
	} else {
		pr_warn_ratelimited("%s: Not found callback for index %d\n",
			__func__, index);
		return ERR_NO_CALLBACK;
	}
}

int ciCsdDataInitTask(void *data)
{
	ShmApiMsg datastartmsg;

	while (!kthread_should_stop()) {
		if (!csdChannelInited) {
			ciCsdSendMsgToServer(CiDataStubRequestStartProcId,
					     (unsigned char *)&datastartmsg, 0);
			msleep_interruptible(3000);
		} else {
			break;
		}
	}

	ciCsdInitTaskRef = NULL;
	DBGMSG("csd channel csdChannelInited:%d!\n", csdChannelInited);
	return 0;
}

static inline int should_retry(int tried_times, int err)
{
	return (err == ERR_NO_CALLBACK &&
		tried_times < CSD_NO_CALLBACK_MAX_TRY_NUM) ||
		(err < 0 && err != ERR_NO_CALLBACK &&
		tried_times < CSD_MAX_TRY_NUM);
}

static void ciCsdStubEventHandler(UINT8 *rxmsg)
{
	ShmApiMsg *pData;
	int tried_times = 0, ret;

	pData = (ShmApiMsg *) rxmsg;
	if (pData->svcId != CiCsdStubSvc_Id) {
		pr_err("%s, svcId(%d) is incorrect, expect %d",
			__func__, pData->svcId, CiCsdStubSvc_Id);
		return;
	}

	/* DBGMSG("ciCsdStubEventHandler,procId=%d\n", pData->procId); */

	switch (pData->procId) {
	case CiDatStubConfirmStartProcId:
		csdChannelInited = 1;
		break;
	case CiDataStubIndDataProcId:
		while (1) {
			ret =
			    clientCiDataIndicateCallback(rxmsg +
							 SHM_HEADER_SIZE);
			tried_times++;
			if (!should_retry(tried_times, ret))
				break;
			msleep_interruptible(20);	/* 20ms */
		}

		if (ret <= 0) {
			/* not corecttly send to tty */
			csd_package_lose_counter++;
			pr_warn_ratelimited(
				"%s,tried_times=%d, ret=%d, drop_count=%d\n",
				__func__, tried_times, ret,
				csd_package_lose_counter);
		}
		break;
	case MsocketLinkdownProcId:
		DPRINT("%s: received  MsocketLinkdownProcId!\n", __func__);
		csdChannelInited = 0;
		break;
	case MsocketLinkupProcId:
		DPRINT("%s: received  MsocketLinkupProcId!\n", __func__);
		if (ciCsdInitTaskRef)
			kthread_stop(ciCsdInitTaskRef);
		ciCsdInitTaskRef =
		    kthread_run(ciCsdDataInitTask, NULL, "ciCsdDataInitTask");
		break;
	}

}

int ciCsdRcvDataTask(void *data)
{
	struct sk_buff *skb = NULL;
	allow_signal(SIGSTOP);
	while (!kthread_should_stop()) {
		if (cicsdstubsockfd == -1) {
			DBGMSG("%s: cicsdstubsockfd is closed, quit thread\n",
			       __func__);
			break;
		}
		skb = mrecvskb(cicsdstubsockfd, MAX_CI_DATA_STUB_RXMSG_LEN, 0);
		if (!skb)
			continue;
		ciCsdStubEventHandler(skb->data);
		kfree_skb(skb);
		skb = NULL;
	}
	return 0;
}

void InitCsdChannel(void)
{
	if (cicsdstubsockfd == -1) {
		cicsdstubsockfd = msocket(CICSDSTUB_PORT);
		DBGMSG("cicsdstubsockfd=0x%x\n", cicsdstubsockfd);
		if (cicsdstubsockfd < 0) {
			WARN_ON(1);
			return;
		}
	}
	if (ciCsdRcvTaskRef == NULL)
		ciCsdRcvTaskRef =
		    kthread_run(ciCsdRcvDataTask, NULL, "ciCsdRcvDataTask");
	if (ciCsdInitTaskRef == NULL)
		ciCsdInitTaskRef =
		    kthread_run(ciCsdDataInitTask, NULL, "ciCsdDataInitTask");
}

void DeInitCsdChannel(void)
{
	mclose(cicsdstubsockfd);
	cicsdstubsockfd = -1;
	if (ciCsdRcvTaskRef) {
		send_sig(SIGSTOP, ciCsdRcvTaskRef, 1);
		ciCsdRcvTaskRef = NULL;
	}
	/* kthread_stop(ciCsdRcvTaskRef); */
	if (ciCsdInitTaskRef)
		kthread_stop(ciCsdInitTaskRef);
	while (ciCsdInitTaskRef)
		msleep_interruptible(20);
}

void ciImsSendMsgToServer(int procId, unsigned char *msg, int msglen)
{
	ShmApiMsg *pShmArgs;

	pShmArgs = (ShmApiMsg *) msg;
	pShmArgs->svcId = CiImsStubSvc_Id;
	pShmArgs->msglen = msglen;
	pShmArgs->procId = procId;

	msend(ciimsstubsockfd, msg, msglen + SHM_HEADER_SIZE, MSOCKET_ATOMIC);
}

static void clientCiImsDataIndicateCallback(unsigned char *data, int len)
{
	int index = IMS_RAW;
	int cid = 0;

	if (dataRxCbFunc[index])
		dataRxCbFunc[index] (data, len, cid);
	else
		DBGMSG
		    ("%s: Not found callback for index %d\n",
		     __func__, index);
}

int ciImsDataInitTask(void *data)
{
	ShmApiMsg datastartmsg;

	while (!kthread_should_stop()) {
		if (!imsChannelInited) {
			ciImsSendMsgToServer(CiDataStubRequestStartProcId,
					     (unsigned char *)&datastartmsg, 0);
			msleep_interruptible(3000);
		} else {
			break;
		}
	}

	ciImsInitTaskRef = NULL;
	DPRINT("ims channel imsChannelInited:%d!\n", imsChannelInited);
	return 0;
}

static void ciImsStubEventHandler(UINT8 *rxmsg)
{
	ShmApiMsg *pData;

	pData = (ShmApiMsg *) rxmsg;
	if (pData->svcId != CiImsStubSvc_Id) {
		pr_err("%s, svcId(%d) is incorrect, expect %d",
			__func__, pData->svcId, CiImsStubSvc_Id);
		return;
	}

	DBGMSG("%s,srvId=%d, procId=%d, len=%d\n",
		__func__, pData->svcId, pData->procId, pData->msglen);

	switch (pData->procId) {
	case CiDatStubConfirmStartProcId:
		imsChannelInited = 1;
		break;
	case CiDataStubIndDataProcId:
		clientCiImsDataIndicateCallback(rxmsg + SHM_HEADER_SIZE,
				pData->msglen);
		break;
	case MsocketLinkdownProcId:
		DPRINT("%s: received  MsocketLinkdownProcId!\n", __func__);
		imsChannelInited = 0;
		break;
	case MsocketLinkupProcId:
		DPRINT("%s: received  MsocketLinkupProcId!\n", __func__);
		if (ciImsInitTaskRef)
			kthread_stop(ciImsInitTaskRef);
		ciImsInitTaskRef =
		    kthread_run(ciImsDataInitTask, NULL, "ciImsDataInitTask");
		break;
	}
}

int ciImsRcvDataTask(void *data)
{
	struct sk_buff *skb = NULL;
	allow_signal(SIGSTOP);
	while (!kthread_should_stop()) {
		if (ciimsstubsockfd == -1) {
			DBGMSG("%s: ciimsstubsockfd is closed, quit thread\n",
			       __func__);
			break;
		}
		skb = mrecvskb(ciimsstubsockfd, MAX_CI_DATA_STUB_RXMSG_LEN, 0);
		if (!skb)
			continue;
		ciImsStubEventHandler(skb->data);
		kfree_skb(skb);
		skb = NULL;
	}
	return 0;
}

void InitImsChannel(void)
{
	if (ciimsstubsockfd == -1) {
		ciimsstubsockfd = msocket(CIIMSSTUB_PORT);
		if (ciimsstubsockfd < 0) {
			WARN_ON(1);
			return;
		}
	}
	if (ciImsRcvTaskRef == NULL)
		ciImsRcvTaskRef =
		    kthread_run(ciImsRcvDataTask, NULL, "ciImsRcvDataTask");
	if (ciImsInitTaskRef == NULL)
		ciImsInitTaskRef =
		    kthread_run(ciImsDataInitTask, NULL, "ciImsDataInitTask");
}

void DeInitImsChannel(void)
{
	mclose(ciimsstubsockfd);
	ciimsstubsockfd = -1;
	if (ciImsRcvTaskRef) {
		send_sig(SIGSTOP, ciImsRcvTaskRef, 1);
		ciImsRcvTaskRef = NULL;
	}
	if (ciImsInitTaskRef)
		kthread_stop(ciImsInitTaskRef);
	while (ciImsInitTaskRef)
		msleep_interruptible(20);
}

/*
** return values:
** 0	TX_SUCCESS,
** 1	CI_INTERLINK_FAIL,
** 3	DATA_SIZE_EXCEED,
** 5	NO_CID,
*/
static struct sk_buff *MallocCiSkb(const char *buf, int len)
{
	struct sk_buff *skb;
	struct shm_skhdr *hdr;
	ShmApiMsg *pShm;
	CiDatPduInfo *p_ciPdu;

	skb = alloc_skb(len + sizeof(*hdr) + sizeof(*pShm) + sizeof(*p_ciPdu),
			GFP_ATOMIC);
	if (!skb) {
		pr_err("Data_channel: %s: out of memory.\n", __func__);
		return NULL;
	}
	skb_reserve(skb, sizeof(*hdr) + sizeof(*pShm) + sizeof(*p_ciPdu));
	memcpy(skb_put(skb, len), buf, len);
	return skb;
}

static int sendCsdData_ex(CiDatConnType connType, unsigned char cid,
			  const char *buf, int len, int connectionType)
{
	CiDatPduInfo *p_ciPdu;
	CiStubInfo *p_header;
	static UINT32 reqHandle;
	DATAHANDLELIST *plist, *pNode;
	unsigned long flags;
	struct sk_buff *skb;

	int ret = TX_SUCCESS;

	if (len > CI_STUB_DAT_BUFFER_SIZE - sizeof(CiDatPduInfo) -
	    SHM_HEADER_SIZE - sizeof(struct shm_skhdr)) {
		DBGMSG("sendCsdDataReq: DATA size bigger than buffer size\n");
		return DATA_SIZE_EXCEED;
	}

	spin_lock_irqsave(&data_handle_list_lock, flags);
	plist = hCsdataList;

	pNode = search_handlelist_by_cid(plist, cid);
	if (!pNode) {
		DBGMSG("sendCsdDataReq: no cid %d!\n", cid);
		spin_unlock_irqrestore(&data_handle_list_lock, flags);
		return NO_CID;
	}

	if (pNode->handle.connectionType != connectionType) {
		DBGMSG
		    ("sendCsdDataReq: cid %d connection type(%d) not match!\n",
		     cid, connectionType);
		spin_unlock_irqrestore(&data_handle_list_lock, flags);
		return NO_CID;
	}

	skb = MallocCiSkb(buf, len);
	if (NULL == skb) {
		pr_err("Data_channel: %s: out of memory.\n", __func__);
		spin_unlock_irqrestore(&data_handle_list_lock, flags);
		return MEM_ERROR;
	}

	p_ciPdu = (CiDatPduInfo *) skb_push(skb, sizeof(*p_ciPdu));
	p_header = (CiStubInfo *) p_ciPdu->aciHeader;
	p_header->info.type = CI_DATA_PDU;
	p_header->info.param1 = CI_DAT_INTERNAL_BUFFER;
	p_header->info.param2 = 0;
	p_header->info.param3 = 0;
	p_header->gHandle.svcHandle = dataSvgHandle;
	p_header->cHandle.reqHandle = reqHandle++;
	(p_ciPdu->ciHeader).connId = cid;
	(p_ciPdu->ciHeader).connType = pNode->handle.m_connType;

	spin_unlock_irqrestore(&data_handle_list_lock, flags);

	(p_ciPdu->ciHeader).datType = CI_DAT_TYPE_RAW;

	(p_ciPdu->ciHeader).isLast = TRUE;
	(p_ciPdu->ciHeader).seqNo = 0;
	(p_ciPdu->ciHeader).datLen = len;
	(p_ciPdu->ciHeader).pPayload = 0;

	ciSendSkbToServer(cicsdstubsockfd, CiDataStubReqDataProcId,
			  CiCsdStubSvc_Id, skb, sizeof(CiDatPduInfo) + len);

	ret = TX_SUCCESS;
	return ret;
}

static int sendImsData_ex(const char *buf, int len)
{
	struct sk_buff *skb;
	struct shm_skhdr *hdr;
	ShmApiMsg *pShm;

	if (len > IMS_DATA_STUB_WRITE_LIMIT) {
		DBGMSG("sendImsDataReq: DATA size bigger than buffer size\n");
		return DATA_SIZE_EXCEED;
	}

	skb = alloc_skb(len + sizeof(*hdr)+sizeof(*pShm), GFP_KERNEL);
	if (!skb) {
		pr_err("Data_channel: %s: out of memory.\n", __func__);
		return MEM_ERROR;
	}

	skb_reserve(skb, sizeof(*hdr) + sizeof(*pShm));
	memcpy(skb_put(skb, len), buf, len);

	pShm = (ShmApiMsg *)skb_push(skb, sizeof(*pShm));
	pShm->msglen = len;
	pShm->procId = CiDataStubReqDataProcId;
	pShm->svcId = CiImsStubSvc_Id;
	msendskb(ciimsstubsockfd, skb, len + SHM_HEADER_SIZE, MSOCKET_KERNEL);

	return TX_SUCCESS;
}

int sendCSData(unsigned char cid, const char *buf, int len)
{
	if (csdChannelInited == 0) {
		DBGMSG("sendCSDataReq: CI_INTERLINK_FAIL\n");
		return CI_INTERLINK_FAIL;
	}

	return sendCsdData_ex(CI_DAT_CONNTYPE_CS, cid, buf, len, ATCI_LOCAL);
}
EXPORT_SYMBOL(sendCSData);

int sendIMSData(unsigned char cid, const char *buf, int len)
{
	if (imsChannelInited == 0) {
		DBGMSG("sendIMSataReq: CI_INTERLINK_FAIL\n");
		return CI_INTERLINK_FAIL;
	}

	return sendImsData_ex(buf, len);
}
EXPORT_SYMBOL(sendIMSData);

int registerRxCallBack(SVCTYPE pdpTye, DataRxCallbackFunc callback)
{
	dataRxCbFunc[pdpTye] = callback;
	return TRUE;
}
EXPORT_SYMBOL(registerRxCallBack);

int unregisterRxCallBack(SVCTYPE pdpTye)
{
	dataRxCbFunc[pdpTye] = NULL;
	return TRUE;
}
EXPORT_SYMBOL(unregisterRxCallBack);

