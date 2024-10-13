/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/* Implements */
#include "pcie.h"

/* Uses */
#include "../pcie_mif.h"
#include <linux/pfn.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/msi.h>
#include <linux/pci.h>
#include <linux/exynos-pci-ctrl.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>

#include <scsc/scsc_debug-snapshot.h>

#include "ka_patch.h"
#include "pmu_reg.h"
#include "mif_reg.h"

#include "pcie_remapper.h"
#define DRV_NAME "scscPCIe"

static DEFINE_MUTEX(pci_link_lock);
static struct pcie_mif single_pci;
struct pcie_mif *pcie = &single_pci;

static bool fw_compiled_in_kernel;
static void *ramrp_buff;
module_param(fw_compiled_in_kernel, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fw_compiled_in_kernel, "Use FW compiled in kernel");

static bool enable_pcie_mif_arm_reset = true;
module_param(enable_pcie_mif_arm_reset, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_pcie_mif_arm_reset, "Enables ARM cores reset");

#ifdef CONFIG_SCSC_BB_PAEAN
/* Workaround for design issue that didn't make PMU a wakeup source from PCIe doorbell */
static bool wake_pmu = true;
module_param(wake_pmu, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(wake_pmu, "Write wakeup to PMU when in deep sleep");
#endif

static bool enable_pcie_cb_panic = false;
module_param(enable_pcie_cb_panic, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_pcie_cb_panic, "Enables linkdown and cpl timeout callback gose to panic");

struct pcie_mif;

void pcie_s6165_irqdump(struct pcie_mif *pcie)
{
	u32 val;
	if (!pcie_is_on(pcie)){
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "PCIe link is off\n");
		return;
	}
	exynos_pcie_rc_print_msi_register(pcie->ch_num);
	pci_read_config_dword(pcie->pdev, PCI_MSI_CAP_OFF_10H_REG, &val);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "msi mask register value 0x%x\n", val);
	val = readl(pcie->registers_pert + PMU_INTERRUPT_FROM_HOST_INTGR);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "msi generation register value 0x%x\n", val);

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "last wlan interrupt bits %d\n", pcie->rcv_irq_wlan);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "last wpan interrupt bits %d\n", pcie->rcv_irq_wpan);
}

static int pcie_s6165_filter_set_param_cb(const char *buffer, const struct kernel_param *kp)
{
	int ret;
	u32 value;

	if (!pcie_is_on(pcie))
		return -EFAULT;

	ret = kstrtou32(buffer, 0, &value);
	writel(value, pcie->registers_ramrp);
	value = readl(pcie->registers_ramrp);

	writel(value + 1, pcie->registers_ramrp + 4);
	value = readl(pcie->registers_ramrp + 4);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "CAFE?????? 0x%x\n", value);
	return 0;
}

static struct kernel_param_ops pcie_s6165_filter_ops = {
	.set = pcie_s6165_filter_set_param_cb,
	.get = NULL,
};

module_param_cb(pcie_write_ramp, &pcie_s6165_filter_ops, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(pcie_write_ramp, "Forces a crash of the Bluetooth driver");

static inline void pcie_enable_unmasked_irq(struct pcie_mif *pcie)
{
	int i = 0;

	for (i = 0; i < NUM_TO_HOST_IRQ_PMU; i++) {
		if (!pcie->pmu_msi[i].is_masked)
			enable_irq(pcie->pmu_msi[i].msi_vec);
	}
	for (i = 0; i < NUM_TO_HOST_IRQ_WLAN; i++) {
		if (!pcie->wlan_msi[i].is_masked)
			enable_irq(pcie->wlan_msi[i].msi_vec);
	}
	for (i = 0; i < NUM_TO_HOST_IRQ_WPAN; i++) {
		if (!pcie->wpan_msi[i].is_masked)
			enable_irq(pcie->wpan_msi[i].msi_vec);
	}
}

static void pcie_mask_wlan_wpan_th_irq(struct pcie_mif *pcie)
{
	int i = 0;
	int bit_num = 0;
	u32 val = 0;
	unsigned long flags;

	SCSC_TAG_DEBUG(PCIE_MIF, "mask irq\n");
	spin_lock_irqsave(&pcie->mask_spinlock, flags);
	pci_read_config_dword(pcie->pdev, PCI_MSI_CAP_OFF_10H_REG, &val);
	for (i = 0; i < NUM_TO_HOST_IRQ_WLAN; i++) {
		pcie->wlan_msi[i].is_masked = true;
		bit_num = pcie->wlan_msi[i].msi_bit;
		val |= (1 << bit_num);
	}
	for (i = 0; i < NUM_TO_HOST_IRQ_WPAN; i++) {
		pcie->wpan_msi[i].is_masked = true;
		bit_num = pcie->wpan_msi[i].msi_bit;
		val |= (1 << bit_num);
	}
	pci_write_config_dword(pcie->pdev, PCI_MSI_CAP_OFF_10H_REG, val);
	spin_unlock_irqrestore(&pcie->mask_spinlock, flags);
}

static void pcie_unmask_wlan_wpan_th_irq(struct pcie_mif *pcie)
{
	int i = 0;
	int bit_num = 0;
	u32 val = 0;
	unsigned long flags;

	SCSC_TAG_DEBUG(PCIE_MIF, "unmask irq\n");
	spin_lock_irqsave(&pcie->mask_spinlock, flags);
	pci_read_config_dword(pcie->pdev, PCI_MSI_CAP_OFF_10H_REG, &val);
	for (i = 0; i < NUM_TO_HOST_IRQ_WLAN; i++) {
		pcie->wlan_msi[i].is_masked = false;
		bit_num = pcie->wlan_msi[i].msi_bit;
		val &= ~(1 << bit_num);
	}
	for (i = 0; i < NUM_TO_HOST_IRQ_WPAN; i++) {
		pcie->wpan_msi[i].is_masked = false;
		bit_num = pcie->wpan_msi[i].msi_bit;
		val &= ~(1 << bit_num);
	}
	pci_write_config_dword(pcie->pdev, PCI_MSI_CAP_OFF_10H_REG, val);
	spin_unlock_irqrestore(&pcie->mask_spinlock, flags);
}

static void pcie_mif_irq_default_handler(int irq, void *data)
{
	/* Avoid unused parameter error */
	(void)irq;
	(void)data;
}

static void pcie_set_atu(struct pcie_mif *pcie)
{
	pcie_atu_config_t atu;

	if (!pcie_is_on(pcie))
		return;

	/* Send ATU mappings */
	atu.start_addr_0 = (uint64_t)pcie->mem_start;
	atu.end_addr_0 = (uint64_t)(pcie->mem_start + pcie->mem_sz);

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Copy ATU offset 0x%x start 0x%lx end 0x%lx\n", RAMRP_HOSTIF_PMU_PCIE_CONFIG_PTR,
			  atu.start_addr_0, atu.end_addr_0);

	writel(atu.start_addr_0 & 0xffffffff, pcie->registers_ramrp + RAMRP_HOSTIF_PMU_PCIE_CONFIG_PTR);
	writel(atu.start_addr_0 >> 32, pcie->registers_ramrp + RAMRP_HOSTIF_PMU_PCIE_CONFIG_PTR + 0x4);
	writel(atu.end_addr_0 & 0xffffffff, pcie->registers_ramrp + RAMRP_HOSTIF_PMU_PCIE_CONFIG_PTR + 0x8);
	writel(atu.end_addr_0 >> 32, pcie->registers_ramrp + RAMRP_HOSTIF_PMU_PCIE_CONFIG_PTR + 0xc);

	msleep(1);
}

