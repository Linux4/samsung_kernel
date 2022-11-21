/*
 * ppp_platform.h -- platform specific header
 *
 * (C) Copyright [2006-2008] Marvell International Ltd.
 * All Rights Reserved
 *
 */

#ifndef _PPP_PLATFORM_H_
#define _PPP_PLATFORM_H_

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/semaphore.h>

#include "linux_driver_types.h"

//#define PPP_DEBUG
#if defined (PPP_DEBUG)
//#define PPP_DEBUG_BUFFERS
//#define PPP_DEBUG_ALL
#endif
#define UNUSEDPARAM(param) ((void)param)


#define PPP_SUCCESS 1
#define PPP_FAIL 0

#define PPP_SUSPEND 0xFFFFFFFF
#define PPP_NO_SUSPEND 0

/* Message Queue reference */
#define MSL_BUF_SIZE 1024
typedef struct {
	struct list_head head;
	long size;
	void *msg;
} PPP_Msg;

typedef struct {
	struct semaphore sema;
	struct list_head fifo;
	spinlock_t fifo_lock;
	UINT32 id;
	UINT32 maxSize;
	UINT32 maxNum;
	UINT32 msgCount;
} PPP_MsgQ_t;

//Task
#define PPP_TASK_REF		struct task_struct *
#define PPP_CREATE_TASK(fUNC)	linuxPppCreateTask(fUNC)
#define PPP_DELETE_TASK(tREF)	linuxPppDeleteTask(tREF)

//Queue
#define PPP_Q_REF_TYPE			PPP_MsgQ_t *
#define PPP_CREATE_Q_MSG(nUMmESSAGES,sIZEoFmESSAGE)		linuxPppCreateMsgQ(nUMmESSAGES,sIZEoFmESSAGE)
#define PPP_DELETE_Q_MSG(qREF)					linuxPppDeleteMsgQ(qREF)
#define PPP_RECV_Q_MSG(qREF,pTR,sIZE)				linuxPppRecvMsgQ(qREF,(U_CHAR *)pTR,sIZE, PPP_SUSPEND)
#define PPP_SEND_Q_MSG(qREF,pTR,sIZE)				linuxPppSendMsgQ(qREF,(U_CHAR *)pTR,sIZE)

//Memory
#define PPP_MALLOC(sIZE)		kmalloc(sIZE,GFP_ATOMIC)
#define PPP_ZALLOC(sIZE)		kzalloc(sIZE,GFP_ATOMIC)
#define PPP_FREE(pTR)			kfree(pTR)
#define PPP_MEMSET(pTR,vAL,sIZE)	memset(pTR,vAL,sIZE)
#define PPP_MEMCPY(dPTR,sPTR,sIZE)	memcpy(dPTR,sPTR,sIZE)

//timestamp
#define PPP_GET_TIMESTAMP()			0

//Critical Section
#define PPP_CRITICAL_SECTION_ENTER()
#define PPP_CRITICAL_SECTION_LEAVE()

#define PPP_TERMINATE(x)				linuxPppTerminate(x)
#define PPP_CONNECT(x,y)				linuxPppConnected(x,y)
#define PPP_AUTHENTICATE(a,b,c)			linuxPppAuthenticate(a,b,c)
#define PPP_GETIP(a,b)					linuxPppGetIP(a,b)

//Trace

#if !defined (PPP_DEBUG)
#define PPP_TRACE(sTRING)
#define PPP_TRACE1(sTRING,v1)
#define PPP_TRACE2(sTRING,v1,v2)
#define PPP_TRACE3(sTRING,v1,v2,v3)
#define PPP_TRACE_BUF(sTRING,p,l)
#else
#define PPP_TRACE(sTRING)			printk(sTRING)
#define PPP_TRACE1(sTRING,v1)			printk(sTRING,v1)
#define PPP_TRACE2(sTRING,v1,v2)		printk(sTRING,v1,v2)
#define PPP_TRACE3(sTRING,v1,v2,v3)		printk(sTRING,v1,v2,v3)
#if defined (PPP_DEBUG_BUFFERS)
#define PPP_TRACE_BUF(sTRING,p,l)		linuxPppTraceBuf (sTRING,p,l)
#else
#define PPP_TRACE_BUF(sTRING,p,l)
#endif
#endif

#define PPP_PRINTF		if (printk_ratelimit()) printk

//Random Number
#define PPP_RAND_INIT() linuxPppRandInit()
#define PPP_RAND() linuxPppRand()

// Types
typedef unsigned char U_CHAR;
typedef unsigned short U_SHORT;
typedef unsigned int U_INT;

typedef U_CHAR PPP_STATUS;

#if (!defined (CONFIG_USB_ACM_PPP_MODE) && !defined (CONFIG_USB_PXA955_ACM_PPP_MODE))	/* || !defined (USB_SERIAL_PPP) */
typedef struct _PapAuthenticationParamsS {
	char Username[256];
	char Password[256];
} PapAuthenticationParamsS;

typedef struct _PppAuthenticationParamsS {
	unsigned short auth_type;
	union {
		PapAuthenticationParamsS *PapAuthenticationParams;
	};
} PppAuthenticationParamsS;
#endif

typedef int (*PppAuthenticateCallbackFunc) (unsigned char cid, PppAuthenticationParamsS * auth_params);
typedef int (*PppTerminateCallbackFunc) (void);
typedef int (*PppConnectCallbackFunc) (unsigned char cid);
typedef int (*PppGetIpCallbackFunc) (unsigned char cid);


/************************************************************************/
/* Declarations                                                         */
/************************************************************************/
PPP_TASK_REF linuxPppCreateTask(void *pFunc);
void linuxPppDeleteTask(PPP_TASK_REF tRef);
PPP_Q_REF_TYPE linuxPppCreateMsgQ(U_INT, U_INT);
void linuxPppDeleteMsgQ(PPP_Q_REF_TYPE qRef);
PPP_STATUS linuxPppRecvMsgQ(PPP_Q_REF_TYPE qRef, U_CHAR * ptr, U_INT size, U_INT timeout);
PPP_STATUS linuxPppSendMsgQ(PPP_Q_REF_TYPE, U_CHAR *, U_INT);

void linuxPppRandInit(void);
U_INT linuxPppRand(void);

int linuxPppTerminate(PppTerminateCallbackFunc terminateCallbackFunc);
int linuxPppConnected(PppConnectCallbackFunc connectedCallbackFunc, unsigned char cid);
int linuxPppAuthenticate(PppAuthenticateCallbackFunc authenticateCallbackFunc, unsigned char cid,
			  PppAuthenticationParamsS * authenticationParams);
int linuxPppGetIP(PppGetIpCallbackFunc getIpCallbackFunc, unsigned char cid);
void linuxPppTraceBuf(char *string, U_CHAR * buf, U_INT length);

#endif
