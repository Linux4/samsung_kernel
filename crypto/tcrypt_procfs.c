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
#include "tcrypt_dbgfs.h"

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
static char *tvmem[TVMEMSIZE];


struct tcrypt_result {
	struct completion completion;
	int err;
};

static void tcrypt_complete(struct crypto_async_request *req, int err)
{
	struct tcrypt_result *res = req->data;

	if (err == -EINPROGRESS)
		return;

	res->err = err;
	complete(&res->completion);
}

#if 0
static u32 block_sizes[] = { 16, 64, 256, 1024, 8192, 0 };
#else
static u32 block_sizes[] = {  64, 1024, 0 };
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
	if (ret == -EINPROGRESS || ret == -EBUSY) {
		struct tcrypt_result *tr = req->base.data;

		wait_for_completion(&tr->completion);
		reinit_completion(&tr->completion);
		ret = tr->err;
	}
	return ret;
}

struct test_mb_ahash_data {
	struct scatterlist sg[TVMEMSIZE];
	char result[64];
	struct ahash_request *req;
	struct tcrypt_result tresult;
	char *xbuf[XBUFSIZE];
};

static int test_mb_ahash_speed(const char *algo, unsigned int sec,
				struct t_hash_speed *speed)
{
	struct test_mb_ahash_data *data;
	struct crypto_ahash *tfm;
	unsigned long start, end;
	unsigned long cycles;
	unsigned int i, j, k;
	int ret = -ENOMEM;

	data = kzalloc(sizeof(*data) * 8, GFP_KERNEL);
	if (!data)
		return ret;

	tfm = crypto_alloc_ahash(algo, 0, 0);
	if (IS_ERR(tfm)) {
		pr_err("failed to load transform for %s: %ld\n",
			algo, PTR_ERR(tfm));
			ret = PTR_ERR(tfm);
		goto free_data;
	}

	for (i = 0; i < 8; ++i) {
		if (testmgr_alloc_buf(data[i].xbuf))
			goto out;

		init_completion(&data[i].tresult.completion);

		data[i].req = ahash_request_alloc(tfm, GFP_KERNEL);
		if (!data[i].req) {
			pr_err("alg: hash: Failed to allocate request for %s\n",
			       algo);
			goto out;
		}

		ahash_request_set_callback(data[i].req, 0,
					   tcrypt_complete, &data[i].tresult);
		test_hash_sg_init(data[i].sg);
	}

	pr_info("\ntesting speed of multibuffer %s (%s)\n", algo,
		get_driver_name(crypto_ahash, tfm));

	for (i = 0; speed[i].blen != 0; i++) {
		/* For some reason this only tests digests. */
		if (speed[i].blen != speed[i].plen)
			continue;

		if (speed[i].blen > TVMEMSIZE * PAGE_SIZE) {
			pr_err("template (%u) too big for tvmem (%lu)\n",
			       speed[i].blen, TVMEMSIZE * PAGE_SIZE);
			goto out;
		}

		if (speed[i].klen)
			crypto_ahash_setkey(tfm, tvmem[0], speed[i].klen);

		for (k = 0; k < 8; k++)
			ahash_request_set_crypt(data[k].req, data[k].sg,
						data[k].result, speed[i].blen);

		pr_info("test%3u "
			"(%5u byte blocks,%5u bytes per update,%4u updates): ",
			i, speed[i].blen, speed[i].plen,
			speed[i].blen / speed[i].plen);

		start = get_cycles();

		for (k = 0; k < 8; k++) {
			ret = crypto_ahash_digest(data[k].req);
			if (ret == -EINPROGRESS) {
				ret = 0;
				continue;
			}

			if (ret)
				break;

			complete(&data[k].tresult.completion);
			data[k].tresult.err = 0;
		}

		for (j = 0; j < k; j++) {
			struct tcrypt_result *tr = &data[j].tresult;

			wait_for_completion(&tr->completion);
			if (tr->err)
				ret = tr->err;
		}

		end = get_cycles();
		cycles = end - start;
		pr_cont("%6lu cycles/operation, %4lu cycles/byte\n",
			cycles, cycles / (8 * speed[i].blen));

		if (ret) {
			pr_err("At least one hashing failed ret=%d\n", ret);
			break;
		}
	}

out:
	for (k = 0; k < 8; ++k)
		ahash_request_free(data[k].req);

