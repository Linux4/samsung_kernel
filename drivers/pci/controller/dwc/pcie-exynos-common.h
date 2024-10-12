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

#ifndef __PCIE_EXYNOS_H
#define __PCIE_EXYNOS_H

#define PCI_DEVICE_ID_EXYNOS	0xecec

#define MAX_TIMEOUT		12000
#define MAX_TIMEOUT_SPEEDCHANGE 10000
#define MAX_TIMEOUT_LANECHANGE  10000
#define MAX_TIMEOUT_GEN1_GEN2	5000
#define MAX_L2_TIMEOUT		2000
#define MAX_L1_EXIT_TIMEOUT	300
#define LNKRCVYWAIT_TIMEOUT	500
#define PLL_LOCK_TIMEOUT	500
#define RX_OC_TIMEOUT		500
#define ID_MASK			0xffff
#define TPUT_THRESHOLD		150
#define MAX_RC_NUM		2

#define pcie_info(exynos_pcie, fmt, args...) \
		dev_info(exynos_pcie->pci->dev, fmt, ##args)
#define pcie_err(exynos_pcie, fmt, args...) \
		dev_err(exynos_pcie->pci->dev, fmt, ##args)
#define pcie_warn(exynos_pcie, fmt, args...) \
		dev_warn(exynos_pcie->pci->dev, fmt, ##args)
#define pcie_dbg(exynos_pcie, fmt, args...) \
		dev_dbg(exynos_pcie->pci->dev, fmt, ##args)

#define to_exynos_pcie(x)	dev_get_drvdata((x)->dev)

#define PCIE_BUS_PRIV_DATA(pdev) \
	((struct pcie_port *)pdev->bus->sysdata)

#define MAX_PCIE_PIN_STATE	2
#define PCIE_PIN_ACTIVE		0
#define PCIE_PIN_IDLE		1

#define APP_REQ_EXIT_L1_MODE		0x1

#define CAP_NEXT_OFFSET_MASK 		(0xFF << 8)
#define CAP_ID_MASK			0xFF
/* add on PCI spec 4.0 */
#define PCI_EXT_CAP_ID_DLINK_FEATURE	0x25
#define PCI_EXT_CAP_ID_PHYLYR16		0x26
#define PCI_EXT_CAP_ID_MRGN_EXT		0x27

#define PCI_MAX_CAP_NUM			(48)

#define EXYNOS_PCIE_AP_MASK		(0xFFFF00)
#define EXYNOS_PCIE_REV_MASK		(0xFF)


extern int hsi_tcxo_far_control(int owner, int on);

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

#define CAP_ID_NAME(id)	\
	(id == PCI_CAP_ID_PM)	?	"Power Management" :	\
	(id == PCI_CAP_ID_MSI)	?	"Message Signalled Interrupts" :	\
	(id == PCI_CAP_ID_EXP)	?	"PCI Express" :	\
	" Unknown id..!!"

#define EXT_CAP_ID_NAME(id)	\
	(id == PCI_EXT_CAP_ID_ERR)	?	"Advanced Error Reporting" :	\
	(id == PCI_EXT_CAP_ID_VC)	?	"Virtual Channel Capability" :	\
	(id == PCI_EXT_CAP_ID_DSN)	?	"Device Serial Number" :	\
	(id == PCI_EXT_CAP_ID_PWR)	?	"Power Budgeting" :	\
	(id == PCI_EXT_CAP_ID_RCLD)	?	"RC Link Declaration" :	\
	(id == PCI_EXT_CAP_ID_SECPCI)	?	"Secondary PCIe Capability" :	\
	(id == PCI_EXT_CAP_ID_L1SS)	?	"L1 PM Substates" :	\
	(id == PCI_EXT_CAP_ID_DLINK_FEATURE)	?	"Data Link Feature" :	\
	(id == PCI_EXT_CAP_ID_PHYLYR16)	?	"Physical Layer 16GT/s"	:	\
	(id == PCI_EXT_CAP_ID_MRGN_EXT)	?	"Physical Layer 16GT/s Margining" :	\
	" Unknown id ..!!"

struct regmap;

struct exynos_pcie_clks {
	struct clk	*pcie_clks[10];
	struct clk	*phy_clks[3];
};

enum exynos_pcie_state {
	STATE_LINK_DOWN = 0,
	STATE_LINK_UP_TRY,
	STATE_LINK_DOWN_TRY,
	STATE_LINK_UP,
	STATE_PHY_OPT_OFF,
};

#define EXUNOS_PCIE_STATE_NAME(state)	\
	(state == STATE_LINK_DOWN)	?	"LINK_DOWN" :		\
	(state == STATE_LINK_UP_TRY)	?	"LINK_UP_TRY" :		\
	(state == STATE_LINK_DOWN_TRY)	?	"LINK_DOWN_TRY" :	\
	(state == STATE_LINK_UP)	?	"LINK_UP"	:	\
	" Unknown state ...!!"

struct exynos_pcie;

struct exynos_pcie_ops {
	int (*poweron)(int ch_num);
	void (*poweroff)(int ch_num);
	int (*rd_own_conf)(struct pcie_port *pp, int where, int size,
				u32 *val);
	int (*wr_own_conf)(struct pcie_port *pp, int where, int size,
				u32 val);
	int (*rd_other_conf)(struct pcie_port *pp,
		struct pci_bus *bus, u32 devfn, int where, int size, u32 *val);
	int (*wr_other_conf)(struct pcie_port *pp,
		struct pci_bus *bus, u32 devfn, int where, int size, u32 val);
};

struct exynos_pcie_info {
	/* ex.*/
	int (*msi_init)(struct pcie_port *pp);
	void (*l1ss_ep_cfg)(struct pcie_port *pp);
	int (*l1ss_set)(int enable, struct pcie_port *pp, int id);
	void (*save_rmem)(struct exynos_pcie *exynos_pcie);
	void (*set_dma_ops)(struct device *dev);
	void (*set_ia_code)(struct exynos_pcie *exynos_pcie);
	int l1ss_delayed_work;
	unsigned long udelay_before_perst;
	unsigned long udelay_after_perst;
	unsigned long mdelay_before_perst_low;
	unsigned long mdelay_after_perst_low;
	int linkdn_callback_direct; /* Call callback function in ISR */
	int skip_boot_l1ss_ctrl; /* skip L1ss disable at booting time for 40sec */
	u32 tpoweron_time;
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
	unsigned int		pci_cap[PCI_MAX_CAP_NUM];
	unsigned int		pci_ext_cap[PCI_MAX_CAP_NUM];
	u32			ep_pcie_cap_off;
	u32			ep_ext_l1ss_cap_off;
	u32			ep_ext_ltr_cap_off;
	struct regmap		*pmureg;
	struct regmap		*sysreg;
	int			perst_gpio;
	int			ch_num;
	int			pcie_clk_num;
	int			phy_clk_num;
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
	struct exynos_pcie_clks	clks;
	struct pci_dev		*pci_dev;
	struct pci_saved_state	*pci_saved_configs;
	struct notifier_block	power_mode_nb;
	struct notifier_block   ss_dma_mon_nb;
	struct delayed_work	dislink_work;
	struct delayed_work	cpl_timeout_work;
	struct exynos_pcie_register_event *rc_event_reg[2];
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
	unsigned int            int_min_lock;
#endif
	u32			ip_ver;
	struct exynos_pcie_phy	*phy;
	struct exynos_pcie_ops	exynos_pcie_ops;
	int			l1ss_ctrl_id_state;
	struct workqueue_struct *pcie_wq_l1ss;
	struct delayed_work     l1ss_boot_delay_work;
	int			boot_cnt;
	int			work_l1ss_cnt;
	int			ep_device_type;
	int			max_link_speed;
	u32			current_speed;
	u32			dstate;

	struct pinctrl		*pcie_pinctrl;
	struct pinctrl_state	*pin_state[MAX_PCIE_PIN_STATE];
	struct pcie_eom_result **eom_result;
	struct notifier_block	itmon_nb;

	const struct exynos_pcie_info *info;
	struct platform_device *pcie_mem_pdev;

	char			*rmem_msi_name;
	phys_addr_t		rmem_msi_base;
	u32			rmem_msi_size;

	int wlan_gpio;
	int ssd_gpio;
	u32 pmu_phy_isolation;
	u32 pmu_phy_ctrl;
	u32 pmu_gpio_retention;
	u32 sysreg_sharability;
	u32 linkup_offset;

	u32 app_req_exit_l1;
	u32 app_req_exit_l1_mode;

	u32 btl_target_addr;
	u32 btl_offset;
	u32 btl_size;

	u32 sysreg_ia0_sel;
	u32 sysreg_ia1_sel;
	u32 sysreg_ia2_sel;

	struct device dup_ep_dev;
	int copy_dup_ep;
	int force_recover_linkdn;
	int separated_msi;
	u32 current_busdev;
};

#define PCIE_MAX_SEPA_IRQ_NUM	(5)
#define PCIE_START_SEP_MSI_VEC	(1)
#define PCIE_DOMAIN_MAX_IRQ	(256)

struct separated_msi_vector {
	int is_used;
	int irq;
	void *context;
	irq_handler_t msi_irq_handler;
	int flags;
};

#define PCIE_EXYNOS_MSI_DEFAULT_HANDLER_VEC(vector)				\
static irqreturn_t exynos_pcie_msi##vector##_handler(int irq, void *arg)	\
{										\
	struct pcie_port *pp = arg;						\
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);				\
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);			\
	struct device *dev = pci->dev;						\
	int ch_num = exynos_pcie->ch_num;					\
	struct separated_msi_vector *msi_vec = &sep_msi_vec[ch_num][vector];	\
	int vec_num = vector;							\
	if (!msi_vec->is_used) {						\
		dev_info(dev, "Unexpected separated MSI%d interrupt!", vec_num);\
		goto clear_irq;							\
	}									\
	dw_handle_msi_irq(pp);							\
clear_irq:									\
	return IRQ_HANDLED;							\
}

#define PCIE_EXYNOS_MSI_HANDLER_VEC(vector)					\
static irqreturn_t exynos_pcie_msi##vector##_handler(int irq, void *arg)	\
{										\
	struct pcie_port *pp = arg;						\
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);				\
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);			\
	struct device *dev = pci->dev;						\
	int ch_num = exynos_pcie->ch_num;					\
	struct separated_msi_vector *msi_vec = &sep_msi_vec[ch_num][vector];	\
	int vec_num = vector;							\
	exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_STATUS +			\
			(vec_num * MSI_REG_CTRL_BLOCK_SIZE), 4, 0x1);		\
	if (!msi_vec->is_used) {						\
		dev_info(dev, "Unexpected separated MSI%d interrupt!", vec_num);\
		goto clear_irq;							\
	}									\
	if (msi_vec->msi_irq_handler != NULL)					\
		msi_vec->msi_irq_handler(irq, msi_vec->context);		\
