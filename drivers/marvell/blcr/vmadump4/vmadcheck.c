/*-------------------------------------------------------------------------
 *  vmadcheck.c:  Dump file sanity checker (for debugging only)
 *
 *  Copyright (C) 1999-2001 by Erik Hendriks <erik@hendriks.cx>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: vmadcheck.c,v 1.7 2008/04/26 20:04:51 phargrov Exp $
 *-----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <signal.h>

/* Including these files is fairly perilous. */
#include <asm/page.h>
#include <asm/ptrace.h>
struct task_struct;
#include <asm/types.h>

#if defined(__i386__)
/*#include <asm/processor.h>*/
#endif

#if defined(__x86_64__)
#endif

#include "vmadump.h"

/* This stuff is here because the kernel and glibc disagree about signal datatypes */
#undef _NSIG
#undef _NSIG_BPW
#undef _NSIG_WORDS
#define _NSIG           64
#define _NSIG_BPW       (sizeof(long)*8)
#define _NSIG_WORDS     (_NSIG / _NSIG_BPW)
typedef struct {
        unsigned long sig[_NSIG_WORDS];
} k_sigset_t;

struct k_sigaction {
    void *ksa_handler;
    unsigned long ksa_flags;
    void *ksa_restorer;
    k_sigset_t ksa_mask;
};

static
void read_check(int fd, void *p, size_t bytes) {
    int r;
    r = read(fd, p, bytes);
    if (r < 0) {
	fprintf(stderr, "Read error: %s\n", strerror(errno));
	exit(1);
    }
    if (r != bytes) {
	printf("Short file.\n");
	exit(1);
    }
}

static
void read_skip(int fd, size_t bytes) {
    int len;
    char buffer[PAGE_SIZE];

    while (bytes > 0) {
	len = bytes > PAGE_SIZE ? PAGE_SIZE : bytes;
	read_check(fd, buffer, len);
	bytes -= len;
    }
}

#define PTRWIDTH ((int)(sizeof(void *)*2))
char *arch_names[] = {"","i386","sparc", "alpha", "ppc", "x86_64", "ppc64"};
#define MAX_ARCH ((sizeof(arch_names)/sizeof(char *))-1)

static char vmad_magic[4] = VMAD_MAGIC;
int main(int argc, char *argv[]) {
    int fd, i;
    char filename[PAGE_SIZE];
    struct vmadump_header      head;
    struct vmadump_vma_header  map;
    struct vmadump_page_header page;
    struct vmadump_mm_info     mm;
    struct vmadump_hook_header hook;
    struct pt_regs regs;
    k_sigset_t blocked;
    struct k_sigaction sa;

    if (argc != 2) {
	fprintf(stderr, "Usage: %s dumpfile\n", argv[0]);
	exit(0);
    }
    
    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
	perror(argv[1]);
	exit(1);
    }

    read_check(fd, &head, sizeof(head));
    if (memcmp(head.magic, vmad_magic, sizeof(head.magic))) {
	printf("Invalid magic number.\n");
	exit(1);
    }
    printf("dump version: %d\n", (int) head.fmt_vers);
    if (head.fmt_vers != VMAD_FMT_VERS) {
	fprintf(stderr, "I only know about dump format version %d.  (Sorry)\n", VMAD_FMT_VERS);
	exit(1);
    }
    printf("kernel: %d.%d.%d\n",
	   (int) head.major, (int) head.minor, (int) head.patch);
    if (head.arch <= MAX_ARCH)
	printf("arch:   %s\n", arch_names[head.arch]);
    else
	printf("arch:   %d (unknown arch)\n", (int) head.arch);

    if (head.arch != VMAD_ARCH) {
	fprintf(stderr, "I can only inspect dumps from my own architecture.  (Sorry)\n");
	exit(1);
    }

    read_check(fd, filename, 16);
    printf("comm:   %s\n", filename);

    {	int pid;
	read_check(fd, &pid, sizeof(pid));
	printf("pid:   %d\n", pid);
    }

    read_check(fd, &regs, sizeof(regs));

    /*---------------------------------------------------------------------
     *  There are a bunch of magical numbers being used in here for
     *  different cpu specific structure sizes.  These are (were)
     *  correct but they might change at some point.
     *-------------------------------------------------------------------*/
