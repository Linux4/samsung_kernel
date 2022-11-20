# SPDX-License-Identifier: GPL-2.0-only

# auto-detect subdirs
ifeq ($(CONFIG_ARCH_KONA), y)
include $(srctree)/techpack/display/config/konadisp.conf
endif

ifeq ($(CONFIG_ARCH_KONA), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/konadispconf.h
endif

ifeq ($(CONFIG_ARCH_LITO), y)
include $(srctree)/techpack/display/config/saipdisp.conf
ifeq ($(PROJECT_NAME),$(filter $(PROJECT_NAME),gts7xllite gts7xllitewifi))
include $(srctree)/techpack/display/config/lito_dp.conf
endif
endif

ifeq ($(CONFIG_ARCH_LITO), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/saipdispconf.h
ifeq ($(PROJECT_NAME),$(filter $(PROJECT_NAME),gts7xllite gts7xllitewifi))
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/lito_dp.h
endif
endif

ifeq ($(CONFIG_ARCH_BENGAL), y)
include $(srctree)/techpack/display/config/bengaldisp.conf
endif

ifeq ($(CONFIG_ARCH_BENGAL), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/bengaldispconf.h
endif

obj-$(CONFIG_DRM_MSM) += msm/
obj-$(CONFIG_MSM_SDE_ROTATOR) += rotator/
obj-$(CONFIG_QCOM_MDSS_PLL) += pll/
