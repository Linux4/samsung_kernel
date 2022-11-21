/*
 * linux/regulator/s2abb01-regulator.h
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_S2ABB01_REGULATOR_H
#define __LINUX_S2ABB01_REGULATOR_H

/*******************************************************************************
 * Useful Macros
 ******************************************************************************/
#undef  __CONST_FFS
#define __CONST_FFS(_x) \
	((_x) & 0x0F ? ((_x) & 0x03 ? ((_x) & 0x01 ? 0 : 1) :\
					((_x) & 0x04 ? 2 : 3)) :\
			((_x) & 0x30 ? ((_x) & 0x10 ? 4 : 5) :\
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


/*******************************************************************************
 * Register
 ******************************************************************************/
/* Slave Address */
#define S2ABB01_I2C_ADDR	(0xAA >> 1)

/* Register */
#define REG_PMIC_ID		0x00

#define REG_BB_CTRL		0x01
#define BIT_DSCH_BB		BIT(4)
#define BIT_MODE_BB		BITS(3, 2)
#define BIT_BB			BIT(1)
#define BIT_MODE		BIT(0)

#define REG_BB_OUT		0x02
#define BIT_BB_EN		BIT(7)
#define BIT_VBB_CTRL		BITS(6, 0)

#define REG_LDO_CTRL		0x03
#define BIT_LDO_EN		BIT(7)
#define BIT_OUT_L		BITS(5, 0)

#define REG_LDO_DSCH		0x04
#define BIT_DSCH_LDO		BIT(1)
#define BIT_OVCB		BIT(0)

/* voltage range and step */
#define LDO_MINUV		1500000		/* 1.500V */
#define LDO_MAXUV		3000000		/* 3.000V */
#define LDO_STEP		50000		/* 50mV */

#define BUCK_MINUV		2600000		/* 2.6000V */
#define BUCK_MAXUV		3600000		/* 3.6000V */
#define BUCK_STEP		12500		/* 12.5mV */


#define BUCK_ALWAYS_3_2V	0x40
#define BUCK_ALWAYS_3_4V	0x50
#define BUCK_ALWAYS_3_5V	0x58

/* S2ABB01 regulator ids */
enum s2abb01_regulator_id {
	S2ABB01_LDO = 0,
	S2ABB01_BUCK,
	S2ABB01_REGULATORS = S2ABB01_BUCK,
};

enum s2abbo1_bb_mode {
	BB_MODE_FPFM = 0,
	BB_MODE_FPWM,
	BB_MODE_AUTO,
	/*	BB_MODE_FPWM,	 */
};

struct s2abb01_regulator_data {
	struct regulator_init_data *initdata;
	struct device_node *of_node;

	int adchg;	/* active discharge */
	int alwayson;
};

struct s2abb01_regulator_pdata {
	int num_regulators;
	struct s2abb01_regulator_data *regulators;
};

#endif
