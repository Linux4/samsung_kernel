/*-------------------------------------------------------------------------
 *  vmadump_alpha.c:  alpha specific dumping/undumping routines
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
 * $Id: vmadump_alpha.c,v 1.6.16.1 2010/08/12 21:58:35 phargrov Exp $
 *-----------------------------------------------------------------------*/
#define __NO_VERSION__

#ifdef CR_NEED_AUTOCONF_H
#include <linux/autoconf.h>
#endif
#if defined(CONFIG_SMP) && ! defined(__SMP__)
#define __SMP__
#endif
#if defined(CONFIG_MODVERSIONS) && ! defined(MODVERSIONS)
#define MODVERSIONS
#endif
#if defined(MODVERSIONS)
#include <linux/modversions.h>
#endif

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>

#define __VMADUMP_INTERNAL__
#include "vmadump.h"

long vmadump_store_cpu(cr_chkpt_proc_req_t *ctx, struct file *file,
		       struct pt_regs *regs) {
    struct switch_stack *ss;
    unsigned long usp;
    long bytes = 0, r;

    /* Store struct pt_regs */
    r = write_kern(ctx, file, regs, sizeof(*regs));
    if (r != sizeof(*regs)) goto err;
    bytes += r;

    /* Alpha has more registers than what's in struct ptregs to save
     * The 'switch_stack' structure contains the rest of the integer
     * registers and the floating point registers.  Then we still have
     * to go after the user's stack pointer via the PAL code. */
    /* Some pointer voodoo... the switch_stack structure got
     * pushed on top of the struct pt_regs.. */
    ss = ((struct switch_stack *) regs)-1;
    usp = rdusp();

    r = write_kern(ctx, file, ss, sizeof(*ss));
    if (r != sizeof(*ss)) goto err;
    bytes += r;

    r = write_kern(ctx, file, &usp, sizeof(usp));
    if (r != sizeof(usp)) goto err;
    bytes += r;
    return bytes;

 err:
    if (r >= 0) r = -EIO;
    return r;
}

int vmadump_restore_cpu(cr_rstrt_proc_req_t *ctx, struct file *file,
			struct pt_regs *regs) {
    struct pt_regs regtmp;
    struct switch_stack *ss, sstmp;
    unsigned long usp;
    int r;

    r = read_kern(ctx, file, &regtmp, sizeof(regtmp));
    if (r != sizeof(regtmp)) goto bad_read;

    /* Read extra reg info for alpha */
    ss = ((struct switch_stack *)regs)-1;
    r = read_kern(ctx, file, &sstmp, sizeof(sstmp));
    if (r != sizeof(sstmp)) goto bad_read;
    r = read_kern(ctx, file, &usp, sizeof(usp));
    if (r != sizeof(usp)) goto bad_read;

    /* These are the tid-bits we don't want to restore */
    regtmp.ps  = 8;		/* Processor status 8 = user space */
    regtmp.hae = regs->hae;	/* XXX What is this reg? */
    sstmp.r26  = ss->r26;	/* Not actually the user r26(ra). That
				 * one is in struct pt_regs. */
    /* Ok, do restore */
    memcpy(regs, &regtmp, sizeof(regtmp));
    memcpy(ss, &sstmp, sizeof(sstmp)); /* Restores FP regs + saved regs */
    wrusp(usp);
    return 0;

 bad_read:
    if (r >= 0) r = -EIO;
    return r;
}

