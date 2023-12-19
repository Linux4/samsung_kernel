/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/adsp/slpi_motor.h>
#include <linux/adsp/ssc_ssr_reason.h>
#ifdef CONFIG_SUPPORT_SSC_MODE
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#endif

#ifdef CONFIG_SUPPORT_DEVICE_MODE
#include <linux/hall.h>
#endif
#include "adsp.h"

#define SSR_REASON_LEN	128
#ifdef CONFIG_SEC_FACTORY
#define SLPI_STUCK "SLPI_STUCK"
#define SLPI_PASS "SLPI_PASS"
#endif

#define SENSOR_DUMP_CNT 3 /* Accel/Gyro, Magnetic, Light */
#define SENSOR_DUMP_DONE "SENSOR_DUMP_DONE"

#ifdef CONFIG_SUPPORT_SSC_MODE
#ifdef CONFIG_SUPPORT_SSC_MODE_FOR_MAG
#define ANT_NFC_MST  0
#define ANT_NFC_ONLY 1
#define ANT_DUMMY    2
#endif
#endif

struct sdump_data {
	struct workqueue_struct *sdump_wq;
	struct work_struct work_sdump;
	struct adsp_data *dev_data;
};
struct sdump_data *pdata_sdump;

static int pid;
static char panic_msg[SSR_REASON_LEN];
/*************************************************************************/
/* factory Sysfs							 */
/*************************************************************************/

static char operation_mode_flag[11];

static ssize_t dumpstate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int32_t type[1];

	if (pid != 0) {
		pr_info("[FACTORY] to take the logs\n");
		type[0] = 0;
	} else {
		type[0] = 2;
		pr_info("[FACTORY] logging service was stopped %d\n", pid);
	}

	adsp_unicast(type, sizeof(type), MSG_SSC_CORE, 0, MSG_TYPE_DUMPSTATE);
	return snprintf(buf, PAGE_SIZE, "SSC_CORE\n");
}

static ssize_t operation_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s", operation_mode_flag);
}

static ssize_t operation_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int i;

	for (i = 0; i < 10 && buf[i] != '\0'; i++)
		operation_mode_flag[i] = buf[i];
	operation_mode_flag[i] = '\0';

	pr_info("[FACTORY] %s: operation_mode_flag = %s\n", __func__,
		operation_mode_flag);
	return size;
}

static ssize_t mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	unsigned long timeout;
	unsigned long timeout_2;
	int32_t type[1];

	if (pid != 0) {
		timeout = jiffies + (2 * HZ);
		timeout_2 = jiffies + (10 * HZ);
		type[0] = 1;
		pr_info("[FACTORY] To stop logging %d\n", pid);
		adsp_unicast(type, sizeof(type), MSG_SSC_CORE, 0, MSG_TYPE_DUMPSTATE);

		while (pid != 0) {
			msleep(25);
			if (time_after(jiffies, timeout))
				pr_info("[FACTORY] %s: Timeout!!!\n", __func__);
			if (time_after(jiffies, timeout_2)) {
				pr_info("[FACTORY] pid %d\n", pid);
				return snprintf(buf, PAGE_SIZE, "1\n");
			}
		}
	}

	return snprintf(buf, PAGE_SIZE, "0\n");
}

static ssize_t mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int data = 0;
	int32_t type[1];
	if (kstrtoint(buf, 10, &data)) {
		pr_err("[FACTORY] %s: kstrtoint fail\n", __func__);
		return -EINVAL;
	}

	if (data != 1) {
		pr_err("[FACTORY] %s: data was wrong\n", __func__);
		return -EINVAL;
	}

	if (pid != 0) {
		type[0] = 1;
		adsp_unicast(type, sizeof(type), MSG_SSC_CORE, 0, MSG_TYPE_DUMPSTATE);
	}

	return size;
}

static ssize_t pid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", pid);
}

