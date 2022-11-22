/*
 * linux/include/video/mmpdisp_export_funcs.h
 * Header file for Marvell MMP Display Controller
 *
 * Copyright (C) 2012 Marvell Technology Group Ltd.
 * Authors: Zhou Zhu <zzhu3@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _MMPDISP_EXPORT_FUNC_H_
#define _MMPDISP_EXPORT_FUNC_H_
#define MMP_DISP_NO_UPSTREAM
#if defined(CONFIG_MMP_DISP)

static inline int disp_is_on(void)
{
	return 1;
}

static inline void disp_onoff(int onoff)
{
}


static inline void disp_eofintr_onoff(int on)
{
}
#elif defined(CONFIG_FB_PXA168)
#include <mach/pxa168fb.h>
extern atomic_t displayon;
static inline int disp_is_on(void)
{
	return atomic_read(&displayon);
}

static inline void disp_onoff(int flag)
{
	panel_dma_ctrl(flag);
}

static inline void disp_eofintr_onoff(int on)
{
	if (!on)
		irq_mask_eof(0);
	else
		irq_unmask_eof(0);
}
#endif
#endif	/* _MMPDISP_EXPORT_FUNC_ */
