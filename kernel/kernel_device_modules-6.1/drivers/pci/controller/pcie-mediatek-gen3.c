// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek PCIe host controller driver.
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Jianjun Wang <jianjun.wang@mediatek.com>
 */

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/iopoll.h>
#include <linux/irq.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/msi.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pci.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/spinlock.h>
#if IS_ENABLED(CONFIG_ANDROID_FIX_PCIE_SLAVE_ERROR)
#include <trace/hooks/traps.h>
#endif

#include "../pci.h"
#include "../../misc/mediatek/clkbuf/src/clkbuf-ctrl.h"

u32 mtk_pcie_dump_link_info(int port);

/* pextp register, CG,HW mode */
#define PCIE_PEXTP_CG_0			0x14
#define PEXTP_CLOCK_CON			0x20
#define CLKREQ_SAMPLE_CON		BIT(0)
#define PEXTP_PWRCTL_0			0x40
#define PCIE_HW_MTCMOS_EN_P0		BIT(0)
#define PEXTP_TIMER_SET			GENMASK(31, 8)
#define CK_DIS_TIMER_32K		0x400
#define PEXTP_PWRCTL_1			0x44
#define PCIE_HW_MTCMOS_EN_P1		BIT(0)
#define PEXTP_PWRCTL_3			0x4c
#define PEXTP_RSV_0			0x60
#define PCIE_HW_MTCMOS_EN_MD_P0		BIT(0)
#define PCIE_BBCK2_BYPASS		BIT(5)
#define PEXTP_RSV_1			0x64
#define PCIE_HW_MTCMOS_EN_MD_P1		BIT(0)
#define PCIE_MSI_SEL			0x304

#define PEXTP_SW_RST			0x4
#define PEXTP_SW_RST_SET_OFFSET		0x8
#define PEXTP_SW_RST_CLR_OFFSET		0xc
#define PEXTP_SW_RST_MAC0_BIT		BIT(0)
#define PEXTP_SW_RST_PHY0_BIT		BIT(1)
#define PEXTP_SW_MAC0_PHY0_BIT \
	(PEXTP_SW_RST_MAC0_BIT | PEXTP_SW_RST_PHY0_BIT)
#define PEXTP_SW_RST_MAC1_BIT		BIT(8)
#define PEXTP_SW_RST_PHY1_BIT		BIT(9)
#define PEXTP_SW_MAC1_PHY1_BIT \
	(PEXTP_SW_RST_MAC1_BIT | PEXTP_SW_RST_PHY1_BIT)

#define PCIE_BASE_CONF_REG              0x14
#define PCIE_SUPPORT_SPEED_MASK         GENMASK(15, 8)
#define PCIE_SUPPORT_SPEED_SHIFT        8
#define PCIE_SUPPORT_SPEED_2_5GT        BIT(8)
#define PCIE_SUPPORT_SPEED_5_0GT        BIT(9)
#define PCIE_SUPPORT_SPEED_8_0GT        BIT(10)
#define PCIE_SUPPORT_SPEED_16_0GT       BIT(11)

#define PCIE_BASIC_STATUS		0x18

#define PCIE_SETTING_REG                0x80
#define PCIE_RC_MODE                    BIT(0)
#define PCIE_GEN_SUPPORT_MASK           GENMASK(14, 12)
#define PCIE_GEN_SUPPORT_SHIFT          12
#define PCIE_GEN2_SUPPORT               BIT(12)
#define PCIE_GEN3_SUPPORT               BIT(13)
#define PCIE_GEN4_SUPPORT               BIT(14)
#define PCIE_GEN_SUPPORT(max_lspd) \
	GENMASK((max_lspd) - 2 + PCIE_GEN_SUPPORT_SHIFT, PCIE_GEN_SUPPORT_SHIFT)
#define PCIE_TARGET_SPEED_MASK          GENMASK(3, 0)

#define PCIE_CFGCTRL			0x84
#define PCIE_DISABLE_LTSSM		BIT(2)
#define PCIE_PCI_IDS_1			0x9c
#define PCI_CLASS(class)		(class << 8)

#define PCIE_CFGNUM_REG			0x140
#define PCIE_CFG_DEVFN(devfn)		((devfn) & GENMASK(7, 0))
#define PCIE_CFG_BUS(bus)		(((bus) << 8) & GENMASK(15, 8))
#define PCIE_CFG_BYTE_EN(bytes)		(((bytes) << 16) & GENMASK(19, 16))
#define PCIE_CFG_FORCE_BYTE_EN		BIT(20)
#define PCIE_CFG_OFFSET_ADDR		0x1000
#define PCIE_CFG_HEADER(bus, devfn) \
	(PCIE_CFG_BUS(bus) | PCIE_CFG_DEVFN(devfn))
#define PCIE_RC_CFG \
	(PCIE_CFG_FORCE_BYTE_EN | PCIE_CFG_BYTE_EN(0xf) | PCIE_CFG_HEADER(0, 0))

#define PCIE_RST_CTRL_REG		0x148
#define PCIE_MAC_RSTB			BIT(0)
#define PCIE_PHY_RSTB			BIT(1)
#define PCIE_BRG_RSTB			BIT(2)
#define PCIE_PE_RSTB			BIT(3)

#define PCIE_LTSSM_STATUS_REG		0x150
#define PCIE_LTSSM_STATE_MASK		GENMASK(28, 24)
#define PCIE_LTSSM_STATE(val)		((val & PCIE_LTSSM_STATE_MASK) >> 24)
#define PCIE_LTSSM_STATE_L2_IDLE	0x14

#define PCIE_LINK_STATUS_REG		0x154
#define PCIE_PORT_LINKUP		BIT(8)

#define PCIE_ASPM_CTRL			0x15c
#define PCIE_P2_EXIT_BY_CLKREQ		BIT(17)
#define PCIE_P2_IDLE_TIME_MASK		GENMASK(27, 24)
#define PCIE_P2_IDLE_TIME(x)		((x << 24) & PCIE_P2_IDLE_TIME_MASK)
#define PCIE_P2_SLEEP_TIME_MASK		GENMASK(31, 28)
#define PCIE_P2_SLEEP_TIME_4US		(0x4 << 28)

#define PCIE_MSI_SET_NUM		8
#define PCIE_MSI_IRQS_PER_SET		32
#define PCIE_MSI_IRQS_NUM \
	(PCIE_MSI_IRQS_PER_SET * PCIE_MSI_SET_NUM)

#define PCIE_DEBUG_MONITOR		0x2c
#define PCIE_DEBUG_SEL_0		0x164
#define PCIE_DEBUG_SEL_1		0x168
#define PCIE_MONITOR_DEBUG_EN		0x100
#define PCIE_DEBUG_SEL_BUS(b0, b1, b2, b3) \
	((((b0) << 24) & GENMASK(31, 24)) | (((b1) << 16) & GENMASK(23, 16)) | \
	(((b2) << 8) & GENMASK(15, 8)) | ((b3) & GENMASK(7, 0)))
#define PCIE_DEBUG_SEL_PARTITION(p0, p1, p2, p3) \
	((((p0) << 28) & GENMASK(31, 28)) | (((p1) << 24) & GENMASK(27, 24)) | \
	(((p2) << 20) & GENMASK(23, 20)) | (((p3) << 16) & GENMASK(19, 16)) | \
	PCIE_MONITOR_DEBUG_EN)

#define PCIE_INT_ENABLE_REG		0x180
#define PCIE_MSI_ENABLE			GENMASK(PCIE_MSI_SET_NUM + 8 - 1, 8)
#define PCIE_MSI_SHIFT			8
#define PCIE_INTX_SHIFT			24
#define PCIE_INTX_ENABLE \
	GENMASK(PCIE_INTX_SHIFT + PCI_NUM_INTX - 1, PCIE_INTX_SHIFT)

#define PCIE_INT_STATUS_REG		0x184
#define PCIE_AXIERR_COMPL_TIMEOUT	BIT(18)
#define PCIE_AXI_READ_ERR		GENMASK(18, 16)
#define PCIE_AER_EVT			BIT(29)
#define PCIE_ERR_RST_EVT		BIT(31)
#define PCIE_MSI_SET_ENABLE_REG		0x190
#define PCIE_MSI_SET_ENABLE		GENMASK(PCIE_MSI_SET_NUM - 1, 0)

#define PCIE_MSI_SET_BASE_REG		0xc00
#define PCIE_MSI_SET_OFFSET		0x10
#define PCIE_MSI_SET_STATUS_OFFSET	0x04
#define PCIE_MSI_SET_ENABLE_OFFSET	0x08
#define PCIE_MSI_SET_ENABLE_GRP1_OFFSET	0x0c
#define DRIVER_OWN_IRQ_STATUS		BIT(27)

#define PCIE_MSI_SET_ADDR_HI_BASE	0xc80
#define PCIE_MSI_SET_ADDR_HI_OFFSET	0x04

#define PCIE_ERR_DEBUG_LANE0		0xd40

#define PCIE_MSI_GRP2_SET_OFFSET	0xDC0
#define PCIE_MSI_GRPX_PER_SET_OFFSET	4
#define PCIE_MSI_GRP3_SET_OFFSET	0xDE0

#define PCIE_RES_STATUS                 0xd28

#define PCIE_AXI0_ERR_ADDR_L		0xe00
#define PCIE_AXI0_ERR_INFO		0xe08
#define PCIE_ERR_STS_CLEAR		BIT(0)

#define PCIE_LOW_POWER_CTRL		0x194
#define PCIE_ICMD_PM_REG		0x198
#define PCIE_TURN_OFF_LINK		BIT(4)

#define PCIE_ISTATUS_PM			0x19C
#define PCIE_L1PM_SM			GENMASK(10, 8)

#define PCIE_AXI_IF_CTRL		0x1a8
#define PCIE_AXI0_SLV_RESP_MASK		BIT(12)

#define PCIE_WCPL_TIMEOUT		0x340
#define WCPL_TIMEOUT_4MS		0x4

#define PCIE_MISC_CTRL_REG		0x348
#define PCIE_DVFS_REQ_FORCE_ON		BIT(1)
#define PCIE_MAC_SLP_DIS		BIT(7)
#define PCIE_DVFS_REQ_FORCE_OFF		BIT(12)

#define PCIE_TRANS_TABLE_BASE_REG	0x800
#define PCIE_ATR_SRC_ADDR_MSB_OFFSET	0x4
#define PCIE_ATR_TRSL_ADDR_LSB_OFFSET	0x8
#define PCIE_ATR_TRSL_ADDR_MSB_OFFSET	0xc
#define PCIE_ATR_TRSL_PARAM_OFFSET	0x10
#define PCIE_ATR_TLB_SET_OFFSET		0x20

#define PCIE_MAX_TRANS_TABLES		8
#define PCIE_ATR_EN			BIT(0)
#define PCIE_ATR_SIZE(size) \
	(((((size) - 1) << 1) & GENMASK(6, 1)) | PCIE_ATR_EN)
#define PCIE_ATR_ID(id)			((id) & GENMASK(3, 0))
#define PCIE_ATR_TYPE_MEM		PCIE_ATR_ID(0)
#define PCIE_ATR_TYPE_IO		PCIE_ATR_ID(1)
#define PCIE_ATR_TLP_TYPE(type)		(((type) << 16) & GENMASK(18, 16))
#define PCIE_ATR_TLP_TYPE_MEM		PCIE_ATR_TLP_TYPE(0)
#define PCIE_ATR_TLP_TYPE_IO		PCIE_ATR_TLP_TYPE(2)

#define PCIE_RESOURCE_CTRL		0xd2c
#define PCIE_APSRC_ACK			BIT(10)

