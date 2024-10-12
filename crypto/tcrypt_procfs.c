/*
 * Quick & dirty crypto testing module.
 *
 * This will only exist until we have a better testing mechanism
 * (e.g. a char device).
 *
 * Copyright (c) 2002 James Morris <jmorris@intercode.com.au>
 * Copyright (c) 2002 Jean-Francois Dive <jef@linuxbe.org>
 * Copyright (c) 2007 Nokia Siemens Networks
 *
 * Updated RFC4106 AES-GCM testing.
 *    Authors: Aidan O'Mahony (aidan.o.mahony@intel.com)
 *             Adrian Hoban <adrian.hoban@intel.com>
 *             Gabriele Paoloni <gabriele.paoloni@intel.com>
 *             Tadeusz Struk (tadeusz.struk@intel.com)
 *             Copyright (c) 2010, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

//KERN_EMERG, KERN_ALERT, KERN_CRIT, KERN_ERR, KERN_WARNING, KERN_NOTICE, KERN_INFO, KERN_DEBUG, KERN_DEFAULT, KERN_CONT
#define PR_CRYPT(fmt, ...) \
	printk(KERN_EMERG pr_fmt(fmt), ##__VA_ARGS__)

#if 1
#define PR_ECHO(fmt, ...) {}
#else
static void PR_ECHO(fmt, ...)
{
	char cmd[100];
	snprintf(cmd, sizeof(cmd), pr_fmt(fmt),  ##__VA_ARGS__);
	char *arg[] = { "/bin/echo", cmd, 0 };
	execve(arg[0], &arg[0], 0);
}
#endif

#include <linux/fs.h>
#include <linux/pid_namespace.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/seqlock.h>
#include <linux/time.h>

#include <crypto/aead.h>
#include <crypto/hash.h>
#include <crypto/skcipher.h>
#include <linux/err.h>
#include <linux/fips.h>
#include <linux/init.h>
#include <linux/gfp.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/string.h>
#include <linux/moduleparam.h>
#include <linux/jiffies.h>
#include <linux/timex.h>
#include <linux/interrupt.h>
#include "tcrypt_procfs.h"

/*
 * Need slab memory for testing (size in number of pages).
 */
#define TVMEMSIZE	4

/*
* Used by test_cipher_speed()
*/
#define ENCRYPT 1
#define DECRYPT 0

#define MAX_DIGEST_SIZE		64

/*
 * return a string with the driver name
 */
#define get_driver_name(tfm_type, tfm) crypto_tfm_alg_driver_name(tfm_type ## _tfm(tfm))

/*
 * Used by test_cipher_speed()
 */
static unsigned int sec;

static char *alg = NULL;
static u32 type;
static u32 mask;
static int mode;
static u32 num_mb = 8;
static char *tvmem[TVMEMSIZE];

#if 0
static u32 block_sizes[] = { 16, 64, 256, 1024, 8192, 0 };
static u32 aead_sizes[] = { 16, 64, 256, 512, 1024, 2048, 4096, 8192, 0 };
#else
static u32 block_sizes[] = {  64, 1024, 0 };
static u32 aead_sizes[] = { 16, 512, 4096, 0 };
#endif

#define XBUFSIZE 8
#define MAX_IVLEN 32

static int testmgr_alloc_buf(char *buf[XBUFSIZE])
{
	int i;

	for (i = 0; i < XBUFSIZE; i++) {
		buf[i] = (void *)__get_free_page(GFP_KERNEL);
		if (!buf[i])
			goto err_free_buf;
	}

	return 0;

err_free_buf:
	while (i-- > 0)
		free_page((unsigned long)buf[i]);

	return -ENOMEM;
}

static void testmgr_free_buf(char *buf[XBUFSIZE])
{
	int i;

	for (i = 0; i < XBUFSIZE; i++)
		free_page((unsigned long)buf[i]);
}

static void sg_init_aead(struct scatterlist *sg, char *xbuf[XBUFSIZE],
			 unsigned int buflen, const void *assoc,
			 unsigned int aad_size)
{
	int np = (buflen + PAGE_SIZE - 1)/PAGE_SIZE;
	int k, rem;

	if (np > XBUFSIZE) {
		rem = PAGE_SIZE;
		np = XBUFSIZE;
	} else {
		rem = buflen % PAGE_SIZE;
	}

	sg_init_table(sg, np + 1);

	sg_set_buf(&sg[0], assoc, aad_size);

	if (rem)
		np--;
	for (k = 0; k < np; k++)
		sg_set_buf(&sg[k + 1], xbuf[k], PAGE_SIZE);

	if (rem)
		sg_set_buf(&sg[k + 1], xbuf[k], rem);
}

