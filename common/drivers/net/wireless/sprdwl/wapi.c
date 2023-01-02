/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Filename : wapi.c
 * Abstract : This file is a implementation of WAPI decryption
 *            and encryption
 *
 * Authors      :
 * Wenjie.Zhang <Wenjie.Zhang@spreadtrum.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "cfg80211.h"
#include "wapi.h"

#define KEYID_LEN 1
#define RESERVD_LEN 1
#define PN_LEN 16
#define MIC_LEN 16

/* little order,4, 5, 6, 11, 12, 13 should be set 0 */
const unsigned short frame_cntl_mask = 0x8FC7;
/* little order,bit 4~15 should be set 0 */
const unsigned short seq_cntl_mask = 0x0F00;

#define SHA256_BLOCK_SIZE 64
#define SHA256_DIGEST_SIZE 32

#define BYTES_PER_WORD  4
#define BYTE_LEN        8
#define WORD_LEN        (BYTE_LEN * BYTES_PER_WORD)
#define TEXT_LEN        128
#define MK_LEN          (TEXT_LEN / WORD_LEN)
#define RK_LEN          32
#define TEXT_BYTES      (TEXT_LEN / BYTE_LEN)

#define CK_INCREMENT    7
#define KEY_MULTIPLIER  0x80040100
#define TEXT_MULTIPLIER 0xa0202080
#define FK_PARAMETER_0  0xa3b1bac6
#define FK_PARAMETER_1  0x56aa3350
#define FK_PARAMETER_2  0x677d9197
#define FK_PARAMETER_3  0xb27022dc

static const unsigned char s_box[] = {
	0xd6, 0x90, 0xe9, 0xfe, 0xcc, 0xe1, 0x3d, 0xb7,
	0x16, 0xb6, 0x14, 0xc2, 0x28, 0xfb, 0x2c, 0x05,
	0x2b, 0x67, 0x9a, 0x76, 0x2a, 0xbe, 0x04, 0xc3,
	0xaa, 0x44, 0x13, 0x26, 0x49, 0x86, 0x06, 0x99,
	0x9c, 0x42, 0x50, 0xf4, 0x91, 0xef, 0x98, 0x7a,
	0x33, 0x54, 0x0b, 0x43, 0xed, 0xcf, 0xac, 0x62,
	0xe4, 0xb3, 0x1c, 0xa9, 0xc9, 0x08, 0xe8, 0x95,
	0x80, 0xdf, 0x94, 0xfa, 0x75, 0x8f, 0x3f, 0xa6,
	0x47, 0x07, 0xa7, 0xfc, 0xf3, 0x73, 0x17, 0xba,
	0x83, 0x59, 0x3c, 0x19, 0xe6, 0x85, 0x4f, 0xa8,
	0x68, 0x6b, 0x81, 0xb2, 0x71, 0x64, 0xda, 0x8b,
	0xf8, 0xeb, 0x0f, 0x4b, 0x70, 0x56, 0x9d, 0x35,
	0x1e, 0x24, 0x0e, 0x5e, 0x63, 0x58, 0xd1, 0xa2,
	0x25, 0x22, 0x7c, 0x3b, 0x01, 0x21, 0x78, 0x87,
	0xd4, 0x00, 0x46, 0x57, 0x9f, 0xd3, 0x27, 0x52,
	0x4c, 0x36, 0x02, 0xe7, 0xa0, 0xc4, 0xc8, 0x9e,
	0xea, 0xbf, 0x8a, 0xd2, 0x40, 0xc7, 0x38, 0xb5,
	0xa3, 0xf7, 0xf2, 0xce, 0xf9, 0x61, 0x15, 0xa1,
	0xe0, 0xae, 0x5d, 0xa4, 0x9b, 0x34, 0x1a, 0x55,
	0xad, 0x93, 0x32, 0x30, 0xf5, 0x8c, 0xb1, 0xe3,
	0x1d, 0xf6, 0xe2, 0x2e, 0x82, 0x66, 0xca, 0x60,
	0xc0, 0x29, 0x23, 0xab, 0x0d, 0x53, 0x4e, 0x6f,
	0xd5, 0xdb, 0x37, 0x45, 0xde, 0xfd, 0x8e, 0x2f,
	0x03, 0xff, 0x6a, 0x72, 0x6d, 0x6c, 0x5b, 0x51,
	0x8d, 0x1b, 0xaf, 0x92, 0xbb, 0xdd, 0xbc, 0x7f,
	0x11, 0xd9, 0x5c, 0x41, 0x1f, 0x10, 0x5a, 0xd8,
	0x0a, 0xc1, 0x31, 0x88, 0xa5, 0xcd, 0x7b, 0xbd,
	0x2d, 0x74, 0xd0, 0x12, 0xb8, 0xe5, 0xb4, 0xb0,
	0x89, 0x69, 0x97, 0x4a, 0x0c, 0x96, 0x77, 0x7e,
	0x65, 0xb9, 0xf1, 0x09, 0xc5, 0x6e, 0xc6, 0x84,
	0x18, 0xf0, 0x7d, 0xec, 0x3a, 0xdc, 0x4d, 0x20,
	0x79, 0xee, 0x5f, 0x3e, 0xd7, 0xcb, 0x39, 0x48
};

