// SPDX-License-Identifier: GPL-2.0
/*
 * xhci-plat.c - xHCI host controller driver platform Bus Glue.
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com
 * Author: Sebastian Andrzej Siewior <bigeasy@linutronix.de>
 *
 * A lot of code borrowed from the Linux xHCI driver.
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/usb/of.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>

#include <linux/dmapool.h>
#include <linux/dma-mapping.h>
#ifdef CONFIG_SND_EXYNOS_USB_AUDIO_GIC
#include "../../../sound/usb/exynos_usb_audio.h"
#endif

#include <linux/phy/phy.h>
#include "xhci.h"
#include "xhci-exynos-audio.h"

struct xhci_exynos_audio *g_xhci_exynos_audio;
#ifdef CONFIG_SND_EXYNOS_USB_AUDIO_GIC
//extern struct usb_xhci_pre_alloc xhci_pre_alloc;
extern struct hcd_hw_info *g_hwinfo;
extern int xhci_check_trb_in_td_math(struct xhci_hcd *xhci);
extern int xhci_address_device(struct usb_hcd *hcd, struct usb_device *udev);

void xhci_exynos_ring_free(struct xhci_hcd *xhci, struct xhci_ring *ring);

struct xhci_ring *xhci_ring_alloc_uram(struct xhci_hcd *xhci,
				       unsigned int num_segs,
				       unsigned int cycle_state,
				       enum xhci_ring_type type,
				       unsigned int max_packet, gfp_t flags, u32 endpoint_type);
u32 ext_ep_type = 0;
static void *dma_pre_alloc_coherent(struct xhci_hcd *xhci, size_t size, dma_addr_t * dma_handle, gfp_t gfp)
{
#ifdef CONFIG_SND_EXYNOS_USB_AUDIO_GIC
	return (void *)xhci;
#else
	struct usb_xhci_pre_alloc *xhci_alloc = g_xhci_exynos_audio->xhci_alloc;
	u64 align = size % PAGE_SIZE;
	u64 b_offset = xhci_alloc->offset;

	if (align)
		xhci_alloc->offset = xhci_alloc->offset + size + (PAGE_SIZE - align);
	else
		xhci_alloc->offset = xhci_alloc->offset + size;

	*dma_handle = xhci_alloc->dma + b_offset;

	return (void *)xhci_alloc->pre_dma_alloc + b_offset;
#endif
}

void xhci_exynos_enable_event_ring(struct xhci_hcd *xhci)
{
	u32 temp;
	u64 temp_64;

	temp_64 = xhci_read_64(xhci, &g_xhci_exynos_audio->ir_set_audio->erst_dequeue);
	temp_64 &= ~ERST_PTR_MASK;
	xhci_info(xhci, "ERST2 deq = 64'h%0lx", (unsigned long)temp_64);

	xhci_info(xhci, "// [USB Audio] Set the interrupt modulation register");
	temp = readl(&g_xhci_exynos_audio->ir_set_audio->irq_control);
	temp &= ~ER_IRQ_INTERVAL_MASK;
	/*
	 * the increment interval is 8 times as much as that defined
	 * in xHCI spec on MTK's controller
	 */
	temp |= (u32) ((xhci->quirks & XHCI_MTK_HOST) ? 20 : 160);
	writel(temp, &g_xhci_exynos_audio->ir_set_audio->irq_control);

	temp = readl(&g_xhci_exynos_audio->ir_set_audio->irq_pending);
	xhci_info(xhci,
		  "// [USB Audio] Enabling event ring interrupter %p by writing 0x%x to irq_pending",
		  g_xhci_exynos_audio->ir_set_audio, (unsigned int)ER_IRQ_ENABLE(temp));
	writel(ER_IRQ_ENABLE(temp), &g_xhci_exynos_audio->ir_set_audio->irq_pending);
}

void xhci_exynos_usb_offload_store_hw_info(struct xhci_hcd *xhci, struct usb_hcd *hcd, struct usb_device *udev)
{
	struct xhci_virt_device *virt_dev;
	struct xhci_erst_entry *entry = &g_xhci_exynos_audio->erst_audio.entries[0];

	virt_dev = xhci->devs[udev->slot_id];

	g_hwinfo->dcbaa_dma = xhci->dcbaa->dma;
	g_hwinfo->save_dma = g_xhci_exynos_audio->save_dma;
	g_hwinfo->cmd_ring = xhci->op_regs->cmd_ring;
	g_hwinfo->slot_id = udev->slot_id;
	g_hwinfo->in_dma = g_xhci_exynos_audio->in_dma;
	g_hwinfo->in_buf = g_xhci_exynos_audio->in_addr;
	g_hwinfo->out_dma = g_xhci_exynos_audio->out_dma;
	g_hwinfo->out_buf = g_xhci_exynos_audio->out_addr;
	g_hwinfo->in_ctx = virt_dev->in_ctx->dma;
	g_hwinfo->out_ctx = virt_dev->out_ctx->dma;
	g_hwinfo->erst_addr = entry->seg_addr;
	g_hwinfo->speed = udev->speed;
	g_hwinfo->phy = g_xhci_exynos_audio->phy;

	if (xhci->quirks & XHCI_USE_URAM_FOR_EXYNOS_AUDIO)
		g_hwinfo->use_uram = true;
	else
		g_hwinfo->use_uram = false;

	pr_info("<<< %s\n", __func__);
}

