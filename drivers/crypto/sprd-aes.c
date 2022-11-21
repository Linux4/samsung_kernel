/*
 * drivers/crypto/sprd-aes.c
 *
 * Driver for SPREADTRUM AES hardware engine with ecb/cbc/ctr/xts mode.
 *
 *
 * This engine has standard mode and link list mode, however, this driver
 * make use of the link list mode. In the link list mode, the operation is
 * in the internal memory configured by the cpu, ce must read the configure
 * information from internal memory before starting the encrypt/decrypt 
 * operation. in addition, the scatterlist of req is the same as the list of
 * linklist, i.e. the meanings of the two list is same.
 *
 * Copyright (c) 2015, SPREADTRUM Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * Haidian, Beijing, CHINA, 3/20/2015.
 */

#include <crypto/aes.h>
#include <crypto/algapi.h>
#include <linux/completion.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/workqueue.h>
#include <soc/sprd/sci.h>
#include "sprd-aes.h"

/* async queue length //'4096' is a temp value-lixr20150320. */
#define SPRD_AES_QUEUE_LENGTH	4096

/* the buffer size of the link list mode in bytes.
 * the number of list is about 4096. here, to 
 * assume that the node amount in one list are 16.
 * so, the following value is canculated by
 * ((24 + 12 * 16) * 4096) which is fact 
 * ((ptr + len + iv) + node * node_num) * list_num
 * //'0xd8000' is a temp value-lixr20150320
 * */
#define AES_LINK_LIST_BUF_SIZE_BYTES	0xd8000

/* the max value of the waiting time to interrupt of crypto engine.
 * the value is canculated by the mount of data and the rate of the
 * data process. the following value is canculated by 
 * SPRD_AES_QUEUE_LENGTH * max_node_in_one_list * max_data_in_one_node
 * (4096 * 512B) / 160MB/s * 1000 = 12.5 //'20000' is a temp value-lixr20150320
 * the unit is millisecond.
 * */
#define MAX_COM_TIMEOUT_VAL	20000

struct sprd_aes_dev {
	struct device *dev;
	void __iomem *io_base;
	dma_addr_t key_phys_base;
	uint *key_base;
	struct sprd_aes_ctx *ctx;
	struct completion op_complete;
	uint *link_list;
	dma_addr_t link_list_buf;
	uint *cur_ptr;
	dma_addr_t cur_dma_ptr;
	uint *lptr;
	struct crypto_queue queue;
	uint cur_queue_num;
	struct list_head ce_req_list;
	int irq;
	unchar *iv;
	int ivlen;
	spinlock_t lock;
	ulong flags;
};

struct sprd_aes_ctx {
	struct sprd_aes_dev *dd;
	unchar key[2 * AES_MAX_KEY_SIZE];
	size_t keylen;
};

struct sprd_aes_reqctx {
	ulong mode;
	uint node_num;
};

static struct sprd_aes_dev *aes_dev;

static DEFINE_MUTEX(aes_lock);

static void aes_workqueue_handler(struct work_struct *work);
static DECLARE_WORK(aes_work, aes_workqueue_handler);
static struct workqueue_struct *aes_wq;

static inline uint aes_readl(struct sprd_aes_dev *dd, uint offset)
{
	return readl(dd->io_base + offset);
}

static inline void aes_writel(struct sprd_aes_dev *dd, uint val, uint offset)
{
	writel(val, dd->io_base + offset);
}

static inline void clk_main_aes_enable(struct sprd_aes_dev *sd)
{
	aes_writel(sd, (CMD_MAIN_CLK_EN | CMD_AES_CLK_EN), SPRD_AES_CLK_EN);
}

static inline void clk_main_aes_disable(struct sprd_aes_dev *sd)
{
	aes_writel(sd, CMD_DISABLE, SPRD_AES_CLK_EN);
}

static inline void clk_aes_disable(struct sprd_aes_dev *sd)
{
	uint tmp = 0;

	tmp = aes_readl(sd, SPRD_AES_CLK_EN);
	aes_writel(sd, (tmp & ~CMD_AES_CLK_EN), SPRD_AES_CLK_EN);
}

