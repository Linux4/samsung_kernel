/*
 * Copyright (c) [2009-2013] Marvell International Ltd. and its affiliates.
 * All rights reserved.
 * This software file (the "File") is owned and distributed by Marvell
 * International Ltd. and/or its affiliates ("Marvell") under the following
 * licensing terms.
 * If you received this File from Marvell, you may opt to use, redistribute
 * and/or modify this File in accordance with the terms and conditions of
 * the General Public License Version 2, June 1991 (the "GPL License"), a
 * copy of which is available along with the File in the license.txt file
 * or by writing to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 or on the worldwide web at
 * http://www.gnu.org/licenses/gpl.txt. THE FILE IS DISTRIBUTED AS-IS,
 * WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
 * DISCLAIMED. The GPL License provides additional details about this
 * warranty disclaimer.
 */

#include "tee_client_api.h"
#include "tee_cm_internal.h"
#include "tee_cm.h"
#include "tee_perf.h"

/* variables */
extern void *in_header;
extern void *in_buf;		/* rb data buffer */
extern uint32_t in_buf_size;
extern void *out_header;
extern void *out_buf;		/* rc data buffer */
extern uint32_t out_buf_size;
extern int32_t _g_idx_in_read;

/* rb mapping info */
static ulong_t _g_phys_rb;
static ulong_t _g_size_rb;
static ulong_t _g_virt_rb;
/* rc mapping info */
static ulong_t _g_phys_rc;
static ulong_t _g_size_rc;
static ulong_t _g_virt_rc;

#ifdef TEE_DEBUG_ENALBE_PROC_FS_LOG
extern void tee_dbg_log_init(ulong_t buffer, ulong_t ctl);
#endif

extern int32_t *_cm_in_write_idx(void);

tee_cm_stat tee_cm_init(void)
{
	tee_cm_stat ret = TEEC_SUCCESS;
#ifdef TEE_PERF_MEASURE
	uint32_t *perf_buffer;
#endif
	/* map RB */
	_g_phys_rb = _read_rb_phys_addr();
	_g_size_rb = _read_rb_size();
	_g_virt_rb =
		(ulong_t)ioremap_wc(_g_phys_rb, (uint32_t)(_g_size_rb));
	printk(KERN_ERR "remap rb to 0x%lx\n", _g_virt_rb);

	/* map RC */
	_g_phys_rc = _read_rc_phys_addr();
	_g_size_rc = _read_rc_size();
	_g_virt_rc =
		(ulong_t)ioremap_wc(_g_phys_rc, (uint32_t)(_g_size_rc));
	printk(KERN_ERR "remap rc to 0x%lx\n", _g_virt_rc);

#ifdef TEE_PERF_MEASURE
	/* map permance log buffer */
	perf_buffer =
	    (uint32_t *) ioremap_wc(PERF_BUF_ADDR, PERF_BUF_SIZE);
	tee_perf_init((uint32_t *) perf_buffer);
	printk(KERN_ERR
		"remap perf log buffer to 0x%x\n",
		(uint32_t) perf_buffer);
#endif

	in_header = (void *)_g_virt_rb;
	in_buf = (void *)((ulong_t) in_header + sizeof(cm_index_header_t));
	in_buf_size = _g_size_rb / 2 - sizeof(cm_index_header_t);

	OSA_ASSERT(_g_virt_rc);
	memset((void *)_g_virt_rc, 0, _g_size_rc);
	out_header = (void *)_g_virt_rc;
	out_buf = (void *)((ulong_t) out_header + sizeof(cm_index_header_t));
	out_buf_size = _g_size_rc / 2 - sizeof(cm_index_header_t);

#ifdef TEE_DEBUG_ENALBE_PROC_FS_LOG
	tee_dbg_log_init((ulong_t) in_buf + in_buf_size,
			 (ulong_t) out_buf + out_buf_size);
#endif

	_g_idx_in_read = *_cm_in_write_idx();
	return ret;
}

void tee_cm_cleanup(void)
{
	/* resetting */
	in_header = NULL;
	in_buf = NULL;
	in_buf_size = 0;
	out_header = NULL;
	out_buf = NULL;
	out_buf_size = 0;

	/* unmap RB */
	if (_g_virt_rb)
		osa_iounmap_cached((void *)_g_virt_rb, _g_size_rb);

	/* free RC */
	if (_g_virt_rc)
		osa_iounmap_cached((void *)_g_virt_rc, _g_size_rc);

	_g_phys_rb = 0;
	_g_virt_rb = 0;
	_g_size_rb = 0;
	_g_phys_rc = 0;
	_g_virt_rc = 0;
	_g_size_rc = 0;

	return;
}
