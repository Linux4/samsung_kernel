// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME "@(%s:%d) " fmt, __func__, __LINE__

#include <linux/gpio/consumer.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>

#include "connv3_hw.h"
#include "connv3_pinctrl_mng.h"

#include "consys_reg_util.h"

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

#define MT6983_PINCTRL_IMPL_READY	0

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
enum uart_gpio_type {
	GPIO_COEX_UTXD,
	GPIO_COEX_URXD,
	GPIO_SCP_WB_UTXD,
	GPIO_SCP_WB_URXD,
	GPIO_UART_MAX,
};

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
static struct pinctrl *pinctrl_ptr = NULL;

void __iomem *vir_0x1000_5000 = NULL; /* GPIO */
void __iomem *vir_0x11D3_0000 = NULL; /* IOCFG_BM */
void __iomem *vir_0x11F1_0000 = NULL; /* IOCFG_TR */

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
int connv3_plt_pinctrl_init_mt6983(struct platform_device *pdev);
int connv3_plt_pinctrl_deinit_mt6983(void);
int connv3_plt_pinctrl_setup_pre_mt6983(void);
int connv3_plt_pinctrl_setup_done_mt6983(void);
int connv3_plt_pinctrl_remove_mt6983(void);

const struct connv3_platform_pinctrl_ops g_connv3_platform_pinctrl_ops_mt6983 = {
	.pinctrl_init = connv3_plt_pinctrl_init_mt6983,
	.pinctrl_deinit = connv3_plt_pinctrl_deinit_mt6983,
	.pinctrl_setup_pre = connv3_plt_pinctrl_setup_pre_mt6983,
	.pinctrl_setup_done = connv3_plt_pinctrl_setup_done_mt6983,
	.pinctrl_remove = connv3_plt_pinctrl_remove_mt6983,
	.pinctrl_ext_32k_ctrl = NULL, // In 6983, use TCXO. Don't need to control ext 32K.
};

#if MT6983_PINCTRL_IMPL_READY
static int _drv_map(unsigned int drv)
{
	const int drv_map[] = {2, 4, 6, 8, 10, 12, 14, 16};

	if (drv >= 8)
		return 0;
	return drv_map[drv];
}

static void _dump_uart_gpio_state(char* tag)
{
	unsigned int pu_cfg1, pd_cfg1, pupd_cfg;
	unsigned int drv1, drv2;
	unsigned int aux1, aux2;

	if (vir_0x1000_5000 == NULL)
		vir_0x1000_5000 = ioremap(0x10005000, 0x1000);
	if (vir_0x11D3_0000 == NULL)
		vir_0x11D3_0000 = ioremap(0x11d30000, 0x1000);
	if (vir_0x11F1_0000 == NULL)
		vir_0x11F1_0000 = ioremap(0x11f10000, 0x1000);

	if (vir_0x1000_5000 == NULL || vir_0x11D3_0000 == NULL || vir_0x11F1_0000 == NULL) {
		pr_err("[%s] vir_0x1000_5000=%lx vir_0x11D3_0000=%lx vir_0x11F1_0000=%lx",
			__func__, vir_0x1000_5000, vir_0x11D3_0000, vir_0x11F1_0000);
		return;
	}

	/* Base: 0x1000_5000
	 * 0x0340	GPIO_MODE4[26:24]	Aux. mode of PAD_MSDC2_CLK (GPIO38)
	 * 0x0340	GPIO_MODE4[30:28]	Aux. mode of PAD_MSDC2_CMD (GPIO39)
	 * 0x0410	GPIO_MODE17[22:20]	Aux. mode of PAD_URXD1 (GPIO141)
	 * 0x0410	GPIO_MODE17[26:24]	Aux. mode of PAD_UTXD1 (GPIO142)
	 */
	aux1 = CONSYS_REG_READ(vir_0x1000_5000 + 0x0340);
	aux2 = CONSYS_REG_READ(vir_0x1000_5000 + 0x0410);

	/* Base: 0x11F1_0000	IOCFG_TR
	 * 0x0080	PUPD_CFG0[0]	Controls PAD_MSDC2_CLK PUPD pin
	 * 0x0080	PUPD_CFG0[1]	Controls PAD_MSDC2_CMD PUPD pin
	 * - PUPD = 0: pull up; PUPD = 1: pull down
	 * 0x0010	DRV_CFG1[26:24]	Controls PAD_MSDC2_CLK driving selection
	 * 0x0010	DRV_CFG1[29:27]	Controls PAD_MSDC2_CMD driving selection
	 */
	pupd_cfg = CONSYS_REG_READ(vir_0x11F1_0000 + 0x0080);
	drv1 = CONSYS_REG_READ(vir_0x11F1_0000 + 0x0010);

	/* Base: 0x11D3_0000	IOCFG_BM
	 * 0x00b0	PU_CFG1[4]	Controls PAD_UTXD1 pull-up
	 * 0x00b0	PU_CFG1[2]	Controls PAD_URXD1 pull-up
	 * 0x0080	PD_CFG1[4]	Controls PAD_UTXD1 pull-down
	 * 0x0080	PD_CFG1[2]	Controls PAD_URXD1 pull-down
	 * 0x0000	DRV_CFG0[20:18]	Control PAD_UTXD1; PAD_URXD1 driving selection
	 */
	pu_cfg1 = CONSYS_REG_READ(vir_0x11D3_0000 + 0x00b0);
	pd_cfg1 = CONSYS_REG_READ(vir_0x11D3_0000 + 0x0080);
	drv2 = _drv_map(CONSYS_REG_READ_BIT(vir_0x11D3_0000 + 0x0000, 0x1c0000) >> 18);

	pr_info("[%s][%s]", __func__, tag);
	pr_info("GPIO38\tmsdc2_clk\taux=[%d]\tpupd=[%d]\tdrv=[%d]",
		((aux1 & 0x7000000) >> 24), (pupd_cfg & 0x1), _drv_map(((drv1 & 0x7000000) >> 24)));
	pr_info("GPIO39\tmsdc2_cmd\taux=[%d]\tpupd=[%d]\tdrv=[%d]",
		((aux1 & 0x70000000) >> 28), ((pupd_cfg & (0x1 << 1)) >> 1), _drv_map((drv1 & 0x38000000) >> 27));

	pr_info("GPIO141\turxd1\taux=[%d]\tpu=[%d]\tpd=[%d]\tdrv=[%d]",
		((aux2 & 0x700000) >> 20), ((pu_cfg1 & (0x1 << 2)) >> 2), ((pd_cfg1 & (0x1 << 2)) >> 2), drv2);
	pr_info("GPIO142\tutxd1\taux=[%d]\tpu=[%d]\tpd=[%d]\tdrv=[%d]",
		((aux2 & 0x7000000) >> 24), ((pu_cfg1 & (0x1 << 4)) >> 4), ((pd_cfg1 & (0x1 << 4)) >> 4), drv2);
}
#endif

