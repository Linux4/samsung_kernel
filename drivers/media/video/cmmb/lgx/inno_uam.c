
/*
 * innofidei if2xx demod uam driver
 * 
 * Copyright (C) 2010 Innofidei Corporation
 * Author:      sean <zhaoguangyu@innofidei.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/suspend.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/slab.h>
#include <linux/time.h>
#include "inno_uam.h"
#include "inno_comm.h"
#include "inno_io.h"
#include "inno_demod.h"

#ifdef INNO_DEBUG_LOGTOFILE
extern unsigned char inno_log_buf[];
extern void inno_logfile_write(unsigned char*in_buf);
#undef pr_debug
#undef pr_err
#define pr_debug(fmt, ...) \
		{sprintf(inno_log_buf,"INFO:[INNO/lgx/inno_uam.c]" pr_fmt(fmt),##__VA_ARGS__); \
				inno_logfile_write(inno_log_buf);}
#define pr_err(fmt, ...) \
		{sprintf(inno_log_buf,"ERR:[INNO/lgx/inno_uam.c]" pr_fmt(fmt),##__VA_ARGS__); \
				inno_logfile_write(inno_log_buf);}
#else

#undef pr_debug
#define pr_debug(fmt, ...) \
printk(KERN_DEBUG "[INNO/lgx/inno_uam.c]" pr_fmt(fmt),##__VA_ARGS__)

#undef pr_err
#define pr_err(fmt, ...) \
printk(KERN_ERR "[INNO/lgx/inno_uam.c]" pr_fmt(fmt), ##__VA_ARGS__)

#endif

#ifdef IF228

#define IF228_HW_DESCRAMBLE

/* following 2 line also defined in inno_cmd.h */
#define RSP_DATA_VALID                          0x80
#define CMD_BUSY                                        0x80
/* following 1 line also defined in inno_reg.h */
#define FETCH_PER_COMM0             0x50002140     /* CMD code */
#define FETCH_PER_COMM1             0x50002141     /* AP communication register 1 */
#define FETCH_PER_COMM31            0x5000215f    /* bit7:1 busy 0 over bit6-0:0 sucess 1 error */


#define UAM_BASE_ADDR           0x0000BC00
#define UAM_STATUS_REG          (UAM_BASE_ADDR + 0x004)
#define UAM_INT_REG             (UAM_BASE_ADDR + 0x005)   //temp
#define UAM_DATA_REG            (UAM_BASE_ADDR + 0x008)

#define SCARD_PROTOCOL_UNDEFINED    0x00000000  // There is no active protocol.
#define SCARD_PROTOCOL_T0           0x00000001  // T=0 is the active protocol.
#define SCARD_PROTOCOL_T1           0x00000002  // T=1 is the active protocol.

/* Block waiting integer default value as definded by ISO */
#define T1_BWI_DEFAULT               4
/* Character waiting integer default value as definded by ISO */
#define T1_CWI_DEFAULT               13

#define CMD_UAM_CMD_TEST                        0xC0
#define CMD_UAM_RESET                           0xC1
#define CMD_UAM_SEND_DATA                       0xC2
#define CMD_UAM_GET_DATA                        0xC3
#define CMD_UAM_PPS                             0xC4
#define CMD_UAM_SET_CLOCK                       0xC5
#define CMD_GET_AES_CIPER                       0x08
#define CMD_GET_FW_STATE		           0xF

#ifdef IF228_HW_DESCRAMBLE
#define CMD_SET_CARD_ENV                        0x07
#define CMD_SET_MBBMS_ISMA                      0xc8
#define CMD_SET_UAM_AID_3G			0xc9
#define CMD_SET_UAM_OVER                        0xED
#define CMD_UAM_SET_CW                          0xCA
#endif


//#define NS_TO_JIFFIES(TIME)	((unsigned long)(TIME) / (NSEC_PER_SEC / HZ))

typedef enum _OP_MODE
{
        OP_SPECIFIC,
        OP_NEGOTIABLE,
}OP_MODE_T;

typedef struct _CLOCK_RATE_CONVERSION {

        const unsigned long F;
        const unsigned long fs; 

} CLOCK_RATE_CONVERSION, *PCLOCK_RATE_CONVERSION;

typedef struct _BIT_RATE_ADJUSTMENT {

        const unsigned long DNumerator;
        const unsigned long DDivisor;

} BIT_RATE_ADJUSTMENT, *PBIT_RATE_ADJUSTMENT;

typedef struct _PTS_DATA {
#define PTS_TYPE_DEFAULT 0x00
#define PTS_TYPE_OPTIMAL 0x01
#define PTS_TYPE_USER    0x02

    unsigned char Type;  

    unsigned char Fl;    // Fl value for PTS

    unsigned char Dl;               // Dl value for PTS

    unsigned long  CLKFrequency;    // New clock frequency

    unsigned long DataRate;    // New baud rate to be used after pts

    unsigned char StopBits;    // new stop bits to be used after pts
} PTS_DATA, *PPTS_DATA;

