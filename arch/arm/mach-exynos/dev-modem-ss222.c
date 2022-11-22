/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <asm/system.h>

#include <mach/pmu.h>
#ifdef CONFIG_REGULATOR_S2MPU01A
#include <linux/mfd/samsung/s2mpu01a.h>
#endif

#include <linux/platform_data/modem_v1.h>
#include "dev-shdmem.h"
#include "dev-modem-ss222.h"

static struct modem_io_t ss222_io_devices[] = {
	[0] = {
		.name = "umts_boot0",
		.id = SIPC5_CH_ID_BOOT_0,
		.format = IPC_BOOT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SHMEM),
		.app = "CBD"
	},
	[1] = {
		.name = "umts_ipc0",
		.id = SIPC5_CH_ID_FMT_0,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SHMEM),
		.app = "RIL"
	},
	[2] = {
		.name = "umts_rfs0",
		.id = SIPC5_CH_ID_RFS_0,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SHMEM),
		.app = "RFS"
	},
	[3] = {
		.name = "multipdp",
		.id = SIPC_CH_ID_RAW_0,
		.format = IPC_MULTI_RAW,
		.io_type = IODEV_DUMMY,
		.links = LINKTYPE(LINKDEV_SHMEM),
	},
	[4] = {
		.name = "rmnet0",
		.id = SIPC_CH_ID_PDP_0,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SHMEM),
	},
	[5] = {
		.name = "rmnet1",
		.id = SIPC_CH_ID_PDP_1,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SHMEM),
	},
	[6] = {
		.name = "rmnet2",
		.id = SIPC_CH_ID_PDP_2,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SHMEM),
	},
	[7] = {
		.name = "rmnet3",
		.id = SIPC_CH_ID_PDP_3,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SHMEM),
	},
	[8] = {
		.name = "umts_csd",	/* CS Video Telephony */
		.id = SIPC_CH_ID_CS_VT_DATA,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SHMEM),
	},
	[9] = {
		.name = "umts_router",	/* AT Iface & Dial-up */
		.id = SIPC_CH_ID_BT_DUN,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SHMEM),
		.app = "Data Router"
	},
	[10] = {
		.name = "umts_dm0",	/* DM Port */
		.id = SIPC_CH_ID_CPLOG1,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SHMEM),
		.app = "DIAG"
	},
	[11] = {
		.name = "umts_loopback_cp2ap",
		.id = SIPC_CH_ID_LOOPBACK1,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SHMEM),
		.app = "CP Loopback"
	},
	[12] = {
		.name = "umts_loopback_ap2cp",
		.id = SIPC_CH_ID_LOOPBACK2,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SHMEM),
		.app = "AP loopback"
	},
	[13] = {
		.name = "umts_ramdump0",
		.id = SIPC5_CH_ID_DUMP_0,
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
};

static struct modem_mbox ss222_mbox = {
	/*
	** Mailbox registers (with the corresponding interrupt number)
	**  even number : AP->CP (0 to 30)
	**  odd number  : CP->AP (1 to 31), AP->CP (from 33)
	*/
	.mbx_ap2cp_msg = 0,
	.mbx_cp2ap_msg = 1,
	.mbx_ap2cp_active = MBREG_PDA_ACTIVE,
	.mbx_cp2ap_active = MBREG_PHONE_ACTIVE,
	.mbx_ap2cp_wakeup = 2,
	.mbx_cp2ap_wakeup = 3,
	.mbx_ap2cp_status = 4,
	.mbx_cp2ap_status = 5,

	.mbx_cp2ap_perf_req = 7,

	.mbx_ap2cp_sys_rev = MBREG_HW_REVISION,
	.mbx_ap2cp_pmic_rev = MBREG_PMIC_REVISION,
	.mbx_ap2cp_pkg_id = MBREG_PACKAGE_ID,

	.int_ap2cp_msg = MCU_IPC_INT0,
	.int_ap2cp_active = MCU_IPC_INT3,
	.int_ap2cp_wakeup = MCU_IPC_INT1,
	.int_ap2cp_status = MCU_IPC_INT2,

	.irq_cp2ap_msg = MCU_IPC_INT0,
	.irq_cp2ap_active = MCU_IPC_INT4,
	.irq_cp2ap_wakeup = MCU_IPC_INT1,
	.irq_cp2ap_status = MCU_IPC_INT2,

	.irq_cp2ap_perf_req = MCU_IPC_INT3,
};

static struct modem_pmu ss222_pmu = {
	.power = exynos4_set_cp_power_onoff,
	.get_pwr_status = exynos4_get_cp_power_status,
	.stop = exynos4_cp_reset,
	.start = exynos4_cp_release,
	.clear_cp_fail = exynos4_cp_active_clear,
	.clear_cp_wdt = exynos4_clear_cp_reset_req,
};

static struct modem_data ss222_modem_pdata = {
	.name = "sh222ap",
	.modem_type = SEC_SH222AP,
	.modem_net = UMTS_NETWORK,
	.link_types = LINKTYPE(LINKDEV_SHMEM),
	.link_name = "shmem",
	.num_iodevs = ARRAY_SIZE(ss222_io_devices),
	.iodevs = ss222_io_devices,
	.ipc_version = SIPC_VER_50,

	.mbx = &ss222_mbox,
	.pmu = &ss222_pmu,
};

static struct resource ss222_modem_resources[] = {
	[0] = DEFINE_RES_IRQ_NAMED(EXYNOS3470_IRQ_CP_FAIL, STR_CP_FAIL),
	[1] = DEFINE_RES_IRQ_NAMED(EXYNOS3470_IRQ_CP_WDT, STR_CP_WDT),
};

static u64 modem_if_dma_mask = DMA_BIT_MASK(32);

static struct platform_device ss222_modem = {
	.name = "mif_sipc5",
	.id = -1,
	.num_resources = ARRAY_SIZE(ss222_modem_resources),
	.resource = ss222_modem_resources,
	.dev = {
		.dma_mask = &modem_if_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};

static unsigned int get_package_id(void)
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

int __init init_exynos_dev_modem_ss222(struct modem_io_t *io_devices,
				       int num_io_devices)
{
	int err;
	pr_err("mif: %s: +++\n", __func__);

	err = init_exynos_dev_shdmem();
	if (err) {
		pr_err("mif: %s: ERR! init_exynos_dev_shdmem fail (err %d)\n",
			__func__, err);
		goto error;
	}

	ss222_modem_pdata.sys_rev = system_rev;
#ifdef CONFIG_REGULATOR_S2MPU01A
	ss222_modem_pdata.pmic_rev = get_pmic_rev();
#endif
	ss222_modem_pdata.pkg_id = get_package_id();

	if (io_devices) {
		ss222_modem_pdata.iodevs = io_devices;
		ss222_modem_pdata.num_iodevs = num_io_devices;
	}

	ss222_modem.dev.platform_data = &ss222_modem_pdata;
	err = platform_device_register(&ss222_modem);
	if (err) {
		pr_err("mif: %s: ERR! platform_device_register fail (err %d)\n",
			__func__, err);
		goto error;
	}

	pr_err("mif: %s: ---\n", __func__);
	return 0;

error:
	pr_err("mif: %s: xxx\n", __func__);
	return err;
}
EXPORT_SYMBOL(init_exynos_dev_modem_ss222);

MODULE_DESCRIPTION("Samsung Shannon222 Modem Device (IP)");
MODULE_AUTHOR("Hankook Jang <hankook.jang@samsung.com>");
MODULE_LICENSE("GPL");