static int connv3_plt_pinctrl_initial_state(void)
{
#if MT6983_PINCTRL_IMPL_READY
	struct pinctrl_state *pinctrl_init;
	int ret;

	if (IS_ERR_OR_NULL(pinctrl_ptr)) {
		pr_err("[%s] fail to get connv3 pinctrl", __func__);
	} else {
		pinctrl_init = pinctrl_lookup_state(
				pinctrl_ptr, "connsys_combo_gpio_init");
		if (IS_ERR_OR_NULL(pinctrl_init))
			pr_err("[%s] fail to get \"connsys_combo_gpio_init\"",  __func__);
		else {
			ret = pinctrl_select_state(pinctrl_ptr, pinctrl_init);
			if (ret)
				pr_err("[%s] pinctrl init fail, %d", __func__, ret);
		}
	}
	_dump_uart_gpio_state("init");
#endif
	return 0;
}

int connv3_plt_pinctrl_init_mt6983(struct platform_device *pdev)
{
	pinctrl_ptr = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(pinctrl_ptr))
		pr_err("[%s] fail to get connv3 pinctrl, ret=%p", __func__, pinctrl_ptr);
#if MT6983_PINCTRL_IMPL_READY
	connv3_plt_pinctrl_initial_state();
#endif

	return 0;
}

int connv3_plt_pinctrl_deinit_mt6983(void)
{
	if (vir_0x1000_5000)
		iounmap(vir_0x1000_5000);
	if (vir_0x11D3_0000)
		iounmap(vir_0x11D3_0000);
	if (vir_0x11F1_0000)
		iounmap(vir_0x11F1_0000);

	vir_0x1000_5000 = NULL;
	vir_0x11F1_0000 = NULL;
	vir_0x11D3_0000 = NULL;
	return 0;
}

int connv3_plt_pinctrl_setup_pre_mt6983(void)
{
#if MT6983_PINCTRL_IMPL_READY
	struct pinctrl_state *pinctrl_pre_on;
	int ret;

	_dump_uart_gpio_state("pre before");

	if (IS_ERR_OR_NULL(pinctrl_ptr)) {
		pr_err("[%s] fail to get connv3 pinctrl", __func__);
	} else {
		pinctrl_pre_on = pinctrl_lookup_state(
				pinctrl_ptr, "connsys_combo_gpio_pre_on");
		if (IS_ERR_OR_NULL(pinctrl_pre_on)) {
			pr_err("[%s] fail to get \"connsys_combo_gpio_pre_on\"",  __func__);
		} else {
			ret = pinctrl_select_state(pinctrl_ptr, pinctrl_pre_on);
			if (ret)
				pr_err("[%s] pinctrl pre on fail, %d", __func__, ret);
		}
	}
	_dump_uart_gpio_state("pre after");
#endif
	return 0;
}

int connv3_plt_pinctrl_setup_done_mt6983(void)
{
#if MT6983_PINCTRL_IMPL_READY
	struct pinctrl_state *pinctrl_on;
	int ret;

	if (IS_ERR_OR_NULL(pinctrl_ptr)) {
		pr_err("[%s] fail to get connv3 pinctrl", __func__);
	} else {
		pinctrl_on = pinctrl_lookup_state(
				pinctrl_ptr, "connsys_combo_gpio_on");
		if (IS_ERR_OR_NULL(pinctrl_on)) {
			pr_err("[%s] fail to get \"connsys_combo_gpio_on\"",  __func__);
		} else {
			ret = pinctrl_select_state(pinctrl_ptr, pinctrl_on);
			if (ret)
				pr_err("[%s] pinctrl on fail, %d", __func__, ret);
		}
	}
	_dump_uart_gpio_state("setup done");
#endif
	return 0;
}

int connv3_plt_pinctrl_remove_mt6983(void)
{
	connv3_plt_pinctrl_initial_state();
	return 0;
}

