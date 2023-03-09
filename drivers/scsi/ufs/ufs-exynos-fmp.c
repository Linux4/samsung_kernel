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

#include "ufshcd.h"
#include "ufshcd-crypto.h"
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

#ifndef CONFIG_KEYS_IN_PRDT
static void exynos_ufs_fmp_populate_dt(struct device *dev, struct exynos_fmp* fmp)
{
	struct device_node *np = dev->of_node;
	struct device_node *child_np;
	int ret = 0;

	/* Check fmp status for featuring */
	child_np = of_get_child_by_name(np, "ufs-protector");
	if (!child_np) {
		dev_info(dev, "No ufs-protector node, set to 0\n");
		fmp->cfge_en = 0;
	}
	else {
		ret = of_property_read_u8(child_np, "cfge_en", &fmp->cfge_en);
		if (ret) {
			dev_info(dev, "read cfge_en failed = 0x%x, set to 0\n", __func__, ret);
			fmp->cfge_en = 0;
		}
	}
}
#endif

int exynos_ufs_fmp_check_selftest(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	u32 fips_status;
	u32 count = 0;
	int ret = 0;

	do {
		fips_status = ufsp_readl(&ufs->handle, FMP_SELFTESTSTAT);
		if (!(fips_status & FMP_SELF_TEST_DONE)) {
			udelay(20);
			count++;
			continue;
		}
		else {
			break;
		}
	} while (count < MAX_RETRY_COUNT);

	if (!(fips_status & FMP_SELF_TEST_DONE)) {
		ret = -ETIMEDOUT;
		dev_err(hba->dev, "FMP selftest timeout\n");
		goto err;
	}

	/* write to clear */
	ufsp_writel(&ufs->handle, FMP_SELF_TEST_DONE, FMP_SELFTESTSTAT);
	if (fips_status & FMP_SELF_TEST_FAIL) {
		ret = -EINVAL;
		dev_err(hba->dev, "FMP selftest failed\n");
		goto err;
	}

	dev_info(hba->dev, "FMP selftest success\n");
err:
	return ret;
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
	dev_info(hba->dev, "Exynos FMP Version: %s\n", FMP_DRV_VERSION);
	dev_info(hba->dev, "KEYS_IN_PRDT\n");

	ret = exynos_smc(SMC_CMD_FMP_SECURITY, 0, FMP_EMBEDDED, CFG_DESCTYPE_3);
	if (ret) {
		dev_warn(hba->dev, "SMC_CMD_FMP_SECURITY failed on init: %ld\n",ret);
		goto disable;
	}
	else
		hba->sg_entry_size = sizeof(struct fmp_sg_entry);

	ret = exynos_smc(SMC_CMD_FMP_KW_MODE, 0, FMP_EMBEDDED, SWKEY_MODE);
	if (ret) {
		dev_warn(hba->dev, "SMC_CMD_FMP_KW_MODE failed on init: %ld\n",ret);
		goto disable;
	}

	/* Advertise crypto support to ufshcd-core. */
	hba->caps |= UFSHCD_CAP_CRYPTO;

	/* Advertise crypto quirks to ufshcd-core. */
	hba->quirks |= UFSHCD_QUIRK_CUSTOM_KEYSLOT_MANAGER;
	hba->quirks |= UFSHCD_QUIRK_KEYS_IN_PRDT;

	dev_info(hba->dev, "Exynos FMP quirks: 0x%x\n", hba->quirks);

	/* Advertise crypto capabilities to the block layer. */
	blk_ksm_init_passthrough(&hba->ksm);
	hba->ksm.max_dun_bytes_supported = AES_BLOCK_SIZE;
	hba->ksm.features = BLK_CRYPTO_FEATURE_STANDARD_KEYS;
	hba->ksm.dev = hba->dev;
	hba->ksm.crypto_modes_supported[BLK_ENCRYPTION_MODE_AES_256_XTS] =
		FMP_DATA_UNIT_SIZE;

	register_trace_android_vh_ufs_fill_prdt(exynos_ufs_fmp_fill_prdt, NULL);
	return;

disable:
	dev_warn(hba->dev, "Disabling inline encryption support\n");
	hba->caps &= ~UFSHCD_CAP_CRYPTO;
}
#endif /* CONFIG_KEYS_IN_PRDT */
#ifdef CONFIG_HW_KEYS_IN_CUSTOM_KEYSLOT
static int fmp_program_key(struct ufs_hba *hba,
			const u32* fmp_key,
			const union exynos_ufs_crypto_cfg_entry *cfg, int slot)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct exynos_fmp *fmp = (struct exynos_fmp *)ufs->fmp;
	unsigned int num_keyslots = hba->crypto_capabilities.config_count;
	size_t i, limit;
	u32 count = 0;
	u32 kw_keyvalid;
	u32 kw_tagabort;
	u32 kw_pbkabort;
	u32 kw_control;

	u32 slot_offset = FMP_KW_SECUREKEY + slot*FMP_KW_SECUREKEY_OFFSET;
	u32 tag_offset = FMP_KW_TAG + slot*FMP_KW_TAG_OFFSET;
	u32 cryptocfg_offset = CRYPTOCFG + slot*CRYPTOCFG_OFFSET;

	dev_info(hba->dev, "%s slot = %d/%d\n",__func__, slot, num_keyslots);

	/* Ensure that CFGE is cleared before programming the key */
	/* Do not clear CFGE because this should be 1 when CFGE_EN == 1 */
	if (!fmp->cfge_en)
		hci_writel(&ufs->handle, 0, cryptocfg_offset - HCI_VS_BASE_GAP);
	else
		hci_writel(&ufs->handle, CRYPTOCFG_GFGE_EN, cryptocfg_offset - HCI_VS_BASE_GAP);

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

	/* Unwrap command */
	kw_control = (1<<slot);
	ufsp_writel(&ufs->handle, kw_control, FMP_KW_CONTROL);

	/* Keyslot should be valid for crypto IO */
	do {
		kw_keyvalid = ufsp_readl(&ufs->handle, FMP_KW_KEYVALID);
		if (!(kw_keyvalid & (0x1 << slot))) {
			dev_warn(hba->dev, "Key slot #%d is not valid yet\n", slot);
			udelay(2);
			count++;
			continue;
		}
		else {
			break;
		}
	} while (count < MAX_RETRY_COUNT);

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
	else {
		/* Dword 16 must be written last */
		hci_writel(&ufs->handle, le32_to_cpu(cfg->reg_val), cryptocfg_offset - HCI_VS_BASE_GAP);
		dev_info(hba->dev, "%s Key valid = 0x%x\n", __func__, kw_keyvalid);
	}

	/* To Do
	 * This should be fixed to return by goto label in error cases if ufshcd control is needed.
	 */
	return 0;
}

