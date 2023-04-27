obj-y		                        += npu-system-soc.o

ccflags-y += -Idrivers/vision/vision-core/include -Idrivers/vision/npu/core/include -Idrivers/vision/npu/core
ifdef CONFIG_NPU_LOOPBACK
ccflags-y += -Idrivers/vision/npu/core/interface/loopback
else
ccflags-y += -Idrivers/vision/npu/core/interface/hardware
endif

# SoC specific inclusion
ccflags-y += -Idrivers/vision/npu/soc/$(CONFIG_EXYNOS_SOC_NAME)/include
