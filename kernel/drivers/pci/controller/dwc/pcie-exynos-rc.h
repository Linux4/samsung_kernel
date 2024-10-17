/*
 * PCIe host controller driver for Samsung EXYNOS SoCs
 *
 * Copyright (C) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PCIE_EXYNOS_RC_H
#define __PCIE_EXYNOS_RC_H

/* PCIe ELBI registers */
#define PCIE_IRQ0			0x000
#define IRQ_INTA_ASSERT			(0x1 << 14)
#define IRQ_INTB_ASSERT			(0x1 << 16)
#define IRQ_INTC_ASSERT			(0x1 << 18)
#define IRQ_INTD_ASSERT			(0x1 << 20)
#define IRQ_RADM_PM_TO_ACK              (0x1 << 29)
#define PCIE_IRQ1			0x004
#define IRQ_LINK_DOWN			(0x1 << 10)
#define PCIE_IRQ2			0x008
#define IRQ_MSI_FALLING_ASSERT		(0x1 << 16)
#define IRQ_MSI_RISING_ASSERT		(0x1 << 17)
#define IRQ_MSI_LEVEL			(0x1 << 20)
#define IRQ_RADM_CPL_TIMEOUT		(0x1 << 24)
#define PCIE_IRQ0_EN			0x010
#define PCIE_IRQ1_EN			0x014
#define IRQ_LINKDOWN_ENABLE_EVT1_1	(0x1 << 10)
#define PCIE_IRQ2_EN			0x018
#define IRQ_LINKDOWN_ENABLE		(0x1 << 14)
#define IRQ_MSI_CTRL_EN_FALLING_EDG	(0x1 << 16)
#define IRQ_MSI_CTRL_EN_RISING_EDG	(0x1 << 17)
#define IRQ_MSI_CTRL_EN_LEVEL		(0x1 << 20)
#define PCIE_APP_LTSSM_ENABLE		0x054
#define PCIE_ELBI_LTSSM_DISABLE		0x0
#define PCIE_ELBI_LTSSM_ENABLE		0x1
#define PCIE_APP_REQ_EXIT_L1		0x06C
#define XMIT_PME_TURNOFF		0x118
#define PCIE_ELBI_RDLH_LINKUP		0x2C8
#define PCIE_CXPL_DEBUG_INFO_H		0x2CC
#define PCIE_PM_DSTATE			0x2E8
#define PCIE_LINKDOWN_RST_CTRL_SEL	0x3A0
#define PCIE_LINKDOWN_RST_MANUAL	(0x1 << 1)
#define PCIE_LINKDOWN_RST_FSM		(0x1 << 0)
#define PCIE_SOFT_RESET			0x3A4
#define SOFT_CORE_RESET			(0x1 << 0)
#define SOFT_PWR_RESET			(0x1 << 1)
#define SOFT_STICKY_RESET		(0x1 << 2)
#define SOFT_NON_STICKY_RESET		(0x1 << 3)
#define SOFT_PHY_RESET			(0x1 << 4)
#define PCIE_QCH_SEL			0x3A8
#define CLOCK_GATING_IN_L12		0x1
#define CLOCK_NOT_GATING		0x3
#define CLOCK_GATING_MASK		0x3
#define CLOCK_GATING_PMU_L1		(0x1 << 11)
#define CLOCK_GATING_PMU_L23READY	(0x1 << 10)
#define CLOCK_GATING_PMU_DETECT_QUIET	(0x1 << 9)
#define CLOCK_GATING_PMU_L12		(0x1 << 8)
#define CLOCK_GATING_PMU_ALL		(0xF << 8)
#define CLOCK_GATING_PMU_MASK		(0xF << 8)
#define CLOCK_GATING_APB_L1		(0x1 << 7)
#define CLOCK_GATING_APB_L23READY	(0x1 << 6)
#define CLOCK_GATING_APB_DETECT_QUIET	(0x1 << 5)
#define CLOCK_GATING_APB_L12		(0X1 << 4)
#define CLOCK_GATING_APB_ALL		(0xF << 4)
#define CLOCK_GATING_APB_MASK		(0xF << 4)
#define CLOCK_GATING_AXI_L1		(0x1 << 3)
#define CLOCK_GATING_AXI_L23READY	(0x1 << 2)
#define CLOCK_GATING_AXI_DETECT_QUIET	(0x1 << 1)
#define CLOCK_GATING_AXI_L12		(0x1 << 0)
#define CLOCK_GATING_AXI_ALL		(0xF << 0)
#define CLOCK_GATING_AXI_MASK		(0xF << 0)
#define PCIE_APP_REQ_EXIT_L1_MODE	0x3BC
#define APP_REQ_EXIT_L1_MODE		0x1
#define L1_REQ_NAK_CONTROL_MASTER	(0x1 << 4)
#define PCIE_SW_WAKE			0x3D4
#define PCIE_STATE_HISTORY_CHECK	0xC00
#define HISTORY_BUFFER_ENABLE		0x3
#define HISTORY_BUFFER_CLEAR		(0x1 << 1)
#define HISTORY_BUFFER_CONDITION_SEL	(0x1 << 2)
#define PCIE_STATE_POWER_S		0xC04
#define PCIE_STATE_POWER_M		0xC08
#define PCIE_HISTORY_REG(x)		(0xC0C + ((x) * 0x4)) /*history_reg0 : 0xC0C*/
#define LTSSM_STATE(x)			(((x) >> 16) & 0x3f)
#define PM_DSTATE(x)			(((x) >> 8) & 0x7)
#define L1SUB_STATE(x)			(((x) >> 0) & 0x7)
#define PCIE_DMA_MONITOR1		0x460
#define PCIE_DMA_MONITOR2		0x464
#define PCIE_DMA_MONITOR3		0x468
#define FSYS1_MON_SEL_MASK		0xf
#define PCIE_MON_SEL_MASK		0xff

