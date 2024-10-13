#
#  Makefile for Exynos-Debug
#

obj-$(CONFIG_EXYNOS_CORESIGHT)		+= exynos-coresight.o
ifeq ($(CONFIG_SOC_EXYNOS9610),y)
obj-$(CONFIG_EXYNOS_ITMON)		+= exynos9610-itmon.o
endif
ifeq ($(CONFIG_SOC_EXYNOS9630),y)
obj-$(CONFIG_EXYNOS_ITMON)		+= exynos9630-itmon.o
endif
ifeq ($(CONFIG_SOC_EXYNOS3830),y)
obj-$(CONFIG_EXYNOS_ITMON)		+= exynos3830-itmon.o
endif
ifeq ($(CONFIG_DEBUG_SNAPSHOT),y)
obj-$(CONFIG_ARCH_EXYNOS)		+= exynos-helper.o
obj-$(CONFIG_ARCH_EXYNOS)		+= exynos-handler.o

endif
obj-$(CONFIG_EXYNOS_ADV_TRACER) 	+= exynos-adv-tracer.o exynos-adv-tracer-ipc.o
obj-$(CONFIG_EXYNOS_ADV_TRACER_S2D)	+= exynos-adv-tracer-s2d.o
obj-$(CONFIG_EXYNOS_ADV_TRACER_MISC)	+= exynos-adv-tracer-misc.o
obj-$(CONFIG_EXYNOS_DEBUG_TEST)         += exynos-debug-test.o
obj-$(CONFIG_EXYNOS_EHLD)               += exynos-ehld.o
obj-$(CONFIG_EXYNOS_HALT)		+= exynos-halt.o
