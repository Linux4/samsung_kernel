/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __PCIE_H
#define __PCIE_H
#include "scsc_mif_abs.h"
#include <linux/exynos-pci-noti.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/resource.h>
#include "../pmu_host_if.h"
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <scsc/scsc_wakelock.h>
#else
#include <linux/wakelock.h>
#endif
#include <scsc/scsc_logring.h>

#define PCIE_MIF_PREALLOC_MEM (16 * 1024 * 1024)
#define PCI_MSI_CAP_OFF_10H_REG 0x60

static DEFINE_RWLOCK(pcie_on_lock);

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(6, 1, 0))
extern int exynos_pcie_rc_poweron(int ch_num);
extern int exynos_pcie_rc_poweroff(int ch_num);
extern int exynos_pcie_rc_chk_link_status(int ch_num);
extern int exynos_pcie_l1ss_ctrl(int enable, int id, int ch_num);
extern int exynos_pcie_get_irq_num(int ch_num);
#endif
extern void exynos_pcie_rc_print_msi_register(int ch_num);
#define NUM_TO_HOST_IRQ_WLAN 23
#define NUM_TO_HOST_IRQ_WPAN 6
#define NUM_TO_HOST_IRQ_PMU 3

#define NUM_TO_HOST_IRQ_WLAN_START 0
#define NUM_TO_HOST_IRQ_WLAN_END ((NUM_TO_HOST_IRQ_WLAN_START + NUM_TO_HOST_IRQ_WLAN) - 1) //22

#define NUM_TO_HOST_IRQ_WPAN_START (NUM_TO_HOST_IRQ_WLAN_END + 1) 			   //23
#define NUM_TO_HOST_IRQ_WPAN_END ((NUM_TO_HOST_IRQ_WPAN_START + NUM_TO_HOST_IRQ_WPAN) - 1) //28

#define NUM_TO_HOST_IRQ_PMU_START (NUM_TO_HOST_IRQ_WPAN_END + 1) 			   //29
#define NUM_TO_HOST_IRQ_PMU_END ((NUM_TO_HOST_IRQ_PMU_START + NUM_TO_HOST_IRQ_PMU) - 1)	   //31

#define NUM_TO_HOST_IRQ_TOTAL (NUM_TO_HOST_IRQ_PMU + NUM_TO_HOST_IRQ_WLAN + NUM_TO_HOST_IRQ_WPAN)
#define NUM_TO_HOST_IRQ_MAX_CAP 32

#define CEILING(x,y) (((x) + (y) - 1) / (y))
#define MAX_RAMRP_SZ			33 * 1024
#define MAX_RAMRP_SZ_ALIGNED	((CEILING(MAX_RAMRP_SZ, PAGE_SIZE)) * (PAGE_SIZE))	/* Maximum memory for ramrp, note that it has to be page aligned*/

/* Types */
struct msi_emul {
	u8 msi_bit;
	u32 msi_vec;
	bool is_masked;
};

struct pcie_mif {
	struct pci_dev *pdev;
	int dma_using_dac; /* =1 if 64-bit DMA is used, =0 otherwise. */
	__iomem void *registers_pert;
	__iomem void *registers_ramrp;
	__iomem void *registers_msix;

	struct device *dev;

	bool is_on;
	bool initialized;

	void *mem; /* DMA memory mapped to PCIe space for MX-AP comms */
	size_t mem_allocated;
	dma_addr_t dma_addr;
	unsigned long mem_start;
	size_t mem_sz;

	/* spinlock to serialize driver access */
	spinlock_t mif_spinlock;
	spinlock_t mask_spinlock;
	/* Reset Request handler and context */
	void (*reset_request_handler)(int irq_num_ignored, void *data);
	void *reset_request_handler_data;

	enum wlbt_boot_state {
		WLBT_BOOT_IN_RESET = 0,
		WLBT_BOOT_WAIT_CFG_REQ,
		WLBT_BOOT_ACK_CFG_REQ,
		WLBT_BOOT_CFG_DONE,
		WLBT_BOOT_CFG_ERROR
	} boot_state;

	int *ka_patch_fw;
	size_t ka_patch_len;
	/* Callback function and dev pointer mif_intr manager handler */
	void (*pmu_handler)(int irq, void *data);
	void (*pmu_pcie_off_handler)(int irq, void *data);
	void (*pmu_error_handler)(int irq, void *data);
	void (*wlan_handler)(int irq, void *data);
	void (*wpan_handler)(int irq, void *data);
	void *irq_dev_wlan;
	void *irq_dev_wpan;
	void *irq_dev_pmu;

	uintptr_t remap_addr_wlan;
	uintptr_t remap_addr_wpan;

