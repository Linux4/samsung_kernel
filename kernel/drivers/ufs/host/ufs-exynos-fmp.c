// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Exynos FMP (Flash Memory Protector) UFS crypto support
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 * Copyright 2020 Google LLC
 *
 * Authors: Hyeyeon Chung <hyeon.chung@samsung.com>
 *	    Boojin Kim <boojin.kim@samsung.com>
 *	    Eric Biggers <ebiggers@google.com>
 */

#include <soc/samsung/exynos-smc.h>
#include <linux/of.h>
#include <linux/delay.h>

#include <ufs/ufshcd.h>
#include "../core/ufshcd-crypto.h"

#include "ufs-vs-mmio.h"
#include "ufs-cal-if.h"
#include "ufs-exynos.h"
#include "ufs-exynos-fmp.h"

#ifdef CONFIG_KEYS_IN_PRDT
#include <asm/unaligned.h>
#include <crypto/aes.h>
#include <crypto/algapi.h>
#include <trace/hooks/ufshcd.h>
#endif
#ifdef CONFIG_EXYNOS_FMP_FIPS
#include "../../crypto/fmp/fmp_fips.h"
#endif

#ifdef CONFIG_HW_KEYS_IN_CUSTOM_KEYSLOT
static struct device dev_fmp;
#endif
#define MAX_RETRY_COUNT 0x100

int exynos_ufs_fmp_check_selftest(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	u32 selftest_stat0;
	u32 selftest_stat1;
	u32 count = 0;
	int ret = 0;

	do {
		selftest_stat0 = ufsp_readl(&ufs->handle, FMP_SELFTESTSTAT0);
		if (!(selftest_stat0 & FMP_SELF_TEST_DONE)) {
			udelay(20);
			count++;
			continue;
		}
		else {
			break;
		}
	} while (count < MAX_RETRY_COUNT);

	if (!(selftest_stat0 & FMP_SELF_TEST_DONE)) {
		ret = -ETIMEDOUT;
		dev_err(hba->dev, "FMP selftest timeout\n");
		goto err;
	}

	/* write to clear */
	ufsp_writel(&ufs->handle, FMP_SELF_TEST_DONE, FMP_SELFTESTSTAT0);

	selftest_stat1 = ufsp_readl(&ufs->handle, FMP_SELFTESTSTAT1);
	if (selftest_stat1 & FMP_SELF_TEST_FAIL) {
		ret = -EINVAL;
		dev_err(hba->dev, "FMP selftest failed\n");
		goto err;
	}

	dev_info(hba->dev, "FMP selftest success\n");
err:
	return ret;
}

static inline u32 check_keyvalid(struct ufs_hba *hba, int slot)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	u32 kw_keyvalid = 0;
	u32 count = 0;

	/* Keyslot should be valid for crypto IO */
	do {
		kw_keyvalid = ufsp_readl(&ufs->handle, FMP_KW_KEYVALID);
		if (!(kw_keyvalid & (0x1 << slot))) {
			dev_warn(hba->dev, "Key slot #%d is not valid yet\n", slot);
			udelay(2);
			count++;
			continue;
		} else {
			break;
		}
	} while (count < MAX_RETRY_COUNT);

	return 	kw_keyvalid;
}

#ifdef CONFIG_KEYS_IN_PRDT
static inline __be32 fmp_key_word(const u8 *key, int j)
{
	return cpu_to_be32(get_unaligned_le32(
			key + AES_KEYSIZE_256 - (j + 1) * sizeof(__le32)));
}

