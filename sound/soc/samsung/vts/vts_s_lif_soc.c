/* sound/soc/samsung/vts/vts_s_lif.c
 *
 * ALSA SoC - Samsung VTS Serial Local Interface driver
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* #define DEBUG */
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/pm_runtime.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/wakelock.h>
#include <linux/sched/clock.h>
#include <linux/miscdevice.h>

#include <asm-generic/delay.h>

#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/vts.h>

#include <sound/samsung/mailbox.h>
#include <sound/samsung/vts.h>
#include <soc/samsung/exynos-pmu.h>
#include <soc/samsung/exynos-el3_mon.h>

#include "vts.h"
#include "vts_s_lif.h"
#include "vts_s_lif_soc.h"
#include "vts_s_lif_nm.h"
#include "vts_s_lif_clk_table.h"

#define vts_set_mask_value(id, mask, value) \
		{id = (typeof(id))((id & ~mask) | (value & mask)); }

#define vts_set_value_by_name(id, name, value) \
		vts_set_mask_value(id, name##_MASK, value << name##_L)

#define VTS_S_LIF_USE_AUD0
/* #define VTS_S_LIF_REG_LOW_DUMP */

void vts_s_lif_debug_pad_en(int en)
{
	volatile unsigned long gpio_peric1;

	gpio_peric1 =
		(volatile unsigned long)ioremap_nocache(0x10730100, 0x100);

	if (en) {
		writel(0x04440000, (volatile void *)(gpio_peric1 + 0x20));
		pr_info("gpio_peric1(0x120) 0x%08x\n",
				readl((volatile void *)(gpio_peric1 + 0x20)));
	} else {
		writel(0x00000000, (volatile void *)(gpio_peric1 + 0x20));
		pr_info("gpio_peric1(0x120) 0x%08x\n",
				readl((volatile void *)(gpio_peric1 + 0x20)));
	}
	iounmap((volatile void __iomem *)gpio_peric1);
}

#ifdef VTS_S_LIF_REG_LOW_DUMP
static void vts_s_lif_check_reg(int write_enable)
{
	volatile unsigned long rco_reg;
	volatile unsigned long gpv0_con;
	volatile unsigned long sysreg_vts;
	volatile unsigned long dmic_aud0;
	volatile unsigned long dmic_aud1;
	volatile unsigned long dmic_aud2;
	volatile unsigned long serial_lif;

	printk("%s : %d\n", __func__, __LINE__);
	/* check rco */
	rco_reg =
		(volatile unsigned long)ioremap_nocache(0x15860000, 0x1000);
	pr_info("rco_reg : 0x%8x\n",
			readl((volatile void *)(rco_reg + 0xb00)));
	iounmap((volatile void __iomem *)rco_reg);

	/* check gpv0 */
	gpv0_con =
		(volatile unsigned long)ioremap_nocache(0x15580000, 0x1000);
	pr_info("gpv0_con(0x00) 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(gpv0_con + 0x0)),
			readl((volatile void *)(gpv0_con + 0x4)),
			readl((volatile void *)(gpv0_con + 0x8)));
	pr_info("gpv0_con(0x10) 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(gpv0_con + 0x10)),
			readl((volatile void *)(gpv0_con + 0x14)),
			readl((volatile void *)(gpv0_con + 0x18)));
	pr_info("gpv0_con(0x20) 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(gpv0_con + 0x20)),
			readl((volatile void *)(gpv0_con + 0x24)),
			readl((volatile void *)(gpv0_con + 0x28)));
	iounmap((volatile void __iomem *)gpv0_con);

	/* check sysreg_vts */
	sysreg_vts =
		(volatile unsigned long)ioremap_nocache(0x15510000, 0x2000);
#if 0
	if (write_enable) {
		writel(0x1c0, (volatile void *)(sysreg_vts + 0x1000));
	}
#endif
	pr_info("sysreg_vts(0x1000) 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(sysreg_vts + 0x1000)),
			readl((volatile void *)(sysreg_vts + 0x1004)),
			readl((volatile void *)(sysreg_vts + 0x1008)),
			readl((volatile void *)(sysreg_vts + 0x100C)),
			readl((volatile void *)(sysreg_vts + 0x1010)),
			readl((volatile void *)(sysreg_vts + 0x1014)));
	pr_info("sysreg_vts(0x0108) 0x%08x\n",
			readl((volatile void *)(sysreg_vts + 0x108)));
	iounmap((volatile void __iomem *)sysreg_vts);

	/* check dmic_aud0 *//* undescribed register */
	dmic_aud0 =
		(volatile unsigned long)ioremap_nocache(0x15350000, 0x10);
	pr_info("dmic_aud0(0x0) 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x",
			readl((volatile void *)(dmic_aud0 + 0x0)),
			readl((volatile void *)(dmic_aud0 + 0x4)),
			readl((volatile void *)(dmic_aud0 + 0x8)),
			readl((volatile void *)(dmic_aud0 + 0xC)),
			readl((volatile void *)(dmic_aud0 + 0x10)));
	iounmap((volatile void __iomem *)dmic_aud0);
	/* check dmic_aud1 */
	dmic_aud1 =
		(volatile unsigned long)ioremap_nocache(0x15360000, 0x10);
	pr_info("dmic_aud1(0x1) 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x",
			readl((volatile void *)(dmic_aud1 + 0x0)),
			readl((volatile void *)(dmic_aud1 + 0x4)),
			readl((volatile void *)(dmic_aud1 + 0x8)),
			readl((volatile void *)(dmic_aud1 + 0xC)),
			readl((volatile void *)(dmic_aud1 + 0x10)));
	iounmap((volatile void __iomem *)dmic_aud1);
	/* check dmic_aud2 */
	dmic_aud2 =
		(volatile unsigned long)ioremap_nocache(0x15370000, 0x10);
	pr_info("dmic_aud2(0x2) 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x",
			readl((volatile void *)(dmic_aud2 + 0x0)),
			readl((volatile void *)(dmic_aud2 + 0x4)),
			readl((volatile void *)(dmic_aud2 + 0x8)),
			readl((volatile void *)(dmic_aud2 + 0xC)),
			readl((volatile void *)(dmic_aud2 + 0x10)));
	iounmap((volatile void __iomem *)dmic_aud2);

	/* check serial lif */
	serial_lif =
		(volatile unsigned long)ioremap_nocache(0x15340100, 0x1000);
	pr_info("s_lif(0x000) 0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(serial_lif + 0x0)),
			readl((volatile void *)(serial_lif + 0x4)),
			readl((volatile void *)(serial_lif + 0x8)),
			readl((volatile void *)(serial_lif + 0xC)));
	pr_info("s_lif(0x010) 0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(serial_lif + 0x10)),
			readl((volatile void *)(serial_lif + 0x14)),
			readl((volatile void *)(serial_lif + 0x18)),
			readl((volatile void *)(serial_lif + 0x1C)));
	pr_info("s_lif(0x020) 0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(serial_lif + 0x20)),
			readl((volatile void *)(serial_lif + 0x24)),
			readl((volatile void *)(serial_lif + 0x28)),
			readl((volatile void *)(serial_lif + 0x2C)));
	pr_info("s_lif(0x030) 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(serial_lif + 0x30)),
			readl((volatile void *)(serial_lif + 0x34)),
			readl((volatile void *)(serial_lif + 0x38)));
	pr_info("s_lif(0x050) 0x%08x \n",
			readl((volatile void *)(serial_lif + 0x50)));
	pr_info("s_lif(0x200) 0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(serial_lif + 0x200)),
			readl((volatile void *)(serial_lif + 0x204)),
			readl((volatile void *)(serial_lif + 0x208)),
			readl((volatile void *)(serial_lif + 0x20C)));
	pr_info("s_lif(0x210) 0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(serial_lif + 0x210)),
			readl((volatile void *)(serial_lif + 0x214)),
			readl((volatile void *)(serial_lif + 0x218)),
			readl((volatile void *)(serial_lif + 0x21C)));
	pr_info("s_lif(0x220) 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(serial_lif + 0x220)),
			readl((volatile void *)(serial_lif + 0x224)),
			readl((volatile void *)(serial_lif + 0x228)));
	iounmap((volatile void __iomem *)serial_lif);
}
#endif

static u32 vts_s_lif_direct_readl(const volatile void __iomem *addr)
{
	u32 ret = readl(addr);

	return ret;
}

static void vts_s_lif_direct_writel(u32 b, volatile void __iomem *addr)
{
	writel(b, addr);
}

/* private functions */
static void vts_s_lif_soc_reset_status(struct vts_s_lif_data *data)
{
	/* set_bit(0, &data->mode); */
	data->enabled = 0;
	data->running = 0;
	/* data->state = 0; */
	/* data->fmt = -1 */;
}

static void vts_s_lif_soc_set_default_gain(struct vts_s_lif_data *data)
{
	data->gain_mode[0] =
		data->gain_mode[1] =
		data->gain_mode[2] = VTS_S_DEFAULT_GAIN_MODE;

	data->max_scale_gain[0] =
		data->max_scale_gain[1] =
		data->max_scale_gain[2] = VTS_S_DEFAULT_MAX_SCALE_GAIN;

	data->control_dmic_aud[0] =
		data->control_dmic_aud[1] =
		data->control_dmic_aud[2] = VTS_S_DEFAULT_CONTROL_DMIC_AUD;

	vts_s_lif_soc_dmic_aud_gain_mode_write(data, 0);
	vts_s_lif_soc_dmic_aud_gain_mode_write(data, 1);
	vts_s_lif_soc_dmic_aud_gain_mode_write(data, 2);
	vts_s_lif_soc_dmic_aud_max_scale_gain_write(data, 0);
	vts_s_lif_soc_dmic_aud_max_scale_gain_write(data, 1);
	vts_s_lif_soc_dmic_aud_max_scale_gain_write(data, 2);
	vts_s_lif_soc_dmic_aud_control_gain_write(data, 0);
	vts_s_lif_soc_dmic_aud_control_gain_write(data, 1);
	vts_s_lif_soc_dmic_aud_control_gain_write(data, 2);
}

static void vts_s_lif_soc_set_dmic_aud(struct vts_s_lif_data *data, int enable)
{
	if (enable) {
#if (VTS_S_SOC_VERSION(1, 0, 0) == CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
		vts_s_lif_direct_writel(0x80030000, data->dmic_aud[0] + 0x0);
		vts_s_lif_direct_writel(0x80030000, data->dmic_aud[1] + 0x0);
		vts_s_lif_direct_writel(0x80030000, data->dmic_aud[2] + 0x0);
#elif (VTS_S_SOC_VERSION(1, 1, 1) >= CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
		vts_s_lif_direct_writel(0xA0030000, data->dmic_aud[0] + 0x0);
		vts_s_lif_direct_writel(0xA0030000, data->dmic_aud[1] + 0x0);
		vts_s_lif_direct_writel(0xA0030000, data->dmic_aud[2] + 0x0);
#else
#error "VTS_S_SOC_VERSION is not defined"
#endif
	} else {
	}
}

static void vts_s_lif_soc_set_sel_pad(struct vts_s_lif_data *data, int enable)
{
	struct device *dev = data->dev;
	unsigned int ctrl;

	if (enable) {
		vts_s_lif_direct_writel(0x7, data->sfr_sys_base +
			VTS_S_SEL_PAD_AUD_BASE);
		ctrl = vts_s_lif_direct_readl(data->sfr_sys_base +
				VTS_S_SEL_PAD_AUD_BASE);
		dev_info(dev, "SEL_PAD_AUD(0x%08x)\n", ctrl);

		vts_s_lif_direct_writel(0x38, data->sfr_sys_base +
			VTS_S_SEL_DIV2_CLK_BASE);
		ctrl = vts_s_lif_direct_readl(data->sfr_sys_base +
				VTS_S_SEL_DIV2_CLK_BASE);
		dev_info(dev, "SEL_DIV2_CLK(0x%08x)\n", ctrl);
	} else {
		vts_s_lif_direct_writel(0, data->sfr_sys_base +
			VTS_S_SEL_PAD_AUD_BASE);
		ctrl = vts_s_lif_direct_readl(data->sfr_sys_base +
				VTS_S_SEL_PAD_AUD_BASE);
		dev_info(dev, "SEL_PAD_AUD(0x%08x)\n", ctrl);

		vts_s_lif_direct_writel(0, data->sfr_sys_base +
			VTS_S_SEL_DIV2_CLK_BASE);
		ctrl = vts_s_lif_direct_readl(data->sfr_sys_base +
				VTS_S_SEL_DIV2_CLK_BASE);
		dev_info(dev, "SEL_DIV2_CLK(0x%08x)\n", ctrl);
	}
}

/* public functions */
int vts_s_lif_soc_dmic_aud_gain_mode_write(struct vts_s_lif_data *data,
		unsigned int id)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	int ret = 0;

	ret = regmap_write(regmap,
			VTS_S_SFR_GAIN_MODE_BASE,
			data->gain_mode[id]);
	if (ret < 0)
		dev_err(dev, "Failed to write GAIN_MODE(%d): %d\n",
			id, data->gain_mode[id]);

	return ret;
}

