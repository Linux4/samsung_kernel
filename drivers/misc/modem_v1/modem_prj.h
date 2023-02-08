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

#ifndef __MODEM_PRJ_H__
#define __MODEM_PRJ_H__

#include <linux/wait.h>
#include <linux/miscdevice.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/wakelock.h>
#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/platform_data/modem_v1.h>

#include "include/sipc5.h"

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
#define DEBUG_MODEM_IF
#endif

#ifdef DEBUG_MODEM_IF
#define DEBUG_MODEM_IF_LINK_TX
#define DEBUG_MODEM_IF_LINK_RX
#if 0
#define DEBUG_MODEM_IF_IODEV_TX
#define DEBUG_MODEM_IF_IODEV_RX
#endif
#if 1
#define DEBUG_MODEM_IF_FLOW_CTRL
#endif

#if 1
#define DEBUG_MODEM_IF_BOOT
#endif
#if 1
#define DEBUG_MODEM_IF_DUMP
#endif
#if 0
#define DEBUG_MODEM_IF_RFS
#endif
#if 0
#define DEBUG_MODEM_IF_CSVT
#endif
#if 0
#define DEBUG_MODEM_IF_LOG
#endif
#if 0
#define DEBUG_MODEM_IF_PS
#endif
#endif

#define MAX_CPINFO_SIZE		512

#define MAX_LINK_DEVTYPE	3

#define MAX_FMT_DEVS	10
#define MAX_RAW_DEVS	32
#define MAX_RFS_DEVS	10
#define MAX_BOOT_DEVS	10
#define MAX_DUMP_DEVS	10

#define MAX_IOD_RXQ_LEN	2048

#define IOCTL_MODEM_ON			_IO('o', 0x19)
#define IOCTL_MODEM_OFF			_IO('o', 0x20)
#define IOCTL_MODEM_RESET		_IO('o', 0x21)
#define IOCTL_MODEM_BOOT_ON		_IO('o', 0x22)
#define IOCTL_MODEM_BOOT_OFF		_IO('o', 0x23)
#define IOCTL_MODEM_BOOT_DONE		_IO('o', 0x24)

#define IOCTL_MODEM_PROTOCOL_SUSPEND	_IO('o', 0x25)
#define IOCTL_MODEM_PROTOCOL_RESUME	_IO('o', 0x26)

#define IOCTL_MODEM_STATUS		_IO('o', 0x27)
#define IOCTL_MODEM_DL_START		_IO('o', 0x28)
#define IOCTL_MODEM_FW_UPDATE		_IO('o', 0x29)

#define IOCTL_MODEM_NET_SUSPEND		_IO('o', 0x30)
#define IOCTL_MODEM_NET_RESUME		_IO('o', 0x31)

#define IOCTL_MODEM_DUMP_START		_IO('o', 0x32)
#define IOCTL_MODEM_DUMP_UPDATE		_IO('o', 0x33)
#define IOCTL_MODEM_FORCE_CRASH_EXIT	_IO('o', 0x34)
#define IOCTL_MODEM_CP_UPLOAD		_IO('o', 0x35)
#define IOCTL_MODEM_DUMP_RESET		_IO('o', 0x36)

#define IOCTL_MODEM_RAMDUMP_START	_IO('o', 0xCE)
#define IOCTL_MODEM_RAMDUMP_STOP	_IO('o', 0xCF)

#define IOCTL_MODEM_XMIT_BOOT		_IO('o', 0x40)
#ifdef CONFIG_LINK_DEVICE_SHMEM
#define IOCTL_MODEM_GET_SHMEM_INFO	_IO('o', 0x41)
#endif
#define IOCTL_DPRAM_INIT_STATUS		_IO('o', 0x43)

/* ioctl command for IPC Logger */
#define IOCTL_MIF_LOG_DUMP		_IO('o', 0x51)
#define IOCTL_MIF_DPRAM_DUMP		_IO('o', 0x52)

