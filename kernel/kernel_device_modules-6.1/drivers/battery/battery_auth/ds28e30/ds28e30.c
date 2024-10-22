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
 *  ds28e30.c - ds28e30 device module. Requires low level I2C connection.
 */

#include <linux/kernel.h>
//#define ds28e30 to do
#include "ds28e30.h"
#include "1wire_protocol.h"
#include "deep_cover_coproc.h"
#include "ecdsa_generic_api.h"
#include "ucl_defs.h"
#include "ucl_sha256.h"
#include "sec_auth_ds28e30.h"

// Command Functions (no high level verification)
int ds28e30_cmd_writeMemory(int pg, uchar *data);
int ds28e30_cmd_readMemory(int pg, uchar *data);
int ds28e30_cmd_readStatus(int pg, uchar *pr_data, uchar *manid, uchar *hardware_version);
int ds28e30_cmd_setPageProtection(int pg, uchar prot);
int ds28e30_cmd_computeReadPageAuthentication(int pg, int anon, uchar *challenge, uchar *sig);
int ds28e30_cmd_decrementCounter(void);
int ds28e30_cmd_DeviceDisable(uchar *release_sequence);
int ds28e30_cmd_generateECDSAKeyPair(int use_puf, int lock_enable);
int ds28e30_cmd_verifyECDSASignature(uchar *sig_r, uchar *sig_s, uchar *custom_cert_fields, int cert_len);
int ds28e30_cmd_authendicatedECDSAWriteMemory(int pg, uchar *data, uchar *sig_r, uchar *sig_s );

// High level functions
int ds28e30_cmd_readDevicePublicKey(uchar *data);
int ds28e30_computeAndVerifyECDSA(int pg, int anon, uchar *mempage, uchar *challenge, uchar *sig_r, uchar *sig_s);
int ds28e30_computeAndVerifyECDSA_NoRead(int pg, int anon, uchar *mempage, uchar *challenge, uchar *sig_r, uchar *sig_s);
//DS28E30 application functions
int verifyECDSACertificateOfDevice(uchar *sig_r, uchar *sig_s, uchar *pub_key_x, uchar *pub_key_y, uchar *SECONDARY_ROMID, uchar *SECONDARY_MANID, uchar *system_level_pub_key_x, uchar *system_level_pub_key_y);
int Authenticate_DS28E30(int PageNumber);


// Helper functions
//int ds28e30_detect(uchar addr);
//uchar calc_crc8(uchar data);
int standard_cmd_flow(uchar *write_buf, int write_len, int delay_ms, int expect_read_len, uchar *read_buf, int *read_len);
uchar ds28e30_getLastResultByte(void);

void ds28e30_setPublicKey(uchar *px, uchar *py);
void ds28e30_setPrivateKey(uchar *priv);
int ds28e30_read_ROMNO_MANID_HardwareVersion(void);
//void ds28e30_set_ROMNO_MANID(uchar *setROMNO, uchar *setMANID);
//void ds28e30_get_ROMNO_MANID(uchar *getROMNO, uchar *getMANID);
unsigned short docrc16(unsigned short data);
unsigned short CRC16;

//ECDSA algorithm achieved by software
int  sw_computeECDSASignature(uchar *message, int msg_len,  uchar *sig_r, uchar *sig_s);

// keys in byte array format, used by software compute functions
uchar private_key[32];
uchar public_key_x[2][32];
uchar public_key_y[2][32];

// ds28e30 state
// just to buiild static uchar crc8;
uchar crc8;
uchar ROM_NO[2][8];
uchar MANID[2];
uchar HardwareVersion[2];

// last result byte
uchar last_result_byte = RESULT_SUCCESS; 

extern void delayms(unsigned int delay_ms);

unsigned char gPageData[2][32];
unsigned char gDevicePublicKey_X[2][32], gDevicePublicKey_Y[2][32];
unsigned char gPage_Certificate_R[2][32], gPage_Certificate_S[2][32];
bool PageData0ReadStatus[2] = {false, false};
bool PublicKeyReadStatus[2] = {false, false};
bool PageCertificateReadStatus[2] = {false, false};
bool RomManIdReadStatus[2] = {false, false};

//---------------------------------------------------------------------------
//-------- ds28e30 Memory functions 
//---------------------------------------------------------------------------

//--------------------------------------------------------------------------
/// 'Write Memory' command
///
/// @param[in] pg
/// page number to write
/// @param[in] data
/// buffer must be at least 32 bytes
///
///  @return
///  TRUE - command successful @n
///  FALSE - command failed
///
int ds28e30_cmd_writeMemory(int pg, uchar *data)
{
	uchar write_buf[50];
	int write_len;
	uchar read_buf[255];
	int read_len;
	int i;

	/*
	 * Reset
	 * Presence Pulse
	 * <ROM Select>
	 * TX: XPC Command (66h)
	 * TX: Length byte 34d
	 * TX: XPC sub-command 96h (Write Memory)
	 * TX: Parameter
	 * TX: New page data (32d bytes)
	 * RX: CRC16 (inverted of XPC command, length, sub-command, parameter)
	 * TX: Release Byte
	 * <Delay TBD>
	 * RX: Dummy Byte
	 * RX: Length Byte (1d)
	 * RX: Result Byte
	 * RX: CRC16 (inverted of length and result byte)
	 * Reset or send XPC command (66h) for a new sequence
	 */
	auth_dbg("%s: called\n", __func__);

	for (i = 0; i < SEC_AUTH_RETRY; i++) {
		// construct the write buffer
		write_len = 0;
		write_buf[write_len++] = CMD_WRITE_MEM;
		write_buf[write_len++] = pg;
		memcpy(&write_buf[write_len], data, 32);
		write_len += 32;

		// preload read_len with expected length
		read_len = 1;

		// default failure mode
		last_result_byte = RESULT_FAIL_COMMUNICATION;

		if (standard_cmd_flow(write_buf, write_len,  DELAY_DS28E30_EE_WRITE_TWM,
								read_len, read_buf, &read_len)) {
			// get result byte
			last_result_byte = read_buf[0];
			// check result
			if (read_len == 1)
				return (read_buf[0] == RESULT_SUCCESS);
		}
	}

	auth_info("%s: fail\n", __func__);
	// no payload in read buffer or failed command
	return FALSE;
}

