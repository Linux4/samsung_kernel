/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
 *
 * modules/DISPC/dsi_1.21a/mipi_api_dsihost/mipi_dsih_dphy_megacores.c
 *
 * Author: Haibing.Yang <haibing.yang@spreadtrum.com>
 * 						<yanghb41@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <asm/string.h>
#include "mipi_dsih_dphy.h"

#define L						0
#define H						1
#define CLK						0
#define DATA					1

#ifndef INFINITY
#define INFINITY				0xffffffff
#endif

#define MIN_OUTPUT_FREQ			(100)
#define MAX_LANES				2

#ifndef ROUND_UP
#define ROUND_UP(a, b) (((a) + (b) - 1) / (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) > (b) ? (b) : (a))
#endif

#ifndef abs
/*
 * abs() should not be used for 64-bit types
 * (s64, u64, long long) - use abs64() for those.
 */
#define abs(x) ({ \
	long ret; \
	if (sizeof((x)) == sizeof(long)) { \
		long __x = (x); \
		ret = (__x < 0) ? - __x : __x; \
	} else { \
		int __x = (x); \
		ret = (__x < 0) ? - __x : __x; \
	} \
	ret; \
})
#endif

#define AVERAGE(a, b) (MIN(a, b) + abs((b) - (a)) / 2)

typedef unsigned int 		u32;
typedef unsigned short 	u16;
typedef unsigned char 	u8;

enum TIMING {
	NONE,
	REQUEST_TIME,
	PREPARE_TIME,
	SETTLE_TIME,
	ZERO_TIME,
	TRAIL_TIME,
	EXIT_TIME,
	CLKPOST_TIME,
	TA_GET,
	TA_GO,
	TA_SURE,
	TA_WAIT,
};

struct pll_regs {
	union __reg_03__ {
		struct __03 {
			u8 prbs_bist: 1;
			u8 en_lp_treot: 1;
			u8 lpf_sel: 4;
			u8 txfifo_bypass: 1;
			u8 freq_hopping: 1;
		} bits;
		u8 val;
	} _03;
	union __reg_04__ {
		struct __04 {
			u8 div: 3;
			u8 reserved: 2;
			u8 cp_s: 2;
			u8 fdk_s: 1;
		} bits;
		u8 val;
	} _04;
	union __reg_06__ {
		struct __06 {
			u8 nint: 7;
			u8 mod_en: 1;
		} bits;
		u8 val;
	} _06;
	union __reg_07__ {
		struct __07 {
			u8 kdelta_h: 8;
		} bits;
		u8 val;
	} _07;
	union __reg_08__ {
		struct __08 {
			u8 vco_band: 1;
			u8 sdm_en: 1;
			u8 refin: 2;
			u8 kdelta_l: 4;
		} bits;
		u8 val;
	} _08;
	union __reg_09__ {
		struct __09 {
			u8 kint_h: 8;
		} bits;
		u8 val;
	} _09;
	union __reg_0a__ {
		struct __0a {
			u8 kint_m: 8;
		} bits;
		u8 val;
	} _0a;
	union __reg_0b__ {
		struct __0b {
			u8 out_sel: 4;
			u8 kint_l: 4;
		} bits;
		u8 val;
	} _0b;
	union __reg_0c__ {
		struct __0c {
			u8 kstep_h: 8;
		} bits;
		u8 val;
	} _0c;
	union __reg_0d__ {
		struct __0d {
			u8 kstep_m: 8;
		} bits;
		u8 val;
	} _0d;
	union __reg_0e__ {
		struct __0e {
			u8 pll_pu_byp: 1;
			u8 pll_pu: 1;
			u8 reserved1: 3;
			u8 kstep_l: 3;
		} bits;
		u8 val;
	} _0e;
};

struct dphy_pll {
	u8 refin; /* Pre-divider control signal */
	u8 cp_s; /* 00: SDM_EN=1, 10: SDM_EN=0 */
	u8 fdk_s; /* PLL mode control: integer or fraction */
	u8 hop_en;
	u8 mod_en; /* ssc enable */
	u8 sdm_en;
	u8 div;
	u8 int_n; /* integer N PLL */
	u32 ref_clk; /* dphy reference clock, unit: MHz */
	u32 freq; /* panel config, unit: KHz */
	u32 fvco; /* MHz */
	u32 potential_fvco; /* MHz */
	u32 nint; /* sigma delta modulator NINT control */
	u32 kint; /* sigma delta modulator KINT control */
	u32 sign;
	u32 kdelta;
	u32 kstep;
	u8 lpf_sel; /* low pass filter control */
	u8 out_sel; /* post divider control */
	u8 vco_band; /* vco range */
};

/**
 * Initialise D-PHY module and power up
 * @param phy: pointer to structure
 *  which holds information about the d-phy module
 * @return error code
 */
dsih_error_t mipi_dsih_dphy_open(dphy_t *phy)
{
	if (!phy || !phy->core_read_function || !phy->core_write_function)
		return ERR_DSI_PHY_INVALID;
	else if (phy->status == INITIALIZED)
		return ERR_DSI_PHY_INVALID;

	phy->status = INITIALIZED;
	return OK;
}

