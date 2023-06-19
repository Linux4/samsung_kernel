/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <linux/string.h>
#include "sha256.h"
#include "hmac-sha256.h"

#define EVP_MAX_MD_SIZE 64

unsigned char *HMAC_SHA256(const void *key, unsigned int key_len,
				const unsigned char *data,
				unsigned int data_len,
				unsigned char *out)
{
	HMAC_SHA256_CTX ctx;

	if (out == NULL)
		return NULL;

	HMAC_SHA256_CTX_init(&ctx);
	if (!HMAC_SHA256_Init_ex(&ctx, key, key_len) ||
			!HMAC_SHA256_Update(&ctx, data, data_len) ||
			!HMAC_SHA256_Final(&ctx, out))
		out = NULL;

	HMAC_SHA256_CTX_cleanup(&ctx);
	return out;
}

void HMAC_SHA256_CTX_init(HMAC_SHA256_CTX *ctx)
{
	SHA256_Init(&ctx->i_ctx);
	SHA256_Init(&ctx->o_ctx);
	SHA256_Init(&ctx->md_ctx);
}

void HMAC_SHA256_CTX_cleanup(HMAC_SHA256_CTX *ctx)
{
	memset(ctx, 0x00, sizeof(HMAC_SHA256_CTX));
}

int HMAC_SHA256_Init_ex(HMAC_SHA256_CTX *ctx, const void *key, unsigned int key_len)
{
	unsigned int i;
	unsigned char pad[HMAC_MAX_MD_CBLOCK];
	unsigned char key_block[HMAC_MAX_MD_CBLOCK];
	unsigned int key_block_len = SHA256_DIGEST_LENGTH;
	unsigned int block_size = SHA256_CBLOCK;

	/* If either |key| is non-NULL or |md| has changed, initialize with a new key
	 * rather than rewinding the previous one.
	 *
	 * TODO(davidben,eroman): Passing the previous |md| with a NULL |key| is
	 * ambiguous between using the empty key and reusing the previous key. There
	 * exist callers which intend the latter, but the former is an awkward edge
	 * case. Fix to API to avoid this.
	 */
	if (key == NULL)
		goto out;

	if (block_size < key_len) {
		/* Long keys are hashed. */
		if (!SHA256_Init(&ctx->md_ctx) ||
				!SHA256_Update(&ctx->md_ctx, key, key_len) ||
				!SHA256_Final(key_block, &ctx->md_ctx))
			return 0;
	} else {
		memcpy(key_block, key, key_len);
		key_block_len = (unsigned int)key_len;
	}

	/* Keys are then padded with zeros. */
	if (key_block_len != HMAC_MAX_MD_CBLOCK)
		memset(&key_block[key_block_len], 0, sizeof(key_block) - key_block_len);

	for (i = 0; i < HMAC_MAX_MD_CBLOCK; i++)
		pad[i] = 0x36 ^ key_block[i];

	if (!SHA256_Init(&ctx->i_ctx) ||
			!SHA256_Update(&ctx->i_ctx, pad, block_size))
		return 0;

	for (i = 0; i < HMAC_MAX_MD_CBLOCK; i++)
		pad[i] = 0x5c ^ key_block[i];

	if (!SHA256_Init(&ctx->o_ctx) ||
			!SHA256_Update(&ctx->o_ctx, pad, block_size))
		return 0;

out:
	if (!SHA256_CTX_copy(&ctx->md_ctx, &ctx->i_ctx))
		return 0;

	return 1;
}

int HMAC_SHA256_Update(HMAC_SHA256_CTX *ctx, const unsigned char *data,
			unsigned int data_len)
{
	return SHA256_Update(&ctx->md_ctx, data, data_len);
}

int HMAC_SHA256_Final(HMAC_SHA256_CTX *ctx, unsigned char *out)
{
	unsigned char buf[EVP_MAX_MD_SIZE];

	/* TODO(davidben): The only thing that can officially fail here is
	 * |EVP_MD_CTX_copy_ex|, but even that should be impossible in this case.
	 */
	if (!SHA256_Final(buf, &ctx->md_ctx) ||
			!SHA256_CTX_copy(&ctx->md_ctx, &ctx->o_ctx) ||
			!SHA256_Update(&ctx->md_ctx, buf, SHA256_DIGEST_LENGTH) ||
			!SHA256_Final(out, &ctx->md_ctx))
		return 0;

	return 1;
}

unsigned int HMAC_size(const HMAC_SHA256_CTX *ctx)
{
	return SHA256_DIGEST_LENGTH;
}

int HMAC_SHA256_CTX_copy_ex(HMAC_SHA256_CTX *dest, const HMAC_SHA256_CTX *src)
{
	if (!SHA256_CTX_copy(&dest->i_ctx, &src->i_ctx) ||
			!SHA256_CTX_copy(&dest->o_ctx, &src->o_ctx) ||
			!SHA256_CTX_copy(&dest->md_ctx, &src->md_ctx))
		return 0;

	return 1;
}

int HMAC_Init(HMAC_SHA256_CTX *ctx, const void *key, int key_len)
{
	if (key)
		HMAC_SHA256_CTX_init(ctx);

	return HMAC_SHA256_Init_ex(ctx, key, key_len);
}

int HMAC_SHA256_CTX_copy(HMAC_SHA256_CTX *dest, const HMAC_SHA256_CTX *src)
{
	HMAC_SHA256_CTX_init(dest);
	return HMAC_SHA256_CTX_copy_ex(dest, src);
}
