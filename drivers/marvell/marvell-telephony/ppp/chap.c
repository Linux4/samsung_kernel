/*
 * chap.c - chap functions
 *
 * (C) Copyright [2006-2008] Marvell International Ltd.
 * All Rights Reserved
 *
 */

#include "ppp.h"
#include "chap.h"
#include "lcp.h"
#include "ppp_log.h"

//static const char *authAckMsg = "Marvell PPP - Password Verified OK";
//static const char *authNakMsg = "Marvell PPP - Password Incorrect";

static const char authChalName[PPP_MAX_CHAP_NAME_LENGTH] = "Marvell PPP";

#if defined (PPP_USE_FIXED_CHAP_CHALLENGE)
static char fixedChallengeValue[] = { 'F', 'I', 'X', 'E', 'D' };
#endif

static void chapHandleChallengeTimoeut(unsigned long val)
{
	PppControlS *pppControl = (PppControlS *)val;

	PPP_PRINTF("[PPP] %s: chapHandleChallengeTimoeut: timeout occurred\n", __func__);

	pppControl->chapChallengeTryCount++;

	if (pppControl->chapChallengeTryCount < CHAP_MAX_CHALLENGE_TRIES) {
		ChapSendAuthChallenge(pppControl);
	} else {
		del_timer(&pppControl->PppTimer);
		LcpSendTerminateReq(pppControl);
	}

}

static void chapStartTimer(PppControlS *pppControl)
{
	init_timer(&pppControl->PppTimer);

	pppControl->PppTimer.expires = jiffies + msecs_to_jiffies(CHAP_CHALLENGE_RESPONSE_TIMEOUT_IN_MSEC);
	pppControl->PppTimer.function = chapHandleChallengeTimoeut;
	pppControl->PppTimer.data = (unsigned long)pppControl;

	add_timer(&pppControl->PppTimer);

}

static void chapSendAuthSuccessOrFail(PppControlS *pppControl, U_CHAR success_or_fail_code)
{
	const char *message[] = { "Authentication Successful", "Authentication Failed" };
	char *pMessage = (char *)message[success_or_fail_code - CHAP_AUTHENTICATE_SUCCESS];
	U_CHAR *buf = pppControl->OutMessageContainer.Buf;
	U_INT index = 0;
	U_INT lenIndex;
	U_SHORT len = PPP_HDRLEN;
	U_SHORT fcs = PPP_INITFCS;
	UNUSEDPARAM(fcs);

	if (!pppControl->lcpXmitParams.isAcfcEnabled) {
		PPP_APPEND(buf, index, PPP_ALLSTATIONS, fcs);
		PPP_APPEND(buf, index, PPP_UI, fcs);
	}

	PPP_APPEND_SHORT(buf, index, PPP_CHAP, fcs);

	PPP_APPEND(buf, index, success_or_fail_code, fcs);
	PPP_APPEND(buf, index, pppControl->LastXmitId++, fcs);

	lenIndex = index;

	index += 2;

	while (*pMessage != '\0') {
		PPP_APPEND(buf, index, *pMessage, fcs);
		pMessage++;
		len++;
	}

	PPP_APPEND_SHORT(buf, lenIndex, len, fcs);

	PPP_APPEND_FCS(buf, index, fcs);

	pppControl->OutMessageContainer.Length = index;

	PppCreateMessageFrame();

	PppSendToCallback(pppControl, pppControl->OutFrameContainer.Buf, pppControl->OutFrameContainer.Length);
}

static void chapCreateNewChallange(PppControlS *pppControl)
{
	U_INT i;

#if defined (PPP_USE_FIXED_CHAP_CHALLENGE)
	for (i = 0; i < sizeof(fixedChallengeValue); i++) {
		pppControl->chapChallenge.challengeValue[i] = fixedChallengeValue[i];
	}
	pppControl->chapChallenge.challengeValueLength = sizeof(fixedChallengeValue);

#else
	PPP_PRINTF(KERN_ERR "[PPP] %s: Variable challenge length is currently not supported", __func__);
#endif

	for (i = 0; i < sizeof(authChalName); i++) {
		pppControl->chapChallenge.challengeName[i] = authChalName[i];
	}
	pppControl->chapChallenge.challengeNameLength = sizeof(authChalName);

}

