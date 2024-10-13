/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __HW_FENCE_DRV_UTILS_H
#define __HW_FENCE_DRV_UTILS_H

/**
 * HW_FENCE_MAX_CLIENT_TYPE_STATIC:
 * Total number of client types without configurable number of sub-clients (GFX, DPU, VAL, IPE, VPU)
 */
#define HW_FENCE_MAX_CLIENT_TYPE_STATIC 5

/**
 * HW_FENCE_MAX_CLIENT_TYPE_CONFIGURABLE:
 * Maximum number of client types with configurable number of sub-clients (e.g. IFE)
 */
#define HW_FENCE_MAX_CLIENT_TYPE_CONFIGURABLE 8

/**
 * HW_FENCE_MAX_STATIC_CLIENTS_INDEX:
 * Maximum number of static clients, i.e. clients without configurable numbers of sub-clients
 */
#define HW_FENCE_MAX_STATIC_CLIENTS_INDEX HW_FENCE_CLIENT_ID_IFE0

/**
 * enum hw_fence_mem_reserve - Types of reservations for the carved-out memory.
 * HW_FENCE_MEM_RESERVE_CTRL_QUEUE: Reserve memory for the ctrl rx/tx queues.
 * HW_FENCE_MEM_RESERVE_LOCKS_REGION: Reserve memory for the per-client locks memory region.
 * HW_FENCE_MEM_RESERVE_TABLE: Reserve memory for the hw-fences global table.
 * HW_FENCE_MEM_RESERVE_CLIENT_QUEUE: Reserve memory per-client for the rx/tx queues.
 */
enum hw_fence_mem_reserve {
	HW_FENCE_MEM_RESERVE_CTRL_QUEUE,
	HW_FENCE_MEM_RESERVE_LOCKS_REGION,
	HW_FENCE_MEM_RESERVE_TABLE,
	HW_FENCE_MEM_RESERVE_CLIENT_QUEUE
};

/**
 * struct hw_fence_client_type_desc - Structure holding client type properties, including static
 *                                    properties and client queue properties read from device-tree.
 *
 * @name: name of client type, used to parse properties from device-tree
 * @init_id: initial client_id for given client type within the 'hw_fence_client_id' enum, e.g.
 *           HW_FENCE_CLIENT_ID_CTL0 for DPU clients
 * @max_clients_num: maximum number of clients of given client type
 * @clients_num: number of clients of given client type
 * @queues_num: number of queues per client of given client type; either one (for only Tx Queue) or
 *              two (for both Tx and Rx Queues)
 * @queue_entries: number of entries per client queue of given client type
 * @mem_size: size of memory allocated for client queue(s) per client
 * @skip_txq_wr_idx: bool to indicate if update to tx queue write_index is skipped within hw fence
 *                   driver and hfi_header->tx_wm is updated instead
 */
struct hw_fence_client_type_desc {
	char *name;
	enum hw_fence_client_id init_id;
	u32 max_clients_num;
	u32 clients_num;
	u32 queues_num;
	u32 queue_entries;
	u32 mem_size;
	bool skip_txq_wr_idx;
};

/**
 * global_atomic_store() - Inter-processor lock
 * @drv_data: hw fence driver data
 * @lock: memory to lock
 * @val: if true, api locks the memory, if false it unlocks the memory
 */
void global_atomic_store(struct hw_fence_driver_data *drv_data, uint64_t *lock, bool val);

/**
 * hw_fence_utils_init_virq() - Initialilze doorbell (i.e. vIRQ) for SVM to HLOS signaling
 * @drv_data: hw fence driver data
 *
 * Returns zero if success, otherwise returns negative error code.
 */
int hw_fence_utils_init_virq(struct hw_fence_driver_data *drv_data);

/**
 * hw_fence_utils_process_doorbell_mask() - Sends doorbell mask to process the signaled clients
 *                                          this API is only exported for simulation purposes.
 * @drv_data: hw fence driver data.
 * @db_flags: doorbell flag
 */
void hw_fence_utils_process_doorbell_mask(struct hw_fence_driver_data *drv_data, u64 db_flags);