int vts_s_lif_soc_dmic_aud_gain_mode_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	int ret = 0;

	ret = regmap_read(regmap,
			VTS_S_SFR_GAIN_MODE_BASE,
			&data->gain_mode[id]);

	if (ret < 0)
		dev_err(dev, "Failed to get GAIN_MODE(%d): %d\n",
			id, data->gain_mode[id]);

	*val = data->gain_mode[id];

	return ret;
}

int vts_s_lif_soc_dmic_aud_gain_mode_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val)
{
	int ret = 0;

	data->gain_mode[id] = val;
	ret = vts_s_lif_soc_dmic_aud_gain_mode_write(data, id);

	return ret;
}


int vts_s_lif_soc_dmic_aud_max_scale_gain_write(struct vts_s_lif_data *data,
		unsigned int id)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	int ret = 0;

	ret = regmap_write(regmap,
			VTS_S_SFR_MAX_SCALE_GAIN_BASE,
			data->max_scale_gain[id]);
	if (ret < 0)
		dev_err(dev, "Failed to write MAX_SCALE_GAIN(%d): %d\n",
				id, data->max_scale_gain[id]);

	return ret;
}

int vts_s_lif_soc_dmic_aud_max_scale_gain_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	int ret = 0;

	ret = regmap_read(regmap,
			VTS_S_SFR_MAX_SCALE_GAIN_BASE,
			&data->max_scale_gain[id]);

	if (ret < 0)
		dev_err(dev, "Failed to get MAX_SCALE_GAIN(%d): %d\n",
				id, data->max_scale_gain[id]);

	*val = data->max_scale_gain[id];

	return ret;
}

int vts_s_lif_soc_dmic_aud_max_scale_gain_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val)
{
	int ret = 0;

	data->max_scale_gain[id] = val;
	ret = vts_s_lif_soc_dmic_aud_max_scale_gain_write(data, id);

	return ret;
}

