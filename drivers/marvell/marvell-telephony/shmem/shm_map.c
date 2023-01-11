/*
* This software program is licensed subject to the GNU General Public License
* (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

* (C) Copyright 2013 Marvell International Ltd.
* All Rights Reserved
*/

#include <linux/device.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/skbuff.h>

void *shm_vmap(phys_addr_t start, size_t size)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int i, page_count;
	pgprot_t prot;
	void *vaddr;

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);

	prot = pgprot_noncached(PAGE_KERNEL);

	pages = kmalloc(sizeof(struct page *) * page_count, GFP_KERNEL);
	if (!pages) {
		pr_err("%s: Failed to allocate array for %u pages\n", __func__,
			page_count);
		return NULL;
	}

	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = page_start + i * PAGE_SIZE;
		pages[i] = phys_to_page(addr);
	}
	vaddr = vmap(pages, page_count, VM_MAP, prot);
	kfree(pages);

	return vaddr + offset_in_page(start);
}

void shm_vunmap(void *vaddr)
{
	vunmap(vaddr - offset_in_page(vaddr));
}

void *shm_map(phys_addr_t start, size_t size)
{
	/* check if addr is normal memory */
	if (pfn_valid(__phys_to_pfn(start)))
		return shm_vmap(start, size);
	else
		return ioremap_nocache(start, size);
}
EXPORT_SYMBOL(shm_map);

void shm_unmap(phys_addr_t start, void *vaddr)
{
	if (pfn_valid(__phys_to_pfn(start)))
		shm_vunmap(vaddr);
	else
		iounmap(vaddr);
}
EXPORT_SYMBOL(shm_unmap);

struct shmpriv {
	void *addr;
	resource_size_t offset;
};

static void devm_shm_map_release(struct device *dev, void *res)
{
	struct shmpriv *ptr = res;
	shm_unmap((phys_addr_t)(ptr->offset), ptr->addr);
}

void *devm_shm_map(struct device *dev, resource_size_t offset,
		unsigned long size)
{
	struct shmpriv *ptr;
	void *addr;

	ptr = devres_alloc(devm_shm_map_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return NULL;

	addr = shm_map(offset, size);
	if (addr) {
		ptr->addr = addr;
		ptr->offset = offset;
		devres_add(dev, ptr);
	} else
		devres_free(ptr);

	return addr;
}
EXPORT_SYMBOL(devm_shm_map);

static int devm_shm_map_match(struct device *dev, void *res, void *match_data)
{
	return *(void **)res == match_data;
}

void devm_shm_unmap(struct device *dev, void *addr)
{
	WARN_ON(devres_release(dev, devm_shm_map_release, devm_shm_map_match,
			(void *)addr));
}
EXPORT_SYMBOL(devm_shm_unmap);
