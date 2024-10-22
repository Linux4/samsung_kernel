// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/clk.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#include "scpsys.h"
#include "mtk-scpsys.h"

#define MTK_POLL_DELAY_US		10
#define MTK_POLL_TIMEOUT		USEC_PER_SEC
#define MTK_POLL_DELAY_US		10
#define MTK_POLL_1MS_TIMEOUT		(1 * USEC_PER_MSEC)
#define MTK_POLL_IRQ_DELAY_US		3
#define MTK_POLL_IRQ_TIMEOUT		USEC_PER_SEC
#define MTK_POLL_HWV_PREPARE_CNT	100
#define MTK_POLL_HWV_PREPARE_US		2

#define MTK_SCPD_CAPS(_scpd, _x)	((_scpd)->data->caps & (_x))

#define PWR_RST_B_BIT			BIT(0)
#define PWR_ISO_BIT			BIT(1)
#define PWR_ON_BIT			BIT(2)
#define PWR_ON_2ND_BIT			BIT(3)
#define PWR_CLK_DIS_BIT			BIT(4)
#define PWR_SRAM_CLKISO_BIT		BIT(5)
#define PWR_SRAM_ISOINT_B_BIT		BIT(6)
#define SSYS_SEL_SPM_OR_SUB_PM		BIT(16)
#define SSYS_PWR_ON_OFF			BIT(17)
#define SSYS_RTFF_GRP_EN_0		BIT(18)
#define SSYS_RTFF_GRP_EN_1		BIT(19)
#define SSYS_SRAM_DORMANT_PD_0		BIT(20)
#define SSYS_SRAM_DORMANT_PD_1		BIT(21)
#define SSYS_SRAM_DORMANT_PD_2		BIT(22)
#define SSYS_SRAM_DORMANT_PD_3		BIT(23)
#define PWR_RTFF_SAVE			BIT(24)
#define PWR_RTFF_NRESTORE		BIT(25)
#define PWR_RTFF_CLK_DIS		BIT(26)
#define PWR_RTFF_SAVE_FLAG		BIT(27)
#define PWR_RTFF_UFS_CLK_DIS		BIT(28)
#define SYSS_PWR_ACK			BIT(29)
#define PWR_ACK				BIT(30)
#define PWR_ACK_2ND			BIT(31)

#define MMINFRA_ON_STA			BIT(0)
#define MMINFRA_OFF_STA			BIT(1)

enum regmap_type {
	INVALID_TYPE = 0,
	BUS_TYPE_NUM,
};

static bool scpsys_init_flag;
static BLOCKING_NOTIFIER_HEAD(scpsys_notifier_list);

int register_scpsys_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&scpsys_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(register_scpsys_notifier);

int unregister_scpsys_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&scpsys_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(unregister_scpsys_notifier);

static struct apu_callbacks *g_apucb;

void register_apu_callback(struct apu_callbacks *apucb)
{
	g_apucb = apucb;
}
EXPORT_SYMBOL_GPL(register_apu_callback);

static int scpsys_regulator_is_enabled(struct scp_domain *scpd)
{
	if (!scpd->supply)
		return 0;

	return regulator_is_enabled(scpd->supply);
}

static int scpsys_regulator_enable(struct scp_domain *scpd)
{
	if (!scpd->supply)
		return 0;

	return regulator_enable(scpd->supply);
}

static int scpsys_regulator_disable(struct scp_domain *scpd)
{
	if (!scpd->supply)
		return 0;

	return regulator_disable(scpd->supply);
}

static void scpsys_clk_disable(struct clk *clk[], int max_num)
{
	int i;

	for (i = max_num - 1; i >= 0; i--)
		clk_disable_unprepare(clk[i]);
}

static int scpsys_clk_enable(struct clk *clk[], int max_num)
{
	int i, ret = 0;

	for (i = 0; i < max_num && clk[i]; i++) {
		ret = clk_prepare_enable(clk[i]);
		if (ret) {
			scpsys_clk_disable(clk, i);
			break;
		}
	}

	return ret;
}

static int scpsys_sram_on(struct scp_domain *scpd, void __iomem *ctl_addr,
			u32 set_bits, u32 ack_bits, bool wait_ack)
{
	pr_notice("%s sram on(%x)\n", scpd->data->name, set_bits);
	return 0;
}

static int scpsys_sram_off(struct scp_domain *scpd, void __iomem *ctl_addr,
			u32 set_bits, u32 ack_bits, bool wait_ack)
{
	pr_notice("%s sram off(%x)\n", scpd->data->name, set_bits);
	return 0;
}

static int scpsys_sram_enable(struct scp_domain *scpd, void __iomem *ctl_addr)
{
	int ret = 0;

	if (MTK_SCPD_CAPS(scpd, MTK_SCPD_SRAM_SLP))
		ret = scpsys_sram_on(scpd, ctl_addr, scpd->data->sram_slp_bits,
				scpd->data->sram_slp_ack_bits, true);
	else
		ret = scpsys_sram_on(scpd, ctl_addr, scpd->data->sram_pdn_bits,
				scpd->data->sram_pdn_ack_bits, true);

	return ret;
}

static int scpsys_sram_disable(struct scp_domain *scpd, void __iomem *ctl_addr)
{
	int ret = 0;

	if (MTK_SCPD_CAPS(scpd, MTK_SCPD_SRAM_SLP))
		ret = scpsys_sram_off(scpd, ctl_addr, scpd->data->sram_slp_bits,
				scpd->data->sram_slp_ack_bits, true);
	else
		ret = scpsys_sram_off(scpd, ctl_addr, scpd->data->sram_pdn_bits,
				scpd->data->sram_pdn_ack_bits, true);

	return ret;
}

