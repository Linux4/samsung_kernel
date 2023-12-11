// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "hw_fence_drv_priv.h"
#include "hw_fence_drv_utils.h"
#include "hw_fence_drv_ipc.h"
#include "hw_fence_drv_debug.h"

/**
 * struct hw_fence_client_ipc_map - map client id with ipc signal for trigger.
 * @ipc_client_id: ipc client id for the hw-fence client.
 * @ipc_signal_id: ipc signal id for the hw-fence client.
 * @update_rxq: bool to indicate if clinet uses rx-queue.
 */
struct hw_fence_client_ipc_map {
	int ipc_client_id;
	int ipc_signal_id;
	bool update_rxq;
};

/**
 * struct hw_fence_clients_ipc_map_no_dpu - Table makes the 'client to signal' mapping, which
 *		is used by the hw fence driver to trigger ipc signal when the hw fence is already
 *		signaled.
 *		This no_dpu version is for targets that do not support dpu client id
 *
 * Notes:
 * The index of this struct must match the enum hw_fence_client_id.
 * To change to a loopback signal instead of GMU, change ctx0 row to use:
 *   {HW_FENCE_IPC_CLIENT_ID_APPS, 20}.
 */
struct hw_fence_client_ipc_map hw_fence_clients_ipc_map_no_dpu[HW_FENCE_CLIENT_MAX] = {
	{HW_FENCE_IPC_CLIENT_ID_APPS, 1, true}, /* ctrl queue loopback */
	{HW_FENCE_IPC_CLIENT_ID_GPU,  0, true}, /* ctx0 */
	{HW_FENCE_IPC_CLIENT_ID_APPS, 14, false}, /* ctl0 */
	{HW_FENCE_IPC_CLIENT_ID_APPS, 15, false}, /* ctl1 */
	{HW_FENCE_IPC_CLIENT_ID_APPS, 16, false}, /* ctl2 */
	{HW_FENCE_IPC_CLIENT_ID_APPS, 17, false}, /* ctl3 */
	{HW_FENCE_IPC_CLIENT_ID_APPS, 18, false}, /* ctl4 */
	{HW_FENCE_IPC_CLIENT_ID_APPS, 19, false}, /* ctl5 */
};

/**
 * struct hw_fence_clients_ipc_map - Table makes the 'client to signal' mapping, which is
 *		used by the hw fence driver to trigger ipc signal when hw fence is already
 *		signaled.
 *		This version is for targets that support dpu client id.
 *
 * Note that the index of this struct must match the enum hw_fence_client_id
 */
struct hw_fence_client_ipc_map hw_fence_clients_ipc_map[HW_FENCE_CLIENT_MAX] = {
	{HW_FENCE_IPC_CLIENT_ID_APPS, 1, true}, /* ctrl queue loopback */
	{HW_FENCE_IPC_CLIENT_ID_GPU,  0, true}, /* ctx0 */
	{HW_FENCE_IPC_CLIENT_ID_DPU,  0, false}, /* ctl0 */
	{HW_FENCE_IPC_CLIENT_ID_DPU,  1, false}, /* ctl1 */
	{HW_FENCE_IPC_CLIENT_ID_DPU,  2, false}, /* ctl2 */
	{HW_FENCE_IPC_CLIENT_ID_DPU,  3, false}, /* ctl3 */
	{HW_FENCE_IPC_CLIENT_ID_DPU,  4, false}, /* ctl4 */
	{HW_FENCE_IPC_CLIENT_ID_DPU,  5, false}, /* ctl5 */
};

int hw_fence_ipcc_get_client_id(struct hw_fence_driver_data *drv_data, u32 client_id)
{
	if (!drv_data || client_id >= HW_FENCE_CLIENT_MAX)
		return -EINVAL;

	return drv_data->ipc_clients_table[client_id].ipc_client_id;
}

int hw_fence_ipcc_get_signal_id(struct hw_fence_driver_data *drv_data, u32 client_id)
{
	if (!drv_data || client_id >= HW_FENCE_CLIENT_MAX)
		return -EINVAL;

	return drv_data->ipc_clients_table[client_id].ipc_signal_id;
}