#if defined(__i386__)
    {
	uint32_t fpu[128];  /* union i387_union */
	long debug[8], i;
	char flag;
	struct {
	    uint32_t a, b;
	} tls[3];    /* 3 = GDT_ENTRY_TLS_ENTRIES */
	uint32_t fs, gs;

	printf("regs:   eax=%08lx  ebx=%08lx  ecx=%08lx  edx=%08lx\n"
	       "regs:   esi=%08lx  edi=%08lx  esp=%08lx  ebp=%08lx\n"
	       "regs:   eip=%08lx flgs=%08lx   cs=%04x      ss=%04x\n"
	       "regs:    ds=%04x       es=%04x\n",
	       regs.eax, regs.ebx, regs.ecx, regs.edx,
	       regs.esi, regs.edi, regs.esp, regs.ebp,
	       regs.eip, regs.eflags, regs.xcs, regs.xss,
	       regs.xds, regs.xes);
	read_check(fd, &flag, sizeof(flag));
	if (flag) {
	    read_check(fd, &fpu, sizeof(fpu));
	    printf("fpu:    FPU state present.\n");
	} else
	    printf("fpu:    No FPU state.\n");

	read_check(fd, debug, sizeof(long)*8);
	printf("db:    ");
	for (i=0; i < 8; i++) printf(" %08lx", debug[i]);
	printf("\n");

	read_check(fd, &tls, sizeof(tls));
#define GET_BASE(desc) ( \
        (((desc)->a >> 16) & 0x0000ffff) | \
        (((desc)->b << 16) & 0x00ff0000) | \
        ( (desc)->b        & 0xff000000)   )
#define GET_LIMIT(desc) ( \
        ((desc)->a & 0x0ffff) | \
         ((desc)->b & 0xf0000) )
#define GET_PRESENT(desc)       (((desc)->b >> 15) & 1)	
	for (i=0; i < 3; i++)
	    if (GET_PRESENT(&tls[i]))
		printf("tls:    %ld  base=0x%08x limit=0x%08x\n", i,
		       GET_BASE(&tls[i]), GET_LIMIT(&tls[i]));
	    else
		printf("tls:    %ld  (not present)\n", i);

	read_check(fd, &fs, sizeof(fs));
	read_check(fd, &gs, sizeof(gs));
	printf("seg:    fs=0x%04x gs=0x%04x\n", (int) fs, (int) gs);
    }
#elif defined(__sparc__)
    {
	int i;
	printf("regs:   pc=%08lx  npc=%08lx  y=%08lx\n",
	       regs.pc, regs.npc, regs.y);
	printf("regs: G");
	for (i=UREG_G0; i <=UREG_G7; i++) printf(" %08lx", regs.u_regs[i]);
	printf("\n");
	printf("regs: I");
	for (i=UREG_I0; i <=UREG_I7; i++) printf(" %08lx", regs.u_regs[i]);
	printf("\n");
    }
#elif defined(__alpha__)
    {
	struct switch_stack ss;
	unsigned long usp;
	read_check(fd, &ss,  sizeof(ss));
	read_check(fd, &usp, sizeof(usp));
	printf("regs: pc = %016lx  ra = %016lx  ps = %04lx\n"
	       "regs: r0 = %016lx  r1 = %016lx  r2 = %016lx\n"
	       "regs: r3 = %016lx  r4 = %016lx  r5 = %016lx\n"
	       "regs: r6 = %016lx  r7 = %016lx  r8 = %016lx\n"
	       "regs: r9 = %016lx  r10= %016lx  r11= %016lx\n"
	       "regs: r12= %016lx  r13= %016lx  r14= %016lx\n"
	       "regs: r15= %016lx\n"
	       "regs: r16= %016lx  r17= %016lx  r18= %016lx\n"
	       "regs: r19= %016lx  r20= %016lx  r21= %016lx\n"
	       "regs: r22= %016lx  r23= %016lx  r24= %016lx\n"
	       "regs: r25= %016lx  r27= %016lx  r28= %016lx\n"
	       "regs: gp = %016lx  sp = %016lx\n",
               regs.pc,  regs.r26, regs.ps,
               regs.r0,  regs.r1,  regs.r2,
               regs.r3,  regs.r4,  regs.r5,
               regs.r6,  regs.r7,  regs.r8,
               ss.r9,    ss.r10,   ss.r11,
               ss.r12,   ss.r13,   ss.r14,
               ss.r15,
               regs.r16, regs.r17, regs.r18,
	       regs.r19, regs.r20, regs.r21,
               regs.r22, regs.r23, regs.r24,
               regs.r25, regs.r27, regs.r28,
	       regs.gp, usp);
    }
