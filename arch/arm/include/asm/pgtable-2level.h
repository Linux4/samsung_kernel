/*
 *  arch/arm/include/asm/pgtable-2level.h
 *
 *  Copyright (C) 1995-2002 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _ASM_PGTABLE_2LEVEL_H
#define _ASM_PGTABLE_2LEVEL_H

/*
 * Hardware-wise, we have a two level page table structure, where the first
 * level has 4096 entries, and the second level has 256 entries.  Each entry
 * is one 32-bit word.  Most of the bits in the second level entry are used
 * by hardware, and there aren't any "accessed" and "dirty" bits.
 *
 * Linux on the other hand has a three level page table structure, which can
 * be wrapped to fit a two level page table structure easily - using the PGD
 * and PTE only.  However, Linux also expects one "PTE" table per page, and
 * at least a "dirty" bit.
 *
 * Therefore, we tweak the implementation slightly - we tell Linux that we
 * have 2048 entries in the first level, each of which is 8 bytes (iow, two
 * hardware pointers to the second level.)  The second level contains two
 * hardware PTE tables arranged contiguously, preceded by Linux versions
 * which contain the state information Linux needs.  We, therefore, end up
 * with 512 entries in the "PTE" level.
 *
 * This leads to the page tables having the following layout:
 *
 *    pgd             pte
 * |        |
 * +--------+
 * |        |       +------------+ +0
 * +- - - - +       | Linux pt 0 |
 * |        |       +------------+ +1024
 * +--------+ +0    | Linux pt 1 |
 * |        |-----> +------------+ +2048
 * +- - - - + +4    |  h/w pt 0  |
 * |        |-----> +------------+ +3072
 * +--------+ +8    |  h/w pt 1  |
 * |        |       +------------+ +4096
 *
 * See L_PTE_xxx below for definitions of bits in the "Linux pt", and
 * PTE_xxx for definitions of bits appearing in the "h/w pt".
 *
 * PMD_xxx definitions refer to bits in the first level page table.
 *
 * The "dirty" bit is emulated by only granting hardware write permission
 * iff the page is marked "writable" and "dirty" in the Linux PTE.  This
 * means that a write to a clean page will cause a permission fault, and
 * the Linux MM layer will mark the page dirty via handle_pte_fault().
 * For the hardware to notice the permission change, the TLB entry must
 * be flushed, and ptep_set_access_flags() does that for us.
 *
 * The "accessed" or "young" bit is emulated by a similar method; we only
 * allow accesses to the page if the "young" bit is set.  Accesses to the
 * page will cause a fault, and handle_pte_fault() will set the young bit
 * for us as long as the page is marked present in the corresponding Linux
 * PTE entry.  Again, ptep_set_access_flags() will ensure that the TLB is
 * up to date.
 *
 * However, when the "young" bit is cleared, we deny access to the page
 * by clearing the hardware PTE.  Currently Linux does not flush the TLB
 * for us in this case, which means the TLB will retain the transation
 * until either the TLB entry is evicted under pressure, or a context
 * switch which changes the user space mapping occurs.
 */
#ifdef CONFIG_TIMA_RKP
#include <asm/tlbflush.h>
#include <asm/cp15.h>
#endif
#define PTRS_PER_PTE		512
#define PTRS_PER_PMD		1
#define PTRS_PER_PGD		2048

#define PTE_HWTABLE_PTRS	(PTRS_PER_PTE)
#define PTE_HWTABLE_OFF		(PTE_HWTABLE_PTRS * sizeof(pte_t))
#define PTE_HWTABLE_SIZE	(PTRS_PER_PTE * sizeof(u32))

/*
 * PMD_SHIFT determines the size of the area a second-level page table can map
 * PGDIR_SHIFT determines what a third-level page table entry can map
 */
#define PMD_SHIFT		21
#define PGDIR_SHIFT		21

