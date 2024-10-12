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

#include "pablo-kunit-test.h"
#include "lib/votf/pablo-votf.h"
#include "lib/votf/pablo-votf-reg.h"

#define GET_CR_VAL(base, reg)	(*(u32 *)(base + reg.sfr_offset))
#define SET_CR_VAL(base, reg, val) 			\
	do {						\
		u32 *addr = (u32 *)(base + reg.sfr_offset);	\
		*addr = val;				\
	} while (0)

static struct pablo_kunit_test_ctx {
	struct votf_dev *mock_votf_dev[VOTF_DEV_NUM];
	struct votf_dev *votf_dev[VOTF_DEV_NUM];
} pkt_ctx;

/* Define test cases. */
static void votfitf_init_kunit_test(struct kunit *test)
{
	struct votf_dev *dev;
	u32 dev_id;

	votfitf_init();

	for (dev_id = 0; dev_id < VOTF_DEV_NUM; dev_id++) {
		dev = pkt_ctx.mock_votf_dev[dev_id];
		if (!dev)
			continue;

		KUNIT_EXPECT_EQ(test, dev->ring_request, 0);
		KUNIT_EXPECT_EQ(test, dev->ring_pair[0][0][0], VS_DISCONNECTED);
		KUNIT_EXPECT_EQ(test, atomic_read(&dev->ip_enable_cnt[0]), 0);
		KUNIT_EXPECT_EQ(test, atomic_read(&dev->id_enable_cnt[0][0]), 0);
	}
}

static void votfitf_create_ring_kunit_test(struct kunit *test)
{
	struct votf_dev *dev = pkt_ctx.mock_votf_dev[0];
	void *dev_addr;
	u32 svc = TWS;
	u32 ip = 0;
	u32 id = 0;
	u32 local_ip;

	dev_addr = dev->votf_addr[ip];

	/* TC #1. Create VOTF ring with empty table. */
	votfitf_create_ring();

	KUNIT_EXPECT_EQ(test, dev->ring_request, 1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_LOCAL_IP]), (u32)0);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_RING_CLK_EN]), (u32)0);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_RING_ENABLE]), (u32)0);

	/* TC #2. VOTF ring has already been created. */
	dev->votf_table[svc][ip][id].use = true;
	SET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_RING_CLK_EN], 1);
	SET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_RING_ENABLE], 1);

	votfitf_create_ring();

	KUNIT_EXPECT_EQ(test, dev->ring_request, 2);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_RING_CLK_EN]), (u32)1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_RING_ENABLE]), (u32)1);

	/* TC #3. Create VOTF ring. */
	local_ip = __LINE__;
	dev->votf_table[svc][ip][id].ip = local_ip;
	SET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_RING_CLK_EN], 0);
	SET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_RING_ENABLE], 0);

	votfitf_create_ring();

	KUNIT_EXPECT_EQ(test, dev->ring_request, 1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_LOCAL_IP]), local_ip);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_RING_CLK_EN]), (u32)1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_RING_ENABLE]), (u32)1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_SELREGISTERMODE]), (u32)1); /* Direct mode */
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_SELREGISTER]), (u32)1); /* SetA */
}

static void votfitf_destroy_ring_kunit_test(struct kunit *test)
{
	struct votf_dev *dev = pkt_ctx.mock_votf_dev[0];
	void *dev_addr;
	u32 svc = TWS;
	u32 ip = 0;
	u32 id = 0;
	u32 local_ip;

	dev_addr = dev->votf_addr[ip];

	/* TC #1. Destroy VOTF ring with empty table */
	dev->ring_request = 1;

	votfitf_destroy_ring();

	/* TC #2. Destroy VOTF ring without previous request */
	dev->votf_table[svc][ip][id].use = true;
	SET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_RING_CLK_EN], 1);
	SET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_RING_ENABLE], 1);

	dev->ring_request = 0;

	votfitf_destroy_ring();

	KUNIT_EXPECT_EQ(test, dev->ring_request, 0);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_RING_CLK_EN]), (u32)1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_RING_ENABLE]), (u32)1);

	/* TC #3. Destroy VOTF ring with multipe previous request */
	dev->ring_request = 2;

	votfitf_destroy_ring();

	KUNIT_EXPECT_EQ(test, dev->ring_request, 1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_RING_CLK_EN]), (u32)1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_RING_ENABLE]), (u32)1);

	/* TC #4. Destroy VOTF ring with last request */
	local_ip = __LINE__;
	dev->votf_table[svc][ip][id].ip = local_ip;
	dev->ring_request = 1;

	votfitf_destroy_ring();

	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_LOCAL_IP]), local_ip);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_RING_CLK_EN]), (u32)0);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_RING_ENABLE]), (u32)0);
	KUNIT_EXPECT_EQ(test, dev->ring_pair[svc][ip][id], VS_DISCONNECTED);
}

static void votfitf_set_service_cfg_negative_kunit_test(struct kunit *test)
{
	struct votf_dev *dev = pkt_ctx.mock_votf_dev[0];
	struct votf_info v_info;
	struct votf_service_cfg *v_cfg = NULL;
	int ret;
	u32 svc, ip, id;

	memset(&v_info, 0, sizeof(struct votf_info));

	/* TC #1. NULL votf_info structure */
	ret = votfitf_set_service_cfg(NULL, v_cfg);
	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #2. Un-registered IP */
	v_info.ip = __LINE__;

	ret = votfitf_set_service_cfg(&v_info, v_cfg);
	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #3. VOTF IP which is not in use state. */
	svc = v_info.service = TWS;
	ip = id = 0;
	dev->votf_table[svc][ip][id].ip = v_info.ip;
	dev->votf_table[svc][ip][id].use = false;
	v_cfg = &dev->votf_cfg[svc][ip][id];

	ret = votfitf_set_service_cfg(&v_info, v_cfg);
	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #4. VOTF IP which has already been connected. */
	dev->votf_table[svc][ip][id].use = true;
	v_cfg->option |= VOTF_OPTION_MSK_COUNT;
	dev->ring_pair[svc][ip][id] = VS_CONNECTED;

	ret = votfitf_set_service_cfg(&v_info, v_cfg);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, atomic_read(&dev->id_enable_cnt[ip][id]), 1);

	/* TC #5. There is no connected VOTF IP */
	dev->ring_pair[svc][ip][id] = VS_DISCONNECTED;
	v_cfg->connected_ip = __LINE__;

	ret = votfitf_set_service_cfg(&v_info, v_cfg);
	KUNIT_EXPECT_LT(test, ret, 0);
}

