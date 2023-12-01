/*
 * copyright (c) 2022 Samsung Electronics Co., Ltd.
 *            http://www.samsung.com/
 *
 * exynos-icm.c - Samsung Isolated CryptoManager driver for the Exynos
 * Author: Jiye Min <jiye.min@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/printk.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/ioctl.h>
#include <linux/sched/mm.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <asm/arch_timer.h>
#include <soc/samsung/exynos-smc.h>

#define ICM_MAX_USER_PATH_LEN			(128)

/* Time information */
#define FREQ_26MHZ				(26 * 1000 * 1000)
#define MAX_SEC_VALUE				(1000000)
#define MAX_USEC_VALUE				(1000000)

/* smc to call ldfw functions */
#define SMC_EL3_ICM_GET_INFO			(0x82000800)
#define SMC_CM_ICM				(0xC2001050)

/* command for ICM */
#define CMD_ICM_BACKUP				(0x1)
#define CMD_ICM_CHECK_VERSION			(0x2)

/* ioctl command for TAC */
#define ICM_CMD_SYNC_MAGIC			'e'
#define ICM_CMD_SYNC_BACKUP			_IO(ICM_CMD_SYNC_MAGIC, 1)
#define ICM_CMD_SYNC_RESTORE			_IO(ICM_CMD_SYNC_MAGIC, 2)

#undef ICM_DEBUG
#ifdef ICM_DEBUG
#define icm_debug(dev, fmt, arg...)		pr_info("[EXYNOS][ICM][DEBUG] " fmt, ##arg)
#else
#define icm_debug(dev, fmt, arg...)		do { } while(0)
#endif

#define icm_err(dev, fmt, arg...)		pr_err("[EXYNOS][ICM][ERROR] " fmt, ##arg)
#define icm_info(dev, fmt, arg...)		pr_info("[EXYNOS][ICM] " fmt, ##arg)

struct mutex icm_sync_lock;

struct icm_device {
	struct device *dev;
};

static uint64_t get_current_time(void)
{
	return arch_timer_read_cntpct_el0();
}

static uint64_t get_time_us(uint64_t diff_tc)
{
	return (((diff_tc % FREQ_26MHZ) * MAX_USEC_VALUE) / FREQ_26MHZ);
}

static int exynos_cm_smc(u64 *arg0, u64 *arg1, u64 *arg2, u64 *arg3)
{
	struct arm_smccc_res res;

	arm_smccc_smc(*arg0, *arg1, *arg2, *arg3, 0, 0, 0, 0, &res);

	*arg0 = res.a0;
	*arg1 = res.a1;
	*arg2 = res.a2;
	*arg3 = res.a3;

	return *arg0;
}

static int exynos_icm_get_info(void)
{
	return exynos_smc(SMC_EL3_ICM_GET_INFO, 0, 0, 0);
}

static int exynos_icm_backup(void)
{
	int ret;
	u64 reg0;
	u64 reg1;
	u64 reg2;
	u64 reg3;
	u64 start_tc = 0;
	u64 diff_tc = 0;

	reg0 = SMC_CM_ICM;
	reg1 = CMD_ICM_BACKUP;
	reg2 = 0;
	reg3 = 0;

	start_tc = get_current_time();
	ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
	if (ret)
		return ret;

	diff_tc = get_time_us(get_current_time() - start_tc);
	icm_debug(NULL, "ICM backup success. exec time = %d us\n", diff_tc);

	return ret;
}

static int exynos_icm_check_version(void)
{
	u64 reg0;
	u64 reg1;
	u64 reg2;
	u64 reg3;

	reg0 = SMC_CM_ICM;
	reg1 = CMD_ICM_CHECK_VERSION;
	reg2 = 0;
	reg3 = 0;

	return exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
}

static int exynos_icm_get_path(struct device *dev, struct task_struct *task, char *task_path)
{
	int ret = 0;
	char *buf;
	char *path;
	struct mm_struct *mm;
	struct file *exe_file;

	buf = (char *)__get_free_page(GFP_KERNEL);
	if (!buf) {
		icm_err(dev, "%s: fail to __get_free_page.\n", __func__);
		return -ENOMEM;
	}

	mm = get_task_mm(task);
	exe_file = mm->exe_file;
	if (!exe_file) {
		ret = -ENOENT;
		icm_err(dev, "%s: fail to get_task_exe_file.\n", __func__);
		goto end;
	}

	path = d_path(&exe_file->f_path, buf, PAGE_SIZE);
	if (IS_ERR(path)) {
		ret = PTR_ERR(path);
		icm_err(dev, "%s: fail to d_path. ret = 0x%x\n", __func__, ret);
		goto end;
	}

	memset(task_path, 0, ICM_MAX_USER_PATH_LEN);
	strncpy(task_path, path, ICM_MAX_USER_PATH_LEN - 1);
end:
	free_page((unsigned long)buf);

	return ret;
}

