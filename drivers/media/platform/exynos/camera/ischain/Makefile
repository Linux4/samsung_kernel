obj-$(CONFIG_EXYNOS_PABLO_ISP) += setup-is-module.o

obj-$(CONFIG_EXYNOS_PABLO_ISP) += is-v7_0_0/
ifeq ($(CONFIG_SOC_EXYNOS9830_EVT0), y)
obj-$(CONFIG_EXYNOS_PABLO_ISP) += is-v8_0_0/
else
obj-$(CONFIG_EXYNOS_PABLO_ISP) += is-v8_1_0/
endif
obj-$(CONFIG_EXYNOS_PABLO_ISP) += is-v8_10_0/
obj-$(CONFIG_EXYNOS_PABLO_ISP) += is-v8_20_0/

EXTRA_CFLAGS += -Idrivers/media/platform/exynos/camera/include
