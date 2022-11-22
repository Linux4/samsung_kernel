/* linux/arch/arm/mach-xxxx/board-baffinve-modems.c
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/vmalloc.h>
#include <linux/if_arp.h>
#include <mach/mfp-pxa1088-delos3gvia.h>
#include "common.h"
#include <mach/irqs.h>
#include <mach/delos3gvia-modem.h>
/* inlcude platform specific file */
#include <linux/platform_data/modem.h>
//#include <mach/sec_modem.h>
//#include <mach/gpio.h>
//#include <mach/gpio-exynos4.h>
//#include <plat/gpio-cfg.h>
//#include <mach/regs-mem.h>
//#include <plat/regs-srom.h>

//#include <plat/devs.h>
//#include <plat/ehci.h>
#if defined(CONFIG_LINK_DEVICE_PLD)
#include <linux/spi/spi.h>
#include <mach/pld_pdata.h>
#endif


#define SROM_CS0_BASE		0x04000000
#define SROM_WIDTH		0x01000000
#define SROM_NUM_ADDR_BITS	15

/*
 * For SROMC Configuration:
 * SROMC_ADDR_BYTE enable for byte access
 */
#define SROMC_DATA_16		0x1
#define SROMC_ADDR_BYTE		0x2
#define SROMC_WAIT_EN		0x4
#define SROMC_BYTE_EN		0x8
#define SROMC_MASK		0xF

/* Memory attributes */
enum sromc_attr {
	MEM_DATA_BUS_8BIT = 0x00000000,
	MEM_DATA_BUS_16BIT = 0x00000001,
	MEM_BYTE_ADDRESSABLE = 0x00000002,
	MEM_WAIT_EN = 0x00000004,
	MEM_BYTE_EN = 0x00000008,

};

/* DPRAM configuration */
struct sromc_cfg {
	enum sromc_attr attr;
	unsigned size;
	unsigned csn;		/* CSn #                        */
	unsigned addr;		/* Start address (physical)     */
	unsigned end;		/* End address (physical)       */
};

/* DPRAM access timing configuration */
struct sromc_access_cfg {
	u32 tacs;		/* Address set-up before CSn            */
	u32 tcos;		/* Chip selection set-up before OEn     */
	u32 tacc;		/* Access cycle                         */
	u32 tcoh;		/* Chip selection hold on OEn           */
	u32 tcah;		/* Address holding time after CSn       */
	u32 tacp;		/* Page mode access cycle at Page mode  */
	u32 pmc;		/* Page Mode config                     */
};

/* For MDM6600 EDPRAM (External DPRAM) */
#define MSM_EDPRAM_SIZE		0x10000	/* 8 KB */

#define INT_MASK_REQ_ACK_F	0x0020
#define INT_MASK_REQ_ACK_R	0x0010
#define INT_MASK_RES_ACK_F	0x0008
#define INT_MASK_RES_ACK_R	0x0004
#define INT_MASK_SEND_F		0x0002
#define INT_MASK_SEND_R		0x0001

#define INT_MASK_REQ_ACK_RFS	0x0400	/* Request RES_ACK_RFS           */
#define INT_MASK_RES_ACK_RFS	0x0200	/* Response of REQ_ACK_RFS       */
#define INT_MASK_SEND_RFS	0x0100	/* Indicate sending RFS data     */

/* Function prototypes */
static void config_dpram_port_gpio(void);
static int __init init_modem(void);

static struct sromc_cfg msm_edpram_cfg = {
	.attr = (MEM_DATA_BUS_8BIT),
	.size = 0x10000,
};

typedef unsigned char	u8;
typedef unsigned short	u16;
typedef unsigned int		u32;

#define DELAYED_WORK
#ifdef DELAYED_WORK
static struct workqueue_struct *enable_wq;
void pld_loopback_test(struct work_struct *work);
static DECLARE_DELAYED_WORK(enable_work, pld_loopback_test);
#else
void pld_loopback_test(void);
#endif

#define M_32_SWAP(a) {					\
		u32 _tmp;				\
		_tmp = a;					\
		((u8 *)&a)[0] = ((u8 *)&_tmp)[3]; \
		((u8 *)&a)[1] = ((u8 *)&_tmp)[2]; \
		((u8 *)&a)[2] = ((u8 *)&_tmp)[1]; \
		((u8 *)&a)[3] = ((u8 *)&_tmp)[0]; \
		}

#define M_16_SWAP(a) {					\
		u16 _tmp;					\
		_tmp = (u16)a;			\
		((u8 *)&a)[0] = ((u8 *)&_tmp)[1];	\
		((u8 *)&a)[1] = ((u8 *)&_tmp)[0];	\
		}

#define PLD_ADDR_MASK(x)	(0x00003FFF & (unsigned long)(x))
#define IPC_RW_BIT 0x00008000

#define CP_WAKEUP_HOST	GPIO073_GPIO_73
#define HOST_WAKEUP_CP	GPIO074_GPIO_74
#define AP_SPI_INT	GPIO007_GPIO_7
#define CP_READY	GPIO031_GPIO_31
#define AP_READY	GPIO069_GPIO_69
#define FPGA_RST_N	GPIO070_GPIO_70
#define FPGA_CDONE	GPIO071_GPIO_71
#define FPGA_CRESET_B	GPIO072_GPIO_72
#define AP_SPI_ADDR	GPIO078_GPIO_78
#define VIA_ON	GPIO097_GPIO_97
#define FPGA_SPI_CS_N	ND_IO10_ND_DAT10 | MFP_PULL_LOW
#define VIA_PS_HOLD_OFF	GPIO_CLK_REQ | MFP_PULL_LOW
#define VIA_RST	GPIO_VCXO_REQ | MFP_PULL_LOW
#define AP_CP_GPIO_BCK1	ND_RDY1_GPIO_1
#define AP_CP_GPIO_BCK2	DF_WEn_DF_WEn

