/*
 * Copyright (C) 2012 Samsung Electronics.
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

#ifndef __MODEM_V1_H__
#define __MODEM_V1_H__

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/types.h>
#include <linux/shm_ipc.h>

#include "cp_btl.h"

#define MAX_STR_LEN		256
#define MAX_NAME_LEN		64
#define MAX_DUMP_LEN		20

#define SMC_ID		0x82000700
#define SMC_ID_CLK	0x82001011
#define SSS_CLK_ENABLE	0
#define SSS_CLK_DISABLE	1

enum modem_t {
	SEC_S5000AP = 0,
	SEC_S5100 = 1,
	DUMMY,
	MAX_MODEM_TYPE
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

enum modem_io {
	IODEV_BOOTDUMP,
	IODEV_IPC,
	IODEV_NET,
	IODEV_DUMMY,
};

/* Caution!!
 * Please make sure that below sequence is exactly matched with cbd.
 * If you want one more link type, append it to the end of list.
 */
enum modem_link {
	LINKDEV_SHMEM = 0,
	LINKDEV_PCIE = 1,
	LINKDEV_UNDEFINED,
	LINKDEV_MAX
};

enum modem_interrupt {
	INTERRUPT_MAILBOX = 0,
	INTERRUPT_GPIO,
	INTERRUPT_MAX
};


enum modem_network {
	UMTS_NETWORK,
	CDMA_NETWORK,
	TDSCDMA_NETWORK,
	LTE_NETWORK,
	MAX_MODEM_NETWORK
};

enum sipc_ver {
	NO_SIPC_VER = 0,
	SIPC_VER_40 = 40,
	SIPC_VER_41 = 41,
	SIPC_VER_42 = 42,
	SIPC_VER_50 = 50,
	MAX_SIPC_VER
};

enum sipc_ch_id {
	SIPC_CH_ID_RAW_0 = 0,	/*reserved*/
	SIPC_CH_ID_CS_VT_DATA,
	SIPC_CH_ID_CS_VT_CONTROL,
	SIPC_CH_ID_CS_VT_AUDIO,
	SIPC_CH_ID_CS_VT_VIDEO,
	SIPC_CH_ID_RAW_5,	/*reserved*/
	SIPC_CH_ID_RAW_6,	/*reserved*/
	SIPC_CH_ID_CDMA_DATA,
	SIPC_CH_ID_PCM_DATA,
	SIPC_CH_ID_TRANSFER_SCREEN,

	SIPC_CH_ID_PDP_0 = 10,	/*ID:10*/
	SIPC_CH_ID_PDP_1,
	SIPC_CH_ID_PDP_2,
	SIPC_CH_ID_PDP_3,
	SIPC_CH_ID_PDP_4,
	SIPC_CH_ID_PDP_5,
	SIPC_CH_ID_PDP_6,
	SIPC_CH_ID_PDP_7,
	SIPC_CH_ID_PDP_8,
	SIPC_CH_ID_PDP_9,
	SIPC_CH_ID_PDP_10,
	SIPC_CH_ID_PDP_11,
	SIPC_CH_ID_PDP_12,
	SIPC_CH_ID_PDP_13,
	SIPC_CH_ID_PDP_14,
	SIPC_CH_ID_BT_DUN,	/*ID:25*/
	SIPC_CH_ID_CIQ_DATA,
	SIPC_CH_ID_PDP_17,	/*reserved*/
	SIPC_CH_ID_CPLOG1,	/*ID:28*/
	SIPC_CH_ID_CPLOG2,	/*ID:29*/
	SIPC_CH_ID_LOOPBACK1,	/*ID:30*/
	SIPC_CH_ID_LOOPBACK2,	/*ID:31*/

	SIPC_CH_ID_CASS = 35,

	/* 32 ~ 214 are reserved */

	SIPC5_CH_ID_BOOT_0 = 215,
	SIPC5_CH_ID_BOOT_1,
	SIPC5_CH_ID_BOOT_2,
	SIPC5_CH_ID_BOOT_3,
	SIPC5_CH_ID_BOOT_4,
	SIPC5_CH_ID_BOOT_5,
	SIPC5_CH_ID_BOOT_6,
	SIPC5_CH_ID_BOOT_7,
	SIPC5_CH_ID_BOOT_8,
	SIPC5_CH_ID_BOOT_9,