static const unsigned int fk_parameter[] = { FK_PARAMETER_0, FK_PARAMETER_1,
	FK_PARAMETER_2, FK_PARAMETER_3
};

static const unsigned char s_xstate[] = {
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,	/* 0x00-0x0F */
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,	/* 0x10-0x1F */
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,	/* 0x20-0x2F */
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,	/* 0x30-0x3F */
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,	/* 0x40-0x4F */
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,	/* 0x50-0x5F */
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,	/* 0x60-0x6F */
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,	/* 0x70-0x7F */
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,	/* 0x80-0x8F */
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,	/* 0x90-0x9F */
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,	/* 0xA0-0xAF */
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,	/* 0xB0-0xBF */
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,	/* 0xC0-0xCF */
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,	/* 0xD0-0xDF */
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,	/* 0xE0-0xEF */
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0	/* 0xF0-0xFF */
};

static const unsigned int g_nextinputtable[RK_LEN] = {
	0x00070e15, 0x1c232a31, 0x383f464d, 0x545b6269,
	0x70777e85, 0x8c939aa1, 0xa8afb6bd, 0xc4cbd2d9,
	0xe0e7eef5, 0xfc030a11, 0x181f262d, 0x343b4249,
	0x50575e65, 0x6c737a81, 0x888f969d, 0xa4abb2b9,
	0xc0c7ced5, 0xdce3eaf1, 0xf8ff060d, 0x141b2229,
	0x30373e45, 0x4c535a61, 0x686f767d, 0x848b9299,
	0xa0a7aeb5, 0xbcc3cad1, 0xd8dfe6ed, 0xf4fb0209,
	0x10171e25, 0x2c333a41, 0x484f565d, 0x646b7279
};

static const unsigned int cipherdataidx[MK_LEN][MK_LEN] = {
	{3, 2, 1, 0},
	{0, 3, 2, 1},
	{1, 0, 3, 2},
	{2, 1, 0, 3}
};

#define PARITY_MACRO(value) (s_xstate[(value) >> 24]			\
				^ s_xstate[((value) >> 16) & 0xFF]	\
				^ s_xstate[((value) >> 8) & 0xFF]	\
				^ s_xstate[(value) & 0xFF])
#define XOR_MACRO(A, B) ((A) ^ (B))
#define L_TRANSFORM_MACRO(word, key) multiplycircular(word,		\
				key ? KEY_MULTIPLIER : TEXT_MULTIPLIER)

static unsigned int t_transform(unsigned int word)
{
	unsigned int j;
	unsigned int new_word;
	int offset = 0;

	new_word = 0;
	for (j = 0; j < MK_LEN; j++) {
		new_word = (new_word << BYTE_LEN);
		offset =
		    ((unsigned int)(word >> (WORD_LEN - BYTE_LEN))) &
		    ((unsigned int)((1 << BYTE_LEN) - 1));
		new_word = new_word | (unsigned int)s_box[offset];
		word = (word << BYTE_LEN);
	}
	return new_word;
}

static unsigned int multiplycircular(unsigned int word, unsigned int basis)
{
	unsigned int new_word;
	unsigned int i;

	new_word = 0;

	for (i = 0; i < WORD_LEN; i++) {
		new_word = (new_word << 1) | PARITY_MACRO(word & basis);

		basis = (basis >> 1) | ((basis & 1) << (WORD_LEN - 1));
	}
	return new_word;
}

