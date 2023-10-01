/*
 * camellia_test.c - Samsung Camellia test for the Exynos
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/types.h>
#include <linux/slab.h>
#include "strong_common.h"
#include "strong_mailbox_ree.h"
#include "strong_mailbox_common.h"

#define CAMEL_MAX_USER_PATH_LEN		(128)
#define TEST_RX_BUFFER_LEN		(4 * 1024)
#define TEST_TX_BUFFER_LEN              (1 * 1024)

static unsigned int tx_data[TEST_TX_BUFFER_LEN / WORD_SIZE];

struct camel_test_device {
	struct device *dev;
	struct miscdevice misc_device;
};

typedef struct {
	unsigned int arg1;
	unsigned int arg2;
	unsigned int reserved1;
	unsigned int reserved2;
} mailbox_ioctl_t;

void print_rx_data(uint32_t *rx_data)
{
	uint32_t i;

	for (i = 0; i < TEST_RX_BUFFER_LEN / WORD_SIZE; i += 8)
		pr_info("[STRONG] %X %X %X %X %X %X %X %X\n",
			rx_data[i+0], rx_data[i+1], rx_data[i+2], rx_data[i+3],
			rx_data[i+4], rx_data[i+5], rx_data[i+6], rx_data[i+7]);
}

void *allocate_buffer(ssize_t len)
{
	void *buf;

	pr_info("%s: start\n", __func__);
	buf = kzalloc(TEST_RX_BUFFER_LEN, GFP_KERNEL);
	pr_info("%s: end\n", __func__);

	return buf;
}

int receive_message(void *buf, unsigned int len, unsigned int status)
{
	pr_info("%s: start\n", __func__);
	print_rx_data((uint32_t *)buf);
	pr_info("%s: len = 0x%X\n", __func__, len);
	kfree(buf);
	pr_info("%s: end\n", __func__);

	return 0;
}

static uint32_t mailbox_send_test(void)
{
	uint32_t ret = 0;
	uint32_t i;
	uint32_t tx_out;
	static uint32_t tc;

	pr_info("[STRONG_MB][camellia_test] %s: start\n", __func__);

	for (i = 0; i < TEST_TX_BUFFER_LEN / WORD_SIZE;  i++)
		tx_data[i] = 0xA0000000 + (0x1000 * tc) + i;

	ret = mailbox_send((uint8_t *)tx_data, sizeof(tx_data), &tx_out);
	pr_info("[STRONG_MB][camellia_test] tx_out: 0x%X\n", tx_out);
	if (ret) {
		pr_info("[STRONG_MB][camellia_test] %s: end [ret: 0x%X]\n",
				__func__, ret);
	}
	tc++;

	return ret;
}

static int camellia_test_open(struct inode *inode, struct file *file)
{
	struct miscdevice *misc = file->private_data;
	struct camel_test_device *cameldev = container_of(misc,
			struct camel_test_device, misc_device);

	file->private_data = cameldev;

	pr_info("[STRONG_MB][camellia_test] driver open is done\n");

	return 0;
}

static long camellia_test_ioctl(struct file *filp, unsigned int cmd, unsigned long __arg)
{
	uint32_t ret = 0;
	uint32_t tx_out;
	mailbox_ioctl_t data;
	void __user *arg = (void __user *)__arg;

	pr_info("[STRONG_MB][camellia_test] ioctl start\n");

	ret = copy_from_user(&data, arg, sizeof(mailbox_ioctl_t));
	pr_info("[STRONG_MB][camellia_test] test_mode: %d\n", data.arg1);
	pr_info("[STRONG_MB][camellia_test] data.arg2: %d\n", data.arg2);

	switch (data.arg1) {
	case 1:
		pr_info("[STRONG_MB][camellia_test] mailbox_open\n");
		mailbox_open();
		break;
	case 2:
		pr_info("[STRONG_MB][camellia_test] mailbox_close\n");
		mailbox_close();
		break;
	case 3:
		pr_info("[STRONG_MB][camellia_test] mailbox_send_test\n");
		ret = mailbox_send_test();
		pr_info("[STRONG_MB][camellia_test] mailbox_send_test [ret: 0x%X]\n", ret);
		break;
	case 4:
		pr_info("[STRONG_MB][camellia_test] notify ap sleep\n");
		ret = exynos_ssp_notify_ap_sleep();
		pr_info("[STRONG_MB][camellia_test] notify ap sleep [ret: 0x%X]\n", ret);
		break;
	case 5:
		pr_info("[STRONG_MB][camellia_test] notify ap wakeup\n");
		ret = exynos_ssp_notify_ap_wakeup();
		pr_info("[STRONG_MB][camellia_test] notify ap wakeup [ret: 0x%X]\n", ret);
		break;
	case 11:
		pr_info("[STRONG_MB][camellia_test] mailbox_test %d %d\n", data.arg1, data.arg2);
		ret = mailbox_send((uint8_t *)&(data.arg2), 4, &tx_out);
		pr_info("[STRONG_MB][camellia_test] mailbox_test [ret: 0x%X]\n", ret);
		break;
	default:
		pr_info("[STRONG_MB][camellia_test] Invalid test_mode\n");
		ret = -1;
		break;
	}

	pr_info("[STRONG_MB][camellia_test] ioctl done\n");

	return ret;
}

static const struct file_operations camellia_test_fops = {
	.owner          = THIS_MODULE,
	.open           = camellia_test_open,
	.unlocked_ioctl = camellia_test_ioctl,
	.compat_ioctl   = camellia_test_ioctl,
};

static int exynos_camellia_test_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct camel_test_device *cameldev = NULL;

	cameldev = kzalloc(sizeof(struct camel_test_device), GFP_KERNEL);
	if (!cameldev) {
		pr_err("[STRONG_MB][camellia_test] %s: fail to kzalloc.\n",
				__func__);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, cameldev);
	cameldev->dev = &pdev->dev;

	/* Set misc driver */
	memset((void *)&cameldev->misc_device, 0, sizeof(struct miscdevice));
	cameldev->misc_device.minor = MISC_DYNAMIC_MINOR;
	cameldev->misc_device.name = "camellia_test";
	cameldev->misc_device.fops = &camellia_test_fops;
	ret = misc_register(&cameldev->misc_device);
	if (ret) {
		pr_err("[STRONG_MB][camellia_test] %s: fail to misc_register. \
				[ret = 0x%X]\n", __func__, ret);
		return -ENOMEM;
	}

	/* Set callback APIs */
	mailbox_register_allocator(allocate_buffer);
	mailbox_register_message_receiver(receive_message);

	return 0;
}