bool hw_fence_ipcc_needs_rxq_update(struct hw_fence_driver_data *drv_data, int client_id)
{
	if (!drv_data || client_id >= HW_FENCE_CLIENT_MAX)
		return -EINVAL;

	return drv_data->ipc_clients_table[client_id].update_rxq;
}

/**
 * _get_ipc_client_name() - Returns ipc client name, used for debugging.
 */
static inline char *_get_ipc_client_name(u32 client_id)
{
	switch (client_id) {
	case HW_FENCE_IPC_CLIENT_ID_APPS:
		return "APPS";
	case HW_FENCE_IPC_CLIENT_ID_GPU:
		return "GPU";
	case HW_FENCE_IPC_CLIENT_ID_DPU:
		return "DPU";
	}

	return "UNKNOWN";
}

void hw_fence_ipcc_trigger_signal(struct hw_fence_driver_data *drv_data,
	u32 tx_client_id, u32 rx_client_id, u32 signal_id)
{
	void __iomem *ptr;
	u32 val;

	/* Send signal */
	ptr = IPC_PROTOCOLp_CLIENTc_SEND(drv_data->ipcc_io_mem, drv_data->protocol_id,
		tx_client_id);
	val = (rx_client_id << 16) | signal_id;

	HWFNC_DBG_IRQ("Sending ipcc from %s (%d) to %s (%d) signal_id:%d [wr:0x%x to off:0x%pK]\n",
		_get_ipc_client_name(tx_client_id), tx_client_id,
		_get_ipc_client_name(rx_client_id), rx_client_id,
		signal_id, val, ptr);
	HWFNC_DBG_H("Write:0x%x to RegOffset:0x%pK\n", val, ptr);
	writel_relaxed(val, ptr);

	/* Make sure value is written */
	wmb();
}

/**
 * _hw_fence_ipcc_hwrev_init() - Initializes internal driver struct with corresponding ipcc data,
 *		according to the ipcc hw revision.
 * @drv_data: driver data.
 * @hwrev: ipcc hw revision.
 */
static int _hw_fence_ipcc_hwrev_init(struct hw_fence_driver_data *drv_data, u32 hwrev)
{
	switch (hwrev) {
	case HW_FENCE_IPCC_HW_REV_100:
		drv_data->ipcc_client_id = HW_FENCE_IPC_CLIENT_ID_APPS;
		drv_data->protocol_id = HW_FENCE_IPC_COMPUTE_L1_PROTOCOL_ID_LAHAINA;
		drv_data->ipc_clients_table = hw_fence_clients_ipc_map_no_dpu;
		HWFNC_DBG_INIT("ipcc protocol_id: Lahaina\n");
		break;
	case HW_FENCE_IPCC_HW_REV_110:
		drv_data->ipcc_client_id = HW_FENCE_IPC_CLIENT_ID_APPS;
		drv_data->protocol_id = HW_FENCE_IPC_COMPUTE_L1_PROTOCOL_ID_WAIPIO;
		drv_data->ipc_clients_table = hw_fence_clients_ipc_map_no_dpu;
		HWFNC_DBG_INIT("ipcc protocol_id: Waipio\n");
		break;
	case HW_FENCE_IPCC_HW_REV_170:
		drv_data->ipcc_client_id = HW_FENCE_IPC_CLIENT_ID_APPS;
		drv_data->protocol_id = HW_FENCE_IPC_COMPUTE_L1_PROTOCOL_ID_KAILUA;
		drv_data->ipc_clients_table = hw_fence_clients_ipc_map;
		HWFNC_DBG_INIT("ipcc protocol_id: Kailua\n");
		break;
	default:
		return -1;
	}

	return 0;
}

