/*
 * =============================================================================
 *
 *       Filename:  cmainfo.h
 *
 *      description: All the declaration related to cmainfo
 *
 *        Version:  1.0
 *        Created:  17/7/2014
 *       Revision:  none
 *       Compiler:  arm-gcc
 *
 *   Organization:  AP Systems R&D 2 , SRIB
 *
 *Copyright (C) 2014, Samsung Electronics Co., Ltd. All Rights Reserved.
 * =============================================================================
 */

#include <linux/types.h>

#define CMA_AREA_DEVICE		0
#define CMA_AREA_GLOBAL		1
#define NR_CMA_AREA		10

/*
 * free block is a structure created which stores the
 * address and order of the maximum free order chunk till then
 * phy_addr: store the starting addr of the page in struct array
 * block_order: order of the block
 */
struct free_block{
     phys_addr_t phys_addr;
     int block_order;
};
typedef struct free_block free_block_t;

struct cma_area{
	phys_addr_t cma_phy_start;
	phys_addr_t cma_phy_end;
	unsigned int flag;
};
typedef struct cma_area cma_area_t;

struct cmainfo{
	unsigned long totalcma;
	unsigned long freecma;
	unsigned long cma_active_anon;
	unsigned long cma_active_file;
	unsigned long cma_inactive_anon;
	unsigned long cma_inactive_file;
	unsigned long nr_cma_areas;
	cma_area_t cma_areas[NR_CMA_AREA];
};
typedef struct cmainfo cmainfo_t;

void cma_page_type_det(cmainfo_t *cmainfo);
void cma_mem_info(cmainfo_t *cmainfo);
void cma_walk_pgd(struct sed_file *m);
void cmatypeinfo_showfree_print(struct seq_file *m, unsigned long totalcma);
unsigned int cma_free_mem_get(void);
void cma_range_populate(unsigned long virt_start, unsigned long virt_end);

