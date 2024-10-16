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
#include <linux/mm.h>
#include <linux/device.h>
#include <soc/samsung/exynos-smc.h>
#include <soc/samsung/exynos-pd.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#include <soc/samsung/exynos/memlogger.h>
#endif

#include "strong_mailbox_common.h"
#include "strong_mailbox_ree.h"
#include "strong_mailbox_sfr.h"

#define	CONFIG_SSP_BOOT_ASYNC
#define	CONFIG_SSP_SECURE_NVM

#define SSP_RET_OK		0
#define SSP_RET_FAIL		-1
#define SSP_RET_BUSY		0x20010	/* defined at LDFW */
#define SSP_MAX_USER_CNT	100
#define SSP_MAX_USER_PATH_LEN	128
#define SSP_K250_STABLE_TIME_MS    20

/* smc to call ldfw functions */
#define SMC_CMD_SSP		(0xC2001040)
#define SSP_CMD_BOOT		(0x1)
#define SSP_CMD_BACKUP		(0x2)
#define SSP_CMD_RESTORE		(0x3)
#define SSP_CMD_SELF_TEST	(0x10)
#define SSP_CMD_BOOT_INIT	(0x11)
#define SSP_CMD_BOOT_CHECK	(0x12)
#define SSP_CMD_RESTORE_INIT	(0x31)
#define SSP_CMD_RESTORE_CHECK	(0x32)
#define SSP_CMD_NOTIFY_AP_SLEEP		(0x40)
#define SSP_CMD_NOTIFY_AP_WAKEUP	(0x41)

/* ioctl command for TAC */
#define SSP_CMD_SYNC_MAGIC		'c'
#define SSP_CMD_SYNC_INIT		_IO(SSP_CMD_SYNC_MAGIC, 1)
#define SSP_CMD_SYNC_EXIT		_IOWR(SSP_CMD_SYNC_MAGIC, 2, uint64_t)
#define SSP_CMD_SYNC_TEST		_IOWR(SSP_CMD_SYNC_MAGIC, 3, uint64_t)
#define SSP_CMD_SYNC_DEBUG		_IOWR(SSP_CMD_SYNC_MAGIC, 4, uint64_t)

#define ssp_err(dev, fmt, arg...)	printk("[EXYNOS][CMSSP][ERROR] " fmt, ##arg)
#define ssp_info(dev, fmt, arg...)	printk("[EXYNOS][CMSSP][ INFO] " fmt, ##arg)

/* SFR for ssp power control */
#define PMU_ALIVE_PA_BASE		(0x15860000 + 0x3000)
#define PMU_SSP_STATUS_VA_OFFSET	(0x704)
#define PMU_SSP_STATUS_MASK		(1 << 0)

void __iomem *pmu_va_base;
extern void __iomem *mb_va_base;

spinlock_t ssp_lock;
struct mutex ssp_sync_lock;
static int ssp_power_count;
static int ssp_idle_ip_index;
extern struct exynos_chipid_info exynos_soc_info;

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
struct memlogs {
	struct memlog *mlog;
	struct memlog_obj *mlog_log;
	struct memlog_obj *mlog_log_file;
};
#endif

struct ssp_device {
	struct device *dev;
	struct miscdevice misc_device;
	struct regulator *secure_nvm_ldo;
	unsigned long snvm_init_time;
	/* dbg */
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
	struct memlogs mlog;
#endif
	struct timer_list dbg_timer;
	unsigned int wdt_irq;
};

struct ssp_user {
	char path[SSP_MAX_USER_PATH_LEN];
	unsigned long init_count;
	unsigned long init_time;
	unsigned long exit_count;
	unsigned long exit_time;
	unsigned long init_fail_count;
	unsigned long init_fail_time;
	unsigned long exit_fail_count;
	unsigned long exit_fail_time;
	struct ssp_user *next;
};

struct ssp_user *head = NULL;
unsigned int mb_irq;
struct ssp_device *sspdev_bak = NULL;

static void exynos_ssp_print_user_list(struct device *dev)
{
	struct ssp_user *ptr = head;
	int i = 0;

	ssp_info(dev, "====== Power control status ================="
		      "=============================================\n");
	ssp_info(dev, "No: %8s:\t%8s\t%8s\t%16s\t%16s\n",
		      "Caller", "ON", "OFF", "ON-TIME", "OFF-TIME");
	ssp_info(dev, "---------------------------------------------"
		      "---------------------------------------------\n");

	while (ptr != NULL) {
		i++;

		ssp_info(dev, "%2u:\t%8s: %s\n",
			i,
			"Path",
			ptr->path);

		ssp_info(dev, "%2u:\t%8s:\t%8u\t%8u\t%16u\t%16u\n",
			i,
			"Success",
			ptr->init_count,
			ptr->exit_count,
			ptr->init_time,
			ptr->exit_time);

		ssp_info(dev, "%2u:\t%8s:\t%8u\t%8u\t%16u\t%16u\n",
			i,
			"Fail",
			ptr->init_fail_count,
			ptr->exit_fail_count,
			ptr->init_fail_time,
			ptr->exit_fail_time);

		ptr = ptr->next;
	}
	ssp_info(dev, "---------------------------------------------"
		      "---------------------------------------------\n");
}