static ssize_t pid_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int data = 0;

	if (kstrtoint(buf, 10, &data)) {
		pr_err("[FACTORY] %s: kstrtoint fail\n", __func__);
		return -EINVAL;
	}

	pid = data;
	pr_info("[FACTORY] %s: pid %d\n", __func__, pid);

	return size;
}

static ssize_t remove_sensor_sysfs_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int type = MSG_SENSOR_MAX;
	struct adsp_data *data = dev_get_drvdata(dev);

	if (kstrtouint(buf, 10, &type)) {
		pr_err("[FACTORY] %s: kstrtouint fail\n", __func__);
		return -EINVAL;
	}

	if (type > PHYSICAL_SENSOR_SYSFS) {
		pr_err("[FACTORY] %s: Invalid type %u\n", __func__, type);
		return size;
	}

	pr_info("[FACTORY] %s: type = %u\n", __func__, type);

	mutex_lock(&data->remove_sysfs_mutex);
	adsp_factory_unregister(type);
	mutex_unlock(&data->remove_sysfs_mutex);

	return size;
}

static unsigned int system_rev __read_mostly;

static int __init sec_hw_rev_setup(char *p)
{
	int ret;

	ret = kstrtouint(p, 0, &system_rev);
	if (unlikely(ret < 0)) {
		pr_warn("androidboot.revision is malformed (%s)\n", p);
		return -EINVAL;
	}

	pr_info("androidboot.revision %x\n", system_rev);

	return 0;
}
early_param("androidboot.revision", sec_hw_rev_setup);

static unsigned int sec_hw_rev(void)
{
	return system_rev;
}

int ssc_system_rev_test;
static ssize_t ssc_hw_rev_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("[FACTORY] ssc_rev:%d\n", sec_hw_rev());

	if (!ssc_system_rev_test)
		return snprintf(buf, PAGE_SIZE, "%d\n", sec_hw_rev());
	else
		return snprintf(buf, PAGE_SIZE, "%d\n", ssc_system_rev_test);
}

static ssize_t ssc_hw_rev_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	if (kstrtoint(buf, 10, &ssc_system_rev_test)) {
		pr_err("[FACTORY] %s: kstrtoint fail\n", __func__);
		return -EINVAL;
	}

	pr_info("[FACTORY] system_rev_ssc %d\n", ssc_system_rev_test);

	return size;
}
#ifdef CONFIG_SUPPORT_SSC_MODE
static int ssc_mode;
static ssize_t ssc_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("[FACTORY] ssc_mode:%d\n", ssc_mode);

	return snprintf(buf, PAGE_SIZE, "%d\n", ssc_mode);
}

static ssize_t ssc_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	if (kstrtoint(buf, 10, &ssc_mode)) {
		pr_err("[FACTORY] %s: kstrtoint fail\n", __func__);
		return -EINVAL;
	}

	pr_info("[FACTORY] ssc_mode %d\n", ssc_mode);

	return size;
}
#endif
void ssr_reason_call_back(char reason[], int len)
{
	if (len <= 0) {
		pr_info("[FACTORY] ssr %d\n", len);
		return;
	}
	memset(panic_msg, 0, SSR_REASON_LEN);
	strlcpy(panic_msg, reason, min(len, (int)(SSR_REASON_LEN - 1)));

	pr_info("[FACTORY] ssr %s\n", panic_msg);
}

static ssize_t ssr_msg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#ifdef CONFIG_SEC_FACTORY
	struct adsp_data *data = dev_get_drvdata(dev);
	int32_t raw_data_acc[3] = {0, };
	int32_t raw_data_mag[3] = {0, };
	int ret_acc = 0;
	int ret_mag = 0;

	mutex_lock(&data->accel_factory_mutex);
	ret_acc = get_accel_raw_data(raw_data_acc);
	mutex_unlock(&data->accel_factory_mutex);

	ret_mag = get_mag_raw_data(raw_data_mag);

	pr_info("[FACTORY] %s: accel(%d, %d, %d), mag(%d, %d, %d)\n", __func__,
		raw_data_acc[0], raw_data_acc[1], raw_data_acc[2],
		raw_data_mag[0], raw_data_mag[1], raw_data_mag[2]);

	if (ret_acc == -1 && ret_mag == -1) {
		pr_err("[FACTORY] %s: SLPI stuck, check hal log\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s\n", SLPI_STUCK);
	} else {
		return snprintf(buf, PAGE_SIZE, "%s\n", SLPI_PASS);
	}
