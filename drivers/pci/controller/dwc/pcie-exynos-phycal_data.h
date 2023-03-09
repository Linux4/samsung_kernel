#include <linux/sizes.h>

#if IS_ENABLED(CONFIG_SOC_S5E9935)
#include "pcie-exynosS5E9935-phycal.h"
#endif
struct exynos_pcie_phycal pcical_list[] = {
	{
		.pwrdn = pciphy_pwrdn_ch0,
		.pwrdn_clr = pciphy_pwrdn_clr_ch0,
		.config = pciphy_config_ch0,
		.ia0 = pciphy_ch0_ia0,
		.ia1 = pciphy_ch0_ia1,
		.ia2 = pciphy_ch0_ia2,
		.pwrdn_size = ARRAY_SIZE(pciphy_pwrdn_ch0),
		.pwrdn_clr_size = ARRAY_SIZE(pciphy_pwrdn_clr_ch0),
		.config_size = ARRAY_SIZE(pciphy_config_ch0),
		.ia0_size = ARRAY_SIZE(pciphy_ch0_ia0),
		.ia1_size = ARRAY_SIZE(pciphy_ch0_ia1),
		.ia2_size = ARRAY_SIZE(pciphy_ch0_ia2),
	}, {
		.pwrdn = pciphy_pwrdn_ch1,
		.pwrdn_clr = pciphy_pwrdn_clr_ch1,
		.config = pciphy_config_ch1,
		.ia0 = pciphy_ch1_ia0,
		.ia1 = pciphy_ch1_ia1,
		.ia2 = pciphy_ch1_ia2,
		.pwrdn_size = ARRAY_SIZE(pciphy_pwrdn_ch1),
		.pwrdn_clr_size = ARRAY_SIZE(pciphy_pwrdn_clr_ch1),
		.config_size = ARRAY_SIZE(pciphy_config_ch1),
		.ia0_size = ARRAY_SIZE(pciphy_ch1_ia0),
		.ia1_size = ARRAY_SIZE(pciphy_ch1_ia1),
		.ia2_size = ARRAY_SIZE(pciphy_ch1_ia2),
	},
};

char *exynos_pcie_phycal_revinfo = phycal_revinfo;