static int aes_set_key(struct sprd_aes_dev *dd)
{
	uint val = 0;

	val = aes_readl(dd, SPRD_AES_DMA_STATUS);
	if (val & (CMD_AES_CE_BUSY << CMD_BIT24_SHIFT)) {
		dev_err(dd->dev, "error: crypto engine is busy, try again");
		return -EBUSY;
	}
	val = dd->ctx->keylen;
	aes_writel(dd, val, SPRD_AES_KEY_ADDR_LEN);
	aes_writel(dd, dd->key_phys_base, SPRD_AES_KEY_ADDR);

	return 0;
}

static int aes_start_crypt(struct sprd_aes_dev *dd, uint mode)
{
	uint val = 0;
	unsigned long ret;

	/* CMD_AES_LEN_ERR_INT is used for protect from the length of data
	 * (key, the data) is empty or none. 
	 * */
	val |= CMD_AES_LEN_ERR_INT | CMD_AES_LIST_END_INT;
	aes_writel(dd, val, SPRD_AES_INT_EN);

	val = CMD_DISABLE;
	if (dd->ctx->keylen == AES_KEYSIZE_128) {
		val |= (CMD_AES_KEY128 << CMD_BIT8_SHIFT);
	} else if (dd->ctx->keylen == AES_KEYSIZE_192) {
		val |= (CMD_AES_KEY192 << CMD_BIT8_SHIFT);
	} else if (dd->ctx->keylen == AES_KEYSIZE_256) {
		if (mode & FLAGS_XTS)
			val |= (CMD_AES_KEY128 << CMD_BIT8_SHIFT);
		else
			val |= (CMD_AES_KEY256 << CMD_BIT8_SHIFT);
	} else {
		if (mode & FLAGS_XTS)
			val |= (CMD_AES_KEY256 << CMD_BIT8_SHIFT);
		else
			return -EPERM;
	}
	if (mode & FLAGS_ECB)
		val |= (CMD_AES_ECB_MODE << CMD_BIT8_SHIFT);
	else if (mode & FLAGS_CBC)
		val |= (CMD_AES_CBC_MODE << CMD_BIT8_SHIFT);
	else if (mode & FLAGS_CTR)
		val |= (CMD_AES_CTR_MODE << CMD_BIT8_SHIFT);
	else if (mode & FLAGS_XTS)
		val |= (CMD_AES_XTS_MODE << CMD_BIT8_SHIFT);
	else
		return -EINVAL;
	if (mode & FLAGS_DECRYPT)
		val |= CMD_AES_ENC_DEC_SEL;
	else
		val &= ~CMD_AES_ENC_DEC_SEL;
	val |= CMD_AES_EN;
	aes_writel(dd, val, SPRD_AES_MODE);

	val = CMD_DISABLE;
	val |= CMD_AES_LINK_MODE | (CMD_AES_KEY_IN_DDR << CMD_BIT8_SHIFT);
	val |= (CMD_AES_WAIT_BDONE << CMD_BIT8_SHIFT);
	val &= ~(CMD_AES_GEN_INT_EACH << CMD_BIT8_SHIFT);
	val |= (CMD_AES_DST_BYTE_SWITCH << CMD_BIT16_SHIFT);
	val |= (CMD_AES_SRC_BYTE_SWITCH << CMD_BIT16_SHIFT);
	aes_writel(dd, val, SPRD_AES_CFG);

	/* setup a new key. */
	if (dd->flags & FLAGS_NEW_KEY) {
		memset(dd->key_base, 0, 2 * AES_MAX_KEY_SIZE);
		memcpy(dd->key_base, dd->ctx->key, dd->ctx->keylen);
		ret = aes_set_key(dd);
		if (ret)
			return -EPERM;
		dd->flags &= ~FLAGS_NEW_KEY;
	}

	val = CMD_DISABLE;
	val |= CMD_AES_CE_START;
	aes_writel(dd, val, SPRD_AES_START);

	ret = wait_for_completion_timeout(&dd->op_complete,
					msecs_to_jiffies(MAX_COM_TIMEOUT_VAL));
	if (ret == 0) {
		dev_err(dd->dev, "comlpetion wait for interrupt timeout...\n");
		val = aes_readl(dd, SPRD_AES_DMA_STATUS);
		dev_err(dd->dev, "AES_DMA_STATUS:0x%x\n", val);

		return -ETIMEDOUT;
	}

	return 0;
}

