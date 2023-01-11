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
#include <linux/clk/mmpdcstat.h>

#include "pm_domain.h"

#define APMU_PWR_CTRL_REG	0xd8
#define APMU_PWR_BLK_TMR_REG	0xdc
#define APMU_PWR_STATUS_REG	0xf0
#define APMU_CCIC_DBG       0x088

#define MAX_TIMEOUT	5000

#define MMP_PD_POWER_ON_LATENCY		300
#define MMP_PD_POWER_OFF_LATENCY	20

enum {
	MMP_PD_COMMON_VPU,
	MMP_PD_COMMON_ISP_V1,
	MMP_PD_COMMON_ISP_V2,
	MMP_PD_COMMON_GC,
	MMP_PD_COMMON_GC2D,
};

static DEFINE_SPINLOCK(mmp_pd_apmu_lock);

static struct mmp_pd_common_data vpu_data = {
	.id			= MMP_PD_COMMON_VPU,
	.name			= "power-domain-common-vpu",
	.reg_clk_res_ctrl	= 0xa4,
	.bit_hw_mode		= 19,
	.bit_auto_pwr_on	= 2,
	.bit_pwr_stat		= 2,
};

/* Area51 ISP */
static struct mmp_pd_common_data isp_v1_data = {
	.id			= MMP_PD_COMMON_ISP_V1,
	.name			= "power-domain-common-isp-v1",
	.reg_clk_res_ctrl	= 0x38,
	.bit_hw_mode		= 15,
	.bit_auto_pwr_on	= 4,
	.bit_pwr_stat		= 4,
};
/* B52 ISP */
static struct mmp_pd_common_data isp_v2_data = {
	.id			= MMP_PD_COMMON_ISP_V2,
	.name			= "power-domain-common-isp-v2",
	.reg_clk_res_ctrl	= 0x38,
	.bit_hw_mode		= 15,
	.bit_auto_pwr_on	= 4,
	.bit_pwr_stat		= 4,
};

static struct mmp_pd_common_data gc_data = {
	.id			= MMP_PD_COMMON_GC,
	.name			= "power-domain-common-gc",
	.reg_clk_res_ctrl	= 0xcc,
	.bit_hw_mode		= 11,
	.bit_auto_pwr_on	= 0,
	.bit_pwr_stat		= 0,
};

static struct mmp_pd_common_data gc2d_data = {
	.id			= MMP_PD_COMMON_GC2D,
	.name			= "power-domain-common-gc2d",
	.reg_clk_res_ctrl	= 0xf4,
	.bit_hw_mode		= 11,
	.bit_auto_pwr_on	= 6,
	.bit_pwr_stat		= 6,
};


static void mmp_pd_common_dumpregs(struct mmp_pd_common *pd)
{
	const struct mmp_pd_common_data *data = pd->data;
	void __iomem *base = pd->reg_base;

	dev_info(pd->dev, "======DUMP REGISTER=====\n");

	pr_info("clk_res_ctrl(0x%x) is 0x08%x\n", data->reg_clk_res_ctrl,
			__raw_readl(base + data->reg_clk_res_ctrl));
	pr_info("APMU_PWR_CTRL_REG(0x%x)is 0x08%x\n", APMU_PWR_CTRL_REG,
			__raw_readl(base + APMU_PWR_CTRL_REG));
	pr_info("APMU_PWR_BLK_TMR_REG(0x%x)is 0x08%x\n", APMU_PWR_BLK_TMR_REG,
			__raw_readl(base + APMU_PWR_BLK_TMR_REG));

	if (data->id == MMP_PD_COMMON_ISP_V1) {
		pr_info("APMU_CCIC_DBG(0x%x)is 0x08%x\n", APMU_CCIC_DBG,
			__raw_readl(base + APMU_CCIC_DBG));
	}

	pr_info("APMU_PWR_STATUS_REG(0x%x) is 0x08%x\n", APMU_PWR_STATUS_REG,
			__raw_readl(base + APMU_PWR_STATUS_REG));
}

