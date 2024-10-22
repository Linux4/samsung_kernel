// SPDX-License-Identifier: GPL-2.0
/*
 * MTK USB Offload Driver
 * *
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Jeremy Chou <jeremy.chou@mediatek.com>
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/usb.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
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
#include <linux/iommu.h>
#include <linux/dma-mapping.h>
#include <adsp_helper.h>
#include <trace/hooks/audio_usboffload.h>
#include "clk-mtk.h"
#if IS_ENABLED(CONFIG_USB_NOTIFY_LAYER)
#include <linux/usb_notify.h>
#endif

#if IS_ENABLED(CONFIG_SND_USB_AUDIO)
#include "usbaudio.h"
#include "card.h"
#include "helper.h"
#include "pcm.h"
#include "power.h"
#endif
#if IS_ENABLED(CONFIG_DEVICE_MODULES_USB_XHCI_MTK)
#include "xhci.h"
#include "xhci-mtk.h"
#endif
#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
#include <adsp_helper.h>
#include "audio_messenger_ipi.h"
#include "audio_task.h"
#include "audio_controller_msg_id.h"
#endif
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
#include <scp.h>
#endif
#include "usb_boost.h"
#include "usb_offload.h"
#include "audio_task_usb_msg_id.h"

enum offload_smc_request {
	OFFLOAD_SMC_AUD_SUSPEND = 6,
	OFFLOAD_SMC_AUD_RESUME = 7,
};

static DEFINE_MUTEX(register_mutex);

struct usb_offload_buffer *buf_dcbaa;
struct usb_offload_buffer *buf_ctx;
struct usb_offload_buffer *buf_seg;
struct usb_offload_buffer *buf_ev_table;
struct usb_offload_buffer *buf_ev_ring;
struct usb_offload_buffer buf_allocated[2];

static bool is_vcore_hold;

unsigned int usb_offload_log;
module_param(usb_offload_log, uint, 0644);
MODULE_PARM_DESC(usb_offload_log, "Enable/Disable USB Offload log");

unsigned int debug_memory_log;
module_param(debug_memory_log, uint, 0644);
MODULE_PARM_DESC(debug_memory_log, "Enable/Disable Debug Memory log");

u32 sram_version = 0x2;
module_param(sram_version, uint, 0644);
MODULE_PARM_DESC(sram_version, "Sram Version Control");

/* region counter for ERST/EV_RING/TR_RING/URB */
static u8 rsv_region;
#define REGION_ERST_SFT     (0)
#define REGION_EV_RING_SFT  (1)
#define REGION_TR_RING_SFT  (2)
#define REGION_URB_SFT      (4)
#define REGION_ERST_MSK     (0x1 << REGION_ERST_SFT)
#define REGION_EV_RING_MSK  (0x1 << REGION_EV_RING_SFT)
#define REGION_TR_RING_MSK  (0x3 << REGION_TR_RING_SFT)
#define REGION_URB_MSK      (0x3 << REGION_URB_SFT)