void pcie_prepare_boot(struct pcie_mif *pcie)
{
	unsigned int ka_addr = 0x0;
	uint32_t *ka_patch_addr;
	uint32_t *ka_patch_addr_end;
	size_t ka_patch_len;
	u32 cmd_dw;

	if (!pcie_is_on(pcie))
		return;

	if (pcie->ka_patch_fw && !fw_compiled_in_kernel) {
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "ka_patch present in FW image\n");
		ka_patch_addr = pcie->ka_patch_fw;
		ka_patch_addr_end = ka_patch_addr + (pcie->ka_patch_len / sizeof(uint32_t));
		ka_patch_len = pcie->ka_patch_len;
	} else {
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "ka_patch not present in FW image. ARRAY_SIZE %d Use default\n",
				  ARRAY_SIZE(ka_patch));
		ka_patch_addr = &ka_patch[0];
		ka_patch_addr_end = ka_patch_addr + ARRAY_SIZE(ka_patch);
		ka_patch_len = ARRAY_SIZE(ka_patch);
	}

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "KA patch download start\n");
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "ka_patch_len: 0x%x\n", ka_patch_len);

	while (ka_patch_addr < ka_patch_addr_end) {
		writel(*ka_patch_addr, pcie->registers_ramrp + ka_addr);
		ka_addr += (unsigned int)sizeof(unsigned int);
		ka_patch_addr++;
	}

	msleep(100);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "KA patch done\n");

	pcie_set_atu(pcie);
	wmb();

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "End CFG\n");
	writel(SECOND_STAGE_BOOT_ADDR, pcie->registers_pert + PMU_SECOND_STAGE_BOOT_ADDR);
	msleep(100);
	cmd_dw = readl(pcie->registers_pert + PMU_SECOND_STAGE_BOOT_ADDR);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "PMU_SECOND_STAGE_BOOT_ADDR 0x%x\n", cmd_dw);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TRIGGER IRQ 0x1: PMU_INTERRUPT_FROM_HOST_INTGR\n");
	writel(0x1, pcie->registers_pert + PMU_INTERRUPT_FROM_HOST_INTGR);
	msleep(100);

	pcie_set_remappers(pcie);

	pcie->boot_state = WLBT_BOOT_CFG_DONE;
}

void pcie_remap_set(struct pcie_mif *pcie, uintptr_t remap_addr, enum scsc_mif_abs_target target)
{
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Setting remapper address %u target %s\n", remap_addr,
			  (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN" : "WLAN");

	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		pcie->remap_addr_wlan = remap_addr + DRAM_BASE_ADDR;
	else if (target == SCSC_MIF_ABS_TARGET_WPAN)
		pcie->remap_addr_wpan = remap_addr + DRAM_BASE_ADDR;
	else
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Incorrect target %d\n", target);
}

void pcie_irq_bit_set(struct pcie_mif *pcie, int bit_num, enum scsc_mif_abs_target target)
{
	u32 wake;
	u32 target_off = WLAN_INTERRUPT_FROM_HOST_PENDING_BIT_ARRAY_SET;

	if (!pcie_is_on(pcie))
		return;

	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Setting INT from host: bit %d subsystem %d\n", bit_num, target);

	if (bit_num >= NUM_TO_HOST_IRQ_TOTAL) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}

	if (target == SCSC_MIF_ABS_TARGET_WPAN) {
		target_off = WPAN_INTERRUPT_FROM_HOST_INTGR;
		wake = PMU_INT_FROM_HOST_WAKE_WPAN_MASK;
	} else {
		target_off = WLAN_INTERRUPT_FROM_HOST_PENDING_BIT_ARRAY_SET;
		wake = PMU_INT_FROM_HOST_WAKE_WLAN_MASK;
	}

	writel(1 << bit_num, pcie->registers_pert + target_off);

#ifdef CONFIG_SCSC_BB_PAEAN
	/* Wake PMU to tell it to wake BT/WLAN */
	if (wake_pmu)
		writel(wake, pcie->registers_pert + PMU_INTERRUPT_FROM_HOST_INTGR);
#endif
}

void pcie_irq_bit_clear(struct pcie_mif *pcie, int bit_num, enum scsc_mif_abs_target target)
{
	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Bit Clear %u subsystem %d\n", bit_num, target);
#if 0 /* not yet */
	if (target == SCSC_MIF_ABS_TARGET_WLAN) {
		if (pcie->wlan_msi[bit_num].is_masked == false) {
			enable_irq(pcie->wlan_msi[bit_num].msi_vec);
			SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Enable IRQ %d\n", pcie->wlan_msi[bit_num].msi_vec);
		}
	} else if (pcie->wpan_msi[bit_num].is_masked == false) {
			enable_irq(pcie->wpan_msi[bit_num].msi_vec);
			SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Enable IRQ %d\n", pcie->wpan_msi[bit_num].msi_vec);
	}

#endif
	return;
}

int pcie_mif_set_affinity_cpu(struct pcie_mif *pcie, u8 msi, u8 cpu)
{
	int irq_vec;
#if defined(CONFIG_SOC_S5E9945)
	int i;

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Change MSI %d affinity to CPU to %d\n", msi, cpu);

	for (i = 0; i < NUM_TO_HOST_IRQ_WLAN; i++) {
		if (msi == pcie->wlan_msi[i].msi_bit) {
			irq_vec = pcie->wlan_msi[i].msi_vec;
			goto found;
		}
	}

	for (i = 0; i < NUM_TO_HOST_IRQ_WPAN; i++) {
		if (msi == pcie->wlan_msi[i].msi_bit) {
			irq_vec = pcie->wlan_msi[i].msi_vec;
			goto found;
		}
	}

	return -EIO;
found:
#else
	/* 0 indicates channel 0 - WLBT pcie channel*/
	irq_vec = exynos_pcie_get_irq_num(0);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev,
			 "Change CPU affinity to %d, line %d\n", cpu, irq_vec);
#endif
	return irq_set_affinity_hint(irq_vec, cpumask_of(cpu));
}

