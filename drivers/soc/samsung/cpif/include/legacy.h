#ifndef __BOOT_H__
#define __BOOT_H__

#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/kfifo.h>
#include <linux/netdevice.h>

#include "circ_queue.h"
#include "link_device_memory_config.h"
#include "sipc5.h"

#ifdef GROUP_MEM_TYPE_SHMEM

#ifdef CONFIG_LINK_DEVICE_PCIE
#define DOORBELL_INT_ADD		0x10000
#define MODEM_INT_NUM			16
#endif /* end of CONFIG_LINK_DEVICE_PCIE */

#if defined(CONFIG_SEC_MODEM_S5000AP) && defined(CONFIG_SEC_MODEM_S5100)
#define RMNET_COUNT 8
#define SHM_2CP_UL_PATH_CTL_MAGIC	0x4B5B

struct __packed shmem_ulpath_ctl {
	u32 status;
	u32 path;
};

struct __packed shmem_ulpath_table {
	u32 magic;
	struct shmem_ulpath_ctl ctl[RMNET_COUNT];
};
#endif

#endif /* end of GROUP_MEM_TYPE_SHMEM */

enum legacy_ipc_map {
	IPC_MAP_FMT = 0,
#ifdef CONFIG_MODEM_IF_LEGACY_QOS
	IPC_MAP_HPRIO_RAW,
#endif
	IPC_MAP_NORM_RAW,
	MAX_SIPC_MAP,
};


struct legacy_ipc_device {
	enum legacy_ipc_map id;
	char name[16];

	struct circ_queue txq;
	struct circ_queue rxq;

	u16 msg_mask;
	u16 req_ack_mask;
	u16 res_ack_mask;

	struct sk_buff_head *skb_txq;
	struct sk_buff_head *skb_rxq;

	unsigned int req_ack_cnt[MAX_DIR];

	spinlock_t tx_lock;
};

struct legacy_link_device {

	struct link_device *ld;

	atomic_t active;

	u32 __iomem *magic;
	u32 __iomem *mem_access;

	struct legacy_ipc_device *dev[MAX_SIPC_MAP];


};

static inline enum legacy_ipc_map get_mmap_idx(enum sipc_ch_id ch,
		struct sk_buff *skb)
{
	if (sipc5_fmt_ch(ch))
		return IPC_MAP_FMT;
#ifdef CONFIG_MODEM_IF_LEGACY_QOS
	return (skb->queue_mapping == 1) ?
		IPC_MAP_HPRIO_RAW : IPC_MAP_NORM_RAW;
#else
	return IPC_MAP_NORM_RAW;
#endif
}

int create_legacy_link_device(struct mem_link_device *mld);
int init_legacy_link(struct legacy_link_device *bl);
int xmit_to_legacy_link(struct mem_link_device *mld, enum sipc_ch_id ch,
		struct sk_buff *skb, enum legacy_ipc_map legacy_buffer_index);
struct sk_buff *recv_from_legacy_link(struct mem_link_device *mld,
		struct legacy_ipc_device *dev, unsigned int in, int *ret);
bool check_legacy_tx_pending(struct mem_link_device *mld);

#endif /* end of __BOOT_H__ */

