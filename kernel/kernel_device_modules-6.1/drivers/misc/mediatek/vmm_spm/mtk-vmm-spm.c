// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Yuan Jung Kuo <yuan-jung.kuo@mediatek.com>
 */

#include <linux/io.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/irqflags.h>

#define ISPDVFS_DBG
#ifdef ISPDVFS_DBG
#define ISP_LOGD(fmt, args...) \
	do { \
		if (mtk_ispdvfs_dbg_level) \
			pr_notice("[ISPDVFS] %s(): " fmt "\n",\
				__func__, ##args); \
	} while (0)
#else
#define ISPDVFS_DBG(fmt, args...)
#endif
#define ISP_LOGI(fmt, args...) \
	pr_notice("[ISPDVFS] %s(): " fmt "\n", \
		__func__, ##args)
#define ISP_LOGE(fmt, args...) \
	pr_notice("[ISPDVFS] error %s(),%d: " fmt "\n", \
		__func__, __LINE__, ##args)
#define ISP_LOGF(fmt, args...) \
	pr_notice("[ISPDVFS] fatal %s(),%d: " fmt "\n", \
		__func__, __LINE__, ##args)

#define SPM_CHECK_IDX(idx, max) ((idx) >= (max) ? 1 : 0)

#define SPM_VMM_HW_SEM_REG_OFST 0x6A8
#define SPM_VMM_ISO_REG_OFST 0xF30
#define SPM_VMM_ISO_REG_OFST_MT6886 0xF74
#define SPM_VMM_EXT_BUCK_ISO_BIT 16
#define SPM_AOC_VMM_SRAM_ISO_DIN_BIT 17
#define SPM_AOC_VMM_SRAM_LATCH_ENB 18
#define SPM_HW_SEM_TIMEOUT_RUN 5000
#define SPM_HW_SEM_SET_DELAY_US 10

#define SPM_VMM_HW_SEM_REG_OFST_M0 0x69C
#define SPM_VMM_HW_SEM_REG_OFST_M1 0x6A0
#define SPM_VMM_HW_SEM_REG_OFST_M2 0x6A4
#define SPM_VMM_HW_SEM_REG_OFST_M3 0x6A8
#define SPM_VMM_HW_SEM_REG_OFST_M4 0x6AC
#define SPM_VMM_HW_SEM_REG_OFST_M5 0x6B0
#define SPM_VMM_HW_SEM_REG_OFST_M6 0x6B4
#define SPM_VMM_HW_SEM_REG_OFST_M7 0x6B8

/* Buck ISO control for 7s */
#define SPM_VMM_EXT_BUCK_ISO_BIT_7S 20
#define SPM_AOC_VMM_SRAM_ISO_DIN_BIT_7S 21
#define SPM_AOC_VMM_SRAM_LATCH_ENB_7S 22
#define SOC_BUCK_ISO_CON_SET 0xF7C
#define SOC_BUCK_ISO_CON_CLR 0xF80

static unsigned int spm_ver;

enum {
	VMM_SPM_DEFAULT = 0,
	VMM_SPM_MT6886,
	VMM_SPM_7S,
	VMM_SPM_7SP,
	VMM_SPM_MT6878,

	VMM_SPM_MAX,
};

struct vmm_spm_reg_info {
	unsigned int iso_con_status;
	unsigned int iso_con_set;
	unsigned int iso_con_clr;
	unsigned int ext_iso_bit;
	unsigned int iso_din_bit;
	unsigned int latch_en_bit;
};

struct vmm_spm_drv_data {
	void __iomem *spm_reg;
	struct notifier_block nb;
};

static struct vmm_spm_drv_data g_drv_data;

static const struct vmm_spm_reg_info spm_reg_tbl[VMM_SPM_MAX] = {
	[VMM_SPM_7S] = {
		.iso_con_status = 0xF78,
		.iso_con_set = 0xF7C,
		.iso_con_clr = 0xF80,
		.ext_iso_bit = 20,
		.iso_din_bit = 21,
		.latch_en_bit = 22,
	},
	[VMM_SPM_7SP] = {
		.iso_con_status = 0xFA4,
		.iso_con_set = 0xFA8,
		.iso_con_clr = 0xFAC,
		.ext_iso_bit = 20,
		.iso_din_bit = 21,
		.latch_en_bit = 22,
	},
	[VMM_SPM_MT6878] = {
		.iso_con_status = 0xF28,
		.iso_con_set = 0xF2C,
		.iso_con_clr = 0xF30,
		.ext_iso_bit = 24,
		.iso_din_bit = 25,
		.latch_en_bit = 26,
	}
};

static void vmm_dump_spm_sem_reg(void __iomem *base)
{
	ISP_LOGE("SPM_VMM_HW_SEM_REG_OFST_M0 0x(%x)\n",
			readl_relaxed(base + SPM_VMM_HW_SEM_REG_OFST_M0));
	ISP_LOGE("SPM_VMM_HW_SEM_REG_OFST_M1 0x(%x)\n",
			readl_relaxed(base + SPM_VMM_HW_SEM_REG_OFST_M1));
	ISP_LOGE("SPM_VMM_HW_SEM_REG_OFST_M2 0x(%x)\n",
			readl_relaxed(base + SPM_VMM_HW_SEM_REG_OFST_M2));
	ISP_LOGE("SPM_VMM_HW_SEM_REG_OFST_M3 0x(%x)\n",
			readl_relaxed(base + SPM_VMM_HW_SEM_REG_OFST_M3));
	ISP_LOGE("SPM_VMM_HW_SEM_REG_OFST_M4 0x(%x)\n",
			readl_relaxed(base + SPM_VMM_HW_SEM_REG_OFST_M4));
	ISP_LOGE("SPM_VMM_HW_SEM_REG_OFST_M5 0x(%x)\n",
			readl_relaxed(base + SPM_VMM_HW_SEM_REG_OFST_M5));
	ISP_LOGE("SPM_VMM_HW_SEM_REG_OFST_M6 0x(%x)\n",
			readl_relaxed(base + SPM_VMM_HW_SEM_REG_OFST_M6));
	ISP_LOGE("SPM_VMM_HW_SEM_REG_OFST_M7 0x(%x)\n",
			readl_relaxed(base + SPM_VMM_HW_SEM_REG_OFST_M7));
}

static int acquire_hw_semaphore(void __iomem *hw_sem_addr, unsigned long *pFlags)
{
	u32 hw_sem;
	int i = 1;
	unsigned long flags;

	ISP_LOGI("VMM will try to acquire hw sem");

	for (;;) {
		local_irq_save(flags);
		hw_sem = readl_relaxed(hw_sem_addr);
		hw_sem |= 0x1;
		writel_relaxed(hw_sem, hw_sem_addr);

		udelay(SPM_HW_SEM_SET_DELAY_US);

		/* *hw_sem_addr == 0x1 means acquire succeed */
		if (readl_relaxed(hw_sem_addr) & 0x1)
			break;

		local_irq_restore(flags);

		if (i++ > SPM_HW_SEM_TIMEOUT_RUN) {
			ISP_LOGE("Acquire HW SEM timeout");
			return -ETIMEDOUT;
		}
	}

	*pFlags = flags;

	return 0;
}

static void release_hw_semaphore(void __iomem *hw_sem_addr, unsigned long *pFlags)
{
	u32 hw_sem;

	hw_sem = readl_relaxed(hw_sem_addr);
	hw_sem |= 0x1;
	writel_relaxed(hw_sem, hw_sem_addr);

	udelay(SPM_HW_SEM_SET_DELAY_US);
	if (readl_relaxed(hw_sem_addr) & 0x1)
		goto error_out;

	local_irq_restore(*pFlags);

	return;

error_out:
	local_irq_restore(*pFlags);
	ISP_LOGE("Release HW SEM FAIL");
	BUG_ON(1);
}

static void vmm_buck_isolation_off(void __iomem *base)
{
	void __iomem *reg_buck_iso_addr = base + SPM_VMM_ISO_REG_OFST;
	void __iomem *hw_sem_addr = base + SPM_VMM_HW_SEM_REG_OFST;
	u32 reg_buck_iso_val;
	unsigned long flags;

	if (acquire_hw_semaphore(hw_sem_addr, &flags)) {
		vmm_dump_spm_sem_reg(base);
		BUG_ON(1);
		return;
	}

	reg_buck_iso_val = readl_relaxed(reg_buck_iso_addr);
	reg_buck_iso_val &= ~(1UL << SPM_VMM_EXT_BUCK_ISO_BIT);
	writel_relaxed(reg_buck_iso_val, reg_buck_iso_addr);

	reg_buck_iso_val = readl_relaxed(reg_buck_iso_addr);
	reg_buck_iso_val &= ~(1UL << SPM_AOC_VMM_SRAM_ISO_DIN_BIT);
	writel_relaxed(reg_buck_iso_val, reg_buck_iso_addr);

	reg_buck_iso_val = readl_relaxed(reg_buck_iso_addr);
	reg_buck_iso_val &= ~(1UL << SPM_AOC_VMM_SRAM_LATCH_ENB);
	writel_relaxed(reg_buck_iso_val, reg_buck_iso_addr);

	release_hw_semaphore(hw_sem_addr, &flags);
}

static void vmm_buck_isolation_on(void __iomem *base)
{
	void __iomem *reg_buck_iso_addr = base + SPM_VMM_ISO_REG_OFST;
	void __iomem *hw_sem_addr = base + SPM_VMM_HW_SEM_REG_OFST;
	u32 reg_buck_iso_val;
	unsigned long flags;

	if (acquire_hw_semaphore(hw_sem_addr, &flags)) {
		vmm_dump_spm_sem_reg(base);
		BUG_ON(1);
		return;
	}

	reg_buck_iso_val = readl_relaxed(reg_buck_iso_addr);
	reg_buck_iso_val |= (1UL << SPM_AOC_VMM_SRAM_LATCH_ENB);
	writel_relaxed(reg_buck_iso_val, reg_buck_iso_addr);

	reg_buck_iso_val = readl_relaxed(reg_buck_iso_addr);
	reg_buck_iso_val |= (1UL << SPM_AOC_VMM_SRAM_ISO_DIN_BIT);
	writel_relaxed(reg_buck_iso_val, reg_buck_iso_addr);

	reg_buck_iso_val = readl_relaxed(reg_buck_iso_addr);
	reg_buck_iso_val |= (1UL << SPM_VMM_EXT_BUCK_ISO_BIT);
	writel_relaxed(reg_buck_iso_val, reg_buck_iso_addr);

	release_hw_semaphore(hw_sem_addr, &flags);
}

static void vmm_buck_isolation_off_mt6886(void __iomem *base)
{
	void __iomem *reg_buck_iso_addr = base + SPM_VMM_ISO_REG_OFST_MT6886;
	void __iomem *hw_sem_addr = base + SPM_VMM_HW_SEM_REG_OFST;
	u32 reg_buck_iso_val;
	unsigned long flags;

	if (acquire_hw_semaphore(hw_sem_addr, &flags)) {
		vmm_dump_spm_sem_reg(base);
		BUG_ON(1);
		return;
	}

	reg_buck_iso_val = readl_relaxed(reg_buck_iso_addr);
	reg_buck_iso_val &= ~(1UL << SPM_VMM_EXT_BUCK_ISO_BIT_7S);
	writel_relaxed(reg_buck_iso_val, reg_buck_iso_addr);

	reg_buck_iso_val = readl_relaxed(reg_buck_iso_addr);
	reg_buck_iso_val &= ~(1UL << SPM_AOC_VMM_SRAM_ISO_DIN_BIT_7S);
	writel_relaxed(reg_buck_iso_val, reg_buck_iso_addr);

	reg_buck_iso_val = readl_relaxed(reg_buck_iso_addr);
	reg_buck_iso_val &= ~(1UL << SPM_AOC_VMM_SRAM_LATCH_ENB_7S);
	writel_relaxed(reg_buck_iso_val, reg_buck_iso_addr);

	release_hw_semaphore(hw_sem_addr, &flags);
}

static void vmm_buck_isolation_on_mt6886(void __iomem *base)
{
	void __iomem *reg_buck_iso_addr = base + SPM_VMM_ISO_REG_OFST_MT6886;
	void __iomem *hw_sem_addr = base + SPM_VMM_HW_SEM_REG_OFST;
	u32 reg_buck_iso_val;
	unsigned long flags;

	if (acquire_hw_semaphore(hw_sem_addr, &flags)) {
		vmm_dump_spm_sem_reg(base);
		BUG_ON(1);
		return;
	}

	reg_buck_iso_val = readl_relaxed(reg_buck_iso_addr);
	reg_buck_iso_val |= (1UL << SPM_AOC_VMM_SRAM_LATCH_ENB_7S);
	writel_relaxed(reg_buck_iso_val, reg_buck_iso_addr);

	reg_buck_iso_val = readl_relaxed(reg_buck_iso_addr);
	reg_buck_iso_val |= (1UL << SPM_AOC_VMM_SRAM_ISO_DIN_BIT_7S);
	writel_relaxed(reg_buck_iso_val, reg_buck_iso_addr);

	reg_buck_iso_val = readl_relaxed(reg_buck_iso_addr);
	reg_buck_iso_val |= (1UL << SPM_VMM_EXT_BUCK_ISO_BIT_7S);
	writel_relaxed(reg_buck_iso_val, reg_buck_iso_addr);

	release_hw_semaphore(hw_sem_addr, &flags);
}

static void vmm_buck_isolation_off_general(void __iomem *base)
{
	void __iomem *addr;

	if (SPM_CHECK_IDX(spm_ver, VMM_SPM_MAX)) {
		ISP_LOGE("[fatal]VMM spm version[cur=%u/max=%u] error!\n",
				spm_ver, VMM_SPM_MAX);
		return;
	}

	addr = base + spm_reg_tbl[spm_ver].iso_con_clr;
	writel_relaxed((1 << spm_reg_tbl[spm_ver].ext_iso_bit), addr);
	writel_relaxed((1 << spm_reg_tbl[spm_ver].iso_din_bit), addr);
	writel_relaxed((1 << spm_reg_tbl[spm_ver].latch_en_bit), addr);
}

static void vmm_buck_isolation_on_general(void __iomem *base)
{
	void __iomem *addr;

	if (SPM_CHECK_IDX(spm_ver, VMM_SPM_MAX)) {
		ISP_LOGE("[fatal]VMM spm version[cur=%u/max=%u] error!\n",
				spm_ver, VMM_SPM_MAX);
		return;
	}

	addr = base + spm_reg_tbl[spm_ver].iso_con_set;
	writel_relaxed((1 << spm_reg_tbl[spm_ver].latch_en_bit), addr);
	writel_relaxed((1 << spm_reg_tbl[spm_ver].iso_din_bit), addr);
	writel_relaxed((1 << spm_reg_tbl[spm_ver].ext_iso_bit), addr);
}

static void vmm_buck_isolation_check_reg(void __iomem *base, bool iso_on)
{
	void __iomem *addr;
	u32 iso_val;

	if (SPM_CHECK_IDX(spm_ver, VMM_SPM_MAX)) {
		ISP_LOGE("[fatal]VMM spm version[cur=%u/max=%u] error!\n",
				spm_ver, VMM_SPM_MAX);
		return;
	}

	addr = base + spm_reg_tbl[spm_ver].iso_con_status;
	iso_val = readl_relaxed(addr);
	if (iso_on) {
		if (((iso_val & BIT(spm_reg_tbl[spm_ver].latch_en_bit))
			!= BIT(spm_reg_tbl[spm_ver].latch_en_bit)) ||
			((iso_val & BIT(spm_reg_tbl[spm_ver].iso_din_bit))
			!= BIT(spm_reg_tbl[spm_ver].iso_din_bit)) ||
			((iso_val & BIT(spm_reg_tbl[spm_ver].ext_iso_bit))
			!= BIT(spm_reg_tbl[spm_ver].ext_iso_bit))) {
			ISP_LOGE("ISO on error! iso_val[0x%x] = %u, spm_ver=%d\n",
				spm_reg_tbl[spm_ver].iso_con_status, iso_val, spm_ver);
			panic("VMM ISO set on wrong!");
		}
	} else {
		if (((iso_val & BIT(spm_reg_tbl[spm_ver].latch_en_bit)) != 0) ||
			((iso_val & BIT(spm_reg_tbl[spm_ver].iso_din_bit)) != 0) ||
			((iso_val & BIT(spm_reg_tbl[spm_ver].ext_iso_bit)) != 0)) {
			ISP_LOGE("ISO off_error!!!!!!!!!! iso_val[0x%x] = %u, spm_ver=%d\n",
				spm_reg_tbl[spm_ver].iso_con_status, iso_val, spm_ver);
			panic("VMM ISO set off wrong!");
		}
	}
}

static int regulator_event_notify(struct notifier_block *nb,
				  unsigned long event, void *data)
{
	struct vmm_spm_drv_data *drv_data = &g_drv_data;

	if (!drv_data->spm_reg) {
		ISP_LOGE("SPM_BASE is NULL");
		return -EINVAL;
	}

	if (event == REGULATOR_EVENT_ENABLE) {
		if (spm_ver == VMM_SPM_MT6886)
			vmm_buck_isolation_off_mt6886(drv_data->spm_reg);
		else if (spm_ver == VMM_SPM_DEFAULT)
			vmm_buck_isolation_off(drv_data->spm_reg);
		else {
			vmm_buck_isolation_off_general(drv_data->spm_reg);
			vmm_buck_isolation_check_reg(drv_data->spm_reg, false);
		}
		ISP_LOGI("VMM regulator enable done, ver = %u\n", spm_ver);
	} else if (event == REGULATOR_EVENT_PRE_DISABLE) {
		if (spm_ver == VMM_SPM_MT6886)
			vmm_buck_isolation_on_mt6886(drv_data->spm_reg);
		else if (spm_ver == VMM_SPM_DEFAULT)
			vmm_buck_isolation_on(drv_data->spm_reg);
		else {
			vmm_buck_isolation_on_general(drv_data->spm_reg);
			vmm_buck_isolation_check_reg(drv_data->spm_reg, true);
		}
		ISP_LOGI("VMM regulator before disable, ver = %u\n", spm_ver);
	}

	return 0;
}

static int vmm_spm_probe(struct platform_device *pdev)
{
	struct vmm_spm_drv_data *drv_data = &g_drv_data;
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct regulator *reg;
	s32 ret;

	/* SPM registers */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ISP_LOGE("fail to get resource SPM_BASE");
		return -EINVAL;
	}

	drv_data->spm_reg = devm_ioremap(dev, res->start, resource_size(res));
	if (!(drv_data->spm_reg)) {
		ISP_LOGE("fail to ioremap SPM_BASE: 0x%llx", res->start);
		return -EINVAL;
	}

	reg = devm_regulator_get(dev, "vmm-pmic");
	if (IS_ERR(reg)) {
		ISP_LOGE("devm_regulator_get vmm-pmic fail");
		return PTR_ERR(reg);
	}

	if (of_device_is_compatible(dev->of_node, "mediatek,vmm_spm_7sp"))
		spm_ver = VMM_SPM_7SP;
	else if (of_device_is_compatible(dev->of_node, "mediatek,vmm_spm_7s"))
		spm_ver = VMM_SPM_7S;
	else if (of_device_is_compatible(dev->of_node, "mediatek,vmm_spm_mt6886"))
		spm_ver = VMM_SPM_MT6886;
	else if (of_device_is_compatible(dev->of_node, "mediatek,vmm_spm_mt6878"))
		spm_ver = VMM_SPM_MT6878;
	else
		spm_ver = VMM_SPM_DEFAULT;

	drv_data->nb.notifier_call = regulator_event_notify;
	ret = devm_regulator_register_notifier(reg, &drv_data->nb);
	if (ret)
		ISP_LOGE("Failed to register notifier: %d\n", ret);

	return ret;
}

static const struct of_device_id of_vmm_spm_match_tbl[] = {
	{
		.compatible = "mediatek,vmm_spm",
	},
	{
		.compatible = "mediatek,vmm_spm_7s",
	},
	{
		.compatible = "mediatek,vmm_spm_7sp",
	},
	{
		.compatible = "mediatek,vmm_spm_mt6886",
	},
	{
		.compatible = "mediatek,vmm_spm_mt6878",
	},
	{}
};

int mtk_vmm_spm_get_data(const char *val, const struct kernel_param *kp)
{
	struct vmm_spm_drv_data *drv_data = &g_drv_data;
	void __iomem *addr;
	void __iomem *base;

	if (!drv_data->spm_reg || spm_ver >= VMM_SPM_MAX) {
		ISP_LOGE("SPM_BASE is NULL or spm_ver[%u] is wrong. Max=%u", spm_ver, VMM_SPM_MAX);
		return -EINVAL;
	}

	base = drv_data->spm_reg;

	addr = base + spm_reg_tbl[spm_ver].iso_con_status;
	ISP_LOGI("%s iso_con_status[0x%x] = 0x%x\n",
			__func__,
			spm_reg_tbl[spm_ver].iso_con_status,
			readl_relaxed(addr));
	addr = base + spm_reg_tbl[spm_ver].iso_con_set;
	ISP_LOGI("%s iso_con_set[0x%x] = 0x%x\n",
			__func__,
			spm_reg_tbl[spm_ver].iso_con_set,
			readl_relaxed(addr));
	addr = base + spm_reg_tbl[spm_ver].iso_con_clr;
	ISP_LOGI("%s iso_con_clr[0x%x] = 0x%x\n",
			__func__,
			spm_reg_tbl[spm_ver].iso_con_clr,
			readl_relaxed(addr));
	return 0;
}

static const struct kernel_param_ops vmm_spm_get_data = {
	.set = mtk_vmm_spm_get_data,
};

module_param_cb(vmm_spm_info, &vmm_spm_get_data, NULL, 0644);
MODULE_PARM_DESC(vmm_spm_info,
	"get vmm spm info");

static struct platform_driver drv_vmm_spm = {
	.probe = vmm_spm_probe,
	.driver = {
		.name = "mtk-vmm-spm",
		.of_match_table = of_vmm_spm_match_tbl,
	},
};

static int __init mtk_vmm_spm_init(void)
{
	s32 status;

	status = platform_driver_register(&drv_vmm_spm);
	if (status) {
		pr_notice("Failed to register VMM SPM driver(%d)\n", status);
		return -ENODEV;
	}
	return 0;
}

static void __exit mtk_vmm_spm_exit(void)
{
	platform_driver_unregister(&drv_vmm_spm);
}

module_init(mtk_vmm_spm_init);
module_exit(mtk_vmm_spm_exit);
MODULE_DESCRIPTION("MTK VMM SPM driver");
MODULE_AUTHOR("Yuan Jung Kuo <yuan-jung.kuo@mediatek.com>");
MODULE_LICENSE("GPL v2");