static int scpsys_sram_table_enable(struct scp_domain *scpd)
{
	const struct sram_ctl *sram_table = scpd->data->sram_table;
	struct scp *scp = scpd->scp;
	int ret = 0;
	int i;

	for (i = 0; i < MAX_SRAM_STEPS; i++) {
		if (sram_table[i].offs) {
			void __iomem *ctl_addr = scp->base + sram_table[i].offs;

			ret = scpsys_sram_on(scpd, ctl_addr, sram_table[i].msk,
					sram_table[i].ack_msk, sram_table[i].wait_ack);
			if (ret)
				return ret;
		}
	}

	return ret;
}

static int scpsys_sram_table_disable(struct scp_domain *scpd)
{
	const struct sram_ctl *sram_table = scpd->data->sram_table;
	struct scp *scp = scpd->scp;
	int ret = 0;
	int i;

	for (i = 0; i < MAX_SRAM_STEPS; i++) {
		if (sram_table[i].offs) {
			void __iomem *ctl_addr = scp->base + sram_table[i].offs;

			ret = scpsys_sram_off(scpd, ctl_addr, sram_table[i].msk,
					sram_table[i].ack_msk, sram_table[i].wait_ack);
			if (ret)
				return ret;
		}
	}

	return ret;
}

static int set_bus_protection(struct regmap *map, struct bus_prot *bp)
{
	u32 set_ofs = bp->set_ofs;
	u32 en_ofs = bp->en_ofs;
	u32 sta_ofs = bp->sta_ofs;
	u32 mask = bp->mask;

	//pr_notice("%s (0x%x 0x%x 0x%x) set bus %x\n", dev_name(regmap_get_device(map)),
	//		en_ofs, set_ofs, sta_ofs, mask);
	pr_notice("(0x%x 0x%x 0x%x) set bus %x\n",
			en_ofs, set_ofs, sta_ofs, mask);

	return 0;
}

static int clear_bus_protection(struct regmap *map, struct bus_prot *bp)
{
	u32 clr_ofs = bp->clr_ofs;
	u32 en_ofs = bp->en_ofs;
	u32 sta_ofs = bp->sta_ofs;
	u32 mask = bp->mask;

	//pr_notice("%s (0x%x 0x%x 0x%x) clr bus %x\n", dev_name(regmap_get_device(map)),
	//		en_ofs, clr_ofs, sta_ofs, mask);
	pr_notice("(0x%x 0x%x 0x%x) clr bus %x\n",
			en_ofs, clr_ofs, sta_ofs, mask);

	return 0;
}

