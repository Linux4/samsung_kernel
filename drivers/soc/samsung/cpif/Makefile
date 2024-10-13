# Makefile of modem_v1

EXTRA_CFLAGS += -Idrivers/soc/samsung/cpif \
		-DCONFIG_OPTION_REGION=\"$(PROJECT_REGION)\"

obj-$(CONFIG_MCU_IPC) += mcu_ipc.o
obj-$(CONFIG_SHM_IPC) += shm_ipc.o

obj-$(CONFIG_BOOT_DEVICE_SPI) += boot_device_spi.o

obj-y += modem_main.o modem_variation.o modem_io_device.o net_io_device.o bootdump_io_device.o ipc_io_device.o modem_utils.o modem_dump.o cpif_clat_info.o cpif_tethering_info.o

obj-$(CONFIG_LINK_DEVICE_SHMEM) += link_device.o \
				   link_device_memory_snapshot.o \
				   link_device_memory_flow_control.o \
				   link_device_memory_debug.o \
				   link_device_memory_sbd.o \
				   link_device_memory_legacy.o \
				   modem_notifier.o

obj-$(CONFIG_LINK_DEVICE_PCIE) += link_device.o \
				   s51xx_pcie.o \
				   link_device_memory_snapshot.o \
				   link_device_memory_flow_control.o \
				   link_device_memory_debug.o \
				   link_device_memory_sbd.o \
				   link_device_memory_legacy.o \
				   modem_notifier.o

obj-$(CONFIG_SEC_MODEM_S5000AP) += modem_ctrl_s5000ap.o modem_ctrl.o
obj-$(CONFIG_SEC_MODEM_S5100) += modem_ctrl_s5100.o modem_ctrl.o

obj-$(CONFIG_CP_ZEROCOPY) += link_rx_zerocopy.o
obj-$(CONFIG_CP_PKTPROC) += link_rx_pktproc.o
obj-$(CONFIG_CP_PKTPROC_V2) += link_rx_pktproc.o

obj-$(CONFIG_LINK_CONTROL_MSG_IOSM) += link_ctrlmsg_iosm.o

obj-$(CONFIG_CP_BTL) += cp_btl.o

obj-$(CONFIG_CPIF_TP_MONITOR) += cpif_tp_monitor.o

obj-$(CONFIG_MODEM_IF_LEGACY_QOS) += cpif_qos_info.o

obj-$(CONFIG_USB_CONFIGFS_F_MBIM) += mbim_io_device.o

ifeq ($(CONFIG_RPS), y)
ifeq ($(CONFIG_ARGOS), y)
obj-y += modem_argos_notifier.o
endif
endif
