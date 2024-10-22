/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef __LINUX_TRUSTY_TRUSTY_SHM_H
#define __LINUX_TRUSTY_TRUSTY_SHM_H

struct trusty_shm {
	phys_addr_t paddr;
	phys_addr_t ioremap_paddr;
	size_t mem_size;
	void *vaddr;
};

extern struct trusty_shm gshm;

void *trusty_shm_alloc(size_t size, gfp_t gfp);
void trusty_shm_free(void *virt, size_t size);
int trusty_shm_init_pool(struct device *dev);
int trusty_shm_init(struct device *dev);
void trusty_shm_destroy_pool(struct device *dev);
phys_addr_t trusty_shm_virt_to_phys(void *vaddr);
void *trusty_shm_phys_to_virt(unsigned long addr);

#endif /* __LINUX_TRUSTY_TRUSTY_SHM_H */
