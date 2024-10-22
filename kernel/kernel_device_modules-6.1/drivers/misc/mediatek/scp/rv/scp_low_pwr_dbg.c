// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include "scp_mbrain_dbg.h"
#include "scp_low_pwr_dbg.h"
#include "scp_dvfs.h"
#include "scp.h"
#include "scp_helper.h"

struct scp_res_mbrain_header header;
struct scp_res_info scp_res;
struct sys_wlock_info scp_wlock;

static void get_scp_res_header(void)
{
	header.mbrain_module = SCP_RES_DATA_MODULE_ID;
	header.version = SCP_RES_DATA_VERSION;
	header.data_offset = sizeof(struct scp_res_mbrain_header);
	header.index_data_length = sizeof(struct res_duration_t)*3 +
							sizeof(struct wlock_duration_t)*3 + header.data_offset;
	pr_notice("res_duration_t size=%lx, wlock_duration_t size=%lx\n",
			sizeof(struct res_duration_t), sizeof(struct wlock_duration_t));
}

static void *scp_res_data_copy(void *dest, void *src, uint64_t size)
{
	memcpy(dest, src, size);
	dest += size;
	return dest;
}

static int scp_mbrain_get_sys_res_data(void *address, uint32_t size)
{
	struct wlock_duration_t wlock[SCP_MAX_CORE_NUM];
	struct res_duration_t res[SCP_MAX_CORE_NUM];
	struct slp_ack_t *addr[SCP_MAX_CORE_NUM];
	uint64_t suspend_time = 0;
	int i;

	pr_notice("[SCP] %s start\n",__func__);
	addr[SCP_CORE_0] = (struct slp_ack_t *)scp_get_reserve_mem_virt(SCP_LOW_PWR_DBG_MEM_ID);

	for(i = 0; i < scpreg.core_nums; i++) {
		if (!addr[i]) {
			pr_notice("addr[%d]=null\n",i);
			memset(&wlock[i], 0, sizeof(struct wlock_duration_t));
			memset(&res[i], 0, sizeof(struct res_duration_t));
		} else {
			wlock[i] = addr[i]->wlock;
			res[i] = addr[i]->res;
			suspend_time = addr[i]->suspend_time;
		}
	}

	pr_notice("Prepare header\n");
	/* cpy header */
	get_scp_res_header();
	address = scp_res_data_copy(address, &header, sizeof(struct scp_res_mbrain_header));
	pr_notice("Prepare data\n");
	/* cpy res data */
	address = scp_res_data_copy(address, &res[SCP_CORE_0], sizeof(struct res_duration_t));
	address = scp_res_data_copy(address, &res[SCP_CORE_1], sizeof(struct res_duration_t));
	address = scp_res_data_copy(address, &res[SCP_CORE_2], sizeof(struct res_duration_t));
	/* cpy wakelock data */
	address = scp_res_data_copy(address, &wlock[SCP_CORE_0], sizeof(struct wlock_duration_t));
	address = scp_res_data_copy(address, &wlock[SCP_CORE_1], sizeof(struct wlock_duration_t));
	address = scp_res_data_copy(address, &wlock[SCP_CORE_2], sizeof(struct wlock_duration_t));
	pr_notice("[SCP] %s end\n",__func__);
	return 0;
}

static unsigned int scp_mbrain_get_sys_res_length(void)
{
	get_scp_res_header();
	return header.index_data_length;
}

static struct scp_res_mbrain_dbg_ops scp_res_mbrain_ops = {
	.get_length = scp_mbrain_get_sys_res_length,
	.get_data = scp_mbrain_get_sys_res_data,
};

int scp_sys_res_mbrain_plat_init (void)
{
	return register_scp_mbrain_dbg_ops(&scp_res_mbrain_ops);
}
