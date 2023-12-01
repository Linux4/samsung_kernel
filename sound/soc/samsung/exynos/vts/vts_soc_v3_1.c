// SPDX-License-Identifier: GPL-2.0-or-later
/* sound/soc/samsung/vts/vts.c
 *
 * ALSA SoC - Samsung VTS driver
 *
 * Copyright (c) 2022 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <sound/samsung/vts.h>
#include <linux/firmware.h>
#include <soc/samsung/exynos-pmu-if.h>
#include <linux/delay.h>

#include "vts_soc.h"
#include "vts_soc_v3_1.h"
#include "vts_dbg.h"
#include "vts_util.h"
#include "vts_res.h"

#define LIMIT_IN_JIFFIES (msecs_to_jiffies(1000))

void vts_cpu_power(struct device *dev, bool on)
{
	vts_dev_info(dev, "%s(%d)\n", __func__, on);

	exynos_pmu_update(VTS_CPU_CONFIGURATION, VTS_CPU_LOCAL_PWR_CFG,
		on ? VTS_CPU_LOCAL_PWR_CFG : 0);
}

int vts_cpu_power_chk(struct device *dev)
{
	uint32_t status = 1;

	exynos_pmu_read(VTS_CPU_STATUS, &status);

	return status;
}

#define VTS_WFI_VAL (0x4A004)
#define VTS_WFI_MASK (0xFFFFFF)
#define VTS_WFI_CNT (20)
int vts_cpu_enable(struct device *dev, bool enable)
{
	struct vts_data *data = dev_get_drvdata(dev);
	unsigned int status = 0;
	unsigned int status_prev = 0;
	unsigned int hit_rate = 0;
	unsigned long after;

	if (data->vts_status == ABNORMAL && data->silent_reset_support) {
		vts_dev_err(dev, "%s: abnormal status, skip checking status\n", __func__);
		return 0;
	}

	after = jiffies + LIMIT_IN_JIFFIES;

	do {
		msleep(1);
		status_prev = status;
		status = readl(data->gpr_base + VTS_CM4_PC);
		if (((status & VTS_WFI_MASK) == VTS_WFI_VAL))
			hit_rate = VTS_WFI_CNT + 2;
		else
			hit_rate ++;

		vts_dev_info(dev, "%s: PC : cur:0x%x prev:0x%x\n",
				__func__, status, status_prev);
	} while (time_is_after_eq_jiffies(after) &&
			hit_rate < VTS_WFI_CNT);

	if (time_is_before_jiffies(after)) {
		vts_err("[VTS] cpu disable timeout\n");
		return -ETIME;
	}
	return 0;
}

void vts_reset_cpu(struct device *dev)
{
	vts_cpu_enable(dev, false);
	vts_cpu_power(dev, false);
	vts_cpu_power(dev, true);
}

void vts_disable_fw_ctrl(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);

	vts_dev_info(dev, "%s: enter\n", __func__);

	/* Controls by firmware should be disabled before pd_vts off. */
	writel(0x0, data->dmic_if0_base);
	writel(0x0, data->sfr_base + VTS_ENABLE_DMIC_IF);
	writel(0x0, data->dmic_ahb0_base);

	return;
}

void vts_pad_retention(bool retention)
{
	if (!retention) {
		exynos_pmu_update(PAD_RETENTION_VTS_OPTION,
			0x1 << 11, 0x1 << 11);
	}
}
EXPORT_SYMBOL(vts_pad_retention);

u32 vts_set_baaw(void __iomem *sfr_base, u64 base, u32 size)
{
	u32 aligned_size = round_up(size, SZ_4M);
	u64 aligned_base = round_down(base, aligned_size);

	/* set VTS BAAW config */
	/* ALIVE SFR */
	writel(0x04090000, sfr_base + 0x0);
	writel(0x040CFFFF, sfr_base + 0x4);
	writel(0x01180000, sfr_base + 0x8);
	writel(0x80000003, sfr_base + 0xC);

	/* CHUB SRAM */
	writel(0x040E0000, sfr_base + 0x10);
	writel(0x040E6800, sfr_base + 0x14);
	writel(0x01120000, sfr_base + 0x18);
	writel(0x80000003, sfr_base + 0x1C);

	/* DRAM */
	pr_info("[vts] %s: %d start_addr:0x%x\n", __func__, __LINE__, VTS_BAAW_BASE / VTS_BAAW_DRAM_DIV);
	pr_info("[vts] %s: %d end_addr:0x%x\n", __func__, __LINE__, (VTS_BAAW_BASE + aligned_size) / VTS_BAAW_DRAM_DIV);
	pr_info("[vts] %s: %d remapped_addr:0x%x\n", __func__, __LINE__, aligned_base / VTS_BAAW_DRAM_DIV);
	/* GUIDE: writel(0x06000000, sfr_base + 0x10); */
	writel(VTS_BAAW_BASE / VTS_BAAW_DRAM_DIV, sfr_base + 0x10);
	/* GUIDE: writel(0x0FFFFFFF, sfr_base + 0x14); */
	writel((VTS_BAAW_BASE + aligned_size) / VTS_BAAW_DRAM_DIV, sfr_base + 0x14);
	/* GUIDE: writel(0x08000000, sfr_base + 0x18); */
	writel(aligned_base / VTS_BAAW_DRAM_DIV, sfr_base + 0x18);
	writel(0x80000003, sfr_base + 0x1C);

	return base - aligned_base + VTS_BAAW_BASE;
}