u8 mipi_dsih_dphy_test_data_read(dphy_t *phy, u8 address)
{
	mipi_dsih_dphy_test_clock(phy, 1);
	/* set the desired test code in the input 8-bit bus TESTDIN[7:0] */
	mipi_dsih_dphy_test_data_in(phy, address);
	/* set TESTEN input high  */
	mipi_dsih_dphy_test_en(phy, 1);

	/*
	 * drive the TESTCLK input low;
	 * the falling edge captures the
	 * chosen test code into the transceiver.
	 */
	mipi_dsih_dphy_test_clock(phy, 0);

	/* set TESTEN input low to disable further test mode code latching */
	mipi_dsih_dphy_test_en(phy, 0);

	udelay(1);

	return mipi_dsih_dphy_test_data_out(phy);
}

static inline int mipi_dsih_if_dphy_keep_work(dphy_t *phy)
{
	int keep_work;

#ifdef CONFIG_FB_DYNAMIC_FREQ_SCALING
	keep_work = phy->phy_keep_work;
#else
	keep_work = false;
#endif
	return keep_work;
}

static int mipi_dsih_dphy_calc_pll_param(dphy_t *phy, struct dphy_pll *pll)
{
	int i;
	const u32 hopping_period = 200;
	const u32 khz = 1000;
	const u32 mhz = 1000000;
	int delta_freq;
	static u32 origin_freq = 0;
	const unsigned long long factor = 100;
	unsigned long long tmp;

	if (!pll || !pll->freq)
		goto FAIL;

	pll->potential_fvco = pll->freq / khz; /* MHz */
	pll->ref_clk = phy->reference_freq / khz; /* MHz */

	for (i = 0; i < 4; ++i) {
		if (pll->potential_fvco >= 750 && pll->potential_fvco <= 1500) {
			pll->fvco = pll->potential_fvco;
			pll->out_sel = BIT(i);
			break;
		}
		pll->potential_fvco <<= 1;
	}
	if (pll->fvco == 0)
		goto FAIL;

	if (pll->fvco >= 750 && pll->fvco <= 1100) {
		/* vco band control */
		pll->vco_band = 0x0;
		/* low pass filter control */
		pll->lpf_sel = 0x7;
	} else if (pll->fvco > 1100 && pll->fvco <= 1500) {
		pll->vco_band = 0x1;
		pll->lpf_sel = 0x6;
	} else
		goto FAIL;

	pll->nint = pll->fvco / pll->ref_clk;
	tmp = pll->fvco * factor * mhz;
	do_div(tmp, pll->ref_clk);
	tmp = tmp - pll->nint * factor * mhz;
	tmp *= BIT(20);
	do_div(tmp, 100000000);
	pll->kint = (u32)tmp;
	pll->refin = 0x3; /* pre-divider bypass */
	pll->sdm_en = true; /* use fraction N PLL */
	pll->fdk_s = 0x1; /* fraction */
	pll->cp_s = 0x0;
	pll->mod_en = false;

	if (0)
		pll->hop_en = true;
	else
		pll->hop_en = false;

	if (pll->hop_en) {
		if (origin_freq == 0)
			goto FAIL;

		delta_freq = (int)pll->freq - (int)origin_freq;
		if (delta_freq < 0) {
			delta_freq = - delta_freq;
			pll->sign = true;
		}
		pll->kstep = pll->ref_clk * hopping_period;
		pll->kdelta = (1 << 20) * delta_freq * pll->out_sel
				/ pll->kstep / pll->ref_clk;
	} else
		origin_freq = pll->freq;

	return 0;

FAIL:
	if (pll)
		pll->fvco = 0;

	phy->log_error("sprdfb: failed to calculate dphy pll parameters\n");
	return ERR_DSI_PHY_PLL_NOT_LOCKED;
}

