// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
#include <soc/samsung/exynos/imgloader.h>
#endif

#include "pablo-kunit-test.h"
#include "icpu/firmware/pablo-icpu-imgloader.h"
#include "icpu/mem/pablo-icpu-mem.h"

/* Define the test cases. */

void *pablo_kunit_init_imgloader_desc(void *dev);
void pablo_kunit_deinit_imgloader_desc(void *desc);

struct imgloader_ops *pablo_kunit_get_imgloader_ops(void);
int pablo_kunit_update_bufinfo(void *icpubuf);

static void pablo_icpu_imgloader_init_kunit_test(struct kunit *test)
{
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	int ret;
	struct pablo_icpu_imgloader_ops icpu_img_ops = { 0, };
	struct device dev;

	ret = pablo_icpu_imgloader_init(&dev, true, &icpu_img_ops);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	pablo_icpu_imgloader_release();
#endif
}

static void pablo_icpu_imgloader_mem_setup_kunit_test(struct kunit *test)
{
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	int ret;
	struct pablo_icpu_imgloader_ops icpu_img_ops = { 0, };
	struct device dev;
	struct imgloader_ops *imgloader_ops;
	u8 *metadata;
	size_t size;
	unsigned long align = 0;
	phys_addr_t fw_phys_base;
	size_t fw_bin_size;
	size_t fw_mem_size;
	void *dst_buf;

	ret = pablo_icpu_imgloader_init(&dev, false, &icpu_img_ops);
	KUNIT_EXPECT_EQ(test, ret, 0);

	imgloader_ops = pablo_kunit_get_imgloader_ops();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, imgloader_ops);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, imgloader_ops->mem_setup);

	dst_buf = pablo_icpu_mem_alloc(ICPU_MEM_TYPE_PMEM, 1 * SZ_1M, "system", align);
	ret = pablo_kunit_update_bufinfo(dst_buf);
	KUNIT_ASSERT_EQ(test, ret, 0);

	size = 512 * 1024;
	metadata = kunit_kzalloc(test, size, 0);

	ret = imgloader_ops->mem_setup(NULL, metadata, size, &fw_phys_base, &fw_bin_size, &fw_mem_size);
	KUNIT_ASSERT_EQ(test, ret, 0);

	pablo_icpu_imgloader_release();
#endif
}

static struct kunit_case pablo_icpu_imgloader_kunit_test_cases[] = {
	KUNIT_CASE(pablo_icpu_imgloader_init_kunit_test),
	KUNIT_CASE(pablo_icpu_imgloader_mem_setup_kunit_test),
	{},
};

static int pablo_icpu_imgloader_kunit_test_init(struct kunit *test)
{
	return 0;
}

static void pablo_icpu_imgloader_kunit_test_exit(struct kunit *test)
{
}

struct kunit_suite pablo_icpu_imgloader_kunit_test_suite = {
	.name = "pablo-icpu-imgloader-kunit-test",
	.init = pablo_icpu_imgloader_kunit_test_init,
	.exit = pablo_icpu_imgloader_kunit_test_exit,
	.test_cases = pablo_icpu_imgloader_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_icpu_imgloader_kunit_test_suite);

MODULE_LICENSE("GPL");
