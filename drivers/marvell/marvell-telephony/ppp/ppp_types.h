/*****************************************************************************
  * ppp.h - Network Point to Point Protocol header file.
  *
  * Copyright (c) 2003 by Marc Boucher, Services Informatiques (MBSI) inc.
  * portions Copyright (c) 1997 Global Election Systems Inc.
  *
  * The authors hereby grant permission to use, copy, modify, distribute,
  * and license this software and its documentation for any purpose, provided
  * that existing copyright notices are retained in all copies and that this
  * notice and the following disclaimer are included verbatim in any
  * distributions. No written agreement, license, or royalty fee is required
  * for any of the authorized uses.
  *
  * THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS *AS IS* AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
  * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
  * IN NO EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
  * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  * REVISION HISTORY
  *
  * 03-01-01 Marc Boucher <marc@mbsi.ca>
  *   Ported to lwIP.
  * 97-11-05 Guy Lancaster <glanca@gesn.com>, Global Election Systems Inc.
  *   Original derived from BSD codes.
  *****************************************************************************/

#ifndef _PPP_TYPES_H_
#define _PPP_TYPES_H_

#if defined __cplusplus
extern "C" {
#endif				//__cplusplus

#include "ppp_platform.h"

//extern void kdiag_print(char *fmt, ...);

#define PPP_FRAME_SIZE		0xC00

#define PPP_LCP_MRU_SIZE	0x640	//1600 bytes
#define PPP_LCP_RECV_MRU_SIZE	1500

#define LCP_MAX_OPTIONS		0x1E

#define PPP_MAX_CHAP_NAME_LENGTH        50
#define PPP_MAX_CHAP_CHALLENGE_LENGTH   50
#define PPP_MAX_CHAP_RESPONSE_LENGTH    50
#define PPP_MAX_AUTH_MESSAGE_LENGTH		200

#define CHAP_CHALLENGE_RESPONSE_TIMEOUT_IN_MSEC	6000
#define PPP_TERMINATE_TIMEOUT					5000

#define LCP_ECHO_REQ_INTERVAL_TIME				3000
#define LCP_ECHO_REQ_TIMEOUT					3000
#define LCP_ECHO_REQ_NUM_TRIES					10

/*
 * Constants and structures defined by the internet system,
 * Per RFC 790, September 1981, and numerous additions.
 */

/*
 * The basic PPP frame.
 */
#define PPP_HDRLEN      4	/* octets for standard ppp header */
#define PPP_FCSLEN      2	/* octets for FCS */

#define LCP_MRU_LEN     4
#define LCP_ACCM_LEN    6
#define LCP_AUTH_LEN    4
#define LCP_MAGIC_LEN   6
#define LCP_PFC_LEN     2
#define LCP_ACFC_LEN    2

/*
 * Significant octet values.
 */
#define PPP_ALLSTATIONS 0xff	/* All-Stations broadcast address */
#define PPP_UI          0x03	/* Unnumbered Information */
#define PPP_FLAG        0x7e	/* Flag Sequence */
#define PPP_ESCAPE      0x7d	/* Asynchronous Control Escape */
#define PPP_TRANS       0x20	/* Asynchronous transparency modifier */

/*
 * Protocol field values.
 */
#define PPP_IP          0x21	/* Internet Protocol */
#define PPP_AT          0x29	/* AppleTalk Protocol */
#define PPP_VJC_COMP    0x2d	/* VJ compressed TCP */
#define PPP_VJC_UNCOMP  0x2f	/* VJ uncompressed TCP */
#define PPP_COMP        0xfd	/* compressed packet */
#define PPP_IPCP        0x8021	/* IP Control Protocol */
#define PPP_ATCP        0x8029	/* AppleTalk Control Protocol */
#define PPP_CCP         0x80fd	/* Compression Control Protocol */
#define PPP_LCP         0xc021	/* Link Control Protocol */
#define PPP_PAP         0xc023	/* Password Authentication Protocol */
#define PPP_LQR         0xc025	/* Link Quality Report protocol */
#define PPP_CHAP        0xc223	/* Cryptographic Handshake Auth. Protocol */
#define PPP_CBCP        0xc029	/* Callback Control Protocol */
#define PPP_IPv6CP      0x8057  /*IPv6 Control Protocol*/

#define CHAP_MD5	0x05

/*
 * Values for FCS calculations.
 */
#define PPP_INITFCS     0xffff	/* Initial FCS value */
#define PPP_GOODFCS     0xf0b8	/* Good final FCS value */
#define PPP_FCS(fcs, c) fcs = (((fcs) >> 8) ^ fcstab[((fcs) ^ (c)) & 0xff])

#define PPP_ESCAPE_C(d,s)	(d)=((s)^0x20)

#define PPP_SET_BIT(b,s)	(b)|=(1<<(s))
#define PPP_CLEAR_BIT(b,s)	(b)=~(b)&(1<<(s))
#define PPP_IS_BIT_SET(b,s) ((b)&(1<<(s)))

/*
 * Inline versions of get/put char/short/long.
 * Pointer is advanced; we assume that both arguments
 * are lvalues and will already be in registers.
 * cp MUST be U_CHAR *.
 */
#define GETCHAR(c, cp) { \
    (c) = *(cp)++; \
}
#define PUTCHAR(c, cp) { \
    *(cp)++ = (U_CHAR) (c); \
}