	SIPC5_CH_ID_DUMP_0 = 225,
	SIPC5_CH_ID_DUMP_1,
	SIPC5_CH_ID_DUMP_2,
	SIPC5_CH_ID_DUMP_3,
	SIPC5_CH_ID_DUMP_4,
	SIPC5_CH_ID_DUMP_5,
	SIPC5_CH_ID_DUMP_6,
	SIPC5_CH_ID_DUMP_7,
	SIPC5_CH_ID_DUMP_8,
	SIPC5_CH_ID_DUMP_9,

	SIPC5_CH_ID_FMT_0 = 235,
	SIPC5_CH_ID_FMT_1,
	SIPC5_CH_ID_FMT_2,
	SIPC5_CH_ID_FMT_3,
	SIPC5_CH_ID_FMT_4,
	SIPC5_CH_ID_FMT_5,
	SIPC5_CH_ID_FMT_6,
	SIPC5_CH_ID_FMT_7,
	SIPC5_CH_ID_FMT_8,
	SIPC5_CH_ID_FMT_9,

	SIPC5_CH_ID_RFS_0 = 245,
	SIPC5_CH_ID_RFS_1,
	SIPC5_CH_ID_RFS_2,
	SIPC5_CH_ID_RFS_3,
	SIPC5_CH_ID_RFS_4,
	SIPC5_CH_ID_RFS_5,
	SIPC5_CH_ID_RFS_6,
	SIPC5_CH_ID_RFS_7,
	SIPC5_CH_ID_RFS_8,
	SIPC5_CH_ID_RFS_9,

	SIPC5_CH_ID_MAX = 255
};

struct __packed multi_frame_control {
	u8 id:7,
	   more:1;
};

enum io_mode {
	PIO,
	DMA
};

enum direction {
	TX = 0,
	UL = 0,
	AP2CP = 0,
	RX = 1,
	DL = 1,
	CP2AP = 1,
	TXRX = 2,
	ULDL = 2,
	MAX_DIR = 2
};

enum read_write {
	RD = 0,
	WR = 1,
	RDWR = 2
};

#define STR_CP_FAIL	"cp_fail"
#define STR_CP_WDT	"cp_wdt"	/* CP watchdog timer */

/* You can define modem specific attribute here.
 * It could be all the different behaviour between many modem vendor.
 */
enum link_attr_bit {
	LINK_ATTR_SBD_IPC,	/* IPC over SBD (from MIPI-LLI)		*/
	LINK_ATTR_IPC_ALIGNED,	/* IPC with 4-bytes alignment		*/
	LINK_ATTR_IOSM_MESSAGE,	/* IOSM message				*/
	LINK_ATTR_DPRAM_MAGIC,	/* DPRAM-style magic code validation	*/
	/*--------------------------------------------------------------*/
	LINK_ATTR_SBD_BOOT,	/* BOOT over SBD			*/
	LINK_ATTR_SBD_DUMP,	/* DUMP over SBD			*/
	LINK_ATTR_MEM_BOOT,	/* BOOT over legacy memory-type I/F	*/
	LINK_ATTR_MEM_DUMP,	/* DUMP over legacy memory-type I/F	*/
	LINK_ATTR_BOOT_ALIGNED,	/* BOOT with 4-bytes alignment		*/
	LINK_ATTR_DUMP_ALIGNED,	/* DUMP with 4-bytes alignment		*/
	/*--------------------------------------------------------------*/
	LINK_ATTR_XMIT_BTDLR,	/* Used to download CP bootloader	*/
	LINK_ATTR_XMIT_BTDLR_SPI, /* Download CP bootloader by SPI */
};
#define LINK_ATTR(b)	(0x1 << b)

enum iodev_attr_bit {
	ATTR_SIPC4,
	ATTR_SIPC5,
	ATTR_CDC_NCM,
	ATTR_MULTIFMT,
	ATTR_HANDOVER,
	ATTR_LEGACY_RFS,
	ATTR_RX_FRAGMENT,
	ATTR_SBD_IPC,		/* IPC using SBD designed from MIPI-LLI	*/
	ATTR_NO_LINK_HEADER,	/* Link-layer header is not needed	*/
	ATTR_NO_CHECK_MAXQ,     /* no need to check rxq overflow condition */
	ATTR_DUALSIM,		/* support Dual SIM */
	ATTR_OPTION_REGION,	/* region & operator info */
	ATTR_ZEROCOPY,		/* support Zerocopy : 0x1 << 12*/
};
#define IODEV_ATTR(b)	(0x1 << b)