typedef struct _SCARD_CARD_CAPABILITIES
{
        /* Flag that indicates that the current card uses invers convention */
        int InversConvention;

        /* Calculated etu */
        unsigned long   etu;
      
    /* Answer To Reset string returned by card. */
        struct {
                unsigned char Buffer[64];
                unsigned char Length;
        } ATR;

        struct {
                unsigned char Buffer[16];
                unsigned char Length;
        } HistoricalChars;

        PCLOCK_RATE_CONVERSION  ClockRateConversion;
        PBIT_RATE_ADJUSTMENT    BitRateAdjustment;

        unsigned char FI;       // Clock rate conversion 
        unsigned char DI;       // Bit rate adjustment
        unsigned char II;       // Maximum programming current
        unsigned char P;        // Programming voltage in .1 Volts
        unsigned char N;        // Extra guard time in etu 
        unsigned long GT;// Calculated guard time in micro seconds

        struct {
                unsigned long Supported;                // This is a bit mask of the supported protocols
                unsigned long Selected;         // The currently selected protocol
        } Protocol;

        /* T=0 specific data */
        struct {
                unsigned char WI;               // Waiting integer
                unsigned long WT;               // Waiting time in micro seconds
        } T0;

        /* T=1 specific data */
        struct {
                unsigned char IFSC;             // Information field size of card

                /* Character waiting integer and block waiting integer */
                unsigned char CWI;
                unsigned char BWI;

                unsigned char EDC;              // Error detection code

                /* Character and block waiting time in micro seconds */
                unsigned long CWT;
                unsigned long BWT;

                unsigned long BGT;              // Block guarding time in micro seconds
        } T1;

    PTS_DATA PtsData;

    unsigned char Reserved[100 - sizeof(PTS_DATA)];

} SCARD_CARD_CAPABILITIES, *PSCARD_CARD_CAPABILITIES;




static int inno_uam_major = -1;

static struct lgx_device *uam_dev;
static DEFINE_MUTEX(uam_lock);


static OP_MODE_T       OperationMode_UAM;
static SCARD_CARD_CAPABILITIES  cardCapabilities_UAM;

struct uam_cmd {
        unsigned char   *cmd;
        int             len;
};

struct uam_rsp {
        unsigned char   *rsp;
        int             *len;
};

static int uam_send_cmd_handler(void *data)
{
        struct uam_cmd *cmd_frm = (struct uam_cmd *)data;
        unsigned char *cmd = cmd_frm->cmd;
        int len = cmd_frm->len;
	unsigned char cmd_status=0;

        if (inno_comm_cmd_idle(&cmd_status)) {
		if (len > 8) {
                        inno_comm_send_unit(UAM_BASE_ADDR, (cmd+8), (len-8));
                }
				
		inno_comm_send_unit(FETCH_PER_COMM1, (cmd+1), 7);

		/* set busy status */	
		cmd_status = CMD_BUSY;	
		inno_comm_send_unit(FETCH_PER_COMM31, &cmd_status, 1);	

                inno_comm_send_unit(FETCH_PER_COMM0, &cmd[0], 1);
                return 0;
        }
        return -EBUSY;
}

static int uam_send_cmd(unsigned char *cmd, int len)
{
        struct inno_req req;
        struct uam_cmd cmd_frm;
 
        pr_debug("%s:CLA=0x%02x,INS=0x%02x\n", __func__,cmd[16],cmd[17]);
        cmd_frm.cmd = cmd;
        cmd_frm.len = len;
        req.handler = uam_send_cmd_handler;
        req.context = &cmd_frm;
        return inno_req_sync(&req);
}

static int uam_get_rsp_handler(void *data)
{
        struct uam_rsp *rsp_frm = (struct uam_rsp *)data;
        unsigned char *rsp = rsp_frm->rsp;
        int *len = rsp_frm->len;
        unsigned char status = 0, outBuf[8] = {0};
        unsigned int datalen = 0;
        int i;
        int ret = 0;

        for (i = 0; i < 1000; i++) {
                inno_comm_get_unit(UAM_STATUS_REG, &status, 1);   
                if (status != 0x00) {
                        if (status  == 0x80) {
                                inno_comm_get_unit(UAM_BASE_ADDR, outBuf, 8);   
                                datalen= (outBuf[2] << 8) + outBuf[3];
                                *len = datalen;
                                if(datalen > *len) 
								{
									ret = -ENOMEM; //edit by liujinpeng
									break;
								}
                                
                                inno_comm_get_unit(UAM_DATA_REG, rsp, datalen);
                        } else {
                                ret = -EIO;
                        }
                        break;
                }
        }
		if (i>=1000) {
			ret = -EIO;
			pr_err("%s:1000 times failed.Data in buffer is:", __func__);
            inno_comm_get_unit(UAM_BASE_ADDR, outBuf, 8);   
			for (i=0;i<8;i++)
			{
				printk(KERN_ERR "%02x ",outBuf[i]);
			}
			printk(KERN_DEBUG "\n");
		}//add by liujinpeng ,in case of timeout, we must tell the caller that we failed.
        status = 0;
        inno_comm_send_unit(UAM_STATUS_REG, &status, 1);  
        return ret;
}

