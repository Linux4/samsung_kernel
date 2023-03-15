obj-y		                        += npu-system-soc.o
obj-$(CONFIG_CORESIGHT_STM)		+= npu-stm-soc.o

ccflags-y += -Idrivers/vision/vision-core/include
ccflags-y += -Idrivers/vision/npu/core/include
ccflags-y += -Idrivers/vision/npu/core
ccflags-y += -Idrivers/hwtracing/coresight
ifdef CONFIG_NPU_LOOPBACK
ccflags-y += -Idrivers/vision/npu/core/interface/loopback
else
ccflags-y += -Idrivers/vision/npu/core/interface/hardware
endif

# SoC specific inclusion
ccflags-y += -Idrivers/vision/npu/soc/$(CONFIG_EXYNOS_SOC_NAME)/include
# Common header inclusion for MBR
ccflags-y += -Idrivers/vision/dsp