void pcie_irq_bit_mask(struct pcie_mif *pcie, int bit_num, enum scsc_mif_abs_target target)
{
	unsigned long flags;
	unsigned int logic_bit_num;
	u32 val = 0;

	/* bit_num is MSI, so we need to modify to logical */
	spin_lock_irqsave(&pcie->mask_spinlock, flags);
	if (target == SCSC_MIF_ABS_TARGET_WLAN) {
		logic_bit_num = bit_num - pcie->wlan_msi[0].msi_bit;
		if (logic_bit_num >= NUM_TO_HOST_IRQ_WLAN)
			goto end;
		if (pcie->wlan_msi[logic_bit_num].is_masked) {
			SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev,
					   "MSI Bit Mask %u subsystem %d line %d. Already masked ignore request\n",
					   bit_num, target, pcie->wlan_msi[logic_bit_num].msi_vec);
			goto end;
		}
		pcie->wlan_msi[logic_bit_num].is_masked = true;
		SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "MSI Bit Mask %u subsystem %d line %d\n", bit_num, target,
				   pcie->wlan_msi[logic_bit_num].msi_vec);
	} else {
		logic_bit_num = bit_num - pcie->wpan_msi[0].msi_bit;
		if (logic_bit_num >= NUM_TO_HOST_IRQ_WPAN)
			goto end;
		if (pcie->wpan_msi[logic_bit_num].is_masked) {
			SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev,
					   "MSI Bit Mask %u subsystem %d line %d. Already masked ignore request\n",
					   bit_num, target, pcie->wlan_msi[logic_bit_num].msi_vec);
			goto end;
		}
		pcie->wpan_msi[logic_bit_num].is_masked = true;
		SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "MSI Bit Mask %u subsystem %d line %d\n", bit_num, target,
				   pcie->wpan_msi[logic_bit_num].msi_vec);
	}
	/* workaround for missing MSI interrupts, root cause needs investigation and fix */
	pci_read_config_dword(pcie->pdev, PCI_MSI_CAP_OFF_10H_REG, &val);
	pci_write_config_dword(pcie->pdev, PCI_MSI_CAP_OFF_10H_REG, val | (1 << bit_num));
end:
	spin_unlock_irqrestore(&pcie->mask_spinlock, flags);
	return;
}

void pcie_irq_bit_unmask(struct pcie_mif *pcie, int bit_num, enum scsc_mif_abs_target target)
{
	unsigned long flags;
	unsigned int logic_bit_num;
	u32 val = 0;

	/* bit_num is MSI, so we need to modify to logical */
	spin_lock_irqsave(&pcie->mask_spinlock, flags);
	if (target == SCSC_MIF_ABS_TARGET_WLAN) {
		logic_bit_num = bit_num - pcie->wlan_msi[0].msi_bit;
		if (logic_bit_num >= NUM_TO_HOST_IRQ_WLAN)
			goto end;
		if (!pcie->wlan_msi[logic_bit_num].is_masked) {
			SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev,
					   "MSI Bit Mask %u subsystem %d line %d. Already enabled ignore request\n",
					   bit_num, target, pcie->wlan_msi[logic_bit_num].msi_vec);
			goto end;
		}
		pcie->wlan_msi[logic_bit_num].is_masked = false;
		SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "MSI Bit Unmask %u subsystem %d line %d\n", bit_num, target,
				   pcie->wlan_msi[logic_bit_num].msi_vec);
	} else {
		logic_bit_num = bit_num - pcie->wpan_msi[0].msi_bit;
		if (logic_bit_num >= NUM_TO_HOST_IRQ_WPAN)
			goto end;
		if (!pcie->wpan_msi[logic_bit_num].is_masked) {
			SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev,
					   "MSI Bit Mask %u subsystem %d line %d. Already enabled ignore request\n",
					   bit_num, target, pcie->wlan_msi[logic_bit_num].msi_vec);
			goto end;
		}
		pcie->wpan_msi[logic_bit_num].is_masked = false;
		SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "MSI Bit Unmask %u subsystem %d line %d\n", bit_num, target,
				   pcie->wpan_msi[logic_bit_num].msi_vec);
	}
	/* workaround for missing MSI interrupts, root cause needs investigation and fix */
	pci_read_config_dword(pcie->pdev, PCI_MSI_CAP_OFF_10H_REG, &val);
	pci_write_config_dword(pcie->pdev, PCI_MSI_CAP_OFF_10H_REG, val & ~(1 << bit_num));
end:
	spin_unlock_irqrestore(&pcie->mask_spinlock, flags);
	return;
}

int pcie_get_mbox_pmu(struct pcie_mif *pcie)
{
	u32 val = 0;

	val = readl(pcie->registers_ramrp + RAMRP_HOSTIF_PMU_MBOX_TO_HOST_PTR);
	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Read RAMRP_HOSTIF_PMU_MBOX_TO_HOST_PTR: %u\n", val);
	return val;
}

int pcie_set_mbox_pmu(struct pcie_mif *pcie, u32 val)
{
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Write CMD 0x%x at offset 0x%x\n", val,
			  RAMRP_HOSTIF_PMU_MBOX_FROM_HOST_PTR);

	if (!pcie_is_on(pcie))
		return -EFAULT;

#ifdef CONFIG_SCSC_XO_CDAC_CON
	if (val == PMU_AP_MSG_DCXO_CONFIG)
		// 384 is test value. Have to get dynamically later.
		writel(384, pcie->registers_ramrp + RAMRP_HOSTIF_XO_CDAC_CON);
#endif

	writel(val, pcie->registers_ramrp + RAMRP_HOSTIF_PMU_MBOX_FROM_HOST_PTR);

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TRIGGER IRQ : PMU_INTERRUPT_FROM_HOST_INTGR\n");

	writel(0x2, pcie->registers_pert + PMU_INTERRUPT_FROM_HOST_INTGR);

	return 0;
}

int pcie_get_mbox_pmu_pcie_off(struct pcie_mif *pcie)
{
	u32 val = 0;

	val = readl(pcie->registers_ramrp + RAMRP_HOSTIF_PCIE_MBOX_TO_HOST_PTR);
	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Read RAMRP_HOSTIF_PCIE_MBOX_TO_HOST_PTR: %u\n", val);
	return val;
}

int pcie_set_mbox_pmu_pcie_off(struct pcie_mif *pcie, u32 val)
{
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Write CMD %d at offset 0x%x\n", val,
			  RAMRP_HOSTIF_PMU_MBOX_FROM_HOST_PTR);

	if (!pcie_is_on(pcie))
		return -EFAULT;

	writel(val, pcie->registers_ramrp + RAMRP_HOSTIF_PCIE_MBOX_FROM_HOST_PTR);

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TRIGGER IRQ 0x%d: PMU_INTERRUPT_FROM_HOST_INTGR\n",
			  PMU_INT_FROM_HOST_PCIE_MBOX_REQ_MASK);

	writel(PMU_INT_FROM_HOST_PCIE_MBOX_REQ_MASK, pcie->registers_pert + PMU_INTERRUPT_FROM_HOST_INTGR);

	return 0;
}

u32 pcie_get_s2m_size_octets(struct pcie_mif *pcie)
{
	u32 s2m_size_octets, s2m_dram_offset;

	s2m_size_octets = readl(pcie->registers_ramrp + RAMRP_HOSTIF_S2M_SIZE_OCTETS);
	s2m_dram_offset = readl(pcie->registers_ramrp + RAMRP_HOSTIF_S2M_DRAM_OFFSET);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "s2m_size_octets : 0x%x, s2m_dram_offset : 0x%x\n", s2m_size_octets, s2m_dram_offset);
	return readl(pcie->registers_ramrp + RAMRP_HOSTIF_S2M_SIZE_OCTETS);
}

void pcie_set_s2m_dram_offset(struct pcie_mif *pcie, u32 val)
{
	writel(val, pcie->registers_ramrp + RAMRP_HOSTIF_S2M_DRAM_OFFSET);
}