static int sprd_aes_fill_list(struct sprd_aes_dev *dd)
{
	struct crypto_async_request *async_req, *backlog;
	struct crypto_ablkcipher *tfm;
	struct sprd_aes_ctx *ctx;
	struct sprd_aes_reqctx *rctx;
	struct ablkcipher_request *req;
	ulong flags;
	int dma_max = AES_LINK_LIST_BUF_SIZE_BYTES;
	int ret = 0, node_num = 0, total, i;
	uint count = 0;
	uint *tmp_ptr = dd->cur_ptr;
	unchar tweak[16];
	dma_addr_t addr_in, addr_out;
	struct scatterlist *in_sg, *out_sg;

	spin_lock_irqsave(&dd->lock, flags);
	backlog = crypto_get_backlog(&dd->queue);
	async_req = crypto_dequeue_request(&dd->queue);
	if (!async_req) {
		dev_err(dd->dev, "error: queue is empty\n");
		return 0; /* 0 means for error. */
	}
	spin_unlock_irqrestore(&dd->lock, flags);
	if (backlog)
		backlog->complete(backlog, -EINPROGRESS);
	req = ablkcipher_request_cast(async_req);
	if (!req->src || !req->dst)
		return 0; /*0 means for error.*/
	/* assign new request to the engine */
	list_add_tail(&async_req->list, &dd->ce_req_list);
	total = req->nbytes;
	in_sg = req->src;
	out_sg = req->dst;
	tfm = crypto_ablkcipher_reqtfm(req);
	rctx = ablkcipher_request_ctx(req);
	ctx = crypto_ablkcipher_ctx(tfm);
	rctx->mode &= FLAGS_MODE_MASK;
	dd->flags = (dd->flags & ~FLAGS_MODE_MASK) | rctx->mode;
	dd->iv = (unchar *)req->info;
	dd->ivlen = crypto_ablkcipher_ivsize(tfm);
	/* assign new context to the engine */
	ctx->dd = dd;
	dd->ctx = ctx;

	/* make the pointer lptr pointing to the previous list of the current
	 * list. the value in the pointer lptr of the last list is always right,
	 * when it is a valid one. */
	if (!(dd->flags & FLAGS_FIRST_LIST)) {
		*(dd->lptr) = dd->cur_dma_ptr;
	}
	if (dd->iv) {
		switch (dd->flags & FLAGS_OPS_MODE_MASK) {
		case FLAGS_XTS:
			for (i = 0; i < 16; i++)
				tweak[i] = dd->iv[15 - i];
			dd->iv = tweak;
		case FLAGS_CBC:
		case FLAGS_CTR:
			memcpy(dd->cur_ptr + 2, dd->iv, dd->ivlen); /*fill iv into this list.*/
			break;
		case FLAGS_ECB:
			dev_dbg(dd->dev, "iv is not NULL in ECB mode\n");
			break;
		default:
			dev_err(dd->dev, "the operation mode is error\n");
			return 0; /* 0 means for error! */
		}
	}
	/* the space of iv must be left, even if there are no valid content in
	 * it. 6 is the fixed value(unsigned int-unit) according to the list of
	 * link mode. */
	dd->cur_ptr += 6;
	while (total) {
		dev_dbg(dd->dev, "remain: %d Bytes...\n", total);
		ret = dma_map_sg(dd->dev, in_sg, 1, DMA_TO_DEVICE);
		if (!ret) {
			dev_err(dd->dev, "%d:dma_map_sg() error\n", __LINE__);
			goto out;
		}
		ret = dma_map_sg(dd->dev, out_sg, 1, DMA_FROM_DEVICE);
		if (!ret) {
			dev_err(dd->dev, "%d:dma_map_sg() error\n", __LINE__);
			dma_unmap_sg(dd->dev, in_sg, 1, DMA_TO_DEVICE);
			goto out;
		}
		addr_in = sg_dma_address(in_sg);
		addr_out = sg_dma_address(out_sg);
		count = min_t(int, sg_dma_len(in_sg), dma_max);
		WARN_ON(sg_dma_len(in_sg) != sg_dma_len(out_sg));
		*(dd->cur_ptr) = addr_in;
		*(dd->cur_ptr + 1) = addr_out;
		*(dd->cur_ptr + 2) = (count & NODE_DATA_LEN_MASK);
		dd->cur_ptr += 3;
		node_num++;
		total -= count;
		dev_dbg(dd->dev, "processed: %d Bytes...\n", count);
		in_sg = sg_next(in_sg);
		out_sg = sg_next(out_sg);
		WARN_ON(((total != 0) && (!in_sg || !out_sg)));
	}

out:
	dd->cur_dma_ptr += (6 * sizeof(int) + 3 * sizeof(int) * node_num);
	rctx->node_num = node_num;
	if (dd->flags & FLAGS_FIRST_LIST) {
		aes_writel(dd, ((6 * sizeof(int) + node_num * 3 * sizeof(int)) &
					NEXT_LIST_LEN_MASK), SPRD_AES_LIST_LEN);
		dd->flags &= ~FLAGS_FIRST_LIST;
	} else {
		*(dd->lptr + 1) = (6 * sizeof(int) + node_num * 3 * sizeof(int))
							& NEXT_LIST_LEN_MASK;
	}
	/* setup update iv_sector_cnt flag in list_mode[2], and list_mode[1] */
	if ((dd->flags & FLAGS_CBC) || (dd->flags & FLAGS_CTR) ||
						(dd->flags & FLAGS_XTS))
		*(dd->lptr + 1) |= (CMD_AES_UPD_IV_SEC_CNT << CMD_BIT16_SHIFT);
	*(dd->lptr + 1) &= ~(CMD_AES_LIST_END << CMD_BIT16_SHIFT);
	dd->lptr = tmp_ptr;

	dev_dbg(dd->dev, "total: %d\n", total);
	dev_dbg(dd->dev, "%s is done\n", __func__);

	return ret;
}