/* pcie read completion timeout */
#define PCIE_CONF_DEV2_CTL_STS		0x10a8
#define PCIE_DCR2_CPL_TO		GENMASK(3, 0)
#define PCIE_CPL_TIMEOUT_64US		0x1
#define PCIE_CPL_TIMEOUT_4MS		0x2

#define PCIE_CONF_EXP_LNKCTL2_REG	0x10b0

/* AER status */
#define PCIE_AER_UNC_STATUS		0x1204
#define PCIE_AER_UNC_MTLP		BIT(18)
#define PCIE_AER_CO_STATUS		0x1210
#define AER_CO_RE			BIT(0)
#define AER_CO_BTLP			BIT(6)
#define PCIE_VENDOR_STS_0		0x1488
#define PCIE_VENDOR_STS_1		0x148c

/* vlpcfg register */
#define PCIE_VLPCFG_BASE		0x1C00C000
#define PCIE_VLP_AXI_PROTECT_STA	0x240
#define PCIE_MAC_SLP_READY_MASK(port)	BIT(11 - port)
#define PCIE_PHY_SLP_READY_MASK(port)	BIT(13 - port)
#define PCIE_SUM_SLP_READY(port) \
	(PCIE_MAC_SLP_READY_MASK(port) | PCIE_PHY_SLP_READY_MASK(port))
#define SRCLKEN_SPM_REQ_STA             0x1114
#define SRCLKEN_RC_REQ_STA		0x1130

#define SRCLKEN_BASE			0x1c00d000
#define M07_CFG				0x3c
#define IGNORE_SETTLE_TIME		BIT(2)
#define BBCK2_FORCE_ACK			GENMASK(4, 3)
#define CENTRAL_CFG6			0x74
#define PMRC7_VOTE_MASK			BIT(7)

#define MTK_PCIE_MAX_PORT		2
#define PCIE_CLKBUF_SUBSYS_ID		7
#define PCIE_CLKBUF_XO_ID		1
#define PMRC7_BBCK2_UNBIND		0x241
#define PMRC7_BBCK2_BIND		0x2c0
#define BBCK1_BBCK2_BIND		0x2c1

enum mtk_pcie_suspend_link_state {
	LINK_STATE_L12 = 0,
	LINK_STATE_L2,
};

struct mtk_pcie_port;

/**
 * struct mtk_pcie_data - PCIe data for each SoC
 * @pre_init: Specific init data, called before linkup
 * @suspend_l12: To implement special setting in L1.2 suspend flow
 * @resume_l12: To implement special setting in L1.2 resume flow
 * @pre_power_up: To implement special setting before power up
 * @post_power_down: To implement special setting after power down
 */
struct mtk_pcie_data {
	int (*pre_init)(struct mtk_pcie_port *port);
	int (*suspend_l12)(struct mtk_pcie_port *port);
	int (*resume_l12)(struct mtk_pcie_port *port);
	int (*pre_power_up)(struct mtk_pcie_port *port);
	void (*post_power_down)(struct mtk_pcie_port *port);
};

/**
 * struct mtk_msi_set - MSI information for each set
 * @base: IO mapped register base
 * @msg_addr: MSI message address
 * @saved_irq_state: IRQ enable state saved at suspend time
 */
struct mtk_msi_set {
	void __iomem *base;
	phys_addr_t msg_addr;
	u32 saved_irq_state;
};

/**
 * struct mtk_pcie_port - PCIe port information
 * @dev: pointer to PCIe device
 * @base: IO mapped register base
 * @pextpcfg: pextpcfg_ao(pcie HW MTCMOS) IO mapped register base
 * @vlpcfg_base: vlpcfg(bus protect ready) IO mapped register base
 * @reg_base: physical register base
 * @mac_reset: MAC reset control
 * @phy_reset: PHY reset control
 * @phy: PHY controller block
 * @clks: PCIe clocks
 * @num_clks: PCIe clocks count for this port
 * @data: special init data of each SoC
 * @port_num: serial number of pcie port
 * @suspend_mode: pcie enter low poer mode when the system enter suspend
 * @dvfs_req_en: pcie wait request to reply ack when pcie exit from P2 state
 * @peri_reset_en: clear peri pcie reset to open pcie phy & mac
 * @irq: PCIe controller interrupt number
 * @saved_irq_state: IRQ enable state saved at suspend time
 * @irq_lock: lock protecting IRQ register access
 * @intx_domain: legacy INTx IRQ domain
 * @msi_domain: MSI IRQ domain
 * @msi_bottom_domain: MSI IRQ bottom domain
 * @msi_sets: MSI sets information
 * @lock: lock protecting IRQ bit map
 * @vote_lock: lock protecting vote HW control mode
 * @ep_hw_mode_en: flag of ep control hw mode
 * @rc_hw_mode_en: flag of rc control hw mode
 * @msi_irq_in_use: bit map for assigned MSI IRQ
 */
struct mtk_pcie_port {
	struct device *dev;
	void __iomem *base;
	void __iomem *pextpcfg;
	void __iomem *vlpcfg_base;
	phys_addr_t reg_base;
	struct reset_control *mac_reset;
	struct reset_control *phy_reset;
	struct phy *phy;
	struct clk_bulk_data *clks;
	int num_clks;
	int max_link_speed;

	struct mtk_pcie_data *data;
	int port_num;
	u32 suspend_mode;
	bool dvfs_req_en;
	bool peri_reset_en;
	bool soft_off;
	int irq;
	u32 saved_irq_state;
	raw_spinlock_t irq_lock;
	struct irq_domain *intx_domain;
	struct irq_domain *msi_domain;
	struct irq_domain *msi_bottom_domain;
	struct mtk_msi_set msi_sets[PCIE_MSI_SET_NUM];
	struct mutex lock;
	spinlock_t vote_lock;
	bool ep_hw_mode_en;
	bool rc_hw_mode_en;
	DECLARE_BITMAP(msi_irq_in_use, PCIE_MSI_IRQS_NUM);
};

static struct platform_device *pdev_list[MTK_PCIE_MAX_PORT];

/**
 * mtk_pcie_config_tlp_header() - Configure a configuration TLP header
 * @bus: PCI bus to query
 * @devfn: device/function number
 * @where: offset in config space
 * @size: data size in TLP header
 *
 * Set byte enable field and device information in configuration TLP header.
 */
static void mtk_pcie_config_tlp_header(struct pci_bus *bus, unsigned int devfn,
					int where, int size)
{
	struct mtk_pcie_port *port = bus->sysdata;
	int bytes;
	u32 val;

	bytes = (GENMASK(size - 1, 0) & 0xf) << (where & 0x3);

	val = PCIE_CFG_FORCE_BYTE_EN | PCIE_CFG_BYTE_EN(bytes) |
	      PCIE_CFG_HEADER(bus->number, devfn);

	writel_relaxed(val, port->base + PCIE_CFGNUM_REG);
}

static void __iomem *mtk_pcie_map_bus(struct pci_bus *bus, unsigned int devfn,
				      int where)
{
	struct mtk_pcie_port *port = bus->sysdata;

	return port->base + PCIE_CFG_OFFSET_ADDR + where;
}

static int mtk_pcie_config_read(struct pci_bus *bus, unsigned int devfn,
				int where, int size, u32 *val)
{
	mtk_pcie_config_tlp_header(bus, devfn, where, size);

	return pci_generic_config_read32(bus, devfn, where, size, val);
}

static int mtk_pcie_config_write(struct pci_bus *bus, unsigned int devfn,
				 int where, int size, u32 val)
{
	mtk_pcie_config_tlp_header(bus, devfn, where, size);

	if (size <= 2)
		val <<= (where & 0x3) * 8;

	return pci_generic_config_write32(bus, devfn, where, 4, val);
}

static struct pci_ops mtk_pcie_ops = {
	.map_bus = mtk_pcie_map_bus,
	.read  = mtk_pcie_config_read,
	.write = mtk_pcie_config_write,
};

static int mtk_pcie_set_trans_table(struct mtk_pcie_port *port,
				    resource_size_t cpu_addr,
				    resource_size_t pci_addr,
				    resource_size_t size,
				    unsigned long type, int num)
{
	void __iomem *table;
	u32 val;

	if (num >= PCIE_MAX_TRANS_TABLES) {
		dev_err(port->dev, "not enough translate table for addr: %#llx, limited to [%d]\n",
			(unsigned long long)cpu_addr, PCIE_MAX_TRANS_TABLES);
		return -ENODEV;
	}

	table = port->base + PCIE_TRANS_TABLE_BASE_REG +
		num * PCIE_ATR_TLB_SET_OFFSET;

	writel_relaxed(lower_32_bits(cpu_addr) | PCIE_ATR_SIZE(fls(size) - 1),
		       table);
	writel_relaxed(upper_32_bits(cpu_addr),
		       table + PCIE_ATR_SRC_ADDR_MSB_OFFSET);
	writel_relaxed(lower_32_bits(pci_addr),
		       table + PCIE_ATR_TRSL_ADDR_LSB_OFFSET);
	writel_relaxed(upper_32_bits(pci_addr),
		       table + PCIE_ATR_TRSL_ADDR_MSB_OFFSET);

	if (type == IORESOURCE_IO)
		val = PCIE_ATR_TYPE_IO | PCIE_ATR_TLP_TYPE_IO;
	else
		val = PCIE_ATR_TYPE_MEM | PCIE_ATR_TLP_TYPE_MEM;

	writel_relaxed(val, table + PCIE_ATR_TRSL_PARAM_OFFSET);

	return 0;
}

static void mtk_pcie_enable_msi(struct mtk_pcie_port *port)
{
	int i;
	u32 val;

	for (i = 0; i < PCIE_MSI_SET_NUM; i++) {
		struct mtk_msi_set *msi_set = &port->msi_sets[i];

		msi_set->base = port->base + PCIE_MSI_SET_BASE_REG +
				i * PCIE_MSI_SET_OFFSET;
		msi_set->msg_addr = port->reg_base + PCIE_MSI_SET_BASE_REG +
				    i * PCIE_MSI_SET_OFFSET;

		/* Configure the MSI capture address */
		writel_relaxed(lower_32_bits(msi_set->msg_addr), msi_set->base);
		writel_relaxed(upper_32_bits(msi_set->msg_addr),
			       port->base + PCIE_MSI_SET_ADDR_HI_BASE +
			       i * PCIE_MSI_SET_ADDR_HI_OFFSET);
	}

	val = readl_relaxed(port->base + PCIE_MSI_SET_ENABLE_REG);
	val |= PCIE_MSI_SET_ENABLE;
	writel_relaxed(val, port->base + PCIE_MSI_SET_ENABLE_REG);

	val = readl_relaxed(port->base + PCIE_INT_ENABLE_REG);
	val |= PCIE_MSI_ENABLE;
	writel_relaxed(val, port->base + PCIE_INT_ENABLE_REG);
}

/*
 * mtk_pcie_clkbuf_control() - Switch BBCK2 to SW mode or HW mode
 * @dev: the request device
 * @enable: true is SW mode, false is HW mode
 *
 * SW mode will always on BBCK2, HW mode will be controlled by HW
 */
static void mtk_pcie_clkbuf_control(struct device *dev, bool enable)
{
	static int count;
	int err = 0;

	if (!dev)
		return;

	if (enable) {
		if (++count > 1) {
			dev_info(dev, "PCIe BBCK2 already enabled, count = %d\n", count);
			return;
		}
	} else {
		if (count == 0) {
			dev_info(dev, "PCIe BBCK2 already disabled\n");
			return;
		}

		if (--count) {
			dev_info(dev, "PCIe BBCK2 has user, count = %d\n", count);
			return;
		}
	}

	dev_info(dev, "Current PCIe BBCK2 count = %d\n", count);
	err = clkbuf_srclken_ctrl(enable ? "RC_FPM_REQ" : "RC_NONE_REQ",
				  PCIE_CLKBUF_SUBSYS_ID);
	if (err)
		dev_info(dev, "PCIe fail to request BBCK2\n");
}