static void xhci_exynos_set_hc_event_deq_audio(struct xhci_hcd *xhci)
{
	u64 temp;
	dma_addr_t deq;

	deq =
	    xhci_trb_virt_to_dma(g_xhci_exynos_audio->event_ring_audio->deq_seg,
				 g_xhci_exynos_audio->event_ring_audio->dequeue);
	if (deq == 0 && !in_interrupt())
		xhci_warn(xhci, "WARN something wrong with SW event ring " "dequeue ptr.\n");
	/* Update HC event ring dequeue pointer */
	temp = xhci_read_64(xhci, &g_xhci_exynos_audio->ir_set_audio->erst_dequeue);
	temp &= ERST_PTR_MASK;
	/* Don't clear the EHB bit (which is RW1C) because
	 * there might be more events to service.
	 */
	temp &= ~ERST_EHB;
	xhci_info(xhci, "//[%s] Write event ring dequeue pointer = 0x%llx, "
		  "preserving EHB bit", __func__, ((u64) deq & (u64) ~ ERST_PTR_MASK) | temp);
	xhci_write_64(xhci, ((u64) deq & (u64) ~ ERST_PTR_MASK) | temp,
		      &g_xhci_exynos_audio->ir_set_audio->erst_dequeue);
}

void xhci_exynos_parse_endpoint(struct xhci_hcd *xhci, struct usb_device *udev,
				struct usb_endpoint_descriptor *desc, struct xhci_container_ctx *ctx)
{
	struct usb_endpoint_descriptor *d = desc;
	int size = 0x100;
	unsigned ep_index;

	g_hwinfo->rawdesc_length = size;
	ep_index = xhci_get_endpoint_index(d);

	pr_info("udev = 0x%8x, Ep = 0x%x, desc = 0x%8x\n", udev, d->bEndpointAddress, d);