#elif defined(powerpc)
    {
	double fpr[32];
	unsigned long fpscr;
	typedef struct {
	    __u32 u[4];
	} vector128;
	vector128 vr[32];
	vector128 vscr;
	read_check(fd, fpr, sizeof(fpr));
	read_check(fd, &fpscr, sizeof(fpscr));
	read_check(fd, vr, sizeof(vr));
	read_check(fd, &vscr, sizeof(vscr));

	for (i=0; i < 32; i++) {
	    if (i%4 == 0) printf("regs:");
	    printf(" r%02d = %08lx", i, regs.gpr[i]);
	    if (i%4 == 3) printf("\n");
	}

	for (i=0; i < 32; i++) {
	    if (i%4 == 0) printf("regs:");
	    printf(" fp%02d = %f", i, fpr[i]);
	    if (i%4 == 3) printf("\n");
	}
	printf("regs: fpscr = %08lx\n", fpscr);

	/* XXX We're presuming altivec is present here */
	for (i=0; i < 32; i++) {
	    if (i%2 == 0) printf("regs:");
	    printf(" av%02d = %08x%08x%08x%08x", i,
		   vr[i].u[0], vr[i].u[1], vr[i].u[2], vr[i].u[3]);
	    if (i%2 == 1) printf("\n");
	}
	printf("regs: vscr = %08x%08x%08x%08x\n",
	       vscr.u[0], vscr.u[1], vscr.u[2], vscr.u[3]);
    }
#elif defined(__x86_64__)
    {
	struct i387_fxsave_struct {
	    uint16_t     cwd;
	    uint16_t     swd;
	    uint16_t     twd;
	    uint16_t     fop;
	    uint64_t     rip;
	    uint64_t     rdp; 
	    uint32_t     mxcsr;
	    uint32_t     mxcsr_mask;
	    uint32_t     st_space[32];
	    uint32_t     xmm_space[64];
	    uint32_t     padding[24];
	} __attribute__ ((aligned (16)));
	union i387_union {
	    struct i387_fxsave_struct       fxsave;
	};

	union i387_union fpu;
	long debug[8], i;
	char flag;
	unsigned long fs, gs;
	uint16_t fsindex, gsindex;
	struct {
	    uint32_t a, b;
	} tls[3];    /* 3 = GDT_ENTRY_TLS_ENTRIES */
	printf("regs:   rax=%016lx rbx=%016lx\n"
	       "regs:   rcx=%016lx rdx=%016lx\n"
	       "regs:   rbp=%016lx rsi=%016lx\n"
	       "regs:   rdi=%016lx rsp=%016lx\n"
	       "regs:   r8 =%016lx r9 =%016lx\n"
	       "regs:   r10=%016lx r11=%016lx\n"
	       "regs:   r12=%016lx r13=%016lx\n"
	       "regs:   r14=%016lx r15=%016lx\n"
	       "regs:   rip=%016lx efl=%016lx\n",
	       regs.rax, regs.rbx, regs.rcx, regs.rdx,
	       regs.rbp, regs.rsi, regs.rdi, regs.rsp,
	       regs.r8, regs.r9, regs.r10, regs.r11,
	       regs.r12, regs.r13, regs.r14, regs.r15,
	       regs.rip, regs.eflags);
	printf("regs:   cs=%04lx ss=%04lx\n", regs.cs, regs.ss);
	read_check(fd, &flag, sizeof(flag));
	if (flag) {
	    read_check(fd, &fpu, sizeof(fpu));
	    printf("fpu:    FPU state present.\n");
	} else
	    printf("fpu:    No FPU state.\n");


	printf("seg:    %d-bit Mode Process.\n", (regs.cs == 0x23) ? 32 : 64);

	/* Segments and crap */
	read_check(fd, &fs, sizeof(fs));
	read_check(fd, &gs, sizeof(gs));
	printf("seg:    fs=%016lx gs=%016lx\n", fs, gs);

	read_check(fd, &fsindex, sizeof(fsindex));
	read_check(fd, &gsindex, sizeof(gsindex));
	printf("seg:    fsindex=%04x gsindex=%04x\n", fsindex, gsindex);
	    
	read_check(fd, &tls, sizeof(tls));

#define GET_BASE(desc) ( \
        (((desc)->a >> 16) & 0x0000ffff) | \
        (((desc)->b << 16) & 0x00ff0000) | \
        ( (desc)->b        & 0xff000000)   )