int vts_s_lif_soc_dmic_aud_control_gain_write(struct vts_s_lif_data *data,
		unsigned int id)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	int ret = 0;

	ret = regmap_write(regmap,
			VTS_S_SFR_CONTROL_DMIC_AUD_BASE,
			data->control_dmic_aud[id]);

	if (ret < 0)
		dev_err(dev, "Failed to write CONTROL_DMIC_AUD(%d): %d\n",
				id, data->control_dmic_aud[id]);

	return ret;
}

int vts_s_lif_soc_dmic_aud_control_gain_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	unsigned int mask = 0;
	int ret = 0;

	ret = regmap_read(regmap,
			VTS_S_SFR_CONTROL_DMIC_AUD_BASE,
			&data->control_dmic_aud[id]);

	if (ret < 0)
		dev_err(dev, "Failed to get CONTROL_DMIC_AUD(%d): %d\n",
				id, data->control_dmic_aud[id]);

	mask = VTS_S_SFR_CONTROL_DMIC_AUD_GAIN_MASK;
	*val = ((data->control_dmic_aud[id] & mask) >>
			VTS_S_SFR_CONTROL_DMIC_AUD_GAIN_L);

	dev_dbg(dev, "%d mask 0x%x\n", __LINE__, mask);
	dev_dbg(dev, "%d dmic aud[%d] 0x%x \n", __LINE__,
			id, data->control_dmic_aud[id]);
	dev_dbg(dev, "%d val 0x%x \n", __LINE__, *val);

	return ret;
}

int vts_s_lif_soc_dmic_aud_control_gain_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val)
{
	struct device *dev = data->dev;
	unsigned int mask = 0;
	unsigned int value = 0;
	int ret = 0;

	mask = VTS_S_SFR_CONTROL_DMIC_AUD_GAIN_MASK;
	value = (val << VTS_S_SFR_CONTROL_DMIC_AUD_GAIN_L) & mask;
	data->control_dmic_aud[id] &= ~mask;
	data->control_dmic_aud[id] |= value;

	dev_dbg(dev, "%d mask 0x%x val 0x%x value 0x%x\n", __LINE__,
			mask, val, value);
	dev_dbg(dev, "%d dmic aud[%d] 0x%x \n", __LINE__,
			id, data->control_dmic_aud[id]);

	ret = vts_s_lif_soc_dmic_aud_control_gain_write(data, id);

	return ret;
}

int vts_s_lif_soc_vol_set_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl;
	unsigned int volumes;
	int ret = 0;

	ret = snd_soc_component_read(cmpnt, VTS_S_VOL_SET(id), &ctrl);
	if (ret < 0)
		dev_err(dev, "%s failed(%d): %d\n", __func__, __LINE__, ret);

	volumes = (ctrl & VTS_S_VOL_SET_MASK);

	dev_dbg(dev, "%s(0x%08x, %u)\n", __func__, id, volumes);

	*val = volumes;

	return ret;
}

int vts_s_lif_soc_vol_set_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	int ret = 0;

	ret = snd_soc_component_update_bits(cmpnt,
			VTS_S_VOL_SET(id),
			VTS_S_VOL_SET_MASK,
			val << VTS_S_VOL_SET_L);
	if (ret < 0)
		dev_err(dev, "%s failed(%d): %d\n",
			__func__, __LINE__, ret);

	dev_dbg(dev, "%s(0x%08x, %u)\n", __func__, id, val);

	return ret;
}

int vts_s_lif_soc_vol_change_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl;
	unsigned int volumes;
	int ret = 0;

	ret = snd_soc_component_read(cmpnt, VTS_S_VOL_CHANGE(id), &ctrl);
	if (ret < 0)
		dev_err(dev, "%s failed(%d): %d\n", __func__, __LINE__, ret);

	volumes = (ctrl & VTS_S_VOL_CHANGE_MASK);

	dev_dbg(dev, "%s(0x%08x, %u)\n", __func__, id, volumes);

	*val = volumes;

	return ret;
}

int vts_s_lif_soc_vol_change_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	int ret = 0;

	ret = snd_soc_component_update_bits(cmpnt,
			VTS_S_VOL_CHANGE(id),
			VTS_S_VOL_CHANGE_MASK,
			val << VTS_S_VOL_CHANGE_L);
	if (ret < 0)
		dev_err(dev, "%s failed(%d): %d\n",
			__func__, __LINE__, ret);

	dev_dbg(dev, "%s(0x%08x, %u)\n", __func__, id, val);

	return ret;
}

int vts_s_lif_soc_channel_map_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val)
{
#if (VTS_S_SOC_VERSION(1, 0, 0) == CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	return 0;

#elif (VTS_S_SOC_VERSION(1, 1, 1) >= CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl;
	unsigned int channel_map;
	unsigned int channel_map_mask;
	int ret = 0;

	if (id > 7) {
		dev_err(dev, "id(%d) is not valid\n", id);
		return -EINVAL;
	}

	ret = snd_soc_component_read(cmpnt, VTS_S_CHANNEL_MAP_BASE, &ctrl);
	if (ret < 0)
		dev_err(dev, "%s failed(%d): %d\n", __func__, __LINE__, ret);

	channel_map_mask = VTS_S_CHANNEL_MAP_MASK(id);
	channel_map = ((ctrl & channel_map_mask) >>
			(VTS_S_CHANNEL_MAP_ITV * id));

	dev_dbg(dev, "%s(0x%08x 0x%08x)\n", __func__, ctrl, channel_map_mask);
	dev_dbg(dev, "%s(%d, 0x%08x)\n", __func__, id, channel_map);

	*val = channel_map;
	data->channel_map = ctrl;

	return ret;
#endif
}

int vts_s_lif_soc_channel_map_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val)
{
#if (VTS_S_SOC_VERSION(1, 0, 0) == CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	return 0;

#elif (VTS_S_SOC_VERSION(1, 1, 1) >= CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	int ret = 0;

	if (id > 7) {
		dev_err(dev, "id(%d) is not valid\n", id);
		return -EINVAL;
	}

	ret = snd_soc_component_update_bits(cmpnt,
			VTS_S_CHANNEL_MAP_BASE,
			VTS_S_CHANNEL_MAP_MASK(id),
			val << (VTS_S_CHANNEL_MAP_ITV * id));
	if (ret < 0)
		dev_err(dev, "%s failed(%d): %d\n",
			__func__, __LINE__, ret);

	snd_soc_component_read(cmpnt, VTS_S_CHANNEL_MAP_BASE, &data->channel_map);
	dev_info(dev, "%s(0x%08x, 0x%x)\n", __func__, id, data->channel_map);

	return ret;
#endif
}

int vts_s_lif_soc_dmic_aud_control_hpf_sel_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	unsigned int mask = 0;
	int ret = 0;

	ret = regmap_read(regmap,
			VTS_S_SFR_CONTROL_DMIC_AUD_BASE,
			&data->control_dmic_aud[id]);

	if (ret < 0)
		dev_err(dev, "Failed to get HPF_SEL(%d): %d\n",
				id, data->control_dmic_aud[id]);

	mask = VTS_S_SFR_CONTROL_DMIC_AUD_HPF_SEL_MASK;
	*val = ((data->control_dmic_aud[id] & mask) >>
			VTS_S_SFR_CONTROL_DMIC_AUD_HPF_SEL_L);

	dev_dbg(dev, "%d mask 0x%x\n", __LINE__, mask);
	dev_dbg(dev, "%d dmic aud[%d] 0x%x \n", __LINE__,
			id, data->control_dmic_aud[id]);
	dev_dbg(dev, "%d val 0x%x \n", __LINE__, *val);

	return ret;
}