/**
 * uam_get_rsp - get response from demod uam
 *
 * this func is called after uam_send_cmd,
 * an interrupt should occur after uam_send_cmd
 * so this func wait interrupt first ,then call inno_req_sync to get response data
 */
static int uam_get_rsp(unsigned char *rsp, int *len)
{
        struct inno_req req;
        struct uam_rsp rsp_frm;
		int ret;

        DECLARE_COMPLETION_ONSTACK(done);
        pr_debug("%s\n", __func__); 

        mutex_lock(&uam_lock);
        uam_dev->priv = NULL;
        if (uam_dev->valid == 0) {
                uam_dev->priv = &done;
                mutex_unlock(&uam_lock);

                pr_debug("%s:wait for rsp irq(set time out 10s)\n", __func__); 
                //if (wait_for_completion_interruptible(&done)) {
                if (!wait_for_completion_interruptible_timeout(&done,msecs_to_jiffies(10000))) { //edit by liujinpeng ,wait 10s
                        pr_err("%s:wait failed\n",__func__);
                        return -ERESTARTSYS;
                }
								
				mutex_lock(&uam_lock);
				uam_dev->priv = NULL;
        }
        uam_dev->valid = 0;
        mutex_unlock(&uam_lock);

        rsp_frm.rsp = rsp;
        rsp_frm.len = len;

        req.handler = uam_get_rsp_handler;
        req.context = &rsp_frm;
        ret = inno_req_sync(&req);
		//pr_debug("Data len=0x%x,timer=%ld\n",*(rsp_frm.len),timer);
		//pr_debug("uam_get_rsp:exit i=%d\n",i);
		return ret;
}

static int uam_reset(unsigned char *pATRValue, unsigned int *pATRLen)
{
        unsigned char cmd[16] = {0};
        int ret = 0;
        unsigned int i;
        
        cmd[0] = CMD_UAM_RESET;
        cmd[8] = 0xAA;
        cmd[9] = 0x55;
        cmd[10] = 0x00;
        cmd[11] = 0x00;  

        for (i = 0; i < 3; i++) {
                ret = uam_send_cmd(cmd, 16); 
                if (ret < 0) 
                        continue;
                               
                ret = uam_get_rsp(pATRValue, pATRLen);
                
                if (ret < 0)
                        continue;
                else 
                        break;
        }

        return ret;
}

/*************************************************************************************
 * CHIP_V5 PPS  CMD FORMAT:
 *      CMD_UAM_PPS | 0xAA | 0x55 | 0x00 | 0x04 | 0x00 | 0x00 | 0x00 | pps[0] | pps[1] | pps[2] | pps[3]
 *      ***************************************************************************************/
static int uam_pps(unsigned char *ppsReqValue)
{
        unsigned char cmd[32] = {0};
        unsigned char rsp[33] = {0};
        unsigned int len = 33;
        unsigned int retry = 0;
        int ret = 0;

        cmd[0] = CMD_UAM_PPS;
        cmd[8] = 0xAA;
        cmd[9] = 0x55;
        cmd[10] = 0x00;
        cmd[11] = 0x04;
        
        memcpy(cmd+16, ppsReqValue, 4);
        
        for (retry = 0; retry < 3; retry++) {
                ret = uam_send_cmd(cmd, 20);
                if (ret < 0)
                        continue;

                ret = uam_get_rsp(rsp, &len);
                if (ret < 0)
                        continue;
                else
                        break;
        }
        if (ret < 0)
                return -EIO;

        if (memcmp(rsp, ppsReqValue, 4) == 0)
                return 0;
        else
                return -EIO;
}

