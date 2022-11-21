/*
 * 1-Wire implementation for the ds28el35 chip
 *
 * Copyright (C) 2014 maximintergrated
 *
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/idr.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/err.h>
#include <linux/input.h>
#include <linux/of.h>

#include "../w1.h"
#include "../w1_int.h"
#include "../w1_family.h"
#include "./w1_ds28el35/ecdsa_software_gmp.h"

// 1-Wire commands
// delay durations
#define DELAY_EEPROM_WRITE		20
#define DELAY_KEY_GEN           	320
#define DELAY_SIGNATURE_COMPUTE 	320
#define DELAY_SHA256            	0	// delay already included in DELAY_SIGNATURE_COMPUTE

bool attached;;

// 1-Wire Memory commands
#define CMD_WRITE_READ_BUFFER   0x0F
#define CMD_LOAD_DATA           0x33
#define CMD_GENERATE_KEY_PAIR   0x3C
#define CMD_COMPUTE_READ_PAGE   0xA5
#define CMD_READ_MEMORY         0xF0
#define CMD_WRITE_MEMORY        0x55
#define CMD_READ_ADMIN          0xAA
#define CMD_SET_PROTECTION      0xC3
#define CMD_DECREMENT_COUNTER   0x69

// Selection methods
#define SELECT_SKIP     	0
#define SELECT_RESUME   	1
#define SELECT_MATCH    	2
#define SELECT_ODMATCH  	3
#define SELECT_SEARCH   	4
#define SELECT_READROM  	5
#define SELECT_ODSKIP   	6

// Family code
#define DS28EL35_FAMILY		0x40

// Lock flags for generating KeyPair
#define FLAG_LOCK        	0x07
#define FLAG_NO_LOCK     	0x00

// Public key extra bit flag
#define FLAG_PUBLIC_KEY_XTRA_BIT    0x80
#define FLAG_PUBLIC_KEY_NO_XTRA_BIT 0x00

// Release byte
#define RELEASE_BYTE     	0xAA

// Memory destinations
#define MEM_PRIVATE_KEY   	0x00
#define MEM_PUBLIC_KEY    	0x20
#define MEM_CERT_PART1    	0x40
#define MEM_CERT_PART2    	0x60
#define MEM_CHALLENGE     	0x80
#define MEM_COUNTER       	0xA0

// flags
#define FLAG_WRITE        	0x00
#define FLAG_READ         	0x0F

// Administation destinations
#define ADMIN_PAGE_PROTECTION	0x00
#define ADMIN_PUBLIC_KEY       	0x20
#define ADMIN_CERT_PART1       	0x40
#define ADMIN_CERT_PART2       	0x60
#define ADMIN_COUNTER          	0xA0
#define ADMIN_PERSONALITY      	0xE0

// Protection flags
#define PROTECT_READ   		0x80
#define PROTECT_WRITE  		0x40
#define PROTECT_EPROM  		0x20

// Object destination
#define OBJECT_PAGE0   		0x00
#define OBJECT_PAGE1   		0x01
#define OBJECT_PAGE2   		0x02
#define OBJECT_PAGE3   		0x03
#define OBJECT_KEY     		0x0C
#define OBJECT_CERT    		0x0D

// Samsung define
#define ID_MIN		0
#define ID_MAX		100
#define CO_MIN		0
#define CO_MAX		16
#define ID_DEFAULT	1
#define CO_DEFAULT	1
#define RETRY_LIMIT	5
// end customer define

#ifndef uchar
   typedef unsigned char uchar;
#endif

// Memory level functions
int write_buffer(struct w1_slave *sl, int mem_destination, uchar *data);
int read_buffer(struct w1_slave *sl, int mem_destination, uchar *data);
int load_data(struct w1_slave *sl, int mem_destination, int public_key_extra_bit);
int generate_keypair(struct w1_slave *sl, int lock);
int compute_readpage_signature(struct w1_slave *sl, int page, uchar *r, uchar *s);
int read_memorypage(struct w1_slave *sl, int segment, int page, uchar *data, int cont);
int write_memorysegment(struct w1_slave *sl, int segment, int page, uchar *data, int cont);
int read_administrativedata(struct w1_slave *sl, int admin_destination, int prot_page, uchar *data);
int set_pageprotection(struct w1_slave *sl, int protect_flags, int object, int cont);
int decrement_counter(struct w1_slave *sl);

// High level functions
BOOL read_publickey(struct w1_slave *sl);
BOOL read_authverify(struct w1_slave *sl, int page, uchar *challenge, uchar *pg_data, uchar *r, uchar *s);
BOOL verify_certificate(struct w1_slave *sl, char *cert_r, char *cert_s);

static unsigned short docrc16(unsigned short data);
static int deviceselect(struct w1_slave *sl);

static int BUGFIX = 1; // Flag to indicate fix around known bug

// misc state
static unsigned short slave_crc16;

// debug
static int ecdsa_debug = 1;
static int total_run=0, fail_run=0;

int verification = -1, id = 2, color, model, detect;	// for samsung
#ifdef CONFIG_W1_SN
char g_sn[14];
#endif

static u8 init_verify = 1;	// for inital verifying


//-----------------------------------------------------------------------------
// ------ DS28EL35 ECDSA High Level Functions
//-----------------------------------------------------------------------------

//--------------------------------------------------------------------------
/**
 * Read the device public key and save in global for later use to
 * verify signatures.
 *
 * @return
 * TRUE - Device public key read successfully @n
 * FALSE - Error reading device public key
 */
BOOL read_publickey(struct w1_slave *sl)
{
	uchar compressed_y[80], compressed_x[80];  // ARRAY SIZE CHANGE
	char compressed_xy[100];  // ARRAY SIZE CHANGE
	uchar manid[2];
	int i;
	int pos;
	int rt = 0;

	// read the public key 'x'
	if (read_administrativedata(sl, ADMIN_PUBLIC_KEY, 0, compressed_x))
	{
		rt = 1;
		dprintf("DS28EL35, get public key done\n");
		// read the compress 'y'
		if (read_administrativedata(sl, ADMIN_PERSONALITY, 0, compressed_y))
		{
			rt = 2;
			dprintf("DS28EL35, get personality done\n");
			// extract MANID
			manid[0] = compressed_y[2];
			manid[1] = compressed_y[3];

			// construct the combined compressed xy
			pos = sprintf(compressed_xy,"%02X",(compressed_y[1] & 0x80) ? 0x03 : 0x02);
			for (i = 23; i >= 0; i--)
			pos += sprintf(&compressed_xy[pos],"%02X",compressed_x[i]);
			compressed_xy[pos] = 0;

			if (ecdsa_debug)
			{
				dprintf("DS28EL35, compressed: %s\n",compressed_xy);
			}

			// Decompress Public key
			if (decompress_public_key(compressed_xy, manid)) {
				rt = 3;
				dprintf("DS28EL35, read_publickey, rt = %d\n", rt);
				return TRUE;
			}
		}
	}
	dprintf("DS28EL35, read_publickey, rt = %d\n", rt);
	return FALSE;
}

