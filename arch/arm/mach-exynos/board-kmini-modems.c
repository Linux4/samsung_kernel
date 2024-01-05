/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gpio.h>
#include <linux/dma-mapping.h>
#include <linux/platform_data/modem_v1.h>
#include <linux/delay.h>

#include <plat/gpio-cfg.h>
#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/devs.h>

#include <mach/regs-pmu.h>
#include <mach/pmu.h>
#include <mach/irqs.h>

#include "board-universal222ap.h"

#define MCU_AP2CP_IPC_INT		0
#define MCU_CP2AP_IPC_INT		0
#define MCU_AP2CP_WKUP_INT		1
#define MCU_CP2AP_WKUP_INT		1
#define MCU_AP2CP_STAT_INT		2
#define MCU_CP2AP_STAT_INT		2
#define MCU_PDA_ACTIVE_INT		3	/* AP->CP */
#define MCU_CP2AP_DVFSREQ_INT	3
#define MCU_PHONE_ACTIVE_INT	4	/*CP->AP */

#define MBREG_AP2CP_IPC			0
#define MBREG_CP2AP_IPC			1
#define MBREG_AP2CP_WAKEUP		2
#define MBREG_CP2AP_WAKEUP		3
#define MBREG_AP2CP_STATUS		4
#define MBREG_CP2AP_STATUS		5
#define MBREG_PDA_ACTIVE		6
#define MBREG_CP2AP_DVFSREQ	7
#define MBREG_PHONE_ACTIVE		9
#define MBREG_HW_REVISION		33
#define MBREG_PMIC_REVISION		35
#define MBREG_PACKAGE_ID		37

#define REG_PA_PACKAGE_ID		0x10000004

static struct modem_io_t io_devices[] = {
#ifdef CONFIG_LINK_DEVICE_SHMEM
	[0] = {
		.name = "umts_boot0",
		.id = 215,
		.format = IPC_BOOT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SHMEM),
		.app = "CBD"
	},

	[1] = {
		.name = "umts_ipc0",
		.id = 235,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SHMEM),
		.app = "RIL"
	},
	[2] = {
		.name = "umts_rfs0",
		.id = 245,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SHMEM),
		.app = "RFS"
	},
	[3] = {
		.name = "multipdp",
		.id = 0,
		.format = IPC_MULTI_RAW,
		.io_type = IODEV_DUMMY,
		.links = LINKTYPE(LINKDEV_SHMEM),
	},
	[4] = {
		.name = "rmnet0",
		.id = 10,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SHMEM),
	},
	[5] = {
		.name = "rmnet1",
		.id = 11,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SHMEM),
	},
	[6] = {
		.name = "rmnet2",
		.id = 12,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SHMEM),
	},
	[7] = {
		.name = "rmnet3",
		.id = 13,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SHMEM),
	},
	[8] = {
		.name = "umts_csd",	/* CS Video Telephony */
		.id = 1,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SHMEM),
	},
	[9] = {
		.name = "umts_router",	/* AT Iface & Dial-up */
		.id = 25,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SHMEM),
		.app = "Data Router"
	},
	[10] = {
		.name = "umts_dm0",	/* DM Port */
		.id = 28,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SHMEM),
		.app = "DIAG"
	},
	[11] = {
		.name = "umts_loopback_cp2ap",
		.id = 30,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SHMEM),
		.app = "CP Loopback"
	},
	[12] = {
		.name = "umts_loopback_ap2cp",
		.id = 31,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SHMEM),
		.app = "AP loopback"
	},
	[13] = {
		.name = "umts_ramdump0",
		.id = 225,
		.format = IPC_RAMDUMP,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SHMEM),
	},
	[14] = {
		.name = "umts_log",
		.id = 0,
		.format = IPC_RAMDUMP,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SHMEM),
	},
#endif
};

static struct modem_mbox ss222_mbox = {
	/*
	** Mailbox registers (with the corresponding interrupt number)
	**  even number : AP->CP (0 to 30)
	**  odd number  : CP->AP (1 to 31)
	*/
	.mbx_ap2cp_msg = MBREG_AP2CP_IPC,
	.mbx_cp2ap_msg = MBREG_CP2AP_IPC,
	.mbx_ap2cp_active = MBREG_PDA_ACTIVE,
	.mbx_cp2ap_active = MBREG_PHONE_ACTIVE,

	.mbx_ap2cp_wakeup = MBREG_AP2CP_WAKEUP,
	.mbx_cp2ap_wakeup = MBREG_CP2AP_WAKEUP,
	.mbx_ap2cp_status = MBREG_AP2CP_STATUS,
	.mbx_cp2ap_status = MBREG_CP2AP_STATUS,