static int uam_UpdateCardCapabilities(void)
{
                unsigned char *atrString = cardCapabilities_UAM.ATR.Buffer;
                unsigned long atrLength = (unsigned long) cardCapabilities_UAM.ATR.Length;
                unsigned char   Tck, TA[4]={0}, TB[4]={0}, TC[4]={0}, TD[4]={0}, Y;
                unsigned char TA2Present = 0;
                unsigned long i, numProtocols = 0, protocolTypes = 0;
                int status =0;
                
                if (atrLength < 2) {
                        pr_err("ATR is too short (Min. length is 2)!\n");
                        return -EINVAL;
                }
                
                if (atrString[0] != 0x3b && atrString[0] != 0x3f && atrString[0] != 0x03) {
                        pr_err("Initial character %02xh of ATR is invalid\n",atrString[0]);
                        return -EINVAL;
                }
                
                /* Test for invers convention */
                if (*atrString == 0x03) {
                        cardCapabilities_UAM.InversConvention = 1; //????
                        return -EINVAL;                   
                }
                
                atrString += 1;
                atrLength -= 1;
                
                /* Calculate check char, but do not test now since if only T=0  is present the ATR doesn't contain a check char */
                for (i = 0, Tck = 0; i < atrLength; i++) {
                        Tck ^= atrString[i];
                }
                
                /* Set default values as described in ISO 7816-3 */
                TA[0] = 0x11;           // TA1 codes FI in high-byte and Dl in low-byte;
                TB[0] = 0x25;           // TB1 codes II in bits b7/b6 and PI1 in b5-b1. b8 has to be 0
                // *TC[0] = 0x0;       // TC1 codes N in bits b8-b1
                TC[1] = 10;                     // TC2 codes T=0 WI
                
                /* Translate ATR string to TA to TD values (See ISO) */
                cardCapabilities_UAM.HistoricalChars.Length = *atrString & 0x0f;
                Y = *atrString & 0xf0;
                
                atrString += 1;
                atrLength -= 1;
                
                for (i=0; i<4; i++) {
                        if (Y & 0x10) {
                                if (i==1)
                                        TA2Present = 1; 
                                                                
                                TA[i] = *atrString++;
                                atrLength--;
                        }
                        
                        if (Y & 0x20) {
                                TB[i] = *atrString++;
                                atrLength--;
                        }
                        
                        if (Y & 0x40) {
                                TC[i] = *atrString++;
                                atrLength--;
                        }
                        
                        if (Y & 0x80) {
                                Y = *atrString & 0xf0;
                                TD[i] = *atrString++ & 0x0f;
                                atrLength--;
                                
                                /* Check if the next parameters are for a new protocol. */
                                if (((1 << TD[i]) & protocolTypes) == 0){
                                        /* Count the number of protocols that the card supports */
                                        numProtocols++;
                                }
                                protocolTypes |= 1 << TD[i];
                        } else {
                                break;
                        }
                }
                
                /* Check if the card supports a protocol other than T=0 */
                if (protocolTypes & ~1) {
                        /* The atr contains a checksum byte. Exclude that from the historical byte length check */
                        atrLength -=1;          
        
                        /* This card supports more than one protocol or a protocol other than T=0, so test if the checksum is correct */
                        if (Tck != 0) {
                                pr_err("ATR Checksum is invalid\n");
                                status = 0;
                                goto _leave_lab;
                        }
                }
                
                if (atrLength < 0 || atrLength != cardCapabilities_UAM.HistoricalChars.Length) {
                        pr_err("ATR length is inconsistent\n");
                        status = 0;
                        goto _leave_lab;
                }
                
        _leave_lab:
                if (status != 0) {
                        return status;
                }
                
                /* store historical characters */
                memcpy(cardCapabilities_UAM.HistoricalChars.Buffer, atrString, cardCapabilities_UAM.HistoricalChars.Length);
                
                /* Now convert TA - TD values to global interface bytes */
                cardCapabilities_UAM.FI = (TA[0] & 0xf0) >> 4;// Clock rate conversion
                cardCapabilities_UAM.DI = (TA[0] & 0x0f);       // bit rate adjustment
                cardCapabilities_UAM.II = (TB[0] & 0xc0) >> 6;  // Maximum programming current factor
                cardCapabilities_UAM.P = (TB[1] ? TB[1] : (TB[0] & 0x1f) * 10); // Programming voltage in 0.1 Volts
                cardCapabilities_UAM.N = TC[0]; // Extra guard time
        
                if ((TA2Present || (numProtocols <= 1)) && (cardCapabilities_UAM.FI == 1) && (cardCapabilities_UAM.DI == 1)) {
                        /* If the card supports only one protocol (or T=0 as default) */
                        /* and only standard paramters then PTS selection is not available */
                        OperationMode_UAM = OP_SPECIFIC;
                } else {
                        OperationMode_UAM = OP_NEGOTIABLE;
                }
        
                /* Now find protocol specific data */
                if (TD[0] == 0) {               
                        cardCapabilities_UAM.Protocol.Supported |= SCARD_PROTOCOL_T0;
                        cardCapabilities_UAM.T0.WI = TC[1];
                        pr_debug("T=0 Values from ATR:\n  WI = %x\n",cardCapabilities_UAM.T0.WI);
                }
          
                if (protocolTypes & SCARD_PROTOCOL_T1) {
                        for (i = 0; TD[i] != 1 && i < 4; i++)
                                ;
                
                        for (; TD[i] == 1 && i < 4; i++) 
                                ;
        
                        if (i == 4)
                                return -EINVAL;
        
                        cardCapabilities_UAM.Protocol.Supported |= SCARD_PROTOCOL_T1;
        
                        cardCapabilities_UAM.T1.IFSC = (TA[i] ? TA[i] : 32);
                        cardCapabilities_UAM.T1.CWI = ((TB[i] & 0x0f) ? (TB[i] & 0x0f) : T1_CWI_DEFAULT);
                        cardCapabilities_UAM.T1.BWI = ((TB[i] & 0xf0) >> 4 ? (TB[i] & 0xf0) >> 4 : T1_BWI_DEFAULT);
                        cardCapabilities_UAM.T1.EDC = (TC[i] & 0x01);
        
                }
                
                if (OperationMode_UAM == OP_SPECIFIC) {
                        if (TA2Present) {
                                /* TA2 is present in the ATR, so use the protocol indicated in the ATR */
                                cardCapabilities_UAM.Protocol.Selected = 1 << TA[1];
                        } else {
                                /* The card only supports one protocol So make that one protocol the current one to use */
                                cardCapabilities_UAM.Protocol.Selected = cardCapabilities_UAM.Protocol.Supported;
                        }
                }
                        
                return 0;       
}