static unsigned int iterate(bool key, unsigned int next_input,
			    unsigned int *cipher_text, unsigned int curidx)
{
	unsigned int new_state;

	new_state = next_input;
	new_state = XOR_MACRO(new_state, cipher_text[cipherdataidx[curidx][0]]);
	new_state = XOR_MACRO(new_state, cipher_text[cipherdataidx[curidx][1]]);
	new_state = XOR_MACRO(new_state, cipher_text[cipherdataidx[curidx][2]]);
	new_state = L_TRANSFORM_MACRO(t_transform(new_state), key);
	new_state = XOR_MACRO(new_state, cipher_text[cipherdataidx[curidx][3]]);

	cipher_text[curidx] = new_state;

	return new_state;
}

static void calculateenkey(unsigned char *key, unsigned int *key_store)
{
	unsigned int cipher_text[MK_LEN];

	unsigned int next, i, j, next_input;

	for (j = 0; j < MK_LEN; j++) {
		next = 0;
		for (i = 0; i < BYTES_PER_WORD; i++) {
			next = (next << BYTE_LEN);
			next = next | key[(j << 2) + i];
		}

		cipher_text[j] = XOR_MACRO(next, fk_parameter[j]);
	}

	for (i = 0; i < RK_LEN; i++) {
		next_input = g_nextinputtable[i];

		key_store[i] =
		    iterate(true, next_input, cipher_text, i & (MK_LEN - 1));
	}
}

static void SMS4_Run(unsigned int *key_store, unsigned char *plaintext,
		     unsigned char *ciphertext)
{
	unsigned int i, j;
	unsigned int next;
	unsigned int next_input;
	unsigned int plain_text[MK_LEN];

	for (j = 0; j < MK_LEN; j++) {
		next = 0;
		for (i = 0; i < BYTES_PER_WORD; i++) {
			next = (next << BYTE_LEN);
			next = next | plaintext[(j << 2) + i];
		}
		plain_text[j] = next;
	}

	for (i = 0; i < RK_LEN; i++) {
		next_input = key_store[i];
		(void)iterate(false, next_input, plain_text, i & (MK_LEN - 1));
	}

	for (j = 0; j < MK_LEN; j++) {
		next = plain_text[(MK_LEN - 1) - j];
		for (i = 0; i < BYTES_PER_WORD; i++) {
			ciphertext[(j << 2) + i] =
			    (unsigned char)((next >> (WORD_LEN - BYTE_LEN)) &
					    ((1 << BYTE_LEN) - 1));
			next = (next << BYTE_LEN);
		}
	}
}

void WapiCryptoSms4(unsigned char *iv, unsigned char *key, unsigned char *input,
		    unsigned int length,
		    unsigned char *mic, unsigned char *output)
{
	unsigned int i;
	unsigned char sms4Output[TEXT_BYTES];
	unsigned char tmp_data[TEXT_BYTES];

	unsigned int key_store[RK_LEN];

	unsigned int j = 0;
	unsigned char *p[2];

	p[0] = sms4Output;
	p[1] = tmp_data;

	memcpy(tmp_data, iv, TEXT_BYTES);

	calculateenkey(key, key_store);

	for (i = 0; i < length; i++) {
		if ((i & (TEXT_BYTES - 1)) == 0) {
			SMS4_Run(key_store, p[1 - j], p[j]);

			j = 1 - j;
		}

		if (i < (length - 16))
			output[i] = input[i] ^ p[1 - j][i & (TEXT_BYTES - 1)];
		else
			output[i] =
			    mic[i - length + 16] ^ p[1 -
						     j][i & (TEXT_BYTES - 1)];
	}
}

