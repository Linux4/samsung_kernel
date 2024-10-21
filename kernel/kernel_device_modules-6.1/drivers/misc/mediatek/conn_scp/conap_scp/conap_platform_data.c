// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/types.h>
#include "conap_platform_data.h"

/* 6893 */
#if IS_ENABLED(CONFIG_MTK_COMBO_CHIP_CONSYS_6893)

struct conap_scp_shm_config g_adp_shm_mt6893 = {
	.conap_scp_shm_offset = 0x1E0000,
	.conap_scp_shm_size = 0x50000,
	.conap_scp_ipi_mbox_size = 40,
};
#endif
#if IS_ENABLED(CONFIG_MTK_COMBO_CHIP_CONSYS_6983)
struct conap_scp_shm_config g_adp_shm_mt6983 = {
	.conap_scp_shm_offset = 0x2100000,
	.conap_scp_shm_size = 0x20000,
	.conap_scp_ipi_mbox_size = 64,
};
#endif
#if IS_ENABLED(CONFIG_MTK_COMBO_CHIP_CONSYS_6895)
struct conap_scp_shm_config g_adp_shm_mt6895 = {
	.conap_scp_shm_offset = 0x2100000,
	.conap_scp_shm_size = 0x20000,
	.conap_scp_ipi_mbox_size = 64,
};
#endif
#if IS_ENABLED(CONFIG_MTK_COMBO_CHIP_CONSYS_6886)
struct conap_scp_shm_config g_adp_shm_mt6886 = {
	.conap_scp_shm_offset = 0,
	.conap_scp_shm_size = 0,
	.conap_scp_ipi_mbox_size = 64,
};
#endif

struct conap_scp_shm_config g_adp_shm;
struct conap_scp_batching_config g_adp_batching;
uint32_t g_conap_conn_state_enable = 1;

uint32_t connsys_scp_shm_get_addr(void)
{
	return 0;
}

uint32_t connsys_scp_shm_get_size(void)
{
	return 0;
}

uint32_t connsys_scp_ipi_mbox_size(void)
{
	return g_adp_shm.conap_scp_ipi_mbox_size;
}

phys_addr_t connsys_scp_shm_get_batching_addr(void)
{
	return (g_adp_batching.buff_offset & 0xFFFFFFFF);
}

uint32_t connsys_scp_shm_get_batching_size(void)
{
	return g_adp_batching.buff_size;
}

uint32_t connsys_scp_conn_state_en(void)
{
	return g_conap_conn_state_enable;
}

int connsys_scp_plt_data_init(struct platform_device *pdev)
{
	int ret;
	struct device_node *node;
	u32 value;
	u32 ipi_mbox_size = 0;
	u32 batching_buf_sz = 0;
	u64 value64 = 0, batching_buf_addr = 0;

	node = pdev->dev.of_node;

	/* ipi mbox setting */
	ret = of_property_read_u32(node, "ipi-mbox-size", &value);
	if (ret < 0)
		pr_notice("[%s] prop ipi-mbox-size fail %d", __func__, ret);
	else
		ipi_mbox_size = value;

	/* report location setting */
	ret = of_property_read_u32(node, "report-buf-size", &value);
	if (ret < 0)
		pr_notice("[%s] prop batching-buf-size fail %d", __func__, ret);
	else
		batching_buf_sz = value;

	ret = of_property_read_u64(node, "report-buf-addr", &value64);
	if (ret < 0)
		pr_notice("[%s] prop batching-buf-addr fail %d", __func__, ret);
	else
		batching_buf_addr = value64;

	pr_info("[%s] ipi_mbox_size=[%x] batching=[%x][%llx]", __func__,
			ipi_mbox_size, batching_buf_sz, batching_buf_addr);

	/* connsys statue update */
	ret = of_property_read_u32(node, "conn-state-enabled", &value);
	if (ret < 0)
		pr_notice("[%s] conn-state-enabled fail %d", __func__, ret);
	else
		g_conap_conn_state_enable = value;

	memset(&g_adp_shm, 0, sizeof(g_adp_shm));
	memset(&g_adp_batching, 0, sizeof(g_adp_batching));

	g_adp_shm.conap_scp_ipi_mbox_size = ipi_mbox_size;
	g_adp_batching.buff_offset = batching_buf_addr;
	g_adp_batching.buff_size = batching_buf_sz;

	return 0;
}