static int uam_init(void)
{
        unsigned char ATRBuf[40] ={0};  
        unsigned char pps_request[] ={0xFF, 0x10, 0, 0};
        
        unsigned int atrLen = 40;
        int ret=0;
        
        ret = uam_reset(ATRBuf, &atrLen);
        if (ret < 0) {
                pr_err("[INNO_UAM_Init] - Reset fail\n");
                return -EIO;
        } else {
                if (atrLen <= 0) {
                        pr_err("[INNO_UAM_Init] - ATR Value Length error!!!\n");
                        return -EINVAL;
                }
                
                memcpy(cardCapabilities_UAM.ATR.Buffer,ATRBuf,atrLen);          
                cardCapabilities_UAM.ATR.Length = atrLen;
                 
                ret = uam_UpdateCardCapabilities();
                if (ret < 0)
                        return -EINVAL;
                
                pr_debug("[INNO_UAM_Init] - INNO_UAM_Request_PPS\n");
        
                if (cardCapabilities_UAM.FI == 0x1 && cardCapabilities_UAM.DI == 0x8)
                                pps_request[2]=0x18;
                else
                                pps_request[2]=0x96;  //if208: 0x94; v5:0x96
                       
                        
                pps_request[3] = pps_request[0] ^ pps_request[1] ^pps_request[2];
                        
                ret = uam_pps(pps_request);
                if (ret < 0) {
                        pr_err("UAM PPS Request failed\n");
                        return -EIO;
                }
                        
                pr_debug("UAM PPS Request successfully\n");
                
         }
        
         return 0;
}


#ifdef IF228_HW_DESCRAMBLE
int uam_setover(void)  
{
/* 
 *      CHIP_V5 CMD Description:
 *      A command identify that a group of UAM APDU is over 
 *
 *      CMD[0] = CMD_SET_UAM_OVER
 *      CMD[1] = 0
 *      CMD[2] = 0
 *      CMD[3] = 0
 *      CMD[4] = 0
 *      CMD[5] = 0
 *      CMD[6] = 0
 *      CMD[7] = 0
 */
        int ret = 0;
        struct inno_cmd_frame cmd_frm = {0};
        int retry = 0;

        cmd_frm.code = CMD_SET_UAM_OVER;
        for(retry = 0; retry < 20; retry ++){

                ret = inno_comm_send_cmd(&cmd_frm);
                
                if(ret < 0)
                        continue;
                else 
                        break;
        }

        return ret;
}


static int uam_set_cardenv(unsigned char airnetwork)  
{
        int ret = 0;
        struct inno_cmd_frame cmd_frm = {0};
        int retry = 0;

        cmd_frm.code = CMD_SET_CARD_ENV;
        cmd_frm.data[0] = airnetwork;

        for (retry = 0; retry < 20; retry ++) {
                
                ret = inno_comm_send_cmd(&cmd_frm);
                if (ret < 0)
                        continue;
                else 
                        break;
        }
        
        return ret;
}


static int uam_set_mbbms_isma(unsigned char ch_id, unsigned char subframe_id, unsigned char isBase64, struct cmbbms_isma *isma)
{
        unsigned char cmd[64] = {0};
        int ret = 0;
        unsigned int retry = 0;

	pr_debug("[setisma] ch_id=%d, subframe_id=%d\n", ch_id, subframe_id);
        
        cmd[0] = CMD_SET_MBBMS_ISMA;
	cmd[1] = ch_id+1;
	cmd[2] = subframe_id;
	cmd[8] = 0xAA;
        cmd[9] = 0x55;
        cmd[10] = 0x00;
        cmd[11] = 0x27;
        cmd[12] = 0x00;
        cmd[13] = 0x00;
        cmd[14] = 0x00;
        cmd[15] = isBase64;
        
        cmd[16] = (unsigned char)isma->MBBMS_ECMDataType;
        memcpy(cmd+17, isma->ISMACrypAVSK[0].ISMACrypSalt,18);
        cmd[35] = isma->ISMACrypAVSK[0].SKLength;   
        memcpy(cmd+36, isma->ISMACrypAVSK[1].ISMACrypSalt,18);
        cmd[54] = isma->ISMACrypAVSK[1].SKLength;

        //OutputStr(cmd+9, 20);
        //OutputStr(cmd+29, 19);
        
        for (retry = 0; retry < 20; retry ++) {
                ret = uam_send_cmd(cmd, 55);
                if (ret < 0)
                        continue;
                else 
                        break;
        }

        return ret;
}


static int uam_set_cw(unsigned char ch_id, unsigned char subframe_id, struct cmbbms_cw *cw)
{
        unsigned char cmd[64] = {0};
        int ret = 0;
        unsigned int retry = 0;
        
	pr_debug("[setcw] ch_id=%d, subframe_id=%d\n", ch_id, subframe_id);

        cmd[0] = CMD_UAM_SET_CW;
	cmd[1] = ch_id+1;
	cmd[2] = subframe_id;
        cmd[8] = 0xAA;
        cmd[9] = 0x55;
        cmd[10] = 0x00;
        cmd[11] = 0x11;
        cmd[12] = 0x00;
        cmd[13] = 0x00;
        cmd[14] = 0x00;
        cmd[15] = 0x00;
 
	cmd[16] = cw->KI_INDEX;
        memcpy(cmd+17, cw->CW_DATA,16);

        for (retry = 0; retry < 20; retry ++) {
                ret = uam_send_cmd(cmd, 33);
                if (ret < 0)
                        continue;
                else 
                        break;
        }

        return ret;
}

