#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/skbuff.h>
#include "diag.h"
#include "msocket.h"
#include "shm_share.h"
#include "portqueue.h"
#include "shm.h"

int diagsockfd = -1;
struct task_struct *diagRcvTaskRef = NULL;
struct task_struct *diagInitTaskRef = NULL;
BOOL diagChannelInited = FALSE;
BOOL diagDiscAcked = FALSE;
BOOL diagConnAcked = FALSE;
BOOL diagHostUsbConn = FALSE;
extern int gRcvdCpPkt;
extern int gRcvdCpLongPkt;
extern int gUsbSentCpPkt;
extern int gUsbSentCpPktError;

static DiagRxCallbackFunc diagRxCbFunc = NULL;

int diagInitTask(void *data);

//static struct proc_dir_entry *diag_proc;

#if     1
#define DPRINT(fmt, args ...)     printk("DIAG: " fmt, ## args)
#define DBGMSG(fmt, args ...)     printk("DIAG: " fmt, ## args)
#define ENTER()                 printk("DIAG: ENTER %s\n", __FUNCTION__)
#define LEAVE()                 printk("DIAG: LEAVE %s\n", __FUNCTION__)
#define FUNC_EXIT()                     printk("DIAG: EXIT %s\n", __FUNCTION__)
#define ASSERT(a)  if (!(a)) { \
		while (1) { \
			printk("ASSERT FAIL AT FILE %s FUNC %s LINE %d\n", __FILE__, __FUNCTION__, __LINE__); \
		} \
}

#else
#define DPRINT(fmt, args ...)     printk("DIAG: " fmt, ## args)
#define DBGMSG(fmt, args ...)     do {} while (0)
#define ENTER()         do {} while (0)
#define LEAVE()         do {} while (0)
#define FUNC_EXIT()     do {} while (0)
#define ASSERT(a)       if (!(a)) { \
		while (1) { \
			printk("ASSERT FAIL AT FILE %s FUNC %s LINE %d\n", __FILE__, __FUNCTION__, __LINE__); \
		} \
}
#endif
static struct sk_buff *MallocDiagSkb(const char * buf, int len, int flags)
{
	struct sk_buff *skb;
	struct shm_skhdr *hdr;
	DIAGHDR* pDiag;
	bool block = flags == MSOCKET_KERNEL;
	if (block)
		skb = alloc_skb(len + sizeof(*hdr)+sizeof(*pDiag), GFP_KERNEL);
	else
		skb = alloc_skb(len + sizeof(*hdr)+sizeof(*pDiag), GFP_ATOMIC);
	if (!skb) {
		printk(KERN_ERR "Data_channel: %s: out of memory.\n", __func__);
		return NULL;
	}
	skb_reserve(skb, sizeof(*hdr)+sizeof(*pDiag));
	memcpy(skb_put(skb, len), buf, len);
	return skb;
}
void diagTxRawSkb(struct sk_buff * skb, int msglen)
{
	ASSERT(skb != NULL);
	msendskb(diagsockfd, skb, msglen, MSOCKET_ATOMIC);
}
void diagSendUsbData(unsigned char*msg, int msglen)
{
	DIAGHDR *pHeader;
	struct sk_buff * skb;

	skb = MallocDiagSkb(msg,msglen,MSOCKET_ATOMIC);
	if(NULL == skb){
		printk(KERN_ERR "Diag_channel: %s: out of memory.\n", __func__);
		return;
	}
	pHeader = (DIAGHDR*)skb_push(skb,sizeof(*pHeader));
	pHeader->packetlen = msglen;
	pHeader->seqNo = 0;
	pHeader->msgType = 0;
	diagTxRawSkb(skb, msglen + sizeof(DIAGHDR));
}
void diagTxRawData(unsigned char *msg, int msglen)
{
	int ret;

	ASSERT(msg != NULL);

	//DBGMSG("ciDataSendMsgToServer, procId=%d\n",procId);

	ret = msend(diagsockfd, msg, msglen, MSOCKET_ATOMIC);
//	DBGMSG("diagTxRawData: ret=%d\n",ret);
//     ASSERT(ret >= 0);

}