#define GETSHORT(s, cp) {\
	(s) = *(cp); (cp)++; (s) <<= 8;\
    (s) |= *(cp); (cp)++;\
}
#define PUTSHORT(s, cp) { \
    *(cp)++ = (U_CHAR) ((s) >> 8); \
    *(cp)++ = (U_CHAR) (s & 0xff); \
}

#define GETLONG(l, cp) { \
    (l) = *(cp); (cp)++; (l) <<= 8; \
    (l) |= *(cp); (cp)++; (l) <<= 8; \
    (l) |= *(cp); (cp)++; (l) <<= 8; \
    (l) |= *(cp); (cp)++; \
}
#define PUTLONG(l, cp) { \
    *(cp)++ = (U_CHAR) ((l) >> 24); \
    *(cp)++ = (U_CHAR) ((l) >> 16); \
    *(cp)++ = (U_CHAR) ((l) >> 8); \
    *(cp)++ = (U_CHAR) (l); \
}
/*
 *	Same macros as above, but ptr is not incremented
 */

#define GETCHAR_NOINC(c, cp) { \
	U_CHAR *p = (U_CHAR *)cp; \
	(c) = *(p); \
}
#define PUTCHAR_NOINC(c, cp) { \
	U_CHAR *p = (U_CHAR *)cp; \
	*(p) = (U_CHAR) (c); \
}

#define GETSHORT_NOINC(s, cp) { \
	U_CHAR *p = (U_CHAR *)cp; \
	(s) = *(p); (p)++; (s) <<= 8; \
	(s) |= *(p);  \
}
#define PUTSHORT_NOINC(s, cp) { \
	U_CHAR *p = (U_CHAR *)cp; \
	*(p)++ = (U_CHAR) ((s) >> 8); \
	*(p) = (U_CHAR) (s & 0xff); \
}

#define GETLONG_NOINC(l, cp) { \
	U_CHAR *p = (U_CHAR *)cp; \
	(l) = *(p); (p)++; (l) <<= 8; \
	(l) |= *(p); (p)++; (l) <<= 8; \
	(l) |= *(p); (p)++; (l) <<= 8; \
	(l) |= *(p);  \
}
#define PUTLONG_NOINC(l, cp) { \
	U_CHAR *p = (U_CHAR *)cp; \
	*(p)++ = (U_CHAR) ((l) >> 24); \
	*(p)++ = (U_CHAR) ((l) >> 16); \
	*(p)++ = (U_CHAR) ((l) >> 8); \
	*(p) = (U_CHAR) (l); \
}

