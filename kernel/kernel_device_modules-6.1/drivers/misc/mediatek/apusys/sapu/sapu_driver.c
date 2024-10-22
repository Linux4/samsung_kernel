// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include "sapu_plat.h"
#include "mtk-smmu-v3.h"

int sapu_ha_bridge(struct sapu_datamem_info *datamem_info, struct sapu_ha_tranfer *ha_transfer)
{
	union MTEEC_PARAM p[4];
	TZ_RESULT ret;

	KREE_SESSION_HANDLE ha_sn = 0;

	pr_info("[SAPU_LOG] KREE_CreateSession\n");
	ret = KREE_CreateSession(datamem_info->haSrvName, &ha_sn);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("[%s]CreateSession ha_sn fail(%d)\n", __func__, ret);
		return -EPIPE;
	}

	p[0].mem.size = sizeof(*ha_transfer);
	p[0].mem.buffer = (void *)ha_transfer;

	pr_info("[SAPU_LOG] KREE_TeeServiceCall\n");
	ret = KREE_TeeServiceCall(ha_sn, datamem_info->command,
						TZ_ParamTypes2(TZPT_MEM_INPUT,
						TZPT_VALUE_OUTPUT), p);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("[%s]TeeServiceCall fail(%d)\n", __func__, ret);
		return -EPIPE;
	}

	// return code
	pr_info("[SAPU_LOG] p[1].value.a = %x", p[1].value.a);

	pr_info("[SAPU_LOG] KREE_CloseSession\n");
	ret = KREE_CloseSession(ha_sn);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("[%s]ha_sn(0x%x) CloseSession fail(%d)\n",
					__func__, ha_sn, ret);
		return -EPIPE;
	}

	return 0;
}

long apusys_sapu_internal_ioctl(struct file *filep, unsigned int cmd, void __user *arg,
	unsigned int compat)
{
	int ret;

	struct dma_buf *dmem_dmabuf = NULL;
	struct dma_buf_attachment *dmem_attach;
	struct device *dmem_device;
	struct device_node *sapu_node;
	struct device_node *smmu_node = NULL;
	struct sg_table *dmem_sgt;
	struct sapu_private *sapu;
	struct platform_device *pdev;
	struct platform_device *smmu_dev = NULL;

	struct sapu_datamem_info datamem_info;
	struct sapu_ha_tranfer ha_transfer;
	uint32_t power_on;

	(void)compat;

	sapu = get_sapu_private();
	if (sapu == NULL) {
		pr_info("%s %d\n", __func__, __LINE__);
		return 0;
	}

	if (_IOC_TYPE(cmd) != APUSYS_SAPU_IOC_MAGIC) {
		pr_info("%s %d -EINVAL\n", __func__, __LINE__);
		return -EINVAL;
	}

	switch (cmd) {
	case APUSYS_SAPU_DATAMEM:
		pr_info("%s APUSYS_SAPU_DATAMEM 0x%x\n", __func__, cmd);
		ret = copy_from_user(&datamem_info, arg, sizeof(datamem_info));
		if (ret) {
			pr_info("[%s]copy_from_user fail(0x%x)\n",
					__func__, ret);
			return ret;
		}
		// pr_info("%s fd=%d\n", __func__, datamem_info.fd);

		/* get handle */
		dmem_dmabuf = dma_buf_get(datamem_info.fd);
		if (!dmem_dmabuf || IS_ERR(dmem_dmabuf)) {
			pr_info("dma_buf_get error %d\n", __LINE__);
			return -EINVAL;
		}

		ha_transfer.handle = dmabuf_to_secure_handle(dmem_dmabuf);
		if (!ha_transfer.handle) {
			pr_info("dmabuf_to_secure_handle failed!\n");
			ret = -EINVAL;
			goto datamem_dmabuf_put;
		}

		/* get iova */
		pdev = sapu->pdev;
		if (pdev == NULL) {
			pr_info("%s %d\n", __func__, __LINE__);
			ret = -EINVAL;
			goto datamem_dmabuf_put;
		}

		dmem_device = &pdev->dev;
		if (dmem_device == NULL) {
			pr_info("%s %d\n", __func__, __LINE__);
			ret = -EINVAL;
			goto datamem_dmabuf_put;
		}

		if (smmu_v3_enabled()) {
			sapu_node = dmem_device->of_node;
			if (!sapu_node) {
				pr_info("%s sapu_node is NULL\n", __func__);
				return -ENODEV;
			}

			smmu_node = of_parse_phandle(sapu_node, "smmu-device", 0);
			if (!smmu_node) {
				pr_info("%s get smmu_node failed\n", __func__);
				return -ENODEV;
			}

			smmu_dev = of_find_device_by_node(smmu_node);
			if (!smmu_node) {
				pr_info("%s get smmu_dev failed\n", __func__);
				return -ENODEV;
			}

			dmem_attach = dma_buf_attach(dmem_dmabuf, &smmu_dev->dev);
		} else {
			dmem_attach = dma_buf_attach(dmem_dmabuf, dmem_device);
		}
		if (IS_ERR(dmem_attach)) {
			pr_info("%s dmem_attach fail\n", __func__);
			ret = -EINVAL;
			goto datamem_dmabuf_put;
		}

		dmem_sgt = dma_buf_map_attachment(dmem_attach,
					DMA_BIDIRECTIONAL);
		if (IS_ERR(dmem_sgt)) {
			pr_info("%s map failed, detach and return\n", __func__);
			ret = -EINVAL;
			goto datamem_dmabuf_detach;
		}

		ha_transfer.dma_addr = sg_dma_address(dmem_sgt->sgl);
		// pr_info("dma_addr=%xad\n", ha_transfer.dma_addr);

		ha_transfer.model_hd_ha = datamem_info.model_hd_ha;
		/* Call to HA with params */
		ret = sapu_ha_bridge(&datamem_info, &ha_transfer);
		if (ret)
			pr_info("%s call to HA failed (%d)\n", __func__, ret);

		dma_buf_unmap_attachment(dmem_attach,
					dmem_sgt, DMA_BIDIRECTIONAL);
datamem_dmabuf_detach:
		dma_buf_detach(dmem_dmabuf, dmem_attach);
datamem_dmabuf_put:
		dma_buf_put(dmem_dmabuf);
	break;

	case APUSYS_POWER_CONTROL:
		pr_info("%s APUSYS_POWER_CONTROL 0x%x\n", __func__, cmd);
		ret = copy_from_user(&power_on, arg, sizeof(u32));
		if (ret) {
			pr_info("[%s]copy_from_user fail(0x%x)\n",
						__func__, ret);
			return ret;
		}

		sapu->platdata->ops.power_ctrl(sapu, power_on);

	break;

	default:
		pr_info("%s unknown 0x%x\n", __func__, cmd);
		ret = -EINVAL;
	break;
	}

	return ret;
}

MODULE_IMPORT_NS(DMA_BUF);