//--------------------------------------------------------------------------
/**
 * Read the device public key and save in global for later use to
 * verify signatures.
 *
 * @param[in] page
 * Page number to perform authenticate
 * @param[in] challenge
 * Pointer to byte array containing 32 bytes of random data
 * @param[out] pg_data
 * Pointer to byte array to return the 32 bytes of page data
 * @param[out] r
 * Signature r value returned in 24 byte array
 * @param[out] s
 * Signature s value returned in 24 byte array
 *
 * @return
 * TRUE - Device and page signature authenticated successfully @n
 * FALSE - Error with authentication
 */
BOOL read_authverify(struct w1_slave *sl, int page, uchar *challenge,
						uchar *pg_data, uchar *r, uchar *s)
{
	char sig_r[80], sig_s[80];
	unsigned long int m[64];
	int i,pos;
	int rt = 0;

	// read page
	if (!read_memorypage(sl, 0, page, pg_data, FALSE)) {
		rt = 1;
		goto out;
	}

	// write challenge to buffer
	if (!write_buffer(sl, MEM_CHALLENGE,challenge)) {
		rt = 2;
		goto out;
	}

	// compute and read signature
	if (!compute_readpage_signature(sl, page, r, s)) {
		rt = 3;
		goto out;
	}
	rt = 4;
	// convert signature to cstr to use with ECDSA library
	pos = 0;
	for (i = 23; i >= 0; i--)
		pos += sprintf(&sig_r[pos],"%02X",r[i]);
	sig_r[pos] = 0;
	pos = 0;
	for (i = 23; i >= 0; i--)
		pos += sprintf(&sig_s[pos],"%02X",s[i]);
	sig_s[pos] = 0;

	make_sha256_message(pg_data, challenge, page, m);
	if (ecdsa_debug)
	{
		dprintf("sig s: %s\n",sig_s);
		dprintf("sig r: %s\n",sig_r);
	}
	rt = 5;
	dprintf("DS28EL35, read_authverify, rt=%d\n",rt);
	// verify signature and return result
	return verify_ecdsa_signature((int)0x278, m, sig_r, sig_s, 1);
out:
	dprintf("DS28EL35, read_authverify, rt=%d\n",rt);
	return FALSE;
}

//--------------------------------------------------------------------------
/**
 * Read device certificate and verify using the system public key.
 * The input to the certificate is the public key of the
 * device (global) and the ROM ID (global).
 *
 * @param[in] system_PubKey_x
 * System Public key x, pointer to a character string
 * @param[in] system_PubKey_y
 * System Public key y, pointer to a character string
 * @param[out] cert_r
 * Certificate signature r read in c string format
 * @param[out] cert_s
 * Certificate signature s read in c string format
 *
 * @return
 * TRUE - Certificate generated and written @n
 * FALSE - Failed to write certificate
 */
BOOL verify_certificate(struct w1_slave *sl, char *cert_r, char *cert_s)
{
	unsigned long int m[64];
	int i;
	uchar cert_r_bytes[80], cert_s_bytes[80], cert_r_bytes_ms[80], cert_s_bytes_ms[80];
	uchar temp[80];
	char cert_r_ms[80], cert_s_ms[80];

	// make  message
	if (!make_message(m))
		return FALSE;

	// Read out the certificate from the device
	if (!read_administrativedata(sl, ADMIN_CERT_PART1, 0, cert_r_bytes))
		return FALSE;
	if (!read_administrativedata(sl, ADMIN_CERT_PART2, 0, cert_s_bytes))
		return FALSE;

	// convert certificate byte array to cstr (return values)
	convert_bytes_to_str(cert_r_bytes, 24, cert_r);
	convert_bytes_to_str(cert_s_bytes, 24, cert_s);

	// Certificate stored LSByte first, Reverse byte order for ECDSA library
	// library does MS -> LS
	memcpy(temp, cert_r_bytes, 24);
	for (i = 0; i < 24; i++)
		cert_r_bytes_ms[i] = temp[23 - i];
	memcpy(temp, cert_s_bytes, 24);
	for (i = 0; i < 24; i++)
		cert_s_bytes_ms[i] = temp[23 - i];

	// convert certificate byte array to cstr
	convert_bytes_to_str(cert_r_bytes_ms, 24, cert_r_ms);
	convert_bytes_to_str(cert_s_bytes_ms, 24, cert_s_ms);

	// verify certificate and return result
	return verify_ecdsa_signature (0x278, m, cert_r_ms, cert_s_ms, 0);
}

//-----------------------------------------------------------------------------
// ------ DS28EL35 Basic Functions
//-----------------------------------------------------------------------------

//--------------------------------------------------------------------------
/**
 * Write Butffer with provided destination and verify CRC
 * @param[in] mem_destination
 *  bit mask indicating memory destination
 * @param[in] data
 *  buffer containing data to write. Length dependent on destination
 *
 * @return
 * TRUE - data written to buffer and slave_crc16 verified @n
 * FALSE - device not detected or slave_crc16 invalid
 */
