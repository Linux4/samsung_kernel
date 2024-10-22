/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef MBRAINK_AUDIO_H
#define MBRAINK_AUDIO_H

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include "mbraink_ioctl_struct_def.h"

int mbraink_audio_init(void);
int mbraink_audio_deinit(void);
int mbraink_audio_getIdleRatioInfo(struct mbraink_audio_idleRatioInfo *pmbrainkAudioIdleRatioInfo);
ssize_t getTimeoutCouterReport(char *pBuf);

extern int mbraink_netlink_send_msg(const char *msg); //EXPORT_SYMBOL_GPL
#if (MBRAINK_LANDING_FEATURE_CHECK == 0)
extern void (*audiokeylog2mbrain_fp)(int level, const char *buf);
#endif


void audiokeylog2mbrain(int level, const char *buf); //For Audio UDM callback.

#endif /*end of MBRAINK_AUDIO_H*/

