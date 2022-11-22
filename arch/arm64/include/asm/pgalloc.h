/*
 * Based on arch/arm/include/asm/pgalloc.h
 *
 * Copyright (C) 2000-2001 Russell King
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __ASM_PGALLOC_H
#define __ASM_PGALLOC_H

#include <asm/pgtable-hwdef.h>
#include <asm/processor.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>

#define check_pgt_cache()		do { } while (0)

#ifndef CONFIG_ARM64_64K_PAGES

static inline pmd_t *pmd_alloc_one(struct mm_struct *mm, unsigned long addr)
{
#ifndef CONFIG_SPRD_PAGERECORDER
	return (pmd_t *)get_zeroed_page(GFP_KERNEL | __GFP_REPEAT);
#else
	return (pmd_t *)get_zeroed_page_nopagedebug(GFP_KERNEL | __GFP_REPEAT);
#endif
}

static inline void pmd_free(struct mm_struct *mm, pmd_t *pmd)
{
	BUG_ON((unsigned long)pmd & (PAGE_SIZE-1));
#ifndef CONFIG_SPRD_PAGERECORDER
	free_page((unsigned long)pmd);
#else
	free_page_nopagedebug((unsigned long)pmd);
#endif
}

static inline void pud_populate(struct mm_struct *mm, pud_t *pud, pmd_t *pmd)
{
	set_pud(pud, __pud(__pa(pmd) | PMD_TYPE_TABLE));
}

#endif	/* CONFIG_ARM64_64K_PAGES */

extern pgd_t *pgd_alloc(struct mm_struct *mm);
extern void pgd_free(struct mm_struct *mm, pgd_t *pgd);

#define PGALLOC_GFP	(GFP_KERNEL | __GFP_NOTRACK | __GFP_REPEAT | __GFP_ZERO)

static inline pte_t *
pte_alloc_one_kernel(struct mm_struct *mm, unsigned long addr)
{
#ifndef CONFIG_SPRD_PAGERECORDER
	return (pte_t *)__get_free_page(PGALLOC_GFP);
#else
	return (pte_t *)__get_free_page_nopagedebug(PGALLOC_GFP);
#endif
}

static inline pgtable_t
pte_alloc_one(struct mm_struct *mm, unsigned long addr)
{
	struct page *pte;

#ifndef CONFIG_SPRD_PAGERECORDER
	pte = alloc_pages(PGALLOC_GFP, 0);
#else
	pte = alloc_pages_nopagedebug(PGALLOC_GFP, 0);
#endif
	if (pte)
		pgtable_page_ctor(pte);

	return pte;
}

/*
 * Free a PTE table.
 */
static inline void pte_free_kernel(struct mm_struct *mm, pte_t *pte)
{
	if (pte)
#ifndef CONFIG_SPRD_PAGERECORDER
		free_page((unsigned long)pte);
#else
		free_page_nopagedebug((unsigned long)pte);
#endif
}

static inline void pte_free(struct mm_struct *mm, pgtable_t pte)
{
	pgtable_page_dtor(pte);
#ifndef CONFIG_SPRD_PAGERECORDER
	__free_page(pte);
#else
	__free_page_nopagedebug(pte);
#endif
}

static inline void __pmd_populate(pmd_t *pmdp, phys_addr_t pte,
				  pmdval_t prot)
{
	set_pmd(pmdp, __pmd(pte | prot));
}

/*
 * Populate the pmdp entry with a pointer to the pte.  This pmd is part
 * of the mm address space.
 */
static inline void
pmd_populate_kernel(struct mm_struct *mm, pmd_t *pmdp, pte_t *ptep)
{
	/*
	 * The pmd must be loaded with the physical address of the PTE table
	 */
	__pmd_populate(pmdp, __pa(ptep), PMD_TYPE_TABLE);
}

static inline void
pmd_populate(struct mm_struct *mm, pmd_t *pmdp, pgtable_t ptep)
{
	__pmd_populate(pmdp, page_to_phys(ptep), PMD_TYPE_TABLE);
}
#define pmd_pgtable(pmd) pmd_page(pmd)

#endif
