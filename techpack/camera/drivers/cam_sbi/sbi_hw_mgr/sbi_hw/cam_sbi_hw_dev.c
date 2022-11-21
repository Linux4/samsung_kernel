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

#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include "media/cam_req_mgr.h"

#include "cam_subdev.h"
#include "cam_sbi_hw_intf.h"
#include "cam_sbi_hw_core.h"
#include "cam_sbi_hw_soc.h"
#include "cam_sbi_hw_reg.h"
#include "cam_req_mgr_workq.h"
#include "cam_sbi_hw_mgr.h"
#include "cam_mem_mgr_api.h"
#include "cam_smmu_api.h"
#include "camera_main.h"

struct cam_hw_info *g_sbi_hw;


#define CAM_CUSTOM_SUB_MOD_MAX_INSTANCES 2

static struct cam_hw_intf *
	cam_sbi_hw_sub_mod_list[CAM_CUSTOM_SUB_MOD_MAX_INSTANCES] = { 0, 0 };

int cam_sbi_hw_sub_mod_init(struct cam_hw_intf **custom_hw, uint32_t hw_idx)
{
	int rc = 0;

	if (cam_sbi_hw_sub_mod_list[hw_idx]) {
		*custom_hw = cam_sbi_hw_sub_mod_list[hw_idx];
		rc = 0;
	} else {
		*custom_hw = NULL;
		rc = -ENODEV;
	}
	return 0;
}


//static int cam_sbi_hw_dev_probe(struct platform_device *pdev)
static int cam_sbi_hw_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct cam_hw_info *sbi_hw;
	struct cam_hw_intf sbi_hw_intf;
	struct cam_sbi_core *sbi_core;
	const struct of_device_id *match_dev = NULL;
	struct cam_sbi_hw_info *hw_info;
	int rc, i;
	struct platform_device *pdev = to_platform_device(dev);

	sbi_hw = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!sbi_hw) {
		CAM_ERR(CAM_SBI, "No memory to create sbi_hw");
		return -ENOMEM;
	}

	sbi_core = kzalloc(sizeof(struct cam_sbi_core), GFP_KERNEL);
	if (!sbi_core) {
		CAM_ERR(CAM_SBI, "No memory to create sbi_core");
		kfree(sbi_hw);
		return -ENOMEM;
	}

	sbi_hw->core_info = sbi_core;
	sbi_hw->hw_state = CAM_HW_STATE_POWER_DOWN;
	sbi_hw->soc_info.pdev = pdev;
	sbi_hw->soc_info.dev = &pdev->dev;
	sbi_hw->soc_info.dev_name = pdev->name;
	sbi_hw->open_count = 0;
	sbi_core->state = CAM_SBI_CORE_STATE_INIT;

	mutex_init(&sbi_hw->hw_mutex);
	spin_lock_init(&sbi_hw->hw_lock);
	init_completion(&sbi_hw->hw_complete);
	init_completion(&sbi_core->sbi0_reset_complete);
	init_completion(&sbi_core->sbi1_reset_complete);
	init_completion(&sbi_core->sbi2_reset_complete);

	rc = cam_req_mgr_workq_create("cam_sbi_hw_worker",
		CAM_SBI_HW_WORKQ_NUM_TASK,
		&sbi_core->work, CRM_WORKQ_USAGE_IRQ, 0,
		cam_req_mgr_process_workq_cam_sbi_worker);
	if (rc) {
		CAM_ERR(CAM_SBI, "Unable to create a workq, rc=%d", rc);
		goto free_memory;
	}

	for (i = 0; i < CAM_SBI_HW_WORKQ_NUM_TASK; i++)
		sbi_core->work->task.pool[i].payload = &sbi_core->work_data[i];

	match_dev = of_match_device(pdev->dev.driver->of_match_table,
		&pdev->dev);
	if (!match_dev || !match_dev->data) {
		CAM_ERR(CAM_SBI, "No Of_match data, %pK", match_dev);
		rc = -EINVAL;
		goto destroy_workqueue;
	}
	hw_info = (struct cam_sbi_hw_info *)match_dev->data;
	sbi_core->hw_info = hw_info;

	rc = cam_sbi_soc_init_resources(&sbi_hw->soc_info,
		cam_sbi_hw_irq, sbi_hw);
	if (rc) {
		CAM_ERR(CAM_SBI, "Failed to init soc, rc=%d", rc);
		goto destroy_workqueue;
	}

	rc = cam_sbi_hw_start(sbi_hw, NULL, 0);
	if (rc) {
		CAM_ERR(CAM_SBI, "Failed to hw init, rc=%d", rc);
		goto detach_smmu;
	}

	rc = cam_sbi_hw_util_get_caps(sbi_hw, &sbi_core->hw_caps);
	if (rc) {
		CAM_ERR(CAM_SBI, "Failed to get hw caps, rc=%d", rc);
		if (cam_sbi_hw_stop(sbi_hw, NULL, 0))
			CAM_ERR(CAM_SBI, "Failed in hw deinit");
		goto detach_smmu;
	}

	rc = cam_sbi_hw_stop(sbi_hw, NULL, 0);
	if (rc) {
		CAM_ERR(CAM_SBI, "Failed to deinit hw, rc=%d", rc);
		goto detach_smmu;
	}

	sbi_core->hw_idx = sbi_hw->soc_info.index;
	sbi_hw_intf.hw_priv = sbi_hw;
	sbi_hw_intf.hw_idx = sbi_hw->soc_info.index;
	sbi_hw_intf.hw_ops.get_hw_caps = cam_sbi_hw_get_caps;
	sbi_hw_intf.hw_ops.init = NULL;
	sbi_hw_intf.hw_ops.deinit = NULL;
	sbi_hw_intf.hw_ops.reset = cam_sbi_hw_reset;
	sbi_hw_intf.hw_ops.reserve = NULL;
	sbi_hw_intf.hw_ops.release = NULL;
	sbi_hw_intf.hw_ops.start = cam_sbi_hw_start;
	sbi_hw_intf.hw_ops.stop = cam_sbi_hw_stop;
	sbi_hw_intf.hw_ops.read = NULL;
	sbi_hw_intf.hw_ops.write = NULL;
	sbi_hw_intf.hw_ops.process_cmd = cam_sbi_hw_process_cmd;
	sbi_hw_intf.hw_type = CAM_HW_SBI;

	rc = cam_sbi_mgr_register_device(&sbi_hw_intf,
		&sbi_core->device_iommu
		// ,&sbi_core->cdm_iommu
		);

	if (rc) {
		CAM_ERR(CAM_SBI, "Failed to register device");
		goto detach_smmu;
	}

	platform_set_drvdata(pdev, sbi_hw);
	CAM_INFO(CAM_SBI, "SBI-%d probe successful", sbi_hw_intf.hw_idx);

	g_sbi_hw = sbi_hw;// debugging purpose

	//cam_sbi_hw_dump_all_reg(sbi_hw, 1);

	return rc;


 detach_smmu:
	cam_smmu_ops(sbi_core->device_iommu.non_secure, CAM_SMMU_DETACH);

