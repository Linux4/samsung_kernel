/*------------------------------------------------------------
(C) Copyright [2006-2008] Marvell International Ltd.
All Rights Reserved
------------------------------------------------------------*/

/* Revision history:
	0.1 (antone@marvell.com): Implemented basic RAMDUMP file format parsing and CP data extraction.
	0.2 (antone@marvell.com): Fixed bugs.
	0.3 (antone@marvell.com): Added T32 scipt generation for MMU-enabled Linux aware analysis in simulator.
	0.4 (antone@marvell.com): Removed splitting of RAMDUMP file into separate files per DDR bank; updated T32 script accordingly.
	0.5 (antone@marvell.com): Moved ELF and BIN file loading in ramdump.cmm before setting register values. Reason: ELF resets PC.
	                          Continue when RAMFILE list is broken (happens when RAMDUMP.gz CRC error destroys the 2nd bank image).
							  Added rdp.log that captures RDP printouts during parsing.
	0.6 (antone@marvell.com): ramdump.cmm: added PC register initialization (was missing).
							  ramdump.cmm: added commands to dump printk buffer contents to a text file.
							  ramdump.cmm: added command to open ramdump_data struct for viewing.
	0.7 (antone@marvell.com): ramdump.cmm: fixed v.value(log_buf) syntax so it is accepted by all T32 versions.
							  ramdump.cmm: added support for path/names with spaces.
							  ramdump.cmm: added check of kernel version string (linux_banner) in vmlinux vs ramdump. Warns on mismatch.
							  ramdump.cmm: disabled printk file generation on mismatch - could result in very long file due to wrong buf_len value.
	0.8 (antone@marvell.com): Added support for Nevo memory map (RDC in CP area locate at the lowest 16MB of DDR bank#0).
							  Fixed robustness issues to guarantee proper behavior in unexpected cases,
							  e.g. garbled CP objects when running in AP-only configuration.
							  Fixed bug in ramdump_data (kernel bss) analysis when it's located outside the first 16MB of DDR.
	0.9 (antone@marvell.com): ramdump.cmm: added sim.cache.on in order to enable MMU translation in instruction set simulator.
							  This enables execution of the code in virtual address spaces (single stepping etc).
							  ramdump.cmm: changed "sys.cpu ARM1176JZ" to "sys.cpu 88AP955". ARM1176JZ does not support thumb-2
							  (T2), which is used in pxa95x code, consequently simulator failed to parse user space code and
							  failed to present user space stack frames.
							  ramdump.cmm: added mmu.off after sys.up to enable re-running ramdump.cmm in the same T32 session.
							  ramdump.cmm: Added loading of AP SRAM in simulator.
							  T32 scripts: added T32 folder for auxiliary scripts supplied for use in simulator.
							  Added rd_print_tasks.cmm: finds and prints all the tasks AND SUB-THREADS in the system.
							  The task_struct address printed can be used to identify the task to simulator, e.g.
							  to switch to the task context etc.
	1.0 (antone@marvell.com): Corrected CP post-mortem data extraction. Since TAI is locked, the load-table cannot be accessed at CP RO DDR start.
							  The RW copy of load-table is therefore looked up.
	1.1 (antone@marvell.com): ramdump.cmm: added verification of kernel .text in the dump against the vmlinux file: detects corruptions in DDR.
							  ramdump.cmm: moved cpsr before other registers.
	1.2 (antone@marvell.com): Ramfile objects: added detection of aligned data block duplication,
							  a known issue in some dump ports, which overwrite the correct data
							  with a copy of previous data, normally 4KB aligned, and cause
							  CRC errors when uncompressing ramfile.*.tgz. When detected, rdplog.txt
							  will include a warning with location/size of the block.
							  Warning fix in ramdump.cmm generation: output not affected.
	1.3 (antone@marvell.com): ramdump.cmm: Fix CPU type detection: Cortex-A9 ID was erroneously recognized as XScale (non-PJ4).
	1.4 (antone@marvell.com): ramdump.cmm: cmm variables &log_path and &vmlinux_path defined for easy migration of working
						      directory to another location.
							  ramdump.cmm: added loading of com_EXTRA_DDR.bin if exists (user to extract this from ramfile's).
	1.5 (antone@marvell.com): Eshel: wider range for loadtable search (was 0xd0400000, now 0xd0000000), so on eshel it finds the thing.
							  Eshel: reduce bank0 size if the file is smaller and no explicit size is found in RDC (u-boot would set this,
							  but on Eshel dump might be taken using SWD, before u-boot runs).
	1.6 (antone@marvell.com): Ramfile objects: add support for non-continuous physical memory (vmalloc).
							  com_DDR_RW.bin: take addr/size (were hardcoded) from CP LT, so it's works on all CP memory maps.
	1.7 (antone@marvell.com): Generate ramdump.cmm even when ramdump_data is not avilable (e.g. dump after force reset).
							  In this case use init_mm primary MMU table, which enables full kernel space visibility.
							  Added recovery of kernel RO segment contents for Eshel memory map (obm/u-boot overwrite this area).
							  Also, fixed typo in ramdump.cmm: NMRR register name in the comment.
							  Added LT fields sanity check for CP DDR RW addresses.
	1.8 (antone@marvell.com): Remove excessive sanity check in extractPostmortemfiles() to allow objects located below RDC.
							  This is required to support MSA objects for PXA1801 memory map,
							  where RDC is located at the end of MSA DDR area.
	2.0 (antone@marvell.com): Added RDI mechanism that allows objects to be registered with RD on target so RDP can find these objects
							  in the dump (no kernel symbol table needed), and extract/parse them.
	2.1 (antone@marvell.com): Added support for kernel version string pointer in RDC header kernel_build_id field.
	2.2 (antone@marvell.com): Fixed Linux build (now compiles and works on Linux, use build_rdp_linux.sh).
	2.3 (antone@marvell.com): Revised pmlog.cpp to support event db default from pmlog_app_db.txt file and builtin default.
							  Fixed Linux build of pmlog.cpp
	2.4 (ymarkman@marvell.com): Allow addresses 0x40xxxxxx but not only 0xD0xxxxxx
	3.0 (ymarkman@marvell.com): If RDC not present, try to parse COMM-ONLY:
	                          look for COMM's LOAD_TABLE_SIGNATURE_STR and extractPostmortemfiles()
							  TBD: search LOAD_TABLE_SIGNATURE_STR in bad RAMDUMP...
	3.1 (ymarkman@marvell.com): 1). Do NOT abort the COMM Postmortem parsing if one of entry has no appropriated space
                                but continue and go over others. 2). Look for SIGNATURE in file including 3MB MSA preamble.
	3.2 (ymarkman@marvell.com): Create com_DDR_RW_Full.bin from RAMDUMP including MSA
	3.3 (ymarkman@marvell.com): add to ramdump.cmm script printer selection "ClipBoard" instead of default "printer"
	3.4 (ymarkman@marvell.com): cmm script is wrong with \n, \r inside. Replace them with space
	3.5 (ymarkman@marvell.com): expand COMM RW DDR to 19MB
	3.6 (ymarkman@marvell.com): Detect alternative signature 'RDC1' created by uboot to avoid multiple ramdump
	4.0 (ymarkman@marvell.com): DDR0_BASE depending upon RDC and may be 0x0 or 0x80000000;
	                            Always try the best - don't abort the parsing but continue on error (DEF_CONTINUE_ON_ERROR)
	4.1 (antone@marvell.com):   Fixed linux build
	4.2 (antone@marvell.com):	Added support of EMMD ELF format input files,
								and revised the way target physical addresses are mapped into the file, see segments[].
								Added generation of EMMD ELF header emmd_elf_head.bin for binary dump input file,
								to be concatenated with the dump for use with the crash utility.
	4.3 (antone@marvell.com): Fixed writeElfHeader.
	4.4 (antone@marvell.com): Allow detection of the DDR physical address based on RDC header pgd field.
								The RDC header ramdump_data_addr field is 0 when the dump is forced, and RDP code
								uses this to detect a force dump. pgd (set on ramdump init) is more likely to be present,
								and if not, it can be set manually in the dump file, while RDP still will detect forced dump.
	4.5 (antone@marvell.com): Fix PHY_OFFSET to allow more than 16MB CP DDR: clear [31:28] only.
	5.0 (antone@marvell.com): ARM64 support. From now on arm32 and arm64 are supported. Detection based on ELF header.
	5.1 (antone@marvell.com): Generate a cmm even when RDC is not found
		based on ELF header (good for EMMD logs).
	5.2 (antone@marvell.com): Fix ELF32 parsing bug introduced in 5.0.
		Use linux-3.x for arm64 only.
	5.3 (antone@marvell.com): Fix ELF64 parsing bug introduced in 5.2.
		Fix RDI CBUF parsing with cur wrap case.
	5.4 (antone@marvell.com): Zero RDC content when RDC detection
		fails to avoid failures due to random content in it.
*/

#define VERSION_STRING "5.4"

#include <stdio.h>
#include "rdp.h"

// Always try the best - don't abort the parsing but continue on error
#define DEF_CONTINUE_ON_ERROR

#define COM_DDR_RW_BIN_NAME         "com_DDR_RW.bin"
#define COM_DDR_RW_FULL_BIN_NAME    "com_DDR_RW_Full.bin"
#define MSA_DDR_SIZE                (3*1024*1024)

const char *apSramFileName = "ap_SRAM.bin";
const char *comExtraDdrFileName = "com_EXTRA_DDR.bin";

FILE* rdplog;
const char *rdpPath;
enum aarch_type_e aarch_type;

char* changeExt(const char* inName,const char* ext)
{
	char* outName=(char*)malloc(strlen(inName)+strlen(ext)+1+1); //+ext+'.'+\0
	int i,exti;
	if(outName)
	{
	  for(i=0, exti=-1;inName[i];i++)
	  {
		  if((outName[i]=inName[i])=='.') exti=i;
	  }

	  if(exti<0) //no dot
	  {
		  exti=i;
		  outName[exti]='.';
	  }
	  strcpy(&outName[exti+1],ext);
	}
    return outName;
}

// Inherit the path
char* changeNameExt(const char* inName,const char* nameext)
{
	int len;
	const char *p;
	const char *pp=0;

	// Find path last '\'
	for(p=inName; p=strchr(p,PATH_SLASH); pp=++p);
	len = pp?pp-inName:0;

	char* outName=(char*)malloc(strlen(nameext)+len+1); // +\0

	if(outName)
	{
		strncpy(outName, inName, len);
		outName[len]=0;
		strcat(outName, nameext);
	}
    return outName;
}

const char* getExt(const char* inName)
{
	int i,exti;
	  for(i=0, exti=-1;inName[i];i++)
	  {
		  if(inName[i]=='.') exti=i;
	  }

	  if(exti<0) //no dot
	  {
		  exti=i;
	  }
	  else
	  {
		  exti++;
	  }
    return &inName[exti];
}

