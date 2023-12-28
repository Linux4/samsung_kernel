/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __ADSP_SLEEPMON_H__
#define __ADSP_SLEEPMON_H__

#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_AUDIO)
bool check_is_audio_active(void);
#endif

#endif
