/* SPDX-License-Identifier: GPL-2.0-or-later
 * sound/soc/samsung/vts/vts_misc.h
 *
 * ALSA SoC - Samsung VTS driver
 *
 * Copyright (c) 2022 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_VTS_MISC_H
#define __SND_SOC_VTS_MISC_H

#include <linux/miscdevice.h>
#include <linux/device.h>
#include "vts.h"

/**
 * Register log buffer
 * @param[in]	dev		pointer to vts device
 * @return	error code if any
 */
extern int vts_misc_register(
	struct device *dev_vts);

#endif /* __SND_SOC_VTS_MISC_H */
