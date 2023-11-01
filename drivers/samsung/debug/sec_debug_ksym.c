/*
 * sec_debug_ksym.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/sec_debug.h>
#include "sec_debug_internal.h"

#ifdef CONFIG_KALLSYMS_ALL
#define all_var 1
#else
#define all_var 0
#endif
/*
 * These will be re-linked against their real values
 * during the second link stage.
 */
extern const unsigned long kallsyms_addresses[] __weak;
extern const int kallsyms_offsets[] __weak;
extern const u8 kallsyms_names[] __weak;

/*
 * Tell the compiler that the count isn't in the small data section if the arch
 * has one (eg: FRV).
 */
extern const unsigned long kallsyms_num_syms
__attribute__((weak, section(".rodata")));

extern const unsigned long kallsyms_relative_base
__attribute__((weak, section(".rodata")));

extern const u8 kallsyms_token_table[] __weak;
extern const u16 kallsyms_token_index[] __weak;

extern const unsigned long kallsyms_markers[] __weak;

void secdbg_base_set_kallsyms_info(struct sec_debug_ksyms *ksyms, int magic)
{
	if (!IS_ENABLED(CONFIG_KALLSYMS_BASE_RELATIVE)) {
		ksyms->addresses_pa = __pa(kallsyms_addresses);
		ksyms->relative_base = 0x0;
		ksyms->offsets_pa = 0x0;
	} else {
		ksyms->addresses_pa = 0x0;
		ksyms->relative_base = (uint64_t)kallsyms_relative_base;
		ksyms->offsets_pa = __pa(kallsyms_offsets);
	}

	ksyms->names_pa = __pa(kallsyms_names);
	ksyms->num_syms = kallsyms_num_syms;
	ksyms->token_table_pa = __pa(kallsyms_token_table);
	ksyms->token_index_pa = __pa(kallsyms_token_index);
	ksyms->markers_pa = __pa(kallsyms_markers);

	ksyms->sect.sinittext = (uint64_t)_sinittext;
	ksyms->sect.einittext = (uint64_t)_einittext;
	ksyms->sect.stext = (uint64_t)_stext;
	ksyms->sect.etext = (uint64_t)_etext;
	ksyms->sect.end = (uint64_t)_end;

	ksyms->kallsyms_all = all_var;

	ksyms->magic = magic;
	ksyms->kimage_voffset = kimage_voffset;
}