int write_buffer(struct w1_slave *sl, int mem_destination, uchar *data)
{
	int len,i;
	uchar buf[256];

	// get length from destination
	switch (mem_destination)
	{
	case MEM_CHALLENGE:
		len = 32;
		break;
	case MEM_COUNTER:
		len = 4;
		break;
	case MEM_PRIVATE_KEY:
	case MEM_PUBLIC_KEY:
	case MEM_CERT_PART1:
	case MEM_CERT_PART2:
		len = 24;
		break;
	default:
		len = 0;
		break;
	};

	// Select device
	if (!deviceselect(sl))
		  return FALSE;

	// send command and parameter
	buf[0] = CMD_WRITE_READ_BUFFER;
	buf[1] = mem_destination | FLAG_WRITE;

	// pre-calculate the slave_crc16
	slave_crc16 = 0;
	for (i = 0; i < 2; i++)
		docrc16(buf[i]);

	// Send command and parameter
	if (BUGFIX)
	{
		w1_write_8(sl->master, buf[0]);
		msleep(1);
		w1_write_8(sl->master, buf[1]);
	}
	else
		w1_write_block(sl->master, &buf[0], 2);

	// Read the slave_crc16
	w1_read_block(sl->master, &buf[0], 2);
	for (i = 0; i < 2; i++)
		docrc16(buf[i]);

	// check First slave_crc16
	if (slave_crc16 != 0xB001)
	  	return FALSE;

	// pre-compute second slave_crc16
	slave_crc16 = 0;
	for (i = 0; i < len; i++)
	  	docrc16(data[i]);

	// send the data to write
	w1_write_block(sl->master, &data[0], len);

	// Read the slave_crc16
	w1_read_block(sl->master, &buf[0], 2);
	for (i = 0; i < 2; i++)
	  	docrc16(buf[i]);

	// check Second slave_crc16
	if (slave_crc16 != 0xB001)
	  	return FALSE;

	return TRUE;
}

//--------------------------------------------------------------------------
/**
 * Read buffer and verify CRC
 * @param[in] mem_destination
 *  bit mask indicating memory destination
 * @param[out] data
 *  buffer containing data read. Length dependent on destination
 *
 * @return
 * TRUE - data read and savedd buffer and slave_crc16 verified @n
 * FALSE - device not detected or slave_crc16 invalid
 */
int read_buffer(struct w1_slave *sl, int mem_destination, uchar *data)
{
	int len,i;
	uchar buf[256];

	// get length from destination
	switch (mem_destination)
	{
	case MEM_CHALLENGE:
		len = 32;
		break;
	case MEM_COUNTER:
		len = 4;
		break;
	case MEM_PRIVATE_KEY:
	case MEM_PUBLIC_KEY:
	case MEM_CERT_PART1:
	case MEM_CERT_PART2:
		len = 24;
		break;
	default:
		len = 0;
		break;
	};

	// Select device
	if (!deviceselect(sl))
		return FALSE;

	// send command and parameter
	buf[0] = CMD_WRITE_READ_BUFFER;
	buf[1] = mem_destination | FLAG_READ;

	// pre-calculate the slave_crc16
	slave_crc16 = 0;
	for (i = 0; i < 2; i++)
		docrc16(buf[i]);

	// Send command and parameter
	if (BUGFIX)
	{
		w1_write_8(sl->master, buf[0]);
		msleep(1);
		w1_write_8(sl->master, buf[1]);
	}
	else
		w1_write_block(sl->master, &buf[0], 2);

	// Read the slave_crc16
	w1_read_block(sl->master, &buf[0], 2);
	for (i = 0; i < 2; i++)
		docrc16(buf[i]);

	// check First slave_crc16
	if (slave_crc16 != 0xB001)
		return FALSE;

	// read the data to write
	w1_read_block(sl->master, &data[0], len);
	slave_crc16 = 0;
	for (i = 0; i < len; i++)
		docrc16(data[i]);

	// Read the slave_crc16
	w1_read_block(sl->master, &buf[0], 2);
	for (i = 0; i < 2; i++)
		docrc16(buf[i]);

	// check Second slave_crc16
	if (slave_crc16 != 0xB001)
		return FALSE;

	return TRUE;
}

//--------------------------------------------------------------------------
/**
 * Copy the buffer to the destination.
 * @param[in] public_key_extra_bit
 *  Flag to indicate if if there is an extra bit (1) or not extra bit (0)
 *
 * @return
 * TRUE - Load data successful with valid slave_crc16 and CS @n
 * FALSE - device not detected, slave_crc16 invalid, or CS byte indicates failure
 */
int load_data(struct w1_slave *sl, int mem_destination, int public_key_extra_bit)
{
	int i,cs,delay;
	uchar buf[256];

	// get length from destination
	switch (mem_destination)
	{
	case MEM_COUNTER:
		delay = DELAY_EEPROM_WRITE * 2;
		break;
	case MEM_PRIVATE_KEY:
	case MEM_CERT_PART1:
	case MEM_CERT_PART2:
		delay = DELAY_EEPROM_WRITE * 6;
		break;
	case MEM_PUBLIC_KEY:
		delay = DELAY_EEPROM_WRITE * 7;
		break;
	default:
		delay = 0;
		break;
	};

	// Select device
	if (!deviceselect(sl))
		return FALSE;

	// send command and parameter
	buf[0] = CMD_LOAD_DATA;
	buf[1] = (public_key_extra_bit) ? FLAG_PUBLIC_KEY_XTRA_BIT : FLAG_PUBLIC_KEY_NO_XTRA_BIT;

	// pre-calculate the slave_crc16
	slave_crc16 = 0;
	for (i = 0; i < 2; i++)
		docrc16(buf[i]);

	// Send command and parameter
	w1_write_block(sl->master, &buf[0], 2);

	// Read the slave_crc16
	w1_read_block(sl->master, &buf[0], 2);
	for (i = 0; i < 2; i++)
		docrc16(buf[i]);

	// check First slave_crc16
	if (slave_crc16 != 0xB001)
		return FALSE;

	// Send release byte and start strong pull-up
	w1_write_8(sl->master, RELEASE_BYTE);

	// delay for EE write
	msleep(delay);

	// disable strong pullup

	// read the CS byte
	cs = w1_read_8(sl->master);

	return (cs == 0xAA);
}

//--------------------------------------------------------------------------
//
/**
 * Generate a key pair and optionally lock
 * @param[in] lock
 *  Flag to indicate if if key pair is to be locked (1) or not locked (0)
 *
 * @return
 * TRUE - Generate successful with valid slave_crc16 and CS @n
 * FALSE - device not detected, slave_crc16 invalid, or CS byte indicates failure
 */
int generate_keypair(struct w1_slave *sl, int lock)
{
	int i,cs;
	uchar buf[256];

	// Select device
	if (!deviceselect(sl))
		return FALSE;

	// send command and parameter
	buf[0] = CMD_GENERATE_KEY_PAIR;
	buf[1] = (lock)? FLAG_LOCK : FLAG_NO_LOCK;

	// pre-calculate the slave_crc16
	slave_crc16 = 0;
	for (i = 0; i < 2; i++)
		docrc16(buf[i]);

	// Send command and parameter
	w1_write_block(sl->master, &buf[0], 2);

	// Read the slave_crc16
	w1_read_block(sl->master, &buf[0], 2);
	for (i = 0; i < 2; i++)
		docrc16(buf[i]);

	// check First slave_crc16
	if (slave_crc16 != 0xB001)
		return FALSE;

	// Send release byte and start strong pull-up
	w1_write_8(sl->master, RELEASE_BYTE);

	// delay for EE write
	msleep(DELAY_EEPROM_WRITE * 20 + DELAY_KEY_GEN);

	// disable strong pullup

	// read the CS byte
	cs = w1_read_8(sl->master);

	return (cs == 0xAA);
}