#define PPP_LCP_INIT_MRU(x)	{x.Header.Type = LCP_MRU;x.Header.Length=LCP_MRU_LEN;PPP_TRACE2("LcpMru.Type=%x;LcpMru.Length=%x",x.Header.Type,x.Header.Length);}
#define PPP_LCP_INIT_ACCM(x) {x.Header.Type = LCP_ACCM;x.Header.Length=LCP_ACCM_LEN;PPP_TRACE2("LcpAccm.Type=%x;LcpAccm.Length=%x",x.Header.Type,x.Header.Length);}
#define PPP_LCP_INIT_AUTH_PROTO(x) {x.Header.Type = LCP_AUTH;x.Header.Length=LCP_AUTH_LEN;PPP_TRACE2("LcpAuthProto.Type=%x;LcpAuthProto.Length=%x",x.Header.Type,x.Header.Length);}
#define PPP_LCP_INIT_MAGIC_NUM(x) {x.Header.Type = LCP_MAGIC;x.Header.Length=LCP_MAGIC_LEN;PPP_TRACE2("LcpMagic.Type=%x;LcpMagic.Length=%x",x.Header.Type,x.Header.Length);}

#define PPP_LCP_SET_MRU(m,s)\
    {\
    m.Mru.Mru=(s);\
    PPP_SET_BIT(m.supportedParams,LCP_MRU);\
    }

#define PPP_LCP_SET_ACCM(a,v)\
    {\
    a.Accm.Accm=(v);\
    PPP_SET_BIT(a.supportedParams,LCP_ACCM);\
    }

#define PPP_LCP_SET_AUTH_PROTO(a,v)\
    {\
    a.AuthProtocol.Proto=(v);\
    PPP_SET_BIT(a.supportedParams,LCP_AUTH);\
    }

#define PPP_LCP_SET_MAGIC_NUM(m)\
    {\
    U_INT	n;\
    n=PPP_RAND();\
    m.MagicNumber.Number=n;\
    PPP_SET_BIT(m.supportedParams,LCP_MAGIC);\
    }

/*
#define PPP_LCP_SET_MRU(m,s)\
	{\
		U_CHAR *p = (U_CHAR *)&(m.Mru.Mru);\
		PUTSHORT(s,p);\
		PPP_SET_BIT(m.supportedParams,LCP_MRU);\
	}

#define PPP_LCP_SET_ACCM(a,v)\
	{\
		U_CHAR *p = (U_CHAR *)&(a.Accm.Accm);\
		PUTLONG(v,p);\
		PPP_SET_BIT(a.supportedParams,LCP_ACCM);\
	}

#define PPP_LCP_SET_AUTH_PROTO(a,v)\
	{\
		U_CHAR *p = (U_CHAR *)&(a.AuthProtocol.Proto);\
		PUTSHORT(v,p);\
		PPP_SET_BIT(a.supportedParams,LCP_AUTH);\
	}

#define PPP_LCP_SET_MAGIC_NUM(m)\
	{\
		U_CHAR *p = (U_CHAR *)&(m.MagicNumber.Number);\
		U_INT	n;\
		n=PPP_RAND();\
		PUTLONG(n,p);\
		PPP_SET_BIT(m.supportedParams,LCP_MAGIC);\
	}
*/
#define PPP_LCP_SET_PFC(a,v)	{(a).isPfcEnabled=(v);PPP_SET_BIT(a.supportedParams,LCP_PFC);}
#define PPP_LCP_SET_ACFC(a,v)	{(a).isAcfcEnabled=(v);PPP_SET_BIT(a.supportedParams,LCP_ACFC);}

/*
#define PPP_APPEND(bUF,iNDEX,cHAR,fCS)\
	{ \
extern const U_SHORT fcstab[];\
PPP_FCS((fCS), (cHAR)); \
(bUF)[(iNDEX)++]=(cHAR); \
	}
*/

#define PPP_APPEND(bUF,iNDEX,cHAR,fCS)\
	{ \
	(bUF)[(iNDEX)++]=(cHAR); \
	}

#define PPP_APPEND_SHORT(bUF,iNDEX,sHORT,fCS)\
	{\
    U_SHORT t;\
    U_CHAR *p;\
    PUTSHORT_NOINC((sHORT),&t);\
    p = (U_CHAR *)&t;\
	PPP_APPEND(bUF,iNDEX,(*p)&0xFF,fCS);\
    p++;\
	PPP_APPEND(bUF,iNDEX,(*p)&0xFF,fCS);\
}