static int mipi_dsih_dphy_set_pll_reg(dphy_t *phy, struct dphy_pll *pll)
{
	int i;
	u8 *val;
	struct pll_regs regs;
	u8 regs_off_addr[] = {
			0x03, 0x04, 0x06, 0x07, 0x08, 0x09,
			0x0a, 0x0b, 0x0c, 0x0d, 0x0e
			};
	u8 regs_on_addr[] = {
			0x03, 0x04, 0x07, 0x08, 0x09, 0x0a,
			0x0b, 0x0c, 0x0d, 0x0e, 0x06
			};
	int reg_30_val[] = {
			0x1F, 0x82, 0x99, 0x9E, 0x76, 0x27,
			0x62, 0x00, 0x18, 0xA0, 0xA6
			};

	if (!pll || !pll->fvco)
		goto FAIL;

#ifndef CONFIG_FB_HOP_SPECTRUM_FREQ_SCALING
	memset((u8 *)&regs, '\0', sizeof(regs));
	regs._03.bits.prbs_bist = 1;
	regs._03.bits.en_lp_treot = true;
	regs._03.bits.lpf_sel = pll->lpf_sel;
	regs._03.bits.txfifo_bypass = 0;
	regs._03.bits.freq_hopping = pll->hop_en;
	regs._04.bits.div = pll->div;
	regs._04.bits.cp_s = pll->cp_s;
	regs._04.bits.fdk_s = pll->fdk_s;
	regs._06.bits.nint = pll->nint;
	regs._06.bits.mod_en = pll->mod_en;
	regs._07.bits.kdelta_h = pll->kdelta >> 4;
	regs._07.bits.kdelta_h |= pll->sign << 7;
	regs._08.bits.vco_band = pll->vco_band;
	regs._08.bits.sdm_en = pll->sdm_en;
	regs._08.bits.refin = pll->refin;
	regs._08.bits.kdelta_l = pll->kdelta & 0xf;
	regs._09.bits.kint_h = pll->kint >> 12;
	regs._0a.bits.kint_m = (pll->kint >> 4) & 0xff;
	regs._0b.bits.out_sel = pll->out_sel;
	regs._0b.bits.kint_l = pll->kint & 0xf;
	regs._0c.bits.kstep_h = pll->kstep >> 11;
	regs._0d.bits.kstep_m = (pll->kstep >> 3) & 0xff;
	regs._0e.bits.pll_pu_byp = 0;
	regs._0e.bits.pll_pu = 0;
	regs._0e.bits.kstep_l = pll->kstep & 0x7;

	val = (u8 *)&regs;
	for (i = 0; i < sizeof(regs_off_addr); ++i)
		mipi_dsih_dphy_write(phy, regs_off_addr[i], &val[i], 1);

#else
	for (i = 0; i < sizeof(regs_on_addr); ++i)
		mipi_dsih_dphy_write(phy, regs_on_addr[i], &reg_30_val[i], 1);
#endif
	return 0;

FAIL:
	phy->log_error("sprdfb: failed to set dphy pll registers\n");
	return ERR_DSI_PHY_PLL_NOT_LOCKED;
}

static int mipi_dsih_dphy_config_pll(dphy_t *phy, u32 freq)
{
	int ret;
	struct dphy_pll pll = {0};

	pll.freq = freq;

	/* FREQ = 26M * (NINT + KINT / 2^20) / out_sel */
	ret = mipi_dsih_dphy_calc_pll_param(phy, &pll);
	if (ret)
		goto FAIL;
	ret = mipi_dsih_dphy_set_pll_reg(phy, &pll);
	if (ret)
		goto FAIL;

	return 0;

FAIL:
	phy->log_error("sprdfb: failed to config dphy pll\n");
	return ERR_DSI_PHY_PLL_NOT_LOCKED;
}

static int mipi_dsih_dphy_set_timing_regs(dphy_t *phy, int type, u8 val[])
{
	switch (type) {
	case REQUEST_TIME:
		if (!mipi_dsih_if_dphy_keep_work(phy)) {
			mipi_dsih_dphy_write(phy, 0x31, &val[CLK], 1);
			mipi_dsih_dphy_write(phy, 0x41, &val[DATA], 1);
			mipi_dsih_dphy_write(phy, 0x51, &val[DATA], 1);
		} else {
			mipi_dsih_dphy_write(phy, 0x90, &val[CLK], 1);
			mipi_dsih_dphy_write(phy, 0xa0, &val[DATA], 1);
			mipi_dsih_dphy_write(phy, 0xb0, &val[DATA], 1);
		}
		break;
	case PREPARE_TIME:
		if (!mipi_dsih_if_dphy_keep_work(phy)) {
			mipi_dsih_dphy_write(phy, 0x32, &val[CLK], 1);
			mipi_dsih_dphy_write(phy, 0x42, &val[DATA], 1);
			mipi_dsih_dphy_write(phy, 0x52, &val[DATA], 1);
		} else {
			mipi_dsih_dphy_write(phy, 0x91, &val[CLK], 1);
			mipi_dsih_dphy_write(phy, 0xa1, &val[DATA], 1);
			mipi_dsih_dphy_write(phy, 0xb1, &val[DATA], 1);
		}
		break;
	case ZERO_TIME:
		if (!mipi_dsih_if_dphy_keep_work(phy)) {
			mipi_dsih_dphy_write(phy, 0x33, &val[CLK], 1);
			mipi_dsih_dphy_write(phy, 0x43, &val[DATA], 1);
			mipi_dsih_dphy_write(phy, 0x53, &val[DATA], 1);
		} else {
			mipi_dsih_dphy_write(phy, 0x92, &val[CLK], 1);
			mipi_dsih_dphy_write(phy, 0xa2, &val[DATA], 1);
			mipi_dsih_dphy_write(phy, 0xb2, &val[DATA], 1);
		}
		break;
	case TRAIL_TIME:
		if (!mipi_dsih_if_dphy_keep_work(phy)) {
			mipi_dsih_dphy_write(phy, 0x34, &val[CLK], 1);
			mipi_dsih_dphy_write(phy, 0x44, &val[DATA], 1);
			mipi_dsih_dphy_write(phy, 0x54, &val[DATA], 1);
		} else {
			mipi_dsih_dphy_write(phy, 0x93, &val[CLK], 1);
			mipi_dsih_dphy_write(phy, 0xa3, &val[DATA], 1);
			mipi_dsih_dphy_write(phy, 0xb3, &val[DATA], 1);
		}
		break;
	case EXIT_TIME:
		if (!mipi_dsih_if_dphy_keep_work(phy)) {
			mipi_dsih_dphy_write(phy, 0x36, &val[CLK], 1);
			mipi_dsih_dphy_write(phy, 0x46, &val[DATA], 1);
			mipi_dsih_dphy_write(phy, 0x56, &val[DATA], 1);
		} else {
			mipi_dsih_dphy_write(phy, 0x95, &val[CLK], 1);
			mipi_dsih_dphy_write(phy, 0xA5, &val[DATA], 1);
			mipi_dsih_dphy_write(phy, 0xB5, &val[DATA], 1);
		}
		break;
	case CLKPOST_TIME:
		if (!mipi_dsih_if_dphy_keep_work(phy))
			mipi_dsih_dphy_write(phy, 0x35, &val[CLK], 1);
		else
			mipi_dsih_dphy_write(phy, 0x94, &val[CLK], 1);
		break;

	/* the following just use default value */
	case SETTLE_TIME:
	case TA_GET:
	case TA_GO:
	case TA_SURE:
		break;
	default:
		break;
	}
	return 0;
}

