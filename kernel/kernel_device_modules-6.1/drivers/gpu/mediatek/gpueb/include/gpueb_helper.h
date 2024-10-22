// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GPUEB_HELPER_H__
#define __GPUEB_HELPER_H__

#define GPUEB_TAG   "[GPU/EB] "
#define gpueb_pr_info(fmt, args...) \
	pr_info(GPUEB_TAG"@%s: "fmt"\n", __func__, ##args)
#define gpueb_pr_debug(fmt, args...) \
	pr_info(GPUEB_TAG"@%s: "fmt"\n", __func__, ##args)
#define gpueb_pr_logbuf(buf, len, size, fmt, args...) \
	{ \
		pr_info(GPUEB_TAG"@%s: "fmt"\n", __func__, ##args); \
		if (buf && len) \
			*len += snprintf(buf + *len, size - *len, fmt"\n", ##args); \
	}

#endif /* __GPUEB_HELPER_H__ */