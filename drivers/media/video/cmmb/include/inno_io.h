
/*
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


#ifndef __INNO_CTL_H_
#define __INNO_CTL_H_
#include <linux/types.h>
#include <linux/ioctl.h>

/**
 * ch_config -channel config parameter
 * @ch_id       :channel id, >= 1
 * @start_timeslot: start timeslot for this channel
 * @timeslot_count: timeslot count 
 * @demod_config: 
 * @sub_frame: if206 compatibale
 */
struct ch_config {
        unsigned char ch_id;
        unsigned char start_timeslot;
        unsigned char timeslot_count;
        unsigned char demod_config;
	unsigned char in_reserved_out_sub_frame;
	unsigned char in_sub_frame_out_data_type;
};

/**
 * sys_status - 
 * @sync :signal sync status, 1 sync, 0 no sync
 * @signal_strength: signal strength
 * @cur_freq: current frequency
 * @ldpc_err_percent: 
 * @rs_err_percent:
 */
struct sys_status {
        unsigned char sync;
        unsigned char signal_strength;
        unsigned char signal_quality;
        unsigned char cur_freq;
        unsigned char ldpc_err_percent;
        unsigned char rs_err_percent;
//add by mahanghong 20110118
//	 unsigned long err_status;
};

/**
 * err_info 
 */
struct err_info {
        unsigned int ldpc_total_count;
        unsigned int ldpc_error_count;
        unsigned int rs_total_count;
        unsigned int rs_error_count;
        unsigned short BER;       
        unsigned short SNR;       
};

//add by mahanghong 20110118
/**
 * err_status
 */
typedef enum{
	CAS_OK = 0x00,
	NO_MATCHING_CAS = 0x15,
	CARD_OP_ERROR = 0x17,

	MAC_ERR = 0x80,
	GSM_ERR = 0x81,
	KEY_ERR	= 0x82,
	KS_NOT_FIND	= 0x83,
	KEY_NOT_FIND	= 0x84,
	CMD_ERR	= 0x85,
}ERR_STATUS;

/**
 * uam_param - uam transfer parameter
 * @buf_in :in buf 
 * @len_in :len for in buf
 * @buf_out:out buf
 * @len_out:len for out buf
 * @sw : rsponse status
 */
struct uam_param {
        unsigned char *buf_in;
        unsigned int  len_in;
        unsigned char *buf_out;
        unsigned int  *len_out;
        unsigned short sw;
};

struct cmbbms_sk {
        unsigned char ISMACrypSalt[18];
        unsigned char SKLength;
};

struct cmbbms_isma {
        unsigned char MBBMS_ECMDataType;
        struct cmbbms_sk ISMACrypAVSK[2];
};

//add by mahanghong 20110210
struct cmbbms_cw {
		unsigned char KI_INDEX;
		unsigned char CW_DATA[16];
};

//add by mahanghong 20110218
struct aid_3g {
		unsigned char AIDLength;
		unsigned char AID_DATA[16];
};

struct mbbms_isma_param {
	unsigned char ch_id;
	unsigned char subframe_id;
        unsigned char isbase64;
        struct cmbbms_isma mbbms_isma;
};

struct mbbms_cw_param {
	unsigned char ch_id;
	unsigned char subframe_id;
	struct cmbbms_cw cw;
};

struct fw_info {
      unsigned char input[36]; 
      unsigned char output[16]; 
};

#ifdef IF206
/**
 * emm_ch_config -emm channel config parameter, this structure is IF206 specific
 * @ch_id       :channel id, >= 1
 * @start_timeslot: start timeslot for this channel
 * @timeslot_count: timeslot count 
 * @demod_config: combination of rs-mode, byte-interleave mode, ldpc mode and modulation mode
 * @sub_frame: sub-frame ID 
 * @is_emm: a flag to indicate this channel is configged for emm channel or not
 * @is_open: a flag stands for to open this channel or not
 */
struct emm_ch_config {
        unsigned char ch_id;
        unsigned char start_timeslot;
        unsigned char timeslot_count;
        unsigned char demod_config;
        unsigned char is_open;
        unsigned char sub_frame;
        unsigned char is_emm; //always 1 for emm channel
        unsigned char reserve;	
};

/**
* card sn information structure
*/
struct card_sn_info {
    int len;               /*output*/
    unsigned char sn[256]; /*output*/
};

/**
* raw ca table structure
*/
struct raw_cat_info {

/**
* Error Codes for structure raw_cat_info
*/
#define  CAT_OPS_SUCCESS        0
#define  CAT_OPS_CASID_MISMATCH 1
#define  CAT_OPS_CRC_ERROR      2
#define  CAT_OPS_COMM_ERROR     3

    int   err;    /*output*/
    int   emm_id; /*output*/
    int   len;    /*input*/
    void* data;   /*input*/
};
#endif

