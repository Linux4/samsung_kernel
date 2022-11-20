/* linux/arch/arm/mach-exynos/setup-fimc-is2-sensor.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * FIMC-IS gpio and clock configuration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <plat/map-s5p.h>
#include <plat/cpu.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#if defined(CONFIG_SOC_EXYNOS3475)
#include <mach/regs-clock-exynos3475.h>
#endif

#include <mach/exynos-fimc-is2.h>
#include <mach/exynos-fimc-is2-sensor.h>
#include <mach/exynos-fimc-is2-module.h>

char *clk_g_list[CLK_NUM] = {
	"cam_pll",
	"isp_pll",
	"dout_aclk_cam0_3aa0_690",
	"dout_aclk_cam0_3aa1_468",
	"dout_aclk_cam0_bnsa_690",
	"dout_aclk_cam0_bnsb_690",
	"dout_aclk_cam0_bnsd_690",
	"dout_aclk_cam0_csis0_690",
	"dout_aclk_cam0_csis1_174",
	"dout_aclk_cam0_nocp_133",
	"dout_aclk_cam0_trex_532",
	"dout_aclk_cam1_arm_668",
	"dout_aclk_cam1_bnscsis_133",
	"dout_aclk_cam1_busperi_334",
	"dout_aclk_cam1_nocp_133",
	"dout_aclk_cam1_sclvra_491",
	"dout_aclk_cam1_trex_532",
	"dout_aclk_isp0_isp0_590",
	"dout_aclk_isp0_tpu_590",
	"dout_aclk_isp0_trex_532",
	"dout_aclk_isp1_ahb_117",
	"dout_aclk_isp1_isp1_468",
	"dout_clkdiv_pclk_cam0_3aa0_345",
	"dout_clkdiv_pclk_cam0_3aa1_234",
	"dout_clkdiv_pclk_cam0_bnsa_345",
	"dout_clkdiv_pclk_cam0_bnsb_345",
	"dout_clkdiv_pclk_cam0_bnsd_345",
	"dout_clkdiv_pclk_cam0_trex_133",
	"dout_clkdiv_pclk_cam0_trex_266",
	"dout_clkdiv_pclk_cam1_arm_167",
	"dout_clkdiv_pclk_cam1_busperi_167",
	"dout_clkdiv_pclk_cam1_busperi_84",
	"dout_clkdiv_pclk_cam1_sclvra_246",
	"dout_clkdiv_pclk_isp0_isp0_295",
	"dout_clkdiv_pclk_isp0_tpu_295",
	"dout_clkdiv_pclk_isp0_trex_133",
	"dout_clkdiv_pclk_isp0_trex_266",
	"dout_clkdiv_pclk_isp1_isp1_234",
	"dout_sclk_isp_spi0",
	"dout_sclk_isp_spi1",
	"dout_sclk_isp_uart",
	"gate_aclk_csis0_i_wrap",
	"gate_aclk_csis1_i_wrap",
	"gate_aclk_csis3_i_wrap",
	"gate_aclk_fimc_bns_a",
	"gate_aclk_fimc_bns_b",
	"gate_aclk_fimc_bns_c",
	"gate_aclk_fimc_bns_d",
	"gate_aclk_lh_cam0",
	"gate_aclk_lh_cam1",
	"gate_aclk_lh_isp",
	"gate_aclk_noc_bus0_nrt",
	"gate_aclk_wrap_csis2",
	"gate_cclk_asyncapb_socp_fimc_bns_a",
	"gate_cclk_asyncapb_socp_fimc_bns_b",
	"gate_cclk_asyncapb_socp_fimc_bns_c",
	"gate_cclk_asyncapb_socp_fimc_bns_d",
	"gate_pclk_asyncapb_socp_fimc_bns_a",
	"gate_pclk_asyncapb_socp_fimc_bns_b",
	"gate_pclk_asyncapb_socp_fimc_bns_c",
	"gate_pclk_asyncapb_socp_fimc_bns_d",
	"gate_pclk_csis0",
	"gate_pclk_csis1",
	"gate_pclk_csis2",
	"gate_pclk_csis3",
	"gate_pclk_fimc_bns_a",
	"gate_pclk_fimc_bns_b",
	"gate_pclk_fimc_bns_c",
	"gate_pclk_fimc_bns_d",
	"mout_user_mux_aclk_cam0_3aa0_690",
	"mout_user_mux_aclk_cam0_3aa1_468",
	"mout_user_mux_aclk_cam0_bnsa_690",
	"mout_user_mux_aclk_cam0_bnsb_690",
	"mout_user_mux_aclk_cam0_bnsd_690",
	"mout_user_mux_aclk_cam0_csis0_690",
	"mout_user_mux_aclk_cam0_csis1_174",
	"mout_user_mux_aclk_cam0_nocp_133",
	"mout_user_mux_aclk_cam0_trex_532",
	"mout_user_mux_aclk_cam1_arm_668",
	"mout_user_mux_aclk_cam1_bnscsis_133",
	"mout_user_mux_aclk_cam1_busperi_334",
	"mout_user_mux_aclk_cam1_nocp_133",
	"mout_user_mux_aclk_cam1_sclvra_491",
	"mout_user_mux_aclk_cam1_trex_532",
	"mout_user_mux_aclk_isp0_isp0_590",
	"mout_user_mux_aclk_isp0_tpu_590",
	"mout_user_mux_aclk_isp0_trex_532",
	"mout_user_mux_aclk_isp1_ahb_117",
	"mout_user_mux_aclk_isp1_isp1_468",
	"mout_user_mux_phyclk_hs0_csis2_rx_byte",
	"mout_user_mux_phyclk_rxbyteclkhs0_s2a",
	"mout_user_mux_phyclk_rxbyteclkhs0_s4",
	"mout_user_mux_phyclk_rxbyteclkhs1_s4",
	"mout_user_mux_phyclk_rxbyteclkhs2_s4",
	"mout_user_mux_phyclk_rxbyteclkhs3_s4",
	"mout_user_mux_sclk_isp_spi0",
	"mout_user_mux_sclk_isp_spi1",
	"mout_user_mux_sclk_isp_uart",
	"phyclk_hs0_csis2_rx_byte",
	"phyclk_rxbyteclkhs0_s2a",
	"phyclk_rxbyteclkhs0_s4",
	"phyclk_rxbyteclkhs1_s4",
	"phyclk_rxbyteclkhs2_s4",
	"phyclk_rxbyteclkhs3_s4",
};

struct clk *clk_target_list[CLK_NUM];

#if defined(CONFIG_SOC_EXYNOS3475)
int exynos3475_fimc_is_sensor_iclk_cfg(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	/*TODO*/

	return 0;
}

