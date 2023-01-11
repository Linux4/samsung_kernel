#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/pm_domain.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>

#include "pm_domain.h"

#define APMU_PWR_CTRL_REG	0xd8
/*
#define APMU_PWR_BLK_TMR_REG	0xdc
#define APMU_PWR_STBL_TMR_REG   0x084
*/
#define APMU_PWR_STATUS_REG	0xf0
#define APMU_CCIC_DBG       0x088
#define APMU_CCIC_CLK_RES_CTRL     0x50
#define APMU_ISP_CLK_RES_CTRL   0x038
#define APMU_CSI_CCIC2_CLK_RES_CTRL	0x24
#define APMU_CCIC1_CLK_GATE_CTRL 0x28
#define APMU_CCIC2_CLK_GATE_CTRL 0x1bc

#define ISP_AHB_EN ((1 << 21) | (1 << 22))

#define MAX_TIMEOUT	5000

#define MMP_PD_POWER_ON_LATENCY		300
#define MMP_PD_POWER_OFF_LATENCY	20


struct mmp_pd_isp_data {
	int id;
	char *name;
	u32 reg_clk_res_ctrl;
	u32 bit_hw_mode;
	u32 bit_auto_pwr_on;
	u32 bit_pwr_stat;
};

struct mmp_pd_isp {
	struct generic_pm_domain genpd;
	void __iomem *reg_base;
	struct clk *clk;
	struct device *dev;
	/* latency for us. */
	u32 power_on_latency;
	u32 power_off_latency;
	const struct mmp_pd_isp_data *data;
	struct pm_qos_request qos_idle;
	u32 lpm_qos;
};

enum {
	MMP_PD_COMMON_ISP_V3,
};

static struct mmp_pd_isp_data isp_v3_data = {
	.id			= MMP_PD_COMMON_ISP_V3,
	.name			= "power-domain-common-isp-v3",
	.reg_clk_res_ctrl	= 0x38,
	.bit_hw_mode		= 15,
	.bit_auto_pwr_on	= 4,
	.bit_pwr_stat		= 4,
};

static int mmp_pd_isp_v3_power_on(struct generic_pm_domain *domain)
{
	struct mmp_pd_isp *pd = container_of(domain,
					struct mmp_pd_isp, genpd);
	const struct mmp_pd_isp_data *data = pd->data;
	void __iomem *base = pd->reg_base;
	u32 val;
	int ret = 0, loop = MAX_TIMEOUT;
	int retry_times = 0;

	val = readl(pd->reg_base + APMU_PWR_STATUS_REG);
	if ((val & (1 << data->bit_pwr_stat)))
		dev_err(pd->dev, "ISP pwr on status is abnormal, it need to check\n");

	pm_qos_update_request(&pd->qos_idle, pd->lpm_qos);

	if (pd->clk)
		clk_prepare_enable(pd->clk);

	/*
	 * val = readl(base + APMU_CCIC_CLK_RES_CTRL);
	 * val |= ISP_AHB_EN;
	 * writel(val, base + APMU_CCIC_CLK_RES_CTRL);
	 */

retry:
	/* set ISP HW on/off mode  */
	val = readl(base + data->reg_clk_res_ctrl);
	val |= (1 << data->bit_hw_mode);
	writel(val, base + data->reg_clk_res_ctrl);

	/* on1, on2, off timer */
	/*
	 * keep this, ISP need wait HW stable
	 * the reg val is recommended
	 * __raw_writel(0x20001fff, base + APMU_PWR_BLK_TMR_REG);
	 * __raw_writel(0x28207, base + APMU_PWR_STBL_TMR_REG);
	 */
	/* auto power on */
	val = readl(base + APMU_PWR_CTRL_REG);
	val |= (1 << data->bit_auto_pwr_on);
	writel(val, base + APMU_PWR_CTRL_REG);

	/*
	 * power on takes 316us, usleep_range(280,290) takes about
	 * 300~320us, so it can reduce the duty cycle.
	 */
	usleep_range(pd->power_on_latency - 10, pd->power_on_latency + 10);

	/* polling PWR_STAT bit */
	for (loop = MAX_TIMEOUT; loop > 0; loop--) {
		val = readl(base + APMU_PWR_STATUS_REG);
		if (val & (1 << data->bit_pwr_stat))
			break;
		usleep_range(4, 6);
	}

	if (loop <= 0) {
		dev_err(pd->dev, "power on timeout\n");
		if (retry_times++ <= 3) {
			dev_err(pd->dev, "power on retrying %d times\n", retry_times);
			goto retry;
		}
		ret = -EBUSY;
		goto out;
	}

out:
	if (pd->clk)
		clk_disable_unprepare(pd->clk);

	return ret;
}