clear_irq:									\
	return IRQ_HANDLED;							\
}

#define PCIE_EXYNOS_OP_READ(base, type)					\
static inline type exynos_##base##_read					\
	(struct exynos_pcie *pcie, u32 reg)				\
{									\
		u32 data = 0;						\
		data = readl(pcie->base##_base + reg);			\
		return (type)data;					\
}

#define PCIE_EXYNOS_OP_WRITE(base, type)				\
static inline void exynos_##base##_write						\
	(struct exynos_pcie *pcie, type value, type reg)		\
{									\
		writel(value, pcie->base##_base + reg);			\
}

PCIE_EXYNOS_OP_READ(elbi, u32);
PCIE_EXYNOS_OP_READ(phy, u32);
PCIE_EXYNOS_OP_READ(phy_pcs, u32);
PCIE_EXYNOS_OP_READ(sysreg, u32);
PCIE_EXYNOS_OP_READ(ia, u32);
PCIE_EXYNOS_OP_READ(phyudbg, u32);
PCIE_EXYNOS_OP_WRITE(elbi, u32);
PCIE_EXYNOS_OP_WRITE(phy, u32);
PCIE_EXYNOS_OP_WRITE(phy_pcs, u32);
PCIE_EXYNOS_OP_WRITE(sysreg, u32);
PCIE_EXYNOS_OP_WRITE(ia, u32);
PCIE_EXYNOS_OP_WRITE(phyudbg, u32);
#endif