static int uam_set_aid_3g(struct aid_3g *aid)
{
		unsigned char cmd[50] = {0};
		int ret = 0;
		unsigned int retry = 0;
		
		cmd[0] = CMD_SET_UAM_AID_3G;
		cmd[8] = 0xAA;
		cmd[9] = 0x55;
		cmd[10] = 0x00;
		cmd[11] = 0x10;
		cmd[12] = 0x00;
		cmd[13] = 0x00;
		cmd[14] = 0x00;
		cmd[15] = 0x00;

		memcpy(cmd+16, aid->AID_DATA, aid->AIDLength);
		
        for (retry = 0; retry < 20; retry ++) {
       			ret = uam_send_cmd(cmd, aid->AIDLength+16);
        		if (ret < 0)
                		continue;
       			else 
               			break;
        }

		return ret;
}
#endif

#define MAKEWORD(low,high)      ((unsigned short)((unsigned char)(low)) | (((unsigned short)(unsigned char)(high))<<8))

#ifdef IF228_HW_DESCRAMBLE
static int isLastCmd;
#endif

static int uam_transfer(void *data)
{
        struct uam_param *param = (struct uam_param *)data;
        int             ret = 0;
        unsigned char   cmd[270] = {0}, rsp[256] = {0};
        unsigned int    cmdLen = 0, rsplen = 256;
        unsigned char *pBufIn = param->buf_in;
        unsigned char *pBufOut = param->buf_out;
        unsigned int  bufInLen = param->len_in;
        unsigned int  *bufOutLen = param->len_out;

        pr_debug("%s\n", __func__);
 
        if (bufInLen == 4) {
                pr_err("%s:len_in == 4, inval param\n", __func__);
                return -EINVAL;

        } else if(bufInLen == 5) {               
                /* cmd = CMD_UAM_GET_DATA + 0xaa+0x55+data len high + data len low + data */
                cmd[0] = CMD_UAM_GET_DATA;
                cmd[8] = 0xAA;
                cmd[9] = 0x55;    
                cmd[10] = 0x00;
                cmd[11] = 0x05;    
                
#ifdef IF228_HW_DESCRAMBLE
                if(isLastCmd == 1)
                {
                        cmd[15] = 0xED;
                        isLastCmd = 0;
                }
#endif
                memcpy(cmd+16, pBufIn, 5);
                
                cmdLen = 0x15;  //bufInLen + 16
                
                ret = uam_send_cmd(cmd, cmdLen);   
                if (ret < 0)  
                        return -EIO;

                ret = uam_get_rsp(rsp, &rsplen);
                if (ret < 0)
                        return -EIO;               
 
                if (rsplen < 3)
                        return -EIO;    

                param->sw = MAKEWORD(rsp[rsplen-1], rsp[rsplen-2]);
                *bufOutLen = rsplen - 3;
                memcpy(pBufOut, rsp+1, *bufOutLen);
                
        } else if (bufInLen > 5 && bufInLen == (int)pBufIn[4] + 5) {
                
                /* cmd = CMD_UAM_SEND_DATA + 0xaa + 0x55+ data len low + data len high + data */
                cmd[0] = CMD_UAM_SEND_DATA;
                cmd[8] = 0xAA;
                cmd[9] = 0x55;
                
#ifdef IF228_HW_DESCRAMBLE
                if(isLastCmd == 1)
                {
                        cmd[15] = 0xED;
                        isLastCmd = 0;
                        pr_debug("last uam cmd\n");
                }
#endif
                
                if (bufInLen < 256) {
                        cmd[10] = 0x00;
                        cmd[11] = bufInLen;                      
                } else {               
                        pr_err("%s:len_in > 256, inval param\n", __func__);
                        return -EINVAL;      
                }       

                memcpy(cmd+16, pBufIn, bufInLen);
                
                cmdLen = bufInLen + 16; 

                ret = uam_send_cmd(cmd, cmdLen);
                if (ret < 0) 
                        return -EIO;

                ret = uam_get_rsp(rsp, &rsplen);
                if (ret < 0)
                        return -EIO;
                
                if (rsplen < 3)
                        return -EIO;

                param->sw = MAKEWORD(rsp[rsplen-1], rsp[rsplen-2]);

                *bufOutLen = rsplen - 3;
                memcpy(pBufOut, rsp+1, *bufOutLen);

        } else if (bufInLen > 5 && bufInLen > (int)pBufIn[4] + 5) {
                pr_err("%s:len_in > 5 && bufInLen > pBufIn[4] + 5\n", __func__);
                return -EINVAL;
        } else {
                pr_err("%s:%d:inval param\n", __func__, __LINE__);
                return -EINVAL;
        }

        pr_debug("%s:exit\n", __func__);
        return 0;   
}

