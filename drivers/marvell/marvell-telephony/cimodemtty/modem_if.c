/*

 *(C) Copyright 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 */
/*******************************************************************
*
*  FILE:	 modem_if.c
*
*  DESCRIPTION: interface between PPP and USB gadget modem/TTY
*
*
*******************************************************************/
#include <linux/ioctl.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/inet.h>
#include "modem_if.h"

/* Defines - must be the same as in pxa910_f_modem.c */
#define TIOPPPON        _IOW('T', 208, int)
#define TIOPPPOFF       _IOW('T', 209, int)

#define INVALID_CID 0xFF
#define TIMEOUT_CGACT_DEACT (57 * HZ)
#define TIMEOUT_CGDATA (157 * HZ)
#define TIMEOUT_CGDCONT (10 * HZ)
#define TIMEOUT_AUTHReq (10 * HZ)

/*******************************************\
  Static Functions
\*******************************************/
static void modem_init_data_service(void);
static void modem_send_from_tty(char *packet, unsigned int size);
static void modem_rx(unsigned char cid, char *packet, int size);
static int modem_ioctl(void *tty, unsigned int cmd, unsigned long arg);

static struct modem_data_channel_interface modem_if;

/*******************************************\
  externs from tty
\*******************************************/

typedef void (*marvell_modem_rx_callback) (unsigned char cid, char *packet, int len);

typedef int (*marvell_modem_ioctl_notify_callback) (void *tty, unsigned int cmd, unsigned long arg);

extern int gs_marvell_modem_send(const unsigned char *buf, int count, unsigned char cid);
extern int gs_marvell_modem_send_to_atcmdsrv(char *buf, int count);

extern marvell_modem_rx_callback gs_marvell_modem_rx_psd_callback;
extern marvell_modem_ioctl_notify_callback gs_marvell_modem_ioctl_notify_callback;

void modem_register_data_service(dataCbFuncs DataCbFuncs)
{
	modem_if.cbFuncs = DataCbFuncs;
	modem_init_data_service();
}

void modem_unregister_data_service(void)
{
	dataCbFuncs DataCbFuncs = {NULL, NULL, NULL, NULL, NULL, NULL};
	gs_marvell_modem_rx_psd_callback = NULL;
	gs_marvell_modem_ioctl_notify_callback = NULL;
	if (modem_if.cbFuncs.deInit) {
		modem_if.cbFuncs.deInit();
	}
	modem_if.cbFuncs = DataCbFuncs;
}

static void modem_send_from_tty(char *packet, unsigned int size)
{
	gs_marvell_modem_send_to_atcmdsrv(packet, size);
}

static int modem_data_tx_callback(const unsigned char *buf, int count, unsigned char cid)
{
	return gs_marvell_modem_send(buf, count, cid);
}

int modem_set_modem_control_mode(void)
{
	char deact_pdp[20];
	int buf_len = 0;
	int ret = ATCMD_RESULT_ERROR;

	if (modem_if.modem_curr_cid != INVALID_CID) {
		/* simulate at command to deactivate pdp context as if received from PC */
		buf_len = snprintf(deact_pdp, sizeof(deact_pdp), "AT+CGACT=0,%d\x0d", modem_if.modem_curr_cid + 1);
		modem_if.atcmd_if.atcmd_result = ATCMD_RESULT_ERROR;
		modem_send_from_tty(deact_pdp, buf_len);
		if(wait_for_completion_timeout(&modem_if.atcmd_if.atcmd_response, TIMEOUT_CGACT_DEACT) == 0)
		{
			printk(KERN_ERR "%s: ppp deactive fail\n", __func__);
			return ret;
		}
		ret = modem_if.atcmd_if.atcmd_result;
	}
	modem_if.modem_state = ACM_CONTROL_MODE;
	return ret;
}