#define PPP_APPEND_LONG(bUF,iNDEX,lONG,fCS)\
	{\
    U_INT t = (lONG);\
	PPP_APPEND(bUF,iNDEX,(t>>24)&0xFF,fCS);\
	PPP_APPEND(bUF,iNDEX,(t>>16)&0xFF,fCS);\
	PPP_APPEND(bUF,iNDEX,(t>>8)&0xFF,fCS);\
	PPP_APPEND(bUF,iNDEX,t&0xFF,fCS);\
}

#define PPP_APPEND_FCS_C(bUF,iNDEX,fCS)\
	{\
	if (fCS < 0x20)\
		{\
		(bUF)[(iNDEX)++]=PPP_ESCAPE; \
		PPP_ESCAPE_C((bUF)[(iNDEX)],fCS); \
		(iNDEX)++; \
}\
		else\
		{\
		(bUF)[(iNDEX)++]=fCS; \
}\
}
/*
#define PPP_APPEND_FCS(bUF,iNDEX,fCS)\
	{\
	fCS ^= 0xffff;\
    PUTCHAR_NOINC((fCS)&0xFF,&(bUF)[(iNDEX)++]);\
    PUTCHAR_NOINC(((fCS)>>8)&0xFF,&(bUF)[(iNDEX)++]);\
}
*/
#define PPP_APPEND_FCS(bUF,iNDEX,fCS)

#define PPP_SET_NEXT_MESSAGE(mSGlIST,mSG)	\
	{\
		mSGlIST->Message	=	mSG;\
		mSGlIST	=	mSGlIST->Next;\
	}

#define PPP_GET_NEXT_MESSAGE(mSGlIST,mSG)	\
	{\
		mSG = mSGlIST->Message;\
	}

#define PPP_DEL_NEXT_MESSAGE(mSGlIST)	\
	{\
		mSGlIST	=	mSGlIST->Next;\
	}