static uint32_t icm_cmd_sync(struct icm_device *icmdev, unsigned int cmd, void __user *arg)
{
	uint32_t ret = 0;

	mutex_lock(&icm_sync_lock);

	switch (cmd) {
	case ICM_CMD_SYNC_BACKUP:
		ret = exynos_icm_backup();
		if (ret)
			icm_err(icmdev->dev, "%s: exynos_icm_backup() failed. ret = 0x%x\n",
				__func__, ret);
		break;
	case ICM_CMD_SYNC_RESTORE:
		ret = exynos_icm_get_info();
		if (ret)
			icm_err(icmdev->dev, "%s: exynos_icm_get_info() failed. ret = 0x%x\n",
				__func__, ret);

		break;
	default:
		icm_err(icmdev->dev, "%s: invalid ioctl cmd: 0x%x\n", __func__, cmd);
		ret = -EPERM;
		break;
	}

	mutex_unlock(&icm_sync_lock);

	return ret;
}

static int icm_open(struct inode *inode, struct file *file)
{
	pr_info("[EXYNOS][ICM] icm_open called\n");

	return 0;
}

static long icm_ioctl(struct file *filp, unsigned int cmd, unsigned long __arg)
{
	int ret = 0;
	char path[ICM_MAX_USER_PATH_LEN];
	struct icm_device *icmdev = filp->private_data;
	void __user *arg = (void __user *)__arg;

	ret = exynos_icm_get_path(icmdev->dev, current, path);
	if (ret) {
		icm_err(icmdev->dev, "%s: fail to get user path. ret = 0x%x\n", __func__, ret);
		return ret;
	}

	icm_info(icmdev->dev, "requested by %s\n", path);

	return icm_cmd_sync(icmdev, cmd, arg);
}

static const struct file_operations icm_fops = {
	.owner          = THIS_MODULE,
	.open           = icm_open,
	.unlocked_ioctl = icm_ioctl,
};

static struct miscdevice icm_miscdev = {
	.minor          = MISC_DYNAMIC_MINOR,
	.name           = "icm",
	.fops           = &icm_fops,
};

static void __exit unregister_miscdev(void)
{
	misc_deregister(&icm_miscdev);
}

static int __init register_miscdev(void)
{
	return misc_register(&icm_miscdev);
}

static int exynos_icm_probe(struct platform_device *pdev)
{
	struct icm_device *icmdev = NULL;
	int ret;

	icmdev = devm_kzalloc(&pdev->dev, sizeof(*icmdev), GFP_KERNEL);
	if (!icmdev) {
		icm_err(&pdev->dev, "%s: fail to devm_zalloc()\n", __func__);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, icmdev);

	/* initialize icmdev struct */
	icmdev->dev = &pdev->dev;

	mutex_init(&icm_sync_lock);

	/* set misc driver */
	ret = register_miscdev();
	if (ret) {
		icm_err(icmdev->dev, "%s: fail to misc_register. ret = %d\n", __func__, ret);
		devm_kfree(icmdev->dev, icmdev);
		return -ENOMEM;
	}

	icm_info(icmdev->dev, "%s: exynos-icm registered.\n", __func__);

	return 0;
}

static int exynos_icm_remove(struct platform_device *pdev)
{
	struct icm_device *icmdev = platform_get_drvdata(pdev);

	unregister_miscdev();
	devm_kfree(icmdev->dev, icmdev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int exynos_icm_resume(struct device *dev)
{
	exynos_icm_check_version();
	exynos_icm_get_info();

	return 0;
}

static int exynos_icm_suspend(struct device *dev)
{
	int ret;

	ret = exynos_icm_backup();
	if (ret)
		icm_err(dev, "%s: icm suspend failed. ret = 0x%x\n", __func__, ret);
	else
		icm_info(dev, "%s: icm suspend success\n", __func__);

	return 0;
}
#endif

static const struct dev_pm_ops exynos_icm_pm_ops = {
	.suspend	= exynos_icm_suspend,
	.resume		= exynos_icm_resume,
};

static struct platform_driver exynos_icm_driver = {
	.probe		= exynos_icm_probe,
	.remove		= exynos_icm_remove,
	.driver		= {
		.name	= "exynos-icm",
		.owner	= THIS_MODULE,
		.pm	= &exynos_icm_pm_ops,
	},
};

static struct platform_device exynos_icm_device = {
	.name = "exynos-icm",
	.id = -1,
};

static int __init exynos_icm_init(void)
{
	int ret;

	ret = platform_device_register(&exynos_icm_device);
	if (ret) {
		pr_err("[EXYNOS][ICM] %s: fail to register device. ret = 0x%x\n",
			__func__, ret);
		return ret;
	}

	ret = platform_driver_register(&exynos_icm_driver);
	if (ret) {
		pr_err("[EXYNOS][ICM] %s: fail to register driver. ret = 0x%x\n",
			__func__, ret);
		return ret;
	}

	pr_info("[EXYNOS][ICM] %s: Exynos Isolated CM driver init done.\n", __func__);

	return 0;
}

static void __exit exynos_icm_exit(void)
{
	platform_driver_unregister(&exynos_icm_driver);
	platform_device_unregister(&exynos_icm_device);
}

module_init(exynos_icm_init);
module_exit(exynos_icm_exit);

MODULE_DESCRIPTION("Exynos Isolated CryptoManager driver");
MODULE_AUTHOR("<jiye.min@samsung.com>");
MODULE_LICENSE("GPL");

