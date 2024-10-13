#ifndef __UNPU_H__
#define __UNPU_H__

#include <linux/debugfs.h>
#include "include/npu-binary.h"

enum unpu_iomem_names {
	UNPU_CTRL,
	YAMIN_CTRL,
	UNPU_SRAM,
	UNPU_MAX_IOMEM,
};

struct unpu_iomem_data {
	void __iomem *vaddr;
	phys_addr_t paddr;
	resource_size_t	size;
};

struct unpu_debug {
	struct dentry *dfile_root;
	unsigned long  state;
};

struct unpu_device
{
	u32 pmu_offset;
	struct device *dev;
	struct unpu_iomem_data iomem_data[UNPU_MAX_IOMEM];
	struct unpu_debug debug;
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	struct imgloader_desc imgloader;
#endif
};

int __init unpu_register(void);
#endif