//--------------------------------------------------------------------------
/// 'Read Memory' command
///
/// @param[in] pg
/// page number to read
/// @param[out] data
/// buffer length must be at least 32 bytes to hold memory read
///
///  @return
///  TRUE - command successful @n
///  FALSE - command failed
///
int ds28e30_cmd_readMemory(int pg, uchar *data)
{
	uchar write_buf[10];
	int write_len;
	uchar read_buf[255];
	int read_len;
	int i;

	/*
	 * Reset
	 * Presence Pulse
	 * <ROM Select>
	 * TX: XPC Command (66h)
	 * TX: Length byte 2d
	 * TX: XPC sub-command 69h (Read Memory)
	 * TX: Parameter (page)
	 * RX: CRC16 (inverted of XPC command, length, sub-command, and parameter)
	 * TX: Release Byte
	 * <Delay TBD>
	 * RX: Dummy Byte
	 * RX: Length (33d)
	 * RX: Result Byte
	 * RX: Read page data (32d bytes)
	 * RX: CRC16 (inverted, length byte, result byte, and page data)
	 * Reset or send XPC command (66h) for a new sequence
	 */
	auth_dbg("%s: called\n", __func__);
	for (i = 0; i < SEC_AUTH_RETRY; i++)	{
		// construct the write buffer
		write_len = 0;
		write_buf[write_len++] = CMD_READ_MEM;
		write_buf[write_len++] = pg;

		// preload read_len with expected length
		read_len = 33;

		// default failure mode
		last_result_byte = RESULT_FAIL_COMMUNICATION;

		if (standard_cmd_flow(write_buf, write_len,  DELAY_DS28E30_EE_READ_TRM,
								read_len, read_buf, &read_len)) {
			// get result byte
			last_result_byte = read_buf[0];
			// check result
			if (read_len == 33) {
				if (read_buf[0] == RESULT_SUCCESS) {
					memcpy(data, &read_buf[1], 32);
					return TRUE;
				}
			}
		}
	}

	auth_info("%s: fail\n", __func__);
	// no payload in read buffer or failed command
	return FALSE;
}

//--------------------------------------------------------------------------
/// 'Read Status' command
///
/// @param[in] pg
/// page to read protection
/// @param[out] pr_data
/// pointer to uchar buffer of length 6 for page protection data
/// @param[out] manid
/// pointer to uchar buffer of length 2 for manid (manufactorur ID)
///
///  @return
///  TRUE - command successful @n
///  FALSE - command failed
///
int ds28e30_cmd_readStatus(int pg, uchar *pr_data, uchar *manid, uchar *hardware_version)
{
	uchar write_buf[10];
	uchar read_buf[255];
	int read_len = 2, write_len;
	int i;
	/*
	 * Reset
	 * Presence Pulse
	 * <ROM Select>
	 * TX: XPC Command (66h)
	 * TX: Length byte 1d
	 * TX: XPC sub-command AAh (Read Status)
	 * RX: CRC16 (inverted of XPC command, length, and sub-command)
	 * TX: Release Byte
	 * <Delay TBD>
	 * RX: Dummy Byte
	 * RX: Length Byte (11d)
	 * RX: Result Byte
	 * RX: Read protection values (6 Bytes), MANID (2 Bytes), ROM VERSION (2 bytes)
	 * RX: CRC16 (inverted, length byte, protection values, MANID, ROM_VERSION)
	 * Reset or send XPC command (66h) for a new sequence
	 */
	auth_dbg("%s: called\n", __func__);

	for (i = 0; i < SEC_AUTH_RETRY; i++) {
		// construct the write buffer
		write_len = 0;
		write_buf[write_len++] = CMD_READ_STATUS;
		write_buf[write_len++] = pg;

		// preload read_len with expected length
		if (pg & 0x80)
			read_len = 5;

		// default failure mode
		last_result_byte = RESULT_FAIL_COMMUNICATION;

	//return standard_cmd_flow(write_buf, write_len,  DELAY_DS28E30_EE_READ_TRM,
	//							read_len, read_buf, &read_len);  //?????

		if (standard_cmd_flow(write_buf, write_len,  DELAY_DS28E30_EE_READ_TRM,
								read_len, read_buf, &read_len)) {
			// get result byte
			last_result_byte = read_buf[0];
			// should always be 2 or 5 length for status data
			if (read_len == 2 || read_len == 5) {
				if (read_buf[0] == RESULT_SUCCESS || read_buf[0] == RESULT_DEVICE_DISABLED) {
					if (read_len == 2)
						memcpy(pr_data, &read_buf[1], 1);
					else {
						memcpy(manid, &read_buf[1], 2);
						memcpy(hardware_version, &read_buf[3], 2);
					}
					return TRUE;
				}
			}
	   }
	}

	auth_info("%s: fail\n", __func__);
	// no payload in read buffer or failed command
	return FALSE;
}