/* ioctl command definitions. */
#define IOCTL_DPRAM_PHONE_POWON		_IO('o', 0xD0)
#define IOCTL_DPRAM_PHONEIMG_LOAD	_IO('o', 0xD1)
#define IOCTL_DPRAM_NVDATA_LOAD		_IO('o', 0xD2)
#define IOCTL_DPRAM_PHONE_BOOTSTART	_IO('o', 0xD3)

#define IOCTL_DPRAM_PHONE_UPLOAD_STEP1	_IO('o', 0xDE)
#define IOCTL_DPRAM_PHONE_UPLOAD_STEP2	_IO('o', 0xDF)

#define CPBOOT_DIR_MASK		0xF000
#define CPBOOT_STAGE_MASK	0x0F00
#define CPBOOT_CMD_MASK		0x000F
#define CPBOOT_REQ_RESP_MASK	0x0FFF

#define CPBOOT_DIR_AP2CP	0x9000
#define CPBOOT_DIR_CP2AP	0xA000

#define CPBOOT_STAGE_SHIFT	8

#define CPBOOT_STAGE_START	0x0000
#define CPBOOT_CRC_SEND		0x000C
#define CPBOOT_STAGE_DONE	0x000D
#define CPBOOT_STAGE_FAIL	0x000F

/* modem status */
#define MODEM_OFF		0
#define MODEM_CRASHED		1
#define MODEM_RAMDUMP		2
#define MODEM_POWER_ON		3
#define MODEM_BOOTING_NORMAL	4
#define MODEM_BOOTING_RAMDUMP	5
#define MODEM_DUMPING		6
#define MODEM_RUNNING		7

#define HDLC_HEADER_MAX_SIZE	6 /* fmt 3, raw 6, rfs 6 */

#define PSD_DATA_CHID_BEGIN	0x2A
#define PSD_DATA_CHID_END	0x38

#define PS_DATA_CH_0		10
#define PS_DATA_CH_LAST		24
#define RMNET0_CH_ID		PS_DATA_CH_0

#define IP6VERSION		6

#define SOURCE_MAC_ADDR		{0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC}

/* IP loopback */
#define DATA_DRAIN_CHANNEL	30	/* Drain channel to drop RX packets */
#define DATA_LOOPBACK_CHANNEL	31

/* Debugging features */
#define MIF_LOG_DIR		"/sdcard/log"
#define MIF_MAX_PATH_LEN	256

#define MAX_HEX_LEN		32
#define MAX_TAG_LEN		32
#define MAX_PREFIX_LEN		64
#define MAX_NAME_LEN		64
#define MAX_STR_LEN		256

#define CP_CRASH_TAG		"CP Crash "

enum ipc_layer {
	APP = 0,
	IODEV,
	LINK,
	MAX_SIPC_LAYER
};

static const char const *sipc_layer_string[] = {
	[APP] = "APP",
	[IODEV] = "IOD",
	[LINK] = "LNK"
};

static const inline char *layer_str(enum ipc_layer layer)
{
	if (unlikely(layer >= MAX_SIPC_LAYER))
		return "INVALID";
	else
		return sipc_layer_string[layer];
}

static const char const *dev_format_string[] = {
	[IPC_FMT]	= "FMT",
	[IPC_RAW]	= "RAW",
	[IPC_RFS]	= "RFS",
	[IPC_MULTI_RAW]	= "MULTI_RAW",
	[IPC_CMD]	= "CMD",
	[IPC_BOOT]	= "BOOT",
	[IPC_RAMDUMP]	= "RAMDUMP",
	[IPC_DEBUG]	= "DEBUG",
};

static const inline char *dev_str(enum dev_format dev)
{
	if (unlikely(dev >= MAX_DEV_FORMAT))
		return "INVALID";
	else
		return dev_format_string[dev];
}

enum direction {
	TX,
	RX,
	MAX_DIR
};

static inline enum direction opposite(enum direction dir)
{
	return (dir == TX) ? RX : TX;
}