int pcie_get_mbox_pmu_error(struct pcie_mif *pcie)
{
	u32 val = 0;

	val = readl(pcie->registers_ramrp + RAMRP_HOSTIF_ERROR_MBOX_TO_HOST_PTR);
	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Read RAMRP_HOSTIF_ERROR_MBOX_TO_HOST_PTR: %u\n", val);
	return val;
}

int pcie_load_pmu_fw(struct pcie_mif *pcie, u32 *ka_patch, size_t ka_patch_len)
{
	pcie->ka_patch_fw = kzalloc(ka_patch_len, GFP_KERNEL);

	if (!pcie->ka_patch_fw) {
		SCSC_TAG_WARNING_DEV(PCIE_MIF, pcie->dev, "Unable to allocate 0x%x\n", ka_patch_len);
		return -ENOMEM;
	}

	memcpy(pcie->ka_patch_fw, ka_patch, ka_patch_len);
	pcie->ka_patch_len = ka_patch_len;
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "load_pmu_fw sz %u done\n", ka_patch_len);
	return 0;
}
/*
int pcie_load_pmu_fw_flags(struct pcie_mif *pcie, u32 *ka_patch, size_t ka_patch_len, uint32_t flags)
{
	pcie->ka_patch_fw = kzalloc(ka_patch_len, GFP_KERNEL);

	if (!pcie->ka_patch_fw) {
		SCSC_TAG_WARNING_DEV(PCIE_MIF, pcie->dev, "Unable to allocate 0x%x\n", ka_patch_len);
		return -ENOMEM;
	}

	memcpy(pcie->ka_patch_fw, ka_patch, ka_patch_len);
	*(uint *)(pcie->ka_patch_fw + RAMRP_HOSTIF_PMU_BOOTFLAGS) = flags;
	pcie->ka_patch_len = ka_patch_len;

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "load_pmu_fw sz %u done, flags 0x%x\n", ka_patch_len, flags);
	return 0;
}
*/

static void pcie_unload_pmu_fw(struct pcie_mif *pcie)
{
	if (pcie->ka_patch_fw) {
		SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "free ka_patch_fw\n");
		kfree(pcie->ka_patch_fw);
		pcie->ka_patch_fw = NULL;
		pcie->ka_patch_len = 0;
	}
}

int pci_get_mifram_ref(struct pcie_mif *pcie, void *ptr, scsc_mifram_ref *ref)
{
	if (ptr > (pcie->mem + pcie->mem_sz)) {
		SCSC_TAG_ERR(PCIE_MIF, "ooops limits reached\n");
		return -ENOMEM;
	}

	/* Ref is byte offset wrt start of shared memory */
	*ref = (scsc_mifram_ref)((uintptr_t)ptr - (uintptr_t)pcie->mem);

	return 0;
}

void *pcie_get_mifram_ptr(struct pcie_mif *pcie, scsc_mifram_ref ref)
{
	/* Check limits */
	if (ref >= 0 && ref < pcie->mem_sz)
		return (void *)((uintptr_t)pcie->mem + (uintptr_t)ref);
	else
		return NULL;
}

void *pcie_get_mifram_phy_ptr(struct pcie_mif *pcie, scsc_mifram_ref ref)
{
	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "\n");

	if (!pcie->mem_start) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Memory unmmaped\n");
		return NULL;
	}

	return (void *)((uintptr_t)pcie->mem_start + (uintptr_t)ref);
}

uintptr_t pci_get_mif_pfn(struct pcie_mif *pcie)
{
	return vmalloc_to_pfn(pcie->mem);
}

#define SCSC_STATIC_MIFRAM_PAGE_TABLE
#define MAX_RSV_MEM 16 * 1024 * 1204

static void __iomem *pcie_mif_map_region(unsigned long phys_addr, size_t size)
{
	size_t i;
	struct page **pages;
	void *vmem;

	size = PAGE_ALIGN(size);
#ifndef SCSC_STATIC_MIFRAM_PAGE_TABLE
	pages = kmalloc((size >> PAGE_SHIFT) * sizeof(*pages), GFP_KERNEL);
	if (!pages) {
		SCSC_TAG_ERR(PCIE_MIF, "wlbt: kmalloc of %zd byte pages table failed\n",
			     (size >> PAGE_SHIFT) * sizeof(*pages));
		return NULL;
	}
#else
	/* Reserve the table statically, but make sure .dts doesn't exceed it */
	{
		static struct page *mif_map_pages[MAX_RSV_MEM >> PAGE_SHIFT];

		pages = mif_map_pages;

		SCSC_TAG_INFO(PLAT_MIF, "static mif_map_pages size %zd\n", sizeof(mif_map_pages));

		if (size > MAX_RSV_MEM) { /* Size passed in from .dts exceeds array */
			SCSC_TAG_ERR(PCIE_MIF, "wlbt: shared DRAM requested in .dts %zd exceeds mapping table %d\n",
				     size, MAX_RSV_MEM);
			return NULL;
		}

		SCSC_TAG_INFO(PCIE_MIF, "static mif_map_pages size %zd\n", sizeof(mif_map_pages));
	}
#endif

	/* Map NORMAL_NC pages with kernel virtual space */
	for (i = 0; i < (size >> PAGE_SHIFT); i++) {
		pages[i] = phys_to_page(phys_addr);
		phys_addr += PAGE_SIZE;
	}

	vmem = vmap(pages, size >> PAGE_SHIFT, VM_MAP, pgprot_writecombine(PAGE_KERNEL));

#ifndef SCSC_STATIC_MIFRAM_PAGE_TABLE
	kfree(pages);
#endif
	if (!vmem)
		SCSC_TAG_ERR(PCIE_MIF, "wlbt: vmap of %zd pages failed\n", (size >> PAGE_SHIFT));
	return (void __iomem *)vmem;
}

void pcie_get_msi_range(struct pcie_mif *pcie, u8 *start, u8 *end, enum scsc_mif_abs_target target)
{
	if (target == SCSC_MIF_ABS_TARGET_WLAN) {
		*start = NUM_TO_HOST_IRQ_WLAN_START;
		*end = NUM_TO_HOST_IRQ_WLAN_END;
	} else {
		*start = NUM_TO_HOST_IRQ_WPAN_START;
		*end = NUM_TO_HOST_IRQ_WPAN_END;
	}
}

void *pcie_map(struct pcie_mif *pcie, size_t *allocated)
{
	if (allocated)
		*allocated = 0;

	pcie->mem = pcie_mif_map_region(pcie->mem_start, pcie->mem_sz);

	if (!pcie->mem) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Error remaping shared memory\n");
		return NULL;
	}

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Map: virt %p phys %lx\n", pcie->mem, (uintptr_t)pcie->mem_start);

	if (allocated)
		*allocated = pcie->mem_sz;
	return pcie->mem;
}

static void pcie_mif_unmap_region(void *vmem)
{
	vunmap(vmem);
}

