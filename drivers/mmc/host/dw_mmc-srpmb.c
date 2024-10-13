/*
 * Secure RPMB Driver for Exynos MMC RPMB
 *
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/dma-mapping.h>
#include <linux/mmc/core.h>
#include <linux/mmc/ioctl.h>
#include <linux/mmc/mmc.h>
#include <linux/delay.h>
#include <linux/suspend.h>
#include <soc/samsung/exynos-smc.h>

#include "../core/queue.h"
#include "dw_mmc-srpmb.h"

#define MMC_SRPMB_DEVICE_PROPNAME	"samsung,mmc-srpmb"
#define MMC_BLK_NAME			"/dev/block/mmcblk0"

static inline void wakesrc_destroy(struct wakeup_source *ws)
{
	wakeup_source_remove(ws);

	if (!ws)
		return;

	__pm_relax(ws);
}

#if defined(DEBUG_SRPMB)
static void dump_packet(u8 *data, int len)
{
	u8 s[17];
	int i, j;

	s[16]='\0';
	for (i = 0; i < len; i += 16) {
		printk("%06x :", i);
		for (j=0; j<16; j++) {
			printk(" %02x", data[i+j]);
			s[j] = (data[i+j]<' ' ? '.' : (data[i+j]>'}' ? '.' : data[i+j]));
		}
		printk(" |%s|\n",s);
	}
	printk("\n");
}
#endif

static void swap_packet(u8 *p, u8 *d)
{
	int i;
	for (i = 0; i < RPMB_PACKET_SIZE; i++)
		d[i] = p[RPMB_PACKET_SIZE - 1 - i];
}

static void mmc_cmd_init(struct mmc_ioc_cmd *icmd)
{
	memset(icmd, 0, sizeof(struct mmc_ioc_cmd));

	icmd->is_acmd = false;
	icmd->arg = 0;
	icmd->flags = MMC_RSP_R1;
	icmd->blksz = RPMB_PACKET_SIZE;
	icmd->blocks = 1;
	icmd->postsleep_min_us = 0;
	icmd->postsleep_max_us = 0;
	icmd->data_timeout_ns = 0;
	icmd->cmd_timeout_ms = 0;
}

static void update_rpmb_status_flag(struct _mmc_rpmb_ctx *ctx,
				struct _mmc_rpmb_req *req, int status)
{
	unsigned long flags;

	spin_lock_irqsave(&ctx->lock, flags);
	req->status_flag = status;
	spin_unlock_irqrestore(&ctx->lock, flags);

	return;
}

static struct mmc_blk_ioc_data *mmc_blk_ioctl_copy_from_kernel(
	struct mmc_ioc_cmd *icmd)
{
	struct mmc_blk_ioc_data *idata;
	int err;

	idata = kmalloc(sizeof(*idata), GFP_NOIO);
	if (!idata) {
		err = -ENOMEM;
		goto out;
	}

	memcpy(&idata->ic, icmd, sizeof(idata->ic));

	idata->buf_bytes = (u64) idata->ic.blksz * idata->ic.blocks;
	if (idata->buf_bytes > MMC_IOC_MAX_BYTES) {
		err = -EOVERFLOW;
		goto idata_err;
	}

	if (!idata->buf_bytes) {
		idata->buf = NULL;
		return idata;
	}

	idata->buf = kmalloc(idata->buf_bytes, GFP_NOIO);
	if (!idata->buf) {
		err = -ENOMEM;
		goto idata_err;
	}

	memcpy(idata->buf, (void *)idata->ic.data_ptr, idata->buf_bytes);

	return idata;

idata_err:
	kfree(idata);
out:
	return ERR_PTR(err);
}

static void mmc_blk_ioctl_copy_to_kernel(struct mmc_ioc_cmd *icmd,
				      struct mmc_blk_ioc_data *idata)
{
	struct mmc_ioc_cmd *ic = &idata->ic;

	memcpy(icmd->response, ic->response, sizeof(ic->response));

	if (!idata->ic.write_flag)
		memcpy((void *)icmd->data_ptr, idata->buf, idata->buf_bytes);
}

static long mmc_srpmb_access_ioctl(struct mmc_blk_data *md,
				struct mmc_rpmb_data *rpmb,
				struct mmc_ioc_cmd *icmd)
{
	struct mmc_blk_ioc_data *idata;
	struct mmc_blk_ioc_data *idatas[1];
	struct mmc_queue *mq;
	struct mmc_card *card;
	struct request *req;
	int err = 0, ioc_err = 0;

	if (IS_ERR(md)) {
		pr_err("%s: invalid mmc_blk_data structure ptr\n", __func__);
		return PTR_ERR(md);
	}

	if (IS_ERR(rpmb)) {
		pr_err("%s: invalid rpmb structure ptr\n", __func__);
		return PTR_ERR(rpmb);
	}

	idata = mmc_blk_ioctl_copy_from_kernel(icmd);
	if (IS_ERR(idata)) {
		pr_err("%s: invalid idata ptr\n", __func__);
		return PTR_ERR(idata);
	}

	idata->rpmb = rpmb;

	card = md->queue.card;
	if (IS_ERR(card)) {
		err = PTR_ERR(card);
		pr_err("%s: invalid card ptr\n", __func__);
		goto cmd_done;
	}

	mq = &md->queue;
	req = blk_get_request(mq->queue, idata->ic.write_flag ? REQ_OP_DRV_OUT : REQ_OP_DRV_IN, 0);
	if (IS_ERR(req)) {
		err = PTR_ERR(req);
		pr_err("%s: invalid req ptr\n", __func__);
		goto cmd_done;
	}

	idatas[0] = idata;
	req_to_mmc_queue_req(req)->drv_op = MMC_DRV_OP_IOCTL_RPMB;
	req_to_mmc_queue_req(req)->drv_op_data = idatas;
	req_to_mmc_queue_req(req)->ioc_count = 1;
	blk_execute_rq(mq->queue, NULL, req, 0);
	ioc_err = req_to_mmc_queue_req(req)->drv_op_result;
	mmc_blk_ioctl_copy_to_kernel(icmd, idata);
	blk_put_request(req);

cmd_done:
	kfree(idata->buf);
	kfree(idata);

	return ioc_err ? ioc_err : err;
}

static int mmc_rpmb_access(struct _mmc_rpmb_ctx *ctx, struct _mmc_rpmb_req *req)
{
	int ret = 0;
	struct device *dev = ctx->dev;
	struct mmc_ioc_cmd icmd;
	struct rpmb_packet packet;
	static struct block_device *bdev = NULL;
	struct gendisk *gd;
	static struct mmc_blk_data *md;
	static struct mmc_rpmb_data *rpmb;
	struct list_head *pos, *q;
	u8 *result_buf = NULL;

	dev_dbg(dev, "start rpmb workqueue with command(%d)\n", req->type);

	if (bdev == NULL) {
		bdev = blkdev_get_by_path(MMC_BLK_NAME, FMODE_READ, NULL);
		if (IS_ERR(bdev)) {
			dev_err(dev, "Fail to get block device for mmc srpmb\n");
			ret = -ENOENT;
			goto out;
		}

		if ((!capable(CAP_SYS_RAWIO)) || (bdev != bdev->bd_contains)) {
			dev_err(dev, "Invalid bdev setting for mmc srpmb\n");
			ret = -EPERM;
			goto out;
		}

		gd = bdev->bd_disk;
		if (IS_ERR(gd)) {
			dev_err(dev, "Invalid gendisk structure ptr\n");
			ret = PTR_ERR(gd);
			goto out;
		}

		md = gd->private_data;
		if (IS_ERR(md)) {
			dev_err(dev, "Invalid mmc_blk_data structure ptr\n");
			ret = PTR_ERR(md);
			goto out;
		}

		list_for_each_safe(pos, q, &md->rpmbs)
			rpmb = list_entry(pos, struct mmc_rpmb_data, node);

		if (IS_ERR(rpmb)) {
			dev_err(dev, "Invalid rpmb structure ptr\n");
			ret = PTR_ERR(rpmb);
			goto out;
		}
	}

	__pm_stay_awake(&ctx->wakesrc);

	/* Initialize mmc ioc command */
	mmc_cmd_init(&icmd);

	switch(req->type) {
	case GET_WRITE_COUNTER:
		icmd.write_flag = true;
		icmd.flags = MMC_RSP_R1;
		icmd.opcode = MMC_WRITE_MULTIPLE_BLOCK;
		icmd.data_ptr = (unsigned long)req->rpmb_data;

		ret = mmc_srpmb_access_ioctl(md, rpmb, &icmd);
		if (ret != 0) {
			update_rpmb_status_flag(ctx, req,
					WRITE_COUNTER_SECURITY_OUT_ERROR);
			dev_err(dev, "Fail to execute for srpmb write counter \
				security out: %d\n", ret);
			goto unlock_out;
		}

		memset(req->rpmb_data, 0, RPMB_PACKET_SIZE);
		icmd.write_flag = false;
		icmd.flags = MMC_RSP_R1;
		icmd.opcode = MMC_READ_MULTIPLE_BLOCK;

		ret = mmc_srpmb_access_ioctl(md, rpmb, &icmd);
		if (ret != 0) {
			update_rpmb_status_flag(ctx, req,
					WRITE_COUNTER_SECURITY_IN_ERROR);
			dev_err(dev, "Fail to execute for srpmb write counter \
				security in: %d\n", ret);
			goto unlock_out;
		}
		if (req->rpmb_data[RPMB_RESULT] || req->rpmb_data[RPMB_RESULT+1]) {
			dev_info(dev, "GET_WRITE_COUNTER: REQ/RES = %02x%02x, RESULT = %02x%02x\n",
			req->rpmb_data[RPMB_REQRES], req->rpmb_data[RPMB_REQRES+1],
			req->rpmb_data[RPMB_RESULT], req->rpmb_data[RPMB_RESULT+1]);
		}

		update_rpmb_status_flag(ctx, req, RPMB_PASSED);
		break;
	case WRITE_DATA:
		icmd.write_flag = RELIABLE_WRITE_REQ_SET;
		icmd.flags = MMC_RSP_R1;
		icmd.blocks = req->data_len / RPMB_PACKET_SIZE;
		icmd.opcode = MMC_WRITE_MULTIPLE_BLOCK;
		icmd.data_ptr = (unsigned long)req->rpmb_data;

		if (icmd.blocks == 0) {
			dev_err(dev, "Invalid block size from secure world\n"
					"cmd(%d), type(%d), data length(%d)\n",
					req->cmd, req->type, req->data_len);
			ret = -EINVAL;
			break;
		}

		/* program data packet */
		ret = mmc_srpmb_access_ioctl(md, rpmb, &icmd);
		if (ret != 0) {
			update_rpmb_status_flag(ctx, req, WRITE_DATA_SECURITY_OUT_ERROR);
			dev_err(dev, "Fail to write block for program data: %d\n", ret);
			goto unlock_out;
		}

		result_buf = (u8 *)kzalloc(RPMB_PACKET_SIZE, GFP_NOIO);
		if (result_buf == NULL) {
			dev_err(dev, "Memory allocation failed\n");
			ret = -1;
			break;
		}
		icmd.write_flag = true;
		icmd.blocks = 1;
		icmd.data_ptr = (unsigned long)result_buf;
		memset(&packet, 0, RPMB_PACKET_SIZE);
		packet.request = RESULT_READ_REQ;
		swap_packet((u8 *)&packet, result_buf);

		/* result read request */
		ret = mmc_srpmb_access_ioctl(md, rpmb, &icmd);
		if (ret != 0) {
			update_rpmb_status_flag(ctx, req, WRITE_DATA_SECURITY_OUT_ERROR);
			dev_err(dev, "Fail to write block for result: %d\n", ret);
			goto free_out;
		}

		memset(result_buf, 0, RPMB_PACKET_SIZE);
		icmd.write_flag = false;
		icmd.blocks = 1;
		icmd.opcode = MMC_READ_MULTIPLE_BLOCK;

		/* read multiple block for response */
		ret = mmc_srpmb_access_ioctl(md, rpmb, &icmd);
		if (ret != 0) {
			update_rpmb_status_flag(ctx, req, WRITE_DATA_SECURITY_IN_ERROR);
			dev_err(dev, "Fail to read block for response: %d\n", ret);
			goto free_out;
		}
		if (result_buf[RPMB_RESULT] || result_buf[RPMB_RESULT+1]) {
			dev_info(dev, "WRITE_DATA: REQ/RES = %02x%02x, RESULT = %02x%02x\n",
			result_buf[RPMB_REQRES], result_buf[RPMB_REQRES+1],
			result_buf[RPMB_RESULT], result_buf[RPMB_RESULT+1]);
		}

		memcpy(req->rpmb_data, result_buf, RPMB_PACKET_SIZE);
		update_rpmb_status_flag(ctx, req, RPMB_PASSED);
		kfree(result_buf);
		break;
	case READ_DATA:
		icmd.write_flag = true;
		icmd.flags = MMC_RSP_R1;
		icmd.opcode = MMC_WRITE_MULTIPLE_BLOCK;
		icmd.data_ptr = (unsigned long)req->rpmb_data;

		result_buf = (u8 *)kzalloc(RPMB_PACKET_SIZE, GFP_NOIO);
		if (result_buf == NULL) {
			dev_err(dev, "Memory allocation failed\n");
			ret = -1;
			break;
		}

		/* At read data with MMC, block count has to '0' */
		swap_packet(req->rpmb_data, result_buf);
		((struct rpmb_packet *)(result_buf))->count = 0;
		swap_packet(result_buf, req->rpmb_data);

		kfree(result_buf);

		/* read data packet */
		ret = mmc_srpmb_access_ioctl(md, rpmb, &icmd);
		if (ret != 0) {
			update_rpmb_status_flag(ctx, req, READ_DATA_SECURITY_OUT_ERROR);
			dev_err(dev, "Fail to write block for read data: %d\n", ret);
			goto unlock_out;
		}

		memset(req->rpmb_data, 0, req->data_len);
		icmd.write_flag = false;
		icmd.opcode = MMC_READ_MULTIPLE_BLOCK;
		icmd.blocks = req->data_len/RPMB_PACKET_SIZE;

		if (icmd.blocks == 0) {
			dev_err(dev, "Invalid block size from secure world\n"
					"cmd(%d), type(%d), data length(%d)\n",
					req->cmd, req->type, req->data_len);
			ret = -EINVAL;
			break;
		}

		/* read multiple block for response */
		ret = mmc_srpmb_access_ioctl(md, rpmb, &icmd);
		if (ret != 0) {
			update_rpmb_status_flag(ctx, req, READ_DATA_SECURITY_IN_ERROR);
			dev_err(dev, "Fail to read block for response: %d\n", ret);
			goto unlock_out;
		}
		if (req->rpmb_data[RPMB_RESULT] || req->rpmb_data[RPMB_RESULT+1]) {
			dev_info(dev, "READ_DATA: REQ/RES = %02x%02x, RESULT = %02x%02x\n",
			req->rpmb_data[RPMB_REQRES], req->rpmb_data[RPMB_REQRES+1],
			req->rpmb_data[RPMB_RESULT], req->rpmb_data[RPMB_RESULT+1]);
		}

		update_rpmb_status_flag(ctx, req, RPMB_PASSED);
		break;
	default:
		dev_err(dev, "Fail to invalid command: %x\n", req->type);
		update_rpmb_status_flag(ctx, req, RPMB_INVALID_COMMAND);
		ret = -EINVAL;
	}

	__pm_relax(&ctx->wakesrc);
	dev_dbg(dev, "finish rpmb workqueue with command(%d)\n", req->type);

	return ret;

