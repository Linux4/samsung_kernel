#ifndef __SPRD_MEM_POOL_H
#define __SPRD_MEM_POOL_H
 
extern struct page *sprd_page_alloc(gfp_t gfp_mask, unsigned int order, unsigned long zoneidx);

#endif
