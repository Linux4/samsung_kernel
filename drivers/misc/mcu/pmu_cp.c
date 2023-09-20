 /*
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2010 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/pmu_cp.h>

#include <mach/regs-pmu-exynos7580.h>

struct pmu_cp_regs {
	u32 ctrl;
	u32 stat;
	u32 debug;
	u32 rev1;
	u32 semaphore;
	u32 rev4;
	u32 rev5;
	u32 rev6;
	u32 mem_cfg;
	u32 mif_access_win0;
	u32 mif_access_win1;
	u32 mif_access_win2;
	u32 peri_access_win;
	u32 boot_rst_cfg;
	u32 mif_tzpc_cfg;
	u32 aud_path_cfg;
	u32 mif_clkctrl;
} __packed;

struct pmu_cp_reset_regs {
	u32 logic_reset_cp;	/* 8.8.1.1.221 */
	u32 reset_asb_cp;	/* 8.8.1.1.222 */
	u32 tcxo_gate;		/* 8.8.1.1.223 */
	u32 cleany_bus;		/* 8.8.1.1.224 */
} __packed;

struct pmu_cp_data {
	struct pmu_cp_regs *regs;
	struct pmu_cp_reset_regs *reset;
	u32 smem_base; /*TODO: get address from DT phandle*/
	u32 smem_size;
} pcdata;

#define CP_PWRON BIT(1) /* 8.8.1.1.7 CP_CTRL */
#define CP_START BIT(3)
#define CP_ACTIVE_REQ_CLR BIT(6)
#define CP_RESET_REQ_CLR BIT(8)
#define MASK_CP_PWRDN_DONE BIT(9)
int exynos_pmu_cp_off(void)
{
	struct pmu_cp_regs *regs = pcdata.regs;
	u32 ctrl;

	ctrl = __raw_readl(&regs->ctrl);
	ctrl &= ~CP_PWRON;
	__raw_writel(ctrl, &regs->ctrl);
	pr_info("%s: write(0x%x)\n", __func__, regs->ctrl);

	return 0;
}

int exynos_pmu_cp_on(void)
{
	struct pmu_cp_regs *regs = pcdata.regs;
	u32 ctrl;

	ctrl = __raw_readl(&regs->ctrl);
	if (ctrl & CP_PWRON) {
		pr_err("%s: CP aleady on\n", __func__);
		/* print log */
		return -EINVAL;
	}
	ctrl |= (CP_PWRON | CP_START);
	__raw_writel(ctrl, &regs->ctrl);
	pr_info("%s: write(0x%x)\n", __func__, regs->ctrl);

	return 0;
}

int exynos_pmu_get_cp_on_status(void)
{
	struct pmu_cp_regs *regs = pcdata.regs;
	u32 ctrl;

	ctrl = __raw_readl(&regs->ctrl);
	pr_info("%s: read(0x%x)\n", __func__, regs->ctrl);
	return !!(ctrl & CP_PWRON);
}

static void _exynos_sys_cp_powerdown(void)
{
	struct pmu_cp_reset_regs *regs = pcdata.reset;

	pr_info("%s:\n", __func__);
	__raw_writel(0, &regs->logic_reset_cp);
	__raw_writel(0, &regs->reset_asb_cp);
	__raw_writel(0, &regs->tcxo_gate);
	__raw_writel(0, &regs->cleany_bus);

	__raw_writel(0, EXYNOS_PMU_CENTRAL_SEQ_CP_CONFIGURATION);
	__raw_writel(0, EXYNOS_PMU_OSCCLK_GATE_CPU_SYS_PWR_REG);
}

int exynos_pmu_cp_clear_active(void)
{
	struct pmu_cp_regs *regs = pcdata.regs;
	u32 ctrl;

	ctrl = __raw_readl(&regs->ctrl);
	ctrl |= CP_ACTIVE_REQ_CLR;
	__raw_writel(ctrl, &regs->ctrl);

	pr_info("%s: cp_ctrl [0x%08x] -> [0x%08x]\n", __func__,
		ctrl, __raw_readl(&regs->ctrl));

	return 0;
}

