/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 * Authors: Thomas Abraham <thomas.ab@samsung.com>
 *	    Chander Kashyap <k.chander@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Common Clock Framework support for Exynos7570 SoC.
*/

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <soc/samsung/ect_parser.h>

#include <dt-bindings/clock/exynos7570.h>
#include "../../soc/samsung/pwrcal/S5E7570/S5E7570-vclk.h"
#include "composite.h"

enum exynos7570_clks {
	none,

	oscclk = 1,

	/* mif group */
	mif_adcif = 10, mif_speedy,
	mif_uart_debug = 20, mif_uart_sensor, mif_spi_rearfrom, mif_spi_ese,
	mif_mmc0 = 30, mif_mmc2, mif_usb20drd_refclk,
	mif_usi0 = 40, mif_usi1, mif_apm, mif_isp_sensor0,
	mif_pdma0 = 50,

	/* cpucl0 group */
	cpucl0_bcm = 100, cpucl0_bts,

	/* g3d group */
	g3d_sysmmu = 200, g3d_bcm, g3d_bts, gate_g3d,

	/* peri group */
	peri_i2c_sensor1 = 300, peri_i2c_sensor2, peri_i2c_tsp,
	peri_i2c_fuelgauge = 310, peri_i2c_nfc, peri_i2c_muic,
	peri_hsi2c_frontsensor = 320, peri_hsi2c_rearsensor, peri_hsi2c_rearaf,
	peri_pwm_motor = 330, peri_sclk_pwm_motor, peri_hsi2c_maincam, peri_hsi2c_frontcam,
	peri_gpio_touch = 340, peri_gpio_top, peri_gpio_nfc, peri_gpio_ese,
	peri_gpio_alive = 350, peri_usi0, peri_usi1, peri_otp_con_top,
	peri_uart_debug = 360, peri_uart_sensor, peri_spi_ese, peri_spi_rearfrom,
	peri_mct = 370, peri_wdt_cpucl0, peri_tmu_cpucl0, peri_chipid, peri_rtc_alive, peri_rtc_top,

	/* fsys group */
	fsys_sysmmu = 500, fsys_bcm, fsys_bts,
	fsys_sss = 510, fsys_rtic, fsys_usb20drd, fsys_mmc0, fsys_mmc2, fsys_sclk_mmc0, fsys_sclk_mmc2,
	fsys_usb20drd_phyclock_user = 520,
	wpll_usb = 530,

	/* mfcmscl group */
	mfcmscl_bcm = 601, mfcmscl_bts,
	gate_mscl = 610, gate_jpeg, gate_mfc,

	/* dispaud group */
	dispaud_bcm = 701, dispaud_bts, dispaud_decon, dispaud_dsim0,
	dispaud_mixer = 710, dispaud_mi2s_aud, dispaud_mi2s_amp,
	dispaud_mipiphy_txbyteclkhs_user = 720,
	dispaud_mipiphy_rxclkesc0_user,
	decon_vclk = 730, decon_vclk_local,
	aud_pll = 740, mi2s_div, mixer_div, oscclk_fm_52m_div,

	/* isp group */
	isp_bcm = 801, isp_bts, isp_cam, isp_vra,
	isp_s_rxbyteclkhs0_s4_user = 810,

	/* apm group */
	gate_apm = 900,

	/* number of dfs driver starts from 1000 */
	dfs_mif = 1000, dfs_mif_sw, dfs_int, dfs_cam, dfs_disp,

	/* number for clkout port starts from 1050 */
	oscclk_aud = 1050,

	/* clk id for sysmmu: 1100 ~ 1149
	 * NOTE: clock IDs of sysmmus are defined in
	 * include/dt-bindings/clock/exynos8890.h
	 */
	sysmmu_last = 1149,

	nr_clks,
};

/* fixed rate clocks generated outside the soc */
static struct samsung_fixed_rate exynos7570_fixed_rate_ext_clks[] __initdata = {
	FRATE(oscclk, "fin_pll", NULL, CLK_IS_ROOT, 26000000),
};

static struct init_vclk exynos7570_mif_vclks[] __initdata = {
	VCLK(mif_adcif, gate_mif_adcif, "gate_mif_adcif", 0, 0, NULL),
	VCLK(mif_speedy, gate_mif_speedy, "gate_mif_speedy", 0, 0, NULL),
	VCLK(mif_pdma0, gate_mif_pdma, "gate_mif_pdma", 0, 0, NULL),

