// SPDX-License-Identifier: GPL-2.0

/*
 * xHCI host controller exynos driver
 *
 *
 */

#include "xhci.h"
#include "../dwc3/dwc3-exynos.h"

/*
 * Sometimes deadlock occurred between hub_event and remove_hcd.
 * In order to prevent it, waiting for completion of hub_event was added.
 * This is a timeout (300msec) value for the waiting.
 */
#define XHCI_HUB_EVENT_TIMEOUT	(300)

#define XHCI_L2_SUPPORT			BIT_ULL(63)

#define PORTSC_OFFSET		0x430

enum usb_port_state {
	PORT_EMPTY = 0,		/* OTG only */
	PORT_USB2,		/* usb 2.0 device only */
	PORT_USB3,		/* usb 3.0 device only */
	PORT_HUB,		/* usb hub single */
	PORT_DP			/* DP device */
};

#define AUDIO_MODE_NORMAL		0
#define AUDIO_MODE_RINGTONE		1
#define AUDIO_MODE_IN_CALL		2
#define AUDIO_MODE_IN_COMMUNICATION	3
#define AUDIO_MODE_CALL_SCREEN		4

#define BUS_SUSPEND	1
#define BUS_RESUME	0

struct xhci_hcd_exynos {
	struct device *dev;
	struct usb_hcd *hcd;
	struct usb_hcd *shared_hcd;

	void __iomem *usb3_portsc;
	spinlock_t xhcioff_lock;
	int port_off_done;
	int port_set_delayed;
	u32 portsc_control_priority;
	enum usb_port_state port_state;
	int usb3_phy_control;

	struct usb_xhci_pre_alloc *xhci_alloc;
	struct phy *phy_usb2;
	struct phy *phy_usb3;

	int dp_use;
};

struct usb_phy_roothub {
	struct phy *phy;
	struct list_head list;
};

struct xhci_exynos_priv {
	const char *firmware_name;
	unsigned long long quirks;
	struct xhci_vendor_data *vendor_data;
	int (*plat_setup) (struct usb_hcd *);
	void (*plat_start) (struct usb_hcd *);
	int (*init_quirk) (struct usb_hcd *);
	int (*suspend_quirk) (struct usb_hcd *);
	int (*resume_quirk) (struct usb_hcd *);
	struct xhci_hcd_exynos *xhci_exynos;
};

#define hcd_to_xhci_exynos_priv(h) ((struct xhci_exynos_priv *)hcd_to_xhci(h)->priv)
#define xhci_to_exynos_priv(x) ((struct xhci_exynos_priv *)(x)->priv)

/* xhci_exynos suspend/resume state, suspend = 1, resume = 0 */
extern int xhci_exynos_pm_state;
extern u32 usb_user_scenario;
extern int is_otg_only;
extern struct wakeup_source *main_hcd_wakelock; /* Wakelock for HS HCD */
extern struct wakeup_source *shared_hcd_wakelock; /* Wakelock for SS HCD */

extern int exynos_usbdrd_phy_vendor_set(struct phy *phy, int is_enable, int is_cancel);
extern int get_idle_ip_index(void);

int xhci_hub_check_speed(struct usb_hcd *hcd);
int xhci_check_usbl2_support(struct usb_hcd *hcd);
int xhci_wake_lock(struct usb_hcd *hcd, int is_lock);
void xhci_usb_parse_endpoint(struct usb_device *udev, struct usb_endpoint_descriptor *desc, int size);
extern int xhci_exynos_wake_lock(int is_main_hcd, int is_lock);
extern int exynos_usbdrd_phy_vendor_set(struct phy *phy, int is_enable, int is_cancel);
extern int exynos_usbdrd_phy_tune(struct phy *phy, int phy_state);

void usb_power_notify_control(int owner, int on);
