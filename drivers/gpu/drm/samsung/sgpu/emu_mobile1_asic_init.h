/*
 * Copyright 2019-2021 Advanced Micro Devices, Inc.
 */

#ifndef MOBILE1_ASIC_INIT_H
#define MOBILE1_ASIC_INIT_H

struct __reg_setting_m1 {
	unsigned int reg;
	unsigned int val;
};

/* From the mobile1 Gopher build, cl72071 */
struct __reg_setting_m1 emu_mobile1_setting_cl72071[] = {
	{0x000098F8, 0x00000142},
	{0x00033440, 0x0000008D},
	{0x00033640, 0x0000008D},
	{0x00009508, 0x010B0000},
	{0x00009510, 0x00000000},
	{0x0003380C, 0xFFFFFFF3},
	{0x00033884, 0xFFFFFFF3},
	{0x00033894, 0x32103210},
	{0x00033898, 0x32103210},
	{0x00009F80, 0x00000500},
};

/* From $out/linux_3.10.0_64.VCS/mobile1/common/pub/include/reg/golden_register_value.h */
struct __reg_setting_m1 emu_mobile1_setting_cl87835[] = {
	{0x000098F8, 0x00000142},/* GB_ADDR_CONFIG */
	{0x00033440, 0x0000008D},/* GL1_PIPE_STEER */
	{0x00033640, 0x0000008D},/* CH_PIPE_STEER */
	{0x00009508, 0x010B0000},/* TA_CNTL_AUX */
	{0x00009510, 0x00000000},/* TA_CNTL2 */
	{0x0003380C, 0xFFFFFFF3},/* GL2C_ADDR_MATCH_MASK */
	{0x00033884, 0xFFFFFFF3},/* GL2A_ADDR_MATCH_MASK */
	{0x00033894, 0x32103210},/* GL2_PIPE_STEER_0 */
	{0x00033898, 0x32103210},/* GL2_PIPE_STEER_1 */
	{0x00009834, 0x00000421},/* DB_DEBUG2 */
	{0x00009F80, 0x00000500},/* GCR_GENERAL_CNTL */
};

/* From $out/linux_3.10.0_64.VCS/mobile1/common/pub/include/reg/golden_register_value.h */
struct __reg_setting_m1 emu_mobile1_setting_cl112609[] = {
	{ 0x00009508, 0X010B0000}, /* TA_CNTL_AUX */
	{ 0x00009510, 0X00000000}, /* TA_CNTL2 */
	{ 0x000098F8, 0X00000142}, /* GB_ADDR_CONFIG */
	{ 0x00009834, 0X00000421}, /* DB_DEBUG2 */
	{ 0x00009F80, 0X00000500}, /* GCR_GENERAL_CNTL */
	{ 0x00033440, 0X0000008D}, /* GL1_PIPE_STEER */
	{ 0x00033640, 0X0000008D}, /* CH_PIPE_STEER */
	{ 0x0003380C, 0XFFFFFFF3}, /* GL2C_ADDR_MATCH_MASK */
	{ 0x00033884, 0XFFFFFFF3}, /* GL2A_ADDR_MATCH_MASK */
	{ 0x00033894, 0X32103210}, /* GL2_PIPE_STEER_0 */
	{ 0x00033898, 0X32103210}, /* GL2_PIPE_STEER_1 */
};

/* From $out/linux_3.10.0_64.VCS/mobile1/common/pub/include/reg/golden_register_value.h */
struct __reg_setting_m1 emu_mobile1_setting_cl128286[] = {
	{ 0x000098F8, 0X00000142}, /* GB_ADDR_CONFIG */
};

/* From Viking_Integration_Guide.pdf, 6.4.3 GFX initializations
 * and 6.4.6 Initialize and prepare CP for operations
 */
struct __reg_setting_m1 emu_mobile1_setting_revision_3[] = {
	/* 6.4.3*/
	{ 0x00031104, 0x000C0108}, /* SPI_CONFIG_CNTL_1 */
	{ 0x00030934, 0x00000001}, /* VGT_NUM_INSTANCES */
	{ 0x00028350, 0x00000000}, /* PA_SC_RASTER_CONFIG */
	{ 0x00037008, 0x00000000}, /* CB_PERFCOUNTER0_SELECT1 */
	{ 0x00008A14, 0x00000007}, /* PA_CL_ENHANCE */

	/* 6.4.6*/
	{ 0x0000317D, 0x00020000}, /* CP_DEBUG_2 */
};
#endif