void pcie_unmap(struct pcie_mif *pcie)
{
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Unmap: virt %p phys %lx\n", pcie->mem, (uintptr_t)pcie->mem_start);

	pcie_mif_unmap_region(pcie->mem);
	pcie->mem = NULL;
}
/* Functions for proc entry */
void *pcie_mif_get_mem(struct pcie_mif *pcie)
{
	/* SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "READ %x\n", pcie->mem); */
	return pcie->mem;
}

static void wlbt_pcie_linkdown_cb(struct exynos_pcie_notify *noti)
{
	struct pci_dev *pdev = (struct pci_dev *)noti->user;
	struct pcie_mif *pcie = pci_get_drvdata(pdev);

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "wlbt Link-Down notification callback function!!!\n");
	pcie->is_on = false;

	if (pcie->scan2mem_mode)
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "wlbt try to get scan2mem dump. Ignore it.\n");
	else if (enable_pcie_cb_panic)
		dbg_snapshot_do_dpm_policy(GO_S2D_ID);
}

static void wlbt_pcie_cpl_timeout_cb(struct exynos_pcie_notify *noti)
{
	struct pci_dev *pdev = (struct pci_dev *)noti->user;
	struct pcie_mif *pcie = pci_get_drvdata(pdev);

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "wlbt CPL-TIMEOUT notification callback function!!!\n");
	pcie->is_on = false;

	if (enable_pcie_cb_panic)
		dbg_snapshot_do_dpm_policy(GO_S2D_ID);
}

irqreturn_t pcie_mif_pmu_isr(int irq, void *data)
{
	struct pcie_mif *pcie = (struct pcie_mif *)data;

	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "INT received, boot_state = %u\n", pcie->boot_state);

	/* pcie->boot_state = WLBT_BOOT_CFG_DONE; */
	if (pcie->pmu_handler != pcie_mif_irq_default_handler)
		pcie->pmu_handler(irq, pcie->irq_dev_pmu);
	else
		SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "MIF PMU Interrupt Handler not registered\n");

	return IRQ_HANDLED;
}

irqreturn_t pcie_mif_pmu_pcie_off_isr(int irq, void *data)
{
	struct pcie_mif *pcie = (struct pcie_mif *)data;

	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "PCIE_OFF INT %d received, boot_state = %u\n", irq, pcie->boot_state);

	if (pcie->pmu_pcie_off_handler != pcie_mif_irq_default_handler)
		pcie->pmu_pcie_off_handler(irq, pcie->irq_dev_pmu);
	else
		SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "MIF PMU PCIE_OFF Interrupt Handler not registered\n");

	return IRQ_HANDLED;
}

irqreturn_t pcie_mif_pmu_error_isr(int irq, void *data)
{
	struct pcie_mif *pcie = (struct pcie_mif *)data;

	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "PCIE_ERROR INT %d received, boot_state = %u\n", irq, pcie->boot_state);

	if (pcie->pmu_error_handler != pcie_mif_irq_default_handler)
		pcie->pmu_error_handler(irq, pcie->irq_dev_pmu);
	else
		SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "MIF PMU PCIE_ERROR Interrupt Handler not registered\n");

	return IRQ_HANDLED;
}

irqreturn_t pcie_mif_isr_wlan(int irq, void *data)
{
	struct pcie_mif *pcie = (struct pcie_mif *)data;
	unsigned long flags;

	spin_lock_irqsave(&pcie->mif_spinlock, flags);

	/* get MSI bit correspin to line */
	pcie->rcv_irq_wlan = irq - pcie->wlan_msi[0].msi_vec;


	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "IRQ received line %d msi_bit %u cpu %d\n", irq,
			   pcie->rcv_irq_wlan, smp_processor_id());
#if 0 /* not yet */
	/* We need to disable this IRQ line based on MBOX emulation */
	disable_irq_nosync(irq);
#endif

	if (pcie->wlan_handler != pcie_mif_irq_default_handler) {
		pcie->wlan_handler(irq, pcie->irq_dev_wlan);
	} else
		SCSC_TAG_ERR_DEV(PLAT_MIF, pcie->dev, "MIF Interrupt Handler not registered.\n");

	spin_unlock_irqrestore(&pcie->mif_spinlock, flags);
	return IRQ_HANDLED;
}

irqreturn_t pcie_mif_isr_wpan(int irq, void *data)
{
	struct pcie_mif *pcie = (struct pcie_mif *)data;
	unsigned long flags;

	spin_lock_irqsave(&pcie->mif_spinlock, flags);

	/* get MSI bit correspin to line */
	pcie->rcv_irq_wpan = NUM_TO_HOST_IRQ_WLAN + (irq - pcie->wpan_msi[0].msi_vec);

	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "IRQ received line %d msi_bit %u cpu %d\n", irq,
			   pcie->rcv_irq_wpan, smp_processor_id());
#if 0 /* not yet */
	/* We need to disable this IRQ line based on MBOX emulation */
	disable_irq_nosync(irq);
#endif

	if (pcie->wpan_handler != pcie_mif_irq_default_handler)
		pcie->wpan_handler(irq, pcie->irq_dev_wpan);
	else
		SCSC_TAG_ERR_DEV(PLAT_MIF, pcie->dev, "MIF Interrupt Handler not registered.\n");

	spin_unlock_irqrestore(&pcie->mif_spinlock, flags);
	return IRQ_HANDLED;
}

u32 pcie_irq_get(struct pcie_mif *pcie, enum scsc_mif_abs_target target)
{
	u32 val = 0;
	u32 bit;

	if (target == SCSC_MIF_ABS_TARGET_WLAN) {
		bit = pcie->rcv_irq_wlan;
	} else {
		bit = pcie->rcv_irq_wpan;
	}

	val = 1 << bit;

	/* Function has to return the interrupts that are enabled *AND* not masked */
	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Bit %u get val 0x%x\n", bit, val);
	return val;
}

__iomem void *pcie_get_ramrp_ptr(struct pcie_mif *pcie)
{
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "base 0x%lx\n", (uintptr_t)pcie->registers_ramrp);
	return pcie->registers_ramrp;
}

int pcie_get_ramrp_buff(struct pcie_mif *pcie, void** buff, int count, u64 offset)
{
	int i = 0;
	if (!pcie_is_on(pcie))
		return -EPERM;

	/* Adjust the 'count' */
	if((offset + count) > MAX_RAMRP_SZ)
		count = MAX_RAMRP_SZ - offset;

	if (count % 4)
		count = ((count/4) + 1) * 4;

	if ((offset % 4) && (offset > 4))
		offset = ((offset/4) - 1) * 4;
	else if (offset <= 3)
		offset = 0;

	if (pcie->registers_ramrp == NULL)
		return -EPERM;

	if (*buff == NULL) {
		*buff = kzalloc(MAX_RAMRP_SZ_ALIGNED, GFP_KERNEL);
		if (*buff == NULL)
			return -ENOMEM;
		else
			ramrp_buff = *buff;
	}

	for (i = 0; i < (count / 4); i++) {
		((u32 *)*buff)[i] = readl((pcie->registers_ramrp) + (4 * i) + (offset));
		udelay(10);
	}
	return count;
}