/* Configure FMP on requests that have an encryption context. */
static void exynos_ufs_fmp_fill_prdt(void *ignore, struct ufs_hba *hba, struct ufshcd_lrb *lrbp,
					unsigned int segments, int *err)
{
#ifdef CONFIG_EXYNOS_FMP_FIPS
	struct exynos_fmp_crypt_info fmp_ci;
	int ret;
#endif
	const struct bio_crypt_ctx *bc;
	struct request *rq;
	const u8 *enckey, *twkey;
	u64 dun_lo, dun_hi;
	struct fmp_sg_entry *table;
	unsigned int i;

	/*
	 * There's nothing to do for unencrypted requests, since the mode field
	 * ("FAS") is already 0 (FMP_BYPASS_MODE) by default, as it's in the
	 * same word as ufshcd_sg_entry::size which was already initialized.
	 */
	rq = scsi_cmd_to_rq(lrbp->cmd);
	bc = rq->crypt_ctx;
	BUILD_BUG_ON(FMP_BYPASS_MODE != 0);

#ifdef CONFIG_EXYNOS_FMP_FIPS
	if (!bc) {
		if (!(rq->bio))
			return;

		if (is_fmp_fips_op(rq->bio)) {
			fmp_ci.fips = true;
			goto encrypt;
		}
		return;
	}

	enckey = bc->bc_key->raw;
	twkey = enckey + AES_KEYSIZE_256;
	dun_lo = bc->bc_dun[0];
	dun_hi = bc->bc_dun[1];
	fmp_ci.fips = false;
encrypt:
#else /* CONFIG_EXYNOS_FMP_FIPS */
	if (!bc)
		return;
#endif /* CONFIG_EXYNOS_FMP_FIPS */

	/* If FMP wasn't enabled, we shouldn't get any encrypted requests. */
	if (WARN_ON_ONCE(!(hba->caps & UFSHCD_CAP_CRYPTO))) {
		dev_err(hba->dev, "UFSHCD_CAP_CRYPTO is not enabled\n");
		*err = -ENOTSUPP;
		return;
	}

#ifdef CONFIG_EXYNOS_FMP_FIPS
	table = (struct fmp_sg_entry *)lrbp->ucd_prdt_ptr;
	for (i = 0; i < segments; i++) {
		struct fmp_sg_entry *ent = &table[i];
		struct ufshcd_sg_entry *prd = (struct ufshcd_sg_entry *)ent;

		/* Each segment must be exactly one data unit. */
		if (le32_to_cpu(prd->size) + 1 != FMP_DATA_UNIT_SIZE) {
			dev_err(hba->dev, "scatterlist segment is misaligned for FMP\n");
			*err = -EINVAL;
			return;
		}

		fmp_ci.enckey = enckey;
		fmp_ci.twkey = twkey;
		fmp_ci.dun_lo = dun_lo;
		fmp_ci.dun_hi = dun_hi;

		ret = exynos_fmp_crypt(&fmp_ci, (void *)ent);
		if (ret) {
			dev_err(hba->dev, "%s: fails to crypt fmp. ret:%d\n", __func__, ret);
			*err = ret;
			return;
		}

		/* Increment the data unit number. */
		dun_lo++;
		if (dun_lo == 0)
			dun_hi++;
	}
#else
	enckey = bc->bc_key->raw;
	twkey = enckey + AES_KEYSIZE_256;
	dun_lo = bc->bc_dun[0];
	dun_hi = bc->bc_dun[1];

	/* Reject weak AES-XTS keys. */
	if (!crypto_memneq(enckey, twkey, AES_KEYSIZE_256)) {
		dev_err(hba->dev, "Can't use weak AES-XTS key\n");
		*err = -EINVAL;
		return;
	}

	/* Configure FMP on each segment of the request. */
	table = (struct fmp_sg_entry *)lrbp->ucd_prdt_ptr;
	for (i = 0; i < segments; i++) {
		struct fmp_sg_entry *ent = &table[i];
		struct ufshcd_sg_entry *prd = (struct ufshcd_sg_entry *)ent;
		int j;

		/* Each segment must be exactly one data unit. */
		if (le32_to_cpu(prd->size) + 1 != FMP_DATA_UNIT_SIZE) {
			dev_err(hba->dev,
				"scatterlist segment is misaligned for FMP\n");
			*err = -EINVAL;
			return;
		}

		/* Set the algorithm and key length. */
		SET_FAS(ent, FMP_ALGO_MODE_AES_XTS);
		SET_KEYLEN(ent, FKL);

		/* Set the key. */
		for (j = 0; j < AES_KEYSIZE_256 / sizeof(u32); j++) {
			ent->file_enckey[j] = fmp_key_word(enckey, j);
			ent->file_twkey[j] = fmp_key_word(twkey, j);
		}

		/* Set the IV. */
		ent->file_iv[0] = cpu_to_be32(upper_32_bits(dun_hi));
		ent->file_iv[1] = cpu_to_be32(lower_32_bits(dun_hi));
		ent->file_iv[2] = cpu_to_be32(upper_32_bits(dun_lo));
		ent->file_iv[3] = cpu_to_be32(lower_32_bits(dun_lo));

		/* Increment the data unit number. */
		dun_lo++;
		if (dun_lo == 0)
			dun_hi++;
	}
#endif
	return;
}

