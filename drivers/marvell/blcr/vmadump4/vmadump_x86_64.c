/*-------------------------------------------------------------------------
 *  vmadump_x86_64.c:  x86-64 specific dumping/undumping routines
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
 * $Id: vmadump_x86_64.c,v 1.50.8.4 2011/09/16 00:26:08 phargrov Exp $
 *
 * THIS VERSION MODIFIED FOR BLCR <http://ftg.lbl.gov/checkpoint>
 *-----------------------------------------------------------------------*/
#ifdef CR_NEED_AUTOCONF_H
#include <linux/autoconf.h>
#endif
#if defined(CONFIG_SMP) && ! defined(__SMP__)
#define __SMP__
#endif

#define __FRAME_OFFSETS		/* frame offset macros from ptrace.h */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>

#define __VMADUMP_INTERNAL__
#include "vmadump.h"
#include "vmadump_x86.h"

#ifndef savesegment
#define savesegment(seg, value) \
        asm volatile("mov %%" #seg ",%0":"=m" (*(int *)&(value)))
#endif

#if HAVE_WRITE_PDA && HAVE_READ_PDA
  #define vmad_write_oldrsp(_val) write_pda(oldrsp, (_val))
  #define vmad_read_oldrsp()      read_pda(oldrsp)
#elif HAVE_PERCPU_WRITE && HAVE_PERCPU_READ
  #define vmad_write_oldrsp(_val) percpu_write(old_rsp, (_val))
  #define vmad_read_oldrsp()      percpu_read(old_rsp)
#else
  #error "No access to per-cpu data?"
#endif

long vmadump_store_cpu(cr_chkpt_proc_req_t *ctx, struct file *file,
		       struct pt_regs *regs) {
    long bytes = 0, r;

    /* Store struct pt_regs */
#ifdef CONFIG_XEN
    /* Ensure CS and SS are not Xen-modified, others restore based on CS */
    {   struct pt_regs regtmp = *regs;
        if (test_thread_flag(TIF_IA32)) {
            regtmp.cs = __USER32_CS;
            regtmp.ss = __USER32_DS;
        } else {
            regtmp.cs = __USER_CS;
            regtmp.ss = __USER_DS;
        }
        r = write_kern(ctx, file, &regtmp, sizeof(regtmp));
    }
#else
    r = write_kern(ctx, file, regs, sizeof(*regs));
#endif
    if (r != sizeof(*regs)) goto err;
    bytes += r;

    /* Store FPU info (and later general "extended state") */
    r = vmadump_store_i387(ctx, file);
    if (r <= 0) goto err;
    bytes += r;

    /* Store debugging state */
    r = vmadump_store_debugreg(ctx, file);
    if (r < 0) goto err;
    bytes += r;

    /* user(r)sp, since we don't use the ptrace entry path in BLCR */
#if HAVE_THREAD_USERSP
    current->thread.usersp = vmad_read_oldrsp();
    r = write_kern(ctx, file, &current->thread.usersp,
		   sizeof(current->thread.usersp));
    if (r != sizeof(current->thread.usersp)) goto err;
#elif HAVE_THREAD_USERRSP
    current->thread.userrsp = vmad_read_oldrsp();
    r = write_kern(ctx, file, &current->thread.userrsp,
		   sizeof(current->thread.userrsp));
    if (r != sizeof(current->thread.userrsp)) goto err;
#else
    #error
#endif
    bytes += r;

    /* Store all weird segmenty crap */

    /* 64-bit offsets for FS and GS */
    r = write_kern(ctx, file, &current->thread.fs,
		   sizeof(current->thread.fs));
    if (r != sizeof(current->thread.fs)) goto err;
    bytes += r;
    
    r = write_kern(ctx, file, &current->thread.gs,
		   sizeof(current->thread.gs));
    if (r != sizeof(current->thread.gs)) goto err;
    bytes += r;

    savesegment(fs,current->thread.fsindex);
    savesegment(gs,current->thread.gsindex);

    /* 32-bit segment descriptors for FS and GS */
    r = write_kern(ctx, file, &current->thread.fsindex,
		   sizeof(current->thread.fsindex));
    if (r != sizeof(current->thread.fsindex)) goto err;
    bytes += r;
    
    r = write_kern(ctx, file, &current->thread.gsindex,
		   sizeof(current->thread.gsindex));
    if (r != sizeof(current->thread.gsindex)) goto err;
    bytes += r;

    /* TLS segment descriptors */
    r = write_kern(ctx, file, &current->thread.tls_array,
		   sizeof(current->thread.tls_array));
    if (r != sizeof(current->thread.tls_array)) goto err;
    bytes += r;

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
    int idx, i, cpu;
    uint16_t fsindex, gsindex;
#if HAVE_STRUCT_N_DESC_STRUCT
    struct n_desc_struct tls_array[GDT_ENTRY_TLS_ENTRIES];
#else
    struct desc_struct tls_array[GDT_ENTRY_TLS_ENTRIES];
#endif

    /* XXX: Note allocation assumes i387tmp and threadtmp are never active at the same time */
    x86tmps = kmalloc(sizeof(*x86tmps), GFP_KERNEL);
    if (!x86tmps) return -ENOMEM;
    regtmp = VMAD_REGTMP(x86tmps);
    threadtmp = VMAD_THREADTMP(x86tmps);

    r = read_kern(ctx, file, regtmp, sizeof(*regtmp));
    if (r != sizeof(*regtmp)) goto bad_read;

    /* Don't let the user pick funky segments */
    if ((regtmp->cs != __USER_CS && regtmp->cs != __USER32_CS) &&
	(regtmp->ss != __USER_DS && regtmp->ss != __USER32_DS)) {
	r = -EINVAL;
	goto bad_read;
    }

    /* Set our process type */
    if (regtmp->cs == __USER32_CS)
	set_thread_flag(TIF_IA32);
    else
	clear_thread_flag(TIF_IA32);	

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

    /* user(r)sp, since we don't use the ptrace entry path in BLCR */
#if HAVE_THREAD_USERSP
    r = read_kern(ctx, file, &threadtmp->usersp, sizeof(threadtmp->usersp));
    if (r != sizeof(threadtmp->usersp)) goto bad_read;
    current->thread.usersp = threadtmp->usersp;
    vmad_write_oldrsp(threadtmp->usersp);
#elif HAVE_THREAD_USERRSP
    r = read_kern(ctx, file, &threadtmp->userrsp, sizeof(threadtmp->userrsp));
    if (r != sizeof(threadtmp->userrsp)) goto bad_read;
    current->thread.userrsp = threadtmp->userrsp;
    vmad_write_oldrsp(threadtmp->userrsp);
#else
    #error
#endif

    /*-- restore segmentation related stuff */

    /* Restore FS_BASE MSR */
    r = read_kern(ctx, file, &threadtmp->fs, sizeof(threadtmp->fs));
    if (r != sizeof(threadtmp->fs)) goto bad_read;
    if (threadtmp->fs >= TASK_SIZE) {
	r = -EINVAL;
	goto bad_read;
    }
    current->thread.fs = threadtmp->fs;
    if ((r = checking_wrmsrl(MSR_FS_BASE, threadtmp->fs)))
	goto bad_read;
	
    /* Restore GS_KERNEL_BASE MSR */
    r = read_kern(ctx, file, &threadtmp->gs, sizeof(threadtmp->gs));
    if (r != sizeof(threadtmp->gs)) goto bad_read;
    if (threadtmp->gs >= TASK_SIZE) {
	r = -EINVAL;
	goto bad_read;
    }
    current->thread.gs = threadtmp->gs;
    if ((r = checking_wrmsrl(MSR_KERNEL_GS_BASE, threadtmp->gs)))
	goto bad_read;

    /* Restore 32 bit segment stuff */
    r = read_kern(ctx, file, &fsindex, sizeof(fsindex));
    if (r != sizeof(fsindex)) goto bad_read;

    r = read_kern(ctx, file, &gsindex, sizeof(gsindex));
    if (r != sizeof(gsindex)) goto bad_read;

    r = read_kern(ctx, file, tls_array, sizeof(tls_array));
    if (r != sizeof(tls_array)) goto bad_read;

    /* Sanitize fs, gs.  These segment descriptors should load one
     * of the TLS entries and have DPL = 3.  If somebody is doing
     * some other LDT monkey business, I'm currently not
     * supporting that here.  Also, I'm presuming that the offsets
     * to the GDT_ENTRY_TLS_MIN is the same in both kernels. */
    idx = fsindex >> 3;
    if (idx<GDT_ENTRY_TLS_MIN || idx>GDT_ENTRY_TLS_MAX || (fsindex&7) != 3)
	fsindex = 0;
    idx = gsindex >> 3;
    if (idx<GDT_ENTRY_TLS_MIN || idx>GDT_ENTRY_TLS_MAX || (gsindex&7) != 3)
	gsindex = 0;

    /* Sanitize the TLS entries...
     * Make sure the following bits are set/not set:
     *  bit 12   : S    =  1    (code/data - not system)
     *  bit 13-14: DPL  = 11    (priv level = 3 (user))
     *  bit 21   :      =  0    (reserved)
     *
     * If the entry isn't valid, zero the whole descriptor.
     */
    for (i=0; i < GDT_ENTRY_TLS_ENTRIES; i++) {
	if (tls_array[i].b != 0 && 
	    (tls_array[i].b & 0x00207000) != 0x00007000) {
	    r = -EINVAL;
	    goto bad_read;
	}
    }

    /* Ok load this crap */
    cpu = get_cpu();	/* load_TLS can't get pre-empted. */
    memcpy(current->thread.tls_array, tls_array,
	   sizeof(current->thread.tls_array));
    current->thread.fsindex = fsindex;
    current->thread.gsindex = gsindex;
    load_TLS(&current->thread, cpu);

    loadsegment(fs, current->thread.fsindex);
    load_gs_index(current->thread.gsindex);
    put_cpu();

    /* In case cr_restart and child don't have same ABI */
    if (regtmp->cs == __USER32_CS) {
	loadsegment(ds, __USER32_DS);
	loadsegment(es, __USER32_DS);
    } else {
	loadsegment(ds, __USER_DS);
	loadsegment(es, __USER_DS);
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
    if (r >= 0) r = -EIO;
    return r;
}


#if VMAD_HAVE_ARCH_MAPS
  #include <linux/mman.h>
  #if HAVE_ASM_VSYSCALL32_H
    #include <asm/vsyscall32.h>
  #endif

  #if HAVE_MM_CONTEXT_VDSO
    /* Used by both 32-bit and 64-bit since 2.6.23 */
    #define vmad_vdso_base	current->mm->context.vdso
  #elif defined(VSYSCALL32_BASE)
    /* 32-bit at a fixed offset */
    #define vmad_vdso_base	(test_thread_flag(TIF_IA32) ? VSYSCALL32_BASE : 1UL)
  #else
    #error "No support yet for VDSO on your kernel - please report as a BLCR bug"
  #endif

int vmad_is_arch_map(const struct vm_area_struct *map)
{
    unsigned long vdso_base = (unsigned long) vmad_vdso_base;

  #if HAVE_MM_CONTEXT_VDSO && defined(VSYSCALL32_BASE)
    /* Some RHEL5 kernels use fix the VSYSCALL32_BASE for 32-bit tasks and 
     * current->mm->context.vdso for 64-bit tasks.  Since we need to
     * assign to vmad_vdso_base, it isn't convenient to redefine it. */
     	if (test_thread_flag(TIF_IA32)) { 
		vdso_base = VSYSCALL32_BASE;
	} 
  #endif

	return (map->vm_start == vdso_base);
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
    if (test_thread_flag(TIF_IA32)) {
	r = syscall32_setup_pages(NULL, 0);
    } else {
  #if HAVE_2_ARG_ARCH_SETUP_ADDITIONAL_PAGES
	r = arch_setup_additional_pages(NULL, 0);
  #elif HAVE_4_ARG_ARCH_SETUP_ADDITIONAL_PAGES
	r = arch_setup_additional_pages(NULL, 0, 0, 0);
  #else
    	r = -EINVAL;
  #endif
    }
    if (r < 0) {
	CR_ERR_CTX(ctx, "arch_setup_additional_pages failed %d", (int)r);
	goto err;
    }

  #if HAVE_MM_CONTEXT_VDSO
    /* Call above should have overwritten vmad_vdso_base with a new value.
     * Relocate if needed here.
     */
    if (vmad_vdso_base == (void *)(~0UL)) {
	/* The call above didn't overwrite mm->context.vdso.
	 * Since no failure was indictated we just fill it in.
	 */
	vmad_vdso_base = (void *)head->start;
    #if defined(VSYSCALL32_BASE)
        /* we probably didn't want to do that. */
        if (test_thread_flag(TIF_IA32)) {
            /* this seems to work, is there a better way? */
            vmad_vdso_base = (void *)(0UL);
        }
    #endif
    } else if (vmad_vdso_base != (void *)head->start) {
	r = vmad_remap(ctx, (unsigned long)vmad_vdso_base, head->start, head->end - head->start);
	if (r) {
	    CR_ERR_CTX(ctx, "vdso remap failed %d", (int)r);
	    goto err;
	}
	vmad_vdso_base = (void *)head->start;
    }
  #else
    /* VSYSCALL32_BASE is a fixed value */
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