	struct msi_emul pmu_msi[NUM_TO_HOST_IRQ_PMU];
	struct msi_emul wlan_msi[NUM_TO_HOST_IRQ_WLAN];
	struct msi_emul wpan_msi[NUM_TO_HOST_IRQ_WPAN];
	u32 rcv_irq_wlan;
	u32 rcv_irq_wpan;

	struct pci_saved_state *pci_saved_configs;
	struct wake_lock pcie_link_wakelock;

	int ch_num;
	struct exynos_pcie_register_event pcie_link_down_event;

	/* This variable is used to check if scan2mem dump is in progress */
	bool scan2mem_mode;
};

/* PCIE interface */
void pcie_init(void);
void pcie_deinit(void);
struct pcie_mif *pcie_get_instance(void);
void pcie_register_driver(void);
void pcie_unregister_driver(void);
void pcie_set_mem_range(struct pcie_mif *pcie, unsigned long mem_start, size_t mem_sz);
void pcie_set_ch_num(struct pcie_mif *pcie, int ch_num);
void pcie_irq_reg_pmu_handler(struct pcie_mif *pcie, void (*handler)(int irq, void *data), void *dev);
void pcie_irq_reg_pmu_pcie_off_handler(struct pcie_mif *pcie, void (*handler)(int irq, void *data), void *dev);
void pcie_irq_reg_pmu_error_handler(struct pcie_mif *pcie, void (*handler)(int irq, void *data), void *dev);
void pcie_irq_reg_wlan_handler(struct pcie_mif *pcie, void (*handler)(int irq, void *data), void *dev);
void pcie_irq_reg_wpan_handler(struct pcie_mif *pcie, void (*handler)(int irq, void *data), void *dev);
void pcie_remap_set(struct pcie_mif *pcie, uintptr_t remap_addr, enum scsc_mif_abs_target target);
int pcie_get_mbox_pmu(struct pcie_mif *pcie);
int pcie_set_mbox_pmu(struct pcie_mif *pcie, u32 val);
int pcie_get_mbox_pmu_pcie_off(struct pcie_mif *pcie);
int pcie_set_mbox_pmu_pcie_off(struct pcie_mif *pcie, u32 val);
int pcie_get_mbox_pmu_error(struct pcie_mif *pcie);
int pcie_load_pmu_fw(struct pcie_mif *pcie, u32 *ka_patch, size_t ka_patch_len);
//int pcie_load_pmu_fw_flags(struct pcie_mif *pcie, u32 *ka_patch, size_t ka_patch_len, uint32_t flags);
void *pcie_map(struct pcie_mif *pcie, size_t *allocated);
void pcie_unmap(struct pcie_mif *pcie);
int pci_get_mifram_ref(struct pcie_mif *pcie, void *ptr, scsc_mifram_ref *ref);
void *pcie_get_mifram_ptr(struct pcie_mif *pcie, scsc_mifram_ref ref);
void *pcie_get_mifram_phy_ptr(struct pcie_mif *pcie, scsc_mifram_ref ref);
uintptr_t pci_get_mif_pfn(struct pcie_mif *pcie);
bool pcie_is_initialized(void);
void pcie_irq_bit_set(struct pcie_mif *pcie, int bit_num, enum scsc_mif_abs_target target);
u32 pcie_irq_get(struct pcie_mif *pcie, enum scsc_mif_abs_target target);
int pcie_mif_set_affinity_cpu(struct pcie_mif *pcie, u8 msi, u8 cpu);
void pcie_irq_bit_clear(struct pcie_mif *pcie, int bit_num, enum scsc_mif_abs_target target);
void pcie_irq_bit_mask(struct pcie_mif *pcie, int bit_num, enum scsc_mif_abs_target target);
void pcie_irq_bit_unmask(struct pcie_mif *pcie, int bit_num, enum scsc_mif_abs_target target);
void pcie_get_msi_range(struct pcie_mif *pcie, u8 *start, u8 *end, enum scsc_mif_abs_target target);
bool pcie_mif_get_scan2mem_mode(struct pcie_mif *pcie);
void pcie_mif_set_scan2mem_mode(struct pcie_mif *pcie, bool enable);
int pcie_mif_poweron(struct pcie_mif *pcie);
void pcie_mif_poweroff(struct pcie_mif *pcie);
void pcie_mif_l1ss_ctrl(struct pcie_mif *pcie, int enable);
void pcie_prepare_boot(struct pcie_mif *pcie);
__iomem void *pcie_get_ramrp_ptr(struct pcie_mif *pcie);
int pcie_get_ramrp_buff(struct pcie_mif *pcie, void **buff, int count, u64 offset);
void pcie_s6165_irqdump(struct pcie_mif *pcie);
bool pcie_is_on(struct pcie_mif *pcie);
void pcie_set_s2m_dram_offset(struct pcie_mif *pcie, u32 offset);
u32 pcie_get_s2m_size_octets(struct pcie_mif *pcie);
#endif