int modem_ppp_connect(unsigned char cid)
{
	char connect_str[30];
	int buf_len = 0;
	int ret = ATCMD_RESULT_ERROR;
	if (cid != INVALID_CID) {
		buf_len = snprintf(connect_str, sizeof(connect_str), "AT+CGDATA=\"NULL\",%d\x0d", cid + 1);
		modem_if.atcmd_if.atcmd_result = ATCMD_RESULT_ERROR;
		modem_send_from_tty(connect_str, buf_len);
		if(wait_for_completion_timeout(&modem_if.atcmd_if.atcmd_response, TIMEOUT_CGDATA) == 0)
		{
			printk(KERN_ERR "%s: ppp connect fail\n", __func__);
			return ret;
		}
		ret = modem_if.atcmd_if.atcmd_result;
	}
	return ret;
}
int modem_ppp_get_ip(unsigned char cid)
{
	char getIP_str[30];
	int buf_len = 0;
	int ret = ATCMD_RESULT_ERROR;
	if (cid != INVALID_CID) {
		buf_len = snprintf(getIP_str, sizeof(getIP_str), "AT+CGDCONT?\x0d");
		modem_if.atcmd_if.atcmd_result = ATCMD_RESULT_ERROR;
		modem_send_from_tty(getIP_str, buf_len);
		if(wait_for_completion_timeout(&modem_if.atcmd_if.atcmd_response, TIMEOUT_CGDCONT) == 0)
		{
			printk(KERN_ERR "%s: ppp get ip fail\n", __func__);
			return ret;
		}
		ret = modem_if.atcmd_if.atcmd_result;
	}
	return ret;
}

int modem_authenticate(unsigned char cid, PppAuthenticationParamsS * auth_params)
{
	char auth_str[256];
	int buf_len = 0;
	int ret = ATCMD_RESULT_ERROR;

	if (auth_params->auth_type == PPP_PAP) {
		if (cid != INVALID_CID) {
			buf_len = snprintf(auth_str, sizeof(auth_str), "AT*AUTHReq=%d,%d,%s,%s\x0d", cid + 1, 1,
				 auth_params->PapAuthenticationParams->Username,
				 auth_params->PapAuthenticationParams->Password);
			modem_if.atcmd_if.atcmd_result = ATCMD_RESULT_ERROR;
			modem_send_from_tty(auth_str, buf_len);
			if(wait_for_completion_timeout(&modem_if.atcmd_if.atcmd_response, TIMEOUT_AUTHReq) == 0)
			{
				printk(KERN_ERR "%s: ppp authenticate fail\n", __func__);
				return ret;
			}
			ret = modem_if.atcmd_if.atcmd_result;
		}
	} else if (auth_params->auth_type == PPP_CHAP) {
		/* Currently not supported */
		printk(KERN_ERR "%s: ppp authenticate protocol CHAP currently not supported\n", __func__);
		return ret;
	}
	return ret;
}

static void modem_init_data_service(void)
{
	PppInitParamsS InitParam = {
		.ServiceType = PDP_PPP_MODEM,
		.RxCallbackFunc = modem_data_tx_callback,
		.TerminateCallbackFunc = modem_set_modem_control_mode,
		.AuthenticateCallbackFunc = modem_authenticate,
		.ConnectCallbackFunc = modem_ppp_connect,
		.GetIpCallbackFunc = modem_ppp_get_ip,
	};
	if (modem_if.cbFuncs.init) {
		modem_if.cbFuncs.init(&InitParam);
		modem_if.modem_state = ACM_CONTROL_MODE;
		modem_if.initialized = ACM_INITIALIZED;
		modem_if.modem_curr_cid = INVALID_CID;
		init_completion(&modem_if.atcmd_if.atcmd_response);
	}

	gs_marvell_modem_rx_psd_callback = modem_rx;
	gs_marvell_modem_ioctl_notify_callback = modem_ioctl;
}