static void diagDataEventHandler (UINT8*  rxmsg, int actlen)
{
	UINT32  *pData;
	DIAGHDR *pHeader;
	UINT32 ret;
	static UINT16 expSeqNo = 0;
	static UINT8 *pTempBuf;
	static UINT8 *pWalk;
	static UINT16 expPktLen;
	static UINT16 expPktNo;
	static BOOL isPktRcving = FALSE;
	static BOOL isPktDiscarding = FALSE;

	pHeader = (DIAGHDR*)rxmsg;
	pData = (UINT32*)(rxmsg + sizeof(DIAGHDR));

	switch (*pData)
	{
	case DiagDataConfirmStartMsg:
		diagChannelInited = TRUE;
		break;
	case DiagExtIfConnectedAckMsg:
		diagConnAcked = TRUE;
		break;
	case DiagExtIfDisconnectedAckMsg:
		diagDiscAcked = TRUE;
		break;
	case MsocketLinkdownProcId:
		DPRINT("%s: received MsocketLinkdownProcId!\n", __FUNCTION__);
		diagChannelInited = FALSE;
		diagConnAcked = FALSE;
		diagDiscAcked = FALSE;
		break;
	case MsocketLinkupProcId:
		DPRINT("%s: received MsocketLinkupProcId!\n", __FUNCTION__);
		if (diagInitTaskRef)
			kthread_stop(diagInitTaskRef);
		diagInitTaskRef = kthread_run(diagInitTask, NULL, "diagInitTask");
		break;
	default:
		//	DBGMSG("diagDataEventHandler,pktlen=%d, seqNo=%d\n",pHeader->packetlen, pHeader->seqNo);
		if ((pHeader->packetlen <= SinglePktMaxLen) && (pHeader->seqNo == 0))
		{
			gRcvdCpPkt++;
			/* reset isPktDiscarding */
			isPktDiscarding = FALSE;

			if (diagRxCbFunc)
			{
				ret = diagRxCbFunc(rxmsg + sizeof(DIAGHDR), pHeader->packetlen);
				if (ret != pHeader->packetlen)
				{
					gUsbSentCpPktError++;
				}
				else
				{
					gUsbSentCpPkt++;
				}
			}
			else
				DPRINT("usb rx call back not registered\n");

			break;
		}
		if (pHeader->packetlen > SinglePktMaxLen)
		{
			gRcvdCpLongPkt++;
			//		DBGMSG("long packet received, len=%d\n",pHeader->packetlen);

			if (isPktDiscarding)
			{
				/* Check whether it's a new large packet using the seqNo == 0. */
				/* if yes, reset the isPktDiscarding and go on the frame processing, */
				/* Otherwise, just ignore this frmae.  */
				if (pHeader->seqNo == 0)
					isPktDiscarding = FALSE;
				else
					break;
			}

			if (isPktRcving)
			{
				if ((expPktLen != pHeader->packetlen) || (expSeqNo != pHeader->seqNo))
				{
					DPRINT("invalid packet len, rx pdu dropped.");
					kfree(pTempBuf);
					pWalk = NULL;
					pTempBuf = NULL;
					expSeqNo = 0;
					expPktLen = 0;
					expPktNo = 0;
					isPktRcving = FALSE;
					break;
				}
				memcpy(pWalk, pData, actlen - 8);
				pWalk += actlen - 8;
				expSeqNo++;

				if (expPktNo == expSeqNo)
				{
					if (diagRxCbFunc)
					{
						ret = diagRxCbFunc(pTempBuf, expPktLen);
						if (ret != pHeader->packetlen)
						{
							gUsbSentCpPktError++;
						}
						else
						{
							gUsbSentCpPkt++;
						}
					}
					kfree(pTempBuf);
					pWalk = NULL;
					pTempBuf = NULL;
					expSeqNo = 0;
					expPktLen = 0;
					expPktNo = 0;
					isPktRcving = FALSE;
					break;
				}

			}
			else if (pHeader->packetlen > SinglePktMaxLen)
			{
				/* first frame in a large packet. */
				pTempBuf = kmalloc(pHeader->packetlen, GFP_KERNEL);
				if (!pTempBuf) {
					/* Buffer allocation failed for this multiple-frame large packet. */
					/* rest frames in this large packet should be discarded. */
					gUsbSentCpPktError++;
					isPktDiscarding = TRUE;
					break;
				}
				isPktRcving = TRUE;
				expPktLen = pHeader->packetlen;
				expPktNo = (pHeader->packetlen % SinglePktMaxLen == 0) ?
					   pHeader->packetlen / SinglePktMaxLen : pHeader->packetlen / SinglePktMaxLen + 1;
				pWalk = pTempBuf;
				memcpy(pWalk, pData, actlen - 8);
				pWalk += actlen - 8;
				expSeqNo++;
			}
			break;
		}
	}

}