	if ((d->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_ISOC) {
		if ((d->bmAttributes & USB_ENDPOINT_USAGE_MASK) == USB_ENDPOINT_USAGE_FEEDBACK) {
			/* Only Feedback endpoint(Not implict feedback data endpoint) */
			if (d->bEndpointAddress & USB_ENDPOINT_DIR_MASK) {
				g_hwinfo->fb_in_ep = d->bEndpointAddress;
				pr_info("Feedback IN ISO endpoint #0%x 0x%x\n", d->bEndpointAddress, d->bSynchAddress);
			} else {
				g_hwinfo->fb_out_ep = d->bEndpointAddress;
				pr_info("Feedback OUT ISO endpoint #0%x 0x%x\n", d->bEndpointAddress, d->bSynchAddress);
			}
		} else {
			/* Data Stream Endpoint only */
			if (d->bEndpointAddress & USB_ENDPOINT_DIR_MASK) {
				if (d->bEndpointAddress != g_hwinfo->fb_in_ep) {
					g_hwinfo->in_ep = d->bEndpointAddress;
					pr_info
					    (" This is IN ISO endpoint #0%x 0x%x\n",
					     d->bEndpointAddress, d->bSynchAddress);
				} else {
					pr_info("IN ISO endpoint is same with FB #0%x\n", d->bEndpointAddress);
				}

				if ((d->bLength > 7)
				    && (d->bSynchAddress != 0x0)) {
					g_hwinfo->fb_out_ep = d->bSynchAddress;
					pr_info
					    ("Feedback OUT ISO endpoint #0%x 0x%x\n",
					     d->bEndpointAddress, d->bSynchAddress);
				}
			} else {
				g_hwinfo->out_ep = d->bEndpointAddress;
				pr_info(" This is OUT ISO endpoint #0%x 0x%x\n", d->bEndpointAddress, d->bSynchAddress);

				if ((d->bLength > 7)
				    && (d->bSynchAddress != 0x0)) {
					g_hwinfo->fb_in_ep = d->bSynchAddress;
					pr_info
					    ("Feedback IN ISO endpoint #0%x 0x%x\n",
					     d->bEndpointAddress, d->bSynchAddress);
				}
			}
		}
	}

}

int xhci_exynos_alloc_event_ring(struct xhci_hcd *xhci, gfp_t flags)
{
	dma_addr_t dma;
	unsigned int val;
	u64 val_64;
	struct xhci_segment *seg;
	struct device *dev = xhci_to_hcd(xhci)->self.sysdev;

	g_xhci_exynos_audio->ir_set_audio = &xhci->run_regs->ir_set[1];
	g_xhci_exynos_audio->save_addr = dma_alloc_coherent(dev, sizeof(PAGE_SIZE), &dma, flags);
	g_xhci_exynos_audio->save_dma = dma;
	xhci_info(xhci, "// Save address = 0x%llx (DMA), %p (virt)",
		  (unsigned long long)g_xhci_exynos_audio->save_dma, g_xhci_exynos_audio->save_addr);

	if ((xhci->quirks & XHCI_USE_URAM_FOR_EXYNOS_AUDIO)) {
		/* for AUDIO erst */
		g_xhci_exynos_audio->event_ring_audio =
		    xhci_ring_alloc_uram(xhci, ERST_NUM_SEGS, 1, TYPE_EVENT, 0, flags, 0);
		if (!g_xhci_exynos_audio->event_ring_audio)
			goto fail;

		g_xhci_exynos_audio->erst_audio.entries =
		    ioremap(EXYNOS_URAM_ABOX_ERST_SEG_ADDR, sizeof(struct xhci_erst_entry) * ERST_NUM_SEGS);
		if (!g_xhci_exynos_audio->erst_audio.entries)
			goto fail;

		dma = EXYNOS_URAM_ABOX_ERST_SEG_ADDR;
		xhci_info(xhci, "ABOX audio ERST allocated at 0x%x", EXYNOS_URAM_ABOX_ERST_SEG_ADDR);
	} else {
		/* for AUDIO erst */
		g_xhci_exynos_audio->event_ring_audio = xhci_ring_alloc(xhci, ERST_NUM_SEGS, 1, TYPE_EVENT, 0, flags);
		if (!g_xhci_exynos_audio->event_ring_audio)
			goto fail;
		if (xhci_check_trb_in_td_math(xhci) < 0)
			goto fail;
		g_xhci_exynos_audio->erst_audio.entries =
		    dma_pre_alloc_coherent(xhci, sizeof(struct xhci_erst_entry) * ERST_NUM_SEGS, &dma, flags);
		if (!g_xhci_exynos_audio->erst_audio.entries)
			goto fail;
	}
	xhci_info(xhci, "// Allocated event ring segment table at 0x%llx", (unsigned long long)dma);

	memset(g_xhci_exynos_audio->erst_audio.entries, 0, sizeof(struct xhci_erst_entry) * ERST_NUM_SEGS);
	g_xhci_exynos_audio->erst_audio.num_entries = ERST_NUM_SEGS;
	g_xhci_exynos_audio->erst_audio.erst_dma_addr = dma;
	xhci_info(xhci,
		  "// Set ERST to 0; private num segs = %i, virt addr = %p, dma addr = 0x%llx",
		  xhci->erst.num_entries, xhci->erst.entries, (unsigned long long)xhci->erst.erst_dma_addr);

	/* set ring base address and size for each segment table entry */
	for (val = 0, seg = g_xhci_exynos_audio->event_ring_audio->first_seg; val < ERST_NUM_SEGS; val++) {
		struct xhci_erst_entry *entry = &g_xhci_exynos_audio->erst_audio.entries[val];

		entry->seg_addr = cpu_to_le64(seg->dma);
		entry->seg_size = cpu_to_le32(TRBS_PER_SEGMENT);
		entry->rsvd = 0;
		seg = seg->next;
	}

	/* set ERST count with the number of entries in the segment table */
	val = readl(&g_xhci_exynos_audio->ir_set_audio->erst_size);
	val &= ERST_SIZE_MASK;
	val |= ERST_NUM_SEGS;
	xhci_info(xhci, "// Write ERST size = %i to ir_set 0 (some bits preserved)", val);
	writel(val, &g_xhci_exynos_audio->ir_set_audio->erst_size);

	xhci_info(xhci, "// Set ERST entries to point to event ring.");
	/* set the segment table base address */
	xhci_info(xhci, "// Set ERST base address for ir_set 0 = 0x%llx",
		  (unsigned long long)g_xhci_exynos_audio->erst_audio.erst_dma_addr);
	val_64 = xhci_read_64(xhci, &g_xhci_exynos_audio->ir_set_audio->erst_base);
	val_64 &= ERST_PTR_MASK;
	val_64 |= (g_xhci_exynos_audio->erst_audio.erst_dma_addr & (u64) ~ ERST_PTR_MASK);
	xhci_write_64(xhci, val_64, &g_xhci_exynos_audio->ir_set_audio->erst_base);

	/* Set the event ring dequeue address */
	xhci_exynos_set_hc_event_deq_audio(xhci);
	xhci_info(xhci, "// Wrote ERST address to ir_set 1.");

	return 0;
 fail:
	return -1;
}

void xhci_exynos_free_container_ctx(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx)
{
	/* Ignore dma_pool_free if it is allocated from URAM */
	if (ctx->dma != EXYNOS_URAM_DEVICE_CTX_ADDR)
		dma_pool_free(xhci->device_pool, ctx->bytes, ctx->dma);
}

void xhci_exynos_alloc_container_ctx(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx, int type, gfp_t flags)
{
	if (type != XHCI_CTX_TYPE_INPUT
	    && g_xhci_exynos_audio->exynos_uram_ctx_alloc == 0 && xhci->quirks & XHCI_USE_URAM_FOR_EXYNOS_AUDIO) {
		/* Only first Device Context uses URAM */
		int i;
		ctx->bytes = ioremap(EXYNOS_URAM_DEVICE_CTX_ADDR, 2112);
		if (!ctx->bytes)
			return;

		for (i = 0; i < 2112; i++)
			ctx->bytes[i] = 0;

		ctx->dma = EXYNOS_URAM_DEVICE_CTX_ADDR;
		g_xhci_exynos_audio->usb_audio_ctx_addr = ctx->bytes;
		g_xhci_exynos_audio->exynos_uram_ctx_alloc = 1;
		xhci_info(xhci, "First device context allocated at URAM(%x)", EXYNOS_URAM_DEVICE_CTX_ADDR);
	} else {
		ctx->bytes = dma_pool_zalloc(xhci->device_pool, flags, &ctx->dma);
		if (!ctx->bytes)
			return;
	}
}

struct xhci_ring *xhci_exynos_alloc_transfer_ring(struct xhci_hcd *xhci,
						  u32 endpoint_type,
						  enum xhci_ring_type ring_type,
						  unsigned int max_packet, gfp_t mem_flags)
{
	if (xhci->quirks & XHCI_USE_URAM_FOR_EXYNOS_AUDIO) {
		/* If URAM is not allocated, it try to allocate from URAM */
		if (g_xhci_exynos_audio->exynos_uram_isoc_out_alloc == 0 && endpoint_type == ISOC_OUT_EP) {
			xhci_info(xhci, "First ISOC OUT ring is allocated from URAM.\n");
			return xhci_ring_alloc_uram(xhci, 1, 1, ring_type, max_packet, mem_flags, endpoint_type);

			g_xhci_exynos_audio->exynos_uram_isoc_out_alloc = 1;
		} else if (g_xhci_exynos_audio->exynos_uram_isoc_in_alloc == 0
			   && endpoint_type == ISOC_IN_EP && EXYNOS_URAM_ISOC_IN_RING_ADDR != 0x0) {
			xhci_info(xhci, "First ISOC IN ring is allocated from URAM.\n");
			return xhci_ring_alloc_uram(xhci, 1, 1, ring_type, max_packet, mem_flags, endpoint_type);

			g_xhci_exynos_audio->exynos_uram_isoc_in_alloc = 1;
		} else {
			return xhci_ring_alloc(xhci, 2, 1, ring_type, max_packet, mem_flags);
		}

	} else {
		return xhci_ring_alloc(xhci, 2, 1, ring_type, max_packet, mem_flags);
	}
}

void xhci_exynos_segment_free_skip(struct xhci_hcd *xhci, struct xhci_segment *seg)
{
	if (seg->trbs) {
		/* Check URAM address for memory free */
		if (seg->dma == EXYNOS_URAM_ABOX_EVT_RING_ADDR) {
			iounmap(seg->trbs);
		} else if (seg->dma == EXYNOS_URAM_ISOC_OUT_RING_ADDR) {
			g_xhci_exynos_audio->exynos_uram_isoc_out_alloc = 0;
			if (in_interrupt())
				g_xhci_exynos_audio->usb_audio_isoc_out_addr = (u8 *) seg->trbs;
			else
				iounmap(seg->trbs);
		} else if (seg->dma == EXYNOS_URAM_ISOC_IN_RING_ADDR) {
			g_xhci_exynos_audio->exynos_uram_isoc_in_alloc = 0;
			if (in_interrupt())
				g_xhci_exynos_audio->usb_audio_isoc_in_addr = (u8 *) seg->trbs;
			else
				iounmap(seg->trbs);
		} else
			dma_pool_free(xhci->segment_pool, seg->trbs, seg->dma);

		seg->trbs = NULL;
	}
	kfree(seg->bounce_buf);
	kfree(seg);
}

void xhci_exynos_free_segments_for_ring(struct xhci_hcd *xhci, struct xhci_segment *first)
{
	struct xhci_segment *seg;

	seg = first->next;

	if (!seg)
		xhci_err(xhci, "segment is null unexpectedly\n");

	while (seg != first) {
		struct xhci_segment *next = seg->next;
		xhci_exynos_segment_free_skip(xhci, seg);
		seg = next;
	}
	xhci_exynos_segment_free_skip(xhci, first);
}

void xhci_exynos_ring_free(struct xhci_hcd *xhci, struct xhci_ring *ring)
{
	if (!ring)
		return;

	//trace_xhci_ring_free(ring);

	if (ring->first_seg) {
		if (ring->type == TYPE_STREAM)
			xhci_remove_stream_mapping(ring);

		xhci_exynos_free_segments_for_ring(xhci, ring->first_seg);
	}

	kfree(ring);
}

int endpoint_ring_table[31];

void xhci_exynos_free_transfer_ring(struct xhci_hcd *xhci, struct xhci_virt_device *virt_dev, unsigned int ep_index)
{
	if (endpoint_ring_table[ep_index]) {
		pr_info("%s: endpoint %d has been already free-ing\n", __func__, ep_index);
	}
	if (xhci->quirks & BIT_ULL(60)) {
		pr_info("%s: endpoint %d free is requested from reset_bandwidth\n", __func__, ep_index);
		xhci_exynos_ring_free(xhci, virt_dev->eps[ep_index].new_ring);
		xhci->quirks &= ~(BIT_ULL(60));
		return;
	}
	endpoint_ring_table[ep_index] = 1;
	xhci_exynos_ring_free(xhci, virt_dev->eps[ep_index].ring);
	endpoint_ring_table[ep_index] = 0;

	return;
}

bool xhci_exynos_is_usb_offload_enabled(struct xhci_hcd * xhci,
					struct xhci_virt_device * virt_dev, unsigned int ep_index)
{
	return true;
}

struct xhci_device_context_array *xhci_exynos_alloc_dcbaa(struct xhci_hcd *xhci, gfp_t flags)
{
	if (xhci->quirks & XHCI_USE_URAM_FOR_EXYNOS_AUDIO) {
		int i;

		xhci_info(xhci, "DCBAA is allocated at 0x%x(URAM)", EXYNOS_URAM_DCBAA_ADDR);
		/* URAM allocation for DCBAA */
		xhci->dcbaa = ioremap(EXYNOS_URAM_DCBAA_ADDR, sizeof(*xhci->dcbaa));
		if (!xhci->dcbaa)
			return NULL;
		/* Clear DCBAA */
		for (i = 0; i < MAX_HC_SLOTS; i++)
			xhci->dcbaa->dev_context_ptrs[i] = 0x0;

		xhci->dcbaa->dma = EXYNOS_URAM_DCBAA_ADDR;
	} else
		xhci_info(xhci, "URAM qurik is not set! Please check it\n");

