/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/kfifo.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/iommu.h>
#include <linux/pm_runtime.h>

#include "npu-config.h"
#include "vs4l.h"
#include "vision-dev.h"
#include "vision-ioctl.h"
#include "npu-clock.h"
#include "npu-device.h"
#include "npu-debug.h"
#include "npu-protodrv.h"
#include "npu-util-memdump.h"
#include "npu-util-regs.h"
#include "npu-ver.h"
#include "npu-hw-device.h"
#include "dsp-dhcp.h"
#include "npu-afm.h"

#if IS_ENABLED(CONFIG_EXYNOS_NPU_PUBLISH_NPU_BUILD_VER)
#include "npu-ver-info.h"
#endif

extern struct class vision_class;
extern int npu_system_save_result(struct npu_session *session, struct nw_result nw_result);

#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
#include <soc/samsung/exynos/exynos-s2mpu.h>

static struct s2mpu_notifier_block *s2mpu_nb;

static int npu_s2mpu_fault_handler(struct s2mpu_notifier_block *nb,
						struct s2mpu_notifier_info *ni)
{
	fw_will_note_to_kernel(0x10000);
	return S2MPU_NOTIFY_BAD;
}

#if IS_ENABLED(CONFIG_NPU_USE_S2MPU_NAME_IS_NPUS)
#define NPU_S2MPU_NAME	"NPUS"
#else
#define NPU_S2MPU_NAME	"DNC"
#endif

static void npu_s2mpu_set_fault_handler(struct device *dev)
{
	s2mpu_nb = NULL;

	s2mpu_nb = devm_kzalloc(dev, sizeof(struct s2mpu_notifier_block),
				GFP_KERNEL);
	if (!s2mpu_nb) {
		npu_err("s2mpu notifier block alloc failed\n");
		return;
	}

	s2mpu_nb->subsystem = NPU_S2MPU_NAME;
	s2mpu_nb->notifier_call = npu_s2mpu_fault_handler;

	exynos_s2mpu_notifier_call_register(s2mpu_nb);
}

static void npu_s2mpu_free_fault_handler(struct device *dev)
{
	if (s2mpu_nb)
		devm_kfree(dev, s2mpu_nb);
}
#else // !IS_ENABLED(CONFIG_EXYNOS_S2MPU)
static void npu_s2mpu_set_fault_handler(
			__attribute__((unused))struct device *dev)
{
}

static void npu_s2mpu_free_fault_handler(
			__attribute__((unused))struct device *dev)
{
}
#endif

static int npu_iommu_fault_handler(struct iommu_fault *fault, void *token)
{
	struct npu_device *device = token;

	if (test_bit(NPU_DEVICE_STATE_OPEN, &device->state)) {
		npu_err("called with mode(%pK), state(%pK))\n",
				&device->mode, &device->state);
		npu_util_dump_handle_error_k(device);
	}

	return 0;
}

static int npu_iommu_set_fault_handler(struct device *dev,
					struct npu_device *device)
{	int ret = 0;
	ret = iommu_register_device_fault_handler(dev,
				npu_iommu_fault_handler, device);

	return ret;
}

static void npu_dev_set_socdata(__attribute__((unused))struct device *dev)
{
}

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
static void npu_itmon_noti_reg(__attribute__((unused))struct npu_device *device)
{
	itmon_notifier_chain_register(&device->itmon_nb);
	return;
}
#endif

static void __npu_scheduler_early_close(struct npu_device *device)
{
	npu_scheduler_early_close(device);
}

static void __npu_scheduler_late_open(struct npu_device *device)
{
	npu_scheduler_late_open(device);
}

static int __npu_device_start(struct npu_device *device)
{
	int ret = 0;

	if (test_bit(NPU_DEVICE_STATE_START, &device->state)) {
		npu_err("already started\n");
		ret = -EINVAL;
		goto ErrorExit;
	}

	ret = npu_system_start(&device->system);
	if (ret) {
		npu_err("fail(%d) in npu_system_start\n", ret);
		goto ErrorExit;
	}

	ret = npu_debug_start(device);
	if (ret) {
		npu_err("fail(%d) in npu_debug_start\n", ret);
		goto ErrorExit;
	}

	ret = npu_log_start(device);
	if (ret) {
		npu_err("fail(%d) in npu_log_start\n", ret);
		goto ErrorExit;
	}

	ret = proto_drv_start(device);
	if (ret) {
		npu_err("fail(%d) in proto_drv_start\n", ret);
		goto ErrorExit;
	}

	set_bit(NPU_DEVICE_STATE_START, &device->state);
ErrorExit:
	return ret;
}