static int mtk_pcie_set_link_speed(struct mtk_pcie_port *port)
{
	u32 val;

	if ((port->max_link_speed < 1) || (port->port_num < 0))
		return -EINVAL;

	val = readl_relaxed(port->base + PCIE_BASE_CONF_REG);
	val = (val & PCIE_SUPPORT_SPEED_MASK) >> PCIE_SUPPORT_SPEED_SHIFT;
	if (val & BIT(port->max_link_speed - 1)) {
		val = readl_relaxed(port->base + PCIE_SETTING_REG);
		val &= ~PCIE_GEN_SUPPORT_MASK;

		if (port->max_link_speed > 1)
			val |= PCIE_GEN_SUPPORT(port->max_link_speed);

		writel_relaxed(val, port->base + PCIE_SETTING_REG);

		/* Set target speed */
		val = readl_relaxed(port->base + PCIE_CONF_EXP_LNKCTL2_REG);
		val &= ~PCIE_TARGET_SPEED_MASK;
		writel(val | port->max_link_speed,
		       port->base + PCIE_CONF_EXP_LNKCTL2_REG);

		return 0;
	}

	return -EINVAL;
}

static int mtk_pcie_startup_port(struct mtk_pcie_port *port)
{
	struct resource_entry *entry;
	struct pci_host_bridge *host = pci_host_bridge_from_priv(port);
	unsigned int table_index = 0;
	int err;
	u32 val;

	/* Set as RC mode */
	val = readl_relaxed(port->base + PCIE_SETTING_REG);
	val |= PCIE_RC_MODE;
	writel_relaxed(val, port->base + PCIE_SETTING_REG);

	/* Set class code */
	val = readl_relaxed(port->base + PCIE_PCI_IDS_1);
	val &= ~GENMASK(31, 8);
	val |= PCI_CLASS(PCI_CLASS_BRIDGE_PCI << 8);
	writel_relaxed(val, port->base + PCIE_PCI_IDS_1);

	if (port->pextpcfg) {
		spin_lock_init(&port->vote_lock);
		port->vlpcfg_base = devm_ioremap(port->dev, PCIE_VLPCFG_BASE, 0x2000);

		/* Just port0 enter L12 when suspend */
		if (port->port_num == 0) {
			port->ep_hw_mode_en = false;
			port->rc_hw_mode_en = false;
		}

		if (port->port_num == 1) {
			/* Choose one of port0 and port1 to dispatch msi to ADSP */
			val = readl_relaxed(port->pextpcfg + PCIE_MSI_SEL);
			val |= BIT(0);
			writel_relaxed(val, port->pextpcfg + PCIE_MSI_SEL);
			dev_info(port->dev, "PCIE MSI select=%#x\n",
				readl_relaxed(port->pextpcfg + PCIE_MSI_SEL));
		}
	}

	if (port->data && port->data->pre_init) {
		err = port->data->pre_init(port);
		if (err)
			return err;
	}

	/* Mask all INTx interrupts */
	val = readl_relaxed(port->base + PCIE_INT_ENABLE_REG);
	val &= ~PCIE_INTX_ENABLE;
	writel_relaxed(val, port->base + PCIE_INT_ENABLE_REG);

	/* DVFSRC voltage request state */
	val = readl_relaxed(port->base + PCIE_MISC_CTRL_REG);
	val |= PCIE_DVFS_REQ_FORCE_ON;
	if (!port->dvfs_req_en) {
		val &= ~PCIE_DVFS_REQ_FORCE_ON;
		val |= PCIE_DVFS_REQ_FORCE_OFF;
	}
	writel_relaxed(val, port->base + PCIE_MISC_CTRL_REG);

	/* Set max link speed */
	err = mtk_pcie_set_link_speed(port);
	if (err)
		dev_info(port->dev, "unsupported speed: GEN%d\n",
			 port->max_link_speed);

	dev_info(port->dev, "PCIe link start ...\n");

	/* Assert all reset signals */
	val = readl_relaxed(port->base + PCIE_RST_CTRL_REG);
	val |= PCIE_MAC_RSTB | PCIE_PHY_RSTB | PCIE_BRG_RSTB | PCIE_PE_RSTB;
	writel_relaxed(val, port->base + PCIE_RST_CTRL_REG);

	/*
	 * Described in PCIe CEM specification setctions 2.2 (PERST# Signal)
	 * and 2.2.1 (Initial Power-Up (G3 to S0)).
	 * The deassertion of PERST# should be delayed 100ms (TPVPERL)
	 * for the power and clock to become stable.
	 */
	msleep(50);

	/* De-assert reset signals */
	val &= ~(PCIE_MAC_RSTB | PCIE_PHY_RSTB | PCIE_BRG_RSTB);
	writel_relaxed(val, port->base + PCIE_RST_CTRL_REG);

	msleep(50);

	/* De-assert PERST# */
	val &= ~PCIE_PE_RSTB;
	writel_relaxed(val, port->base + PCIE_RST_CTRL_REG);

	/* Check if the link is up or not */
	if (!port->soft_off) {
		err = readl_poll_timeout(port->base + PCIE_LINK_STATUS_REG, val,
					 !!(val & PCIE_PORT_LINKUP), 20,
					 PCI_PM_D3COLD_WAIT * USEC_PER_MSEC);
		if (err) {
			val = readl_relaxed(port->base + PCIE_LTSSM_STATUS_REG);
			dev_info(port->dev, "PCIe link down, ltssm reg val: %#x\n", val);
			return err;
		}
	}

	dev_info(port->dev, "PCIe linkup success ...\n");

	mtk_pcie_enable_msi(port);

	if (port->pextpcfg) {
		/* PCIe port0 read completion timeout is adjusted to 4ms */
		val = PCIE_CFG_FORCE_BYTE_EN | PCIE_CFG_BYTE_EN(0xf) |
		      PCIE_CFG_HEADER(0, 0);
		writel_relaxed(val, port->base + PCIE_CFGNUM_REG);
		val = readl_relaxed(port->base + PCIE_CONF_DEV2_CTL_STS);
		val &= ~PCIE_DCR2_CPL_TO;
		val |= PCIE_CPL_TIMEOUT_4MS;
		writel_relaxed(val, port->base + PCIE_CONF_DEV2_CTL_STS);
		dev_info(port->dev, "PCIe RC control 2 register=%#x",
			readl_relaxed(port->base + PCIE_CONF_DEV2_CTL_STS));
	}

	/* Set PCIe translation windows */
	resource_list_for_each_entry(entry, &host->windows) {
		struct resource *res = entry->res;
		unsigned long type = resource_type(res);
		resource_size_t cpu_addr;
		resource_size_t pci_addr;
		resource_size_t size;
		const char *range_type;

		if (type == IORESOURCE_IO) {
			cpu_addr = pci_pio_to_address(res->start);
			range_type = "IO";
		} else if (type == IORESOURCE_MEM) {
			cpu_addr = res->start;
			range_type = "MEM";
		} else {
			continue;
		}

		pci_addr = res->start - entry->offset;
		size = resource_size(res);
		err = mtk_pcie_set_trans_table(port, cpu_addr, pci_addr, size,
					       type, table_index);
		if (err)
			return err;

		dev_dbg(port->dev, "set %s trans window[%d]: cpu_addr = %#llx, pci_addr = %#llx, size = %#llx\n",
			range_type, table_index, (unsigned long long)cpu_addr,
			(unsigned long long)pci_addr, (unsigned long long)size);

		table_index++;
	}

	return 0;
}

static int mtk_pcie_set_affinity(struct irq_data *data,
				 const struct cpumask *mask, bool force)
{
	struct mtk_pcie_port *port = data->domain->host_data;
	struct irq_data *port_data = irq_get_irq_data(port->irq);
	struct irq_chip *port_chip = irq_data_get_irq_chip(port_data);
	int ret;

	if (!port_chip || !port_chip->irq_set_affinity)
		return -EINVAL;

	ret = port_chip->irq_set_affinity(port_data, mask, force);

	irq_data_update_effective_affinity(data, mask);

	return ret;
}

static void mtk_pcie_msi_irq_mask(struct irq_data *data)
{
	pci_msi_mask_irq(data);
	irq_chip_mask_parent(data);
}

static void mtk_pcie_msi_irq_unmask(struct irq_data *data)
{
	pci_msi_unmask_irq(data);
	irq_chip_unmask_parent(data);
}

static struct irq_chip mtk_msi_irq_chip = {
	.irq_ack = irq_chip_ack_parent,
	.irq_mask = mtk_pcie_msi_irq_mask,
	.irq_unmask = mtk_pcie_msi_irq_unmask,
	.name = "MSI",
};

static struct msi_domain_info mtk_msi_domain_info = {
	.flags	= (MSI_FLAG_USE_DEF_DOM_OPS | MSI_FLAG_USE_DEF_CHIP_OPS |
		   MSI_FLAG_PCI_MSIX | MSI_FLAG_MULTI_PCI_MSI),
	.chip	= &mtk_msi_irq_chip,
};

static void mtk_compose_msi_msg(struct irq_data *data, struct msi_msg *msg)
{
	struct mtk_msi_set *msi_set = irq_data_get_irq_chip_data(data);
	struct mtk_pcie_port *port = data->domain->host_data;
	unsigned long hwirq;

	hwirq =	data->hwirq % PCIE_MSI_IRQS_PER_SET;

	msg->address_hi = upper_32_bits(msi_set->msg_addr);
	msg->address_lo = lower_32_bits(msi_set->msg_addr);
	msg->data = hwirq;
	dev_dbg(port->dev, "msi#%#lx address_hi %#x address_lo %#x data %d\n",
		hwirq, msg->address_hi, msg->address_lo, msg->data);
}

static void mtk_msi_bottom_irq_ack(struct irq_data *data)
{
	struct mtk_msi_set *msi_set = irq_data_get_irq_chip_data(data);
	unsigned long hwirq;

	hwirq =	data->hwirq % PCIE_MSI_IRQS_PER_SET;

	writel_relaxed(BIT(hwirq), msi_set->base + PCIE_MSI_SET_STATUS_OFFSET);
}

static void mtk_msi_bottom_irq_mask(struct irq_data *data)
{
	struct mtk_msi_set *msi_set = irq_data_get_irq_chip_data(data);
	struct mtk_pcie_port *port = data->domain->host_data;
	unsigned long hwirq, flags;
	u32 val;

	hwirq =	data->hwirq % PCIE_MSI_IRQS_PER_SET;

	raw_spin_lock_irqsave(&port->irq_lock, flags);
	val = readl_relaxed(msi_set->base + PCIE_MSI_SET_ENABLE_OFFSET);
	val &= ~BIT(hwirq);
	writel_relaxed(val, msi_set->base + PCIE_MSI_SET_ENABLE_OFFSET);
	raw_spin_unlock_irqrestore(&port->irq_lock, flags);
}

static void mtk_msi_bottom_irq_unmask(struct irq_data *data)
{
	struct mtk_msi_set *msi_set = irq_data_get_irq_chip_data(data);
	struct mtk_pcie_port *port = data->domain->host_data;
	unsigned long hwirq, flags;
	u32 val;

	hwirq =	data->hwirq % PCIE_MSI_IRQS_PER_SET;

	raw_spin_lock_irqsave(&port->irq_lock, flags);
	val = readl_relaxed(msi_set->base + PCIE_MSI_SET_ENABLE_OFFSET);
	val |= BIT(hwirq);
	writel_relaxed(val, msi_set->base + PCIE_MSI_SET_ENABLE_OFFSET);
	raw_spin_unlock_irqrestore(&port->irq_lock, flags);
}

