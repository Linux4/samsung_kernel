/*
 *  linux/arch/arm/mach-pxa/ramdump_defs.h
 *
 *  Support for the Marvell PXA RAMDUMP error handling capability.
 *
 *  Author:     Anton Eidelman (anton.eidelman@marvell.com)
 *  Created:    May 20, 2010
 *  Copyright:  (C) Copyright 2006 Marvell International Ltd.
 *
 */
/*******************************************************************************
Copyright (C) Marvell International Ltd. and its affiliates

This software file (the "File") is owned and distributed by Marvell
International Ltd. and/or its affiliates ("Marvell") under the following
alternative licensing terms.  Once you have made an election to distribute the
File under one of the following license alternatives, please (i) delete this
introductory statement regarding license alternatives, (ii) delete the two
license alternatives that you have not elected to use and (iii) preserve the
Marvell copyright notice above.

********************************************************************************
Marvell Commercial License Option

If you received this File from Marvell and you have entered into a commercial
license agreement (a "Commercial License") with Marvell, the File is licensed
to you under the terms of the applicable Commercial License.

********************************************************************************
Marvell GPL License Option

If you received this File from Marvell, you may opt to use, redistribute and/or
modify this File in accordance with the terms and conditions of the General
Public License Version 2, June 1991 (the "GPL License"), a copy of which is
available along with the File in the license.txt file or by writing to the Free
Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or
on the worldwide web at http://www.gnu.org/licenses/gpl.txt.

THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED
WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
DISCLAIMED.  The GPL License provides additional details about this warranty
disclaimer.
********************************************************************************
Marvell BSD License Option

If you received this File from Marvell, you may opt to use, redistribute and/or
modify this File under the following licensing terms.
Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    *   Redistributions of source code must retain the above copyright notice,
	this list of conditions and the following disclaimer.

    *   Redistributions in binary form must reproduce the above copyright
	notice, this list of conditions and the following disclaimer in the
	documentation and/or other materials provided with the distribution.

    *   Neither the name of Marvell nor the names of its contributors may be
	used to endorse or promote products derived from this software without
	specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

#ifndef ARCH_ARM_MACH_PXA_RAMDUMP_DEFS_H
#define ARCH_ARM_MACH_PXA_RAMDUMP_DEFS_H
/************************************************************************/
/*				RAMDUMP definitions			*/
/************************************************************************/
/* RDC header is at fixed address adjacent to the top of DDR space */
#define RDC_SIGNATURE   0x41434452 /* ascii(RDCA), little endian */
#define RDC1_SIGNATURE  0x31434452 /* ascii(RDC1), little endian */
#define RDC_OFFSET	0x002FFC00 /* from CP area start */
#define ISRAM_PA	0x5c000000

/* Legacy Tavor memory map addresses */
#define DDR0_BASE 0x80000000
#define DDR1_BASE 0xc0000000
#define CP_AREA_SIZE 0x01000000 /* fixed top 16MB of bank 0 */
#define CP_AREA_SIZE_MAX 0x02000000 /* up to 32MB for now (no physical restriction) */

#define CP_ADDRESS(ddr0_size) (segments[0].pa+(ddr0_size)-CP_AREA_SIZE)
#define RDC_ADDRESS(ddr0_size) (CP_ADDRESS(ddr0_size)+RDC_OFFSET)

/* ISRAM dump is located before RDC header. More objects can be added here.
	Not included into the struct rdc_area as the size of isram and
	other objects might not be known at compile time.
	Macro's below may use function calls or variable references.*/
#define RDC_ISRAM_START(header, isram_size) \
	((unsigned)(((unsigned)header)-isram_size))
/* RAMFILE object descriptor */
#define RAMFILE_PHYCONT 1 /* physical memory is continuous (kmalloc) */
struct ramfile_desc {
	unsigned next; /* next object (pa) or NULL */
	unsigned payload_size; /* bytes, excluding this header */
	unsigned flags;
	unsigned vaddr; /* virtual start address for discontinous case */
	unsigned vaddr_hi; /* upper 32 bit of the above (misaligned) */
	unsigned reserved[3];
};