static inline int do_one_aead_op(struct aead_request *req, int ret)
{
	struct crypto_wait *wait = req->base.data;

	return crypto_wait_req(ret, wait);
}

static int test_aead_jiffies(struct aead_request *req, int enc,
				int blen, int secs)
{
	unsigned long start, end;
	int bcount;
	int ret;

	for (start = jiffies, end = start + secs * HZ, bcount = 0;
	     time_before(jiffies, end); bcount++) {
		if (enc)
			ret = do_one_aead_op(req, crypto_aead_encrypt(req));
		else
			ret = do_one_aead_op(req, crypto_aead_decrypt(req));

		if (ret)
			return ret;
	}

	printk("%d operations in %d seconds (%ld bytes)\n",
	       bcount, secs, (long)bcount * blen);
	return 0;
}

static int test_aead_cycles(struct aead_request *req, int enc, int blen)
{
	unsigned long cycles = 0;
	int ret = 0;
	int i;

	/* Warm-up run. */
	for (i = 0; i < 4; i++) {
		if (enc)
			ret = do_one_aead_op(req, crypto_aead_encrypt(req));
		else
			ret = do_one_aead_op(req, crypto_aead_decrypt(req));

		if (ret)
			goto out;
	}

	/* The real thing. */
	for (i = 0; i < 8; i++) {
		cycles_t start, end;

		start = get_cycles();
		if (enc)
			ret = do_one_aead_op(req, crypto_aead_encrypt(req));
		else
			ret = do_one_aead_op(req, crypto_aead_decrypt(req));
		end = get_cycles();

		if (ret)
			goto out;

		cycles += end - start;
	}

out:
	if (ret == 0)
		printk("1 operation in %lu cycles (%d bytes)\n",
		       (cycles + 4) / 8, blen);

	return ret;
}

static int test_aead_speed(const char *algo, int enc, unsigned int secs,
			    struct aead_speed_template *template,
			    unsigned int tcount, u8 authsize,
			    unsigned int aad_size, u8 *keysize)
{
	unsigned int i, j;
	struct crypto_aead *tfm;
	int ret = -ENOMEM;
	const char *key;
	struct aead_request *req;
	struct scatterlist *sg;
	struct scatterlist *sgout;
	const char *e;
	void *assoc;
	char *iv;
	char *xbuf[XBUFSIZE];
	char *xoutbuf[XBUFSIZE];
	char *axbuf[XBUFSIZE];
	unsigned int *b_size;
	unsigned int iv_len;
	struct crypto_wait wait;

	iv = kzalloc(MAX_IVLEN, GFP_KERNEL);
	if (!iv)
		return ret;

	if (aad_size >= PAGE_SIZE) {
		pr_err("associate data length (%u) too big\n", aad_size);
		ret = -E2BIG;
		goto out_noxbuf;
	}

	if (enc == ENCRYPT)
		e = "encryption";
	else
		e = "decryption";

	if (testmgr_alloc_buf(xbuf))
		goto out_noxbuf;
	if (testmgr_alloc_buf(axbuf))
		goto out_noaxbuf;
	if (testmgr_alloc_buf(xoutbuf))
		goto out_nooutbuf;

	sg = kmalloc(sizeof(*sg) * 9 * 2, GFP_KERNEL);
	if (!sg)
		goto out_nosg;
	sgout = &sg[9];

	tfm = crypto_alloc_aead(algo, 0, 0);

	if (IS_ERR(tfm)) {
		pr_err("alg: aead: Failed to load transform for %s: %ld\n", algo,
		       PTR_ERR(tfm));
		       ret = PTR_ERR(tfm);
		goto out_notfm;
	}

	crypto_init_wait(&wait);
	pr_info("\ntesting speed of %s (%s) %s\n", algo,
			get_driver_name(crypto_aead, tfm), e);

	req = aead_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		pr_err("alg: aead: Failed to allocate request for %s\n",
		       algo);
		goto out_noreq;
	}

	aead_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
				  crypto_req_done, &wait);

	i = 0;
	do {
		b_size = aead_sizes;
		do {
			assoc = axbuf[0];
			memset(assoc, 0xff, aad_size);

			if ((*keysize + *b_size) > TVMEMSIZE * PAGE_SIZE) {
				pr_err("template (%u) too big for tvmem (%lu)\n",
				       *keysize + *b_size,
					TVMEMSIZE * PAGE_SIZE);
				goto out;
			}

			key = tvmem[0];
			for (j = 0; j < tcount; j++) {
				if (template[j].klen == *keysize) {
					key = template[j].key;
					break;
				}
			}
			ret = crypto_aead_setkey(tfm, key, *keysize);
			ret = crypto_aead_setauthsize(tfm, authsize);

			iv_len = crypto_aead_ivsize(tfm);
			if (iv_len)
				memset(iv, 0xff, iv_len);

			crypto_aead_clear_flags(tfm, ~0);
			pr_info("test %u (%d bit key, %d byte blocks): ",
					i, *keysize * 8, *b_size);


			memset(tvmem[0], 0xff, PAGE_SIZE);

			if (ret) {
				pr_err("setkey() failed flags=%x\n",
						crypto_aead_get_flags(tfm));
						//ret = -ECRYPTOFLAG;
				goto out;
			}

			sg_init_aead(sg, xbuf, *b_size + (enc ? 0 : authsize),
				     assoc, aad_size);

			sg_init_aead(sgout, xoutbuf,
				     *b_size + (enc ? authsize : 0), assoc,
				     aad_size);

			aead_request_set_ad(req, aad_size);

			if (!enc) {

				/*
				 * For decryption we need a proper auth so
				 * we do the encryption path once with buffers
				 * reversed (input <-> output) to calculate it
				 */
				aead_request_set_crypt(req, sgout, sg,
						       *b_size, iv);
				ret = do_one_aead_op(req,
						     crypto_aead_encrypt(req));

				if (ret) {
					pr_err("calculating auth failed failed (%d)\n",
					       ret);
					break;
				}
			}

			aead_request_set_crypt(req, sg, sgout,
					       *b_size + (enc ? 0 : authsize),
					       iv);

			if (secs) {
				ret = test_aead_jiffies(req, enc, *b_size,
							secs);
				cond_resched();
			} else {
				ret = test_aead_cycles(req, enc, *b_size);
			}

			if (ret) {
				pr_err("%s() failed return code=%d\n", e, ret);
				break;
			}
			b_size++;
			i++;
		} while (*b_size);
		keysize++;
	} while (*keysize);

