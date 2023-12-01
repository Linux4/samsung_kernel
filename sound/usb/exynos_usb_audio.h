// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *   USB Audio Driver for Exynos
 *
 *   Copyright (c) 2017 by Kyounghye Yun <k-hye.yun@samsung.com>
 *
 */

#ifndef __LINUX_USB_EXYNOS_AUDIO_H
#define __LINUX_USB_EXYNOS_AUDIO_H

#ifndef CONFIG_SND_EXYNOS_USB_AUDIO_GIC
#include <sound/samsung/abox.h>
#endif
#include "usbaudio.h"

/* for KM */

#define USB_AUDIO_MEM_BASE	0xC0000000

#define USB_AUDIO_SAVE_RESTORE	(USB_AUDIO_MEM_BASE)
#define USB_AUDIO_DEV_CTX	(USB_AUDIO_SAVE_RESTORE+PAGE_SIZE)
#define USB_AUDIO_INPUT_CTX	(USB_AUDIO_DEV_CTX+PAGE_SIZE)
#define USB_AUDIO_OUT_DEQ	(USB_AUDIO_INPUT_CTX+PAGE_SIZE)
#define USB_AUDIO_IN_DEQ	(USB_AUDIO_OUT_DEQ+PAGE_SIZE)
#define USB_AUDIO_FBOUT_DEQ	(USB_AUDIO_IN_DEQ+PAGE_SIZE)
#define USB_AUDIO_FBIN_DEQ	(USB_AUDIO_FBOUT_DEQ+PAGE_SIZE)
#define USB_AUDIO_ERST		(USB_AUDIO_FBIN_DEQ+PAGE_SIZE)
#define USB_AUDIO_DESC		(USB_AUDIO_ERST+PAGE_SIZE)
#define USB_AUDIO_PCM_OUTBUF	(USB_AUDIO_MEM_BASE+0x100000)
#define USB_AUDIO_PCM_INBUF	(USB_AUDIO_MEM_BASE+0x800000)

#if defined(CONFIG_SOC_EXYNOS9820)
#define USB_AUDIO_XHCI_BASE	0x10c00000
#elif defined(CONFIG_SOC_EXYNOS9830)
#define USB_AUDIO_XHCI_BASE	0x10e00000
#elif defined(CONFIG_SOC_EXYNOS2100)
#define USB_AUDIO_XHCI_BASE	0x10e00000
#define USB_URAM_BASE		0x10ff0000
#define USB_URAM_SIZE		SZ_1K * 60
#elif defined(CONFIG_SOC_S5E9815)
#define USB_AUDIO_XHCI_BASE	0x12210000
#define USB_URAM_BASE		0x122a0000
#define USB_URAM_SIZE		SZ_1K * 24
#elif defined(CONFIG_SOC_S5E9925)
#define USB_AUDIO_XHCI_BASE	0x10B00000
#define USB_URAM_BASE		0x02a00000
#define USB_URAM_SIZE		SZ_1K * 24
#elif defined(CONFIG_SOC_S5E9935)
#define USB_AUDIO_XHCI_BASE	0x10B00000
#define USB_URAM_BASE		0x02a00000
#define USB_URAM_SIZE		SZ_1K * 24
#define EXYNOS_URAM_ABOX_EMPTY_ADDR	0x02a03D00
#define EMPTY_SIZE			128
#elif defined(CONFIG_SOC_S5E8825)
#define USB_AUDIO_XHCI_BASE	0x13200000
#define USB_URAM_BASE		0x13300000
#define USB_URAM_SIZE		SZ_1K * 24
#else
#define USB_AUDIO_XHCI_BASE     0x13200000
#define USB_URAM_BASE           0x13300000
#define USB_URAM_SIZE           SZ_1K * 24
#define EXYNOS_URAM_ABOX_EMPTY_ADDR	0x13303D00
#define EMPTY_SIZE			128
#endif

#define USB_AUDIO_CONNECT		(1 << 0)
#define USB_AUDIO_REMOVING		(1 << 1)
#define USB_AUDIO_DISCONNECT		(1 << 2)
#define USB_AUDIO_TIMEOUT_PROBE	(1 << 3)

#define DISCONNECT_TIMEOUT	(500)

#define AUDIO_MODE_NORMAL		0
#define AUDIO_MODE_RINGTONE		1
#define AUDIO_MODE_IN_CALL		2
#define AUDIO_MODE_IN_COMMUNICATION	3
#define AUDIO_MODE_CALL_SCREEN		4

#define	CALL_INTERVAL_THRESHOLD		3

#define USB_AUDIO_CONNECT		(1 << 0)
#define USB_AUDIO_REMOVING		(1 << 1)
#define USB_AUDIO_DISCONNECT		(1 << 2)
#define USB_AUDIO_TIMEOUT_PROBE	(1 << 3)