enum rdi_type {
	RDI_NONE, /* End of list */
	RDI_CBUF, /* virtual addr, size, opt current idx/ptr; refs supported */
	RDI_PBUF, /* physical addr, size in bytes; no refs */
	RDI_CUST, /* Custom: no 1st level parser for this */
};

/* Attrbits: OR these: for each data word tells if it's an address or a value */
#define RDI_ATTR_VAL(wordnum)	(0<<(wordnum))
#define RDI_ATTR_ADDR(wordnum)	(1<<(wordnum))

/* Data Item objects in RDC */
#define MAX_RDI_NAME	8
#define RDI_HEAD_SIZE	12
struct rdc_dataitem {
	unsigned char	size;	/* total bytes to the next item */
	unsigned char	type;	/* one of enum rdc_di_type */
	unsigned short	attrbits;/* bit per body element, lsb first */
	char 		name[MAX_RDI_NAME];/* 8-char ascii item name */
	union {
		unsigned w[1];	/* contents */
	} body;
};

/* RDC: the location is fixed at RDC_ADDRESS. Size is 1KB. */
struct rdc_area {
	struct rdc_header {
		unsigned signature;
		unsigned kernel_build_id;
		unsigned error_id;
		unsigned reserved[5];
		unsigned ramdump_data_addr; /* physical addr of ramdump_data */
		unsigned isram_pa; /* physical addr of ISRAM dump */
		unsigned isram_size; /* size of ISRAM dump */
		unsigned isram_crc32; /* verify contents survived flush/reset */
		unsigned ramfile_addr; /* physical addr of the first or NULL */
		/* mipsram is here so it can be extracted without symbol table */
		unsigned mipsram_pa; /* physical addr of mipsram buffer */
		unsigned mipsram_size; /* size of mipsram buffer */
		unsigned ddr_bank0_size; /* for future use in RDP */
		unsigned pgd; /* init_mm.pgd  pa: translate vmalloc addresses */
		unsigned kallsyms; /* VA of struct kallsyms_record */
		unsigned kallsyms_hi; /* upper 32 bit of the above */
		unsigned kernel_build_id_hi; /* upper 32 bit of the above */
		unsigned reserved1[12]; /* Up to offset 0x80 */
	} header;
	union { /*upto 1KB*/
		unsigned char space[0x400-sizeof(struct rdc_header)];
		struct rdc_dataitem rdi[1];
	} body;
};

/* use this for debug and memory consistency checking.
   Note: CRC32 no 2-s complement option is used */
#define RAMDUMP_ISRAM_CRC_CHECK

/* Error_id values:
[0xffff..0]  is used for ARM die() err codes, see fault.c/trapc.c */
#define RAMDUMP_ERR_EEH_CP 0x80000000
#define RAMDUMP_ERR_EEH_AP 0x80000100
#define RAMDUMP_ERR_NONE   0x8FFFFFFF

#ifndef _ONPC_
/* Functions: */
int ramdump_attach_ramfile(struct ramfile_desc *desc);

/* RDI (data item) generic object attachment:
	type: for unknown types rdp will just print the data words suplied;
	name: string used by rdp as filename, at most MAX_RDI_NAME chars;
	attrbits: 1 bit per data word, 0=value, 1=address of a value;
		Callers can assume "value" is the default (0).
	nwords: number of data words, max 16;
	32-bit data words follow
*/
int ramdump_attach_item(enum rdi_type type,
			const char *name,
			unsigned attrbits,
			unsigned nwords,...);

/* Buffer (RDI_CBUF) type RDI object attachment:
	name:  string used by rdp as filename, at most MAX_RDI_NAME chars;
	buf: address of the pointer to the buffer;
	buf_size: address of an unsigned containing buffer size;
		OR an address of a pointer to the buffer end;
		rdp will automatically figure out which;
	cur_ptr: address of a pointer to the current byte inside the buffer;
		OR an address of an unsigned containing the current byte index;
		OR an address of an unsigned containing the total byte count
		(with no wrap-around);
		rdp will automatically figure out which;
	unit:	unit buf_size (and cur_ptr if is an index) are expressed in.
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
		(unsigned)buf, (unsigned)buf_size, (unsigned)cur_ptr, unit);
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

#endif /* _ONPC_ */
#define RAMDUMP_PHASE_1_1 /* allow KO's to work with older kernel */
#endif
