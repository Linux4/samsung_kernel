/*

 *(C) Copyright 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 */
#ifndef _MODEM_IF_H_
#define _MODEM_IF_H_

#include "ppp_types.h"
#include "common_datastub.h"

typedef int (*rxCallbackFunc) (const unsigned char *packet, int len, unsigned char cid);
typedef int (*terminateCallbackFunc) (void);
typedef int (*authenticateCallbackFunc) (unsigned char cid, PppAuthenticationParamsS *auth_params);
typedef int (*connectCallbackFunc) (unsigned char cid);
typedef int (*getIPCallbackFunc) (unsigned char cid);


typedef struct _initParams {
	unsigned int ServiceType;
	rxCallbackFunc RxCallbackFunc;
	terminateCallbackFunc TerminateCallbackFunc;
	authenticateCallbackFunc AuthenticateCallbackFunc;
	connectCallbackFunc ConnectCallbackFunc;
	getIPCallbackFunc GetIPCallbackFunc;
} initParams;

typedef struct _IpConnectionParams {
	unsigned int IpAddress;
	unsigned int PrimaryDns;
	unsigned int SecondaryDns;
} ipConnectionParams;

/* PppInit prototype */
typedef void (*initFunc) (PppInitParamsS *);
/* PppDeinit prototype */
typedef void (*deInitFunc) (void);
/* PppReset prototype */
typedef void (*resetFunc) (void);
/* PppSetCid prototype */
typedef void (*setCidFunc) (unsigned int);
/* PppMessageReq prototype */
typedef void (*messageReqFunc) (const U_CHAR *, U_INT);
/* PppUpdateIpParams prototype */
typedef void (*updateIpParamsFunc) (IpcpConnectionParamsS *);

typedef struct _dataCBFuncs {
	initFunc init;
	deInitFunc deInit;
	resetFunc reset;
	setCidFunc setCid;
	messageReqFunc messageReq;
	updateIpParamsFunc updateParameters;
} dataCbFuncs;

typedef enum _atcmdResultE {
	ATCMD_RESULT_ERROR,
	ATCMD_RESULT_OK,
}atcmdResultE;

typedef struct _modem_atcmd_interface
{
	int atcmd_result;
	int ip_dns_has_update;
	DIRECTIPCONFIG ipParams;
	struct completion atcmd_response;
}modem_atcmd_interface;

struct modem_data_channel_interface {
	dataCbFuncs cbFuncs;
#define ACM_INITIALIZED 1
#define ACM_NON_INITIALIZED 0
	unsigned int initialized;
#define ACM_DATA_MODE 1
#define ACM_CONTROL_MODE 0
	unsigned int modem_state;
	unsigned char modem_curr_cid;
	modem_atcmd_interface atcmd_if;
};

void modem_register_data_service(dataCbFuncs dataCbFuncs);
void modem_unregister_data_service(void);

#endif
