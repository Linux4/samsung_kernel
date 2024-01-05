
/* linux/drivers/video/s5p_smies.c
 *
 * Samsung SoC SMIES driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/fb.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include <mach/exynos5_bus.h>
#include <mach/map.h>
#include <mach/bts.h>
#include <plat/regs-smies.h>
#include <plat/fb.h>
#include <plat/cpu.h>
#include <plat/smies.h>

#include "s5p_smies.h"

static const u32 gamma_curve_linear[65] = {
	0,	4,	8,	12,	16,	20,	24,	28,
	32,	36,	40,	44,	48,	52,	56,	60,
	64,	68,	72,	76,	80,	84,	88,	92,
	96,	100,	104,	108,	112,	116,	120,	124,
	128,	132,	136,	140,	144,	148,	152,	156,
	160,	164,	168,	172,	176,	180,	184,	188,
	192,	196,	200,	204,	208,	212,	216,	220,
	224,	228,	232,	236,	240,	244,	248,	252,
	256,
};

static const u32 gamma_curve_2_1[65] = {
	0,	25,	43,	55,	64,	72,	79,	86,
	92,	98,	103,	108,	113,	117,	122,	126,
	130,	134,	138,	141,	145,	149,	152,	155,
	159,	162,	165,	168,	171,	174,	177,	180,
	182,	185,	188,	190,	193,	196,	198,	201,
	203,	205,	208,	210,	213,	215,	217,	219,
	222,	224,	226,	228,	230,	232,	235,	237,
	239,	241,	243,	245,	247,	249,	251,	253,
	255,
};

static const u32 gamma_curve_2_2[65] = {
	0,	28, 	46, 	59, 	68, 	76, 	84,	90,
	96,	102,	107,	112,	117,	122,	126,	130,
	134,	138,	142,	145,	149,	152,	156,	159,
	162,	165,	168,	171,	174,	177,	180,	182,
	185,	188,	190,	193,	195 ,	198,	200,	203,
	205,	207,	210,	212,	214,	217,	219,	221,
	223,	225,	227,	229,	231,	233,	235,	237,
	239,	241,	243,	245,	247,	249,	251,	253,
	255,
};

static void smies_dump_registers(struct s5p_smies_device *smies)
{
	dev_dbg(smies->dev, "dumping registers\n");
	print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_ADDRESS, 32, 4, smies->reg_base,
			0x0200, false);
}

static void smies_set_image_size(struct s5p_smies_device *smies)
{
	u32 val;
	u32 width, height;

	width = smies->pdata->width;
	height = smies->pdata->height;

	val = SMIES_IMGSIZE_HEIGHT_VAL(height) | SMIES_IMGSIZE_WIDTH_VAL(width);
	smies_write(smies, SMIESIMG_SIZESET, val);
}

static void smies_set_crc(struct s5p_smies_device *smies, int on)
{
	u32 val = on ? ~0 : 0;

	smies_write_mask(smies, SMIESCRC_CON, val,
		(SMIES_CRC_CON_CLKEN | SMIES_CRC_CON_START | SMIES_CRC_CON_EN));
}

static void smies_scr_cofig(struct s5p_smies_device *smies)
{
	smies_write(smies, SMIESSCR_RED, smies->pdata->scr_r);
	smies_write(smies, SMIESSCR_GREEN, smies->pdata->scr_g);
	smies_write(smies, SMIESSCR_BLUE, smies->pdata->scr_b);

	smies_write(smies, SMIESSCR_CYAN, smies->pdata->scr_c);
	smies_write(smies, SMIESSCR_MAGENTA, smies->pdata->scr_m);
	smies_write(smies, SMIESSCR_YELLOW, smies->pdata->scr_y);

	smies_write(smies, SMIESSCR_WHITE, smies->pdata->scr_white);
	smies_write(smies, SMIESSCR_BLACK, smies->pdata->scr_black);
}

static void smies_sae_config(struct s5p_smies_device *smies)
{
	u32 val = smies->pdata->sae_skin_check ? ~0 : 0;

	smies_write_mask(smies, SMIESCON, val, SMIES_CON_SAE_SKIN_CHECK);

	val = SMIES_CON_SAE_GAIN_VAL(smies->pdata->sae_gain);
	smies_write_mask(smies, SMIESCON, val, SMIES_CON_SAE_GAIN_MASK);
}

static void smies_set_core_clk_gate(struct s5p_smies_device *smies, int on)
{
	u32 val = on ? ~0 : 0;

	smies_write_mask(smies, SMIESCON, val, SMIES_CON_CLK_GATE_ON);
}

static int smies_gamma_lut_config(struct s5p_smies_device *smies,
		enum SMIES_GAMMA_CUV cuv)
{
	const u32 *gamma_r, *gamma_g, *gamma_b;
	u32 val, i;
	int cnt = (SMIES_GAMMALUT_CNT + 1) >> 1;

	switch (cuv) {
	case SMIES_CUV_LINEAR:
		gamma_r = gamma_curve_linear;
		gamma_g = gamma_curve_linear;
		gamma_b = gamma_curve_linear;
		break;
	case SMIES_CUV_2_1:
		gamma_r = gamma_curve_2_1;
		gamma_g = gamma_curve_2_1;
		gamma_b = gamma_curve_2_1;
		break;
	case SMIES_CUV_2_2:
		gamma_r = gamma_curve_2_2;
		gamma_g = gamma_curve_2_2;
		gamma_b = gamma_curve_2_2;
		break;
	default:
		dev_info(smies->dev, "default gamma value is used\n");
		return -EINVAL;
	}

	if (!(gamma_r && gamma_g && gamma_b)) {
		dev_info(smies->dev, "setting gamma LUT is failed\n");
		return -ENXIO;
	}

	for (i = 0; i < cnt; ++i) {
		val = SMIES_GAMMALUT_EVEN_VAL(*gamma_r);
		gamma_r++;
		if (i == (cnt - 1)) {
			smies_write(smies, SMIESGAMMALUT_R + i * 4, val);
		} else {
			val |= SMIES_GAMMALUT_ODD_VAL(*gamma_r);
			smies_write(smies, SMIESGAMMALUT_R + i * 4, val);
			gamma_r++;
		}
	}

	for (i = 0; i < cnt; ++i) {
		val = SMIES_GAMMALUT_EVEN_VAL(*gamma_g);
		gamma_g++;
		if (i == (cnt - 1)) {
			smies_write(smies, SMIESGAMMALUT_G + i * 4, val);
		} else {
			val |= SMIES_GAMMALUT_ODD_VAL(*gamma_g);
			smies_write(smies, SMIESGAMMALUT_G + i * 4, val);
			gamma_g++;
		}
	}

	for (i = 0; i < cnt; ++i) {
		val = SMIES_GAMMALUT_EVEN_VAL(*gamma_b);
		gamma_b++;
		if (i == (cnt - 1)) {
			smies_write(smies, SMIESGAMMALUT_B + i * 4, val);
		} else {
			val |= SMIES_GAMMALUT_ODD_VAL(*gamma_b);
			smies_write(smies, SMIESGAMMALUT_B + i * 4, val);
			gamma_b++;
		}
	}

	return 0;
}

static void smies_all_reset(struct s5p_smies_device *smies)
{
	smies_write_mask(smies, SMIESCON, ~0, SMIES_CON_ALL_RESET);
}

static void smies_set_update_reg_mask(struct s5p_smies_device *smies,
		enum SMIES_MASK mask)
{
	u32 val = (mask == SMIES_MASK_ON) ? ~0: 0;

	smies_write_mask(smies, SMIESCON_MASK, val, SMIES_CON_MASK_CTRL);
}

static void smies_set_update_reg_direct(struct s5p_smies_device *smies, int on)
{
	u32 val = on ? ~0 : 0;

	smies_write_mask(smies, SMIESCON, val, SMIES_CON_UPDATE_NO_COND);
}

static void smies_set_dither(struct s5p_smies_device *smies, int on)
{
	u32 val = on ? ~0 : 0;

	smies_write_mask(smies, SMIESCON, val, SMIES_CON_DITHER_ON);
}

static void smies_set_sae(struct s5p_smies_device *smies, int on)
{
	u32 val = on ? ~0 : 0;

	smies_write_mask(smies, SMIESCON, val, SMIES_CON_SAE_ON);
}

static void smies_set_scr(struct s5p_smies_device *smies, int on)
{
	u32 val = on ? ~0 : 0;

	smies_write_mask(smies, SMIESCON, val, SMIES_CON_SCR_ON);
}

static void smies_set_gamma(struct s5p_smies_device *smies, int on)
{
	u32 val = on ? ~0 : 0;

	smies_write_mask(smies, SMIESCON, val, SMIES_CON_GAMMA_ON);
}

static void smies_run(struct s5p_smies_device *smies, int on)
{
	u32 val = on ? ~0 : 0;

	smies_write_mask(smies, SMIESCON, val, SMIES_CON_SMIES_ON);
}

static void smies_set_interface(struct s5p_smies_device *smies, enum SMIES_IF interface)
{
	u32 val;

	val = interface ? ~0 : 0;
	smies_write_mask(smies, SMIESCON, val, SMIES_CON_I80_MODE);
}

static void smies_set_path(struct s5p_smies_device *smies, int on)
{
	u32 val;

	if (on) { /* fimd -> smies -> dsim */
		val = readl(SYSREG_LCDBLK_CFG);
		val &= ~LCDBLK_CFG_FIMDBYPASS;
	} else {  /* fimd -> dsim */
		val = readl(SYSREG_LCDBLK_CFG);
		val |= LCDBLK_CFG_FIMDBYPASS;
	}

	writel(val, SYSREG_LCDBLK_CFG);
	dev_dbg(smies->dev, "SYSREG_LCDBLK_CFG(0x%x)\n", readl(SYSREG_LCDBLK_CFG));
}

