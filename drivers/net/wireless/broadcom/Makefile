obj-$(CONFIG_B43)		+= b43/
obj-$(CONFIG_B43LEGACY)		+= b43legacy/

obj-$(CONFIG_BRCMFMAC)	+= brcm80211/
obj-$(CONFIG_BRCMSMAC)	+= brcm80211/

ifeq ($(CONFIG_BCM4375),y)
ifeq ($(SEC_BUILD_OPTION_HW_REVISION),00)
CONFIG_BCM4389=y
CONFIG_BCM4375=n
endif
endif

ifeq ($(CONFIG_BCM4389),y)
obj-$(CONFIG_BCM4389)   += bcmdhd_101_10/
obj-$(CONFIG_SOC_EXYNOS9830) += bcmdhd_101_10/dhd_custom_memprealloc.o
obj-$(CONFIG_SOC_EXYNOS9830) += bcmdhd_101_10/dhd_custom_exynos.o
endif

ifeq ($(CONFIG_BCM4375),y)
ifneq ($(PLATFORM_VERSION),)
ifeq ($(shell expr $(PLATFORM_VERSION) \>= 11),1)
obj-$(CONFIG_BCM4375)   += bcmdhd_101_16/
obj-$(CONFIG_SOC_EXYNOS9830) += bcmdhd_101_16/dhd_custom_memprealloc.o
obj-$(CONFIG_SOC_EXYNOS9830) += bcmdhd_101_16/dhd_custom_exynos.o
else
obj-$(CONFIG_BCM4375)   += bcmdhd_101_12/
obj-$(CONFIG_SOC_EXYNOS9830) += bcmdhd_101_12/dhd_custom_memprealloc.o
obj-$(CONFIG_SOC_EXYNOS9830) += bcmdhd_101_12/dhd_custom_exynos.o
endif
endif
endif