int vts_s_lif_soc_dmic_aud_control_hpf_sel_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val)
{
	struct device *dev = data->dev;
	unsigned int mask = 0;
	unsigned int value = 0;
	int ret = 0;

	mask = VTS_S_SFR_CONTROL_DMIC_AUD_HPF_SEL_MASK;
	value = (val << VTS_S_SFR_CONTROL_DMIC_AUD_HPF_SEL_L) & mask;
	data->control_dmic_aud[id] &= ~mask;
	data->control_dmic_aud[id] |= value;

	dev_dbg(dev, "%d mask 0x%x val 0x%x value 0x%x\n", __LINE__,
			mask, val, value);
	dev_dbg(dev, "%d dmic aud[%d] 0x%x \n", __LINE__,
			id, data->control_dmic_aud[id]);

	ret = vts_s_lif_soc_dmic_aud_control_gain_write(data, id);

	return ret;
}

int vts_s_lif_soc_dmic_aud_control_hpf_en_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	unsigned int mask = 0;
	int ret = 0;

	ret = regmap_read(regmap,
			VTS_S_SFR_CONTROL_DMIC_AUD_BASE,
			&data->control_dmic_aud[id]);

	if (ret < 0)
		dev_err(dev, "Failed to get HPF_EN(%d): %d\n",
				id, data->control_dmic_aud[id]);

	mask = VTS_S_SFR_CONTROL_DMIC_AUD_HPF_EN_MASK;
	*val = ((data->control_dmic_aud[id] & mask) >>
			VTS_S_SFR_CONTROL_DMIC_AUD_HPF_EN_L);
	dev_dbg(dev, "%d mask 0x%x\n", __LINE__, mask);
	dev_dbg(dev, "%d dmic aud[%d] 0x%x \n", __LINE__,
			id, data->control_dmic_aud[id]);
	dev_dbg(dev, "%d val 0x%x \n", __LINE__, *val);

	return ret;
}

int vts_s_lif_soc_dmic_aud_control_hpf_en_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val)
{
	struct device *dev = data->dev;
	unsigned int mask = 0;
	unsigned int value = 0;
	int ret = 0;

	mask = VTS_S_SFR_CONTROL_DMIC_AUD_HPF_EN_MASK;
	value = (val << VTS_S_SFR_CONTROL_DMIC_AUD_HPF_EN_L) & mask;
	data->control_dmic_aud[id] &= ~mask;
	data->control_dmic_aud[id] |= value;

	dev_dbg(dev, "%d mask 0x%x val 0x%x value 0x%x\n", __LINE__,
			mask, val, value);
	dev_dbg(dev, "%d dmic aud[%d] 0x%x \n", __LINE__,
			id, data->control_dmic_aud[id]);

	ret = vts_s_lif_soc_dmic_aud_control_gain_write(data, id);

	return ret;
}

int vts_s_lif_soc_dmic_en_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val)
{
	int ret = 0;

	*val = data->dmic_en[id];

	return ret;
}

static int vts_s_lif_soc_set_gpio_port(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val)
{
	struct device *dev = data->dev;
	const char *pin_name;

	switch (id) {
	case 0:
		if (!!val)
			pin_name = "s_lif_0_default";
		else
			pin_name = "s_lif_0_idle";
		break;
	case 1:
		if (!!val)
			pin_name = "s_lif_1_default";
		else
			pin_name = "s_lif_1_idle";
		break;
	case 2:
		if (!!val)
			pin_name = "s_lif_2_default";
		else
			pin_name = "s_lif_2_idle";
		break;
	default:
		dev_err(dev, "%s failed(%d): %d\n", __func__, __LINE__, -EINVAL);
		return -EINVAL;
	}

	if (!test_bit(VTS_S_STATE_OPENED, &data->state)) {
		dev_info(dev, "%s not powered(%d)\n", __func__, __LINE__);
		return 0;
	} else {
		return vts_s_lif_cfg_gpio(data->dev_vts, pin_name);
	}
}

static atomic_t dmic_state[VTS_S_LIF_DMIC_AUD_NUM];
int vts_s_lif_soc_dmic_en_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val)
{
	struct device *dev = data->dev;
	int ret = 0;

	if (val) {
		if (atomic_cmpxchg(&dmic_state[id], 0, 1) == 0) {
			/* for debug */
			data->dmic_en[id] = 1;
			dev_info(dev, "pin[%d] is %s (%d, %d)\n",
					id,
					val ? "enabled" : "disabled",
					atomic_read(&dmic_state[id]),
					data->dmic_en[id]);

			ret = vts_s_lif_soc_set_gpio_port(data, id, val);
		} else {
			dev_info(dev, "pin[%d] is already %s\n",
					id,
					val ? "enabled" : "disabled");
			return 0;
		}
	} else {
		if (atomic_cmpxchg(&dmic_state[id], 1, 0) == 1) {
			/* for debug */
			data->dmic_en[id] = 0;
			dev_info(dev, "pin [%d]is %s (%d, %d)\n",
					id,
					val ? "enabled" : "disabled",
					atomic_read(&dmic_state[id]),
					data->dmic_en[id]);

			ret = vts_s_lif_soc_set_gpio_port(data, id, val);
		} else {
			dev_info(dev, "pin[%d] is already %s\n",
					id,
					val ? "enabled" : "disabled");
			return 0;
		}
	}

	return ret;
}

int vts_s_lif_soc_dmic_aud_control_sys_sel_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val)
{
	struct device *dev = data->dev;
	unsigned int mask = 0;
	unsigned int value = 0;
	int ret = 0;

	mask = VTS_S_SFR_CONTROL_DMIC_AUD_SYS_SEL_MASK;
	value = (val << VTS_S_SFR_CONTROL_DMIC_AUD_SYS_SEL_L) & mask;
	data->control_dmic_aud[id] &= ~mask;
	data->control_dmic_aud[id] |= value;

	dev_dbg(dev, "%d mask 0x%x val 0x%x value 0x%x\n", __LINE__,
			mask, val, value);
	dev_dbg(dev, "%d dmic aud[%d] 0x%x \n", __LINE__,
			id, data->control_dmic_aud[id]);

	ret = vts_s_lif_soc_dmic_aud_control_gain_write(data, id);

	return ret;
}

static void vts_s_lif_mark_dirty_register(struct vts_s_lif_data *data)
{
	regcache_mark_dirty(data->regmap_dmic_aud[0]);
	regcache_mark_dirty(data->regmap_dmic_aud[1]);
	regcache_mark_dirty(data->regmap_dmic_aud[2]);
	regcache_mark_dirty(data->regmap_sfr);
}

static void vts_s_lif_save_register(struct vts_s_lif_data *data)
{
	regcache_cache_only(data->regmap_dmic_aud[0], true);
	regcache_mark_dirty(data->regmap_dmic_aud[0]);
	regcache_cache_only(data->regmap_dmic_aud[1], true);
	regcache_mark_dirty(data->regmap_dmic_aud[1]);
	regcache_cache_only(data->regmap_dmic_aud[2], true);
	regcache_mark_dirty(data->regmap_dmic_aud[2]);
	regcache_cache_only(data->regmap_sfr, true);
	regcache_mark_dirty(data->regmap_sfr);
}

static void vts_s_lif_restore_register(struct vts_s_lif_data *data)
{
	regcache_cache_only(data->regmap_dmic_aud[0], false);
	regcache_sync(data->regmap_dmic_aud[0]);
	regcache_cache_only(data->regmap_dmic_aud[1], false);
	regcache_sync(data->regmap_dmic_aud[1]);
	regcache_cache_only(data->regmap_dmic_aud[2], false);
	regcache_sync(data->regmap_dmic_aud[2]);
	regcache_cache_only(data->regmap_sfr, false);
	regcache_sync(data->regmap_sfr);
}

