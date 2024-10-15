// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-common.h"
#include "../ba_replay.c"

/* unit test function definition */
static void test_slsi_ba_replay_store_pn(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_peer *peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	((struct slsi_skb_cb *)skb->cb)->is_ciphered = true;

	slsi_ba_replay_store_pn(dev, peer, skb);
}

static void test_slsi_ba_replay_get_pn(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_peer *peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	struct slsi_ba_frame_desc *frame_desc = kunit_kzalloc(test, sizeof(struct slsi_ba_frame_desc), GFP_KERNEL);
	((struct slsi_skb_cb *)skb->cb)->is_ciphered = true;

	slsi_ba_replay_get_pn(dev, peer, skb, frame_desc);
}

static void test_slsi_ba_replay_reset_pn(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_peer *peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);

	slsi_ba_replay_reset_pn(dev, peer);
}

static void test_ba_replay_check_option_1(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_ba_session_rx *ba_session_rx = kunit_kzalloc(test, sizeof(struct slsi_ba_session_rx), GFP_KERNEL);
	struct slsi_ba_frame_desc *frame_desc = kunit_kzalloc(test, sizeof(struct slsi_ba_frame_desc), GFP_KERNEL);
	char cb[48] = {[0 ... 47] = 1};

	frame_desc->flag_old_sn = true;
	KUNIT_EXPECT_TRUE(test, ba_replay_check_option_1(dev, ba_session_rx, frame_desc));

	frame_desc->flag_old_sn = false;
	frame_desc->signal = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	memcpy(frame_desc->signal->cb, cb, sizeof(frame_desc->signal->cb));

	ba_session_rx->peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);

	KUNIT_EXPECT_TRUE(test, ba_replay_check_option_1(dev, ba_session_rx, frame_desc));

#ifdef CONFIG_SCSC_SMAPPER
	cb[25] = 0;		//is_ciphered
#else
	cb[13] = 0;
#endif
	memcpy(frame_desc->signal->cb, cb, sizeof(frame_desc->signal->cb));

	KUNIT_EXPECT_FALSE(test, ba_replay_check_option_1(dev, ba_session_rx, frame_desc));
}

static void test_ba_replay_check_option_2(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_ba_session_rx *ba_session_rx = kunit_kzalloc(test, sizeof(struct slsi_ba_session_rx), GFP_KERNEL);
	struct slsi_ba_frame_desc *frame_desc = kunit_kzalloc(test, sizeof(struct slsi_ba_frame_desc), GFP_KERNEL);
	char cb[48] = {[0 ... 47] = 1};

	frame_desc->signal = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	memcpy(frame_desc->signal->cb, cb, sizeof(frame_desc->signal->cb));

	ba_session_rx->peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	ba_session_rx->start_sn = 0;
	ba_session_rx->buffer_size = 1;

	frame_desc->sn = 3;

	frame_desc->flag_old_sn = false;
	KUNIT_EXPECT_TRUE(test, ba_replay_check_option_2(dev, ba_session_rx, frame_desc));

	frame_desc->flag_old_sn = true;
	ba_session_rx->ba_window[SN_TO_INDEX(ba_session_rx, frame_desc->sn)].sent = true;
	KUNIT_EXPECT_TRUE(test, ba_replay_check_option_2(dev, ba_session_rx, frame_desc));

	ba_session_rx->ba_window[SN_TO_INDEX(ba_session_rx, frame_desc->sn)].sent = false;
	KUNIT_EXPECT_TRUE(test, ba_replay_check_option_2(dev, ba_session_rx, frame_desc));

	ba_session_rx->highest_received_sn = BA_WINDOW_BOUNDARY;
	KUNIT_EXPECT_TRUE(test, ba_replay_check_option_2(dev, ba_session_rx, frame_desc));

#ifdef CONFIG_SCSC_SMAPPER
	cb[25] = 0;		//is_ciphered
#else
	cb[13] = 0;
#endif
	memcpy(frame_desc->signal->cb, cb, sizeof(frame_desc->signal->cb));
	KUNIT_EXPECT_FALSE(test, ba_replay_check_option_2(dev, ba_session_rx, frame_desc));

	frame_desc->flag_old_sn = false;
	KUNIT_EXPECT_FALSE(test, ba_replay_check_option_2(dev, ba_session_rx, frame_desc));
}

static void test_slsi_ba_replay_check_pn(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_ba_session_rx *ba_session_rx = kunit_kzalloc(test, sizeof(struct slsi_ba_session_rx), GFP_KERNEL);
	struct slsi_ba_frame_desc *frame_desc = kunit_kzalloc(test, sizeof(struct slsi_ba_frame_desc), GFP_KERNEL);

	char cb[48] = {[0 ... 47] = 1};
	frame_desc->signal = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	memcpy(frame_desc->signal->cb, cb, sizeof(frame_desc->signal->cb));

	ba_session_rx->peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);

	KUNIT_EXPECT_TRUE(test, slsi_ba_replay_check_pn(dev, ba_session_rx, frame_desc));
}


/* Test fictures */
static int ba_replay_test_init(struct kunit *test)
{
	test_dev_init(test);

	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
	return 0;
}

static void ba_replay_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case ba_replay_test_cases[] = {
	KUNIT_CASE(test_slsi_ba_replay_store_pn),
	KUNIT_CASE(test_slsi_ba_replay_get_pn),
	KUNIT_CASE(test_slsi_ba_replay_reset_pn),
	KUNIT_CASE(test_ba_replay_check_option_1),
	KUNIT_CASE(test_ba_replay_check_option_2),
	KUNIT_CASE(test_slsi_ba_replay_check_pn),
	{}
};

static struct kunit_suite ba_replay_test_suite[] = {
	{
		.name = "ba_replay-test",
		.test_cases = ba_replay_test_cases,
		.init = ba_replay_test_init,
		.exit = ba_replay_test_exit,
	}
};

kunit_test_suites(ba_replay_test_suite);