#define PCIE_MSTR_PEND_SEL_NAK		0x474
#define NACK_ENABLE			0x1

/* PCIe PMU registers */
#define PCIE_PHY_CONTROL_MASK		0x1
#define PCIE_GPIO_RETENTION(x)		(0x1 << (16 + (x)))

/* PCIe DBI registers */

/* For PCIe CAP */
#define PCI_EXP_LNKCAP_L1EL_64USEC	(0x7 << 15)
#define PCI_EXP_DEVCTL2_COMP_TOUT_6_2MS	0x2

/* For L1SS CAP */
#define PCI_L1SS_TCOMMON_32US		(0x20 << 8)
#define PCI_L1SS_TCOMMON_70US		(0x46 << 8)
#define PCI_L1SS_CTL1_ALL_PM_EN		(0xf << 0)
#define PCI_L1SS_CTL1_LTR_THRE_VAL	(0x40a0 << 16)
#define PCI_L1SS_CTL1_LTR_THRE_VAL_0US	(0x4000 << 16)
#define PCI_L1SS_CTL2_TPOWERON_90US	(0x49 << 0)
#define PCI_L1SS_CTL2_TPOWERON_130US	(0x69 << 0)
#define PCI_L1SS_CTL2_TPOWERON_170US	(0x89 << 0)
#define PCI_L1SS_CTL2_TPOWERON_180US	(0x91 << 0)
#define PCI_L1SS_CTL2_TPOWERON_3100US	(0xfa << 0)

/* For Port Logic Registers */
#define PORT_AFR_L1_ENTRANCE_LAT_8us 	(0x3 << 27)
#define PORT_AFR_L1_ENTRANCE_LAT_16us	(0x4 << 27)
#define PORT_AFR_L1_ENTRANCE_LAT_32us	(0x5 << 27)
#define PORT_AFR_L1_ENTRANCE_LAT_64us	(0x7 << 27)

