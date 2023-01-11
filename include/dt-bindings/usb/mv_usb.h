#ifndef _DT_BINDINGS_USB_MV_USB_H
#define _DT_BINDINGS_USB_MV_USB_H

/*
 * PHY revision: For those has small difference with default setting.
 * bit [15..8]: represent PHY IP as below:
 *     PHY_55LP        0x5500,
 *     PHY_40LP        0x4000,
 *     PHY_28LP        0x2800,
 */
#define REV_PXA168     0x5500
#define REV_PXA910     0x5501

#define MV_USB_HAS_VBUS_DETECTION	(1 << 0)
#define MV_USB_HAS_IDPIN_DETECTION	(1 << 1)
#define MV_USB_HAS_VBUS_IDPIN_DETECTION	\
			(MV_USB_HAS_VBUS_DETECTION | MV_USB_HAS_IDPIN_DETECTION)

#define	MV_USB_MODE_UDC	(0)
#define	MV_USB_MODE_OTG	(1)
#define	MV_USB_MODE_HOST	(2)

#endif
