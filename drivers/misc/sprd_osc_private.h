#ifndef __SPRD_OSC_PRIVATE_H__
#define __SPRD_OSC_PRIVATE_H__

#include <linux/io.h>

/* osc ctrl register */
#define OSC_CTRL_CLK_NUM_MSK	(0x1FFFUL)
#define OSC_CTRL_RSTN		(BIT(13))
#define OSC_CTRL_START		(BIT(14))
#define OSC_CTRL_EN		(BIT(15))
#define OSC_CTRL_SEL_MSK	(0xF0000UL)

/* osc obs register */
#define OSC_OBS_OVERFLOW_CNT_MSK	(0x7FFFUL)
#define OSC_OBS_OVERFLOW		(BIT(15))

#define SPRD_OSC_IS_OEPN		(0x01)

struct sprd_osc_reg {
	void __iomem *osc_ctrl_reg;
	void __iomem *osc_obs_reg;
};

enum osc_ctrl_num {
	OSC_BIG_CPU0,
	OSC_BIG_CPU1,
	OSC_BIG_CPU2,
	OSC_BIG_CPU3,
	OSB_LIT_CPU0,
	OSB_LIT_CPU1,
	OSB_LIT_CPU2,
	OSB_LIT_CPU3,
	OSC_HVT,
	OSC_SVT,
	OSC_MAX,
};

int sprd_osc_ctrl_sel(unsigned int chain);
int sprd_osc_ctrl_clk_num_set(unsigned int num);
int sprd_osc_start(void);
int sprd_osc_stop(void);
unsigned int sprd_osc_read_obs_speed_cnt(void);

#endif