static int sprd_aes_handle_req(struct sprd_aes_dev *dd)
{
	int ret = 0, retn = 0, i, total, count;
	struct ablkcipher_request *req;
	struct crypto_async_request *async_req, *temp_areq;
	struct sprd_aes_reqctx *rctx;
	struct scatterlist *in_sg, *out_sg;
	int dma_max = AES_LINK_LIST_BUF_SIZE_BYTES;

	if (!dd)
		return -EINVAL;
	/* whether the async queue is empty ? it is mainly used for debug*/
	if (list_empty(&(dd->queue.list)))
		dev_dbg(dd->dev, "async queue is empty\n");
	/* take mutex to access the aes engine */
	mutex_lock(&aes_lock);
	dd->cur_queue_num = dd->queue.qlen;
	if (!dd->cur_queue_num) {
		clear_bit(FLAGS_BUSY, &dd->flags);
		ret |= -ENODATA;
		goto out;
	}
	dd->cur_ptr = dd->lptr = dd->link_list;
	dd->cur_dma_ptr = dd->link_list_buf;
	dd->flags |= FLAGS_FIRST_LIST;
	aes_writel(dd, dd->cur_dma_ptr, SPRD_AES_LIST_PTR);
	for (i = 0; i < dd->cur_queue_num; i++) {
		retn = sprd_aes_fill_list(dd);
		if (retn) {
			retn = 0;
		} else {
			retn |= -EFAULT;
			dev_err(dd->dev, "%d:%s error\n", __LINE__, __func__);
			break;
		}
	}
	/* update_iv_sec_cnt */
	if ((dd->flags & FLAGS_CBC) || (dd->flags & FLAGS_CTR) ||
						(dd->flags & FLAGS_XTS))
		*(dd->lptr + 1) |= (CMD_AES_UPD_IV_SEC_CNT << CMD_BIT16_SHIFT);
	/* setup end flag */
	*(dd->lptr + 1) |= (CMD_AES_LIST_END_FLAG << CMD_BIT16_SHIFT);
	ret = aes_start_crypt(dd, dd->flags);
	if (ret) {
		dev_err(dd->dev, "aes_start_crypt fail(%d)\n", ret);
	}
	list_for_each_entry_safe(async_req, temp_areq, &dd->ce_req_list, list) {
		i = 0;
		req = ablkcipher_request_cast(async_req);
		rctx = ablkcipher_request_ctx(req);
		total = req->nbytes;
		in_sg = req->src;
		out_sg = req->dst;
		while (total && (i < rctx->node_num)) {
			count = min_t(int, sg_dma_len(in_sg), dma_max);
			dma_unmap_sg(dd->dev, out_sg, 1, DMA_FROM_DEVICE);
			dma_unmap_sg(dd->dev, in_sg, 1, DMA_TO_DEVICE);
			total -= count;
			i++;
			in_sg = sg_next(in_sg);
			out_sg = sg_next(out_sg);
		}
		list_del(&async_req->list);
		async_req->complete(async_req, ret);
	}
out:
	ret |= retn;
	mutex_unlock(&aes_lock);

	/* whether the req restore queue is empty ? it is mainly used for debug.*/
	if (list_empty(&dd->ce_req_list))
		dev_dbg(dd->dev, "req restore queue is done(empty)\n");

	return ret;
}

