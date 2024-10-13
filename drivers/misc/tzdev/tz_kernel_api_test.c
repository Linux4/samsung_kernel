/*
 * Copyright (C) 2012-2016 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include "sysdep.h"
#include "tzdev.h"
#include "tz_cdev.h"
#include "tz_common.h"
#include "tz_kernel_api.h"
#include "tz_kernel_api_test.h"

MODULE_AUTHOR("Oleksandr Targunakov <o.targunakov@samsung.com>");
MODULE_DESCRIPTION("Trustzone Kernel API test driver");
MODULE_LICENSE("GPL");

#define TZ_KAPI_DEVICE_NAME "tz_kapi_test"

/* Timeout in seconds */
#define IWNOTIFY_TIMEOUT	5

enum {
	KAPI_SIMPLE_SEND_RECV_TEST,
	KAPI_IWSHMEM_TEST,
	KAPI_MULTIPLE_SEND_RECV_TEST,
	KAPI_MULTIPLE_SEND_RECV_IWSHMEM_TEST,
	KAPI_IWSHMEM_STRESS_TEST,
};

struct test_msg {
	uint32_t test_num;
	char str[128];
	uint32_t iwshmem_id;
	uint32_t result;
} __packed;

static struct tz_uuid uuid = {
	.time_low = 0xc8c7fc92,
	.time_mid = 0x7496,
	.time_hi_and_version = 0x4d61,
	.clock_seq_and_node = {0x94, 0xad, 0x90, 0x9e, 0x49, 0x0d, 0xdc, 0x1f}
};

static DECLARE_COMPLETION(kcmd_event);

static int __kcmd_notifier(struct notifier_block *nb, unsigned long action, void *data)
{
	tzdev_print(0, "Complete kcmd_event\n");
	complete(&kcmd_event);
	return 0;
}

static struct notifier_block kcmd_notifier = {
	.notifier_call	= __kcmd_notifier,
};

static int kapi_send_recv(int client_id, const void *msg_in, size_t size_in,
		void *msg_out, size_t size_out)
{
	int ret;

	ret = tzdev_kapi_send(client_id, msg_in, size_in);
	if (ret < 0) {
		tzdev_print(0, "tzdev_kapi_send failed - %d\n", ret);
		return ret;
	}

	ret = wait_for_completion_interruptible_timeout(&kcmd_event,
			IWNOTIFY_TIMEOUT * HZ);
	if (ret <= 0) {
		tzdev_print(0, "Wait for message timed out or interrupted, ret = %d\n",
				ret);
		if (ret == 0)
			return -EBUSY;

		return ret;
	}

	ret = tzdev_kapi_recv(client_id, msg_out, size_out);
	if (ret < 0) {
		tzdev_print(0, "tzdev_kapi_recv failed - %d\n", ret);
		return ret;
	}

	return ret;
}

static int kapi_simple_send_recv_test(unsigned int iterations)
{
	int client_id;
	int ret, test_return = 0;
	struct test_msg msg_in, msg_out;
	const char *str = "NWd: Hello from tz_kernel_api_test";
	const char expected_str[] = "SWd: Hello from kapi_simple_send_recv_test";

	tzdev_print(0, "Running kapi_simple_send_recv_test\n");

	init_completion(&kcmd_event);

	tzdev_print(0, "tz_iwnotify_chain_register()\n");
	ret = tz_iwnotify_chain_register(TZ_IWNOTIFY_OEM_NOTIFICATION_FLAG_16, &kcmd_notifier);
	if (ret < 0) {
		tzdev_print(0, "tz_iwnotify_chain_register failed. ret = %d\n", ret);
		return ret;
	}

	tzdev_print(0, "tzdev_kapi_open()\n");
	client_id = tzdev_kapi_open(&uuid);
	if (client_id < 0) {
		test_return = client_id;
		tzdev_print(0, "  tzdev_kapi_open failed - %d\n", test_return);
		goto out_iwnotify;
	}
	tzdev_print(0, "  client_id = %d\n", client_id);

	tzdev_print(0, "Calling kapi_send_recv %u time(s)\n", iterations);
	msg_in.test_num = KAPI_SIMPLE_SEND_RECV_TEST;
	strcpy(msg_in.str, str);
	while (iterations--) {
		memset(&msg_out, 0, sizeof(msg_out));
		ret = kapi_send_recv(client_id, &msg_in, sizeof(msg_in),
				&msg_out, sizeof(msg_out));
		if (ret < 0) {
			test_return = ret;
			tzdev_print(0, "kapi_send_recv failed - %d\n", ret);
			break;
		}

		if (msg_out.result) {
			tzdev_print(0, "Bad msg.result from TA - %u\n", msg_out.result);
			test_return = -EPERM;
			break;
		}

		if (strncmp(expected_str, msg_out.str, sizeof(expected_str))) {
			msg_out.str[sizeof(expected_str)] = '\0';
			tzdev_print(0, "Received msg.str '%s' mismatch expected one '%s'\n",
					msg_out.str, expected_str);
			test_return = -EPERM;
			break;
		}
	}

	tzdev_print(0, "tzdev_kapi_close()\n");
	ret = tzdev_kapi_close(client_id);
	if (ret < 0) {
		test_return = ret;
		tzdev_print(0, "tzdev_kapi_close failed - %d\n", ret);
	}

out_iwnotify:
	tzdev_print(0, "tz_iwnotify_chain_unregister()\n");
	ret = tz_iwnotify_chain_unregister(TZ_IWNOTIFY_OEM_NOTIFICATION_FLAG_16, &kcmd_notifier);
	if (ret < 0) {
		test_return = ret;
		tzdev_print(0, "tz_iwnotify_chain_unregister failed - %d\n", ret);
	}

	return test_return;
}

