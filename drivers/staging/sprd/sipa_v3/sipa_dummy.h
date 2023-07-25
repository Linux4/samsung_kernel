#ifndef __SIPA_DUMMY_H_
#define __SIPA_DUMMY_H_

/* Device status */
#define SIPA_DUMMY_DEV_ON 1
#define SIPA_DUMMY_DEV_OFF 0

#define SIPA_DUMMY_MAX_CPUS		8
#define SIPA_DUMMY_MAX_PACKET_SIZE	16384
#define SIPA_DUMMY_NAPI_WEIGHT		64

#define SIPA_DUMMY_MTU_DEF 1514U

/*
 * We stipulate the order of all net_devices that may
 * associated with sipa_dummy interface.
 * So that we can fetch them from a global array easily.
 * and effectively.
 */
enum sipa_dummy_ndev_id {
	SIPA_DUMMY_ETH0,
	SIPA_DUMMY_ETH1,
	SIPA_DUMMY_ETH2,
	SIPA_DUMMY_ETH3,
	SIPA_DUMMY_ETH4,
	SIPA_DUMMY_ETH5,
	SIPA_DUMMY_ETH6,
	SIPA_DUMMY_ETH7,
	SIPA_DUMMY_ETH8,
	SIPA_DUMMY_ETH9,
	SIPA_DUMMY_ETH10,
	SIPA_DUMMY_ETH11,
	SIPA_DUMMY_ETH12,
	SIPA_DUMMY_ETH13,
	SIPA_DUMMY_ETH14,
	SIPA_DUMMY_ETH15,
	SIPA_DUMMY_USB0,
	SIPA_DUMMY_WIFI0,
	SIPA_DUMMY_NDEV_MAX
};

enum sipa_dummy_ts_field {
	SIPA_DUMMY_TS_RD_EMPTY,
	SIPA_DUMMY_TS_NAPI_COMPLETE,
	SIPA_DUMMY_TS_NAPI_RESCHEDULE,
	SIPA_DUMMY_TS_IRQ_TRIGGER,
};

struct sipa_dummy_ndev_info {
	u32 src_id;
	int netid;
	int state;
	struct napi_struct *napi;
	struct net_device *ndev;
};

struct sipa_dummy_cfg {
	u32 vecs;
	u32 mtu;
	u32 flow_control;
	u32 num_rss_queues;
	bool is_polling;
};

/*
 * Packets received on each cpu
 */
struct sipa_dummy_self_stats {
	unsigned long rx_packets;
	unsigned long rx_bytes;
};

/*
 * Napi instance for each cpu
 */
struct sipa_dummy_ring {
	struct net_device *ndev;/* Linux net device */
	struct napi_struct napi;/* Napi instance */

	u64 last_read_empty;
	u64 last_napi_complete;
	u64 last_napi_reschedule;
	u64 last_irq_trigger;
};

/* Device instance data. */
struct sipa_dummy {
	struct sipa_dummy_cfg cfg;
	u8 mac_addr[ETH_ALEN];
	struct net_device *ndev;/* Linux net device */
	struct net_device_stats stats;/* Net statistics */
	struct sipa_dummy_ring rings[SIPA_DUMMY_MAX_CPUS];/* Napi instance */
	struct sipa_dummy_self_stats per_stats[SIPA_DUMMY_MAX_CPUS];
};

irqreturn_t sipa_dummy_recv_trigger(unsigned int cpu);

#endif