	return xhci->dcbaa;
}

void xhci_exynos_free_dcbaa(struct xhci_hcd *xhci)
{
	if (xhci->quirks & XHCI_USE_URAM_FOR_EXYNOS_AUDIO) {
		iounmap(xhci->dcbaa);
		if (g_xhci_exynos_audio->usb_audio_ctx_addr != NULL) {
			iounmap(g_xhci_exynos_audio->usb_audio_ctx_addr);
			g_xhci_exynos_audio->usb_audio_ctx_addr = NULL;
		}
		if (g_xhci_exynos_audio->usb_audio_isoc_out_addr != NULL) {
			iounmap(g_xhci_exynos_audio->usb_audio_isoc_out_addr);
			g_xhci_exynos_audio->usb_audio_isoc_out_addr = NULL;
		}
		if (g_xhci_exynos_audio->usb_audio_isoc_in_addr != NULL) {
			iounmap(g_xhci_exynos_audio->usb_audio_isoc_in_addr);
			g_xhci_exynos_audio->usb_audio_isoc_in_addr = NULL;
		}
	} else
		xhci->dcbaa = NULL;
}

/* URAM Allocation Functions */
extern void xhci_segment_free(struct xhci_hcd *xhci, struct xhci_segment *seg);
extern void xhci_link_segments(struct xhci_segment *prev,
			       struct xhci_segment *next, enum xhci_ring_type type, bool chain_links);
extern void xhci_initialize_ring_info(struct xhci_ring *ring, unsigned int cycle_state);

static struct xhci_segment *xhci_segment_alloc_uram(struct xhci_hcd *xhci,
						    unsigned int cycle_state, unsigned int max_packet, gfp_t flags)
{
	struct xhci_segment *seg;
	dma_addr_t dma;
	int i;
	struct device *dev = xhci_to_hcd(xhci)->self.sysdev;

	seg = kzalloc_node(sizeof(*seg), flags, dev_to_node(dev));
	if (!seg)
		return NULL;

	seg->trbs = ioremap(EXYNOS_URAM_ABOX_EVT_RING_ADDR, TRB_SEGMENT_SIZE);
	if (!seg->trbs)
		return NULL;

	dma = EXYNOS_URAM_ABOX_EVT_RING_ADDR;

	if (max_packet) {
		seg->bounce_buf = kzalloc_node(max_packet, flags, dev_to_node(dev));
		if (!seg->bounce_buf) {
			dma_pool_free(xhci->segment_pool, seg->trbs, dma);
			kfree(seg);
			return NULL;
		}
	}
	/* If the cycle state is 0, set the cycle bit to 1 for all the TRBs */
	if (cycle_state == 0) {
		for (i = 0; i < TRBS_PER_SEGMENT; i++)
			seg->trbs[i].link.control |= cpu_to_le32(TRB_CYCLE);
	}
	seg->dma = dma;
	xhci_info(xhci, "ABOX Event Ring is allocated at 0x%x", EXYNOS_URAM_ABOX_EVT_RING_ADDR);
	seg->next = NULL;

	return seg;
}

static struct xhci_segment *xhci_segment_alloc_uram_ep(struct xhci_hcd *xhci,
						       unsigned int cycle_state,
						       unsigned int max_packet,
						       gfp_t flags, int seg_num, u32 endpoint_type)
{
	struct xhci_segment *seg;
	dma_addr_t dma;
	int i;
	struct device *dev = xhci_to_hcd(xhci)->self.sysdev;

	seg = kzalloc_node(sizeof(*seg), flags, dev_to_node(dev));
	if (!seg)
		return NULL;

	if (seg_num != 0) {
		/* Support just one segment */
		xhci_err(xhci, "%s : Unexpected SEG NUMBER!\n", __func__);
		return NULL;
	}

	if (endpoint_type == ISOC_OUT_EP) {
		if (!g_hwinfo->feedback)
			seg->trbs = ioremap(EXYNOS_URAM_ISOC_OUT_RING_ADDR, TRB_SEGMENT_SIZE);
		else
			seg->trbs = dma_pool_zalloc(xhci->segment_pool, flags, &dma);
		if (!seg->trbs)
			return NULL;

		if (!g_hwinfo->feedback)
			dma = EXYNOS_URAM_ISOC_OUT_RING_ADDR;
		xhci_info(xhci, "First ISOC-OUT Ring is allocated at 0x%x", dma);
	} else if (endpoint_type == ISOC_IN_EP) {
		if (!g_hwinfo->feedback)
			seg->trbs = ioremap(EXYNOS_URAM_ISOC_IN_RING_ADDR, TRB_SEGMENT_SIZE);
		else
			seg->trbs = dma_pool_zalloc(xhci->segment_pool, flags, &dma);
		if (!seg->trbs)
			return NULL;

		if (!g_hwinfo->feedback)
			dma = EXYNOS_URAM_ISOC_IN_RING_ADDR;
		xhci_info(xhci, "First ISOC-IN Ring is allocated at 0x%x", dma);
	} else {
		xhci_err(xhci, "%s : Unexpected EP Type!\n", __func__);
		return NULL;
	}

	for (i = 0; i < 256; i++) {
		seg->trbs[i].link.segment_ptr = 0;
		seg->trbs[i].link.intr_target = 0;
		seg->trbs[i].link.control = 0;
	}

	if (max_packet) {
		seg->bounce_buf = kzalloc_node(max_packet, flags, dev_to_node(dev));
		if (!seg->bounce_buf) {
			dma_pool_free(xhci->segment_pool, seg->trbs, dma);
			kfree(seg);
			return NULL;
		}
	}
	/* If the cycle state is 0, set the cycle bit to 1 for all the TRBs */
	if (cycle_state == 0) {
		for (i = 0; i < TRBS_PER_SEGMENT; i++)
			seg->trbs[i].link.control |= cpu_to_le32(TRB_CYCLE);
	}
	seg->dma = dma;
	seg->next = NULL;

	return seg;
}

static int xhci_alloc_segments_for_ring_uram(struct xhci_hcd *xhci,
					     struct xhci_segment **first,
					     struct xhci_segment **last,
					     unsigned int num_segs,
					     unsigned int cycle_state,
					     enum xhci_ring_type type,
					     unsigned int max_packet, gfp_t flags, u32 endpoint_type)
{
	struct xhci_segment *prev;
	bool chain_links;

	/* Set chain bit for 0.95 hosts, and for isoc rings on AMD 0.96 host */
	chain_links = ! !(xhci_link_trb_quirk(xhci) || (type == TYPE_ISOC && (xhci->quirks & XHCI_AMD_0x96_HOST)));

	if (type == TYPE_ISOC) {
		prev = xhci_segment_alloc_uram_ep(xhci, cycle_state, max_packet, flags, 0, endpoint_type);
	} else if (type == TYPE_EVENT) {
		prev = xhci_segment_alloc_uram(xhci, cycle_state, max_packet, flags);
	} else {
		xhci_err(xhci, "Unexpected TYPE for URAM allocation!\n");
		return -ENOMEM;
	}

	if (!prev)
		return -ENOMEM;
	num_segs--;

	*first = prev;
	while (num_segs > 0) {
		struct xhci_segment *next = NULL;

		if (type == TYPE_ISOC) {
			prev = xhci_segment_alloc_uram_ep(xhci, cycle_state, max_packet, flags, 1, endpoint_type);
		} else if (type == TYPE_EVENT) {
			next = xhci_segment_alloc_uram(xhci, cycle_state, max_packet, flags);
		} else {
			xhci_err(xhci, "Unexpected TYPE for URAM alloc(multi)!\n");
			return -ENOMEM;
		}

		if (!next) {
			prev = *first;
			while (prev) {
				next = prev->next;
				xhci_segment_free(xhci, prev);
				prev = next;
			}
			return -ENOMEM;
		}
		xhci_link_segments(prev, next, type, chain_links);

		prev = next;
		num_segs--;
	}
	xhci_link_segments(prev, *first, type, chain_links);
	*last = prev;

	return 0;
}

struct xhci_ring *xhci_ring_alloc_uram(struct xhci_hcd *xhci,
				       unsigned int num_segs,
				       unsigned int cycle_state,
				       enum xhci_ring_type type,
				       unsigned int max_packet, gfp_t flags, u32 endpoint_type)
{
	struct xhci_ring *ring;
	int ret;
	struct device *dev = xhci_to_hcd(xhci)->self.sysdev;

	ring = kzalloc_node(sizeof(*ring), flags, dev_to_node(dev));
	if (!ring)
		return NULL;

	ring->num_segs = num_segs;
	ring->bounce_buf_len = max_packet;
	INIT_LIST_HEAD(&ring->td_list);
	ring->type = type;
	if (num_segs == 0)
		return ring;

	ret = xhci_alloc_segments_for_ring_uram(xhci, &ring->first_seg,
						&ring->last_seg, num_segs,
						cycle_state, type, max_packet, flags, endpoint_type);
	if (ret)
		goto fail;

	/* Only event ring does not use link TRB */
	if (type != TYPE_EVENT) {
		/* See section 4.9.2.1 and 6.4.4.1 */
		ring->last_seg->trbs[TRBS_PER_SEGMENT - 1].link.control |= cpu_to_le32(LINK_TOGGLE);
	}
	xhci_initialize_ring_info(ring, cycle_state);
	//trace_xhci_ring_alloc(ring);
	return ring;

 fail:
	kfree(ring);
	return NULL;
}

static unsigned int xhci_exynos_get_endpoint_address(unsigned int ep_index)
{
	unsigned int number = DIV_ROUND_UP(ep_index, 2);
	unsigned int direction = ep_index % 2 ? USB_DIR_OUT : USB_DIR_IN;
	return direction | number;
}

int xhci_exynos_sync_dev_ctx(struct xhci_hcd *xhci, unsigned int slot_id)
{
	struct xhci_virt_device *virt_dev;
	struct xhci_slot_ctx *slot_ctx;

	int i;
	int last_ep;
	int last_ep_ctx = 31;

	/* Use default hwinfo in case
	 * there are no audio devices occupied
	 */
	virt_dev = xhci->devs[slot_id];
	slot_ctx = xhci_get_slot_ctx(xhci, virt_dev->in_ctx);

	last_ep = LAST_CTX_TO_EP_NUM(le32_to_cpu(slot_ctx->dev_info));

	if (last_ep < 31)
		last_ep_ctx = last_ep + 1;
	for (i = 0; i < last_ep_ctx; ++i) {
		unsigned int epaddr = xhci_exynos_get_endpoint_address(i);
		struct xhci_ep_ctx *ep_ctx = xhci_get_ep_ctx(xhci, virt_dev->out_ctx, i);

		if (epaddr == g_hwinfo->in_ep) {
			pr_info("[%s] ep%d set in deq : %#08llx\n", __func__, i, ep_ctx->deq);
			g_hwinfo->old_in_deq = g_hwinfo->in_deq;
			g_hwinfo->in_deq = ep_ctx->deq;
		} else if (epaddr == g_hwinfo->out_ep) {
			pr_info("[%s] ep%d set out deq : %#08llx\n", __func__, i, ep_ctx->deq);
			g_hwinfo->old_out_deq = g_hwinfo->out_deq;
			g_hwinfo->out_deq = ep_ctx->deq;
		} else if (epaddr == g_hwinfo->fb_out_ep) {
			pr_info("[%s] ep%d set fb out deq : %#08llx\n", __func__, i, ep_ctx->deq);
			g_hwinfo->fb_old_out_deq = g_hwinfo->fb_out_deq;
			g_hwinfo->fb_out_deq = ep_ctx->deq;
		} else if (epaddr == g_hwinfo->fb_in_ep) {
			pr_info("[%s] ep%d set fb in deq : %#08llx\n", __func__, i, ep_ctx->deq);
			g_hwinfo->fb_old_in_deq = g_hwinfo->fb_in_deq;
			g_hwinfo->fb_in_deq = ep_ctx->deq;
		}
	}

	return 0;

}

int xhci_exynos_audio_init(struct device *parent, struct platform_device *pdev)
{
	int			ret;
	int			value;

	ret = of_property_read_u32(parent->of_node,
				"xhci_use_uram_for_audio", &value);
	if (ret == 0 && value == 1) {
		/*
		 * Check URAM address. At least the following address should
		 * be defined.(Otherwise, URAM feature will be disabled.)
		 */
		if (EXYNOS_URAM_DCBAA_ADDR == 0x0 ||
				EXYNOS_URAM_ABOX_ERST_SEG_ADDR == 0x0 ||
				EXYNOS_URAM_ABOX_EVT_RING_ADDR == 0x0 ||
				EXYNOS_URAM_DEVICE_CTX_ADDR == 0x0 ||
				EXYNOS_URAM_ISOC_OUT_RING_ADDR == 0x0) {
			dev_info(&pdev->dev,
				"Some URAM addresses are not defiend!\n");
			return -1;
		}
		dev_info(&pdev->dev, "Support URAM for USB audio.\n");
		//xhci->quirks |= XHCI_USE_URAM_FOR_EXYNOS_AUDIO;
		/* Initialization Default Value */
		dev_info(&pdev->dev, "Init g_xhci_exynos_audio, g_hwinfo.\n");
		g_xhci_exynos_audio->exynos_uram_ctx_alloc = false;
		g_xhci_exynos_audio->exynos_uram_isoc_out_alloc = false;
		g_xhci_exynos_audio->exynos_uram_isoc_in_alloc = false;
		g_xhci_exynos_audio->usb_audio_ctx_addr = NULL;
		g_xhci_exynos_audio->usb_audio_isoc_out_addr = NULL;
		g_xhci_exynos_audio->usb_audio_isoc_in_addr = NULL;

		memset(g_hwinfo, 0, sizeof(struct hcd_hw_info));
	} else {
		dev_err(&pdev->dev, "URAM is not used.\n");
	}

	ret = of_property_read_u32(parent->of_node,
			"usb_audio_offloading", &value);
	if (ret == 0 && value == 1) {
		ret = exynos_usb_audio_init(parent, pdev);
		if (ret) {
			dev_err(&pdev->dev, "USB Audio INIT fail\n");
			return -1;
		}
		dev_info(&pdev->dev, "USB Audio offloading is supported\n");
	} else
		dev_err(&pdev->dev, "No usb offloading, err = %d\n", ret);

	return 0;
}

int xhci_exynos_audio_alloc(struct device *parent)
{
	dma_addr_t	dma;

	dev_info(parent, "%s\n", __func__);

	g_xhci_exynos_audio = devm_kzalloc(parent, sizeof(struct xhci_exynos_audio), GFP_KERNEL);

	g_hwinfo = devm_kzalloc(parent, sizeof(struct hcd_hw_info), GFP_KERNEL);

	/* In data buf alloc */
	g_xhci_exynos_audio->in_addr = dma_alloc_coherent(parent,
			(PAGE_SIZE * 256), &dma, GFP_KERNEL);
	g_xhci_exynos_audio->in_dma = dma;
	dev_info(parent, "// IN Data address = 0x%llx (DMA), %p (virt)",
		(unsigned long long)g_xhci_exynos_audio->in_dma, g_xhci_exynos_audio->in_addr);

	/* Out data buf alloc */
	g_xhci_exynos_audio->out_addr = dma_alloc_coherent(parent,
			(PAGE_SIZE * 256), &dma, GFP_KERNEL);
	g_xhci_exynos_audio->out_dma = dma;
	dev_info(parent, "// OUT Data address = 0x%llx (DMA), %p (virt)",
		(unsigned long long)g_xhci_exynos_audio->out_dma, g_xhci_exynos_audio->out_addr);

	return 0;
}
#endif