	/* sclk */
	VCLK(mif_uart_debug, sclk_uart_debug, "sclk_uart_debug", 0, 0, NULL),
	VCLK(mif_uart_sensor, sclk_uart_sensor, "sclk_uart_sensor", 0, 0, NULL),
	VCLK(mif_spi_rearfrom, sclk_spi_rearfrom, "sclk_spi_rearfrom", 0, 0, NULL),
#ifdef CONFIG_SENSORS_FINGERPRINT
	VCLK(mif_spi_ese, sclk_spi_ese, "sclk_spi_ese", 0, 0, "fp-spi-sclk"),
#else
	VCLK(mif_spi_ese, sclk_spi_ese, "sclk_spi_ese", 0, 0, NULL),
#endif
	VCLK(mif_mmc0, sclk_mmc0, "sclk_mmc0", 0, 0, NULL),
	VCLK(mif_mmc2, sclk_mmc2, "sclk_mmc2", 0, 0, NULL),
	VCLK(mif_usb20drd_refclk, sclk_usb20drd_refclk, "sclk_usb20drd_refclk", 0, 0, NULL),
	VCLK(mif_usi0, sclk_usi0, "sclk_usi0", 0, 0, NULL),
	VCLK(mif_usi1, sclk_usi1, "sclk_usi1", 0, 0, NULL),
	VCLK(mif_apm, sclk_apm, "sclk_apm", 0, 0, NULL),
	VCLK(mif_isp_sensor0, sclk_isp_sensor0, "sclk_isp_sensor0", 0, 0, NULL),
};

static struct init_vclk exynos7570_g3d_vclks[] __initdata = {
	VCLK(g3d_sysmmu, gate_g3d_sysmmu, "gate_g3d_sysmmu", 0, 0, NULL),
	VCLK(g3d_bcm, gate_g3d_bcm, "gate_g3d_bcm", 0, 0, "gate_g3d_bcm"),
	VCLK(g3d_bts, gate_g3d_bts, "gate_g3d_bts", 0, 0, NULL),
	VCLK(gate_g3d, gate_g3d_g3d, "gate_g3d_g3d", 0, 0, NULL),
};

