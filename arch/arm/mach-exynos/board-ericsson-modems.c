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

/* Modem configuraiton for Ericsson (P-Q + M74XX)*/

#include <linux/platform_device.h>
//#include <linux/cpufreq_pegasusq.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/usb.h>
#include <linux/usb/ehci_def.h>
#include <linux/platform_data/modem_v2.h>
#include <mach/cpufreq.h>
#include <mach/dev.h>
#include <mach/map.h>
#include <mach/sec_modem.h>
#include <plat/gpio-cfg.h>
#include <linux/platform_data/sipc_def.h>

/* Modem Interface GPIOs - STE specific */
#define GPIO_HOST_ACTIVE	GPIO_ACTIVE_STATE
#define GPIO_AP2CP_RESET	GPIO_PEN_IRQ_18V
#define GPIO_CP_RESOUT		GPIO_CP_PMU_RST

#if 0
static int exynos_frequency_lock(struct device *dev, unsigned int qos_val);
static int exynos_frequency_unlock(struct device *dev);
#endif

/* umts target platform data */
static struct modem_io_t umts_io_devices[] = {
	[0] = {
		.name = "umts_ipc0",
		.id = SIPC5_CH_ID_FMT_0,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
		.attr = IODEV_ATTR(ATTR_SIPC5),
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
		.rxq_max = 8192,
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
	[12] = { /* for TP testing using iperf*/
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

static int umts_link_ldo_enble(bool enable)
{
	/* Exynos HSIC V1.2 LDO was controlled by kernel */
	return 0;
}

#ifdef EHCI_REG_DUMP
struct dump_ehci_regs {
	unsigned caps_hc_capbase;
	unsigned caps_hcs_params;
	unsigned caps_hcc_params;
	unsigned reserved0;
	struct ehci_regs regs;
	unsigned port_usb;  /*0x54*/
	unsigned port_hsic0;
	unsigned port_hsic1;
	unsigned reserved[12];
	unsigned insnreg00;	/*0x90*/
	unsigned insnreg01;
	unsigned insnreg02;
	unsigned insnreg03;
	unsigned insnreg04;
	unsigned insnreg05;
	unsigned insnreg06;
	unsigned insnreg07;
};

struct s5p_ehci_hcd_stub {
	struct device *dev;
	struct usb_hcd *hcd;
	struct clk *clk;
	int power_on;
};
/* for EHCI register dump */
struct dump_ehci_regs sec_debug_ehci_regs;

#define pr_hcd(s, r) printk(KERN_DEBUG "hcd reg(%s):\t 0x%08x\n", s, r)
static void print_ehci_regs(struct dump_ehci_regs *base)
{
	pr_hcd("HCCPBASE", base->caps_hc_capbase);
	pr_hcd("HCSPARAMS", base->caps_hcs_params);
	pr_hcd("HCCPARAMS", base->caps_hcc_params);
	pr_hcd("USBCMD", base->regs.command);
	pr_hcd("USBSTS", base->regs.status);
	pr_hcd("USBINTR", base->regs.intr_enable);
	pr_hcd("FRINDEX", base->regs.frame_index);
	pr_hcd("CTRLDSSEGMENT", base->regs.segment);
	pr_hcd("PERIODICLISTBASE", base->regs.frame_list);
	pr_hcd("ASYNCLISTADDR", base->regs.async_next);
	pr_hcd("CONFIGFLAG", base->regs.configured_flag);
	pr_hcd("PORT0 Status/Control", base->port_usb);
	pr_hcd("PORT1 Status/Control", base->port_hsic0);
	pr_hcd("PORT2 Status/Control", base->port_hsic1);
	pr_hcd("INSNREG00", base->insnreg00);
	pr_hcd("INSNREG01", base->insnreg01);
	pr_hcd("INSNREG02", base->insnreg02);
	pr_hcd("INSNREG03", base->insnreg03);
	pr_hcd("INSNREG04", base->insnreg04);
	pr_hcd("INSNREG05", base->insnreg05);
	pr_hcd("INSNREG06", base->insnreg06);
	pr_hcd("INSNREG07", base->insnreg07);
}

static void debug_ehci_reg_dump(struct device *hdev)
{
	struct s5p_ehci_hcd_stub *s5p_ehci = dev_get_drvdata(hdev);
	struct usb_hcd *hcd = s5p_ehci->hcd;
	char *buf = (char *)&sec_debug_ehci_regs;

	if (s5p_ehci->power_on && hcd) {
		memcpy(buf, hcd->regs, 0xB);
		memcpy(buf + 0x10, hcd->regs + 0x10, 0x1F);
		memcpy(buf + 0x50, hcd->regs + 0x50, 0xF);
		memcpy(buf + 0x90, hcd->regs + 0x90, 0x1F);
		print_ehci_regs(hcd->regs);
	}
}
#else
#define debug_ehci_reg_dump (NULL)
#endif

#define EHCI_PORTREG_OFFSET 0x54
#define WAIT_CONNECT_CHECK_CNT 30
static u32 __iomem *port_reg;
static int s5p_ehci_port_reg_init(void)
{
	if (port_reg) {
		mif_info("port reg aleady initialized\n");
		return -EBUSY;
	}

	port_reg = ioremap((S5P_PA_EHCI + EHCI_PORTREG_OFFSET), SZ_16);
	if (!port_reg) {
		mif_err("fail to get port reg address\n");
		return -EINVAL;
	}
	mif_info("port reg get success (%p)\n", port_reg);

	return 0;
}

static void s5p_ehci_wait_cp_resume(int port)
{
	u32 __iomem *portsc;
	int cnt = WAIT_CONNECT_CHECK_CNT;
	u32 val;

	if (!port_reg) {
		mif_err("port reg addr invalid\n");
		return;
	}
	portsc = &port_reg[port-1];

	do {
		msleep(20);
		val = readl(portsc);
		mif_info("port(%d), reg(0x%x)\n", port, val);

		if (!get_cp_active_state())
			break;
	} while (cnt-- && !(val & PORT_CONNECT));

	if (!(val & PORT_CONNECT))
		mif_err("port(%d) connect failed\n", port);
}

static int cp_force_crash_exit(void);

#if 0
#define MAX_FREQ_LEVEL	4

static struct {
	unsigned int throughput;	/* KBps */
	unsigned int cpufreq;		/* Mhz */
	unsigned int busfreq;		/* MIF/INT: Mhz */
	unsigned int mincpu;		/* Minimum number of core */
} freq_table[MAX_FREQ_LEVEL] = {
	{ 2 * 1024, 300, 147147, 1 },       /* 0x400 */  /*10Mb DL 5Mb UL */
	{ 10 * 1024, 800, 176176, 2 },      /* 0xC00 */
	{ 18 * 1024, 1000, 293176, 3 },     /* 0x2800 */
	{ 25 * 1024, 1600, 440220, 4 },     /* 0x5000 */
};
#endif

static struct modemlink_pm_data modem_link_pm_data = {
	.link_ldo_enable = umts_link_ldo_enble,

	/* OOB Signalling */
	.gpio_link_active = GPIO_HOST_ACTIVE,
	.gpio_link_hostwake = GPIO_IPC_HOST_WAKEUP,
	.gpio_link_slavewake = GPIO_IPC_SLAVE_WAKEUP,

	.cp_force_crash_exit = cp_force_crash_exit,
	.ehci_reg_dump = debug_ehci_reg_dump,
	.port = 2,
	.wait_cp_resume = s5p_ehci_wait_cp_resume,
	.freqlock = ATOMIC_INIT(0),
#if 0
	.freq_lock = exynos_frequency_lock,
	.freq_unlock = exynos_frequency_unlock,
#endif
};

static struct platform_device modem_linkpm_plat_device = {
	.name = "link_pm_hsic",
	.id = -1,
	.dev = {
		.platform_data = &modem_link_pm_data,
	},
};

static struct modemlink_pm_link_activectl active_ctl;


static struct modem_data umts_modem_data = {
	.name = "m74xx",

	/* CP on/off/reset */
	.gpio_cp_on = GPIO_PHONE_ON,
	.gpio_cp_reset = GPIO_AP2CP_RESET,
	.gpio_pda_active = GPIO_PDA_ACTIVE,

	/* CP ramdump */
	.gpio_phone_active = GPIO_PHONE_ACTIVE,
	.gpio_ap_dump_int = GPIO_AP_DUMP_INT, 
	.gpio_cp_dump_int = GPIO_CP_DUMP_INT,

#ifdef GPIO_SIM_DETECT
	.gpio_sim_detect = GPIO_SIM_DETECT,
#endif

	.modem_type = ERICSSON_M74XX,
	.link_types = LINKTYPE(LINKDEV_HSIC),
	.modem_net = UMTS_NETWORK,
	.use_handover = false,

	.num_iodevs = ARRAY_SIZE(umts_io_devices),
	.iodevs = umts_io_devices,

	.cp_force_crash = cp_force_crash_exit,
	.max_acm_channel = 1,
	.max_link_channel = 2,
	.ipc_version = SIPC_VER_50,
};

#if 0
void set_host_states(struct platform_device *pdev, int type)
{
	int val;

	if (umts_modem_data.gpio_cp_reset)
		val = gpio_get_value(umts_modem_data.gpio_cp_reset);
	else
		val = 1;

	if (!val) {
		mif_info("CP not ready, Active State low\n");
		return;
	}

	if (active_ctl.gpio_initialized) {
		mif_err("HOST ACTIVE state=%d\n", type);
		gpio_direction_output(modem_link_pm_data.gpio_link_active,
			type);
	}
}
#endif

int get_hostwake_state(void)
{
	if (!gpio_get_value(umts_modem_data.gpio_phone_active)) {
		mif_info("CP is not active\n");
		return 0;
	}

	/* HOST_WAKEUP is active-high irq */
	return gpio_get_value(modem_link_pm_data.gpio_link_hostwake);
}

void set_hsic_lpa_states(int states)
{
	int val;

	if (umts_modem_data.gpio_cp_reset)
		val = gpio_get_value(umts_modem_data.gpio_cp_reset);
	else
		val = 1;

	mif_info("states = %d\n", states);

	if (val) {
		switch (states) {
		case STATE_HSIC_LPA_ENTER:
			gpio_set_value(modem_link_pm_data.gpio_link_active, 0);
			gpio_set_value(umts_modem_data.gpio_pda_active, 0);
			mif_info("enter: active state(%d), pda active(%d)\n",
				gpio_get_value(modem_link_pm_data.gpio_link_active),
				gpio_get_value(umts_modem_data.gpio_pda_active));
			break;
		case STATE_HSIC_LPA_WAKE:
			gpio_set_value(umts_modem_data.gpio_pda_active, 1);
			mif_info("wake: pda active(%d)\n",
				gpio_get_value(umts_modem_data.gpio_pda_active));
			break;
		case STATE_HSIC_LPA_PHY_INIT:
			gpio_set_value(umts_modem_data.gpio_pda_active, 1);
			break;
		}
	}
}

int get_cp_active_state(void)
{
	return gpio_get_value(umts_modem_data.gpio_phone_active);
}

/* if use more than one modem device, then set id num */
static struct platform_device umts_modem = {
	.name = "mif_sipc5",
	.id = -1,
	.dev = {
		.platform_data = &umts_modem_data,
	},
};

static void umts_modem_cfg_gpio(void)
{
	int ret = 0;

	unsigned gpio_cp_on = umts_modem_data.gpio_cp_on;
	unsigned gpio_cp_rst = umts_modem_data.gpio_cp_reset;
	unsigned gpio_pda_active = umts_modem_data.gpio_pda_active;
	unsigned gpio_phone_active = umts_modem_data.gpio_phone_active;
	unsigned gpio_cp_dump_int = umts_modem_data.gpio_cp_dump_int;
	unsigned gpio_ap_dump_int = umts_modem_data.gpio_ap_dump_int;
	unsigned gpio_flm_uart_sel = umts_modem_data.gpio_flm_uart_sel;
	unsigned gpio_sim_detect = umts_modem_data.gpio_sim_detect;

	if (gpio_cp_on) {
		ret = gpio_request(gpio_cp_on, "CP_ON");
		if (ret)
			mif_err("fail to request gpio %s:%d\n", "CP_ON", ret);
		gpio_direction_output(gpio_cp_on, 0);
	}

	if (gpio_cp_rst) {
		ret = gpio_request(gpio_cp_rst, "AP2CP_RESET");
		if (ret)
			mif_err("fail to request gpio %s:%d\n", "CP_RST", ret);
		gpio_direction_output(gpio_cp_rst, 0);
		s3c_gpio_setpull(gpio_cp_rst, S3C_GPIO_PULL_NONE);
	}

	if (gpio_pda_active) {
		ret = gpio_request(gpio_pda_active, "PDA_ACTIVE");
		if (ret)
			mif_err("fail to request gpio %s:%d\n", "PDA_ACTIVE",
				ret);
		gpio_direction_output(gpio_pda_active, 0);
	}

	if (gpio_phone_active) {
		ret = gpio_request(gpio_phone_active, "PHONE_ACTIVE");
		if (ret)
			mif_err("fail to request gpio %s:%d\n", "PHONE_ACTIVE",
				ret);
		gpio_direction_input(gpio_phone_active);
	}

	if (gpio_sim_detect) {
		ret = gpio_request(gpio_sim_detect, "SIM_DETECT");
		if (ret)
			mif_err("fail to request gpio %s:%d\n", "SIM_DETECT",
				ret);

		/* gpio_direction_input(gpio_sim_detect); */
		irq_set_irq_type(gpio_to_irq(gpio_sim_detect),
							IRQ_TYPE_EDGE_BOTH);
	}

	if (gpio_cp_dump_int) {
		ret = gpio_request(gpio_cp_dump_int, "CP_DUMP_INT");
		if (ret)
			mif_err("fail to request gpio %s:%d\n", "CP_DUMP_INT",
				ret);
		gpio_direction_input(gpio_cp_dump_int);
	}

	if (gpio_ap_dump_int) {
		ret = gpio_request(gpio_ap_dump_int, "AP_DUMP_INT");
		if (ret)
			mif_err("fail to request gpio %s:%d\n", "AP_DUMP_INT",
				ret);
		gpio_direction_output(gpio_ap_dump_int, 0);
	}

	if (gpio_flm_uart_sel) {
		ret = gpio_request(gpio_flm_uart_sel, "GPS_UART_SEL");
		if (ret)
			mif_err("fail to request gpio %s:%d\n", "FLM_SEL",
				ret);
	}

	if (gpio_phone_active)
		irq_set_irq_type(gpio_to_irq(gpio_phone_active),
							IRQ_TYPE_LEVEL_HIGH);

	/* set low unused gpios between AP and CP */
	ret = gpio_request(GPIO_SUSPEND_REQUEST, "SUS_REQ");
	if (ret) {
		mif_err("fail to request gpio %s : %d\n", "SUS_REQ", ret);
	} else {
		gpio_direction_output(GPIO_SUSPEND_REQUEST, 0);
		s3c_gpio_setpull(GPIO_SUSPEND_REQUEST, S3C_GPIO_PULL_NONE);
	}
	mif_info("umts_modem_cfg_gpio done\n");
}

static int cp_force_crash_exit(void)
{
	unsigned gpio_ap_dump_int = umts_modem_data.gpio_ap_dump_int;
	unsigned gpio_phone_active = umts_modem_data.gpio_phone_active;

	if (!gpio_ap_dump_int || !gpio_phone_active)
		return -ENXIO;

	if (!gpio_get_value(gpio_phone_active))
		return 0;

	if (gpio_get_value(gpio_ap_dump_int)) {
		gpio_set_value(gpio_ap_dump_int, 0);
		msleep(20);
	}
	gpio_set_value(gpio_ap_dump_int, 1);
	msleep(20);
	mif_info("ap_dump_int(%d) is set to high=%d\n",
		gpio_ap_dump_int, gpio_get_value(gpio_ap_dump_int));

	return 0;
}

#if 0
static int exynos_frequency_lock(struct device *dev, unsigned int qosval)
{
	int i, ret = 0;
	unsigned int level, cpufreq, busfreq, mincpu;
	struct device *busdev = dev_get("exynos-busfreq");

	for (i = 0; i < MAX_FREQ_LEVEL; i++)
		if (qosval < freq_table[i].throughput)
			break;

	i = min(i, MAX_FREQ_LEVEL - 1);

	if (!i) {
		if (atomic_read(&modem_link_pm_data.freqlock)) {
			mif_info("locked level = %d, requested level = %d\n",
				atomic_read(&modem_link_pm_data.freqlock), i);
			exynos_frequency_unlock(dev);
			atomic_set(&modem_link_pm_data.freqlock, i);
		}
		goto exit;
	}

	cpufreq = freq_table[i].cpufreq;
	busfreq = freq_table[i].busfreq;
	mincpu = freq_table[i].mincpu;

	if (atomic_read(&modem_link_pm_data.freqlock) != i) {
		mif_info("locked level = %d, requested level = %d\n",
			atomic_read(&modem_link_pm_data.freqlock), i);

		exynos_frequency_unlock(dev);
		mdelay(50);

		/* cpu frequency lock */
		ret = exynos_cpufreq_get_level(cpufreq * 1000, &level);
		if (ret < 0) {
			mif_err("ERR: exynos_cpufreq_get_level fail: %d\n",
					ret);
			goto exit;
		}

		ret = exynos_cpufreq_lock(DVFS_LOCK_ID_USB_IF, level);
		if (ret < 0) {
			mif_err("ERR: exynos_cpufreq_lock fail: %d\n", ret);
			goto exit;
		}

		/* bus frequncy lock */
		if (!busdev) {
			mif_err("ERR: busdev is not exist\n");
			ret = -ENODEV;
			goto exit;
		}

		ret = dev_lock(busdev, dev, busfreq);
		if (ret < 0) {
			mif_err("ERR: dev_lock error: %d\n", ret);
			goto exit;
		}

		/* lock minimum number of cpu cores */
		cpufreq_pegasusq_min_cpu_lock(mincpu);

		atomic_set(&modem_link_pm_data.freqlock, i);

		mif_info("level=%d, cpufreq=%dMHz, busfreq=%dMHz, mincpu=%d\n",
			level, cpufreq, busfreq, mincpu);
	}
exit:
	return ret;
}

static int exynos_frequency_unlock(struct device *dev)
{
	int ret = 0;
	struct device *busdev = dev_get("exynos-busfreq");

	if (atomic_read(&modem_link_pm_data.freqlock) != 0) {
		mif_debug("unlocking level = %d\n", atomic_read(&modem_link_pm_data.freqlock));

		/* cpu frequency unlock */
		exynos_cpufreq_lock_free(DVFS_LOCK_ID_USB_IF);

		/* bus frequency unlock */
		ret = dev_unlock(busdev, dev);
		if (ret < 0) {
			mif_err("ERR: dev_unlock error: %d\n", ret);
			goto exit;
		}

		/* unlock minimum number of cpu cores */
		cpufreq_pegasusq_min_cpu_unlock();

		atomic_set(&modem_link_pm_data.freqlock, 0);
	}
exit:
	return ret;
}
#endif

static void modem_link_pm_config_gpio(void)
{
	int ret = 0;

	unsigned gpio_link_enable = modem_link_pm_data.gpio_link_enable;
	unsigned gpio_link_active = modem_link_pm_data.gpio_link_active;
	unsigned gpio_link_hostwake = modem_link_pm_data.gpio_link_hostwake;
	unsigned gpio_link_slavewake = modem_link_pm_data.gpio_link_slavewake;

	if (gpio_link_enable) {
		ret = gpio_request(gpio_link_enable, "LINK_EN");
		if (ret) {
			mif_err("fail to request gpio %s:%d\n", "LINK_EN",
				ret);
		}
		gpio_direction_output(gpio_link_enable, 0);
	}

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
		s3c_gpio_setpull(gpio_link_hostwake, S3C_GPIO_PULL_DOWN);
	}

	if (gpio_link_slavewake) {
		ret = gpio_request(gpio_link_slavewake, "SLAVEWAKE");
		if (ret) {
			mif_err("fail to request gpio %s:%d\n", "SLAVEWAKE",
				ret);
		}
		gpio_direction_output(gpio_link_slavewake, 0);
	}

	if (gpio_link_hostwake)
		irq_set_irq_type(gpio_to_irq(gpio_link_hostwake),
							IRQ_TYPE_EDGE_BOTH);

	active_ctl.gpio_initialized = 1;

	mif_info("modem_link_pm_config_gpio done\n");
}

static void board_set_simdetect_polarity(void)
{
	if (umts_modem_data.gpio_sim_detect)
		umts_modem_data.sim_polarity = 0;
}

static void board_gpio_export(unsigned gpio, unsigned dir, const char *label)
{
	int ret;

	ret = gpio_request(gpio, label);
	if (ret != 0)
		pr_err("fail to request gpio %s : %d\n", label, ret);

	if (dir)
		gpio_direction_output(gpio, 0);
	else
		gpio_direction_input(gpio);

	ret = gpio_export(gpio, 1);
	if (ret != 0)
		pr_err("%s: gpio_export error\n", __func__);

	ret = gpio_export_link(&umts_modem.dev, label, gpio);
	if (ret != 0)
		pr_err("%s: gpio_export_link error\n", __func__);
}

static int __init init_modem(void)
{
	int ret;

	mif_info("init_modem\n");

	/* umts gpios configuration */
	umts_modem_cfg_gpio();
	modem_link_pm_config_gpio();
	board_set_simdetect_polarity();
	s5p_ehci_port_reg_init();

	ret = platform_device_register(&modem_linkpm_plat_device);
	if (ret < 0)
		mif_err("(%s) register fail with (%d)\n",
				modem_linkpm_plat_device.name, ret);

	ret = platform_device_register(&umts_modem);
	if (ret < 0)
		mif_err("(%s) register fail with (%d)\n",
				umts_modem.name, ret);

	/* to be used by CBD */
	board_gpio_export(GPIO_CP_RESOUT, 0, "cp_resout");
	board_gpio_export(GPIO_IPC_HOST_WAKEUP, 0, "hsic_wake_ca");
	board_gpio_export(GPIO_IPC_SLAVE_WAKEUP, 1, "hsic_wake_ac");
	board_gpio_export(GPIO_HOST_ACTIVE, 1, "hsic_resume_ac");

	/* reason for export? */
	board_gpio_export(GPIO_PHONE_ON, 1, "cp_on");
	board_gpio_export(GPIO_AP_DUMP_INT, 1, "ap_dump_int");

	mif_info("forcefully setting GPIO_AP2CP_RESET to 1\n");
	gpio_set_value(GPIO_AP2CP_RESET, 1);

	mif_info("init_modem-\n");
	return ret;
}
late_initcall(init_modem);