void WapiCryptoSms4Mic(unsigned char *iv, unsigned char *key,
		       unsigned char *header,
		       unsigned int headerlength, unsigned char *input,
		       unsigned int datalength, unsigned char *mic)
{
	unsigned int i, j = 0, totallength;
	unsigned char sms4Output[TEXT_BYTES], sms4Input[TEXT_BYTES];
	unsigned int tmp_headerlength = 0;
	unsigned int tmp_datalenth = 0;

	unsigned int header_cnt = 0;
	unsigned int header0_cnt = 0;
	unsigned int data_cnt = 0;
	unsigned int data0_cnt = 0;

	unsigned int key_store[RK_LEN];

	memcpy(sms4Input, iv, TEXT_BYTES);

	totallength = headerlength + datalength;
	tmp_headerlength =
	    ((headerlength & (TEXT_BYTES - 1)) ==
	     0) ? 0 : (TEXT_BYTES - (headerlength & (TEXT_BYTES - 1)));
	tmp_datalenth =
	    ((datalength & (TEXT_BYTES - 1)) ==
	     0) ? 0 : (TEXT_BYTES - (datalength & (TEXT_BYTES - 1)));

	totallength += tmp_headerlength;
	totallength += tmp_datalenth;

	calculateenkey(key, key_store);

	for (i = 0; i < totallength; i++) {
		if ((i & (TEXT_BYTES - 1)) == 0)
			SMS4_Run(key_store, sms4Input, sms4Output);

		if ((datalength == 0) && (headerlength == 0)) {
			sms4Input[i & (TEXT_BYTES - 1)] =
			    0 ^ sms4Output[i & (TEXT_BYTES - 1)];
			data0_cnt++;
		} else if ((headerlength == 0) && (tmp_headerlength == 0)) {
			sms4Input[i & (TEXT_BYTES - 1)] =
			    input[j] ^ sms4Output[i & (TEXT_BYTES - 1)];
			j++;
			datalength--;
			data_cnt++;
		} else if (headerlength == 0) {
			sms4Input[i & (TEXT_BYTES - 1)] =
			    0 ^ sms4Output[i & (TEXT_BYTES - 1)];
			tmp_headerlength--;
			header0_cnt++;
		} else {
			sms4Input[i & (TEXT_BYTES - 1)] =
			    header[i] ^ sms4Output[i & (TEXT_BYTES - 1)];
			headerlength--;
			header_cnt++;
		}
	}

	SMS4_Run(key_store, sms4Input, mic);
}

