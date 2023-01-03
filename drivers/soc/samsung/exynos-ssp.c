/*
 * exynos-ssp.c - Samsung Secure Platform driver for the Exynos
 *
 * Copyright (C) 2019 Samsung Electronics
 * Keunyoung Park <keun0.park@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/pm_wakeup.h>
#include <linux/smc.h>
#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/debugfs.h>
#include <linux/timer.h>
#include <soc/samsung/exynos-pd.h>

#define SSP_RET_OK		0
#define SSP_RETRY_MAX_COUNT	1000000

/* smc to call ldfw functions */
#define SMC_CMD_SSP		(0xC2001040)
#define SSP_CMD_BOOT		(0x1)
#define SSP_CMD_BACKUP		(0x2)
#define SSP_CMD_RESTORE		(0x3)
#define SSP_CMD_SELF_TEST	(0x10)

/* ioctl command for TAC */
#define SSP_IOCTL_MAGIC		'c'
#define SSP_IOCTL_INIT		_IO(SSP_IOCTL_MAGIC, 1)
#define SSP_IOCTL_EXIT		_IOWR(SSP_IOCTL_MAGIC, 2, uint64_t)
#define SSP_IOCTL_TEST		_IOWR(SSP_IOCTL_MAGIC, 3, uint64_t)

spinlock_t ssp_lock;
struct mutex ssp_ioctl_lock;
int ssp_power_count;
int idle_ip_index;

struct ssp_device {
	struct device *dev;
	struct miscdevice misc_device;
};

static int exynos_cm_smc(uint64_t *arg0, uint64_t *arg1,
			 uint64_t *arg2, uint64_t *arg3)
{
	register uint64_t reg0 __asm__("x0") = *arg0;
	register uint64_t reg1 __asm__("x1") = *arg1;
	register uint64_t reg2 __asm__("x2") = *arg2;
	register uint64_t reg3 __asm__("x3") = *arg3;

	__asm__ volatile (
		"smc    0\n"
		: "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3)
	);

	*arg0 = reg0;
	*arg1 = reg1;
	*arg2 = reg2;
	*arg3 = reg3;

	return *arg0;
}

static void exynos_ssp_pm_enable(struct ssp_device *sspdev)
{
	pm_runtime_enable(sspdev->dev);
	dev_info(sspdev->dev, "pm_runtime_enable\n");
}

static int exynos_ssp_power_on(struct ssp_device *sspdev)
{
	int ret = SSP_RET_OK;

	ret = pm_runtime_get_sync(sspdev->dev);
	if (ret != SSP_RET_OK)
		dev_err(sspdev->dev, "%s: fail to pm_runtime_get_sync. ret = 0x%x\n", __func__, ret);
	else
		dev_info(sspdev->dev, "pm_runtime_get_sync done\n");

	return ret;
}

static int exynos_ssp_power_off(struct ssp_device *sspdev)
{
	int ret = SSP_RET_OK;

	ret = pm_runtime_put_sync(sspdev->dev);
	if (ret != SSP_RET_OK)
		dev_err(sspdev->dev, "%s: fail to pm_runtime_put_sync. ret = 0x%x\n", __func__, ret);
	else
		dev_info(sspdev->dev, "pm_runtime_put_sync done\n");

	return ret;
}

static int exynos_ssp_boot(struct device *dev)
{
	int ret = SSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	unsigned long flag;

	dev_info(dev, "ssp boot start\n");

	reg0 = SMC_CMD_SSP;
	reg1 = SSP_CMD_BOOT;
	reg2 = 0;
	reg3 = 0;

	exynos_update_ip_idle_status(idle_ip_index, 0);

	spin_lock_irqsave(&ssp_lock, flag);
	ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
	spin_unlock_irqrestore(&ssp_lock, flag);

	exynos_update_ip_idle_status(idle_ip_index, 1);

	if (ret != SSP_RET_OK)
		dev_err(dev, "%s: fail to boot at ldfw. ret = 0x%x\n", __func__, ret);
	else
		dev_info(dev, "ssp boot done\n");

	return ret;
}