static int mmp_pd_common_power_on(struct generic_pm_domain *domain)
{
	struct mmp_pd_common *pd = container_of(domain,
					struct mmp_pd_common, genpd);
	const struct mmp_pd_common_data *data = pd->data;
	void __iomem *base = pd->reg_base;
	u32 val;
	int ret = 0, loop = MAX_TIMEOUT;

	pm_qos_update_request(&pd->qos_idle, pd->lpm_qos);

	if (pd->clk)
		clk_prepare_enable(pd->clk);

	/* set GPU HW on/off mode  */
	val = __raw_readl(base + data->reg_clk_res_ctrl);
	val |= (1 << data->bit_hw_mode);
	__raw_writel(val, base + data->reg_clk_res_ctrl);

	spin_lock(&mmp_pd_apmu_lock);
	/* on1, on2, off timer */
	__raw_writel(0x20001fff, base + APMU_PWR_BLK_TMR_REG);

	/* auto power on */
	val = __raw_readl(base + APMU_PWR_CTRL_REG);
	val |= (1 << data->bit_auto_pwr_on);
	__raw_writel(val, base + APMU_PWR_CTRL_REG);

	spin_unlock(&mmp_pd_apmu_lock);

	if (data->id == MMP_PD_COMMON_ISP_V1) {
		val =  __raw_readl(base + APMU_CCIC_DBG);
		val |= 0x06000000;
		__raw_writel(val, base + APMU_CCIC_DBG);
	}

	/*
	 * power on takes 316us, usleep_range(280,290) takes about
	 * 300~320us, so it can reduce the duty cycle.
	 */
	usleep_range(pd->power_on_latency - 10, pd->power_on_latency + 10);

	/* polling PWR_STAT bit */
	for (loop = MAX_TIMEOUT; loop > 0; loop--) {
		val = __raw_readl(base + APMU_PWR_STATUS_REG);
		if (val & (1 << data->bit_pwr_stat))
			break;
		usleep_range(4, 6);
	}

	if (loop <= 0) {
		dev_err(pd->dev, "power on timeout\n");
		mmp_pd_common_dumpregs(pd);
		ret = -EBUSY;
		goto out;
	}
	clk_dcstat_event_check(pd->clk, PWR_ON, 0);

out:
	if (pd->clk)
		clk_disable_unprepare(pd->clk);

	return ret;
}

static int mmp_pd_common_power_off(struct generic_pm_domain *domain)
{
	struct mmp_pd_common *pd = container_of(domain,
					struct mmp_pd_common, genpd);
	const struct mmp_pd_common_data *data = pd->data;
	void __iomem *base = pd->reg_base;
	u32 val;
	int loop;

	spin_lock(&mmp_pd_apmu_lock);

	/* auto power off */
	val = __raw_readl(base + APMU_PWR_CTRL_REG);
	val &= ~(1 << data->bit_auto_pwr_on);
	__raw_writel(val, base + APMU_PWR_CTRL_REG);

	spin_unlock(&mmp_pd_apmu_lock);

	/*
	 * power off takes 23us, add a pre-delay to reduce the
	 * number of polling
	 */
	usleep_range(pd->power_off_latency - 10, pd->power_off_latency + 10);

	/* polling PWR_STAT bit */
	for (loop = MAX_TIMEOUT; loop > 0; loop--) {
		val = __raw_readl(pd->reg_base + APMU_PWR_STATUS_REG);
		if (!(val & (1 << data->bit_pwr_stat)))
			break;
		usleep_range(4, 6);
	}
	if (loop <= 0) {
		dev_err(pd->dev, "power off timeout\n");
		mmp_pd_common_dumpregs(pd);
		return -EBUSY;
	}

	clk_dcstat_event_check(pd->clk, PWR_OFF, 0);

	pm_qos_update_request(&pd->qos_idle,
		PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);

	val = __raw_readl(base + data->reg_clk_res_ctrl);
	val &= ~(1 << data->bit_hw_mode);
	__raw_writel(val, base + data->reg_clk_res_ctrl);

	return 0;
}

static const struct of_device_id of_mmp_pd_match[] = {
	{
		.compatible = "marvell,power-domain-common-vpu",
		.data = (void *)&vpu_data,
	},
	{
		.compatible = "marvell,power-domain-common-isp-v1",
		.data = (void *)&isp_v1_data,
	},
	{
		.compatible = "marvell,power-domain-common-isp-v2",
		.data = (void *)&isp_v2_data,
	},
	{
		.compatible = "marvell,power-domain-common-gc",
		.data = (void *)&gc_data,
	},
	{
		.compatible = "marvell,power-domain-common-gc2d",
		.data = (void *)&gc2d_data,
	},
	{ },
};
MODULE_DEVICE_TABLE(of, of_mmp_pd_match);

static int mmp_pd_common_probe(struct platform_device *pdev)
{
	struct mmp_pd_common *pd;
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *of_id;
	struct resource *res;
	int ret;
	u32 latency;

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

	pd->tag = PD_TAG;
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
		ret = of_property_read_u32(np, "lpm-qos",
				&pd->lpm_qos);
		if (ret)
			return ret;
	}

	pd->qos_idle.name = pd->data->name;
	pm_qos_add_request(&pd->qos_idle, PM_QOS_CPUIDLE_BLOCK,
			PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);

	pd->genpd.of_node = np;
	pd->genpd.name = pd->data->name;
	pd->genpd.power_on = mmp_pd_common_power_on;
	pd->genpd.power_on_latency_ns = pd->power_on_latency * 1000;
	pd->genpd.power_off = mmp_pd_common_power_off;
	pd->genpd.power_off_latency_ns = pd->power_off_latency * 1000;

	ret = mmp_pd_init(&pd->genpd, NULL, true);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, pd);

	return 0;
}

static int mmp_pd_common_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver mmp_pd_common_driver = {
	.probe		= mmp_pd_common_probe,
	.remove		= mmp_pd_common_remove,
	.driver		= {
		.name	= "mmp-pd-common",
		.owner	= THIS_MODULE,
		.of_match_table = of_mmp_pd_match,
	},
};

static int __init mmp_pd_common_init(void)
{
	return platform_driver_register(&mmp_pd_common_driver);
}
subsys_initcall(mmp_pd_common_init);