static int mmp_pd_isp_v3_power_off(struct generic_pm_domain *domain)
{
	struct mmp_pd_isp *pd = container_of(domain,
					struct mmp_pd_isp, genpd);
	const struct mmp_pd_isp_data *data = pd->data;
	void __iomem *base = pd->reg_base;
	u32 val;
	int loop, ret = 0;
	int retry_times = 0;

	val = readl(pd->reg_base + APMU_PWR_STATUS_REG);
	if (!(val & (1 << data->bit_pwr_stat)))
		dev_err(pd->dev, "ISP pwr off status is abnormal, it need to check\n");

retry:
	/* auto power off */
	val = readl(base + APMU_PWR_CTRL_REG);
	val &= ~(1 << data->bit_auto_pwr_on);
	writel(val, base + APMU_PWR_CTRL_REG);

	/*
	 * power off takes 23us, add a pre-delay to reduce the
	 * number of polling
	 */
	usleep_range(pd->power_off_latency - 10, pd->power_off_latency + 10);

	/* polling PWR_STAT bit */
	for (loop = MAX_TIMEOUT; loop > 0; loop--) {
		val = readl(pd->reg_base + APMU_PWR_STATUS_REG);
		if (!(val & (1 << data->bit_pwr_stat)))
			break;
		usleep_range(4, 6);
	}
	if (loop <= 0) {
		dev_err(pd->dev, "power off timeout\n");
		if (retry_times++ <= 3) {
			dev_err(pd->dev, "power off retrying %d times\n", retry_times);
			goto retry;
		}
		dev_err(pd->dev, "power off retrying 3 times failed\n");
		ret = -EBUSY;
	}

	pm_qos_update_request(&pd->qos_idle,
		PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);

	val = __raw_readl(base + data->reg_clk_res_ctrl);
	val &= ~(1 << data->bit_hw_mode);
	__raw_writel(val, base + data->reg_clk_res_ctrl);

	return ret;
}

static const struct of_device_id of_mmp_pd_match[] = {
	{
		.compatible = "marvell,power-domain-common-isp-v3",
		.data = (void *)&isp_v3_data,
	},
	{ },
};
MODULE_DEVICE_TABLE(of, of_mmp_pd_match);

static int mmp_pd_isp_v3_probe(struct platform_device *pdev)
{
	struct mmp_pd_isp *pd;
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *of_id;
	struct resource *res;
	int ret;
	u32 latency;
	u32 val;

	if (!np)
		return -EINVAL;

	pd = devm_kzalloc(&pdev->dev, sizeof(*pd), GFP_KERNEL);
	if (!pd)
		return -ENOMEM;

	of_id = of_match_device(of_mmp_pd_match, &pdev->dev);
	if (!of_id)
		return -ENODEV;

	pd->data = of_id->data;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	pd->reg_base = devm_ioremap(&pdev->dev, res->start,
					resource_size(res));
	if (!pd->reg_base)
		return -EINVAL;

	/* set CCIC1 and CCIC2 clk dynamic gating for power saving */
	val = readl(pd->reg_base + APMU_CCIC1_CLK_GATE_CTRL);
	if (val != 0xffff0000)
		writel(0xffff0000, pd->reg_base + APMU_CCIC1_CLK_GATE_CTRL);
	val = readl(pd->reg_base + APMU_CCIC2_CLK_GATE_CTRL);
	if (val != 0xffff0000)
		writel(0xffff0000, pd->reg_base + APMU_CCIC2_CLK_GATE_CTRL);

	/* Some power domain may need clk for power on. */
	pd->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(pd->clk))
		pd->clk = NULL;

	latency = MMP_PD_POWER_ON_LATENCY;
	if (of_find_property(np, "power-on-latency", NULL)) {
		ret = of_property_read_u32(np, "power-on-latency",
						&latency);
		if (ret)
			return ret;
	}
	pd->power_on_latency = latency;

	latency = MMP_PD_POWER_OFF_LATENCY;
	if (of_find_property(np, "power-off-latency-ns", NULL)) {
		ret = of_property_read_u32(np, "power-off-latency-ns",
						&latency);
		if (ret)
			return ret;
	}
	pd->power_off_latency = latency;

	pd->dev = &pdev->dev;

	pd->lpm_qos = PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE;
	if (of_find_property(np, "lpm-qos", NULL)) {
		ret = of_property_read_u32(np, "lpm-qos", &pd->lpm_qos);
		if (ret)
			return ret;
	}
	pd->qos_idle.name = pd->data->name;
	pm_qos_add_request(&pd->qos_idle, PM_QOS_CPUIDLE_BLOCK,
					   PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);

	pd->genpd.of_node = np;
	pd->genpd.name = pd->data->name;
	pd->genpd.power_on = mmp_pd_isp_v3_power_on;
	pd->genpd.power_on_latency_ns = pd->power_on_latency * 1000;
	pd->genpd.power_off = mmp_pd_isp_v3_power_off;
	pd->genpd.power_off_latency_ns = pd->power_off_latency * 1000;

	ret = mmp_pd_init(&pd->genpd, NULL, true);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, pd);

	return 0;
}

static int mmp_pd_isp_v3_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver mmp_pd_isp_v3_driver = {
	.probe		= mmp_pd_isp_v3_probe,
	.remove		= mmp_pd_isp_v3_remove,
	.driver		= {
		.name	= "mmp-pd-isp-v3",
		.owner	= THIS_MODULE,
		.of_match_table = of_mmp_pd_match,
	},
};

static int __init mmp_pd_isp_v3_init(void)
{
	return platform_driver_register(&mmp_pd_isp_v3_driver);
}
subsys_initcall(mmp_pd_isp_v3_init);
