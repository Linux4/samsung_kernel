#include <linux/string.h>
#include "oaes_lib.h"
#include "bn.h"
#include "rsa.h"
#include "rsa_lib.h"

#define DAR_MAX_RSA_BITS	2048

#define	KEY_TYPE_PUBLIC		1
#define	KEY_TYPE_PAIR		2

int	RSA_bits		= DAR_MAX_RSA_BITS;

void rsa_genKeyPair(unsigned char *keyPair, int *len, int bits)
{
	BIGNUM* bn = BN_new();
	unsigned char*	n = NULL;
	unsigned char*	d = NULL;
	unsigned short slen, nlen, dlen;
	unsigned char* p;
	RSA*	key = NULL;

	printk("rsa_genKeyPair\n");
	BN_add_word(bn, RSA_F4);
	key = (RSA*)kmalloc(sizeof(RSA), 1);
	memset(key, 0x00, sizeof(RSA));
	RSA_eay_init(key);

	// 512bit key for 64byte block encryption
	RSA_bits = bits;
	RSA_generate_key_ex(key, RSA_bits, bn, NULL);

	n = (unsigned char*)kmalloc(bits/2, 1);
	d = (unsigned char*)kmalloc(bits/2, 1);
	nlen = BN_bn2bin(key->n, n);
	dlen = BN_bn2bin(key->d, d);
	printk("N=%s\n", BN_bn2dec(key->n));
	printk("D=%s\n", BN_bn2dec(key->d));

	if (*len < 4 + nlen + dlen) {
		printk("RSA not enough buffer for gen key\n");
		goto ret;
	}
	slen = ((nlen & 0x00ff) << 8) | ((nlen & 0xff00) >> 8);
	p = keyPair;
	memcpy(p, &slen, 2);
	p += 2;
	memcpy(p, n, nlen);
	p += nlen;

	slen = ((dlen & 0x00ff) << 8) | ((dlen & 0xff00) >> 8);
	memcpy(p, &slen, 2);
	p += 2;
	memcpy(p, d, dlen);
	p += dlen;
	*len = nlen + dlen + 4;
ret:
	if(key)	kfree(key);
	if (n)	kfree(n);
	if (d)	kfree(d);
}
//EXPORT_SYMBOL(rsa_genKeyPair);

void rsa_getPublicKey(const unsigned char *keyPair, int keyPairLen, unsigned char *pubKey, int *pubKeyLen)
{
	short slen;
	int len;
	unsigned char *p;

	p = (unsigned char*)keyPair;
	memcpy(&slen, p, 2);
	len = ((slen & 0x00ff) << 8) | ((slen & 0xff00) >> 8);
	p += 2;
	printk("RSA get pub key len=%d\n", len);
	if (len > *pubKeyLen)
		return;

	*pubKeyLen = 2+len;
	memcpy(pubKey, keyPair, 2+len);
}
//EXPORT_SYMBOL(rsa_getPublicKey);

static int byteToKey(const unsigned char* byte, int byteLen, RSA* key, int type)
{
	unsigned short slen;
	unsigned int 	len, len2;
	unsigned char *p = (unsigned char*)byte;

	memcpy(&slen, p, 2);
	len = ((slen & 0x00ff) << 8) | ((slen & 0xff00) >> 8);
	if (2 + len > byteLen)
		return -1;
	p += 2;

	key->n = BN_new();
	BN_bin2bn(p, len, key->n);
	p += len;
	if (type == KEY_TYPE_PAIR) {
		if (byteLen < 2 + len)
			return -1;
		memcpy(&slen, p, 2);
		len2 = ((slen & 0x00ff) << 8) | ((slen & 0xff00) >> 8);
		p += 2;
		if (byteLen < 2 + len + 2 + len2)
			return -1;

		key->d = BN_new();
		BN_bin2bn(p, len2, key->d);
	}

	key->e = BN_new();
	BN_add_word(key->e, RSA_F4);
	return 0;
}

void rsa_encryptByPub(const unsigned char *pubKey, int pubKeyLen, const unsigned char *in, int inLen, unsigned char *out, int *outLen)
{
	RSA*	key = NULL;
	unsigned char* buf = NULL;
	int num, rc;

	if (inLen > RSA_bits/8 - 6) {
		printk("RSA enc max block byte is %d\n", RSA_bits/8 - 6);
		*outLen = 0;
		goto ret;
	}
	key = (RSA*)kmalloc(sizeof(RSA), 1);
	memset(key, 0x00, sizeof(RSA));
	RSA_eay_init(key);
	rc = byteToKey(pubKey, pubKeyLen, key, KEY_TYPE_PUBLIC);
	if (rc < 0) {
		printk("RSA key format is not correct\n");
		*outLen = 0;
		goto ret;
	}

	// RSA enc block size must be 64. put byte size on first 2 byte and pad by 0
	buf = (unsigned char*)kmalloc(RSA_bits/8, 1);
	memset(buf, 0, RSA_bits/8);
	// Put 4 byte tag for detecting wrong key enc/dec TODO:not put constant string
	strncpy(buf, "KNOX", 4);
	buf[4] = 0;
	buf[5] = (unsigned char)inLen;
	memcpy(buf+6, in, inLen);
	num = RSA_eay_public_encrypt(RSA_bits/8, buf, out, key, RSA_NO_PADDING);
	*outLen = num;
ret:
	if (key)
		kfree(key);
	if(buf)
		kfree(buf);
}
//EXPORT_SYMBOL(rsa_encyptByPub);

