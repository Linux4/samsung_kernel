/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/iommu.h>

#include "cam_subdev.h"
#include "cam_node.h"
#include "cam_sbi_context.h"
#include "cam_sbi_hw_mgr.h"
#include "cam_sbi_hw_mgr_intf.h"
#include "cam_smmu_api.h"
#include "camera_main.h"

#define CAM_SBI_DEV_NAME "cam-sbi"

/**
 * struct cam_sbi_dev
 *
 * @sd			: Subdev information
 * @ctx			: List of base contexts
 * @sbi_ctx		: List of SBI contexts
 * @lock		: Mutex for SBI subdev
 * @open_cnt		: Open count of SBI subdev
 */
struct cam_sbi_dev {
	struct cam_subdev		sd;
	struct cam_context		ctx[CAM_CTX_MAX];
	struct cam_sbi_dev_context	sbi_ctx[CAM_CTX_MAX];
	struct mutex			lock;
	uint32_t			open_cnt;
};

static struct cam_sbi_dev g_sbi_dev;

static int cam_sbi_dev_buf_done_cb(void *ctxt_to_hw_map, uint32_t evt_id,
	void *evt_data)
{
	uint64_t index;
	struct cam_context *ctx;
	int rc;

	index = CAM_SBI_DECODE_CTX_INDEX(ctxt_to_hw_map);
	CAM_DBG(CAM_SBI, "ctx index %llu, evt_id %u\n", index, evt_id);
	ctx = &g_sbi_dev.ctx[index];
	rc = ctx->irq_cb_intf(ctx, evt_id, evt_data);
	if (rc)
		CAM_ERR(CAM_SBI, "irq callback failed");

	return rc;
}

static int cam_sbi_dev_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_sbi_dev *sbi_dev = &g_sbi_dev;

	if (!sbi_dev) {
		CAM_ERR(CAM_SBI,
			"SBI Dev not initialized, dev=%pK", sbi_dev);
		return -ENODEV;
	}

	mutex_lock(&sbi_dev->lock);
	sbi_dev->open_cnt++;
	mutex_unlock(&sbi_dev->lock);

	return 0;
}

static int cam_sbi_dev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	int rc = 0;
	struct cam_sbi_dev *sbi_dev = &g_sbi_dev;
	struct cam_node *node = v4l2_get_subdevdata(sd);

	if (!sbi_dev) {
		CAM_ERR(CAM_SBI, "Invalid args");
		return -ENODEV;
	}

	mutex_lock(&sbi_dev->lock);
	if (sbi_dev->open_cnt <= 0) {
		CAM_DBG(CAM_SBI, "SBI subdev is already closed");
		rc = -EINVAL;
		goto end;
	}

	sbi_dev->open_cnt--;
	if (!node) {
		CAM_ERR(CAM_SBI, "Node is NULL");
		rc = -EINVAL;
		goto end;
	}

	if (sbi_dev->open_cnt == 0)
		cam_node_shutdown(node);

end:
	mutex_unlock(&sbi_dev->lock);
	return rc;
}

static const struct v4l2_subdev_internal_ops cam_sbi_subdev_internal_ops = {
	.open = cam_sbi_dev_open,
	.close = cam_sbi_dev_close,
};

static void cam_sbi_dev_iommu_fault_handler(struct cam_smmu_pf_info *pf_info)
{
	//int i = 0;
	struct cam_node *node = NULL;

	if (!pf_info || !pf_info->token) {
                CAM_ERR(CAM_ISP, "invalid token in page handler cb");
                return;
        }

	node = (struct cam_node *)pf_info->token;

	//for (i = 0; i < node->ctx_size; i++)
	//	cam_context_dump_pf_info(&(node->ctx_list[i]), iova, buf_info);
}

//static int cam_sbi_dev_probe(struct platform_device *pdev)
//static int cam_sbi_component_bind(struct platform_device *dev)
static int cam_sbi_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	int rc;
	int i;
	struct cam_hw_mgr_intf hw_mgr_intf;
	struct cam_node *node;
	int iommu_hdl = -1;
	struct platform_device *pdev = to_platform_device(dev);

	CAM_DBG(CAM_SBI, "enter");
	g_sbi_dev.sd.internal_ops = &cam_sbi_subdev_internal_ops;

	mutex_init(&g_sbi_dev.lock);
	rc = cam_subdev_probe(&g_sbi_dev.sd, pdev, CAM_SBI_DEV_NAME,
		CAM_SBI_DEVICE_TYPE);
	if (rc) {
		CAM_ERR(CAM_SBI, "SBI cam_subdev_probe failed");
		goto err;
	}
	node = (struct cam_node*)g_sbi_dev.sd.token;

	rc = cam_sbi_hw_mgr_init(&hw_mgr_intf, cam_sbi_dev_buf_done_cb, &iommu_hdl);
	if (rc) {
		CAM_ERR(CAM_SBI, "Can not initialized SBI HW manager");
		goto unregister;
	}

	for (i = 0; i < CAM_CTX_MAX; i++) {
		rc = cam_sbi_context_init(&g_sbi_dev.sbi_ctx[i],
				&g_sbi_dev.ctx[i],
				&node->crm_node_intf,
				&node->hw_mgr_intf, i);
		if (rc) {
			CAM_ERR(CAM_SBI, "SBI context init failed");
			goto deinit_ctx;
		}
	}

	rc = cam_node_init(node, &hw_mgr_intf, g_sbi_dev.ctx, CAM_CTX_MAX, CAM_SBI_DEV_NAME);
	if (rc) {
		CAM_ERR(CAM_SBI, "SBI node init failed");
		goto deinit_ctx;
	}

	cam_smmu_set_client_page_fault_handler(
		iommu_hdl, cam_sbi_dev_iommu_fault_handler, node);

	//CAM_INFO(CAM_SBI, "%s probe complete", g_sbi_dev->sd.name);
	CAM_DBG(CAM_SBI, "%s component bound successfully", pdev->name);

	return 0;

