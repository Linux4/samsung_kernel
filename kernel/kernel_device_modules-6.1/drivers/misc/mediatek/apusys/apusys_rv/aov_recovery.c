// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/types.h>
#include <linux/rpmsg.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/mutex.h>

#include "scp.h"
#include "apusys_core.h"
#include "apusys_secure.h"
#include "aov_recovery.h"
#include "apu_ipi.h"
#include "apu_hw_sema.h"

static struct mtk_apu *m_apu;

struct aov_rpmsg_device {
	struct rpmsg_endpoint *ept;
	struct rpmsg_device *rpdev;
	struct completion ack;
};

static struct aov_rpmsg_device aov_rpm_dev;
static struct mutex aov_ipi_mtx;
static void apusys_aov_recovery_remove(struct rpmsg_device *rpdev);

static uint32_t apusys_rv_smc_call(struct device *dev, uint32_t smc_id,
	uint32_t a2)
{
	struct arm_smccc_res res;

	dev_info(dev, "%s: smc call %d\n",
			__func__, smc_id);

	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL, smc_id,
				a2, 0, 0, 0, 0, 0, &res);
	if (((int) res.a0) < 0)
		dev_info(dev, "%s: smc call %d return error(%ld)\n",
			__func__,
			smc_id, res.a0);

	return res.a0;
}


static int aov_recovery_ipi_send(void)
{
	uint32_t param = APU_IPI_SCP_NP_RECOVER;
	int ret = 0;

	if (!aov_rpm_dev.ept)
		return -1;

	mutex_lock(&aov_ipi_mtx);

    /* power on */
	ret = rpmsg_sendto(aov_rpm_dev.ept, NULL, 1, 0);
	if (ret && ret != -EOPNOTSUPP) {
		pr_info("%s: rpmsg_sendto(power on) fail(%d)\n", __func__, ret);
		goto out;
	}

	ret = rpmsg_send(aov_rpm_dev.ept, &param, sizeof(param));
	if (ret) {
		pr_info("%s: rpmsg_send fail(%d)\n", __func__, ret);
		/* power off to restore ref cnt */
		ret = rpmsg_sendto(aov_rpm_dev.ept, NULL, 0, 1);
		if (ret && ret != -EOPNOTSUPP)
			pr_info("%s: rpmsg_sendto(power off) fail(%d)\n", __func__, ret);
		goto out;
	}

    /* wait for receiving ack to ensure uP clear irq status done */
	ret = wait_for_completion_timeout(
		&aov_rpm_dev.ack, msecs_to_jiffies(100));
	if (ret == 0) {
		pr_info("%s: wait for completion timeout\n", __func__);
		ret = -1;
	} else {
		ret = 0;
	}

out:
	mutex_unlock(&aov_ipi_mtx);

	return ret;
}

int aov_recovery_cb(void)
{
	apusys_rv_smc_call(m_apu->dev,
					MTK_APUSYS_KERNEL_OP_APUSYS_RELESE_SCP_HW_SEM, 0);

	//blocking wait for ipi ack and LP logger buffer copy done
	aov_recovery_ipi_send();
	//return and then scp kernel driver trigger AEE and let scp reboot
	return 0;
}
EXPORT_SYMBOL(aov_recovery_cb);

static int aov_recovery_probe(struct rpmsg_device *rpdev)
{
	struct device *dev = &rpdev->dev;

	dev_info(dev, "%s: name=%s, src=%d\n",
		__func__, rpdev->id.name, rpdev->src);

	aov_rpm_dev.ept = rpdev->ept;
	aov_rpm_dev.rpdev = rpdev;

	return 0;
}

static int aov_recovery_callback(struct rpmsg_device *rpdev, void *data, int len, void *priv,
				 u32 src)
{
	int ret;

	complete(&aov_rpm_dev.ack);

	/* power off */
	ret = rpmsg_sendto(aov_rpm_dev.ept, NULL, 0, 1);
	if (ret && ret != -EOPNOTSUPP)
		pr_info("%s: rpmsg_sendto(power off) fail(%d)\n", __func__, ret);

	return 0;
}

static const struct of_device_id apu_aov_recovery_of_match[] = {
	{ .compatible = "mediatek,apu-scp-np-recover-rpmsg", },
	{},
};


static struct rpmsg_driver aov_recovery_driver = {
	.drv = {
		.name = "apu-scp-np-recover-rpmsg",
		.owner = THIS_MODULE,
		.of_match_table = apu_aov_recovery_of_match,
	},
	.probe = aov_recovery_probe,
	.callback = aov_recovery_callback,
	.remove = apusys_aov_recovery_remove,
};

int aov_recovery_ipi_init(struct platform_device *pdev, struct mtk_apu *apu)
{
	int ret = 0;

	mutex_init(&aov_ipi_mtx);
	init_completion(&aov_rpm_dev.ack);
	m_apu = apu;

	ret = register_rpmsg_driver(&aov_recovery_driver);
	if (ret)
		pr_info("%s Failed to register aov rpmsg driver, ret %d\n", __func__, ret);

	return ret;
}

static void apusys_aov_recovery_remove(struct rpmsg_device *rpdev)
{
}

void aov_recovery_exit(void)
{
}


