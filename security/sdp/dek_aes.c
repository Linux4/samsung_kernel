/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * Sensitive Data Protection
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
 
#include <linux/err.h>
#include <sdp/dek_common.h>
#include <sdp/dek_aes.h>
#include <crypto/skcipher.h>

static struct crypto_skcipher *dek_aes_key_setup(kek_t *kek)
{
	struct crypto_skcipher *tfm = NULL;

	tfm = crypto_alloc_skcipher("cbc(aes)", 0, CRYPTO_ALG_ASYNC);
	if (!IS_ERR(tfm)) {
		crypto_skcipher_setkey(tfm, kek->buf, kek->len);
	} else {
		printk("dek: failed to alloc blkcipher\n");
		tfm = NULL;
	}
	return tfm;
}

static void dek_aes_key_free(struct crypto_skcipher *tfm)
{
	crypto_free_skcipher(tfm);
}

#define DEK_AES_CBC_IV_SIZE 16

static int __dek_aes_do_crypt(struct crypto_skcipher *tfm,
		unsigned char *src, unsigned char *dst, int len, bool encrypt) {
	int rc = 0;
	struct skcipher_request *req = NULL;
	DECLARE_CRYPTO_WAIT(wait);
	struct scatterlist src_sg, dst_sg;
	u8 iv[DEK_AES_CBC_IV_SIZE] = {0,};

	req = skcipher_request_alloc(tfm, GFP_NOFS);
	if (!req) {
		return -ENOMEM;
	}

	skcipher_request_set_callback(req,
					CRYPTO_TFM_REQ_MAY_BACKLOG | CRYPTO_TFM_REQ_MAY_SLEEP,
					crypto_req_done, &wait);

	sg_init_one(&src_sg, src, len);
	sg_init_one(&dst_sg, dst, len);

	skcipher_request_set_crypt(req, &src_sg, &dst_sg, len, iv);

	if (encrypt)
		rc = crypto_wait_req(crypto_skcipher_encrypt(req), &wait);
	else
		rc = crypto_wait_req(crypto_skcipher_decrypt(req), &wait);

	skcipher_request_free(req);
	return rc;
}

int dek_aes_encrypt(kek_t *kek, unsigned char *src, unsigned char *dst, int len) {
	int rc;
	struct crypto_skcipher *tfm;

	if(kek == NULL) return -EINVAL;

	tfm = dek_aes_key_setup(kek);

	if(tfm) {
		rc = __dek_aes_do_crypt(tfm, src, dst, len, true);
		dek_aes_key_free(tfm);
		return rc;
	} else
		return -ENOMEM;
}

int dek_aes_decrypt(kek_t *kek, unsigned char *src, unsigned char *dst, int len) {
	int rc;
	struct crypto_skcipher *tfm;

	if(kek == NULL) return -EINVAL;

	tfm = dek_aes_key_setup(kek);

	if(tfm) {
		rc = __dek_aes_do_crypt(tfm, src, dst, len, false);
		dek_aes_key_free(tfm);
		return rc;
	} else
		return -ENOMEM;
}
