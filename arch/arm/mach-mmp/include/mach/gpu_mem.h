/*
*  arch/arm/plat-pxa/include/plat/gpu_mem.h
*
*  GPU reserved memory management
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License version 2 as
*  published by the Free Software Foundation.
*  (C) Copyright 2011 Marvell International Ltd.
*  All Rights Reserved
*/

#ifndef _PXA_GPU_MEM_H_
#define _PXA_GPU_MEM_H_

extern void __init pxa_reserve_gpu_memblock(void);
extern void __init pxa_add_gpu(void);

#endif