static int uam_fwinfo(void *data)
{
        int ret = 0;
        int retry = 0,i;
        struct inno_cmd_frame cmd_frm = {0};
        unsigned char cmd_status = 0;
        struct fw_info *info = (struct fw_info *)data;
         
        for(retry = 0; retry < 3; retry++) {

                inno_comm_send_unit(UAM_BASE_ADDR,info->input+3,32);    

                cmd_frm.code = CMD_GET_AES_CIPER;
                ret = inno_comm_send_cmd(&cmd_frm);
                if(ret < 0)
                        continue;                    
                for(i=0;i<1000;i++) {

                        ret = inno_comm_get_unit(FETCH_PER_COMM31, &cmd_status, 1);
                        if (ret < 0)
                                break;
                        if ((cmd_status & CMD_BUSY) == 0) {
                                     inno_comm_get_unit(UAM_BASE_ADDR,info->output,16); 
                                     return ret;
                        }
               }
        }

        return ret;
}

static int uam_fwstate(void *data)
{
        struct inno_cmd_frame cmd_frame = {0};
        struct inno_rsp_frame rsp_frame = {0};
        unsigned char *status = (unsigned char *)data;
        int ret = 0;        
 
        pr_debug("%s\n", __func__);
        if (data == NULL)
                return -EINVAL;        

        cmd_frame.code  = CMD_GET_FW_STATE; 

	 ret = inno_comm_send_cmd(&cmd_frame);        
        if(ret == 0) {
                ret = inno_comm_get_rsp(&rsp_frame);
                if (ret < 0)
                        goto done;                
                
                *status = rsp_frame.data[0];
        } 

done:
        return ret;
}


static int innouam_open(struct inode* inode, struct file* filep)
{
        int ret = 0;
        pr_debug("%s\n", __func__);

        ret = inno_demod_inc_ref();
        if (ret < 0)
                return ret;
        
        ret = uam_init();
        
        return ret;
}

static int innouam_release(struct inode *inode, struct file *filep)
{
        pr_debug("%s\n", __func__);
        inno_demod_dec_ref();
        return 0;
}
//////////////inno change begin from liugge 20120816 ///////////////////
//static unsigned char uam_buffer[600];//should big enough for tx+rx+some headers
///////////////inno change end from liuge 20120816//////////////////////

//static int innouam_ioctl(struct inode* inode, struct file* filp, unsigned int cmd, unsigned long arg)
long innouam_ioctl(struct file* filp, unsigned int cmd, unsigned long arg)
{
        int ret = 0;
        void __user *argp = (void __user *)arg;
        switch(cmd) {
                case INNO_IO_UAM_TRANSFER:
                {
                        struct uam_param param;
                        struct uam_param __user *paramp;
                        unsigned char *buf;
                        int buf_len;
                        unsigned int len_out_want;
                        unsigned int len_out_result=256;

                        paramp = argp;

                        if (copy_from_user(&param, argp, sizeof(param)))
                                return -EFAULT;

                        if (copy_from_user(&len_out_want, paramp->len_out, sizeof(len_out_want)))
                                return -EFAULT;


                        pr_debug("%s:len_out_want = %d\n", __func__, len_out_want);

                        /* we assume out data max len is 256 bytes */
                        buf_len = param.len_in + 256;  
                        if (buf_len == 0)
                                return -EINVAL;
                         
                        
                        /* alloc tx and rx buf */
//////////////inno change begin from liugge 20120816 ///////////////////
                        buf = kmalloc(buf_len, GFP_KERNEL);//checked
						//buf = uam_buffer;
///////////////inno change end from liuge 20120816//////////////////////
						if (buf == NULL) {return -ENOMEM;}

                        /* get tx buf data if tx exist */
                        if (param.len_in && param.buf_in) {
                                if (copy_from_user(buf, param.buf_in, param.len_in))
                                        return -EFAULT;
                                param.buf_in = buf;
                        }

                        param.buf_out = buf + param.len_in;
                        param.len_out = &len_out_result;
                                 
                        ret = uam_transfer(&param);
                        if (ret < 0) {
//////////////inno change begin from liugge 20120816 ///////////////////
                                kfree(buf);
///////////////inno change end from liuge 20120816//////////////////////
                                return ret;       
                        }
                     
                        pr_debug("%s:len_out_result = %d\n",__func__, len_out_result); 
                        //if (len_out_result) {  //by liujinpeng in 20120419
                                if (len_out_result > len_out_want)
                                        return -EINVAL;
                                if (copy_to_user(paramp->buf_out, param.buf_out, len_out_result))
                                        return -EFAULT;
                                if (copy_to_user(paramp->len_out, &len_out_result, sizeof(len_out_result)))
                                        return -EFAULT;
                        //}

			pr_debug("%s:param.sw = %x\n",__func__, param.sw); 
                        if (copy_to_user(&paramp->sw, &param.sw, sizeof(param.sw))) {
                        
                                pr_err("%s:sw copy failed\n", __func__); 
                                return -EFAULT;
                
                        }
 //////////////inno change begin from liugge 20120816 ///////////////////                       
                        kfree(buf);
 ///////////////inno change end from liuge 20120816//////////////////////
                        break;
                }
                case INNO_IO_UAM_SETOVER:
                        if (uam_setover())
                                return -EIO;
                        break;
                case INNO_IO_UAM_SETCARDENV:
                {
                        unsigned char airnetwork;
                        if (copy_from_user(&airnetwork, argp, sizeof(airnetwork)))
                                return -EFAULT;
                        if (uam_set_cardenv(airnetwork))
                                return -EIO;
                        break;
                }
                case INNO_IO_UAM_MBBMS_ISMA:
                {
                        struct mbbms_isma_param isma_param;
                        if (copy_from_user(&isma_param, argp, sizeof(isma_param)))
                                return -EFAULT;
                        if (uam_set_mbbms_isma(isma_param.ch_id, isma_param.subframe_id, isma_param.isbase64, &isma_param.mbbms_isma))
                                return -EIO;        
                        break;
                }
				
		case INNO_IO_UAM_SETCW:
                {
                        struct mbbms_cw_param cw_param;			
		
                        if (copy_from_user(&cw_param, argp, sizeof(cw_param)))
                                return -EFAULT;
                        if (uam_set_cw(cw_param.ch_id, cw_param.subframe_id, &cw_param.cw))
                                return -EIO;        
                        break;
                }
				
		case INNO_IO_UAM_SetAID3G:
                {
                        struct aid_3g aid_param;
                        if (copy_from_user(&aid_param, argp, sizeof(aid_param)))
                                return -EFAULT;
                        if (uam_set_aid_3g(&aid_param))
                                return -EIO;        
                        break;
                }

                case INNO_IO_UAM_FWINFO:
                {
                        struct inno_req req;
                        struct fw_info info;
                        struct fw_info *infop;
                        infop = argp;

                        if (copy_from_user(&info, infop->input, sizeof(info.input)))
                                return -EFAULT;
                        
                        req.handler = uam_fwinfo;
                        req.context = &info;
                        ret = inno_req_sync(&req);
                        if (ret < 0)
                                break;
                        if (copy_to_user(infop->output, info.output, sizeof(info.output)))
                                return -EFAULT;
                        break;
                }
		  case INNO_IO_GET_FW_STATE:
                {
                        struct inno_req req;
                        unsigned char status;
                        
                        req.handler = uam_fwstate;
                        req.context = &status;
                        ret = inno_req_sync(&req);
                        if (ret < 0)
                                break;
                        if (copy_to_user((unsigned char *)argp, &status, 1))
                                return -EFAULT;
                        break;
                }
                default:
                        return -EINVAL;
                
        }
        return ret;
}

