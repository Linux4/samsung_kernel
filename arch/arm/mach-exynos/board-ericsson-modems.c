/* linux/arch/arm/mach-xxxx/board-ericsson-modems.c
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

#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/usb.h>
#include <linux/usb/ehci_def.h>
#include <linux/pm_qos.h>
#include <linux/platform_data/modem_v2.h>
#include <mach/cpufreq.h>
#include <mach/dev.h>
#include <mach/map.h>
#include <mach/sec_modem.h>
#include <plat/gpio-cfg.h>
#include <linux/platform_data/sipc_def.h>

/* umts target platform data */
static struct modem_io_t umts_io_devices[] = {
	[0] = {
		.name = "umts_ipc0",
		.id = SIPC5_CH_ID_FMT_0,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
		.rxq_max = 1024,
		.attr = IODEV_ATTR(ATTR_SIPC5) | IODEV_ATTR(ATTR_MULTIFMT),
		.multi_len = 3584,
	},
	[1] = {
		.name = "umts_rfs0",
		.id = SIPC5_CH_ID_RFS_0,
		.format = IPC_RFS,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = IODEV_ATTR(ATTR_SIPC5),
	},
	[2] = {
		.name = "umts_boot0",
		.id = 0x0,
		.format = IPC_BOOT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
	},
	[3] = {
		.name = "multipdp",
		.id = 0x0,
		.format = IPC_MULTI_RAW,
		.io_type = IODEV_DUMMY,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = IODEV_ATTR(ATTR_SIPC5),
	},
	[4] = {
		.name = "umts_router",
		.id = SIPC_CH_ID_BT_DUN,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = IODEV_ATTR(ATTR_SIPC5),
	},
	[5] = {
		.name = "umts_csd",
		.id = SIPC_CH_ID_CS_VT_DATA,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = IODEV_ATTR(ATTR_SIPC5),
	},
	[6] = {
		.name = "umts_dm0",
		.id = SIPC_CH_ID_CPLOG1,
		.format = IPC_RAW_NCM,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
		.rxq_max = 1024,
		.attr = IODEV_ATTR(ATTR_SIPC5) | IODEV_ATTR(ATTR_CDC_NCM),
	},
	[7] = { /* To use IPC_Logger */
		.name = "umts_log",
		.id = SIPC_CH_ID_PDP_18,
		.format = IPC_BOOT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = IODEV_ATTR(ATTR_SIPC5),
	},
#ifndef CONFIG_USB_NET_CDC_NCM
	[8] = {
		.name = "rmnet0",
		.id = SIPC_CH_ID_PDP_0,
		.format = IPC_RAW_NCM,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = IODEV_ATTR(ATTR_SIPC5) | IODEV_ATTR(ATTR_CDC_NCM),
	},
	[9] = {
		.name = "rmnet1",
		.id = SIPC_CH_ID_PDP_1,
		.format = IPC_RAW_NCM,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = IODEV_ATTR(ATTR_SIPC5) | IODEV_ATTR(ATTR_CDC_NCM),
	},
	[10] = {
		.name = "rmnet2",
		.id = SIPC_CH_ID_PDP_2,
		.format = IPC_RAW_NCM,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = IODEV_ATTR(ATTR_SIPC5) | IODEV_ATTR(ATTR_CDC_NCM),
	},
	[11] = {
		.name = "rmnet3",
		.id = SIPC_CH_ID_PDP_3,
		.format = IPC_RAW_NCM,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = IODEV_ATTR(ATTR_SIPC5) | IODEV_ATTR(ATTR_CDC_NCM),
	},
	/* 3x loopback to measure TP using iperf */
	[12] = {
		.name = "rmnet4",
		.id = SIPC_CH_ID_LOOPBACK2,
		.format = IPC_RAW_NCM,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = IODEV_ATTR(ATTR_SIPC5) | IODEV_ATTR(ATTR_CDC_NCM),
	},
#endif
	[13] = {
		.name = "ipc_loopback0",
		.id = SIPC5_CH_ID_FMT_9,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = IODEV_ATTR(ATTR_SIPC5),
	},
	[14] = {
		.name = "umts_ciq0",
		.id = SIPC_CH_ID_CIQ_DATA,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
		.rxq_max = 8192,
		.attr = IODEV_ATTR(ATTR_SIPC5),
	},
	[15] = {
		.name = "umts_calibcmd",
		.id = SIPC_CH_ID_CALIBCMD,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = IODEV_ATTR(ATTR_SIPC5),
	},
};

static void exynos_frequency_unlock(void);
static void exynos_frequency_lock(unsigned long qosval);
static struct modemlink_pm_data modem_link_pm_data = {
	.gpio_link_active = GPIO_HOST_ACTIVE,
	.gpio_link_hostwake = GPIO_HSIC_WAKE_CA,
	.gpio_link_slavewake = GPIO_HSIC_WAKE_AC,
	.gpio_link_cp2ap_status = GPIO_HSIC_CP2AP_STATUS,