/**
 * hw_fence_utils_alloc_mem() - Allocates the carved-out memory pool that will be used for the HW
 *                              Fence global table, locks and queues.
 * @hw_fence_drv_data: hw fence driver data
 *
 * Returns zero if success, otherwise returns negative error code.
 */
int hw_fence_utils_alloc_mem(struct hw_fence_driver_data *hw_fence_drv_data);

/**
 * hw_fence_utils_reserve_mem() - Reserves memory from the carved-out memory pool.
 * @drv_data: hw fence driver data.
 * @type: memory reservation type.
 * @phys: physical address of the carved-out memory pool
 *
 * Returns zero if success, otherwise returns negative error code.
 */
int hw_fence_utils_reserve_mem(struct hw_fence_driver_data *drv_data,
	enum hw_fence_mem_reserve type, phys_addr_t *phys, void **pa, u32 *size, int client_id);

/**
 * hw_fence_utils_parse_dt_props() -  Init dt properties
 * @drv_data: hw fence driver data
 *
 * Returns zero if success, otherwise returns negative error code.
 */
int hw_fence_utils_parse_dt_props(struct hw_fence_driver_data *drv_data);

/**
 * hw_fence_utils_map_ipcc() -  Maps IPCC registers and enable signaling
 * @drv_data: hw fence driver data
 *
 * Returns zero if success, otherwise returns negative error code.
 */
int hw_fence_utils_map_ipcc(struct hw_fence_driver_data *drv_data);

/**
 * hw_fence_utils_map_qtime() -  Maps qtime register
 * @drv_data: hw fence driver data
 *
 * Returns zero if success, otherwise returns negative error code.
 */
int hw_fence_utils_map_qtime(struct hw_fence_driver_data *drv_data);

/**
 * hw_fence_utils_map_ctl_start() -  Maps ctl_start registers from dpu hw
 * @drv_data: hw fence driver data
 *
 * Returns zero if success, otherwise returns negative error code. This API is only used
 * for simulation purposes in platforms where dpu does not support ipc signal.
 */
int hw_fence_utils_map_ctl_start(struct hw_fence_driver_data *drv_data);

/**
 * hw_fence_utils_cleanup_fence() -  Cleanup the hw-fence from a specified client
 * @drv_data: hw fence driver data
 * @hw_fence_client: client, for which the fence must be cleared
 * @hw_fence: hw-fence to cleanup
 * @hash: hash of the hw-fence to cleanup
 * @reset_flags: flags to determine how to handle the reset
 *
 * Returns zero if success, otherwise returns negative error code.
 */
int hw_fence_utils_cleanup_fence(struct hw_fence_driver_data *drv_data,
	struct msm_hw_fence_client *hw_fence_client, struct msm_hw_fence *hw_fence, u64 hash,
	u32 reset_flags);

/**
 * hw_fence_utils_get_client_id_priv() - Gets the index into clients struct within hw fence driver
 *                                       from the client_id used externally
 *
 * Performs a 1-to-1 mapping for all client IDs less than HW_FENCE_MAX_STATIC_CLIENTS_INDEX,
 * otherwise consolidates client IDs of clients with configurable number of sub-clients. Fails if
 * provided with client IDs for such clients when support for those clients is not configured in
 * device-tree.
 *
 * @drv_data: hw fence driver data
 * @client_id: external client_id to get internal client_id for
 *
 * Returns client_id < drv_data->clients_num if success, otherwise returns HW_FENCE_CLIENT_MAX
 */
enum hw_fence_client_id hw_fence_utils_get_client_id_priv(struct hw_fence_driver_data *drv_data,
	enum hw_fence_client_id client_id);

/**
 * hw_fence_utils_skips_txq_wr_index() - Returns bool to indicate if client Tx Queue write_index
 *                                       is not updated in hw fence driver. Instead,
 *                                       hfi_header->tx_wm tracks where payload is written within
 *                                       the queue.
 *
 * @drv_data: driver data
 * @client_id: hw fence driver client id
 *
 * Returns: true if hw fence driver skips update to client tx queue write_index, false otherwise
 */
bool hw_fence_utils_skips_txq_wr_idx(struct hw_fence_driver_data *drv_data, int client_id);

#endif /* __HW_FENCE_DRV_UTILS_H */
