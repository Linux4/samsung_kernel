/*-------------------------------------------------------------------------
 *  vmadump_i386.c:  i386 specific dumping/undumping routines
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: vmadump_i386.c,v 1.38.8.2 2011/09/16 00:26:08 phargrov Exp $
 *
 * THIS VERSION MODIFIED FOR BLCR <http://ftg.lbl.gov/checkpoint>
 *-----------------------------------------------------------------------*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>

#define __VMADUMP_INTERNAL__
#include "vmadump.h"
#include "vmadump_x86.h"

long vmadump_store_cpu(cr_chkpt_proc_req_t *ctx, struct file *file,
		       struct pt_regs *regs) {
    struct pt_regs regtmp;
    long bytes = 0, r;
    

    /* Zero upper 2 bytes of the stored segment registers.
     * These bytes are uninitialized and could, in theory, produce
     * a leak of kernel information if saved to the dump file.
     */
    regtmp = *regs;
#if HAVE_PT_REGS_XCS
    regtmp.xcs &= 0xffff;
    regtmp.xds &= 0xffff;
    regtmp.xes &= 0xffff;
    regtmp.xss &= 0xffff;
  #if HAVE_PT_REGS_XFS
    regtmp.xfs &= 0xffff;
  #endif
  #if HAVE_PT_REGS_XGS
    regtmp.xgs &= 0xffff;
  #endif
#elif HAVE_PT_REGS_CS
    regtmp.cs &= 0xffff;
    regtmp.ds &= 0xffff;
    regtmp.es &= 0xffff;
    regtmp.ss &= 0xffff;
  #if HAVE_PT_REGS_FS
    regtmp.fs &= 0xffff;
  #endif
  #if HAVE_PT_REGS_GS
    regtmp.gs &= 0xffff;
  #endif
#else
  #error
#endif

    /* Store struct pt_regs */
    r = write_kern(ctx, file, &regtmp, sizeof(regtmp));
    if (r != sizeof(regtmp)) goto err;
    bytes += r;

    /* Store FPU info (and later general "extended state") */
    r = vmadump_store_i387(ctx, file);
    if (r <= 0) goto err;
    bytes += r;

    /* Store debugging state */
    r = vmadump_store_debugreg(ctx, file);
    if (r <= 0) goto err;
    bytes += r;

    /* Store TLS (segment) information */
    r = write_kern(ctx, file, &current->thread.tls_array,
		   sizeof(current->thread.tls_array));
    if (r != sizeof(current->thread.tls_array)) goto err;
    bytes += r;

  #if !(HAVE_PT_REGS_XFS || HAVE_PT_REGS_FS)
    savesegment(fs,current->thread.fs);
    current->thread.fs &= 0xffff;
    r = write_kern(ctx, file, &current->thread.fs, sizeof(current->thread.fs));
    if (r != sizeof(current->thread.fs)) goto err;
    bytes += r;
  #endif

  #if !(HAVE_PT_REGS_XGS || HAVE_PT_REGS_GS)
    savesegment(gs,current->thread.gs);
    current->thread.gs &= 0xffff;
    r = write_kern(ctx, file, &current->thread.gs, sizeof(current->thread.gs));
    if (r != sizeof(current->thread.gs)) goto err;
    bytes += r;
  #endif

#if HAVE_THREAD_INFO_SYSENTER_RETURN
    {
	void *sysenter_return = current_thread_info()->sysenter_return;
	r = write_kern(ctx, file, &sysenter_return, sizeof(sysenter_return));
	if (r != sizeof(sysenter_return)) goto err;
    }
#endif

    return bytes;

 err:
    if (r >= 0) r = -EIO;
    return r;
}

