#ifndef __MACH_CLK_CORE_PXA1928_H
#define __MACH_CLK_CORE_PXA1928_H


extern void __init pxa1928_core_clk_init(void __iomem *apmu_base);
extern void __init pxa1928_axi_clk_init(void __iomem *apmu_base,
		struct mmp_clk_unit *unit);
extern void __init pxa1928_axi11_clk_init(void __iomem *apmu_base);
extern void __init pxa1928_ddr_clk_init(void __iomem *apmu_base,
		void __iomem *ddrdfc_base, void __iomem *ciu_base,
		struct mmp_clk_unit *unit);

#endif