static int mipi_dsih_dphy_config_lane_timing(dphy_t *phy,
				u32 bitclk, int type)
{
	u8 val[2];
	u32 tmp = 0;
	u32 range[2], constant;
	u32 t_ui, t_byteck, t_half_byteck;
	const u32 factor = 2;
	const u32 scale = 100;

	/* t_ui: 1 ui, byteck: 8 ui, half byteck: 4 ui */
	t_ui = 1000 * scale / (bitclk / 1000);
	t_byteck = t_ui << 3;
	t_half_byteck = t_ui << 2;
	constant = t_ui << 1;

	switch (type) {
	case NONE:
	case REQUEST_TIME: /* HS T-LPX: LP-01 */
		/*
		 *  			NOTE
		 * For T-LPX, mipi spec defined min value is 50ns,
		 * but maybe it shouldn't be too small, because BTA,
		 * LP-10, LP-00, LP-01, all of this is related to T-LPX.
		 */
		range[L] = 50 * scale;
		range[H] = INFINITY;
		val[CLK] = ROUND_UP(range[L] * (factor << 1), t_byteck) - 2;
		val[DATA] = val[CLK];
		mipi_dsih_dphy_set_timing_regs(phy, REQUEST_TIME, val);

	case PREPARE_TIME: /* HS sequence: LP-00 */
		range[L] = 38 * scale; /* clock */
		range[H] = 95 * scale;
		tmp = AVERAGE(range[L], range[H]);
		val[CLK] = ROUND_UP(AVERAGE(range[L], range[H]),
				t_half_byteck) - 1;
		range[L] = 40 * scale + 4 * t_ui; /* data */
		range[H] = 85 * scale + 6 * t_ui;
		tmp |= AVERAGE(range[L], range[H]) << 16;
		val[DATA] = ROUND_UP(AVERAGE(range[L], range[H]),
				t_half_byteck) - 1;
		mipi_dsih_dphy_set_timing_regs(phy, PREPARE_TIME, val);

	case ZERO_TIME: /* HS-ZERO */
		range[L] = 300 * scale; /* clock */
		range[H] = INFINITY;
		val[CLK] = ROUND_UP(range[L] * factor + (tmp & 0xffff)
				- 525 * t_byteck / 100, t_byteck) - 2;
		range[L] = 145 * scale + 10 * t_ui; /* data */
		val[DATA] = ROUND_UP(range[L] * factor
				+ ((tmp >> 16) & 0xffff) - 525 * t_byteck / 100,
				t_byteck) - 2;
		mipi_dsih_dphy_set_timing_regs(phy, ZERO_TIME, val);

	case TRAIL_TIME: /* HS-TRAIL */
		range[L] = 60 * scale;
		range[H] = INFINITY;
		val[CLK] = ROUND_UP(range[L] * factor - constant, t_half_byteck);
		range[L] = MAX(8 * t_ui, 60 * scale + 4 * t_ui);
		val[DATA] = ROUND_UP(range[L] * 3 / 2 - constant, t_half_byteck);
		mipi_dsih_dphy_set_timing_regs(phy, TRAIL_TIME, val);

	case EXIT_TIME:
		range[L] = 100 * scale;
		range[H] = INFINITY;
		val[CLK] = ROUND_UP(range[L] * factor, t_byteck) - 2;
		val[DATA] = val[CLK];
		mipi_dsih_dphy_set_timing_regs(phy, EXIT_TIME, val);

	case CLKPOST_TIME:
		range[L] = 60 * scale + 52 * t_ui;
		range[H] = INFINITY;
		val[CLK] = ROUND_UP(range[L] * factor, t_byteck) - 2;
		val[DATA] = val[CLK];
		mipi_dsih_dphy_set_timing_regs(phy, CLKPOST_TIME, val);

	case SETTLE_TIME:
		/*
		 * This time is used for receiver. So for transmitter,
		 * it can be ignored.
		 */
	case TA_GO:
		/*
		 * transmitter drives bridge state(LP-00) before releasing control,
		 * reg 0x1f default value: 0x04, which is good.
		 */
	case TA_SURE:
		/*
		 * After LP-10 state and before bridge state(LP-00),
		 * reg 0x20 default value: 0x01, which is good.
		 */
	case TA_GET:
		/*
		 * receiver drives Bridge state(LP-00) before releasing control
		 * reg 0x21 default value: 0x03, which is good.
		 */
		break;
	default:
		break;
	}
	return 0;
}

