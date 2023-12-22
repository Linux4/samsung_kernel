// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *   USB Audio offloading Driver for QC
 *
 *   Copyright (c) 2020 by Byung Jun Kim <jjuny79.kim@samsung.com>
 *
 */


#include <linux/bitops.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/usb.h>
#include <linux/usb/audio.h>
#include <linux/usb/audio-v2.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/completion.h>

#include <sound/pcm.h>
#include <linux/usb/qc_usb_audio.h>
#include "../../drivers/usb/host/xhci.h"
#include "usbaudio.h"
#include "helper.h"
#include "card.h"
#include "quirks.h"

#include <linux/usb_notify.h>

#define DEBUG 1

static struct snd_usb_audio_vendor_ops qc_usb_audio_ops;

int qc_usb_audio_init(struct device *dev, struct platform_device *pdev)
{
	/* interface function mapping */
	snd_vendor_set_ops(&qc_usb_audio_ops);
	return 0;
}
EXPORT_SYMBOL_GPL(qc_usb_audio_init);

/* card */
static int qc_usb_audio_connect(struct usb_interface *intf)
{
	return 0;
}

static void qc_usb_audio_disconn(struct usb_interface *intf)
{
#ifdef CONFIG_USB_AUDIO_ENHANCED_DETECT_TIME
	struct usb_device *udev = interface_to_usbdev(intf);
	struct snd_usb_audio *chip = usb_get_intfdata(intf);
	
	send_usb_audio_uevent(udev, chip->card->number, 0);
	set_usb_audio_cardnum(chip->card->number, 0, 0);
#endif	
}

/* clock */
static int qc_usb_audio_set_inferface(struct usb_device *udev,
		struct usb_host_interface *alts, int iface, int alt)
{
	return 0;
}

/* pcm */
static int qc_usb_audio_set_rate(struct usb_interface *intf, int iface, int rate, int alt)
{
	int ret = 0;
	return ret;
}

static int qc_usb_audio_set_pcmbuf(struct usb_device *dev, int iface)
{
	int ret = 0;
	return ret;
}

static int qc_usb_audio_set_pcm_intf(struct usb_interface *intf,
					int iface, int alt, int direction)
{
	return 0;
}

static int qc_usb_audio_pcm_control( struct usb_device *udev,
			enum snd_vendor_pcm_open_close onoff, int direction)
{
	return 0;
}

static int qc_usb_audio_add_control(struct snd_usb_audio *chip)
{
	int ret = -1;

	if (chip != NULL) {
#ifdef CONFIG_USB_AUDIO_ENHANCED_DETECT_TIME
		set_usb_audio_cardnum(chip->card->number, 0, 1);
		send_usb_audio_uevent(chip->dev, chip->card->number, 1);
#endif		
		return 0;
	} 
	
	return ret;
}

static int qc_usb_audio_set_pcm_binterval(struct audioformat *fp,
				 struct audioformat *found,
				 int *cur_attr, int *attr)
{
	return 0;
}

/* Set interface function */
static struct snd_usb_audio_vendor_ops qc_usb_audio_ops = {
	/* card */
	.connect = qc_usb_audio_connect,
	.disconnect = qc_usb_audio_disconn,
	/* clock */
	.set_interface = qc_usb_audio_set_inferface,
	/* pcm */
	.set_rate = qc_usb_audio_set_rate,
	.set_pcm_buf = qc_usb_audio_set_pcmbuf,
	.set_pcm_intf = qc_usb_audio_set_pcm_intf,
	.set_pcm_connection = qc_usb_audio_pcm_control,
	.set_pcm_binterval = qc_usb_audio_set_pcm_binterval,
	.usb_add_ctls = qc_usb_audio_add_control,
};

int qc_usb_audio_exit(void)
{
	/* future use */
	return 0;
}
EXPORT_SYMBOL_GPL(qc_usb_audio_exit);

MODULE_AUTHOR("Byung Jun <jjuny79.kim@samsung.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("QC vendor USB Audio hooking driver");