int smies_disable_by_fimd(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s5p_smies_device *smies = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s +\n", __func__);

	mutex_lock(&smies->mutex);

	if(smies->state == SMIES_ENABLED){
		smies_set_update_reg_mask(smies, SMIES_MASK_ON);
		smies_run(smies, 0);
		smies_set_update_reg_mask(smies, SMIES_MASK_OFF);
		smies_set_path(smies, 0);
		smies->state = SMIES_DISABLED;
		pm_runtime_put_sync(smies->dev);
	}

	mutex_unlock(&smies->mutex);

	dev_dbg(dev, "%s -\n", __func__);

	return 0;
}

int smies_enable_by_fimd(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s5p_smies_device *smies = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s: state(%d)\n", __func__, smies->state);

	mutex_lock(&smies->mutex);

	if (smies->state == SMIES_ENABLED) {
		dev_info(dev, "already enabled\n");
		mutex_unlock(&smies->mutex);
		return -EBUSY;
	}

	pm_runtime_get_sync(smies->dev);

	smies_set_path(smies, 1);
	smies_all_reset(smies);

	smies_set_update_reg_mask(smies, SMIES_MASK_ON);

	smies_set_core_clk_gate(smies, 1);
	smies_set_update_reg_direct(smies, 0);

	smies_set_interface(smies, SMIES_I80_IF);
	smies_set_image_size(smies);
	smies_sae_config(smies);
	smies_scr_cofig(smies);
	smies_gamma_lut_config(smies, SMIES_CUV_LINEAR);

	smies_set_sae(smies, smies->pdata->sae_on);
	smies_set_scr(smies, smies->pdata->scr_on);
	smies_set_gamma(smies, smies->pdata->gamma_on);
	smies_set_dither(smies, smies->pdata->dither_on);

	smies_set_crc(smies, 0);

	smies_run(smies, 1);
	smies_set_update_reg_mask(smies, SMIES_MASK_OFF);
	smies->state = SMIES_ENABLED;

	mutex_unlock(&smies->mutex);

	smies_dump_registers(smies);
	dev_dbg(dev, "%s -\n", __func__);

	return 0;
}