static int modem_ioctl(void *tty, unsigned int cmd, unsigned long arg)
{
	/* Unused Params */
	(void)tty;

	if (modem_if.initialized != ACM_INITIALIZED) {
		printk("%s: modem_if is not initialized\n", __func__);
		return -EIO;
	}

	switch (cmd) {
	case TIOPPPON:
		printk("%s: TIOPPON: cid=%ld\n", __func__, arg);
		modem_if.cbFuncs.reset();
		modem_if.modem_curr_cid = (unsigned char)arg;
		modem_if.cbFuncs.setCid(modem_if.modem_curr_cid);
		modem_if.modem_state = ACM_DATA_MODE;
		gs_marvell_modem_send("\r\nCONNECT\r\n", sizeof("\r\nCONNECT\r\n") - 1, modem_if.modem_curr_cid);
		return 0;
	case TIOPPPOFF:
		printk("%s: TIOPPPOFF: cid=%ld\n", __func__, arg);
		modem_if.modem_curr_cid = INVALID_CID;
		modem_if.cbFuncs.setCid(modem_if.modem_curr_cid);
		modem_if.modem_state = ACM_CONTROL_MODE;
		modem_if.cbFuncs.reset();
		return 0;
	default:
		/* could not handle ioctl */
		printk("%s: unsupported cmd: %x\n", __func__, cmd);
		return -ENOIOCTLCMD;
	}
}
static void getDNS(char *dns, int *priDNS, int *secDNS)
{
	unsigned char *buf, *buf_save, *packetEnd, *ipcpEnd;
	int len = strlen(dns);
	int buf_len = len / 2;
	char primaryDNS[INET6_ADDRSTRLEN] = { '\0' };
	char secondaryDNS[INET6_ADDRSTRLEN] = { '\0' };
	int i = 0;
	unsigned int tmp;
	char tmpbuf[3];
	buf = kmalloc(buf_len, GFP_ATOMIC);
	if(buf == NULL)
	{
		printk("%s: malloc error\n", __func__);
		return;
	}
	buf_save = buf;
	while (i < len)
	{
		memcpy(tmpbuf, &dns[i], 2);
		tmpbuf[2] = '\0';
		sscanf(tmpbuf, "%x", &tmp);
		buf[i / 2] = tmp;
		i += 2;
	}
	packetEnd = buf + buf_len;
	while (buf < packetEnd)
	{

		unsigned short type = buf[0] << 8 | buf[1];
		unsigned char len = buf[2]; //  this length field includes only what follows it
		switch (type)
		{
		case 0x8021:
			// IPCP option for IP configuration - we are looking for DNS parameters.
			// it seem that this option may appear more than once!
			ipcpEnd = buf + len + 3;
			buf += 3;
			// Ido : I guess that this must be a Nak because of the conf-request structure ???
			if (*buf == 0x03)
			{
				// Config-Nak found, advance to IPCP data start
				buf += 4;
				// parse any configuration
				while (buf < ipcpEnd)
				{
					if (*buf == 129)
					{
						// Primary DNS address
						buf += 2;
						sprintf(primaryDNS, "%d.%d.%d.%d", buf[0], buf[1], buf[2], buf[3]);
						buf += 4;
						continue;
					}
					if (*buf == 131)
					{
						// Secondary DNS address
						buf += 2;
						sprintf(secondaryDNS, "%d.%d.%d.%d", buf[0], buf[1], buf[2], buf[3]);
						buf += 4;
						continue;
					}
					// buf[0] includes the ipcp type, buf[1] the length of this field includes the whole TLV
					buf += buf[1];
				}
			}
			else
			{
				buf += len;
			}
			break;
		default:
			buf += len + 3;
			break;
		}
	}
	if ((primaryDNS[0] != '\0') && strcmp(primaryDNS, "0.0.0.0"))
	{
		in4_pton(primaryDNS, strlen(primaryDNS), (u8 *)priDNS, '\0', NULL);
	}
	if ((secondaryDNS[0] != '\0') && strcmp(secondaryDNS, "0.0.0.0"))
	{
		in4_pton(secondaryDNS, strlen(secondaryDNS), (u8 *)secDNS, '\0', NULL);
	}
	kfree(buf_save);
}

