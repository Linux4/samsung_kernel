/*

 *(C) Copyright 2006, 2007 Marvell DSPC Ltd. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 *
 * FILENAME:   common_datastub.h
 * PURPOSE:    data_channel struct used by stub in kernel space
 */

#ifndef __COMMON_DATASTUB_H__
#define __COMMON_DATASTUB_H__

#include "linux_driver_types.h"
#include "common_datastub_macro.h"

#if 0
typedef struct _ttyfd {
	int fd;
	BOOL occupied;
	unsigned char cid;
} TTYFD;
#endif
typedef struct directipconfig_tag {
	INT32 dwContextId;
	INT32 dwProtocol;
	struct {
		INT32 inIPAddress;
		INT32 inPrimaryDNS;
		INT32 inSecondaryDNS;
		INT32 inDefaultGateway;
		INT32 inSubnetMask;
	} ipv4;
} DIRECTIPCONFIG;
typedef struct _ipconfiglist {
	DIRECTIPCONFIG directIpAddress;
	struct _ipconfiglist *next;
} DIRECTIPCONFIGLIST;
struct ccitx_packet {
	char *pktdata;
	int length;
	unsigned char cid;
};
typedef struct _cci_tx_buf_list {
	struct ccitx_packet tx_packet;
	struct _cci_tx_buf_list *next;
} TXBUFNODE;
typedef enum {			/* com events */
	DATA_CHAN_INIT_STATE = 0,
	DATA_CHAN_NOTREADY_STATE,
	DATA_CHAN_READY_STATE,
	DATA_CHAN_SNUMSTATES
} DATACHANSTATES;

typedef enum CIDATCONNTYPE_TAG {
	CI_DAT_CONNTYPE_CS = 0,	/**< CS connection */
	CI_DAT_CONNTYPE_PS,	/**< PS connection */
	CI_DAT_CONNTYPE_IMS,	/**< IMS connection */
	CI_DAT_NUM_CONNTYPES
} _CiDatConnType;

typedef UINT8 CiDatConnType;

/* Connection Information */
/* ICAT EXPORTED STRUCTURE */
typedef struct CiDatConnInfo_struct {
	CiDatConnType type;	/* connection type */
	UINT32 id;		/* link id: call id for CS connection;
				 * context id for PS connection */
} CiDatConnInfo;
typedef enum {
	CSD_RAW,
	IMS_RAW,
	SRV_MAX
} SVCTYPE;

typedef enum ATCI_CONNECTION_TYPE {
	ATCI_LOCAL,
	ATCI_REMOTE = 2
} _AtciConnectionType;

struct datahandle_obj {
	CiDatConnInfo m_datConnInfo;
	BOOL m_fOptimizedData;
	INT16 m_maxPduSize;
	CiDatConnType m_connType;
	UINT32 m_cid;
	DATACHANSTATES chanState;
	UINT16 m_dwCurSendPduSeqNo;
	UINT16 m_dwCurSendReqHandle;
	SVCTYPE pdptype;
	UINT32 connectionType;
	/* OSAFlagRef writeSync; */
};

typedef struct _datahandle {
	struct datahandle_obj handle;
	struct _datahandle *next;
} DATAHANDLELIST;

typedef struct {
	UINT32 cid;
	UINT32 len;		/* length of databuf */
	UINT8 *databuf;
} GCFDATA;

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
struct GCFDATA32 {
	UINT32 cid;
	UINT32 len;		/* length of databuf */
	compat_uptr_t databuf;
};
#endif

typedef enum CIDATTYPE_TAG {

	CI_DAT_TYPE_PPP = 0,    /**< PPP */
	CI_DAT_TYPE_IP,         /**< IPv4 */
	CI_DAT_TYPE_IPV6,       /**< IPv6 */
	CI_DAT_TYPE_RAW,        /**< Raw data */
	CI_DAT_TYPE_HDLC,       /**< Not supported */
	CI_DAT_NUM_TYPES
} _CiDatType;

typedef UINT8 CiDatType;

/** Data PDU */
typedef struct CiDatPdu_struct {
	CiDatType type;                 /**< Data type \sa CiDatType */
	UINT16 len;                     /**< Data length */
	UINT32 data;                    /**< Pointer to data, discard to compatible 32/64bit architecture */
	CiBoolean isLast;               /**< Indicates this is the last PDU.
									  \sa CCI API Ref Manual*/
	UINT8 seqNo;                    /**< Sequence number for each CI
									  data PDU */
} CiDatPdu;

typedef struct CiDatPrimRecvDataOptInd_struct {

	CiDatConnInfo connInfo;		/**< Connection information, including
					 the PS or CS connection type and
					 context ID for the PS and CS
					 connection type, respectively \sa
					 CiDatConnInfo_struct*/
	CiDatPdu recvPdu;		/**< Received data PDU
						\sa CiDatPdu_struct */
	u32 reserved;
} CiDatPrimRecvDataOptInd;

#endif /* __COMMON_DATASTUB_H__ */