static int exynos_ssp_backup(struct device *dev)
{
	int ret = SSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	unsigned long flag;

	dev_info(dev, "ssp backup start\n");

	reg0 = SMC_CMD_SSP;
	reg1 = SSP_CMD_BACKUP;
	reg2 = 0;
	reg3 = 0;

	exynos_update_ip_idle_status(idle_ip_index, 0);

	spin_lock_irqsave(&ssp_lock, flag);
	ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
	spin_unlock_irqrestore(&ssp_lock, flag);

	exynos_update_ip_idle_status(idle_ip_index, 1);

	if (ret != SSP_RET_OK)
		dev_err(dev, "%s: fail to backup at ldfw. ret = 0x%x\n", __func__, ret);
	else
		dev_info(dev, "ssp backup done\n");

	return ret;
}

static int exynos_ssp_restore(struct device *dev)
{
	int ret = SSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	unsigned long flag;

	dev_info(dev, "ssp restore start\n");

	reg0 = SMC_CMD_SSP;
	reg1 = SSP_CMD_RESTORE;
	reg2 = 0;
	reg3 = 0;

	exynos_update_ip_idle_status(idle_ip_index, 0);

	spin_lock_irqsave(&ssp_lock, flag);
	ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
	spin_unlock_irqrestore(&ssp_lock, flag);

	exynos_update_ip_idle_status(idle_ip_index, 1);

	if (ret != SSP_RET_OK)
		dev_err(dev, "%s: fail to restore at ldfw. ret = 0x%x\n", __func__, ret);
	else
		dev_info(dev, "ssp restore done\n");

	return ret;
}

static int exynos_ssp_self_test(struct device *dev, uint64_t test_mode)
{
	int ret = SSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	unsigned long flag;

	dev_info(dev, "ssp test_mode: %d\n", (int)test_mode);

	reg0 = SMC_CMD_SSP;
	reg1 = SSP_CMD_SELF_TEST;
	reg2 = test_mode;
	reg3 = 0;

	spin_lock_irqsave(&ssp_lock, flag);
	ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
	spin_unlock_irqrestore(&ssp_lock, flag);

	if (ret != SSP_RET_OK)
		dev_err(dev, "%s: ssp test fail at ldfw. ret = 0x%x\n", __func__, ret);
	else
		dev_info(dev, "ssp test done\n");

	return ret;
}

static int exynos_ssp_enable(struct ssp_device *sspdev)
{
	int ret = SSP_RET_OK;
	static int ssp_boot_flag;

	++ssp_power_count;

	if (ssp_power_count == 1) {
		pm_stay_awake(sspdev->dev);

		ret = exynos_ssp_power_on(sspdev);
		if (unlikely(ret))
			goto OUT;

		if (!ssp_boot_flag) {
			ret = exynos_ssp_boot(sspdev->dev);
			if (unlikely(ret))
				goto OUT;
			ssp_boot_flag = 1;
		} else {
			ret = exynos_ssp_restore(sspdev->dev);
			if (unlikely(ret))
				goto OUT;
		}
	}

	dev_info(sspdev->dev, "ssp enable: count: %d\n", ssp_power_count);

	return ret;
OUT:
	pm_relax(sspdev->dev);
	return ret;
}

static int exynos_ssp_disable(struct ssp_device *sspdev)
{
	int ret = SSP_RET_OK;

	if (ssp_power_count == 0) {
		dev_err(sspdev->dev, "%s ssp has already been disabled\n", __func__);
		return ret;
	}

	--ssp_power_count;

	if (ssp_power_count == 0) {
		ret = exynos_ssp_backup(sspdev->dev);
		if (unlikely(ret))
			return ret;

		ret = exynos_ssp_power_off(sspdev);
		if (unlikely(ret))
			return ret;

		/* keep the wake-up lock when above two functions fail */
		/* for debugging purpose */

		pm_relax(sspdev->dev);
	}

	dev_info(sspdev->dev, "ssp disable: count: %d\n", ssp_power_count);

	return ret;
}

static long ssp_ioctl(struct file *filp, unsigned int cmd, unsigned long __arg)
{
	int ret = SSP_RET_OK;
	uint64_t test_mode;

	struct ssp_device *sspdev = filp->private_data;
	void __user *arg = (void __user *)__arg;

	mutex_lock(&ssp_ioctl_lock);

	switch (cmd) {
	case SSP_IOCTL_INIT:
		ret = exynos_ssp_enable(sspdev);
		break;
	case SSP_IOCTL_EXIT:
		ret = exynos_ssp_disable(sspdev);
		break;
	case SSP_IOCTL_TEST:
		ret = get_user(test_mode, (uint64_t __user *)arg);
		if (unlikely(ret)) {
			dev_err(sspdev->dev, "%s: fail to get_user. ret = 0x%x\n", __func__, ret);
			break;
		}
		ret = exynos_ssp_self_test(sspdev->dev, test_mode);
		break;
	default:
		dev_err(sspdev->dev, "%s: invalid ioctl cmd: 0x%x\n", __func__, cmd);
		ret = -EPERM;
		break;
	}

	mutex_unlock(&ssp_ioctl_lock);

	return ret;
}

