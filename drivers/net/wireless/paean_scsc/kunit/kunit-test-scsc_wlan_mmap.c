// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-mock-kernel.h"
#include "../scsc_wlan_mmap.c"


static void test_scsc_wlan_mmap_open(struct kunit *test)
{
	struct inode *inode = kunit_kzalloc(test, sizeof(struct inode), GFP_KERNEL);
	struct file *filp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, scsc_wlan_mmap_open(inode, filp));
}

static void test_scsc_wlan_mmap_release(struct kunit *test)
{
	struct inode *inode = kunit_kzalloc(test, sizeof(struct inode), GFP_KERNEL);
	struct file *filp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, scsc_wlan_mmap_release(inode, filp));
}

#define MAX_LOG_SIZE		500*1024
#define SCSC_WLAN_MAX_SIZE	4*1024*1024
static void test_scsc_wlan_mmap(struct kunit *test)
{
	struct file *filp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct vm_area_struct *vma = kunit_kzalloc(test, sizeof(struct vm_area_struct), GFP_KERNEL);
	void *buf = kunit_kzalloc(test, MAX_LOG_SIZE, GFP_KERNEL);
	size_t sz = SCSC_WLAN_MAX_SIZE;

	vma->vm_start = 0;
	vma->vm_end = 0;
	KUNIT_EXPECT_EQ(test, -EAGAIN, scsc_wlan_mmap(filp, vma));

	vma->vm_end = PAGE_SIZE + 1024;
	KUNIT_EXPECT_EQ(test, -EAGAIN, scsc_wlan_mmap(filp, vma));

	KUNIT_EXPECT_EQ(test, 0, scsc_wlan_mmap_set_buffer(buf, sz));
	KUNIT_EXPECT_EQ(test, 0, scsc_wlan_mmap(filp, vma));
}

static void test_scsc_wlan_mmap_set_buffer(struct kunit *test)
{
	void *buf = kunit_kzalloc(test, MAX_LOG_SIZE, GFP_KERNEL);
	size_t sz = SCSC_WLAN_MAX_SIZE + 1;

	KUNIT_EXPECT_EQ(test, -EIO, scsc_wlan_mmap_set_buffer(buf, sz));
}

static void test_scsc_wlan_mmap_create(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 0, scsc_wlan_mmap_create());
}

static void test_scsc_wlan_mmap_destroy(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 0, scsc_wlan_mmap_destroy());
}

/* Test fictures */
static int scsc_wlan_mmap_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s completed.", __func__);
	return 0;
}

static void scsc_wlan_mmap_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case scsc_wlan_mmap_test_cases[] = {
	KUNIT_CASE(test_scsc_wlan_mmap_open),
	KUNIT_CASE(test_scsc_wlan_mmap_release),
	KUNIT_CASE(test_scsc_wlan_mmap),
	KUNIT_CASE(test_scsc_wlan_mmap_set_buffer),
	KUNIT_CASE(test_scsc_wlan_mmap_create),
	KUNIT_CASE(test_scsc_wlan_mmap_destroy),
	{}
};

static struct kunit_suite scsc_wlan_mmap_test_suite[] = {
	{
		.name = "scsc_wlan_mmap-test",
		.test_cases = scsc_wlan_mmap_test_cases,
		.init = scsc_wlan_mmap_test_init,
		.exit = scsc_wlan_mmap_test_exit,
	}
};

kunit_test_suites(scsc_wlan_mmap_test_suite);