out:
	aead_request_free(req);
out_noreq:
	crypto_free_aead(tfm);
out_notfm:
	kfree(sg);
out_nosg:
	testmgr_free_buf(xoutbuf);
out_nooutbuf:
	testmgr_free_buf(axbuf);
out_noaxbuf:
	testmgr_free_buf(xbuf);
out_noxbuf:
	kfree(iv);
	
	return ret;
}

static void test_hash_sg_init(struct scatterlist *sg)
{
	int i;

	sg_init_table(sg, TVMEMSIZE);
	for (i = 0; i < TVMEMSIZE; i++) {
		sg_set_buf(sg + i, tvmem[i], PAGE_SIZE);
		memset(tvmem[i], 0xff, PAGE_SIZE);
	}
}

static inline int do_one_ahash_op(struct ahash_request *req, int ret)
{
	struct crypto_wait *wait = req->base.data;

	return crypto_wait_req(ret, wait);
}

static int test_ahash_jiffies_digest(struct ahash_request *req, int blen,
				     char *out, int secs)
{
	unsigned long start, end;
	int bcount;
	int ret;

	for (start = jiffies, end = start + secs * HZ, bcount = 0;
	     time_before(jiffies, end); bcount++) {
		ret = do_one_ahash_op(req, crypto_ahash_digest(req));
		if (ret)
			return ret;
	}

	printk("%6u opers/sec, %9lu bytes/sec\n",
	       bcount / secs, ((long)bcount * blen) / secs);

	return 0;
}

static int test_ahash_jiffies(struct ahash_request *req, int blen,
			      int plen, char *out, int secs)
{
	unsigned long start, end;
	int bcount, pcount;
	int ret;

	if (plen == blen)
		return test_ahash_jiffies_digest(req, blen, out, secs);

	for (start = jiffies, end = start + secs * HZ, bcount = 0;
	     time_before(jiffies, end); bcount++) {
		ret = do_one_ahash_op(req, crypto_ahash_init(req));
		if (ret)
			return ret;
		for (pcount = 0; pcount < blen; pcount += plen) {
			ret = do_one_ahash_op(req, crypto_ahash_update(req));
			if (ret)
				return ret;
		}
		/* we assume there is enough space in 'out' for the result */
		ret = do_one_ahash_op(req, crypto_ahash_final(req));
		if (ret)
			return ret;
	}

	pr_cont("%6u opers/sec, %9lu bytes/sec\n",
		bcount / secs, ((long)bcount * blen) / secs);

	return 0;
}