static int mipi_dsih_dphy_powerup(dphy_t *phy, int enable)
{
	/*
	 * Make sure that DSI IP is connected to TestChip IO
	 *
	 * get the PHY in power down mode (shutdownz = 0) and
	 * reset it (rstz = 0) to avoid transient periods in
	 * PHY operation during re-configuration procedures.
	 */
	if (enable) {
		mipi_dsih_dphy_clock_en(phy, true);
		mipi_dsih_dphy_stop_wait_time(phy, 0x1C);
		mipi_dsih_dphy_clock_en(phy, true);
		mipi_dsih_dphy_shutdown(phy, false);
		mipi_dsih_dphy_reset(phy, false);
	} else {
		mipi_dsih_dphy_reset(phy, true);
		mipi_dsih_dphy_shutdown(phy, true);
		mipi_dsih_dphy_clock_en(phy, false);
		/* provide an initial active-high test clear pulse in TESTCLR */
		/* DPHY registers will be reset after test clear is set to high */
		mipi_dsih_dphy_test_clear(phy, 0);
		mipi_dsih_dphy_test_clear(phy, 1);
		mipi_dsih_dphy_test_clear(phy, 0);
	}
	return 0;
}

/**
 * Configure D-PHY and PLL module to desired operation mode
 * @param phy: pointer to structure
 *  which holds information about the d-phy module
 * @param no_of_lanes active
 * @param output_freq desired high speed frequency
 * @return error code
 */
dsih_error_t mipi_dsih_dphy_configure(dphy_t *phy,
				u8 lane_num,
				u32 output_freq)
{
	if (!phy)
		return ERR_DSI_INVALID_INSTANCE;
	if (phy->status < INITIALIZED)
		return ERR_DSI_INVALID_INSTANCE;
	if (output_freq < MIN_OUTPUT_FREQ)
		return ERR_DSI_PHY_FREQ_OUT_OF_BOUND;

	if (!mipi_dsih_if_dphy_keep_work(phy))
		mipi_dsih_dphy_powerup(phy, false);

	/* set up board depending on environment if any */
	if (phy->bsp_pre_config)
		phy->bsp_pre_config(phy, 0);

	mipi_dsih_dphy_config_pll(phy, output_freq);
	mipi_dsih_dphy_config_lane_timing(phy, output_freq, NONE);
	mipi_dsih_dphy_no_of_lanes(phy, lane_num);

	if (!mipi_dsih_if_dphy_keep_work(phy))
		mipi_dsih_dphy_powerup(phy, true);

	return 0;
}

/**
 * Close and power down D-PHY module
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @return error code
 */
dsih_error_t mipi_dsih_dphy_close(dphy_t *phy)
{
	if (!phy)
		return ERR_DSI_INVALID_INSTANCE;

	if (!phy->core_read_function || !phy->core_write_function)
		return ERR_DSI_INVALID_IO;

	if (phy->status < INITIALIZED)
		return ERR_DSI_INVALID_INSTANCE;

	mipi_dsih_dphy_reset(phy, true);
	mipi_dsih_dphy_shutdown(phy, true);
	mipi_dsih_dphy_reset(phy, false);
	phy->status = NOT_INITIALIZED;

	return OK;
}

/**
 * Enable clock lane module
 * @param phy pointer to structure
 *  which holds information about the d-phy module
 * @param en
 */
void mipi_dsih_dphy_clock_en(dphy_t *phy, int en)
{
	mipi_dsih_dphy_write_part(phy, R_DPHY_RSTZ, en, 2, 1);
}

/**
 * SPRD ADD
 * Set non-continuous clock mode
 * @param phy pointer to structure
 * which holds information about the d-phy module
 * @param enable 1: enable non continuous clock
 */
void mipi_dsih_dphy_enable_nc_clk(dphy_t *phy, int enable)
{
	mipi_dsih_dphy_write_part(phy, R_DPHY_LPCLK_CTRL, enable, 1, 1);
}

/**
 * Reset D-PHY module
 * @param phy: pointer to structure
 *  which holds information about the d-phy module
 * @param reset
 */
void mipi_dsih_dphy_reset(dphy_t *phy, int enable)
{
	mipi_dsih_dphy_write_part(phy, R_DPHY_RSTZ, !!!enable, 1, 1);
}

