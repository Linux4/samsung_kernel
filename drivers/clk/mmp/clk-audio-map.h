#ifndef __CLK_AUDIO_MAP_H
#define __CLK_AUDIO_MAP_H

#include "clk.h"

enum map_power_control {
	HELANX_POWER_CTRL = 0,
	EDENX_POWER_CTRL,
	ULCX_POWER_CTRL,
};

struct map_clk_unit {
	struct mmp_clk_unit unit;
	void __iomem *apmu_base;
	void __iomem *map_base;
	void __iomem *dspaux_base;
	void __iomem *apbsp_base;
	u32 apll;
	u32 power_ctrl;
	void (*poweron_cb)(void __iomem*, int);
};

void __init audio_clk_init(struct device_node *np);
#endif
