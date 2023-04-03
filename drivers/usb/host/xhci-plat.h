// SPDX-License-Identifier: GPL-2.0
/*
 * xhci-plat.h - xHCI host controller driver platform Bus Glue.
 *
 * Copyright (C) 2015 Renesas Electronics Corporation
 */

#ifndef _XHCI_PLAT_H
#define _XHCI_PLAT_H

#include "xhci.h"	/* for hcd_to_xhci() */

struct xhci_plat_priv {
	const char *firmware_name;
	void (*plat_start)(struct usb_hcd *);
	int (*init_quirk)(struct usb_hcd *);
	int (*resume_quirk)(struct usb_hcd *);
};

#define hcd_to_xhci_priv(h) ((struct xhci_plat_priv *)hcd_to_xhci(h)->priv)

struct usb_xhci_pre_alloc {
	u8 *pre_dma_alloc;
	u64 offset;

	dma_addr_t	dma;
};

extern struct usb_xhci_pre_alloc xhci_pre_alloc;
extern void __iomem *phycon_base_addr;

#if defined(CONFIG_USB_DWC3_EXYNOS)
int xhci_soc_config_after_reset(struct xhci_hcd *xhci);
#endif

#endif	/* _XHCI_PLAT_H */