static void votfitf_set_service_cfg_tws_kunit_test(struct kunit *test)
{
	struct votf_dev *dev = pkt_ctx.mock_votf_dev[0];
	struct votf_info v_info;
	struct votf_service_cfg *v_cfg = NULL;
	int ret;
	u32 svc, ip, id, dest;
	void *dev_addr;

	memset(&v_info, 0, sizeof(struct votf_info));

	/* TWS setup */
	svc = v_info.service = TWS;
	ip = id = 0;
	v_info.ip = __LINE__;
	dev->votf_table[svc][ip][id].ip = v_info.ip;
	dev->votf_table[svc][ip][id].use = true;
	dev->ring_pair[svc][ip][id] = VS_DISCONNECTED;

	/* TRS setup */
	v_cfg = &dev->votf_cfg[svc][ip][id];
	v_cfg->connected_ip = __LINE__;
	dev->votf_table[!svc][ip][id].ip = v_cfg->connected_ip;
	dev->votf_table[!svc][ip][id].use = true;

	/* TC #1. TRS has already been connected */
	dev->ring_pair[!svc][ip][id] = VS_CONNECTED;

	ret = votfitf_set_service_cfg(&v_info, v_cfg);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC #2. TWS with C2SERV */
	dev->ring_pair[!svc][ip][id] = VS_DISCONNECTED;
	dev->votf_table[svc][ip][id].module = C2SERV;
	v_cfg->enable = true;
	v_cfg->limit = __LINE__ & 0xff;
	v_cfg->token_size = __LINE__ & 0xff;

	ret = votfitf_set_service_cfg(&v_info, v_cfg);

	dest = (v_cfg->connected_ip << 4) | v_cfg->connected_id;
	dev_addr = dev->votf_addr[ip];
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_TWS_DEST]), dest);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_TWS_ENABLE]), (u32)v_cfg->enable);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_TWS_LIMIT]), v_cfg->limit);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_TWS_LINES_IN_TOKEN]), v_cfg->token_size);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[svc][ip][id].connected_ip, v_cfg->connected_ip);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[svc][ip][id].connected_id, v_cfg->connected_id);
	KUNIT_EXPECT_EQ(test, dev->ring_pair[svc][ip][id], VS_READY);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[svc][ip][id].enable, v_cfg->enable);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[svc][ip][id].limit, v_cfg->limit);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[svc][ip][id].token_size, v_cfg->token_size);

	/* TC #3. TWS with C2AGENT, TRS also be connected. */
	dev->votf_table[svc][ip][id].module = C2AGENT;
	v_cfg->width = 1;
	v_cfg->bitwidth = 8;
	dev->ring_pair[!svc][ip][id] = VS_READY;

	ret = votfitf_set_service_cfg(&v_info, v_cfg);

	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2agent_regs[C2AGENT_R_TWS_DEST]), dest);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2agent_regs[C2AGENT_R_TWS_ENABLE]), (u32)v_cfg->enable);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2agent_regs[C2AGENT_R_TWS_LIMIT]), v_cfg->limit);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2agent_regs[C2AGENT_R_TWS_TOKEN_SIZE]), v_cfg->token_size);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[svc][ip][id].connected_ip, v_cfg->connected_ip);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[svc][ip][id].connected_id, v_cfg->connected_id);
	KUNIT_EXPECT_EQ(test, dev->ring_pair[svc][ip][id], VS_CONNECTED);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[svc][ip][id].enable, v_cfg->enable);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[svc][ip][id].limit, v_cfg->limit);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[svc][ip][id].token_size, v_cfg->token_size);
	KUNIT_EXPECT_EQ(test, dev->ring_pair[!svc][ip][id], VS_CONNECTED);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[!svc][ip][id].connected_ip, v_info.ip);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[!svc][ip][id].connected_id, id);

	/* TC #4. Disable entire services */
	votfitf_disable_service();

	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_TWS_ENABLE]), (u32)0);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2agent_regs[C2AGENT_R_TWS_ENABLE]), (u32)0);
}

static void votfitf_set_service_cfg_trs_kunit_test(struct kunit *test)
{
	struct votf_dev *dev = pkt_ctx.mock_votf_dev[0];
	struct votf_info v_info;
	struct votf_service_cfg *v_cfg = NULL;
	int ret;
	u32 svc, ip, id, dest;
	void *dev_addr;

	memset(&v_info, 0, sizeof(struct votf_info));

	/* TRS setup */
	svc = v_info.service = TRS;
	ip = id = 0;
	v_info.ip = __LINE__;
	dev->votf_table[svc][ip][id].ip = v_info.ip;
	dev->votf_table[svc][ip][id].use = true;
	dev->ring_pair[svc][ip][id] = VS_DISCONNECTED;

	/* TWS setup */
	v_cfg = &dev->votf_cfg[svc][ip][id];
	v_cfg->connected_ip = __LINE__;
	dev->votf_table[!svc][ip][id].ip = v_cfg->connected_ip;
	dev->votf_table[!svc][ip][id].use = true;

	/* TC #1. TWS has already been connected */
	dev->ring_pair[!svc][ip][id] = VS_CONNECTED;

	ret = votfitf_set_service_cfg(&v_info, v_cfg);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC #2. TRS with C2SERV */
	dev->ring_pair[!svc][ip][id] = VS_DISCONNECTED;
	dev->votf_table[svc][ip][id].module = C2SERV;
	v_cfg->enable = true;
	v_cfg->limit = __LINE__ & 0xff;
	v_cfg->token_size = __LINE__ & 0xff;
	v_cfg->height = __LINE__;

	ret = votfitf_set_service_cfg(&v_info, v_cfg);

	dest = (v_cfg->connected_ip << 4) | v_cfg->connected_id;
	dev_addr = dev->votf_addr[ip];
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_TRS_ENABLE]), (u32)v_cfg->enable);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_TRS_LIMIT]), v_cfg->limit);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_TRS_LINES_COUNT]), v_cfg->height);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_TRS_LINES_IN_TOKEN]), v_cfg->token_size);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[svc][ip][id].connected_ip, v_cfg->connected_ip);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[svc][ip][id].connected_id, v_cfg->connected_id);
	KUNIT_EXPECT_EQ(test, dev->ring_pair[svc][ip][id], VS_READY);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[svc][ip][id].enable, v_cfg->enable);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[svc][ip][id].limit, v_cfg->limit);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[svc][ip][id].token_size, v_cfg->token_size);

	/* TC #3. TRS with C2AGENT, TRS also be connected. */
	dev->votf_table[svc][ip][id].module = C2AGENT;
	v_cfg->width = 1;
	v_cfg->bitwidth = 8;
	dev->ring_pair[!svc][ip][id] = VS_READY;
	dev->votf_cfg[!svc][ip][id].connected_ip = v_info.ip;
	dev->votf_cfg[!svc][ip][id].connected_id = v_info.id;

	ret = votfitf_set_service_cfg(&v_info, v_cfg);

	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2agent_regs[C2AGENT_R_TRS_ENABLE]), (u32)v_cfg->enable);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2agent_regs[C2AGENT_R_TRS_LIMIT]), v_cfg->limit);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2agent_regs[C2AGENT_TRS_FRAME_SIZE]), v_cfg->height);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2agent_regs[C2AGENT_R_TRS_TOKEN_SIZE]), v_cfg->token_size);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[svc][ip][id].connected_ip, v_cfg->connected_ip);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[svc][ip][id].connected_id, v_cfg->connected_id);
	KUNIT_EXPECT_EQ(test, dev->ring_pair[svc][ip][id], VS_CONNECTED);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[svc][ip][id].enable, v_cfg->enable);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[svc][ip][id].limit, v_cfg->limit);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[svc][ip][id].token_size, v_cfg->token_size);
	KUNIT_EXPECT_EQ(test, dev->ring_pair[!svc][ip][id], VS_CONNECTED);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[!svc][ip][id].connected_ip, v_info.ip);
	KUNIT_EXPECT_EQ(test, dev->votf_cfg[!svc][ip][id].connected_id, id);

	/* TC #4. Disable entire services */
	votfitf_disable_service();

	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_TRS_ENABLE]), (u32)0);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2agent_regs[C2AGENT_R_TRS_ENABLE]), (u32)0);
}

