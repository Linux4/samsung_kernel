/* linux/arch/arm/mach-xxxx/board-superior-cmcc-modems.c
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/delay.h>

#include <linux/platform_data/modem.h>

#include <mach/gpiomux.h>


#if defined(CONFIG_MACH_H3GDUOS)
#define GPIO_GSM_PHONE_ON	135
#define GPIO_PDA_ACTIVE		136
#define GPIO_PHONE_ACTIVE	18
#define GPIO_CP_DUMP_INT	73
#define GPIO_AP_CP_INT1		124
#define GPIO_AP_CP_INT2		125

#define GPIO_UART_SEL		119
#define GPIO_SIM_SEL		115
#define ESC_SIM_DETECT_IRQ	125

#elif defined(CONFIG_MACH_K3GDUOS_CTC)
#define GPIO_GSM_PHONE_ON	127
#define GPIO_PDA_ACTIVE		118
#define GPIO_PHONE_ACTIVE	107
#define GPIO_AP_CP_INT1		0xFFF
#define GPIO_AP_CP_INT2		0xFFF

#define GPIO_UART_SEL		135
#define GPIO_SIM_SEL		123
#define ESC_SIM_DETECT_IRQ	123

#elif defined(CONFIG_SEC_TRLTE_CHNDUOS)
#define GPIO_GSM_PHONE_ON	315 /* GPIO_EXPANDER P1_7 pin */
#define GPIO_PDA_ACTIVE		305 /* GPIO_EXPANDER P0_5 pin */
#define GPIO_PHONE_ACTIVE	86
#define GPIO_AP_CP_INT1		0xFFF
#define GPIO_AP_CP_INT2		0xFFF

#define GPIO_UART_SEL		682 /* PMA8084 GPIO_19 pin */
#define GPIO_SIM_SEL		0
/* #define ESC_SIM_DETECT_IRQ	123 */

#endif