static void aes_workqueue_handler(struct work_struct *work)
{
	struct sprd_aes_dev *dd = aes_dev;
	int ret;

	clk_main_aes_enable(dd);
	/* empty the crypto queue and then return */
	do {
		ret = sprd_aes_handle_req(dd);
	} while (!ret);
	clk_main_aes_disable(dd);
}

static irqreturn_t aes_irq(int irq, void *dev_id)
{
	struct sprd_aes_dev *dd = (struct sprd_aes_dev *)dev_id;
	uint value = aes_readl(dd, SPRD_AES_INT_STATUS);
	int busy = test_bit(FLAGS_BUSY, &dd->flags);

	if (!busy) {
		dev_info(dd->dev, "exceptional interrupt...\n");
		return IRQ_NONE;
	}
	/*clear list end irq*/
	aes_writel(dd, CMD_AES_LIST_END_INT | CMD_AES_LIST_DONE_INT,
							SPRD_AES_INT_CLEAR);
	if (value & CMD_AES_LIST_END_INT) {
		complete(&dd->op_complete);
	} else {
		/* the following print is used for debuging. */
		if (value & CMD_AES_LIST_DONE_INT) {
			dev_err(dd->dev, "irq err: only done interrupt!\n");
		} else if (value & CMD_AES_LEN_ERR_INT) {
			dev_err(dd->dev, "irq err: src/dst length error!\n");
			aes_writel(dd, CMD_AES_LEN_ERR_INT, SPRD_AES_INT_CLEAR);
		} else {
			dev_err(dd->dev, "irq status:0x%x: unknow error!",
					aes_readl(dd, SPRD_AES_INT_STATUS));
			aes_writel(dd, AES_ERR_INT_MASK, SPRD_AES_INT_CLEAR);
		}
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

static int sprd_aes_crypt(struct ablkcipher_request *req, ulong mode)
{
	struct sprd_aes_reqctx *rctx = ablkcipher_request_ctx(req);
	struct sprd_aes_dev *dd = aes_dev;
	ulong flags;
	int err = 0;
	int busy;

	dev_dbg(dd->dev, "sprd-aes-crypt # nbytes: %d, enc: %d, cbc: %d, ctr: %d, xts: %d\n",
		req->nbytes, !!(mode & FLAGS_ENCRYPT), !!(mode & FLAGS_CBC),
		!!(mode & FLAGS_CTR), !!(mode & FLAGS_XTS));

	rctx->mode = mode;
	spin_lock_irqsave(&dd->lock, flags);
	err = ablkcipher_enqueue_request(&dd->queue, req);
	busy = test_and_set_bit(FLAGS_BUSY, &dd->flags);
	spin_unlock_irqrestore(&dd->lock, flags);
	if (!busy)
		queue_work(aes_wq, &aes_work);

	return err;
}

static int sprd_aes_setkey(struct crypto_ablkcipher *tfm, const unchar *key,
			   uint keylen)
{
	struct sprd_aes_ctx *ctx = crypto_ablkcipher_ctx(tfm);
	struct sprd_aes_dev *dd = aes_dev;

	if ((keylen != AES_KEYSIZE_128) && (keylen != AES_KEYSIZE_192) &&
	     (keylen != AES_KEYSIZE_256) && (keylen != 2 * AES_KEYSIZE_256)) {
		dev_err(dd->dev, "unsupported key size\n");
		crypto_ablkcipher_set_flags(tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);

		return -EINVAL;
	}
	ctx->dd = dd;
	if (key) {
		memcpy(ctx->key, key, keylen);
		ctx->keylen = keylen;
		dd->flags |= FLAGS_NEW_KEY;

		return 0;
	} else
		return -EINVAL;
}

static int sprd_aes_ecb_encrypt(struct ablkcipher_request *req)
{
	return sprd_aes_crypt(req, FLAGS_ENCRYPT | FLAGS_ECB);
}

static int sprd_aes_ecb_decrypt(struct ablkcipher_request *req)
{
	return sprd_aes_crypt(req, FLAGS_DECRYPT | FLAGS_ECB);
}

static int sprd_aes_cbc_encrypt(struct ablkcipher_request *req)
{
	return sprd_aes_crypt(req, FLAGS_ENCRYPT | FLAGS_CBC);
}

static int sprd_aes_cbc_decrypt(struct ablkcipher_request *req)
{
	return sprd_aes_crypt(req, FLAGS_DECRYPT | FLAGS_CBC);
}

static int sprd_aes_ctr_encrypt(struct ablkcipher_request *req)
{
	return sprd_aes_crypt(req, FLAGS_ENCRYPT | FLAGS_CTR);
}

static int sprd_aes_ctr_decrypt(struct ablkcipher_request *req)
{
	return sprd_aes_crypt(req, FLAGS_DECRYPT | FLAGS_CTR);
}

static int sprd_aes_xts_encrypt(struct ablkcipher_request *req)
{
	return sprd_aes_crypt(req, FLAGS_ENCRYPT | FLAGS_XTS);
}

static int sprd_aes_xts_decrypt(struct ablkcipher_request *req)
{
	return sprd_aes_crypt(req, FLAGS_DECRYPT | FLAGS_XTS);
}

static int sprd_aes_cra_init(struct crypto_tfm *tfm)
{
	tfm->crt_ablkcipher.reqsize = sizeof(struct sprd_aes_reqctx);

	return 0;
}

void sprd_aes_cra_exit(struct crypto_tfm *tfm)
{
	return;
}

static struct crypto_alg algs[] = {
	{
		.cra_name = "ecb(aes)",
		.cra_driver_name = "ecb-aes-sprd",
		.cra_priority = 300,
		.cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize = AES_BLOCK_SIZE,
		.cra_ctxsize = sizeof(struct sprd_aes_ctx),
		.cra_alignmask = 0xf,
		.cra_type = &crypto_ablkcipher_type,
		.cra_module = THIS_MODULE,
		.cra_init = sprd_aes_cra_init,
		.cra_exit = sprd_aes_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.setkey = sprd_aes_setkey,
			.encrypt = sprd_aes_ecb_encrypt,
			.decrypt = sprd_aes_ecb_decrypt,
		},
	}, {
		.cra_name = "cbc(aes)",
		.cra_driver_name = "cbc-aes-sprd",
		.cra_priority = 300,
		.cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize = AES_BLOCK_SIZE,
		.cra_ctxsize = sizeof(struct sprd_aes_ctx),
		.cra_alignmask = 0xf,
		.cra_type = &crypto_ablkcipher_type,
		.cra_module = THIS_MODULE,
		.cra_init = sprd_aes_cra_init,
		.cra_exit = sprd_aes_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize = AES_BLOCK_SIZE,
			.setkey = sprd_aes_setkey,
			.encrypt = sprd_aes_cbc_encrypt,
			.decrypt = sprd_aes_cbc_decrypt,
		}
	}, {
		.cra_name = "ctr(aes)",
		.cra_driver_name = "ctr-aes-sprd",
		.cra_priority = 300,
		.cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize = AES_BLOCK_SIZE,
		.cra_ctxsize = sizeof(struct sprd_aes_ctx),
		.cra_alignmask = 0xf,
		.cra_type = &crypto_ablkcipher_type,
		.cra_module = THIS_MODULE,
		.cra_init = sprd_aes_cra_init,
		.cra_exit = sprd_aes_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize = AES_BLOCK_SIZE,
			.setkey = sprd_aes_setkey,
			.encrypt = sprd_aes_ctr_encrypt,
			.decrypt = sprd_aes_ctr_decrypt,
		}
	}, {
		.cra_name = "xts(aes)",
		.cra_driver_name = "xts-aes-sprd",
		.cra_priority = 300,
		.cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize = AES_BLOCK_SIZE,
		.cra_ctxsize = sizeof(struct sprd_aes_ctx),
		.cra_alignmask = 0xf,
		.cra_type = &crypto_ablkcipher_type,
		.cra_module = THIS_MODULE,
		.cra_init = sprd_aes_cra_init,
		.cra_exit = sprd_aes_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize = 2 * AES_MIN_KEY_SIZE,
			.max_keysize = 2 * AES_MAX_KEY_SIZE,
			.ivsize = AES_BLOCK_SIZE,
			.setkey = sprd_aes_setkey,
			.encrypt = sprd_aes_xts_encrypt,
			.decrypt = sprd_aes_xts_decrypt,
		}
	}

};