static int test_ahash_cycles_digest(struct ahash_request *req, int blen,
				    char *out)
{
	unsigned long cycles = 0;
	int ret, i;

	/* Warm-up run. */
	for (i = 0; i < 4; i++) {
		ret = do_one_ahash_op(req, crypto_ahash_digest(req));
		if (ret)
			goto out;
	}

	/* The real thing. */
	for (i = 0; i < 8; i++) {
		cycles_t start, end;

		start = get_cycles();

		ret = do_one_ahash_op(req, crypto_ahash_digest(req));
		if (ret)
			goto out;

		end = get_cycles();

		cycles += end - start;
	}

out:
	if (ret)
		return ret;

	pr_cont("%6lu cycles/operation, %4lu cycles/byte\n",
		cycles / 8, cycles / (8 * blen));

	return 0;
}

static int test_ahash_cycles(struct ahash_request *req, int blen,
			     int plen, char *out)
{
	unsigned long cycles = 0;
	int i, pcount, ret;

	if (plen == blen)
		return test_ahash_cycles_digest(req, blen, out);

	/* Warm-up run. */
	for (i = 0; i < 4; i++) {
		ret = do_one_ahash_op(req, crypto_ahash_init(req));
		if (ret)
			goto out;
		for (pcount = 0; pcount < blen; pcount += plen) {
			ret = do_one_ahash_op(req, crypto_ahash_update(req));
			if (ret)
				goto out;
		}
		ret = do_one_ahash_op(req, crypto_ahash_final(req));
		if (ret)
			goto out;
	}

	/* The real thing. */
	for (i = 0; i < 8; i++) {
		cycles_t start, end;

		start = get_cycles();

		ret = do_one_ahash_op(req, crypto_ahash_init(req));
		if (ret)
			goto out;
		for (pcount = 0; pcount < blen; pcount += plen) {
			ret = do_one_ahash_op(req, crypto_ahash_update(req));
			if (ret)
				goto out;
		}
		ret = do_one_ahash_op(req, crypto_ahash_final(req));
		if (ret)
			goto out;

		end = get_cycles();

		cycles += end - start;
	}

out:
	if (ret)
		return ret;

	pr_cont("%6lu cycles/operation, %4lu cycles/byte\n",
		cycles / 8, cycles / (8 * blen));

	return 0;
}

static int test_ahash_speed_common(const char *algo, unsigned int secs, unsigned mask)
{
	struct scatterlist sg[TVMEMSIZE];
	struct crypto_wait wait;
	struct ahash_request *req;
	struct crypto_ahash *tfm;
	char *output;
	int i, ret = -ENOMEM;
	struct hash_speed *speed = generic_hash_speed_template;

	tfm = crypto_alloc_ahash(algo, 0, mask);
	if (IS_ERR(tfm)) {
		pr_err("failed to load transform for %s: %ld\n",
		       algo, PTR_ERR(tfm));
		       ret = PTR_ERR(tfm);
		return ret;
	}

	pr_info("\ntesting speed of async(%s,%d) %s (%s)\n",
			(tfm->base.__crt_alg->cra_flags&CRYPTO_ALG_ASYNC)?"true":"false",
			tfm->base.__crt_alg->cra_flags,
			algo,
			get_driver_name(crypto_ahash, tfm));

	if( !strcmp(algo, "ghash") ) speed = hash_speed_template_16;
	else if( !strcmp(algo, "poly1305") || !strcmp(algo, "nhpoly1305") ) speed = poly1305_speed_template;

	if (crypto_ahash_digestsize(tfm) > MAX_DIGEST_SIZE) {
		pr_err("digestsize(%u) > %d\n", crypto_ahash_digestsize(tfm),
		       MAX_DIGEST_SIZE);
		       ret = -E2BIG;
		goto out;
	}

	test_hash_sg_init(sg);
	req = ahash_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		pr_err("ahash request allocation failure\n");
		goto out;
	}

	crypto_init_wait(&wait);
	ahash_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
				   crypto_req_done, &wait);

	output = kmalloc(MAX_DIGEST_SIZE, GFP_KERNEL);
	if (!output)
		goto out_nomem;

	for (i = 0; speed[i].blen != 0; i++) {
		if (speed[i].blen > TVMEMSIZE * PAGE_SIZE) {
			pr_err("template (%u) too big for tvmem (%lu)\n",
			       speed[i].blen, TVMEMSIZE * PAGE_SIZE);
			break;
		}

		if (speed[i].klen)
			crypto_ahash_setkey(tfm, tvmem[0], speed[i].klen);

		pr_info("test%3u "
			"(%5u byte blocks,%5u bytes per update,%4u updates): ",
			i, speed[i].blen, speed[i].plen, speed[i].blen / speed[i].plen);

		ahash_request_set_crypt(req, sg, output, speed[i].plen);

		if (secs) {
			ret = test_ahash_jiffies(req, speed[i].blen,
						 speed[i].plen, output, secs);
			cond_resched();
		} else {
			ret = test_ahash_cycles(req, speed[i].blen,
						speed[i].plen, output);
		}

		if (ret) {
			pr_err("hashing failed ret=%d\n", ret);
			break;
		}
	}

	kfree(output);

