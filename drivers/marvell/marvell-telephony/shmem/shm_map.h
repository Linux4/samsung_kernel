/*
* This software program is licensed subject to the GNU General Public License
* (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

* (C) Copyright 2013 Marvell International Ltd.
* All Rights Reserved
*/

#ifndef SHM_MAP_H_
#define SHM_MAP_H_

#include <linux/types.h>

extern void *shm_map(phys_addr_t start, size_t size);
extern void shm_unmap(phys_addr_t start, void *vaddr);
extern void *devm_shm_map(struct device *dev, resource_size_t offset,
		unsigned long size);
extern void devm_shm_unmap(struct device *dev, void *addr);


#endif /* SHM_MAP_H_ */
