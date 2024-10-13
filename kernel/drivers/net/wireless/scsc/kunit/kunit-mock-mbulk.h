/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_MBULK_H__
#define __KUNIT_MOCK_MBULK_H__

#include "../mbulk.h"

#define mbulk_chain_free(args...)			kunit_mock_mbulk_chain_free(args)
#define mbulk_chain_tlen(args...)			kunit_mock_mbulk_chain_tlen(args)
#define mbulk_chain_with_signal_alloc_by_pool(args...)	kunit_mock_mbulk_chain_with_signal_alloc_by_pool(args)
#define mbulk_with_signal_alloc_by_pool(args...)	kunit_mock_mbulk_with_signal_alloc_by_pool(args)
#define mbulk_pool_dump(args...)			kunit_mock_mbulk_pool_dump(args)
#define mbulk_get_signal(args...)			kunit_mock_mbulk_get_signal(args)
#define mbulk_dat_rw(args ...)				kunit_mock_mbulk_dat_rw(args)
#define mbulk_append_tail(args ...)			kunit_mock_mbulk_append_tail(args)
#define mbulk_get_colour(args...)			kunit_mock_mbulk_get_colour(args)
#define mbulk_pool_get_free_count(args...)		kunit_mock_mbulk_pool_get_free_count(args)
#define mbulk_chain_tail(args...)			kunit_mock_mbulk_chain_tail(args)
#define mbulk_chain_next(args...)			kunit_mock_mbulk_chain_next(args)
#define mbulk_chain_bufsz(args...)			kunit_mock_mbulk_chain_bufsz(args)
#define mbulk_pool_remove(args...)			kunit_mock_mbulk_pool_remove(args)
#define mbulk_free_virt_host(args...)			kunit_mock_mbulk_free_virt_host(args)
#define mbulk_reserve_head(args ...)			kunit_mock_mbulk_reserve_head(args)
#define mbulk_pool_add(args ...)			kunit_mock_mbulk_pool_add(args)


static struct mbulk m;

static void kunit_mock_mbulk_chain_free(struct mbulk *sg)
{
	return;
}

static size_t kunit_mock_mbulk_chain_tlen(struct mbulk *m)
{
	return 0;
}

static struct mbulk *kunit_mock_mbulk_chain_with_signal_alloc_by_pool(u8 pool_id,
								      enum mbulk_class clas,
								      size_t sig_bufsz,
								      size_t dat_bufsz)

{
	return NULL;
}

static struct mbulk *kunit_mock_mbulk_with_signal_alloc_by_pool(u8 pool_id,
								mbulk_colour colour,
								enum mbulk_class clas,
								size_t sig_bufsz_req,
								size_t dat_bufsz)
{
	/* Return NULL when the pool_id is out of boundary */
	if (pool_id >= MBULK_POOL_ID_MAX) {
		return NULL;
	}

	m.flag = MBULK_F_SIG;

	return &m;
}

static bool mbulk_reserve_head(struct mbulk *m, size_t headroom)
{
	return true;
}

static void *kunit_mock_mbulk_get_signal(const struct mbulk *m)
{
	if (m && ((m)->flag & MBULK_F_SIG))
		return ((uintptr_t)(m) + sizeof(struct mbulk));
	else
		return NULL;
}

static void *kunit_mock_mbulk_dat_rw(const struct mbulk *m)
{
	if (m)
		return (void *)m;
	else
		return NULL;
}

static void kunit_mock_mbulk_append_tail(struct mbulk *m, size_t more)
{
	return;
}

static void kunit_mock_mbulk_pool_dump(u8 pool_id, int max_cnt)
{
	return;
}

static mbulk_colour kunit_mock_mbulk_get_colour(u8 pool_id, struct mbulk *m)
{
	return (mbulk_colour)0;
}

static int kunit_mock_mbulk_pool_get_free_count(u8 pool_id)
{
	return 0;
}

static struct mbulk *kunit_mock_mbulk_chain_tail(struct mbulk *m)
{
	return NULL;
}

static inline scsc_mifram_ref mbulk_chain_next(struct mbulk *m)
{
	if (!m)
		return NULL;
	if (!m->chain_next_offset)
		return NULL;
	return m->chain_next_offset;
}

static size_t kunit_mock_mbulk_chain_bufsz(struct mbulk *m)
{
	return 0;
}

#ifdef CONFIG_SCSC_WLAN_DEBUG
static int kunit_mock_mbulk_pool_add(u8 pool_id, char *base, char *end,
				     size_t seg_size, u8 guard, int minor)
{
	return 0;
}
#else
static int kunit_mock_mbulk_pool_add(u8 pool_id, char *base, char *end,
				     size_t buf_size, u8 guard)
{
	return 0;
}
#endif

static void kunit_mock_mbulk_pool_remove(u8 pool_id)
{
	return;
}

static void kunit_mock_mbulk_free_virt_host(struct mbulk *m)
{
	return;
}
#endif