static int exynos_ssp_get_path(struct device *dev, struct task_struct *task, char *task_path)
{
	int ret = SSP_RET_OK;
	char *buf;
	char *path;
	struct mm_struct *mm;
	struct file *exe_file = NULL;

	buf = (char *)__get_free_page(GFP_KERNEL);
	if (!buf) {
		ssp_err(dev, "%s: fail to __get_free_page.\n", __func__);
		return -ENOMEM;
	}

	mm = get_task_mm(task);
	if (mm)
		exe_file = mm->exe_file;

	if (!exe_file) {
		ret = -ENOENT;
		//ssp_err(dev, "%s: fail to get_task_exe_file.\n", __func__);
		ssp_info(dev, "%s: check to get_task_exe_file.\n", __func__);
		goto end;
	}

	path = d_path(&exe_file->f_path, buf, PAGE_SIZE);
	if (IS_ERR(path)) {
		ret = PTR_ERR(path);
		ssp_err(dev, "%s: fail to d_path. ret = 0x%x\n", __func__, ret);
		goto end;
	}

	memset(task_path, 0, SSP_MAX_USER_PATH_LEN);
	strncpy(task_path, path, SSP_MAX_USER_PATH_LEN - 1);
end:
	free_page((unsigned long)buf);

	return ret;
}

static struct ssp_user *exynos_ssp_find_user(char *path)
{
	struct ssp_user *now = head;

	if (head == NULL)
		return NULL;

	while (strncmp(now->path, path, SSP_MAX_USER_PATH_LEN)) {
		if (now->next == NULL)
			return NULL;

		now = now->next;
	}

	return now;
}

static int exynos_ssp_powerctl_update(struct device *dev, bool power_on, bool pass)
{
	int ret = SSP_RET_OK;
	char path[SSP_MAX_USER_PATH_LEN];
	static int ssp_user_count;
	struct ssp_user *link = NULL;

	ret = exynos_ssp_get_path(dev, current, path);
	if (ret) {
		//ssp_err(dev, "%s: fail to get path. ret = 0x%x\n", __func__, ret);
		ssp_info(dev, "%s: check to get path. ret = 0x%x\n", __func__, ret);
		return ret;
	}

	link = exynos_ssp_find_user(path);

	if (link == NULL) {
		if (++ssp_user_count >= SSP_MAX_USER_CNT) {
			ssp_err(dev, "%s: exceed max user count.\n", __func__);
			return -ENOMEM;
		}

		link = (struct ssp_user *)kzalloc(sizeof(struct ssp_user), GFP_KERNEL);
		if (!link) {
			ssp_err(dev, "%s: fail to kzalloc.\n", __func__);
			return -ENOMEM;
		}

		memcpy(link->path, path, sizeof(path));
		link->init_count = 0;
		link->exit_count = 0;

		link->next = head;
		head = link;
	}

	if (power_on == 1 && pass == 1) {
		link->init_count++;
		link->init_time = ktime_get_boottime_ns() / NSEC_PER_USEC;
	} else if (power_on == 1 && pass == 0) {
		link->init_fail_count++;
		link->init_fail_time = ktime_get_boottime_ns() / NSEC_PER_USEC;
	} else if (power_on == 0 && pass == 1) {
		link->exit_count++;
		link->exit_time = ktime_get_boottime_ns() / NSEC_PER_USEC;
	} else if (power_on == 0 && pass == 0) {
		link->exit_fail_count++;
		link->exit_fail_time = ktime_get_boottime_ns() / NSEC_PER_USEC;
	}

	return ret;
}

static int exynos_cm_smc(uint64_t *arg0, uint64_t *arg1,
			 uint64_t *arg2, uint64_t *arg3)
{
	struct arm_smccc_res res;

	arm_smccc_smc(*arg0, *arg1, *arg2, *arg3, 0, 0, 0, 0, &res);

	*arg0 = res.a0;
	*arg1 = res.a1;
	*arg2 = res.a2;
	*arg3 = res.a3;

	return *arg0;
}

static int exynos_ssp_map_sfr(struct ssp_device *sspdev)
{
	int ret = SSP_RET_OK;

	pmu_va_base = ioremap(PMU_ALIVE_PA_BASE, SZ_4K);
	if (!pmu_va_base) {
		ssp_err(sspdev->dev, "%s: fail to ioremap\n", __func__);
		ret = SSP_RET_FAIL;
	}

	return ret;
}

