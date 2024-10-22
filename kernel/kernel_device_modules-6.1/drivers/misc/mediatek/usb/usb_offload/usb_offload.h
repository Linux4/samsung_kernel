/* SPDX-License-Identifier: GPL-2.0 */
/*
 * MTK USB Offload Driver
 * *
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Jeremy Chou <jeremy.chou@mediatek.com>
 */

#ifndef __USB_OFFLOAD_H__
#define __USB_OFFLOAD_H__

#include <linux/types.h>
#include <sound/asound.h>

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/usb/audio.h>
#include <linux/usb/audio-v2.h>
#include <linux/usb/audio-v3.h>
#include <linux/uaccess.h>
#include <sound/pcm.h>
#include <sound/core.h>
#include <sound/asound.h>
#include <linux/usb.h>
#include <linux/iommu.h>
#include <linux/dma-mapping.h>

#define MIN_USB_OFFLOAD_SHIFT (8)
#define MIN_USB_OFFLOAD_POOL_SIZE (1 << MIN_USB_OFFLOAD_SHIFT)

#define BUF_CTX_SIZE					31
#define TR_MAX_SEG						((15 * 2 + 1) * 2)
#define EV_MAX_SEG						1
#define BUF_SEG_SIZE					(TR_MAX_SEG)
#define ERST_SIZE						16
#define ERST_NUMBER						EV_MAX_SEG
#define USB_OFFLOAD_TRBS_PER_SEGMENT	256
#define USB_OFFLOAD_TRB_SEGMENT_SIZE	(USB_OFFLOAD_TRBS_PER_SEGMENT*16)

struct usb_offload_mem_info {
	unsigned long long phy_addr;
	unsigned long long va_addr;
	unsigned long long size;
	unsigned char *vir_addr;
	bool is_valid;
	struct gen_pool *pool;
};

/* struct usb_offload_buffer
 * @dma_addr: physical address (not accessible from main CPU).
 * @dma_area: viutal address for main CPU.
 * @is_sram: indicate it's placed on sram.
 * @type: indicate type of sram, useful when is_sram=1.
 */
struct usb_offload_buffer {
	/* -- DMA -- */
	unsigned char *dma_area;
	dma_addr_t dma_addr;
	size_t dma_bytes;
	bool allocated;
	bool is_sram;
	bool is_rsv;
	u8 type;
	struct list_head list;
};

enum usb_offload_mem_id {
	USB_OFFLOAD_MEM_DRAM_ID = 0,
	USB_OFFLOAD_MEM_SRAM_ID = 1,
	USB_OFFLOAD_MEM_NUM,
};

enum {
	ENABLE_STREAM,
	DISABLE_STREAM,
};
#define USB_OFFLOAD_IOC_MAGIC 'U'
/* Enable/Disable USB Offload */
#define USB_OFFLOAD_INIT_ADSP		_IOW(USB_OFFLOAD_IOC_MAGIC, 0, int)
/* Enable USB Stream */
#define USB_OFFLOAD_ENABLE_STREAM	_IOW(USB_OFFLOAD_IOC_MAGIC, 1, unsigned int)
/* Disable USB Stream */
#define USB_OFFLOAD_DISABLE_STREAM	_IOW(USB_OFFLOAD_IOC_MAGIC, 2, unsigned int)

#define BUS_INTERVAL_FULL_SPEED 1000 /* in us */
#define BUS_INTERVAL_HIGHSPEED_AND_ABOVE 125 /* in us */
#define MAX_BINTERVAL_ISOC_EP 16
#define DEV_RELEASE_WAIT_TIMEOUT 10000 /* in ms */

enum usb_audio_stream_status {
	USB_AUDIO_STREAM_STATUS_ENUM_MIN_VAL = INT_MIN,
	USB_AUDIO_STREAM_REQ_START = 0,
	USB_AUDIO_STREAM_REQ_STOP = 1,
	USB_AUDIO_STREAM_STATUS_ENUM_MAX_VAL = INT_MAX,
};

enum usb_audio_device_speed {
	USB_AUDIO_DEVICE_SPEED_ENUM_MIN_VAL = INT_MIN,
	USB_AUDIO_DEVICE_SPEED_INVALID = 0,
	USB_AUDIO_DEVICE_SPEED_LOW = 1,
	USB_AUDIO_DEVICE_SPEED_FULL = 2,
	USB_AUDIO_DEVICE_SPEED_HIGH = 3,
	USB_AUDIO_DEVICE_SPEED_SUPER = 4,
	USB_AUDIO_DEVICE_SPEED_SUPER_PLUS = 5,
	USB_AUDIO_DEVICE_SPEED_ENUM_MAX_VAL = INT_MAX,
};

