#
# Makefile for Profiler
#

INCLUDE_PATH = $(srctree)/drivers/soc/samsung/profiler/include
ccflags-y += -I$(INCLUDE_PATH)
obj-$(CONFIG_EXYNOS_MAIN_PROFILER) += include/exynos-profiler-fn.o

obj-$(CONFIG_EXYNOS_MAIN_PROFILER)	+= v1/exynos-main-profiler.o
obj-$(CONFIG_EXYNOS_CPU_PROFILER)	+= v1/exynos-cpu-profiler.o
obj-$(CONFIG_EXYNOS_GPU_PROFILER)	+= v1/exynos-gpu-profiler.o
obj-$(CONFIG_EXYNOS_MIF_PROFILER)	+= v1/exynos-mif-profiler.o