out_nomem:
	ahash_request_free(req);

out:
	crypto_free_ahash(tfm);
	
	return ret;
}

struct test_mb_skcipher_data {
	struct scatterlist sg[XBUFSIZE];
	struct skcipher_request *req;
	struct crypto_wait wait;
	char *xbuf[XBUFSIZE];
};

static inline int do_one_acipher_op(struct skcipher_request *req, int ret)
{
	struct crypto_wait *wait = req->base.data;

	return crypto_wait_req(ret, wait);
}

static int test_acipher_jiffies(struct skcipher_request *req, int enc,
				int blen, int secs)
{
	unsigned long start, end;
	int bcount;
	int ret;

	for (start = jiffies, end = start + secs * HZ, bcount = 0;
	     time_before(jiffies, end); bcount++) {
		if (enc)
			ret = do_one_acipher_op(req,
						crypto_skcipher_encrypt(req));
		else
			ret = do_one_acipher_op(req,
						crypto_skcipher_decrypt(req));

		if (ret)
			return ret;
	}

	pr_cont("%d operations in %d seconds (%ld bytes)\n",
		bcount, secs, (long)bcount * blen);
	return 0;
}

static int test_acipher_cycles(struct skcipher_request *req, int enc,
			       int blen)
{
	unsigned long cycles = 0;
	int ret = 0;
	int i;

	/* Warm-up run. */
	for (i = 0; i < 4; i++) {
		if (enc)
			ret = do_one_acipher_op(req,
						crypto_skcipher_encrypt(req));
		else
			ret = do_one_acipher_op(req,
						crypto_skcipher_decrypt(req));

		if (ret)
			goto out;
	}

	/* The real thing. */
	for (i = 0; i < 8; i++) {
		cycles_t start, end;

		start = get_cycles();
		if (enc)
			ret = do_one_acipher_op(req,
						crypto_skcipher_encrypt(req));
		else
			ret = do_one_acipher_op(req,
						crypto_skcipher_decrypt(req));
		end = get_cycles();

		if (ret)
			goto out;

		cycles += end - start;
	}

out:
	if (ret == 0)
		pr_cont("1 operation in %lu cycles (%d bytes)\n",
			(cycles + 4) / 8, blen);

	return ret;
}