void ChapSendAuthChallenge(PppControlS *pppControl)
{
	U_CHAR *buf = pppControl->OutMessageContainer.Buf;
	U_INT index = 0;
	U_INT lenIndex;
	U_SHORT len = PPP_HDRLEN;
	U_SHORT fcs = PPP_INITFCS;
	U_CHAR i;

	PPP_LOG_ENTRY(MDB_ChapSendAuthChallenge);
	UNUSEDPARAM(fcs);

	if (!pppControl->lcpXmitParams.isAcfcEnabled) {
		PPP_LOG();
		PPP_APPEND(buf, index, PPP_ALLSTATIONS, fcs);
		PPP_APPEND(buf, index, PPP_UI, fcs);
	}

	PPP_APPEND_SHORT(buf, index, PPP_CHAP, fcs);

	PPP_APPEND(buf, index, CHAP_AUTHENTICATE_CHALLENGE, fcs);
	PPP_LOG_1(pppControl->LastXmitId);
	PPP_APPEND(buf, index, pppControl->LastXmitId++, fcs);

	chapCreateNewChallange(pppControl);

	lenIndex = index;

	index += 2;

	PPP_APPEND(buf, index, pppControl->chapChallenge.challengeValueLength, fcs);

	for (i = 0; i < pppControl->chapChallenge.challengeValueLength; i++, len++) {
		PPP_APPEND(buf, index, pppControl->chapChallenge.challengeValue[i], fcs);
	}

	for (i = 0; i < pppControl->chapChallenge.challengeNameLength; i++, len++) {
		PPP_APPEND(buf, index, pppControl->chapChallenge.challengeName[i], fcs);
	}
	PPP_APPEND(buf, index, '\0', fcs);
	len++;

	// Put length in right place
	PPP_APPEND_SHORT(buf, lenIndex, len, fcs);

	PPP_APPEND_FCS(buf, index, fcs);

	PPP_LOG_1(fcs);

	pppControl->OutMessageContainer.Length = index;
	PPP_LOG_1(index);

	PppCreateMessageFrame();

	//Start timer ....
	chapStartTimer(pppControl);

	PppSendToCallback(pppControl, pppControl->OutFrameContainer.Buf, pppControl->OutFrameContainer.Length);

	PPP_LOG_EXIT();

}

static void chapHandleAuthenticateResponse(PppControlS *pppControl)
{
	ChapMessageS *chapMessage = (ChapMessageS *)pppControl->InMessage->Data;
	ChapResponseS *chapResponse = (ChapResponseS *)chapMessage->Data;
	U_CHAR i;

	del_timer(&pppControl->PppTimer);

	if (chapResponse->responseValueLength > PPP_MAX_CHAP_RESPONSE_LENGTH) {
		PPP_PRINTF("[PPP] %s: chapHandleAuthenticateResponse: responseValueLength greater than %d\n",
		       __func__, PPP_MAX_CHAP_RESPONSE_LENGTH);

		del_timer(&pppControl->PppTimer);

		LcpSendTerminateReq(pppControl);
	} else {
		pppControl->chapResponse.responseValueLength = chapResponse->responseValueLength;
		PPP_PRINTF("[PPP] %s: chapHandleAuthenticateResponse: responseValueLength = %d\n", __func__, chapResponse->responseValueLength);
		for (i = 0; i < chapResponse->responseValueLength; i++) {
			pppControl->chapResponse.responseValue[i] = chapResponse->responseValue[i];
		}

		chapSendAuthSuccessOrFail(pppControl, CHAP_AUTHENTICATE_SUCCESS);
	}

}

/************************************************************************/
/* APIs                                                                 */
/************************************************************************/
void ChapHandleIncomingFrame(PppControlS *pppControl)
{
	ChapMessageS *chapMessage = (ChapMessageS *)pppControl->InMessage->Data;
	PPP_LOG_ENTRY(MDB_ChapHandleIncomingFrame);

	pppControl->LastRecvId = chapMessage->Id;

	PPP_LOG_1(pppControl->LastRecvId);
	PPP_LOG_1(chapMessage->Type);

	switch (chapMessage->Type) {
	case CHAP_AUTHENTICATE_RESPONSE:
		chapHandleAuthenticateResponse(pppControl);
		break;

	default:
		PPP_PRINTF("[PPP] %s: ChapHandleIncomingFrame: received ChapMessageS->Type = %d\n", __func__, chapMessage->Type);
		break;

	}

	PPP_LOG_EXIT();
}