static int fmp_evict_key(struct ufs_hba *hba,
			const u32* fmp_key,
			const union exynos_ufs_crypto_cfg_entry *cfg, int slot)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct exynos_fmp *fmp = (struct exynos_fmp *)ufs->fmp;
	unsigned int num_keyslots = hba->crypto_capabilities.config_count;
	size_t i, limit;

	u32 slot_offset = FMP_KW_SECUREKEY + slot*FMP_KW_SECUREKEY_OFFSET;
	u32 tag_offset = FMP_KW_TAG + slot*FMP_KW_TAG_OFFSET;
	u32 cryptocfg_offset = CRYPTOCFG + slot*CRYPTOCFG_OFFSET;

	dev_info(hba->dev, "%s slot = %d/%d\n",__func__, slot, num_keyslots);

	/* Ensure that CFGE is cleared before programming the key */
	/* Do not clear CFGE because this should be 1 when CFGE_EN == 1 */
	if (!fmp->cfge_en)
		hci_writel(&ufs->handle, 0, cryptocfg_offset - HCI_VS_BASE_GAP);
	else
		hci_writel(&ufs->handle, CRYPTOCFG_GFGE_EN, cryptocfg_offset - HCI_VS_BASE_GAP);

	/* Key program in keyslot */
	/* No need to Swap File key and Tweak key for zeroization */
	for (i = 0, limit = AES_256_XTS_KEY_SIZE / sizeof(u32); i < limit; i++) {
		ufsp_writel(&ufs->handle, le32_to_cpu(fmp_key[i]), slot_offset + (i * sizeof(u32)));
	}

	/* KEY TAG */
	for (i = 0, limit = AES_GCM_TAG_SIZE / sizeof(u32); i < limit; i++) {
		ufsp_writel(&ufs->handle, le32_to_cpu(fmp_key[i + HW_WRAPPED_KEY_TAG_OFFSET]),
				tag_offset + (i * sizeof(u32)));
	}

	/* Do not Unwrap */
	/* Dword 16 must be written last */
	hci_writel(&ufs->handle, le32_to_cpu(cfg->reg_val), cryptocfg_offset - HCI_VS_BASE_GAP);

	/* To Do
	 * This should be fixed to return by goto label in error cases if ufshcd control is needed.
	 */
	return 0;
}