free_out:
	kfree(result_buf);
unlock_out:
	__pm_relax(&ctx->wakesrc);
out:
	bdev = NULL;
	return ret;
}

static void mmc_rpmb_worker(struct work_struct *work)
{
	int ret;
	struct device *dev;
	struct _mmc_rpmb_ctx *ctx;
	struct _mmc_rpmb_req *req;

	if (!work) {
		printk(KERN_ERR "Fail to get work for mmc rpmb\n");
		return;
	}

	ctx = container_of(work, struct _mmc_rpmb_ctx, work);
	dev = ctx->dev;
	req = (struct _mmc_rpmb_req *)ctx->wsm_virtaddr;
	if (!req) {
		dev_err(dev, "Invalid wsm virtual address for rpmb\n");
		return;
	}

	ret = mmc_rpmb_access(ctx, req);
	if (ret) {
		dev_err(dev, "Fail to access mmc rpmb\n");
		return;
	}

	return;
}

static int mmc_rpmb_pm_notifier(struct notifier_block *nb, unsigned long event,
				void *dummy)
{
	struct device *dev;
	struct _mmc_rpmb_ctx *ctx;
	struct _mmc_rpmb_req *req;

	if (!nb) {
		printk(KERN_ERR "noti_blk work_struct data invalid\n");
		return -EINVAL;
	}

	ctx = container_of(nb, struct _mmc_rpmb_ctx, pm_notifier);
	dev = ctx->dev;
	req = (struct _mmc_rpmb_req *)ctx->wsm_virtaddr;
	if (!req) {
		dev_err(dev, "Invalid wsm address for rpmb\n");
		return -EINVAL;
	}

	switch (event) {
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
	case PM_RESTORE_PREPARE:
		flush_workqueue(ctx->srpmb_queue);
		if (req->status_flag != RPMB_PASSED)
			update_rpmb_status_flag(ctx, req, RPMB_FAIL_SUSPEND_STATUS);
		break;
	case PM_POST_SUSPEND:
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
		if (req->status_flag != RPMB_PASSED)
			update_rpmb_status_flag(ctx, req, 0);
		break;
	default:
		break;
	}

	return 0;
}