//--------------------------------------------------------------------------
/// 'Set Page Protection' command
///
/// @param[in] pg
/// page to set protection
/// @param[in] prot
/// protection value to set
///
///  @return
///  TRUE - command successful @n
///  FALSE - command failed
///
int ds28e30_cmd_setPageProtection(int pg, uchar prot)
{
	uchar write_buf[10];
	int write_len;
	uchar read_buf[255];
	int read_len;

	/*
	 * Reset
	 * Presence Pulse
	 * <ROM Select>
	 * TX: XPC Command (66h)
	 * TX: Length byte 3d
	 * TX: XPC sub-command C3h (Set Protection)
	 * TX: Parameter (page)
	 * TX: Parameter (protection)
	 * RX: CRC16 (inverted of XPC command, length, sub-command, parameters)
	 * TX: Release Byte
	 * <Delay TBD>
	 * RX: Dummy Byte
	 * RX: Length Byte (1d)
	 * RX: Result Byte
	 * RX: CRC16 (inverted, length byte and result byte)
	 * Reset or send XPC command (66h) for a new sequence
	 */

	auth_dbg("%s: called\n", __func__);

	// construct the write buffer
	write_len = 0;
	write_buf[write_len++] = CMD_SET_PAGE_PROT;
	write_buf[write_len++] = pg;
	write_buf[write_len++] = prot;

	// preload read_len with expected length
	read_len = 1;

	// default failure mode
	last_result_byte = RESULT_FAIL_COMMUNICATION;

	if (standard_cmd_flow(write_buf, write_len,  DELAY_DS28E30_EE_WRITE_TWM, read_len, read_buf, &read_len)) {
		// get result byte
		last_result_byte = read_buf[0];
		// check result
		if (read_len == 1)
			return (read_buf[0] == RESULT_SUCCESS);
	}

	auth_info("%s: fail\n", __func__);
	// no payload in read buffer or failed command
	return FALSE;
}

//--------------------------------------------------------------------------
/// 'Compute and Read Page Authentication' command
///
/// @param[in] pg - page number to compute auth on
/// @param[in] anon - anonymous flag (1) for anymous
/// @param[in] challenge
/// buffer length must be at least 32 bytes containing the challenge
/// @param[out] data
/// buffer length must be at least 64 bytes to hold ECDSA signature
///
/// @return
/// TRUE - command successful @n
/// FALSE - command failed
///
int ds28e30_cmd_computeReadPageAuthentication(int pg, int anon, uchar *challenge, uchar *sig)
{
	uchar write_buf[200];
	int write_len;
	uchar read_buf[255];
	int read_len;
	int i;
	//int delay;

	/*
	 * Reset
	 * Presence Pulse
	 * <ROM Select>
	 * TX: XPC Command (66h)
	 * TX: Length byte 34d
	 * TX: XPC sub-command A5h (Compute and Read Page Authentication)
	 * TX: Parameter (page)
	 * TX: Challenge (32d bytes)
	 * RX: CRC16 (inverted of XPC command, length, sub-command, parameter, and challenge)
	 * TX: Release Byte
	 * <Delay TBD>
	 * RX: Dummy Byte
	 * RX: Length byte (65d)
	 * RX: Result Byte
	 * RX: Read ECDSA Signature (64 bytes, ‘s’ and then ‘r’, MSByte first, [same as ES10]),
	 * signature 00h's if result byte is not AA success
	 * RX: CRC16 (inverted, length byte, result byte, and signature)
	 * Reset or send XPC command (66h) for a new sequence
	 */

	auth_dbg("%s: called\n", __func__);

	for (i = 0; i < SEC_AUTH_RETRY; i++)	{
		// construct the write buffer
		write_len = 0;
		write_buf[write_len++] = CMD_COMP_READ_AUTH;
		write_buf[write_len] = pg&0x7f;
		if (anon)
			write_buf[write_len] |= 0xE0;
		write_len++;
		write_buf[write_len++] = 0x03; //authentication parameter
		memcpy(&write_buf[write_len], challenge, 32);
		write_len += 32;

		// preload read_len with expected length
		read_len = 65;

		// default failure mode
		last_result_byte = RESULT_FAIL_COMMUNICATION;

		if (standard_cmd_flow(write_buf, write_len,
				DELAY_DS28E30_ECDSA_GEN_TGES, read_len, read_buf, &read_len)) {
			// get result byte
			last_result_byte = read_buf[0];
			// check result
			if (read_len == 65) {
				if (read_buf[0] == RESULT_SUCCESS) {
					memcpy(sig, &read_buf[1], 64);
					return TRUE;
				}
			}
		}
	}

	auth_info("%s: fail\n", __func__);
	// no payload in read buffer or failed command
	return FALSE;
}

//--------------------------------------------------------------------------
/// 'Decrement Counter' command
///
///  @return
///  TRUE - command successful @n
///  FALSE - command failed
///
int ds28e30_cmd_decrementCounter(void)
{
	int write_len;
	uchar write_buf[10];
	uchar read_buf[255];
	int read_len;

	/*
	 * Presence Pulse
	 * <ROM Select>
	 * TX: XPC Command (66h)
	 * TX: Length byte 1d
	 * TX: XPC sub-command C9h (Decrement Counter)
	 * RX: CRC16 (inverted of XPC command, length, sub-command)
	 * TX: Release Byte
	 * <Delay TBD>
	 * RX: Dummy Byte
	 * RX: Length Byte (1d)
	 * RX: Result Byte
	 * RX: CRC16 (inverted, length byte and result byte)
	 * Reset or send XPC command (66h) for a new sequence
	 */
	
	auth_dbg("%s: called\n", __func__);

	// construct the write buffer
	write_len = 0;
	write_buf[write_len++] = CMD_DECREMENT_CNT;

	// preload read_len with expected length
	read_len = 1;

	// default failure mode
	last_result_byte = RESULT_FAIL_COMMUNICATION;


	if (standard_cmd_flow(write_buf, write_len,  DELAY_DS28E30_EE_WRITE_TWM+50, read_len, read_buf, &read_len)) {
		// get result byte
		last_result_byte = read_buf[0];
		// check result byte
		if (read_len == 1)
			return (read_buf[0] == RESULT_SUCCESS);
	}

	auth_info("%s: fail\n", __func__);
	// no payload in read buffer or failed command
	return FALSE;
}