//--------------------------------------------------------------------------
/**
 * Compute and read a page signature and verify CRC
 * @param[in] page
 *  Page number to perform signature.
 * @param[out] r
 *  Signature 'r' result
 * @param[out] s
 *  Signature 's' result
 *
 * @return
 * TRUE - Signature computed and read, slave_crc16 verified @n
 * FALSE - Device not detected or slave_crc16 invalid
 */
int compute_readpage_signature(struct w1_slave *sl, int page, uchar *r, uchar *s)
{
	int i,cs;
	uchar buf[256];
	int rt = 0;

	// Select device
	if (!deviceselect(sl)) {
		rt = -1;
		goto out;
	}

	// send command and parameter
	buf[0] = CMD_COMPUTE_READ_PAGE;
	buf[1] = page;

	// pre-calculate the slave_crc16
	slave_crc16 = 0;
	for (i = 0; i < 2; i++)
		docrc16(buf[i]);

	// Send command and parameter
	w1_write_block(sl->master, &buf[0], 2);

	// Read the slave_crc16
	buf[0] = w1_read_8(sl->master);

	// on second byte, start strong pull-up
	buf[1] = w1_read_8(sl->master);

	// check First slave_crc16
	for (i = 0; i < 2; i++)
		docrc16(buf[i]);
	if (slave_crc16 != 0xB001) {
		rt = -2;
		goto out;
	}

	// delay for EE write
	msleep(DELAY_SHA256 + DELAY_SIGNATURE_COMPUTE);

	// disable strong pullup

	// read the CS byte and verify
	cs = w1_read_8(sl->master);
	if (cs != 0xAA) {
		rt = -3;
		goto out;
	}

	// read the 24 bytes of 'r'
	w1_read_block(sl->master, &r[0], 24);

	// slave_crc16
	w1_read_block(sl->master, &buf[0], 2);
	slave_crc16 = 0;
	for (i = 0; i < 24; i++)
		docrc16(r[i]);
	for (i = 0; i < 2; i++)
		docrc16(buf[i]);

	// check Second slave_crc16
	if (slave_crc16 != 0xB001) {
		rt = -4;
		goto out;
	}

	// read 24 bytes of 's'
	w1_read_block(sl->master, &s[0], 24);

	// slave_crc16
	w1_read_block(sl->master, &buf[0], 2);
	slave_crc16 = 0;
	for (i = 0; i < 24; i++)
		docrc16(s[i]);
	for (i = 0; i < 2; i++)
		docrc16(buf[i]);

	// check Third slave_crc16
	if (slave_crc16 != 0xB001) {
		rt = -5;
		goto out;
	}

	return TRUE;
out:
	dprintf("DS28EL35, compute_readpage_signature, rt = %d\n", rt);
	return FALSE;
}

//--------------------------------------------------------------------------
/**
 * Read memory page starting at the provided segment number and verify CRC.
 * Optionally continue from last read.
 * @param[in] segment
 *  starting segment number (0 to 7)
 * @param[in] page
 *  Page number (0 to 3)
 * @param[out] data
 *  Buffer containing data read. Length dependent on starting segment.
 * @param[in] cont
 *  Continue flag. If (1) then continue reading from last page. Do not reselect.
 *  (0) to reselect.
 *
 * @return
 * TRUE - Data read and saved in buffer and slave_crc16 verified @n
 * FALSE - Device not detected or slave_crc16 invalid
 */
int read_memorypage(struct w1_slave *sl, int segment, int page, uchar *data, int cont)
{
	int len,i;
	uchar buf[256];

	// calculate the length
	len = (8 - segment) * 4;

	// check if not continuing from last read
	if (!cont)
	{
		// Select device
		if (!deviceselect(sl))
			return FALSE;

		// send command and parameter
		buf[0] = CMD_READ_MEMORY;
		buf[1] = (segment << 5) | page;

		// pre-calculate the slave_crc16
		slave_crc16 = 0;
		for (i = 0; i < 2; i++)
			docrc16(buf[i]);

		// Send command and parameter
		w1_write_block(sl->master, &buf[0], 2);

		// Read the slave_crc16
		w1_read_block(sl->master, &buf[0], 2);
		for (i = 0; i < 2; i++)
			docrc16(buf[i]);

		// check First slave_crc16
		if (slave_crc16 != 0xB001)
			return FALSE;
	}

	// read the data to write
	w1_read_block(sl->master, &data[0], len);
	slave_crc16 = 0;
	for (i = 0; i < len; i++)
		docrc16(data[i]);

	// Read the slave_crc16
	w1_read_block(sl->master, &buf[0], 2);
	for (i = 0; i < 2; i++)
		docrc16(buf[i]);

	// check Second slave_crc16
	if (slave_crc16 != 0xB001)
		return FALSE;

	return TRUE;
}

//--------------------------------------------------------------------------
/**
 * Write memory segment. Optionally continue from last write.
 * @param[in] segment
 *  starting segment number (0 to 7)
 * @param[in] page
 *  Page number (0 to 3)
 * @param[in] data
 *  Buffer containing data to write. Length dependent must be 4 bytes
 * @param[in] cont
 *  Continue flag. If (1) then continue writing from last segment
 *  ,do not reselect. (0) to reselect.
 *
 * @return
 * TRUE - Data written to segment and slave_crc16 verified @n
 * FALSE - Device not detected or slave_crc16 invalid
 */
