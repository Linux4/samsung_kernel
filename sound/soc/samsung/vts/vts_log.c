// SPDX-License-Identifier: GPL-2.0-or-later
/* sound/soc/samsung/vts/vts_log.c
 *
 * ALSA SoC - Samsung VTS Log driver
 *
 * Copyright (c) 2017 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <sound/soc.h>

#include "vts.h"
#include "vts_dbg.h"
#include "vts_log.h"

struct vts_log_buffer {
	char *addr;
	unsigned int size;
};

struct vts_log_buffer_info {
	struct device *dev;
	struct mutex lock;
	struct vts_log_buffer log_buffer;
	bool registered;
	u32 logbuf_index;
};

static struct vts_log_buffer_info glogbuf_info;

static void vts_log_flush_work_func(struct work_struct *work)
{
	struct device *dev = glogbuf_info.dev;
	struct vts_log_buffer *log_buffer = &glogbuf_info.log_buffer;
	struct vts_data *data = dev_get_drvdata(dev);
	int logbuf_index = glogbuf_info.logbuf_index;

	vts_dev_dbg(dev, "%s: LogBuffer Index: %d\n", __func__,
		logbuf_index);

	mutex_lock(&glogbuf_info.lock);
	if (data->fw_logger_enabled && data->fw_log_obj) {
		memlog_write(data->fw_log_obj, MEMLOG_LEVEL_INFO,
			(log_buffer->addr+log_buffer->size*logbuf_index),
			log_buffer->size);
	}
	mutex_unlock(&glogbuf_info.lock);
}

static DECLARE_WORK(vts_log_work, vts_log_flush_work_func);
void vts_log_schedule_flush(struct device *dev, u32 index)
{
	if (glogbuf_info.registered &&
		glogbuf_info.log_buffer.size) {
		glogbuf_info.logbuf_index = index;
		schedule_work(&vts_log_work);
		vts_dev_dbg(dev, "%s: VTS Log Buffer[%d] Scheduled\n",
			 __func__, index);
	} else
		vts_dev_warn(dev, "%s: VTS Debugging buffer not registered\n",
				 __func__);
}
EXPORT_SYMBOL(vts_log_schedule_flush);

int vts_register_log_buffer(
	struct device *dev,
	u32 addroffset,
	u32 logsz)
{
	struct vts_data *data = dev_get_drvdata(dev);

	vts_dev_dbg(dev, "%s(offset 0x%x)\n", __func__, addroffset);

	if ((addroffset + logsz) > data->sram_size) {
		vts_dev_warn(dev, "%s: wrong offset[0x%x] or size[0x%x]\n",
				__func__, addroffset, logsz);
		return -EINVAL;
	}

	if (!glogbuf_info.registered) {
		glogbuf_info.dev = dev;
		mutex_init(&glogbuf_info.lock);
		glogbuf_info.registered = true;
	}

	/* Update logging buffer address and size info */
	glogbuf_info.log_buffer.addr = data->sram_base + addroffset;
	glogbuf_info.log_buffer.size = logsz;

	return 0;
}
EXPORT_SYMBOL(vts_register_log_buffer);