#define PMD_SIZE		(1UL << PMD_SHIFT)
#define PMD_MASK		(~(PMD_SIZE-1))
#define PGDIR_SIZE		(1UL << PGDIR_SHIFT)
#define PGDIR_MASK		(~(PGDIR_SIZE-1))

/*
 * section address mask and size definitions.
 */
#define SECTION_SHIFT		20
#define SECTION_SIZE		(1UL << SECTION_SHIFT)
#define SECTION_MASK		(~(SECTION_SIZE-1))

/*
 * ARMv6 supersection address mask and size definitions.
 */
#define SUPERSECTION_SHIFT	24
#define SUPERSECTION_SIZE	(1UL << SUPERSECTION_SHIFT)
#define SUPERSECTION_MASK	(~(SUPERSECTION_SIZE-1))

#define USER_PTRS_PER_PGD	(TASK_SIZE / PGDIR_SIZE)

/*
 * "Linux" PTE definitions.
 *
 * We keep two sets of PTEs - the hardware and the linux version.
 * This allows greater flexibility in the way we map the Linux bits
 * onto the hardware tables, and allows us to have YOUNG and DIRTY
 * bits.
 *
 * The PTE table pointer refers to the hardware entries; the "Linux"
 * entries are stored 1024 bytes below.
 */
#define L_PTE_VALID		(_AT(pteval_t, 1) << 0)		/* Valid */
#define L_PTE_PRESENT		(_AT(pteval_t, 1) << 0)
#define L_PTE_YOUNG		(_AT(pteval_t, 1) << 1)
#define L_PTE_FILE		(_AT(pteval_t, 1) << 2)	/* only when !PRESENT */
#define L_PTE_DIRTY		(_AT(pteval_t, 1) << 6)
#define L_PTE_RDONLY		(_AT(pteval_t, 1) << 7)
#define L_PTE_USER		(_AT(pteval_t, 1) << 8)
#define L_PTE_XN		(_AT(pteval_t, 1) << 9)
#define L_PTE_SHARED		(_AT(pteval_t, 1) << 10)	/* shared(v6), coherent(xsc3) */
#define L_PTE_NONE		(_AT(pteval_t, 1) << 11)

/*
 * These are the memory types, defined to be compatible with
 * pre-ARMv6 CPUs cacheable and bufferable bits:   XXCB
 */
#define L_PTE_MT_UNCACHED	(_AT(pteval_t, 0x00) << 2)	/* 0000 */
#define L_PTE_MT_BUFFERABLE	(_AT(pteval_t, 0x01) << 2)	/* 0001 */
#define L_PTE_MT_WRITETHROUGH	(_AT(pteval_t, 0x02) << 2)	/* 0010 */
#define L_PTE_MT_WRITEBACK	(_AT(pteval_t, 0x03) << 2)	/* 0011 */
#define L_PTE_MT_MINICACHE	(_AT(pteval_t, 0x06) << 2)	/* 0110 (sa1100, xscale) */
#define L_PTE_MT_WRITEALLOC	(_AT(pteval_t, 0x07) << 2)	/* 0111 */
#define L_PTE_MT_DEV_SHARED	(_AT(pteval_t, 0x04) << 2)	/* 0100 */
#define L_PTE_MT_DEV_NONSHARED	(_AT(pteval_t, 0x0c) << 2)	/* 1100 */
#define L_PTE_MT_DEV_WC		(_AT(pteval_t, 0x09) << 2)	/* 1001 */
#define L_PTE_MT_DEV_CACHED	(_AT(pteval_t, 0x0b) << 2)	/* 1011 */
#define L_PTE_MT_MASK		(_AT(pteval_t, 0x0f) << 2)

#ifndef __ASSEMBLY__

/*
 * The "pud_xxx()" functions here are trivial when the pmd is folded into
 * the pud: the pud entry is never bad, always exists, and can't be set or
 * cleared.
 */
#define pud_none(pud)		(0)
#define pud_bad(pud)		(0)
#define pud_present(pud)	(1)
#define pud_clear(pudp)		do { } while (0)
#define set_pud(pud,pudp)	do { } while (0)

