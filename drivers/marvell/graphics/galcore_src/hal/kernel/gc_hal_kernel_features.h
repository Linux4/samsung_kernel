#ifndef _gc_hal_kernel_features_h_
#define _gc_hal_kernel_features_h_

#include <linux/cputype.h>

static inline int has_feat_axi_freq_change(void)
{
    return cpu_is_pxa1908() || cpu_is_pxa1U88()
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
    || cpu_is_pxa1936()
#endif
    ;
}

/* refer to MRVL_CONFIG_MMP_PM_DOMAIN(used in mrvl4x)*/
static inline int has_feat_power_domain(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
    return cpu_is_pxa1U88() || cpu_is_pxa1908() || cpu_is_pxa1936();
#else
    return 0;
#endif
}

/* refer to MRVL_CONFIG_POWER_CLOCK_SEPARATED(removed) */
static inline int has_feat_separated_power_clock(void)
{
    return 1;
}

/* refer to MRVL_2D3D_CLOCK_SEPARATED(removed) */
static inline int has_feat_separated_gc_clock(void)
{
    return 1;
}

/* refer to MRVL_2D3D_POWER_SEPARATED(removed) */
static inline int has_feat_separated_gc_power(void)
{
    return 1;
}

/* refer to MRVL_2D_POWER_DYNAMIC_ONOFF(removed)
    On pxa1928 Zx, GC2D has no power switch.
*/
static inline int has_feat_2d_power_onoff(void)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,10,0)
    return !cpu_is_pxa1928_zx();
#else
    return 1;
#endif
}

/* has pulse eater support */
static inline int has_feat_pulse_eater_profiler(void)
{
    return cpu_is_pxa1U88();
}

/* refer to MRVL_CONFIG_SHADER_CLK_CONTROL */
static inline int has_feat_shader_indept_dfc(void)
{
    return cpu_is_pxa1U88();
}

/* MRVL_3D_CORE_SH_CLOCK_SEPARATED */
static inline int has_feat_3d_shader_clock(void)
{
    return 1;
}

/* refer to MRVL_POLICY_CLKOFF_WHEN_IDLE(removed) */
static inline int has_feat_policy_clock_off_when_idle(void)
{
    return 1;
}

/* refer to MRVL_DFC_PROTECT_CLK_OPERATION(removed)
    To prevent clock on/off when DFC.
*/
static inline int has_feat_dfc_protect_clk_op(void)
{
    return cpu_is_pxa1928() || cpu_is_pxa1U88() || cpu_is_pxa1908()
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
    || cpu_is_pxa1936()
#endif
    ;
}

/*
    refer to MRVL_DFC_JUMP_HI_INDIRECT (removed)
    Change to intermediate frequency before to high frequency,
    and set freq to lowest when power off.
*/
static inline int has_feat_freq_change_indirect(void)
{
    return cpu_is_pxa1928();
}

/*
    fix gc2d axi bus error changeing rtc/wtc reigister when there is tranfic.
*/
static inline int has_feat_freq_change_when_idle(void)
{
    return cpu_is_pxa1928();
}

static inline int has_feat_ulc(void)
{
    return cpu_is_pxa1908();
}

static inline int has_feat_3d_axi_combine_limit(void)
{
    return 0;
}

/**************************************************************
 * Workaround for hardware bugs
 *************************************************************/
/*
 * not enable on pxa1908.
 */
static inline int has_workaround_fe_read_cmd_end(void)
{
	return !cpu_is_pxa1908();
}
#endif
