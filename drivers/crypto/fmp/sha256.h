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

#ifndef SHA256_FMP_H
#define SHA256_FMP_H

#if defined(__cplusplus)
extern "C" {
#endif

/* SHA-256. */


/* SHA256_CBLOCK is the block size of SHA-256. */
#define SHA256_CBLOCK 64

/* SHA256_DIGEST_LENGTH is the length of a SHA-256 digest. */
#define SHA256_DIGEST_LENGTH 32

typedef struct sha256_state_st {
	unsigned int h[8];
	unsigned int Nl, Nh;
	unsigned char data[SHA256_CBLOCK];
	unsigned int num, md_len;
} SHA256_CTX;

/* SHA256_Init initialises |sha| and returns 1. */
int SHA256_Init(SHA256_CTX *sha);

/* SHA256_Update adds |len| bytes from |data| to |sha| and returns 1. */
int SHA256_Update(SHA256_CTX *sha, const void *data, unsigned int len);

/* SHA256_Final adds the final padding to |sha| and writes the resulting digest
 * to |md|, which must have at least |SHA256_DIGEST_LENGTH| bytes of space. It
 * returns one on success and zero on programmer error.
 */
int SHA256_Final(unsigned char *md, SHA256_CTX *sha);

/* SHA256 writes the digest of |len| bytes from |data| to |out| and returns
 * |out|. There must be at least |SHA256_DIGEST_LENGTH| bytes of space in |out|.
 */
unsigned char *SHA256(const unsigned char *data, unsigned int len, unsigned char *out);

/* SHA256_Transform is a low-level function that performs a single, SHA-1 block
 * transformation using the state from |sha| and 64 bytes from |block|.
 */
void SHA256_Transform(SHA256_CTX *sha, const unsigned char *data);

/* CTX dup */
int SHA256_CTX_copy(SHA256_CTX *dst, const SHA256_CTX *src);

#if defined(__cplusplus)
}  /* extern C */
#endif

#endif  /* SHA256_FMP_H */