static int pcie_mif_enable_msi(struct pcie_mif *pcie)
{
	struct msi_desc *msi_desc;
	int num_vectors;
	int ret;
	u32 acc = 0, i;

	num_vectors = pci_alloc_irq_vectors(pcie->pdev, NUM_TO_HOST_IRQ_MAX_CAP, NUM_TO_HOST_IRQ_MAX_CAP, PCI_IRQ_MSI);

	if (num_vectors != NUM_TO_HOST_IRQ_MAX_CAP) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "failed to get %d MSI vectors, only %d available\n",
				 NUM_TO_HOST_IRQ_MAX_CAP, num_vectors);

		if (num_vectors >= 0)
			return -EINVAL;
		else
			return num_vectors;
	}

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "vectors %d\n", num_vectors);

	for (i = 0; i < NUM_TO_HOST_IRQ_WLAN; i++, acc++) {
		pcie->wlan_msi[i].msi_vec = pci_irq_vector(pcie->pdev, acc);
		pcie->wlan_msi[i].msi_bit = acc;
		pcie->wlan_msi[i].is_masked = false;
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Setting MSI %d interrupt vector line %d for WLAN\n", acc,
				 pcie->wlan_msi[i].msi_vec);
		ret = devm_request_irq(&pcie->pdev->dev, pcie->wlan_msi[i].msi_vec, pcie_mif_isr_wlan, 0, DRV_NAME, pcie);
		if (ret) {
			SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Failed to register MSI handler. Aborting.\n");
			return -EINVAL;
		}
	}

	for (i = 0; i < NUM_TO_HOST_IRQ_WPAN; i++, acc++) {
		pcie->wpan_msi[i].msi_vec = pci_irq_vector(pcie->pdev, acc);
		pcie->wpan_msi[i].msi_bit = acc;
		pcie->wpan_msi[i].is_masked = false;
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Setting MSI %d interrupt vector line %d for WPAN\n", acc,
				 pcie->wpan_msi[i].msi_vec);
		ret = devm_request_irq(&pcie->pdev->dev, pcie->wpan_msi[i].msi_vec, pcie_mif_isr_wpan, 0, DRV_NAME, pcie);

		if (ret) {
			SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Failed to register MSI handler. Aborting.\n");
			return -EINVAL;
		}
	}

	for (i = 0; i < NUM_TO_HOST_IRQ_PMU; i++, acc++) {
		pcie->pmu_msi[i].msi_vec = pci_irq_vector(pcie->pdev, acc);
		pcie->pmu_msi[i].msi_bit = acc;
		pcie->pmu_msi[i].is_masked = false;
		if (acc == PMU_MBOX_PCIE) {
			SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Setting MSI %d interrupt vector line %d for PMU\n", acc,
					 pcie->pmu_msi[i].msi_vec);
			ret = devm_request_irq(&pcie->pdev->dev, pcie->pmu_msi[i].msi_vec, pcie_mif_pmu_pcie_off_isr, IRQF_TRIGGER_NONE, "PMU_MBOX_PCIE", pcie);
		} else if (acc == PMU_MBOX_ERROR) {
			SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Setting MSI %d interrupt vector line %d for PMU\n", acc,
					 pcie->pmu_msi[i].msi_vec);
			ret = devm_request_irq(&pcie->pdev->dev, pcie->pmu_msi[i].msi_vec, pcie_mif_pmu_error_isr, IRQF_TRIGGER_NONE, "PMU_MBOX_ERROR", pcie);
		} else {
			SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Setting MSI %d interrupt vector line %d for PMU\n", acc,
					 pcie->pmu_msi[i].msi_vec);
			ret = devm_request_irq(&pcie->pdev->dev, pcie->pmu_msi[i].msi_vec, pcie_mif_pmu_isr, IRQF_TRIGGER_NONE, "PMU_MBOX_PMU", pcie);
		}
		if (ret) {
			SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Failed to register MSI handler. Aborting.\n");
			return -EINVAL;
		}
	}

	msi_desc = irq_get_msi_desc(pcie->pdev->irq);
	if (!msi_desc) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "msi_desc is NULL\n");
		ret = -EINVAL;
		goto free_msi_vector;
	}

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "msi base data is %d\n", msi_desc->msg.data);

	return 0;

free_msi_vector:
	pci_free_irq_vectors(pcie->pdev);

	return ret;
}

static bool pcie_mif_check_handler(struct irq_desc *desc)
{
	return desc && desc->action && (desc->action->handler == pcie_mif_isr_wlan
		|| desc->action->handler == pcie_mif_isr_wpan || desc->action->handler == pcie_mif_pmu_pcie_off_isr
		|| desc->action->handler == pcie_mif_pmu_error_isr || desc->action->handler == pcie_mif_pmu_isr);
}

static void pcie_mif_disable_msi(struct pcie_mif *pcie)
{
	struct pci_dev *pci_dev = pcie->pdev;
	int irq_vec;
	int i = 0;

	while (i < NUM_TO_HOST_IRQ_MAX_CAP) {
		if(pcie_mif_check_handler(irq_to_desc(pci_dev->irq+i))){
			SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "free irq : %d\n", pci_dev->irq + i);
			/* clear affinity mask */
			irq_set_affinity_hint(pci_dev->irq + i, NULL);
			devm_free_irq(&pci_dev->dev, pci_dev->irq + i, pcie);
		}
		else{
			SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "failed to free irq : %d, it is not registered\n", pci_dev->irq + i);
		}
		++i;
	}
	/* 0 indicates channel 0 - WLBT pcie channel*/
	irq_vec = exynos_pcie_get_irq_num(0);
	irq_set_affinity_hint(irq_vec, NULL);
	pci_free_irq_vectors(pci_dev);
}

void __iomem *pcie_mif_map_bar(struct pci_dev *pdev, u8 index)
{
	void __iomem *vaddr;
#if 0
	struct resource *temp;
	temp = &pdev->resource[index];

	vaddr = devm_ioremap_resource(&pdev->dev, temp);
#else
	dma_addr_t busaddr;
	size_t len;
	int ret;

	ret = pcim_iomap_regions(pdev, 1 << index, DRV_NAME);
	if (ret)
		return IOMEM_ERR_PTR(ret);

	busaddr = pci_resource_start(pdev, index);
	len = pci_resource_len(pdev, index);
	vaddr = pcim_iomap_table(pdev)[index];
	if (!vaddr)
		return IOMEM_ERR_PTR(-ENOMEM);

	SCSC_TAG_ERR_DEV(PCIE_MIF, &pdev->dev, "BAR%u vaddr=0x%p busaddr=%pad len=%u\n", index, vaddr, &busaddr,
			 (int)len);
#endif
	return vaddr;
}

extern int exynos_pcie_register_event(struct exynos_pcie_register_event *reg);
extern int exynos_pcie_deregister_event(struct exynos_pcie_register_event *reg);
static int pcie_mif_module_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int rc = 0;
	int ret = 0;
	u16 cmd;
#if 0
	u16 val16;
	u32 val;
