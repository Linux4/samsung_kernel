#include <linux/string.h>
#include <linux/slab.h>
#include "oaes_lib.h"

// 128 bit key
#define OAES_KEY_LENGTH	16
#define OAES_HEADER_SIZE 32

int oaes_fs_encrypt(const unsigned char* keyArray, const unsigned char* inArray, int inLen,
		unsigned char* outArray, int* outLen)
{
	OAES_CTX * ctx = NULL;
	int rc;

	printk("AES7 oaes_encrypt\n");
	ctx = oaes_alloc();
	//:TODO switch to CBC
	rc = oaes_set_option( ctx, OAES_OPTION_ECB, NULL );

	rc = oaes_key_import_data( ctx, keyArray, OAES_KEY_LENGTH);

	if (*outLen < inLen + OAES_HEADER_SIZE)
		return -1;
	rc = oaes_encrypt(ctx, inArray, inLen, outArray, outLen);
	return rc;
}

int oaes_fs_decrypt(const unsigned char* keyArray, const unsigned char* inArray, int inLen,
		unsigned char* outArray, int* outLen)
{
	OAES_CTX * ctx = NULL;
	int rc;

	printk("AES7 oaes_encrypt\n");
	ctx = oaes_alloc();
	//:TODO switch to CBC
	rc = oaes_set_option( ctx, OAES_OPTION_ECB, NULL );

	rc = oaes_key_import_data( ctx, keyArray, OAES_KEY_LENGTH);

	if (*outLen < inLen - OAES_HEADER_SIZE)
		return -1;
	rc = oaes_decrypt(ctx, inArray, inLen, outArray, outLen);
	return rc;
}