static int vts_s_lif_soc_set_fmt_master(struct vts_s_lif_data *data,
		unsigned int fmt)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl;
	int ret = 0;

	dev_info(dev, "%s(0x%08x)\n", __func__, fmt);

	if (fmt < 0)
		return -EINVAL;

	ret = snd_soc_component_read(cmpnt, VTS_S_CONFIG_MASTER_BASE, &ctrl);
	if (ret < 0)
		dev_err(dev, "%s failed(%d): %d\n", __func__, __LINE__, ret);

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_MASTER_WS_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_NB_IF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_MASTER_WS_POLAR, 1);
		break;
	case SND_SOC_DAIFMT_IB_NF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_MASTER_WS_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_IB_IF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_MASTER_WS_POLAR, 1);
		break;
	default:
		ret = -EINVAL;
	}

	dev_info(dev, "%s ctrl(0x%08x)\n", __func__, ctrl);
	snd_soc_component_write(cmpnt, VTS_S_CONFIG_MASTER_BASE, ctrl);

	return ret;
}

static int vts_s_lif_soc_set_fmt_slave(struct vts_s_lif_data *data,
		unsigned int fmt)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl;
	int ret = 0;

	dev_info(dev, "%s(0x%08x)\n", __func__, fmt);

	if (fmt < 0)
		return -EINVAL;

	ret = snd_soc_component_read(cmpnt, VTS_S_CONFIG_SLAVE_BASE, &ctrl);
	if (ret < 0)
		dev_err(dev, "%s failed(%d): %d\n", __func__, __LINE__, ret);

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_SLAVE_WS_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_NB_IF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_SLAVE_WS_POLAR, 1);
		break;
	case SND_SOC_DAIFMT_IB_NF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_SLAVE_WS_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_IB_IF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_SLAVE_WS_POLAR, 1);
		break;
	default:
		ret = -EINVAL;
	}

	dev_info(dev, "%s ctrl(0x%08x)\n", __func__, ctrl);
	snd_soc_component_write(cmpnt, VTS_S_CONFIG_SLAVE_BASE, ctrl);

	return ret;
}

int vts_s_lif_soc_set_fmt(struct vts_s_lif_data *data, unsigned int fmt)
{
	struct device *dev = data->dev;
	int ret = 0;

	dev_info(dev, "%s(0x%08x)\n", __func__, fmt);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
	case SND_SOC_DAIFMT_CBM_CFS:
		set_bit(VTS_S_MODE_MASTER, &data->mode);
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
	case SND_SOC_DAIFMT_CBS_CFS:
		set_bit(VTS_S_MODE_SLAVE, &data->mode);
		break;
	default:
		ret = -EINVAL;
	}

	data->fmt = fmt;

	return ret;
}

int vts_s_lif_soc_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params,
		struct vts_s_lif_data *data)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl0, ctrl1;
	int clk_val = 0;
	int val = 0;
	int ret = 0;
	unsigned int i = 0;

	dev_info(dev, "%s[%c]\n", __func__,
			(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ?
			'C' : 'P');

	data->channels = params_channels(hw_params);
	data->rate = params_rate(hw_params);
	data->width = params_width(hw_params);
	data->clk_table_id = vts_s_lif_clk_table_id_search(data->rate,
			data->width);

	dev_info(dev, "rate=%u, width=%d, channel=%u\n",
			data->rate, data->width, data->channels);

	if (data->channels > 8) {
		dev_err(dev, "(%d) is not support channels\n", data->channels);
		return -EINVAL;
	}

	ret = snd_soc_component_read(cmpnt, VTS_S_CONFIG_MASTER_BASE, &ctrl0);
	if (ret < 0)
		dev_err(dev, "Failed to access CONFIG_MASTER sfr(%d): %d\n",
				__LINE__, ret);
	ret = snd_soc_component_read(cmpnt, VTS_S_CONFIG_SLAVE_BASE, &ctrl1);
	if (ret < 0)
		dev_err(dev, "Failed to access CONFIG_SLAVE sfr(%d): %d\n",
				__LINE__, ret);

#if (VTS_S_SOC_VERSION(1, 0, 0) == CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	switch (params_format(hw_params)) {
	case SNDRV_PCM_FORMAT_S16:
		vts_set_value_by_name(ctrl0, VTS_S_CONFIG_MASTER_OPMODE, 1);
		vts_set_value_by_name(ctrl1, VTS_S_CONFIG_SLAVE_OPMODE, 1);

		break;
	case SNDRV_PCM_FORMAT_S24:
		vts_set_value_by_name(ctrl0, VTS_S_CONFIG_MASTER_OPMODE, 3);
		vts_set_value_by_name(ctrl1, VTS_S_CONFIG_SLAVE_OPMODE, 3);

		break;
	default:
		return -EINVAL;
	}
#elif (VTS_S_SOC_VERSION(1, 1, 1) >= CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	switch (params_format(hw_params)) {
	case SNDRV_PCM_FORMAT_S16:
		vts_set_value_by_name(ctrl0, VTS_S_CONFIG_MASTER_OP_D, 1);
		vts_set_value_by_name(ctrl1, VTS_S_CONFIG_SLAVE_OP_D, 1);

		break;
	case SNDRV_PCM_FORMAT_S24:
		vts_set_value_by_name(ctrl0, VTS_S_CONFIG_MASTER_OP_D, 0);
		vts_set_value_by_name(ctrl1, VTS_S_CONFIG_SLAVE_OP_D, 0);

		break;
	default:
		return -EINVAL;
	}

	vts_set_value_by_name(ctrl0, VTS_S_CONFIG_MASTER_OP_C,
			(data->channels - 1));
	vts_set_value_by_name(ctrl1, VTS_S_CONFIG_SLAVE_OP_C,
			(data->channels - 1));
#else
#error "VTS_S_SOC_VERSION is not defined"
#endif

	/* SYS_SEL */
	for (i = 0; i < VTS_S_LIF_DMIC_AUD_NUM; i ++) {
		val = vts_s_lif_clk_table_get(data->clk_table_id,
				CLK_TABLE_SYS_SEL);

		ret = vts_s_lif_soc_dmic_aud_control_sys_sel_put(data, i, val);
		if (ret < 0)
			dev_err(dev, "%s (%d) failed SYS_SEL[%d] ctrl %d\n",
				__func__, __LINE__, i, ret);
	}


	clk_val = vts_s_lif_clk_table_get(data->clk_table_id,
			CLK_TABLE_DMIC_AUD);
	if (clk_val < 0)
		dev_err(dev, "Failed to find clk table : %s\n", "clk_dmic_aud");
	dev_info(dev, "find clk table : %s: (%d)\n", "clk_dmic_aud", clk_val);
	ret = clk_set_rate(data->clk_dmic_aud, clk_val);
	if (ret < 0) {
		dev_err(dev, "Failed to set rate : %s\n", "dmic_aud");
		return ret;
	}

	clk_val = vts_s_lif_clk_table_get(data->clk_table_id,
			CLK_TABLE_DMIC_AUD_PAD);
	if (clk_val < 0)
		dev_err(dev, "Failed to find clk table : %s\n", "clk_dmic_pad");
	dev_info(dev, "find clk table : %s: (%d)\n", "clk_dmic_pad", clk_val);
	ret = clk_set_rate(data->clk_dmic_aud_pad, clk_val);
	if (ret < 0) {
		dev_err(dev, "Failed to set rate : %s\n", "dmic_aud_pad");
		return ret;
	}

	clk_val = vts_s_lif_clk_table_get(data->clk_table_id,
			CLK_TABLE_DMIC_AUD_DIV2);
	if (clk_val < 0)
		dev_err(dev, "Failed to find clk : %s\n", "clk_dmic_div2");
	dev_info(dev, "find clk table : %s: (%d)\n", "clk_dmic_div2", clk_val);
	ret = clk_set_rate(data->clk_dmic_aud_div2, clk_val);
	if (ret < 0) {
		dev_err(dev, "Failed to set rate : %s\n", "dmic_aud_div2");
		return ret;
	}

	clk_val = vts_s_lif_clk_table_get(data->clk_table_id,
			CLK_TABLE_SERIAL_LIF);
	if (clk_val < 0)
		dev_err(dev, "Failed to find clk table : %s\n",
				"clk_serial_lif");
	dev_info(dev, "find clk table : %s: (%d)\n",
			"clk_serial_lif", clk_val);
	/* change blck to supprot channel */
	clk_val = (clk_val * VTS_S_LIF_MAX_CHANNEL) / data->channels;
	ret = clk_set_rate(data->clk_serial_lif, clk_val);
	if (ret < 0)
		dev_err(dev, "Failed to set rate : %s\n", "clk_s_lif");

