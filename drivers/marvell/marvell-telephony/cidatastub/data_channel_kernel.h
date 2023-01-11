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
#include <linux/skbuff.h>

#include "shm_share.h"

enum TXSTATUS {
	TX_SUCCESS,
	CI_INTERLINK_FAIL = -1,
	DATA_CHAN_NOT_READY = -2,
	DATA_SIZE_EXCEED = -3,
	NO_RX_BUFS = -4,
	NO_CID = -5,
	PREVIOUS_SEND_ERROR = -6,
	DATA_SEND_ERROR = -7,
	MEM_ERROR = -8
};
#define txDataMask 0x0001
#define MAX_CI_DATA_STUB_RXMSG_LEN 2048
#define CI_STUB_DAT_BUFFER_SIZE 0x800

#define SHM_SK_HEADER_SIZE 16 /*sizeof(struct shm_skhdr)*/
#define IMS_DATA_STUB_WRITE_LIMIT (CI_STUB_DAT_BUFFER_SIZE - \
		SHM_HEADER_SIZE - SHM_SK_HEADER_SIZE)

extern int csdChannelInited;
extern DATAHANDLELIST *hCsdataList;
extern int dataSvgHandle;
extern spinlock_t data_handle_list_lock;

extern int imsChannelInited;
extern int gCcinetDataEnabled;

typedef int (*DataRxCallbackFunc) (char *packet, int len, unsigned char cid);

extern void InitCsdChannel(void);
extern void DeInitCsdChannel(void);
extern int InitPsdChannel(void);
extern void DeInitPsdChannel(void);
extern void InitImsChannel(void);
extern void DeInitImsChannel(void);
extern int sendCSData(unsigned char cid, const char *buf, int len);
extern int sendIMSData(unsigned char cid, const char *buf, int len);
extern int registerRxCallBack(SVCTYPE pdpTye, DataRxCallbackFunc callback);
extern int unregisterRxCallBack(SVCTYPE pdpTye);
extern DATAHANDLELIST *search_handlelist_by_cid(DATAHANDLELIST *pHeader,
					 unsigned char cid);

#endif
