/* sound/soc/samsung/vts/vts_dbg.h
 *
 * ALSA SoC - Samsung Abox Debug driver
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_VTS_DEBUG_H
#define __SND_SOC_VTS_DEBUG_H

#include "vts.h"

/**
 * Initialize vts debug driver
 * @return	dentry of vts debugfs root directory
 */
extern struct dentry *vts_dbg_get_root_dir(void);

#endif /* __SND_SOC_VTS_DEBUG_H */
