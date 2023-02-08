/**
@file		link_device_mem_define.h
@brief		definitions (structure, macro, etc) for memory-type interface
@date		2014/02/05
@author		Hankook Jang (hankook.jang@samsung.com)
*/

/*
 * Copyright (C) 2010 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MODEM_LINK_DEVICE_MEM_DEFINE_H__
#define __MODEM_LINK_DEVICE_MEM_DEFINE_H__

#include "link_device_memory_config.h"
#include "include/circ_queue.h"

#ifdef GROUP_MEM_TYPE
/**
@addtogroup group_mem_type
@{
*/

enum mem_iface_type {
	MEM_EXT_DPRAM = 0x0001,	/* External DPRAM */
	MEM_AP_IDPRAM = 0x0002,	/* DPRAM in AP    */
	MEM_CP_IDPRAM = 0x0004,	/* DPRAM in CP    */
	MEM_PLD_DPRAM = 0x0008,	/* PLD or FPGA    */
	MEM_SYS_SHMEM = 0x0100,	/* Shared-memory (SHMEM) on a system bus   */
	MEM_C2C_SHMEM = 0x0200,	/* SHMEM with C2C (Chip-to-chip) interface */
	MEM_LLI_SHMEM = 0x0400,	/* SHMEM with MIPI-LLI interface           */
};

#define MEM_DPRAM_TYPE_MASK	0x00FF
#define MEM_SHMEM_TYPE_MASK	0xFF00

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_TYPE_SHMEM
/**
@addtogroup group_mem_type_shmem
@{
*/

#define SHM_4M_RESERVED_SZ	4056
#define SHM_4M_FMT_TX_BUFF_SZ	4096
#define SHM_4M_FMT_RX_BUFF_SZ	4096
#define SHM_4M_RAW_TX_BUFF_SZ	2084864
#define SHM_4M_RAW_RX_BUFF_SZ	2097152

#define SHM_UL_USAGE_LIMIT	SZ_32K	/* Uplink burst limit */

struct __packed shmem_4mb_phys_map {
	u32 magic;
	u32 access;

	u32 fmt_tx_head;
	u32 fmt_tx_tail;

	u32 fmt_rx_head;
	u32 fmt_rx_tail;

	u32 raw_tx_head;
	u32 raw_tx_tail;

	u32 raw_rx_head;
	u32 raw_rx_tail;

	char reserved[SHM_4M_RESERVED_SZ];

	char fmt_tx_buff[SHM_4M_FMT_TX_BUFF_SZ];
	char fmt_rx_buff[SHM_4M_FMT_RX_BUFF_SZ];

	char raw_tx_buff[SHM_4M_RAW_TX_BUFF_SZ];
	char raw_rx_buff[SHM_4M_RAW_RX_BUFF_SZ];
};

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_INTERRUPT
/**
@addtogroup group_mem_link_interrupt
@{
*/

#define MASK_INT_VALID		0x0080

#define MASK_CMD_VALID		0x0040
#define MASK_CMD_FIELD		0x003F

#define MASK_REQ_ACK_FMT	0x0020
#define MASK_REQ_ACK_RAW	0x0010
#define MASK_RES_ACK_FMT	0x0008
#define MASK_RES_ACK_RAW	0x0004
#define MASK_SEND_FMT		0x0002
#define MASK_SEND_RAW		0x0001

#define CMD_INIT_START		0x0001
#define CMD_INIT_END		0x0002
#define CMD_REQ_ACTIVE		0x0003
#define CMD_RES_ACTIVE		0x0004
#define CMD_REQ_TIME_SYNC	0x0005
#define CMD_CRASH_RESET		0x0007
#define CMD_PHONE_START		0x0008
#define CMD_CRASH_EXIT		0x0009
#define CMD_CP_DEEP_SLEEP	0x000A
#define CMD_NV_REBUILDING	0x000B
#define CMD_EMER_DOWN		0x000C
#define CMD_PIF_INIT_DONE	0x000D
#define CMD_SILENT_NV_REBUILD	0x000E
#define CMD_NORMAL_POWER_OFF	0x000F

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_IPC_DEVICE
/**
@addtogroup group_mem_ipc_device
@{
*/

/**
@brief		the structure for each logical IPC device (i.e. virtual lane) in
		a memory-type interface media
*/
struct mem_ipc_device {
	enum dev_format id;
	char name[16];

	struct circ_queue txq;
	struct circ_queue rxq;

	u16 msg_mask;
	u16 req_ack_mask;
	u16 res_ack_mask;

	spinlock_t *tx_lock;
	struct sk_buff_head *skb_txq;

	spinlock_t *rx_lock;
	struct sk_buff_head *skb_rxq;

	unsigned int req_ack_cnt[MAX_DIR];
};

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_FLOW_CONTROL
/**
@addtogroup group_mem_flow_control
@{
*/

#define MAX_SKB_TXQ_DEPTH		1024

#define TX_PERIOD_MS			1		/* 1 ms */
#define MAX_TX_BUSY_COUNT		(1024 << 4)	/* 16384 ms */
#define BUSY_COUNT_MASK			0x7F		/* at every 128 ms */

#define RES_ACK_WAIT_TIMEOUT		10		/* 10 ms */

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_CP_CRASH
/**
@addtogroup group_mem_cp_crash
@{
*/

#define FORCE_CRASH_ACK_TIMEOUT		(10 * HZ)

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_SNAPSHOT
/**
@addtogroup group_mem_link_snapshot
@{
*/