#ifdef VTS_S_LIF_USE_AUD0
	ret = clk_enable(data->clk_mux_dmic_aud);
	if (ret < 0) {
		dev_err(dev, "Failed to enable clk_mux_dmic_aud: %d\n", ret);
		return ret;
	}
	ret = clk_enable(data->clk_mux_serial_lif);
	if (ret < 0) {
		dev_err(dev, "Failed to enable clk_mux_serial_lif: %d\n", ret);
		return ret;
	}
#endif

	ret = clk_enable(data->clk_dmic_aud_pad);
	if (ret < 0) {
		dev_err(dev, "Failed to enable clk_dmic_aud_pad: %d\n", ret);
		return ret;
	}
	ret = clk_enable(data->clk_dmic_aud_div2);
	if (ret < 0) {
		dev_err(dev, "Failed to enable clk_dmic_aud_div2: %d\n", ret);
		return ret;
	}
	ret = clk_enable(data->clk_dmic_aud);
	if (ret < 0) {
		dev_err(dev, "Failed to enable clk_dmic_aud: %d\n", ret);
		return ret;
	}
	ret = clk_enable(data->clk_serial_lif);
	if (ret < 0) {
		dev_err(dev, "Failed to enable clk_s_lif: %d\n", ret);
		return ret;
	}

	ret = snd_soc_component_write(cmpnt, VTS_S_CONFIG_MASTER_BASE, ctrl0);
	if (ret < 0)
		dev_err(dev, "Failed to access CONFIG_MASTER sfr(%d): %d\n",
				__LINE__, ret);
	ret = snd_soc_component_write(cmpnt, VTS_S_CONFIG_SLAVE_BASE, ctrl1);
	if (ret < 0)
		dev_err(dev, "Failed to access CONFIG_SLAVE sfr(%d): %d\n",
				__LINE__, ret);

	set_bit(VTS_S_STATE_SET_PARAM, &data->state);

	return 0;
}

int vts_s_lif_soc_startup(struct snd_pcm_substream *substream,
		struct vts_s_lif_data *data)
{
	struct device *dev = data->dev;
	int ret = 0;

	dev_info(dev, "%s[%c]\n", __func__,
			(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ?
			'C' : 'P');

	pm_runtime_get_sync(dev);

	ret = clk_enable(data->clk_src);
	if (ret < 0) {
		dev_err(dev, "Failed to enable clk_src: %d\n", ret);
		goto err;
	}

#ifdef VTS_S_LIF_USE_AUD0
	ret = clk_set_rate(data->clk_mux_dmic_aud,
			clk_get_rate(data->clk_src));
	if (ret < 0) {
		dev_err(dev, "Failed to set rate : %s\n", "clk_mux_dmic_aud");
		return ret;
	}

	ret = clk_set_rate(data->clk_mux_serial_lif,
			clk_get_rate(data->clk_src));
	if (ret < 0) {
		dev_err(dev, "Failed to set rate : %s\n", "clk_mux_serial_lif");
		return ret;
	}
#endif

	vts_s_lif_restore_register(data);
	set_bit(VTS_S_STATE_OPENED, &data->state);

	if (data->dmic_en[0])
		vts_s_lif_soc_set_gpio_port(data, 0, data->dmic_en[0]);
	if (data->dmic_en[1])
		vts_s_lif_soc_set_gpio_port(data, 1, data->dmic_en[1]);
	if (data->dmic_en[2])
		vts_s_lif_soc_set_gpio_port(data, 2, data->dmic_en[2]);

	return 0;

err:
	pm_runtime_put_sync(dev);
	return ret;
}

void vts_s_lif_soc_shutdown(struct snd_pcm_substream *substream,
		struct vts_s_lif_data *data)
{
	struct device *dev = data->dev;

	dev_dbg(dev, "%s[%c]\n", __func__,
			(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ?
			'C' : 'P');

	/* make default pin state as idle to prevent conflict with vts */
	if (data->dmic_en[0])
		vts_s_lif_soc_set_gpio_port(data, 0, 0);
	if (data->dmic_en[1])
		vts_s_lif_soc_set_gpio_port(data, 1, 0);
	if (data->dmic_en[2])
		vts_s_lif_soc_set_gpio_port(data, 2, 0);

	vts_s_lif_save_register(data);

	/* reset status */
	vts_s_lif_soc_reset_status(data);

	if (test_bit(VTS_S_STATE_SET_PARAM, &data->state)) {
		clk_disable(data->clk_serial_lif);
		clk_disable(data->clk_mux_serial_lif);
		clk_disable(data->clk_dmic_aud_pad);
		clk_disable(data->clk_dmic_aud_div2);
		clk_disable(data->clk_dmic_aud);
		clk_disable(data->clk_mux_dmic_aud);
		clear_bit(VTS_S_STATE_SET_PARAM, &data->state);
	}
	clk_disable(data->clk_src);

	clear_bit(VTS_S_STATE_OPENED, &data->state);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_sync(dev);
}

int vts_s_lif_soc_hw_free(struct snd_pcm_substream *substream,
		struct vts_s_lif_data *data)
{
	struct device *dev = data->dev;

	dev_dbg(dev, "%s[%c]\n", __func__,
			(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ?
			'C' : 'P');

	return 0;
}

int vts_s_lif_soc_dma_en(int enable,
		struct vts_s_lif_data *data)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl;
	int ret = 0;

	dev_info(dev, "%s enable(%d)\n", __func__, enable);

	if (unlikely(data->slif_dump_enabled)) {
		ret = vts_s_lif_soc_set_fmt_slave(data, data->fmt);
		ret |= vts_s_lif_soc_set_fmt_master(data, data->fmt);
		if (ret < 0) {
			dev_err(dev, "Failed to access CTRL sfr(%d): %d\n",
					__LINE__, ret);
			return ret;
		}
	} else {
		if (test_bit(VTS_S_MODE_SLAVE, &data->mode))
			ret = vts_s_lif_soc_set_fmt_slave(data, data->fmt);
		if (test_bit(VTS_S_MODE_MASTER, &data->mode))
			ret = vts_s_lif_soc_set_fmt_master(data, data->fmt);

		if (ret < 0) {
			dev_err(dev, "Failed to access CTRL sfr(%d): %d\n",
					__LINE__, ret);
			return ret;
		}
	}