static struct irq_chip mtk_msi_bottom_irq_chip = {
	.irq_ack		= mtk_msi_bottom_irq_ack,
	.irq_mask		= mtk_msi_bottom_irq_mask,
	.irq_unmask		= mtk_msi_bottom_irq_unmask,
	.irq_compose_msi_msg	= mtk_compose_msi_msg,
	.irq_set_affinity	= mtk_pcie_set_affinity,
	.name			= "MSI",
};

static int mtk_msi_bottom_domain_alloc(struct irq_domain *domain,
				       unsigned int virq, unsigned int nr_irqs,
				       void *arg)
{
	struct mtk_pcie_port *port = domain->host_data;
	struct mtk_msi_set *msi_set;
	int i, hwirq, set_idx;

	mutex_lock(&port->lock);

	hwirq = bitmap_find_free_region(port->msi_irq_in_use, PCIE_MSI_IRQS_NUM,
					order_base_2(nr_irqs));

	mutex_unlock(&port->lock);

	if (hwirq < 0)
		return -ENOSPC;

	set_idx = hwirq / PCIE_MSI_IRQS_PER_SET;
	msi_set = &port->msi_sets[set_idx];

	for (i = 0; i < nr_irqs; i++)
		irq_domain_set_info(domain, virq + i, hwirq + i,
				    &mtk_msi_bottom_irq_chip, msi_set,
				    handle_edge_irq, NULL, NULL);

	return 0;
}

static void mtk_msi_bottom_domain_free(struct irq_domain *domain,
				       unsigned int virq, unsigned int nr_irqs)
{
	struct mtk_pcie_port *port = domain->host_data;
	struct irq_data *data = irq_domain_get_irq_data(domain, virq);

	mutex_lock(&port->lock);

	bitmap_release_region(port->msi_irq_in_use, data->hwirq,
			      order_base_2(nr_irqs));

	mutex_unlock(&port->lock);

	irq_domain_free_irqs_common(domain, virq, nr_irqs);
}

static const struct irq_domain_ops mtk_msi_bottom_domain_ops = {
	.alloc = mtk_msi_bottom_domain_alloc,
	.free = mtk_msi_bottom_domain_free,
};

static void mtk_intx_mask(struct irq_data *data)
{
	struct mtk_pcie_port *port = irq_data_get_irq_chip_data(data);
	unsigned long flags;
	u32 val;

	raw_spin_lock_irqsave(&port->irq_lock, flags);
	val = readl_relaxed(port->base + PCIE_INT_ENABLE_REG);
	val &= ~BIT(data->hwirq + PCIE_INTX_SHIFT);
	writel_relaxed(val, port->base + PCIE_INT_ENABLE_REG);
	raw_spin_unlock_irqrestore(&port->irq_lock, flags);
}

static void mtk_intx_unmask(struct irq_data *data)
{
	struct mtk_pcie_port *port = irq_data_get_irq_chip_data(data);
	unsigned long flags;
	u32 val;

	raw_spin_lock_irqsave(&port->irq_lock, flags);
	val = readl_relaxed(port->base + PCIE_INT_ENABLE_REG);
	val |= BIT(data->hwirq + PCIE_INTX_SHIFT);
	writel_relaxed(val, port->base + PCIE_INT_ENABLE_REG);
	raw_spin_unlock_irqrestore(&port->irq_lock, flags);
}

/**
 * mtk_intx_eoi() - Clear INTx IRQ status at the end of interrupt
 * @data: pointer to chip specific data
 *
 * As an emulated level IRQ, its interrupt status will remain
 * until the corresponding de-assert message is received; hence that
 * the status can only be cleared when the interrupt has been serviced.
 */
static void mtk_intx_eoi(struct irq_data *data)
{
	struct mtk_pcie_port *port = irq_data_get_irq_chip_data(data);
	unsigned long hwirq;

	hwirq = data->hwirq + PCIE_INTX_SHIFT;
	writel_relaxed(BIT(hwirq), port->base + PCIE_INT_STATUS_REG);
}

static struct irq_chip mtk_intx_irq_chip = {
	.irq_mask		= mtk_intx_mask,
	.irq_unmask		= mtk_intx_unmask,
	.irq_eoi		= mtk_intx_eoi,
	.irq_set_affinity	= mtk_pcie_set_affinity,
	.name			= "INTx",
};

static int mtk_pcie_intx_map(struct irq_domain *domain, unsigned int irq,
			     irq_hw_number_t hwirq)
{
	irq_set_chip_data(irq, domain->host_data);
	irq_set_chip_and_handler_name(irq, &mtk_intx_irq_chip,
				      handle_fasteoi_irq, "INTx");
	return 0;
}

static const struct irq_domain_ops intx_domain_ops = {
	.map = mtk_pcie_intx_map,
};

static int mtk_pcie_init_irq_domains(struct mtk_pcie_port *port)
{
	struct device *dev = port->dev;
	struct device_node *intc_node, *node = dev->of_node;
	int ret;

	raw_spin_lock_init(&port->irq_lock);

	/* Setup INTx */
	intc_node = of_get_child_by_name(node, "interrupt-controller");
	if (!intc_node) {
		dev_err(dev, "missing interrupt-controller node\n");
		return -ENODEV;
	}

	port->intx_domain = irq_domain_add_linear(intc_node, PCI_NUM_INTX,
						  &intx_domain_ops, port);
	if (!port->intx_domain) {
		dev_err(dev, "failed to create INTx IRQ domain\n");
		return -ENODEV;
	}

	/* Setup MSI */
	mutex_init(&port->lock);

	port->msi_bottom_domain = irq_domain_add_linear(node, PCIE_MSI_IRQS_NUM,
				  &mtk_msi_bottom_domain_ops, port);
	if (!port->msi_bottom_domain) {
		dev_err(dev, "failed to create MSI bottom domain\n");
		ret = -ENODEV;
		goto err_msi_bottom_domain;
	}

	port->msi_domain = pci_msi_create_irq_domain(dev->fwnode,
						     &mtk_msi_domain_info,
						     port->msi_bottom_domain);
	if (!port->msi_domain) {
		dev_err(dev, "failed to create MSI domain\n");
		ret = -ENODEV;
		goto err_msi_domain;
	}

	return 0;

err_msi_domain:
	irq_domain_remove(port->msi_bottom_domain);
err_msi_bottom_domain:
	irq_domain_remove(port->intx_domain);

	return ret;
}

static void mtk_pcie_irq_teardown(struct mtk_pcie_port *port)
{
	irq_set_chained_handler_and_data(port->irq, NULL, NULL);

	if (port->intx_domain) {
		int virq, i;

		for (i = 0; i < PCI_NUM_INTX; i++) {
			virq = irq_find_mapping(port->intx_domain, i);
			if (virq > 0)
				irq_dispose_mapping(virq);
		}
		irq_domain_remove(port->intx_domain);
	}

	if (port->msi_domain)
		irq_domain_remove(port->msi_domain);

	if (port->msi_bottom_domain)
		irq_domain_remove(port->msi_bottom_domain);

	irq_dispose_mapping(port->irq);
}

static void mtk_pcie_msi_handler(struct mtk_pcie_port *port, int set_idx)
{
	struct mtk_msi_set *msi_set = &port->msi_sets[set_idx];
	unsigned long msi_enable, msi_status;
	irq_hw_number_t bit, hwirq;

	msi_enable = readl_relaxed(msi_set->base + PCIE_MSI_SET_ENABLE_OFFSET);

	do {
		msi_status = readl_relaxed(msi_set->base +
					   PCIE_MSI_SET_STATUS_OFFSET);
		msi_status &= msi_enable;
		if (!msi_status)
			break;

		for_each_set_bit(bit, &msi_status, PCIE_MSI_IRQS_PER_SET) {
			hwirq = bit + set_idx * PCIE_MSI_IRQS_PER_SET;
			generic_handle_domain_irq(port->msi_bottom_domain, hwirq);
		}
	} while (true);
}

static void mtk_pcie_irq_handler(struct irq_desc *desc)
{
	struct mtk_pcie_port *port = irq_desc_get_handler_data(desc);
	struct irq_chip *irqchip = irq_desc_get_chip(desc);
	unsigned long status;
	unsigned int val;
	irq_hw_number_t irq_bit = PCIE_INTX_SHIFT;

	chained_irq_enter(irqchip, desc);

	status = readl_relaxed(port->base + PCIE_INT_STATUS_REG);
	if (status & PCIE_AER_EVT) {
		writel_relaxed(PCIE_RC_CFG, port->base + PCIE_CFGNUM_REG);
		val = readl_relaxed(port->base  + PCIE_AER_CO_STATUS);
		if (val & AER_CO_RE)
			mtk_pcie_dump_link_info(port->port_num);
	}

	for_each_set_bit_from(irq_bit, &status, PCI_NUM_INTX +
			      PCIE_INTX_SHIFT)
		generic_handle_domain_irq(port->intx_domain,
					  irq_bit - PCIE_INTX_SHIFT);

	irq_bit = PCIE_MSI_SHIFT;
	for_each_set_bit_from(irq_bit, &status, PCIE_MSI_SET_NUM +
			      PCIE_MSI_SHIFT) {
		mtk_pcie_msi_handler(port, irq_bit - PCIE_MSI_SHIFT);

		writel_relaxed(BIT(irq_bit), port->base + PCIE_INT_STATUS_REG);
	}

	chained_irq_exit(irqchip, desc);
}

static int mtk_pcie_setup_irq(struct mtk_pcie_port *port)
{
	struct device *dev = port->dev;
	struct platform_device *pdev = to_platform_device(dev);
	int err;

	err = mtk_pcie_init_irq_domains(port);
	if (err)
		return err;

	port->irq = platform_get_irq(pdev, 0);
	if (port->irq < 0)
		return port->irq;

	irq_set_chained_handler_and_data(port->irq, mtk_pcie_irq_handler, port);

	return 0;
}

