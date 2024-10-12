/*
 * Exynos FMP driver
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 * Authors: Boojin Kim <boojin.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <asm/unaligned.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <linux/crypto.h>
#include <soc/samsung/exynos-smc.h>
#include <crypto/aes.h>
#include <crypto/algapi.h>

#include "fmp.h"
#include "fmp_fips_main.h"
#include "fmp_test.h"
#include "fmp_fips_info.h"

#include <linux/genhd.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>

#define WORD_SIZE 4
#define FMP_IV_MAX_IDX (FMP_IV_SIZE_16 / WORD_SIZE)

#define byte2word(b0, b1, b2, b3)       \
			(((unsigned int)(b0) << 24) | \
			((unsigned int)(b1) << 16) | \
			((unsigned int)(b2) << 8) | (b3))
#define get_word(x, c)  byte2word(((unsigned char *)(x) + 4 * (c))[0], \
				((unsigned char *)(x) + 4 * (c))[1], \
				((unsigned char *)(x) + 4 * (c))[2], \
				((unsigned char *)(x) + 4 * (c))[3])

#define FKL				BIT(26)
#define SET_KEYLEN(d, v)	((d)->des3 |= (uint32_t)v)
#define SET_FAS(d, v)		((d)->des3 = ((d)->des3 & 0xcfffffff) | v << 28)
#define GET_LENGTH(d)	((d)->des3 & 0x3ffffff)

static struct device *fmp_dev;

static inline struct exynos_fmp *get_fmp(void)
{
	return dev_get_drvdata(fmp_dev);
}

static inline void dump_ci(struct fmp_crypto_info *c)
{
	if (c) {
		pr_info
		    ("%s: algo:%d enc:%d key_size:%d\n",
		     __func__, c->algo_mode, c->enc_mode,c->key_size);
		if (c->enc_mode == EXYNOS_FMP_FILE_ENC)
			print_hex_dump(KERN_CONT, "key:",
				       DUMP_PREFIX_OFFSET, 16, 1, c->key,
				       sizeof(c->key), false);
	}
}

static inline void dump_table(struct fmp_table_setting *table)
{
	print_hex_dump(KERN_CONT, "dump:", DUMP_PREFIX_OFFSET, 16, 1,
		       table, sizeof(struct fmp_table_setting), false);
}

static inline int is_supported_ivsize(u32 ivlen)
{
	if (ivlen && (ivlen <= FMP_IV_SIZE_16))
		return TRUE;
	else
		return FALSE;
}

static inline int check_aes_xts_size(struct fmp_table_setting *table,
				     bool cmdq_enabled)
{
	int size;
	size = GET_LENGTH(table);
	return (size > MAX_AES_XTS_TRANSFER_SIZE) ? size : 0;
}

/* check for fips that no allow same keys */
static inline int check_aes_xts_key(char *key,
				    enum fmp_crypto_key_size key_size)
{
	char *enckey, *twkey;

	enckey = key;
	twkey = key + key_size;
	return (memcmp(enckey, twkey, key_size)) ? 0 : -1;
}

int fmplib_set_algo_mode(struct fmp_table_setting *table,
			 struct fmp_crypto_info *crypto, bool cmdq_enabled)
{
	int ret;
	enum fmp_crypto_fips_algo_mode algo_mode = crypto->algo_mode & EXYNOS_FMP_ALGO_MODE_MASK;

	if (algo_mode == EXYNOS_FMP_ALGO_MODE_AES_XTS) {
		ret = check_aes_xts_size(table, cmdq_enabled);
		if (ret) {
			pr_err("%s: Fail FMP XTS due to invalid size(%d), cmdq:%d\n",
			       __func__, ret, cmdq_enabled);
			return -EINVAL;
		}
	}

	switch (crypto->enc_mode) {
	case EXYNOS_FMP_FILE_ENC:
		SET_FAS(table, algo_mode);
		break;
	default:
		pr_err("%s: Invalid fmp enc mode %d\n", __func__,
		       crypto->enc_mode);
		return -EINVAL;
	}
	return 0;
}