	if (unlikely(data->slif_dump_enabled)) {
		ret = snd_soc_component_update_bits(cmpnt,
				VTS_S_CONFIG_DONE_BASE,
				VTS_S_CONFIG_DONE_MASTER_CONFIG_MASK |
				VTS_S_CONFIG_DONE_SLAVE_CONFIG_MASK |
				VTS_S_CONFIG_DONE_ALL_CONFIG_MASK,
				(enable << VTS_S_CONFIG_DONE_MASTER_CONFIG_L) |
				(enable << VTS_S_CONFIG_DONE_SLAVE_CONFIG_L) |
				(enable << VTS_S_CONFIG_DONE_ALL_CONFIG_L));
		if (ret < 0)
			dev_err(dev, "Failed to access CTRL sfr(%d): %d\n",
				__LINE__, ret);
	} else {
		if (test_bit(VTS_S_MODE_SLAVE, &data->mode)) {
			ret = snd_soc_component_update_bits(cmpnt,
					VTS_S_CONFIG_DONE_BASE,
					VTS_S_CONFIG_DONE_SLAVE_CONFIG_MASK |
					VTS_S_CONFIG_DONE_ALL_CONFIG_MASK,
					(enable << VTS_S_CONFIG_DONE_SLAVE_CONFIG_L) |
					(enable << VTS_S_CONFIG_DONE_ALL_CONFIG_L));
			if (ret < 0)
				dev_err(dev, "Failed to access CTRL sfr(%d): %d\n",
					__LINE__, ret);
		}
		if (test_bit(VTS_S_MODE_MASTER, &data->mode)) {
			ret = snd_soc_component_update_bits(cmpnt,
					VTS_S_CONFIG_DONE_BASE,
					VTS_S_CONFIG_DONE_MASTER_CONFIG_MASK |
					VTS_S_CONFIG_DONE_ALL_CONFIG_MASK,
					(enable << VTS_S_CONFIG_DONE_MASTER_CONFIG_L) |
					(enable << VTS_S_CONFIG_DONE_ALL_CONFIG_L));
			if (ret < 0)
				dev_err(dev, "Failed to access CTRL sfr(%d): %d\n",
					__LINE__, ret);
		}
	}

	ret = snd_soc_component_read(cmpnt, VTS_S_CONFIG_DONE_BASE, &ctrl);
	if (ret < 0)
		dev_err(dev, "%s failed(%d): %d\n", __func__, __LINE__, ret);
	dev_info(dev, "%s ctrl(0x%08x)\n", __func__, ctrl);

	/* PAD configuration */
	vts_s_lif_soc_set_sel_pad(data, enable);
	vts_s_lif_dmic_aud_en(data->dev_vts, enable);
	vts_s_lif_dmic_if_en(data->dev_vts, enable);

	/* HACK : MOVE to resume */
	if (enable)
		vts_pad_retention(false);

	/* DMIC_IF configuration */
	vts_s_lif_soc_set_dmic_aud(data, enable);

	data->mute_enable = enable;
	dev_info(dev, "%s en(%d) ms(%d)\n", __func__, enable, data->mute_ms);
	if (enable && (data->mute_ms != 0)) {
		/* queue delayed work at starting */
		schedule_delayed_work(&data->mute_work, msecs_to_jiffies(data->mute_ms));
	} else {
		/* check dmic port and enable EN bit */
		ret = snd_soc_component_update_bits(cmpnt,
				VTS_S_INPUT_EN_BASE,
				VTS_S_INPUT_EN_EN0_MASK |
				VTS_S_INPUT_EN_EN1_MASK |
				VTS_S_INPUT_EN_EN2_MASK,
				(enable << VTS_S_INPUT_EN_EN0_L) |
				(enable << VTS_S_INPUT_EN_EN1_L) |
				(enable << VTS_S_INPUT_EN_EN2_L));
		if (ret < 0)
			dev_err(dev, "Failed to access INPUT_EN sfr(%d): %d\n",
				__LINE__, ret);
	}

	if (unlikely(data->slif_dump_enabled)) {
		ret = snd_soc_component_update_bits(cmpnt,
				VTS_S_CTRL_BASE,
				VTS_S_CTRL_SLAVE_IF_EN_MASK |
				VTS_S_CTRL_MASTER_IF_EN_MASK |
				VTS_S_CTRL_LOOPBACK_EN_MASK |
				VTS_S_CTRL_SPU_EN_MASK,
				(enable << VTS_S_CTRL_SLAVE_IF_EN_L) |
				(enable << VTS_S_CTRL_MASTER_IF_EN_L) |
				(enable << VTS_S_CTRL_LOOPBACK_EN_L) |
				(enable << VTS_S_CTRL_SPU_EN_L));
		if (ret < 0)
			dev_err(dev, "Failed to access CTRL sfr(%d): %d\n",
				__LINE__, ret);
	} else {
		if (test_bit(VTS_S_MODE_SLAVE, &data->mode)) {
			ret = snd_soc_component_update_bits(cmpnt,
					VTS_S_CTRL_BASE,
					VTS_S_CTRL_SLAVE_IF_EN_MASK |
					VTS_S_CTRL_SPU_EN_MASK,
					(enable << VTS_S_CTRL_SLAVE_IF_EN_L) |
					(enable << VTS_S_CTRL_SPU_EN_L));
			if (ret < 0)
				dev_err(dev, "Failed to access CTRL sfr(%d): %d\n",
					__LINE__, ret);
		}
		if (test_bit(VTS_S_MODE_MASTER, &data->mode)) {
			ret = snd_soc_component_update_bits(cmpnt,
					VTS_S_CTRL_BASE,
					VTS_S_CTRL_MASTER_IF_EN_MASK |
					VTS_S_CTRL_SPU_EN_MASK,
					(enable << VTS_S_CTRL_MASTER_IF_EN_L) |
					(enable << VTS_S_CTRL_SPU_EN_L));
			if (ret < 0)
				dev_err(dev, "Failed to access CTRL sfr(%d): %d\n",
					__LINE__, ret);
		}
	}

	ret = snd_soc_component_read(cmpnt, VTS_S_CTRL_BASE, &ctrl);
	if (ret < 0)
		dev_err(dev, "%s failed(%d): %d\n", __func__, __LINE__, ret);
	dev_info(dev, "%s ctrl(0x%08x)\n", __func__, ctrl);
#if 0
	if (unlikely(data->slif_dump_enabled)) {
		ret = snd_soc_component_update_bits(cmpnt,
				VTS_S_CONFIG_DONE_BASE,
				VTS_S_CONFIG_DONE_MASTER_CONFIG_MASK |
				VTS_S_CONFIG_DONE_SLAVE_CONFIG_MASK |
				VTS_S_CONFIG_DONE_ALL_CONFIG_MASK,
				(enable << VTS_S_CONFIG_DONE_MASTER_CONFIG_L) |
				(enable << VTS_S_CONFIG_DONE_SLAVE_CONFIG_L) |
				(enable << VTS_S_CONFIG_DONE_ALL_CONFIG_L));
		if (ret < 0)
			dev_err(dev, "Failed to access CTRL sfr(%d): %d\n",
				__LINE__, ret);
	} else {
		if (test_bit(VTS_S_MODE_SLAVE, &data->mode)) {
			ret = snd_soc_component_update_bits(cmpnt,
					VTS_S_CONFIG_DONE_BASE,
					VTS_S_CONFIG_DONE_SLAVE_CONFIG_MASK |
					VTS_S_CONFIG_DONE_ALL_CONFIG_MASK,
					(enable << VTS_S_CONFIG_DONE_SLAVE_CONFIG_L) |
					(enable << VTS_S_CONFIG_DONE_ALL_CONFIG_L));
			if (ret < 0)
				dev_err(dev, "Failed to access CTRL sfr(%d): %d\n",
					__LINE__, ret);
		}
		if (test_bit(VTS_S_MODE_MASTER, &data->mode)) {
			ret = snd_soc_component_update_bits(cmpnt,
					VTS_S_CONFIG_DONE_BASE,
					VTS_S_CONFIG_DONE_MASTER_CONFIG_MASK |
					VTS_S_CONFIG_DONE_ALL_CONFIG_MASK,
					(enable << VTS_S_CONFIG_DONE_MASTER_CONFIG_L) |
					(enable << VTS_S_CONFIG_DONE_ALL_CONFIG_L));
			if (ret < 0)
				dev_err(dev, "Failed to access CTRL sfr(%d): %d\n",
					__LINE__, ret);
		}
	}