static int kapi_iwshmem_test(unsigned int iterations)
{
	int client_id;
	int ret, test_return = 0;
	struct test_msg msg_in, msg_out;
	char *shmem;
	int shmem_id;
	struct page *shmem_page;
	const char expected_str[] = "SWd: Hello from kapi_iwshmem_test";
	char str[sizeof(expected_str) + 1];

	tzdev_print(0, "Running kapi_iwshmem_test\n");

	init_completion(&kcmd_event);

	shmem_page = alloc_page(GFP_KERNEL);
	if (!shmem_page) {
		tzdev_print(0, "alloc_page failed\n");
		return -ENOMEM;
	}

	shmem = page_address(shmem_page);
	memset(shmem, 0, PAGE_SIZE);

	tzdev_print(0, "tzdev_mem_register() shmem=0x%lx\n", (unsigned long)shmem);
	ret = tzdev_mem_register(shmem, PAGE_SIZE, 1);
	if (ret < 0) {
		__free_page(shmem_page);
		tzdev_print(0, "tzdev_mem_register failed - %d\n", ret);
		return ret;
	}

	shmem_id = ret;
	tzdev_print(0, "  shmem_id = %d\n", shmem_id);

	tzdev_print(0, "tz_iwnotify_chain_register()\n");
	ret = tz_iwnotify_chain_register(TZ_IWNOTIFY_OEM_NOTIFICATION_FLAG_16, &kcmd_notifier);
	if (ret < 0) {
		test_return = ret;
		tzdev_print(0, "tz_iwnotify_chain_register failed. ret = %d\n", ret);
		goto out_mem_register;
	}

	tzdev_print(0, "tzdev_kapi_open()\n");
	client_id = tzdev_kapi_open(&uuid);
	if (client_id < 0) {
		test_return = client_id;
		tzdev_print(0, "  tzdev_kapi_open failed - %d\n", test_return);
		goto out_iwnotify;
	}
	tzdev_print(0, "  client_id = %d\n", client_id);

	tzdev_print(0, "tzdev_kapi_mem_grant()\n");
	ret = tzdev_kapi_mem_grant(client_id, shmem_id);
	if (ret) {
		test_return = ret;
		tzdev_print(0, "  tzdev_kapi_mem_grant failed - %d\n", ret);
		goto out_open;
	}

	tzdev_print(0, "Calling kapi_send_recv %u time(s)\n", iterations);
	msg_in.test_num = KAPI_IWSHMEM_TEST;
	msg_in.iwshmem_id = shmem_id;
	while (iterations--) {
		strcpy(shmem, "NWd: Hello from kapi_iwshmem_test");
		memset(&msg_out, 0, sizeof(msg_out));

		ret = kapi_send_recv(client_id, &msg_in, sizeof(msg_in),
				&msg_out, sizeof(msg_out));
		if (ret < 0) {
			test_return = ret;
			tzdev_print(0, "kapi_send_recv failed - %d\n", ret);
			break;
		}

		if (msg_out.result) {
			tzdev_print(0, "Bad msg.result from TA - %d\n", msg_out.result);
			test_return = -EPERM;
			break;
		}

		strncpy(str, shmem, sizeof(expected_str));

		if (strncmp(expected_str, str, sizeof(expected_str))) {
			str[sizeof(expected_str)] = '\0';
			tzdev_print(0, "Received shmem string '%s' mismatch expected one '%s'\n",
					str, expected_str);
			test_return = -EPERM;
			break;
		}
	}

	tzdev_print(0, "tzdev_kapi_mem_revoke()\n");
	ret = tzdev_kapi_mem_revoke(client_id, shmem_id);
	if (ret) {
		test_return = ret;
		tzdev_print(0, "  tzdev_kapi_mem_revoke failed - %d\n", ret);
	}

out_open:
	tzdev_print(0, "tzdev_kapi_close()\n");
	ret = tzdev_kapi_close(client_id);
	if (ret < 0) {
		test_return = ret;
		tzdev_print(0, "tzdev_kapi_close failed - %d\n", ret);
	}

out_iwnotify:
	tzdev_print(0, "tz_iwnotify_chain_unregister()\n");
	ret = tz_iwnotify_chain_unregister(TZ_IWNOTIFY_OEM_NOTIFICATION_FLAG_16, &kcmd_notifier);
	if (ret < 0) {
		test_return = ret;
		tzdev_print(0, "tz_iwnotify_chain_unregister failed - %d\n", ret);
	}

out_mem_register:
	tzdev_print(0, "tzdev_mem_release()\n");
	ret = tzdev_mem_release(shmem_id);
	if (ret < 0) {
		test_return = ret;
		tzdev_print(0, "tzdev_mem_release failed - %d\n", ret);
	}
	__free_page(shmem_page);

	return test_return;
}

