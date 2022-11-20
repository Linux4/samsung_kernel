/* linux/arch/arm/mach-xxxx/board-xmm6360-modems.c
 * Copyright (C) 2010 Samsung Electronics. All rights reserved.
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

/* Modem configuraiton for H Neo LTE (Exynos5260 + XMM7160) */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/usb/ehci_def.h>
#include <linux/pm_qos.h>

#include <linux/platform_data/modem_v2.h>
#include <mach/sec_modem.h>

#include <linux/io.h>
#include <mach/map.h>
#include <mach/regs-pmu.h>
#include <mach/regs-usb-phy.h>

#include <plat/s3c64xx-spi.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <mach/spi-clocks.h>
#include <plat/devs.h>

#define EHCI_REG_DUMP

extern unsigned int system_rev;

/* umts target platform data */
static struct modem_io_t umts_io_devices[] = {
	[0] = {
		.name = "umts_ipc0",
		.id = 235,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = 0x0a,/* SIPC5 */
		.rxq_max = 1024,
		.multi_len = 3456,
	},
	[1] = {
		.name = "umts_rfs0",
		.id = 245,
		.format = IPC_RFS,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = 0x02,
	},
	[2] = {
		.name = "umts_boot0",
		.id = 0,
		.format = IPC_BOOT,
		.io_type = IODEV_MISC,
		.links = (LINKTYPE(LINKDEV_SPI) | LINKTYPE(LINKDEV_HSIC)),
		.tx_link = LINKDEV_HSIC,
		.attr = 0x02,
	},
	[3] = {
		.name = "multipdp",
		.id = 0,
		.format = IPC_MULTI_RAW,
		.io_type = IODEV_DUMMY,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = 0x02,
	},
	[4] = {
		.name = "umts_router",	/* AT Iface & Dial-up */
		.id = 25,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = 0x02,
	},
	[5] = {
		.name = "umts_csd",	/* CS Video Telephony */
		.id = 1,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = 0x02,
	},
	[6] = {
		.name = "umts_dm0",	/* DM Port */
		.id = 28,
		.format = IPC_RAW_NCM,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = 0x06,
		.rxq_max = 1024,
	},
	[7] = {
		.name = "umts_log",
		.id = 27,
		.format = IPC_BOOT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = 0x02,
	},
	[8] = {
		.name = "rmnet0",
		.id = 10,
		.format = IPC_RAW_NCM,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = 0x06,
	},
	[9] = {
		.name = "rmnet1",
		.id = 11,
		.format = IPC_RAW_NCM,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = 0x06,
	},
	[10] = {
		.name = "rmnet2",
		.id = 12,
		.format = IPC_RAW_NCM,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = 0x06,
	},
	[11] = {
		.name = "rmnet3",
		.id = 13,
		.format = IPC_RAW_NCM,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = 0x06,
	},
	[12] = {
		.name = "rmnet4",
		.id = 31,
		.format = IPC_RAW_NCM,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = 0x06,
	},
	[13] = {
		.name = "ipc_loopback0",
		.id = 13,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = 0x02,
	},
};

static struct pm_qos_request mif_qos_req;
static struct pm_qos_request int_qos_req;
#define REQ_PM_QOS(req, class_id, arg) \
	do { \
		if (pm_qos_request_active(req)) \
			pm_qos_update_request(req, arg); \
		else \
			pm_qos_add_request(req, class_id, arg); \
	} while (0) \

#define MAX_FREQ_LEVEL 2
static struct {
	unsigned throughput;
	unsigned mif_freq_lock;
	unsigned int_freq_lock;
} freq_table[MAX_FREQ_LEVEL] = {
	{ 50, 667000, 100000 }, /* default */
	{ 150, 667000, 533000 }, /* 100Mbps */
};

static void exynos_frequency_unlock(void);
static void exynos_frequency_lock(unsigned long qosval);

static struct modemlink_pm_data modem_link_pm_data = {
	.name = "link_pm",
	.gpio_link_enable = 0,
	.gpio_link_active = GPIO_AP2CP_STATUS,
	.gpio_link_hostwake = GPIO_IPC_HOST_WAKEUP,
	.gpio_link_slavewake = GPIO_IPC_SLAVE_WAKEUP,
	.gpio_link_cp2ap_status = GPIO_CP2AP_STATUS,
	.port = 2,
	.freqlock = ATOMIC_INIT(0),
	.freq_lock = exynos_frequency_lock,
	.freq_unlock = exynos_frequency_unlock,
};

static struct platform_device modem_linkpm_shannon300 = {
	.name = "link_pm_hsic",
	.id = -1,
	.dev = {
		.platform_data = &modem_link_pm_data,
	},
};