void exynos_ufs_fmp_init(struct ufs_hba *hba)
{
	unsigned long ret;
	int err = 0;

	dev_info(hba->dev, "Exynos FMP Version: %s\n", FMP_DRV_VERSION);
	dev_info(hba->dev, "KEYS_IN_PRDT FMP_MODE: %d\n", FMP_MODE);

	ret = exynos_smc(SMC_CMD_FMP, 0, FMP_EMBEDDED, FMP_MODE);
	if (ret) {
		dev_warn(hba->dev, "SMC_CMD_FMP failed on init: %ld\n",ret);
		goto disable;
	}
	else
		hba->sg_entry_size = sizeof(struct fmp_sg_entry);

	/* Advertise crypto support to ufshcd-core. */
	hba->caps |= UFSHCD_CAP_CRYPTO;

	/* Advertise crypto android_quirks to ufshcd-core. */
	hba->android_quirks |= UFSHCD_ANDROID_QUIRK_CUSTOM_CRYPTO_PROFILE;
	hba->android_quirks |= UFSHCD_ANDROID_QUIRK_KEYS_IN_PRDT;

	dev_info(hba->dev, "Exynos FMP android_quirks: 0x%x\n", hba->android_quirks);

	/* Advertise crypto capabilities to the block layer. */
	err = devm_blk_crypto_profile_init(
			hba->dev, &hba->crypto_profile, 0);
	if (err) {
		dev_err(hba->dev, "devm_blk_crypto_profile_init fail = %d", err);
		goto disable;
	}

	hba->crypto_profile.max_dun_bytes_supported = AES_BLOCK_SIZE;
	hba->crypto_profile.key_types_supported = BLK_CRYPTO_KEY_TYPE_STANDARD;
	hba->crypto_profile.dev = hba->dev;
	hba->crypto_profile.modes_supported[BLK_ENCRYPTION_MODE_AES_256_XTS] =
		FMP_DATA_UNIT_SIZE;

	register_trace_android_vh_ufs_fill_prdt(exynos_ufs_fmp_fill_prdt, NULL);
	return;

disable:
	dev_warn(hba->dev, "FMP: Disabling inline encryption support\n");
	hba->caps &= ~UFSHCD_CAP_CRYPTO;
}
#endif /* CONFIG_KEYS_IN_PRDT */
#ifdef CONFIG_HW_KEYS_IN_CUSTOM_KEYSLOT
static int fmp_program_key(struct ufs_hba *hba,
			const u32* fmp_key,
			const union exynos_ufs_crypto_cfg_entry *cfg, int slot, bool evict)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	unsigned int num_keyslots = hba->crypto_capabilities.config_count;
	size_t i, limit;
	u32 kw_keyvalid;
	u32 kw_tagabort;
	u32 kw_pbkabort;
	u32 kw_control;

	u32 slot_offset = FMP_KW_SECUREKEY + slot*FMP_KW_SECUREKEY_OFFSET;
	u32 tag_offset = FMP_KW_TAG + slot*FMP_KW_TAG_OFFSET;
	u32 cryptocfg_offset = CRYPTOCFG + slot*CRYPTOCFG_OFFSET;

	if (evict)
		dev_info(hba->dev, "fmp_evict_key slot = %d/%d\n", slot, num_keyslots);
	else
		dev_info(hba->dev, "fmp_program_key slot = %d/%d\n", slot, num_keyslots);

	/* Ensure that CFGE is cleared before programming the key */
	hci_writel(&ufs->handle, 0, cryptocfg_offset);

	/* Key program in keyslot */
	/* Swap File key and Tweak key */
	for (i = 0, limit = AES_256_XTS_KEY_SIZE / (sizeof(u32) * 2); i < limit; i++) {
		ufsp_writel(&ufs->handle, le32_to_cpu(fmp_key[i]),
			slot_offset + ((i + AES_256_XTS_TWK_OFFSET) * sizeof(u32)));
		ufsp_writel(&ufs->handle, le32_to_cpu(fmp_key[i + AES_256_XTS_TWK_OFFSET]),
			slot_offset + (i * sizeof(u32)));
	}

	/* KEY TAG */
	for (i = 0, limit = AES_GCM_TAG_SIZE / sizeof(u32); i < limit; i++) {
		ufsp_writel(&ufs->handle, le32_to_cpu(fmp_key[i + HW_WRAPPED_KEY_TAG_OFFSET]),
			tag_offset + (i * sizeof(u32)));
	}

	if(!evict) {
		/* Unwrap command */
		kw_control = (1<<slot);
		ufsp_writel(&ufs->handle, kw_control, FMP_KW_CONTROL);

		kw_keyvalid = check_keyvalid(hba, slot);
		if (!(kw_keyvalid & (0x1 << slot))) {
			dev_err(hba->dev, "Key slot #%d is not valid\n", slot);

			kw_tagabort = ufsp_readl(&ufs->handle, FMP_KW_TAGABORT);
			kw_pbkabort = ufsp_readl(&ufs->handle, FMP_KW_PBKABORT);
			dev_err(hba->dev, "tag abort = 0x%x, pbk abort = 0x%x\n", kw_tagabort, kw_pbkabort);

			// set to clear
			if (kw_tagabort)
				ufsp_writel(&ufs->handle, 1 << slot, FMP_KW_TAGABORT);
			if (kw_pbkabort)
				ufsp_writel(&ufs->handle, 1, FMP_KW_PBKABORT);

			kw_tagabort = ufsp_readl(&ufs->handle, FMP_KW_TAGABORT);
			kw_pbkabort = ufsp_readl(&ufs->handle, FMP_KW_PBKABORT);
			dev_err(hba->dev, "set to clear tag abort = 0x%x, pbk abort = 0x%x\n", kw_tagabort, kw_pbkabort);
			return -EINVAL;
		}
		dev_info(hba->dev, "%s Key valid = 0x%x\n", __func__, kw_keyvalid);
	}

	/* Dword 16 must be written last */
	hci_writel(&ufs->handle, le32_to_cpu(cfg->reg_val), cryptocfg_offset);

	/* To Do
	 * This should be fixed to return by goto label in error cases if ufshcd control is needed.
	 */
	return 0;
}

