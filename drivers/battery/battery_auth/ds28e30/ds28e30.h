/*******************************************************************************
 * Copyright (C) 2017 Maxim Integrated Products, Inc., All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY,  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of Maxim Integrated shall
 * not be used except as stated in the Maxim Integrated Products, Inc. Branding Policy.
 *
 * The mere transfer of this software does not imply any licenses
 * of trade secrets, proprietary technology, copyrights, patents,
 * trademarks, maskwork rights, or any other form of intellectual
 * property whatsoever. Maxim Integrated Products, Inc. retains all ownership rights.
 *******************************************************************************
 *
 *  DS28E30.h - Include file for DS28E30
 */

#ifndef __DS28E30_H__
#define __DS28E30_H__

#include "ucl_types.h"

/*just to build*/
//define return constant for authenticate_DS28E30 subroutine
#define No_Device -2
#define CRC_Error  -1
#define NotAuthenticated  0
#define Authenticated  1
/*just to build*/

// DS28E30 commands
#define XPC_COMMAND              0x66
#define CMD_WRITE_MEM            0x96
#define CMD_READ_MEM             0x44
#define CMD_READ_STATUS          0xAA
#define CMD_SET_PAGE_PROT        0xC3
#define CMD_COMP_READ_AUTH       0xA5
#define CMD_DECREMENT_CNT        0xC9
#define CMD_DISABLE_DEVICE       0x33
//#define CMD_GEN_ECDSA_KEY        0xCB
//#define CMD_READ_RNG             0xD2
#define CMD_READ_DEVICE_PUBLIC_KEY      0xCB
#define CMD_AUTHENTICATE_PUBLIC_KEY      0x59
#define CMD_AUTHENTICATE_WRITE      0x89

// Test Mode sub-commands
#define CMD_TM_ENABLE_DISABLE    0xDD
#define CMD_TM_WRITE_BLOCK       0xBB
#define CMD_TM_READ_BLOCK        0x66

// Result bytes
#define RESULT_SUCCESS                0xAA
#define RESULT_FAIL_PROTECTION        0x55
#define RESULT_FAIL_PARAMETETER       0x77
#define RESULT_FAIL_INVALID_SEQUENCE  0x33
#define RESULT_FAIL_ECDSA             0x22
#define RESULT_DEVICE_DISABLED	      0x88
#define RESULT_FAIL_VERIFY            0x00

#define RESULT_FAIL_COMMUNICATION     0xFF

// Pages
#define PG_USER_EEPROM_0  0
#define PG_USER_EEPROM_1  1
#define PG_USER_EEPROM_2  2
#define PG_USER_EEPROM_3  3
#define PG_CERTIFICATE_R    4
#define PG_CERTIFICATE_S    5
#define PG_AUTHORITY_PUB_KEY_X    6
#define PG_AUTHORITY_PUB_KEY_Y    7
#define PG_DS28E30_PUB_KEY_X    28
#define PG_DS28E30_PUB_KEY_Y    29
#define PG_DS28E30_PRIVATE_KEY    36

#define PG_DEC_COUNTER  106
//#define PG_WRITE_PUB_KEY_X    7
//#define PG_WRITE_PUB_KEY_Y    8


// Delays 
/* 32 PUF OVERSAMPLING
#define DELAY_DS28E30_EE_WRITE_TWM     100
#define DELAY_DS28E30_EE_READ_TRM      100   
#define DELAY_DS28E30_ECDSA_GEN_TGES   410
#define DELAY_DS28E30_TCMP             20
*/

// delays
#define DELAY_DS28E30_EE_WRITE_TWM     100       //maximal 100ms
#define DELAY_DS28E30_EE_READ_TRM      50       //maximal 100ms
#define DELAY_DS28E30_ECDSA_GEN_TGES   200      //maximal 130ms (tGFS)
#define DELAY_DS28E30_Verify_ECDSA_Signature_tEVS   200         //maximal 130ms (tGFS)
#define DELAY_DS28E30_ECDSA_WRITE 350          //for ECDSA write EEPROM


// Protection bit fields
#define PROT_RP                  0x01  // Read Protection
#define PROT_WP                  0x02  // Write Protection 
#define PROT_EM                  0x04  // EPROM Emulation Mode 
#define PROT_DC                  0x08  // Decrement Counter mode (only page 4)

