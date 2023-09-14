#ifndef __SPRD_TKS_H
#define __SPRD_TKS_H

#include <linux/types.h>

enum key_type {
	KEY_AES_128,
	KEY_AES_256,
	KEY_RSA_1024_PUB,
	KEY_RSA_1024_PRV,
	KEY_RSA_2048_PUB,
	KEY_RSA_2048_PRV,
	KEY_NAME
};
/*use for sharemem data struct loc.*/
struct tks_key_cal {
	enum  key_type type;
	unsigned int   keylen;
	char  keydata[0];
};

struct tks_key {
	enum  key_type type;
	unsigned int   keylen;
       /*__u8* keydata;*/
	__u8 keydata[1280];
};

struct pack_data {
	unsigned int   len;
	__u8 *data;
};

struct big_int {
	unsigned int   len;
	__u8 *data;
};

struct dh_param {
	struct big_int a;
	struct big_int g;
	struct big_int p;
};

struct request_gen_keypair {
	/*struct tks_key rsapub;*/        /*[out]*/
	struct tks_key rsaprv;        /*[out]*/
	struct tks_key out_rsapub;    /*[out]*/
};

struct request_dec_simple {
	struct tks_key   encryptkey;  /*[in]*/
	struct pack_data data;        /*[in/out]*/
};

struct request_dec_pack_userpass {
	struct tks_key encryptkey;   /*[in]*/
	struct tks_key userpass;     /*[in/out]*/
};

struct request_crypto {
	__u8   op;                    /*0=SIGN,1=VERIFY,2=ENC,3=DEC; [in]*/
	struct tks_key   encryptkey;  /*[in] */
	struct tks_key   userpass;    /*encryptkey userpass;[in]*/
	struct pack_data data;        /*[in/out]*/
	struct pack_data signature;   /*[in] only used for op==VERIFY*/
};

struct request_gen_dhkey {
	struct tks_key  encryptkey;  /*[in]*/
	struct dh_param param;       /*[in]*/
	struct big_int  b;           /*[out]*/
	struct big_int  k;           /*[out]*/
};

struct request_hmac {
	struct pack_data message;          /*[in(4090-4)/out(32)]*/
};

#define SEC_IDC_TEST_USED           _IOWR('S', 0x0, struct request_gen_keypair)
#define	SEC_IOC_GEN_KEYPAIR         _IOWR('S', 0x1, struct request_gen_keypair)
#define	SEC_IOC_DEC_SIMPLE          _IOWR('S', 0x2, struct request_dec_simple)
#define	SEC_IOC_DEC_PACK_USERPASS   _IOWR('S', 0x3, struct request_dec_pack_userpass)
#define	SEC_IOC_CRYPTO              _IOWR('S', 0x4, struct request_crypto)
#define	SEC_IOC_GEN_DHKEY           _IOWR('S', 0x5, struct request_gen_dhkey)
#define	SEC_IOC_HMAC                _IOWR('S', 0x6, struct request_hmac)
#endif
