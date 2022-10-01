/*
 * Exynos FMP MMC crypto interface
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/keyslot-manager.h>
#include <crypto/fmp.h>
#include <linux/mmc/core.h>
#include "../core/queue.h"

#ifdef CONFIG_MMC_DW_EXYNOS_FMP
int fmp_mmc_crypt_cfg(struct bio *bio, void *desc,
					struct mmc_data *data, int page_index,
					bool cmdq_enabled)
{
	struct mmc_blk_request *brq = container_of(data, struct mmc_blk_request, data);
	struct mmc_queue_req *mq_rq = container_of(brq, struct mmc_queue_req, brq);
	struct request *rq = mmc_queue_req_to_req(mq_rq);
	struct request_queue *q = rq->q;
	struct fmp_crypto_info fmp_info;
	struct fmp_request req;
	int ret = 0;
	u64 iv = 0;

	if (!bio || !q)
		return 0;

	if (!q->ksm || !bio_crypt_should_process(rq)) {
		req.table = desc;
		req.prdt_cnt = 1;
		req.prdt_off = 0;
		ret = exynos_fmp_bypass(&req, bio);
		if (ret) {
			pr_debug("%s: find fips\n", __func__);
			req.fips = true;
			goto encrypt;
		}
		return 0;
	}
	fmp_info.enc_mode = EXYNOS_FMP_FILE_ENC;
	fmp_info.algo_mode = EXYNOS_FMP_ALGO_MODE_AES_XTS;

	ret = exynos_fmp_setkey(&fmp_info,
		(u8 *)bio->bi_crypt_context->bc_key->raw,
		bio->bi_crypt_context->bc_key->size, 0);
	if (ret) {
		pr_err("%s: fails to set fmp key. ret:%d\n", __func__, ret);
		print_hex_dump(KERN_CONT, "fmp:", DUMP_PREFIX_OFFSET,
					16, 1, req.table, 64, false);
		return ret;
	}

	req.iv =  &iv;
	req.ivsize = sizeof(iv);
	req.fips = false;
encrypt:
	req.cmdq_enabled = cmdq_enabled;
	if (!req.fips)
		iv = bio->bi_crypt_context->bc_dun[0] + page_index;
	req.table = desc;
	ret = exynos_fmp_crypt(&fmp_info, &req);
	if (ret) {
		pr_err("%s: fails to crypt fmp key. ret:%d\n", __func__, ret);
		print_hex_dump(KERN_CONT, "fmp:", DUMP_PREFIX_OFFSET,
					16, 1, req.table, 64, false);
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL(fmp_mmc_crypt_cfg);

int fmp_mmc_crypt_clear(struct bio *bio, void *desc,
					struct mmc_data *data, bool cmdq_enabled)
{
	struct mmc_blk_request *brq = container_of(data, struct mmc_blk_request, data);
	struct mmc_queue_req *mq_rq = container_of(brq, struct mmc_queue_req, brq);
	struct request *rq = mmc_queue_req_to_req(mq_rq);
	struct request_queue *q = rq->q;
	struct fmp_crypto_info fmp_info;
	struct fmp_request req;
	int ret = 0;

	if (!bio || !q)
		return 0;

	if (!q->ksm || !bio_crypt_should_process(rq)) {
		ret = exynos_fmp_fips(bio);
		if (ret) {
			pr_debug("%s: find fips\n", __func__);
			req.fips = true;
		} else {
			return 0;
		}
	}

	req.table = desc;
	ret = exynos_fmp_clear(&fmp_info, &req);
	if (ret) {
		pr_warn("%s: fails to clear fips\n", __func__);
	}
	return 0;
}
EXPORT_SYMBOL(fmp_mmc_crypt_clear);

static const struct keyslot_mgmt_ll_ops fmp_ksm_ops = {
};

int fmp_mmc_init_crypt(struct mmc_host *mmc)
{
	unsigned int crypto_modes_supported[BLK_ENCRYPTION_MODE_MAX] = {
		[BLK_ENCRYPTION_MODE_AES_256_XTS] = 4096,
	};

	mmc->ksm = keyslot_manager_create_passthrough(NULL, &fmp_ksm_ops,
					crypto_modes_supported, NULL);
	if(!mmc->ksm) {
		pr_info("%s fails to get keyslot manager\n", __func__);
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL(fmp_mmc_init_crypt);
#endif
