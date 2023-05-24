// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *   USB Audio Driver for QC
 *
 *   Copyright (c) 2020 by Byung Jun Kim <jjuny79.kim@samsung.com>
 *
 */

#ifndef __LINUX_USB_QC_AUDIO_H
#define __LINUX_USB_QC_AUDIO_H

#include <sound/soc.h>
#include "../../../sound/usb/usbaudio.h"


#define USB_AUDIO_CONNECT		(1 << 0)
#define USB_AUDIO_REMOVING		(1 << 1)
#define USB_AUDIO_DISCONNECT		(1 << 2)
#define USB_AUDIO_TIMEOUT_PROBE	(1 << 3)

#define DISCONNECT_TIMEOUT	(500)

#define AUDIO_MODE_NORMAL		0
#define AUDIO_MODE_RINGTONE		1
#define AUDIO_MODE_IN_CALL		2
#define AUDIO_MODE_IN_COMMUNICATION	3
#define AUDIO_MODE_CALL_SCREEN		4

#define	CALL_INTERVAL_THRESHOLD		3

extern struct host_data xhci_data;

struct qc_usb_audio {
	struct usb_device *udev;
	struct platform_device *abox;
	struct platform_device *hcd_pdev;
};

extern int otg_connection;

int qc_usb_audio_init(struct device *dev, struct platform_device *pdev);
int qc_usb_audio_exit(void);
#endif /* __LINUX_USB_EXYNOS_AUDIO_H */
