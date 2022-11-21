/*
 * vbus.h
 *
 * Copyright (C) 2010 Marvell International Ltd.
 * 	Haojian Zhuang <haojian.zhuang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PLAT_VBUS_H
#define __PLAT_VBUS_H

#define USB_STS				0x0144
#define USB_INTR			0x0148
#define USB_OTGSC			0x01a4

#define INTR_UE				(1 << 0) /* USB Int Enable */

#define OTGSC_OT			(1 << 3) /* OTG Termination */
#define OTGSC_IDPU			(1 << 5) /* ID Pullup */
#define OTGSC_ID			(1 << 8) /* USB ID */
#define OTGSC_AVV			(1 << 9) /* A VBus Valid */
#define OTGSC_ASV			(1 << 10) /* A Session Valid */
#define OTGSC_BSV			(1 << 11) /* B Session Valid */
#define OTGSC_BSE			(1 << 12) /* B Session End */
#define OTGSC_1MST			(1 << 13) /* 1 msec timer toggle */
#define OTGSC_DPS			(1 << 14) /* Data Bus Pulsing */
#define OTGSC_IDIS			(1 << 16) /* USB ID Int Status */
#define OTGSC_AVVIS			(1 << 17) /* A VBus Valid Int Status */
#define OTGSC_ASVIS			(1 << 18) /* A Session Valid Int Status */
#define OTGSC_BSVIS			(1 << 19) /* B Session Valid Int Status */
#define OTGSC_BSEIS			(1 << 20) /* B Session End Int Status */
#define OTGSC_1MSS			(1 << 21) /* 1 msec timer Int Status */
#define OTGSC_DPIS			(1 << 22) /* Data Pulse Int Status */
#define OTGSC_IDIE			(1 << 24) /* USB ID Int Enable */
#define OTGSC_AVVIE			(1 << 25) /* A VBus Valid Int Enable */
#define OTGSC_ASVIE			(1 << 26) /* A Session Valid Int Enable */
#define OTGSC_BSVIE			(1 << 27) /* B Session Valid Int Enable */
#define OTGSC_BSEIE			(1 << 28) /* B Session End Int Enable */
#define OTGSC_1MSE			(1 << 29) /* 1 msec timer Int Enable */
#define OTGSC_DPIE			(1 << 30) /* Data Pulse Int Enable */

#define OTGSC_INTS_MASK			(OTGSC_IDIS | OTGSC_AVVIS	\
					| OTGSC_ASVIS | OTGSC_BSVIS	\
					| OTGSC_BSEIS | OTGSC_DPIS)

enum {
	VBUS_A_VALID		= 1 << 0,
	VBUS_A_SESSION_VALID	= 1 << 1,
	VBUS_B_SESSION_VALID	= 1 << 2,
	VBUS_B_SESSION_END	= 1 << 3,
	VBUS_ID			= 1 << 4,
	VBUS_DPS		= 1 << 5,
	VBUS_1MS		= 1 << 6,
	VBUS_EVENT		= 1 << 16,

};

enum {

	VBUS_LOW		= 0,
	VBUS_HIGH		= 1 << 0,
	VBUS_CHARGE		= 1 << 1,
	VBUS_SRP		= 1 << 2,
};

#define USB_A_DEVICE		0
#define USB_B_DEVICE		1
#define VBUS_FLAGS_MASK		(0xffff)

/* OTG interrupt type*/
#define OTG_INT_INIT	0
#define OTG_INT_IDF	(1 << 0)
#define OTG_INT_IDR	(1 << 1)
#define OTG_INT_RSV	(1 << 2)
#define OTG_INT_FSV	(1 << 3)
#define OTG_INT_RVV	(1 << 4)
#define OTG_INT_FVV	(1 << 5)
#define OTG_INT_B_RSV	(1 << 6)
#define OTG_INT_B_FSV	(1 << 7)
#define OTG_INT_B_RSE	(1 << 8)
#define OTG_INT_B_FSE	(1 << 9)
#define OTG_INT_LS	(1 << 10)
#define OTG_INT_LP_DIS	(1 << 11)

struct u2o_vbus_pdata {
	int		vbus_en;		/* control VBus by GPIO */
	int		vbus_irq;		/* IRQ of VBus event */
	unsigned int	reg_base;		/* register base of USB */
	unsigned int	reg_end;		/* range of USB registers */
};

struct pxa_vbus_info {
	int	(*prepare_vbus)(int enable);
	int	(*vbus_init)(void);
	int	(*set_vbus)(int enable, int srp);
	int	(*query_vbus)(void);
	int     (*query_usbid)(void);
	int     (*query_session)(void);
	int	(*mask_vbus)(int flags);
	int	(*unmask_vbus)(int flags);
	int	(*handler)(void *data);
	void	(*func)(int);

	unsigned int	base;
	struct device	*dev;
	struct resource	*res;
	struct clk	*clk;
	int		gpio_en;
	int		usbdev;		/* Device A or B */
};

extern int pxa_set_usbdev(int usbdev);
extern int pxa_query_usbdev(void);
extern int pxa_prepare_vbus(int enable);
extern int pxa_set_vbus(int enable, int srp);
extern int pxa_query_vbus(void);
extern int pxa_query_usbid(void);
extern int pxa_query_session(void);
extern int pxa_query_usbid(void);
extern int pxa_mask_vbus(int flags);
extern int pxa_unmask_vbus(int flags);
extern void pxa_vbus_handler(int status);
extern int pxa_register_vbus_event(void (*func)(int));
extern int pxa_unregister_vbus_event(void (*func)(int));
extern int __devinit pxa_vbus_init(struct pxa_vbus_info *info);
extern void __devexit pxa_vbus_deinit(void);

#endif	/* __PLAT_VBUS_H */