static inline pmd_t *pmd_offset(pud_t *pud, unsigned long addr)
{
	return (pmd_t *)pud;
}

#define pmd_bad(pmd)		(pmd_val(pmd) & 2)

#ifdef  CONFIG_TIMA_RKP_L1_TABLES
#ifndef rkp_call
#ifdef CONFIG_HYP_RKP 
#define rkp_call "hvc #10\n"
asm(".arch_extension virt");
#else
#define rkp_call "smc #10\n" 
asm(".arch_extension sec\n");
#endif 
#endif	//CONFIG_HYP_RKP
#endif 

extern int boot_mode_security;
#ifdef	CONFIG_TIMA_RKP_L1_TABLES
static inline void copy_pmd(pmd_t *pmdpd, pmd_t *pmdps)
{
	if (boot_mode_security) {
		unsigned long cmd_id = 0x83809000;
		unsigned long tima_wr_out;

		cpu_dcache_clean_area(pmdpd, 8);
		__asm__ __volatile__ (
			"stmfd  sp!,{r0-r4}\n"
			"mov   	r2, r0\n"  /* useless here for backward compatible reason */
			"mov    r0, %1\n"
			"mov	r1, %2\n"
			"mov    r3, %3\n"
			"mov    r4, %4\n"
			"mcr    p15, 0, r1, c7, c14, 1\n"
			"add    r1, r1, #4\n"
			"mcr    p15, 0, r1, c7, c14, 1\n"
			"dsb\n"
			rkp_call
	//		"mcr    p15, 0, r1, c7, c10,  1\n"
	//		"sub    r1, r1, #4\n"
	//		"mcr    p15, 0, r1, c7, c10,  1\n"
	//		"dsb\n"
			"mov    r0, #0\n"
			"mcr    p15, 0, r0, c8, c3, 0\n"
			"dsb\n"
			"isb\n"
			"ldmfd   sp!, {r0-r4}\n"
			:"=r"(tima_wr_out):"r"(cmd_id),"r"((unsigned long)pmdpd),"r"(pmdps[0]),"r"(pmdps[1]):"r0","r1","r2","r3","r4","cc");

			flush_pmd_entry(pmdpd);
	} else {
		do {
			pmdpd[0] = pmdps[0];
			pmdpd[1] = pmdps[1];
			flush_pmd_entry(pmdpd);
		} while (0);
	}
}
#else
#define copy_pmd(pmdpd,pmdps)		\
	do {				\
		pmdpd[0] = pmdps[0];	\
		pmdpd[1] = pmdps[1];	\
		flush_pmd_entry(pmdpd);	\
	} while (0)
#endif

#ifdef  CONFIG_TIMA_RKP_L1_TABLES
#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
        asm(".arch_extension virt");
#endif
#endif

#ifdef CONFIG_TIMA_RKP
extern void cpu_v7_tima_iommu_opt(unsigned long start,
					unsigned long end, unsigned long pgd);
#endif 

#ifdef  CONFIG_TIMA_RKP_L1_TABLES
static inline void pmd_clear(pmd_t *pmdp)
{
	if (boot_mode_security) {
		unsigned long cmd_id = 0x8380a000;
		unsigned long tima_wr_out;
		cpu_dcache_clean_area(pmdp, 8);	
		__asm__ __volatile__ (
			"stmfd  sp!,{r0-r2}\n"
			"mov   	r2, r0\n"
			"mov    r0, %1\n"
			"mov	r1, %2\n"
			"mcr    p15, 0, r1, c7, c14, 1\n"
			"add    r1, r1, #4\n"
			"mcr    p15, 0, r1, c7, c14, 1\n"
			"dsb\n"
			rkp_call
	//		"mcr    p15, 0, r1, c7, c10,  1\n"
	//		"sub    r1, r1, #4\n"
	//		"mcr    p15, 0, r1, c7, c10,  1\n"
	//		"dsb\n"
			"mov    r0, #0\n" 
			"mov    %0, r0\n"
			"mcr    p15, 0, r0, c8, c3, 0\n"
			"dsb\n"
			"isb\n"
			"ldmfd  sp!, {r0-r2}\n"
			:"=r"(tima_wr_out):"r"(cmd_id),"r"((unsigned long)pmdp):"r0","r1","r2","cc");
			 
			clean_pmd_entry(pmdp);
	} else {
		do {
			pmdp[0] = __pmd(0);
			pmdp[1] = __pmd(0);
			clean_pmd_entry(pmdp);
		} while (0);
	}
}
#else
#define pmd_clear(pmdp)			\
	do {				\
		pmdp[0] = __pmd(0);	\
		pmdp[1] = __pmd(0);	\
		clean_pmd_entry(pmdp);	\
	} while (0)