static int scpsys_bus_protect_disable(struct scp_domain *scpd, unsigned int index)
{
	struct scp *scp = scpd->scp;
	const struct bus_prot *bp_table = scpd->data->bp_table;
	int i;
	int ret = 0;

	for (i = index; i >= 0; i--) {
		struct regmap *map;

		struct bus_prot bp = bp_table[i];

		if (bp.type > INVALID_TYPE && bp.type < scp->num_bus_type)
			map = scp->bus_regmap[bp.type];
		else
			continue;

		if (!map)
			continue;

		if (index != (MAX_STEPS - 1)) {
			pr_notice("step%d (%d)\n", i, bp.type);
			/* restore bus protect setting */
			clear_bus_protection(map, &bp);
		} else {
			pr_notice("step%d (%d)\n", i, bp.type);
			ret = clear_bus_protection(map, &bp);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int scpsys_bus_protect_enable(struct scp_domain *scpd)
{
	struct scp *scp = scpd->scp;
	const struct bus_prot *bp_table = scpd->data->bp_table;
	int i;
	int ret = 0;

	for (i = 0; i < MAX_STEPS; i++) {
		struct regmap *map;
		struct bus_prot bp = bp_table[i];

		if (bp.type > INVALID_TYPE && bp.type < scp->num_bus_type)
			map = scp->bus_regmap[bp.type];
		else
			continue;
		if (!map)
			continue;

		ret = set_bus_protection(map, &bp);

		if (ret) {
			scpsys_bus_protect_disable(scpd, i);
			return ret;
		}
	}

	return 0;
}

static void scpsys_extb_iso_down(struct scp_domain *scpd)
{
	struct scp *scp;
	void __iomem *ctl_addr;

	if (!scpd->data->extb_iso_offs)
		return;

	scp = scpd->scp;
	ctl_addr = scp->base + scpd->data->extb_iso_offs;

	pr_notice("extiso off 0x%x\n", scpd->data->extb_iso_bits);
}

static void scpsys_extb_iso_up(struct scp_domain *scpd)
{
	struct scp *scp;
	void __iomem *ctl_addr;

	if (!scpd->data->extb_iso_offs)
		return;

	scp = scpd->scp;
	ctl_addr = scp->base + scpd->data->extb_iso_offs;
	pr_notice("extiso on 0x%x\n", scpd->data->extb_iso_bits);
}

static int scpsys_power_on(struct generic_pm_domain *genpd)
{
	struct scp_domain *scpd = container_of(genpd, struct scp_domain, genpd);
	struct scp *scp = scpd->scp;
	void __iomem *ctl_addr = scp->base + scpd->data->ctl_offs;
	u32 val;
	int ret = 0;

	pr_notice("[%s](0x%x) on\n", genpd->name, scpd->data->ctl_offs);

	ret = scpsys_regulator_enable(scpd);
	if (ret < 0)
		goto err_regulator;

	ret = scpsys_clk_enable(scpd->clk, MAX_CLKS);
	if (ret)
		goto err_clk;

	ret = scpsys_clk_enable(scpd->lp_clk, MAX_CLKS);
	if (ret)
		goto err_lp_clk;

	if (MTK_SCPD_CAPS(scpd, MTK_SCPD_NON_CPU_RTFF))
		pr_notice("[%s]rtff rs\n", genpd->name);
	else if (MTK_SCPD_CAPS(scpd, MTK_SCPD_PEXTP_PHY_RTFF))
		pr_notice("[%s]rtff rs\n", genpd->name);
	else if (MTK_SCPD_CAPS(scpd, MTK_SCPD_UFS_RTFF) && scpd->rtff_flag) {
		pr_notice("[%s]rtff rs\n", genpd->name);
		scpd->rtff_flag = false;
	}

	if (!MTK_SCPD_CAPS(scpd, MTK_SCPD_BYPASS_CLK) || scpsys_init_flag) {
		pr_notice("subsys clock on\n");
		ret = scpsys_clk_enable(scpd->subsys_clk, MAX_SUBSYS_CLKS);
		if (ret < 0)
			goto err_pwr_ack;

		pr_notice("subsys lp clock on\n");
		ret = scpsys_clk_enable(scpd->subsys_lp_clk, MAX_SUBSYS_CLKS);
		if (ret < 0)
			goto err_pwr_ack;
	}

	if (MTK_SCPD_CAPS(scpd, MTK_SCPD_L2TCM_SRAM)) {
		ret = scpsys_sram_table_enable(scpd);
		if (ret < 0)
			goto err_sram;
	}

	ret = scpsys_sram_enable(scpd, ctl_addr);
	if (ret < 0)
		goto err_sram;

	ret = scpsys_bus_protect_disable(scpd, MAX_STEPS - 1);
	if (ret < 0)
		goto err_sram;

	pr_notice("subsys lp clock off\n");
	scpsys_clk_disable(scpd->subsys_lp_clk, MAX_SUBSYS_CLKS);
	pr_notice("lp clock off\n");
	scpsys_clk_disable(scpd->lp_clk, MAX_CLKS);

	return 0;

err_sram:
	dev_err(scp->dev, "Failed to power on sram/bus %s(%d)\n", genpd->name, ret);
err_pwr_ack:
	dev_err(scp->dev, "Failed to power on mtcmos %s(%d)\n", genpd->name, ret);
err_lp_clk:
	dev_err(scp->dev, "Failed to enable lp_clk %s(%d)\n", genpd->name, ret);
err_clk:
	val = scpsys_regulator_is_enabled(scpd);
	dev_err(scp->dev, "Failed to enable clk %s(%d %d)\n", genpd->name, ret, val);
err_regulator:
	dev_err(scp->dev, "Failed to power on regulator %s(%d)\n", genpd->name, ret);

	return 0;
}

static int scpsys_power_off(struct generic_pm_domain *genpd)
{
	struct scp_domain *scpd = container_of(genpd, struct scp_domain, genpd);
	struct scp *scp = scpd->scp;
	void __iomem *ctl_addr = scp->base + scpd->data->ctl_offs;
	u32 val;
	int ret = 0;

	pr_notice("[%s](0x%x) on\n", genpd->name, scpd->data->ctl_offs);

	if (MTK_SCPD_CAPS(scpd, MTK_SCPD_BYPASS_OFF)) {
		dev_err(scp->dev, "bypass power off %s for bringup\n", genpd->name);
		return 0;
	}

	ret = scpsys_clk_enable(scpd->lp_clk, MAX_CLKS);
	if (ret)
		goto err_lp_clk;

	ret = scpsys_clk_enable(scpd->subsys_lp_clk, MAX_SUBSYS_CLKS);
	if (ret < 0)
		goto err_subsys_lp_clk;

	ret = scpsys_bus_protect_enable(scpd);
	if (ret < 0)
		goto out;

	if (MTK_SCPD_CAPS(scpd, MTK_SCPD_L2TCM_SRAM)) {
		ret = scpsys_sram_table_disable(scpd);
		if (ret < 0)
			goto out;
	}

	ret = scpsys_sram_disable(scpd, ctl_addr);
	if (ret < 0)
		goto out;

	if (!MTK_SCPD_CAPS(scpd, MTK_SCPD_BYPASS_CLK)) {
		scpsys_clk_disable(scpd->subsys_clk, MAX_SUBSYS_CLKS);
		scpsys_clk_disable(scpd->subsys_lp_clk, MAX_SUBSYS_CLKS);
	}

	if (MTK_SCPD_CAPS(scpd, MTK_SCPD_NON_CPU_RTFF) ||
			MTK_SCPD_CAPS(scpd, MTK_SCPD_PEXTP_PHY_RTFF))
		pr_notice("[%s]rtff bk\n", genpd->name);
	else if (MTK_SCPD_CAPS(scpd, MTK_SCPD_UFS_RTFF)) {
		pr_notice("[%s]rtff bk\n", genpd->name);
		scpd->rtff_flag = true;
	}

	scpsys_clk_disable(scpd->clk, MAX_CLKS);

	scpsys_clk_disable(scpd->lp_clk, MAX_CLKS);

	ret = scpsys_regulator_disable(scpd);
	if (ret < 0)
		goto out;

	return 0;

err_subsys_lp_clk:
	scpsys_clk_disable(scpd->subsys_lp_clk, MAX_SUBSYS_CLKS);
err_lp_clk:
	scpsys_clk_disable(scpd->lp_clk, MAX_CLKS);
out:
	val = scpsys_regulator_is_enabled(scpd);
	dev_err(scp->dev, "Failed to power off domain %s(%d %d)\n", genpd->name, ret, val);

	return 0;
}

static int scpsys_md_power_on(struct generic_pm_domain *genpd)
{
	struct scp_domain *scpd = container_of(genpd, struct scp_domain, genpd);
	struct scp *scp = scpd->scp;
	void __iomem *ctl_addr = scp->base + scpd->data->ctl_offs;
	int ret = 0;

	pr_notice("[%s](0x%x) on\n", genpd->name, scpd->data->ctl_offs);

	ret = scpsys_regulator_enable(scpd);
	if (ret < 0)
		return ret;

	scpsys_extb_iso_down(scpd);

	ret = scpsys_clk_enable(scpd->clk, MAX_CLKS);
	if (ret)
		goto err_clk;

	ret = scpsys_clk_enable(scpd->subsys_clk, MAX_SUBSYS_CLKS);
	if (ret < 0)
		goto err_pwr_ack;

	ret = scpsys_sram_enable(scpd, ctl_addr);
	if (ret < 0)
		goto err_sram;

	ret = scpsys_bus_protect_disable(scpd, MAX_STEPS - 1);
	if (ret < 0)
		goto err_sram;

	return 0;

err_sram:
	scpsys_clk_disable(scpd->subsys_clk, MAX_SUBSYS_CLKS);
err_pwr_ack:
	scpsys_clk_disable(scpd->clk, MAX_CLKS);
err_clk:
	scpsys_extb_iso_up(scpd);
	scpsys_regulator_disable(scpd);

	dev_notice(scp->dev, "Failed to power on domain %s\n", genpd->name);

	return 0;
}

static int scpsys_md_power_off(struct generic_pm_domain *genpd)
{
	struct scp_domain *scpd = container_of(genpd, struct scp_domain, genpd);
	struct scp *scp = scpd->scp;
	void __iomem *ctl_addr = scp->base + scpd->data->ctl_offs;
	int ret = 0;

	pr_notice("[%s](0x%x) off\n", genpd->name, scpd->data->ctl_offs);

	ret = scpsys_bus_protect_enable(scpd);
	if (ret < 0)
		goto out;

	ret = scpsys_sram_disable(scpd, ctl_addr);
	if (ret < 0)
		goto out;

	scpsys_clk_disable(scpd->subsys_clk, MAX_SUBSYS_CLKS);

	scpsys_clk_disable(scpd->clk, MAX_CLKS);

	ret = scpsys_regulator_disable(scpd);
	if (ret < 0)
		goto out;

	return 0;

out:
	dev_notice(scp->dev, "Failed to power off domain %s\n", genpd->name);

	return 0;
}

static int scpsys_apu_power_on(struct generic_pm_domain *genpd)
{
	struct scp_domain *scpd = container_of(genpd, struct scp_domain, genpd);
	struct scp *scp = scpd->scp;
	int ret = 0;

	if (g_apucb && g_apucb->apu_power_on) {
		ret = g_apucb->apu_power_on();
		if (ret) {
			dev_notice(scp->dev,
				"Failed to power on domain %s\n", genpd->name);
		}
	}
	return 0;
}

static int scpsys_apu_power_off(struct generic_pm_domain *genpd)
{
	struct scp_domain *scpd = container_of(genpd, struct scp_domain, genpd);
	struct scp *scp = scpd->scp;
	int ret = 0;

	if (g_apucb && g_apucb->apu_power_off) {
		ret = g_apucb->apu_power_off();
		if (ret) {
			dev_notice(scp->dev,
				"Failed to power off domain %s\n", genpd->name);
		}
	}
	return 0;
}

static int mtk_hwv_is_done(struct scp_domain *scpd)
{
	struct scp *scp = scpd->scp;
	u32 val = 0;

	if (scpd->hwv_regmap)
		regmap_read(scpd->hwv_regmap, scpd->data->hwv_done_ofs, &val);
	else
		regmap_read(scp->hwv_regmap, scpd->data->hwv_done_ofs, &val);

	return (val & BIT(scpd->data->hwv_shift));
}

static int mtk_hwv_is_enable_done(struct scp_domain *scpd)
{
	struct scp *scp = scpd->scp;
	struct regmap *hwv_regmap;
	u32 val = 0, val2 = 0, val3 = 0;

	if (scpd->hwv_regmap)
		hwv_regmap = scpd->hwv_regmap;
	else
		hwv_regmap = scp->hwv_regmap;

	regmap_read(hwv_regmap, scpd->data->hwv_done_ofs, &val);
	regmap_read(hwv_regmap, scpd->data->hwv_en_ofs, &val2);
	regmap_read(hwv_regmap, scpd->data->hwv_set_sta_ofs, &val3);

	if ((val & BIT(scpd->data->hwv_shift)) && (val2 & BIT(scpd->data->hwv_shift))
			&& ((val3 & BIT(scpd->data->hwv_shift)) == 0x0))
		return 1;

	return 0;
}

static int mtk_hwv_is_disable_done(struct scp_domain *scpd)
{
	struct scp *scp = scpd->scp;
	struct regmap *hwv_regmap;
	u32 val = 0, val2 = 0;

	if (scpd->hwv_regmap)
		hwv_regmap = scpd->hwv_regmap;
	else
		hwv_regmap = scp->hwv_regmap;

	regmap_read(hwv_regmap, scpd->data->hwv_done_ofs, &val);
	regmap_read(hwv_regmap, scpd->data->hwv_clr_sta_ofs, &val2);

	if ((val & BIT(scpd->data->hwv_shift)) && ((val2 & BIT(scpd->data->hwv_shift)) == 0x0))
		return 1;

	return 0;
}

static int scpsys_hwv_power_on(struct generic_pm_domain *genpd)
{
	struct scp_domain *scpd = container_of(genpd, struct scp_domain, genpd);
	struct scp *scp = scpd->scp;
	struct regmap *hwv_regmap;
	u32 val = 0;
	int ret = 0;
	int tmp;
	int i = 0;

	if (scpd->hwv_regmap)
		hwv_regmap = scpd->hwv_regmap;
	else
		hwv_regmap = scp->hwv_regmap;

	ret = scpsys_regulator_enable(scpd);
	if (ret < 0)
		goto err_regulator;

	ret = scpsys_clk_enable(scpd->clk, MAX_CLKS);
	if (ret)
		goto err_clk;

	ret = scpsys_clk_enable(scpd->lp_clk, MAX_CLKS);
	if (ret)
		goto err_lp_clk;

	/* wait for irq status idle */
	ret = readx_poll_timeout_atomic(mtk_hwv_is_done, scpd, tmp, tmp > 0,
			MTK_POLL_DELAY_US, MTK_POLL_IRQ_TIMEOUT);
	if (ret < 0)
		goto err_hwv_prepare;

	val = BIT(scpd->data->hwv_shift);
	regmap_write(hwv_regmap, scpd->data->hwv_set_ofs, val);
	do {
		regmap_read(hwv_regmap, scpd->data->hwv_set_ofs, &val);
		if ((val & BIT(scpd->data->hwv_shift)) != 0)
			break;

		if (i > MTK_POLL_HWV_PREPARE_CNT)
			goto err_hwv_vote;

		udelay(MTK_POLL_HWV_PREPARE_US);
		i++;
	} while (1);

	/* wait until VOTER_ACK = 1 */
	ret = readx_poll_timeout_atomic(mtk_hwv_is_enable_done, scpd, tmp, tmp > 0,
			MTK_POLL_DELAY_US, MTK_POLL_1MS_TIMEOUT);
	if (ret < 0)
		goto err_hwv_done;

	scpsys_clk_disable(scpd->lp_clk, MAX_CLKS);

	return 0;

err_hwv_done:
	dev_err(scp->dev, "Failed to hwv done timeout %s(%d)\n", genpd->name, ret);
err_hwv_vote:
	dev_err(scp->dev, "Failed to hwv vote timeout %s(%d %x)\n", genpd->name, ret, val);
err_hwv_prepare:
	regmap_read(hwv_regmap, scpd->data->hwv_done_ofs, &val);
	dev_err(scp->dev, "Failed to hwv prepare timeout %s(%d %x)\n", genpd->name, ret, val);
err_lp_clk:
	dev_err(scp->dev, "Failed to enable lp clk %s(%d)\n", genpd->name, ret);
err_clk:
	dev_err(scp->dev, "Failed to enable clk %s(%d)\n", genpd->name, ret);
err_regulator:
	dev_err(scp->dev, "Failed to power on domain %s(%d)\n", genpd->name, ret);

	return 0;
}

static int scpsys_hwv_power_off(struct generic_pm_domain *genpd)
{
	struct scp_domain *scpd = container_of(genpd, struct scp_domain, genpd);
	struct scp *scp = scpd->scp;
	struct regmap *hwv_regmap;
	u32 val = 0;
	int ret = 0;
	int tmp;
	int i = 0;

	if (scpd->hwv_regmap)
		hwv_regmap = scpd->hwv_regmap;
	else
		hwv_regmap = scp->hwv_regmap;

	ret = scpsys_clk_enable(scpd->lp_clk, MAX_CLKS);
	if (ret)
		goto err_lp_clk;

	/* wait for irq status idle */
	ret = readx_poll_timeout_atomic(mtk_hwv_is_done, scpd, tmp, tmp > 0,
			MTK_POLL_DELAY_US, MTK_POLL_IRQ_TIMEOUT);
	if (ret < 0)
		goto err_hwv_prepare;

	val = BIT(scpd->data->hwv_shift);
	regmap_write(hwv_regmap, scpd->data->hwv_clr_ofs, val);
	do {
		regmap_read(hwv_regmap, scpd->data->hwv_clr_ofs, &val);
		if ((val & BIT(scpd->data->hwv_shift)) == 0)
			break;

		if (i > MTK_POLL_HWV_PREPARE_CNT)
			goto err_hwv_vote;
		i++;
		udelay(MTK_POLL_HWV_PREPARE_US);
	} while (1);

	/* delay 100us for stable status */
	udelay(100);

	/* wait until VOTER_ACK = 0 */
	ret = readx_poll_timeout_atomic(mtk_hwv_is_disable_done, scpd, tmp, tmp > 0,
			MTK_POLL_DELAY_US, MTK_POLL_1MS_TIMEOUT);
	if (ret < 0)
		goto err_hwv_done;

	scpsys_clk_disable(scpd->clk, MAX_CLKS);

	scpsys_clk_disable(scpd->lp_clk, MAX_CLKS);

	ret = scpsys_regulator_disable(scpd);
	if (ret < 0)
		goto err_regulator;

	return 0;

err_regulator:
	dev_err(scp->dev, "Failed to regulator disable %s(%d)\n", genpd->name, ret);
err_hwv_done:
	dev_err(scp->dev, "Failed to hwv done timeout %s(%d)\n", genpd->name, ret);
err_hwv_vote:
	dev_err(scp->dev, "Failed to hwv vote timeout %s(%d %x)\n", genpd->name, ret, val);
err_hwv_prepare:
	regmap_read(hwv_regmap, scpd->data->hwv_done_ofs, &val);
	dev_err(scp->dev, "Failed to hwv prepare timeout %s(%d %x)\n", genpd->name, ret, val);
	scpsys_clk_disable(scpd->lp_clk, MAX_CLKS);
err_lp_clk:
	dev_err(scp->dev, "Failed to power off domain %s(%d)\n", genpd->name, ret);

	return 0;
}

static int mtk_mminfra_hwv_is_done(struct scp_domain *scpd)
{
	u32 val = 0;

	regmap_read(scpd->hwv_regmap, scpd->data->hwv_done_ofs, &val);

	if (val & BIT(scpd->data->hwv_shift))
		return 1;

	return 0;
}

static int mtk_mminfra_hwv_is_enable_done(struct scp_domain *scpd)
{
	struct regmap *hwv_regmap;
	u32 val = 0, val2 = 0;

	hwv_regmap = scpd->hwv_regmap;

	regmap_read(hwv_regmap, scpd->data->hwv_done_ofs, &val);
	regmap_read(hwv_regmap, scpd->data->hwv_en_ofs, &val2);

	if ((val & BIT(scpd->data->hwv_shift)) && (val2 & MMINFRA_ON_STA))
		return 1;

	return 0;
}

static int mtk_mminfra_hwv_is_disable_done(struct scp_domain *scpd)
{
	struct regmap *hwv_regmap;
	u32 val = 0, val2 = 0;

	hwv_regmap = scpd->hwv_regmap;

	regmap_read(hwv_regmap, scpd->data->hwv_done_ofs, &val);
	regmap_read(hwv_regmap, scpd->data->hwv_en_ofs, &val2);

	if ((val & BIT(scpd->data->hwv_shift)) && (val2 & MMINFRA_OFF_STA))
		return 1;

	return 0;
}

static int scpsys_mminfra_hwv_power_on(struct generic_pm_domain *genpd)
{
	struct scp_domain *scpd = container_of(genpd, struct scp_domain, genpd);
	struct scp *scp = scpd->scp;
	struct regmap *hwv_regmap;
	u32 val = 0;
	int ret = 0;
	int tmp;
	int i = 0;

	pr_notice("[%s](0x%x) on\n", genpd->name, scpd->data->hwv_set_ofs);

	hwv_regmap = scpd->hwv_regmap;

	/* wait for irq status idle */
	ret = readx_poll_timeout_atomic(mtk_mminfra_hwv_is_done, scpd, tmp, tmp > 0,
			MTK_POLL_DELAY_US, MTK_POLL_IRQ_TIMEOUT);
	if (ret < 0)
		goto err_hwv_prepare;

	val = BIT(scpd->data->hwv_shift);
	regmap_write(hwv_regmap, scpd->data->hwv_set_ofs, val);
	do {
		regmap_read(hwv_regmap, scpd->data->hwv_set_ofs, &val);
		if ((val & BIT(scpd->data->hwv_shift)) != 0)
			break;

		if (i > MTK_POLL_HWV_PREPARE_CNT)
			goto err_hwv_vote;

		udelay(MTK_POLL_HWV_PREPARE_US);
		i++;
	} while (1);

	/* wait until VOTER_ACK = 1 */
	ret = readx_poll_timeout_atomic(mtk_mminfra_hwv_is_enable_done, scpd, tmp, tmp > 0,
			MTK_POLL_DELAY_US, MTK_POLL_1MS_TIMEOUT);
	if (ret < 0)
		goto err_hwv_done;

	return 0;

err_hwv_done:
	dev_err(scp->dev, "Failed to hwv done timeout %s(%d)\n", genpd->name, ret);
err_hwv_vote:
	dev_err(scp->dev, "Failed to hwv vote timeout %s(%d %x)\n", genpd->name, ret, val);
err_hwv_prepare:
	regmap_read(hwv_regmap, scpd->data->hwv_done_ofs, &val);
	dev_err(scp->dev, "Failed to hwv prepare timeout %s(%d %x)\n", genpd->name, ret, val);

	return 0;
}

static int scpsys_mminfra_hwv_power_off(struct generic_pm_domain *genpd)
{
	struct scp_domain *scpd = container_of(genpd, struct scp_domain, genpd);
	struct scp *scp = scpd->scp;
	struct regmap *hwv_regmap;
	u32 val = 0;
	int ret = 0;
	int tmp;
	int i = 0;

	pr_notice("[%s](0x%x) on\n", genpd->name, scpd->data->hwv_clr_ofs);
	hwv_regmap = scpd->hwv_regmap;

	/* wait for irq status idle */
	ret = readx_poll_timeout_atomic(mtk_mminfra_hwv_is_done, scpd, tmp, tmp > 0,
			MTK_POLL_DELAY_US, MTK_POLL_IRQ_TIMEOUT);
	if (ret < 0)
		goto err_hwv_prepare;

	val = BIT(scpd->data->hwv_shift);
	regmap_write(hwv_regmap, scpd->data->hwv_clr_ofs, val);
	do {
		regmap_read(hwv_regmap, scpd->data->hwv_clr_ofs, &val);
		if ((val & BIT(scpd->data->hwv_shift)) == 0)
			break;

		if (i > MTK_POLL_HWV_PREPARE_CNT)
			goto err_hwv_vote;
		i++;
		udelay(MTK_POLL_HWV_PREPARE_US);
	} while (1);

	/* delay 100us for stable status */
	udelay(100);

	/* wait until VOTER_ACK = 0 */
	ret = readx_poll_timeout_atomic(mtk_mminfra_hwv_is_disable_done, scpd, tmp, tmp > 0,
			MTK_POLL_DELAY_US, MTK_POLL_1MS_TIMEOUT);
	if (ret < 0)
		goto err_hwv_done;

	return 0;

err_hwv_done:
	dev_err(scp->dev, "Failed to hwv done timeout %s(%d)\n", genpd->name, ret);
err_hwv_vote:
	dev_err(scp->dev, "Failed to hwv vote timeout %s(%d %x)\n", genpd->name, ret, val);
err_hwv_prepare:
	regmap_read(hwv_regmap, scpd->data->hwv_done_ofs, &val);
	dev_err(scp->dev, "Failed to hwv prepare timeout %s(%d %x)\n", genpd->name, ret, val);
	scpsys_clk_disable(scpd->lp_clk, MAX_CLKS);

	return 0;
}

static int init_subsys_clks(struct platform_device *pdev,
		const char *prefix, struct clk **clk)
{
	struct device_node *node = pdev->dev.of_node;
	u32 prefix_len, sub_clk_cnt = 0;
	struct property *prop;
	const char *clk_name;

	if (!node) {
		dev_notice(&pdev->dev, "Cannot find scpsys node: %ld\n",
			PTR_ERR(node));
		return PTR_ERR(node);
	}

	prefix_len = strlen(prefix);

	of_property_for_each_string(node, "clock-names", prop, clk_name) {
		if (!strncmp(clk_name, prefix, prefix_len) &&
				(clk_name[prefix_len] == '-')) {
			if (sub_clk_cnt >= MAX_SUBSYS_CLKS) {
				dev_notice(&pdev->dev,
					"subsys clk out of range %d\n",
					sub_clk_cnt);
				return -EINVAL;
			}

			clk[sub_clk_cnt] = devm_clk_get(&pdev->dev,
						clk_name);

			if (IS_ERR(clk[sub_clk_cnt])) {
				dev_notice(&pdev->dev,
					"Subsys clk get fail %ld\n",
					PTR_ERR(clk[sub_clk_cnt]));
				return PTR_ERR(clk[sub_clk_cnt]);
			}
			sub_clk_cnt++;
		}
	}

	return sub_clk_cnt;
}

static int init_basic_clks(struct platform_device *pdev, struct clk **clk,
			const char * const *name)
{
	int i;

	for (i = 0; i < MAX_CLKS && name[i]; i++) {
		clk[i] = devm_clk_get(&pdev->dev, name[i]);

		if (IS_ERR(clk[i])) {
			dev_notice(&pdev->dev,
				"get basic clk %s fail %ld\n",
				name[i], PTR_ERR(clk[i]));
			return PTR_ERR(clk[i]);
		}
	}

	return 0;
}

static int mtk_pd_set_performance(struct generic_pm_domain *genpd,
				  unsigned int state)
{
	int i;
	struct scp_domain *scpd =
		container_of(genpd, struct scp_domain, genpd);
	struct scp_event_data scpe;
	struct scp *scp = scpd->scp;
	struct genpd_onecell_data *pd_data = &scp->pd_data;

	for (i = 0; i < pd_data->num_domains; i++) {
		if (genpd == pd_data->domains[i]) {
			dev_dbg(scp->dev, "%d. %s = %d\n",
				i, genpd->name, state);
			break;
		}
	}

	if (i == pd_data->num_domains)
		return 0;

	scpe.event_type = MTK_SCPSYS_PSTATE;
	scpe.genpd = genpd;
	scpe.domain_id = i;
	blocking_notifier_call_chain(&scpsys_notifier_list, state, &scpe);

	return 0;
}

static unsigned int mtk_pd_get_performance(struct generic_pm_domain *genpd,
					   struct dev_pm_opp *opp)
{
	return dev_pm_opp_get_level(opp);
}

static int mtk_pd_get_regmap(struct platform_device *pdev, struct regmap **regmap,
			const char *name)
{
	*regmap = syscon_regmap_lookup_by_phandle(pdev->dev.of_node, name);
	if (PTR_ERR(*regmap) == -ENODEV) {
		dev_notice(&pdev->dev, "%s regmap is null(%ld)\n",
				name, PTR_ERR(*regmap));

		*regmap = NULL;
	} else if (IS_ERR(*regmap)) {
		dev_notice(&pdev->dev, "Cannot find %s controller: %ld\n",
				name, PTR_ERR(*regmap));

		return PTR_ERR(*regmap);
	}

	return 0;
}

struct scp *init_scp(struct platform_device *pdev,
			const struct scp_domain_data *scp_domain_data, int num,
			const struct scp_ctrl_reg *scp_ctrl_reg,
			const char *bus_list[],
			unsigned int type_num)
{
	struct genpd_onecell_data *pd_data;
	struct resource *res;
	int i, ret;
	struct scp *scp;

	scp = devm_kzalloc(&pdev->dev, sizeof(*scp), GFP_KERNEL);
	if (!scp)
		return ERR_PTR(-ENOMEM);

	scp->ctrl_reg.pwr_sta_offs = scp_ctrl_reg->pwr_sta_offs;
	scp->ctrl_reg.pwr_sta2nd_offs = scp_ctrl_reg->pwr_sta2nd_offs;

	scp->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	scp->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(scp->base))
		return ERR_CAST(scp->base);

	scp->domains = devm_kcalloc(&pdev->dev,
				num, sizeof(*scp->domains), GFP_KERNEL);
	if (!scp->domains)
		return ERR_PTR(-ENOMEM);

	pd_data = &scp->pd_data;

	pd_data->domains = devm_kcalloc(&pdev->dev,
			num, sizeof(*pd_data->domains), GFP_KERNEL);
	if (!pd_data->domains)
		return ERR_PTR(-ENOMEM);

	scp->num_bus_type = type_num;
	scp->bus_regmap = devm_kcalloc(&pdev->dev,
			type_num, sizeof(*scp->bus_regmap), GFP_KERNEL);
	if (!scp->bus_regmap)
		return ERR_PTR(-ENOMEM);

	/* get bus prot regmap from dts node, 0 means invalid bus type */
	for (i = 1; i < type_num; i++) {
		ret = mtk_pd_get_regmap(pdev, &scp->bus_regmap[i], bus_list[i]);
		if (ret)
			return ERR_PTR(ret);
	}

	/* get hw voter regmap from dts node */
	ret = mtk_pd_get_regmap(pdev, &scp->hwv_regmap, "hw-voter-regmap");
	if (ret)
		return ERR_PTR(ret);

	for (i = 0; i < num; i++) {
		struct scp_domain *scpd = &scp->domains[i];
		const struct scp_domain_data *data = &scp_domain_data[i];

		if (!data->hwv_comp)
			continue;

		ret = mtk_pd_get_regmap(pdev, &scpd->hwv_regmap, data->hwv_comp);
		if (ret)
			return ERR_PTR(ret);
	}

	for (i = 0; i < num; i++) {
		struct scp_domain *scpd = &scp->domains[i];
		const struct scp_domain_data *data = &scp_domain_data[i];

		scpd->supply = devm_regulator_get_optional(&pdev->dev, data->name);
		if (IS_ERR(scpd->supply)) {
			if (PTR_ERR(scpd->supply) == -ENODEV)
				scpd->supply = NULL;
			else
				return ERR_CAST(scpd->supply);
		}
	}

	pd_data->num_domains = num;

	for (i = 0; i < num; i++) {
		struct scp_domain *scpd = &scp->domains[i];
		struct generic_pm_domain *genpd = &scpd->genpd;
		const struct scp_domain_data *data = &scp_domain_data[i];

		pd_data->domains[i] = genpd;
		scpd->scp = scp;

		scpd->data = data;

		ret = init_basic_clks(pdev, scpd->clk, data->basic_clk_name);
		if (ret)
			return ERR_PTR(ret);
		ret = init_basic_clks(pdev, scpd->lp_clk, data->basic_lp_clk_name);
		if (ret)
			return ERR_PTR(ret);

		if (data->subsys_clk_prefix) {
			ret = init_subsys_clks(pdev,
					data->subsys_clk_prefix,
					scpd->subsys_clk);
			if (ret < 0) {
				dev_notice(&pdev->dev,
					"%s: subsys clk unavailable\n",
					data->name);
				return ERR_PTR(ret);
			}
		}

		if (data->subsys_lp_clk_prefix) {
			ret = init_subsys_clks(pdev,
					data->subsys_lp_clk_prefix,
					scpd->subsys_lp_clk);
			if (ret < 0) {
				dev_notice(&pdev->dev,
					"%s: subsys clk unavailable\n",
					data->name);
				return ERR_PTR(ret);
			}
		}

		genpd->name = data->name;
		if (MTK_SCPD_CAPS(scpd, MTK_SCPD_MD_OPS)) {
			genpd->power_off = scpsys_md_power_off;
			genpd->power_on = scpsys_md_power_on;
		} else if (MTK_SCPD_CAPS(scpd, MTK_SCPD_APU_OPS)) {
			genpd->power_on = scpsys_apu_power_on;
			genpd->power_off = scpsys_apu_power_off;
		} else if (MTK_SCPD_CAPS(scpd, MTK_SCPD_HWV_OPS)) {
			genpd->power_on = scpsys_hwv_power_on;
			genpd->power_off = scpsys_hwv_power_off;
		} else if (MTK_SCPD_CAPS(scpd, MTK_SCPD_MMINFRA_HWV_OPS)) {
			genpd->power_on = scpsys_mminfra_hwv_power_on;
			genpd->power_off = scpsys_mminfra_hwv_power_off;
		} else {
			genpd->power_off = scpsys_power_off;
			genpd->power_on = scpsys_power_on;
		}

		if (MTK_SCPD_CAPS(scpd, MTK_SCPD_ACTIVE_WAKEUP))
			genpd->flags |= GENPD_FLAG_ACTIVE_WAKEUP;
		if (MTK_SCPD_CAPS(scpd, MTK_SCPD_ALWAYS_ON))
			genpd->flags |= GENPD_FLAG_ALWAYS_ON;

		/* Add opp table check first to avoid OF runtime parse failed */
		if (of_count_phandle_with_args(pdev->dev.of_node,
		    "operating-points-v2", NULL) > 0) {
			genpd->set_performance_state = mtk_pd_set_performance;
			genpd->opp_to_performance_state =
				mtk_pd_get_performance;
		}
	}

	return scp;
}
EXPORT_SYMBOL_GPL(init_scp);

int mtk_register_power_domains(struct platform_device *pdev,
				struct scp *scp, int num)
{
	struct genpd_onecell_data *pd_data;
	int i, ret = 0;

	scpsys_init_flag = true;
	for (i = 0; i < num; i++) {
		struct scp_domain *scpd = &scp->domains[i];
		struct generic_pm_domain *genpd = &scpd->genpd;
		bool on;

		/*
		 * Initially turn on all domains to make the domains usable
		 * with !CONFIG_PM and to get the hardware in sync with the
		 * software.  The unused domains will be switched off during
		 * late_init time.
		 */
		if (MTK_SCPD_CAPS(scpd, MTK_SCPD_BYPASS_INIT_ON))
			on = false;
		else
			on = !WARN_ON(genpd->power_on(genpd) < 0);

		pm_genpd_init(genpd, NULL, !on);
	}

	scpsys_init_flag = false;

	/*
	 * We are not allowed to fail here since there is no way to unregister
	 * a power domain. Once registered above we have to keep the domains
	 * valid.
	 */

	pd_data = &scp->pd_data;

	ret = of_genpd_add_provider_onecell(pdev->dev.of_node, pd_data);
	if (ret)
		dev_err(&pdev->dev, "Failed to add OF provider: %d\n", ret);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_register_power_domains);

MODULE_LICENSE("GPL");
