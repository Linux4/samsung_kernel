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

enum cp_type {
	cp_type_pxa988,
	cp_type_pxa1L88,
	cp_type_pxa1928,
	cp_type_pxa1908,

	cp_type_cnt
};

struct cp_buffer {
	char *addr;
	int len;
};

struct cp_dev {
	u32 cp_type;
	u32 lpm_qos;
	u32 por_offset;
	void (*release_cp)(void);
	void (*hold_cp)(void);
	bool (*get_status)(void);
};

void cp1928_releasecp(void);
void cp1928_holdcp(void);
bool cp1928_get_status(void);
void cp988_releasecp(void);
void cp988_holdcp(void);
bool cp988_get_status(void);
void __cp988_releasecp(void);
void cp1908_releasecp(void);
void cp1908_holdcp(void);
bool cp1908_get_status(void);

extern struct bus_type cpu_subsys;
extern void cp_releasecp(void);
extern void cp_holdcp(void);
extern bool cp_get_status(void);

extern uint32_t arbel_bin_phys_addr;
extern uint32_t seagull_remap_smc_funcid;

extern void (*watchdog_count_stop_fp)(void);

/**
 * interface exported by kernel for disabling FC during hold/release CP
 */
extern void acquire_fc_mutex(void);
extern void release_fc_mutex(void);

int cp_invoke_smc(u64 function_id, u64 arg0, u64 arg1,
	u64 arg2);

static inline int cp_set_seagull_remap_reg(u64 val)
{
	int ret;

	ret = cp_invoke_smc(seagull_remap_smc_funcid, val, 0, 0);

	pr_info("%s: function_id: 0x%llx, arg0: 0x%llx, ret 0x%x\n",
		__func__, (u64)seagull_remap_smc_funcid, val, ret);

	return ret;
}

#endif /* _PXA_CP_LOAD_H_ */