/**
 * struct modem_io_t - declaration for io_device
 * @name:	device name
 * @id:		for SIPC4, contains format & channel information
 *		(id & 11100000b)>>5 = format  (eg, 0=FMT, 1=RAW, 2=RFS)
 *		(id & 00011111b)    = channel (valid only if format is RAW)
 *		for SIPC5, contains only 8-bit channel ID
 * @format:	device format
 * @io_type:	type of this io_device
 * @link_type:	link_devices to use this io_device
 *		for example, LINKDEV_SHMEM or LINKDEV_PCIE
 */
struct modem_io_t {
	char *name;
	u32 ch;
	enum dev_format format;
	enum modem_io io_type;
	u32 link_type;
	u32 attrs;
	char *option_region;
#if 1/*defined(CONFIG_LINK_DEVICE_MEMORY_SBD)*/
	unsigned int ul_num_buffers;
	unsigned int ul_buffer_size;
	unsigned int dl_num_buffers;
	unsigned int dl_buffer_size;
#endif
};

struct modemlink_pm_data {
	char *name;
	struct device *dev;
	/* link power contol 2 types : pin & regulator control */
	int (*link_ldo_enable)(bool);
	unsigned int gpio_link_enable;
	unsigned int gpio_link_active;
	unsigned int gpio_link_hostwake;
	unsigned int gpio_link_slavewake;
	int (*link_reconnect)(void);

	/* usb hub only */
	int (*port_enable)(int, int);
	int (*hub_standby)(void *);
	void *hub_pm_data;
	bool has_usbhub;

	/* cpu/bus frequency lock */
	atomic_t freqlock;
	int (*freq_lock)(struct device *dev);
	int (*freq_unlock)(struct device *dev);

	int autosuspend_delay_ms; /* if zero, the default value is used */
	void (*ehci_reg_dump)(struct device *);
};

struct modemlink_pm_link_activectl {
	int gpio_initialized;
	int gpio_request_host_active;
};

#if defined(CONFIG_LINK_DEVICE_SHMEM) || defined(CONFIG_LINK_DEVICE_PCIE)
enum shmem_type {
	REAL_SHMEM,
	C2C_SHMEM,
	MAX_SHMEM_TYPE
};

struct modem_mbox {
	unsigned int mbx_ap2cp_msg;
	unsigned int mbx_cp2ap_msg;
	unsigned int mbx_ap2cp_wakeup;	/* CP_WAKEUP	*/
	unsigned int mbx_cp2ap_wakeup;	/* AP_WAKEUP	*/
	unsigned int mbx_ap2cp_status;	/* AP_STATUS	*/
	unsigned int mbx_cp2ap_status;	/* CP_STATUS	*/
	unsigned int mbx_cp2ap_wakelock; /* Wakelock for VoLTE */
	unsigned int mbx_cp2ap_ratmode; /* Wakelock for pcie */
	unsigned int mbx_ap2cp_kerneltime; /* Kernel time */

	unsigned int int_ap2cp_msg;
	unsigned int int_ap2cp_active;
	unsigned int int_ap2cp_wakeup;
	unsigned int int_ap2cp_status;
	unsigned int int_ap2cp_lcd_status;
	unsigned int int_ap2cp_uart_noti;

	unsigned int irq_cp2ap_msg;
	unsigned int irq_cp2ap_active;
	unsigned int irq_cp2ap_wakeup;
	unsigned int irq_cp2ap_status;
	unsigned int irq_cp2ap_wakelock;
	unsigned int irq_cp2ap_rat_mode;
	unsigned int irq_cp2ap_change_ul_path;
};
#endif

/* platform data */
struct modem_data {
	char *name;
	u32 cp_num;

	struct modem_mbox *mbx;
	struct mem_link_device *mld;

	/* Modem component */
	enum modem_network modem_net;
	enum modem_t modem_type;

	u32 link_type;
	char *link_name;
	unsigned long link_attrs;	/* Set of link_attr_bit flags	*/
	enum modem_interrupt interrupt_types;

	u32 protocol;

	/* SIPC version */
	enum sipc_ver ipc_version;

	/* Information of IO devices */
	unsigned int num_iodevs;
	struct modem_io_t *iodevs;

	/* Modem link PM support */
	struct modemlink_pm_data *link_pm_data;

	/* SIM Detect polarity */
	bool sim_polarity;

	/* legacy buffer setting */
	u32 legacy_fmt_head_tail_offset;
	u32 legacy_fmt_buffer_offset;
	u32 legacy_fmt_txq_size;
	u32 legacy_fmt_rxq_size;
	u32 legacy_raw_head_tail_offset;
	u32 legacy_raw_buffer_offset;
	u32 legacy_raw_txq_size;
	u32 legacy_raw_rxq_size;