#endif
	/* Update state */
	pcie->pdev = pdev;
	pcie->dev = &pdev->dev;

	/* Initialize spinlock */
	spin_lock_init(&pcie->mif_spinlock);
	spin_lock_init(&pcie->mask_spinlock);

	if (pcie->pci_saved_configs) {
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "wlbt pcie saved configs is not NULL(restart)\n");
		pci_load_saved_state(pdev, pcie->pci_saved_configs);
		pci_restore_state(pdev);
	}

	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Register WLBT PCIE notification LINKDOWN event...\n");
	pcie->pcie_link_down_event.events = EXYNOS_PCIE_EVENT_LINKDOWN;
	pcie->pcie_link_down_event.user = pdev;
	pcie->pcie_link_down_event.mode = EXYNOS_PCIE_TRIGGER_CALLBACK;
	pcie->pcie_link_down_event.callback = wlbt_pcie_linkdown_cb;
	if(exynos_pcie_register_event(&pcie->pcie_link_down_event)) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Register event FAILED!!\n");
	}

	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Register WLBT PCIE notification CPLTIMEOUT event...\n");
	pcie->pcie_cpl_timeout_event.events = EXYNOS_PCIE_EVENT_CPL_TIMEOUT;
	pcie->pcie_cpl_timeout_event.user = pdev;
	pcie->pcie_cpl_timeout_event.mode = EXYNOS_PCIE_TRIGGER_CALLBACK;
	pcie->pcie_cpl_timeout_event.callback = wlbt_pcie_cpl_timeout_cb;
	if(exynos_pcie_register_event(&pcie->pcie_cpl_timeout_event)) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Register CPL TIMEOUT event FAILED!!\n");
	}

	rc = pcim_enable_device(pdev);
	if (rc) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Error enabling device.\n");
		return rc;
	}
	/* This function returns the flags associated with this resource.*/
	/* esource flags are used to define some features of the individual resource.
	 *      For PCI resources associated with PCI I/O regions, the information is extracted from the base address registers */
	/* IORESOURCE_MEM = If the associated I/O region exists, one and only one of these flags is set */
	if (!(pci_resource_flags(pdev, 0) & IORESOURCE_MEM)) {
		SCSC_TAG_ERR(PCIE_MIF, "Incorrect BAR configuration\n");
		return -EIO;
	}
	pci_set_master(pdev);
	pci_set_drvdata(pdev, pcie);
	ret = pcie_mif_enable_msi(pcie);
	if(ret){
		SCSC_TAG_ERR(PCIE_MIF, "failed to set msi\n");
		return ret;
	}

	/* Access iomap allocation table */
	/* return __iomem * const * */
	pcie->registers_pert = pcie_mif_map_bar(pdev, 0);
	SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "BAR0/1 registers %x\n", pcie->registers_pert);
	if(IS_ERR(pcie->registers_pert)){
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "failed to set BAR0/1 registers\n");
		return -EIO;
	}
	pcie->registers_ramrp = pcie_mif_map_bar(pdev, 2);
	SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "BAR2/3 registers %x\n", pcie->registers_ramrp);
	if(IS_ERR(pcie->registers_ramrp)){
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "failed to set BAR2/3 registers\n");
		return -EIO;
	}
	pcie->registers_msix = pcie_mif_map_bar(pdev, 4);
	SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "BAR4/5 registers %x\n", pcie->registers_msix);
	if(IS_ERR(pcie->registers_msix)){
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "failed to set BAR4/5 registers\n");
		return -EIO;
	}

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "PCI read configuration\n");

	pci_read_config_word(pdev, PCI_COMMAND, &cmd);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "PCIE PCI_COMMAND 0x%x\n", cmd);
	pci_read_config_word(pdev, PCI_VENDOR_ID, &cmd);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "PCIE PCI_VENDOR_ID 0x%x\n", cmd);
	pci_read_config_word(pdev, PCI_DEVICE_ID, &cmd);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "PCIE PCI_DEVICE_ID 0x%x\n", cmd);
	pcie->mem_allocated = 0;

	pcie->initialized = true;
	return 0;
}

static void pcie_mif_module_remove(struct pci_dev *pcie_dev)
{
	pcie->initialized = false;

	pcie_unload_pmu_fw(pcie);
	pcie_mif_disable_msi(pcie);
	pci_release_regions(pcie_dev);
	exynos_pcie_deregister_event(&pcie->pcie_link_down_event);
	exynos_pcie_deregister_event(&pcie->pcie_cpl_timeout_event);
}

static const struct pci_device_id pcie_mif_module_tbl[] = { {
								    PCI_VENDOR_ID_SAMSUNG,
								    PCI_DEVICE_ID_WLBT,
								    PCI_ANY_ID,
								    PCI_ANY_ID,
							    },
							    { /*End: all zeroes */ } };

MODULE_DEVICE_TABLE(pci, pcie_mif_module_tbl);

static struct pci_driver scsc_pcie = {
	.name = "scsc_pcie",
	.id_table = pcie_mif_module_tbl,
	.probe = pcie_mif_module_probe,
	.remove = pcie_mif_module_remove,
};

void pcie_irq_reg_pmu_handler(struct pcie_mif *pcie, void (*handler)(int irq, void *data), void *dev)
{
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Registering mif pmu int handler %pS\n", handler);
	pcie->pmu_handler = handler;
	pcie->irq_dev_pmu = dev;
}

void pcie_irq_reg_pmu_pcie_off_handler(struct pcie_mif *pcie, void (*handler)(int irq, void *data), void *dev)
{
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Registering mif pmu pcie_off int handler %pS\n", handler);
	pcie->pmu_pcie_off_handler = handler;
	pcie->irq_dev_pmu = dev;
}

void pcie_irq_reg_pmu_error_handler(struct pcie_mif *pcie, void (*handler)(int irq, void *data), void *dev)
{
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Registering mif pmu pcie_error int handler %pS\n", handler);
	pcie->pmu_error_handler = handler;
	pcie->irq_dev_pmu = dev;
}

void pcie_irq_reg_wlan_handler(struct pcie_mif *pcie, void (*handler)(int irq, void *data), void *dev)
{
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Registering mif wlan int handler %pS\n", handler);
	pcie->wlan_handler = handler;
	pcie->irq_dev_wlan = dev;
}

void pcie_irq_reg_wpan_handler(struct pcie_mif *pcie, void (*handler)(int irq, void *data), void *dev)
{
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Registering mif wpan int handler %pS\n", handler);
	pcie->wpan_handler = handler;
	pcie->irq_dev_wpan = dev;
}

bool pcie_is_initialized(void)
{
	return pcie->initialized;
}

void pcie_set_ch_num(struct pcie_mif *pcie, int ch_num)
{
	pcie->ch_num = ch_num;
}

void pcie_set_mem_range(struct pcie_mif *pcie, unsigned long mem_start, size_t mem_sz)
{
	pcie->mem_start = mem_start;
	pcie->mem_sz = mem_sz;
}

void pcie_register_driver(void)
{
	int ret;
	ret = pci_register_driver(&scsc_pcie);
	if(ret){
		SCSC_TAG_ERR(PCIE_MIF, "wlbt pcie driver register failed\n");
	}
}

void pcie_unregister_driver(void)
{
	pci_unregister_driver(&scsc_pcie);
}

void pcie_init(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	wake_lock_init(NULL, &(pcie->pcie_link_wakelock.ws), "pcie_link_wakelock");
#else
	wake_lock_init(&pcie->pcie_link_wakelock, WAKE_LOCK_SUSPEND, "pcie_link_wakelock");
#endif
}