//--------------------------------------------------------------------------
/// 'Device Disable' command
///
/// @param[in] release_sequence
/// 8 byte release sequence to disable device
///
///  @return
///  TRUE - command successful @n
///  FALSE - command failed
///
int ds28e30_cmd_DeviceDisable(uchar *release_sequence)
{
	uchar write_buf[10];
	int write_len;
	uchar read_buf[255];
	int read_len;
	int i;

	/*
	 * Reset
	 * Presence Pulse
	 * <ROM Select>
	 * TX: XPC Command (66h)
	 * TX: Length byte 9d
	 * TX: XPC sub-command 33h (Disable command)
	 * TX: Release Sequence (8 bytes)
	 * RX: CRC16 (inverted of XPC command, length, sub-command, and release sequence)
	 * TX: Release Byte
	 * <Delay TBD>
	 * RX: Dummy Byte
	 * RX: Length Byte (1d)
	 * RX: Result Byte
	 * RX: CRC16 (inverted, length byte and result byte)
	 * Reset or send XPC command (66h) for a new sequence
	 */

	auth_dbg("%s: called\n", __func__);

	for (i = 0; i < SEC_AUTH_RETRY; i++) {
		// construct the write buffer
		write_len = 0;
		write_buf[write_len++] = CMD_DISABLE_DEVICE;
		memcpy(&write_buf[write_len], release_sequence, 8);
		write_len += 8;

		// preload read_len with expected length
		read_len = 1;

		// default failure mode
		last_result_byte = RESULT_FAIL_COMMUNICATION;

		if (standard_cmd_flow(write_buf, write_len,
				DELAY_DS28E30_EE_WRITE_TWM, read_len, read_buf, &read_len)) {
			// get result byte
			last_result_byte = read_buf[0];
			// check result
			if (read_len == 1)
				return (read_buf[0] == RESULT_SUCCESS);
		}
	}

	auth_info("%s: fail\n", __func__);
	// no payload in read buffer or failed command
	return FALSE;
}

//--------------------------------------------------------------------------
/// 'Read device public key' command
///
/// @param[in]
/// no param required
/// @param[out] data
/// buffer length must be at least 64 bytes to hold device public key read
///
///  @return
///  TRUE - command successful @n
///  FALSE - command failed
///
int ds28e30_cmd_readDevicePublicKey(uchar *data)
{
	auth_dbg("%s: called\n", __func__);
	if ((ds28e30_cmd_readMemory(PG_DS28E30_PUB_KEY_X, data)) == false) {
		auth_info("%s: PG_DS28E30_PUB_KEY_X read fail\n", __func__);
		return FALSE;
	}
	if ((ds28e30_cmd_readMemory(PG_DS28E30_PUB_KEY_Y, data+32)) == false) {
		auth_info("%s: PG_DS28E30_PUB_KEY_Y read fail\n", __func__);
		return FALSE;
	}
	return TRUE;
}
//--------------------------------------------------------------------------
/// 'authenticated Write memory' command
///
/// @param[in] pg
/// page number to write
/// @param[in] data
/// buffer must be at least 32 bytes
/// @param[in] certificate sig_r
/// @param[in] certificate sig_s

///
///  @return
///  TRUE - command successful @n
///  FALSE - command failed
///
int ds28e30_cmd_authendicatedECDSAWriteMemory(int pg, uchar *data, uchar *sig_r, uchar *sig_s )
{
	uchar write_buf[128];
	int write_len;
	uchar read_buf[16];
	int read_len;
	int i;

	/*
	 * Reset
	 * Presence Pulse
	 * <ROM Select>
	 * TX: XPC Command (66h)
	 * TX: Length byte 98d
	 * TX: XPC sub-command 89h (authenticated Write Memory)
	 * TX: Parameter
	 * TX: New page data (32d bytes)
	 * TX: Certificate R&S (64 bytes)
	 * RX: CRC16 (inverted of XPC command, length, sub-command, parameter, page data, certificate R&S)
	 * TX: Release Byte
	 * <Delay TBD>
	 * RX: Dummy Byte
	 * RX: Length Byte (1d)
	 * RX: Result Byte
	 * RX: CRC16 (inverted of length and result byte)
	 * Reset or send XPC command (66h) for a new sequence
	 */

	auth_dbg("%s: called\n", __func__);

	for (i = 0; i < SEC_AUTH_RETRY; i++)	{
		// construct the write buffer
		write_len = 0;
		write_buf[write_len++] = CMD_AUTHENTICATE_WRITE;
		write_buf[write_len++] = pg & 0x03;
		memcpy(&write_buf[write_len], data, 32);
		write_len += 32;
		memcpy(&write_buf[write_len], sig_r, 32);
		write_len += 32;
		memcpy(&write_buf[write_len], sig_s, 32);
		write_len += 32;

		// preload read_len with expected length
		read_len = 1;

		// default failure mode
		last_result_byte = RESULT_FAIL_COMMUNICATION;

		if (standard_cmd_flow(write_buf, write_len,
					DELAY_DS28E30_EE_WRITE_TWM + DELAY_DS28E30_Verify_ECDSA_Signature_tEVS,
					read_len, read_buf, &read_len)) {
			// get result byte
			last_result_byte = read_buf[0];
			// check result
			if (read_len == 1)
				return (read_buf[0] == RESULT_SUCCESS);
		}
	}

	auth_info("%s: fail\n", __func__);
	// no payload in read buffer or failed command
   return FALSE;
}


