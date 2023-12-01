// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * ALSA SoC - Samsung Abox timer driver
 *
 * Copyright (c) 2021 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_ABOX_TIMER_H
#define __SND_SOC_ABOX_TIMER_H

/**
 * Get timer device
 * @return      pointer to timer device or NULL
 */
extern struct device *abox_timer_get_device(void);

/**
 * Get current value of the timer
 * @param[in]	dev	timer device
 * @param[in]   id      id of the timer
 * @param[out]  value   current value
 * @return	0 or error code
 */
extern int abox_timer_get_current(struct device *dev, int id, u64 *value);

/**
 * Get preset value of the timer
 * @param[in]	dev	timer device
 * @param[in]   id      id of the timer
 * @param[out]  value   preset value
 * @return	0 or error code
 */
extern int abox_timer_get_preset(struct device *dev, int id, u64 *value);

/**
 * Start the timer
 * @param[in]	dev	timer device
 * @param[in]   id      id of the timer
 * @return	0 or error code
 */
extern int abox_start_timer(struct device *dev, int id);

#endif /* __SND_SOC_ABOX_TIMER_H */