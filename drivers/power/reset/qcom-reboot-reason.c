// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/pm.h>
#include <linux/of_address.h>
#include <linux/nvmem-consumer.h>

#if IS_ENABLED(CONFIG_SEC_DEBUG)
#include <linux/sec_debug.h>
#include <soc/qcom/restart.h>

static struct nvmem_cell *nvmem_cell;
#endif

struct qcom_reboot_reason {
	struct device *dev;
	struct notifier_block reboot_nb;
	struct nvmem_cell *nvmem_cell;
};

struct poweroff_reason {
	const char *cmd;
	unsigned char pon_reason;
};

static struct poweroff_reason reasons[] = {
	{ "recovery",			0x01 },
	{ "bootloader",			0x02 },
	{ "rtc",			0x03 },
	{ "dm-verity device corrupted",	0x04 },
	{ "dm-verity enforcing",	0x05 },
	{ "keys clear",			0x06 },
#if !defined(CONFIG_QGKI)
	{ "download",			0x15 },
#endif
	{ "recovery-update",    0x60 },
	{}
};

#if IS_ENABLED(CONFIG_SEC_DEBUG)
void nvmem_write_pon_restart_reason(u8 pon_rr)
{
	if (nvmem_cell)
		nvmem_cell_write(nvmem_cell, &pon_rr, sizeof(pon_rr));
}
#endif

static int qcom_reboot_reason_reboot(struct notifier_block *this,
				     unsigned long event, void *ptr)
{
	char *cmd = ptr;
	struct qcom_reboot_reason *reboot = container_of(this,
		struct qcom_reboot_reason, reboot_nb);
	struct poweroff_reason *reason;

	if (cmd) {
		for (reason = reasons; reason->cmd; reason++) {
			if (!strcmp(cmd, reason->cmd)) {
				nvmem_cell_write(reboot->nvmem_cell,
						&reason->pon_reason,
						sizeof(reason->pon_reason));
				break;
			}
		}
	}

#if IS_ENABLED(CONFIG_SEC_DEBUG)
	if (event != SYS_POWER_OFF)
		sec_debug_update_restart_reason(cmd, 0, RESTART_NORMAL);
#endif

	return NOTIFY_OK;
}

static int qcom_reboot_reason_probe(struct platform_device *pdev)
{
	struct qcom_reboot_reason *reboot;
#if IS_ENABLED(CONFIG_SEC_DEBUG)
	struct nvmem_cell *pon_nvmem;
	uint8_t *pon_reason;
	size_t len;
#endif

	reboot = devm_kzalloc(&pdev->dev, sizeof(*reboot), GFP_KERNEL);
	if (!reboot)
		return -ENOMEM;

	reboot->dev = &pdev->dev;

	reboot->nvmem_cell = nvmem_cell_get(reboot->dev, "restart_reason");

	if (IS_ERR(reboot->nvmem_cell)) {
		pr_err("%s : probe error %d\n", __func__, PTR_ERR(reboot->nvmem_cell));
		return PTR_ERR(reboot->nvmem_cell);
	}

#if IS_ENABLED(CONFIG_SEC_DEBUG)
	nvmem_cell = reboot->nvmem_cell;
	if (nvmem_cell)
		sec_nvmem_pon_write = nvmem_write_pon_restart_reason;

	pon_nvmem = nvmem_cell_get(reboot->dev, "pon_reason");
	if (!IS_ERR(pon_nvmem)) {
		pon_reason = nvmem_cell_read(pon_nvmem, &len);
		if (*pon_reason == 0x40) {	// #define PON_PBS_SMPL_RSN 0x0640
			pr_info("pon_reason get nvmem 0x%02x\n", *pon_reason);
			panic("SMPL Occurred");
		}
	}
#endif  

	reboot->reboot_nb.notifier_call = qcom_reboot_reason_reboot;
	reboot->reboot_nb.priority = 255;
	register_reboot_notifier(&reboot->reboot_nb);

	platform_set_drvdata(pdev, reboot);

	return 0;
}

static int qcom_reboot_reason_remove(struct platform_device *pdev)
{
	struct qcom_reboot_reason *reboot = platform_get_drvdata(pdev);

	unregister_reboot_notifier(&reboot->reboot_nb);

	return 0;
}

static const struct of_device_id of_qcom_reboot_reason_match[] = {
	{ .compatible = "qcom,reboot-reason", },
	{},
};
MODULE_DEVICE_TABLE(of, of_qcom_reboot_reason_match);

static struct platform_driver qcom_reboot_reason_driver = {
	.probe = qcom_reboot_reason_probe,
	.remove = qcom_reboot_reason_remove,
	.driver = {
		.name = "qcom-reboot-reason",
		.of_match_table = of_match_ptr(of_qcom_reboot_reason_match),
	},
};

module_platform_driver(qcom_reboot_reason_driver);

MODULE_DESCRIPTION("MSM Reboot Reason Driver");
MODULE_LICENSE("GPL v2");