static int __npu_device_stop(struct npu_device *device)
{
	int ret = 0;

	if (!test_bit(NPU_DEVICE_STATE_START, &device->state))
		goto ErrorExit;

	ret = proto_drv_stop(device);
	if (ret)
		npu_err("fail(%d) in proto_drv_stop\n", ret);

	ret = npu_log_stop(device);
	if (ret)
		npu_err("fail(%d) in npu_log_stop\n", ret);

	ret = npu_debug_stop(device);
	if (ret)
		npu_err("fail(%d) in npu_debug_stop\n", ret);

	ret = npu_system_stop(&device->system);
	if (ret)
		npu_err("fail(%d) in npu_system_stop\n", ret);

	clear_bit(NPU_DEVICE_STATE_START, &device->state);

ErrorExit:
	return ret;
}

/*
 * Called after all the open procedure is completed.
 * NPU H/W is fully working at this stage
 */
static int __npu_device_late_open(struct npu_device *device)
{
	int ret = 0;

	__npu_scheduler_late_open(device);

	return ret;
}

/*
 * Called before the close procedure is started.
 * NPU H/W is fully working at this stage
 */
static int __npu_device_early_close(struct npu_device *device)
{
	int ret = 0;

	npu_trace("Starting.\n");

	__npu_scheduler_early_close(device);

	return ret;
}

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
static int npu_itmon_notifier(struct notifier_block *nb, unsigned long action, void *nb_data)
{
	struct npu_device *device;
	struct itmon_notifier *itmon_info = nb_data;
	int i = 0;

	device = container_of(nb, struct npu_device, itmon_nb);

	if (IS_ERR_OR_NULL(itmon_info))
		return NOTIFY_DONE;

	/*
	 * 1. When BUS Error occurs, IPs Traffic Monitor (ITMON) Error occurs.
	 * 2. Delivery of Master/Slave/Target information, etc. from ITMON Notifiercall to each driver.
	 * 3. The driver determines and prints the register dump, etc.
	 * 4. After that, ITMON counted three times and judged it as Panic or Not Panic to proceed with the Post Action.
	 */
	// Master or Slave
	if ((!strncmp("DNC", itmon_info->port, sizeof("DNC") - 1)) ||
	    (!strncmp("DNC", itmon_info->master, sizeof("DNC") - 1)) ||
	    (!strncmp("DNC", itmon_info->dest, sizeof("DNC") - 1))) {
		npu_err("NPU/DSP ITMON(Master) : power domain(%s), power on/off(%s)\n",
			itmon_info->pd_name, itmon_info->onoff ? "on" : "off");

		fw_will_note(FW_LOGSIZE);

		for (i = 0; i < 5; i++)
			npu_soc_status_report(&device->system);

#if IS_ENABLED(CONFIG_NPU_USE_HW_DEVICE)
		if (itmon_info->onoff)
			npu_reg_dump(device);
#endif
		return NOTIFY_BAD; // Error
	}

	return NOTIFY_OK;
}
#endif

extern int npu_pwr_on(struct npu_system *system);
extern int npu_clk_init(struct npu_system *system);

static int npu_device_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;
	struct npu_device *device;

	BUG_ON(!pdev);

	dev = &pdev->dev;
#ifdef NPU_EXYNOS_PRINTK
	/* activate exynos printk */
	npu_dev_set_socdata(dev);
#endif

	device = devm_kzalloc(dev, sizeof(*device), GFP_KERNEL);
	if (!device) {
		probe_err("fail in devm_kzalloc\n");
		ret = -ENOMEM;
		goto err_exit;
	}
	device->dev = dev;

	mutex_init(&device->start_stop_lock);
	probe_info("NPU Device - init start_stop lock\n");

	ret = npu_create_config(dev);
	if (ret) {
		probe_err("failed(%d) npu_create_config\n", ret);
		goto err_exit;
	}

	ret = npu_system_probe(&device->system, pdev);
	if (ret) {
		probe_err("fail(%d) in npu_system_probe\n", ret);
		ret = -EINVAL;
		goto err_exit;
	}

	/* Probe sub modules */
	ret = npu_debug_probe(device);
	if (ret) {
		probe_err("fail(%d) in npu_debug_probe\n", ret);
		goto err_exit;
	}

	ret = npu_log_probe(device);
	if (ret) {
		probe_err("fail(%d) in npu_log_probe\n", ret);
		goto err_exit;
	}

	ret = npu_vertex_probe(&device->vertex, dev);
	if (ret) {
		probe_err("fail(%d) in npu_vertex_probe\n", ret);
		goto err_exit;
	}
