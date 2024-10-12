#
# Makefile for Profiler
#

ifeq ($(AMIGO_VER), )
ifdef CONFIG_SOC_S5E8535
	ccflags-y += -DAMIGO_BUILD_VER=3
	AMIGO_VER=3
else
	ccflags-y += -DAMIGO_BUILD_VER=4
	AMIGO_VER=4
endif
else
	ccflags-y += -DAMIGO_BUILD_VER=$(AMIGO_VER)
endif

obj-$(CONFIG_EXYNOS_PROFILER)		+= v$(AMIGO_VER)/exynos-main-profiler.o
obj-$(CONFIG_EXYNOS_CPU_PROFILER)	+= v$(AMIGO_VER)/exynos-cpu-profiler.o
obj-$(CONFIG_EXYNOS_GPU_PROFILER)	+= v$(AMIGO_VER)/exynos-gpu-profiler.o
obj-$(CONFIG_EXYNOS_MIF_PROFILER)	+= v$(AMIGO_VER)/exynos-mif-profiler.o