#define GET_LIMIT(desc) ( \
        ((desc)->a & 0x0ffff) | \
         ((desc)->b & 0xf0000) )
#define GET_PRESENT(desc)       (((desc)->b >> 15) & 1)	
	for (i=0; i < 3; i++)
	    if (GET_PRESENT(&tls[i]))
		printf("tls:    %ld  base=0x%08x limit=0x%08x\n", i,
		       GET_BASE(&tls[i]), GET_LIMIT(&tls[i]));
	    else
		printf("tls:    %ld  (not present)\n", i);

	read_check(fd, debug, sizeof(long)*6);
	printf("db:    ");
	for (i=0; i < 3; i++) printf(" %016lx", debug[i]);
	printf("\ndb:    ");
	for (i=0; i < 3; i++) printf(" %016lx", debug[i]);
	printf("\n");
    }
#elif defined(__powerpc64__)
    {
	double fpr[32];
	unsigned long fpscr;
	typedef struct {
	    __u32 u[4];
	} vector128;
	vector128 vr[32];
	vector128 vscr;
	read_check(fd, fpr, sizeof(fpr));
	read_check(fd, &fpscr, sizeof(fpscr));
	read_check(fd, vr, sizeof(vr));
	read_check(fd, &vscr, sizeof(vscr));

	printf("regs:   %d-bit Mode Process.\n",
	       (regs.msr & (1L<<63)) ? 64 : 32);

	for (i=0; i < 32; i++) {
	    if (i%4 == 0) printf("regs:");
	    printf(" r%02d = %016lx", i, regs.gpr[i]);
	    if (i%4 == 3) printf("\n");
	}

	for (i=0; i < 32; i++) {
	    if (i%4 == 0) printf("regs:");
	    printf(" fp%02d = %f", i, fpr[i]);
	    if (i%4 == 3) printf("\n");
	}
	printf("regs: fpscr = %08lx\n", fpscr);

	/* XXX We're presuming altivec is present here */
	for (i=0; i < 32; i++) {
	    if (i%2 == 0) printf("regs:");
	    printf(" av%02d = %08x%08x%08x%08x", i,
		   vr[i].u[0], vr[i].u[1], vr[i].u[2], vr[i].u[3]);
	    if (i%2 == 1) printf("\n");
	}
	printf("regs: vscr = %08x%08x%08x%08x\n",
	       vscr.u[0], vscr.u[1], vscr.u[2], vscr.u[3]);
    }
#else
    printf("regs: no register display for this arch\n");