static const char const *direction_string[] = {
	[TX] = "TX",
	[RX] = "RX"
};

static const inline char *dir_str(enum direction dir)
{
	if (unlikely(dir >= MAX_DIR))
		return "INVALID";
	else
		return direction_string[dir];
}

static const char const *q_direction_string[] = {
	[TX] = "TXQ",
	[RX] = "RXQ"
};

static const inline char *q_dir(enum direction dir)
{
	if (unlikely(dir >= MAX_DIR))
		return "INVALID";
	else
		return q_direction_string[dir];
}

static const char const *ipc_direction_string[] = {
	[TX] = "AP->CP",
	[RX] = "CP->AP"
};

static const inline char *ipc_dir(enum direction dir)
{
	if (unlikely(dir >= MAX_DIR))
		return "INVALID";
	else
		return ipc_direction_string[dir];
}

static const char const *arrow_direction[] = {
	[TX] = "->",
	[RX] = "<-"
};

static const inline char *arrow(enum direction dir)
{
	if (unlikely(dir >= MAX_DIR))
		return "><";
	else
		return arrow_direction[dir];
}

/* Does modem ctl structure will use state ? or status defined below ?*/
enum modem_state {
	STATE_OFFLINE,
	STATE_CRASH_RESET,	/* silent reset */
	STATE_CRASH_EXIT,	/* cp ramdump */
	STATE_BOOTING,
	STATE_ONLINE,
	STATE_NV_REBUILDING,	/* <= rebuilding start */
	STATE_LOADER_DONE,
	STATE_SIM_ATTACH,
	STATE_SIM_DETACH,
};

static const char const *modem_state_string[] = {
	[STATE_OFFLINE]		= "OFFLINE",
	[STATE_CRASH_RESET]	= "CRASH_RESET",
	[STATE_CRASH_EXIT]	= "CRASH_EXIT",
	[STATE_BOOTING]		= "BOOTING",
	[STATE_ONLINE]		= "ONLINE",
	[STATE_NV_REBUILDING]	= "NV_REBUILDING",
	[STATE_LOADER_DONE]	= "LOADER_DONE",
	[STATE_SIM_ATTACH]	= "SIM_ATTACH",
	[STATE_SIM_DETACH]	= "SIM_DETACH",
};

/**
@brief		return the phone_state string
@param state	the state of a phone (CP)
*/
static const inline char *cp_state_str(enum modem_state state)
{
	return modem_state_string[state];
}

struct sim_state {
	bool online;	/* SIM is online? */
	bool changed;	/* online is changed? */
};

struct modem_firmware {
	char *binary;
	u32 size;
};

#define HDLC_START		0x7F
#define HDLC_END		0x7E
#define SIZE_OF_HDLC_START	1
#define SIZE_OF_HDLC_END	1
#define MAX_LINK_PADDING_SIZE	3

struct header_data {
	char hdr[HDLC_HEADER_MAX_SIZE];
	u32 len;
	u32 frag_len;
	char start; /*hdlc start header 0x7F*/
};

struct  __packed fmt_hdr {
	u16 len;
	u8 control;
};

struct  __packed raw_hdr {
	u32 len;
	u8 channel;
	u8 control;
};

struct  __packed rfs_hdr {
	u32 len;
	u8 cmd;
	u8 id;
};

struct __packed sipc_fmt_hdr {
	u16 len;
	u8  msg_seq;
	u8  ack_seq;
	u8  main_cmd;
	u8  sub_cmd;
	u8  cmd_type;
};

/**
@brief		return true if the channel ID is for a CSD
@param ch	channel ID
*/
static inline bool sipc_csd_ch(u8 ch)
{
	return (ch >= SIPC_CH_ID_CS_VT_DATA && ch <= SIPC_CH_ID_CS_VT_VIDEO) ?
		true : false;
}

/**
@brief		return true if the channel ID is for a PS network
@param ch	channel ID
*/
static inline bool sipc_ps_ch(u8 ch)
{
	return (ch >= SIPC_CH_ID_PDP_0 && ch <= SIPC_CH_ID_PDP_14) ?
		true : false;
}