#define PAGE_SIZE 0x1000
#define MAXBUF PAGE_SIZE /* One MMU page */
unsigned char buffer[MAXBUF];

struct rdc_area rdc;
unsigned dump_total_size;
unsigned cpmem_addr;
unsigned rdc_addr;
struct segment segments[MAX_SEGMENTS];

static int checkRdc(FILE* in, unsigned bank0size)
{
	unsigned rdcAddr = RDC_FILE_OFFSET(bank0size);
	unsigned ddr0_base;

	if(fseek(in, rdcAddr, SEEK_SET)) return -1;
	if(fread((void*)&rdc, sizeof(rdc), 1, in) != 1) return -1;
	if ((rdc.header.signature != RDC_SIGNATURE) &&
		(rdc.header.signature != RDC1_SIGNATURE))
		return -1;

	// There are 2 fields could be used for detection:
	//  ramdump_data_addr - physical addr of ramdump_data
	//  pgd               - init_mm.pgd  pa: translate vmalloc addresses
	if (rdc.header.ramdump_data_addr)
		ddr0_base = rdc.header.ramdump_data_addr & 0xC0000000;
	else if (rdc.header.pgd) /* ramdump_data_addr is set when dump is triggered, so it's 0 on a dump forced after reset */
		ddr0_base = rdc.header.pgd & 0xC0000000;
	else
		ddr0_base = 0x80000000; /* Default for backwards compatibility */

	if (!DUMP_IS_ELF) {
		/* Not ELF */
		segments[0].pa = ddr0_base;
		/* 16MB is not the real bank size, but an indication that this RD is taken on Nevo,
			where CP is located at the bottom (offset 0) of DDR bank#0.
			In this case RDC location cannot be used to detect the real DDR bank size.
			Therefore: check if RDC already contains the ddr_bank0_size field, otherwise assume default 256MB.
		*/
		segments[0].size = bank0size;
		if (bank0size == CP_AREA_SIZE)
			segments[0].size = rdc.header.ddr_bank0_size ? rdc.header.ddr_bank0_size : 0x10000000;
	} else {
		/* ELF: check consistency */
		if (segments[0].pa != ddr0_base)
			fprintf(rdplog, "WARNING: DDR address in the ELF header (0x%.8x) does not match the one in RDC (0x%.8x)\n",
					segments[0].pa, ddr0_base);
	}

	cpmem_addr = CP_ADDRESS(bank0size);
	rdc_addr = RDC_ADDRESS(bank0size);

/***
	printf("==============================\n");
    printf("signature         %08x\n", rdc.header.signature        );
    printf("ramdump_data_addr %08x\n", rdc.header.ramdump_data_addr);
    printf("ramfile_addr      %08x\n", rdc.header.ramfile_addr     );
    printf("mipsram_pa        %08x\n", rdc.header.mipsram_pa       );
    printf("mipsram_size      %08x\n", rdc.header.mipsram_size     );
    printf("ddr_bank0_size    %08x\n", rdc.header.ddr_bank0_size   );
    printf("pgd               %08x\n", rdc.header.pgd              );
	printf("------------------------------\n");
    printf("ddr0_base         %08x\n", ddr0_base );
    printf("bank0size         %08x\n", bank0size );
    printf("cpmem_addr        %08x\n", cpmem_addr);
	printf("==============================\n");
***/
	return 0;
}

static int checkRdc_alt(FILE* in, unsigned addr)
{
	unsigned rdcAddr = FILE_OFFSET(addr);
	unsigned ddr0_base;

	if(fseek(in, rdcAddr, SEEK_SET)) return -1;
	if(fread((void*)&rdc, sizeof(rdc), 1, in) != 1) return -1;
	if ((rdc.header.signature != RDC_SIGNATURE) &&
		(rdc.header.signature != RDC1_SIGNATURE))
		return -1;

	ddr0_base = 0; /* Default for new arch's */

	if (!DUMP_IS_ELF) {
		/* Not ELF */
		fprintf(rdplog, "ERROR: input file should be ELF\n");
		return -1;
	}

	cpmem_addr = 0;
	rdc_addr = addr;

	return 0;
}

static int findRdc(FILE* in)
{
	static const unsigned bankSizeOptions[] = {0x08000000, 0x10000000, 0x20000000, CP_AREA_SIZE /*Nevo - bottom of the bank*/};
	int i;
	for(i=0;i<(sizeof(bankSizeOptions)/sizeof(bankSizeOptions[0]));i++)
		if(!checkRdc(in, bankSizeOptions[i])) return 0;
	if (!checkRdc_alt(in, 0x08140400))
		return 0;
	memset(&rdc,0,sizeof(rdc));
	return -1;
}

/* MMU table walks */
#define _1MB (0x100000)
#define PMD_VA_SHIFT (20)
#define PMD_VA_MASK (0xffffffff<<PMD_VA_SHIFT)
#define VA2PMDOFF(va) ((va>>PMD_VA_SHIFT)<<2)
#define PMDT_MASK 3
#define PMDT_PT 1
#define PMDT_SECT 2
#define PT_SIZE (_1MB/PAGE_SIZE)
#define PMD_PT_ADDR(pmd) (pmd & ~0x3ff)
#define VA2PTE(va) ((va>>12)&(PT_SIZE-1))
#define PTE_MASK 3
#define PTE_PAGE 2 /* 2b'10 or 2b'11 */
#define PTE_PA_SHIFT (12)
#define PTE_PA_MASK (0xffffffff<<PTE_PA_SHIFT)


static struct mmu_state {
	unsigned va;
	unsigned pmd;
	unsigned pagetable[PT_SIZE];
}mmu;

unsigned v2p32(unsigned va, FILE *fin)
{
	unsigned off;
	if ((va ^ mmu.va) & PMD_VA_MASK) {
		unsigned pos = ftell(fin);
		off = FILE_OFFSET(rdc.header.pgd+VA2PMDOFF(va));
		if (fseek(fin, off, SEEK_SET) || (fread(&mmu.pmd, sizeof(mmu.pmd), 1, fin) != 1)) {
			fprintf(rdplog, "v2p(0x%.8x) failed: cannot read 1st level desc at offset 0x%.8x\n", va, off);
			return BADPA;
		}
		if ((mmu.pmd&PMDT_MASK) == PMDT_PT) {
			off = FILE_OFFSET(PMD_PT_ADDR(mmu.pmd));
			if (fseek(fin, off, SEEK_SET) || fread(&mmu.pagetable[0], sizeof(mmu.pagetable[0]), PT_SIZE, fin) != PT_SIZE) {
				fprintf(rdplog, "v2p(0x%.8x) failed: cannot read page table at offset 0x%.8x\n", va, off);
				return BADPA;
			}
		}
		fseek(fin, pos, SEEK_SET);
	}
	if ((mmu.pmd&PMDT_MASK) == PMDT_SECT)
		return (va&~PMD_VA_MASK)|(mmu.pmd&PMD_VA_MASK);
	if ((mmu.pmd&PMDT_MASK) != PMDT_PT) {
		fprintf(rdplog, "v2p(0x%.8x) failed: bad 1st level desc at offset 0x%.8x: 0x%.8x\n", va, off, mmu.pmd);
		return BADPA;
	} else {
		unsigned pte = mmu.pagetable[VA2PTE(va)];
		if ((pte & PTE_MASK)&PTE_PAGE) {
			//fprintf(rdplog, "v2p(0x%.8x) ok: -> 0x%.8x -> 0x%.8x\n", va, pte, (va&~PTE_PA_MASK)|(pte&PTE_PA_MASK));
			return (va&~PTE_PA_MASK)|(pte&PTE_PA_MASK);
		}
		else {
			fprintf(rdplog, "v2p(0x%.8x) failed: bad 2st level desc at offset 0x%.8x: 0x%.8x\n", va, off, pte);
		}
	}
	return BADPA;
}

#define U64(x) ((u64)(x))
#define MMU64_VABITS 39
#define MMU64_GRANULE 12 /* 4KB: n */
#define MMU64_STAGEBITS (MMU64_GRANULE-3)
#define MMU_STAGE0_BITS (MMU64_VABITS-MMU64_GRANULE-MMU64_STAGEBITS*3)
#define ONES64 U64(0xffffffffffffffff)
#define MMU64_VAMASK (~(ONES64<<MMU64_VABITS))
#define MMU64_REGIONMASK (~MMU64_VAMASK)
#define MMU64_PT_SIZE (U64(1)<<MMU64_STAGEBITS) /* 2**(n-3) descs of 8 bytes each */
#define MMU64_STAGEMASK ((U64(1)<<MMU64_STAGEBITS)-1)
#define MMU64_DESC_SIZE 8
#define MMU64_DESCT_MASK 3
#define MMU64_DESCT_BLOCK 1
#define MMU64_DESCT_TABLE 3
#define MMU64_PA_MASK ((U64(1)<<48)-1)
#define MMU64_LEVELS 4

static struct mmu_state64 {
	u64 va;
	u64 desc[MMU64_LEVELS];
	int table_lvl;
	u64 pagetable[MMU64_PT_SIZE];
}mmu64;

/* See DDI0487A, section D4.2 */
unsigned v2p64(u64 va, FILE *fin) {
	unsigned off;
	unsigned pa;
	unsigned base = rdc.header.pgd;
	u64 offsetmask;
	int lvl; /* level */
	int i;
	if ((va & MMU64_REGIONMASK) != MMU64_REGIONMASK) {
		fprintf(rdplog, "v2p(0x%.8x%.8x) failed: not in kernel range\n", hi32(va), lo32(va));
		return BADPA;
	}

	va &= MMU64_VAMASK; /* clear the upper bits not relevant for this table walk */

	/* Does translation start at level 0 or 1 */
	lvl = MMU_STAGE0_BITS?0:1;
	for (; lvl < MMU64_LEVELS; lvl++) {
		int lower = MMU64_GRANULE+(3-lvl)*MMU64_STAGEBITS;
		u64 mask = MMU64_STAGEMASK<<lower;
		offsetmask = (U64(1)<<lower)-1;
		unsigned index = (unsigned)((va >> lower)&(MMU64_PT_SIZE-1));
		if (!mmu64.desc[lvl] || ((va^mmu64.va) & mask)) {
			/* We need this level table to get the descriptor */
			for (i=lvl;i<MMU64_LEVELS;i++) mmu64.desc[i]=0;
			if ((mmu64.table_lvl-1) != lvl) {
				unsigned pos = ftell(fin);
				/* previous state is not relevant any more */
				mmu64.va = va;
				mmu64.table_lvl = 0;

				off = FILE_OFFSET(base);
				if (fseek(fin, off, SEEK_SET) || (fread(&mmu64.pagetable[0], sizeof(mmu64.pagetable[0]), MMU64_PT_SIZE, fin)
						!= MMU64_PT_SIZE)) {
					fprintf(rdplog, "v2p(0x%.8x%.8x) failed: cannot read level#%d table at offset 0x%.8x\n",
						hi32(va), lo32(va), lvl, off);
					return BADPA;
				}
				fseek(fin, pos, SEEK_SET);
				mmu64.table_lvl = lvl + 1;
			}
		}
		if (!mmu64.desc[lvl])
			mmu64.desc[lvl]=mmu64.pagetable[index];
		if ((mmu64.desc[lvl]&MMU64_DESCT_MASK) == MMU64_DESCT_BLOCK) {
			if ((lvl==0) || (lvl==3)) {
				fprintf(rdplog, "v2p(0x%.8x%.8x) failed: block desc at level#%d\n", hi32(va), lo32(va), lvl);
				return BADPA;
			}
			lvl++; /* we go one back after the loop exits this way or another */
			break;
		} else if ((mmu64.desc[lvl]&MMU64_DESCT_MASK) == MMU64_DESCT_TABLE) {
				base = (unsigned)(mmu64.desc[lvl]&MMU64_PA_MASK&~((1<<MMU64_GRANULE)-1));
		}
	} /* for lvl */

	pa = (unsigned)(mmu64.desc[lvl-1]&MMU64_PA_MASK&~offsetmask);
	pa += (unsigned)(va & offsetmask);
	return pa;
}

