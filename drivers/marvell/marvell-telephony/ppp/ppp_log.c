/*
 * ppp_log.c -- ppp log module
 *
 * (C) Copyright [2006-2008] Marvell International Ltd.
 * All Rights Reserved
 *
 */

#include <linux/kernel.h>

#include "ppp_log.h"

#if defined (PPP_DEBUG)
static char *PppLogEntryTypesStrings[] = {
	"MDB_PppInit",
	"MDB_PppDeinit",
	"MDB_PppMessageReq",
	"MDB_PppCreateMessageFrame",
	"MDB_PppUpdateIpParams",
	"MDB_pppHandleIncomingFrame",
	"MDB_pppProcessQMsg",
	"MDB_pppHandler",
	"MDB_pppSetLcpParams",

	"MDB_pppSendCommMessageInd",
	"MDB_pppRelayMessageToComm",
	"MDB_pppGetProtoFromMessage",

	"MDB_pppIsWriteThrough",
	"MDB_pppRelayMessageToCommDirect",

	"MDB_pppCheckFcs",

	"MDB_LcpHandleIncomingFrame",
	"MDB_LcpSendConfigReq",
	"MDB_lcpSendReject",
	"MDB_lcpSendAck",
	"MDB_lcpHandleConfigAck",
	"MDB_lcpHandleConfigRej",
	"MDB_lcpHandleConfigReq",
	"MDB_lcpHandleTerminateReq",
	"MDB_LcpSendTerminateReq",
	"MDB_lcpHandleEchoRequest",
	"MDB_LcpSendEchoReq",
	"MDB_LcpKickoffEchoRequest",
	"MDB_lcpHandleEchoReqAck",
	"MDB_lcpHandleEchoReqTimeout",
	"MDB_lcpSendNextEchoReq",
	"MDB_lcpStartEchoReqTimer",

	"MDB_CcpHandleIncomingFrame",

	"MDB_PapHandleIncomingFrame",
	"MDB_papSendAuthenticateResp",

	"MDB_ChapHandleIncomingFrame",
	"MDB_ChapSendAuthChallenge",

	"MDB_IpcpHandleIncomingFrame",
	"MDB_ipcpSendConfigReq",
	"MDB_ipcpSendConfigAck",
	"MDB_ipcpSendConfigReject",
	"MDB_ipcpHandleConfigAck",
	"MDB_ipcpHandleConfigReq",
	"MDB_ipcpSendConfigNak",
	"MDB_IpcpUpdateIpParams",
	"MDB_pv6cpHandleIncomingFrame"
};
#endif

static PppLogDbS PppLogDb;

void PppLogInit(void)
{
	PppLogDb.IsEnabled = (U_INT)TRUE;
	PppLogDb.LogEntry = PPP_ZALLOC(PPP_LOG_SIZE * sizeof(PppLogEntryS));
	if (PppLogDb.LogEntry == NULL) {
		PppLogDb.IsEnabled = (U_INT)FALSE;
		return;
	}

	PppLogDb.LastEntry = 0;
	PppLogDb.Size = PPP_LOG_SIZE;

}

void PppAddLogEntry(U_CHAR type, U_INT entryNum, U_CHAR numVariables, ...)
{
	va_list vars;
	int i;
#if defined (PPP_DEBUG)
	char varsString[128] = "\0";
	U_CHAR varsStringIndex = 0;
	const char hexArray[] = "0123456789ABCDEF";
#endif

	if (PppLogDb.IsEnabled) {
		PppLogEntryS *currentEntry;

		PPP_CRITICAL_SECTION_ENTER();
		currentEntry = &PppLogDb.LogEntry[PppLogDb.LastEntry];
		PppLogDb.LastEntry++;
		if (PppLogDb.LastEntry == PppLogDb.Size) {
			PppLogDb.LastEntry = 0;
		}
		PPP_CRITICAL_SECTION_LEAVE();

		currentEntry->Type = type;
		currentEntry->EntryNumber = entryNum;
		currentEntry->Timestamp = PPP_GET_TIMESTAMP();
		currentEntry->NumVars = numVariables;

		va_start(vars, numVariables);

		for (i = 0; i < numVariables; i++) {
			currentEntry->Vars[i] = va_arg(vars, U_INT);

#if defined (PPP_DEBUG)
			varsString[varsStringIndex++] = hexArray[(currentEntry->Vars[i] >> 28) & 0xF];
			varsString[varsStringIndex++] = hexArray[(currentEntry->Vars[i] >> 24) & 0xF];
			varsString[varsStringIndex++] = hexArray[(currentEntry->Vars[i] >> 20) & 0xF];
			varsString[varsStringIndex++] = hexArray[(currentEntry->Vars[i] >> 16) & 0xF];
			varsString[varsStringIndex++] = hexArray[(currentEntry->Vars[i] >> 12) & 0xF];
			varsString[varsStringIndex++] = hexArray[(currentEntry->Vars[i] >> 8) & 0xF];
			varsString[varsStringIndex++] = hexArray[(currentEntry->Vars[i] >> 4) & 0xF];
			varsString[varsStringIndex++] = hexArray[(currentEntry->Vars[i]) & 0xF];
			varsString[varsStringIndex++] = ' ';
#endif
		}
#if defined (PPP_DEBUG)
		varsString[varsStringIndex] = '\0';
#endif
		va_end(vars);
	}
#if defined (PPP_DEBUG)
	printk("\n\r[PPP] %s(L:%d) - %s\n\r", PppLogEntryTypesStrings[type], entryNum, varsString);
#endif
}