/**
@brief		return true if the channel ID is for CP LOG channel
@param ch	channel ID
*/
static inline bool sipc_log_ch(u8 ch)
{
	return (ch >= SIPC_CH_ID_CPLOG1 && ch <= SIPC_CH_ID_CPLOG2) ?
		true : false;
}

#define FLOW_CTRL_SUSPEND	((u8)(0xCA))
#define FLOW_CTRL_RESUME	((u8)(0xCB))

/* If iod->id is 0, do not need to store to `iodevs_tree_fmt' in SIPC4 */
#define sipc4_is_not_reserved_channel(ch) ((ch) != 0)

/* Channel 0, 5, 6, 27, 255 are reserved in SIPC5.
 * see SIPC5 spec: 2.2.2 Channel Identification (Ch ID) Field.
 * They do not need to store in `iodevs_tree_fmt'
 */
#define sipc5_is_not_reserved_channel(ch) \
	((ch) != 0 && (ch) != 5 && (ch) != 6 && (ch) != 27 && (ch) != 255)

struct vnet {
	struct io_device *iod;
};

/* for fragmented data from link devices */
struct fragmented_data {
	struct sk_buff *skb_recv;
	struct header_data h_data;
	struct sipc5_frame_data f_data;
	/* page alloc fail retry*/
	unsigned int realloc_offset;
};
#define fragdata(iod, ld) (&(iod)->fragments[(ld)->link_type])

/** struct skbuff_priv - private data of struct sk_buff
 * this is matched to char cb[48] of struct sk_buff
 */
struct skbuff_private {
	struct io_device *iod;
	struct link_device *ld;
	struct io_device *real_iod; /* for rx multipdp */

	/* for time-stamping */
	struct timespec ts;

	/* for indicating that thers is only one IPC frame in an skb */
	bool single_frame;
} __packed;

static inline struct skbuff_private *skbpriv(struct sk_buff *skb)
{
	BUILD_BUG_ON(sizeof(struct skbuff_private) > sizeof(skb->cb));
	return (struct skbuff_private *)&skb->cb;
}

enum iod_rx_state {
	IOD_RX_ON_STANDBY = 0,
	IOD_RX_HEADER,
	IOD_RX_PAYLOAD,
	IOD_RX_PADDING,
	MAX_IOD_RX_STATE
};

static const char const *rx_state_string[] = {
	[IOD_RX_ON_STANDBY]	= "RX_ON_STANDBY",
	[IOD_RX_HEADER]		= "RX_HEADER",
	[IOD_RX_PAYLOAD]	= "RX_PAYLOAD",
	[IOD_RX_PADDING]	= "RX_PADDING",
};

/**
@brief		return RX FSM state as a string.
@param state	the RX FSM state of an I/O device
*/
static const inline char *rx_state(enum iod_rx_state state)
{
	if (unlikely(state >= MAX_IOD_RX_STATE))
		return "INVALID_STATE";
	else
		return rx_state_string[state];
}

struct io_device {
	/* rb_tree node for an io device */
	struct rb_node node_chan;
	struct rb_node node_fmt;

	/* Name of the IO device */
	char *name;

	/* Reference count */
	atomic_t opened;

	/* Wait queue for the IO device */
	wait_queue_head_t wq;

	/* Misc and net device structures for the IO device */
	struct miscdevice  miscdev;
	struct net_device *ndev;

	/* ID and Format for channel on the link */
	unsigned int id;
	enum modem_link link_types;
	enum dev_format format;
	enum modem_io io_typ;
	enum modem_network net_typ;

	/* The name of the application that will use this IO device */
	char *app;

	/* Whether or not handover among 2+ link devices */
	bool use_handover;

	/* SIPC version */
	enum sipc_ver ipc_version;

	/* Rx queue of sk_buff */
	struct sk_buff_head sk_rx_q;