static int exynos_ufs_fmp_keyslot_program(struct blk_keyslot_manager *ksm,
					 const struct blk_crypto_key *key,
					 unsigned int slot)
{
	struct ufs_hba *hba = container_of(ksm, struct ufs_hba, ksm);
	u32 secret_size = SECRET_SIZE + AES_GCM_TAG_SIZE;
	union exynos_ufs_crypto_cfg_entry cfg = {};
	union {
		u8 bytes[AES_256_XTS_KEY_SIZE + AES_GCM_TAG_SIZE];
		u32 words[(AES_256_XTS_KEY_SIZE + AES_GCM_TAG_SIZE) / sizeof(u32)];
	} fmp_key;

	int err = 0;

	/* Only AES-256-XTS is supported */
	if (key->crypto_cfg.crypto_mode != BLK_ENCRYPTION_MODE_AES_256_XTS ||
		(key->crypto_cfg.data_unit_size != FMP_DATA_UNIT_SIZE) ||
		(key->crypto_cfg.is_hw_wrapped != true) ||
		((key->size - secret_size)!= (AES_256_XTS_KEY_SIZE + AES_GCM_TAG_SIZE))) {
		dev_err(hba->dev,
			"Unhandled crypto capability: crypto_mode=%d, key_size=%d, data_unit_size=%d, is_hw_wrapped%d\n",
			key->crypto_cfg.crypto_mode, key->size - secret_size,
			key->crypto_cfg.data_unit_size, key->crypto_cfg.is_hw_wrapped);
		return -EINVAL;
	}

	/* In XTS mode, the blk_crypto_key's size is already doubled */
	memcpy(fmp_key.bytes, key->raw, key->size - secret_size);

	/* Set crypto config register.*/
	cfg.data_unit_size = FMP_DATA_UNIT_SIZE / 512;
	cfg.crypto_cap_idx = 1;
	cfg.config_enable = UFS_CRYPTO_CONFIGURATION_ENABLE;

	err = fmp_program_key(hba, fmp_key.words, &cfg, slot);

	memzero_explicit(&fmp_key, key->size - secret_size);
	return err;
}

static int exynos_ufs_fmp_keyslot_evict(struct blk_keyslot_manager *ksm,
					const struct blk_crypto_key *key,
					unsigned int slot)
{
	struct ufs_hba *hba = container_of(ksm, struct ufs_hba, ksm);
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct exynos_fmp *fmp = (struct exynos_fmp *)ufs->fmp;
	union exynos_ufs_crypto_cfg_entry cfg = {};
	u32 fmp_key[(AES_256_XTS_KEY_SIZE + AES_GCM_TAG_SIZE) / sizeof(u32)] = {};
	int err = 0;

	/* Set CFGE bit to 1 by force in case of key eviction
	 * because this should be 1 when CFGE_EN == 1
	*/
	if (fmp->cfge_en)
		cfg.config_enable = UFS_CRYPTO_CONFIGURATION_ENABLE;

	err = fmp_evict_key(hba, fmp_key, &cfg, slot);

	return err;
}

static int exynos_ufs_fmp_derive_raw_secret(struct blk_keyslot_manager *ksm,
					const u8 *wrapped_key,
					unsigned int wrapped_key_size,
					u8 *secret, unsigned int secret_size)
{
	struct ufs_hba *hba = container_of(ksm, struct ufs_hba, ksm);
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
	secret_key = dmam_alloc_coherent(&dev_fmp, secret_size, &secret_key_pa, GFP_KERNEL);
	if (!secret_key) {
		dev_err(hba->dev, "Fail to allocate memory for secret_key\n");
		ret = -ENOMEM;
		goto free_fmp_key;
	}

	memcpy(fmp_key, wrapped_key, wrapped_key_size);

	smc_ret = exynos_smc(SMC_CMD_FMP_SECRET, fmp_key_pa, secret_key_pa, secret_size);
	if (smc_ret) {
		dev_err(hba->dev, "SMC_CMD_FMP_SECRET failed: %ld\n", smc_ret);
		ret = -EINVAL;
		goto free_secret_key;
	}

	memcpy(secret, secret_key, secret_size);

free_secret_key:
	memzero_explicit(secret_key, secret_size);
	dmam_free_coherent(&dev_fmp, secret_size, secret_key, secret_key_pa);
free_fmp_key:
	memzero_explicit(fmp_key, wrapped_key_size);
	dmam_free_coherent(&dev_fmp, wrapped_key_size, fmp_key, fmp_key_pa);
out:
	return ret;
}

