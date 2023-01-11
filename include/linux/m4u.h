#ifndef _MRVL_MEDIA_MMU_H
#define _MRVL_MEDIA_MMU_H

#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>

/*
 * Marvell Media MMU - M4U, is a Marvell specific MMU used by LCD/Camera
 * controller. Although the H/W implementation is different from each other
 * the descriptor table shares the same memory layout.
 */
struct m4u_dscr {
	__u32	dma_addr;	/* DMA destination address */
	__u32	dma_size;	/* PA continue size */
};

#define M4U_DMABUF_META_ID	0x20000

/*
 * M4U Buffer Descriptor Table
 */
struct m4u_bdt {
	size_t		dscr_cnt;	/* descriptor table entry counter*/
	size_t		bpd;		/* byte per descriptor*/
	dma_addr_t	dscr_dma;	/* descriptor table start address, Used by controller */
	struct m4u_dscr	*dscr_cpu;	/* descriptor table address*/
};

/*
 * Create a physical continuous descriptor table in Marvell Media MMU format,
 ************************************************
 * 32-bits		| 32-bits		|
 ************************************************
 * DMA_address_1	| size_1		|
 * DMA_address_2	| size_2		|
 * DMA_address_3	| size_3		|
 * ...			| ...			|
 * DMA_address_N	| size_N		|
 ************************************************
 */

/*
 * Get M4U format Buffer Descriptor Table from scatter-gather list
 * @dbuf	: dmabuf on which BDT is attached on
 * @sgt		: m4u build BDT based on this scatter-gather list
 * return	: pointer at M4U format BDT on success, or NULL on failure
 */
struct m4u_bdt *m4u_get_bdt(struct dma_buf *dbuf, struct sg_table *sgt,
			size_t offset, size_t size, unsigned long align_mask_size,
			unsigned long align_mask_addr);

/*
 * Put M4U format Buffer Descriptor Table, this notify M4U that the BDT is
 * nolonger used, M4U may do some clean up job in this function
 * @bdt		: M4U BDT as returned by m4u_get_meta
 */
void m4u_put_bdt(struct m4u_bdt *bdt);
#endif /* _MRVL_MEDIA_MMU_H */