int exynos_pmu_cp_clear_reset(void)
{
	struct pmu_cp_regs *regs = pcdata.regs;
	u32 ctrl;

	ctrl = __raw_readl(&regs->ctrl);
	ctrl |= CP_RESET_REQ_CLR;
	__raw_writel(ctrl, &regs->ctrl);

	pr_info("%s: cp_ctrl [0x%08x] -> [0x%08x]\n", __func__,
		ctrl, __raw_readl(&regs->ctrl));

	return 0;
}

int exynos_pmu_cp_reset(void)
{
	struct pmu_cp_regs *regs = pcdata.regs;
	u32 ctrl;

	_exynos_sys_cp_powerdown();

	ctrl = __raw_readl(&regs->ctrl);
	ctrl &= ~(CP_START | CP_PWRON);
	__raw_writel(ctrl, &regs->ctrl);

	cpu_relax();
	usleep_range(80, 100);

	return 0;
}

int exynos_pmu_cp_release(void)
{
	struct pmu_cp_regs *regs = pcdata.regs;
	u32 cp_ctrl;

	pr_info("%s\n", __func__);

	cp_ctrl = __raw_readl(&regs->ctrl);
	cp_ctrl |= (CP_START | CP_PWRON);
	__raw_writel(cp_ctrl, &regs->ctrl);
	pr_info("%s, cp_ctrl[0x%08x] -> [0x%08x]\n", __func__, cp_ctrl,
		__raw_readl(&regs->ctrl));

	return 0;
}

#define CP_SECURE_BOOT
#ifdef CP_SECURE_BOOT
static void config_cp_memory_cfg(struct pmu_cp_data *pmu_cp) {}
#else
#define CP_MEM_SIZE_OFFSET 0 /* 8.8.1.1.11 CP2AP_MEM_CONFIG */
#define CP_MEM_BASE_OFFSET 16
#define PMU_ALIVE 24 /* 8.8.1.1.12 CPA2AP_MIF_ACCESS_WIN0 */
#define PMU_MIF 16
#define CMU_MIF 12
#define DDRPHY0 8
#define DREX0 0
#define HSI2C_CP 20 /* 8.8.1.1.13 CP2AP_MIF_ACCESS_WIN1 */
#define HSI2C_AP 16
static void config_cp_memory_cfg(struct pmu_cp_data *pmu_cp)
{
	u32 value, address, size;

	pr_info("%s: CP non-Secure boot!!\n", __func__);

	address = (pmu_cp->smem_base >> 22);
	size = (pmu_cp->smem_size >> 2); /* size / 4 */

	/* config shmem base address and size */
	value = (address << CP_MEM_SIZE_OFFSET)
		| (size << CP_MEM_BASE_OFFSET);
	__raw_writel(value, &pmu_cp->regs->mem_cfg);

	/* set cp memory access window */
	value = __raw_readl(&pmu_cp->regs->mif_access_win0);
	value |= (0xc << PMU_ALIVE) | (0xc << PMU_MIF) | (0xc << DDRPHY0)
			| (0xc << DREX0);
	__raw_writel(value, &pmu_cp->regs->mif_access_win0);

	value = __raw_readl(&pmu_cp->regs->mif_access_win1);
	value |= (0xc << HSI2C_CP) | (0xc << HSI2C_AP);
	__raw_writel(value, &pmu_cp->regs->mif_access_win1);
}
#endif