#else
	return snprintf(buf, PAGE_SIZE, "%s\n", panic_msg);
#endif
}

static ssize_t ssr_reset_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int32_t type[1];

	type[0] = 0xff;

	adsp_unicast(type, sizeof(type), MSG_SSC_CORE, 0, MSG_TYPE_DUMPSTATE);
	pr_info("[FACTORY] %s\n", __func__);

	return snprintf(buf, PAGE_SIZE, "%s\n", "Success");
}

#ifdef CONFIG_SUPPORT_DEVICE_MODE
int sns_device_mode_notify(struct notifier_block *nb,
	unsigned long flip_state, void *v)
{
	int state = (int)flip_state;

	pr_info("[FACTORY] %s - device mode %d", __func__, state);
	if(state == 0)
		adsp_unicast(NULL, 0, MSG_SSC_CORE, 0, MSG_TYPE_FACTORY_ENABLE);
	else
		adsp_unicast(NULL, 0, MSG_SSC_CORE, 0, MSG_TYPE_FACTORY_DISABLE);

	return 0;
}

static struct notifier_block sns_device_mode_notifier = {
	.notifier_call = sns_device_mode_notify,
	.priority = 1,
};
#endif

static void print_sensor_dump(struct adsp_data *data, int sensor)
{
	int i = 0;

	switch (sensor) {
	case MSG_ACCEL:
		for (i = 0; i < 8; i++) {
			pr_info("[FACTORY] %s - %d: %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
				__func__, sensor,
				data->msg_buf[sensor][i * 16 + 0],
				data->msg_buf[sensor][i * 16 + 1],
				data->msg_buf[sensor][i * 16 + 2],
				data->msg_buf[sensor][i * 16 + 3],
				data->msg_buf[sensor][i * 16 + 4],
				data->msg_buf[sensor][i * 16 + 5],
				data->msg_buf[sensor][i * 16 + 6],
				data->msg_buf[sensor][i * 16 + 7],
				data->msg_buf[sensor][i * 16 + 8],
				data->msg_buf[sensor][i * 16 + 9],
				data->msg_buf[sensor][i * 16 + 10],
				data->msg_buf[sensor][i * 16 + 11],
				data->msg_buf[sensor][i * 16 + 12],
				data->msg_buf[sensor][i * 16 + 13],
				data->msg_buf[sensor][i * 16 + 14],
				data->msg_buf[sensor][i * 16 + 15]);
		}
		break;
	case MSG_MAG:
		pr_info("[FACTORY] %s - %d: WIA1/2: 0x%02x/0x%02x, ST1/2: 0x%02x/0x%02x, DATA: 0x%02x/0x%02x/0x%02x/0x%02x/0x%02x/0x%02x, CNTL1/2/3: 0x%02x/0x%02x/0x%02x\n",
			__func__, sensor,
			data->msg_buf[sensor][0], data->msg_buf[sensor][1],
			data->msg_buf[sensor][2], data->msg_buf[sensor][9],
			data->msg_buf[sensor][3], data->msg_buf[sensor][4],
			data->msg_buf[sensor][5], data->msg_buf[sensor][6],
			data->msg_buf[sensor][7], data->msg_buf[sensor][8],
			data->msg_buf[sensor][10], data->msg_buf[sensor][11],
			data->msg_buf[sensor][12]);
		break;
	default:
		break;
	}
}