//TODO check After applying pm runtime
#if 0
static bool exynos_ssp_check_power_status(void)
{
	unsigned int reg;

	if (!pmu_va_base)
		return false;

	reg = readl(pmu_va_base + PMU_SSP_STATUS_VA_OFFSET);

	if (reg & PMU_SSP_STATUS_MASK) {
		return true;
	} else {
		ssp_err(sspdev->dev, "%s: ssp power off\n", __func__);
		return false;
	}
}
#endif

static void exynos_ssp_pm_enable(struct ssp_device *sspdev)
{
	pm_runtime_enable(sspdev->dev);
	ssp_info(sspdev->dev, "pm_runtime_enable\n");
}

static int exynos_ssp_power_on(struct ssp_device *sspdev)
{
	int ret = SSP_RET_OK;

	ret = pm_runtime_get_sync(sspdev->dev);
	if (ret != SSP_RET_OK)
		ssp_err(sspdev->dev, "%s: fail to pm_runtime_get_sync. ret = 0x%x\n", __func__, ret);
	else
		ssp_info(sspdev->dev, "pm_runtime_get_sync done\n");

//TODO check After applying pm runtime
#if 0
	if (exynos_ssp_check_power_status() == false) {
		ssp_err(sspdev->dev, "%s: ssp power status\n", __func__);
		return SSP_RET_FAIL;
	}
#endif

	return ret;
}

static int exynos_ssp_power_off(struct ssp_device *sspdev)
{
	int ret = SSP_RET_OK;

	ret = pm_runtime_put_sync(sspdev->dev);
	if (ret != SSP_RET_OK)
		ssp_err(sspdev->dev, "%s: fail to pm_runtime_put_sync. ret = 0x%x\n", __func__, ret);
	else
		ssp_info(sspdev->dev, "pm_runtime_put_sync done\n");

	return ret;
}

#if defined(CONFIG_SSP_BOOT_ASYNC)
static int exynos_ssp_boot(struct device *dev)
{
	int ret = SSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	uint64_t count;
	unsigned long flag;

	ssp_info(dev, "ssp boot start\n");

	exynos_update_ip_idle_status(ssp_idle_ip_index, 0);
	count = 0;

	do {
		count++;

		reg0 = SMC_CMD_SSP;
		reg1 = SSP_CMD_BOOT;
		reg2 = 0;
		reg3 = 0;

		spin_lock_irqsave(&ssp_lock, flag);
		ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
		spin_unlock_irqrestore(&ssp_lock, flag);

		if (ret == SSP_RET_BUSY)
			usleep_range(500, 1000);

	} while (ret == SSP_RET_BUSY);

	exynos_update_ip_idle_status(ssp_idle_ip_index, 1);

	if (ret != SSP_RET_OK)
		ssp_err(dev, "%s: fail to boot at ldfw. ret = 0x%x\n", __func__, ret);
	else
		ssp_info(dev, "ssp boot done: %d\n", count);

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

	ssp_info(dev, "ssp restore start\n");

	reg0 = SMC_CMD_SSP;
	reg1 = SSP_CMD_RESTORE;
	reg2 = 0;
	reg3 = 0;

	exynos_update_ip_idle_status(ssp_idle_ip_index, 0);

	spin_lock_irqsave(&ssp_lock, flag);
	ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
	spin_unlock_irqrestore(&ssp_lock, flag);

	exynos_update_ip_idle_status(ssp_idle_ip_index, 1);

	if (ret != SSP_RET_OK)
		ssp_err(dev, "%s: fail to restore at ldfw. ret = 0x%x\n", __func__, ret);
	else
		ssp_info(dev, "ssp restore done\n");

	return ret;
}
#else
static int exynos_ssp_boot_init(struct device *dev)
{
	int ret = SSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	uint64_t count;
	unsigned long flag;

	ssp_info(dev, "ssp boot init start\n");

	exynos_update_ip_idle_status(ssp_idle_ip_index, 0);
	count = 0;

	do {
		count++;

		reg0 = SMC_CMD_SSP;
		reg1 = SSP_CMD_BOOT_INIT;
		reg2 = 0;
		reg3 = 0;

		spin_lock_irqsave(&ssp_lock, flag);
		ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
		spin_unlock_irqrestore(&ssp_lock, flag);

		if (ret == SSP_RET_BUSY)
			usleep_range(500, 1000);

	} while (ret == SSP_RET_BUSY);

	if (ret != SSP_RET_OK)
		ssp_err(dev, "%s: fail to boot at ldfw. ret = 0x%x\n", __func__, ret);
	else
		ssp_info(dev, "ssp boot init done: %d\n", count);

	return ret;
}

static int exynos_ssp_boot_check(struct device *dev)
{
	int ret = SSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	uint64_t count;
	unsigned long flag;

	ssp_info(dev, "ssp boot check start\n");

	count = 0;

	do {
		count++;

		reg0 = SMC_CMD_SSP;
		reg1 = SSP_CMD_BOOT_CHECK;
		reg2 = 0;
		reg3 = 0;

		spin_lock_irqsave(&ssp_lock, flag);
		ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
		spin_unlock_irqrestore(&ssp_lock, flag);

		if (ret == SSP_RET_BUSY)
			usleep_range(500, 1000);

	} while (ret == SSP_RET_BUSY);

	exynos_update_ip_idle_status(ssp_idle_ip_index, 1);

	if (ret != SSP_RET_OK)
		ssp_err(dev, "%s: fail to boot check at ldfw. ret = 0x%x\n", __func__, ret);
	else
		ssp_info(dev, "ssp boot check done: %d\n", count);

	return ret;
}

