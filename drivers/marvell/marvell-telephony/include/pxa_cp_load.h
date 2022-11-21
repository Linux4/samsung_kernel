/*
 * PXA CP load header
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2007 Marvell International Ltd.
 * All Rights Reserved
 */

#ifndef _PXA_CP_LOAD_H_
#define _PXA_CP_LOAD_H_

#include "linux_driver_types.h"

extern struct bus_type cpu_subsys;
extern void cp_releasecp(void);
extern void cp_holdcp(void);
extern bool cp_get_status(void);
extern const struct cp_load_table_head *get_cp_load_table(void);

extern unsigned long arbel_bin_phys_addr;

extern int g_simcard;

/**
 * interface exported by kernel for disabling FC during hold/release CP
 */
extern void acquire_fc_mutex(void);
extern void release_fc_mutex(void);

#endif /* _PXA_CP_LOAD_H_ */