void sensor_dump_work_func(struct work_struct *work)
{
	struct sdump_data *sensor_dump_data = container_of((struct work_struct *)work,
		struct sdump_data, work_sdump);
	struct adsp_data *data = sensor_dump_data->dev_data;

	int sensor_type[SENSOR_DUMP_CNT] = { MSG_ACCEL, MSG_MAG, MSG_LIGHT };

	int i, cnt;

	for (i = 0; i < SENSOR_DUMP_CNT; i++) {
		if (!data->sysfs_created[sensor_type[i]]) {
			pr_info("[FACTORY] %s: %d was not probed\n",
				__func__, sensor_type[i]);
			continue;
		}
		pr_info("[FACTORY] %s: %d\n", __func__, sensor_type[i]);
		cnt = 0;
		adsp_unicast(NULL, 0, sensor_type[i], 0, MSG_TYPE_GET_DHR_INFO);
		while (!(data->ready_flag[MSG_TYPE_GET_DHR_INFO] & 1 << sensor_type[i]) &&
			cnt++ < TIMEOUT_CNT)
			msleep(10);

		data->ready_flag[MSG_TYPE_GET_DHR_INFO] &= ~(1 << sensor_type[i]);

		if (cnt >= TIMEOUT_CNT) {
			pr_err("[FACTORY] %s: %d Timeout!!!\n",
				__func__, sensor_type[i]);
		} else {
			print_sensor_dump(data, sensor_type[i]);
			msleep(200);
		}
	}
}

static ssize_t sensor_dump_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);

	pdata_sdump->dev_data = data;
	queue_work(pdata_sdump->sdump_wq, &pdata_sdump->work_sdump);

	return snprintf(buf, PAGE_SIZE, "%s\n", SENSOR_DUMP_DONE);
}


static ssize_t ar_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int32_t msg_buf[2] = {OPTION_TYPE_SSC_AUTO_ROTATION_MODE, 0};

	msg_buf[1] = buf[0] - 48;
	pr_info("[FACTORY]%s: ar_mode:%d\n", __func__, msg_buf[1]);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		MSG_SSC_CORE, 0, MSG_TYPE_OPTION_DEFINE);

	return size;
}

static int sbm_init;
static ssize_t sbm_init_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("[FACTORY] %s sbm_init_show:%d\n", __func__, sbm_init);

	return snprintf(buf, PAGE_SIZE, "%d\n", sbm_init);
}

static ssize_t sbm_init_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int32_t msg_buf[2] = {OPTION_TYPE_SSC_SBM_INIT, 0};

	if (kstrtoint(buf, 10, &sbm_init)) {
		pr_err("[FACTORY] %s: kstrtoint fail\n", __func__);
		return -EINVAL;
	}

	if (sbm_init) {
		msg_buf[1] = sbm_init;
		pr_info("[FACTORY] %s sbm_init_store %d\n", __func__, sbm_init);
		adsp_unicast(msg_buf, sizeof(msg_buf),
			MSG_SSC_CORE, 0, MSG_TYPE_OPTION_DEFINE);
	}

	return size;
}

static DEVICE_ATTR(dumpstate, 0440, dumpstate_show, NULL);
static DEVICE_ATTR(operation_mode, 0664,
	operation_mode_show, operation_mode_store);
static DEVICE_ATTR(mode, 0660, mode_show, mode_store);
static DEVICE_ATTR(ssc_pid, 0660, pid_show, pid_store);
static DEVICE_ATTR(remove_sysfs, 0220, NULL, remove_sensor_sysfs_store);
static DEVICE_ATTR(ssc_hw_rev, 0664, ssc_hw_rev_show, ssc_hw_rev_store);
static DEVICE_ATTR(ssr_msg, 0440, ssr_msg_show, NULL);
static DEVICE_ATTR(ssr_reset, 0440, ssr_reset_show, NULL);
#ifdef CONFIG_SUPPORT_SSC_MODE
static DEVICE_ATTR(ssc_mode, 0664, ssc_mode_show, ssc_mode_store);
#endif
static DEVICE_ATTR(sensor_dump, 0444, sensor_dump_show, NULL);
static DEVICE_ATTR(sbm_init, 0660, sbm_init_show, sbm_init_store);
static DEVICE_ATTR(ar_mode, 0220, NULL, ar_mode_store);