void pmu_cp_reg_dump(void)
{
	struct pmu_cp_regs *regs = pcdata.regs;
#if 1 /* To do: will be removed */
	u32 win = 0;

	win = 0x4c444444;
	__raw_writel(win, &regs->mif_access_win0);
	win = 0x4cc44444;
	__raw_writel(win, &regs->mif_access_win1);
	win = 0x44034444;
	__raw_writel(win, &regs->mif_access_win2);
#endif

	pr_info("---------- PMU CP RegDump ----------\n");
	PR_REG("CP_CTRL", &regs->ctrl);
	PR_REG("CP_STAT", &regs->stat);
	PR_REG("CP_DEBUG", &regs->debug);

	PR_REG("CP_SEMAPHORE", &regs->semaphore);

	PR_REG("CP2AP_MEM_CONFIG", &regs->mem_cfg);
	PR_REG("CP2AP_MIF_ACCESS_WIN0", &regs->mif_access_win0);
	PR_REG("CP2AP_MIF_ACCESS_WIN1", &regs->mif_access_win1);
	PR_REG("CP2AP_MIF_ACCESS_WIN2", &regs->mif_access_win2);
	PR_REG("CP2AP_PERI_ACCESS_WIN", &regs->peri_access_win);

	PR_REG("CP_BOOT_TEST_RST_CONFIG", &regs->boot_rst_cfg);
	PR_REG("MIF_TZPC_CFG", &regs->mif_tzpc_cfg);
	PR_REG("AUD_PATH_CFG", &regs->aud_path_cfg);
	PR_REG("CP_MIF_CLKCTRL", &regs->mif_clkctrl);
}

#define PMU_CP_CTRL_OFFSET (0x0030)
#define PMU_CP_LOGIC_RESET_OFFSET (0x11F0)
static int pmu_cp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct resource *res;
	struct pmu_cp_data *pmu_cp = &pcdata;
	u32 cp_ctrl;
	char *pmu_alive;
	unsigned cp_rst_n;
	int ret;

	pr_info("%s:\n", __func__);

	if (np) {
		/* GPIO_CP_RST_N */
		cp_rst_n = of_get_named_gpio(np, "pmu_cp,gpio_cp_rst_n", 0);
		if (!gpio_is_valid(cp_rst_n)) {
			dev_err(dev, "cp_rst_n: Invalied gpio pins\n");
			return -EINVAL;
		}

		dev_err(dev, "cp_rst_n: %d\n", cp_rst_n);
		ret = gpio_request(cp_rst_n, "CP_RST_N");
		if (ret)
			dev_err(dev, "fail req gpio %s:%d\n", "CP_RST_N", ret);
		gpio_direction_output(cp_rst_n, 1);
	} else {
		dev_err(dev, "cannot find device_tree for pmu_cu!\n");
		return 0;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Failed to get I/O memory\n");
		return -ENXIO;
	}

	pmu_alive = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!pmu_alive) {
		dev_err(&pdev->dev, "Failed to remap I/O memory\n");
		return -ENOMEM;
	}
	pmu_cp->regs = (struct pmu_cp_regs *)(pmu_alive + PMU_CP_CTRL_OFFSET);
	pmu_cp->reset =	(struct pmu_cp_reset_regs *)
					(pmu_alive + PMU_CP_LOGIC_RESET_OFFSET);

	platform_set_drvdata(pdev, pmu_cp);

	cp_ctrl = __raw_readl(&pmu_cp->regs->ctrl);
	cp_ctrl |= MASK_CP_PWRDN_DONE;
	__raw_writel(cp_ctrl, &pmu_cp->regs->ctrl);

	config_cp_memory_cfg(pmu_cp);

	return 0;
}

static void pmu_cp_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	pr_info("%s:\n", __func__);

	if (!dev->of_node)
		return;
}

static const struct of_device_id pmu_cp_match[] = {
	{ .compatible = "samsung,exynos7580-pmu_cp", },
	{},
};
MODULE_DEVICE_TABLE(of, pmu_cp_match);

static struct platform_driver pmu_cp_driver = {
	.probe = pmu_cp_probe,
	.shutdown = pmu_cp_shutdown,
	.driver = {
		.name = "exynos7-pmu_cp",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(pmu_cp_match),
	},
};
module_platform_driver(pmu_cp_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Samsung CP PMU Driver");