//  deinit_platform_res:
	if (cam_sbi_soc_deinit_resources(&sbi_hw->soc_info))
		CAM_ERR(CAM_SBI, "Failed in soc deinit");
	mutex_destroy(&sbi_hw->hw_mutex);
destroy_workqueue:
	cam_req_mgr_workq_destroy(&sbi_core->work);

 free_memory:
	mutex_destroy(&sbi_hw->hw_mutex);
	kfree(sbi_hw);
	kfree(sbi_core);

	return rc;
}

//static int cam_sbi_hw_dev_remove(struct platform_device *pdev)
static void cam_sbi_hw_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	int rc = 0;
	struct cam_hw_info *sbi_hw;
	struct cam_sbi_core *sbi_core;
	struct platform_device *pdev = to_platform_device(dev);

	sbi_hw = platform_get_drvdata(pdev);
	if (!sbi_hw) {
		CAM_ERR(CAM_SBI, "Invalid sbi_hw from pdev");
		//rc = -ENODEV;
		goto err;
	}

	sbi_core = (struct cam_sbi_core *)sbi_hw->core_info;
	if (!sbi_core) {
		CAM_ERR(CAM_SBI, "Invalid sbi_core from sbi_hw");
		//rc = -EINVAL;
		goto deinit_platform_res;
	}

	cam_smmu_ops(sbi_core->device_iommu.non_secure, CAM_SMMU_DETACH);
	cam_smmu_destroy_handle(sbi_core->device_iommu.non_secure);
	cam_sbi_mgr_deregister_device(sbi_core->hw_idx);

	kfree(sbi_core);

deinit_platform_res:
	rc = cam_sbi_soc_deinit_resources(&sbi_hw->soc_info);
	if (rc)
		CAM_ERR(CAM_SBI, "Error in SBI soc deinit, rc=%d", rc);

	mutex_destroy(&sbi_hw->hw_mutex);
	kfree(sbi_hw);
err:
	//return rc;
	return;
}


const static struct component_ops cam_sbi_hw_component_ops = {
	.bind = cam_sbi_hw_component_bind,
	.unbind = cam_sbi_hw_component_unbind,
};

static int cam_sbi_hw_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_SBI, "Adding SBI hw component");
	rc = component_add(&pdev->dev, &cam_sbi_hw_component_ops);
	if (rc)
		CAM_ERR(CAM_SBI, "failed to add component rc: %d", rc);

	return rc;
}

static int cam_sbi_hw_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_sbi_hw_component_ops);
	return 0;
}

static const struct of_device_id cam_sbi_hw_dt_match[] = {
	{
		.compatible = "qcom,cam-sbi",
		.data = &cam_sbi10_hw_info,
	},
	{}
};

MODULE_DEVICE_TABLE(of, cam_sbi_hw_dt_match);

struct platform_driver cam_sbi_hw_driver = {
   .probe = cam_sbi_hw_probe,
   .remove = cam_sbi_hw_remove,
   .driver = {
	   .name = "cam_sbi_hw",
	   .owner = THIS_MODULE,
	   .of_match_table = cam_sbi_hw_dt_match,
	   .suppress_bind_attrs = true,
   },
};

int  cam_sbi_hw_init_module(void)
{
	return platform_driver_register(&cam_sbi_hw_driver);
}

void  cam_sbi_hw_exit_module(void)
{
	platform_driver_unregister(&cam_sbi_hw_driver);
}

// module_init(cam_sbi_hw_init_module);
// module_exit(cam_sbi_hw_exit_module);
MODULE_DESCRIPTION("CAM SBI HW driver");
MODULE_LICENSE("GPL v2");