static struct init_vclk exynos7570_peri_vclks[] __initdata = {
	VCLK(peri_i2c_sensor1, gate_peri_i2c_sensor1, "gate_peri_i2c_sensor1", 0, 0, NULL),
	VCLK(peri_i2c_sensor2, gate_peri_i2c_sensor2, "gate_peri_i2c_sensor2", 0, 0, NULL),
	VCLK(peri_i2c_tsp, gate_peri_i2c_tsp, "gate_peri_i2c_tsp", 0, 0, NULL),
	VCLK(peri_i2c_fuelgauge, gate_peri_i2c_fuelgauge, "gate_peri_i2c_fuelgauge", 0, 0, NULL),
	VCLK(peri_i2c_nfc, gate_peri_i2c_nfc, "gate_peri_i2c_nfc", 0, 0, NULL),
	VCLK(peri_i2c_muic, gate_peri_i2c_muic, "gate_peri_i2c_muic", 0, 0, NULL),
	VCLK(peri_hsi2c_frontsensor, gate_peri_hsi2c_frontsensor, "gate_peri_hsi2c_frontsensor", 0, 0, NULL),
	VCLK(peri_hsi2c_rearsensor, gate_peri_hsi2c_rearsensor, "gate_peri_hsi2c_rearsensor", 0, 0, NULL),
	VCLK(peri_hsi2c_rearaf, gate_peri_hsi2c_rearaf, "gate_peri_hsi2c_rearaf", 0, 0, NULL),
	VCLK(peri_pwm_motor, gate_peri_pwm_motor, "gate_peri_pwm_motor", 0, 0, NULL),
	VCLK(peri_sclk_pwm_motor, gate_peri_sclk_pwm_motor, "gate_peri_sclk_pwm_motor", 0, 0, NULL),
	VCLK(peri_hsi2c_maincam, gate_peri_hsi2c_maincam, "gate_peri_hsi2c_maincam", 0, 0, NULL),
	VCLK(peri_hsi2c_frontcam, gate_peri_hsi2c_frontcam, "gate_peri_hsi2c_frontcam", 0, 0, NULL),
	VCLK(peri_gpio_touch, gate_peri_gpio_touch, "gate_peri_gpio_touch", 0, 0, NULL),
	VCLK(peri_gpio_top, gate_peri_gpio_top, "gate_peri_gpio_top", 0, 0, NULL),
	VCLK(peri_gpio_nfc, gate_peri_gpio_nfc, "gate_peri_gpio_nfc", 0, 0, NULL),
	VCLK(peri_gpio_ese, gate_peri_gpio_ese, "gate_peri_gpio_ese", 0, 0, NULL),
	VCLK(peri_gpio_alive, gate_peri_gpio_alive, "gate_peri_gpio_alive", 0, 0, NULL),
	VCLK(peri_usi0, gate_peri_usi0, "gate_peri_usi0", 0, 0, NULL),
	VCLK(peri_usi1, gate_peri_usi1, "gate_peri_usi1", 0, 0, NULL),
	VCLK(peri_otp_con_top, gate_peri_otp_con_top, "gate_peri_otp_con_top", 0, 0, NULL),
	VCLK(peri_uart_debug, gate_peri_uart_debug, "gate_peri_uart_debug", 0, 0, NULL),
	VCLK(peri_uart_sensor, gate_peri_uart_sensor, "gate_peri_uart_sensor", 0, 0, NULL),
#ifdef CONFIG_SENSORS_FINGERPRINT
	VCLK(peri_spi_ese, gate_peri_spi_ese, "gate_peri_spi_ese", 0, 0, "fp-spi-pclk"),
#else
	VCLK(peri_spi_ese, gate_peri_spi_ese, "gate_peri_spi_ese", 0, 0, NULL),
#endif
	VCLK(peri_spi_rearfrom, gate_peri_spi_rearfrom, "gate_peri_spi_rearfrom", 0, 0, NULL),
	VCLK(peri_mct, gate_peri_mct, "gate_peri_mct", 0, 0, NULL),
	VCLK(peri_wdt_cpucl0, gate_peri_wdt_cpucl0, "gate_peri_wdt_cpucl0", 0, 0, NULL),
	VCLK(peri_tmu_cpucl0, gate_peri_tmu_cpucl0, "gate_peri_tmu_cpucl0", 0, 0, NULL),
	VCLK(peri_chipid, gate_peri_chipid, "gate_peri_chipid", 0, 0, NULL),
	VCLK(peri_rtc_alive, gate_peri_rtc_alive, "gate_peri_rtc_alive", 0, 0, NULL),
	VCLK(peri_rtc_top, gate_peri_rtc_top, "gate_peri_rtc_top", 0, 0, NULL),
};

static struct init_vclk exynos7570_fsys_vclks[] __initdata = {
	VCLK(fsys_sysmmu, gate_fsys_sysmmu, "gate_fsys_sysmmu", 0, 0, NULL),
	VCLK(fsys_bcm, gate_fsys_bcm, "gate_fsys_bcm", 0, 0, "gate_fsys_bcm"),
	VCLK(fsys_bts, gate_fsys_bts, "gate_fsys_bts", 0, 0, NULL),
	VCLK(fsys_sss, gate_fsys_sss, "gate_fsys_sss", 0, 0, NULL),
	VCLK(fsys_rtic, gate_fsys_rtic, "gate_fsys_rtic", 0, 0, NULL),
	VCLK(fsys_usb20drd, gate_fsys_usb20drd, "gate_fsys_usb20drd", 0, 0, NULL),
	VCLK(fsys_mmc0, gate_fsys_mmc0, "gate_fsys_mmc0", 0, 0, NULL),
	VCLK(fsys_mmc2, gate_fsys_mmc2, "gate_fsys_mmc2", 0, 0, NULL),
	VCLK(fsys_sclk_mmc0, gate_fsys_sclk_mmc0, "gate_fsys_sclk_mmc0", 0, 0, NULL),
	VCLK(fsys_sclk_mmc2, gate_fsys_sclk_mmc2, "gate_fsys_sclk_mmc2", 0, 0, NULL),
	VCLK(fsys_usb20drd_phyclock_user, umux_fsys_clkphy_fsys_usb20drd_phyclock_user, \
			"umux_fsys_clkphy_fsys_usb20drd_phyclock_user", 0, 0, NULL),
	VCLK(wpll_usb, p1_wpll_usb_pll, "p1_wpll_usb_pll", 0, 0, NULL),
};