unsigned v2p(u64 va, FILE *fin) {
	if (aarch_type == aarch64)	return v2p64(va, fin);
	else						return v2p32((unsigned)va, fin);
}

int extractFile(const char* inName, const char* outShortName, FILE* fin,
	unsigned offset, unsigned size, int flexSize, u64 vaddr, int append)
{
	int ret = -1, earlyEnd = 0;
	unsigned sizeLeft, sz;
	FILE* fout=0;
	char *outName = 0;
	unsigned szr;

	outName = changeNameExt(inName, outShortName);
	if(outName) {
		if (!vaddr)
			fprintf(rdplog, "Saving %d bytes at offset 0x%.8x into %s\n", size, offset, outName);
		else
			fprintf(rdplog, "Saving %d bytes at offset 0x%.8x vaddr=0x%.8x%.8x into %s\n", size, offset, hi32(vaddr), lo32(vaddr), outName);
		fout = fopen(outName, append ? "ab" : "wb");

		if(!fout || (append && fseek(fout, 0, SEEK_END))) {
			fprintf(rdplog, "Failed to write output file %s\n", outName);
		}
		else {
			if(fseek(fin, offset, SEEK_SET)) {
			fprintf(rdplog, "Failed to read input file at offset 0x%.8x\n", offset);
		  }
		  else if (!vaddr) {
			for(sizeLeft=size;sizeLeft;sizeLeft-=sz) {
				sz=sizeLeft>MAXBUF?MAXBUF:sizeLeft;
				if((szr=fread(buffer, sizeof(buffer[0]), sz, fin)) != sz) {
					if(flexSize) {
						earlyEnd = 1;
						fprintf(rdplog, "   short %s size 0x%.8x created vs expected 0x%.8x\n",
							outShortName, offset+size-sizeLeft+szr, size);
					} else {
						fprintf(rdplog, "Failed to read input file at offset 0x%.8x, %d\n", offset+size-sizeLeft, szr);
						goto bail;
					}
				}
				if(fwrite(buffer, sizeof(buffer[0]), szr, fout)!=szr) {
					fprintf(rdplog, "Failed to write output file %s at offset 0x%.8x\n", outName, size-sizeLeft);
					goto bail;
				}
				if(earlyEnd)
					break;
			} /* for */
			ret = 0;
		  } else {
			/* Discontinous physical space */
			unsigned po, po_last;
			/* Sanity check vaddr of the first page: should match the physical (file offset) */
			if (offset) {
				if ((po = FILE_OFFSET(v2p(vaddr, fin))) != offset) {
					fprintf(rdplog, "vaddr=0x%.8x does not match offset 0x%.8x\n", vaddr, offset);
					goto bail;
				}
				po_last = po;
			} else
				po_last = 0xffffffff; /* should not match the condition for fseek below */
			sz = 0;
  			for(sizeLeft=size;sizeLeft;sizeLeft-=sz) {
				unsigned leftOnPage = PAGE_SIZE - (unsigned)(vaddr & (PAGE_SIZE-1));
				unsigned pa;
				if ((pa = v2p(vaddr, fin)) == BADPA) goto bail;
				po = FILE_OFFSET(pa);
				/* check if the page follows the previous one, otherwise fseek */
				if ((po != (po_last + sz)) && fseek(fin, po, SEEK_SET)) {
					fprintf(rdplog, "Failed to read input file at offset 0x%.8x\n", po);
					goto bail;
				}

				sz=sizeLeft>leftOnPage?leftOnPage:sizeLeft;
				if((szr=fread(buffer, sizeof(buffer[0]), sz, fin)) != sz) {
					fprintf(rdplog, "Failed to read input file at offset 0x%.8x, %d\n", po, szr);
					goto bail;
				}
				if(fwrite(buffer, sizeof(buffer[0]), sz, fout)!=sz) {
					fprintf(rdplog, "Failed to write output file %s at offset 0x%.8x\n", outName, size-sizeLeft);
					goto bail;
				}
				vaddr += sz;
				po_last = po;
			} /* for */
			ret = 0;

		  }
		}
	}

bail:
#ifdef DEF_CONTINUE_ON_ERROR
	ret = 0;
#endif
	if(fout) fclose(fout);
	if(ret) {
		if(outName) unlink(outName);
	}
	if(outName) free(outName);
	return ret;
}

/*
Some ports exhibit a dump issue that results in duplication of data
at certain aligned offsets. Check and warn for ramfiles, which would then fail
with crc error on decompress.
Assuming ramfile size is not big, use a dumb simple scan to find such duplications.
*/
static int checkFileDupBlocks(FILE* fin, unsigned offset, unsigned size)
{
	unsigned char *buffer;
	int ret = -1;

	if(fseek(fin, offset, SEEK_SET)) {
		fprintf(rdplog, "Failed to read input file at offset 0x%.8x\n", offset);
		return -1;
	}
	if((buffer=(unsigned char *)malloc(size)) == NULL) {
		fprintf(rdplog, "Failed to allocate %d bytes\n", size);
		return -1;
	}
	if((fread(buffer, sizeof(buffer[0]), size, fin)) != size) {
		fprintf(rdplog, "Failed to read input file %d bytes at offset %.8x\n", size, offset);
	} else {
		unsigned align = 0x200;
		unsigned window = 0x200;
		unsigned index = ((offset+align-1)&~(align-1)) - offset;
		unsigned sza = (size - index)&~(align-1);
		unsigned i, j;
		for (i = index; i<sza; i+=align) {
			for(j = i+align; j<sza; j+=align) {
				unsigned k;
				unsigned non0=0;
				for (k=0;(k<(sza-j)) && (buffer[i+k]==buffer[j+k]);k++)
					if(buffer[i+k] != 0)
						non0++;
					if ((k>window) && non0) {
						fprintf(rdplog, "WARNING: suspicious block duplication at 0x%.8x and 0x%.8x: 0x%.8x bytes\n",
							offset+i, offset+j,k);
					}
			}
		}
		ret = 0;
	}

	free(buffer);
	return ret;
}
static int extractRamfiles(const char* inName, FILE* fin)
{
	unsigned rfAddr = rdc.header.ramfile_addr;
	unsigned rfOffset;
	u64 vaddr;
	struct ramfile_desc rf;
	char ramfilename[20];
	int ramfilenum;

	if(!rfAddr) {
		fprintf(rdplog, "No ramfile objects found\n");
		return 0;
	}
	for(ramfilenum=0;rfAddr;ramfilenum++) {
		rfOffset = FILE_OFFSET(rfAddr);
		//fprintf(rdplog, "RFADDR=0x%.8x OFF=0x%.8x\n",rfAddr, rfOffset);
		if(fseek(fin, rfOffset, SEEK_SET) || (fread(&rf, sizeof(rf), 1, fin)!=1)) {
			fprintf(rdplog, "Failed to read ramfile desc in input file at offset 0x%.8x\n", rfOffset);
			return -1;
		}
		sprintf(ramfilename, "ramfile.%d.tgz", ramfilenum);
		if (rf.flags & RAMFILE_PHYCONT) {
			vaddr = 0;
		} else {
			vaddr = aarch_type == aarch64 ? mk64(rf.vaddr_hi, rf.vaddr) : rf.vaddr;
			vaddr+=sizeof(rf);
		}
		if(extractFile(inName, ramfilename, fin, rfOffset+sizeof(rf), rf.payload_size-0x20, 0, vaddr, 0))
			return -1;
		if(rf.flags & RAMFILE_PHYCONT)
			/* Specific to old versions that did not support discontinuous physical anyway.
			Assumes continuous, because data is read from input file.*/
			checkFileDupBlocks(fin, rfOffset+sizeof(rf), rf.payload_size);
		rfAddr = rf.next;
	}
	return 0;
}

// Extract legacy CP/EEH objects located in CP DDR
#define CP_FILENAME_FORMAT "com_%s.bin"

