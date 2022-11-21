/*

 *(C) Copyright 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 */

#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/export.h>
#include "common_datastub.h"
#include "portqueue.h"
#include "shm_share.h"
#include "shm.h"
#include "ci_stub_ttc_macro.h"
#include "msocket.h"
#include "ci_opt_dat_types.h"
#include "data_channel_kernel.h"

#define SEARCH_CID_RETRIES 3
#define ERR_NO_HANDLER -123 //magic number that no handler found for data packages
#define CSD_CALLBACK_RETRIES 5
#define PSD_CALLBACK_RETRIES 3

static int csd_package_lose_counter = 0;
static int psd_package_lose_counter = 0;

#if 0
static int cidatastubsockfd = -1;
#endif
static int cicsdstubsockfd = -1;
DataRxCallbackFunc dataRxCbFunc[4] = { NULL, NULL, NULL, NULL };

#if 0
struct task_struct *ciDataRcvTaskRef = NULL;
#endif
struct task_struct *ciCsdRcvTaskRef = NULL;
#if 0
struct task_struct *ciDataInitTaskRef = NULL;
#endif
struct task_struct *ciCsdInitTaskRef = NULL;

extern int dataChannelInited;
extern int csdChannelInited;

extern DATAHANDLELIST *hDataList;
extern DATAHANDLELIST *hCsdataList;

extern int dataSvgHandle;

extern spinlock_t data_handle_list_lock;

#if     0

#define DPRINT(fmt, args ...)	printk(fmt, ## args)
#define DBGMSG(fmt, args ...)	printk("CIN: " fmt, ## args)
#define ENTER()			printk("CIN: ENTER %s\n", __FUNCTION__)
#define LEAVE()			printk("CIN: LEAVE %s\n", __FUNCTION__)
#define FUNC_EXIT()		printk("CIN: EXIT %s\n", __FUNCTION__)
#define ASSERT(value) do { \
	if (!(value)) { \
		printk("ASSERT FAIL AT FILE %s FUNC %s LINE %d\n", __FILE__, __func__, __LINE__); \
		while(1); \
	} \
} while (0)

#else

#define DPRINT(fmt, args ...)     printk("CI_DATA_STUB: " fmt, ## args)
#define DBGMSG(fmt, args ...)     printk(KERN_DEBUG "CI_DATA_STUB: " fmt, ## args)
#define ENTER()         do {} while (0)
#define LEAVE()         do {} while (0)
#define FUNC_EXIT()     do {} while (0)
#define ASSERT(value) do { \
	if (!(value)) { \
		printk("ASSERT FAIL AT FILE %s FUNC %s LINE %d\n", __FILE__, __func__, __LINE__); \
		while(1); \
	} \
} while (0)

#endif
void ciSendSkbToServer(int Ciport, int procId, int svcID,struct sk_buff *skb, int msglen)
{
	ShmApiMsg *pShmArgs;
	if(skb==NULL){
		printk(KERN_ERR"CI:%s:skb buff is NULL!\n",__func__);
		return;
	}
	pShmArgs = (ShmApiMsg *)skb_push(skb,sizeof(*pShmArgs));
	pShmArgs->msglen = msglen;
	pShmArgs->procId = procId;
	pShmArgs->svcId = svcID;
	msendskb(Ciport, skb, msglen+SHM_HEADER_SIZE, MSOCKET_ATOMIC);
}
#if 0
void ciDataSendMsgToServer(int procId, unsigned char *msg, int msglen)
{
	ShmApiMsg *pShmArgs;

	pShmArgs = (ShmApiMsg *) msg;
	pShmArgs->svcId = CiDataStubSvc_Id;
	pShmArgs->msglen = msglen;
	pShmArgs->procId = procId;

	msend(cidatastubsockfd, msg, msglen + SHM_HEADER_SIZE, MSOCKET_ATOMIC);
}
#endif

void ciCsdSendMsgToServer(int procId, unsigned char *msg, int msglen)
{
	ShmApiMsg *pShmArgs;

	pShmArgs = (ShmApiMsg *) msg;
	pShmArgs->svcId = CiCsdStubSvc_Id;
	pShmArgs->msglen = msglen;
	pShmArgs->procId = procId;

	msend(cicsdstubsockfd, msg, msglen + SHM_HEADER_SIZE, MSOCKET_ATOMIC);
}