static int exynos_ssp_restore_init(struct device *dev)
{
	int ret = SSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	unsigned long flag;

	ssp_info(dev, "ssp restore init start\n");

	reg0 = SMC_CMD_SSP;
	reg1 = SSP_CMD_RESTORE_INIT;
	reg2 = 0;
	reg3 = 0;

	exynos_update_ip_idle_status(ssp_idle_ip_index, 0);

	spin_lock_irqsave(&ssp_lock, flag);
	ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
	spin_unlock_irqrestore(&ssp_lock, flag);

	if (ret != SSP_RET_OK)
		ssp_err(dev, "%s: fail to restore check at ldfw. ret = 0x%x\n", __func__, ret);
	else
		ssp_info(dev, "ssp restore init done\n");

	return ret;
}

static int exynos_ssp_restore_check(struct device *dev)
{
	int ret = SSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	unsigned long flag;

	ssp_info(dev, "ssp restore check start\n");

	reg0 = SMC_CMD_SSP;
	reg1 = SSP_CMD_RESTORE_CHECK;
	reg2 = 0;
	reg3 = 0;

	spin_lock_irqsave(&ssp_lock, flag);
	ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
	spin_unlock_irqrestore(&ssp_lock, flag);

	exynos_update_ip_idle_status(ssp_idle_ip_index, 1);

	if (ret != SSP_RET_OK)
		ssp_err(dev, "%s: fail to restore check at ldfw. ret = 0x%x\n", __func__, ret);
	else
		ssp_info(dev, "ssp restore check done\n");

	return ret;
}
#endif

static int exynos_ssp_backup(struct device *dev)
{
	int ret = SSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	unsigned long flag;

	ssp_info(dev, "ssp backup start\n");

	reg0 = SMC_CMD_SSP;
	reg1 = SSP_CMD_BACKUP;
	reg2 = 0;
	reg3 = 0;

	exynos_update_ip_idle_status(ssp_idle_ip_index, 0);

	spin_lock_irqsave(&ssp_lock, flag);
	ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
	spin_unlock_irqrestore(&ssp_lock, flag);

	exynos_update_ip_idle_status(ssp_idle_ip_index, 1);

	if (ret != SSP_RET_OK)
		ssp_err(dev, "%s: fail to backup at ldfw. ret = 0x%x\n", __func__, ret);
	else
		ssp_info(dev, "ssp backup done\n");

	return ret;
}

int exynos_ssp_notify_ap_sleep(void)
{
	int ret = SSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	unsigned long flag;

	ssp_info(0, "ssp notify ap sleep start\n");

	reg0 = SMC_CMD_SSP;
	reg1 = SSP_CMD_NOTIFY_AP_SLEEP;
	reg2 = 0;
	reg3 = 0;

	spin_lock_irqsave(&ssp_lock, flag);
	ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
	spin_unlock_irqrestore(&ssp_lock, flag);

	if (ret != SSP_RET_OK)
		ssp_err(0, "%s: fail to notify ap sleep at ldfw. ret = 0x%x\n", __func__, ret);
	else
		ssp_info(0, "ssp notify ap sleep done\n");

	return ret;
}
EXPORT_SYMBOL(exynos_ssp_notify_ap_sleep);

int exynos_ssp_notify_ap_wakeup(void)
{
	int ret = SSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	unsigned long flag;

	ssp_info(0, "ssp notify ap wakeup start\n");

	reg0 = SMC_CMD_SSP;
	reg1 = SSP_CMD_NOTIFY_AP_WAKEUP;
	reg2 = 0;
	reg3 = 0;

	spin_lock_irqsave(&ssp_lock, flag);
	ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
	spin_unlock_irqrestore(&ssp_lock, flag);

	if (ret != SSP_RET_OK)
		ssp_err(0, "%s: fail to notify ap wakeup at ldfw. ret = 0x%x\n", __func__, ret);
	else
		ssp_info(0, "ssp notify ap wakeup done\n");

	return ret;
}
EXPORT_SYMBOL(exynos_ssp_notify_ap_wakeup);

static int exynos_ssp_self_test(struct device *dev, uint64_t test_mode)
{
	int ret = SSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	unsigned long flag;

	ssp_info(dev, "call ssp function: %d\n", (int)test_mode);

	reg0 = SMC_CMD_SSP;
	reg1 = SSP_CMD_SELF_TEST;
	reg2 = test_mode;
	reg3 = 0;

	spin_lock_irqsave(&ssp_lock, flag);
	ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
	spin_unlock_irqrestore(&ssp_lock, flag);

	ssp_info(dev, "return from ldfw: 0x%x\n", ret);

	return ret;
}

