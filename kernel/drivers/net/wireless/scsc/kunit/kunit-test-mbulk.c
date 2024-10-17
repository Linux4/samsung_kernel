// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "../mbulk.c"


static void test_mbulk_pool_put(struct kunit *test)
{
	struct mbulk_pool *pool = kunit_kzalloc(test, sizeof(struct mbulk_pool), GFP_KERNEL);
	struct mbulk *m = kunit_kzalloc(test, sizeof(struct mbulk), GFP_KERNEL);

	m->flag = MBULK_F_FREE;
	mbulk_pool_put(pool, m);

	m->flag = 0;
	m->clas = MBULK_CLASS_FROM_RADIO_FORWARDED + 1;
	mbulk_pool_put(pool, m);

	m->clas = 0;
	mbulk_pool_put(pool, m);

	m->flag = 0;
	mbulk_pool_put(pool, m);
}

static void test_mbulk_seg_generic_alloc(struct kunit *test)
{
	struct mbulk_pool *pool = kunit_kzalloc(test, sizeof(struct mbulk_pool), GFP_KERNEL);
	struct mbulk *m = kunit_kzalloc(test, sizeof(struct mbulk), GFP_KERNEL);
	enum mbulk_class clas = 0;

	KUNIT_EXPECT_PTR_EQ(test, NULL, mbulk_seg_generic_alloc(NULL, clas, 0, 0));

	KUNIT_EXPECT_PTR_EQ(test, NULL, mbulk_seg_generic_alloc(pool, clas, 0, 0));

	pool->free_list = m;
	pool->free_cnt = 3;
	KUNIT_EXPECT_PTR_EQ(test, m, mbulk_seg_generic_alloc(pool, clas, 1, 0));

	pool->free_list = m;
	KUNIT_EXPECT_PTR_EQ(test, m, mbulk_seg_generic_alloc(pool, clas, 1, MBULK_DAT_BUFSZ_REQ_BEST_MAGIC));

	pool->free_list = m;
	m->next_offset = 1;
	KUNIT_EXPECT_PTR_EQ(test, m, mbulk_seg_generic_alloc(pool, clas, 1, 1));
}

static void test_mbulk_pool_get_free_count(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, -EIO, mbulk_pool_get_free_count(MBULK_POOL_ID_MAX));

	mbulk_pools[0].valid = 0;
	KUNIT_EXPECT_EQ(test, -EIO, mbulk_pool_get_free_count(0));

	mbulk_pools[0].valid = 1;
	KUNIT_EXPECT_EQ(test, 0, mbulk_pool_get_free_count(0));
}

static void test_mbulk_with_signal_alloc_by_pool(struct kunit *test)
{
	u8 pool_id;
	mbulk_colour colour = 0;
	enum mbulk_class clas = MBULK_CLASS_CONTROL;
	size_t sig_bufsz_req = 0;
	size_t dat_bufsz = 0;
	struct mbulk *m_ret;

	pool_id = MBULK_POOL_ID_MAX;
	KUNIT_EXPECT_TRUE(test, 0 == mbulk_with_signal_alloc_by_pool(pool_id, colour, clas, sig_bufsz_req, dat_bufsz));

	pool_id = 0;
	mbulk_pools[pool_id].valid = 0;
	KUNIT_EXPECT_TRUE(test, 0 == mbulk_with_signal_alloc_by_pool(pool_id, colour, clas, sig_bufsz_req, dat_bufsz));

	mbulk_pools[pool_id].valid = 1;
	mbulk_pools[pool_id].seg_size = 2;
	mbulk_pools[pool_id].end_addr = 1;
	mbulk_pools[pool_id].shift = 0;
	mbulk_pools[pool_id].tot_seg_num = 1;
	mbulk_pools[pool_id].mbulk_tracker = kunit_kzalloc(test, sizeof(struct mbulk_tracker), GFP_KERNEL);
	dat_bufsz = 1;
	KUNIT_EXPECT_TRUE(test, 0 == mbulk_with_signal_alloc_by_pool(pool_id, colour, clas, sig_bufsz_req, dat_bufsz));
}

