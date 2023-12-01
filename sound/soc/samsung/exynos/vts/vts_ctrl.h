/* SPDX-License-Identifier: GPL-2.0-or-later
 * sound/soc/samsung/vts/vts_ctrl.h
 *
 * ALSA SoC - Samsung VTS driver
 *
 * Copyright (c) 2022 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_VTS_CTRL_H
#define __SND_SOC_VTS_CTRL_H

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/soc-dai.h>
#include "vts.h"

/**
 * register snd_soc_component object for current SoC
 * @param[in]	dev	pointer to vts device
 * @return		0 or error code
 */
extern int vts_ctrl_register(struct device *vdev);

#ifndef __ALIGN_UP
#define __ALIGN_UP(x,a) (((x) + ((a) - 1)) & ~((a) - 1))
#endif

#endif /* __SND_SOC_VTS_CTRL_H */