#if defined(CONFIG_SSP_SECURE_NVM)
static int exynos_se_power_on_phase1(struct ssp_device *dev)
{
	int ret = SSP_RET_OK;

	ssp_info(dev, "Secure NVM on\n");

	if (IS_ERR(dev->secure_nvm_ldo) || regulator_enable(dev->secure_nvm_ldo) < 0) {
		ssp_err(dev, "%s - failed to enable LDO for SE\n", __func__);
		return -ENODEV;
	}
	dev->snvm_init_time = ktime_get_boottime_ns();

	return ret;
}

static int exynos_se_power_on_phase2(struct ssp_device *dev)
{
	int ret = SSP_RET_OK;
	unsigned long delay;

	/* delay for chip stabilization */
	delay = (ktime_get_boottime_ns() - dev->snvm_init_time) / NSEC_PER_MSEC;

	if (delay < SSP_K250_STABLE_TIME_MS) {
		mdelay(SSP_K250_STABLE_TIME_MS - delay);
	}

	ssp_info(dev, "Secure NVM on\n");
	return ret;
}

static int exynos_se_power_off(struct ssp_device *dev)
{
	int ret = SSP_RET_OK;

	ssp_info(dev, "Secure NVM off\n");

	if (IS_ERR(dev->secure_nvm_ldo) || regulator_disable(dev->secure_nvm_ldo) < 0) {
		ssp_err(dev, "%s - failed to disable LDO for SE\n", __func__);
		return -ENODEV;
	}

	mdelay(SSP_K250_STABLE_TIME_MS);

	ssp_info(dev, "Secure NVM off done\n");

	return ret;
}
#endif

static int exynos_ssp_enable(struct ssp_device *sspdev)
{
	int ret = SSP_RET_OK;
	static int ssp_boot_flag;
	static int fail_cnt;

	++ssp_power_count;
	ssp_info(sspdev->dev, "ssp enable start: count: %d\n", ssp_power_count);

	if (ssp_power_count == 1) {
		pm_stay_awake(sspdev->dev);

		if (!ssp_boot_flag) {
#if defined(CONFIG_SSP_SECURE_NVM)
			ret = exynos_se_power_on_phase1(sspdev);
			if (unlikely(ret))
				goto ERR_OUT1;
#endif
#if defined(CONFIG_SSP_BOOT_ASYNC)
			ret = exynos_ssp_power_on(sspdev);
			if (unlikely(ret))
				goto ERR_OUT2;
			printk("[june] test async and get sync\n");

			ret = exynos_ssp_boot(sspdev->dev);
			if (unlikely(ret))
				goto ERR_OUT3;
#else
			ret = exynos_ssp_power_off(sspdev);
			if (unlikely(ret))
				goto ERR_OUT2;

			/* step1: write FW base & size to mailbox for boot */
			ret = exynos_ssp_boot_init(sspdev->dev);
			if (unlikely(ret))
				goto ERR_OUT2;

			/* step2: power on */
			ret = exynos_ssp_power_on(sspdev);
			if (unlikely(ret))
				goto ERR_OUT2;

			/* step3: check booting status */
			ret = exynos_ssp_boot_check(sspdev->dev);
			if (unlikely(ret))
				goto ERR_OUT3;
#endif
#if defined(CONFIG_SSP_SECURE_NVM)
			ret = exynos_se_power_on_phase2(sspdev);
			if (unlikely(ret))
				goto ERR_OUT3;
#endif
			ssp_boot_flag = 1;
		} else {
#if defined(CONFIG_SSP_SECURE_NVM)
			ret = exynos_se_power_on_phase1(sspdev);
			if (unlikely(ret))
				goto ERR_OUT1;
#endif
#if defined(CONFIG_SSP_BOOT_ASYNC)
			ret = exynos_ssp_power_on(sspdev);
			if (unlikely(ret))
				goto ERR_OUT2;

			ret = exynos_ssp_restore(sspdev->dev);
			if (unlikely(ret))
				goto ERR_OUT3;
#else
			/* step1: write FW base & size to mailbox for boot */
			ret = exynos_ssp_restore_init(sspdev->dev);
			if (unlikely(ret))
				goto ERR_OUT2;

			/* step2: power on */
			ret = exynos_ssp_power_on(sspdev);
			if (unlikely(ret))
				goto ERR_OUT2;

			/* step3: check booting status */
			ret = exynos_ssp_restore_check(sspdev->dev);
			if (unlikely(ret))
				goto ERR_OUT3;
#endif
#if defined(CONFIG_SSP_SECURE_NVM)
			ret = exynos_se_power_on_phase2(sspdev);
			if (unlikely(ret))
				goto ERR_OUT3;
#endif
		}
	}

	exynos_ssp_powerctl_update(sspdev->dev, 1, 1);
	ssp_info(sspdev->dev, "ssp enable done: count: %d\n", ssp_power_count);

	return ret;

ERR_OUT3:
	exynos_ssp_power_off(sspdev);

ERR_OUT2:
#if defined(CONFIG_SSP_SECURE_NVM)
	exynos_se_power_off(sspdev);

ERR_OUT1:
#endif
	exynos_ssp_powerctl_update(sspdev->dev, 1, 0);
	pm_relax(sspdev->dev);
	--ssp_power_count;

	if (ret != SSP_RET_OK)
	    fail_cnt++;

	ssp_err(sspdev->dev, "ssp enable fail: count: %d, fail_cnt: %d\n", ssp_power_count, fail_cnt);
	BUG_ON(fail_cnt >= 3);

	return ret;
}