int exynos3475_fimc_is_sensor_iclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	pr_debug("%s\n", __func__);

	if (scenario == SENSOR_SCENARIO_NORMAL) {
		/* phy clock on */
		fimc_is_enable_dt(pdev, "phyclk_csi_link0_rx_byte_clk_hs0");
	} else {
		/* phy clock, bns and csi clock on for external */
		fimc_is_enable_dt(pdev, "phyclk_csi_link0_rx_byte_clk_hs0");
		fimc_is_enable_dt(pdev, "aclk_isp_300_aclk_isp_d");
	}

	return 0;
}

int exynos3475_fimc_is_sensor_iclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	pr_debug("%s\n", __func__);

	if (scenario == SENSOR_SCENARIO_NORMAL) {
		/* phy clock off */
		fimc_is_disable_dt(pdev, "phyclk_csi_link0_rx_byte_clk_hs0");
	} else {
		/* phy clock, isp clock off for external */
		fimc_is_disable_dt(pdev, "phyclk_csi_link0_rx_byte_clk_hs0");
		fimc_is_disable_dt(pdev, "aclk_isp_300_aclk_isp_d");
	}

	return 0;
}

int exynos3475_fimc_is_sensor_mclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	char mux_name[30];
	char div_a_name[30];
	char div_b_name[30];
	char sclk_name[30];

	pr_debug("%s\n", __func__);

	snprintf(mux_name, sizeof(mux_name), "mout_sclk_isp_sensor%d", channel);
	snprintf(sclk_name, sizeof(sclk_name), "sclk_isp_sensor%d", channel);

	/* set parent to set oscclk */
	fimc_is_set_parent_dt(pdev, mux_name, "oscclk");

	/* set div max other channel mclk */
	snprintf(div_a_name, sizeof(div_a_name), "dout_sclk_isp_sensor%d_a", (channel == 1) ? 0 : 1);
	snprintf(div_b_name, sizeof(div_b_name), "dout_sclk_isp_sensor%d_b", (channel == 1) ? 0 : 1);

	fimc_is_set_rate_dt(pdev, div_a_name, 1);
	fimc_is_set_rate_dt(pdev, div_b_name, 1);

	/* set divider ratio to 1 ratio */
	snprintf(div_a_name, sizeof(div_a_name), "dout_sclk_isp_sensor%d_a", channel);
	snprintf(div_b_name, sizeof(div_b_name), "dout_sclk_isp_sensor%d_b", channel);

	fimc_is_set_rate_dt(pdev, div_a_name, 26 * 1000000);
	fimc_is_set_rate_dt(pdev, div_b_name, 26 * 1000000);

	fimc_is_enable_dt(pdev, sclk_name);

	return 0;
}

int exynos3475_fimc_is_sensor_mclk_off(struct platform_device *pdev,
		u32 scenario,
		u32 channel)
{
	char div_a_name[30];
	char div_b_name[30];
	char sclk_name[30];

	pr_debug("%s\n", __func__);

	snprintf(div_a_name, sizeof(div_a_name), "dout_sclk_isp_sensor%d_a", channel);
	snprintf(div_b_name, sizeof(div_b_name), "dout_sclk_isp_sensor%d_b", channel);
	snprintf(sclk_name, sizeof(sclk_name), "sclk_isp_sensor%d", channel);

	fimc_is_set_rate_dt(pdev, div_a_name, 1);
	fimc_is_set_rate_dt(pdev, div_b_name, 1);

	fimc_is_disable_dt(pdev, sclk_name);

	return 0;
}
#endif

/* Wrapper functions */
int exynos_fimc_is_sensor_iclk_get(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
       return 0;
}
int exynos_fimc_is_sensor_iclk_cfg(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
#if defined(CONFIG_SOC_EXYNOS3475)
	exynos3475_fimc_is_sensor_iclk_cfg(pdev, scenario, channel);
#endif
	return 0;
}

int exynos_fimc_is_sensor_iclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
#if defined(CONFIG_SOC_EXYNOS3475)
	exynos3475_fimc_is_sensor_iclk_on(pdev, scenario, channel);
#endif
	return 0;
}

int exynos_fimc_is_sensor_iclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
#if defined(CONFIG_SOC_EXYNOS3475)
	exynos3475_fimc_is_sensor_iclk_off(pdev, scenario, channel);
#endif
	return 0;
}

int exynos_fimc_is_sensor_mclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
#if defined(CONFIG_SOC_EXYNOS3475)
	exynos3475_fimc_is_sensor_mclk_on(pdev, scenario, channel);
#endif
	return 0;
}

int exynos_fimc_is_sensor_mclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
#if defined(CONFIG_SOC_EXYNOS3475)
	exynos3475_fimc_is_sensor_mclk_off(pdev, scenario, channel);
#endif
	return 0;
}