static irqreturn_t mmc_rpmb_interrupt(int intr, void *arg)
{
	struct _mmc_rpmb_ctx *ctx = (struct _mmc_rpmb_ctx *)arg;
	struct device *dev;
	struct _mmc_rpmb_req *req;

	dev = ctx->dev;
	req = (struct _mmc_rpmb_req *)ctx->wsm_virtaddr;
	if (!req) {
		dev_err(dev, "Invalid wsm address for rpmb\n");
		return IRQ_HANDLED;
	}

	update_rpmb_status_flag(ctx, req, RPMB_IN_PROGRESS);
	queue_work(ctx->srpmb_queue, &ctx->work);

	return IRQ_HANDLED;
}

static int init_mmc_srpmb(struct platform_device *pdev, struct _mmc_rpmb_ctx *ctx)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct resource *res;

	if (!ctx) {
		dev_err(dev, "Invalid rpmb context address\n");
		return -ENOMEM;
	}

	/* allocation for wsm(world shared memory) */
	ctx->wsm_virtaddr = dma_alloc_coherent(dev,
				sizeof(struct _mmc_rpmb_req) + RPMB_BUF_MAX_SIZE,
				&ctx->wsm_phyaddr, GFP_KERNEL);
	if (!ctx->wsm_virtaddr || !ctx->wsm_phyaddr) {
		dev_err(dev, "Fail to alloc for srpmb wsm (world shared memory)\n");
		goto alloc_wsm_fail;
	}

	dev_info(dev, "%s: srpmb: wsm initialized successfully\n", __func__);
	dev_dbg(dev, "srpmb dma addr: virt_pK(%pK), phy(%llx)\n",
			ctx->wsm_virtaddr, (uint64_t)ctx->wsm_phyaddr);

	/* get mmc srpmb irq number */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(dev, "Fail to get IRQ resource\n");
		goto get_irq_fail;
	}

	ctx->irq = res->start;
	if (ctx->irq <= 0) {
		dev_err(dev, "Fail to get irq number for mmc rpmb\n");
		goto get_irq_fail;
	}

	return ret;