static const struct blk_ksm_ll_ops exynos_ufs_fmp_ksm_ll_ops = {
	.keyslot_program	= exynos_ufs_fmp_keyslot_program,
	.keyslot_evict		= exynos_ufs_fmp_keyslot_evict,
	.derive_raw_secret	= exynos_ufs_fmp_derive_raw_secret,
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

	if (!(hba->quirks & UFSHCD_QUIRK_CUSTOM_KEYSLOT_MANAGER)) {
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
	err = blk_ksm_init(&hba->ksm, hba->crypto_capabilities.config_count + 1);
	if (err) {
		dev_err(hba->dev, "blk_ksm_init fail");
		err = -EINVAL;
		goto out;
	}

	hba->ksm.ksm_ll_ops = exynos_ufs_fmp_ksm_ll_ops;
	/* UFS only supports 8 bytes for any DUN */
	hba->ksm.max_dun_bytes_supported = 8;
	hba->ksm.features = BLK_CRYPTO_FEATURE_WRAPPED_KEYS;
	hba->ksm.dev = hba->dev;

	/*
	 * UFS can support crypto capabilities but FMP can only 1 capability for hw wrapped key
	 */
	hba->ksm.crypto_modes_supported[BLK_ENCRYPTION_MODE_AES_256_XTS] =
			FMP_DATA_UNIT_SIZE;

	return 0;

out:
	return err;
}


void exynos_ufs_fmp_init(struct ufs_hba *hba)
{
	unsigned long ret;
	int err;
	unsigned int kw_indataswap;
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct exynos_fmp *fmp;

	dev_info(hba->dev, "Exynos FMP Version: %s\n", FMP_DRV_VERSION);
	dev_info(hba->dev, "HW_KEYS_IN_CUSTOM_KEYSLOT\n");

	ufs->fmp = devm_kzalloc(ufs->dev, sizeof(struct exynos_fmp), GFP_KERNEL);
	if (ufs->fmp == NULL) {
		dev_warn(hba->dev, "failed to alloc ufs->fmp\n");
		goto disable_nofree;
	}
	fmp = (struct exynos_fmp *)ufs->fmp;

	ret = exynos_smc(SMC_CMD_FMP_SECURITY, 0, FMP_EMBEDDED, CFG_DESCTYPE_0);
	if (ret) {
		dev_warn(hba->dev, "SMC_CMD_FMP_SECURITY failed on init: %ld\n", ret);
		goto disable;
	}

	ret = exynos_smc(SMC_CMD_FMP_KW_MODE, 0, FMP_EMBEDDED, SECUREKEY_MODE | UNWRAP_EN | WAP0_NS);
	if (ret) {
		dev_warn(hba->dev, "SMC_CMD_FMP_KW_MODE failed on init: %ld\n", ret);
		goto disable;
	}

	/* Advertise crypto support to ufshcd-core. */
	hba->caps |= UFSHCD_CAP_CRYPTO;

	/* Advertise crypto quirks to ufshcd-core. */
	hba->quirks |= UFSHCD_QUIRK_CUSTOM_KEYSLOT_MANAGER;
	dev_info(hba->dev, "Exynos FMP quirks: 0x%x\n", hba->quirks);

	/*
	 * Set data swap mode
	 * secure key/tag swap options are for swapping key at writing key in slot
	 * Ohter options are for key unwrap and as same for all secure key mode
	 */
	kw_indataswap = 0;
	kw_indataswap |= (SECURE_FILEKEY_WORDSWAP | SECURE_TWEAKKEY_WORDSWAP |
			SECURE_FILEKEY_BYTESWAP | SECURE_TWEAKKEY_BYTESWAP |
			PBK_WORDSWAP | IV_WORDSWAP | TAG_WORDSWAP |
			PBK_BYTESWAP | IV_BYTESWAP | TAG_BYTESWAP);

	ret = exynos_smc(SMC_CMD_FMP_KW_INDATASWAP, 0, FMP_EMBEDDED, kw_indataswap);
	if (ret) {
		dev_err(hba->dev, "SMC_CMD_FMP_KW_INDATASWAP failed: %ld\n", ret);
		goto disable;
	}

	exynos_ufs_fmp_populate_dt(ufs->dev, fmp);
	dev_info(ufs->dev, "ufs->fmp cfge_en = %d\n", fmp->cfge_en);

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
	devm_kfree(ufs->dev, ufs->fmp);
disable_nofree:
	/* Indicate that init failed by clearing UFSHCD_CAP_CRYPTO */
	dev_warn(hba->dev, "Disabling inline encryption support\n");
	hba->caps &= ~UFSHCD_CAP_CRYPTO;
}
#endif /* CONFIG_HW_KEYS_IN_CUSTOM_KEYSLOT */
#ifdef CONFIG_KEYS_IN_CUSTOM_KEYSLOT
static int fmp_evict_key(struct ufs_hba *hba,
			const union ufs_crypto_cfg_entry *cfg, int slot)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct exynos_fmp *fmp = (struct exynos_fmp *)ufs->fmp;
	size_t i;
	u32 slot_offset = hba->crypto_cfg_register + slot * sizeof(*cfg);
	u32 dword16;

	dev_info(hba->dev, "%s slot = %d/%d\n",__func__, slot, hba->crypto_capabilities.config_count);

	/* Ensure that CFGE is cleared before programming the key */
	/* Do not clear CFGE because this shoul be 1 when CFGE_EN == 1 */
	if (!fmp->cfge_en)
		hci_writel(&ufs->handle, 0, slot_offset + 16 * sizeof(cfg->reg_val[0]) - HCI_VS_BASE_GAP);
	else
		hci_writel(&ufs->handle, CRYPTOCFG_GFGE_EN, slot_offset + 16 * sizeof(cfg->reg_val[0]) - HCI_VS_BASE_GAP);

	for (i = 0; i < 16; i++) {
		hci_writel(&ufs->handle, le32_to_cpu(cfg->reg_val[i]),
			      slot_offset + i * sizeof(cfg->reg_val[0]) - HCI_VS_BASE_GAP);
	}

	/* Write key partially to make key invalid */
	hci_writel(&ufs->handle, le32_to_cpu(cfg->reg_val[0]), slot_offset - HCI_VS_BASE_GAP);

	/* Write dword 17 */
	hci_writel(&ufs->handle, le32_to_cpu(cfg->reg_val[17]),
		      slot_offset + 17 * sizeof(cfg->reg_val[0]) - HCI_VS_BASE_GAP);

	/* Dword 16 must be written last */
	/* Set CFGE bit to 1 by force in case of key eviction
	 *  because this should be 1 when CFGE_EN == 1
	 */
	if (fmp->cfge_en)
		dword16 = cfg->reg_val[16] | 0x80000000;

	/* CFGE bit should be 1 when CFGE_EN == 1 */
	if (fmp->cfge_en)
		hci_writel(&ufs->handle, le32_to_cpu(dword16),
				slot_offset + 16 * sizeof(cfg->reg_val[0]) - HCI_VS_BASE_GAP);
	else
		hci_writel(&ufs->handle, le32_to_cpu(cfg->reg_val[16]),
				slot_offset + 16 * sizeof(cfg->reg_val[0]) - HCI_VS_BASE_GAP);

	/* To Do
	 * This should be fixed to return by goto label in error cases if ufshcd control is needed.
	 */
	return 0;
}