int write_memorysegment(struct w1_slave *sl, int segment, int page, uchar *data, int cont)
{
	int i,cs;
	uchar buf[256];

	// check if not continuing from last read
	if (!cont)
	{
		// Select device
		if (!deviceselect(sl))
			return FALSE;

		// send command and parameter
		buf[0] = CMD_WRITE_MEMORY;
		buf[1] = (segment << 5) | page;

		// pre-calculate the slave_crc16
		slave_crc16 = 0;
		for (i = 0; i < 2; i++)
			docrc16(buf[i]);

		// Send command and parameter
		w1_write_block(sl->master, &buf[0], 2);

		// Read the slave_crc16
		w1_read_block(sl->master, &buf[0], 2);
		for (i = 0; i < 2; i++)
			docrc16(buf[i]);

		// check First slave_crc16
		if (slave_crc16 != 0xB001)
			return FALSE;
	}

	// send the data to write
	w1_write_block(sl->master, &data[0], 4);

	// pre-calculate slave_crc16
	slave_crc16 = 0;
	for (i = 0; i < 4; i++)
		docrc16(data[i]);

	// Read the slave_crc16
	w1_read_block(sl->master, &buf[0], 2);
	for (i = 0; i < 2; i++)
		docrc16(buf[i]);

	// check Second slave_crc16
	if (slave_crc16 != 0xB001)
		return FALSE;

	// Send release byte and start strong pull-up
	w1_write_8(sl->master, RELEASE_BYTE);

	// delay for EE write
	msleep(DELAY_EEPROM_WRITE);

	// disable strong pullup

	// read the CS byte
	cs = w1_read_8(sl->master);

	return (cs == 0xAA);
}


//--------------------------------------------------------------------------
/**
 * Read Administration data.
 * @param[in] admin_destination
 *  Bit mask indicating administrative memory destination
 * @param[in] prot_page
 *  Protection Page to start reading from. Only valid if destination is
 *  page protection (ADMIN_PAGE_PROTECTION)
 * @param[out] data
 *  Buffer containing data read. Length dependent on destination.
 *
 * @return
 * TRUE - Data read and saved in buffer and slave_crc16 verified @n
 * FALSE - Device not detected or slave_crc16 invalid
 */
int read_administrativedata(struct w1_slave *sl, int admin_destination, int prot_page, uchar *data)
{
	int len,i;
	uchar buf[256];
	int rt = 0;

	switch (admin_destination)
	{
	case ADMIN_PAGE_PROTECTION:
		len = 4 - prot_page;  // based on starting page
		break;
	case ADMIN_COUNTER:
	case ADMIN_PERSONALITY:
		len = 4;
		break;
	case ADMIN_PUBLIC_KEY:
	case ADMIN_CERT_PART1:
	case ADMIN_CERT_PART2:
		len = 24;
		break;
	default:
		len = 0;
		break;
	};

	// Select device
	if (!deviceselect(sl)) {
		rt = -1;
		goto out;
	}

	// send command and parameter
	buf[0] = CMD_READ_ADMIN;
	if (admin_destination == ADMIN_PAGE_PROTECTION)
		buf[1] = admin_destination | prot_page;
	else
		buf[1] = admin_destination;

	// pre-calculate the slave_crc16
	slave_crc16 = 0;
	for (i = 0; i < 2; i++)
		docrc16(buf[i]);

	// Send command and parameter
	w1_write_block(sl->master, &buf[0], 2);

	// Read the slave_crc16
	w1_read_block(sl->master, &buf[0], 2);
	for (i = 0; i < 2; i++)
		docrc16(buf[i]);

	// check First slave_crc16
	if (slave_crc16 != 0xB001) {
		rt = -2;
		goto out;
	}
	// read the data
	w1_read_block(sl->master, &data[0], len);
	slave_crc16 = 0;
	dprintf("DS28EL35 : Administration data: ");
	for (i = 0; i < len; i++) {
		dprintf("%02X ",data[i]);
		docrc16(data[i]);
	}
	dprintf("\n");

	// Read the slave_crc16
	w1_read_block(sl->master, &buf[0], 2);
	for (i = 0; i < 2; i++)
		docrc16(buf[i]);

	// check Second slave_crc16
	if (slave_crc16 != 0xB001) {
		rt = -3;
		goto out;
	}
	return TRUE;

out:
	dprintf("DS28EL35, read_administrativedata, rt = %d\n", rt);
	return FALSE;
}

//--------------------------------------------------------------------------
/**
 * Set protection on destination object and verify CRC.
 * Optionally continue from last set protection.
 * @param[in] protect_flags
 *  Bit mask indicating which protections to set.
 * @param[in] object
 *  Bit mask of the object identifier
 * @param[in] cont
 *  Continue flag. If (1) then continue writing protection. Do not reselect.
 *  (0) to reselect.
 *
 * @return
 * TRUE - Object protected and slave_crc16 verified @n
 * FALSE - Device not detected or slave_crc16 invalid
 */
int set_pageprotection(struct w1_slave *sl, int protect_flags, int object, int cont)
{
	int i,cs;
	uchar buf[256];

	if (!cont)
	{
		// Select device
		if (!deviceselect(sl))
			return FALSE;

		// send command and parameter
		buf[0] = CMD_SET_PROTECTION;
		buf[1] = protect_flags | object;

		// pre-calculate the slave_crc16
		slave_crc16 = 0;
		for (i = 0; i < 2; i++)
			docrc16(buf[i]);

		// Send command and parameter
		w1_write_block(sl->master, &buf[0], 2);

		// Read the slave_crc16
		w1_read_block(sl->master, &buf[0], 2);
		for (i = 0; i < 2; i++)
			docrc16(buf[i]);

		// check First slave_crc16
		if (slave_crc16 != 0xB001)
			return FALSE;
	}
	else
	{
		// send just parameter
		buf[0] = protect_flags | object;

		// pre-calculate the slave_crc16
		slave_crc16 = 0;
		for (i = 0; i < 1; i++)
			docrc16(buf[i]);

		// Send command and parameter
		w1_write_8(sl->master, buf[0]);

		// Read the slave_crc16
		w1_read_block(sl->master, &buf[0], 2);
		for (i = 0; i < 2; i++)
			docrc16(buf[i]);

		// check First slave_crc16
		if (slave_crc16 != 0xB001)
			return FALSE;
	}

	// Send release byte and start strong pull-up
	w1_write_8(sl->master, RELEASE_BYTE);

	// delay for EE write
	msleep(DELAY_EEPROM_WRITE);

	// disable strong pullup

	// read the CS byte
	cs = w1_read_8(sl->master);

	return (cs == 0xAA);
}

//--------------------------------------------------------------------------
/**
 * Decrement the counter
 *
 * @return
 * TRUE - Decrement the counter with valid slave_crc16 and CS @n
 * FALSE - Device not detected, slave_crc16 invalid, or CS byte indicates failure
 */