void pcie_deinit(void)
{
	wake_lock_destroy(&pcie->pcie_link_wakelock);
	if (ramrp_buff)
		kfree(ramrp_buff);
}

struct pcie_mif *pcie_get_instance(void)
{
	/* return singleton refererence */
	return pcie;
}
EXPORT_SYMBOL(pcie_get_instance);

static int pcie_mif_check_link_status(int ch_num)
{
	return exynos_pcie_rc_chk_link_status(ch_num);
}

static void set_pcie_on(bool on)
{
	unsigned long flags;

	write_lock_irqsave(&pcie_on_lock, flags);
	pcie->is_on = on;
	write_unlock_irqrestore(&pcie_on_lock, flags);
}

bool pcie_is_on(struct pcie_mif *pcie)
{
	bool ret;
	unsigned long flags;

	read_lock_irqsave(&pcie_on_lock, flags);
	ret = pcie->is_on;
	read_unlock_irqrestore(&pcie_on_lock, flags);

	return ret;
}
EXPORT_SYMBOL(pcie_is_on);

/* It's for external use of l1ss ctrl */
void pcie_mif_l1ss_ctrl(struct pcie_mif *pcie, int enable)
{
	exynos_pcie_l1ss_ctrl(enable, PCIE_L1SS_CTRL_WIFI, pcie->ch_num);
}

static void pcie_mif_save_state(struct pci_dev *pdev, int ch_num)
{
	struct pcie_mif *pcie = pci_get_drvdata(pdev);

	SCSC_TAG_DEBUG(PCIE_MIF, "\n");

	if (pcie_mif_check_link_status(ch_num) == 0) {
		SCSC_TAG_ERR(PCIE_MIF, "It's not Linked - Ignore save state!!!\n");
		return;
	}

	/* pci_pme_active(pdev, 0); */

	/* Disable L1.2 before PCIe power off */
	exynos_pcie_l1ss_ctrl(0, PCIE_L1SS_CTRL_WIFI, 0);

	pci_clear_master(pdev);

	pci_save_state(pdev);

	pcie->pci_saved_configs = pci_store_saved_state(pdev);

	/* pcie_chk_ep_conf(pdev); */

	/* disable_msi_int(pdev); */

	/* pci_enable_wake(pdev, PCI_D0, 0); */

	/* pci_disable_device(pdev); */

	pci_wake_from_d3(pdev, false);
	if (pci_set_power_state(pdev, PCI_D3hot))
		SCSC_TAG_ERR(PCIE_MIF, "Can't set D3 state!\n");
}

static void pcie_mif_restore_state(struct pci_dev *pdev, int ch_num)
{
	struct pcie_mif *pcie = pci_get_drvdata(pdev);
	int ret;

	SCSC_TAG_DEBUG(PCIE_MIF, "\n");

	if (pcie_mif_check_link_status(ch_num) == 0) {
		SCSC_TAG_ERR(PCIE_MIF, "It's not Linked - Ignore restore state!!!\n");
		return;
	}

	if (pci_set_power_state(pdev, PCI_D0))
		SCSC_TAG_ERR(PCIE_MIF, "Can't set D0 state!\n");

	if (!pcie->pci_saved_configs)
		SCSC_TAG_ERR(PCIE_MIF, "wlbt pcie saved configs is NULL\n");

	pci_load_saved_state(pdev, pcie->pci_saved_configs);
	pci_restore_state(pdev);

	/* move chk_ep_conf function after setting BME(Bus Master Enable)
	 * pcie_chk_ep_conf(pdev);
	 */

	pci_enable_wake(pdev, PCI_D0, false);
	/* pci_enable_wake(pdev, PCI_D3hot, 0); */

	ret = pcim_enable_device(pdev);

	if (ret)
		SCSC_TAG_ERR(PCIE_MIF, "Can't enable PCIe Device after linkup!\n");
	pci_set_master(pdev);

	pci_set_drvdata(pdev, pcie);
	/* DBG: print out EP config values after restore_state
	 * pcie_chk_ep_conf(pdev);
	 */

	/* Enable L1.2 after PCIe power on */
	exynos_pcie_l1ss_ctrl(1, PCIE_L1SS_CTRL_WIFI, 0);

	/* pci_pme_active(pdev, 1); */
}

bool pcie_mif_get_scan2mem_mode(struct pcie_mif *pcie)
{
	return pcie->scan2mem_mode;
}
EXPORT_SYMBOL(pcie_mif_get_scan2mem_mode);

void pcie_mif_set_scan2mem_mode(struct pcie_mif *pcie, bool enable)
{
	pcie->scan2mem_mode = enable;
}
EXPORT_SYMBOL(pcie_mif_set_scan2mem_mode);

/* Entering L0 state through link up
 * If the previous state was L2.0, it restores configurations to enter L0 state
 */
int pcie_mif_poweron(struct pcie_mif *pcie)
{
	int ret;
	SCSC_TAG_DEBUG(PCIE_MIF, "start\n");

	mutex_lock(&pci_link_lock);
	wake_lock(&pcie->pcie_link_wakelock);
	ret = exynos_pcie_rc_poweron(pcie->ch_num);
	if (ret) {
		set_pcie_on(false);
		wake_unlock(&pcie->pcie_link_wakelock);
		mutex_unlock(&pci_link_lock);
		SCSC_TAG_ERR(PCIE_MIF, "Error poweron PCIe, ret = %d\n", ret);
		return ret;
	}

	set_pcie_on(true);

	if (pcie_is_initialized()){
		SCSC_TAG_DEBUG(PCIE_MIF, "trying to restore state\n");
		pcie_mif_restore_state(pcie->pdev, pcie->ch_num);
		/* unmask the interrupts, so driver can process them when fired */
		pcie_unmask_wlan_wpan_th_irq(pcie);
	}
	mutex_unlock(&pci_link_lock);
	SCSC_TAG_DEBUG(PCIE_MIF, "end\n");
	return 0;
}
EXPORT_SYMBOL(pcie_mif_poweron);

/* Saving configurations and entering L2.0 state */
void pcie_mif_poweroff(struct pcie_mif *pcie)
{
	SCSC_TAG_DEBUG(PCIE_MIF, "start\n");

	mutex_lock(&pci_link_lock);

	if(!pcie_is_on(pcie)){
		SCSC_TAG_ERR(PCIE_MIF, "PCIe is not on, reject poweroff\n");
		mutex_unlock(&pci_link_lock);
		return;
	}

	/* mask the interrupts */
	if(pcie->pdev)
		pcie_mask_wlan_wpan_th_irq(pcie);

	set_pcie_on(false);

	if(pcie_is_initialized() && pcie->pdev)
		pcie_mif_save_state(pcie->pdev, pcie->ch_num);

	exynos_pcie_rc_poweroff(pcie->ch_num);
	wake_unlock(&pcie->pcie_link_wakelock);
	mutex_unlock(&pci_link_lock);
	SCSC_TAG_DEBUG(PCIE_MIF, "end\n");
}
EXPORT_SYMBOL(pcie_mif_poweroff);
