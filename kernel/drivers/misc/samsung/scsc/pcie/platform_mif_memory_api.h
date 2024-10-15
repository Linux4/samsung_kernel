#ifndef __PLATFORM_MIF_MODAP_MEMORY_API_H__
#define __PLATFORM_MIF_MODAP_MEMORY_API_H__

#include "platform_mif.h"

void *platform_mif_map(struct scsc_mif_abs *interface, size_t *allocated);
void platform_mif_unmap(struct scsc_mif_abs *interface, void *mem);
void platform_mif_remap_set(struct scsc_mif_abs *interface, uintptr_t remap_addr, enum scsc_mif_abs_target target);
int platform_mif_get_mbox_pmu(struct scsc_mif_abs *interface);
int platform_mif_set_mbox_pmu(struct scsc_mif_abs *interface, u32 val);
int platform_mif_get_mbox_pmu_error(struct scsc_mif_abs *interface);
int platform_mif_get_mifram_ref(struct scsc_mif_abs *interface, void *ptr, scsc_mifram_ref *ref);
void *platform_mif_get_mifram_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref);
void *platform_mif_get_mifram_phy_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref);
uintptr_t platform_mif_get_mif_pfn(struct scsc_mif_abs *interface);
int __init platform_mif_wifibt_if_reserved_mem_setup(struct reserved_mem *remem);
int platform_mif_get_mbox_pmu_pcie_off(struct scsc_mif_abs *interface);
int platform_mif_set_mbox_pmu_pcie_off(struct scsc_mif_abs *interface, u32 val);

void platform_mif_remap_set(struct scsc_mif_abs *interface, uintptr_t remap_addr, enum scsc_mif_abs_target target);
void platform_mif_memory_api_init(struct scsc_mif_abs *interface);

#endif