static int exynos_ufs_fmp_keyslot_program(struct blk_crypto_profile *profile,
					 const struct blk_crypto_key *key,
					 unsigned int slot)
{
	struct ufs_hba *hba = container_of(profile, struct ufs_hba, crypto_profile);
	u32 secret_size = BLK_CRYPTO_SW_SECRET_SIZE + AES_GCM_TAG_SIZE;
	union exynos_ufs_crypto_cfg_entry cfg = {};
	union {
		u8 bytes[AES_256_XTS_KEY_SIZE + AES_GCM_TAG_SIZE];
		u32 words[(AES_256_XTS_KEY_SIZE + AES_GCM_TAG_SIZE) / sizeof(u32)];
	} fmp_key;

	int err = 0;

	/* Only AES-256-XTS is supported */
	if (key->crypto_cfg.crypto_mode != BLK_ENCRYPTION_MODE_AES_256_XTS ||
		(key->crypto_cfg.data_unit_size != FMP_DATA_UNIT_SIZE) ||
		(key->crypto_cfg.key_type != BLK_CRYPTO_KEY_TYPE_HW_WRAPPED) ||
		((key->size - secret_size)!= (AES_256_XTS_KEY_SIZE + AES_GCM_TAG_SIZE))) {
		dev_err(hba->dev,
			"Unhandled crypto capability slot = %d: crypto_mode=%d, key_size=%d, data_unit_size=%d, key_type=%d\n",
			slot, key->crypto_cfg.crypto_mode, key->size - secret_size,
			key->crypto_cfg.data_unit_size, key->crypto_cfg.key_type);
		return -EINVAL;
	}

	/* In XTS mode, the blk_crypto_key's size is already doubled */
	memcpy(fmp_key.bytes, key->raw, key->size - secret_size);

	/* Set crypto config register.*/
	cfg.data_unit_size = FMP_DATA_UNIT_SIZE / 512;
	cfg.crypto_cap_idx = 1;
	cfg.config_enable = UFS_CRYPTO_CONFIGURATION_ENABLE;

	err = fmp_program_key(hba, fmp_key.words, &cfg, slot, false);

	memzero_explicit(&fmp_key, key->size - secret_size);
	return err;
}

