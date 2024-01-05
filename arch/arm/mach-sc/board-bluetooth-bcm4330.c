/************************************************************************/
/*                                                                      */
/*  Copyright 2012  Broadcom Corporation                                */
/*                                                                      */
/* Unless you and Broadcom execute a separate written software license  */
/* agreement governing use of this software, this software is licensed  */
/* to you under the terms of the GNU General Public License version 2   */
/* (the GPL), available at						*/
/*                                                                      */
/*          http://www.broadcom.com/licenses/GPLv2.php                  */
/*                                                                      */
/*  with the following added to such license:                           */
/*                                                                      */
/*  As a special exception, the copyright holders of this software give */
/*  you permission to link this software with independent modules, and  */
/*  to copy and distribute the resulting executable under terms of your */
/*  choice, provided that you also meet, for each linked independent    */
/*  module, the terms and conditions of the license of that module. An  */
/*  independent module is a module which is not derived from this       */
/*  software.  The special   exception does not apply to any            */
/*  modifications of the software.					*/
/*									*/
/*  Notwithstanding the above, under no circumstances may you combine	*/
/*  this software in any way with any other Broadcom software provided	*/
/*  under a license other than the GPL, without Broadcom's express	*/
/*  prior written consent.						*/
/*									*/
/************************************************************************/
#include <linux/version.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/serial_8250.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/clk.h>
#include <linux/bootmem.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/i2c.h>
#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <asm/mach/map.h>

#include <mach/board.h>

#include "devices.h"

//#if defined(CONFIG_BCM_BT_RFKILL)
#include <linux/broadcom/bcm-bt-rfkill.h>
//#endif

#ifdef CONFIG_BCM_BZHW
#include <linux/broadcom/bcm_bzhw.h>
#endif

//#if defined(CONFIG_BCM_BT_LPM)
#include <linux/broadcom/bcm-bt-lpm.h>
//#endif


//#if defined(CONFIG_BCM_BT_RFKILL)

#define BCMBT_VREG_GPIO		28
#define BCMBT_N_RESET_GPIO       71

static struct bcm_bt_rfkill_platform_data hawaii_bcm_bt_rfkill_cfg = {
	.bcm_bt_rfkill_vreg_gpio = GPIO_BT_POWER,
	.bcm_bt_rfkill_n_reset_gpio = GPIO_BT_RESET,
};

static struct platform_device hawaii_bcm_bt_rfkill_device = {
	.name = "bcm-bt-rfkill",
	.id = -1,
	.dev =	{
		.platform_data = &hawaii_bcm_bt_rfkill_cfg,
	},
};
//#endif

#ifdef CONFIG_BCM_BZHW
#define GPIO_BT_WAKE	32
#define GPIO_HOST_WAKE	72
static struct bcm_bzhw_platform_data bcm_bzhw_data = {
	.gpio_bt_wake = GPIO_BT_WAKE,
	.gpio_host_wake = GPIO_HOST_WAKE,
};

static struct platform_device hawaii_bcm_bzhw_device = {
	.name = "bcm_bzhw",
	.id = -1,
	.dev =	{
		.platform_data = &bcm_bzhw_data,
	},
};
#endif


//#if defined(CONFIG_BCM_BT_LPM)
#define GPIO_BT_WAKE	32
#define GPIO_HOST_WAKE	72

static struct bcm_bt_lpm_platform_data brcm_bt_lpm_data = {
	.bt_wake_gpio = GPIO_AP2BT_WAKE,
	.host_wake_gpio = GPIO_BT2AP_WAKE,
};

static struct platform_device board_bcm_bt_lpm_device = {
	.name = "bcm-bt-lpm",
	.id = -1,
	.dev = {
		.platform_data = &brcm_bt_lpm_data,
	},
};
//#endif


static struct platform_device *hawaii_bt_devices[] __initdata = {

//#if defined(CONFIG_BCM_BT_RFKILL)
	&hawaii_bcm_bt_rfkill_device,
//#endif

#ifdef CONFIG_BCM_BZHW
	&hawaii_bcm_bzhw_device,
#endif

//#if defined(CONFIG_BCM_BT_LPM)
	&board_bcm_bt_lpm_device,
//#endif

};


void __init hawaii_bt_init(void)
{
	platform_add_devices(hawaii_bt_devices, ARRAY_SIZE(hawaii_bt_devices));
return;
}
