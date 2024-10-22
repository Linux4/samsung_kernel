/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef _FLT_INIT_H
#define _FLT_INIT_H

/* FLT mode */
enum _flt_mode {
	FLT_MODE_0 = 0,
	FLT_MODE_1 = 1,
	FLT_MODE_2 = 2,
	FLT_MODE_NUM,
};

void __iomem *get_flt_xrg(void);
unsigned long long get_flt_xrg_size(void);
void  flt_set_mode(u32 mode);
u32 flt_get_mode(void);
int flt_init_res(void);
#endif /* _FLT_INIT_H*/
