/*
 * Copyright 2021 Advanced Micro Devices, Inc.
 */

#ifndef MOBILE2_ASIC_INIT_H
#define MOBILE2_ASIC_INIT_H

struct __reg_setting_m2 {
	unsigned int reg;
	unsigned int val;
};

/* From ./out/linux_3.10.0_64.VCS/mobile2/common/tmp/proj/verif_release_ro/
 * register_pkg/63.11/src/generators/gen_register_info/flows/golden_register_value.h */
struct __reg_setting_m2 emu_mobile2_setting_cl135710[] = {
        { 0x000098F8, 0X00000142}, /* GB_ADDR_CONFIG */
};

#endif