int diagRcvDataTask(void *data)
{
	struct sk_buff *skb = NULL;

#ifndef DIAG_USB_TEST
	DBGMSG("diagRcvDataTask start...\n");
	allow_signal(SIGSTOP);
	while (!kthread_should_stop())
	{
		if(diagsockfd == -1)
		{
			DBGMSG("%s: diagsockfd is closed, quit thread\n", __func__);
			break;
		}
		skb = mrecvskb(diagsockfd, MAX_DIAG_DATA_RXMSG_LEN, 0);
		if (!skb)
			continue;
//		ASSERT(ret > 0);
		diagDataEventHandler(skb->data, skb->len);
		kfree_skb(skb);
		skb = NULL;
	}
#endif
	return 0;
}

int diagInitTask(void *data)
{
	UINT32 startId = DiagDataRequestStartMsg;
	UINT8 startmsg[sizeof(DIAGHDR) + sizeof(UINT32)];
	DIAGHDR *pHeader;

	pHeader = (DIAGHDR *)startmsg;
	pHeader->packetlen = sizeof(UINT32);
	pHeader->seqNo = 0;
	pHeader->msgType = startId;
	*(UINT32 *)(startmsg+sizeof(DIAGHDR)) = startId;
	DBGMSG("diagInitTask start...\n");
	while (!kthread_should_stop())
	{
		if (!diagChannelInited)
		{
			diagTxRawData(startmsg, sizeof(DIAGHDR) + sizeof(UINT32));
			msleep_interruptible(100);
		}
		else
		{
			DBGMSG("diag channel inited:%d!\n", diagChannelInited);

			/* If the host USB is connected, we should inform CP about it */
			if (diagHostUsbConn)
				diagUsbConnectNotify();

			break;
		}
	}

	diagInitTaskRef = NULL;
	return 0;


}

void InitUSBChannel(void)
{
	if (diagInitTaskRef == NULL)
		diagInitTaskRef = kthread_run(diagInitTask, NULL, "diagInitTask");
}

void InitDiagChannel(void)
{
	if (diagsockfd == -1) {
		diagsockfd = msocket(DIAG_PORT);
		DBGMSG("diagsockfd=0x%x\n", diagsockfd);
		ASSERT(diagsockfd > 0);
	}
	if (diagRcvTaskRef == NULL)
		diagRcvTaskRef = kthread_run(diagRcvDataTask, NULL, "diagRcvDataTask");
}
void DeInitDiagChannel(void)
{
	if (diagsockfd > 0)
		mclose(diagsockfd);
	diagsockfd = -1;

	if (diagRcvTaskRef) {
		send_sig(SIGSTOP, diagRcvTaskRef, 1);
		//kthread_stop(diagRcvTaskRef);
		diagRcvTaskRef = NULL;
	}
}

int diagRegisterRxCallBack(DiagRxCallbackFunc callback)
{
	diagRxCbFunc = callback;
	/* comment it since msocket is not opened at this point */
	/* diagUsbConnectNotify(); */
	return TRUE;
}
int diagUnregisterRxCallBack(void)
{
	diagRxCbFunc = NULL;
	/*diagUsbDisConnectNotify();*/
	return TRUE;
}
