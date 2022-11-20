/*-------------------------------------------------------------------------
 *  vmadump_sparc.c:  sparc specific dumping/undumping routines
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
 * $Id: vmadump_sparc.c,v 1.4 2008/12/02 00:57:12 phargrov Exp $
 *
 * THIS VERSION MODIFIED FOR BLCR <http://ftg.lbl.gov/checkpoint>
 *
 * Experimental SPARC support contributed to BLCR by Vincentius Robby
 * <vincentius@umich.edu> and Andrea Pellegrini <apellegr@umich.edu>.
 *-----------------------------------------------------------------------*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <asm/processor.h>

#define __VMADUMP_INTERNAL__
#include "vmadump.h"

long vmadump_store_cpu(cr_chkpt_proc_req_t *ctx, struct file *file,
		       struct pt_regs *regs) {
    long bytes = 0, r;

    /* Store struct pt_regs */
    r = write_kern(ctx, file, regs, sizeof(*regs));
    if (r != sizeof(*regs)) goto err;
    
    bytes += r;
    return bytes;
// apellegrini@umich.edu
// 2008/08/28
// No FPU support at this point

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

    /* Only allow chages to certain bits of PSR/TSTATE */
#if defined(__arch64__)
    regtmp.tstate &= (TSTATE_ASI | TSTATE_ICC | TSTATE_XCC); 
    regtmp.tstate |= (regs->tstate & ~(TSTATE_ASI | TSTATE_ICC | TSTATE_XCC));
#else
    regtmp.psr &= (PSR_ICC | PSR_EF); 
    regtmp.psr |= (regs->psr & ~(PSR_ICC | PSR_EF));
#endif

    memcpy(regs, &regtmp, sizeof(regtmp));
    return 0;

 bad_read:
    if (r >= 0) r = -EIO;
    return r;
}