enum ssusb_offload_mode {
	SSUSB_OFFLOAD_MODE_NONE = 0,
	SSUSB_OFFLOAD_MODE_D,
	SSUSB_OFFLOAD_MODE_S,
};

/* struct ssusb_offload */
struct ssusb_offload {
	struct device *dev;
	int	(*get_mode)(struct device *dev);
};

struct mem_info_xhci {
	bool adv_lowpwr;
	unsigned int sram_version;
	unsigned int xhci_dram_addr;
	unsigned int xhci_dram_size;
	unsigned int xhci_sram_addr;
	unsigned int xhci_sram_size;
	unsigned long long ev_ring;
	unsigned long long erst_table;
};

struct usb_audio_stream_info {
	unsigned char enable;
	unsigned char pcm_card_num;
	unsigned char pcm_dev_num;
	unsigned char direction;

	unsigned int bit_depth;
	unsigned int number_of_ch;
	unsigned int bit_rate;

	unsigned char service_interval_valid;
	unsigned int service_interval;
	unsigned char xhc_irq_period_ms;
	unsigned char xhc_urb_num;
	unsigned char dram_size;
	unsigned char dram_cnt;
	unsigned char start_thld;
	unsigned char stop_thld;
	unsigned int pcm_size;

	snd_pcm_format_t audio_format;
};

struct usb_audio_stream_msg {
	unsigned char status_valid;
	enum usb_audio_stream_status status;
	unsigned char internal_status_valid;
	unsigned int internal_status;
	unsigned char slot_id_valid;
	unsigned int slot_id;
	unsigned char usb_token_valid;
	unsigned int usb_token;
	unsigned char pcm_card_num_valid;
	unsigned char pcm_card_num;
	unsigned char pcm_dev_num_valid;
	unsigned char pcm_dev_num;
	unsigned char direction_valid;
	unsigned char direction;
	unsigned char std_as_opr_intf_desc_valid;
	struct usb_interface_descriptor std_as_opr_intf_desc;
	unsigned char std_as_data_ep_desc_valid;
	struct usb_endpoint_descriptor std_as_data_ep_desc;
	unsigned char std_as_sync_ep_desc_valid;
	struct usb_endpoint_descriptor std_as_sync_ep_desc;
	unsigned char usb_audio_spec_revision_valid;
	u16 usb_audio_spec_revision;
	unsigned char data_path_delay_valid;
	unsigned char data_path_delay;
	unsigned char usb_audio_subslot_size_valid;
	unsigned char usb_audio_subslot_size;
	unsigned char interrupter_num_valid;
	unsigned char interrupter_num;
	unsigned char speed_info_valid;
	enum usb_audio_device_speed speed_info;
	unsigned char controller_num_valid;
	unsigned char controller_num;
	unsigned long long urb_start_addr;
	unsigned int urb_size;
	unsigned int urb_num;
	unsigned int urb_packs;
	struct usb_audio_stream_info uainfo;
};

struct usb_offload_urb_msg {
	unsigned char enable;
	unsigned char direction;
	unsigned int slot_id;
	struct usb_interface_descriptor intf_desc;
	struct usb_endpoint_descriptor ep_desc;
	unsigned long long urb_start_addr;
	unsigned int urb_size;
	unsigned long long first_trb;
	unsigned char cycle_state;
};

struct usb_offload_urb_complete {
	unsigned char direction;
	unsigned int slot_id;
	unsigned int ep_id;
	unsigned long long urb_start_addr;
	unsigned int actual_length;
	unsigned char more_complete;
	unsigned long long cur_trb;
	unsigned char cycle_state;
	int status;
};

struct usb_offload_xhci_ep {
	unsigned char direction;
	unsigned int slot_id;
	unsigned int ep_id;
	unsigned long long cur_trb;
	unsigned char cycle_state;
};

struct intf_info {
	unsigned int data_ep_pipe;
	unsigned int sync_ep_pipe;
	u8 *xfer_buf;
	u8 intf_num;
	u8 pcm_card_num;
	u8 pcm_dev_num;
	u8 direction;
	bool in_use;
};

struct usb_audio_dev {
	struct usb_device *udev;
	/* audio control interface */
	struct usb_host_interface *ctrl_intf;
	unsigned int card_num;
	unsigned int usb_core_id;
	atomic_t in_use;
	struct kref kref;
	wait_queue_head_t disconnect_wq;

