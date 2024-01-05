#ifndef __SND_SOC_SAMSUNG_AUDSS_H
#define __SND_SOC_SAMSUNG_AUDSS_H

struct audss_info {
	/* AUDSS SFRs */
	void __iomem	*base;
	struct device	*dev;

	/* Rate of RCLK source clock */
	unsigned long rclk_srcrate;
	/* Frame Clock */
	unsigned frmclk;

	/* I2S Controller's core clock */
	struct clk *clk;
#ifdef CONFIG_SND_SAMSUNG_USE_IDMA
	/* SRP clock */
	struct clk *srpclk;
#endif
	/* Audss's source clock */
	struct clk *fout_epll;
	struct clk *mout_epll;
	struct clk *mout_audss;

	/* Audss's i2s source clock */
	struct clk *mout_i2s;
	/* SRP clock's divider */
	struct clk *dout_srp;
	/* Bus clock's divider */
	struct clk *dout_bus;
	/* i2s clock's divider */
	struct clk *dout_i2s;
	/* Clock for generating I2S signals */
	struct clk *op_clk;
	/* Array of clock names for op_clk */
	const char **src_clk;

	bool	clk_init;

	u32	suspend_audss_clksrc;
	u32	suspend_audss_clkdiv;
	u32	suspend_audss_clkgate;
};

void audss_enable(void);
void audss_disable(void);

extern struct clk *audss_get_i2s_opclk(int clk_id);

#endif