//---------------------------------------------------------------------------
//-------- ds28e30 High level functions 
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
/// High level function to do a full challenge/response ECDSA operation 
/// on specified page 
///
/// @param[in] pg
/// page to do operation on
/// @param[in] anon
/// flag to indicate in anonymous mode (1) or not anonymous (0)
/// @param[out] mempage
/// buffer to return the memory page contents
/// @param[in] challenge
/// buffer containing challenge, must be 32 bytes
/// @param[out] sig_r
/// buffer for r portion of signature, must be 32 bytes 
/// @param[out] sig_s
/// buffer for s portion of signature, must be 32 bytes 
///
/// @return
/// TRUE - command successful @n
/// FALSE - command failed
///
int ds28e30_computeAndVerifyECDSA(int pg, int anon, uchar *mempage, uchar *challenge, uchar *sig_r, uchar *sig_s)
{
	auth_dbg("%s: called\n", __func__);
	// read destination page
	if (!ds28e30_cmd_readMemory(pg, mempage)) {
		auth_info("%s: ds28e30_cmd_readMemory fail\n", __func__);
		return FALSE;
	}

	return ds28e30_computeAndVerifyECDSA_NoRead(pg, anon, mempage, challenge, sig_r, sig_s);
}

//---------------------------------------------------------------------------
/// High level function to do a full challenge/response ECDSA operation
/// on specified page
///
/// @param[in] pg
/// page to do operation on
/// @param[in] anon
/// flag to indicate in anonymous mode (1) or not anonymous (0)
/// @param[in] mempage
/// buffer with memory page contents, required for verification of ECDSA signature
/// @param[in] challenge
/// buffer containing challenge, must be 32 bytes
/// @param[out] sig_r
/// buffer for r portion of signature, must be 32 bytes
/// @param[out] sig_s
/// buffer for s portion of signature, must be 32 bytes
///
/// @return
/// TRUE - command successful @n
/// FALSE - command failed
///
int ds28e30_computeAndVerifyECDSA_NoRead(int pg, int anon, uchar *mempage, uchar *challenge, uchar *sig_r, uchar *sig_s)
{
	uchar signature[64], message[256];
	int msg_len;
	uchar *pubkey_x, *pubkey_y;

	auth_dbg("%s: called\n", __func__);
	// compute and read auth command
	if (!ds28e30_cmd_computeReadPageAuthentication(pg, anon, challenge, signature)) {
		auth_info("%s: fail\n", __func__);
		return FALSE;
	}

	// put the signature in the return buffers, signature is 's' and then 'r', MSByte first
	memcpy(sig_s, signature, 32);
	memcpy(sig_r, &signature[32], 32);

	// construct the message to hash for signature verification
	// ROM NO | Page Data | Challenge (Buffer) | Page# | MANID

	// ROM NO
	msg_len = 0;
	if (anon)
		memset(&message[msg_len], 0xFF, 8);
	else
		memcpy(&message[msg_len], ROM_NO[Active_Device - 1], 8);
	msg_len += 8;
	// Page Data
	memcpy(&message[msg_len], mempage, 32);
	msg_len += 32;
	// Challenge (Buffer)
	memcpy(&message[msg_len], challenge, 32);
	msg_len += 32;
	// Page#
	message[msg_len++] = pg;
	// MANID
	memcpy(&message[msg_len], MANID, 2);
	msg_len += 2;

	pubkey_x = public_key_x[Active_Device - 1];
	pubkey_y = public_key_y[Active_Device - 1];

	// verify Signature and return result
	return deep_cover_verifyECDSASignature(message, msg_len, pubkey_x, pubkey_y, sig_r, sig_s);
}

//---------------------------------------------------------------------------
/// Verify certificate of devices like DS28C36/DS28C36/DS28E38/DS28E30.
///
/// @param[in] sig_r
/// Buffer for R portion of certificate signature (MSByte first)
/// @param[in] sig_s
/// Buffer for S portion of certificate signature (MSByte first)
/// @param[in] pub_x
/// Public Key x to verify
/// @param[in] pub_y
/// Public Key y to verify
/// @param[in] SECONDARY_ROMID
/// device's 64-bit ROMID (LSByte first)
/// @param[in] SECONDARY_MANID
/// Maxim defined as manufacturing ID
/// @param[in] system_level_pub_key_x
/// 32-byte buffer container the system level public key x
/// @param[in] system_level_pub_key_y
/// 32-byte buffer container the system level public key y
///
///  @return
///  TRUE - certificate valid @n
///  FALSE - certificate not valid
///
int verifyECDSACertificateOfDevice(uchar *sig_r, uchar *sig_s, uchar *pub_key_x, uchar *pub_key_y, uchar *SECONDARY_ROMID, uchar *SECONDARY_MANID, uchar *system_level_pub_key_x, uchar *system_level_pub_key_y)
{
		unsigned char buf[32];
	auth_dbg("%s: called\n", __func__);
	// setup software ECDSA computation
	deep_cover_coproc_setup(0, 0, 0, 0);

	// create customization field
	// 16 zeros (can be set to other customer specific value)
	memcpy(buf, Certificate_Constant, 16);
	// ROMID
	memcpy(&buf[16], SECONDARY_ROMID, 8);
	// MANID
	memcpy(&buf[24], SECONDARY_MANID, 2);
	return deep_cover_verifyECDSACertificate(sig_r, sig_s, pub_key_x, pub_key_y, buf, 26,
								system_level_pub_key_x, system_level_pub_key_y);
}