static int exynos_camellia_test_remove(struct platform_device *pdev)
{
	return 0;
}

static int exynos_camellia_test_suspend(struct device *dev)
{
	int ret = RV_FAIL;

	ret = exynos_ssp_notify_ap_sleep();

	return ret;
}

static int exynos_camellia_test_resume(struct device *dev)
{
	int ret = RV_FAIL;

	ret = exynos_ssp_notify_ap_wakeup();

	return ret;
}

const static struct dev_pm_ops exynos_camellia_test_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(exynos_camellia_test_suspend,
			exynos_camellia_test_resume)
};

static struct platform_driver exynos_camellia_test_driver = {
	.probe		= exynos_camellia_test_probe,
	.remove		= exynos_camellia_test_remove,
	.driver		= {
		.name	= "camellia_test",
		.owner	= THIS_MODULE,
		.pm     = &exynos_camellia_test_pm_ops,
	},
};

static int __init exynos_camellia_test_init(void)
{
	int ret;

	ret = platform_driver_register(&exynos_camellia_test_driver);
	if (ret) {
		pr_err("[STRONG_MB][camellia_test] %s: fail to register driver. \
				[ret = 0x%X]\n", __func__, ret);
		return ret;
	}

	pr_info("[STRONG_MB][camellia_test] driver, (c) 2021 Samsung Electronics\n");

	return 0;
}

static void __exit exynos_camellia_test_exit(void)
{
	platform_driver_unregister(&exynos_camellia_test_driver);
}

module_init(exynos_camellia_test_init);
module_exit(exynos_camellia_test_exit);

MODULE_DESCRIPTION("EXYNOS Camellia test driver");
MODULE_AUTHOR("Jungi Lee <jungilsi.lee@samsung.com>");
MODULE_LICENSE("GPL");