static int ssp_open(struct inode *inode, struct file *file)
{
	struct miscdevice *misc = file->private_data;
	struct ssp_device *sspdev = container_of(misc,
			struct ssp_device, misc_device);

	file->private_data = sspdev;

	dev_info(sspdev->dev, "driver open is done\n");

	return 0;
}

static const struct file_operations ssp_fops = {
	.owner		= THIS_MODULE,
	.open		= ssp_open,
	.unlocked_ioctl	= ssp_ioctl,
	.compat_ioctl	= ssp_ioctl,
};

static int exynos_ssp_probe(struct platform_device *pdev)
{
	int ret = SSP_RET_OK;
	struct ssp_device *sspdev = NULL;

	dev_set_socdata(&pdev->dev, "Exynos", "CMSSP");

	sspdev = kzalloc(sizeof(struct ssp_device), GFP_KERNEL);
	if (!sspdev) {
		dev_err(&pdev->dev, "%s: fail to kzalloc.\n", __func__);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, sspdev);
	sspdev->dev = &pdev->dev;

	spin_lock_init(&ssp_lock);
	mutex_init(&ssp_ioctl_lock);

	/* enable runtime PM */
	exynos_ssp_pm_enable(sspdev);
	idle_ip_index = exynos_get_idle_ip_index(dev_name(sspdev->dev));
	exynos_update_ip_idle_status(idle_ip_index, 1);

	/* set misc driver */
	memset((void *)&sspdev->misc_device, 0, sizeof(struct miscdevice));
	sspdev->misc_device.minor = MISC_DYNAMIC_MINOR;
	sspdev->misc_device.name = "ssp";
	sspdev->misc_device.fops = &ssp_fops;
	ret = misc_register(&sspdev->misc_device);
	if (ret) {
		dev_err(sspdev->dev, "%s: fail to misc_register. ret = %d\n", __func__, ret);
		ret = -ENOMEM;
		goto err;
	}

	return SSP_RET_OK;
err:
	kfree(sspdev);

	return ret;
}

static int exynos_ssp_remove(struct platform_device *pdev)
{
	struct ssp_device *sspdev = platform_get_drvdata(pdev);

	misc_deregister(&sspdev->misc_device);
	kfree(sspdev);

	return 0;
}

#if defined(CONFIG_PM_SLEEP) || defined(CONFIG_PM_RUNTIME)
static int exynos_ssp_suspend(struct device *dev)
{
	int ret = SSP_RET_OK;

	/* Do nothing */

	return ret;
}

static int exynos_ssp_resume(struct device *dev)
{
	int ret = SSP_RET_OK;

	/* Do nothing */

	return ret;
}
#endif

static struct dev_pm_ops exynos_ssp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(exynos_ssp_suspend,
			exynos_ssp_resume)
};

#ifdef CONFIG_OF
static const struct of_device_id exynos_ssp_match[] = {
	{
		.compatible = "samsung,exynos-ssp",
	},
	{},
};
#endif

static struct platform_driver exynos_ssp_driver = {
	.probe		= exynos_ssp_probe,
	.remove		= exynos_ssp_remove,
	.driver		= {
		.name	= "ssp",
		.owner	= THIS_MODULE,
		.pm     = &exynos_ssp_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = exynos_ssp_match,
#endif
	},
};

static int __init exynos_ssp_init(void)
{
	int ret = SSP_RET_OK;

	ret = platform_driver_register(&exynos_ssp_driver);
	if (ret) {
		pr_err("[Exynos][CMSSP] %s: fail to register driver. ret = 0x%x\n", __func__, ret);
		return ret;
	}

	pr_info("[Exynos][CMSSP] driver, (c) 2019 Samsung Electronics\n");

	return 0;
}

static void __exit exynos_ssp_exit(void)
{
	platform_driver_unregister(&exynos_ssp_driver);
}

module_init(exynos_ssp_init);
module_exit(exynos_ssp_exit);

MODULE_DESCRIPTION("EXYNOS Samsung Secure Platform driver");
MODULE_AUTHOR("Keunyoung Park <keun0.park@samsung.com>");
MODULE_LICENSE("GPL");