static void votfitf_reset_kunit_test(struct kunit *test)
{
	struct votf_dev *dev = pkt_ctx.mock_votf_dev[0];
	struct votf_info v_info;
	int ret, type = 0;
	u32 svc, ip, id;

	memset(&v_info, 0, sizeof(struct votf_info));

	/* TC #1. Invalid votf_info */
	ret = votfitf_reset(NULL, type);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #2. VOTF IP which is not in use state. */
	v_info.ip = __LINE__;
	svc = v_info.service = TWS;
	ip = id = 0;
	dev->votf_table[svc][ip][id].ip = v_info.ip;
	dev->votf_table[svc][ip][id].use = false;

	ret = votfitf_reset(&v_info, type);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #3. Un-registered target VOTF IP */
	dev->votf_table[svc][ip][id].use = true;
	dev->votf_table[svc][ip][id].module = C2SERV;
	dev->votf_cfg[svc][ip][id].connected_ip = __LINE__;

	ret = votfitf_reset(&v_info, type);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #4. C2SERV TWS SW core reset */
	dev->votf_table[!svc][ip + 1][id].ip = dev->votf_cfg[svc][ip][id].connected_ip;
	dev->votf_table[!svc][ip + 1][id].use = true;
	type = SW_CORE_RESET;

	ret = votfitf_reset(&v_info, type);

	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev->votf_addr[ip], c2serv_regs[C2SERV_R_SW_CORE_RESET]), (u32)1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev->votf_addr[ip + 1], c2serv_regs[C2SERV_R_SW_CORE_RESET]), (u32)1);
	KUNIT_EXPECT_EQ(test, dev->ring_pair[svc][ip][id], VS_DISCONNECTED);
	KUNIT_EXPECT_EQ(test, dev->ring_pair[!svc][ip + 1][id], VS_DISCONNECTED);

	/* TC #5. C2SERV TWS SW reset */
	type = SW_RESET;

	ret = votfitf_reset(&v_info, type);

	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev->votf_addr[ip], c2serv_regs[C2SERV_R_SW_RESET]), (u32)1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev->votf_addr[ip + 1], c2serv_regs[C2SERV_R_SW_RESET]), (u32)1);

	/* TC #6. C2AGENT TRS SW reset */
	ip += 2;
	v_info.ip = __LINE__;
	svc = v_info.service = TRS;
	dev->votf_table[svc][ip][id].ip = v_info.ip;
	dev->votf_table[svc][ip][id].use = true;
	dev->votf_table[svc][ip][id].module = C2AGENT;
	dev->votf_cfg[svc][ip][id].connected_ip = __LINE__;

	dev->votf_table[!svc][ip + 1][id].ip = dev->votf_cfg[svc][ip][id].connected_ip;
	dev->votf_table[!svc][ip + 1][id].use = true;

	ret = votfitf_reset(&v_info, type);

	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev->votf_addr[ip], c2agent_regs[C2AGENT_R_SW_RESET]), (u32)1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev->votf_addr[ip + 1], c2agent_regs[C2AGENT_R_SW_RESET]), (u32)1);
	KUNIT_EXPECT_EQ(test, dev->ring_pair[svc][ip][id], VS_DISCONNECTED);
	KUNIT_EXPECT_EQ(test, dev->ring_pair[!svc][ip + 1][id], VS_DISCONNECTED);

	/* TC #7. C2AGENT TRS SW core reset */
	type = SW_CORE_RESET;

	ret = votfitf_reset(&v_info, type);

	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev->votf_addr[ip], c2agent_regs[C2AGENT_R_SW_CORE_RESET]), (u32)1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev->votf_addr[ip + 1], c2agent_regs[C2AGENT_R_SW_CORE_RESET]), (u32)1);
}

static void votfitf_create_link_kunit_test(struct kunit *test)
{
	struct votf_dev *dev = pkt_ctx.mock_votf_dev[0];
	struct votf_table_info *tws_info, *trs_info;
	void *tws_addr, *trs_addr;
	int tws_ip, trs_ip;
	u32 tws_ip_idx, trs_ip_idx;
	int ret;

	/* TC #1. VOTF IP, not in VOTF table. */
	tws_ip = __LINE__;
	tws_ip_idx = 0;
	trs_ip = __LINE__;
	trs_ip_idx = 1;

	ret = votfitf_create_link(tws_ip, trs_ip);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* Setup TWS */
	tws_addr = dev->votf_addr[tws_ip_idx];
	tws_info = &dev->votf_table[TWS][tws_ip_idx][0];
	tws_info->ip = tws_ip;
	tws_info->module = C2SERV;
	tws_info->use = 0;

	/* Setup TRS */
	trs_addr = dev->votf_addr[trs_ip_idx];
	trs_info = &dev->votf_table[TRS][trs_ip_idx][0];
	trs_info->ip = trs_ip;
	trs_info->module = C2SERV;
	trs_info->use = 0;

	/* TC #2. TWS IP is not in use state. */
	ret = votfitf_create_link(tws_ip, trs_ip);

	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, atomic_read(&dev->ip_enable_cnt[tws_ip_idx]), 0);
	KUNIT_EXPECT_EQ(test, atomic_read(&dev->ip_enable_cnt[trs_ip_idx]), 0);

	/* TC #3. TRS IP is not in use state. */
	tws_info->use = 1;

	ret = votfitf_create_link(tws_ip, trs_ip);

	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, atomic_read(&dev->ip_enable_cnt[tws_ip_idx]), 0);
	KUNIT_EXPECT_EQ(test, atomic_read(&dev->ip_enable_cnt[trs_ip_idx]), 0);

	/* TC #4. Link TWS & TRS */
	trs_info->use = 1;

	ret = votfitf_create_link(tws_ip, trs_ip);

	KUNIT_EXPECT_EQ(test, ret, 1);

	KUNIT_EXPECT_EQ(test, atomic_read(&dev->ip_enable_cnt[tws_ip_idx]), 1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_C2COM_LOCAL_IP]), (u32)tws_ip);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_C2COM_RING_CLK_EN]), (u32)1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_C2COM_RING_ENABLE]), (u32)1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_SELREGISTERMODE]), (u32)1); /* Direct mode */
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_SELREGISTER]), (u32)1); /* SetA */

	KUNIT_EXPECT_EQ(test, atomic_read(&dev->ip_enable_cnt[trs_ip_idx]), 1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_C2COM_LOCAL_IP]), (u32)trs_ip);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_C2COM_RING_CLK_EN]), (u32)1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_C2COM_RING_ENABLE]), (u32)1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_SELREGISTERMODE]), (u32)1); /* Direct mode */
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_SELREGISTER]), (u32)1); /* SetA */

	KUNIT_EXPECT_EQ(test, GET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_TWS_ENABLE]), (u32)0);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_TRS_ENABLE]), (u32)0);

	/* TC #5. Link, again */
	ret = votfitf_create_link(tws_ip, trs_ip);

	KUNIT_EXPECT_EQ(test, ret, 1);
	KUNIT_EXPECT_EQ(test, atomic_read(&dev->ip_enable_cnt[tws_ip_idx]), 2);
	KUNIT_EXPECT_EQ(test, atomic_read(&dev->ip_enable_cnt[trs_ip_idx]), 2);
}

