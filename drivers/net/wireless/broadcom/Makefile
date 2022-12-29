# SPDX-License-Identifier: GPL-2.0-only
obj-$(CONFIG_B43)		+= b43/
obj-$(CONFIG_B43LEGACY)		+= b43legacy/

obj-$(CONFIG_BRCMFMAC)	+= brcm80211/
obj-$(CONFIG_BRCMSMAC)	+= brcm80211/

ifeq ($(SEC_BUILD_OPTION_ALT_WIFI), true)
export CONFIG_BCM4389=m
export CONFIG_BCM4375=n
endif
$(warning DHDMAKEFILE CONFIG_BCM4389 = $(CONFIG_BCM4389))
$(warning DHDMAKEFILE CONFIG_BCM4375 = $(CONFIG_BCM4375))

ifneq ($(filter m y, $(CONFIG_BCM4389)),)
obj-$(CONFIG_BCM4389)   += bcmdhd_101_18/
endif
ifneq ($(filter y, $(CONFIG_BCM4389)),)
obj-$(CONFIG_ARCH_LAHAINA) += bcmdhd_101_18/dhd_custom_memprealloc.o
obj-$(CONFIG_ARCH_LAHAINA) += bcmdhd_101_18/dhd_custom_msm.o
endif

ifneq ($(filter m y, $(CONFIG_BCM4375)),)
obj-$(CONFIG_BCM4375)   += bcmdhd_101_16/
endif
ifneq ($(filter y, $(CONFIG_BCM4375)),)
obj-$(CONFIG_ARCH_LAHAINA) += bcmdhd_101_16/dhd_custom_memprealloc.o
obj-$(CONFIG_ARCH_LAHAINA) += bcmdhd_101_16/dhd_custom_msm.o
endif