#define IWSHMEM_STRESS_TEST_PAGES_NUMBER	256

static int kapi_iwshmem_stress_test(unsigned int iterations)
{
	int client_id;
	int ret, test_return = 0;
	struct test_msg msg_in, msg_out;
	const char expected_str[] = "SWd: Hello from kapi_iwshmem_test";
	char str[sizeof(expected_str) + 1];
	int *mem_id = NULL;
	struct page **pages = NULL;
	unsigned int i;

	tzdev_print(0, "Running kapi_iwshmem_stress_test\n");

	init_completion(&kcmd_event);

	mem_id = kcalloc(IWSHMEM_STRESS_TEST_PAGES_NUMBER, sizeof(int), GFP_KERNEL);
	if (!mem_id) {
		tzdev_print(0, "kcalloc ids failed\n");
		return -ENOMEM;
	}

	pages = kcalloc(IWSHMEM_STRESS_TEST_PAGES_NUMBER, sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		test_return = -ENOMEM;
		tzdev_print(0, "kcalloc pages failed\n");
		goto out_mem_id;
	}

	tzdev_print(0, "Allocating %d pages\n", IWSHMEM_STRESS_TEST_PAGES_NUMBER);
	for (i = 0; i < IWSHMEM_STRESS_TEST_PAGES_NUMBER; i++) {
		pages[i] = alloc_page(GFP_KERNEL);
		if (!pages[i]) {
			test_return = -ENOMEM;
			tzdev_print(0, "alloc_page failed\n");
			goto out_pages;
		}

		mem_id[i] = tzdev_mem_register(page_address(pages[i]), PAGE_SIZE, 1);
		if (mem_id[i] < 0) {
			test_return = mem_id[i];
			tzdev_print(0, "tzdev_mem_register failed - %d\n", mem_id[i]);
			goto out_pages;
		}
	}

	tzdev_print(0, "tz_iwnotify_chain_register()\n");
	ret = tz_iwnotify_chain_register(TZ_IWNOTIFY_OEM_NOTIFICATION_FLAG_16, &kcmd_notifier);
	if (ret < 0) {
		test_return = ret;
		tzdev_print(0, "tz_iwnotify_chain_register failed. ret = %d\n", ret);
		goto out_pages;
	}

	tzdev_print(0, "tzdev_kapi_open()\n");
	client_id = tzdev_kapi_open(&uuid);
	if (client_id < 0) {
		test_return = client_id;
		tzdev_print(0, "  tzdev_kapi_open failed - %d\n", test_return);
		goto out_iwnotify;
	}
	tzdev_print(0, "  client_id = %d\n", client_id);


	tzdev_print(0, "Calling iwshmem stress test %u time(s)\n", iterations);
	while (iterations--) {
		tzdev_print(0, "Grant access for each iwshmem\n");
		for (i = 0; i < IWSHMEM_STRESS_TEST_PAGES_NUMBER; i++) {
			ret = tzdev_kapi_mem_grant(client_id, mem_id[i]);
			if (ret) {
				test_return = ret;
				tzdev_print(0, "  tzdev_kapi_mem_grant failed - %d\n", ret);
				goto out_mem_grant;
			}
		}

		tzdev_print(0, "Run simple test for each iwshmem\n");
		for (i = 0; i < IWSHMEM_STRESS_TEST_PAGES_NUMBER; i++) {
			msg_in.test_num = KAPI_IWSHMEM_TEST;
			msg_in.iwshmem_id = mem_id[i];
			strcpy(page_address(pages[i]), "NWd: Hello from kapi_iwshmem_test");
			memset(&msg_out, 0, sizeof(msg_out));


			ret = kapi_send_recv(client_id, &msg_in, sizeof(msg_in),
					&msg_out, sizeof(msg_out));
			if (ret < 0) {
				test_return = ret;
				tzdev_print(0, "kapi_send_recv failed - %d\n", ret);
				goto out_mem_grant;
			}

			if (msg_out.result) {
				tzdev_print(0, "Bad msg.result from TA - %d\n", msg_out.result);
				test_return = -EPERM;
				goto out_mem_grant;
			}

			strncpy(str, page_address(pages[i]), sizeof(expected_str));

			if (strncmp(expected_str, str, sizeof(expected_str))) {
				str[sizeof(expected_str)] = '\0';
				tzdev_print(0, "Received shmem string '%s' mismatch expected one '%s'\n",
						str, expected_str);
				test_return = -EPERM;
				goto out_mem_grant;
			}
		}

		tzdev_print(0, "Revoke access for each iwshmem\n");
		for (i = 0; i < IWSHMEM_STRESS_TEST_PAGES_NUMBER; i++)
			tzdev_kapi_mem_revoke(client_id, mem_id[i]);
	}


out_mem_grant:
	for (i = 0; i < IWSHMEM_STRESS_TEST_PAGES_NUMBER; i++)
		tzdev_kapi_mem_revoke(client_id, mem_id[i]);

	tzdev_print(0, "tzdev_kapi_close()\n");
	ret = tzdev_kapi_close(client_id);
	if (ret < 0) {
		test_return = ret;
		tzdev_print(0, "tzdev_kapi_close failed - %d\n", ret);
	}

out_iwnotify:
	tzdev_print(0, "tz_iwnotify_chain_unregister()\n");
	ret = tz_iwnotify_chain_unregister(TZ_IWNOTIFY_OEM_NOTIFICATION_FLAG_16, &kcmd_notifier);
	if (ret < 0) {
		test_return = ret;
		tzdev_print(0, "tz_iwnotify_chain_unregister failed - %d\n", ret);
	}

out_pages:
	for (i = 0; i < IWSHMEM_STRESS_TEST_PAGES_NUMBER; i++) {
		if (!pages[i])
			break;
		__free_page(pages[i]);

		if (mem_id[i] < 0)
			break;
		tzdev_mem_release(mem_id[i]);
	}

	kfree(pages);

out_mem_id:
	kfree(mem_id);

	return test_return;
}

