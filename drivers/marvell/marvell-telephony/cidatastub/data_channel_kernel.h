/*

 *(C) Copyright 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 */

#ifndef _DATA_CHANNEL_KERNEL_H_
#define _DATA_CHANNEL_KERNEL_H_

#include <linux/list.h>

typedef enum {
	TX_SUCCESS,
	CI_INTERLINK_FAIL = -1,
	DATA_CHAN_NOT_READY = -2,
	DATA_SIZE_EXCEED = -3,
	NO_RX_BUFS = -4,
	NO_CID = -5,
	PREVIOUS_SEND_ERROR = -6,
	DATA_SEND_ERROR = -7,
	MEM_ERROR = -8
} TXSTATUS;
#define txDataMask 0x0001
#define MAX_CI_DATA_STUB_RXMSG_LEN 2048
#define CI_STUB_DAT_BUFFER_SIZE 0x800

typedef int (*DataRxCallbackFunc) (char *packet, int len, unsigned char cid);

extern void InitDataChannel(void);
extern void DeInitDataChannel(void);
extern void DeInitCsdChannel(void);
extern int sendData(unsigned char cid, const char *buf, int len);
extern int sendCSData(unsigned char cid, const char *buf, int len);
extern int sendDataRemote(unsigned char cid, const char *buf, int len);
extern int sendCSDataRemote(unsigned char cid, const char *buf, int len);
extern void sendPSDData(int cid, struct sk_buff *skb);
extern int registerRxCallBack(SVCTYPE pdpTye, DataRxCallbackFunc callback);
extern int unregisterRxCallBack(SVCTYPE pdpTye);
DATAHANDLELIST *search_handlelist_by_cid(DATAHANDLELIST * pHeader,
					 unsigned char cid);

#endif