static int test_skcipher_speed(const char *algo, int enc, unsigned int secs, bool async)
{
	unsigned int ret, i, j, k, iv_len;
	struct crypto_wait wait;
	const char *key;
	char iv[128];
	struct skcipher_request *req;
	struct crypto_skcipher *tfm;
	const char *e;
	u32 *b_size;
	
	struct cipher_speed_template *template = NULL;
	unsigned int tcount = 0;
	u8 keysize = 8;

	if (enc == ENCRYPT)
		e = "encryption";
	else
		e = "decryption";

	crypto_init_wait(&wait);

	tfm = crypto_alloc_skcipher(algo, 0, async ? 0 : CRYPTO_ALG_ASYNC);

	if (IS_ERR(tfm)) {
		pr_err("failed to load transform for %s: %ld\n", algo,
		       PTR_ERR(tfm));
		       ret = PTR_ERR(tfm);
		return ret;
	}

	if( (tfm->base.__crt_alg->cra_flags & CRYPTO_ALG_TYPE_SKCIPHER) == CRYPTO_ALG_TYPE_SKCIPHER ) {
		keysize = tfm->base.__crt_alg->cra_u.ablkcipher.min_keysize;
		if(keysize == 0) {
			struct skcipher_alg *skcipher = container_of(tfm->base.__crt_alg, struct skcipher_alg, base);
			//struct skcipher_alg* skcipher = (struct skcipher_alg*)(tfm->base.__crt_alg);
			keysize = skcipher->min_keysize;
			pr_err("%s keysize %d, %d, %d, %d, %d, %d, %d, %d\n", get_driver_name(crypto_skcipher, tfm),
		       tfm->base.__crt_alg->cra_u.ablkcipher.min_keysize,
		       tfm->base.__crt_alg->cra_u.ablkcipher.max_keysize,
		       tfm->base.__crt_alg->cra_u.blkcipher.min_keysize,
		       tfm->base.__crt_alg->cra_u.blkcipher.max_keysize,
		       tfm->base.__crt_alg->cra_u.cipher.cia_min_keysize,
		       tfm->base.__crt_alg->cra_u.cipher.cia_max_keysize,
		       skcipher->min_keysize,
		       skcipher->max_keysize);
			if(keysize == 0) {
			  goto out;
			}
		}
	}
	else if( (tfm->base.__crt_alg->cra_flags & CRYPTO_ALG_TYPE_SKCIPHER) == CRYPTO_ALG_TYPE_BLKCIPHER ) {
		keysize = tfm->base.__crt_alg->cra_u.blkcipher.min_keysize;
	}
	else if( (tfm->base.__crt_alg->cra_flags & CRYPTO_ALG_TYPE_SKCIPHER) == CRYPTO_ALG_TYPE_CIPHER	) {
		keysize = tfm->base.__crt_alg->cra_u.cipher.cia_min_keysize;
	}
	else {
		pr_info("\nHELIO: check the cipher type %d", tfm->base.__crt_alg->cra_flags);
		return -EOPNOTSUPP;
	}

	pr_info("\ntesting speed of async(%s,%d) %s (%s) %s with keysize(%d)\n",
			(tfm->base.__crt_alg->cra_flags&CRYPTO_ALG_ASYNC)?"true":"false",
			tfm->base.__crt_alg->cra_flags,
			algo,
			get_driver_name(crypto_skcipher, tfm), e, keysize);
	
	if(strstr(algo, "des3_ede")) {
		template = des3_speed_template;
		tcount = DES3_SPEED_VECTORS;
	}

	req = skcipher_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		pr_err("tcrypt: skcipher: Failed to allocate request for %s\n",
		       algo);
		goto out;
	}

	skcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
				      crypto_req_done, &wait);

	i = 0;
	b_size = block_sizes;

	do {
		struct scatterlist sg[TVMEMSIZE];

		if ((keysize + *b_size) > TVMEMSIZE * PAGE_SIZE) {
			pr_err("template (%u) too big for "
			       "tvmem (%lu)\n", keysize + *b_size,
			       TVMEMSIZE * PAGE_SIZE);
			goto out_free_req;
		}

		pr_info("test %u (%d bit key, %d byte blocks): ", i,
			keysize * 8, *b_size);

		memset(tvmem[0], 0xff, PAGE_SIZE);

		/* set key, plain text and IV */
		key = tvmem[0];
		for (j = 0; j < tcount; j++) {
			if (template[j].klen == keysize) {
				key = template[j].key;
				break;
			}
		}

		crypto_skcipher_clear_flags(tfm, ~0);

		ret = crypto_skcipher_setkey(tfm, key, keysize);
		if (ret) {
			pr_err("setkey() failed flags=%x\n",
				crypto_skcipher_get_flags(tfm));
			goto out_free_req;
		}

		k = keysize + *b_size;
		sg_init_table(sg, DIV_ROUND_UP(k, PAGE_SIZE));

		if (k > PAGE_SIZE) {
			sg_set_buf(sg, tvmem[0] + keysize,
			   PAGE_SIZE - keysize);
			k -= PAGE_SIZE;
			j = 1;
			while (k > PAGE_SIZE) {
				sg_set_buf(sg + j, tvmem[j], PAGE_SIZE);
				memset(tvmem[j], 0xff, PAGE_SIZE);
				j++;
				k -= PAGE_SIZE;
			}
			sg_set_buf(sg + j, tvmem[j], k);
			memset(tvmem[j], 0xff, k);
		} else {
			sg_set_buf(sg, tvmem[0] + keysize, *b_size);
		}

		iv_len = crypto_skcipher_ivsize(tfm);
		if (iv_len)
			memset(&iv, 0xff, iv_len);

		skcipher_request_set_crypt(req, sg, sg, *b_size, iv);

		if (secs) {
			ret = test_acipher_jiffies(req, enc,
						   *b_size, secs);
			cond_resched();
		} else {
			ret = test_acipher_cycles(req, enc,
						  *b_size);
		}

		if (ret) {
			pr_err("%s() failed flags=%x\n", e,
			       crypto_skcipher_get_flags(tfm));
			break;
		}
		b_size++;
		i++;
	} while (*b_size);

