// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/types.h>
#include <linux/arm-smccc.h> /* for Kernel Native SMC API */
#include <linux/soc/mediatek/mtk_sip_svc.h> /* for SMC ID table */
#include <linux/of_reserved_mem.h>
#include <linux/of_fdt.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>

#include <mtk-smmu-v3.h>
#include "apusys_secure.h"
#include "npu_scp_ipi.h"
#include "aov_mem_service_v2.h"

struct aov_mem_service_ctx {
	struct completion worker_comp;
	struct task_struct *service_worker;

	spinlock_t lock; // protect received_act & received_pa
	enum npu_scp_mem_service_action received_act;
	uint64_t received_pa;
	struct platform_device *parent_pdev;
	struct iommu_domain *domain;
	uint64_t data_iova;
	size_t data_size;
	uint64_t ctrl_iova;
};

static struct aov_mem_service_ctx *ctx;

static int query_aov_reserved_iova(enum npu_scp_mem_service_action act, uint64_t pa, uint64_t *iova)
{
	struct device_node *aov_node = NULL;
	struct device_node *smmu_node = NULL;
	struct platform_device *smmu_dev = NULL;
	struct arm_smccc_res res;
	struct iommu_domain *smmu_domain;
	int ret = 0;
	u32 reg[2] = {0, 0};
	u32 base, size;
	u32 ctrl_size;

	if (!ctx) {
		pr_info("%s mem service ctx is not available\n", __func__);
		return -ENODEV;
	}

	if (act == NPU_SCP_QUERY_DRAM_DATA_DEVADDR) {
		/* map data iova by smmu or iommu */
		if (ctx->data_iova) {
			*iova = ctx->data_iova;
			pr_debug("%s data iova = %llx (allocated)\n", __func__, ctx->data_iova);
		} else if (smmu_v3_enabled()) {
			aov_node = dev_of_node(&ctx->parent_pdev->dev);
			if (!aov_node) {
				pr_info("%s not found aov node\n", __func__);
				return -EINVAL;
			}

			ret = of_property_read_u32(aov_node, "ctrl-size", &ctrl_size);
			if (ret < 0) {
				pr_info("%s - of_property_read_u32 err : %d\n",
					__func__, ret);
				return -EINVAL;
			}

			smmu_node = of_parse_phandle(aov_node, "smmu-device", 0);
			if (!smmu_node){
				pr_info("%s Failed to get smmu_node smmu-device\n", __func__);
				return -ENODEV;
			}

			ret = of_property_read_u32_array(smmu_node, "mtk,iommu-dma-range", reg, 2);
			if (ret < 0) {
				pr_info("%s - of_property_read_u32_array err : %d\n",
					__func__, ret);
				return -EINVAL;
			}
			base = reg[0];
			size = reg[1];

			smmu_dev = of_find_device_by_node(smmu_node);
			if (!smmu_dev){
				pr_info("%s Failed to get smmu_dev\n", __func__);
				return -ENODEV;
			}

			smmu_domain = iommu_get_domain_for_dev(&smmu_dev->dev);
			if (!smmu_domain) {
				pr_info("%s Failed to get smmu\n", __func__);
				return -ENODEV;
			}
			ctx->domain = smmu_domain;

			ret = iommu_map(smmu_domain, base, pa, size - ctrl_size,
					IOMMU_READ | IOMMU_WRITE);
			if (ret){
				pr_info("%s Failed to immu_map, ret %d\n", __func__, ret);
				return ret;
			}

			ctx->data_iova = base;
			ctx->data_size = size - ctrl_size;
			*iova = base;
			pr_debug("%s data iova = %x (smmu)\n", __func__, base);
		} else {
			arm_smccc_smc(MTK_SIP_APUSYS_CONTROL,
				MTK_APUSYS_KERNEL_OP_APUSYS_AOVDRAM_DATA_IOVA,
				0, 0, 0, 0, 0, 0, &res);
			ctx->data_iova = res.a1;
			*iova = res.a1;
			pr_debug("%s data iova = %lx (non-smmu)\n", __func__, res.a1);
		}
	} else if (act == NPU_SCP_QUERY_DRAM_CTRL_DEVADDR) {
		if (ctx->ctrl_iova) {
			*iova = ctx->ctrl_iova;
			pr_debug("%s ctrl iova = %llx (allocated)\n", __func__, ctx->ctrl_iova);
		} else {
			arm_smccc_smc(MTK_SIP_APUSYS_CONTROL,
				MTK_APUSYS_KERNEL_OP_APUSYS_AOVDRAM_CTRL_IOVA,
				0, 0, 0, 0, 0, 0, &res);
			ctx->ctrl_iova = res.a1;
			*iova = res.a1;
			pr_debug("%s ctrl iova = %lx\n", __func__, res.a1);
		}
	} else
		return -EINVAL;

	return 0;
}