static struct init_vclk exynos7570_mfcmscl_vclks[] __initdata = {
	VCLK(CLK_VCLK_SYSMMU_MFC_MSCL, gate_mfcmscl_sysmmu, "gate_mfcmscl_sysmmu", 0, 0, NULL),
	VCLK(mfcmscl_bcm, gate_mfcmscl_bcm, "gate_mfcmscl_bcm", 0, 0, "gate_mfcmscl_bcm"),
	VCLK(mfcmscl_bts, gate_mfcmscl_bts, "gate_mfcmscl_bts", 0, 0, NULL),
	VCLK(gate_mscl, gate_mfcmscl_mscl, "gate_mfcmscl_mscl", 0, 0, NULL),
	VCLK(gate_jpeg, gate_mfcmscl_jpeg, "gate_mfcmscl_jpeg", 0, 0, NULL),
	VCLK(gate_mfc, gate_mfcmscl_mfc, "gate_mfcmscl_mfc", 0, 0, NULL),
};

static struct init_vclk exynos7570_dispaud_vclks[] __initdata = {
	VCLK(CLK_VCLK_SYSMMU_DISP_AUD, gate_dispaud_sysmmu, "gate_dispaud_sysmmu", 0, 0, NULL),
	VCLK(dispaud_bcm, gate_dispaud_bcm, "gate_dispaud_bcm", 0, 0, "gate_dispaud_bcm"),
	VCLK(dispaud_bts, gate_dispaud_bts, "gate_dispaud_bts", 0, 0, NULL),
	VCLK(dispaud_decon, gate_dispaud_decon, "gate_dispaud_decon", 0, 0, NULL),
	VCLK(dispaud_dsim0, gate_dispaud_dsim0, "gate_dispaud_dsim0", 0, 0, NULL),
	VCLK(dispaud_mixer, gate_dispaud_mixer, "gate_dispaud_mixer", 0, 0, NULL),
	VCLK(dispaud_mi2s_aud, gate_dispaud_mi2s_aud, "gate_dispaud_mi2s_aud", 0, 0, NULL),
	VCLK(dispaud_mi2s_amp, gate_dispaud_mi2s_amp, "gate_dispaud_mi2s_amp", 0, 0, NULL),
	VCLK(dispaud_mipiphy_txbyteclkhs_user, umux_dispaud_clkphy_dispaud_mipiphy_txbyteclkhs_user, \
			"umux_dispaud_clkphy_dispaud_mipiphy_txbyteclkhs_user", 0, 0, NULL),
	VCLK(dispaud_mipiphy_rxclkesc0_user, umux_dispaud_clkphy_dispaud_mipiphy_rxclkesc0_user, \
			"umux_dispaud_clkphy_dispaud_mipiphy_rxclkesc0_user", 0, 0, NULL),
	/* sclk */
	VCLK(decon_vclk, sclk_decon_vclk, "sclk_decon_vclk", 0, 0, NULL),
	VCLK(decon_vclk_local, sclk_decon_vclk_local, "sclk_decon_vclk_local", 0, 0, NULL),
	VCLK(aud_pll, p1_aud_pll, "p1_aud_pll", 0, 0, NULL),
	VCLK(mi2s_div, d1_dispaud_mi2s, "d1_dispaud_mi2s", 0, 0, NULL),
	VCLK(mixer_div, d1_dispaud_mixer, "d1_dispaud_mixer", 0, 0, NULL),
	VCLK(oscclk_fm_52m_div, d1_dispaud_oscclk_fm_52m, "d1_dispaud_oscclk_fm_52m", 0, 0, NULL),
};

static struct init_vclk exynos7570_isp_vclks[] __initdata = {
	VCLK(CLK_VCLK_SYSMMU_ISP, gate_isp_sysmmu, "gate_isp_sysmmu", 0, 0, NULL),
	VCLK(isp_bcm, gate_isp_bcm, "gate_isp_bcm", 0, 0, "gate_isp_bcm"),
	VCLK(isp_bts, gate_isp_bts, "gate_isp_bts", 0, 0, NULL),
	VCLK(isp_cam, gate_isp_cam, "gate_isp_cam", 0, 0, NULL),
	VCLK(isp_vra, gate_isp_vra, "gate_isp_vra", 0, 0, NULL),
	VCLK(isp_s_rxbyteclkhs0_s4_user, umux_isp_clkphy_isp_s_rxbyteclkhs0_s4_user, \
			"umux_isp_clkphy_isp_s_rxbyteclkhs0_s4_user", 0, 0, NULL),
};