static int checkFileName(const char *s)
{
	for(;*s;s++)
		if(!isprint(*s)) return -1;
	return 0;
}
static int extractPostmortemfiles(const char* inName, FILE* fin, int commOnly)
{
	unsigned offset, commOnlyOffs = 0;
	LoadTableType lt;
	EE_PostmortemDesc_Entry pm_array[POSTMORTEM_NUMOF_ENTRIES];
	unsigned cpFileOffset = FILE_OFFSET(cpmem_addr);
	int i;
	unsigned len;
	unsigned w;
	unsigned addr;
	const char *basename;
	int comDdrRwBin_isInput;

	char cp_filename[POSTMORTEM_BUF_NAME_SIZE+sizeof(CP_FILENAME_FORMAT)];// two spare chars on %s
	int namelen;

	if(commOnly) cpFileOffset = 0;

	/* Look for CP load-table at one of the possible locations; since TAI is locked, the one in RO area heading is not accessible. */
	for (addr = COMM_LOW_ADDRESS; addr<=COMM_RO_ADDRESS; addr+=0x00100000) {
		offset=cpFileOffset+PHY_OFFSET(addr)+LOAD_TABLE_OFFSET;

		if(fseek(fin, offset, SEEK_SET) || (fread(&lt, sizeof(lt), 1, fin)!=1)) {
			fprintf(rdplog, "Failed to read CP LoadTable desc in input file at offset 0x%.8x\n", offset);
			return -1;
		}
		if(!strcmp(lt.Signature, LOAD_TABLE_SIGNATURE_STR)) break;
	}
	if(addr > COMM_RO_ADDRESS) {
		fprintf(rdplog, "CP LoadTable signature is missing in input file at offset 0x%.8x\n", offset);
		return -1;
	}
	fprintf(rdplog, "CP LoadTable signature found at 0x%.8x\n", addr);

	/* Extract CP DDR_RW */
	/* Sanity check the LT contents */
	//Allow addresses 0x40xxxxxx but not only 0xD0xxxxxx
	if ((((lt.ramRWbegin>>28)==0xd) || ((lt.ramRWbegin>>28)==0x4)) && ((lt.ramRWend-lt.ramRWbegin)<=MAX_POSTMORTEM_FILE)) {
		 offset = cpFileOffset + PHY_OFFSET(lt.ramRWbegin);
		 len = lt.ramRWend-lt.ramRWbegin;
	} else {
		 fprintf(rdplog, "CP LT values for DDR RW look wrong: 0x%.8x - 0x%.8x; using defaults\n", lt.ramRWbegin, lt.ramRWend);
		 offset = cpFileOffset + PHY_OFFSET(CP_DDR_RW_OFFSET);
		 len = CP_DDR_RW_SIZE;
	}

	if(commOnly && ((addr-COMM_LOW_ADDRESS)==0))  commOnlyOffs = PHY_OFFSET(lt.ramRWbegin);

	//DDR-RW Full including MSA 3MB
	if (!commOnly &&
		(((addr & 0x0FFFFFFF) - (COMM_LOW_ADDRESS & 0x0FFFFFFF)) == MSA_DDR_SIZE)) {

		extractFile(inName, COM_DDR_RW_FULL_BIN_NAME, fin,
			             cpFileOffset+PHY_OFFSET(COMM_LOW_ADDRESS),
			len+(offset-(cpFileOffset+PHY_OFFSET(COMM_LOW_ADDRESS))),
			commOnly, 0);
			//if failed, do not abort, but continue for backward compatibility
	}

	//DDR-RW COMM ONLY
	comDdrRwBin_isInput = 0;
	if(commOnly) {
		basename = strstr(inName, "\\" COM_DDR_RW_BIN_NAME);
		if(basename && (strlen(basename) == strlen("\\" COM_DDR_RW_BIN_NAME)))
			comDdrRwBin_isInput = 1;
	}
	if(!comDdrRwBin_isInput) {
		offset -= commOnlyOffs;
		if(extractFile(inName, COM_DDR_RW_BIN_NAME, fin, offset, len, commOnly, 0))
			return -1;
	}

	offset = cpFileOffset + PHY_OFFSET(/*lt.imageBegin*/addr) + PUB_ADDR_OF_PTR2POSTMORTEM_OFFS;/* right above loadtable */
	if(fseek(fin, offset, SEEK_SET) || (fread(&w, sizeof(w), 1, fin)!=1)) {
		fprintf(rdplog, "Failed to read CP PostMortem addr in input file at offset 0x%.8x\n", offset);
		return -1;
	}

	offset = cpFileOffset + PHY_OFFSET(w);
	offset -= commOnlyOffs;
	if(fseek(fin, offset, SEEK_SET) || (fread(&pm_array[0], sizeof(pm_array), 1, fin)!=1)) {
		fprintf(rdplog, "Failed to read CP PostMortem descs in input file at offset 0x%.8x\n", offset);
		return -1;
	}

	for(i=0;i<POSTMORTEM_NUMOF_ENTRIES;i++) {
		if(pm_array[i].name[0]==0) break;
		// Truncate the name if not null-terminated in the range
		if((namelen=strlen(pm_array[i].name))>=POSTMORTEM_BUF_NAME_SIZE)
			pm_array[i].name[POSTMORTEM_BUF_NAME_SIZE-1]=0;

		offset = cpFileOffset + PHY_OFFSET((unsigned)pm_array[i].bufAddr);
		offset -= commOnlyOffs;
		/* Sanity check the object, might be not valid e.g. in case of AP only system configuration */
		if((offset >= (cpFileOffset+CP_AREA_SIZE_MAX))
			|| (pm_array[i].bufLen > MAX_POSTMORTEM_FILE)
			|| checkFileName(pm_array[i].name)) {
			fprintf(rdplog, "Invalid CP PostMortem desc: name=%s, offset=0x%.8x, size=%d\n", pm_array[i].name, offset, pm_array[i].bufLen);
			continue; //return -1; //do NOT abort but continue; other dumps may be OK
		}

		sprintf(cp_filename, CP_FILENAME_FORMAT, pm_array[i].name);
		extractFile(inName, cp_filename, fin, offset, pm_array[i].bufLen, 0, 0);
	}
	return 0;
}

static int resolveRefs(FILE *fin, struct rdc_dataitem *rdi, int nw)
{
	int i;
	u64 va;
	int size = 1;
	unsigned *p = &rdi->body.w[0];
	unsigned pa;

	if (aarch_type == aarch64)
		size = 2;

	for (i = 0; i < nw; i++, p+=size) {
		if (rdi->attrbits & RDI_ATTR_ADDR(i)) {
			va = aarch_type == aarch64 ? mk64(p[1], p[0]): p[0];
			pa = v2p(va, fin);
			if (pa == BADPA)
				return -1;
			pa=FILE_OFFSET(pa);
			if(fseek(fin, pa, SEEK_SET) || (fread(p, size*sizeof(rdi->body.w[0]), 1, fin)!=1)) {
				fprintf(rdplog, "Failed to read data at offset 0x%.8x\n", pa);
				return -1;
			}
		}
	}
	return 0;
}

static int extractCyclicBuffer(const char* inName, FILE *fin, const char *name, struct rdc_dataitem *rdi, int nw)
{
	u64 addr, size, cur;
	unsigned u;
	unsigned unit=1;
	int i = 0;
	if (nw < 2)
		return -1;

	if (aarch_type == aarch64)
		nw*=2;

	addr = rdi->body.w[i++];
	if (aarch_type == aarch64) addr=mk64(rdi->body.w[i++], lo32(addr));
	/* Size might be actually the endPtr instead */
	size = rdi->body.w[i++];
	if (aarch_type == aarch64) size=mk64(rdi->body.w[i++], lo32(size));

	cur = 0;
	if (i < nw) {
		cur = rdi->body.w[i++];
		if (aarch_type == aarch64) cur=mk64(rdi->body.w[i++], lo32(cur));
	}


	/* Since any address (virtual) is either in kernel or vmalloc spaces, it's above 0xc0000000 */
	if (size > MAX_POSTMORTEM_FILE) {
		u = (unsigned)(size - addr);
		if (u <= MAX_POSTMORTEM_FILE) size -= addr;
		else if (aarch_type == aarch64)
			size=lo32(size); /* if the value is 32-bit only, discard the upper 32 */
	}
	if (size > MAX_POSTMORTEM_FILE)
		return -1;

	if (cur > size) {
		u = (unsigned)(cur - addr);
		if (u <= size) cur -= addr;
		else if (aarch_type == aarch64)
			size=lo32(cur); /* if the value is 32-bit only, discard the upper 32 */
	}

	if (cur >= size)
			/* cur is assumed to be running index with no wrap around (can be > size) */
			cur %= size;

	if (cur > size)
		return -1;

	/* unit is object size in bytes, where size and cur are numbers of such objects */
	unit = nw > i ? rdi->body.w[i++] : 1;
	if (!unit || (unit > 0x100))
		unit = 1;

	size *= unit;
	cur *= unit;

	/* At this point:
		addr: virtual address of the buffer;
		size: size of the buffer (in bytes)
		cur: index of current point inside the buffer (in bytes); cur < size */
	fprintf(rdplog, "RDI_CBUF: 0x%.8x%.8x, 0x%.8x, 0x%.8x\n", hi32(addr), lo32(addr), lo32(size), lo32(cur));
	if (extractFile(inName, name, fin, 0, (unsigned)(size-cur), 0, addr+cur)
		|| (cur && extractFile(inName, name, fin, 0, (unsigned)cur, 0, addr, 1)))
		return -1;
	return 0;
}

static int extractPhysicalBuffer(const char* inName, FILE *fin, const char *name, struct rdc_dataitem *rdi, int nw)
{
	unsigned addr, size;
	int i = 0;
	if (nw < 2)
		return -1;

	addr = rdi->body.w[i++];
	if (aarch_type == aarch64) i++; /* no support for physical address >32 bit for now */
	size = rdi->body.w[i++];
	if (aarch_type == aarch64) i++; /* no support for physical address >32 bit for now */

	fprintf(rdplog, "RDI_PBUF: 0x%.8x, 0x%.8x\n", addr, size);
	return extractFile(inName, name, fin, addr, size, 0, 0);
}

int readObjectAtVa(FILE *fin, void *buf, u64 addr, int size)
{
	unsigned pa;
	pa = v2p(addr, fin);
	if (pa != BADPA) {
		pa=FILE_OFFSET(pa);
		if(!fseek(fin, pa, SEEK_SET))
			return fread(buf, 1, size, fin);
	}
	return -1;
}


#define RDI_FILE_EXT ".bin"
typedef int parser_f(const char* inName, FILE *fin, const char *name, struct rdc_dataitem *rdi, int nw);
//-----------------------------------------------------------------------------------------------------------------------------------------
//                                                          PARSER TABLE
//-----------------------------------------------------------------------------------------------------------------------------------------
extern int pmlog_parser(const char* inName, FILE *fin, const char *name, struct rdc_dataitem *rdi, int nw);
extern int i2clog_parser(const char* inName, FILE *fin, const char *name, struct rdc_dataitem *rdi, int nw);

const struct parser_def {
	/* filename as appears in the rdi object .name field */
	const char *name;
	/* The 1st parser is invoked before=instead of the default one than would extract the binary buffer contents or like */
	parser_f *parser_1;
	/* The 2st parser is invoked after of the 1st default/non-default parser is done; the default 1st already produced the binary file (name) */
	parser_f *parser_2;
} parser_table[] = {
	{ "pmlog", NULL, &pmlog_parser },
	{ "i2clog", NULL, &i2clog_parser },
};
//-----------------------------------------------------------------------------------------------------------------------------------------
//                                                       END OF PARSER TABLE
//-----------------------------------------------------------------------------------------------------------------------------------------

void find_parser(const char *name, parser_f **parser_1, parser_f **parser_2)
{
	int i;
	for (i = 0; i < (sizeof(parser_table)/sizeof(parser_table[0])); i++)
		if (!strcmp(name, parser_table[i].name)) {
			*parser_1 = parser_table[i].parser_1;
			*parser_2 = parser_table[i].parser_2;
			break;
		}
	return;
}

static int getRdiNw(struct rdc_dataitem *rdi)
{
	int size = rdi->size - RDI_HEAD_SIZE;
	int item_size = sizeof(unsigned);
	if (aarch_type == aarch64) {
		size -= sizeof(unsigned);
		item_size = sizeof(u64);
	}
	if (size < 0) return -1;
	return size/item_size;
}

static struct rdc_dataitem *extractRdi(const char* inName, FILE *fin, int index, struct rdc_dataitem *rdi)
{
	int size, nw, i;
	parser_f *parser_1 = 0;
	parser_f *parser_2 = 0;
	char name[MAX_RDI_NAME+sizeof(RDI_FILE_EXT)];
	int ret = 0;