int vmadump_restore_cpu(cr_rstrt_proc_req_t *ctx, struct file *file,
			struct pt_regs *regs) {
    struct vmadump_restore_tmps *x86tmps;
    struct thread_struct *threadtmp;
    struct pt_regs *regtmp;
    int r;
  #if HAVE_PT_REGS_XFS || HAVE_PT_REGS_FS
    unsigned long saved_fs;
  #endif
  #if HAVE_PT_REGS_XGS || HAVE_PT_REGS_GS
    unsigned long saved_gs;
  #endif

    /* XXX: Note allocation assumes i387tmp and threadtmp are never active at the same time */
    x86tmps = kmalloc(sizeof(*x86tmps), GFP_KERNEL);
    if (!x86tmps) return -ENOMEM;
    regtmp = VMAD_REGTMP(x86tmps);
    threadtmp = VMAD_THREADTMP(x86tmps);

    r = read_kern(ctx, file, regtmp, sizeof(*regtmp));
    if (r != sizeof(*regtmp)) goto bad_read;

    /* Don't let the user pick bogo-segregs.  Restoring other values
     * will either lead us to fault while restoring or worse it might
     * allow users to do bad(tm) things in kernel space. */
#if HAVE_PT_REGS_XCS
    regtmp->xcs = __USER_CS;
    regtmp->xds = __USER_DS;
    regtmp->xes = __USER_DS;
    regtmp->xss = __USER_DS;
  #if HAVE_PT_REGS_XFS
    saved_fs = regtmp->xfs;  /* restored after TLS and validation */
    regtmp->xfs = 0;
  #endif
  #if HAVE_PT_REGS_XGS
    saved_gs = regtmp->xgs;  /* restored after TLS and validation */
    regtmp->xgs = 0;
  #endif
#elif HAVE_PT_REGS_CS
    regtmp->cs = __USER_CS;
    regtmp->ds = __USER_DS;
    regtmp->es = __USER_DS;
    regtmp->ss = __USER_DS;
  #if HAVE_PT_REGS_FS
    saved_fs = regtmp->fs;  /* restored after TLS and validation */
    regtmp->fs = 0;
  #endif
  #if HAVE_PT_REGS_GS
    saved_gs = regtmp->gs;  /* restored after TLS and validation */
    regtmp->gs = 0;
  #endif
#else
  #error "No x86 segments in pt_regs?"
#endif

    /* Only restore bottom 9 bits of eflags.  Restoring anything else
     * is bad bad mojo for security. (0x200 = interrupt enable) */
#if HAVE_PT_REGS_EFLAGS
    regtmp->eflags = 0x200 | (regtmp->eflags & 0x000000FF);
#elif HAVE_PT_REGS_FLAGS
    regtmp->flags = 0x200 | (regtmp->flags & 0x000000FF);
#else
    #error
#endif
    memcpy(regs, regtmp, sizeof(*regtmp));

    /* Restore FPU info (and later general "extended state") */
    r = vmadump_restore_i387(ctx, file, VMAD_I387TMP(x86tmps));
    if (r < 0) goto bad_read;

    /* XXX FIX ME: RESTORE DEBUG INFORMATION ?? */
    /* Here we read it but ignore it. */
    r = vmadump_restore_debugreg(ctx, file);
    if (r < 0) goto bad_read;

    /* Restore TLS information */
    {
	unsigned long fs, gs;
	int i, idx, cpu;
	struct desc_struct tls_array[GDT_ENTRY_TLS_ENTRIES];
	
	r = read_kern(ctx, file, tls_array, sizeof(tls_array));
	if (r != sizeof(tls_array)) goto bad_read;

      #if !(HAVE_PT_REGS_XFS || HAVE_PT_REGS_FS)
	r = read_kern(ctx, file, &fs, sizeof(fs));
	if (r != sizeof(fs)) goto bad_read;
      #else
	fs = saved_fs;
      #endif

      #if !(HAVE_PT_REGS_XGS || HAVE_PT_REGS_GS)
	r = read_kern(ctx, file, &gs, sizeof(gs));
	if (r != sizeof(gs)) goto bad_read;
      #else
	gs = saved_gs;
      #endif

	/* Sanitize the TLS entries...
	 * Make sure the following bits are set/not set:
         *  bit 12   : S    =  1    (code/data - not system)
	 *  bit 13-14: DPL  = 11    (priv level = 3 (user))
	 *  bit 21   :      =  0    (reserved)
	 *
	 * If the entry isn't valid, zero the whole descriptor.
	 */
	for (i=0; i < GDT_ENTRY_TLS_ENTRIES; i++) {
	    if ((tls_array[i].b & 0x00207000) != 0x00007000)
		tls_array[i].a = tls_array[i].b = 0;
	}

	/* Sanitize fs, gs.  These segment descriptors should load one
	 * of the TLS entries and have DPL = 3.  If somebody is doing
	 * some other LDT monkey business, I'm currently not
	 * supporting that here.  Also, I'm presuming that the offsets
	 * to the GDT_ENTRY_TLS_MIN is the same in both kernels. */
	idx = fs >> 3;
	if (idx < GDT_ENTRY_TLS_MIN || idx > GDT_ENTRY_TLS_MAX || (fs&7) != 3)
	    fs = 0;
	idx = gs >> 3;
	if (idx < GDT_ENTRY_TLS_MIN || idx > GDT_ENTRY_TLS_MAX || (gs&7) != 3)
	    gs = 0;
	
	/* Load the freshly sanitized entries */
	memcpy(current->thread.tls_array, tls_array,
	       sizeof(current->thread.tls_array));

	/* load_TLS can't get pre-empted */
	cpu = get_cpu();
	load_TLS(&current->thread, cpu);
	put_cpu();

	/* this stuff will get stored in thread->fs,gs at the next
	 * context switch. */
      #if HAVE_PT_REGS_XFS
	regs->xfs = fs;
      #elif HAVE_PT_REGS_FS
	regs->fs = fs;
      #else
	loadsegment(fs, fs);
      #endif
      #if HAVE_PT_REGS_XGS
	regs->xgs = gs;
      #elif HAVE_PT_REGS_GS
	regs->gs = gs;
      #else
	loadsegment(gs, gs);
      #endif
    }

#if HAVE_THREAD_INFO_SYSENTER_RETURN
    {
	void *sysenter_return;
	r = read_kern(ctx, file, &sysenter_return, sizeof(sysenter_return));
	if (r != sizeof(sysenter_return)) goto bad_read;
	current_thread_info()->sysenter_return = sysenter_return;
    }
#endif
    
    kfree(x86tmps);
    return 0;

 bad_read:
    kfree(x86tmps);
    if (r >= 0) return -EIO;
    return r;
}