	/* several 4 byte length info in ipc region */
	u32 offset_ap_version;
	u32 offset_cp_version;
	u32 offset_cmsg_offset;
	u32 offset_srinfo_offset;
	u32 offset_clk_table_offset;
	u32 offset_buff_desc_offset;

	/* ctrl messages between cp and ap */
	u32 ap2cp_msg[2];
	u32 cp2ap_msg[2];
	u32 cp2ap_united_status[2];
	u32 ap2cp_united_status[2];
	u32 ap2cp_kerneltime[2];
	u32 ap2cp_kerneltime_sec[2];
	u32 ap2cp_kerneltime_usec[2];

	/* Status Bit Info */
	unsigned int sbi_lte_active_mask;
	unsigned int sbi_lte_active_pos;
	unsigned int sbi_cp_status_mask;
	unsigned int sbi_cp_status_pos;
	unsigned int sbi_cp2ap_wakelock_mask;
	unsigned int sbi_cp2ap_wakelock_pos;
	unsigned int sbi_cp2ap_rat_mode_mask;
	unsigned int sbi_cp2ap_rat_mode_pos;

	unsigned int sbi_pda_active_mask;
	unsigned int sbi_pda_active_pos;
	unsigned int sbi_ap_status_mask;
	unsigned int sbi_ap_status_pos;

	unsigned int sbi_ap2cp_kerneltime_sec_mask;
	unsigned int sbi_ap2cp_kerneltime_sec_pos;
	unsigned int sbi_ap2cp_kerneltime_usec_mask;
	unsigned int sbi_ap2cp_kerneltime_usec_pos;

	unsigned int sbi_uart_noti_mask;
	unsigned int sbi_uart_noti_pos;
	unsigned int sbi_crash_type_mask;
	unsigned int sbi_crash_type_pos;
	unsigned int sbi_ds_det_mask;
	unsigned int sbi_ds_det_pos;
	unsigned int sbi_lcd_status_mask;
	unsigned int sbi_lcd_status_pos;

	/* ulpath offset for 2CP models */
	u32 ulpath_offset;

	/* control message offset */
	u32 cmsg_offset;

	/* srinfo settings */
	u32 srinfo_offset;
	u32 srinfo_size;

	/* clk_table offset */
	u32 clk_table_offset;

	/* new SIT buffer descriptor offset */
	u32 buff_desc_offset;

#ifdef CONFIG_MODEM_IF_LEGACY_QOS
	/* SIT priority queue info */
	u32 legacy_raw_qos_head_tail_offset;
	u32 legacy_raw_qos_buffer_offset;
	u32 legacy_raw_qos_txq_size;
	u32 legacy_raw_qos_rxq_size; /* unused for now */
#endif
	struct cp_btl btl;	/* CP background trace log */

	void (*gpio_revers_bias_clear)(void);
	void (*gpio_revers_bias_restore)(void);
};

struct modem_irq {
	spinlock_t lock;
	unsigned int num;
	char name[MAX_NAME_LEN];
	unsigned long flags;
	bool active;
	bool registered;
};

#ifdef CONFIG_OF
#define mif_dt_read_enum(np, prop, dest) \
	do { \
		u32 val; \
		if (of_property_read_u32(np, prop, &val)) { \
			mif_err("%s is not defined\n", prop); \
			return -EINVAL; \
		} \
		dest = (__typeof__(dest))(val); \
	} while (0)

#define mif_dt_read_bool(np, prop, dest) \
	do { \
		u32 val; \
		if (of_property_read_u32(np, prop, &val)) { \
			mif_err("%s is not defined\n", prop); \
			return -EINVAL; \
		} \
		dest = val ? true : false; \
	} while (0)

#define mif_dt_read_string(np, prop, dest) \
	do { \
		if (of_property_read_string(np, prop, \
				(const char **)&dest)) { \
			mif_err("%s is not defined\n", prop); \
			return -EINVAL; \
		} \
	} while (0)

#define mif_dt_read_u32(np, prop, dest) \
	do { \
		u32 val; \
		if (of_property_read_u32(np, prop, &val)) { \
			mif_err("%s is not defined\n", prop); \
			return -EINVAL; \
		} \
		dest = val; \
	} while (0)

#define mif_dt_read_u32_noerr(np, prop, dest) \
	do { \
		u32 val; \
		if (!of_property_read_u32(np, prop, &val)) \
			dest = val; \
	} while (0)
#endif

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

#endif
