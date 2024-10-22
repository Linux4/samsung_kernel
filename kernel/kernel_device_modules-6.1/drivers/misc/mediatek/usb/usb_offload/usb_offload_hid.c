// SPDX-License-Identifier: GPL-2.0
/*
 * MTK USB Offload - HID Key Support
 * *
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Yu-chen.Liu <yu-chen.liu@mediatek.com>
 */

#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/bitops.h>
#include <linux/timekeeping.h>
#include "xhci.h"
#include "xhci-mtk.h"
#include "xhci-trace.h"
#include "usb_offload.h"
#include "audio_task_usb_msg_id.h"
#include "audio_task_manager.h"

static unsigned int hid_debug;
module_param(hid_debug, uint, 0644);
MODULE_PARM_DESC(hid_debug, "Enable/Disable HID Offload debug log");

unsigned int hid_disable_offload;
module_param(hid_disable_offload, uint, 0644);
MODULE_PARM_DESC(hid_disable_offload, "Disable HID Offload");

static unsigned int hid_disable_sync;
module_param(hid_disable_sync, uint, 0644);
MODULE_PARM_DESC(hid_disable_sync, "Disable Sync of HID");

unsigned int hid_direct_reset;
module_param(hid_direct_reset, uint, 0644);
MODULE_PARM_DESC(hid_direct_reset, "Direct Reset Ring");

#define hid_dbg(fmt, args...) do { \
		if (hid_debug > 0) \
			pr_info("HID, %s(%d) " fmt, __func__, __LINE__, ## args); \
	} while (0)
