/*
 * Copyright (C) 2019 Samsung Electronics.
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

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>

#define DEBUG_MODEM_IF
#ifdef DEBUG_MODEM_IF
#if 1
#define DEBUG_MODEM_IF_LINK_TX
#endif
#if 1
#define DEBUG_MODEM_IF_LINK_RX
#endif
#if defined(DEBUG_MODEM_IF_LINK_TX) && defined(DEBUG_MODEM_IF_LINK_RX)
#define DEBUG_MODEM_IF_LINK_HEADER
#endif

#if 0
#define DEBUG_MODEM_IF_IODEV_TX
#endif
#if 0
#define DEBUG_MODEM_IF_IODEV_RX
#endif

#if 0
#define DEBUG_MODEM_IF_FLOW_CTRL
#endif

#if 0
#define DEBUG_MODEM_IF_PS_DATA
#endif
#if 0
#define DEBUG_MODEM_IF_IP_DATA
#endif
#endif

/*
IOCTL commands
*/
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

#define IOCTL_LINK_CONNECTED		_IO('o', 0x33)
#define IOCTL_MODEM_SET_TX_LINK		_IO('o', 0x37)

#define IOCTL_MODEM_RAMDUMP_START	_IO('o', 0xCE)
#define IOCTL_MODEM_RAMDUMP_STOP	_IO('o', 0xCF)

#define IOCTL_MODEM_XMIT_BOOT		_IO('o', 0x40)
#ifdef CONFIG_LINK_DEVICE_SHMEM
#define IOCTL_MODEM_GET_SHMEM_INFO	_IO('o', 0x41)
#endif
#define IOCTL_DPRAM_INIT_STATUS		_IO('o', 0x43)

#define IOCTL_LINK_DEVICE_RESET		_IO('o', 0x44)

#ifdef CONFIG_LINK_DEVICE_SHMEM
#define IOCTL_MODEM_GET_SHMEM_SRINFO	_IO('o', 0x45)
#define IOCTL_MODEM_SET_SHMEM_SRINFO	_IO('o', 0x46)
#endif
/* ioctl command for IPC Logger */
#define IOCTL_MIF_LOG_DUMP		_IO('o', 0x51)
#define IOCTL_MIF_DPRAM_DUMP		_IO('o', 0x52)

#define IOCTL_SECURITY_REQ		_IO('o', 0x53)	/* Request smc_call */
#define IOCTL_SHMEM_FULL_DUMP		_IO('o', 0x54)	/* For shmem dump */
#define IOCTL_MODEM_CRASH_REASON	_IO('o', 0x55)	/* Get Crash Reason */

/*
Definitions for IO devices
*/
#define MAX_IOD_RXQ_LEN		2048

#define CP_CRASH_INFO_SIZE	512
#define CP_CRASH_TAG		"CP Crash "

#define IPv6			6
#define SOURCE_MAC_ADDR		{0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC}

/* Loopback */
#define CP2AP_LOOPBACK_CHANNEL	30
#define DATA_LOOPBACK_CHANNEL	31

#define DATA_DRAIN_CHANNEL	30	/* Drain channel to drop RX packets */

/* Debugging features */
#define MIF_LOG_DIR		"/sdcard/log"
#define MIF_MAX_PATH_LEN	256

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
	STATE_CRASH_WATCHDOG,	/* cp watchdog crash */
};

enum link_state {
	LINK_STATE_OFFLINE = 0,
	LINK_STATE_IPC,
	LINK_STATE_CP_CRASH
};

struct sim_state {
	bool online;	/* SIM is online? */
	bool changed;	/* online is changed? */
};

enum cp_boot_mode {
	CP_BOOT_MODE_NORMAL,
	CP_BOOT_MODE_DUMP,
	CP_BOOT_RE_INIT,
	MAX_CP_BOOT_MODE
};

struct sec_info {
	enum cp_boot_mode mode;
	u32 size;
};

