/*
 *  linux/arch/arm/mach-mmp/ramdump.h
 *
 *  Support for the Marvell PXA RAMDUMP error handling capability.
 *
 *  Author:     Anton Eidelman (anton.eidelman@marvell.com)
 *  Created:    May 20, 2010
 *  Copyright:  (C) Copyright 2006 Marvell International Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  publishhed by the Free Software Foundation.
 */

#ifndef _RAMDUMP_H
#define _RAMDUMP_H

#include <linux/ptrace.h> /*pt_regs*/

/*
 * Error_id values:
 * [0xffff..0]  is used for ARM die() err codes, see fault.c/trapc.c
 */
#define RAMDUMP_ERR_EEH_CP 0x80000000
#define RAMDUMP_ERR_EEH_AP 0x80000100
#define RAMDUMP_ERR_NONE   0x8FFFFFFF

void ramdump_save_dynamic_context(const char *str, int err,
			struct thread_info *thread, struct pt_regs *regs);
void ramdump_save_panic_text(const char *str);
void ramdump_prepare_in_advance(void);
void ramdump_rdc_reset(void);
void ramdump_panic(void);
#define RAMDUMP_ERR_STR_LEN 100

/* RAMFILE related definitions and functions */

/* RAMFILE object descriptor */
#define RAMFILE_PHYCONT 1 /* physical memory is continuous (kmalloc) */
struct ramfile_desc {
	unsigned next; /* next object (pa) or NULL */
	unsigned payload_size; /* bytes, excluding this header */
	unsigned flags;
	unsigned vaddr; /* virtual start address for discontinous case */
	unsigned vaddr_hi; /* upper 32 bit of the above, which is misaligned */
	unsigned reserved[3];
};

int ramdump_attach_ramfile(struct ramfile_desc *desc);


/* RDI related definitions and functions */
enum rdi_type {
	RDI_NONE, /* End of list */
	RDI_CBUF, /* virtual addr, size, opt current idx/ptr; refs supported */
	RDI_PBUF, /* physical addr, size in bytes; no refs */
	RDI_CUST, /* Custom: no 1st level parser for this */
};

/* Attrbits: OR these: for each data word tells if it's an address or a value */
#define RDI_ATTR_VAL(wordnum)	(0<<(wordnum))
#define RDI_ATTR_ADDR(wordnum)	(1<<(wordnum))

/*
 * RDI (data item) generic object attachment:
 *	type: for unknown types rdp will just print the data words suplied;
 *	name: string used by rdp as filename, at most MAX_RDI_NAME chars;
 *	attrbits: 1 bit per data word, 0=value, 1=address of a value;
 *		Callers can assume "value" is the default (0).
 *	nwords: number of data words, max 16;
 *	unsigned long data elements follow (32-bit or 64-bit)
 */
int ramdump_attach_item(enum rdi_type type,
			const char *name,
			unsigned attrbits,
			unsigned nwords, ...);

/*
 * Buffer (RDI_CBUF) type RDI object attachment:
 *	name:  string used by rdp as filename, at most MAX_RDI_NAME chars;
 *	buf: address of the pointer to the buffer;
 *	buf_size: address of an unsigned containing buffer size;
 *		OR an address of a pointer to the buffer end;
 *		rdp will automatically figure out which;
 *	cur_ptr: address of a pointer to the current byte inside the buffer;
 *		OR an address of an unsigned containing the current byte index;
 *		OR an address of an unsigned containing the total byte count
 *		(with no wrap-around);
 *		rdp will automatically figure out which;
 *	unit:	unit buf_size (and cur_ptr if is an index) are expressed in.
 */
static inline
int ramdump_attach_cbuffer(const char	*name,
			void		**buf,
			unsigned	*buf_size,
			unsigned	*cur_ptr,
			unsigned	unit)
{
	return ramdump_attach_item(RDI_CBUF, name,
		RDI_ATTR_ADDR(0) | RDI_ATTR_ADDR(1)
		| RDI_ATTR_ADDR(2), 4,
		(unsigned long)buf, (unsigned long)buf_size,
		(unsigned long)cur_ptr, (unsigned long)unit);
}

static inline
int ramdump_attach_pbuffer(const char	*name,
			unsigned	buf_physical,
			unsigned	buf_size)
{
	return ramdump_attach_item(RDI_PBUF, name,
		RDI_ATTR_VAL(0) | RDI_ATTR_VAL(1), 2,
		(unsigned)buf_physical, (unsigned)buf_size);
}

/* Configuration */
#define RAMDUMP_PHASE_1_1 /* allow KO's to work with older kernel */
#define RAMDUMP_RDI		/* allow detecting RDI interface is present */

#endif