static int query_aov_reserved_mblock(enum npu_scp_mem_service_action act, uint64_t *pa,
				     size_t *size)
{
	struct device_node *mblock_root = NULL, *aov_mem_node = NULL;
	struct reserved_mem *rmem;

	// get reserved pa/size from reserved mem

	mblock_root = of_find_node_by_path("/reserved-memory");
	if (!mblock_root) {
		pr_info("%s not found /reserved-memory\n", __func__);
		return -EINVAL;
	}

	aov_mem_node = of_find_compatible_node(mblock_root, NULL, "mediatek,scp_aov_reserved");
	if (!aov_mem_node) {
		pr_info("%s not found scp_aov_reserved\n", __func__);
		return -EINVAL;
	}

	rmem = of_reserved_mem_lookup(aov_mem_node);
	if (!rmem) {
		pr_info("[%s] ERROR: not found address\n", __func__);
		return -EINVAL;
	}

	if (act == NPU_SCP_QUERY_DRAM_DATA_DEVADDR) {
		*pa = rmem->base;
		*size = rmem->size;
	} else if (act == NPU_SCP_QUERY_DRAM_CTRL_DEVADDR) {
		struct device_node *aov_node = NULL;
		u32 ctrl_size = 0;
		int ret = 0;

		aov_node = dev_of_node(&ctx->parent_pdev->dev);
		if (!aov_node) {
			pr_info("%s not found aov node\n", __func__);
			return -EINVAL;
		}

		ret = of_property_read_u32(aov_node, "ctrl-size", &ctrl_size);
		if (ret < 0) {
			pr_info("%s - of_property_read_u32 err : %d\n", __func__, ret);
			return -EINVAL;
		}

		if (ctrl_size >= rmem->size) {
			pr_info("%s ctrl size %d is greater than reserved size %lld\n",
				__func__, ctrl_size, rmem->size);
			return -EINVAL;
		}

		*pa = rmem->base + (rmem->size - ctrl_size);
		*size = ctrl_size;
	}

	return 0;
}

static int mem_service_reply_error(enum npu_scp_mem_service_action act, int ret)
{
	struct npu_scp_ipi_param send_msg = {
		.cmd = NPU_SCP_MEM_SERVICE,
		.act = NPU_SCP_SEND_ERROR,
		.arg = act,
		.ret = ret,
	};

	return npu_scp_ipi_send(&send_msg, NULL, SCP_IPI_TIMEOUT_MS);
}

static int service_worker_func(void *data)
{
	struct aov_mem_service_ctx *ctx = (struct aov_mem_service_ctx *)data;
	unsigned long flags;

	pr_info("%s start +++\n", __func__);

	while (!kthread_should_stop()) {
		int ret = 0;
		enum npu_scp_mem_service_action received_act;
		uint64_t received_pa;
		uint64_t send_dev_addr;
		uint64_t mem_pa, mem_iova;
		size_t mem_size;
		struct npu_scp_ipi_param send_msg = { 0, 0, 0, 0 };

		wait_for_completion_interruptible(&ctx->worker_comp);

		pr_info("%s start mem service worker +++\n", __func__);

		spin_lock_irqsave(&ctx->lock, flags);
		received_act = ctx->received_act;
		received_pa = ctx->received_pa;

		// reset
		ctx->received_pa = 0;
		ctx->received_act = 0;
		spin_unlock_irqrestore(&ctx->lock, flags);

		ret = query_aov_reserved_mblock(received_act, &mem_pa, &mem_size);
		if (ret) {
			pr_info("%s Failed to query AOV reserved memory\n", __func__);
			mem_service_reply_error(received_act, ret);
			continue;
		}

		ret = query_aov_reserved_iova(received_act, mem_pa, &mem_iova);
		if (ret) {
			pr_info("%s Failed to query IOVA of AOV reserved memory\n", __func__);
			mem_service_reply_error(received_act, ret);
			continue;
		}

		send_msg.cmd = NPU_SCP_MEM_SERVICE;

		switch (received_act) {
		case NPU_SCP_QUERY_DRAM_DATA_DEVADDR:
			if (received_pa < mem_pa || received_pa > mem_pa + mem_size) {
				pr_info("%s received pa is out of range\n", __func__);
				ret = -EINVAL;
				break;
			}
			send_dev_addr = (received_pa - mem_pa) + mem_iova;

			send_msg.act = NPU_SCP_SEND_DRAM_DATA_DEVADDR;
			send_msg.arg = (uint32_t)(send_dev_addr >> 32);
			send_msg.ret = (uint32_t)send_dev_addr;
			break;
		case NPU_SCP_QUERY_DRAM_CTRL_DEVADDR:
			if (received_pa < mem_pa || received_pa > mem_pa + mem_size) {
				pr_info("%s received pa is out of range\n", __func__);
				ret = -EINVAL;
				break;
			}
			send_dev_addr = (received_pa - mem_pa) + mem_iova;

			send_msg.act = NPU_SCP_SEND_DRAM_CTRL_DEVADDR;
			send_msg.arg = (uint32_t)(send_dev_addr >> 32);
			send_msg.ret = (uint32_t)send_dev_addr;
			break;
		default:
			pr_info("%s Unknown action %d\n", __func__, received_act);
			ret = -EINVAL;
			break;
		}

		if (ret == 0) {
			ret = npu_scp_ipi_send(&send_msg, NULL, SCP_IPI_TIMEOUT_MS);
			if (ret)
				pr_info("%s Failed to send to scp, ret %d\n", __func__, ret);
		} else {
			mem_service_reply_error(received_act, ret);
			continue;
		}
	}

	pr_info("%s end ---\n", __func__);

	return 0;
}