enum ca_err_state {
        FW_CAS_OK                       = 0x00,  
        FW_CARD_NOT_EXIST               = 0x10,  
        FW_NOT_AUTHORIZED               = 0x11,
        FW_CARD_FREEZED                 = 0x12,
        FW_NO_MATCHING_EMM              = 0x13,  
        FW_NO_MATCHING_ECM              = 0x14,
        FW_NO_MATCHING_CAS              = 0x15,
        FW_DATA_ERR                     = 0x16,
        FW_CARD_OP_ERROR                = 0x17,

        FW_CA_PRODUCT_UNKNOWN           = 0x40,
        FW_CA_PRODUCT_VALID 	        = 0x41,
        FW_CA_PRODUCT_OBSOLETE          = 0x42,
        FW_CA_ERROR_PARAMETER           = 0x45,
        FW_CA_SUBSCRIBER_UNKNOWN        = 0x47,
        FW_CA_SUBSCRIBER_REGISTERED     = 0x48,
        FW_CA_SUBSCRIBER_SUSPENDED      = 0x49,
        FW_CA_SUBSCRIBER_NOT_REGISTERED = 0x4A,

        FW_MAC_ERR                      = 0x80,
        FW_NO_SPPORT_GSM_TAG            = 0x81,
        FW_KEY_ERR                      = 0x82,
        FW_KS_NOT_FIND                  = 0x83,
        FW_KEY_NOT_FIND                 = 0x84,
        FW_UAM_CMD_ERR                  = 0x85
};

struct ca_err_stat {
	unsigned char ch_id;
	int intr;
	int err[10];
};

#define INNO_IOC_MAGIC      'i'
#define INNO_IO_POWERENABLE             _IOW(INNO_IOC_MAGIC, 1, int)
#define INNO_IO_RESET                   _IOW(INNO_IOC_MAGIC, 2, int)

#define INNO_IO_GET_FW_VERSION          _IOR(INNO_IOC_MAGIC, 5, unsigned int)
#define INNO_IO_SET_FREQUENCY           _IOW(INNO_IOC_MAGIC, 6, unsigned char)
#define INNO_IO_GET_FREQUENCY           _IOR(INNO_IOC_MAGIC, 7, unsigned char)
#define INNO_IO_SET_CH_CONFIG           _IOW(INNO_IOC_MAGIC, 8, struct ch_config)
#define INNO_IO_GET_CH_CONFIG           _IOWR(INNO_IOC_MAGIC, 9, struct ch_config)
#define INNO_IO_GET_SYS_STATUS          _IOR(INNO_IOC_MAGIC, 10, struct sys_status)
#define INNO_IO_GET_ERR_INFO            _IOR(INNO_IOC_MAGIC, 11, struct err_info)
#define INNO_IO_GET_CHIP_ID             _IOR(INNO_IOC_MAGIC, 12, int)

#ifdef IF228
#define INNO_IO_UAM_TRANSFER            _IOWR(INNO_IOC_MAGIC, 13, struct uam_param)
#define INNO_IO_UAM_SETOVER             _IO(INNO_IOC_MAGIC, 14)
#define INNO_IO_UAM_SETCARDENV          _IOW(INNO_IOC_MAGIC, 15, unsigned char)
#define INNO_IO_UAM_MBBMS_ISMA          _IOW(INNO_IOC_MAGIC, 16, struct mbbms_isma_param)
#define INNO_IO_UAM_FWINFO              _IOW(INNO_IOC_MAGIC, 17, struct fw_info)

#define INNO_IO_UAM_SETCW               _IOW(INNO_IOC_MAGIC, 18, struct cmbbms_cw)
#define INNO_IO_UAM_SetAID3G            _IOW(INNO_IOC_MAGIC, 19, struct aid_3g)
#endif

#define INNO_IO_ENABLE_CW_DETECT_MODE	_IOW(INNO_IOC_MAGIC, 20, unsigned char)
#define INNO_IO_GET_CW_FREQ_OFFSET      _IOR(INNO_IOC_MAGIC, 21, int)

#ifdef IF206
#define INNO_IO_GET_CA_CARD_SN          _IOW(INNO_IOC_MAGIC, 22, struct card_sn_info) 
#define INNO_IO_SET_EMM_CHANNEL         _IOW(INNO_IOC_MAGIC, 23, struct emm_ch_config)
#define INNO_IO_SET_CA_TABLE            _IOW(INNO_IOC_MAGIC, 24, struct raw_cat_info)
#endif

#define INNO_IO_GET_CA_STATE            _IOWR(INNO_IOC_MAGIC, 25, struct ca_err_stat)
#define INNO_IO_SYS_STATUS_CHG        _IOW(INNO_IOC_MAGIC, 26, int)
#define INNO_IO_SET_SLEEP	        	  _IOW(INNO_IOC_MAGIC, 27, int)

#ifdef IF228
#define INNO_IO_GET_FW_STATE              _IOR(INNO_IOC_MAGIC, 28, int)
#endif

#endif