#define PPP_IS_LIST_FULL(mSGhEAD,mSGtAIL)	(mSGtAIL->Next == mSGhEAD)

	typedef void (*PppTimeoutCallbackFunc) (unsigned long val);

	typedef enum {
		PPP_STATE_NONE,
		PPP_STATE_LCP,
		PPP_STATE_PAP,
		PPP_STATE_IPCP,
		PPP_STATE_CONNECTED
	} PppStateE;

	typedef enum _QueueMessagesE {
		PPP_MSG_REQ,
		PPP_MSG_SEND_CNF,
		PPP_MSG_SEND_REQ,
		PPP_IPCP_SEND_REQ,
		PPP_MESSAGE_FROM_COMM,
		PPP_SEND_TERMINATE_REQ,
		PPP_LCP_ECHO_REQ_TIMEOUT,
		PPP_LCP_ECHO_REQ_ACK,
		PPP_KICKOFF_ECHO_REQ,
		PPP_SEND_ECHO_REQ,
		PPP_IPCP_GET_IP_REQ,
		PPP_MSG_DEINIT,

		PPP_LAST_Q_MSG = 0xFF
	} QueueMessagesE;

	typedef enum _MessageTypeE {
		CONFIG_REQ = 1,
		CONFIG_ACK,
		CONFIG_NAK,
		CONFIG_REJ,
		TERMINATE_REQ,
		TERMINATE_ACK,
		CODE_REJECT,
		PROTO_REJECT,
		ECHO_REQUEST,
		ECHO_REPLY,
		DISCARD_REQUEST,
		IDENTIFICATION,
		TIME_REMAINING,
		LAST_MESSAGE_TYPE = 0xFF
	} MessageTypeE;

	typedef enum _LcpOptionE {
		LCP_VENDOR,
		LCP_MRU,
		LCP_ACCM,
		LCP_AUTH,
		LCP_QUALITY,
		LCP_MAGIC,
		LCP_RESERVED,
		LCP_PFC,
		LCP_ACFC,
		LCP_FCS,
		LCP_MAXSUPPORTEDOPTION,
		/*
		LCP_SDP,
		LCP_NUMMODE,
		LCP_MULTILINK,
		LCP_CALLBACK,
		LCP_CONNECTTIME,
		LCP_COMPFRAME,
		*/
		LCP_OPTEND = 0Xff
	} LcpOptionE;

	typedef enum _PapOptionsE {
		PAP_AUTHENTICATE_REQ = 1,
		PAP_AUTHENTICATE_ACK,
		PAP_AUTHENTICATE_NAK,

		PAP_OPTION_LAST = 0xFF
	} PapOptionsE;

	typedef enum _ChapOptionsE {
		CHAP_AUTHENTICATE_CHALLENGE = 1,
		CHAP_AUTHENTICATE_RESPONSE,
		CHAP_AUTHENTICATE_SUCCESS,
		CHAP_AUTHENTICATE_FAIL,

		CHAP_OPTION_LAST = 0xFF
	} ChapOptionsE;

	typedef int (*PppRxCallbackFunc) (const unsigned char *packet, int len, unsigned char cid);

	typedef enum _PppLogEntryTypesE {
		MDB_PppInit,
		MDB_PppDeinit,
		MDB_PppMessageReq,
		MDB_PppCreateMessageFrame,
		MDB_PppUpdateIpParams,
		MDB_pppHandleIncomingFrame,
		MDB_pppProcessQMsg,
		MDB_pppHandler,
		MDB_pppSetLcpParams,

		MDB_pppSendCommMessageInd,
		MDB_pppRelayMessageToComm,
		MDB_pppGetProtoFromMessage,

		MDB_pppIsWriteThrough,
		MDB_pppRelayMessageToCommDirect,

		MDB_pppCheckFcs,

		MDB_LcpHandleIncomingFrame,
		MDB_LcpSendConfigReq,
		MDB_lcpSendReject,
		MDB_lcpSendAck,
		MDB_lcpHandleConfigAck,
		MDB_lcpHandleConfigRej,
		MDB_lcpHandleConfigReq,
		MDB_lcpHandleTerminateReq,
		MDB_LcpSendTerminateReq,
		MDB_lcpHandleEchoRequest,
		MDB_LcpSendEchoReq,
		MDB_LcpKickoffEchoRequest,
		MDB_lcpHandleEchoReqAck,
		MDB_lcpHandleEchoReqTimeout,
		MDB_lcpSendNextEchoReq,
		MDB_lcpStartEchoReqTimer,

		MDB_CcpHandleIncomingFrame,

		MDB_PapHandleIncomingFrame,
		MDB_papSendAuthenticateResp,

		MDB_ChapHandleIncomingFrame,
		MDB_ChapSendAuthChallenge,

		MDB_IpcpHandleIncomingFrame,
		MDB_ipcpSendConfigReq,
		MDB_ipcpSendConfigAck,
		MDB_ipcpSendConfigReject,
		MDB_ipcpHandleConfigAck,
		MDB_ipcpHandleConfigReq,
		MDB_ipcpSendConfigNak,
		MDB_IpcpUpdateIpParams,
		MDB_pv6cpHandleIncomingFrame
	} PppLogEntryTypesE;

	typedef struct _PppHeaderS {
		U_CHAR Address;
		U_CHAR Control;
	} PppHeaderS;

	typedef struct _PppMessageS {
		U_SHORT Protocol;
		U_CHAR Data[1];
	} PppMessageS;

	typedef struct _PppFrameS {
		PppHeaderS Header;
		PppMessageS Message;
	} PppFrameS;

	typedef struct _MessageBufS {
		U_CHAR *Buf;
		U_INT Length;
	} MessageBufS;

	typedef struct _MessageBufListS {
		MessageBufS *Message;
		U_CHAR *Next;
	} MessageBufListS;

	typedef struct _LcpMessageS {
		U_CHAR Type;
		U_CHAR Id;
		U_SHORT Length;
		U_CHAR Data[1];
	} LcpMessageS, PapMessageS, ChapMessageS, PppMessageHeaderS;

	typedef struct _LcpOptionS {
		U_CHAR Type;
		U_CHAR Length;
		U_CHAR Data[1];
	} LcpOptionS, PppOptionS;

	typedef struct _LcpOptionHeaderS {
		U_CHAR Type;
		U_CHAR Length;
	} LcpOptionHeaderS;

	typedef struct _LcpMruS {
		LcpOptionHeaderS Header;
		U_SHORT Mru;
	} LcpMruS;

	typedef struct _LcpAuthProtoS {
		LcpOptionHeaderS Header;
		U_SHORT Proto;
	} LcpAuthProtoS;

	typedef struct _LcpMagicNumberS {
		LcpOptionHeaderS Header;
		U_INT Number;
	} LcpMagicNumberS;

	typedef struct _LcpAccmS {
		LcpOptionHeaderS Header;
		U_INT Accm;
	} LcpAccmS;

	typedef struct _LcpPfcS {
		LcpOptionHeaderS Header;
	} LcpPfcS;

	typedef struct _LcpAcfcS {
		LcpOptionHeaderS Header;
	} LcpAcfcS;

	typedef struct _LcpParamsS {
		LcpMruS Mru;
		LcpAuthProtoS AuthProtocol;	//Authentication Protocol
		LcpMagicNumberS MagicNumber;
		LcpAccmS Accm;

		BOOL isPfcEnabled;
		BOOL isAcfcEnabled;

		U_INT supportedParams;

	} LcpParamsS;

	typedef struct _PapAuthReqS {
		U_CHAR UserLen;
		U_CHAR *UseName;

		U_CHAR PassLen;
		U_CHAR *Password;

	} PapAuthReqS;

	typedef struct _ChapChallengeS {
		U_CHAR challengeValueLength;
		U_CHAR challengeValue[PPP_MAX_CHAP_CHALLENGE_LENGTH];
		U_CHAR challengeNameLength;
		U_CHAR challengeName[PPP_MAX_CHAP_NAME_LENGTH];
	} ChapChallengeS;

	typedef struct _ChapResponseS {
		U_CHAR responseValueLength;
		U_CHAR responseValue[PPP_MAX_CHAP_RESPONSE_LENGTH];
		U_CHAR responseNameLength;
		U_CHAR responseName[PPP_MAX_CHAP_NAME_LENGTH];
	} ChapResponseS;

	typedef struct _ChapSuccFailS {
		U_CHAR messageLength;
		U_CHAR message[PPP_MAX_AUTH_MESSAGE_LENGTH];
	} ChapSuccFailS;

