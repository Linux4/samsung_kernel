#
# Makefile for sensor chip drivers.
#

obj-$(CONFIG_THERMAL)		+= thermal_sys.o
obj-$(CONFIG_CPU_THERMAL)       += cpu_cooling.o
obj-$(CONFIG_SPEAR_THERMAL)		+= spear_thermal.o
ifeq ($(CONFIG_SOC_EXYNOS3470),y)
obj-$(CONFIG_EXYNOS_THERMAL)		+= exynos3470_thermal.o
else
ifeq ($(CONFIG_SOC_EXYNOS4415),y)
obj-$(CONFIG_EXYNOS_THERMAL)		+= exynos4415_thermal.o
else
obj-$(CONFIG_EXYNOS_THERMAL)		+= exynos_thermal.o
endif
endif
