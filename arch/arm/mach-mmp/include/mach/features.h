#ifndef __MACH_FEATURES_H
#define __MACH_FEATURES_H

#include <mach/cputype.h>
#include <mach/pxa168fb.h>

static inline int has_feat_fhd_ddr_rqs(void)
{
	return is_fhd_lcd();
}

/* This is a feature to use SDIO_DAT1 edge wakeup */
static inline int has_feat_sdio_edge_wakeup_only(void)
{
	return cpu_is_pxa988_z1() || cpu_is_pxa986_z1() ||
		cpu_is_pxa988_z2() || cpu_is_pxa986_z2() ||
		cpu_is_pxa986_z3();
}

/* A new feature to indicate pll lock status */
static inline int has_feat_pll_lock_signal(void)
{
	return !(cpu_is_pxa988_z1() || cpu_is_pxa986_z1() ||
			cpu_is_pxa988_z2() || cpu_is_pxa986_z2() ||
			cpu_is_pxa988_z3() || cpu_is_pxa986_z3());
}

/* A new feature to enable MCK4/AXI clock gating */
static inline int has_feat_mck4_axi_clock_gate(void)
{
	return !(cpu_is_pxa988_z1() || cpu_is_pxa986_z1() ||
			cpu_is_pxa988_z2() || cpu_is_pxa986_z2() ||
			cpu_is_pxa988_z3() || cpu_is_pxa986_z3());
}

/* l2 and scu power connected, so must be enable/disable together */
static inline int has_feat_scu_l2_power_connection(void)
{
	return cpu_is_pxa988_z1() || cpu_is_pxa986_z1() ||
		cpu_is_pxa988_z2() || cpu_is_pxa986_z2() ||
		cpu_is_pxa988_z3() || cpu_is_pxa986_z3();
}

/* Disable c2 on other cpus when freq change to avoid conflict */
static inline int has_feat_freqchg_disable_c2(void)
{
	return cpu_is_pxa988_a0() || cpu_is_pxa986_a0();
}

/* This feature allow SW to use IPC to wakeup another core */
static inline int has_feat_ipc_wakeup_core(void)
{
	return cpu_is_pxa988_z1() || cpu_is_pxa986_z1() ||
		cpu_is_pxa988_z2() || cpu_is_pxa986_z2() ||
		cpu_is_pxa988_z3() || cpu_is_pxa986_z3();
}

/* mck4 table0 will be triggered when exit D1P */
static inline int has_feat_mck4_table0_trigger(void)
{
	return cpu_is_pxa988_z1() || cpu_is_pxa986_z1() ||
		cpu_is_pxa988_z2() || cpu_is_pxa986_z2() ||
		cpu_is_pxa988_z3() || cpu_is_pxa986_z3();
}

/* Use vdma as overlay */
static inline int has_feat_video_replace_graphics_dma(void)
{
	return cpu_is_pxa988_z1() || cpu_is_pxa986_z1() ||
		cpu_is_pxa988_z2() || cpu_is_pxa986_z2();
}

static inline int has_feat_isp_reset(void)
{
	return cpu_is_pxa988_z1() || cpu_is_pxa986_z1() ||
		cpu_is_pxa988_z2() || cpu_is_pxa986_z2() ||
		cpu_is_pxa988_z3() || cpu_is_pxa986_z3();
}

static inline int has_feat_legacy_apmu_core_status(void)
{
	return cpu_is_pxa988_z1() || cpu_is_pxa986_z1() ||
		cpu_is_pxa988_z2() || cpu_is_pxa986_z2() ||
		cpu_is_pxa988_z3() || cpu_is_pxa986_z3();
}

static inline int has_feat_d1p_hipwr(void)
{
	return cpu_is_pxa988_z1() || cpu_is_pxa988_z2() ||
		cpu_is_pxa986_z1() || cpu_is_pxa986_z2();
}

static inline int has_feat_debugreg_in_d1p(void)
{
	return cpu_is_pxa988_z1() || cpu_is_pxa988_z2() ||
		cpu_is_pxa988_z3() || cpu_is_pxa986_z1() ||
		cpu_is_pxa986_z2() || cpu_is_pxa986_z3();
}

static inline int has_feat_dll_bypass_opti(void)
{
	return cpu_is_pxa988() || cpu_is_pxa986() ||
		cpu_is_pxa1088();
}

/* PMU DVC feature to ignore core volt request in M2 */
static inline int has_feat_dvc_M2D1Pignorecore(void)
{
	return !(cpu_is_pxa988() || cpu_is_pxa986() || cpu_is_pxa1088());
}

/* default peri_clk = div_value *4, for Zx, peri_clk = div_value *2  */
static inline int has_feat_periclk_mult2(void)
{
	return cpu_is_pxa988_z1() || cpu_is_pxa986_z1() ||
		cpu_is_pxa988_z2() || cpu_is_pxa986_z2() ||
		cpu_is_pxa988_z3() || cpu_is_pxa986_z3();
}

/* when GC is clock on, D1P is disabled. */
static inline int has_feat_disable_d1p_gc_on(void)
{
	return cpu_is_pxa988() || cpu_is_pxa986();
}
/* When VPU is clock on, D1P can't be entered */
static inline int has_feat_disable_d1P_vpu_on(void)
{
	return cpu_is_pxa988() || cpu_is_pxa986();
}

/* enable hardware v-blank DFC feature in pxa1088 */
static inline int has_feat_enable_hw_vblank_DFC(void)
{
	return cpu_is_pxa1088();
}

/* Higher Vmin of DDR feature on 1088 */
static inline int has_feat_higher_ddr_vmin(void)
{
	return cpu_is_pxa1088_a0();
}

static inline int has_feat_pmu_support(void)
{
	return cpu_is_pxa988() || cpu_is_pxa986() ||
		cpu_is_pxa1088_a0();
}
static inline int has_feat_panic_freqscaling(void)
{
	return cpu_is_pxa988() || cpu_is_pxa986();
}

static inline int has_feat_thermal_only_support_polling(void)
{
	return cpu_is_pxa988() || cpu_is_pxa986() || cpu_is_pxa1088();
}
#endif
