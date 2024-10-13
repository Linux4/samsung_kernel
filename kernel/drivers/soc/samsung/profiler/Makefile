#
# Makefile for Profiler
#

INCLUDE_PATH = $(srctree)/drivers/soc/samsung/profiler/include
ccflags-y += -I$(INCLUDE_PATH)
obj-$(CONFIG_EXYNOS_MAIN_PROFILER) += include/exynos-profiler-fn.o

ifeq ($(CONFIG_EXYNOS_PROFILER_LEGACY), y)
	PROFILER_VERSION=0
	obj-$(CONFIG_EXYNOS_MAIN_PROFILER)	+= legacy/exynos-main-profiler.o
	obj-$(CONFIG_EXYNOS_CPU_PROFILER)	+= legacy/exynos-cpu-profiler.o
	obj-$(CONFIG_EXYNOS_GPU_PROFILER)	+= legacy/exynos-gpu-profiler.o
	obj-$(CONFIG_EXYNOS_MIF_PROFILER)	+= legacy/exynos-mif-profiler.o
else
	PROFILER_VERSION=1
	obj-$(CONFIG_EXYNOS_MAIN_PROFILER)	+= v$(PROFILER_VERSION)/exynos-main-profiler.o
	obj-$(CONFIG_EXYNOS_CPU_PROFILER)	+= v$(PROFILER_VERSION)/exynos-cpu-profiler.o
	obj-$(CONFIG_EXYNOS_GPU_PROFILER)	+= v$(PROFILER_VERSION)/exynos-gpu-profiler.o
	obj-$(CONFIG_EXYNOS_MIF_PROFILER)	+= v$(PROFILER_VERSION)/exynos-mif-profiler.o
endif

