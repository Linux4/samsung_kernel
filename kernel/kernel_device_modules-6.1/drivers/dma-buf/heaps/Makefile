# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/dma-buf/heaps

# system heap
system_heap-objs := \
	mtk_system_heap.o \
	mtk_page_pool.o \
	mtk_cache_pool.o \
	mtk_dmabuf_monitor.o \
	mtk-deferred-free-helper.o \
	rbin_heap.o \
	rbincache.o \
	rbinregion.o
obj-$(CONFIG_DEVICE_MODULES_DMABUF_HEAPS_SYSTEM)	+= system_heap.o
obj-$(CONFIG_DEVICE_MODULES_DMABUF_HEAPS_REFILL)	+= mtk_heap_refill.o

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/iommu/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/iommu/arm/arm-smmu-v3/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/trusted_mem/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/monitor_hang/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
obj-$(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM)	+= mtk_sec_heap.o
obj-$(CONFIG_DMABUF_HEAPS_DEBUG) += mtk_heap_debug.o
# system_heap-objs-$(CONFIG_RBIN) += rbin_heap.o rbincache.o rbinregion.o