void DS28E30_Fixed_data(void)
{
	int i = 0;

	for (i = 0; i < 8; i++)
		auth_dbg("%s: ROM_NO[0][%d] = %d, ROM_NO[1][%d] = %d\n",
				__func__, i, ROM_NO[0][i], i, ROM_NO[1][i]);

	for (i = 0; i < 32; i++)
		auth_dbg("%s: gPageData[0][%d] = %d, gPageData[1][%d] = %d\n",
				__func__, i, gPageData[0][i], i, gPageData[1][i]);

	for (i = 0; i < 32; i++)
		auth_dbg("%s: gDevicePublicKey_X[0][%d] = %d, gDevicePublicKey_X[1][%d] = %d\n",
				__func__, i, gDevicePublicKey_X[0][i], i, gDevicePublicKey_X[1][i]);

	for (i = 0; i < 32; i++)
		auth_dbg("%s: gDevicePublicKey_Y[0][%d] = %d, gDevicePublicKey_Y[1][%d] = %d\n",
					__func__, i, gDevicePublicKey_Y[0][i], i, gDevicePublicKey_Y[1][i]);

	for (i = 0; i < 32; i++)
		auth_dbg("%s: gPage_Certificate_R[0][%d] = %d, gPage_Certificate_R[1][%d] = %d\n",
				__func__, i, gPage_Certificate_R[0][i], i, gPage_Certificate_R[1][i]);

	for (i = 0; i < 32; i++)
		auth_dbg("%s: gPage_Certificate_S[0][%d] = %d, gPage_Certificate_S[1][%d] = %d\n",
				__func__, i, gPage_Certificate_S[0][i], i, gPage_Certificate_S[1][i]);

	for (i = 0; i < 2; i++)
		auth_dbg("%s: MANID[%d] = %d\n", __func__, i, MANID[i]);
}

//---------------------------------------------------------------------------
/// Authenticate both device certificate and digital signautre
/// @param[in] PageNumber
/// Indicate which EEPROM page number will used, could be 0,1,2,3,4,5,6,7
/// @param[in] anon
/// if anon='1', then device's ROMID will be displaced with all 0xFFs to generate/verify signature; if anon='0', unique ROMID is used
/// @param[in] challenge
/// reserved challenge data to generate random digital signature
///  @return
///  TRUE - both certificate/digital signature is valid @n
///  FALSE or NotAuthecticated - either certificate or digital signature is invalid
///  No_device - DS28E30 is not found
///  CRC_Error - 1-wire communication error, need to do authentication again
int Authenticate_DS28E30(int PageNumber)
{
	int i = 0;
	unsigned char flag = false;
	unsigned char buf[128];
	static unsigned char sig_r[32], sig_s[32];
	unsigned char challenge[32];

	//read the device ROMID, MANID and HardwareVersion from the active DS28E30
	if (RomManIdReadStatus[Active_Device - 1] == false) {
		if ((ds28e30_read_ROMNO_MANID_HardwareVersion()) == false) {
			auth_err("%s: CRC Error\n", __func__);
			return CRC_Error;
		}
		RomManIdReadStatus[Active_Device - 1] = true;
		auth_dbg("%s: First time read rommanID\n", __func__);
	}

	//read the device public key X&Y
	if (PublicKeyReadStatus[Active_Device - 1] == false) {
		flag = ds28e30_cmd_readDevicePublicKey(buf);   //read device public key X&Y
		if (flag != true) {
			auth_err("%s: CRC Error\n", __func__);
			return CRC_Error;
		}

		memcpy(gDevicePublicKey_X[Active_Device - 1], buf, 32);			//reserve device public key X
		memcpy(gDevicePublicKey_Y[Active_Device - 1], &buf[32], 32);	//reserve device public key Y
		ds28e30_setPublicKey(gDevicePublicKey_X[Active_Device - 1],
							gDevicePublicKey_Y[Active_Device - 1]);
		PublicKeyReadStatus[Active_Device - 1] = true;
		auth_dbg("%s: First time read publicKeys\n", __func__);
	}

	//read device certificate
	if (PageCertificateReadStatus[Active_Device - 1] == false) {
		for (i = 0; i < 2; i++) {
			flag = ds28e30_cmd_readMemory(PG_CERTIFICATE_R + i, buf + i * 32); //read device Certificate R&S
			if (flag != true) {
				auth_err("%s: CRC Error\n", __func__);
				return CRC_Error;
			}
		}

		memcpy(gPage_Certificate_R[Active_Device - 1], buf, 32);  //reserve device certificate R
		memcpy(gPage_Certificate_S[Active_Device - 1], &buf[32], 32);  //reserve device certificate S
		PageCertificateReadStatus[Active_Device - 1] = true;
		auth_dbg("%s: First time read Certificates\n", __func__);
	}

	/* authenticate DS28E30 by two steps: digital signature verification, device certificate verification
	 * to verify the digital signature
	 * read page data for digital signature
	 */
	if (PageData0ReadStatus[Active_Device - 1] == false) {
		flag = ds28e30_cmd_readMemory(PageNumber, gPageData[Active_Device - 1]);   //read page data
		if (flag != true) {
			auth_err("%s: CRC Error\n", __func__);
			return CRC_Error;
		}

		PageData0ReadStatus[Active_Device - 1] = true;
		auth_dbg("%s: First time read pagedata for pagenumber(%d)\n", __func__, PageNumber);
	}

	//Read all fixed data
	DS28E30_Fixed_data();

	//generate random challenge
	memcpy(buf, sig_r, 32);
	memcpy(&buf[32], sig_s, 32);
	ucl_sha256(challenge, buf, 64);
	// setup software ECDSA computation
	deep_cover_coproc_setup(0, 0, 0, 0);
	flag = ds28e30_computeAndVerifyECDSA_NoRead(0, 0, gPageData[Active_Device - 1],
										challenge, sig_r, sig_s);
	if (flag == false) {
		auth_info("%s: ds28e30_computeAndVerifyECDSA fail\n", __func__);
		return NotAuthenticated;  //digital signature is not verified
	}

	//verify device certificate
	flag = verifyECDSACertificateOfDevice(gPage_Certificate_R[Active_Device - 1],
			gPage_Certificate_S[Active_Device - 1], gDevicePublicKey_X[Active_Device - 1],
			gDevicePublicKey_Y[Active_Device - 1], ROM_NO[Active_Device - 1],
			MANID, SystemPublicKeyX, SystemPublicKeyY);

	if (flag == false) {
		auth_info("%s: verifyECDSACertificate Of Device fail\n", __func__);
		return NotAuthenticated;  //device certificate is not verified
	} else
		return Authenticated;  //both digital signature & device certificate are verified
}