static int mtk_pcie_parse_port(struct mtk_pcie_port *port)
{
	struct device *dev = port->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct resource *regs;
	struct device_node *pextp_node;
	int ret;

	regs = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pcie-mac");
	if (!regs)
		return -EINVAL;

	port->base = devm_ioremap_resource(dev, regs);
	if (IS_ERR(port->base)) {
		dev_err(dev, "failed to map register base\n");
		return PTR_ERR(port->base);
	}

	port->reg_base = regs->start;

	port->port_num = of_get_pci_domain_nr(dev->of_node);
	if (port->port_num < 0) {
		dev_info(dev, "failed to get domain number\n");
		return port->port_num;
	}

	if (port->port_num < MTK_PCIE_MAX_PORT)
		pdev_list[port->port_num] = pdev;

	port->dvfs_req_en = true;
	ret = of_property_read_bool(dev->of_node, "mediatek,dvfs-req-dis");
	if (ret)
		port->dvfs_req_en = false;

	port->peri_reset_en = true;
	ret = of_property_read_bool(dev->of_node, "mediatek,peri-reset-dis");
	if (ret)
		port->peri_reset_en = false;

	pextp_node = of_parse_phandle(dev->of_node, "pextpcfg", 0);
	if (pextp_node) {
		port->pextpcfg = of_iomap(pextp_node, 0);
		of_node_put(pextp_node);
		if (IS_ERR(port->pextpcfg))
			return PTR_ERR(port->pextpcfg);
	}

	port->suspend_mode = LINK_STATE_L2;
	ret = of_property_read_bool(dev->of_node, "mediatek,suspend-mode-l12");
	if (ret)
		port->suspend_mode = LINK_STATE_L12;

	port->phy_reset = devm_reset_control_get_optional_exclusive(dev, "phy");
	if (IS_ERR(port->phy_reset)) {
		ret = PTR_ERR(port->phy_reset);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to get PHY reset\n");

		return ret;
	}

	port->mac_reset = devm_reset_control_get_optional_exclusive(dev, "mac");
	if (IS_ERR(port->mac_reset)) {
		ret = PTR_ERR(port->mac_reset);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to get MAC reset\n");

		return ret;
	}

	port->phy = devm_phy_optional_get(dev, "pcie-phy");
	if (IS_ERR(port->phy)) {
		ret = PTR_ERR(port->phy);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to get PHY\n");

		return ret;
	}

	port->num_clks = devm_clk_bulk_get_all(dev, &port->clks);
	if (port->num_clks < 0) {
		dev_err(dev, "failed to get clocks\n");
		return port->num_clks;
	}

	port->max_link_speed = of_pci_get_max_link_speed(dev->of_node);
	if (port->max_link_speed > 0)
		dev_info(dev, "max speed to GEN%d\n", port->max_link_speed);

	return 0;
}

static int mtk_pcie_peri_reset(struct mtk_pcie_port *port, bool enable)
{
	struct arm_smccc_res res;
	struct device *dev = port->dev;

	arm_smccc_smc(MTK_SIP_KERNEL_PCIE_CONTROL, port->port_num, enable,
		      0, 0, 0, 0, 0, &res);

	if (res.a0)
		dev_info(dev, "Can't %s sw reset through SMC call\n",
			 enable ? "set" : "clear");

	return res.a0;
}

static int mtk_pcie_power_up(struct mtk_pcie_port *port)
{
	struct device *dev = port->dev;
	struct pinctrl *p;
	int err;

	if (port->data->pre_power_up) {
		err = port->data->pre_power_up(port);
		if (err)
			return err;
	}

	/* Clear PCIe pextp sw reset bit */
	if (port->pextpcfg && port->port_num == 0)
		writel_relaxed(PEXTP_SW_MAC0_PHY0_BIT,
			       port->pextpcfg + PEXTP_SW_RST_CLR_OFFSET);
	if (port->pextpcfg && port->port_num == 1)
		writel_relaxed(PEXTP_SW_MAC1_PHY1_BIT,
			       port->pextpcfg + PEXTP_SW_RST_CLR_OFFSET);

	/* Clear PCIe sw reset bit */
	if (port->peri_reset_en) {
		err = mtk_pcie_peri_reset(port, false);
		if (err) {
			dev_info(dev, "failed to clear PERI reset control bit\n");
			return err;
		}
	}

	/* PHY power on and enable pipe clock */
	reset_control_deassert(port->phy_reset);

	err = phy_init(port->phy);
	if (err) {
		dev_err(dev, "failed to initialize PHY\n");
		goto err_phy_init;
	}

	err = phy_power_on(port->phy);
	if (err) {
		dev_err(dev, "failed to power on PHY\n");
		goto err_phy_on;
	}

	/* MAC power on and enable transaction layer clocks */
	reset_control_deassert(port->mac_reset);

	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);

	err = clk_bulk_prepare_enable(port->num_clks, port->clks);
	if (err) {
		dev_err(dev, "failed to enable clocks\n");
		goto err_clk_init;
	}

	/*
	 * Leroy + Falcon SDES issue workaround, switch pinmux after
	 * PCIe RC MTCMOS on completed
	 */
	p = pinctrl_get_select(dev, "work");
	if (IS_ERR(p))
		dev_info(dev, "failed to get and select work state\n");
	else
		pinctrl_put(p);

	return 0;

err_clk_init:
	pm_runtime_put_sync(dev);
	pm_runtime_disable(dev);
	reset_control_assert(port->mac_reset);
	phy_power_off(port->phy);
err_phy_on:
	phy_exit(port->phy);
err_phy_init:
	reset_control_assert(port->phy_reset);

	return err;
}

static void mtk_pcie_power_down(struct mtk_pcie_port *port)
{
	clk_bulk_disable_unprepare(port->num_clks, port->clks);

	pm_runtime_put_sync(port->dev);
	pm_runtime_disable(port->dev);
	reset_control_assert(port->mac_reset);

	phy_power_off(port->phy);
	phy_exit(port->phy);
	reset_control_assert(port->phy_reset);

	/* Set PCIe sw reset bit */
	if (port->peri_reset_en)
		mtk_pcie_peri_reset(port, true);

	/* Set PCIe pextp sw reset bit */
	if (port->pextpcfg && port->port_num == 0)
		writel_relaxed(PEXTP_SW_MAC0_PHY0_BIT,
			       port->pextpcfg + PEXTP_SW_RST_SET_OFFSET);
	if (port->pextpcfg && port->port_num == 1)
		writel_relaxed(PEXTP_SW_MAC1_PHY1_BIT,
			       port->pextpcfg + PEXTP_SW_RST_SET_OFFSET);

	if (port->data->post_power_down)
		port->data->post_power_down(port);

	if (port->pextpcfg)
		iounmap(port->pextpcfg);
}

static int mtk_pcie_setup(struct mtk_pcie_port *port)
{
	int err;

	err = mtk_pcie_parse_port(port);
	if (err)
		return err;

	/* Don't touch the hardware registers before power up */
	err = mtk_pcie_power_up(port);
	if (err)
		return err;

	/* Try link up */
	err = mtk_pcie_startup_port(port);
	if (err)
		goto err_setup;

	err = mtk_pcie_setup_irq(port);
	if (err)
		goto err_setup;

	device_enable_async_suspend(port->dev);

	return 0;

err_setup:
	mtk_pcie_power_down(port);

	return err;
}

static int mtk_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_pcie_port *port;
	struct pci_host_bridge *host;
	int err;

	host = devm_pci_alloc_host_bridge(dev, sizeof(*port));
	if (!host)
		return -ENOMEM;

	port = pci_host_bridge_priv(host);

	port->dev = dev;
	port->data = (struct mtk_pcie_data *)of_device_get_match_data(dev);
	platform_set_drvdata(pdev, port);

	err = mtk_pcie_setup(port);
	if (err)
		goto err_probe;

	host->ops = &mtk_pcie_ops;
	host->sysdata = port;

	err = pci_host_probe(host);
	if (err) {
		mtk_pcie_irq_teardown(port);
		mtk_pcie_power_down(port);
		goto err_probe;
	}

	return 0;

err_probe:
	pinctrl_pm_select_sleep_state(&pdev->dev);

	return err;
}

static int mtk_pcie_remove(struct platform_device *pdev)
{
	struct mtk_pcie_port *port = platform_get_drvdata(pdev);
	struct pci_host_bridge *host = pci_host_bridge_from_priv(port);
	int err = 0;

	pci_lock_rescan_remove();
	pci_stop_root_bus(host->bus);
	pci_remove_root_bus(host->bus);
	pci_unlock_rescan_remove();

	mtk_pcie_irq_teardown(port);
	mtk_pcie_power_down(port);

	err = pinctrl_pm_select_sleep_state(&pdev->dev);
	if (err) {
		dev_info(&pdev->dev, "Failed to set PCIe pins sleep state\n");
		return err;
	}

	return 0;
}

static struct platform_device *mtk_pcie_find_pdev_by_port(int port)
{
	struct platform_device *pdev = NULL;

	if ((port >= 0) && (port < MTK_PCIE_MAX_PORT) && pdev_list[port])
		pdev = pdev_list[port];

	return pdev;
}

int mtk_pcie_probe_port(int port)
{
	struct platform_device *pdev;

	pdev = mtk_pcie_find_pdev_by_port(port);
	if (!pdev) {
		pr_info("pcie platform device not found!\n");
		return -ENODEV;
	}

	if (device_attach(&pdev->dev) < 0) {
		device_release_driver(&pdev->dev);
		pr_info("%s: pcie probe fail!\n", __func__);
		return -ENODEV;
	}

	return 0;
}
EXPORT_SYMBOL(mtk_pcie_probe_port);

int mtk_pcie_remove_port(int port)
{
	struct platform_device *pdev;

	pdev = mtk_pcie_find_pdev_by_port(port);
	if (!pdev) {
		pr_info("pcie platform device not found!\n");
		return -ENODEV;
	}

	mtk_pcie_dump_link_info(port);

	device_release_driver(&pdev->dev);

	return 0;
}
EXPORT_SYMBOL(mtk_pcie_remove_port);

/* Set partition when use PCIe MAC debug probe table */
static void mtk_pcie_mac_dbg_set_partition(struct mtk_pcie_port *port, u32 partition)
{
	writel_relaxed(partition, port->base + PCIE_DEBUG_SEL_1);
}

/* Read the PCIe MAC internal signal corresponding to the debug probe table bus */
static void mtk_pcie_mac_dbg_read_bus(struct mtk_pcie_port *port, u32 bus)
{
	writel_relaxed(bus, port->base + PCIE_DEBUG_SEL_0);
	pr_info("PCIe debug table: bus=%#x, partition=%#x, monitor=%#x\n",
		readl_relaxed(port->base + PCIE_DEBUG_SEL_0),
		readl_relaxed(port->base + PCIE_DEBUG_SEL_1),
		readl_relaxed(port->base + PCIE_DEBUG_MONITOR));
}

