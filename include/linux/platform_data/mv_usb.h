/*
 * Copyright (C) 2011 Marvell International Ltd. All rights reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __MV_PLATFORM_USB_H
#define __MV_PLATFORM_USB_H

#include <linux/notifier.h>

enum pxa_ehci_type {
	EHCI_UNDEFINED = 0,
	PXA_U2OEHCI,	/* pxa 168, 9xx */
	PXA_SPH,	/* pxa 168, 9xx SPH */
	MMP3_HSIC,	/* mmp3 hsic */
	MMP3_FSIC,	/* mmp3 fsic */
};

enum usb_port_speed {
	USB_PORT_SPEED_FULL = 0,	/* full speed: 0x0 */
	USB_PORT_SPEED_LOW,		/* low speed: 0x1 */
	USB_PORT_SPEED_HIGH,		/* high speed: 0x2 */
	USB_PORT_SPEED_UNKNOWN,		/* unknown speed: 0x3 */
};

enum {
	MV_USB_MODE_OTG,
	MV_USB_MODE_HOST,
	MV_USB_MODE_DEVICE,
};

enum {
	VBUS_LOW	= 0,
	VBUS_HIGH	= 1 << 0,
};

enum charger_type {
	NULL_CHARGER	= 0,
	DEFAULT_CHARGER,
	VBUS_CHARGER,
	AC_CHARGER_STANDARD,
	AC_CHARGER_OTHER,
};

/* for usb middle layer support */
enum {
	PXA_USB_DEV_OTG,
	PXA_USB_DEV_SPH1,
	PXA_USB_DEV_SPH2,
	PXA_USB_DEV_SPH3,
	PXA_USB_DEV_MAX,
};

enum {
	EVENT_VBUS,
	EVENT_ID,
};

struct pxa_usb_vbus_ops {
	int (*get_vbus)(unsigned int *level);
	int (*set_vbus)(unsigned int level);
	int (*init)(void);
};

struct pxa_usb_idpin_ops {
	int (*get_idpin)(unsigned int *level);
	int (*init)(void);
};

struct pxa_usb_extern_ops {
	struct pxa_usb_vbus_ops		vbus;
	struct pxa_usb_idpin_ops	idpin;
};

#define pxa_usb_has_extern_call(id, o, f, arg...)	( \
{ \
	struct pxa_usb_extern_ops *ops;			\
	int ret;					\
	ops  = pxa_usb_get_extern_ops(id);		\
	ret = (!ops ? 0 : ((ops->o.f) ?			\
		1 : 0));				\
	ret;						\
} \
)

#define pxa_usb_extern_call(id, o, f, args...)	( \
{ \
	struct pxa_usb_extern_ops *ops;			\
	int ret;					\
	ops  = pxa_usb_get_extern_ops(id);		\
	ret = (!ops ? -ENODEV : ((ops->o.f) ?		\
		ops->o.f(args) : -ENOIOCTLCMD));	\
	ret;						\
} \
)

#define pxa_usb_set_extern_call(id, o, f, p) ( \
{ \
	struct pxa_usb_extern_ops *ops;		\
	int ret;				\
	ops = pxa_usb_get_extern_ops(id);	\
	ret = !ops ? -ENODEV : ((ops->o.f) ?	\
		-EINVAL : ({ops->o.f = p; 0; }));\
	ret;					\
} \
)

extern struct pxa_usb_extern_ops *pxa_usb_get_extern_ops(unsigned int id);
extern int pxa_usb_register_notifier(unsigned int id,
					struct notifier_block *nb);
extern int pxa_usb_unregister_notifier(unsigned int id,
					struct notifier_block *nb);
extern int pxa_usb_notify(unsigned int id, unsigned long val, void *v);
/* end of usb middle layer support */

extern int mv_udc_register_client(struct notifier_block *nb);
extern int mv_udc_unregister_client(struct notifier_block *nb);

#define MV_USB_HAS_VBUS_DETECTION	(1 << 0)
#define MV_USB_HAS_IDPIN_DETECTION	(1 << 1)
struct mv_usb_platform_data {
	unsigned int		clknum;
	char			**clkname;
	/*
	 * select from PXA_USB_DEV_OTG to PXA_USB_DEV_MAX.
	 * It indicates the index of usb device.
	 */
	unsigned int		id;
	unsigned int		extern_attr;

	/* only valid for HCD. OTG or Host only*/
	unsigned int		mode;

	/* This flag is used for that needs id pin checked by otg */
	unsigned int    disable_otg_clock_gating:1;
	/* Force a_bus_req to be asserted */
	 unsigned int    otg_force_a_bus_req:1;

	int	(*phy_init)(void __iomem *regbase);
	void	(*phy_deinit)(void __iomem *regbase);
	int     (*private_init)(void __iomem *opregs, void __iomem *phyregs);
};

#ifndef CONFIG_HAVE_CLK
/* Dummy stub for clk framework */
#define clk_get(dev, id)       NULL
#define clk_put(clock)         do {} while (0)
#define clk_enable(clock)      do {} while (0)
#define clk_disable(clock)     do {} while (0)
#endif

#endif