static int exynos_ufs_fmp_keyslot_evict(struct blk_crypto_profile *profile,
					const struct blk_crypto_key *key,
					unsigned int slot)
{
	struct ufs_hba *hba = container_of(profile, struct ufs_hba, crypto_profile);
	union exynos_ufs_crypto_cfg_entry cfg = {};

	u32 fmp_key[(AES_256_XTS_KEY_SIZE + AES_GCM_TAG_SIZE) / sizeof(u32)] = {};
	int err = 0;

	err = fmp_program_key(hba, fmp_key, &cfg, slot, true);

	return err;
}

static int exynos_ufs_fmp_derive_sw_secret(struct blk_crypto_profile *profile,
				const u8 *wrapped_key,
				size_t wrapped_key_size,
				u8 sw_secret[BLK_CRYPTO_SW_SECRET_SIZE])
{
	struct ufs_hba *hba = container_of(profile, struct ufs_hba, crypto_profile);
	unsigned long smc_ret = 0;
	int ret = 0;
	u8 *fmp_key;
	dma_addr_t fmp_key_pa;
	u8 *secret_key;
	dma_addr_t secret_key_pa;

	/* Allocate key buffer as non-cacheable */
	fmp_key = dmam_alloc_coherent(&dev_fmp, wrapped_key_size, &fmp_key_pa, GFP_KERNEL);
	if (!fmp_key) {
		dev_err(hba->dev, "Fail to allocate memory for fmp_key\n");
		ret = -ENOMEM;
		goto out;
	}

	/* Allocate secret buffer as non-cacheable */
	secret_key = dmam_alloc_coherent(&dev_fmp, BLK_CRYPTO_SW_SECRET_SIZE, &secret_key_pa, GFP_KERNEL);
	if (!secret_key) {
		dev_err(hba->dev, "Fail to allocate memory for secret_key\n");
		ret = -ENOMEM;
		goto free_fmp_key;
	}

	memcpy(fmp_key, wrapped_key, wrapped_key_size);

	smc_ret = exynos_smc(SMC_CMD_FMP_SECRET, fmp_key_pa, secret_key_pa, BLK_CRYPTO_SW_SECRET_SIZE);
	if (smc_ret) {
		dev_err(hba->dev, "SMC_CMD_FMP_SECRET failed: %ld\n", smc_ret);
		ret = -EINVAL;
		goto free_secret_key;
	}

	memcpy(sw_secret, secret_key, BLK_CRYPTO_SW_SECRET_SIZE);

free_secret_key:
	memzero_explicit(secret_key, BLK_CRYPTO_SW_SECRET_SIZE);
	dmam_free_coherent(&dev_fmp, BLK_CRYPTO_SW_SECRET_SIZE, secret_key, secret_key_pa);
free_fmp_key:
	memzero_explicit(fmp_key, wrapped_key_size);
	dmam_free_coherent(&dev_fmp, wrapped_key_size, fmp_key, fmp_key_pa);
out:
	return ret;
}

