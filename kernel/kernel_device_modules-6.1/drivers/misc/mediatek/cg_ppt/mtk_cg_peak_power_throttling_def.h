/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Clouds Lee <clouds.lee@mediatek.com>
 */

#ifndef _MTK_CG_PEAK_POWER_THROTTLING_DEF_H_
#define _MTK_CG_PEAK_POWER_THROTTLING_DEF_H_

/*
 * ========================================================
 * Definitions
 * ========================================================
 */
#define PPT_SRAM_INIT_VALUE (0xD903D903) //55555(short) & 55555(short)

/*
 * ========================================================
 * Thermal SRAM (temparary)
 * ========================================================
 */
#define THERMAL_CSRAM_BASE (0x00114000)
#define THERMAL_CSRAM_SIZE (0x400)
#define THERMAL_CSRAM_CTRL_BASE (THERMAL_CSRAM_BASE + 0x360)

#define DLPT_CSRAM_BASE (0x00116400)
#define DLPT_CSRAM_SIZE (0x1400) //5KB
#define DLPT_CSRAM_CTRL_RESERVED_SIZE                                          \
	(128) //reserve last 128B for control purpose
#define DLPT_CSRAM_CTRL_BASE                                                   \
	(DLPT_CSRAM_BASE + DLPT_CSRAM_SIZE - DLPT_CSRAM_CTRL_RESERVED_SIZE)

#define DLPT_WIFI_DRAM_BASE (0x9D5A07F0)


/*
 * ========================================================
 * CGPPT Model Option
 * ========================================================
 */
#define MO_FAVOR_CPU                 (1 << 0)
#define MO_FAVOR_GPU                 (1 << 1)
#define MO_FAVOR_MULTISCENE          (1 << 2)
#define MO_CPU_AVS                   (1 << 3)
#define MO_GPU_AVS                   (1 << 4)
#define MO_GPU_CURR_FREQ_POWER_CALC  (1 << 5)

#define CGPPT_CHECKBIT(value, bit_mask) (((value) & (bit_mask)) == (bit_mask))
#define CGPPT_SETBIT(value, bit_mask) ((value) |= (bit_mask))
#define CGPPT_CLEARBIT(value, bit_mask) ((value) &= ~(bit_mask))


/*
 * ========================================================
 * GAC Boost Option
 * ========================================================
 */
#define GACBOOST_CPU_BOOST                         (1<<0)
#define GACBOOST_CCI_FREQ                          (1<<1)
#define GACBOOST_CM_MGR_MAP_DRAM_ENABLE            (1<<2)
#define GACBOOST_GPU_GED_FALLBACK_FREQUENCY_ADJUST (1<<3)
#define CPU_BOOST_FOREGROUND                       (1<<4)






/*
 * ========================================================
 * [Kernel]
 * ========================================================
 */
#if defined(__KERNEL__)
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/string.h>
#include "mtk_cg_peak_power_throttling_plat_kl.h"

extern uintptr_t THERMAL_CSRAM_BASE_REMAP;
extern uintptr_t THERMAL_CSRAM_CTRL_BASE_REMAP;
extern uintptr_t DLPT_CSRAM_BASE_REMAP;
extern uintptr_t DLPT_CSRAM_CTRL_BASE_REMAP;

extern void cg_ppt_thermal_sram_remap(uintptr_t virtual_addr);
extern void cg_ppt_dlpt_sram_remap(uintptr_t virtual_addr);

#define pp_print(fmt, ...) pr_info(fmt, ##__VA_ARGS__)

/*
 * ========================================================
 * [SW Runner]
 * ========================================================
 */
#elif defined(CFG_CPU_PEAKPOWERTHROTTLING) || \
defined(CFG_GPU_PEAKPOWERTHROTTLING) || \
defined(CFG_CPU_SIGNALMONITOR)       || \
defined(CFG_GPU_SIGNALMONITOR)

#include "mtk_cg_peak_power_throttling_plat_eb.h"
#include "mt_printf.h"