static struct modem_data umts_modem_data = {
	.name = "shannon300",

	.gpio_cp_on = GPIO_PHONE_ON,
	.gpio_cp_reset = GPIO_RESET_REQ_N,
	.gpio_cp_dump_int = GPIO_CP_DUMP_INT,
	.gpio_ap_dump_int = GPIO_AP_DUMP_INT,
	.gpio_pda_active = GPIO_PDA_ACTIVE,
	.gpio_cp_pmic_pwr_hold = GPIO_CP_PMIC_HOLD,

	.modem_net = UMTS_NETWORK,
	.modem_type = SEC_SHANNON,
	.link_types = (LINKTYPE(LINKDEV_SPI) | LINKTYPE(LINKDEV_HSIC)),
	.link_name = "hsic",

	.num_iodevs = ARRAY_SIZE(umts_io_devices),
	.iodevs = umts_io_devices,

	.use_handover = false,

	.ipc_version = SIPC_VER_50,
};

static struct platform_device umts_modem = {
	.name = "mif_sipc5",
	.id = -1,
	.dev = {
		.platform_data = &umts_modem_data,
	},
};

static void exynos_frequency_unlock(void)
{
	if (atomic_read(&modem_link_pm_data.freqlock) != 0) {
		mif_info("unlocking level = %d\n",
			atomic_read(&modem_link_pm_data.freqlock));

		REQ_PM_QOS(&int_qos_req, PM_QOS_DEVICE_THROUGHPUT, 0);
		REQ_PM_QOS(&mif_qos_req, PM_QOS_BUS_THROUGHPUT, 0);
		atomic_set(&modem_link_pm_data.freqlock, 0);
	} else {
		mif_debug("already unlocked, curr_level = %d\n",
			atomic_read(&modem_link_pm_data.freqlock));
	}
}

static void exynos_frequency_lock(unsigned long qosval)
{
	int level;
	unsigned mif_freq, int_freq;

	for (level = 0; level < MAX_FREQ_LEVEL; level++)
		if (qosval < freq_table[level].throughput)
			break;

	level = min(level, MAX_FREQ_LEVEL - 1);
	if (!level && atomic_read(&modem_link_pm_data.freqlock)) {
		mif_debug("locked level = %d, requested level = %d\n",
			atomic_read(&modem_link_pm_data.freqlock), level);
		exynos_frequency_unlock();
		atomic_set(&modem_link_pm_data.freqlock, level);
		return;
	}

	mif_freq = freq_table[level].mif_freq_lock;
	int_freq = freq_table[level].int_freq_lock;

	if (atomic_read(&modem_link_pm_data.freqlock) != level) {
		mif_debug("locked level = %d, requested level = %d\n",
			atomic_read(&modem_link_pm_data.freqlock), level);

		exynos_frequency_unlock();
		mdelay(50);

		REQ_PM_QOS(&mif_qos_req, PM_QOS_BUS_THROUGHPUT, mif_freq);
		REQ_PM_QOS(&int_qos_req, PM_QOS_DEVICE_THROUGHPUT, int_freq);
		atomic_set(&modem_link_pm_data.freqlock, level);

		mif_info("TP=%ld, MIF=%d, INT=%d\n",
				qosval, mif_freq, int_freq);
	} else {
		mif_debug("already locked, curr_level = %d[%d]\n",
			atomic_read(&modem_link_pm_data.freqlock), level);
	}
}

