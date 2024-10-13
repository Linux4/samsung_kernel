# SPDX-License-Identifier: GPL-2.0
# COPYRIGHT(C) 2023 Samsung Electronics Co., Ltd. All Right Reserved.

#dtstree	:= $(srctree)/$(src)

define __sec_dtbo_build
dtbo-$(2) += $(1)
$(1)-base := $(3)
endef

define sec_dtbo_build
$(foreach dtbo, $(1), $(eval $(call __sec_dtbo_build,$(dtbo),$(2),$(3))))
endef

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
SEC_M55XQ_BOARDS        := \
        diwali-sec.m55xq.overlay.00.dtbo \
        diwali-sec.m55xq.overlay.01.dtbo \
        diwali-sec.m55xq.overlay.02.dtbo \
        diwali-sec.m55xq.overlay.03.dtbo \
        diwali-sec.m55xq.overlay.04.dtbo \
        diwali-sec.m55xq.overlay.05.dtbo \
        diwali-sec.m55xq.overlay.06.dtbo \
        diwali-sec.m55xq.overlay.07.dtbo

# DIWALI BASE DTB
QCOM_DTB := ../../vendor/samsung
SEC_DIWALI_BASE_DTB := $(QCOM_DTB)/diwali.dtb $(QCOM_DTB)/diwalip.dtb $(QCOM_DTB)/diwali-lte.dtb

$(eval $(call sec_dtbo_build, \
		$(SEC_M55XQ_BOARDS),$(CONFIG_ARCH_X55),$(SEC_DIWALI_BASE_DTB)))
else
dtb-$(CONFIG_ARCH_X55) += $(QCOM_DTB)/diwali-qrd.dtb
endif

always-y        := $(dtb-y)
subdir-y        := $(dts-dirs)
clean-files	:= *.dtb *.reverse.dts *.dtbo