out_free_req:
	skcipher_request_free(req);
out:
	crypto_free_skcipher(tfm);
	
	return ret;
}

#define ERR_NO					"PASS"
#define ERR_NOMEM			"ERROR: Out of memory"
#define ERR_2BIG					"ERROR: Argument list too long"
#define ERR_FAULT				"ERROR: Bad address"
#define ERR_INVAL				"ERROR: Invalid arguments"
#define ERR_RANGE				"ERROR: Out of range"
#define ERR_OPNOTSUPP	"ERROR: Not supported"
#define ERR_UNKNOWN		"ERROR: Undefined error or crypt error"
#define ERR_NOENT				"ERROR: Failed to load transform"

static int tc_status = 0;
static char tc_alg[100] = {0,};
static int tc_error = 0;
static int crypto_test_show(struct seq_file *m, void *v)
{
	char *p = NULL;
	switch(tc_error) {
		default:
			p = ERR_UNKNOWN; break;
		case 0:
			p = ERR_NO; break;
		case -ENOMEM:
			p = ERR_NOMEM; break;
		case -E2BIG:
			p = ERR_2BIG; break;
		case -EFAULT:
			p = ERR_FAULT; break;
		case -EINVAL:
			p = ERR_INVAL; break;
		case -ERANGE:
			p = ERR_RANGE; break;
		case -ENOENT:
			p = ERR_NOENT; break;
		case -EOPNOTSUPP:
			p = ERR_OPNOTSUPP; break;
	}
	
	switch(tc_status) {
		default:
		case 0:
			seq_printf(m,"tcrypt status: idle, last result: %s(%d)\n", p, tc_error);
			break;
		case 1:
			seq_printf(m,"tcrypt status: running (%s)\n", tc_alg);
			break;
		case 2:
			seq_printf(m,"tcrypt status: done (%s), last result: %s(%d)\n", tc_alg, p, tc_error);
			tc_status = 0;
			break;
	}
	return 0;
}

static int crypto_test_open(struct inode *inode, struct file *file)
{
	return single_open(file, crypto_test_show, NULL);
}