static unsigned long baffinve_pin_config[] __initdata = {
	CP_WAKEUP_HOST,
	HOST_WAKEUP_CP,
	AP_SPI_INT,
	CP_READY,
	AP_READY,
	FPGA_RST_N,
	FPGA_CDONE,
	FPGA_CRESET_B,
	AP_SPI_ADDR,
	VIA_ON,
	FPGA_SPI_CS_N,
	VIA_PS_HOLD_OFF,
	VIA_RST,
	AP_CP_GPIO_BCK1,
	AP_CP_GPIO_BCK2,
};

static unsigned long baffinve_spi_pin_config[] __initdata = {
	ND_IO9_FPGA_SPI_CLK | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	ND_IO10_ND_DAT10,
	ND_IO11_FPGA_SPI_MOSI | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	ND_IO12_FPGA_SPI_MISO | MFP_PULL_HIGH | MFP_LPM_FLOAT,
};

/*
	mbx_ap2cp +			0x0
	magic_code +
	access_enable +
	padding +
	mbx_cp2ap +			0x1000
	magic_code +
	access_enable +
	padding +
	fmt_tx_head + fmt_tx_tail + fmt_tx_buff +		0x2000
	raw_tx_head + raw_tx_tail + raw_tx_buff +
	fmt_rx_head + fmt_rx_tail + fmt_rx_buff +		0x3000
	raw_rx_head + raw_rx_tail + raw_rx_buff +
  =	2 +
	4094 +
	2 +
	4094 +
	2 +
	2 +
	2 + 2 + 1020 +
	2 + 2 + 3064 +
	2 + 2 + 1020 +
	2 + 2 + 3064
 */

#define MSM_DP_FMT_TX_BUFF_SZ	1024
#define MSM_DP_RAW_TX_BUFF_SZ	3072
#define MSM_DP_FMT_RX_BUFF_SZ	1024
#define MSM_DP_RAW_RX_BUFF_SZ	3072

#define MAX_MSM_EDPRAM_IPC_DEV	2	/* FMT, RAW */

struct msm_edpram_ipc_cfg {
	u16 mbx_ap2cp;
	u16 magic_ap2cp;
	u16 access_ap2cp;
	u16 fmt_tx_head;
	u16 raw_tx_head;
	u16 fmt_rx_tail;
	u16 raw_rx_tail;
	u16 temp1;
	u8 padding1[4080];

	u16 mbx_cp2ap;
	u16 magic_cp2ap;
	u16 access_cp2ap;
	u16 fmt_tx_tail;
	u16 raw_tx_tail;
	u16 fmt_rx_head;
	u16 raw_rx_head;
	u16 temp2;
	u8 padding2[4080];

	u8 fmt_tx_buff[MSM_DP_FMT_TX_BUFF_SZ];
	u8 raw_tx_buff[MSM_DP_RAW_TX_BUFF_SZ];
	u8 fmt_rx_buff[MSM_DP_FMT_RX_BUFF_SZ];
	u8 raw_rx_buff[MSM_DP_RAW_RX_BUFF_SZ];

	u8 padding3[16384];

	u16 address_buffer;
};

#define DP_BOOT_CLEAR_OFFSET    0
#define DP_BOOT_RSRVD_OFFSET    0
#define DP_BOOT_SIZE_OFFSET     0x2
#define DP_BOOT_TAG_OFFSET      0x4
#define DP_BOOT_COUNT_OFFSET    0x6

#define DP_BOOT_FRAME_SIZE_LIMIT     0x1000	/* 15KB = 15360byte = 0x3C00 */

#if defined(CONFIG_LINK_DEVICE_PLD)
static struct pld_ipc_map msm_ipc_map;
#else
static struct dpram_ipc_map msm_ipc_map;
#endif

static struct modemlink_dpram_data msm_edpram = {
	.type = PLD_DPRAM,
	.disabled = true,

	.ipc_map = &msm_ipc_map,

	.boot_size_offset = DP_BOOT_SIZE_OFFSET,
	.boot_tag_offset = DP_BOOT_TAG_OFFSET,
	.boot_count_offset = DP_BOOT_COUNT_OFFSET,
	.max_boot_frame_size = DP_BOOT_FRAME_SIZE_LIMIT,

#if defined(CONFIG_LINK_DEVICE_PLD)
	.aligned = 1,
#endif
};

/*
** CDMA target platform data
*/