#endif

    /*--- Signal Information ---------------------------------------*/
    read_check(fd, &blocked, sizeof(blocked));
    printf("sigblk: ");
    for (i=0; i < _NSIG; i++) {
	if (blocked.sig[i / (sizeof(long)*8)] & (1 << (i % (sizeof(long)*8))))
	    printf(" %d", i+1);
    }
    printf("\n");

    {	k_sigset_t pending_set;
	int flags;

	read_check(fd, &flags, sizeof(flags));

	while (flags != -1) {
	    siginfo_t info;

	    read_check(fd, &info, sizeof(info));
	    read_check(fd, &flags, sizeof(flags));
	}
	read_check(fd, &pending_set, sizeof(pending_set));
	printf("private sigpend: ");
	for (i=0; i < _NSIG; i++) {
	    if (pending_set.sig[i / (sizeof(long)*8)] & (1 << (i % (sizeof(long)*8))))
		printf(" %d", i+1);
	}
	printf("\n");
	read_check(fd, &pending_set, sizeof(pending_set));
	printf("shared sigpend: ");
	for (i=0; i < _NSIG; i++) {
	    if (pending_set.sig[i / (sizeof(long)*8)] & (1 << (i % (sizeof(long)*8))))
		printf(" %d", i+1);
	}
	printf("\n");
    }
    
    for (i=0; i < _NSIG; i++) {
	read_check(fd, &sa, sizeof(sa));
	if (sa.ksa_handler == 0) continue;
	printf("sigact: %2d", i+1);
	if (sa.ksa_handler == (void *)1)
	    printf(" IGNORED");
	else {
	    printf(" handler=%0*lx", PTRWIDTH,(long)sa.ksa_handler);
#if 0
	if (blocked[i / (sizeof(long)*8)] & (1 << (i % (sizeof(long)*8))))
	    printf(" %d", i+1);
#endif
	}
	printf("\n");
    }

    /*--- Misc process information ---------------------------------*/
    {
	void *clear_child_tid;
	unsigned long personality;
	read_check(fd, &clear_child_tid, sizeof(clear_child_tid));
	read_check(fd, &personality, sizeof(personality));

	printf("child_tid: %p\n", clear_child_tid);
	printf("personality: %p\n", (void*)personality);
    }

    /*--- Memory meta-data -----------------------------------------*/
    read_check(fd, &mm, sizeof(mm));
    printf("code:   %0*lx-%0*lx\n", PTRWIDTH, mm.start_code, PTRWIDTH, mm.end_code);
    printf("data:   %0*lx-%0*lx\n", PTRWIDTH, mm.start_data, PTRWIDTH, mm.end_data);
    printf("brk:    %0*lx-%0*lx\n", PTRWIDTH, mm.start_brk,  PTRWIDTH, mm.brk);
    printf("stack:  %*s-%0*lx\n",   PTRWIDTH, "",            PTRWIDTH, mm.start_stack);
    printf("arg:    %0*lx-%0*lx\n", PTRWIDTH, mm.arg_start,  PTRWIDTH, mm.arg_end);
    printf("env:    %0*lx-%0*lx\n", PTRWIDTH, mm.env_start,  PTRWIDTH, mm.env_end);

    /*--- Memory data ----------------------------------------------*/
    read_check(fd, &map, sizeof(map));
    while (map.start != ~0 || map.end != ~0) {
	if (map.namelen) {
	    if (map.namelen > PAGE_SIZE) {
		printf("Map has invalid name length: %d\n", (int) map.namelen);
		exit(1);
	    }
	    read_check(fd, filename, map.namelen);
	    filename[map.namelen]=0;
	    printf("map:    %0*lx-%0*lx %04x file %s:%0*lx\n",
		   PTRWIDTH, map.start, PTRWIDTH, map.end, (int)map.flags,
		   filename, PTRWIDTH, map.offset);
	} else {
	    printf("map:    %0*lx-%0*lx %04x (data provided)\n",
		   PTRWIDTH, map.start, PTRWIDTH, map.end, (int)map.flags);
	}
	read_check(fd, &page, sizeof(page));
	while (page.start != ~0) {
	    printf("page:   %0*lx (data provided)\n", PTRWIDTH, page.start);
	    lseek(fd, PAGE_SIZE, SEEK_CUR);
	    read_check(fd, &page, sizeof(page));
	}
	read_check(fd, &map, sizeof(map));
    }

    /*--- Read hook data -------------------------------------------*/
    read_check(fd, &hook, sizeof(hook));
    while (hook.tag[0]) {
	printf("hook:   tag=\"%s\"  bytes=%ld\n", hook.tag, hook.size);
	read_skip(fd, hook.size);
	read_check(fd, &hook, sizeof(hook));
    }

    /* Complain about trailing garbage? */
    if (read(fd, &i, sizeof(i)) != 0) {
	fprintf(stderr, "Trailing garbage at end of file.\n");
	exit(1);
    }
    exit(0);
}
  
/*
 * Local variables:
 * c-basic-offset: 4
 * End:
 */