static const struct blk_crypto_ll_ops exynos_ufs_fmp_crypto_ll_ops = {
	.keyslot_program	= exynos_ufs_fmp_keyslot_program,
	.keyslot_evict		= exynos_ufs_fmp_keyslot_evict,
	.derive_sw_secret	= exynos_ufs_fmp_derive_sw_secret,
};

/**
 * exynos_ufs_fmp_init_crypto_capabilities - Read crypto capabilities, init crypto
 *					 fields in hba
 * @hba: Per adapter instance
 *
 * Return: 0 if crypto was initialized else a -errno value.
 */
int exynos_ufs_fmp_init_crypto_capabilities(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int err = 0;

	if (!(hba->android_quirks & UFSHCD_ANDROID_QUIRK_CUSTOM_CRYPTO_PROFILE)) {
		err = -EINVAL;
		goto out;
	}

	/*
	 * Don't use crypto if either the hardware doesn't advertise the
	 * standard crypto capability bit *or* if the vendor specific driver
	 * hasn't advertised that crypto is supported.
	 */
	if (!(std_readl(&ufs->handle, REG_CONTROLLER_CAPABILITIES) & MASK_CRYPTO_SUPPORT) ||
	    !(hba->caps & UFSHCD_CAP_CRYPTO)) {
		err = -EOPNOTSUPP;
		goto out;
	}

	hba->crypto_capabilities.reg_val =
			cpu_to_le32(std_readl(&ufs->handle, REG_UFS_CCAP));
	hba->crypto_cfg_register =
		(u32)hba->crypto_capabilities.config_array_ptr * 0x100;

	/* The actual number of configurations supported is (CFGC+1) */
	err = devm_blk_crypto_profile_init(
			hba->dev, &hba->crypto_profile,
			hba->crypto_capabilities.config_count + 1);
	if (err) {
		dev_err(hba->dev, "devm_blk_crypto_profile_init fail = %d", err);
		err = -EINVAL;
		goto out;
	}

	hba->crypto_profile.ll_ops = exynos_ufs_fmp_crypto_ll_ops;
	/* UFS only supports 8 bytes for any DUN */
	hba->crypto_profile.max_dun_bytes_supported = 8;
	hba->crypto_profile.key_types_supported = BLK_CRYPTO_KEY_TYPE_HW_WRAPPED;
	hba->crypto_profile.dev = hba->dev;

	/*
	 * UFS can support crypto capabilities but FMP can only 1 capability for hw wrapped key
	 */
	hba->crypto_profile.modes_supported[BLK_ENCRYPTION_MODE_AES_256_XTS] =
			FMP_DATA_UNIT_SIZE;

	return 0;

out:
	return err;
}



