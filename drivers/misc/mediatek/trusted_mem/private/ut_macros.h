/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef TMEM_UT_MACROS_H
#define TMEM_UT_MACROS_H

#include "private/tmem_device.h"

#define secmem_minmal_chunk_size() tmem_core_get_min_chunk_size(TRUSTED_MEM_SVP)
#define secmem_session_open() tmem_core_session_open(TRUSTED_MEM_SVP)
#define secmem_session_close() tmem_core_session_close(TRUSTED_MEM_SVP)
#define secmem_ssmr_allocate() tmem_core_ssmr_allocate(TRUSTED_MEM_SVP)
#define secmem_ssmr_release() tmem_core_ssmr_release(TRUSTED_MEM_SVP)
#define secmem_alloc_chunk(...)                                                \
	tmem_core_alloc_chunk(TRUSTED_MEM_SVP, ##__VA_ARGS__)
#define secmem_unref_chunk(...)                                                \
	tmem_core_unref_chunk(TRUSTED_MEM_SVP, ##__VA_ARGS__)
#define secmem_is_regmgr_region_on()                                           \
	tmem_core_is_regmgr_region_on(TRUSTED_MEM_SVP)
#define secmem_get_regmgr_region_online_cnt()                                  \
	tmem_core_get_regmgr_region_online_cnt(TRUSTED_MEM_SVP)
#define secmem_get_regmgr_region_ref_cnt()                                     \
	tmem_core_get_regmgr_region_ref_cnt(TRUSTED_MEM_SVP)
#define secmem_regmgr_online() tmem_core_regmgr_online(TRUSTED_MEM_SVP)
#define secmem_regmgr_offline() tmem_core_regmgr_offline(TRUSTED_MEM_SVP)
#define is_secmem_registered() tmem_core_is_device_registered(TRUSTED_MEM_SVP)

#endif /* end of TMEM_UT_MACROS_H */
