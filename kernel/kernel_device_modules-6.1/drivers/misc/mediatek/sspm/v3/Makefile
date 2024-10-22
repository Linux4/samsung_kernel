obj-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += sspm_v3.o

sspm_v3-y += sspm_sysfs.o
sspm_v3-y += sspm_helper.o
sspm_v3-y += sspm_plt.o
sspm_v3-y += sspm_logger_impl.o
sspm_v3-y += sspm_timesync.o

sspm_v3-$(CONFIG_OF_RESERVED_MEM) += sspm_reservedmem.o

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v3
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/tinysys_scmi

#ccflags-y += -DDEBUG