static int __devinit smies_probe(struct platform_device *pdev)
{

	struct device *dev = &pdev->dev;
	struct s5p_smies_platdata *pd = dev->platform_data;
	struct s5p_smies_device *smies;
	struct resource *res;
	int ret;

	dev_dbg(dev, "probe start\n");

	smies = kzalloc(sizeof(struct s5p_smies_device), GFP_KERNEL);
	if (!smies) {
		dev_err(dev, "no memory for SMIES\n");
		ret = -ENOMEM;
		goto err;
	}

	smies->dev = dev;
	smies->pdata = pd;

	spin_lock_init(&smies->slock);
	mutex_init(&smies->mutex);

	smies->clock = clk_get(NULL, SMIES_CLK_NAME);
	if (IS_ERR_OR_NULL(smies->clock)) {
		dev_err(dev, "failed to get smies clock\n");
		ret = -ENXIO;
		goto err_clk_get;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "failed to get io memory region\n");
		ret = -ENXIO;
		goto err_platform_get;
	}

	res = request_mem_region(res->start, resource_size(res), dev_name(dev));
	if (!res) {
		dev_err(dev, "failed to request io memory region\n");
		ret = -ENXIO;
		goto err_platform_get;
	}

	smies->res = res;
	smies->reg_base = ioremap(res->start, resource_size(res));
	if (!smies->reg_base) {
		dev_err(dev, "failed to remap io region\n");
		ret = -ENXIO;
		goto err_ioremap;
	}

	pm_runtime_enable(dev);

	smies->state = SMIES_INITIALIZED;

	platform_set_drvdata(pdev, smies);