	/* interface specific */
	int num_intf;
	struct intf_info *info;
};

/* struct usb_offload_dev
 * @event_ring: event ring for interrupter target 1.
 * @erst: event ring segment table for interrupter target 1.
 * @num_entries_in_use: number of entry of erst.
 * @enable_adv_lowpwr: if platform supports sram mode.
 * @adv_lowpwr: in this round, if it's under sram mode.
 * @smc_ctrl: if platform needs specific action in tfa.
 * @smc_suspned/resume: identifier of suspend/resume smc case.
 */
struct usb_offload_dev {
	struct device *dev;
	struct xhci_hcd *xhci;
	struct xhci_ring *event_ring;
	struct xhci_erst *erst;
	struct xhci_erst_entry *backup_erst;
	unsigned char *backup_ev_ring;
	unsigned int num_entries_in_use;
	u32 intr_num;
	unsigned long card_slot;
	unsigned int card_num;
	bool smc_ctrl;
	int smc_suspend;
	int smc_resume;
	bool enable_adv_lowpwr;
	bool adv_lowpwr;
	bool adv_lowpwr_dl_only;
	bool is_streaming;
	bool tx_streaming;
	bool rx_streaming;
	bool adsp_inited;
	bool connected;
	bool opened;
	bool adsp_exception;
	bool adsp_ready;
	struct ssusb_offload *ssusb_offload_notify;
	struct mutex dev_lock;
	u64 *mapping_table;
};

extern int ssusb_offload_register(struct ssusb_offload *offload);
extern int ssusb_offload_unregister(struct device *dev);

extern bool usb_offload_ready(void);

extern struct usb_offload_dev *uodev;
extern unsigned int usb_offload_log;
#define USB_OFFLOAD_DBG(fmt, args...) do { \
	if (usb_offload_log > 0) \
		pr_info("UD, %s(%d) " fmt, __func__, __LINE__, ## args); \
	} while (0)
extern unsigned int debug_memory_log;
#define USB_OFFLOAD_MEM_DBG(fmt, args...) do { \
	if (debug_memory_log > 0) \
		pr_info("UD, %s(%d) " fmt, __func__, __LINE__, ## args); \
	} while (0)

#define USB_OFFLOAD_INFO(fmt, args...) do { \
	if (1) \
		pr_info("UO, %s(%d) " fmt, __func__, __LINE__, ## args); \
	} while (0)

#define USB_OFFLOAD_PROBE(fmt, args...) do { \
	if (1) \
		pr_info("UD, %s(%d) " fmt, __func__, __LINE__, ## args); \
	} while (0)

#define USB_OFFLOAD_ERR(fmt, args...) do { \
	if (1) \
		pr_info("UD, %s(%d) " fmt, __func__, __LINE__, ## args); \
	} while (0)

extern u32 sram_version;
extern struct usb_offload_buffer *usb_offload_get_ring_buf(dma_addr_t phy);
extern enum usb_offload_mem_id lowpwr_mem_type(void);
extern int xhci_mtk_realloc_transfer_ring(unsigned int slot_id, unsigned int ep_id,
	enum usb_offload_mem_id mem_type);
extern int soc_init_aud_intf(void);
extern int mtk_offload_init_rsv_dram(int min_alloc_order);
extern int mtk_offload_init_rsv_sram(int min_alloc_order);
extern int mtk_offload_deinit_rsv_sram(void);
extern int mtk_offload_rsv_sram_pwr_ctrl(bool power);
extern int mtk_offload_alloc_mem(struct usb_offload_buffer *buf, unsigned int size,
	int align, enum usb_offload_mem_id mem_id, bool is_rsv);
extern int mtk_offload_free_mem(struct usb_offload_buffer *buf);
extern bool mtk_offload_is_advlowpwr(struct usb_offload_dev *udev);
extern int mtk_offload_get_rsv_mem_info(enum usb_offload_mem_id mem_id,
	unsigned int *phys, unsigned int *size);
extern bool is_sram(enum usb_offload_mem_id id);

extern unsigned int hid_disable_offload;
extern void usb_offload_hid_probe(void);
extern void usb_offload_hid_finish(void);
extern bool xhci_mtk_skip_hid_urb(struct xhci_hcd *xhci, struct urb *urb);
extern int usb_offload_hid_start(void);
extern void usb_offload_register_ipi_recv(void);
#endif /* __USB_OFFLOAD_H__ */