get_irq_fail:
	dma_free_coherent(dev, sizeof(struct _mmc_rpmb_req) + RPMB_BUF_MAX_SIZE, ctx->wsm_virtaddr,
				ctx->wsm_phyaddr);
alloc_wsm_fail:
	ret = -ENOMEM;
	return ret;
}

static int mmc_srpmb_probe(struct platform_device *pdev)
{
	int ret;
	unsigned long smc_ret;
	struct _mmc_rpmb_ctx *ctx;
	struct device *dev = &pdev->dev;
        struct irq_data *rpmb_irqd;
        irq_hw_number_t hwirq;

	dma_set_mask(&pdev->dev, DMA_BIT_MASK(36));
	/* allocation for rpmb context */
	ctx = kzalloc(sizeof(struct _mmc_rpmb_ctx), GFP_KERNEL);
	if (!ctx) {
		dev_err(dev, "Fail to alloc for mmc rpmb context\n");
		return -1;
	}

	/* initialize mmc rpmb context */
	ret = init_mmc_srpmb(pdev, ctx);
	if (ret) {
		dev_err(dev, "Fail to initialize mmc srpmb\n");
		goto ctx_kfree;
	}

        /* Get irq_data from irq number */
        rpmb_irqd = irq_get_irq_data(ctx->irq);
        if (!rpmb_irqd) {
                dev_err(dev, "Fail to get irq_data from irq number\n");
                goto dma_free;
        }

        /* Get hwirq from irq_data */
        hwirq = irqd_to_hwirq(rpmb_irqd);

        ctx->dev = dev;
        INIT_WORK(&ctx->work, mmc_rpmb_worker);

        /* initialize workqueue for mmc rpmb handler */
        ctx->srpmb_queue = alloc_workqueue("srpmb_wq",
                WQ_MEM_RECLAIM | WQ_UNBOUND | WQ_HIGHPRI, 1);
        if (!ctx->srpmb_queue) {
                dev_err(dev, "Fail to alloc workqueue for mmc srpmb\n");
                goto dma_free;
        }

	/* request irq for mmc rpmb handler */
	ret = request_irq(ctx->irq, mmc_rpmb_interrupt,
			IRQF_TRIGGER_RISING, pdev->name, ctx);
	if (ret) {
		dev_err(dev, "Fail to request irq handler for mmc srpmb\n");
		goto workqueue_free;
	}

	ctx->pm_notifier.notifier_call = mmc_rpmb_pm_notifier;
	ret = register_pm_notifier(&ctx->pm_notifier);
	if (ret) {
		dev_err(dev, "Fail to setup pm notifier\n");
		goto free_irq;
	}

	platform_set_drvdata(pdev, ctx);
	memset(&ctx->wakesrc, 0, sizeof(ctx->wakesrc));
	(&ctx->wakesrc)->name = "srpmb";
	wakeup_source_add(&ctx->wakesrc);
	spin_lock_init(&ctx->lock);

	/* Smc call to transfer wsm address to secure world */
        smc_ret = exynos_smc(SMC_SRPMB_WSM, ctx->wsm_phyaddr, hwirq, 0);
        if (smc_ret){
                dev_err(dev, "Fail to smc call to initial wsm buffer: %x\n", smc_ret);
		goto destroy_wakesrc;
	}
	return 0;

destroy_wakesrc:
	wakesrc_destroy(&ctx->wakesrc);
	unregister_pm_notifier(&ctx->pm_notifier);
free_irq:
	free_irq(ctx->irq, ctx);
workqueue_free:
        destroy_workqueue(ctx->srpmb_queue);
dma_free:
	dma_free_coherent(dev, sizeof(struct _mmc_rpmb_req) + RPMB_BUF_MAX_SIZE, ctx->wsm_virtaddr,
				ctx->wsm_phyaddr);
ctx_kfree:
	kfree(ctx);
	return -1;
}