static struct device_attribute *core_attrs[] = {
	&dev_attr_dumpstate,
	&dev_attr_operation_mode,
	&dev_attr_mode,
	&dev_attr_ssc_pid,
	&dev_attr_remove_sysfs,
	&dev_attr_ssc_hw_rev,
	&dev_attr_ssr_msg,
	&dev_attr_ssr_reset,
	&dev_attr_sensor_dump,
#ifdef CONFIG_SUPPORT_SSC_MODE
	&dev_attr_ssc_mode,
#endif
	&dev_attr_sbm_init,
	&dev_attr_ar_mode,
	NULL,
};

#ifdef CONFIG_SUPPORT_SSC_MODE
static int ssc_core_probe(struct platform_device *pdev)
{
#ifdef CONFIG_SUPPORT_SSC_MODE_FOR_MAG
	struct device *dev = &pdev->dev;

#ifdef CONFIG_OF
	if (dev->of_node) {
		struct device_node *np = dev->of_node;
		int check_mst_gpio, check_nfc_gpio;
		int value_mst = 0, value_nfc = 0;

		check_mst_gpio = of_get_named_gpio_flags(np, "ssc_core,mst_gpio", 0, NULL);
		if(check_mst_gpio >= 0)
			value_mst = gpio_get_value(check_mst_gpio);

		if (value_mst == 1) {
			ssc_mode = ANT_NFC_MST;
			return 0;
		}

		check_nfc_gpio = of_get_named_gpio_flags(np, "ssc_core,nfc_gpio", 0, NULL);
		if(check_nfc_gpio >= 0)
			value_nfc = gpio_get_value(check_nfc_gpio);

		if (value_nfc == 1) {
			ssc_mode = ANT_NFC_ONLY;
			return 0;
		}
	}
#endif
	ssc_mode = ANT_DUMMY;
#endif
	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id ssc_core_dt_ids[] = {
	{ .compatible = "ssc_core" },
	{ },
};
MODULE_DEVICE_TABLE(of, ssc_core_dt_ids);
#endif /* CONFIG_OF */

static struct platform_driver ssc_core_driver = {
	.probe		= ssc_core_probe,
	.driver		= {
		.name	= "ssc_core",
		.owner	= THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table	= ssc_core_dt_ids,
#endif /* CONFIG_OF */
	}
};
#endif

static int __init core_factory_init(void)
{
	adsp_factory_register(MSG_SSC_CORE, core_attrs);
#ifdef CONFIG_SUPPORT_SSC_MODE
	platform_driver_register(&ssc_core_driver);
#endif
	pdata_sdump = kzalloc(sizeof(*pdata_sdump), GFP_KERNEL);
	if (pdata_sdump == NULL)
		return -ENOMEM;

	pdata_sdump->sdump_wq = create_singlethread_workqueue("sdump_wq");
	if (pdata_sdump->sdump_wq == NULL) {
		pr_err("[FACTORY]: %s - could not create sdump wq\n", __func__);
		kfree(pdata_sdump);
		return -ENOMEM;
	}
	INIT_WORK(&pdata_sdump->work_sdump, sensor_dump_work_func);
	pr_info("[FACTORY] %s\n", __func__);

	return 0;
}

static void __exit core_factory_exit(void)
{
	adsp_factory_unregister(MSG_SSC_CORE);
#ifdef CONFIG_SUPPORT_SSC_MODE
	platform_driver_unregister(&ssc_core_driver);
#endif
	if (pdata_sdump->sdump_wq)
		destroy_workqueue(pdata_sdump->sdump_wq);
	if (pdata_sdump)
		kfree(pdata_sdump);

	pr_info("[FACTORY] %s\n", __func__);
}
module_init(core_factory_init);
module_exit(core_factory_exit);