void rsa_decryptByPair(const unsigned char *keyPair, int keyPairLen, const unsigned char *in, int inLen, unsigned char *out, int *outLen)
{
	RSA*	key = NULL;
	unsigned char* buf = NULL;
	int num, rc;

	key = (RSA*)kmalloc(sizeof(RSA), 1);
	memset(key, 0x00, sizeof(RSA));
	RSA_eay_init(key);

	rc = byteToKey(keyPair, keyPairLen, key, KEY_TYPE_PAIR);
	if (rc < 0) {
		printk("RSA key format is not correct\n");
		*outLen = 0;
		goto ret;
	}
	buf = (unsigned char*)kmalloc(RSA_bits/8, 1);
	num = RSA_eay_private_decrypt(RSA_bits/8, in, buf, key, RSA_NO_PADDING);
	// Check 4 byte tag
	if (strncmp(buf, "KNOX", 4) != 0) {
		printk("RSA Key Pair is not correct\n");
		*outLen = 0;
		goto ret;
	}

	// RSA enc block size must be 64. put byte size on first 2 byte and pad by 0
	if ((unsigned int)buf[5] > *outLen) {
		printk("out buffer is too small. Or encrypt/decrypt error");
		*outLen = 0;
		goto ret;
	}
	*outLen = (unsigned int)buf[5];
	memcpy(out, buf+6, *outLen);
ret:
	if (key)	kfree(key);
	if (buf)	kfree(buf);
}
//EXPORT_SYMBOL(rsa_decryptByPri);

void rsa_oaes_genKey(unsigned char *key, int *len)
{
	OAES_CTX * ctx = NULL;
	ctx = oaes_alloc();
	if (ctx == NULL)
		return;

	if (*len == 24) {
		oaes_key_gen_192( ctx );
	} else if (*len == 32) {
		oaes_key_gen_256( ctx );
	} else {
		oaes_key_gen_128( ctx );
	}

	oaes_key_export_data(ctx, key, len);
	printk("rsa_oaes_genKey len=%d\n", *len);
	if (ctx)
		oaes_free(ctx);
}
//EXPORT_SYMBOL(rsa_oaes_genKey);

void rsa_oaes_encrypt(const unsigned char* keyArray, const unsigned char* inArray, int inLen,
		unsigned char* outArray, int* outLen)
{
	OAES_CTX * ctx = NULL;
	int rc;

	ctx = oaes_alloc();
	//:TODO switch to CBC
	rc = oaes_set_option( ctx, OAES_OPTION_ECB, NULL );

	rc = oaes_key_import_data( ctx, keyArray, OAES_KEY_LENGTH);

	if (*outLen < oaes_get_oaes_bloc_len(inLen)) {
		*outLen = oaes_get_oaes_bloc_len(inLen);
		printk("RSA oaes_encrypt buffer is too small\n");
		return;
	}
	rc = oaes_encrypt(ctx, inArray, inLen, outArray, outLen);
	printk("RSA oaes_encrypt rc=%d len=%d\n", rc, *outLen);
	if (ctx)
		oaes_free(ctx);
	return;
}
//EXPORT_SYMBOL(rsa_oaes_encrypt);

void rsa_oaes_decrypt(const unsigned char* keyArray, const unsigned char* inArray, int inLen,
		unsigned char* outArray, int* outLen)
{
	OAES_CTX * ctx = NULL;
	int rc;

	printk("RSA oaes_decrypt\n");
	ctx = oaes_alloc();
	//:TODO switch to CBC
	rc = oaes_set_option( ctx, OAES_OPTION_ECB, NULL );
	printk("RSA oaes_decrypt rc=%d\n", rc);

	rc = oaes_key_import_data( ctx, keyArray, OAES_KEY_LENGTH);
	printk("RSA oaes_decrypt rc=%d\n", rc);

	rc = oaes_decrypt(ctx, inArray, inLen, outArray, outLen);
	printk("RSA oaes_decrypt rc=%d outLen=%d\n", rc, *outLen);
	if (ctx)
		oaes_free(ctx);
	return;
}
//EXPORT_SYMBOL(rsa_oaes_decrypt);