enum mem_info {
	MAGIC_CODE = 16,
	CP_MEM,
	SHM_MEM
};
#define SIPC_MULTI_FRAME_MORE_BIT	(0b10000000)	/* 0x80 */
#define SIPC_MULTI_FRAME_ID_MASK	(0b01111111)	/* 0x7F */
#define SIPC_MULTI_FRAME_ID_BITS	7
#define NUM_SIPC_MULTI_FRAME_IDS	(2 ^ SIPC_MULTI_FRAME_ID_BITS)
#define MAX_SIPC_MULTI_FRAME_ID		(NUM_SIPC_MULTI_FRAME_IDS - 1)

struct __packed sipc_fmt_hdr {
	u16 len;
	u8  msg_seq;
	u8  ack_seq;
	u8  main_cmd;
	u8  sub_cmd;
	u8  cmd_type;
};

enum modem_io {
	IODEV_MISC,
	IODEV_NET,
	IODEV_DUMMY,
};

enum dev_format {
	IPC_FMT,
	IPC_RAW,
	IPC_RFS,
	IPC_MULTI_RAW,
	IPC_BOOT,
	IPC_DUMP,
	IPC_CMD,
	IPC_DEBUG,
	MAX_DEV_FORMAT,
};

struct io_device {
	struct list_head list;

	/* Name of the IO device */
	char *name;

	/* Reference count */
	atomic_t opened;

	/* Misc and net device structures for the IO device */
	struct miscdevice  miscdev;
	struct net_device *ndev;
	struct list_head node_ndev;
	struct napi_struct napi;

	/* ID and Format for channel on the link */
	unsigned int id;
	unsigned int minor;
	enum dev_format format;
	enum modem_io io_typ;

	/* Attributes of an IO device */
	u32 attrs;

	/* The name of the application that will use this IO device */
	char *app;

};
#define to_io_device(misc) container_of(misc, struct io_device, miscdev)

/* platform data */
struct modem_data {
	char *name;
	unsigned num_iodevs;
	struct io_device *iodevs;
};

#define LOG_TAG	"mif: "
#define FUNC	(__func__)
#define CALLER	(__builtin_return_address(0))

#define mif_err_limited(fmt, ...) \
	printk_ratelimited(KERN_ERR LOG_TAG "%s: " pr_fmt(fmt), __func__, ##__VA_ARGS__)
#define mif_err(fmt, ...) \
	pr_err(LOG_TAG "%s: " pr_fmt(fmt), __func__, ##__VA_ARGS__)
#define mif_debug(fmt, ...) \
	pr_debug(LOG_TAG "%s: " pr_fmt(fmt), __func__, ##__VA_ARGS__)
#define mif_info(fmt, ...) \
	pr_info(LOG_TAG "%s: " pr_fmt(fmt), __func__, ##__VA_ARGS__)
#define mif_trace(fmt, ...) \
	printk(KERN_DEBUG "mif: %s: %d: called(%pF): " fmt, \
			__func__, __LINE__, __builtin_return_address(0), ##__VA_ARGS__)

#ifdef CONFIG_OF
#define mif_dt_read_enum(np, prop, dest) \
	do { \
		u32 val; \
		if (of_property_read_u32(np, prop, &val)) \
		return -EINVAL; \
		dest = (__typeof__(dest))(val); \
	} while (0)

#define mif_dt_read_bool(np, prop, dest) \
	do { \
		u32 val; \
		if (of_property_read_u32(np, prop, &val)) \
		return -EINVAL; \
		dest = val ? true : false; \
	} while (0)

#define mif_dt_read_string(np, prop, dest) \
	do { \
		if (of_property_read_string(np, prop, \
					(const char **)&dest)) \
		return -EINVAL; \
	} while (0)

#define mif_dt_read_u32(np, prop, dest) \
	do { \
		u32 val; \
		if (of_property_read_u32(np, prop, &val)) \
		return -EINVAL; \
		dest = val; \
	} while (0)
#endif



extern void register_port_ops(const struct file_operations *ops);
extern void print_ipc_pkt(u16 ch, struct sk_buff *skb);

#endif