static int mmc_srpmb_remove(struct platform_device *pdev)
{
	struct _mmc_rpmb_ctx *ctx = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	if (ctx->srpmb_queue)
		destroy_workqueue(ctx->srpmb_queue);

	unregister_pm_notifier(&ctx->pm_notifier);
	dma_free_coherent(dev, RPMB_BUF_MAX_SIZE, ctx->wsm_virtaddr,
			ctx->wsm_phyaddr);

	wakesrc_destroy(&ctx->wakesrc);

	kfree(ctx);
	return 0;
}

static const struct of_device_id of_match_table[] = {
	{ .compatible = MMC_SRPMB_DEVICE_PROPNAME },
	{ }
};

static struct platform_driver mmc_srpmb_plat_driver = {
	.probe = mmc_srpmb_probe,
	.driver = {
		.name = "exynos-mmc-srpmb",
		.owner = THIS_MODULE,
		.of_match_table = of_match_table,
	},
	.remove = mmc_srpmb_remove,
};

static int __init mmc_srpmb_init(void)
{
	return platform_driver_register(&mmc_srpmb_plat_driver);
}

static void __exit mmc_srpmb_exit(void)
{
	platform_driver_unregister(&mmc_srpmb_plat_driver);
}

module_init(mmc_srpmb_init);
module_exit(mmc_srpmb_exit);

MODULE_AUTHOR("Jonghwi Rha <jonghwi.rha@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MMC SRPMB driver");