	/* Validate object size */
	size = &rdc.body.space[sizeof(rdc.body.space)] - (unsigned char *)rdi;
	if (rdi->size > size)
		fprintf(rdplog, "RDI#%d is too long (%d): truncated to %d\n", index, rdi->size, size);
	else
		size = rdi->size;
	if (!size) return 0; /* end */
	nw = getRdiNw(rdi);
	if (nw < 0) {
		fprintf(rdplog, "RDI#%d of illegal size %d: stopping RDI parsing\n", index, size);
		return 0;
	}

	/* Extract object name */
	name[MAX_RDI_NAME] = 0;
	strncpy(name, rdi->name, MAX_RDI_NAME);
	find_parser(name, &parser_1, &parser_2);

	strcat(name, RDI_FILE_EXT);

	if (resolveRefs(fin, rdi, nw))
		fprintf(rdplog, "RDI#%d is invalid: bad reference\n", index, rdi->size, size);
	else {
		if (parser_1) {
			fprintf(rdplog, "RDI#%d, size %d (\"%s\"): invoking dedicated parser#1: ", index, size, name);
			parser_1(inName, fin, name, rdi, nw);
		} else {
			/* Extract/parse the object itself */
			switch (rdi->type) {
			case RDI_CBUF:
				fprintf(rdplog, "RDI#%d of cyclic buffer type, size %d (\"%s\"): extracting: ", index, size, name);
				ret = extractCyclicBuffer(inName, fin, name, rdi, nw);
				break;
			case RDI_PBUF:
				fprintf(rdplog, "RDI#%d of physical buffer type, size %d (\"%s\"): extracting: ", index, size, name);
				ret = extractPhysicalBuffer(inName, fin, name, rdi, nw);
				break;
			default:
				fprintf(rdplog, "RDI#%d of unknown type %d, size %d (\"%s\"): contents:\n\t", index, rdi->type, size, name);
				for (i = 0 ; i < nw; i++)
					fprintf(rdplog, "0x%.8x ", rdi->body.w[i]);
				break;
			}
		}

		if (ret)
			fprintf(rdplog, "RDI#%d, size %d (\"%s\"): %s default parser#1 FAILED\n", index, size, name, parser_1?"non-":"");
		else if (parser_2) {
			fprintf(rdplog, "RDI#%d, size %d (\"%s\"): invoking dedicated parser#2: ", index, size, name);
			parser_2(inName, fin, name, rdi, nw);
		}
	}
	fprintf(rdplog, "\n");
	rdi = (struct rdc_dataitem *)(((unsigned char *)rdi) + size);
	if (rdi >= (struct rdc_dataitem *)&rdc.body.space[sizeof(rdc.body.space)])
		rdi = 0;
	return rdi;
}

static int extractRdiObjects(const char* inName, FILE *fin)
{
	struct rdc_dataitem *rdi;
	int i;

	rdi = &rdc.body.rdi[0];
	for (i = 0; rdi; i++) {
		rdi = extractRdi(inName, fin, i, rdi);
	}

	return 0;
}

#define CP15_ID_DEFAULT 0x000f0000 /* ARMV7, but not equal to any supported SoC: the value is used to tell if we're using defaults */
/* Set default cp15 and cpu registers that should mimic kernel mode state to enable virtual memory */
void checkRdd(struct ramdump_state *prdd)
{
	if (!prdd->cp15.id && !prdd->cp15.cr && !prdd->cp15.ttb) {
		fprintf(rdplog, "SNAP! The ramdump_data is invalid or its address in RDC is zero: probably dump after force reset:\nGenerating ramdump.cmm assuming defaults\n");
		prdd->cp15.id = CP15_ID_DEFAULT; /* default: ARMV7*/
		prdd->cp15.cr = 0x10c5387d; /* all MMU and caches enabled, remap enabled */
		prdd->cp15.da_ctrl = 0x00000017; /* kernel mode setting */
		prdd->cp15pj4.prremap = 0x000a81a8;
		prdd->cp15pj4.nmremap = 0x40e040e0;
		prdd->regs.ARM_cpsr = 0xd3; /* SVC mode */
		/* Do not set prdd->cp15.ttb: the best fit would be the init_mm promary table at PAGE_OFFSET+TEXT_OFFSET-0x4000,
		  but we don't know what these are (differ between architectures). So extractCpuState will set it based on kernel symbol table.
		*/
	}
}

void stripTextCR(char* text)
{
	char* p = text;
	//The text definitelly has \0
	while(*p) {
		if ((*p == '\n') || (*p == '\r'))
			*p = ' ';
		p++;
	}
}