/* Dump PCIe MAC signal*/
static void mtk_pcie_monitor_mac(struct mtk_pcie_port *port)
{
	u32 val;

	/* Dump the debug table probe when AER event occurs */
	val = readl_relaxed(port->base + PCIE_INT_STATUS_REG);
	if (val & PCIE_AER_EVT) {
		mtk_pcie_mac_dbg_set_partition(port, PCIE_DEBUG_SEL_PARTITION(0x0, 0x0, 0x0, 0x0));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x0a, 0x0b, 0x15, 0x16));
		mtk_pcie_mac_dbg_set_partition(port, PCIE_DEBUG_SEL_PARTITION(0x1, 0x1, 0x1, 0x1));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x04, 0x0a, 0x0b, 0x15));
		mtk_pcie_mac_dbg_set_partition(port, PCIE_DEBUG_SEL_PARTITION(0x2, 0x2, 0x2, 0x2));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x04, 0x05, 0x06, 0x07));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x80, 0x81, 0x82, 0x87));
		mtk_pcie_mac_dbg_set_partition(port, PCIE_DEBUG_SEL_PARTITION(0x3, 0x3, 0x3, 0x3));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x00, 0x01, 0x07, 0x16));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x42, 0x43, 0x54, 0x55));
		mtk_pcie_mac_dbg_set_partition(port, PCIE_DEBUG_SEL_PARTITION(0x4, 0x4, 0x4, 0x4));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0xc9, 0xca, 0xcb, 0xcd));
		mtk_pcie_mac_dbg_set_partition(port, PCIE_DEBUG_SEL_PARTITION(0x5, 0x5, 0x5, 0x5));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x08, 0x09, 0x0a, 0x0b));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x0c, 0x0d, 0x0e, 0x0f));
		mtk_pcie_mac_dbg_set_partition(port, PCIE_DEBUG_SEL_PARTITION(0xc, 0xc, 0xc, 0xc));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x45, 0x47, 0x48, 0x49));
		mtk_pcie_mac_dbg_read_bus(port, PCIE_DEBUG_SEL_BUS(0x4a, 0x4b, 0x4c, 0x4d));
	}

	pr_info("Port%d, ltssm reg:%#x, link sta:%#x, power sta:%#x, LP ctrl:%#x, IP basic sta:%#x, int sta:%#x, msi set0 sta: %#x, msi set1 sta: %#x, axi err add:%#x, axi err info:%#x, spm res ack=%#x, PHY_err(d40)=%#x\n",
		port->port_num,
		readl_relaxed(port->base + PCIE_LTSSM_STATUS_REG),
		readl_relaxed(port->base + PCIE_LINK_STATUS_REG),
		readl_relaxed(port->base + PCIE_ISTATUS_PM),
		readl_relaxed(port->base + PCIE_LOW_POWER_CTRL),
		readl_relaxed(port->base + PCIE_BASIC_STATUS),
		readl_relaxed(port->base + PCIE_INT_STATUS_REG),
		readl_relaxed(port->base + PCIE_MSI_SET_BASE_REG +
			      PCIE_MSI_SET_STATUS_OFFSET),
		readl_relaxed(port->base + PCIE_MSI_SET_BASE_REG +
			      PCIE_MSI_SET_OFFSET +
			      PCIE_MSI_SET_STATUS_OFFSET),
		readl_relaxed(port->base + PCIE_AXI0_ERR_ADDR_L),
		readl_relaxed(port->base + PCIE_AXI0_ERR_INFO),
		readl_relaxed(port->base + PCIE_RES_STATUS),
		readl_relaxed(port->base + PCIE_ERR_DEBUG_LANE0));

	/* Dump RC configuration space */
	writel_relaxed(PCIE_RC_CFG, port->base + PCIE_CFGNUM_REG);
	pr_info("aer_cor(1204)=%#x, aer_unc(1210)=%#x, hw_sta(1488)=%#x, fw_sta(148c)=%#x\n",
		readl_relaxed(port->base + PCIE_AER_UNC_STATUS),
		readl_relaxed(port->base + PCIE_AER_CO_STATUS),
		readl_relaxed(port->base + PCIE_VENDOR_STS_0),
		readl_relaxed(port->base + PCIE_VENDOR_STS_1));

	pr_info("clock gate:%#x, PCIe HW MODE BIT:%#x, Modem HW MODE BIT:%#x, PEXTP_PWRCTL_3:%#x, slp ready:%#x, SPM ready:%#x, BBCK2:%#x\n",
		readl_relaxed(port->pextpcfg + PCIE_PEXTP_CG_0),
		readl_relaxed(port->pextpcfg + PEXTP_PWRCTL_0),
		readl_relaxed(port->pextpcfg + PEXTP_RSV_0),
		readl_relaxed(port->pextpcfg + PEXTP_PWRCTL_3),
		readl_relaxed(port->vlpcfg_base + PCIE_VLP_AXI_PROTECT_STA),
		readl_relaxed(port->vlpcfg_base + SRCLKEN_SPM_REQ_STA),
		readl_relaxed(port->vlpcfg_base + SRCLKEN_RC_REQ_STA));
}

static int mtk_pcie_sleep_protect_status(struct mtk_pcie_port *port)
{
	return readl_relaxed(port->vlpcfg_base + PCIE_VLP_AXI_PROTECT_STA) &
	       PCIE_SUM_SLP_READY(port->port_num);
}

static bool mtk_pcie_sleep_protect_ready(struct mtk_pcie_port *port)
{
	u32 sleep_protect;

	sleep_protect = mtk_pcie_sleep_protect_status(port);
	if (!sleep_protect) {
		/*
		 * Sleep-protect signal will be de-asserted about 2.59us and
		 * asserted again in HW MTCMOS-on flow. If we run subsequent
		 * flow immediately after seen sleep-protect ready, it may
		 * cause unexcepted errors.
		 * Add SW debounce time here to avoid that corner case and
		 * check again.
		 */
		udelay(6);

		sleep_protect = mtk_pcie_sleep_protect_status(port);
		if (!sleep_protect)
			return true;
	}

	dev_info(port->dev, "PCIe%d sleep protect not ready = %#x\n",
		 port->port_num, sleep_protect);

	return false;
}

#if IS_ENABLED(CONFIG_ANDROID_FIX_PCIE_SLAVE_ERROR)
static void pcie_android_rvh_do_serror(void *data, struct pt_regs *regs,
				       unsigned int esr, int *ret)
{
	struct mtk_pcie_port *pcie_port;
	struct platform_device *pdev;
	u32 val, i;

	for (i = 0; i < MTK_PCIE_MAX_PORT; i++) {
		pdev = mtk_pcie_find_pdev_by_port(i);
		if (!pdev)
			continue;

		pcie_port = platform_get_drvdata(pdev);
		if (!pcie_port)
			continue;

		pr_info("PCIe%d port found\n", pcie_port->port_num);

		if (!mtk_pcie_sleep_protect_ready(pcie_port))
			continue;

		/* Debug monitor pcie design internal signal */
		writel_relaxed(0x80810001, pcie_port->base + PCIE_DEBUG_SEL_0);
		writel_relaxed(0x22330100, pcie_port->base + PCIE_DEBUG_SEL_1);
		pr_info("Port%d debug recovery:%#x\n", pcie_port->port_num,
			readl_relaxed(pcie_port->base + PCIE_DEBUG_MONITOR));

		pr_info("Port%d ltssm reg: %#x, PCIe interrupt status=%#x, AXI0 ERROR address=%#x, AXI0 ERROR status=%#x\n",
			pcie_port->port_num,
			readl_relaxed(pcie_port->base + PCIE_LTSSM_STATUS_REG),
			readl_relaxed(pcie_port->base + PCIE_INT_STATUS_REG),
			readl_relaxed(pcie_port->base + PCIE_AXI0_ERR_ADDR_L),
			readl_relaxed(pcie_port->base + PCIE_AXI0_ERR_INFO));

		val = readl_relaxed(pcie_port->base + PCIE_INT_STATUS_REG);
		if (val & PCIE_AXI_READ_ERR) {
			*ret = 1;
			dump_stack();
		}

		if (pcie_port->port_num == 1)
			writel(BIT(0), pcie_port->base + PCIE_AXI0_ERR_INFO);
	}
}
#endif

/**
 * mtk_pcie_dump_link_info() - Dump PCIe RC information
 * @port: The port number which EP use
 * @ret_val: bit[4:0]: LTSSM state (PCIe MAC offset 0x150 bit[28:24])
 *           bit[5]: DL_UP state (PCIe MAC offset 0x154 bit[8])
 *           bit[6]: Completion timeout status (PCIe MAC offset 0x184 bit[18])
 *                   AXI fetch error (PCIe MAC offset 0x184 bit[17])
 *           bit[7]: RxErr
 *           bit[8]: MalfTLP
 *           bit[9]: Driver own irq status (MSI set 1 bit[27])
 *           bit[10]: DL_UP exit (SDES) event
 */
u32 mtk_pcie_dump_link_info(int port)
{
	struct platform_device *pdev;
	struct mtk_pcie_port *pcie_port;
	u32 val, ret_val = 0;

	pdev = mtk_pcie_find_pdev_by_port(port);
	if (!pdev) {
		pr_info("PCIe platform device not found!\n");
		return 0;
	}

	pcie_port = platform_get_drvdata(pdev);
	if (!pcie_port) {
		pr_info("PCIe port not found!\n");
		return 0;
	}

	/* Check the sleep protect ready */
	if (!mtk_pcie_sleep_protect_ready(pcie_port))
		return 0;

	mtk_pcie_monitor_mac(pcie_port);

	val = readl_relaxed(pcie_port->base + PCIE_LTSSM_STATUS_REG);
	ret_val |= PCIE_LTSSM_STATE(val);
	val = readl_relaxed(pcie_port->base + PCIE_LINK_STATUS_REG);
	ret_val |= (val >> 3) & BIT(5);

	/* AXI read request error: AXI fetch error and completion timeout */
	val = readl_relaxed(pcie_port->base + PCIE_INT_STATUS_REG);
	if (val & PCIE_AXI_READ_ERR)
		ret_val |= BIT(6);

	if (val & PCIE_ERR_RST_EVT)
		ret_val |= BIT(10);

	if (val & PCIE_AER_EVT) {
		pcie_port->phy->ops->calibrate(pcie_port->phy);
		writel_relaxed(PCIE_AER_EVT,
			       pcie_port->base + PCIE_INT_STATUS_REG);
	}

	/* PCIe RxErr */
	val = PCIE_CFG_FORCE_BYTE_EN | PCIE_CFG_BYTE_EN(0xf) |
	      PCIE_CFG_HEADER(0, 0);
	writel_relaxed(val, pcie_port->base + PCIE_CFGNUM_REG);
	val = readl_relaxed(pcie_port->base + PCIE_AER_CO_STATUS);
	if (val & AER_CO_RE)
		ret_val |= BIT(7);

	val = readl_relaxed(pcie_port->base + PCIE_AER_UNC_STATUS);
	if (val & PCIE_AER_UNC_MTLP)
		ret_val |= BIT(8);

	val = readl_relaxed(pcie_port->base + PCIE_MSI_SET_BASE_REG +
			    PCIE_MSI_SET_OFFSET + PCIE_MSI_SET_STATUS_OFFSET);
	if (val & DRIVER_OWN_IRQ_STATUS)
		ret_val |= BIT(9);

	return ret_val;
}
EXPORT_SYMBOL(mtk_pcie_dump_link_info);

/**
 * mtk_pcie_disable_data_trans - Block pcie
 * and do not accept any data packet transmission.
 * @port: The port number which EP use
 */
int mtk_pcie_disable_data_trans(int port)
{
	struct platform_device *pdev;
	struct mtk_pcie_port *pcie_port;
	u32 val;

	pdev = mtk_pcie_find_pdev_by_port(port);
	if (!pdev) {
		pr_info("PCIe platform device not found!\n");
		return -ENODEV;
	}

	pcie_port = platform_get_drvdata(pdev);
	if (!pcie_port) {
		pr_info("PCIe port not found!\n");
		return -ENODEV;
	}

	/* Check the sleep protect ready */
	if (!mtk_pcie_sleep_protect_ready(pcie_port))
		return -EPERM;

	val = readl_relaxed(pcie_port->base + PCIE_RST_CTRL_REG);
	val |= (PCIE_MAC_RSTB | PCIE_PHY_RSTB);
	writel_relaxed(val, pcie_port->base + PCIE_RST_CTRL_REG);

	val = readl_relaxed(pcie_port->base + PCIE_CFGCTRL);
	val |= PCIE_DISABLE_LTSSM;
	writel_relaxed(val, pcie_port->base + PCIE_CFGCTRL);

	val = readl_relaxed(pcie_port->base + PCIE_RST_CTRL_REG);
	val &= ~PCIE_MAC_RSTB;
	writel_relaxed(val, pcie_port->base + PCIE_RST_CTRL_REG);

	/*
	 * Set completion timeout to 64us to avoid corner case
	 * PCIe received a read command from AP but set MAC_RESET=1 before
	 * reply response signal to bus. After set MAC_RESET=0 again,
	 * completion timeout setting will be reset to default value(50ms).
	 * Then internal timer will keep counting until it achieve 50ms limit,
	 * and will cause bus tracker timeout.
	 * (note: bus tracker timeout = 5ms).
	 */
	val = PCIE_CFG_FORCE_BYTE_EN | PCIE_CFG_BYTE_EN(0xf) |
	      PCIE_CFG_HEADER(0, 0);
	writel_relaxed(val, pcie_port->base + PCIE_CFGNUM_REG);
	val = readl_relaxed(pcie_port->base + PCIE_CONF_DEV2_CTL_STS);
	val &= ~PCIE_DCR2_CPL_TO;
	val |= PCIE_CPL_TIMEOUT_64US;
	writel_relaxed(val, pcie_port->base + PCIE_CONF_DEV2_CTL_STS);

	pr_info("reset control signal(0x148)=%#x, IP config control(0x84)=%#x\n",
		readl_relaxed(pcie_port->base + PCIE_RST_CTRL_REG),
		readl_relaxed(pcie_port->base + PCIE_CFGCTRL));

	return 0;
}
EXPORT_SYMBOL(mtk_pcie_disable_data_trans);

