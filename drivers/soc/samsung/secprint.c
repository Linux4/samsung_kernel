/*
 * drivers/soc/samsung/secprint.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Exynos - Support secure drm logging
 * Author: Ritesh Kumar Solanki <r.solanki@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/kthread.h>
#include <linux/exynos_ion.h>
#include <linux/ion.h>
#include <linux/sizes.h>
#include <linux/smc.h>
#include <linux/delay.h>

#define LOG_BUFFER_SIZE			SZ_64K
#define ARRAY_SZ			512
#define ADDR(X)				(base_vaddr + ((unsigned long)(X) - phys_addr))

extern struct ion_device *ion_exynos;

/*
 * structure which is used to get information from ldfw.
 * Two queue will be maintained, when one queue is full we will
 * start using other queue
 * @counter_write:  Current counter till where logger framework has written data in this queue.
 * @counter_read:   Current counter till where kernel thread has printed data in this queue.
 * @start:          Array to hold the address of the variable to print.
 */
struct sec_print_info {
	int counter_write;
	int counter_read;
	void *start[ARRAY_SZ];
};

volatile struct sec_print_info *gsec_print_info;
char *base_vaddr;
unsigned long phys_addr;
struct ion_handle *handle;
struct ion_client *ion_client;
struct task_struct *logger_thread;

static void print_log(const char *format, int index, void *vp[])
{
	switch (index) {
	case 0:
		printk(format);
		break;
	case 1:
		printk(format, vp[0]);
		break;
	case 2:
		printk(format, vp[0], vp[1]);
		break;
	case 3:
		printk(format, vp[0], vp[1], vp[2]);
		break;
	case 4:
		printk(format, vp[0], vp[1], vp[2], vp[3]);
		break;
	case 5:
		printk(format, vp[0], vp[1], vp[2], vp[3], vp[4]);
		break;
	case 6:
		printk(format, vp[0], vp[1], vp[2], vp[3], vp[4], vp[5]);
		break;
	case 7:
		printk(format, vp[0], vp[1], vp[2], vp[3], vp[4], vp[5], vp[6]);
		break;
	case 8:
		printk(format, vp[0], vp[1], vp[2], vp[3], vp[4], vp[5], vp[6], vp[7]);
		break;
	case 9:
		printk(format, vp[0], vp[1], vp[2], vp[3], vp[4], vp[5], vp[6], vp[7], vp[8]);
		break;
	default:
		pr_err("logger support print with max 9 arguments\n");

	}
}

static void parse_format(char *ptr, char *vaddr, int *index, void *vp[])
{
	int gindex = 0;

	while (*ptr) {
		if (*ptr == '%') {
			++ptr;
			if (*ptr == 's') {
				vp[gindex] = (void *)vaddr;

				while (*vaddr++)
					;

				vaddr = PTR_ALIGN(vaddr, 8);
				gindex++;
				continue;
			} else if ((*ptr == 'c') || (*ptr == 'd') ||
					 (*ptr == 'x') || (*ptr == 'u')) {
				vp[gindex] = (void *) (*((unsigned long *)vaddr));
				vaddr = vaddr + sizeof(int);
				vaddr = PTR_ALIGN(vaddr, 8);
				gindex++;
				continue;
			} else if (*ptr == 'p') {
				vp[gindex] = (void *) (*((unsigned long *)vaddr));
				vaddr = vaddr + sizeof(void *);
				vaddr = PTR_ALIGN(vaddr, 8);
				gindex++;
				continue;
			} else if ((*ptr == 'l') && (*(ptr + 1) == 'd')) {
				ptr++;
				vp[gindex] = (void *) (*((unsigned long *)vaddr));
				vaddr = vaddr + sizeof(long int);
				vaddr = PTR_ALIGN(vaddr, 8);
				gindex++;
				continue;
			}
		}
		ptr++;
	}
	*index = gindex;
}

static int secure_logger_threadfn(void *p)
{
	char *ptr;
	void *vp[10];
	char *format;
	int gindex = 0;
	int cindex;
	char *vaddr;

	while (1) {
		if (gsec_print_info->counter_read !=
			gsec_print_info->counter_write) {
			cindex = gsec_print_info->counter_read;
			ptr = ADDR(gsec_print_info->start[cindex]);
			format = ptr;

			while (*ptr)
				ptr++;

			ptr = PTR_ALIGN(ptr, 8);
			vaddr = ptr;
			ptr = format;
			parse_format(ptr, vaddr, &gindex, vp);
			print_log(format, gindex, vp);
			gindex = 0;

			if (++gsec_print_info->counter_read == (ARRAY_SZ - 1))
				gsec_print_info->counter_read = 0;
		}
		msleep(30);
	}
	return 0;
}

static int secure_logger_init(void)
{
	char device_name[] = "ldfw_debugger";
	size_t len = LOG_BUFFER_SIZE;
	int ret = 0;

	ion_client = ion_client_create(ion_exynos, device_name);
	if (IS_ERR(ion_client)) {
		pr_err("failed to ion_client_create\n");
		return -ENOMEM;
	}

	handle = ion_alloc(ion_client, LOG_BUFFER_SIZE, 0,
			EXYNOS_ION_HEAP_EXYNOS_CONTIG_MASK,
			EXYNOS_ION_HEAP_CRYPTO_MASK);
	if (IS_ERR(handle)) {
		pr_err("failed to ion_alloc\n");
		ion_client_destroy(ion_client);
		return -ENOMEM;
	}

	base_vaddr = (char *)ion_map_kernel(ion_client, handle);
	gsec_print_info = (struct sec_print_info *)base_vaddr;
	ion_phys(ion_client, handle, &phys_addr, &len);

	ret = exynos_smc(SMC_DRM_SET_LOG_BUF, phys_addr, len, 0);
	if (ret) {
		ion_free(ion_client, handle);
		ion_client_destroy(ion_client);
		pr_err("exynos_smc call returned with err:%d\n", ret);
		return ret;
	}

	logger_thread = kthread_create(secure_logger_threadfn,
				NULL, "secure-logger");
	if (logger_thread == ERR_PTR(-ENOMEM)) {
		pr_err("failed to run logger thread\n");
		ion_unmap_kernel(ion_client, handle);
		ion_free(ion_client, handle);
		ion_client_destroy(ion_client);
		logger_thread = NULL;
		return ENOMEM;
	}

	pr_info("secure logger thread using virtual addrees:%p\n", base_vaddr);
	kthread_bind(logger_thread, 3);
	wake_up_process(logger_thread);
	return 0;
}

static void __exit secure_logger_exit(void)
{
	kthread_stop(logger_thread);
	ion_unmap_kernel(ion_client, handle);
	ion_free(ion_client, handle);
	ion_client_destroy(ion_client);
}

module_init(secure_logger_init);
module_exit(secure_logger_exit);