	/* RX state used in RX FSM */
	enum iod_rx_state curr_rx_state;
	enum iod_rx_state next_rx_state;

	/*
	** work for each io device, when delayed work needed
	** use this for private io device rx action
	*/
	struct delayed_work rx_work;

	struct fragmented_data fragments[LINKDEV_MAX];

	/* for multi-frame */
	struct sk_buff *skb[128];

	/* called from linkdevice when a packet arrives for this iodevice */
	int (*recv)(struct io_device *iod, struct link_device *ld,
					const char *data, unsigned int len);

	/**
	 * If a link device can pass multiple IPC frames per each skb, this
	 * method must be used. (Each IPC frame can be processed with skb_clone
	 * function.)
	 */
	int (*recv_skb)(struct io_device *iod, struct link_device *ld,
					struct sk_buff *skb);

	/**
	 * If a link device passes only one IPC frame per each skb, this method
	 * should be used.
	 */
	int (*recv_skb_single)(struct io_device *iod, struct link_device *ld,
			       struct sk_buff *skb);

	/* inform the IO device that the modem is now online or offline or
	 * crashing or whatever...
	 */
	void (*modem_state_changed)(struct io_device *iod, enum modem_state);

	/* inform the IO device that the SIM is not inserting or removing */
	void (*sim_state_changed)(struct io_device *iod, bool sim_online);

	struct modem_ctl *mc;
	struct modem_shared *msd;

	struct wake_lock wakelock;
	long waketime;

	/* DO NOT use __current_link directly
	 * you MUST use skbpriv(skb)->ld in mc, link, etc..
	 */
	struct link_device *__current_link;
};
#define to_io_device(misc) container_of(misc, struct io_device, miscdev)

/* get_current_link, set_current_link don't need to use locks.
 * In ARM, set_current_link and get_current_link are compiled to
 * each one instruction (str, ldr) as atomic_set, atomic_read.
 * And, the order of set_current_link and get_current_link is not important.
 */
#define get_current_link(iod) ((iod)->__current_link)
#define set_current_link(iod, ld) ((iod)->__current_link = (ld))

struct link_device {
	struct list_head  list;
	char *name;

	enum modem_link link_type;
	unsigned int aligned;

	/* SIPC version */
	enum sipc_ver ipc_version;

	/* Maximum IPC device = the last IPC device (e.g. IPC_RFS) + 1 */
	unsigned int max_ipc_dev;

	/* Modem data */
	struct modem_data *mdm_data;

	/* Modem control */
	struct modem_ctl *mc;

	/* Modem shared data */
	struct modem_shared *msd;

	/* completion for waiting for link initialization */
	struct completion init_cmpl;

	/* completion for waiting for PIF initialization in a CP */
	struct completion pif_cmpl;

	struct io_device *fmt_iods[4];

	/* TX queue of socket buffers */
	struct sk_buff_head sk_fmt_tx_q;
	struct sk_buff_head sk_raw_tx_q;
	struct sk_buff_head sk_rfs_tx_q;

	struct sk_buff_head *skb_txq[MAX_IPC_DEV];

	/* RX queue of socket buffers */
	struct sk_buff_head sk_fmt_rx_q;
	struct sk_buff_head sk_raw_rx_q;
	struct sk_buff_head sk_rfs_rx_q;

	struct sk_buff_head *skb_rxq[MAX_IPC_DEV];

	/* Spinlocks for TX & RX */
	spinlock_t tx_lock[MAX_IPC_DEV];
	spinlock_t rx_lock[MAX_IPC_DEV];

	bool raw_tx_suspended; /* for misc dev */
	struct completion raw_tx_resumed_by_cp;

	/**
	 * This flag is for TX flow control on network interface.
	 * This must be set and clear only by a flow control command from CP.
	 */
	bool suspend_netif_tx;

	/* Stop/resume control for network ifaces */
	spinlock_t netif_lock;
	atomic_t netif_stopped;

	struct workqueue_struct *tx_wq;
	struct work_struct tx_work;
	struct delayed_work tx_delayed_work;