static struct file_operations innouam_fops = {
        .owner          = THIS_MODULE,
        .open           = innouam_open,
//        .ioctl          = innouam_ioctl, 
	.unlocked_ioctl = innouam_ioctl,
        .release        = innouam_release,
};


static int inno_uam_probe(struct lgx_device *lgx)
{
        int ret = 0;

        pr_debug("%s\n", __func__);
        
        return ret;
}

static int __devexit inno_uam_remove(struct lgx_device *lgx)
{
        pr_debug("%s\n", __func__);
         
        return 0;
}

/**
 * innouam_data_notify - called by inno_irq_req_handler if an uam interrupt occur
 *
 * uam interrupt only occur after uam_send_cmd
 */
static void innouam_data_notify(struct lgx_device *lgx)
{
        pr_debug("%s\n", __func__);
        mutex_lock(&uam_lock);
        uam_dev->valid = 1;
        mutex_unlock(&uam_lock);
        if (lgx->priv != NULL)
                complete(lgx->priv);
        return ;
}

static struct lgx_driver inno_uam_driver = {
        .probe          = inno_uam_probe,
        .remove         = __devexit_p(inno_uam_remove),
        .data_notify    = innouam_data_notify,
        .driver         = {
                .name   = UAM_PREFIX,
                        .owner  = THIS_MODULE,
        },
};


int inno_uam_init(void)
{
        int ret;
        pr_debug("%s\n", __func__);
        inno_uam_major = register_chrdev(0, "innouam", &innouam_fops);
        if (inno_uam_major < 0){ 
                pr_err("failed to register character device.\n");     
                return inno_uam_major; 
        }
        pr_debug("%s:uam major = %d\n", __func__, inno_uam_major);                

        ret = inno_lgx_driver_register(&inno_uam_driver);
        if (ret < 0) {
                pr_err("register inno uam driver failed! (%d)\n", ret);
                goto err;
        }
        uam_dev = inno_lgx_new_device(UAM_PREFIX, inno_uam_major, 0, NULL);
        if (uam_dev == NULL)
                goto err1;
        return 0;
err1:
        inno_lgx_driver_unregister(&inno_uam_driver);
err:
        unregister_chrdev(inno_uam_major, "innouam");
        return -EIO;        
}

void inno_uam_exit(void)
{
        pr_debug("%s\n", __func__);
        inno_lgx_driver_unregister(&inno_uam_driver);

        unregister_chrdev(inno_uam_major, "innouam");
        if (uam_dev)
                inno_lgx_device_unregister(uam_dev);
}
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sean.zhao <zhaoguangyu@innofidei.com>");
MODULE_DESCRIPTION("innofidei cmmb uam");

