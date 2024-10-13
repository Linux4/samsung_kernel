// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/usb/audio.h>
#include <linux/usb/audio-v2.h>
#include <linux/usb_notify.h>
#include <linux/usb/mtk-usb-audio.h>
#include <sound/pcm.h>
#include "usbaudio.h"
#include "card.h"

static struct snd_usb_audio_vendor_ops mtk_usb_audio_ops;

void mtk_usb_audio_init(void)
{
	pr_info("%s\n", __func__);
	snd_vendor_set_ops(&mtk_usb_audio_ops);
}
EXPORT_SYMBOL_GPL(mtk_usb_audio_init);

void mtk_usb_audio_exit(void)
{
	/* future use */
	pr_info("%s\n", __func__);
}
EXPORT_SYMBOL_GPL(mtk_usb_audio_exit);

/* card */
static int mtk_usb_audio_connect(struct usb_interface *intf)
{
	return 0;
}

static void mtk_usb_audio_disconnect(struct usb_interface *intf)
{
}

/* clock */
static int mtk_usb_audio_set_interface(struct usb_device *udev,
		struct usb_host_interface *alts, int iface, int alt)
{
	return 0;
}

/* pcm */
static int mtk_usb_audio_set_rate(struct usb_interface *intf, int iface, int rate, int alt)
{
	return 1;
}

static int mtk_usb_audio_set_pcmbuf(struct usb_device *udev, int iface)
{
	return 0;
}

static int mtk_usb_audio_set_pcm_intf(struct usb_interface *intf,
					int iface, int alt, int direction)
{
	return 0;
}

static int mtk_usb_audio_set_pcm_connection(struct usb_device *udev,
			enum snd_vendor_pcm_open_close onoff, int direction)
{
#if IS_ENABLED(CONFIG_USB_NOTIFY_PROC_LOG)
	int enable = (int)onoff, type = 0;

	if (direction == SNDRV_PCM_STREAM_PLAYBACK)
		type = NOTIFY_PCM_PLAYBACK;
	else
		type = NOTIFY_PCM_CAPTURE;

	store_usblog_notify(type, (void *)&enable, NULL);

	pr_info("%s - en : %d, dir : %d\n", __func__, enable, direction);
#endif
	return 0;
}

static int mtk_usb_audio_add_ctls(struct snd_usb_audio *chip)
{
	return 0;
}

static int mtk_usb_audio_set_pcm_binterval(struct audioformat *fp,
				 struct audioformat *found,
				 int *cur_attr, int *attr)
{
	return 0;
}

static struct snd_usb_audio_vendor_ops mtk_usb_audio_ops = {
	/* card */
	.connect = mtk_usb_audio_connect,
	.disconnect = mtk_usb_audio_disconnect,
	/* clock */
	.set_interface = mtk_usb_audio_set_interface,
	/* pcm */
	.set_rate = mtk_usb_audio_set_rate,
	.set_pcm_buf = mtk_usb_audio_set_pcmbuf,
	.set_pcm_intf = mtk_usb_audio_set_pcm_intf,
	.set_pcm_connection = mtk_usb_audio_set_pcm_connection,
	.set_pcm_binterval = mtk_usb_audio_set_pcm_binterval,
	.usb_add_ctls = mtk_usb_audio_add_ctls,
};

MODULE_AUTHOR("Wookwang Lee <wookwang.lee@samsung.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MTK USB Audio vendor hooking driver");