	struct delayed_work *tx_dwork[MAX_IPC_DEV];
	struct delayed_work fmt_tx_dwork;
	struct delayed_work raw_tx_dwork;
	struct delayed_work rfs_tx_dwork;

	struct workqueue_struct *rx_wq;
	struct work_struct rx_work;
	struct delayed_work rx_delayed_work;

	/* init communication - setting link driver */
	int (*init_comm)(struct link_device *ld, struct io_device *iod);

	/* terminate communication */
	void (*terminate_comm)(struct link_device *ld, struct io_device *iod);

	/* called by an io_device when it has a packet to send over link
	 * - the io device is passed so the link device can look at id and
	 *   format fields to determine how to route/format the packet
	 */
	int (*send)(struct link_device *ld, struct io_device *iod,
			struct sk_buff *skb);

	/* method for CP booting */
	int (*xmit_boot)(struct link_device *ld, struct io_device *iod,
			unsigned long arg);

	/* methods for CP firmware upgrade */
	int (*dload_start)(struct link_device *ld, struct io_device *iod);
	int (*firm_update)(struct link_device *ld, struct io_device *iod,
			unsigned long arg);

	/* methods for CP crash dump */
	int (*force_dump)(struct link_device *ld, struct io_device *iod);
	int (*dump_start)(struct link_device *ld, struct io_device *iod);
	int (*dump_update)(struct link_device *ld, struct io_device *iod,
			unsigned long arg);
	int (*dump_finish)(struct link_device *ld, struct io_device *iod,
			unsigned long arg);

	/* IOCTL extension */
	int (*ioctl)(struct link_device *ld, struct io_device *iod,
			unsigned int cmd, unsigned long arg);
};

/**
@brief		allocate an skbuff and set skb's iod, ld

@param length	the length to allocate
@param iod	struct io_device *
@param ld	struct link_device *

@retval "NULL"	if there is no free memory.
*/
static inline struct sk_buff *rx_alloc_skb(unsigned int length,
		struct io_device *iod, struct link_device *ld)
{
	struct sk_buff *skb;

	skb = dev_alloc_skb(length);
	if (likely(skb)) {
		skbpriv(skb)->iod = iod;
		skbpriv(skb)->ld = ld;
	}

	return skb;
}

struct modemctl_ops {
	int (*modem_on)(struct modem_ctl *);
	int (*modem_off)(struct modem_ctl *);
	int (*modem_reset)(struct modem_ctl *);
	int (*modem_boot_on)(struct modem_ctl *);
	int (*modem_boot_off)(struct modem_ctl *);
	int (*modem_boot_done)(struct modem_ctl *);
	int (*modem_force_crash_exit)(struct modem_ctl *);
	int (*modem_dump_reset)(struct modem_ctl *);
	int (*modem_dump_start)(struct modem_ctl *);
};

/* for IPC Logger */
struct mif_storage {
	char *addr;
	unsigned int cnt;
};

/* modem_shared - shared data for all io/link devices and a modem ctl
 * msd : mc : iod : ld = 1 : 1 : M : N
 */
struct modem_shared {
	/* list of link devices */
	struct list_head link_dev_list;

	/* rb_tree root of io devices. */
	struct rb_root iodevs_tree_chan; /* group by channel */
	struct rb_root iodevs_tree_fmt; /* group by dev_format */

	/* for IPC Logger */
	struct mif_storage storage;
	spinlock_t lock;

	/* CP crash information */
	char cp_crash_info[530];

	/* loopbacked IP address
	 * default is 0.0.0.0 (disabled)
	 * after you setted this, you can use IP packet loopback using this IP.
	 * exam: echo 1.2.3.4 > /sys/devices/virtual/misc/umts_multipdp/loopback
	 */
	__be32 loopback_ipaddr;
};

struct modem_irq {
	spinlock_t lock;
	unsigned int num;
	char name[MAX_NAME_LEN];
	unsigned long flags;
	bool active;
};