static void votfitf_destroy_link_kunit_test(struct kunit *test)
{
	struct votf_dev *dev = pkt_ctx.mock_votf_dev[0];
	struct votf_table_info *tws_info, *trs_info;
	void *tws_addr, *trs_addr;
	int tws_ip, trs_ip;
	u32 tws_ip_idx, trs_ip_idx;
	int ret;

	/* TC #1. VOTF IP, not in VOTF table. */
	tws_ip = __LINE__;
	tws_ip_idx = 0;
	trs_ip = __LINE__;
	trs_ip_idx = 1;

	ret = votfitf_destroy_link(tws_ip, trs_ip);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* Setup TWS. refcount = 2 */
	tws_addr = dev->votf_addr[tws_ip_idx];
	tws_info = &dev->votf_table[TWS][tws_ip_idx][0];
	tws_info->ip = tws_ip;
	tws_info->module = C2SERV;
	tws_info->use = 1;
	atomic_set(&dev->ip_enable_cnt[tws_ip_idx], 2);
	SET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_C2COM_LOCAL_IP], tws_ip);
	SET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_C2COM_RING_CLK_EN], 1);
	SET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_C2COM_RING_ENABLE], 1);

	/* TC #2. TRS IP is not in VOTF table */
	ret = votfitf_destroy_link(tws_ip, trs_ip);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* Setup TRS. refcount = 2 */
	trs_addr = dev->votf_addr[trs_ip_idx];
	trs_info = &dev->votf_table[TRS][trs_ip_idx][0];
	trs_info->ip = trs_ip;
	trs_info->module = C2SERV;
	trs_info->use = 1;
	atomic_set(&dev->ip_enable_cnt[trs_ip_idx], 2);
	SET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_C2COM_LOCAL_IP], trs_ip);
	SET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_C2COM_RING_CLK_EN], 1);
	SET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_C2COM_RING_ENABLE], 1);

	/* TC #3. Destroy link */
	ret = votfitf_destroy_link(tws_ip, trs_ip);

	KUNIT_EXPECT_EQ(test, ret, 1);

	KUNIT_EXPECT_EQ(test, atomic_read(&dev->ip_enable_cnt[tws_ip_idx]), 1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_C2COM_LOCAL_IP]), (u32)tws_ip);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_C2COM_RING_CLK_EN]), (u32)1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_C2COM_RING_ENABLE]), (u32)1);

	KUNIT_EXPECT_EQ(test, atomic_read(&dev->ip_enable_cnt[trs_ip_idx]), 1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_C2COM_LOCAL_IP]), (u32)trs_ip);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_C2COM_RING_CLK_EN]), (u32)1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_C2COM_RING_ENABLE]), (u32)1);

	/* TC #4. Destroy last link */
	ret = votfitf_destroy_link(tws_ip, trs_ip);

	KUNIT_EXPECT_EQ(test, ret, 1);

	KUNIT_EXPECT_EQ(test, atomic_read(&dev->ip_enable_cnt[tws_ip_idx]), 0);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_C2COM_LOCAL_IP]), (u32)tws_ip);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_C2COM_RING_CLK_EN]), (u32)0);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_C2COM_RING_ENABLE]), (u32)0);

	KUNIT_EXPECT_EQ(test, atomic_read(&dev->ip_enable_cnt[trs_ip_idx]), 0);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_C2COM_LOCAL_IP]), (u32)trs_ip);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_C2COM_RING_CLK_EN]), (u32)0);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_C2COM_RING_ENABLE]), (u32)0);

	/* TC #5. Destroy link over refcount */
	ret = votfitf_destroy_link(tws_ip, trs_ip);

	KUNIT_EXPECT_EQ(test, ret, 1);

	KUNIT_EXPECT_EQ(test, atomic_read(&dev->ip_enable_cnt[tws_ip_idx]), 0);
	KUNIT_EXPECT_EQ(test, atomic_read(&dev->ip_enable_cnt[trs_ip_idx]), 0);
}