#if IS_ENABLED(CONFIG_NPU_USE_FENCE_SYNC)
	ret = npu_fence_probe(&device->fence, &device->system);
	if (ret) {
		probe_err("fail(%d) in npu_fence_probe\n", ret);
		goto err_exit;
	}
#endif
	ret = proto_drv_probe(device);
	if (ret) {
		probe_err("fail(%d) in proto_drv_probe\n", ret);
		goto err_exit;
	}

	ret = npu_sessionmgr_probe(&device->sessionmgr);

#if IS_ENABLED(CONFIG_NPU_GOLDEN_MATCH)
	ret = register_golden_matcher(dev);
	if (ret) {
		probe_err("fail(%d) in register_golden_matcher\n", ret);
		goto err_exit;
	}
#endif

	ret = npu_iommu_set_fault_handler(dev, device);
	if (ret) {
		probe_err("fail(%d) in iommu_register_device_fault_handler\n", ret);
		goto err_exit;
	}

	npu_s2mpu_set_fault_handler(dev);

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
	device->itmon_nb.notifier_call = npu_itmon_notifier;
	npu_itmon_noti_reg(device);
#endif

#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
	ret = dsp_kernel_manager_probe(device);
	if (ret) {
		probe_err("fail(%d) in dsp_kernel_manager_probe\n", ret);
		goto err_exit;
	}
#endif

	ret = dsp_dhcp_probe(device);
	if (ret) {
		probe_err("fail(%d) in dsp_dhcp_probe\n", ret);
		goto err_exit;
	}

	ret = npu_afm_probe(device);
	if (ret) {
		probe_err("npu_afm_probe is fail(%d)\n", ret);
		goto err_exit;
	}

	dev_set_drvdata(dev, device);

	ret = 0;
	probe_info("[EVT1] complete in %s\n", __func__);

	goto ok_exit;

err_exit:
	npu_s2mpu_free_fault_handler(dev);
	probe_err("error: ret(%d)\n", ret);
ok_exit:
	return ret;

}

int npu_device_open(struct npu_device *device)
{
	int ret = 0, ret2 = 0;

	BUG_ON(!device);

	npu_hwdev_register_hwdevs(device, &device->system);

	ret = npu_debug_open(device);
	if (ret) {
		npu_err("fail(%d) in npu_debug_open\n", ret);
		goto ErrorExit;
	}

	ret = npu_log_open(device);
	if (ret) {
		npu_err("fail(%d) in npu_log_open\n", ret);
		goto err_log_open;
	}

	ret = npu_system_open(&device->system);
	if (ret) {
		npu_err("fail(%d) in npu_system_open\n", ret);
		goto err_sys_open;
	}

	ret = npu_sessionmgr_open(&device->sessionmgr);
	if (ret) {
		npu_err("fail(%d) in npu_sessionmgr_open\n", ret);
		goto err_sys_open;
	}

	/* clear npu_devcice_err_state to no_error for emergency recover */
	clear_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state);

	set_bit(NPU_DEVICE_STATE_OPEN, &device->state);

	npu_info("%d\n", ret);
	return ret;
err_sys_open:
	ret2 = npu_log_close(device);
	if (ret2)
		npu_err("fail(%d) in npu_log_close)\n", ret2);
err_log_open:
	ret2 = npu_debug_close(device);
	if (ret2)
		npu_err("fail(%d) in npu_debug_close\n", ret2);
ErrorExit:
	npu_err("ret(%d) ret2(%d)\n", ret, ret2);
	return ret;
}

int npu_device_recovery_close(struct npu_device *device)
{

	int ret = 0;

	ret += npu_system_suspend(&device->system);
	ret += npu_hwdev_recovery_shutdown(device);
	ret += npu_log_close(device);
	ret += npu_debug_close(device);
	if (ret)
		goto err_coma;

	clear_bit(NPU_DEVICE_STATE_OPEN, &device->state);

	npu_info("%d\n", ret);
	return ret;

err_coma:
	BUG_ON(1);
}

int npu_device_bootup(struct npu_device *device)
{
	int ret = 0, ret2 = 0;

	BUG_ON(!device);

	ret = npu_system_resume(&device->system, device->mode);
	if (ret) {
		npu_err("fail(%d) in npu_system_resume\n", ret);
		goto err_system_resume;
	}

	if (test_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state)) {
		ret = -ELIBACC;
		goto err_system_resume;
	}

	ret = dsp_dhcp_init(device);
	if (ret) {
		npu_err("fail(%d) in dsp_dhcp_init\n", ret);
		goto err_system_resume;
	}

	ret = proto_drv_open(device);
	if (ret) {
		npu_err("fail(%d) in proto_drv_open\n", ret);
		goto err_proto_open;
	}

	ret = __npu_device_late_open(device);
	if (ret) {
		npu_err("fail(%d) in __npu_device_late_open\n", ret);
		goto err_dev_lopen;
	}

	npu_info("%d\n", ret);
	return ret;