/************************************************************************/
/* IPCP                                                                 */
/************************************************************************/
// Defined by RFC 1332, there are total of 10 IPCP options
// This implementation supports only 4, as listed in IpcpOptionsE
#define IPCP_NUM_AVAILABLE_OPTIONS	10
#define IPCP_DUMMY_ADDRESS			0x10000001	//10.0.0.1
#define IPCP_DUMMY_DNS              0xFFFFFF00	//255.255.255.0

	typedef enum _IpcpOptionsE {
		IPCP_OPT_IP_COMPRESSION_PROTOCOL = 2,
		IPCP_OPT_IP_ADDRESS = 3,
		IPCP_OPT_PRIMARY_DNS_ADDRESS = 0x81,
		IPCP_OPT_SECONDARY_DNS_ADDRESS = 0x83
	} IpcpOptionsE;

	typedef enum _HdlcFlagStateE {
		HDLC_FLAG_NONE,
		HDLC_FLAG_FIRST,
		HDLC_FLAG_LAST
	} HdlcFlagStateE;

// IPCP Option 0x02
	typedef struct _IpcpIpCompressionS {
		U_CHAR Type;
		U_CHAR Length;
		U_SHORT Protocol;
		U_CHAR Data[1];
	} IpcpIpCompressionS;

// IPCP Option 0x03
	typedef struct _IpcpIpAddressS {
		U_CHAR Type;
		U_CHAR Length;
		U_INT Address;
	} __attribute__ ((packed)) IpcpIpAddressS;