static int sprd_aes_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sprd_aes_dev *dd;
	struct resource *res;
	unsigned long crypto_sys_en;
	void __iomem *crypto_apb_base;
	unsigned int reg = 0;
	int err = -ENOMEM, i = 0, j;

	dd = devm_kzalloc(dev, sizeof(struct sprd_aes_dev), GFP_KERNEL);
	if (dd == NULL) {
		dev_err(dev, "unable to alloc dev memory\n");
		goto init_err;
	}
	dd->dev = dev;
	platform_set_drvdata(pdev, dd);

	dev_info(dev, "aes acceletor probe......\n");

	spin_lock_init(&dd->lock);
	crypto_init_queue(&dd->queue, SPRD_AES_QUEUE_LENGTH);
	INIT_LIST_HEAD(&dd->ce_req_list);
	/* Get the module base address */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "invalid resource type: base address\n");
		err = -ENODEV;
		goto init_err;
	}
	if (!devm_request_mem_region(&pdev->dev, res->start,
						resource_size(res),
						dev_name(&pdev->dev))) {
		dev_err(&pdev->dev, "Couldn't request memory\n");
		err = -ENODEV;
		goto init_err;
	}
	dd->io_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!dd->io_base) {
		dev_err(dev, "can't remap register space\n");
		err = -ENOMEM;
		goto init_err;
	}
	/* get the irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(dev, "invalid resource type: irq\n");
		err = -ENODEV;
		goto init_err;
	}
	dd->irq = res->start;
	err = devm_request_irq(dev, dd->irq, aes_irq, IRQF_TRIGGER_HIGH |
						IRQF_SHARED, "sprd-aes", dd);
	if (err) {
		dev_err(dev, "Fail to request irq : errno: %d\n", err);
		goto init_err;
	}
	/* enable the outside registers. */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(dev, "invalid resource type: crypto_sys_en\n");
		err = -ENODEV;
		goto init_err;
	}
	crypto_sys_en = (unsigned long)ioremap_nocache(res->start,
						       resource_size(res));
	sci_glb_set(crypto_sys_en + 0x04, 0x8000);
	iounmap((void *)crypto_sys_en);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!res) {
		dev_err(dev, "invalid resource type: crypro_apb_base\n");
		err = -ENODEV;
		goto init_err;
	}
	crypto_apb_base = ioremap_nocache(res->start, resource_size(res));
	reg = readl(crypto_apb_base + 0x0);
	reg |= 0x1;
	writel(reg, crypto_apb_base);
	iounmap(crypto_apb_base);
	/* Enable the main and aes clock */
	clk_main_aes_enable(dd);
	/* the follow contiguous memory is allocated as hardware key table */
	dd->key_base = dma_alloc_coherent(dev, 2 * AES_MAX_KEY_SIZE,
							&dd->key_phys_base,
								GFP_KERNEL);
	if (!dd->key_base) {
		dev_err(dev, "can't allocate key buffer\n");
		err = -ENOMEM;
		goto key_err;
	}
	dd->link_list = dma_alloc_coherent(dev, AES_LINK_LIST_BUF_SIZE_BYTES,
						&dd->link_list_buf, GFP_KERNEL);
	if (!dd->link_list) {
		dev_err(dev, "can't allocate link-list buffer\n");
		err = -ENOMEM;
		goto link_list_buf_err;
	}
	init_completion(&dd->op_complete);
	aes_wq = alloc_workqueue("sprd_aes_wq", WQ_HIGHPRI | WQ_UNBOUND, 1);
	if (!aes_wq) {
		dev_err(dev, "alloc_workqueue is failed\n");
		err = -ENOMEM;
		goto wq_err;
	}
	mutex_init(&aes_lock);
	aes_dev = dd;
	for (i = 0; i < ARRAY_SIZE(algs); i++) {
		err = crypto_register_alg(&algs[i]);
		if (err)
			goto alg_err;
	}

	dev_info(dev, "sprd-aes driver has been registered\n");

	return 0;

