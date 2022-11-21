/*
 * ppp_platform.c -- platform specific code
 *
 * (C) Copyright [2006-2008] Marvell International Ltd.
 * All Rights Reserved
 *
 */

#include <linux/kernel.h>
#include <linux/random.h>
#include <linux/kthread.h>

#include "ppp_platform.h"
#include "ppp_types.h"

#define PPP_TASK_NAME		"PppTask"

int linuxPppTerminate(PppTerminateCallbackFunc terminateCallbackFunc)
{
	int ret = 0;
	if (terminateCallbackFunc) {
		ret = terminateCallbackFunc();
	}
	return ret;
}

int linuxPppConnected(PppConnectCallbackFunc connectedCallbackFunc, unsigned char cid)
{
	int ret = 0;
	if (connectedCallbackFunc) {
		ret = connectedCallbackFunc(cid);
	}
	return ret;
}

int linuxPppAuthenticate(PppAuthenticateCallbackFunc authenticateCallbackFunc, unsigned char cid,
			  PppAuthenticationParamsS * authenticationParams)
{
	int ret = 0;
	if (authenticateCallbackFunc) {
		ret = authenticateCallbackFunc(cid, authenticationParams);
	}
	return ret;
}

int linuxPppGetIP(PppGetIpCallbackFunc getIpCallbackFunc, unsigned char cid)
{
	int ret = 0;
	if (getIpCallbackFunc) {
		ret = getIpCallbackFunc(cid);
	}
	return ret;
}


void linuxPppDeleteTask(PPP_TASK_REF tRef)
{
	if(tRef)
		kthread_stop(tRef);
}

PPP_TASK_REF linuxPppCreateTask(void *funcPtr)
{
	PPP_TASK_REF pppTaskRef = NULL;

	pppTaskRef = kthread_run(funcPtr, NULL, PPP_TASK_NAME);

	return pppTaskRef;

}

void linuxPppDeleteMsgQ(PPP_Q_REF_TYPE qRef)
{
	QueueMessageS qMsg;
	PPP_STATUS status;
	MessageBufS *qMsgbuf;
	/*delete message in the queue*/
	while((status = linuxPppRecvMsgQ(qRef, (U_CHAR *)&qMsg, sizeof(QueueMessageS), PPP_NO_SUSPEND)) == PPP_SUCCESS)
	{
		qMsgbuf = (MessageBufS *)qMsg.Ptr;
		PPP_FREE(qMsgbuf->Buf);
		PPP_FREE(qMsgbuf);
	}
	PPP_PRINTF("[PPP] %s: delete message queue\n", __func__);
	PPP_FREE(qRef);
	qRef = NULL;
	return;
}

PPP_Q_REF_TYPE linuxPppCreateMsgQ(U_INT numMessages, U_INT sizeOfMessage)
{
	PPP_Q_REF_TYPE qRef = NULL;

	qRef = PPP_MALLOC(sizeof(*qRef));
	if(qRef == NULL)
		return NULL;
	INIT_LIST_HEAD(&qRef->fifo);
	qRef->maxNum = numMessages;
	qRef->maxSize = sizeOfMessage;
	qRef->msgCount = 0;
	sema_init(&qRef->sema, 0);
	spin_lock_init(&qRef->fifo_lock);
	return qRef;
}

PPP_STATUS linuxPppRecvMsgQ(PPP_Q_REF_TYPE qRef, U_CHAR *ptr, U_INT size, U_INT timeout)
{
	int firstMsg = 0;
	PPP_Msg *head;
	unsigned long flags;

	if(qRef == NULL || ptr == NULL || ((timeout != PPP_SUSPEND) && (timeout != PPP_NO_SUSPEND)))
	{
		PPP_PRINTF(KERN_ERR "[PPP] %s: invalid input parameters\n", __func__);
		return PPP_FAIL;
	}
	do {
		head = NULL;

		spin_lock_irqsave(&qRef->fifo_lock, flags);
		/* Atomic block */
		if (qRef->msgCount > 0) {
			head = (PPP_Msg *) qRef->fifo.next;
			list_del(&head->head);
			qRef->msgCount--;
		}
		/* Atomic block end */
		spin_unlock_irqrestore(&qRef->fifo_lock, flags);

		if (head) {
			/* Got a message */
			size = (head->size > size) ? size : head->size;
			PPP_MEMCPY(ptr, head->msg, size);
			PPP_FREE(head->msg);
			PPP_FREE(head);
			if (!firstMsg) {
				/* Actually, this means FIRST msg. Otherwise we
				   already down'ed the sema when exiting wait for it
				   in PPP_SUSPEND mode */
				down(&qRef->sema);
			}
			break;
		} else {
			if (timeout == PPP_SUSPEND) {
				down(&qRef->sema);
				firstMsg = 1;
			} else if (timeout == PPP_NO_SUSPEND) {
				return PPP_FAIL;
			}
		}

	} while (timeout == PPP_SUSPEND);

	return PPP_SUCCESS;
}

PPP_STATUS linuxPppSendMsgQ(PPP_Q_REF_TYPE qRef, U_CHAR * ptr, U_INT size)
{
	PPP_Msg *msgBuf;
	unsigned long flags;

	if(qRef == NULL || ptr == NULL)
	{
		PPP_PRINTF(KERN_ERR "[PPP] %s: invalid input parameters\n", __func__);
		return PPP_FAIL;
	}
	msgBuf = (PPP_Msg *)PPP_MALLOC(sizeof(PPP_Msg));
	if(msgBuf == NULL)
	{
		PPP_PRINTF(KERN_ERR "[PPP] %s: malloc error\n", __func__);
		return PPP_FAIL;
	}
	msgBuf->msg = (char *)PPP_MALLOC(size);
	if(msgBuf->msg == NULL)
	{
		PPP_FREE(msgBuf);
		PPP_PRINTF(KERN_ERR "[PPP] %s: malloc error\n", __func__);
		return PPP_FAIL;
	}
	msgBuf->size = (long)size;
	PPP_MEMCPY(msgBuf->msg, (char *)ptr, size);

	spin_lock_irqsave(&qRef->fifo_lock, flags);
	/* Atomic block */
	list_add_tail(&msgBuf->head, &qRef->fifo);

	qRef->msgCount++;
	spin_unlock_irqrestore(&qRef->fifo_lock, flags);
	up(&qRef->sema);
	return (PPP_SUCCESS);
}

/************************************************************************/
/* Random Number Generator                                              */
/************************************************************************/
void linuxPppRandInit(void)
{
}

U_INT linuxPppRand(void)
{
	return (U_INT)random32();
}

/************************************************************************/
/* Debug Functions                                                      */
/************************************************************************/
void linuxPppTraceBuf(char *string, U_CHAR * buf, U_INT length)
{
	U_INT i;

	printk("\n\r[PPP] %s - ", string);

	for (i = 0; length > 0; length--, i++) {
		printk("%02X ", buf[i]);
	}

	printk("\n\r");
}