static void votfitf_set_flush_kunit_test(struct kunit *test)
{
	struct votf_dev *dev = pkt_ctx.mock_votf_dev[0];
	struct votf_info v_info;
	struct votf_table_info *tws_info, *trs_info;
	void *tws_addr, *trs_addr;
	int ret;
	u32 svc, tws_ip, trs_ip, id;

	/* TC #1. NULL votf_info structure */
	ret = votfitf_set_flush(NULL);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* Setup TWS */
	v_info.ip = __LINE__;
	svc = v_info.service = TWS;
	tws_ip = id = 0;
	tws_info = &dev->votf_table[svc][tws_ip][id];
	tws_info->ip = v_info.ip;
	tws_addr = dev->votf_addr[tws_ip];

	/* TC #2. VOTF IP which is not in use state */
	tws_info->use = 0;

	ret = votfitf_set_flush(&v_info);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #3. Un-registered TRS */
	tws_info->use = 1;
	dev->votf_cfg[svc][tws_ip][id].connected_ip = __LINE__;
	dev->votf_cfg[svc][tws_ip][id].connected_id = id;

	ret = votfitf_set_flush(&v_info);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* Setup TRS */
	trs_ip = tws_ip + 1;
	trs_info = &dev->votf_table[!svc][trs_ip][id];
	trs_info->ip = dev->votf_cfg[svc][tws_ip][id].connected_ip;
	trs_info->use = 1;
	trs_addr = dev->votf_addr[trs_ip];

	/* TC #4. Refcount, more than 1 */
	atomic_set(&dev->ip_enable_cnt[tws_ip], 1);
	atomic_set(&dev->id_enable_cnt[tws_ip][id], 2);

	ret = votfitf_set_flush(&v_info);

	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, atomic_read(&dev->id_enable_cnt[tws_ip][id]), 1);

	/* TC #5. TWS is in busy state */
	SET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_TWS_BUSY], 1);

	ret = votfitf_set_flush(&v_info);

	KUNIT_EXPECT_LT(test, ret, 0);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_TWS_FLUSH]), (u32)1);
	KUNIT_EXPECT_EQ(test, atomic_read(&dev->id_enable_cnt[tws_ip][id]), 0);
	KUNIT_EXPECT_EQ(test, dev->ring_pair[svc][tws_ip][id], VS_DISCONNECTED);
	KUNIT_EXPECT_EQ(test, dev->ring_pair[!svc][trs_ip][id], VS_DISCONNECTED);

	/* TC #6. Flush TRS */
	v_info.ip = trs_info->ip;
	svc = v_info.service = TRS;
	atomic_set(&dev->ip_enable_cnt[trs_ip], 1);
	atomic_set(&dev->id_enable_cnt[trs_ip][id], 1);

	ret = votfitf_set_flush(&v_info);

	KUNIT_EXPECT_EQ(test, ret, 1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_TRS_FLUSH]), (u32)1);
	KUNIT_EXPECT_EQ(test, atomic_read(&dev->id_enable_cnt[trs_ip][id]), 0);
	KUNIT_EXPECT_EQ(test, dev->ring_pair[svc][trs_ip][id], VS_DISCONNECTED);
	KUNIT_EXPECT_EQ(test, dev->ring_pair[!svc][tws_ip][id], VS_DISCONNECTED);
}

static void votfitf_crop_start_kunit_test(struct kunit *test)
{
	struct votf_dev *dev = pkt_ctx.mock_votf_dev[0];
	struct votf_info v_info;
	struct votf_table_info *trs_info;
	void *trs_addr;
	u32 svc, ip, id;
	int ret;

	/* TC #1. NULL votf_info structure */
	ret = votfitf_set_crop_start(NULL, true);

	KUNIT_EXPECT_LT(test, ret, 0);

	ret = votfitf_get_crop_start(NULL);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* Setup TRS */
	v_info.ip = __LINE__;
	svc = v_info.service = TRS;
	ip = id = 0;
	trs_info = &dev->votf_table[svc][ip][id];
	trs_info->ip = v_info.ip;
	trs_info->module = C2AGENT;
	trs_addr = dev->votf_addr[ip];

	/* TC #2. TRS, not in use state */
	trs_info->use = 0;

	ret = votfitf_set_crop_start(&v_info, true);

	KUNIT_EXPECT_LT(test, ret, 0);

	ret = votfitf_get_crop_start(&v_info);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #3. Set crop start */
	trs_info->use = 1;

	ret = votfitf_set_crop_start(&v_info, true);

	KUNIT_EXPECT_EQ(test, ret, 1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(trs_addr, c2agent_regs[C2AGENT_TRS_CROP_TOKENS_START]), (u32)1);

	ret = votfitf_get_crop_start(&v_info);

	KUNIT_EXPECT_EQ(test, ret, 1);
}

static void votfitf_crop_enable_kunit_test(struct kunit *test)
{
	struct votf_dev *dev = pkt_ctx.mock_votf_dev[0];
	struct votf_info v_info;
	struct votf_table_info *trs_info;
	void *trs_addr;
	u32 svc, ip, id;
	int ret;

	/* TC #1. NULL votf_info structure */
	ret = votfitf_set_crop_enable(NULL, true);

	KUNIT_EXPECT_LT(test, ret, 0);

	ret = votfitf_get_crop_enable(NULL);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* Setup TRS */
	v_info.ip = __LINE__;
	svc = v_info.service = TRS;
	ip = id = 0;
	trs_info = &dev->votf_table[svc][ip][id];
	trs_info->ip = v_info.ip;
	trs_info->module = C2AGENT;
	trs_addr = dev->votf_addr[ip];

	/* TC #2. TRS, not in use state */
	trs_info->use = 0;

	ret = votfitf_set_crop_enable(&v_info, true);

	KUNIT_EXPECT_LT(test, ret, 0);

	ret = votfitf_get_crop_enable(&v_info);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #3. Set crop start */
	trs_info->use = 1;

	ret = votfitf_set_crop_enable(&v_info, true);

	KUNIT_EXPECT_EQ(test, ret, 1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(trs_addr, c2agent_regs[C2AGENT_TRS_CROP_ENABLE]), (u32)1);

	ret = votfitf_get_crop_enable(&v_info);

	KUNIT_EXPECT_EQ(test, ret, 1);
}

static void votfitf_check_votf_ring_kunit_test(struct kunit *test)
{
	struct votf_dev *dev = pkt_ctx.mock_votf_dev[0];
	void *dev_addr;
	int module;
	bool en;

	dev_addr = dev->votf_addr[0];

	/* TC #1. C2SERV is enabled. */
	module = C2SERV;
	SET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_RING_CLK_EN], 1);
	SET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_RING_ENABLE], 1);

	en = votfitf_check_votf_ring(dev_addr, module);

	KUNIT_EXPECT_TRUE(test, en);

	/* TC #2. C2SERV is disabled. */
	SET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_RING_ENABLE], 0);

	en = votfitf_check_votf_ring(dev_addr, module);

	KUNIT_EXPECT_FALSE(test, en);

	/* TC #3. C2AGENT is enabled. */
	dev_addr = dev->votf_addr[1];
	module = C2AGENT;
	SET_CR_VAL(dev_addr, c2agent_regs[C2AGENT_R_C2COM_RING_CLK_EN], 1);
	SET_CR_VAL(dev_addr, c2agent_regs[C2AGENT_R_C2COM_RING_ENABLE], 1);

	en = votfitf_check_votf_ring(dev_addr, module);

	KUNIT_EXPECT_TRUE(test, en);

	/* TC #4. C2AGENT is disabled. */
	SET_CR_VAL(dev_addr, c2agent_regs[C2AGENT_R_C2COM_RING_ENABLE], 0);

	en = votfitf_check_votf_ring(dev_addr, module);

	KUNIT_EXPECT_FALSE(test, en);
}

