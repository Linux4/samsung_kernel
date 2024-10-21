# SPDX-License-Identifier: GPL-2.0
# Mediatek UFS extended features makefile
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/clk/mediatek/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/power_throttling/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/ufs/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/ufs/vendor/

# must after -I$(DEVICE_MODULES_PATH)/drivers/ufs/
ccflags-y += -I$(srctree)/drivers/ufs/
ccflags-y += -I$(srctree)/drivers/ufs/core/
ccflags-y += -I$(srctree)/drivers/ufs/host/

# UFS KO without iSE
obj-$(CONFIG_DEVICE_MODULES_SCSI_UFS_MEDIATEK)  += ufs-mediatek-mod.o
ufs-mediatek-mod-y                              += ../ufs-mediatek.o ../ufs-mediatek-sysfs.o ../ufs-mediatek-mimic.o
ufs-mediatek-mod-$(CONFIG_SEC_UFS_FEATURE) += ufs-sec-feature.o ufs-sec-sysfs.o
ufs-mediatek-mod-$(CONFIG_MTK_BLOCK_IO_TRACER)  += ../ufs-mediatek-btag.o
ufs-mediatek-mod-$(CONFIG_RPMB)                 += ../ufs-mediatek-rpmb.o
ufs-mediatek-mod-$(CONFIG_RPMB)                 += ../ufs-mediatek-ise-dummy.o
ufs-mediatek-mod-$(CONFIG_UFSFEATURE)           += ufsfeature.o ufsfeature-mimic.o
ufs-mediatek-mod-$(CONFIG_UFSSHPB)              += ufsshpb.o
ufs-mediatek-mod-$(CONFIG_UFSTW)                += ufstw.o
ufs-mediatek-mod-$(CONFIG_UFSHID)               += ufshid.o
ufs-mediatek-mod-$(CONFIG_UFSRINGBUF)           += ufsringbuf.o

# UFS KO with iSE enabled
obj-$(CONFIG_DEVICE_MODULES_SCSI_UFS_MEDIATEK)  += ufs-mediatek-mod-ise.o
ufs-mediatek-mod-ise-y                              += ../ufs-mediatek.o ../ufs-mediatek-sysfs.o ../ufs-mediatek-mimic.o
ufs-mediatek-mod-ise-$(CONFIG_SEC_UFS_FEATURE) += ufs-sec-feature.o ufs-sec-sysfs.o
ufs-mediatek-mod-ise-$(CONFIG_MTK_BLOCK_IO_TRACER)  += ../ufs-mediatek-btag.o
ufs-mediatek-mod-ise-$(CONFIG_RPMB)                 += ../ufs-mediatek-rpmb.o
ufs-mediatek-mod-ise-$(CONFIG_RPMB)                 += ../ufs-mediatek-ise.o
ufs-mediatek-mod-ise-$(CONFIG_UFSFEATURE)           += ufsfeature.o ufsfeature-mimic.o
ufs-mediatek-mod-ise-$(CONFIG_UFSSHPB)              += ufsshpb.o
ufs-mediatek-mod-ise-$(CONFIG_UFSTW)                += ufstw.o
ufs-mediatek-mod-ise-$(CONFIG_UFSHID)               += ufshid.o
ufs-mediatek-mod-ise-$(CONFIG_UFSRINGBUF)           += ufsringbuf.o