int exynos_ufs_fmp_program_key(struct ufs_hba *hba,
			const union ufs_crypto_cfg_entry *cfg, int slot)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct exynos_fmp *fmp = (struct exynos_fmp *)ufs->fmp;
	const u8 *enckey, *twkey;
	u32 count = 0;
	u32 kw_keyvalid;
	size_t i;
	u32 slot_offset = hba->crypto_cfg_register + slot * sizeof(*cfg);

	if (!(cfg->config_enable & UFS_CRYPTO_CONFIGURATION_ENABLE))
		return fmp_evict_key(hba, cfg, slot);

	dev_info(hba->dev, "%s slot = %d/%d\n",__func__, slot, hba->crypto_capabilities.config_count);

	enckey = cfg->crypto_key;
	twkey = enckey + (AES_256_XTS_KEY_SIZE/2);

	/* Reject weak AES-XTS keys */
	if (!memcmp(enckey, twkey, AES_256_XTS_KEY_SIZE/2)) {
		dev_err(hba->dev, "Can't use weak AES-XTS key\n");
		return -EINVAL;
	}

	/* Ensure that CFGE is cleared before programming the key */
	/* Do not clear CFGE because this shoul be 1 when CFGE_EN == 1 */
	if (!fmp->cfge_en)
		hci_writel(&ufs->handle, 0, slot_offset + 16 * sizeof(cfg->reg_val[0]) - HCI_VS_BASE_GAP);
	else
		hci_writel(&ufs->handle, CRYPTOCFG_GFGE_EN, slot_offset + 16 * sizeof(cfg->reg_val[0]) - HCI_VS_BASE_GAP);

	for (i = 0; i < 16; i++) {
		hci_writel(&ufs->handle, le32_to_cpu(cfg->reg_val[i]),
			      slot_offset + i * sizeof(cfg->reg_val[0]) - HCI_VS_BASE_GAP);
	}

	/* Write dword 17 */
	hci_writel(&ufs->handle, le32_to_cpu(cfg->reg_val[17]),
		      slot_offset + 17 * sizeof(cfg->reg_val[0]) - HCI_VS_BASE_GAP);

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

	if (!(kw_keyvalid & (0x1 << slot))) {
		dev_err(hba->dev, "Key slot #%d is not valid\n", slot);
		return -EINVAL;
	}
	else {
		/* Dword 16 must be written last */
		hci_writel(&ufs->handle, le32_to_cpu(cfg->reg_val[16]),
				slot_offset + 16 * sizeof(cfg->reg_val[0]) - HCI_VS_BASE_GAP);
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
	unsigned int kw_indataswap;
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct exynos_fmp *fmp;

	dev_info(hba->dev, "Exynos FMP Version: %s\n", FMP_DRV_VERSION);
	dev_info(hba->dev, "KEYS_IN_CUSTOM_KEYSLOT\n");

	ufs->fmp = devm_kzalloc(ufs->dev, sizeof(struct exynos_fmp), GFP_KERNEL);
	if (ufs->fmp == NULL) {
		dev_warn(hba->dev, "failed to alloc ufs->fmp\n");
		goto disable_nofree;
	}
	fmp = (struct exynos_fmp *)ufs->fmp;

	ret = exynos_smc(SMC_CMD_FMP_SECURITY, 0, FMP_EMBEDDED, CFG_DESCTYPE_0);
	if (ret) {
		dev_warn(hba->dev, "SMC_CMD_FMP_SECURITY failed on init: %ld\n", ret);
		goto disable;
	}

	ret = exynos_smc(SMC_CMD_FMP_KW_MODE, 0, FMP_EMBEDDED, SECUREKEY_MODE | UNWRAP_BYPASS);
	if (ret) {
		dev_warn(hba->dev, "SMC_CMD_FMP_KW_MODE failed on init: %ld\n", ret);
		goto disable;
	}

	/* Advertise crypto support to ufshcd-core. */
	hba->caps |= UFSHCD_CAP_CRYPTO;

	/* Advertise crypto quirks to ufshcd-core. */
	dev_info(hba->dev, "Exynos FMP quirks: 0x%x\n", hba->quirks);

	/*
	 * Set data swap mode
	 * secure key swap option is for swapping key at writing key in slot
	 * Ohter options doensn't affect bypass mode but for setting as same for all secure key mode
	 */
	kw_indataswap = 0;
	kw_indataswap |= (SECURE_FILEKEY_WORDSWAP | SECURE_TWEAKKEY_WORDSWAP |
			SECURE_FILEKEY_BYTESWAP | SECURE_TWEAKKEY_BYTESWAP |
			PBK_WORDSWAP | IV_WORDSWAP | TAG_WORDSWAP |
			PBK_BYTESWAP | IV_BYTESWAP | TAG_BYTESWAP);

	ret = exynos_smc(SMC_CMD_FMP_KW_INDATASWAP, 0, FMP_EMBEDDED, kw_indataswap);
	if (ret) {
		dev_err(hba->dev, "SMC_CMD_FMP_KW_INDATASWAP failed: %ld\n", ret);
		goto disable;
	}

	exynos_ufs_fmp_populate_dt(ufs->dev, fmp);
	dev_info(ufs->dev, "ufs->fmp cfge_en = %d\n", fmp->cfge_en);

	err = exynos_ufs_fmp_check_selftest(hba);
	if (err) {
		dev_warn(hba->dev, "FMP selftest failed: %d\n", err);
		goto disable;
	}

	return;

disable:
	devm_kfree(ufs->dev, ufs->fmp);
disable_nofree:
	/* Indicate that init failed by clearing UFSHCD_CAP_CRYPTO */
	dev_warn(hba->dev, "Disabling inline encryption support\n");
	hba->caps &= ~UFSHCD_CAP_CRYPTO;
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