#if 1
static struct modem_io_t cdma_io_devices[] = {
	[0] = {
		.name = "cdma_boot0",
		.id = 0,
		.format = IPC_BOOT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
		.app = "CBD"
	},
	[1] = {
		.name = "cdma_ipc0",
		.id = 235,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
		.app = "RIL"
	},
	[2] = {
		.name = "cdma_rfs0",
		.id = 245,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
		.app = "RFS"
	},
	[3] = {
		.name = "cdma_multipdp",
		.id = 0,
		.format = IPC_MULTI_RAW,
		.io_type = IODEV_DUMMY,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[4] = {
		.name = "cdma_rmnet0",
		.id = 10,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[5] = {
		.name = "cdma_rmnet1",
		.id = 11,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[6] = {
		.name = "cdma_rmnet2",
		.id = 12,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[7] = {
		.name = "cdma_rmnet3",
		.id = 13,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[8] = {
		.name = "cdma_rmnet4",
		.id = 7,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[9] = {
		.name = "cdma_rmnet5", /* DM Port IO device */
		.id = 26,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[10] = {
		.name = "cdma_rmnet6", /* AT CMD IO device */
		.id = 17,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[11] = {
		.name = "cdma_ramdump0",
		.id = 0,
		.format = IPC_RAMDUMP,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[12] = {
		.name = "cdma_cplog",
		.id = 29,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
		.app = "DIAG"
	},
};

#else
static struct modem_io_t cdma_io_devices[] = {
	[0] = {
		.name = "cdma_boot0",
		.id = 0x1,
		.format = IPC_BOOT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD)
	},
	[1] = {
		.name = "cdma_ramdump0",
		.id = 0x1,
		.format = IPC_RAMDUMP,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[2] = {
		.name = "cdma_ipc0",
		.id = 0x00,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[3] = {
		.name = "umts_ipc0",
		.id = 0x01,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[4] = {
		.name = "multi_pdp",
		.id = 0x1,
		.format = IPC_MULTI_RAW,
		.io_type = IODEV_DUMMY,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[5] = {
		.name = "cdma_CSD",
		.id = (1 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[6] = {
		.name = "cdma_FOTA",
		.id = (2 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[7] = {
		.name = "cdma_GPS",
		.id = (5 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[8] = {
		.name = "cdma_XTRA",
		.id = (6 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[9] = {
		.name = "cdma_CDMA",
		.id = (7 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[10] = {
		.name = "cdma_EFS",
		.id = (8 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[11] = {
		.name = "cdma_TRFB",
		.id = (9 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[12] = {
		.name = "rmnet0",
		.id = 0x2A,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[13] = {
		.name = "rmnet1",
		.id = 0x2B,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[14] = {
		.name = "rmnet2",
		.id = 0x2C,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[15] = {
		.name = "rmnet3",
		.id = 0x2D,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[16] = {
		.name = "cdma_SMD",
		.id = (25 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[17] = {
		.name = "cdma_VTVD",
		.id = (26 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[18] = {
		.name = "cdma_VTAD",
		.id = (27 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[19] = {
		.name = "cdma_VTCTRL",
		.id = (28 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[20] = {
		.name = "cdma_VTENT",
		.id = (29 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
};
#endif

static struct modem_data cdma_modem_data = {
	.name = "cbp82",

	.gpio_cp_on = mfp_to_gpio(VIA_ON),
	.gpio_cp_off = mfp_to_gpio(VIA_PS_HOLD_OFF),
	.gpio_reset_req_n = 0,
	.gpio_cp_reset = mfp_to_gpio(VIA_RST),

	.gpio_pda_active = mfp_to_gpio(AP_READY),

	.gpio_phone_active = mfp_to_gpio(CP_WAKEUP_HOST),
	.irq_phone_active = MMP_GPIO_TO_IRQ(mfp_to_gpio(CP_WAKEUP_HOST)),

	.gpio_flm_uart_sel = 0,

	.gpio_ipc_int2ap = mfp_to_gpio(AP_SPI_INT),
	.irq_ipc_int2ap = MMP_GPIO_TO_IRQ(mfp_to_gpio(AP_SPI_INT)),
	.irqf_ipc_int2ap = IRQF_TRIGGER_FALLING,

	.gpio_cp_dump_int = mfp_to_gpio(CP_READY),
	.gpio_cp_warm_reset = 0,

	.use_handover = false,

#if defined(CONFIG_SIM_DETECT)
	.gpio_sim_detect = GPIO_CP_SIM_DETECT,
	.irq_sim_detect = CP_SIM_DETECT_IRQ,
#else
	.gpio_sim_detect = 0,
#endif

	.gpio_fpga2_creset = mfp_to_gpio(FPGA_CRESET_B),
	.gpio_fpga2_cdone = mfp_to_gpio(FPGA_CDONE),
	.gpio_fpga2_rst_n = mfp_to_gpio(FPGA_RST_N),
	.gpio_fpga2_cs_n = mfp_to_gpio(FPGA_SPI_CS_N),

	.modem_net = CDMA_NETWORK,
	.modem_type = VIA_CBP82,
	.link_types = LINKTYPE(LINKDEV_PLD),
	.link_name = "cbp82_pld_spi",
	.dpram = &msm_edpram,

#if defined(CONFIG_MACH_M0_CTC) && !defined(CONFIG_GSM_MODEM_ESC6270)
	.ipc_version = SIPC_VER_42,
#else
	.ipc_version = SIPC_VER_50,
#endif
	.max_ipc_dev = (IPC_RAW + 1),

	.num_iodevs = ARRAY_SIZE(cdma_io_devices),
	.iodevs = cdma_io_devices,
};

static struct platform_device cdma_modem = {
	//.name = "modem_if",
	.name = "mif_sipc5",
	.id = -1,
	.num_resources = 0,
	.resource = NULL,
	.dev = {
		.platform_data = &cdma_modem_data,
	},
};

static void config_cdma_modem_gpio(void)
{
	int err;
	unsigned gpio_cp_on = cdma_modem_data.gpio_cp_on;
	unsigned gpio_cp_off = cdma_modem_data.gpio_cp_off;
	unsigned gpio_rst_req_n = cdma_modem_data.gpio_reset_req_n;
	unsigned gpio_cp_rst = cdma_modem_data.gpio_cp_reset;
	unsigned gpio_pda_active = cdma_modem_data.gpio_pda_active;
	unsigned gpio_phone_active = cdma_modem_data.gpio_phone_active;
	unsigned gpio_flm_uart_sel = cdma_modem_data.gpio_flm_uart_sel;

	unsigned gpio_ipc_int2ap = cdma_modem_data.gpio_ipc_int2ap;
	unsigned gpio_cp_dump_int = cdma_modem_data.gpio_cp_dump_int;
	unsigned gpio_sim_detect = cdma_modem_data.gpio_sim_detect;

#if defined(CONFIG_LINK_DEVICE_PLD)
	unsigned gpio_fpga2_creset = cdma_modem_data.gpio_fpga2_creset;
	unsigned gpio_fpga2_cdone = cdma_modem_data.gpio_fpga2_cdone;
	unsigned gpio_fpga2_rst_n = cdma_modem_data.gpio_fpga2_rst_n;
	unsigned gpio_fpga2_cs_n = cdma_modem_data.gpio_fpga2_cs_n;
#endif
	pr_info("[MODEMS] <%s>\n", __func__);

	if (gpio_pda_active) {
		err = gpio_request(gpio_pda_active, "PDA_ACTIVE");
		if (err) {
			pr_err("fail to request gpio %s\n", "PDA_ACTIVE");
		} else {
			pr_info("gpio request PDA_ACTIVE \n");
			gpio_direction_output(gpio_pda_active, 0);
			//s3c_gpio_setpull(gpio_pda_active, S3C_GPIO_PULL_NONE);
			//gpio_set_value(gpio_pda_active, 0);
		}
	}

	if (gpio_phone_active) {
		err = gpio_request(gpio_phone_active, "MSM_ACTIVE");
		if (err) {
			pr_err("fail to request gpio %s\n", "MSM_ACTIVE");
		} else {
			pr_info("gpio request MSM_ACTIVE \n");
			//s3c_gpio_cfgpin(gpio_phone_active, S3C_GPIO_SFN(0xF));
			//s3c_gpio_setpull(gpio_phone_active, S3C_GPIO_PULL_NONE);
			irq_set_irq_type(gpio_phone_active, IRQ_TYPE_EDGE_BOTH);
		}
	}

	if (gpio_cp_on) {
		gpio_free(gpio_cp_on);
		err = gpio_request(gpio_cp_on, "MSM_ON");
		if (err) {
			pr_err("fail to request gpio %s\n", "MSM_ON");
		} else {
			pr_info("gpio request MSM_ON \n");
			gpio_direction_output(gpio_cp_on, 0);
			//s3c_gpio_setpull(gpio_cp_on, S3C_GPIO_PULL_NONE);
			//gpio_set_value(gpio_cp_on, 0);
		}
	}

	if (gpio_cp_off) {
		err = gpio_request(gpio_cp_off, "MSM_OFF");
		if (err) {
			pr_err("fail to request gpio %s\n", "MSM_OFF");
		} else {
			pr_info("gpio request MSM_OFF \n");
			gpio_direction_output(gpio_cp_off, 0);
			//s3c_gpio_setpull(gpio_cp_off, S3C_GPIO_PULL_NONE);
			//gpio_set_value(gpio_cp_off, 1);
		}
	}

	if (gpio_rst_req_n) {
		err = gpio_request(gpio_rst_req_n, "MSM_RST_REQ");
		if (err) {
			pr_err("fail to request gpio %s\n", "MSM_RST_REQ");
		} else {
			pr_info("gpio request MSM_RST_REQ \n");
			gpio_direction_output(gpio_rst_req_n, 0);
			//s3c_gpio_setpull(gpio_rst_req_n, S3C_GPIO_PULL_NONE);
		}
		//gpio_set_value(gpio_rst_req_n, 0);
	}

	if (gpio_cp_rst) {
		err = gpio_request(gpio_cp_rst, "MSM_RST");
		if (err) {
			pr_err("fail to request gpio %s\n", "MSM_RST");
		} else {
			pr_info("gpio request MSM_RST \n");
			gpio_direction_output(gpio_cp_rst, 0);
			//s3c_gpio_setpull(gpio_cp_rst, S3C_GPIO_PULL_NONE);
		}
		//gpio_set_value(gpio_cp_rst, 0);
	}

	if (gpio_ipc_int2ap) {
		err = gpio_request(gpio_ipc_int2ap, "MSM_DPRAM_INT");
		if (err) {
			pr_err("fail to request gpio %s\n", "MSM_DPRAM_INT");
		} else {
			pr_info("gpio request MSM_DPRAM_INT \n");
			/* Configure as a wake-up source */
			//s3c_gpio_cfgpin(gpio_ipc_int2ap, S3C_GPIO_SFN(0xF));
			//s3c_gpio_setpull(gpio_ipc_int2ap, S3C_GPIO_PULL_NONE);
		}
	}

	if (gpio_cp_dump_int) {
		err = gpio_request(gpio_cp_dump_int, "MSM_CP_DUMP_INT");
		if (err) {
			pr_err("fail to request gpio %s\n", "MSM_CP_DUMP_INT");
		} else {
			pr_info("gpio request MSM_CP_DUMP_INT \n");
			//s3c_gpio_cfgpin(gpio_cp_dump_int, S3C_GPIO_INPUT);
			//s3c_gpio_setpull(gpio_cp_dump_int, S3C_GPIO_PULL_DOWN);
		}
	}

#if defined(CONFIG_LINK_DEVICE_PLD)
	if (gpio_fpga2_creset) {
		err = gpio_request(gpio_fpga2_creset, "FPGA2_CRESET");
		if (err) {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
					"FPGA2_CRESET", gpio_fpga2_creset, err);
		} else {
			pr_info("gpio request FPGA2_CRESET \n");
			gpio_direction_output(gpio_fpga2_creset, 0);
			//s3c_gpio_setpull(gpio_fpga2_creset, S3C_GPIO_PULL_NONE);
		}
	}

	if (gpio_fpga2_cdone) {
		err = gpio_request(gpio_fpga2_cdone, "FPGA2_CDONE");
		if (err) {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
					"FPGA2_CDONE", gpio_fpga2_cdone, err);
		} else {
			pr_info("gpio request FPGA2_CDONE \n");
			//s3c_gpio_cfgpin(gpio_fpga2_cdone, S3C_GPIO_INPUT);
			//s3c_gpio_setpull(gpio_fpga2_cdone, S3C_GPIO_PULL_NONE);
		}
	}

	if (gpio_fpga2_rst_n)	{
		err = gpio_request(gpio_fpga2_rst_n, "FPGA2_RESET");
		if (err) {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
					"FPGA2_RESET", gpio_fpga2_rst_n, err);
		} else {
			pr_info("gpio request FPGA2_RESET \n");
			gpio_direction_output(gpio_fpga2_rst_n, 0);
			//s3c_gpio_setpull(gpio_fpga2_rst_n, S3C_GPIO_PULL_NONE);
		}
	}

	if (gpio_fpga2_cs_n)	{
		err = gpio_request(gpio_fpga2_cs_n, "SPI_CS2_1");
		if (err) {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
					"SPI_CS2_1", gpio_fpga2_cs_n, err);
		} else {
			pr_info("gpio request SPI_CS2_1 \n");
			gpio_direction_output(gpio_fpga2_cs_n, 0);
			//s3c_gpio_setpull(gpio_fpga2_cs_n, S3C_GPIO_PULL_NONE);
		}
	}

	
	err = gpio_request(mfp_to_gpio(AP_SPI_ADDR), "AP_SPI_ADDR");
	if (err) {
		pr_err("fail to request gpio %s\n", "AP_SPI_ADDR");
	} else {
		pr_info("gpio request AP_SPI_ADDR \n");
		gpio_direction_output(mfp_to_gpio(AP_SPI_ADDR), 0);
	}
#if 1
	err = gpio_request(mfp_to_gpio(HOST_WAKEUP_CP), "HOST_WAKEUP_CP");
	if (err) {
		pr_err("fail to request gpio %s\n", "HOST_WAKEUP_CP");
	} else {
		pr_info("gpio request HOST_WAKEUP_CP \n");
		gpio_direction_output(mfp_to_gpio(HOST_WAKEUP_CP), 0);
	}
#endif
#endif

	if (gpio_sim_detect) {
		err = gpio_request(gpio_sim_detect, "MSM_SIM_DETECT");
		if (err) {
			pr_err("fail to request gpio %s: %d\n",
					"MSM_SIM_DETECT", err);
		} else {
			pr_info("gpio request MSM_SIM_DETECT \n");
			/* gpio_direction_input(gpio_sim_detect); */
			//s3c_gpio_cfgpin(gpio_sim_detect, S3C_GPIO_SFN(0xF));
			//s3c_gpio_setpull(gpio_sim_detect, S3C_GPIO_PULL_NONE);
			irq_set_irq_type(gpio_sim_detect, IRQ_TYPE_EDGE_BOTH);
		}
	}
}

static u8 *msm_edpram_remap_mem_region(struct sromc_cfg *cfg)
{
	int dp_addr = 0;
	int dp_size = 0;
	u8 __iomem *dp_base = NULL;
	struct msm_edpram_ipc_cfg *ipc_map = NULL;
	struct dpram_ipc_device *dev = NULL;
#if 0
	dp_addr = cfg->addr;
	dp_size = cfg->size;
	dp_base = (u8 *) ioremap_nocache(dp_addr, dp_size);
	if (!dp_base) {
		pr_err("[MDM] <%s> dpram base ioremap fail\n", __func__);
		return NULL;
	}
	pr_info("[MDM] <%s> DPRAM VA=0x%08X\n", __func__, (int)dp_base);

	msm_edpram.base = (u8 __iomem *)dp_base;
	msm_edpram.size = dp_size;

	/* Map for IPC */
	ipc_map = (struct msm_edpram_ipc_cfg *)dp_base;
#else
	//char *pld_base;
	//pld_base = kmalloc(cfg->size, GFP_ATOMIC);

	//ipc_map = (struct msm_edpram_ipc_cfg *)pld_base;
	ipc_map = 0x0;
#endif
	/* Magic code and access enable fields */
	msm_ipc_map.magic_ap2cp = &ipc_map->magic_ap2cp;
	msm_ipc_map.access_ap2cp = &ipc_map->access_ap2cp;

	msm_ipc_map.magic_cp2ap = &ipc_map->magic_cp2ap;
	msm_ipc_map.access_cp2ap = &ipc_map->access_cp2ap;

	msm_ipc_map.address_buffer = &ipc_map->address_buffer;

	/* FMT */
	dev = &msm_ipc_map.dev[IPC_FMT];

	strcpy(dev->name, "FMT");
	dev->id = IPC_FMT;

	dev->txq.head = &ipc_map->fmt_tx_head;
	dev->txq.tail = &ipc_map->fmt_tx_tail;
	dev->txq.buff = &ipc_map->fmt_tx_buff[0];
	dev->txq.size = MSM_DP_FMT_TX_BUFF_SZ;

	dev->rxq.head = &ipc_map->fmt_rx_head;
	dev->rxq.tail = &ipc_map->fmt_rx_tail;
	dev->rxq.buff = &ipc_map->fmt_rx_buff[0];
	dev->rxq.size = MSM_DP_FMT_RX_BUFF_SZ;

	dev->mask_req_ack = INT_MASK_REQ_ACK_F;
	dev->mask_res_ack = INT_MASK_RES_ACK_F;
	dev->mask_send = INT_MASK_SEND_F;

	/* RAW */
	dev = &msm_ipc_map.dev[IPC_RAW];

	strcpy(dev->name, "RAW");
	dev->id = IPC_RAW;

	dev->txq.head = &ipc_map->raw_tx_head;
	dev->txq.tail = &ipc_map->raw_tx_tail;
	dev->txq.buff = &ipc_map->raw_tx_buff[0];
	dev->txq.size = MSM_DP_RAW_TX_BUFF_SZ;

	dev->rxq.head = &ipc_map->raw_rx_head;
	dev->rxq.tail = &ipc_map->raw_rx_tail;
	dev->rxq.buff = &ipc_map->raw_rx_buff[0];
	dev->rxq.size = MSM_DP_RAW_RX_BUFF_SZ;

	dev->mask_req_ack = INT_MASK_REQ_ACK_R;
	dev->mask_res_ack = INT_MASK_RES_ACK_R;
	dev->mask_send = INT_MASK_SEND_R;

	/* Mailboxes */
	msm_ipc_map.mbx_ap2cp = &ipc_map->mbx_ap2cp;
	msm_ipc_map.mbx_cp2ap = &ipc_map->mbx_cp2ap;

	pr_err("mbx_ap2cp = 0x%x\n", msm_ipc_map.mbx_ap2cp);
	pr_err("magic_ap2cp = 0x%x\n", msm_ipc_map.access_ap2cp);
	pr_err("mbx_cp2ap = 0x%x\n", msm_ipc_map.mbx_cp2ap);
	pr_err("access_cp2ap = 0x%x\n", msm_ipc_map.access_cp2ap);

	return 1;//dp_base;
}

#if defined(CONFIG_LINK_DEVICE_PLD)
#define PLD_BLOCK_SIZE	0x8000

static struct spi_device *p_spi;

static int pld_spi_probe(struct spi_device *spi)
{
	int ret = 0;

	pr_info("pld spi proble.\n");

	p_spi = spi;
	p_spi->mode = SPI_MODE_3;
	p_spi->bits_per_word = 8;
	p_spi->max_speed_hz = 12000000;

	ret = spi_setup(p_spi);
	if (ret != 0) {
		mif_err("spi_setup ERROR : %d\n", ret);
		return ret;
	}

	dev_info(&p_spi->dev, "(%d) spi probe Done.\n", __LINE__);

	return ret;
}

static int pld_spi_remove(struct spi_device *spi)
{
	return 0;
}

static struct spi_driver pld_spi_driver = {
	.probe = pld_spi_probe,
	.remove = __devexit_p(pld_spi_remove),
	.driver = {
		.name = "fpga_spi",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
	},
};

static int config_spi_dev_init(void)
{
	int ret = 0;

	ret = spi_register_driver(&pld_spi_driver);
	if (ret < 0) {
		pr_info("spi_register_driver() fail : %d\n", ret);
		return ret;
	}

	pr_info("[%s] Done\n", __func__);
	return 0;
}

static int spi_tx_rx_sync(u8 *tx_d, u8 *rx_d, unsigned len)
{
	struct spi_transfer t;
	struct spi_message msg;

	memset(&t, 0, sizeof t);

	t.len = len;

	t.tx_buf = tx_d;
	t.rx_buf = rx_d;

	t.cs_change = 0;

	t.bits_per_word = 8;
	t.speed_hz = 12000000;

	spi_message_init(&msg);
	spi_message_add_tail(&t, &msg);
	return spi_sync(p_spi, &msg);
}

void pld_control_cs_n(u32 value)
{
	unsigned long fpga_cs_high[] = {
		ND_IO10_ND_DAT10 | MFP_PULL_HIGH,
	};

	unsigned long fpga_cs_low[] = {
		ND_IO10_ND_DAT10 | MFP_PULL_LOW,
	};

	if (value)
		mfp_config(ARRAY_AND_SIZE(fpga_cs_high));
	else
		mfp_config(ARRAY_AND_SIZE(fpga_cs_low));
}

void pld_control_via_off(u32 value)
{
	unsigned long ps_hold_off_high[] = {
		VIA_PS_HOLD_OFF | MFP_PULL_HIGH,
	};

	unsigned long ps_hold_off_low[] = {
		VIA_PS_HOLD_OFF | MFP_PULL_LOW,
	};

	if (value)
		mfp_config(ARRAY_AND_SIZE(ps_hold_off_high));
	else
		mfp_config(ARRAY_AND_SIZE(ps_hold_off_low));
}
void power_control_via_off(u32 value)
{
	unsigned long ps_hold_off_high[] = {
		GPIO097_GPIO_97 | MFP_PULL_HIGH,
	};

	unsigned long ps_hold_off_low[] = {
		GPIO097_GPIO_97 | MFP_PULL_LOW,
	};

	if (value)
		mfp_config(ARRAY_AND_SIZE(ps_hold_off_high));
	else
		mfp_config(ARRAY_AND_SIZE(ps_hold_off_low));
}

void pld_control_via_reset(u32 value)
{
	unsigned long via_reset_high[] = {
		VIA_RST | MFP_PULL_HIGH,
	};

	unsigned long via_reset_low[] = {
		VIA_RST | MFP_PULL_LOW,
	};

	if (value)
		mfp_config(ARRAY_AND_SIZE(via_reset_high));
	else
		mfp_config(ARRAY_AND_SIZE(via_reset_low));
}

#define MAX_WRITE_SIZE 32;
static int pld_send_fgpa_bin(void)
{
	int retval = 0;
	int i = 0;
	char *tx_b, *rx_b;
	unsigned int sent_len = 0;
	unsigned int rest_len = 0;
	unsigned int packet_size = 0;

#if defined(CONFIG_LINK_DEVICE_PLD)
	unsigned gpio_fpga2_creset = cdma_modem_data.gpio_fpga2_creset;
	unsigned gpio_fpga2_cdone = cdma_modem_data.gpio_fpga2_cdone;
	unsigned gpio_fpga2_rst_n = cdma_modem_data.gpio_fpga2_rst_n;
	unsigned gpio_fpga2_cs_n = cdma_modem_data.gpio_fpga2_cs_n;
#endif
	char dummy_data[8] = "abcdefg";

	mif_info("size of pld : %d \n", sizeof(fpga_bin));

	pld_control_cs_n(0);
	gpio_set_value(gpio_fpga2_creset, 0);
	mif_info("gpio_fpga2_cs_n[%d], gpio_fpga2_creset[%d]\n",
		gpio_get_value(gpio_fpga2_cs_n), gpio_get_value(gpio_fpga2_creset));
	msleep(20);

	gpio_set_value(gpio_fpga2_creset, 1);
	mif_info("gpio_fpga2_creset[%d]\n",
		gpio_get_value(gpio_fpga2_creset));
	msleep(20);
	mif_info(" PLD_CDone[%d]\n",
		gpio_get_value(gpio_fpga2_cdone));

	tx_b = kmalloc(PLD_BLOCK_SIZE*2, GFP_ATOMIC);
	if (!tx_b) {
		mif_err("(%d) tx_b kmalloc fail.",
			__LINE__);
		return -ENOMEM;
	}
	memset(tx_b, 0, PLD_BLOCK_SIZE*2);
	memcpy(tx_b, fpga_bin, sizeof(fpga_bin));

	rx_b = kmalloc(PLD_BLOCK_SIZE*2, GFP_ATOMIC);
	if (!rx_b) {
		mif_err("(%d) rx_b kmalloc fail.",
			__LINE__);
		retval = -ENOMEM;
		goto err;
	}
	memset(rx_b, 0, PLD_BLOCK_SIZE*2);

	mif_info("%s sync bin\n", __func__);
#if 1
	packet_size = MAX_WRITE_SIZE;
	sent_len = 0;
	rest_len = sizeof(fpga_bin);
	while (sent_len < sizeof(fpga_bin)) {
		if (rest_len < packet_size)
			packet_size = rest_len;
		retval = spi_tx_rx_sync(tx_b + sent_len, rx_b, packet_size);
		if (retval < 0) {
			mif_err("i = %d\n", i);
			goto err;
		}
		sent_len += packet_size;
		rest_len  -= packet_size;
		i++;
	}
#else
	retval = spi_tx_rx_sync(tx_b, rx_b, sizeof(fpga_bin));
	if (retval != 0) {
		mif_err("(%d) spi sync error : %d\n",
			__LINE__, retval);
		goto err;
	}

#endif
	memset(tx_b, 0, PLD_BLOCK_SIZE*2);
	memcpy(tx_b, dummy_data, sizeof(dummy_data));

	mif_info("%s sync dummy\n", __func__);
	
	retval = spi_tx_rx_sync(tx_b, rx_b, sizeof(dummy_data));
	if (retval != 0) {
		mif_err("(%d) spi sync error : %d\n",
			__LINE__, retval);
		goto err;
	}

	msleep(20);
	mif_info(" PLD_CDone[%d]\n",
		gpio_get_value(gpio_fpga2_cdone));

	gpio_set_value(gpio_fpga2_rst_n, 1);
	pld_control_cs_n(1);
	mif_info("FPGA_RST_N : %d\n", gpio_get_value(gpio_fpga2_rst_n));
err:
	kfree(tx_b);
	kfree(rx_b);

	return retval;

}
#endif

static void data_swap(u8 *data, u32 data_len)
{
	int i = 0;
	u8 tmp = 0;

	for(i=0; i<data_len; i=i+2){
		tmp = *((u8 *)data+i); 
		*((u8 *)data+i) = *((u8 *)data+i+1);
		*((u8 *)data+i+1) = tmp;
		}
}

u32 SPI_IPC_Txd(u16 addr, u8 *data, u32 data_len)
{
	u32 retval;
	u8 *tx_b;
	int i = 0;
	int printLen;
	unsigned long int flags;

	addr = PLD_ADDR_MASK(addr);
	addr <<= 1;
	addr |= IPC_RW_BIT;

	/*send addr*/
	gpio_set_value(mfp_to_gpio(AP_SPI_ADDR), 1);
	retval=spi_write(p_spi, (u8 *)&addr, 2);
	if (retval != 0) {
		mif_err("(%d) spi sync error : %d\n",
			__LINE__, retval);
		goto err;
	}
	gpio_set_value(mfp_to_gpio(AP_SPI_ADDR), 0);
	retval=spi_write(p_spi, data, data_len);
	if (retval != 0) {
		mif_err("(%d) spi sync error : %d\n",
			__LINE__, retval);
		goto err;
	}
err:
	return retval;
}

u32 SPI_IPC_Rxd(u16 addr, u8 *data, u32 data_len)
{
	u32 retval;
	u8 *tx_b;
	int i=0;

	addr = PLD_ADDR_MASK(addr);
	addr <<= 1;
	addr &= ~IPC_RW_BIT;

	/*send addr*/
	gpio_set_value(mfp_to_gpio(AP_SPI_ADDR), 1);
	retval = spi_write(p_spi, (u8 *)&addr, 2);
	if (retval != 0) {
		mif_err("(%d) spi sync error : %d\n",
			__LINE__, retval);
		goto err;
	}

	gpio_set_value(mfp_to_gpio(AP_SPI_ADDR), 0);
	retval = spi_read(p_spi, data, data_len);
	if (retval != 0) {
		mif_err("(%d) spi sync error : %d\n",
			__LINE__, retval);
		goto err;
	}
err:
	return retval;
}

int _pld_write_register(u16 addr, u8 *data, u32 len)
{
	int err = 0;
	int i = 0;
	data_swap(data,len);
	err = (int)SPI_IPC_Txd(addr, data, len);
	return err;
	
}

int _pld_read_register(u16 addr, u8* data, u32 len)
{
	int err = 0;
	err = SPI_IPC_Rxd(addr, data, len);
	data_swap(data,len);
	return err;
}

#ifdef DELAYED_WORK
void pld_loopback_test(struct work_struct *work)
#else
void pld_loopback_test(void)
#endif
{
	u16 rx_data;
	u16 addr = 0x0002;
	u16 data = 0xABCD;
	u32 err;
	

	//gpio_set_value(mfp_to_gpio(VIA_ON), 0);
	//msleep(500);
	//gpio_set_value(mfp_to_gpio(VIA_ON), 1);

//	_pld_write_register(addr, data,2);
//	mif_info("[PLD_WRITE] addr = 0x%04x, data = 0x%04x\n", addr, data);
	msleep(100);
//	rx_data = _pld_read_register(addr);
//	mif_info("[PLD_READ] addr = 0x%04x, data = 0x%04x\n", addr, rx_data);
#if 0
	addr = 0x2000;
	data = 0x5678;
	mif_info("[PLD_WRITE] addr = 0x%04x, data = 0x%04x\n", addr, data);
	_pld_write_register(addr, data);
	rx_data = _pld_read_register(addr);
	mif_info("[PLD_READ] addr = 0x%04x, data = 0x%04x\n", addr, rx_data);
#endif
}

void do_loopback(void)
{
#ifdef DELAYED_WORK
	queue_delayed_work(enable_wq, &enable_work, 0);
#else
	pld_loopback_test();
#endif
}
EXPORT_SYMBOL(do_loopback);

static int __init init_modem(void)
{
	struct sromc_cfg *cfg = NULL;
	struct sromc_access_cfg *acc_cfg = NULL;

#ifdef DELAYED_WORK
	enable_wq = create_singlethread_workqueue("enable_wq");
#endif

#if defined(CONFIG_MACH_T0_CHN_CTC)
	msm_edpram_cfg.csn = 1;
#else
	msm_edpram_cfg.csn = 0;
#endif
	msm_edpram_cfg.addr = SROM_CS0_BASE + (SROM_WIDTH * msm_edpram_cfg.csn);
	msm_edpram_cfg.end = msm_edpram_cfg.addr + msm_edpram_cfg.size - 1;

	mfp_config(ARRAY_AND_SIZE(baffinve_pin_config));
	//config_dpram_port_gpio();
	config_cdma_modem_gpio();

#if defined(CONFIG_LINK_DEVICE_PLD)
	config_spi_dev_init();
	pld_send_fgpa_bin();
	mfp_config(ARRAY_AND_SIZE(baffinve_spi_pin_config));
#endif

	//init_sromc();
	//cfg = &msm_edpram_cfg;
	//acc_cfg = &msm_edpram_access_cfg[DPRAM_SPEED_LOW];
	//setup_sromc(cfg->csn, cfg, acc_cfg);

	if (!msm_edpram_remap_mem_region(&msm_edpram_cfg))
		return -1;
	platform_device_register(&cdma_modem);
	//loopback
	//queue_delayed_work(enable_wq, &enable_work, 1000);
	return 0;
}

late_initcall(init_modem);
/*device_initcall(init_modem);*/