int decrement_counter(struct w1_slave *sl)
{
	int i,cs;
	uchar buf[256];

	// Select device
	if (!deviceselect(sl))
		return FALSE;

	// send command and parameter
	buf[0] = CMD_DECREMENT_COUNTER;
	buf[1] = 0xFF;

	// pre-calculate the slave_crc16
	slave_crc16 = 0;
	for (i = 0; i < 2; i++)
		docrc16(buf[i]);

	// Send command and parameter
	w1_write_block(sl->master, &buf[0], 2);

	// Read the slave_crc16
	w1_read_block(sl->master, &buf[0], 2);
	for (i = 0; i < 2; i++)
		docrc16(buf[i]);

	// check slave_crc16
	if (slave_crc16 != 0xB001)
		return FALSE;

	// Send release byte and start strong pull-up
	w1_write_8(sl->master, RELEASE_BYTE);

	// delay for EE write
	msleep(2 * DELAY_EEPROM_WRITE);

	// disable strong pullup

	// read the CS byte
	cs = w1_read_8(sl->master);

	return (cs == 0xAA);
}

//--------------------------------------------------------------------------
//  Select the DS28E35 in the global ROM_NO
//
static int deviceselect(struct w1_slave *sl)
{
	if (w1_reset_select_slave(sl) == 0)
		return TRUE;
	else
		return FALSE;
}

//--------------------------------------------------------------------------
// Calculate a new CRC16 from the input data shorteger.  Return the current
// CRC16 and also update the global variable CRC16.
//
static short oddparity[16] = { 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };

static unsigned short docrc16(unsigned short data)
{
	data = (data ^ (slave_crc16 & 0xff)) & 0xff;
	slave_crc16 >>= 8;

	if (oddparity[data & 0xf] ^ oddparity[data >> 4])
		slave_crc16 ^= 0xc001;

	data <<= 6;
	slave_crc16  ^= data;
	data <<= 1;
	slave_crc16   ^= data;

	return slave_crc16;
}


//--------------------------------------------------------------------------
//  w1_ds28el35_verifyecdsa
//
//  compare two mac data, Device mac and calculated mac and
//  returned the result
//  Returns: 0 - Success, have the same mac data in both of Device and AP.
//               else - Failed, have the different mac data or verifying sequnce failed.
//
int w1_ds28el35_verifyecdsa(struct w1_slave *sl)
{
	int rslt, rt;

	int pg;
	uchar buf[256], challenge[32], sig_r[25], sig_s[25];

	char device_cert_r[80], device_cert_s[80];

	// ----- DS28E35 Application Example
	dprintf("\n-------- DS28EL35 Verify ECDSA\n");

	rt = 0;

	// set the rom number
	set_romid((u8 *)&sl->reg_num);

	// read system public key //
	set_system_key();

	// Read deivce Public Key
	dprintf("DS28EL35: Read the device Public Key\n");
	rslt = read_publickey(sl);
	if (!rslt) {
		rt = -1;
		goto out;
	}
	dprintf("DS28EL35: %s\n",(rslt) ? " SUCCESS" : "FAIL");

	// Read and Verify Certificate
	dprintf("DS28EL35: Read and verify certificate\n");
	rslt = verify_certificate(sl, device_cert_r, device_cert_s);
	if (!rslt) {
		rt = -2;
		goto out;
	}
#ifdef DEBUG_KEY
	else
	{
		dprintf("Certificate r: %s\n",device_cert_r);
		dprintf("			s: %s\n",device_cert_s);
	}
#endif
	dprintf("DS28EL35: %s\n",(rslt) ? " SUCCESS" : "FAIL");

	// Read User page and verify Signature
	pg = 0;
	dprintf("DS28EL35: Read page %d, generate signature and verify\n", pg);
	get_random_bytes(challenge, 32);  // random challenge
	rslt = read_authverify(sl, pg, challenge, buf, sig_r, sig_s);
	if (!rslt) {
		rt = -3;
		goto out;
	}
#ifdef DEBUG_KEY
	else
	{
		dprintf("Page Data: ");
		for (i = 0; i < 32; i++)
			dprintf("%02X",buf[i]);
		dprintf("\nChallenge: ");
		for (i = 0; i < 32; i++)
			dprintf("%02X",challenge[i]);
		dprintf("\nSignature r: ");
		for (i = 0; i < 24; i++)
			dprintf("%02X",sig_r[i]);
		dprintf("\n			s: ");
		for (i = 0; i < 24; i++)
			dprintf("%02X",sig_s[i]);
		dprintf("\n");
	}
#endif
	dprintf("DS28EL35: %s\n",(rslt) ? " SUCCESS" : "FAIL");

out:
	total_run++;
	if(!rslt)	fail_run++;
	pr_info("%s:DS28EL35: %s\n",__func__, (rslt) ? " SUCCESS" : "FAIL");
	pr_info("%s: FAIL : %d TOTAL : %d\n", __func__, fail_run, total_run);
	dprintf("DS28EL35 Verify ECDSA: %s, rt=%d\n",(rt) ? "FAIL" : " SUCCESS", rt);
	dprintf("-------------------------------------------------------\n");

	return rt;
}

//--------------------------------------------------------------------------
// Take provided hex string and reverse endian and print.
//
void print_str_reverse_endian(char *str)
{
   int i;
   int len = strlen(str);

   // only print if multple of 2
   if ((len % 2) != 0)
      return;

   // print byte at a time
   for (i = (len - 1); i >= 0; i -= 2)
      dprintf("%X%X",str[i-1], str[i]);
}

static ssize_t w1_ds28el35_read_user_eeprom(struct device *device,
	struct device_attribute *attr, char *buf);

static struct device_attribute w1_read_user_eeprom_attr =
	__ATTR(read_eeprom, S_IRUGO, w1_ds28el35_read_user_eeprom, NULL);

// read 64 bytes user eeprom data
static ssize_t w1_ds28el35_read_user_eeprom(struct device *device,
	struct device_attribute *attr, char *buf)
{
	struct w1_slave *sl = dev_to_w1_slave(device);
	u8 rdbuf[64];
	int result, i, page_num;
	ssize_t c = PAGE_SIZE;

	// read page 0
	page_num=0;
	c -= snprintf(buf + PAGE_SIZE - c, c, "page %d read\n", page_num);
	result = read_memorypage(sl, 0, page_num, &rdbuf[0], 0);
	if (result == FALSE) {
		c -= snprintf(buf + PAGE_SIZE - c, c, "\npage %d read fail\n", page_num);
		return 0;
	}
	for (i = 0; i < 32; ++i)
		c -= snprintf(buf + PAGE_SIZE - c, c, "%02x ", rdbuf[i]);
	c -= snprintf(buf + PAGE_SIZE - c, c, "\npage %d read done\n", page_num);