#if VMAD_HAVE_ARCH_MAPS
  #include <linux/mman.h>
  #if HAVE_ASM_VSYSCALL32_H
    #include <asm/vsyscall32.h>
  #endif

  #if HAVE_MM_CONTEXT_VDSO
    #define vmad_vdso_base current->mm->context.vdso
  #elif HAVE_VSYSCALL_BASE
    #define vmad_vdso_base VSYSCALL_BASE
  #else
    #error "No support yet for VDSO on your kernel - please report as a BLCR bug"
  #endif

int vmad_is_arch_map(const struct vm_area_struct *map)
{
	return (map->vm_start == (unsigned long)vmad_vdso_base);
}
EXPORT_SYMBOL_GPL(vmad_is_arch_map);

loff_t vmad_store_arch_map(cr_chkpt_proc_req_t *ctx, struct file *file,
			   struct vm_area_struct *map, int flags)
{
    loff_t r = 0;

    if (vmad_is_arch_map(map)) {
	/* Just write out a section header */
        struct vmadump_vma_header head;
	head.start   = map->vm_start;
	head.end     = map->vm_end;
	head.flags   = map->vm_flags;
	head.namelen = VMAD_NAMELEN_ARCH;
	head.offset  = 0;

	r = write_kern(ctx, file, &head, sizeof(head));
	if (r < 0) return r;
	if (r != sizeof(head)) r = -EIO;
    }

    return r;
}

int vmad_load_arch_map(cr_rstrt_proc_req_t *ctx, struct file *file,
		       struct vmadump_vma_header *head)
{
    long r;

  #if HAVE_THREAD_INFO_SYSENTER_RETURN
    /* Save against overwrite by arch_setup_additional_pages() */
    void *sysenter_return = current_thread_info()->sysenter_return;
  #endif

  #if HAVE_MM_CONTEXT_VDSO
    vmad_vdso_base = (void *)(~0UL);
  #endif
  #if HAVE_2_ARG_ARCH_SETUP_ADDITIONAL_PAGES
    r = arch_setup_additional_pages(NULL, 0);
    if (r < 0) {
        CR_ERR_CTX(ctx, "arch_setup_additional_pages failed %d", (int)r);
        goto err;
    }
  #elif HAVE_4_ARG_ARCH_SETUP_ADDITIONAL_PAGES
    r = arch_setup_additional_pages(NULL, 0, 0, 0);
    if (r < 0) {
        CR_ERR_CTX(ctx, "arch_setup_additional_pages failed %d", (int)r);
        goto err;
    }
  #elif HAVE_MAP_VSYSCALL
    map_vsyscall();
  #else
    #error "Unknown calling convention to map the VDSO"
  #endif

    
  #if HAVE_MM_CONTEXT_VDSO
    /* Call to arch_setup_additional_pages() or map_vsyscall() should have written
     * the new value to vmad_vdso_base.  We relocate it here if different.
     */
    if (vmad_vdso_base == (void *)(~0UL)) {
	/* The call above didn't overwrite mm->context.vdso.
	 * Since no failure was indicatated we just fill it in below.
	 */
    } else if (vmad_vdso_base != (void *)head->start) {
	r = vmad_remap(ctx, (unsigned long)vmad_vdso_base, head->start, head->end - head->start);
	if (r) {
	    CR_ERR_CTX(ctx, "vdso remap failed %d", (int)r);
	    goto err;
	}
    }
    vmad_vdso_base = (void *)head->start;
  #else
    /* VSYSCALL_BASE is a fixed location */
  #endif

  #if HAVE_THREAD_INFO_SYSENTER_RETURN
    current_thread_info()->sysenter_return = sysenter_return;
  #endif

    r = 0;
err:
    return r;
}
#endif


/*
 * Local variables:
 * c-basic-offset: 4
 * End:
 */