err_dev_lopen:
	ret2 = proto_drv_close(device);
	if (ret2)
		npu_err("fail(%d) in proto_drv_close\n", ret2);

err_proto_open:
	ret2 = npu_system_suspend(&device->system);
	if (ret2)
		npu_err("fail(%d) in npu_system_suspend\n", ret2);

err_system_resume:
	ret2 = npu_system_close(&device->system);
	if (ret2)
		npu_err("fail(%d) in npu_system_close\n", ret2);

	npu_err("ret(%d) ret2(%d)\n", ret, ret2);
	return ret;
}

int npu_device_shutdown(struct npu_device *device)
{
	int ret = 0;

	BUG_ON(!device);

	if (!test_bit(NPU_DEVICE_STATE_OPEN, &device->state)) {
		npu_err("already closed of device\n");
		ret = -EINVAL;
		goto ErrorExit;
	}

	ret = __npu_device_early_close(device);
	if (ret)
		npu_err("fail(%d) in __npu_device_early_close\n", ret);

	if (npu_device_is_emergency_err(device))
		npu_warn("NPU DEVICE IS EMERGENCY ERROR!\n");


	ret = proto_drv_close(device);
	if (ret)
		npu_err("fail(%d) in proto_drv_close\n", ret);

	dsp_dhcp_deinit(device->system.dhcp);

	ret = npu_system_suspend(&device->system);
	if (ret)
		npu_err("fail(%d) in npu_system_suspend\n", ret);
ErrorExit:
	npu_info("%d\n", ret);
	return ret;
}

int npu_device_close(struct npu_device *device)
{
	int ret = 0;

	BUG_ON(!device);

	ret = npu_system_close(&device->system);
	if (ret)
		npu_err("fail(%d) in npu_system_close\n", ret);

	ret = npu_log_close(device);
	if (ret)
		npu_err("fail(%d) in npu_log_close)\n", ret);

	ret = npu_debug_close(device);
	if (ret)
		npu_err("fail(%d) in npu_debug_close\n", ret);

	clear_bit(NPU_DEVICE_STATE_OPEN, &device->state);

	if (test_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state)) {
		npu_dbg("NPU_DEVICE_ERR_STATE_EMERGENCY bit was set before\n");
		clear_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state);
		npu_dbg("NPU_DEVICE_ERR_STATE_EMERGENCY bit is clear now");
		npu_dbg(" (%d) \n", test_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state));
	}

	npu_info("%d\n", ret);
	return ret;
}

int npu_device_start(struct npu_device *device)
{
	int ret = 0;

	BUG_ON(!device);

	mutex_lock(&device->start_stop_lock);
	ret = __npu_device_start(device);
	if (ret)
		npu_err("fail(%d) in __npu_device_start\n", ret);

	mutex_unlock(&device->start_stop_lock);
	npu_info("%d\n", ret);
	return ret;
}

int npu_device_stop(struct npu_device *device)
{
	int ret = 0;

	BUG_ON(!device);

	mutex_lock(&device->start_stop_lock);
	ret = __npu_device_stop(device);
	if (ret)
		npu_err("fail(%d) in __npu_device_stop\n", ret);

	mutex_unlock(&device->start_stop_lock);
	npu_info("%d\n", ret);

	return ret;
}

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int npu_device_suspend(struct device *dev)
{
	npu_dbg("called\n");
	return 0;
}

static int npu_device_resume(struct device *dev)
{
	npu_dbg("called\n");
	return 0;
}
#endif

static int npu_device_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;
	struct npu_device *device;

	BUG_ON(!pdev);

	dev = &pdev->dev;
	device = dev_get_drvdata(dev);

	dsp_dhcp_remove(device->system.dhcp);
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
	dsp_kernel_manager_remove(&device->kmgr);
#endif

	ret = npu_afm_release(device);
	if (ret)
		probe_err("fail(%d) in npu_afm_release\n", ret);

	ret = proto_drv_release();
	if (ret)
		probe_err("fail(%d) in proto_drv_release\n", ret);

	ret = npu_debug_release();
	if (ret)
		probe_err("fail(%d) in npu_debug_release\n", ret);

	ret = npu_log_release(device);
	if (ret)
		probe_err("fail(%d) in npu_log_release\n", ret);

	ret = npu_system_release(&device->system, pdev);
	if (ret)
		probe_err("fail(%d) in npu_system_release\n", ret);