/**
 * Power up/down D-PHY module
 * @param phy: pointer to structure
 *  which holds information about the d-phy module
 * @param enable (1: shutdown)
 */
void mipi_dsih_dphy_shutdown(dphy_t *phy, int enable)
{
	mipi_dsih_dphy_write_part(phy, R_DPHY_RSTZ, !!!enable, 0, 1);
}

/**
 * Force D-PHY PLL to stay on while in ULPS
 * @param phy: pointer to structure
 *  which holds information about the d-phy module
 * @param force (1) disable (0)
 * @note To follow the programming model, use wakeup_pll function
 */
void mipi_dsih_dphy_force_pll(dphy_t *phy, int force)
{
	u8 data;

	if (force)
		data = 0x03;
	else
		data = 0x0;

	/* for megocores, to force pll, dphy register should be set */
	mipi_dsih_dphy_write(phy, 0x0e, &data, 1);

	mipi_dsih_dphy_write_part(phy, R_DPHY_RSTZ, force, 3, 1);
}

/**
 * Get force D-PHY PLL module
 * @param phy pointer to structure
 *  which holds information about the d-phy module
 * @return force value
 */
int mipi_dsih_dphy_get_force_pll(dphy_t *phy)
{
	/* FIXME: TODO: for megacores, this status is not true */
	return mipi_dsih_dphy_read_part(phy, R_DPHY_RSTZ, 3, 1);
}

/**
 * Wake up or make sure D-PHY PLL module is awake
 * This function must be called after going into ULPS and before exiting it
 * to force the DPHY PLLs to wake up. It will wait until the DPHY status is
 * locked. It follows the procedure described in the user guide.
 * This function should be used to make sure the PLL is awake, rather than
 * the force_pll above.
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @return error code
 * @note this function has an active wait
 */
int mipi_dsih_dphy_wakeup_pll(dphy_t *phy)
{
	unsigned i = 0;

	if (mipi_dsih_dphy_status(phy, 0x1))
		return OK;

	mipi_dsih_dphy_force_pll(phy, 1);

	for (i = 0; i < DSIH_PHY_ACTIVE_WAIT; i++) {
		if (mipi_dsih_dphy_status(phy, 0x1))
			break;
	}

	if (mipi_dsih_dphy_status(phy, 0x1) == 0)
		return ERR_DSI_PHY_PLL_NOT_LOCKED;

	return OK;
}

/**
 * Configure minimum wait period for HS transmission request after a stop state
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param no_of_byte_cycles [in byte (lane) clock cycles]
 */
void mipi_dsih_dphy_stop_wait_time(dphy_t *phy,
				u8 no_of_byte_cycles)
{
	mipi_dsih_dphy_write_part(phy,
			R_DPHY_IF_CFG,
			no_of_byte_cycles,
			8, 8);
}

/**
 * Set number of active lanes
 * @param phy: pointer to structure
 *  which holds information about the d-phy module
 * @param no_of_lanes
 */
void mipi_dsih_dphy_no_of_lanes(dphy_t *phy, u8 no_of_lanes)
{
	/* currently, dphy only support 2 lanes */
	if (no_of_lanes > MAX_LANES)
		no_of_lanes = MAX_LANES;

	mipi_dsih_dphy_write_part(phy, R_DPHY_IF_CFG,
			no_of_lanes - 1, 0, 2);
}

/**
 * Get number of currently active lanes
 * @param phy: pointer to structure
 *  which holds information about the d-phy module
 * @return number of active lanes
 */
u8 mipi_dsih_dphy_get_no_of_lanes(dphy_t *phy)
{
	return mipi_dsih_dphy_read_part(phy, R_DPHY_IF_CFG, 0, 2);
}

/**
 * Request the PHY module to start transmission of high speed clock.
 * This causes the clock lane to start transmitting DDR clock on the
 * lane interconnect.
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param enable
 * @note this function should be called explicitly by user always except for
 * transmitting
 */
void mipi_dsih_dphy_enable_hs_clk(dphy_t *phy, int enable)
{
	mipi_dsih_dphy_write_part(phy, R_DPHY_LPCLK_CTRL, enable, 0, 1);
}

/**
 * One bit is asserted in the trigger_request (4bits) to cause the lane module
 * to cause the associated trigger to be sent across the lane interconnect.
 * The trigger request is synchronous with the rising edge of the clock.
 * @note: Only one bit of the trigger_request is asserted at any given time, the
 * remaining must be left set to 0, and only when not in LPDT or ULPS modes
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param trigger_request 4 bit request
 */
