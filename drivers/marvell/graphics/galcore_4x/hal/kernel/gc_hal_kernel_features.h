#ifndef _gc_hal_kernel_features_h_
#define _gc_hal_kernel_features_h_

#include <mach/cputype.h>

/*
 *  +------------------------------------------------------------+
 *  |                  DEFAULT FEATURE TABLE                     |
 *  +------------------------------------------------------------+
 *  |                                       platforms            |
 *  |     features               pxa986  pxa988  pxa1088 pxa1L88 |
 *  +------------------------------------------------------------+
 *  | separated power clock         +       +       +       +    |
 *  | separated gc clock                            +       +    |
 *  | separated gc power                                    +    |
 *  | has shader clock                                      +    |
 *  | has available gpu nums        1       1       2       3    |
 *  +------------------------------------------------------------+
 *  | clock gate when idle          +       +       +       +    |
 *  | power gate when idle          +       +       +       +    |
 *  | enable dfc gpufreq            +       +       +       +    |
 *  | use pulse eater profiler                      +       +    |
 *  | enable qos support            +       +       +       +    |
 *  | enable pm_runtime                                          |
 *  | enable gc trace               +       +       +       +    |
 *  +------------------------------------------------------------+
 *  | enable 4K MMU config          +       +       +            |
 *  | limit gc outstanding num      +       +       +            |
 *  +------------------------------------------------------------+
 */

/* refer to MRVL_CONFIG_POWER_CLOCK_SEPARATED(removed) */
static inline int has_feat_separated_power_clock(void)
{
    return 1;
}

/* refer to MRVL_2D3D_CLOCK_SEPARATED(removed) */
static inline int has_feat_separated_gc_clock(void)
{
    /* !(cpu_is_pxa988() || cpu_is_pxa986()) */
    return cpu_is_pxa1088() || cpu_is_pxa1L88();
}

/* refer to MRVL_2D3D_POWER_SEPARATED(removed) */
static inline int has_feat_separated_gc_power(void)
{
    return cpu_is_pxa1L88();
}

/* refer to MRVL_CONFIG_SHADER_CLK_CONTROL */
static inline int has_feat_gc_shader(void)
{
    return cpu_is_pxa1L88();
}

static inline unsigned int has_avail_gpu_numbers(void)
{
    /* 3d+2d combined */
    if(cpu_is_pxa988() || cpu_is_pxa986())
        return 1;

    /* 3d,2d separated */
    else if(cpu_is_pxa1088())
        return 2;

    /* 3d,2d,shader for cpu_is_pxa1L88() alike */
    return 3;
}

#if (defined ANDROID)
/* refer to MRVL_POLICY_CLKOFF_WHEN_IDLE(removed) & gcdPOWER_SUSPEND_WHEN_IDLE */
static inline int has_feat_policy_clock_off_when_idle(void)
{
    return 1;
}

/* refer to gcdPOWEROFF_TIMEOUT!=0  */
static inline int has_feat_policy_power_off_when_idle(void)
{
    return 1;
}

/* refer to MRVL_CONFIG_ENABLE_GPUFREQ */
static inline int has_feat_gpufreq(void)
{
    return 1;
}

/* refer to MRVL_PULSE_EATER_COUNT_NUM!=0  */
static inline int has_feat_pulse_eater_profiler(void)
{
    return (cpu_is_pxa1088() || cpu_is_pxa1L88());
}

/* refer to MRVL_CONFIG_ENABLE_QOS_SUPPORT */
static inline int has_feat_qos_support(void)
{
    return 1;
}

/* refer to MRVL_CONFIG_USE_PM_RUNTIME (disable by default) */
static inline int has_feat_pm_runtime(void)
{
#if MRVL_CONFIG_USE_PM_RUNTIME
    return 1;
#else
    return 0;
#endif
}

/* refer to gcdENABLE_4K_MMU_CONGIG(removed) */
static inline int has_feat_enable_4kmmu_config(void)
{
    return !(cpu_is_pxa1L88());
}

/* refer to gcdENABLE_SET_LIMITED_OUTSTANDING(removed) */
static inline int has_feat_limit_gc_outstanding(void)
{
    return !(cpu_is_pxa1L88());
}

#else
static inline int has_feat_policy_clock_off_when_idle(void) { return 0; }
static inline int has_feat_policy_power_off_when_idle(void) { return 0; }
static inline int has_feat_gpufreq(void) { return 0; }
static inline int has_feat_pulse_eater_profiler(void) { return 0; }
static inline int has_feat_qos_support(void) { return 0; }
static inline int has_feat_pm_runtime(void) { return 0; }
static inline int has_feat_enable_4kmmu_config(void) { return 0; }
static inline int has_feat_limit_gc_outstanding(void) { return 0; }
#endif /* defined ANDROID */

#endif /* _gc_hal_kernel_features_h_ */
