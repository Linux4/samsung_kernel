/*
 * pap.c -- pap module
 *
 * (C) Copyright [2006-2008] Marvell International Ltd.
 * All Rights Reserved
 *
 */
#include "ppp.h"
#include "pap.h"
#include "lcp.h"
#include "ppp_types.h"
#include "ppp_log.h"

static void papSendAuthenticateResp(PppControlS *pppControl)
{
	U_CHAR *buf = pppControl->OutMessageContainer.Buf;
	U_INT index = 0;
	U_SHORT fcs = PPP_INITFCS;

	PPP_LOG_ENTRY(MDB_papSendAuthenticateResp);
	UNUSEDPARAM(fcs);
	pppControl->PppAuthenticationParams.auth_type = PPP_PAP;
	pppControl->PppAuthenticationParams.PapAuthenticationParams = &pppControl->PapAuthenticationParams;

	if(PPP_AUTHENTICATE(pppControl->AuthenticateCallbackFunc, pppControl->Cid, &pppControl->PppAuthenticationParams) == 0)
	{
		PPP_PRINTF("[PPP] %s: authenticate fail\n", __func__);
		LcpSendTerminateReq(pppControl);
	}
	else
	{
		if (!pppControl->lcpXmitParams.isAcfcEnabled) {
			PPP_LOG();
			PPP_APPEND(buf, index, PPP_ALLSTATIONS, fcs);
			PPP_APPEND(buf, index, PPP_UI, fcs);
		}

		PPP_APPEND_SHORT(buf, index, PPP_PAP, fcs);

		PPP_APPEND(buf, index, PAP_AUTHENTICATE_ACK, fcs);
		PPP_APPEND(buf, index, pppControl->LastRecvId, fcs);

		//Returning zero length message, so total length is 6 bytes including header
		PPP_APPEND_SHORT(buf, index, 6, fcs);
		PPP_APPEND(buf, index, 0, fcs);
		PPP_APPEND(buf, index, 0, fcs);

		PPP_APPEND_FCS(buf, index, fcs);

		pppControl->OutMessageContainer.Length = index;

		PppCreateMessageFrame();

		PppSendToCallback(pppControl, pppControl->OutFrameContainer.Buf, pppControl->OutFrameContainer.Length);
	}
	PPP_LOG_EXIT();
}

static void papSaveAuthenticationParams(PppControlS *pppControl)
{
	PapMessageS *papMessage = (PapMessageS *)pppControl->InMessage->Data;
	U_INT len;
	short index, length;

	GETSHORT_NOINC(length, &papMessage->Length);

	index = 0;

	*pppControl->PapAuthenticationParams.Username = '\0';
	*pppControl->PapAuthenticationParams.Password = '\0';

	len = papMessage->Data[index++];

	if (len != 0) {
		memcpy(pppControl->PapAuthenticationParams.Username, &papMessage->Data[index], len);
		index += len;
		len++;
		pppControl->PapAuthenticationParams.Username[len] = '\0';
		len = papMessage->Data[index++];
		if (len != 0) {
			memcpy(pppControl->PapAuthenticationParams.Password, &papMessage->Data[index], len);
			len++;
			pppControl->PapAuthenticationParams.Password[len] = '\0';
		}
	}

}

//APIs
void PapHandleIncomingFrame(PppControlS *pppControl)
{
	PapMessageS *papMessage = (PapMessageS *)pppControl->InMessage->Data;

	PPP_LOG_ENTRY(MDB_PapHandleIncomingFrame);

	pppControl->LastRecvId = papMessage->Id;

	PPP_LOG_1(pppControl->LastRecvId);
	PPP_LOG_1(papMessage->Type);

	switch (papMessage->Type) {
	case PAP_AUTHENTICATE_REQ:
		PPP_LOG();
		papSaveAuthenticationParams(pppControl);
		papSendAuthenticateResp(pppControl);
		break;

	default:
		break;
	}

	PPP_LOG_EXIT();
}
