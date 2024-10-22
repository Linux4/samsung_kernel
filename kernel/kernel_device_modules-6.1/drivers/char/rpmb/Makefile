ifneq ($(findstring $(CONFIG_TRUSTONIC_TEE_SUPPORT), y m),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/char/rpmb/drrpmb_gp/public
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/tee/gud/$(CONFIG_TRUSTONIC_TEE_VERSION)/MobiCoreDriver/public/
endif
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/ufs
ccflags-y += -I$(srctree)/drivers/ufs
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/ufs/vendor

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/mmc/host
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/mmc/core
ccflags-y += -I$(srctree)/drivers/mmc/host
ccflags-y += -I$(srctree)/drivers/mmc/core

ifneq ($(findstring $(CONFIG_TEEGRIS_TEE_SUPPORT), y m),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/char/rpmb/drrpmb_gp/public
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/tee/tzdev
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/tee/tzdev/include/tzdev
endif

obj-$(CONFIG_RPMB) += rpmb-mtk.o

obj-$(CONFIG_RPMB) += rpmb.o
rpmb-objs += core.o
rpmb-$(CONFIG_RPMB_INTF_DEV) += cdev.o
obj-$(CONFIG_RPMB_SIM) += rpmb_sim.o

ccflags-y += -D__CHECK_ENDIAN
