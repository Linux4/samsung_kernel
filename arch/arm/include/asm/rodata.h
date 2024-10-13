/*
 *  arch/arm/include/asm/rodata.h
 *
 *  Copyright (C) 2011 Google, Inc.
 *
 *  Author: Colin Cross <ccross@android.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _ASMARM_RODATA_H
#define _ASMARM_RODATA_H

#ifndef __ASSEMBLY__

#if defined(CONFIG_DEBUG_RODATA) || defined(CONFIG_USE_DIRECT_IS_CONTROL) || defined(CONFIG_USE_HOST_FD_LIBRARY)
int set_memory_rw(unsigned long virt, int numpages);
int set_memory_ro(unsigned long virt, int numpages);
int set_memory_xn(unsigned long virt, int numpages);
#if defined(CONFIG_USE_DIRECT_IS_CONTROL) || defined(CONFIG_USE_HOST_FD_LIBRARY)
int set_memory_x(unsigned long virt, int numpages);
#endif

#ifdef CONFIG_DEBUG_RODATA
void mark_rodata_ro(void);
void set_kernel_text_rw(void);
void set_kernel_text_ro(void);
#endif
#else
static inline void set_kernel_text_rw(void) { }
static inline void set_kernel_text_ro(void) { }
static inline int set_memory_xn(unsigned long virt, int numpages) {return 0; }
#endif

#endif

#endif