void exynos_ufs_fmp_init(struct ufs_hba *hba)
{
	unsigned long ret;
	int err;

	dev_info(hba->dev, "Exynos FMP Version: %s\n", FMP_DRV_VERSION);
	dev_info(hba->dev, "HW_KEYS_IN_CUSTOM_KEYSLOT FMP_MODE: %d\n", FMP_MODE);

	ret = exynos_smc(SMC_CMD_FMP, 0, FMP_EMBEDDED, FMP_MODE);
	if (ret) {
		dev_warn(hba->dev, "SMC_CMD_FMP failed on init: %ld\n", ret);
		goto disable;
	}

	/* Advertise crypto support to ufshcd-core. */
	hba->caps |= UFSHCD_CAP_CRYPTO;

	/* Advertise crypto android_quirks to ufshcd-core. */
	hba->android_quirks |= UFSHCD_ANDROID_QUIRK_CUSTOM_CRYPTO_PROFILE;
	dev_info(hba->dev, "Exynos FMP android_quirks: 0x%x\n", hba->android_quirks);

	/* Write Perboot key, IV and Valid bit setting */
	ret = exynos_smc(SMC_CMD_FMP_KW_SYSREG, 0, 0, 0);
	if (ret) {
		dev_err(hba->dev, "SMC_CMD_FMP_KW_SYSREG failed: %ld\n", ret);
		goto disable;
	}

	err = exynos_ufs_fmp_init_crypto_capabilities(hba);
	if (err) {
		dev_warn(hba->dev, "exynos_ufs_fmp_init_crypto_capabilities failed: %d\n", err);
		goto disable;
	}

	device_initialize(&dev_fmp);
	err = dma_coerce_mask_and_coherent(&dev_fmp, DMA_BIT_MASK(36));
	if (err) {
		dev_warn(hba->dev, "No suitable DMA available: %d\n", err);
		goto disable;
	}

	err = exynos_ufs_fmp_check_selftest(hba);
	if (err) {
		dev_warn(hba->dev, "FMP selftest failed: %d\n", err);
		goto disable;
	}

	return;
disable:
	/* Indicate that init failed by clearing UFSHCD_CAP_CRYPTO */
	dev_warn(hba->dev, "FMP: Disabling inline encryption support\n");
	hba->caps &= ~UFSHCD_CAP_CRYPTO;
}
#endif /* CONFIG_HW_KEYS_IN_CUSTOM_KEYSLOT */
#ifdef CONFIG_KEYS_IN_CUSTOM_KEYSLOT
static int fmp_program_key(struct ufs_hba *hba,
			const union ufs_crypto_cfg_entry *cfg, int slot, bool evict)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	unsigned int num_keyslots = hba->crypto_capabilities.config_count;
	size_t i;

	u32 slot_offset = hba->crypto_cfg_register + slot * sizeof(*cfg);

	if (evict)
		dev_info(hba->dev, "fmp_evict_key slot = %d/%d\n", slot, num_keyslots);
	else
		dev_info(hba->dev, "fmp_program_key slot = %d/%d\n", slot, num_keyslots);

	/* Ensure that CFGE is cleared before programming the key */
	hci_writel(&ufs->handle, 0, slot_offset + 16 * sizeof(cfg->reg_val[0]));

	for (i = 0; i < 16; i++) {
		hci_writel(&ufs->handle, le32_to_cpu(cfg->reg_val[i]),
			      slot_offset + i * sizeof(cfg->reg_val[0]));
	}

	/* Write key partially to make key invalid */
	if (evict)
		hci_writel(&ufs->handle, le32_to_cpu(cfg->reg_val[0]), slot_offset);

	/* Write dword 17 */
	hci_writel(&ufs->handle, le32_to_cpu(cfg->reg_val[17]),
		      slot_offset + 17 * sizeof(cfg->reg_val[0]));

	/* Dword 16 must be written last */
	hci_writel(&ufs->handle, le32_to_cpu(cfg->reg_val[16]),
			slot_offset + 16 * sizeof(cfg->reg_val[0]));

	/* To Do
	 * This should be fixed to return by goto label in error cases if ufshcd control is needed.
	 */
	return 0;
}

int exynos_ufs_fmp_program_key(struct ufs_hba *hba,
			const union ufs_crypto_cfg_entry *cfg, int slot)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	const u8 *enckey, *twkey;
	u32 kw_keyvalid;
	u32 slot_offset = hba->crypto_cfg_register + slot * sizeof(*cfg);

	if (!(cfg->config_enable & UFS_CRYPTO_CONFIGURATION_ENABLE))
		return fmp_program_key(hba, cfg, slot, true);

	enckey = cfg->crypto_key;
	twkey = enckey + (AES_256_XTS_KEY_SIZE/2);

	/* Reject weak AES-XTS keys */
	if (!memcmp(enckey, twkey, AES_256_XTS_KEY_SIZE/2)) {
		dev_err(hba->dev, "Can't use weak AES-XTS key slot = %d\n", slot);
		return -EINVAL;
	}

	fmp_program_key(hba, cfg, slot, false);

	kw_keyvalid = check_keyvalid(hba, slot);
	if (!(kw_keyvalid & (0x1 << slot))) {
		dev_err(hba->dev, "Key slot #%d is not valid\n", slot);
		return -EINVAL;
	}
	else {
		/* Dword 16 must be written last */
		hci_writel(&ufs->handle, le32_to_cpu(cfg->reg_val[16]),
				slot_offset + 16 * sizeof(cfg->reg_val[0]));
		dev_info(hba->dev, "%s Key valid = 0x%x\n", __func__, kw_keyvalid);
	}

	/* To Do
	 * This should be fixed to return by goto label in error cases if ufshcd control is needed.
	 */
	return 0;
}