static void votfitf_votf_create_ring_kunit_test(struct kunit *test)
{
	struct votf_dev *dev = pkt_ctx.mock_votf_dev[0];
	void *dev_addr;
	int module;
	int ip;

	/* TC #1 C2SERV */
	dev_addr = dev->votf_addr[0];
	ip = __LINE__;
	module = C2SERV;

	votfitf_votf_create_ring(dev_addr, ip, module);

	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_LOCAL_IP]), (u32)ip);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_RING_CLK_EN]), (u32)1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_C2COM_RING_ENABLE]), (u32)1);

	/* TC #2 C2AGENT */
	dev_addr = dev->votf_addr[1];
	ip = __LINE__;
	module = C2AGENT;

	votfitf_votf_create_ring(dev_addr, ip, module);

	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2agent_regs[C2AGENT_R_C2COM_LOCAL_IP]), (u32)ip);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2agent_regs[C2AGENT_R_C2COM_RING_CLK_EN]), (u32)1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2agent_regs[C2AGENT_R_C2COM_RING_ENABLE]), (u32)1);
}

static void votfitf_votf_set_sel_reg_kunit_test(struct kunit *test)
{
	struct votf_dev *dev = pkt_ctx.mock_votf_dev[0];
	void *dev_addr;
	u32 set, mode;

	dev_addr = dev->votf_addr[0];
	set = 1;
	mode = 1;

	votfitf_votf_set_sel_reg(dev_addr, set, mode);

	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_SELREGISTERMODE]), mode);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_SELREGISTER]), set);
}

static void votfitf_wrapper_reset_kunit_test(struct kunit *test)
{
	struct votf_dev *dev = pkt_ctx.mock_votf_dev[0];
	void *dev_addr;
	int ret;

	dev_addr = dev->votf_addr[0];

	ret = votfitf_wrapper_reset(dev_addr);

	KUNIT_EXPECT_GT(test, ret, 0);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, votf_wrapper_regs[VOTF_WRAPPER_SWRST]), (u32)0x11);
}

static void votfitf_check_wait_con_kunit_test(struct kunit *test)
{
	struct votf_dev *dev = pkt_ctx.mock_votf_dev[0];
	struct votf_info tws_info, trs_info;
	struct votf_table_info *tws_t_info, *trs_t_info;
	struct votf_debug_info *dbg_info;
	void *tws_addr, *trs_addr;
	int ret;
	u32 dbg_val, tws_dbg_out, trs_dbg_out;

	/* TC #1. NULL TWS info */
	ret = votfitf_check_wait_con(NULL, NULL);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* Setup TWS */
	tws_info.ip = __LINE__;
	tws_info.service = TWS;
	tws_t_info = &dev->votf_table[tws_info.service][0][0];
	tws_t_info->ip = tws_info.ip;
	tws_t_info->module = C2SERV;
	tws_addr = dev->votf_addr[0];
	tws_dbg_out = VOTF_TWS_WAIT_CON;
	SET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_C2COM_DEBUG_DOUT], tws_dbg_out);

	/* TC #2. NULL TRS info */
	ret = votfitf_check_wait_con(&tws_info, NULL);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* Setup TRS */
	trs_info.ip = __LINE__;
	trs_info.service = TRS;
	trs_t_info = &dev->votf_table[trs_info.service][1][0];
	trs_t_info->ip = trs_info.ip;
	trs_t_info->module = C2SERV;
	trs_addr = dev->votf_addr[1];
	trs_dbg_out = VOTF_TRS_WAIT_CON;
	SET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_C2COM_DEBUG_DOUT], trs_dbg_out);

	/* TC #3. TWS is not in use state. */
	tws_t_info->use = 0;

	ret = votfitf_check_wait_con(&tws_info, &trs_info);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #4. Check wait_con state */
	tws_t_info->use = 1;
	trs_t_info->use = 1;

	ret = votfitf_check_wait_con(&tws_info, &trs_info);

	KUNIT_EXPECT_EQ(test, ret, 0);

	/* Validate TWS debug state */
	dbg_val = 1;
	dbg_info = &tws_t_info->debug_info;

	KUNIT_EXPECT_GT(test, dbg_info->time, (unsigned long long)0);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_C2COM_DEBUG]), dbg_val);
	KUNIT_EXPECT_EQ(test, dbg_info->debug[0], dbg_val);
	KUNIT_EXPECT_EQ(test, dbg_info->debug[1], tws_dbg_out);

	/* Validate TRS debug state */
	dbg_val = 1 | (1 << 5);
	dbg_info = &trs_t_info->debug_info;

	KUNIT_EXPECT_GT(test, dbg_info->time, (unsigned long long)0);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_C2COM_DEBUG]), dbg_val);
	KUNIT_EXPECT_EQ(test, dbg_info->debug[0], dbg_val);
	KUNIT_EXPECT_EQ(test, dbg_info->debug[1], trs_dbg_out);

	/* Check rejection token */
	dbg_val = (trs_info.ip << 16) | (0x0a << 8);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_VOTF_PKT_DATA]), dbg_val);
}

static void votfitf_check_invalid_state_kunit_test(struct kunit *test)
{
	struct votf_dev *dev = pkt_ctx.mock_votf_dev[0];
	struct votf_info tws_info, trs_info;
	struct votf_table_info *tws_t_info, *trs_t_info;
	struct votf_debug_info *dbg_info;
	void *tws_addr, *trs_addr;
	int ret;
	u32 dbg_val, tws_dbg_out, trs_dbg_out;

	/* TC #1. NULL TWS info */
	ret = votfitf_check_invalid_state(NULL, NULL);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* Setup TWS */
	tws_info.ip = __LINE__;
	tws_info.service = TWS;
	tws_t_info = &dev->votf_table[tws_info.service][0][0];
	tws_t_info->ip = tws_info.ip;
	tws_t_info->module = C2SERV;
	tws_addr = dev->votf_addr[0];
	tws_dbg_out = VOTF_IDLE;
	SET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_C2COM_DEBUG_DOUT], tws_dbg_out);
	SET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_TWS_BUSY], 1);

	/* TC #2. NULL TRS info */
	ret = votfitf_check_invalid_state(&tws_info, NULL);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* Setup TRS */
	trs_info.ip = __LINE__;
	trs_info.service = TRS;
	trs_t_info = &dev->votf_table[trs_info.service][1][0];
	trs_t_info->ip = trs_info.ip;
	trs_t_info->module = C2SERV;
	trs_addr = dev->votf_addr[1];
	trs_dbg_out = VOTF_IDLE;
	SET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_C2COM_DEBUG_DOUT], trs_dbg_out);
	SET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_TRS_BUSY], 1);

	/* TC #3. TWS is not in use state. */
	tws_t_info->use = 0;

	ret = votfitf_check_invalid_state(&tws_info, &trs_info);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #4. Check wait_con state */
	tws_t_info->use = 1;
	trs_t_info->use = 1;

	ret = votfitf_check_invalid_state(&tws_info, &trs_info);

	KUNIT_EXPECT_EQ(test, ret, 0);

	/* Validate TWS state */
	dbg_val = 1;
	dbg_info = &tws_t_info->debug_info;

	KUNIT_EXPECT_GT(test, dbg_info->time, (unsigned long long)0);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_C2COM_DEBUG]), dbg_val);
	KUNIT_EXPECT_EQ(test, dbg_info->debug[0], dbg_val);
	KUNIT_EXPECT_EQ(test, dbg_info->debug[1], tws_dbg_out);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(tws_addr, c2serv_regs[C2SERV_R_TWS_FLUSH]), (u32)1);

	/* Validate TRS debug state */
	dbg_val = 1 | (1 << 5);
	dbg_info = &trs_t_info->debug_info;

	KUNIT_EXPECT_GT(test, dbg_info->time, (unsigned long long)0);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_C2COM_DEBUG]), dbg_val);
	KUNIT_EXPECT_EQ(test, dbg_info->debug[0], dbg_val);
	KUNIT_EXPECT_EQ(test, dbg_info->debug[1], trs_dbg_out);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_TRS_FLUSH]), (u32)1);
}