int clientCiDataIndicateCallback(UINT8 * dataIndArgs)
{
	CiDatPrimRecvDataOptInd *pDataOptIndParam;
	DATAHANDLELIST *pNode, *plist;
	CiDatPdu *pRecvPdu;
	int index;
	UINT8 cid;
	int ret = 0;

	//DBGMSG("clientCiDataIndicateCallback\n");

	pDataOptIndParam = (CiDatPrimRecvDataOptInd *) (dataIndArgs);
	pRecvPdu = &pDataOptIndParam->recvPdu;
	pRecvPdu->data = dataIndArgs + sizeof(CiDatPrimRecvDataOptInd);

	spin_lock_irq(&data_handle_list_lock);

	if (pDataOptIndParam->connInfo.type == CI_DAT_CONNTYPE_CS) {
		plist = hCsdataList;
	} else if (pDataOptIndParam->connInfo.type == CI_DAT_CONNTYPE_PS) {
		plist = hDataList;
	} else {
		spin_unlock_irq(&data_handle_list_lock);
		DPRINT("clientCiDataIndicateCallback: No data list found for type %d\n", pDataOptIndParam->connInfo.type);
		return ERR_NO_HANDLER;
	}

	cid = pDataOptIndParam->connInfo.id;
	pNode = search_handlelist_by_cid(plist, cid);

	if (!pNode) {
		spin_unlock_irq(&data_handle_list_lock);
		DBGMSG("clientCiDataIndicateCallback: Not found handle node for cid %d\n", cid);
		return ERR_NO_HANDLER;
	}

	if (pDataOptIndParam->connInfo.type == CI_DAT_CONNTYPE_CS) {
		if (pNode->handle.connectionType == ATCI_REMOTE)
			index = PDP_PPP + ATCI_REMOTE;
		else if (pNode->handle.connectionType == ATCI_LOCAL)
			index = CSD_RAW;
		else {
			spin_unlock_irq(&data_handle_list_lock);
			DPRINT("clientCiDataIndicateCallback: Not match callback for CSD cid %d\n", cid);
			return -1;
		}
	} else if (pNode->handle.connectionType == ATCI_REMOTE ||
		   pNode->handle.pdptype == PDP_PPP_MODEM)
		index = PDP_PPP_MODEM;
	else
		index = pNode->handle.pdptype;

	spin_unlock_irq(&data_handle_list_lock);

	if (dataRxCbFunc[index])
	{
		ret = dataRxCbFunc[index] (pRecvPdu->data, pRecvPdu->len, cid);
		if(ret < pRecvPdu->len)
			return -1;
		else
			return ret;
	}
	else
	{
		DBGMSG("clientCiDataIndicateCallback: Not found callback for index %d\n", index);
		return -1;
	}
}

#if 0
int ciDataInitTask(void *data)
{
	ShmApiMsg datastartmsg;

	while (!kthread_should_stop()) {
		if (!dataChannelInited) {
			ciDataSendMsgToServer(CiDataStubRequestStartProcId,
					      (unsigned char *)&datastartmsg,
					      0);
			msleep_interruptible(100);	//100ms
		} else {
			break;
		}
	}

	ciDataInitTaskRef = NULL;
	DBGMSG("data channel dataChannelInited:%d!\n", dataChannelInited);
	return 0;
}
#endif

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

#if 0
static void ciDataStubEventHandler(UINT8 * rxmsg)
{
	ShmApiMsg *pData;
	int reties = 0, ret;
	
	pData = (ShmApiMsg *) rxmsg;
	ASSERT(pData->svcId == CiDataStubSvc_Id);

//      DBGMSG("ciDataStubEventHandler,procId=%d\n", pData->procId);

	switch (pData->procId) {
	case CiDatStubConfirmStartProcId:
		dataChannelInited = 1;
		break;
	case CiDataStubIndDataProcId:
		do {
			ret = clientCiDataIndicateCallback(rxmsg + SHM_HEADER_SIZE);
			
			if(ret == ERR_NO_HANDLER)
			{
				msleep_interruptible(20);	//20ms
				continue;
			}
			else if(ret <= 0) //not corecttly send to tty
			{
				psd_package_lose_counter++;
				if(psd_package_lose_counter%100 == 0)
					DPRINT("ciDataStubEventHandler,psd_package_lose_counter = %d\n", psd_package_lose_counter);
				break;
			}
			else
				break;

			}while(reties++ < PSD_CALLBACK_RETRIES);

		if(reties > 0)
			DPRINT("ciDataStubEventHandler,retries = %d\n", reties);

		break;
	case MsocketLinkdownProcId:
		DPRINT("%s: received  MsocketLinkdownProcId!\n", __FUNCTION__);
		dataChannelInited = 0;
		break;
	case MsocketLinkupProcId:
		DPRINT("%s: received  MsocketLinkupProcId!\n", __FUNCTION__);
		if (ciDataInitTaskRef) {
			kthread_stop(ciDataInitTaskRef);
			while (ciDataInitTaskRef)
				msleep_interruptible(20);
		}
		ciDataInitTaskRef =
		    kthread_run(ciDataInitTask, NULL, "ciDataInitTask");
		break;
	}

}
#endif