static inline __be32 fmp_key_word(const u8 *key, int j)
{
	return cpu_to_be32(get_unaligned_le32(
			key + AES_KEYSIZE_256 - (j + 1) * sizeof(__le32)));
}

static int fmplib_set_file_key(struct fmp_table_setting *table,
			struct fmp_crypto_info *crypto)
{
	enum fmp_crypto_fips_algo_mode algo_mode = crypto->algo_mode & EXYNOS_FMP_ALGO_MODE_MASK;
	enum fmp_crypto_key_size key_size = crypto->fmp_key_size;
	char *key = crypto->key;
	int idx, max;

	if (!key || (crypto->enc_mode != EXYNOS_FMP_FILE_ENC) ||
		((key_size != EXYNOS_FMP_KEY_SIZE_16) &&
		 (key_size != EXYNOS_FMP_KEY_SIZE_32))) {
		pr_err("%s: Invalid key_size:%d enc_mode:%d\n",
		       __func__, key_size, crypto->enc_mode);
		return -EINVAL;
	}

	if ((algo_mode == EXYNOS_FMP_ALGO_MODE_AES_XTS)
	    && check_aes_xts_key(key, key_size)) {
		pr_err("%s: Fail FMP XTS due to the same enc and twkey\n",
		       __func__);
		return -EINVAL;
	}

	if (algo_mode == EXYNOS_FMP_ALGO_MODE_AES_CBC) {
		max = key_size / WORD_SIZE;
		for (idx = 0; idx < max; idx++)
			*(&table->file_enckey0 + idx) =
			    get_word(key, max - (idx + 1));
	} else if (algo_mode == EXYNOS_FMP_ALGO_MODE_AES_XTS) {
		key_size *= 2;
		max = key_size / WORD_SIZE;
		for (idx = 0; idx < (max / 2); idx++)
			*(&table->file_enckey0 + idx) =
			    get_word(key, (max / 2) - (idx + 1));
		for (idx = 0; idx < (max / 2); idx++)
			*(&table->file_twkey0 + idx) =
			    get_word(key, max - (idx + 1));
	}
	return 0;
}

static int fmplib_set_key_size(struct fmp_table_setting *table,
			struct fmp_crypto_info *crypto, bool cmdq_enabled)
{
	enum fmp_crypto_key_size key_size;

	key_size = crypto->fmp_key_size;

	if ((key_size != EXYNOS_FMP_KEY_SIZE_16) &&
		(key_size != EXYNOS_FMP_KEY_SIZE_32)) {
		pr_err("%s: Invalid keysize %d\n", __func__, key_size);
		return -EINVAL;
	}

	switch (crypto->enc_mode) {
	case EXYNOS_FMP_FILE_ENC:
		SET_KEYLEN(table,
			   (key_size == FMP_KEY_SIZE_32) ? FKL : 0);
		break;
	default:
		pr_err("%s: Invalid fmp enc mode %d\n", __func__,
		       crypto->enc_mode);
		return -EINVAL;
	}
	return 0;
}

static int fmplib_set_iv(struct fmp_table_setting *table,
		  struct fmp_crypto_info *crypto, u8 *iv)
{
	int idx;

	switch (crypto->enc_mode) {
	case EXYNOS_FMP_FILE_ENC:
		for (idx = 0; idx < FMP_IV_MAX_IDX; idx++)
			*(&table->file_iv0 + idx) =
			    get_word(iv, FMP_IV_MAX_IDX - (idx + 1));
		break;
	default:
		pr_err("%s: Invalid fmp enc mode %d\n", __func__,
		       crypto->enc_mode);
		return -EINVAL;
	}
	return 0;
}