static int exynos_ssp_disable(struct ssp_device *sspdev, uint32_t mailbox_flag)
{
	int ret = SSP_RET_OK;

	if (ssp_power_count <= 0) {
		ssp_err(sspdev->dev, "%s ssp has already been disabled\n", __func__);
		ssp_err(sspdev->dev, "ssp disable: count: %d\n", ssp_power_count);
		return ret;
	}

	--ssp_power_count;
	ssp_info(sspdev->dev, "ssp disable start: count: %d\n", ssp_power_count);

	if (ssp_power_count == 0) {
		if (mailbox_flag == ON) {
			ret = mailbox_cmd(SRAM_BACKUP);
			ssp_info(sspdev->dev, "mailbox_backup_sram [ret: 0x%X]\n", ret);
		} else {
			ret = exynos_ssp_backup(sspdev->dev);
		}
		if (unlikely(ret))
			goto ERR_OUT1;

		ret = exynos_ssp_power_off(sspdev);
		if (unlikely(ret))
			goto ERR_OUT2;

#if defined(CONFIG_SSP_SECURE_NVM)
		ret = exynos_se_power_off(sspdev);
		if (unlikely(ret))
			goto ERR_OUT3;
#endif

		/* keep the wake-up lock when above two functions fail */
		/* for debugging purpose */

		pm_relax(sspdev->dev);
	}

	exynos_ssp_powerctl_update(sspdev->dev, 0, 1);
	ssp_info(sspdev->dev, "ssp disable done: count: %d\n", ssp_power_count);

	return ret;

ERR_OUT1:
	exynos_ssp_power_off(sspdev);

ERR_OUT2:
#if defined(CONFIG_SSP_SECURE_NVM)
	exynos_se_power_off(sspdev);

ERR_OUT3:
#endif
	exynos_ssp_powerctl_update(sspdev->dev, 0, 0);
	pm_relax(sspdev->dev);

	return ret;
}

void ssp_sync_mutex(uint32_t lock)
{
	if (lock == ON)
		mutex_lock(&ssp_sync_lock);
	else if (lock == OFF)
		mutex_unlock(&ssp_sync_lock);
}

static uint32_t ssp_cmd_sync(struct ssp_device *sspdev, unsigned int cmd, void __user *arg)
{
	uint32_t ret = SSP_RET_OK;
	uint64_t test_mode;

	ssp_sync_mutex(ON);

	switch (cmd) {
	case SSP_CMD_SYNC_INIT:
		ret = exynos_ssp_enable(sspdev);
		if (ret != SSP_RET_OK)
			exynos_ssp_print_user_list(sspdev->dev);
		break;
	case SSP_CMD_SYNC_EXIT:
		ret = exynos_ssp_disable(sspdev, ON);
		if (ret != SSP_RET_OK)
			exynos_ssp_print_user_list(sspdev->dev);
		break;
	case SSP_CMD_SYNC_TEST:
		ret = get_user(test_mode, (uint64_t __user *)arg);
		if (unlikely(ret)) {
			ssp_err(sspdev->dev, "%s: fail to get_user. ret = 0x%x\n", __func__, ret);
			break;
		}
		ret = exynos_ssp_self_test(sspdev->dev, test_mode);
		break;
	case SSP_CMD_SYNC_DEBUG:
		ssp_info(sspdev->dev, "power-on count: %d\n", ssp_power_count);
		exynos_ssp_print_user_list(sspdev->dev);
		break;
	default:
		ssp_err(sspdev->dev, "%s: invalid ioctl cmd: 0x%x\n", __func__, cmd);
		ret = -EPERM;
		break;
	}

	ssp_sync_mutex(OFF);

	return ret;
}

static long ssp_ioctl(struct file *filp, unsigned int cmd, unsigned long __arg)
{
	int ret = SSP_RET_OK;
	char path[SSP_MAX_USER_PATH_LEN];

	struct ssp_device *sspdev = filp->private_data;
	void __user *arg = (void __user *)__arg;

	ret = exynos_ssp_get_path(sspdev->dev, current, path);
	if (ret) {
		ssp_err(sspdev->dev, "%s: fail to get user path. ret = 0x%x\n", __func__, ret);
		return ret;
	}

	ssp_info(sspdev->dev, "requested by %s\n", path);

	return ssp_cmd_sync(sspdev, cmd, arg);
}