// IPCP Option 0x81,0x83
	typedef U_INT IpcpPrimaryDns;
	typedef U_INT IpcpSecondaryDns;

	typedef struct _IpcpConnectionParamsS {
		U_INT IpAddress;
		U_INT PrimaryDns;
		U_INT SecondaryDns;
	} IpcpConnectionParamsS;

	typedef struct _IpcpParamsS {
		IpcpIpCompressionS IpCompression;
		IpcpIpAddressS IpRemoteAddress;
		IpcpIpAddressS IpMobileAddress;
		U_INT IpAddress;
		U_INT PrimaryDns;
		U_INT SecondaryDns;
	} IpcpParamsS;

/************************************************************************/
/* Message Queue                                                        */
/************************************************************************/
	typedef struct _QueueMessageS {
		U_CHAR *Ptr;
		U_INT Size;
		U_CHAR Type;
	} QueueMessageS;

/************************************************************************/
/* PPP Main                                                             */
/************************************************************************/

	typedef struct _PppInitParamsS {
		U_INT ServiceType;
		PppRxCallbackFunc RxCallbackFunc;
		PppTerminateCallbackFunc TerminateCallbackFunc;
		PppAuthenticateCallbackFunc AuthenticateCallbackFunc;
		PppConnectCallbackFunc ConnectCallbackFunc;
		PppGetIpCallbackFunc GetIpCallbackFunc;

	} PppInitParamsS;

	typedef struct _PppControlS {
		PPP_Q_REF_TYPE MsgQRef;
		PPP_TASK_REF TaskRef;
		U_INT Event;

		U_INT	QueuedDataInPacketsCount;	//how many incoming packets are allowed in queue before start dropping
		PppStateE PppState;
		BOOL isConfigReqSent;
		BOOL isConnectIndSent;

		U_INT Cid;
		U_INT ServiceType;

		PppRxCallbackFunc RxCallbackFunc;
		PppTerminateCallbackFunc TerminateCallbackFunc;
		PppConnectCallbackFunc ConnectCallbackFunc;
		PppAuthenticateCallbackFunc AuthenticateCallbackFunc;
		PppGetIpCallbackFunc GetIpCallbackFunc;

		MessageBufListS *InFrameListHead, *InFrameListTail, *InFrameListStart;
		MessageBufListS *OutMessageListHead, *OutMessageListTail, *OutMessageListStart;
		MessageBufS *InFrameContainer;
		MessageBufS *InFramePlaceholder;
		MessageBufS OutFrameContainer;	//transmitted PPP frame, including START/END flag and escape chars
		MessageBufS OutMessageContainer;	//transmitted PPP message before encoding
		PppMessageS *InMessage;

		BOOL IsRxFragmented;
		BOOL IsEscapeFound;
		HdlcFlagStateE FlagState;
		U_SHORT IncomingFcs;

		U_CHAR LastRecvId;
		U_CHAR LastXmitId;

		LcpOptionS *lcpOptionRej;	//points to rejected option in message

		LcpParamsS lcpRecvParams;
		LcpParamsS lcpXmitParams;

		U_CHAR lcpEchoReqTimeoutCount;
		BOOL isLcpEchoRequestEnabled;

		ChapChallengeS chapChallenge;
		ChapResponseS chapResponse;
		ChapSuccFailS chapSuccFail;
		U_CHAR chapChallengeTryCount;

		PapAuthenticationParamsS PapAuthenticationParams;

		PppAuthenticationParamsS PppAuthenticationParams;

		struct timer_list PppTimer;

		struct timer_list LcpEchoReqTimeoutTimer;

		IpcpParamsS ipcpRecvParams;
		IpcpParamsS ipcpXmitParams;
		BOOL ipcpRecvAck;	// Other peer ack'ed our Config Request
		BOOL ipcpXmitAck;	// This peer sent ack to other peer's config req
		U_INT CPOOMDrops;
		U_INT APOOMDrops;
		U_INT QUEFULLDrops;
		struct completion task_exit;
	} PppControlS;

#if defined __cplusplus
}
#endif				//__cplusplus
#endif
