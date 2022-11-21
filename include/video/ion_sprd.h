/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef	_ION_SPRD_H
#define _ION_SPRD_H

#define ION_HEAP_ID_SYSTEM	1
#define ION_HEAP_ID_MM		2
#define ION_HEAP_ID_OVERLAY	3
#define ION_HEAP_ID_FB 4

#define ION_HEAP_ID_MASK_SYSTEM 	(1<<ION_HEAP_ID_SYSTEM)
#define ION_HEAP_ID_MASK_MM		(1<<ION_HEAP_ID_MM)
#define ION_HEAP_ID_MASK_OVERLAY	(1<<ION_HEAP_ID_OVERLAY)
#define ION_HEAP_ID_MASK_FB             (1<<ION_HEAP_ID_FB)

#define ION_DRIVER_VERSION	1

enum ION_MASTER_ID {
	ION_GSP = 0,
	ION_MM,
	/*for whale iommu*/
	ION_VSP,
	ION_DCAM,
	ION_DISPC,
	ION_GSP0,
	ION_GSP1,
	ION_VPP,
};

enum ION_SPRD_CUSTOM_CMD {
	ION_SPRD_CUSTOM_PHYS,
	ION_SPRD_CUSTOM_MSYNC,

	ION_SPRD_CUSTOM_GSP_MAP,
	ION_SPRD_CUSTOM_GSP_UNMAP,
	ION_SPRD_CUSTOM_MM_MAP,
	ION_SPRD_CUSTOM_MM_UNMAP,

	ION_SPRD_CUSTOM_FENCE_CREATE,
	ION_SPRD_CUSTOM_FENCE_SIGNAL,
	ION_SPRD_CUSTOM_FENCE_DUP,
	/*for new MemoryHeapIon*/
	ION_SPRD_CUSTOM_MAP,
	ION_SPRD_CUSTOM_UNMAP,
};

enum SPRD_DEVICE_SYNC_TYPE {
	SPRD_DEVICE_PRIMARY_SYNC,
	SPRD_DEVICE_VIRTUAL_SYNC,
};

struct ion_phys_data {
	int fd_buffer;
	unsigned long phys;
	size_t size;
};

struct ion_mmu_data {
	int master_id;
	int fd_buffer;
	unsigned long iova_addr;
	size_t iova_size;
};

struct ion_addr_data {
	int fd_buffer;
	bool iova_enabled;
	unsigned long iova_addr;
	unsigned long phys_addr;
	struct dma_buf *dmabuf;
	size_t size;
};

struct ion_msync_data {
	int fd_buffer;
	void *vaddr;
	void *paddr;
	size_t size;
};

struct ion_map_data {
	int fd_buffer;
	unsigned long dev_addr;
};

struct ion_unmap_data {
	int fd_buffer;
};

struct ion_fence_data {
	uint32_t device_type;
	int life_value;
	int release_fence_fd;
	int retired_fence_fd;
};

int sprd_ion_get_gsp_addr(struct ion_addr_data *data);

int sprd_ion_free_gsp_addr(int fd);

struct dma_buf *sprd_ion_get_iommu_gspn_dmabuf_from_fd(int fd);
int sprd_ion_get_iommu_gspn_dmabuf(struct ion_addr_data *data);
int sprd_ion_free_iommu_gspn_dmabuf(struct ion_addr_data *data);

long sprd_ion_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg);

#endif /* _ION_SPRD_H */