static void votfitf_set_trs_lost_cfg_kunit_test(struct kunit *test)
{
	struct votf_dev *dev = pkt_ctx.mock_votf_dev[0];
	struct votf_info trs_info;
	struct votf_table_info *t_info;
	struct votf_lost_cfg cfg;
	void *trs_addr;
	int ret;
	u32 ip, val;

	/* TC #1. NULL votf_info */
	ret = votfitf_set_trs_lost_cfg(NULL, NULL);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* Setup TRS */
	trs_info.ip = __LINE__;
	trs_info.service = TRS;
	ip = 0;
	t_info = &dev->votf_table[trs_info.service][ip][0];
	t_info->ip = trs_info.ip;
	trs_addr = dev->votf_addr[ip];

	/* TC #2. TRS is not in use state. */
	t_info->use = 0;

	ret = votfitf_set_trs_lost_cfg(&trs_info, &cfg);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #3. Set TRS lost_recover configuration. */
	t_info->use = 1;
	cfg.recover_enable = true;
	cfg.flush_enable = true;
	val = 1 | (1 << 1);

	ret = votfitf_set_trs_lost_cfg(&trs_info, &cfg);

	KUNIT_EXPECT_EQ(test, ret, 1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_TRS_CONNECTION_LOST_RECOVER_ENABLE]), val);
}

static void votfitf_set_first_token_size_kunit_test(struct kunit *test)
{
	struct votf_dev *dev = pkt_ctx.mock_votf_dev[0];
	struct votf_info tws_info, trs_info;
	struct votf_table_info *t_info;
	void *trs_addr;
	int ret;
	u32 ip, size;

	/* TC #1. NULL votf_info */
	ret = votfitf_set_first_token_size(NULL, 0);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* Setup TWS */
	tws_info.ip = __LINE__;
	tws_info.service = TWS;
	ip = 0;
	t_info = &dev->votf_table[tws_info.service][ip][0];
	t_info->ip = tws_info.ip;
	t_info->use = 1;

	/* Setup TRS */
	trs_info.ip = __LINE__;
	trs_info.service = TRS;
	ip = 1;
	t_info = &dev->votf_table[trs_info.service][ip][0];
	t_info->ip = trs_info.ip;
	trs_addr = dev->votf_addr[ip];

	/* TC #2. TRS is not in use state. */
	t_info->use = 0;

	ret = votfitf_set_first_token_size(&trs_info, 0);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #3. Set into TWS */
	ret = votfitf_set_first_token_size(&tws_info, 0);

	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC #4. Set 1st token size into TRS */
	t_info->use = 1;
	size = __LINE__;

	ret = votfitf_set_first_token_size(&trs_info, size);

	KUNIT_EXPECT_EQ(test, ret, 1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_TRS_LINES_IN_FIRST_TOKEN]), size);
}

static void votfitf_set_token_size_kunit_test(struct kunit *test)
{
	struct votf_dev *dev = pkt_ctx.mock_votf_dev[0];
	struct votf_info v_info;
	struct votf_table_info *t_info;
	void *dev_addr;
	int ret;
	u32 ip, size;

	/* TC #1. NULL votf_info */
	ret = votfitf_set_token_size(NULL, 0);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* Setup TWS */
	v_info.ip = __LINE__;
	v_info.service = TWS;
	ip = 0;
	t_info = &dev->votf_table[v_info.service][ip][0];
	t_info->ip = v_info.ip;
	dev_addr = dev->votf_addr[ip];

	/* TC #2. TWS is not in use state. */
	t_info->use = 0;

	ret = votfitf_set_token_size(&v_info, 0);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #3. Set token size into TWS */
	t_info->use = 1;
	size = __LINE__;

	ret = votfitf_set_token_size(&v_info, size);

	KUNIT_EXPECT_EQ(test, ret, 1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_regs[C2SERV_R_TWS_LINES_IN_TOKEN]), size);
}

static void votfitf_set_frame_size_kunit_test(struct kunit *test)
{
	struct votf_dev *dev = pkt_ctx.mock_votf_dev[0];
	struct votf_info tws_info, trs_info;
	struct votf_table_info *t_info;
	void *trs_addr;
	int ret;
	u32 ip, size;

	/* TC #1. NULL votf_info */
	ret = votfitf_set_frame_size(NULL, 0);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* Setup TWS */
	tws_info.ip = __LINE__;
	tws_info.service = TWS;
	ip = 0;
	t_info = &dev->votf_table[tws_info.service][ip][0];
	t_info->ip = tws_info.ip;
	t_info->use = 1;

	/* Setup TRS */
	trs_info.ip = __LINE__;
	trs_info.service = TRS;
	ip = 1;
	t_info = &dev->votf_table[trs_info.service][ip][0];
	t_info->ip = trs_info.ip;
	trs_addr = dev->votf_addr[ip];

	/* TC #2. TRS is not in use state. */
	t_info->use = 0;

	ret = votfitf_set_frame_size(&trs_info, 0);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #3. Set into TWS */
	ret = votfitf_set_frame_size(&tws_info, 0);

	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC #4. Set 1st token size into TRS */
	t_info->use = 1;
	size = __LINE__;

	ret = votfitf_set_frame_size(&trs_info, size);

	KUNIT_EXPECT_EQ(test, ret, 1);
	KUNIT_EXPECT_EQ(test, GET_CR_VAL(trs_addr, c2serv_regs[C2SERV_R_TRS_LINES_COUNT]), size);
}

static void votf_sfr_dump_kunit_test(struct kunit *test)
{
	struct votf_dev *dev = pkt_ctx.mock_votf_dev[0];
	struct votf_table_info *t_info;
	u32 module_type;

	t_info = &dev->votf_table[TWS][0][0];
	t_info->use = 1;

	/* TC #1. C2SERV */
	t_info->module = C2SERV;

	for (module_type = M0S4; module_type < MODULE_TYPE_CNT; module_type++) {
		t_info->module_type = module_type;

		votf_sfr_dump();
	}

	/* TC #2. C2SERV, M2S2 */
	t_info->module = C2AGENT;

	votf_sfr_dump();
}

