// SPDX-License-Identifier: GPL-2.0-only
/*
 * Crypto TZ library for storage encryption.
 *
 * Copyright (c) 2020-2021, Linux Foundation. All rights reserved.
 */

#include <asm/cacheflush.h>
#include <linux/qcom_scm.h>
#include <linux/qtee_shmbridge.h>
#include <linux/crypto-qti-common.h>
#include <linux/module.h>
#include <linux/of.h>
#include "crypto-qti-platform.h"

#define ICE_CIPHER_MODE_XTS_256 3
#define UFS_CE 10
#define SDCC_CE 20
#define UFS_CARD_CE 30

static bool is_boot_dev_type_emmc(void)
{
	struct device_node *np;
	const char *bootparams;

	np = of_find_node_by_path("/chosen");
	of_property_read_string(np, "bootargs", &bootparams);
	if (!bootparams)
		pr_err("%s: failed to get bootargs property\n", __func__);
	else if (strnstr(bootparams, "androidboot.bootdevice",
			strlen(bootparams)) &&
			strnstr(bootparams, "sdhci", strlen(bootparams)))
		return true;

	return false;

}

int crypto_qti_program_key(struct crypto_vops_qti_entry *ice_entry,
			   const struct blk_crypto_key *key, unsigned int slot,
			   unsigned int data_unit_mask, int capid)
{
	int err = 0;
	int i;
	struct qtee_shm shm;
	union {
		u8 bytes[BLK_CRYPTO_MAX_WRAPPED_KEY_SIZE];
		u32 words[BLK_CRYPTO_MAX_WRAPPED_KEY_SIZE / sizeof(u32)];
	} key_new;

	err = qtee_shmbridge_allocate_shm(key->size, &shm);
	if (err)
		return -ENOMEM;

	memcpy(key_new.bytes, key->raw, key->size);
	if (!key->is_hw_wrapped) {
		for (i = 0; i < ARRAY_SIZE(key_new.words); i++)
			__cpu_to_be32s(&key_new.words[i]);
	}

	memcpy(shm.vaddr, key_new.bytes, key->size);
	qtee_shmbridge_flush_shm_buf(&shm);

	if (is_boot_dev_type_emmc())
		err = qcom_scm_config_set_ice_key(slot, shm.paddr, key->size,
						ICE_CIPHER_MODE_XTS_256,
						data_unit_mask, SDCC_CE);
	else
		err = qcom_scm_config_set_ice_key(slot, shm.paddr, key->size,
						ICE_CIPHER_MODE_XTS_256,
						data_unit_mask, UFS_CE);
	if (err)
		pr_err("%s:SCM call Error: 0x%x slot %d\n",
				__func__, err, slot);

	qtee_shmbridge_inv_shm_buf(&shm);
	qtee_shmbridge_free_shm(&shm);

	return err;
}
EXPORT_SYMBOL(crypto_qti_program_key);

int crypto_qti_invalidate_key(struct crypto_vops_qti_entry *ice_entry,
			      unsigned int slot)
{
	int err = 0;

	if (is_boot_dev_type_emmc())
		err = qcom_scm_clear_ice_key(slot, SDCC_CE);
	else
		err = qcom_scm_clear_ice_key(slot, UFS_CE);

	if (err)
		pr_err("%s:SCM call Error: 0x%x\n", __func__, err);

	return err;
}
EXPORT_SYMBOL(crypto_qti_invalidate_key);

int crypto_qti_derive_raw_secret_platform(
				struct crypto_vops_qti_entry *ice_entry,
				const u8 *wrapped_key,
				unsigned int wrapped_key_size, u8 *secret,
				unsigned int secret_size)
{
	memcpy(secret, wrapped_key, secret_size);
	return 0;
}
EXPORT_SYMBOL(crypto_qti_derive_raw_secret_platform);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Crypto TZ library for storage encryption");