int exynos_fmp_crypt(struct exynos_fmp_crypt_info *fmp_ci, struct fmp_table_setting *table)
{
	struct exynos_fmp *fmp = get_fmp();
	struct fmp_crypto_info *ci;
	struct fmp_sg_entry *ent = (struct fmp_sg_entry *)table;
	u8 iv[FMP_IV_SIZE_16];
	u64 ret = 0;
	size_t j, limit;

	if (!fmp) {
		pr_err("%s: invalid fmp\n", __func__);
		return -EINVAL;
	}

	if (!table) {
		pr_err("%s: invalid table setting\n", __func__);
		return -EINVAL;
	}

	if (fmp_ci->fips) {
		/* check fips test data */
		if (!fmp->test_data) {
			dev_err(fmp->dev, "%s:  no test_data for test mode\n", __func__);
			return -EINVAL;
		}

		ci = &fmp->test_data->ci;
		if (!ci || !(ci->algo_mode & EXYNOS_FMP_ALGO_MODE_TEST)) {
			dev_err(fmp->dev, "%s: Invalid fmp crypto_info for test mode\n", __func__);
			return -EINVAL;
		}

		if (!(ci->algo_mode & EXYNOS_FMP_ALGO_MODE_MASK)) {
			dev_err(fmp->dev, "%s: no test_data for algo mode\n", __func__);
			return -EINVAL;
		}

		ret = fmplib_set_algo_mode(table, ci, CMDQ_DISABLED);
		if (ret) {
			dev_err(fmp->dev, "%s: Fail to set FMP encryption mode\n", __func__);
			ret = -EINVAL;
			goto out;
		}

		/* set key into table */
		switch (ci->enc_mode) {
		case EXYNOS_FMP_FILE_ENC:
			ret = fmplib_set_file_key(table, ci);
			if (ret) {
				dev_err(fmp->dev, "%s: Fail to set FMP key\n", __func__);
				ret = -EINVAL;
				goto out;
			}
			break;
		default:
			dev_err(fmp->dev, "%s: Invalid fmp enc mode %d\n", __func__, ci->enc_mode);
			ret = -EINVAL;
			goto out;
		}

		/* set key size into table */
		ret = fmplib_set_key_size(table, ci, CMDQ_DISABLED);
		if (ret) {
			dev_err(fmp->dev, "%s: Fail to set FMP key size\n", __func__);
			goto out;
		}

		/* use test manager's iv instead of host driver's iv */
		dev_dbg(fmp->dev, "%s: fips crypt: ivsize:%d, key_size: %d, %d\n",
				__func__, sizeof(fmp->test_data->iv), ci->key_size, ci->fmp_key_size);

		if (!is_supported_ivsize(sizeof(fmp->test_data->iv))) {
			dev_err(fmp->dev, "%s: invalid (mode:%d ivsize:%d)\n",
					 __func__, ci->algo_mode & EXYNOS_FMP_ALGO_MODE_MASK,
					sizeof(fmp->test_data->iv));
			ret = -EINVAL;
			goto out;
		}

		/* set iv as intended size */
		memset(iv, 0, FMP_IV_SIZE_16);
		memcpy(iv, fmp->test_data->iv, sizeof(fmp->test_data->iv));
		ret = fmplib_set_iv(table, ci, iv);
		if (ret) {
			dev_err(fmp->dev, "%s: Fail to set FMP IV\n", __func__);
			ret = -EINVAL;
			goto out;
		}
	} else {
		if (!crypto_memneq(fmp_ci->enckey, fmp_ci->twkey, AES_KEYSIZE_256)) {
			dev_err(fmp->dev, "Can't use weak AES-XTS key\n");
			ret = -EINVAL;
			goto out;
		}

		SET_FAS(ent, EXYNOS_FMP_ALGO_MODE_AES_XTS);
		SET_KEYLEN(ent, FKL);

		/* set the file/tweak key in table */
		for (j = 0, limit = AES_KEYSIZE_256 / sizeof(u32); j < limit; j++) {
			ent->file_enckey[j] = fmp_key_word(fmp_ci->enckey, j);
			ent->file_twkey[j] = fmp_key_word(fmp_ci->twkey, j);
		}

		/* Set the IV. */
		ent->file_iv[0] = cpu_to_be32(upper_32_bits(fmp_ci->dun_hi));
		ent->file_iv[1] = cpu_to_be32(lower_32_bits(fmp_ci->dun_hi));
		ent->file_iv[2] = cpu_to_be32(upper_32_bits(fmp_ci->dun_lo));
		ent->file_iv[3] = cpu_to_be32(lower_32_bits(fmp_ci->dun_lo));
	}

out:
	if (ret) {
		dump_ci(ci);
		dump_table(table);
	}
	return ret;
}
EXPORT_SYMBOL(exynos_fmp_crypt);

