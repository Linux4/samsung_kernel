ifneq ($(findstring $(CONFIG_TRUSTONIC_TEE_SUPPORT), y m),)
ccflags-y += -I$(srctree)/drivers/char/rpmb/drrpmb_gp/public
ccflags-y += -I$(srctree)/drivers/tee/gud/500/MobiCoreDriver/public/
endif

ccflags-y += -I$(srctree)/drivers/scsi/ufs
ccflags-y += -I$(srctree)/drivers/scsi/ufs/vendor

ccflags-y += -I$(srctree)/drivers/mmc/host
ccflags-y += -I$(srctree)/drivers/mmc/core

ifneq ($(findstring $(CONFIG_TEEGRIS_TEE_SUPPORT), y m),)
ccflags-y += -I$(srctree)/drivers/char/rpmb/drrpmb_gp/public
ccflags-y += -I$(srctree)/drivers/misc/tzdev
ccflags-y += -I$(srctree)/drivers/misc/tzdev/include/tzdev
endif

obj-$(CONFIG_RPMB) += rpmb-mtk.o

obj-$(CONFIG_RPMB) += rpmb.o
rpmb-objs += core.o
rpmb-$(CONFIG_RPMB_INTF_DEV) += cdev.o
obj-$(CONFIG_RPMB_SIM) += rpmb_sim.o

ccflags-y += -D__CHECK_ENDIAN