#if defined(CONFIG_GSM_MODEM_GG_DUOS)
/* gsm target platform data */
static struct modem_io_t gsm_io_devices[] = {
	[0] = {
		.name = "gsm_boot0",
		.id = 0x1,
		.format = IPC_BOOT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[1] = {
		.name = "gsm_ipc0",
		.id = 0x00,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[2] = {
		.name = "umts_ipc0",
		.id = 0x01,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[3] = {
		.name = "gsm_rfs0",
		.id = 0x28,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[4] = {
		.name = "gsm_multi_pdp",
		.id = 0x1,
		.format = IPC_MULTI_RAW,
		.io_type = IODEV_DUMMY,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[5] = {
		.name = "gsm_rmnet0",
		.id = 0x2A,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[6] = {
		.name = "gsm_rmnet1",
		.id = 0x2B,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[7] = {
		.name = "gsm_rmnet2",
		.id = 0x2C,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[8] = {
		.name = "gsm_router",
		.id = 0x39,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	/*
	[8] = {
		.name = "gsm_csd",
		.id = 0x21,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[9] = {
		.name = "gsm_ramdump0",
		.id = 0x1,
		.format = IPC_RAMDUMP,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[10] = {
		.name = "gsm_loopback0",
		.id = 0x3F,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	*/
};

static struct modem_data gsm_modem_data = {
	.name = "sprd6500",

	.modem_type = SPRD_SC6500,
	.link_types = LINKTYPE(LINKDEV_SPI),
	.modem_net = TDSCDMA_NETWORK,
	.use_handover = false,
	.ipc_version = SIPC_VER_42,

	.num_iodevs = ARRAY_SIZE(gsm_io_devices),
	.iodevs = gsm_io_devices,
};

#else
/* gsm target platform data */
static struct modem_io_t gsm_io_devices[] = {
	[0] = {
		.name = "gsm_boot0",
		.id = 0x1,
		.format = IPC_BOOT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[1] = {
		.name = "gsm_ipc0",
		.id = 0x01,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[2] = {
		.name = "gsm_rfs0",
		.id = 0x28,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[3] = {
		.name = "gsm_multi_pdp",
		.id = 0x1,
		.format = IPC_MULTI_RAW,
		.io_type = IODEV_DUMMY,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[4] = {
		.name = "gsm_rmnet0",
		.id = 0x2A,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[5] = {
		.name = "gsm_rmnet1",
		.id = 0x2B,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[6] = {
		.name = "gsm_rmnet2",
		.id = 0x2C,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[7] = {
		.name = "gsm_router",
		.id = 0x39,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	/*
	[8] = {
		.name = "gsm_csd",
		.id = 0x21,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[9] = {
		.name = "gsm_ramdump0",
		.id = 0x1,
		.format = IPC_RAMDUMP,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[10] = {
		.name = "gsm_loopback0",
		.id = 0x3F,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	*/
};

static struct modem_data gsm_modem_data = {
	.name = "sprd6500",

	.modem_type = SPRD_SC6500,
	.link_types = LINKTYPE(LINKDEV_SPI),
	.modem_net = TDSCDMA_NETWORK,
	.use_handover = false,
	.ipc_version = SIPC_VER_40,

	.num_iodevs = ARRAY_SIZE(gsm_io_devices),
	.iodevs = gsm_io_devices,
};

#endif

static int sprd6500_is_qc_gpio(int gpio)
{	
	if (gpio < 300)
		return 1;

	return 0;
}

void sprd6500_modem_cfg_gpio(struct device *dev)
{
	of_property_read_u32(dev->of_node, "mif_cp2,phone_on-gpio", &gsm_modem_data.gpio_cp_on);
	of_property_read_u32(dev->of_node, "mif_cp2,pda_active-gpio", &gsm_modem_data.gpio_pda_active);
	of_property_read_u32(dev->of_node, "mif_cp2,phone_active-gpio", &gsm_modem_data.gpio_phone_active);
	of_property_read_u32(dev->of_node, "mif_cp2,ap_cp_int1-gpio", &gsm_modem_data.gpio_ap_cp_int1);
	of_property_read_u32(dev->of_node, "mif_cp2,ap_cp_int2-gpio", &gsm_modem_data.gpio_ap_cp_int2);
	gsm_modem_data.gpio_uart_sel = of_get_named_gpio(dev->of_node, "mif_cp2,uart_sel-gpio", 0);
	of_property_read_u32(dev->of_node, "mif_cp2,sim_sel-gpio", &gsm_modem_data.gpio_sim_sel);
	of_property_read_u32(dev->of_node, "mif_cp2,uart_txd-gpio", &gsm_modem_data.gpio_uart_txd);
	of_property_read_u32(dev->of_node, "mif_cp2,uart_rxd-gpio", &gsm_modem_data.gpio_uart_rxd);
	of_property_read_u32(dev->of_node, "mif_cp2,uart_cts-gpio", &gsm_modem_data.gpio_uart_cts);
	of_property_read_u32(dev->of_node, "mif_cp2,uart_rts-gpio", &gsm_modem_data.gpio_uart_rts);

	if (sprd6500_is_qc_gpio(gsm_modem_data.gpio_cp_on))	{
		gpio_tlmm_config(GPIO_CFG(gsm_modem_data.gpio_cp_on, GPIOMUX_FUNC_GPIO,
		GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);
		gpio_set_value(gsm_modem_data.gpio_cp_on, 0);
	}
	else {
		gpio_request(gsm_modem_data.gpio_cp_on, "mif_cp2,phone_on-gpio");
		gpio_direction_output(gsm_modem_data.gpio_cp_on, 0);
	}

	if (sprd6500_is_qc_gpio(gsm_modem_data.gpio_pda_active))	{
		gpio_tlmm_config(GPIO_CFG(gsm_modem_data.gpio_pda_active, GPIOMUX_FUNC_GPIO,
		GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);
		gpio_set_value(gsm_modem_data.gpio_pda_active, 0);
	}
	else {
		gpio_request(gsm_modem_data.gpio_pda_active, "mif_cp2,pda_active-gpio");
		gpio_direction_output(gsm_modem_data.gpio_pda_active, 0);
	}

	if(gpio_is_valid(gsm_modem_data.gpio_ap_cp_int2))	{
		gpio_tlmm_config(GPIO_CFG(gsm_modem_data.gpio_ap_cp_int2, GPIOMUX_FUNC_GPIO,
		GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);
		gpio_set_value(gsm_modem_data.gpio_ap_cp_int2, 0);
	}

	if (sprd6500_is_qc_gpio(gsm_modem_data.gpio_uart_sel))	{
		gpio_tlmm_config(GPIO_CFG(gsm_modem_data.gpio_uart_sel, GPIOMUX_FUNC_GPIO,
		GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);
		gpio_set_value(gsm_modem_data.gpio_uart_sel, 0);
	}
	else {
		gpio_request(gsm_modem_data.gpio_uart_sel, "mif_cp2,uart_sel-gpio");
		gpio_direction_output(gsm_modem_data.gpio_uart_sel, 0);
	}

	gpio_request(gsm_modem_data.gpio_phone_active, "mif_cp2,phone_active-gpio");
	gpio_tlmm_config(GPIO_CFG(gsm_modem_data.gpio_phone_active, GPIOMUX_FUNC_GPIO,
		GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);

	if(gpio_is_valid(gsm_modem_data.gpio_ap_cp_int1))	{
		gpio_tlmm_config(GPIO_CFG(gsm_modem_data.gpio_ap_cp_int1, GPIOMUX_FUNC_GPIO,
		GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);
	}

	pr_info("sprd6500_modem_cfg_gpio done\n");
	pr_info("uart_sel : [%d]\n", gpio_get_value(gsm_modem_data.gpio_uart_sel));
}

static struct resource gsm_modem_res[] = {
	[0] = {
		.name = "cp_active_irq",
		.start = 0,
		.end = 0,
		.flags = IORESOURCE_IRQ,
	},
#if defined(CONFIG_SIM_DETECT)
	[1] = {
		.name = "sim_irq",
		.start = ESC_SIM_DETECT_IRQ,
		.end = ESC_SIM_DETECT_IRQ,
		.flags = IORESOURCE_IRQ,
	},
#endif
};

/* if use more than one modem device, then set id num */
static struct platform_device gsm_modem = {
	.name = "mif_sipc4",
	.id = -1,
	.num_resources = ARRAY_SIZE(gsm_modem_res),
	.resource = gsm_modem_res,
	.dev = {
		.platform_data = &gsm_modem_data,
	},
};

static int __exit msm_sprd6500_remove(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);

	return 0;
}

static int msm_sprd6500_probe(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);

	sprd6500_modem_cfg_gpio(&(pdev->dev));

	gsm_modem_res[0].start = gpio_to_irq(gsm_modem_data.gpio_phone_active);
	gsm_modem_res[0].end = gpio_to_irq(gsm_modem_data.gpio_phone_active);

	mif_info("board init_sprd6500_modem done\n");

	return 0;
}

static const struct of_device_id if_sprd6500_match_table[] = {
	{   .compatible = "if_sprd6500_comp",
	},
	{}
};

static struct platform_driver cp2_sprd6500_driver = {
	.probe	= msm_sprd6500_probe,
	.remove	= msm_sprd6500_remove,
	.driver	= {
		.name		= "cp2_sprd6500",
		.owner		= THIS_MODULE,
		.of_match_table	= if_sprd6500_match_table,
	},
};

static int __init init_modem(void)
{
	platform_driver_register(&cp2_sprd6500_driver);
	platform_device_register(&gsm_modem);

	mif_info("board init_sprd6500_modem done\n");
	return 0;
}

static void __exit exit_modem(void)
{
	platform_device_unregister(&gsm_modem);
	platform_driver_unregister(&cp2_sprd6500_driver);
}

late_initcall(init_modem);
module_exit(exit_modem);
