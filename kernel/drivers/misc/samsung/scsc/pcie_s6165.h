/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __PCIE_S56165_H
#define __PCIE_S56165_H
#include "scsc_mif_abs.h"
#include <linux/exynos-pci-noti.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/resource.h>
#include "pmu_host_if.h"

struct pcie_mif;

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
int pcie_load_pmu_fw_flags(struct pcie_mif *pcie, u32 *ka_patch, size_t ka_patch_len, uint32_t flags);
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