static void test_mbulk_get_colour(struct kunit *test)
{
	u8 pool_id = 0;
	struct mbulk *m = NULL;

	mbulk_pools[pool_id].valid = 0;
	KUNIT_EXPECT_TRUE(test, 0 == mbulk_get_colour(pool_id, m));

	mbulk_pools[pool_id].valid = 1;
	mbulk_pools[pool_id].end_addr = 1;
	mbulk_pools[pool_id].shift = 0;
	mbulk_pools[pool_id].tot_seg_num = 1;
	mbulk_pools[pool_id].mbulk_tracker = kunit_kzalloc(test, sizeof(struct mbulk_tracker), GFP_KERNEL);
	KUNIT_EXPECT_TRUE(test, 0 == mbulk_get_colour(pool_id, m));
}

static int call_mbulk_pool_add(u8 pool_id, char *base, char *end, size_t seg_size, u8 guard)
{
#ifdef CONFIG_SCSC_WLAN_DEBUG
	return mbulk_pool_add(pool_id, base, end, seg_size, guard, 0);
#else
	return mbulk_pool_add(pool_id, base, end, seg_size, guard);
#endif
}

static void test_mbulk_pool_add(struct kunit *test)
{
	u8 pool_id = 2;
	char *base = kunit_kzalloc(test, sizeof(u8)*500, GFP_KERNEL);
	char *end = base;
	size_t seg_size = 0;
	u8 guard = 0;

	KUNIT_EXPECT_EQ(test, -EIO, call_mbulk_pool_add(pool_id, base, end, seg_size, guard));

	pool_id = 0;
	KUNIT_EXPECT_EQ(test, -EIO, call_mbulk_pool_add(pool_id, ((int)base + 1), end, seg_size, guard));

	KUNIT_EXPECT_EQ(test, 0, call_mbulk_pool_add(pool_id, base, end + 128, seg_size, guard));
	vfree(mbulk_pools[pool_id].mbulk_tracker);
}

static void test_mbulk_pool_remove(struct kunit *test)
{
	u8 pool_id = 2;

	mbulk_pool_remove(pool_id);

	pool_id = 0;
	mbulk_pools[pool_id].mbulk_tracker = vmalloc(sizeof(struct mbulk_tracker));
	mbulk_pool_remove(pool_id);
}

static void test_mbulk_pool_dump(struct kunit *test)
{
	u8 pool_id = 2;

	pool_id = 0;
	mbulk_pools[pool_id].free_list = kunit_kzalloc(test, sizeof(struct mbulk_pool), GFP_KERNEL);
	mbulk_pool_dump(pool_id, 1);
}

static void test_mbulk_free_virt_host(struct kunit *test)
{
	struct mbulk *m = kunit_kzalloc(test, sizeof(struct mbulk), GFP_KERNEL);

	m->flag = MBULK_F_FREE;
	mbulk_pools[0].valid = 0;
	mbulk_free_virt_host(m);

	mbulk_pools[0].valid = 1;
	mbulk_pools[0].end_addr = 1;
	mbulk_pools[0].shift = 0;
	mbulk_pools[0].tot_seg_num = 0;
	mbulk_free_virt_host(m);
}

static int mbulk_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void mbulk_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

static struct kunit_case mbulk_test_cases[] = {
	KUNIT_CASE(test_mbulk_pool_put),
	KUNIT_CASE(test_mbulk_seg_generic_alloc),
	KUNIT_CASE(test_mbulk_pool_get_free_count),
	KUNIT_CASE(test_mbulk_with_signal_alloc_by_pool),
	KUNIT_CASE(test_mbulk_get_colour),
	KUNIT_CASE(test_mbulk_pool_add),
	KUNIT_CASE(test_mbulk_pool_remove),
	KUNIT_CASE(test_mbulk_pool_dump),
	KUNIT_CASE(test_mbulk_free_virt_host),
	{}
};

static struct kunit_suite mbulk_test_suite[] = {
	{
		.name = "kunit-mbulk-test",
		.test_cases = mbulk_test_cases,
		.init = mbulk_test_init,
		.exit = mbulk_test_exit,
	}
};

kunit_test_suites(mbulk_test_suite);