#define SWITCH_STACK_SIZE "320"
/* do_switch_stack/undo_switch_stack stolen from arch/alpha/kernel/entry.S */
asm(
".align 3			\n"
".ent	save_switch_stack	\n"
"save_switch_stack:		\n"
"	lda	$30,-"SWITCH_STACK_SIZE"($30)	\n"
"	stq	$9,0($30)	\n"
"	stq	$10,8($30)	\n"
"	stq	$11,16($30)	\n"
"	stq	$12,24($30)	\n"
"	stq	$13,32($30)	\n"
"	stq	$14,40($30)	\n"
"	stq	$15,48($30)	\n"
"	stq	$26,56($30)	\n"
"	stt	$f0,64($30)	\n"
"	stt	$f1,72($30)	\n"
"	stt	$f2,80($30)	\n"
"	stt	$f3,88($30)	\n"
"	stt	$f4,96($30)	\n"
"	stt	$f5,104($30)	\n"
"	stt	$f6,112($30)	\n"
"	stt	$f7,120($30)	\n"
"	stt	$f8,128($30)	\n"
"	stt	$f9,136($30)	\n"
"	stt	$f10,144($30)	\n"
"	stt	$f11,152($30)	\n"
"	stt	$f12,160($30)	\n"
"	stt	$f13,168($30)	\n"
"	stt	$f14,176($30)	\n"
"	stt	$f15,184($30)	\n"
"	stt	$f16,192($30)	\n"
"	stt	$f17,200($30)	\n"
"	stt	$f18,208($30)	\n"
"	stt	$f19,216($30)	\n"
"	stt	$f20,224($30)	\n"
"	stt	$f21,232($30)	\n"
"	stt	$f22,240($30)	\n"
"	stt	$f23,248($30)	\n"
"	stt	$f24,256($30)	\n"
"	stt	$f25,264($30)	\n"
"	stt	$f26,272($30)	\n"
"	stt	$f27,280($30)	\n"
"	mf_fpcr	$f0		\n"
"	stt	$f28,288($30)	\n"
"	stt	$f29,296($30)	\n"
"	stt	$f30,304($30)	\n"
"	stt	$f0,312($30)	\n"
"	ldt	$f0,64($30)	\n"
"	ret	$31,($1),1	\n"
".end save_switch_stack		\n"
"\n"
".align 3			\n"
".ent	restore_switch_stack	\n"
"restore_switch_stack:		\n"
"	ldq	$9,0($30)	\n"
"	ldq	$10,8($30)	\n"
"	ldq	$11,16($30)	\n"
"	ldq	$12,24($30)	\n"
"	ldq	$13,32($30)	\n"
"	ldq	$14,40($30)	\n"
"	ldq	$15,48($30)	\n"
"	ldq	$26,56($30)	\n"
"	ldt	$f30,312($30)	# get saved fpcr	\n"
"	ldt	$f0,64($30)	\n"
"	ldt	$f1,72($30)	\n"
"	ldt	$f2,80($30)	\n"
"	ldt	$f3,88($30)	\n"
"	mt_fpcr	$f30		# install saved fpcr	\n"
"	ldt	$f4,96($30)	\n"
"	ldt	$f5,104($30)	\n"
"	ldt	$f6,112($30)	\n"
"	ldt	$f7,120($30)	\n"
"	ldt	$f8,128($30)	\n"
"	ldt	$f9,136($30)	\n"
"	ldt	$f10,144($30)	\n"
"	ldt	$f11,152($30)	\n"
"	ldt	$f12,160($30)	\n"
"	ldt	$f13,168($30)	\n"
"	ldt	$f14,176($30)	\n"
"	ldt	$f15,184($30)	\n"
"	ldt	$f16,192($30)	\n"
"	ldt	$f17,200($30)	\n"
"	ldt	$f18,208($30)	\n"
"	ldt	$f19,216($30)	\n"
"	ldt	$f20,224($30)	\n"
"	ldt	$f21,232($30)	\n"
"	ldt	$f22,240($30)	\n"
"	ldt	$f23,248($30)	\n"
"	ldt	$f24,256($30)	\n"
"	ldt	$f25,264($30)	\n"
"	ldt	$f26,272($30)	\n"
"	ldt	$f27,280($30)	\n"
"	ldt	$f28,288($30)	\n"
"	ldt	$f29,296($30)	\n"
"	ldt	$f30,304($30)	\n"
"	lda	$30,"SWITCH_STACK_SIZE"($30)	\n"
"	ret	$31,($1),1	\n"
".end restore_switch_stack	\n"
"				\n"
".align 3			\n"
".globl  sys_vmadump		\n"
".ent    sys_vmadump		\n"
"sys_vmadump:			\n"
"        ldgp    $29,0($27)	\n"
"        bsr     $1,save_switch_stack	\n"
"        lda     $16,"SWITCH_STACK_SIZE"($30)	\n"
"        jsr     $26,do_vmadump	\n"
"        bsr     $1,restore_switch_stack	\n"
"        ret     $31,($26),1	\n"
".end    sys_vmadump		\n"
);


/*
 * Local variables:
 * c-basic-offset: 4
 * End:
 */
