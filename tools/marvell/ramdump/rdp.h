/*------------------------------------------------------------
(C) Copyright [2006-2008] Marvell International Ltd.
All Rights Reserved
------------------------------------------------------------*/
#ifndef _RDP_H_
#define _RDP_H_
typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned long UINT32;

#ifndef WIN32
typedef unsigned long long u64;
#else
typedef __int64 u64;
#endif
typedef u64 __u64;
typedef unsigned u32;
typedef unsigned long T_PTR;
static inline unsigned hi32(u64 x) { return (unsigned)(x>>32); }
static inline unsigned lo32(u64 x) { return (unsigned)(x); }
static inline u64 mk64(unsigned hi, unsigned lo) { return (((u64)hi)<<32) | lo; }

#include <stdio.h>
#include <stdlib.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>


#define _ONPC_ // for eehandler.h - replace enums with UINT8 as MSVC allocates 32-bit for enums
#include "ramdump_defs.h"
#include "EEHandler.h"
#include "ramdump_data.h"

/*
 * An array of the following structures is used to describe the mapping of target physical address space
 * into the dump file.
 * This information is built based on RDC and apriory known memory map for a bin dump, or based on the dump ELF header.
 */
struct segment {
	unsigned pa; /* physical address */
	unsigned foffs; /* file offset */
	unsigned size; /* size */
	u64		 va;
};
#define MAX_SEGMENTS 4
extern struct segment segments[MAX_SEGMENTS];

static inline unsigned FILE_OFFSET(unsigned pa)
{
	int i;
	for (i = 0; i < MAX_SEGMENTS; i++)
		if ((pa >= segments[i].pa) && (pa < (segments[i].pa + segments[i].size)))
			return (pa - segments[i].pa) + segments[i].foffs;
	return 0xffffffff; /* bail out on fseek() failure */
}
#define DUMP_IS_ELF (segments[0].foffs)

#define RDC_FILE_OFFSET(size) FILE_OFFSET(RDC_ADDRESS(size))
#define CP_DDR_RW_OFFSET    0           // 0x00300000 for MSA
#define CP_DDR_RW_SIZE      0x01300000  // MSA_3MB + COM_16MB = 19MB
#define MAX_POSTMORTEM_FILE 0x1300000   // com_DDR_RW_Full is 19MB

#define AP_SRAM_ADDRESS 0x5c000000

#if defined(WIN32)
#define PATH_SLASH '\\'
#else
#define PATH_SLASH '/'
#endif


extern FILE* rdplog;

extern enum aarch_type_e {
	aarch32,
	aarch64
} aarch_type;
// Functions

// Read an object contents at specified kernel virtual address and of specified size
int readObjectAtVa(FILE *fin, void *buf, u64 addr, int size);

// Translate a kernel virtual address into physical one (use FILE_OFFSET(v2p(va, fin)) for file offset)
#define BADPA 0xffffffff
unsigned v2p(u64 va, FILE *fin);

// Extract RAMDUMP area into an output file
// offset: if vaddr=0, area located at this file offset
// vaddr: if given, area located at this kernel virtual address; if offset is also non-0, vaddr will be checked to match it.
// size:  size in bytes
// flexSize: 0= the inFile should have the exact "size", 1= iFile could be smaller than "size"
// append: if 1, the contents will be appended to the output file; otherwise output file is written at offset 0.
int extractFile(const char* inName, const char* outShortName, FILE* fin,
	unsigned offset, unsigned size, int flexSize, u64 vaddr,
	int append = 0);


// File name operations
char* changeNameExt(const char* inName, const char* nameext);
char* changeExt(const char* inName, const char* ext);
const char* getExt(const char* inName);

// Public stream for diagnostic and progress indications
extern FILE* rdplog;
extern const char *rdpPath; // path-name of the RDP executable from command line (use to locate config-files or like).


void *init_elf(int *elf_size);
int check_elf(FILE *fin);
#endif