dsih_error_t mipi_dsih_dphy_escape_mode_trigger(dphy_t *phy,
				u8 trigger_request)
{
	u8 sum = 0;
	int i = 0;
	for (i = 0; i < 4; i++) {
		sum += ((trigger_request >> i) & 1);
	}

	if (sum == 1) { /* clear old trigger */
		mipi_dsih_dphy_write_part(phy, R_DPHY_TX_TRIGGERS,
				0x00, 0, 4);
		mipi_dsih_dphy_write_part(phy, R_DPHY_TX_TRIGGERS,
				trigger_request, 0, 4);

		for (i = 0; i < DSIH_PHY_ACTIVE_WAIT; i++) {
			if (mipi_dsih_dphy_status(phy, 0x0010)) {
				break;
			}
		}
		mipi_dsih_dphy_write_part(phy, R_DPHY_TX_TRIGGERS, 0x00, 0, 4);
		if (i >= DSIH_PHY_ACTIVE_WAIT)
			return ERR_DSI_TIMEOUT;

		return OK;
	}
	return ERR_DSI_INVALID_COMMAND;
}

/**
 * ULPS mode request/exit on all active data lanes.
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param enable (request 1/ exit 0)
 * @return error code
 * @note this is a blocking function. wait upon exiting the ULPS will exceed 1ms
 */
dsih_error_t mipi_dsih_dphy_ulps_data_lanes(dphy_t *phy, int enable)
{
	int timeout;
	/* mask 1 0101 0010 0000 */
	u16 data_lanes_mask = 0;

	if (enable) {
		mipi_dsih_dphy_write_part(phy, R_DPHY_ULPS_CTRL, 1, 2, 1);
		return OK;
	} else {
		if (!mipi_dsih_dphy_status(phy, 0x1))
			return ERR_DSI_PHY_PLL_NOT_LOCKED;

		mipi_dsih_dphy_write_part(phy, R_DPHY_ULPS_CTRL, 1, 3, 1);
		switch (mipi_dsih_dphy_get_no_of_lanes(phy)) {
		/* Fall through */
		case 3:
			data_lanes_mask |= (1 << 12);
		/* Fall through */
		case 2:
			data_lanes_mask |= (1 << 10);
		/* Fall through */
		case 1:
			data_lanes_mask |= (1 << 8);
		/* Fall through */
		case 0:
			data_lanes_mask |= (1 << 5);
			break;
		default:
			data_lanes_mask = 0;
			break;
		}
		/* verify that the DPHY has left ULPM */
		for (timeout = 0; timeout < DSIH_PHY_ACTIVE_WAIT; timeout++) {
			if (mipi_dsih_dphy_status(phy, data_lanes_mask)
					== data_lanes_mask)
				break;
			/* wait at least 1ms */
			for (timeout = 0; timeout < ONE_MS_ACTIVE_WAIT; timeout++)
				;
		}

		if (mipi_dsih_dphy_status(phy, data_lanes_mask)
				!= data_lanes_mask) {
			phy->log_info("stat %x, mask %x",
					mipi_dsih_dphy_status(phy, data_lanes_mask),
					data_lanes_mask);
			return ERR_DSI_TIMEOUT;
		}
		mipi_dsih_dphy_write_part(phy, R_DPHY_ULPS_CTRL, 0, 2, 1);
		mipi_dsih_dphy_write_part(phy, R_DPHY_ULPS_CTRL, 0, 3, 1);
	}
	return OK;
}

/**
 * ULPS mode request/exit on Clock Lane.
 * @param phy pointer to structure which holds information about the
 * d-phy module
 * @param enable 1 or disable 0 of the Ultra Low Power State of the clock lane
 * @return error code
 * @note this is a blocking function. wait upon exiting the ULPS will exceed 1ms
 */
dsih_error_t mipi_dsih_dphy_ulps_clk_lane(dphy_t *phy, int enable)
{
	int timeout;
	/* mask 1000 */
	u16 clk_lane_mask = 0x0008;
	if (enable)
		mipi_dsih_dphy_write_part(phy, R_DPHY_ULPS_CTRL, 1, 0, 1);
	else {
		if (!mipi_dsih_dphy_status(phy, 0x1))
			return ERR_DSI_PHY_PLL_NOT_LOCKED;

		mipi_dsih_dphy_write_part(phy, R_DPHY_ULPS_CTRL, 1, 1, 1);
		/* verify that the DPHY has left ULPM */
		for (timeout = 0; timeout < DSIH_PHY_ACTIVE_WAIT; timeout++) {
			if (mipi_dsih_dphy_status(phy, clk_lane_mask)
					== clk_lane_mask) {
				phy->log_info("stat %x, mask %x",
						mipi_dsih_dphy_status(phy, clk_lane_mask),
						clk_lane_mask);
				break;
			}
			/* wait at least 1ms */
			/* dummy operation for the loop not to be optimised */
			for (timeout = 0; timeout < ONE_MS_ACTIVE_WAIT; timeout++)
				enable = mipi_dsih_dphy_status(phy, clk_lane_mask);
		}
		if (mipi_dsih_dphy_status(phy, clk_lane_mask) != clk_lane_mask)
			return ERR_DSI_TIMEOUT;

		mipi_dsih_dphy_write_part(phy, R_DPHY_ULPS_CTRL, 0, 0, 1);
		mipi_dsih_dphy_write_part(phy, R_DPHY_ULPS_CTRL, 0, 1, 1);
	}
	return OK;
}

/**
 * Get D-PHY PPI status
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param mask
 * @return status
 */
