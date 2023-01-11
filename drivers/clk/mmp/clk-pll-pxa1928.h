#ifndef __MACH_CLK_PLL_PXA1928_H
#define __MACH_CLK_PLL_PXA1928_H

#define PXA1928_PLL_DIV_3 8

/* pll vco flags */
#define PXA1928_PLL_POST_ENABLE BIT(0)

struct pxa1928_clk_pll_vco_table {
	unsigned long input_rate;
	unsigned long output_rate;
	unsigned long output_rate_offseted;
	u16 refdiv;
	u16 fbdiv;
	u16 icp;
	u16 kvco;
	u16 ssc_en;
	u16 offset_en;
};

struct pxa1928_clk_pll_vco_params {
	unsigned long	vco_min;
	unsigned long	vco_max;
	/* refdiv, fbdiv, ctrl_bit, sw_en_bit */
	u32	reg_pll_cr;
	/* icp, kvco, rst */
	u32	reg_pll_ctrl1;
	u32	reg_pll_ctrl2;
	u32	reg_pll_ctrl3;
	u32	reg_pll_ctrl4;
	/* lock status */
	u32	reg_pll_lock;
	u32	lock_bit;
	/* gate control */
	u32	reg_pll_gate;
	u16	gate_width;
	u16	gate_shift;
	u32	reg_glb_clk_ctrl;
};

struct pxa1928_clk_pll_out_table {
	unsigned long	input_rate;
	unsigned long	output_rate;
	unsigned int	div_sel;
};

struct pxa1928_clk_pll_out_params {
	unsigned long	input_rate_min;
	/* post divider selection */
	u32	reg_pll_out_div;
	u16	div_width;
	u16	div_shift;
	/* misc ctrl */
	u32	reg_pll_out_ctrl;
};

struct pll_tbl {
	spinlock_t lock;
	const char *vco_name;
	const char *out_name;
	const char *outp_name;
	unsigned long vco_flags;
	unsigned long out_flags;
	unsigned long outp_flags;
	/* dt index */
	unsigned long out_index;
	unsigned long outp_index;
	struct pxa1928_clk_pll_vco_table  *vco_tbl;
	struct pxa1928_clk_pll_out_table  *out_tbl;
	struct pxa1928_clk_pll_out_table  *outp_tbl;
	struct pxa1928_clk_pll_vco_params vco_params;
	struct pxa1928_clk_pll_out_params out_params;
	struct pxa1928_clk_pll_out_params outp_params;
};


/* pll_out flags */
#define PXA1928_PLL_USE_DIV_3	BIT(0)
#define PXA1928_PLL_USE_SYNC_DDR	BIT(1)
#define PXA1928_PLL_USE_ENABLE_BIT	BIT(2)
struct pxa1928_clk_pll_out {
	struct clk_hw	hw;
	unsigned long	flags;
	spinlock_t	*lock;
	void __iomem	*mpmu_base;
	void __iomem	*apmu_base;
	struct pxa1928_clk_pll_out_params	*params;
	struct pxa1928_clk_pll_out_table	*freq_table;
};

extern struct clk *pxa1928_clk_register_pll_vco(const char *name,
		const char *parent_name,
		unsigned long flags, unsigned long vco_flags, spinlock_t *lock,
		void __iomem *mpmu_base, void __iomem *apmu_base,
		struct pxa1928_clk_pll_vco_params *params,
		struct pxa1928_clk_pll_vco_table *freq_table);
extern struct clk *pxa1928_clk_register_pll_out(const char *name,
		const char *parent_name,
		unsigned long pll_flags, spinlock_t *lock,
		void __iomem *mpmu_base, void __iomem *apmu_base,
		struct pxa1928_clk_pll_out_params *params,
		struct pxa1928_clk_pll_out_table *freq_table);


#endif