unsigned short wlan_tx_wapi_encryption(struct sprdwl_priv *priv,
				       unsigned char *data,
				       unsigned short len,
				       unsigned char *ouput_buf)
{
	unsigned short offset = 0;
	bool qos_in = false;
	bool valid_addr4 = true;

	unsigned short eth_type = 0;
	unsigned char snap_hdr[8] = { 0 };
	unsigned char *data_pos = data + 14;
	unsigned short data_len = len;
	unsigned char snap_backup[8] = { 0 };
	unsigned char arp_backup[6] = { 0 };
	unsigned char snap_flag = 0;
	unsigned char arp_flag = 0;

	unsigned char ptk_header[36] = { 0 };
	unsigned short ptk_headr_len = 32;
	unsigned char *p_ptk_header = ptk_header;
	unsigned char *p_outputdata = ouput_buf;
	unsigned char data_mic[16] = { 0 };

	unsigned char *iv = inc_wapi_pairwise_key_txrsc(priv);
	unsigned char keyid = priv->key_index[PAIRWISE];

	int i = 0;

	/* save frame cntl */
	*p_ptk_header = 8;
	*(p_ptk_header + 1) = 65;

	if (*p_ptk_header & 0x80) {
		qos_in = true;
		/* add qos len 2 byte */
		ptk_headr_len += 2;
	}

	/* valid addr4 in case:ToDS==1 && FromDS==1 */
	if ((*(p_ptk_header + 1) & 0x03) != 0x03)
		valid_addr4 = false;

	p_ptk_header += 2;
	offset += 2;

	/* jump over duration id */
	offset += 2;

	/* save addr1 addr2 */
	memcpy(p_ptk_header, priv->bssid, 6);
	memcpy(p_ptk_header + 6, data + 6, 6);
	p_ptk_header += 12;
	offset += 12;

	/* save seq cntl */
	*p_ptk_header = 0;
	*(p_ptk_header + 1) = 0;
	p_ptk_header += 2;

	/* save addr3 */
	memcpy(p_ptk_header, data, 6);
	p_ptk_header += 6;
	offset += 6;

	/* save addr4 */
	memset(p_ptk_header, 0x00, 6);
	p_ptk_header += 6;

	/* jump seq cntl */
	offset += 2;

	/* save qos */
	/*if(qos_in)
	{
		memcpy(p_ptk_header,&header[offset],2);
		p_ptk_header += 2;
		offset       += 2;
	}
	*/

	/* save keyid */
	*p_ptk_header = keyid;
	p_ptk_header++;

	/* reserved */
	*p_ptk_header = 0x00;
	p_ptk_header++;

	eth_type =
	    ((*(data + ETH_PKT_TYPE_OFFSET) << 8) |
	     *(data + ETH_PKT_TYPE_OFFSET + 1));
	if ((eth_type == ARP_TYPE) || (eth_type == IP_TYPE) ||
	    (eth_type == ONE_X_TYPE) || (eth_type == VLAN_TYPE) ||
	    (eth_type == WAPI_TYPE) || (eth_type == IPV6_TYPE) ||
	    (eth_type == LLTD_TYPE)) {
		snap_hdr[0] = 0xAA;
		snap_hdr[1] = 0xAA;
		snap_hdr[2] = 0x03;
		snap_hdr[3] = 0x00;
		snap_hdr[4] = 0x00;
		snap_hdr[5] = 0x00;
		snap_hdr[6] = *(data + 12);
		snap_hdr[7] = *(data + 13);
		/* An ARP request/response frame has to be dissected to modify*/
		/* MAC address, for the host interface. MAC layer acts as an  */
		/* interface to the packets from Etherent and WLAN and takes  */
		/* responsibility of ensuring proper interfacing.             */
		/* The source MAC address is modified only if the packet is an*/
		/* ARP Request or a Response. The appropriate bytes are checke*/
		/* Type field (2 bytes): ARP Request (1) or an ARP Response(2)*/
		if (eth_type == ARP_TYPE) {
			if ((*(data + 20) == 0x00) &&
			    (*(data + 21) == 0x02 || *(data + 21) == 0x01)) {
				/* Set Address2 field with source address */
				memcpy(arp_backup, data + 22, 6);
				arp_flag = 1;
				memcpy((data + 22), data + 6, 6);
			}
		}

		/* Set the data length parameter to the MAC data length only */
		/* not include headers)                                      */
		data_pos = data + 6;
		data_len = len + 8;
		memcpy(snap_backup, data_pos, sizeof(snap_hdr));
		snap_flag = 1;
		memcpy(data_pos, snap_hdr, sizeof(snap_hdr));
	} else {
		data_len = len;
		data_pos = data + 14;
	}

	/* save data len */
	*p_ptk_header = (data_len >> 8);
	*(p_ptk_header + 1) = data_len & 0xFF;

	/* calc mic */
	WapiCryptoSms4Mic(iv,
			  mget_wapi_pairwise_mic_key(priv, keyid),
			  ptk_header, ptk_headr_len, data_pos, data_len,
			  data_mic);

	/* add mic to data */
	data_len += 16;

	/* encryption data(inclue mic) & save keyid & iv */
	WapiCryptoSms4(iv,
		       mget_wapi_pairwise_pkt_key(priv, keyid),
		       data_pos, data_len, data_mic, p_outputdata + 1 + 1 + 16);
	if (snap_flag)
		memcpy(data_pos, snap_backup, sizeof(snap_hdr));

	if (arp_flag)
		memcpy(data + 22, arp_backup, 6);

	*p_outputdata = keyid;
	*(p_outputdata + 1) = 0x00;
	p_outputdata += 2;

	for (i = 15; i >= 0; i--) {
		*p_outputdata = iv[i];
		p_outputdata++;
	}

	return data_len + 1 + 1 + 16;
}