u32 mipi_dsih_dphy_status(dphy_t *phy, u16 mask)
{
	return mipi_dsih_dphy_read_word(phy, R_DPHY_STATUS) & mask;
}

/**
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param value
 */
void mipi_dsih_dphy_test_clock(dphy_t *phy, int value)
{
	mipi_dsih_dphy_write_part(phy, R_DPHY_TST_CRTL0, value, 1, 1);
}

/**
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param value
 */
void mipi_dsih_dphy_test_clear(dphy_t *phy, int value)
{
	mipi_dsih_dphy_write_part(phy, R_DPHY_TST_CRTL0, value, 0, 1);
}

/**
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param on_falling_edge
 */
void mipi_dsih_dphy_test_en(dphy_t *phy, u8 on_falling_edge)
{
	mipi_dsih_dphy_write_part(phy, R_DPHY_TST_CRTL1,
			on_falling_edge, 16, 1);
}

/**
 * @param phy pointer to structure which holds information about the d-phy
 * module
 */
u8 mipi_dsih_dphy_test_data_out(dphy_t *phy)
{
	return mipi_dsih_dphy_read_part(phy, R_DPHY_TST_CRTL1, 8, 8);
}

/**
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param test_data
 */
void mipi_dsih_dphy_test_data_in(dphy_t *phy, u8 test_data)
{
	mipi_dsih_dphy_write_word(phy, R_DPHY_TST_CRTL1, test_data);
}

/**
 * Write to D-PHY module (encapsulating the digital interface)
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param address offset inside the D-PHY digital interface
 * @param data array of bytes to be written to D-PHY
 * @param data_length of the data array
 */
void mipi_dsih_dphy_write(dphy_t *phy, u8 address,
				u8 *data, u8 data_length)
{
	int i = 0;

	if (data) {
		/*
		 * set the TESTCLK input high in preparation
		 * to latch in the desired test mode
		 */
		mipi_dsih_dphy_test_clock(phy, 1);
		/* set the desired test code in the input 8-bit bus TESTDIN[7:0] */
		mipi_dsih_dphy_test_data_in(phy, address);
		/* set TESTEN input high  */
		mipi_dsih_dphy_test_en(phy, 1);
		/*
		 * drive the TESTCLK input low;
		 * the falling edge captures the chosen test code
		 * into the transceiver
		 */
		mipi_dsih_dphy_test_clock(phy, 0);
		/*
		 * set TESTEN input low
		 * to disable further test mode code latching
		 */
		mipi_dsih_dphy_test_en(phy, 0);

		/*
		 * start writing MSB first
		 * set TESTDIN[7:0] to the desired test data appropriate
		 * to the chosen test mode
		 */
		for (i = data_length; i > 0; i--) {
			mipi_dsih_dphy_test_data_in(phy, data[i - 1]);
			/*
			 * pulse TESTCLK high to capture this test data
			 * into the macrocell; repeat these two steps as necessary
			 */
			mipi_dsih_dphy_test_clock(phy, 1);
			mipi_dsih_dphy_test_clock(phy, 0);
		}
	}
}

/**
 * Write to whole register to D-PHY module (encapsulating the bus interface)
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param reg_address offset
 * @param data 32-bit word
 */
void mipi_dsih_dphy_write_word(dphy_t *phy,
				u32 reg_address, u32 data)
{
	if (phy->core_write_function)
		phy->core_write_function(phy->address,
				reg_address, data);
}

/**
 * Write bit field to D-PHY module (encapsulating the bus interface)
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param reg_address offset
 * @param data bits to be written to D-PHY
 * @param shift from the right hand side of the register (big endian)
 * @param width of the bit field
 */
void mipi_dsih_dphy_write_part(dphy_t *phy,
				u32 reg_address, u32 data,
				u8 shift, u8 width)
{
	u32 mask = 0;
	u32 temp = 0;

	if (phy->core_read_function) {
		mask = (1 << width) - 1;
		temp = mipi_dsih_dphy_read_word(phy, reg_address);
		temp &= ~(mask << shift);
		temp |= (data & mask) << shift;
		mipi_dsih_dphy_write_word(phy, reg_address, temp);
	}
}

/**
 * Read whole register from D-PHY module (encapsulating the bus interface)
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param reg_address offset
 * @return data 32-bit word
 */
u32 mipi_dsih_dphy_read_word(dphy_t *phy, u32 reg_address)
{
	if (!phy->core_read_function)
		return ERR_DSI_INVALID_IO;

	return phy->core_read_function(phy->address, reg_address);
}

/**
 * Read bit field from D-PHY module (encapsulating the bus interface)
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param reg_address offset
 * @param shift from the right hand side of the register (big endian)
 * @param width of the bit field
 * @return data bits to be written to D-PHY
 */
u32 mipi_dsih_dphy_read_part(dphy_t *phy,
				u32 reg_address,
				u8 shift,
				u8 width)
{
	return (mipi_dsih_dphy_read_word(phy, reg_address) >> shift)
			& ((1 << width) - 1);
}