#define MAX_CRYPTO_TEST_PARAM	3
static ssize_t crypto_test_write(struct file *file,
    const char __user *buffer, size_t count, loff_t *pos)
{
    int i = 0;
    char *buf, *tok, *tmp;
    char tc_type[100] = {0,};
    int fasync = 0;
    if(tc_status == 1) return ETXTBSY;

    tc_status = 1;
    tc_error = 0;
    memset(tc_alg,  0, sizeof(tc_alg));
    //memset(tc_type,  0, sizeof(tc_type));
    	
    buf = (char *)__get_free_page(GFP_USER);
    if (!buf) {
    	tc_error = -ENOMEM;
		goto out1;
  }
  	
    if (count >= PAGE_SIZE) {
    	tc_error = -E2BIG;
        goto out2;
      }
    if (copy_from_user(buf, buffer, count)) {
    	tc_error = -EFAULT;
        goto out2;
      }
    buf[count] = 0;

    tmp = buf;
    while ((tok = strsep(&tmp, " ")) != NULL) {
        if(i>=MAX_CRYPTO_TEST_PARAM) {
            //seq_printf(m, "over number of input params(%d): %s\n", MAX_CALC_LOAD_PRAM, tok);
            goto out2;
        }
        switch(i) {
        	case 0:
        		strcpy(tc_alg, tok);
		        while(tc_alg[strlen(tc_alg)-1] == '\r' || tc_alg[strlen(tc_alg)-1] == '\n')
		        	tc_alg[strlen(tc_alg)-1] = 0;
		      	break;
		      case 1:
        		strcpy(tc_type, tok);
		        while(tc_type[strlen(tc_type)-1] == '\r' || tc_type[strlen(tc_type)-1] == '\n')
		        	tc_type[strlen(tc_type)-1] = 0;
		      	break;
		      case 2:
		      	fasync = simple_strtol(tok, NULL, 0);
		      	//kstrtoint(tok, 10, &fasync);
		      	break;
        }
        i++;
    }

    if( !strcmp(tc_type, "skcipher") || !strcmp(tc_type, "blkcipher") || !strcmp(tc_type, "cipher") ) {
        tc_error = test_skcipher_speed(tc_alg, ENCRYPT, sec, (fasync)?true:false);
        if(!tc_error)
            tc_error = test_skcipher_speed(tc_alg, DECRYPT, sec, (fasync)?true:false);
    }
    else if( !strcmp(tc_type, "shash") || !strcmp(tc_type, "ahash") ) {
        tc_error = test_ahash_speed_common(tc_alg, sec, (fasync)?CRYPTO_ALG_ASYNC:0);
    }
    else if( !strcmp(tc_type, "aead") ) {
        int aad_size = 0;
        u8 *keysize = NULL;
        if( !strcmp(tc_alg, "rfc4106(gcm(aes))") ) { aad_size = 16; keysize = aead_speed_template_20; }
        else if( !strcmp(tc_alg, "gcm(aes)") ) { aad_size = 8; keysize = speed_template_16; }
        else if( !strcmp(tc_alg, "rfc4309(ccm(aes))") ) { aad_size = 16; keysize = aead_speed_template_19; }
        else if( !strcmp(tc_alg, "rfc7539esp(chacha20,poly1305)") ) { aad_size = 8; keysize = aead_speed_template_36; }
        if(aad_size>0 && keysize!=NULL) {
            tc_error = test_aead_speed(tc_alg, ENCRYPT, sec, NULL, 0, 16, aad_size, keysize);
            if(tc_error == 0) {
                tc_error = test_aead_speed(tc_alg, DECRYPT, sec, NULL, 0, 16, aad_size, keysize);
            }
        }
	    	else tc_error = -EOPNOTSUPP;
		}
out2:
    free_page((unsigned long)buf);

out1:    
    tc_status = 2;
    
    return count;
}
static const struct file_operations crypto_test_fops = {
	.open		= crypto_test_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write	= crypto_test_write,
};


static struct proc_dir_entry *crypto_procfs_dir = NULL;
static struct proc_dir_entry *crypto_procfs_de = NULL;

static int __init tcrypt_node_init(void)
{
	int i;

	printk(KERN_ERR "Helio : enter tcrypt_node_init\n");
	pr_debug("Helio : enter tcrypt_node_init\n");

	sec = 1;

	if(crypto_procfs_dir == NULL) crypto_procfs_dir = proc_mkdir("crypt", NULL);
	if(crypto_procfs_de == NULL) crypto_procfs_de = proc_create("test", 0644, crypto_procfs_dir, &crypto_test_fops);

	for (i = 0; i < TVMEMSIZE; i++) {
		tvmem[i] = (void *)__get_free_page(GFP_KERNEL);
		if (!tvmem[i]) {
			//tc_error = -ENOMEM;
			goto out;
		}
	}
	return 0;

out:	
	for (i = 0; i < TVMEMSIZE && tvmem[i]; i++) {
		free_page((unsigned long)tvmem[i]);
		tvmem[i] = 0;
	}
	return(-ENOMEM);
}

static void __exit tcrypt_node_fini(void)
{
	int i;
	
	if(crypto_procfs_de) {
		proc_remove(crypto_procfs_de);
		crypto_procfs_de = NULL;
	}
	if(crypto_procfs_dir) {
		proc_remove(crypto_procfs_dir);
		crypto_procfs_dir = NULL;
	}

	for (i = 0; i < TVMEMSIZE && tvmem[i]; i++) {
		free_page((unsigned long)tvmem[i]);
		tvmem[i] = 0;
	}
}
#if 1
module_init(tcrypt_node_init);
module_exit(tcrypt_node_fini);
#else
module_init(tcrypt_mod_init);
module_exit(tcrypt_mod_fini);
#endif

module_param(alg, charp, 0);
module_param(type, uint, 0);
module_param(mask, uint, 0);
module_param(mode, int, 0);
module_param(sec, uint, 0);
MODULE_PARM_DESC(sec, "Length in seconds of speed tests "
		      "(defaults to zero which uses CPU cycles instead)");
module_param(num_mb, uint, 0000);
MODULE_PARM_DESC(num_mb, "Number of concurrent requests to be used in mb speed tests (defaults to 8)");

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Quick & dirty crypto testing module");
MODULE_AUTHOR("James Morris <jmorris@intercode.com.au>");
