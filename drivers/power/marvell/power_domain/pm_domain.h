#ifndef __MMP_POWER_DOMAIN_H
#define __MMP_POWER_DOMAIN_H
#include <linux/pm_qos.h>

int mmp_pd_init(struct generic_pm_domain *genpd,
	struct dev_power_governor *gov, bool is_off);

#define PD_TAG 0x88888888
struct mmp_pd_common_data {
	int id;
	char *name;
	u32 reg_clk_res_ctrl;
	u32 bit_hw_mode;
	u32 bit_auto_pwr_on;
	u32 bit_pwr_stat;
};

struct mmp_pd_common {
	u32 tag;
	struct generic_pm_domain genpd;
	void __iomem *reg_base;
	struct clk *clk;
	struct device *dev;
	/* latency for us. */
	u32 power_on_latency;
	u32 power_off_latency;
	const struct mmp_pd_common_data *data;

	struct pm_qos_request qos_idle;
	u32		lpm_qos;
};

extern struct list_head gpd_list;
extern struct mutex gpd_list_lock;

#endif