static void ciCsdStubEventHandler(UINT8 * rxmsg)
{
	ShmApiMsg *pData;
	int reties = 0, ret;
	
	pData = (ShmApiMsg *) rxmsg;
	ASSERT(pData->svcId == CiCsdStubSvc_Id);

	//DBGMSG("ciCsdStubEventHandler,procId=%d\n", pData->procId);

	switch (pData->procId) {
	case CiDatStubConfirmStartProcId:
		csdChannelInited = 1;
		break;
	case CiDataStubIndDataProcId:
		do {
			ret = clientCiDataIndicateCallback(rxmsg + SHM_HEADER_SIZE);
			if(ret == ERR_NO_HANDLER)
			{
				msleep_interruptible(20);	//20ms
				continue;
			}
			else if(ret <= 0) //not corecttly send to tty
			{
				csd_package_lose_counter++;
				if(csd_package_lose_counter%100 == 0)
					DPRINT("ciCsdStubEventHandler,csd_package_lose_counter = %d\n", csd_package_lose_counter);
				break;
			}
			else
				break;

			}while(reties++ < CSD_CALLBACK_RETRIES);

		if(reties > 0)
			DPRINT("ciCsdStubEventHandler,retries = %d\n", reties);
		
		break;
	case MsocketLinkdownProcId:
		DPRINT("%s: received  MsocketLinkdownProcId!\n", __FUNCTION__);
		csdChannelInited = 0;
		break;
	case MsocketLinkupProcId:
		DPRINT("%s: received  MsocketLinkupProcId!\n", __FUNCTION__);
		if (ciCsdInitTaskRef)
			kthread_stop(ciCsdInitTaskRef);
		ciCsdInitTaskRef =
		    kthread_run(ciCsdDataInitTask, NULL, "ciCsdDataInitTask");
		break;
	}

}

#if 0
int ciRcvDataTask(void *data)
{
	struct sk_buff *skb = NULL;
	allow_signal(SIGSTOP);
	while (!kthread_should_stop()) {
		if(cidatastubsockfd == -1)
		{
			DBGMSG("%s: cidatastubsockfd is closed, quit thread\n", __FUNCTION__);
			break;
		}
		skb = mrecvskb(cidatastubsockfd, MAX_CI_DATA_STUB_RXMSG_LEN, 0);
		if (!skb)
			continue;
		ciDataStubEventHandler(skb->data);
		kfree_skb(skb);
		skb = NULL;
	}
	return 0;
}
#endif