#define PCIE_PORT_COHERENCY_CTR_3	0x8E8

#define PCIE_PORT_AUX_CLK_FREQ		0xB40
#define PORT_AUX_CLK_FREQ_76MHZ		0x4C

#define PCIE_PORT_L1_SUBSTATES		0xB44
#define PORT_L1_SUBSTATES_VAL		0xEA

/* For ATU CAP */
#define PCIE_ATU_CR1_OUTBOUND0		0x300000
#define PCIE_ATU_CR2_OUTBOUND0		0x300004
#define PCIE_ATU_LOWER_BASE_OUTBOUND0	0x300008
#define PCIE_ATU_UPPER_BASE_OUTBOUND0	0x30000C
#define PCIE_ATU_LIMIT_OUTBOUND0	0x300010
#define PCIE_ATU_LOWER_TARGET_OUTBOUND0	0x300014
#define PCIE_ATU_UPPER_TARGET_OUTBOUND0	0x300018

#define PCIE_ATU_CR1_OUTBOUND1		0x300200
#define PCIE_ATU_CR2_OUTBOUND1		0x300204
#define PCIE_ATU_LOWER_BASE_OUTBOUND1	0x300208
#define PCIE_ATU_UPPER_BASE_OUTBOUND1	0x30020C
#define PCIE_ATU_LIMIT_OUTBOUND1	0x300210
#define PCIE_ATU_LOWER_TARGET_OUTBOUND1	0x300214
#define PCIE_ATU_UPPER_TARGET_OUTBOUND1	0x300218

#define PCIE_ATU_CR1_OUTBOUND2		0x300400
#define PCIE_ATU_CR2_OUTBOUND2		0x300404
#define PCIE_ATU_LOWER_BASE_OUTBOUND2	0x300408
#define PCIE_ATU_UPPER_BASE_OUTBOUND2	0x30040C
#define PCIE_ATU_LIMIT_OUTBOUND2	0x300410
#define PCIE_ATU_LOWER_TARGET_OUTBOUND2	0x300414
#define PCIE_ATU_UPPER_TARGET_OUTBOUND2	0x300418

/* PCIe EOM related definitions */
#define PCIE_EOM_PH_SEL_MAX		72
#define PCIE_EOM_DEF_VREF_MAX		256

#define PCIE_EOM_RX_EFOM_DONE			0x140C
#define PCIE_EOM_RX_EFOM_EOM_PH_SEL		0x1558
#define PCIE_EOM_RX_EFOM_MODE			0x1548
#define PCIE_EOM_RX_SSLMS_ADAP_HOLD_PMAD	0x11A0
#define PCIE_EOM_RX_EFOM_NUM_OF_SAMPLE_13_8	0x154C
#define PCIE_EOM_RX_EFOM_NUM_OF_SAMPLE_7_0	0x154C
#define PCIE_EOM_MON_RX_EFOM_ERR_CNT_13_8	0x1564
#define PCIE_EOM_MON_RX_EFOM_ERR_CNT_7_0	0x1560
#define PCIE_EOM_RX_EFOM_DFE_VREF_CTRL		0x1550
#define PCIE_EOM_RX_EFOM_START			0x1540

/* PCIe SYSREG registers */
#define PCIE_SYSREG_HSI1_SHARABLE_MASK		0x3
#define PCIE_SYSREG_HSI1_SHARABLE_ENABLE	0x3
#define PCIE_SYSREG_HSI1_SHARABLE_DISABLE	0x0

/* ETC definitions */
#define PCI_DEVICE_ID_EXYNOS		(0xECEC)

#define CAP_NEXT_OFFSET_MASK 		(0xFF << 8)
#define CAP_ID_MASK			(0xFF)
#define EXYNOS_PCIE_AP_MASK		(0xFFFF00)
#define EXYNOS_PCIE_REV_MASK		(0xFF)

#define MAX_RC_NUM		2
#define PCIE_IS_IDLE		1
#define PCIE_IS_ACTIVE		0