const char PRINTK_FILE_NAME[] = "printks.txt";
static int extractCpuState(const char* inName, const char* outShortName, FILE* fin, unsigned addr)
{
	unsigned offset;
	struct ramdump_state rdd;
	FILE *fout;
	char *outName=0;
	char *outPath=0;
	const char *inShortName;
	int i;
	unsigned *p;
	bool isPJ4;
	int ret=-1;

	if (addr) {
		if(addr>=0xc0000000) {
			/* Originally the ramdump_data_addr field was set to virtual address of ramdump_data object.
			   This has been changed to physical address. The object resides in kernel bss, physical 0x8000000..0x80xxxxx */
			addr &= ~0x40000000; /* convert to physical */
		}

		offset=FILE_OFFSET(addr);

		if(fseek(fin, offset, SEEK_SET) || (fread(&rdd, sizeof(rdd), 1, fin)!=1)) {
			fprintf(rdplog, "Failed to read struct ramdump_state at 0x%.8x\n", offset);
			return -1;
		}
	} else
		memset(&rdd, 0, sizeof(rdd));

	checkRdd(&rdd);

	outName = changeNameExt(inName, outShortName);
	if(!outName || (fout=fopen(outName, "wt"))==NULL) goto bail;

	if(outName) free(outName), outName=0;
	outPath = changeNameExt(inName, "");

	// Get input file name without the path
	inShortName = inName + strlen(outPath);

	// Trim the trailing "\": this makes things easier in the cmm script: the slash added serves as a separator
	// between variable reference (path) and the following literal string (file name).
	if (outPath[i=strlen(outPath)-1] == '\\') outPath[i]=0;

	fprintf(rdplog, "Error type 0x%.8x, description: %s\n", rdd.err, rdd.text);

	fprintf(fout, "; This script has been generated by Marvell rdp version %s\n; Source: %s\n", VERSION_STRING, inName);
	fprintf(fout, "AREA.CREATE CONTEXT\nAREA.VIEW CONTEXT\nAREA.SELECT CONTEXT\n");
	fprintf(fout, "printer.ClipBoard\n");
	fprintf(fout, "PRINT \"------------------------------------------------------------------\"\n");
	fprintf(fout, "&log_path=\"%s\"\n", outPath);
	fprintf(fout, "&vmlinux_path=\"&log_path\"\n");
	fprintf(fout, "//------------------------------------------------------------------\n");
	fprintf(fout, "PRINT \"LOG location: &log_path (if moved, please, modify the logpath var in ramdump.cmm)\"\n");
	stripTextCR(rdd.text); //The fout-cmm script is wrong with \n, \r inside
	fprintf(fout, "PRINT \"Error type 0x%.8x, description: %s\"\n", rdd.err, rdd.text);
	if (rdd.cp15.id == CP15_ID_DEFAULT)
		fprintf(fout, "PRINT \"SNAP! The ramdump_data is invalid or its address in RDC is zero: probably dump after force reset\"\n");

	// Identify the CPU
	isPJ4 = ((rdd.cp15.id>>16)&0xf)==0xf;
	fprintf(fout, "PRINT \"CPU is %s (ID=0x%.8x)\"\n", isPJ4?"ARMV7":"XScale", rdd.cp15.id);
	// CPU type
	// ARM1176JZ was not good - noes not support thumb-2 (T2). 88AP955 is the best fot. If not supported use CORTEXA9.
	fprintf(fout, "; CPU type: best is to use 88AP955, if not supported in your T32, use CORTEXA9 instead\n");
	fprintf(fout, "sys.cpu %s\n;sys.cpu CORTEXA9\nsys.up\nmmu.off\n", isPJ4?"88AP955":"XSCALE");

	// Add LOAD commands
	fprintf(fout, "PRINT \"Loading kernel ELF\"\n");
	fprintf(fout, "data.load.elf \"&vmlinux_path\\%s\"\n", "vmlinux");
	fprintf(fout, "&vmlinux_kver=data.string(linux_banner)\n");
	fprintf(fout, "PRINT \"vmlinux kernel version string: &vmlinux_kver\"\n");

	fprintf(fout, "  PRINT \"Adding the Linux Awareness Support...\"\n");
#if (0) /* Linux3 available supports only arm64 for now */
	fprintf(fout, "data.find v.range(\"linux_banner\") \"version 3.\"\n");
	fprintf(fout, "IF FOUND()\n(\n");
	fprintf(fout, "  TASK.CONFIG c:\\t32\\demo\\arm\\kernel\\linux-3.x\\linux3     ; loads Linux awareness (linux.t32)\n");
	fprintf(fout, "  MENU.ReProgram c:\\t32\\demo\\arm\\kernel\\linux-3.x\\linux  ; loads Linux menu (linux.men)\n");
	fprintf(fout, ")\nELSE\n(\n");
	fprintf(fout, "  TASK.CONFIG c:\\t32\\demo\\arm\\kernel\\linux\\linux     ; loads Linux awareness (linux.t32)\n");
	fprintf(fout, "  MENU.ReProgram c:\\t32\\demo\\arm\\kernel\\linux\\linux  ; loads Linux menu (linux.men)\n");
	fprintf(fout, ")\n");
#else
	fprintf(fout, "  TASK.CONFIG c:\\t32\\demo\\arm\\kernel\\linux\\linux     ; loads Linux awareness (linux.t32)\n");
	fprintf(fout, "  MENU.ReProgram c:\\t32\\demo\\arm\\kernel\\linux\\linux  ; loads Linux menu (linux.men)\n");
#endif
	fprintf(fout, "HELP.FILTER.Add rtoslinux                          ; add linux awareness manual to help filter\n");

	// Dump original kernel code to file for later comparison with actual kernel code from dump
	fprintf(fout, "&ktext_start=v.value(\"&__init_end\")\n");
	fprintf(fout, "&ktext_end=v.value(\"&_data\")\n"); /* All RO sections */
	fprintf(fout, "&ktext_size=&ktext_end-&ktext_start\n");
	fprintf(fout, "IF (&ktext_size>0x0)&&(&ktext_size<0x800000)\n(\n");
	fprintf(fout, "			data.save.binary \"&log_path\\%s\" &ktext_start--&ktext_end\n", "kernel_text.vmlinux.bin");
	fprintf(fout, "			PRINT \"Saved actual kernel .text contents into: &log_path\\%s, &ktext_size bytes\"\n", "kernel_text.vmlinux.bin");
	fprintf(fout, ")\n");

	fprintf(fout, "PRINT \"Loading RAMDUMP files: %dMB: please, be patient...\"\n", dump_total_size/0x100000);
	for (i = 0; i < MAX_SEGMENTS; i++) {
		if (segments[i].size)
			fprintf(fout, "data.load.binary \"&log_path\\%s\" 0x%.8x++0x%.8x /skip 0x%.8x /nosymbol\n",
					inShortName, segments[i].pa, segments[i].size-1, segments[i].foffs);
	}

	// Load AP SRAM contents
	fprintf(fout, "IF OS.FILE(\"&log_path\\%s\")\n", apSramFileName);
	fprintf(fout, "\tdata.load.binary \"&log_path\\%s\" 0x%.8x /nosymbol\n", apSramFileName, AP_SRAM_ADDRESS);

	// Load CP extra DDR segment:
	// This segment is overwritten before dump is taken per memory map definition (by OBM/U-boot).
	// However, since some CP designs hold valueable data there, the contents is saved in a ramfile.
	// User is responsible to extract the ramfile to make the file available to T32.
	fprintf(fout, "IF OS.FILE(\"&log_path\\%s\")\n(\n", comExtraDdrFileName);
	fprintf(fout, "\tdata.load.binary \"&log_path\\%s\" 0x%.8x /nosymbol\n", comExtraDdrFileName, cpmem_addr);
	fprintf(fout, "\tPRINT \"%s has been loaded at 0x%.8x\"\n)\n", comExtraDdrFileName, cpmem_addr);
	fprintf(fout, "ELSE\n\tPRINT \"WARNING: %s NOT FOUND\"\n", comExtraDdrFileName);

	/* On pxa998 with only 32MB DDR there's no room to reserve permanently for OBM/U-boot, therefore
	   these use kernel RO segment and overwrite its contents. Reload from vmlinux. Ability to check the actual
	   kernel code contents against the vmlinux reference is therefore lost */
	if (dump_total_size<=0x04000000) {
		fprintf(fout, "PRINT \"Restoring the contents of kernel RO sections from vmlinux: [&ktext_start-&ktext_end]\"\n");
		fprintf(fout, "data.load.binary \"&log_path\\kernel_text.vmlinux.bin\" v.value(&ktext_start&~0x40000000) /nosymbol\n");
	}

	// CPU registers
	fprintf(fout, "r.set cpsr 0x%.8x\n", rdd.regs.ARM_cpsr); /* this first, so the banked registers below are set for correct mode */
	for(i=0,p=(unsigned*)&rdd.regs.uregs[0];i<15;i++,p++)
	{
		fprintf(fout,"r.set r%d 0x%.8X\n",i,*p);
	}

	fprintf(fout, "r.set pc 0x%.8x\n", rdd.regs.ARM_pc);

	// CP15 registers
	fprintf(fout, "per.set C15:0x201 0x%.8x ; MIDR\n", rdd.cp15.id);

	fprintf(fout, "per.set C15:0x1 0x%.8x ; CR\n", rdd.cp15.cr);

	if (rdd.cp15.ttb)
		fprintf(fout, "per.set C15:0x2 0x%.8x ; TTB0\n", rdd.cp15.ttb);
	else
		/* Set TTB to contain the PHYSICAL address of the init_mm primary MMU table (swapper_pg_dir: right before kernel code start */
		fprintf(fout, "per.set C15:0x2 v.value(((unsigned)&swapper_pg_dir)&~0x40000000 | 0x19) ; TTB0 - not available, set to default init_mm \n");

	fprintf(fout, "per.set C15:0x3 0x%.8x ; DAC\n", rdd.cp15.da_ctrl);
	fprintf(fout, "per.set C15:0x101 0x%.8x ; AUX CR\n", rdd.cp15.aux_cr);
	fprintf(fout, "per.set C15:0x201 0x%.8x ; CACR\n", rdd.cp15.cpar);

	fprintf(fout, "per.set C15:0x5 0x%.8x ; FSR\n", rdd.cp15.fsr);
	fprintf(fout, "per.set C15:0x6 0x%.8x ; FAR\n", rdd.cp15.far_);
	fprintf(fout, "per.set C15:0x10D 0x%.8x ; CIDR\n", rdd.cp15.procid);

	if(isPJ4) {
		/* ARMV7 */
		fprintf(fout, "per.set C15:0x202 0x%.8x ; TTBCR\n", rdd.cp15pj4.ttbc);
		fprintf(fout, "per.set C15:0x105 0x%.8x ; IFSR\n", rdd.cp15pj4.ifsr);
		fprintf(fout, "per.set C15:0x206 0x%.8x ; IFAR\n", rdd.cp15pj4.ifar);
		fprintf(fout, "per.set C15:0x102 0x%.8x ; TTB1\n", rdd.cp15pj4.ttb1);
		fprintf(fout, "per.set C15:0x2A 0x%.8x ; PPMRR\n", rdd.cp15pj4.prremap);
		fprintf(fout, "per.set C15:0x12A 0x%.8x ; NMRR\n", rdd.cp15pj4.nmremap);
		fprintf(fout, "per.set C15:0x0D 0x%.8x ; FCSEP\n", rdd.cp15pj4.fcsepid);
		fprintf(fout, "per.set C15:0x20D 0x%.8x ; URWTPIDR\n", rdd.cp15pj4.urwpid);
		fprintf(fout, "per.set C15:0x30D 0x%.8x ; UROTPIDR\n", rdd.cp15pj4.uropid);
		fprintf(fout, "per.set C15:0x40D 0x%.8x ; UPOTPIDR\n", rdd.cp15pj4.privpid);

		fprintf(fout, "per.set C15:0x11 0x%.8x ; SCR\n", rdd.cp15pj4.seccfg);
		fprintf(fout, "per.set C15:0x111 0x%.8x ; SDER\n", rdd.cp15pj4.secdbg);
		fprintf(fout, "per.set C15:0x211 0x%.8x ; NSACR\n", rdd.cp15pj4.nsecac);
	}


	// Moved loading of ELF and bin files from here up, as ELF loading operation resets some registers, e.g. the PC.

	fprintf(fout, "mmu.format std\nmmu.scan\nmmu.on\nsim.cache.on\n");

	// Check the actual version string in the DDR data loaded (using virtual addresses)
	if (rdc.header.kernel_build_id) {
		// Newer versions of kernel ramdump.c set a pointer to the linux_banner[] string in kernel_build_id
		// This gives a symbol table independent location of the actual version string,
		// and also provides a RW copy of this string e.g. on PXA1801, where kernel RO is not present in the dump.
		char verstring[150];
		if (readObjectAtVa(fin, verstring, rdc.header.kernel_build_id, sizeof(verstring)) <= 0) {
			fprintf(rdplog, "Failed to read kernel version at 0x%.8x\n", rdc.header.kernel_build_id);
			rdc.header.kernel_build_id = 0;
		} else {
			fprintf(rdplog, "Actual kernel version at 0x%.8x:\n\t%s\n", rdc.header.kernel_build_id, verstring);
		}
	}

	if (rdc.header.kernel_build_id)
		fprintf(fout, "&act_kver=data.string(C:0x%.8x)\n", rdc.header.kernel_build_id);
	else
		fprintf(fout, "&act_kver=data.string(linux_banner)\n");

	fprintf(fout, "PRINT \"Actual kernel version string: &act_kver\"\n");
	fprintf(fout, "&version_mismatch=0\n");
	fprintf(fout, "IF (\"&act_kver\"!=\"&vmlinux_kver\")\n(\n");
	fprintf(fout, "     PRINT \"ATTENTION: MISMATCHED KERNEL VERSION - this implies vmlinux file is wrong\"\n     &version_mismatch=1\n)\n");

	fprintf(fout, "PRINT \"READY\"\n");

	/* Extract additional objects for user convenience */
	// printk buffer: see kernel/printk.c
	fprintf(fout, "&printk_buf=v.value(\"(unsigned)log_buf\")\n");
	fprintf(fout, "&printk_size=v.value(\"(unsigned)log_buf_len\")\n");
	fprintf(fout, "IF (&version_mismatch==0) \n(\n");
	fprintf(fout, "     data.save.binary \"&log_path\\%s\" &printk_buf++&printk_size\n", PRINTK_FILE_NAME);
	fprintf(fout, "     PRINT \"Saved printk buffer contents into &log_path\\%s\"\n", PRINTK_FILE_NAME);
	/* Save actual kernel text (code) */
	fprintf(fout, "     IF (&ktext_size>0x0)&&(&ktext_size<0x800000)\n     (\n");
	fprintf(fout, "			data.save.binary \"&log_path\\%s\" &ktext_start--&ktext_end\n", "kernel_text.dump.bin");
	fprintf(fout, "          PRINT \"Saved actual kernel .text contents into: &log_path\\%s, &ktext_size bytes\"\n", "kernel_text.dump.bin");
	/* Now check if the kernel text is intact */
	fprintf(fout, "          COMPARE \"&log_path\\%s\" \"&log_path\\%s\"\n", "kernel_text.dump.bin", "kernel_text.vmlinux.bin");
	fprintf(fout, "          IF FOUND()\n          (\n");
	fprintf(fout, "			     PRINT \"ATTENTION: Kernel code mismatch, suspected corruption, check manually\"\n          )\n");
	fprintf(fout, "          ELSE\n          (\n");
	fprintf(fout, "			     PRINT \"Kernel code verified: ok\"\n          )\n");
	fprintf(fout, "     )\n");
	fprintf(fout, ")\n");
	// ramdump_data struct
	fprintf(fout, "var.view %%HEX ramdump_data\n");
	ret=0;

bail:
	if(fout) fclose(fout);
	if(outName) free(outName);
	if(outPath) free(outPath);

	return ret;
}

/* Set default cp15 and cpu registers that should mimic kernel mode state to enable virtual memory */
void checkRdd_64(struct ramdump_state_64 *prdd)
{
	if (!prdd->spr.midr && !prdd->spr.sctlr && !prdd->spr.ttbr1) {
		fprintf(rdplog, "SNAP! The ramdump_data is invalid or its address in RDC is zero: probably dump after force reset:\nGenerating ramdump.cmm assuming defaults\n");
		prdd->spr.midr = CP15_ID_DEFAULT; /* default: ARMV7*/
		prdd->spr.sctlr = 0x34d5d91d; /* all MMU and caches enabled, remap enabled */
		prdd->spr.mair = 0x000000ff440c0400;
		prdd->spr.current_el = 4; /* EL1 */
		prdd->regs.pstate=0x80000145;
		/* Do not set prdd->spr.ttbr1: extractCpuState_64 will set it based on kernel symbol table. */
	}
}