	// read page 1
	page_num=1;
	result = read_memorypage(sl, 0, page_num, &rdbuf[32], 0);

	if (result == FALSE) {
		c -= snprintf(buf + PAGE_SIZE - c, c, "\npage %d read fail\n", page_num);
		return 0;
	}
	for (i = 32; i < 64; ++i)
		c -= snprintf(buf + PAGE_SIZE - c, c, "%02x ", rdbuf[i]);
	c -= snprintf(buf + PAGE_SIZE - c, c, "\npage %d read done\n", page_num);

	return PAGE_SIZE - c;
}

static ssize_t w1_ds28el35_check_model(struct device *device,
	struct device_attribute *attr, char *buf);

static struct device_attribute w1_check_model_attr =
	__ATTR(check_model, S_IRUGO, w1_ds28el35_check_model, NULL);

// check MODEL for cover
static ssize_t w1_ds28el35_check_model(struct device *device,
	struct device_attribute *attr, char *buf)
{
	// read cover model
	return sprintf(buf, "%d\n", model);
}

static ssize_t w1_ds28el35_check_detect(struct device *device,
	struct device_attribute *attr, char *buf);

static struct device_attribute w1_check_detect_attr =
	__ATTR(check_detect, S_IRUGO, w1_ds28el35_check_detect, NULL);

// check detect for cover
static ssize_t w1_ds28el35_check_detect(struct device *device,
	struct device_attribute *attr, char *buf)
{
	detect = w1_read_detect_state();
	// read cover detect
	return sprintf(buf, "%d\n", detect);
}

static ssize_t w1_ds28el35_check_color(struct device *device,
	struct device_attribute *attr, char *buf);

static struct device_attribute w1_check_color_attr =
	__ATTR(check_color, S_IRUGO, w1_ds28el35_check_color, NULL);

// check COLOR for cover
static ssize_t w1_ds28el35_check_color(struct device *device,
	struct device_attribute *attr, char *buf)
{
	// read cover color
	return sprintf(buf, "%d\n", color);
}

static ssize_t w1_ds28el35_check_id(struct device *device,
	struct device_attribute *attr, char *buf);

static struct device_attribute w1_check_id_attr =
	__ATTR(check_id, S_IRUGO, w1_ds28el35_check_id, NULL);

// check ID for cover
static ssize_t w1_ds28el35_check_id(struct device *device,
	struct device_attribute *attr, char *buf)
{
	// read cover id
	return sprintf(buf, "%d\n", id);
}

static ssize_t w1_ds28el35_verify_mac(struct device *device,
	struct device_attribute *attr, char *buf);

static struct device_attribute w1_verifymac_attr =
	__ATTR(verify_mac, S_IRUGO, w1_ds28el35_verify_mac, NULL);

// verified mac value between the device and AP
static ssize_t w1_ds28el35_verify_mac(struct device *device,
	struct device_attribute *attr, char *buf)
{
	struct w1_slave *sl = dev_to_w1_slave(device);
	int result;

	// verify mac
	result = w1_ds28el35_verifyecdsa(sl);
	return sprintf(buf, "%d\n", result);
}

static int w1_ds28el35_get_buffer(struct w1_slave *sl, uchar *rdbuf, int retry_limit)
{
	int ret = 0, retry = 0;
	bool rslt = false;
	while((rslt == FALSE) && (retry < retry_limit)) {
		rslt = read_memorypage(sl, 0, 0, &rdbuf[0], 0);
		if(rslt == FALSE )
			pr_info("%s : error %d\n", __func__, rslt);

		retry++;
	}

	if (rslt == TRUE)
		ret = 0;
	else
		ret = -1;
	return ret;
}

#ifdef CONFIG_W1_SN
static const int sn_cdigit[19] = {
	0x0e, 0x0d, 0x1f, 0x0b, 0x1c,
	0x12, 0x0f, 0x1e, 0x0a, 0x13,
	0x14, 0x15, 0x19, 0x16, 0x17,
	0x20, 0x1b, 0x1d, 0x11};

static bool w1_ds28e15_check_digit(const uchar *sn)
{
	int i, tmp1 = 0, tmp2 = 0;
	int cdigit = sn[3];

	for (i=4;i<10;i++)
		tmp1 += sn[i];

	tmp1 += sn[4]*5;
	tmp2 = (tmp1 * sn[9] * sn[13]) % 19;

	tmp1 = (sn[10] + sn[12]) * 3 + (sn[11] + sn[13]) * 6 + 14;

	if (cdigit == sn_cdigit[((tmp1 + tmp2) % 19)])
		return true;
	else
		return false;
}

static uchar w1_ds28e15_char_convert(uchar c)
{
	char ctable[36] = {
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'J', 'K',
	'L', 'M', 'N', 'P', 'Q', 'R', 'S', 'T', 'V', 'W',
	'X', 'Y', 'Z', 'I', 'O', 'U'};

	return ctable[c];
}

static void w1_ds28e15_slave_sn(const uchar *rdbuf)
{
	int i;
	u8 sn[15];

	sn[14] = 0;

	if (w1_ds28e15_check_digit(&rdbuf[4])) {
		for (i = 0 ; i < 14 ; i++)
			sn[i] = w1_ds28e15_char_convert(rdbuf[i+4]);

		for (i = 0 ; i < 14 ; i++)
			g_sn[i] = sn[13 - i];
	} else {
		pr_info("%s: sn is not good %s\n", __func__, sn);
	}
}
#endif