int hw_fence_ipcc_enable_signaling(struct hw_fence_driver_data *drv_data)
{
	void __iomem *ptr;
	u32 val;

	HWFNC_DBG_H("enable ipc +\n");

	/* Read IPC Version from Client=0x8 (apps) for protocol=2 (compute_l1) */
	val = readl_relaxed(IPC_PROTOCOLp_CLIENTc_VERSION(drv_data->ipcc_io_mem,
		HW_FENCE_IPC_COMPUTE_L1_PROTOCOL_ID_LAHAINA, HW_FENCE_IPC_CLIENT_ID_APPS));
	HWFNC_DBG_INIT("ipcc version:0x%x\n", val);

	if (_hw_fence_ipcc_hwrev_init(drv_data, val)) {
		HWFNC_ERR("ipcc protocol id not supported\n");
		return -EINVAL;
	}

	/* Enable compute l1 (protocol_id = 2) */
	val = 0x00000000;
	ptr = IPC_PROTOCOLp_CLIENTc_CONFIG(drv_data->ipcc_io_mem, drv_data->protocol_id,
		HW_FENCE_IPC_CLIENT_ID_APPS);
	HWFNC_DBG_H("Write:0x%x to RegOffset:0x%pK\n", val, ptr);
	writel_relaxed(val, ptr);

	/* Enable Client-Signal pairs from APPS(NS) (0x8) to APPS(NS) (0x8) */
	val = 0x000080000;
	ptr = IPC_PROTOCOLp_CLIENTc_RECV_SIGNAL_ENABLE(drv_data->ipcc_io_mem, drv_data->protocol_id,
		HW_FENCE_IPC_CLIENT_ID_APPS);
	HWFNC_DBG_H("Write:0x%x to RegOffset:0x%pK\n", val, ptr);
	writel_relaxed(val, ptr);

	HWFNC_DBG_H("enable ipc -\n");

	return 0;
}

#ifdef HW_DPU_IPCC
int hw_fence_ipcc_enable_dpu_signaling(struct hw_fence_driver_data *drv_data)
{
	struct hw_fence_client_ipc_map *hw_fence_client;
	void __iomem *ptr;
	u32 val;
	int i;

	HWFNC_DBG_H("enable dpu ipc +\n");

	if (!drv_data || !drv_data->protocol_id || !drv_data->ipc_clients_table) {
		HWFNC_ERR("invalid drv data\n");
		return -1;
	}

	HWFNC_DBG_H("ipcc_io_mem:0x%lx\n", (u64)drv_data->ipcc_io_mem);

	/*
	 * Enable compute l1 (protocol_id = 2) for dpu (25)
	 * Sets bit(1) to clear when RECV_ID is read
	 */
	val = 0x00000001;
	ptr = IPC_PROTOCOLp_CLIENTc_CONFIG(drv_data->ipcc_io_mem, drv_data->protocol_id,
		HW_FENCE_IPC_CLIENT_ID_DPU);
	HWFNC_DBG_H("Write:0x%x to RegOffset:0x%lx\n", val, (u64)ptr);
	writel_relaxed(val, ptr);

	HWFNC_DBG_H("Initialize dpu signals\n");
	/* Enable Client-Signal pairs from DPU (25) to APPS(NS) (8) */
	for (i = 0; i < HW_FENCE_CLIENT_MAX; i++) {
		hw_fence_client = &drv_data->ipc_clients_table[i];

		/* skip any client that is not a dpu client */
		if (hw_fence_client->ipc_client_id != HW_FENCE_IPC_CLIENT_ID_DPU)
			continue;

		/* Enable signals for dpu client */
		HWFNC_DBG_H("dpu:%d client:%d signal:%d\n", hw_fence_client->ipc_client_id, i,
			hw_fence_client->ipc_signal_id);
		val = 0x000080000 | (hw_fence_client->ipc_signal_id & 0xFFFF);
		ptr = IPC_PROTOCOLp_CLIENTc_RECV_SIGNAL_ENABLE(drv_data->ipcc_io_mem,
			drv_data->protocol_id, HW_FENCE_IPC_CLIENT_ID_DPU);
		HWFNC_DBG_H("Write:0x%x to RegOffset:0x%lx\n", val, (u64)ptr);
		writel_relaxed(val, ptr);
	}

	HWFNC_DBG_H("enable dpu ipc -\n");

	return 0;
}
#endif /* HW_DPU_IPCC */