	ret = snd_soc_component_read(cmpnt, VTS_S_CONFIG_DONE_BASE, &ctrl);
	if (ret < 0)
		dev_err(dev, "%s failed(%d): %d\n", __func__, __LINE__, ret);
	dev_info(dev, "%s ctrl(0x%08x)\n", __func__, ctrl);
#endif
#ifdef VTS_S_LIF_REG_LOW_DUMP
	vts_s_lif_check_reg(0);
#endif

	return ret;
}

static struct clk *devm_clk_get_and_prepare(struct device *dev, const char *name)
{
	struct clk *clk;
	int result;

	clk = devm_clk_get(dev, name);
	if (IS_ERR(clk)) {
		dev_err(dev, "Failed to get clock %s\n", name);
		goto error;
	}

	result = clk_prepare(clk);
	if (result < 0) {
		dev_err(dev, "Failed to prepare clock %s\n", name);
		goto error;
	}

error:
	return clk;
}

static void vts_s_lif_soc_mute_func(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct vts_s_lif_data *data = container_of(dwork, struct vts_s_lif_data,
			mute_work);
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	int ret = 0;

	dev_info(dev, "%s: (en:%d)\n", __func__, data->mute_enable);

	/* check dmic port and enable EN bit */
	ret = snd_soc_component_update_bits(cmpnt,
			VTS_S_INPUT_EN_BASE,
			VTS_S_INPUT_EN_EN0_MASK |
			VTS_S_INPUT_EN_EN1_MASK |
			VTS_S_INPUT_EN_EN2_MASK,
			(data->mute_enable << VTS_S_INPUT_EN_EN0_L) |
			(data->mute_enable << VTS_S_INPUT_EN_EN1_L) |
			(data->mute_enable << VTS_S_INPUT_EN_EN2_L));
	if (ret < 0)
		dev_err(dev, "Failed to access INPUT_EN sfr(%d): %d\n",
			__LINE__, ret);

}

static DECLARE_DELAYED_WORK(vts_s_lif_soc_mute, vts_s_lif_soc_mute_func);

int vts_s_lif_soc_probe(struct vts_s_lif_data *data)
{
	struct device *dev = data->dev;
	int ret = 0;

#ifdef VTS_S_LIF_USE_AUD0
	data->clk_src = devm_clk_get_and_prepare(dev, "aud0");
	if (IS_ERR(data->clk_src)) {
		ret = PTR_ERR(data->clk_src);
		goto err;
	}
#else
	data->clk_src = devm_clk_get_and_prepare(dev, "rco");
	if (IS_ERR(data->clk_src)) {
		ret = PTR_ERR(data->clk_src);
		goto err;
	}
#endif

	data->clk_mux_dmic_aud = devm_clk_get_and_prepare(dev, "mux_dmic_aud");
	if (IS_ERR(data->clk_mux_dmic_aud)) {
		ret = PTR_ERR(data->clk_mux_dmic_aud);
		goto err;
	}

#ifdef VTS_S_LIF_USE_AUD0
	/* HACK: only use in probe to disable VTS_AUD1_USER_MUX */
	data->clk_src1 = devm_clk_get_and_prepare(dev, "aud1");
	if (IS_ERR(data->clk_src1)) {
		ret = PTR_ERR(data->clk_src1);
		goto err;
	}
#endif

	data->clk_mux_serial_lif = devm_clk_get_and_prepare(dev, "mux_serial_lif");
	if (IS_ERR(data->clk_mux_serial_lif)) {
		ret = PTR_ERR(data->clk_mux_serial_lif);
		goto err;
	}

	data->clk_dmic_aud_pad = devm_clk_get_and_prepare(dev, "dmic_pad");
	if (IS_ERR(data->clk_dmic_aud_pad)) {
		ret = PTR_ERR(data->clk_dmic_aud_pad);
		goto err;
	}

	data->clk_dmic_aud_div2 = devm_clk_get_and_prepare(dev, "dmic_aud_div2");
	if (IS_ERR(data->clk_dmic_aud_div2)) {
		ret = PTR_ERR(data->clk_dmic_aud_div2);
		goto err;
	}

	data->clk_dmic_aud = devm_clk_get_and_prepare(dev, "dmic_aud");
	if (IS_ERR(data->clk_dmic_aud)) {
		ret = PTR_ERR(data->clk_dmic_aud);
		goto err;
	}

	data->clk_serial_lif = devm_clk_get_and_prepare(dev, "serial_lif");
	if (IS_ERR(data->clk_serial_lif)) {
		ret = PTR_ERR(data->clk_serial_lif);
		goto err;
	}

/* HACK: dummy code : disable input clk of mux_dmic_aud */
#ifdef VTS_S_LIF_USE_AUD0
	ret = clk_set_rate(data->clk_src, 0);
	if (ret < 0) {
		dev_err(dev, "Failed to set rate : %s\n", "clk_src");
		return ret;
	}
	ret = clk_enable(data->clk_src);
	if (ret < 0) {
		dev_err(dev, "Failed to enable clk_src: %d\n", ret);
		goto err;
	}

	ret = clk_set_rate(data->clk_src1, 0);
	if (ret < 0) {
		dev_err(dev, "Failed to set rate : %s\n", "clk_src1");
		return ret;
	}
	ret = clk_enable(data->clk_src1);
	if (ret < 0) {
		dev_err(dev, "Failed to enable clk_src1: %d\n", ret);
		goto err;
	}

	clk_disable(data->clk_src);
	clk_disable(data->clk_src1);
#endif
	/* dummy setting */
	vts_s_lif_soc_set_gpio_port(data, 1, 1);
	vts_s_lif_soc_set_gpio_port(data, 2, 1);
	vts_s_lif_soc_set_gpio_port(data, 3, 1);
	vts_s_lif_soc_set_gpio_port(data, 1, 0);
	vts_s_lif_soc_set_gpio_port(data, 2, 0);
	vts_s_lif_soc_set_gpio_port(data, 3, 0);

	data->mute_enable = false;
	data->mute_ms = 0;
	atomic_set(&dmic_state[0], 0);
	atomic_set(&dmic_state[1], 0);
	atomic_set(&dmic_state[2], 0);
	vts_s_lif_mark_dirty_register(data);
	vts_s_lif_save_register(data);

	vts_s_lif_soc_set_default_gain(data);

	pm_runtime_no_callbacks(dev);
	pm_runtime_enable(dev);

	INIT_DELAYED_WORK(&data->mute_work, vts_s_lif_soc_mute_func);

	return 0;

err:
	return ret;
}
