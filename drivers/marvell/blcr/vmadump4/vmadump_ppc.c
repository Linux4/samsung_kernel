/*-------------------------------------------------------------------------
 *  vmadump_powerpc.c:  powerpc specific dumping/undumping routines
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
 * $Id: vmadump_ppc.c,v 1.10.14.1 2011/09/16 00:26:08 phargrov Exp $
 *
 * THIS VERSION MODIFIED FOR BLCR <http://ftg.lbl.gov/checkpoint>
 *-----------------------------------------------------------------------*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>

#define __VMADUMP_INTERNAL__
#include "vmadump.h"

long vmadump_store_cpu(cr_chkpt_proc_req_t *ctx, struct file *file,
		       struct pt_regs *regs) {
    long bytes = 0, r;

    /* Store struct pt_regs */
    r = write_kern(ctx, file, regs, sizeof(*regs));
    if (r != sizeof(*regs)) goto err;
    bytes += r;

    /* Floating point regs */
    if (regs->msr & MSR_FP)
	giveup_fpu(current);
    r = write_kern(ctx, file, &current->thread.fpr,
		   sizeof(current->thread.fpr));
    if (r != sizeof(current->thread.fpr)) goto err;
    bytes += r;

    r = write_kern(ctx, file, &current->thread.fpscr,
		   sizeof(current->thread.fpscr));
    if (r != sizeof(current->thread.fpscr)) goto err;
    bytes += r;

#if HAVE_THREAD_VDSO_BASE
    /* unconditionally store the base of the VDSO library */
    r = write_kern(ctx, file, &current->thread.vdso_base,
                   sizeof(current->thread.vdso_base));
    if (r != sizeof(current->thread.vdso_base)) goto err;
    bytes += r;
#endif

#ifdef CONFIG_ALTIVEC
    /* XXX I really need to find out if this is right */
    if (regs->msr & MSR_VEC)
	giveup_altivec(current);
    r = write_kern(ctx, file, &current->thread.vr,
		   sizeof(current->thread.vr));
    if (r != sizeof(current->thread.vr)) goto err;
    bytes += r;

    r = write_kern(ctx, file, &current->thread.vscr,
		   sizeof(current->thread.vscr));
    if (r != sizeof(current->thread.vscr)) goto err;
    bytes += r;
#endif
    return bytes;

 err:
    if (r >= 0) r = -EIO;
    return r;
}


int vmadump_restore_cpu(cr_rstrt_proc_req_t *ctx, struct file *file,
			struct pt_regs *regs) {
    struct pt_regs regtmp;
    int r;

    r = read_kern(ctx, file, &regtmp, sizeof(regtmp));
    if (r != sizeof(regtmp)) goto bad_read;

    /* Don't restore machine state register since this is
     * unpriviledged user space stuff we're restoring. */
    regtmp.msr = MSR_USER; /*regs->msr | MSR_PR;*/
    memcpy(regs, &regtmp, sizeof(regtmp));

    /* Floating point regs */
    r = read_kern(ctx, file, &current->thread.fpr,
		  sizeof(current->thread.fpr));
    if (r != sizeof(current->thread.fpr)) goto bad_read;

    r = read_kern(ctx, file, &current->thread.fpscr,
		  sizeof(current->thread.fpscr));
    if (r != sizeof(current->thread.fpscr)) goto bad_read;

#if HAVE_THREAD_VDSO_BASE
    /* unconditonally restore this */
    r = read_kern(ctx, file, &current->thread.vdso_base,
		  sizeof(current->thread.vdso_base));
    if (r != sizeof(current->thread.vdso_base)) goto bad_read;
#endif

#ifdef CONFIG_ALTIVEC
    /* Restore Altivec */
    r = read_kern(ctx, file, &current->thread.vr,
		  sizeof(current->thread.vr));
    if (r != sizeof(current->thread.vr)) goto bad_read;

    r = read_kern(ctx, file, &current->thread.vscr,
		  sizeof(current->thread.vscr));
    if (r != sizeof(current->thread.vscr)) goto bad_read;
#endif

    current->thread.regs = regs;
    return 0;

 bad_read:
    if (r >= 0) r = -EIO;
    return r;
}


#if VMAD_HAVE_ARCH_MAPS
#include <linux/mman.h>
#if HAVE_THREAD_VDSO_BASE
    #define vmad_vdso_base	current->thread.vdso_base
#elif HAVE_MM_CONTEXT_VDSO_BASE
    #define vmad_vdso_base	current->mm->context.vdso_base
#else
    #error "No support yet for VDSO on your kernel - please report as a BLCR bug"
#endif

int vmad_is_arch_map(const struct vm_area_struct *map)
{
	return (map->vm_start == vmad_vdso_base);
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

  #if HAVE_2_ARG_ARCH_SETUP_ADDITIONAL_PAGES
    r = arch_setup_additional_pages(NULL, 0);
  #elif HAVE_4_ARG_ARCH_SETUP_ADDITIONAL_PAGES
    r = arch_setup_additional_pages(NULL, 0, 0, 0);
  #else
    #error "Unknown calling convention to map the VDSO"
  #endif
    if (r < 0) {
	CR_ERR_CTX(ctx, "arch_setup_additional_pages failed %d", (int)r);
	goto err;
    }

    /* arch_setup_additional_pages() has overwritten vdso_base w/ the newly allocated one.
     * Here we check this new value against against the desired location (in head->start).
     */
    if (head->start != vmad_vdso_base) {
	r = vmad_remap(ctx, (unsigned long)vmad_vdso_base, head->start, head->end - head->start);
	if (r) {
	    CR_ERR_CTX(ctx, "vdso remap failed %d", (int)r);
	    goto err;
	}
	vmad_vdso_base = head->start;
    }

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