static long tz_kapi_test_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	switch (cmd) {
	case TZ_KAPI_TEST_RUN: {
		int res;
		int test_num = arg;

		switch (test_num) {
		case KAPI_SIMPLE_SEND_RECV_TEST:
			res = kapi_simple_send_recv_test(1);
			break;
		case KAPI_IWSHMEM_TEST:
			res = kapi_iwshmem_test(1);
			break;
		case KAPI_MULTIPLE_SEND_RECV_TEST:
			res = kapi_simple_send_recv_test(1000);
			break;
		case KAPI_MULTIPLE_SEND_RECV_IWSHMEM_TEST:
			res = kapi_iwshmem_test(1000);
			break;
		case KAPI_IWSHMEM_STRESS_TEST:
			res = kapi_iwshmem_stress_test(300);
			break;
		default:
			tzdev_print(0, "Invalid test number: %d\n", test_num);
			return -EINVAL;
		}

		return res;
	}
	default:
		return -ENOTTY;
	}
}

static int tz_kapi_test_open(struct inode *inode, struct file *filp)
{
	if (!tzdev_is_up())
		return -EPERM;

	return 0;
}

static const struct file_operations tz_kapi_test_fops = {
	.owner = THIS_MODULE,
	.open = tz_kapi_test_open,
	.unlocked_ioctl = tz_kapi_test_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tz_kapi_test_ioctl,
#endif /* CONFIG_COMPAT */
};

static struct tz_cdev tz_kapi_test_cdev = {
	.name = TZ_KAPI_DEVICE_NAME,
	.fops = &tz_kapi_test_fops,
	.owner = THIS_MODULE,
};

static int __init tz_kapi_test_init(void)
{
	return tz_cdev_register(&tz_kapi_test_cdev);
}

static void __exit tz_kapi_test_exit(void)
{
	tz_cdev_unregister(&tz_kapi_test_cdev);
}

module_init(tz_kapi_test_init);
module_exit(tz_kapi_test_exit);