#define DISCONNECT_TIMEOUT	(500)

#define PHY_MODE_ABOX_POWER	0x32
#define PHY_MODE_CALL_ENTER	0x33
#define PHY_MODE_CALL_EXIT	0x34

enum USB_GIC_MSG_TYPE {
	GIC_USB_PCM_OPEN,	/* USB -> ABOX */
	GIC_USB_DESC,
	GIC_USB_XHCI,
	GIC_USB_L2,
	GIC_USB_CONN,
	GIC_USB_CTRL,
	GIC_USB_SET_INTF,
	GIC_USB_SAMPLE_RATE,
	GIC_USB_PCM_BUF,
	GIC_USB_MAP,
	GIC_USB_UNMAP,
	GIC_USB_TASK = 0x80,	/* ABOX -> USB */
	GIC_USB_STOP_DONE,
};

struct host_data {
	dma_addr_t out_data_dma;
	dma_addr_t in_data_dma;
	void *out_data_addr;
	void *in_data_addr;
};

extern struct host_data xhci_data;

struct exynos_usb_audio {
	struct usb_device *udev;
	struct platform_device *abox;
	struct platform_device *hcd_pdev;
	struct phy *phy;
	struct snd_usb_audio *chip;
	struct mutex lock;
	struct mutex msg_lock;
	struct work_struct usb_work;
	struct completion in_conn_stop;
	struct completion out_conn_stop;
	struct completion discon_done;

	u64 out_buf_addr;
	u64 in_buf_addr;
	u64 pcm_offset;
	u64 desc_addr;
	u64 offset;

	/* for hw_info */
	u64 dcbaa_dma;
	u64 in_ctx;
	u64 out_ctx;
	u64 erst_addr;

	int speed;
	/* 1: in, 0: out */
	int set_ep;
	int is_audio;
	int is_first_probe;
	u8 indeq_map_done;
	u8 outdeq_map_done;
	u8 fb_indeq_map_done;
	u8 fb_outdeq_map_done;
	u32 pcm_open_done;
	u32 usb_audio_state;

	struct snd_kcontrol *kctl;
	u32 user_scenario;

	void *pcm_buf;
	u64 save_dma;

	bool use_uram;
};

struct hcd_hw_info {
	/* for XHCI */
	int slot_id;
	dma_addr_t erst_addr;
	dma_addr_t dcbaa_dma;
	dma_addr_t in_ctx;
	dma_addr_t out_ctx;
	dma_addr_t save_dma;
	u64 cmd_ring;
	/* Data Stream EP */
	u64 old_out_deq;
	u64 old_in_deq;
	u64 out_deq;
	u64 in_deq;
	int in_ep;
	int out_ep;
	/* feedback ep */
	int fb_in_ep;
	int fb_out_ep;
	int feedback;
	u64 fb_old_out_deq;
	u64 fb_old_in_deq;
	u64 fb_out_deq;
	u64 fb_in_deq;
	/* Device Common Information */
	int speed;
	void *out_buf;
	u64 out_dma;
	void *in_buf;
	u64 in_dma;
	int use_uram;
	int rawdesc_length;
	/* phy */
	struct phy *phy;
};

#ifdef CONFIG_SND_EXYNOS_USB_AUDIO_GIC
struct usb_audio_msg {
	int type;
	unsigned int param1;
	unsigned int param2;
	unsigned int param3;
	unsigned int param4;
	unsigned int response;
};

struct usb_audio_gic_data {
	struct device *dev;
	void __iomem *gicd_base;
	void __iomem *gicc_base;
	phys_addr_t gicd_base_phys;
	phys_addr_t gicc_base_phys;
};

int exynos_usb_audio_connect(struct usb_interface *intf);
void exynos_usb_audio_disconn(struct usb_interface *intf);
int exynos_usb_audio_set_interface(struct usb_device *udev,
				   struct usb_host_interface *intf, int iface,
				   int alt);
int exynos_usb_audio_set_rate(int iface, int rate, int alt);
int exynos_usb_audio_pcmbuf(struct usb_device *udev, int iface);
int exynos_usb_audio_set_pcmintf(struct usb_interface *intf,
				 int iface, int alt, int direction);
int exynos_usb_audio_pcm_control(struct usb_device *udev,
				 enum snd_vendor_pcm_open_close onoff,
				 int direction);
int exynos_usb_audio_set_pcm_binterval(const struct audioformat *fp,
		const struct audioformat *found, int *cur_attr,
		int *attr);
int exynos_usb_audio_add_control(struct snd_usb_audio *chip);
#endif
int exynos_usb_audio_init(struct device *dev, struct platform_device *pdev);
int exynos_usb_audio_exit(void);
#endif				/* __LINUX_USB_EXYNOS_AUDIO_H */