static int extractCpuState_64(const char* inName, const char* outShortName, FILE* fin, unsigned addr)
{
	unsigned offset;
	struct ramdump_state_64 rdd;
	FILE *fout;
	char *outName=0;
	char *outPath=0;
	const char *inShortName;
	int i;
	int seg0 = -1;
	u64 *p;
	u64 va;
	int ret=-1;

	if (addr) {
		offset=FILE_OFFSET(addr);

		if(fseek(fin, offset, SEEK_SET) || (fread(&rdd, sizeof(rdd), 1, fin)!=1)) {
			fprintf(rdplog, "Failed to read struct ramdump_state at 0x%.8x\n", offset);
			return -1;
		}
	} else
		memset(&rdd, 0, sizeof(rdd));

	checkRdd_64(&rdd);

	outName = changeNameExt(inName, outShortName);
	if(!outName || (fout=fopen(outName, "wt"))==NULL) goto bail;

	if(outName) free(outName), outName=0;
	outPath = changeNameExt(inName, "");

	// Get input file name without the path
	inShortName = inName + strlen(outPath);

	// Trim the trailing "\": this makes things easier in the cmm script: the slash added serves as a separator
	// between variable reference (path) and the following literal string (file name).
	if (outPath[i=strlen(outPath)-1] == '\\') outPath[i]=0;

	fprintf(rdplog, "Error type 0x%.8x, description: %s\n", rdd.err, rdd.text);

	fprintf(fout, "; This script has been generated by Marvell rdp version %s\n; Source: %s\n", VERSION_STRING, inName);
	fprintf(fout, "AREA.CREATE CONTEXT\nAREA.VIEW CONTEXT\nAREA.SELECT CONTEXT\n");
	fprintf(fout, "printer.ClipBoard\n");
	fprintf(fout, "PRINT \"------------------------------------------------------------------\"\n");
	fprintf(fout, "&log_path=\"%s\"\n", outPath);
	fprintf(fout, "&vmlinux_path=\"&log_path\"\n");
	fprintf(fout, "//------------------------------------------------------------------\n");
	fprintf(fout, "PRINT \"LOG location: &log_path (if moved, please, modify the logpath var in ramdump.cmm)\"\n");
	stripTextCR(rdd.text); //The fout-cmm script is wrong with \n, \r inside
	fprintf(fout, "PRINT \"Error type 0x%.8x, description: %s\"\n", rdd.err, rdd.text);

	if (rdd.spr.midr == CP15_ID_DEFAULT)
		fprintf(fout, "PRINT \"SNAP! The ramdump_data is invalid or its address in RDC is zero: probably dump after force reset\"\n");

	fprintf(fout, "PRINT \"CPU is %s (ID=0x%.8x)\"\n", "ARMV8", rdd.spr.midr);
	// CPU type
	fprintf(fout, "sys.cpu CORTEXA53\nsys.up\nmmu.off\n");

	// Add LOAD commands
	fprintf(fout, "PRINT \"Loading kernel ELF\"\n");
	fprintf(fout, "data.load.elf \"&vmlinux_path\\%s\"\n", "vmlinux");
	fprintf(fout, "&vmlinux_kver=data.string(linux_banner)\n");
	fprintf(fout, "PRINT \"vmlinux kernel version string: &vmlinux_kver\"\n");

	fprintf(fout, "  PRINT \"Adding the Linux Awareness Support...\"\n");
	fprintf(fout, "data.find v.range(\"linux_banner\") \"version 3.\"\n");
	fprintf(fout, "IF FOUND()\n(\n");
	fprintf(fout, "  TASK.CONFIG c:\\t32\\demo\\arm\\kernel\\linux-3.x\\linux3     ; loads Linux awareness (linux.t32)\n");
	fprintf(fout, "  MENU.ReProgram c:\\t32\\demo\\arm\\kernel\\linux-3.x\\linux  ; loads Linux menu (linux.men)\n");
	fprintf(fout, ")\nELSE\n(\n");
	fprintf(fout, "  TASK.CONFIG c:\\t32\\demo\\arm\\kernel\\linux\\linux     ; loads Linux awareness (linux.t32)\n");
	fprintf(fout, "  MENU.ReProgram c:\\t32\\demo\\arm\\kernel\\linux\\linux  ; loads Linux menu (linux.men)\n");
	fprintf(fout, ")\n");
	fprintf(fout, "HELP.FILTER.Add rtoslinux                          ; add linux awareness manual to help filter\n");

	// Dump original kernel code to file for later comparison with actual kernel code from dump
	fprintf(fout, "&ktext_start=v.value(\"&__init_end\")\n");
	fprintf(fout, "&ktext_end=v.value(\"&_data\")\n"); /* All RO sections */
	fprintf(fout, "&ktext_size=&ktext_end-&ktext_start\n");
	fprintf(fout, "IF (&ktext_size>0x0)&&(&ktext_size<0x800000)\n(\n");
	fprintf(fout, "			data.save.binary \"&log_path\\%s\" &ktext_start--&ktext_end\n", "kernel_text.vmlinux.bin");
	fprintf(fout, "			PRINT \"Saved actual kernel .text contents into: &log_path\\%s, &ktext_size bytes\"\n", "kernel_text.vmlinux.bin");
	fprintf(fout, ")\n");

	fprintf(fout, "PRINT \"Loading RAMDUMP files: %dMB: please, be patient...\"\n", dump_total_size/0x100000);
	for (i = 0; i < MAX_SEGMENTS; i++) {
		if (segments[i].size) {
			fprintf(fout, "data.load.binary \"&log_path\\%s\" 0x%.8x++0x%.8x /skip 0x%.8x /nosymbol\n",
					inShortName, segments[i].pa, segments[i].size-1, segments[i].foffs);
			if (seg0 < 0) seg0 = i;
		}
	}

#if (0)
	// Load AP SRAM contents
	fprintf(fout, "IF OS.FILE(\"&log_path\\%s\")\n", apSramFileName);
	fprintf(fout, "\tdata.load.binary \"&log_path\\%s\" 0x%.8x /nosymbol\n", apSramFileName, AP_SRAM_ADDRESS);

	// Load CP extra DDR segment:
	// This segment is overwritten before dump is taken per memory map definition (by OBM/U-boot).
	// However, since some CP designs hold valueable data there, the contents is saved in a ramfile.
	// User is responsible to extract the ramfile to make the file available to T32.
	fprintf(fout, "IF OS.FILE(\"&log_path\\%s\")\n(\n", comExtraDdrFileName);
	fprintf(fout, "\tdata.load.binary \"&log_path\\%s\" 0x%.8x /nosymbol\n", comExtraDdrFileName, cpmem_addr);
	fprintf(fout, "\tPRINT \"%s has been loaded at 0x%.8x\"\n)\n", comExtraDdrFileName, cpmem_addr);
	fprintf(fout, "ELSE\n\tPRINT \"WARNING: %s NOT FOUND\"\n", comExtraDdrFileName);

	/* On pxa998 with only 32MB DDR there's no room to reserve permanently for OBM/U-boot, therefore
	   these use kernel RO segment and overwrite its contents. Reload from vmlinux. Ability to check the actual
	   kernel code contents against the vmlinux reference is therefore lost */
	if (dump_total_size<=0x04000000) {
		fprintf(fout, "PRINT \"Restoring the contents of kernel RO sections from vmlinux: [&ktext_start-&ktext_end]\"\n");
		fprintf(fout, "data.load.binary \"&log_path\\kernel_text.vmlinux.bin\" v.value(&ktext_start&~0x40000000) /nosymbol\n");
	}
#endif

	// CPU registers
	fprintf(fout, "per.set SPR:0x30422 0x%.8x ; currentEL\n", rdd.spr.current_el);
	fprintf(fout, "r.set cpsr 0x%.8x\n", rdd.regs.pstate);
	for(i=0,p=(u64*)&rdd.regs.regs[0];i<31;i++,p++)
	{
		fprintf(fout,"r.set x%d 0x%.8x%.8x\n",i,hi32(*p),lo32(*p));
	}

	fprintf(fout, "r.set pc 0x%.8x%.8x\n", hi32(rdd.regs.pc),lo32(rdd.regs.pc));
	fprintf(fout, "r.set sp 0x%.8x%.8x\n", hi32(rdd.regs.sp),lo32(rdd.regs.sp));

	// SPR registers
	fprintf(fout, "per.set SPR:0x30000 0x%.8x ; MIDR\n", rdd.spr.midr);
	fprintf(fout, "per.set SPR:0x30006 0x%.8x ; REVIDR\n", rdd.spr.revidr);
	fprintf(fout, "per.set SPR:0x30100 0x%.8x ; SCTLR\n", rdd.spr.sctlr);
	fprintf(fout, "per.set SPR:0x30101 0x%.8x ; ACTLR\n", rdd.spr.actlr);
	fprintf(fout, "per.set SPR:0x30102 0x%.8x ; CPACR\n", rdd.spr.cpacr);

	if (rdd.spr.ttbr1) {
		fprintf(fout, "per.set SPR:0x30201 0x%.8x%.8x ; TTBR1\n", hi32(rdd.spr.ttbr1),lo32(rdd.spr.ttbr1));
		fprintf(fout, "per.set SPR:0x30202 0x%.8x%.8x ; TCR\n", hi32(rdd.spr.tcr), lo32(rdd.spr.tcr));
	} else {
		// Note: there's a compiler bug with implicit case: hi32(unsigned value) returns lo32(value) instead of 0
		u64 base_pa = (u64)segments[seg0].pa;
		/* Set TTBR1 to contain the PHYSICAL address of the init_mm primary MMU table (swapper_pg_dir) */
		fprintf(fout, "per.set SPR:0x30201 v.value(((void *)&swapper_pg_dir) - 0x%.8x%.8x + 0x%.8x%.8x) ; TTBR1 - not available, set to default init_mm \n",
			hi32(segments[seg0].va), lo32(segments[seg0].va), hi32(base_pa), lo32(base_pa));
		fprintf(fout, "per.set SPR:0x30202 0x%.8x%.8x ; TCR\n", 0x00000032, 0xb5193519);
	}
	fprintf(fout, "per.set SPR:0x30200 0x%.8x%.8x ; TTBR0\n", hi32(rdd.spr.ttbr0), lo32(rdd.spr.ttbr0));
	fprintf(fout, "per.set SPR:0x30A20 0x%.8x%.8x ; MAIR\n", hi32(rdd.spr.mair), lo32(rdd.spr.mair));

	fprintf(fout, "per.set SPR:0x30C10 0x%.8x ; ISR\n", rdd.spr.isr);
	fprintf(fout, "per.set SPR:0x30D04 0x%.8x%.8x ; TPIDR\n", hi32(rdd.spr.tpidr), lo32(rdd.spr.tpidr));
	fprintf(fout, "per.set SPR:0x30C00 0x%.8x%.8x ; VBAR\n", hi32(rdd.spr.vbar), lo32(rdd.spr.vbar));
	fprintf(fout, "per.set SPR:0x30520 0x%.8x ; ESR\n", rdd.spr.esr);
	fprintf(fout, "per.set SPR:0x30600 0x%.8x%.8x ; FAR\n", hi32(rdd.spr.far_), lo32(rdd.spr.far_));

	// Moved loading of ELF and bin files from here up, as ELF loading operation resets some registers, e.g. the PC.

	fprintf(fout, "mmu.format std\nmmu.scan\nmmu.on\nsim.cache.on\n");

	// Check the actual version string in the DDR data loaded (using virtual addresses)
	va = 0;
	if (rdc.header.kernel_build_id_hi) {
		// Newer versions of kernel ramdump.c set a pointer to the linux_banner[] string in kernel_build_id
		// This gives a symbol table independent location of the actual version string,
		// and also provides a RW copy of this string e.g. on PXA1801, where kernel RO is not present in the dump.
		char verstring[150];
		va = mk64(rdc.header.kernel_build_id_hi, rdc.header.kernel_build_id);
		if (readObjectAtVa(fin, verstring, va, sizeof(verstring)) <= 0) {
			fprintf(rdplog, "Failed to read kernel version at 0x%.8%.8x\n", hi32(va), lo32(va));
			va = 0;
		} else {
			fprintf(rdplog, "Actual kernel version at 0x%.8x%.8x:\n\t%s\n", hi32(va), lo32(va), verstring);
		}
	}

	if (va)
		fprintf(fout, "&act_kver=data.string(C:0x%.8x%.8x)\n", hi32(va), lo32(va));
	else
		fprintf(fout, "&act_kver=data.string(linux_banner)\n");

	fprintf(fout, "PRINT \"Actual kernel version string: &act_kver\"\n");
	fprintf(fout, "&version_mismatch=0\n");
	fprintf(fout, "IF (\"&act_kver\"!=\"&vmlinux_kver\")\n(\n");
	fprintf(fout, "     PRINT \"ATTENTION: MISMATCHED KERNEL VERSION - this implies vmlinux file is wrong\"\n     &version_mismatch=1\n)\n");

	fprintf(fout, "PRINT \"READY\"\n");

	/* Extract additional objects for user convenience */
	// printk buffer: see kernel/printk.c
	fprintf(fout, "&printk_buf=v.value(\"(void**)log_buf\")\n");
	fprintf(fout, "&printk_size=v.value(\"(unsigned)log_buf_len\")\n");
	fprintf(fout, "IF (&version_mismatch==0) \n(\n");
	fprintf(fout, "     data.save.binary \"&log_path\\%s\" &printk_buf++&printk_size\n", PRINTK_FILE_NAME);
	fprintf(fout, "     PRINT \"Saved printk buffer contents into &log_path\\%s\"\n", PRINTK_FILE_NAME);
	/* Save actual kernel text (code) */
	fprintf(fout, "     IF (&ktext_size>0x0)&&(&ktext_size<0x800000)\n     (\n");
	fprintf(fout, "			data.save.binary \"&log_path\\%s\" &ktext_start--&ktext_end\n", "kernel_text.dump.bin");
	fprintf(fout, "          PRINT \"Saved actual kernel .text contents into: &log_path\\%s, &ktext_size bytes\"\n", "kernel_text.dump.bin");
	/* Now check if the kernel text is intact */
	fprintf(fout, "          COMPARE \"&log_path\\%s\" \"&log_path\\%s\"\n", "kernel_text.dump.bin", "kernel_text.vmlinux.bin");
	fprintf(fout, "          IF FOUND()\n          (\n");
	fprintf(fout, "			     PRINT \"ATTENTION: Kernel code mismatch, suspected corruption, check manually\"\n          )\n");
	fprintf(fout, "          ELSE\n          (\n");
	fprintf(fout, "			     PRINT \"Kernel code verified: ok\"\n          )\n");
	fprintf(fout, "     )\n");
	fprintf(fout, ")\n");
	// ramdump_data struct
	fprintf(fout, "var.view %%HEX ramdump_data\n");
	ret=0;

bail:
	if(fout) fclose(fout);
	if(outName) free(outName);
	if(outPath) free(outPath);

	return ret;
}