//---------------------------------------------------------------------------
//-------- ds28e30 Helper functions
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
/// Return last result byte. Useful if a function fails.
///
/// @return
/// Result byte
///
uchar ds28e30_getLastResultByte(void)
{
   return last_result_byte;
}

//---------------------------------------------------------------------------
/// @internal
///
/// Sent/receive standard flow command 
///
/// @param[in] write_buf
/// Buffer with write contents (preable payload)
/// @param[in] write_len
/// Total length of data to write in 'write_buf'
/// @param[in] delay_ms
/// Delay in milliseconds after command/preable.  If == 0 then can use 
/// repeated-start to re-access the device for read of result byte. 
/// @param[in] expect_read_len
/// Expected result read length 
/// @param[out] read_buf
/// Buffer to hold data read from device. It must be at least 255 bytes long. 
/// @param[out] read_len
/// Pointer to an integer to contain the length of data read and placed in read_buf
/// Preloaded with expected read length for 1-Wire mode. If (0) but expected_read=TRUE
/// then the first byte read is the length of data to read. 
///
///  @return
///  TRUE - command successful @n
///  FALSE - command failed
///
/// @endinternal
///

static int ds28e30_ow_skip_rom(void)
{
	int i;
	unsigned char flag = false;
	
	for (i = 0; i < SEC_AUTH_RETRY; i++)	{
		flag = OWSkipROM();
		if(flag)
			return TRUE;
	}

	auth_info("%s: fail\n", __func__);
	return FALSE;	
}

int standard_cmd_flow(uchar *write_buf, int write_len, int delay_ms, int expect_read_len, uchar *read_buf, int *read_len)
{
	uchar pkt[256];
	int pkt_len = 0;
	int i;

	auth_dbg("%s: write_len(%d), delay_ms(%d), expect_read_len(%d)\n",
			__func__, write_len, delay_ms, expect_read_len);
	// int start_offset = 0;
	// Reset/presence
	// Rom COMMAND (set from select options)
	if (!ds28e30_ow_skip_rom())
		return FALSE;

	// set result byte to no response
	last_result_byte = RESULT_FAIL_COMMUNICATION;

	// Construct write block, start with XPC command
	pkt[pkt_len++] = XPC_COMMAND;

	// Add length
	pkt[pkt_len++] = write_len;

	// write (first byte will be sub-command)
	memcpy(&pkt[pkt_len], write_buf, write_len);
	pkt_len += write_len;

	//send packet to DS28E30
	for (i = 0; i < pkt_len; i++)
		write_byte(pkt[i]);

	// read two CRC bytes
	pkt[pkt_len++] = read_byte();
	pkt[pkt_len++] = read_byte();

	// check CRC16
	CRC16 = 0;
	for (i = 0; i < pkt_len; i++)
		docrc16(pkt[i]);

	// check if ROMID is populated after power up, skip CRC check if ROM_NO[Active_Device][0]==0x00.
	if (ROM_NO[Active_Device - 1][0] != 0) {
		if (CRC16 != 0xB001) {
			auth_err("%s: CRC 16 fail\n", __func__);
			return FALSE;
		}
	}

	// Send release byte, start strong pull-up
	write_byte(0xAA);

	// optional delay
	if (delay_ms)
		delayms(delay_ms);

	// turn off strong pull-up
	// OWLevel(MODE_NORMAL);

	// read FF and the length byte
	pkt[0] = read_byte();
	pkt[1] = read_byte();
	*read_len = pkt[1];

	auth_dbg("%s: pkt0(%x), pkt1(%x), read_len(%d), CRC16(%x)\n",
			__func__, pkt[0], pkt[1], *read_len, CRC16);

	// make sure there is a valid length
	if (*read_len != RESULT_FAIL_COMMUNICATION) {
		//read packet
		for (i = 0; i < *read_len + 2; i++) {
			read_buf[i] = read_byte();
			auth_dbg("%s: read_buf[%d] = %x\n",
					__func__, i, read_buf[i]);
		}
		
		// check CRC16
		CRC16 = 0;
		docrc16(*read_len);
		for (i = 0; i < (*read_len + 2); i++)
			docrc16(read_buf[i]);

		auth_dbg("%s: CRC16[%x], read_len(%d)\n",
				__func__, CRC16, *read_len);

		if (CRC16 != 0xB001) {
			auth_info("%s: CRC 16 fail\n", __func__);
			return FALSE;
		}

		if (expect_read_len != *read_len) {
			auth_info("%s: expect_read_len fail\n", __func__);
			return FALSE;
		}
	} else {
		auth_info("%s: fail\n", __func__);
		return FALSE;
	}

	// success
	return TRUE;
}

