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

#if IS_ENABLED(CONFIG_MTK_USB_OFFLOAD)
struct xhci_plat_priv_overwrite {
	struct xhci_vendor_ops *vendor_ops;
};

int xhci_plat_register_vendor_ops(struct xhci_vendor_ops *vendor_ops);
#endif
#endif	/* _XHCI_PLAT_H */
