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

#define ION_HEAP_ID_MASK_SYSTEM 	(1<<ION_HEAP_ID_SYSTEM)
#define ION_HEAP_ID_MASK_MM		(1<<ION_HEAP_ID_MM)
#define ION_HEAP_ID_MASK_OVERLAY	(1<<ION_HEAP_ID_OVERLAY)


#define ION_DRIVER_VERSION	1

struct ion_phys_data {
	int fd_buffer;
	unsigned long phys;
	size_t size;
};

struct ion_mmu_data {
	int fd_buffer;
	unsigned long iova_addr;
	size_t iova_size;
};

struct ion_addr_data {
	int fd_buffer;
	bool iova_enabled;
	unsigned long iova_addr;
	unsigned long phys_addr;
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

enum SPRD_DEVICE_SYNC_TYPE {
	SPRD_DEVICE_PRIMARY_SYNC,
	SPRD_DEVICE_VIRTUAL_SYNC,
};

struct ion_fence_data {
	uint32_t device_type;
	int life_value;
	int release_fence_fd;
	int retired_fence_fd;
};

enum ION_SPRD_CUSTOM_CMD {
	ION_SPRD_CUSTOM_PHYS,
	ION_SPRD_CUSTOM_MSYNC,

	/* to get/free mmu iova */ //added by yfs
	ION_SPRD_CUSTOM_GSP_MAP,
	ION_SPRD_CUSTOM_GSP_UNMAP,
	ION_SPRD_CUSTOM_MM_MAP,
	ION_SPRD_CUSTOM_MM_UNMAP,
        ION_SPRD_CUSTOM_FENCE_CREATE,
        ION_SPRD_CUSTOM_FENCE_SIGNAL,
        ION_SPRD_CUSTOM_FENCE_DUP,
};

int sprd_ion_get_gsp_addr(struct ion_addr_data *data);

int sprd_ion_free_gsp_addr(int fd);

#endif /* _ION_SPRD_H */