#define PROT_AUTH                 0x20  // AUTH mode for authority public key X&Y

#define PROT_ECH                 0x40  // Encrypted read and write using shared key from ECDH
#define PROT_ECW                 0x80  // Authentication Write Protection ECDSA (not applicable to KEY_PAGES)

// Generate key flags
#define ECDSA_KEY_LOCK           0x80
#define ECDSA_USE_PUF            0x01

// misc constants
#ifndef TRUE
#define TRUE    1
#endif
#ifndef FALSE
#define FALSE   0
#endif

// 1-Wire selection methods
#define SELECT_SKIP     0
#define SELECT_RESUME   1
#define SELECT_MATCH    2
#define SELECT_ODMATCH  3
#define SELECT_SEARCH   4
#define SELECT_READROM  5
#define SELECT_ODSKIP   6

/* constants */
#define DS28E30_FAM  0x5B       //0xDB for custom DS28E30

// Command Functions (no high level verification)
extern int ds28e30_cmd_writeMemory(int pg, uchar *data);
extern int ds28e30_cmd_readMemory(int pg, uchar *data);
extern int ds28e30_cmd_readStatus(int pg, uchar *pr_data, uchar *manid, uchar *hardware_version);
extern int ds28e30_cmd_setPageProtection(int pg, uchar prot);
extern int ds28e30_cmd_computeReadPageAuthentication(int pg, int anon, uchar *challenge, uchar *sig);
extern int ds28e30_cmd_decrementCounter(void);
extern int ds28e30_cmd_DeviceDisable(uchar *release_sequence);
extern int ds28e30_cmd_readRNG(int len, uchar *data);

extern int ds28e30_cmd_verifyECDSASignature(uchar *sig_r, uchar *sig_s, uchar *custom_cert_fields, int cert_len);
extern int ds28e30_cmd_authendicatedECDSAWriteMemory(int pg, uchar *data, uchar *sig_r, uchar *sig_s );


// TEST MODE functions
extern int ds28e30_cmd_TM_EnableDisable(int enable_flag, uchar *psw, uchar *state);
extern int ds28e30_cmd_TM_writeBlock(int block, uchar *data);
extern int ds28e30_cmd_TM_readBlock(int block, uchar *data);

// High level functions
extern int ds28e30_cmd_readDevicePublicKey(uchar *data);
extern int ds28e30_computeAndVerifyECDSA(int pg, int anon, uchar *mempage, uchar *challenge, uchar *sig_r, uchar *sig_s);
extern int ds28e30_computeAndVerifyECDSA_NoRead(int pg, int anon, uchar *mempage, uchar *challenge, uchar *sig_r, uchar *sig_s);

//DS28E30 application functions
extern int verifyECDSACertificateOfDevice(uchar *sig_r, uchar *sig_s, uchar *pub_key_x, uchar *pub_key_y, uchar *SECONDARY_ROMID, uchar *SECONDARY_MANID, uchar *system_level_pub_key_x, uchar *system_level_pub_key_y);
extern int Authenticate_DS28E30(int PageNumber);



// Helper functions
extern int ds28e30_detect(uchar addr);
extern uchar ds28e30_getLastResultByte(void);
extern int standard_cmd_flow(uchar *write_buf, int write_len, int delay_ms, int expect_read_len, uchar *read_buf, int *read_len);

extern void ds28e30_setPublicKey(uchar *px, uchar *py);
extern void ds28e30_setPrivateKey(uchar *priv);
extern int ds28e30_read_ROMNO_MANID_HardwareVersion(void);
extern void ds28e30_set_ROMNO_MANID(uchar *setROMNO, uchar *setMANID);
extern void ds28e30_get_ROMNO_MANID(uchar *getROMNO, uchar *getMANID);

extern void setDeviceSelectMode(int method);
extern int DeviceSelect(void);

//ECDSA algorithm achieved by software
extern int sw_computeECDSASignature(uchar *message, int msg_len,  uchar *sig_r, uchar *sig_s);


extern uchar ROM_NO[2][8];
extern uchar MANID[2];
extern uchar HardwareVersion[2];
extern uchar crc8;
extern uchar last_result_byte;

extern unsigned char Certificate_Constant[16];
extern unsigned char  AuthorityPrivateKey[32];

extern unsigned char SystemPublicKeyX[32];
extern unsigned char SystemPublicKeyY[32];

#endif
