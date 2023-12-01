#include <linux/types.h>
#include <soc/samsung/exynos-phycal-if.h>
#include <dt-bindings/pci/pci.h>

#include "pcie-designware.h"
#include "pcie-exynos-common.h"

struct exynos_pcie_phycal {
	struct phycal_seq *pwrdn;
	struct phycal_seq *pwrdn_clr;
	struct phycal_seq *config;
	struct phycal_seq *ia0;
	struct phycal_seq *ia1;
	struct phycal_seq *ia2;
	u32 pwrdn_size;
	u32 pwrdn_clr_size;
	u32 config_size;
	u32 ia0_size;
	u32 ia1_size;
	u32 ia2_size;
};

struct exynos_pcie_phycal_p2vmap {
	struct	phycal_p2v_map *p2vmap;
	u32 size;
};

struct pcie_phyops {

	void (*phy_check_rx_elecidle)(void *phy_pcs_base_regs, int val,
				int ch_num);
	void (*phy_all_pwrdn)(struct exynos_pcie *exynos_pcie, int ch_num);
	void (*phy_all_pwrdn_clear)(struct exynos_pcie *exynos_pcie, int ch_num);
	void (*phy_config)(struct exynos_pcie *exynos_pcie, int ch_num);
	void (*phy_config_regmap)(void *phy_base_regs, void *phy_pcs_base_regs,
				struct regmap *sysreg_phandle,
				void *elbi_base_regs, int ch_num);
	int (*phy_eom)(struct device *dev, void *phy_base_regs);
	void (*phy_input_clk_change)(struct exynos_pcie *exynos_pcie, bool enable);
};

struct exynos_pcie_phy {
	struct pcie_phyops	phy_ops;
	struct exynos_pcie_phycal *pcical_lst;
	struct exynos_pcie_phycal_p2vmap *pcical_p2vmap;
	char *revinfo;
};

#define BIN_MAGIC_NUMBER	0x40484b59
#define BIN_MAGIC_NUM_SIZE	0x4
#define BIN_TOTAL_SIZE		0x4
#define BIN_CHKSUM_SIZE		0x10
#define BIN_REV_HEADER_SIZE	0x8
#define BIN_SIZE_MASK		0xFFFF
#define BIN_HEADER_MASK		(0xFFFF << 16)
#define BIN_HEADER_CH_MASK	(0xFF << 8)
#define BIN_HEADER_ID_MASK	(0xFF << 0)

#define ID_PWRDN		0x1
#define ID_PWRDN_CLR		0x2
#define ID_CONFIG		0x3
#define ID_IA0			0x4
#define ID_IA1			0x5
#define ID_IA2			0x6
