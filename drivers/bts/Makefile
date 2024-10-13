#
# Makefile for BTS device driver
#

obj-$(CONFIG_EXYNOS_BTS)	+= exynos-bts.o

ifdef CONFIG_SOC_S5E9925
obj-$(CONFIG_EXYNOS_BTS)	+= exynos-btsops9925.o
endif

ifdef CONFIG_SOC_S5E9935
obj-$(CONFIG_EXYNOS_BTS)        += exynos-btsops9935.o
endif

ifdef CONFIG_SOC_S5E8535
obj-$(CONFIG_EXYNOS_BTS)        += exynos-btsops8535.o
endif

ifdef CONFIG_SOC_S5E8835
obj-$(CONFIG_EXYNOS_BTS)        += exynos-btsops8835.o
endif
