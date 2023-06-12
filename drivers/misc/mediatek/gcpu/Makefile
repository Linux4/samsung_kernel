#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

#######################################
# Specify source files
#######################################

# BUILD-IN
ifneq ($(wildcard $(srctree)/drivers/misc/mediatek/gcpu/$(MTK_PLATFORM)),)
obj-y += $(MTK_PLATFORM)/
else ### For IP-BASE
obj-$(CONFIG_TRUSTONIC_TEE_SUPPORT) += $(subst ",,common)/    ### For MT6xxx
obj-$(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) += $(subst ",,mt8xxx)/ ### For MT8xxx
endif