static void config_umts_modem_gpio(void)
{
	int ret = 0;

	unsigned gpio_cp_on = umts_modem_data.gpio_cp_on;
	unsigned gpio_cp_reset = umts_modem_data.gpio_cp_reset;
	unsigned gpio_pda_active = umts_modem_data.gpio_pda_active;
	unsigned gpio_cp_dump_int = umts_modem_data.gpio_cp_dump_int;
	unsigned gpio_ap_dump_int = umts_modem_data.gpio_ap_dump_int;
	unsigned gpio_cp_pmic_pwr_hold = umts_modem_data.gpio_cp_pmic_pwr_hold;

	if (gpio_cp_on) {
		ret = gpio_request(gpio_cp_on, "CP_ON");
		if (ret)
			mif_err("fail to request gpio %s:%d\n", "CP_ON", ret);
		gpio_direction_output(gpio_cp_on, 0);
	}

	if (gpio_cp_reset) {
		ret = gpio_request(gpio_cp_reset, "CP_RST");
		if (ret)
			mif_err("fail to request gpio %s:%d\n", "CP_RST", ret);
		gpio_direction_output(gpio_cp_reset, 0);
		s3c_gpio_setpull(gpio_cp_reset, S3C_GPIO_PULL_NONE);
	}

	if (gpio_pda_active) {
		ret = gpio_request(gpio_pda_active, "PDA_ACTIVE");
		if (ret)
			mif_err("fail to request gpio %s:%d\n", "PDA_ACTIVE",
				ret);
		gpio_direction_output(gpio_pda_active, 1);
	}

	if (gpio_cp_dump_int) {
		ret = gpio_request(gpio_cp_dump_int, "CP_DUMP_INT");
		if (ret)
			mif_err("fail to request gpio %s:%d\n", "CP_DUMP_INT",
				ret);
		gpio_direction_input(gpio_cp_dump_int);
		s3c_gpio_setpull(gpio_cp_dump_int, S3C_GPIO_PULL_NONE);
	}

	if (gpio_ap_dump_int) {
		ret = gpio_request(gpio_ap_dump_int, "AP_DUMP_INT");
		if (ret)
			mif_err("fail to request gpio %s:%d\n", "AP_DUMP_INT",
				ret);
		gpio_direction_output(gpio_ap_dump_int, 0);
	}

	if (gpio_cp_pmic_pwr_hold) {
		ret = gpio_request(gpio_cp_pmic_pwr_hold, "CP_PMIC_HOLD");
		if (ret)
			mif_err("fail to request gpio %s:%d\n", "CP_PMIC_HOLD",
				ret);
		gpio_direction_input(gpio_cp_pmic_pwr_hold);
		s3c_gpio_setpull(gpio_cp_pmic_pwr_hold, S3C_GPIO_PULL_NONE);
	}

	/* setting for reserved gpio */
	gpio_direction_input(GPIO_TP_INDICATOR);
	s3c_gpio_setpull(GPIO_TP_INDICATOR, S3C_GPIO_PULL_DOWN);

	mif_info("umts_modem_cfg_gpio done\n");
}

static void config_umts_modem_link_pm_gpio(void)
{
	int ret = 0;

	unsigned gpio_link_active = modem_link_pm_data.gpio_link_active;
	unsigned gpio_link_hostwake = modem_link_pm_data.gpio_link_hostwake;
	unsigned gpio_link_slavewake = modem_link_pm_data.gpio_link_slavewake;
	unsigned gpio_link_cp2ap_status = modem_link_pm_data.gpio_link_cp2ap_status;

	if (gpio_link_active) {
		ret = gpio_request(gpio_link_active, "LINK_ACTIVE");
		if (ret) {
			mif_err("fail to request gpio %s:%d\n", "LINK_ACTIVE",
				ret);
		}
		gpio_direction_output(gpio_link_active, 0);
	}

	if (gpio_link_hostwake) {
		ret = gpio_request(gpio_link_hostwake, "HOSTWAKE");
		if (ret) {
			mif_err("fail to request gpio %s:%d\n", "HOSTWAKE",
				ret);
		}
		gpio_direction_input(gpio_link_hostwake);
		s3c_gpio_setpull(gpio_link_hostwake, S3C_GPIO_PULL_UP);

		irq_set_irq_type(gpio_to_irq(gpio_link_hostwake),
					IRQ_TYPE_EDGE_BOTH);
	}

	if (gpio_link_slavewake) {
		ret = gpio_request(gpio_link_slavewake, "SLAVEWAKE");
		if (ret) {
			mif_err("fail to request gpio %s:%d\n", "SLAVEWAKE",
				ret);
		}
		gpio_direction_output(gpio_link_slavewake, 0);
	}

	if (gpio_link_cp2ap_status) {
		ret = gpio_request(gpio_link_cp2ap_status, "CP2AP_STATUS");
		if (ret)
			mif_err("fail to request gpio %s:%d\n", "CP2AP_STATUS",
				ret);
		gpio_direction_input(gpio_link_cp2ap_status);
		s3c_gpio_setpull(gpio_link_cp2ap_status, S3C_GPIO_PULL_UP);
	}

	mif_info("modem_link_pm_config_gpio done\n");
}

static int __init init_modem(void)
{
	int err = 0;
	mif_err("+++\n");

	/*
	** Configure GPIO pins for the modem
	*/
	config_umts_modem_gpio();
	config_umts_modem_link_pm_gpio();

	err = platform_device_register(&modem_linkpm_shannon300);
	if (err < 0)
		mif_err("%s: ERR! platform_device_register fail (err %d)\n",
				modem_linkpm_shannon300.name, err);

	err = platform_device_register(&umts_modem);
	if (err < 0)
		mif_err("%s: %s: ERR! platform_device_register fail (err %d)\n",
				umts_modem_data.name, umts_modem.name, err);

	mif_err("---\n");

	return err;
}
late_initcall(init_modem);