//---------------------------------------------------------------------------
/// @internal
///
/// Calculate the CRC8 of the byte value provided with the current 
/// global 'crc8' value. 
///
/// @param[in] data
/// data to compute crc8 on
///
/// @return
/// CRC8 result
/// @endinternal
//
/*
uchar calc_crc8(uchar data)
{
   int i; 

   // See Application Note 27
   crc8 = crc8 ^ data;
   for (i = 0; i < 8; ++i)
   {
      if (crc8 & 1)
         crc8 = (crc8 >> 1) ^ 0x8c;
      else
         crc8 = (crc8 >> 1);
   }

   return crc8;
}       */

//--------------------------------------------------------------------------
// Calculate a new CRC16 from the input data shorteger.  Return the current
// CRC16 and also update the global variable CRC16.
//
static short oddparity[16] = { 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };

unsigned short docrc16(unsigned short data)
{
   data = (data ^ (CRC16 & 0xff)) & 0xff;
   CRC16 >>= 8;

   if (oddparity[data & 0xf] ^ oddparity[data >> 4])
     CRC16 ^= 0xc001;

   data <<= 6;
   CRC16  ^= data;
   data <<= 1;
   CRC16   ^= data;

   return CRC16;
}
//--------------------------------------------------------------------------
/// read device ROM NO, MANID and hardware version
///
/// @return
/// TRUE read ROM ID, man_id and hardware version and confirmed @n
/// FALSE failure to read ds28e30
///
static int ds28e30_ow_read_rom(void)
{
	int i;
	unsigned char flag = false;

	for (i = 0; i < SEC_AUTH_RETRY; i++)	{
		flag = OWReadROM();
		if(flag)
			return TRUE;
	}
	auth_info("%s: fail\n", __func__);
	return FALSE;
}

int ds28e30_read_ROMNO_MANID_HardwareVersion(void)
{
	uchar i, temp = 0, buf[10], pg = 0, flag;

	ROM_NO[Active_Device - 1][0] = 0x00;
	ds28e30_ow_read_rom();      //search DS28E30
	if ((ROM_NO[Active_Device - 1][0] & 0x7F) == DS28E30_FAM) {
		for (i = 0; i < 6; i++)
			temp |= ROM_NO[Active_Device - 1][1+i];  //check if the device is power up at the first time
		if (temp == 0) {        //power up the device, then read ROMID again
			ROM_NO[Active_Device - 1][0] = 0x00;
			ds28e30_cmd_readStatus(pg, buf, MANID, HardwareVersion);  //page number=0
			ds28e30_ow_read_rom();      //read ROMID from DS28E30
			flag = ds28e30_cmd_readStatus(0x80|pg, buf, MANID, HardwareVersion);  //page number=0
			return flag;
		}
		flag = ds28e30_cmd_readStatus(0x80|pg, buf, MANID, HardwareVersion);  //page number=0
		return flag;
	}
	auth_info("%s: fail\n", __func__);
	return false;
}

//--------------------------------------------------------------------------
/// Set device ROM NO and MANID,
///
/// @param[in] setROMNO
/// ROM number of device, used in computations
/// @param[in] setMANID
/// Manufacturer number, used in computations
///
/* void ds28e30_set_ROMNO_MANID(uchar *setROMNO, uchar *setMANID)
{
   memcpy(ROM_NO,setROMNO,8);
   memcpy(MANID,setMANID,2);
}       */

//--------------------------------------------------------------------------
/// Get device ROM NO and MANID, 
///
/// @param[out] setROMNO
/// ROM number of device, used in computations
/// @param[out] setMANID
/// Manufacturer number, used in computations
///
/* void ds28e30_get_ROMNO_MANID(uchar *getROMNO, uchar *getMANID)
{
   memcpy(getROMNO,ROM_NO,8);
   memcpy(getMANID,MANID,2);
}       */

//---------------------------------------------------------------------------
/// Set public key in module.  This will be used in other helper functions. 
///
/// @param[in] pubkey_x
/// buffer for x portion of public key, must be 32 bytes 
/// @param[in] pubkey_y
/// buffer for y portion of public key, must be 32 bytes 
///
void ds28e30_setPublicKey(uchar *pubkey_x, uchar *pubkey_y)
{
	memcpy(public_key_x[Active_Device - 1], pubkey_x, 32);
	memcpy(public_key_y[Active_Device - 1], pubkey_y, 32);
}

//---------------------------------------------------------------------------
/// Set public key in module.  This will be used in other helper functions. 
///
/// @param[in] priv
/// buffer for private key, must be 32 bytes 
///
void ds28e30_setPrivateKey(uchar *priv)
{
   memcpy(private_key, priv, 32);
}

/*******************************************************************
@brief Helper function to compute Signature using the specified private key. 

@param[in] message
Messge to hash for signature verification
@param[in] msg_len
Length of message in bytes
@param[in] key
(0,1) to indicate private key A,B
@param[out] sig_r
signature portion r
@param[out] sig_s
signature portion s

@return
TRUE - signature verified @n
FALSE - signature not verified
*******************************************************************/
int	sw_computeECDSASignature(uchar *message, int msg_len,  uchar *sig_r, uchar *sig_s)
{
	int configuration;
	ucl_type_ecdsa_signature signature;

	auth_dbg("%s: called\n", __func__);
	// hook up r/s to the signature structure
	signature.r = sig_r;
	signature.s = sig_s;

	// construct configuration
	configuration = (SECP256R1 << UCL_CURVE_SHIFT) ^ (UCL_MSG_INPUT << UCL_INPUT_SHIFT) ^ (UCL_SHA256 << UCL_HASH_SHIFT);

	// create signature and return result
	return (ucl_ecdsa_signature(signature, private_key, ucl_sha256, message, msg_len, &secp256r1, configuration) == 0);
}