static int mem_service_handler(struct npu_scp_ipi_param *recv_msg)
{
	unsigned long flags;
	int ret = 0;

	if (!recv_msg)
		return -EINVAL;

	if (!ctx)
		return -ENODEV;

	spin_lock_irqsave(&ctx->lock, flags);

	switch (recv_msg->act) {
	case NPU_SCP_QUERY_DRAM_DATA_DEVADDR:
		ctx->received_act = NPU_SCP_QUERY_DRAM_DATA_DEVADDR;
		ctx->received_pa = (uint64_t)(((uint64_t)recv_msg->arg << 32) | recv_msg->ret);
		break;
	case NPU_SCP_QUERY_DRAM_CTRL_DEVADDR:
		ctx->received_act = NPU_SCP_QUERY_DRAM_CTRL_DEVADDR;
		ctx->received_pa = (uint64_t)(((uint64_t)recv_msg->arg << 32) | recv_msg->ret);
		break;
	default:
		pr_info("%s Not supported act %d\n", __func__, recv_msg->act);
		ctx->received_act = 0;
		ctx->received_pa = 0;
		ret = -EINVAL;
		break;
	}

	spin_unlock_irqrestore(&ctx->lock, flags);

	pr_info("%s trigger mem service worker thread +++\n", __func__);
	complete(&ctx->worker_comp);

	return ret;
}

int aov_mem_service_v2_init(struct platform_device *pdev)
{
	int ret = 0;

	pr_info("%s +++\n", __func__);

	if (!pdev) {
		pr_info("%s error platform device\n", __func__);
		return -EINVAL;
	}

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->parent_pdev = pdev;
	ctx->service_worker = kthread_create(service_worker_func, (void *)ctx,
					     "aov-mem-service-thread");
	if (IS_ERR(ctx->service_worker)) {
		ret = PTR_ERR(ctx->service_worker);
		goto scp_kthread_error;
	}

	spin_lock_init(&ctx->lock);

	init_completion(&ctx->worker_comp);

	//set_user_nice(ctx->service_worker, PRIO_TO_NICE(MAX_RT_PRIO) + 1);
	wake_up_process(ctx->service_worker);

	npu_scp_ipi_register_handler(NPU_SCP_MEM_SERVICE, mem_service_handler, NULL);

	return 0;

scp_kthread_error:
	kfree(ctx);
	ctx = NULL;

	return ret;
}

void aov_mem_service_v2_exit(struct platform_device *pdev)
{
	npu_scp_ipi_unregister_handler(NPU_SCP_MEM_SERVICE);

	if (!ctx)
		return;

	complete_all(&ctx->worker_comp);

	if (ctx->service_worker)
		kthread_stop(ctx->service_worker);

	if (ctx->domain && ctx->data_iova && ctx->data_size)
		iommu_unmap(ctx->domain, ctx->data_iova, ctx->data_size);

	kfree(ctx);

	ctx = NULL;
}
