// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * ALSA SoC - Samsung Abox OEM module
 *
 * Copyright (c) 2021 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "abox_dbg.h"
#include "abox_oem.h"

char* abox_oem_update_bootargs(struct abox_data *data)
{
	char *bootargs = NULL;
	/* SAMPLE CODE - START*/
#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_ABOX_CHANGE_RMEM_SIZE)
	char *ptr = strstr(data->bootargs, "vss");

	if (check_debug_level_low()) {
		bootargs = kmalloc(strlen(data->bootargs) + 1, GFP_KERNEL);
		memcpy(bootargs, "slog=0 ", 7);
		memcpy(bootargs + 7, ptr, strlen(ptr) + 1);
	}
#endif
	return bootargs;
	/* SAMPLE CODE - END */
	/* TO DO: Delete upper code and implement OEM specific code */
}

static ssize_t abox_oem_resize_reserved_memory_dbg(void)
{
	ssize_t size = -ENODEV;

	/* SAMPLE CODE - START*/
#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_ABOX_CHANGE_RMEM_SIZE)
	if (check_upload_mode_disabled())
		size = get_rmem_size_min(TYPE_ABOX_DBG_SIZE);
#endif
	/* SAMPLE CODE - END */
	/* TO DO: Delete upper code and implement OEM specific code */

	return size;
}

static ssize_t abox_oem_resize_reserved_memory_slog(void)
{
	ssize_t size = -ENODEV;

	/* SAMPLE CODE - START */
#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_ABOX_CHANGE_RMEM_SIZE)
	if (check_debug_level_low())
		size = get_rmem_size_min(TYPE_ABOX_SLOG_SIZE);
#endif
	/* SAMPLE CODE - END */
	/* TO DO: Delete upper code and implement OEM specific code */

	return size;
}

ssize_t abox_oem_resize_reserved_memory(enum ABOX_OEM_RESERVED_MEMORY type)
{
	ssize_t size = -ENODEV;

	switch (type) {
	case ABOX_OEM_RESERVED_MEMORY_DBG:
		size = abox_oem_resize_reserved_memory_dbg();
		break;
	case ABOX_OEM_RESERVED_MEMORY_SLOG:
		size = abox_oem_resize_reserved_memory_slog();
		break;
	}

	return size;
}