#define MAX_PCIE_PIN_STATE	2
#define PCIE_PIN_ACTIVE		0
#define PCIE_PIN_IDLE		1

#define PCI_FIND_CAP_TTL	(48)

#define MAX_TIMEOUT		12000
#define MAX_TIMEOUT_GEN1_GEN2	5000
#define MAX_L2_TIMEOUT		2000
#define MAX_L1_EXIT_TIMEOUT	300
#define ID_MASK			0xffff

/* PCIe driver macro definitions */
#define pcie_info(fmt, args...) \
		dev_info(exynos_pcie->pci->dev, fmt, ##args)
#define pcie_err(fmt, args...) \
		dev_err(exynos_pcie->pci->dev, fmt, ##args)
#define pcie_warn(fmt, args...) \
		dev_warn(exynos_pcie->pci->dev, fmt, ##args)
#define pcie_dbg(fmt, args...) \
		dev_dbg(exynos_pcie->pci->dev, fmt, ##args)

#define to_exynos_pcie(x)	dev_get_drvdata((x)->dev)
#define to_pci_dev_from_dev(dev) container_of((dev), struct pci_dev, dev)

#define PCIE_BUS_PRIV_DATA(pdev) \
	((struct dw_pcie_rp *)pdev->bus->sysdata)

#define CAP_ID_NAME(id)	\
	(id == PCI_CAP_ID_PM)	?	"Power Management" :	\
	(id == PCI_CAP_ID_MSI)	?	"Message Signalled Interrupts" :	\
	(id == PCI_CAP_ID_EXP)	?	"PCI Express" :	\
	(id == PCI_CAP_ID_MSIX)	?	"MSI-X" :	\
	" Unknown id..!!"

#define PCI_EXT_CAP_ID_LM	(0x27)

#define EXT_CAP_ID_NAME(id)	\
	(id == PCI_EXT_CAP_ID_ERR)	?	"Advanced Error Reporting" :	\
	(id == PCI_EXT_CAP_ID_VC)	?	"Virtual Channel Capability" :	\
	(id == PCI_EXT_CAP_ID_DSN)	?	"Device Serial Number" :	\
	(id == PCI_EXT_CAP_ID_PWR)	?	"Power Budgeting" :	\
	(id == PCI_EXT_CAP_ID_RCLD)	?	"RC Link Declaration" :	\
	(id == PCI_EXT_CAP_ID_SECPCI)	?	"Secondary PCIe Capability" :	\
	(id == PCI_EXT_CAP_ID_L1SS)	?	"L1 PM Substates" :	\
	(id == PCI_EXT_CAP_ID_DLF)	?	"Data Link Feature" :	\
	(id == PCI_EXT_CAP_ID_PL_16GT)	?	"Physical Layer 16GT/s"	:	\
	(id == PCI_EXT_CAP_ID_VNDR)	?	"Vendor-Specific"	:	\
	(id == PCI_EXT_CAP_ID_LTR)	?	"Latency Tolerance Reporting"	:	\
	(id == PCI_EXT_CAP_ID_LM)	?	"Lane Margining at the Receiver" :	\
	" Unknown id ..!!"

enum exynos_pcie_opr_state {
	OPR_STATE_PROBE = 0,
	OPR_STATE_POWER_ON,
	OPR_STATE_POWER_OFF,
};

enum exynos_pcie_state {
	STATE_LINK_DOWN = 0,
	STATE_LINK_UP_TRY,
	STATE_LINK_DOWN_TRY,
	STATE_LINK_UP,
	STATE_PHY_OPT_OFF,
};

enum __ltssm_states {
	S_DETECT_QUIET			= 0x00,
	S_DETECT_ACT			= 0x01,
	S_POLL_ACTIVE			= 0x02,
	S_POLL_COMPLIANCE		= 0x03,
	S_POLL_CONFIG			= 0x04,
	S_PRE_DETECT_QUIET		= 0x05,
	S_DETECT_WAIT			= 0x06,
	S_CFG_LINKWD_START		= 0x07,
	S_CFG_LINKWD_ACEPT		= 0x08,
	S_CFG_LANENUM_WAIT		= 0x09,
	S_CFG_LANENUM_ACEPT		= 0x0A,
	S_CFG_COMPLETE			= 0x0B,
	S_CFG_IDLE			= 0x0C,
	S_RCVRY_LOCK			= 0x0D,
	S_RCVRY_SPEED			= 0x0E,
	S_RCVRY_RCVRCFG			= 0x0F,
	S_RCVRY_IDLE			= 0x10,
	S_L0				= 0x11,
	S_L0S				= 0x12,
	S_L123_SEND_EIDLE		= 0x13,
	S_L1_IDLE			= 0x14,
	S_L2_IDLE			= 0x15,
	S_L2_WAKE			= 0x16,
	S_DISABLED_ENTRY		= 0x17,
	S_DISABLED_IDLE			= 0x18,
	S_DISABLED			= 0x19,
	S_LPBK_ENTRY			= 0x1A,
	S_LPBK_ACTIVE			= 0x1B,
	S_LPBK_EXIT			= 0x1C,
	S_LPBK_EXIT_TIMEOUT		= 0x1D,
	S_HOT_RESET_ENTRY		= 0x1E,
	S_HOT_RESET			= 0x1F,
};

#define LINK_STATE_DISP(state)  \
	(state == S_DETECT_QUIET)       ? "DETECT QUIET" : \
        (state == S_DETECT_ACT)         ? "DETECT ACT" : \
        (state == S_POLL_ACTIVE)        ? "POLL ACTIVE" : \
        (state == S_POLL_COMPLIANCE)    ? "POLL COMPLIANCE" : \
        (state == S_POLL_CONFIG)        ? "POLL CONFIG" : \
        (state == S_PRE_DETECT_QUIET)   ? "PRE DETECT QUIET" : \
        (state == S_DETECT_WAIT)        ? "DETECT WAIT" : \
        (state == S_CFG_LINKWD_START)   ? "CFG LINKWD START" : \
        (state == S_CFG_LINKWD_ACEPT)   ? "CFG LINKWD ACEPT" : \
        (state == S_CFG_LANENUM_WAIT)   ? "CFG LANENUM WAIT" : \
        (state == S_CFG_LANENUM_ACEPT)  ? "CFG LANENUM ACEPT" : \
        (state == S_CFG_COMPLETE)       ? "CFG COMPLETE" : \
        (state == S_CFG_IDLE)           ? "CFG IDLE" : \
        (state == S_RCVRY_LOCK)         ? "RCVRY LOCK" : \
        (state == S_RCVRY_SPEED)        ? "RCVRY SPEED" : \
        (state == S_RCVRY_RCVRCFG)      ? "RCVRY RCVRCFG" : \
        (state == S_RCVRY_IDLE)         ? "RCVRY IDLE" : \
        (state == S_L0)                 ? "L0" : \
        (state == S_L0S)                ? "L0s" : \
        (state == S_L123_SEND_EIDLE)    ? "L123 SEND EIDLE" : \
        (state == S_L1_IDLE)            ? "L1 IDLE " : \
        (state == S_L2_IDLE)            ? "L2 IDLE"  : \
        (state == S_L2_WAKE)            ? "L2 _WAKE" : \
        (state == S_DISABLED_ENTRY)     ? "DISABLED ENTRY" : \
        (state == S_DISABLED_IDLE)      ? "DISABLED IDLE" : \
        (state == S_DISABLED)           ? "DISABLED" : \
        (state == S_LPBK_ENTRY)         ? "LPBK ENTRY " : \
        (state == S_LPBK_ACTIVE)        ? "LPBK ACTIVE" : \
        (state == S_LPBK_EXIT)          ? "LPBK EXIT" : \
        (state == S_LPBK_EXIT_TIMEOUT)  ? "LPBK EXIT TIMEOUT" : \
        (state == S_HOT_RESET_ENTRY)    ? "HOT RESET ENTRY" : \
        (state == S_HOT_RESET)          ? "HOT RESET" : \
        " Unknown state..!! "

#define EXUNOS_PCIE_STATE_NAME(state)	\
	(state == STATE_LINK_DOWN)	?	"LINK_DOWN" :		\
	(state == STATE_LINK_UP_TRY)	?	"LINK_UP_TRY" :		\
	(state == STATE_LINK_DOWN_TRY)	?	"LINK_DOWN_TRY" :	\
	(state == STATE_LINK_UP)	?	"LINK_UP"	:	\
	" Unknown state ...!!"

/* PCIe driver structure definitions */
struct regmap;
struct exynos_pcie;

struct pcie_eom_result {
	unsigned int phase;
	unsigned int vref;
	unsigned long int err_cnt;
};

struct exynos_pcie_ep_cfg {
	/* ex.*/
	int (*msi_init)(struct exynos_pcie *exynos_pcie);
	int (*l1ss_ep_specific_cfg)(struct exynos_pcie *exynos_pcie);
	void (*save_rmem)(struct exynos_pcie *exynos_pcie);
	void (*set_dma_ops)(struct device *dev);
	void (*set_ia_code)(struct exynos_pcie *exynos_pcie);
	unsigned long udelay_before_perst;
	unsigned long udelay_after_perst;
	unsigned long mdelay_before_perst_low;
	unsigned long mdelay_after_perst_low;
	int linkdn_callback_direct; /* Call callback function in ISR */
	u32 tpoweron_time;
	u32 common_restore_time;
	u32 ltr_threshold;
};

struct exynos_pcie {
	struct dw_pcie		*pci;
	struct pci_bus		*ep_pci_bus;
	struct pci_dev		*ep_pci_dev;
	void __iomem		*elbi_base;
	void __iomem		*soc_base;
	void __iomem		*phy_base;
	void __iomem		*phyudbg_base;
	void __iomem		*sysreg_iocc_base;
	void __iomem		*sysreg_base;
	void __iomem		*rc_dbi_base;
	void __iomem		*phy_pcs_base;
	void __iomem		*ia_base;
	void __iomem		*dbg_pmu1;
	void __iomem		*dbg_pmu2;
	unsigned int		pci_cap[PCI_FIND_CAP_TTL];
	unsigned int		pci_ext_cap[PCI_FIND_CAP_TTL];
	struct regmap		*pmureg;
	struct regmap		*sysreg;
	int			perst_gpio;
	int			ch_num;
	enum exynos_pcie_state	state;
	int			is_ep_scaned;
	int			probe_done;
	int			linkdown_cnt;
	int			idle_ip_index;
	int			pcie_is_linkup;
	bool			use_pcieon_sleep;
	bool			use_sysmmu;
	bool			use_ia;
	bool			cpl_timeout_recovery;
	bool			sudden_linkdown;
	spinlock_t		conf_lock;
	spinlock_t              reg_lock;
	spinlock_t              pcie_l1_exit_lock;
	struct workqueue_struct	*pcie_wq;
	struct pci_dev		*pci_dev;
	struct pci_saved_state	*pci_saved_configs;
	struct delayed_work	dislink_work;
	struct delayed_work	cpl_timeout_work;
	struct exynos_pcie_register_event *rc_event_reg[2];
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
	unsigned int            int_min_lock;
#endif
	u32			ip_ver;
	struct exynos_pcie_phy	*phy;
	int			l1ss_ctrl_id_state;
	int			ep_device_type;
	int			max_link_speed;

	struct pinctrl		*pcie_pinctrl;
	struct pinctrl_state	*pin_state[MAX_PCIE_PIN_STATE];
	struct pcie_eom_result **eom_result;
	struct notifier_block	itmon_nb;

	const struct exynos_pcie_ep_cfg *ep_cfg;

	char			*rmem_msi_name;
	phys_addr_t		rmem_msi_base;
	u32			rmem_msi_size;

	int ep_power_gpio;
	u32 pmu_phy_isolation;
	u32 pmu_gpio_retention;
	u32 sysreg_sharability;

	u32 btl_target_addr;
	u32 btl_offset;
	u32 btl_size;

	u32 sysreg_ia0_sel;
	u32 sysreg_ia1_sel;
	u32 sysreg_ia2_sel;

	struct device dup_ep_dev;
	int copy_dup_ep;
	int force_recover_linkdn;
	u32 current_busdev;

	int separated_msi_start_num;
};

struct separated_msi_vector {
	int is_used;
	int irq;
	void *context;
	irq_handler_t msi_irq_handler;
	int flags;
};

#define PCIE_EXYNOS_OP_READ(base, type)					\
static inline type exynos_##base##_read					\
	(struct exynos_pcie *pcie, u32 reg)				\
{									\
		u32 data = 0;						\
		data = readl(pcie->base##_base + reg);			\
		return (type)data;					\
}

#define PCIE_EXYNOS_OP_WRITE(base, type)				\
static inline void exynos_##base##_write				\
	(struct exynos_pcie *pcie, type value, type reg)		\
{									\
		writel(value, pcie->base##_base + reg);			\
}

PCIE_EXYNOS_OP_READ(elbi, u32);
PCIE_EXYNOS_OP_READ(phy, u32);
PCIE_EXYNOS_OP_READ(phy_pcs, u32);
PCIE_EXYNOS_OP_READ(ia, u32);
PCIE_EXYNOS_OP_READ(phyudbg, u32);
PCIE_EXYNOS_OP_WRITE(elbi, u32);
PCIE_EXYNOS_OP_WRITE(phy, u32);
PCIE_EXYNOS_OP_WRITE(phy_pcs, u32);
PCIE_EXYNOS_OP_WRITE(ia, u32);
PCIE_EXYNOS_OP_WRITE(phyudbg, u32);

/* PCIe driver function prototypes */
int exynos_pcie_rc_poweron(int ch_num);
void exynos_pcie_rc_poweroff(int ch_num);
int exynos_pcie_rc_l1ss_ctrl(int enable, int id);
void exynos_pcie_rc_dump_link_down_status(int ch_num);
int exynos_pcie_rc_rd_own_conf(struct dw_pcie_rp *pp, int where, int size, u32 *val);
int exynos_pcie_rc_wr_own_conf(struct dw_pcie_rp *pp, int where, int size, u32 val);
int exynos_pcie_rc_rd_other_conf(struct dw_pcie_rp *pp, struct pci_bus *bus,
				 u32 devfn, int where, int size, u32 *val);
int exynos_pcie_rc_wr_other_conf(struct dw_pcie_rp *pp, struct pci_bus *bus,
				 u32 devfn, int where, int size, u32 val);
/* PCIe debug related functions */
int create_pcie_eom_file(struct device *dev);
void remove_pcie_eom_file(struct device *dev);
void exynos_pcie_dbg_register_dump(struct exynos_pcie *exynos_pcie);
void exynos_pcie_dbg_print_link_history(struct exynos_pcie *exynos_pcie);
void exynos_pcie_dbg_dump_link_down_status(struct exynos_pcie *exynos_pcie);
void exynos_pcie_dbg_print_msi_register(struct exynos_pcie *exynos_pcie);
void exynos_pcie_dbg_print_oatu_register(struct exynos_pcie *exynos_pcie);
int create_pcie_sys_file(struct device *dev);
void remove_pcie_sys_file(struct device *dev);

/* For coverage check */
void exynos_pcie_rc_check_function_validity(struct exynos_pcie *exynos_pcie);
#endif