/**
 * mtk_msi_unmask_to_other_mcu() - Unmask msi dispatch to other mcu
 * @data: The irq_data of virq
 * @group: MSI will dispatch to which group number
 */
int mtk_msi_unmask_to_other_mcu(struct irq_data *data, u32 group)
{
	struct irq_data *parent_data = data->parent_data;
	struct mtk_msi_set *msi_set;
	struct mtk_pcie_port *port;
	void __iomem *dest_addr;
	unsigned long hwirq;
	u32 val, set_num;

	if (!parent_data)
		return -EINVAL;

	msi_set = irq_data_get_irq_chip_data(parent_data);
	if (!msi_set)
		return -ENODEV;

	port = parent_data->domain->host_data;
	hwirq = parent_data->hwirq % PCIE_MSI_IRQS_PER_SET;
	set_num = parent_data->hwirq / PCIE_MSI_IRQS_PER_SET;

	switch (group) {
	case 1:
		dest_addr = msi_set->base + PCIE_MSI_SET_ENABLE_GRP1_OFFSET;
		break;
	case 2:
		dest_addr = port->base + PCIE_MSI_GRP2_SET_OFFSET +
			    PCIE_MSI_GRPX_PER_SET_OFFSET * set_num;
		break;
	case 3:
		dest_addr = port->base + PCIE_MSI_GRP3_SET_OFFSET +
			    PCIE_MSI_GRPX_PER_SET_OFFSET * set_num;
		break;
	default:
		pr_info("Group %d out of max range\n", group);

		return -EINVAL;
	}

	val = readl_relaxed(dest_addr);
	val |= BIT(hwirq);
	writel_relaxed(val, dest_addr);

	pr_info("group=%d, hwirq=%ld, SET num=%d, Enable status=%#x\n",
		group, hwirq, set_num, readl_relaxed(dest_addr));

	return 0;
}
EXPORT_SYMBOL(mtk_msi_unmask_to_other_mcu);

static void __maybe_unused mtk_pcie_irq_save(struct mtk_pcie_port *port)
{
	int i;
	unsigned long flags;

	raw_spin_lock_irqsave(&port->irq_lock, flags);

	port->saved_irq_state = readl_relaxed(port->base + PCIE_INT_ENABLE_REG);

	for (i = 0; i < PCIE_MSI_SET_NUM; i++) {
		struct mtk_msi_set *msi_set = &port->msi_sets[i];

		msi_set->saved_irq_state = readl_relaxed(msi_set->base +
					   PCIE_MSI_SET_ENABLE_OFFSET);
	}

	raw_spin_unlock_irqrestore(&port->irq_lock, flags);
}

static void __maybe_unused mtk_pcie_irq_restore(struct mtk_pcie_port *port)
{
	int i;
	unsigned long flags;

	raw_spin_lock_irqsave(&port->irq_lock, flags);

	writel_relaxed(port->saved_irq_state, port->base + PCIE_INT_ENABLE_REG);

	for (i = 0; i < PCIE_MSI_SET_NUM; i++) {
		struct mtk_msi_set *msi_set = &port->msi_sets[i];

		writel_relaxed(msi_set->saved_irq_state,
			       msi_set->base + PCIE_MSI_SET_ENABLE_OFFSET);
	}

	raw_spin_unlock_irqrestore(&port->irq_lock, flags);
}

static int __maybe_unused mtk_pcie_turn_off_link(struct mtk_pcie_port *port)
{
	u32 val;

	val = readl_relaxed(port->base + PCIE_ICMD_PM_REG);
	val |= PCIE_TURN_OFF_LINK;
	writel_relaxed(val, port->base + PCIE_ICMD_PM_REG);

	/* Check the link is L2 */
	return readl_poll_timeout(port->base + PCIE_LTSSM_STATUS_REG, val,
				  (PCIE_LTSSM_STATE(val) ==
				   PCIE_LTSSM_STATE_L2_IDLE), 20,
				   50 * USEC_PER_MSEC);
}

static void mtk_pcie_enable_hw_control(struct mtk_pcie_port *port, bool enable)
{
	u32 val;

	val = readl_relaxed(port->pextpcfg + PEXTP_PWRCTL_0);
	if (enable)
		val |= PCIE_HW_MTCMOS_EN_P0;
	else
		val &= ~PCIE_HW_MTCMOS_EN_P0;

	writel_relaxed(val, port->pextpcfg + PEXTP_PWRCTL_0);

	if (enable)
		dev_info(port->dev, "PCIe HW MODE BIT=%#x\n",
			 readl_relaxed(port->pextpcfg + PEXTP_PWRCTL_0));
}

/*
 * mtk_pcie_hw_control_vote() - Vote mechanism
 * @port: The port number which EP use
 * @hw_mode_en: vote mechanism, true: agree open hw mode;
 *        false: disagree open hw mode
 * @who: 0 is rc, 1 is wifi
 */
int mtk_pcie_hw_control_vote(int port, bool hw_mode_en, u8 who)
{
	struct platform_device *pdev;
	struct mtk_pcie_port *pcie_port;
	bool vote_hw_mode_en = false, last_hw_mode = false;
	unsigned long flags;
	int err = 0;
	u32 val;

	pdev = mtk_pcie_find_pdev_by_port(port);
	if (!pdev) {
		pr_info("PCIe platform device not found!\n");
		return -ENODEV;
	}

	pcie_port = platform_get_drvdata(pdev);
	if (!pcie_port)
		return -ENODEV;

	spin_lock_irqsave(&pcie_port->vote_lock, flags);

	last_hw_mode = (pcie_port->ep_hw_mode_en && pcie_port->rc_hw_mode_en)
			? true : false;
	if (who)
		pcie_port->ep_hw_mode_en = hw_mode_en;
	else
		pcie_port->rc_hw_mode_en = hw_mode_en;

	vote_hw_mode_en = (pcie_port->ep_hw_mode_en && pcie_port->rc_hw_mode_en)
			   ? true : false;
	mtk_pcie_enable_hw_control(pcie_port, vote_hw_mode_en);

	if (!vote_hw_mode_en && last_hw_mode) {
		/* Check the sleep protect ready */
		err = readl_poll_timeout_atomic(pcie_port->vlpcfg_base +
			      PCIE_VLP_AXI_PROTECT_STA, val,
			      !(val & PCIE_SUM_SLP_READY(pcie_port->port_num)),
			      10, 10 * USEC_PER_MSEC);
		if (err) {
			dev_info(pcie_port->dev, "PCIe sleep protect not ready, %#x, PCIe HW MODE BIT=%#x\n",
				 readl_relaxed(pcie_port->vlpcfg_base +
					       PCIE_VLP_AXI_PROTECT_STA),
				 readl_relaxed(pcie_port->pextpcfg +
					       PEXTP_PWRCTL_0));
		} else {
			if (!mtk_pcie_sleep_protect_ready(pcie_port))
				err = -EPERM;
		}
	}

	spin_unlock_irqrestore(&pcie_port->vote_lock, flags);

	return err;
}
EXPORT_SYMBOL(mtk_pcie_hw_control_vote);

static int __maybe_unused mtk_pcie_suspend_noirq(struct device *dev)
{
	struct mtk_pcie_port *port = dev_get_drvdata(dev);
	struct pci_host_bridge *host = pci_host_bridge_from_priv(port);
	struct pci_dev *pdev = pci_get_slot(host->bus, 0);
	int err;
	u32 val;

	if (!pdev)
		return -ENODEV;

	if (port->suspend_mode == LINK_STATE_L12) {
		dev_info(port->dev, "pcie LTSSM=%#x, pcie L1SS_pm=%#x\n",
			 readl_relaxed(port->base + PCIE_LTSSM_STATUS_REG),
			 readl_relaxed(port->base + PCIE_ISTATUS_PM));

		if (port->data->suspend_l12) {
			err = port->data->suspend_l12(port);
			if (err)
				return err;
		}

		if (port->port_num == 0) {
			err = mtk_pcie_hw_control_vote(0, true, 0);
			if (err)
				return err;
		} else if (port->port_num == 1) {
			val = readl_relaxed(port->pextpcfg + PEXTP_PWRCTL_1);
			val |= PCIE_HW_MTCMOS_EN_P1;
			writel_relaxed(val, port->pextpcfg + PEXTP_PWRCTL_1);
		}

		/* srclken rc request state */
		dev_info(port->dev, "PCIe0 Modem HW MODE BIT=%#x, PEXTP_PWRCTL_3=%#x, srclken rc state=%#x\n",
			 readl_relaxed(port->pextpcfg + PEXTP_RSV_0),
			 readl_relaxed(port->pextpcfg + PEXTP_PWRCTL_3),
			 readl_relaxed(port->vlpcfg_base + SRCLKEN_RC_REQ_STA));
	} else {
		/* The user enters L2 in advance by himself, so skip suspend directly */
		if (port->soft_off) {
			dev_info(port->dev, "PCIe soft off mode\n");
			return 0;
		}

		if (port->port_num == 1) {
			val = readl_relaxed(port->pextpcfg + PEXTP_PWRCTL_1);
			if (val & BIT(7)) {
				dev_info(port->dev, "Suspend, Keep PCIe active, don't enter L2\n");
				return 0;
			}
		}

		pci_save_state(pdev);

		/* Trigger link to L2 state */
		err = mtk_pcie_turn_off_link(port);
		if (err) {
			dev_info(port->dev, "cannot enter L2 state\n");
			return err;
		}

		dev_info(port->dev, "entered L2 states successfully");

		mtk_pcie_irq_save(port);
		mtk_pcie_power_down(port);
		pinctrl_pm_select_idle_state(port->dev);
	}

	return 0;
}

static int __maybe_unused mtk_pcie_resume_noirq(struct device *dev)
{
	struct mtk_pcie_port *port = dev_get_drvdata(dev);
	struct pci_host_bridge *host = pci_host_bridge_from_priv(port);
	struct pci_dev *pdev = pci_get_slot(host->bus, 0);
	int err;
	u32 val;

	if (!pdev)
		return -ENODEV;

	if (port->suspend_mode == LINK_STATE_L12) {
		if (port->port_num == 0) {
			err = mtk_pcie_hw_control_vote(0, false, 0);
			if (err)
				return err;

			dev_info(port->dev, "Modem HW MODE BIT=%#x, PEXTP_PWRCTL_3=%#x\n",
				 readl_relaxed(port->pextpcfg + PEXTP_RSV_0),
				 readl_relaxed(port->pextpcfg + PEXTP_PWRCTL_3));
		} else if (port->port_num == 1) {
			val = readl_relaxed(port->pextpcfg + PEXTP_PWRCTL_1);
			val &= ~PCIE_HW_MTCMOS_EN_P1;
			writel_relaxed(val, port->pextpcfg + PEXTP_PWRCTL_1);
		}

		if (port->data->resume_l12) {
			err = port->data->resume_l12(port);
			if (err)
				return err;
		}
	} else {
		/* The user controls the exit from L2 by himself */
		if (port->soft_off) {
			dev_info(port->dev, "PCIe soft off mode\n");
			return 0;
		}

		if (port->port_num == 1) {
			val = readl_relaxed(port->pextpcfg + PEXTP_PWRCTL_1);
			if (val & BIT(7)) {
				dev_info(port->dev, "Resume, PCIe is ready active\n");
				return 0;
			}
		}

		pinctrl_pm_select_default_state(port->dev);

		err = mtk_pcie_power_up(port);
		if (err)
			return err;

		err = mtk_pcie_startup_port(port);
		if (err) {
			mtk_pcie_power_down(port);
			return err;
		}

		pci_restore_state(pdev);
		mtk_pcie_irq_restore(port);
	}

	return 0;
}

