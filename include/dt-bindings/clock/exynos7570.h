/*
 * Device Tree binding constants for Exynos7570 clock controller.
 */

#ifndef _DT_BINDINGS_CLOCK_EXYNOS_7570_H
#define _DT_BINDINGS_CLOCK_EXYNOS_7570_H

/* Refers to clock id (enum exynos7570_clks) clk-exynos7570.c */
#define OSCCLK		1
#define CLK_FIN_PLL	OSCCLK

#define CLK_GATE_UART	20
#define CLK_SCLK_UART	20
#define CLK_PCLK_UART	360
#define CLK_MCT		370

#define CLK_DVFS_MIF	1000
#define CLK_DVFS_MIF_SW	1001
#define CLK_DVFS_INT	1002
#define CLK_DVFS_CAM	1003
#define CLK_DVFS_DISP	1004

#define CLK_GATE_MSCL	610 /* see clk-exynos7570.c */
#define CLK_GATE_JPEG	611 /* see clk-exynos7570.c */

#define CLK_SYSMMU_BASE 1100
#define CLK_VCLK_SYSMMU_MFC_MSCL		(CLK_SYSMMU_BASE + 0)
#define CLK_VCLK_SYSMMU_DISP_AUD		(CLK_SYSMMU_BASE + 1)
#define CLK_VCLK_SYSMMU_ISP			(CLK_SYSMMU_BASE + 2)

#endif /* _DT_BINDINGS_CLOCK_EXYNOS_7570_H */