#if defined(CONFIG_FB_SMIES_ENABLE_BOOTTIME)
	ret = smies_enable_by_fimd(dev);
	if (!ret)
		dev_info(dev, "SMIES enabled\n");
#endif
	dev_info(dev, "probe succeeded\n");

	return 0;

err_ioremap:
	release_resource(smies->res);
err_platform_get:
	clk_put(smies->clock);
err_clk_get:
	kfree(smies);
err:
	dev_err(dev, "probe failed\n");
	return ret;

}

static int __devexit smies_remove(struct platform_device *pdev)
{
	struct s5p_smies_device *smies = platform_get_drvdata(pdev);

	pm_runtime_disable(smies->dev);
	iounmap(smies->reg_base);
	release_resource(smies->res);
	clk_put(smies->clock);
	mutex_destroy(&smies->mutex);
	kfree(smies);

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int smies_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s5p_smies_device *smies = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s +\n", __func__);
	clk_disable(smies->clock);
	dev_dbg(dev, "%s -\n", __func__);

	return 0;
}

static int smies_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s5p_smies_device *smies = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s +\n", __func__);
	clk_enable(smies->clock);
	dev_dbg(dev, "%s -\n", __func__);

	return 0;
}
#endif

static const struct dev_pm_ops smies_pm_ops = {
#ifdef CONFIG_PM_RUNTIME
	SET_RUNTIME_PM_OPS(smies_runtime_suspend, smies_runtime_resume, NULL)
#endif
};

static struct platform_driver s5p_smies_driver = {
	.probe		= smies_probe,
	.remove		= __devexit_p(smies_remove),
	.driver		= {
		.name	= "s5p-smies",
		.owner	= THIS_MODULE,
		.pm	= &smies_pm_ops,
	},
};

static int __init smies_init(void)
{
	int ret;

	static const char banner[] __initdata = KERN_INFO
		"Samsung MIE driver\n";
	printk(banner);

	ret = platform_driver_register(&s5p_smies_driver);
	if (ret != 0) {
		printk(KERN_ERR "registration of SMIES driver failed\n");
		return -ENXIO;
	}

	return 0;
}

static void __exit smies_exit(void)
{
	platform_driver_unregister(&s5p_smies_driver);
}
module_init(smies_init);
module_exit(smies_exit);

MODULE_AUTHOR("dshj <shaojie.dong@samsung.com>");;
MODULE_DESCRIPTION("Samsung S3C SoC SMIES driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:s5p-smies");
