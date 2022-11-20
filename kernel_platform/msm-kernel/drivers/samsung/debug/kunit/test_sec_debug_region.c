// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2020 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/kernel.h>
#include <linux/platform_device.h>

#include <linux/samsung/debug/sec_debug_region.h>

#include <kunit/test.h>

#define MOCK_DBG_REGION_RESERVED_SIZE		(2 * 1024 * 1024)

extern void kunit_dbg_region_set_reserved_size(size_t size);
extern unsigned long kunit_dbg_region_get_pool_virt(void);
extern int kunit_dbg_region_mock_probe(struct platform_device *pdev);
extern int kunit_dbg_region_mock_remove(struct platform_device *pdev);

static struct platform_device *mock_dbg_region_pdev;

static void __dbg_region_create_mock_dbg_region_pdev(void)
{
	mock_dbg_region_pdev = kzalloc(sizeof(struct platform_device),
			GFP_KERNEL);
	device_initialize(&mock_dbg_region_pdev->dev);
}

static void __dbg_region_remove_mock_dbg_region_pdev(void)
{
	kfree(mock_dbg_region_pdev);
}

static int sec_dbg_region_test_init(struct kunit *test)
{
	__dbg_region_create_mock_dbg_region_pdev();
	kunit_dbg_region_set_reserved_size(MOCK_DBG_REGION_RESERVED_SIZE);
	kunit_dbg_region_mock_probe(mock_dbg_region_pdev);

	return 0;
}

static void sec_dbg_region_test_exit(struct kunit *test)
{
	kunit_dbg_region_mock_remove(mock_dbg_region_pdev);
	__dbg_region_remove_mock_dbg_region_pdev();
}

static void test_case_0_probe_and_remove(struct kunit *test)
{
	unsigned long virt;

	virt = kunit_dbg_region_get_pool_virt();

	KUNIT_EXPECT_NE(test, virt, 0UL);
	if (!virt)
		return;
}

static void test_case_0_alloc_client(struct kunit *test)
{
	struct sec_dbg_region_client *client;
	uint32_t unique_id = 0xCAFEBEEF;

	client = sec_dbg_region_alloc(unique_id, 256);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, client);
}

static void test_case_1_alloc_client(struct kunit *test)
{
	struct sec_dbg_region_client *client;
	uint32_t unique_id = 0xCAFEBEEF;

	client = sec_dbg_region_alloc(unique_id, MOCK_DBG_REGION_RESERVED_SIZE);

	KUNIT_EXPECT_TRUE(test, IS_ERR_OR_NULL(client));
}

static void test_case_0_find_client(struct kunit *test)
{
	uint32_t unique_id[] = {
		0xCAFEBEEF,
		0xCAFEBABE,
		0xCAFEC0C0,
	};
	const size_t nr_clients = ARRAY_SIZE(unique_id);
	struct sec_dbg_region_client **client_alloc;
	const struct sec_dbg_region_client **client_found;
	size_t i;

	client_alloc = kunit_kmalloc(test, nr_clients * sizeof(*client_alloc),
			GFP_KERNEL);
	client_found = kunit_kmalloc(test, nr_clients * sizeof(*client_found),
			GFP_KERNEL);

	/* STEP 1. alloc clients */
	for (i = 0; i < nr_clients; i++)
		client_alloc[i] = sec_dbg_region_alloc(unique_id[i], 256);

	/* STEP 2. find clients */
	for (i = 0; i < nr_clients; i++)
		client_found[i] = sec_dbg_region_find(unique_id[i]);

	/* STEP 3. Verify */
	for (i = 0; i < nr_clients; i++) {
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, client_alloc[i]);
		KUNIT_EXPECT_PTR_EQ(test,
			(const struct sec_dbg_region_client *)client_alloc[i],
			client_found[i]);
	}
}

static void test_case_0_free_client(struct kunit *test)
{
	uint32_t unique_id[] = {
		0xCAFEBEEF,
		0xCAFEBABE,
		0xCAFEC0C0,
	};
	const size_t nr_clients = ARRAY_SIZE(unique_id);
	struct sec_dbg_region_client **client_alloc;
	const struct sec_dbg_region_client **client_found;
	size_t i;

	client_alloc = kunit_kmalloc(test, nr_clients * sizeof(*client_alloc),
			GFP_KERNEL);
	client_found = kunit_kmalloc(test, nr_clients * sizeof(*client_found),
			GFP_KERNEL);

	/* STEP 1. alloc clients */
	for (i = 0; i < nr_clients; i++)
		client_alloc[i] = sec_dbg_region_alloc(unique_id[i], 256);

	/* STEP 2. free clients */
	for (i = 0; i < nr_clients; i++)
		sec_dbg_region_free(client_alloc[i]);

	/* STEP 3. find clients */
	for (i = 0; i < nr_clients; i++)
		client_found[i] = sec_dbg_region_find(unique_id[i]);

	/* STEP 4. Verify */
	for (i = 0; i < nr_clients; i++) {
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, client_alloc[i]);
		KUNIT_EXPECT_TRUE(test, IS_ERR_OR_NULL(client_found[i]));
		KUNIT_EXPECT_PTR_NE(test,
			(const struct sec_dbg_region_client *)client_alloc[i],
			client_found[i]);
	}
}

static struct kunit_case sec_dbg_region_test_cases[] = {
	KUNIT_CASE(test_case_0_probe_and_remove),
	KUNIT_CASE(test_case_0_alloc_client),
	KUNIT_CASE(test_case_1_alloc_client),
	KUNIT_CASE(test_case_0_find_client),
	KUNIT_CASE(test_case_0_free_client),
	{}
};

static struct kunit_suite sec_dbg_region_test_suite = {
	.name = "SEC Memory pool for debugging features",
	.init = sec_dbg_region_test_init,
	.exit = sec_dbg_region_test_exit,
	.test_cases = sec_dbg_region_test_cases,
};

kunit_test_suites(&sec_dbg_region_test_suite);
