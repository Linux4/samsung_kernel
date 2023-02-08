# Makefile of modem_v1

EXTRA_CFLAGS += -Idrivers/misc/modem_v1

obj-y += modem_main.o io_device.o modem_utils.o

obj-$(CONFIG_LINK_DEVICE_MEMORY) += link_device_memory_main.o \
				    link_device_memory_snapshot.o \
				    link_device_memory_command.o \
				    link_device_memory_flow_control.o \
				    link_device_memory_debug.o
obj-$(CONFIG_LINK_DEVICE_C2C) += link_device_c2c.o
obj-$(CONFIG_LINK_DEVICE_SHMEM) += link_device_shmem.o

obj-$(CONFIG_UMTS_MODEM_SS222) += modem_ctrl_ss222.o boot_device_spi.o
obj-$(CONFIG_UMTS_MODEM_SH222AP) += modem_ctrl_sh222ap.o