#endif

#ifdef CONFIG_TIMA_RKP_L2_GROUP 
extern int cpu_v7_timal2group_set_pte_ext(pte_t *ptep, pte_t pte, unsigned int ext,
		 	unsigned long tima_l2group_entry_ptr);
extern void cpu_v7_timal2group_set_pte_commit(void *tima_l2group_entry_ptr,
					unsigned long tima_l2group_entries_count);
#endif /* CONFIG_TIMA_RKP_L2_GROUP */
/* we don't need complex calculations here as the pmd is folded into the pgd */
#define pmd_addr_end(addr,end) (end)

#ifdef CONFIG_RKP_DBLMAP_PROT
#define RAM_SIZE (0xbfffffffUL)
//the space is reserved in vmlinux.lds.S
extern u8 rkp_double_bitmap[];
static u8 rkp_is_pg_double_mapped(unsigned long pa)
{
	unsigned long addr, bit_index, u8_index;
	u8 val;

	if (pa < PHYS_OFFSET){ return 0; }

	addr = pa - PHYS_OFFSET;
	bit_index = (addr>>PAGE_SHIFT);
	u8_index = bit_index >> 3;

	//larger than bit map size, ignore
	if (u8_index >= 0x18000) { return 0; }

	val = (((rkp_double_bitmap[u8_index]) & (1<< (bit_index % 8)))?1:0);
	if(val){
		printk("DBLMAP pa=0x%08lx, pa-PHYS_OFFSET=0x%08lx\n", pa, addr);
		printk("DBLMAP bitmap=0x%08lx, byte addr=0x%08lx, byte val=0x%02x\n", 
				(unsigned long)rkp_double_bitmap, 
				(unsigned long) (&rkp_double_bitmap[u8_index]), 
				rkp_double_bitmap[u8_index]);
	}
	return val;
}
#endif 

#ifdef CONFIG_TIMA_RKP_L2_TABLES
static inline void set_pte_ext(pte_t *ptep,pte_t pte,unsigned int ext)
{
	if(!(boot_mode_security)){
		cpu_set_pte_ext(ptep,pte,ext);
		return;
	}

	if (tima_is_pg_protected((unsigned long) ptep) == 0)
		cpu_set_pte_ext(ptep,pte,ext);
	else
		cpu_tima_set_pte_ext(ptep,pte,ext);
}
#else
	#ifdef CONFIG_RKP_DBLMAP_PROT
		static inline void set_pte_ext(pte_t *ptep,pte_t pte,unsigned int ext){			
			if (rkp_is_pg_double_mapped((u32)pte)) {
				panic("\n Trying to double map the page \n");
				return;
			}	
			cpu_set_pte_ext(ptep,pte,ext);
		}
	#else
		#define set_pte_ext(ptep,pte,ext) cpu_set_pte_ext(ptep,pte,ext)
	#endif
#endif

#ifdef CONFIG_TIMA_RKP_LAZY_MMU
#define TIMA_LAZY_MMU_CMDID  0x25
#define TIMA_LAZY_MMU_START  0
#define TIMA_LAZY_MMU_STOP   1
#endif

#endif /* __ASSEMBLY__ */

#endif /* _ASM_PGTABLE_2LEVEL_H */