#if IS_ENABLED(CONFIG_NPU_USE_FENCE_SYNC)
	npu_fence_remove(&device->fence);
#endif

	probe_info("completed in %s\n", __func__);
	return 0;
}

void npu_device_set_emergency_err(struct npu_device *device)
{
	npu_warn("Emergency flag set.\n");
	set_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state);
}

int npu_device_is_emergency_err(struct npu_device *device)
{

	return test_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state);
}

static const struct dev_pm_ops npu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(npu_device_suspend, npu_device_resume)
};

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id exynos_npu_match[] = {
	{
		.compatible = "samsung,exynos-npu"
	},
	{}
};
MODULE_DEVICE_TABLE(of, exynos_npu_match);
#endif

static struct platform_driver npu_driver = {
	.probe	= npu_device_probe,
	.remove = npu_device_remove,
	.driver = {
		.name	= "exynos-npu",
		.owner	= THIS_MODULE,
		.pm	= &npu_pm_ops,
		.of_match_table = of_match_ptr(exynos_npu_match),
	},
}
;
#if IS_ENABLED(CONFIG_NPU_CORE_DRIVER)		/* TODO : Move wrapper */
static const struct dev_pm_ops npu_core_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(npu_core_suspend, npu_core_resume)
	SET_RUNTIME_PM_OPS(npu_core_runtime_suspend, npu_core_runtime_resume, NULL)
};

#if IS_ENABLED(CONFIG_OF)
const static struct of_device_id exynos_npu_core_match[] = {
	{
		.compatible = "samsung,exynos-npu-core"
	},
	{}
};
MODULE_DEVICE_TABLE(of, exynos_npu_core_match);
#endif

static struct platform_driver npu_core_driver = {
	.probe	= npu_core_probe,
	.remove = npu_core_remove,
	.driver = {
		.name	= "exynos-npu-core",
		.owner	= THIS_MODULE,
		.pm	= &npu_core_pm_ops,
		.of_match_table = of_match_ptr(exynos_npu_core_match),
	},
};
#endif

static int __init npu_device_init(void)
{
	int ret = 0;
	dev_t dev = MKDEV(VISION_MAJOR, 0);

	probe_info("Linux vision interface: v1.00\n");
	ret = register_chrdev_region(dev, VISION_NUM_DEVICES, VISION_NAME);
	if (ret < 0) {
		probe_err("videodev: unable to get major %d (%d)\n", VISION_MAJOR, ret);
		return ret;
	}

	ret = class_register(&vision_class);
	if (ret < 0) {
		unregister_chrdev_region(dev, VISION_NUM_DEVICES);
		probe_err("video_dev: class_register failed(%d)\n", ret);
		return -EIO;
	}

	ret = npu_hwdev_register();
	if (ret) {
		probe_err("error(%d) in npu_hwdev_register\n", ret);
		goto err_exit;
	}

#if IS_ENABLED(CONFIG_NPU_CORE_DRIVER)
	ret = platform_driver_register(&npu_core_driver);
	if (ret) {
		probe_err("error(%d) in platform_driver_register\n", ret);
		goto err_exit;
	}
#endif

	ret = platform_driver_register(&npu_driver);
	if (ret) {
		probe_err("error(%d) in platform_driver_register\n", ret);
		goto err_exit;
	}

	probe_info("success in %s\n", __func__);
	ret = 0;
	goto ok_exit;

err_exit:
	// necessary clean-up

ok_exit:
	return ret;
}

static void __exit npu_device_exit(void)
{
	dev_t dev = MKDEV(VISION_MAJOR, 0);

	platform_driver_unregister(&npu_driver);
#if IS_ENABLED(CONFIG_NPU_CORE_DRIVER)
	platform_driver_unregister(&npu_core_driver);
#endif

	npu_hwdev_unregister();
	class_unregister(&vision_class);
	unregister_chrdev_region(dev, VISION_NUM_DEVICES);
#if IS_ENABLED(CONFIG_NPU_USE_FENCE_SYNC)
	unregister_chrdev_region(dev, FENCE_NUM_DEVICES);
#endif

	probe_info("success in %s\n", __func__);
}

module_init(npu_device_init);
module_exit(npu_device_exit);

MODULE_AUTHOR("Eungjoo Kim<ej7.kim@samsung.com>");
MODULE_DESCRIPTION("Exynos NPU driver");
MODULE_LICENSE("GPL");