int vts_acquire_sram(struct platform_device *pdev, int vts)
{
	return 0;
}
EXPORT_SYMBOL(vts_acquire_sram);

int vts_release_sram(struct platform_device *pdev, int vts)
{
	return 0;
}
EXPORT_SYMBOL(vts_release_sram);

int vts_clear_sram(struct platform_device *pdev)
{
	struct vts_data *data = platform_get_drvdata(pdev);

	vts_info("%s\n", __func__);

	memset(data->sram_base, 0, data->sram_size);

	return 0;
}
EXPORT_SYMBOL(vts_clear_sram);

static void vts_soc_complete_fw_request(
	const struct firmware *fw, void *context)
{
	struct device *dev = context;
	struct vts_data *data = dev_get_drvdata(dev);
	unsigned int *pversion = NULL;

	if (!fw) {
		vts_dev_err(dev, "Failed to request firmware\n");
		return;
	}

	data->firmware = fw;
	pversion = (unsigned int *) (fw->data + VTSFW_VERSION_OFFSET);
	/* data->vtsfw_version = *pversion; */
	pversion = (unsigned int *) (fw->data + DETLIB_VERSION_OFFSET);
	/* data->vtsdetectlib_version = *pversion; */

	vts_dev_info(dev, "firmware loaded at %p (%zu)\n", fw->data, fw->size);
}

int vts_soc_runtime_resume(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);
	int ret = 0;

	vts_dev_info(dev, "%s: enter\n", __func__);

	ret = vts_clk_set_rate(dev, data->alive_clk_path);
	if (ret < 0)
		vts_dev_warn(dev, "vts_clk_set_rate(%d)\n", ret);

	/* SRAM intmem */
	if (data->intmem_code)
		writel(0x03FF0000, data->intmem_code + 0x4);
	if (data->intmem_data)
		writel(0x03FF0000, data->intmem_data + 0x4);
	if (data->intmem_pcm)
		writel(0x03FF0000, data->intmem_pcm + 0x4);
	if (data->intmem_data1)
		writel(0x03FF0000, data->intmem_data1 + 0x4);

	if (!data->firmware) {
		vts_dev_info(dev, "%s : request_firmware_direct\n",
			__func__);
		ret = request_firmware_direct(
			(const struct firmware **)&data->firmware,
			"vts.bin", dev);

		if (ret < 0) {
			vts_dev_err(dev, "request_firmware_direct failed\n");
			return ret;
		}
		vts_dev_info(dev, "vts_soc_complete_fw_request : OK\n");
		vts_soc_complete_fw_request(data->firmware, dev);
	}

	return ret;
}

#define YAMIN_MCU_VTS_QCH_CLKIN (0x70e8)
int vts_soc_runtime_suspend(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);

#if 0
	volatile unsigned long yamin_mcu_vts_qch_clkin;
	unsigned int status = 0;

	yamin_mcu_vts_qch_clkin =
		(volatile unsigned long)ioremap_wt(0x15507000, 0x100);

	pr_info("[VTS]YAMIN QCH(0xe8) 0x%08x\n",
			readl((volatile void *)(yamin_mcu_vts_qch_clkin
					+ 0xe8)));
	iounmap((volatile void __iomem *)yamin_mcu_vts_qch_clkin);

	exynos_pmu_read(YAMIN_MCU_VTS_QCH_CLKIN, &status);
	pr_info("[VTS]YAMIN QCH(0xe8) 0x%08x\n", status);
#endif
	vts_dev_info(dev, "%s\n", __func__);

	vts_clk_disable(dev, data->sys_clk_path);
	vts_clk_disable(dev, data->tri_clk_path);
	vts_clk_disable(dev, data->alive_clk_path);

	return 0;
}

int vts_soc_cmpnt_probe(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);
	int ret;

	vts_dev_info(dev, "%s\n", __func__);

	if (!data->firmware) {
		vts_dev_info(dev, "%s : request_firmware_direct\n", __func__);
		ret = request_firmware_direct(
			(const struct firmware **)&data->firmware,
			"vts.bin", dev);

		if (ret < 0) {
			vts_dev_err(dev, "Failed to request_firmware_nowait\n");
		} else {
			vts_dev_info(dev, "vts_soc_complete_fw_request : OK\n");
			vts_soc_complete_fw_request(data->firmware, dev);
		}
	}

	vts_dev_info(dev, "%s(%d)\n", __func__, __LINE__);

	return 0;
}

int vts_soc_probe(struct device *dev)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	return ret;
}

void vts_soc_remove(struct device *dev)
{
	pr_info("%s\n", __func__);
}
