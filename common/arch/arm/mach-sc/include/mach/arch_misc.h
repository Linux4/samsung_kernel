/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 * Copyright (C) steve.zhan
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define SCX35_ALPHA_TAPOUT	(0x8300aaaa)
#define SCX35_ALPHA_TAPOUT_MASK	(0xFFFFFFFF)

#define SCX35_BETA_TAPOUT	(0x8300a001)
#define SCX35_BETA_TAPOUT_MASK	(0xFFFFFFFF)

#define SCX35G_ALPHA_TAPOUT            (0x8730b000)
#define SCX35G_ALPHA_TAPOUT_MASK       (0xFFFFF000)

/**
 * sci_get_chip_id - read chip id
 *
 *
 * Return read the chip id
 */
u32 sci_get_chip_id(void);

/**
 * sci_get_ana_chip_id - read a-die chip id
 *
 * Return the a-die chip id, example: 0X2711A000
 */
u32 sci_get_ana_chip_id(void);

#define MASK_ANA_VER	( 0xFFF )
enum sci_ana_chip_ver {
	SC2711AA = 0x000,
	SC2711AB = 0x000,	/* discard */
	SC2711AC = 0x002,
	SC2711BA = 0x100,
	SC2713CA = 0xA00,
	//TODO: Add more ana chip version here
};

/**
 * sci_get_ana_chip_ver - read a-die chip version
 *
 * Return the a-die chip version, example: 0X100
 */
int sci_get_ana_chip_ver(void);

/**
 * sci_ana_chip_vers_is_compatible
 *
 * Check if the given "chip_ver" string matches the a-die chip on board
 */
static inline int sci_ana_chip_ver_is_compatible(enum sci_ana_chip_ver compat)
{
	return sci_get_ana_chip_ver() == (u32)compat;
}

/**
 *
 * sc_init_chip_id(void) - read chip id from hardware and save it.
 *
 */
void __init sc_init_chip_id(void);



#define IS_CPU(name, id, mask)		\
static inline int soc_id_is_##name(void)	\
{						\
	return ((sci_get_chip_id() & mask) == (id & mask));	\
}

IS_CPU(sc8735v0, SCX35_ALPHA_TAPOUT, SCX35_ALPHA_TAPOUT_MASK)
IS_CPU(sc8735v1, SCX35_BETA_TAPOUT, SCX35_BETA_TAPOUT_MASK)
IS_CPU(sc8735gv0, SCX35G_ALPHA_TAPOUT, SCX35G_ALPHA_TAPOUT_MASK)

/*Driver can use this MACRO to distinguish different chip code */
#define soc_is_scx35_v0()	soc_id_is_sc8735v0()
#define soc_is_scx35_v1()	soc_id_is_sc8735v1()
#define soc_is_scx35g_v0()     soc_id_is_sc8735gv0()

/**
* read value from virtual address. Pls make sure val is not NULL.
* return 0 is successful
*/
int sci_read_va(u32 vreg, u32 *val);

/**
* write value to virtual address. if clear_msk is ~0, or_val will fully set to vreg.
* return 0 is successful
*/
int sci_write_va(u32 vreg, const u32 or_val, const u32 clear_msk);

/**
* read value from pysical address. Pls make sure val is not NULL.
* return 0 is successful
*/
int sci_read_pa(u32 preg, u32 *val);

/**
* write value to pysical address. if clear_msk is ~0, or_val will fully set to paddr.
* return 0 is successful
*/
int sci_write_pa(u32 paddr, const u32 or_val, const u32 clear_msk);

#define SPRD_CHIPID_7715 (0x77150000)
#define SPRD_CHIPID_8815 (0X88150000)

static inline int soc_is_sc7715(void)
{
	unsigned int flag = 0;
	flag = sci_get_chip_id() & 0xffff0000;
	if ((SPRD_CHIPID_7715 == flag)
			||(SPRD_CHIPID_8815 == flag))
	{
		return 1;
	}
	else
	{
		pr_err("%s error chip id\n", __func__);
		return 0;
	}
}