deinit_ctx:
	for (--i; i >= 0; i--) {
		if (cam_sbi_context_deinit(&g_sbi_dev.sbi_ctx[i]))
			CAM_ERR(CAM_SBI, "SBI context %d deinit failed", i);
	}
unregister:
	if (cam_subdev_remove(&g_sbi_dev.sd))
		CAM_ERR(CAM_SBI, "Failed in subdev remove");
err:
	return rc;
}

//static int cam_sbi_dev_remove(struct platform_device *pdev)
//{
//	int i;
//	int rc = 0;
//
//	for (i = 0; i < CAM_CTX_MAX; i++) {
//		rc = cam_sbi_context_deinit(&g_sbi_dev.sbi_ctx[i]);
//		if (rc)
//			CAM_ERR(CAM_SBI, "SBI context %d deinit failed", i);
//	}
//
//	rc = cam_sbi_hw_mgr_deinit();
//	if (rc)
//		CAM_ERR(CAM_SBI, "Failed in hw mgr deinit, rc=%d", rc);
//
//	rc = cam_subdev_remove(&g_sbi_dev.sd);
//	if (rc)
//		CAM_ERR(CAM_SBI, "Unregister failed");
//
//	mutex_destroy(&g_sbi_dev.lock);
//
//	return rc;
//}



static void cam_sbi_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	int rc = 0;
	int i;

	/* clean up resources */
	for (i = 0; i < CAM_CTX_MAX; i++) {
		rc = cam_sbi_context_deinit(&g_sbi_dev.sbi_ctx[i]);
		if (rc)
			CAM_ERR(CAM_SBI,
				"Custom context %d deinit failed", i);
	}

	rc = cam_sbi_hw_mgr_deinit();
	if (rc)
		CAM_ERR(CAM_SBI, "Failed in hw mgr deinit, rc=%d", rc);

	rc = cam_subdev_remove(&g_sbi_dev.sd);
	if (rc)
		CAM_ERR(CAM_SBI, "Unregister failed");

	mutex_destroy(&g_sbi_dev.lock);// andy custom

	memset(&g_sbi_dev, 0, sizeof(g_sbi_dev));
}

const static struct component_ops cam_sbi_component_ops = {
	.bind = cam_sbi_component_bind,
	.unbind = cam_sbi_component_unbind,
};

static int cam_sbi_dev_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_sbi_component_ops);
	return 0;
}

static int cam_sbi_dev_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_SBI, "Adding SBI HW component");
	rc = component_add(&pdev->dev, &cam_sbi_component_ops);
	if (rc)
		CAM_ERR(CAM_SBI, "failed to add component rc: %d", rc);

	return rc;
}






ssize_t cam_sbi_test(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	switch (buf[0]) {
	case '0':
		CAM_INFO(CAM_SBI, "cam_sbi_close");
		break;
	case '1':
		CAM_INFO(CAM_SBI, "cam_sbi_open");
		break;

	default:
		CAM_INFO(CAM_SBI, "%s, default no action",__FUNCTION__);
		break;
	}
	return size;
}

static const struct of_device_id cam_sbi_dt_match[] = {
	{
		.compatible = "qcom,sbi"
	},
	{}
};

struct platform_driver cam_sbi_driver = {
	.probe = cam_sbi_dev_probe,
	.remove = cam_sbi_dev_remove,
	.driver = {
		.name = "cam_sbi",
		.owner = THIS_MODULE,
		.of_match_table = cam_sbi_dt_match,
		.suppress_bind_attrs = true,
	},
};

int  cam_sbi_dev_init_module(void)
{
	int32_t rc = 0;

	rc = platform_driver_register(&cam_sbi_driver);
	if (rc < 0)
		CAM_ERR(CAM_SENSOR, "platform_driver_register Failed: rc = %d",
			rc);

	return rc;
}

void  cam_sbi_dev_exit_module(void)
{
	platform_driver_unregister(&cam_sbi_driver);
}

// module_init(cam_sbi_dev_init_module);
// module_exit(cam_sbi_dev_exit_module);
MODULE_DESCRIPTION("MSM SBI driver");
MODULE_LICENSE("GPL v2");