#define THERMAL_CSRAM_BASE_REMAP (THERMAL_CSRAM_BASE | 0x10000000)
#define THERMAL_CSRAM_CTRL_BASE_REMAP (THERMAL_CSRAM_CTRL_BASE | 0x10000000)
#define DLPT_CSRAM_BASE_REMAP (DLPT_CSRAM_BASE | 0x10000000)
#define DLPT_CSRAM_CTRL_BASE_REMAP (DLPT_CSRAM_CTRL_BASE | 0x10000000)


#define pp_print(fmt, ...) PRINTF_I(fmt, ##__VA_ARGS__)

extern void ppt_critical_enter(void);
extern void ppt_critical_exit(void);

/*
 * ========================================================
 * [UNKNOWN]
 * ========================================================
 */
#else
#pragma message("pp def compiled as UNKNOWN")

#endif

/*
 * ...................................
 * Thermal SRAM variables
 * ...................................
 */

#define G2C_B_PP_LMT_FREQ (THERMAL_CSRAM_BASE_REMAP + 0x360)
#define G2C_B_PP_LMT_FREQ_ACK (THERMAL_CSRAM_BASE_REMAP + 0x364)
#define G2C_M_PP_LMT_FREQ (THERMAL_CSRAM_BASE_REMAP + 0x368)
#define G2C_M_PP_LMT_FREQ_ACK (THERMAL_CSRAM_BASE_REMAP + 0x36C)
#define G2C_L_PP_LMT_FREQ (THERMAL_CSRAM_BASE_REMAP + 0x370)
#define G2C_L_PP_LMT_FREQ_ACK (THERMAL_CSRAM_BASE_REMAP + 0x374)
#define CPU_LOW_KEY (THERMAL_CSRAM_BASE_REMAP + 0x378)
#define G2C_PP_LMT_FREQ_ACK_TIMEOUT (THERMAL_CSRAM_BASE_REMAP + 0x37C)

struct ThermalCsramCtrlBlock {
	int g2c_b_pp_lmt_freq;
	int g2c_b_pp_lmt_freq_ack;
	int g2c_m_pp_lmt_freq;
	int g2c_m_pp_lmt_freq_ack;
	int g2c_l_pp_lmt_freq;
	int g2c_l_pp_lmt_freq_ack;
	int cpu_low_key;
	int g2c_pp_lmt_freq_ack_timeout;
};

/*
 * ...................................
 * DLPT DRAM Control Block
 * ...................................
 */
struct DlptDramMdCtrlBlock {
	int modem_peak_power_mw;
	int ap2md_ack;
};

struct DlptDramWifiCtrlBlock {
	int wifi_peak_power_mw;
	int ap2wifi_ack;
};


/*
 * ...................................
 * DLPT SRAM Control Block
 * ...................................
 */
struct DlptCsramCtrlBlock {
	/* mode 0:CG no peak at same time 1:peak power budget 2:OFF*/
	int peak_power_budget_mode; /* 1*/
	int cg_min_power_mw; /* 2*/
	int vsys_power_budget_mw; /* 3*/ /*include modem, wifi*/
	int vsys_power_budget_ack; /* 4*/
	int flash_peak_power_mw; /* 5*/
	int audio_peak_power_mw; /* 6*/
	int camera_peak_power_mw; /* 7*/
	int apu_peak_power_mw; /* 8*/
	int display_lcd_peak_power_mw; /* 9*/
	int dram_peak_power_mw; /*10*/
	int modem_peak_power_mw_shadow; /*11*/
	int wifi_peak_power_mw_shadow; /*12*/
	int reserved_4; /*13*/
	int reserved_5; /*14*/
	int apu_peak_power_ack; /*15*/
	int boot_mode; /*16*/
	int md_smem_addr; /*17*/
	int wifi_smem_addr; /*18*/
	int cg_power_threshold; /*19*/
	int cg_power_threshold_count; /*20*/
};


/*
 * ========================================================
 * Functions
 * ========================================================
 */
struct ThermalCsramCtrlBlock *thermal_csram_ctrl_block_get(void);
void thermal_csram_ctrl_block_release(
	struct ThermalCsramCtrlBlock *remap_status_base);
struct DlptCsramCtrlBlock *dlpt_csram_ctrl_block_get(void);
void dlpt_csram_ctrl_block_release(
	struct DlptCsramCtrlBlock *remap_status_base);

#endif /*_MTK_CG_PEAK_POWER_THROTTLING_DEF_H_*/