	for (k = 0; k < 8; ++k)
		testmgr_free_buf(data[k].xbuf);

	crypto_free_ahash(tfm);

free_data:
	kfree(data);
	
	return ret;
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

static int test_ahash_speed_common(const char *algo, unsigned int secs,
				    struct t_hash_speed *speed, unsigned mask)
{
	struct scatterlist sg[TVMEMSIZE];
	struct tcrypt_result tresult;
	struct ahash_request *req;
	struct crypto_ahash *tfm;
	char *output;
	int i, ret = -ENOMEM;

	tfm = crypto_alloc_ahash(algo, 0, mask);
	if (IS_ERR(tfm)) {
		pr_err("failed to load transform for %s: %ld\n",
		       algo, PTR_ERR(tfm));
		       ret = PTR_ERR(tfm);
		return ret;
	}

	printk(KERN_INFO "\ntesting speed of async %s (%s)\n", algo,
			get_driver_name(crypto_ahash, tfm));

	if (crypto_ahash_digestsize(tfm) > MAX_DIGEST_SIZE) {
		pr_err("digestsize(%u) > %d\n", crypto_ahash_digestsize(tfm),
		       MAX_DIGEST_SIZE);
		goto out;
	}

	test_hash_sg_init(sg);
	req = ahash_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		pr_err("ahash request allocation failure\n");
		goto out;
	}

	init_completion(&tresult.completion);
	ahash_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
				   tcrypt_complete, &tresult);

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

		if (secs)
			ret = test_ahash_jiffies(req, speed[i].blen,
						 speed[i].plen, output, secs);
		else
			ret = test_ahash_cycles(req, speed[i].blen,
						speed[i].plen, output);

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


static int test_hash_speed(const char *algo, unsigned int secs,
			    struct t_hash_speed *speed)
{
	return test_ahash_speed_common(algo, secs, speed, CRYPTO_ALG_ASYNC);
}

static inline int do_one_acipher_op(struct skcipher_request *req, int ret)
{
	if (ret == -EINPROGRESS || ret == -EBUSY) {
		struct tcrypt_result *tr = req->base.data;

		wait_for_completion(&tr->completion);
		reinit_completion(&tr->completion);
		ret = tr->err;
	}

	return ret;
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

static int test_skcipher_speed(const char *algo, int enc, unsigned int secs,
				struct t_cipher_speed_template *template,
				unsigned int tcount, u8 *keysize, bool async)
{
	unsigned int ret, i, j, k, iv_len;
	struct tcrypt_result tresult;
	const char *key;
	char iv[128];
	struct skcipher_request *req;
	struct crypto_skcipher *tfm;
	const char *e;
	u32 *b_size;

	if (enc == ENCRYPT)
		e = "encryption";
	else
		e = "decryption";

	init_completion(&tresult.completion);

	tfm = crypto_alloc_skcipher(algo, 0, async ? 0 : CRYPTO_ALG_ASYNC);

	if (IS_ERR(tfm)) {
		pr_err("failed to load transform for %s: %ld\n", algo,
		       PTR_ERR(tfm));
		       ret = PTR_ERR(tfm);
		return ret;
	}

	pr_info("\ntesting speed of async %s (%s) %s\n", algo,
			get_driver_name(crypto_skcipher, tfm), e);

	req = skcipher_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		pr_err("tcrypt: skcipher: Failed to allocate request for %s\n",
		       algo);
		goto out;
	}

	skcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
				      tcrypt_complete, &tresult);

	i = 0;
	do {
		b_size = block_sizes;

		do {
			struct scatterlist sg[TVMEMSIZE];

			if ((*keysize + *b_size) > TVMEMSIZE * PAGE_SIZE) {
				pr_err("template (%u) too big for "
				       "tvmem (%lu)\n", *keysize + *b_size,
				       TVMEMSIZE * PAGE_SIZE);
				goto out_free_req;
			}

			pr_info("test %u (%d bit key, %d byte blocks): ", i,
				*keysize * 8, *b_size);

			memset(tvmem[0], 0xff, PAGE_SIZE);

			/* set key, plain text and IV */
			key = tvmem[0];
			for (j = 0; j < tcount; j++) {
				if (template[j].klen == *keysize) {
					key = template[j].key;
					break;
				}
			}

			crypto_skcipher_clear_flags(tfm, ~0);

			ret = crypto_skcipher_setkey(tfm, key, *keysize);
			if (ret) {
				pr_err("setkey() failed flags=%x\n",
					crypto_skcipher_get_flags(tfm));
				goto out_free_req;
			}

			k = *keysize + *b_size;
			sg_init_table(sg, DIV_ROUND_UP(k, PAGE_SIZE));

			if (k > PAGE_SIZE) {
				sg_set_buf(sg, tvmem[0] + *keysize,
				   PAGE_SIZE - *keysize);
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
				sg_set_buf(sg, tvmem[0] + *keysize, *b_size);
			}

			iv_len = crypto_skcipher_ivsize(tfm);
			if (iv_len)
				memset(&iv, 0xff, iv_len);

			skcipher_request_set_crypt(req, sg, sg, *b_size, iv);

			if (secs)
				ret = test_acipher_jiffies(req, enc,
							   *b_size, secs);
			else
				ret = test_acipher_cycles(req, enc,
							  *b_size);

			if (ret) {
				pr_err("%s() failed flags=%x\n", e,
				       crypto_skcipher_get_flags(tfm));
				break;
			}
			b_size++;
			i++;
		} while (*b_size);
		keysize++;
	} while (*keysize);