int writeElfHeader(const char* inName, const char* outShortName)
{
	FILE *fout = 0;
	char *outName = 0;
	void *elf = 0;
	int elf_size;
	int	ret = -1;

	elf = init_elf(&elf_size);
	if (!elf) goto bail;
	outName = changeNameExt(inName, outShortName);
	if(!outName || (fout=fopen(outName, "wb"))==NULL) goto bail;

	if(outName) free(outName), outName=0;
	if (fwrite(elf, 1, elf_size, fout) != elf_size) goto bail;

	ret = 0;
bail:
	if(fout) fclose(fout);
	if(outName) free(outName);
	if (elf) free(elf);
	return ret;
}

int main(int argc, char* argv[])
{
	FILE* fin;
	char* inName=0;
	char* outName=0;
	int ret = -1;
	struct stat fst;
	int	i;

	fprintf(stderr,"Marvell RAMDUMP parser, version %s\n", VERSION_STRING);
	if(argc<2)
	{
		fprintf(stderr,"USAGE: %s input-file-name\n", argv[0]);
		exit(1);
	}

	inName = argv[1];

	if (stat(inName, &fst))
	{
		fprintf(stderr,"Cannot stat input file \"%s\"\n", inName);
		exit(1);
	}

	if(!(fin=fopen(argv[1],"rb")))
	{
		fprintf(stderr,"Cannot open input file \"%s\"\n", inName);
		exit(1);
	}

	outName = changeNameExt(inName, "rdplog.txt");

	if(!outName || (rdplog=fopen(outName, "wt"))==NULL)
	{
		fprintf(stderr, "Cannot open log file \"%s\"\n", outName);
		fclose(fin);
		exit(1);
	}
	free(outName);
	outName=0;

	fprintf(rdplog, "Marvell RAMDUMP parser, version %s\n", VERSION_STRING);
	fprintf(rdplog, "Parsing input file: \"%s\" size 0x%.8x\n", inName, (unsigned)fst.st_size);
	rdpPath = argv[0];

	switch (check_elf(fin)) {
	case 1:
		fprintf(rdplog, "Input file is in ELF format\n");
		break;
	case 2:
		fprintf(rdplog, "Input file is in ELF64 format\n");
		aarch_type = aarch64;
		break;
	case -1:
		fprintf(rdplog, "Error: Input file is in ELF format but is not valid\n");
		goto bail;
	case 0:
		fprintf(rdplog, "Input file is in BINARY format\n");
		/* Set up initial mapping so FILE_OFFSET works */
		segments[0].size = (unsigned)fst.st_size;
		break;
	}

	if(findRdc(fin)) {
		fprintf(rdplog, "RDC not found. Try parsing COMM-ONLY...\n");
		ret = extractPostmortemfiles(inName, fin, 1);
		if (!DUMP_IS_ELF) /* Without RDC and ELF header we do not have memory map info needed to proceed */
			goto bail;
	} else {
		//RDC found, but could be COM_DDR_RW_FULL_BIN_NAME
		char *basename = strstr(inName, "\\" COM_DDR_RW_FULL_BIN_NAME);
		if(basename && (strlen(basename) == strlen("\\" COM_DDR_RW_FULL_BIN_NAME))) {
			fprintf(rdplog, "Input file is COMM. Parse COMM-ONLY. APPS ignored\n");
			ret = extractPostmortemfiles(inName, fin, 1);
			goto bail;
		}
	}

	if (!DUMP_IS_ELF)
		/* NOT ELF: sanity check the size info and assign the remaining file space to bank 1 if relevant */
		if ((unsigned)fst.st_size > segments[0].size) {
			segments[1].size = fst.st_size - segments[0].size;
			/* Only Tavor physical memory map supported discontinuous dual-bank space */
			segments[1].pa = segments[0].pa == DDR0_BASE ? DDR1_BASE : segments[0].pa + segments[0].size;
			segments[1].foffs = segments[0].foffs + segments[0].size;
		} else if ((unsigned)fst.st_size < segments[0].size) {
			segments[0].size = fst.st_size;
		}

	/* OK, found RDC */
	fprintf(rdplog, "Found RDC, CP area at offset 0x%.8x, RDC at offset 0x%.8x\n", cpmem_addr, rdc_addr);
	for (i = 0, dump_total_size = 0; (i < MAX_SEGMENTS) && segments[i].size; i++) {
		fprintf(rdplog, "Segments[%d]: physical [0x%.8x-0x%.8x] at file offset 0x%.8x\n",
			i, segments[i].pa, segments[i].pa + segments[i].size, segments[i].foffs);
		dump_total_size += segments[i].size;
	}

#if defined(CONFIG_GENERATE_PER_BANK_FILES)
	// Split RAMDUMP into two bank files: not needed, RAM contents is loaded into simulator directly from the original file.
	if (segments[1].size) {
		for (int is = 0; is < MAX_SEGMENTS; is++) {
			char sname[30];
			sprintf(sname, "DDR0_0x%.8x.bin", segments[is].pa);
			if(extractFile(inName, sname, fin, segments[is].pa, segments[is].size, 0, 0)) goto bail;
		}
	}
#endif

	/* Extract ISRAM */
	if(extractFile(inName, apSramFileName, fin,
		FILE_OFFSET(RDC_ISRAM_START(rdc_addr, rdc.header.isram_size)), rdc.header.isram_size, 0, 0)) goto bail;

	/* Extract RAMFILES */
	if(extractRamfiles(inName, fin)) {
		fprintf(rdplog, "RAMFILE list is broken! Some or all ramfile's have been skipped.\n");
		//goto bail;
	}

	/* Extract CP post-mortem files from CP DDR RW area */
	extractPostmortemfiles(inName, fin, 0); /* continue even if fails - depend on CP data */

	if(rdc.header.mipsram_pa) {
		if(extractFile(inName, "ap_mipsram.bin", fin,
			FILE_OFFSET(rdc.header.mipsram_pa), rdc.header.mipsram_size, 0, 0)) goto bail;
	}

	/* Generate the ramdump.cmm file even if rdc.header.ramdump_data_addr is 0 (function takes care of this case) */
	if (aarch_type == aarch32)
		extractCpuState(inName, "ramdump.cmm", fin, rdc.header.ramdump_data_addr);
	else
		extractCpuState_64(inName, "ramdump.cmm", fin, rdc.header.ramdump_data_addr);

	/* Extract the RDI objects based on the descriptors in the RDC body */
	extractRdiObjects(inName, fin);


	/* Generate the ELF header for use with crash utility with binary dump */
	if (!DUMP_IS_ELF)
		writeElfHeader(inName, "emmd_elf_head.bin");
	ret = 0;

bail:
	fclose(fin);
//	if(outName!=argv[2]) free(outName);
	if(!ret)
		fprintf(rdplog,"Done\n");
	else
		fprintf(rdplog,"Failed, no output produced\n");
	if(!ret)
		fprintf(stderr,"Done\n");
	else
		fprintf(stderr,"Failed, no output produced\n");
	if(rdplog) fclose(rdplog);

	exit(ret);
	return ret;
}