struct __packed mem_snapshot {
	/* Timestamp */
	struct timespec ts;

	/* The status of memory interface at the time */
	unsigned int magic;
	unsigned int access;

	unsigned int head[MAX_SIPC5_DEV][MAX_DIR];
	unsigned int tail[MAX_SIPC5_DEV][MAX_DIR];

	u16 int2ap;
	u16 int2cp;

	/* Direction (TX or RX) */
	enum direction dir;
};

/**
@brief		Memory snapshot (MST) buffer
*/
struct mst_buff {
	/* These two members must be first. */
	struct mst_buff *next;
	struct mst_buff *prev;

	struct mem_snapshot snapshot;
};

struct mst_buff_head {
	/* These two members must be first. */
	struct mst_buff *next;
	struct mst_buff	*prev;

	u32 qlen;
	spinlock_t lock;
};

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_DEVICE
/**
@addtogroup group_mem_link_device
@{
*/

/**
@brief		the structure for a memory-type link device
*/
struct mem_link_device {
	/**
	 * COMMON and MANDATORY to all link devices
	 */
	struct link_device link_dev;

	/**
	 * Global lock for a mem_link_device instance
	 */
	spinlock_t lock;

	/**
	 * MEMORY type
	 */
	enum mem_iface_type type;

	/**
	 * {physical address, size, virtual address} for BOOT region
	 */
	unsigned long boot_start;
	unsigned long boot_size;
	char __iomem *boot_base;

	/**
	 * {physical address, size, virtual address} for IPC region
	 */
	unsigned long start;
	unsigned long size;
	char __iomem *base;

	/**
	 * Actual logical IPC devices (for IPC_FMT and IPC_RAW)
	 */
	struct mem_ipc_device ipc_dev[MAX_SIPC5_DEV];

	/**
	 * Pointers (aliases) to IPC device map
	 */
	u32 __iomem *magic;
	u32 __iomem *access;
	struct mem_ipc_device *dev[MAX_SIPC5_DEV];

	/**
	 * GPIO#, MBOX#, IRQ# for IPC
	 */
	unsigned int mbx_cp2ap_msg;	/* MBOX# for IPC RX */
	unsigned int irq_cp2ap_msg;	/* IRQ# for IPC RX  */

	unsigned int mbx_ap2cp_msg;	/* MBOX# for IPC TX */
	unsigned int int_ap2cp_msg;	/* INTR# for IPC TX */

	/**
	 * Member variables for TX & RX
	 */
	struct mst_buff_head msb_rxq;
	struct mst_buff_head msb_log;
	struct tasklet_struct rx_tsk;
	struct hrtimer tx_timer;

	/**
	 * Member variables for CP booting and crash dump
	 */
	struct delayed_work udl_rx_dwork;
	struct std_dload_info img_info;	/* Information of each binary image */
	spinlock_t pif_init_lock;
	atomic_t cp_boot_done;

	/**
	 * Member variables for handling forced CP crash
	 */
	struct wake_lock dump_wlock;
	bool forced_cp_crash;
	struct timer_list crash_ack_timer;

#ifdef DEBUG_MODEM_IF
	/* for logging MEMORY dump */
	struct work_struct dump_work;
	char dump_path[MIF_MAX_PATH_LEN];
#endif

#ifdef CONFIG_LINK_POWER_MANAGEMENT
	unsigned int gpio_ap_wakeup;		/* CP-to-AP wakeup GPIO */
	unsigned int irq_ap_wakeup;		/* CP-to-AP wakeup IRQ# */

	unsigned int gpio_cp_wakeup;		/* AP-to-CP wakeup GPIO */

	unsigned int gpio_cp_status;		/* CP-to-AP status GPIO */
	unsigned int irq_cp_status;		/* CP-to-AP status IRQ# */

	unsigned int gpio_ap_status;		/* AP-to-CP status GPIO */

	struct wake_lock ap_wlock;		/* locked by ap_wakeup */
	struct wake_lock cp_wlock;		/* locked by cp_status */

	struct delayed_work cp_sleep_dwork;	/* to hold ap2cp_wakeup */

	spinlock_t pm_lock;
	atomic_t ref_cnt;
#endif

	/**
	 * Mandatory methods for the common memory-type interface framework
	 */
	void *(*remap_region)(unsigned long phys_addr, unsigned long size);
	u16 (*recv_cp2ap_irq)(struct mem_link_device *mld);
	void (*send_ap2cp_irq)(struct mem_link_device *mld, u16 mask);

	/**
	 * Optional methods for some kind of memory-type interface media
	 */
	void (*unmap_region)(void *rgn);
	u16 (*read_ap2cp_irq)(struct mem_link_device *mld);
	void (*finalize_cp_start)(struct mem_link_device *mld);

#ifdef CONFIG_LINK_POWER_MANAGEMENT
	void (*forbid_cp_sleep)(struct mem_link_device *mld);
	void (*permit_cp_sleep)(struct mem_link_device *mld);
	bool (*link_active)(struct mem_link_device *mld);
#endif
};

#define to_mem_link_device(ld) \
		container_of(ld, struct mem_link_device, link_dev)

#define MEM_IPC_MAGIC		0xAA
#define MEM_CRASH_MAGIC		0xDEADDEAD
#define SHM_BOOT_MAGIC		0x424F4F54
#define SHM_DUMP_MAGIC		0x44554D50
#define SHM_PM_MAGIC		0x5F

#define CP_WAKEUP_HOLD_TIME	500	/* 500 ms */

/**
@}
*/
#endif

#endif
