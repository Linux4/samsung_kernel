/*
 * Copyright (C) 2018 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Functions related to blkcrypt(for inline encryption) handling
 *
 * NOTE :
 * blkcrypt implementation depends on a inline encryption hardware.
 */

#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/pfk.h>

static struct kmem_cache *ice_key_cachep;

/* For ICE driver */
static void *blk_crypt_ice_alloc_aes_xts(void)
{
	struct blk_encryption_key *ice_key;
	const char *cipher_str = "xts(aes)";

	ice_key = kmem_cache_zalloc(ice_key_cachep, GFP_KERNEL);
	if (!ice_key) {
		pr_debug("error allocating blk_encryption_key '%s' transform: %d",
			cipher_str, -ENOMEM);
		return ERR_PTR(-ENOMEM);
	}

	return ice_key;
}

static void blk_crypt_ice_free_aes_xts(void *data)
{
	if (!data)
		return;

	kmem_cache_free(ice_key_cachep, (struct blk_encryption_key *)data);
}

static int blk_crypt_ice_set_key(void *data, const char *raw_key, int keysize)
{
	BUG_ON(keysize != BLK_ENCRYPTION_KEY_SIZE_AES_256_XTS);
	memcpy(((struct blk_encryption_key *)data)->raw, raw_key, keysize);
	return 0;
}

static unsigned char *blk_crypt_ice_get_key(void *data)
{
	return ((struct blk_encryption_key *)data)->raw;
}

static const struct blk_crypt_algorithm_cbs ice_hw_xts_cbs = {
	.alloc = blk_crypt_ice_alloc_aes_xts,
	.free = blk_crypt_ice_free_aes_xts,
	.get_key = blk_crypt_ice_get_key,
	.set_key = blk_crypt_ice_set_key,
};

static void *blk_crypt_handle = NULL;

/* Blk-crypt core functions */
static int __init blk_crypt_alg_ice_init(void)
{
	ice_key_cachep = KMEM_CACHE(blk_encryption_key, SLAB_RECLAIM_ACCOUNT);
	if (!ice_key_cachep)
		return -ENOMEM;

	pr_info("%s\n", __func__);
	blk_crypt_handle = blk_crypt_alg_register(NULL, "xts(aes)",
				BLK_CRYPT_MODE_INLINE_PRIVATE, &ice_hw_xts_cbs);
	if (IS_ERR(blk_crypt_handle)) {
		pr_err("%s: failed to register alg(xts(aes)), err:%d\n",
			__func__, PTR_ERR(blk_crypt_handle) );
		blk_crypt_handle = NULL;
		kmem_cache_destroy(ice_key_cachep);
	}
		
	return 0;
}

static void __exit blk_crypt_alg_ice_exit(void)
{
	pr_info("%s\n", __func__);
	blk_crypt_alg_unregister(blk_crypt_handle);
	blk_crypt_handle = NULL;
	kmem_cache_destroy(ice_key_cachep);
}

module_init(blk_crypt_alg_ice_init);
module_exit(blk_crypt_alg_ice_exit);