alg_err:
	for (j = 0; j < i; j++)
		crypto_unregister_alg(&algs[j]);
	aes_dev = NULL;
	destroy_workqueue(aes_wq);
wq_err:
	dma_free_coherent(dev, AES_LINK_LIST_BUF_SIZE_BYTES,
				  dd->link_list, dd->link_list_buf);
link_list_buf_err:
	dma_free_coherent(dev, 2 * AES_MAX_KEY_SIZE, dd->key_base,
							dd->key_phys_base);
key_err:
init_err:
	dev_err(dev, "sprd-aes initialization is failed(errno:%d).\n", -err);

	return err;
}

static int sprd_aes_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sprd_aes_dev *dd = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < ARRAY_SIZE(algs); i++)
		crypto_unregister_alg(&algs[i]);
	cancel_work_sync(&aes_work);
	destroy_workqueue(aes_wq);
	dma_free_coherent(dev, 2 * AES_MAX_KEY_SIZE, dd->key_base,
							dd->key_phys_base);
	dma_free_coherent(dev, AES_LINK_LIST_BUF_SIZE_BYTES,
				dd->link_list, dd->link_list_buf);
	clk_aes_disable(dd);
	aes_dev = NULL;

	return 0;
}

static struct of_device_id sprd_aes_of_match[] = {
	{ .compatible = "sprd,sprd-aes", },
	{ },
};

#ifdef CONFIG_PM_SLEEP
static int sprd_aes_suspend(struct device *dev)
{
	struct sprd_aes_dev *pd = dev_get_drvdata(dev);

	clk_main_aes_disable(pd);

	return 0;
}

static int sprd_aes_resume(struct device *dev)
{
	struct sprd_aes_dev *pd = dev_get_drvdata(dev);

	clk_main_aes_enable(pd);

	return 0;
}

static const struct dev_pm_ops sprd_aes_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sprd_aes_suspend, sprd_aes_resume)
};
#endif

static struct platform_driver sprd_aes_driver = {
	.probe  = sprd_aes_probe,
	.remove = sprd_aes_remove,
	.driver = {
		.name   = "sprd-aes",
		.owner  = THIS_MODULE,
		.of_match_table = sprd_aes_of_match,
#ifdef CONFIG_PM_SLEEP
		.pm = &sprd_aes_pm_ops,
#endif
	},
};

module_platform_driver(sprd_aes_driver);

MODULE_DESCRIPTION("SPREADTRUM AES(ECB/CBC/CTR/XTS) hw acceleration support.");
MODULE_AUTHOR("xiaoran.li@spreadtrum.com");
MODULE_LICENSE("GPL v2");
