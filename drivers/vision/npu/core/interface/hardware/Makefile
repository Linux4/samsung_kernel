obj-y				+= npu-interface.o
#obj-y				+= npu-mailbox.o
obj-y				+= mailbox_ipc.o

ccflags-y +=	-Idrivers/vision/vision-core/include \
		-Idrivers/vision/npu/core/include \
		-Idrivers/vision/npu/core \
		-Idrivers/vision/npu/core/interface/hardware

# SoC specific inclusion
ccflags-y += -Idrivers/vision/npu/soc/$(CONFIG_EXYNOS_SOC_NAME)/include