static void modem_parse_atcmd( char *packet, int size)
{
	if(strncasecmp(packet, "\r\nOK", strlen("\r\nOK")) == 0)
	{
		modem_if.atcmd_if.atcmd_result = ATCMD_RESULT_OK;
		if(modem_if.atcmd_if.ip_dns_has_update)
		{
			IpcpConnectionParamsS dataIpParams;
			dataIpParams.IpAddress = modem_if.atcmd_if.ipParams.ipv4.inIPAddress;
			dataIpParams.PrimaryDns = modem_if.atcmd_if.ipParams.ipv4.inPrimaryDNS;
			dataIpParams.SecondaryDns = modem_if.atcmd_if.ipParams.ipv4.inSecondaryDNS;
			modem_if.cbFuncs.updateParameters(&dataIpParams);
			modem_if.atcmd_if.ip_dns_has_update = 0;
		}
		complete(&modem_if.atcmd_if.atcmd_response);
	}
	else if(strncasecmp(packet, "\r\nERROR", strlen("\r\nERROR")) == 0 ||
		strncasecmp(packet, "\r\n+CME ERROR", strlen("\r\n+CME ERROR")) == 0)
	{
		modem_if.atcmd_if.atcmd_result = ATCMD_RESULT_ERROR;
		complete(&modem_if.atcmd_if.atcmd_response);
	}
	else if(strncasecmp(packet, "\r\nCONNECT", strlen("\r\nCONNECT")) == 0)
	{
		modem_if.atcmd_if.atcmd_result = ATCMD_RESULT_OK;
		complete(&modem_if.atcmd_if.atcmd_response);
	}
	else if(strncasecmp(packet, "\r\n+CGDCONT: ", strlen("\r\n+CGDCONT: ")) == 0)
	{
		/* Parse PDP context list.
		 * Sample: +CGDCONT: 1,"IP","cmnet","10.60.176.183",0,0,802110030100108106d38870328306d38814cb,
		 */
		char *buf, *start;
		char *tmpStr;
		int i = 0;
		int ip_address = 0, priDNS = 0, secDNS = 0;

		buf = kmalloc(size, GFP_ATOMIC);
		if(buf == NULL)
		{
			printk("%s: malloc error\n", __func__);
			return;
		}
		memcpy(buf, packet, size);
		start = buf + strlen("\r\n+CGDCONT: ");
		while((tmpStr = strsep(&start, ",")))
		{
			i++;
			if(*tmpStr == '\0')
				continue;
			/*cid*/
			if(i == 1)
			{
				if(tmpStr[0] - '0' != modem_if.modem_curr_cid + 1)
					break;
			}
			/*IP address*/
			else if(i == 4)
			{
				int len = strlen(tmpStr);
				tmpStr[len - 1] = '\0';
				if(tmpStr[1] != '\0')
					in4_pton(tmpStr + 1, len - 2, (u8 *)&ip_address, '\0', NULL);
				modem_if.atcmd_if.ipParams.ipv4.inIPAddress = ntohl(ip_address);
				printk("%s: ip address: %x\n",  __func__, ip_address);
			}
			/*DNS address*/
			else if(i == 7)
			{
				getDNS(tmpStr, &priDNS, &secDNS);
				modem_if.atcmd_if.ip_dns_has_update = 1;
				modem_if.atcmd_if.ipParams.ipv4.inPrimaryDNS = ntohl(priDNS);
				modem_if.atcmd_if.ipParams.ipv4.inSecondaryDNS = ntohl(secDNS);
				printk("%s: dns address: %x,%x\n", __func__, priDNS, secDNS);
			}
		}
		kfree(buf);
	}
}
/*
 * RX tasklet takes data out of the RX queue and hands it up to the TTY
 * layer until it refuses to take any more data (or is throttled back).
 * Then it issues reads for any further data.
 *
 * If the RX queue becomes full enough that no usb_request is queued,
 * the OUT endpoint may begin NAKing as soon as its FIFO fills up.
 * So QUEUE_SIZE packets plus however many the FIFO holds (usually two)
 * can be buffered before the TTY layer's buffers (currently 64 KB).
 */
static void modem_rx(unsigned char cid, char *packet, int size)
{
	/* when
	   modem_if.modem_state == ACM_CONTROL_MODE then data
	   passed via TTY to atcmdsrv otherwise passed to PPP */
	if ((modem_if.modem_state == ACM_DATA_MODE) && (modem_if.cbFuncs.messageReq) && (size > 0)) {
		if(cid == INVALID_CID)
		{
			modem_parse_atcmd(packet, size);
		}
		else
		{
			modem_if.cbFuncs.messageReq(packet, size);
		}
	}
}