int ciCsdRcvDataTask(void *data)
{
	struct sk_buff *skb = NULL;
	allow_signal(SIGSTOP);
	while (!kthread_should_stop()) {
		if(cicsdstubsockfd == -1)
		{
			DBGMSG("%s: cicsdstubsockfd is closed, quit thread\n", __FUNCTION__);
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

void InitDataChannel(void)
{
#ifdef CS_PS_PORTS
	if (cicsdstubsockfd == -1) {
		cicsdstubsockfd = msocket(CICSDSTUB_PORT);
		DBGMSG("cicsdstubsockfd=0x%x\n", cicsdstubsockfd);
		ASSERT(cicsdstubsockfd > 0);
	}
#endif
#if 0
	if (cidatastubsockfd == -1) {
		cidatastubsockfd = msocket(CIDATASTUB_PORT);
		DBGMSG("cidatastubsockfd=0x%x\n", cidatastubsockfd);
		ASSERT(cidatastubsockfd > 0);
	}
#endif
#ifdef CS_PS_PORTS
	if (ciCsdRcvTaskRef == NULL)
		ciCsdRcvTaskRef = kthread_run(ciCsdRcvDataTask, NULL, "ciCsdRcvDataTask");
	if (ciCsdInitTaskRef == NULL)
		ciCsdInitTaskRef = kthread_run(ciCsdDataInitTask, NULL, "ciCsdDataInitTask");
#endif
#if 0
	if (ciDataRcvTaskRef == NULL)
		ciDataRcvTaskRef = kthread_run(ciRcvDataTask, NULL, "ciRcvDataTask");
	if (ciDataInitTaskRef == NULL)
		ciDataInitTaskRef = kthread_run(ciDataInitTask, NULL, "ciDataInitTask");
#endif

}

void DeInitDataChannel(void)
{
#if 0
	mclose(cidatastubsockfd);
	cidatastubsockfd = -1;
	if (ciDataRcvTaskRef) {
		send_sig(SIGSTOP, ciDataRcvTaskRef, 1);
		ciDataRcvTaskRef = NULL;
	}
	//kthread_stop(ciDataRcvTaskRef);
	if (ciDataInitTaskRef) {
		kthread_stop(ciDataInitTaskRef);
		while (ciDataInitTaskRef)
			msleep_interruptible(20);
	}
#endif
}

void DeInitCsdChannel(void)
{
	mclose(cicsdstubsockfd);
	cicsdstubsockfd = -1;
	if (ciCsdRcvTaskRef)
		send_sig(SIGSTOP, ciCsdRcvTaskRef, 1);
	//kthread_stop(ciCsdRcvTaskRef);
	if (ciCsdInitTaskRef)
		kthread_stop(ciCsdInitTaskRef);
}

/*
** return values:
** 0	TX_SUCCESS,
** 1	CI_INTERLINK_FAIL,
** 3	DATA_SIZE_EXCEED,
** 5	NO_CID,
*/
static struct sk_buff *MallocCiSkb(const char * buf, int len, int flags)
{
	struct sk_buff *skb;
	struct shm_skhdr *hdr;
	ShmApiMsg *pShm;
	CiDatPduInfo *p_ciPdu;

	bool block = flags == MSOCKET_KERNEL;

	if (block)
		skb = alloc_skb(len + sizeof(*hdr)+sizeof(*pShm)+sizeof(*p_ciPdu), GFP_KERNEL);
	else
		skb = alloc_skb(len + sizeof(*hdr)+sizeof(*pShm)+sizeof(*p_ciPdu), GFP_ATOMIC);
	if (!skb) {
		printk(KERN_ERR "Data_channel: %s: out of memory.\n", __func__);
		return NULL;
	}
	skb_reserve(skb, sizeof(*hdr)+sizeof(*pShm)+sizeof(*p_ciPdu));
	memcpy(skb_put(skb, len), buf, len);
	return skb;
}

static int sendData_ex(CiDatConnType connType, unsigned char cid,
		       const char *buf, int len, int connectionType)
{
	CiDatPduInfo *p_ciPdu;
	CiStubInfo *p_header;
	static UINT32 reqHandle = 0;
	DATAHANDLELIST *plist, *pNode;
	unsigned long flags;
	 struct sk_buff *skb;

	int ret = TX_SUCCESS;

	if (len > CI_STUB_DAT_BUFFER_SIZE - sizeof(CiDatPduInfo) -
	    SHM_HEADER_SIZE - sizeof(struct shm_skhdr)) {
		DBGMSG("sendDataReq: DATA size bigger than buffer size\n");
		return DATA_SIZE_EXCEED;
	}

	spin_lock_irqsave(&data_handle_list_lock, flags);

	if (connType == CI_DAT_CONNTYPE_PS)
		plist = hDataList;
	else			/* dataHandle.m_connType == CI_DAT_CONNTYPE_CS */
		plist = hCsdataList;

	if (!(pNode = search_handlelist_by_cid(plist, cid))) {
		DBGMSG("sendDataReq: no cid %d!\n", cid);
		spin_unlock_irqrestore(&data_handle_list_lock, flags);
		return NO_CID;
	}

	if (pNode->handle.connectionType != connectionType)
	{
		DBGMSG("sendDataReq: cid %d connection type(%d) not match!\n", cid, connectionType);
		spin_unlock_irqrestore(&data_handle_list_lock, flags);
		return NO_CID;
	}


	/*txbuf = kmalloc(SHM_HEADER_SIZE + sizeof(CiDatPduInfo) + len,
			GFP_ATOMIC);
	if (!txbuf) {
		DBGMSG("sendDataReq: Out of memory\n");
		spin_unlock_irqrestore(&data_handle_list_lock, flags);
		return DATA_SEND_ERROR;
	}*/
	skb = MallocCiSkb(buf,len,MSOCKET_ATOMIC);
	if(NULL == skb){
		printk(KERN_ERR "Data_channel: %s: out of memory.\n", __func__);
		spin_unlock_irqrestore(&data_handle_list_lock, flags);
		return MEM_ERROR;
	}

	p_ciPdu = (CiDatPduInfo *)skb_push(skb,sizeof(* p_ciPdu));
	p_header = (CiStubInfo *) p_ciPdu->aciHeader;
	p_header->info.type = CI_DATA_PDU;
	p_header->info.param1 = CI_DAT_INTERNAL_BUFFER;
	p_header->info.param2 = 0;
	p_header->info.param3 = 0;
	p_header->gHandle.svcHandle = dataSvgHandle;
	p_header->cHandle.reqHandle = reqHandle++;
	(p_ciPdu->ciHeader).connId = cid;
	(p_ciPdu->ciHeader).connType = pNode->handle.m_connType;	//(reqParam->connInfo).type;

	spin_unlock_irqrestore(&data_handle_list_lock, flags);

	if (connType == CI_DAT_CONNTYPE_CS)
		(p_ciPdu->ciHeader).datType = CI_DAT_TYPE_RAW;
	else
		(p_ciPdu->ciHeader).datType = CI_DAT_TYPE_HDLC;

	(p_ciPdu->ciHeader).isLast = TRUE;
	(p_ciPdu->ciHeader).seqNo = 0;
	(p_ciPdu->ciHeader).datLen = len;
	(p_ciPdu->ciHeader).pPayload = NULL;

#ifdef CS_PS_PORTS
	if (connType == CI_DAT_CONNTYPE_CS)
		ciSendSkbToServer(cicsdstubsockfd, CiDataStubReqDataProcId, CiCsdStubSvc_Id, skb,
				     sizeof(CiDatPduInfo) + len);
#if 0
	else
		ciSendSkbToServer(cidatastubsockfd, CiDataStubReqDataProcId, CiDataStubSvc_Id, skb,
				      sizeof(CiDatPduInfo) + len);
#endif
#else
#if 0
		ciSendSkbToServer(cidatastubsockfd, CiDataStubReqDataProcId, CiDataStubSvc_Id, skb,
				      sizeof(CiDatPduInfo) + len);
#endif
#endif

	ret = TX_SUCCESS;

	return ret;
}

int sendData(unsigned char cid, const char *buf, int len)
{
	if (dataChannelInited == 0) {
		DBGMSG("sendDataReq: CI_INTERLINK_FAIL\n");
		return CI_INTERLINK_FAIL;
	}

	return sendData_ex(CI_DAT_CONNTYPE_PS, cid, buf, len, ATCI_LOCAL);
}
EXPORT_SYMBOL(sendData);

int sendCSData(unsigned char cid, const char *buf, int len)
{
#ifdef CS_PS_PORTS
	if (csdChannelInited == 0)
#else
	if (dataChannelInited == 0)
#endif
	{
		DBGMSG("sendCSDataReq: CI_INTERLINK_FAIL\n");
		return CI_INTERLINK_FAIL;
	}

	return sendData_ex(CI_DAT_CONNTYPE_CS, cid, buf, len, ATCI_LOCAL);
}
EXPORT_SYMBOL(sendCSData);

int sendDataRemote(unsigned char cid, const char *buf, int len)
{
	if (dataChannelInited == 0) {
		DBGMSG("sendDataReq: CI_INTERLINK_FAIL\n");
		return CI_INTERLINK_FAIL;
	}

	return sendData_ex(CI_DAT_CONNTYPE_PS, cid, buf, len, ATCI_REMOTE);
}
EXPORT_SYMBOL(sendDataRemote);

int sendCSDataRemote(unsigned char cid, const char *buf, int len)
{
#ifdef CS_PS_PORTS
	if (csdChannelInited == 0)
#else
	if (dataChannelInited == 0)
#endif
	{
		DBGMSG("sendCSDataReq: CI_INTERLINK_FAIL\n");
		return CI_INTERLINK_FAIL;
	}

	return sendData_ex(CI_DAT_CONNTYPE_CS, cid, buf, len, ATCI_REMOTE);
}
EXPORT_SYMBOL(sendCSDataRemote);

int registerRxCallBack(SVCTYPE pdpTye, DataRxCallbackFunc callback)
{
	spin_lock_irq(&data_handle_list_lock);
	dataRxCbFunc[pdpTye] = callback;
	spin_unlock_irq(&data_handle_list_lock);

	return TRUE;
}
EXPORT_SYMBOL(registerRxCallBack);

int unregisterRxCallBack(SVCTYPE pdpTye)
{
	spin_lock_irq(&data_handle_list_lock);
	dataRxCbFunc[pdpTye] = NULL;
	spin_unlock_irq(&data_handle_list_lock);

	return TRUE;
}
EXPORT_SYMBOL(unregisterRxCallBack);
