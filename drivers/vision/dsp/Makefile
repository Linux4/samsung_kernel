#
# Makefile for the dsp common driver
#

ccflags-y := -Wformat
subdir-ccflags-y := -Wformat

ccflags-y += -Idrivers/vision/dsp
ccflags-$(CONFIG_EXYNOS_DSP_HARDWARE) += -Idrivers/vision/dsp/hardware/exynos$(CONFIG_EXYNOS_SOC_NAME)

obj-y					+= dsp-device.o
obj-y					+= dsp-context.o
obj-y					+= dsp-core.o
obj-y					+= dsp-ioctl.o
ifeq ($(CONFIG_COMPAT),y)
obj-y					+= dsp-compat-ioctl.o
endif
obj-y					+= dsp-debug.o
obj-y					+= dsp-binary.o
obj-y					+= dsp-task.o
obj-y					+= dsp-kernel.o
obj-y					+= dsp-graph.o
obj-y					+= dsp-util.o
obj-y					+= dsp-time.o
obj-y					+= dl/

obj-y					+= dsp-npu.o

obj-$(CONFIG_EXYNOS_DSP_HARDWARE)	+= hardware/