static const struct dev_pm_ops mtk_pcie_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(mtk_pcie_suspend_noirq,
				      mtk_pcie_resume_noirq)
};

int mtk_pcie_soft_off(struct pci_bus *bus)
{
	struct pci_host_bridge *host;
	struct mtk_pcie_port *port;
	struct pci_dev *dev;
	int ret;
	u32 val;

	if (!bus) {
		pr_info("There is no bus, please check the host driver\n");
		return -ENODEV;
	}

	port = bus->sysdata;
	host = pci_host_bridge_from_priv(port);
	dev = pci_get_slot(host->bus, 0);
	if (!dev)
		return -ENODEV;

	/* Trigger link to L2 state */
	ret = mtk_pcie_turn_off_link(port);
	if (ret) {
		val = readl_relaxed(port->base + PCIE_LTSSM_STATUS_REG);
		dev_info(port->dev, "cannot enter L2 state, LTSSM=%#x\n", val);

		/* Need to clear the turn_off_link bit */
		val = readl_relaxed(port->base + PCIE_ICMD_PM_REG);
		val &= ~PCIE_TURN_OFF_LINK;
		writel_relaxed(val, port->base + PCIE_ICMD_PM_REG);

		return ret;
	}

	dev_info(port->dev, "entered L2 states successfully\n");

	pci_save_state(dev);
	pci_dev_put(dev);
	mtk_pcie_irq_save(port);
	mtk_pcie_power_down(port);
	port->soft_off = true;

	dev_info(port->dev, "mtk pcie soft off done\n");

	return ret;
}
EXPORT_SYMBOL(mtk_pcie_soft_off);

int mtk_pcie_soft_on(struct pci_bus *bus)
{
	struct pci_host_bridge *host;
	struct mtk_pcie_port *port;
	struct pci_dev *dev;
	int ret;

	if (!bus) {
		pr_info("There is no bus, please check the host driver\n");
		return -ENODEV;
	}

	port = bus->sysdata;
	host = pci_host_bridge_from_priv(port);
	dev = pci_get_slot(host->bus, 0);
	if (!dev)
		return -ENODEV;

	if (!port->soft_off) {
		pr_info("The soft_off is false, can't soft on\n");
		return -EPERM;
	}

	ret = mtk_pcie_power_up(port);
	if (ret)
		return ret;

	ret = mtk_pcie_startup_port(port);
	if (ret)
		return ret;

	mtk_pcie_irq_restore(port);
	pci_restore_state(dev);
	pci_dev_put(dev);

	port->soft_off = false;

	dev_info(port->dev, "mtk pcie soft on done\n");

	return ret;
}
EXPORT_SYMBOL(mtk_pcie_soft_on);

static int mtk_pcie_pre_init_6985(struct mtk_pcie_port *port)
{
	u32 val;

	/* Enable P2_EXIT signal to phy, wait 8us for EP entering L1ss */
	val = readl_relaxed(port->base + PCIE_ASPM_CTRL);
	val &= ~PCIE_P2_IDLE_TIME_MASK;
	val |= PCIE_P2_EXIT_BY_CLKREQ | PCIE_P2_IDLE_TIME(8);
	writel_relaxed(val, port->base + PCIE_ASPM_CTRL);

	return 0;
}

static int mtk_pcie_suspend_l12_6985(struct mtk_pcie_port *port)
{
	int err;
	u32 val;

	/* Binding of BBCK1 and BBCK2 */
	err = clkbuf_xo_ctrl("SET_XO_VOTER", PCIE_CLKBUF_XO_ID, BBCK1_BBCK2_BIND);
	if (err)
		dev_info(port->dev, "Failed to bind BBCK2 with BBCK1\n");

	/* Wait 400us for BBCK2 bind ready */
	udelay(400);

	/* Enable Bypass BBCK2 */
	val = readl_relaxed(port->pextpcfg + PEXTP_RSV_0);
	val |= PCIE_BBCK2_BYPASS;
	writel_relaxed(val, port->pextpcfg + PEXTP_RSV_0);

	mtk_pcie_clkbuf_control(port->dev, false);

	return 0;
}

static int mtk_pcie_resume_l12_6985(struct mtk_pcie_port *port)
{
	int err;

	mtk_pcie_clkbuf_control(port->dev, true);

	/* Wait 400us for BBCK2 switch SW Mode ready */
	udelay(400);

	/* Binding of PMRC7 and BBCK2 */
	err = clkbuf_xo_ctrl("SET_XO_VOTER", PCIE_CLKBUF_XO_ID, PMRC7_BBCK2_BIND);
	if (err)
		dev_info(port->dev, "Failed to bind PMRC7 with BBCK2\n");

	return 0;
}

static int mtk_pcie_pre_power_up_6985(struct mtk_pcie_port *port)
{
	mtk_pcie_clkbuf_control(port->dev, true);
	return 0;
}

static void mtk_pcie_post_power_down_6985(struct mtk_pcie_port *port)
{
	mtk_pcie_clkbuf_control(port->dev, false);
}

static const struct mtk_pcie_data mt6985_data = {
	.pre_init		= mtk_pcie_pre_init_6985,
	.suspend_l12		= mtk_pcie_suspend_l12_6985,
	.resume_l12		= mtk_pcie_resume_l12_6985,
	.pre_power_up		= mtk_pcie_pre_power_up_6985,
	.post_power_down	= mtk_pcie_post_power_down_6985,
};

static int mtk_pcie_pre_init_6989(struct mtk_pcie_port *port)
{
	u32 val;

	/* Make PCIe RC wait apsrc_ack signal before access EMI */
	val = readl_relaxed(port->base + PCIE_RESOURCE_CTRL);
	val |= PCIE_APSRC_ACK;
	writel_relaxed(val, port->base + PCIE_RESOURCE_CTRL);

	/* Don't let PCIe AXI0 port reply slave error */
	val = readl_relaxed(port->base + PCIE_AXI_IF_CTRL);
	val |= PCIE_AXI0_SLV_RESP_MASK;
	writel_relaxed(val, port->base + PCIE_AXI_IF_CTRL);

	/* Set write completion timeout to 4ms */
	writel_relaxed(WCPL_TIMEOUT_4MS, port->base + PCIE_WCPL_TIMEOUT);

	/* Set p2_sleep_time to 4us */
	val = readl_relaxed(port->base + PCIE_ASPM_CTRL);
	val &= ~PCIE_P2_SLEEP_TIME_MASK;
	val |= PCIE_P2_SLEEP_TIME_4US;
	writel_relaxed(val, port->base + PCIE_ASPM_CTRL);

	return 0;
}

static int mtk_pcie_pre_power_up_6989(struct mtk_pcie_port *port)
{
	void __iomem *srclken_base = ioremap(SRCLKEN_BASE, 0x1000);
	u32 val;

	/* Make PMRC7 out of vote */
	val = readl_relaxed(srclken_base + CENTRAL_CFG6);
	val |= PMRC7_VOTE_MASK;
	writel_relaxed(val, srclken_base + CENTRAL_CFG6);

	/* PMRC7 ignore settle time */
	val = readl_relaxed(srclken_base + M07_CFG);
	val |= IGNORE_SETTLE_TIME;
	writel_relaxed(val, srclken_base + M07_CFG);

	/* RC force req = 1 */
	val = readl_relaxed(srclken_base + M07_CFG);
	val |= BBCK2_FORCE_ACK;
	writel_relaxed(val, srclken_base + M07_CFG);

	/* Unbind PMRC7 with BBCK2 and bind PMRC0 with BBCK2 */
	if (clkbuf_xo_ctrl("SET_XO_VOTER", PCIE_CLKBUF_XO_ID,
			   PMRC7_BBCK2_UNBIND))
		dev_info(port->dev, "Failed to unbind PMRC7 with BBCK2\n");

	udelay(400);
	iounmap(srclken_base);

	return 0;
}

static void mtk_pcie_post_power_down_6989(struct mtk_pcie_port *port)
{
	void __iomem *srclken_base = ioremap(SRCLKEN_BASE, 0x1000);
	u32 val;

	/* Unbind PMRC0 with BBCK2 and bind BBCK2 with PMRC7 */
	if (clkbuf_xo_ctrl("SET_XO_VOTER", PCIE_CLKBUF_XO_ID, PMRC7_BBCK2_BIND))
		dev_info(port->dev, "Failed to bind PMRC7 with BBCK2\n");

	/* RC HW mode and clear force req = 1 */
	val = readl_relaxed(srclken_base + M07_CFG);
	val &= ~BBCK2_FORCE_ACK;
	writel_relaxed(val, srclken_base + M07_CFG);

	/* PMRC7 clear ignore settle time */
	val = readl_relaxed(srclken_base + M07_CFG);
	val &= ~IGNORE_SETTLE_TIME;
	writel_relaxed(val, srclken_base + M07_CFG);

	/* Make PMRC7 can vote for BBCK2 */
	val = readl_relaxed(srclken_base + CENTRAL_CFG6);
	val &= ~PMRC7_VOTE_MASK;
	writel_relaxed(val, srclken_base + CENTRAL_CFG6);

	iounmap(srclken_base);
}

static const struct mtk_pcie_data mt6989_data = {
	.pre_init		= mtk_pcie_pre_init_6989,
	.pre_power_up		= mtk_pcie_pre_power_up_6989,
	.post_power_down	= mtk_pcie_post_power_down_6989,
};

static const struct of_device_id mtk_pcie_of_match[] = {
	{ .compatible = "mediatek,mt8192-pcie" },
	{ .compatible = "mediatek,mt6985-pcie", .data = &mt6985_data },
	{ .compatible = "mediatek,mt6989-pcie", .data = &mt6989_data },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_pcie_of_match);

static struct platform_driver mtk_pcie_driver = {
	.probe = mtk_pcie_probe,
	.remove = mtk_pcie_remove,
	.driver = {
		.name = "mtk-pcie",
		.of_match_table = mtk_pcie_of_match,
		.pm = &mtk_pcie_pm_ops,
	},
};

static int mtk_pcie_init_func(void *pvdev)
{
#if IS_ENABLED(CONFIG_ANDROID_FIX_PCIE_SLAVE_ERROR)
	int err = 0;

	err = register_trace_android_rvh_do_serror(
			pcie_android_rvh_do_serror, NULL);
	if (err)
		pr_info("register pcie android_rvh_do_serror failed!\n");
#endif

	return platform_driver_register(&mtk_pcie_driver);
}

static int __init mtk_pcie_init(void)
{
	struct task_struct *driver_thread_handle;

	driver_thread_handle = kthread_run(mtk_pcie_init_func,
					   NULL, "pcie_thread");

	if (IS_ERR(driver_thread_handle))
		return PTR_ERR(driver_thread_handle);

	return 0;
}

static void __exit mtk_pcie_exit(void)
{
	platform_driver_unregister(&mtk_pcie_driver);
}

module_init(mtk_pcie_init);
module_exit(mtk_pcie_exit);
MODULE_LICENSE("GPL v2");