void exynos_ufs_fmp_init(struct ufs_hba *hba)
{
	unsigned long ret;
	int err;

	dev_info(hba->dev, "Exynos FMP Version: %s\n", FMP_DRV_VERSION);
	dev_info(hba->dev, "KEYS_IN_CUSTOM_KEYSLOT FMP_MODE: %d\n", FMP_MODE);

	ret = exynos_smc(SMC_CMD_FMP, 0, FMP_EMBEDDED, FMP_MODE);
	if (ret) {
		dev_warn(hba->dev, "SMC_CMD_FMP failed on init: %ld\n", ret);
		goto disable;
	}

	/* Advertise crypto support to ufshcd-core. */
	hba->caps |= UFSHCD_CAP_CRYPTO;

	/* Advertise crypto android_quirks to ufshcd-core. */
	dev_info(hba->dev, "Exynos FMP android_quirks: 0x%x\n", hba->android_quirks);

	err = exynos_ufs_fmp_check_selftest(hba);
	if (err) {
		dev_warn(hba->dev, "FMP selftest failed: %d\n", err);
		goto disable;
	}

	return;

disable:
	/* Indicate that init failed by clearing UFSHCD_CAP_CRYPTO */
	dev_warn(hba->dev, "FMP Disabling inline encryption support\n");
	hba->caps &= ~UFSHCD_CAP_CRYPTO;
}
#endif


#ifndef CONFIG_KEYS_IN_PRDT
bool exynos_ufs_fmp_crypto_enable(struct ufs_hba *hba)
{
	if (!(hba->caps & UFSHCD_CAP_CRYPTO))
		return false;

	/* Reset might clear all keys, so reprogram all the keys. */
	blk_crypto_reprogram_all_keys(&hba->crypto_profile);

	if (hba->android_quirks & UFSHCD_ANDROID_QUIRK_BROKEN_CRYPTO_ENABLE)
		return false;

	return true;
}

void exynos_ufs_fmp_hba_start(struct ufs_hba *hba)
{
	u32 val = ufshcd_readl(hba, REG_CONTROLLER_ENABLE);

	if (exynos_ufs_fmp_crypto_enable(hba))
		val |= CRYPTO_GENERAL_ENABLE;

	ufshcd_writel(hba, val, REG_CONTROLLER_ENABLE);
}
#endif

void exynos_ufs_fmp_enable(struct ufs_hba *hba)
{
	int err;

	if (!(hba->caps & UFSHCD_CAP_CRYPTO))
		return;

	err = exynos_ufs_fmp_check_selftest(hba);
	if (err) {
		panic("FMP selftest failed: %d\n", err);
	}
}

void exynos_ufs_fmp_resume(struct ufs_hba *hba)
{
	unsigned long ret;
	dev_info(hba->dev, "%s\n",__func__);

	/* Restore all fmp registers on init - Security, Kwmode, kwindataswap */
	ret = exynos_smc(SMC_CMD_FMP_RESUME, 0, FMP_EMBEDDED, 0);
	if (ret)
		dev_err(hba->dev, "SMC_CMD_FMP_RESUME failed on resume: %ld\n", ret);

	if (!(hba->caps &UFSHCD_CAP_CRYPTO))
		return;

#ifdef CONFIG_HW_KEYS_IN_CUSTOM_KEYSLOT
	/* Restore Perboot key, IV and Valid bit setting */
	ret = exynos_smc(SMC_CMD_FMP_KW_SYSREG, 0, 0, 0);
	if (ret) {
		dev_err(hba->dev, "SMC_CMD_FMP_KW_SYSREG failed: %ld\n", ret);
		return;
	}
#endif
}
