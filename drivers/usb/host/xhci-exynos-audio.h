// SPDX-License-Identifier: GPL-2.0

/*
 * xHCI host controller exynos driver
 *
 *
 */

#include "xhci.h"

/*
 * Sometimes deadlock occurred between hub_event and remove_hcd.
 * In order to prevent it, waiting for completion of hub_event was added.
 * This is a timeout (300msec) value for the waiting.
 */
#define XHCI_HUB_EVENT_TIMEOUT	(300)

#define XHCI_USE_URAM_FOR_EXYNOS_AUDIO        BIT_ULL(62)

#ifdef CONFIG_SND_EXYNOS_USB_AUDIO_GIC
/* EXYNOS uram memory map */
#if defined(CONFIG_SOC_S5E9935)
#define EXYNOS_URAM_ABOX_EVT_RING_ADDR 0x02a00000
#define EXYNOS_URAM_ISOC_OUT_RING_ADDR 0x02a01000
#define EXYNOS_URAM_ISOC_IN_RING_ADDR  0x02a02000
#define EXYNOS_URAM_DEVICE_CTX_ADDR    0x02a03000
#define EXYNOS_URAM_DCBAA_ADDR         0x02a03880
#define EXYNOS_URAM_ABOX_ERST_SEG_ADDR 0x02a03C80
#elif defined(CONFIG_SOC_S5E9925)
#define EXYNOS_URAM_ABOX_EVT_RING_ADDR 0x02a00000
#define EXYNOS_URAM_ISOC_OUT_RING_ADDR 0x02a01000
#define EXYNOS_URAM_ISOC_IN_RING_ADDR  0x02a02000
#define EXYNOS_URAM_DEVICE_CTX_ADDR    0x02a03000
#define EXYNOS_URAM_DCBAA_ADDR         0x02a03880
#define EXYNOS_URAM_ABOX_ERST_SEG_ADDR 0x02a03C80
#elif defined(CONFIG_SOC_S5E8825)
#define EXYNOS_URAM_ABOX_EVT_RING_ADDR 0x13300000
#define EXYNOS_URAM_ISOC_OUT_RING_ADDR 0x13301000
#define EXYNOS_URAM_ISOC_IN_RING_ADDR  0x13302000
#define EXYNOS_URAM_DEVICE_CTX_ADDR    0x13303000
#define EXYNOS_URAM_DCBAA_ADDR         0x13303880
#define EXYNOS_URAM_ABOX_ERST_SEG_ADDR 0x13303C80
#else
#define EXYNOS_URAM_ABOX_EVT_RING_ADDR  0x13300000
#define EXYNOS_URAM_ISOC_OUT_RING_ADDR  0x13301000
#define EXYNOS_URAM_ISOC_IN_RING_ADDR   0x13302000
#define EXYNOS_URAM_DEVICE_CTX_ADDR     0x13303000
#define EXYNOS_URAM_DCBAA_ADDR          0x13303880
#define EXYNOS_URAM_ABOX_ERST_SEG_ADDR  0x13303C80
#endif
#endif


#ifdef CONFIG_SND_EXYNOS_USB_AUDIO_GIC
struct xhci_exynos_audio {
	struct phy *phy;
	struct usb_hcd *hcd;
	struct xhci_intr_reg __iomem *ir_set_audio;
	struct xhci_ring *event_ring_audio;
	struct xhci_erst erst_audio;
	dma_addr_t save_dma;
	void *save_addr;
	dma_addr_t out_dma;
	void *out_addr;
	dma_addr_t in_dma;
	void *in_addr;
	/* This flag is used to check first allocation for URAM */
	bool exynos_uram_ctx_alloc;
	bool exynos_uram_isoc_out_alloc;
	bool exynos_uram_isoc_in_alloc;
	u8 *usb_audio_ctx_addr;
	u8 *usb_audio_isoc_out_addr;
	u8 *usb_audio_isoc_in_addr;

};
#endif

#ifdef CONFIG_SND_EXYNOS_USB_AUDIO_GIC
int xhci_exynos_sync_dev_ctx(struct xhci_hcd *xhci, unsigned int slot_id);
void xhci_exynos_free_container_ctx(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx);
void xhci_exynos_alloc_container_ctx(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx, int type, gfp_t flags);
struct xhci_ring *xhci_exynos_alloc_transfer_ring(struct xhci_hcd *xhci,
						  u32 endpoint_type,
						  enum xhci_ring_type ring_type,
						  unsigned int max_packet, gfp_t mem_flags);
void xhci_exynos_free_transfer_ring(struct xhci_hcd *xhci, struct xhci_virt_device *virt_dev, unsigned int ep_index);
bool xhci_exynos_is_usb_offload_enabled(struct xhci_hcd *xhci,
					struct xhci_virt_device *virt_dev, unsigned int ep_index);
struct xhci_device_context_array *xhci_exynos_alloc_dcbaa(struct xhci_hcd *xhci, gfp_t flags);
void xhci_exynos_free_dcbaa(struct xhci_hcd *xhci);
int xhci_exynos_vendor_init(struct xhci_hcd *xhci);
void xhci_exynos_vendor_cleanup(struct xhci_hcd *xhci);
int xhci_exynos_audio_alloc(struct device *parent);
int xhci_exynos_audio_init(struct device *parent, struct platform_device *pdev);
int xhci_exynos_alloc_event_ring(struct xhci_hcd *xhci, gfp_t flags);
void xhci_exynos_enable_event_ring(struct xhci_hcd *xhci);
int xhci_store_hw_info(struct usb_hcd *hcd, struct usb_device *udev);
int xhci_set_deq(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx, unsigned int last_ep, struct usb_device *udev);
void xhci_remove_stream_mapping(struct xhci_ring *ring);
#endif
