/*
 * linux/regulator/max77826-regulator.h
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_MAX77826_REGULATOR_H
#define __LINUX_MAX77826_REGULATOR_H

/*******************************************************************************
 * Useful Macros
 ******************************************************************************/

#undef  __CONST_FFS
#define __CONST_FFS(_x) \
	((_x) & 0x0F ? ((_x) & 0x03 ? ((_x) & 0x01 ? 0 : 1) :	\
			((_x) & 0x04 ? 2 : 3)) :		\
			((_x) & 0x30 ? ((_x) & 0x10 ? 4 : 5) :	\
			((_x) & 0x40 ? 6 : 7)))

#undef  BIT_RSVD
#define BIT_RSVD  0

#undef  BITS
#define BITS(_end, _start) \
	((BIT(_end) - BIT(_start)) + BIT(_end))

#undef  __BITS_GET
#define __BITS_GET(_word, _mask, _shift) \
	(((_word) & (_mask)) >> (_shift))

#undef  BITS_GET
#define BITS_GET(_word, _bit) \
	__BITS_GET(_word, _bit, FFS(_bit))

#undef  __BITS_SET
#define __BITS_SET(_word, _mask, _shift, _val) \
	(((_word) & ~(_mask)) | (((_val) << (_shift)) & (_mask)))

#undef  BITS_SET
#define BITS_SET(_word, _bit, _val) \
	__BITS_SET(_word, _bit, FFS(_bit), _val)

#undef  BITS_MATCH
#define BITS_MATCH(_word, _bit) \
	(((_word) & (_bit)) == (_bit))

enum max77826_reg_id {
	MAX77826_LDO1 = 1,
	MAX77826_LDO2,
	MAX77826_LDO3,
	MAX77826_LDO4,
	MAX77826_LDO5,
	MAX77826_LDO6,
	MAX77826_LDO7,
	MAX77826_LDO8,
	MAX77826_LDO9,
	MAX77826_LDO10,
	MAX77826_LDO11,
	MAX77826_LDO12,
	MAX77826_LDO13,
	MAX77826_LDO14,
	MAX77826_LDO15,
	MAX77826_LDO_MAX = MAX77826_LDO15,

	MAX77826_BUCK,

	MAX77826_BB,

	MAX77826_REGULATORS = MAX77826_BB,
};

struct max77826_regulator_data {
	int active_discharge_enable;

	struct regulator_init_data *initdata;
	struct device_node *of_node;
};

struct max77826_regulator_platform_data {
	int num_regulators;
	struct max77826_regulator_data *regulators;

	int buck_ramp_rate;
	int buck_fpwm;
	int buck_fsrade;
	int buck_ss;
	int buck_freq;

	int bb_ovp_th;
	int bb_hskip;
	int bb_fpwm;

	int uvlo_fall_threshold;
};

#endif