uint32_t strong_enable(void)
{
	return ssp_cmd_sync(sspdev_bak, SSP_CMD_SYNC_INIT, NULL);
}

uint32_t strong_disable(void)
{
	return ssp_cmd_sync(sspdev_bak, SSP_CMD_SYNC_EXIT, NULL);
}

/*
 * strong_get_power_count must be used on environment
 * which ssp_sync_mutex turn on
 */
uint32_t strong_get_power_count(void)
{
	return ssp_power_count;
}

static int ssp_open(struct inode *inode, struct file *file)
{
	struct miscdevice *misc = file->private_data;
	struct ssp_device *sspdev = container_of(misc,
			struct ssp_device, misc_device);

	file->private_data = sspdev;

	ssp_info(sspdev->dev, "driver open is done\n");

	return 0;
}

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#define pr_memlog(sspdev, fmt, ...)                             \
do {                                                            \
	if (sspdev->mlog.mlog_log)                    \
		memlog_write_printf(sspdev->mlog.mlog_log,               \
	                        MEMLOG_LEVEL_EMERG,             \
	                        fmt, ##__VA_ARGS__);            \
	else                                                    \
		pr_err(fmt, ##__VA_ARGS__);                     \
} while (0)
#else
#define pr_memlog ssp_info
#endif

static char dbgvalname[MB_DBG_MSG][8] = {
	"msgcnt", "r0", "r1", "r2", "r3", "ip", "st",
	"lr", "pc", "xpsr", "msp", "psp",  "cfsr", "far"};

void exynos_strong_faulthandler(struct ssp_device *sspd)
{
	int i;
	char str[STR_MAX_MB_DATA_LEN];
	int dbgval[MB_DBG_MSG];

	mailbox_fault_and_callback(1, dbgval, str);

	pr_memlog(sspd, "\n\n[DEBUG Handle fault] %s\n", str);
	ssp_info(sspd->dev, "\n\n[DEBUG Handle fault] %s\n", str);
	for (i = 1; i < MB_DBG_MSG; i++) {
		pr_memlog(sspd, "%4s: 0x%x\n", dbgvalname[i], dbgval[i]);
		ssp_info(sspd->dev, "%8s: %x\n", dbgvalname[i], dbgval[i]);
	}

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
	if (sspd->mlog.mlog_log) {
		pr_info("[ssp-dbg] write memlog write to file!!  ");
		memlog_sync_to_file(sspd->mlog.mlog_log);
	}
#endif
}

static irqreturn_t exynos_strong_mb_irq_handler(int irq, void *data)
{
	uint32_t ret;
	unsigned int status = read_sfr(mb_va_base + STR_MB_INTSR0_OFFSET);
	struct ssp_device *sspdev = data;

	if (status & STR_MB_INTGR0_ON) {
		ret = mailbox_receive_and_callback();
		if (ret) {
			ssp_err(0, "%s [ret: 0x%X]\n", __func__, ret);
		}
		write_sfr(mb_va_base + STR_MB_INTCR0_OFFSET, STR_MB_INTGR0_ON);
	}
	if (status & STR_MB_INTGR0_DBG_ON) {
		exynos_strong_faulthandler(sspdev);
		write_sfr(mb_va_base + STR_MB_INTCR0_OFFSET, STR_MB_INTGR0_DBG_ON);
	}
	return IRQ_HANDLED;
}

static void ssp_dbg_timer_callback(struct timer_list *timer)
{
	ssp_info(0, "%s called\n", __func__);

	mailbox_fault_and_callback(0, NULL, NULL); /* for wdt without fault */
}

static irqreturn_t ssp_irq_wdt_handler(int irq, void *data)
{
	struct ssp_device *sspdev = data;

	pr_memlog(sspdev, "\n\n%s occurs.\n", __func__);
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
	if (sspdev->mlog.mlog_log) {
		pr_info("[ssp-dbg] %s: write memlog write to file!!", __func__);
		memlog_sync_to_file(sspdev->mlog.mlog_log);
	}
#endif

	ssp_info(0, "%s occurs.\n", __func__);
	disable_irq_nosync(sspdev->wdt_irq);
	mod_timer(&sspdev->dbg_timer, jiffies + (3 * HZ));
	return IRQ_HANDLED;
}

static int exynos_strong_memlog_init(struct ssp_device *sspdev)
{
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
	int ret;

	ret = memlog_register("ssp", sspdev->dev, &sspdev->mlog.mlog);
	if (ret) {
		ssp_err(sspdev->dev, "%s: ssp memlog registration fail ret:%d\n", __func__, ret);
		return ret;
	}

	sspdev->mlog.mlog_log_file =
			memlog_alloc_file(sspdev->mlog.mlog, "log-fil", SZ_4K, SZ_16K, 200, 4);
	dev_info(sspdev->dev, "ssp memlog log file alloc: %s\n",  sspdev->mlog.mlog_log_file ? "pass" : "fail");

	if (sspdev->mlog.mlog_log_file) {
		memlog_obj_set_sysfs_mode(sspdev->mlog.mlog_log_file, true);
		/* error handling */
		sspdev->mlog.mlog_log =
		    memlog_alloc_printf(sspdev->mlog.mlog, SZ_4K, sspdev->mlog.mlog_log_file, "log-mem", 0);
		dev_info(sspdev->dev, "ssp memlog log print alloc: %s\n",  sspdev->mlog.mlog_log ? "pass" : "fail");
	}
	dev_info(sspdev->dev, "ssp memlog log print %s\n", sspdev->mlog.mlog_log ? "pass" : "fail");

	return ret;
#else
	return 0;
#endif
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
	struct irq_data *stmb_irqd = NULL;
	irq_hw_number_t hwirq = 0;

	sspdev = kzalloc(sizeof(struct ssp_device), GFP_KERNEL);
	if (!sspdev) {
		ssp_err(&pdev->dev, "%s: fail to kzalloc.\n", __func__);
		return -ENOMEM;
	}
	sspdev_bak = sspdev;

	platform_set_drvdata(pdev, sspdev);
	sspdev->dev = &pdev->dev;

	spin_lock_init(&ssp_lock);
	mutex_init(&ssp_sync_lock);

	exynos_ssp_map_sfr(sspdev);

#if defined(CONFIG_SSP_SECURE_NVM)
	/* get SE regulator */
	sspdev->secure_nvm_ldo = devm_regulator_get_optional(&(pdev->dev), "vdd_se");
	if (IS_ERR(sspdev->secure_nvm_ldo)) {
		ssp_err(sspdev->dev, "%s: failed to get regulator", __func__);
		ret = -ENODEV;
		goto err;
	}
#endif

	/* enable runtime PM */
	exynos_ssp_pm_enable(sspdev);
	ssp_idle_ip_index = exynos_get_idle_ip_index(dev_name(sspdev->dev), 1);
	exynos_update_ip_idle_status(ssp_idle_ip_index, 1);

	/* set misc driver */
	memset((void *)&sspdev->misc_device, 0, sizeof(struct miscdevice));
	sspdev->misc_device.minor = MISC_DYNAMIC_MINOR;
	sspdev->misc_device.name = "ssp";
	sspdev->misc_device.fops = &ssp_fops;
	ret = misc_register(&sspdev->misc_device);
	if (ret) {
		ssp_err(sspdev->dev, "%s: fail to misc_register. ret = %d\n", __func__, ret);
		ret = -ENOMEM;
		goto err;
	}

	ret = device_init_wakeup(sspdev->dev, true);
	if (ret) {
		ssp_err(sspdev->dev, "%s: fail to init wakeup. ret = %d\n", __func__, ret);
		goto err;
	}

	mb_irq = irq_of_parse_and_map(sspdev->dev->of_node, 0);
	if (!mb_irq) {
		ssp_err(sspdev->dev, "Fail to get irq from dt. mb_irq = 0x%X\n", mb_irq);
		return -EINVAL;
	}

	stmb_irqd = irq_get_irq_data(mb_irq);
	if (!stmb_irqd) {
		ssp_err(sspdev->dev, "Fail to get irq_data\n");
		return -EINVAL;
	}

	hwirq = irqd_to_hwirq(stmb_irqd);

	ret = devm_request_irq(sspdev->dev, mb_irq,
			exynos_strong_mb_irq_handler,
			IRQF_TRIGGER_RISING, pdev->name, sspdev);

	sspdev->wdt_irq = irq_of_parse_and_map(sspdev->dev->of_node, 1);
	if (sspdev->wdt_irq) {
		ret = devm_request_irq(sspdev->dev, sspdev->wdt_irq, ssp_irq_wdt_handler,
							IRQF_TRIGGER_RISING, pdev->name, sspdev);
		ssp_info(sspdev->dev, "Registered wdt_irq = 0x%X\n", sspdev->wdt_irq, ret);
		if (ret) {
			ssp_err(sspdev->dev, "failed to request: %d\n", ret);
			return ret;
		}
	}
	ssp_info(sspdev->dev, "Inited wdt_irq = 0x%X. ret:%d\n", sspdev->wdt_irq, ret);

	ret = exynos_strong_mailbox_map_sfr();
	if (ret) {
		dev_err(sspdev->dev, "Fail to ioremap. ret = 0x%X\n", ret);
		return -EINVAL;
	}

	/* debug features */
	timer_setup(&sspdev->dbg_timer, ssp_dbg_timer_callback, 0);
	ret = exynos_strong_memlog_init(sspdev);
	if (ret) {
		dev_err(sspdev->dev, "Fail to init memlog. ret = 0x%X\n", ret);
		return -EINVAL;
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
