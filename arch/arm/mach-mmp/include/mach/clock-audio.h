
#ifndef __MACH_CLK_AUDIO_H
#define __MACH_CLK_AUDIO_H
/*
 * MMP platform has audio island, the clock tree is:
 *
 *  ------------------
 *  | MPMU's i2s clk |---                          div1
 *  ------------------  |                         ------   ----------
 *                      |                      -->| /n |-->| sysclk |
 *                      |                      |  ------   ----------
 *  ------------------  |   ----------------   |
 *  |      vcxo      |--|-->| sspa clk mux |-->|
 *  ------------------  |   ----------------   |   div2
 *          |           |                      |  ------   ------------
 *         \ /          |                      -->| /n |-->| sspa clk |
 *  ------------------  |                         ------   ------------
 *  |   audio pll    |---
 *  ------------------
 *
 * For audio island has completely isolation by self,
 * it's better only use the vcxo or audio pll as the source;
 * and use audio pll we can get more flexible clock tree
 * for different sample rate.
 *
 * So now the flow for setting audio clock is:
 * vcxo -> pll -> audio clock
 *
 */
struct apll_set {
	/* in */
	uint32_t vcxo;

	/* pll setting */
	uint32_t mclk;
	uint32_t fbcclk;
	uint32_t fract;

	/* audio clk setting */
	struct aclk_set *ac_set;
	uint32_t ac_num;
};

struct aclk_set {
	/* audio clk out */
	uint32_t aclk;

	/* setting val */
	uint32_t postdiv;
	uint32_t oclk_modulo;
	uint32_t oclk_pattern;
};

#define AUDIO_CLOCK_RATE_PRE_INIT	(-1)

extern struct clkops audio_clk_ops;
extern struct clkops sysclk_ops;
extern struct clkops sspa1_clk_ops;
extern struct clkops sspa2_clk_ops;

#endif