	.mbx_cp2ap_perf_req = MBREG_CP2AP_DVFSREQ,

	.mbx_ap2cp_sys_rev = MBREG_HW_REVISION,
	.mbx_ap2cp_pmic_rev = MBREG_PMIC_REVISION,
	.mbx_ap2cp_pkg_id = MBREG_PACKAGE_ID,

	.int_ap2cp_msg = MCU_AP2CP_IPC_INT,
	.int_ap2cp_active = MCU_PDA_ACTIVE_INT,
	.int_ap2cp_wakeup = MCU_AP2CP_WKUP_INT,
	.int_ap2cp_status = MCU_AP2CP_STAT_INT,

	.irq_cp2ap_msg = MCU_CP2AP_IPC_INT,
	.irq_cp2ap_active = MCU_PHONE_ACTIVE_INT,
	.irq_cp2ap_wakeup = MCU_CP2AP_WKUP_INT,
	.irq_cp2ap_status = MCU_CP2AP_STAT_INT,

	.irq_cp2ap_perf_req = MCU_CP2AP_DVFSREQ_INT,
};

static struct modem_pmu ss222_pmu = {
	.power = (int(*)(int))exynos4_set_cp_power_onoff,
	.get_pwr_status = (int(*)(void))exynos4_get_cp_power_status,
	.stop = exynos4_cp_reset,
	.start = exynos4_cp_release,
	.clear_cp_fail = exynos4_cp_active_clear,
	.clear_cp_wdt = exynos4_clear_cp_reset_req,
};

struct modem_data carmen_modem_pdata = {
	.name = "Shannon222AP",
	.modem_type = LSI_SHANNON222AP,
	.modem_net = UMTS_NETWORK,
	.link_types = LINKTYPE(LINKDEV_SHMEM),
	.link_name = "shmem",
	.num_iodevs = ARRAY_SIZE(io_devices),
	.iodevs = io_devices,
	.ipc_version = SIPC_VER_50,
	.mbx = &ss222_mbox,
	.pmu = &ss222_pmu,
};

static struct resource modem_res[] = {
	[0] = DEFINE_RES_IRQ(EXYNOS3470_IRQ_CP_ACTIVE),
	[1] = DEFINE_RES_IRQ(EXYNOS3470_IRQ_CP_WDT),
};

static u64 modem_if_dma_mask = DMA_BIT_MASK(32);

struct platform_device exynos4_device_modem = {
	.name		= "mif_sipc5",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(modem_res),
	.resource	= modem_res,
	.dev		= {
		.dma_mask		= &modem_if_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

unsigned int get_package_id(void)
{
	void __iomem *pkg_reg_addr;
	unsigned int pkg_id;

	pkg_reg_addr = ioremap(REG_PA_PACKAGE_ID, SZ_4);
	pkg_id = readl(pkg_reg_addr);
	pkg_id &= (0x3 << 4);
	pkg_id >>= 4;

	iounmap(pkg_reg_addr);

	return pkg_id;
}
static struct gpio hw_rev_gpios[] = {
	{ EXYNOS4_GPF3(2), GPIOF_IN, "hw_rev0" },
	{ EXYNOS4_GPF3(3), GPIOF_IN, "hw_rev1" },
	{ EXYNOS4_GPF2(5), GPIOF_IN, "hw_rev2" },
	{ EXYNOS4_GPF2(6), GPIOF_IN, "hw_rev3" },
};

static u32 __init get_hw_rev(void)
{
	int ret, i;
	u32 hw_rev = 0;

	ret = gpio_request_array(hw_rev_gpios, ARRAY_SIZE(hw_rev_gpios));
	for (i=0; i < ARRAY_SIZE(hw_rev_gpios); i++)
		s3c_gpio_setpull(hw_rev_gpios[i].gpio, S3C_GPIO_PULL_NONE);

	udelay(9);

	BUG_ON(ret);
	for (i = 0; i < ARRAY_SIZE(hw_rev_gpios); i++)
		hw_rev |= gpio_get_value(hw_rev_gpios[i].gpio) << i;

	pr_debug("[Modem] Board Revision Value : (0x%x).\n", hw_rev);

	return hw_rev;
}

void __init exynos4_smdk4270_modem_init(void)
{
	/* It is possible to change modem by changing platform data */
	exynos4_device_modem.dev.platform_data = &carmen_modem_pdata;

	pr_debug("[Modem] modem initialization.\n");

	carmen_modem_pdata.package_id = get_package_id();
	carmen_modem_pdata.hw_revision = get_hw_rev();

	platform_device_register(&exynos4_device_modem);
}