#define hid_info(fmt, args...) \
	pr_info("HID, %s(%d) " fmt, __func__, __LINE__, ## args)
#define hid_err(fmt, args...) \
	pr_info("HID ERROR, %s(%d) " fmt, __func__, __LINE__, ## args)

/* sync_flag bit defined*/
#define HID_NEED_OFFLOAD    0
#define HID_DSP_RUNNING     1
#define HID_AP_QUEUE        2
#define HID_DSP_ABNORMAL    3
#define HID_ON_RESETING     4

#define HID_EP_NAME_LEN  30
struct hid_ep_info {
	char name[HID_EP_NAME_LEN];
	struct usb_interface_descriptor intf_desc;
	struct usb_endpoint_descriptor ep_desc;
	struct urb *urb;
	int dir;
	unsigned int slot_id;
	unsigned int ep_id;
	struct delayed_work giveback_work;
	struct usb_offload_buffer buf_payload;

	/* current ring info reported from dsp*/
	union xhci_trb *cur_enqueue;
	unsigned int cycle_state;

	/* synchronization item */
	unsigned long sync_flag;
	unsigned int irq_total_cnt;
	struct list_head payload_list;
	spinlock_t lock;
	struct mutex ring_lock;

	/* wait ipi_msg from dsp */
	struct completion *wait_dsp;
};

/* store payload info from dsp */
struct dsp_payload {
	void *data;
	int actual_length;
	int status;
	struct list_head list;
};

#define UO_HID_EP_NUM   2
static struct hid_ep_info hid_ep[UO_HID_EP_NUM];
static struct workqueue_struct *giveback_wq;
static bool register_ipi_receiver;

static void xhci_mtk_trace_init(void);
static int sned_payload_to_adsp(unsigned int msg_id,
		void *payload, unsigned int length, int need_ack);

static void hid_giveback_urb(struct work_struct *work_struct);
static void hid_dump_ep(struct hid_ep_info *hid, const char *tag);
static void hid_dump_xhci(struct hid_ep_info *hid, const char *tag);
static void hid_clean_buf(bool is_in);

static int xhci_hid_ep_ctx_control(struct hid_ep_info *hid,
		struct xhci_ep_ctx *new_ctx, bool reconfigure);
static int xhci_hid_move_deq(struct hid_ep_info *hid,
	struct xhci_segment *new_seg, union xhci_trb *new_deq, int new_cycle);
static int xhci_stop_hid_ep(struct hid_ep_info *hid);
static int xhci_realloc_hid_ring(struct hid_ep_info *hid);
static struct xhci_ring *xhci_get_hid_tr_ring(struct hid_ep_info *hid);
static struct xhci_virt_device *xhci_get_hid_virt_dev(struct hid_ep_info *hid);
static struct xhci_ep_ctx *xhci_get_hid_ep_ctx(struct hid_ep_info *hid, bool in_ctx);
static int xhci_hid_move_enq(struct hid_ep_info *hid, union xhci_trb *new_enq, int new_cycle);
static dma_addr_t xhci_trb_to_dma(struct hid_ep_info *hid, void *target_virt);
static union xhci_trb *xhci_trb_to_virt(struct hid_ep_info *hid, dma_addr_t phy);
static int xhci_get_ep_state(struct xhci_ep_ctx *ctx);

static struct hid_ep_info *get_hid_ep(int dir)
{
	return dir ? &hid_ep[1] : &hid_ep[0];
}

static struct hid_ep_info *get_hid_ep_safe(int dir, int slot, int ep)
{
	struct hid_ep_info *hid;

	hid = dir ? &hid_ep[1] : &hid_ep[0];
	if ((hid->slot_id != slot) || (hid->ep_id != ep))
		return NULL;
	else
		return hid;
}

static void hid_ep_lock(struct hid_ep_info *hid, const char *tag)
{
	unsigned long flags = 0;

	if (hid_disable_sync)
		return;

	hid_dbg("%s wait lock\n", tag);
	spin_lock_irqsave(&hid->lock, flags);
	hid_dbg("%s hold lock\n", tag);
}
static void hid_ep_unlock(struct hid_ep_info *hid, const char *tag)
{
	unsigned long flags = 0;

	if (hid_disable_sync)
		return;

	spin_unlock_irqrestore(&hid->lock, flags);
	hid_dbg("%s release lock\n", tag);
}

static void hid_ring_lock(struct hid_ep_info *hid, const char *tag)
{
	if (hid_disable_sync)
		return;

	hid_dbg("%s wait ring\n", tag);
	mutex_lock(&hid->ring_lock);
	hid_dbg("%s hold ring\n", tag);
}

static void hid_ring_unlock(struct hid_ep_info *hid, const char *tag)
{
	if (hid_disable_sync)
		return;

	mutex_unlock(&hid->ring_lock);
	hid_dbg("%s release ring\n", tag);
}

static bool is_hid_urb(struct urb *urb, struct usb_interface_descriptor *intf_desc,
	struct usb_endpoint_descriptor *ep_desc)
{
	struct usb_host_config *actconfig = NULL;
	struct usb_host_endpoint *ep;
	struct usb_device *dev;
	struct usb_host_interface *intf;
	int intf_num, i, dir;
	bool found_hid_req = false;

	if (!urb || !urb->dev || urb->setup_packet)
		return found_hid_req;

	dev = urb->dev;
	ep = urb->ep;
	actconfig = dev->actconfig;

	if (!actconfig)
		return found_hid_req;

	dir = usb_endpoint_dir_in(&urb->ep->desc);

	intf_num = actconfig->desc.bNumInterfaces;
	for (i = 0; i < intf_num; i++) {
		if (!actconfig->interface[i]->cur_altsetting)
			continue;
		intf = actconfig->interface[i]->cur_altsetting;
		if (intf->desc.bInterfaceClass != USB_CLASS_HID)
			continue;
		if (intf->endpoint == ep) {
			memcpy(intf_desc, &intf->desc, sizeof(intf->desc));
			memcpy(ep_desc, &ep->desc, sizeof(ep->desc));
			found_hid_req = true;
			break;
		}
	}

	return found_hid_req;
}

static void hid_trace_dequeue(void *unused, struct urb *urb)
{
	struct hid_ep_info *hid;
	int dir, cnt;

	if (hid_disable_offload)
		return;

	if (!uodev->is_streaming) {
		hid_dbg("no streaming, no need offloading\n");
		return;
	}

	dir = usb_endpoint_dir_in(&urb->ep->desc);
	hid = get_hid_ep(dir);

	if (is_hid_urb(urb, &hid->intf_desc, &hid->ep_desc)) {
		hid_ep_lock(hid, "<HID Dequeue>");
		set_bit(HID_NEED_OFFLOAD, &hid->sync_flag);
		hid->urb = urb;
		hid->dir = dir;
		hid->slot_id = urb->dev->slot_id;
		hid->ep_id = xhci_get_endpoint_index_(&hid->ep_desc);
		cnt = snprintf(hid->name, HID_EP_NAME_LEN, "hid_ep(dir%d slot%d ep%d)",
			hid->dir, hid->slot_id, hid->ep_id);
		if (!cnt)
			hid_dbg("hid name might be weird\n");
		clear_bit(HID_DSP_ABNORMAL, &hid->sync_flag);
		hid_dump_ep(hid, "<HID Dequeue>");
		hid_ep_unlock(hid, "<HID Dequeue>");
	}
}

bool xhci_mtk_skip_hid_urb(struct xhci_hcd *xhci, struct urb *urb)
{
	struct usb_interface_descriptor intf_desc;
	struct usb_endpoint_descriptor ep_desc;
	struct hid_ep_info *hid;
	int dir;
	bool skip = false, giveback;

	if (hid_disable_offload)
		goto NOT_SKIP_URB;

	dir = usb_endpoint_dir_in(&urb->ep->desc);
	if (is_hid_urb(urb, &intf_desc, &ep_desc)) {
		hid = get_hid_ep_safe(dir, urb->dev->slot_id,
					xhci_get_endpoint_index_(&urb->ep->desc));
		giveback = false;
		if (unlikely(!hid))
			goto NOT_SKIP_URB;
		hid_ep_lock(hid, "<AP Enqueue>");
		if (!test_bit(HID_NEED_OFFLOAD, &hid->sync_flag)) {
			hid_ep_unlock(hid, "<AP Enqueue>");
			hid_dbg("%s do not skip\n", hid->name);
			goto NOT_SKIP_URB;
		}

		/* if ring was on resetting, don't let urb queued into XHC */
		if (test_bit(HID_ON_RESETING, &hid->sync_flag)) {
			hid_info("%s on resetting\n", hid->name);
			goto FORCE_SKIP;
		}

		if (!test_bit(HID_DSP_RUNNING, &hid->sync_flag) && list_empty(&hid->payload_list)) {
			hid_ep_unlock(hid, "<AP Enqueue>");
			hid_dbg("%s no need to skip\n", hid->name);
			goto NOT_SKIP_URB;
		}

FORCE_SKIP:
		set_bit(HID_AP_QUEUE, &hid->sync_flag);
		skip = true;
		giveback = !list_empty(&hid->payload_list);
		hid_dump_ep(hid, "<AP Enqueue>");
		hid_ep_unlock(hid, "<AP Enqueue>");

		if (giveback)
			queue_delayed_work(giveback_wq, &hid->giveback_work, msecs_to_jiffies(3));
	}
NOT_SKIP_URB:
	return skip;
}

static int hid_dsp_irq(struct hid_ep_info *hid, struct usb_offload_urb_complete *urb_complete)
{
	struct usb_offload_buffer *buf;
	struct dsp_payload *payload;
	bool ap_queue;
	int ret = 0;

	if (urb_complete->status != 0) {
		hid_err("unexpected status:%d\n", urb_complete->status);
		return 0;
	}

	buf = &hid->buf_payload;
	hid_ep_lock(hid, "<DSP IRQ>");
	hid->irq_total_cnt++;
	if (urb_complete->more_complete)
		set_bit(HID_DSP_RUNNING, &hid->sync_flag);
	else
		clear_bit(HID_DSP_RUNNING, &hid->sync_flag);
	ap_queue = test_bit(HID_AP_QUEUE, &hid->sync_flag);

	hid->cur_enqueue = xhci_trb_to_virt(hid, (dma_addr_t)urb_complete->cur_trb);
	hid->cycle_state = urb_complete->cycle_state;
	if (!hid->cur_enqueue) {
		set_bit(HID_DSP_ABNORMAL, &hid->sync_flag);
		hid_ep_unlock(hid, "<DSP IRQ>");
		hid_err("fail translating from phy:0x%llx is NULL\n", urb_complete->cur_trb);
		return -EPROTO;
	}

	/* new irq completed on dsp, add new payload to list */
	payload = kzalloc(sizeof(struct dsp_payload), GFP_ATOMIC);
	if (!payload) {
		hid_ep_unlock(hid, "<DSP IRQ>");
		hid_err("fail allocating payload\n");
		return -ENOMEM;
	}
	payload->actual_length = urb_complete->actual_length;

	if ((unsigned long long)buf->dma_addr == urb_complete->urb_start_addr) {
		payload->data = kzalloc(payload->actual_length, GFP_ATOMIC);
		if (!payload->data) {
			kfree(payload);
			return -ENOMEM;
		}
		memcpy(payload->data, (void *)buf->dma_area, payload->actual_length);
		payload->status = 0;
	} else {
		hid_err("buffer unmatch, phy:0x%llx\n", urb_complete->urb_start_addr);
		payload->data = NULL;
		payload->status = -EPROTO;
	}

	list_add_tail(&payload->list, &hid->payload_list);

	hid_ep_unlock(hid, "<DSP IRQ>");
	hid_dbg("payload:%p actual_length:%d status:%d\n", payload,
		payload->actual_length, payload->status);

	hid_dump_ep(hid, "<DSP IRQ>");
	hid_dump_xhci(hid, "<DSP IRQ>");

	/* if hid has submitted urb and we skipped it, giveback immediately now*/
	if (ap_queue)
		queue_delayed_work(giveback_wq, &hid->giveback_work, msecs_to_jiffies(0));
	return ret;
}

static void hid_giveback_urb(struct work_struct *work_struct)
{
	struct hid_ep_info *hid = container_of(work_struct, struct hid_ep_info, giveback_work.work);
	struct usb_hcd	*hcd = bus_to_hcd(hid->urb->dev->bus);
	struct dsp_payload *payload;
	struct urb *urb;
	bool dsp_abnormal;
	int status;

	hid_ep_lock(hid, "<Finish Giveback>");
	if (list_empty(&hid->payload_list)) {
		hid_err("payload_list is empty");
		goto error;
	}

	payload = list_first_entry(&hid->payload_list, struct dsp_payload, list);
	if (!payload) {
		hid_err("payload is NULL");
		goto error;
	}
	/* successfully fetch a payload from list */
	clear_bit(HID_AP_QUEUE, &hid->sync_flag);
	list_del(&payload->list);
	hid_dbg("payload:%p actual_length:%d status:%d\n", payload,
		payload->actual_length, payload->status);

	urb = hid->urb;
	/* unlinked=0 was set in usb_hcd_unlink_urb_to_ep which we did not call */
	urb->unlinked = 0;
	urb->actual_length = payload->actual_length;
	if (payload->actual_length && payload->data != NULL)
		memcpy(urb->transfer_buffer, payload->data, payload->actual_length);
	status = payload->status;

	if (!test_bit(HID_DSP_RUNNING, &hid->sync_flag) && list_empty(&hid->payload_list)) {
		dsp_abnormal = test_bit(HID_DSP_ABNORMAL, &hid->sync_flag);
		clear_bit(HID_NEED_OFFLOAD, &hid->sync_flag);
		hid->irq_total_cnt = 0;
		hid_ep_unlock(hid, "<Finish Giveback>");

		hid_info("complete a key-press action on dsp\n");
		hid_clean_buf(hid->dir);
		hid_ring_lock(hid, "<Finish Giveback>");
		xhci_stop_hid_ep(hid);
		if (!dsp_abnormal && xhci_hid_move_enq(hid, hid->cur_enqueue, hid->cycle_state)) {
			struct xhci_ring *ring;

			ring = xhci_get_hid_tr_ring(hid);
			if (unlikely(!ring))
				goto error;
			hid_info("%s ring was abnormal, reset whole ring\n", hid->name);
			xhci_stop_hid_ep(hid);
			xhci_initialize_ring_info_(ring, 1);
			hid->cur_enqueue = ring->enqueue;
			hid->cycle_state = ring->cycle_state;
			xhci_hid_move_deq(hid, ring->enq_seg, ring->enqueue, ring->cycle_state);
		}
		hid_ring_unlock(hid, "<Finish Giveback>");
	} else {
		hid_ep_unlock(hid, "<Finish Giveback>");
		hid_dump_xhci(hid, "<Finish Giveback>");
	}

	usb_hcd_giveback_urb(hcd, urb, status);
	hid_dump_ep(hid, "<Finish Giveback>");

	kfree(payload->data);
	kfree(payload);
error:
	return;
}

static void hid_update_ep_info(struct hid_ep_info *hid, struct usb_offload_xhci_ep *xhci_ep_info)
{
	hid->cur_enqueue = xhci_trb_to_virt(hid, (dma_addr_t)xhci_ep_info->cur_trb);
	hid->cycle_state = xhci_ep_info->cycle_state;

	if (hid->wait_dsp)
		complete(hid->wait_dsp);
}

/* audio_send_ipi_msg might sleep, do not call during holding spinlock */
static int usb_offload_prepare_send_urb_msg(struct hid_ep_info *hid, bool enable, int need_ack)
{
	struct usb_offload_urb_msg msg = {0};
	struct usb_offload_buffer *buf;
	int ret = 0, urb_size;
	enum usb_offload_mem_id type;
	struct xhci_ring *ring;

	type = uodev->adv_lowpwr ?
		USB_OFFLOAD_MEM_SRAM_ID : USB_OFFLOAD_MEM_DRAM_ID;

	if (!test_bit(HID_NEED_OFFLOAD, &hid->sync_flag)) {
		hid_err("hid:%p does not need offloading\n", hid);
		ret = 0;
		goto error;
	}

	ring = xhci_get_hid_tr_ring(hid);
	if (unlikely(!ring)) {
		ret = -EINVAL;
		goto error;
	}

	memcpy(&msg.intf_desc, &hid->intf_desc, sizeof(hid->intf_desc));
	memcpy(&msg.ep_desc, &hid->ep_desc, sizeof(hid->ep_desc));
	msg.slot_id = hid->slot_id;
	msg.enable = enable;
	msg.direction = (unsigned char)hid->dir;

	if (enable) {
		if (test_bit(HID_DSP_RUNNING, &hid->sync_flag)) {
			hid_info("hid eq has already been submitted on dsp size\n");
			ret = 0;
			goto error;
		}

		hid->cur_enqueue = ring->enqueue;

		buf = &hid->buf_payload;
		urb_size = (unsigned int)hid->urb->transfer_buffer_length;
		ret = mtk_offload_alloc_mem(buf, urb_size, USB_OFFLOAD_TRB_SEGMENT_SIZE,
					type, false);
		if (ret) {
			hid_err("%s fail allocate hid-offload urb\n", hid->name);
			goto error;
		}
		msg.urb_size = (unsigned int)hid->urb->transfer_buffer_length;
		msg.urb_start_addr = (unsigned long long)buf->dma_addr;
		msg.first_trb = (unsigned long long)ring->first_seg->dma;
		msg.cycle_state = (unsigned char)ring->cycle_state;
		hid_dump_xhci(hid, "<Start DSP>");
	} else {
		msg.urb_size = 0;
		msg.urb_start_addr = 0;
		msg.first_trb = 0;
		hid_dump_xhci(hid, "<End DSP>");
	}

	hid_info("[urb_msg] en:%d dir:%d slot:%d phy:0x%llx sz:%d 1st_trb:0x%llx\n",
		msg.enable,	msg.direction, msg.slot_id,
		msg.urb_start_addr,	msg.urb_size, msg.first_trb);

	ret = sned_payload_to_adsp(AUD_USB_MSG_A2D_ENABLE_HID, &msg, sizeof(msg), need_ack);
	if (!ret) {
		if (enable) {
			hid_dump_ep(hid, "<Start DSP>");
			set_bit(HID_DSP_RUNNING, &hid->sync_flag);
		} else {
			clear_bit(HID_DSP_RUNNING, &hid->sync_flag);
			hid_dump_ep(hid, "<End DSP>");
		}
	}
error:
	return ret;

}

static int xhci_hid_move_deq(struct hid_ep_info *hid,
	struct xhci_segment *new_seg, union xhci_trb *new_deq, int new_cycle)
{
	struct xhci_hcd *xhci = uodev->xhci;
	struct xhci_virt_device *virt_dev;
	struct xhci_virt_ep *ep;
	struct xhci_ring *ring;
	struct xhci_command *cmd;
	u32 trb_sct = 0;
	dma_addr_t addr;
	int ret, count = 0;

	virt_dev = xhci_get_hid_virt_dev(hid);
	if (unlikely(!virt_dev)) {
		hid_err("%s virt_dev is NULL\n", hid->name);
		return -EINVAL;
	}
	ep = &virt_dev->eps[hid->ep_id];

	ring = xhci_get_hid_tr_ring(hid);
	if (unlikely(!ring)) {
		hid_err("%s ring is NULL\n", hid->name);
		return -EINVAL;
	}

	addr = xhci_trb_virt_to_dma_(new_seg, new_deq);
	if (addr == 0) {
		hid_err("Can't find dma of new dequeue ptr\n");
		return -EINVAL;
	}

	if ((ep->ep_state & SET_DEQ_PENDING)) {
		hid_info("Set TR Deq already pending, don't submit for 0x%p\n", &addr);
		return -EBUSY;
	}

	cmd = xhci_alloc_command_(xhci, false, GFP_ATOMIC);
	if (!cmd) {
		hid_err("Can't alloc Set TR Deq cmd 0x%p\n", &addr);
		return -ENOMEM;
	}

	if (hid->urb->stream_id)
		trb_sct = SCT_FOR_TRB(SCT_PRI_TR);

	ret = xhci_vendor_queue_command_(xhci, cmd,
		lower_32_bits(addr) | trb_sct | new_cycle,
		upper_32_bits(addr), STREAM_ID_FOR_TRB(hid->urb->stream_id),
		SLOT_ID_FOR_TRB(hid->slot_id) | EP_ID_FOR_TRB(hid->ep_id) |
		TRB_TYPE(TRB_SET_DEQ), false);
	if (ret < 0) {
		xhci_free_command_(xhci, cmd);
		return ret;
	}
	ep->queued_deq_seg = new_seg;
	ep->queued_deq_ptr = new_deq;
	ep->ep_state |= SET_DEQ_PENDING;
	xhci_ring_cmd_db_(xhci);

	while ((ep->ep_state & SET_DEQ_PENDING) && count < 10000) {
		mdelay(1);
		count++;
	}

	if ((ep->ep_state & SET_DEQ_PENDING) && count == 10000)
		hid_err("timeout for waiting set_tr_dequeue cmd\n");
	else {
		hid_info("Successful Set TR Deq Ptr cmd, deq = 0x%llx\n",
			xhci_trb_to_dma(hid, new_deq));
		ring->dequeue = new_deq;
		ring->deq_seg = new_seg;
	}

	hid_dump_xhci(hid, "<Move Dequeue>");
	return 0;
}


static void hid_reset(struct hid_ep_info *hid)
{
	struct usb_hcd	*hcd = bus_to_hcd(hid->urb->dev->bus);
	struct xhci_ring *ring;
	int ret;

	ring = xhci_get_hid_tr_ring(hid);
	if (unlikely(!ring))
		return;

	/* inform dsp to stop hid offloading */
	hid->wait_dsp = kmalloc(sizeof(struct completion), GFP_ATOMIC);
	if (!hid->wait_dsp)
		return;
	init_completion(hid->wait_dsp);
	usb_offload_prepare_send_urb_msg(hid, false, AUDIO_IPI_MSG_BYPASS_ACK);

	/* wait dsp to send back last trb which was lastset queued */
	ret = wait_for_completion_timeout(hid->wait_dsp, msecs_to_jiffies(5000));
	kfree(hid->wait_dsp);
	hid->wait_dsp = NULL;

	hid_ring_lock(hid, "<HID Reset>");
	xhci_stop_hid_ep(hid);

	if (!ret || hid_direct_reset) {
		hid_info("reset whole ring, hid_direct_reset=%d\n", hid_direct_reset);
		xhci_initialize_ring_info_(ring, 1);
		hid->cur_enqueue = ring->enqueue;
		hid->cycle_state = ring->cycle_state;
	} else
		xhci_hid_move_enq(hid, hid->cur_enqueue, hid->cycle_state);

	xhci_hid_move_deq(hid, ring->enq_seg, ring->enqueue, ring->cycle_state);
	hid_ring_unlock(hid, "<HID Reset>");

	hid_clean_buf(hid->dir);
	clear_bit(HID_ON_RESETING, &hid->sync_flag);
	clear_bit(HID_NEED_OFFLOAD, &hid->sync_flag);
	hid->irq_total_cnt = 0;

	hid_dump_ep(hid, "<HID RESET>");

	/* directlly giveback if there's still urb requeset during resetting */
	if (test_bit(HID_AP_QUEUE, &hid->sync_flag)) {
		clear_bit(HID_AP_QUEUE, &hid->sync_flag);
		hid_dump_ep(hid, "<Force Giveback>");
		hid->urb->unlinked = 0;
		hid->urb->actual_length = 0;
		usb_hcd_giveback_urb(hcd, hid->urb, -EPERM);
	}

}

void usb_offload_ipi_recv(struct ipi_msg_t *ipi_msg)
{
	struct hid_ep_info *hid;
	int direction, slot, ep;

	if (!ipi_msg) {
		hid_err("%s null ipi_msg\n", __func__);
		return;
	}

	hid_dbg("msg_id:0x%x\n", ipi_msg->msg_id);

	switch (ipi_msg->msg_id) {
	case AUD_USB_MSG_D2A_XHCI_IRQ:
	{
		struct usb_offload_urb_complete urb_complete;

		if (ipi_msg->payload_size != sizeof(struct usb_offload_urb_complete)) {
			hid_err("wrong payload size, msg_id:0x%x payload_size:%d\n",
				ipi_msg->msg_id, ipi_msg->payload_size);
			goto error;
		} else
			memcpy(&urb_complete, (void *)ipi_msg->payload, sizeof(urb_complete));

		direction = urb_complete.direction;
		slot = urb_complete.slot_id;
		/* minus 1 to fit ap xhci view */
		ep = urb_complete.ep_id - 1;

		hid_info("[xhci irq] slot:%d ep:%d dir:%d urb:0x%llx act:%d more:%d sta:%d cur:0x%llx\n",
			slot, ep, direction, urb_complete.urb_start_addr, urb_complete.actual_length,
			urb_complete.more_complete,	urb_complete.status, urb_complete.cur_trb);

		hid = get_hid_ep_safe(direction, slot, ep);
		if (unlikely(!hid)) {
			hid_err("can't find hid, dir:%d slot:%d ep:%d\n", direction, slot, ep);
			break;
		}
		hid_dsp_irq(hid, &urb_complete);
		break;
	}
	case AUD_USB_MSG_D2A_XHCI_EP_INFO:
	{
		struct usb_offload_xhci_ep xhci_ep_info;

		if (ipi_msg->payload_size != sizeof(struct usb_offload_xhci_ep)) {
			hid_err("wrong payload size, msg_id:0x%x payload_size:%d\n",
				ipi_msg->msg_id, ipi_msg->payload_size);
			goto error;
		} else
			memcpy(&xhci_ep_info, (void *)ipi_msg->payload, sizeof(xhci_ep_info));

		direction = xhci_ep_info.direction;
		slot = xhci_ep_info.slot_id;
		/* minus 1 to fit ap xhci view */
		ep = xhci_ep_info.ep_id - 1;

		hid_info("[xhci_ep] slot:%d ep:%d dir:%d cur:0x%llx cycle:%d\n",
			slot, ep, direction, xhci_ep_info.cur_trb, xhci_ep_info.cycle_state);

		hid = get_hid_ep_safe(direction, slot, ep);
		if (unlikely(!hid)) {
			hid_err("can't find hid, dir:%d slot:%d ep:%d\n", direction, slot, ep);
			break;
		}
		hid_update_ep_info(hid, &xhci_ep_info);
		break;
	}
	default:
		hid_err("unknown msg_id:0x%x\n", ipi_msg->msg_id);
		break;
	}

error:
	return;
}

int usb_offload_hid_start(void)
{
	struct hid_ep_info *hid;
	struct usb_hcd *hcd;
	struct xhci_ep_ctx *out_ep_ctx;
	struct xhci_ring *ring;
	struct usb_offload_buffer *buf;
	bool need_offload;
	int i;

	if (hid_disable_offload)
		return 0;

	for (i = 0; i < UO_HID_EP_NUM; i++) {
		hid = &hid_ep[i];
		hid_ep_lock(hid, "<AP Suspend>");
		need_offload = test_bit(HID_NEED_OFFLOAD, &hid->sync_flag);
		if (need_offload) {
			hcd = bus_to_hcd(hid->urb->dev->bus);
			out_ep_ctx = xhci_get_hid_ep_ctx(hid, false);
			if (unlikely(!out_ep_ctx))
				continue;
			ring = xhci_get_hid_tr_ring(hid);
			if (unlikely(!ring))
				continue;

			buf = usb_offload_get_ring_buf(ring->first_seg->dma);
			hid_ep_unlock(hid, "<AP Suspend>");
			hid_ring_lock(hid, "<AP Suspend>");
			if (!buf) {
				hid_info("hid transfer ring isn't under managed\n");
				xhci_realloc_hid_ring(hid);
			} else
				xhci_hid_ep_ctx_control(hid, out_ep_ctx, true);
			hid_ring_unlock(hid, "<AP Suspend>");

			/* inform dsp to start hid offloading */
			usb_offload_prepare_send_urb_msg(hid, true, AUDIO_IPI_MSG_NEED_ACK);
		} else
			hid_ep_unlock(hid, "<AP Suspend>");
	}

	return 0;
}

void usb_offload_hid_finish(void)
{
	struct hid_ep_info *hid;
	struct timespec64 ref, cur;
	int i, irq_total;
	long timeout = 200000; /* 200ms */

	if (hid_disable_offload)
		return;

	for (i = 0; i < UO_HID_EP_NUM; i++) {
		hid = &hid_ep[i];
		if (test_bit(HID_NEED_OFFLOAD, &hid->sync_flag)) {
			ktime_get_ts64(&ref);
			cur = ref;
			while (!hid->irq_total_cnt && (cur.tv_nsec - ref.tv_nsec) < timeout) {
				mdelay(1);
				ktime_get_ts64(&cur);
			}

			hid_ep_lock(hid, "<AP Resume>");
			irq_total = hid->irq_total_cnt;
			if (!irq_total)
				set_bit(HID_ON_RESETING, &hid->sync_flag);
			hid_ep_unlock(hid, "<AP Resume>");

			if (!irq_total) {
				hid_dump_ep(&hid_ep[i], "<User Not Press>");
				hid_reset(hid);
			}
		}
	}
}

void usb_offload_hid_probe(void)
{
	struct hid_ep_info *hid;
	int i, cnt;

	giveback_wq = create_singlethread_workqueue("uo_hid_giveback");
	for (i = 0; i < UO_HID_EP_NUM; i++) {
		hid = &hid_ep[i];
		hid->dir = i;
		hid->wait_dsp = NULL;
		INIT_DELAYED_WORK(&hid->giveback_work, hid_giveback_urb);
		INIT_LIST_HEAD(&hid->payload_list);
		spin_lock_init(&hid->lock);
		mutex_init(&hid->ring_lock);
		cnt = snprintf(hid->name, HID_EP_NAME_LEN, "hid_ep(dir%d slot? ep?)",
			hid->dir);
		if (!cnt)
			hid_dbg("hid name might be weird\n");
	}

	xhci_mtk_trace_init();
}

static void hid_dump_ep(struct hid_ep_info *hid, const char *tag)
{
	struct dsp_payload *pos;
	int count = 0;

	list_for_each_entry(pos, &hid->payload_list, list) {
		count ++;
	}

	hid_info("%s %s need:%d dsp:%d queue:%d payload:%d irq:%d rst:%d\n",
		tag, hid->name,
		test_bit(HID_NEED_OFFLOAD, &hid->sync_flag),
		test_bit(HID_DSP_RUNNING, &hid->sync_flag),
		test_bit(HID_AP_QUEUE, &hid->sync_flag),
		count, hid->irq_total_cnt,
		test_bit(HID_ON_RESETING, &hid->sync_flag));
}

static void hid_clean_buf(bool is_in)
{
	struct hid_ep_info *hid;

	hid = get_hid_ep(is_in);
	if (!hid)
		return;

	if (mtk_offload_free_mem(&hid->buf_payload))
		hid_err("fail freeing hid buf, dir:%d\n", is_in);
}

static void hid_dump_xhci(struct hid_ep_info *hid, const char *tag)
{
	struct xhci_ring *ring;
	struct xhci_virt_device *virt_dev;
	struct xhci_ep_ctx *ep_ctx;

	virt_dev = xhci_get_hid_virt_dev(hid);
	if (unlikely(!virt_dev))
		return;
	ep_ctx = xhci_get_hid_ep_ctx(hid, false);
	if (unlikely(!ep_ctx))
		return;
	ring = xhci_get_hid_tr_ring(hid);
	if (unlikely(!ring))
		return;

	hid_info("%s %s [xhci_ep_ctx] deq:0x%llx\n", tag, hid->name, ep_ctx->deq);

	hid_info("%s %s [xhci_ring] enq:trb:0x%llx deq:trb:0x%llx cycle:%d\n",
		tag, hid->name,	xhci_trb_to_dma(hid, ring->enqueue),
		xhci_trb_to_dma(hid, ring->dequeue), ring->cycle_state);

	hid_info("%s %s [hid_ep_info] cur_enq:0x%llx cycle:%d\n",
		tag, hid->name,	xhci_trb_to_dma(hid, hid->cur_enqueue),	hid->cycle_state);
}

static int xhci_hid_ep_ctx_control(struct hid_ep_info *hid,
		struct xhci_ep_ctx *new_ctx, bool reconfigure)
{
	struct xhci_hcd *xhci = uodev->xhci;
	struct usb_hcd *hcd = xhci->main_hcd;
	struct xhci_virt_device *virt_dev;
	struct xhci_ep_ctx *in_ep_ctx;
	struct xhci_input_control_ctx *ctrl_ctx;
	unsigned int ep_id;
	int ret = 0;

	if (!new_ctx) {
		hid_err("%s virtual device is NULL\n", hid->name);
		ret = -EINVAL;
		goto error;
	}

	virt_dev = xhci_get_hid_virt_dev(hid);
	if (unlikely(!virt_dev)) {
		ret = -EINVAL;
		goto error;
	}
	ep_id = hid->ep_id;
	in_ep_ctx = xhci_get_hid_ep_ctx(hid, true);
	if (unlikely(!in_ep_ctx)) {
		ret = -1;
		goto error;
	}

	/* assign new_ctx to input ctx */
	in_ep_ctx->ep_info  = new_ctx->ep_info;
	in_ep_ctx->ep_info2 = new_ctx->ep_info2;
	in_ep_ctx->deq      = new_ctx->deq;
	in_ep_ctx->tx_info  = new_ctx->tx_info;
	if (xhci->quirks & XHCI_MTK_HOST) {
		in_ep_ctx->reserved[0] = new_ctx->reserved[0];
		in_ep_ctx->reserved[1] = new_ctx->reserved[1];
	}

	ctrl_ctx = (struct xhci_input_control_ctx *)virt_dev->in_ctx->bytes;
	ctrl_ctx->add_flags = cpu_to_le32(BIT(ep_id + 1));
	if (reconfigure) {
		ctrl_ctx->drop_flags = cpu_to_le32(BIT(ep_id + 1));
		ret = xhci_check_bandwidth_(hcd, virt_dev->udev);
	} else {
		/* Todo: maybe we need evaluate_context someday ? */
		ctrl_ctx->drop_flags = 0;
	}
error:
	return ret;
}

static int xhci_realloc_hid_ring(struct hid_ep_info *hid)
{
	return xhci_mtk_realloc_transfer_ring(hid->slot_id,
		hid->ep_id,	USB_OFFLOAD_MEM_DRAM_ID);
}

static int xhci_get_ep_state(struct xhci_ep_ctx *ctx)
{
	if (!ctx)
		return -EINVAL;

	return (ctx->ep_info & EP_STATE_MASK);
}

static dma_addr_t xhci_trb_to_dma(struct hid_ep_info *hid, void *virt)
{
	struct xhci_ring *ring;
	struct xhci_segment *seg;
	union xhci_trb *trb = (union xhci_trb *)virt;
	dma_addr_t phy;

	ring = xhci_get_hid_tr_ring(hid);
	if (unlikely(!ring))
		return 0;
	seg = ring->first_seg;
	do {
		/* if phy does not equal to 0, trb lies on this segment */
		phy = xhci_trb_virt_to_dma_(seg, trb);
		if (phy)
			return phy;
		seg = seg->next;
	} while(seg && seg != ring->first_seg);

	return 0;
}

static union xhci_trb *xhci_trb_to_virt(struct hid_ep_info *hid, dma_addr_t phy)
{
	struct xhci_ring *ring;
	struct xhci_segment *seg;
	dma_addr_t seg_start, seg_end;
	void *virt;

	ring = xhci_get_hid_tr_ring(hid);
	if (unlikely(!ring))
		return NULL;

	seg = ring->first_seg;
	do {
		virt = seg->trbs;
		seg_start = seg->dma;
		seg_end = seg->dma + (USB_OFFLOAD_TRBS_PER_SEGMENT * sizeof(union xhci_trb));
		if (phy >= seg_start && phy < seg_end)
			return (union xhci_trb *)(virt + (phy - seg_start));
		seg = seg->next;
	} while (seg && seg != ring->first_seg);

	return NULL;
}

static int xhci_hid_move_enq(struct hid_ep_info *hid, union xhci_trb *new_enq, int new_cycle)
{
	struct xhci_ring *ring;
	struct xhci_segment *seg;
	struct xhci_segment *old_enq_seg;
	union xhci_trb *old_enqueue;
	dma_addr_t phy;
	bool found = false;
	u32 old_cycle;
	int ret;

	hid_dbg("%s ep_state:%d\n", hid->name,
		xhci_get_ep_state(xhci_get_hid_ep_ctx(hid, false)));

	ring = xhci_get_hid_tr_ring(hid);
	if (unlikely(!ring)) {
		ret = -EINVAL;
		goto error;
	}
	old_enq_seg = ring->enq_seg;
	old_enqueue = ring->enqueue;
	old_cycle = ring->cycle_state;

	/* check if enq_seq also needed updated */
	seg = ring->enq_seg;
	do {
		phy = xhci_trb_virt_to_dma_(seg, new_enq);
		if (phy) {
			ring->enq_seg = seg;
			found = true;
			break;
		}
	} while (seg && seg != old_enq_seg);

	if (found) {
		ring->enqueue = new_enq;
		ring->cycle_state = new_cycle;
		ring->enq_seg = seg;
		hid_info("%s enq:0x%llx->0x%llx cycle:%d->%d\n",
			hid->name, xhci_trb_to_dma(hid, old_enqueue),
			xhci_trb_to_dma(hid, ring->enqueue),
			old_cycle, ring->cycle_state);
		hid_dump_xhci(hid, "<Move Enqueue>");
		ret = 0;
	} else {
		hid_err("%s fail updating enqueue pointer\n", hid->name);
		ret = -EINVAL;
		goto error;
	}

error:
	return ret;
}

static struct xhci_virt_device *xhci_get_hid_virt_dev(struct hid_ep_info *hid)
{
	return uodev->xhci->devs[hid->slot_id];
}

static struct xhci_ep_ctx *xhci_get_hid_ep_ctx(struct hid_ep_info *hid, bool in_ctx)
{
	struct xhci_hcd *xhci = uodev->xhci;
	struct xhci_virt_device *virt_dev;

	virt_dev = xhci_get_hid_virt_dev(hid);
	if (unlikely(!virt_dev))
		return NULL;

	if (in_ctx)
		return xhci_get_ep_ctx__(xhci, virt_dev->in_ctx, hid->ep_id);
	else
		return xhci_get_ep_ctx__(xhci, virt_dev->out_ctx, hid->ep_id);
}

static struct xhci_ring *xhci_get_hid_tr_ring(struct hid_ep_info *hid)
{
	struct xhci_hcd *xhci = uodev->xhci;
	struct xhci_virt_device *virt_dev;

	virt_dev = xhci->devs[hid->slot_id];
	if (unlikely(!virt_dev))
		return NULL;

	return virt_dev->eps[hid->ep_id].ring;
}

/* do not call it while holding spinlock, xhci_alloc_command_ might sleep */
static int xhci_stop_hid_ep(struct hid_ep_info *hid)
{
	struct xhci_hcd *xhci = uodev->xhci;
	struct xhci_command *command;
	struct xhci_ep_ctx *ctx;
	int ret = 0, pre_state;

	ctx = xhci_get_hid_ep_ctx(hid, false);
	pre_state = xhci_get_ep_state(ctx);

	/* if command->completion was allocated, set_tr_deq would not follow stop_endpoint */
	command = xhci_alloc_command_(xhci, true, GFP_ATOMIC);
	if (!command) {
		ret = -ENOMEM;
		goto error;
	}

	xhci_queue_stop_endpoint_(xhci, command, hid->slot_id, hid->ep_id, 0);
	xhci_ring_cmd_db_(xhci);

	/* wait stopping ep done */
	wait_for_completion(command->completion);

	switch (command->status) {
	case COMP_SUCCESS:
		hid_info("%s success stopping ep, ep_state:%d->%d\n",
			hid->name, pre_state, xhci_get_ep_state(ctx));
		break;
	default:
		hid_err("%s unexpected during stopping ep, status:%d\n",
			hid->name, command->status);
		break;
	}
	xhci_free_command_(uodev->xhci, command);
error:
	hid_dbg("ret:%d\n", ret);
	return ret;
}

static int sned_payload_to_adsp(unsigned int msg_id, void *payload,
	unsigned int length, int need_ack)
{
	int send_result = 0;
	struct ipi_msg_t ipi_msg;

	send_result = audio_send_ipi_msg(&ipi_msg, TASK_SCENE_USB_DL,
		AUDIO_IPI_LAYER_TO_DSP, AUDIO_IPI_DMA, need_ack,
		msg_id, length, 0, payload);

	if (send_result == 0)
		send_result = ipi_msg.param2;

	if (send_result != 0)
		hid_err("USB Offload urb IPI msg send fail\n");
	else
		hid_info("USB Offload urb ipi msg send succeed\n");

	return send_result;
}

void usb_offload_register_ipi_recv(void)
{
	if (hid_disable_offload)
		return;

	if (!register_ipi_receiver) {
		audio_task_register_callback(
			TASK_SCENE_USB_DL, usb_offload_ipi_recv);
		register_ipi_receiver = true;
	}
}

static void xhci_mtk_trace_init(void)
{
	WARN_ON(register_trace_xhci_urb_dequeue_(hid_trace_dequeue, NULL));
}