static bool fmp_check_fips(struct bio *bio, struct exynos_fmp *fmp)
{
	struct page *page;
	struct buffer_head *bh;
	bool find = false;
	struct fmp_crypto_info *ci;

	if (bio->bi_io_vec)  {
		page = bio->bi_io_vec[0].bv_page;
		if (page && !PageAnon(page) && page_has_buffers(page)) {
			bh = page_buffers(page);
			if (bh != fmp->bh) {
				dev_err(fmp->dev, "%s: invalid fips bh\n", __func__);
				return false;
			}

			if (bh && ((void *)bh->b_private == (void *)fmp))
				find = true;
		}
	}

	if (find) {
		fmp->fips_fin++;
		ci = &fmp->test_data->ci;
		dev_dbg(fmp->dev, "%s: find fips run(%d) with algo:%d, enc:%d, key_size:%d\n",
			__func__, fmp->fips_run, ci->algo_mode, ci->enc_mode, ci->key_size);
		if (ci->algo_mode == (EXYNOS_FMP_ALGO_MODE_TEST | EXYNOS_FMP_BYPASS_MODE)) {
			dev_dbg(fmp->dev, "%s: find fips for bypass mode\n", __func__);
			return 0;
		}
	}

	return find;
}

bool is_fmp_fips_op(struct bio *bio)
{
	struct exynos_fmp *fmp = get_fmp();

	if (!fmp->fips_run)
		return false;

	if (fmp_check_fips(bio, fmp)) {
		dev_dbg(fmp->dev, "%s: find fips\n", __func__);
		return true;
	}

	return false;
}
EXPORT_SYMBOL(is_fmp_fips_op);


static void *exynos_fmp_init(struct platform_device *pdev)
{
	struct exynos_fmp *fmp;
	int ret = 0;

	if (!pdev) {
		pr_err("%s: Invalid platform_device.\n", __func__);
		goto err;
	}

	fmp = devm_kzalloc(&pdev->dev, sizeof(struct exynos_fmp), GFP_KERNEL);
	if (!fmp)
		goto err;

	fmp->dev = &pdev->dev;
	if (!fmp->dev) {
		pr_err("%s: Invalid device.\n", __func__);
		goto err_dev;
	}

	atomic_set(&fmp->fips_start, 0);

	ret = exynos_fmp_fips_register(fmp);
	if (ret) {
		dev_err(fmp->dev, "%s: Fail to exynos_fmp_fips_register. ret(0x%x)",
				__func__, ret);
		goto err_dev;
	}

	dev_info(fmp->dev, "Exynos FMP driver is initialized\n");
	return fmp;

err_dev:
	devm_kfree(&pdev->dev, fmp);
err:
	return NULL;
}

void exynos_fmp_exit(struct platform_device *pdev)
{
	struct exynos_fmp *fmp = dev_get_drvdata(&pdev->dev);

	exynos_fmp_fips_deregister(fmp);
	devm_kfree(&pdev->dev, fmp);
}

static int exynos_fmp_probe(struct platform_device *pdev)
{
	struct exynos_fmp *fmp_ctx = exynos_fmp_init(pdev);

	if (!fmp_ctx) {
		dev_err(&pdev->dev,
			"%s: Fail to get fmp_ctx\n", __func__);
		return -EINVAL;
	}

	fmp_dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, fmp_ctx);

	return 0;
}

static int exynos_fmp_remove(struct platform_device *pdev)
{
	exynos_fmp_exit(pdev);
	return 0;
}

static const struct of_device_id exynos_fmp_match[] = {
	{ .compatible = "samsung,exynos-fmp" },
	{},
};

static struct platform_driver exynos_fmp_driver = {
	.driver = {
		   .name = "exynos-fmp",
			.owner = THIS_MODULE,
			.pm = NULL,
		   .of_match_table = exynos_fmp_match,
		   },
	.probe = exynos_fmp_probe,
	.remove = exynos_fmp_remove,
};
module_platform_driver(exynos_fmp_driver);

MODULE_DESCRIPTION("FMP driver");
MODULE_AUTHOR("Hyeyeon Chung <hyeon.chung@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