#define inc_region(region) do { \
	u8 temp = ((rsv_region & REGION_##region##_MSK) >> REGION_##region##_SFT); \
	\
	if (sram_version != 0x3)\
		break; \
	temp = temp + 1; \
	rsv_region &= ~(REGION_##region##_MSK); \
	rsv_region |= ((temp << REGION_##region##_SFT) & REGION_##region##_MSK); \
	USB_OFFLOAD_MEM_DBG("rsv_region:0x%x\n", rsv_region); \
} while(0)
#define dec_region(region) do { \
	u8 temp = ((rsv_region & REGION_##region##_MSK) >> REGION_##region##_SFT); \
	\
	if (!temp || sram_version != 0x3)\
		break; \
	temp = temp - 1; \
	rsv_region &= ~(REGION_##region##_MSK); \
	rsv_region |= ((temp << REGION_##region##_SFT) & REGION_##region##_MSK); \
	USB_OFFLOAD_MEM_DBG("rsv_region:0x%x\n", rsv_region); \
} while(0)

enum usb_offload_mem_id lowpwr_mem_type(void)
{
	return uodev->adv_lowpwr ?
		USB_OFFLOAD_MEM_SRAM_ID : USB_OFFLOAD_MEM_DRAM_ID;
}

static void print_all_memory(void)
{
	int i;

	if (buf_dcbaa && buf_dcbaa->allocated)
		USB_OFFLOAD_MEM_DBG("dcbaa:0x%llx\n", buf_dcbaa->dma_addr);

	for (i = 0; i < BUF_CTX_SIZE; i ++) {
		if (buf_ctx && buf_ctx[i].allocated)
			USB_OFFLOAD_MEM_DBG("ctx%d:0x%llx\n", i, buf_ctx[i].dma_addr);
	}

	if (buf_ev_table && buf_ev_table->allocated)
		USB_OFFLOAD_MEM_DBG("erst:0x%llx\n", buf_ev_table->dma_addr);

	if (buf_ev_ring && buf_ev_ring->allocated)
		USB_OFFLOAD_MEM_DBG("ev_ring:0x%llx\n", buf_ev_ring->dma_addr);

	for (i = 0; i < BUF_SEG_SIZE; i ++) {
		if (buf_seg && buf_seg[i].allocated)
			USB_OFFLOAD_MEM_DBG("tr_ring%d:0x%llx\n", i, buf_seg[i].dma_addr);
	}

	for (i = 0; i < 2; i ++) {
		if (buf_allocated[i].allocated)
			USB_OFFLOAD_MEM_DBG("urb%d:0x%llx\n", i, buf_allocated[i].dma_addr);
	}

	USB_OFFLOAD_MEM_DBG("rsv_region:0x%x\n", rsv_region);
}

static bool ir_lost;
static struct usb_audio_dev uadev[SNDRV_CARDS];
struct usb_offload_dev *uodev;
static struct snd_usb_audio *usb_chip[SNDRV_CARDS];

static void uaudio_disconnect_cb(struct snd_usb_audio *chip);

static int mtk_usb_offload_free_allocated(bool is_in);
static void xhci_mtk_free_ring(struct xhci_hcd *xhci, struct xhci_ring *ring);
static int xhci_mtk_alloc_erst(struct usb_offload_dev *udev);
static int xhci_mtk_alloc_event_ring(struct usb_offload_dev *udev);
static int xhci_initialize_ir(struct usb_offload_dev *udev);
static void xhci_free_ir(struct usb_offload_dev *udev);
static void xhci_mtk_free_erst(struct usb_offload_dev *udev);
static void xhci_mtk_free_event_ring(struct usb_offload_dev *udev);
static int xhci_mtk_ring_expansion(struct xhci_hcd *xhci,
	struct xhci_ring *ring, dma_addr_t phys, void *vir);
static int xhci_mtk_update_erst(struct usb_offload_dev *udev,
	struct xhci_segment *first,	struct xhci_segment *last, unsigned int num_segs);
static int xhci_mtk_realloc_isoc_ring(struct snd_usb_substream *subs);
static void fake_sram_pwr_ctrl(bool power);

static int set_interface(struct usb_device *udev,
			     struct usb_host_interface *alts,
			     int iface, int alt)
{
	return 0;
}
static int set_pcm_intf(struct usb_interface *intf, int iface, int alt,
			    int direction, struct snd_usb_substream *subs)
{
	return 0;
}
static int set_pcm_connection(struct usb_device *udev,
				  enum snd_vendor_pcm_open_close onoff,
				  int direction)
{
	return 0;
}

static struct snd_usb_audio_vendor_ops alsa_ops = {
	.set_interface = set_interface,
	.set_pcm_intf = set_pcm_intf,
	.set_pcm_connection = set_pcm_connection,
};

static void memory_cleanup(void)
{
	USB_OFFLOAD_MEM_DBG("++\n");
	/* urb buffers aren't freed if plug-out event is prior to disable stream*/
	mtk_usb_offload_free_allocated(true);
	mtk_usb_offload_free_allocated(false);

	/* free interrupter resource */
	xhci_free_ir(uodev);

	/* disconnect event may came prior to freeing transfer ring
	 * check rsv_region before powering off sram
	 */
	if (sram_version == 0x3 && !rsv_region) {
		fake_sram_pwr_ctrl(false);
		rsv_region = 0;
	}

	USB_OFFLOAD_DBG("is_vcore_hold:%d\n", is_vcore_hold);
	if (is_vcore_hold) {
		usb_boost_vcore_control(false);
		USB_OFFLOAD_INFO("request releasing vcore\n");
		is_vcore_hold = false;
	}

	USB_OFFLOAD_MEM_DBG("--\n");
}

static void init_fake_rsv_sram(void)
{
	int ret;

	if (uodev->adv_lowpwr) {
		ret = mtk_offload_init_rsv_sram(MIN_USB_OFFLOAD_SHIFT);

		if (ret != 0) {
			USB_OFFLOAD_INFO("adv_lowpwr:%d->0\n", uodev->adv_lowpwr);
			uodev->adv_lowpwr = false;
		}
	}
}

static void deinit_fake_rsv_sram(void)
{
	if (uodev->adv_lowpwr)
		mtk_offload_deinit_rsv_sram();

	uodev->adv_lowpwr = uodev->enable_adv_lowpwr;
}

static void fake_sram_pwr_ctrl(bool power)
{
	if (uodev->adv_lowpwr)
		mtk_offload_rsv_sram_pwr_ctrl(power);
}

static void adsp_ee_recovery(void)
{
	u32 temp, irq_pending;
	u64 temp_64;

	if (!uodev->xhci)
		return;

	USB_OFFLOAD_INFO("ADSP EE ++ op:0x%08x, iman:0x%08X, erdp:0x%llX\n",
			readl(&uodev->xhci->op_regs->status),
			readl(&uodev->xhci->run_regs->ir_set[1].irq_pending),
			xhci_read_64(uodev->xhci, &uodev->xhci->ir_set[1].erst_dequeue));

	USB_OFFLOAD_INFO("// Disabling event ring interrupts\n");
	temp = readl(&uodev->xhci->op_regs->status);
	writel((temp & ~0x1fff) | STS_EINT, &uodev->xhci->op_regs->status);
	temp = readl(&uodev->xhci->ir_set[1].irq_pending);
	writel(ER_IRQ_DISABLE(temp), &uodev->xhci->ir_set[1].irq_pending);

	irq_pending = readl(&uodev->xhci->run_regs->ir_set[1].irq_pending);
	irq_pending |= IMAN_IP;
	writel(irq_pending, &uodev->xhci->run_regs->ir_set[1].irq_pending);

	temp_64 = xhci_read_64(uodev->xhci, &uodev->xhci->ir_set[1].erst_dequeue);
	/* Clear the event handler busy flag (RW1C) */
	temp_64 |= ERST_EHB;
	xhci_write_64(uodev->xhci, temp_64, &uodev->xhci->ir_set[1].erst_dequeue);

	uodev->adsp_exception = false;

	USB_OFFLOAD_INFO("ADSP EE -- op:0x%08x, iman:0x%08X, erdp:0x%llX\n",
			readl(&uodev->xhci->op_regs->status),
			readl(&uodev->xhci->run_regs->ir_set[1].irq_pending),
			xhci_read_64(uodev->xhci, &uodev->xhci->ir_set[1].erst_dequeue));
}

#ifdef CFG_RECOVERY_SUPPORT
static int usb_offload_event_receive(struct notifier_block *this, unsigned long event,
			    void *ptr)
{
	int ret = 0;

	switch (event) {
	case ADSP_EVENT_STOP:
		pr_info("%s event[%lu]\n", __func__, event);
		uodev->adsp_exception = true;
		uodev->adsp_ready = false;
		if (uodev->connected)
			adsp_ee_recovery();
		break;
	case ADSP_EVENT_READY: {
		pr_info("%s event[%lu]\n", __func__, event);
		uodev->adsp_ready = true;
		break;
	}
	default:
		pr_info("%s event[%lu]\n", __func__, event);
	}
	return ret;
}

static struct notifier_block adsp_usb_offload_notifier = {
	.notifier_call = usb_offload_event_receive,
	.priority = PRIMARY_FEATURE_PRI,
};
#endif
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
static int usb_offload_event_receive_scp(struct notifier_block *this,
					 unsigned long event,
					 void *ptr)
{

	switch (event) {
	case SCP_EVENT_STOP:
		pr_info("%s event[%lu]\n", __func__, event);
		uodev->adsp_exception = true;
		uodev->adsp_ready = false;
		if (uodev->connected)
			adsp_ee_recovery();
		break;
	case SCP_EVENT_READY:
		pr_info("%s event[%lu]\n", __func__, event);
		uodev->adsp_ready = true;
		break;
	default:
		pr_info("%s event[%lu]\n", __func__, event);
	}
	return 0;
}

static struct notifier_block scp_usb_offload_notifier = {
	.notifier_call = usb_offload_event_receive_scp,
};
#endif
static struct snd_usb_substream *find_snd_usb_substream(unsigned int card_num,
	unsigned int pcm_idx, unsigned int direction, struct snd_usb_audio
	**uchip)
{
	int idx;
	struct snd_usb_stream *as;
	struct snd_usb_substream *subs = NULL;
	struct snd_usb_audio *chip = NULL;

	mutex_lock(&register_mutex);
	/*
	 * legacy audio snd card number assignment is dynamic. Hence
	 * search using chip->card->number
	 */
	for (idx = 0; idx < SNDRV_CARDS; idx++) {
		if (!usb_chip[idx])
			continue;
		if (usb_chip[idx]->card->number == card_num) {
			chip = usb_chip[idx];
			break;
		}
	}

	if (!chip || atomic_read(&chip->shutdown)) {
		USB_OFFLOAD_ERR("instance of usb crad # %d does not exist\n", card_num);
		goto err;
	}

	if (pcm_idx >= chip->pcm_devs) {
		USB_OFFLOAD_ERR("invalid pcm dev number %u > %d\n", pcm_idx, chip->pcm_devs);
		goto err;
	}

	if (direction > SNDRV_PCM_STREAM_CAPTURE) {
		USB_OFFLOAD_ERR("invalid direction %u\n", direction);
		goto err;
	}

	list_for_each_entry(as, &chip->pcm_list, list) {
		if (as->pcm_index == pcm_idx) {
			subs = &as->substream[direction];
			if (!subs->data_endpoint && !subs->sync_endpoint) {
				USB_OFFLOAD_ERR("stream disconnected, bail out\n");
				subs = NULL;
				goto err;
			}
			goto done;
		}
	}

done:
	USB_OFFLOAD_MEM_DBG("done\n");
err:
	*uchip = chip;
	if (!subs)
		USB_OFFLOAD_ERR("substream instance not found\n");
	mutex_unlock(&register_mutex);
	return subs;
}

static void sound_usb_connect(void *data, struct usb_interface *intf, struct snd_usb_audio *chip)
{
	struct device_node *node_xhci_host;
	struct platform_device *pdev_xhci_host = NULL;
	struct xhci_hcd_mtk *mtk;
	struct xhci_hcd *xhci;

	USB_OFFLOAD_INFO("index=%d\n", chip->index);

	if (chip->index >= 0)
		usb_chip[chip->index] = chip;

	uodev->is_streaming = false;
	uodev->tx_streaming = false;
	uodev->rx_streaming = false;
	uodev->adsp_inited = false;
	uodev->connected = true;
	uodev->opened = false;
	uodev->adsp_exception = false;

	node_xhci_host = of_parse_phandle(uodev->dev->of_node, "xhci-host", 0);
	if (node_xhci_host) {
		pdev_xhci_host = of_find_device_by_node(node_xhci_host);
		if (!pdev_xhci_host) {
			USB_OFFLOAD_ERR("no device found by node!\n");
			return;
		}
		of_node_put(node_xhci_host);

		mtk = platform_get_drvdata(pdev_xhci_host);
		if (!mtk) {
			USB_OFFLOAD_ERR("no drvdata set!\n");
			return;
		}
		xhci = hcd_to_xhci(mtk->hcd);
		uodev->xhci = xhci;
	} else {
		USB_OFFLOAD_ERR("No 'xhci_host' node, NOT SUPPORT USB Offload!\n");
		uodev->xhci = NULL;
		return;
	}
}

static void sound_usb_disconnect(void *data, struct usb_interface *intf)
{
	struct snd_usb_audio *chip = usb_get_intfdata(intf);
	unsigned int card_num;

	USB_OFFLOAD_INFO("\n");

	uodev->is_streaming = false;
	uodev->tx_streaming = false;
	uodev->rx_streaming = false;
	uodev->adsp_inited = false;
	uodev->connected = false;
	uodev->opened = false;
	uodev->adsp_exception = false;

	if (chip == USB_AUDIO_IFACE_UNUSED)
		return;

	card_num = chip->card->number;

	USB_OFFLOAD_INFO("index=%d num_interfaces=%d, card_num=%d\n",
			chip->index, chip->num_interfaces, card_num);

	uaudio_disconnect_cb(chip);

	if (chip->num_interfaces < 1)
		if (chip->index >= 0)
			usb_chip[chip->index] = NULL;
}

static int sound_usb_trace_init(void)
{
	int ret = 0;

	WARN_ON(register_trace_android_vh_audio_usb_offload_connect(sound_usb_connect, NULL));
	WARN_ON(register_trace_android_rvh_audio_usb_offload_disconnect(
		sound_usb_disconnect, NULL));

	return ret;
}

static bool is_support_format(snd_pcm_format_t fmt)
{
	switch (fmt) {
	case SNDRV_PCM_FORMAT_S8:
	case SNDRV_PCM_FORMAT_U8:
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_S16_BE:
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_U16_BE:
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_BE:
	case SNDRV_PCM_FORMAT_U24_LE:
	case SNDRV_PCM_FORMAT_U24_BE:
	case SNDRV_PCM_FORMAT_S24_3LE:
	case SNDRV_PCM_FORMAT_S24_3BE:
	case SNDRV_PCM_FORMAT_U24_3LE:
	case SNDRV_PCM_FORMAT_U24_3BE:
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_S32_BE:
	case SNDRV_PCM_FORMAT_U32_LE:
	case SNDRV_PCM_FORMAT_U32_BE:
		return true;
	default:
		return false;
	}
}

static enum usb_audio_device_speed
get_speed_info(enum usb_device_speed udev_speed)
{
	switch (udev_speed) {
	case USB_SPEED_LOW:
		return USB_AUDIO_DEVICE_SPEED_LOW;
	case USB_SPEED_FULL:
		return USB_AUDIO_DEVICE_SPEED_FULL;
	case USB_SPEED_HIGH:
		return USB_AUDIO_DEVICE_SPEED_HIGH;
	case USB_SPEED_SUPER:
		return USB_AUDIO_DEVICE_SPEED_SUPER;
	case USB_SPEED_SUPER_PLUS:
		return USB_AUDIO_DEVICE_SPEED_SUPER_PLUS;
	default:
		USB_OFFLOAD_INFO("udev speed %d\n", udev_speed);
		return USB_AUDIO_DEVICE_SPEED_INVALID;
	}
}

static bool is_pcm_size_valid(struct usb_audio_stream_info *uainfo)
{
	struct snd_usb_audio *chip = NULL;
	struct snd_usb_substream *subs = NULL;
	struct snd_usb_endpoint *data_ep = NULL;
	int pkt_sz, ringbuf_sz, packs_per_ms;

	subs = find_snd_usb_substream(
		uainfo->pcm_card_num,
		uainfo->pcm_dev_num,
		uainfo->direction,
		&chip);

	if (!subs || !chip || atomic_read(&chip->shutdown)) {
		USB_OFFLOAD_ERR("can't find substream for card# %u, dev# %u, dir: %u\n",
			uainfo->pcm_card_num, uainfo->pcm_dev_num, uainfo->direction);
		return false;
	}

	data_ep = subs->data_endpoint;
	if (!data_ep) {
		USB_OFFLOAD_ERR("data_endpoint can't be NULL(card# %u, dev# %u, dir: %u)\n",
			uainfo->pcm_card_num, uainfo->pcm_dev_num, uainfo->direction);
		return false;
	}

	if (get_speed_info(subs->dev->speed) != USB_AUDIO_DEVICE_SPEED_FULL)
		packs_per_ms = 8 >> data_ep->datainterval;
	else
		packs_per_ms = 1;

	/* multiple by 10 to avoid 1 digit after decimal dot */
	pkt_sz = 10 * (((uainfo->bit_depth * uainfo->number_of_ch) >> 3) * uainfo->bit_rate) / data_ep->pps;
	ringbuf_sz = pkt_sz * packs_per_ms * uainfo->dram_size * uainfo->dram_cnt;

	USB_OFFLOAD_DBG("10*pkt_sz:%d 10*ringbuf_sz:%d 10*pcm_sz:%d\n",
		pkt_sz, ringbuf_sz, 10* uainfo->pcm_size);

	/* cause we multiple pkt_sz by 10 before, pcm_size should be also multipled */
	if (ringbuf_sz < uainfo->pcm_size * 10) {
		USB_OFFLOAD_ERR("Invalid uainfo(10*pcm_sz:%d > 10*ringbuf_sz:%d) 10*pkt_sz:%d\n",
			uainfo->pcm_size, ringbuf_sz, pkt_sz);
		USB_OFFLOAD_ERR("pkt_per_ms:%d interval:%d frame_bit:%d rate:%d pps:%d\n",
			packs_per_ms, data_ep->datainterval, uainfo->bit_depth * uainfo->number_of_ch,
			uainfo->bit_rate, data_ep->pps);
		return false;
	}

	return true;
}

static bool is_uainfo_valid(struct usb_audio_stream_info *uainfo)
{
	if (uainfo == NULL) {
		USB_OFFLOAD_ERR("uainfo is NULL\n");
		return false;
	}

	if (uainfo->enable > 1) {
		USB_OFFLOAD_ERR("uainfo->enable invalid (%d)\n", uainfo->enable);
		return false;
	}

	if (uainfo->bit_rate > 768000) {
		USB_OFFLOAD_ERR("uainfo->bit_rate invalid (%d)\n", uainfo->bit_rate);
		return false;
	}

	if (uainfo->bit_depth > 32) {
		USB_OFFLOAD_ERR("uainfo->bit_depth invalid (%d)\n", uainfo->bit_depth);
		return false;
	}

	if (uainfo->number_of_ch > 2) {
		USB_OFFLOAD_ERR("uainfo->number_of_ch invalid (%d)\n", uainfo->number_of_ch);
		return false;
	}

	if (uainfo->direction > 1) {
		USB_OFFLOAD_ERR("uainfo->direction invalid (%d)\n", uainfo->direction);
		return false;
	}

	return is_pcm_size_valid(uainfo);
}

static void dump_uainfo(struct usb_audio_stream_info *uainfo)
{
	USB_OFFLOAD_INFO(
		"enable:%d rate:%d ch:%d depth:%d dir:%d period:%d card:%d pcm:%d drm_sz:%d drm_cnt:%d\n",
		uainfo->enable,
		uainfo->bit_rate,
		uainfo->number_of_ch,
		uainfo->bit_depth,
		uainfo->direction,
		uainfo->xhc_irq_period_ms,
		uainfo->pcm_card_num,
		uainfo->pcm_dev_num,
		uainfo->dram_size,
		uainfo->dram_cnt);
}

static void usb_audio_dev_intf_cleanup(struct usb_device *udev,
		struct intf_info *info)
{
	info->in_use = false;
}

static void uaudio_dev_cleanup(struct usb_audio_dev *dev)
{
	int if_idx;

	if (!dev) {
		USB_OFFLOAD_ERR("USB audio device is already freed.\n");
		return;
	}

	if (!dev->udev) {
		USB_OFFLOAD_ERR("USB device is already freed.\n");
		return;
	}

	/* free xfer buffer and unmap xfer ring and buf per interface */
	for (if_idx = 0; if_idx < dev->num_intf; if_idx++) {
		if (!dev->info[if_idx].in_use)
			continue;
		usb_audio_dev_intf_cleanup(dev->udev, &dev->info[if_idx]);
		USB_OFFLOAD_INFO("release resources: if_idx:%d intf# %d card# %d\n",
				if_idx, dev->info[if_idx].intf_num, dev->card_num);
	}

	dev->num_intf = 0;

	/* free interface info */
	kfree(dev->info);
	dev->info = NULL;
	dev->udev = NULL;
}

int send_disconnect_ipi_msg_to_adsp(void)
{
	int send_result = 0;
	struct ipi_msg_t ipi_msg;
	uint8_t scene = 0;

	USB_OFFLOAD_INFO("\n");

	// Send DISCONNECT msg to ADSP Via IPI
	for (scene = TASK_SCENE_USB_DL; scene <= TASK_SCENE_USB_UL; scene++) {
		send_result = audio_send_ipi_msg(
						 &ipi_msg, scene,
						 AUDIO_IPI_LAYER_TO_DSP,
						 AUDIO_IPI_MSG_ONLY,
						 AUDIO_IPI_MSG_NEED_ACK,
						 AUD_USB_MSG_A2D_DISCONNECT,
						 0,
						 0,
						 NULL);
		if (send_result == 0) {
			send_result = ipi_msg.param2;
			if (send_result)
				break;
		}
	}

	if (send_result != 0)
		USB_OFFLOAD_ERR("USB Offload disconnect IPI msg send fail\n");
	else
		USB_OFFLOAD_INFO("USB Offload disconnect IPI msg send succeed\n");

	return send_result;
}

static void uaudio_disconnect_cb(struct snd_usb_audio *chip)
{
	int ret;
	struct usb_audio_dev *dev;
	int card_num = chip->card->number;
	struct usb_audio_stream_msg msg = {0};

	USB_OFFLOAD_INFO("for card# %d\n", card_num);

	if (card_num >=  SNDRV_CARDS) {
		USB_OFFLOAD_ERR("invalid card number\n");
		return;
	}

	mutex_lock(&uodev->dev_lock);
	dev = &uadev[card_num];

	/* clean up */
	if (!dev->udev) {
		USB_OFFLOAD_INFO("no clean up required\n");
		goto done;
	}

	if (atomic_read(&dev->in_use)) {
		mutex_unlock(&uodev->dev_lock);

		msg.status = USB_AUDIO_STREAM_REQ_STOP;
		msg.status_valid = 1;

		/* write to audio ipi*/
		ret = send_disconnect_ipi_msg_to_adsp();
		/* wait response */
		USB_OFFLOAD_INFO("send_disconnect_ipi_msg_to_adsp msg, ret: %d\n", ret);

		memory_cleanup();
		atomic_set(&dev->in_use, 0);
		mutex_lock(&uodev->dev_lock);
	}

	uaudio_dev_cleanup(dev);
done:
	mutex_unlock(&uodev->dev_lock);

	USB_OFFLOAD_INFO("done\n");
}

static void uaudio_dev_release(struct kref *kref)
{
	struct usb_audio_dev *dev = container_of(kref, struct usb_audio_dev, kref);

	if (!dev) {
		USB_OFFLOAD_ERR("dev has been freed!!\n");
		return;
	}
	USB_OFFLOAD_INFO("in_use:%d -> 0\n",
			atomic_read(&uadev[uodev->card_num].in_use));

	atomic_set(&dev->in_use, 0);
	wake_up(&dev->disconnect_wq);
}

static int info_idx_from_ifnum(unsigned int card_num, int intf_num, bool enable)
{
	int i;

	USB_OFFLOAD_DBG("enable:%d, card_num:%d, intf_num:%d\n",
			enable, card_num, intf_num);

	/*
	 * default index 0 is used when info is allocated upon
	 * first enable audio stream req for a pcm device
	 */
	if (enable && !uadev[card_num].info) {
		USB_OFFLOAD_MEM_DBG("enable:%d, uadev[%d].info:%p\n",
				enable, card_num, uadev[card_num].info);
		return 0;
	}

	USB_OFFLOAD_DBG("num_intf:%d\n", uadev[card_num].num_intf);

	for (i = 0; i < uadev[card_num].num_intf; i++) {
		USB_OFFLOAD_DBG("info_idx:%d, in_use:%d, intf_num:%d\n",
			i,
			uadev[card_num].info[i].in_use,
			uadev[card_num].info[i].intf_num);
		if (enable && !uadev[card_num].info[i].in_use)
			return i;
		else if (!enable &&
				uadev[card_num].info[i].intf_num == intf_num)
			return i;
	}

	return -EINVAL;
}

static int get_data_interval_from_si(struct snd_usb_substream *subs,
		u32 service_interval)
{
	unsigned int bus_intval, bus_intval_mult, binterval;

	if (subs->dev->speed >= USB_SPEED_HIGH)
		bus_intval = BUS_INTERVAL_HIGHSPEED_AND_ABOVE;
	else
		bus_intval = BUS_INTERVAL_FULL_SPEED;

	if (service_interval % bus_intval)
		return -EINVAL;

	bus_intval_mult = service_interval / bus_intval;
	binterval = ffs(bus_intval_mult);
	if (!binterval || binterval > MAX_BINTERVAL_ISOC_EP)
		return -EINVAL;

	/* check if another bit is set then bail out */
	bus_intval_mult = bus_intval_mult >> binterval;
	if (bus_intval_mult)
		return -EINVAL;

	return (binterval - 1);
}

/* looks up alias, if any, for controller DT node and returns the index */
static int usb_get_controller_id(struct usb_device *udev)
{
	if (udev->bus->sysdev && udev->bus->sysdev->of_node)
		return of_alias_get_id(udev->bus->sysdev->of_node, "usb");

	return -ENODEV;
}

static void *find_csint_desc(unsigned char *descstart, int desclen, u8 dsubtype)
{
	u8 *p, *end, *next;

	p = descstart;
	end = p + desclen;
	while (p < end) {
		if (p[0] < 2)
			return NULL;
		next = p + p[0];
		if (next > end)
			return NULL;
		if (p[1] == USB_DT_CS_INTERFACE && p[2] == dsubtype)
			return p;
		p = next;
	}
	return NULL;
}

static int usb_offload_prepare_msg(struct snd_usb_substream *subs,
		struct usb_audio_stream_info *uainfo,
		struct usb_audio_stream_msg *msg,
		int info_idx)
{
	struct usb_interface *iface;
	struct usb_host_interface *alts;
	struct usb_interface_descriptor *altsd;
	struct usb_interface_assoc_descriptor *assoc;
	struct usb_host_endpoint *ep;
	struct uac_format_type_i_continuous_descriptor *fmt;
	struct uac_format_type_i_discrete_descriptor *fmt_v1;
	struct uac_format_type_i_ext_descriptor *fmt_v2;
	struct uac1_as_header_descriptor *as;
	int ret;
	unsigned int protocol, card_num, pcm_dev_num;
	int interface, altset_idx;
	void *hdr_ptr;
	unsigned int data_ep_pipe = 0, sync_ep_pipe = 0;

	if (subs == NULL) {
		USB_OFFLOAD_ERR("substream is NULL!\n");
		ret = -ENODEV;
		goto err;
	}

	if (subs->cur_audiofmt == NULL) {
		USB_OFFLOAD_ERR("substream->cur_audio_fmt is NULL!\n");
		ret = -ENODEV;
		goto err;
	}
	interface = subs->cur_audiofmt->iface;
	altset_idx = subs->cur_audiofmt->altset_idx;

	iface = usb_ifnum_to_if(subs->dev, interface);
	if (!iface) {
		USB_OFFLOAD_ERR("interface # %d does not exist\n", interface);
		ret = -ENODEV;
		goto err;
	}
	msg->uainfo = *uainfo;

	assoc = iface->intf_assoc;
	pcm_dev_num = uainfo->pcm_dev_num;
	card_num = uainfo->pcm_card_num;

	msg->direction = uainfo->direction;
	msg->pcm_dev_num = uainfo->pcm_dev_num;
	msg->pcm_card_num = uainfo->pcm_card_num;

	alts = &iface->altsetting[altset_idx];
	altsd = get_iface_desc(alts);
	protocol = altsd->bInterfaceProtocol;

	/* get format type */
	if (protocol != UAC_VERSION_3) {
		fmt = find_csint_desc(alts->extra, alts->extralen,
				UAC_FORMAT_TYPE);
		if (!fmt) {
			USB_OFFLOAD_ERR("%u:%d : no UAC_FORMAT_TYPE desc\n",
					interface, altset_idx);
			ret = -ENODEV;
			goto err;
		}
	}

	if (!uadev[card_num].ctrl_intf) {
		USB_OFFLOAD_ERR("audio ctrl intf info not cached\n");
		ret = -ENODEV;
		goto err;
	}

	if (protocol != UAC_VERSION_3) {
		hdr_ptr = find_csint_desc(uadev[card_num].ctrl_intf->extra,
				uadev[card_num].ctrl_intf->extralen,
				UAC_HEADER);
		if (!hdr_ptr) {
			USB_OFFLOAD_ERR("no UAC_HEADER desc\n");
			ret = -ENODEV;
			goto err;
		}
	}

	if (protocol == UAC_VERSION_1) {
		struct uac1_ac_header_descriptor *uac1_hdr = hdr_ptr;

		as = find_csint_desc(alts->extra, alts->extralen,
			UAC_AS_GENERAL);
		if (!as) {
			USB_OFFLOAD_ERR("%u:%d : no UAC_AS_GENERAL desc\n",
					interface, altset_idx);
			ret = -ENODEV;
			goto err;
		}
		msg->data_path_delay = as->bDelay;
		msg->data_path_delay_valid = 1;
		fmt_v1 = (struct uac_format_type_i_discrete_descriptor *)fmt;
		msg->usb_audio_subslot_size = fmt_v1->bSubframeSize;
		msg->usb_audio_subslot_size_valid = 1;

		msg->usb_audio_spec_revision = le16_to_cpu(uac1_hdr->bcdADC);
		msg->usb_audio_spec_revision_valid = 1;
	} else if (protocol == UAC_VERSION_2) {
		struct uac2_ac_header_descriptor *uac2_hdr = hdr_ptr;

		fmt_v2 = (struct uac_format_type_i_ext_descriptor *)fmt;
		msg->usb_audio_subslot_size = fmt_v2->bSubslotSize;
		msg->usb_audio_subslot_size_valid = 1;

		msg->usb_audio_spec_revision = le16_to_cpu(uac2_hdr->bcdADC);
		msg->usb_audio_spec_revision_valid = 1;
	} else if (protocol == UAC_VERSION_3) {
		if (assoc->bFunctionSubClass ==
					UAC3_FUNCTION_SUBCLASS_FULL_ADC_3_0) {
			USB_OFFLOAD_ERR("full adc is not supported\n");
			ret = -EINVAL;
		}

		switch (le16_to_cpu(get_endpoint(alts, 0)->wMaxPacketSize)) {
		case UAC3_BADD_EP_MAXPSIZE_SYNC_MONO_16:
		case UAC3_BADD_EP_MAXPSIZE_SYNC_STEREO_16:
		case UAC3_BADD_EP_MAXPSIZE_ASYNC_MONO_16:
		case UAC3_BADD_EP_MAXPSIZE_ASYNC_STEREO_16: {
			msg->usb_audio_subslot_size = 0x2;
			break;
		}

		case UAC3_BADD_EP_MAXPSIZE_SYNC_MONO_24:
		case UAC3_BADD_EP_MAXPSIZE_SYNC_STEREO_24:
		case UAC3_BADD_EP_MAXPSIZE_ASYNC_MONO_24:
		case UAC3_BADD_EP_MAXPSIZE_ASYNC_STEREO_24: {
			msg->usb_audio_subslot_size = 0x3;
			break;
		}

		default:
			USB_OFFLOAD_ERR("%d: %u: Invalid wMaxPacketSize\n",
					interface, altset_idx);
			ret = -EINVAL;
			goto err;
		}
		msg->usb_audio_subslot_size_valid = 1;
	} else {
		USB_OFFLOAD_ERR("unknown protocol version %x\n", protocol);
		ret = -ENODEV;
		goto err;
	}

	msg->slot_id = subs->dev->slot_id;
	msg->slot_id_valid = 1;

	memcpy(&msg->std_as_opr_intf_desc, &alts->desc, sizeof(alts->desc));
	msg->std_as_opr_intf_desc_valid = 1;

	ep = usb_pipe_endpoint(subs->dev, subs->data_endpoint->pipe);
	if (!ep) {
		USB_OFFLOAD_ERR("data ep # %d context is null\n",
				subs->data_endpoint->ep_num);
		ret = -ENODEV;
		goto err;
	}
	data_ep_pipe = subs->data_endpoint->pipe;
	memcpy(&msg->std_as_data_ep_desc, &ep->desc, sizeof(ep->desc));
	msg->std_as_data_ep_desc_valid = 1;

	if (subs->sync_endpoint) {
		ep = usb_pipe_endpoint(subs->dev, subs->sync_endpoint->pipe);
		if (!ep) {
			USB_OFFLOAD_ERR("implicit fb on data ep\n");
			goto skip_sync_ep;
		}
		sync_ep_pipe = subs->sync_endpoint->pipe;
		memcpy(&msg->std_as_sync_ep_desc, &ep->desc, sizeof(ep->desc));
		msg->std_as_sync_ep_desc_valid = 1;
	}

skip_sync_ep:
	msg->interrupter_num = uodev->intr_num;
	msg->interrupter_num_valid = 1;
	msg->controller_num_valid = 0;
	ret = usb_get_controller_id(subs->dev);
	if (ret >= 0) {
		msg->controller_num = ret;
		msg->controller_num_valid = 1;
	}

	msg->speed_info = get_speed_info(subs->dev->speed);
	if (msg->speed_info == USB_AUDIO_DEVICE_SPEED_INVALID) {
		ret = -ENODEV;
		goto err;
	}

	msg->speed_info_valid = 1;

	if (!atomic_read(&uadev[card_num].in_use)) {
		kref_init(&uadev[card_num].kref);
		init_waitqueue_head(&uadev[card_num].disconnect_wq);
		uadev[card_num].num_intf =
				subs->dev->config->desc.bNumInterfaces;
		uadev[card_num].info = kcalloc(uadev[card_num].num_intf,
				sizeof(struct intf_info), GFP_KERNEL);
		if (!uadev[card_num].info) {
			ret = -ENOMEM;
			goto err;
		}
		uadev[card_num].udev = subs->dev;
		atomic_set(&uadev[card_num].in_use, 1);
	} else {
		kref_get(&uadev[card_num].kref);
	}

	uadev[card_num].card_num = card_num;
	uadev[card_num].usb_core_id = msg->controller_num;

	uadev[card_num].info[info_idx].data_ep_pipe = data_ep_pipe;
	uadev[card_num].info[info_idx].sync_ep_pipe = sync_ep_pipe;
	uadev[card_num].info[info_idx].pcm_card_num = card_num;
	uadev[card_num].info[info_idx].pcm_dev_num = pcm_dev_num;
	uadev[card_num].info[info_idx].direction = subs->direction;
	uadev[card_num].info[info_idx].intf_num = interface;
	uadev[card_num].info[info_idx].in_use = true;

	set_bit(card_num, &uodev->card_slot);
	uodev->card_num = card_num;
	return 0;
err:
	return ret;
}

int send_init_ipi_msg_to_adsp(struct mem_info_xhci *mpu_info)
{
	int send_result = 0;
	struct ipi_msg_t ipi_msg;
	uint8_t scene = 0;

	USB_OFFLOAD_INFO("[reserved dram] addr:0x%x size:%d\n",
			mpu_info->xhci_dram_addr, mpu_info->xhci_dram_size);
	USB_OFFLOAD_INFO("[reserved sram] addr:0x%x size:%d\n",
			mpu_info->xhci_sram_addr, mpu_info->xhci_sram_size);

	// Send struct usb_audio_stream_info Address to Hifi3 Via IPI
	for (scene = TASK_SCENE_USB_DL; scene <= TASK_SCENE_USB_UL; scene++) {
		send_result = audio_send_ipi_msg(
						 &ipi_msg, scene,
						 AUDIO_IPI_LAYER_TO_DSP,
						 AUDIO_IPI_PAYLOAD,
						 AUDIO_IPI_MSG_NEED_ACK,
						 AUD_USB_MSG_A2D_INIT_ADSP,
						 sizeof(struct mem_info_xhci),
						 0,
						 mpu_info);
		if (send_result == 0) {
			send_result = ipi_msg.param2;
			if (send_result)
				break;
		}
	}

	if (send_result != 0)
		USB_OFFLOAD_ERR("USB Offload init IPI msg send fail\n");
	else
		USB_OFFLOAD_INFO("USB Offload init IPI msg send succeed\n");

	return send_result;
}

int send_uas_ipi_msg_to_adsp(struct usb_audio_stream_msg *uas_msg)
{
	int send_result = 0;
	struct ipi_msg_t ipi_msg;
	uint8_t task_scene = 0;

	USB_OFFLOAD_DBG("msg: %p, size: %lu\n",	uas_msg, sizeof(*uas_msg));

	if (uas_msg->uainfo.direction == 0)
		task_scene = TASK_SCENE_USB_DL;
	else
		task_scene = TASK_SCENE_USB_UL;

	// Send struct usb_audio_stream_info Address to ADSP Via IPI
	send_result = audio_send_ipi_msg(
					 &ipi_msg, task_scene,
					 AUDIO_IPI_LAYER_TO_DSP,
					 AUDIO_IPI_DMA,
					 AUDIO_IPI_MSG_NEED_ACK,
					 AUD_USB_MSG_A2D_ENABLE_STREAM,
					 sizeof(struct usb_audio_stream_msg),
					 0,
					 uas_msg);
	if (send_result == 0)
		send_result = ipi_msg.param2;

	if (send_result != 0)
		USB_OFFLOAD_ERR("USB Offload uas IPI msg send fail\n");
	else
		USB_OFFLOAD_INFO("USB Offload uas ipi msg send succeed\n");

	return send_result;
}

static int mtk_usb_offload_free_allocated(bool is_in)
{
	unsigned int buf_idx = is_in ? 1 : 0;
	struct usb_offload_buffer *buf;

	USB_OFFLOAD_MEM_DBG("(%s)\n", is_in ? "IN" : "OUT");
	buf = &buf_allocated[buf_idx];
	if (!buf) {
		USB_OFFLOAD_INFO("buf(%s) has already freed\n",
			is_in ? "in" : "out");
		return 0;
	}

	if (!buf->allocated) {
		USB_OFFLOAD_INFO("buf(%s) has already freed\n",
			is_in ? "in" : "out");
		return 0;
	}
	if (mtk_offload_free_mem(buf))
		USB_OFFLOAD_ERR("Fail to free urb\n");
	dec_region(URB);
	return 0;
}

struct urb_information {
	unsigned int align_size;
	unsigned int urb_size;
	unsigned int urb_num;
	unsigned int urb_packs;
};

static unsigned int get_usb_speed_rate(bool is_full_speed, unsigned int rate)
{
	if (is_full_speed)
		return ((rate << 13) + 62) / 125;
	else
		return ((rate << 10) + 62) / 125;
}

static struct urb_information mtk_usb_offload_calculate_urb(
	struct usb_audio_stream_info *uainfo, struct snd_usb_substream *subs)
{
	struct urb_information urb_info;
	struct snd_usb_endpoint *ep = subs->data_endpoint;
	struct snd_usb_audio *chip = ep->chip;
	unsigned int freqn, freqmax;
	unsigned int maxsize, packs_per_ms, max_packs_per_urb, urb_packs, nurbs;
	unsigned int buffer_size, align = 64 - 1;
	int frame_bits, packets;

	frame_bits = uainfo->bit_depth * uainfo->number_of_ch;
	freqn = get_usb_speed_rate(
		get_speed_info(subs->dev->speed) == USB_AUDIO_DEVICE_SPEED_FULL,
		uainfo->bit_rate);
	freqmax = freqn + (freqn >> 1);
	maxsize = (((freqmax << ep->datainterval) + 0xffff) >> 16) * (frame_bits >> 3);

	if (ep->maxpacksize && ep->maxpacksize < maxsize) {
		unsigned int pre_maxsize = maxsize, pre_freqmax = freqmax;
		unsigned int data_maxsize = maxsize = ep->maxpacksize;

		freqmax = (data_maxsize / (frame_bits >> 3)) << (16 - ep->datainterval);
		USB_OFFLOAD_INFO("maxsize:%d->%d freqmax:%d->%d\n",
			pre_maxsize, maxsize, pre_freqmax, freqmax);
	}

	if (chip->dev->speed != USB_SPEED_FULL) {
		packs_per_ms = 8 >> ep->datainterval;
		max_packs_per_urb = MAX_PACKS_HS;
	} else {
		packs_per_ms = 1;
		max_packs_per_urb = MAX_PACKS;
	}
	max_packs_per_urb = max(1u, max_packs_per_urb >> ep->datainterval);

	if (usb_pipein(ep->pipe)) {
		urb_packs = packs_per_ms;
		urb_packs = min(max_packs_per_urb, urb_packs);

		while (urb_packs > 1 && urb_packs * maxsize >= uainfo->pcm_size)
			urb_packs >>= 1;

		if (uainfo->xhc_irq_period_ms * uainfo->xhc_urb_num * packs_per_ms
			> USB_OFFLOAD_TRBS_PER_SEGMENT) {
			urb_packs = uainfo->xhc_irq_period_ms * packs_per_ms;
			nurbs = USB_OFFLOAD_TRBS_PER_SEGMENT / urb_packs;
		} else {
			urb_packs = uainfo->xhc_irq_period_ms * packs_per_ms;
			nurbs = uainfo->xhc_urb_num;
		}
	} else {
		nurbs = uainfo->xhc_urb_num;
		urb_packs = uainfo->xhc_irq_period_ms * packs_per_ms;
	}

	packets = urb_packs;
	buffer_size = maxsize * packets;

	USB_OFFLOAD_INFO(
		"maxsz:%d frame_bits:%d freqmax:%d freqn:%d intval:%d urb_pkt:%d bufsz:%d maxpktsz:%d",
		maxsize, frame_bits, freqmax, freqn, ep->datainterval,
		packets, buffer_size, ep->maxpacksize);

	urb_info.urb_size = buffer_size;
	urb_info.urb_num = nurbs;
	urb_info.urb_packs = packets;
	urb_info.align_size = buffer_size + align;

	return urb_info;
}

/* allocate urb and 2nd segment in one time */
static int usb_offload_prepare_msg_ext(struct usb_audio_stream_msg *msg,
	struct usb_audio_stream_info *uainfo, struct snd_usb_substream *subs)
{
	struct urb_information urb_info;
	struct usb_host_endpoint *ep;
	struct xhci_ring *ring;
	unsigned int total_size, align = 64 - 1;
	unsigned int slot_id, ep_id;
	struct usb_offload_buffer *buf = &buf_allocated[uainfo->direction];
	dma_addr_t phy_addr;
	void *vir_addr;
	int ret, i;
	bool expend_tr;
	enum usb_offload_mem_id mem_type;

	/* calculate urb */
	urb_info = mtk_usb_offload_calculate_urb(uainfo, subs);
	USB_OFFLOAD_INFO("[urb_info] align_size:%d size:%d nurbs:%d packs:%d\n",
		urb_info.align_size, urb_info.urb_size, urb_info.urb_num, urb_info.urb_packs);
	total_size = urb_info.align_size * urb_info.urb_num;

	/* check if it needs 2nd segment */
	expend_tr = get_speed_info(subs->dev->speed) > USB_AUDIO_DEVICE_SPEED_FULL &&
		uainfo->xhc_irq_period_ms == 20 ? true : false;
	if (expend_tr)
		total_size += USB_OFFLOAD_TRB_SEGMENT_SIZE + align;

	USB_OFFLOAD_INFO("total_size:%d direction:%d\n", total_size, uainfo->direction);

	if (uodev->adv_lowpwr_dl_only && uainfo->direction == SNDRV_PCM_STREAM_CAPTURE)
		mem_type = USB_OFFLOAD_MEM_DRAM_ID;
	else
		mem_type = lowpwr_mem_type();

	/* requeset for memory (urbs + 2nd segment)*/
	ret = mtk_offload_alloc_mem(buf, total_size, USB_OFFLOAD_TRB_SEGMENT_SIZE,
				mem_type, false);
	if (ret != 0)
		return ret;
	inc_region(URB);

	/* assign memory for 2nd segment */
	phy_addr = buf->dma_addr;
	if (expend_tr) {
		slot_id = subs->dev->slot_id;
		ep = usb_pipe_endpoint(subs->dev, subs->data_endpoint->pipe);
		if (ep) {
#if IS_ENABLED(CONFIG_DEVICE_MODULES_USB_XHCI_MTK)
			ep_id = xhci_get_endpoint_index_(&ep->desc);
#else
			ep_id = xhci_get_endpoint_index(&ep->desc);
#endif
			ring = uodev->xhci->devs[slot_id]->eps[ep_id].ring;
			phy_addr = (phy_addr + align) & (~align);
			vir_addr = (void *)((u64)buf->dma_area + (u64)(phy_addr - buf->dma_addr));
			xhci_mtk_ring_expansion(uodev->xhci, ring, phy_addr, vir_addr);

			phy_addr = buf->dma_addr + USB_OFFLOAD_TRB_SEGMENT_SIZE;
		}
	}

	/* assign memory for urbs */
	phy_addr = (phy_addr + align) & (~align);
	msg->urb_start_addr = (unsigned long long)phy_addr;
	msg->urb_size = urb_info.urb_size;
	msg->urb_num = urb_info.urb_num;
	msg->urb_packs = urb_info.urb_packs;

	for (i = 0; i < msg->urb_num; i++) {
		USB_OFFLOAD_INFO("[urb(%s)%d] phys:0x%llx size:%d\n",
			uainfo->direction == SNDRV_PCM_STREAM_CAPTURE ? "in" : "out",
			i, phy_addr, msg->urb_size);

		phy_addr += msg->urb_size;
		phy_addr = (phy_addr + align) & (~align);
	}

	return 0;
}

static int usb_offload_enable_stream(struct usb_audio_stream_info *uainfo)
{
	struct usb_audio_stream_msg msg = {0};
	struct snd_usb_substream *subs;
	struct snd_pcm_substream *substream;
	struct snd_usb_audio *chip = NULL;
	struct intf_info *info;
	struct usb_host_endpoint *ep;
	u8 pcm_card_num, pcm_dev_num, direction;
	int info_idx = -EINVAL, datainterval = -EINVAL, ret = 0;
	int interface;

	direction = uainfo->direction;
	pcm_dev_num = uainfo->pcm_dev_num;
	pcm_card_num = uainfo->pcm_card_num;

	if (pcm_card_num >= SNDRV_CARDS) {
		USB_OFFLOAD_ERR("invalid card # %u", pcm_card_num);
		ret = -EINVAL;
		goto done;
	}

	if (!is_support_format(uainfo->audio_format)) {
		USB_OFFLOAD_ERR("unsupported pcm format received %d\n",
				uainfo->audio_format);
		ret = -EINVAL;
		goto done;
	}

	subs = find_snd_usb_substream(pcm_card_num, pcm_dev_num, direction,
					&chip);
	if (!subs || !chip || atomic_read(&chip->shutdown)) {
		USB_OFFLOAD_ERR("can't find substream for card# %u, dev# %u, dir: %u\n",
				pcm_card_num, pcm_dev_num, direction);
		ret = -ENODEV;
		goto done;
	}

	mutex_lock(&uodev->dev_lock);

	if (subs->cur_audiofmt)
		interface = subs->cur_audiofmt->iface;
	else
		interface = -1;

	info_idx = info_idx_from_ifnum(pcm_card_num, interface,
		uainfo->enable);
	USB_OFFLOAD_INFO("info_idx: %d, interface: %d\n",
			info_idx, interface);

	if (uainfo->enable) {
		if (info_idx < 0) {
			USB_OFFLOAD_ERR("interface# %d already in use card# %d\n",
					interface, pcm_card_num);
			ret = -EBUSY;
			mutex_unlock(&uodev->dev_lock);
			goto done;
		}
	}

	if (atomic_read(&chip->shutdown) || !subs->stream || !subs->stream->pcm
			|| !subs->stream->chip || !subs->pcm_substream || info_idx < 0) {
		USB_OFFLOAD_INFO("chip->shutdown:%d\n", atomic_read(&chip->shutdown));
		if (!subs->stream)
			USB_OFFLOAD_INFO("NO subs->stream\n");
		else {
			if (!subs->stream->pcm)
				USB_OFFLOAD_INFO("NO subs->stream->pcm\n");
			if (!subs->stream->chip)
				USB_OFFLOAD_INFO("NO subs->stream->chip\n");
		}
		if (!subs->pcm_substream)
			USB_OFFLOAD_INFO("NO subs->pcm_substream\n");

		ret = -ENODEV;
		mutex_unlock(&uodev->dev_lock);
		goto done;
	}

	if (uainfo->service_interval_valid) {
		ret = get_data_interval_from_si(subs,
						uainfo->service_interval);
		if (ret == -EINVAL) {
			USB_OFFLOAD_ERR("invalid service interval %u\n",
					uainfo->service_interval);
			mutex_unlock(&uodev->dev_lock);
			goto done;
		}

		datainterval = ret;
		USB_OFFLOAD_INFO("data interval %u\n", ret);
	}

	uadev[pcm_card_num].ctrl_intf = chip->ctrl_intf;

	if (!uainfo->enable) {
		info = &uadev[pcm_card_num].info[info_idx];
		if (info->data_ep_pipe) {
			ep = usb_pipe_endpoint(uadev[pcm_card_num].udev,
					info->data_ep_pipe);
			if (!ep)
				USB_OFFLOAD_ERR("no data ep\n");
			else
				USB_OFFLOAD_DBG("stop data ep\n");
			info->data_ep_pipe = 0;
		}

		if (info->sync_ep_pipe) {
			ep = usb_pipe_endpoint(uadev[pcm_card_num].udev,
					info->sync_ep_pipe);
			if (!ep)
				USB_OFFLOAD_ERR("no sync ep\n");
			else
				USB_OFFLOAD_MEM_DBG("stop sync ep\n");
			info->sync_ep_pipe = 0;
		}
	}

	substream = subs->pcm_substream;
	USB_OFFLOAD_DBG("pcm_substream->wait_time: %lu\n", substream->wait_time);

	if (!substream->ops->hw_params || !substream->ops->hw_free
		|| !substream->ops->prepare) {

		USB_OFFLOAD_ERR("no hw_params/hw_free/prepare ops\n");
		ret = -ENODEV;
		mutex_unlock(&uodev->dev_lock);
		goto done;
	}

	if (uainfo->enable) {
		ret = usb_offload_prepare_msg(subs, uainfo, &msg, info_idx);
		USB_OFFLOAD_INFO("prepare msg, ret: %d\n", ret);
		if (ret < 0) {
			mutex_unlock(&uodev->dev_lock);
			return ret;
		}

		ret = usb_offload_prepare_msg_ext(&msg, uainfo, subs);
		USB_OFFLOAD_INFO("prepare msg ext, ret: %d\n", ret);
		if (ret < 0) {
			mutex_unlock(&uodev->dev_lock);
			return ret;
		}

		if (sram_version == 0x3) {
			/* re-placing trasnfer ring on rsv_sram/dram */
			ret = xhci_mtk_realloc_isoc_ring(subs);
			USB_OFFLOAD_INFO("re-alloc transfer ring, ret: %d\n", ret);
			if (ret != 0) {
				mutex_unlock(&uodev->dev_lock);
				return ret;
			}
		}

		USB_OFFLOAD_DBG("is_vcore_hold:%d dir:%d speed:%d\n",
			is_vcore_hold, uainfo->direction, subs->dev->speed);
		if (uainfo->direction &&
			subs->dev->speed == USB_SPEED_HIGH) {
			if (!is_vcore_hold) {
				USB_OFFLOAD_INFO("request holding vcore\n");
				usb_boost_vcore_control(true);
				is_vcore_hold = true;
			}
		}

	} else {
		ret = substream->ops->hw_free(substream);
		USB_OFFLOAD_INFO("hw_free, ret: %d\n", ret);

		msg.uainfo.direction = uainfo->direction;

		USB_OFFLOAD_DBG("is_vcore_hold:%d dir:%d speed:%d\n",
			is_vcore_hold, uainfo->direction, subs->dev->speed);
		if (uainfo->direction &&
			subs->dev->speed == USB_SPEED_HIGH) {
			if (is_vcore_hold) {
				usb_boost_vcore_control(false);
				USB_OFFLOAD_INFO("request releasing vcore\n");
				is_vcore_hold = false;
			}
		}
	}
	mutex_unlock(&uodev->dev_lock);

	msg.status = uainfo->enable ?
		USB_AUDIO_STREAM_REQ_START : USB_AUDIO_STREAM_REQ_STOP;

	/* write to audio ipi*/
	ret = send_uas_ipi_msg_to_adsp(&msg);
	USB_OFFLOAD_INFO("send_ipi_msg_to_adsp msg, ret: %d\n", ret);
	/* wait response */
	if (!uainfo->enable)
		mtk_usb_offload_free_allocated(uainfo->direction == SNDRV_PCM_STREAM_CAPTURE);

done:
	if ((!uainfo->enable && ret != -EINVAL && ret != -ENODEV) ||
		(uainfo->enable && ret == -ENODEV)) {
		mutex_lock(&uodev->dev_lock);
		if (info_idx >= 0) {
			if (!uadev[pcm_card_num].info) {
				USB_OFFLOAD_ERR("uaudio_dev cleanup already!\n");
				mutex_unlock(&uodev->dev_lock);
				return ret;
			}
			info = &uadev[pcm_card_num].info[info_idx];

			usb_audio_dev_intf_cleanup(
									uadev[pcm_card_num].udev,
									info);
			USB_OFFLOAD_INFO("release resources: intf# %d card# %d\n",
					interface, pcm_card_num);
		}
		if (atomic_read(&uadev[pcm_card_num].in_use))
			kref_put(&uadev[pcm_card_num].kref, uaudio_dev_release);
		mutex_unlock(&uodev->dev_lock);
	}

	return ret;
}

static bool xhci_mtk_is_usb_offload_enabled(struct xhci_hcd *xhci,
					   struct xhci_virt_device *vdev,
					   unsigned int ep_index)
{
	return true;
}

static struct xhci_device_context_array *xhci_mtk_alloc_dcbaa(struct xhci_hcd *xhci,
						 gfp_t flags)
{
	struct xhci_device_context_array *xhci_ctx;

	USB_OFFLOAD_MEM_DBG("\n");
	buf_dcbaa = kzalloc(sizeof(struct usb_offload_buffer), GFP_KERNEL);
	if (mtk_offload_alloc_mem(buf_dcbaa, sizeof(*xhci_ctx), 64,
				USB_OFFLOAD_MEM_DRAM_ID, true)) {
		USB_OFFLOAD_ERR("FAIL to allocate mem for USB Offload DCBAA\n");
		return NULL;
	}

	xhci_ctx = (struct xhci_device_context_array *) buf_dcbaa->dma_area;
	xhci_ctx->dma = buf_dcbaa->dma_addr;
	USB_OFFLOAD_MEM_DBG("#dcbaa vir:%p phy:0x%llx size:%lu\n",
			xhci_ctx->dev_context_ptrs, xhci_ctx->dma, sizeof(*xhci_ctx));

	buf_ctx = kzalloc(sizeof(struct usb_offload_buffer) * BUF_CTX_SIZE, GFP_KERNEL);

	return xhci_ctx;
}

static void xhci_mtk_free_dcbaa(struct xhci_hcd *xhci)
{
	USB_OFFLOAD_MEM_DBG("\n");
	if (!buf_dcbaa) {
		USB_OFFLOAD_ERR("DCBAA has not been initialized.\n");
		return;
	}

	if (mtk_offload_free_mem(buf_dcbaa))
		USB_OFFLOAD_ERR("FAIL to free mem for USB Offload DCBAA\n");
	else
		USB_OFFLOAD_MEM_DBG("Free mem DCBAA DONE\n");

	kfree(buf_ctx);
	kfree(buf_dcbaa);
	buf_ctx = NULL;
	buf_dcbaa = NULL;

	memory_cleanup();
	deinit_fake_rsv_sram();
}

static int get_first_avail_buf_ctx_idx(struct xhci_hcd *xhci)
{
	unsigned int idx;

	for (idx = 0; idx <= BUF_CTX_SIZE; idx++) {
		if (!buf_ctx[idx].allocated)
			return idx;
	}
	USB_OFFLOAD_ERR("NO Available BUF Context.\n");
	return 0;
}

static void xhci_mtk_alloc_container_ctx(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx,
				int type, gfp_t flags)
{

	int buf_ctx_slot = get_first_avail_buf_ctx_idx(xhci);

	if (mtk_offload_alloc_mem(&buf_ctx[buf_ctx_slot], ctx->size, 64,
				USB_OFFLOAD_MEM_DRAM_ID, true)) {
		USB_OFFLOAD_ERR("FAIL to allocate mem for USB Offload Context %d size: %d\n",
				buf_ctx_slot, ctx->size);
		return;
	}

	ctx->bytes = buf_ctx[buf_ctx_slot].dma_area;
	ctx->dma = buf_ctx[buf_ctx_slot].dma_addr;
	USB_OFFLOAD_MEM_DBG("#ctx%d vir:%p dma:0x%llx size:%d\n", buf_ctx_slot, ctx->bytes, ctx->dma, ctx->size);
}

static void xhci_mtk_free_container_ctx(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx)
{
	unsigned int idx;

	for (idx = 0; idx < BUF_CTX_SIZE; idx++) {
		if (buf_ctx[idx].allocated && buf_ctx[idx].dma_addr == ctx->dma) {
			if (mtk_offload_free_mem(&buf_ctx[idx]))
				USB_OFFLOAD_ERR("FAIL: free mem ctx: %d\n", idx);
			return;
		}
	}
	USB_OFFLOAD_MEM_DBG("NO Context MATCH to be freed. vir:%p dma:0x%llx size:%d\n",
			ctx->bytes,	ctx->dma, ctx->size);
}

static int get_first_avail_buf_seg_idx(void)
{
	unsigned int idx;

	if (!buf_seg) {
		USB_OFFLOAD_ERR("buf_seg is NULL\n");
		return -1;
	}

	for (idx = 0; idx < BUF_SEG_SIZE; idx++) {
		USB_OFFLOAD_MEM_DBG("buf_seg[%d]:%p alloc:%d va:%p phy:0x%llx size:%zu\n",
					idx,
					&buf_seg[idx],
					buf_seg[idx].allocated,
					buf_seg[idx].dma_area,
					buf_seg[idx].dma_addr,
					buf_seg[idx].dma_bytes);
		if (!buf_seg[idx].allocated)
			return idx;
	}
	USB_OFFLOAD_ERR("NO Available BUF Segment.\n");
	return -1;
}

struct usb_offload_buffer *usb_offload_get_ring_buf(dma_addr_t phy)
{
	unsigned int idx;

	if (!buf_seg) {
		USB_OFFLOAD_ERR("buf_seg is NULL\n");
		return NULL;
	}

	for (idx = 0; idx < BUF_SEG_SIZE; idx++) {
		if (buf_seg[idx].allocated && buf_seg[idx].dma_addr == phy)
			return &buf_seg[idx];
	}

	USB_OFFLOAD_INFO("NO Segment MATCH. dma:0x%llx\n",	phy);
	return NULL;
}

static void xhci_mtk_usb_offload_segment_free(struct xhci_hcd *xhci,
			struct xhci_segment *seg)
{
	struct usb_offload_buffer *buf;

	if (seg->trbs) {
		buf = usb_offload_get_ring_buf(seg->dma);
		if (buf && mtk_offload_free_mem(buf))
			USB_OFFLOAD_ERR("FAIL: free mem seg\n");
		seg->trbs = NULL;
	}

	kfree(seg->bounce_buf);
	kfree(seg);
}

static void xhci_mtk_usb_offload_free_segments_for_ring(struct xhci_hcd *xhci,
				struct xhci_segment *first)
{
	/* only free 1st segment */
	xhci_mtk_usb_offload_segment_free(xhci, first);
}

/*
 * Allocates a generic ring segment from the ring pool, sets the dma address,
 * initializes the segment to zero, and sets the private next pointer to NULL.
 *
 * Section 4.11.1.1:
 * "All components of all Command and Transfer TRBs shall be initialized to '0'"
 */
static struct xhci_segment *xhci_mtk_usb_offload_segment_alloc(struct xhci_hcd *xhci,
						   unsigned int cycle_state,
						   unsigned int max_packet,
						   gfp_t flags,
						   enum usb_offload_mem_id mem_id,
						   bool is_rsv,
						   enum xhci_ring_type type)
{
	struct xhci_segment *seg;
	dma_addr_t	dma;
	int		i;
	int buf_seg_slot = 0;
	struct usb_offload_buffer *buf;

	if (type == TYPE_EVENT)
		buf = buf_ev_ring;
	else {
		buf_seg_slot = get_first_avail_buf_seg_idx();
		buf = &buf_seg[buf_seg_slot];
		if (buf_seg_slot < 0)
			return NULL;
	}

	seg = kzalloc(sizeof(*seg), flags);
	if (!seg)
		return NULL;

	if (mtk_offload_alloc_mem(buf, USB_OFFLOAD_TRB_SEGMENT_SIZE,
			USB_OFFLOAD_TRB_SEGMENT_SIZE, mem_id, is_rsv)) {
		USB_OFFLOAD_ERR("fail allocating ring (type:%d size:%d mem_id:%d seg_slot:%d)\n",
			type, USB_OFFLOAD_TRB_SEGMENT_SIZE, mem_id, buf_seg_slot);
		kfree(seg);
		return NULL;
	}

	seg->trbs = (void *) buf->dma_area;
	seg->dma = 0;
	dma = buf->dma_addr;
	USB_OFFLOAD_MEM_DBG("#%s_ring%d vir:%p phy:0x%llx size:%lu\n",
			type == TYPE_EVENT ? "ev" : "tr",
			type == TYPE_EVENT ? 0 : buf_seg_slot,
			seg->trbs, dma, (unsigned long)buf->dma_bytes);

	if (!seg->trbs) {
		USB_OFFLOAD_ERR("No seg->trbs\n");
		kfree(seg);
		return NULL;
	}

	if (max_packet) {
		seg->bounce_buf = kzalloc(max_packet, flags);
		if (!seg->bounce_buf) {
			xhci_mtk_usb_offload_segment_free(xhci, seg);
			return NULL;
		}
	}
	/* If the cycle state is 0, set the cycle bit to 1 for all the TRBs */
	if (cycle_state == 0) {
		for (i = 0; i < USB_OFFLOAD_TRBS_PER_SEGMENT; i++)
			seg->trbs[i].link.control |= cpu_to_le32(TRB_CYCLE);
	}
	seg->dma = dma;
	seg->next = NULL;

	return seg;
}

/* Allocate segments and link them for a ring */
static int xhci_mtk_usb_offload_alloc_segments_for_ring(struct xhci_hcd *xhci,
		struct xhci_segment **first, struct xhci_segment **last,
		unsigned int num_segs, unsigned int cycle_state,
		enum xhci_ring_type type, unsigned int max_packet, gfp_t flags,
		enum usb_offload_mem_id mem_id, bool is_rsv)
{
	struct xhci_segment *prev;
	bool chain_links;

	/* Set chain bit for 0.95 hosts, and for isoc rings on AMD 0.96 host */
	chain_links = !!(xhci_link_trb_quirk(xhci) ||
			 (type == TYPE_ISOC &&
			  (xhci->quirks & XHCI_AMD_0x96_HOST)));

	prev = xhci_mtk_usb_offload_segment_alloc(xhci, cycle_state, max_packet, flags,
			mem_id, is_rsv, type);
	if (!prev)
		return -ENOMEM;
	num_segs--;

	*first = prev;
	while (num_segs > 0) {
		struct xhci_segment	*next;

		next = xhci_mtk_usb_offload_segment_alloc(
					xhci, cycle_state, max_packet, flags, mem_id, is_rsv, type);
		if (!next) {
			prev = *first;
			while (prev) {
				next = prev->next;
				xhci_mtk_usb_offload_segment_free(xhci, prev);
				prev = next;
			}
			return -ENOMEM;
		}
#if IS_ENABLED(CONFIG_DEVICE_MODULES_USB_XHCI_MTK)
		xhci_link_segments_(prev, next, type, chain_links);
#else
		xhci_link_segments(prev, next, type, chain_links);
#endif
		prev = next;
		num_segs--;
	}
#if IS_ENABLED(CONFIG_DEVICE_MODULES_USB_XHCI_MTK)
	xhci_link_segments_(prev, *first, type, chain_links);
#else
	xhci_link_segments(prev, *first, type, chain_links);
#endif
	*last = prev;
	return 0;
}

static void xhci_mtk_link_rings(struct xhci_hcd *xhci, struct xhci_ring *ring,
	struct xhci_segment *first, struct xhci_segment *last, unsigned int num_segs)
{
	struct xhci_segment *next;
	bool chain_links;

	chain_links = !!(xhci_link_trb_quirk(xhci) ||
			 (ring->type == TYPE_ISOC &&
			  (xhci->quirks & XHCI_AMD_0x96_HOST)));

	next = ring->enq_seg->next;
#if IS_ENABLED(CONFIG_DEVICE_MODULES_USB_XHCI_MTK)
	xhci_link_segments_(ring->enq_seg, first, ring->type, chain_links);
	xhci_link_segments_(last, next, ring->type, chain_links);
#else
	xhci_link_segments(ring->enq_seg, first, ring->type, chain_links);
	xhci_link_segments(last, next, ring->type, chain_links);
#endif
	ring->num_segs += num_segs;
	ring->num_trbs_free += (TRBS_PER_SEGMENT - 1) * num_segs;

	if (ring->type != TYPE_EVENT && ring->enq_seg == ring->last_seg) {
		ring->last_seg->trbs[TRBS_PER_SEGMENT-1].link.control
			&= ~cpu_to_le32(LINK_TOGGLE);
		last->trbs[TRBS_PER_SEGMENT-1].link.control
			|= cpu_to_le32(LINK_TOGGLE);
		ring->last_seg = last;
	}
}

static int xhci_mtk_update_erst(struct usb_offload_dev *udev, struct xhci_segment *first,
	struct xhci_segment *last, unsigned int num_segs)
{
	struct xhci_segment *segment;
	struct xhci_ring *event_ring = udev->event_ring;
	struct xhci_erst *erst = udev->erst;
	struct xhci_erst_entry *entry;
	unsigned int entries_idx, entries_in_use = udev->num_entries_in_use;
	unsigned int ev_seg_num = event_ring->num_segs;

	USB_OFFLOAD_INFO("ev_segs:%d entries_in_use:%d",
		ev_seg_num, entries_in_use);

	if (ev_seg_num <= entries_in_use) {
		USB_OFFLOAD_INFO("no need to update erst\n");
		return 0;
	}

	entries_idx = ev_seg_num - num_segs;
	USB_OFFLOAD_MEM_DBG("need %d more erst entries, start with entry%d\n",
		ev_seg_num - entries_in_use, entries_idx);

	segment = first;
	while (ev_seg_num > entries_in_use) {
		USB_OFFLOAD_INFO("[erst]entry%d\n", entries_idx);
		entry = &erst->entries[entries_idx];
		entry->seg_addr = cpu_to_le64(segment->dma);
		entry->seg_size = cpu_to_le32(USB_OFFLOAD_TRBS_PER_SEGMENT);

		USB_OFFLOAD_MEM_DBG("entry:%p seg_addr:0x%llx seg_size:%d\n",
			entry, entry->seg_addr, entry->seg_size);

		entries_idx++;
		entries_in_use++;
		segment = segment->next;
	}
	udev->num_entries_in_use = entries_in_use;
	USB_OFFLOAD_INFO("num_entries_in_use:%d\n", udev->num_entries_in_use);

	return 0;
}

static int xhci_mtk_ring_expansion(struct xhci_hcd *xhci,
	struct xhci_ring *ring, dma_addr_t phys, void *vir)
{
	struct xhci_segment *new_seg, *seg;
	unsigned int num_segs = 1, max_packet, i;
	bool chain_links;

	if (!ring)
		return -EINVAL;

	chain_links = !!(xhci_link_trb_quirk(xhci) ||
			 (ring->type == TYPE_ISOC &&
			  (xhci->quirks & XHCI_AMD_0x96_HOST)));

	USB_OFFLOAD_INFO("phys:0x%llx vir:%p\n", (u64)phys, vir);

	new_seg = kzalloc(sizeof(*new_seg), GFP_ATOMIC);
	if (!new_seg)
		return -ENOMEM;

	new_seg->trbs = vir;
	new_seg->dma = phys;

	USB_OFFLOAD_INFO("vir:%p phy:0x%llx\n",
		new_seg->trbs, (u64)new_seg->dma);

	if (!new_seg->trbs) {
		USB_OFFLOAD_ERR("No seg->trbs\n");
		kfree(new_seg);
		return -EINVAL;
	}

	max_packet = ring->bounce_buf_len;
	if (max_packet) {
		new_seg->bounce_buf = kzalloc(max_packet, GFP_ATOMIC);
		if (!new_seg->bounce_buf) {
			kfree(new_seg);
			return -ENOMEM;
		}
	}

	if (ring->cycle_state == 0) {
		for (i = 0; i < USB_OFFLOAD_TRBS_PER_SEGMENT; i++)
			new_seg->trbs[i].link.control |= cpu_to_le32(TRB_CYCLE);
	}
	new_seg->next = NULL;

#if IS_ENABLED(CONFIG_DEVICE_MODULES_USB_XHCI_MTK)
	xhci_link_segments_(new_seg, new_seg, ring->type, chain_links);
#else
	xhci_link_segments(new_seg, new_seg, ring->type, chain_links);
#endif

	xhci_mtk_link_rings(xhci, ring, new_seg, new_seg, num_segs);

	if (ring->type == TYPE_EVENT)
		xhci_mtk_update_erst(uodev, new_seg, new_seg, num_segs);

	seg = ring->first_seg;
	for (i = 0; i < ring->num_segs; i++) {
		USB_OFFLOAD_INFO("[seg%d] vir:%p phy:0x%llx point_to:0x%llx\n",
			i, seg->trbs, (u64)seg->dma,
			seg->trbs[USB_OFFLOAD_TRBS_PER_SEGMENT - 1].link.segment_ptr);
		seg = seg->next;
	}

	return 0;
}

static struct xhci_ring *xhci_mtk_alloc_ring(struct xhci_hcd *xhci,
		int num_segs, int cycle_state, enum xhci_ring_type ring_type,
		unsigned int max_packet, gfp_t mem_flags,
		enum usb_offload_mem_id mem_id, bool is_rsv)
{
	struct xhci_ring	*ring;
	int ret;

	ring = kzalloc(sizeof(*ring), mem_flags);
	if (!ring)
		return NULL;

	ring->num_segs = num_segs;
	ring->bounce_buf_len = max_packet;
	INIT_LIST_HEAD(&ring->td_list);
	ring->type = ring_type;

	ret = xhci_mtk_usb_offload_alloc_segments_for_ring(xhci, &ring->first_seg,
			&ring->last_seg, num_segs, cycle_state, ring_type,
			max_packet, mem_flags, mem_id, is_rsv);
	if (ret) {
		USB_OFFLOAD_ERR("Fail to alloc segment for rings (mem_id:%d)\n", lowpwr_mem_type());
		goto fail;
	}

	if (ring_type != TYPE_EVENT) {
		/* See section 4.9.2.1 and 6.4.4.1 */
		ring->last_seg->trbs[USB_OFFLOAD_TRBS_PER_SEGMENT - 1].link.control |=
			cpu_to_le32(LINK_TOGGLE);
	}
	xhci_initialize_ring_info_(ring, cycle_state);
	return ring;

fail:
	kfree(ring);
	return NULL;
}

static void xhci_mtk_usb_offload_free_segments_for_ev_ring(struct xhci_hcd *xhci,
				struct xhci_segment *first)
{
	if (first->trbs) {
		if (mtk_offload_free_mem(buf_ev_ring))
			USB_OFFLOAD_ERR("Fail to free ev_ring\n");
		first->trbs = NULL;
	}

	kfree(first->bounce_buf);
	kfree(first);
}

static void xhci_mtk_free_ring(struct xhci_hcd *xhci, struct xhci_ring *ring)
{
	if (!ring)
		return;

	if (ring->first_seg) {
		if (ring->type == TYPE_EVENT)
			xhci_mtk_usb_offload_free_segments_for_ev_ring(xhci, ring->first_seg);
		else
			xhci_mtk_usb_offload_free_segments_for_ring(xhci, ring->first_seg);
	}

	kfree(ring);
}

static void xhci_mtk_free_event_ring(struct usb_offload_dev *udev)
{
	USB_OFFLOAD_MEM_DBG("\n");

	if (!udev->event_ring) {
		USB_OFFLOAD_INFO("Event ring has already freed\n");
		return;
	}

	xhci_mtk_free_ring(udev->xhci, udev->event_ring);
	udev->event_ring = NULL;
	dec_region(EV_RING);

	/* Detect by check_service: kfree(NULL) is safe, remove NULL checker */
	kfree(udev->backup_ev_ring);
	udev->backup_ev_ring = NULL;
}

static void xhci_mtk_free_erst(struct usb_offload_dev *udev)
{
	USB_OFFLOAD_MEM_DBG("\n");

	if (!udev->erst) {
		USB_OFFLOAD_INFO("ERST has already freed\n");
		return;
	}

	if (mtk_offload_free_mem(buf_ev_table))
		USB_OFFLOAD_ERR("Fail to free erst\n");
	else {
		udev->erst->entries = NULL;
		udev->erst = NULL;
		udev->num_entries_in_use = 0;
		dec_region(ERST);

		/* Detect by check_service: kfree(NULL) is safe, remove NULL checker */
		kfree(udev->backup_erst);
		udev->backup_erst = NULL;
	}
}

static struct xhci_ring *xhci_mtk_alloc_transfer_ring(struct xhci_hcd *xhci,
		u32 endpoint_type, enum xhci_ring_type ring_type,
		unsigned int max_packet, gfp_t mem_flags)
{
	struct xhci_ring *ring;
	int num_segs = 1;
	int cycle_state = 1;
	enum usb_offload_mem_id mem_type;

	USB_OFFLOAD_MEM_DBG("\n");

	if (endpoint_type != ISOC_OUT_EP && endpoint_type != ISOC_IN_EP) {
		USB_OFFLOAD_ERR("wrong endpoint type, type=%d\n", endpoint_type);
		return NULL;
	}

	if (sram_version == 0x3) {
		/* place transfer ring on native dram
		 * to prevent from occupying sram in non-offload mode
		 */
		ring = xhci_ring_alloc_(xhci, 2, 1, ring_type, max_packet, mem_flags);
		if (ring && ring->first_seg)
			USB_OFFLOAD_MEM_DBG("(native ring) vir:%p phy:0x%llx\n", ring, ring->first_seg->dma);
		return ring;
	}

	init_fake_rsv_sram();

	if (uodev->adv_lowpwr_dl_only && endpoint_type == ISOC_IN_EP)
		mem_type = USB_OFFLOAD_MEM_DRAM_ID;
	else
		mem_type = lowpwr_mem_type();

	ring = xhci_mtk_alloc_ring(xhci, num_segs, cycle_state,
			ring_type, max_packet, mem_flags, mem_type, true);
	if (!ring) {
		USB_OFFLOAD_ERR("ring is NULL\n");
		return ring;
	}

	return ring;
}

static int get_segment_buf(struct xhci_ring *ring, struct usb_offload_buffer **buf)
{
	struct xhci_segment *seg;
	unsigned int idx;

	if (!ring || !ring->first_seg)
		return -1;

	seg = ring->first_seg;
	if (!buf_seg || !seg->trbs)
		return -1;

	*buf = NULL;
	for (idx = 0; idx < BUF_SEG_SIZE; idx++) {
		if (buf_seg[idx].allocated && buf_seg[idx].dma_addr == seg->dma) {
			*buf = &buf_seg[idx];
			break;
		}
	}

	return 0;
}

/* realloc transfer ring, placing on adsp/ap-view memory (sram or dram) */
int xhci_mtk_realloc_transfer_ring(unsigned int slot_id, unsigned int ep_id,
	enum usb_offload_mem_id mem_type)
{
	struct xhci_hcd *xhci = uodev->xhci;
	struct xhci_virt_device *virt_dev;
	struct xhci_ep_ctx *out_ep_ctx, *in_ep_ctx;
	struct xhci_input_control_ctx *ctrl_ctx;
	struct usb_hcd *hcd = xhci->main_hcd;
	struct xhci_ring *ring;
	int num_segs = 1, max_packet;
	int cycle_state = 1;
	enum xhci_ring_type ring_type;

	virt_dev = xhci->devs[slot_id];
	if (!virt_dev) {
		USB_OFFLOAD_ERR("(slot:%d) virtual device is NULL\n", slot_id);
		return -1;
	}

	USB_OFFLOAD_INFO("(slot:%d ep:%d) virt_dev:%p\n", slot_id, ep_id, virt_dev);

	out_ep_ctx = xhci_get_ep_ctx__(xhci, virt_dev->out_ctx, ep_id);
	in_ep_ctx = xhci_get_ep_ctx__(xhci, virt_dev->in_ctx, ep_id);
	if (!out_ep_ctx || !in_ep_ctx) {
		USB_OFFLOAD_ERR("(slot:%d ep:%d) endpoint context is NULL\n", slot_id, ep_id);
		return -1;
	}

	/* 1. create new ring
	 * note that it's unnecessary to free old ring, because as long as we pull
	 * high drop_flag, xhci_check_bandwidth_() would free old one.
	 *
	 * virt_dev->eps[ep_id].ring is the ring which xhci currently uses.
	 * virt_dev->eps[ep_id].new_ring is the temp buffer where we place new ring.
	 * xhci_check_bandwidth_() would assign (ring = new_ring).
	 */
	max_packet = virt_dev->eps[ep_id].ring->bounce_buf_len;
	ring_type = virt_dev->eps[ep_id].ring->type;
	ring = xhci_mtk_alloc_ring(xhci, num_segs, cycle_state, ring_type,
		max_packet, GFP_NOIO, mem_type, true);
	if (!ring) {
		USB_OFFLOAD_ERR("ring is NULL\n");
		return -1;
	}
	inc_region(TR_RING);
	virt_dev->eps[ep_id].new_ring = ring;
	USB_OFFLOAD_INFO("(slot:%d ep:%d) create new ring, ring:%p va:%p phy:0x%llx\n",
		slot_id, ep_id, virt_dev->eps[ep_id].new_ring,
		virt_dev->eps[ep_id].new_ring->first_seg->trbs,
		virt_dev->eps[ep_id].new_ring->first_seg->dma);

	/* 2. update dequeue pointer of output ep context */
	out_ep_ctx->deq = cpu_to_le64(virt_dev->eps[ep_id].new_ring->first_seg->dma |
					virt_dev->eps[ep_id].new_ring->cycle_state);

	/* 3. copy output endpoint context to input endpoint context */
	in_ep_ctx->ep_info = out_ep_ctx->ep_info;
	in_ep_ctx->ep_info2 = out_ep_ctx->ep_info2;
	in_ep_ctx->deq = out_ep_ctx->deq;
	in_ep_ctx->tx_info = out_ep_ctx->tx_info;
	if (xhci->quirks & XHCI_MTK_HOST) {
		in_ep_ctx->reserved[0] = out_ep_ctx->reserved[0];
		in_ep_ctx->reserved[1] = out_ep_ctx->reserved[1];
	}

	/* 4. pull high both add and drop flag to reconfigure ep
	 * note ep_id is actully the index of virt_dev->eps
	 * for input control context, it should be (ep_id + 1)
	 */
	ctrl_ctx = (struct xhci_input_control_ctx *)virt_dev->in_ctx->bytes;
	ctrl_ctx->add_flags = cpu_to_le32(BIT(ep_id + 1));
	ctrl_ctx->drop_flags = cpu_to_le32(BIT(ep_id + 1));

	/* 5. inform XHC to reconfigure endoint */
	return xhci_check_bandwidth_(hcd, virt_dev->udev);
}

static int xhci_mtk_realloc_isoc_ring(struct snd_usb_substream *subs)
{
	struct usb_host_endpoint *ep;
	unsigned int slot_id, ep_id;
	enum usb_offload_mem_id mem_type;

	ep = usb_pipe_endpoint(subs->dev, subs->data_endpoint->pipe);
	if (!ep) {
		USB_OFFLOAD_ERR("host endpoint is NULL\n");
		return -EINVAL;
	}

	slot_id = subs->dev->slot_id;
	ep_id = xhci_get_endpoint_index_(&ep->desc);

	if (uodev->adv_lowpwr_dl_only && usb_endpoint_dir_in(&ep->desc))
		mem_type = USB_OFFLOAD_MEM_DRAM_ID;
	else
		mem_type = lowpwr_mem_type();

	return xhci_mtk_realloc_transfer_ring(slot_id, ep_id, mem_type);
}

/* Xhci checkes ep type before allocating transfer ring but it doesn't
 * when it comes to freeing transfer ring. So here, prior to actually
 * free memory, we'll check if segment lies on buf_seg.
 */
static void xhci_mtk_free_transfer_ring(struct xhci_hcd *xhci,
	struct xhci_ring *ring, unsigned int ep_index)
{
	int ret;
	struct usb_offload_buffer *buf;

	USB_OFFLOAD_MEM_DBG("ep_index:%d\n", ep_index);

	ret = get_segment_buf(ring, &buf);
	if (ret != 0)
		return;

	if (buf) {
		xhci_mtk_free_ring(xhci, ring);
		dec_region(TR_RING);
	}
	else {
		USB_OFFLOAD_MEM_DBG("phy:0x%llx isn't under managed\n", ring->first_seg->dma);
		xhci_ring_free_(xhci, ring);
	}
}

static int xhci_mtk_alloc_event_ring(struct usb_offload_dev *udev)
{
	int num_segs = 1;
	int cycle_state = 1;
	int ret;

	udev->event_ring = xhci_mtk_alloc_ring(udev->xhci, num_segs, cycle_state,
			TYPE_EVENT, 0, GFP_ATOMIC, lowpwr_mem_type(), true);
	if (!udev->event_ring) {
		USB_OFFLOAD_ERR("error allocating event ring\n");
		ret = -ENOMEM;
		goto FAIL_ALLOCATE_EV_RING;
	}

	USB_OFFLOAD_INFO("[event ring] va:%p phy:0x%llx\n",
		udev->event_ring->first_seg->trbs,
		(unsigned long long)udev->event_ring->first_seg->dma);

	inc_region(EV_RING);
	if (sram_version == 0x3) {
		udev->backup_ev_ring = kzalloc(USB_OFFLOAD_TRB_SEGMENT_SIZE, GFP_ATOMIC);
		if (!udev->backup_ev_ring) {
			USB_OFFLOAD_ERR("error allocating backup event ring\n");
			ret = -ENOMEM;
			goto FAIL_ALLOCATE_BACKUP_EV_RING;
		}
	}
	return 0;
FAIL_ALLOCATE_BACKUP_EV_RING:
	xhci_mtk_free_ring(udev->xhci, udev->event_ring);
	udev->event_ring = NULL;
	dec_region(EV_RING);
FAIL_ALLOCATE_EV_RING:
	return ret;
}

static int xhci_mtk_alloc_erst(struct usb_offload_dev *udev)
{
	int ret;

	ret = mtk_offload_alloc_mem(buf_ev_table, ERST_SIZE * ERST_NUMBER,
		64, lowpwr_mem_type(), true);
	if (ret != 0) {
		USB_OFFLOAD_ERR("Allocate ERST Fail!!!\n");
		goto FAIL_TO_ALLOC_ERST;
	}

	udev->num_entries_in_use = 1;
	udev->erst = kzalloc(sizeof(struct xhci_erst), GFP_ATOMIC);
	if (!udev->erst) {
		USB_OFFLOAD_ERR("Allocate xhci_erst Fail!!!\n");
		ret = -ENOMEM;
		goto FAIL_TO_ALLOC_XHCI_ERST;
	}
	udev->erst->entries = (struct xhci_erst_entry *)buf_ev_table->dma_area;
	udev->erst->erst_dma_addr = buf_ev_table->dma_addr;
	udev->erst->num_entries = ERST_NUMBER;

	USB_OFFLOAD_INFO("[erst] va:%p phy:0x%llx is_sram:%d is_rsv:%d\n",
		buf_ev_table->dma_area,
		(unsigned long long)buf_ev_table->dma_addr,
		buf_ev_table->is_sram,
		buf_ev_table->is_rsv);

	inc_region(ERST);
	if (sram_version == 0x3) {
		udev->backup_erst = kzalloc(sizeof(struct xhci_erst_entry)
			* ERST_NUMBER, GFP_ATOMIC);
		if (!udev->backup_erst){
			USB_OFFLOAD_ERR("error allocating backup erst\n");
			ret = -ENOMEM;
			goto FAIL_TO_ALLOC_XHCI_BACKUP_ERST;
		}
	}
	return 0;
FAIL_TO_ALLOC_XHCI_BACKUP_ERST:
	kfree(udev->erst);
	udev->erst = NULL;
	dec_region(ERST);
FAIL_TO_ALLOC_XHCI_ERST:
	udev->num_entries_in_use = 0;
	if (mtk_offload_free_mem(buf_ev_table))
		USB_OFFLOAD_ERR("Fail to free erst\n");
FAIL_TO_ALLOC_ERST:
	return ret;
}

#define USB_OFFLOAD_XHCI_INTR_TARGET 1
static int xhci_initialize_ir(struct usb_offload_dev *udev)
{
	int ret = 0, i;
	unsigned int val;
	u64 val_64;
	struct xhci_hcd *xhci = udev->xhci;
	struct xhci_ring *ev_ring = udev->event_ring;
	struct xhci_erst *erst = udev->erst;
	struct xhci_intr_reg *ir_set = &xhci->run_regs->ir_set[USB_OFFLOAD_XHCI_INTR_TARGET];
	struct xhci_segment *seg;
	struct xhci_erst_entry *entry;

	/* 1. set event ring info into erst */
	erst->num_entries = ev_ring->num_segs;
	seg = ev_ring->first_seg;
	for (i = 0; i < ev_ring->num_segs; i++) {
		entry = &erst->entries[i];
		entry->seg_addr = cpu_to_le64(seg->dma);
		entry->seg_size = cpu_to_le32(USB_OFFLOAD_TRBS_PER_SEGMENT);
		entry->rsvd = 0;
		seg = seg->next;
		USB_OFFLOAD_MEM_DBG("entry%d seg_addr:0x%llx seg_size:0x%lx\n",
			i, (unsigned long long)entry->seg_addr, (unsigned long)entry->seg_size);
	}

	/* 2. pass event ring table to hardware */
	ir_set = &xhci->run_regs->ir_set[USB_OFFLOAD_XHCI_INTR_TARGET];
	val = readl(&ir_set->erst_size);
	val &= ERST_SIZE_MASK;
	val |= EV_MAX_SEG;
	writel(val, &ir_set->erst_size);

	val_64 = xhci_read_64(xhci, &ir_set->erst_base);
	val_64 &= ERST_PTR_MASK;
	val_64 |= (erst->erst_dma_addr & (u64) ~ERST_PTR_MASK);
	xhci_write_64(xhci, val_64, &ir_set->erst_base);

	USB_OFFLOAD_MEM_DBG("erat_base:0x%llx erst_size:0x%lx\n",
		(unsigned long long)(xhci_read_64(xhci, &ir_set->erst_base) & ERST_PTR_MASK),
		(unsigned long)readl(&ir_set->erst_size));
	return ret;
}

static void xhci_free_ir(struct usb_offload_dev *udev)
{
	xhci_mtk_free_erst(uodev);
	xhci_mtk_free_event_ring(uodev);
}

static int xhci_backup_ir(struct usb_offload_dev *udev)
{
	int i;
	struct xhci_erst_entry *entry, *backup_entry;
	struct xhci_erst *erst = udev->erst;
	struct xhci_ring *ev_ring = udev->event_ring;
	struct xhci_segment *seg;
	void *ev_src, *ev_des;

	if (!erst || !ev_ring) {
		USB_OFFLOAD_ERR("erst/ev_ring are NULL\n");
		return -EINVAL;
	}

	ir_lost = true;
	USB_OFFLOAD_MEM_DBG("ir_lost:%d->%d\n", !ir_lost, ir_lost);

	/* backup event ring table */
	for (i = 0; i < ERST_NUMBER; i++) {
		entry = &erst->entries[i];
		backup_entry = &udev->backup_erst[i];
		memcpy((void *)backup_entry, (void *)entry, ERST_SIZE);
	}

	seg = ev_ring->first_seg;
	ev_des = (void *)udev->backup_ev_ring;
	if (!seg || !ev_des) {
		USB_OFFLOAD_ERR("seg/ev_des are NULL\n");
		return -EINVAL;
	}

	/* backup event ring */
	for (i = 0; i < ev_ring->num_segs; i++) {
		ev_src = (void *)seg->trbs;
		memcpy(ev_des, ev_src, USB_OFFLOAD_TRB_SEGMENT_SIZE);
		ev_des += USB_OFFLOAD_TRB_SEGMENT_SIZE;
		seg = seg->next;
	}

	return 0;
}

static int xhci_restore_ir(struct usb_offload_dev *udev)
{
	int i;
	struct xhci_erst_entry *entry, *backup_entry;
	struct xhci_erst *erst = udev->erst;
	struct xhci_ring *ev_ring = udev->event_ring;
	struct xhci_segment *seg;
	void *ev_src, *ev_des;

	if (!erst || !ev_ring) {
		USB_OFFLOAD_ERR("erst/ev_ring are NULL\n");
		return -EINVAL;
	}

	ir_lost = false;
	USB_OFFLOAD_MEM_DBG("ir_lost:%d->%d\n", !ir_lost, ir_lost);

	for (i = 0; i < ERST_NUMBER; i++) {
		entry = &erst->entries[i];
		backup_entry = &udev->backup_erst[i];
		memcpy((void *)entry, (void *)backup_entry, ERST_SIZE);
	}

	seg = ev_ring->first_seg;
	ev_src = (void *)udev->backup_ev_ring;
	if (!seg || !ev_src) {
		USB_OFFLOAD_ERR("seg/ev_src are NULL\n");
		return -EINVAL;
	}

	for (i = 0; i < ev_ring->num_segs; i++) {
		ev_des = (void *)seg->trbs;
		memcpy(ev_des, ev_src, USB_OFFLOAD_TRB_SEGMENT_SIZE);
		ev_src += USB_OFFLOAD_TRB_SEGMENT_SIZE;
		seg = seg->next;
	}

	return 0;
}

static bool xhci_mtk_is_streaming(struct xhci_hcd *xhci)
{
	USB_OFFLOAD_DBG("is_streaming: %d\n", uodev->is_streaming);
	return uodev->is_streaming;
}

static int check_usb_offload_quirk(int vid, int pid)
{
	if (vid == 0x046D && pid == 0x0A38) {
		USB_OFFLOAD_INFO("Logitech USB Headset H340 NOT SUPPORT!!\n");
		return -1;
	}

	if (vid == 0x0BDA && pid == 0x4BD1) {
		USB_OFFLOAD_INFO("JOWOYE MH339 NOT SUPPORT!!\n");
		return -1;
	}
	return 0;
}

static int check_is_multiple_ep(struct usb_host_config *config)
{
	struct usb_interface *intf;
	struct usb_host_interface *hostif;
	struct usb_interface_descriptor *intfd;
	int i, j;

	if (!config)
		return -1;

	USB_OFFLOAD_DBG("num of intf: %d\n", config->desc.bNumInterfaces);

	for (i = 0; i < config->desc.bNumInterfaces; i++) {

		intf = config->interface[i];
		for (j = 0; j < intf->num_altsetting; j++) {
			if (!uodev->connected) {
				USB_OFFLOAD_ERR("No dev(%d)\n",
						uodev->connected);
				return -1;
			}
			hostif = &intf->altsetting[j];
			if (!hostif) {
				USB_OFFLOAD_ERR("No alt(%d)\n",
						uodev->connected);
				return -1;
			}
			intfd = get_iface_desc(hostif);
			if (!intfd) {
				USB_OFFLOAD_ERR("No intf desc(%d)\n",
						uodev->connected);
				return -1;
			}
			USB_OFFLOAD_DBG("intf:%d, alt:%d, numEP: %d, class:0x%x, sub:0x%x\n",
					i, j,
					intfd->bNumEndpoints,
					intfd->bInterfaceClass,
					intfd->bInterfaceSubClass);
			if (intfd->bNumEndpoints > 1 &&
				intfd->bInterfaceClass == 0x1 &&
				intfd->bInterfaceSubClass == 0x2) {
				USB_OFFLOAD_INFO("Multiple EP in one intf. NOT SUPPORT!!\n");
				return -1;
			}
		}
	}
	return 0;
}

int usb_offload_cleanup(void)
{
	int ret = 0;
	struct usb_audio_stream_msg msg = {0};
	unsigned int card_num = uodev->card_num;

	USB_OFFLOAD_INFO("%d\n", __LINE__);
	uodev->is_streaming = false;
	uodev->tx_streaming = false;
	uodev->rx_streaming = false;
	uodev->adsp_inited = false;
	uodev->opened = false;

	msg.status = USB_AUDIO_STREAM_REQ_STOP;
	msg.status_valid = 1;

	/* write to audio ipi*/
	ret = send_disconnect_ipi_msg_to_adsp();
	/* wait response */
	USB_OFFLOAD_INFO("send_disconnect_ipi_msg_to_adsp msg, ret: %d\n", ret);

	memory_cleanup();
	mutex_lock(&uodev->dev_lock);
	uaudio_dev_cleanup(&uadev[card_num]);
	USB_OFFLOAD_INFO("uadev[%d].in_use: %d ==> 0\n",
			card_num, atomic_read(&uadev[card_num].in_use));
	atomic_set(&uadev[card_num].in_use, 0);
	mutex_unlock(&uodev->dev_lock);
	return ret;
}

static int usb_offload_open(struct inode *ip, struct file *fp)
{
	struct xhci_hcd *xhci;
	struct usb_device *udev;
	struct usb_host_config *config;

	int err = 0;
	int i, class, vid, pid;

	mutex_lock(&uodev->dev_lock);
	if (!buf_dcbaa || !buf_ctx || !buf_seg) {
		USB_OFFLOAD_ERR("USB_OFFLOAD_NOT_READY yet!!!\n");
		mutex_unlock(&uodev->dev_lock);
		return -1;
	}

	if (!uodev->connected) {
		USB_OFFLOAD_ERR("No UAC Device Connected!!!\n");
		mutex_unlock(&uodev->dev_lock);
		return -1;
	}

	if (uodev->opened) {
		USB_OFFLOAD_ERR("USB Offload Already Opened!!!\n");
		err = usb_offload_cleanup();
		if (err)
			USB_OFFLOAD_ERR("Unable to notify ADSP.\n");
	}

	if (uodev->xhci == NULL) {
		USB_OFFLOAD_ERR("No 'xhci_host' node, NOT SUPPORT USB Offload!\n");
		err = -EINVAL;
		goto GET_OF_NODE_FAIL;
	}
	xhci = uodev->xhci;

	for (i = 0; i <= 2; i++) {
		if (xhci->devs[i] != NULL)
			if (xhci->devs[i]->udev != NULL) {
				USB_OFFLOAD_INFO("dev %d bDeviceClass: 0x%x\n",
						i, xhci->devs[i]->udev->descriptor.bDeviceClass);
				USB_OFFLOAD_INFO("dev %d idVendor: 0x%x\n",
						i, xhci->devs[i]->udev->descriptor.idVendor);
				USB_OFFLOAD_INFO("dev %d idProduct: 0x%x\n",
						i, xhci->devs[i]->udev->descriptor.idProduct);
			}
	}

	if (xhci->devs[2] != NULL) {
		USB_OFFLOAD_INFO("Multiple Devices - NOT SUPPORT USB OFFLOAD!!\n");
		mutex_unlock(&uodev->dev_lock);
		return -1;
	}

	if (xhci->devs[1] != NULL) {

		udev = xhci->devs[1]->udev;
		class = udev->descriptor.bDeviceClass;
		vid = udev->descriptor.idVendor;
		pid = udev->descriptor.idProduct;
		USB_OFFLOAD_INFO("Single Device - bDeviceClass: 0x%x, VID: 0x%x, PID: 0x%x\n",
				class, vid, pid);

		if ((class == 0x00 || class == 0xef) &&
			 udev->actconfig != NULL &&
			 udev->actconfig->interface[0] != NULL &&
			 udev->actconfig->interface[0]->cur_altsetting != NULL) {

			config = udev->actconfig;
			class = config->interface[0]->cur_altsetting->desc.bInterfaceClass;
			USB_OFFLOAD_INFO("Single Device - bInterfaceClass: 0x%x\n", class);

			if (class == 0x01) {
				if (check_usb_offload_quirk(vid, pid)) {
					mutex_unlock(&uodev->dev_lock);
					return -1;
				}

				if (check_is_multiple_ep(config)) {
					mutex_unlock(&uodev->dev_lock);
					return -1;
				}

				USB_OFFLOAD_INFO("Single UAC - SUPPORT USB OFFLOAD!!\n");
				uodev->opened = true;
				mutex_unlock(&uodev->dev_lock);
				return 0;
			}
		}
		USB_OFFLOAD_INFO("Single Device - Not UAC. NOT SUPPORT USB OFFLOAD!!\n");
	}
GET_OF_NODE_FAIL:
	mutex_unlock(&uodev->dev_lock);
	return -1;
}

static int usb_offload_release(struct inode *ip, struct file *fp)
{
	USB_OFFLOAD_INFO("%d\n", __LINE__);
	return usb_offload_cleanup();
}

static long usb_offload_ioctl(struct file *fp,
	unsigned int cmd, unsigned long value)
{
	long ret = 0;
	struct usb_audio_stream_info uainfo;
	struct mem_info_xhci *xhci_mem;
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
	int enable = 0, type = 0;
#endif

	switch (cmd) {
	case USB_OFFLOAD_INIT_ADSP:
		USB_OFFLOAD_INFO("USB_OFFLOAD_INIT_ADSP: %ld\n", value);

		if (uodev->adsp_inited && value == 1) {
			USB_OFFLOAD_ERR("ADSP ALREADY INITED!!!\n");
			ret = -EBUSY;
			goto fail;
		}

		if (!uodev->adsp_inited && value == 0) {
			USB_OFFLOAD_ERR("ADSP ALREADY DEINITED!!!\n");
			ret = -EBUSY;
			goto fail;
		}

		xhci_mem = kzalloc(sizeof(*xhci_mem), GFP_KERNEL);
		if (!xhci_mem) {
			USB_OFFLOAD_ERR("Fail to allocate xhci_mem\n");
			ret = -ENOMEM;
			goto fail;
		}

		/* Fiil in info of reserved region */
		if (value == 1) {
			init_fake_rsv_sram();
			xhci_mem->adv_lowpwr = uodev->adv_lowpwr;
			mtk_offload_get_rsv_mem_info(USB_OFFLOAD_MEM_DRAM_ID,
				&xhci_mem->xhci_dram_addr, &xhci_mem->xhci_dram_size);
			mtk_offload_get_rsv_mem_info(USB_OFFLOAD_MEM_SRAM_ID,
				&xhci_mem->xhci_sram_addr, &xhci_mem->xhci_sram_size);

			ret = xhci_mtk_alloc_event_ring(uodev);
			if (ret) {
				USB_OFFLOAD_ERR("error allocating event ring\n");
				kfree(xhci_mem);
				goto fail;
			}

			ret = xhci_mtk_alloc_erst(uodev);
			if (ret) {
				USB_OFFLOAD_ERR("error allocating erst\n");
				kfree(xhci_mem);
				goto fail;
			}

			if (sram_version == 0x3) {
				ret = xhci_initialize_ir(uodev);
				if (ret != 0) {
					USB_OFFLOAD_ERR("error initializing ir\n");
					kfree(xhci_mem);
					goto fail;
				}
			}

			xhci_mem->erst_table = (unsigned long long)uodev->erst->erst_dma_addr;
			xhci_mem->ev_ring = (unsigned long long)uodev->event_ring->first_seg->dma;

			USB_OFFLOAD_MEM_DBG("ev_ring:0x%llx erst_table:0x%llx\n",
			xhci_mem->ev_ring, xhci_mem->erst_table);
		} else {
			xhci_mem->adv_lowpwr = false;
			xhci_mem->xhci_dram_addr = 0;
			xhci_mem->xhci_dram_size = 0;
			xhci_mem->xhci_sram_addr = 0;
			xhci_mem->xhci_sram_size = 0;
			xhci_mem->erst_table = 0;
			xhci_mem->ev_ring = 0;
		}
		xhci_mem->sram_version = sram_version;

		USB_OFFLOAD_INFO("adsp_exception:%d, adsp_ready:%d\n",
				uodev->adsp_exception, uodev->adsp_ready);

		ret = send_init_ipi_msg_to_adsp(xhci_mem);
		print_all_memory();
		if (ret || (value == 0)) {
			uodev->is_streaming = false;
			uodev->tx_streaming = false;
			uodev->rx_streaming = false;
			uodev->adsp_inited = false;
		} else {
			uodev->adsp_inited = true;
			if (sram_version == 0x3) {
				xhci_backup_ir(uodev);
				fake_sram_pwr_ctrl(false);
			}
		}
		kfree(xhci_mem);
		usb_offload_register_ipi_recv();
		break;
	case USB_OFFLOAD_ENABLE_STREAM:
	case USB_OFFLOAD_DISABLE_STREAM:
		USB_OFFLOAD_INFO("%s\n",
			(cmd == USB_OFFLOAD_ENABLE_STREAM) ?
			"USB_OFFLOAD_ENABLE_STREAM":"USB_OFFLOAD_DISABLE_STREAM");

		if (!uodev->adsp_inited) {
			USB_OFFLOAD_ERR("ADSP NOT INITED YET!!!\n");
			ret = -EFAULT;
			goto fail;
		}

		if (copy_from_user(&uainfo, (void __user *)value, sizeof(uainfo))) {
			USB_OFFLOAD_ERR("copy_from_user ERR!!!\n");
			ret = -EFAULT;
			goto fail;
		}

		dump_uainfo(&uainfo);
		if (!is_uainfo_valid(&uainfo)) {
			USB_OFFLOAD_ERR("uainfo invalid!!!\n");
			ret = -EFAULT;
			goto fail;
		}

#ifdef CONFIG_USB_NOTIFY_PROC_LOG
		if (cmd == USB_OFFLOAD_ENABLE_STREAM)
			enable = 1;

		if (uainfo.direction == 0)
			type = NOTIFY_PCM_PLAYBACK;
		else
			type = NOTIFY_PCM_CAPTURE;

		store_usblog_notify(type, (void *)&enable, NULL);
#endif

		if (uainfo.enable && sram_version == 0x3) {
			fake_sram_pwr_ctrl(true);
			USB_OFFLOAD_MEM_DBG("ir_lost:%d\n", ir_lost);
			if (ir_lost) {
				/* non-1st time starting stream, but sram power-off before */
				ret = xhci_restore_ir(uodev);
				if (ret != 0) {
					USB_OFFLOAD_ERR("xhci_restore_ir fail\n");
					goto fail;
				}
			}
		}

		if (cmd == USB_OFFLOAD_ENABLE_STREAM) {
			switch (uainfo.direction) {
			case 0:
				if (uodev->tx_streaming) {
					ret = -EBUSY;
					USB_OFFLOAD_ERR("TX Stream Already ENABLE!!!\n");
					goto fail;
				}
				break;
			case 1:
				if (uodev->rx_streaming) {
					USB_OFFLOAD_ERR("RX Stream Already ENABLE!!!\n");
					ret = -EBUSY;
					goto fail;
				}
				break;
			}
		}

		if (cmd == USB_OFFLOAD_DISABLE_STREAM) {
			switch (uainfo.direction) {
			case 0:
				if (!uodev->tx_streaming) {
					USB_OFFLOAD_ERR("TX Stream Already DISABLE!!!\n");
					ret = -EBUSY;
					goto fail;
				}
				break;
			case 1:
				if (!uodev->rx_streaming) {
					USB_OFFLOAD_ERR("RX Stream Already DISABLE!!!\n");
					ret = -EBUSY;
					goto fail;
				}
				break;
			}
		}

		ret = usb_offload_enable_stream(&uainfo);
		print_all_memory();
		if (cmd == USB_OFFLOAD_ENABLE_STREAM && ret == 0) {
			switch (uainfo.direction) {
			case 0:
				uodev->tx_streaming = true;
				break;
			case 1:
				uodev->rx_streaming = true;
				break;
			}
		}

		if (cmd == USB_OFFLOAD_DISABLE_STREAM) {
			switch (uainfo.direction) {
			case 0:
				uodev->tx_streaming = false;
				break;
			case 1:
				uodev->rx_streaming = false;
				break;
			}
		}
		uodev->is_streaming = uodev->tx_streaming || uodev->rx_streaming;

		if (!uainfo.enable && !uodev->is_streaming && sram_version == 0x3) {
			/* power-off sram if no streaming */
			ret = xhci_backup_ir(uodev);
			if (ret != 0) {
				USB_OFFLOAD_ERR("xhci_backup_ir fail\n");
				goto fail;
			}
			fake_sram_pwr_ctrl(false);
		}

		if (uodev->is_streaming) {
			mtk_clk_notify(NULL, NULL, NULL, 1, 1, 0, CLK_EVT_BYPASS_PLL);
			USB_OFFLOAD_MEM_DBG("CLK_EVT_BYPASS_PLL 1 suspend\n");
		} else {
			mtk_clk_notify(NULL, NULL, NULL, 0, 1, 0, CLK_EVT_BYPASS_PLL);
			USB_OFFLOAD_MEM_DBG("CLK_EVT_BYPASS_PLL 0 suspend\n");
		}
		break;
	}

	USB_OFFLOAD_INFO("is_stream:%d, tx_stream:%d, rx_stream:%d, inited:%d, opened:%d\n",
			uodev->is_streaming, uodev->tx_streaming,
			uodev->rx_streaming, uodev->adsp_inited, uodev->opened);
fail:
	USB_OFFLOAD_INFO("ioctl returning, ret: %ld\n", ret);
	return ret;
}

static const char usb_offload_shortname[] = "mtk_usb_offload";

/* file operations for /dev/mtk_usb_offload */
static const struct file_operations usb_offload_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = usb_offload_ioctl,
	.open = usb_offload_open,
	.release = usb_offload_release,
};

static struct miscdevice usb_offload_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = usb_offload_shortname,
	.fops = &usb_offload_fops,
};

static struct xhci_vendor_ops xhci_mtk_vendor_ops = {
	.is_usb_offload_enabled = xhci_mtk_is_usb_offload_enabled,
	.alloc_dcbaa = xhci_mtk_alloc_dcbaa,
	.free_dcbaa = xhci_mtk_free_dcbaa,
	.alloc_container_ctx = xhci_mtk_alloc_container_ctx,
	.free_container_ctx = xhci_mtk_free_container_ctx,
	.alloc_transfer_ring = xhci_mtk_alloc_transfer_ring,
	.free_transfer_ring = xhci_mtk_free_transfer_ring,
	.is_streaming = xhci_mtk_is_streaming,
	.usb_offload_skip_urb = xhci_mtk_skip_hid_urb,
};

int xhci_mtk_ssusb_offload_get_mode(struct device *dev)
{
	bool is_in_advanced;

	if (!uodev->is_streaming)
		return SSUSB_OFFLOAD_MODE_NONE;

	is_in_advanced = mtk_offload_is_advlowpwr(uodev);
	USB_OFFLOAD_DBG("is_streaming:%d, is_in_advanced:%d\n",
		uodev->is_streaming, is_in_advanced);

	/* we only release APSRC request in advanced mode by
	 * notifying SSUSB_OFFLOAD_MODE_S to mtu3 driver
	 */
	return is_in_advanced ? SSUSB_OFFLOAD_MODE_S : SSUSB_OFFLOAD_MODE_D;
}

static void usb_offload_link_mtu3(struct device *uo_dev)
{
	struct device_node *node;
	struct device *mtu3_dev;
	struct device_link *link;
	struct platform_device *pdev = NULL;

	node = of_find_node_by_name(NULL, "usb0");
	if (node) {
		pdev = of_find_device_by_node(node);
		if (!pdev) {
			USB_OFFLOAD_ERR("no device found by ssusb node!\n");
			hid_disable_offload = 1;
			goto put_node;
		}

		mtu3_dev = &pdev->dev;

		link =  device_link_add(uo_dev, mtu3_dev,
						DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS);
		if (!link) {
			USB_OFFLOAD_ERR("fail to link mtu3\n");
			hid_disable_offload = 1;
		} else {
			USB_OFFLOAD_ERR("success to link mtu3\n");
			hid_disable_offload = 0;
		}
put_node:
		of_node_put(node);
	} else {
		USB_OFFLOAD_ERR("no 'usb0' node!\n");
		hid_disable_offload = 1;
	}
}

static int usb_offload_probe(struct platform_device *pdev)
{
	struct device_node *node_xhci_host;
	int ret = 0;
	int dsp_type;

	uodev = devm_kzalloc(&pdev->dev, sizeof(struct usb_offload_dev),
		GFP_KERNEL);
	if (!uodev) {
		USB_OFFLOAD_ERR("Fail to allocate usb_offload_dev\n");
		return -ENOMEM;
	}

	if (snd_vendor_set_ops(&alsa_ops))
		USB_OFFLOAD_ERR("fail registering ALSA OPS\n");

	uodev->dev = &pdev->dev;
	uodev->enable_adv_lowpwr =
		of_property_read_bool(pdev->dev.of_node, "adv-lowpower");
	uodev->adv_lowpwr = uodev->enable_adv_lowpwr;
	uodev->smc_ctrl = of_property_read_bool(pdev->dev.of_node, "smc-ctrl");
	uodev->smc_suspend = uodev->smc_ctrl ? OFFLOAD_SMC_AUD_SUSPEND : -1;
	uodev->smc_resume = uodev->smc_ctrl ? OFFLOAD_SMC_AUD_RESUME : -1;
	uodev->adv_lowpwr_dl_only =
		of_property_read_bool(pdev->dev.of_node, "adv-lowpower-dl-only");

	uodev->is_streaming = false;
	uodev->tx_streaming = false;
	uodev->rx_streaming = false;
	uodev->adsp_inited = false;
	uodev->connected = false;
	uodev->opened = false;
	uodev->xhci = NULL;

	buf_seg = NULL;
	buf_ev_table = NULL;
	buf_ev_ring = NULL;
	uodev->event_ring = NULL;
	uodev->erst = NULL;
	uodev->num_entries_in_use = 0;
	is_vcore_hold = false;
	uodev->backup_erst = NULL;
	uodev->backup_ev_ring = NULL;
	ir_lost = false;
	rsv_region = 0;
	if (of_property_read_u32(pdev->dev.of_node, "sram-version", &sram_version))
		sram_version = 0x2;

	USB_OFFLOAD_INFO("adv_lowpwr:%d smc_suspend:%d smc_resume:%d sram_version:%d\n",
		uodev->adv_lowpwr, uodev->smc_suspend, uodev->smc_resume, sram_version);

	node_xhci_host = of_parse_phandle(uodev->dev->of_node, "xhci-host", 0);
	if (node_xhci_host) {
		ret = mtk_offload_init_rsv_dram(MIN_USB_OFFLOAD_SHIFT);
		if (ret != 0)
			goto INIT_SHAREMEM_FAIL;

		/* init audio interface from ASOC*/
		if (uodev->adv_lowpwr && soc_init_aud_intf() != 0) {
			USB_OFFLOAD_INFO("not support advanced low power, adv_lowpwr:%d->0\n"
				, uodev->adv_lowpwr);
			uodev->adv_lowpwr = false;
		}

		ret = misc_register(&usb_offload_device);
		if (ret) {
			USB_OFFLOAD_ERR("Fail to allocate usb_offload_device\n");
			ret = -ENOMEM;
			goto INIT_MISC_DEV_FAIL;
		}

		uodev->ssusb_offload_notify = kzalloc(
					sizeof(*uodev->ssusb_offload_notify), GFP_KERNEL);
		if (!uodev->ssusb_offload_notify) {
			USB_OFFLOAD_ERR("Fail to alloc ssusb_offload_notify\n");
			ret = -ENOMEM;
			goto INIT_OFFLOAD_NOTIFY_FAIL;
		}
		uodev->ssusb_offload_notify->dev = uodev->dev;
		uodev->ssusb_offload_notify->get_mode = xhci_mtk_ssusb_offload_get_mode;
		ret = ssusb_offload_register(uodev->ssusb_offload_notify);
		if (ret) {
			USB_OFFLOAD_ERR("Fail to register ssusb_offload\n");
			ret = -ENOMEM;
			goto REG_SSUSB_OFFLOAD_FAIL;
		}
		mutex_init(&uodev->dev_lock);

		USB_OFFLOAD_INFO("Set XHCI vendor hook ops\n");
		platform_set_drvdata(pdev, &xhci_mtk_vendor_ops);
		dsp_type = get_adsp_type();
		if (dsp_type == ADSP_TYPE_HIFI3) {
#ifdef CFG_RECOVERY_SUPPORT
			adsp_register_notify(&adsp_usb_offload_notifier);
#else
			USB_OFFLOAD_ERR("Do not register notifier. Recovery not enabled\n");
#endif
		} else if (dsp_type == ADSP_TYPE_RV55) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
			scp_A_register_notify(&scp_usb_offload_notifier);
#else
			USB_OFFLOAD_ERR("Do not register notifier. SCP not enabled\n");
#endif
		} else {
			USB_OFFLOAD_ERR("Unknown dsp type: %d\n", dsp_type);
			ret = -EINVAL;
			goto GET_DSP_TYPE_FAIL;
		}
		ret = sound_usb_trace_init();
		if (ret != 0) {
			USB_OFFLOAD_ERR("Fail to register offload_ops\n");
			goto REG_SSUSB_OFFLOAD_FAIL;
		}

		usb_offload_link_mtu3(&pdev->dev);

	} else {
		USB_OFFLOAD_ERR("No 'xhci_host' node, NOT support USB_OFFLOAD\n");
		ret = -ENODEV;
		goto INIT_SHAREMEM_FAIL;
	}

	buf_seg = kzalloc(sizeof(struct usb_offload_buffer) * BUF_SEG_SIZE, GFP_KERNEL);
	buf_ev_table = kzalloc(sizeof(struct usb_offload_buffer), GFP_KERNEL);
	buf_ev_ring = kzalloc(sizeof(struct usb_offload_buffer), GFP_KERNEL);

	usb_offload_hid_probe();

	USB_OFFLOAD_INFO("Probe Success!!!");
	return ret;
GET_DSP_TYPE_FAIL:
REG_SSUSB_OFFLOAD_FAIL:
	kfree(uodev->ssusb_offload_notify);
INIT_OFFLOAD_NOTIFY_FAIL:
	misc_deregister(&usb_offload_device);
INIT_MISC_DEV_FAIL:
INIT_SHAREMEM_FAIL:
	of_node_put(node_xhci_host);
	USB_OFFLOAD_ERR("Probe Fail!!!");
	return ret;
}

static int usb_offload_remove(struct platform_device *pdev)
{
	int ret;

	USB_OFFLOAD_INFO("\n");
	ret = ssusb_offload_unregister(uodev->ssusb_offload_notify->dev);

	kfree(buf_ev_table);
	buf_ev_table = NULL;

	kfree(buf_ev_ring);
	buf_ev_ring = NULL;

	kfree(buf_seg);
	buf_seg = NULL;

	return 0;
}

static int usb_offload_smc_ctrl(int smc_req)
{
	struct arm_smccc_res res;

	USB_OFFLOAD_INFO("smc_req:%d\n", smc_req);
	if (smc_req != -1)
		arm_smccc_smc(MTK_SIP_KERNEL_USB_CONTROL,
			smc_req, 0, 0, 0, 0, 0, 0, &res);

	return 0;
}

static int __maybe_unused usb_offload_suspend(struct device *dev)
{
	if (!uodev->connected)
		return 0;

	if (!uodev->is_streaming) {
		/* turn rsv_sram off if it's not streaming */
		if (sram_version != 0x3)
			fake_sram_pwr_ctrl(false);
		return 0;
	}

	usb_offload_hid_start();

	/* if it's streaming, call to TFA if it's required */
	return usb_offload_smc_ctrl(uodev->smc_suspend);
}

static int __maybe_unused usb_offload_resume(struct device *dev)
{
	if (!uodev->connected)
		return 0;

	if (!uodev->is_streaming) {
		if (sram_version != 0x3)
			fake_sram_pwr_ctrl(true);
		return 0;
	}

	usb_offload_hid_finish();

	return usb_offload_smc_ctrl(uodev->smc_resume);
}

static int __maybe_unused usb_offload_runtime_suspend(struct device *dev)
{
	if (!device_may_wakeup(dev))
		return 0;

	if (!uodev->connected)
		return 0;

	if (!uodev->is_streaming) {
		if (sram_version != 0x3)
			fake_sram_pwr_ctrl(false);
		return 0;
	}

	usb_offload_hid_start();

	return usb_offload_smc_ctrl(uodev->smc_suspend);
}

static int __maybe_unused usb_offload_runtime_resume(struct device *dev)
{
	if (!device_may_wakeup(dev))
		return 0;

	if (!uodev->connected)
		return 0;

	if (!uodev->is_streaming) {
		if (sram_version != 0x3)
			fake_sram_pwr_ctrl(true);
		return 0;
	}

	usb_offload_hid_finish();

	return usb_offload_smc_ctrl(uodev->smc_resume);
}

static const struct dev_pm_ops usb_offload_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(usb_offload_suspend, usb_offload_resume)
	SET_RUNTIME_PM_OPS(usb_offload_runtime_suspend,
					usb_offload_runtime_resume, NULL)
};

#define DEV_PM_OPS (IS_ENABLED(CONFIG_PM) ? &usb_offload_pm_ops : NULL)

static const struct of_device_id usb_offload_of_match[] = {
	{.compatible = "mediatek,usb-offload",},
	{},
};

MODULE_DEVICE_TABLE(of, usb_offload_of_match);

static struct platform_driver usb_offload_driver = {
	.probe = usb_offload_probe,
	.remove = usb_offload_remove,
	.driver = {
		.name = "mtk-usb-offload",
		.pm = DEV_PM_OPS,
		.of_match_table = of_match_ptr(usb_offload_of_match),
	},
};
module_platform_driver(usb_offload_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek USB Offload Driver");