struct modem_ctl {
	struct device *dev;
	char *name;
	struct modem_data *mdm_data;

	struct modem_shared *msd;

	enum modem_state phone_state;
	struct sim_state sim_state;

	unsigned int gpio_cp_on;
	unsigned int gpio_cp_off;
	unsigned int gpio_reset_req_n;
	unsigned int gpio_cp_reset;

	/* for broadcasting AP's PM state (active or sleep) */
	unsigned int gpio_pda_active;
	unsigned int int_pda_active;

	/* for checking aliveness of CP */
	unsigned int gpio_phone_active;
	unsigned int irq_phone_active;

	/* for AP-CP power management (PM) handshaking */
	unsigned int gpio_ap_wakeup;
	unsigned int irq_ap_wakeup;

	unsigned int gpio_ap_status;
	unsigned int int_ap_status;

	unsigned int gpio_cp_wakeup;
	unsigned int int_cp_wakeup;

	unsigned int gpio_cp_status;
	unsigned int irq_cp_status;

	/* for performance tuning */
	unsigned int gpio_perf_req;
	unsigned int irq_perf_req;

	/* for USB/HSIC PM */
	unsigned int gpio_host_wakeup;
	unsigned int irq_host_wakeup;
	unsigned int gpio_host_active;
	unsigned int gpio_slave_wakeup;

	unsigned int gpio_cp_dump_int;
	unsigned int gpio_ap_dump_int;
	unsigned int gpio_flm_uart_sel;
	unsigned int gpio_cp_warm_reset;

	unsigned int gpio_sim_detect;
	unsigned int irq_sim_detect;

#ifdef CONFIG_LINK_DEVICE_SHMEM
	unsigned int mbx_pda_active;
	unsigned int mbx_phone_active;
	unsigned int mbx_ap_wakeup;
	unsigned int mbx_ap_status;
	unsigned int mbx_cp_wakeup;
	unsigned int mbx_cp_status;
	unsigned int mbx_perf_req;

	/* for system revision information */
	unsigned int sys_rev;
	unsigned int pmic_rev;
	unsigned int pkg_id;
	unsigned int mbx_sys_rev;
	unsigned int mbx_pmic_rev;
	unsigned int mbx_pkg_id;

	struct modem_pmu *pmu;

	/* for checking aliveness of CP */
	struct modem_irq irq_cp_wdt;		/* watchdog timer */
	struct modem_irq irq_cp_fail;
#endif

	struct work_struct pm_qos_work;

	/* completion for waiting for CP power-off */
	struct completion off_cmpl;

	/* Switch with 2 links in a modem */
	unsigned int gpio_link_switch;

	const struct attribute_group *group;

	struct delayed_work dwork;
	struct work_struct work;

	struct modemctl_ops ops;
	struct io_device *iod;
	struct io_device *bootd;

	/* Wakelock for modem_ctl */
	struct wake_lock mc_wake_lock;

	void (*gpio_revers_bias_clear)(void);
	void (*gpio_revers_bias_restore)(void);

	bool need_switch_to_usb;
	bool sim_polarity;

	bool sim_shutdown_req;
	void (*modem_complete)(struct modem_ctl *mc);
};

/**
@brief		return the phone_state string
*/
static const inline char *mc_state(struct modem_ctl *mc)
{
	return cp_state_str(mc->phone_state);
}

static inline bool cp_online(struct modem_ctl *mc)
{
	return (mc->phone_state == STATE_ONLINE);
}

static inline bool cp_booting(struct modem_ctl *mc)
{
	return (mc->phone_state == STATE_BOOTING);
}

static inline bool cp_crashed(struct modem_ctl *mc)
{
	return (mc->phone_state == STATE_CRASH_EXIT);
}

static inline bool rx_possible(struct modem_ctl *mc)
{
	if (likely(cp_online(mc)))
		return true;

	if (cp_booting(mc) || cp_crashed(mc))
		return true;

	return false;
}

int sipc5_init_io_device(struct io_device *iod);

#endif