out_free_req:
	skcipher_request_free(req);
out:
	crypto_free_skcipher(tfm);
	
	return ret;
}

static int test_acipher_speed(const char *algo, int enc, unsigned int secs,
			       struct t_cipher_speed_template *template,
			       unsigned int tcount, u8 *keysize)
{
	return test_skcipher_speed(algo, enc, secs, template, tcount, keysize,
				   true);
}


static int do_test_alg(const char *alg)
{
	int ret = 0;
	
	char alg_name[100]="";
	//to deal cipher type as ecb mode by default
	if ( !strcmp(alg, "des") ) strcpy(alg_name, "ecb(des)");
	else if( !strcmp(alg, "arc4") ) strcpy(alg_name, "ecb(arc4)");
	else if( !strcmp(alg, "cast5") ) strcpy(alg_name, "ecb(cast5)");
	else if( !strcmp(alg, "blowfish") ) strcpy(alg_name, "ecb(blowfish)");
	else if( !strcmp(alg, "serpent") ) strcpy(alg_name, "ecb(serpent)");
	else if( !strcmp(alg, "cast6") ) strcpy(alg_name, "ecb(cast6)");
	else if( !strcmp(alg, "camellia") ) strcpy(alg_name, "ecb(camellia)");
	else if( !strcmp(alg, "aes") ) strcpy(alg_name, "ecb(aes)");
	else if( !strcmp(alg, "twofish") ) strcpy(alg_name, "ecb(twofish)");
	else if( !strcmp(alg, "des3_ede") ) strcpy(alg_name, "ecb(des3_ede)");
	else strcpy(alg_name, alg);
		
	pr_info("test alg : %s)", alg_name);

	if ( !strcmp(alg_name, "md4")
		|| !strcmp(alg_name, "md5")
		|| !strcmp(alg_name, "sha1")
		|| !strcmp(alg_name, "sha256")
		|| !strcmp(alg_name, "sha384")
		|| !strcmp(alg_name, "sha512")
		|| !strcmp(alg_name, "wp256")
		|| !strcmp(alg_name, "wp384")
		|| !strcmp(alg_name, "wp512")
		|| !strcmp(alg_name, "tgr128")
		|| !strcmp(alg_name, "tgr160")
		|| !strcmp(alg_name, "tgr192")
		|| !strcmp(alg_name, "sha224")
		|| !strcmp(alg_name, "rmd128")
		|| !strcmp(alg_name, "rmd160")
		|| !strcmp(alg_name, "rmd256")
		|| !strcmp(alg_name, "rmd320")
		|| !strcmp(alg_name, "ghash")
		|| !strcmp(alg_name, "crc32c")
		|| !strcmp(alg_name, "crct10dif")
		|| !strcmp(alg_name, "poly1305")
		|| !strcmp(alg_name, "sha3-224")
		|| !strcmp(alg_name, "sha3-256")
		|| !strcmp(alg_name, "sha3-384")
		|| !strcmp(alg_name, "sha3-512")
		|| !strcmp(alg_name, "crc32")
//		|| !strcmp(alg_name, "cbcmac(aes)")
//		|| !strcmp(alg_name, "xcbc(aes)")
//		|| !strcmp(alg_name, "cmac(aes)")
		 )
		{
			if( !strcmp(alg_name, "ghash") )
				ret = test_hash_speed("ghash-generic", sec, t_hash_speed_template_16);
			else
				ret = test_hash_speed(alg_name, sec, t_generic_hash_speed_template);
				if( !ret && ( !strcmp(alg_name, "sha1") || !strcmp(alg_name, "sha256") || !strcmp(alg_name, "sha512") ) )
					ret = test_mb_ahash_speed(alg_name, sec, t_generic_hash_speed_template);
		}
	else if( !strcmp(alg_name, "ecb(des)")
		|| !strcmp(alg_name, "cbc(des)")
		|| !strcmp(alg_name, "cfb(des)")
		|| !strcmp(alg_name, "ofb(des)")
		|| !strcmp(alg_name, "ecb(arc4)") )
		{ //t_speed_template_8
			ret = test_acipher_speed(alg_name, ENCRYPT, sec, NULL, 0,
					   t_speed_template_8);
			if( !ret && strcmp(alg_name, "ecb(arc4)") )
				ret = test_acipher_speed(alg_name, DECRYPT, sec, NULL, 0,
						   t_speed_template_8);
		}
	else if( !strcmp(alg_name, "ecb(cast5)")
		|| !strcmp(alg_name, "cbc(cast5)")
		|| !strcmp(alg_name, "ctr(cast5)") )
		{ //t_speed_template_8_16
			ret = test_acipher_speed(alg_name, ENCRYPT, sec, NULL, 0,
					   t_speed_template_8_16);
			if( !ret )
				ret = test_acipher_speed(alg_name, DECRYPT, sec, NULL, 0,
						   t_speed_template_8_16);
		}
	else if( !strcmp(alg_name, "ecb(blowfish)")
		|| !strcmp(alg_name, "cbc(blowfish)")
		|| !strcmp(alg_name, "ctr(blowfish)") )
		{ //t_speed_template_8_32
			ret = test_acipher_speed(alg_name, ENCRYPT, sec, NULL, 0,
					   t_speed_template_8_32);
			if( !ret )
				ret = test_acipher_speed(alg_name, DECRYPT, sec, NULL, 0,
						   t_speed_template_8_32);
		}
	else if( !strcmp(alg_name, "ecb(serpent)")
		|| !strcmp(alg_name, "cbc(serpent)")
		|| !strcmp(alg_name, "ctr(serpent)")
		|| !strcmp(alg_name, "ecb(cast6)")
		|| !strcmp(alg_name, "cbc(cast6)")
		|| !strcmp(alg_name, "ctr(cast6)")
		|| !strcmp(alg_name, "ecb(camellia)")
		|| !strcmp(alg_name, "cbc(camellia)")
		|| !strcmp(alg_name, "ctr(camellia)") )
		{ //t_speed_template_16_32
			ret = test_acipher_speed(alg_name, ENCRYPT, sec, NULL, 0,
					   t_speed_template_16_32);
			if( !ret )
				ret = test_acipher_speed(alg_name, DECRYPT, sec, NULL, 0,
						   t_speed_template_16_32);
		} 		
	else if( !strcmp(alg_name, "ecb(aes)")
		|| !strcmp(alg_name, "cbc(aes)")
		|| !strcmp(alg_name, "cts(cbc(aes))")
		|| !strcmp(alg_name, "ctr(aes)")
		|| !strcmp(alg_name, "cfb(aes)")
		|| !strcmp(alg_name, "ofb(aes)")
		|| !strcmp(alg_name, "ecb(twofish)")
		|| !strcmp(alg_name, "cbc(twofish)")
		|| !strcmp(alg_name, "ctr(twofish)") )
		{ //t_speed_template_16_24_32
			ret = test_acipher_speed(alg_name, ENCRYPT, sec, NULL, 0,
					   t_speed_template_16_24_32);
			if( !ret )
				ret = test_acipher_speed(alg_name, DECRYPT, sec, NULL, 0,
						   t_speed_template_16_24_32);
		}
	else if( !strcmp(alg_name, "rfc3686(ctr(aes))") )
		{ //t_speed_template_20_28_36
			ret = test_acipher_speed(alg_name, ENCRYPT, sec, NULL, 0,
					   t_speed_template_20_28_36);
			if( !ret )
				ret = test_acipher_speed(alg_name, DECRYPT, sec, NULL, 0,
						   t_speed_template_20_28_36);
		}
	else if( !strcmp(alg_name, "ecb(des3_ede)")
		|| !strcmp(alg_name, "cbc(des3_ede)")
		|| !strcmp(alg_name, "cfb(des3_ede)")
		|| !strcmp(alg_name, "ofb(des3_ede)") )
		{ //t_speed_template_24
			ret = test_acipher_speed(alg_name, ENCRYPT, sec,
					   t_des3_speed_template, T_DES3_SPEED_VECTORS,
					   t_speed_template_24);
			if( !ret )
				ret = test_acipher_speed(alg_name, DECRYPT, sec,
						   t_des3_speed_template, T_DES3_SPEED_VECTORS,
						   t_speed_template_24);
		}
	else if( !strcmp(alg_name, "lrw(aes)")
		|| !strcmp(alg_name, "lrw(twofish)") )
		{ //t_speed_template_32_40_48
			ret = test_acipher_speed(alg_name, ENCRYPT, sec, NULL, 0,
					   t_speed_template_32_40_48);
			if( !ret )
				ret = test_acipher_speed(alg_name, DECRYPT, sec, NULL, 0,
						   t_speed_template_32_40_48);
		}
	else if( !strcmp(alg_name, "lrw(serpent)")
		||  !strcmp(alg_name, "lrw(cast6)")
		||  !strcmp(alg_name, "xts(cast6)")
		||  !strcmp(alg_name, "lrw(serpent)")
		||  !strcmp(alg_name, "lrw(camellia)") )
		{ //t_speed_template_32_48
			ret = test_acipher_speed(alg_name, ENCRYPT, sec, NULL, 0,
					   t_speed_template_32_48);
			if( !ret )
				ret = test_acipher_speed(alg_name, DECRYPT, sec, NULL, 0,
						   t_speed_template_32_48);
		}
	else if( !strcmp(alg_name, "xts(twofish)") )
		{ //t_speed_template_32_48_64
			ret = test_acipher_speed(alg_name, ENCRYPT, sec, NULL, 0,
					   t_speed_template_32_48_64);
			if( !ret )
				ret = test_acipher_speed(alg_name, DECRYPT, sec, NULL, 0,
						   t_speed_template_32_48_64);
		}
	else if( !strcmp(alg_name, "xts(aes)")
		|| !strcmp(alg_name, "xts(serpent)")
		|| !strcmp(alg_name, "xts(camellia)") )
		{ //t_speed_template_32_64
			ret = test_acipher_speed(alg_name, ENCRYPT, sec, NULL, 0,
					   t_speed_template_32_64);
			if( !ret )
				ret = test_acipher_speed(alg_name, DECRYPT, sec, NULL, 0,
						   t_speed_template_32_64);
		}
	else ret = -EOPNOTSUPP;

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
			p = ENOENT; break;
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

#define MAX_CRYPTO_TEST_PARAM	1
static ssize_t crypto_test_write(struct file *file,
    const char __user *buffer, size_t count, loff_t *pos)
{
    int i = 0;
    char *buf, *tok, *tmp;
    
    if(tc_status == 1) return ETXTBSY;

    tc_status = 1;
    tc_error = 0;
    memset(tc_alg,  0, sizeof(tc_alg));
    	
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
        	tc_error = -EINVAL;
            goto out2;
        }
        strcpy(tc_alg, tok);
        while(tc_alg[strlen(tc_alg)-1] == '\r' || tc_alg[strlen(tc_alg)-1] == '\n')
        	tc_alg[strlen(tc_alg)-1] = 0;
        i++;
    }
	
    tc_error = do_test_alg(tc_alg);
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

/*
 * If an init function is provided, an exit function must also be provided
 * to allow module unload.
 */
static void __exit tcrypt_mod_fini(void)
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

module_init(tcrypt_node_init)
module_exit(tcrypt_mod_fini);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Quick & dirty crypto testing module");
MODULE_AUTHOR("James Morris <jmorris@intercode.com.au>");