static struct init_vclk exynos7570_apm_vclks[] __initdata = {
	VCLK(gate_apm, gate_apm_apm, "gate_apm_apm", 0, 0, NULL),
};

static struct of_device_id ext_clk_match[] __initdata = {
	{ .compatible = "samsung,exynos7570-oscclk", .data = (void *)0, },
};

static struct init_vclk exynos7570_dfs_vclks[] __initdata = {
	/* DFS */
	VCLK(dfs_mif, dvfs_mif, "dvfs_mif", 0, VCLK_DFS, NULL),
	VCLK(dfs_mif_sw, dvfs_mif, "dvfs_mif_sw", 0, VCLK_DFS_SWITCH, NULL),
	VCLK(dfs_int, dvfs_int, "dvfs_int", 0, VCLK_DFS, NULL),
	VCLK(dfs_cam, dvfs_cam, "dvfs_cam", 0, VCLK_DFS, NULL),
	VCLK(dfs_disp, dvfs_disp, "dvfs_disp", 0, VCLK_DFS, NULL),
};

static struct init_vclk exynos7570_clkout_vclks[] __initdata = {
	VCLK(oscclk_aud, pxmxdx_oscclk_aud, "pxmxdx_oscclk_aud", 0, 0, NULL),
};

/* register exynos7570 clocks */
void __init exynos7570_clk_init(struct device_node *np)
{
	struct samsung_clk_provider *ctx;
	void __iomem *reg_base;

	if (np) {
		reg_base = of_iomap(np, 0);
		if (!reg_base)
			panic("%s: failed to map registers\n", __func__);
	} else {
		panic("%s: unable to determine soc\n", __func__);
	}

	ect_parse_binary_header();

	if (cal_init())
		pr_err("%s: unable to initialize power cal\n", __func__);
	else
		pr_info("%s: Exynos power cal initialized\n", __func__);

	ctx = samsung_clk_init(np, reg_base, nr_clks);
	if (!ctx)
		panic("%s: unable to allocate context.\n", __func__);

	samsung_register_of_fixed_ext(ctx, exynos7570_fixed_rate_ext_clks,
			ARRAY_SIZE(exynos7570_fixed_rate_ext_clks), ext_clk_match);

	samsung_register_vclk(ctx, exynos7570_mif_vclks, ARRAY_SIZE(exynos7570_mif_vclks));
	samsung_register_vclk(ctx, exynos7570_g3d_vclks, ARRAY_SIZE(exynos7570_g3d_vclks));
	samsung_register_vclk(ctx, exynos7570_peri_vclks, ARRAY_SIZE(exynos7570_peri_vclks));
	samsung_register_vclk(ctx, exynos7570_fsys_vclks, ARRAY_SIZE(exynos7570_fsys_vclks));
	samsung_register_vclk(ctx, exynos7570_mfcmscl_vclks, ARRAY_SIZE(exynos7570_mfcmscl_vclks));
	samsung_register_vclk(ctx, exynos7570_dispaud_vclks, ARRAY_SIZE(exynos7570_dispaud_vclks));
	samsung_register_vclk(ctx, exynos7570_isp_vclks, ARRAY_SIZE(exynos7570_isp_vclks));
	samsung_register_vclk(ctx, exynos7570_apm_vclks, ARRAY_SIZE(exynos7570_apm_vclks));
	samsung_register_vclk(ctx, exynos7570_dfs_vclks, ARRAY_SIZE(exynos7570_dfs_vclks));
	samsung_register_vclk(ctx, exynos7570_clkout_vclks, ARRAY_SIZE(exynos7570_clkout_vclks));

	samsung_clk_of_add_provider(np, ctx);

	clk_register_fixed_factor(NULL, "pwm-clock", "gate_peri_sclk_pwm_motor",CLK_SET_RATE_PARENT, 1, 1);

	pr_info("Exynos clock initialized\n");
}
CLK_OF_DECLARE(exynos7570_clk, "samsung,exynos7570-clock", exynos7570_clk_init);