	.freqlock = ATOMIC_INIT(0),
	.freq_lock = exynos_frequency_lock,
	.freq_unlock = exynos_frequency_unlock,
};

static struct platform_device modem_linkpm_plat_device = {
	.name = "link_pm_hsic",
	.id = -1,
	.dev = {
		.platform_data = &modem_link_pm_data,
	},
};

static struct modem_data umts_modem_data = {
	.name = "m74xx",

	.gpio_cp_on = GPIO_CP_ON,
	.gpio_cp_reset = GPIO_AP2CP_RESET,

	.gpio_phone_active = GPIO_PHONE_ACTIVE,
	.gpio_pda_active = GPIO_PDA_ACTIVE,
	.gpio_ap_dump_int = GPIO_AP_DUMP_INT,
	.gpio_cp_dump_int = GPIO_CP_RESOUT,

	.modem_type = ERICSSON_M74XX,
	.link_types = LINKTYPE(LINKDEV_HSIC),
	.modem_net = UMTS_NETWORK,
	.use_handover = false,

	.num_iodevs = ARRAY_SIZE(umts_io_devices),
	.iodevs = umts_io_devices,

	.max_acm_channel = 1,
	.max_link_channel = 2,
	.ipc_version = SIPC_VER_50,
};

/* if use more than one modem device, then set id num */
static struct platform_device umts_modem = {
	.name = "mif_sipc5",
	.id = -1,
	.dev = {
		.platform_data = &umts_modem_data,
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
	{ 100, 275000, 100000 }, /* default */
	{ 150, 543000, 267000 }, /* 100Mbps */
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
		mif_info("already unlocked, curr_level = %d\n",
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
		mif_info("locked level = %d, requested level = %d\n",
			atomic_read(&modem_link_pm_data.freqlock), level);
		exynos_frequency_unlock();
		atomic_set(&modem_link_pm_data.freqlock, level);
		return;
	}

	mif_freq = freq_table[level].mif_freq_lock;
	int_freq = freq_table[level].int_freq_lock;

	if (atomic_read(&modem_link_pm_data.freqlock) != level) {
		mif_info("locked level = %d, requested level = %d\n",
			atomic_read(&modem_link_pm_data.freqlock), level);

		exynos_frequency_unlock();
		mdelay(50);

		REQ_PM_QOS(&mif_qos_req, PM_QOS_BUS_THROUGHPUT, mif_freq);
		REQ_PM_QOS(&int_qos_req, PM_QOS_DEVICE_THROUGHPUT, int_freq);
		atomic_set(&modem_link_pm_data.freqlock, level);

		mif_info("TP=%ld, MIF=%d, INT=%d\n",
				qosval, mif_freq, int_freq);
	} else {
		mif_info("already locked, curr_level = %d[%d]\n",
			atomic_read(&modem_link_pm_data.freqlock), level);
	}
}

static int board_gpio_export(struct device *dev, unsigned gpio, const char *label)
{
	int ret;

	ret = gpio_export(gpio, 1);
	if (ret < 0) {
		mif_err("gpio_export(%s) err(%d)\n", label, ret);
		return ret;
	}

	ret = gpio_export_link(dev, label, gpio);
	if (ret <  0) {
		mif_err("gpio_export_link(%s) err(%d)\n", label, ret);
		return ret;
	}

	return ret;
}

static int umts_modem_cfg_gpio(void)
{
	int ret = 0;

	unsigned gpio_cp_on = umts_modem_data.gpio_cp_on;
	unsigned gpio_cp_rst = umts_modem_data.gpio_cp_reset;
	unsigned gpio_phone_active = umts_modem_data.gpio_phone_active;
	unsigned gpio_pda_active = umts_modem_data.gpio_pda_active;
	unsigned gpio_cp_dump_int = umts_modem_data.gpio_cp_dump_int;
	unsigned gpio_ap_dump_int = umts_modem_data.gpio_ap_dump_int;

	if (gpio_cp_on) {
		ret = gpio_request(gpio_cp_on, "cp_on");
		if (ret) {
			mif_err("CP_ON gpio_request fail(%d)\n", ret);
			goto exit;
		}
		gpio_direction_output(gpio_cp_on, 0);

		board_gpio_export(&umts_modem.dev, gpio_cp_on, "cp_on");
	}

	if (gpio_cp_rst) {
		ret = gpio_request(gpio_cp_rst, "cp_rst");
		if (ret) {
			mif_err("AP2CP_RESET gpio_request fail(%d)\n", ret);
			goto exit;
		}
		gpio_direction_output(gpio_cp_rst, 1);
		s3c_gpio_setpull(gpio_cp_rst, S3C_GPIO_PULL_UP);
	}

	if (gpio_phone_active) {
		ret = gpio_request(gpio_phone_active, "phone_active");
		if (ret) {
			mif_err("PHONE_ACTIVE gpio_request fail(%d)\n", ret);
			goto exit;
		}
		gpio_direction_input(gpio_phone_active);
		s3c_gpio_setpull(gpio_phone_active, S3C_GPIO_PULL_NONE);
	}

	if (gpio_pda_active) {
		ret = gpio_request(gpio_pda_active, "pda_active");
		if (ret) {
			mif_err("PDA_ACTIVE gpio_request fail(%d)\n", ret);
			goto exit;
		}
		gpio_direction_output(gpio_pda_active, 0);
	}

	if (gpio_cp_dump_int) {
		ret = gpio_request(gpio_cp_dump_int, "cp_resout");
		if (ret) {
			mif_err("CP_DUMP_INT gpio_request fail(%d)\n", ret);
			goto exit;
		}
		gpio_direction_input(gpio_cp_dump_int);

		board_gpio_export(&umts_modem.dev, gpio_cp_dump_int, "cp_resout");
	}

	if (gpio_ap_dump_int) {
		ret = gpio_request(gpio_ap_dump_int, "ap_dump_int");
		if (ret) {
			mif_err("AP_DUMP_INT gpio_request fail(%d)\n", ret);
			goto exit;
		}
		gpio_direction_output(gpio_ap_dump_int, 0);

		board_gpio_export(&umts_modem.dev, gpio_ap_dump_int, "ap_dump_int");
	}

	mif_info("done\n");
exit:
	return ret;
}

static int modem_link_pm_config_gpio(void)
{
	int ret = 0;

	unsigned gpio_link_active = modem_link_pm_data.gpio_link_active;
	unsigned gpio_link_hostwake = modem_link_pm_data.gpio_link_hostwake;
	unsigned gpio_link_slavewake = modem_link_pm_data.gpio_link_slavewake;
	unsigned gpio_link_cp2ap_status =
				modem_link_pm_data.gpio_link_cp2ap_status;

	if (gpio_link_active) {
		ret = gpio_request(gpio_link_active, "hsic_resume_ac");
		if (ret) {
			mif_err("LINK_ACTIVE gpio_request fail(%d)\n", ret);
			goto exit;
		}
		gpio_direction_output(gpio_link_active, 0);
		s3c_gpio_setpull(gpio_link_active, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio_link_active, S5P_GPIO_DRVSTR_LV4);

		board_gpio_export(&modem_linkpm_plat_device.dev,
				gpio_link_active, "hsic_resume_ac");
	}

	if (gpio_link_hostwake) {
		ret = gpio_request(gpio_link_hostwake, "hsic_wake_ca");
		if (ret) {
			mif_err("HOSTWAKE gpio_request fail(%d)\n", ret);
			goto exit;
		}
		gpio_direction_input(gpio_link_hostwake);
		s3c_gpio_setpull(gpio_link_hostwake, S3C_GPIO_PULL_DOWN);

		board_gpio_export(&modem_linkpm_plat_device.dev,
				gpio_link_hostwake, "hsic_wake_ca");
	}

	if (gpio_link_slavewake) {
		ret = gpio_request(gpio_link_slavewake, "hsic_wake_ac");
		if (ret) {
			mif_err("SLAVE_WAKE gpio_request fail(%d)\n", ret);
			goto exit;
		}
		gpio_direction_output(gpio_link_slavewake, 0);
		s3c_gpio_setpull(gpio_link_slavewake, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio_link_slavewake, S5P_GPIO_DRVSTR_LV4);

		board_gpio_export(&modem_linkpm_plat_device.dev,
				gpio_link_slavewake, "hsic_wake_ac");
	}

	if (gpio_link_cp2ap_status) {
		ret = gpio_request(gpio_link_cp2ap_status, "hsic_cp2ap_status");
		if (ret) {
			mif_err("CP2AP_STATUS gpio_request fail(%d)\n", ret);
			goto exit;
		}
		gpio_direction_input(gpio_link_cp2ap_status);
		s3c_gpio_setpull(gpio_link_cp2ap_status, S3C_GPIO_PULL_DOWN);
	}

	mif_info("done\n");
exit:
	return ret;
}

static int __init init_modem(void)
{
	int ret;

	mif_info("init_modem\n");

	/* umts_modem platform data */
	ret = platform_device_register(&umts_modem);
	if (ret < 0)
		mif_err("(%s) register fail with (%d)\n",
			umts_modem.name, ret);

	ret = umts_modem_cfg_gpio();
	if (ret < 0)
		return ret;

	/* link_pm platform data */
	ret = platform_device_register(&modem_linkpm_plat_device);
	if (ret < 0)
		mif_err("(%s) register fail with (%d)\n",
			modem_linkpm_plat_device.name, ret);

	ret = modem_link_pm_config_gpio();
	if (ret < 0)
		return ret;

	return ret;
}
late_initcall(init_modem);