static void votf_set_mcsc_hwfc_kunit_test(struct kunit *test)
{
	struct votf_dev *dev = pkt_ctx.mock_votf_dev[0];
	struct votf_info v_info;
	struct votf_table_info *t_info;
	void *dev_addr;
	int ret;
	u32 mcsc_out, cr_id, cr_offset;

	dev_addr = dev->votf_addr[0];
	cr_offset = C2SERV_R_M16S16_TWS1_ENABLE - C2SERV_R_M16S16_TWS0_ENABLE;

	/* TC #1. NULL votf_info */
	ret = votf_set_mcsc_hwfc(NULL, 0);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* Setup TWS */
	v_info.ip = __LINE__;
	v_info.service = TWS;
	t_info = &dev->votf_table[v_info.service][0][0];
	t_info->ip = v_info.ip;

	/* TC #2. Set MCSC_HWFC for valid MCSC output id */
	for (mcsc_out = 0; mcsc_out < 5; mcsc_out++) {
		cr_id = C2SERV_R_M16S16_TWS0_ENABLE + (cr_offset * 3 * mcsc_out);
		SET_CR_VAL(dev_addr, c2serv_m16s16_regs[cr_id], 0xFFFF);
		SET_CR_VAL(dev_addr, c2serv_m16s16_regs[cr_id + cr_offset], 0xFFFF);
		SET_CR_VAL(dev_addr, c2serv_m16s16_regs[cr_id + (cr_offset * 2)], 0xFFFF);

		votf_set_mcsc_hwfc(&v_info, mcsc_out);

		KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_m16s16_regs[cr_id]), (u32)0);
		KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_m16s16_regs[cr_id + cr_offset]), (u32)0);
		KUNIT_EXPECT_EQ(test, GET_CR_VAL(dev_addr, c2serv_m16s16_regs[cr_id + (cr_offset * 2)]), (u32)0);
	}

	/* TC #3. Set MCSC_HWFC with invalid MCSC output id */
	votf_set_mcsc_hwfc(&v_info, 10);
}
/* End of test case definition */

static void pablo_votf_kunit_test_init_votfdev(struct kunit *test)
{
	struct votf_dev *dev, *org_dev;
	u32 dev_id, ip;
	void *votf_addr;

	for (dev_id = 0; dev_id < VOTF_DEV_NUM; dev_id++) {
		org_dev = get_votf_dev(dev_id);
		/* For the 1st dev, just allocate it to run various TCs */
		if (dev_id && !org_dev)
			continue;

		dev = kunit_kzalloc(test, sizeof(struct votf_dev), 0);
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev);

		/* Set dummy VOTF addr region */
		for (ip = 0; ip < IP_MAX; ip++) {
			votf_addr = kunit_kzalloc(test, 0x10000, 0);
			KUNIT_ASSERT_NOT_ERR_OR_NULL(test, votf_addr);
			dev->votf_addr[ip] = votf_addr;
		}

		pkt_ctx.votf_dev[dev_id] = org_dev;
		pkt_ctx.mock_votf_dev[dev_id] = dev;

		set_votf_dev(dev_id, dev);
	}
}

static void pablo_votf_kunit_test_deinit_votfdev(struct kunit *test)
{
	struct votf_dev *dev;
	u32 dev_id, ip;
	void *votf_addr;

	for (dev_id = 0; dev_id < VOTF_DEV_NUM; dev_id++) {
		dev = pkt_ctx.mock_votf_dev[dev_id];
		if (!dev)
			continue;

		for (ip = 0; ip < IP_MAX; ip++) {
			votf_addr = dev->votf_addr[ip];
			if (!votf_addr)
				continue;

			kunit_kfree(test, votf_addr);
			dev->votf_addr[ip] = NULL;
		}

		set_votf_dev(dev_id, pkt_ctx.votf_dev[dev_id]);
		kunit_kfree(test, pkt_ctx.mock_votf_dev[dev_id]);

		pkt_ctx.votf_dev[dev_id] = NULL;
		pkt_ctx.mock_votf_dev[dev_id] = NULL;
	}
}

static int pablo_votf_kunit_test_init(struct kunit *test)
{
	pablo_votf_kunit_test_init_votfdev(test);

	return 0;
}

static void pablo_votf_kunit_test_exit(struct kunit *test)
{
	pablo_votf_kunit_test_deinit_votfdev(test);
}

static struct kunit_case pablo_votf_kunit_test_cases[] = {
	KUNIT_CASE(votfitf_init_kunit_test),
	KUNIT_CASE(votfitf_create_ring_kunit_test),
	KUNIT_CASE(votfitf_destroy_ring_kunit_test),
	KUNIT_CASE(votfitf_set_service_cfg_negative_kunit_test),
	KUNIT_CASE(votfitf_set_service_cfg_tws_kunit_test),
	KUNIT_CASE(votfitf_set_service_cfg_trs_kunit_test),
	KUNIT_CASE(votfitf_reset_kunit_test),
	KUNIT_CASE(votfitf_create_link_kunit_test),
	KUNIT_CASE(votfitf_destroy_link_kunit_test),
	KUNIT_CASE(votfitf_set_flush_kunit_test),
	KUNIT_CASE(votfitf_crop_start_kunit_test),
	KUNIT_CASE(votfitf_crop_enable_kunit_test),
	KUNIT_CASE(votfitf_check_votf_ring_kunit_test),
	KUNIT_CASE(votfitf_votf_create_ring_kunit_test),
	KUNIT_CASE(votfitf_votf_set_sel_reg_kunit_test),
	KUNIT_CASE(votfitf_wrapper_reset_kunit_test),
	KUNIT_CASE(votfitf_check_wait_con_kunit_test),
	KUNIT_CASE(votfitf_check_invalid_state_kunit_test),
	KUNIT_CASE(votfitf_set_trs_lost_cfg_kunit_test),
	KUNIT_CASE(votfitf_set_first_token_size_kunit_test),
	KUNIT_CASE(votfitf_set_token_size_kunit_test),
	KUNIT_CASE(votfitf_set_frame_size_kunit_test),
	KUNIT_CASE(votf_sfr_dump_kunit_test),
	KUNIT_CASE(votf_set_mcsc_hwfc_kunit_test),
	{},
};

struct kunit_suite pablo_votf_kunit_test_suite = {
	.name = "pablo-votf-kunit-test",
	.init = pablo_votf_kunit_test_init,
	.exit = pablo_votf_kunit_test_exit,
	.test_cases = pablo_votf_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_votf_kunit_test_suite);

MODULE_LICENSE("GPL");