unsigned short wlan_rx_wapi_decryption(struct sprdwl_priv *priv,
				       unsigned char *input_ptk,
				       unsigned short header_len,
				       unsigned short data_len,
				       unsigned char *output_buf)
{
	unsigned short offset = 0;
	bool qos_in = false;
	bool valid_addr4 = true;
	bool is_group_ptk = false;

	unsigned char ptk_header[36] = { 0 };
	unsigned short ptk_headr_len = 32;
	unsigned char *p_ptk_header = ptk_header;
	unsigned char data_mic[16] = { 0 };
	unsigned char calc_data_mic[16] = { 0 };
	unsigned char iv[16] = { 0 };
	unsigned char keyid = { 0 };

	unsigned short ral_data_len = 0;
	unsigned short encryp_data_len = 0;

	int i = 0;

	/* save calc mic header */

	/* save frame cntl */
	*p_ptk_header = input_ptk[offset] & (frame_cntl_mask >> 8);
	*(p_ptk_header + 1) = input_ptk[offset + 1] & (frame_cntl_mask & 0xFF);

	if (*p_ptk_header & 0x80) {
		qos_in = true;
		/* add qos len 2 byte */
		ptk_headr_len += 2;
	}

	/* valid addr4 in case:ToDS==1 && FromDS==1 */
	if ((*(p_ptk_header + 1) & 0x03) != 0x03)
		valid_addr4 = false;

	p_ptk_header += 2;
	offset += 2;

	/* jump over duration id */
	offset += 2;

	/* save addr1 addr2 */
	memcpy(p_ptk_header, &input_ptk[offset], 12);
	is_group_ptk = is_group(p_ptk_header);
	p_ptk_header += 12;
	offset += 12;

	/* save seq cntl */
	*p_ptk_header = input_ptk[offset + 6] & (seq_cntl_mask >> 8);
	*(p_ptk_header + 1) =
	    input_ptk[offset + 6 + 1] & (seq_cntl_mask & 0xFF);
	p_ptk_header += 2;

	/* save addr3 */
	memcpy(p_ptk_header, &input_ptk[offset], 6);
	p_ptk_header += 6;
	offset += 6;

	/* save addr4 */
	if (valid_addr4) {
		memcpy(p_ptk_header, &input_ptk[offset], 6);
		p_ptk_header += 6;
		offset += 6;
	} else {
		memset(p_ptk_header, 0x00, 6);
		p_ptk_header += 6;
	}

	/* jump seq cntl */
	offset += 2;

	/* save qos */
	if (qos_in) {
		memcpy(p_ptk_header, &input_ptk[offset], 2);
		p_ptk_header += 2;
		offset += 2;

		/* mac h/w offset 2 byte to multiple of 4 */
		offset += 2;
	}

	/* save keyid */
	*p_ptk_header = input_ptk[offset];
	keyid = input_ptk[offset];
	p_ptk_header++;
	offset++;

	/* reserved */
	*p_ptk_header = input_ptk[offset];
	p_ptk_header++;
	offset++;

	/* save data len */
	encryp_data_len = data_len - KEYID_LEN - RESERVD_LEN - PN_LEN;
	ral_data_len = data_len - KEYID_LEN - RESERVD_LEN - PN_LEN - MIC_LEN;
	*p_ptk_header = (ral_data_len >> 8);
	*(p_ptk_header + 1) = ral_data_len & 0xFF;

	/* save calc mic header over */

	/* save iv */
	for (i = 15; i >= 0; i--) {
		iv[i] = input_ptk[offset];
		offset++;
	}

	/* add adjust here,later... */
	if (is_group_ptk) {
		/*if( (iv[15] & 0x01) == 0x00 )
		{
			return 0;
		}
		*/
	} else {
		if ((iv[15] & 0x01) != 0x01)
			return 0;
	}

	/* decryption */
	if (is_group_ptk) {
		WapiCryptoSms4(iv,
			       mget_wapi_group_pkt_key(priv, keyid),
			       (input_ptk + header_len + KEYID_LEN +
				RESERVD_LEN + PN_LEN), encryp_data_len,
			       (input_ptk + header_len + KEYID_LEN +
				RESERVD_LEN + PN_LEN + encryp_data_len - 16),
			       output_buf);
	} else {
		WapiCryptoSms4(iv,
			       mget_wapi_pairwise_pkt_key(priv, keyid),
			       (input_ptk + header_len + KEYID_LEN +
				RESERVD_LEN + PN_LEN), encryp_data_len,
			       (input_ptk + header_len + KEYID_LEN +
				RESERVD_LEN + PN_LEN + encryp_data_len - 16),
			       output_buf);
	}
	memcpy(data_mic, output_buf + ral_data_len, MIC_LEN);

	/* calc mic */
	if (is_group_ptk) {
		WapiCryptoSms4Mic(iv,
				  mget_wapi_group_mic_key(priv, keyid),
				  ptk_header, ptk_headr_len,
				  (output_buf), ral_data_len, calc_data_mic);
	} else {
		WapiCryptoSms4Mic(iv,
				  mget_wapi_pairwise_mic_key(priv, keyid),
				  ptk_header, ptk_headr_len,
				  (output_buf), ral_data_len, calc_data_mic);
	}

	if (memcmp(calc_data_mic, data_mic, MIC_LEN) != 0)
		return 0;
	else
		return ral_data_len;
}
