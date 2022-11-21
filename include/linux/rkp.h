/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __RKP_H__
#define __RKP_H__

#ifndef __ASSEMBLY__
#include <asm/stack_pointer.h>
#include <asm/thread_info.h>
#include <linux/spinlock.h>
#include <linux/fastuh.h>

#ifdef CONFIG_RKP_TEST
#define RKP_INIT_MAGIC		0x5afe0002
#else
#define RKP_INIT_MAGIC		0x5afe0001
#endif

#define __rkp_ro __section(.rkp_ro)

enum __RKP_CMD_ID {
	RKP_START = 0x00,
	RKP_DEFERRED_START = 0x01,
	/* RKP robuffer cmds*/
#ifdef CONFIG_RKP_TEST
	RKP_ROBUFFER_ALLOC = 0x10,
	RKP_ROBUFFER_FREE = 0x11,
#endif
	RKP_PGD_RO = 0x12,
	RKP_PGD_RWX = 0x13,
	RKP_GET_RO_BUFFER = 0x15,
	RKP_MODULE_LOAD = 0x21,
#ifdef CONFIG_RKP_TEST
	CMD_ID_TEST_GET_PAR = 0x41,
	CMD_ID_TEST_GET_RO = 0x43,
#endif
};

#define RKP_MODULE_PXN_CLEAR	0x1
#define RKP_MODULE_PXN_SET		0x2

struct rkp_init { //copy from uh (app/rkp/rkp.h)
	u32 magic;
	u64 vmalloc_start;
	u64 vmalloc_end;
	u64 init_mm_pgd;
	u64 id_map_pgd;
	u64 zero_pg_addr;
	u64 rkp_pgt_bitmap;
	u64 rkp_dbl_bitmap;
	u32 rkp_bitmap_size;
	u32 no_fimc_verify;
	u64 fimc_phys_addr;
	u64 _text;
	u64 _etext;
	u64 extra_memory_addr;
	u32 extra_memory_size;
	u64 physmap_addr; //not used. what is this for?
	u64 _srodata;
	u64 _erodata;
	u32 large_memory;
	u64 tramp_pgd;
	u64 tramp_valias;
};

struct module_info {
	u64 base_va;
	u64 vm_size;
	u64 core_base_va;
	u64 core_text_size;
	u64 core_ro_size;
	u64 init_base_va;
	u64 init_text_size;
};

extern bool rkp_started;

extern void __init rkp_init(void);
extern void rkp_deferred_init(void);
extern void rkp_robuffer_init(void);

extern inline phys_addr_t rkp_ro_alloc_phys(int shift);
extern inline void *rkp_ro_alloc(void);
extern inline void rkp_ro_free(void *free_addr);
extern inline bool is_rkp_ro_buffer(u64 addr);

#endif //__ASSEMBLY__
#endif //__RKP_H__
