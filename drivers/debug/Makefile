# When adding new entries keep the list in alphabetical order

obj-$(CONFIG_SEC_BSP) += sec_bsp.o sec_smem.o

ifeq ($(CONFIG_ARCH_MSM8976),y)
obj-$(CONFIG_SEC_DEBUG) += sec_debug.o
endif

obj-$(CONFIG_SEC_DEBUG) += sec_gaf.o sec_getlog.o
obj-$(CONFIG_SEC_DEBUG_DOUBLE_FREE) += sec_debug-dfd.o
obj-$(CONFIG_SEC_DEBUG_SUMMARY) += sec_debug_summary.o