static void w1_ds28el35_update_slave_info(struct w1_slave *sl) {
	u8 rdbuf[32];
	int ret, retry = 0;

	ret = w1_ds28el35_get_buffer(sl, &rdbuf[0], 10);
	if(ret != 0)
		pr_info("%s : fail to get buffer %d\n", __func__, ret);

	while((rdbuf[0] > sl->id_max) || (rdbuf[1] > sl->color_max)) {
		printk(KERN_ERR "%s: out of range : %d %d \n", __func__, rdbuf[0], rdbuf[1]);

		if(retry > 10) {
			printk(KERN_ERR "%s: out of range over 10 times.\n", __func__);

			pr_info("%s: Change ID(%d) & Color(%d) to Default Value(%d, %d)",
				__func__, rdbuf[0], rdbuf[1], ID_DEFAULT, CO_DEFAULT);
			rdbuf[0] = ID_DEFAULT;
			rdbuf[1] = CO_DEFAULT;

			break;
		}
		ret = w1_ds28el35_get_buffer(sl, &rdbuf[0], 10);
		if(ret != 0)
			pr_info("%s : fail to get buffer %d\n", __func__, ret);

		retry++;
	}

	//read page 0, ID is 0 ~ 7 bit(rdbuf[0])
	id = rdbuf[0];

	// read page 0, COLOR is 8 ~ 15 bit(rdbuf[1])
	color = rdbuf[1];

	// read page 0, MODEL is 24 ~ 31 bit(rdbuf[3])
	model = rdbuf[3];

	attached = true;

	pr_info("%s Read Model(%d) & ID(%d) & Color(%d) & Verification State(%d)\n",
			__func__, model, id, color, verification);
#ifdef CONFIG_W1_SN
	w1_ds28e15_slave_sn(&rdbuf[0]);
#endif
}

static int w1_parse_dt(struct w1_slave *sl)
{
	struct device_node* np;

	np = of_find_node_by_path("/soc/ds28el35");
	if (!np) {
		pr_err("%s: get ds28e15 node failed\n", __func__);
		return -ENODEV;
	}
	of_property_read_u32(np, "ds28el35,id-min", &sl->id_min);
	of_property_read_u32(np, "ds28el35,id-max", &sl->id_max);
	of_property_read_u32(np, "ds28el35,id-default", &sl->id_default);
	of_property_read_u32(np, "ds28el35,color-min", &sl->color_min);
	of_property_read_u32(np, "ds28el35,color-max", &sl->color_max);
	of_property_read_u32(np, "ds28el35,color-default", &sl->color_default);
	pr_info("%s : id min[%d] max[%d] default[%d]\n", __func__,
			sl->id_min, sl->id_max, sl->id_default);
	pr_info("%s : color min[%d] max[%d] default[%d]\n", __func__,
			sl->color_min, sl->color_max, sl->color_default);

	return 0;
}

static void w1_set_default_range(struct w1_slave *sl)
{
	pr_info("%s : get default range\n", __func__);

	sl->id_min = ID_MIN;
	sl->id_max = ID_MAX;
	sl->id_default = ID_DEFAULT;
	sl->color_min = CO_MIN;
	sl->color_max = CO_MAX;
	sl->color_default = CO_DEFAULT;

	pr_info("%s : id min[%d] max[%d] default[%d]\n", __func__,
			sl->id_min, sl->id_max, sl->id_default);
	pr_info("%s : color min[%d] max[%d] default[%d]\n", __func__,
			sl->color_min, sl->color_max, sl->color_default);
}

//--------------------------------------------------------------------------
// DS28EL35 device driver function
//

static int w1_ds28el35_add_slave(struct w1_slave *sl)
{
	int err = 0;

	printk(KERN_ERR "\nw1_ds28el35_add_slave start\n");

	err = w1_parse_dt(sl);
	if (err) {
		printk(KERN_ERR "%s: w1_parse_dt error\n", __func__);
		w1_set_default_range(sl);
	}

	err = device_create_file(&sl->dev, &w1_read_user_eeprom_attr);
	if (err) {
		device_remove_file(&sl->dev, &w1_read_user_eeprom_attr);
		printk(KERN_ERR "%s: w1_read_user_eeprom_attr error\n", __func__);
		return err;
	}

	err = device_create_file(&sl->dev, &w1_verifymac_attr);
	if (err) {
		device_remove_file(&sl->dev, &w1_verifymac_attr);
		printk(KERN_ERR "%s: w1_verifymac_attr error\n", __func__);
		return err;
	}

	err = device_create_file(&sl->dev, &w1_check_id_attr);
	if (err) {
		device_remove_file(&sl->dev, &w1_check_id_attr);
		printk(KERN_ERR "%s: w1_check_id_attr error\n", __func__);
		return err;
	}

	err = device_create_file(&sl->dev, &w1_check_color_attr);
	if (err) {
		device_remove_file(&sl->dev, &w1_check_color_attr);
		printk(KERN_ERR "%s: w1_check_color_attr error\n", __func__);
		return err;
	}

	err = device_create_file(&sl->dev, &w1_check_model_attr);
	if (err) {
		device_remove_file(&sl->dev, &w1_check_model_attr);
		printk(KERN_ERR "%s: w1_check_model_attr error\n", __func__);
		return err;
	}

	err = device_create_file(&sl->dev, &w1_check_detect_attr);
	if (err) {
		device_remove_file(&sl->dev, &w1_check_detect_attr);
		printk(KERN_ERR "%s: w1_check_detect_attr error\n", __func__);
		return err;
	}

	if (init_verify) {
		err = w1_ds28el35_verifyecdsa(sl);
		verification = err;
		printk(KERN_ERR "w1_ds28el35_verifyecdsa\n");
	}
	if(!verification)
		w1_ds28el35_update_slave_info(sl);

	printk(KERN_ERR "w1_ds28el35_add_slave end, err=%d\n", err);
	return err;
}

static void w1_ds28el35_remove_slave(struct w1_slave *sl)
{
	device_remove_file(&sl->dev, &w1_read_user_eeprom_attr);
	device_remove_file(&sl->dev, &w1_verifymac_attr);
	device_remove_file(&sl->dev, &w1_check_id_attr);
	device_remove_file(&sl->dev, &w1_check_color_attr);
	device_remove_file(&sl->dev, &w1_check_model_attr);
	device_remove_file(&sl->dev, &w1_check_detect_attr);

	verification = -1;
	attached = false;
	printk(KERN_ERR "\nw1_ds28el35_remove_slave\n");
}

static struct w1_family_ops w1_ds28el35_fops = {
	.add_slave      = w1_ds28el35_add_slave,
	.remove_slave   = w1_ds28el35_remove_slave,
};

static struct w1_family w1_ds28el35_family = {
	.fid = W1_FAMILY_DS28EL35,
	.fops = &w1_ds28el35_fops,
};

static int __init w1_ds28el35_init(void)
{
	return w1_register_family(&w1_ds28el35_family);
}

static void __exit w1_ds28el35_exit(void)
{
	w1_unregister_family(&w1_ds28el35_family);
}

module_init(w1_ds28el35_init);
module_exit(w1_ds28el35_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("TaiEup Kim <clark.kim@maximintegrated.com>");
MODULE_DESCRIPTION("1-wire Driver for Maxim/Dallas DS23EL35 DeepCover Secure Authenticator IC");
